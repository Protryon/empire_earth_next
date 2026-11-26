// Lobby logic modules: LobbyLogic (their base), LobbyLogicMgr and RustLogic (the shell of the logics that are Rust
// objects; NetLogic, LAN or internet play, is its name for the two network kinds)
#ifndef EE_WONAPI_WONLOBBYLOGIC_H
#define EE_WONAPI_WONLOBBYLOGIC_H
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyGuard.h"
#include "rust/won_lobby/logicmgr.h"
#include "rust/won_lobby/logicshell.h"
#include "rust/won_lobby/logics.h"
#include "rust/won_lobby/routelogic.h"
#include "rust/won_lobby/stagingobj.h"
#include "rust/won_lobby/lanobj.h"
#include "rust/won_lobby/dlglogic.h"
namespace WONAPI {
class ComponentConfig;
class ComponentEvent;
class LobbyGame;
class LobbyClient;
class ShellCompletion;
// RTTI: LobbyLogic > RefCount; vtable 0x855a04 with six slots; derived members begin at 0x40.
// AsyncOpTracker is 0x34 bytes; constructor 0x7e5930, destructor 0x7e5a40.
class LobbyLogic : public RefCount {
public:
    LobbyLogic();
    virtual ~LobbyLogic();
    virtual void KillHook(); // slot 0x04: empty, the shared no-argument stub at 0x8142a0
    virtual void InitComponent(ComponentConfig* inConfig); // slot 0x08: empty, the shared one-argument stub at 0x6fa500
    virtual void Reset(); // slot 0x0c: empty, the shared no-argument stub at 0x8142a0
    virtual void HandleControlEvent(int inControlId); // slot 0x10: empty, the shared one-argument stub at 0x6fa500
    virtual void HandleComponentEvent(ComponentEvent* inEvent); // slot 0x14: empty, the shared one-argument stub at 0x6fa500
    void Kill();
    void RunTrackedOp(AsyncOp* inOp, ShellCompletion* inCompletion, unsigned long inTimeout);
    void RunOp(AsyncOp* inOp, CompletionBase<AsyncOpPtr>* inCompletion, unsigned long inTimeout);
    bool RemoveTrackedOp(AsyncOp* inOp); // 0x7e5c00; LogicCompletion uses it to untrack its operation
    AsyncOpTracker mTracker; // 0x08, 0x34 bytes
    bool mKilled; // 0x3c
};
// LobbyLogicMgr (0x0c bytes, no vtable): owns the logic modules and dispatches events to them. Its list and every
// method are a Rust object since R6 WON lobby and network round 35 (rust/crates/ee_won_lobby/src/logicmgr.rs; the
// host is LobbyLogicMgr.cpp's): these forward; a logic's exception comes back and is thrown again here.
class LobbyLogicMgr {
    intptr_t Call(EeWonLogicMgrCall c, intptr_t a = 0, intptr_t b = 0) { return WonRethrow(ee_won_logicmgr_call(this->mRust, c, a, b)); }
public:
    LobbyLogicMgr();
    ~LobbyLogicMgr();
    void AddLogic(LobbyLogic* l) { this->Call(EeWonLogicMgrCall::AddLogic, (intptr_t)l); } // 0x7e5c10
    void Init(ComponentConfig* c) { this->Call(EeWonLogicMgrCall::Init, (intptr_t)c); }
    void Kill() { this->Call(EeWonLogicMgrCall::Kill); }
    void DispatchControlEvent(int i) { this->Call(EeWonLogicMgrCall::DispatchControlEvent, i); }
    void DispatchComponentEvent(ComponentEvent* e) { this->Call(EeWonLogicMgrCall::DispatchComponentEvent, (intptr_t)e); }
    void DispatchTimerEvent() { this->Call(EeWonLogicMgrCall::DispatchTimerEvent); } // 0x7e5e30; Reset (slot 0x0c)
    void HandleComponentEvent(ComponentEvent* e); // the event's type passed with it (LobbyLogicMgr.cpp)
    EeWonLogicMgr* mRust; // owned
private:
    LobbyLogicMgr(const LobbyLogicMgr&);
    LobbyLogicMgr& operator=(const LobbyLogicMgr&);
};

// The one shell of the lobby's logics that are Rust objects (R6 WON lobby and network round 36): AccountLogic,
// DialogLogic, InitLogic, RoutingLogic, StagingLogic and LanLogic were six classes; each is a RustLogic of its kind
// now. The virtuals, the dialogs InitComponent looks up and the completions are routed by
// rust/crates/ee_won_lobby/src/logicshell.rs to the kind's Rust object (mRust: an EeWonLogic, EeWonDlgLogic,
// EeWonRouteLogic, EeWonStaging or EeWonLan); the kind's host (its unit's) reaches the C++ objects the class held
// through the slots. NetLogic (14 pure virtuals; RoutingLogic and LanLogic) is this class too: its calls are the
// same-named methods. The slots are destroyed as the classes' members were: the dialogs last to first, then the
// others.
class LobbyRustDialog;
class RustLogic : public LobbyLogic {
public:
    typedef void* (*Maker)(RustLogic* inSelf); // the kind's Rust object over the shell and its unit's host
    RustLogic(EeWonLogicShellKind inKind, Maker inMake);
    virtual ~RustLogic();
    virtual void KillHook() { this->V(EeWonLogicShellV::KillHook); }
    virtual void InitComponent(ComponentConfig* inConfig) { WonRethrow(this->V(EeWonLogicShellV::InitComponent, (intptr_t)inConfig)); }
    virtual void Reset() { this->V(EeWonLogicShellV::Reset); }
    virtual void HandleControlEvent(int inControlId) { this->V(EeWonLogicShellV::HandleControlEvent, inControlId); }
    virtual void HandleComponentEvent(ComponentEvent* inEvent); // with the event's type
    // NetLogic's (the LAN and routing kinds; nothing and 0 for another kind)
    bool HostGame(LobbyGame* inGame) { return this->V(EeWonLogicShellV::HostGame, (intptr_t)inGame) != 0; }
    bool JoinGame(LobbyGame* inGame) { return this->V(EeWonLogicShellV::JoinGame, (intptr_t)inGame) != 0; }
    LobbyClient* GetMyClient() { return reinterpret_cast<LobbyClient*>(this->V(EeWonLogicShellV::GetMyClient)); }
    void SendToCaptain(const ByteBuffer* inBuffer) { this->VMessage(EeWonLogicShellV::SendToCaptain, inBuffer, NULL); }
    void BroadcastToGame(const ByteBuffer* inBuffer) { this->VMessage(EeWonLogicShellV::BroadcastToGame, inBuffer, NULL); }
    void SendToClient(LobbyClient* inClient, const ByteBuffer* inBuffer) { this->VMessage(EeWonLogicShellV::SendToClient, inBuffer, inClient); }
    void AdvertiseGame(LobbyGame* inGame) { this->V(EeWonLogicShellV::AdvertiseGame, (intptr_t)inGame); }
    void RemoveGameAdvertisement(LobbyGame* inGame) { this->V(EeWonLogicShellV::RemoveGameAdvertisement, (intptr_t)inGame); }
    void KickClient(LobbyClient* inClient, bool inBan) { this->V(EeWonLogicShellV::KickClient, (intptr_t)inClient, inBan); }
    void SetGameInProgress(bool inFlag) { this->V(EeWonLogicShellV::SetGameInProgress, inFlag); }
    void LeaveGame(LobbyGame* inGame) { this->V(EeWonLogicShellV::LeaveGame, (intptr_t)inGame); }
    void Suspend() { this->V(EeWonLogicShellV::Suspend); }
    void Resume(); // LanLogic's leaves the current game
    void Logout() { this->V(EeWonLogicShellV::Logout); }
    intptr_t V(EeWonLogicShellV inVirtual, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0);
    // a message virtual: the buffer, the client, the buffer's bytes and their length
    void VMessage(EeWonLogicShellV inVirtual, const ByteBuffer* inMessage, LobbyClient* inClient)
    { this->V(inVirtual, (intptr_t)inMessage, (intptr_t)inClient, inMessage != NULL ? (intptr_t)inMessage->data() : 0, inMessage != NULL ? (intptr_t)inMessage->length() : 0); }
    // the kind's own calls (the caller knows the kind)
    intptr_t Call(EeWonLogicCall inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
    { return ee_won_logic_call(static_cast<EeWonLogic*>(this->mRust), inCall, a, b, c); }
    intptr_t Call(EeWonRouteLogicCall inCall, intptr_t a = 0, intptr_t b = 0)
    { return ee_won_routelogic_call(static_cast<EeWonRouteLogic*>(this->mRust), inCall, a, b); }
    intptr_t Call(EeWonStagingCall inCall, void* a = NULL, void* b = NULL, intptr_t c = 0, intptr_t d = 0)
    { return ee_won_staging_call(static_cast<EeWonStaging*>(this->mRust), inCall, a, b, c, d); }
    intptr_t Call(EeWonLanCall inCall, void* a = NULL, void* b = NULL, intptr_t c = 0, intptr_t d = 0)
    { return ee_won_lan_call(static_cast<EeWonLan*>(this->mRust), inCall, a, b, c, d); }
    intptr_t Call(EeWonDlgLogicCall inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0, intptr_t e = 0)
    { return ee_won_dlglogic_call(static_cast<EeWonDlgLogic*>(this->mRust), inCall, a, b, c, d, e); }
    // a staging or LAN message call: the ByteBuffer, the client, the bytes and their length
    template <class C> void CallMessage(C inCall, const ByteBuffer* inMessage, LobbyClient* inClient)
    { this->Call(inCall, const_cast<ByteBuffer*>(inMessage), inClient, inMessage != NULL ? (intptr_t)inMessage->data() : 0, inMessage != NULL ? (intptr_t)inMessage->length() : 0); }
    template <class T> T* Slot(int inSlot) const { return static_cast<T*>(this->mSlot[inSlot].get()); }
    EeWonLogicShellKind mKind;
    SmartPtr<RefCount> mSlot[3]; // the kind's other objects (its unit names them)
    std::string mText; // InitLogic's cross-promotion link (0x44)
    IPAddr mAddr; // InitLogic's firewall test address (0x5c)
    SmartPtr<LobbyRustDialog> mDialog[6]; // the kind's dialogs (logicshell.rs's rows)
    void* mRust; // owned; made last: the maker may fill the slots
};
typedef RustLogic NetLogic;
// The kinds' slots and dialogs, named (the classes' members, in their order; WONLobbyAccountLogic.h's and
// WONLobbyRoutingLogic.h's until R6 WON round 39). The Init kind's cross-promotion link is mText (0x44), its
// firewall test address mAddr (0x5c); the routing kind's game data type was "G".
enum { kAccountCredits, kAccountTOU }; // mDialog: mCreditsDialog (0x54), mTOUDialog (0x58)
enum { kInitAdapter }; // mDialog: mNetworkAdapterDialog (0x84)
enum { kInitSession, kInitAcceptOp, kInitFirewallSocket }; // mSlot: mHTTPSession (0x40), mAcceptOp (0x54), mFirewallSocket (0x58)
enum { kRoutingServerList, kRoutingGameList }; // mSlot: mServerList (0x54), mGameList (0x58)
// mDialog: 0x74 (AddFriend) .. 0x88 (Password)
enum { kRoutingAddFriend, kRoutingAdminAction, kRoutingBadUser, kRoutingCreateRoom, kRoutingRoom, kRoutingPassword };
// The kinds' makers (each in its host's unit)
void* AccountLogicMake(RustLogic* inSelf);
void* DialogLogicMake(RustLogic* inSelf);
void* InitLogicMake(RustLogic* inSelf);
void* RoutingLogicMake(RustLogic* inSelf);
void* StagingLogicMake(RustLogic* inSelf);
void* LanLogicMake(RustLogic* inSelf);
void* PingLogicMake(RustLogic* inSelf);
// A completion of a RustLogic (LogicCompletion<T> with a member of one of the six classes, or RoutingLogic.cpp's
// RoutingOpCompletion with its code): logicshell.rs runs LogicCompletion's test and the member's call.
class ShellCompletion : public LobbyCompletionBase {
public:
    ShellCompletion(RustLogic* inLogic, EeWonLogicShellDone inDone, int inCode = 0) : mCheckTracker(false), mDone(inDone), mCode(inCode), mLogic(inLogic) {}
    virtual void Run(AsyncOp* inOp)
    { ee_won_logicshell_done(this->mLogic->mKind, this->mLogic->mRust, this->mDone, this->mCode, inOp, this->mLogic->mKilled, this->mCheckTracker, &Untrack, this->mLogic.get()); }
    static bool Untrack(void* inLogic, void* inOp) { return static_cast<RustLogic*>(inLogic)->RemoveTrackedOp(static_cast<AsyncOp*>(inOp)); }
    bool mCheckTracker; // (the original's LogicCompletionBase +0x08: set when run tracked)
    EeWonLogicShellDone mDone;
    int mCode;
    SmartPtr<RustLogic> mLogic;
};
}
#endif
