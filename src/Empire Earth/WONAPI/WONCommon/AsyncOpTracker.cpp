// AsyncOpTracker: ops tracked by id, a Rust object since round 13 (rust/crates/ee_won/src/opobj.rs EeWonOpTracker:
// the ops in id order, a reference each, the latest first among equal ids, an op tracked once, the lock; the
// original's two multimaps and their entries are gone)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {

// FUNCTION: WON 0x0069fc80
// Tracks an op under the given id (refused when it is tracked already); returns the op
AsyncOp* AsyncOpTracker::AddOp(AsyncOp* op,unsigned long id)
{
    return static_cast<AsyncOp*>(ee_won_op_tracker_add(this->mTracker,static_cast<void*>(op),static_cast<uint32_t>(id)));
}
// FUNCTION: WON 0x0069fe50
// Stops tracking an op; returns whether it was tracked
bool AsyncOpTracker::RemoveOp(AsyncOp* op) { return ee_won_op_tracker_remove(this->mTracker,static_cast<void*>(op)); }
// FUNCTION: WON 0x0069ffa0
// Stops tracking every op and kills each one with completion
void AsyncOpTracker::KillAll() { ee_won_op_tracker_kill_all(this->mTracker); }
}
