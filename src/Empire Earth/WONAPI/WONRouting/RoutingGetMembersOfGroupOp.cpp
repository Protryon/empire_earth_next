// WONAPI routing op that fetches the members of one group
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
// FUNCTION: WON 0x6c6320
// Clears the member map and asks the routing server for the members of the group
void RoutingGetMembersOfGroupOp::SendRequest()
{
    this->mMembers.clear();
    // the message is Rust's (ee_won routemsg.rs)
    RouteSendGroupId(this, EE_WON_RT_REQ_MEMBERS);
    this->SendMsg();
    this->AddOp();
}

// FUNCTION: WON 0x6c63f0
// Parses the member list reply for this group into the member map, with client names/flags when sent
WONStatus RoutingGetMembersOfGroupOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteMembers(this, inMsgType, inMsg);
}
}
