// won_eewindow_test's stand-in world (R6 WONLobby round 29): the game's and the lobby's globals EELobbyGlueHost.cpp's
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
struct Settings {
    bool mFullScreen = false;
    float GetMusicVolume() { return 0.75f; }
};
struct Preferences {
    Settings mSettings;
    Settings& GetSettings() { return this->mSettings; }
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
#endif
