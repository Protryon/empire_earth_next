
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyScreens.h"
namespace WONAPI {
// UserListItem: a client of the chat room, or a separator line between the list's groups
// FUNCTION: WON 0x007ca6a0
// Creates a room user list entry with no client (a separator line until a client is set)
UserListItem::UserListItem() { this->mSortKey=0; this->mShowSeparator=true; }
// FUNCTION: WON 0x007ca820
// Orders entries by sort key, highest first (admins, moderators, separator, friends, separator, others), then text
int UserListItem::Compare(const ListItem* inOther,ListArea* inArea) const
{
    const UserListItem* other=static_cast<const UserListItem*>(inOther);
    int result=other->mSortKey-this->mSortKey;
    if(result!=0) return result;
    return TextListItem::Compare(inOther,inArea);
}
// FUNCTION: WON 0x007ca840
// Sizes a separator (8 pixels high when shown, else 1) or a client's text entry
void UserListItem::CalcDimensions(ListArea* inArea)
{
    if(this->mClient.get()==NULL) {
        this->mWidth=1;
        if(this->mShowSeparator) this->mHeight=8;
        else this->mHeight=1;
    } else TextListItem::CalcDimensions(inArea);
}
// Graphics::SetSysColor is the original's function at 0x726d40
// FUNCTION: WON 0x007ca880
// Draws a shown separator as a line across the list, or a client's text entry
void UserListItem::Paint(Graphics& inGraphics,ListArea* inArea)
{
    if(this->mClient.get()==NULL) {
        if(this->mShowSeparator) {
            int width=inArea->mTotalWidth+10;
            if(inArea->mPaintWidth>width) width=inArea->mPaintWidth;
            int y=this->mHeight>>2;
            inGraphics.SetSysColor(StandardColor_Text);
            inGraphics.DrawLine(0,y,width,y);
        }
    } else TextListItem::Paint(inGraphics,inArea);
}
// FUNCTION: WON 0x007ca8f0
// Creates the room user list with room flags 5, not updating
UserListCtrl::UserListCtrl() { this->mUpdating = false; this->mModeFlags = 5; }
// explicit: the original destructor stores its own vptr (0x7ca97f) before releasing five members
// FUNCTION: WON 0x007ca960
// Destroys the room user list, releasing its list, buttons and separators
UserListCtrl::~UserListCtrl() {}
// FUNCTION: WON 0x007cac50
// Returns a client's status icon, first match wins: ignored, blocked, muted, away, admin, moderator, captain, normal
Image* GetLobbyClientIcon(LobbyClient* inClient,bool inMode)
{
    if(inClient==NULL) return NULL;
    if(inClient->mIgnored) return ::UserListCtrl_Ignored_Image.get();
    if(inClient->mBlocked) return ::UserListCtrl_Blocked_Image.get();
    if(inClient->IsMuted(inMode)) return ::UserListCtrl_Muted_Image.get();
    if(inClient->IsAway()) return ::UserListCtrl_Away_Image.get();
    if(inClient->IsAdmin()) return ::UserListCtrl_Admin_Image.get();
    if(inClient->IsModerator()) return ::UserListCtrl_Moderator_Image.get();
    return inClient->IsCaptain(inMode)?::UserListCtrl_Captain_Image.get() : ::UserListCtrl_Normal_Image.get();
}
// FUNCTION: WON 0x007caa70
// Fills an entry from a client: name, status icon, sort group, and bold for friends who are not anonymous
void UserListCtrl::UpdateItem(LobbyClient* inClient,UserListItem* inItem)
{
    inItem->mClient=inClient;
    unsigned char sortKey;
    if(inClient->IsAdmin()) sortKey=5;
    else if(inClient->IsModerator()) sortKey=4;
    else sortKey = inClient->IsFriend() && !inClient->IsAnonymous() ? 2 : 0;
    inItem->mSortKey=sortKey;
    inItem->mText=inClient->mName;
    inItem->mImage=GetLobbyClientIcon(inClient,(this->mModeFlags&2)!=0);
    if(inClient->IsFriend() && !inClient->IsAnonymous()) inItem->mFont=this->GetDefaultFontMod(1,0);
    else inItem->mFont=this->GetDefaultFont();
}
// FUNCTION: WON 0x007cacf0
// Shows each separator only when it has entries on both sides
void UserListCtrl::RefreshSeparators()
{
    this->mUserList->RefreshItem(this->mFirstSeparator.get());
    this->mUserList->RefreshItem(this->mSecondSeparator.get());
    int first=this->mUserList->GetItemPos(this->mFirstSeparator.get());
    int second=this->mUserList->GetItemPos(this->mSecondSeparator.get());
    bool before=first>0;
    bool middle=second>first+1;
    bool after=second<this->mUserList->GetNumItems()-1;
    bool showFirst=before&&(middle||after);
    bool showSecond=middle&&after;
    if(this->mFirstSeparator->mShowSeparator!=showFirst) {
        this->mFirstSeparator->mShowSeparator=showFirst;
        this->mUserList->RefreshItem(this->mFirstSeparator.get());
    }
    if(this->mSecondSeparator->mShowSeparator!=showSecond) {
        this->mSecondSeparator->mShowSeparator=showSecond;
        this->mUserList->RefreshItem(this->mSecondSeparator.get());
    }
}
// FUNCTION: WON 0x007cadf0
// Looks up the user list and the Whisper and Ignore buttons, creates the two separators, listens to broadcasts
void UserListCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2) return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config,this->mUserList,"UserList",__FILE__,194);
    SafeGetComponent(config,this->mWhisperButton,"WhisperButton",__FILE__,195);
    SafeGetComponent(config,this->mIgnoreButton,"IgnoreButton",__FILE__,196);
    this->SubscribeToBroadcast(1);
    this->mFirstSeparator=new UserListItem;
    this->mFirstSeparator->mSelectable=false;
    this->mFirstSeparator->mSortKey=3;
    this->mSecondSeparator=new UserListItem;
    this->mSecondSeparator->mSelectable=false;
    this->mSecondSeparator->mSortKey=1;
    this->UpdateButtons();
}
// FUNCTION: WON 0x007cb0e0
// Returns the entry of the client with the same client id, or NULL
UserListItem* UserListCtrl::FindItem(LobbyClient* inClient)
{
    for(int i=0;i<this->mUserList->GetNumItems();++i) {
        UserListItem* item=static_cast<UserListItem*>(this->mUserList->GetItem(i));
        if(item->mClient.get()!=NULL && item->mClient->mClientId==inClient->mClientId)
            return item;
    }
    return NULL;
}
// FUNCTION: WON 0x007cb130
// Returns the selected entry, or NULL
UserListItem* UserListCtrl::GetSelectedItem()
{
    return static_cast<UserListItem*>(this->mUserList->mSelItem);
}
// FUNCTION: WON 0x007cb140
// Returns the client of the selected entry, or NULL
LobbyClient* UserListCtrl::GetSelectedClient()
{
    UserListItem* item=static_cast<UserListItem*>(this->mUserList->mSelItem);
    return item==NULL?NULL:item->mClient.get();
}
// FUNCTION: WON 0x007cb160
// Enables Whisper and Ignore while a client is selected, labelling Ignore or Unignore by its state
void UserListCtrl::UpdateButtons()
{
    LobbyClient* client=this->GetSelectedClient();
    if(client==NULL) {
        this->mIgnoreButton->SetText(::UserListCtrl_Ignore_String,true);
        this->mIgnoreButton->SetEnabled(false);
        this->mWhisperButton->SetEnabled(false);
    } else {
        this->mWhisperButton->SetEnabled(true);
        this->mIgnoreButton->SetEnabled(true);
        if(client->mIgnored) this->mIgnoreButton->SetText(::UserListCtrl_Unignore_String,true);
        else this->mIgnoreButton->SetText(::UserListCtrl_Ignore_String,true);
    }
}
// FUNCTION: WON 0x007cb200
// Fills the list with the joined room's clients and the two separators
void UserListCtrl::JoinedRoom(JoinedRoomEvent* inEvent)
{
    if((this->mModeFlags&inEvent->mRoomSpec)!=inEvent->mRoomSpec) return;
    this->mUpdating=true;
    this->mUserList->BeginUpdate();
    this->mUserList->Clear(true);
    this->mUserList->InsertItem(this->mFirstSeparator.get(),-1);
    this->mUserList->InsertItem(this->mSecondSeparator.get(),-1);
    LobbyClientList* clients=inEvent->mClients.get();
    for(LobbyClientMap::iterator i=clients->mClients.begin();i!=clients->mClients.end();++i) {
        LobbyClient* client=i->second.get();
        SmartPtr<UserListItem> item=new UserListItem;
        this->UpdateItem(client,item.get());
        this->mUserList->InsertItem(item.get(),-1);
    }
    this->mUserList->EndUpdate();
    this->UpdateButtons();
    this->RefreshSeparators();
}
// FUNCTION: WON 0x007cb360
// Empties the list when we leave the room
void UserListCtrl::LeftRoom(LeftRoomEvent* inEvent)
{
    if((this->mModeFlags&inEvent->mRoomSpec)!=inEvent->mRoomSpec) return;
    this->mUpdating=false;
    this->mUserList->Clear(true);
    this->UpdateButtons();
}
// FUNCTION: WON 0x007cb3a0
// Adds, removes or refreshes a client's entry, then updates the buttons and separators
void UserListCtrl::ClientChanged(ClientChangedEvent* inEvent)
{
    if((this->mModeFlags&inEvent->mRoomSpec)!=inEvent->mRoomSpec) return;
    if(inEvent->IsAdd()) {
        SmartPtr<UserListItem> item=new UserListItem;
        LobbyClient* client=inEvent->mClient.get();
        this->UpdateItem(client,item.get());
        this->mUserList->InsertItem(item.get(),-1);
    } else if(inEvent->IsDelete()) {
        UserListItem* item=this->FindItem(inEvent->mClient.get());
        if(item==NULL) return;
        this->mUserList->RemoveItem(item);
    } else {
        UserListItem* item=this->FindItem(inEvent->mClient.get());
        if(item==NULL) return;
        this->UpdateItem(inEvent->mClient.get(),item);
        this->mUserList->RefreshItem(item);
    }
    this->UpdateButtons();
    this->RefreshSeparators();
}
// FUNCTION: WON 0x007cb4d0
// Handles room/client events, Ignore/Unignore, Whisper, the right-click client menu and double-click whisper
void UserListCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch(inEvent->mType) {
    case LobbyEvent_ClientChanged: this->ClientChanged(static_cast<ClientChangedEvent*>(inEvent)); return;
    case LobbyEvent_JoinedRoom: this->JoinedRoom(static_cast<JoinedRoomEvent*>(inEvent)); return;
    case LobbyEvent_LeftRoom: this->LeftRoom(static_cast<LeftRoomEvent*>(inEvent)); return;
    case ComponentEvent_ButtonPressed: {
        Component* anIgnore=this->mIgnoreButton.get();
        Component* component=inEvent->mComponent;
        if(component==anIgnore) {
            LobbyClient* client=this->GetSelectedClient();
            if(client!=NULL)
                this->DeliverEvent(new ClientActionEvent(client->IsIgnored()?12:5,this->GetRoomSpecFlags(),client),false);
        } else if(component==this->mWhisperButton.get()) {
            LobbyClient* client=this->GetSelectedClient();
            if(client!=NULL)
                LobbyBroadcastEvent(new SetChatInputEvent(2,client->GetName(),this->mModeFlags,false),true);
        }
        return;
    }
    case ComponentEvent_ListSelChanged: this->UpdateButtons(); return;
    case ComponentEvent_ListItemClicked: {
        LobbyClient* client=this->GetSelectedClient();
        if(client==NULL) return;
        ListItemClickedEvent* clicked=static_cast<ListItemClickedEvent*>(inEvent);
        ListItem* item=clicked->mItem.get();
        if(this->GetSelectedItem()!=item) return;
        if(clicked->GetButton()==2)
            this->DeliverEvent(new ClientActionEvent(18,this->GetRoomSpecFlags(),client),false);
        else if(clicked->GetButton()==0 && clicked->IsDoubleClick())
            LobbyBroadcastEvent(new SetChatInputEvent(2,client->GetName(),this->mModeFlags,false),true);
        return;
    }
    default: Component::HandleComponentEvent(inEvent); return;
    }
}
}


namespace WONAPI {
// FUNCTION: WON 0x00765a60
// Creates the layout reader for a room user list
ComponentConfig* UserListCtrlConfig::CfgFactory()
{
    return new UserListCtrlConfig;
}
}

namespace WONAPI {
// FUNCTION: WON 0x007cb770
// Accepts (and ignores) the GROUPTYPE and CTRLLINKID layout keys; passes other keys to the container's reader
bool UserListCtrlConfig::Virtual24(const std::string& instruction)
{
    if(instruction=="GROUPTYPE") { }
    else if(instruction=="CTRLLINKID") { }
    else return ContainerConfig::Virtual24(instruction);
    return true;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007ca940
// (public: virtual void * __thiscall WONAPI::UserListCtrl::`scalar deleting destructor'(unsigned int)    [??_GUserListCtrl@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007ca720
// (public: virtual void * __thiscall WONAPI::UserListItem::`scalar deleting destructor'(unsigned int)    [??_GUserListItem@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007ca740
// (public: virtual __thiscall WONAPI::UserListItem::~UserListItem(void)    [??1UserListItem@WONAPI@@UAE@XZ])
