// No original address identified: WON SDK registry authentication settings
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include "ee/wonapi/WONRegAuth.h"



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
		unsigned long anAuthPort(15200);
		aKey.QueryDWORD(VALUE_AUTHSERVERPORT, anAuthPort);
		theAuthPortR = anAuthPort;
		aKey.GetValue(VALUE_USERNAME, theLoginNameR);
		aKey.GetValue(VALUE_COMMUNITY, theCommunityNameR);

		ByteBufferPtr anEncryptP;
		aKey.QueryBinary(VALUE_PASSWORD, anEncryptP);
		if(anEncryptP.get() != NULL)
		{
			ByteBufferPtr aCryptReturnP = gPasswordKey.Decrypt(anEncryptP->data(), anEncryptP->length());
			if(aCryptReturnP.get() != NULL)
			{
				thePasswordR.assign((char*)aCryptReturnP->data(), aCryptReturnP->length());
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
		ByteBufferPtr aCryptReturnP = gPasswordKey.Encrypt((const char*)thePassword.c_str(), thePassword.size());
		if(aCryptReturnP.get() != NULL)
		{
			aKey.SetValue(VALUE_PASSWORD, aCryptReturnP->data(), aCryptReturnP->length());
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
			ByteBufferPtr aCryptReturnP = gPasswordKey.Decrypt(anEncryptP->data(), anEncryptP->length());
			if(aCryptReturnP.get() != NULL)
			{
				thePasswordR.assign((char*)aCryptReturnP->data(), aCryptReturnP->length());
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
		ByteBufferPtr aCryptReturnP = gPasswordKey.Encrypt((const char*)thePassword.c_str(), thePassword.size());
		if(aCryptReturnP.get() != NULL)
		{
			aKey.SetValue(VALUE_PASSWORD, aCryptReturnP->data(), aCryptReturnP->length());
			return true;
		}
	}
	return false;
}
