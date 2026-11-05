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

// Atomic increment and decrement of a 32-bit value -> the new value (a full memory barrier).
int32_t PlatAtomicIncrement(volatile int32_t* ioValue);
int32_t PlatAtomicDecrement(volatile int32_t* ioValue);

#ifdef __cplusplus
}
#endif

#endif
