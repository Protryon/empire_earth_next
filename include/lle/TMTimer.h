// C:\EEX\Low-Level Engine\Time Manager\TMTimer.h (folder name inferred; RECONSTRUCTION from TMTimer.cpp
// 0x53ad3..0x53ca1). A system-timer wrapper (platform/PlatTime.h: the multimedia timers on Win32). Whole in Rust
// (rust/crates/ee_base/src/time, LLE lane round 3): the class owns the Rust timer and every member forwards.
#ifndef LLE_TMTIMER_H
#define LLE_TMTIMER_H
#include "lle/LLEForward.h"

#include "rust/base/time.h"

class LLE_API TMTimer {
public:
    TMTimer(unsigned long inInterval, unsigned long inTimeUnit, bool inRepeating)
        : mTimer(ee_time_timer_new((uint32_t)inInterval, (uint32_t)inTimeUnit, inRepeating)) {}
    ~TMTimer() { ee_time_timer_free(this->mTimer); }
    // the Rust timer (ee_game rts physics.rs makes every call on it: the callback, the event, the interval, Start, Stop)
    EeTimeTimer* Rust() const { return this->mTimer; }

    URust<EeTimeTimer> mTimer;
};

#endif
