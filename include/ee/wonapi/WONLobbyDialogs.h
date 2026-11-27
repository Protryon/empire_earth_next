// WONLobby dialogs and popup menus.
#ifndef EE_WONAPI_WONLOBBYDIALOGS_H
#define EE_WONAPI_WONLOBBYDIALOGS_H
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/lobbypopup.h"
#include "rust/won_lobby/scrolltext.h"
#include "rust/won_net/routeop.h"
#include "rust/won_gui/cfgkind.h"
namespace WONAPI {
class LobbyGame;
class LobbyClient;
// The lobby's dialogs (R6 WONLobby rounds 1-3): LanName, Password, AddFriend, DirectConnect, LanPort, CreateRoom,
// CreateGame, AdminAction, Status, OkCancel, Room, Tou, BadUser, ChooseAdapter and Credits are one Rust object each
// (ee_won lobbydlg.rs: the controls held, the scalars, every method); this shell forwards the virtuals (the terms of use's
// Paint skipped Dialog's, which has none here: Container's either way). Since WON
// round 22 the shell is the only class: the subclasses (LanNameDialog .. CreditsDialog, one-line forwards) are gone,
// their callers make it with its kind and call LCall; SafeGetDialog is SafeGetComponent's type check by kind. The
// host is LobbyDialogHost.cpp.
class LobbyRustDialog : public Dialog {
public:
    explicit LobbyRustDialog(EeWonLobbyDlgKind inKind);
    virtual ~LobbyRustDialog();
    virtual void InitComponent(ComponentInit& inInit);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    EeWonLobbyDlg* mLobbyDlg;
    EeWonLobbyDlgKind mKind;
    intptr_t LCall(EeWonLobbyDlgCall c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0) {
        return ee_won_lobbydlg_call(this->mLobbyDlg, c, a, b, cc); }
    const GUIString& InputText(int inMember); // the member input box's GetText()
    SmartPtr<LobbyGame> GetGame(); // the create game dialog's new game
};
template <EeWonLobbyDlgKind K> Component* NewLobbyDialog() { return new LobbyRustDialog(K); }
// SafeGetComponent of a lobby dialog: the named component must be one of kind k
inline void SafeGetDialog(ComponentConfig* inConfig, SmartPtr<LobbyRustDialog>& outPtr, EeWonLobbyDlgKind k,
                          const char* inName, const char* inFile, int inLine)
{
    LobbyRustDialog* d = dynamic_cast<LobbyRustDialog*>(ComponentConfigGet(inConfig, inName));
    if (d == NULL || d->mKind != k) { ComponentConfigMissing(inName, inFile, inLine); d = NULL; }
    outPtr = d;
}
// Credits (R6 WONLobby round 3): ScrollingTextComponent's lines (each a GUIString copy and its colour), scroll clock,
// paint and fades are ee_won scrolltext.rs (its host is in CreditsDialog.cpp); CreditsDialog is the lobby dialog kind
// Credits (its two buttons, the scrolling text it holds and places, AddLine's widening).
class ScrollingTextComponent : public Component {
public:
    ScrollingTextComponent();
    virtual ~ScrollingTextComponent();
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
class LobbyGame;

class PopupMenu;
class LobbyClient;
class LobbyFriend;
// The options popups (R6 WONLobby round 3): ClientOptionsPopup, GameOptionsPopup and FriendOptionsPopup are one Rust
// object each (ee_won lobbypopup.rs: the menu, the subject and the room flags held, Init's rules, the click's action);
// this shell forwards the event and the classes below are its Init forwarders. The host is in LobbyDialogHost.cpp.
class LobbyRustPopup : public Container {
public:
    explicit LobbyRustPopup(EeWonLobbyPopupKind inKind);
    virtual ~LobbyRustPopup();
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    EeWonLobbyPopup* mLobbyPopup;
    intptr_t PCall(EeWonLobbyPopupCall c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0) {
        return ee_won_lobbypopup_call(this->mLobbyPopup, c, a, b, cc); }
};
// the options popups (ClientOptionsPopup, GameOptionsPopup, FriendOptionsPopup until WON round 22: Init forwards):
// a new popup of kind k, its Init(a, b, c) and LobbyOpenPopup as a standard popup
void LobbyOpenRustPopup(EeWonLobbyPopupKind k, intptr_t a, intptr_t b = 0, intptr_t c = 0);

// CreditsDialogConfig: reads the credits dialog's resource configuration; 14 vtable slots (partial declaration).
class CreditsDialogConfig : public ContainerConfig {
public:
    CreditsDialogConfig() : ContainerConfig(EeWonCfgKind::Dialog) {}
    virtual bool Virtual24(const std::string& inInstruction); // 0x7f7780; handles the ADDLINE instruction
};

}
#endif
