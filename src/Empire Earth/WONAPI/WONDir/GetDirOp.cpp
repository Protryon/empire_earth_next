// WONAPI op that lists a directory on the directory server
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x006f1190
// Sets 4-byte length fields and creates the entity request
void GetDirOp::Init()
{
    this->mLengthFieldSize=4;
    this->mGetEntityRequest=new GetEntityRequest;
}
// FUNCTION: WON 0x006f12d0
// Creates a get-directory request for a server context
GetDirOp::GetDirOp(ServerContext* context) : ServerRequestOp(context)
{
    this->Init();
}
// FUNCTION: WON 0x006f13c0
// Forgets the entities received so far
void GetDirOp::Reset()
{
    this->mGetEntityRequest->Reset();
}
// FUNCTION: WON 0x006f13d0
// Builds the get-directory message from the path, flags and data types
WONStatus GetDirOp::GetNextRequest()
{
    WriteBuffer message(this->mLengthFieldSize,true);
    message.AppendByte(5);
    message.AppendShort(2);
    message.AppendShort(103);
    this->mGetEntityRequest->Pack(&message,GetEntityRequest::Pack_GetDirOp);
    this->mRequest=message.ToByteBuffer(true);
    return (WONStatus)WS_ServerReq_Recv;
}
// FUNCTION: WON 0x006f14e0
// Parses a reply into the entity list; returns the status (more replies, done or bad header)
WONStatus GetDirOp::CheckResponse()
{
    WONStatus status=this->mGetEntityRequest->ParseMultiEntityReply(this->mResponse->data(),this->mResponse->length());
    if(status==(WONStatus)100063) return this->InvalidReplyHeader();
    return status;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f1330
// (public: virtual void * __thiscall WONAPI::GetDirOp::`scalar deleting destructor'(unsigned int)    [??_GGetDirOp@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006f1350
// (public: virtual __thiscall WONAPI::GetDirOp::~GetDirOp(void)    [??1GetDirOp@WONAPI@@UAE@XZ])
