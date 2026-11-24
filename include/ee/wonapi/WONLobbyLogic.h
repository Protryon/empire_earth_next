// Lobby logic modules: LobbyLogic (their base), LobbyLogicMgr and NetLogic (LAN or internet play)
#ifndef EE_WONAPI_WONLOBBYLOGIC_H
#define EE_WONAPI_WONLOBBYLOGIC_H
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyGuard.h"
#include "rust/won_lobby/logicmgr.h"
namespace WONAPI {
class ComponentConfig;
class ComponentEvent;
class LobbyGame;
class LobbyClient;
class LogicCompletionBase;
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
    void RunTrackedOp(AsyncOp* inOp, LogicCompletionBase* inCompletion, unsigned long inTimeout);
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

// RTTI: NetLogic > LobbyLogic > RefCount; vtable 0x855974 adds 14 pure virtual slots.
// NetLogic has no data members of its own.
class NetLogic : public LobbyLogic {
public:
    // Implicit virtual destructor: 0x7dfe00 (tail jump to the base), deleting destructor 0x7dfed0.
    virtual bool HostGame(LobbyGame* inGame) = 0;
    virtual bool JoinGame(LobbyGame* inGame) = 0;
    virtual LobbyClient* GetMyClient() = 0;
    virtual void SendToCaptain(const ByteBuffer* inBuffer) = 0;
    virtual void BroadcastToGame(const ByteBuffer* inBuffer) = 0;
    virtual void SendToClient(LobbyClient* inClient, const ByteBuffer* inBuffer) = 0;
    virtual void AdvertiseGame(LobbyGame* inGame) = 0;
    virtual void RemoveGameAdvertisement(LobbyGame* inGame) = 0;
    virtual void KickClient(LobbyClient* inClient, bool inFlag) = 0;
    virtual void SetGameInProgress(bool inFlag) = 0;
    virtual void LeaveGame(LobbyGame* inGame) = 0;
    virtual void Suspend() = 0;
    virtual void Resume() = 0;
    virtual void Logout() = 0;
};
}
#endif
