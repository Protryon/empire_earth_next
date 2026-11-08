// RecvBytesOp accessor whose body the original shares with another function
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// Same body as 0x2b1df0 in the original (folded by the linker)
ByteBufferPtr RecvBytesOp::GetBytes() const { return this->mReceivedData; }
}
