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
ActivatePatchOp::ActivatePatchOp(ServerContext* theContext)
	: DBProxyOp(theContext),
	  mMsgType(15)
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ActivatePatchOp::ActivatePatchOp(const IPAddr& theAddr)
	: DBProxyOp(theAddr),
	  mMsgType(15)
{
	this->Init();
} 

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus ActivatePatchOp::CheckResponse()
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
void ActivatePatchOp::RunHook()
{
	this->mMessageType=1004;
	this->mSubMessageType=this->mMsgType;

	// Pack the message data
	WriteBuffer requestData(0,true);
	requestData.AppendString(this->mProductName);
	requestData.AppendString(this->mConfigName);
	requestData.AppendString(this->mFromVersion);
	requestData.AppendString(this->mToVersion);
	requestData.AppendString(this->mNetAddress);		// patch url
	requestData.AppendByte(this->mIsActive);

	// Pack and call base class implementation
	this->mRequestData=requestData.ToByteBuffer(true);
	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ActivatePatchOp::Init()
{
	this->Reset();
	ServerRequestOp::mLengthFieldSize = 4;

	this->mProductName	= "";
	this->mConfigName		= "";
	this->mFromVersion	= "";
	this->mToVersion		= "";
	this->mNetAddress		= "";		// patch url
	this->mIsActive		= FALSE;
}

