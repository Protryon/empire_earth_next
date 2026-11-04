// C:\EEX\Low-Level Engine\Network Engine\NESynchronousCommunications.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The class's inline members are emitted by NEMessageManager.cpp (first includer); this unit holds the out-of-line
// members only. ESVERIFY in the ctor is on line 43 (0x2b).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/NEProtocol.h"
#include "lle/NESynchronousCommunications.h"

// FUNCTION: LLE 0x10047301
// Creates the shared client/server state: tick counters, pause flag, bandwidth counters and debug lock
NESynchronousCommunications::NESynchronousCommunications(unsigned long inNumberOfCTsProcessedInPhysics)
{
    this->mNumberOfCTsProcessedInPhysics = inNumberOfCTsProcessedInPhysics;
    this->mLastTickReceived = -2;
    this->mAveragePhysicsUpdateTime = 0;
    this->mAverageTimeBetweenPhysicsTicks = 0;
    this->mIsGamePaused = false;
    this->mNumHumanPlayers = 0;
    this->mPlayerNumber = 0;
    this->mIsMultiplayerSession = false;
    this->mLastReceiveBytes = 0;
    this->mLastSendBytes = 0;
    this->mLastReceiveTime = this->mLastSendTime = TMGetTime();
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mDebugInfoLock) == keseOK, 43);
}

// FUNCTION: LLE 0x10047365
// Destroys the communications state (frees the debug lock)
NESynchronousCommunications::~NESynchronousCommunications()
{
    TSScheduler::DestroyCriticalSection(this->mDebugInfoLock);
}

// FUNCTION: LLE 0x10047375
// Returns bytes received per second since the last call
unsigned long NESynchronousCommunications::GetCurrentReceiveBytesPerSec()
{
    NEProtocol* theProtocol = NULL;
    NEProtocol::GetProtocol(0, theProtocol);
    unsigned long theTime = TMGetTime();
    unsigned long theBytes = theProtocol->GetBytesReceived();
    unsigned long theDeltaBytes = theBytes - this->mLastReceiveBytes;
    unsigned long theDeltaTime = theTime - this->mLastReceiveTime;
    // Fix: a second call within the same millisecond divided by zero; it now reports 0 and keeps measuring from
    // the previous call.
    if (theDeltaTime == 0)
        return 0;
    this->mLastReceiveBytes = theBytes;
    this->mLastReceiveTime = theTime;
    return theDeltaBytes * 1000 / theDeltaTime;
}

// FUNCTION: LLE 0x100473bc
// Returns bytes sent per second since the last call
unsigned long NESynchronousCommunications::GetCurrentSendBytesPerSec()
{
    NEProtocol* theProtocol = NULL;
    NEProtocol::GetProtocol(0, theProtocol);
    unsigned long theTime = TMGetTime();
    unsigned long theBytes = theProtocol->GetBytesSent();
    unsigned long theDeltaBytes = theBytes - this->mLastSendBytes;
    unsigned long theDeltaTime = theTime - this->mLastSendTime;
    // Fix: a second call within the same millisecond divided by zero; it now reports 0 and keeps measuring from
    // the previous call.
    if (theDeltaTime == 0)
        return 0;
    this->mLastSendBytes = theBytes;
    this->mLastSendTime = theTime;
    return theDeltaBytes * 1000 / theDeltaTime;
}
