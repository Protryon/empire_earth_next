// C:\EEX\Low-Level Engine\Thread System\TSScheduler.h (RECONSTRUCTION; TSScheduler.cpp 0x53e55..0x54203).
// Include <windows.h> first: CreateEvent is exported as ?CreateEventA@TSScheduler@@...
// Whole in Rust (rust/crates/ee_ts, LLE lane round 3): the statics forward (inline since round 7: TSScheduler.cpp
// and Thread System.cpp went); the thread registry is the crate's SCHEDULER.
#ifndef LLE_TSSCHEDULER_H
#define LLE_TSSCHEDULER_H

#include "lle/TSCriticalSection.h"

class TSEvent;
class TSReadWriteLock;
class TSThread;

// Starts the thread system: registers its shutdown (destroys the calling thread's object), then the calling (main)
// thread as a thread object (Thread System.cpp in the original)
inline long TSInitialize() { return ee_ts_system_initialize(); }

// Whole in Rust (ee_ts): every static forwards (TSScheduler.cpp in the original)
class LLE_API TSScheduler {
public:
    // a critical section (a NULL platform mutex fails its verification)
    static long CreateCriticalSection(TSCriticalSection*& outSection)
    {
        outSection = reinterpret_cast<TSCriticalSection*>(ee_ts_section_new());
        return kErrorOK;
    }
    // NULL is ignored
    static void DestroyCriticalSection(TSCriticalSection* inSection)
        { ee_ts_section_free(reinterpret_cast<EeTsSection*>(inSection)); }
    // an event (manual or auto reset); the native error recorded when it fails (outEvent is then unchanged).
    // Defaults proven by TCP 0x287c: the assert text passes one argument, the call pushes 1
    static long CreateEvent(TSEvent*& outEvent, bool inManualReset = true)
        { return ee_ts_event_new(reinterpret_cast<EeTsEvent**>(&outEvent), inManualReset); }
    static void DestroyEvent(TSEvent* inEvent) { ee_ts_event_free(reinterpret_cast<EeTsEvent*>(inEvent)); }
    // a write section and two events, the parts made so far destroyed on failure
    static long CreateReadWriteLock(TSReadWriteLock*& outLock)
        { return ee_ts_lock_new(reinterpret_cast<EeTsLock**>(&outLock)); }
    static void DestroyReadWriteLock(TSReadWriteLock* inLock) { ee_ts_lock_free(reinterpret_cast<EeTsLock*>(inLock)); }
    // creates, registers and starts a thread object for a function.
    // Defaults proven by TCP 0x287c: the assert text passes three arguments, the call pushes 0, 0
    static long CreateThread(TSThread*& outThread, unsigned long (*inFunction)(void*), void* inParameter,
                             unsigned long inStackSize = 0, bool inSuspended = false)
    {
        return ee_ts_thread_new(reinterpret_cast<EeTsThread**>(&outThread), inFunction, inParameter,
                                (uint32_t)inStackSize, inSuspended);
    }
    // unregisters and deletes a thread object; kErrorNotFound if it is not registered (NULL is ignored)
    static long DestroyThread(TSThread* inThread) { return ee_ts_thread_free(reinterpret_cast<EeTsThread*>(inThread)); }
    // the calling thread's registered object, or NULL
    static TSThread* GetCurrentThread() { return reinterpret_cast<TSThread*>(ee_ts_thread_current()); }
    // the calling thread's registry index, or -1 if it is not registered
    static unsigned long GetCurrentThreadIndex()
    {
        uint32_t theIndex = ee_ts_thread_current_index();
        return theIndex == 0xFFFFFFFFu ? (unsigned long)-1 : theIndex;
    }
};

#endif
