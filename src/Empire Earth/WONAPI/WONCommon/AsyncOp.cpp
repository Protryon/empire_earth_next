// AsyncOp: lifecycle, status, timeout and completion
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// The run state's transitions are Rust's (rust/crates/ee_won/src/asyncop.rs OpState, ee_won_op_*): each method
// copies the members into an EeWonOpState under mLock, asks Rust what to do, copies the result back and acts
// (queue in the container, arm the timer, run the hook, deliver the completion). The members, the lock, the
// completion, the container and the hooks stay here.

// FUNCTION: WON 0x0069ef80
// Creates an idle op with an infinite timeout, async run mode and no container or timer entry
AsyncOp::AsyncOp()
{
    EeWonOpState aState;
    ee_won_op_state_init(&aState);
    this->SetState(aState);
    this->mTimerQueue=NULL;
    this->mTimerKey=0;
    this->mContainer=NULL;
}
// FUNCTION: WON 0x0069f000
// Destroys the op (members only)
AsyncOp::~AsyncOp() {}

void AsyncOp::RunHook() {}
void AsyncOp::CleanupHook() {}

// FUNCTION: WON 0x0069f1b0
// Sets the op's timeout unless the op is already running
void AsyncOp::SetTimeout(unsigned long timeout)
{
    EeWonOpState aState=this->GetState();
    ee_won_op_set_timeout(&aState,static_cast<uint32_t>(timeout));
    this->SetState(aState);
}
// FUNCTION: WON 0x0069f1d0
// Sets whether an async run starts at once instead of being queued
void AsyncOp::SetRunAsyncImmediately(bool flag) {this->mRunAsyncImmediately=flag;}
// FUNCTION: WON 0x0069f1e0
// Marks the op killed and, when asked, finishes it with the killed status
void AsyncOp::Kill(bool finish)
{
    AutoCrit lock(this->mLock);
    EeWonOpState aState=this->GetState();
    int anAction=ee_won_op_kill(&aState,finish);
    this->SetState(aState);
    lock.Leave();
    if(anAction==1) this->QueueInOpContainer();
}
// FUNCTION: WON 0x0069f200
// Queues the op on its container, or on the global WONAPI core queue when it has none
void AsyncOp::QueueInOpContainer()
{
    if(this->mContainer!=NULL) this->mContainer->QueueOp(this);
    else WONAPICore::QueueOp(this);
}
// FUNCTION: WON 0x0069f220
// Finishes a running op with the given status and queues its completion for async runs
void AsyncOp::Finish(WONStatus status)
{
    AutoCrit lock(this->mLock);
    EeWonOpState aState=this->GetState();
    int anAction=ee_won_op_finish(&aState,status);
    this->SetState(aState);
    lock.Leave();
    if(anAction==1) this->QueueInOpContainer();
}
// FUNCTION: WON 0x0069f2f0
// Finishes the op with the given status even when it already finished, unless a completion is pending
void AsyncOp::ForceFinish(WONStatus status)
{
    AutoCrit lock(this->mLock);
    EeWonOpState aState=this->GetState();
    int anAction=ee_won_op_force_finish(&aState,status);
    this->SetState(aState);
    lock.Leave();
    if(anAction==1) this->QueueInOpContainer();
}
// FUNCTION: WON 0x0069f3a0
// Delivers a pending completion: drops the timer entry, runs the cleanup hook and calls the completion
void AsyncOp::Complete()
{
    AutoCrit lock(this->mLock);
    EeWonOpState aState=this->GetState();
    bool aDeliver=ee_won_op_complete(&aState);
    this->SetState(aState);
    if(!aDeliver) return;
    if(this->mTimerQueue!=NULL) {
        lock.Leave();
        WONAPICore::RemoveTimerOp(this);
    } else lock.Leave();
    this->CleanupHook();
    SmartPtr<CompletionBase<AsyncOpPtr> > completion=this->mCompletion;
    if(completion.get()!=NULL) completion->Complete(this);
}
// FUNCTION: WON 0x0069f4c0
// Starts an async run: runs the hook and arms the timeout timer, or times out at once for a zero timeout
void AsyncOp::StartRunAsync()
{
    this->RunHook();
    EeWonOpState aState=this->GetState();
    int anAction=ee_won_op_after_run_hook(&aState);
    if(anAction==1) WONAPICore::AddTimerOp(this,this->mTimeout);
    else if(anAction==2) this->Finish(WS_TimedOut);
}
// FUNCTION: WON 0x0069f500
// Starts the op in the given mode and timeout (async queued/immediate, else blocking); returns its status
WONStatus AsyncOp::Run(long mode,unsigned long timeout)
{
    AutoCrit lock(this->mLock);
    EeWonOpState aState=this->GetState();
    int anAction=ee_won_op_run(&aState,static_cast<int32_t>(mode),static_cast<uint32_t>(timeout));
    this->SetState(aState);
    lock.Leave();
    switch(anAction) {
    case 0: return WS_Pending;
    case 1: return WS_AwaitingCompletion;
    case 2: this->StartRunAsync(); break;
    case 3: this->QueueInOpContainer(); break;
    default:
        this->RunHook();
        this->Complete();
        break;
    }
    return this->mStatus;
}
// FUNCTION: WON 0x0069f610
// Starts an idle op as a pure timer that fires after the given timeout
void AsyncOp::RunAsTimer(unsigned long timeout)
{
    AutoCrit lock(this->mLock);
    EeWonOpState aState=this->GetState();
    bool anArm=ee_won_op_run_as_timer(&aState,static_cast<uint32_t>(timeout));
    this->SetState(aState);
    lock.Leave();
    if(anArm) WONAPICore::AddTimerOp(this,timeout);
}
// FUNCTION: WON 0x0069f6d0
// Runs the op asynchronously with the given timeout
void AsyncOp::RunAsync(long timeout) { this->Run(1,timeout); }
// FUNCTION: WON 0x0069f6e0
// Runs the op blocking with the given timeout; returns true on success
bool AsyncOp::RunBlock(unsigned long timeout) {return this->Run(2,timeout)==WS_Success;}
// FUNCTION: WON 0x0069f700
// Returns the time left before the op times out; 0 for async runs or once the timeout has passed
unsigned long AsyncOp::TimeLeft()
{
    return ee_won_op_time_left(this->GetTiming());
}
}

// No original address identified: WON SDK AsyncOp::RealTimeLeft
namespace WONAPI {
DWORD AsyncOp::RealTimeLeft()
{
	return ee_won_real_time_left(this->mStartTime, static_cast<uint32_t>(this->mTimeout));
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0069efe0
// (public: virtual void * __thiscall WONAPI::AsyncOp::`scalar deleting destructor'(unsigned int)    [??_GAsyncOp@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0069f070
// (public: __thiscall WONAPI::CriticalSection::~CriticalSection(void)    [??1CriticalSection@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x0069f2c0
// (public: __thiscall WONAPI::AutoCrit::~AutoCrit(void)    [??1AutoCrit@WONAPI@@QAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a8770
// (public: class WONAPI::CompletionBase<class WONAPI::SmartPtr<class WONAPI::AsyncOp> > * __thiscall WONAPI::SmartPtr<class WONAPI::CompletionBase<class )
// TEMPLATE: WON 0x006a8910
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::AsyncOp> >,struct std::_Mmapk<unsign)
// TEMPLATE: WON 0x006a8e00
// (public: virtual __thiscall std::out_of_range::~out_of_range(void))
// TEMPLATE: WON 0x006a8f10
// (public: struct std::pair<class std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::AsyncOp> >,struct s)
// TEMPLATE: WON 0x006a9490
// (protected: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::AsyncOp> >,struct std::_Mmapk<uns)
// TEMPLATE: WON 0x006a9790
// (public: virtual __thiscall std::length_error::~length_error(void))
// TEMPLATE: WON 0x006aa210
// (void __cdecl std::_Construct(struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::AsyncOp> > *,struct std::pair<unsigned long co)
// TEMPLATE: WON 0x00769eb0
// (protected: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::AsyncOp> >,struct std::_Mmapk<uns)
