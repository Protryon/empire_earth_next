// AsyncOp run-mode setter
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
// FUNCTION: WON 0x0069f190
// Sets the op's run mode unless the op is already running (the Rust op object: ee_won opobj.rs)
void AsyncOp::SetMode(AsyncRunMode mode) { ee_won_asyncop_set_mode(this->mOp,static_cast<int32_t>(mode)); }
}
