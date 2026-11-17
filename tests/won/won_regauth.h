#include "rust/won_net/regauth.h"

#if !defined(REG_AUTH_H)
#define REG_AUTH_H

#include <string>
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"


namespace WONAPI
{

	const std::string KEY_TOOLS("Software\\Sierra\\MPTools");
	const std::string KEY_TOOLS_AUTH(KEY_TOOLS + "\\Auth");
	const std::string VALUE_AUTHSERVERIP("AuthServerIP");
	const std::string VALUE_AUTHSERVERPORT("AuthServerPort");
	const std::string VALUE_USERNAME("UserName");
	const std::string VALUE_COMMUNITY("Community");
	const std::string VALUE_PASSWORD("Password");

	// the settings' reads and writes are ee_won's (regauth.rs; RegAuth.cpp is deleted: net round 27)
	inline void RegAuthSink(void* inOut, int32_t inWhich, const uint8_t* inBytes, size_t inLength)
	{ static_cast<std::string**>(inOut)[inWhich]->assign(reinterpret_cast<const char*>(inBytes), inLength); }
	inline EeWonRegAuthText RegAuthText(const std::string& inText)
	{ EeWonRegAuthText t = {reinterpret_cast<const uint8_t*>(inText.data()), inText.size()}; return t; }
	inline bool ReadAuthSettings(std::string& theAuthAddressR, unsigned short& theAuthPortR, std::string& theLoginNameR, std::string& theCommunityNameR, std::string& thePasswordR)
	{ std::string* anOut[4] = {&theAuthAddressR, &theLoginNameR, &theCommunityNameR, &thePasswordR}; return ee_won_regauth_read(true, &theAuthPortR, RegAuthSink, anOut); }
	inline bool WriteAuthSettings(const std::string& theAuthAddress, unsigned short theAuthPort, const std::string& theLoginName, const std::string& theCommunityName, const std::string& thePassword)
	{ EeWonRegAuthText anAddr = RegAuthText(theAuthAddress); return ee_won_regauth_write(&anAddr, theAuthPort, RegAuthText(theLoginName), RegAuthText(theCommunityName), RegAuthText(thePassword)); }
	inline bool ReadAuthSettings(std::string& theLoginNameR, std::string& theCommunityNameR, std::string& thePasswordR)
	{ std::string* anOut[4] = {NULL, &theLoginNameR, &theCommunityNameR, &thePasswordR}; return ee_won_regauth_read(false, NULL, RegAuthSink, anOut); }
	inline bool WriteAuthSettings(const std::string& theLoginName, const std::string& theCommunityName, const std::string& thePassword)
	{ return ee_won_regauth_write(NULL, 0, RegAuthText(theLoginName), RegAuthText(theCommunityName), RegAuthText(thePassword)); }

};

#endif