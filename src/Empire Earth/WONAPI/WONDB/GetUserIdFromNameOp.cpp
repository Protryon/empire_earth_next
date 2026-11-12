// WONAPI DBProxy op that looks up a user's WON id by name
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
extern wchar_t gEmptyWideString[]; // 0x8e60bc, an empty wide string (zero .bss; defined in EEWonNet.cpp)
namespace WONAPI {
// FUNCTION: WON 0x006cd3a0
// Creates a user id lookup with an empty name
GetUserIdFromNameOp::GetUserIdFromNameOp(ServerContext* inContext)
    : DBProxyOp(inContext),mUserName(gEmptyWideString),mUserId(0)
{
}
// FUNCTION: WON 0x006cd4d0
// Destroys the lookup (members only)
GetUserIdFromNameOp::~GetUserIdFromNameOp()
{
}
// FUNCTION: WON 0x006cd600
// Builds the lookup request for the user name and starts it
void GetUserIdFromNameOp::RunHook()
{
    this->mMessageType=1002;
    this->mSubMessageType=16;
    // the request data is Rust's (ee_won db.rs)
    DbRequest q(EE_WON_DB_REQ_USER_ID,0);
    q.mReq.name=q.Text(this->mUserName);
    this->mRequestData=q.Build();
    ServerRequestOp::RunHook();
}
// FUNCTION: WON 0x006cd700
// Checks the reply and reads the user's WON id; returns the status
WONStatus GetUserIdFromNameOp::CheckResponse()
{
    WONStatus result=DBProxyOp::CheckResponse();
    if (result!=WS_Success) return result;
    // the reply's read is Rust's (ee_won db.rs)
    if (!ee_won_db_answers(this->mSubMessageReplyType,16)) return this->InvalidReplyHeader();
    this->mUserId=DbUserId(this->mReplyData.get());
    return WS_Success;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006cd4b0
// (protected: virtual void * __thiscall WONAPI::GetUserIdFromNameOp::`scalar deleting destructor'(unsigned int)    [??_GGetUserIdFromNameOp@WONAPI@@MAEPA)
