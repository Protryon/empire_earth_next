// The network library's C++ pieces that only the drivers still use (R6 WON lobby and network round 37): they left
// include/ee/wonapi/WONRouting.h and WONTimerThread.h as they were there. Include after ee/wonapi/WONRouting.h.
#ifndef EE_TESTS_WON_NET37_ORIG_H
#define EE_TESTS_WON_NET37_ORIG_H
#include "rust/won_net/hostop.h"
namespace WONAPI {
// AsyncOpContainer: a Rust object (ee_won thread.rs EeWonOpContainer: the queue holding a reference per op, the op
// being pumped, the lock, the signal, the pump and the purge) behind this shim; each op's steps come back through
// AsyncOp.cpp's hooks.
class AsyncOpContainer {
public:
    AsyncOpContainer() : mContainer(ee_won_container_new(WonCore())) {} // the ops' calls answered in Rust
    virtual ~AsyncOpContainer() { ee_won_container_free(this->mContainer); }
    void QueueOp(AsyncOp* inOp) { ee_won_container_queue(this->mContainer,inOp); }
    void PurgeOps() { ee_won_container_purge(this->mContainer); }
    void Pump(unsigned long inWaitTime,int inMaxOps=0)
    { ee_won_container_pump(this->mContainer,static_cast<uint32_t>(inWaitTime),inMaxOps); }
    EeWonOpContainer* mContainer;
private:
    AsyncOpContainer(const AsyncOpContainer&);
    AsyncOpContainer& operator=(const AsyncOpContainer&);
};
// AsyncOp::SetContainer (NULL: WONAPICore's queue) and GetContainer as they were
inline void SetContainer(AsyncOp* inOp, AsyncOpContainer* inContainer)
{
    ee_won_asyncop_set_container(inOp->GetOpObject(),inContainer,inContainer!=NULL ? inContainer->mContainer : NULL);
}
inline AsyncOpContainer* GetContainer(const AsyncOp* inOp)
{ return static_cast<AsyncOpContainer*>(ee_won_asyncop_container(inOp->GetOpObject())); }

// ParamCompletion<AsyncOpPtr,SmartPtr<RefCount>>: 0x10 bytes,
// callback 8, owned param c, vtable 44ca3c -> deleting dtor 2aeab0, Complete 2ae840.
template <class ResultType, class ParamType>
class ParamCompletion : public CompletionBase<ResultType> {
    typedef void (*Callback)(ResultType, ParamType);
    Callback mCallback;
    ParamType mParam;
public:
    ParamCompletion(Callback inCallback, ParamType inParam)
        : mCallback(inCallback), mParam(inParam) {}
    virtual void Complete(ResultType inResult) { this->mCallback(inResult, this->mParam); }
};

enum RoutingBadUserListType {
    RoutingBadUserListType_BannedUsers=1,
    RoutingBadUserListType_MutedUsers=2
};

// TimerThread (ee/wonapi/WONTimerThread.h): the Rust timer thread (ee_won thread.rs) behind its shim; WONAPICore's
// timer is the Rust core's own
class TimerThread {
public:
    TimerThread() : mTimer(ee_won_timer_thread_new(WonCore())) {} // a due op times out, a purged one is killed
    ~TimerThread() { ee_won_timer_thread_free(this->mTimer); }   // kills every pending op first
    void Start() { ee_won_timer_thread_start(this->mTimer); }
    void Stop(bool waitForStop=true) { ee_won_timer_thread_stop(this->mTimer,waitForStop); }
    void WaitForStop() { ee_won_timer_thread_wait_for_stop(this->mTimer); }
    void PurgeOps() { ee_won_timer_thread_purge(this->mTimer); }
    void AddTimerOp(AsyncOp* inOp,unsigned long inTimeout)
    {
        inOp->AddRef();
        if(ee_won_timer_thread_add(this->mTimer,inOp,static_cast<uint32_t>(inTimeout)))
            inOp->Release();
    }
    void RemoveTimerOp(AsyncOp* inOp)
    {
        if(ee_won_timer_thread_remove(this->mTimer,inOp))
            inOp->Release();
    }
    void Pump() { ee_won_timer_thread_pump(this->mTimer); }
    unsigned long GetWaitTime() { return ee_won_timer_thread_wait_time(this->mTimer); }
    EeWonTimerThread* mTimer;
private:
    TimerThread(const TimerThread&);
    TimerThread& operator=(const TimerThread&);
};
}
#endif
