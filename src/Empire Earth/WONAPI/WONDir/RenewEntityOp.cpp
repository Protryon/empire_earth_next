// No original address identified: WON SDK directory server renew-entity request
// Expected flags: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONDirExtended.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONDirRequest.h"
using namespace std;
using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenewEntityOp::Init()
{
	this->mLengthFieldSize = 4;
	this->mLifespan = 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RenewEntityOp::RenewEntityOp(ServerContext *theDirContext, bool isService) : ServerRequestOp(theDirContext)
{
	this->mIsService = isService;
	this->Init();
}
   
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RenewEntityOp::RenewEntityOp(const IPAddr &theAddr, bool isService) : ServerRequestOp(theAddr)
{
	this->mIsService = isService;
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RenewEntityOp::GetNextRequest()
{
	// the message's bytes are Rust's (ee_won dirops.rs)
	DirRequestFields aFields(9, this->mIsService);
	aFields.mReq.path = aFields.Text(this->mPath);
	aFields.mReq.name = aFields.Text(this->mName);
	aFields.mReq.net_addr = DirRequestFields::Buffer(this->mNetAddr.get());
	aFields.mReq.lifespan = (uint32_t)this->mLifespan;
	this->mRequest = aFields.Build(this->mLengthFieldSize);
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RenewEntityOp::CheckResponse()
{
	// the reply's read is Rust's (ee_won dirops.rs)
	WONStatus aStatus = DirStatusReply(this->mResponse.get(), false, NULL);
	if(aStatus==(WONStatus)WS_ServerReq_InvalidReplyHeader)
		return this->InvalidReplyHeader();
	return aStatus;
}
