// The credits (R6 WONLobby round 3): the original CreditsDialog.cpp's ScrollingTextComponent (AddLine, TimerEvent,
// Paint, Start, Stop, Reset, the destructor's lines) against ee_won's scrolltext.rs, and its CreditsDialog
// (InitComponent, AddLine, the scrolling's start, stop and reset, the destructor's members) against lobbydlg.rs's
// kind Credits; the statements copied from the C++ before the port, every call on a component, a Graphics or the
// dialog replaced by the host op that makes it (CreditsDialog.cpp's and LobbyDialogHost.cpp's hosts run the same
// calls), over scripted hosts.
//
// 20,000 scrolling sequences: AddLine with and without a colour, timer ticks around the 60 ms period, paints over
// heights from 3 to 300, fonts 1 to 40 high, the scroll top above, inside and below the box (a long run of ticks
// scrolls the text out and back), Start, Stop, Reset; the host's Graphics answers its colour with the last one set.
// 20,000 dialog sequences: InitComponent, then 10 random calls (AddLine on components narrower and wider than the
// line, the scrolling's calls, events, a second InitComponent now and then). The traces must be the same.
//
// `--print-scroll` and `--print-dialog` print rust/crates/ee_won_lobby/src/golden_scrolltext.txt and golden_credits.txt
// (the first 20 scrolling and 60 dialog sequences and every later one that makes an op or call none before made),
// replayed by cargo.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/scrolltext.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sSeed) % n; }
static std::vector<std::string> sTrace;

static void Line(const char* inFormat, long long a = 0, long long b = 0, long long c = 0, long long d = 0,
                 long long e = 0, long long f = 0, long long g = 0, long long h = 0)
{
    char aBuffer[200];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c, d, e, f, g, h);
    sTrace.push_back(aBuffer);
}

// ======== the scrolling text

typedef EeWonScrollTextOp SOp;
typedef EeWonScrollTextCall SCall;

static int sHeight, sWidth, sFontHeight; // the box and the font of this sequence (the host's answers)
static unsigned sColor;                  // the Graphics' colour: the last SetColor
static intptr_t sNextLine;

static intptr_t SHost(SOp inOp, intptr_t a, intptr_t b, int x, int y, int w, int h)
{
    intptr_t r = 0;
    switch (inOp) {
    case SOp::Width: r = sWidth; break;
    case SOp::Height: r = sHeight; break;
    case SOp::LineNew: r = sNextLine; sNextLine += 16; break;
    case SOp::SetColor: sColor = (unsigned)x; break;
    case SOp::SysHilight: sColor = 0xc0c0c0; break;
    case SOp::SysShadow: sColor = 0x808080; break;
    case SOp::FontHeight: r = sFontHeight; break;
    case SOp::Color: r = sColor; break;
    case SOp::StringWidth: r = R(400); break;
    default: break;
    }
    Line("op %lld %lld %lld %lld %lld %lld %lld = %lld", (long long)inOp, a, b, x, y, w, h, r);
    return r;
}

// the original's members and statements
struct OrigScroll {
    std::vector<std::pair<intptr_t, unsigned>> mLines; // CreditsList: each line's GUIString and colour
    unsigned mLastColor;
    int mTimeSinceLastScroll, mScrollSpeed, mScrollTop, mTotalTextHeight;
};
static OrigScroll sOS;
static int OH() { return (int)SHost(SOp::Height, 0, 0, 0, 0, 0, 0); } // this->Height()
static int OW() { return (int)SHost(SOp::Width, 0, 0, 0, 0, 0, 0); }  // this->Width()

// DimColor, copied
static unsigned DimColor(unsigned color, int percent)
{
    if (percent <= 0) return 0xffffff;
    if (percent >= 100) return color;
    int red = (color >> 16) & 255, green = (color >> 8) & 255, blue = color & 255;
    red = 255 - ((255 - red) * percent / 100);
    green = 255 - ((255 - green) * percent / 100);
    blue = 255 - ((255 - blue) * percent / 100);
    return (red << 16) | (green << 8) | blue;
}

static void OrigReset() { sOS.mScrollTop = OH(); }

static void OrigPaint(intptr_t g)
{
    SHost(SOp::BasePaint, g, 0, 0, 0, 0, 0);           // Component::Paint(g)
    SHost(SOp::SetDefaultFont, g, 0, 0, 0, 0, 0);      // g.SetFont(this->GetDefaultFont())
    SHost(SOp::SetColor, g, 0, 0xffffff, 0, 0, 0);     // g.SetColor(0xffffff)
    { int w = OW() - 4; int h = OH() - 4; SHost(SOp::FillRect, g, 0, 2, 2, w, h); }
    SHost(SOp::SysHilight, g, 0, 0, 0, 0, 0);
    { int w = OW(); int h = OH(); SHost(SOp::DrawRect, g, 0, 0, 0, w, h); }
    { int w = OW() - 1; int h = OH() - 1; SHost(SOp::DrawRect, g, 0, 1, 1, w, h); }
    SHost(SOp::SysShadow, g, 0, 0, 0, 0, 0);
    { int w = OW() - 1; int h = OH() - 1; SHost(SOp::DrawRect, g, 0, 0, 0, w, h); }
    int top = sOS.mScrollTop;
    for (size_t i = 0; i < sOS.mLines.size(); i++) {
        intptr_t aLine = sOS.mLines[i].first;
        unsigned aColor = sOS.mLines[i].second;
        int height = (int)SHost(SOp::FontHeight, g, 0, 0, 0, 0, 0);
        unsigned c;
        if (top < OH() / 3) c = DimColor(aColor, top * 100 / (OH() / 3));
        else if (OH() - top <= height) c = 0xffffff;
        else if (top > (OH() * 2 / 3) - height) { int below = OH() - (top + height); c = DimColor(aColor, below * 100 / (OH() / 3)); }
        else c = aColor;
        SHost(SOp::SetColor, g, 0, (int)c, 0, 0, 0);
        if ((unsigned)SHost(SOp::Color, g, 0, 0, 0, 0, 0) != 0xffffff && top > 2 && top < OH() - height - 2) {
            int width = (int)SHost(SOp::StringWidth, g, aLine, 0, 0, 0, 0);
            int x = (OW() - width) / 2;
            SHost(SOp::DrawString, g, aLine, x, top, 0, 0);
        }
        top += height;
    }
    sOS.mTotalTextHeight = top - sOS.mScrollTop;
}

static intptr_t OrigScrollCall(SCall inCall, intptr_t a, intptr_t b)
{
    switch (inCall) {
    case SCall::AddLine:
        if ((unsigned)b != (unsigned)-1) sOS.mLastColor = (unsigned)b;
        sOS.mLines.push_back(std::make_pair(SHost(SOp::LineNew, a, 0, 0, 0, 0, 0), sOS.mLastColor));
        return 0;
    case SCall::Timer:
        sOS.mTimeSinceLastScroll += (int)a;
        if (!(sOS.mTimeSinceLastScroll >= sOS.mScrollSpeed)) return 1;
        if (--sOS.mScrollTop < -sOS.mTotalTextHeight) sOS.mScrollTop = OH();
        SHost(SOp::Invalidate, 0, 0, 0, 0, 0, 0);
        sOS.mTimeSinceLastScroll = 0;
        return 1;
    case SCall::Paint: OrigPaint(a); return 0;
    case SCall::Start: SHost(SOp::RequestTimer, 0, 0, 1, 0, 0, 0); OrigReset(); return 0;
    case SCall::Stop: SHost(SOp::RequestTimer, 0, 0, 0, 0, 0, 0); return 0;
    case SCall::Reset: OrigReset(); return 0;
    case SCall::Field:
        switch (a) {
        case 0: return sOS.mScrollTop;
        case 1: return sOS.mTotalTextHeight;
        case 2: return sOS.mTimeSinceLastScroll;
        case 3: return (intptr_t)sOS.mLastColor;
        case 4: return (intptr_t)sOS.mLines.size();
        case 5: return sOS.mScrollSpeed;
        }
        return 0;
    }
    return 0;
}

static intptr_t RustSHost(void*, SOp inOp, intptr_t a, intptr_t b, int x, int y, int w, int h)
{
    return SHost(inOp, a, b, x, y, w, h);
}

struct SStep { SCall mCall; intptr_t a, b; };

static std::vector<std::string> RunScroll(bool inRust, unsigned inSeed, const std::vector<SStep>& inSteps)
{
    sTrace.clear();
    sSeed = inSeed;
    sColor = 0;
    sNextLine = 0x3000;
    EeWonScrollText* aRust = NULL;
    if (inRust) aRust = ee_won_scrolltext_new((void*)0x5C0, RustSHost);
    else { sOS = OrigScroll(); sOS.mLastColor = 0; sOS.mTimeSinceLastScroll = 0; sOS.mScrollSpeed = 60; sOS.mScrollTop = 0; sOS.mTotalTextHeight = 0; }
    for (size_t i = 0; i < inSteps.size(); i++) {
        const SStep& s = inSteps[i];
        Line("call %lld %lld %lld", (long long)s.mCall, s.a, s.b);
        intptr_t r = inRust ? ee_won_scrolltext_call(aRust, s.mCall, s.a, s.b) : OrigScrollCall(s.mCall, s.a, s.b);
        Line("ret %lld", r);
    }
    Line("free");
    if (inRust) ee_won_scrolltext_free(aRust);
    else for (size_t i = 0; i < sOS.mLines.size(); i++) SHost(SOp::LineFree, sOS.mLines[i].first, 0, 0, 0, 0, 0);
    return sTrace;
}

// ======== the credits dialog (lobbydlg kind Credits)

typedef EeWonLobbyDlgOp Op;
typedef EeWonLobbyDlgCall Call;
static const intptr_t kConfig = 0x77, kEvent = 0xE1, kDialog = 0xD1A, kScroll = 0x5C0;

static intptr_t DHost(Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    intptr_t r = 0;
    if (inOp == Op::Find) {
        const EeWonLobbyDlgSlot* aSlot = reinterpret_cast<const EeWonLobbyDlgSlot*>(b);
        r = strcmp(aSlot->name, "CreditsButton") == 0 ? 0x1100 : 0x1200;
        sTrace.push_back("op 1 " + std::to_string((long long)a) + " " + aSlot->name + " " + aSlot->file + " " +
                        std::to_string(aSlot->line) + " " + std::to_string((int)aSlot->ty) + " = " + std::to_string((long long)r));
        return r;
    }
    switch (inOp) {
    case Op::Width: case Op::Height: r = 50 * (intptr_t)R(8); break;
    case Op::DefaultFontStringWidth: r = 40 * (intptr_t)R(10); break;
    default: break;
    }
    Line("op %lld %lld %lld %lld = %lld", (long long)inOp, a, b, c, r);
    return r;
}

struct OrigCredits { intptr_t mScrollingText, mCreditsButton, mCloseButton; };
static OrigCredits sOC;

static void OrigCreditsFind(intptr_t& ioMember, const EeWonLobbyDlgSlot* inSlot)
{
    intptr_t aNew = DHost(Op::Find, kConfig, (intptr_t)inSlot, 0);
    intptr_t anOld = ioMember;
    ioMember = aNew;
    if (anOld != 0) DHost(Op::Release, anOld, 0, 0);
}

static const EeWonLobbyDlgSlot* sCreditsSlots[2];

static intptr_t OrigCreditsCall(Call inCall, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inCall) {
    case Call::HoldScrollText: // this->mScrollingText=new ScrollingTextComponent (the constructor)
        DHost(Op::AddRef, a, 0, 0);
        sOC.mScrollingText = a;
        return 0;
    case Call::Init:
        OrigCreditsFind(sOC.mCreditsButton, sCreditsSlots[0]); // "CreditsButton", 271
        OrigCreditsFind(sOC.mCloseButton, sCreditsSlots[1]);   // "CancelButton", 272
        // aParent=this->mCreditsButton->mParent; this->Fn2F6140(mScrollingText,0x5003,mCreditsButton,aParent->Left(),aParent->Top(),0,0,0)
        DHost(Op::CreditsPlace, sOC.mScrollingText, sOC.mCreditsButton, 0);
        DHost(Op::AddChild, sOC.mScrollingText, 0, 0);
        DHost(Op::SetVisible, sOC.mCreditsButton, 0, 0);
        return 0; // UpdateCredits() is empty
    case Call::Event: DHost(Op::BaseEvent, a, 0, 0); return 0;
    case Call::Slot: return a == 0 ? sOC.mCreditsButton : a == 1 ? sOC.mCloseButton : 0;
    case Call::StartScrolling: DHost(Op::ScrollStart, sOC.mScrollingText, 0, 0); return 0;
    case Call::StopScrolling: DHost(Op::ScrollStop, sOC.mScrollingText, 0, 0); return 0;
    case Call::ResetScrolling: DHost(Op::ScrollReset, sOC.mScrollingText, 0, 0); return 0;
    case Call::AddLine: {
        int aWidth = (int)DHost(Op::DefaultFontStringWidth, a, 0, 0) + 20;
        if (aWidth > (int)DHost(Op::Width, b, 0, 0)) {
            int aHeight = (int)DHost(Op::Height, b, 0, 0);
            DHost(Op::SetSize, b, aWidth, aHeight);
        }
        DHost(Op::ScrollAddLine, sOC.mScrollingText, a, c);
        return 0;
    }
    default: return 0;
    }
}

static intptr_t RustDHost(void*, Op inOp, intptr_t a, intptr_t b, intptr_t c) { return DHost(inOp, a, b, c); }

struct DStep { Call mCall; intptr_t a, b, c; };

static std::vector<std::string> RunCredits(bool inRust, unsigned inSeed, const std::vector<DStep>& inSteps)
{
    sTrace.clear();
    sSeed = inSeed;
    EeWonLobbyDlg* aRust = NULL;
    if (inRust) aRust = ee_won_lobbydlg_new((void*)kDialog, EeWonLobbyDlgKind::Credits, RustDHost);
    else memset(&sOC, 0, sizeof(sOC));
    for (size_t i = 0; i < inSteps.size(); i++) {
        const DStep& s = inSteps[i];
        Line("call %lld %lld %lld %lld", (long long)s.mCall, s.a, s.b, s.c);
        intptr_t r = inRust ? ee_won_lobbydlg_call(aRust, s.mCall, s.a, s.b, s.c) : OrigCreditsCall(s.mCall, s.a, s.b, s.c);
        Line("ret %lld", r);
    }
    Line("free");
    if (inRust) ee_won_lobbydlg_free(aRust);
    else { // the members' destruction, in reverse order
        if (sOC.mCloseButton) DHost(Op::Release, sOC.mCloseButton, 0, 0);
        if (sOC.mCreditsButton) DHost(Op::Release, sOC.mCreditsButton, 0, 0);
        if (sOC.mScrollingText) DHost(Op::Release, sOC.mScrollingText, 0, 0);
    }
    return sTrace;
}

static const EeWonLobbyDlgSlot* sLearn[4];
static int sLearnCount;
static intptr_t LearnHost(void*, Op inOp, intptr_t, intptr_t b, intptr_t)
{
    if (inOp == Op::Find) { sLearn[sLearnCount] = reinterpret_cast<const EeWonLobbyDlgSlot*>(b); return 0x100 + sLearnCount++; }
    return 0;
}

// ======== main

static bool Compare(const char* inWhat, int n, const std::vector<std::string>& a, const std::vector<std::string>& b,
                    int& ioDiffer)
{
    if (a == b) return true;
    if (++ioDiffer <= 3) {
        printf("%s sequence %d differs\n", inWhat, n);
        for (size_t i = 0; i < a.size() || i < b.size(); i++) {
            const char* x = i < a.size() ? a[i].c_str() : "-";
            const char* y = i < b.size() ? b[i].c_str() : "-";
            if (strcmp(x, y) != 0) { printf("  line %zu: C++ '%s' Rust '%s'\n", i, x, y); break; }
        }
    }
    return false;
}

static void PrintIfNew(bool inPrint, int n, int inFirst, const char* inHead, const char* inTail,
                       const std::vector<std::string>& a, std::set<std::string>& ioSeen)
{
    bool aNew = n < inFirst;
    for (size_t i = 0; i < a.size(); i++) {
        // an op or call by its number (a return by its kind only: Field answers many values)
        std::string aKey = a[i].compare(0, 4, "ret ") == 0 ? std::string("ret") : a[i].substr(0, a[i].find(' ', a[i].find(' ') + 1));
        if (ioSeen.insert(aKey).second) aNew = true;
    }
    if (inPrint && aNew) {
        printf("%s %d%s\n", inHead, n, inTail);
        for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
    }
}

int main(int argc, char** argv)
{
    bool aPrintScroll = argc > 1 && strcmp(argv[1], "--print-scroll") == 0;
    bool aPrintDialog = argc > 1 && strcmp(argv[1], "--print-dialog") == 0;
    bool aQuiet = aPrintScroll || aPrintDialog;
    // the dialog's table, through a Rust dialog's look-ups, against the original's SafeGetComponent arguments
    {
        EeWonLobbyDlg* d = ee_won_lobbydlg_new(NULL, EeWonLobbyDlgKind::Credits, LearnHost);
        ee_won_lobbydlg_call(d, Call::Init, kConfig, 0, 0);
        CHECK(sLearnCount == 2);
        for (int m = 0; m < 2; m++) {
            intptr_t p = ee_won_lobbydlg_call(d, Call::Slot, m, 0, 0);
            sCreditsSlots[m] = p >= 0x100 && p < 0x100 + sLearnCount ? sLearn[p - 0x100] : NULL;
        }
        ee_won_lobbydlg_free(d);
        CHECK(sCreditsSlots[0] != NULL && strcmp(sCreditsSlots[0]->name, "CreditsButton") == 0 &&
              sCreditsSlots[0]->line == 271 && sCreditsSlots[0]->ty == EeWonLobbyDlgType::Button);
        CHECK(sCreditsSlots[1] != NULL && strcmp(sCreditsSlots[1]->name, "CancelButton") == 0 &&
              sCreditsSlots[1]->line == 272 && sCreditsSlots[1]->ty == EeWonLobbyDlgType::Button);
        if (sCreditsSlots[0] == NULL || sCreditsSlots[1] == NULL) { printf("no table\n"); return 1; }
    }
    unsigned long long aLines = 0;
    int aScrollDiffer = 0, aDialogDiffer = 0;
    std::set<std::string> aScrollSeen, aDialogSeen;
    unsigned aStepSeed = 2718;
    for (int n = 0; n < 20000; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        static const int kHeights[] = {3, 4, 9, 30, 100, 200, 300};
        sHeight = kHeights[R(7)];
        sWidth = 50 + (int)R(300);
        sFontHeight = 1 + (int)R(40);
        std::vector<SStep> aSteps;
        int aCount = 4 + (int)R(30);
        for (int i = 0; i < aCount; i++) {
            SStep s = {SCall::Paint, 0x6A0, 0};
            switch (R(10)) {
            case 0: case 1: s.mCall = SCall::AddLine; s.a = 0x2000 + 8 * (intptr_t)R(16);
                s.b = R(3) == 0 ? (intptr_t)(unsigned long)0xffffffffu : (intptr_t)(R(0x7fff) * 517u & 0xffffff); break;
            case 2: case 3: case 4: s.mCall = SCall::Timer; s.a = R(4) == 0 ? 6000 : (intptr_t)R(90); break;
            case 5: case 6: s.mCall = SCall::Paint; break;
            case 7: { static const SCall k[] = {SCall::Start, SCall::Stop, SCall::Reset}; s.mCall = k[R(3)]; break; }
            default: s.mCall = SCall::Field; s.a = R(7); break;
            }
            aSteps.push_back(s);
            // a run of ticks and paints: the text scrolls through the box
            if (R(8) == 0)
                for (int k = 0; k < 40; k++) { aSteps.push_back(SStep{SCall::Timer, 60, 0}); if (k % 8 == 0) aSteps.push_back(SStep{SCall::Paint, 0x6A0, 0}); }
        }
        aStepSeed = sSeed;
        sSeed = aSave;
        unsigned aSeed = 31u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunScroll(false, aSeed, aSteps);
        std::vector<std::string> b = RunScroll(true, aSeed, aSteps);
        aLines += a.size();
        sChecks++;
        if (Compare("scroll", n, a, b, aScrollDiffer)) PrintIfNew(aPrintScroll, n, 20, "sseq", "", a, aScrollSeen);
    }
    for (int n = 0; n < 20000; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        std::vector<DStep> aSteps;
        aSteps.push_back(DStep{Call::HoldScrollText, kScroll, 0, 0});
        aSteps.push_back(DStep{Call::Init, kConfig, 0, 0});
        for (int i = 0; i < 10; i++) {
            DStep s = {Call::AddLine, 0x2000 + 8 * (intptr_t)R(16), 0x4000 + 16 * (intptr_t)R(3), (intptr_t)R(3)};
            switch (R(8)) {
            case 0: if (R(3) == 0) { s.mCall = Call::Init; s.a = kConfig; s.b = s.c = 0; } break;
            case 1: s.mCall = Call::StartScrolling; s.a = s.b = s.c = 0; break;
            case 2: s.mCall = Call::StopScrolling; s.a = s.b = s.c = 0; break;
            case 3: s.mCall = Call::ResetScrolling; s.a = s.b = s.c = 0; break;
            case 4: s.mCall = Call::Event; s.a = kEvent; s.b = R(5); s.c = 0x1100; break;
            case 5: s.mCall = Call::Slot; s.a = R(3); s.b = s.c = 0; break;
            default: break;
            }
            aSteps.push_back(s);
        }
        aStepSeed = sSeed;
        sSeed = aSave;
        unsigned aSeed = 57u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunCredits(false, aSeed, aSteps);
        std::vector<std::string> b = RunCredits(true, aSeed, aSteps);
        aLines += a.size();
        sChecks++;
        if (Compare("dialog", n, a, b, aDialogDiffer)) PrintIfNew(aPrintDialog, n, 60, "seq", " kind 14", a, aDialogSeen);
    }
    if (aScrollDiffer) { sFailures++; printf("FAIL: %d of 20000 scroll sequences differ\n", aScrollDiffer); }
    if (aDialogDiffer) { sFailures++; printf("FAIL: %d of 20000 dialog sequences differ\n", aDialogDiffer); }
    if (!aQuiet)
        printf("won_credits_test: 40000 sequences, %llu trace lines, %zu + %zu distinct ops and calls; %d checks, %d failures\n",
               aLines, aScrollSeen.size(), aDialogSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
