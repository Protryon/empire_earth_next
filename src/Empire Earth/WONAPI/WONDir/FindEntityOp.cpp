// No original address identified: WON SDK directory server find-entity request
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
	WriteBuffer aMsg(this->mLengthFieldSize,true);
	aMsg.AppendByte(5);						// Small message
	aMsg.AppendShort(2);					// Directory Server
	aMsg.AppendShort(this->mIsService?109:107);	// FindServiceEx : FindDirectoryEx
	
	aMsg.AppendLong(this->mGetFlags);				
	aMsg.AppendByte(this->mMatchMode);
	aMsg.AppendByte(this->mFindFlags);
	aMsg.AppendShort(0);					// MaxEntitiesPerReply
	aMsg.AppendWString(this->mPath,2);				// Directory path
	aMsg.AppendWString(this->mName,2);				// Match name

	if(this->mIsService)
		aMsg.AppendBuffer(this->mNetAddr,1);		// Match NetAddress

	aMsg.AppendWString(this->mDisplayName,2);		// Match DisplayName

	aMsg.AppendShort(this->mFindDataObjects.size());	// num data objects
	
	DirDataObjectList::iterator aFindItr = this->mFindDataObjects.begin();
	while(aFindItr!=this->mFindDataObjects.end())
	{
		DirDataObject &aDataObject = *aFindItr;
		aMsg.AppendString(aDataObject.mDataType,1);
		aMsg.AppendBuffer(aDataObject.mData,2);
		
		++aFindItr;
	}

	aMsg.AppendShort(this->mGetDataTypes.size()); // Num Get DataObjects
	DirDataTypeSet::iterator aGetItr = this->mGetDataTypes.begin();
	while(aGetItr!=this->mGetDataTypes.end())
	{
		aMsg.AppendString(*aGetItr,1);
		++aGetItr;
	}

	this->mRequest = aMsg.ToByteBuffer(true);
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus FindEntityOp::CheckResponse()
{
	WONStatus aStatus;
	if(this->mIsService && !(this->mFindFlags & DIR_FF_MATCHALL))
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
