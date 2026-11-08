// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified: WON SDK account-from-CD-key DBProxy operation
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONAccountFromCDKey.h"
#include "ee/wonapi/WONStatus.h"




using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GetAccountFromCDKeyOp::GetAccountFromCDKeyOp(ServerContext* theContext, std::string theCDKey, unsigned long theCommunity)
	: DBProxyOp(theContext),
	  mCDKey(theCDKey),
	  mCommunity(theCommunity),
	  mUserName(L""),
	  mPassword(L""),
	  mEmail(""),
	  mLastLogin("")
{
	this->mProfileData = new ProfileData();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetAccountFromCDKeyOp::Reset()
{
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetAccountFromCDKeyOp::RunHook()
{
	this->mMessageType=1002;
	this->mSubMessageType=MSGTYPE;

	// Stores the message data
	WriteBuffer requestData(0,true);

	// Append the CDKey (string)
	requestData.AppendString(this->mCDKey,2);

	// Append the community (unsigned long)
	requestData.AppendLong(this->mCommunity);
	
	// Append the profile data
	this->mProfileData->PackRequestData(requestData,true);

	// Copy buffer into the class
	this->mRequestData=requestData.ToByteBuffer(true);

	DBProxyOp::RunHook();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus GetAccountFromCDKeyOp::CheckResponse()
{
	// Call the base class implementation
	WONStatus result = DBProxyOp::CheckResponse();
	if (result != (WONStatus)WS_Success)
		return result;
	
	// Make sure this is the expected reply type
	if (this->mSubMessageReplyType != this->mSubMessageType+1)
		return this->InvalidReplyHeader();

	// Extended unpack:
	// [WString] UserName
	// [WString] Password
	// [String ] Email

	ReadBuffer theData(this->mReplyData->data(), this->mReplyData->length());

	try
	{
		theData.ReadWString(this->mUserName,2);
		theData.ReadString (this->mEmail,2);
		theData.ReadWString(this->mPassword,2);
		theData.ReadString (this->mLastLogin,2);
		theData.ReadString (this->mActivated,2);

		// Unpack the profile data (custom actions)
		this->mProfileData->UnpackGetData(theData); 
	}
	catch (ReadBufferException&)
	{
	}

	// Everything checked out
	return (WONStatus)WS_Success;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
