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

// The queue and the pump's accounting are Rust's (rust/crates/ee_won/src/asyncop.rs OpQueue, PumpPlan): the
// queue holds an op as a handle with a reference this container takes when queuing and gives back when the op
// comes out (into mCurPumpOp). The lock, the signal and the ops' steps stay here.

// FUNCTION: WON 0x0069f730
// Creates an empty op container
AsyncOpContainer::AsyncOpContainer()
{
    this->mPurging=false;
    this->mOpQueue=ee_won_opqueue_new();
}
// FUNCTION: WON 0x0069f830
// Destroys the container, killing and completing every queued op first
AsyncOpContainer::~AsyncOpContainer()
{
    this->PurgeOps();
    ee_won_opqueue_destroy(this->mOpQueue);
    this->mOpQueue=NULL;
}
// The front op into outOp (the queue's reference released) -> false when the queue is empty
static bool TakeFront(EeWonOpQueue* inQueue,AsyncOpPtr& outOp)
{
    AsyncOp* op=static_cast<AsyncOp*>(ee_won_opqueue_pop(inQueue));
    if(op==NULL) return false;
    outOp=op;
    op->Release();
    return true;
}
// FUNCTION: WON 0x0069f900
// Empties the queue, killing every running op and delivering each op's completion
void AsyncOpContainer::PurgeOps()
{
    this->mPurging=true;
    while(TakeFront(this->mOpQueue,this->mCurPumpOp)) {
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
    AsyncOp* current=this->mCurPumpOp.get();
    int aResult=ee_won_opqueue_queue(this->mOpQueue,op,current,current!=NULL && current->mStatus==WS_Pending,this->mPurging);
    if(aResult==0) return;
    op->AddRef();
    if(aResult==2) this->mSignal.Set();
}
// FUNCTION: WON 0x0069fad0
// Runs and completes queued ops, waiting up to waitTime for work, until maxOps ran or the wait expired
void AsyncOpContainer::Pump(unsigned long waitTime,int maxOps)
{
    AutoCrit lock(this->mDataCrit);
    size_t queued=ee_won_opqueue_len(this->mOpQueue);
    EeWonPumpPlan aPlan=ee_won_pump_plan_new(static_cast<uint32_t>(waitTime),maxOps,queued);
    if(ee_won_pump_plan_should_wait(&aPlan,queued)) {
        lock.Leave();
        this->mSignal.WaitFor(waitTime);
        lock.Enter();
    }
    while(TakeFront(this->mOpQueue,this->mCurPumpOp)) {
        lock.Leave();
        if(this->mCurPumpOp->mStatus==WS_Pending) this->mCurPumpOp->StartRunAsync();
        if(this->mCurPumpOp->mCompletionPending) this->mCurPumpOp->Complete();
        this->mCurPumpOp=NULL;
        lock.Enter();
        if(!ee_won_pump_plan_go_on(&aPlan)) break;
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
