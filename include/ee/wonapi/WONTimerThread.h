
#ifndef EE_WONAPI_WONTIMERTHREAD_H
#define EE_WONAPI_WONTIMERTHREAD_H
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// TimerThread (0x50 bytes), derived from Thread (0x1c)
class TimerThread : public Thread {
public:
    TimerThread(); //2a02a0
    virtual ~TimerThread(); //2a0480
    virtual void ThreadFunc(); //2a09c0, inherited slot 0x10
    void PurgeOps(); //2a0540
    void AddTimerOp(AsyncOp*,unsigned long); //2a0700
    void RemoveTimerOp(AsyncOp*); //2a0790
    void Pump(); //2a0800
    unsigned long GetWaitTime(); //2a0970
    void Detach(AsyncOp* inOp);   // takes the op's entry out of the queue (the lock held)
    CriticalSection mDataCrit; //1c
    // the original's two deadline multimaps (this tick era's and the one past the 32-bit wrap) and mLastTick:
    // Rust's (rust/crates/ee_won/src/asyncop.rs TimerQueue), holding a reference per op
    EeWonTimerQueue* mTimers;
};

}
#endif
