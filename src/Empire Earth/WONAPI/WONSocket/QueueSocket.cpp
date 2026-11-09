// WONAPI QueueSocket: runs queued socket ops one at a time plus one repeating op. The file name comes from
// the original's anonymous-namespace RTTI (C:\EEX\Empire Earth\WONAPI\WONSocket\QueueSocket.cpp); object
// boundaries inferred
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
}
namespace WONAPI {
// FUNCTION: WON 0x006b3930
// Creates a queue socket of the given type with no ops queued
QueueSocket::QueueSocket(int inType) : AsyncSocket(inType)
{
    this->mCloseQueued = false;
}

// FUNCTION: WON 0x006b39d0
// Returns a new, unopened queue socket of the same type
AsyncSocket* QueueSocket::Duplicate()
{
    return new QueueSocket(this->mType);
}

// FUNCTION: WON 0x006b3bf0
// Sets the completion called each time the repeating op finishes
void QueueSocket::SetRepeatCompletion(CompletionBase<AsyncOpPtr>* inCompletion)
{
    AutoCrit aCrit(this->mDataCrit);
    this->mRepeatCompletion = inCompletion;
}

// FUNCTION: WON 0x006b3ca0
// Sets the completion called when the socket closes
void QueueSocket::SetCloseCompletion(CompletionBase<AsyncOpPtr>* inCompletion)
{
    AutoCrit aCrit(this->mDataCrit);
    this->mCloseCompletion = inCompletion;
}

// FUNCTION: WON 0x006b3d50
// Restarts the close timer: after the timeout the socket is force-closed (0 just cancels it)
void QueueSocket::StartCloseTimer(long inTimeout)
{
    AutoCrit aCrit(this->mDataCrit);
    this->CancelCloseTimer();
    if (inTimeout != 0) {
        this->mCloseTimer = new AsyncOp;
        this->mCloseTimer->mCompletion =
            new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(QueueSocket::StaticCloseTimerCallback, this);
        this->mCloseTimer->RunAsync(inTimeout);
    }
}

// FUNCTION: WON 0x006b3ed0
// Kills a running close timer
void QueueSocket::CancelCloseTimer()
{
    if (this->mCloseTimer.mPtr != NULL) {
        AsyncOpPtr aTimer = this->mCloseTimer;
        this->mCloseTimer = NULL;
        aTimer->Kill(true);
    }
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
    AutoCrit aCrit(this->mDataCrit);
    if (inTimer != this->mCloseTimer.get() || inTimer->GetStatus() != WS_TimedOut)
        return;
    this->mCloseTimer = NULL;
    OpCompletionBasePtr aCloseCompletion = this->mCloseCompletion;
    aCrit.Leave();
    this->Close();
    if (aCloseCompletion.get() != NULL) {
        SmartPtr<CloseOp> aCloseOp = new CloseOp(this);
        aCloseOp->mCompletion = aCloseCompletion;
        aCloseOp->ForceFinish(WS_Killed);
    }
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
    AutoCrit aCrit(this->mDataCrit);
    bool aClose = false;
    if ((inOp->GetStatus() != WS_Success && this->GetType() != 1) || !this->IsValid())
        aClose = true;
    OpCompletionBasePtr aCompletion;
    if (inOp == this->mQueueOp.get()) {
        aCompletion = this->mCurQueueCompletion;
        this->mCurQueueCompletion = NULL;
        this->mQueueOp = NULL;
        if (!aClose) {
            if (this->mCloseQueued && this->mOpQueue.empty())
                aClose = true;
            else
                this->RunQueueOp();
        }
    }
    else if (inOp == this->mRepeatOp.get()) {
        aCompletion = this->mRepeatCompletion;
        if (inOp->mKilled)
            aClose = true;
        if (!aClose) {
            // Duplicate (vtable slot 0x14) returns a SocketOp: AcceptOp (0x60 bytes), RecvBytesOp (0xac),
            // RecvMsgOp (0xb4), the RecvFrom op (0x90) or NULL, all at base offset 0
            this->mRepeatOp = static_cast<SocketOp*>(this->mRepeatOp->Duplicate());
            this->RunRepeatOp();
        }
        else {
            if (this->mCloseQueued && inOp->mKilled) {
                aClose = false;
                aCompletion = NULL;
            }
            this->mRepeatOp = NULL;
        }
    }
    else {
        QueueOpFastCompletion* aFast = dynamic_cast<QueueOpFastCompletion*>(inOp->mCompletion.get());
        if (aFast != NULL)
            aCompletion = aFast->mOriginalCompletion;
    }
    OpCompletionBasePtr aCloseCompletion;
    if (aClose)
        aCloseCompletion = this->mCloseCompletion;
    aCrit.Leave();
    if (aCompletion.get() != NULL)
        aCompletion->Complete(inOp);
    if (aClose) {
        this->Close();
        if (aCloseCompletion.get() != NULL) {
            SmartPtr<CloseOp> aCloseOp = new CloseOp(this);
            aCloseOp->mCompletion = aCloseCompletion;
            aCloseOp->ForceFinish(inOp->GetStatus());
        }
    }
}

// FUNCTION: WON 0x006b4500
// Sets the op the socket reruns after each completion (e.g. a receive) and starts it
void QueueSocket::SetRepeatOp(SocketOp* inOp)
{
    AutoCrit aCrit(this->mDataCrit);
    this->mRepeatOp = inOp;
    this->RunRepeatOp();
}

// FUNCTION: WON 0x006b45b0
// Starts the repeating op asynchronously with no timeout unless it is missing or already pending
void QueueSocket::RunRepeatOp()
{
    AutoCrit aCrit(this->mDataCrit);
    if (this->mRepeatOp.get() == NULL || this->mRepeatOp->GetStatus() == WS_Pending)
        return;
    this->mRepeatOp->mCompletion =
        new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(QueueSocket::StaticOpCallback, this);
    this->mRepeatOp->mSocket = this;
    this->mRepeatOp->RunAsync(-1);
}

// FUNCTION: WON 0x006b4740
// Appends an op with its timeout to the queue (default completion if none) and starts it if the queue is idle
void QueueSocket::QueueOp(SocketOp* inOp, long inTimeout)
{
    AutoCrit aCrit(this->mDataCrit);
    inOp->mSocket = this;
    inOp->SetTimeout(inTimeout);
    if (inOp->mCompletion.get() == NULL)
        inOp->mCompletion = this->mDefaultQueueCompletion;
    this->mOpQueue.push_back(inOp);
    this->RunQueueOp();
}

// FUNCTION: WON 0x006b48b0
// Starts the next queued op asynchronously when no queued op is running, routing its completion through the socket
void QueueSocket::RunQueueOp()
{
    AutoCrit aCrit(this->mDataCrit);
    if (this->mQueueOp.get() != NULL)
        return;
    if (this->mOpQueue.empty())
        return;
    this->mQueueOp = this->mOpQueue.front();
    this->mOpQueue.pop_front();
    this->mCurQueueCompletion = this->mQueueOp->GetCompletion();
    this->mQueueOp->SetCompletion(new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(QueueSocket::StaticOpCallback, this));
    this->mQueueOp->SetSocket(this);
    this->mQueueOp->RunAsync(this->mQueueOp->GetTimeout());
}

// FUNCTION: WON 0x006b4a70
// Queues a close after the pending ops: kills the repeating op and optionally starts a forced-close timer
void QueueSocket::QueueClose(long inTimeout)
{
    AutoCrit aCrit(this->mDataCrit);
    this->mCloseQueued = true;
    if (this->mRepeatOp.get() != NULL)
        this->mRepeatOp->Kill(true);
    SmartPtr<QueueCloseOp> aCloseOp = new QueueCloseOp;
    aCloseOp->SetRunAsyncImmediately(false);
    this->QueueOp(aCloseOp.get(), -1);
    if (inTimeout != -1)
        this->StartCloseTimer(inTimeout);
}

// FUNCTION: WON 0x006b4c00
// Clears the queued-close flag when the socket opens
void QueueSocket::OpenHook()
{
    this->mCloseQueued = false;
}

// FUNCTION: WON 0x006b4c10
// Kills the repeating op, the running and all queued ops and the close timer when the socket closes
void QueueSocket::CloseHook()
{
    this->mCloseQueued = false;
    if (this->mRepeatOp.get() != NULL) {
        this->mRepeatOp->mCompletion = this->mRepeatCompletion;
        this->mRepeatOp->Kill(true);
        this->mRepeatOp = NULL;
    }
    if (this->mQueueOp.get() != NULL) {
        this->mQueueOp->mCompletion = this->mCurQueueCompletion;
        this->mQueueOp->Kill(true);
        this->mQueueOp = NULL;
        this->mCurQueueCompletion = NULL;
    }
    std::list<SmartPtr<SocketOp> >::iterator anItr=this->mOpQueue.begin();
    while(anItr != this->mOpQueue.end()) {
        (*anItr)->Kill(true);
        ++anItr;
    }
    if (this->mCloseTimer.mPtr != NULL) {
        this->mCloseTimer->Kill(true);
        this->mCloseTimer = NULL;
    }
    this->mOpQueue.clear();
}

// FUNCTION: WON 0x006b4d40
// Drops every completion the socket holds (default, current, repeat and close)
void QueueSocket::KillHook()
{
    this->mDefaultQueueCompletion = NULL;
    this->mCurQueueCompletion = NULL;
    this->mRepeatCompletion = NULL;
    this->mCloseCompletion = NULL;
}
}
// No original address identified: WON SDK QueueSocket methods
namespace WONAPI {
void QueueSocket::SetQueueCompletion(CompletionBase<AsyncOpPtr> *theCompletion)
{
	AutoCrit aCrit(this->mDataCrit);
	this->mDefaultQueueCompletion = theCompletion;
}

void QueueSocket::QueueOpFast(SocketOp *theOp, DWORD theTimeout)
{
	AutoCrit aCrit(this->mDataCrit);
	if(this->mQueueOp.get()!=NULL)
	{
		this->QueueOp(theOp,theTimeout);
		return;
	}

	theOp->SetSocket(this);
	OpCompletionBasePtr anOriginalCompletion  = theOp->mCompletion;
	if(anOriginalCompletion.get()==NULL)
		anOriginalCompletion =  this->mDefaultQueueCompletion;

	theOp->mCompletion = new QueueOpFastCompletion(QueueSocket::StaticOpCallback,this,anOriginalCompletion);
	theOp->RunAsync(theTimeout);
	if(theOp->mStatus==WS_Pending)
	{
		this->mQueueOp = theOp;
		this->mCurQueueCompletion = anOriginalCompletion;
	}
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b3a40
// (public: virtual void * __thiscall WONAPI::QueueSocket::`scalar deleting destructor'(unsigned int)    [??_GQueueSocket@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b3a60
// (public: virtual __thiscall WONAPI::QueueSocket::~QueueSocket(void)    [??1QueueSocket@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006b96a0
// (protected: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::SocketOp>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::SocketOp)
