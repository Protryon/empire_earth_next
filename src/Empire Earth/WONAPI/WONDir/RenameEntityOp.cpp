// No original address identified: WON SDK directory server rename-entity request
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
void RenameEntityOp::Init()
{
	this->mLengthFieldSize = 4;
	this->mIsUnique = true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RenameEntityOp::RenameEntityOp(ServerContext *theDirContext, bool isService) : ServerRequestOp(theDirContext)
{
	this->mIsService = isService;
	this->Init();
}
   
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RenameEntityOp::RenameEntityOp(const IPAddr &theAddr, bool isService) : ServerRequestOp(theAddr)
{
	this->mIsService = isService;
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RenameEntityOp::GetNextRequest()
{
	WriteBuffer aMsg(this->mLengthFieldSize,true);
	aMsg.AppendByte(5);						// Small message
	aMsg.AppendShort(2);					// Directory Server
	aMsg.AppendShort(this->mIsService?207:206);	// RenameService : RenameDir
	aMsg.AppendBool(this->mIsUnique);				// Require unique display name
	aMsg.AppendWString(this->mPath,2);				// Directory path

	if(this->mIsService)
	{
		aMsg.AppendWString(this->mName,2);				// Entity name
		aMsg.AppendBuffer(this->mNetAddr,1);
	}

	aMsg.AppendWString(this->mNewDisplayName,2);

	this->mRequest = aMsg.ToByteBuffer(true);
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RenameEntityOp::CheckResponse()
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
