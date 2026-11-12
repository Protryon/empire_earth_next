// No original address identified: WON SDK directory server rename-entity request
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
void RenameEntityOp::Init()
{
	this->mLengthFieldSize = 4;
	this->mIsUnique = true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RenameEntityOp::RenameEntityOp(ServerContext *theDirContext, bool isService) : ServerRequestOp(theDirContext)
{
	this->mIsService = isService;
	this->Init();
}
   
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RenameEntityOp::RenameEntityOp(const IPAddr &theAddr, bool isService) : ServerRequestOp(theAddr)
{
	this->mIsService = isService;
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RenameEntityOp::GetNextRequest()
{
	// the message's bytes are Rust's (ee_won dirops.rs)
	DirRequestFields aFields(8, this->mIsService);
	aFields.mReq.path = aFields.Text(this->mPath);
	aFields.mReq.name = aFields.Text(this->mName);
	aFields.mReq.net_addr = DirRequestFields::Buffer(this->mNetAddr.get());
	aFields.mReq.is_unique = this->mIsUnique;
	aFields.mReq.new_display_name = aFields.Text(this->mNewDisplayName);
	this->mRequest = aFields.Build(this->mLengthFieldSize);
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RenameEntityOp::CheckResponse()
{
	// the reply's read is Rust's (ee_won dirops.rs)
	WONStatus aStatus = DirStatusReply(this->mResponse.get(), false, NULL);
	if(aStatus==(WONStatus)WS_ServerReq_InvalidReplyHeader)
		return this->InvalidReplyHeader();
	return aStatus;
}
