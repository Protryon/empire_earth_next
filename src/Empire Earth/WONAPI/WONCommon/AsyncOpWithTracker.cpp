// AsyncOpWithTracker: an op that owns sub-ops and their nested completion
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// RTTI .?AVTrackerCompletion@AsyncOpWithTracker@WONAPI@@; 16 bytes, two vtable slots
class AsyncOpWithTracker::TrackerCompletion : public CompletionBase<AsyncOpPtr> {
    SmartPtr<AsyncOpWithTracker> mOp; //08 keeps the parent op alive
    int mParam; //0c
public:
    TrackerCompletion(AsyncOpWithTracker* op,int param):mOp(op),mParam(param) {}
    // 0x6a01a0: forwards the finished sub-op to the parent's Callback with the tracked parameter
    virtual void Complete(AsyncOpPtr op) {this->mOp->Callback(op.get(),this->mParam);}
};
// FUNCTION: WON 0x006a00d0
// Handles a finished sub-op: while running, untracks it and passes it to CallbackHook
void AsyncOpWithTracker::Callback(AsyncOp* op,int param)
{
    if(this->GetStatus()==WS_Pending && this->mTracker.RemoveOp(op)) this->CallbackHook(op,param);
}
// FUNCTION: WON 0x006a0100
// Tracks a sub-op under the given parameter and routes its completion back to this op; returns the sub-op
AsyncOp* AsyncOpWithTracker::Track(AsyncOp* op,int param)
{
    op->mCompletion=new TrackerCompletion(this,param);
    return this->mTracker.AddOp(op,param);
}
// FUNCTION: WON 0x006a0290
// Kills every tracked sub-op when the op finishes
void AsyncOpWithTracker::CleanupHook() {this->mTracker.KillAll();}
}
namespace WONAPI {

// FUNCTION: WON 0x006accc0
// Default sub-op hook: ignores the sub-op and returns false
bool AsyncOpWithTracker::CallbackHook(AsyncOp*,int) { return false; }
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a0230
// (public: virtual __thiscall WONAPI::AsyncOpWithTracker::TrackerCompletion::~TrackerCompletion(void)    [??1TrackerCompletion@AsyncOpWithTracker@WONAPI@)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a0210
// (public: virtual void * __thiscall WONAPI::AsyncOpWithTracker::TrackerCompletion::`scalar deleting destructor'(unsigned int)    [??_GTrackerCompletion@)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a01a0
// (public: virtual void __thiscall WONAPI::AsyncOpWithTracker::TrackerCompletion::Complete(class WONAPI::SmartPtr<class WONAPI::AsyncOp>))
