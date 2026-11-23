// The stand-in world of won_leaf34_test.cpp: the WONGUI classes BrowserComponent's and ListAreaToolTip's statements
// touch (the components, the text view, the scrollers, the list, its items, the window's popups, the Graphics),
// every call logged with its arguments, the members the statements read back kept. The texts are the real GUIString
// (ee/wonapi/WONLobbyResource.h, over ee_won_gui's guistrobj.rs) and the references the real RefCount and SmartPtr, so
// both sides make and give back the same references. The original classes (won_leaf34_orig.h) and the new
// one-statement host cases (won_leaf34_new.h) are compiled over it.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

namespace Lf {
using WONAPI::GUIString;
using WONAPI::GUIStringData;
using WONAPI::RefCount;
using WONAPI::SmartPtr;
using WONAPI::URL;
using WONAPI::URLType_File;
using WONAPI::URLType_FTP;
using WONAPI::URLType_HTTP;
using WONAPI::WonWideSink;

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
// a text as its units (non-ASCII as \u{..}), its length first
static std::string Text(const GUIString& s)
{
    std::string r = "[" + N(s.length()) + "]";
    for (int i = 0; i < s.length(); i++) {
        unsigned c = (unsigned)s.at(i);
        if (c >= 32 && c < 127) r += (char)c;
        else r += "\\u{" + N(c) + "}";
    }
    return r;
}

// ---- constants the statements name
enum { TextFlag_ScrollOnBottom = 0x10, ComponentFlag_GrabBG = 0x4000 };
enum ScrollbarCondition { ScrollbarCondition_Disable, ScrollbarCondition_Enable, ScrollbarCondition_Conditional };
enum HTMLFontSize { HTMLFontSize_Smallest, HTMLFontSize_Small, HTMLFontSize_Medium, HTMLFontSize_Large, HTMLFontSize_Largest };
enum StandardColor { StandardColor_ToolTipBack = 9, StandardColor_ToolTipText = 10 };

// ---- drawing
struct ColorScheme {
    int mId;
};
struct Graphics {
    ColorScheme* mColorScheme = NULL;
    void SetSysColor(int c) { Log("SetSysColor " + N(c) + " scheme " + Id(mColorScheme)); }
    void FillRect(int x, int y, int w, int h) { Log("FillRect " + N(x) + " " + N(y) + " " + N(w) + " " + N(h)); }
    void DrawRect(int x, int y, int w, int h) { Log("DrawRect " + N(x) + " " + N(y) + " " + N(w) + " " + N(h)); }
    void PushClipRect(int x, int y, int w, int h) { Log("PushClipRect " + N(x) + " " + N(y) + " " + N(w) + " " + N(h)); }
    void PopClipRect() { Log("PopClipRect"); }
    void Translate(int x, int y) { Log("Translate " + N(x) + " " + N(y)); }
};
// WONGUIDraw.h's run of one operation (the kinds a tip draws; the others are logged as unknown)
inline void WONDrawPlain(Graphics& g, const EeWonDrawOp& o)
{
    switch (o.kind) {
    case EE_WON_DRAW_SET_SYS_COLOR: g.SetSysColor(o.a); break;
    case EE_WON_DRAW_FILL_RECT: g.FillRect(o.a, o.b, o.c, o.d); break;
    case EE_WON_DRAW_RECT: g.DrawRect(o.a, o.b, o.c, o.d); break;
    case EE_WON_DRAW_PUSH_CLIP: g.PushClipRect(o.a, o.b, o.c, o.d); break;
    case EE_WON_DRAW_POP_CLIP: g.PopClipRect(); break;
    case EE_WON_DRAW_TRANSLATE: g.Translate(o.a, o.b); break;
    default: Log("draw kind " + N(o.kind)); break;
    }
}

// ---- components
struct Background {
    int mColor, mBorderColor;
    Background(int c, int b) : mColor(c), mBorderColor(b) {}
};
struct Window;
struct Component : RefCount {
    Component* mParent = NULL;
    int mWidth = 0, mHeight = 0, mRootX = 0, mRootY = 0;
    ColorScheme* mOwnScheme = NULL;
    ColorScheme* mScheme = NULL;
    Window* mRoot = NULL;
    virtual ~Component() {}
    int Width() { return mWidth; }
    int Height() { return mHeight; }
    void SetSize(int w, int h)
    {
        Log(Id(this) + " SetSize " + N(w) + " " + N(h));
        mWidth = w;
        mHeight = h;
    }
    virtual void Paint(Graphics& g) { Log(Id(this) + " Component::Paint scheme " + Id(g.mColorScheme)); }
    ColorScheme* GetOwnColorScheme() { return mOwnScheme; }
    ColorScheme* GetColorScheme() { return mScheme; }
    void LocalToRoot(int& x, int& y)
    {
        x += mRootX;
        y += mRootY;
    }
    Window* GetRoot() { return mRoot; }
    void Invalidate() { Log(Id(this) + " Invalidate"); }
    void SetComponentFlags(int f, bool on) { Log(Id(this) + " SetComponentFlags " + N(f) + " " + N(on)); }
    void SetBackground(const Background& b) { Log(Id(this) + " SetBackground " + N(b.mColor) + " " + N(b.mBorderColor)); }
};
struct Container : Component {
    std::vector<SmartPtr<Component>> mChildren;
    void Fn2F6140(Component* a, int k, Component* b, int c, int d, int e, int f, int g)
    {
        Log(Id(this) + " Fn2F6140 " + Id(a) + " " + N(k) + " " + Id(b) + " " + N(c + d + e + f + g));
    }
    void AddChild(Component* c)
    {
        Log(Id(this) + " AddChild " + Id(c));
        mChildren.push_back(SmartPtr<Component>(c));
    }
    virtual void AddedToParent() { Log(Id(this) + " Container::AddedToParent"); }
};
struct PopupParams {
    PopupParams(Component* p, int f, Component* o, int x = -1, int y = -1) : mPopup(p), mOwnerComponent(o), mFlags(f), mx(x), my(y) {}
    Component* mPopup;
    Component* mOwnerComponent;
    int mFlags, mx, my;
};
struct Window {
    Container mPopupParent; // what an open popup's parent is
    void OpenPopup(PopupParams& p)
    {
        Log(Id(this) + " OpenPopup " + Id(p.mPopup) + " " + N(p.mFlags) + " " + Id(p.mOwnerComponent) + " " + N(p.mx) + " " + N(p.my));
        p.mPopup->mParent = &mPopupParent;
    }
    void ClosePopup(Component* c)
    {
        Log(Id(this) + " ClosePopup " + Id(c));
        c->mParent = NULL;
    }
};

// ---- the browser's parts
struct ScrollArea : Container {};
struct TextArea : ScrollArea {
    void SetTextFlags(int f, bool on) { Log(Id(this) + " SetTextFlags " + N(f) + " " + N(on)); }
    void Clear() { Log(Id(this) + " Clear"); }
    void AddFormatedText(const GUIString& s, bool b) { Log(Id(this) + " AddFormatedText " + Text(s) + " " + N(b)); }
    void SetOffsets(int h, int v) { Log(Id(this) + " SetOffsets " + N(h) + " " + N(v)); }
    ~TextArea() { Log("~TextArea"); }
};
struct Scroller : Container {
    ScrollArea* mArea = NULL;
    ScrollArea* GetScrollArea() { return mArea; }
    void SetScrollArea(ScrollArea* a)
    {
        Log(Id(this) + " SetScrollArea " + Id(a));
        mArea = a;
    }
    ~Scroller() { Log("~Scroller"); }
};
struct ScrollbarScroller : Scroller {
    void SetScrollbarConditions(ScrollbarCondition a, ScrollbarCondition b) { Log(Id(this) + " SetScrollbarConditions " + N(a) + " " + N(b)); }
};
enum class EeWonShellKind { MsScrollContainer = 7 };
// the MS scroll container kind: a scrollbar scroller around the area
static Component* WONShellNew(EeWonShellKind k, intptr_t a)
{
    ScrollbarScroller* s = new ScrollbarScroller;
    s->mArea = (ScrollArea*)a;
    Log("WONShellNew " + N((int)k) + " " + Id(s) + " area " + Id(s->mArea));
    return s;
}

// ---- the list and its items
struct ListItem : RefCount {
    int mTop = 0, mWidth = 0, mHeight = 0;
    ~ListItem() { Log("~ListItem"); }
};
struct ListArea : Container {
    int mLeftPad = 0, mRightPad = 0, mTopPad = 0, mVertOffset = 0;
    int GetLeftPad() { return mLeftPad; }
    int GetRightPad() { return mRightPad; }
    int GetTopPad() { return mTopPad; }
    int GetVertOffset() { return mVertOffset; }
    void ScrollPaint(Graphics& g) { Log(Id(this) + " ScrollPaint scheme " + Id(g.mColorScheme)); }
};
} // namespace Lf
