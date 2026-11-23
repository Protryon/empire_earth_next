// C:\EEX\Empire Earth\WONAPI\WONLobby\LobbyConfig.cpp (file name inferred: no __FILE__)
// LobbyConfig: the lobby's WONLobby.cfg settings (servers, product, versions, ports, checks). The unit spans
// 0x757f30..0x758de2 (factory, static initializers, constructor, RTTI/vtable, EH); the code at
// 0x757d70..0x757f24 belongs to another unit (inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "rust/won_lobby/lobbycfg.h"
#include "rust/won_lobby/lobbyconf.h"
#include "ee/wonapi/WONLobbyGuard.h"
using namespace WONAPI;

// The class-static allocator at 0x8d954c holds DefaultAllocator (0x757f30), the SDK's default factory
LobbyConfig::ConfigAllocator LobbyConfig::mAllocator = LobbyConfig::DefaultAllocator;

std::string LobbyConfig::mDefaultFileName("WONLobby.cfg");

namespace WONAPI {
bool gLobbyUseLocalIP;
}

// Creates a default lobby configuration object
LobbyConfig* LobbyConfig::DefaultAllocator()
{
    return new LobbyConfig;
}

// the constructor's and HandleValueInstruction's statements (lobbyconf.rs composes them, R6 WON round 33); their
// locals are these statics
static RegKey* sConfRegistry;
static DWORD sConfRegistryValue;
static std::string sConfName, sConfValue;
static intptr_t ConfHost(void* inConfig, EeWonLobbyConfOp inOp, intptr_t a, intptr_t b, intptr_t cc)
{
    typedef EeWonLobbyConfOp Op;
    LobbyConfig* c = static_cast<LobbyConfig*>(inConfig);
    switch (inOp) {
    case Op::KeyDirServer: c->KeyAdd("DirServer", c->mDirServers, NULL, true); return 0;
    case Op::KeyRoomDirServer: c->KeyAdd("RoomDirServer", c->mRoomDirServers, NULL, true); return 0;
    case Op::KeyAuthServers: c->KeyAdd("AuthServers", c->mAuthServers, NULL, true); return 0;
    case Op::KeyProductName: c->KeyAdd("ProductName", c->mProductName, NULL); return 0;
    case Op::KeyProductDirectory: c->KeyAdd("ProductDirectory", c->mProductDirectory, NULL); return 0;
    case Op::KeyResourceDirectory: c->KeyAdd("ResourceDirectory", c->mResourceDirectory, NULL); return 0;
    case Op::KeyVersion: c->KeyAdd("Version", c->mVersion, NULL); return 0;
    case Op::KeyLanVersion: c->KeyAdd("LanVersion", c->mLanVersion, NULL); return 0;
    case Op::KeyLanguage: c->KeyAdd("Language", c->mLanguage, ""); return 0;
    case Op::KeyCrcCheck: c->KeyAdd("CRCCheck", c->mCRCCheck, true); return 0;
    case Op::KeyCdKeyCheck: c->KeyAdd("CDKeyCheck", c->mCDKeyCheck, true); return 0;
    case Op::KeyPublic: c->KeyAdd("Public", c->mPublic, false); return 0;
    case Op::KeyLanBroadcastPort: c->KeyAdd("LanBroadcastPort", c->mLanBroadcastPort, 8585); return 0;
    case Op::KeyNoPingInProgress: c->KeyAdd("NoPingInProgress", c->mNoPingInProgress, true); return 0;
    case Op::KeyLobbyPort: c->KeyAdd("LobbyPort", c->mLobbyPort, 8888); return 0;
    case Op::KeyCheckForInternet: c->KeyAdd("CheckForInternet", c->mCheckForInternet, true); return 0;
    case Op::KeyPingCeiling: c->KeyAdd("PingCeiling", c->mPingCeiling, 1000); return 0;
    case Op::KeyAllowDuplicateNames: c->KeyAdd("AllowDuplicateNames", c->mAllowDuplicateNames, true); return 0;
    case Op::KeyAppendNumberToDefaultLanName: c->KeyAdd("AppendNumberToDefaultLanName", c->mAppendNumberToDefaultLanName, true); return 0;
    case Op::KeyAllowJoinGameWithoutPing: c->KeyAdd("AllowJoinGameWithoutPing", c->mAllowJoinGameWithoutPing, true); return 0;
    case Op::RegistryOpen: sConfRegistry = new RegKey(2, "SOFTWARE\\Sierra\\WONLobby", false, true); return 0;
    case Op::RegistryValueClear: sConfRegistryValue = 0; return 0;
    case Op::RegistryQuery: return (intptr_t)(sConfRegistry->QueryDWORD("UseLocalIP", sConfRegistryValue));
    case Op::RegistryValue: return (intptr_t)(sConfRegistryValue);
    case Op::SetUseLocalIp: c->mUseLocalIP = a != 0; return 0;
    case Op::KeyUseLocalIp: c->KeyAdd("UseLocalIP", gLobbyUseLocalIP, a != 0); return 0;
    case Op::RegistryClose: delete sConfRegistry; return 0;
    case Op::KeyIsLobbyDefine: return (*(const std::string*)a == "LOBBYDEFINE") ? 1 : 0;
    case Op::DefineLocalsClear: sConfName = sConfValue = std::string(); return 0;
    case Op::DefineReadName: ((StringParser*)a)->ReadString(sConfName, true); return 0;
    case Op::DefineReadValue: ((StringParser*)a)->ReadString(sConfValue, false); return 0;
    case Op::DefineNameEmpty: return (sConfName.empty()) ? 1 : 0;
    case Op::DefineStore: c->mDefines[sConfName] = sConfValue; return 0;
    case Op::BaseHandleValue: return WON_GUARDED(c->ConfigObject::HandleValueInstruction((ConfigParser*)a, *(const std::string*)b, *(StringParser*)cc));
    }
    return 0;
}

// Creates the lobby configuration and registers every WONLobby.cfg key with its default; UseLocalIP comes
// from the HKLM\SOFTWARE\Sierra\WONLobby registry value (the config key writes the global gLobbyUseLocalIP,
// 0x96634d, which nothing reads, not mUseLocalIP: a UseLocalIP line in WONLobby.cfg has no effect)
LobbyConfig::LobbyConfig() { ee_won_lobbyconf_call(this, ConfHost, EeWonLobbyConfCall::Construct, 0, 0, 0); }

// The destructor is implicit (the shared one at 0x694e04); the scalar deleting destructor 0x758570 and the
// map destructors 0x758590/0x758650 are compiler-generated

// Handles a LOBBYDEFINE name value line by storing the define; passes every other key to the base class
bool LobbyConfig::HandleValueInstruction(ConfigParser* parser, const std::string& key, StringParser& values) { return WonRethrow(ee_won_lobbyconf_call(this, ConfHost, EeWonLobbyConfCall::HandleValue, (intptr_t)parser, (intptr_t)&key, (intptr_t)&values)) != 0; }

// Checks that the config names directory servers, product, version and language (setting a parser error
// otherwise) and fills in the LAN version, resource and product directories and the Product define
// PostParse's statements (lobbycfg.rs composes them, R6 WON round 32)
static intptr_t CfgHost(void* inConfig, void* inParser, EeWonLobbyCfgOp inOp)
{
    typedef EeWonLobbyCfgOp Op;
    LobbyConfig* c = static_cast<LobbyConfig*>(inConfig);
    ConfigParser* parser = static_cast<ConfigParser*>(inParser);
    switch (inOp) {
    case Op::DirServersEmpty: return c->mDirServers.empty();
    case Op::ProductNameEmpty: return c->mProductName.empty();
    case Op::VersionEmpty: return c->mVersion.empty();
    case Op::LanguageEmpty: return c->mLanguage.empty();
    case Op::ErrorNoDirServers: parser->SetError("No Directory Servers specified.", false); return 0;
    case Op::ErrorNoProductName: parser->SetError("No product name specified.", false); return 0;
    case Op::ErrorNoVersion: parser->SetError("No version specified.", false); return 0;
    case Op::ErrorNoLanguage: parser->SetError("No language specified.", false); return 0;
    case Op::LanVersionEmpty: return c->mLanVersion.empty();
    case Op::LanVersionFromVersion: c->mLanVersion = c->mVersion; return 0;
    case Op::ResourceDirEmpty: return c->mResourceDirectory.empty();
    case Op::ResourceDirDefault: c->mResourceDirectory = ".\\" + c->mProductName; return 0;
    case Op::ProductDirEmpty: return c->mProductDirectory.empty();
    case Op::ProductDirDefault: c->mProductDirectory = std::wstring(L"/") + StringToWString(c->mProductName); return 0;
    case Op::DefineProduct: c->mDefines["Product"] = c->GetProductName(); return 0;
    }
    return 0;
}
bool LobbyConfig::PostParse(ConfigParser* parser) { return ee_won_lobbycfg_post_parse(this, parser, CfgHost); }

// Returns the lobby's configuration, or NULL while the lobby is not running
LobbyConfig* WONAPI::GetLobbyConfig()
{
    if (gLobbyData.mPtr != NULL)
        return gLobbyData.mPtr->mConfig.mPtr;
    return NULL;
}
