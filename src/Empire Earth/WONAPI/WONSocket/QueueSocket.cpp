// WONAPI QueueSocket: runs queued socket ops one at a time plus one repeating op. The file name comes from
// the original's anonymous-namespace RTTI (C:\EEX\Empire Earth\WONAPI\WONSocket\QueueSocket.cpp); object
// boundaries inferred. A Rust object since round 14 (rust/crates/ee_won/src/qsock.rs EeWonQueueSocket: the queue,
// the queued and repeat ops, the completions, the close timer, the queued-close flag and every step below); these
// are its calls back into the C++ objects it holds
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
using namespace WONAPI;
namespace {
// SDK QueueOpFastCompletion: callback at +8, owned param at +0xc, original completion at +0x10
// No original address identified for its constructor (used by QueueOpFast); implicit destructor
class QueueOpFastCompletion : public ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> > {
protected:
    typedef void (*Callback)(AsyncOpPtr, SmartPtr<RefCount>);
public:
    OpCompletionBasePtr mOriginalCompletion;
    QueueOpFastCompletion(Callback callback, RefCount* param, CompletionBase<AsyncOpPtr>* original)
        : ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(callback,param), mOriginalCompletion(original) {}
};
// 0x5c bytes (allocation at 2b4ac3), six-slot vtable (RTTI 44caac); the destructor 2b4b90 is implicit
// (no own vptr store)
class QueueCloseOp : public SocketOp {
public:
    QueueCloseOp() : SocketOp(NULL) {}
    virtual WONStatus Continue() { return WS_Success; }
};

typedef CompletionBase<AsyncOpPtr> Completion;
static Completion* C(void* inCompletion) { return static_cast<Completion*>(inCompletion); }
static AsyncOp* O(void* inOp) { return static_cast<AsyncOp*>(inOp); }
static QueueSocket* Q(void* inSocket) { return static_cast<QueueSocket*>(inSocket); }
void HookCompletionAddRef(void* c) { C(c)->AddRef(); }
void HookCompletionRelease(void* c) { C(c)->Release(); }
void HookCompletionCall(void* c, void* op) { C(c)->Complete(O(op)); }
void HookOpAddRef(void* op) { O(op)->AddRef(); }
void HookOpRelease(void* op) { O(op)->Release(); }
long HookOpStatus(void* op) { return O(op)->GetStatus(); }
bool HookOpKilled(void* op) { return O(op)->IsKilled(); }
uint32_t HookOpTimeout(void* op) { return static_cast<uint32_t>(O(op)->GetTimeout()); }
void* HookOpCompletion(void* op) { return O(op)->GetCompletion(); }
void HookOpSetCompletion(void* op, void* c) { O(op)->SetCompletion(C(c)); }
void HookOpSetTimeout(void* op, uint32_t t) { O(op)->SetTimeout(t); }
void HookOpRunAsync(void* op, long t) { O(op)->RunAsync(t); }
void HookOpKill(void* op) { O(op)->Kill(true); }
void HookOpForceFinish(void* op, long s) { O(op)->ForceFinish(s); }
// the queued and repeat ops are SocketOps (QueueOp, SetRepeatOp and QueueOpFast take SocketOp*)
void HookOpSetSocket(void* op, void* s) { static_cast<SocketOp*>(O(op))->SetSocket(Q(s)); }
// Duplicate (vtable slot 0x14) returns a SocketOp: AcceptOp (0x60 bytes), RecvBytesOp (0xac), RecvMsgOp (0xb4), the
// RecvFrom op (0x90) or NULL, all at base offset 0
void* HookOpDuplicate(void* op) { return static_cast<SocketOp*>(O(op))->Duplicate(); }
void* HookFastOriginal(void* c)
{
    QueueOpFastCompletion* aFast = dynamic_cast<QueueOpFastCompletion*>(C(c));
    return aFast != NULL ? aFast->mOriginalCompletion.get() : NULL;
}
void* HookNewOpCompletion(void* s)
{
    return static_cast<Completion*>(
        new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(QueueSocket::StaticOpCallback, Q(s)));
}
void* HookNewFastCompletion(void* s, void* original)
{
    return static_cast<Completion*>(new QueueOpFastCompletion(QueueSocket::StaticOpCallback, Q(s), C(original)));
}
void* HookNewCloseTimer(void* s)
{
    AsyncOp* aTimer = new AsyncOp;
    aTimer->SetCompletion(new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(QueueSocket::StaticCloseTimerCallback, Q(s)));
    return aTimer;
}
void* HookNewQueueCloseOp(void*)
{
    QueueCloseOp* anOp = new QueueCloseOp;
    anOp->SetRunAsyncImmediately(false);
    return static_cast<AsyncOp*>(anOp);
}
void* HookNewCloseOp(void* s) { return static_cast<AsyncOp*>(new CloseOp(Q(s))); }
void HookSocketClose(void* s) { Q(s)->Close(); }
bool HookSocketValid(void* s) { return Q(s)->IsValid(); }
bool HookSocketDatagram(void* s) { return Q(s)->GetType() == AsyncSocket::UDP; }
const EeWonQueueSocketHooks kQueueHooks = {
    HookCompletionAddRef, HookCompletionRelease, HookCompletionCall, HookOpAddRef, HookOpRelease, HookOpStatus,
    HookOpKilled, HookOpTimeout, HookOpCompletion, HookOpSetCompletion, HookOpSetTimeout, HookOpRunAsync,
    HookOpKill, HookOpForceFinish, HookOpSetSocket, HookOpDuplicate, HookFastOriginal, HookNewOpCompletion,
    HookNewFastCompletion, HookNewCloseTimer, HookNewQueueCloseOp, HookNewCloseOp, HookSocketClose,
    HookSocketValid, HookSocketDatagram};
}
namespace WONAPI {
// FUNCTION: WON 0x006b3930
// Creates a queue socket of the given type with no ops queued
QueueSocket::QueueSocket(int inType) : AsyncSocket(inType)
{
    this->mQueue = ee_won_queuesocket_new(static_cast<void*>(this), &kQueueHooks, this->GetDataLock());
}

// The queue's references go (the original's implicit destructor of the members, 2b3a60)
QueueSocket::~QueueSocket() { ee_won_queuesocket_free(this->mQueue); }

// FUNCTION: WON 0x006b39d0
// Returns a new, unopened queue socket of the same type
AsyncSocket* QueueSocket::Duplicate()
{
    return new QueueSocket(this->GetType());
}

// FUNCTION: WON 0x006b3bf0
// Sets the completion called each time the repeating op finishes
void QueueSocket::SetRepeatCompletion(CompletionBase<AsyncOpPtr>* inCompletion)
{
    ee_won_queuesocket_set_repeat_completion(this->mQueue, inCompletion);
}

// FUNCTION: WON 0x006b3ca0
// Sets the completion called when the socket closes
void QueueSocket::SetCloseCompletion(CompletionBase<AsyncOpPtr>* inCompletion)
{
    ee_won_queuesocket_set_close_completion(this->mQueue, inCompletion);
}

// FUNCTION: WON 0x006b3d50
// Restarts the close timer: after the timeout the socket is force-closed (0 just cancels it)
void QueueSocket::StartCloseTimer(long inTimeout)
{
    ee_won_queuesocket_start_close_timer(this->mQueue, inTimeout);
}

// FUNCTION: WON 0x006b3ed0
// Kills a running close timer
void QueueSocket::CancelCloseTimer()
{
    ee_won_queuesocket_cancel_close_timer(this->mQueue);
}

// FUNCTION: WON 0x006b3f80
// Forwards a close timer's expiry to its queue socket
void QueueSocket::StaticCloseTimerCallback(AsyncOpPtr inOp, SmartPtr<RefCount> inParam)
{
    static_cast<QueueSocket*>(inParam.get())->CloseTimerCallback(inOp.get());
}

// FUNCTION: WON 0x006b4010
// On expiry of the current close timer, closes the socket and reports a killed close to the close completion
void QueueSocket::CloseTimerCallback(AsyncOp* inTimer)
{
    ee_won_queuesocket_close_timer_callback(this->mQueue, inTimer);
}

// FUNCTION: WON 0x006b4190
// Forwards a finished socket op to its queue socket
void QueueSocket::StaticOpCallback(AsyncOpPtr inOp, SmartPtr<RefCount> inParam)
{
    static_cast<QueueSocket*>(inParam.get())->OpCallback(static_cast<SocketOp*>(inOp.get()));
}

// FUNCTION: WON 0x006b4220
// Handles a finished op: passes it to its completion, starts the next queued op or reruns the repeating op,
// and closes the socket (reporting to the close completion) on failure or a finished queued close
void QueueSocket::OpCallback(SocketOp* inOp)
{
    ee_won_queuesocket_op_callback(this->mQueue, static_cast<AsyncOp*>(inOp));
}

// FUNCTION: WON 0x006b4500
// Sets the op the socket reruns after each completion (e.g. a receive) and starts it
void QueueSocket::SetRepeatOp(SocketOp* inOp)
{
    ee_won_queuesocket_set_repeat_op(this->mQueue, static_cast<AsyncOp*>(inOp));
}

// FUNCTION: WON 0x006b45b0
// Starts the repeating op asynchronously with no timeout unless it is missing or already pending
void QueueSocket::RunRepeatOp()
{
    ee_won_queuesocket_run_repeat_op(this->mQueue);
}

// FUNCTION: WON 0x006b4740
// Appends an op with its timeout to the queue (default completion if none) and starts it if the queue is idle
void QueueSocket::QueueOp(SocketOp* inOp, long inTimeout)
{
    ee_won_queuesocket_queue_op(this->mQueue, static_cast<AsyncOp*>(inOp), inTimeout);
}

// FUNCTION: WON 0x006b48b0
// Starts the next queued op asynchronously when no queued op is running, routing its completion through the socket
void QueueSocket::RunQueueOp()
{
    ee_won_queuesocket_run_queue_op(this->mQueue);
}

// FUNCTION: WON 0x006b4a70
// Queues a close after the pending ops: kills the repeating op and optionally starts a forced-close timer
void QueueSocket::QueueClose(long inTimeout)
{
    ee_won_queuesocket_queue_close(this->mQueue, inTimeout);
}

// FUNCTION: WON 0x006b4c00
// Clears the queued-close flag when the socket opens
void QueueSocket::OpenHook()
{
    ee_won_queuesocket_open_hook(this->mQueue);
}

// FUNCTION: WON 0x006b4c10
// Kills the repeating op, the running and all queued ops and the close timer when the socket closes
void QueueSocket::CloseHook()
{
    ee_won_queuesocket_close_hook(this->mQueue);
}

// FUNCTION: WON 0x006b4d40
// Drops every completion the socket holds (default, current, repeat and close)
void QueueSocket::KillHook()
{
    ee_won_queuesocket_kill_hook(this->mQueue);
}
}
// No original address identified: WON SDK QueueSocket methods
namespace WONAPI {
void QueueSocket::SetQueueCompletion(CompletionBase<AsyncOpPtr> *theCompletion)
{
	ee_won_queuesocket_set_queue_completion(this->mQueue, theCompletion);
}

void QueueSocket::QueueOpFast(SocketOp *theOp, DWORD theTimeout)
{
	ee_won_queuesocket_queue_op_fast(this->mQueue, static_cast<AsyncOp*>(theOp), static_cast<uint32_t>(theTimeout));
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b3a40
// (public: virtual void * __thiscall WONAPI::QueueSocket::`scalar deleting destructor'(unsigned int)    [??_GQueueSocket@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b3a60
// (public: virtual __thiscall WONAPI::QueueSocket::~QueueSocket(void)    [??1QueueSocket@WONAPI@@UAE@XZ])
