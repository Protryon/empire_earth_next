// Room and server list controls. RoomLabelCtrl (0xa0 bytes): the label naming the current room.

#ifndef EE_WONAPI_WONLOBBYROOMSERVER_H
#define EE_WONAPI_WONLOBBYROOMSERVER_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONLobbyRustCtrl.h"
namespace WONAPI {
class RoomLabelCtrl : public LobbyRustCtrl {
public:
    RoomLabelCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::RoomLabel) {}
    void SetField(int inField, intptr_t inValue) { this->CCall(EeWonLobbyCtlCall::SetField, inField, inValue); }
};
}
// Widgets used by RootScreen.
namespace WONAPI {
// MSSeperator: 0x30 bytes, a Component with no members of its own.
class MSSeperator : public Component {
public:
    MSSeperator(int=6);
    virtual void GetDesiredSize(int& width,int& height) { width=100; height=6; } // inline as in the SDK (0x700700)
    virtual void Paint(Graphics&); // 0x700720, slot 0x18
};
class RectangleComponent3D : public Component {
public:
    RectangleComponent3D(bool up=true,int thickness=2); // 0x7068f0; object size 0x44
    virtual void Paint(Graphics&); // 0x706930, slot 0x18
    // Implicit destructor: nothing of its own to free.
    int mSolidColor,mLightColor,mDarkColor; // 0x30, 0x34, 0x38 (set at 0x794c3b, 0x794c5f, 0x794c83)
    int mThickness; // 0x3c (set by RootScreen at 0x7c47fa)
    bool mUp; // 0x40 (set at 0x794ca7)
    void SetSolidColor(int c) { this->mSolidColor=c; }
    void SetLightColor(int c) { this->mLightColor=c; }
    void SetDarkColor(int c) { this->mDarkColor=c; }
    void SetThickness(int n) { this->mThickness=n; }
    void SetIsUp(bool up) { this->mUp=up; }
};
}

// RootScreen (0xcc bytes): the internet or LAN lobby screen with its tabs (lobby, game, direct connect,
// LAN, help, news).
namespace WONAPI {
class RootScreen : public Container {
public:
    RootScreen(bool);
    virtual ~RootScreen(); // writes its own vtable pointer (0x7c3d2f); frees ten members, then the base
    virtual void InitComponent(ComponentInit&);
    virtual void Virtual08(); // 0x7c46d0; slot 0x08 (SizeChanged)
    virtual void HandleComponentEvent(ComponentEvent*);
    bool OnDirectConnectTab();
    bool OnLanTab();
    void EnterGameTab(bool);
    void ShowLobbyTab();
    void HandleJoinedGame();
    void HandleLeftGame();
    void HandleEnteredScreen();
    void HandleTabCtrlTabSelected(ComponentEvent*);
    bool mIsLan; // 0x84
    SmartPtr<TabCtrl> mTabCtrl; // 0x88
    GUIString mDefaultTabName; // 0x8c
    SmartPtr<GameBrowserCtrl> mHelpTab; // 0x90
    SmartPtr<GameBrowserCtrl> mNewsTab; // 0x94
    SmartPtr<Component> mLowerLayoutPoint; // 0x98
    SmartPtr<MSSeperator> mLowerDivider; // 0x9c
    SmartPtr<Component> mSierraLogo; // 0xa0
    SmartPtr<Component> mLowerBanner; // 0xa4
    SmartPtr<RectangleComponent3D> mLowerBannerFrame; // 0xa8
    SmartPtr<Component> mBackButton; // 0xac
    bool mHideLowerControls; // 0xb0
    bool mIsSwat; // 0xb1
    int mLastJoinTab; // 0xb4
    int mGameTab; // 0xb8, not initialized by the constructor
    int mDirectConnectTab; // 0xbc
    int mLanTab; // 0xc0
    int mLobbyTab; // 0xc4
    int mRoomSpecFlags; // 0xc8
};
}

// RoomCtrl: the room list (servers and their chat rooms) with Join and Create buttons.
namespace WONAPI {
class ServerChangedEvent;
class GroupChangedEvent;
// TreeItem and TreeArea are declared in WONLobbyGUI.h.
class RoomCtrl : public LobbyRustCtrl {
public:
    RoomCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::Room) {}
}; // 0x90 bytes; Container is 0x84 bytes with 51 vtable slots
}

// ServerListCtrl (R6 WONLobby round 13): the game list with filter combos and sortable columns, a lobbyctl.rs
// object (srvlistctl.rs: the controls, the columns, the rows by game, the filters, the timer); the shell forwards
// TimerEvent, the config reader (LobbyNetCtrlHost.cpp) and DirectConnectCtrl call it.
namespace WONAPI {
class ServerListCtrl : public LobbyRustCtrl {
public:
    ServerListCtrl() : LobbyRustCtrl(EeWonLobbyCtlKind::ServerList) {}
    virtual bool TimerEvent(int inDelta) { return this->CCall(EeWonLobbyCtlCall::Timer, inDelta) != 0; }
    intptr_t Cfg(EeWonLobbyCtlCfg inCall, intptr_t a = 0, intptr_t b = 0)
    { return this->CCall(EeWonLobbyCtlCall::Cfg, (intptr_t)inCall, a, b); }
    LobbyGame* GetSelGame() { return (LobbyGame*)this->Cfg(EeWonLobbyCtlCfg::SelGame); }
};
}

#endif
