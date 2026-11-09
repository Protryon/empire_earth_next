// AsyncSocket's empty default open/close/kill hooks; in the original they fold into a shared empty body
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// Same body as 0x326b60 in the original (folded by the linker)
void AsyncSocket::OpenHook() {}
// Same body as 0x326b60 in the original (folded by the linker)
void AsyncSocket::CloseHook() {}
// Same body as 0x326b60 in the original (folded by the linker)
void AsyncSocket::KillHook() {}
}
