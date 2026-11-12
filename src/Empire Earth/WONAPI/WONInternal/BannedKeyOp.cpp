// No original address identified: WON SDK key-server requests and replies
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include "ee/wonapi/WONBannedKeyOp.h"
#include "ee/wonapi/WONStatus.h"


#include "ee/wonapi/WONAdminMsg.h"
using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
InsertBannedKeyOp::InsertBannedKeyOp(ServerContext* theContext)
	: DBProxyOp(theContext),
	  mCommunityName(L""),
	  mProductName(""),
	  mBannedUntil(0)
{
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void InsertBannedKeyOp::Reset()
{
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void InsertBannedKeyOp::RunHook()
{
	this->mMessageType = 1002;
	this->mSubMessageType = MSGTYPE;
	this->mStatusList.clear();

	// Stores the message data
	this->mRequestData = AdminInsertBannedKeyRequest(*this);

	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus InsertBannedKeyOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	// Make sure this is the expected reply type
	if (this->mSubMessageReplyType != this->mSubMessageType+1)
		return this->InvalidReplyHeader();

	// Extended unpack
	// Extended unpack: the statuses (ee_won admin.rs)
	if (!AdminStatusReply(EE_WON_ADMIN_INSERT_BANNED_KEY, this->mReplyData.get(), this->mStatusList, false))
		return (WONStatus)WS_InvalidMessage;
	return (WONStatus)WS_Success;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
QueryBannedKeyOp::QueryBannedKeyOp(ServerContext* theContext)
	: DBProxyOp(theContext),
	  mCommunityName(L""),
	  mProductName("")
{
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void QueryBannedKeyOp::RunHook()
{
	this->mMessageType = 1002;
	this->mSubMessageType = MSGTYPE;
	this->mKeyList.clear();

	// Stores the message data
	this->mRequestData = AdminQueryRequest(*this, EE_WON_ADMIN_QUERY_BANNED_KEY);

	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus QueryBannedKeyOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	// Make sure this is the expected reply type
	if (this->mSubMessageReplyType != this->mSubMessageType+1)
		return this->InvalidReplyHeader();

	// Extended unpack
	// Extended unpack: the banned keys (ee_won admin.rs)
	if (!AdminQueryBannedKeyReply(*this, this->mReplyData.get()))
		return (WONStatus)WS_InvalidMessage;
	return (WONStatus)WS_Success;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
QueryListBannedKeyOp::QueryListBannedKeyOp(ServerContext* theContext)
	: DBProxyOp(theContext),
	  mCommunityName(L""),
	  mProductName("")
{
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void QueryListBannedKeyOp::RunHook()
{
	this->mMessageType = 1002;
	this->mSubMessageType = MSGTYPE;
	this->mKeyDataList.clear();

	// Stores the message data
	this->mRequestData = AdminKeyListRequest(*this, EE_WON_ADMIN_QUERY_LIST_BANNED_KEY);

	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus QueryListBannedKeyOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	// Make sure this is the expected reply type
	if (this->mSubMessageReplyType != this->mSubMessageType+1)
		return this->InvalidReplyHeader();

	// Extended unpack
	// Extended unpack: the keys' data (ee_won admin.rs)
	if (!AdminQueryListBannedKeyReply(*this, this->mReplyData.get()))
		return (WONStatus)WS_InvalidMessage;
	return (WONStatus)WS_Success;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RemoveBannedKeyOp::RemoveBannedKeyOp(ServerContext* theContext)
	: DBProxyOp(theContext),
	  mCommunityName(L""),
	  mProductName("")
{
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RemoveBannedKeyOp::Reset()
{
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RemoveBannedKeyOp::RunHook()
{
	this->mMessageType = 1002;
	this->mSubMessageType = MSGTYPE;
	this->mStatusList.clear();

	// Stores the message data
	this->mRequestData = AdminKeyListRequest(*this, EE_WON_ADMIN_REMOVE_BANNED_KEY);

	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RemoveBannedKeyOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	// Make sure this is the expected reply type
	if (this->mSubMessageReplyType != this->mSubMessageType+1)
		return this->InvalidReplyHeader();

	// Extended unpack
	// Extended unpack: the statuses (ee_won admin.rs)
	if (!AdminStatusReply(EE_WON_ADMIN_REMOVE_BANNED_KEY, this->mReplyData.get(), this->mStatusList, false))
		return (WONStatus)WS_InvalidMessage;
	return (WONStatus)WS_Success;
}
