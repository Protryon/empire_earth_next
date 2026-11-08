#ifndef EE_WONAPI_WONLOBBYPING_H
#define EE_WONAPI_WONLOBBYPING_H
#include <list>
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyModel.h"
// PingGameOp (0x98 bytes; global namespace, not WONAPI, per its RTTI) is defined in PingLogic.cpp.
// PingLogic (0x50 bytes): pings listed games and answers ping requests.
namespace WONAPI {
class PingLogic : public LobbyLogic {
public:
    PingLogic(); // 0x7e6490
    virtual ~PingLogic(); // 0x7e6520; explicit: writes its own vtable pointer
    virtual void InitComponent(ComponentConfig*); // 0x7e65d0
    virtual void HandleComponentEvent(ComponentEvent*); // 0x7e6680
    void HandlePingRequest(ReadBuffer&, const IPAddr&); // 0x7e66a0
    void HandleUDPRecv(AsyncOp*); // 0x7e6810, UDP receive completion
    void PingGameDone(AsyncOp*); // 0x7e68b0, PingGameOp completion
    int GetNumPingsToDo(); // 0x7e6b40
    void PingGame(LobbyGame*, bool, bool); // 0x7e6b50
    void KillPings(int); // 0x7e6db0; the argument is unused
    unsigned short mNextRequestId; // 0x40
    std::list<SmartPtr<LobbyGame> > mPending; // 0x44
};
}

#endif
