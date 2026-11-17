// C:\EEX\Low-Level Engine\Thread System\TSThread.h (RECONSTRUCTION; TSThread.cpp 0x54203..0x545ff, operator
// new(0x3c) in TSScheduler::CreateThread). A platform thread with its message queues. Whole in Rust
// (rust/crates/ee_base/src/ts, LLE lane round 3): the class has no data; a TSThread* is the Rust thread object TSScheduler
// made, and every member forwards. The message-id queue is under the thread's message lock (LockMessageQueue), the
// posted messages (the original's Win32 thread messages) under a lock of their own.
#ifndef LLE_TSTHREAD_H
#define LLE_TSTHREAD_H

#include "rust/base/ts.h"

// winbase.h's `#define Yield()` would swallow TSThread::Yield (exported ?Yield@TSThread@@SAXK@Z)
#ifdef Yield
#undef Yield
#endif

class LLE_API TSThread {
    friend class TSScheduler;
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
    long Resume() { return ee_ts_thread_resume(this->Rust()); }
    long Suspend() { return ee_ts_thread_suspend(this->Rust()); }
    long Terminate() { return ee_ts_thread_terminate(this->Rust()); }
    // stores the name (the debugger's thread name is a no-op in this build; nothing reads the name back)
    void SetName(const char* inName) { ee_ts_thread_set_name(this->Rust(), inName); }
    static void SetThreadName(unsigned long inThreadID, const char* inName) {}
    bool IsSuspended() const { return ee_ts_thread_is_suspended(this->Rust()); }
    bool IsTerminated() { return this->GetExitCode() != PLAT_THREAD_STILL_ACTIVE; }
    void LockMessageQueue() { ee_ts_section_enter(ee_ts_thread_message_lock(this->Rust())); }
    void UnlockMessageQueue() { ee_ts_section_leave(ee_ts_thread_message_lock(this->Rust())); }
    static void Yield(unsigned long inMilliseconds) { PlatSleep(inMilliseconds); }
    // The window events' pump (platform/PlatWindow.h): Receive on inThreadID (the thread that runs the window system)
    // dispatches one window event per call once its posted messages are drained, as the original's PeekMessage
    // dispatched window messages. inPump returns true when it dispatched one.
    static void SetEventPump(bool (*inPump)(), unsigned long inThreadID)
    {
        ee_ts_set_event_pump(inPump, (uint32_t)inThreadID);
    }
    // Dispatches the window events waiting now (on the event thread; elsewhere nothing): what Win32 sent a new window
    // during its creation (activation, focus) is handled before the game knows the window, as it was
    static void DispatchWindowEvents() { ee_ts_dispatch_window_events(); }
private:
    TSThread() = delete;
    ~TSThread() = delete;
    const EeTsThread* Rust() const { return reinterpret_cast<const EeTsThread*>(this); }
};

// TSThread::Post message ids. Only the id every thread shares is named: the other ids mean different things per receiving thread.
enum TSThreadMessage {
    kThreadMessageQuit = 0x401,  // every thread's HandleMessage stops on 0x401 (RTSGraphics mIsQuitting, RTSPhysics mStopThread, RTSMousePointer/EEDataPreLoading mQuit, EESELoadThread mIsDone, EEUserInterface mB05F); every Shutdown() / PostQuit posts it (provisional name)
};

#endif
