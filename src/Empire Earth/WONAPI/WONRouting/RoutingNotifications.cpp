// Parsers for the routing server's unsolicited notification messages
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

// FUNCTION: WON 0x6c4580
// Reads a client-flags-changed notice: which client and its new flags
WONStatus RoutingClientFlagsChangedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteClientFlagsChanged(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c45c0
// Reads a client-joined-group notice: group, client, member flags and optionally name and client flags
WONStatus RoutingClientJoinedGroupOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteClientJoinedGroup(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c4800
// Reads a client-left-group notice: which group and client
WONStatus RoutingClientLeftGroupOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteClientLeftGroup(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c4840
// Reads a client-left-server notice: which client
WONStatus RoutingClientLeftServerOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteClientLeftServer(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c4f30
// Reads a data-object-deleted notice: link id, data type and object name
WONStatus RoutingDataObjectDeletedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteDataObjectDeleted(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c6c10
// Reads a captain-changed notice: the group and its new captain
WONStatus RoutingGroupCaptainChangedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteCaptainChanged(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c6c50
// Reads a group-created notice: id, name, client limit, flags and async flags
WONStatus RoutingGroupCreatedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteGroupCreated(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c6da0
// Reads a group-deleted notice: which group
WONStatus RoutingGroupDeletedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteGroupDeleted(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c6dd0
// Reads a group-flags-changed notice: the group, its flags and async flags
WONStatus RoutingGroupFlagsChangedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteGroupFlagsChanged(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c6e10
// Reads a group invitation notice: group, inviting client, invited/uninvited and comment
WONStatus RoutingGroupInvitationOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteGroupInvitation(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c6e60
// Reads a join-attempt notice for the captain: client, group, comment and flags
WONStatus RoutingGroupJoinAttemptOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteGroupJoinAttempt(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c6ef0
// Reads a max-players-changed notice: the group and its new player limit
WONStatus RoutingGroupMaxPlayersChangedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteMaxPlayersChanged(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c6f30
// Reads a group-renamed notice: the group and its new name
WONStatus RoutingGroupNameChangedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteGroupNameChanged(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c6f70
// Reads an observer-count notice: the group and its observer count
WONStatus RoutingGroupObserverCountOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteObserverCount(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c7650
// Reads a member-flags-changed notice: group, client and new member flags
WONStatus RoutingMemberFlagsChangedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteMemberFlagsChanged(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c7d40
// Reads a chat message: sender, chat flags, text and the recipient client ids
WONStatus RoutingPeerChatOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RoutePeerChat(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c8b80
// Reads a server alert notice and its text
WONStatus RoutingServerAlertOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteServerAlert(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c8bb0
// Accepts the server-shutdown-aborted notice (no body)
WONStatus RoutingServerShutdownAbortedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteShutdownAborted(inMsgType, inMsg);
}

// FUNCTION: WON 0x6c8bd0
// Reads a server-shutdown-started notice: message text and seconds left
WONStatus RoutingServerShutdownStartedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteShutdownStarted(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c8eb0
// Reads a you-were-banned notice: group, banned/unbanned, comment and duration
WONStatus RoutingYouWereBannedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteYouWere(EE_WON_RT_N_BANNED, this, this->mIsBanned, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c8f00
// Reads a you-were-muted notice: group, muted/unmuted, comment and duration
WONStatus RoutingYouWereMutedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteYouWere(EE_WON_RT_N_MUTED, this, this->mIsMuted, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c6eb0
// Reads a member-count notice: the group and its member count
WONStatus RoutingGroupMemberCountOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteMemberCount(this, inMsgType, inMsg);
}
}

namespace WONAPI {
// FUNCTION: WON 0x6c4e40
// Reads a data-object-created notice: link id, data type, name and data
WONStatus RoutingDataObjectCreatedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteDataObjectCreated(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c4f80
// Reads a data-object-modified notice: link id, type, name, offset, insert/replace and data
WONStatus RoutingDataObjectModifiedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteDataObjectModified(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c7e00
// Reads a peer data message: the sending client and its data block
WONStatus RoutingPeerDataOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RoutePeerData(this, inMsgType, inMsg);
}

// FUNCTION: WON 0x6c4700
// Reads a client-joined-server notice into a new client record, with extended data when present
WONStatus RoutingClientJoinedServerOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteClientJoinedServer(this, inMsgType, inMsg);
}

}

namespace WONAPI {
// FUNCTION: WON 0x006c6cb0
// Returns a new group record filled from the group-created notice
RoutingGroupInfoPtr RoutingGroupCreatedOp::GetGroupInfo()
{
    RoutingGroupInfoPtr group=this->GetNewGroupInfo();
    group->mId=this->mGroupId;
    group->mName=this->mName;
    group->mMaxClients=this->mMaxClients;
    group->mFlags=this->mFlags;
    group->mAsyncFlags=this->mAsyncFlags;
    return group;
}
}
