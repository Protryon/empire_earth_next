// Connectionless server request that fetches a routing server's group list
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONRoutingMsg.h"
namespace WONAPI {
// FUNCTION: WON 0x006c5ff0
// Creates a group list request to a routing server address with the given list flags
RoutingGetGroupListReqOp::RoutingGetGroupListReqOp(const IPAddr& inAddress,unsigned char inFlags)
 : ServerRequestOp(inAddress),mOp(new RoutingGetGroupListOp)
{
    this->mLengthFieldSize=2;
    this->mOp->SetFlags(inFlags);
}
// FUNCTION: WON 0x006c6150
// Builds the get-group-list request message and asks for the reply next
WONStatus RoutingGetGroupListReqOp::GetNextRequest()
{
    // the request is Rust's (ee_won routemsg.rs)
    this->mRequest=RouteGroupListRequest(this->mLengthFieldSize,this->mOp->mFlags);
    return WS_ServerReq_Recv;
}
// FUNCTION: WON 0x006c6270
// Checks the reply header and status and parses the group list into the inner op; returns the status
WONStatus RoutingGetGroupListReqOp::CheckResponse()
{
    // the read is Rust's (ee_won routemsg.rs): the header, the status and the group list into the inner op
    bool anInvalidHeader=false;
    WONStatus aStatus=RouteGroupListReqReply(this->mOp.get(),this->mResponse.get(),&anInvalidHeader);
    if(anInvalidHeader)
        return this->InvalidReplyHeader();
    return aStatus;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c60c0
// (public: virtual void * __thiscall WONAPI::RoutingGetGroupListReqOp::`scalar deleting destructor'(unsigned int)    [??_GRoutingGetGroupListReqOp@WONAPI)
// TEMPLATE: WON 0x006c60e0
// (public: virtual __thiscall WONAPI::RoutingGetGroupListReqOp::~RoutingGetGroupListReqOp(void)    [??1RoutingGetGroupListReqOp@WONAPI@@UAE@XZ])
