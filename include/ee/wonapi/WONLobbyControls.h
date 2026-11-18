// WONLobby controls: a Container base (0x84 bytes) plus their own fields.

#ifndef EE_WONAPI_WONLOBBYCONTROLS_H
#define EE_WONAPI_WONLOBBYCONTROLS_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyRustCtrl.h"
namespace WONAPI {
class LoginInitStatusEvent;
class LoginStatusCtrl : public LobbyRustCtrl {
public:
    LoginStatusCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::LoginStatus) {}
};
class InitFailedCtrl : public LobbyRustCtrl {
public:
    InitFailedCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::InitFailed) {}
};
// GameStagingCtrl (R6 WONLobby round 4): the game room's leave/dissolve and start/ready buttons, a lobbyctl.rs object
class GameStagingCtrl : public LobbyRustCtrl {
public:
    GameStagingCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::Staging) {}
};
}

// Proposal depends on the root-admitted cohesive GUI/events/rich-text batch.
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyRichText.h"
namespace WONAPI {
class ChatCommandParser;
// Forward declarations (pointers only).
class LeftRoomEvent;
class SetChatInputEvent;
class ChatCommandErrorEvent;
}

// LoginCtrl and related declarations.
namespace WONAPI {
class InputBox;
class LoginEvent;
class LoginInitStatusEvent;
class SetLoginFieldsEvent;
class LoginCtrl : public LobbyRustDialogCtrl {
public:
    LoginCtrl() : LobbyRustDialogCtrl(EeWonLobbyCtlKind::Login) {}
};
}


// LobbyOptionsCtrl.
namespace WONAPI {
class ImageComponent;
// LobbyOptionsCtrl (R6 WONLobby round 4): the LAN or Internet options tab, a lobbyctl.rs object
class LobbyOptionsCtrl : public LobbyRustCtrl {
public:
    explicit LobbyOptionsCtrl(bool inIsLan)
        : LobbyRustCtrl(inIsLan ? EeWonLobbyCtlKind::LanOptions : EeWonLobbyCtlKind::NetOptions) {}
};
}


// FriendsListCtrl.
namespace WONAPI {
class FriendsListCtrl : public LobbyRustCtrl {
public:
    FriendsListCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::Friends) {}
    static ComponentConfig* CfgFactory(); // at 0x00765320
    static Component* CompFactory(); // at 0x007652c0
};
}


// LostPasswordCtrl and LostUserNameCtrl.
namespace WONAPI {
class LostPasswordCtrl : public LobbyRustCtrl {
public:
    LostPasswordCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::LostPassword) {}
};
class LostUserNameCtrl : public LobbyRustCtrl {
public:
    LostUserNameCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::LostUserName) {}
};
}


// CreateAccountCtrl (0xbc bytes): RTTI CreateAccountCtrl > Container > Component > RefCount,
// vtable 0x00852e3c.
namespace WONAPI {
class CreateAccountCtrl : public LobbyRustCtrl {
public:
    CreateAccountCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::CreateAccount) {}
};
}


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
