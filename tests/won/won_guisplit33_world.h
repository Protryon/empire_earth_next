// The stand-in world of won_guisplit33_test.cpp: the WONGUI classes and functions that the GUI hosts' cases R6 WON
// round 33 composed in Rust (guisplit.rs) touch, every call logged with its arguments, the members the cases read
// back kept. The original cases (won_guisplit33_orig.h) and the new one-statement cases (won_guisplit33_new.h) are
// compiled over it, so the two sides make their calls on objects made alike.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

namespace GsWorld {
static std::vector<std::string> sTrace;
static void Log(const std::string& s) { sTrace.push_back(s); }
static std::string N(long long v) { return std::to_string(v); }

// every object the world made, numbered in order of first mention (the traces name objects, not addresses)
static std::vector<const void*> sObjects;
static std::string Id(const void* p)
{
    if (p == NULL) return "null";
    for (size_t i = 0; i < sObjects.size(); i++)
        if (sObjects[i] == p) return "#" + N((long long)i);
    sObjects.push_back(p);
    return "#" + N((long long)sObjects.size() - 1);
}

// ---- references
struct RefCount {
    int mRefs = 1;
    virtual ~RefCount() {}
    // the counts, not the calls, are compared (a host's static SmartPtr takes and gives back its reference at
    // other moments than the original's local, with the same counts after every case)
    void AddRef() { mRefs++; }
    void Release() { mRefs--; }
};
template <class T> struct SmartPtr {
    T* mPtr = NULL;
    SmartPtr() {}
    SmartPtr(T* p) : mPtr(p) { if (p) p->AddRef(); }
    SmartPtr(const SmartPtr& o) : mPtr(o.mPtr) { if (mPtr) mPtr->AddRef(); }
    ~SmartPtr() { if (mPtr) mPtr->Release(); }
    SmartPtr& operator=(const SmartPtr& o)
    {
        T* old = mPtr;
        mPtr = o.mPtr;
        if (mPtr) mPtr->AddRef();
        if (old) old->Release();
        return *this;
    }
    SmartPtr& operator=(T* p)
    {
        SmartPtr keep(p);
        if (p) p->mRefs--; // `new` objects start at one reference; the pointer takes it
        return *this = keep;
    }
    T* get() const { return mPtr; }
    T* operator->() const { return mPtr; }
};

// ---- texts and the clipboard
struct GUIStringData {
    const wchar_t* mText;
};
struct GUIString {
    std::wstring w;
    GUIStringData d;
    GUIStringData* mData;
    GUIString() : mData(&d) { d.mText = w.c_str(); }
    GUIString(const std::wstring& s) : w(s), mData(&d) { d.mText = w.c_str(); }
    GUIString(const GUIString& o) : w(o.w), mData(&d) { d.mText = w.c_str(); }
    GUIString& operator=(const GUIString& o)
    {
        w = o.w;
        d.mText = w.c_str();
        return *this;
    }
    unsigned short length() const { return (unsigned short)w.size(); }
};
static std::wstring sClipboard;
struct Clipboard {
    static GUIString Get() { Log("Clipboard::Get"); return GUIString(sClipboard); }
};
static std::string Narrow(const wchar_t* t, size_t n)
{
    std::string s;
    for (size_t i = 0; i < n; i++) s += (char)t[i];
    return s;
}
// the Rust's text sink (a stand-in, a better match than the real one: the sink is a number here)
static void ee_won_gui_sink_units(void* inSink, const wchar_t* inUnits, size_t inLength, size_t inUnit)
{
    Log("sink " + N((intptr_t)inSink) + " '" + Narrow(inUnits, inLength) + "' " + N((long long)inUnit));
}

// ---- components, windows, events
struct Font : RefCount {
    int mSize = 8;
};
struct FontDescriptor {
    std::string mFace;
    int mStyle, mSize;
    FontDescriptor(const std::string& f, int s, int z) : mFace(f), mStyle(s), mSize(z) {}
};
static Font* sFont; // the window's font (NULL: none)
struct WONRectangle {
    int x, y, width, height;
    WONRectangle() : x(0), y(0), width(0), height(0) {}
    WONRectangle(int a, int b, int c, int d) : x(a), y(b), width(c), height(d) {}
};
static std::string R(const WONRectangle& r) { return N(r.x) + "," + N(r.y) + "," + N(r.width) + "," + N(r.height); }
struct WindowEventBase : RefCount {
    std::string mText;
};
struct Window;
static Window* CreateNativeWindow();
struct Window : RefCount {
    int mMouseX = 0, mMouseY = 0, mKeyMod = 0;
    int GetMouseX() { Log("GetMouseX " + Id(this)); return mMouseX; }
    int GetMouseY() { Log("GetMouseY " + Id(this)); return mMouseY; }
    int GetKeyMod() { Log("GetKeyMod " + Id(this)); return mKeyMod; }
    void QueueEvent(WindowEventBase* e) { Log("QueueEvent " + Id(this) + " " + e->mText); delete e; }
    void SetOneComponentWindow(bool b) { Log("SetOneComponentWindow " + Id(this) + " " + N(b)); }
    void SetStyle(int s) { Log("SetStyle " + Id(this) + " " + N(s)); }
    void SetParent(Window* p) { Log("SetParent " + Id(this) + " " + Id(p)); }
    void SetIsPopup(bool b) { Log("SetIsPopup " + Id(this) + " " + N(b)); }
    void Create(const WONRectangle& r) { Log("Create " + Id(this) + " " + R(r)); }
    SmartPtr<Font> GetFont(const FontDescriptor& d)
    {
        Log("GetFont " + Id(this) + " " + d.mFace + " " + N(d.mStyle) + " " + N(d.mSize));
        return SmartPtr<Font>(sFont);
    }
};
static Window* sNewWindow;
static Window* CreateNativeWindow() { Log("CreateNativeWindow"); return sNewWindow; }
static const int InputEvent_MouseUp = 4;
struct MouseEvent : WindowEventBase {
    MouseEvent(int t, int x, int y, int m, int b) { mText = "mouse " + N(t) + " " + N(x) + " " + N(y) + " " + N(m) + " " + N(b); }
};
// (the game's input events carry winobj.rs's record: the original's fields are its)
struct SizeEvent : WindowEventBase {
    EeWonWinEvent mEv = EeWonWinEvent();
    int& mx = mEv.x;
    int& my = mEv.y;
    int& mWidth = mEv.width;
    int& mHeight = mEv.height;
};
struct Component : RefCount {
    Window* mRoot = NULL;
    struct Container* mParent = NULL;
    Window* GetRoot() { Log("GetRoot " + Id(this)); return mRoot; }
};
struct Container : Component {
    int mDx = 0, mDy = 0;
    void RootToChild(int& x, int& y) { Log("RootToChild " + Id(this) + " " + N(x) + " " + N(y)); x -= mDx; y -= mDy; }
};
struct ComponentEvent : RefCount {
    std::string mText;
};
struct TextSegment : RefCount {
    int mParam = 0;
    int GetLinkParam() { Log("GetLinkParam " + Id(this)); return mParam; }
};
struct TextLinkActivatedEvent : ComponentEvent {
    TextLinkActivatedEvent(Component* c, TextSegment* s) { mText = "link " + Id(c) + " " + Id(s); }
};
struct TextArea : Component {
    int mLinkParam = -1;
    void DeliverEvent(ComponentEvent* e, bool b) { Log("DeliverEvent " + Id(this) + " " + e->mText + " " + N(b)); delete e; }
};
struct ListItem : RefCount {};
struct ListArea;
struct ListAreaToolTip : RefCount {
    ListAreaToolTip(ListArea* a) { Log("new ListAreaToolTip " + Id(a)); }
    void DoTip(ListItem* i) { Log("DoTip " + Id(this) + " " + Id(i)); }
};
struct PopupMenu : Component {};
struct PopupParams {
    Window* mParent;
    Component* mPopup;
    Component* mInputComponent;
    Component* mOwnerComponent;
    int mFlags, mx, my;
    WONRectangle mAvoidRect;
    PopupParams(Component* p, int f, Component* o, int x = -1, int y = -1)
        : mParent(NULL), mPopup(p), mInputComponent(NULL), mOwnerComponent(o), mFlags(f), mx(x), my(y)
    {
        Log("PopupParams " + Id(p) + " " + N(f) + " " + Id(o) + " " + N(x) + " " + N(y));
    }
};
struct DoPopupEvent : WindowEventBase {
    DoPopupEvent(const PopupParams& p)
    {
        mText = "popup " + Id(p.mPopup) + " " + N(p.mFlags) + " " + Id(p.mOwnerComponent) + " " + N(p.mx) + " " + N(p.my) + " " +
                R(p.mAvoidRect);
    }
};
struct ListArea : Component {
    SmartPtr<ListAreaToolTip> mItemToolTip;
    int mLocalDx = 0, mLocalDy = 0;
    bool mContains = false;
    bool Contains(int x, int y) { Log("Contains " + Id(this) + " " + N(x) + " " + N(y)); return mContains; }
    void LocalToRoot(int& x, int& y) { Log("LocalToRoot " + Id(this) + " " + N(x) + " " + N(y)); x += mLocalDx; y += mLocalDy; }
};
struct WindowManager : RefCount {
    Window* mDefault = NULL;
    bool mSetOk = false;
    Window* GetDefaultWindow() { Log("GetDefaultWindow"); return mDefault; }
    void GetScreenResolutionHook(int& w, int& h) { Log("GetScreenResolutionHook"); w = 1024; h = 768; }
    bool SetScreenResolutionHook(int w, int h) { Log("SetScreenResolutionHook " + N(w) + " " + N(h)); return mSetOk; }
};
}
