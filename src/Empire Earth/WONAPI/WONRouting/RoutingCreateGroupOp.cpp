// WONAPI routing op that creates a group (and optionally joins it)
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

// FUNCTION: WON 0x006c4a80
// Creates a create-group request with no group id, no parent group and no flags
RoutingCreateGroupOp::RoutingCreateGroupOp(RoutingConnection* inConnection)
    : RoutingOp(inConnection)
{
    this->mGroupId = 0xffff;
    this->mParentGroupId = 0xffff;
    this->mMaxPlayers = 0;
    this->mJoinFlags = 0;
    this->mGroupFlags = 0;
    this->mAsyncFlags = 0;
    this->mLeaveGroupOnCancel = false;
}

// FUNCTION: WON 0x006c4c20
// Sends the routing server the create-group request (name, password, size, join and group flags, parent)
void RoutingCreateGroupOp::SendRequest()
{
    this->InitSendMsg(13);
    this->mSendMsg.AppendShort(this->mGroupId);
    this->mSendMsg.AppendWString(this->mGroupName, 2);
    this->mSendMsg.AppendWString(this->mGroupPassword, 2);
    this->mSendMsg.AppendShort(this->mMaxPlayers);
    this->mSendMsg.AppendByte(this->mJoinFlags);
    this->mSendMsg.AppendLong(this->mGroupFlags);
    this->mSendMsg.AppendLong(this->mAsyncFlags);
    this->mSendMsg.AppendShort(this->mParentGroupId);
    this->SendMsg();
    this->AddOp();
}

// FUNCTION: WON 0x006c4d90
// Reads the create-group reply (new group id and name, parent) and the join status that follows when joining
WONStatus RoutingCreateGroupOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType == 41) {
        if (this->mJoinFlags == 0)
            return (WONStatus)WS_RoutingOp_DontWantReply;
        short status = inMsg.ReadShort();
        this->CheckLeaveGroup(status);
        return status;
    }
    if (inMsgType != 12)
        return (WONStatus)WS_RoutingOp_DontWantReply;
    WONStatus status = inMsg.ReadShort();
    if (status == 0) {
        this->mGroupId = inMsg.ReadShort();
        inMsg.ReadWString(this->mGroupName, 2);
        this->CheckLeaveGroup((short)status);
        if (this->mJoinFlags != 0)
            return (WONStatus)WS_RoutingOp_NeedMoreReplies;
    }
    // The original compares the reader positions unsigned; the members are signed, so the cast stays
    if ((unsigned long)inMsg.pos() < inMsg.length())
        this->mParentGroupId = inMsg.ReadShort();
    return status;
}

// FUNCTION: WON 0x006c4cb0
// Leaves the new group again (async) when the op already ended before success arrived and leave-on-cancel is set
void RoutingCreateGroupOp::CheckLeaveGroup(short inStatus)
{
    if (inStatus != 0 || this->mStatus == (WONStatus)WS_Pending || !this->mLeaveGroupOnCancel)
        return;
    this->mLeaveGroupOnCancel = false;
    SmartPtr<RoutingLeaveGroupOp> op = new RoutingLeaveGroupOp(this->mConnection.get(),this->mGroupId);
    op->RunAsync(-1);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c4b40
// (public: virtual __thiscall WONAPI::RoutingCreateGroupOp::~RoutingCreateGroupOp(void)    [??1RoutingCreateGroupOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c4b10
// (public: virtual long __thiscall WONAPI::RoutingCreateGroupOp::GetType(void)const     [?GetType@RoutingCreateGroupOp@WONAPI@@UBEJXZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c4b20
// (public: virtual void * __thiscall WONAPI::RoutingCreateGroupOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingCreateGroupOp@WONAPI@@UAEPAX)
