// WONLobby configuration: WONAPI's config file parser and registry key, and the lobby's private state
#ifndef EE_WONAPI_WONLOBBYCONFIG_H
#define EE_WONAPI_WONLOBBYCONFIG_H
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
protected:
    static int mLineCount; // 0x966b54 (static: no storage in the object)
public:
    ConfigParser(); // 0x7b1d20
    static void ResetLineCount() { ConfigParser::mLineCount=0; }
    static int GetLineCount() { return ConfigParser::mLineCount; }
    virtual ~ConfigParser(); // 0x7b1e10 (closes the file; the only virtual)
    bool ReadToken(std::string&, bool inToEndLine=false, bool inSkip=false); // 0x7b3d70
    bool ReadToken(std::wstring&, bool inToEndLine=false, bool inSkip=false); // 0x7b3d50
    void SetParent(ConfigParser*);
    void SetError(const std::string&, bool); // 0x7b1fb0
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
    struct DefineMapContainer; // defined after this class
    typedef std::vector<SmartPtr<DefineMapContainer> > DefineStack;
    DefineStack mDefineStack; // 0x04..0x14
    ConfigParser* mParent; // 0x14
    FileReader mFileReader; // 0x18..0x2c (0x14 bytes)
    std::wstring mBackStr; // 0x2c
    std::string mAbortReason; // 0x3c
    std::string mWarnings; // 0x4c
    bool mWarningsAsErrors; // 0x5c
    bool mIsUnicode; // 0x5d (the constructor leaves it uninitialized)
    int mLineNum; // 0x60
    std::string mFilePath; // 0x64
    std::string mFileNameAndPath; // 0x74
    bool mIgnorePathOnIncludeFile; // 0x84
    bool mStopped; // 0x85 (0x88 bytes in all)
};

// one scope of the parser's defines (0x20 bytes; the SDK's DefineStruct is from another revision)
struct ConfigParser::DefineMapContainer : public RefCount {
    typedef std::map<std::string, std::string, StringLessNoCase> DefineMap;
    typedef std::map<std::string, ConfigParserState, StringLessNoCase> GroupDefineMap;
    DefineMap mDefineMap; // 0x08 (0x0c bytes)
    GroupDefineMap mGroupDefineMap; // 0x14 (0x0c bytes; 0x20 bytes in all)
};

// a registry key (RTTI RegKey, three virtual slots; 0x2c bytes on the stack; constructor and destructor out of line)
class RegKey {
public:
    enum GetResult { Ok, NotFound, BadType, NoMore, NoSize };
    enum DataType { Long, String, Binary, Other };
    GetResult GetValue(const std::string&,std::string&) const;
    bool OpenSubKey(const std::string&, bool = false, bool = true);
    bool GetSubKey(const std::string&, RegKey&, bool = false, bool = true) const;
    GetResult GetFirstSubKey(RegKey&) const;
    GetResult GetNextSubKey(RegKey&) const;
    GetResult GetFirstValueName(std::string&, DataType&) const;
    GetResult GetNextValueName(std::string&, DataType&) const;
    bool SetValue(const std::string&, const std::string&);
    bool SetValue(const std::string&, unsigned long);
    bool SetValue(const std::string&, const void*, unsigned long);
    bool DeleteValue(const std::string&);
    RegKey(int inRoot, const std::string& inPath, bool inCreate, bool inNonvolatile); // 0x6a4260
    virtual ~RegKey();                      // 0x6a42e0
    virtual bool DeleteSubKey(const std::string& inKey); // 0x6a4830
    virtual bool DeleteSubKey(RegKey& inKey); // 0x6a4870 (unused overload; qualifiers inferred)
    int QueryDWORD(const std::string& inName, DWORD& outValue); // 0x6a4680
    bool OpenNewKey(int inRoot, const std::string& inPath, bool inCreate, bool inNonvolatile); // 0x6a4440
    void CloseKey(); // 0x6a4370
    DWORD GetToBuf(const std::string& inName, unsigned long& outLength) const; // 0x6a4390
    int QueryBinary(const std::string& inName, ConstSmartPtr<ByteBuffer>& outValue); // 0x6a46e0
    bool IsOpen() const { return this->mIsOpen; } // mIsOpen only; the handle itself is not checked
    const std::string& GetLeafName(void) const { return this->mPath; }
    HKEY mKey;                              // +0x04
    bool mIsOpen;                           // +0x08
    std::string mPath;                      // +0x0c
    mutable char* mBuffer;                          // +0x1c
    mutable DWORD mBufferLength;                    // registry query/enum buffer management
    mutable DWORD mSubKeyIndex;
    mutable DWORD mValueIndex;
private:
    HKEY GetRootHKey(int inRoot); // 0x6a41d0
    bool OpenNewKey(HKEY inRoot,const std::string& inPath,bool inCreate = false,bool inNonvolatile = true); // 0x6a4470
};

class LobbyPersistentData;
class LobbyContainer;
class AccountLogic; class DialogLogic; class InitLogic; class RoutingLogic;
class StagingLogic; class LanLogic; class PingLogic; class WindowEventBase;
class ChatCommandParser;
enum LobbyStatus; enum LobbyRunMode; enum LobbyGameType;
// the lobby's private state (0x118 bytes; constructor 0x7628c0, destructor 0x7630a0; created by 0x761fb0).
// This revision has no BigFileList after mInitError (later SDKs add one).
class WONLobbyPrv : public RefCount {
public:
    WONLobbyPrv(); // 0x7628c0
    virtual ~WONLobbyPrv(); // 0x7630a0 (scalar deleting destructor 0x763080)
    bool Init(ResourceConfigTable*,void (*)(ComponentConfig*)); // 0x765ac0
    void InitAuth(const std::wstring&,const char* = NULL); // 0x766010
    void Destroy(); // 0x7660f0
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
std::wstring StringToWString(const std::string& inString); // 0x6a6130
}
#endif
