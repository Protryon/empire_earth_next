// C:\EEX\Low-Level Engine\Thread System\TSScheduler.h (RECONSTRUCTION; TSScheduler.cpp 0x53e55..0x54203).
// Include <windows.h> first: CreateEvent is exported as ?CreateEventA@TSScheduler@@...
// Whole in Rust (rust/crates/ee_base/src/ts, LLE lane round 3): the statics forward (inline since round 7: TSScheduler.cpp
// and Thread System.cpp went); the thread registry is the crate's SCHEDULER.
#ifndef LLE_TSSCHEDULER_H
#define LLE_TSSCHEDULER_H

#include "lle/TSCriticalSection.h"   // (the game includes it alone too)

#include "lle/LLEForward.h"
#include "platform/PlatThread.h"
#include "rust/base/ts.h"

// C:\EEX\Low-Level Engine\Thread System\TSEvent.h (RECONSTRUCTION; TSEvent.cpp 0x53cee..0x53d8b,
// TSScheduler::CreateEvent operator new(8)). A platform event, manual or automatic reset. Whole in Rust
// (rust/crates/ee_base/src/ts, LLE lane round 3): the class has no data; a TSEvent* is the Rust event TSScheduler made, and
// every member forwards.
// TSEvent::Wait / WaitAny timeouts are milliseconds; this one waits forever (Win32 INFINITE)
const unsigned long kTSWaitInfinite = PLAT_WAIT_INFINITE;
// TSEvent::WaitAny results besides the index of the event that was set
enum TSWaitResult {
    kTSWaitTimeout = PLAT_WAIT_TIMEOUT,
    kTSWaitFailed = PLAT_WAIT_FAILED,
};

class LLE_API TSEvent : URustView {
public:
    long Set() { return ee_ts_event_set(this->Rust()); }
    long Clear() { return ee_ts_event_clear(this->Rust()); }
    long Wait(unsigned long inTimeout) { return ee_ts_event_wait(this->Rust(), (uint32_t)inTimeout); }
    operator void*() const { return ee_ts_event_handle(this->Rust()); }
    // the platform event (TMTimer's event timers set it)
    PlatEvent* GetHandle() const { return static_cast<PlatEvent*>(ee_ts_event_handle(this->Rust())); }
    const EeTsEvent* Rust() const { return reinterpret_cast<const EeTsEvent*>(this); }
};

// C:\EEX\Low-Level Engine\Thread System\TSReadWriteLock.h (RECONSTRUCTION; TSReadWriteLock.cpp 0x53d8b..0x53e55,
// operator new(0x10) in TSScheduler::CreateReadWriteLock). Readers share, one writer excludes them: a writer
// section, an automatic-reset "no readers" event and a manual-reset "reading" event. Whole in Rust
// (rust/crates/ee_base/src/ts, LLE lane round 3): the class has no data; a TSReadWriteLock* is the Rust lock TSScheduler
// made, and every member forwards.
class LLE_API TSReadWriteLock : URustView {
public:
    void ReadLock() { ee_ts_lock_read(this->Rust()); }
    void ReadUnlock() { ee_ts_lock_read_unlock(this->Rust()); }
    void WriteLock() { ee_ts_lock_write(this->Rust()); }
    void WriteUnlock() { ee_ts_lock_write_unlock(this->Rust()); }
    const EeTsLock* Rust() const { return reinterpret_cast<const EeTsLock*>(this); }
};

// C:\EEX\Low-Level Engine\Thread System\TSThread.h (RECONSTRUCTION; TSThread.cpp 0x54203..0x545ff, operator
// new(0x3c) in TSScheduler::CreateThread). A platform thread with its message queues. Whole in Rust
// (rust/crates/ee_base/src/ts, LLE lane round 3): the class has no data; a TSThread* is the Rust thread object TSScheduler
// made, and every member forwards. The message-id queue is under the thread's message lock (LockMessageQueue), the
// posted messages (the original's Win32 thread messages) under a lock of their own.
// winbase.h's `#define Yield()` would swallow TSThread::Yield (exported ?Yield@TSThread@@SAXK@Z)
#ifdef Yield
#undef Yield
#endif

class LLE_API TSThread : URustView {
public:
    unsigned long GetExitCode() { return ee_ts_thread_exit_code(this->Rust()); }
    long GetPriority() { return ee_ts_thread_priority(this->Rust()); }
    long SetPriority(long inPriority) { return ee_ts_thread_set_priority(this->Rust(), (int32_t)inPriority); }
    // Posts a message (optionally queuing its id too)
    long Post(unsigned long inMessage, void* inParameter1, void* inParameter2, bool inQueue)
    {
        return ee_ts_thread_post(this->Rust(), inMessage, inParameter1, inParameter2, inQueue);
    }
    // Takes one message off the queue, else (on the window thread) dispatches one window event; kErrorNotFound if
    // none is left. A posted message counts when it is WM_QUIT (0x12) or a WM_USER (0x400) one, as a window-less
    // Win32 thread message did; a dispatched event counts only while queued ids are left (Post inQueue).
    long Receive(unsigned long& outMessage, void*& outParameter1, void*& outParameter2)
    {
        return ee_ts_thread_receive(this->Rust(), &outMessage, &outParameter1, &outParameter2);
    }
    // stores the name (the debugger's thread name is a no-op in this build; nothing reads the name back)
    void SetName(const char* inName) { ee_ts_thread_set_name(this->Rust(), inName); }
    // Dispatches the window events waiting now (on the event thread; elsewhere nothing): what Win32 sent a new window
    // during its creation (activation, focus) is handled before the game knows the window, as it was
    static void DispatchWindowEvents() { ee_ts_dispatch_window_events(); }
    const EeTsThread* Rust() const { return reinterpret_cast<const EeTsThread*>(this); }
};

// TSThread::Post message ids. Only the id every thread shares is named: the other ids mean different things per receiving thread.
enum TSThreadMessage {
    kThreadMessageQuit = 0x401,  // every thread's HandleMessage stops on 0x401 (RTSGraphics mIsQuitting, RTSPhysics mStopThread, RTSMousePointer/EEDataPreLoading mQuit, EESELoadThread mIsDone, EEUserInterface mB05F); every Shutdown() / PostQuit posts it (provisional name)
};

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
    static void DestroyCriticalSection(TSCriticalSection* inSection) { ee_ts_section_free(reinterpret_cast<EeTsSection*>(inSection)); }
    // an event (manual or auto reset); the native error recorded when it fails (outEvent is then unchanged).
    // Defaults proven by TCP 0x287c: the assert text passes one argument, the call pushes 1
    static long CreateEvent(TSEvent*& outEvent, bool inManualReset = true) { return ee_ts_event_new(reinterpret_cast<EeTsEvent**>(&outEvent), inManualReset); }
    static void DestroyEvent(TSEvent* inEvent) { ee_ts_event_free(reinterpret_cast<EeTsEvent*>(inEvent)); }
    // a write section and two events, the parts made so far destroyed on failure
    static long CreateReadWriteLock(TSReadWriteLock*& outLock) { return ee_ts_lock_new(reinterpret_cast<EeTsLock**>(&outLock)); }
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
};

#endif
