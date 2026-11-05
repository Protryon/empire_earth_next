// C:\EEX\Low-Level Engine\Time Manager\TMTimer.cpp (folder name inferred)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/TMTimer.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x10053ad3

// FUNCTION: LLE 0x10053ae7
// Creates a stopped system timer with an interval (ms, or us for unit 1, kept in ms) and no callback or event
TMTimer::TMTimer(unsigned long inInterval, unsigned long inTimeUnit, bool inRepeating)
{
    this->mInterval = inInterval / (inTimeUnit ? 1000 : 1);
    this->mUnknown10 = 0;
    this->mRunning = false;
    this->mRepeating = inRepeating;
    this->mCallback = NULL;
    this->mParameter = NULL;
    this->mEvent = NULL;
    this->mTimerType = 0;
    this->mTimerID = 0;
    this->mStartTime = 0;
}

// FUNCTION: LLE 0x10053b28
// Destroys the timer, stopping it first
TMTimer::~TMTimer()
{
    this->Stop();
}

// FUNCTION: LLE 0x10053b2d
// Sets the callback and its parameter; kErrorBusy while the timer runs
long TMTimer::SetCallback(void (*inCallback)(void*), void* inParameter)
{
    long theResult = kErrorOK;
    if (!this->mRunning) {
        this->mCallback = inCallback;
        this->mParameter = inParameter;
    } else
        theResult = kErrorBusy;
    return theResult;
}

// FUNCTION: LLE 0x10053b49
// Sets the event signalled by an event-type timer; kErrorBusy while the timer runs
long TMTimer::SetEvent(TSEvent* inEvent)
{
    long theResult = kErrorOK;
    if (!this->mRunning)
        this->mEvent = inEvent;
    else
        theResult = kErrorBusy;
    return theResult;
}

// FUNCTION: LLE 0x10053b5f
// Sets the timer interval (ms, or us for unit 1); kErrorBusy while the timer runs
long TMTimer::SetInterval(unsigned long inInterval, unsigned long inTimeUnit)
{
    if (!this->mRunning) {
        this->mInterval = inInterval / (inTimeUnit ? 1000 : 1);
        this->mUnknown10 = 0;
        return kErrorOK;
    }
    return kErrorBusy;
}

// FUNCTION: LLE 0x10053b8f
// Sets whether the timer repeats; kErrorBusy while the timer runs
long TMTimer::SetRepeating(bool inRepeating)
{
    long theResult = kErrorOK;
    if (!this->mRunning)
        this->mRepeating = inRepeating;
    else
        theResult = kErrorBusy;
    return theResult;
}

// FUNCTION: LLE 0x10053ba5
// Sets the timer type (0 = callback, else event); kErrorBusy while the timer runs
long TMTimer::SetTimerType(unsigned long inType)
{
    long theResult = kErrorOK;
    if (!this->mRunning)
        this->mTimerType = inType;
    else
        theResult = kErrorBusy;
    return theResult;
}

// FUNCTION: LLE 0x10053bbb
// Starts the system timer (callback or event, one-shot or periodic); kErrorBusy if running, kErrorNotFound if not
long TMTimer::Start()
{
    long theResult;
    if (!this->mRunning) {
        this->mStartTime = PlatTimeMilliseconds();
        if (this->mTimerType == 0)
            this->mTimerID = PlatTimerStartCallback(this->mInterval, this->mRepeating, TMTimer::TimerCallback, this);
        else
            this->mTimerID = PlatTimerStartEvent(this->mInterval, this->mRepeating, this->mEvent->mHandle);
        if (this->mTimerID) {
            this->mRunning = true;
            theResult = kErrorOK;
        } else
            theResult = kErrorNotFound;
    } else
        theResult = kErrorBusy;
    return theResult;
}

// FUNCTION: LLE 0x10053c18
// Stops the timer; kErrorNotFound if it is not running or cannot be killed
long TMTimer::Stop()
{
    if (this->mRunning && PlatTimerStop(this->mTimerID)) {
        this->mRunning = false;
        return kErrorOK;
    }
    return kErrorNotFound;
}

// FUNCTION: LLE 0x10053c38
// Stops the timer, returning the time since it last started (the remaining time is always 0)
long TMTimer::Stop(unsigned long& outElapsed, unsigned long& outRemaining)
{
    if (!this->mRunning)
        return kErrorNotFound;
    unsigned long theNow = PlatTimeMilliseconds();
    if (PlatTimerStop(this->mTimerID)) {
        outElapsed = theNow - this->mStartTime;
        outRemaining = 0;
        this->mRunning = false;
        return kErrorOK;
    }
    return kErrorNotFound;
}

// FUNCTION: LLE 0x10053c77
// System-timer callback: restarts the elapsed clock (repeating) or marks the timer stopped, then calls the callback
void TMTimer::TimerCallback(void* inUser)
{
    TMTimer* theTimer = static_cast<TMTimer*>(inUser);
    if (theTimer->mRepeating)
        theTimer->mStartTime = PlatTimeMilliseconds();
    else
        theTimer->mRunning = false;
    if (theTimer->mCallback)
        theTimer->mCallback(theTimer->mParameter);
}
