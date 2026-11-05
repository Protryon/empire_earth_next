// C:\EEX\Low-Level Engine\Thread System\TSScheduler.h (RECONSTRUCTION; TSScheduler.cpp 0x53e55..0x54203).
// Include <windows.h> first: CreateEvent is exported as ?CreateEventA@TSScheduler@@...
#ifndef LLE_TSSCHEDULER_H
#define LLE_TSSCHEDULER_H

#include <vector>
#include "lle/TSCriticalSection.h"

// Only declared here: TSEvent, TSReadWriteLock and TSThread have exported inline members, which the original's first
// includer (NELagLossSimulator.cpp) emits; Error System.cpp uses the scheduler without seeing them.
// lle/LowLevelEngine.h includes their headers before this one.
class TSEvent;
class TSReadWriteLock;
class TSThread;

LLE_API long TSInitialize();   // Thread System.cpp

class LLE_API TSScheduler {
    friend LLE_API long TSInitialize();           // registers the main thread through the private CreateThread
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
private:
    static long CreateThread(PlatThread* inHandle, unsigned long inThreadID, bool inSuspended);

    // Inferred: visible to the engine only. A static data member consumes a VC6 `$E` number in every unit that
    // sees the class, and the plug-ins that use TSScheduler (tcp, mixer) number their initializers as if they saw
    // none (see LowLevelEngine.h, Timing).
#ifdef LOWLEVELENGINE_EXPORTS
    static std::vector<TSThread*> sThreads;
#endif
};

#endif
