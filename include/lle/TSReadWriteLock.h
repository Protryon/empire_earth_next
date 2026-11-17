// C:\EEX\Low-Level Engine\Thread System\TSReadWriteLock.h (RECONSTRUCTION; TSReadWriteLock.cpp 0x53d8b..0x53e55,
// operator new(0x10) in TSScheduler::CreateReadWriteLock). Readers share, one writer excludes them: a writer
// section, an automatic-reset "no readers" event and a manual-reset "reading" event. Whole in Rust
// (rust/crates/ee_base/src/ts, LLE lane round 3): the class has no data; a TSReadWriteLock* is the Rust lock TSScheduler
// made, and every member forwards.
#ifndef LLE_TSREADWRITELOCK_H
#define LLE_TSREADWRITELOCK_H

#include "platform/PlatThread.h"
#include "rust/base/ts.h"

class LLE_API TSReadWriteLock {
    friend class TSScheduler;
public:
    long GetReadLockCount() const { return ee_ts_lock_read_count(this->Rust()); }
    void ReadLock() { ee_ts_lock_read(this->Rust()); }
    void ReadUnlock() { ee_ts_lock_read_unlock(this->Rust()); }
    void WriteLock() { ee_ts_lock_write(this->Rust()); }
    void WriteUnlock() { ee_ts_lock_write_unlock(this->Rust()); }
private:
    TSReadWriteLock() = delete;
    ~TSReadWriteLock() = delete;
    const EeTsLock* Rust() const { return reinterpret_cast<const EeTsLock*>(this); }
};

#endif
