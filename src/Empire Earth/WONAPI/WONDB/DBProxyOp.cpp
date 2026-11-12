// WONAPI DBProxy request op (file grouping inferred)

// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONDBMsg.h"
namespace WONAPI {
// Direct-address form of the constructor; nothing in this build calls it.
DBProxyOp::DBProxyOp(const IPAddr& theAddr)
	: ServerRequestOp(theAddr),
	  mMessageType(0),
	  mMessageReplyType(0),
	  mSubMessageType(0),
	  mSubMessageReplyType(0),
      mReceivedData(0,true),
	  mReplyErrorString("")
{

}
// FUNCTION: WON 0x006cca10
// Creates a DBProxy request for a server context with no message types set
DBProxyOp::DBProxyOp(ServerContext* inContext)
    : ServerRequestOp(inContext), mMessageType(0), mMessageReplyType(0),
      mSubMessageType(0), mSubMessageReplyType(0), mReceivedData(0,true),
      mReplyErrorString("")
{
}
// FUNCTION: WON 0x006ccc00
// Builds the DBProxy message (message and sub-message type plus the request data)
WONStatus DBProxyOp::GetNextRequest()
{
    // the request is Rust's (ee_won db.rs)
    DbRequest q(EE_WON_DB_REQ_PROXY, this->mLengthFieldSize);
    q.mReq.message = this->mMessageType;
    q.mReq.sub = this->mSubMessageType;
    q.mReq.data = DbRequest::Buffer(this->mRequestData.get());
    this->mRequest=q.Build();
    return WS_ServerReq_Recv;
}
// FUNCTION: WON 0x006ccd30
// Collects reply data across messages until the last-reply flag; then returns the server status
WONStatus DBProxyOp::CheckResponse()
{
    // the reply's read is Rust's (ee_won db.rs)
    bool anInvalid=false;
    WONStatus aStatus=DbProxyReply(this->mResponse.get(),this->mMessageReplyType,this->mSubMessageReplyType,
                                   this->mReplyErrorString,this->mReceivedData,this->mReplyData,&anInvalid);
    if (anInvalid)
        return this->InvalidReplyHeader();
    return aStatus;
}
}

// No original address identified: WON SDK Reset
namespace WONAPI {
void DBProxyOp::Reset()
{
	// Clear all received data
	//mReplyData->Release();

	ServerRequestOp::Reset();
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ccb20
// (public: virtual __thiscall WONAPI::DBProxyOp::~DBProxyOp(void)    [??1DBProxyOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ccb00
// (public: virtual void * __thiscall WONAPI::DBProxyOp::`scalar deleting destructor'(unsigned int)    [??_GDBProxyOp@WONAPI@@UAEPAXI@Z])
