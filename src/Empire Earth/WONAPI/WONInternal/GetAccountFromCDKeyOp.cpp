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




#include "ee/wonapi/WONAdminMsg.h"
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
	AdminValues().Str(this->mCDKey).Num(this->mCommunity).AppendTo(EE_WON_ADMIN_ACCOUNT_FROM_CDKEY, requestData);
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

	// Extended unpack: the strings (ee_won admin.rs), then the profile; a short reply keeps what was read
	long aProfile = AdminAccountReply(*this, this->mReplyData.get());
	if (aProfile >= 0)
	{
		ReadBuffer theData(this->mReplyData->data(), this->mReplyData->length());
		theData.SetPos(aProfile);
		try
		{
			this->mProfileData->UnpackGetData(theData);
		}
		catch (ReadBufferException&)
		{
		}
	}
	return (WONStatus)WS_Success;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
