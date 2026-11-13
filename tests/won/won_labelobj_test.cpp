// Label as a whole object (R6 WON round 22): the original Label.cpp's methods, the statements copied from the
// C++ before the port with every call on a C++ object (the base Component, the wrapped text, the background, the
// Graphics) replaced by the host op that makes it (GuiHost.cpp runs the same calls in the same order), against
// ee_won's Rust Label (labelobj.rs) over one scripted host. The host keeps the size and a wrapped text whose width
// and height follow its text, wrap, centring and maximum width; SetSize calls the label's size change back (the
// base class's Virtual08), as the C++ component does when the size changes.
//
// Each of 20,000 sequences creates the label and makes 30 random inward calls (texts, alignments, wrap, fit to
// text, pads, colours, size changes, desired sizes, paints). The traces (every op with its arguments and answer,
// the draws) must be the same. The first 40 sequences and every later one that makes an op or a call none before
// made are rust/crates/ee_won/src/golden_labelobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include "rust/ee_won.h"

typedef EeWonGuiOp Op;
typedef EeWonGuiField Fd;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

// the host: the component's size and the wrapped text
static int sWidth, sHeight;
static int sTextLength, sMaxWidth, sFontHeight;
static bool sWrap, sCentered;
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
    case Op::WrapSetMaxWidth: sMaxWidth = (int)a; break;
    case Op::WrapSetCentered: sCentered = a != 0; break;
    case Op::WrapSetWrap: sWrap = a != 0; break;
    case Op::WrapWidth: r = TextWidth(); break;
    case Op::WrapHeight: r = TextHeight(); break;
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

// ---- the original (Label.cpp; the base Component, the wrapped text, the background and the Graphics through the
// host)

namespace orig {
static const intptr_t kG = 0x6;
enum { HorzAlign_Left = 0, HorzAlign_Center = 1, VertAlign_Top = 0 };
struct Label {
    int mColor, mAlign38, mAlign3C, mLeftPad, mRightPad, mTopPad, mBottomPad;
    bool mAdjustHeightToFitText;
    Label()
    {
        this->mColor = (int)0x80000005;
        this->mAlign38 = HorzAlign_Left;
        this->mAlign3C = VertAlign_Top;
        this->mLeftPad = this->mRightPad = this->mTopPad = this->mBottomPad = 0;
        this->mAdjustHeightToFitText = false;
    }
    int Width() { return sWidth; }
    int Height() { return sHeight; }
    void SetMaxWidth()
    {
        int width = this->Width() - this->mLeftPad - this->mRightPad;
        if (width < 0) width = 5;
        H(Op::WrapSetMaxWidth, width);
        this->AdjustHeightToFitText();
    }
    void Virtual08()
    {
        H(Op::BaseSizeChanged);
        this->SetMaxWidth();
    }
    void SetHorzAlign(int align)
    {
        this->mAlign38 = align;
        H(Op::WrapSetCentered, align == HorzAlign_Center);
    }
    void SetText(bool invalidate) // after the wrapped text's SetText
    {
        this->AdjustHeightToFitText();
        if (invalidate) H(Op::Invalidate);
    }
    void AdjustHeightToFitText()
    {
        if (!this->mAdjustHeightToFitText) return;
        if (this->Height() - this->mTopPad - this->mBottomPad != (int)H(Op::WrapHeight)) {
            int w = this->Width();
            H(Op::SetSize, w, (int)H(Op::WrapHeight) + this->mTopPad + this->mBottomPad);
        }
    }
    void SetAdjustHeightToFitText(bool adjust)
    {
        if (adjust == this->mAdjustHeightToFitText) return;
        this->mAdjustHeightToFitText = adjust;
        this->AdjustHeightToFitText();
    }
    void SetWrap(bool wrap)
    {
        H(Op::WrapSetWrap, wrap);
        this->AdjustHeightToFitText();
    }
    void GetDesiredSize(int& width, int& height)
    {
        width = (int)H(Op::WrapWidth) + this->mLeftPad + this->mRightPad;
        height = (int)H(Op::WrapHeight) + this->mTopPad + this->mBottomPad;
    }
    void Paint()
    {
        H(Op::BasePaint, kG);
        H(Op::PaintBackground, kG, this->Width(), this->Height());
        int pads[4] = {this->mLeftPad, this->mTopPad, this->mRightPad, this->mBottomPad};
        int textWidth = (int)H(Op::WrapWidth);
        int textHeight = (int)H(Op::WrapHeight);
        EeWonDrawOp ops[16];
        size_t n = ee_won_gui_draw_label(this->Width(), this->Height(), pads, this->mAlign38, this->mAlign3C, textWidth,
                                         textHeight, this->mColor, ops, 16);
        for (size_t i = 0; i < n && i < 16; i++) H(Op::Draw, kG, (intptr_t)&ops[i]);
    }
};
} // namespace orig

static orig::Label* sOrig;
static const EeWonLabel* sRust;
static void OrigSizeChanged() { sOrig->Virtual08(); }
static void RustSizeChanged() { ee_won_label_call(sRust, EeWonLabelCall::SizeChanged, 0, 0, 0); }

enum { kText, kHorz, kVert, kWrap, kAdjust, kPads, kColor, kSize, kDesired, kPaint, kGetColor, kCalls };
static const char* kCallNames[kCalls] = {"text", "horz", "vert", "wrap", "adjust", "pads", "color", "size", "desired",
                                         "paint", "get-color"};
static unsigned sDriverSeed;
static unsigned DR(unsigned n) { sDriverSeed = sDriverSeed * 69069u + 1u; return n ? (sDriverSeed >> 16) % n : 0; }

static void Run(bool inRust)
{
    static const int kWeights[kCalls] = {6, 2, 2, 3, 3, 2, 1, 4, 2, 4, 1};
    int aTotal = 0;
    for (int i = 0; i < kCalls; i++) aTotal += kWeights[i];
    int r0 = (int)DR(aTotal), k = 0;
    while (r0 >= kWeights[k]) r0 -= kWeights[k++];
    int a = 0, b = 0;
    switch (k) {
    case kText: a = (int)DR(40); b = (int)DR(2); break;
    case kHorz: case kVert: a = (int)DR(3); break;
    case kWrap: case kAdjust: a = (int)DR(2); break;
    case kPads: a = (int)DR(8); b = (int)DR(8); break;
    case kColor: a = (int)DR(0x1000000); break;
    case kSize: a = (int)DR(200) - 10; b = 5 + (int)DR(60); break;
    default: break;
    }
    char aLine[100];
    snprintf(aLine, sizeof(aLine), "call %s %d %d", kCallNames[k], a, b);
    sTrace.push_back(aLine);
    size_t aCallLine = sTrace.size() - 1;
    intptr_t r = 0;
    if (k == kText) sTextLength = a; // the C++ sets the wrapped text first
    if (k == kSize) { sWidth = a; sHeight = b; }
    if (inRust) {
        typedef EeWonLabelCall C;
        const EeWonLabel* t = sRust;
        switch (k) {
        case kText: ee_won_label_call(t, C::TextSet, b, 0, 0); break;
        case kHorz: ee_won_label_call(t, C::SetHorzAlign, a, 0, 0); break;
        case kVert: ee_won_label_call(t, C::SetVertAlign, a, 0, 0); break;
        case kWrap: ee_won_label_call(t, C::SetWrap, a, 0, 0); break;
        case kAdjust: ee_won_label_call(t, C::SetAdjustHeightToFitText, a, 0, 0); break;
        case kPads: ee_won_label_call(t, C::SetVertPad, a, b, 0); break;
        case kColor: ee_won_label_call(t, C::SetTextColor, a, 0, 0); break;
        case kSize: ee_won_label_call(t, C::SizeChanged, 0, 0, 0); break;
        case kDesired: { int w = 0, h = 0; ee_won_label_call(t, C::DesiredSize, (intptr_t)&w, (intptr_t)&h, 0); r = w * 1000 + h; break; }
        case kPaint: ee_won_label_call(t, C::Paint, orig::kG, 0, 0); break;
        case kGetColor: r = ee_won_label_call(t, C::TextColor, 0, 0, 0); break;
        }
    } else {
        orig::Label* t = sOrig;
        switch (k) {
        case kText: t->SetText(b != 0); break;
        case kHorz: t->SetHorzAlign(a); break;
        case kVert: t->mAlign3C = a; break;
        case kWrap: t->SetWrap(a != 0); break;
        case kAdjust: t->SetAdjustHeightToFitText(a != 0); break;
        case kPads: t->mTopPad = a; t->mBottomPad = b; break;
        case kColor: t->mColor = a; break;
        case kSize: t->Virtual08(); break;
        case kDesired: { int w = 0, h = 0; t->GetDesiredSize(w, h); r = w * 1000 + h; break; }
        case kPaint: t->Paint(); break;
        case kGetColor: r = t->mColor; break;
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
    sWrap = false; // the constructor's SetWrap(false)
    sCentered = false;
    char aLine[100];
    snprintf(aLine, sizeof(aLine), "start %d %d %d", sWidth, sHeight, sFontHeight);
    sTrace.push_back(aLine);
    if (inRust) {
        sSizeChanged = RustSizeChanged;
        EeWonLabel* t = ee_won_label_new((void*)0x53, ScriptedHost);
        sRust = t;
        for (int i = 0; i < inCalls; i++) Run(true);
        sTrace.push_back("free");
        ee_won_label_free(t);
        sRust = NULL;
    } else {
        sSizeChanged = OrigSizeChanged;
        sOrig = new orig::Label;
        for (int i = 0; i < inCalls; i++) Run(false);
        sTrace.push_back("free");
        delete sOrig;
        sOrig = NULL;
    }
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kSequences = 20000;
    const int kCalls = aPrint ? 16 : 30;
    size_t aLines = 0;
    std::set<std::string> aSeen;
    int aDiffering = 0;
    for (int seq = 0; seq < kSequences; seq++) {
        sOpsMet.clear();
        std::vector<std::string> a = RunSide(false, (unsigned)seq, kCalls);
        std::vector<std::string> b = RunSide(true, (unsigned)seq, kCalls);
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
        printf("won_labelobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
