// C:\EEX\Low-Level Engine\Thread System\TSReadWriteLock.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngine.h"

// FUNCTION: LLE 0x10053d8b
// Creates a read/write lock from a writer lock and two events, with no readers
TSReadWriteLock::TSReadWriteLock(TSCriticalSection* inWriteLock, TSEvent* inNoReaders, TSEvent* inReading)
    : mWriteLock(inWriteLock), mNoReaders(inNoReaders), mReading(inReading)
{
    this->mNoReaders->Set();
    this->mReading->Clear();
    this->mReaderCount = 0;
}

// FUNCTION: LLE 0x10053db9
// Destroys the read/write lock and its writer lock and events
TSReadWriteLock::~TSReadWriteLock()
{
    TSScheduler::DestroyCriticalSection(this->mWriteLock);
    TSScheduler::DestroyEvent(this->mNoReaders);
    TSScheduler::DestroyEvent(this->mReading);
}

// FUNCTION: LLE 0x10053dd8
// Takes a read lock: the first reader waits until no writer holds the lock, then readers wait for reading to open
void TSReadWriteLock::ReadLock()
{
    if (PlatAtomicIncrement(&this->mReaderCount) == 1) {
        this->mNoReaders->Wait(kTSWaitInfinite);
        this->mReading->Set();
    }
    this->mReading->Wait(kTSWaitInfinite);
}

// FUNCTION: LLE 0x10053e08
// Releases a read lock: the last reader closes reading and lets writers in
void TSReadWriteLock::ReadUnlock()
{
    if (PlatAtomicDecrement(&this->mReaderCount) == 0) {
        this->mReading->Clear();
        this->mNoReaders->Set();
    }
}

// FUNCTION: LLE 0x10053e2b
// Takes the write lock: one writer at a time, waiting until no readers are left
void TSReadWriteLock::WriteLock()
{
    this->mWriteLock->Enter();
    this->mNoReaders->Wait(kTSWaitInfinite);
}

// FUNCTION: LLE 0x10053e41
// Releases the write lock and lets readers in
void TSReadWriteLock::WriteUnlock()
{
    this->mNoReaders->Set();
    this->mWriteLock->Leave();
}
