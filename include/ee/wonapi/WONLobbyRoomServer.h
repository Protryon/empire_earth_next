// Room and server list controls. RoomLabelCtrl (0xa0 bytes): the label naming the current room.

#ifndef EE_WONAPI_WONLOBBYROOMSERVER_H
#define EE_WONAPI_WONLOBBYROOMSERVER_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONLobbyRustCtrl.h"
namespace WONAPI {
class RoomLabelCtrl : public Container {
public:
    RoomLabelCtrl();
    // Implicit destructor (0x7c33a0): it does not write its own vtable pointer.
    virtual void InitComponent(ComponentInit&);
    virtual void HandleComponentEvent(ComponentEvent*);
    void CalcLabel();
    void SetPrefix(const GUIString& inPrefix) { this->mPrefix=inPrefix; }
    void SetRoomSpecFlags(int inFlags) { this->mRoomSpecFlags=inFlags; }
    void SetShowUserCount(bool inShow) { if(!inShow) this->mUserCt=-1; else if(this->mUserCt<0) this->mUserCt=0; }
    void SetShowServer(bool inShow) { this->mShowServer=inShow; }
    int mRoomSpecFlags; // 0x84
    SmartPtr<Label> mLabel; // 0x88
    GUIString mPrefix; // 0x8c
    GUIString mServer; // 0x90
    GUIString mRoom; // 0x94
    int mUserCt; // 0x98
    bool mShowServer; // 0x9c
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
class RoomCtrl : public Container {
public:
    RoomCtrl();
    // Implicit destructor: it does not write its own vtable pointer.
    virtual void InitComponent(ComponentInit&);
    virtual void HandleComponentEvent(ComponentEvent*);
    void EnableControls();
    void DoJoinRoom();
    void DoCreateRoom();
    TreeItem* GetServerItem(const IPAddr&);
    TreeItem* GetServerItem(LobbyServer*);
    TreeItem* GetGroupItem(const IPAddr&,int);
    TreeItem* GetGroupItem(LobbyServer*,LobbyGroup*);
    void AddServer(LobbyServer*,bool,int);
    void AddGroup(TreeItem*,LobbyServer*,LobbyGroup*);
    void HandleSetRoomList();
    void HandleServerChanged(ServerChangedEvent*);
    void HandleGroupChanged(GroupChangedEvent*);
    void HandleItemOpened(TreeItemOpenedEvent*);
    SmartPtr<TreeArea> mRoomList; // 0x84
    SmartPtr<Button> mJoinButton; // 0x88
    SmartPtr<Button> mCreateButton; // 0x8c
}; // 0x90 bytes; Container is 0x84 bytes with 51 vtable slots
}

// One row of the room list (a server or a room); global namespace, not WONAPI (RTTI .?AVRoomListItem@@).
// 0x58 bytes; the ListItem base is 0x18 bytes with 14 vtable slots.
class RoomListItem : public WONAPI::ListItem {
public:
    RoomListItem(WONAPI::LobbyServer*,bool);
    RoomListItem(WONAPI::LobbyGroup*,WONAPI::LobbyServer*); // 0x7c1c50: the group comes first
    // Implicit destructor (0x7c1b70): it does not write its own vtable pointer.
    virtual void Paint(WONAPI::Graphics&,WONAPI::ListArea*);
    virtual void CalcDimensions(WONAPI::ListArea*);
    virtual int Compare(const WONAPI::ListItem*,WONAPI::ListArea*) const;
    void UpdateText(WONAPI::LobbyServer*,WONAPI::LobbyGroup*);
    int mColor; // 0x18
    bool mLoading; // 0x1c
    WONAPI::SmartPtr<WONAPI::Image> mImage; // 0x20
    WONAPI::SmartPtr<WONAPI::Font> mFont; // 0x24
    WONAPI::GUIString mText; // 0x28
    WONAPI::IPAddr mAddress; // 0x2c
    unsigned long mGroupId; // 0x54, the group id (zero-extended), 0xffff on a server row
};


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
