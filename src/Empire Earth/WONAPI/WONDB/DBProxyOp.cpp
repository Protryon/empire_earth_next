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
    WriteBuffer aRequest(this->mLengthFieldSize,true);
    aRequest.AppendByte(5);
    aRequest.AppendShort(14);
    aRequest.AppendShort(this->mMessageType);
    aRequest.AppendShort(this->mSubMessageType);
    aRequest.AppendBuffer(this->mRequestData.get(),0);
    this->mRequest=aRequest.ToByteBuffer(true);
    return WS_ServerReq_Recv;
}
// FUNCTION: WON 0x006ccd30
// Collects reply data across messages until the last-reply flag; then returns the server status
WONStatus DBProxyOp::CheckResponse()
{
    ReadBuffer aReply(this->mResponse->data(),this->mResponse->length());
    unsigned char aHeader=aReply.ReadByte();
    unsigned short aService=aReply.ReadShort();
    this->mMessageReplyType=aReply.ReadShort();
    this->mSubMessageReplyType=aReply.ReadShort();
    WONStatus aStatus=aReply.ReadShort();
    aReply.ReadString(this->mReplyErrorString,2);
    unsigned char aFlags=aReply.ReadByte();
    if (aHeader!=5 || aService!=14)
        return this->InvalidReplyHeader();
    if (aReply.mPos<aReply.length())
        this->mReceivedData.AppendBytes(aReply.mData+aReply.mPos,aReply.length()-aReply.mPos);
    if ((aFlags&0x80)!=0) {
        this->mReplyData=this->mReceivedData.ToByteBuffer(true);
        return aStatus;
    }
    return WS_ServerReq_Recv;
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
