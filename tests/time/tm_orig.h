// The original Time Manager (include/lle/TMPoll.h, include/lle/TMTimer.h and src/Low-Level Engine/Time Manager/
// TMPoll.cpp, TMTimer.cpp, Time Manager.cpp at a1775ddf, before they moved to rust/crates/ee_time), unchanged but
// for the includes, for the golden driver tm_golden.cpp (which supplies the platform clock and timers, TSEvent and
// URegisterShutdown).
// C:\EEX\Low-Level Engine\Time Manager\TMPoll.h (folder name inferred; RECONSTRUCTION from TMPoll.cpp
// 0x53819..0x53ad3). Inline members (the UI expands them) are emitted first in TMPoll.cpp. A polled countdown on
// the platform's high-resolution counter (or its millisecond clock when there is none; platform/PlatTime.h).
#ifndef LLE_TMPOLL_H
#define LLE_TMPOLL_H

class LLE_API TMPoll {
public:
    TMPoll(unsigned long inTimeDelay, unsigned long inTimeUnit, bool inStart);
    ~TMPoll() {}                                                                    // exported, ICF-folded at 0xc16a; UIHotKey's sPressedPoll registers a guard-only atexit dtor
    bool GetIsPaused() const { return this->mRunning && this->mPauseRemaining > 0; }  // 0x53819
    bool GetIsRunning() const { return this->mRunning; }                            // 0x53835
    unsigned long GetTimeLeft() const;
    void SetTimeDelay(unsigned long inTimeDelay) { this->mTimeDelay = inTimeDelay > 0 ? inTimeDelay : 1; }  // 0x53839
    void Start();
    void Stop() { this->mRunning = false; }                                         // 0x5384a
    void Pause();
    void Resume();
    unsigned long Poll();
private:
    int64_t mStartTime;                     // +0x00 counter ticks (or millisecond-clock ms; the original stored the halves separately)
    int64_t mPauseRemaining;                // +0x08 time left when paused (0: not paused)
    unsigned long mTimeUnit;                // +0x10 0: milliseconds, 1: microseconds (ctor: divisor 1000 / 1000000)
    unsigned long mTimeDelay;               // +0x14
    bool mUseCounter;                       // +0x18 the system has a high-resolution counter
    bool mRunning;                          // +0x19

    static int64_t sCounterFrequency;       // exported ?sCounterFrequency@TMPoll@@0_JA: ticks per time unit
};

#endif
// C:\EEX\Low-Level Engine\Time Manager\TMTimer.h (folder name inferred; RECONSTRUCTION from TMTimer.cpp
// 0x53ad3..0x53ca1; sizeof 0x24 from the implicit operator= `rep movsd` x9). A system-timer wrapper (platform/PlatTime.h:
// the multimedia timers on Win32).
#ifndef LLE_TMTIMER_H
#define LLE_TMTIMER_H

class TSEvent;

class LLE_API TMTimer {
public:
    TMTimer(unsigned long inInterval, unsigned long inTimeUnit, bool inRepeating);
    ~TMTimer();
    long SetCallback(void (*inCallback)(void*), void* inParameter);
    long SetEvent(TSEvent* inEvent);
    long SetInterval(unsigned long inInterval, unsigned long inTimeUnit);
    long SetRepeating(bool inRepeating);
    long SetTimerType(unsigned long inType);
    long Start();
    long Stop();
    long Stop(unsigned long& outElapsed, unsigned long& outRemaining);
private:
    static void TimerCallback(void* inUser);

    void (*mCallback)(void*);               // +0x00
    void* mParameter;                       // +0x04
    TSEvent* mEvent;                        // +0x08 signalled instead of the callback when mTimerType != 0
    unsigned long mTimerType;               // +0x0c 0: callback, else event
    unsigned long mUnknown10;               // +0x10 zeroed by the ctor and SetInterval only; (provisional name)
    unsigned long mInterval;                // +0x14 milliseconds
    unsigned long mStartTime;               // +0x18 the millisecond clock at Start / each period
    unsigned long mTimerID;                 // +0x1c the platform timer's id
    bool mRepeating;                        // +0x20
    bool mRunning;                          // +0x21
};

#endif
// C:\EEX\Low-Level Engine\Time Manager\TMPoll.cpp (folder name inferred)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS


int64_t TMPoll::sCounterFrequency;

// TMPoll.h's inline members, emitted here (first includer):
// SYNTHETIC: LLE 0x10053819
// SYNTHETIC: LLE 0x10053835
// SYNTHETIC: LLE 0x10053839
// SYNTHETIC: LLE 0x1005384a

// FUNCTION: LLE 0x1005384f
// Creates a poll that fires every delay (ms, or us for unit 1), using the performance counter when there is one
TMPoll::TMPoll(unsigned long inTimeDelay, unsigned long inTimeUnit, bool inStart)
{
    long theDivisor = inTimeUnit ? 1000000 : 1000;
    int64_t theFrequency = PlatCounterFrequency();
    if (theFrequency) {
        this->mUseCounter = true;
        TMPoll::sCounterFrequency = theFrequency / theDivisor;
        this->mStartTime = PlatCounterTicks();
    } else {
        // Fixed: the delay used to become inTimeDelay / divisor / 1000, i.e. 0 for any realistic delay, and Poll
        // then divided by zero; timeGetTime counts milliseconds, so only a microsecond delay is converted
        if (inTimeUnit)
            inTimeDelay /= 1000;
        if (inTimeDelay == 0)
            inTimeDelay = 1;
        TMPoll::sCounterFrequency = 1;
        this->mStartTime = PlatTimeMilliseconds();
        this->mUseCounter = false;
    }
    this->mPauseRemaining = 0;
    this->mTimeUnit = inTimeUnit;
    this->mRunning = false;
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
        int64_t theNow;
        if (this->mUseCounter)
            theNow = PlatCounterTicks();
        else {
            theNow = PlatTimeMilliseconds();
            if (this->mTimeUnit == 1)
                theScale = 1000;
        }
        int64_t theElapsed = (theNow - this->mStartTime) / TMPoll::sCounterFrequency;
        if (theElapsed < this->mTimeDelay)
            theTimeLeft = this->mTimeDelay - (unsigned long)theElapsed;
    }
    return theScale * theTimeLeft;
}

// FUNCTION: LLE 0x10053974
// Pauses the poll, remembering the time already elapsed in the current period
void TMPoll::Pause()
{
    this->mPauseRemaining = (uint32_t)(this->mTimeDelay - this->GetTimeLeft());
}

// FUNCTION: LLE 0x1005398a
// Returns the whole periods passed since the last poll and advances the start by them (0 if stopped or paused)
unsigned long TMPoll::Poll()
{
    unsigned long theCount = 0;
    unsigned long theScale = 1;
    if (this->mRunning && this->mPauseRemaining == 0) {
        int64_t theNow;
        if (this->mUseCounter)
            theNow = PlatCounterTicks();
        else {
            theNow = PlatTimeMilliseconds();
            if (this->mTimeUnit == 1)
                theScale = 1000;
        }
        int64_t theStart = this->mStartTime;
        int64_t theElapsed = (theNow - theStart) / TMPoll::sCounterFrequency;
        theCount = (unsigned long)(theElapsed / this->mTimeDelay);
        if (theCount) {
            unsigned long theRemainder = (unsigned long)theElapsed - this->mTimeDelay * theCount;
            this->mStartTime = theStart + (theElapsed - theRemainder) * TMPoll::sCounterFrequency;
        }
    }
    return theScale * theCount;
}

// FUNCTION: LLE 0x10053a56
// Resumes a paused poll: the current period continues from the part that had elapsed when it was paused
void TMPoll::Resume()
{
    if (!(this->mPauseRemaining > 0) || !this->mRunning)
        return;
    // Fixed: the elapsed part used to be added to the start time (in counter mode without converting it to counter
    // ticks), so the current period started over; it is subtracted now
    if (this->mUseCounter) {
        this->mStartTime = PlatCounterTicks();
        this->mStartTime -= this->mPauseRemaining * TMPoll::sCounterFrequency;
    } else
        this->mStartTime = (int64_t)PlatTimeMilliseconds() - this->mPauseRemaining;
    this->mPauseRemaining = 0;
}

// FUNCTION: LLE 0x10053aa2
// Starts the poll if it is not running, with the period beginning now
void TMPoll::Start()
{
    if (this->mRunning)
        return;
    this->mPauseRemaining = 0;
    this->mRunning = true;
    if (this->mUseCounter)
        this->mStartTime = PlatCounterTicks();
    else
        this->mStartTime = PlatTimeMilliseconds();
}
// C:\EEX\Low-Level Engine\Time Manager\TMTimer.cpp (folder name inferred)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS


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
// C:\EEX\Low-Level Engine\Time Manager\Time Manager.cpp (folder and file name inferred: sorts between
// Thread System.cpp and TMPoll.cpp)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The clocks come from the platform layer (platform/PlatTime.h: the performance counter and timeGetTime on Win32).


static void TMShutdown(bool inForced);

// Globals in address order (provisional names)
int64_t gCounterFrequency;                  // 0x12ded8 counter ticks per millisecond
float gMicrosecondFrequency;                // 0x12dee0 counter ticks per microsecond
bool gTimeManagerInitialized;               // 0x12dee4
bool gHasPerformanceCounter;                // 0x12dee5
thread_local long long gTMPinnedTime = -1;  // TEST ONLY (port): EE-SIM's scripted editor (EESimEditor.cpp)

// FUNCTION: LLE 0x10053701
// Returns the current time in milliseconds (performance counter if present, else the millisecond clock); a time
// pinned on this thread (EE-SIM's scripted editor) instead of either
unsigned long TMGetTime()
{
    if (gTMPinnedTime >= 0)
        return (unsigned long)gTMPinnedTime;
    if (gHasPerformanceCounter)
        // 32-bit milliseconds on every data model: the times go into 32-bit message fields and are subtracted from
        // them (LP64 kept the full count, which passes 2^32 after 49.7 days of uptime; macOS counts sleep too)
        return (uint32_t)(PlatCounterTicks() / gCounterFrequency);
    return PlatTimeMilliseconds();
}

// FUNCTION: LLE 0x1005373a
// Returns the current time in microseconds from the performance counter (milliseconds from the millisecond clock
// without one)
int64_t TMGetMicroTime()
{
    if (gHasPerformanceCounter)
        return (int64_t)(PlatCounterTicks() / gMicrosecondFrequency);
    return PlatTimeMilliseconds();
}

// FUNCTION: LLE 0x1005376c
// Fills a time-of-day record from the local clock (month counted from 0)
void TMGetTimeOfDay(TMTime& outTime)
{
    PlatDateTime theTime;
    PlatGetLocalTime(&theTime);
    outTime.mSecond = theTime.mSecond;
    outTime.mMinute = theTime.mMinute;
    outTime.mHour = theTime.mHour;
    outTime.mDay = theTime.mDay;
    outTime.mMonth = theTime.mMonth - 1;
    outTime.mYear = theTime.mYear;
}

// FUNCTION: LLE 0x100537ab
// Starts the time manager: 1 ms timer resolution, performance-counter frequencies, and its shutdown registration
long TMInitialize()
{
    PlatTimeBeginPeriod(1);
    gTimeManagerInitialized = true;
    int64_t theFrequency = PlatCounterFrequency();
    gHasPerformanceCounter = theFrequency != 0;
    if (gHasPerformanceCounter) {
        gCounterFrequency = theFrequency / 1000;
        gMicrosecondFrequency = theFrequency * 0.000001f;
    }
    return URegisterShutdown(TMShutdown);
}

// FUNCTION: LLE 0x10053810
// Shuts the time manager down, restoring the timer resolution
static void TMShutdown(bool inForced)
{
    PlatTimeEndPeriod(1);
}
