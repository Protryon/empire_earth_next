// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// GetPatchListOp: patch list request and reply through the DB proxy server (WON SDK)
// No original address identified (file grouping inferred)
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONPatchProtocol.h"
#include "ee/wonapi/WONStatus.h"

#include "ee/wonapi/WONPatchMsg.h"
using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GetPatchListOp::GetPatchListOp(ServerContext* theContext)
	: DBProxyOp(theContext),
	  mMsgType(17)
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GetPatchListOp::GetPatchListOp(const IPAddr& theAddr)
	: DBProxyOp(theAddr),
	  mMsgType(17)
{
	this->Init();
} 

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GetPatchListOp::~GetPatchListOp() // Safety Lock -- Enforces Smart Ptr Use
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus GetPatchListOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	if (this->mSubMessageReplyType != this->mMsgType+1)
		return this->InvalidReplyHeader();
	
	// Do extended unpack
	PatchReplyData<PatchData,PatchDataList,VersionData>(this->mReplyData.get(),NULL,NULL,NULL,this->mPatchDataList);

	// Finished
	return (WONStatus)WS_Success;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetPatchListOp::RunHook()
{
	this->mMessageType=1004;
	this->mSubMessageType=this->mMsgType;

	// Pack the message data
	std::vector<std::string> aStrings;
	aStrings.push_back(this->mProductName);
	aStrings.push_back(this->mConfigName);
	aStrings.push_back(this->mFromVersion);
	aStrings.push_back(this->mToVersion);

	// Pack and call base class implementation
	this->mRequestData=PatchRequest(1,aStrings,this->mStateFilter);
	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetPatchListOp::Init()
{
	this->Reset();
	ServerRequestOp::mLengthFieldSize = 4;

	this->mProductName	= "";
	this->mConfigName		= "";
	this->mFromVersion	= "";
	this->mToVersion		= "";
	this->mStateFilter	= 2;
}

