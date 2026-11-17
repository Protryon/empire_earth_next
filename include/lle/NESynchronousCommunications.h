// C:\EEX\Low-Level Engine\Network Engine\NESynchronousCommunications.h (RECONSTRUCTION; NESynchronousCommunications.cpp
// 0x47301..0x47400). The abstract base of the lock-step multiplayer server and client: game-session flags, physics
// tick bookkeeping and the bytes-per-second meters over protocol 0. Abstract: the game subclasses the server/client.
// Inline members are exported COMDATs emitted by NEMessageManager.cpp (0x4012f..0x4027f, declaration order).
#ifndef LLE_NESYNCHRONOUSCOMMUNICATIONS_H
#define LLE_NESYNCHRONOUSCOMMUNICATIONS_H

#include <stddef.h>
#include <stdint.h>
#include "rust/net/net.h"

class NEMessage;
enum class EeNetQOp : uint32_t;             // the queues' calls out (rust/ee_net.h)

// The recorder's mode (ee_net recplay.rs, NERecordPlayback whole in Rust; the game only ever passes)
enum NERecordPlaybackMode {
    kRecordModePassing = 1,                 // messages go straight to the callback
    kRecordModeRecording = 2,
    kRecordModePlaying = 3,
};

// Its state is Rust's (ee_net comms.rs; PORT_LEARNINGS "LLE Network Engine, round 5"): the class keeps the type the
// game holds, its pure virtuals and one-line forwarders. The original layout (sizeof 0x30) went with its fields;
// the debug lock and the two bytes-per-second meters (no caller) went too.
class LLE_API NESynchronousCommunications {
public:
    NESynchronousCommunications(unsigned long inNumberOfCTsProcessedInPhysics = 0);   // default arg: ??_F 0x4021b
    // vtable 0x988f4: the dtor, then pure virtuals in alphabetical order (slots 2, 5..8 are named by the server's
    // and client's overrides; slots 1, 3, 4 stay pure in both: the game implements them; named by vtable offset, signatures unknown;
    // the server and client have non-virtual members called Disconnect/ProcessMessage, so no guessing names here)
    virtual ~NESynchronousCommunications();
#ifdef LLE_EXE_HEADERS
    // a player left (EEServer 0xcb8f6 / EEClient 0xb7722: ret 0x10; the player, then three flags)
    virtual void ExecuteRemovePlayer(unsigned char inPlayer, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting) = 0;   // slot 1 (provisional name)
#else
    virtual void ExecuteRemovePlayer() = 0;                                     // slot 1 (provisional name)
#endif
    virtual unsigned long GetCurrentCommunicationsTick() const = 0;             // slot 2
#ifdef LLE_EXE_HEADERS
    // the game tests the result (`test al, al`: lobby 0x1da50b/0x1db01e/0x1db772, EEPMultiplayerHelper)
    virtual bool IsClient() = 0;                                                // slot 3 (provisional name)
    virtual bool IsServer() = 0;                                                // slot 4 (provisional name)
#else
    virtual void IsClient() = 0;                                                // slot 3 (provisional name)
    virtual void IsServer() = 0;                                                // slot 4 (provisional name)
#endif
    virtual long ProcessGuaranteedLocalUserInput(NEMessage* inMessage, unsigned char inPlayer) = 0;   // slot 5
    virtual long ProcessLocalUserInput(NEMessage* inMessage, unsigned char inPlayer) = 0;            // slot 6
    virtual long SearchForGame(bool inStart) = 0;                               // slot 7
    virtual void SetCurrentCommunicationsTick(unsigned long inTick) = 0;        // slot 8

    // the fields (ee_net comms.rs, shared with the Rust server or client)
    unsigned long GetAveragePhysicsUpdateTime() { return this->Get(EeNetCommsField::AveragePhysicsUpdateTime); }
    bool GetIsGamePaused() { return this->Get(EeNetCommsField::IsGamePaused) != 0; }
    bool GetIsMultiplayerSession() { return this->Get(EeNetCommsField::IsMultiplayerSession) != 0; }
    // a `long` (-2 before the connection, -1 "none") returned as the original's unsigned long
    unsigned long GetLastTickReceived() { return (unsigned long)(long)(int32_t)this->Get(EeNetCommsField::LastTickReceived); }
    unsigned char GetNumHumanPlayers() const { return (unsigned char)this->Get(EeNetCommsField::NumHumanPlayers); }
    unsigned long GetNumberOfCTsProcessedInPhysics() { return this->Get(EeNetCommsField::CtsProcessed); }
    unsigned long GetPlayerNumber() { return this->Get(EeNetCommsField::PlayerNumber); }
    void IncrementNumberOfCTsProcessedInPhysics() { ee_net_comms_increment_cts(this->mComms); }
    void SetAveragePhysicsUpdateTime(unsigned long inTime) { this->Set(EeNetCommsField::AveragePhysicsUpdateTime, inTime); }
    void SetAverageTimeBetweenPhysicsTicks(unsigned long inTime) { this->Set(EeNetCommsField::AverageTimeBetweenPhysicsTicks, inTime); }
    void SetIsMultiplayerSession(bool inIsMultiplayerSession) { this->Set(EeNetCommsField::IsMultiplayerSession, inIsMultiplayerSession); }
    void SetLastTickReceived(unsigned long inTick) { this->Set(EeNetCommsField::LastTickReceived, inTick); }
    void SetNumHumanPlayers(unsigned char inNumHumanPlayers) { this->Set(EeNetCommsField::NumHumanPlayers, inNumHumanPlayers); }
    void SetNumberOfCTsProcessedInPhysics(unsigned long inNumber) { this->Set(EeNetCommsField::CtsProcessed, inNumber); }
    void SetPlayerNumber(unsigned long inPlayerNumber) { this->Set(EeNetCommsField::PlayerNumber, inPlayerNumber); }
protected:
    // the calls out of the lag simulator and the recorder (ee_net's EeNetQOp) that do not reach their owner: the
    // clock, events, threads, floating-point control, messages and record files (the owner answers Deliver)
    static ptrdiff_t QueueHostCommon(EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d);

    EeNetComms* mComms;                     // the fields (the subclass hands them to its Rust server or client)

private:
    NESynchronousCommunications(const NESynchronousCommunications&);
    NESynchronousCommunications& operator=(const NESynchronousCommunications&);
    uint32_t Get(EeNetCommsField inField) const { return ee_net_comms_get(this->mComms, inField); }
    void Set(EeNetCommsField inField, unsigned long inValue) { ee_net_comms_set(this->mComms, inField, (uint32_t)inValue); }
};

#endif
