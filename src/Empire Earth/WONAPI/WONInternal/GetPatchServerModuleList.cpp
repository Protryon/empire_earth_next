// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified: WON SDK patch/version DBProxy operation
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONPatchAdmin.h"
#include "ee/wonapi/WONStatus.h"


using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GetPatchServerModuleListOp::GetPatchServerModuleListOp(ServerContext* theContext)
	: DBProxyOp(theContext),
	  mMsgType(1)
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GetPatchServerModuleListOp::GetPatchServerModuleListOp(const IPAddr& theAddr)
	: DBProxyOp(theAddr),
	  mMsgType(1)
{
	this->Init();
} 

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Exceptions handled by ServerRequestOp
WONStatus GetPatchServerModuleListOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	if (this->mMessageReplyType != 7)
		return this->InvalidReplyHeader();
	
	// Do extended unpack
	ReadBuffer aReadBuf(this->mReplyData->data(), this->mReplyData->length());
	unsigned short numModules = aReadBuf.ReadShort();

	// Read in each module
	for (int i=0; i < numModules; i++)
	{
		unsigned long moduleID = aReadBuf.ReadLong();
		this->mModuleList.push_back(moduleID);
	}

	// Finished
	return (WONStatus)WS_Success;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetPatchServerModuleListOp::RunHook()
{
	this->mMessageType=6;
	this->mSubMessageType=this->mMsgType;

	// Call base class implementation
	DBProxyOp::RunHook();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetPatchServerModuleListOp::Init()
{
	this->Reset();
	ServerRequestOp::mLengthFieldSize = 4;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool GetPatchServerModuleListOp::IsModuleAvailable(unsigned short theMsgType, unsigned short theSubMsgType)
{
	std::list<unsigned long>::const_iterator anItr = this->mModuleList.begin();
	for (; anItr != this->mModuleList.end(); ++anItr)
	{
		// Search for this message type and sub message type
		if (theMsgType == LOWORD(*anItr) && theSubMsgType == HIWORD(*anItr))
			return true;
	}

	return false;
}
