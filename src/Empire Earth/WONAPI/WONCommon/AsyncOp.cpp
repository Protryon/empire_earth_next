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
#include "ee/wonapi/WONTimerThread.h"
namespace WONAPI {
// The op is a Rust object (rust/crates/ee_won/src/opobj.rs EeWonAsyncOp): its state, lock, completion and
// container, and every step below. These are the calls it makes back into the C++ (one table for every op and
// AsyncOpTracker): the virtual hooks, WONAPICore's queue and timer, the completion's references and call, the op's
// references and kill.
static void HookRun(void* inOp) { static_cast<AsyncOp*>(inOp)->RunHook(); }
static void HookCleanup(void* inOp) { static_cast<AsyncOp*>(inOp)->CleanupHook(); }
static void HookCoreQueue(void* inOp) { WONAPICore::QueueOp(static_cast<AsyncOp*>(inOp)); }
static void HookTimerAdd(void* inOp,uint32_t inTimeout) { WONAPICore::AddTimerOp(static_cast<AsyncOp*>(inOp),inTimeout); }
static void HookTimerRemove(void* inOp) { WONAPICore::RemoveTimerOp(static_cast<AsyncOp*>(inOp)); }
static void HookCompletionAddRef(void* inCompletion)
{ static_cast<CompletionBase<AsyncOpPtr>*>(inCompletion)->AddRef(); }
static void HookCompletionRelease(void* inCompletion)
{ static_cast<CompletionBase<AsyncOpPtr>*>(inCompletion)->Release(); }
static void HookCompletionCall(void* inCompletion,void* inOp)
{ static_cast<CompletionBase<AsyncOpPtr>*>(inCompletion)->Complete(static_cast<AsyncOp*>(inOp)); }
static void HookOpAddRef(void* inOp) { static_cast<AsyncOp*>(inOp)->AddRef(); }
static void HookOpRelease(void* inOp) { static_cast<AsyncOp*>(inOp)->Release(); }
static void HookOpKill(void* inOp) { static_cast<AsyncOp*>(inOp)->Kill(true); }
static const EeWonAsyncOpHooks kAsyncOpHooks={HookRun,HookCleanup,HookCoreQueue,HookTimerAdd,HookTimerRemove,
                                             HookCompletionAddRef,HookCompletionRelease,HookCompletionCall,
                                             HookOpAddRef,HookOpRelease,HookOpKill};
const EeWonAsyncOpHooks* AsyncOpHooks() { return &kAsyncOpHooks; }

// FUNCTION: WON 0x0069ef80
// Creates an idle op with an infinite timeout, async run mode and no container or timer entry
AsyncOp::AsyncOp() { this->mOp=ee_won_asyncop_new(static_cast<void*>(this),&kAsyncOpHooks); }
// FUNCTION: WON 0x0069f000
// Destroys the op: the completion's reference and the lock go with the Rust object
AsyncOp::~AsyncOp() { ee_won_asyncop_free(this->mOp); }

void AsyncOp::RunHook() {}
void AsyncOp::CleanupHook() {}

// Sets the op's container (NULL: WONAPICore's queue)
void AsyncOp::SetContainer(AsyncOpContainer* inContainer)
{
    ee_won_asyncop_set_container(this->mOp,inContainer,inContainer!=NULL ? inContainer->mContainer : NULL);
}
// FUNCTION: WON 0x0069f1b0
// Sets the op's timeout unless the op is already running
void AsyncOp::SetTimeout(unsigned long timeout) { ee_won_asyncop_set_timeout(this->mOp,static_cast<uint32_t>(timeout)); }
// FUNCTION: WON 0x0069f1d0
// Sets whether an async run starts at once instead of being queued
void AsyncOp::SetRunAsyncImmediately(bool flag) { ee_won_asyncop_set_run_async_immediately(this->mOp,flag); }
// FUNCTION: WON 0x0069f1e0
// Marks the op killed and, when asked, finishes it with the killed status
void AsyncOp::Kill(bool finish) { ee_won_asyncop_kill(this->mOp,finish); }
// FUNCTION: WON 0x0069f200
// Queues the op on its container, or on the global WONAPI core queue when it has none
void AsyncOp::QueueInOpContainer() { ee_won_asyncop_queue_in_container(this->mOp); }
// FUNCTION: WON 0x0069f220
// Finishes a running op with the given status and queues its completion for async runs
void AsyncOp::Finish(WONStatus status) { ee_won_asyncop_finish(this->mOp,status); }
// FUNCTION: WON 0x0069f2f0
// Finishes the op with the given status even when it already finished, unless a completion is pending
void AsyncOp::ForceFinish(WONStatus status) { ee_won_asyncop_force_finish(this->mOp,status); }
// FUNCTION: WON 0x0069f3a0
// Delivers a pending completion: drops the timer entry, runs the cleanup hook and calls the completion
void AsyncOp::Complete() { ee_won_asyncop_complete(this->mOp); }
// FUNCTION: WON 0x0069f4c0
// Starts an async run: runs the hook and arms the timeout timer, or times out at once for a zero timeout
void AsyncOp::StartRunAsync() { ee_won_asyncop_start_run_async(this->mOp); }
// FUNCTION: WON 0x0069f500
// Starts the op in the given mode and timeout (async queued/immediate, else blocking); returns its status
WONStatus AsyncOp::Run(long mode,unsigned long timeout)
{
    return ee_won_asyncop_run(this->mOp,static_cast<int32_t>(mode),static_cast<uint32_t>(timeout));
}
// FUNCTION: WON 0x0069f610
// Starts an idle op as a pure timer that fires after the given timeout
void AsyncOp::RunAsTimer(unsigned long timeout) { ee_won_asyncop_run_as_timer(this->mOp,static_cast<uint32_t>(timeout)); }
// FUNCTION: WON 0x0069f6d0
// Runs the op asynchronously with the given timeout
void AsyncOp::RunAsync(long timeout) { this->Run(1,timeout); }
// FUNCTION: WON 0x0069f6e0
// Runs the op blocking with the given timeout; returns true on success
bool AsyncOp::RunBlock(unsigned long timeout) {return this->Run(2,timeout)==WS_Success;}
// FUNCTION: WON 0x0069f700
// Returns the time left before the op times out; 0 for async runs or once the timeout has passed
unsigned long AsyncOp::TimeLeft() { return ee_won_asyncop_time_left(this->mOp); }
}

// No original address identified: WON SDK AsyncOp::RealTimeLeft
namespace WONAPI {
DWORD AsyncOp::RealTimeLeft() { return ee_won_asyncop_real_time_left(this->mOp); }
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

// ---- the C++ side of the Rust thread, timer thread and op container (ee_won thread.rs): their calls into the
// objects they hold (Thread.cpp, TimerThread.cpp and AsyncOpContainer.cpp are deleted)
namespace WONAPI {
// the thread's body: ThreadFunc, with the crash dump's SEH filter on Windows (elsewhere there is no SEH)
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

// a due op times out; a purged one is killed; then the timer's reference goes
static void TimerOpDue(void*,void* inOp)
{
    AsyncOp* anOp=static_cast<AsyncOp*>(inOp);
    anOp->Finish(WS_TimedOut);
    anOp->Release();
}
static void TimerOpPurged(void*,void* inOp)
{
    AsyncOp* anOp=static_cast<AsyncOp*>(inOp);
    anOp->Kill(true);
    anOp->Release();
}
TimerThread::TimerThread()
{
    EeWonTimerHooks aHooks={TimerOpDue,TimerOpPurged,NULL};
    this->mTimer=ee_won_timer_thread_new(aHooks);
}

static void OpAddRef(void*,void* inOp) { static_cast<AsyncOp*>(inOp)->AddRef(); }
static void OpRelease(void*,void* inOp) { static_cast<AsyncOp*>(inOp)->Release(); }
static bool OpIsPending(void*,void* inOp) { return static_cast<AsyncOp*>(inOp)->GetStatus()==WS_Pending; }
static bool OpCompletionPending(void*,void* inOp) { return static_cast<AsyncOp*>(inOp)->IsCompletionPending(); }
static void OpStartRunAsync(void*,void* inOp) { static_cast<AsyncOp*>(inOp)->StartRunAsync(); }
static void OpComplete(void*,void* inOp) { static_cast<AsyncOp*>(inOp)->Complete(); }
static void OpKill(void*,void* inOp) { static_cast<AsyncOp*>(inOp)->Kill(true); }
AsyncOpContainer::AsyncOpContainer()
{
    EeWonContainerHooks aHooks={OpAddRef,OpRelease,OpIsPending,OpCompletionPending,OpStartRunAsync,OpComplete,OpKill,
                                NULL};
    this->mContainer=ee_won_container_new(aHooks);
}
}
