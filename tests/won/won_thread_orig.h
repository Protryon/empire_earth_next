// The WON thread shims as they were before R6 WON lobby and network round 35 (WONRouting.h's ThreadBase, WONDebug,
// Thread, SocketThread, SocketThreadSimple; AsyncOp.cpp's ThreadBase bodies and WONAPICore's socket thread table):
// the game's socket thread is Rust's since (ee_won sockthread.rs EeWonApiSocketThread). The drivers that run C++
// threads over the Rust thread object (won_asyncop_test, won_apicore_test, won_net33_test, won_net34_test) build
// these. Include after ee/wonapi/WONRouting.h and ee/wonapi/WONAPICore.h, in one unit of a program.
#ifndef EE_TESTS_WON_THREAD_ORIG_H
#define EE_TESTS_WON_THREAD_ORIG_H
namespace WONAPI {
enum ThreadPriority { ThreadPriority_Highest,ThreadPriority_High,ThreadPriority_Normal,ThreadPriority_Low,ThreadPriority_Lowest };
// ThreadBase / Thread: a Rust object (ee_won thread.rs EeWonThread: the run flags, priority, signal event and the
// platform thread) behind these shims; the object's thread calls the virtual ThreadFunc (and deletes an
// auto-deleting thread after it). The original units (Thread.cpp: the start, stop and priority hooks) are deleted.
class ThreadBase {
protected:
    virtual void ThreadFunc()=0;
public:
    ThreadBase();
    virtual ~ThreadBase() { ee_won_thread_free(this->mThread); }
    virtual void Signal() { ee_won_thread_signal(this->mThread); }
    void Start() { ee_won_thread_start(this->mThread); }
    // marks the thread stopped, signals it and waits for it when asked (nothing when it was stopped)
    void Stop(bool waitForStop=true) { ee_won_thread_stop(this->mThread,waitForStop); } // (thread.rs; Signal has no override)
    bool InThisThread() { return ee_won_thread_in_this_thread(this->mThread); }
    void WaitForStop() { ee_won_thread_wait_for_stop(this->mThread); }
    void SetPriority(ThreadPriority thePriority)
    { ee_won_thread_set_priority(this->mThread,static_cast<int32_t>(thePriority)); }
protected:
    // the original's mStopped and mSignalEvent, as a ThreadFunc reads them
    bool IsStopped() const { return ee_won_thread_is_stopped(this->mThread); }
    bool WaitForSignal(unsigned long inMilliseconds)
    { return ee_won_thread_wait_signal(this->mThread,static_cast<uint32_t>(inMilliseconds)); }
    EeWonThread* mThread;
private:
    ThreadBase(const ThreadBase&);
    ThreadBase& operator=(const ThreadBase&);
    static void RunThreadFunc(void* inThread);     // the Rust thread's body (AsyncOp.cpp: the crash dump's SEH filter)
    static void DeleteThread(void* inThread);
};
// WON SDK static-only diagnostics API (ee_won debug.rs: the stack file and the crash dump; the trace is Rust's of
// the calling thread, the filter's: theInfo is not read)
class WONDebug {
public:
    static int DumpStackToFile(void* = NULL) { return ee_won_debug_dump_stack(); }
};
class Thread : public ThreadBase {
public:
    void SetAutoDelete(bool inAutoDelete) { ee_won_thread_set_auto_delete(this->mThread,inAutoDelete); }
};

class SocketThread : public Thread {
public:
    SocketThread() { this->mSingleThreaded=false; } //2b60d0: on its own thread
    virtual ~SocketThread() {} //2b6120, explicit own vtable 44cb10
    virtual void ThreadFunc() { while(!this->IsStopped()) this->Pump(1000); } //base slot10,2b6140
    virtual void AddSocketOp(SocketOp*) {} //new slot1c, 2fa500 (empty)
    virtual void RemoveSocketOp(SocketOp*) {} //20, version-empty2fa500
    virtual void Pump(unsigned long) {} //24, version-empty2fa500
    virtual void PurgeOps() {} //28, version-empty4142a0
    virtual bool NeedThread() {return true;} //2c, true2f30d0
    void SetSingleThreaded(bool value) { this->mSingleThreaded=value; } //inline bool 34 (WON SDK)
    // continues a pending op and finishes it unless it still waits; whether it is done (2b6160, cdecl)
    bool mSingleThreaded; //34; total38 (the original's lock at 1c is SocketThreadSimple's Rust object's)
};
// SocketThreadSimple: a Rust object since round 13 (rust/crates/ee_won_net/src/sockthread.rs EeWonSocketThread: the ops
// held, a reference each, in address order as the original's std::set of SmartPtrs; the lock; DoOp, the pump, the
// purge, the adds and removes) behind this shim; the ops' steps come back through SocketThread.cpp's hook table. The
// thread is the Thread shim's; ThreadFunc pumps the object, then waits on the thread's signal
class SocketThreadSimple : public SocketThread {
public:
    SocketThreadSimple() : mOps(ee_won_socket_thread_new(WonCore())) {}
    virtual ~SocketThreadSimple() { ee_won_socket_thread_free(this->mOps); } //2b61a0: every op still held killed
    virtual void ThreadFunc() { while(!this->IsStopped()) this->Pump(20); } //2b6890
    virtual void AddSocketOp(SocketOp* inOp) { ee_won_socket_thread_add(this->mOps,static_cast<AsyncOp*>(inOp)); }
    virtual void RemoveSocketOp(SocketOp* inOp) { ee_won_socket_thread_remove(this->mOps,static_cast<AsyncOp*>(inOp)); }
    // every op continued once, the finished dropped, then a wait of up to 20 ms for a signal (2b6760)
    virtual void Pump(unsigned long inWaitTime)
    { ee_won_socket_thread_pump_wait(this->mOps,this->mThread,static_cast<uint32_t>(inWaitTime)); }
    virtual void PurgeOps() { ee_won_socket_thread_purge(this->mOps); } //2b6380: killed and forgotten
private:
    EeWonSocketThread* mOps; //owned
};

// AsyncOp.cpp's
void ThreadBase::RunThreadFunc(void* inThread)
{
    ThreadBase* aThread=static_cast<ThreadBase*>(inThread);
#ifdef _WIN32
    __try {
        aThread->ThreadFunc();
    } __except(WONDebug::DumpStackToFile(GetExceptionInformation())) {
    }
#else
    aThread->ThreadFunc();
#endif
}
void ThreadBase::DeleteThread(void* inThread) { delete static_cast<ThreadBase*>(inThread); }
ThreadBase::ThreadBase()
{
    EeWonThreadBody aBody={&ThreadBase::RunThreadFunc,&ThreadBase::DeleteThread,this};
    this->mThread=ee_won_thread_new(aBody);
}
static SocketThread* ApiThread(void* inThread) { return static_cast<SocketThread*>(inThread); }
static void* ApiSocketNew() { return static_cast<void*>(static_cast<SocketThread*>(new SocketThreadSimple)); }
static void ApiSocketDelete(void* inThread) { delete ApiThread(inThread); }
static void ApiSocketSingle(void* inThread,bool inOn) { ApiThread(inThread)->SetSingleThreaded(inOn); }
static bool ApiSocketNeedThread(void* inThread) { return ApiThread(inThread)->NeedThread(); }
static void ApiSocketStart(void* inThread) { ApiThread(inThread)->Start(); }
static void ApiSocketStop(void* inThread,bool inWait) { ApiThread(inThread)->Stop(inWait); }
static void ApiSocketWait(void* inThread) { ApiThread(inThread)->WaitForStop(); }
static void ApiSocketPurge(void* inThread) { ApiThread(inThread)->PurgeOps(); }
static void ApiSocketPump(void* inThread,uint32_t inWait) { ApiThread(inThread)->Pump(inWait); }
static void ApiSocketAdd(void* inThread,void* inOp) { ApiThread(inThread)->AddSocketOp(static_cast<SocketOp*>(inOp)); }
static void ApiSocketRemove(void* inThread,void* inOp) { ApiThread(inThread)->RemoveSocketOp(static_cast<SocketOp*>(inOp)); }
static const EeWonApiHooks kApiHooks={ApiSocketNew,ApiSocketDelete,ApiSocketSingle,ApiSocketNeedThread,ApiSocketStart,
                                      ApiSocketStop,ApiSocketWait,ApiSocketPurge,ApiSocketPump,ApiSocketAdd,
                                      ApiSocketRemove};
const EeWonApiHooks* WonApiHooks() { return &kApiHooks; }
}
#endif
