// Lobby logic modules: LobbyLogic (their base), LobbyLogicMgr and NetLogic (LAN or internet play)
#ifndef EE_WONAPI_WONLOBBYLOGIC_H
#define EE_WONAPI_WONLOBBYLOGIC_H
#include "ee/wonapi/WONRouting.h"
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
    LobbyLogic(); // 0x7e5930
    virtual ~LobbyLogic(); // 0x7e5a40
    virtual void KillHook(); // slot 0x04: empty, the shared no-argument stub at 0x8142a0
    virtual void InitComponent(ComponentConfig* inConfig); // slot 0x08: empty, the shared one-argument stub at 0x6fa500
    virtual void Reset(); // slot 0x0c: empty, the shared no-argument stub at 0x8142a0
    virtual void HandleControlEvent(int inControlId); // slot 0x10: empty, the shared one-argument stub at 0x6fa500
    virtual void HandleComponentEvent(ComponentEvent* inEvent); // slot 0x14: empty, the shared one-argument stub at 0x6fa500
    void Kill(); // 0x7e5b00
    void RunTrackedOp(AsyncOp* inOp, LogicCompletionBase* inCompletion, unsigned long inTimeout); // 0x7e5b20
    void RunOp(AsyncOp* inOp, CompletionBase<AsyncOpPtr>* inCompletion, unsigned long inTimeout); // 0x7e5ba0
    bool RemoveTrackedOp(AsyncOp* inOp); // 0x7e5c00; LogicCompletion uses it to untrack its operation
    AsyncOpTracker mTracker; // 0x08, 0x34 bytes
    bool mKilled; // 0x3c
};
// LobbyLogicMgr (0x0c bytes, no vtable): owns the logic modules and dispatches events to them.
class LobbyLogicMgr {
public:
    LobbyLogicMgr();
    void AddLogic(LobbyLogic*); // 0x7e5c10; appends to the logic list
    void Init(ComponentConfig*); // 0x7e5d40
    void Kill(); // 0x7e5d80
    void DispatchControlEvent(int); // 0x7e5db0
    void DispatchComponentEvent(ComponentEvent*); // 0x7e5df0
    void DispatchTimerEvent(); // 0x7e5e30; calls Reset (slot 0x0c) on every logic
    void HandleComponentEvent(ComponentEvent*); // 0x7e5e60
    typedef std::list<SmartPtr<LobbyLogic> > LogicList;
    LogicList mLogicList; // 0x00 allocator, 0x04 head, 0x08 count
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
