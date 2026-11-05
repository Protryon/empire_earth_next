// C:\EEX\Low-Level Engine\Time Manager\Time Manager.cpp (folder and file name inferred: sorts between
// Thread System.cpp and TMPoll.cpp)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The clocks come from the platform layer (platform/PlatTime.h: the performance counter and timeGetTime on Win32).
#include <windows.h>

#include "lle/LowLevelEngine.h"

static void TMShutdown(bool inForced);

// Globals in address order (provisional names)
int64_t gCounterFrequency;                  // 0x12ded8 counter ticks per millisecond
float gMicrosecondFrequency;                // 0x12dee0 counter ticks per microsecond
bool gTimeManagerInitialized;               // 0x12dee4
bool gHasPerformanceCounter;                // 0x12dee5

// FUNCTION: LLE 0x10053701
// Returns the current time in milliseconds (performance counter if present, else the millisecond clock)
unsigned long TMGetTime()
{
    if (gHasPerformanceCounter)
        return (unsigned long)(PlatCounterTicks() / gCounterFrequency);
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
