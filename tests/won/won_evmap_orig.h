// The shells' event choices R6 WONLobby round 29 moved to Rust (ee_won_lobby evmap.rs), as they were: LobbyCtrlHost.cpp's
// CtlEventOf, LobbyDialogHost.cpp's LobbyRustDialog::HandleComponentEvent and LobbyRustPopup::HandleComponentEvent
// (copied from the units at c01702f3f; the two methods' calls out are the driver's LCall and PCall, which keep the kind).
#ifndef WON_EVMAP_ORIG_H
#define WON_EVMAP_ORIG_H
namespace WonEvmapOrig {
using namespace WONAPI;
static EeWonLobbyCtlEvent CtlEventOf(int inType)
{
    typedef EeWonLobbyCtlEvent E;
    E aType = E::Other;
    switch (inType) {
    case ComponentEvent_ButtonPressed: aType = E::ButtonPressed; break;
    case ComponentEvent_ComboSelChanged: aType = E::ComboSelChanged; break;
    case ComponentEvent_ListSelChanged: aType = E::ListSelChanged; break;
    case ComponentEvent_ListItemClicked: aType = E::ListItemClicked; break;
    case ComponentEvent_BrowserButtonChange: aType = E::BrowserButtonChange; break;
    case ComponentEvent_BrowserStatusChange: aType = E::BrowserStatusChange; break;
    case 100029: aType = E::JoinedGame; break;
    case 100032: aType = E::LeftGame; break;
    case LobbyEvent_SetRoomSpecFlags: aType = E::SetRoomSpecFlags; break;
    case 100061: aType = E::ReadyChanged; break;
    case LobbyEvent_ClientChanged: aType = E::ClientChanged; break;
    case LobbyEvent_PlayerChanged: aType = E::PlayerChanged; break;
    case 100014: aType = E::SyncOptions; break;
    case 100050: aType = E::LanOptions; break;
    case 100055: aType = E::PublicAddress; break;
    case 100065: aType = E::AllOptions; break;
    case LobbyEvent_EELobbySyncOption: aType = E::SyncOption; break;
    case LobbyEvent_EELobbySyncPlayer: aType = E::SyncPlayer; break;
    case 100064: aType = E::ColorChanged; break;
    case ComponentEvent_InputTextChanged: aType = E::InputTextChanged; break;
    case ComponentEvent_InputReturn: aType = E::InputReturn; break;
    case LobbyEvent_GameChanged: aType = E::GameChanged; break;
    case LobbyEvent_SyncGameList: aType = E::SyncGameList; break;
    case 100034: aType = E::FiltersChanged; break;
    case 100059: aType = E::RefilterAll; break;
    case 100058: aType = E::RefilterShown; break;
    case ComponentEvent_TextLinkActivated: aType = E::TextLinkActivated; break;
    case LobbyEvent_Alert: aType = E::Alert; break;
    case LobbyEvent_AnnounceFriend: aType = E::AnnounceFriend; break;
    case LobbyEvent_AnnounceClientLocation: aType = E::AnnounceClientLocation; break;
    case LobbyEvent_ClientModerated: aType = E::ClientModerated; break;
    case LobbyEvent_ChatCommandError: aType = E::ChatCommandError; break;
    case LobbyEvent_GroupInvitation: aType = E::GroupInvitation; break;
    case LobbyEvent_GroupJoinAttempt: aType = E::GroupJoinAttempt; break;
    case LobbyEvent_JoinedRoom: aType = E::JoinedRoom; break;
    case LobbyEvent_LeftRoom: aType = E::LeftRoom; break;
    case LobbyEvent_PlayerTeamChanged: aType = E::PlayerTeamChanged; break;
    case LobbyEvent_RecvChat: aType = E::RecvChat; break;
    case LobbyEvent_SetChatInput: aType = E::SetChatInput; break;
    case ComponentEvent_TreeItemOpened: aType = E::TreeItemOpened; break;
    case LobbyEvent_GroupChanged: aType = E::GroupChanged; break;
    case LobbyEvent_ServerChanged: aType = E::ServerChanged; break;
    case 100053: aType = E::SetRoomList; break;
    case LobbyEvent_FriendChanged: aType = E::FriendChanged; break;
    case 100056: aType = E::RefreshFriends; break;
    case LobbyEvent_LoginInitStatus: aType = E::LoginInitStatus; break;
    case LobbyEvent_SetLoginFields: aType = E::SetLoginFields; break;
    case 100052: aType = E::FillUserNames; break;
    case LobbyEvent_QueryAccountResult: aType = E::QueryAccountResult; break;
    case LobbyEvent_GotHTTPDocument: aType = E::GotHttpDocument; break;
    default: break;
    }
    return aType;
}

struct ComponentEvent { int mType; void* mComponent; };
struct LobbyRustDialog {
    intptr_t mKind;
    void LCall(EeWonLobbyDlgCall, intptr_t, intptr_t inKind, intptr_t) { this->mKind = inKind; }
    void HandleComponentEvent(ComponentEvent* inEvent);
};
struct LobbyRustPopup {
    intptr_t mKind;
    void PCall(EeWonLobbyPopupCall, intptr_t, intptr_t inKind) { this->mKind = inKind; }
    void HandleComponentEvent(ComponentEvent* inEvent);
};
void LobbyRustDialog::HandleComponentEvent(ComponentEvent* inEvent)
{
    EeWonLobbyDlgEvent aType = EeWonLobbyDlgEvent::Other;
    if (inEvent->mType == ComponentEvent_InputTextChanged)
        aType = EeWonLobbyDlgEvent::InputTextChanged;
    else if (inEvent->mType == ComponentEvent_ButtonPressed)
        aType = EeWonLobbyDlgEvent::ButtonPressed;
    else if (inEvent->mType == ComponentEvent_ComboSelChanged)
        aType = EeWonLobbyDlgEvent::ComboSelChanged;
    else if (inEvent->mType == LobbyEvent_GotHTTPDocument)
        aType = EeWonLobbyDlgEvent::GotHttpDocument;
    else if (inEvent->mType == ComponentEvent_ListItemClicked)
        aType = EeWonLobbyDlgEvent::ListItemClicked;
    this->LCall(EeWonLobbyDlgCall::Event, (intptr_t)inEvent, (intptr_t)aType, (intptr_t)inEvent->mComponent);
}

void LobbyRustPopup::HandleComponentEvent(ComponentEvent* inEvent)
{
    EeWonLobbyPopupEvent aType = inEvent->mType == ComponentEvent_ListItemUpClicked
                                     ? EeWonLobbyPopupEvent::ItemUpClicked
                                     : EeWonLobbyPopupEvent::Other;
    this->PCall(EeWonLobbyPopupCall::Event, (intptr_t)inEvent, (intptr_t)aType);
}
}
#endif
