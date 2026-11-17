// DialogLogic, StatusDialog and OkCancelDialog in Rust (R6 WONLobby round 2): the original DialogLogic.cpp,
// StatusDialog.cpp and OkCancelDialog.cpp (the statements copied from the C++ before the port, every call on
// another object replaced by the host op that makes it: DialogLogic.cpp's and LobbyDialogHost.cpp's hosts run the
// same calls) against ee_won's dlglogic.rs and lobbydlg.rs (kinds Status and OkCancel) over one scripted host.
//
// Part 1, DialogLogic: each of 20,000 sequences makes the logic, its InitComponent, 10 random calls (every method:
// the status waits, errors, OK/Cancel and Yes/No boxes, DoDialog of the two boxes and two other dialogs, the status
// operation's completion, UpdateStatus, ClearDialogs, KillHook, a second InitComponent) and frees it. The host
// answers every op from a seeded generator, the same stream for both sides; a window's modal loop (RunDialog)
// re-enters the logic with one random call a third of the time (two deep at most), and RunOp completes the
// operation at once a third of the time, as the real ones can.
// Part 2, the boxes: 20,000 sequences of a Status or OkCancel dialog's InitComponent and 8 calls (SetText,
// SetStatus, SetStartTick, events, the start tick).
//
// The traces (every op with its arguments and answer, every call's result, the releases at the end) must be the
// same. The first sequences and every later one that makes an op or call none before made are
// rust/crates/ee_won_lobby/src/golden_dlglogic.txt, which the Rust tests replay; `--print` prints them.
//
// The one deliberate change: RunOpWithStatus no longer reads the status operation after the box when a KillHook in
// the box's loop cleared it (the original read through NULL); both sides here skip it.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <set>
#include "rust/won_lobby/dialogs.h"
#include "rust/won_lobby/dlglogic.h"
#include "rust/won_lobby/lobbydlg.h"

typedef EeWonDlgLogicOp Op;
typedef EeWonDlgLogicCall Call;
typedef EeWonLobbyDlgOp DOp;
typedef EeWonLobbyDlgCall DCall;
typedef EeWonLobbyDlgKind Kind;

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
    char aBuffer[200];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c, d, e, f, g);
    sTrace.push_back(aBuffer);
}

// the test's world
static const intptr_t kConfig = 0x77;
static const intptr_t kOkDialog = 0x100;
static const intptr_t kStatusDialog = 0x200;
static const intptr_t kOtherDialogs[2] = {0x300, 0x310};
static const intptr_t kRoot = 0x600;
static const intptr_t kParent = 0x650;
static const intptr_t kRootWindow = 0x700;
static const intptr_t kOps[2] = {0x4000, 0x4100};
static const intptr_t kTitle = 0x51, kText = 0x52;
static int sNewWindows;
static int sDepth;

// ---- part 1: DialogLogic

// one side's entry: a call on that side's logic
typedef intptr_t (*EnterFn)(Call inCall, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e);
static EnterFn sEnter;

struct Step { Call mCall; intptr_t a, b, c, d, e; };

static Step RandomStep(bool inNested)
{
    Step s = {Call::ClearDialogs, 0, 0, 0, 0, 0};
    switch (R(inNested ? 12 : 14)) {
    case 0: s.mCall = Call::ShowStatus; s.a = kTitle; s.b = kText; s.c = R(2); break;
    case 1: s.mCall = Call::ShowError; s.a = kTitle; s.b = kText; break;
    case 2: s.mCall = Call::ShowOkCancel; s.a = kTitle; s.b = kText; s.c = R(2); break;
    case 3: s.mCall = Call::ShowYesNo; s.a = kTitle; s.b = kText; s.c = R(2); break;
    case 4: {
        s.mCall = Call::DoDialog;
        static const intptr_t kDialogs[] = {0x100, 0x200, 0x300, 0x310};
        s.a = kDialogs[R(4)];
        break;
    }
    case 5: s.mCall = Call::StatusOpComplete; s.a = kOps[R(2)]; break;
    case 6: s.mCall = Call::ClearDialogs; break;
    case 7: s.mCall = R(2) ? Call::KillHook : Call::Reset; break;
    case 8: { static const intptr_t kStatus[] = {0, 5, 100000}; s.mCall = Call::UpdateStatus; s.a = kStatus[R(3)]; break; }
    case 9: case 10:
        s.mCall = Call::RunOpWithStatus; s.a = kOps[R(2)]; s.b = kTitle; s.c = kText; s.d = R(3); s.e = 60000;
        break;
    case 11: s.mCall = Call::Field; s.a = R(8); break;
    default: s.mCall = Call::Init; s.a = kConfig; break;
    }
    return s;
}

static intptr_t H(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    intptr_t r = 0;
    int* aRect = NULL;
    switch (inOp) {
    case Op::FindOkCancel: r = R(8) ? kOkDialog : 0; break;
    case Op::FindStatus: r = R(8) ? kStatusDialog : 0; break;
    case Op::WmPopups: r = (intptr_t)R(3) - 1; break;
    case Op::OpGetStatus: { static const intptr_t kSt[] = {0, 100000, 5}; r = kSt[R(3)]; break; }
    case Op::LobbyRoot: r = R(8) ? kRoot : 0; break;
    case Op::RootWindow: r = R(8) ? kRootWindow : 0; break;
    case Op::Parent: r = R(2) ? kParent : 0; break;
    case Op::Width: case Op::Height: r = 100 + 20 * R(2); break;
    case Op::Bounds: aRect = reinterpret_cast<int*>(b); break;
    case Op::NewWindow: r = 0x800 + 16 * sNewWindows++; break;
    case Op::WindowRoot: r = R(8) ? a + 0x100000 : 0; break;
    case Op::RunDialog: r = (intptr_t)R(5) - 1; break;
    case Op::LobbySuspended: r = R(6) == 0; break;
    case Op::SoundEffects: r = R(2); break;
    case Op::RootWindowOf: r = R(6) ? 0xA00 : 0; break;
    default: break;
    }
    if (aRect != NULL) {
        for (int i = 0; i < 4; i++) aRect[i] = i < 2 ? (int)R(50) : 100 + 20 * (int)R(2);
        Line("op %lld %lld rect %lld %lld %lld %lld", (int)inOp, a, aRect[0], aRect[1], aRect[2], aRect[3]);
        return 0;
    }
    if (inOp == Op::InvalidateRect) {
        const int* p = reinterpret_cast<const int*>(b);
        Line("op %lld %lld rect %lld %lld %lld %lld", (int)inOp, a, p[0], p[1], p[2], p[3]);
        return 0;
    }
    Line("op %lld %lld %lld %lld %lld = %lld", (int)inOp, a, b, c, d, r);
    // re-entry: a modal loop runs any lobby code; an operation may complete at once
    if (sEnter != NULL && sDepth < 2 && (inOp == Op::RunDialog || inOp == Op::RunOp) && R(3) == 0) {
        Step s = {Call::StatusOpComplete, a, 0, 0, 0, 0};
        if (inOp == Op::RunDialog)
            s = RandomStep(true);
        Line("reenter %lld %lld %lld %lld %lld %lld", (int)s.mCall, s.a, s.b, s.c, s.d, s.e);
        sDepth++;
        intptr_t rr = sEnter(s.mCall, s.a, s.b, s.c, s.d, s.e);
        sDepth--;
        Line("reret %lld", rr);
    }
    return r;
}

// the original DialogLogic, every call on another object replaced by its op
struct OInfo { intptr_t mWindow; intptr_t mComponent; int mRect[4]; bool mInModal; };
struct OLogic {
    std::list<OInfo> mDialogStack;
    bool mPopupWindows, mStatusCancelled;
    intptr_t mOkCancelDialog, mStatusDialog;
    bool mStatusVisible;
    intptr_t mStatusOp;
    int mStatusMode;
};
static OLogic* sO;

// a SmartPtr assignment: the old one released, then a reference to the new one; nothing for the same pointer
static void Assign(intptr_t& ioSlot, intptr_t inNew, Op inAdd, Op inRelease)
{
    if (ioSlot == inNew) return;
    if (ioSlot != 0) H(inRelease, ioSlot);
    ioSlot = inNew;
    if (inNew != 0) H(inAdd, inNew);
}

// SafeGetComponent into a member (the port's shape: the found one with a reference, then the old one released)
static void Find(intptr_t& ioSlot, Op inOp, intptr_t inConfig)
{
    intptr_t aFound = H(inOp, inConfig);
    intptr_t anOld = ioSlot;
    ioSlot = aFound;
    if (anOld != 0) H(Op::ReleaseComponent, anOld);
}

static void OUpdateStatus(int inStatus);
static int ODoDialog(intptr_t inDialog);

static void OInitComponent(intptr_t inConfig)
{
    sO->mStatusVisible = false;
    sO->mStatusMode = 0;
    Assign(sO->mStatusOp, 0, Op::AddRefOp, Op::ReleaseOp);
    Find(sO->mOkCancelDialog, Op::FindOkCancel, inConfig);
    Find(sO->mStatusDialog, Op::FindStatus, inConfig);
    intptr_t aWm = H(Op::WmPopups);
    if (aWm != -1) // WindowManager::GetDefaultWindowManager() != NULL
        sO->mPopupWindows = aWm != 0;
}

static void OClearDialogs()
{
    if (sO->mDialogStack.empty() || H(Op::LobbyRoot) == 0)
        return;
    bool aChanged = false;
    std::list<OInfo>::iterator aBackItr = sO->mDialogStack.end();
    while (aBackItr != sO->mDialogStack.begin()) {
        --aBackItr;
        if (aBackItr->mInModal)
            break;
        intptr_t aComponent = aBackItr->mComponent;
        intptr_t aParent = H(Op::Parent, aComponent);
        if (aParent != 0)
            H(Op::RemoveChild, aParent, aComponent);
        if (sO->mPopupWindows)
            H(Op::WindowClose, aBackItr->mWindow);
        aChanged = true;
    }
    if (!aChanged)
        return;
    std::list<OInfo>::iterator anItr = sO->mDialogStack.begin();
    while (anItr != sO->mDialogStack.end()) {
        if (!anItr->mInModal) {
            OInfo e = *anItr;
            sO->mDialogStack.erase(anItr++);
            if (e.mComponent != 0) H(Op::ReleaseComponent, e.mComponent);
            if (e.mWindow != 0) H(Op::ReleaseWindow, e.mWindow);
        }
        else
            ++anItr;
    }
}

static void OKillHook()
{
    if (sO->mStatusOp != 0) {
        H(Op::OpKill, sO->mStatusOp);
        Assign(sO->mStatusOp, 0, Op::AddRefOp, Op::ReleaseOp);
    }
    OClearDialogs();
}

static void OStatusOpComplete(intptr_t inOp)
{
    if (inOp != sO->mStatusOp)
        return;
    bool aDone = sO->mStatusMode == 2 || (sO->mStatusMode == 1 && H(Op::OpGetStatus, inOp) == 0);
    if (aDone)
        H(Op::EndDialog, sO->mStatusDialog);
    else
        OUpdateStatus((int)H(Op::OpGetStatus, inOp));
}

static bool OCheckDialog(intptr_t inDialog, bool& outSizeChanged)
{
    outSizeChanged = true;
    if (sO->mDialogStack.empty())
        return false;
    OInfo& anInfo = sO->mDialogStack.back();
    if (anInfo.mComponent != inDialog)
        return false;
    if (H(Op::Width, inDialog) == anInfo.mRect[2] && H(Op::Height, inDialog) == anInfo.mRect[3])
        outSizeChanged = false;
    return true;
}

// GetDialogWindow: the window with a reference (the returned SmartPtr)
static intptr_t OGetDialogWindow(bool inReuse)
{
    if (!sO->mPopupWindows) {
        intptr_t w = H(Op::RootWindow);
        if (w != 0) H(Op::AddRefWindow, w);
        return w;
    }
    if (inReuse && !sO->mDialogStack.empty()) {
        intptr_t w = sO->mDialogStack.back().mWindow;
        if (w != 0) H(Op::AddRefWindow, w);
        return w;
    }
    intptr_t aWindow = H(Op::NewWindow);
    intptr_t aParent = !sO->mDialogStack.empty() ? sO->mDialogStack.back().mWindow : H(Op::RootWindow);
    H(Op::SetUpPopup, aWindow, aParent);
    return aWindow;
}

static int ODoDialog(intptr_t inDialog)
{
    bool aSizeChanged = true;
    bool aReuse = OCheckDialog(inDialog, aSizeChanged);
    if (!aReuse)
        OClearDialogs();
    intptr_t aWindow = OGetDialogWindow(aReuse);
    if (aWindow == 0)
        return -1;
    intptr_t aRoot = 0;
    if (sO->mPopupWindows) {
        intptr_t w = H(Op::Width, inDialog);
        intptr_t h = H(Op::Height, inDialog);
        H(Op::SetClientSize, aWindow, w, h);
        aRoot = H(Op::WindowRoot, aWindow);
    }
    else
        aRoot = H(Op::LobbyRoot);
    if (aRoot != 0) H(Op::AddRefComponent, aRoot);
    if (aRoot == 0) {
        H(Op::ReleaseWindow, aWindow);
        return -1;
    }
    if (aSizeChanged) {
        if (sO->mPopupWindows)
            H(Op::WindowCenter, aWindow);
        else
            H(Op::CenterLayout, aRoot, inDialog);
    }
    if (!aReuse) {
        H(Op::AddChild, aRoot, inDialog);
        sO->mDialogStack.push_back(OInfo());
        memset(&sO->mDialogStack.back(), 0, sizeof(OInfo));
    }
    OInfo& anInfo = sO->mDialogStack.back();
    Assign(anInfo.mWindow, aWindow, Op::AddRefWindow, Op::ReleaseWindow);
    Assign(anInfo.mComponent, inDialog, Op::AddRefComponent, Op::ReleaseComponent);
    anInfo.mInModal = true;
    H(Op::Bounds, inDialog, (intptr_t)anInfo.mRect);
    if (sO->mPopupWindows)
        H(Op::WindowShow, aWindow);
    int aResult = (int)H(Op::RunDialog, aWindow, inDialog);
    anInfo.mInModal = false;
    H(Op::ReleaseComponent, aRoot);
    H(Op::ReleaseWindow, aWindow);
    return aResult;
}

static void OShowStatus(intptr_t inTitle, intptr_t inStatus, bool inCancel)
{
    if (sO->mStatusVisible || H(Op::LobbySuspended) != 0)
        return;
    sO->mStatusVisible = true;
    H(Op::StatusSetText, sO->mStatusDialog, inTitle, inStatus, inCancel);
    ODoDialog(sO->mStatusDialog);
    H(Op::StatusSetStartTick, sO->mStatusDialog);
    sO->mStatusVisible = false;
}

static void ORunOpWithStatus(intptr_t inOp, intptr_t inTitle, intptr_t inStatus, int inStatusMode, intptr_t inTimeout)
{
    if (sO->mStatusVisible) {
        H(Op::OpForceFinishKilled, inOp);
        return;
    }
    Assign(sO->mStatusOp, inOp, Op::AddRefOp, Op::ReleaseOp);
    sO->mStatusCancelled = false;
    H(Op::RunOp, sO->mStatusOp, inTimeout);
    sO->mStatusMode = inStatusMode;
    OShowStatus(inTitle, inStatus, true);
    // (the port's guard: a KillHook in the box's loop cleared the operation)
    if (sO->mStatusOp != 0 && H(Op::OpGetStatus, sO->mStatusOp) == 100000) {
        sO->mStatusCancelled = true;
        H(Op::OpKill, sO->mStatusOp);
    }
    Assign(sO->mStatusOp, 0, Op::AddRefOp, Op::ReleaseOp);
}

static void OShowError(intptr_t inTitle, intptr_t inStatus)
{
    if (sO->mStatusVisible)
        return;
    if (H(Op::SoundEffects))
        H(Op::PlayErrorSound);
    OShowStatus(inTitle, inStatus, false);
}

static int OShowOkCancel(intptr_t inTitle, intptr_t inText, bool inPlaySound, bool inYesNo)
{
    H(Op::OkCancelSetText, sO->mOkCancelDialog, inTitle, inText, inYesNo);
    if (inPlaySound && H(Op::SoundEffects))
        H(Op::PlayErrorSound);
    return ODoDialog(sO->mOkCancelDialog);
}

static void OUpdateStatus(int inStatus)
{
    int anOld[4];
    H(Op::Bounds, sO->mStatusDialog, (intptr_t)anOld);
    int anOldWidth = anOld[2];
    int anOldHeight = anOld[3];
    H(Op::StatusSetStatus, sO->mStatusDialog, inStatus);
    if (inStatus != 0 && H(Op::SoundEffects))
        H(Op::PlayErrorSound);
    if (anOldWidth == H(Op::Width, sO->mStatusDialog) && anOldHeight == H(Op::Height, sO->mStatusDialog))
        return;
    if (sO->mPopupWindows) {
        intptr_t aWindow = H(Op::RootWindowOf, sO->mStatusDialog);
        if (aWindow == 0)
            return;
        intptr_t w = H(Op::Width, sO->mStatusDialog);
        intptr_t h = H(Op::Height, sO->mStatusDialog);
        H(Op::SetClientSize, aWindow, w, h);
        H(Op::WindowCenter, aWindow);
    }
    else {
        intptr_t aRoot = H(Op::LobbyRoot);
        if (aRoot != 0) {
            H(Op::CenterLayout, aRoot, sO->mStatusDialog);
            int aBounds[4];
            H(Op::Bounds, sO->mStatusDialog, (intptr_t)aBounds);
            H(Op::InvalidateRect, aRoot, (intptr_t)aBounds);
        }
    }
}

static intptr_t OCall(Call inCall, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e)
{
    switch (inCall) {
    case Call::Init: OInitComponent(a); return 0;
    case Call::KillHook: OKillHook(); return 0;
    case Call::Reset: case Call::ClearDialogs: OClearDialogs(); return 0;
    case Call::StatusOpComplete: OStatusOpComplete(a); return 0;
    case Call::DoDialog: return ODoDialog(a);
    case Call::RunOpWithStatus: ORunOpWithStatus(a, b, c, (int)d, e); return 0;
    case Call::ShowStatus: OShowStatus(a, b, c != 0); return 0;
    case Call::ShowError: OShowError(a, b); return 0;
    case Call::UpdateStatus: OUpdateStatus((int)a); return 0;
    case Call::ShowOkCancel: return OShowOkCancel(a, b, c != 0, false);
    case Call::ShowYesNo: return OShowOkCancel(a, b, c != 0, true);
    case Call::Field:
        switch (a) {
        case 0: return sO->mStatusCancelled;
        case 1: return sO->mPopupWindows;
        case 2: return sO->mStatusVisible;
        case 3: return sO->mStatusMode;
        case 4: return sO->mStatusOp;
        case 5: return sO->mOkCancelDialog;
        case 6: return sO->mStatusDialog;
        case 7: return (intptr_t)sO->mDialogStack.size();
        }
        return 0;
    }
    return 0;
}

static void OFree()
{
    // the members in reverse order: the status op, the status and OK/Cancel dialogs, then the stack's entries
    if (sO->mStatusOp != 0) H(Op::ReleaseOp, sO->mStatusOp);
    if (sO->mStatusDialog != 0) H(Op::ReleaseComponent, sO->mStatusDialog);
    if (sO->mOkCancelDialog != 0) H(Op::ReleaseComponent, sO->mOkCancelDialog);
    for (std::list<OInfo>::iterator i = sO->mDialogStack.begin(); i != sO->mDialogStack.end(); ++i) {
        if (i->mComponent != 0) H(Op::ReleaseComponent, i->mComponent);
        if (i->mWindow != 0) H(Op::ReleaseWindow, i->mWindow);
    }
    delete sO;
    sO = NULL;
}

static EeWonDlgLogic* sRust;
static intptr_t RustHost(void*, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d) { return H(inOp, a, b, c, d); }
static intptr_t RustEnter(Call inCall, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e)
{
    return ee_won_dlglogic_call(sRust, inCall, a, b, c, d, e);
}

static std::vector<std::string> RunLogic(bool inRust, unsigned inSeed, const std::vector<Step>& inSteps)
{
    sTrace.clear();
    sSeed = inSeed;
    sNewWindows = 0;
    sDepth = 0;
    if (inRust) {
        sRust = ee_won_dlglogic_new((void*)0xD10, RustHost);
        sEnter = RustEnter;
    }
    else {
        sO = new OLogic();
        sO->mPopupWindows = false;
        sO->mStatusCancelled = false;
        sO->mOkCancelDialog = sO->mStatusDialog = sO->mStatusOp = 0;
        sO->mStatusVisible = false; // unset by the original's constructor (the Rust's false)
        sO->mStatusMode = 0;        // unset (the Rust's 0)
        sEnter = OCall;
    }
    for (size_t i = 0; i < inSteps.size(); i++) {
        const Step& s = inSteps[i];
        Line("call %lld %lld %lld %lld %lld %lld", (int)s.mCall, s.a, s.b, s.c, s.d, s.e);
        intptr_t r = sEnter(s.mCall, s.a, s.b, s.c, s.d, s.e);
        Line("ret %lld", r);
    }
    sEnter = NULL;
    Line("free");
    if (inRust) { ee_won_dlglogic_free(sRust); sRust = NULL; }
    else OFree();
    return sTrace;
}

// ---- part 2: the two boxes (lobbydlg kinds Status and OkCancel)

static const intptr_t kDialog = 0xD1A;
static const intptr_t kButtons[2] = {0x53, 0x54};
static const intptr_t kTitleText = 0x55;
static uint32_t sTick;

static intptr_t Control(const char* inName) { intptr_t r = 0; for (const char* p = inName; *p; p++) r = r * 31 + *p; return 0x1000 + 16 * (intptr_t)(((uintptr_t)r) % 64); }

static intptr_t HD(DOp inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
{
    intptr_t r = 0;
    if (inOp == DOp::Find) {
        const EeWonLobbyDlgSlot* aSlot = reinterpret_cast<const EeWonLobbyDlgSlot*>(b);
        r = R(10) ? Control(aSlot->name) : 0;
        char aBuffer[200];
        snprintf(aBuffer, sizeof(aBuffer), "op %d %lld %s %s %d %d = %lld", (int)inOp, (long long)a, aSlot->name,
                 aSlot->file, aSlot->line, (int)aSlot->ty, (long long)r);
        sTrace.push_back(aBuffer);
        return r;
    }
    switch (inOp) {
    case DOp::Width: case DOp::Height: { static const intptr_t k[] = {0, 40, 100, 260, 300, 700}; r = k[R(6)]; break; }
    case DOp::Left: case DOp::Top: r = R(60); break;
    case DOp::FrameLeft: case DOp::FrameRight: r = R(8); break;
    case DOp::RootWidth: case DOp::RootHeight: { static const intptr_t k[] = {640, 800, 300}; r = k[R(3)]; break; }
    case DOp::FontStringWidth: case DOp::FontStringWidthRange: { static const intptr_t k[] = {0, 120, 249, 250, 600, 799, 800, 2000}; r = k[R(8)]; break; }
    case DOp::FontCharWidth: r = 5 + R(10); break;
    case DOp::FontHeight: { static const intptr_t k[] = {12, 16, 30, 70}; r = k[R(4)]; break; }
    case DOp::TickCount: r = (intptr_t)(uint32_t)(sTick + R(100)); break;
    case DOp::IsDragging: r = R(2); break;
    case DOp::TitleText: r = kTitleText; break;
    default: break;
    }
    Line("op %lld %lld %lld %lld = %lld", (int)inOp, a, b, c, r);
    return r;
}

struct ODlg { Kind mKind; intptr_t m[7]; uint32_t mStartTick; };
static ODlg sD;
static const EeWonLobbyDlgSlot* sSlots[2][7];

static int DSlotCount(Kind inKind) { return inKind == Kind::Status ? 6 : 7; }

static void ODlgInit()
{
    // StatusDialog: TitleLabel, CloseButton, StatusLabel, CancelButton, Back, DialogComponent;
    // OkCancelDialog: TitleLabel, CloseButton, BodyLabel, OKButton, CancelButton, Back, DialogComponent
    static const int kStatusOrder[] = {0, 1, 2, 3, 5, 4};
    static const int kOkOrder[] = {0, 4, 1, 2, 3, 6, 5};
    bool aStatus = sD.mKind == Kind::Status;
    for (int i = 0; i < DSlotCount(sD.mKind); i++) {
        int m = aStatus ? kStatusOrder[i] : kOkOrder[i];
        intptr_t aNew = HD(DOp::Find, kConfig, (intptr_t)sSlots[aStatus ? 0 : 1][m]);
        intptr_t anOld = sD.m[m];
        sD.m[m] = aNew;
        if (anOld != 0) HD(DOp::Release, anOld);
    }
}

// StatusDialog::SetText (members: 0 title, 1 close, 2 status, 3 cancel, 4 dialog component, 5 back)
static void OStatusSetText(intptr_t inTitle, intptr_t inStatus, bool inCancel)
{
    intptr_t* m = sD.m;
    intptr_t anOldB8C = HD(DOp::IsDragging, kDialog);
    int anInitialWidth = (int)HD(DOp::Width, kDialog);
    int anInitialHeight = (int)HD(DOp::Height, kDialog);
    int anInitialStatusWidth = (int)HD(DOp::Width, m[2]);
    int anInitialStatusHeight = (int)HD(DOp::Height, m[2]);
    int anInitialComponentWidth = (int)HD(DOp::Width, m[4]);
    int anInitialComponentHeight = (int)HD(DOp::Height, m[4]);
    int anInitialButtonY = (int)HD(DOp::Top, m[3]);
    HD(DOp::LabelSetText, m[0], inTitle);
    HD(DOp::SetDesiredWidth, m[0], 0);
    HD(DOp::ButtonSetString, m[3], inCancel ? 9 : 8);
    HD(DOp::SetDesiredSize, m[3], 0, 0);
    HD(DOp::SetSizeAtLeast, m[3], 100, 0);
    HD(DOp::SetVertPad, m[2], 0, 0);
    HD(DOp::LabelSetText, m[2], 0);
    HD(DOp::SetSize, m[2], 0, 0);
    HD(DOp::SetSize, kDialog, 0, 0);
    int t1 = (int)HD(DOp::Width, m[0]), t2 = (int)HD(DOp::Width, m[1]);
    int t3 = (int)HD(DOp::FrameRight, m[5]), t4 = (int)HD(DOp::FrameLeft, m[5]);
    HD(DOp::SetSizeAtLeast, kDialog, t1 + t2 + t3 + t4 + 5, 0);
    HD(DOp::SetSize, m[4], 0, 0);
    HD(DOp::SetDesiredSize, m[4], 0, 0);
    HD(DOp::SetDesiredSize, kDialog, 20, 20);
    int anOldWidth = (int)HD(DOp::Width, kDialog);
    int anOldHeight = (int)HD(DOp::Height, kDialog);
    int anOldStatusHeight = (int)HD(DOp::Height, m[2]);
    int aRootWidth = (int)HD(DOp::RootWidth);
    int aRootHeight = (int)HD(DOp::RootHeight);
    int aTextWidth = (int)HD(DOp::FontStringWidth, m[2], inStatus);
    int anMWidth = (int)HD(DOp::FontCharWidth, m[2], 'M');
    int aBodyWidth = ee_won_dlg_status_body_width(aTextWidth, anMWidth, aRootWidth);
    int aWidthChange = ee_won_dlg_growth(aBodyWidth, (int)HD(DOp::Width, m[2]));
    int h = (int)HD(DOp::Height, m[2]);
    HD(DOp::SetSize, m[2], aBodyWidth, h);
    HD(DOp::LabelSetText, m[2], inStatus);
    int aLabelHeight = (int)HD(DOp::Height, m[2]);
    int aLineHeight = (int)HD(DOp::FontHeight, m[2]);
    int aNumLines = aLabelHeight / aLineHeight;
    if (aNumLines < 5) {
        int aPad = (5 - aNumLines) * aLineHeight / 2;
        HD(DOp::SetVertPad, m[2], aPad, aPad);
        HD(DOp::SetDesiredSize, m[2], 0, 0);
    }
    int aHeightChange = ee_won_dlg_growth((int)HD(DOp::Height, m[2]), anOldStatusHeight);
    int aNewWidth = anOldWidth + aWidthChange;
    int aNewHeight = aHeightChange + anOldHeight;
    int anExtraWidth;
    if (ee_won_dlg_status_extra_width(aNewWidth, aNewHeight, aRootHeight, aWidthChange, anOldWidth, &anExtraWidth)) {
        aWidthChange += anExtraWidth;
        int h2 = (int)HD(DOp::Height, m[2]);
        HD(DOp::SetSize, m[2], aBodyWidth + anExtraWidth, h2);
        aHeightChange = (int)HD(DOp::Height, m[2]) - anOldStatusHeight;
    }
    HD(DOp::SetSize, kDialog, aWidthChange + anOldWidth, aHeightChange + anOldHeight);
    if (ee_won_dlg_status_young((uint32_t)HD(DOp::TickCount), sD.mStartTick)) {
        HD(DOp::SetSizeAtLeast, m[2], anInitialStatusWidth, anInitialStatusHeight);
        HD(DOp::SetSizeAtLeast, m[4], anInitialComponentWidth, anInitialComponentHeight);
        HD(DOp::SetSizeAtLeast, kDialog, anInitialWidth, anInitialHeight);
        if (anInitialButtonY > HD(DOp::Top, m[3])) {
            intptr_t aLeft = HD(DOp::Left, m[3]);
            HD(DOp::SetPos, m[3], aLeft, anInitialButtonY);
        }
    }
    HD(DOp::Invalidate, kDialog);
    HD(DOp::SetDragging, kDialog, anOldB8C);
}

// OkCancelDialog::SetText (members: 0 title, 1 body, 2 OK, 3 cancel, 4 close, 5 dialog component, 6 back)
static void OOkCancelSetText(intptr_t inTitle, intptr_t inBody, intptr_t inOkText, intptr_t inCancelText)
{
    intptr_t* m = sD.m;
    HD(DOp::LabelSetText, m[0], inTitle);
    HD(DOp::SetDesiredWidth, m[0], 0);
    HD(DOp::ButtonSetText, m[2], inOkText);
    HD(DOp::SetDesiredSize, m[2], 0, 0);
    HD(DOp::SetSizeAtLeast, m[2], 100, 0);
    HD(DOp::ButtonSetText, m[3], inCancelText);
    HD(DOp::SetDesiredSize, m[3], 0, 0);
    HD(DOp::SetSizeAtLeast, m[3], 100, 0);
    HD(DOp::SetVertPad, m[1], 0, 0);
    HD(DOp::LabelSetText, m[1], 0);
    HD(DOp::SetSize, m[1], 0, 0);
    HD(DOp::SetSize, kDialog, 0, 0);
    int t1 = (int)HD(DOp::Width, m[0]), t2 = (int)HD(DOp::Width, m[4]);
    int t3 = (int)HD(DOp::FrameLeft, m[6]), t4 = (int)HD(DOp::FrameRight, m[6]);
    HD(DOp::SetSizeAtLeast, kDialog, t1 + t2 + t3 + t4 + 5, 0);
    HD(DOp::SetSize, m[5], 0, 0);
    HD(DOp::SetDesiredSize, m[5], 0, 0);
    HD(DOp::SetDesiredSize, kDialog, 20, 20);
    int anOldHeight = (int)HD(DOp::Height, kDialog);
    int anOldWidth = (int)HD(DOp::Width, kDialog);
    int anOldBodyHeight = (int)HD(DOp::Height, m[1]);
    int aTextWidth = (int)HD(DOp::FontStringWidthRange, m[1], inBody);
    int aBodyWidth;
    if (aTextWidth < 250)
        aBodyWidth = aTextWidth;
    else if (aTextWidth < 800)
        aBodyWidth = 250;
    else
        aBodyWidth = 400;
    int aWidthChange = aBodyWidth - (int)HD(DOp::Width, m[1]);
    if (aWidthChange < 0)
        aWidthChange = 0;
    int h = (int)HD(DOp::Height, m[1]);
    HD(DOp::SetSize, m[1], aBodyWidth, h);
    HD(DOp::LabelSetText, m[1], inBody);
    int aLabelHeight = (int)HD(DOp::Height, m[1]);
    int aNumLines = aLabelHeight / (int)HD(DOp::FontHeight, m[1]);
    if (aNumLines < 5) {
        int aPad = (5 - aNumLines) * (int)HD(DOp::FontHeight, m[1]) / 2;
        HD(DOp::SetVertPad, m[1], aPad, aPad);
        HD(DOp::SetDesiredSize, m[1], 0, 0);
    }
    int aHeightChange = (int)HD(DOp::Height, m[1]) - anOldBodyHeight;
    if (aHeightChange < 0)
        aHeightChange = 0;
    HD(DOp::RootWidth);
    int aRootHeight = (int)HD(DOp::RootHeight);
    if (anOldHeight + aHeightChange > anOldWidth + aWidthChange || anOldHeight + aHeightChange >= aRootHeight) {
        int anExtraWidth = (anOldWidth + aWidthChange) * 5 / 4 - aWidthChange - anOldWidth;
        aWidthChange += anExtraWidth;
        int h2 = (int)HD(DOp::Height, m[1]);
        HD(DOp::SetSize, m[1], aBodyWidth + anExtraWidth, h2);
        aHeightChange = (int)HD(DOp::Height, m[1]) - anOldBodyHeight;
    }
    HD(DOp::SetSize, kDialog, anOldWidth + aWidthChange, anOldHeight + aHeightChange);
}

struct DStep { DCall mCall; intptr_t a, b, c; };

static intptr_t ODlgCall(const DStep& s)
{
    switch (s.mCall) {
    case DCall::Init: ODlgInit(); return 0;
    case DCall::Event: HD(DOp::BaseEvent, s.a); return 0; // neither overrode HandleComponentEvent
    case DCall::Slot: return s.a < 7 ? sD.m[s.a] : 0;
    case DCall::Field: return s.a == 4 ? (intptr_t)sD.mStartTick : 0;
    case DCall::SetStartTick: sD.mStartTick = (uint32_t)s.a; return 0;
    case DCall::SetText:
        if (sD.mKind == Kind::Status) OStatusSetText(s.a, s.b, s.c != 0);
        else OOkCancelSetText(s.a, s.b, kButtons[0], kButtons[1]);
        return 0;
    case DCall::SetStatus:
        if (sD.mKind == Kind::Status) OStatusSetText(HD(DOp::TitleText, sD.m[0]), s.a, s.b != 0);
        return 0;
    default: return 0;
    }
}

static std::vector<std::string> RunDlg(bool inRust, Kind inKind, unsigned inSeed, const std::vector<DStep>& inSteps)
{
    sTrace.clear();
    sSeed = inSeed;
    EeWonLobbyDlg* d = NULL;
    if (inRust) d = ee_won_lobbydlg_new((void*)kDialog, inKind, [](void*, DOp o, intptr_t a, intptr_t b, intptr_t c) { return HD(o, a, b, c); });
    else { memset(&sD, 0, sizeof(sD)); sD.mKind = inKind; }
    for (size_t i = 0; i < inSteps.size(); i++) {
        DStep s = inSteps[i];
        // the OK/Cancel box's two button texts are passed as a pointer: traced as 1
        Line("call %lld %lld %lld %lld", (int)s.mCall, s.a, s.b, s.c);
        intptr_t r;
        if (inRust) {
            intptr_t c = s.c;
            if (s.mCall == DCall::SetText && inKind == Kind::OkCancel) c = (intptr_t)kButtons;
            r = ee_won_lobbydlg_call(d, s.mCall, s.a, s.b, c);
        }
        else
            r = ODlgCall(s);
        Line("ret %lld", r);
    }
    Line("free");
    if (inRust) ee_won_lobbydlg_free(d);
    else
        for (int i = DSlotCount(inKind) - 1; i >= 0; i--)
            if (sD.m[i] != 0) HD(DOp::Release, sD.m[i]);
    return sTrace;
}

static const EeWonLobbyDlgSlot* sLearnFound[10];
static int sLearnCount;
static intptr_t LearnHost(void*, DOp inOp, intptr_t, intptr_t b, intptr_t)
{
    if (inOp == DOp::Find) {
        sLearnFound[sLearnCount] = reinterpret_cast<const EeWonLobbyDlgSlot*>(b);
        return 0x100 + sLearnCount++;
    }
    return 0;
}

static void Compare(const char* inPart, int n, const std::vector<std::string>& a, const std::vector<std::string>& b,
                    int& ioDiffer, bool inPrint, const char* inHead, unsigned long long& ioLines)
{
    ioLines += a.size();
    sChecks++;
    if (a != b) {
        ioDiffer++;
        if (ioDiffer <= 3) {
            printf("%s sequence %d differs\n", inPart, n);
            for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                const char* x = i < a.size() ? a[i].c_str() : "-";
                const char* y = i < b.size() ? b[i].c_str() : "-";
                if (strcmp(x, y) != 0) { printf("  line %zu: C++ '%s' Rust '%s'\n", i, x, y); break; }
            }
        }
        return;
    }
    bool aNew = n < 40;
    for (size_t i = 0; i < a.size(); i++) {
        size_t p = a[i].find(' ');
        std::string aKey = std::string(inPart) + " " + a[i].substr(0, p == std::string::npos ? p : a[i].find(' ', p + 1));
        if (a[i].compare(0, 3, "ret") == 0 || a[i].compare(0, 5, "reret") == 0) continue; // results are not new kinds
        if (sSeen.insert(aKey).second) aNew = true;
    }
    if (inPrint && aNew) {
        printf("%s\n", inHead);
        for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
    }
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    // the two boxes' tables, checked against the original SafeGetComponent arguments in member order
    struct OrigSlot { const char* mName; int mLine; int mType; }; // 1 Button, 2 Label, 4 Component, 5 Frame
    static const OrigSlot kOrig[2][7] = {
        {{"TitleLabel", 28, 2}, {"CloseButton", 29, 1}, {"StatusLabel", 30, 2}, {"CancelButton", 31, 1},
         {"DialogComponent", 34, 4}, {"Back", 33, 5}},
        {{"TitleLabel", 26, 2}, {"BodyLabel", 28, 2}, {"OKButton", 30, 1}, {"CancelButton", 31, 1},
         {"CloseButton", 27, 1}, {"DialogComponent", 34, 4}, {"Back", 33, 5}},
    };
    for (int k = 0; k < 2; k++) {
        Kind aKind = k == 0 ? Kind::Status : Kind::OkCancel;
        sLearnCount = 0;
        EeWonLobbyDlg* d = ee_won_lobbydlg_new(NULL, aKind, LearnHost);
        ee_won_lobbydlg_call(d, DCall::Init, kConfig, 0, 0);
        for (int m = 0; m < DSlotCount(aKind); m++) {
            intptr_t p = ee_won_lobbydlg_call(d, DCall::Slot, m, 0, 0);
            sSlots[k][m] = p >= 0x100 && p < 0x100 + sLearnCount ? sLearnFound[p - 0x100] : NULL;
            CHECK(sSlots[k][m] != NULL);
            if (sSlots[k][m] == NULL) continue;
            CHECK(strcmp(sSlots[k][m]->name, kOrig[k][m].mName) == 0);
            CHECK(sSlots[k][m]->line == kOrig[k][m].mLine);
            CHECK((int)sSlots[k][m]->ty == kOrig[k][m].mType);
        }
        ee_won_lobbydlg_free(d);
    }
    unsigned long long aLines = 0;
    int aDiffer = 0;
    unsigned aStepSeed = 4242;
    for (int n = 0; n < 20000; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        std::vector<Step> aSteps;
        aSteps.push_back(Step{Call::Init, kConfig, 0, 0, 0, 0});
        for (int i = 0; i < 10; i++) aSteps.push_back(RandomStep(false));
        aStepSeed = sSeed;
        sSeed = aSave;
        unsigned aSeed = 991u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunLogic(false, aSeed, aSteps);
        std::vector<std::string> b = RunLogic(true, aSeed, aSteps);
        char aHead[40];
        snprintf(aHead, sizeof(aHead), "seq %d logic", n);
        Compare("logic", n, a, b, aDiffer, aPrint, aHead, aLines);
    }
    int aDlgDiffer = 0;
    for (int n = 0; n < 20000; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        Kind aKind = R(2) ? Kind::Status : Kind::OkCancel;
        std::vector<DStep> aSteps;
        aSteps.push_back(DStep{DCall::Init, kConfig, 0, 0});
        for (int i = 0; i < 8; i++) {
            DStep s = {DCall::SetText, kTitle, kText, (intptr_t)R(2)};
            switch (R(8)) {
            case 0: s.mCall = DCall::SetStartTick; s.a = (intptr_t)(1000 + R(200)); break;
            case 1: s.mCall = DCall::Field; s.a = 4; break;
            case 2: s.mCall = DCall::Event; s.a = 0xE1; s.b = R(4); s.c = 0x4444; break;
            case 3: s.mCall = DCall::Slot; s.a = R(8); break;
            case 4: if (aKind == Kind::Status) { s.mCall = DCall::SetStatus; s.a = kText; s.b = R(2); s.c = 0; } break;
            case 5: if (R(4) == 0) { s.mCall = DCall::Init; s.a = kConfig; s.b = 0; s.c = 0; } break;
            default: break;
            }
            if (s.mCall == DCall::SetText && aKind == Kind::OkCancel) s.c = 1;
            aSteps.push_back(s);
        }
        aStepSeed = sSeed;
        sSeed = aSave;
        sTick = 1000 + (unsigned)n % 200;
        unsigned aSeed = 31u + (unsigned)n * 104729u;
        std::vector<std::string> a = RunDlg(false, aKind, aSeed, aSteps);
        std::vector<std::string> b = RunDlg(true, aKind, aSeed, aSteps);
        char aHead[40];
        snprintf(aHead, sizeof(aHead), "seq %d kind %d tick %u", n, (int)aKind, sTick);
        Compare("dlg", n, a, b, aDlgDiffer, aPrint, aHead, aLines);
    }
    if (aDiffer) { sFailures++; printf("FAIL: %d of 20000 logic sequences differ\n", aDiffer); }
    if (aDlgDiffer) { sFailures++; printf("FAIL: %d of 20000 box sequences differ\n", aDlgDiffer); }
    if (!aPrint)
        printf("won_dlglogic_test: 40000 sequences, %llu trace lines, %zu distinct ops and calls; %d checks, %d failures\n",
               aLines, sSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
