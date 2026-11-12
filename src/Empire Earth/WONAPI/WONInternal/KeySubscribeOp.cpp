// No original address identified: WON SDK key-server requests and replies
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include "ee/wonapi/WONKeySubscribeOp.h"
#include "ee/wonapi/WONStatus.h"


#include "ee/wonapi/WONAdminMsg.h"
using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
BaseKeySubscribeOp::BaseKeySubscribeOp(ServerContext* theContext)
	: DBProxyOp(theContext),
	  mCommunityName(L""),
	  mProductName("")
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void BaseKeySubscribeOp::Pack(WriteBuffer& theBufferR)
{
	AdminSubscribeValues(*this).AppendTo(EE_WON_ADMIN_REMOVE_KEY_SUBSCRIBE, theBufferR);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus BaseKeySubscribeOp::Unpack(ReadBuffer& theBufferR)
{
	// the statuses of the reader's remaining bytes (ee_won admin.rs)
	ConstSmartPtr<ByteBuffer> aRest = new ByteBuffer(theBufferR.data() + theBufferR.pos(), theBufferR.Available(), false);
	if (!AdminStatusReply(EE_WON_ADMIN_INSERT_KEY_SUBSCRIBE, aRest.get(), this->mStatusList, true))
		return (WONStatus)WS_InvalidMessage;
	return (WONStatus)WS_Success;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
InsertKeySubscribeOp::InsertKeySubscribeOp(ServerContext* theContext)
	: BaseKeySubscribeOp(theContext),
	mAllocateDate(-1),
	mAllocateDuration(0),
	mActivateTime(-1),
	mActivateDuration(0)
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void InsertKeySubscribeOp::RunHook()
{
	this->mMessageType = 1006;
	this->mSubMessageType = MSGTYPE;

	// Stores the message data
	AdminValues aValues = AdminSubscribeValues(*this);
	aValues.Num(this->mAllocateDate).Num(this->mAllocateDuration).Num(this->mActivateTime).Num(this->mActivateDuration);
	this->mRequestData = aValues.Build(EE_WON_ADMIN_INSERT_KEY_SUBSCRIBE);

	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus InsertKeySubscribeOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	// Make sure this is the expected reply type
	if (this->mSubMessageReplyType != this->mSubMessageType+1)
		return this->InvalidReplyHeader();

	// Extended unpack
	ReadBuffer theData(this->mReplyData->data(), this->mReplyData->length());
	return BaseKeySubscribeOp::Unpack(theData);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
UpdateKeySubscribeOp::UpdateKeySubscribeOp(ServerContext* theContext)
	: BaseKeySubscribeOp(theContext),
	mUpdateFields(0),
	mAllocateDate(-1),
	mAllocateDuration(0),
	mActivateTime(-1),
	mActivateDuration(0),
	mActivateUserSeq(0)
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void UpdateKeySubscribeOp::RunHook()
{
	this->mMessageType = 1006;
	this->mSubMessageType = MSGTYPE;

	// Stores the message data
	AdminValues aValues = AdminSubscribeValues(*this);
	aValues.Num(this->mUpdateFields).Num(this->mAllocateDate).Num(this->mAllocateDuration).Num(this->mActivateTime)
		.Num(this->mActivateDuration).Num(this->mActivateUserSeq);
	this->mRequestData = aValues.Build(EE_WON_ADMIN_UPDATE_KEY_SUBSCRIBE);

	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus UpdateKeySubscribeOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	// Make sure this is the expected reply type
	if (this->mSubMessageReplyType != this->mSubMessageType+1)
		return this->InvalidReplyHeader();

	// Extended unpack
	ReadBuffer theData(this->mReplyData->data(), this->mReplyData->length());
	return BaseKeySubscribeOp::Unpack(theData);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RemoveKeySubscribeOp::RemoveKeySubscribeOp(ServerContext* theContext)
	: BaseKeySubscribeOp(theContext)
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RemoveKeySubscribeOp::RunHook()
{
	this->mMessageType = 1006;
	this->mSubMessageType = MSGTYPE;

	// Stores the message data
	this->mRequestData = AdminSubscribeValues(*this).Build(EE_WON_ADMIN_REMOVE_KEY_SUBSCRIBE);

	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus RemoveKeySubscribeOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	// Make sure this is the expected reply type
	if (this->mSubMessageReplyType != this->mSubMessageType+1)
		return this->InvalidReplyHeader();

	// Extended unpack
	ReadBuffer theData(this->mReplyData->data(), this->mReplyData->length());
	return BaseKeySubscribeOp::Unpack(theData);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
QueryKeySubscribeOp::QueryKeySubscribeOp(ServerContext* theContext)
	: DBProxyOp(theContext)
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void QueryKeySubscribeOp::RunHook()
{
	this->mMessageType = 1006;
	this->mSubMessageType = MSGTYPE;

	// Stores the message data
	this->mRequestData = AdminSubscribeValues(*this).Build(EE_WON_ADMIN_QUERY_KEY_SUBSCRIBE);

	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus QueryKeySubscribeOp::CheckResponse()
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
	if (!AdminQueryKeySubscribeReply(*this, this->mReplyData.get()))
		return (WONStatus)WS_InvalidMessage;
	return (WONStatus)WS_Success;
}
