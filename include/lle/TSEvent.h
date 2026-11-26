// C:\EEX\Low-Level Engine\Thread System\TSEvent.h (RECONSTRUCTION; TSEvent.cpp 0x53cee..0x53d8b,
// TSScheduler::CreateEvent operator new(8)). A platform event, manual or automatic reset. Whole in Rust
// (rust/crates/ee_base/src/ts, LLE lane round 3): the class has no data; a TSEvent* is the Rust event TSScheduler made, and
// every member forwards.
#ifndef LLE_TSEVENT_H
#define LLE_TSEVENT_H
#include "lle/LLEForward.h"

#include "platform/PlatThread.h"
#include "rust/base/ts.h"

// TSEvent::Wait / WaitAny timeouts are milliseconds; this one waits forever (Win32 INFINITE)
const unsigned long kTSWaitInfinite = PLAT_WAIT_INFINITE;
// TSEvent::WaitAny results besides the index of the event that was set
enum TSWaitResult {
    kTSWaitTimeout = PLAT_WAIT_TIMEOUT,
    kTSWaitFailed = PLAT_WAIT_FAILED,
};

class LLE_API TSEvent : URustView {
public:
    long Set() { return ee_ts_event_set(this->Rust()); }
    long Clear() { return ee_ts_event_clear(this->Rust()); }
    long Wait(unsigned long inTimeout) { return ee_ts_event_wait(this->Rust(), (uint32_t)inTimeout); }
    operator void*() const { return ee_ts_event_handle(this->Rust()); }
    // the platform event (TMTimer's event timers set it)
    PlatEvent* GetHandle() const { return static_cast<PlatEvent*>(ee_ts_event_handle(this->Rust())); }
    const EeTsEvent* Rust() const { return reinterpret_cast<const EeTsEvent*>(this); }
};

#endif
