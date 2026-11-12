// WONAPI routing op that joins a group
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
// FUNCTION: WON 0x006c7140
// Creates a join-group request with group 0 and no join or member flags
RoutingJoinGroupOp::RoutingJoinGroupOp(RoutingConnection* inConnection)
    : RoutingOp(inConnection)
{
    this->mGroupId = 0;
    this->mJoinFlags = 0;
    this->mMemberFlags = 0;
}

// FUNCTION: WON 0x006c7370
// Sends the routing server the join request (group, password, comment, join and member flags)
void RoutingJoinGroupOp::SendRequest()
{
    // the message is Rust's (ee_won routemsg.rs)
    RouteSendJoinGroup(this);
    this->SendMsg();
    this->AddOp();
}

// FUNCTION: WON 0x006c73e0
// Reads the join status for this group (waits on when pending or members follow) and the member list that follows
WONStatus RoutingJoinGroupOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteJoinGroup(this, inMsgType, inMsg);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c7250
// (public: virtual __thiscall WONAPI::RoutingJoinGroupOp::~RoutingJoinGroupOp(void)    [??1RoutingJoinGroupOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c7220
// (public: virtual long __thiscall WONAPI::RoutingJoinGroupOp::GetType(void)const     [?GetType@RoutingJoinGroupOp@WONAPI@@UBEJXZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c7230
// (public: virtual void * __thiscall WONAPI::RoutingJoinGroupOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingJoinGroupOp@WONAPI@@UAEPAXI@Z])
