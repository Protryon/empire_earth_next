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
using namespace WONAPI;

// The class-static allocator at 0x8d954c holds DefaultAllocator (0x757f30), the SDK's default factory
LobbyConfig::ConfigAllocator LobbyConfig::mAllocator = LobbyConfig::DefaultAllocator;

std::string LobbyConfig::mDefaultFileName("WONLobby.cfg");

namespace WONAPI {
bool gLobbyUseLocalIP;
}

// FUNCTION: WON 0x757f30
// Creates a default lobby configuration object
LobbyConfig* LobbyConfig::DefaultAllocator()
{
    return new LobbyConfig;
}

// FUNCTION: WON 0x758090
// Creates the lobby configuration and registers every WONLobby.cfg key with its default; UseLocalIP comes
// from the HKLM\SOFTWARE\Sierra\WONLobby registry value
LobbyConfig::LobbyConfig()
{
    this->KeyAdd("DirServer", this->mDirServers, NULL, true);
    this->KeyAdd("RoomDirServer", this->mRoomDirServers, NULL, true);
    this->KeyAdd("AuthServers", this->mAuthServers, NULL, true);
    this->KeyAdd("ProductName", this->mProductName, NULL);
    this->KeyAdd("ProductDirectory", this->mProductDirectory, NULL);
    this->KeyAdd("ResourceDirectory", this->mResourceDirectory, NULL);
    this->KeyAdd("Version", this->mVersion, NULL);
    this->KeyAdd("LanVersion", this->mLanVersion, NULL);
    this->KeyAdd("Language", this->mLanguage, "");
    this->KeyAdd("CRCCheck", this->mCRCCheck, true);
    this->KeyAdd("CDKeyCheck", this->mCDKeyCheck, true);
    this->KeyAdd("Public", this->mPublic, false);
    this->KeyAdd("LanBroadcastPort", this->mLanBroadcastPort, 8585);
    this->KeyAdd("NoPingInProgress", this->mNoPingInProgress, true);
    this->KeyAdd("LobbyPort", this->mLobbyPort, 8888);
    this->KeyAdd("CheckForInternet", this->mCheckForInternet, true);
    this->KeyAdd("PingCeiling", this->mPingCeiling, 1000);
    this->KeyAdd("AllowDuplicateNames", this->mAllowDuplicateNames, true);
    this->KeyAdd("AppendNumberToDefaultLanName", this->mAppendNumberToDefaultLanName, true);
    this->KeyAdd("AllowJoinGameWithoutPing", this->mAllowJoinGameWithoutPing, true);

    bool useLocalIP = false;
    const char* regKey = "SOFTWARE\\Sierra\\WONLobby";
    const char* valueName = "UseLocalIP";
    int hive = 2;
    RegKey registry(hive, regKey, false, true);
    DWORD value = 0;
    if (registry.QueryDWORD(valueName, value) == 0)
        useLocalIP = value != 0;
    this->mUseLocalIP = useLocalIP;
    // The config key writes the global gLobbyUseLocalIP (0x96634d), which nothing reads, not mUseLocalIP:
    // a UseLocalIP line in WONLobby.cfg has no effect, only the registry value counts
    this->KeyAdd(valueName, gLobbyUseLocalIP, useLocalIP);
}

// The destructor is implicit (the shared one at 0x694e04); the scalar deleting destructor 0x758570 and the
// map destructors 0x758590/0x758650 are compiler-generated

// FUNCTION: WON 0x758720
// Handles a LOBBYDEFINE name value line by storing the define; passes every other key to the base class
bool LobbyConfig::HandleValueInstruction(ConfigParser* parser, const std::string& key,
                           StringParser& values)
{
    if (key == "LOBBYDEFINE") {
        std::string name;
        std::string value;
        values.ReadString(name, true);
        values.ReadString(value, false);
        if (!name.empty())
            this->mDefines[name] = value;
        return true;
    }
    return ConfigObject::HandleValueInstruction(parser, key, values);
}

// FUNCTION: WON 0x758900
// Checks that the config names directory servers, product, version and language (setting a parser error
// otherwise) and fills in the LAN version, resource and product directories and the Product define
bool LobbyConfig::PostParse(ConfigParser* parser)
{
    if (this->mDirServers.empty()) {
        parser->SetError("No Directory Servers specified.", false);
        return false;
    }
    if (this->mProductName.empty()) {
        parser->SetError("No product name specified.", false);
        return false;
    }
    if (this->mVersion.empty()) {
        parser->SetError("No version specified.", false);
        return false;
    }
    if (this->mLanguage.empty()) {
        parser->SetError("No language specified.", false);
        return false;
    }
    if (this->mLanVersion.empty())
        this->mLanVersion = this->mVersion;
    if (this->mResourceDirectory.empty())
        this->mResourceDirectory = ".\\" + this->mProductName;
    if (this->mProductDirectory.empty())
        this->mProductDirectory = std::wstring(L"/") + StringToWString(this->mProductName);
    this->mDefines["Product"] = this->GetProductName();
    return true;
}

// FUNCTION: WON 0x758710
// Returns the lobby's configuration, or NULL while the lobby is not running
LobbyConfig* WONAPI::GetLobbyConfig()
{
    if (gLobbyData.mPtr != NULL)
        return gLobbyData.mPtr->mConfig.mPtr;
    return NULL;
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00758570
// (public: virtual void * __thiscall WONAPI::LobbyConfig::`scalar deleting destructor'(unsigned int)    [??_GLobbyConfig@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00758590
// (public: __thiscall std::map<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<char,struc)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00757fa0
// (_$E72    [_$E72])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00758650
// (public: __thiscall std::map<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class WONAPI::SmartPtr<class WONAP)
// TEMPLATE: WON 0x758020
// TEMPLATE: WON 0x758030
// TEMPLATE: WON 0x757f90

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007695a0
// (public: class std::list<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::allocator<class std::basic_)
// TEMPLATE: WON 0x0076c080
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class s)
// TEMPLATE: WON 0x0076c0c0
// (protected: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_)
// TEMPLATE: WON 0x00770050
// (public: void * __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WONAPI::Smart)
