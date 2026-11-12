// No original address identified: WON SDK directory server get-service request
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
void GetServiceOp::Init()
{
	this->mLengthFieldSize = 4;

	this->mFlags = DIR_GF_DECOMPSERVICES | DIR_GF_ADDDISPLAYNAME | DIR_GF_ADDDATAOBJECTS | 
		DIR_GF_SERVADDNAME | DIR_GF_SERVADDNETADDR;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

GetServiceOp::GetServiceOp(ServerContext *theDirContext) : ServerRequestOp(theDirContext)
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GetServiceOp::GetServiceOp(const IPAddr &theAddr) : ServerRequestOp(theAddr)
{
	this->Init();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetServiceOp::Reset()
{
	this->mService = NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus GetServiceOp::GetNextRequest()
{
	// the message's bytes are Rust's (ee_won dirops.rs)
	DirRequestFields aFields(3, true);
	aFields.mReq.flags = this->mFlags;
	aFields.mReq.path = aFields.Text(this->mPath);
	aFields.mReq.name = aFields.Text(this->mName);
	aFields.mReq.net_addr = DirRequestFields::Buffer(this->mNetAddr.get());
	aFields.Types(this->mDataTypes);
	this->mRequest = aFields.Build(this->mLengthFieldSize);
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus GetServiceOp::CheckResponse()
{
	WONStatus aStatus = DirEntityReplyParser::ParseSingleEntityReply(this->mResponse->data(),this->mResponse->length(),this->mService);
	if(aStatus==(WONStatus)WS_ServerReq_InvalidReplyHeader)
		return this->InvalidReplyHeader();
	else
		return aStatus;
}
