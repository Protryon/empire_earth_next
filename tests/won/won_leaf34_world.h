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
    void MarkInvalid() { Log(Id(this) + " MarkInvalid"); }
    void SetComponentFlags(int f, bool on) { Log(Id(this) + " SetComponentFlags " + N(f) + " " + N(on)); }
    void SetBackground(const Background& b) { Log(Id(this) + " SetBackground " + N(b.mColor) + " " + N(b.mBorderColor)); }
    void SetPos(int x, int y) { Log(Id(this) + " SetPos " + N(x) + " " + N(y)); }
    virtual void AddedToParent() { Log(Id(this) + " Component::AddedToParent"); }
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

// ---- the paints' world (ImageButton, TextListItem, MenuItem): images, fonts, the list's paint members, the
// Graphics' other calls and WONGUIDraw.h's run over them
namespace Lf {
struct Image : RefCount {
    int mW = 0, mH = 0;
    int GetWidth() { return mW; }
    int GetHeight() { return mH; }
    void SetSize(int w, int h)
    {
        Log(Id(this) + " Image::SetSize " + N(w) + " " + N(h));
        mW = w;
        mH = h;
    }
};
struct Font : RefCount {
    int mHeight = 12;
    int GetStringWidth(const GUIString& s) { return 7 * s.length() + mHeight / 3; }
    int GetHeight() { return mHeight; }
};
struct SelectionColor {
    long mBackgroundColor, mForegroundColor;
    int mMode;
};
struct PaintGraphics : Graphics {
    unsigned long mColor = 0;
    void SetColor(unsigned long c) { Log("SetColor " + N((long long)c)); mColor = c; }
    void DrawDottedRect(int x, int y, int w, int h) { Log("DrawDottedRect " + N(x) + " " + N(y) + " " + N(w) + " " + N(h)); }
    void DrawLine(int x, int y, int w, int h) { Log("DrawLine " + N(x) + " " + N(y) + " " + N(w) + " " + N(h)); }
    void DrawImage(Image* i, int x, int y) { Log("DrawImage " + Id(i) + " " + N(x) + " " + N(y)); }
    void SetFont(Font* f) { Log("SetFont " + Id(f)); }
    void DrawString(const GUIString& s, unsigned short o, unsigned short n, int x, int y) { Log("DrawString " + Text(s) + " " + N(o) + " " + N(n) + " " + N(x) + " " + N(y)); }
    void DrawString(const GUIString& s, int x, int y) { Log("DrawString " + Text(s) + " " + N(x) + " " + N(y)); }
    void SetPixel(int x, int y, unsigned long c) { Log("SetPixel " + N(x) + " " + N(y) + " " + N((long long)c)); }
    unsigned long GetStandardColor(int c) { return 0x100 + c; }
    void FillEllipse(int x, int y, int w, int h) { Log("FillEllipse " + N(x) + " " + N(y) + " " + N(w) + " " + N(h)); }
    void DrawArc(int a, int b, int c, int d, int e, int f) { Log("DrawArc " + N(a) + " " + N(b) + " " + N(c) + " " + N(d) + " " + N(e) + " " + N(f)); }
};
typedef unsigned long DWORD;
struct WrappedText;
struct WONDrawSlots {
    Image* mImages[4];
    Font* mFont;
    const GUIString* mString;
    WONDrawSlots() : mFont(NULL), mString(NULL) { mImages[0] = mImages[1] = mImages[2] = mImages[3] = NULL; }
    WONDrawSlots(Font* f, const GUIString* s, Image* i0 = NULL, WrappedText* = NULL) : mFont(f), mString(s) { mImages[0] = i0; mImages[1] = mImages[2] = mImages[3] = NULL; }
};
class WONDrawOps {
public:
    template <class Plan> explicit WONDrawOps(Plan plan)
    {
        mOps.resize(64);
        const size_t n = plan(&mOps[0], mOps.size());
        if (n > mOps.size()) { // asked again at the plan's length (WONGUIDraw.h's)
            mOps.resize(n);
            mOps.resize(plan(&mOps[0], mOps.size()));
        } else {
            mOps.resize(n);
        }
    }
    std::vector<EeWonDrawOp> mOps;
};
struct WONDrawNoCall {
    void operator()(const EeWonDrawOp&) {}
};
// WONGUIDraw.h's WONDrawOne (the text and wrapped-text kinds left out: the paints here make none)
template <class Call> inline void WONDrawOne(PaintGraphics& g, const EeWonDrawOp& o, const WONDrawSlots& s, Call&& call)
{
    switch (o.kind) {
        case EE_WON_DRAW_SET_COLOR: g.SetColor((DWORD)(unsigned int)o.a); break;
        case EE_WON_DRAW_SET_SYS_COLOR: g.SetSysColor(o.a); break;
        case EE_WON_DRAW_FILL_RECT: g.FillRect(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_RECT: g.DrawRect(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_DOTTED_RECT: g.DrawDottedRect(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_LINE: g.DrawLine(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_IMAGE: g.DrawImage(s.mImages[o.a], o.b, o.c); break;
        case EE_WON_DRAW_IMAGE_SIZE: s.mImages[o.a]->SetSize(o.b, o.c); break;
        case EE_WON_DRAW_PUSH_CLIP: g.PushClipRect(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_POP_CLIP: g.PopClipRect(); break;
        case EE_WON_DRAW_TRANSLATE: g.Translate(o.a, o.b); break;
        case EE_WON_DRAW_SET_FONT: g.SetFont(s.mFont); break;
        case EE_WON_DRAW_STRING: g.DrawString(*s.mString, (unsigned short)o.b, (unsigned short)o.c, o.d, o.e); break;
        case EE_WON_DRAW_STRING_ALL: g.DrawString(*s.mString, o.b, o.c); break;
        case EE_WON_DRAW_CALL: call(o); break;
        case EE_WON_DRAW_PIXEL: g.SetPixel(o.a, o.b, (DWORD)(unsigned int)o.c); break;
        case EE_WON_DRAW_SYS_PIXEL: g.SetPixel(o.a, o.b, g.GetStandardColor(o.c)); break;
        case EE_WON_DRAW_FILL_ELLIPSE: g.FillEllipse(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_ARC: g.DrawArc(o.a, o.b, o.c, o.d, o.e, o.f); break;
        default: Log("draw kind " + N(o.kind)); break;
    }
}
inline void WONDrawRun(PaintGraphics& g, const WONDrawOps& ops, const WONDrawSlots& s)
{
    for (const EeWonDrawOp& o : ops.mOps) WONDrawOne(g, o, s, WONDrawNoCall());
}
inline EeWonSelColor WONDrawSelOf(const SelectionColor& s)
{
    EeWonSelColor r;
    r.background = (uint32_t)s.mBackgroundColor;
    r.foreground = (uint32_t)s.mForegroundColor;
    r.mode = (int32_t)s.mMode;
    return r;
}
// the list's members the item paints read
struct PaintList : ListArea {
    SmartPtr<Font> mFont;
    long mTextColor = 0x123456;
    int mListFlags = 0, mPaintWidth = 0, mViewWidth = 0;
    SelectionColor mSelColor = {0x00ff00, 0x0000ff, 1};
    SelectionColor mOtherSel = {0x111111, 0x222222, 2};
    long GetTextColor() { return mTextColor; }
    bool ListFlagSet(int f) { return (mListFlags & f) != 0; }
    const SelectionColor& GetSelColor() { return mOtherSel; }
    int GetPaintWidth() { return mPaintWidth; }
    int GetViewWidth() { return mViewWidth; }
};
// the list item base the text items derive from (ListItem's members and mSelected)
struct PaintItem : ListItem {
    bool mSelected = false;
    virtual void SetPos(int, int inTop)
    {
        Log(Id(this) + " ListItem::SetPos " + N(inTop));
        this->mTop = inTop;
    }
};
// ImageButton's base: the button states the paint reads
struct ImageButtonBase : Component {
    bool mDisabled = false, mChecked = false, mPressed = false, mOver = false;
    bool IsDisabled() { return mDisabled; }
    bool IsChecked() { return mChecked; }
    bool IsPressed() { return mPressed; }
    bool IsMouseOver() { return mOver; }
};
} // namespace Lf

// ---- Background's world: its component (a sizeable, invalidated, painted component) and the run with a call
namespace Lf {
template <class Call> inline void WONDrawRun(PaintGraphics& g, const WONDrawOps& ops, const WONDrawSlots& s, Call call)
{
    for (const EeWonDrawOp& o : ops.mOps) WONDrawOne(g, o, s, call);
}
} // namespace Lf
