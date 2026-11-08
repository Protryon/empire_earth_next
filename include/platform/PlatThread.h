// Platform layer: threads and synchronisation (see platform/Platform.h).
#ifndef PLATFORM_PLATTHREAD_H
#define PLATFORM_PLATTHREAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The calling thread's last native error code (Win32: GetLastError), for the engine's error records
// (ESRecordNativeError). Only meaningful right after a platform call that reported a failure.
uint32_t PlatGetLastError(void);

// The calling thread's system id (Win32: GetCurrentThreadId). Never 0 for a running thread.
uint32_t PlatCurrentThreadId(void);

// A recursive mutex: the owning thread may lock it again; each lock needs its unlock (Win32: a critical section).
// Create -> NULL when out of memory.
typedef struct PlatMutex PlatMutex;
PlatMutex* PlatMutexCreate(void);
void PlatMutexDestroy(PlatMutex* inMutex);
void PlatMutexLock(PlatMutex* inMutex);
void PlatMutexUnlock(PlatMutex* inMutex);
int PlatMutexTryLock(PlatMutex* inMutex);   /* nonzero when locked (recursive, like Lock) */

// An event: manual reset (stays set until reset, releases every waiter) or automatic reset (releases one waiter and
// clears itself). Create -> NULL on failure (PlatGetLastError). Set/Reset -> nonzero on success.
typedef struct PlatEvent PlatEvent;
PlatEvent* PlatEventCreate(int inManualReset, int inInitiallySet);
void PlatEventDestroy(PlatEvent* inEvent);
int PlatEventSet(PlatEvent* inEvent);
int PlatEventReset(PlatEvent* inEvent);

// Waits. Timeouts are in milliseconds; PLAT_WAIT_INFINITE waits forever.
// PlatEventWait -> 0 when the event was set, PLAT_WAIT_TIMEOUT, or PLAT_WAIT_FAILED (PlatGetLastError).
// PlatEventWaitAny (at most PLAT_WAIT_MAX events) -> the index of a set event (the lowest when several are),
// PLAT_WAIT_TIMEOUT or PLAT_WAIT_FAILED. An automatic-reset event is cleared by the wait that it releases.
#define PLAT_WAIT_INFINITE 0xFFFFFFFFu
#define PLAT_WAIT_TIMEOUT (-1)
#define PLAT_WAIT_FAILED (-2)
#define PLAT_WAIT_MAX 64
int32_t PlatEventWait(PlatEvent* inEvent, uint32_t inTimeoutMs);
int32_t PlatEventWaitAny(PlatEvent* const* inEvents, uint32_t inCount, uint32_t inTimeoutMs);

// Threads. PlatThreadCreate starts inMain(inParameter) on a new thread with inStackSize bytes of stack (0: the
// default), held before its first instruction when inSuspended (PlatThreadResume starts it) -> its handle and id,
// NULL on failure. The thread ends when inMain returns; its result is the exit code. On Win32 every such thread has
// COM initialised around inMain (the renderer and the old DirectX code need it); when that fails the thread ends at
// once with exit code 0xFFFFFFFF. PlatThreadOpenCurrent -> a handle of the calling thread. PlatThreadClose releases
// a handle only: the thread runs on.
typedef struct PlatThread PlatThread;
typedef uint32_t (*PlatThreadMain)(void* inParameter);
PlatThread* PlatThreadCreate(PlatThreadMain inMain, void* inParameter, uint32_t inStackSize, int inSuspended,
                             uint32_t* outThreadID);
PlatThread* PlatThreadOpenCurrent(void);
void PlatThreadClose(PlatThread* inThread);
// The exit code, PLAT_THREAD_STILL_ACTIVE while the thread runs -> nonzero on success
#define PLAT_THREAD_STILL_ACTIVE 259u
int PlatThreadGetExitCode(PlatThread* inThread, uint32_t* outExitCode);
// Waits up to inTimeoutMs (PLAT_WAIT_INFINITE: forever) for the thread to end -> 0 when it has ended,
// PLAT_WAIT_TIMEOUT, or PLAT_WAIT_FAILED (PlatGetLastError). Win32: WaitForSingleObject on the thread handle.
int32_t PlatThreadWait(PlatThread* inThread, uint32_t inTimeoutMs);
// Scheduling priority, relative to the process (the Win32 THREAD_PRIORITY_* values); PlatThreadGetPriority ->
// PLAT_PRIORITY_ERROR on failure, PlatThreadSetPriority -> nonzero on success. Another system maps them as it can.
#define PLAT_PRIORITY_IDLE (-15)
#define PLAT_PRIORITY_LOWEST (-2)
#define PLAT_PRIORITY_BELOW_NORMAL (-1)
#define PLAT_PRIORITY_NORMAL 0
#define PLAT_PRIORITY_ABOVE_NORMAL 1
#define PLAT_PRIORITY_HIGHEST 2
#define PLAT_PRIORITY_TIME_CRITICAL 15
#define PLAT_PRIORITY_ERROR 0x7FFFFFFF
int32_t PlatThreadGetPriority(PlatThread* inThread);
int PlatThreadSetPriority(PlatThread* inThread, int32_t inPriority);
// Suspend / resume -> the suspend count before the call, 0xFFFFFFFF on failure. Terminate kills the thread (no
// cleanup) -> nonzero on success. These three exist for the engine's TSThread; a system without them may fail them.
uint32_t PlatThreadSuspend(PlatThread* inThread);
uint32_t PlatThreadResume(PlatThread* inThread);
int PlatThreadTerminate(PlatThread* inThread);

// Atomic increment and decrement of a 32-bit value -> the new value (a full memory barrier).
int32_t PlatAtomicIncrement(volatile int32_t* ioValue);
int32_t PlatAtomicDecrement(volatile int32_t* ioValue);

#ifdef __cplusplus
}
#endif

#endif
