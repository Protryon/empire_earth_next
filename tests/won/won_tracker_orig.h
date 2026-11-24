// AsyncOpWithTracker as it was before R6 WON lobby and network round 36 folded it into RustServerOp (its only
// subclass in the game): an op that tracks sub-ops. The drivers' tracking ops derive from it; CallbackHook is
// AsyncOp's virtual now (the hook table calls it on any op).
#ifndef EE_TESTS_WON_TRACKER_ORIG_H
#define EE_TESTS_WON_TRACKER_ORIG_H
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
class AsyncOpWithTracker : public AsyncOp {
public:
    AsyncOp* Track(AsyncOp* op,int param)
    {
        return static_cast<AsyncOp*>(ee_won_asyncop_track(this->GetOpObject(),this->mTracker.GetTrackerObject(),op,
                                                          op->GetOpObject(),param));
    }
    virtual void CleanupHook() { this->mTracker.KillAll(); } //slot08 2a0290
    virtual bool CallbackHook(AsyncOp*,int) { return false; } //slot0c 2accc0: ignores the sub-op
    AsyncOpTracker mTracker;                //44..78
};
}
#endif
