// Component wholly in Rust (R6 WON round 38): the original Component.cpp, ComponentTraversal.cpp,
// ComponentDefaultHooks.cpp, ComponentApiCompletion.cpp and ComponentResourceAdapters.cpp statements (and
// MSControlConstructors.cpp's SetComponentFlags), copied from the C++ before the port, against ee_won's compobj.rs
// through a host that makes the same calls on the same stand-ins as the ported unit's host (ComponentLifetime.cpp
// ee_won_guicpp_comp_call).
//
// The world: 3-7 components (each a traced Container stand-in, the first sometimes the RootContainer of window 1)
// in a random tree, random bounds, flags (every bit the methods test), control ids, invalid states, timer flags and
// extra data (listener, cursor, colour scheme, background grabber, tool tip, the tip flag); window 1 (open or not, its
// mouse, key modifiers, a parent window 2 whose root is one of the components or none), the default window 3, an id
// set, the events the calls make or take (their references counted: an event is deleted, and traced, at its last
// Release). Every virtual a method calls is a traced override that calls the base (the method under test); every call
// on a parent, window, event, listener or grabber is traced. Each of 20,000 sequences runs 40 random Component calls
// from the original's side and from the Rust's over two copies of the world; after each, the traces, the result and
// every component's members are compared. The first 300 sequences are rust/crates/ee_won_gui/src/golden_compobj.txt, which
// the Rust tests replay over the world rebuilt in Rust; `--print` prints them.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include "rust/ee_won.h"

typedef uint32_t DWORD;
static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }
static int RI(int lo, int hi) { return lo + (int)R((unsigned)(hi - lo + 1)); }

static std::vector<std::string> sTrace;
static void T(const char* f, ...)
{
    char aLine[200];
    va_list v;
    va_start(v, f);
    vsnprintf(aLine, sizeof(aLine), f, v);
    va_end(v);
    sTrace.push_back(aLine);
    if (getenv("EE_COMPOBJ_ECHO")) fprintf(stderr, "%s\n", aLine);
}
static bool gRust;
struct World;
static World* gW; // the world the call runs in (its event and grabber numbers)
static int* NextEv();
static int* NextGrab();
static void LiveEv(void* e, bool on);
static bool gQuietDelete;

// ---- the stand-ins; every object has the id the Rust replay uses for it (components 1.., windows 1001..,
// events 2001.., listeners 3001.., cursors 4001.., schemes 5001.., grabbers 6001.., fonts 7001.., Graphics 8001)
struct RefCount {
    int mRefCount;
    int mId;
    RefCount() : mRefCount(0), mId(0) {}
    virtual ~RefCount() {}
    void AddRef() { ++mRefCount; }
    void Release() { if (--mRefCount <= 0) delete this; }
};
template <class X> struct SmartPtr {
    X* p;
    SmartPtr(X* a = NULL) : p(a) { if (p) p->AddRef(); }
    SmartPtr(const SmartPtr& o) : p(o.p) { if (p) p->AddRef(); }
    ~SmartPtr() { if (p) p->Release(); }
    SmartPtr& operator=(X* a) { if (a) a->AddRef(); if (p) p->Release(); p = a; return *this; }
    X* get() const { return p; }
    X* operator->() const { return p; }
};
static int Id(const RefCount* o) { return o ? o->mId : 0; }
struct WONRectangle {
    int x, y, width, height;
    WONRectangle() : x(0), y(0), width(0), height(0) {}
    WONRectangle(int a, int b, int c, int d) : x(a), y(b), width(c), height(d) {}
    int Width() const { return width; }
    int Height() const { return height; }
    bool Intersects(const WONRectangle& o) const
    {
        EeWonRect a = {x, y, width, height}, b = {o.x, o.y, o.width, o.height};
        return ee_won_gui_rect_intersects(a, b);
    }
};
enum ComponentFlag {
    ComponentFlag_Invisible = 0x1, ComponentFlag_HasFocus = 0x10, ComponentFlag_Disabled = 0x80,
    ComponentFlag_Translucent = 0x200, ComponentFlag_NoInput = 0x800, ComponentFlag_WantTabFocus = 0x4000,
};
enum { PopupFlag_StandardTip = 0x6247 };
enum MouseButton { MouseButton_Left = 0 };
typedef std::set<long> ControlIdSet;
struct Graphics { int mId; };
struct Cursor : RefCount {};
struct ColorScheme : RefCount {};
struct FontDescriptor { int mStyle, mSize; };
struct Font : RefCount { FontDescriptor mDescriptor; };
class Component;
class Container;
class RootContainer;
class Window;
struct ComponentEvent : RefCount {
    bool mBroadcast;
    Component* mComponent;
    ComponentEvent(Component* c, int type);
    ~ComponentEvent() { if (!gQuietDelete) T("delete ev%d", mId); LiveEv(this, false); }
    void SetComponent(Component* c);
};
struct ComponentListener : RefCount {
    void HandleComponentEvent(ComponentEvent* e) { T("listener %d ev%d", mId, Id(e)); }
};
struct BackgroundGrabber {
    int mId;
    BackgroundGrabber() : mId(6000 + ++*NextGrab()) { T("new grab %d", mId); }
    void SizeChanged(Component* c);
    void PrePaint(Component* c, Graphics& g);
    void Paint(Component* c, Graphics& g);
    void Clear() { T("grab %d clear", mId); }
    void Invalidate(Component* c);
    bool NeedDrawBG() const { return (mId & 1) != 0; }
    void SetTranslucentColor(int c, int l) { T("grab %d translucent %d %d", mId, c, l); }
};
class ComponentExtra {
public:
    ComponentExtra() { this->mGrabBG = NULL; this->mDontDoTip = false; }
    ~ComponentExtra() { delete this->mGrabBG; }
    SmartPtr<ColorScheme> mColorScheme;
    SmartPtr<ComponentListener> mListener;
    SmartPtr<Cursor> mDefaultCursor;
    BackgroundGrabber* mGrabBG;
    SmartPtr<Component> mToolTip;
    bool mDontDoTip;
};
struct PopupParams {
    Component* mPopup; int mFlags; Component* mOwner; int mx, my;
    PopupParams(Component* p, int f, Component* o, int x, int y) : mPopup(p), mFlags(f), mOwner(o), mx(x), my(y) {}
};
struct WindowManager {
    int mWin;
    SmartPtr<ColorScheme> mColorScheme;
    void BroadcastEvent(ComponentEvent* e);
    void SubscribeToBroadcast(Component* c, bool on);
};

extern "C" intptr_t ee_won_guicpp_comp_call(void*, EeWonGuiOp op, intptr_t k, intptr_t b, intptr_t c, intptr_t d);

class Component : public RefCount {
public:
    Component() : mParent(NULL), mComponentBits(0), mControlId(0), mFocusId(0), mInvalidState(0), mTimerFlags(0),
                  mExtraStuff(NULL) {}
    virtual ~Component() { delete this->mExtraStuff; }
    intptr_t K(EeWonCompCall k, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
    { return ee_won_comp_call(this, ee_won_guicpp_comp_call, k, a, b, c); }
    // the virtuals (the world's overrides trace and call these)
    virtual void PositionChanged() {}
    virtual void Virtual08();
    virtual void GetDesiredSize(int& width, int& height);
    virtual bool NeedsPaint();
    virtual void PrePaint(Graphics& graphics);
    virtual void Paint(Graphics& graphics);
    virtual bool HitTest(int, int);
    virtual void MouseEnter(int x, int y);
    virtual void MouseExit();
    virtual void MouseHover(unsigned long elapsed);
    virtual void GotFocus();
    virtual void LostFocus();
    virtual void InvalidateRect(WONRectangle& rect);
    virtual void Invalidate();
    virtual void MarkInvalid();
    virtual void AddedToParent();
    virtual void ClearGrabBG();
    virtual void EnabledChanged(bool) {}
    virtual void SetCursor(Cursor* cursor);
    virtual void HandleComponentEvent(ComponentEvent* event);
    virtual void HandleBroadcastEvent(ComponentEvent* event);
    virtual void NotifyRootVisibility(bool visible);
    virtual bool TabFocus(bool forward, bool init);
    virtual void EnableControlsById(const ControlIdSet& ids, bool enabled);
    virtual void SetEnabled(bool enabled);
    virtual void SetVisible(bool visible);
    void SetColorScheme(ColorScheme* scheme);
    ColorScheme* GetOwnColorScheme();
    ColorScheme* GetColorScheme();
    void SetPos(int x, int y);
    void SetSize(int width, int height);
    void SetSizeAtLeast(int width, int height);
    void SetPosSize(int x, int y, int width, int height);
    void FireBroadcastEvent(ComponentEvent* event, bool sendNow);
    void BroadcastEventDown(ComponentEvent* event);
    void DeliverEvent(ComponentEvent* event, bool sendNow);
    SmartPtr<Font> GetFont(const FontDescriptor& descriptor);
    Font* GetFontMod(Font* font, int style, int size);
    void AddListener(ComponentListener* listener);
    void SetDesiredSize(int extraWidth, int extraHeight);
    void FireEvent(int type, bool sendNow);
    bool IsRootVisible();
    void LocalToRoot(int& x, int& y);
    void BubbleEvent(ComponentEvent* event);
    Component* GetNextSibling(bool forward, bool wrap);
    bool WantFocus();
    void RootToChild(int& x, int& y);
    void GetMousePos(int& x, int& y);
    int GetKeyMod();
    bool CheckDoubleClick(int button);
    bool Contains(int x, int y);
    bool Intersects(Component* component);
    Cursor* GetDefaultCursor();
    BackgroundGrabber* GetGrabBG();
    void AllocateExtraStuff() { if (this->mExtraStuff == NULL) { T("alloc extra %d", mId); this->mExtraStuff = new ComponentExtra; } }
    bool CheckGrabBG();
    void EnsureOnTopOf(Component* sibling);
    void SetDefaultCursor(Cursor* cursor);
    void SetDesiredHeight(int padding);
    void SetDesiredWidth(int padding);
    void SetTranslucentColor(int color, int level);
    void SetComponentFlags(int flags, bool on);
    void SubscribeToBroadcast(bool subscribe);
    void RequestTimer(bool on);
    void RequestFocus();
    Window* GetRoot();
    RootContainer* GetRootContainer();
    Window* GetRootWindow();
    bool ComponentFlagSet(int flags) { return this->mComponentBits & flags ? true : false; }
    bool IsVisible() { return !this->ComponentFlagSet(1); }
    bool HasFocus() { return this->ComponentFlagSet(0x10); }
    bool IsDisabled() const { return (this->mComponentBits & 0x80) != 0; }
    int Left() { return this->mBounds.x; }
    int Top() { return this->mBounds.y; }
    int Width() { return this->mBounds.Width(); }
    int Height() { return this->mBounds.Height(); }
    Container* mParent;
    WONRectangle mBounds;
    int mComponentBits;
    int mControlId;
    unsigned char mFocusId;
    char mInvalidState;
    char mTimerFlags;
    ComponentExtra* mExtraStuff;
};
// the members compobj reads and writes in place: compmem.rs's block, at its offset (this RefCount's two ints)
static_assert(offsetof(Component, mParent) == EE_WON_COMP_MEMBERS_AT && offsetof(Component, mBounds) == EE_WON_COMP_MEMBERS_AT + offsetof(EeWonCompMembers, bounds) &&
              offsetof(Component, mComponentBits) == EE_WON_COMP_MEMBERS_AT + offsetof(EeWonCompMembers, bits) &&
              offsetof(Component, mTimerFlags) == EE_WON_COMP_MEMBERS_AT + offsetof(EeWonCompMembers, timer) &&
              offsetof(Component, mExtraStuff) == EE_WON_COMP_MEMBERS_AT + offsetof(EeWonCompMembers, extra), "compmem.rs's block");
class Container : public Component {
public:
    bool mModal;
    int mN;
    Container() : mModal(false), mN(0) {}
    bool IsModal() { return mModal; }
    Component* NextChild(Component* c, bool f, bool w);
    virtual void SetFocusChild(Component* c) { T("%d setfocuschild %d", mId, Id(c)); }
    virtual void AddTimerChild(Component* c) { T("%d addtimerchild %d", mId, Id(c)); }
    virtual void ChildStateChanged(Component* c) { T("%d childstate %d", mId, Id(c)); }
    virtual bool ChildInvalidated(Component* c) { T("%d childinvalidated %d", mId, Id(c)); return true; }
    void EnsureOnTopOf(Component* b, Component* t) { T("%d ensureontop %d %d", mId, Id(b), Id(t)); }
};
class RootContainer : public Container {
public:
    Window* mWindow;
    void AddBG(BackgroundGrabber* g) { T("%d addbg grab %d", mId, g ? g->mId : 0); }
};
class Window : public RefCount {
public:
    int mState, mMouseX, mMouseY, mKeyMod;
    Window* mParentWin;
    Component* mRootComp;
    WindowManager* mManager;
    std::map<std::pair<int, int>, Font*> mFonts;
    static Window* sDefault;
    static Window* GetDefaultWindow() { return sDefault; }
    int GetWindowState() { return mState; }
    void QueueEvent(ComponentEvent* e) { T("w%d queue ev%d", mId, Id(e)); e->AddRef(); }
    Window* GetParent() { return mParentWin; }
    int GetMouseX() { return mMouseX; }
    int GetMouseY() { return mMouseY; }
    int GetKeyMod() { return mKeyMod; }
    bool CheckDoubleClick(MouseButton b) { T("w%d dblclick %d", mId, (int)b); return b == 1; }
    void OpenPopup(PopupParams& p) { T("w%d open %d %x %d %d %d", mId, Id(p.mPopup), p.mFlags, Id(p.mOwner), p.mx, p.my); }
    void ClosePopup(Component* c) { T("w%d close %d", mId, Id(c)); }
    WindowManager* GetWindowManager() { return mManager; }
    SmartPtr<Font> GetFont(const FontDescriptor& d)
    {
        T("w%d font %d %d", mId, d.mStyle, d.mSize);
        Font*& f = mFonts[std::make_pair(d.mStyle, d.mSize)];
        if (f == NULL) { f = new Font; f->mId = 7000 + d.mStyle * 50 + (d.mSize & 31) + mId % 1000 * 1000; f->mDescriptor = d; f->AddRef(); }
        return f;
    }
};
Window* Window::sDefault;
static Container* AsC(Component* c) { return static_cast<Container*>(c); }
ComponentEvent::ComponentEvent(Component* c, int type) : mBroadcast(false), mComponent(c)
{
    mId = 2000 + ++*NextEv();
    LiveEv(this, true);
    T("new ev%d %d type %d", mId, Id(c), type);
}
void ComponentEvent::SetComponent(Component* c) { T("ev%d setcomp %d", mId, Id(c)); mComponent = c; }
void WindowManager::BroadcastEvent(ComponentEvent* e) { T("wm%d broadcast ev%d", mWin, Id(e)); }
void WindowManager::SubscribeToBroadcast(Component* c, bool on) { T("wm%d subscribe %d %d", mWin, Id(c), on); }
void BackgroundGrabber::SizeChanged(Component* c) { T("grab %d sizechanged %d", mId, Id(c)); }
void BackgroundGrabber::PrePaint(Component* c, Graphics& g) { T("grab %d prepaint %d g%d", mId, Id(c), g.mId); }
void BackgroundGrabber::Paint(Component* c, Graphics& g) { T("grab %d paint %d g%d", mId, Id(c), g.mId); }
void BackgroundGrabber::Invalidate(Component* c) { T("grab %d invalidate %d", mId, Id(c)); }

// the world's class: every virtual traced, then the base (the method under test)
template <class B> class Traced : public B {
public:
    void PositionChanged() { T("%d positionchanged", this->mId); }
    void Virtual08() { T("%d sizechanged", this->mId); B::Virtual08(); }
    void GetDesiredSize(int& w, int& h)
    {
        T("%d desired", this->mId);
        if (this->mId & 1) B::GetDesiredSize(w, h);
        else { w = 10 + 3 * this->mId; h = 7 + this->mId; }
    }
    void InvalidateRect(WONRectangle& r) { T("%d invalidaterect %d %d %d %d", this->mId, r.x, r.y, r.width, r.height); B::InvalidateRect(r); }
    void Invalidate() { T("%d invalidate", this->mId); B::Invalidate(); }
    void MarkInvalid() { T("%d markinvalid", this->mId); B::MarkInvalid(); }
    void GotFocus() { T("%d gotfocus", this->mId); B::GotFocus(); }
    void LostFocus() { T("%d lostfocus", this->mId); B::LostFocus(); }
    void NotifyRootVisibility(bool v) { T("%d rootvisibility %d", this->mId, v); B::NotifyRootVisibility(v); }
    void SetEnabled(bool e) { T("%d setenabled %d", this->mId, e); B::SetEnabled(e); }
    void EnabledChanged(bool e) { T("%d enabledchanged %d", this->mId, e); }
    void SetCursor(Cursor* c) { T("%d setcursor %d", this->mId, Id(c)); B::SetCursor(c); }
    void HandleComponentEvent(ComponentEvent* e) { T("%d handle ev%d", this->mId, Id(e)); B::HandleComponentEvent(e); }
    void HandleBroadcastEvent(ComponentEvent* e) { T("%d handlebroadcast ev%d", this->mId, Id(e)); B::HandleBroadcastEvent(e); }
};
typedef Traced<Container> TC;
typedef Traced<RootContainer> TR;


// ---- the original statements (each method's Rust forwarder first: the C++ shell's one line)
// Component.cpp
void Component::BroadcastEventDown(ComponentEvent* event)
{
    if (gRust) { K(EeWonCompCall::BroadcastDown, (intptr_t)event); return; }
    SmartPtr<ComponentEvent> eventRef = event;
    event->mBroadcast = true;
    this->HandleBroadcastEvent(event);
}
void Component::DeliverEvent(ComponentEvent* event, bool sendNow)
{
    if (gRust) { K(EeWonCompCall::Deliver, (intptr_t)event, sendNow); return; }
    Window* window = this->GetRootWindow();
    if(window == NULL || window->GetWindowState() != 0)
    {
        SmartPtr<ComponentEvent> eventRef = event;
        return;
    }
    if(event->mComponent == NULL)
        event->SetComponent(this);
    if(sendNow)
    {
        SmartPtr<ComponentEvent> eventRef = event;
        if(event->mBroadcast)
        {
            WindowManager* aManager = window->mManager;
            aManager->BroadcastEvent(event);
        }
        else
            this->BubbleEvent(event);
    }
    else
        window->QueueEvent(event);
}
void Component::SetPos(int x, int y)
{
    if (gRust) { K(EeWonCompCall::SetPos, x, y); return; }
    this->mBounds.x = x;
    this->mBounds.y = y;
    this->PositionChanged();
}
void Component::SetSize(int width, int height)
{
    if (gRust) { K(EeWonCompCall::SetSize, width, height); return; }
    if(!ee_won_gui_set_size(this->Width(), this->Height(), width, height, &width, &height))
        return;
    this->mBounds.height = height;
    this->mBounds.width = width;
    this->Virtual08();
}
void Component::SetSizeAtLeast(int width, int height)
{
    if (gRust) { K(EeWonCompCall::SetSizeAtLeast, width, height); return; }
    if(ee_won_gui_size_at_least(this->Width(), this->Height(), width, height, &width, &height))
        this->SetSize(width,height);
}
void Component::SetPosSize(int x, int y, int width, int height)
{
    if (gRust) { int s[2] = {width, height}; K(EeWonCompCall::SetPosSize, x, y, (intptr_t)s); return; }
    this->SetPos(x,y);
    this->SetSize(width,height);
}
void Component::SetDesiredSize(int extraWidth,int extraHeight)
{
    if (gRust) { K(EeWonCompCall::SetDesiredSize, extraWidth, extraHeight); return; }
    int width = 0, height = 0;
    this->GetDesiredSize(width,height);
    this->SetSize(width+extraWidth,height+extraHeight);
}
void Component::SetDesiredWidth(int padding)
{
    if (gRust) { K(EeWonCompCall::SetDesiredWidth, padding); return; }
    int width = 0, height = 0;
    this->GetDesiredSize(width,height);
    this->SetSize(width+padding,this->Height());
}
void Component::LocalToRoot(int& x,int& y)
{
    if (gRust) { K(EeWonCompCall::LocalToRoot, (intptr_t)&x, (intptr_t)&y); return; }
    x += this->Left();
    y += this->Top();
    for(Component* parent=this->mParent; parent!=NULL; parent=parent->mParent)
    {
        x += parent->Left();
        y += parent->Top();
    }
}
void Component::InvalidateRect(WONRectangle& rect)
{
    if (gRust) { K(EeWonCompCall::InvalidateRect, (intptr_t)&rect); return; }
    if(this->mParent == NULL)
        return;
    rect.x += this->Left();
    rect.y += this->Top();
    this->mParent->InvalidateRect(rect);
}
void Component::EnableControlsById(const ControlIdSet& ids,bool enabled)
{
    if (gRust) { K(EeWonCompCall::EnableControlsById, (intptr_t)&ids, enabled); return; }
    if(this->mControlId!=0)
    {
        int id=this->mControlId;
        if(ids.find(id)!=ids.end()) this->SetEnabled(enabled);
    }
}
bool Component::Contains(int x,int y)
{
    if (gRust) return K(EeWonCompCall::Contains, x, y) != 0;
    return x>=this->Left() && x<this->Left()+this->Width() && y>=this->Top() && y<this->Top()+this->Height();
}
void Component::SetVisible(bool visible)
{
    if (gRust) { K(EeWonCompCall::SetVisible, visible); return; }
    if(visible == this->IsVisible())
        return;
    this->SetComponentFlags(ComponentFlag_Invisible,!visible);
    this->mInvalidState=0;
    if(this->mParent!=NULL) this->mParent->ChildStateChanged(this);
    this->Invalidate();
    this->NotifyRootVisibility(visible);
}
void Component::SetEnabled(bool enabled)
{
    if (gRust) { K(EeWonCompCall::SetEnabled, enabled); return; }
    if(enabled == !this->IsDisabled())
        return;
    this->SetComponentFlags(ComponentFlag_Disabled,!enabled);
    this->EnabledChanged(enabled);
    if(this->mParent!=NULL) this->mParent->ChildStateChanged(this);
    this->Invalidate();
}
bool Component::Intersects(Component* component)
{
    if (gRust) return K(EeWonCompCall::Intersects, (intptr_t)component) != 0;
    return this->mBounds.Intersects(component->mBounds);
}
// ComponentTraversal.cpp
Component* Component::GetNextSibling(bool forward,bool wrap)
{
    if (gRust) return (Component*)K(EeWonCompCall::NextSibling, forward, wrap);
    if(this->mParent==NULL) return NULL;
    return this->mParent->NextChild(this,forward,wrap);
}
void Component::AddedToParent()
{
    if (gRust) { K(EeWonCompCall::AddedToParent); return; }
    if(this->HasFocus()) {
        if(!this->WantFocus()) this->LostFocus();
        else this->mParent->SetFocusChild(this);
    }
    if(this->mTimerFlags!=0) this->mParent->AddTimerChild(this);
    if(this->mParent->IsRootVisible() && this->IsVisible()) this->NotifyRootVisibility(true);
    this->mInvalidState=0;
    this->Invalidate();
}
void Component::RootToChild(int& x,int& y)
{
    if (gRust) { K(EeWonCompCall::RootToChild, (intptr_t)&x, (intptr_t)&y); return; }
    x-=this->Left();
    y-=this->Top();
    if(this->mParent!=NULL) this->mParent->RootToChild(x,y);
}
bool Component::WantFocus()
{
    if (gRust) return K(EeWonCompCall::WantFocus) != 0;
    return ee_won_gui_want_focus((uint32_t)this->mComponentBits,this->IsVisible(),this->IsDisabled(),
                                 this->mParent!=NULL && this->mParent->IsModal());
}
SmartPtr<Font> Component::GetFont(const FontDescriptor& descriptor)
{
    Window* root=this->GetRoot();
    return root->GetFont(descriptor);
}
Font* Component::GetFontMod(Font* font,int style,int size)
{
    if (gRust) return (Font*)K(EeWonCompCall::FontMod, (intptr_t)font, style, size);
    FontDescriptor descriptor=font->mDescriptor;
    descriptor.mStyle=style;
    if(size>0) descriptor.mSize=size;
    return this->GetFont(descriptor).get();
}
void Component::BubbleEvent(ComponentEvent* event)
{
    if (gRust) { K(EeWonCompCall::Bubble, (intptr_t)event); return; }
    if(event->mBroadcast) return;
    if(this->mExtraStuff!=NULL && this->mExtraStuff->mListener.get()!=NULL)
        this->mExtraStuff->mListener->HandleComponentEvent(event);
    else if(this->mParent!=NULL)
        this->mParent->HandleComponentEvent(event);
    else {
        Window* window=this->GetRoot();
        if(window!=NULL) {
            window=window->GetParent();
            if(window!=NULL && window->mRootComp!=NULL)
                window->mRootComp->BubbleEvent(event);
        }
    }
}
DWORD gComponentToolTipStartTime=500;
DWORD gComponentToolTipEndTime=5000;
void Component::MouseEnter(int x,int y)
{
    if (gRust) { K(EeWonCompCall::MouseEnter, x, y); return; }
    if(this->mExtraStuff!=NULL) this->SetCursor(this->mExtraStuff->mDefaultCursor.get());
}
void Component::MouseExit()
{
    if (gRust) { K(EeWonCompCall::MouseExit); return; }
    if(this->mExtraStuff!=NULL && this->mExtraStuff->mToolTip.get()!=NULL) {
        this->mExtraStuff->mDontDoTip=false;
        this->GetRoot()->ClosePopup(this->mExtraStuff->mToolTip.get());
    }
}
void Component::MouseHover(unsigned long elapsed)
{
    if (gRust) { K(EeWonCompCall::MouseHover, (intptr_t)elapsed); return; }
    if(this->mExtraStuff==NULL || this->mExtraStuff->mToolTip.get()==NULL) return;
    if(elapsed>=gComponentToolTipStartTime && !this->mExtraStuff->mDontDoTip) {
        Window* window=this->GetRoot();
        int x=window->GetMouseX()+15; int y=window->GetMouseY()+15;
        PopupParams params(this->mExtraStuff->mToolTip.get(),PopupFlag_StandardTip,this,x,y);
        window->OpenPopup(params);
        this->mExtraStuff->mDontDoTip=true;
    } else if(elapsed>=gComponentToolTipStartTime+gComponentToolTipEndTime)
        this->GetRoot()->ClosePopup(this->mExtraStuff->mToolTip.get());
}
void Component::NotifyRootVisibility(bool visible)
{
    if (gRust) { K(EeWonCompCall::NotifyRootVisibility, visible); return; }
    this->SetComponentFlags(0x40000,visible);
}
bool Component::TabFocus(bool forward,bool init)
{
    if (gRust) return K(EeWonCompCall::TabFocus, forward, init) != 0;
    if(init && this->ComponentFlagSet(ComponentFlag_WantTabFocus) && this->WantFocus()) {
        this->RequestFocus();
        return true;
    }
    return false;
}
void Component::RequestTimer(bool on)
{
    if (gRust) { K(EeWonCompCall::RequestTimer, on); return; }
    bool oldWantTimer=this->mTimerFlags!=0;
    if(on) this->mTimerFlags|=2;
    else this->mTimerFlags&=~2;
    bool wantTimer=this->mTimerFlags!=0;
    if(wantTimer!=oldWantTimer && this->mParent!=NULL) this->mParent->AddTimerChild(this);
}
void Component::RequestFocus()
{
    if (gRust) { K(EeWonCompCall::RequestFocus); return; }
    if(this->HasFocus() || !this->WantFocus()) return;
    if(this->mParent==NULL) this->GotFocus();
    else this->mParent->SetFocusChild(this);
}
RootContainer* Component::GetRootContainer()
{
    if (gRust) return (RootContainer*)K(EeWonCompCall::RootContainer);
    Component* root=this;
    while(root->mParent!=NULL) root=root->mParent;
    return dynamic_cast<RootContainer*>(root);
}
Window* Component::GetRoot()
{
    if (gRust) return (Window*)K(EeWonCompCall::Root);
    RootContainer* root=this->GetRootContainer();
    if(root!=NULL) return root->mWindow;
    return Window::GetDefaultWindow();
}
Window* Component::GetRootWindow()
{
    if (gRust) return (Window*)K(EeWonCompCall::RootWindow);
    RootContainer* root=this->GetRootContainer();
    if(root!=NULL) return root->mWindow;
    return NULL;
}
bool Component::IsRootVisible()
{
    if (gRust) return K(EeWonCompCall::IsRootVisible) != 0;
    return this->ComponentFlagSet(0x40000);
}
void Component::GotFocus()
{
    if (gRust) { K(EeWonCompCall::GotFocus); return; }
    this->SetComponentFlags(ComponentFlag_HasFocus,true);
}
void Component::LostFocus()
{
    if (gRust) { K(EeWonCompCall::LostFocus); return; }
    this->SetComponentFlags(ComponentFlag_HasFocus,false);
}
bool Component::CheckGrabBG()
{
    if (gRust) return K(EeWonCompCall::CheckGrabBG) != 0;
    if(this->ComponentFlagSet(0x300)) {
        this->AllocateExtraStuff();
        if(this->mExtraStuff->mGrabBG==NULL)
            this->mExtraStuff->mGrabBG=new BackgroundGrabber;
        return true;
    }
    return false;
}
bool Component::NeedsPaint()
{
    if (gRust) return K(EeWonCompCall::NeedsPaint) != 0;
    if(this->mInvalidState==0) return false;
    if(this->IsVisible()) return true;
    return this->CheckGrabBG() && this->mExtraStuff->mGrabBG->NeedDrawBG();
}
void Component::Invalidate()
{
    if (gRust) { K(EeWonCompCall::Invalidate); return; }
    if(this->mInvalidState==2) return;
    WONRectangle rect(0,0,this->Width(),this->Height());
    this->InvalidateRect(rect);
    if(this->mParent!=NULL) this->mParent->ChildInvalidated(this);
    this->MarkInvalid();
}
void Component::MarkInvalid()
{
    if (gRust) { K(EeWonCompCall::MarkInvalid); return; }
    this->mInvalidState=2;
    if(!this->CheckGrabBG())
        return;
    this->mExtraStuff->mGrabBG->Invalidate(this);
    RootContainer* root=this->GetRootContainer();
    if(root!=NULL) root->AddBG(this->mExtraStuff->mGrabBG);
}
void Component::Virtual08()
{
    if (gRust) { K(EeWonCompCall::SizeChanged); return; }
    if(this->CheckGrabBG()) this->mExtraStuff->mGrabBG->SizeChanged(this);
}
void Component::PrePaint(Graphics& graphics)
{
    if (gRust) { K(EeWonCompCall::PrePaint, (intptr_t)&graphics); return; }
    if(this->CheckGrabBG()) this->mExtraStuff->mGrabBG->PrePaint(this,graphics);
}
void Component::Paint(Graphics& graphics)
{
    if (gRust) { K(EeWonCompCall::Paint, (intptr_t)&graphics); return; }
    this->mInvalidState=0;
    if(this->CheckGrabBG()) this->mExtraStuff->mGrabBG->Paint(this,graphics);
}
void Component::ClearGrabBG()
{
    if (gRust) { K(EeWonCompCall::ClearGrabBG); return; }
    if(this->CheckGrabBG()) this->mExtraStuff->mGrabBG->Clear();
}
// ComponentResourceAdapters.cpp
BackgroundGrabber* Component::GetGrabBG()
{
    if (gRust) return (BackgroundGrabber*)K(EeWonCompCall::GrabBG);
    return this->mExtraStuff!=NULL ? this->mExtraStuff->mGrabBG : NULL;
}
Cursor* Component::GetDefaultCursor()
{
    if (gRust) return (Cursor*)K(EeWonCompCall::DefaultCursor);
    return this->mExtraStuff!=NULL ? this->mExtraStuff->mDefaultCursor.get() : NULL;
}
void Component::AddListener(ComponentListener* listener)
{
    if (gRust) { K(EeWonCompCall::AddListener, (intptr_t)listener); return; }
    this->AllocateExtraStuff();
    this->mExtraStuff->mListener=listener;
}
void Component::SetDefaultCursor(Cursor* cursor)
{
    if (gRust) { K(EeWonCompCall::SetDefaultCursor, (intptr_t)cursor); return; }
    this->AllocateExtraStuff();
    this->mExtraStuff->mDefaultCursor=cursor;
}
void Component::SubscribeToBroadcast(bool subscribe)
{
    if (gRust) { K(EeWonCompCall::Subscribe, subscribe); return; }
    this->GetRoot()->GetWindowManager()->SubscribeToBroadcast(this,subscribe);
}
void Component::GetMousePos(int& x,int& y)
{
    if (gRust) { K(EeWonCompCall::MousePos, (intptr_t)&x, (intptr_t)&y); return; }
    Window* window=this->GetRoot();
    x=window->GetMouseX();
    y=window->GetMouseY();
    this->RootToChild(x,y);
}
int Component::GetKeyMod()
{
    if (gRust) return (int)K(EeWonCompCall::KeyMod);
    return this->GetRoot()->GetKeyMod();
}
ColorScheme* Component::GetOwnColorScheme()
{
    if (gRust) return (ColorScheme*)K(EeWonCompCall::OwnColorScheme);
    return this->mExtraStuff!=NULL ? this->mExtraStuff->mColorScheme.get() : NULL;
}
ColorScheme* Component::GetColorScheme()
{
    if (gRust) return (ColorScheme*)K(EeWonCompCall::ColorScheme);
    if(this->mExtraStuff!=NULL && this->mExtraStuff->mColorScheme.get()!=NULL)
        return this->mExtraStuff->mColorScheme.get();
    if(this->mParent!=NULL) return this->mParent->GetColorScheme();
    Window* w=this->GetRoot();
    return w->GetWindowManager()->mColorScheme.get();
}
void Component::SetColorScheme(ColorScheme* scheme)
{
    if (gRust) { K(EeWonCompCall::SetColorScheme, (intptr_t)scheme); return; }
    this->AllocateExtraStuff();
    this->mExtraStuff->mColorScheme=scheme;
}
// ComponentDefaultHooks.cpp
bool Component::HitTest(int x,int y)
{
    if (gRust) return K(EeWonCompCall::HitTest, x, y) != 0;
    return this->IsVisible() && !this->ComponentFlagSet(ComponentFlag_NoInput);
}
void Component::SetCursor(Cursor* cursor)
{
    if (gRust) { K(EeWonCompCall::SetCursor, (intptr_t)cursor); return; }
    if(this->mParent!=NULL) this->mParent->SetCursor(cursor);
}
void Component::HandleBroadcastEvent(ComponentEvent* event)
{
    if (gRust) { K(EeWonCompCall::HandleBroadcast, (intptr_t)event); return; }
    this->HandleComponentEvent(event);
}
void Component::FireBroadcastEvent(ComponentEvent* event,bool sendNow)
{
    if (gRust) { K(EeWonCompCall::FireBroadcast, (intptr_t)event, sendNow); return; }
    event->mBroadcast=true;
    this->DeliverEvent(event,sendNow);
}
void Component::EnsureOnTopOf(Component* sibling)
{
    if (gRust) { K(EeWonCompCall::EnsureOnTopOf, (intptr_t)sibling); return; }
    if(this->mParent!=NULL) this->mParent->EnsureOnTopOf(sibling,this);
}
void Component::SetTranslucentColor(int color,int level)
{
    if (gRust) { K(EeWonCompCall::SetTranslucentColor, color, level); return; }
    this->SetComponentFlags(ComponentFlag_Translucent,true);
    if(!this->CheckGrabBG()) return;
    this->mExtraStuff->mGrabBG->SetTranslucentColor(color,level);
}
// ComponentApiCompletion.cpp
void Component::GetDesiredSize(int& width,int& height)
{
    if (gRust) { K(EeWonCompCall::DesiredSize, (intptr_t)&width, (intptr_t)&height); return; }
    width=20; height=20;
}
void Component::SetDesiredHeight(int padding)
{
    if (gRust) { K(EeWonCompCall::SetDesiredHeight, padding); return; }
    int width=0,height=0;
    this->GetDesiredSize(width,height);
    this->SetSize(this->Width(),height+padding);
}
void Component::FireEvent(int type,bool sendNow)
{
    if (gRust) { K(EeWonCompCall::FireEvent, type, sendNow); return; }
    this->DeliverEvent(new ComponentEvent(this,type),sendNow);
}
void Component::HandleComponentEvent(ComponentEvent* event)
{
    if (gRust) { K(EeWonCompCall::HandleEvent, (intptr_t)event); return; }
    this->BubbleEvent(event);
}
bool Component::CheckDoubleClick(int button)
{
    if (gRust) return K(EeWonCompCall::DoubleClick, button) != 0;
    Window* aWindow=this->GetRoot();
    return aWindow->CheckDoubleClick(static_cast<MouseButton>(button));
}
// MSControlConstructors.cpp
void Component::SetComponentFlags(int flags,bool on)
{
    if (gRust) { K(EeWonCompCall::SetFlags, flags, on); return; }
    if(on) this->mComponentBits|=flags;
    else this->mComponentBits&=~flags;
}

// ---- the port's host (ComponentLifetime.cpp's, over the stand-ins)
extern "C" intptr_t ee_won_guicpp_comp_call(void*, EeWonGuiOp op, intptr_t k, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonComp KK;
    typedef EeWonCompExtra X;
    Component* t = (Component*)b;
    Container* p = static_cast<Container*>(t);
    Window* w = (Window*)b;
    ComponentEvent* e = (ComponentEvent*)b;
    BackgroundGrabber* g = (BackgroundGrabber*)b;
    if (op != EeWonGuiOp::Comp)
        return 0;
    switch ((KK)k) {
    case KK::AllocExtra: t->AllocateExtraStuff(); return 0;
    case KK::Extra: case KK::SetExtra: {
        ComponentExtra* x = t->mExtraStuff;
        bool set = (KK)k == KK::SetExtra;
        switch ((X)c) {
        case X::Listener: if (set) x->mListener = (ComponentListener*)d; return (intptr_t)x->mListener.get();
        case X::Cursor: if (set) x->mDefaultCursor = (Cursor*)d; return (intptr_t)x->mDefaultCursor.get();
        case X::Scheme: if (set) x->mColorScheme = (ColorScheme*)d; return (intptr_t)x->mColorScheme.get();
        case X::GrabBG: if (set) x->mGrabBG = new BackgroundGrabber; return (intptr_t)x->mGrabBG;
        case X::ToolTip: return (intptr_t)x->mToolTip.get();
        case X::DontDoTip: if (set) x->mDontDoTip = d != 0; return x->mDontDoTip;
        default: return 0;
        }
    }
    case KK::VPositionChanged: t->PositionChanged(); return 0;
    case KK::VSizeChanged: t->Virtual08(); return 0;
    case KK::VDesiredSize: t->GetDesiredSize(((int*)c)[0], ((int*)c)[1]); return 0;
    case KK::VHandleEvent: t->HandleComponentEvent((ComponentEvent*)c); return 0;
    case KK::VHandleBroadcast: t->HandleBroadcastEvent((ComponentEvent*)c); return 0;
    case KK::VInvalidateRect: t->InvalidateRect(*(WONRectangle*)c); return 0;
    case KK::VInvalidate: t->Invalidate(); return 0;
    case KK::VMarkInvalid: t->MarkInvalid(); return 0;
    case KK::VGotFocus: t->GotFocus(); return 0;
    case KK::VLostFocus: t->LostFocus(); return 0;
    case KK::VNotifyRootVisibility: t->NotifyRootVisibility(c != 0); return 0;
    case KK::VSetEnabled: t->SetEnabled(c != 0); return 0;
    case KK::VEnabledChanged: t->EnabledChanged(c != 0); return 0;
    case KK::VSetCursor: t->SetCursor((Cursor*)c); return 0;
    case KK::PNextChild: return (intptr_t)p->NextChild((Component*)c, (d & 1) != 0, (d & 2) != 0);
    case KK::PSetFocusChild: p->SetFocusChild((Component*)c); return 0;
    case KK::PAddTimerChild: p->AddTimerChild((Component*)c); return 0;
    case KK::PChildStateChanged: p->ChildStateChanged((Component*)c); return 0;
    case KK::PChildInvalidated: p->ChildInvalidated((Component*)c); return 0;
    case KK::PIsModal: return p->IsModal();
    case KK::PEnsureOnTopOf: p->EnsureOnTopOf((Component*)c, (Component*)d); return 0;
    case KK::AsRoot: return (intptr_t)dynamic_cast<RootContainer*>(t);
    case KK::RootWin: return (intptr_t)((RootContainer*)b)->mWindow;
    case KK::RootAddBG: ((RootContainer*)b)->AddBG((BackgroundGrabber*)c); return 0;
    case KK::DefaultWindow: return (intptr_t)Window::GetDefaultWindow();
    case KK::WState: return w->GetWindowState();
    case KK::WBroadcast: w->mManager->BroadcastEvent((ComponentEvent*)c); return 0;
    case KK::WQueue: w->QueueEvent((ComponentEvent*)c); return 0;
    case KK::WParent: return (intptr_t)w->GetParent();
    case KK::WRoot: return (intptr_t)w->mRootComp;
    case KK::WMouse: ((int*)c)[0] = w->GetMouseX(); ((int*)c)[1] = w->GetMouseY(); return 0;
    case KK::WOpenTip: {
        const intptr_t* a = (const intptr_t*)c;
        PopupParams params((Component*)a[0], PopupFlag_StandardTip, (Component*)a[1], (int)a[2], (int)a[3]);
        w->OpenPopup(params);
        return 0;
    }
    case KK::WClosePopup: w->ClosePopup((Component*)c); return 0;
    case KK::WKeyMod: return w->GetKeyMod();
    case KK::WDoubleClick: return w->CheckDoubleClick(static_cast<MouseButton>(c));
    case KK::WSubscribe: w->GetWindowManager()->SubscribeToBroadcast((Component*)c, d != 0); return 0;
    case KK::WScheme: return (intptr_t)w->GetWindowManager()->mColorScheme.get();
    case KK::WFontMod: {
        FontDescriptor descriptor = ((Font*)c)->mDescriptor;
        descriptor.mStyle = ((const int*)d)[0];
        if (((const int*)d)[1] != 0) descriptor.mSize = ((const int*)d)[1];
        return (intptr_t)w->GetFont(descriptor).get();
    }
    case KK::EvHold: e->AddRef(); return 0;
    case KK::EvRelease: e->Release(); return 0;
    case KK::EvBroadcast: return e->mBroadcast;
    case KK::EvSetBroadcast: e->mBroadcast = c != 0; return 0;
    case KK::EvComp: return (intptr_t)e->mComponent;
    case KK::EvSetComp: e->SetComponent((Component*)c); return 0;
    case KK::EvNew: return (intptr_t)new ComponentEvent(t, (int)c);
    case KK::LHandle: ((ComponentListener*)b)->HandleComponentEvent((ComponentEvent*)c); return 0;
    case KK::GSizeChanged: g->SizeChanged((Component*)c); return 0;
    case KK::GPrePaint: g->PrePaint((Component*)c, *(Graphics*)d); return 0;
    case KK::GPaint: g->Paint((Component*)c, *(Graphics*)d); return 0;
    case KK::GClear: g->Clear(); return 0;
    case KK::GInvalidate: g->Invalidate((Component*)c); return 0;
    case KK::GNeedDrawBG: return g->NeedDrawBG();
    case KK::GTranslucent: g->SetTranslucentColor((int)c, (int)d); return 0;
    case KK::IdIn: return ((const ControlIdSet*)b)->find((long)c) != ((const ControlIdSet*)b)->end();
    default: return 0;
    }
}

// ---- the world
struct World {
    std::vector<Component*> comps;
    ComponentListener listeners[3];
    Cursor cursors[3];
    ColorScheme schemes[4];
    Font fonts[2];
    Window* win[4];
    WindowManager wm[4];
    Graphics g;
    ControlIdSet ids;
    std::vector<ComponentEvent*> events; // each held by the world once
    std::set<void*> live;
    int nextEv, nextGrab;
    std::string desc;
    World() : nextEv(0), nextGrab(0) {}
};
Component* Container::NextChild(Component* c, bool f, bool w)
{
    T("%d nextchild %d %d %d", mId, Id(c), f, w);
    int n = (int)gW->comps.size();
    int i = Id(c) - 1 + (f ? 1 : n - 1);
    if (!w && (i >= n && f)) return NULL;
    return gW->comps[i % n];
}
static int* NextEv() { return &gW->nextEv; }
static int* NextGrab() { return &gW->nextGrab; }
static void LiveEv(void* e, bool on) { if (on) gW->live.insert(e); else gW->live.erase(e); }

static void Line(std::string& s, const char* f, ...)
{
    char a[400];
    va_list v;
    va_start(v, f);
    vsnprintf(a, sizeof(a), f, v);
    va_end(v);
    s += a;
    s += "\n";
}

// a random world; its description is the golden's W lines
static void MakeWorld(World& w, unsigned seed)
{
    gW = &w;
    sSeed = seed;
    int n = RI(3, 7);
    bool root = R(4) != 0;
    std::string& s = w.desc;
    for (int i = 0; i < 3; i++) { w.listeners[i].mId = 3001 + i; w.listeners[i].AddRef(); w.cursors[i].mId = 4001 + i; w.cursors[i].AddRef(); }
    for (int i = 0; i < 4; i++) { w.schemes[i].mId = 5001 + i; w.schemes[i].AddRef(); }
    for (int i = 0; i < 2; i++) { w.fonts[i].mId = 7901 + i; w.fonts[i].mDescriptor.mStyle = RI(0, 3); w.fonts[i].mDescriptor.mSize = RI(6, 14); w.fonts[i].AddRef(); }
    w.g.mId = 8001;
    sSeed = seed * 7 + 1;
    Line(s, "W %d %d %d %d %d %d", n, root, w.fonts[0].mDescriptor.mStyle, w.fonts[0].mDescriptor.mSize,
         w.fonts[1].mDescriptor.mStyle, w.fonts[1].mDescriptor.mSize);
    for (int i = 0; i < n; i++) {
        Container* c = (i == 0 && root) ? (Container*)new TR : (Container*)new TC;
        c->mId = i + 1;
        c->AddRef();
        int par = i == 0 ? -1 : (R(6) == 0 ? -1 : (int)R((unsigned)i));
        c->mParent = par < 0 ? NULL : static_cast<Container*>(w.comps[par]);
        c->mBounds = WONRectangle(RI(-5, 40), RI(-5, 40), R(8) == 0 ? RI(-3, 0) : RI(0, 60), RI(0, 60));
        static const int kBits[] = {0x1, 0x8, 0x10, 0x80, 0x100, 0x200, 0x800, 0x4000, 0x40000};
        int bits = 0;
        for (int b = 0; b < 9; b++) if (R(4) == 0) bits |= kBits[b];
        c->mComponentBits = bits;
        c->mControlId = R(3) == 0 ? 0 : RI(1, 6);
        c->mInvalidState = (char)RI(0, 2);
        c->mTimerFlags = (char)(R(3) == 0 ? RI(0, 3) : 0);
        c->mModal = R(4) == 0;
        int ex = R(3) != 0;
        int el = 0, ec = 0, es = 0, eg = 0, ed = 0;
        if (ex) {
            c->mExtraStuff = new ComponentExtra;
            if (R(2)) { el = (int)R(3) + 1; c->mExtraStuff->mListener = &w.listeners[el - 1]; }
            if (R(2)) { ec = (int)R(3) + 1; c->mExtraStuff->mDefaultCursor = &w.cursors[ec - 1]; }
            if (R(2)) { es = (int)R(3) + 1; c->mExtraStuff->mColorScheme = &w.schemes[es - 1]; }
            if (R(2)) { c->mExtraStuff->mGrabBG = new BackgroundGrabber; eg = c->mExtraStuff->mGrabBG->mId; }
            ed = R(2);
            c->mExtraStuff->mDontDoTip = ed != 0;
        }
        w.comps.push_back(c);
        Line(s, "C %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", par, c->mBounds.x, c->mBounds.y, c->mBounds.width,
             c->mBounds.height, bits, c->mControlId, c->mInvalidState, c->mTimerFlags, c->mModal, ex, el, ec, es, eg, ed);
    }
    // the tool tips (a component of the world)
    for (int i = 0; i < n; i++) {
        Component* c = w.comps[i];
        if (c->mExtraStuff != NULL && R(2)) {
            int tip = (int)R((unsigned)n);
            c->mExtraStuff->mToolTip = w.comps[tip];
            Line(s, "P %d %d", i, tip);
        }
    }
    for (int k = 1; k <= 3; k++) {
        Window* x = new Window;
        x->mId = 1000 + k;
        x->AddRef();
        x->mState = k == 1 ? (R(5) == 0 ? 1 : 0) : 0;
        x->mMouseX = RI(-10, 100);
        x->mMouseY = RI(-10, 100);
        x->mKeyMod = RI(0, 7);
        x->mParentWin = NULL;
        x->mRootComp = NULL;
        w.wm[k].mWin = k;
        w.wm[k].mColorScheme = R(4) ? &w.schemes[3] : NULL;
        x->mManager = &w.wm[k];
        w.win[k] = x;
    }
    if (R(3)) w.win[1]->mParentWin = w.win[2];
    // window 2's root: a component outside window 1's tree (one inside would bubble an event round for ever, in the
    // original as in the port)
    std::vector<int> outside;
    for (int i = 0; i < n; i++) {
        Component* top = w.comps[i];
        while (top->mParent != NULL) top = top->mParent;
        if (!root || top != w.comps[0]) outside.push_back(i);
    }
    int r2 = R(3) && !outside.empty() ? outside[R((unsigned)outside.size())] : -1;
    if (r2 >= 0) w.win[2]->mRootComp = w.comps[r2];
    if (root) static_cast<RootContainer*>(w.comps[0])->mWindow = w.win[1];
    Window::sDefault = w.win[3];
    for (int i = 0; i < 4; i++) if (R(2)) w.ids.insert(RI(1, 6));
    std::string ids;
    for (ControlIdSet::iterator it = w.ids.begin(); it != w.ids.end(); ++it) { char a[16]; snprintf(a, 16, " %ld", *it); ids += a; }
    Line(s, "X %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d%s", w.win[1]->mState, w.win[1]->mMouseX, w.win[1]->mMouseY,
         w.win[1]->mKeyMod, w.win[2]->mMouseX, w.win[2]->mMouseY, w.win[2]->mKeyMod, w.win[3]->mMouseX, w.win[3]->mMouseY,
         w.win[3]->mKeyMod, w.win[1]->mParentWin != NULL, r2, w.wm[1].mColorScheme.get() != NULL,
         w.wm[2].mColorScheme.get() != NULL, w.wm[3].mColorScheme.get() != NULL, ids.c_str());
}

static void Dump(World& w, std::string& out)
{
    for (size_t i = 0; i < w.comps.size(); i++) {
        Component* c = w.comps[i];
        ComponentExtra* x = c->mExtraStuff;
        char a[300];
        snprintf(a, sizeof(a), "%d:%d,%d,%d,%d,%x,%d,%d,%d", (int)i + 1, c->mBounds.x, c->mBounds.y, c->mBounds.width,
                 c->mBounds.height, c->mComponentBits, c->mInvalidState, c->mTimerFlags,
                 x == NULL ? -1 : (Id(x->mListener.get()) + Id(x->mDefaultCursor.get()) * 7 + Id(x->mColorScheme.get()) * 13 +
                                   (x->mGrabBG ? x->mGrabBG->mId : 0) * 17 + Id(x->mToolTip.get()) * 19 + x->mDontDoTip));
        if (!out.empty()) out += " ";
        out += a;
    }
    for (size_t i = 0; i < w.events.size(); i++) {
        char a[64];
        ComponentEvent* e = w.events[i];
        snprintf(a, sizeof(a), " ev%d:%d,%d,%d", e->mId, e->mRefCount, e->mBroadcast, Id(e->mComponent));
        out += a;
    }
}

static std::string RetName(EeWonCompCall k, intptr_t r)
{
    char a[32];
    switch (k) {
    case EeWonCompCall::NextSibling: case EeWonCompCall::RootContainer:
    case EeWonCompCall::OwnColorScheme: case EeWonCompCall::ColorScheme: case EeWonCompCall::DefaultCursor:
    case EeWonCompCall::FontMod: snprintf(a, 32, "%d", Id((RefCount*)r)); break;
    case EeWonCompCall::Root: case EeWonCompCall::RootWindow: snprintf(a, 32, "%d", Id((Window*)r)); break;
    case EeWonCompCall::GrabBG: snprintf(a, 32, "%d", r ? ((BackgroundGrabber*)r)->mId : 0); break;
    default: snprintf(a, 32, "%ld", (long)r);
    }
    return a;
}

// one call on component c of the world (from the original's side or the Rust's: gRust)
static std::string Call(World& w, Component* c, EeWonCompCall k, int a, int b, int cc, int dd)
{
    intptr_t r = 0;
    char extra[64] = "";
    ComponentEvent* ev = NULL;
    typedef EeWonCompCall C;
    switch (k) {
    case C::FireBroadcast: case C::BroadcastDown: case C::Deliver: case C::Bubble: case C::HandleEvent:
    case C::HandleBroadcast: {
        if (a == 0 || w.events.empty()) {
            ev = new ComponentEvent((dd & 2) ? NULL : w.comps[(unsigned)b % w.comps.size()], 7);
            ev->mBroadcast = (cc & 1) != 0;
            ev->AddRef();
            w.events.push_back(ev);
        } else
            ev = w.events[(unsigned)a % w.events.size()];
        // the world lets go of its reference to an event now and then before the call (the call's own references
        // then decide its life)
        bool drop = (dd & 1) != 0;
        if (drop) {
            for (size_t i = 0; i < w.events.size(); i++) if (w.events[i] == ev) { w.events.erase(w.events.begin() + i); break; }
            ev->mRefCount--;
        }
        int keep = ev->mId;
        if (k == C::FireBroadcast) c->FireBroadcastEvent(ev, cc & 2);
        else if (k == C::BroadcastDown) c->BroadcastEventDown(ev);
        else if (k == C::Deliver) c->DeliverEvent(ev, cc & 2);
        else if (k == C::Bubble) c->BubbleEvent(ev);
        else if (k == C::HandleEvent) c->HandleComponentEvent(ev);
        else c->HandleBroadcastEvent(ev);
        snprintf(extra, sizeof(extra), " ev%d", keep);
        if (drop && w.live.count(ev) && ev->mRefCount <= 0) {
            // no reference left and none taken by the call: the world deletes it quietly
            gQuietDelete = true;
            delete ev;
            gQuietDelete = false;
            sTrace.push_back("(unheld)");
        }
        break;
    }
    case C::SizeChanged: c->Virtual08(); break;
    case C::DesiredSize: { int x = -1, y = -1; c->GetDesiredSize(x, y); snprintf(extra, 64, " %d %d", x, y); break; }
    case C::NeedsPaint: r = c->NeedsPaint(); break;
    case C::PrePaint: c->PrePaint(w.g); break;
    case C::Paint: c->Paint(w.g); break;
    case C::HitTest: r = c->HitTest(a, b); break;
    case C::MouseEnter: c->MouseEnter(a, b); break;
    case C::MouseExit: c->MouseExit(); break;
    case C::MouseHover: {
        static const unsigned long kE[] = {0, 499, 500, 501, 5499, 5500, 9000, 0xffffffffu};
        c->MouseHover(kE[(unsigned)a % 8]);
        break;
    }
    case C::GotFocus: c->GotFocus(); break;
    case C::LostFocus: c->LostFocus(); break;
    case C::InvalidateRect: { WONRectangle rr(a, b, cc, dd); c->InvalidateRect(rr); snprintf(extra, 64, " %d %d %d %d", rr.x, rr.y, rr.width, rr.height); break; }
    case C::Invalidate: c->Invalidate(); break;
    case C::MarkInvalid: c->MarkInvalid(); break;
    case C::AddedToParent: if (c->mParent != NULL) c->AddedToParent(); break;
    case C::ClearGrabBG: c->ClearGrabBG(); break;
    case C::SetCursor: c->SetCursor((unsigned)a % 4 ? &w.cursors[(unsigned)a % 4 - 1] : NULL); break;
    case C::NotifyRootVisibility: c->NotifyRootVisibility(a & 1); break;
    case C::TabFocus: r = c->TabFocus(a & 1, b & 1); break;
    case C::EnableControlsById: c->EnableControlsById(w.ids, a & 1); break;
    case C::SetEnabled: c->SetEnabled(a & 1); break;
    case C::SetVisible: c->SetVisible(a & 1); break;
    case C::SetColorScheme: c->SetColorScheme((unsigned)a % 4 ? &w.schemes[(unsigned)a % 4 - 1] : NULL); break;
    case C::OwnColorScheme: r = (intptr_t)c->GetOwnColorScheme(); break;
    case C::ColorScheme: r = (intptr_t)c->GetColorScheme(); break;
    case C::SetPos: c->SetPos(a, b); break;
    case C::SetSize: c->SetSize(a, b); break;
    case C::SetSizeAtLeast: c->SetSizeAtLeast(a, b); break;
    case C::SetPosSize: c->SetPosSize(a, b, cc, dd); break;
    case C::AddListener: c->AddListener((unsigned)a % 4 ? &w.listeners[(unsigned)a % 4 - 1] : NULL); break;
    case C::SetDesiredSize: c->SetDesiredSize(a, b); break;
    case C::FireEvent: c->FireEvent(a, b & 1); break;
    case C::IsRootVisible: r = c->IsRootVisible(); break;
    case C::LocalToRoot: case C::RootToChild: case C::MousePos: {
        int x = a, y = b;
        if (k == C::LocalToRoot) c->LocalToRoot(x, y);
        else if (k == C::RootToChild) c->RootToChild(x, y);
        else c->GetMousePos(x, y);
        snprintf(extra, 64, " %d %d", x, y);
        break;
    }
    case C::NextSibling: r = (intptr_t)c->GetNextSibling(a & 1, b & 1); break;
    case C::WantFocus: r = c->WantFocus(); break;
    case C::KeyMod: r = c->GetKeyMod(); break;
    case C::DoubleClick: r = c->CheckDoubleClick(a & 3); break;
    case C::Contains: r = c->Contains(a, b); break;
    case C::Intersects: r = c->Intersects(w.comps[(unsigned)a % w.comps.size()]); break;
    case C::DefaultCursor: r = (intptr_t)c->GetDefaultCursor(); break;
    case C::GrabBG: r = (intptr_t)c->GetGrabBG(); break;
    case C::CheckGrabBG: r = c->CheckGrabBG(); break;
    case C::EnsureOnTopOf: c->EnsureOnTopOf(w.comps[(unsigned)a % w.comps.size()]); break;
    case C::SetDefaultCursor: c->SetDefaultCursor((unsigned)a % 4 ? &w.cursors[(unsigned)a % 4 - 1] : NULL); break;
    case C::SetDesiredHeight: c->SetDesiredHeight(a); break;
    case C::SetDesiredWidth: c->SetDesiredWidth(a); break;
    case C::SetTranslucentColor: c->SetTranslucentColor(a, b); break;
    case C::SetFlags: c->SetComponentFlags(a, b & 1); break;
    case C::Subscribe: c->SubscribeToBroadcast(a & 1); break;
    case C::RequestTimer: c->RequestTimer(a & 1); break;
    case C::RequestFocus: c->RequestFocus(); break;
    case C::Root: r = (intptr_t)c->GetRoot(); break;
    case C::RootContainer: r = (intptr_t)c->GetRootContainer(); break;
    case C::RootWindow: r = (intptr_t)c->GetRootWindow(); break;
    case C::FontMod: r = (intptr_t)c->GetFontMod(&w.fonts[a & 1], b, cc); break;
    default: break;
    }
    return "ret " + RetName(k, r) + extra;
}

static void FreeWorld(World& w)
{
    for (size_t i = 0; i < w.comps.size(); i++) {
        w.comps[i]->mParent = NULL;
        if (w.comps[i]->mExtraStuff) w.comps[i]->mExtraStuff->mToolTip = NULL;
    }
    // leaked on purpose: the stand-ins' references form cycles (tool tips); each world is small
}

int main(int argc, char** argv)
{
    bool print = argc > 1 && strcmp(argv[1], "--print") == 0;
    int sequences = print ? 300 : 20000;
    long checks = 0, failures = 0;
    for (int s = 0; s < sequences; s++) {
        World w[2];
        for (int side = 0; side < 2; side++) MakeWorld(w[side], 1000u + (unsigned)s);
        if (print) printf("S %d\n%s", s, w[0].desc.c_str());
        sSeed = 99991u * (unsigned)s + 17;
        for (int o = 0; o < 40; o++) {
            EeWonCompCall k = (EeWonCompCall)RI(1, (int)EeWonCompCall::FontMod);
            int ci = (int)R((unsigned)w[0].comps.size());
            int a = R(4) ? RI(-3, 8) : RI(-100, 100), b = R(4) ? RI(-3, 8) : RI(-100, 100);
            int cc = RI(-5, 70), dd = RI(-5, 70);
            if (k == EeWonCompCall::SetFlags) a = (int)(R(0x8000) | (R(2) ? 0x40000 : 0));
            if (k == EeWonCompCall::SetTranslucentColor) a = (int)(Rand() << 16 | Rand());
            unsigned save = sSeed;
            std::string res[2], st[2];
            std::vector<std::string> tr[2];
            for (int side = 0; side < 2; side++) {
                gRust = side == 1;
                gW = &w[side];
                Window::sDefault = w[side].win[3];
                sTrace.clear();
                sSeed = save;
                res[side] = Call(w[side], w[side].comps[(size_t)ci], k, a, b, cc, dd);
                tr[side] = sTrace;
                Dump(w[side], st[side]);
            }
            gRust = false;
            ++checks;
            if (res[0] != res[1] || tr[0] != tr[1] || st[0] != st[1]) {
                if (++failures <= 10) {
                    printf("MISMATCH seq %d op %d call %d comp %d args %d %d %d %d\n", s, o, (int)k, ci + 1, a, b, cc, dd);
                    for (int side = 0; side < 2; side++) {
                        printf("  %s: %s |", side ? "rust" : "orig", res[side].c_str());
                        for (size_t i = 0; i < tr[side].size(); i++) printf(" %s;", tr[side][i].c_str());
                        printf("\n    %s\n", st[side].c_str());
                    }
                }
            }
            if (print) {
                printf("O %d %d %d %d %d %d\nR %s\n", (int)k, ci, a, b, cc, dd, res[1].c_str());
                for (size_t i = 0; i < tr[1].size(); i++) printf("T %s\n", tr[1][i].c_str());
                printf("D %s\n", st[1].c_str());
            }
        }
        FreeWorld(w[0]);
        FreeWorld(w[1]);
    }
    if (!print) printf("won_compobj_test: %d sequences, %ld checks, %ld failures\n", sequences, checks, failures);
    return failures ? 1 : 0;
}
