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
#include "ee/wonapi/WONDBMsg.h"
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
    // the request is Rust's (ee_won db.rs)
    DbRequest q(EE_WON_DB_REQ_UPDATE_ACCOUNT,this->mLengthFieldSize);
    q.mReq.password=q.Text(this->mNewPassword);
    q.mReq.email=DbRequest::Narrow(this->mEmail);
    q.mReq.profile=this->mProfileData->GetProfile();
    this->mRequest=q.Build();
    return WS_ServerReq_Recv;
}

// FUNCTION: WON 0x006cec80
// Checks the update reply; returns the status
WONStatus UpdateAccountOp::CheckResponse()
{
    // the reply's read is Rust's (ee_won db.rs)
    EeWonDbReply r=DbAccountReply(EE_WON_DB_UPDATE_ACCOUNT,this->mResponse.get(),this->mErrorString);
    if (r.verdict==EE_WON_DB_INVALID_HEADER)
        return this->InvalidReplyHeader();
    return (WONStatus)r.result;
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
