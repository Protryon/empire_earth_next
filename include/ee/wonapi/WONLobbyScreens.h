#ifndef EE_WONAPI_WONLOBBYSCREENS_H
#define EE_WONAPI_WONLOBBYSCREENS_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyRustCtrl.h"
namespace WONAPI {
class JoinedRoomEvent;
class LeftRoomEvent;
class ClientChangedEvent;
// UserListCtrl (0xa0 bytes, constructor 0x7ca8f0): the room's user list with Whisper and Ignore buttons.
class UserListCtrl : public LobbyRustCtrl {
public:
    UserListCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::Users) {}
};
// QuickPlayCtrl (0xb0 bytes, constructor 0x7c14e0, vtable 0x853bf0 with 51 slots): pick a skill level and
// play; six owned widgets at 0x98..0xac, destructor 0x7c1540.
class QuickPlayCtrl : public LobbyRustDialogCtrl {
public:
    QuickPlayCtrl() : LobbyRustDialogCtrl(EeWonLobbyCtlKind::QuickPlay) {}
};
// GameBrowserCtrl (R6 WONLobby round 4): the help and news browser tabs, a lobbyctl.rs object; the home location
// (assigned by the constructor and the config's HOMELOCATION) and the Paint override stay here.
class GameBrowserCtrl : public LobbyRustCtrl {
public:
    GameBrowserCtrl(const GUIString& inHome = "") : LobbyRustCtrl(EeWonLobbyCtlKind::Browser) { this->mHomeLocation = inHome; }
    virtual void Paint(Graphics& inGraphics) { this->CCall(EeWonLobbyCtlCall::Paint, (intptr_t)&inGraphics); }
    void ResetBrowser() { this->CCall(EeWonLobbyCtlCall::ResetBrowser); }
    void SetHomeLocation(const GUIString& theLocation) { this->mHomeLocation=theLocation; }
    GUIString mHomeLocation;
};
class QueryAccountResultEvent;
void DrawLobbyPing(Graphics*,unsigned short,int,int,int); // 0x7c12b0; draws a ping indicator
// GameUserListCtrl (R6 WONLobby round 4): the game room's players and Kick/Ban button, a lobbyctl.rs object
class GameUserListCtrl : public LobbyRustCtrl {
public:
    GameUserListCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::UserList) {}
};

// UpdateAccountCtrl (0xbc bytes): Container (0x84), 13 owned controls and the old password at 0xb8.
class UpdateAccountCtrl : public LobbyRustCtrl {
public:
    UpdateAccountCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::UpdateAccount) {}
};
// Login screen and its panels.
class RustServerOp;
class LoginScreen : public LobbyRustCtrl {
public:
    LoginScreen() : LobbyRustCtrl(EeWonLobbyCtlKind::LoginScreen) {}
    void ShowMOTD() { this->CCall(EeWonLobbyCtlCall::Page, 0); }
    void ShowCreateAccountCtrl() { this->CCall(EeWonLobbyCtlCall::Page, 1); }
    void ShowLostPasswordCtrl() { this->CCall(EeWonLobbyCtlCall::Page, 2); }
    void ShowLostUsernameCtrl() { this->CCall(EeWonLobbyCtlCall::Page, 3); }
};

class BrowserComponent;
class LoginHelpCtrl : public LobbyRustCtrl {
public:
    LoginHelpCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::LoginHelp) {}
    virtual void Paint(Graphics& inGraphics) { this->CCall(EeWonLobbyCtlCall::Paint, (intptr_t)&inGraphics); }
};

class BrowserComponent;
class MOTDCtrl : public LobbyRustCtrl {
public:
    MOTDCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::Motd) {}
    virtual void Paint(Graphics& inGraphics) { this->CCall(EeWonLobbyCtlCall::Paint, (intptr_t)&inGraphics); }
};

// LobbyContainer (0x94 bytes): switches between the login, internet and LAN screens.
// RootScreen is only forward-declared here (WONLobbyRoomServer.h includes this header).
class RootScreen;
class LobbyContainer : public ScreenContainer {
public:
    LobbyContainer();
    virtual ~LobbyContainer();
    virtual void InitComponent(ComponentInit&);
    void ShowInternetScreen();
    void ShowLanScreen();
    void ShowLoginScreen();
    bool OnInternetScreen();
    bool OnLanScreen();
    bool OnLoginScreen();
    int GetGameScreenType();
    SmartPtr<RootScreen> mInternetScreen; // 0x88
    SmartPtr<RootScreen> mLanScreen; // 0x8c
    SmartPtr<LoginScreen> mLoginScreen; // 0x90; 0x94 bytes
};

}
#endif
