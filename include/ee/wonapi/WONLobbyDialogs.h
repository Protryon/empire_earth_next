// WONLobby dialogs and popup menus.
#ifndef EE_WONAPI_WONLOBBYDIALOGS_H
#define EE_WONAPI_WONLOBBYDIALOGS_H
#include "rust/ee_won.h"
namespace WONAPI {
class LobbyGame;
class LobbyClient;
// The lobby's small dialogs (R6 WONLobby round 1): LanNameDialog, PasswordDialog, AddFriendDialog,
// DirectConnectDialog, LanPortDialog, CreateRoomDialog, CreateGameDialog and AdminActionDialog are one Rust object
// each (ee_won lobbydlg.rs: the controls held, the scalars, every method); this shell forwards the two virtuals and
// the classes below are its one-line forwarders. The host is LobbyDialogHost.cpp.
class LobbyRustDialog : public Dialog {
public:
    explicit LobbyRustDialog(EeWonLobbyDlgKind inKind);
    virtual ~LobbyRustDialog();
    virtual void InitComponent(ComponentInit& inInit);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    EeWonLobbyDlg* mLobbyDlg;
protected:
    intptr_t LCall(EeWonLobbyDlgCall c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0) {
        return ee_won_lobbydlg_call(this->mLobbyDlg, c, a, b, cc); }
    const GUIString& InputText(int inMember); // the member input box's GetText()
    const GUIString& InputTextOf(intptr_t inInput); // an input box's GetText(), gEmptyGUIString for NULL
};
class LanNameDialog : public LobbyRustDialog {
public:
    LanNameDialog() : LobbyRustDialog(EeWonLobbyDlgKind::LanName) {}
    void Reset() { this->LCall(EeWonLobbyDlgCall::Reset); }
    const GUIString& GetName() { return this->InputText(0); }
};
class PasswordDialog : public LobbyRustDialog {
public:
    PasswordDialog() : LobbyRustDialog(EeWonLobbyDlgKind::Password) {}
    void Reset() { this->LCall(EeWonLobbyDlgCall::Reset); }
    const GUIString& GetPassword() { return this->InputText(0); }
};
class AddFriendDialog : public LobbyRustDialog {
public:
    AddFriendDialog() : LobbyRustDialog(EeWonLobbyDlgKind::AddFriend) {}
    void Reset() { this->LCall(EeWonLobbyDlgCall::Reset); }
    const GUIString& GetName() { return this->InputText(0); }
};
class DirectConnectDialog : public LobbyRustDialog {
public:
    DirectConnectDialog() : LobbyRustDialog(EeWonLobbyDlgKind::DirectConnect) {}
    void Reset() { this->LCall(EeWonLobbyDlgCall::Reset); }
    const GUIString& GetName() { return this->InputText(0); }
    const GUIString& GetAddress() { return this->InputText(1); }
};
class LanPortDialog : public LobbyRustDialog {
public:
    LanPortDialog() : LobbyRustDialog(EeWonLobbyDlgKind::LanPort) {}
    void Reset() { this->LCall(EeWonLobbyDlgCall::Reset); }
    void SetPort(unsigned short inPort) { this->LCall(EeWonLobbyDlgCall::SetPort, inPort); }
    void SetGamePort(unsigned short inPort) { this->LCall(EeWonLobbyDlgCall::SetGamePort, inPort); }
    unsigned short GetPort() { return (unsigned short)this->LCall(EeWonLobbyDlgCall::GetPort); }
};
class CreateRoomDialog : public LobbyRustDialog {
public:
    CreateRoomDialog() : LobbyRustDialog(EeWonLobbyDlgKind::CreateRoom) {}
    void Reset(bool inClear) { this->LCall(EeWonLobbyDlgCall::Reset, inClear); }
    const GUIString& GetRoomName() { return this->InputText(0); }
    const GUIString& GetPassword() { return this->InputTextOf(this->LCall(EeWonLobbyDlgCall::GetPassword)); }
};
class CreateGameDialog : public LobbyRustDialog {
public:
    CreateGameDialog() : LobbyRustDialog(EeWonLobbyDlgKind::CreateGame) {}
    void Reset(bool inClear) { this->LCall(EeWonLobbyDlgCall::Reset, inClear); }
    void SetGameType(int inType) { this->LCall(EeWonLobbyDlgCall::SetGameType, inType); }
    SmartPtr<LobbyGame> GetGame();
};
// AdminActionDialog: warn, mute, kick or ban a user in the room or on the server.
class AdminActionDialog : public LobbyRustDialog {
public:
    AdminActionDialog() : LobbyRustDialog(EeWonLobbyDlgKind::AdminAction) {}
    void Reset(int inAction, LobbyClient* inTarget) { this->LCall(EeWonLobbyDlgCall::Reset, inAction, (intptr_t)inTarget); }
    bool GetApplyToServer() { return this->LCall(EeWonLobbyDlgCall::ApplyToServer) != 0; }
    const GUIString& GetExplanation() { return this->InputText(2); }
    unsigned long GetNumSeconds() { return (unsigned long)this->LCall(EeWonLobbyDlgCall::NumSeconds); }
};
// Credits (R6 WONLobby round 3): ScrollingTextComponent's lines (each a GUIString copy and its colour), scroll clock,
// paint and fades are ee_won scrolltext.rs (its host is in CreditsDialog.cpp); CreditsDialog is the lobby dialog kind
// Credits (its two buttons, the scrolling text it holds and places, AddLine's widening).
class ScrollingTextComponent : public Component {
public:
    ScrollingTextComponent(); // 0x7f6de0
    virtual ~ScrollingTextComponent(); // 0x7f6e90
    virtual void Paint(Graphics& inGraphics) { this->SCall(EeWonScrollTextCall::Paint, (intptr_t)&inGraphics); }
    virtual bool TimerEvent(int inDelta) { return this->SCall(EeWonScrollTextCall::Timer, inDelta) != 0; }
    void Start() { this->SCall(EeWonScrollTextCall::Start); }
    void Stop() { this->SCall(EeWonScrollTextCall::Stop); }
    void Reset() { this->SCall(EeWonScrollTextCall::Reset); }
    void AddLine(const GUIString& inText, unsigned long inColor)
    {
        this->SCall(EeWonScrollTextCall::AddLine, (intptr_t)&inText, (intptr_t)(uint32_t)inColor);
    }
    EeWonScrollText* mScroll;
private:
    intptr_t SCall(EeWonScrollTextCall c, intptr_t a = 0, intptr_t b = 0) { return ee_won_scrolltext_call(this->mScroll, c, a, b); }
};
class CreditsDialog : public LobbyRustDialog {
public:
    CreditsDialog() : LobbyRustDialog(EeWonLobbyDlgKind::Credits)
    {
        this->LCall(EeWonLobbyDlgCall::HoldScrollText, (intptr_t) new ScrollingTextComponent);
    }
    void StartScrolling() { this->LCall(EeWonLobbyDlgCall::StartScrolling); }
    void StopScrolling() { this->LCall(EeWonLobbyDlgCall::StopScrolling); }
    void ResetScrolling() { this->LCall(EeWonLobbyDlgCall::ResetScrolling); }
    void AddLine(const GUIString& inText, Component* inComponent, unsigned long inColor)
    {
        this->LCall(EeWonLobbyDlgCall::AddLine, (intptr_t)&inText, (intptr_t)inComponent, (intptr_t)(uint32_t)inColor);
    }
};
// R6 WONLobby round 3: TOUDialog, ChooseNetworkAdapterDialog, RoomDialog and BadUserDialog are lobby dialog kinds
// (their controls, flags and methods in ee_won lobbydlg.rs; the list rows BadUserItem and NetworkAdapterListItem stay
// C++ in LobbyDialogHost.cpp).
// TOUDialog: the terms of use (a GotHTTPDocumentEvent for the terms marks the page for rebuilding).
class TOUDialog : public LobbyRustDialog {
public:
    TOUDialog() : LobbyRustDialog(EeWonLobbyDlgKind::Tou) {}
    virtual void Paint(Graphics& inGraphics) { Container::Paint(inGraphics); } // the original skipped Dialog's
    void Reset() { this->LCall(EeWonLobbyDlgCall::Reset); }
};
// ChooseNetworkAdapterDialog: InitLogic's choice of the lobby's local address.
class ChooseNetworkAdapterDialog : public LobbyRustDialog {
public:
    ChooseNetworkAdapterDialog() : LobbyRustDialog(EeWonLobbyDlgKind::ChooseAdapter) {}
    void Reset() { this->LCall(EeWonLobbyDlgCall::Reset); }
    unsigned long GetSelectedAddress() { return (unsigned long)(uint32_t)this->LCall(EeWonLobbyDlgCall::SelectedAddress); }
};
class LobbyGame;

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



// DialogLogic's two boxes (R6 WONLobby round 2): their controls and SetText (the sizing to the message) are the
// lobby dialog object's kinds OkCancel and Status.
class OkCancelDialog : public LobbyRustDialog {
public:
    OkCancelDialog() : LobbyRustDialog(EeWonLobbyDlgKind::OkCancel) {}
    void SetText(const GUIString& inTitle, const GUIString& inBody, const GUIString& inOkText,
                 const GUIString& inCancelText)
    {
        const GUIString* aButtons[2] = {&inOkText, &inCancelText};
        this->LCall(EeWonLobbyDlgCall::SetText, (intptr_t)&inTitle, (intptr_t)&inBody, (intptr_t)aButtons);
    }
};

class StatusDialog : public LobbyRustDialog {
public:
    StatusDialog(); // its start tick is the constructor's (LobbyDialogHost.cpp)
    void SetStatus(const GUIString& inStatus, bool inCancel)
    {
        this->LCall(EeWonLobbyDlgCall::SetStatus, (intptr_t)&inStatus, inCancel);
    }
    void SetText(const GUIString& inTitle, const GUIString& inStatus, bool inCancel)
    {
        this->LCall(EeWonLobbyDlgCall::SetText, (intptr_t)&inTitle, (intptr_t)&inStatus, inCancel);
    }
    void SetStartTick(unsigned long inTick) { this->LCall(EeWonLobbyDlgCall::SetStartTick, (intptr_t)inTick); }
};




// RoomDialog: RoutingLogic's room chooser, shown and ended by its holder (its RoomCtrl member was never set).
class RoomDialog : public LobbyRustDialog {
public:
    RoomDialog() : LobbyRustDialog(EeWonLobbyDlgKind::Room) {}
};

struct RoutingBadUser; // defined in WONRouting.h
// BadUserDialog: the muted/banned user list (the full list for moderators, the simple one for others).
class BadUserDialog : public LobbyRustDialog {
public:
    typedef std::list<unsigned long> IdList;
    BadUserDialog() : LobbyRustDialog(EeWonLobbyDlgKind::BadUser) {}
    void SetUserList(const std::list<RoutingBadUser>& inList, bool inSimple)
    {
        this->LCall(EeWonLobbyDlgCall::SetUserList, (intptr_t)&inList, inSimple);
    }
    void GetIds(IdList& outIds) { this->LCall(EeWonLobbyDlgCall::GetIds, (intptr_t)&outIds); }
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
