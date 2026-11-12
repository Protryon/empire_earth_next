// WON SDK title/community configuration; the instructions' children, PostParse's rules and the texts are ee_won's
// (rust/crates/ee_won/src/titlecfg.rs, applied by include/ee/wonapi/WONTitleConfigMsg.h)
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
#include "ee/wonapi/WONTitleConfigMsg.h"
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
	return TitleCfgMsg<TitleConfig>::Instruction(*this, false, this->mAlreadyInTitle, theParser, theTitleType);
}

bool TitleConfig::ParseTitleChild(ConfigParser& theParser)
{
	TitleConfigPtr aTitleP = new TitleConfig(true);
	return this->ParseChild(theParser,aTitleP.get());
}

bool TitleConfig::ParseCommunityChild(ConfigParser& theParser, bool inNested)
{
	CommunityConfigPtr aCommunityConfigP = new CommunityConfig(inNested);
	return this->ParseChild(theParser,aCommunityConfigP.get());
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
	return TitleCfgMsg<TitleConfig>::TitleText(*this, includeName);
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
	return TitleCfgMsg<CommunityConfig>::Instruction(*this, true, this->mAlreadyInCommunity, theParser, theCommunityType);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static std::wstring TitleCfgToWide(const std::string& theName) { return StringToWString(theName); }

bool CommunityConfig::PostParse(ConfigParser* theParser)
{
	TitleConfig* anObjectP = dynamic_cast<TitleConfig*>(this->mParent);
	TitleCfgMsg<CommunityConfig>::PostParse(*this, anObjectP ? &anObjectP->mName : NULL, TitleCfgToWide);
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
string CommunityConfig::ToString(void) const
{
	return TitleCfgMsg<CommunityConfig>::CommunityText(*this, WStringToString(this->mCommunityName));
}
