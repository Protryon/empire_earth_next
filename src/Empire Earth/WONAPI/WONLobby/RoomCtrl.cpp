
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
#include "rust/common.h"
#include "rust/won_gui/barobj.h"
#include "rust/won_gui/btnobj.h"
#include "rust/won_gui/cfgtab.h"
#include "rust/won_gui/comboobj.h"
#include "rust/won_gui/compmem.h"
#include "rust/won_gui/compobj.h"
#include "rust/won_gui/ctobj.h"
#include "rust/won_gui/dialogobj.h"
#include "rust/won_gui/fontmap.h"
#include "rust/won_gui/gfxobj.h"
#include "rust/won_gui/grabobj.h"
#include "rust/won_gui/guicfg.h"
#include "rust/won_gui/guiconf.h"
#include "rust/won_gui/guictl.h"
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/guiedit.h"
#include "rust/won_gui/guievent.h"
#include "rust/won_gui/guifocus.h"
#include "rust/won_gui/guihost.h"
#include "rust/won_gui/guilayout.h"
#include "rust/won_gui/guilist.h"
#include "rust/won_gui/guimenu.h"
#include "rust/won_gui/guimulti.h"
#include "rust/won_gui/guiscroll.h"
#include "rust/won_gui/guistring.h"
#include "rust/won_gui/guistrobj.h"
#include "rust/won_gui/guitext.h"
#include "rust/won_gui/guitree.h"
#include "rust/won_gui/guiwin.h"
#include "rust/won_gui/guiwrap.h"
#include "rust/won_gui/imgdec.h"
#include "rust/won_gui/imgfilter.h"
#include "rust/won_gui/inputobj.h"
#include "rust/won_gui/jpeg.h"
#include "rust/won_gui/labelobj.h"
#include "rust/won_gui/layoutobj.h"
#include "rust/won_gui/listobj.h"
#include "rust/won_gui/menuobj.h"
#include "rust/won_gui/msbtnobj.h"
#include "rust/won_gui/mspaint.h"
#include "rust/won_gui/multiobj.h"
#include "rust/won_gui/paneobj.h"
#include "rust/won_gui/platcursor.h"
#include "rust/won_gui/platfont.h"
#include "rust/won_gui/platgfx.h"
#include "rust/won_gui/platimg.h"
#include "rust/won_gui/platwin.h"
#include "rust/won_gui/platwm.h"
#include "rust/won_gui/rootobj.h"
#include "rust/won_gui/scrollerobj.h"
#include "rust/won_gui/skinlay.h"
#include "rust/won_gui/splitobj.h"
#include "rust/won_gui/stretchobj.h"
#include "rust/won_gui/surface.h"
#include "rust/won_gui/tabobj.h"
#include "rust/won_gui/textobj.h"
#include "rust/won_gui/treeobj.h"
#include "rust/won_gui/winobj.h"
#include "rust/won_gui/wmcache.h"
#include "rust/won_gui/wmloop.h"
#include "rust/won_gui/wmobj.h"
#include "rust/won_gui/wonbtnobj.h"
#include "rust/won_gui/wrapobj.h"
#include "rust/won_lobby/chatobj.h"
#include "rust/won_lobby/dialogs.h"
#include "rust/won_lobby/dlglogic.h"
#include "rust/won_lobby/eecolor.h"
#include "rust/won_lobby/eegame.h"
#include "rust/won_lobby/eeglue.h"
#include "rust/won_lobby/eeresource.h"
#include "rust/won_lobby/eesound.h"
#include "rust/won_lobby/eexfer.h"
#include "rust/won_lobby/ffi_lan.h"
#include "rust/won_lobby/flow.h"
#include "rust/won_lobby/lanlogic.h"
#include "rust/won_lobby/lanobj.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/lobbyhost.h"
#include "rust/won_lobby/lobbymap.h"
#include "rust/won_lobby/lobbypopup.h"
#include "rust/won_lobby/logics.h"
#include "rust/won_lobby/model.h"
#include "rust/won_lobby/modelobj.h"
#include "rust/won_lobby/persist.h"
#include "rust/won_lobby/prefsobj.h"
#include "rust/won_lobby/resource.h"
#include "rust/won_lobby/routelogic.h"
#include "rust/won_lobby/routing.h"
#include "rust/won_lobby/scrolltext.h"
#include "rust/won_lobby/staging.h"
#include "rust/won_lobby/stagingobj.h"
#include "rust/won_lobby/statusstr.h"
#include "rust/won_net/admin.h"
#include "rust/won_net/adminop.h"
#include "rust/won_net/auth.h"
#include "rust/won_net/authcpp.h"
#include "rust/won_net/authctx.h"
#include "rust/won_net/authflow.h"
#include "rust/won_net/authlogin.h"
#include "rust/won_net/authobj.h"
#include "rust/won_net/blowfish.h"
#include "rust/won_net/buffer.h"
#include "rust/won_net/cachemap.h"
#include "rust/won_net/cdkeystore.h"
#include "rust/won_net/cert.h"
#include "rust/won_net/certhost.h"
#include "rust/won_net/chat.h"
#include "rust/won_net/config.h"
#include "rust/won_net/configobj.h"
#include "rust/won_net/db.h"
#include "rust/won_net/debug.h"
#include "rust/won_net/dir.h"
#include "rust/won_net/dirchange.h"
#include "rust/won_net/dirops.h"
#include "rust/won_net/elgamal.h"
#include "rust/won_net/ffi.h"
#include "rust/won_net/ffi_net.h"
#include "rust/won_net/ffi_ops.h"
#include "rust/won_net/ffi_parse.h"
#include "rust/won_net/file.h"
#include "rust/won_net/filepush.h"
#include "rust/won_net/ftp.h"
#include "rust/won_net/ftpop.h"
#include "rust/won_net/fwhost.h"
#include "rust/won_net/fwop.h"
#include "rust/won_net/gamespy.h"
#include "rust/won_net/getcert.h"
#include "rust/won_net/hostauth.h"
#include "rust/won_net/hostbase.h"
#include "rust/won_net/hostnative.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/http.h"
#include "rust/won_net/httpcache.h"
#include "rust/won_net/httpcpp.h"
#include "rust/won_net/httpdoc.h"
#include "rust/won_net/httpengine.h"
#include "rust/won_net/httpget.h"
#include "rust/won_net/ipobj.h"
#include "rust/won_net/md5.h"
#include "rust/won_net/miscops.h"
#include "rust/won_net/multiping.h"
#include "rust/won_net/ops.h"
#include "rust/won_net/patch.h"
#include "rust/won_net/path.h"
#include "rust/won_net/peerauth.h"
#include "rust/won_net/peerobj.h"
#include "rust/won_net/peerop.h"
#include "rust/won_net/perftimer.h"
#include "rust/won_net/qsock.h"
#include "rust/won_net/random.h"
#include "rust/won_net/regauth.h"
#include "rust/won_net/regkey.h"
#include "rust/won_net/reqdir.h"
#include "rust/won_net/reqhost.h"
#include "rust/won_net/reqlast.h"
#include "rust/won_net/reqmake.h"
#include "rust/won_net/reqop.h"
#include "rust/won_net/reqops.h"
#include "rust/won_net/routeconn.h"
#include "rust/won_net/routecpp.h"
#include "rust/won_net/routemsg.h"
#include "rust/won_net/routeop.h"
#include "rust/won_net/servconn.h"
#include "rust/won_net/servctx.h"
#include "rust/won_net/server.h"
#include "rust/won_net/smartroute.h"
#include "rust/won_net/sockobj.h"
#include "rust/won_net/sockop.h"
#include "rust/won_net/sockops.h"
#include "rust/won_net/sockthread.h"
#include "rust/won_net/strcase.h"
#include "rust/won_net/titlecfg.h"
#include "rust/won_net/udpmgr.h"
#include "rust/won_net/udpops.h"
#include "rust/won_net/userstatus.h"
#include "rust/won_net/verop.h"
#include "rust/won_net/wonfile.h"
using namespace WONAPI;
// RoomListItem: an entry of the room tree (a server or one of its chat rooms), private to this file
// Creates a room tree entry for a server, or its 'querying server' placeholder when loading
RoomListItem::RoomListItem(WONAPI::LobbyServer* inServer,bool inLoading)
    :mColor(-1),mLoading(inLoading),mAddress(inServer->GetAddress()),mGroupId(0xffff)
{
    this->UpdateText(inServer,NULL);
}
// Creates a room tree entry for a chat room on a server
RoomListItem::RoomListItem(WONAPI::LobbyGroup* inGroup,WONAPI::LobbyServer* inServer)
    :mColor(-1),mLoading(false),mAddress(inServer->GetAddress()),mGroupId(inGroup->GetId())
{
    this->UpdateText(inServer,inGroup);
}
// Orders room tree entries by their text, ignoring case
int RoomListItem::Compare(const WONAPI::ListItem* inOther,WONAPI::ListArea*) const
{
    return this->mText.CompareNoCase(static_cast<const RoomListItem*>(inOther)->mText);
}
// Sets the entry's text ('querying server', or the room or server name with its player count) and lock icon
void RoomListItem::UpdateText(WONAPI::LobbyServer* inServer,WONAPI::LobbyGroup* inGroup)
{
    char aCount[50];
    if(this->mLoading) this->mText=LOBBY_STRING(RoomCtrl_QueryingServer_String);
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
    if(inGroup!=NULL && inGroup->IsPasswordProtected()) this->mImage=LOBBY_IMAGE(RoomCtrl_Password_Image);
    else this->mImage=NULL;
}
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
// Draws the entry with its selection highlight, lock icon and text
void RoomListItem::Paint(WONAPI::Graphics& g,WONAPI::ListArea* inList)
{
    int aColor=this->mColor;
    if(this->mSelected)
    {
        g.SetColor(inList->GetSelColor().GetBackgroundColor(g));
        int aWidth=inList->GetPaintWidth();
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
// Enables Join only while a room tree entry is selected
void RoomCtrl::EnableControls()
{
    this->mJoinButton->SetEnabled(this->mRoomList->GetSelItem()!=NULL);
}
// Adds a chat room entry under a server entry
void RoomCtrl::AddGroup(TreeItem* inParent,LobbyServer* inServer,LobbyGroup* inGroup)
{
    RoomListItem* aItem=new RoomListItem(inGroup,inServer);
    this->mRoomList->InsertTreeItem(inParent,aItem,-1);
}
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
// Returns the server entry with the given address, or NULL
TreeItem* RoomCtrl::GetServerItem(const IPAddr& inAddress)
{
    for(int i=0;i<this->mRoomList->GetNumItems();++i)
    {
        TreeItem* aTree=static_cast<TreeItem*>(this->mRoomList->GetItem(i));
        RoomListItem* aItem=static_cast<RoomListItem*>(aTree->GetWrappedItem());
        if(aItem->mAddress==inAddress) return aTree;
    }
    return NULL;
}
// Returns the entry of the room with the given id under the server at the given address, or NULL
TreeItem* RoomCtrl::GetGroupItem(const IPAddr& inAddress,int inGroupId)
{
    TreeItem* aServer=this->GetServerItem(inAddress);
    if(aServer==NULL)
        return NULL;
    for(int i=0;i<aServer->GetNumChildren();++i)
    {
        TreeItem* aChild=aServer->GetChild(i);
        if(static_cast<RoomListItem*>(aChild->GetWrappedItem())->mGroupId==static_cast<unsigned long>(inGroupId)) return aChild;
    }
    return NULL;
}
// Returns a server's entry, or NULL
TreeItem* RoomCtrl::GetServerItem(LobbyServer* inServer)
{
    if(inServer!=NULL) return this->GetServerItem(inServer->GetAddress());
    return NULL;
}
// Returns a chat room's entry on a server, or NULL
TreeItem* RoomCtrl::GetGroupItem(LobbyServer* inServer,LobbyGroup* inGroup)
{
    if(inServer!=NULL && inGroup!=NULL) return this->GetGroupItem(inServer->GetAddress(),inGroup->GetId());
    return NULL;
}

// Asks the lobby to join the selected room (or server)
void RoomCtrl::DoJoinRoom()
{
    TreeItem* aSelected=static_cast<TreeItem*>(this->mRoomList->GetSelItem());
    if(aSelected!=NULL)
    {
        RoomListItem* aItem=static_cast<RoomListItem*>(aSelected->GetWrappedItem());
        this->DeliverEvent(new JoinRoomEvent(aItem->mAddress,aItem->mGroupId),false);
    }
}
// Asks the lobby to create a room on the selected server (no address when nothing is selected)
void RoomCtrl::DoCreateRoom()
{
    IPAddr aAddress;
    TreeItem* aSelected=static_cast<TreeItem*>(this->mRoomList->GetSelItem());
    if(aSelected!=NULL) aAddress=static_cast<RoomListItem*>(aSelected->GetWrappedItem())->mAddress;
    this->DeliverEvent(new CreateRoomEvent(aAddress),false);
}
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
        static_cast<RoomListItem*>(aGroupItem->GetWrappedItem())->UpdateText(aServer,aGroup);
        static_cast<RoomListItem*>(aGroupItem->GetParentItem()->GetWrappedItem())->UpdateText(aServer,NULL);
        this->mRoomList->RefreshItem(aGroupItem->GetParentItem());
        this->mRoomList->RefreshItem(aGroupItem);
    }
    this->EnableControls();
}
// Asks the lobby to query a server's rooms when its entry is opened
void RoomCtrl::HandleItemOpened(TreeItemOpenedEvent* inEvent)
{
    TreeItem* aTree=inEvent->mItem.get();
    if(aTree->IsOpen())
    {
        RoomListItem* aItem=static_cast<RoomListItem*>(aTree->GetWrappedItem());
        this->DeliverEvent(new QueryServerGroupsEvent(aItem->mAddress),false);
    }
}
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
    bool aOpen=aServerItem->IsOpen();
    TreeItem* aSelected=static_cast<TreeItem*>(this->mRoomList->GetSelItem());
    unsigned long aGroupId=0xffff;
    IPAddr aAddress;
    if(aSelected!=NULL && aSelected->GetParentItem()==aServerItem)
    {
        RoomListItem* aItem=static_cast<RoomListItem*>(aSelected->GetWrappedItem());
        aGroupId=aItem->mGroupId;
        aAddress=aItem->mAddress;
    }
    int aPosition=aServerItem->GetIndex();
    int aVertOffset=this->mRoomList->GetVertOffset();
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
