#ifndef EE_WONAPI_WONLOBBYSCREENS_H
#define EE_WONAPI_WONLOBBYSCREENS_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyRustCtrl.h"
#include "rust/won_lobby/lobbyscreens.h"
namespace WONAPI {
class JoinedRoomEvent;
class LeftRoomEvent;
class ClientChangedEvent;
// (UserListCtrl, QuickPlayCtrl, GameUserListCtrl and UpdateAccountCtrl are their shells made with their kinds)
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

// LobbyContainer (0x94 bytes): switches between the login, internet and LAN screens. Since R6 WON lobby/net round 40
// its three screens (mInternetScreen +0x88, mLanScreen +0x8c, mLoginScreen +0x90) and every method are ee_won_lobby
// lobbyscreens.rs's (the host in WONLobbyPrv.cpp). RootScreen is only forward-declared here (WONLobbyRoomServer.h
// includes this header).
class RootScreen;
class LobbyContainer : public ScreenContainer {
public:
    virtual ~LobbyContainer() { this->SC(EeWonLobbyScreensCall::Drop); }
    virtual void InitComponent(ComponentInit&);
    intptr_t SC(EeWonLobbyScreensCall c, intptr_t a = 0);
    void ShowInternetScreen() { this->SC(EeWonLobbyScreensCall::ShowInternet); }
    void ShowLanScreen() { this->SC(EeWonLobbyScreensCall::ShowLan); }
    void ShowLoginScreen() { this->SC(EeWonLobbyScreensCall::ShowLogin); }
    bool OnInternetScreen() { return this->SC(EeWonLobbyScreensCall::OnInternet) != 0; }
    bool OnLanScreen() { return this->SC(EeWonLobbyScreensCall::OnLan) != 0; }
    bool OnLoginScreen() { return this->SC(EeWonLobbyScreensCall::OnLogin) != 0; }
    int GetGameScreenType() { return (int)this->SC(EeWonLobbyScreensCall::GameScreenType); }
    LoginScreen* Login(); // the screens (borrowed; NULL before InitComponent)
    RootScreen* Internet();
    RootScreen* Lan();
};

}
#endif
