// C:\EEX\Low-Level Engine\Network Engine\NESynchronousCommunications.h (RECONSTRUCTION; NESynchronousCommunications.cpp
// 0x47301..0x47400). The abstract base of the lock-step multiplayer server and client: game-session flags, physics
// tick bookkeeping and the bytes-per-second meters over protocol 0. Abstract: the game subclasses the server/client.
// Inline members are exported COMDATs emitted by NEMessageManager.cpp (0x4012f..0x4027f, declaration order).
#ifndef LLE_NESYNCHRONOUSCOMMUNICATIONS_H
#define LLE_NESYNCHRONOUSCOMMUNICATIONS_H

#include <stddef.h>
#include <stdint.h>
#include "lle/TSCriticalSection.h"
#include "rust/ee_net.h"

class NEMessage;
enum class EeNetQOp : uint32_t;             // the queues' calls out (rust/ee_net.h)

// The recorder's mode (ee_net recplay.rs, NERecordPlayback whole in Rust; the game only ever passes)
enum NERecordPlaybackMode {
    kRecordModePassing = 1,                 // messages go straight to the callback
    kRecordModeRecording = 2,
    kRecordModePlaying = 3,
};

class LLE_API NESynchronousCommunications {   // proven sizeof 0x30 (implicit copy ctor 0x40167)
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

    unsigned long GetAveragePhysicsUpdateTime() { return this->mAveragePhysicsUpdateTime; }  // 0x40123 (ICF-folded)
    bool GetIsGamePaused() { return this->mIsGamePaused; }
    bool GetIsMultiplayerSession() { return this->mIsMultiplayerSession; }
    unsigned long GetLastTickReceived() { return this->mLastTickReceived; }    // ICF-folded (0x4380)
    unsigned char GetNumHumanPlayers() const { return this->mNumHumanPlayers; }
    unsigned long GetNumberOfCTsProcessedInPhysics() { return this->mNumberOfCTsProcessedInPhysics; }  // 0x4012b (folded)
    unsigned long GetPlayerNumber() { return this->mPlayerNumber; }            // ICF-folded (0x4495)
    void IncrementNumberOfCTsProcessedInPhysics() { this->mNumberOfCTsProcessedInPhysics++; }
    // exported setters, all ICF-folded elsewhere (0xdfc9, 0x124c1, 0xc56c, 0x572e7, 0x4c3eb, 0xe773)
    void SetAveragePhysicsUpdateTime(unsigned long inTime) { this->mAveragePhysicsUpdateTime = inTime; }
    void SetAverageTimeBetweenPhysicsTicks(unsigned long inTime) { this->mAverageTimeBetweenPhysicsTicks = inTime; }
    void SetIsMultiplayerSession(bool inIsMultiplayerSession) { this->mIsMultiplayerSession = inIsMultiplayerSession; }
    void SetLastTickReceived(unsigned long inTick) { this->mLastTickReceived = inTick; }
    void SetNumHumanPlayers(unsigned char inNumHumanPlayers) { this->mNumHumanPlayers = inNumHumanPlayers; }
    void SetNumberOfCTsProcessedInPhysics(unsigned long inNumber) { this->mNumberOfCTsProcessedInPhysics = inNumber; }
    void SetPlayerNumber(unsigned long inPlayerNumber) { this->mPlayerNumber = inPlayerNumber; }
protected:
    void SetIsGamePaused(bool inIsGamePaused) { this->mIsGamePaused = inIsGamePaused; }
    // the calls out of the lag simulator and the recorder (ee_net's EeNetQOp) that do not reach their owner: the
    // clock, events, threads, floating-point control, messages and record files (the owner answers Deliver)
    static ptrdiff_t QueueHostCommon(EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d);

    long mLastTickReceived;                 // +0x04 (-2 at construction) the last tick received (accessor names)
    TSCriticalSection* mDebugInfoLock;      // +0x08
    unsigned long mPlayerNumber;            // +0x0c the player number (accessor names)
    volatile unsigned long mNumberOfCTsProcessedInPhysics;   // +0x10 (volatile: the increment is load/inc/store)
    unsigned long mAveragePhysicsUpdateTime;  // +0x14 the average physics update time (accessor names)
    unsigned long mAverageTimeBetweenPhysicsTicks;  // +0x18 the average time between physics ticks (accessor names)
    unsigned long mLastReceiveBytes;        // +0x1c protocol 0's byte count at the last GetCurrentReceiveBytesPerSec
    unsigned long mLastReceiveTime;         // +0x20
    unsigned long mLastSendBytes;           // +0x24
    unsigned long mLastSendTime;            // +0x28
    unsigned char mNumHumanPlayers;         // +0x2c
    bool mIsGamePaused;                     // +0x2d
    bool mIsMultiplayerSession;             // +0x2e
};

#endif
