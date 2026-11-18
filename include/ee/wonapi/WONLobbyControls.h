// WONLobby controls: a Container base (0x84 bytes) plus their own fields.

#ifndef EE_WONAPI_WONLOBBYCONTROLS_H
#define EE_WONAPI_WONLOBBYCONTROLS_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyRustCtrl.h"
namespace WONAPI {
class LoginInitStatusEvent;
class LoginStatusCtrl : public Container {
public:
    LoginStatusCtrl();
    // Implicit inherited virtual destructor (the original stores no own vptr).
    virtual void InitComponent(ComponentInit& inInit);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    SmartPtr<Label> mStatusLabel; // +84, allocation88
};
class InitFailedCtrl : public Container {
public:
    InitFailedCtrl();
    // Implicit inherited virtual destructor (the original stores no own vptr).
    virtual void InitComponent(ComponentInit& inInit);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    void HandleLoginInitStatus(LoginInitStatusEvent* inEvent);
    SmartPtr<Label> mDescLabel; // +84
    SmartPtr<Button> mRetryButton; // +88
    SmartPtr<Button> mIgnoreButton; // +8c; allocation90
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
class LoginCtrl : public Dialog {
public:
    LoginCtrl();
    // Implicit inherited virtual destructor (the original stores no own vptr).
    virtual void InitComponent(ComponentInit& inInit);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    void UpdateLoginButton();
    void FillUserNames();
    void LoadPassword();
    void SavePassword();
    void SetCredentials(SetLoginFieldsEvent* inEvent);
    void HandleLoginInitStatus(LoginInitStatusEvent* inEvent);
    SmartPtr<ComboBox> mUserNameCombo; //98
    SmartPtr<InputBox> mPasswordInput; //9c
    SmartPtr<Button> mRememberPasswordCheck; //a0
    SmartPtr<Button> mLoginButton; //a4
    bool mCanLogin; // offset 0xa8; class size 0xac with alignment
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
class FriendsListItem : public TextListItem {
public:
    FriendsListItem();
    // Implicit inherited virtual destructor (the original stores no own vptr).
    virtual void Paint(Graphics& inGraphics,ListArea* inList);
    virtual void CalcDimensions(ListArea* inList);
    virtual int Compare(const ListItem* inOther,ListArea* inList) const;
    SmartPtr<LobbyFriend> mFriend; //28
    bool mSeparator; //2c
    signed char mSortGroup; //2d;allocation30
};
class FriendsListCtrl : public Container {
public:
    static ComponentConfig* CfgFactory(); // at 0x00765320
    static Component* CompFactory(); // at 0x007652c0
    FriendsListCtrl();
    // Implicit inherited virtual destructor (the original stores no own vptr).
    virtual void InitComponent(ComponentInit& inInit);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    void UpdateSeparator();
    void UpdateFriend(LobbyFriend* inFriend,FriendsListItem* inItem);
    FriendsListItem* FindFriend(LobbyFriend* inFriend);
    LobbyFriend* GetSelectedFriend();
    void HandleFriendChanged(FriendChangedEvent* inEvent);
    void RefreshFriends();
    void FillFriends();
    void UpdateButtons();
    SmartPtr<ListArea> mFriendsList; //84
    SmartPtr<Button> mRemoveButton; //88
    SmartPtr<Button> mLocateButton; //8c
    SmartPtr<FriendsListItem> mSeparator; //90;allocation94
};
}


// LostPasswordCtrl and LostUserNameCtrl.
namespace WONAPI {
class LostPasswordCtrl : public Container {
public:
    LostPasswordCtrl(); // empty, at 0x007c0320
    // Implicit inherited virtual destructor (the original stores no own vptr).
    virtual void InitComponent(ComponentInit& inInit); //3c0400
    virtual void HandleComponentEvent(ComponentEvent* inEvent); //3c0520
    void UpdateRetrieveButton(); //3c0460
    void FillUserNames(); //3c0490
    SmartPtr<ComboBox> mUserNameCombo; //84
    SmartPtr<Button> mRetrieveButton; //88
};
class LostUserNameCtrl : public Container {
public:
    LostUserNameCtrl(); // empty, at 0x007c0750
    // Implicit inherited virtual destructor (the original stores no own vptr).
    virtual void InitComponent(ComponentInit& inInit); //3c0830
    virtual void HandleComponentEvent(ComponentEvent* inEvent); //3c0930
    void UpdateRetrieveButton(); //3c0890
    SmartPtr<InputBox> mEmailInput; //84
    SmartPtr<Button> mRetrieveButton; //88
};
}


// CreateAccountCtrl (0xbc bytes): RTTI CreateAccountCtrl > Container > Component > RefCount,
// vtable 0x00852e3c.
namespace WONAPI {
class CreateAccountCtrl : public Container {
public:
    CreateAccountCtrl(); // at 0x007ba290
    // Implicit virtual destructor (0x007ba320; stores no own vptr).
    virtual void InitComponent(ComponentInit& inInit); // at 0x007ba4f0
    virtual void HandleComponentEvent(ComponentEvent* inEvent); // at 0x007ba990
    bool ShowErrorLabel(bool inVisible, Label* inLabel); // does not use this
    bool IsValidBirthDate(InputBox* inMonth,InputBox* inDay,InputBox* inYear); // does not use this
    void UpdateCreateButton(); // at 0x007ba800
    SmartPtr<InputBox> mUserNameInput; //84
    SmartPtr<InputBox> mPasswordInput; //88
    SmartPtr<InputBox> mConfirmPasswordInput; //8c
    SmartPtr<InputBox> mEmailInput; //90
    SmartPtr<InputBox> mMonthInput; //94
    SmartPtr<InputBox> mDayInput; //98
    SmartPtr<InputBox> mYearInput; //9c
    SmartPtr<Button> mNewsletterCheck; //a0
    SmartPtr<Label> mUserNameDesc; //a4
    SmartPtr<Label> mPasswordDesc; //a8
    SmartPtr<Label> mConfirmPasswordDesc; //ac
    SmartPtr<Label> mEmailDesc; //b0
    SmartPtr<Label> mBirthdateDesc; //b4
    SmartPtr<Button> mCreateButton; //b8
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
