// The lobby logics' host cases R6 WON lobby and network round 35 composed in Rust (ee_won_lobby logicsplit.rs)
// and LobbyLogicMgr, whole in Rust (logicmgr.rs; RunMgr below), against the deleted code as it was at 207fb805f
// (DialogLogic.cpp's WmPopups, Bounds, NewWindow, SetUpPopup, StatusSetStatus and OkCancelSetText; LobbyLogicMgr.cpp;
// LobbyErrorScreen.cpp's LobbyGetContainer, errscreen.rs: RunErr below),
// over one stand-in world that logs every call ("WON goldens by reach": the popup windows and the Yes/No boxes no
// LAN game opens, the status and bounds cases the LAN game reaches only in part, the manager's Kill and its menu
// clicks, which the LAN game never makes).
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
#include <list>
#include <stdexcept>
#include "rust/won_lobby/dlglogic.h"
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/logicsplit.h"
#include "rust/won_lobby/resource.h"
#include "rust/won_lobby/logicmgr.h"
#include "rust/won_lobby/errscreen.h"
#include "ee/wonapi/WONLobbyGuard.h"

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

// ---- LobbyLogicMgr (logicmgr.rs, whole in Rust this round) against LobbyLogicMgr.cpp as it was at 207fb805f, over
// stand-in logics and events (the LAN game never kills the logics nor clicks a menu item: those branches are here)
namespace Mgr {
using namespace WONAPI;
enum { ComponentEvent_ButtonPressed = 15, ComponentEvent_ListItemUpClicked = 24 };
struct ComponentConfig { int mId; };
struct ComponentEvent {
    int mType = 0;
    int mControlId = 0;
    virtual ~ComponentEvent() {}
    int GetControlId() { Log("GetControlId " + N(this->mControlId)); return this->mControlId; }
};
struct ListItem : RefCount {};
struct MenuItem : ListItem { int mMenuId = 0; };
struct ListItemClickedEvent : ComponentEvent { SmartPtr<ListItem> mItem; int mButton = 0; };
struct LobbyLogic;
static void (*sOnControl)(LobbyLogic*, int);
struct LobbyLogic : RefCount {
    bool mThrowInit = false;
    virtual ~LobbyLogic() {}
    virtual void InitComponent(ComponentConfig* c)
    {
        Log("InitComponent " + Id(this) + " " + N(c->mId));
        if (this->mThrowInit) throw std::runtime_error("config");
    }
    virtual void Reset() { Log("Reset " + Id(this)); }
    virtual void HandleControlEvent(int i) { Log("HandleControlEvent " + Id(this) + " " + N(i)); if (sOnControl) sOnControl(this, i); }
    virtual void HandleComponentEvent(ComponentEvent* e) { Log("HandleComponentEvent " + Id(this) + " " + N(e->mType)); }
    void Kill() { Log("Kill " + Id(this)); }
};

// the original (LobbyLogicMgr.cpp at 207fb805f)
class LobbyLogicMgr {
public:
    void AddLogic(LobbyLogic* logic);
    void Init(ComponentConfig* config);
    void Kill();
    void DispatchControlEvent(int control);
    void DispatchComponentEvent(ComponentEvent* event);
    void DispatchTimerEvent();
    void HandleComponentEvent(ComponentEvent* event);
    typedef std::list<SmartPtr<LobbyLogic> > LogicList;
    LogicList mLogicList;
};
void LobbyLogicMgr::AddLogic(LobbyLogic* logic)
{
    this->mLogicList.push_back(logic);
}
void LobbyLogicMgr::Init(ComponentConfig* config)
{
    LogicList::iterator itr=this->mLogicList.begin();
    while(itr!=this->mLogicList.end()) {
        (*itr)->InitComponent(config);
        ++itr;
    }
}
void LobbyLogicMgr::Kill()
{
    LogicList::iterator itr=this->mLogicList.begin();
    while(itr!=this->mLogicList.end()) {
        (*itr)->Kill();
        ++itr;
    }
}
void LobbyLogicMgr::DispatchControlEvent(int control)
{
    LogicList::iterator itr=this->mLogicList.begin();
    while(itr!=this->mLogicList.end()) {
        (*itr)->HandleControlEvent(control);
        ++itr;
    }
}
void LobbyLogicMgr::DispatchComponentEvent(ComponentEvent* event)
{
    LogicList::iterator itr=this->mLogicList.begin();
    while(itr!=this->mLogicList.end()) {
        (*itr)->HandleComponentEvent(event);
        ++itr;
    }
}
void LobbyLogicMgr::DispatchTimerEvent()
{
    LogicList::iterator itr=this->mLogicList.begin();
    while(itr!=this->mLogicList.end()) {
        (*itr)->Reset();
        ++itr;
    }
}
void LobbyLogicMgr::HandleComponentEvent(ComponentEvent* event)
{
    switch(event->mType) {
    default:
        this->DispatchComponentEvent(event);
        break;
    case ComponentEvent_ListItemUpClicked: {
        ListItemClickedEvent* click=static_cast<ListItemClickedEvent*>(event);
        if(click->mItem.get()==NULL || click->mButton!=0) return;
        MenuItem* menu=dynamic_cast<MenuItem*>(click->mItem.get());
        if(menu!=NULL) this->DispatchControlEvent(menu->mMenuId);
        break;
    }
    case ComponentEvent_ButtonPressed:
        this->DispatchControlEvent(event->GetControlId());
        break;
    }
}

// the new host (LobbyLogicMgr.cpp's, as it is)
static intptr_t LogicMgrHost(EeWonLogicMgrOp inOp, intptr_t a, intptr_t b)
{
    typedef EeWonLogicMgrOp Op;
    if ((int)inOp + 100000 == sSkip) return 0;
    LobbyLogic* aLogic = reinterpret_cast<LobbyLogic*>(a);
    ListItemClickedEvent* aClick = static_cast<ListItemClickedEvent*>(reinterpret_cast<ComponentEvent*>(a));
    switch (inOp) {
    case Op::AddRef: aLogic->AddRef(); return 0;
    case Op::Release: aLogic->Release(); return 0;
    case Op::InitComponent: return WON_GUARDED_V(aLogic->InitComponent(reinterpret_cast<ComponentConfig*>(b)));
    case Op::Kill: return WON_GUARDED_V(aLogic->Kill());
    case Op::HandleControlEvent: return WON_GUARDED_V(aLogic->HandleControlEvent((int)b));
    case Op::HandleComponentEvent: return WON_GUARDED_V(aLogic->HandleComponentEvent(reinterpret_cast<ComponentEvent*>(b)));
    case Op::Reset: return WON_GUARDED_V(aLogic->Reset());
    case Op::ClickItem: return (intptr_t)aClick->mItem.get();
    case Op::ClickButton: return aClick->mButton;
    case Op::IsMenuItem: return dynamic_cast<MenuItem*>(reinterpret_cast<ListItem*>(a)) != NULL ? 1 : 0;
    case Op::MenuId: return dynamic_cast<MenuItem*>(reinterpret_cast<ListItem*>(a))->mMenuId;
    case Op::ControlId: return reinterpret_cast<ComponentEvent*>(a)->GetControlId();
    }
    return 0;
}
// the new class's forwards (WONLobbyLogic.h's, as they are)
struct NewMgr {
    EeWonLogicMgr* mRust = ee_won_logicmgr_new(LogicMgrHost);
    ~NewMgr() { ee_won_logicmgr_free(this->mRust); }
    intptr_t Call(EeWonLogicMgrCall c, intptr_t a = 0, intptr_t b = 0) { return WonRethrow(ee_won_logicmgr_call(this->mRust, c, a, b)); }
    void AddLogic(LobbyLogic* l) { this->Call(EeWonLogicMgrCall::AddLogic, (intptr_t)l); }
    void Init(ComponentConfig* c) { this->Call(EeWonLogicMgrCall::Init, (intptr_t)c); }
    void Kill() { this->Call(EeWonLogicMgrCall::Kill); }
    void DispatchControlEvent(int i) { this->Call(EeWonLogicMgrCall::DispatchControlEvent, i); }
    void DispatchComponentEvent(ComponentEvent* e) { this->Call(EeWonLogicMgrCall::DispatchComponentEvent, (intptr_t)e); }
    void DispatchTimerEvent() { this->Call(EeWonLogicMgrCall::DispatchTimerEvent); }
    void HandleComponentEvent(ComponentEvent* e) { this->Call(EeWonLogicMgrCall::HandleComponentEvent, (intptr_t)e, e->mType); }
};

// one script over either manager: k picks the logics' count, the thrower and the late addition
static LobbyLogic sLogics[5];
static void* sMgr;
static bool sMgrNew;
static void AddLate(LobbyLogic* l, int i)
{
    if (i != 99 || l != &sLogics[0]) return;
    if (sMgrNew) static_cast<NewMgr*>(sMgr)->AddLogic(&sLogics[4]);
    else static_cast<LobbyLogicMgr*>(sMgr)->AddLogic(&sLogics[4]);
}
template <class M> static void Script(M& m, int k)
{
    int aCount = 1 + k % 4;
    for (int i = 0; i < aCount; i++) m.AddLogic(&sLogics[i]);
    ComponentConfig aConfig = {7};
    try {
        m.Init(&aConfig);
        Log("init done");
    } catch (std::exception& e) {
        Log(std::string("init threw ") + e.what());
    }
    m.DispatchTimerEvent();
    m.DispatchControlEvent(99); // logic 0 adds logic 4 here
    ComponentEvent aPlain;
    aPlain.mType = 3;
    m.HandleComponentEvent(&aPlain);
    ComponentEvent aButton;
    aButton.mType = ComponentEvent_ButtonPressed;
    aButton.mControlId = 200011;
    m.HandleComponentEvent(&aButton);
    // clicks: a menu item with the left button, the right button, no item, a list item that is no menu
    MenuItem aMenu;
    aMenu.mMenuId = 4321;
    ListItem aRow;
    for (int c = 0; c < 4; c++) {
        ListItemClickedEvent aClick;
        aClick.mType = ComponentEvent_ListItemUpClicked;
        aClick.mItem = c == 2 ? NULL : c == 3 ? &aRow : static_cast<ListItem*>(&aMenu);
        aClick.mButton = c == 1 ? 1 : 0;
        m.HandleComponentEvent(&aClick);
        aClick.mItem = NULL;
    }
    m.DispatchComponentEvent(&aPlain);
    m.Kill();
}
static void RunMgr()
{
    for (int k = 0; k < 8; k++) {
        std::vector<std::string> t[2];
        for (int side = 0; side < 2; side++) {
            for (int i = 0; i < 5; i++) {
                sLogics[i].mId = 10 + i;
                sLogics[i].mRefs = 0;
                sLogics[i].mThrowInit = k >= 4 && i == 1;
            }
            sOnControl = AddLate;
            sTrace.clear();
            sMgrNew = side == 1;
            if (side == 0) {
                LobbyLogicMgr* m = new LobbyLogicMgr;
                sMgr = m;
                Script(*m, k);
                delete m;
            } else {
                NewMgr* m = new NewMgr;
                sMgr = m;
                Script(*m, k);
                delete m;
            }
            for (int i = 0; i < 5; i++) Log("after " + Id(&sLogics[i]));
            t[side] = sTrace;
        }
        Compare("LobbyLogicMgr " + N(k), t[0], t[1]);
    }
    sOnControl = NULL;
}
}

// ---- LobbyGetContainer (errscreen.rs this round) against LobbyErrorScreen.cpp as it was at 207fb805f, over stand-in
// GUI objects that log every call (the error dialog shows only after a failed lobby start, which no LAN game makes)
namespace Err {
using namespace WONAPI;
enum HorzAlign { HorzAlign_Center = 2 };
enum class EeWonShellKind { MsDlgBack = 1, MsButton = 2 };
struct Comp : RefCount {
    virtual ~Comp() {}
    void Do(const std::string& s) { Log(Id(this) + " " + s); }
};
struct Component : Comp {};
struct Container : Component {};
struct Label : Component {
    void SetWrap(bool b) { Do("SetWrap " + N(b)); }
    void SetHorzAlign(HorzAlign a) { Do("SetHorzAlign " + N(a)); }
    void SetAdjustHeightToFitText(bool b) { Do("SetAdjustHeightToFitText " + N(b)); }
    void SetText(const GUIString& t, bool b) { Do("SetText " + Show(&t) + " " + N(b)); }
};
struct Button : Component {
    void SetButtonFlags(int f, bool b) { Do("SetButtonFlags " + N(f) + " " + N(b)); }
    void SetSizeAtLeast(int w, int h) { Do("SetSizeAtLeast " + N(w) + " " + N(h)); }
    void RequestFocus() { Do("RequestFocus"); }
};
struct CenterLayout {
    int mId;
    static int sNext;
    CenterLayout(Container* d, Component* c, bool h, bool v) : mId(sNext++)
    {
        Log("CenterLayout " + N(mId) + " " + Id(d) + " " + Id(c) + " " + N(h) + " " + N(v));
    }
    void Add(Component* c) { Log("CenterLayout " + N(mId) + " Add " + Id(c)); }
};
int CenterLayout::sNext;
struct Dialog : Container {
    void SetEndOnEscape(int i) { Do("SetEndOnEscape " + N(i)); }
    void Fn2F6140(Component* a, int f, Component* b, int x, int y, int w, int h, int z)
    {
        Do("Fn2F6140 " + Id(a) + " " + N(f) + " " + Id(b) + " " + N(x) + " " + N(y) + " " + N(w) + " " + N(h) + " " + N(z));
    }
    void AddChildLayout(CenterLayout* l, int i) { Do("AddChildLayout " + N(l->mId) + " " + N(i)); }
    void AddChild(Component* c) { Do("AddChild " + Id(c)); }
};
static int sNextComp;
template <class T> static T* New() { T* o = Make<T>(300 + sNextComp++); return o; }
static Component* WONShellNew(EeWonShellKind k, intptr_t a = 0)
{
    Component* c = k == EeWonShellKind::MsButton ? static_cast<Component*>(New<Button>()) : New<Component>();
    Log("WONShellNew " + N((int)k) + " " + Id(c) + (a ? " " + Show((const GUIString*)a) : std::string()));
    return c;
}
struct LobbyContainer : Container {};
struct Prv : RefCount { SmartPtr<LobbyContainer> mLobbyContainer; };
static SmartPtr<Prv> gLobbyData;
static SmartPtr<Dialog> gLobbyFailureDialog;
static GUIString gLobbyInitError;
static bool gLobbyInitFailed;
static GUIString Text(const char* s)
{
    GUIStringData* d = Make<GUIStringData>(700 + sNextComp++);
    d->mText = s;
    return GUIString(d);
}
// GUIString's append (a copy on write as the class does: a shared data is copied first)
static void Append(GUIString& t, const GUIString& a)
{
    if (t.mData->mRefs > 1 || t.mData.get() == &sEmpty) {
        GUIStringData* d = Make<GUIStringData>(700 + sNextComp++);
        d->mText = t.mData->mText;
        t.mData = d;
    }
    t.mData->mText += a.mData->mText;
}

namespace Orig {
// LobbyErrorScreen.cpp at 207fb805f (GUIString's constructor from a text and its append spelt as the stand-in's)
static Container* LobbyGetContainer()
{
    if(gLobbyData.get()!=NULL) return gLobbyData->mLobbyContainer.get();
    if(gLobbyInitFailed) {
    if(gLobbyFailureDialog.get()==NULL) {
        SmartPtr<Dialog> dialog=New<Dialog>();
        dialog->SetEndOnEscape(0);
        SmartPtr<Component> background=WONShellNew(EeWonShellKind::MsDlgBack);
        SmartPtr<Label> label=New<Label>();
        label->SetWrap(true);
        label->SetHorzAlign(HorzAlign_Center);
        label->SetAdjustHeightToFitText(true);
        GUIString text(Text("Lobby Error\n\n"));
        Append(text, gLobbyInitError);
        label->SetText(text,true);
        GUIString okText(Text("Ok"));
        SmartPtr<Button> button=static_cast<Button*>(WONShellNew(EeWonShellKind::MsButton,(intptr_t)&okText));
        button->SetButtonFlags(0x100,true);
        button->SetSizeAtLeast(100,0);
        button->RequestFocus();
        dialog->Fn2F6140(background.get(),3,dialog.get(),0,0,0,0,0);
        dialog->Fn2F6140(label.get(),0x1001,dialog.get(),10,0,-20,0,0);
        dialog->AddChildLayout(new CenterLayout(dialog.get(),button.get(),true,false),0);
        dialog->Fn2F6140(button.get(),0x200,label.get(),0,20,0,0,0);
        CenterLayout* vertical=new CenterLayout(dialog.get(),label.get(),false,true);
        vertical->Add(button.get());
        dialog->AddChildLayout(vertical,0);
        dialog->AddChild(background.get());
        dialog->AddChild(label.get());
        dialog->AddChild(button.get());
        gLobbyFailureDialog=dialog;
    }
    return gLobbyFailureDialog.get();
    }
    return NULL;
}
}

// the new host (LobbyErrorScreen.cpp's, as it is; the stand-in's constructor and append as above)
static SmartPtr<Dialog> sErrDialog;
static SmartPtr<Component> sErrBackground;
static SmartPtr<Label> sErrLabel;
static GUIString sErrText;
static GUIString sErrOkText;
static SmartPtr<Button> sErrButton;
static CenterLayout* sErrVertical;
static intptr_t ErrorScreenHost(EeWonErrScreenOp inOp)
{
    typedef EeWonErrScreenOp Op;
    if ((int)inOp + 200000 == sSkip) {
        // a skipped maker answers another object (a null one would only crash the next statement)
        if (inOp == Op::DialogNew) sErrDialog = Make<Dialog>(990);
        if (inOp == Op::BackgroundNew) sErrBackground = Make<Component>(991);
        if (inOp == Op::LabelNew) sErrLabel = Make<Label>(992);
        if (inOp == Op::ButtonNew) sErrButton = Make<Button>(993);
        if (inOp == Op::VerticalNew) sErrVertical = new CenterLayout(NULL, NULL, false, false);
        return 0;
    }
    switch (inOp) {
    case Op::PrvExists: return gLobbyData.get() != NULL ? 1 : 0;
    case Op::PrvContainer: return (intptr_t)static_cast<Container*>(gLobbyData->mLobbyContainer.get());
    case Op::InitFailed: return gLobbyInitFailed ? 1 : 0;
    case Op::FailureExists: return gLobbyFailureDialog.get() != NULL ? 1 : 0;
    case Op::FailureGet: return (intptr_t)static_cast<Container*>(gLobbyFailureDialog.get());
    case Op::DialogNew: sErrDialog = New<Dialog>(); return 0;
    case Op::DialogNoEscape: sErrDialog->SetEndOnEscape(0); return 0;
    case Op::BackgroundNew: sErrBackground = WONShellNew(EeWonShellKind::MsDlgBack); return 0;
    case Op::LabelNew: sErrLabel = New<Label>(); return 0;
    case Op::LabelWrap: sErrLabel->SetWrap(true); return 0;
    case Op::LabelCenter: sErrLabel->SetHorzAlign(HorzAlign_Center); return 0;
    case Op::LabelFit: sErrLabel->SetAdjustHeightToFitText(true); return 0;
    case Op::TextNew: sErrText = Text("Lobby Error\n\n"); return 0;
    case Op::TextAppendError: Append(sErrText, gLobbyInitError); return 0;
    case Op::LabelSetText: sErrLabel->SetText(sErrText, true); return 0;
    case Op::OkTextNew: sErrOkText = Text("Ok"); return 0;
    case Op::ButtonNew: sErrButton = static_cast<Button*>(WONShellNew(EeWonShellKind::MsButton, (intptr_t)&sErrOkText)); return 0;
    case Op::ButtonFlags: sErrButton->SetButtonFlags(0x100, true); return 0;
    case Op::ButtonSize: sErrButton->SetSizeAtLeast(100, 0); return 0;
    case Op::ButtonFocus: sErrButton->RequestFocus(); return 0;
    case Op::LayoutBackground: sErrDialog->Fn2F6140(sErrBackground.get(), 3, sErrDialog.get(), 0, 0, 0, 0, 0); return 0;
    case Op::LayoutLabel: sErrDialog->Fn2F6140(sErrLabel.get(), 0x1001, sErrDialog.get(), 10, 0, -20, 0, 0); return 0;
    case Op::CenterButton: sErrDialog->AddChildLayout(new CenterLayout(sErrDialog.get(), sErrButton.get(), true, false), 0); return 0;
    case Op::LayoutButton: sErrDialog->Fn2F6140(sErrButton.get(), 0x200, sErrLabel.get(), 0, 20, 0, 0, 0); return 0;
    case Op::VerticalNew: sErrVertical = new CenterLayout(sErrDialog.get(), sErrLabel.get(), false, true); return 0;
    case Op::VerticalAdd: sErrVertical->Add(sErrButton.get()); return 0;
    case Op::VerticalLayout: sErrDialog->AddChildLayout(sErrVertical, 0); return 0;
    case Op::AddBackground: sErrDialog->AddChild(sErrBackground.get()); return 0;
    case Op::AddLabel: sErrDialog->AddChild(sErrLabel.get()); return 0;
    case Op::AddButton: sErrDialog->AddChild(sErrButton.get()); return 0;
    case Op::FailureSet: gLobbyFailureDialog = sErrDialog; return 0;
    case Op::ButtonDrop: sErrButton = NULL; return 0;
    case Op::OkTextDrop: sErrOkText = GUIString(); return 0;
    case Op::TextDrop: sErrText = GUIString(); return 0;
    case Op::LabelDrop: sErrLabel = NULL; return 0;
    case Op::BackgroundDrop: sErrBackground = NULL; return 0;
    case Op::DialogDrop: sErrDialog = NULL; return 0;
    }
    return 0;
}

static void RunErr()
{
    // the lobby up; down and not failed; failed with the dialog to build (two errors), then already built
    for (int k = 0; k < 5; k++) {
        std::vector<std::string> t[2];
        for (int side = 0; side < 2; side++) {
            sMade.clear();
            sNextComp = 0;
            CenterLayout::sNext = 0;
            sErrDialog = NULL;
            sErrBackground = NULL;
            sErrLabel = NULL;
            sErrText = GUIString();
            sErrOkText = GUIString();
            sErrButton = NULL;
            gLobbyData = NULL;
            gLobbyFailureDialog = NULL;
            gLobbyInitFailed = k >= 2;
            gLobbyInitError = k == 3 ? Text("") : Text("no WONLobby.cfg");
            if (k == 0) {
                gLobbyData = Make<Prv>(1);
                gLobbyData->mLobbyContainer = Make<LobbyContainer>(2);
            }
            if (k == 4) gLobbyFailureDialog = Make<Dialog>(3);
            sTrace.clear();
            Container* r = side == 0 ? Orig::LobbyGetContainer() : reinterpret_cast<Container*>(ee_won_lobby_error_container(ErrorScreenHost));
            Log("result " + Id(r));
            // a second call returns the dialog the first built
            Container* r2 = side == 0 ? Orig::LobbyGetContainer() : reinterpret_cast<Container*>(ee_won_lobby_error_container(ErrorScreenHost));
            Log("again " + Id(r2) + " failure " + Id(gLobbyFailureDialog.get()));
            gLobbyData = NULL;
            gLobbyFailureDialog = NULL;
            for (RefCount* m : sMade) Log("made " + Id(m));
            t[side] = sTrace;
        }
        Compare("LobbyGetContainer " + N(k), t[0], t[1]);
    }
}
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
    Mgr::RunMgr();
    Err::RunErr();
    int aReal = sFailures;
    printf("won_logic35_test: %d checks, %d mismatches\n", sChecks, aReal);
    std::vector<int> aSkips;
    for (int op = (int)Op::WmDefault; op <= (int)Op::OkCancelSetHeldText; op++) aSkips.push_back(op);
    aSkips.push_back((int)Op::AddRefWindow);
    for (int op = (int)EeWonLogicMgrOp::AddRef; op <= (int)EeWonLogicMgrOp::ControlId; op++) aSkips.push_back(op + 100000);
    for (int op = (int)EeWonErrScreenOp::PrvExists; op <= (int)EeWonErrScreenOp::DialogDrop; op++) aSkips.push_back(op + 200000);
    int aCaught = 0;
    for (int s : aSkips) {
        sSkip = s;
        sFailures = 0;
        Run();
        Mgr::RunMgr();
        Err::RunErr();
        if (sFailures > 0) aCaught++;
        else printf("skip of op %d not caught\n", s);
    }
    sSkip = -1;
    printf("won_logic35_test: %d of %zu skips caught\n", aCaught, aSkips.size());
    return aReal + (int)(aSkips.size() - aCaught);
}
