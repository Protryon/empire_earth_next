// WONLobby controls. The one-line subclasses (LoginStatusCtrl, InitFailedCtrl, GameStagingCtrl, LoginCtrl,
// LobbyOptionsCtrl, FriendsListCtrl, LostPasswordCtrl, LostUserNameCtrl, CreateAccountCtrl; R6 WONLobby round 20)
// were their shells, LobbyRustCtrl and LobbyRustDialogCtrl, made with their kinds: the resource table makes those.

#ifndef EE_WONAPI_WONLOBBYCONTROLS_H
#define EE_WONAPI_WONLOBBYCONTROLS_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyRustCtrl.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyRichText.h"
// ChatCtrl (R6 WONLobby round 13): a chat panel, a lobbyctl.rs object (chatctl.rs: the controls, fonts, colours,
// room kind, client id and room name, every handler); its config reader stores the room kind and colours through
// SetField. The links it prints are LobbyNetCtrlHost.cpp's.
namespace WONAPI {
class ChatCtrl : public LobbyRustCtrl {
public:
    ChatCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::Chat) {}
    void SetField(int inField, intptr_t inValue) { this->CCall(EeWonLobbyCtlCall::SetField, inField, inValue); }
};
}

#endif
