// No original address identified: WON SDK directory server find-entity request
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
void FindEntityOp::Init()
{
	this->mLengthFieldSize = 4;
	this->mIsService = true;

	this->mGetFlags = DIR_GF_DECOMPROOT | DIR_GF_DECOMPSERVICES | DIR_GF_DECOMPSUBDIRS | DIR_GF_ADDTYPE
		| DIR_GF_ADDDISPLAYNAME | DIR_GF_ADDDATAOBJECTS | DIR_GF_SERVADDNAME | DIR_GF_SERVADDNETADDR 
		| DIR_GF_DIRADDNAME;

	this->mMatchMode = DIR_FMM_EXACT;
	this->mFindFlags = 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
FindEntityOp::FindEntityOp(ServerContext *theDirContext, bool isService) : ServerRequestOp(theDirContext)
{
	this->Init();
	this->mIsService = isService;
}
   
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
FindEntityOp::FindEntityOp(const IPAddr &theAddr, bool isService) : ServerRequestOp(theAddr)
{
	this->Init();
	this->mIsService = isService;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FindEntityOp::Reset()
{
	this->mService = NULL;
	this->mDirEntityList.clear();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus FindEntityOp::GetNextRequest()
{
	// the message's bytes are Rust's (ee_won dirops.rs)
	DirRequestFields aFields(4, this->mIsService);
	aFields.mReq.flags = this->mGetFlags;
	aFields.mReq.mode = (uint8_t)this->mMatchMode;
	aFields.mReq.find_flags = this->mFindFlags;
	aFields.mReq.path = aFields.Text(this->mPath);
	aFields.mReq.name = aFields.Text(this->mName);
	aFields.mReq.net_addr = DirRequestFields::Buffer(this->mNetAddr.get());
	aFields.mReq.display_name = aFields.Text(this->mDisplayName);
	aFields.Objects(this->mFindDataObjects);
	aFields.Types(this->mGetDataTypes);
	this->mRequest = aFields.Build(this->mLengthFieldSize);
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus FindEntityOp::CheckResponse()
{
	WONStatus aStatus;
	if(ee_won_dir_find_single(this->mIsService, this->mFindFlags))
	{
		aStatus = DirEntityReplyParser::ParseSingleEntityReply(this->mResponse->data(),this->mResponse->length(),this->mService);
		if(aStatus==(WONStatus)0)
			this->mDirEntityList.push_back(this->mService);
	}
	else
	{
		aStatus = DirEntityReplyParser::ParseMultiEntityReply(this->mResponse->data(),this->mResponse->length(),this->mDirEntityList);
		if(aStatus==(WONStatus)0 && this->mIsService && !this->mDirEntityList.empty())
			this->mService = this->mDirEntityList.front();
	}

	if(aStatus==(WONStatus)100063)
		return this->InvalidReplyHeader();
	else
		return aStatus;
}
