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
void RoutingSetGroupMaxPlayersOp::SendRequest()
{
    // the message is Rust's (ee_won routemsg.rs)
    RouteSendSetGroupMaxPlayers(this, this->mGroupId, this->mMaxPlayers);
    this->SendMsg();
    this->AddOp();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RoutingSetGroupMaxPlayersOp::HandleReply(unsigned char theMsgType, ReadBuffer &theMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteStatusReply(theMsgType, theMsg);
}


}
