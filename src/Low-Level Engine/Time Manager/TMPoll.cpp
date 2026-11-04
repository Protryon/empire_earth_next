// C:\EEX\Low-Level Engine\Time Manager\TMPoll.cpp (folder name inferred)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#include <mmsystem.h>

#include "lle/LowLevelEngine.h"
#include "lle/TMPoll.h"

__int64 TMPoll::sCounterFrequency;

// TMPoll.h's inline members, emitted here (first includer):
// SYNTHETIC: LLE 0x10053819
// SYNTHETIC: LLE 0x10053835
// SYNTHETIC: LLE 0x10053839
// SYNTHETIC: LLE 0x1005384a

// FUNCTION: LLE 0x1005384f
// Creates a poll that fires every delay (ms, or us for unit 1), using the performance counter when there is one
TMPoll::TMPoll(unsigned long inTimeDelay, unsigned long inTimeUnit, bool inStart)
{
    LARGE_INTEGER theFrequency;
    long theDivisor = inTimeUnit ? 1000000 : 1000;
    if (QueryPerformanceFrequency(&theFrequency)) {
        this->mUseCounter = true;
        TMPoll::sCounterFrequency = theFrequency.QuadPart / theDivisor;
        QueryPerformanceCounter(&this->mStartTime);
    } else {
        // Fixed: the delay used to become inTimeDelay / divisor / 1000, i.e. 0 for any realistic delay, and Poll
        // then divided by zero; timeGetTime counts milliseconds, so only a microsecond delay is converted
        if (inTimeUnit)
            inTimeDelay /= 1000;
        if (inTimeDelay == 0)
            inTimeDelay = 1;
        TMPoll::sCounterFrequency = 1;
        this->mStartTime.LowPart = timeGetTime();
        this->mStartTime.HighPart = 0;
        this->mUseCounter = false;
    }
    this->mPauseRemaining.LowPart = 0;
    this->mTimeUnit = inTimeUnit;
    this->mRunning = false;
    this->mPauseRemaining.HighPart = 0;
    this->mTimeDelay = inTimeDelay;
    if (inStart)
        this->Start();
}

// FUNCTION: LLE 0x100538ff
// Returns the time left in the current period (0 when stopped)
unsigned long TMPoll::GetTimeLeft() const
{
    unsigned long theTimeLeft = 0;
    unsigned long theScale = 1;
    if (this->mRunning) {
        __int64 theNow;
        if (this->mUseCounter)
            QueryPerformanceCounter((LARGE_INTEGER*)&theNow);
        else {
            theNow = timeGetTime();
            if (this->mTimeUnit == 1)
                theScale = 1000;
        }
        __int64 theElapsed = (theNow - this->mStartTime.QuadPart) / TMPoll::sCounterFrequency;
        if (theElapsed < this->mTimeDelay)
            theTimeLeft = this->mTimeDelay - (unsigned long)theElapsed;
    }
    return theScale * theTimeLeft;
}

// FUNCTION: LLE 0x10053974
// Pauses the poll, remembering the time already elapsed in the current period
void TMPoll::Pause()
{
    this->mPauseRemaining.HighPart = 0;
    this->mPauseRemaining.LowPart = this->mTimeDelay - this->GetTimeLeft();
}

// FUNCTION: LLE 0x1005398a
// Returns the whole periods passed since the last poll and advances the start by them (0 if stopped or paused)
unsigned long TMPoll::Poll()
{
    unsigned long theCount = 0;
    unsigned long theScale = 1;
    if (this->mRunning && this->mPauseRemaining.QuadPart == 0) {
        __int64 theNow;
        if (this->mUseCounter)
            QueryPerformanceCounter((LARGE_INTEGER*)&theNow);
        else {
            theNow = timeGetTime();
            if (this->mTimeUnit == 1)
                theScale = 1000;
        }
        __int64 theStart = this->mStartTime.QuadPart;
        __int64 theElapsed = (theNow - theStart) / TMPoll::sCounterFrequency;
        theCount = (unsigned long)(theElapsed / this->mTimeDelay);
        if (theCount) {
            unsigned long theRemainder = (unsigned long)theElapsed - this->mTimeDelay * theCount;
            this->mStartTime.QuadPart = theStart + (theElapsed - theRemainder) * TMPoll::sCounterFrequency;
        }
    }
    return theScale * theCount;
}

// FUNCTION: LLE 0x10053a56
// Resumes a paused poll: the current period continues from the part that had elapsed when it was paused
void TMPoll::Resume()
{
    if (!(this->mPauseRemaining.QuadPart > 0) || !this->mRunning)
        return;
    // Fixed: the elapsed part used to be added to the start time (in counter mode without converting it to counter
    // ticks), so the current period started over; it is subtracted now
    if (this->mUseCounter) {
        QueryPerformanceCounter(&this->mStartTime);
        this->mStartTime.QuadPart -= this->mPauseRemaining.QuadPart * TMPoll::sCounterFrequency;
    } else
        this->mStartTime.QuadPart = timeGetTime() - this->mPauseRemaining.QuadPart;
    this->mPauseRemaining.QuadPart = 0;
}

// FUNCTION: LLE 0x10053aa2
// Starts the poll if it is not running, with the period beginning now
void TMPoll::Start()
{
    if (this->mRunning)
        return;
    this->mPauseRemaining.LowPart = 0;
    this->mRunning = true;
    this->mPauseRemaining.HighPart = 0;
    if (this->mUseCounter)
        QueryPerformanceCounter(&this->mStartTime);
    else
        this->mStartTime.QuadPart = timeGetTime();
}
