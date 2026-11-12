// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified: WON SDK disconnect request
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRoutingDisconnectClientOp.h"
#include "ee/wonapi/WONRoutingMsg.h"
namespace WONAPI {
void RoutingDisconnectClientOp::SendRequest()
{
    // the message is Rust's (ee_won routemsg.rs)
    RouteSendBare(this, EE_WON_RT_REQ_DISCONNECT);
    this->SendMsg();
}
}
