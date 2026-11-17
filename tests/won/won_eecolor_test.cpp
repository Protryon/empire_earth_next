// The game room's colour button and colour chooser as Rust objects (R6 WONLobby round 6): the original
// EELobbyColorButton.cpp (the statements copied from the C++ before the port, every call on a component, a graphics
// context, the root window or the lobby replaced by the host op that makes it: EELobbyGlueHost.cpp runs the same
// calls) against ee_won's eecolor.rs over one scripted host; the lobby game is a real ee_won game on both sides (its
// taken colours; the original's GetTakenColors was the C++ forwarder's call).
//
// Each of 40,000 sequences makes a button and its chooser (the chooser's Rust object answers the button's
// ChooserNew), seats players with colours in the game, then makes 1 to 12 random calls (SetColor, the paints, the
// activation, the end of the popup and other events, the chooser's AddedToParent and mouse, the fields) and frees
// both. The host answers every op from a seeded generator (the same stream for both sides). Closing a popup ends it
// (the button's end-of-popup event), opening one adds the chooser to its parent, and the colour change the button
// fires sets its colour again, each half of the time (two deep at most). The traces must be the same.
//
// The first 60 sequences and every later one that makes an op or call none before made are
// rust/crates/ee_won_lobby/src/golden_eecolor.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include "rust/won_lobby/eecolor.h"
#include "rust/won_lobby/eegame.h"

typedef EeWonEeColorOp Op;
typedef EeWonEeColorCall Call;
typedef EeWonEeGameCall GC;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sSeed) % n; }
static std::vector<std::string> sTrace;
static std::set<std::string> sSeen;

static void Line(const char* inFormat, long long a = 0, long long b = 0, long long c = 0, long long d = 0,
                 long long e = 0, long long f = 0, long long g = 0)
{
    char aBuffer[240];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c, d, e, f, g);
    sTrace.push_back(aBuffer);
}

static const intptr_t kButton = 0xB00, kChooser = 0xC00, kGraphics = 0x6A, kEvent = 0xE1, kGame = 0x3000;
static bool sRust;
static EeWonEeColor* sRustButton;
static EeWonEeColor* sRustChooser;
static EeWonEeGame* sGame;
static int sDepth;
static intptr_t OrigButtonCall(Call inCall, intptr_t a, intptr_t b);
static intptr_t OrigChooserCall(Call inCall, intptr_t a, intptr_t b);

static intptr_t ButtonCall(Call inCall, intptr_t a, intptr_t b)
{
    return sRust ? ee_won_eecolor_call(sRustButton, inCall, a, b, 0) : OrigButtonCall(inCall, a, b);
}
static intptr_t ChooserCall(Call inCall, intptr_t a, intptr_t b)
{
    return sRust ? ee_won_eecolor_call(sRustChooser, inCall, a, b, 0) : OrigChooserCall(inCall, a, b);
}

static void Reenter(int inWhat)
{
    if (sDepth >= 2 || R(2) != 0) return;
    sDepth++;
    intptr_t r = 0;
    switch (inWhat) {
    case 0: Line("reenter end"); r = ButtonCall(Call::Event, kEvent, 1); break;
    case 1: Line("reenter added"); r = ChooserCall(Call::AddedToParent, 0, 0); break;
    default: {
        intptr_t c = (intptr_t)R(18) - 1;
        Line("reenter color %lld", c);
        r = ButtonCall(Call::SetColor, c, 0);
        break;
    }
    }
    Line("back %lld", r);
    sDepth--;
}

static intptr_t Host(void* inObj, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e, intptr_t f)
{
    intptr_t r = 0;
    switch (inOp) {
    case Op::ChooserRust: return (intptr_t)sRustChooser; // a field read, not traced
    case Op::GameRust: return (intptr_t)sGame;           // a field read, not traced
    case Op::LocalToRoot: {
        // the coordinates, not the stack addresses
        int* x = reinterpret_cast<int*>(b);
        int* y = reinterpret_cast<int*>(c);
        int x2 = *x + (int)R(400) - 100;
        int y2 = *y + (int)R(300);
        Line("op %lld %lld x %lld y %lld = %lld %lld", (long long)inOp, a, *x, *y, x2, y2);
        *x = x2;
        *y = y2;
        return 0;
    }
    case Op::Width: case Op::Height: r = R(4) ? (intptr_t)R(130) : (intptr_t)R(5) - 2; break;
    case Op::IsDisabled: case Op::IsPressed: r = R(3) == 0; break;
    case Op::ChooserNew: r = kChooser; break;
    case Op::Parent: r = R(2) ? 0x77 : 0; break;
    case Op::MyGame: r = R(4) ? kGame : 0; break;
    default: break;
    }
    Line("op %lld %lld %lld %lld %lld %lld %lld", (long long)inOp, a, b, c, d, e, f);
    sTrace.back() += " = " + std::to_string((long long)r);
    (void)inObj;
    if (inOp == Op::ClosePopup) Reenter(0);
    if (inOp == Op::OpenPopup) Reenter(1);
    if (inOp == Op::FireColorChanged) Reenter(2);
    return r;
}

static intptr_t O(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0, intptr_t e = 0, intptr_t f = 0)
{
    return Host(NULL, inOp, a, b, c, d, e, f);
}

static intptr_t GameHost(void*, EeWonEeGameOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    if (inOp == EeWonEeGameOp::NumMapTypes) return 12; // read by a new game (untraced; the Rust tests answer 12)
    intptr_t r = R(4);
    Line("gop %lld %lld %lld %lld %lld", (long long)inOp, a, b, c, d);
    sTrace.back() += " = " + std::to_string((long long)r);
    return r;
}

// ---- the original classes, every call on another object replaced by its op

// gEELobbyColors (the palette: the test never changes it)
static const unsigned long kColors[16] = {
    0x0000ff, 0xff0000, 0xffff00, 0xff00ff, 0xff8800, 0x008800, 0x00ffff, 0x880000,
    0x888888, 0x000088, 0xbbbbbb, 0xdddddd, 0xffff88, 0x88ff88, 0x8800ff, 0xff8888,
};

static struct OrigButton {
    int mColor;
    intptr_t mChooser; // SmartPtr<EELobbyColorChooser>
} sB;
static struct OrigChooser {
    int mHover, mSelected;
    bool mTaken[16];
} sC;

static bool OrigSetColor(int inColor)
{
    if (sB.mColor != inColor && inColor >= 0 && inColor < 16) {
        sB.mColor = inColor;
        O(Op::Invalidate, kButton);
        return true;
    }
    return false;
}

static void OrigButtonPaint(intptr_t inGraphics)
{
    O(Op::BasePaint, kButton, inGraphics);
    O(Op::SetColor, inGraphics, (intptr_t)kColors[sB.mColor]);
    intptr_t w = O(Op::Width, kButton);
    intptr_t h = O(Op::Height, kButton);
    O(Op::FillRect, inGraphics, 0, 0, w, h);
    if (!O(Op::IsDisabled, kButton)) {
        intptr_t w2 = O(Op::Width, kButton);
        intptr_t h2 = O(Op::Height, kButton);
        O(Op::Draw3DFrame, inGraphics, 0, 0, w2, h2, O(Op::IsPressed, kButton));
    }
}

static bool OrigHandleActivate()
{
    if (sB.mChooser == 0) {
        sB.mChooser = O(Op::ChooserNew); // this->mChooser = new EELobbyColorChooser (held)
        O(Op::SetDesiredSize, sB.mChooser, 0, 0);
        O(Op::AddListener, sB.mChooser, kButton);
    } else if (O(Op::Parent, sB.mChooser) != 0) {
        O(Op::ClosePopup, kButton, sB.mChooser);
        return false;
    }
    sC.mSelected = -1; // this->mChooser->mSelected = -1
    intptr_t w = O(Op::Width, kButton);
    intptr_t cw = O(Op::Width, sB.mChooser);
    int theX = ((int)w - (int)cw) / 2;
    int theY = (int)O(Op::Height, kButton);
    O(Op::LocalToRoot, kButton, (intptr_t)&theX, (intptr_t)&theY);
    // PopupParams theParams(chooser, 0x23e7, this, theX, theY); mAvoidRect...; GetRoot()->OpenPopup(theParams)
    O(Op::OpenPopup, kButton, sB.mChooser, theX, theY);
    return false;
}

static void OrigButtonEvent(intptr_t inEvent, bool inEndPopup)
{
    if (inEndPopup) {
        // this->mChooser->mSelected (the original read through NULL before its chooser was made, which no popup's
        // end can come before; the Rust reads -1 there, and so does this side)
        if (OrigSetColor(sB.mChooser != 0 ? sC.mSelected : -1))
            O(Op::FireColorChanged, kButton);
    } else
        O(Op::BaseEvent, kButton, inEvent);
}

static void OrigAddedToParent()
{
    O(Op::BaseAddedToParent, kChooser);
    sC.mHover = -1;
    intptr_t theGame = O(Op::MyGame);
    if (theGame != 0) // theGame->GetTakenColors(this->mTaken, -1): the forwarder's call
        ee_won_eegame_call(sGame, GC::GetTakenColors, (intptr_t)sC.mTaken, -1, 0, 0);
}

static void OrigMouseMove(int inX, int inY)
{
    bool theOutside = false;
    inX -= 8;
    if (inX < 0 || inX % 28 >= 20)
        theOutside = true;
    else
        inX /= 28;
    inY -= 8;
    if (inY < 0 || inY % 28 >= 20)
        theOutside = true;
    else
        inY /= 28;
    int theHover = theOutside ? -1 : inX + inY * 4;
    if (theHover != sC.mHover) {
        sC.mHover = theHover;
        O(Op::Invalidate, kChooser);
    }
}

static void OrigMouseDown()
{
    if (sC.mHover >= 0 && sC.mHover < 16 && !sC.mTaken[sC.mHover]) {
        sC.mSelected = sC.mHover;
        O(Op::ClosePopup, kChooser, kChooser);
    }
}

static void OrigMouseExit()
{
    O(Op::BaseMouseExit, kChooser);
    if (sC.mHover != -1) {
        sC.mHover = -1;
        O(Op::Invalidate, kChooser);
    }
}

static void OrigChooserPaint(intptr_t g)
{
    O(Op::BasePaint, kChooser, g);
    O(Op::SetMenuBackColor, g);
    intptr_t w = O(Op::Width, kChooser);
    intptr_t h = O(Op::Height, kChooser);
    O(Op::FillRect, g, 0, 0, w, h);
    intptr_t w2 = O(Op::Width, kChooser);
    intptr_t h2 = O(Op::Height, kChooser);
    O(Op::DrawFrame, g, 0, 0, w2, h2, 0);
    int theIndex = 0;
    int theY = 8;
    for (int theRow = 0; theRow < 4; theRow++) {
        int theX = 8;
        for (int theColumn = 0; theColumn < 4; theColumn++) {
            if (!sC.mTaken[theIndex]) {
                O(Op::SetColor, g, (intptr_t)kColors[theIndex]);
                O(Op::FillRect, g, theX, theY, 20, 20);
                O(Op::SetColor, g, 0x888888);
                O(Op::DrawRect, g, theX, theY, 20, 20);
                if (theIndex == sC.mHover)
                    O(Op::Draw3DFrame, g, theX, theY, 20, 20, 0);
            }
            theX += 28;
            theIndex++;
        }
        theY += 28;
    }
}

static intptr_t OrigButtonCall(Call inCall, intptr_t a, intptr_t b)
{
    switch (inCall) {
    case Call::SetColor: return OrigSetColor((int)a);
    case Call::GetColor: return sB.mColor;
    case Call::Paint: OrigButtonPaint(a); return 0;
    case Call::Activate: return OrigHandleActivate();
    case Call::Event: OrigButtonEvent(a, b != 0); return 0;
    case Call::Field: return a == 0 ? sB.mChooser : 0;
    default: return 0;
    }
}

static intptr_t OrigChooserCall(Call inCall, intptr_t a, intptr_t b)
{
    switch (inCall) {
    case Call::Paint: OrigChooserPaint(a); return 0;
    case Call::AddedToParent: OrigAddedToParent(); return 0;
    case Call::MouseMove: OrigMouseMove((int)a, (int)b); return 0;
    case Call::MouseDown: OrigMouseDown(); return 0;
    case Call::MouseExit: OrigMouseExit(); return 0;
    case Call::SetSelected: sC.mSelected = (int)a; return 0;
    case Call::Field:
        if (a == 0) return sC.mHover;
        if (a == 1) return sC.mSelected;
        if (a >= 2 && a <= 17) return sC.mTaken[a - 2];
        return 0;
    default: return 0;
    }
}

// ---- the sequences

struct Step { bool mChooser; Call mCall; intptr_t a, b; };
struct Setup { int mSlot, mOption; intptr_t mValue; };

static std::vector<std::string> RunSide(bool inRust, unsigned inSeed, const std::vector<Step>& inSteps,
                                        const std::vector<Setup>& inSetup)
{
    sTrace.clear();
    sSeed = inSeed;
    sRust = inRust;
    sDepth = 0;
    sGame = ee_won_eegame_new((void*)0x6000, GameHost);
    for (size_t i = 0; i < inSetup.size(); i++) {
        const Setup& u = inSetup[i];
        Line("gslot %lld %lld %lld", u.mSlot, u.mOption, u.mValue);
        ee_won_eegame_call(sGame, GC::SlotSetRawOption, u.mSlot, u.mOption, u.mValue, 0);
    }
    if (inRust) {
        sRustButton = ee_won_eecolor_new((void*)kButton, EeWonEeColorKind::Button, Host);
        sRustChooser = ee_won_eecolor_new((void*)kChooser, EeWonEeColorKind::Chooser, Host);
    } else {
        sB.mColor = 0;
        sB.mChooser = 0;
        sC.mHover = -1;
        sC.mSelected = -1;
        memset(sC.mTaken, 0, sizeof(sC.mTaken));
    }
    for (size_t i = 0; i < inSteps.size(); i++) {
        const Step& s = inSteps[i];
        Line("call %lld %lld %lld %lld", s.mChooser ? 1 : 0, (long long)s.mCall, s.a, s.b);
        intptr_t r = s.mChooser ? ChooserCall(s.mCall, s.a, s.b) : ButtonCall(s.mCall, s.a, s.b);
        Line("ret %lld", r);
    }
    Line("free");
    if (inRust) {
        ee_won_eecolor_free(sRustButton);
        ee_won_eecolor_free(sRustChooser);
    } else if (sB.mChooser != 0)
        O(Op::Release, sB.mChooser); // the button's SmartPtr
    Line("gfree");
    ee_won_eegame_free(sGame);
    sGame = NULL;
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    // the palette is the original table
    for (unsigned i = 0; i < 16; i++) CHECK(ee_won_eecolor_palette(i) == kColors[i]);
    unsigned long long aLines = 0;
    int aDiffer = 0;
    unsigned aStepSeed = 4711;
    const int kSequences = 40000;
    for (int n = 0; n < kSequences; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        std::vector<Setup> aSetup;
        for (int i = 0; i < 8; i++) {
            if (R(2)) {
                aSetup.push_back(Setup{i, 0, R(3) ? 3 : (intptr_t)R(5)});
                aSetup.push_back(Setup{i, 2, R(5) ? (intptr_t)R(16) : (intptr_t)R(40)});
                if (R(4) == 0) aSetup.push_back(Setup{i, 4, 1});
            }
        }
        std::vector<Step> aSteps;
        int aCount = 1 + (int)R(12);
        for (int i = 0; i < aCount; i++) {
            Step s = {false, Call::SetColor, (intptr_t)R(18) - 1, 0};
            switch (R(12)) {
            case 0: s.mCall = Call::Paint; s.a = kGraphics; break;
            case 1: case 2: s.mCall = Call::Activate; s.a = 0; break;
            case 3: s.mCall = Call::Event; s.a = kEvent; s.b = R(2); break;
            case 4: s.mCall = R(2) ? Call::GetColor : Call::Field; s.a = R(2); break;
            case 5: s.mChooser = true; s.mCall = Call::AddedToParent; s.a = 0; break;
            case 6: case 7: s.mChooser = true; s.mCall = Call::MouseMove;
                s.a = (intptr_t)R(140) - 10; s.b = (intptr_t)R(140) - 10; break;
            case 8: s.mChooser = true; s.mCall = Call::MouseDown; s.a = R(3); s.b = R(3); break;
            case 9: s.mChooser = true; s.mCall = R(3) ? Call::MouseExit : Call::Paint; s.a = kGraphics; break;
            case 10: s.mChooser = true; s.mCall = Call::Field; s.a = R(19); break;
            default: break;
            }
            aSteps.push_back(s);
        }
        aStepSeed = sSeed;
        sSeed = aSave;
        unsigned aSeed = 101u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunSide(false, aSeed, aSteps, aSetup);
        std::vector<std::string> b = RunSide(true, aSeed, aSteps, aSetup);
        aLines += a.size();
        sChecks++;
        if (a != b) {
            if (++aDiffer <= 3) {
                printf("sequence %d differs\n", n);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    const char* x = i < a.size() ? a[i].c_str() : "-";
                    const char* y = i < b.size() ? b[i].c_str() : "-";
                    if (strcmp(x, y) != 0) { printf("  line %zu: C++ '%s' Rust '%s'\n", i, x, y); break; }
                }
            }
            continue;
        }
        bool aNew = n < 60;
        for (size_t i = 0; i < a.size(); i++) {
            size_t aSpace = a[i].find(' ');
            std::string aHead = a[i].substr(0, aSpace);
            std::string aKey = aHead == "ret" || aHead == "back" ? aHead : a[i].substr(0, a[i].find(' ', aSpace + 1));
            if (sSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && aNew) {
            printf("seq %d\n", n);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (aDiffer) { sFailures++; printf("FAIL: %d of %d sequences differ\n", aDiffer, kSequences); }
    if (!aPrint)
        printf("won_eecolor_test: %d sequences, %llu trace lines, %zu distinct ops and calls; %d checks, %d failures\n",
               kSequences, aLines, sSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
