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




using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GetAccountFromCDKeyExOp::GetAccountFromCDKeyExOp(ServerContext* theContext, std::string theCDKey, unsigned long theCommunity)
	: GetAccountFromCDKeyOp(theContext,theCDKey,theCommunity),
	  mActionFlag(RESET)
{
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GetAccountFromCDKeyExOp::RunHook()
{
	this->mMessageType=1002;
	this->mSubMessageType=MSGTYPE;

	// Stores the message data
	WriteBuffer requestData(0,true);

	// Append the CDKey (string)
	requestData.AppendString(this->mCDKey,2);

	// Append the community (unsigned long)
	requestData.AppendLong(this->mCommunity);
	
	// Append the Reset or Ban flag
	requestData.AppendByte(this->mActionFlag);

	// Append the profile data
	this->mProfileData->PackRequestData(requestData,true);

	// Copy buffer into the class
	this->mRequestData=requestData.ToByteBuffer(true);

	DBProxyOp::RunHook();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
const char* GetAccountFromCDKeyExOp::GetActionString(ACTIONFLAG actionFlag)
{
	switch(actionFlag)
	{
	case RESET:
		return "RESET";
	case BAN:
		return "BAN";
	default:
		return "Unknown Action";
	}
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
