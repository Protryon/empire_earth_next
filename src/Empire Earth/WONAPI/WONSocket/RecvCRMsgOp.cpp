// WONAPI RecvCRMsgOp: receives one CR-LF terminated line (file grouping inferred)
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
// FUNCTION: WON 0x006b5970
// Starts receiving a line: clears the buffer and the previous message, then reads what is available
WONStatus RecvCRMsgOp::Start() { return ee_won_socketop_start(this->GetSocketOpObject(), this->GetTiming()); }
// FUNCTION: WON 0x006b59b0
// Reads one byte at a time until CR-LF (the line, the loop and the message are Rust's: ee_won sockop.rs,
// sockops.rs); stores the line (CR replaced by a NUL) as the message and returns success, WS_TimedOut when time
// runs out, or the socket error
WONStatus RecvCRMsgOp::Continue() { return ee_won_socketop_continue(this->GetSocketOpObject(), this->GetTiming()); }

}
