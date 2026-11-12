// WONAPI DBProxy ops that ask the server to email account information or a password (file grouping inferred)
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
// FUNCTION: WON 0x006ce090
// Creates a request to email the account information for an address
RequestAccountInfoEmailOp::RequestAccountInfoEmailOp(ServerContext* inContext)
    : DBProxyOp(inContext),mMsgType(14)
{
    this->Init();
}
// FUNCTION: WON 0x006ce130
// Destroys the request (members only)
RequestAccountInfoEmailOp::~RequestAccountInfoEmailOp() {}
// FUNCTION: WON 0x006ce280
// Checks that the reply answers this request; returns the status
WONStatus RequestAccountInfoEmailOp::CheckResponse()
{
    WONStatus status=DBProxyOp::CheckResponse();
    if (status!=WS_Success) return status;
    // the reply's check is Rust's (ee_won db.rs)
    if (!ee_won_db_answers(this->mSubMessageReplyType,this->mMsgType)) return this->InvalidReplyHeader();
    return WS_Success;
}
// FUNCTION: WON 0x006ce2b0
// Builds the request from the email address and profile, then starts it
void RequestAccountInfoEmailOp::RunHook()
{
    this->mMessageType=1002;
    this->mSubMessageType=this->mMsgType;
    // the request data is Rust's (ee_won db.rs)
    DbRequest q(EE_WON_DB_REQ_INFO_EMAIL,0);
    q.mReq.email=DbRequest::Narrow(this->mEmail);
    q.mReq.profile=this->mProfileData->GetProfile();
    this->mRequestData=q.Build();
    ServerRequestOp::RunHook();
}
// FUNCTION: WON 0x006ce3d0
// Sets 4-byte length fields and creates an empty profile
void RequestAccountInfoEmailOp::Init()
{
    this->mLengthFieldSize=4;
    this->mProfileData=new ProfileData;
}
// FUNCTION: WON 0x006ce480
// Creates a request to email a user's password
RequestPasswordEmailOp::RequestPasswordEmailOp(ServerContext* inContext)
    : DBProxyOp(inContext),mMsgType(8)
{
    this->Init();
}
// FUNCTION: WON 0x006ce520
// Destroys the request (members only)
RequestPasswordEmailOp::~RequestPasswordEmailOp() {}
// Same body as 0x6ce280 in the original (folded by the linker)

WONStatus RequestPasswordEmailOp::CheckResponse()
{
    WONStatus status=DBProxyOp::CheckResponse();
    if (status!=WS_Success) return status;
    // the reply's check is Rust's (ee_won db.rs)
    if (!ee_won_db_answers(this->mSubMessageReplyType,this->mMsgType)) return this->InvalidReplyHeader();
    return WS_Success;
}
// FUNCTION: WON 0x006ce670
// Builds the request from the user name and profile, then starts it
void RequestPasswordEmailOp::RunHook()
{
    this->mMessageType=1002;
    this->mSubMessageType=this->mMsgType;
    // the request data is Rust's (ee_won db.rs)
    DbRequest q(EE_WON_DB_REQ_PASSWORD_EMAIL,0);
    q.mReq.name=q.Text(this->mUserName);
    q.mReq.profile=this->mProfileData->GetProfile();
    this->mRequestData=q.Build();
    ServerRequestOp::RunHook();
}
// FUNCTION: WON 0x006ce790
// Sets 4-byte length fields and creates an empty profile
void RequestPasswordEmailOp::Init()
{
    this->mLengthFieldSize=4;
    this->mProfileData=new ProfileData;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ce110
// (protected: virtual void * __thiscall WONAPI::RequestAccountInfoEmailOp::`scalar deleting destructor'(unsigned int)    [??_GRequestAccountInfoEmailOp@W)
// TEMPLATE: WON 0x006ce500
// (protected: virtual void * __thiscall WONAPI::RequestPasswordEmailOp::`scalar deleting destructor'(unsigned int)    [??_GRequestPasswordEmailOp@WONAPI@)
