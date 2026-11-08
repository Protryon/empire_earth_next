// No original address identified: WON SDK key-server requests and replies
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include "ee/wonapi/WONKeySubscribeOp.h"
#include "ee/wonapi/WONStatus.h"


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
	// Stores the message data
	theBufferR.AppendString(this->mProductName);
	theBufferR.AppendWString(this->mCommunityName,2);
	theBufferR.AppendShort(this->mKeyList.size());
	for(std::list<std::string>::iterator anItr = this->mKeyList.begin(); anItr != this->mKeyList.end(); ++anItr)
		theBufferR.AppendString(*anItr, 1);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus BaseKeySubscribeOp::Unpack(ReadBuffer& theBufferR)
{
	try
	{
		this->mStatusList.clear();
		unsigned short aListSize = theBufferR.ReadShort();
		for(unsigned short i = 0; i < aListSize; i++)
			this->mStatusList.push_back(theBufferR.ReadShort());
	}
	catch (ReadBufferException&)
	{
		return (WONStatus)WS_InvalidMessage;
	}

	// Everything checked out
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
	WriteBuffer requestData(0,true);
	BaseKeySubscribeOp::Pack(requestData);

	requestData.AppendLong(this->mAllocateDate);
	requestData.AppendLong(this->mAllocateDuration);
	requestData.AppendLong(this->mActivateTime);
	requestData.AppendLong(this->mActivateDuration);

	// Copy buffer into the class
	this->mRequestData = requestData.ToByteBuffer(true);

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
	WriteBuffer requestData(0,true);
	BaseKeySubscribeOp::Pack(requestData);

	requestData.AppendShort(this->mUpdateFields);
	requestData.AppendLong(this->mAllocateDate);
	requestData.AppendLong(this->mAllocateDuration);
	requestData.AppendLong(this->mActivateTime);
	requestData.AppendLong(this->mActivateDuration);
	requestData.AppendLong(this->mActivateUserSeq);

	// Copy buffer into the class
	this->mRequestData = requestData.ToByteBuffer(true);

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
	WriteBuffer requestData(0,true);
	BaseKeySubscribeOp::Pack(requestData);

	// Copy buffer into the class
	this->mRequestData = requestData.ToByteBuffer(true);

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
	WriteBuffer requestData(0,true);
	requestData.AppendString(this->mProductName);
	requestData.AppendWString(this->mCommunityName,2);
	requestData.AppendShort(this->mKeyList.size());
	for(std::list<std::string>::iterator anItr = this->mKeyList.begin(); anItr != this->mKeyList.end(); ++anItr)
		requestData.AppendString(*anItr, 1);

	// Copy buffer into the class
	this->mRequestData = requestData.ToByteBuffer(true);

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
	ReadBuffer theData(this->mReplyData->data(), this->mReplyData->length());
	try
	{
		KEY_DATA aKey;
		this->mKeyDataList.clear();
		short aListSize = theData.ReadShort();
		for(short i = 0; i < aListSize; i++)
		{
			aKey.mStatus = (WONStatus)theData.ReadShort();
			aKey.mAllocateDate = theData.ReadLong();
			aKey.mAllocateDuration = theData.ReadLong();
			aKey.mActivateTime = theData.ReadLong();
			aKey.mActivateDuration = theData.ReadLong();
			aKey.mActivateUserSeq = theData.ReadLong();
			this->mKeyDataList.push_back(aKey);
		}
	}
	catch (ReadBufferException&)
	{
		return (WONStatus)WS_InvalidMessage;
	}

	// Everything checked out
	return (WONStatus)WS_Success;
}
