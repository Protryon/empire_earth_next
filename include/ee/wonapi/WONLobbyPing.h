#ifndef EE_WONAPI_WONLOBBYPING_H
#define EE_WONAPI_WONLOBBYPING_H
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "rust/ee_won.h"
// PingGameOp (0x98 bytes; global namespace, not WONAPI, per its RTTI) is defined in PingLogic.cpp.
// PingLogic (0x50 bytes): pings listed games and answers ping requests; a Rust object since R6 WONLobby round 8
// (rust/crates/ee_won/src/logics.rs: the request id and the queue there).
namespace WONAPI {
class PingLogic : public LobbyLogic {
public:
    PingLogic(); // 0x7e6490
    virtual ~PingLogic(); // 0x7e6520
    virtual void InitComponent(ComponentConfig*); // 0x7e65d0
    virtual void HandleComponentEvent(ComponentEvent*); // 0x7e6680
    void HandleUDPRecv(AsyncOp*); // 0x7e6810, UDP receive completion
    void PingGameDone(AsyncOp*); // 0x7e68b0, PingGameOp completion
    int GetNumPingsToDo(); // 0x7e6b40
    void PingGame(LobbyGame*, bool, bool); // 0x7e6b50
    void KillPings(int); // 0x7e6db0; the argument is unused
    intptr_t Call(EeWonLogicCall inCall, intptr_t inA = 0, intptr_t inB = 0, intptr_t inC = 0)
    { return ee_won_logic_call(this->mRust, inCall, inA, inB, inC); }
    EeWonLogic* mRust;
};
}

#endif
