// C:\EEX\Low-Level Engine\Network Engine\NESynchronousCommunications.h (RECONSTRUCTION; NESynchronousCommunications.cpp
// 0x47301..0x47400). The abstract base of the lock-step multiplayer server and client: game-session flags, physics
// tick bookkeeping and the bytes-per-second meters over protocol 0. The game subclasses the server/client.
// Inline members are exported COMDATs emitted by NEMessageManager.cpp (0x4012f..0x4027f, declaration order).
#ifndef LLE_NESYNCHRONOUSCOMMUNICATIONS_H
#define LLE_NESYNCHRONOUSCOMMUNICATIONS_H
#include "rust/net/msgid.h"

#include <stddef.h>
#include <stdint.h>
#include "rust/net/net.h"

#include "lle/LLEForward.h"

// Its state is Rust's (ee_net comms.rs; PORT_LEARNINGS "LLE Network Engine, round 5"): the class keeps the type the
// game holds, its pure virtuals and one-line forwarders. The original layout (sizeof 0x30) went with its fields;
// the debug lock and the two bytes-per-second meters (no caller) went too.
class LLE_API NESynchronousCommunications {
public:
    // the shared client/server state (ee_net comms.rs): the CTs processed given, the last tick received -2, the rest
    // 0; the destructor gives up the base's reference (the Rust server or client, freed first, held its own)
    NESynchronousCommunications(unsigned long inNumberOfCTsProcessedInPhysics = 0)   // default arg: ??_F 0x4021b
        : mComms(ee_net_comms_new((uint32_t)inNumberOfCTsProcessedInPhysics)) {}
    virtual ~NESynchronousCommunications() { ee_net_comms_free(this->mComms); }
    // a player left (EEServer 0xcb8f6 / EEClient 0xb7722: the player, then three flags; slot 1, provisional name):
    // the one virtual member left (the original's slots 2..8 are the members below, LLE round 13)
    virtual void ExecuteRemovePlayer(unsigned char inPlayer, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting) = 0;
    // the server's and the client's members, on whichever Rust object the subclass made
    unsigned long GetCurrentCommunicationsTick() const
    {
        return this->mServer ? ee_net_sv_get(this->mServer, EeNetSvGet::CurrentCommunicationsTick)
                             : ee_net_cl_current_communications_tick(this->mClient);
    }
    bool IsClient() const { return this->mClient != NULL; }
    bool IsServer() const { return this->mServer != NULL; }
    long ProcessGuaranteedLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
    {
        return this->mServer ? ee_net_sv_process_local_user_input(this->mServer, inMessage, inPlayer)
                             : ee_net_cl_process_user_input(this->mClient, inMessage, inPlayer, true);
    }
    long ProcessLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
    {
        return this->mServer ? ee_net_sv_process_local_user_input(this->mServer, inMessage, inPlayer)
                             : ee_net_cl_process_user_input(this->mClient, inMessage, inPlayer, false);
    }
    long SearchForGame(bool inStart)
    {
        return this->mServer ? ee_net_sv_search_for_game(this->mServer, inStart)
                             : ee_net_cl_search_for_game(this->mClient, inStart);
    }

    unsigned long GetAveragePhysicsUpdateTime() { return this->Get(EeNetCommsField::AveragePhysicsUpdateTime); }
    bool GetIsGamePaused() { return this->Get(EeNetCommsField::IsGamePaused) != 0; }
    bool GetIsMultiplayerSession() { return this->Get(EeNetCommsField::IsMultiplayerSession) != 0; }
    // a `long` (-2 before the connection, -1 "none") returned as the original's unsigned long
    unsigned long GetLastTickReceived() { return (unsigned long)(long)(int32_t)this->Get(EeNetCommsField::LastTickReceived); }
    unsigned char GetNumHumanPlayers() const { return (unsigned char)this->Get(EeNetCommsField::NumHumanPlayers); }
    unsigned long GetNumberOfCTsProcessedInPhysics() { return this->Get(EeNetCommsField::CtsProcessed); }
    void IncrementNumberOfCTsProcessedInPhysics() { ee_net_comms_increment_cts(this->mComms); }
    void SetAveragePhysicsUpdateTime(unsigned long inTime) { this->Set(EeNetCommsField::AveragePhysicsUpdateTime, inTime); }
    void SetAverageTimeBetweenPhysicsTicks(unsigned long inTime) { this->Set(EeNetCommsField::AverageTimeBetweenPhysicsTicks, inTime); }
    void SetIsMultiplayerSession(bool inIsMultiplayerSession) { this->Set(EeNetCommsField::IsMultiplayerSession, inIsMultiplayerSession); }
    void SetLastTickReceived(unsigned long inTick) { this->Set(EeNetCommsField::LastTickReceived, inTick); }
    void SetNumHumanPlayers(unsigned char inNumHumanPlayers) { this->Set(EeNetCommsField::NumHumanPlayers, inNumHumanPlayers); }
    void SetPlayerNumber(unsigned long inPlayerNumber) { this->Set(EeNetCommsField::PlayerNumber, inPlayerNumber); }
    // the calls out of the lag simulator and the recorder (ee_net's EeNetQOp) that do not reach their owner: the
    // clock, events, threads, floating-point control, messages and record files (the owner answers Deliver)
    static ptrdiff_t QueueHostCommon(EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d);
    // a temporary as the lvalue an out or in-out reference parameter takes (the hosts' calls; it lives to the end of
    // the full expression)
    template <class T> static T& Temp(T&& inValue) { return inValue; }

    URust<EeNetComms> mComms;                     // the fields (the subclass hands them to its Rust server or client)
    EeNetServer* mServer = NULL;            // NESynchronousServer's Rust object (ee_net server.rs), else NULL
    EeNetClient* mClient = NULL;            // NESynchronousClient's (ee_net client.rs), else NULL

    uint32_t Get(EeNetCommsField inField) const { return ee_net_comms_get(this->mComms, inField); }
    void Set(EeNetCommsField inField, unsigned long inValue) { ee_net_comms_set(this->mComms, inField, (uint32_t)inValue); }
};

#endif
