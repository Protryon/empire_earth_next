// C:\EEX\Low-Level Engine\Thread System\TSCriticalSection.h (RECONSTRUCTION; the engine's own TSCriticalSection.cpp
// 0x53ca1..0x53cee and TSScheduler::CreateCriticalSection operator new(0x20)). A recursive lock with its owner and
// count. Whole in Rust (rust/crates/ee_base/src/ts, LLE lane round 3): the class has no data; a TSCriticalSection* is the
// Rust section TSScheduler made, and every member forwards.
#ifndef LLE_TSCRITICALSECTION_H
#define LLE_TSCRITICALSECTION_H

#include "platform/PlatThread.h"
#include "rust/base/ts.h"

class LLE_API TSCriticalSection {
    friend class TSScheduler;               // only the scheduler creates/destroys
public:
    void Enter() { ee_ts_section_enter(this->Rust()); }
    void Leave() { ee_ts_section_leave(this->Rust()); }
    bool TryEnter() { return ee_ts_section_try_enter(this->Rust()); }  // port: Enter without waiting (the harness)
private:
    TSCriticalSection() = delete;
    ~TSCriticalSection() = delete;
    const EeTsSection* Rust() const { return reinterpret_cast<const EeTsSection*>(this); }
};

#endif
