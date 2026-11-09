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
namespace WONAPI {

// FUNCTION: WON 0x6c4580
// Reads a client-flags-changed notice: which client and its new flags
WONStatus RoutingClientFlagsChangedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 6) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mClientId = inMsg.ReadShort();
    this->mClientFlags = inMsg.ReadLong();
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c45c0
// Reads a client-joined-group notice: group, client, member flags and optionally name and client flags
WONStatus RoutingClientJoinedGroupOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 7) return (WONStatus)WS_RoutingOp_DontWantReply;
    unsigned char flags = inMsg.ReadByte();
    this->mGroupId = inMsg.ReadShort();
    this->mClientId = inMsg.ReadShort();
    this->mMemberFlags = inMsg.ReadByte();
    if (flags & 1) inMsg.ReadWString(this->mClientName, 2);
    if (flags & 2) {
        this->mClientFlags = inMsg.ReadLong();
        this->mHasClientFlags = true;
    }
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c4800
// Reads a client-left-group notice: which group and client
WONStatus RoutingClientLeftGroupOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 9) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mGroupId = inMsg.ReadShort();
    this->mClientId = inMsg.ReadShort();
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c4840
// Reads a client-left-server notice: which client
WONStatus RoutingClientLeftServerOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 10) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mClientId = inMsg.ReadShort();
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c4f30
// Reads a data-object-deleted notice: link id, data type and object name
WONStatus RoutingDataObjectDeletedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 15) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mLinkId = inMsg.ReadShort();
    inMsg.ReadString(this->mDataType, 1);
    inMsg.ReadWString(this->mName, 1);
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c6c10
// Reads a captain-changed notice: the group and its new captain
WONStatus RoutingGroupCaptainChangedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 31) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mGroupId = inMsg.ReadShort();
    this->mCaptainId = inMsg.ReadShort();
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c6c50
// Reads a group-created notice: id, name, client limit, flags and async flags
WONStatus RoutingGroupCreatedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 32) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mGroupId = inMsg.ReadShort();
    inMsg.ReadWString(this->mName, 2);
    this->mMaxClients = inMsg.ReadShort();
    this->mFlags = inMsg.ReadLong();
    this->mAsyncFlags = inMsg.ReadLong();
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c6da0
// Reads a group-deleted notice: which group
WONStatus RoutingGroupDeletedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 33) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mGroupId = inMsg.ReadShort();
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c6dd0
// Reads a group-flags-changed notice: the group, its flags and async flags
WONStatus RoutingGroupFlagsChangedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 34) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mGroupId = inMsg.ReadShort();
    this->mGroupFlags = inMsg.ReadLong();
    this->mAsyncFlags = inMsg.ReadLong();
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c6e10
// Reads a group invitation notice: group, inviting client, invited/uninvited and comment
WONStatus RoutingGroupInvitationOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 35) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mGroupId = inMsg.ReadShort();
    this->mClientId = inMsg.ReadShort();
    this->mInvited = inMsg.ReadBool();
    inMsg.ReadWString(this->mComment, 2);
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c6e60
// Reads a join-attempt notice for the captain: client, group, comment and flags
WONStatus RoutingGroupJoinAttemptOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 36) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mClientId = inMsg.ReadShort();
    this->mGroupId = inMsg.ReadShort();
    inMsg.ReadWString(this->mComment, 2);
    this->mFlags = inMsg.ReadByte();
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c6ef0
// Reads a max-players-changed notice: the group and its new player limit
WONStatus RoutingGroupMaxPlayersChangedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 37) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mGroupId = inMsg.ReadShort();
    this->mMaxPlayers = inMsg.ReadShort();
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c6f30
// Reads a group-renamed notice: the group and its new name
WONStatus RoutingGroupNameChangedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 38) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mGroupId = inMsg.ReadShort();
    inMsg.ReadWString(this->mName, 2);
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c6f70
// Reads an observer-count notice: the group and its observer count
WONStatus RoutingGroupObserverCountOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 39) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mGroupId = inMsg.ReadShort();
    this->mObserverCount = inMsg.ReadShort();
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c7650
// Reads a member-flags-changed notice: group, client and new member flags
WONStatus RoutingMemberFlagsChangedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 44) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mGroupId = inMsg.ReadShort();
    this->mClientId = inMsg.ReadShort();
    this->mMemberFlags = inMsg.ReadByte();
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c7d40
// Reads a chat message: sender, chat flags, text and the recipient client ids
WONStatus RoutingPeerChatOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 47) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mClientId = inMsg.ReadShort();
    this->mFlags = inMsg.ReadShort();
    inMsg.ReadWString(this->mText, 2);
    int count = (unsigned short)inMsg.ReadShort();
    for (int i = 0; i < count; ++i) {
        unsigned short recipient = inMsg.ReadShort();
        this->mRecipients.push_back(recipient);
    }
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c8b80
// Reads a server alert notice and its text
WONStatus RoutingServerAlertOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 57) return (WONStatus)WS_RoutingOp_DontWantReply;
    inMsg.ReadWString(this->mText, 2);
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c8bb0
// Accepts the server-shutdown-aborted notice (no body)
WONStatus RoutingServerShutdownAbortedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 58) return (WONStatus)WS_RoutingOp_DontWantReply;
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c8bd0
// Reads a server-shutdown-started notice: message text and seconds left
WONStatus RoutingServerShutdownStartedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 59) return (WONStatus)WS_RoutingOp_DontWantReply;
    inMsg.ReadWString(this->mText, 2);
    this->mSeconds = inMsg.ReadLong();
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c8eb0
// Reads a you-were-banned notice: group, banned/unbanned, comment and duration
WONStatus RoutingYouWereBannedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 69) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mGroupId = inMsg.ReadShort();
    this->mIsBanned = inMsg.ReadBool();
    inMsg.ReadWString(this->mComment, 2);
    this->mDuration = inMsg.ReadLong();
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c8f00
// Reads a you-were-muted notice: group, muted/unmuted, comment and duration
WONStatus RoutingYouWereMutedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 70) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mGroupId = inMsg.ReadShort();
    this->mIsMuted = inMsg.ReadBool();
    inMsg.ReadWString(this->mComment, 2);
    this->mDuration = inMsg.ReadLong();
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c6eb0
// Reads a member-count notice: the group and its member count
WONStatus RoutingGroupMemberCountOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 71) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mGroupId = inMsg.ReadShort();
    this->mMemberCount = inMsg.ReadShort();
    return (WONStatus)WS_Success;
}
}

namespace WONAPI {
// FUNCTION: WON 0x6c4e40
// Reads a data-object-created notice: link id, data type, name and data
WONStatus RoutingDataObjectCreatedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 14) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mLinkId = inMsg.ReadShort();
    inMsg.ReadString(this->mDataType, 1);
    inMsg.ReadWString(this->mName, 1);
    this->mData = inMsg.ReadBuf(2);
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c4f80
// Reads a data-object-modified notice: link id, type, name, offset, insert/replace and data
WONStatus RoutingDataObjectModifiedOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 16) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mLinkId = inMsg.ReadShort();
    inMsg.ReadString(this->mDataType, 1);
    inMsg.ReadWString(this->mName, 1);
    this->mOffset = inMsg.ReadShort();
    this->mIsInsert = inMsg.ReadBool();
    this->mData = inMsg.ReadBuf(2);
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c7e00
// Reads a peer data message: the sending client and its data block
WONStatus RoutingPeerDataOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 48) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mClientId = inMsg.ReadShort();
    this->mData = inMsg.ReadBuf(2);
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x6c4700
// Reads a client-joined-server notice into a new client record, with extended data when present
WONStatus RoutingClientJoinedServerOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 8) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mClientInfo = this->GetNewClientInfo();
    this->mClientInfo->mClientId = inMsg.ReadShort();
    inMsg.ReadWString(this->mClientInfo->mName, 2);
    this->mClientInfo->mFlags = inMsg.ReadLong();
    if (inMsg.Available() > 0)
        RoutingClientJoinedServerOp::ParseExClientData(inMsg, this->mClientInfo.get());
    return (WONStatus)WS_Success;
}

// FUNCTION: WON 0x006c4680
// Reads a client's extended fields (user id, IP address) into the record, skipping unknown kinds
void RoutingClientJoinedServerOp::ParseExClientData(ReadBuffer& inMsg, RoutingClientInfo* inClient)
{
    unsigned char count = inMsg.ReadByte();
    for (int i = 0; i < count; ++i) {
        unsigned char kind = inMsg.ReadByte();
        unsigned char length = inMsg.ReadByte();
        switch (kind) {
        case 1: inClient->mUserId = inMsg.ReadLong(); break;
        case 2: inClient->mAddress = inMsg.ReadLong(); break;
        default: inMsg.ReadBytes(length); break;
        }
    }
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
