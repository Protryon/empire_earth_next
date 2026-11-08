// Platform layer, POSIX: time (include/platform/PlatTime.h). The millisecond clock and the counter run on
// CLOCK_MONOTONIC (nanosecond ticks). System timers are served by one timer thread, started on first use, that
// keeps the running timers ordered by due time and calls each callback (or sets each event) on itself, as the
// Win32 multimedia timers do on their system thread. A periodic timer keeps its phase (the next due time is the last
// due time plus the interval, not the time it fired plus the interval), as timeSetEvent's periodic timers do.
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <sys/time.h>
#include <time.h>

#include <map>
#include <new>

#include "platform/PlatThread.h"
#include "platform/PlatTime.h"
#include "PlatPosix.h"

namespace {

uint64_t MonotonicNanoseconds()
{
    struct timespec theNow;
    clock_gettime(CLOCK_MONOTONIC, &theNow);
    return static_cast<uint64_t>(theNow.tv_sec) * 1000000000ULL + static_cast<uint64_t>(theNow.tv_nsec);
}

}   // namespace

extern "C" uint32_t PlatTimeMilliseconds(void)
{
    return static_cast<uint32_t>(MonotonicNanoseconds() / 1000000ULL);
}

extern "C" uint64_t PlatTimeMilliseconds64(void)
{
    return MonotonicNanoseconds() / 1000000ULL;
}

extern "C" int64_t PlatCounterFrequency(void)
{
    return 1000000000LL;
}

extern "C" int64_t PlatCounterTicks(void)
{
    return static_cast<int64_t>(MonotonicNanoseconds());
}

extern "C" void PlatSleep(uint32_t inMilliseconds)
{
    if (inMilliseconds == 0) {
        sched_yield();
        return;
    }
    struct timespec theLeft;
    theLeft.tv_sec = inMilliseconds / 1000;
    theLeft.tv_nsec = static_cast<long>(inMilliseconds % 1000) * 1000000L;
    while (nanosleep(&theLeft, &theLeft) != 0 && errno == EINTR) {
    }
}

extern "C" void PlatTimeBeginPeriod(uint32_t inMilliseconds)
{
    (void)inMilliseconds;   // the POSIX clocks and sleeps have no system-wide resolution setting
}

extern "C" void PlatTimeEndPeriod(uint32_t inMilliseconds)
{
    (void)inMilliseconds;
}

extern "C" void PlatGetLocalTime(PlatDateTime* outTime)
{
    struct timeval theNow;
    gettimeofday(&theNow, NULL);
    time_t theSeconds = theNow.tv_sec;
    struct tm theLocal;
    localtime_r(&theSeconds, &theLocal);
    outTime->mYear = static_cast<uint16_t>(theLocal.tm_year + 1900);
    outTime->mMonth = static_cast<uint16_t>(theLocal.tm_mon + 1);
    outTime->mDayOfWeek = static_cast<uint16_t>(theLocal.tm_wday);
    outTime->mDay = static_cast<uint16_t>(theLocal.tm_mday);
    outTime->mHour = static_cast<uint16_t>(theLocal.tm_hour);
    outTime->mMinute = static_cast<uint16_t>(theLocal.tm_min);
    outTime->mSecond = static_cast<uint16_t>(theLocal.tm_sec > 59 ? 59 : theLocal.tm_sec);   // no leap second
    outTime->mMilliseconds = static_cast<uint16_t>(theNow.tv_usec / 1000);
}

extern "C" uint64_t PlatTimeWallClock100ns(void)
{
    // the Unix epoch is 11644473600 seconds after 1601-01-01 (FILETIME's)
    struct timespec theNow;
    clock_gettime(CLOCK_REALTIME, &theNow);
    return (static_cast<uint64_t>(theNow.tv_sec) + 11644473600ULL) * 10000000ULL
        + static_cast<uint64_t>(theNow.tv_nsec) / 100ULL;
}

// ---- system timers

namespace {

struct Timer {
    uint64_t mDue;                  // ns, monotonic
    uint64_t mInterval;             // ns
    bool mPeriodic;
    PlatTimerCallback mCallback;    // or
    void* mUser;
    PlatEvent* mEvent;
};

pthread_mutex_t sTimerLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t sTimerCondition;
bool sTimerThreadStarted;
uint32_t sNextTimerID = 1;
std::map<uint32_t, Timer>* sTimers;   // never destroyed (timers may fire during exit)

void* TimerThread(void*)
{
    pthread_mutex_lock(&sTimerLock);
    for (;;) {
        // the earliest due timer
        uint64_t theNow = MonotonicNanoseconds();
        std::map<uint32_t, Timer>::iterator theNext = sTimers->end();
        for (std::map<uint32_t, Timer>::iterator it = sTimers->begin(); it != sTimers->end(); ++it)
            if (theNext == sTimers->end() || it->second.mDue < theNext->second.mDue)
                theNext = it;
        if (theNext == sTimers->end()) {
            pthread_cond_wait(&sTimerCondition, &sTimerLock);
            continue;
        }
        if (theNext->second.mDue > theNow) {
            uint64_t theWait = theNext->second.mDue - theNow;
#if defined(__APPLE__)
            struct timespec theRelative;
            theRelative.tv_sec = static_cast<time_t>(theWait / 1000000000ULL);
            theRelative.tv_nsec = static_cast<long>(theWait % 1000000000ULL);
            pthread_cond_timedwait_relative_np(&sTimerCondition, &sTimerLock, &theRelative);
#else
            uint64_t theDue = theNext->second.mDue;
            struct timespec theDeadline;
            theDeadline.tv_sec = static_cast<time_t>(theDue / 1000000000ULL);
            theDeadline.tv_nsec = static_cast<long>(theDue % 1000000000ULL);
            pthread_cond_timedwait(&sTimerCondition, &sTimerLock, &theDeadline);
#endif
            continue;   // timers may have been added or stopped meanwhile
        }
        Timer theTimer = theNext->second;
        if (theTimer.mPeriodic) {
            theNext->second.mDue += theTimer.mInterval;
            if (theNext->second.mDue < theNow)   // fell far behind (a suspended process): no burst of catch-ups
                theNext->second.mDue = theNow + theTimer.mInterval;
        } else {
            sTimers->erase(theNext);
        }
        pthread_mutex_unlock(&sTimerLock);
        if (theTimer.mCallback)
            theTimer.mCallback(theTimer.mUser);
        else
            PlatEventSet(theTimer.mEvent);
        pthread_mutex_lock(&sTimerLock);
    }
    return NULL;
}

uint32_t StartTimer(const Timer& inTimer)
{
    pthread_mutex_lock(&sTimerLock);
    if (!sTimers)
        sTimers = new (std::nothrow) std::map<uint32_t, Timer>;
    if (sTimers && !sTimerThreadStarted) {
        pthread_condattr_t theAttributes;
        pthread_condattr_init(&theAttributes);
#if !defined(__APPLE__)
        pthread_condattr_setclock(&theAttributes, CLOCK_MONOTONIC);
#endif
        pthread_cond_init(&sTimerCondition, &theAttributes);
        pthread_condattr_destroy(&theAttributes);
        pthread_t theThread;
        pthread_attr_t theThreadAttributes;
        pthread_attr_init(&theThreadAttributes);
        pthread_attr_setdetachstate(&theThreadAttributes, PTHREAD_CREATE_DETACHED);
        sTimerThreadStarted = pthread_create(&theThread, &theThreadAttributes, TimerThread, NULL) == 0;
        pthread_attr_destroy(&theThreadAttributes);
    }
    uint32_t theID = 0;
    if (sTimers && sTimerThreadStarted) {
        theID = sNextTimerID++;
        if (sNextTimerID == 0)
            sNextTimerID = 1;
        (*sTimers)[theID] = inTimer;
        pthread_cond_signal(&sTimerCondition);
    } else {
        PlatPosix::SetLastError(PlatPosix::kErrorNotEnoughMemory);
    }
    pthread_mutex_unlock(&sTimerLock);
    return theID;
}

}   // namespace

extern "C" uint32_t PlatTimerStartCallback(uint32_t inIntervalMs, int inPeriodic, PlatTimerCallback inCallback,
                                           void* inUser)
{
    if (!inCallback)
        return 0;
    Timer theTimer;
    theTimer.mInterval = static_cast<uint64_t>(inIntervalMs ? inIntervalMs : 1) * 1000000ULL;
    theTimer.mDue = MonotonicNanoseconds() + theTimer.mInterval;
    theTimer.mPeriodic = inPeriodic != 0;
    theTimer.mCallback = inCallback;
    theTimer.mUser = inUser;
    theTimer.mEvent = NULL;
    return StartTimer(theTimer);
}

extern "C" uint32_t PlatTimerStartEvent(uint32_t inIntervalMs, int inPeriodic, PlatEvent* inEvent)
{
    if (!inEvent)
        return 0;
    Timer theTimer;
    theTimer.mInterval = static_cast<uint64_t>(inIntervalMs ? inIntervalMs : 1) * 1000000ULL;
    theTimer.mDue = MonotonicNanoseconds() + theTimer.mInterval;
    theTimer.mPeriodic = inPeriodic != 0;
    theTimer.mCallback = NULL;
    theTimer.mUser = NULL;
    theTimer.mEvent = inEvent;
    return StartTimer(theTimer);
}

extern "C" int PlatTimerStop(uint32_t inTimerID)
{
    pthread_mutex_lock(&sTimerLock);
    int theResult = sTimers && sTimers->erase(inTimerID) ? 1 : 0;
    if (theResult)
        pthread_cond_signal(&sTimerCondition);
    pthread_mutex_unlock(&sTimerLock);
    return theResult;
}
