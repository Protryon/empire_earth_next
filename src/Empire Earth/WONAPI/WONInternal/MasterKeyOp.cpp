// No original address identified: WON SDK key-server requests and replies
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include "ee/wonapi/WONMasterKeyOp.h"
#include "ee/wonapi/WONStatus.h"


#include "ee/wonapi/WONAdminMsg.h"
using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
InsertMasterKeyOp::InsertMasterKeyOp(ServerContext* theContext)
	: DBProxyOp(theContext),
	  mCommunityName(L""),
	  mProductName(""),
	  mSequenceNumber(0),
	  mIsBeta(false),
	  mActivateTime(0),
	  mDeactivateTime(0),
	  mKeyLength(0),
	  mCDKeyFlags(0x0001)
{
	memset(this->mKeyP, '\0', 20);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void InsertMasterKeyOp::RunHook()
{
	this->mMessageType = 1002;
	this->mSubMessageType = MSGTYPE;

	// Stores the message data
	this->mRequestData = AdminInsertMasterKeyRequest(*this);

	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus InsertMasterKeyOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	// Make sure this is the expected reply type
	if (this->mSubMessageReplyType != this->mSubMessageType+1)
		return this->InvalidReplyHeader();

	// Extended unpack: None

	// Everything checked out
	return (WONStatus)WS_Success;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
UpdateMasterKeyOp::UpdateMasterKeyOp(ServerContext* theContext)
	: DBProxyOp(theContext),
	  mCommunityName(L""),
	  mProductName(""),
	  mSequenceNumber(0),
	  mIsBeta(false),
	  mActivateTime(0),
	  mDeactivateTime(0),
	  mCDKeyFlags(0x0001)
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void UpdateMasterKeyOp::RunHook()
{
	this->mMessageType = 1002;
	this->mSubMessageType = MSGTYPE;

	// Stores the message data
	this->mRequestData = AdminUpdateMasterKeyRequest(*this);

	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus UpdateMasterKeyOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	// Make sure this is the expected reply type
	if (this->mSubMessageReplyType != this->mSubMessageType+1)
		return this->InvalidReplyHeader();

	// Extended unpack: None

	// Everything checked out
	return (WONStatus)WS_Success;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
DeleteMasterKeyOp::DeleteMasterKeyOp(ServerContext* theContext)
	: DBProxyOp(theContext),
	  mCommunityName(L""),
	  mProductName(""),
	  mSequenceNumber(0)
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void DeleteMasterKeyOp::RunHook()
{
	this->mMessageType = 1002;
	this->mSubMessageType = MSGTYPE;

	// Stores the message data
	this->mRequestData = AdminDeleteMasterKeyRequest(*this);

	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus DeleteMasterKeyOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	// Make sure this is the expected reply type
	if (this->mSubMessageReplyType != this->mSubMessageType+1)
		return this->InvalidReplyHeader();

	// Extended unpack: None

	// Everything checked out
	return (WONStatus)WS_Success;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
QueryMasterKeyOp::QueryMasterKeyOp(ServerContext* theContext)
	: DBProxyOp(theContext),
	  mCommunityName(L""),
	  mProductName("")
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void QueryMasterKeyOp::RunHook()
{
	this->mMessageType = 1002;
	this->mSubMessageType = MSGTYPE;

	// Stores the message data
	this->mRequestData = AdminQueryRequest(*this, EE_WON_ADMIN_QUERY_MASTER_KEY);

	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus QueryMasterKeyOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	// Make sure this is the expected reply type
	if (this->mSubMessageReplyType != this->mSubMessageType+1)
		return this->InvalidReplyHeader();

	// Extended unpack
	// Extended unpack: the master keys (ee_won admin.rs)
	if (!AdminQueryMasterKeyReply(*this, this->mReplyData.get()))
		return (WONStatus)WS_InvalidMessage;
	return (WONStatus)WS_Success;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
AuthCheckKeyOp::AuthCheckKeyOp(ServerContext* theContext)
	: DBProxyOp(theContext),
	  mCommunityName(L""),
	  mProductName("")
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void AuthCheckKeyOp::RunHook()
{
	this->mMessageType = 1002;
	this->mSubMessageType = MSGTYPE;

	// Stores the message data
	this->mRequestData = AdminAuthCheckKeyRequest(*this);

	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus AuthCheckKeyOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	// Make sure this is the expected reply type
	if (this->mSubMessageReplyType != this->mSubMessageType+1)
		return this->InvalidReplyHeader();

	// Extended unpack
	// Extended unpack: the key statuses (ee_won admin.rs)
	if (!AdminStatusReply(EE_WON_ADMIN_AUTH_CHECK_KEY, this->mReplyData.get(), this->mKeyStatusList, true))
		return (WONStatus)WS_InvalidMessage;
	return (WONStatus)WS_Success;
}
