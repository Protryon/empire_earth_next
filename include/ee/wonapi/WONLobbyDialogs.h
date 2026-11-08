// WONLobby dialogs and popup menus.
// LanNameDialog, PasswordDialog and AddFriendDialog are 0xa4 bytes: Dialog (0x98) plus three owned widgets.
#ifndef EE_WONAPI_WONLOBBYDIALOGS_H
#define EE_WONAPI_WONLOBBYDIALOGS_H
namespace WONAPI {
class LanNameDialog : public Dialog {
public:
    LanNameDialog();
    virtual ~LanNameDialog(); // 0x7f8400; explicit: writes its own vtable pointer
    virtual void InitComponent(ComponentInit& inInit);
    void Reset();
    const GUIString& GetName(); // 0x7f90c0, the same code as PasswordDialog::GetPassword
    SmartPtr<InputBox> mNameInput; // 0x98
    SmartPtr<Button> mOkButton; // 0x9c
    SmartPtr<Button> mCancelButton; // 0xa0
};
class PasswordDialog : public Dialog {
public:
    PasswordDialog();
    // Implicit destructor (0x7f8f70): it does not write its own vtable pointer.
    virtual void InitComponent(ComponentInit& inInit);
    void Reset(); // 0x7f8560, the same code as LanNameDialog::Reset
    const GUIString& GetPassword();
    SmartPtr<InputBox> mPasswordInput; // 0x98
    SmartPtr<Button> mOkButton; // 0x9c
    SmartPtr<Button> mCancelButton; // 0xa0
};
class AddFriendDialog : public Dialog {
public:
    AddFriendDialog();
    // Implicit destructor (0x7f48a0): it does not write its own vtable pointer.
    virtual void InitComponent(ComponentInit& inInit);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    void CheckInput();
    void Reset();
    const GUIString& GetName();
    SmartPtr<InputBox> mNameInput; // 0x98; CheckInput reads its text directly
    SmartPtr<Button> mOkButton; // 0x9c
    SmartPtr<Button> mCancelButton; // 0xa0
};
// Credits: CreditLineInfo is one line of text with its colour; ScrollingTextComponent (0x54 bytes) scrolls them.
class CreditLineInfo {
protected:
    GUIString mLine; // 0x00
    DWORD mColor; // 0x04
public:
    CreditLineInfo(const GUIString& text,DWORD color) {this->mLine=text;this->mColor=color;}
    GUIString GetLine(); // 0x7f7450; returns a copy
    DWORD GetColor() {return this->mColor;}
};
typedef std::list<CreditLineInfo> CreditsList;
class ScrollingTextComponent : public Component {
protected:
    SmartPtr<Font> mFont; // 0x30
    CreditsList mLines; // 0x34 allocator, 0x38 head, 0x3c count
    DWORD mLastColor; // 0x40
    int mTimeSinceLastScroll; // 0x44
    int mScrollSpeed; // 0x48
    int mScrollTop; // 0x4c
    int mTotalTextHeight; // 0x50
public:
    ScrollingTextComponent(); // 0x7f6de0
    virtual ~ScrollingTextComponent(); // 0x7f6e90
    virtual void Paint(Graphics&); // slot 0x18: 0x7f71b0
    virtual bool TimerEvent(int); // slot 0x44: 0x7f70b0
    void Start(); // 0x7f6f50; starts the scroll timer
    void Stop(); // 0x7f6f70; stops the scroll timer
    void Reset(); // 0x7f6f80; sets the scroll position to the component height
    void AddLine(const GUIString&,unsigned long); // 0x7f6f90
};
// CreditsDialog: 0xa4 bytes; the destructor is implicit.
class CreditsDialog : public Dialog {
protected:
    void UpdateCredits(); // empty: InitComponent calls the shared empty function at 0x8142a0
public:
    CreditsDialog();
    virtual void InitComponent(ComponentInit&);
    virtual void HandleComponentEvent(ComponentEvent*); // 0x7fa1f0, shared with ChooseNetworkAdapterDialog
    void StartScrolling();
    void StopScrolling();
    void ResetScrolling();
    void AddLine(const GUIString&,Component*,unsigned long);
    SmartPtr<ScrollingTextComponent> mScrollingText; // 0x98
    SmartPtr<Button> mCreditsButton; // 0x9c
    SmartPtr<Button> mCloseButton; // 0xa0
};
class GotHTTPDocumentEvent;
class TOUDialog : public Dialog {
public:
    TOUDialog(); // object size 0xac
    // The destructor is implicit.
    virtual void Paint(Graphics&); // slot 0x18
    virtual void InitComponent(ComponentInit&);
    virtual void HandleComponentEvent(ComponentEvent*);
    void HandleHTTPDocEvent(GotHTTPDocumentEvent*);
    void Reset();
    SmartPtr<BrowserComponent> mBrowser; // 0x98
    SmartPtr<Button> mAgreeCheck; // 0x9c
    SmartPtr<Button> mAcceptButton; // 0xa0
    SmartPtr<Button> mRejectButton; // 0xa4
    bool mNeedGenHTML; // 0xa8, padded to 0xac
};
class ChooseNetworkAdapterDialog : public Dialog {
public:
    ChooseNetworkAdapterDialog(); // object size 0xa0
    virtual ~ChooseNetworkAdapterDialog(); // 0x7f9c80; explicit
    virtual void InitComponent(ComponentInit&);
    virtual void HandleComponentEvent(ComponentEvent*); // 0x7fa1f0, shared with CreditsDialog
    void Reset();
    unsigned long GetSelectedAddress(); // raw IPv4 address
    SmartPtr<MultiListArea> mAdapterList; // 0x98
    SmartPtr<Button> mActivateButton; // 0x9c
};
class LobbyGame;
// CreateGameDialog (0xc0 bytes): Dialog (0x98) plus nine owned widgets and the game type.
class CreateGameDialog : public Dialog {
protected:
    virtual ~CreateGameDialog(); // 0x7f6450
public:
    CreateGameDialog();
    virtual void InitComponent(ComponentInit& inInit);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    void Reset(bool inClear);
    void SetGameType(int inType);
    SmartPtr<LobbyGame> GetGame();
    void CheckInput();
    void UpdatePasswordVisibility();
    SmartPtr<InputBox> mNameInput;
    SmartPtr<ComboBox> mSkillCombo;
    SmartPtr<InputBox> mPasswordInput;
    SmartPtr<Label> mPasswordLabel;
    SmartPtr<Button> mOpenToAllRadio;
    SmartPtr<Button> mPasswordRadio;
    SmartPtr<Button> mInviteOnlyRadio;
    SmartPtr<Button> mAskToJoinRadio;
    SmartPtr<Button> mCreateButton;
    int mGameType;
};

class PopupMenu;
class LobbyClient;
class ClientOptionsPopup : public Container {
protected:
    SmartPtr<PopupMenu> mPopupMenu; // 0x84
    SmartPtr<LobbyClient> mClient; // 0x88
    int mRoomSpecFlags; // 0x8c, not initialized by the constructor
    int GetRoomSpecFlags() const { return this->mRoomSpecFlags; }
    void AddItem(const GUIString&,int); // 0x7f5ea0
    virtual void HandleComponentEvent(ComponentEvent* inEvent); // 0x7f6310, slot 0x7c
    virtual ~ClientOptionsPopup(); // 0x7f5e00
public:
    ClientOptionsPopup(); // 0x7f5d10; object size 0x90
    void Init(LobbyClient* inClient, LobbyClient* inMyClient, int inRoomSpecFlags); // 0x7f5ec0
};

// GameOptionsPopup: 0x8c bytes.
class PopupMenu;
class GameOptionsPopup : public Container {
protected:
    SmartPtr<LobbyGame> mGame; // 0x84, owned
    SmartPtr<PopupMenu> mPopupMenu; // 0x88, owned
    void AddItem(const GUIString&,int); // 0x7f80f0
    virtual void HandleComponentEvent(ComponentEvent* inEvent); // 0x7f82f0, slot 0x7c
    virtual ~GameOptionsPopup(); // 0x7f8050
public:
    GameOptionsPopup(); // 0x7f7f60; object size 0x8c
    void Init(LobbyGame* inGame); // 0x7f8110
};


class LanPortDialog : public Dialog {
public:
    LanPortDialog();
    virtual ~LanPortDialog();
    virtual void InitComponent(ComponentInit& inInit);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    void Reset();
    void SetPort(unsigned short inPort);
    void SetGamePort(unsigned short inPort);
    unsigned short GetPort();
    void CheckInput();
    SmartPtr<InputBox> mPortInput;
    SmartPtr<Label> mGamePortLabel;
    SmartPtr<Button> mOkButton;
    unsigned short mGamePort;
};

class OkCancelDialog : public Dialog {
public:
    OkCancelDialog();
    virtual void InitComponent(ComponentInit& inInit);
    void SetText(const GUIString& inTitle, const GUIString& inBody,
                 const GUIString& inOkText, const GUIString& inCancelText);
    SmartPtr<Label> mTitleLabel;
    SmartPtr<Label> mBodyLabel;
    SmartPtr<Button> mOkButton;
    SmartPtr<Button> mCancelButton;
    SmartPtr<Button> mCloseButton;
    SmartPtr<Component> mDialogComponent;
    SmartPtr<WONFrameComponent> mBack;
};

class StatusDialog : public Dialog {
public:
    StatusDialog();
    virtual void InitComponent(ComponentInit& inInit);
    void SetStatus(const GUIString& inStatus, bool inCancel);
    void SetText(const GUIString& inTitle, const GUIString& inStatus, bool inCancel);
    SmartPtr<Label> mTitleLabel;
    SmartPtr<Button> mCloseButton;
    SmartPtr<Label> mStatusLabel;
    SmartPtr<Button> mCancelButton;
    SmartPtr<Component> mDialogComponent;
    SmartPtr<WONFrameComponent> mBack;
    unsigned long mStartTick;
};

class DirectConnectDialog : public Dialog {
public:
    DirectConnectDialog();
    virtual void InitComponent(ComponentInit& inInit);
    void Reset();
    const GUIString& GetAddress();
    const GUIString& GetName(); // 0x7f90c0, the same code as PasswordDialog::GetPassword
    SmartPtr<InputBox> mNameInput;
    SmartPtr<InputBox> mAddressInput;
    SmartPtr<Button> mAddButton;
    SmartPtr<Button> mCancelButton;
};

class CreateRoomDialog : public Dialog {
public:
    CreateRoomDialog();
    virtual void InitComponent(ComponentInit& inInit);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    void Reset(bool inClear);
    void UpdatePasswordVisibility();
    void CheckInput();
    const GUIString& GetPassword();
    const GUIString& GetRoomName(); // 0x7f90c0, the same code as PasswordDialog::GetPassword
    SmartPtr<InputBox> mNameInput;
    SmartPtr<InputBox> mPasswordInput;
    SmartPtr<Label> mPasswordLabel;
    SmartPtr<Button> mPasswordCheck;
    SmartPtr<Button> mCreateButton;
};

// AdminActionDialog (0xc8 bytes): warn, mute, kick or ban a user in the room or on the server.
class AdminActionDialog : public Dialog {
public:
    AdminActionDialog();
    virtual ~AdminActionDialog();
    virtual void InitComponent(ComponentInit& inInit);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    void UpdateDoActionButton();
    void Reset(int inAction, LobbyClient* inTarget);
    bool GetApplyToServer();
    const GUIString& GetExplanation();
    unsigned long GetNumSeconds();
    int mActionType; // 0x98
    SmartPtr<Label> mTitleLabel; // 0x9c
    SmartPtr<Label> mExplanationHeader; // 0xa0
    SmartPtr<InputBox> mExplanation; // 0xa4
    bool mIsReasonOptional; // 0xa8, padded to 0xac
    SmartPtr<Label> mDurationHeader; // 0xac
    SmartPtr<ComboBox> mDurationCombo; // 0xb0
    SmartPtr<ComboBox> mTimeSegmentCombo; // 0xb4
    SmartPtr<Button> mDoActionButton; // 0xb8
    SmartPtr<Button> mCancelButton; // 0xbc
    SmartPtr<Button> mApplyToRoom; // 0xc0
    SmartPtr<Button> mApplyToServer; // 0xc4
};

class RoomCtrl; // 0x90 bytes, declared in WONLobbyRoomServer.h
// RoomDialog: 0x9c bytes.
class RoomDialog : public Dialog {
protected:
    SmartPtr<RoomCtrl> mRoomCtrl; // 0x98
    virtual ~RoomDialog();
public:
    RoomDialog(); // 0x7f90d0
    virtual void InitComponent(ComponentInit&); // slot 0x1c: the shared empty one-argument function at 0x6fa500
};

struct RoutingBadUser; // defined in WONRouting.h
// BadUserDialog (0xac bytes): the muted/banned user list; 51 vtable slots, its own entries at 0x00 and 0x1c.
// The destructor is implicit (it does not write its own vtable pointer).
class BadUserDialog : public Dialog {
public:
    typedef std::list<unsigned long> IdList;
    BadUserDialog();
    virtual void InitComponent(ComponentInit& inInit); // 0x7f5a90
    void SetUserList(const std::list<RoutingBadUser>& inList, bool inSimple); // 0x7f5b20
    void GetIds(IdList& outIds); // 0x7f5c20
    SmartPtr<MultiListArea> mUserList; // 0x98
    SmartPtr<ListArea> mSimpleUserList; // 0x9c
    SmartPtr<Component> mUserListBox; // 0xa0
    SmartPtr<Component> mSimpleUserListBox; // 0xa4
    bool mIsSimple; // 0xa8, padded to 0xac
};

// CreditsDialogConfig: reads the credits dialog's resource configuration; 14 vtable slots (partial declaration).
class CreditsDialogConfig : public DialogConfig {
public:
    static ComponentConfig* CfgFactory() { return new CreditsDialogConfig; } // inline as in the SDK (0x765260)
    virtual bool Virtual24(const std::string& inInstruction); // 0x7f7780; handles the ADDLINE instruction
    virtual void SetComponent(Component* inComponent); // 0x78f100
    CreditsDialog* mCredits; // 0x9c, not owned
};

class LobbyFriend;
// FriendOptionsPopup (0x90 bytes): the popup menu on a friend.
// Vtable 0x856268, 51 slots; it differs from Container's only at 0x00 and 0x7c.
class FriendOptionsPopup : public Container {
protected:
    SmartPtr<PopupMenu> mPopupMenu; // 0x84, owned
    SmartPtr<LobbyFriend> mFriend; // 0x88, owned
    int mRoomSpecFlags; // 0x8c, not initialized by the constructor
    void AddItem(const GUIString&,int); // 0x7f5ea0, shared with ClientOptionsPopup::AddItem
    virtual void HandleComponentEvent(ComponentEvent*); // 0x7f7ea0
    virtual ~FriendOptionsPopup(); // 0x7f7c50
public:
    FriendOptionsPopup(); // 0x7f7b60
    void Init(LobbyFriend*,int); // 0x7f7cf0
};

}
#endif
