// WON SDK title/community configuration
// No original address identified
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONTitleConfig.h"
using namespace WONAPI;
using namespace std;


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
TitleConfig::TitleConfig(bool alreadyInTitle /* = false */) :
	mAlreadyInTitle(alreadyInTitle)
{
	this->KeyAdd("ForceCDKey",this->mForceCDKey,false);
	this->KeyAdd("RequireCRC",this->mRequireCrc,false);
	this->KeyAdd("AllowBetaKey",this->mAllowBetaKey,false);
	this->KeyAdd("RequireCDKey",this->mRequireCDKey,false);
	this->KeyAdd("FailToGuestLogin",this->mFailToGuest,true);
	this->KeyAdd("AllowGuestLogin",this->mAllowGuest,false);
	this->KeyAdd("ConcurrentUserLogin",this->mConcurrentUserLogin,true);
	this->KeyAdd("ConcurrentCDKeyLogin",this->mConcurrentCDKeyLogin,false);
	this->KeyAdd("GetUserData",this->mGetUserData,false);
	this->KeyAdd("GetForgeableUserData",this->mGetForgeableUserData,false);
	this->KeyAdd("CDKeyForInvite",this->mCDKeyForInvite,false);
	this->KeyAdd("SaveSessionKey",this->mSaveSessionKey,false);
	this->KeyAdd("GetKeyID",this->mGetKeyID,false);
	this->KeyAdd("ASCIIAccountNames",this->mASCIIAccountNames,false);

	this->KeyAdd("NetPort",this->mNetPort,15111);

	this->KeyAdd("Network",this->mNetwork,NULL);
	this->KeyAdd("ImagePath",this->mImagePath,NULL);
	this->KeyAdd("HashString",this->mHashString,NULL);
	this->KeyAdd("KeyPrefix",this->mKeyPrefix,NULL);

	this->KeyAdd("AdminCommunityName",this->mAdminCommunityName,L"");
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool TitleConfig::PreParse(ConfigParser* theParser)
{
	TitleConfig *aParent = dynamic_cast<TitleConfig*>(this->mParent);
	if(aParent!=NULL)
		this->CopyKeyValsFrom(aParent);

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool TitleConfig::HandleInstruction(ConfigParser* theParser, const std::string& theTitleType)
{
	if(ConfigObject::HandleInstruction(theParser, theTitleType))
		return true;

	if(theTitleType=="TITLEDEF")
	{
		if(this->mAlreadyInTitle)
		{
			theParser->SetError("You can't nest Title definitions!",false);
			return false;
		}

		TitleConfigPtr aTitleP = new TitleConfig(true);
		return this->ParseChild(*theParser,aTitleP.get());
	}
	else if(theTitleType=="COMMUNITY")
	{
		CommunityConfigPtr aCommunityConfigP = new CommunityConfig;
		return this->ParseChild(*theParser,aCommunityConfigP.get());
	}
	else
		return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool TitleConfig::PostParse(ConfigParser* theParser)
{
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
string TitleConfig::ToString(bool includeName) const
{
	string aStr;
	if(includeName)
		aStr += this->mName +"\r\n";

	aStr += string("\tNetwork: ") + (this->mNetwork.empty() ? string("EMPTY") : this->mNetwork) + "\r\n";
	char aBuffer[20];
	sprintf(aBuffer, "%d", this->mNetPort);
	aStr += string("\tNetPort: ") + string(aBuffer) + "\r\n";
	aStr += string("\tForceCDKey: ") + string((this->mForceCDKey) ? "TRUE" : "FALSE") + "\r\n";

	aStr += string("\tImagePath: ") + (this->mImagePath.empty() ? string("EMPTY") : this->mImagePath) + "\r\n";
	aStr += string("\tHashString: ") + (this->mHashString.empty() ? string("EMPTY") : this->mHashString) + "\r\n";
	aStr += string("\tKeyPrefix: ") + (this->mKeyPrefix.empty() ? string("EMPTY") : this->mKeyPrefix) + "\r\n";

	aStr += string("\tRequireCrc: ") + string((this->mRequireCrc) ? "TRUE" : "FALSE") + "\r\n";
	aStr += string("\tAllowBetaKey: ") + string((this->mAllowBetaKey) ? "TRUE" : "FALSE") + "\r\n";
	aStr += string("\tRequireCDKey: ") + string((this->mRequireCDKey) ? "TRUE" : "FALSE") + "\r\n";

	aStr += string("\tFailToGuest: ") + string((this->mFailToGuest) ? "TRUE" : "FALSE") + "\r\n";
	aStr += string("\tAllowGuest: ") + string((this->mAllowGuest) ? "TRUE" : "FALSE") + "\r\n";
	
	aStr += string("\tConcurrentUserLogin: ") + string((this->mConcurrentUserLogin) ? "TRUE" : "FALSE") + "\r\n";
	aStr += string("\tConcurrentCDKeyLogin: ") + string((this->mConcurrentCDKeyLogin) ? "TRUE" : "FALSE") + "\r\n";
	
	aStr += string("\tGetUserData: ") + string((this->mGetUserData) ? "TRUE" : "FALSE") + "\r\n";
	aStr += string("\tGetForgeableUserData: ") + string((this->mGetForgeableUserData) ? "TRUE" : "FALSE") + "\r\n";
	
	aStr += string("\tCDKeyForInvite: ") + string((this->mCDKeyForInvite) ? "TRUE" : "FALSE") + "\r\n";
	aStr += string("\tSaveSessionKey: ") + string((this->mSaveSessionKey) ? "TRUE" : "FALSE") + "\r\n";
	aStr += string("\tGetKeyID: ") + string((this->mGetKeyID) ? "TRUE" : "FALSE") +"\r\n";
	aStr += string("\tASCIIAccountNames: ") + string((this->mASCIIAccountNames) ? "TRUE" : "FALSE") +"\r\n";

	return aStr;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CommunityConfig::CommunityConfig(bool alreadyInCommunity /* = false */) :
	TitleConfig(),
	mAlreadyInCommunity(alreadyInCommunity)
{
	this->RegisterIntValue("CommunityID",this->mCommunityID,0);
	this->RegisterIntListListValue("CertificateLifeSpan",this->mCertificateLifeSpanList,NULL,true);
	this->KeyAdd("NameLogin",this->mNameLoginStr,"Required");
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CommunityConfig::CommunityConfig(const wstring& theCommunityName, unsigned long theCommunityID)
{
	this->Reset(true);
	this->mCommunityName = theCommunityName;
	this->mCommunityID = theCommunityID;
	this->mFailToGuest = false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool CommunityConfig::PreParse(ConfigParser* theParser)
{
	TitleConfig *aParent = dynamic_cast<TitleConfig*>(this->mParent);
	if(aParent!=NULL)
		this->CopyKeyValsFrom(aParent);

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool CommunityConfig::HandleInstruction(ConfigParser* theParser, const std::string& theCommunityType)
{
	if(ConfigObject::HandleInstruction(theParser, theCommunityType))
		return true;

	if(theCommunityType=="TITLEDEF")
	{
		theParser->SetError("You can't have a title definition within a community!",false);
		return false;
	}
	else if(theCommunityType=="COMMUNITY")
	{
		if(this->mAlreadyInCommunity)
		{
			theParser->SetError("You can't nest communities!",false);
			return false;
		}

		CommunityConfigPtr aCommutiyP = new CommunityConfig(true);
		return this->ParseChild(*theParser,aCommutiyP.get());
	}
	else
		return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool CommunityConfig::PostParse(ConfigParser* theParser)
{
	string aNameLoginStr = StringToUpperCase(this->mNameLoginStr);
	if(aNameLoginStr == "REQUIRED")
		this->mNameLogin = NameLogin_Required;
	else if(aNameLoginStr == "OPTIONAL")
		this->mNameLogin = NameLogin_Optional;
	else if(aNameLoginStr == "NONE")
		this->mNameLogin = NameLogin_None;
	else
		this->mNameLogin = NameLogin_Unknown;
	
	if(this->mImagePath.empty())
	{
		this->mImagePath = "./";

		TitleConfig* anObjectP = dynamic_cast<TitleConfig*>(this->mParent);
		if(anObjectP)
			this->mImagePath += anObjectP->mName;
		else
			this->mImagePath += this->mName;
	}

	if(this->mHashString.empty())
	{
		TitleConfig* anObjectP = dynamic_cast<TitleConfig*>(this->mParent);
		if(anObjectP)
			this->mHashString = anObjectP->mName;
		else
			this->mHashString = this->mName;
	}

	long aTrustLevel, aDuration;
	this->mTrustLevelLifespanMap.clear();
	for(std::list<std::list<int> >::const_iterator anItr = this->mCertificateLifeSpanList.begin(); anItr != this->mCertificateLifeSpanList.end(); ++anItr)
	{
		std::list<int>::const_iterator anItr2 = anItr->begin();
		if(anItr2 == anItr->end())
			continue;
		aTrustLevel = *anItr2;
		++anItr2;
		if(anItr2 != anItr->end())
		{
			aDuration = *anItr2;
			this->mTrustLevelLifespanMap[aTrustLevel] = aDuration;
		}
	}

	if (this->mCommunityName.empty())
		this->mCommunityName = StringToWString(this->mName);

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
string CommunityConfig::ToString(void) const
{
	string aStr = ((this->mCommunityName.empty()) ? this->mName : WStringToString(this->mCommunityName)) + "\r\n";

	char aBuffer[25];
	sprintf(aBuffer, "%d", this->mCommunityID);
	aStr += string("CommunityID: ") + string(aBuffer) + "\r\n";
	aStr += TitleConfig::ToString(false);

	// Trust level lifespan map
	aStr += string("\tNameLogin: ") + this->mNameLoginStr + "\r\n";
	aStr += "\tTrust Level Lifespans: [";
	if(this->mTrustLevelLifespanMap.empty())
		aStr += "EMPTY";
	else{
		bool isFirst = true;
		for(TRUST_LEVEL_LIFESPAN_MAP_CITR anItr = this->mTrustLevelLifespanMap.begin(); anItr != this->mTrustLevelLifespanMap.end(); ++anItr)
		{
			if(isFirst)
				isFirst = false;
			else
				aStr += ",";

			sprintf(aBuffer, "(%d,%d)", anItr->first, anItr->second);
			aStr += string(aBuffer);
		}
	}
	aStr += "]\r\n";

	return aStr;
}

