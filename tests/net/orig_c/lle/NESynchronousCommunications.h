// ne_client_test stand-in for the client's base class: its fields (the driver sets them) and the queues' common
// calls out over the world (the clock and ESVERIFY; the client's queues pass messages through and make no other).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "c_world.h"
class NEMessage;
enum class EeNetQOp : uint32_t;
enum NERecordPlaybackMode { kRecordModePassing = 1, kRecordModeRecording = 2, kRecordModePlaying = 3 };
class NESynchronousCommunications {
public:
    NESynchronousCommunications() {}
    virtual ~NESynchronousCommunications() {}
    virtual void ExecuteRemovePlayer() = 0;
    virtual unsigned long GetCurrentCommunicationsTick() const = 0;
    virtual void IsClient() = 0;
    virtual void IsServer() = 0;
    virtual long ProcessGuaranteedLocalUserInput(NEMessage* inMessage, unsigned char inPlayer) = 0;
    virtual long ProcessLocalUserInput(NEMessage* inMessage, unsigned char inPlayer) = 0;
    virtual long SearchForGame(bool inStart) = 0;
    virtual void SetCurrentCommunicationsTick(unsigned long inTick) = 0;
    unsigned long GetAveragePhysicsUpdateTime() { return this->mAveragePhysicsUpdateTime; }
protected:
    void SetIsGamePaused(bool inIsGamePaused) { this->mIsGamePaused = inIsGamePaused; }
    static ptrdiff_t QueueHostCommon(EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d);
public:
    long mLastTickReceived = -2;
    volatile unsigned long mNumberOfCTsProcessedInPhysics = 0;
    unsigned long mAveragePhysicsUpdateTime = 0;
    bool mIsGamePaused = false;
    bool mIsMultiplayerSession = false;
};
