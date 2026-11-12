// WONAPI routing op that fetches the routing server's client list
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingMsg.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x6c56d0
// Clears the client map and asks the routing server for its client list (extended form when requested)
void RoutingGetClientListOp::SendRequest()
{
    this->mClientMap.clear();
    // the message is Rust's (ee_won routemsg.rs)
    RouteSendClientList(this);
    this->SendMsg();
    this->AddOp();
}

// FUNCTION: WON 0x6c57a0
// Parses the (extended) client list reply into the client map by client id; returns the status
WONStatus RoutingGetClientListOp::HandleReply(unsigned char inMsgType,ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteClientList(this, inMsgType, inMsg);
}
}
