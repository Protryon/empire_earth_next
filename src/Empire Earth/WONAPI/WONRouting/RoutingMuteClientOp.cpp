// WONAPI routing op that mutes or unmutes a client in a group or on the server
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
#include "ee/wonapi/WONRoutingOpTypes.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x006c78c0
// Creates a mute request for the server-wide group, muting by WON id with no time limit
RoutingMuteClientOp::RoutingMuteClientOp(RoutingConnection* inConnection)
    : RoutingOp(inConnection), mGroupId(0xffff), mIsMuted(true),
      mMuteTime(0), mMuteByWONId(true), mWONIdIsCDKeyId(false),
      mWONId(0), mClientId(0)
{
}
// FUNCTION: WON 0x006c7a10
// Sends the routing server the mute/unmute request, naming the client by WON id, CD-key id or client id
void RoutingMuteClientOp::SendRequest()
{
    // the message is Rust's (ee_won routemsg.rs)
    RouteSendMute(this);
    this->SendMsg();
    this->AddOp();
}
// Accepts only the routing status reply and returns its status
// Same body as 0x6c8e90 in the original (folded by the linker)
WONStatus RoutingMuteClientOp::HandleReply(unsigned char inMsgType, ReadBuffer& inMsg)
{
    // the read is Rust's (ee_won routemsg.rs)
    return RouteStatusReply(inMsgType, inMsg);
}
// FUNCTION: WON 0x006c7930
// Returns the op type (mute client)
RoutingOpType RoutingMuteClientOp::GetType() const { return RoutingOp_MuteClient; }
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c7940
// (public: virtual void * __thiscall WONAPI::RoutingMuteClientOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingMuteClientOp@WONAPI@@UAEPAXI@)
// TEMPLATE: WON 0x006c7960
// (public: virtual __thiscall WONAPI::RoutingMuteClientOp::~RoutingMuteClientOp(void)    [??1RoutingMuteClientOp@WONAPI@@UAE@XZ])
