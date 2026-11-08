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
RemoveVersionOp::RemoveVersionOp(ServerContext* theContext)
	: DBProxyOp(theContext),
	  mMsgType(5),
	  mProductName("")
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RemoveVersionOp::RemoveVersionOp(const IPAddr& theAddr)
	: DBProxyOp(theAddr),
	  mMsgType(5),
	  mProductName("")
{
	this->Init();
} 

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RemoveVersionOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	if (this->mSubMessageReplyType != this->mMsgType+1)
		return this->InvalidReplyHeader();
	
	// Do extended unpack



	// Finished
	return (WONStatus)WS_Success;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RemoveVersionOp::RunHook()
{
	this->mMessageType=1004;
	this->mSubMessageType=this->mMsgType;

	// Pack the message data
	WriteBuffer requestData(0,true);
	requestData.AppendString(this->mProductName);
	requestData.AppendString(this->mConfigName);
	requestData.AppendString(this->mVersion);

	// Pack and call base class implementation
	this->mRequestData=requestData.ToByteBuffer(true);
	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RemoveVersionOp::Init()
{
	this->Reset();
	ServerRequestOp::mLengthFieldSize = 4;

	this->mConfigName = "";
	this->mVersion = "";
}
