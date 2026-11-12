// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified: WON SDK authenticated contest request/reply
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONContestCallOp.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONDBMsg.h"
using namespace WONAPI;

void ContestCallOp::Init()
{
	this->mProcNum = 0;
	this->mLengthFieldSize = 2;
	this->mUseAuth2 = false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ContestCallOp::ContestCallOp(ServerContext *theContestContext) : ServerRequestOp(theContestContext)
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ContestCallOp::ContestCallOp(const IPAddr &theAddr) : ServerRequestOp(theAddr)
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ContestCallOp::RunHook()
{
	if(this->mAuthType==AUTH_TYPE_NONE)
	{
		this->Finish((WONStatus)WS_ServerReq_NeedAuthContext);
		return;
	}

	ServerRequestOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus ContestCallOp::GetNextRequest()
{
	// the request is Rust's (ee_won db.rs)
	DbRequest q(EE_WON_DB_REQ_CONTEST,this->mLengthFieldSize);
	q.mReq.user_id=(uint32_t)this->mAuthContext->GetPeerData()->GetCertificate()->GetUserId(); // WONUserSeq
	q.mReq.message=(short)this->mProcNum; // Contest DB Procedure number
	q.mReq.data=DbRequest::Buffer(this->mRequestData.get());
	this->mRequest = q.Build();
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus ContestCallOp::CheckResponse()
{
	// the reply's read is Rust's (ee_won db.rs)
	bool anInvalid=false;
	WONStatus aStatus=DbContestReply(this->mResponse.get(),&this->mReplyData,&anInvalid);
	if(anInvalid)
		return this->InvalidReplyHeader();
	return aStatus;
}

