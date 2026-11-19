// WONInternal's TitleConfig and CommunityConfig as C++ classes, copied from include/ee/wonapi/WONTitleConfig.h and
// src/Empire Earth/WONAPI/WONInternal/TitleConfig.cpp before WON round 22 moved them whole into Rust (ee_won_net
// titleobj.rs), renamed OrigTitleConfig and OrigCommunityConfig: won_titleobj_test's original side, over the game's
// ConfigObject. Two changes, both for the comparison: mNameLogin (left uninitialised until PostParse by the
// original) starts at NameLogin_None, and the dummy constructor's mAlreadyInCommunity (also uninitialised) at false.
#pragma once
#include <list>
#include <map>
#include <string>
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONConfigObject.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "won_titlecfg_msg.h"

namespace WONAPI {

class OrigTitleConfig : public ConfigObject
{
public:
	OrigTitleConfig(bool alreadyInTitle = false);

	static ConfigObject* ConfigFactory() { return new OrigTitleConfig(); }
	virtual bool HandleInstruction(ConfigParser* theParser, const std::string& theTitleType);
	virtual bool PreParse(ConfigParser* theParser);
	virtual bool PostParse(ConfigParser* theParser);
	std::string ToString(bool includeName) const;
	bool ParseTitleChild(ConfigParser& theParser);
	bool ParseCommunityChild(ConfigParser& theParser, bool inNested);

	bool mAlreadyInTitle;

	std::string mNetwork;
	unsigned short mNetPort;
	bool mForceCDKey;

	std::string mImagePath;
	std::string mHashString;
	std::string mKeyPrefix;

	bool mRequireCrc;
	bool mAllowBetaKey;
	bool mRequireCDKey;

	bool mFailToGuest;
	bool mAllowGuest;

	bool mConcurrentUserLogin;
	bool mConcurrentCDKeyLogin;

	bool mGetUserData;
	bool mGetForgeableUserData;

	bool mCDKeyForInvite;
	bool mSaveSessionKey;
	bool mGetKeyID;
	bool mASCIIAccountNames;

	std::wstring  mAdminCommunityName;
};
typedef WONAPI::SmartPtr<OrigTitleConfig> OrigTitleConfigPtr;

class OrigCommunityConfig : public OrigTitleConfig
{
public:
	enum NameLogin
	{
		NameLogin_None		= 0,
		NameLogin_Optional	= 1,
		NameLogin_Required	= 2,
		NameLogin_Unknown	= 3
	};

	OrigCommunityConfig(bool alreadyInCommunity = false);
	OrigCommunityConfig(const std::wstring& theCommunityName, unsigned long theCommunityID);

	static ConfigObject* ConfigFactory() { return new OrigCommunityConfig(); }
	virtual bool HandleInstruction(ConfigParser* theParser, const std::string& theCommunityType);
	virtual bool PreParse(ConfigParser* theParser);
	virtual bool PostParse(ConfigParser* theParser);
	std::string ToString(void) const;

	bool mAlreadyInCommunity;

	int          mCommunityID;
	std::wstring mCommunityName;
	std::string  mNameLoginStr;
	NameLogin    mNameLogin;

	typedef std::map<long, long> TRUST_LEVEL_LIFESPAN_MAP;
	TRUST_LEVEL_LIFESPAN_MAP mTrustLevelLifespanMap;

	std::list<std::list<int> > mCertificateLifeSpanList;
};
typedef WONAPI::SmartPtr<OrigCommunityConfig> OrigCommunityConfigPtr;

inline OrigTitleConfig::OrigTitleConfig(bool alreadyInTitle /* = false */) :
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

inline bool OrigTitleConfig::PreParse(ConfigParser* theParser)
{
	OrigTitleConfig *aParent = dynamic_cast<OrigTitleConfig*>(this->mParent);
	if(aParent!=NULL)
		this->CopyKeyValsFrom(aParent);

	return true;
}

inline bool OrigTitleConfig::HandleInstruction(ConfigParser* theParser, const std::string& theTitleType)
{
	if(ConfigObject::HandleInstruction(theParser, theTitleType))
		return true;
	return TitleCfgMsg<OrigTitleConfig>::Instruction(*this, false, this->mAlreadyInTitle, theParser, theTitleType);
}

inline bool OrigTitleConfig::ParseTitleChild(ConfigParser& theParser)
{
	OrigTitleConfigPtr aTitleP = new OrigTitleConfig(true);
	return this->ParseChild(theParser,aTitleP.get());
}

inline bool OrigTitleConfig::ParseCommunityChild(ConfigParser& theParser, bool inNested)
{
	OrigCommunityConfigPtr aCommunityConfigP = new OrigCommunityConfig(inNested);
	return this->ParseChild(theParser,aCommunityConfigP.get());
}

inline bool OrigTitleConfig::PostParse(ConfigParser* theParser)
{
	return true;
}

inline std::string OrigTitleConfig::ToString(bool includeName) const
{
	return TitleCfgMsg<OrigTitleConfig>::TitleText(*this, includeName);
}

inline OrigCommunityConfig::OrigCommunityConfig(bool alreadyInCommunity /* = false */) :
	OrigTitleConfig(),
	mAlreadyInCommunity(alreadyInCommunity),
	mNameLogin(NameLogin_None)
{
	this->RegisterIntValue("CommunityID",this->mCommunityID,0);
	this->RegisterIntListListValue("CertificateLifeSpan",this->mCertificateLifeSpanList,NULL,true);
	this->KeyAdd("NameLogin",this->mNameLoginStr,"Required");
}

inline OrigCommunityConfig::OrigCommunityConfig(const std::wstring& theCommunityName, unsigned long theCommunityID) :
	mAlreadyInCommunity(false),
	mNameLogin(NameLogin_None)
{
	this->Reset(true);
	this->mCommunityName = theCommunityName;
	this->mCommunityID = theCommunityID;
	this->mFailToGuest = false;
}

inline bool OrigCommunityConfig::PreParse(ConfigParser* theParser)
{
	OrigTitleConfig *aParent = dynamic_cast<OrigTitleConfig*>(this->mParent);
	if(aParent!=NULL)
		this->CopyKeyValsFrom(aParent);

	return true;
}

inline bool OrigCommunityConfig::HandleInstruction(ConfigParser* theParser, const std::string& theCommunityType)
{
	if(ConfigObject::HandleInstruction(theParser, theCommunityType))
		return true;
	return TitleCfgMsg<OrigCommunityConfig>::Instruction(*this, true, this->mAlreadyInCommunity, theParser, theCommunityType);
}

static inline std::wstring OrigTitleCfgToWide(const std::string& theName) { return StringToWString(theName); }

inline bool OrigCommunityConfig::PostParse(ConfigParser* theParser)
{
	OrigTitleConfig* anObjectP = dynamic_cast<OrigTitleConfig*>(this->mParent);
	TitleCfgMsg<OrigCommunityConfig>::PostParse(*this, anObjectP ? &anObjectP->mName : NULL, OrigTitleCfgToWide);
	return true;
}

inline std::string OrigCommunityConfig::ToString(void) const
{
	return TitleCfgMsg<OrigCommunityConfig>::CommunityText(*this, WStringToString(this->mCommunityName));
}

} // namespace WONAPI
