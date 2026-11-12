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
#include "ee/wonapi/WONDBMsg.h"
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
    // the request is Rust's (ee_won db.rs)
    DbRequest q(EE_WON_DB_REQ_GET_ACCOUNT,this->mLengthFieldSize);
    q.mReq.profile=this->mProfileData->GetProfile();
    this->mRequest=q.Build();
    return WS_ServerReq_Recv;
}

// FUNCTION: WON 0x006cd2b0
// Checks the reply and reads the account's email and profile fields; returns the status
WONStatus GetAccountOp::CheckResponse()
{
    // the reply's read is Rust's (ee_won db.rs)
    EeWonDbReply r=DbAccountReply(EE_WON_DB_GET_ACCOUNT,this->mResponse.get(),this->mErrorString,&this->mEmail);
    if (r.verdict==EE_WON_DB_INVALID_HEADER)
        return this->InvalidReplyHeader();
    if (r.email!=2)
        return (WONStatus)r.result;
    ReadBuffer aReply(this->mResponse->data(),this->mResponse->length());
    aReply.SetPos((int)r.profile_at);
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
