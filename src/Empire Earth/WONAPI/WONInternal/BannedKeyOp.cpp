// No original address identified: WON SDK key-server requests and replies
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include "ee/wonapi/WONBannedKeyOp.h"
#include "ee/wonapi/WONStatus.h"


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
	WriteBuffer requestData(0,true);
	requestData.AppendWString(this->mCommunityName,2);
	requestData.AppendString(this->mProductName);
	requestData.AppendLong(this->mBannedUntil);

	requestData.AppendShort(this->mKeyList.size());
	for(std::list<std::string>::const_iterator anItr = this->mKeyList.begin(); anItr != this->mKeyList.end(); ++anItr)
		requestData.AppendString(*anItr,1);

	// Copy buffer into the class
	this->mRequestData = requestData.ToByteBuffer(true);

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
	ReadBuffer theData(this->mReplyData->data(), this->mReplyData->length());
	try
	{
		unsigned short aListSize = theData.ReadShort();
		for(unsigned short i = 0; i < aListSize; i++)
			this->mStatusList.push_back(theData.ReadShort());
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
	WriteBuffer requestData(0,true);
	requestData.AppendWString(this->mCommunityName,2);
	requestData.AppendString(this->mProductName);

	// Copy buffer into the class
	this->mRequestData = requestData.ToByteBuffer(true);

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
	ReadBuffer theData(this->mReplyData->data(), this->mReplyData->length());
	try
	{
		unsigned short aListSize = theData.ReadShort();
		BANNED_KEY_PAIR aBannedKey;
		for(unsigned short i = 0; i < aListSize; i++)
		{
			theData.ReadString(aBannedKey.first,1);
			aBannedKey.second = theData.ReadLong();
			this->mKeyList.push_back(aBannedKey);
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
	WriteBuffer requestData(0,true);
	requestData.AppendWString(this->mCommunityName,2);
	requestData.AppendString(this->mProductName);

	requestData.AppendShort(this->mKeyList.size());
	for(std::list<std::string>::const_iterator anItr = this->mKeyList.begin(); anItr != this->mKeyList.end(); ++anItr)
		requestData.AppendString(*anItr,1);

	// Copy buffer into the class
	this->mRequestData = requestData.ToByteBuffer(true);

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
	ReadBuffer theData(this->mReplyData->data(), this->mReplyData->length());
	try
	{
		unsigned short aListSize = theData.ReadShort();
		BANNED_KEY_PAIR aBannedKey;
		for(unsigned short i = 0; i < aListSize; i++)
		{
			aBannedKey.first = theData.ReadLong();
			aBannedKey.second = theData.ReadShort();
			this->mKeyDataList.push_back(aBannedKey);
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
	WriteBuffer requestData(0,true);
	requestData.AppendWString(this->mCommunityName,2);
	requestData.AppendString(this->mProductName);

	requestData.AppendShort(this->mKeyList.size());
	for(std::list<std::string>::const_iterator anItr = this->mKeyList.begin(); anItr != this->mKeyList.end(); ++anItr)
		requestData.AppendString(*anItr,1);

	// Copy buffer into the class
	this->mRequestData = requestData.ToByteBuffer(true);

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
	ReadBuffer theData(this->mReplyData->data(), this->mReplyData->length());
	try
	{
		unsigned short aListSize = theData.ReadShort();
		for(unsigned short i = 0; i < aListSize; i++)
			this->mStatusList.push_back(theData.ReadShort());
	}
	catch (ReadBufferException&)
	{
		return (WONStatus)WS_InvalidMessage;
	}

	// Everything checked out
	return (WONStatus)WS_Success;
}
