// WONAPI op that updates the logged-in user's password, email and profile
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
// FUNCTION: WON 0x006ce840
// Sets 4-byte length fields and creates an empty profile
void UpdateAccountOp::Init()
{
    this->mLengthFieldSize=4;
    this->mProfileData=new ProfileData;
}

// FUNCTION: WON 0x006ce8f0
// Creates an update-account request that authenticates with the session of an auth context
UpdateAccountOp::UpdateAccountOp(ServerContext* inContext,AuthContext* inAuth)
    : ServerRequestOp(inContext)
{
    this->mAuthType=AUTH_TYPE_SESSION;
    this->mAuthContext=inAuth;
    this->Init();
}

// FUNCTION: WON 0x006ceb30
// Builds the update message: new password, email and profile fields
WONStatus UpdateAccountOp::GetNextRequest()
{
    WriteBuffer aRequest(this->mLengthFieldSize,true);
    aRequest.AppendByte(5);
    aRequest.AppendShort(14);
    aRequest.AppendShort(1002);
    aRequest.AppendShort(4);
    aRequest.AppendWString(this->mNewPassword,2);
    aRequest.AppendString(this->mEmail,2);
    this->mProfileData->PackRequestData(aRequest,true);
    this->mRequest=aRequest.ToByteBuffer(true);
    return WS_ServerReq_Recv;
}

// FUNCTION: WON 0x006cec80
// Checks the update reply; returns the status
WONStatus UpdateAccountOp::CheckResponse()
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
    if (aMessage!=1002 || aSubMessage!=5)
        return this->InvalidReplyHeader();
    return WS_Success;
}
}

// No original address identified: WON SDK empty Reset
namespace WONAPI {
void UpdateAccountOp::Reset()
{
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006cea10
// (public: virtual __thiscall WONAPI::UpdateAccountOp::~UpdateAccountOp(void)    [??1UpdateAccountOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ce9f0
// (public: virtual void * __thiscall WONAPI::UpdateAccountOp::`scalar deleting destructor'(unsigned int)    [??_GUpdateAccountOp@WONAPI@@UAEPAXI@Z])
