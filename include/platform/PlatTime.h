// Platform layer: time (see platform/Platform.h).
#ifndef PLATFORM_PLATTIME_H
#define PLATFORM_PLATTIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlatEvent;

// Milliseconds of the system's millisecond clock (Win32: timeGetTime, 1 ms resolution once PlatTimeBeginPeriod(1)
// is in force). Monotonic, arbitrary epoch, wraps every 2^32 ms: compare times by unsigned difference.
uint32_t PlatTimeMilliseconds(void);

// Milliseconds since an arbitrary epoch on a 64-bit clock that does not wrap (Win32: GetTickCount64, which the test
// harness's time shim leaves at real time: for timeouts that must not shrink when the game's clock runs faster).
uint64_t PlatTimeMilliseconds64(void);

// The high-resolution counter: its ticks per second, 0 when the system has none (Win32: QueryPerformanceFrequency),
// and its current value (QueryPerformanceCounter; 0 when there is none).
int64_t PlatCounterFrequency(void);
int64_t PlatCounterTicks(void);

// Gives up the processor for at least inMilliseconds (0: yield the rest of the time slice).
void PlatSleep(uint32_t inMilliseconds);

// Requests a system timer resolution of inMilliseconds for the millisecond clock, sleeps and waits, and ends the
// request (Win32: timeBeginPeriod / timeEndPeriod; pair them). A no-op where the system has no such setting.
void PlatTimeBeginPeriod(uint32_t inMilliseconds);
void PlatTimeEndPeriod(uint32_t inMilliseconds);

// The local wall-clock time (Win32: GetLocalTime). mMonth counts from 1, mDayOfWeek from Sunday = 0.
typedef struct PlatDateTime {
    uint16_t mYear;
    uint16_t mMonth;
    uint16_t mDayOfWeek;
    uint16_t mDay;
    uint16_t mHour;
    uint16_t mMinute;
    uint16_t mSecond;
    uint16_t mMilliseconds;
} PlatDateTime;
void PlatGetLocalTime(PlatDateTime* outTime);

// The UTC wall-clock time in 100-nanosecond units since 1601-01-01 (a Windows FILETIME; Win32:
// GetSystemTimeAsFileTime). Follows the system clock: it may jump when the clock is set.
uint64_t PlatTimeWallClock100ns(void);

// System timers (Win32: the multimedia timers, timeSetEvent). A timer fires once, or every inIntervalMs when
// inPeriodic is nonzero, on a system thread. A callback timer calls inCallback(inUser) there; an event timer sets
// inEvent (platform/PlatThread.h) instead. -> the timer id, 0 on failure. PlatTimerStop stops a running timer ->
// nonzero on success, 0 when the id is not (or no longer) a running timer: a one-shot timer that has fired is gone.
typedef void (*PlatTimerCallback)(void* inUser);
uint32_t PlatTimerStartCallback(uint32_t inIntervalMs, int inPeriodic, PlatTimerCallback inCallback, void* inUser);
uint32_t PlatTimerStartEvent(uint32_t inIntervalMs, int inPeriodic, struct PlatEvent* inEvent);
int PlatTimerStop(uint32_t inTimerID);

#ifdef __cplusplus
}
#endif

#endif
