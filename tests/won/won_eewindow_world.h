// won_eewindow_test's stand-in world (R6 WONLobby round 29; the network game functions' added in round 30): the game's and the lobby's globals EELobbyGlueHost.cpp's
// window functions name (EELobbyOpen, EELobbyShow, EELobbyHide, EELobbyClose, EELobbyShowGameScreen), each call
// logged. The deleted functions (won_eewindow_orig.h) and the glue host's new one-statement cases (the test's World)
// run over it; the world's answers are its knobs.
#ifndef WON_EEWINDOW_WORLD_H
#define WON_EEWINDOW_WORLD_H
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace WinWorld {
static std::vector<std::string> sTrace;
static void Log(const std::string& s) { sTrace.push_back(s); }
static std::string N(long long v) { return std::to_string(v); }
static std::string F(float v) { char b[40]; snprintf(b, sizeof b, "%.9g", v); return b; }

struct RefCount {
    int mRefs = 0;
    virtual ~RefCount() {}
    RefCount* CreateRef() { this->mRefs++; return this; }
    void AddRef() { this->mRefs++; }
    void Release() { this->mRefs--; }
};
template <class T> class SmartPtr {
public:
    SmartPtr() : mPtr(NULL) {}
    SmartPtr(T* p) : mPtr(p ? (T*)p->CreateRef() : NULL) {}
    ~SmartPtr() { if (this->mPtr != NULL) this->mPtr->Release(); }
    T* operator=(T* p)
    {
        if (this->mPtr != p) {
            if (this->mPtr != NULL) this->mPtr->Release();
            this->mPtr = p ? (T*)p->CreateRef() : NULL;
        }
        return p;
    }
    T* operator->() const { return this->mPtr; }
    T* get() const { return this->mPtr; }
    T* mPtr;
};

struct GE2DPoint {
    float mX, mY;
    GE2DPoint(float x, float y) : mX(x), mY(y) {}
};
struct WONRectangle {
    int x, y, w, h;
    WONRectangle(int a, int b, int c, int d) : x(a), y(b), w(c), h(d) {}
};
struct Component : RefCount { int mId = 0; };
struct Container : Component {};
struct RootContainer : Container {
    void Fn2F6140(Component* c, int k, Component* r, int, int, int, int, int)
    {
        Log("root layout " + N(c->mId) + " " + N(k) + " " + N(r != NULL));
    }
    void AddChild(Component* c) { Log("root AddChild " + N(c->mId)); }
};
struct Window : RefCount {
    SmartPtr<RootContainer> mRoot;
    Window() { this->mRoot = new RootContainer; }
    void SetStyle(long s) { Log("window SetStyle " + N(s)); }
    void Create(const WONRectangle& r) { Log("window Create " + N(r.x) + " " + N(r.y) + " " + N(r.w) + " " + N(r.h)); }
    void Show(bool b) { Log("window Show " + N(b)); }
    void InvalidateRoot() { Log("window InvalidateRoot"); }
};
struct PlatGUIWindow : Window {
    void SetForwardKeyMsgToParent(bool b) { Log("window ForwardKeys " + N(b)); }
};
static Window* CreateNativeWindow() { Log("CreateNativeWindow"); return new PlatGUIWindow; }
struct ImageDecoder {};
struct SoundDecoder { virtual ~SoundDecoder() {} };
struct EELobbySoundDecoder : SoundDecoder {};
struct PlatInputEvent {};
typedef void (*EventForward)(const PlatInputEvent&);
struct PlatWindowManager : RefCount {
    ImageDecoder mImageDecoder;
    void SetPopupsInOwnWindow(bool b) { Log("root SetPopupsInOwnWindow " + N(b)); }
    void SetSoundDecoder(SoundDecoder* d) { Log("root SetSoundDecoder " + N(d != NULL)); delete d; }
    void SetSurfaceSize(int w, int h) { Log("root SetSurfaceSize " + N(w) + " " + N(h)); }
    void SetEventForward(EventForward f) { Log("root SetEventForward " + N(f != NULL)); }
    void CloseAllWindows() { Log("root CloseAllWindows"); }
};
struct JPGDecoder { static void Add(ImageDecoder*) { Log("JPGDecoder Add"); } };
struct PNGDecoder { static void Add(ImageDecoder*) { Log("PNGDecoder Add"); } };
struct WONAPICoreEx {
    void SetDoPumpThread(bool b) { Log("core SetDoPumpThread " + N(b)); }
    void Startup() { Log("core Startup"); }
    void Shutdown() { Log("core Shutdown"); }
    ~WONAPICoreEx() { Log("core deleted"); }
};
struct GESurface {
    float mWidth, mHeight;
    float GetWidth() { return this->mWidth; }
    float GetHeight() { return this->mHeight; }
};
struct Rasterizer {
    GESurface* mSecondary = NULL;
    GESurface* GetSecondarySurface(int i) { Log("GetSecondarySurface " + N(i)); return this->mSecondary; }
};
static Rasterizer sRasterizer;
static Rasterizer* gRasterizer = &sRasterizer;
static int gScreenIndex = 1;
// ---- the network game functions' world (R6 WONLobby round 30: EELobbyHostGame, EELobbyJoinGame,
// EELobbyStartHostedGame, EELobbyRunSierraUpdate, EEGetLocalAddress)
struct UString : std::string {
    UString() {}
    UString(const char* s) : std::string(s) {}
    UString(const std::string& s) : std::string(s) {}
    operator const char*() const { return this->c_str(); }
};
static UString operator+(const UString& a, const UString& b) { return UString(std::string(a) + std::string(b)); }
struct UWideString : std::string {
    UWideString() {}
    UWideString(const char* s) : std::string(s) {}
    UWideString(const std::string& s) : std::string(s) {}
};
static const int keseOK = 0;
static char gEmptyString[] = "";
static UWideString gPlayerName;
static UWideString gGameVersion = "1.0.2";
static unsigned long gBinaryChecksum = 0x1234abcd;
// a value of a message's lists, logged
struct Val {
    std::string mText;
    Val(bool v) : mText("b" + N(v)) {}
    Val(int v) : mText("i" + N(v)) {}
    Val(unsigned char v) : mText("u8:" + N(v)) {}
    Val(unsigned long v) : mText("ul" + N((long long)v)) {}
    Val(const UWideString& v) : mText("w'" + v + "'") {}
};
static std::string Vals(std::initializer_list<Val> l)
{
    std::string s = "{";
    for (const Val& v : l) s += v.mText + ",";
    return s + "}";
}
static const int kNetMessageClientConnectionRequest = 7;
static const int kMessageStartLoading = 42;
static int sMessageIds;
struct NEMessage {
    int mId;
    virtual ~NEMessage() {}
};
struct NEMRecord : NEMessage {
    NEMRecord(int k, std::initializer_list<Val> a, std::initializer_list<Val> b)
    {
        this->mId = ++sMessageIds;
        Log("new NEMRecord " + N(k) + " " + Vals(a) + " " + Vals(b));
    }
};
struct EEMRecord : NEMessage {
    EEMRecord(int k, std::initializer_list<Val> a) { this->mId = 0; Log("new EEMRecord " + N(k) + " " + Vals(a)); }
};
struct NECommunications {
    bool mServer = false;
    virtual ~NECommunications() { Log(std::string("delete communications ") + (this->mServer ? "server" : "client")); }
    bool IsServer() { Log("IsServer"); return this->mServer; }
    void ProcessLocalUserInput(NEMessage* m, int to) { Log("ProcessLocalUserInput " + N(m->mId) + " " + N(to)); delete m; }
    void ProcessGuaranteedLocalUserInput(NEMessage* m, int to) { Log("ProcessGuaranteed " + N(to)); delete m; }
};
static bool sPublished, sConnectOk;
struct EEServer : NECommunications {
    bool mNumLoadResponses = true, mLoadState = false;
    EEServer(const UWideString& n, unsigned long ad, int a, int b, bool c, bool d)
    {
        this->mServer = true;
        Log("new EEServer " + n + " " + N((long long)ad) + " " + N(a) + N(b) + N(c) + N(d));
    }
    void StartThreads() { Log("server StartThreads"); }
    bool GetGameWasPublished() { Log("GetGameWasPublished"); return sPublished; }
    void AllowNewPlayers(bool b) { Log("AllowNewPlayers " + N(b)); }
    void SetGameState(int s) { Log("SetGameState " + N(s)); }
    void StopPing() { Log("StopPing"); }
    void ResetXfer() { Log("ResetXfer"); }
    ~EEServer() { Log("server state " + N(this->mNumLoadResponses) + " " + N(this->mLoadState)); }
};
struct NESynchronousClient : NECommunications {
    void StartThreads() { Log("client StartThreads"); }
    int Connect(const UString& a, const UString& s, const UWideString& g)
    {
        Log("Connect '" + std::string(a) + "' '" + std::string(s) + "' '" + g + "'");
        return sConnectOk ? 0 : -1;
    }
    void SendStartConnectMessage(NEMessage* m) { Log("SendStartConnectMessage " + N(m->mId)); }
};
struct EEClient : NESynchronousClient {
    EEClient() { Log("new EEClient"); }
};
static NECommunications* gCommunications;
struct EEGameOptionsPlayer {
    int mType = 0;
    bool mIsReady = false;
    UWideString mName;
    EEGameOptionsPlayer() {}
};
struct Lock {
    void ReadLock() { Log("ReadLock"); }
    void ReadUnlock() { Log("ReadUnlock"); }
    void WriteLock() { Log("WriteLock"); }
    void WriteUnlock() { Log("WriteUnlock"); }
};
struct EEGameOptions {
    Lock* mLock = NULL;
    bool mIsMultiplayer = false, mCheatCodes = true;
    int mId = 0;
    EEGameOptionsPlayer mPlayers[16];
    EEGameOptions& operator=(const EEGameOptions& o)
    {
        Log("options = " + N(o.mId));
        this->mId = o.mId;
        for (int i = 0; i < 16; i++) this->mPlayers[i] = o.mPlayers[i];
        return *this;
    }
};
static Lock sOptionsLock;
static EEGameOptions gGameOptions;
static EEGameOptions gLobbyGameOptions;
struct GameWorld {
    EEGameOptions mLobby;
    EEGameOptions& GetLobbyOptions() { return this->mLobby; }
};
static GameWorld gWorld;
static unsigned long sTicks;
static unsigned long TMGetTime() { Log("TMGetTime"); return sTicks += 17; }
static unsigned sRandState;
static void WSrand(unsigned s) { Log("srand " + N(s)); sRandState = s; }
static int WRand() { sRandState = sRandState * 214013u + 2531011u; return (int)((sRandState >> 16) & 0x7fff); }
struct LobbyClient {
    UWideString mName;
    const UWideString& GetName() { Log("client GetName"); return this->mName; }
};
static LobbyClient sMyClient;
static LobbyClient* GetMyLobbyClient() { Log("GetMyLobbyClient"); return &sMyClient; }
static UWideString EELobbyToWideString(const UWideString& s) { return s; }
static bool sBehindFirewall;
static bool LobbyIsBehindFirewall() { Log("LobbyIsBehindFirewall"); return sBehindFirewall; }
struct IPAddr {
    std::string mHost;
    std::string GetHostString(bool b) { Log("GetHostString " + N(b)); return this->mHost; }
};
struct LobbyGame {
    IPAddr mAddress;
    IPAddr& GetAddress() { Log("game GetAddress"); return this->mAddress; }
};
static LobbyGame sMyGame;
static LobbyGame* sMyGamePtr;
static LobbyGame* GetMyLobbyGame() { Log("GetMyLobbyGame"); return sMyGamePtr; }
static bool sMultiplayer;
static bool EEIsMultiplayerGame(bool b) { Log("EEIsMultiplayerGame " + N(b)); return sMultiplayer; }
struct NEProtocol {
    UWideString mAddress;
    UWideString& GetLocalAddress(unsigned long a) { Log("protocol GetLocalAddress " + N((long long)a)); return this->mAddress; }
};
static UString gStartupVolume = "C:";
static UString gStartupDirectory = "\\Game\\";
static bool sOpenOk;
static intptr_t sLaunchResult;
struct FSFileSpec {
    UString mVolume, mDirectory, mFile;
    FSFileSpec() { Log("new FSFileSpec"); }
    ~FSFileSpec() { Log("delete FSFileSpec"); }
    void SetVolume(const UString& v) { Log(std::string("SetVolume ") + v.c_str()); this->mVolume = v; }
    void SetDirectory(const UString& v) { Log(std::string("SetDirectory ") + v.c_str()); this->mDirectory = v; }
    void SetFile(const char* v) { Log(std::string("SetFile ") + v); this->mFile = v; }
    const UString& GetVolume() { return this->mVolume; }
    const UString& GetDirectory() { return this->mDirectory; }
    const UString& GetFile() { return this->mFile; }
    long Open(int m) { Log("Open " + N(m)); return sOpenOk ? keseOK : 5; }
    void Close() { Log("Close"); }
};
typedef void* HWND;
typedef void* HINSTANCE;
typedef intptr_t INT_PTR;
static const int SW_SHOWNORMAL = 1;
static HWND GetDesktopWindow() { Log("GetDesktopWindow"); return (HWND)0x10; }
static HINSTANCE ShellExecuteA(HWND w, const char* op, const char* file, void* p, const char* dir, int show)
{
    Log(std::string("ShellExecuteA ") + N(w != NULL) + " " + op + " " + file + " " + N(p != NULL) + " " + dir + " " + N(show));
    return (HINSTANCE)sLaunchResult;
}
struct Thread {
    void Post(int m, void* a, void* b, bool c) { Log("Post " + N(m) + N(a != NULL) + N(b != NULL) + N(c)); }
};
struct UserInterface {
    Thread mThread;
    Thread* GetThread() { return &this->mThread; }
};
static UserInterface sUserInterface;
static UserInterface* gUserInterface;
static const int IDS_UNABLE_TO_LAUNCH_UPDATE_UTILITY = 3011;
struct Resource {
    void GetString(int id, UWideString& s) { Log("GetString " + N(id)); s = "cannot launch"; }
};
static Resource sResource;
static Resource* gResource = &sResource;

struct Settings {
    bool mFullScreen = false;
    float GetMusicVolume() { return 0.75f; }
    unsigned long GetNetworkAdapter() { Log("GetNetworkAdapter"); return 2; }
};
struct Preferences {
    Settings mSettings;
    Settings& GetSettings() { return this->mSettings; }
    void SetLastGameOptions(const EEGameOptions& o) { Log("SetLastGameOptions " + N(o.mId) + N(o.mIsMultiplayer) + N(o.mCheatCodes)); }
};
static Preferences gGamePreferences;
struct PlatWindow { int mId; };
struct GameWindow {
    PlatWindow* mOs = NULL;
    PlatWindow* GetOSRepresentation() { return this->mOs; }
};
static GameWindow sGameWindow;
static GameWindow* gGameWindow = &sGameWindow;
struct Mouse {
    GE2DPoint mPosition{0.0f, 0.0f};
    long mPointer = 0;
    void SetPosition(const GE2DPoint& p) { Log("mouse SetPosition " + F(p.mX) + " " + F(p.mY)); }
};
static Mouse* gMouse;
struct Device {
    std::string mName;
    void Suspend() { Log(this->mName + " Suspend"); }
    void Resume() { Log(this->mName + " Resume"); }
};
static Device* gISKeyboard;
static Device* gISMouse;
static int32_t sClientW, sClientH, sPointerX, sPointerY;
static void PlatTextInputStart(PlatWindow* w, int, int, int, int) { Log("TextInputStart " + N(w != NULL ? w->mId : -1)); }
static void PlatCursorShow(int s) { Log("CursorShow " + N(s)); }
static void PlatCursorSet(void* c) { Log("CursorSet " + N(c != NULL)); }
static void PlatWindowGetClientSize(PlatWindow*, int32_t* w, int32_t* h) { Log("GetClientSize"); *w = sClientW; *h = sClientH; }
static void PlatMouseWarp(PlatWindow*, int32_t x, int32_t y) { Log("MouseWarp " + N(x) + " " + N(y)); }
static void PlatMouseGetState(PlatWindow*, int32_t* x, int32_t* y) { Log("MouseGetState"); *x = sPointerX; *y = sPointerY; }
struct UIForm {
    void SetFormOpenEvent(RefCount* e) { Log("form SetFormOpenEvent " + N(e != NULL)); }
};
static UIForm sForm30;
struct UIManager {
    void RedrawAll() { Log("ui RedrawAll"); }
    void SetMusicVolume(float v) { Log("ui SetMusicVolume " + F(v)); }
    UIForm* GetFormByID(int i) { Log("ui GetFormByID " + N(i)); return &sForm30; }
    void Push(UIForm*) { Log("ui Push"); }
    void DisplayMessage(const UWideString& s, bool a, void* p, int n, bool c) { Log("DisplayMessage " + s + N(a) + N(p != NULL) + N(n) + N(c)); }
};
static UIManager* gUIManager;
struct EELobbyGameDoneEvent : RefCount {
    EELobbyGameDoneEvent() { Log("new GameDoneEvent"); }
    void Dereference() { Log("event Dereference"); }
};
static void EELobbyLock() { Log("lock"); }
static void EELobbyUnlock() { Log("unlock"); }
static void LobbyStart() { Log("LobbyStart"); }
static void LobbyShutdown() { Log("LobbyShutdown"); }
static void EELobbyInit() { Log("EELobbyInit"); }
static Container* sScreen;
static Container* LobbyGetContainer() { Log("LobbyGetContainer"); return sScreen; }
static const int kEELobbySurfaceWidth = 1024;
static const int kEELobbySurfaceHeight = 768;
static void EELobbyForwardEvent(const PlatInputEvent&) {}

// the lobby's window (the glue host's globals)
static SmartPtr<PlatWindowManager> gEELobbyRootWindow;
static SmartPtr<Window> gEELobbyPlatformWindow;
static WONAPICoreEx* gEELobbyWindowManager;
static bool gEELobbyOpen;
static bool gEELobbyShown;
} // namespace WinWorld
// the world's random numbers for the copied statements (the C library's are the driver's)
#define srand WinWorld::WSrand
#define rand WinWorld::WRand
#endif
