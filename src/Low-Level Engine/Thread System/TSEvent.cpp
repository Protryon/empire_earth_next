// C:\EEX\Low-Level Engine\Thread System\TSEvent.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngine.h"

// FUNCTION: LLE 0x10053cee
// Wraps an event handle (manual or automatic reset)
TSEvent::TSEvent(PlatEvent* inHandle, bool inManualReset)
    : mHandle(inHandle), mManualReset(inManualReset)
{
}

// FUNCTION: LLE 0x10053d00
// Destroys the event, closing its handle
TSEvent::~TSEvent()
{
    PlatEventDestroy(this->mHandle);
}

// FUNCTION: LLE 0x10053d09
// Signals the event; records the native error on failure
long TSEvent::Set()
{
    long theResult;
    if (PlatEventSet(this->mHandle))
        theResult = kErrorOK;
    else {
        theResult = kErrorNativeError;
        ESRecordNativeError(PlatGetLastError());
    }
    return theResult;
}

// FUNCTION: LLE 0x10053d2e
// Resets the event to unsignalled; records the native error on failure
long TSEvent::Clear()
{
    long theResult;
    if (PlatEventReset(this->mHandle))
        theResult = kErrorOK;
    else {
        theResult = kErrorNativeError;
        ESRecordNativeError(PlatGetLastError());
    }
    return theResult;
}

// FUNCTION: LLE 0x10053d53
// Waits for the event up to the timeout; kErrorTimeout on time-out, records the native error on failure
long TSEvent::Wait(unsigned long inTimeout)
{
    long theResult;
    long theWait = PlatEventWait(this->mHandle, inTimeout);
    if (theWait == PLAT_WAIT_TIMEOUT)
        theResult = kErrorTimeout;
    else if (theWait != PLAT_WAIT_FAILED)
        theResult = kErrorOK;
    else {
        theResult = kErrorNativeError;
        ESRecordNativeError(PlatGetLastError());
    }
    return theResult;
}

// Waits until one of the events is set -> its index, kTSWaitTimeout or kTSWaitFailed (the callers report failures)
long TSEvent::WaitAny(TSEvent* const* inEvents, unsigned long inCount, unsigned long inTimeout)
{
    PlatEvent* theHandles[PLAT_WAIT_MAX];
    if (inCount > PLAT_WAIT_MAX)
        return kTSWaitFailed;
    for (unsigned long i = 0; i < inCount; i++)
        theHandles[i] = inEvents[i]->mHandle;
    return PlatEventWaitAny(theHandles, inCount, inTimeout);
}
