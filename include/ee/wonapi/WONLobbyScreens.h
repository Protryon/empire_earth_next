#ifndef EE_WONAPI_WONLOBBYSCREENS_H
#define EE_WONAPI_WONLOBBYSCREENS_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
class JoinedRoomEvent;
class LeftRoomEvent;
class ClientChangedEvent;
// UserListItem (0x30 bytes, constructor 0x7ca6a0): one user in the room's user list.
class UserListItem : public TextListItem {
public:
    UserListItem();
    virtual void Paint(Graphics&,ListArea*);
    virtual void CalcDimensions(ListArea*);
    virtual int Compare(const ListItem*,ListArea*) const;
    SmartPtr<LobbyClient> mClient; // 0x28
    signed char mSortKey; // 0x2c, signed (compared sign-extended)
    bool mShowSeparator; // 0x2d; true from the constructor
};
// UserListCtrl (0xa0 bytes, constructor 0x7ca8f0): the room's user list with Whisper and Ignore buttons.
class UserListCtrl : public Container {
public:
    UserListCtrl();
    virtual ~UserListCtrl(); // 0x7ca960; writes its own vtable pointer
    virtual void InitComponent(ComponentInit&); // 0x7cadf0
    virtual void HandleComponentEvent(ComponentEvent*); // 0x7cb4d0
    void UpdateItem(LobbyClient*,UserListItem*); // 0x7caa70
    void RefreshSeparators(); // 0x7cacf0
    UserListItem* FindItem(LobbyClient*); // 0x7cb0e0
    UserListItem* GetSelectedItem(); // 0x7cb130
    LobbyClient* GetSelectedClient(); // 0x7cb140
    void UpdateButtons(); // 0x7cb160
    void JoinedRoom(JoinedRoomEvent*); // 0x7cb200
    void LeftRoom(LeftRoomEvent*); // 0x7cb360
    void ClientChanged(ClientChangedEvent*); // 0x7cb3a0
    bool mUpdating; // 0x84
    int GetRoomSpecFlags() const { return this->mModeFlags; }
    int mModeFlags; // 0x88, room-spec flags; 5 from the constructor
    SmartPtr<ListArea> mUserList; // 0x8c
    SmartPtr<UserListItem> mFirstSeparator; // 0x90
    SmartPtr<UserListItem> mSecondSeparator; // 0x94
    SmartPtr<Button> mWhisperButton; // 0x98
    SmartPtr<Button> mIgnoreButton; // 0x9c
};
Image* GetLobbyClientIcon(LobbyClient*,bool); // 0x7cac50; picks a client's icon
// QuickPlayCtrl (0xb0 bytes, constructor 0x7c14e0, vtable 0x853bf0 with 51 slots): pick a skill level and
// play; six owned widgets at 0x98..0xac, destructor 0x7c1540.
class QuickPlayCtrl : public Dialog {
public:
    QuickPlayCtrl();
    virtual void InitComponent(ComponentInit&);
    virtual void HandleComponentEvent(ComponentEvent*);
    void RefreshUserName();
    SmartPtr<Label> mWelcomeLabel; // 0x98
    SmartPtr<Button> mNoviceRadio; // 0x9c
    SmartPtr<Button> mIntermediateRadio; // 0xa0
    SmartPtr<Button> mAdvancedRadio; // 0xa4
    SmartPtr<Button> mExpertRadio; // 0xa8
    SmartPtr<Button> mPlayButton; // 0xac
};
// GameBrowserCtrl (0xb8 bytes, constructor 0x7bbdc0): an HTML browser with navigation and find.
class BrowserComponent;
class GameBrowserCtrl : public Container {
public:
    GameBrowserCtrl(const GUIString& = "");
    virtual void InitComponent(ComponentInit&);
    virtual void HandleComponentEvent(ComponentEvent*);
    virtual void Paint(Graphics&);
    void EnableButtons();
    void DoFind(bool);
    void ResetBrowser();
    void GoHomeIfReset();
    void GoHome();
    void SetHomeLocation(const GUIString& theLocation) { this->mHomeLocation=theLocation; } // 0xb4
    SmartPtr<BrowserComponent> mBrowser;
    SmartPtr<Button> mBackButton,mForwardButton,mStopButton,mHomeButton,mRefreshButton;
    SmartPtr<InputBox> mFindInput;
    SmartPtr<Button> mFindNextButton,mFindPrevButton,mCaseSensitiveCheck;
    SmartPtr<Label> mStatusLabel;
    bool mIsReset;
    GUIString mHomeLocation;
};
class QueryAccountResultEvent;
void DrawLobbyPing(Graphics*,unsigned short,int,int,int); // 0x7c12b0; draws a ping indicator
// GameUserListItem (0x24 bytes): one player in the staged game; ListItem (0x18) plus three owned pointers.
class GameUserListItem : public ListItem {
public:
    GameUserListItem(LobbyClient*);
    virtual void Paint(Graphics&,ListArea*);
    virtual void CalcDimensions(ListArea*);
    virtual int Compare(const ListItem*,ListArea*) const;
    void UpdateIcon();
    SmartPtr<LobbyClient> mClient;
    SmartPtr<Image> mImage;
    SmartPtr<Font> mFont;
};
class MultiListArea;
class PlayerChangedEvent;
// GameUserListCtrl (0x94 bytes): the staged game's players; room flags at 0x8c, in-game flag at 0x90.
class GameUserListCtrl : public Container {
public:
    GameUserListCtrl();
    virtual void InitComponent(ComponentInit&);
    virtual void HandleComponentEvent(ComponentEvent*);
    LobbyClient* GetSelectedClient();
    GameUserListItem* GetListItem(LobbyClient*);
    void HandleKickPlayer(bool);
    void UpdateKickButton();
    void HandlePlayerChanged(PlayerChangedEvent*);
    void HandleClientChanged(ClientChangedEvent*);
    void HandleJoinedGame();
    void HandleLeftGame();
    SmartPtr<MultiListArea> mUserList;
    SmartPtr<Button> mKickButton;
    int GetRoomSpecFlags() const { return this->mRoomSpecFlags; }
    int mRoomSpecFlags;
    bool mInGame;
};

// UpdateAccountCtrl (0xbc bytes): Container (0x84), 13 owned controls and the old password at 0xb8.
class UpdateAccountCtrl : public Container {
public:
    UpdateAccountCtrl();
    virtual ~UpdateAccountCtrl();
    virtual void InitComponent(ComponentInit&);
    virtual void HandleComponentEvent(ComponentEvent*);
    static bool SetDesc(bool,Label*);
    void CheckInput();
    void HandleEnteredScreen();
    void HandleQueryAccountResult(QueryAccountResultEvent*);
    SmartPtr<InputBox> mUserNameInput,mOldPasswordInput,mPasswordInput,mConfirmPasswordInput,mEmailInput;
    SmartPtr<Button> mNewsletterCheck;
    SmartPtr<Label> mUserNameDesc,mOldPasswordDesc,mPasswordDesc,mConfirmPasswordDesc,mEmailDesc;
    SmartPtr<Button> mQueryButton,mUpdateButton;
    GUIString mActualOldPassword;
};
// Login screen and its panels.
class GetHTTPDocumentOp;
class LoginScreen : public Container {
public:
    LoginScreen();
    virtual void InitComponent(ComponentInit&);
    virtual void HandleComponentEvent(ComponentEvent*);
    void ShowMOTD();
    void ShowCreateAccountCtrl();
    void ShowLostPasswordCtrl();
    void ShowLostUsernameCtrl();
    SmartPtr<ScreenContainer> mRightPanel; // 0x84
    SmartPtr<Component> mMOTDCtrl; // 0x88
    SmartPtr<Component> mCreateAccountCtrl; // 0x8c
    SmartPtr<Component> mLostUserNameCtrl; // 0x90
    SmartPtr<Component> mLostPasswordCtrl; // 0x94
    SmartPtr<Component> mLoginHelpCtrl; // 0x98
};

class BrowserComponent;
class LoginHelpCtrl : public Container {
public:
    LoginHelpCtrl();
    virtual void InitComponent(ComponentInit&);
    virtual void Paint(Graphics&);
    SmartPtr<BrowserComponent> mBrowser; // 0x84
    bool mHaveLoadedHelp; // 0x88; 0x8c bytes
};

class BrowserComponent;
class MOTDCtrl : public Container {
public:
    MOTDCtrl();
    virtual ~MOTDCtrl();
    virtual void InitComponent(ComponentInit&);
    virtual void HandleComponentEvent(ComponentEvent*);
    virtual void Paint(Graphics&);
    void ShowMessage(GetHTTPDocumentOp*);
    SmartPtr<BrowserComponent> mBrowser; // 0x84
    std::string mGameMOTDPath; // 0x88
    std::string mSysMOTDPath; // 0x98
    bool mNeedGenHTML; // 0xa8; 0xac bytes
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
