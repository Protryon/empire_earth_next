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
// FUNCTION: WON 0x006a02a0
// Creates a timer thread with no pending timers
TimerThread::TimerThread() { this->mLastTick=0; }
// FUNCTION: WON 0x006a0480
// Destroys the timer thread, killing every pending op first
TimerThread::~TimerThread() {this->PurgeOps();}
// FUNCTION: WON 0x006a0540
// Kills every op waiting in either timer map and empties both
void TimerThread::PurgeOps()
{
    AutoCrit lock(this->mDataCrit);
    AsyncOpTimeoutMap::iterator i=this->mTimerMap.begin();
    while(i!=this->mTimerMap.end()) {
        AsyncOp* op=i->second;
        op->RemoveFromTimerMap(false);
        op->Kill(true);
        ++i;
    }
    i=this->mWrapAroundMap.begin();
    while(i!=this->mWrapAroundMap.end()) {
        AsyncOp* op=i->second;
        op->RemoveFromTimerMap(false);
        op->Kill(true);
        ++i;
    }
    this->mTimerMap.clear();
    this->mWrapAroundMap.clear();
    this->mLastTick=0;
}
// FUNCTION: WON 0x006a0700
// Schedules the op to time out after the given milliseconds (in the wrap map past the tick wrap)
void TimerThread::AddTimerOp(AsyncOp* op,unsigned long timeout)
{
    AutoCrit lock(this->mDataCrit);
    unsigned long now=GetTickCount();
    unsigned long untilWrap=0xffffffff-now;
    if(timeout>untilWrap) op->AddToTimerMap(this->mWrapAroundMap,timeout-untilWrap);
    else op->AddToTimerMap(this->mTimerMap,now+timeout);
    this->Signal();
}
// FUNCTION: WON 0x006a0790
// Cancels the op's pending timeout
void TimerThread::RemoveTimerOp(AsyncOp* op)
{
    AutoCrit lock(this->mDataCrit);
    op->RemoveFromTimerMap(true);
}
// FUNCTION: WON 0x006a0800
// Times out every due op; after a tick-count wrap times out the old map and switches to the wrap map
void TimerThread::Pump()
{
    AutoCrit lock(this->mDataCrit);
    unsigned long now=GetTickCount();
    AsyncOpTimeoutMap::iterator i;
    if(now<this->mLastTick) {
        for(i=this->mTimerMap.begin();i!=this->mTimerMap.end();++i) {
            AsyncOp* op=i->second;
            op->RemoveFromTimerMap(false);
            op->Finish(WS_TimedOut);
        }
        this->mTimerMap=this->mWrapAroundMap;
        this->mWrapAroundMap.clear();
    }
    this->mLastTick=now;
    i=this->mTimerMap.begin();
    while(i!=this->mTimerMap.end() && i->first<=this->mLastTick) {
        AsyncOp* op=i->second;
        op->RemoveFromTimerMap(false);
        op->Finish(WS_TimedOut);
        this->mTimerMap.erase(i++);
    }
}
// FUNCTION: WON 0x006a0970
// Returns the milliseconds until the next deadline, or until just past the tick wrap when none is set
unsigned long TimerThread::GetWaitTime()
{
    AutoCrit lock(this->mDataCrit);
    unsigned long waitTime=9-this->mLastTick;
    if(!this->mTimerMap.empty()) {
        unsigned long deadline=this->mTimerMap.begin()->first;
        waitTime=deadline<=this->mLastTick ? 0 : deadline-this->mLastTick;
    }
    return waitTime;
}
// FUNCTION: WON 0x006a09c0
// Thread loop: waits for the next deadline or a signal and pumps the timers until stopped
void TimerThread::ThreadFunc()
{
    this->mLastTick=GetTickCount();
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
