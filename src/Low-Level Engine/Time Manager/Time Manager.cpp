// C:\EEX\Low-Level Engine\Time Manager\Time Manager.cpp (folder and file name inferred: sorts between
// Thread System.cpp and TMPoll.cpp)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// Whole in Rust (rust/crates/ee_time, LLE lane round 3): the clocks, the time base and the pinned time; the exported
// names forward, and TMInitialize registers the Rust shutdown with the engine's list.
#include <windows.h>
#include <stdint.h>

#include "lle/LowLevelEngine.h"
#include "rust/ee_time.h"

// FUNCTION: LLE 0x10053701
// Returns the current time in milliseconds (performance counter if present, else the millisecond clock); a time
// pinned on this thread (EE-SIM's scripted editor, ee_time_pin) instead of either
unsigned long TMGetTime() { return ee_time_get(); }

// FUNCTION: LLE 0x1005373a
// Returns the current time in microseconds from the performance counter (milliseconds without one)
int64_t TMGetMicroTime() { return ee_time_get_micro(); }

// FUNCTION: LLE 0x1005376c
// Fills a time-of-day record from the local clock (month counted from 0)
void TMGetTimeOfDay(TMTime& outTime)
{
    EeTimeOfDay theTime;
    ee_time_of_day(&theTime);
    outTime.mMonth = theTime.mMonth;
    outTime.mDay = theTime.mDay;
    outTime.mYear = theTime.mYear;
    outTime.mHour = theTime.mHour;
    outTime.mMinute = theTime.mMinute;
    outTime.mSecond = theTime.mSecond;
}

// FUNCTION: LLE 0x100537ab
// Starts the time manager: 1 ms timer resolution, performance-counter frequencies, and its shutdown registration
long TMInitialize() { return ee_time_system_initialize(); }
