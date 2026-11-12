// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified: WON SDK routing group administration
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingMsg.h"
#include "ee/wonapi/WONRoutingGroupAdmin.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RoutingSetGroupNameOp::SendRequest()
{
    // the message is Rust's (ee_won routemsg.rs)
    RouteSendGroupText(this, EE_WON_RT_REQ_SET_GROUP_NAME, this->mGroupId, this->mGroupName);
    this->SendMsg();
    this->AddOp();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RoutingSetGroupNameOp::HandleReply(unsigned char theMsgType, ReadBuffer &theMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteStatusReply(theMsgType, theMsg);
}


}
