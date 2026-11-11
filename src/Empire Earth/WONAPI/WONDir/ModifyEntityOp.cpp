// No original address identified: WON SDK directory server modify-entity request
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
void ModifyEntityOp::Init()
{
	this->mFlags = DIR_UF_UNIQUEDISPLAYNAME;
	this->mLengthFieldSize = 4;
	this->mNewLifespan = 0;
	this->mIsService = true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ModifyEntityOp::ModifyEntityOp(ServerContext *theDirContext, bool isService) : ServerRequestOp(theDirContext)
{
	this->Init();
	this->mIsService = isService;
}
   
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ModifyEntityOp::ModifyEntityOp(const IPAddr &theAddr, bool isService) : ServerRequestOp(theAddr)
{
	this->Init();
	this->mIsService = isService;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus ModifyEntityOp::GetNextRequest()
{
	// the message's bytes are Rust's (ee_won dirops.rs)
	DirRequestFields aFields(6, this->mIsService);
	aFields.mReq.path = aFields.Text(this->mPath);
	aFields.mReq.name = aFields.Text(this->mName);
	aFields.mReq.net_addr = DirRequestFields::Buffer(this->mNetAddr.get());
	aFields.mReq.flags = this->mFlags;
	aFields.mReq.new_name = aFields.Text(this->mNewName);
	aFields.mReq.new_net_addr = DirRequestFields::Buffer(this->mNewNetAddr.get());
	aFields.mReq.new_display_name = aFields.Text(this->mNewDisplayName);
	aFields.mReq.lifespan = (uint32_t)this->mNewLifespan;
	aFields.Objects(this->mDataObjects);
	this->mRequest = aFields.Build(this->mLengthFieldSize);
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus ModifyEntityOp::CheckResponse()
{
	// the reply's read is Rust's (ee_won dirops.rs)
	WONStatus aStatus = DirStatusReply(this->mResponse.get(), this->mIsService && (this->mFlags & DIR_UF_SERVRETURNADDR)!=0, &this->mNetAddr);
	if(aStatus==(WONStatus)WS_ServerReq_InvalidReplyHeader)
		return this->InvalidReplyHeader();
	return aStatus;
}
