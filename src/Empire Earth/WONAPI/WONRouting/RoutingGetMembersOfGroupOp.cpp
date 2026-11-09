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
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x6c6320
// Clears the member map and asks the routing server for the members of the group
void RoutingGetMembersOfGroupOp::SendRequest()
{
    this->mMembers.clear();
    this->InitSendMsg(30);
    this->mSendMsg.AppendShort(this->mGroupId);
    this->SendMsg();
    this->AddOp();
}

// FUNCTION: WON 0x6c63f0
// Parses the member list reply for this group into the member map, with client names/flags when sent
WONStatus RoutingGetMembersOfGroupOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 29) return (WONStatus)WS_RoutingOp_DontWantReply;
    WONStatus status = inMsg.ReadShort();
    unsigned short groupId = inMsg.ReadShort();
    if (groupId != this->mGroupId) return (WONStatus)WS_RoutingOp_DontWantReply;
    if (status == 0) {

        unsigned char flags = inMsg.ReadByte();
        this->mHasClientNames = (flags & 1) != 0;
        this->mHasClientFlags = (flags & 2) != 0;
        this->mObserverCount = inMsg.ReadShort();
        int count = (unsigned short)inMsg.ReadShort();
        for (int i = 0; i < count; ++i) {
            unsigned short length = inMsg.ReadShort();
            long start = inMsg.pos();
            RoutingMemberInfoPtr member = this->GetNewMemberInfo();
            member->mClientId = inMsg.ReadShort();
            member->mFlags = inMsg.ReadByte(); // read as a byte
            this->mMembers[member->mClientId] = member;
            if (flags != 0) member->mClientInfo = this->GetNewClientInfo();
            if (this->mHasClientNames) inMsg.ReadWString(member->mClientInfo->mName, 2);
            if (this->mHasClientFlags) member->mClientInfo->mFlags = inMsg.ReadLong();
            inMsg.ReadBytes(length - inMsg.pos() + start);
        }
    }
    return status;
}
}
