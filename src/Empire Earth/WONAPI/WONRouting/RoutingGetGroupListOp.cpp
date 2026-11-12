// WONAPI routing op that fetches the group list (optionally with each group's members)
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
// FUNCTION: WON 0x6c5980
// Clears the group map and asks the routing server for the groups under a parent, with the fields the flags select
void RoutingGetGroupListOp::SendRequest()
{
    this->mGroupMap.clear();
    // the message is Rust's (ee_won routemsg.rs)
    RouteSendGroupList(this);
    this->SendMsg();
    this->AddOp();
}

// FUNCTION: WON 0x6c5bd0
// Accepts the group list reply, parses it into the group map on success and returns its status
WONStatus RoutingGetGroupListOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteGroupList(this, inMsgType, inMsg);
}

}

namespace WONAPI {
// FUNCTION: WON 0x006c5a60
// Returns a new member record, from the connection's factory when the op has a connection
RoutingMemberInfoPtr RoutingGetGroupListOp::GetNewMemberInfo()
{
    if(this->mConnection.get()!=NULL) return RoutingOp::GetNewMemberInfo();
    return new RoutingMemberInfo;
}
// FUNCTION: WON 0x006c5ad0
// Returns a new group record, from the connection's factory when the op has a connection
RoutingGroupInfoPtr RoutingGetGroupListOp::GetNewGroupInfo()
{
    if(this->mConnection.get()!=NULL) return RoutingOp::GetNewGroupInfo();
    return new RoutingGroupInfo;
}
// FUNCTION: WON 0x006c5f70
// Parses a group list from a raw buffer into the group map (ParseReplyExceptForStatus, 0x6c5c10, inlined);
// returns false when the buffer runs short
bool RoutingGetGroupListOp::ParseGroupListObj(const void* data,unsigned long size)
{
    // the read is Rust's (ee_won routemsg.rs): the group map rebuilt from the body, what was read kept when it
    // runs short
    return RouteParseGroupListObj(this, data, size);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c8f50
// (public: struct std::pair<class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<struct WONAPI::RoutingMemberInf)
