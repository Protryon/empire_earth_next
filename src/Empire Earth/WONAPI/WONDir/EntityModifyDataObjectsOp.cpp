// No original address identified: WON SDK directory server modify-data-objects request
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
void EntityModifyDataObjectsOp::Init()
{
	this->mLengthFieldSize = 4;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
EntityModifyDataObjectsOp::EntityModifyDataObjectsOp(ServerContext *theDirContext, bool isService) : ServerRequestOp(theDirContext)
{
	this->mIsService = isService;
	this->Init();
}
   
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
EntityModifyDataObjectsOp::EntityModifyDataObjectsOp(const IPAddr &theAddr, bool isService) : ServerRequestOp(theAddr)
{
	this->mIsService = isService;
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus EntityModifyDataObjectsOp::GetNextRequest()
{
	WriteBuffer aMsg(this->mLengthFieldSize,true);
	aMsg.AppendByte(5);						// Small message
	aMsg.AppendShort(2);					// Directory Server
	aMsg.AppendShort(this->mIsService?305:304);	// ServiceModifyDataObjects : DirModifyDataObjects
	aMsg.AppendWString(this->mPath,2);				// Directory path

	if(this->mIsService)
	{
		aMsg.AppendWString(this->mName,2);				// Entity name
		aMsg.AppendBuffer(this->mNetAddr,1);
	}

	aMsg.AppendShort(this->mDataObjectMods.size());
	DataObjectModList::iterator anItr = this->mDataObjectMods.begin();
	while(anItr!=this->mDataObjectMods.end())
	{
		aMsg.AppendShort(anItr->mOffset);
		aMsg.AppendBool(anItr->mIsInsert);
		aMsg.AppendString(anItr->mObject.mDataType,1);
		aMsg.AppendBuffer(anItr->mObject.mData,2);
		++anItr;
	}

	this->mRequest = aMsg.ToByteBuffer(true);
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus EntityModifyDataObjectsOp::CheckResponse()
{
	ReadBuffer aMsg(this->mResponse->data(),this->mResponse->length());
	unsigned char aHeaderType = aMsg.ReadByte();
	unsigned short aServiceType = aMsg.ReadShort();
	unsigned short aMessageType = aMsg.ReadShort();

	if(aHeaderType!=5 || aServiceType!=2 || aMessageType!=1)
		return this->InvalidReplyHeader();


	short aStatus = aMsg.ReadShort();
	return (WONStatus)aStatus;
}
