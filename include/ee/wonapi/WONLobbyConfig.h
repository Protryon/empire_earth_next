// WONLobby configuration: WONAPI's config file parser and registry key, and the lobby's private state
#ifndef EE_WONAPI_WONLOBBYCONFIG_H
#define EE_WONAPI_WONLOBBYCONFIG_H
#include "rust/ee_won.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"
#include <vector>
#include "ee/wonapi/WONStringCompare.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyLogic.h"
namespace WONAPI {

// A config file parser (0x88 bytes; constructor 0x7b1d20, destructor 0x7b1e10) and a saved parser position
// (0x28 bytes). The constructor leaves mIsUnicode (+0x5d) uninitialized.
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
public:
    ConfigParser();
    // the process-wide line count (0x966b54) is ee_won's (rust/crates/ee_won/src/config.rs)
    static void ResetLineCount() { ee_won_config_reset_line_count(); }
    static int GetLineCount() { return ee_won_config_line_count(); }
    virtual ~ConfigParser(); // 0x7b1e10 (closes the file; the only virtual)
    bool ReadToken(std::string&, bool inToEndLine=false, bool inSkip=false);
    bool ReadToken(std::wstring&, bool inToEndLine=false, bool inSkip=false);
    void SetParent(ConfigParser*);
    void SetError(const std::string&, bool);
    void AddWarning(const std::string&);
    void CopyWarnings(ConfigParser&);
    const std::string& GetAbortReason();
    bool IsStopped() { return this->mStopped; }
    void Stop() { this->mStopped=true; }
    bool GetIgnorePathOnIncludeFile() { return this->mIgnorePathOnIncludeFile; }
    bool EndOfFile();
    int GetChar();
    int PeekChar();
    void UngetChar(int);
    void UngetStr(const std::string&);
    void UngetStr(const std::wstring&);
    void SkipWhitespace();
    void SkipLine();
    void SkipBlockComment();
    bool OpenFile(const std::string&);
    void CloseFile();
    void Reset();
    void PushDefineStack();
    void PopDefineStack();
    bool AddDefine(const std::string&, const std::string&, bool);
    bool GetDefine(const std::string&, std::string&);
    bool GetDefine(const std::string&, std::wstring&);
    bool AddGroupDefine(const std::string&, ConfigParserState*);
    bool GetGroupDefine(const std::string&, ConfigParserState&);
    void GetState(ConfigParserState&);
    bool SetState(const ConfigParserState&);
    ConfigParser* Root(); // the topmost parent (the defines' owner)
    EeWonConfigIo Io(); // the members ee_won reads and writes in a call
    // The characters, tokens, pushback and define scopes are ee_won's (config.rs): the pushback (0x2c) and the
    // define stack (0x04..0x14: scopes of a define map and a group define map) live in mLexer.
    EeWonConfigLexer* mLexer;
    ConfigParser* mParent; // 0x14
    FileReader mFileReader; // 0x18..0x2c (0x14 bytes)
    std::string mAbortReason; // 0x3c
    std::string mWarnings; // 0x4c
    bool mWarningsAsErrors; // 0x5c
    bool mIsUnicode; // 0x5d (the original constructor left it uninitialized; false now)
    int mLineNum; // 0x60
    std::string mFilePath; // 0x64
    std::string mFileNameAndPath; // 0x74
    bool mIgnorePathOnIncludeFile; // 0x84
    bool mStopped; // 0x85 (0x88 bytes in all)
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
    GetResult GetNextSubKey(RegKey&) const { return NoMore; }
    GetResult GetFirstValueName(std::string&, DataType&) const { return NotFound; } // nor their keys
    GetResult GetNextValueName(std::string&, DataType&) const { return NoMore; }
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
    void CloseKey() { ee_won_regkey_op(this->mKey, EE_WON_REGKEY_CLOSE, NULL, 0, NULL, 0, NULL, NULL, NULL); }
    bool GetToBuf(const std::string& inName, std::string& outText) const { return this->Op(EE_WON_REGKEY_GET_TO_BUF, inName, &outText) != 0; }
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
class AccountLogic; class DialogLogic; class InitLogic; class RoutingLogic;
class StagingLogic; class LanLogic; class PingLogic; class WindowEventBase;
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
    SmartPtr<AccountLogic> mAccountLogic; // 0x18
    SmartPtr<DialogLogic> mDialogLogic; // 0x1c
    SmartPtr<InitLogic> mInitLogic; // 0x20
    SmartPtr<RoutingLogic> mRoutingLogic; // 0x24
    SmartPtr<StagingLogic> mStagingLogic; // 0x28
    SmartPtr<LanLogic> mLanLogic; // 0x2c
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
void StringToUpperCaseInPlace(std::string&); // 0x6a69a0 (StringUtil.cpp)
std::string WStringToString(const std::wstring&); // 0x6a6280 (StringUtil.cpp)
std::wstring StringToWString(const std::string& inString);
}
#endif
