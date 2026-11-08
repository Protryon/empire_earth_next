// AsyncOpContainer: a queue of async ops pumped by its owner
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {

// FUNCTION: WON 0x0069f730
// Creates an empty op container
AsyncOpContainer::AsyncOpContainer() { this->mPurging=false; }
// FUNCTION: WON 0x0069f830
// Destroys the container, killing and completing every queued op first
AsyncOpContainer::~AsyncOpContainer() { this->PurgeOps(); }
// FUNCTION: WON 0x0069f900
// Empties the queue, killing every running op and delivering each op's completion
void AsyncOpContainer::PurgeOps()
{
    this->mPurging=true;
    while(!this->mOpQueue.empty()) {
        this->mCurPumpOp=this->mOpQueue.front();
        this->mOpQueue.pop_front();
        if(this->mCurPumpOp->mStatus==WS_Pending) this->mCurPumpOp->Kill(true);
        this->mCurPumpOp->Complete();
    }
    this->mPurging=false;
}
// FUNCTION: WON 0x0069f9b0
// Queues an op (not the one being pumped unless it is still running) and signals the first entry
void AsyncOpContainer::QueueOp(AsyncOp* op)
{
    AutoCrit lock(this->mDataCrit);
    if(op==this->mCurPumpOp.get() && (op->mStatus!=WS_Pending || this->mPurging)) return;
    this->mOpQueue.push_back(op);
    if(this->mOpQueue.size()==1) SetEvent(this->mSignal.mEvent);
}
// FUNCTION: WON 0x0069fad0
// Runs and completes queued ops, waiting up to waitTime for work, until maxOps ran or the wait expired
void AsyncOpContainer::Pump(unsigned long waitTime,int maxOps)
{
    AutoCrit lock(this->mDataCrit);
    unsigned long start=GetTickCount();
    int remaining=maxOps;
    if(remaining<0) remaining=this->mOpQueue.size();
    if(this->mOpQueue.empty() && waitTime>0) {
        lock.Leave();
        this->mSignal.WaitFor(waitTime);
        lock.Enter();
    }
    while(!this->mOpQueue.empty()) {
        this->mCurPumpOp=this->mOpQueue.front();
        this->mOpQueue.pop_front();
        lock.Leave();
        if(this->mCurPumpOp->mStatus==WS_Pending) this->mCurPumpOp->StartRunAsync();
        if(this->mCurPumpOp->mCompletionPending) this->mCurPumpOp->Complete();
        this->mCurPumpOp=NULL;
        lock.Enter();
        if(remaining>0 && --remaining==0) break;
        if(waitTime>0 && GetTickCount()-start>=waitTime) break;
    }
    this->mCurPumpOp=NULL;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0069f7c0
// (public: virtual __thiscall WONAPI::Event::~Event(void)    [??1Event@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x0069f7e0
// (public: virtual void * __thiscall WONAPI::Event::`scalar deleting destructor'(unsigned int)    [??_GEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0069f810
// (public: virtual void * __thiscall WONAPI::AsyncOpContainer::`scalar deleting destructor'(unsigned int)    [??_GAsyncOpContainer@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a9800
// (protected: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::AsyncOp> )
