// C:\EEX\Low-Level Engine\Thread System\TSReadWriteLock.h (RECONSTRUCTION; TSReadWriteLock.cpp 0x53d8b..0x53e55,
// operator new(0x10) in TSScheduler::CreateReadWriteLock). Member names are provisional.
#ifndef LLE_TSREADWRITELOCK_H
#define LLE_TSREADWRITELOCK_H

#include "platform/PlatThread.h"

class TSCriticalSection;
class TSEvent;

class LLE_API TSReadWriteLock {
    friend class TSScheduler;
public:
    long GetReadLockCount() const { return this->mReaderCount; }  // exported, ICF-folded (0x4495)
    void ReadLock();
    void ReadUnlock();
    void WriteLock();
    void WriteUnlock();
private:
    TSReadWriteLock(TSCriticalSection* inWriteLock, TSEvent* inNoReaders, TSEvent* inReading);
    ~TSReadWriteLock();

    TSCriticalSection* mWriteLock;          // +0x00
    TSEvent* mNoReaders;                    // +0x04 auto-reset; set when the last reader leaves
    TSEvent* mReading;                      // +0x08 manual-reset; set while readers hold the lock
    volatile int32_t mReaderCount;          // +0x0c (PlatAtomicIncrement/Decrement)
};

#endif
