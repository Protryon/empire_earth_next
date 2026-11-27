// WONLobby configuration: WONAPI's config file parser and registry key, and the lobby's private state
#ifndef EE_WONAPI_WONLOBBYCONFIG_H
#define EE_WONAPI_WONLOBBYCONFIG_H
#include "rust/won_net/config.h"
#include "rust/won_net/cfgparser.h"
#include "rust/won_net/regkey.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyLogic.h"
namespace WONAPI {

// A config file parser. The parser is ee_won's object whole (rust/crates/ee_won_net/src/cfgparser.rs: the reader,
// the pushback and define scopes, the parent, the texts, the flags; ConfigParser.cpp and ConfigToken.cpp are
// deleted): these are the inline forwards the game calls, which throw the reader's failures as the original did. The
// rest (the token reads, the saved position, the define scopes, ...) only the drivers call: R6 WON lobby and network
// round 38 moved them to tests/won/won_cfgparser_methods.inl (won_cfgparser_driver.h).
class ConfigParser {
private:
    ConfigParser(const ConfigParser&);
    ConfigParser& operator=(const ConfigParser&);
    static void Take(void* c, const uint8_t* b, size_t n) { static_cast<std::string*>(c)->assign((const char*)b, n); }
    static const uint8_t* B(const std::string& s) { return (const uint8_t*)s.data(); }
    EeWonCfgParser* mObj;
    std::string mAbortCopy; // GetAbortReason's answer
public:
    EeWonCfgParser* Obj() { return this->mObj; } // the Rust parser (ConfigObject's host)
    ConfigParser() : mObj(ee_won_cfgp_new()) {}
    ~ConfigParser() { ee_won_cfgp_free(this->mObj); }
    // the process-wide line count (0x966b54) is ee_won's (rust/crates/ee_won_net/src/config.rs)
    static void ResetLineCount() { ee_won_config_reset_line_count(); }
    static int GetLineCount() { return ee_won_config_line_count(); }
    void SetError(const std::string& e, bool a) { ee_won_cfgp_set_error(this->mObj, B(e), e.length(), a); }
    void AddWarning(const std::string& w) { ee_won_cfgp_add_warning(this->mObj, B(w), w.length()); }
    const std::string& GetAbortReason()
    { this->mAbortCopy.erase(); ee_won_cfgp_abort_reason(this->mObj, Take, &this->mAbortCopy); return this->mAbortCopy; }
    void SetWarningsAsErrors(bool on) { ee_won_cfgp_set_field(this->mObj, EeWonCfgpField::WarningsAsErrors, on); }
    bool OpenFile(const std::string& p)
    { int32_t s = 0; bool r = ee_won_cfgp_open_file(this->mObj, B(p), p.length(), &s); FileReaderCheck(s); return r; }
    void CloseFile() { ee_won_cfgp_close_file(this->mObj); }
    bool AddDefine(const std::string& n, const std::string& v, bool r)
    { return ee_won_cfgp_add_define(this->mObj, B(n), n.length(), B(v), v.length(), r); }
#ifdef EE_WON_CFGPARSER_DRIVER
#include "won_cfgparser_methods.inl" // the methods only the drivers call (tests/won)
#endif
};

// a registry key (RTTI RegKey, three virtual slots): the port keeps no registry; a key is a block of config.yaml
// (ee_won regkey.rs EeWonRegKey). R6 WON lobby and network round 37: the game makes its one key's calls on the Rust
// key (LobbyConfig.cpp); the C++ class over it is the drivers' (tests/won/won_regkey.h).

class LobbyPersistentData;
class LobbyContainer;
class RustLogic; class WindowEventBase;
class ChatCommandParser;
// the lobby's private state (0x118 bytes; constructor 0x7628c0, destructor 0x7630a0; created by 0x761fb0).
// This revision has no BigFileList after mInitError (later SDKs add one). R6 WON lobby and network round 38: the
// plain members (the version status, the terms-of-use times, the product ids, the lobby port, the firewall flags,
// the public address) are lobbyprv.rs's (EeWonLobbyPrvField); the never-read mLastTimerTick is gone.
class WONLobbyPrv : public RefCount {
public:
    WONLobbyPrv();
    virtual ~WONLobbyPrv(); // 0x7630a0 (scalar deleting destructor 0x763080)
    bool Init(ResourceConfigTable*,void (*)(ComponentConfig*));
    void InitAuth(const std::wstring&,const char* = NULL);
    void Destroy();
    SmartPtr<LobbyPersistentData> mPersistentData; // 0x08
    SmartPtr<LobbyConfig> mConfig; // 0x0c
    SmartPtr<LobbyContainer> mLobbyContainer; // 0x10
    GUIString mInitError; // 0x14
    SmartPtr<RustLogic> mAccountLogic; // 0x18
    SmartPtr<RustLogic> mDialogLogic; // 0x1c
    SmartPtr<RustLogic> mInitLogic; // 0x20
    SmartPtr<RustLogic> mRoutingLogic; // 0x24
    SmartPtr<RustLogic> mStagingLogic; // 0x28
    SmartPtr<RustLogic> mLanLogic; // 0x2c
    SmartPtr<RustLogic> mPingLogic; // 0x30 (a RustLogic of the Ping kind since R6 WON round 39)
    LobbyLogicMgr mLogicMgr; // 0x34..0x40
    SmartPtr<UDPManager> mUDPManager; // 0x40
    SmartPtr<WindowEventBase> mTimerEvent; // 0x44
    // (mLobbyStatus +0x4c, mRunMode +0x50 and mSuspended +0x54 are lobbyprv.rs's since R6 WON lobby/net round 40)
    SmartPtr<ServerContext> mAccountServers; // 0x58
    SmartPtr<ServerContext> mAuthServers; // 0x5c
    SmartPtr<ServerContext> mDirServers; // 0x60
    SmartPtr<ServerContext> mRoomDirServers; // 0x64
    SmartPtr<ServerContext> mFirewallDetectServers; // 0x68
    SmartPtr<ServerContext> mPatchServers; // 0x6c
    SmartPtr<AuthContext> mAuthContext; // 0x74
    CDKey mCDKey; // 0x78..0x94 (0x1c bytes)
    std::wstring mLoginCommunity; // 0x94..0xa4
    // (mHasSetHashFileList +0xa4 and mHashFileList +0xa8 are lobbyprv.rs's since R6 WON round 39)
    std::string mSysTOUPath,mGameTOUPath; // 0xc8, 0xd8
    SmartPtr<ChatCommandParser> mChatCommandParser; // 0x110
    LobbyGameType mCurGameType; // 0x114..0x118
};
extern SmartPtr<WONLobbyPrv> gLobbyData;     // 0x96630c (an intrusive SmartPtr)
// LobbyConfig::mDefaultFileName is 0x966310; declared in WONLobbyGUI.h.
}
#endif
