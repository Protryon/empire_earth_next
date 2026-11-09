// No original address identified: WON SDK key-server requests and replies
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include "ee/wonapi/WONMasterKeyOp.h"
#include "ee/wonapi/WONStatus.h"


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
	WriteBuffer requestData(0,true);
	requestData.AppendWString(this->mCommunityName,2);
	requestData.AppendString(this->mProductName);
	requestData.AppendShort(this->mSequenceNumber);
	requestData.AppendBool(this->mIsBeta);
	requestData.AppendLong(this->mActivateTime);
	requestData.AppendLong(this->mDeactivateTime);
	requestData.AppendByte(this->mKeyLength);
	requestData.AppendBytes(this->mKeyP, this->mKeyLength);
	requestData.AppendLong(this->mCDKeyFlags);

	// Copy buffer into the class
	this->mRequestData = requestData.ToByteBuffer(true);

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
	WriteBuffer requestData(0,true);
	requestData.AppendWString(this->mCommunityName,2);
	requestData.AppendString(this->mProductName);
	requestData.AppendShort(this->mSequenceNumber);
	requestData.AppendBool(this->mIsBeta);
	requestData.AppendLong(this->mActivateTime);
	requestData.AppendLong(this->mDeactivateTime);
	requestData.AppendLong(this->mCDKeyFlags);

	// Copy buffer into the class
	this->mRequestData = requestData.ToByteBuffer(true);

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
	WriteBuffer requestData(0,true);
	requestData.AppendWString(this->mCommunityName,2);
	requestData.AppendString(this->mProductName);
	requestData.AppendShort(this->mSequenceNumber);

	// Copy buffer into the class
	this->mRequestData = requestData.ToByteBuffer(true);

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
	WriteBuffer requestData(0,true);
	requestData.AppendWString(this->mCommunityName,2);
	requestData.AppendString(this->mProductName);

	// Copy buffer into the class
	this->mRequestData = requestData.ToByteBuffer(true);

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
	ReadBuffer theData(this->mReplyData->data(), this->mReplyData->length());

	try
	{
		MasterKey aKey;
		this->mMasterKeyList.clear();
		short aCount = theData.ReadShort();
		for(short i = 0; i < aCount; i++)
		{
			aKey.mSequenceNumber = theData.ReadShort();
			aKey.mActivateDate = theData.ReadLong();
			aKey.mDeactivateDate = theData.ReadLong();
			aKey.mIsBeta = (theData.ReadByte() == 0) ? false : true;
			aKey.mCDKeyFlags = theData.ReadLong();
			this->mMasterKeyList.push_back(aKey);
		}
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
	WriteBuffer requestData(0,true);
	requestData.AppendWString(this->mCommunityName, 1);
	requestData.AppendString(this->mProductName, 1);
	requestData.AppendShort(this->mKeyList.size());

	for(KEY_LIST_CITR anItr = this->mKeyList.begin(); anItr != this->mKeyList.end(); ++anItr)
		requestData.AppendString(*anItr, 1);

	// Copy buffer into the class
	this->mRequestData = requestData.ToByteBuffer(true);

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
	ReadBuffer theData(this->mReplyData->data(), this->mReplyData->length());

	try
	{
		this->mKeyStatusList.clear();
		short aCount = theData.ReadShort();
		for(short i = 0; i < aCount; i++)
			this->mKeyStatusList.push_back(theData.ReadShort());
	}
	catch (ReadBufferException&)
	{
		return (WONStatus)WS_InvalidMessage;
	}

	// Everything checked out
	return (WONStatus)WS_Success;
}
