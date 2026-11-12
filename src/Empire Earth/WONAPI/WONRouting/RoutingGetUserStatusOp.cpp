// WONAPI datagram op that asks a routing server for the online status of friends
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONRoutingMsg.h"
namespace WONAPI {
// FUNCTION: WON 0x006c6630
// Creates a friend status query to a server address, with 3 retransmits and a 1 s message timeout
RoutingGetUserStatusOp::RoutingGetUserStatusOp(const IPAddr& inAddress, UDPManager* inManager)
{
    this->SetDestAddr(inAddress);
    this->SetUDPManager(inManager);
    this->SetNumRetransmits(3);
    this->SetMsgTimeout(1000);
}
// FUNCTION: WON 0x006c68c0
// Adds a WON user id to query, with status 0 until the reply arrives
void RoutingGetUserStatusOp::AddWONUserId(unsigned long inUserId)
{
    this->mFriendStatusMap.insert(FriendStatusMap::value_type(inUserId, 0));
}
// FUNCTION: WON 0x006c6970
// Builds the user status datagram listing every queried WON id and starts the send/receive
void RoutingGetUserStatusOp::RunHook()
{
    // the datagram is Rust's (ee_won routemsg.rs)
    this->mOutgoing = RouteUserStatusRequest(this->mFriendStatusMap);
    UDPManagerSendRecvOp::RunHook();
}
// FUNCTION: WON 0x006c6ab0
// Checks the status reply header and count and stores each user's status byte; returns the result
WONStatus RoutingGetUserStatusOp::CheckResponse(ReadBuffer& inMessage)
{
    // the read is Rust's (ee_won routemsg.rs); a reply cut short is an unpack failure, what was read kept
    return RouteUserStatusReply(this->mFriendStatusMap, inMessage);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c6730
// (public: virtual void * __thiscall WONAPI::RoutingGetUserStatusOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingGetUserStatusOp@WONAPI@@UA)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c6750
// (public: virtual __thiscall WONAPI::RoutingGetUserStatusOp::~RoutingGetUserStatusOp(void)    [??1RoutingGetUserStatusOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c9f90
// (protected: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned char>,struct std::_Mapk<unsigned long,unsigned char,struct st)
