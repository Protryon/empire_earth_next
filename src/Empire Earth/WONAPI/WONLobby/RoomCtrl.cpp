
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <string>
#include <list>
#include <map>
#include <vector>
#include <stdio.h>
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
using namespace WONAPI;
// RoomListItem: an entry of the room tree (a server or one of its chat rooms), private to this file
// FUNCTION: WON 0x007c19a0
// Creates a room tree entry for a server, or its 'querying server' placeholder when loading
RoomListItem::RoomListItem(WONAPI::LobbyServer* inServer,bool inLoading)
    :mColor(-1),mLoading(inLoading),mAddress(inServer->GetAddress()),mGroupId(0xffff)
{
    this->UpdateText(inServer,NULL);
}
// FUNCTION: WON 0x007c1c50
// Creates a room tree entry for a chat room on a server
RoomListItem::RoomListItem(WONAPI::LobbyGroup* inGroup,WONAPI::LobbyServer* inServer)
    :mColor(-1),mLoading(false),mAddress(inServer->GetAddress()),mGroupId(inGroup->GetId())
{
    this->UpdateText(inServer,inGroup);
}
// FUNCTION: WON 0x007c20a0
// Orders room tree entries by their text, ignoring case
int RoomListItem::Compare(const WONAPI::ListItem* inOther,WONAPI::ListArea*) const
{
    return this->mText.CompareNoCase(static_cast<const RoomListItem*>(inOther)->mText);
}
// FUNCTION: WON 0x007c1d40
// Sets the entry's text ('querying server', or the room or server name with its player count) and lock icon
void RoomListItem::UpdateText(WONAPI::LobbyServer* inServer,WONAPI::LobbyGroup* inGroup)
{
    char aCount[50];
    if(this->mLoading) this->mText=::RoomCtrl_QueryingServer_String;
    else if(inGroup!=NULL)
    {
        sprintf(aCount," (%d)",inGroup->GetNumClients());
        this->mText=inGroup->GetName();
        if(static_cast<int>(inGroup->GetNumClients())>0)
            this->mText.append(aCount);
    }
    else if(inServer!=NULL)
    {
        sprintf(aCount," (%d)",inServer->GetNumClients());
        this->mText=inServer->GetName();
        if(inServer->GetNumClients()!=0)
            this->mText.append(aCount);
    }
    else this->mText.erase();
    if(inGroup!=NULL && inGroup->IsPasswordProtected()) this->mImage=::RoomCtrl_Password_Image;
    else this->mImage=NULL;
}
// FUNCTION: WON 0x007c1fd0
// Measures the entry: server entries use a bold font, and the lock icon adds width and height
void RoomListItem::CalcDimensions(WONAPI::ListArea* inList)
{
    if(this->mFont.get()==NULL)
    {
        if(this->mGroupId==0xffff && !this->mLoading)
            this->mFont=inList->GetFontMod(inList->mFont.get(),1,0);
        else this->mFont=inList->mFont;
    }
    if(this->mColor<0) this->mColor=inList->GetTextColor();
    this->mWidth=this->mFont->GetStringWidth(this->mText,0,this->mText.length())+2;
    this->mHeight=this->mFont->GetHeight();
    if(this->mImage.get()!=NULL)
    {
        this->mWidth+=this->mImage->mWidth+2;
        if(this->mImage->GetHeight()>this->mHeight) this->mHeight=this->mImage->GetHeight();
    }
}
// Uses the list's selection color (ListArea at 0x140) and its full-row selection flag (list flags at 0x124)
// FUNCTION: WON 0x007c20c0
// Draws the entry with its selection highlight, lock icon and text
void RoomListItem::Paint(WONAPI::Graphics& g,WONAPI::ListArea* inList)
{
    int aColor=this->mColor;
    if(this->mSelected)
    {
        g.SetColor(inList->GetSelColor().GetBackgroundColor(g));
        int aWidth=inList->mPaintWidth;
        if(!inList->ListFlagSet(0x10)) aWidth=this->mWidth;
        g.FillRect(0,0,aWidth,this->mHeight);
        aColor=inList->GetSelColor().GetForegroundColor(g,this->mColor);
    }
    int x=2;
    if(this->mImage.get()!=NULL)
    {
        g.DrawImage(this->mImage.get(),x,(this->mHeight-this->mImage->mHeight)/2);
        x=this->mImage->mWidth+4;
    }
    g.SetColor(aColor);
    g.SetFont(this->mFont.get());
    g.DrawString(this->mText,x,(this->mHeight-this->mFont->GetHeight())/2);
}


namespace WONAPI {
RoomCtrl::RoomCtrl() {}
// FUNCTION: WON 0x007c22a0
// Looks up the room tree and the Join and Create buttons, listens to broadcasts and updates Join
void RoomCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2) return;
    ComponentConfig* aConfig=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig,this->mRoomList,"RoomList",__FILE__,168);
    SafeGetComponent(aConfig,this->mJoinButton,"JoinButton",__FILE__,169);
    SafeGetComponent(aConfig,this->mCreateButton,"CreateButton",__FILE__,170);
    this->SubscribeToBroadcast(1);
    this->EnableControls();
}
// FUNCTION: WON 0x007c2320
// Enables Join only while a room tree entry is selected
void RoomCtrl::EnableControls()
{
    this->mJoinButton->SetEnabled(this->mRoomList->mSelItem!=NULL);
}
// FUNCTION: WON 0x007c2350
// Adds a chat room entry under a server entry
void RoomCtrl::AddGroup(TreeItem* inParent,LobbyServer* inServer,LobbyGroup* inGroup)
{
    RoomListItem* aItem=new RoomListItem(inGroup,inServer);
    this->mRoomList->InsertTreeItem(inParent,aItem,-1);
}
// FUNCTION: WON 0x007c23d0
// Adds a server entry with its chat rooms (or a 'querying' placeholder while unconnected), optionally opened
void RoomCtrl::AddServer(LobbyServer* inServer,bool inOpen,int inPos)
{
    this->mRoomList->BeginUpdate();
    RoomListItem* aItem=new RoomListItem(inServer,false);
    TreeItem* aServer=this->mRoomList->InsertTreeItem(NULL,aItem,inPos);
    size_t it=0;
    while(it<inServer->GetGroupCount())
    {
        LobbyGroup* aGroup=inServer->GroupAt(it);
        if(aGroup->IsChatGroup()) this->AddGroup(aServer,inServer,aGroup);
        ++it;
    }
    if(!inServer->IsConnected() && inServer->GetGroupCount()==0)
    {
        this->mRoomList->RemoveAllItems(aServer);
        RoomListItem* aLoading=new RoomListItem(inServer,true);
        TreeItem* aLoadingTree=this->mRoomList->InsertTreeItem(aServer,aLoading,0);
        aLoadingTree->mSelectable=false;
    }
    if(inOpen) this->mRoomList->OpenItem(aServer,true);
    this->mRoomList->EndUpdate();
    this->EnableControls();
}
// FUNCTION: WON 0x007c2b10
// Returns the server entry with the given address, or NULL
TreeItem* RoomCtrl::GetServerItem(const IPAddr& inAddress)
{
    for(int i=0;i<this->mRoomList->GetNumItems();++i)
    {
        TreeItem* aTree=static_cast<TreeItem*>(this->mRoomList->GetItem(i));
        RoomListItem* aItem=static_cast<RoomListItem*>(aTree->mItem.get());
        if(aItem->mAddress==inAddress) return aTree;
    }
    return NULL;
}
// FUNCTION: WON 0x007c2b70
// Returns the entry of the room with the given id under the server at the given address, or NULL
TreeItem* RoomCtrl::GetGroupItem(const IPAddr& inAddress,int inGroupId)
{
    TreeItem* aServer=this->GetServerItem(inAddress);
    if(aServer==NULL)
        return NULL;
    for(int i=0;i<static_cast<int>(aServer->mChildren.size());++i)
    {
        TreeItem* aChild=aServer->GetChild(i);
        if(static_cast<RoomListItem*>(aChild->mItem.get())->mGroupId==static_cast<unsigned long>(inGroupId)) return aChild;
    }
    return NULL;
}
// FUNCTION: WON 0x007c2bc0
// Returns a server's entry, or NULL
TreeItem* RoomCtrl::GetServerItem(LobbyServer* inServer)
{
    if(inServer!=NULL) return this->GetServerItem(inServer->GetAddress());
    return NULL;
}
// FUNCTION: WON 0x007c2be0
// Returns a chat room's entry on a server, or NULL
TreeItem* RoomCtrl::GetGroupItem(LobbyServer* inServer,LobbyGroup* inGroup)
{
    if(inServer!=NULL && inGroup!=NULL) return this->GetGroupItem(inServer->GetAddress(),inGroup->GetId());
    return NULL;
}

// FUNCTION: WON 0x007c2630
// Asks the lobby to join the selected room (or server)
void RoomCtrl::DoJoinRoom()
{
    TreeItem* aSelected=static_cast<TreeItem*>(this->mRoomList->mSelItem);
    if(aSelected!=NULL)
    {
        RoomListItem* aItem=static_cast<RoomListItem*>(aSelected->mItem.get());
        this->DeliverEvent(new JoinRoomEvent(aItem->mAddress,aItem->mGroupId),false);
    }
}
// FUNCTION: WON 0x007c28f0
// Asks the lobby to create a room on the selected server (no address when nothing is selected)
void RoomCtrl::DoCreateRoom()
{
    IPAddr aAddress;
    TreeItem* aSelected=static_cast<TreeItem*>(this->mRoomList->mSelItem);
    if(aSelected!=NULL) aAddress=static_cast<RoomListItem*>(aSelected->mItem.get())->mAddress;
    this->DeliverEvent(new CreateRoomEvent(aAddress),false);
}
// FUNCTION: WON 0x007c2550
// Rebuilds the room tree from the server list and selects our current chat room (or its server)
void RoomCtrl::HandleSetRoomList()
{
    LobbyServerList* aServers=LobbyGetServerList();
    if(aServers==NULL) return;
    this->mRoomList->BeginUpdate();
    this->mRoomList->ClearTree(true);
    for(size_t it=0;it<aServers->GetCount();++it)
        this->AddServer(aServers->ServerAt(it),false,-1);
    this->mRoomList->EndUpdate();
    LobbyGroup* aGroup=LobbyGetChatGroup();
    LobbyServer* aServer=LobbyGetCurrentServer();
    if(aServer!=NULL)
    {
        TreeItem* aItem=this->GetServerItem(aServer);
        if(aItem!=NULL)
        {
            this->mRoomList->OpenItem(aItem,false);
            if(aGroup==NULL || (aItem=this->GetGroupItem(aServer,aGroup))!=NULL)
            {
            ListArea* aList=this->mRoomList.get();
            aList->SetSelItem(aList->GetItemPos(aItem),true);
            this->mRoomList->EnsureVisible(aItem);
            }
        }
    }
    this->EnableControls();
}
// FUNCTION: WON 0x007c2e20
// Adds, removes or refreshes a chat room's entry, refreshing its server's player count too
void RoomCtrl::HandleGroupChanged(GroupChangedEvent* inEvent)
{
    LobbyGroup* aGroup=inEvent->mGroup.get();
    LobbyServer* aServer=inEvent->mServer.get();
    if(aGroup==NULL || aServer==NULL || !aGroup->IsChatGroup()) return;
    if(inEvent->IsAdd())
    {
        TreeItem* aServerItem=this->GetServerItem(aServer);
        if(aServerItem!=NULL) this->AddGroup(aServerItem,aServer,aGroup);
        this->EnableControls();
        return;
    }
    if(inEvent->IsDelete())
    {
        TreeItem* aGroupItem=this->GetGroupItem(aServer,aGroup);
        this->mRoomList->RemoveTreeItem(aGroupItem);
        this->EnableControls();
        return;
    }
    TreeItem* aGroupItem=this->GetGroupItem(aServer,aGroup);
    if(aGroupItem!=NULL)
    {
        static_cast<RoomListItem*>(aGroupItem->mItem.get())->UpdateText(aServer,aGroup);
        static_cast<RoomListItem*>(aGroupItem->mParent->mItem.get())->UpdateText(aServer,NULL);
        this->mRoomList->RefreshItem(aGroupItem->mParent);
        this->mRoomList->RefreshItem(aGroupItem);
    }
    this->EnableControls();
}
// FUNCTION: WON 0x007c2f00
// Asks the lobby to query a server's rooms when its entry is opened
void RoomCtrl::HandleItemOpened(TreeItemOpenedEvent* inEvent)
{
    TreeItem* aTree=inEvent->mItem.get();
    if(aTree->mOpen)
    {
        RoomListItem* aItem=static_cast<RoomListItem*>(aTree->mItem.get());
        this->DeliverEvent(new QueryServerGroupsEvent(aItem->mAddress),false);
    }
}
// FUNCTION: WON 0x007c2c10
// Adds, removes or rebuilds a server's entry, keeping it open, its selected room and the scroll position
void RoomCtrl::HandleServerChanged(ServerChangedEvent* inEvent)
{
    if(inEvent->IsAdd())
    {
        this->AddServer(inEvent->mServer.get(),false,-1);
        return;
    }
    if(inEvent->IsDelete())
    {
        TreeItem* aServerItem=this->GetServerItem(inEvent->mServer.get());
        if(aServerItem!=NULL) this->mRoomList->RemoveTreeItem(aServerItem);
        this->EnableControls();
        return;
    }
    TreeItem* aServerItem=this->GetServerItem(inEvent->mServer.get());
    if(aServerItem==NULL) return;
    bool aOpen=aServerItem->mOpen;
    TreeItem* aSelected=static_cast<TreeItem*>(this->mRoomList->mSelItem);
    unsigned long aGroupId=0xffff;
    IPAddr aAddress;
    if(aSelected!=NULL && aSelected->mParent==aServerItem)
    {
        RoomListItem* aItem=static_cast<RoomListItem*>(aSelected->mItem.get());
        aGroupId=aItem->mGroupId;
        aAddress=aItem->mAddress;
    }
    int aPosition=aServerItem->mIndex;
    int aVertOffset=this->mRoomList->mVertOffset;
    this->mRoomList->BeginUpdate();
    this->mRoomList->RemoveTreeItem(aServerItem);
    this->AddServer(inEvent->mServer.get(),aOpen,aPosition);
    if(aGroupId!=0xffff && aAddress.IsValid())
    {
        TreeItem* aItem=this->GetGroupItem(aAddress,aGroupId);
        if(aItem!=NULL)
        {
            ListArea* aList=this->mRoomList.get();
            aList->SetSelItem(aList->GetItemPos(aItem),true);
        }
    }
    this->mRoomList->EndUpdate();
    this->mRoomList->SetVertOffset(aVertOffset);
    this->EnableControls();
}
// FUNCTION: WON 0x007c31b0
// Routes double clicks, Join and Create, tree opening and room/server changes to the handlers
void RoomCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch(inEvent->mType)
    {
    case ComponentEvent_TreeItemOpened:
        this->HandleItemOpened(static_cast<TreeItemOpenedEvent*>(inEvent));
        return;
    case ComponentEvent_ListItemClicked:
        {
            this->EnableControls();
            ListItemClickedEvent* aEvent=static_cast<ListItemClickedEvent*>(inEvent);
            if(!aEvent->mDoubleClick || aEvent->mComponent!=this->mRoomList.get() || !aEvent->IsSelectedItem()) return;
            TreeItem* aItem=static_cast<TreeItem*>(aEvent->mItem.get());
            if(aItem->ContainsPoint(aEvent->mX,aEvent->mY)) this->DoJoinRoom();
        }
        return;
    case ComponentEvent_ButtonPressed:
        if(inEvent->mComponent==this->mJoinButton.get()) { this->DoJoinRoom(); return; }
        if(inEvent->mComponent==this->mCreateButton.get()) { this->DoCreateRoom(); return; }
        break;
    case LobbyEvent_GroupChanged:
        this->HandleGroupChanged(static_cast<GroupChangedEvent*>(inEvent));
        return;
    case LobbyEvent_ServerChanged:
        this->HandleServerChanged(static_cast<ServerChangedEvent*>(inEvent));
        return;
    case 100053:
        this->HandleSetRoomList();
        return;
    }
    Component::HandleComponentEvent(inEvent);
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c1b70
// (public: virtual __thiscall RoomListItem::~RoomListItem(void)    [??1RoomListItem@@UAE@XZ])
// TEMPLATE: WON 0x007c2190
// (public: __thiscall WONAPI::RoomCtrl::RoomCtrl(void)    [??0RoomCtrl@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007ce350

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c21c0
// (public: virtual void * __thiscall WONAPI::RoomCtrl::`scalar deleting destructor'(unsigned int)    [??_GRoomCtrl@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007c21e0
// (public: virtual __thiscall WONAPI::RoomCtrl::~RoomCtrl(void)    [??1RoomCtrl@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007c2840
// (public: virtual void * __thiscall WONAPI::JoinRoomEvent::`scalar deleting destructor'(unsigned int)    [??_GJoinRoomEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007c2860
// (public: virtual __thiscall WONAPI::JoinRoomEvent::~JoinRoomEvent(void)    [??1JoinRoomEvent@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007c2a60
// (public: virtual void * __thiscall WONAPI::CreateRoomEvent::`scalar deleting destructor'(unsigned int)    [??_GCreateRoomEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007c2a80
// (public: virtual __thiscall WONAPI::CreateRoomEvent::~CreateRoomEvent(void)    [??1CreateRoomEvent@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007c3100
// (public: virtual void * __thiscall WONAPI::QueryServerGroupsEvent::`scalar deleting destructor'(unsigned int)    [??_GQueryServerGroupsEvent@WONAPI@@UA)
// TEMPLATE: WON 0x007c3120
// (public: virtual __thiscall WONAPI::QueryServerGroupsEvent::~QueryServerGroupsEvent(void)    [??1QueryServerGroupsEvent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c1b50
// (public: virtual void * __thiscall RoomListItem::`scalar deleting destructor'(unsigned int)    [??_GRoomListItem@@UAEPAXI@Z])
