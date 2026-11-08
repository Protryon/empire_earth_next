// No original address identified: WON SDK directory server add-entity request
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
void AddEntityOp::Init()
{
	this->mFlags = DIR_UF_UNIQUEDISPLAYNAME;
	this->mLengthFieldSize = 4;
	this->mLifespan = 0;
	this->mIsService = true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
AddEntityOp::AddEntityOp(ServerContext *theDirContext, bool isService) : ServerRequestOp(theDirContext)
{
	this->Init();
	this->mIsService = isService;
}
   
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
AddEntityOp::AddEntityOp(const IPAddr &theAddr, bool isService) : ServerRequestOp(theAddr)
{
	this->Init();
	this->mIsService = isService;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void AddServiceOp::SetNetAddrPort(unsigned short thePort)
{
	WriteBuffer theBuf(0,true);
	theBuf.AppendShort( htons(thePort) );

	this->SetNetAddr(theBuf.ToByteBuffer(true));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus AddEntityOp::GetNextRequest()
{
	WriteBuffer aMsg(this->mLengthFieldSize,true);
	aMsg.AppendByte(5);						// Small message
	aMsg.AppendShort(2);					// Directory Server
	aMsg.AppendShort(this->mIsService?215:214);	// AddServiceEx : AddDirectoryEx
	aMsg.AppendByte(this->mFlags);				// Entity Flags
	aMsg.AppendWString(this->mPath,2);				// Directory path
	aMsg.AppendWString(this->mName,2);				// Entity name

	if(this->mIsService)
		aMsg.AppendBuffer(this->mNetAddr,1);

	aMsg.AppendWString(this->mDisplayName,2);		// Entity display name
	aMsg.AppendLong(this->mLifespan);				// Entity lifespan
	aMsg.AppendShort(this->mDataObjects.size());	// num data objects
	
	DirDataObjectList::iterator anItr = this->mDataObjects.begin();
	while(anItr!=this->mDataObjects.end())
	{
		DirDataObject &aDataObject = *anItr;
		aMsg.AppendString(aDataObject.mDataType,1);
		aMsg.AppendBuffer(aDataObject.mData,2);
		++anItr;
	}

	aMsg.AppendShort(0); // num permissions
	this->mRequest = aMsg.ToByteBuffer(true);
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus AddEntityOp::CheckResponse()
{
	ReadBuffer aMsg(this->mResponse->data(),this->mResponse->length());
	unsigned char aHeaderType = aMsg.ReadByte();
	unsigned short aServiceType = aMsg.ReadShort();
	unsigned short aMessageType = aMsg.ReadShort();

	if(aHeaderType!=5 || aServiceType!=2 || aMessageType!=1)
		return this->InvalidReplyHeader();


	short aStatus = aMsg.ReadShort();
	if(aStatus>=0 && this->mIsService)
	{
		if((this->mFlags & DIR_UF_SERVRETURNADDR)!=0)
	{
			unsigned char aLen = aMsg.ReadByte();
			this->mNetAddr = new ByteBuffer(aMsg.ReadBytes(aLen),aLen,false);
		}
	}

	return (WONStatus)aStatus;
}
