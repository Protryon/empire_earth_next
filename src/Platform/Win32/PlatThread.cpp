// Platform layer, Win32: threads and synchronisation (include/platform/PlatThread.h). A PlatMutex is a critical
// section; a PlatEvent* is the Win32 event HANDLE itself (PlatTimerStartEvent relies on it). The waits call
// WaitForSingleObject / WaitForMultipleObjects as the engine did, which the test harness's time shim hooks.
#include <windows.h>
#include <new>

#include "platform/PlatThread.h"

struct PlatMutex {
    CRITICAL_SECTION mSection;
};

static HANDLE EventHandle(PlatEvent* inEvent)
{
    return reinterpret_cast<HANDLE>(inEvent);
}

extern "C" uint32_t PlatGetLastError(void)
{
    return GetLastError();
}

extern "C" uint32_t PlatCurrentThreadId(void)
{
    return GetCurrentThreadId();
}

extern "C" PlatMutex* PlatMutexCreate(void)
{
    PlatMutex* theMutex = new (std::nothrow) PlatMutex;
    if (theMutex)
        InitializeCriticalSection(&theMutex->mSection);
    return theMutex;
}

extern "C" void PlatMutexDestroy(PlatMutex* inMutex)
{
    if (!inMutex)
        return;
    DeleteCriticalSection(&inMutex->mSection);
    delete inMutex;
}

extern "C" void PlatMutexLock(PlatMutex* inMutex)
{
    EnterCriticalSection(&inMutex->mSection);
}

extern "C" void PlatMutexUnlock(PlatMutex* inMutex)
{
    LeaveCriticalSection(&inMutex->mSection);
}

extern "C" PlatEvent* PlatEventCreate(int inManualReset, int inInitiallySet)
{
    return reinterpret_cast<PlatEvent*>(CreateEventA(NULL, inManualReset ? TRUE : FALSE,
                                                     inInitiallySet ? TRUE : FALSE, NULL));
}

extern "C" void PlatEventDestroy(PlatEvent* inEvent)
{
    CloseHandle(EventHandle(inEvent));
}

extern "C" int PlatEventSet(PlatEvent* inEvent)
{
    return SetEvent(EventHandle(inEvent)) ? 1 : 0;
}

extern "C" int PlatEventReset(PlatEvent* inEvent)
{
    return ResetEvent(EventHandle(inEvent)) ? 1 : 0;
}

static int32_t WaitResult(DWORD inResult, uint32_t inCount)
{
    if (inResult == WAIT_TIMEOUT)
        return PLAT_WAIT_TIMEOUT;
    if (inResult == WAIT_FAILED)
        return PLAT_WAIT_FAILED;
    if (inResult - WAIT_OBJECT_0 < inCount)
        return static_cast<int32_t>(inResult - WAIT_OBJECT_0);
    if (inResult - WAIT_ABANDONED_0 < inCount)   // mutexes only: an event is never abandoned
        return static_cast<int32_t>(inResult - WAIT_ABANDONED_0);
    return PLAT_WAIT_FAILED;
}

extern "C" int32_t PlatEventWait(PlatEvent* inEvent, uint32_t inTimeoutMs)
{
    return WaitResult(WaitForSingleObject(EventHandle(inEvent), inTimeoutMs), 1);
}

extern "C" int32_t PlatEventWaitAny(PlatEvent* const* inEvents, uint32_t inCount, uint32_t inTimeoutMs)
{
    HANDLE theHandles[PLAT_WAIT_MAX];
    if (inCount == 0 || inCount > PLAT_WAIT_MAX) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return PLAT_WAIT_FAILED;
    }
    for (uint32_t i = 0; i < inCount; i++)
        theHandles[i] = EventHandle(inEvents[i]);
    return WaitResult(WaitForMultipleObjects(inCount, theHandles, FALSE, inTimeoutMs), inCount);
}

extern "C" int32_t PlatAtomicIncrement(volatile int32_t* ioValue)
{
    return InterlockedIncrement(reinterpret_cast<volatile LONG*>(ioValue));
}

extern "C" int32_t PlatAtomicDecrement(volatile int32_t* ioValue)
{
    return InterlockedDecrement(reinterpret_cast<volatile LONG*>(ioValue));
}
