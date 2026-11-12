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
// Same body as 0x2b1df0 in the original (folded by the linker); the bytes are the op's Rust object's
ByteBufferPtr RecvBytesOp::GetBytes() const
{ return static_cast<const ByteBuffer*>(static_cast<RefCount*>(ee_won_socketop_received(this->GetSocketOpObject()))); }
}
