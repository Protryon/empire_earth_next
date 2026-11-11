// No original address identified: WON SDK alias directory request/response
// Expected flags: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONDirAlias.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONDirRequest.h"


//#include "DirEntityReplyParser.h"

using namespace std;
using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetAliasDirOp::Init()
{
	this->mLengthFieldSize   = 4;
	this->mNumFailedRequests = 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

GetAliasDirOp::GetAliasDirOp(ServerContext *theDirContext) : ServerRequestOp(theDirContext)
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GetAliasDirOp::GetAliasDirOp(const IPAddr &theAddr) : ServerRequestOp(theAddr)
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetAliasDirOp::Reset()
{
	GetEntityRequestList::const_iterator anItr = this->mRequestList.begin();
	for (;anItr != this->mRequestList.end(); ++anItr)
		(*anItr)->Reset();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetAliasDirOp::AddRequest(GetEntityRequest* theRequest)
{
	this->mRequestList.push_back(theRequest);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetAliasDirOp::AddService(const std::wstring& theService)
{
	GetEntityRequestPtr pRequest = new GetEntityRequest;
	pRequest->SetPath(theService);
	pRequest->SetFlags(this->mFlags);
	pRequest->SetDataTypes(this->mDataTypes);

	this->AddRequest(pRequest);
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus GetAliasDirOp::GetNextRequest()
{
	// the message's bytes are Rust's (ee_won dirops.rs)
	DirRequestFields aFields(2, true);
	aFields.mReq.product = DirRequestFields::Narrow(this->mProductName);
	GetEntityRequestList::const_iterator anItr = this->mRequestList.begin();
	for (;anItr != this->mRequestList.end(); ++anItr)
		aFields.AddQuery((*anItr)->GetFlags(),(*anItr)->GetPath(),(*anItr)->GetDataTypes());
	this->mRequest = aFields.Build(this->mLengthFieldSize);
	// Prepare the iterator for Recv
	this->mCurRequest = this->mRequestList.begin();
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Parse the dir entities (One MultiEntityReply for each Request)
WONStatus GetAliasDirOp::CheckResponse()
{
	// Was the request list empty?
	if (this->mCurRequest == this->mRequestList.end())
	{
		// Construct a new request to parse out the status result
		GetEntityRequestPtr aRequest = new GetEntityRequest;
		return aRequest->ParseMultiEntityReply(this->mResponse->data(),this->mResponse->length());
	}

	(*this->mCurRequest)->ParseMultiEntityReply(this->mResponse->data(), this->mResponse->length());
	// the accounting is Rust's (ee_won dirops.rs)
	int32_t aFailed = this->mNumFailedRequests;
	if (ee_won_dir_multi_step((*this->mCurRequest)->GetStatus(), &aFailed))
		++this->mCurRequest;
	this->mNumFailedRequests = aFailed;
	// Are all requests complete?
	if (this->mCurRequest == this->mRequestList.end())
		return (WONStatus)ee_won_dir_multi_end(this->mNumFailedRequests, this->mRequestList.size());
	else
		return (WONStatus)WS_ServerReq_Recv;

}
