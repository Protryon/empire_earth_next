// MSButtonBase as a whole object (R6 WON round 35): the original MSButtonBase.cpp's methods and the header's inline
// setters, the statements copied from the C++ before the port with every call on a C++ object (the base Component,
// the wrapped text, the button's and component's methods, the Graphics) replaced by the host op that makes it
// (GuiHost.cpp runs the same calls in the same order), against ee_won's Rust MSButtonBase (msbtnobj.rs) over one
// scripted host. The host is a logged draw world: the size, a wrapped text whose width and height follow its text,
// wrap, centring and maximum width, the button's disabled, focus and parent state, the parent's dialog keys; every
// draw operation is traced. SetSize calls the button's size change back (the base class's Virtual08), as the C++
// component does when the size changes.
//
// Each of 20,000 sequences creates the button and makes 40 random inward calls (texts, alignments, colours, pads,
// fit to text, transparency, size changes, desired sizes, the text's paint, field reads, world changes). The traces
// must be the same. The first 40 sequences and every later one that makes an op or a call none before made are
// rust/crates/ee_won_gui/src/golden_msbtnobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include "rust/ee_won.h"

typedef EeWonGuiOp Op;
typedef EeWonGuiField Fd;
typedef EeWonSkinBtn Sb;
typedef EeWonMsBtnField Bf;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

// the world: the component's size, the wrapped text, the button's state
static int sWidth, sHeight;
static int sTextLength, sMaxWidth, sFontHeight;
static bool sWrap, sCentered;
static bool sDisabled, sFocus, sDialogKeys, sPressed, sChecked;
static intptr_t sParent;
static int sComponentBits, sButtonFlags;
static std::vector<std::string> sTrace;
static std::set<int> sOpsMet;
static void (*sSizeChanged)();

static int TextWidth()
{
    int w = sTextLength * 6;
    return sWrap && sMaxWidth > 0 && w > sMaxWidth ? sMaxWidth - (sCentered ? 1 : 0) : w;
}
static int TextHeight()
{
    int w = sTextLength * 6;
    int lines = sWrap && sMaxWidth > 0 ? (w + sMaxWidth - 1) / sMaxWidth : 1;
    return (lines < 1 ? 1 : lines) * sFontHeight;
}

static intptr_t ScriptedHost(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)inComp; (void)d;
    sOpsMet.insert((int)inOp);
    intptr_t r = 0;
    char aLine[300];
    int n = (int)inOp;
    switch (inOp) {
    case Op::Get: return (Fd)a == Fd::Width ? sWidth : (Fd)a == Fd::Height ? sHeight : 0; // pure reads: not traced
    case Op::SetSize: {
        bool aChanged = a != sWidth || b != sHeight;
        sWidth = (int)a; sHeight = (int)b;
        snprintf(aLine, sizeof(aLine), "%d set-size %lld %lld -> %d", n, (long long)a, (long long)b, aChanged ? 1 : 0);
        sTrace.push_back(aLine);
        if (aChanged) sSizeChanged();
        return 0;
    }
    case Op::SkinBoxPainter: break; // the painter function: traced with its Graphics and subclass
    case Op::WrapSetMaxWidth: sMaxWidth = (int)a; break;
    case Op::WrapSetCentered: sCentered = a != 0; break;
    case Op::WrapWidth: r = TextWidth(); break;
    case Op::WrapHeight: r = TextHeight(); break;
    case Op::SkinBtn:
        sOpsMet.insert(1000 + (int)a);
        switch ((Sb)a) {
        case Sb::IsDisabled: r = sDisabled; break;
        case Sb::HasFocus: r = sFocus; break;
        case Sb::Parent: r = sParent; break;
        case Sb::UsedDialogKeys: r = b == sParent && sDialogKeys; break;
        case Sb::IsPressed: r = sPressed; break;
        case Sb::IsChecked: r = sChecked; break;
        case Sb::SetGrabBG: sComponentBits |= 0x100; break;
        case Sb::SetWantFocus: if (b) sComponentBits |= 8; else sComponentBits &= ~8; break;
        case Sb::SetButtonFlags: if (c) sButtonFlags |= (int)b; else sButtonFlags &= ~(int)b; break;
        default: break;
        }
        break;
    case Op::Draw: {
        const EeWonDrawOp* o = (const EeWonDrawOp*)b;
        snprintf(aLine, sizeof(aLine), "%d draw %llx %d %d %d %d %d %d %d %d %d", n, (long long)a, o->kind, o->a, o->b,
                 o->c, o->d, o->e, o->f, o->g, o->h);
        sTrace.push_back(aLine);
        return 0;
    }
    default: break;
    }
    snprintf(aLine, sizeof(aLine), "%d %lld %lld %lld -> %lld", n, (long long)a, (long long)b, (long long)c, (long long)r);
    sTrace.push_back(aLine);
    return r;
}

static intptr_t H(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
{
    return ScriptedHost((void*)0x53, inOp, a, b, c, 0);
}
static intptr_t S(Sb inWhich, intptr_t b = 0, intptr_t c = 0) { return H(Op::SkinBtn, (intptr_t)inWhich, b, c); }

// ---- the original (MSButtonBase.cpp and the header's inlines; the base Component, the wrapped text, the button's
// methods and the Graphics through the host)

namespace orig {
static const intptr_t kG = 0x6;
enum { HorzAlign_Left, HorzAlign_Center, HorzAlign_Right };
enum { VertAlign_Top, VertAlign_Center, VertAlign_Bottom };
enum { StandardColor_3DFace = 1, StandardColor_3DHilight = 2, StandardColor_ButtonText = 5, StandardColor_GrayText = 6,
       StandardColor_Text = 14 };
enum FocusStyle { FocusStyle_None, FocusStyle_AroundWindow, FocusStyle_AroundText };
static void Draw(int kind, int a = 0, int b = 0, int c = 0, int d = 0)
{
    EeWonDrawOp o;
    memset(&o, 0, sizeof(o));
    o.kind = kind; o.a = a; o.b = b; o.c = c; o.d = d;
    H(Op::Draw, kG, (intptr_t)&o);
}
struct Graphics {
    void SetSysColor(int c) { Draw(EE_WON_DRAW_SET_SYS_COLOR, c); }
    void SetColor(int c) { Draw(EE_WON_DRAW_SET_COLOR, c); }
    void DrawDottedRect(int x, int y, int w, int h) { Draw(EE_WON_DRAW_DOTTED_RECT, x, y, w, h); }
    void FillRect(int x, int y, int w, int h) { Draw(EE_WON_DRAW_FILL_RECT, x, y, w, h); }
    void Translate(int x, int y) { Draw(EE_WON_DRAW_TRANSLATE, x, y); }
};
struct WrappedText {
    int GetWidth() { return (int)H(Op::WrapWidth); }
    int GetHeight() { return (int)H(Op::WrapHeight); }
    void SetMaxWidth(int w) { H(Op::WrapSetMaxWidth, w); }
    void SetCentered(bool c) { H(Op::WrapSetCentered, c); }
    void Paint(Graphics&, int x, int y) { Draw(EE_WON_DRAW_TEXT, 0, x, y); }
};
struct Container {
    bool HasUsedDialogKeys() { return S(Sb::UsedDialogKeys, (intptr_t)this) != 0; }
};
struct MSButtonBase {
    WrappedText* mText;
    int mStandardTextColor;
    int mTextColor;
    unsigned char mHAlign, mVAlign, mFocusStyle;
    unsigned char mTopTextPad, mBottomTextPad, mLeftTextPad, mRightTextPad;
    bool mIsTransparent, mAdjustHeightToFitText;
    WrappedText mTextObj;
    MSButtonBase()
    {
        this->mText = &this->mTextObj;
        this->mStandardTextColor = StandardColor_ButtonText;
        this->mTextColor = -1;
        this->mHAlign = HorzAlign_Center;
        this->mVAlign = VertAlign_Center;
        this->mFocusStyle = FocusStyle_AroundWindow;
        this->mTopTextPad = this->mBottomTextPad = this->mLeftTextPad = this->mRightTextPad = 2;
        this->mIsTransparent = false;
        this->mAdjustHeightToFitText = false;
    }
    int Width() { return sWidth; }
    int Height() { return sHeight; }
    void SetSize(int w, int h) { H(Op::SetSize, w, h); }
    bool IsDisabled() { return S(Sb::IsDisabled) != 0; }
    bool HasFocus() { return S(Sb::HasFocus) != 0; }
    void SetComponentFlagsGrabBG() { S(Sb::SetGrabBG); }
    // the inline setters
    void SetTextColor(int c) { this->mTextColor = c; }
    void SetStandardTextColor(int c) { this->mStandardTextColor = c; this->mTextColor = -1; }
    void SetHorzAlign(int h) { this->mHAlign = (unsigned char)h; this->mText->SetCentered(this->mHAlign == HorzAlign_Center); }
    void SetVertAlign(int v) { this->mVAlign = (unsigned char)v; }
    void SetFocusStyle(int s) { this->mFocusStyle = (unsigned char)s; }
    void SetHorzTextPad(int l, int r) { this->mLeftTextPad = (unsigned char)l; this->mRightTextPad = (unsigned char)r; }
    void SetVertTextPad(int t, int b) { this->mTopTextPad = (unsigned char)t; this->mBottomTextPad = (unsigned char)b; }
    void IncrementHorzTextPad(int l, int r) { this->mLeftTextPad = this->mLeftTextPad + (unsigned char)l; this->mRightTextPad = this->mRightTextPad + (unsigned char)r; }
    void IncrementVertTextPad(int t, int b) { this->mTopTextPad = this->mTopTextPad + (unsigned char)t; this->mBottomTextPad = this->mBottomTextPad + (unsigned char)b; }
    // MSButtonBase.cpp
    void SetTextHook() // after the wrapped text's SetText
    {
        this->AdjustHeightToFitText();
    }
    void SetTransparent(bool transparent)
    {
        this->mIsTransparent = transparent;
        if (transparent) this->SetComponentFlagsGrabBG();
    }
    void GetDesiredSize(int& width, int& height)
    {
        width = this->mText->GetWidth() + this->mLeftTextPad + this->mRightTextPad;
        height = this->mText->GetHeight() + this->mTopTextPad + this->mBottomTextPad;
    }
    void AdjustHeightToFitText()
    {
        if (!this->mAdjustHeightToFitText) return;
        if (this->Height() - this->mTopTextPad - this->mBottomTextPad != this->mText->GetHeight())
            this->SetSize(this->Width(), this->mText->GetHeight() + this->mTopTextPad + this->mBottomTextPad);
    }
    void SetAdjustHeightToFitText(bool adjust)
    {
        if (adjust == this->mAdjustHeightToFitText) return;
        this->mAdjustHeightToFitText = adjust;
        this->AdjustHeightToFitText();
    }
    void Virtual08()
    {
        H(Op::BaseSizeChanged);
        this->mText->SetMaxWidth(this->Width() - this->mLeftTextPad - this->mRightTextPad);
        this->AdjustHeightToFitText();
    }
    void DrawButtonText(Graphics& g, int xoff, int yoff)
    {
        int textWidth = this->mText->GetWidth();
        int textHeight = this->mText->GetHeight();
        int width = this->Width() - this->mLeftTextPad - this->mRightTextPad;
        int height = this->Height() - this->mTopTextPad - this->mBottomTextPad;
        int x, y;
        switch (this->mHAlign) {
        case HorzAlign_Left: x = 0; break;
        case HorzAlign_Center: x = (width - textWidth) / 2; break;
        case HorzAlign_Right: x = width - textWidth; break;
        default: x = 0; break;
        }
        switch (this->mVAlign) {
        case VertAlign_Top: y = 0; break;
        case VertAlign_Center: y = (height - textHeight) / 2; break;
        case VertAlign_Bottom: y = height - textHeight; break;
        default: y = 0; break;
        }
        x += xoff + this->mLeftTextPad;
        y += yoff + this->mTopTextPad;
        if (this->IsDisabled()) {
            g.SetSysColor(StandardColor_3DHilight);
            this->mText->Paint(g, x + 1, y + 1);
            g.SetSysColor(StandardColor_GrayText);
        } else if (this->mTextColor < 0) g.SetSysColor(this->mStandardTextColor);
        else g.SetColor(this->mTextColor);
        this->mText->Paint(g, x, y);
        if (!this->HasFocus())
            return;
        Container* parent = (Container*)S(Sb::Parent);
        if (parent == NULL || !parent->HasUsedDialogKeys())
            return;
        if (this->mTextColor < 0) g.SetSysColor(this->mStandardTextColor);
        else g.SetColor(this->mTextColor);
        switch (this->mFocusStyle) {
        case FocusStyle_AroundWindow: g.DrawDottedRect(4, 3, this->Width() - 8, this->Height() - 7); break;
        case FocusStyle_AroundText: g.DrawDottedRect(x - 1, y - 1, textWidth + 2, textHeight + 2); break;
        }
    }
    // MSControlConstructors.cpp's MSCheckbox::Init and MSCheckbox.cpp's Paint (the painter function through the host)
    void CheckboxInit(int checkboxWidth)
    {
        this->mStandardTextColor = StandardColor_Text;
        this->mTextColor = -1;
        this->mHAlign = HorzAlign_Left;
        this->mFocusStyle = FocusStyle_AroundText;
        this->mLeftTextPad = checkboxWidth + 4;
        S(Sb::SetWantFocus, 1);
    }
    void CheckboxPaint(Graphics& g, int checkboxHeight)
    {
        H(Op::BasePaint, kG);
        g.SetSysColor(StandardColor_3DFace);
        if (!this->mIsTransparent) g.FillRect(0, 0, this->Width(), this->Height());
        int height = checkboxHeight;
        int y = 0;
        if (this->mVAlign == VertAlign_Center) y = (this->Height() - height - this->mTopTextPad - this->mBottomTextPad) / 2;
        else if (this->mVAlign == VertAlign_Bottom) y = this->Height() - height - this->mTopTextPad - this->mBottomTextPad;
        g.Translate(0, this->mTopTextPad + y);
        H(Op::SkinBoxPainter, kG, 0);
        g.Translate(0, -y - this->mTopTextPad);
        this->DrawButtonText(g, 0, 0);
    }
    // MSControlConstructors.cpp's MSRadioButton::Init and MSRadioButton.cpp's Paint
    void RadioInit(int radioWidth)
    {
        S(Sb::SetButtonFlags, 8, 1);
        this->mHAlign = HorzAlign_Left;
        this->mFocusStyle = FocusStyle_AroundText;
        this->mLeftTextPad = radioWidth + 4;
    }
    void RadioPaint(Graphics& g, int radioHeight)
    {
        H(Op::BasePaint, kG);
        g.SetSysColor(StandardColor_3DFace);
        if (!this->mIsTransparent) g.FillRect(0, 0, this->Width(), this->Height());
        int height = radioHeight;
        int y = 0;
        if (this->mVAlign == VertAlign_Center) y = (this->Height() - height - this->mTopTextPad - this->mBottomTextPad) / 2;
        else if (this->mVAlign == VertAlign_Bottom) y = this->Height() - height - this->mTopTextPad - this->mBottomTextPad;
        g.Translate(0, this->mTopTextPad + y);
        H(Op::SkinBoxPainter, kG, 1);
        g.Translate(0, -y - this->mTopTextPad);
        this->DrawButtonText(g, 0, 0);
    }
    int GetField(int f)
    {
        switch ((Bf)f) {
        case Bf::StandardTextColor: return this->mStandardTextColor;
        case Bf::TextColor: return this->mTextColor;
        case Bf::HAlign: return this->mHAlign;
        case Bf::VAlign: return this->mVAlign;
        case Bf::FocusStyle: return this->mFocusStyle;
        case Bf::TopTextPad: return this->mTopTextPad;
        case Bf::BottomTextPad: return this->mBottomTextPad;
        case Bf::LeftTextPad: return this->mLeftTextPad;
        case Bf::RightTextPad: return this->mRightTextPad;
        case Bf::Transparent: return this->mIsTransparent;
        case Bf::AdjustHeight: return this->mAdjustHeightToFitText;
        }
        return 0;
    }
};
} // namespace orig

static orig::MSButtonBase* sOrig;
static const EeWonMsButton* sRust;
static void OrigSizeChanged() { sOrig->Virtual08(); }
static void RustSizeChanged() { ee_won_msbtn_call(sRust, EeWonMsBtnCall::SizeChanged, 0, 0, 0); }

enum { kText, kHorz, kVert, kFocusStyle, kHPad, kVPad, kIncH, kIncV, kColor, kStdColor, kTransparent, kAdjust,
       kAdjustNow, kSize, kDesired, kDrawText, kGet, kWorld, kBoxInit, kBoxPaint, kCalls };
static const char* kCallNames[kCalls] = {"text", "horz", "vert", "focus-style", "hpad", "vpad", "inc-h", "inc-v",
                                         "color", "std-color", "transparent", "adjust", "adjust-now", "size",
                                         "desired", "draw-text", "get", "world", "box-init", "box-paint"};
static unsigned sDriverSeed;
static unsigned DR(unsigned n) { sDriverSeed = sDriverSeed * 69069u + 1u; return n ? (sDriverSeed >> 16) % n : 0; }

static void Run(bool inRust)
{
    static const int kWeights[kCalls] = {5, 2, 2, 2, 2, 2, 1, 1, 2, 2, 2, 3, 1, 4, 2, 6, 3, 4, 2, 5};
    int aTotal = 0;
    for (int i = 0; i < kCalls; i++) aTotal += kWeights[i];
    int r0 = (int)DR(aTotal), k = 0;
    while (r0 >= kWeights[k]) r0 -= kWeights[k++];
    int a = 0, b = 0;
    switch (k) {
    case kText: a = (int)DR(40); break;
    case kHorz: case kVert: case kFocusStyle: a = (int)DR(4); break;
    case kHPad: case kVPad: a = (int)DR(12); b = (int)DR(12); if (DR(8) == 0) a += 250; break;
    case kIncH: case kIncV: a = (int)DR(5) - 1; b = (int)DR(260); break;
    case kColor: a = DR(3) == 0 ? -1 : (int)DR(0x1000000); break;
    case kStdColor: a = (int)DR(10); break;
    case kTransparent: case kAdjust: a = (int)DR(2); break;
    case kSize: a = (int)DR(200) - 10; b = 5 + (int)DR(60); break;
    case kDrawText: a = (int)DR(2); b = (int)DR(2); break;
    case kGet: a = 1 + (int)DR(11); break;
    case kWorld: a = (int)DR(64); break;
    case kBoxInit: a = (int)DR(2); b = DR(4) == 0 ? (int)DR(300) : 13; break; // the subclass, its box width
    case kBoxPaint: a = (int)DR(2); b = DR(4) == 0 ? (int)DR(40) : 13; break; // the subclass, its box height
    default: break;
    }
    char aLine[100];
    snprintf(aLine, sizeof(aLine), "call %s %d %d", kCallNames[k], a, b);
    sTrace.push_back(aLine);
    size_t aCallLine = sTrace.size() - 1;
    intptr_t r = 0;
    if (k == kText) sTextLength = a; // the C++ sets the wrapped text first
    if (k == kSize) { sWidth = a; sHeight = b; }
    if (k == kWorld) { // the button's state changes outside it
        sDisabled = (a & 1) != 0; sFocus = (a & 2) != 0; sDialogKeys = (a & 4) != 0;
        sParent = (a & 8) ? 0 : 0x77; sPressed = (a & 16) != 0; sChecked = (a & 32) != 0;
    }
    orig::Graphics g;
    if (inRust) {
        typedef EeWonMsBtnCall C;
        const EeWonMsButton* t = sRust;
        switch (k) {
        case kText: ee_won_msbtn_call(t, C::TextSet, 0, 0, 0); break;
        case kHorz: ee_won_msbtn_call(t, C::SetHorzAlign, a, 0, 0); break;
        case kVert: ee_won_msbtn_call(t, C::Set, (intptr_t)Bf::VAlign, a, 0); break;
        case kFocusStyle: ee_won_msbtn_call(t, C::Set, (intptr_t)Bf::FocusStyle, a, 0); break;
        case kHPad: ee_won_msbtn_call(t, C::Set, (intptr_t)Bf::LeftTextPad, a, 0); ee_won_msbtn_call(t, C::Set, (intptr_t)Bf::RightTextPad, b, 0); break;
        case kVPad: ee_won_msbtn_call(t, C::Set, (intptr_t)Bf::TopTextPad, a, 0); ee_won_msbtn_call(t, C::Set, (intptr_t)Bf::BottomTextPad, b, 0); break;
        case kIncH: ee_won_msbtn_call(t, C::IncrementHorzTextPad, a, b, 0); break;
        case kIncV: ee_won_msbtn_call(t, C::IncrementVertTextPad, a, b, 0); break;
        case kColor: ee_won_msbtn_call(t, C::Set, (intptr_t)Bf::TextColor, a, 0); break;
        case kStdColor: ee_won_msbtn_call(t, C::SetStandardTextColor, a, 0, 0); break;
        case kTransparent: ee_won_msbtn_call(t, C::SetTransparent, a, 0, 0); break;
        case kAdjust: ee_won_msbtn_call(t, C::SetAdjustHeightToFitText, a, 0, 0); break;
        case kAdjustNow: ee_won_msbtn_call(t, C::AdjustHeightToFitText, 0, 0, 0); break;
        case kSize: ee_won_msbtn_call(t, C::SizeChanged, 0, 0, 0); break;
        case kDesired: { int w = 0, h = 0; ee_won_msbtn_call(t, C::DesiredSize, (intptr_t)&w, (intptr_t)&h, 0); r = w * 1000 + h; break; }
        case kDrawText: ee_won_msbtn_call(t, C::DrawButtonText, orig::kG, a, b); break;
        case kGet: r = ee_won_msbtn_call(t, C::Get, a, 0, 0); break;
        case kBoxInit: ee_won_msbtn_call(t, C::BoxInit, a, b, 0); break;
        case kBoxPaint: ee_won_msbtn_call(t, C::BoxPaint, a, orig::kG, b); break;
        default: break;
        }
    } else {
        orig::MSButtonBase* t = sOrig;
        switch (k) {
        case kText: t->SetTextHook(); break;
        case kHorz: t->SetHorzAlign(a); break;
        case kVert: t->SetVertAlign(a); break;
        case kFocusStyle: t->SetFocusStyle(a); break;
        case kHPad: t->SetHorzTextPad(a, b); break;
        case kVPad: t->SetVertTextPad(a, b); break;
        case kIncH: t->IncrementHorzTextPad(a, b); break;
        case kIncV: t->IncrementVertTextPad(a, b); break;
        case kColor: t->SetTextColor(a); break;
        case kStdColor: t->SetStandardTextColor(a); break;
        case kTransparent: t->SetTransparent(a != 0); break;
        case kAdjust: t->SetAdjustHeightToFitText(a != 0); break;
        case kAdjustNow: t->AdjustHeightToFitText(); break;
        case kSize: t->Virtual08(); break;
        case kDesired: { int w = 0, h = 0; t->GetDesiredSize(w, h); r = w * 1000 + h; break; }
        case kDrawText: t->DrawButtonText(g, a, b); break;
        case kGet: r = t->GetField(a); break;
        case kBoxInit: if (a) t->RadioInit(b); else t->CheckboxInit(b); break;
        case kBoxPaint: if (a) t->RadioPaint(g, b); else t->CheckboxPaint(g, b); break;
        default: break;
        }
    }
    sTrace[aCallLine] += " -> " + std::to_string((long long)r);
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sSeed = inSeq * 2654435761u + 29u;
    sDriverSeed = inSeq * 40503u + 11u;
    sWidth = (int)R(200);
    sHeight = 10 + (int)R(30);
    sTextLength = 0;
    sMaxWidth = 0;
    sFontHeight = 10 + (int)R(6);
    sWrap = R(2) != 0;
    sCentered = false;
    sDisabled = sFocus = sDialogKeys = sPressed = sChecked = false;
    sParent = 0x77;
    sComponentBits = 0;
    sButtonFlags = 0;
    char aLine[100];
    snprintf(aLine, sizeof(aLine), "start %d %d %d %d", sWidth, sHeight, sFontHeight, sWrap ? 1 : 0);
    sTrace.push_back(aLine);
    if (inRust) {
        sSizeChanged = RustSizeChanged;
        EeWonMsButton* t = ee_won_msbtn_new((void*)0x53, ScriptedHost);
        sRust = t;
        for (int i = 0; i < inCalls; i++) Run(true);
        ee_won_msbtn_free(t);
        sRust = NULL;
    } else {
        sSizeChanged = OrigSizeChanged;
        sOrig = new orig::MSButtonBase;
        for (int i = 0; i < inCalls; i++) Run(false);
        delete sOrig;
        sOrig = NULL;
    }
    snprintf(aLine, sizeof(aLine), "end bits %d flags %d", sComponentBits, sButtonFlags);
    sTrace.push_back(aLine);
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kSequences = 20000;
    const int kCallsPerSeq = aPrint ? 20 : 40;
    size_t aLines = 0;
    std::set<std::string> aSeen;
    int aDiffering = 0;
    for (int seq = 0; seq < kSequences; seq++) {
        sOpsMet.clear();
        std::vector<std::string> a = RunSide(false, (unsigned)seq, kCallsPerSeq);
        std::vector<std::string> b = RunSide(true, (unsigned)seq, kCallsPerSeq);
        aLines += a.size();
        bool same = a == b;
        CHECK(same);
        if (!same) {
            aDiffering++;
            if (aDiffering <= 3) {
                printf("sequence %d differs\n", seq);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    const char* x = i < a.size() ? a[i].c_str() : "<end>";
                    const char* y = i < b.size() ? b[i].c_str() : "<end>";
                    if (strcmp(x, y) != 0) { printf("  line %u\n  orig: %s\n  rust: %s\n", (unsigned)i, x, y); break; }
                }
            }
        }
        bool aNew = seq < 40;
        for (std::set<int>::const_iterator i = sOpsMet.begin(); i != sOpsMet.end(); ++i)
            if (aSeen.insert("op" + std::to_string(*i)).second) aNew = true;
        for (size_t i = 0; i < a.size(); i++)
            if (a[i].compare(0, 5, "call ") == 0)
                if (aSeen.insert(a[i].substr(0, a[i].find(' ', 5))).second) aNew = true;
        if (aPrint && aNew) {
            printf("seq %d\n", seq);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (!aPrint)
        printf("won_msbtnobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
