// Platform layer, Win32: time (include/platform/PlatTime.h). The same Win32 calls the engine made itself before
// (timeGetTime, QueryPerformanceCounter/Frequency, Sleep, timeBeginPeriod, GetLocalTime, timeSetEvent); the test
// harness's time shim hooks those imports, so they stay the ones called.
#include <windows.h>
#include <mmsystem.h>
#include <map>
#include <new>

#include "platform/PlatTime.h"

extern "C" uint32_t PlatTimeMilliseconds(void)
{
    return timeGetTime();
}

extern "C" int64_t PlatCounterFrequency(void)
{
    LARGE_INTEGER theFrequency;
    if (QueryPerformanceFrequency(&theFrequency) != TRUE)
        return 0;
    return theFrequency.QuadPart;
}

extern "C" int64_t PlatCounterTicks(void)
{
    LARGE_INTEGER theCount;
    if (!QueryPerformanceCounter(&theCount))
        return 0;
    return theCount.QuadPart;
}

extern "C" void PlatSleep(uint32_t inMilliseconds)
{
    Sleep(inMilliseconds);
}

extern "C" void PlatTimeBeginPeriod(uint32_t inMilliseconds)
{
    timeBeginPeriod(inMilliseconds);
}

extern "C" void PlatTimeEndPeriod(uint32_t inMilliseconds)
{
    timeEndPeriod(inMilliseconds);
}

extern "C" void PlatGetLocalTime(PlatDateTime* outTime)
{
    SYSTEMTIME theTime;
    GetLocalTime(&theTime);
    outTime->mYear = theTime.wYear;
    outTime->mMonth = theTime.wMonth;
    outTime->mDayOfWeek = theTime.wDayOfWeek;
    outTime->mDay = theTime.wDay;
    outTime->mHour = theTime.wHour;
    outTime->mMinute = theTime.wMinute;
    outTime->mSecond = theTime.wSecond;
    outTime->mMilliseconds = theTime.wMilliseconds;
}

// ---- system timers ----
// An event timer is a plain TIME_CALLBACK_EVENT_SET timer on the event's handle (platform/PlatThread.h: on Win32 a
// PlatEvent* is the event HANDLE). A callback timer needs its callback and user pointer: they are kept by timer id
// in a table, which the multimedia-timer callback looks up by the id it is given (never through a pointer that a
// concurrent Stop may have freed). A one-shot timer's entry goes when it fires, a stopped timer's when it is killed.

namespace {

struct TimerEntry {
    PlatTimerCallback mCallback;
    void* mUser;
    bool mPeriodic;
};

SRWLOCK sTimerLock = SRWLOCK_INIT;
std::map<UINT, TimerEntry>* sTimers;   // created on first use; never destroyed (timers may fire during exit)

void CALLBACK TimerThunk(UINT inTimerID, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    TimerEntry theEntry;
    AcquireSRWLockExclusive(&sTimerLock);
    std::map<UINT, TimerEntry>::iterator it = sTimers->find(inTimerID);
    if (it == sTimers->end()) {
        ReleaseSRWLockExclusive(&sTimerLock);
        return;   // stopped meanwhile
    }
    theEntry = it->second;
    if (!theEntry.mPeriodic)
        sTimers->erase(it);
    ReleaseSRWLockExclusive(&sTimerLock);
    theEntry.mCallback(theEntry.mUser);
}

}   // namespace

extern "C" uint32_t PlatTimerStartCallback(uint32_t inIntervalMs, int inPeriodic, PlatTimerCallback inCallback,
                                           void* inUser)
{
    // the lock is held across timeSetEvent, so a timer that fires at once waits until its entry exists
    AcquireSRWLockExclusive(&sTimerLock);
    if (!sTimers)
        sTimers = new (std::nothrow) std::map<UINT, TimerEntry>;
    UINT theID = 0;
    if (sTimers) {
        theID = timeSetEvent(inIntervalMs, 0, TimerThunk, 0,
                             TIME_CALLBACK_FUNCTION | (inPeriodic ? TIME_PERIODIC : TIME_ONESHOT));
        if (theID) {
            TimerEntry theEntry = {inCallback, inUser, inPeriodic != 0};
            (*sTimers)[theID] = theEntry;
        }
    }
    ReleaseSRWLockExclusive(&sTimerLock);
    return theID;
}

extern "C" uint32_t PlatTimerStartEvent(uint32_t inIntervalMs, int inPeriodic, PlatEvent* inEvent)
{
    return timeSetEvent(inIntervalMs, 0, reinterpret_cast<LPTIMECALLBACK>(inEvent), 0,
                        TIME_CALLBACK_EVENT_SET | (inPeriodic ? TIME_PERIODIC : TIME_ONESHOT));
}

extern "C" int PlatTimerStop(uint32_t inTimerID)
{
    if (timeKillEvent(inTimerID) != TIMERR_NOERROR)
        return 0;
    AcquireSRWLockExclusive(&sTimerLock);
    if (sTimers)
        sTimers->erase(inTimerID);
    ReleaseSRWLockExclusive(&sTimerLock);
    return 1;
}
