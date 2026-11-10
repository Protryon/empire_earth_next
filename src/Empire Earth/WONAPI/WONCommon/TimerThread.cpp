// TimerThread: times out async ops, with a second map for deadlines past the tick-count wrap
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONTimerThread.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// The deadline maps and their arithmetic are Rust's (rust/crates/ee_won/src/asyncop.rs TimerQueue): the queue
// holds an op as a handle under a key the op keeps (mTimerQueue, mTimerKey), and this thread keeps the op alive
// with a reference while it is queued. The lock, the thread, the signal and the ops' Kill / Finish stay here.

// FUNCTION: WON 0x006a02a0
// Creates a timer thread with no pending timers
TimerThread::TimerThread() { this->mTimers=ee_won_timers_new(); }
// FUNCTION: WON 0x006a0480
// Destroys the timer thread, killing every pending op first
TimerThread::~TimerThread()
{
    this->PurgeOps();
    ee_won_timers_destroy(this->mTimers);
    this->mTimers=NULL;
}
// an op the queue let go: its entry is gone, the thread's reference goes after the op is told
static void TimerOpPurged(void*,void* inOp)
{
    AsyncOp* op=static_cast<AsyncOp*>(inOp);
    op->mTimerQueue=NULL;
    op->Kill(true);
    op->Release();
}
static void TimerOpDue(void*,void* inOp)
{
    AsyncOp* op=static_cast<AsyncOp*>(inOp);
    op->mTimerQueue=NULL;
    op->Finish(WS_TimedOut);
    op->Release();
}
// FUNCTION: WON 0x006a0540
// Kills every op waiting in either timer map and empties both
void TimerThread::PurgeOps()
{
    AutoCrit lock(this->mDataCrit);
    ee_won_timers_purge(this->mTimers,TimerOpPurged,NULL);
}
// Takes the op's entry out of the queue (the lock held); the op was referenced by AddTimerOp
void TimerThread::Detach(AsyncOp* op)
{
    if(op->mTimerQueue!=this->mTimers) return;
    void* held=ee_won_timers_remove(this->mTimers,op->mTimerKey);
    op->mTimerQueue=NULL;
    if(held!=NULL) op->Release();
}
// FUNCTION: WON 0x006a0700
// Schedules the op to time out after the given milliseconds (in the wrap map past the tick wrap)
void TimerThread::AddTimerOp(AsyncOp* op,unsigned long timeout)
{
    AutoCrit lock(this->mDataCrit);
    this->Detach(op);   // (the original moved an op already queued)
    op->AddRef();
    op->mTimerQueue=this->mTimers;
    op->mTimerKey=ee_won_timers_add(this->mTimers,static_cast<uint32_t>(timeout),op);
    this->Signal();
}
// FUNCTION: WON 0x006a0790
// Cancels the op's pending timeout
void TimerThread::RemoveTimerOp(AsyncOp* op)
{
    AutoCrit lock(this->mDataCrit);
    this->Detach(op);
}
// FUNCTION: WON 0x006a0800
// Times out every due op; after a tick-count wrap times out the old map and switches to the wrap map
void TimerThread::Pump()
{
    AutoCrit lock(this->mDataCrit);
    ee_won_timers_pump(this->mTimers,TimerOpDue,NULL);
}
// FUNCTION: WON 0x006a0970
// Returns the milliseconds until the next deadline, or until just past the tick wrap when none is set
unsigned long TimerThread::GetWaitTime()
{
    AutoCrit lock(this->mDataCrit);
    return ee_won_timers_wait_time(this->mTimers);
}
// FUNCTION: WON 0x006a09c0
// Thread loop: waits for the next deadline or a signal and pumps the timers until stopped
void TimerThread::ThreadFunc()
{
    {
        AutoCrit lock(this->mDataCrit);
        ee_won_timers_set_last_tick(this->mTimers);
    }
    for(;;) {
        this->mSignalEvent.WaitFor(this->GetWaitTime());
        if(this->mStopped) break;
        this->Pump();
    }
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a0380
// (public: virtual void * __thiscall WONAPI::TimerThread::`scalar deleting destructor'(unsigned int)    [??_GTimerThread@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a03a0
// (public: __thiscall std::multimap<unsigned long,class WONAPI::SmartPtr<class WONAPI::AsyncOp>,struct std::less<unsigned long>,class std::allocator<stru)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006dccc0
// (public: void * __thiscall std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::AsyncOp> >::`scalar deleting destructor'(unsigned int))
// TEMPLATE: WON 0x006a8900
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::AsyncOp> >,struct std::_Mmapk<unsign)
// TEMPLATE: WON 0x006a8f90
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::AsyncOp> >,struct std::_Mmapk<unsign)
// TEMPLATE: WON 0x006a93c0
// (protected: struct std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::AsyncOp> >,struct std::_Mmapk<un)
// TEMPLATE: WON 0x006a9450
// (protected: void __thiscall std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::AsyncOp> >,struct std::)
// TEMPLATE: WON 0x006a8780
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::AsyncOp> >,struct std::_Mmapk<unsign)
// TEMPLATE: WON 0x006a0460
// (public: virtual void * __thiscall WONAPI::Thread::`scalar deleting destructor'(unsigned int))
// TEMPLATE: WON 0x006f28c0
// (public: virtual __thiscall WONAPI::Thread::~Thread(void))
