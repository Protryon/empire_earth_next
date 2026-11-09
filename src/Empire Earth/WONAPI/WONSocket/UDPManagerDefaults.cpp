// UDPManagerSendRecvOp's default response check; the original shares its body with another function
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
// Same body as 0x2ad870 in the original (folded by the linker); the SDK default accepts any response
WONStatus UDPManagerSendRecvOp::CheckResponse(ReadBuffer&) { return WS_Success; }
}
