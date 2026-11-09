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
	// Once per op
	WriteBuffer aMsg(this->mLengthFieldSize,true);
	aMsg.AppendByte(5);						// Small Header
	aMsg.AppendShort(2);					// DirServer
	aMsg.AppendShort(114);					// AliasGetEntity

	aMsg.AppendShort(0);					// MaxEntitiesPerReply
	aMsg.AppendString(this->mProductName);
	aMsg.AppendByte(this->mRequestList.size());	// Number of entities requests

	// Once per request
	GetEntityRequestList::const_iterator anItr = this->mRequestList.begin();
	for (;anItr != this->mRequestList.end(); ++anItr)
	{	
		(*anItr)->Pack(&aMsg, GetEntityRequest::Pack_GetMultiDirOp);
	}
	this->mRequest = aMsg.ToByteBuffer(true);

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
		
	// Does this request have pending data?
	if ((*this->mCurRequest)->GetStatus() != (WONStatus)100059)
	{
		if ((*this->mCurRequest)->GetStatus() != (WONStatus)0)
			++this->mNumFailedRequests;
		++this->mCurRequest;
	}

	// Are all requests complete?
	if (this->mCurRequest == this->mRequestList.end())
	{
		if (this->mNumFailedRequests == 0)
			return (WONStatus)WS_Success;
		else if (this->mNumFailedRequests != this->mRequestList.size())
			return (WONStatus)-1223;
		else
			return (WONStatus)-1224;
	}
	else
		return (WONStatus)WS_ServerReq_Recv;

}
