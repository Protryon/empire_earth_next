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
// Sets the op's run mode unless the op is already running
void AsyncOp::SetMode(AsyncRunMode mode) { if(this->mStatus!=WS_Pending) this->mRunMode=mode; }
}
