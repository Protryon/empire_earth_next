// No original address identified: WON SDK registry authentication settings; the password's cipher and the port's
// default are ee_won's (rust/crates/ee_won/src/miscops.rs)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include "ee/wonapi/WONRegAuth.h"
#include "ee/wonapi/WONMiscOpsMsg.h"



using std::string;
using WONAPI::Blowfish;
using WONAPI::ByteBufferPtr;
using WONAPI::RegKey;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool WONAPI::ReadAuthSettings(string& theAuthAddressR, 
							  unsigned short& theAuthPortR, 
							  string& theLoginNameR, 
							  string& theCommunityNameR, 
							  string& thePasswordR)
{
	RegKey aKey(1, KEY_TOOLS_AUTH, false, true);
	if(aKey.IsOpen())
	{
		aKey.GetValue(VALUE_AUTHSERVERIP, theAuthAddressR);
		// QueryDWORD clears the port when the value is missing; a key that does not exist kept 15200, and every
		// key opens now (config.yaml)
		DWORD anAuthPort(15200);
		int aResult = aKey.QueryDWORD(VALUE_AUTHSERVERPORT, anAuthPort);
		theAuthPortR = ee_won_regauth_port(aResult, anAuthPort);
		aKey.GetValue(VALUE_USERNAME, theLoginNameR);
		aKey.GetValue(VALUE_COMMUNITY, theCommunityNameR);

		ByteBufferPtr anEncryptP;
		aKey.QueryBinary(VALUE_PASSWORD, anEncryptP);
		if(anEncryptP.get() != NULL)
		{
			std::string aPassword;
			if(RegAuthPassword(true, anEncryptP->data(), anEncryptP->length(), aPassword))
			{
				thePasswordR = aPassword;
				return true;
			}
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool WONAPI::WriteAuthSettings(const string& theAuthAddress, 
							   unsigned short theAuthPort, 
							   const string& theLoginName, 
							   const string& theCommunityName, 
							   const string& thePassword)
{
	RegKey aKey(1, KEY_TOOLS_AUTH, true, true);
	if(!aKey.IsOpen())
		return false;
	aKey.SetValue(VALUE_AUTHSERVERIP, theAuthAddress);
	aKey.SetValue(VALUE_AUTHSERVERPORT, theAuthPort);
	aKey.SetValue(VALUE_USERNAME, theLoginName);
	aKey.SetValue(VALUE_COMMUNITY, theCommunityName);

	if(!thePassword.empty())
	{
		std::string aCrypt;
		if(RegAuthPassword(false, thePassword.data(), thePassword.size(), aCrypt))
		{
			aKey.SetValue(VALUE_PASSWORD, aCrypt.data(), aCrypt.size());
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool WONAPI::ReadAuthSettings(string& theLoginNameR, 
							  string& theCommunityNameR, 
							  string& thePasswordR)
{
	RegKey aKey(1, KEY_TOOLS_AUTH, false, true);
	if(aKey.IsOpen())
	{
		aKey.GetValue(VALUE_USERNAME, theLoginNameR);
		aKey.GetValue(VALUE_COMMUNITY, theCommunityNameR);

		ByteBufferPtr anEncryptP;
		aKey.QueryBinary(VALUE_PASSWORD, anEncryptP);
		if(anEncryptP.get() != NULL)
		{
			std::string aPassword;
			if(RegAuthPassword(true, anEncryptP->data(), anEncryptP->length(), aPassword))
			{
				thePasswordR = aPassword;
				return true;
			}
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool WONAPI::WriteAuthSettings(const string& theLoginName, 
							   const string& theCommunityName, 
							   const string& thePassword)
{
	RegKey aKey(1, KEY_TOOLS_AUTH, true, true);
	if(!aKey.IsOpen())
		return false;
	aKey.SetValue(VALUE_USERNAME, theLoginName);
	aKey.SetValue(VALUE_COMMUNITY, theCommunityName);

	if(!thePassword.empty())
	{
		std::string aCrypt;
		if(RegAuthPassword(false, thePassword.data(), thePassword.size(), aCrypt))
		{
			aKey.SetValue(VALUE_PASSWORD, aCrypt.data(), aCrypt.size());
			return true;
		}
	}
	return false;
}
