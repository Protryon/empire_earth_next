// C:\EEX\Low-Level Engine\Thread System\TSScheduler.h (RECONSTRUCTION; TSScheduler.cpp 0x53e55..0x54203).
// Include <windows.h> first: CreateEvent is exported as ?CreateEventA@TSScheduler@@...
// Whole in Rust (rust/crates/ee_ts, LLE lane round 3): the statics forward (TSScheduler.cpp); the thread registry
// is the crate's SCHEDULER.
#ifndef LLE_TSSCHEDULER_H
#define LLE_TSSCHEDULER_H

#include "lle/TSCriticalSection.h"

class TSEvent;
class TSReadWriteLock;
class TSThread;

LLE_API long TSInitialize();   // Thread System.cpp

class LLE_API TSScheduler {
public:
    static long CreateCriticalSection(TSCriticalSection*& outSection);
    static void DestroyCriticalSection(TSCriticalSection* inSection);
    // defaults proven by TCP 0x287c: the assert text passes one argument, the call pushes 1
    static long CreateEvent(TSEvent*& outEvent, bool inManualReset = true);
    static void DestroyEvent(TSEvent* inEvent);
    static long CreateReadWriteLock(TSReadWriteLock*& outLock);
    static void DestroyReadWriteLock(TSReadWriteLock* inLock);
    // defaults proven by TCP 0x287c: the assert text passes three arguments, the call pushes 0, 0
    static long CreateThread(TSThread*& outThread, unsigned long (*inFunction)(void*), void* inParameter,
                             unsigned long inStackSize = 0, bool inSuspended = false);
    static long DestroyThread(TSThread* inThread);
    static TSThread* GetCurrentThread();
    static unsigned long GetCurrentThreadIndex();
};

#endif
