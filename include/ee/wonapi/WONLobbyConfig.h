// WONLobby configuration: WONAPI's config file parser and registry key, and the lobby's private state
#ifndef EE_WONAPI_WONLOBBYCONFIG_H
#define EE_WONAPI_WONLOBBYCONFIG_H
#include "rust/won_net/config.h"
#include "rust/won_net/cfgparser.h"
#include "rust/won_net/regkey.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"
#include <vector>
#include "ee/wonapi/WONStringCompare.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyLogic.h"
namespace WONAPI {

// A config file parser and a saved parser position (0x28 bytes). The parser is ee_won's object whole
// (rust/crates/ee_won_net/src/cfgparser.rs: the reader, the pushback and define scopes, the parent, the texts, the
// flags; ConfigParser.cpp and ConfigToken.cpp are deleted): these are its inline forwards, which throw the
// reader's failures as the original did.
struct ConfigParserState {
    long mFilePos; // 0x00
    std::string mFileNameAndPath; // 0x04
    int mLineNum; // 0x14
    std::wstring mBackStr; // 0x18 (0x28 bytes in all)
};

class ConfigParser {
private:
    ConfigParser(const ConfigParser&);
    ConfigParser& operator=(const ConfigParser&);
    static void Take(void* c, const uint8_t* b, size_t n) { static_cast<std::string*>(c)->assign((const char*)b, n); }
    static void TakeWide(void* c, const uint32_t* u, size_t n)
    { std::wstring* w = static_cast<std::wstring*>(c); w->erase(); for (size_t i = 0; i < n; i++) *w += (wchar_t)u[i]; }
    static void TakeState(void* c, const EeWonConfigState* s)
    {
        ConfigParserState* o = static_cast<ConfigParserState*>(c);
        o->mFilePos = s->pos; o->mFileNameAndPath.assign((const char*)s->path, s->path_len); o->mLineNum = s->line;
        o->mBackStr.erase(); for (size_t i = 0; i < s->back_len; i++) o->mBackStr += (wchar_t)s->back[i];
    }
    static const uint8_t* B(const std::string& s) { return (const uint8_t*)s.data(); }
    std::string Text(EeWonCfgpText w) const { std::string s; ee_won_cfgp_text(this->mObj, w, Take, &s); return s; }
    int Field(EeWonCfgpField f) const { return ee_won_cfgp_field(this->mObj, f); }
    bool Read(std::string* n, std::wstring* w, bool e, bool k)
    { return ee_won_cfgp_read_token(this->mObj, n ? 0 : (uint32_t)sizeof(wchar_t), e, k, Take, TakeWide, n ? (void*)n : (void*)w); }
    EeWonCfgParser* mObj;
    std::string mAbortCopy; // GetAbortReason's answer
public:
    EeWonCfgParser* Obj() { return this->mObj; } // the Rust parser (ConfigObject's host)
    ConfigParser() : mObj(ee_won_cfgp_new()) {}
    ~ConfigParser() { ee_won_cfgp_free(this->mObj); }
    // the process-wide line count (0x966b54) is ee_won's (rust/crates/ee_won_net/src/config.rs)
    static void ResetLineCount() { ee_won_config_reset_line_count(); }
    static int GetLineCount() { return ee_won_config_line_count(); }
    bool ReadToken(std::string& t, bool inToEndLine=false, bool inSkip=false) { return this->Read(&t, NULL, inToEndLine, inSkip); }
    bool ReadToken(std::wstring& t, bool inToEndLine=false, bool inSkip=false) { return this->Read(NULL, &t, inToEndLine, inSkip); }
    void SetParent(ConfigParser* p) { ee_won_cfgp_set_parent(this->mObj, p ? p->mObj : NULL); }
    void SetError(const std::string& e, bool a) { ee_won_cfgp_set_error(this->mObj, B(e), e.length(), a); }
    void AddWarning(const std::string& w) { ee_won_cfgp_add_warning(this->mObj, B(w), w.length()); }
    void CopyWarnings(ConfigParser& s) { ee_won_cfgp_copy_warnings(this->mObj, s.mObj); }
    const std::string& GetAbortReason()
    { this->mAbortCopy.erase(); ee_won_cfgp_abort_reason(this->mObj, Take, &this->mAbortCopy); return this->mAbortCopy; }
    bool IsStopped() { return this->Field(EeWonCfgpField::Stopped) != 0; }
    void Stop() { ee_won_cfgp_set_field(this->mObj, EeWonCfgpField::Stopped, 1); }
    bool GetIgnorePathOnIncludeFile() { return this->Field(EeWonCfgpField::IgnorePath) != 0; }
    // the members the original's callers read: mAbortReason.empty(), mIsUnicode, mFilePath, mLineNum, ...
    bool HasNoError() const { return this->Field(EeWonCfgpField::NoError) != 0; }
    bool IsUnicode() const { return this->Field(EeWonCfgpField::Unicode) != 0; }
    int GetLineNum() const { return this->Field(EeWonCfgpField::Line); }
    bool IsFileOpen() const { return this->Field(EeWonCfgpField::Open) != 0; }
    bool GetWarningsAsErrors() const { return this->Field(EeWonCfgpField::WarningsAsErrors) != 0; }
    void SetWarningsAsErrors(bool on) { ee_won_cfgp_set_field(this->mObj, EeWonCfgpField::WarningsAsErrors, on); }
    void SetIgnorePathOnIncludeFile(bool on) { ee_won_cfgp_set_field(this->mObj, EeWonCfgpField::IgnorePath, on); }
    std::string GetAbortText() const { return this->Text(EeWonCfgpText::Abort); } // as it is (no "Parse aborted.")
    std::string GetWarnings() const { return this->Text(EeWonCfgpText::Warnings); }
    std::string GetFilePath() const { return this->Text(EeWonCfgpText::FilePath); }
    std::string GetFileNameAndPath() const { return this->Text(EeWonCfgpText::FileName); }
    bool EndOfFile() { return ee_won_cfgp_end_of_file(this->mObj); }
    int GetChar() { return ee_won_cfgp_get_char(this->mObj); }
    int PeekChar() { return ee_won_cfgp_peek_char(this->mObj); }
    void UngetChar(int c) { ee_won_cfgp_unget_char(this->mObj, c); }
    void UngetStr(const std::string& s) { ee_won_cfgp_unget_narrow(this->mObj, B(s), s.length()); }
    void UngetStr(const std::wstring& s)
    { std::vector<uint32_t> u(s.begin(), s.end()); ee_won_cfgp_unget_wide(this->mObj, u.empty() ? NULL : &u[0], u.size()); }
    void SkipWhitespace() { ee_won_cfgp_skip_whitespace(this->mObj); }
    bool OpenFile(const std::string& p)
    { int32_t s = 0; bool r = ee_won_cfgp_open_file(this->mObj, B(p), p.length(), &s); FileReaderCheck(s); return r; }
    void CloseFile() { ee_won_cfgp_close_file(this->mObj); }
    void Reset() { ee_won_cfgp_reset(this->mObj); }
    void PushDefineStack() { ee_won_cfgp_push_define_stack(this->mObj); }
    void PopDefineStack() { ee_won_cfgp_pop_define_stack(this->mObj); }
    bool AddDefine(const std::string& n, const std::string& v, bool r)
    { return ee_won_cfgp_add_define(this->mObj, B(n), n.length(), B(v), v.length(), r); }
    bool GetDefine(const std::string& n, std::string& v)
    { std::string t; if (!ee_won_cfgp_get_define(this->mObj, B(n), n.length(), Take, &t)) return false; v = t; return true; }
    bool GetDefine(const std::string& n, std::wstring& v)
    { std::string t; if (!this->GetDefine(n, t)) return false; v = StringToWString(t); return true; }
    bool AddGroupDefine(const std::string& n, ConfigParserState* s)
    {
        if (s == NULL) return ee_won_cfgp_add_group_define(this->mObj, B(n), n.length(), NULL);
        std::vector<uint16_t> b(s->mBackStr.begin(), s->mBackStr.end());
        EeWonConfigState e = { s->mFilePos, B(s->mFileNameAndPath), s->mFileNameAndPath.length(), s->mLineNum,
                               b.empty() ? NULL : &b[0], b.size() };
        return ee_won_cfgp_add_group_define(this->mObj, B(n), n.length(), &e);
    }
    bool GetGroupDefine(const std::string& n, ConfigParserState& s)
    { return ee_won_cfgp_get_group_define(this->mObj, B(n), n.length(), TakeState, &s); }
    void GetState(ConfigParserState& s) { ee_won_cfgp_get_state(this->mObj, TakeState, &s); }
    bool SetState(const ConfigParserState& s)
    {
        std::vector<uint16_t> b(s.mBackStr.begin(), s.mBackStr.end());
        EeWonConfigState e = { s.mFilePos, B(s.mFileNameAndPath), s.mFileNameAndPath.length(), s.mLineNum,
                               b.empty() ? NULL : &b[0], b.size() };
        int32_t st = 0;
        bool r = ee_won_cfgp_set_state(this->mObj, &e, &st);
        FileReaderCheck(st);
        return r;
    }
};

// a registry key (RTTI RegKey, three virtual slots; constructor and destructor out of line). The port keeps no
// registry: a key is a block of config.yaml (platform/PlatConfig.h), "won.<path with \ as .>" (the root is
// dropped), its values that block's keys: a DWORD an integer, a string a string, binary data a hex string. Opening
// always succeeds for a non-empty path (a missing key reads as an empty one); keys and values are not enumerated.
class RegKey {
public:
    enum GetResult { Ok, NotFound, BadType, NoMore, NoSize };
    enum DataType { Long, String, Binary, Other };
    // the key's block, path and open flag and every method are ee_won's (regkey.rs EeWonRegKey; RegKey.cpp is
    // deleted: net round 27)
    RegKey(int inRoot, const std::string& inPath, bool, bool)
        : mKey(ee_won_regkey_new(inRoot, B(inPath), inPath.size())) {}
    virtual ~RegKey() { ee_won_regkey_free(this->mKey); }
    GetResult GetValue(const std::string& inName, std::string& outValue) const // any value reads as a string
    { outValue.erase(); return this->Op(EE_WON_REGKEY_GET_VALUE, inName, &outValue) ? Ok : NotFound; }
    bool OpenSubKey(const std::string& inName, bool = false, bool = true) { return this->Op(EE_WON_REGKEY_OPEN_SUBKEY, inName) != 0; }
    bool GetSubKey(const std::string& inName, RegKey& outKey, bool = false, bool = true) const
    { return ee_won_regkey_child(this->mKey, outKey.mKey, false, B(inName), inName.size()); }
    GetResult GetFirstSubKey(RegKey&) const { return NotFound; } // config.yaml blocks are not enumerated
    GetResult GetFirstValueName(std::string&, DataType&) const { return NotFound; } // nor their keys
    bool SetValue(const std::string& inName, const std::string& inValue)
    { return this->Op(EE_WON_REGKEY_SET_TEXT, inName, NULL, NULL, B(inValue), inValue.size()) != 0; }
    bool SetValue(const std::string& inName, unsigned long inValue) // a REG_DWORD: 32 bits
    { long long n = (long long)inValue; return this->Op(EE_WON_REGKEY_SET_DWORD, inName, NULL, &n) != 0; }
    bool SetValue(const std::string& inName, const void* inData, unsigned long inLength) // as hex text
    { return this->Op(EE_WON_REGKEY_SET_BINARY, inName, NULL, NULL, static_cast<const uint8_t*>(inData), inLength) != 0; }
    bool DeleteValue(const std::string& inName) { return this->Op(EE_WON_REGKEY_DELETE_VALUE, inName) != 0; }
    virtual bool DeleteSubKey(const std::string& inKey) { return this->Op(EE_WON_REGKEY_DELETE_SUBKEY, inKey) != 0; }
    virtual bool DeleteSubKey(RegKey& inKey) { return ee_won_regkey_child(this->mKey, inKey.mKey, true, NULL, 0); }
    // 0 and the value, 2 for a value that is no integer, 1 when missing (the value 0 then)
    int QueryDWORD(const std::string& inName, DWORD& outValue)
    { long long n = 0; int r = this->Op(EE_WON_REGKEY_QUERY_DWORD, inName, NULL, &n); outValue = (DWORD)n; return r; }
    bool OpenNewKey(int inRoot, const std::string& inPath, bool, bool)
    { long long n = inRoot; return this->Op(EE_WON_REGKEY_OPEN_NEW, inPath, NULL, &n) != 0; }
    // 0, 2 wrong type, 4 empty (the caller's buffer left alone) or 1 missing
    int QueryBinary(const std::string& inName, ConstSmartPtr<ByteBuffer>& outValue)
    {
        std::string aBytes;
        int r = this->Op(EE_WON_REGKEY_QUERY_BINARY, inName, &aBytes);
        if (r == 0) outValue = new ByteBuffer(aBytes.data(), aBytes.length(), false);
        return r;
    }
    bool IsOpen() const { return ee_won_regkey_op(this->mKey, EE_WON_REGKEY_IS_OPEN, NULL, 0, NULL, 0, NULL, NULL, NULL) != 0; }
    std::string GetLeafName() const { std::string s; this->Op(EE_WON_REGKEY_LEAF_NAME, std::string(), &s); return s; }
    std::string GetBlock() const { std::string s; this->Op(EE_WON_REGKEY_BLOCK, std::string(), &s); return s; } // was mBlock
    EeWonRegKey* mKey;
private:
    RegKey(const RegKey&);
    RegKey& operator=(const RegKey&);
    static const uint8_t* B(const std::string& inText) { return reinterpret_cast<const uint8_t*>(inText.data()); }
    static void Sink(void* inText, const uint8_t* inBytes, size_t inLength)
    { static_cast<std::string*>(inText)->assign(reinterpret_cast<const char*>(inBytes), inLength); }
    int Op(int inWhich, const std::string& inName, std::string* outText = NULL, long long* ioNum = NULL,
           const uint8_t* inData = NULL, size_t inLength = 0) const
    { return ee_won_regkey_op(this->mKey, inWhich, B(inName), inName.size(), inData, inLength, ioNum, outText ? Sink : NULL, outText); }
};

class LobbyPersistentData;
class LobbyContainer;
class RustLogic; class PingLogic; class WindowEventBase;
class ChatCommandParser;
enum LobbyStatus : int; enum LobbyRunMode : int; // (values only by number); LobbyGameType: WONLobbyGUI.h
// the lobby's private state (0x118 bytes; constructor 0x7628c0, destructor 0x7630a0; created by 0x761fb0).
// This revision has no BigFileList after mInitError (later SDKs add one).
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
    SmartPtr<PingLogic> mPingLogic; // 0x30
    LobbyLogicMgr mLogicMgr; // 0x34..0x40
    SmartPtr<UDPManager> mUDPManager; // 0x40
    SmartPtr<WindowEventBase> mTimerEvent; // 0x44
    DWORD mLastTimerTick; // 0x48 (the constructor leaves it unset)
    LobbyStatus mLobbyStatus; // 0x4c
    LobbyRunMode mRunMode; // 0x50
    bool mSuspended; // 0x54
    SmartPtr<ServerContext> mAccountServers; // 0x58
    SmartPtr<ServerContext> mAuthServers; // 0x5c
    SmartPtr<ServerContext> mDirServers; // 0x60
    SmartPtr<ServerContext> mRoomDirServers; // 0x64
    SmartPtr<ServerContext> mFirewallDetectServers; // 0x68
    SmartPtr<ServerContext> mPatchServers; // 0x6c
    WONStatus mVersionStatus; // 0x70
    SmartPtr<AuthContext> mAuthContext; // 0x74
    CDKey mCDKey; // 0x78..0x94 (0x1c bytes)
    std::wstring mLoginCommunity; // 0x94..0xa4
    bool mHasSetHashFileList; // 0xa4
    typedef std::list<std::string> HashFileList;
    HashFileList mHashFileList; // 0xa8..0xb4
    unsigned short mProductId; // 0xb4
    unsigned short mLanProductId; // 0xb6
    unsigned short mLobbyPort; // 0xb8
    time_t mSysTOUTime,mGameTOUTime; // 0xbc, 0xc0
    bool mBehindFirewall,mFirewallTestDone; // 0xc4, 0xc5
    std::string mSysTOUPath,mGameTOUPath; // 0xc8, 0xd8
    IPAddr mPublicAddr; // 0xe8..0x110 (0x28 bytes)
    SmartPtr<ChatCommandParser> mChatCommandParser; // 0x110
    LobbyGameType mCurGameType; // 0x114..0x118
};
extern SmartPtr<WONLobbyPrv> gLobbyData;     // 0x96630c (an intrusive SmartPtr)
// LobbyConfig::mDefaultFileName is 0x966310; declared in WONLobbyGUI.h.
extern bool gLobbyUseLocalIP;                // 0x96634d; a config key writes it, nothing reads it
}
#endif
