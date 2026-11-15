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
// FUNCTION: WON 0x006a0100
// Tracks a sub-op under the given parameter and routes its completion back to this op; returns the sub-op. The
// completion is Rust's (ee_won opobj.rs: the original's TrackerCompletion, 0x6a01a0, and Callback, 0x6a00d0: while
// this op is pending and still tracks the sub-op, the sub-op leaves the tracker and CallbackHook runs)
AsyncOp* AsyncOpWithTracker::Track(AsyncOp* op,int param)
{
    return static_cast<AsyncOp*>(ee_won_asyncop_track(this->GetOpObject(),this->mTracker.GetTrackerObject(),op,
                                                      op->GetOpObject(),param));
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

