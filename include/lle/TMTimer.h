// C:\EEX\Low-Level Engine\Time Manager\TMTimer.h (folder name inferred; RECONSTRUCTION from TMTimer.cpp
// 0x53ad3..0x53ca1). A system-timer wrapper (platform/PlatTime.h: the multimedia timers on Win32). Whole in Rust
// (rust/crates/ee_base/src/time, LLE lane round 3): the class owns the Rust timer and every member forwards.
#ifndef LLE_TMTIMER_H
#define LLE_TMTIMER_H

#include "rust/ee_time.h"
#include "lle/TSEvent.h"

class LLE_API TMTimer {
public:
    TMTimer(unsigned long inInterval, unsigned long inTimeUnit, bool inRepeating)
        : mTimer(ee_time_timer_new((uint32_t)inInterval, (uint32_t)inTimeUnit, inRepeating)) {}
    ~TMTimer() { ee_time_timer_free(this->mTimer); }
    long SetCallback(void (*inCallback)(void*), void* inParameter)
    {
        return ee_time_timer_set_callback(this->mTimer, inCallback, inParameter);
    }
    // the event's handle is read here (the original read it at Start; a NULL event crashed there)
    long SetEvent(TSEvent* inEvent)
    {
        return ee_time_timer_set_event(this->mTimer, inEvent != NULL ? inEvent->GetHandle() : NULL);
    }
    long SetInterval(unsigned long inInterval, unsigned long inTimeUnit)
    {
        return ee_time_timer_set_interval(this->mTimer, (uint32_t)inInterval, (uint32_t)inTimeUnit);
    }
    long SetRepeating(bool inRepeating) { return ee_time_timer_set_repeating(this->mTimer, inRepeating); }
    long SetTimerType(unsigned long inType) { return ee_time_timer_set_type(this->mTimer, (uint32_t)inType); }
    long Start() { return ee_time_timer_start(this->mTimer); }
    long Stop() { return ee_time_timer_stop(this->mTimer); }
    long Stop(unsigned long& outElapsed, unsigned long& outRemaining)
    {
        uint32_t theElapsed, theRemaining;
        long theResult = ee_time_timer_stop_elapsed(this->mTimer, &theElapsed, &theRemaining);
        if (theResult == kErrorOK) {
            outElapsed = theElapsed;
            outRemaining = theRemaining;
        }
        return theResult;
    }
private:
    TMTimer(const TMTimer&);
    TMTimer& operator=(const TMTimer&);

    EeTimeTimer* mTimer;
};

#endif
