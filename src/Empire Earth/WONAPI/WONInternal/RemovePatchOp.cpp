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

#include "ee/wonapi/WONAdminMsg.h"
using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RemovePatchOp::RemovePatchOp(ServerContext* theContext)
	: DBProxyOp(theContext),
	  mMsgType(13)
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RemovePatchOp::RemovePatchOp(const IPAddr& theAddr)
	: DBProxyOp(theAddr),
	  mMsgType(13)
{
	this->Init();
} 

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RemovePatchOp::CheckResponse()
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
void RemovePatchOp::RunHook()
{
	this->mMessageType=1004;
	this->mSubMessageType=this->mMsgType;

	// Pack the message data
	WriteBuffer requestData(0,true);
	AdminValues().Str(this->mProductName).Str(this->mConfigName).Str(this->mFromVersion).Str(this->mToVersion)
		.Str(this->mNetAddress).AppendTo(EE_WON_ADMIN_REMOVE_PATCH, requestData);		// the last: the patch url

	// Pack and call base class implementation
	this->mRequestData=requestData.ToByteBuffer(true);
	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RemovePatchOp::Init()
{
	this->Reset();
	ServerRequestOp::mLengthFieldSize = 4;

	this->mProductName	= "";
	this->mConfigName		= "";
	this->mFromVersion	= "";
	this->mToVersion		= "";
	this->mNetAddress		= "";		// patch url
}

