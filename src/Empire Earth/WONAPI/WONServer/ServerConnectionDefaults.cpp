// ServerConnection default handlers; the original folded their bodies into identical functions
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// Same body as 0x2fa500 in the original (folded by the linker)
void ServerConnection::MsgCallback(const ByteBuffer*) {}
}
