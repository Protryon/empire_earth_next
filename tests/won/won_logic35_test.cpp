// The lobby logics' host cases R6 WON lobby and network round 35 composed in Rust (ee_won_lobby logicsplit.rs)
// against the deleted cases as they were at 207fb805f (WONLobby/DialogLogic.cpp's WmPopups, Bounds, NewWindow,
// SetUpPopup, StatusSetStatus and OkCancelSetText), over one stand-in world that logs every call ("WON goldens by
// reach": the popup windows and the Yes/No boxes no LAN game opens, and the status and bounds cases the LAN game
// reaches only in part).
//
// Each case runs on both sides over worlds made alike: the original's statements (Orig::Host), then the Rust over
// the host's new one-statement cases, copied as they are (World). The traces (calls, results, the objects' state and
// references after) must be equal. Then every new op is skipped in turn (the World answers 0 and does nothing):
// each skip must make some case differ. The exit code is the real pass's mismatches plus the skips not caught.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include "rust/won_lobby/dlglogic.h"
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/logicsplit.h"
#include "rust/won_lobby/resource.h"

typedef EeWonDlgLogicOp Op;

// ---- the world

static std::vector<std::string> sTrace;
static void Log(const std::string& s) { sTrace.push_back(s); }
static std::string N(long long v) { return std::to_string(v); }

namespace WONAPI {
struct RefCount {
    int mId = 0;
    int mRefs = 0;
    virtual ~RefCount() {}
    void AddRef() { this->mRefs++; }
    void Release() { this->mRefs--; }
};
template <class T> struct SmartPtr {
    T* mPtr = NULL;
    SmartPtr() {}
    SmartPtr(T* p) : mPtr(p) { if (p) p->AddRef(); }
    SmartPtr(const SmartPtr& o) : mPtr(o.mPtr) { if (mPtr) mPtr->AddRef(); }
    ~SmartPtr() { if (mPtr) mPtr->Release(); }
    SmartPtr& operator=(T* p) { if (p) p->AddRef(); if (mPtr) mPtr->Release(); mPtr = p; return *this; }
    SmartPtr& operator=(const SmartPtr& o) { return *this = o.mPtr; }
    T* get() const { return mPtr; }
    T* operator->() const { return mPtr; }
};
static std::string Id(const RefCount* p) { return p == NULL ? "null" : "#" + N(p->mId) + "/" + N(p->mRefs); }
static std::vector<RefCount*> sMade; // every object a case made (their references are compared after)
template <class T> static T* Make(int inId) { T* o = new T; o->mId = inId; sMade.push_back(o); return o; }

struct GUIStringData : RefCount { std::string mText; };
static GUIStringData sEmpty;
struct GUIString {
    SmartPtr<GUIStringData> mData;
    GUIString() : mData(&sEmpty) {}
    explicit GUIString(GUIStringData* d) : mData(d) {}
};
static std::string Show(const GUIString* t) { return t == NULL ? "null" : "'" + t->mData->mText + "'" + Id(t->mData.get()); }
struct WONRectangle {
    int x, y, width, height;
    WONRectangle(int a, int b, int c, int d) : x(a), y(b), width(c), height(d) {}
};
struct ColorScheme { int mId; };
static ColorScheme sScheme = {5};
static ColorScheme* LobbyGetColorScheme() { Log("LobbyGetColorScheme"); return &sScheme; }
struct Component : RefCount {
    WONRectangle mBounds = WONRectangle(0, 0, 0, 0);
    ColorScheme* mScheme = NULL;
    // (an accessor of a member: not logged, the composition reads it once per field where the original read it once)
    const WONRectangle& GetBounds() { return this->mBounds; }
    void SetColorScheme(ColorScheme* s) { Log("SetColorScheme " + Id(this) + " " + N(s->mId)); this->mScheme = s; }
};
struct Window : RefCount {
    Window* mParent = NULL;
    int mStyle = 0;
    bool mOne = false;
    WONRectangle mRect = WONRectangle(-1, -1, -1, -1);
    SmartPtr<Component> mRoot;
    void SetParent(Window* p) { Log("SetParent " + Id(this) + " " + Id(p)); this->mParent = p; }
    void SetStyle(int s) { Log("SetStyle " + Id(this) + " " + N(s)); this->mStyle = s; }
    void Create(const WONRectangle& r) { Log("Create " + Id(this) + " " + N(r.x) + " " + N(r.y) + " " + N(r.width) + " " + N(r.height)); this->mRect = r; }
    void SetOneComponentWindow(bool b) { Log("SetOneComponentWindow " + Id(this) + " " + N(b)); this->mOne = b; }
};
static int sNextWindow;
static Window* CreateNativeWindow()
{
    Window* w = Make<Window>(100 + sNextWindow++);
    w->mRoot = Make<Component>(w->mId + 50);
    Log("CreateNativeWindow " + Id(w));
    return w;
}
static void LobbyAddLogicListener(Component* c) { Log("LobbyAddLogicListener " + Id(c)); }
struct WindowManager {
    bool mPopups = false;
    bool GetPopupsInOwnWindow() { Log("GetPopupsInOwnWindow " + N(this->mPopups)); return this->mPopups; }
    static WindowManager* sDefault;
    static WindowManager* GetDefaultWindowManager() { Log("GetDefaultWindowManager"); return sDefault; }
};
WindowManager* WindowManager::sDefault;
typedef long WONStatus;
static GUIString GetWONStatusString(WONStatus s)
{
    GUIStringData* d = Make<GUIStringData>(500 + (int)(s & 0xff));
    d->mText = "status" + N(s);
    Log("GetWONStatusString " + N(s));
    return GUIString(d);
}
struct LobbyRustDialog : Component {
    intptr_t LCall(EeWonLobbyDlgCall inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
    {
        if (inCall == EeWonLobbyDlgCall::SetStatus) Log("SetStatus " + Id(this) + " " + Show((const GUIString*)a) + " " + N(b));
        else if (inCall == EeWonLobbyDlgCall::SetText) {
            const GUIString* const* aButtons = (const GUIString* const*)c;
            Log("SetText " + Id(this) + " " + N(a) + " " + N(b) + " " + Show(aButtons[0]) + " " + Show(aButtons[1]));
        } else Log("LCall " + N((int)inCall));
        return 0;
    }
};
struct LobbyResourceSlot { GUIString mString; };
static LobbyResourceSlot gLobbyResources[700];
#define LOBBY_STRING(theName) (gLobbyResources[(int)EeWonLobbyRes::theName].mString)
}
using namespace WONAPI;

// ---- the original's cases (DialogLogic.cpp at 207fb805f)
namespace Orig {
static intptr_t Host(Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    Component* aComp = reinterpret_cast<Component*>(a);
    Window* aWindow = reinterpret_cast<Window*>(a);
    switch (inOp) {
    case Op::WmPopups: {
        if (WONAPI::WindowManager::GetDefaultWindowManager() == NULL)
            return -1;
        return WONAPI::WindowManager::GetDefaultWindowManager()->GetPopupsInOwnWindow() ? 1 : 0;
    }
    case Op::Bounds: {
        const WONRectangle& aBounds = aComp->GetBounds();
        int* anOut = reinterpret_cast<int*>(b);
        anOut[0] = aBounds.x;
        anOut[1] = aBounds.y;
        anOut[2] = aBounds.width;
        anOut[3] = aBounds.height;
        return 0;
    }
    case Op::NewWindow: {
        SmartPtr<Window> aNew(CreateNativeWindow());
        aNew->SetOneComponentWindow(true);
        aNew->AddRef(); // the Rust's reference
        return (intptr_t)aNew.get();
    }
    case Op::SetUpPopup:
        aWindow->SetParent(reinterpret_cast<Window*>(b));
        aWindow->SetStyle(0x22);
        aWindow->Create(WONRectangle(0, 0, 100, 100));
        LobbyAddLogicListener(aWindow->mRoot.get());
        aWindow->mRoot->SetColorScheme(LobbyGetColorScheme());
        return 0;
    case Op::StatusSetStatus: {
        GUIString aStatus = GetWONStatusString((WONStatus)b);
        static_cast<LobbyRustDialog*>(aComp)->LCall(EeWonLobbyDlgCall::SetStatus, (intptr_t)&aStatus, false);
        return 0;
    }
    case Op::OkCancelSetText: {
        const GUIString* aButtons[2] = {&(d != 0 ? LOBBY_STRING(LobbyGlobal_Yes_String) : LOBBY_STRING(LobbyGlobal_OK_String)),
                                        &(d != 0 ? LOBBY_STRING(LobbyGlobal_No_String) : LOBBY_STRING(LobbyGlobal_Cancel_String))};
        static_cast<LobbyRustDialog*>(aComp)->LCall(EeWonLobbyDlgCall::SetText, b, c, (intptr_t)aButtons);
        return 0;
    }
    default: return 0;
    }
}
}

// ---- the new side: DialogLogic.cpp's new one-statement cases (and the old ones they call), as they are
static int sSkip = -1;
static SmartPtr<Window> sDlgNewWindow;
static GUIString sDlgStatusText;
static const GUIString* sDlgButtons[2];
static intptr_t World(void*, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t)
{
    Component* aComp = reinterpret_cast<Component*>(a);
    Window* aWindow = reinterpret_cast<Window*>(a);
    // a skipped maker or getter answers another object (a null one would only crash the next statement)
    if ((int)inOp == sSkip && (inOp == Op::NewWindowMake || inOp == Op::NewWindowGet)) {
        Window* aOther = Make<Window>(900);
        aOther->mRoot = Make<Component>(950);
        if (inOp == Op::NewWindowMake) sDlgNewWindow = aOther;
        return inOp == Op::NewWindowGet ? (intptr_t)aOther : 0;
    }
    if ((int)inOp == sSkip) return 0;
    switch (inOp) {
    case Op::AddRefWindow: aWindow->AddRef(); return 0;
    case Op::WmDefault: return (intptr_t)WONAPI::WindowManager::GetDefaultWindowManager();
    case Op::WmPopupsOf: return reinterpret_cast<WindowManager*>(a)->GetPopupsInOwnWindow() ? 1 : 0;
    case Op::BoundsX: return aComp->GetBounds().x;
    case Op::BoundsY: return aComp->GetBounds().y;
    case Op::BoundsWidth: return aComp->GetBounds().width;
    case Op::BoundsHeight: return aComp->GetBounds().height;
    case Op::NewWindowMake: sDlgNewWindow = CreateNativeWindow(); return 0;
    case Op::NewWindowOneComponent: sDlgNewWindow->SetOneComponentWindow(true); return 0;
    case Op::NewWindowGet: return (intptr_t)sDlgNewWindow.get();
    case Op::NewWindowDrop: sDlgNewWindow = NULL; return 0;
    case Op::WindowSetParent: aWindow->SetParent(reinterpret_cast<Window*>(b)); return 0;
    case Op::WindowSetPopupStyle: aWindow->SetStyle(0x22); return 0;
    case Op::WindowCreatePopup: aWindow->Create(WONRectangle(0, 0, 100, 100)); return 0;
    case Op::WindowListenRoot: LobbyAddLogicListener(aWindow->mRoot.get()); return 0;
    case Op::WindowRootSetScheme: aWindow->mRoot->SetColorScheme(LobbyGetColorScheme()); return 0;
    case Op::StatusTextOf: sDlgStatusText = GetWONStatusString((WONStatus)a); return 0;
    case Op::StatusSetHeldText: static_cast<LobbyRustDialog*>(aComp)->LCall(EeWonLobbyDlgCall::SetStatus, (intptr_t)&sDlgStatusText, false); return 0;
    case Op::StatusTextDrop: sDlgStatusText = GUIString(); return 0;
    case Op::ButtonTextSet: sDlgButtons[a] = &gLobbyResources[b].mString; return 0;
    case Op::OkCancelSetHeldText: static_cast<LobbyRustDialog*>(aComp)->LCall(EeWonLobbyDlgCall::SetText, b, c, (intptr_t)sDlgButtons); return 0;
    default: Log("unexpected op " + N((int)inOp)); return 0;
    }
}

// ---- the cases
static int sChecks, sFailures;
static void Compare(const std::string& inWhat, const std::vector<std::string>& o, const std::vector<std::string>& n)
{
    sChecks++;
    if (o == n) return;
    sFailures++;
    if (sSkip >= 0) return;
    printf("MISMATCH %s\n", inWhat.c_str());
    size_t i = 0;
    while (i < o.size() && i < n.size() && o[i] == n[i]) i++;
    printf("  orig[%zu] %s\n  new [%zu] %s\n", i, i < o.size() ? o[i].c_str() : "(end)", i, i < n.size() ? n[i].c_str() : "(end)");
}

struct Objects {
    WindowManager mManager;
    Window mParent;
    Window* mWindow;
    LobbyRustDialog mDialog;
    int mRect[4] = {-9, -9, -9, -9};
};
// a case: `inMake` on fresh objects for each side, `inRun` with the original host then the Rust over World
template <class Make, class Run> static void Case(const std::string& inWhat, Make inMake, Run inRun)
{
    std::vector<std::string> t[2];
    for (int side = 0; side < 2; side++) {
        // the test's statics start each side empty (a skipped op may have left them set)
        sDlgNewWindow = NULL;
        sDlgStatusText = GUIString();
        sDlgButtons[0] = sDlgButtons[1] = NULL;
        sNextWindow = 0;
        sMade.clear();
        Objects o;
        o.mParent.mId = 1;
        o.mDialog.mId = 2;
        o.mWindow = CreateNativeWindow();
        WindowManager::sDefault = &o.mManager;
        inMake(o);
        sTrace.clear();
        intptr_t r = inRun(o, side == 1);
        Log("result " + (r == (intptr_t)o.mWindow ? std::string("window") : r != 0 && r > 4096 ? Id((RefCount*)r) : N(r)));
        Log("rect " + N(o.mRect[0]) + " " + N(o.mRect[1]) + " " + N(o.mRect[2]) + " " + N(o.mRect[3]));
        for (RefCount* m : sMade) Log("made " + Id(m));
        Window* w = o.mWindow;
        Log("window " + Id(w) + " parent " + Id(w->mParent) + " style " + N(w->mStyle) + " one " + N(w->mOne) + " rect " +
            N(w->mRect.x) + " " + N(w->mRect.width) + " scheme " + N(w->mRoot->mScheme ? w->mRoot->mScheme->mId : 0));
        t[side] = sTrace;
    }
    Compare(inWhat, t[0], t[1]);
}
static intptr_t Call(Objects&, bool inNew, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    if (!inNew) return Orig::Host(inOp, a, b, c, d);
    return ee_won_lobby_logicsplit_dlg_op(World, NULL, inOp, a, b, c, d);
}

static void Run()
{
    // the default window manager: none, popups off, on
    for (int k = 0; k < 3; k++)
        Case("WmPopups " + N(k), [&](Objects& o) { if (k == 0) WindowManager::sDefault = NULL; o.mManager.mPopups = k == 2; },
             [&](Objects& o, bool n) { return Call(o, n, Op::WmPopups, 0, 0, 0, 0); });
    // the bounds of components
    const int kBounds[][4] = {{0, 0, 0, 0}, {1, 2, 3, 4}, {-5, 7, 640, 480}, {100000, -1, 0, 9}};
    for (const int* r : kBounds)
        Case("Bounds " + N(r[0]), [&](Objects& o) { o.mDialog.mBounds = WONRectangle(r[0], r[1], r[2], r[3]); },
             [&](Objects& o, bool n) { return Call(o, n, Op::Bounds, (intptr_t)(Component*)&o.mDialog, (intptr_t)o.mRect, 0, 0); });
    // a popup window: made (its references after), then set up under the parent
    for (int k = 0; k < 2; k++)
        Case("NewWindow " + N(k), [](Objects&) {},
             [&](Objects& o, bool n) {
                 intptr_t w = Call(o, n, Op::NewWindow, 0, 0, 0, 0);
                 if (k) Call(o, n, Op::SetUpPopup, w, (intptr_t)&o.mParent, 0, 0);
                 return w;
             });
    Case("SetUpPopup", [](Objects&) {},
         [&](Objects& o, bool n) { return Call(o, n, Op::SetUpPopup, (intptr_t)o.mWindow, (intptr_t)&o.mParent, 0, 0); });
    // a status's text on the status dialog (its references while it is shown and after)
    for (long s : {0L, 1L, 100000L, -2803L})
        Case("StatusSetStatus " + N(s), [](Objects&) {},
             [&](Objects& o, bool n) { return Call(o, n, Op::StatusSetStatus, (intptr_t)(Component*)&o.mDialog, s, 0, 0); });
    // the OK/Cancel and Yes/No buttons
    for (int yes : {0, 1, 2})
        Case("OkCancelSetText " + N(yes), [](Objects&) {},
             [&](Objects& o, bool n) { return Call(o, n, Op::OkCancelSetText, (intptr_t)(Component*)&o.mDialog, 11, 12, yes); });
}

int main()
{
    for (int i = 0; i < 700; i++) {
        GUIStringData* d = new GUIStringData;
        d->mId = 1000 + i;
        d->mText = "res" + N(i);
        gLobbyResources[i].mString = GUIString(d);
    }
    Run();
    int aReal = sFailures;
    printf("won_logic35_test: %d checks, %d mismatches\n", sChecks, aReal);
    std::vector<int> aSkips;
    for (int op = (int)Op::WmDefault; op <= (int)Op::OkCancelSetHeldText; op++) aSkips.push_back(op);
    aSkips.push_back((int)Op::AddRefWindow);
    int aCaught = 0;
    for (int s : aSkips) {
        sSkip = s;
        sFailures = 0;
        Run();
        if (sFailures > 0) aCaught++;
        else printf("skip of op %d not caught\n", s);
    }
    sSkip = -1;
    printf("won_logic35_test: %d of %zu skips caught\n", aCaught, aSkips.size());
    return aReal + (int)(aSkips.size() - aCaught);
}
