// LobbyConfig (WONLobby.cfg) as a Rust record (R6 WON lobby and network round 40: ee_won_lobby lobbyconf.rs; the
// class's members, LobbyConfig.cpp and its two hosts deleted): the original class (its members, its constructor's
// KeyAdd calls with their defaults and the registry's UseLocalIP, HandleValueInstruction's LOBBYDEFINE line and
// PostParse's checks, as they were before the round, with EELobbyConfig's file transfer port) over the original
// ConfigObject (won_cfgobject_orig.h), against the game's class (WONLobbyPrv.cpp's LobbyConfig part, as it is) over
// the game's ConfigObject: the same WONLobby.cfg texts (fixed ones, then random lines of every key, LOBBYDEFINE lines,
// unknown keys and bad values) parsed by both, and the results compared: the parse's answer, any exception, the
// parser's abort text and warnings, and every value (the lists, texts, the product directory, flags, ports and the
// defines in their order). Exit code: the mismatches.
#include "won_cfgparser_driver.h" // ConfigParser's driver-only methods (before the game headers)
#define EE_WON_STRINGPARSER_DRIVER // StringParser's list reads and writes (won_parser_lists.inl)
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONConfigObject.h"
#include "rust/won_lobby/lobbyconf.h"
#include "won_cfgobject_orig.h"
#include "won_regkey.h"
using namespace WONAPI;

// ---- the original class (LobbyConfig.cpp and WONLobbyGUI.h before round 40, renamed)
static bool sOrigUseLocalIPGlobal; // the original's gLobbyUseLocalIP
struct OrigLobbyConfig : OrigConfigObject {
    std::list<std::string> mDirServers, mRoomDirServers, mAuthServers;
    std::string mProductName;
    std::wstring mProductDirectory;
    std::string mVersion, mLanVersion, mLanguage, mResourceDirectory;
    bool mCRCCheck, mCDKeyCheck, mPublic;
    unsigned short mLanBroadcastPort;
    bool mUseLocalIP, mNoPingInProgress;
    unsigned short mLobbyPort;
    bool mCheckForInternet;
    unsigned short mPingCeiling;
    bool mAllowDuplicateNames, mAppendNumberToDefaultLanName, mAllowJoinGameWithoutPing;
    std::map<std::string, std::string, StringLessNoCase> mDefines;
    unsigned short mFileTransferPort;
    OrigLobbyConfig()
    {
        KeyAdd("DirServer", mDirServers, NULL, true);
        KeyAdd("RoomDirServer", mRoomDirServers, NULL, true);
        KeyAdd("AuthServers", mAuthServers, NULL, true);
        KeyAdd("ProductName", mProductName, NULL);
        KeyAdd("ProductDirectory", mProductDirectory, NULL);
        KeyAdd("ResourceDirectory", mResourceDirectory, NULL);
        KeyAdd("Version", mVersion, NULL);
        KeyAdd("LanVersion", mLanVersion, NULL);
        KeyAdd("Language", mLanguage, "");
        KeyAdd("CRCCheck", mCRCCheck, true);
        KeyAdd("CDKeyCheck", mCDKeyCheck, true);
        KeyAdd("Public", mPublic, false);
        KeyAdd("LanBroadcastPort", mLanBroadcastPort, 8585);
        KeyAdd("NoPingInProgress", mNoPingInProgress, true);
        KeyAdd("LobbyPort", mLobbyPort, 8888);
        KeyAdd("CheckForInternet", mCheckForInternet, true);
        KeyAdd("PingCeiling", mPingCeiling, 1000);
        KeyAdd("AllowDuplicateNames", mAllowDuplicateNames, true);
        KeyAdd("AppendNumberToDefaultLanName", mAppendNumberToDefaultLanName, true);
        KeyAdd("AllowJoinGameWithoutPing", mAllowJoinGameWithoutPing, true);
        bool useLocalIP = false;
        RegKey registry(2, "SOFTWARE\\Sierra\\WONLobby", false, true);
        DWORD value = 0;
        if (registry.QueryDWORD("UseLocalIP", value) == 0) useLocalIP = value != 0;
        mUseLocalIP = useLocalIP;
        KeyAdd("UseLocalIP", sOrigUseLocalIPGlobal, useLocalIP);
        KeyAdd("EEFileTransferPort", mFileTransferPort, 8889);
    }
    virtual bool HandleValueInstruction(ConfigParser* parser, const std::string& key, StringParser& values)
    {
        if (key == "LOBBYDEFINE") {
            std::string name, value;
            values.ReadString(name, true);
            values.ReadString(value, false);
            if (!name.empty()) mDefines[name] = value;
            return true;
        }
        return OrigConfigObject::HandleValueInstruction(parser, key, values);
    }
    virtual bool PostParse(ConfigParser* parser)
    {
        if (mDirServers.empty()) { parser->SetError("No Directory Servers specified.", false); return false; }
        if (mProductName.empty()) { parser->SetError("No product name specified.", false); return false; }
        if (mVersion.empty()) { parser->SetError("No version specified.", false); return false; }
        if (mLanguage.empty()) { parser->SetError("No language specified.", false); return false; }
        if (mLanVersion.empty()) mLanVersion = mVersion;
        if (mResourceDirectory.empty()) mResourceDirectory = ".\\" + mProductName;
        if (mProductDirectory.empty()) mProductDirectory = std::wstring(L"/") + StringToWString(mProductName);
        mDefines["Product"] = mProductName;
        return true;
    }
};

// ---- the game's class (WONLobbyPrv.cpp's LobbyConfig part, as it is)
std::string LobbyConfig::mDefaultFileName("WONLobby.cfg");
LobbyConfig::LobbyConfig() : mConf(ee_won_lobbyconf_new(this->mObj)) {}
LobbyConfig::~LobbyConfig() { ee_won_lobbyconf_free(this->mConf); }
bool LobbyConfig::HandleValueInstruction(ConfigParser* p, const std::string& k, StringParser& v)
{ CfgExc x; return Ret(ee_won_lobbyconf_handle_value(this->mConf, this->mObj, p, B(k), k.length(), (EeWonCfgStringParser*)&v, &x), x); }
bool LobbyConfig::PostParse(ConfigParser* p) { return ee_won_lobbyconf_post_parse(this->mConf, p->Obj()); }

static std::string Int(long v) { char b[32]; snprintf(b, sizeof(b), "%ld", v); return b; }
static std::string W(const std::wstring& w) { std::string s; for (wchar_t c : w) s += Int((long)c) + "."; return s; }
static void Take(void* c, const uint8_t* b, size_t n) { static_cast<std::string*>(c)->append((const char*)b, n); }
static void TakeW(void* c, const void* u, size_t n) { static_cast<std::wstring*>(c)->assign((const wchar_t*)u, n); }
static std::string Lst(const std::list<std::string>& l) { std::string s; for (const auto& i : l) s += "[" + i + "]"; return s; }

static std::string Dump(OrigLobbyConfig* c)
{
    std::string o = "dir " + Lst(c->mDirServers) + " room " + Lst(c->mRoomDirServers) + " auth " + Lst(c->mAuthServers);
    o += "\nproduct [" + c->mProductName + "] pdir " + W(c->mProductDirectory) + " version [" + c->mVersion + "] lan [" +
         c->mLanVersion + "] language [" + c->mLanguage + "] res [" + c->mResourceDirectory + "]\n";
    int v[] = {c->mCRCCheck, c->mCDKeyCheck, c->mPublic, c->mLanBroadcastPort, c->mUseLocalIP, c->mNoPingInProgress,
               c->mLobbyPort, c->mCheckForInternet, c->mPingCeiling, c->mAllowDuplicateNames,
               c->mAppendNumberToDefaultLanName, c->mAllowJoinGameWithoutPing, c->mFileTransferPort};
    for (int x : v) o += Int(x) + " ";
    o += "\ndefines";
    for (const auto& d : c->mDefines) o += " " + d.first + "=" + d.second;
    return o + "\n";
}
static std::string Dump(LobbyConfig* lc)
{
    EeWonLobbyConf* c = lc->mConf;
    std::string o;
    static const char* kLists[3] = {"dir ", " room ", " auth "};
    for (int l = 0; l < 3; l++) {
        o += kLists[l];
        for (size_t i = 0;; i++) {
            std::string item;
            size_t n = ee_won_lobbyconf_list(c, (EeWonLobbyConfList)(l + 1), i, Take, &item);
            if (i >= n) break;
            o += "[" + item + "]";
        }
    }
    std::string t[5];
    for (int i = 0; i < 5; i++) ee_won_lobbyconf_text(c, (EeWonLobbyConfText)(i + 1), Take, &t[i]);
    std::wstring d;
    ee_won_lobbyconf_product_dir(c, TakeW, &d);
    o += "\nproduct [" + t[0] + "] pdir " + W(d) + " version [" + t[1] + "] lan [" + t[2] + "] language [" + t[3] + "] res [" + t[4] + "]\n";
    for (int i = 1; i <= (int)EeWonLobbyConfNum::FileTransferPort; i++) o += Int(ee_won_lobbyconf_num(c, (EeWonLobbyConfNum)i)) + " ";
    o += "\ndefines";
    for (size_t i = 0;; i++) {
        std::string kv[2];
        struct S { std::string* kv; int n; } s = {kv, 0};
        if (!ee_won_lobbyconf_define_at(c, i, [](void* x, const uint8_t* b, size_t n) {
                S* s = (S*)x;
                s->kv[s->n++].assign((const char*)b, n);
            }, &s))
            break;
        o += " " + kv[0] + "=" + kv[1];
    }
    return o + "\n";
}

template <class C> static std::string Run()
{
    std::string o;
    ConfigParser::ResetLineCount();
    ConfigParser aParser;
    aParser.SetWarningsAsErrors(true);
    SmartPtr<C> aConfig = new C;
    try {
        bool r = aConfig->ParseTopLevel(aParser, "WONLobby.cfg");
        o += "r " + Int(r) + "\n";
    } catch (ConfigObjectException& e) {
        o += "threw config " + e.mText + " " + Int(e.mFlag) + "\n";
    } catch (FileReaderException& e) {
        o += "threw file " + e.mDescription + "\n";
    }
    o += "abort [" + aParser.GetAbortText() + "] warn [" + aParser.GetWarnings() + "]\n";
    return o + Dump(aConfig.get());
}

static unsigned long sSeed = 40;
static int Rand(int n) { sSeed = sSeed * 1103515245 + 12345; return (int)((sSeed >> 8) % (unsigned long)n); }
static const char* const kLines[] = {
    "DirServer: dir1.won.net:15101", "DirServer: dir2.won.net:15101, dir3:15101", "RoomDirServer: room:15101",
    "AuthServers: auth1:15200, auth2:15200", "ProductName: EmpireEarth", "ProductName: \"Empire Earth\"",
    "ProductDirectory: /EmpireEarth", "ProductDirectory: \"/EE dir\"", "ResourceDirectory: Lobby", "Version: 2.0",
    "Version: 1.01", "LanVersion: 2.0L", "Language: English", "Language: \"\"", "CRCCheck: false", "CRCCheck: True",
    "CDKeyCheck: no", "Public: yes", "LanBroadcastPort: 9000", "LanBroadcastPort: x", "NoPingInProgress: false",
    "LobbyPort: 2300", "CheckForInternet: false", "PingCeiling: 700", "PingCeiling: 70000", "AllowDuplicateNames: false",
    "AppendNumberToDefaultLanName: false", "AllowJoinGameWithoutPing: false", "UseLocalIP: true",
    "EEFileTransferPort: 9999", "LOBBYDEFINE Product EmpireEarthGold", "LOBBYDEFINE product lower",
    "LOBBYDEFINE Skin \"Blue Steel\"", "LOBBYDEFINE", "LOBBYDEFINE OnlyName", "LobbyDefine Old 1", "Unknown: 5",
    "DirServer:", "ProductName:", "Version:",
};
static const int kLineCount = sizeof(kLines) / sizeof(kLines[0]);

int main()
{
    static const char* const kFixed[] = {
        "", "DirServer: d:1\nProductName: EE\nVersion: 2\nLanguage: English\n",
        "DirServer: d:1\nProductName: EE\nVersion: 2\nLanguage: English\nLanVersion: 3\nResourceDirectory: r\n"
        "ProductDirectory: /p\nLOBBYDEFINE Product Mine\n",
        "DirServer: d:1\nProductName: EE\n", "DirServer: d:1\nProductName: EE\nVersion: 2\n", "ProductName: EE\n",
        "Unknown: 1\n",
    };
    const int aFixedCount = sizeof(kFixed) / sizeof(kFixed[0]);
    int aChecks = 0, aFailures = 0, aParsed = 0, aDefines = 0;
    for (int s = -aFixedCount; s < 3000; s++) {
        std::string aText;
        if (s < 0) aText = kFixed[s + aFixedCount];
        else {
            // half of them name the four required keys first and use no unknown or empty key (the last four lines)
            bool aWhole = Rand(2) == 0;
            if (aWhole) aText = "DirServer: d:1\nProductName: EE\nVersion: 2\nLanguage: English\n";
            for (int n = Rand(14); n > 0; n--) aText += std::string(kLines[Rand(aWhole ? kLineCount - 4 : kLineCount)]) + "\n";
        }
        FILE* f = fopen("WONLobby.cfg", "wb");
        fwrite(aText.data(), 1, aText.size(), f);
        fclose(f);
        std::string a = Run<OrigLobbyConfig>(), b = Run<LobbyConfig>();
        aChecks++;
        aParsed += a.compare(0, 4, "r 1\n") == 0;
        aDefines += a.find("defines ") != std::string::npos && a.find("=", a.find("defines ")) != std::string::npos;
        if (a != b && ++aFailures <= 3) printf("MISMATCH %d\n--- text\n%s--- orig\n%s--- port\n%s", s, aText.c_str(), a.c_str(), b.c_str());
    }
    printf("won_lobbyconf_test: %d configs (%d parsed whole, %d with defines), %d mismatches\n", aChecks, aParsed, aDefines, aFailures);
    return aFailures;
}
