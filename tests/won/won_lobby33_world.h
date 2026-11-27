// won_lobby33_test's stand-in world (R6 WON round 33): the classes and functions WONLobbyPrv's methods and the lobby
// model's buffer methods name (WONLobbyPrv.cpp, LobbyModelHost.cpp), each call with an effect logged in one trace.
// The deleted bodies (won_lobby33_orig.h) and the hosts' new one-statement cases (won_lobby33_new.h) run over it.
// The objects that hold references are numbered in order; their references are compared at the end of each case,
// not traced (a temporary SmartPtr adds and drops one), and the texts are compared by their contents.
#ifndef WON_LOBBY33_WORLD_H
#define WON_LOBBY33_WORLD_H
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "ee/wonapi/WONLobbyGuard.h"
#include "rust/won_gui/cfgkind.h"
#include "rust/won_gui/shell.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/lobbyhost.h"
#include "rust/won_lobby/model.h"
#include "rust/won_lobby/staging.h"
#include "rust/won_lobby/srvlistctl.h"
#include "rust/won_lobby/chatctl.h"
#include "rust/won_net/ipobj.h"
#include "rust/won_gui/guiconf.h"

namespace L33World {
using WONAPI::WonGuard;
using WONAPI::WonGuardV;
using WONAPI::WonRethrow;
static std::vector<std::string> sTrace;
static void Log(const std::string& s) { sTrace.push_back(s); }
static std::string N(long long v) { return std::to_string(v); }
static std::string Hex(const void* p, size_t n)
{
    std::string s;
    char b[4];
    for (size_t i = 0; i < n; i++) {
        snprintf(b, sizeof b, "%02x", static_cast<const unsigned char*>(p)[i]);
        s += b;
    }
    return s;
}

// ---- references

struct RefCount;
static std::vector<RefCount*> sObjects;
struct RefCount {
    int mRefs = 0;
    int mId = -1;
    virtual ~RefCount() {}
    void Track() { this->mId = (int)sObjects.size(); sObjects.push_back(this); }
    RefCount* CreateRef() { this->mRefs++; return this; }
    void AddRef() const { const_cast<RefCount*>(this)->mRefs++; }
    void Release() const { const_cast<RefCount*>(this)->mRefs--; }
};
struct Tracked : RefCount {
    Tracked() { this->Track(); }
};
static std::string Id(const void* p) { return p == NULL ? "null" : "#" + N(static_cast<const RefCount*>(p)->mId); }
// LobbyModelHost.cpp's
static intptr_t Held(RefCount* inObj)
{
    if (inObj != NULL) inObj->AddRef();
    return (intptr_t)inObj;
}
template <class T> class SmartPtr {
public:
    SmartPtr() : mPtr(NULL) {}
    SmartPtr(T* p) : mPtr(p ? (T*)p->CreateRef() : NULL) {}
    SmartPtr(const SmartPtr& o) : mPtr(o.mPtr ? (T*)o.mPtr->CreateRef() : NULL) {}
    ~SmartPtr() { if (this->mPtr != NULL) this->mPtr->Release(); }
    T* operator=(T* p)
    {
        if (this->mPtr != p) {
            if (this->mPtr != NULL) this->mPtr->Release();
            this->mPtr = p ? (T*)p->CreateRef() : NULL;
        }
        return p;
    }
    SmartPtr& operator=(const SmartPtr& o) { this->operator=(o.mPtr); return *this; }
    T* operator->() const { return this->mPtr; }
    T* get() const { return this->mPtr; }
    operator T*() const { return this->mPtr; }
    T* mPtr;
};
template <class T> using ConstSmartPtr = SmartPtr<T>;

// ---- texts

struct GUIStringData : RefCount {
    std::wstring mWide;
    const wchar_t* mText; // the real one's units
    unsigned short mLength;
    explicit GUIStringData(const std::wstring& w) : mWide(w), mText(mWide.c_str()), mLength((unsigned short)w.size()) {}
    explicit GUIStringData(const char* s) : mWide(s, s + strlen(s)), mText(mWide.c_str()), mLength((unsigned short)strlen(s)) {}
    GUIStringData(const char* s, unsigned short n) : mWide(s, s + n), mText(mWide.c_str()), mLength(n) {}
};
static GUIStringData* GetEmptyGUIStringData()
{
    static GUIStringData* sEmpty = new GUIStringData(std::wstring());
    return sEmpty;
}
static std::string Narrow(const std::wstring& w)
{
    std::string s;
    for (wchar_t c : w) s.push_back((char)c);
    return s;
}
struct GUIString {
    SmartPtr<GUIStringData> mData;
    GUIString() : mData(GetEmptyGUIStringData()) {}
    GUIString(GUIStringData* d) : mData(d) {}
    GUIString(const char* s) : mData(new GUIStringData(s)) {}
    GUIString(const std::string& s) : mData(new GUIStringData(s.c_str())) {}
    GUIString(const std::wstring& w) : mData(new GUIStringData(w)) {}
    void append(const GUIString& o) { this->mData = new GUIStringData(this->mData->mWide + o.mData->mWide); }
    int length() const { return this->mData->mLength; }
    operator std::wstring() const { return this->mData->mWide; }
    operator std::string() const { return Narrow(this->mData->mWide); }
    bool operator==(const GUIString& o) const { return this->mData->mWide == o.mData->mWide; }
    int CompareNoCase(const char* s, int) const
    {
        std::string a = Narrow(this->mData->mWide), b(s);
        for (char& c : a) c = (char)tolower((unsigned char)c);
        for (char& c : b) c = (char)tolower((unsigned char)c);
        return a.compare(b);
    }
};
static std::string Text(const GUIString& s) { return "'" + Narrow(s.mData->mWide) + "'"; }
static std::string TextOf(intptr_t inData)
{
    return inData == 0 ? std::string("null") : "'" + Narrow(reinterpret_cast<GUIStringData*>(inData)->mWide) + "'";
}
static const GUIString& LobbyStr(const char* inName)
{
    static std::map<std::string, GUIString> sStrings;
    std::map<std::string, GUIString>::iterator it = sStrings.find(inName);
    if (it == sStrings.end()) it = sStrings.insert(std::make_pair(std::string(inName), GUIString(inName))).first;
    return it->second;
}
#define LOBBY_STRING(n) LobbyStr(#n)
static void WideAppendUtf16LE(std::string& outBytes, const std::wstring& inText)
{
    for (wchar_t c : inText) {
        outBytes.push_back((char)(c & 0xff));
        outBytes.push_back((char)((c >> 8) & 0xff));
    }
}
static void WideAppendUtf16Unit(std::wstring& ioText, uint16_t inUnit) { ioText += (wchar_t)inUnit; }
static void WonWideSink(void* inContext, const uint16_t* inUnits, size_t inCount)
{
    std::wstring& t = *static_cast<std::wstring*>(inContext);
    t.assign(inUnits, inUnits + inCount);
}
struct StringLessNoCase {
    bool operator()(const std::string& a, const std::string& b) const
    {
        std::string x, y;
        for (char c : a) x.push_back((char)tolower((unsigned char)c));
        for (char c : b) y.push_back((char)tolower((unsigned char)c));
        return x < y;
    }
};
static std::wstring StringToWString(const std::string& s) { return std::wstring(s.begin(), s.end()); }

// ---- the world's answers (each case sets them, alike for the two sides)

struct WorldThrow {
    int mWhich;
};
struct ConfigObjectException {
    std::string mText;
    explicit ConfigObjectException(const std::string& t) : mText(t) {}
};
struct ReadBufferException {};
// 0 false, 1 true, 2 a WorldThrow, 3 a ConfigObjectException
static int sOpen, sParse, sReadMaster, sLogicInit, sInitFunc;

// ---- WONLobbyPrv's world

struct WONAPICore {
    static std::string GetDefaultFileDirectory() { return "D:\\EE\\"; }
};
struct ConfigParser {
    int mId;
    static int sNext;
    ConfigParser() : mId(sNext++) { Log("new ConfigParser " + N(this->mId)); }
    ~ConfigParser() { Log("~ConfigParser " + N(this->mId)); }
    void SetWarningsAsErrors(bool b) { Log("parser SetWarningsAsErrors " + N(b)); }
    bool OpenFile(const std::string& p)
    {
        Log("parser OpenFile " + p);
        if (sOpen == 2) throw WorldThrow{1};
        return sOpen != 0;
    }
    const std::string& GetAbortReason()
    {
        static std::string sReason = "line 7: bad";
        Log("parser GetAbortReason");
        return sReason;
    }
    void AddDefine(const std::string& k, const std::string& v, bool b) { Log("parser AddDefine " + k + "=" + v + " " + N(b)); }
};
int ConfigParser::sNext;
struct Component : Tracked {};
// a config reader's tokens: a string, an int, a bool, a colour, a comma, a component (by number)
struct Tok {
    int mKind;
    std::string mText;
    int mValue;
};
static int sBaseRead; // the base readers: 0 false, 1 true, 2 a ConfigObjectException
static std::map<std::string, Component*> sNamed;
static std::vector<Component*> sReadComponents;
static std::vector<GUIStringData*> sReadTexts; // the texts the readers read: their references are compared
struct Image : Tracked {};
struct Font : Tracked {};
struct ComponentConfig : Tracked {
    GUIString mConfigError = GUIString("the master's error");
    static void SetResourceFolder(const std::string& s) { Log("SetResourceFolder " + s); }
    std::vector<Tok> mToks;
    size_t mAt = 0;
    const Tok& Next(int inKind, const char* inWhat)
    {
        if (this->mAt >= this->mToks.size() || this->mToks[this->mAt].mKind != inKind) {
            Log(std::string("config read fails: ") + inWhat);
            throw ConfigObjectException(std::string("Expected ") + inWhat);
        }
        return this->mToks[this->mAt++];
    }
    GUIString ReadString(bool)
    {
        const Tok& k = this->Next(0, "string");
        Log("ReadString " + k.mText);
        GUIStringData* d = new GUIStringData(k.mText.c_str());
        sReadTexts.push_back(d);
        return GUIString(d);
    }
    int ReadColor() { const Tok& k = this->Next(3, "color"); Log("ReadColor " + N(k.mValue)); return k.mValue; }
    bool ReadBool() { const Tok& k = this->Next(2, "bool"); Log("ReadBool " + N(k.mValue)); return k.mValue != 0; }
    int ReadInt() { const Tok& k = this->Next(1, "int"); Log("ReadInt " + N(k.mValue)); return k.mValue; }
    void EnsureComma() { this->Next(4, "comma"); Log("EnsureComma"); }
    bool EndOfString() { Log("EndOfString"); return this->mAt >= this->mToks.size(); }
    Component* ReadComponent() { const Tok& k = this->Next(5, "component"); Log("ReadComponent"); return sReadComponents[k.mValue]; }
    SmartPtr<Image> ReadImage(bool) { this->Next(0, "image"); Image* i = new Image; Log("ReadImage " + Id(i)); return i; }
    SmartPtr<Font> ReadFont();
    Component* Fn386860(const std::string& n)
    {
        std::map<std::string, Component*>::iterator it = sNamed.find(n);
        Component* c = it == sNamed.end() ? NULL : it->second;
        Log("Fn386860 " + n + " " + Id(c));
        return c;
    }
    virtual bool Virtual24(const std::string& k)
    {
        Log("base Virtual24 " + k);
        if (sBaseRead == 2) throw ConfigObjectException("the base's error");
        return sBaseRead != 0;
    }
    virtual bool GetNamedValue(const std::string& k, int& out)
    {
        Log("base GetNamedValue " + k);
        if (sBaseRead == 2) throw ConfigObjectException("the base's error");
        out = 77;
        return sBaseRead != 0;
    }
    static std::string GetResourceFile(const std::string& p) { Log("GetResourceFile " + p); return "res/" + p; }
};
struct Container : Component {};
struct LobbyContainer : Container {
    LobbyContainer() { Log("new LobbyContainer " + Id(this)); }
};
struct ResourceConfigTable;
struct ContainerConfig : ComponentConfig {
    ContainerConfig() { Log("new ContainerConfig " + Id(this)); }
    bool ReadConfig_Unsafe(Component* c, const char* f, ResourceConfigTable* t, ConfigParser* p)
    {
        Log("ReadConfig_Unsafe " + Id(c) + " " + f + " " + Id(t) + " parser " + N(p->mId));
        if (sReadMaster == 2) throw WorldThrow{3};
        return sReadMaster != 0;
    }
};
struct ResourceConfigTable : Tracked {
    template <class A, class B> void AddAllocator(const char* n, A a, B b)
    {
        Log("AddAllocator " + Id(this) + " " + n + " " + N((long long)(intptr_t)a) + " " + N((long long)(intptr_t)b));
    }
};
typedef SmartPtr<ResourceConfigTable> ResourceConfigTablePtr;
static ResourceConfigTablePtr LobbyResource_Init(ResourceConfigTable* t)
{
    ResourceConfigTable* aTable = new ResourceConfigTable;
    Log("LobbyResource_Init " + Id(t) + " -> " + Id(aTable));
    return aTable;
}
static void WONGUIConfig_Init(ResourceConfigTable* t) { Log("WONGUIConfig_Init " + Id(t)); }
// the resource table's factories (their addresses are logged: the same instances on both sides)
template <class T> static ComponentConfig* NewConfigOf() { return NULL; }
template <EeWonCfgKind K> static ComponentConfig* NewConfigOf() { return NULL; }
template <class T> static Component* NewComponentOf() { return NULL; }
template <EeWonLobbyDlgKind K> static Component* NewLobbyDialog() { return NULL; }
enum Direction { Direction_Up };
template <EeWonShellKind K, Direction D> static Component* NewShellOf() { return NULL; }
struct LobbyRustCtrl : Container { explicit LobbyRustCtrl(EeWonLobbyCtlKind) {} };
struct LobbyRustDialogCtrl : Container { explicit LobbyRustDialogCtrl(EeWonLobbyCtlKind) {} };
struct RootScreen : Container { explicit RootScreen(bool) {} };
template <EeWonLobbyCtlKind K> static Component* CtlFactory() { return new LobbyRustCtrl(K); }
template <EeWonLobbyCtlKind K> static Component* DlgCtlFactory() { return new LobbyRustDialogCtrl(K); }
template <bool Lan> static Component* ScreenFactory() { return new RootScreen(Lan); }
typedef EeWonLobbyCtlKind Ck;
struct BrowserComponent {};
struct ChatCtrlConfig;
struct ChatCtrl;
struct GameBrowserCtrlConfig;
struct GameBrowserCtrl;
struct GameUserListCtrlConfig;
struct UserListCtrlConfig;
struct LoginHelpCtrl {};
struct LoginScreen {};
struct MOTDCtrl {};
struct RoomLabelCtrlConfig;
struct RoomLabelCtrl;
struct ServerListCtrlConfig;
struct ServerListCtrl;
struct CreditsDialogConfig;
struct CrossPromotionButtonConfig;
struct CrossPromotionButton;
struct ImageTextButtonConfig;
struct ImageTextButton;

struct LobbyLogic : Tracked {
    const char* mName;
    explicit LobbyLogic(const char* n) : mName(n) { Log(std::string("new ") + n + " " + Id(this)); }
};
struct AccountLogic : LobbyLogic { AccountLogic() : LobbyLogic("AccountLogic") {} };
struct DialogLogic : LobbyLogic { DialogLogic() : LobbyLogic("DialogLogic") {} };
struct InitLogic : LobbyLogic { InitLogic() : LobbyLogic("InitLogic") {} };
struct RoutingLogic : LobbyLogic { RoutingLogic() : LobbyLogic("RoutingLogic") {} };
struct StagingLogic : LobbyLogic { StagingLogic() : LobbyLogic("StagingLogic") {} };
struct LanLogic : LobbyLogic { LanLogic() : LobbyLogic("LanLogic") {} };
struct PingLogic : LobbyLogic { PingLogic() : LobbyLogic("PingLogic") {} };
struct LobbyLogicMgr {
    void Init(ComponentConfig* c)
    {
        Log("LogicMgr Init " + Id(c));
        if (sLogicInit == 2) throw WorldThrow{4};
        if (sLogicInit == 3) throw ConfigObjectException("a logic's config error");
    }
    void Kill() { Log("LogicMgr Kill"); }
    void AddLogic(LobbyLogic* l) { Log("LogicMgr AddLogic " + Id(l)); }
};
// the real CRC16 (ee_won's), untraced: the product ids are computed in Rust since R6 WON lobby/net round 40
struct CRC16 {
    unsigned short mRegister = 0;
    void Put(const void* d, unsigned int n) { this->mRegister = ee_won_crc16_put(this->mRegister, static_cast<const uint8_t*>(d), n); }
    unsigned short Get() const { return this->mRegister; }
    void Reset() { this->mRegister = 0; }
};
struct ChatCommandParser : Tracked {
    ChatCommandParser() { Log("new ChatCommandParser " + Id(this)); }
    void InitCommands() { Log("ChatCommandParser InitCommands"); }
};
struct UDPManager : Tracked {
    UDPManager() { Log("new UDPManager " + Id(this)); }
    void Close() { Log("UDPManager Close " + Id(this)); }
};
struct ServerContext : Tracked {
    ServerContext() { Log("new ServerContext " + Id(this)); }
};
struct CDKey {
    void SetProductString(const std::string& s) { Log("CDKey SetProductString " + s); }
    void LoadFromRegistry() { Log("CDKey LoadFromRegistry"); }
};
struct AuthContext : Tracked {
    AuthContext() { Log("new AuthContext " + Id(this)); }
    void Kill() { Log("AuthContext Kill " + Id(this)); }
    void SetCommunity(const std::wstring& w) { Log("AuthContext SetCommunity " + Narrow(w)); }
    void SetServerContext(ServerContext* s) { Log("AuthContext SetServerContext " + Id(s)); }
    void SetCDKey(const std::wstring& w, const CDKey&) { Log("AuthContext SetCDKey " + Narrow(w)); }
};
struct LobbyPersistentData : Tracked {
    LobbyPersistentData() { Log("new LobbyPersistentData " + Id(this)); }
};
struct StringParser {
    std::string mText;
    size_t mAt = 0;
    explicit StringParser(const char* s) : mText(s) {}
    // a word (or the rest of the line), appended
    bool ReadString(std::string& out, bool inWord)
    {
        while (this->mAt < this->mText.size() && this->mText[this->mAt] == ' ') this->mAt++;
        size_t e = inWord ? this->mText.find(' ', this->mAt) : std::string::npos;
        if (e == std::string::npos) e = this->mText.size();
        out += this->mText.substr(this->mAt, e - this->mAt);
        Log("StringParser ReadString " + N(inWord) + " '" + out + "'");
        this->mAt = e;
        return true;
    }
    bool ReadValue(std::list<std::string>& l)
    {
        Log("StringParser ReadValue " + this->mText);
        size_t at = 0;
        while (at <= this->mText.size()) {
            size_t c = this->mText.find(',', at);
            if (c == std::string::npos) c = this->mText.size();
            l.push_back(this->mText.substr(at, c - at));
            at = c + 1;
        }
        return true;
    }
};
enum WONStatus { WS_None = 100004, WS_Success = 0 }; // WS_None is the game's (lobbyprv.rs's constructor value)
enum LobbyGameType : int {};
enum LobbyStatus : int {};
enum LobbyRunMode : int {};

// ---- the lobby model's world

struct ByteBuffer : Tracked {
    std::string mBytes;
    explicit ByteBuffer(const std::string& b) : mBytes(b) {}
    const char* data() const { return this->mBytes.data(); }
    unsigned long length() const { return (unsigned long)this->mBytes.size(); }
};
struct ReadBuffer {
    std::string mBytes;
    long mPos = 0;
    ReadBuffer(const void* d, unsigned long n) : mBytes(static_cast<const char*>(d), n) {}
    const char* data() const { return this->mBytes.data(); }
    unsigned long length() const { return (unsigned long)this->mBytes.size(); }
    long pos() const { return this->mPos; }
    void SetPos(int p) { Log("read SetPos " + N(p)); this->mPos = p; }
    void Need(long n)
    {
        if (this->mPos + n > (long)this->mBytes.size()) {
            Log("read past the end at " + N(this->mPos));
            throw ReadBufferException();
        }
    }
    unsigned char ReadByte()
    {
        this->Need(1);
        unsigned char v = (unsigned char)this->mBytes[this->mPos++];
        Log("read byte " + N(v));
        return v;
    }
    short ReadShort()
    {
        this->Need(2);
        short v = (short)((unsigned char)this->mBytes[this->mPos] | (unsigned char)this->mBytes[this->mPos + 1] << 8);
        this->mPos += 2;
        Log("read short " + N(v));
        return v;
    }
    bool ReadBool()
    {
        this->Need(1);
        bool v = this->mBytes[this->mPos++] != 0;
        Log("read bool " + N(v));
        return v;
    }
};
struct WriteBuffer {
    std::string mBytes;
    WriteBuffer(int, bool) {}
    void AppendBytes(const void* d, unsigned long n)
    {
        Log("write bytes " + Hex(d, n));
        this->mBytes.append(static_cast<const char*>(d), n);
    }
    void AppendByte(unsigned char v) { Log("write byte " + N(v)); this->mBytes.push_back((char)v); }
    void AppendShort(short v)
    {
        Log("write short " + N(v));
        this->mBytes.push_back((char)(v & 0xff));
        this->mBytes.push_back((char)((v >> 8) & 0xff));
    }
    void SetShort(unsigned long p, short v)
    {
        Log("write set short " + N((long long)p) + " " + N(v));
        if (p + 2 <= this->mBytes.size()) {
            this->mBytes[p] = (char)(v & 0xff);
            this->mBytes[p + 1] = (char)((v >> 8) & 0xff);
        }
    }
    ConstSmartPtr<ByteBuffer> ToByteBuffer(bool b)
    {
        ByteBuffer* aBuffer = new ByteBuffer(this->mBytes);
        Log("ToByteBuffer " + N(b) + " " + Id(aBuffer) + " " + Hex(this->mBytes.data(), this->mBytes.size()));
        return aBuffer;
    }
};
struct IPAddr {
    char mSix[6];
    long mHost;   // a chat link's address (FromValue's)
    unsigned short mPort;
    const char* GetSixByte() const { return this->mSix; }
    static IPAddr FromValue(EeWonIpAddr* v)
    {
        IPAddr a = {{0}, ee_won_ipaddr_host(v), ee_won_ipaddr_port(v)};
        ee_won_ipaddr_free(v);
        Log("IPAddr FromValue " + N(a.mHost) + ":" + N(a.mPort));
        return a;
    }
};
enum class LobbyMC { GameStoreSummary };
enum class LobbyMF { GameHaveDetails, GameNumPlayers };
typedef unsigned long DWORD;
static int sRegistry;     // QueryDWORD's answer (0 found)
static DWORD sRegistryValue;
static bool gLobbyUseLocalIP;
struct RegKey {
    RegKey(int r, const std::string& p, bool a, bool b) { Log("RegKey " + N(r) + " " + p + " " + N(a) + N(b)); }
    ~RegKey() { Log("~RegKey"); }
    int QueryDWORD(const std::string& n, DWORD& v)
    {
        Log("RegKey QueryDWORD " + n + " " + N((long long)v));
        if (sRegistry == 0) v = sRegistryValue;
        return sRegistry;
    }
};
static int sBaseValue; // ConfigObject::HandleValueInstruction: 0 false, 1 true, 2 a WorldThrow
struct ConfigObject : Tracked {
    template <class T, class D> void KeyAddOf(const char* k, T& v, D d, const std::string& kind)
    {
        const void* aAt = &v;
        std::string aWhere = aAt == &gLobbyUseLocalIP ? std::string("gLobbyUseLocalIP") : N((long long)((char*)&v - (char*)this));
        Log("KeyAdd " + std::string(k) + " " + kind + " at " + aWhere + " default " + d);
    }
    void KeyAdd(const char* k, unsigned short& v, unsigned short d) { this->KeyAddOf(k, v, N(d), "ushort"); }
    void KeyAdd(const char* k, bool& v, bool d) { this->KeyAddOf(k, v, N(d), "bool"); }
    void KeyAdd(const char* k, std::string& v, const char* d) { this->KeyAddOf(k, v, d ? std::string(d) : "null", "string"); }
    void KeyAdd(const char* k, std::wstring& v, const wchar_t* d) { this->KeyAddOf(k, v, d ? std::string("w") : "null", "wstring"); }
    void KeyAdd(const char* k, std::list<std::string>& v, const char* d, bool f) { this->KeyAddOf(k, v, (d ? std::string(d) : "null") + " " + N(f), "list"); }
    virtual bool HandleValueInstruction(ConfigParser* p, const std::string& k, StringParser& v)
    {
        Log("ConfigObject HandleValueInstruction parser " + N(p->mId) + " " + k + " at " + N((long long)v.mAt));
        if (sBaseValue == 2) throw WorldThrow{6};
        return sBaseValue != 0;
    }
};
struct LobbyConfig : ConfigObject {
    // the constructor's
    std::list<std::string> mDirServers, mRoomDirServers, mAuthServers;
    std::wstring mProductDirectory;
    std::string mLanguage;
    bool mCRCCheck = false, mPublic = false, mUseLocalIP = true, mNoPingInProgress = false, mCheckForInternet = false;
    bool mAppendNumberToDefaultLanName = false, mAllowJoinGameWithoutPing = false;
    unsigned short mLanBroadcastPort = 0, mPingCeiling = 0;
    // WONLobbyPrv's
    unsigned short mLobbyPort = 2300;
    std::string mResourceDirectory = "Lobby", mProductName = "EmpireEarth", mVersion = "2.0", mLanVersion = "2.0l";
    std::map<std::string, std::string, StringLessNoCase> mDefines;
    bool mCDKeyCheck = false;
    static std::string mDefaultFileName;
    static LobbyConfig* (*mAllocator)();
    bool ParseTopLevel(ConfigParser& p, const char* f)
    {
        Log("config ParseTopLevel parser " + N(p.mId) + " " + (f ? f : "null"));
        if (sParse == 2) throw WorldThrow{2};
        return sParse != 0;
    }
    // the model's
    bool mAllowDuplicateNames = true;
};
std::string LobbyConfig::mDefaultFileName("WONLobby.cfg");
static LobbyConfig* sAllocatedConfig;
static LobbyConfig* AllocateConfig() { Log("LobbyConfig mAllocator"); return sAllocatedConfig; }
LobbyConfig* (*LobbyConfig::mAllocator)() = AllocateConfig;
static LobbyConfig* sConfig;
static LobbyConfig* GetLobbyConfig() { return sConfig; }
static unsigned short GetLobbyProductId() { return 0x4242; }

static int sSide; // 0 the original's bodies, 1 the Rust's
struct LobbyPlayer : Tracked {
    unsigned short mPing = 0;
    bool mReady = false;
    int mReadData = 1;  // 0 false, 1 reads a byte, true
    static LobbyPlayer* CreatePlayer()
    {
        LobbyPlayer* p = new LobbyPlayer;
        Log("CreatePlayer " + Id(p));
        return p;
    }
    void SetPing(unsigned short v) { Log(Id(this) + " SetPing " + N(v)); this->mPing = v; }
    void SetReady(bool v) { Log(Id(this) + " SetReady " + N(v)); this->mReady = v; }
    unsigned short GetPing() const { return this->mPing; }
    bool GetReady() const { return this->mReady; }
    bool ReadData(ReadBuffer& r)
    {
        Log(Id(this) + " ReadData");
        if (this->mReadData == 0) return false;
        r.ReadByte();
        return true;
    }
    void WriteData(WriteBuffer& w) { Log(Id(this) + " WriteData"); w.AppendByte((unsigned char)(0x30 + this->mId)); }
    bool ReadFromBuffer(ReadBuffer& inBuffer);
    void WriteToBuffer(WriteBuffer& outBuffer);
};
struct LobbyClient : Tracked {
    GUIString mName;
    unsigned short mClientId = 0;
    SmartPtr<LobbyPlayer> mPlayer;
    const GUIString& GetName() const { return this->mName; }
    void SetPlayer(LobbyPlayer* p) { Log(Id(this) + " SetPlayer " + Id(p)); this->mPlayer = p; }
    unsigned short GetClientId() const { return this->mClientId; }
};
struct LobbyClientList : Tracked {
    std::vector<LobbyClient*> mClients;
    size_t GetCount() const { return this->mClients.size(); }
    LobbyClient* ClientAt(size_t i) const { return this->mClients[i]; }
};
struct LobbyClientSetIterator {
    SmartPtr<LobbyClientList> mList;
    size_t mAt = 0;
    explicit LobbyClientSetIterator(LobbyClientList* l) : mList(l) {}
    bool HasMore() const { return this->mAt < this->mList->mClients.size(); }
    LobbyPlayer* Next(LobbyClient** outClient)
    {
        LobbyClient* c = this->mList->mClients[this->mAt++];
        *outClient = c;
        return c->mPlayer.get();
    }
};
struct LobbyGame : Tracked {
    int mType = 1;
    IPAddr mAddress = {{1, 2, 3, 4, 5, 6}};
    bool mInProgress = false, mInviteOnly = false, mAskToJoin = false, mFull = false;
    GUIString mPassword, mName;
    int mSkill = 2, mPlayers = 1, mMaxPlayers = 8;
    unsigned short mPing = 77;
    LobbyClientList* mClients = NULL;
    int mHookRead = 0;     // the summary and details hooks: 0 false, 1 reads a byte and true, 2 true
    short mJoinHook = 0;   // the join request hook's answer (after it reads a byte when mJoinHookReads)
    bool mJoinHookReads = false, mReplyHookReads = false;
    GUIString mStatusHook; // GetStatusStringHook's text
    LobbyGameType GetGameType() const { return (LobbyGameType)this->mType; }
    IPAddr GetAddress() const { return this->mAddress; }
    bool IsInProgress() const { return this->mInProgress; }
    const GUIString& GetPassword() const { return this->mPassword; }
    bool GetInviteOnly() const { return this->mInviteOnly; }
    bool GetAskToJoin() const { return this->mAskToJoin; }
    int GetSkill() const { return this->mSkill; }
    int GetNumPlayers() const { return this->mPlayers; }
    int GetMaxPlayers() const { return this->mMaxPlayers; }
    const GUIString& GetName() const { return this->mName; }
    unsigned short GetPing() const { return this->mPing; }
    bool IsFull() const { return this->mFull; }
    SmartPtr<LobbyClientList> GetClients() const { return SmartPtr<LobbyClientList>(this->mClients); }
    void WriteSummaryHook(WriteBuffer& w) { Log("WriteSummaryHook"); w.AppendByte(0x77); }
    intptr_t M(LobbyMC, intptr_t a, intptr_t b)
    {
        const EeWonGameSummary& s = *reinterpret_cast<const EeWonGameSummary*>(a);
        Log("StoreSummary type " + N(s.game_type) + " addr " + Hex(s.address, 6) + " pid " + N(s.product_id) + " flags " +
            N(s.in_progress) + N(s.has_password) + N(s.invite_only) + N(s.ask_to_join) + " skill " + N(s.skill) +
            " players " + N(s.players) + "/" + N(s.open_slots) + " ok " + N(s.ok) + " read " + N(s.read) + " rest " +
            N((long long)s.rest) + " name " + TextOf(b));
        return 0;
    }
    void MSet(LobbyMF f, long v) { Log("MSet " + N((int)f) + " " + N(v)); }
    bool Hook(ReadBuffer& r, const char* n)
    {
        Log(n);
        if (this->mHookRead == 0) return false;
        if (this->mHookRead == 1) r.ReadByte();
        return true;
    }
    bool ReadSummaryHook(ReadBuffer& r) { return this->Hook(r, "ReadSummaryHook"); }
    bool ReadDetailsHook(ReadBuffer& r) { return this->Hook(r, "ReadDetailsHook"); }
    void GetJoinGameRequestHook(WriteBuffer& w) { Log("GetJoinGameRequestHook"); w.AppendByte(0x42); }
    short HandleJoinGameRequestHook(ReadBuffer& r, LobbyClient* c)
    {
        Log("HandleJoinGameRequestHook " + Id(c));
        if (this->mJoinHookReads) r.ReadByte();
        return this->mJoinHook;
    }
    void GetJoinGameReplyHook(LobbyClient* c, WriteBuffer& w) { Log("GetJoinGameReplyHook " + Id(c)); w.AppendByte(0x24); }
    void HandleJoinGameReplyHook(ReadBuffer& r)
    {
        Log("HandleJoinGameReplyHook");
        if (this->mReplyHookReads) r.ReadByte();
    }
    LobbyClient* FindClient(unsigned short id)
    {
        Log("FindClient " + N(id));
        for (LobbyClient* c : this->mClients->mClients)
            if (c->mClientId == id) return c;
        return NULL;
    }
    GUIString GetStatusStringHook(short s) { Log("GetStatusStringHook " + N(s)); return this->mStatusHook; }
    short HandleJoinRequest(ReadBuffer& inBuffer, LobbyClient* inClient, WriteBuffer& outBuffer);
};
struct WONLobbyPrv {
    SmartPtr<LobbyPersistentData> mPersistentData;
    SmartPtr<LobbyConfig> mConfig;
    SmartPtr<LobbyContainer> mLobbyContainer;
    GUIString mInitError = GUIString("unset");
    SmartPtr<AccountLogic> mAccountLogic;
    SmartPtr<DialogLogic> mDialogLogic;
    SmartPtr<InitLogic> mInitLogic;
    SmartPtr<RoutingLogic> mRoutingLogic;
    SmartPtr<StagingLogic> mStagingLogic;
    SmartPtr<LanLogic> mLanLogic;
    SmartPtr<PingLogic> mPingLogic;
    LobbyLogicMgr mLogicMgr;
    SmartPtr<UDPManager> mUDPManager;
    LobbyStatus mLobbyStatus = (LobbyStatus)55;
    LobbyRunMode mRunMode = (LobbyRunMode)55;
    bool mSuspended = true;
    SmartPtr<ServerContext> mAccountServers, mAuthServers, mDirServers, mRoomDirServers, mFirewallDetectServers, mPatchServers;
    WONStatus mVersionStatus = WS_Success;
    SmartPtr<AuthContext> mAuthContext;
    CDKey mCDKey;
    std::wstring mLoginCommunity = L"unset";
    bool mHasSetHashFileList = true;
    std::list<std::string> mHashFileList = {"old"};
    unsigned short mProductId = 55, mLanProductId = 55, mLobbyPort = 55;
    time_t mSysTOUTime = 55, mGameTOUTime = 55;
    bool mBehindFirewall = true, mFirewallTestDone = true;
    std::string mSysTOUPath = "unset", mGameTOUPath = "unset";
    SmartPtr<ChatCommandParser> mChatCommandParser;
    LobbyGameType mCurGameType = (LobbyGameType)55;
    void Dump()
    {
        std::string h;
        for (const std::string& s : this->mHashFileList) h += s + ";";
        Log("prv error " + Text(this->mInitError) + " config " + Id(this->mConfig.get()) + " container " +
            Id(this->mLobbyContainer.get()) + " persistent " + Id(this->mPersistentData.get()) + " logics " +
            Id(this->mAccountLogic.get()) + Id(this->mDialogLogic.get()) + Id(this->mInitLogic.get()) +
            Id(this->mRoutingLogic.get()) + Id(this->mStagingLogic.get()) + Id(this->mLanLogic.get()) +
            Id(this->mPingLogic.get()) + " udp " + Id(this->mUDPManager.get()) + " auth " + Id(this->mAuthContext.get()) +
            " servers " + Id(this->mAccountServers.get()) + Id(this->mAuthServers.get()) + Id(this->mDirServers.get()) +
            Id(this->mRoomDirServers.get()) + Id(this->mFirewallDetectServers.get()) + Id(this->mPatchServers.get()) +
            " parser " + Id(this->mChatCommandParser.get()));
        Log("prv status " + N(this->mLobbyStatus) + " mode " + N(this->mRunMode) + " suspended " + N(this->mSuspended) +
            " version " + N(this->mVersionStatus) + " community " + Narrow(this->mLoginCommunity) + " hashset " +
            N(this->mHasSetHashFileList) + " hashes " + h + " ids " + N(this->mProductId) + " " + N(this->mLanProductId) +
            " port " + N(this->mLobbyPort) + " tou " + N((long long)this->mSysTOUTime) + " " + N((long long)this->mGameTOUTime) +
            " firewall " + N(this->mBehindFirewall) + N(this->mFirewallTestDone) + " paths " + this->mSysTOUPath + " " +
            this->mGameTOUPath + " type " + N(this->mCurGameType));
    }
};

// ---- ChatCtrl's links

struct LobbyFriend : Tracked {
    GUIString mName;
    const GUIString& GetName() const { return this->mName; }
};
struct LobbyServer : Tracked {
    IPAddr mAddress = {{0}, 0x0a000001, 15101};
    IPAddr GetAddress() const { Log("server GetAddress " + Id(this)); return this->mAddress; }
};
// LobbyNetCtrlHost.cpp's
static GUIString Txt(intptr_t inData) { return inData != 0 ? GUIString((GUIStringData*)inData) : GUIString(); }
struct ChatLink : Tracked {
    GUIString mText = GUIString("unset");
    int mColor = 55;
    SmartPtr<Font> mFont;
    int mKind = 55;
    IPAddr mAddress = {{0}, 55, 55};
    unsigned short mGroupId = 55;
    SmartPtr<LobbyClient> mClient;
    SmartPtr<LobbyFriend> mFriend;
    bool mEnabled = false;
    ChatLink* mPrevious = this;
    void Dump()
    {
        Log("link kind " + N(this->mKind) + " text " + Text(this->mText) + " color " + N(this->mColor) + " font " +
            Id(this->mFont.get()) + " address " + N(this->mAddress.mHost) + ":" + N(this->mAddress.mPort) + " group " +
            N(this->mGroupId) + " client " + Id(this->mClient.get()) + " friend " + Id(this->mFriend.get()) +
            " enabled " + N(this->mEnabled) + " previous " + (this->mPrevious == NULL ? "null" : "set"));
    }
};
// ---- the lobby controls' config readers

SmartPtr<Font> ComponentConfig::ReadFont() { this->Next(0, "font"); Font* f = new Font; Log("ReadFont " + Id(f)); return f; }
static intptr_t Held(const GUIString& inText)
{
    inText.mData->AddRef();
    return (intptr_t)inText.mData.get();
}
static intptr_t Dat(const GUIString& inText) { return (intptr_t)inText.mData.get(); }
struct ChatCtrl : Container {
    void SetField(int f, intptr_t v) { Log("ChatCtrl SetField " + N(f) + " " + N((long long)v)); }
};
struct RoomLabelCtrl : Container {
    void SetField(int f, intptr_t v) { Log("RoomLabel SetField " + N(f) + " " + (f == 0 ? TextOf(v) : N((long long)v))); }
};
struct MultiListArea : Component {
    void SetNumColumns(int n) { Log("list SetNumColumns " + N(n)); }
    void SetDynamicSizeColumn(int c) { Log("list SetDynamicSizeColumn " + N(c)); }
    void SetColumnWidth(int c, int w, bool b) { Log("list SetColumnWidth " + N(c) + " " + N(w) + " " + N(b)); }
};
struct MultiListHeader : Component {
    void SetColumnItem(int c, Component* x) { Log("header SetColumnItem " + N(c) + " " + Id(x)); }
};
struct ComboBox : Component {};
template <class T> static T SafeConfigGetComponent(Component* c, const char* n)
{
    T t = dynamic_cast<T>(c);
    Log(std::string("SafeConfigGetComponent ") + n + " " + Id(c));
    if (t == NULL) throw ConfigObjectException(std::string("not a ") + n);
    return t;
}
struct ServerListCtrl : Container {
    intptr_t mList = 0;
    intptr_t Cfg(EeWonLobbyCtlCfg c, intptr_t a = 0, intptr_t b = 0)
    {
        if (c == EeWonLobbyCtlCfg::AddColumn) {
            std::string n((const char*)a, (size_t)b);
            Log("Cfg AddColumn " + n);
            return n == "Name" ? 3 : n == "Skip" ? 0 : -1;
        }
        bool aPointers = c == EeWonLobbyCtlCfg::SetList || c == EeWonLobbyCtlCfg::PushColumn;
        Log("Cfg " + N((int)c) + " " + (c == EeWonLobbyCtlCfg::SetList ? Id((Component*)a) : N((long long)a)) + " " +
            (aPointers ? Id((Component*)b) : N((long long)b)));
        if (c == EeWonLobbyCtlCfg::SetList) this->mList = a;
        if (c == EeWonLobbyCtlCfg::List) return this->mList;
        if (c == EeWonLobbyCtlCfg::ColumnCount) return 5;
        return 0;
    }
};
static void LobbyAddGameOption(const GUIString& c, const GUIString& v, bool b) { Log("LobbyAddGameOption " + Text(c) + " " + Text(v) + " " + N(b)); }
enum URLType { URLType_Http, URLType_File };
struct URL {
    std::string mText;
    URL(const std::string& s) : mText(s) {}
    URLType GetType() const { return this->mText.compare(0, 5, "file:") == 0 ? URLType_File : URLType_Http; }
};
struct GameBrowserCtrl : Container {
    void SetHomeLocation(const GUIString& s) { Log("SetHomeLocation " + Text(s)); }
};
struct CrossPromotionButton : Component {
    void SetDefaultImage(Image* i) { Log("SetDefaultImage " + Id(i)); }
    void SetDefaultBrowse(const GUIString& s) { Log("SetDefaultBrowse " + Text(s)); }
};
struct ImageTextButton : Component {
    SmartPtr<Font> mFont;
    GUIString mText, mBrowse;
};
struct LobbyRustDialog : Container {
    intptr_t LCall(EeWonLobbyDlgCall c, intptr_t a, intptr_t b, intptr_t d)
    {
        Log("LCall " + N((int)c) + " " + Text(*reinterpret_cast<const GUIString*>(a)) + " " + Id((Component*)b) + " " + N((long long)d));
        return 0;
    }
};
struct ChatCtrlConfig : ContainerConfig {
    ChatCtrl* mChatCtrl;
};
struct RoomLabelCtrlConfig : ContainerConfig {
    RoomLabelCtrl* mRoomLabel;
};
struct UserListCtrlConfig : ContainerConfig {
    static Component* CompFactory() { return NULL; }
};
struct ServerListCtrlConfig : ContainerConfig {
    ServerListCtrl* mServerList;
    MultiListHeader* mHeader = NULL;
    int mNumColumns = 0;
    void HandleAddColumn();
};
struct GameBrowserCtrlConfig : ContainerConfig {
    GameBrowserCtrl* mBrowser;
};
struct GameUserListCtrlConfig : ContainerConfig {
    static Component* CompFactory() { return NULL; }
};
struct CrossPromotionButtonConfig : ComponentConfig {
    CrossPromotionButton* mCrossPromotionButton;
};
struct ImageTextButtonConfig : ComponentConfig {
    ImageTextButton* mImageTextButton;
};
struct CreditsDialogConfig : ContainerConfig {
    LobbyRustDialog* mCredits;
};
}
#endif
