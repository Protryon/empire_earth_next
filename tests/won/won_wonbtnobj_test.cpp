// WONButton as a whole object (R6 WON round 35): the original WONButton.cpp's and WONButtonSetup.cpp's statements,
// copied from the C++ before the port, with every call on a C++ object (the base Button, the wrapped texts, the
// images, the fonts, the button's state, the Graphics) replaced by the host op that makes it (WONButton.cpp's host
// runs the same calls in the same order), against ee_won's Rust WONButton (wonbtnobj.rs) over one scripted host.
//
// The host is a logged world: two buttons' components (size, component bits), the button's state (disabled, its
// flags, default, focus, the parent and its dialog keys), fonts, images and wrapped texts as ids with reference
// counts (a release to zero deletes the object; any later op on it is traced as a use after free), a text's width
// and height following its text, wrap, centring, maximum width and font. Every op that is not a pure read is traced
// with its answer (the reads too: the cargo replay answers from them), every draw with its slots; the references
// (whose holds and releases are not traced) are compared after each call (their order within a
// call is the containers' business: the original's std::map copy and clear order differ between standard
// libraries). SetSize calls the button's size change back, as Component::SetSize does.
//
// Each of 20,000 sequences makes two buttons and 40 random calls (states set, texts, sizes, fit to text, wrap,
// alignments, colours, fonts, copies, the state, the text's paint, paints, fields, world changes); at the end both
// buttons go and every reference must be back at the world's. The first 40 sequences and every later one that makes
// an op or a call none before made are rust/crates/ee_won/src/golden_wonbtnobj.txt, which the Rust tests replay;
// `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "rust/ee_won.h"

typedef EeWonGuiOp Op;
typedef EeWonGuiField Fd;
typedef EeWonSkinBtn Sb;
typedef EeWonWbText Wt;
typedef EeWonWbImage Wi;
typedef EeWonWbFont Wf;
typedef EeWonWbField Bf;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

// ---- the world
static const intptr_t kComp0 = 0x53, kComp1 = 0x54, kG = 0x6, kDefaultFont = 209;
struct TextObj { int length, maxWidth; bool wrap, centered; intptr_t font; };
static std::map<intptr_t, int> sRefs; // fonts 200.., images 300.., texts 1000..
static std::set<intptr_t> sDead;
static std::map<intptr_t, TextObj> sTexts;
static intptr_t sNextText, sNextImage;
static int sWidth[2], sHeight[2], sBits[2];
static bool sDisabled, sFocus, sDialogKeys, sDefault, sParentNull;
static int sFlags;
static int sPendingTextLength;
static std::vector<std::string> sTrace;
static std::set<int> sOpsMet;
static void (*sSizeChanged)(int);

static int Side(void* inComp) { return inComp == (void*)kComp1 ? 1 : 0; }
static int FontHeight(intptr_t f) { return f ? 10 + (int)(f % 5) : 0; }
static int TextWidth(const TextObj& t)
{
    int w = t.length * 6;
    return t.wrap && t.maxWidth > 0 && w > t.maxWidth ? t.maxWidth - (t.centered ? 1 : 0) : w;
}
static int TextHeight(const TextObj& t)
{
    int w = t.length * 6;
    int lines = t.wrap && t.maxWidth > 0 ? (w + t.maxWidth - 1) / t.maxWidth : 1;
    return (lines < 1 ? 1 : lines) * FontHeight(t.font);
}
static int ImageWidth(intptr_t i) { return 10 + (int)(i % 7) * 9; }
static int ImageHeight(intptr_t i) { return 8 + (int)(i % 5) * 7; }

static void Trace(const char* s) { sTrace.push_back(s); }
static bool Live(intptr_t o, const char* what)
{
    if (o != 0 && sDead.count(o)) {
        char aLine[100];
        snprintf(aLine, sizeof(aLine), "DEAD %s %lld", what, (long long)o);
        Trace(aLine);
        return false;
    }
    return true;
}
static void Hold(intptr_t o) { if (Live(o, "hold")) sRefs[o]++; }
static void Release(intptr_t o)
{
    if (!Live(o, "release")) return;
    if (--sRefs[o] <= 0) { sRefs.erase(o); sDead.insert(o); sTexts.erase(o); }
}

static intptr_t ScriptedHost(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    int side = Side(inComp);
    sOpsMet.insert((int)inOp);
    intptr_t r = 0;
    char aLine[300];
    int n = (int)inOp;
    switch (inOp) {
    case Op::Get: return (Fd)a == Fd::Width ? sWidth[side] : (Fd)a == Fd::Height ? sHeight[side] : 0; // pure
    case Op::SetSize: {
        bool aChanged = a != sWidth[side] || b != sHeight[side];
        sWidth[side] = (int)a; sHeight[side] = (int)b;
        snprintf(aLine, sizeof(aLine), "%d set-size %d %lld %lld -> %d", n, side, (long long)a, (long long)b, aChanged ? 1 : 0);
        Trace(aLine);
        if (aChanged) sSizeChanged(side);
        return 0;
    }
    case Op::SkinBtn:
        sOpsMet.insert(1000 + (int)a);
        switch ((Sb)a) {
        case Sb::IsDisabled: r = sDisabled; break;
        case Sb::HasFocus: r = sFocus; break;
        case Sb::Parent: r = sParentNull ? 0 : 0x77; break;
        case Sb::IsPressed: r = (sFlags & 0x20) != 0; break;
        case Sb::ButtonFlags: r = sFlags; break;
        case Sb::IsDefaultButton: r = sDefault; break;
        case Sb::FlagSet: r = (sBits[Side((void*)b)] & (int)c) != 0; break;
        case Sb::UsedDialogKeys: r = b == 0x77 && sDialogKeys; break;
        case Sb::SetGrabBG: sBits[side] |= 0x100; break;
        case Sb::SetFlag: sBits[side] |= (int)b; break;
        default: break;
        }
        break;
    case Op::WbText: {
        sOpsMet.insert(2000 + (int)a);
        Wt k = (Wt)a;
        if (k == Wt::New) {
            intptr_t t = sNextText++;
            sTexts[t] = TextObj{0, 0, false, false, 0};
            sRefs[t] = 1; // answered with its one reference
            r = t;
            break;
        }
        if (k == Wt::Hold) { Hold(b); return 0; }
        if (k == Wt::Release) { Release(b); return 0; }
        if (b == 0) { snprintf(aLine, sizeof(aLine), "%d NULL text %lld", n, (long long)a); Trace(aLine); return 0; }
        if (!Live(b, "text")) return 0;
        TextObj& t = sTexts[b];
        switch (k) {
        case Wt::Width: r = TextWidth(t); break;
        case Wt::Height: r = TextHeight(t); break;
        case Wt::Font: r = t.font; break;
        case Wt::SetText: t.length = (int)c; break; // the GUIString is its length here
        case Wt::SetMaxWidth: t.maxWidth = (int)c; break;
        case Wt::SetWrap: t.wrap = c != 0; break;
        case Wt::SetCentered: t.centered = c != 0; break;
        case Wt::SetFont: t.font = c; break;
        case Wt::Text: r = 0x10000 + b; break;
        default: break;
        }
        break;
    }
    case Op::WbImage:
        sOpsMet.insert(3000 + (int)a);
        if ((Wi)a == Wi::Hold) { Hold(b); return 0; }
        if ((Wi)a == Wi::Release) { Release(b); return 0; }
        if (!Live(b, "image")) return 0;
        if ((Wi)a == Wi::Width) r = ImageWidth(b);
        if ((Wi)a == Wi::Height) r = ImageHeight(b);
        if ((Wi)a == Wi::Duplicate) { r = sNextImage++; sRefs[r] = 1; }
        break;
    case Op::WbFont:
        sOpsMet.insert(4000 + (int)a);
        if ((Wf)a == Wf::Hold) { Hold(b); return 0; }
        if ((Wf)a == Wf::Release) { Release(b); return 0; }
        r = kDefaultFont;
        break;
    case Op::Draw: {
        const EeWonDrawOp* o = (const EeWonDrawOp*)b;
        snprintf(aLine, sizeof(aLine), "%d draw %d %llx %d %d %d %d %d %d %d %d %d img %lld txt %lld", n, side,
                 (long long)a, o->kind, o->a, o->b, o->c, o->d, o->e, o->f, o->g, o->h, (long long)c, (long long)d);
        Trace(aLine);
        return 0;
    }
    default: break;
    }
    snprintf(aLine, sizeof(aLine), "%d %d %lld %lld %lld %lld -> %lld", n, side, (long long)a, (long long)b, (long long)c,
             (long long)d, (long long)r);
    Trace(aLine);
    return r;
}

static intptr_t H(int side, Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    return ScriptedHost((void*)(side ? kComp1 : kComp0), inOp, a, b, c, d);
}

// ---- the original (WONButton.cpp, WONButtonSetup.cpp and the header's inlines; the base Button, the objects and the
// Graphics through the host)

namespace orig {
enum { HorzAlign_Left, HorzAlign_Center, HorzAlign_Right };
enum { VertAlign_Top, VertAlign_Center, VertAlign_Bottom };
enum { ComponentFlag_GrabBG = 0x100, ComponentFlag_Translucent = 0x200 };
static int sSide; // the button whose method runs (its component's ops)
static intptr_t T(Wt k, intptr_t t, intptr_t c = 0) { return H(sSide, Op::WbText, (intptr_t)k, t, c); }
static intptr_t I(Wi k, intptr_t i) { return H(sSide, Op::WbImage, (intptr_t)k, i); }
// SmartPtr over the world's objects (the image's Hold/Release the image ops, the text's the text ops)
template <int Kind> struct SP {
    intptr_t p;
    SP() : p(0) {}
    SP(intptr_t x) : p(x) { if (p) Op_(true, p); }
    SP(const SP& o) : p(o.p) { if (p) Op_(true, p); }
    ~SP() { if (p) Op_(false, p); }
    SP& operator=(intptr_t x) { if (x) Op_(true, x); if (p) Op_(false, p); p = x; return *this; }
    SP& operator=(const SP& o) { return *this = o.p; }
    intptr_t get() const { return p; }
    static void Op_(bool hold, intptr_t x)
    {
        if (Kind == 0) T(hold ? Wt::Hold : Wt::Release, x);
        else if (Kind == 1) I(hold ? Wi::Hold : Wi::Release, x);
        else H(sSide, Op::WbFont, (intptr_t)(hold ? Wf::Hold : Wf::Release), x);
    }
    struct Adopt {};
    SP(intptr_t x, Adopt) : p(x) {} // an object answered with its reference (new WrappedText, Duplicate)
};
typedef SP<0> TextPtr;
typedef SP<1> ImagePtr;
typedef SP<2> FontPtr;
struct WONButton {
    struct ButtonState {
        TextPtr mText;
        ImagePtr mImage;
        int mColor;
        ButtonState() : mColor(-1) {}
    };
    typedef std::map<intptr_t, TextPtr> FontTextMap;
    int mSide;
    ButtonState mStates[5][2];
    FontTextMap mFontTextMap;
    bool mAdjustHeightToFitText;
    int mLeftTextPad, mTopTextPad, mRightTextPad, mBottomTextPad;
    int mLeftFocusPad, mTopFocusPad, mRightFocusPad, mBottomFocusPad;
    int mPushDX, mPushDY, mDefaultTextColor;
    bool mHasCheck, mScaleImage, mWrapText, mDrawGrayText, mIsTransparent;
    unsigned char mHAlign, mVAlign, mFocusStyle;
    int Width() { return sWidth[mSide]; }
    int Height() { return sHeight[mSide]; }
    void SetSize(int w, int h) { H(mSide, Op::SetSize, w, h); }
    // WONButtonSetup.cpp (its SetButtonFlags and WantFocus stay in the C++ constructor)
    WONButton(int side) : mSide(side)
    {
        this->mLeftTextPad = this->mRightTextPad = 8;
        this->mTopTextPad = this->mBottomTextPad = 4;
        this->mLeftFocusPad = this->mRightFocusPad = 4;
        this->mTopFocusPad = 3;
        this->mBottomFocusPad = 4;
        this->mPushDX = this->mPushDY = 1;
        this->mHasCheck = false;
        this->mScaleImage = true;
        this->mAdjustHeightToFitText = false;
        this->mWrapText = false;
        this->mDrawGrayText = true;
        this->mIsTransparent = false;
        this->mHAlign = HorzAlign_Center;
        this->mVAlign = VertAlign_Center;
        this->mFocusStyle = 1;
        this->mDefaultTextColor = 0x80000005;
    }
    // WONButton.cpp
    void SetTextHook(intptr_t theText)
    {
        FontTextMap::iterator anItr = this->mFontTextMap.begin();
        while (anItr != this->mFontTextMap.end()) {
            T(Wt::SetText, anItr->second.get(), theText);
            ++anItr;
        }
        this->AdjustHeightToFitText();
    }
    void Virtual08()
    {
        H(mSide, Op::BaseSizeChanged);
        if (!this->mWrapText) return;
        FontTextMap::iterator anItr = this->mFontTextMap.begin();
        while (anItr != this->mFontTextMap.end()) {
            T(Wt::SetMaxWidth, anItr->second.get(), this->Width() - this->mLeftTextPad - this->mRightTextPad);
            ++anItr;
        }
        this->AdjustHeightToFitText();
    }
    void AdjustHeightToFitText()
    {
        if (!this->mAdjustHeightToFitText) return;
        int width, height;
        this->GetTextSize(width, height);
        if (this->Height() - this->mTopTextPad - this->mBottomTextPad != height)
            this->SetSize(this->Width(), height + this->mTopTextPad + this->mBottomTextPad);
    }
    void SetWrapText(bool wrap)
    {
        if (this->mWrapText == wrap) return;
        this->mWrapText = wrap;
        FontTextMap::iterator anItr = this->mFontTextMap.begin();
        while (anItr != this->mFontTextMap.end()) {
            T(Wt::SetWrap, anItr->second.get(), wrap);
            ++anItr;
        }
    }
    void SetButtonState(int theState, intptr_t theImage, int theColor, intptr_t theFont, bool checked)
    {
        int checkedIndex = checked ? 1 : 0;
        ButtonState& aState = this->mStates[theState][checkedIndex];
        if (checkedIndex) this->mHasCheck = true;
        FontPtr aFont;
        if (this->mFontTextMap.empty() && theFont == 0)
            theFont = (aFont = H(mSide, Op::WbFont, (intptr_t)Wf::Default)).get();
        if (theFont != 0) {
            FontTextMap::iterator anItr = this->mFontTextMap.insert(FontTextMap::value_type(theFont, TextPtr())).first;
            if (anItr->second.get() == 0) {
                TextPtr aText(T(Wt::New, 0), TextPtr::Adopt());
                T(Wt::SetCentered, aText.get(), this->mHAlign == HorzAlign_Center);
                T(Wt::SetWrap, aText.get(), false);
                T(Wt::SetFont, aText.get(), theFont);
                anItr->second = aText;
            }
            aState.mText = anItr->second;
        }
        aState.mImage = theImage;
        aState.mColor = theColor;
    }
    void GetTextSize(int& width, int& height)
    {
        width = 0; height = 0;
        FontTextMap::iterator anItr = this->mFontTextMap.begin();
        while (anItr != this->mFontTextMap.end()) {
            intptr_t aText = anItr->second.get();
            if ((int)T(Wt::Width, aText) > width) width = (int)T(Wt::Width, aText);
            if ((int)T(Wt::Height, aText) > height) height = (int)T(Wt::Height, aText);
            ++anItr;
        }
    }
    void GetDesiredSize(int& width, int& height)
    {
        this->GetTextSize(width, height);
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 2; j++) {
                intptr_t anImage = this->mStates[i][j].mImage.get();
                if (anImage == 0) continue;
                if (!this->mScaleImage && (int)I(Wi::Width, anImage) > width) width = (int)I(Wi::Width, anImage);
                if (!this->mScaleImage && (int)I(Wi::Height, anImage) > height) height = (int)I(Wi::Height, anImage);
            }
        }
        width += this->mLeftTextPad + this->mRightTextPad;
        height += this->mTopTextPad + this->mBottomTextPad;
    }
    void SetHorzAlign(int theHorzAlign)
    {
        if (theHorzAlign == this->mHAlign) return;
        if (this->mHAlign != HorzAlign_Center && theHorzAlign != HorzAlign_Center) return;
        this->mHAlign = (unsigned char)theHorzAlign;
        FontTextMap::iterator anItr = this->mFontTextMap.begin();
        while (anItr != this->mFontTextMap.end()) {
            T(Wt::SetCentered, anItr->second.get(), this->mHAlign == HorzAlign_Center);
            ++anItr;
        }
    }
    void SetTextColor(int theColor)
    {
        if (!this->mScaleImage) {
            for (int i = 0; i < 5; i++)
                for (int j = 0; j < 2; j++) this->mStates[i][j].mColor = theColor;
        } else {
            this->mStates[0][0].mColor = theColor;
            this->mStates[0][1].mColor = theColor;
            this->mStates[4][0].mColor = theColor;
            this->mStates[4][1].mColor = theColor;
        }
    }
    void SetFont(intptr_t theFont)
    {
        this->mFontTextMap.clear();
        TextPtr aText = this->mStates[0][0].mText.get();
        T(Wt::SetFont, aText.get(), theFont);
        this->mFontTextMap[theFont] = aText;
        for (int i = 0; i < 5; i++)
            for (int j = 0; j < 2; j++) this->mStates[i][j].mText = aText;
    }
    bool FlagSet(WONButton* c, int f) { return H(mSide, Op::SkinBtn, (intptr_t)Sb::FlagSet, c->mSide ? kComp1 : kComp0, f) != 0; }
    void CopyAttributes(WONButton* from)
    {
        H(mSide, Op::WbBaseCopy, from->mSide ? kComp1 : kComp0);
        this->mLeftTextPad = from->mLeftTextPad;
        this->mTopTextPad = from->mTopTextPad;
        this->mRightTextPad = from->mRightTextPad;
        this->mBottomTextPad = from->mBottomTextPad;
        this->mLeftFocusPad = from->mLeftFocusPad;
        this->mTopFocusPad = from->mTopFocusPad;
        this->mRightFocusPad = from->mRightFocusPad;
        this->mBottomFocusPad = from->mBottomFocusPad;
        this->mPushDX = from->mPushDX;
        this->mPushDY = from->mPushDY;
        this->mHasCheck = from->mHasCheck;
        this->mScaleImage = from->mScaleImage;
        this->mHAlign = from->mHAlign;
        this->mVAlign = from->mVAlign;
        this->mDrawGrayText = from->mDrawGrayText;
        this->mIsTransparent = from->mIsTransparent;
        this->mFocusStyle = from->mFocusStyle;
        this->mDefaultTextColor = from->mDefaultTextColor;
        if (FlagSet(from, ComponentFlag_GrabBG)) H(mSide, Op::SkinBtn, (intptr_t)Sb::SetFlag, ComponentFlag_GrabBG);
        if (FlagSet(from, ComponentFlag_Translucent)) H(mSide, Op::SkinBtn, (intptr_t)Sb::SetFlag, ComponentFlag_Translucent);
        this->mFontTextMap = from->mFontTextMap;
        FontTextMap::iterator anItr = this->mFontTextMap.begin();
        while (anItr != this->mFontTextMap.end()) {
            anItr->second = TextPtr(T(Wt::New, 0), TextPtr::Adopt());
            T(Wt::SetCentered, anItr->second.get(), this->mHAlign == HorzAlign_Center);
            T(Wt::SetWrap, anItr->second.get(), this->mWrapText);
            T(Wt::SetFont, anItr->second.get(), anItr->first);
            ++anItr;
        }
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 2; j++) {
                ButtonState& src = from->mStates[i][j];
                ButtonState& dst = this->mStates[i][j];
                dst.mColor = src.mColor;
                if (src.mImage.get() != 0) {
                    if (this->mScaleImage)
                        dst.mImage = ImagePtr(I(Wi::Duplicate, src.mImage.get()), ImagePtr::Adopt());
                    else
                        dst.mImage = src.mImage;
                }
                if (src.mText.get() != 0)
                    dst.mText = this->mFontTextMap[T(Wt::Font, src.mText.get())];
            }
        }
    }
    // GetButtonState: the state and fallback order are guifocus.rs's (as the C++ called them)
    void GetButtonState(int* theColor, intptr_t* theText, intptr_t* theImage)
    {
        int aState = 0;
        // the call's three reads in their written order (C++ leaves an argument list's order open)
        bool aDisabled = H(mSide, Op::SkinBtn, (intptr_t)Sb::IsDisabled) != 0;
        uint32_t aFlags = (uint32_t)H(mSide, Op::SkinBtn, (intptr_t)Sb::ButtonFlags);
        bool aDefault = H(mSide, Op::SkinBtn, (intptr_t)Sb::IsDefaultButton) != 0;
        int aCheckedIndex = ee_won_gui_button_state(aDisabled, aFlags, aDefault, this->mHasCheck, &aState);
        intptr_t anImage = 0;
        int aColor = -1;
        intptr_t aText = 0;
        int s = 0, c = 0;
        for (int k = 0; ee_won_gui_button_state_cell(aState, aCheckedIndex, k, &s, &c); k++) {
            ButtonState& aCell = this->mStates[s][c];
            if (anImage == 0) anImage = aCell.mImage.get();
            if (aColor == -1) aColor = aCell.mColor;
            if (aText == 0) aText = aCell.mText.get();
        }
        if (aColor == -1) aColor = this->mDefaultTextColor;
        if (theText) *theText = aText;
        if (theColor) *theColor = aColor;
        if (theImage) *theImage = anImage;
    }
    EeWonButtonPaint PaintOf(intptr_t theImage, intptr_t theText, int theColor)
    {
        EeWonButtonPaint p;
        p.width = this->Width();
        p.height = this->Height();
        p.transparent = this->mIsTransparent;
        p.scale_image = this->mScaleImage;
        p.has_image = theImage != 0;
        p.has_text = theText != 0;
        p.pressed = H(mSide, Op::SkinBtn, (intptr_t)Sb::IsPressed) != 0;
        p.disabled = H(mSide, Op::SkinBtn, (intptr_t)Sb::IsDisabled) != 0;
        p.draw_gray_text = this->mDrawGrayText;
        p.focus_shown = this->mFocusStyle && H(mSide, Op::SkinBtn, (intptr_t)Sb::HasFocus) != 0 &&
                        H(mSide, Op::SkinBtn, (intptr_t)Sb::UsedDialogKeys, H(mSide, Op::SkinBtn, (intptr_t)Sb::Parent)) != 0;
        p.h_align = this->mHAlign;
        p.v_align = this->mVAlign;
        p.image_width = theImage ? (int)I(Wi::Width, theImage) : 0;
        p.image_height = theImage ? (int)I(Wi::Height, theImage) : 0;
        p.text_width = theText ? (int)T(Wt::Width, theText) : 0;
        p.text_height = theText ? (int)T(Wt::Height, theText) : 0;
        p.color = theColor;
        p.left_text_pad = this->mLeftTextPad;
        p.top_text_pad = this->mTopTextPad;
        p.right_text_pad = this->mRightTextPad;
        p.bottom_text_pad = this->mBottomTextPad;
        p.left_focus_pad = this->mLeftFocusPad;
        p.top_focus_pad = this->mTopFocusPad;
        p.right_focus_pad = this->mRightFocusPad;
        p.bottom_focus_pad = this->mBottomFocusPad;
        p.push_dx = this->mPushDX;
        p.push_dy = this->mPushDY;
        return p;
    }
    void Run(size_t (*plan)(const EeWonButtonPaint*, EeWonDrawOp*, size_t), const EeWonButtonPaint& p, intptr_t img, intptr_t txt)
    {
        EeWonDrawOp ops[48];
        size_t n = plan(&p, ops, 48);
        for (size_t i = 0; i < n && i < 48; i++) H(mSide, Op::Draw, kG, (intptr_t)&ops[i], img, txt);
    }
    void DrawButtonText(intptr_t theText, int theColor)
    {
        if (theText == 0) return;
        EeWonButtonPaint p = PaintOf(0, theText, theColor);
        Run(ee_won_gui_draw_button_text, p, 0, theText);
    }
    void Paint()
    {
        H(mSide, Op::BasePaint, kG);
        intptr_t anImage;
        int aColor;
        intptr_t aText;
        this->GetButtonState(&aColor, &aText, &anImage);
        EeWonButtonPaint p = PaintOf(anImage, aText, aColor);
        Run(ee_won_gui_draw_button, p, anImage, aText);
    }
    void SetAdjustHeightToFitText(bool adjust)
    {
        if (adjust == this->mAdjustHeightToFitText) return;
        this->mAdjustHeightToFitText = adjust;
        this->AdjustHeightToFitText();
    }
    void SetTransparent(bool isTransparent)
    {
        this->mIsTransparent = isTransparent;
        if (isTransparent) H(mSide, Op::SkinBtn, (intptr_t)Sb::SetGrabBG);
    }
    intptr_t GetText()
    {
        if (this->mFontTextMap.empty()) return 0;
        return T(Wt::Text, this->mFontTextMap.begin()->second.get());
    }
    int* FieldOf(int f)
    {
        switch ((Bf)f) {
        case Bf::LeftTextPad: return &mLeftTextPad;
        case Bf::TopTextPad: return &mTopTextPad;
        case Bf::RightTextPad: return &mRightTextPad;
        case Bf::BottomTextPad: return &mBottomTextPad;
        case Bf::LeftFocusPad: return &mLeftFocusPad;
        case Bf::TopFocusPad: return &mTopFocusPad;
        case Bf::RightFocusPad: return &mRightFocusPad;
        case Bf::BottomFocusPad: return &mBottomFocusPad;
        case Bf::PushDX: return &mPushDX;
        case Bf::PushDY: return &mPushDY;
        case Bf::DefaultTextColor: return &mDefaultTextColor;
        default: return NULL;
        }
    }
    bool* FlagOf(int f)
    {
        switch ((Bf)f) {
        case Bf::AdjustHeight: return &mAdjustHeightToFitText;
        case Bf::HasCheck: return &mHasCheck;
        case Bf::ScaleImage: return &mScaleImage;
        case Bf::WrapText: return &mWrapText;
        case Bf::DrawGrayText: return &mDrawGrayText;
        case Bf::Transparent: return &mIsTransparent;
        default: return NULL;
        }
    }
    unsigned char* ByteOf(int f)
    {
        switch ((Bf)f) {
        case Bf::HAlign: return &mHAlign;
        case Bf::VAlign: return &mVAlign;
        case Bf::FocusStyle: return &mFocusStyle;
        default: return NULL;
        }
    }
    int Get(int f)
    {
        if (int* p = FieldOf(f)) return *p;
        if (bool* p = FlagOf(f)) return *p;
        if (unsigned char* p = ByteOf(f)) return *p;
        return 0;
    }
    void Set(int f, int v)
    {
        if (int* p = FieldOf(f)) *p = v;
        if (bool* p = FlagOf(f)) *p = v != 0;
        if (unsigned char* p = ByteOf(f)) *p = (unsigned char)v;
    }
};
} // namespace orig

static orig::WONButton* sOrig[2];
static const EeWonWonButton* sRust[2];
static void OrigSizeChanged(int side) { int s = orig::sSide; orig::sSide = side; sOrig[side]->Virtual08(); orig::sSide = s; }
static void RustSizeChanged(int side) { ee_won_wonbtn_call(sRust[side], EeWonWbCall::SizeChanged, 0, 0, 0, 0); }

enum { kSetState, kText, kSize, kAdjust, kAdjustNow, kWrap, kTransparent, kTextSize, kDesired, kHorz, kColor, kFont,
       kCopy, kState, kDrawText, kPaint, kGet, kSet, kGetText, kWorld, kCalls };
static const char* kCallNames[kCalls] = {"set-state", "text", "size", "adjust", "adjust-now", "wrap", "transparent",
                                         "text-size", "desired", "horz", "color", "font", "copy", "state",
                                         "draw-text", "paint", "get", "set", "get-text", "world"};
static unsigned sDriverSeed;
static unsigned DR(unsigned n) { sDriverSeed = sDriverSeed * 69069u + 1u; return n ? (sDriverSeed >> 16) % n : 0; }

static std::string RefsLine()
{
    std::string s = "refs";
    for (std::map<intptr_t, int>::const_iterator i = sRefs.begin(); i != sRefs.end(); ++i)
        s += " " + std::to_string((long long)i->first) + ":" + std::to_string(i->second);
    s += " dead " + std::to_string((long long)sDead.size());
    return s;
}

static void Run(bool inRust)
{
    static const int kWeights[kCalls] = {8, 4, 3, 2, 1, 2, 2, 1, 2, 2, 2, 1, 2, 2, 3, 5, 2, 3, 1, 4};
    int aTotal = 0;
    for (int i = 0; i < kCalls; i++) aTotal += kWeights[i];
    int r0 = (int)DR(aTotal), k = 0;
    while (r0 >= kWeights[k]) r0 -= kWeights[k++];
    int side = DR(4) == 0 ? 1 : 0;
    int a = 0, b = 0, c = 0, d = 0, e = 0;
    switch (k) {
    case kSetState: a = (int)DR(5); b = DR(3) ? 300 + (int)DR(8) : 0; c = DR(3) ? (int)DR(0x1000000) : -1;
        d = DR(3) ? 200 + (int)DR(4) : 0; e = (int)DR(2); break;
    case kText: a = (int)DR(40); break;
    case kSize: a = (int)DR(200) - 10; b = 5 + (int)DR(60); break;
    case kAdjust: case kWrap: case kTransparent: a = (int)DR(2); break;
    case kHorz: a = (int)DR(3); break;
    case kColor: a = DR(4) ? (int)DR(0x1000000) : -1; break;
    case kFont: a = 200 + (int)DR(4); break;
    case kCopy: side = 0; break;
    case kGet: a = 1 + (int)DR(20); break;
    case kSet: a = 1 + (int)DR(20); b = (int)DR(12) - 1; if (DR(6) == 0) b += 250; break;
    case kWorld: a = (int)DR(1024); break;
    default: break;
    }
    char aLine[160];
    snprintf(aLine, sizeof(aLine), "call %s %d %d %d %d %d %d", kCallNames[k], side, a, b, c, d, e);
    Trace(aLine);
    size_t aCallLine = sTrace.size() - 1;
    intptr_t r = 0;
    if (k == kSize) { sWidth[side] = a; sHeight[side] = b; }
    if (k == kWorld) {
        sDisabled = (a & 1) != 0; sFocus = (a & 2) != 0; sDialogKeys = (a & 4) != 0; sParentNull = (a & 8) != 0;
        sDefault = (a & 16) != 0; sFlags = ((a & 32) ? 0x20 : 0) | ((a & 64) ? 0x11 : 0) | ((a & 128) ? 4 : 0);
        sBits[1] = ((a & 256) ? 0x100 : 0) | ((a & 512) ? 0x200 : 0);
    }
    if (inRust) {
        typedef EeWonWbCall C;
        const EeWonWonButton* t = sRust[side];
        switch (k) {
        case kSetState: ee_won_wonbtn_set_state(t, a, (void*)(intptr_t)b, c, (void*)(intptr_t)d, e != 0); break;
        case kText: ee_won_wonbtn_call(t, C::SetTextHook, a, 0, 0, 0); break;
        case kSize: ee_won_wonbtn_call(t, C::SizeChanged, 0, 0, 0, 0); break;
        case kAdjust: ee_won_wonbtn_call(t, C::SetAdjustHeightToFitText, a, 0, 0, 0); break;
        case kAdjustNow: ee_won_wonbtn_call(t, C::AdjustHeightToFitText, 0, 0, 0, 0); break;
        case kWrap: ee_won_wonbtn_call(t, C::SetWrapText, a, 0, 0, 0); break;
        case kTransparent: ee_won_wonbtn_call(t, C::SetTransparent, a, 0, 0, 0); break;
        case kTextSize: { int w = 0, h = 0; ee_won_wonbtn_call(t, C::GetTextSize, (intptr_t)&w, (intptr_t)&h, 0, 0); r = w * 1000 + h; break; }
        case kDesired: { int w = 0, h = 0; ee_won_wonbtn_call(t, C::DesiredSize, (intptr_t)&w, (intptr_t)&h, 0, 0); r = w * 1000 + h; break; }
        case kHorz: ee_won_wonbtn_call(t, C::SetHorzAlign, a, 0, 0, 0); break;
        case kColor: ee_won_wonbtn_call(t, C::SetTextColor, a, 0, 0, 0); break;
        case kFont: ee_won_wonbtn_call(t, C::SetFont, a, 0, 0, 0); break;
        case kCopy: ee_won_wonbtn_copy(sRust[0], sRust[1], (void*)kComp1); break;
        case kState: { int col = 0; intptr_t tx = 0, im = 0; ee_won_wonbtn_call(t, C::GetButtonState, kG, (intptr_t)&col, (intptr_t)&tx, (intptr_t)&im); r = (long long)col * 0 + tx * 100000 + im * 10 + (col & 7); break; }
        case kDrawText: { int col = 0; intptr_t tx = 0; ee_won_wonbtn_call(t, C::GetButtonState, kG, (intptr_t)&col, (intptr_t)&tx, 0); ee_won_wonbtn_call(t, C::DrawButtonText, kG, tx, col, 0); break; }
        case kPaint: ee_won_wonbtn_call(t, C::Paint, kG, 0, 0, 0); break;
        case kGet: r = ee_won_wonbtn_call(t, C::Get, a, 0, 0, 0); break;
        case kSet: ee_won_wonbtn_call(t, C::Set, a, b, 0, 0); break;
        case kGetText: r = ee_won_wonbtn_call(t, C::GetText, 0, 0, 0, 0); break;
        default: break;
        }
    } else {
        orig::WONButton* t = sOrig[side];
        orig::sSide = side;
        switch (k) {
        case kSetState: t->SetButtonState(a, b, c, d, e != 0); break;
        case kText: t->SetTextHook(a); break;
        case kSize: t->Virtual08(); break;
        case kAdjust: t->SetAdjustHeightToFitText(a != 0); break;
        case kAdjustNow: t->AdjustHeightToFitText(); break;
        case kWrap: t->SetWrapText(a != 0); break;
        case kTransparent: t->SetTransparent(a != 0); break;
        case kTextSize: { int w = 0, h = 0; t->GetTextSize(w, h); r = w * 1000 + h; break; }
        case kDesired: { int w = 0, h = 0; t->GetDesiredSize(w, h); r = w * 1000 + h; break; }
        case kHorz: t->SetHorzAlign(a); break;
        case kColor: t->SetTextColor(a); break;
        case kFont: t->SetFont(a); break;
        case kCopy: sOrig[0]->CopyAttributes(sOrig[1]); break;
        case kState: { int col = 0; intptr_t tx = 0, im = 0; t->GetButtonState(&col, &tx, &im); r = (long long)col * 0 + tx * 100000 + im * 10 + (col & 7); break; }
        case kDrawText: { int col = 0; intptr_t tx = 0; t->GetButtonState(&col, &tx, NULL); t->DrawButtonText(tx, col); break; }
        case kPaint: t->Paint(); break;
        case kGet: r = t->Get(a); break;
        case kSet: t->Set(a, b); break;
        case kGetText: r = t->GetText(); break;
        default: break;
        }
    }
    sTrace[aCallLine] += " -> " + std::to_string((long long)r);
    Trace(RefsLine().c_str());
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sSeed = inSeq * 2654435761u + 29u;
    sDriverSeed = inSeq * 40503u + 11u;
    sRefs.clear(); sDead.clear(); sTexts.clear();
    for (intptr_t f = 200; f < 204; f++) sRefs[f] = 1;
    sRefs[kDefaultFont] = 1;
    for (intptr_t i = 300; i < 308; i++) sRefs[i] = 1;
    sNextText = 1000; sNextImage = 400;
    for (int s = 0; s < 2; s++) { sWidth[s] = (int)R(200); sHeight[s] = 10 + (int)R(30); sBits[s] = 0; }
    sDisabled = sFocus = sDialogKeys = sDefault = sParentNull = false;
    sFlags = 0;
    char aLine[100];
    snprintf(aLine, sizeof(aLine), "start %d %d %d %d", sWidth[0], sHeight[0], sWidth[1], sHeight[1]);
    Trace(aLine);
    if (inRust) {
        sSizeChanged = RustSizeChanged;
        EeWonWonButton* t0 = ee_won_wonbtn_new((void*)kComp0, ScriptedHost);
        EeWonWonButton* t1 = ee_won_wonbtn_new((void*)kComp1, ScriptedHost);
        sRust[0] = t0; sRust[1] = t1;
        for (int i = 0; i < inCalls; i++) Run(true);
        Trace("free");
        ee_won_wonbtn_free(t0);
        ee_won_wonbtn_free(t1);
        sRust[0] = sRust[1] = NULL;
    } else {
        sSizeChanged = OrigSizeChanged;
        sOrig[0] = new orig::WONButton(0);
        sOrig[1] = new orig::WONButton(1);
        for (int i = 0; i < inCalls; i++) Run(false);
        Trace("free");
        orig::sSide = 0; delete sOrig[0];
        orig::sSide = 1; delete sOrig[1];
        sOrig[0] = sOrig[1] = NULL;
    }
    Trace(RefsLine().c_str());
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kSequences = 20000;
    const int kCallsPerSeq = aPrint ? 20 : 40;
    size_t aLines = 0;
    std::set<std::string> aSeen;
    int aDiffering = 0, aLeaks = 0;
    for (int seq = 0; seq < kSequences; seq++) {
        sOpsMet.clear();
        std::vector<std::string> a = RunSide(false, (unsigned)seq, kCallsPerSeq);
        std::vector<std::string> b = RunSide(true, (unsigned)seq, kCallsPerSeq);
        aLines += a.size();
        // every reference back: the fonts and images at the world's one, no text alive, no use after free
        std::string aEnd = b.back();
        bool aClean = aEnd.compare(0, 5, "refs ") == 0 && aEnd.find(":2") == std::string::npos &&
                      aEnd.find(" 1000:") == std::string::npos;
        for (size_t i = 0; i < b.size(); i++) if (b[i].compare(0, 5, "DEAD ") == 0) aClean = false;
        for (std::map<intptr_t, int>::const_iterator i = sRefs.begin(); i != sRefs.end(); ++i)
            if (i->first >= 1000 || i->second != 1) aClean = false;
        if (!aClean) aLeaks++;
        CHECK(aClean);
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
        printf("won_wonbtnobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d unclean, "
               "%d checks, %d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, aLeaks,
               sChecks, sFailures);
    return sFailures;
}
