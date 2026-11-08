// No original address identified: WON SDK directory server get-service request
// Expected flags: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <winsock2.h>
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
	WriteBuffer aMsg(this->mLengthFieldSize,true);
	aMsg.AppendByte(5);						// Small Header
	aMsg.AppendShort(2);					// DirServer
	aMsg.AppendShort(105);					// GetServiceEx

	aMsg.AppendLong(this->mFlags);				// GetFlags
	aMsg.AppendWString(this->mPath,2);				// Directory Path
	aMsg.AppendWString(this->mName,2);				// Name of service
	aMsg.AppendBuffer(this->mNetAddr,1);			// Net address
	aMsg.AppendShort(this->mDataTypes.size());	// Number of data types requested
	
	DirDataTypeSet::iterator anItr = this->mDataTypes.begin();
	while(anItr!=this->mDataTypes.end())
	{
		aMsg.AppendString(*anItr,1);
		++anItr;
	}

	this->mRequest = aMsg.ToByteBuffer(true);
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
