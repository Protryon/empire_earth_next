// ne_server_test stand-in for the server's base class: its fields (the driver sets them) and the queues' common calls
// out over the world (the recorder passes messages through).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "LowLevelEngine.h"
class NEMessage;
enum class EeNetQOp : uint32_t;
enum NERecordPlaybackMode { kRecordModePassing = 1, kRecordModeRecording = 2, kRecordModePlaying = 3 };
class NESynchronousCommunications {
public:
    NESynchronousCommunications(unsigned long inNumberOfCTsProcessedInPhysics = 0)
        : mNumberOfCTsProcessedInPhysics(inNumberOfCTsProcessedInPhysics)
    {
    }
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
    unsigned long GetNumberOfCTsProcessedInPhysics() { return this->mNumberOfCTsProcessedInPhysics; }
protected:
    void SetIsGamePaused(bool inIsGamePaused) { this->mIsGamePaused = inIsGamePaused; }
    static ptrdiff_t QueueHostCommon(EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d);
public:
    long mLastTickReceived = -2;
    volatile unsigned long mNumberOfCTsProcessedInPhysics = 0;
    unsigned long mAveragePhysicsUpdateTime = 0;
    unsigned char mNumHumanPlayers = 1;
    bool mIsGamePaused = false;
    bool mIsMultiplayerSession = false;
};
