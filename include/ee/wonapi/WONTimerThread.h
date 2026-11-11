#ifndef EE_WONAPI_WONTIMERTHREAD_H
#define EE_WONAPI_WONTIMERTHREAD_H
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// TimerThread: a Rust object (ee_won thread.rs EeWonTimerThread: the two deadline maps of the original, its lock, its
// thread and signal) behind this shim. The queue holds a reference per op, taken here before an op goes in and given
// back when it comes out; a due op is finished with WS_TimedOut, a purged one killed (AsyncOp.cpp's hooks, called
// under the timer's lock as before). The original unit (TimerThread.cpp) is deleted.
class TimerThread {
public:
    TimerThread();
    ~TimerThread() { ee_won_timer_thread_free(this->mTimer); }   // kills every pending op first
    void Start() { ee_won_timer_thread_start(this->mTimer); }
    void Stop(bool waitForStop=true) { ee_won_timer_thread_stop(this->mTimer,waitForStop); }
    void WaitForStop() { ee_won_timer_thread_wait_for_stop(this->mTimer); }
    void PurgeOps() { ee_won_timer_thread_purge(this->mTimer); }
    // the op times out after inTimeout ms (an earlier entry of it is replaced)
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
