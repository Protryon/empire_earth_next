// WONAPI routing op that bans or unbans a client from a group or the server
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingOpTypes.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x006c4320
// Creates a ban request for the server-wide group, banning by WON id with no time limit
RoutingBanClientOp::RoutingBanClientOp(RoutingConnection* inConnection)
    : RoutingOp(inConnection), mGroupId(0xffff), mIsBanned(true),
      mBanTime(0), mBanByWONId(true), mWONIdIsCDKeyId(false),
      mWONId(0), mClientId(0)
{
}
// FUNCTION: WON 0x006c4470
// Sends the routing server the ban/unban request, naming the client by WON id, CD-key id or client id
void RoutingBanClientOp::SendRequest()
{
    this->InitSendMsg(3);
    this->mSendMsg.AppendShort(this->mGroupId);
    this->mSendMsg.AppendBool(this->mIsBanned);
    this->mSendMsg.AppendLong(this->mBanTime);
    this->mSendMsg.AppendWString(this->mBanComment, 2);
    if (this->mBanByWONId) {
        this->mSendMsg.AppendByte(this->mWONIdIsCDKeyId ? 3 : 2);
        this->mSendMsg.AppendLong(this->mWONId);
    } else {
        this->mSendMsg.AppendByte(1);
        this->mSendMsg.AppendShort(this->mClientId);
    }
    this->SendMsg();
    this->AddOp();
}
// FUNCTION: WON 0x006c8e90
// Accepts only the routing status reply and returns its status
WONStatus RoutingBanClientOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    if (inMsgType != 66) return (WONStatus)WS_RoutingOp_DontWantReply;
    return inMsg.ReadShort();
}
// FUNCTION: WON 0x006c4390
// Returns the op type (ban client)
RoutingOpType RoutingBanClientOp::GetType() const { return RoutingOp_BanClient; }
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c43a0
// (public: virtual void * __thiscall WONAPI::RoutingBanClientOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingBanClientOp@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006c43c0
// (public: virtual __thiscall WONAPI::RoutingBanClientOp::~RoutingBanClientOp(void)    [??1RoutingBanClientOp@WONAPI@@UAE@XZ])
