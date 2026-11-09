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
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x6c5980
// Clears the group map and asks the routing server for the groups under a parent, with the fields the flags select
void RoutingGetGroupListOp::SendRequest()
{
    this->mGroupMap.clear();
    this->InitSendMsg(26);
    this->mSendMsg.AppendShort(this->mFlags);
    this->mSendMsg.AppendShort(this->mParentGroupId);
    this->SendMsg();
    this->AddOp();
}

// FUNCTION: WON 0x6c5bd0
// Accepts the group list reply, parses it into the group map on success and returns its status
WONStatus RoutingGetGroupListOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 25) return (WONStatus)WS_RoutingOp_DontWantReply;
    WONStatus status = inMsg.ReadShort();
    if (status == 0) this->ParseReplyExceptForStatus(inMsg);
    return status;
}

// FUNCTION: WON 0x6c5c10
// Rebuilds the group map from a group list body, reading only the fields the reply's flags include
void RoutingGetGroupListOp::ParseReplyExceptForStatus(ReadBuffer& inMsg)
{
    this->mGroupMap.clear();
    this->mFlags = inMsg.ReadShort();
    int count = (unsigned short)inMsg.ReadShort();
    for (int i = 0; i < count; ++i) {
        unsigned short length = inMsg.ReadShort();
        long start = inMsg.pos();
        RoutingGroupInfoPtr group = this->GetNewGroupInfo();
        group->mId = inMsg.ReadShort();
        if (this->IncludeName()) inMsg.ReadWString(group->mName, 2);
        if (this->IncludeCaptain()) group->mCaptainId = inMsg.ReadShort();
        if (this->IncludeMaxPlayers()) group->mMaxClients = inMsg.ReadShort();
        if (this->IncludeFlags()) group->mFlags = inMsg.ReadLong();
        if (this->IncludeAsyncFlags()) group->mAsyncFlags = inMsg.ReadLong();
        if (this->IncludeObserverCount()) group->mObserverCount = inMsg.ReadShort();
        if (this->IncludeMemberCount()) group->mMaxPlayers = inMsg.ReadShort(); // holds the member count
        if (this->IncludeMembers()) {
            for (int j = 0; j < group->mMaxPlayers; ++j) {
                RoutingMemberInfoPtr member = this->GetNewMemberInfo();
                member->mClientId = inMsg.ReadShort();
                member->mFlags = inMsg.ReadByte(); // read as a byte
                group->mMembers[member->mClientId] = member;
            }
        }
        this->mGroupMap[group->mId] = group;
        inMsg.ReadBytes(length - inMsg.pos() + start);
    }
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
// Parses a group list from a raw buffer into the group map; returns false when the buffer runs short
bool RoutingGetGroupListOp::ParseGroupListObj(const void* data,unsigned long size)
{
    try {
        ReadBuffer msg(data,size);
        this->ParseReplyExceptForStatus(msg);
        return true;
    } catch(ReadBufferException&) {}
    return false;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c8f50
// (public: struct std::pair<class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<struct WONAPI::RoutingMemberInf)
