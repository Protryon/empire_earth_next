// GameUserListCtrl: the player list of a game room (status icon, ready mark, name, ping) with its Kick/Ban button
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
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// FUNCTION: WON 0x007bcad0
// Creates a game player list entry for a client
GameUserListItem::GameUserListItem(LobbyClient* inClient) : mClient(inClient) {}
// The original tests in this order and reads mClient again for each test
// FUNCTION: WON 0x007bcbe0
// Picks the client's status icon, first match wins: ignored, blocked, muted, away, admin, moderator, captain, normal
void GameUserListItem::UpdateIcon()
{
    if(this->mClient->IsIgnored()) { this->mImage=::UserListCtrl_Ignored_Image; return; }
    if(this->mClient->IsBlocked()) { this->mImage=::UserListCtrl_Blocked_Image; return; }
    if(this->mClient->IsMuted(true)) { this->mImage=::UserListCtrl_Muted_Image; return; }
    if(this->mClient->IsAway()) { this->mImage=::UserListCtrl_Away_Image; return; }
    if(this->mClient->IsAdmin()) { this->mImage=::UserListCtrl_Admin_Image; return; }
    if(this->mClient->IsModerator()) { this->mImage=::UserListCtrl_Moderator_Image; return; }
    if(this->mClient->IsCaptain(1)) { this->mImage=::UserListCtrl_Captain_Image; return; }
    this->mImage=::UserListCtrl_Normal_Image;
}
// FUNCTION: WON 0x007bcdb0
// Sizes the entry to the list font's height, or the icon's if taller
void GameUserListItem::CalcDimensions(ListArea* inArea)
{
    this->mFont=inArea->mFont;
    this->mHeight=this->mFont->GetHeight();
    if(this->mImage.get()!=NULL && this->mImage->mHeight>this->mHeight) this->mHeight=this->mImage->mHeight;
}
// FUNCTION: WON 0x007bce10
// Orders the game captain first, then players by name ignoring case
int GameUserListItem::Compare(const ListItem* inOther,ListArea*) const
{
    const GameUserListItem* other=static_cast<const GameUserListItem*>(inOther);
    int mine=this->mClient->IsCaptain(1)?1:0;
    int theirs=other->mClient->IsCaptain(1)?1:0;
    if(mine!=theirs) return theirs-mine;
    return this->mClient->GetName().CompareNoCase(other->mClient->GetName());
}
// The ready mark is centered by the status icon's height, not its own (as in the original)
// FUNCTION: WON 0x007bce60
// Draws one column of the entry: selection background, status icon, ready mark, name or ping bars
void GameUserListItem::Paint(Graphics& inGraphics,ListArea* inArea)
{
    MultiListArea* area=static_cast<MultiListArea*>(inArea);
    int column=area->GetColumnPaintContext();
    unsigned long color=inGraphics.GetStandardColor(StandardColor_Text);
    if(this->mSelected) color=area->GetSelColor().GetForegroundColor(inGraphics,color);
    inGraphics.SetColor(color);
    inGraphics.SetFont(this->mFont.get());
    switch(column) {
    case -1:
        if(this->mSelected) {
            inGraphics.SetColor(area->GetSelColor().GetBackgroundColor(inGraphics));
            inGraphics.FillRect(0,0,area->GetPaintWidth(),this->mHeight);
        }
        return;
    case 0:
        inGraphics.DrawImage(this->mImage.get(),0,(this->mHeight-this->mImage->mHeight)/2);
        return;
    case 1:
        if(this->mClient->IsReady())
            inGraphics.DrawImage(::GameUserListCtrl_Ready_Image.get(),0,(this->mHeight-this->mImage->mHeight)/2);
        return;
    case 2:
        inGraphics.SetFont(this->mFont.get());
        inGraphics.DrawString(this->mClient->GetName(),0,(this->mHeight-this->mFont->GetHeight())/2);
        return;
    case 3:
        DrawLobbyPing(&inGraphics,this->mClient->GetPing(),this->mHeight-2,0,0);
        return;
    }
}

// FUNCTION: WON 0x007bcfc0
// Creates the game player list for game rooms, not yet in a game
GameUserListCtrl::GameUserListCtrl() : mInGame(false) { this->mRoomSpecFlags = 2; }
// FUNCTION: WON 0x007bd2e0
// Returns the client of the selected entry, or NULL
LobbyClient* GameUserListCtrl::GetSelectedClient()
{
    GameUserListItem* item=static_cast<GameUserListItem*>(this->mUserList->GetSelItem());
    return item==NULL?NULL:item->mClient.get();
}
// FUNCTION: WON 0x007bd300
// Returns the entry of the client with the same client id, or NULL
GameUserListItem* GameUserListCtrl::GetListItem(LobbyClient* inClient)
{
    for(int i=0;i<this->mUserList->GetNumItems();++i) {
        GameUserListItem* item=static_cast<GameUserListItem*>(this->mUserList->GetItem(i));
        LobbyClient* client=item->mClient.get();
        if(client!=NULL && client->GetClientId()==inClient->GetClientId()) return item;
    }
    return NULL;
}
// FUNCTION: WON 0x007bd350
// Kicks (or bans) the selected client
void GameUserListCtrl::HandleKickPlayer(bool inBan)
{
    LobbyClient* client=this->GetSelectedClient();
    if(client!=NULL) LobbyKickClient(client,inBan);
}
// FUNCTION: WON 0x007bd370
// Enables Kick only for the game's host, with a player other than the captain selected
void GameUserListCtrl::UpdateKickButton()
{
    if(!this->mInGame || this->mKickButton.get()==NULL) return;
    LobbyGame* game=GetMyLobbyGame();
    if(game==NULL) return;
    if(game->GetIsHost()) {
        LobbyClient* client=this->GetSelectedClient();
        if(client!=NULL && client->GetClientId()!=game->GetCaptainId()) this->mKickButton->SetEnabled(true);
        else this->mKickButton->SetEnabled(false);
    } else this->mKickButton->SetEnabled(false);
}
// FUNCTION: WON 0x007bd3f0
// Refreshes a changed client's icon and the Kick button while in a game
void GameUserListCtrl::HandleClientChanged(ClientChangedEvent* inEvent)
{
    if(!this->mInGame) return;
    if((this->mRoomSpecFlags&inEvent->mRoomSpec)!=inEvent->mRoomSpec) return;
    if(!inEvent->IsModify()) return;
    GameUserListItem* item=this->GetListItem(inEvent->mClient.get());
    if(item==NULL) return;
    item->UpdateIcon();
    this->mUserList->RefreshItem(item);
    this->UpdateKickButton();
}
// FUNCTION: WON 0x007bd450
// Adds, removes or refreshes a player's entry while in a game
void GameUserListCtrl::HandlePlayerChanged(PlayerChangedEvent* inEvent)
{
    if(!this->mInGame) return;
    if(inEvent->IsAdd()) {
        SmartPtr<GameUserListItem> item=new GameUserListItem(inEvent->mClient.get());
        item->UpdateIcon();
        this->mUserList->ListArea::InsertItem(item.get(),-1);
    } else if(inEvent->IsDelete()) {
        GameUserListItem* item=this->GetListItem(inEvent->mClient.get());
        if(item==NULL) return;
        this->mUserList->RemoveItem(item);
    } else {
        GameUserListItem* item=this->GetListItem(inEvent->mClient.get());
        if(item==NULL) return;
        item->UpdateIcon();
        this->mUserList->RefreshItem(item);
    }
}
// FUNCTION: WON 0x007bd710
// Empties the list when we leave the game
void GameUserListCtrl::HandleLeftGame()
{
    if(this->mInGame) {
        this->mInGame=false;
        this->mUserList->Clear(true);
    }
}
// FUNCTION: WON 0x007bd0b0
// Looks up the user list and the optional Kick button (disabled), and listens to broadcasts
void GameUserListCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2)
        return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config,this->mUserList,"UserList",__FILE__,159);
    this->mKickButton=dynamic_cast<Button*>(config->Fn386860("KickButton"));
    if(this->mKickButton.get()!=NULL) this->mKickButton->SetEnabled(false);
    this->SubscribeToBroadcast(1);
}
// FUNCTION: WON 0x007bd590
// On joining a game, lists its players, labels the button Kick or Ban (by room kind) and updates it
void GameUserListCtrl::HandleJoinedGame()
{
    LobbyGame* game=GetMyLobbyGame();
    if(game==NULL || !game->MatchesRoomSpec(this->mRoomSpecFlags)) return;
    LobbyClientList* clients=game->GetClients().get();
    if(clients==NULL) return;
    if(this->mKickButton.get()!=NULL) {
        if(this->mRoomSpecFlags&4) this->mKickButton->SetText(::GameUserListCtrl_Ban_String,true);
        else this->mKickButton->SetText(::GameUserListCtrl_Kick_String,true);
    }
    this->mUserList->BeginUpdate();
    this->mUserList->Clear(true);
    for(size_t i=0;i<clients->GetCount();++i) {
        LobbyClient* client=clients->ClientAt(i);
        if(!client->HasPlayer())
            continue;
        SmartPtr<GameUserListItem> item=new GameUserListItem(client);
        item->UpdateIcon();
        this->mUserList->ListArea::InsertItem(item.get(),-1);
    }
    this->mUserList->EndUpdate();
    this->mInGame=true;
    this->UpdateKickButton();
}
// FUNCTION: WON 0x007bd730
// Handles selection, Kick/Ban, the right-click client menu, double-click whisper and game/player events
void GameUserListCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch(inEvent->mType) {
    case ComponentEvent_ListSelChanged: this->UpdateKickButton(); return;
    case ComponentEvent_ButtonPressed:
        if(inEvent->mComponent==this->mKickButton.get()) {
            this->HandleKickPlayer((this->mRoomSpecFlags&4)!=0);
            return;
        }
        break;
    case ComponentEvent_ListItemClicked: {
        LobbyClient* client=this->GetSelectedClient();
        if(client==NULL) return;
        ListItemClickedEvent* clicked=static_cast<ListItemClickedEvent*>(inEvent);
        if(!clicked->IsSelectedItem()) return;
        if(clicked->mButton==2)
            this->DeliverEvent(new ClientActionEvent(18,this->GetRoomSpecFlags(),client),false);
        else if(clicked->mButton==0 && clicked->mDoubleClick)
            LobbyBroadcastEvent(new SetChatInputEvent(2,client->GetName(),this->mRoomSpecFlags,false),true);
        return;
    }
    case LobbyEvent_ClientChanged: this->HandleClientChanged(static_cast<ClientChangedEvent*>(inEvent)); return;
    case 100029: this->HandleJoinedGame(); return;
    case 100032: this->HandleLeftGame(); return;
    case LobbyEvent_PlayerChanged: this->HandlePlayerChanged(static_cast<PlayerChangedEvent*>(inEvent)); return;
    case LobbyEvent_SetRoomSpecFlags: this->mRoomSpecFlags|=static_cast<SetRoomSpecFlagsEvent*>(inEvent)->mFlags; return;
    }
    Component::HandleComponentEvent(inEvent);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007654c0
// Creates the layout reader for a game player list
ComponentConfig* GameUserListCtrlConfig::CfgFactory()
{
    return new GameUserListCtrlConfig;
}
}

namespace WONAPI {
// FUNCTION: WON 0x007bda70
// Accepts (and ignores) the CTRLLINKID layout key; passes other keys to the container's reader
bool GameUserListCtrlConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_GAMEUSERLISTCTRLCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) { }
    else return ContainerConfig::Virtual24(instruction);
    return true;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007bcb30
// (public: virtual __thiscall WONAPI::GameUserListItem::~GameUserListItem(void)    [??1GameUserListItem@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007bd9e0
// (public: virtual __thiscall WONAPI::SetChatInputEvent::~SetChatInputEvent(void)    [??1SetChatInputEvent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007bcb10
// (public: virtual void * __thiscall WONAPI::GameUserListItem::`scalar deleting destructor'(unsigned int)    [??_GGameUserListItem@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007bcff0
// (public: virtual void * __thiscall WONAPI::GameUserListCtrl::`scalar deleting destructor'(unsigned int)    [??_GGameUserListCtrl@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007bd010
// (public: virtual __thiscall WONAPI::GameUserListCtrl::~GameUserListCtrl(void)    [??1GameUserListCtrl@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007bd9c0
// (public: virtual void * __thiscall WONAPI::SetChatInputEvent::`scalar deleting destructor'(unsigned int)    [??_GSetChatInputEvent@WONAPI@@UAEPAXI@Z])
