// C:\EEX\Low-Level Engine\Thread System\TSScheduler.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// Whole in Rust (rust/crates/ee_ts, LLE lane round 3): the objects are Rust's, the registry is the crate's SCHEDULER.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "rust/ee_ts.h"

// FUNCTION: LLE 0x10053e9e
// Creates a critical section (a NULL platform mutex fails its verification)
long TSScheduler::CreateCriticalSection(TSCriticalSection*& outSection)
{
    outSection = reinterpret_cast<TSCriticalSection*>(ee_ts_section_new());
    return kErrorOK;
}

// FUNCTION: LLE 0x10053ec6
// Creates an event (manual or auto reset); records the native error if creation fails (outEvent is then unchanged)
long TSScheduler::CreateEvent(TSEvent*& outEvent, bool inManualReset)
{
    return ee_ts_event_new(reinterpret_cast<EeTsEvent**>(&outEvent), inManualReset);
}

// FUNCTION: LLE 0x10053f26
// Creates a read/write lock from a write critical section and two events, destroying the parts made so far on failure
long TSScheduler::CreateReadWriteLock(TSReadWriteLock*& outLock)
{
    return ee_ts_lock_new(reinterpret_cast<EeTsLock**>(&outLock));
}

// FUNCTION: LLE 0x10053fdc
// Creates, registers and starts a thread object for a function
long TSScheduler::CreateThread(TSThread*& outThread, unsigned long (*inFunction)(void*), void* inParameter,
                               unsigned long inStackSize, bool inSuspended)
{
    return ee_ts_thread_new(reinterpret_cast<EeTsThread**>(&outThread), inFunction, inParameter,
                            (uint32_t)inStackSize, inSuspended);
}

// FUNCTION: LLE 0x1005406f
// Destroys a critical section (NULL is ignored)
void TSScheduler::DestroyCriticalSection(TSCriticalSection* inSection)
{
    ee_ts_section_free(reinterpret_cast<EeTsSection*>(inSection));
}

// FUNCTION: LLE 0x10054088
// Destroys an event (NULL is ignored)
void TSScheduler::DestroyEvent(TSEvent* inEvent) { ee_ts_event_free(reinterpret_cast<EeTsEvent*>(inEvent)); }

// FUNCTION: LLE 0x100540a1
// Destroys a read/write lock (NULL is ignored)
void TSScheduler::DestroyReadWriteLock(TSReadWriteLock* inLock) { ee_ts_lock_free(reinterpret_cast<EeTsLock*>(inLock)); }

// FUNCTION: LLE 0x100540ba
// Unregisters and deletes a thread object; kErrorNotFound if it is not registered (NULL is ignored)
long TSScheduler::DestroyThread(TSThread* inThread)
{
    return ee_ts_thread_free(reinterpret_cast<EeTsThread*>(inThread));
}

// FUNCTION: LLE 0x10054142
// Returns the registered thread object of the calling thread, or NULL
TSThread* TSScheduler::GetCurrentThread() { return reinterpret_cast<TSThread*>(ee_ts_thread_current()); }

// FUNCTION: LLE 0x1005417a
// Returns the registry index of the calling thread, or -1 if it is not registered
unsigned long TSScheduler::GetCurrentThreadIndex()
{
    uint32_t theIndex = ee_ts_thread_current_index();
    return theIndex == 0xFFFFFFFFu ? (unsigned long)-1 : theIndex;
}
