// WONAPI op that reads the logged-in user's account (email and profile)
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
// FUNCTION: WON 0x006ccef0
// Sets 4-byte length fields and creates an empty profile
void GetAccountOp::Init()
{
    this->mLengthFieldSize=4;
    this->mProfileData=new ProfileData;
}

// FUNCTION: WON 0x006ccfa0
// Creates a get-account request that authenticates with the session of an auth context
GetAccountOp::GetAccountOp(ServerContext* inContext,AuthContext* inAuth)
    : ServerRequestOp(inContext)
{
    this->mAuthType=AUTH_TYPE_SESSION;
    this->mAuthContext=inAuth;
    this->Init();
}

// FUNCTION: WON 0x006cd190
// Builds the get-account message listing the profile fields wanted
WONStatus GetAccountOp::GetNextRequest()
{
    WriteBuffer aRequest(this->mLengthFieldSize,true);
    aRequest.AppendByte(5);
    aRequest.AppendShort(14);
    aRequest.AppendShort(1002);
    aRequest.AppendShort(6);
    this->mProfileData->PackRequestData(aRequest,false);
    this->mRequest=aRequest.ToByteBuffer(true);
    return WS_ServerReq_Recv;
}

// FUNCTION: WON 0x006cd2b0
// Checks the reply and reads the account's email and profile fields; returns the status
WONStatus GetAccountOp::CheckResponse()
{
    ReadBuffer aReply(this->mResponse->data(),this->mResponse->length());
    unsigned char aHeader=aReply.ReadByte();
    unsigned short aService=aReply.ReadShort();
    unsigned short aMessage=aReply.ReadShort();
    unsigned short aSubMessage=aReply.ReadShort();
    WONStatus aStatus=aReply.ReadShort();
    aReply.ReadString(this->mErrorString,2);
    aReply.ReadByte();
    if (aHeader!=5 || aService!=14)
        return this->InvalidReplyHeader();
    if (aStatus!=0)
        return aStatus;
    if (aMessage!=1002 || aSubMessage!=7)
        return this->InvalidReplyHeader();
    aReply.ReadString(this->mEmail,1);
    this->mProfileData->UnpackGetData(aReply);
    return WS_Success;
}
}

// No original address identified: WON SDK empty Reset
namespace WONAPI {
void GetAccountOp::Reset()
{
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006cd0a0
// (public: virtual __thiscall WONAPI::GetAccountOp::~GetAccountOp(void)    [??1GetAccountOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006cd080
// (public: virtual void * __thiscall WONAPI::GetAccountOp::`scalar deleting destructor'(unsigned int)    [??_GGetAccountOp@WONAPI@@UAEPAXI@Z])
