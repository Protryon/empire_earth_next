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
GetVersionListOp::GetVersionListOp(const std::string& theProductName, ServerContext* theContext)
	: DBProxyOp(theContext),
	  mMsgType(9),
	  mProductName(theProductName),
	  mStateFilter(VersionState_BadState)
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GetVersionListOp::GetVersionListOp(const std::string& theProductName, const IPAddr& theAddr)
	: DBProxyOp(theAddr),
	  mMsgType(9),
	  mProductName(theProductName)
{
	this->Init();
} 

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Exceptions handled by ServerRequestOp
WONStatus GetVersionListOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	if (this->mSubMessageReplyType != this->mMsgType+1)
		return this->InvalidReplyHeader();
	
	// Do extended unpack
	// Do extended unpack: each version record read by VersionData (ee_won admin.rs, patch.rs)
	AdminVersionListReply<VersionData>(this->mReplyData.get(), this->mVersionDataList);
	return (WONStatus)WS_Success;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetVersionListOp::RunHook()
{
	this->mMessageType=1004;
	this->mSubMessageType=this->mMsgType;

	// Pack the message data
	WriteBuffer requestData(0,true);
	AdminValues().Str(this->mProductName).Str(this->mConfigName).Num(this->mStateFilter)
		.AppendTo(EE_WON_ADMIN_GET_VERSION_LIST, requestData);
	

	// Pack and call base class implementation
	this->mRequestData=requestData.ToByteBuffer(true);
	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetVersionListOp::Init()
{
	this->Reset();
	ServerRequestOp::mLengthFieldSize = 4;

	this->mConfigName = "%";	// % means get every config
	this->mStateFilter = VersionState_BadState;

}
