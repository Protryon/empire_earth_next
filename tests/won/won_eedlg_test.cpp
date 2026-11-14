// The EE game room's connect dialog as a lobby dialog kind (R6 WONLobby round 5): the original
// EELobbyConnectDialog.cpp (the statements copied from the C++ before the port, every call on a control, the game, a
// slot, a list line or the dialog's window replaced by the host op that makes it: LobbyDialogHost.cpp runs the same
// calls) against ee_won's lobbydlg.rs kind Connect over one scripted host.
//
// Each of 30,000 sequences makes the dialog, its InitComponent (now and then twice, or none), then 1 to 10 random
// calls (Run on a game, End, SetConnected of a slot, the fields, an event) and frees it. The host answers every op
// from a seeded generator (the same stream for both sides): the host flag, the slots and their states, the list's
// lines and their slots and pictures, the root window, the modal loop's result. The modal loop (LobbyDoDialog) and
// the window's EndDialog re-enter the dialog with SetConnected, End or Run (the players' messages, the captain's
// cancel) half of the time, two deep at most. The traces must be the same.
//
// The first 60 sequences and every later one that makes an op or call none before made are
// rust/crates/ee_won/src/golden_eedlg.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include "rust/ee_won.h"

typedef EeWonLobbyDlgOp Op;
typedef EeWonLobbyDlgCall Call;
typedef EeWonLobbyDlgKind Kind;
typedef EeWonLobbyDlgString S;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sSeed) % n; }
static std::vector<std::string> sTrace;
static std::set<std::string> sSeen;

static void Line(const char* inFormat, long long a = 0, long long b = 0, long long c = 0, long long d = 0,
                 long long e = 0)
{
    char aBuffer[200];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c, d, e);
    sTrace.push_back(aBuffer);
}

static const intptr_t kConfig = 0x77, kEvent = 0xE1, kDialog = 0xD1A, kGame = 0x3000, kSlot = 0x3600,
                      kRoot = 0x4000, kItem = 0x5000;
static bool sRust;
static EeWonLobbyDlg* sRustDlg;
// R6 WONLobby round 6: the game is a real ee_won game on both sides (the original's slot calls are the C++ views'
// forwarders, the Rust dialog calls it directly); its ops are traced as "gop"
static EeWonEeGame* sGame;
typedef EeWonEeGameCall GC;
static intptr_t EG(GC inCall, intptr_t a = 0, intptr_t b = 0) { return ee_won_eegame_call(sGame, inCall, a, b, 0, 0); }
static int sDepth;
static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c);

static void Reenter()
{
    if (sDepth >= 2 || R(2) != 0) return;
    sDepth++;
    Call aCall = Call::ConnectSetConnected;
    intptr_t a = R(10), b = R(2);
    switch (R(4)) {
    case 0: aCall = Call::ConnectEnd; a = R(2); b = 0; break;
    case 1: aCall = Call::ConnectRun; a = kGame; b = 0; break;
    default: break;
    }
    Line("reenter %lld %lld %lld", (long long)aCall, a, b);
    intptr_t r = sRust ? ee_won_lobbydlg_call(sRustDlg, aCall, a, b, 0) : OrigCall(aCall, a, b, 0);
    Line("back %lld", r);
    sDepth--;
}

static intptr_t Host(Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    intptr_t r = 0;
    switch (inOp) {
    case Op::Find: {
        const EeWonLobbyDlgSlot* aSlot = reinterpret_cast<const EeWonLobbyDlgSlot*>(b);
        r = R(10) ? 0x1000 + 16 * (intptr_t)aSlot->line : 0;
        char aBuffer[200];
        snprintf(aBuffer, sizeof(aBuffer), "op %d %lld %s %s %d %d = %lld", (int)inOp, (long long)a, aSlot->name,
                 aSlot->file, aSlot->line, (int)aSlot->ty, (long long)r);
        sTrace.push_back(aBuffer);
        return r;
    }
    case Op::GameIsHost: case Op::ConnectItemNotReady: r = R(2); break;
    case Op::GameRust: return (intptr_t)sGame; // a field read, not traced
    case Op::ConnectInsert: r = (c >> 8) != 0 ? R(5) != 0 : R(5) == 0; break;
    case Op::DoDialog: r = R(3) ? 100001 : R(2) ? 100002 : -1; break;
    case Op::RootWindow: r = R(5) ? kRoot : 0; break;
    case Op::ListNumItems: r = R(5); break;
    case Op::ListGetItem: r = kItem + 16 * b; break;
    case Op::ConnectItemSlot: r = R(4) ? R(9) : R(256); break;
    default: break;
    }
    Line("op %lld %lld %lld %lld = %lld", (long long)inOp, a, b, c, r);
    if (inOp == Op::DoDialog || inOp == Op::EndDialog) Reenter();
    return r;
}

static intptr_t O(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return Host(inOp, a, b, c); }

static intptr_t GameHost(void*, EeWonEeGameOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    if (inOp == EeWonEeGameOp::NumMapTypes) return 12; // read by a new game (untraced; the Rust tests answer 12)
    intptr_t r = inOp == EeWonEeGameOp::ClientName ? 0x7100 + R(4) : R(4);
    Line("gop %lld %lld %lld %lld %lld", (long long)inOp, a, b, c, d);
    sTrace.back() += " = " + std::to_string((long long)r);
    return r;
}

// ---- the original EELobbyConnectDialog, every call on another object replaced by its op

static struct Orig {
    intptr_t mConnectList, mCancelButton;   // SmartPtr<ListArea>, SmartPtr<Button>
    bool mIsRunning, mIsEnded;
} sO;

// SafeGetComponent(config, member, name, file, line): the control found with a reference (the Find op's), the old
// one released after the new one is held (the lobbydlg convention)
static void SafeGetComponent(intptr_t inConfig, intptr_t& ioMember, int inSlot)
{
    intptr_t aFound = O(Op::Find, inConfig, (intptr_t)ee_won_lobbydlg_slot(Kind::Connect, inSlot));
    intptr_t aOld = ioMember;
    ioMember = aFound;
    if (aOld != 0) O(Op::Release, aOld);
}

static void OrigInit(intptr_t inConfig)
{
    SafeGetComponent(inConfig, sO.mConnectList, 0);
    SafeGetComponent(inConfig, sO.mCancelButton, 1);
}

static void OrigEnd(bool inOK)
{
    intptr_t theRoot = O(Op::RootWindow, kDialog);
    if (theRoot != 0) {
        sO.mIsEnded = true;
        O(Op::EndDialog, theRoot, inOK ? 100001 : 100002);
    }
}

static bool OrigRun(intptr_t inGame)
{
    if (sO.mIsRunning) return false;
    bool theAllConnected = true;
    O(Op::ButtonSetString, sO.mCancelButton, O(Op::GameIsHost, inGame) ? (intptr_t)S::AbortLaunch : (intptr_t)S::LeaveGame);
    O(Op::ListBeginUpdate, sO.mConnectList);
    O(Op::ListClear, sO.mConnectList, 1);
    for (int i = 0; i < 8; i++) {
        // inGame->GetPlayerSlot(i) (&mSlots[i], never NULL); theSlot->Option(0)
        if ((unsigned char)EG(GC::SlotRawOption, i, 0) == 3) {
            // Image* theImage = (i == 0 ? Ready : Searching); InsertItem(new ConnectListItem(theSlot->GetName(),
            // theImage, i))
            intptr_t theName = EG(GC::SlotGetName, i);
            if (O(Op::ConnectInsert, sO.mConnectList, theName, i | (i == 0 ? 0 : 1) << 8))
                theAllConnected = false;
        }
    }
    O(Op::ListEndUpdate, sO.mConnectList);
    if (theAllConnected) return true;
    O(Op::RequestFocus, sO.mConnectList);
    sO.mIsEnded = false;
    sO.mIsRunning = true;
    intptr_t theResult = O(Op::DoDialog, kDialog);
    sO.mIsRunning = false;
    return theResult == 100001;
}

static void OrigSetConnected(unsigned char inSlot, bool inConnected)
{
    if (!sO.mIsRunning) return;
    bool theAllConnected = true;
    intptr_t theChanged = 0; // SmartPtr<ListItem>
    for (int i = 0; i < (int)O(Op::ListNumItems, sO.mConnectList); i++) {
        intptr_t theItem = O(Op::ListGetItem, sO.mConnectList, i);
        if ((unsigned char)O(Op::ConnectItemSlot, theItem) == inSlot) {
            O(Op::ConnectItemSetImage, theItem, inConnected ? 0 : 1);
            if (theChanged != theItem) {
                if (theChanged != 0) O(Op::Release, theChanged);
                theChanged = theItem;
                if (theItem != 0) O(Op::AddRef, theItem);
            }
        }
        if (O(Op::ConnectItemNotReady, theItem)) theAllConnected = false;
    }
    if (theChanged != 0) O(Op::ListRefresh, sO.mConnectList, theChanged);
    if (theAllConnected) OrigEnd(true);
    if (theChanged != 0) O(Op::Release, theChanged);
}

static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inCall) {
    case Call::Init: OrigInit(a); return 0;
    case Call::Event: O(Op::BaseEvent, a); return 0; // no override: the dialog's
    case Call::ConnectRun: return OrigRun(a);
    case Call::ConnectEnd: OrigEnd(a != 0); return 0;
    case Call::ConnectSetConnected: OrigSetConnected((unsigned char)a, b != 0); return 0;
    case Call::Slot: return a == 0 ? sO.mConnectList : a == 1 ? sO.mCancelButton : 0;
    case Call::Field: return a == 7 ? sO.mIsRunning : a == 8 ? sO.mIsEnded : 0;
    default: return 0;
    }
}

static void OrigFree()
{
    // the SmartPtr members destroyed in reverse order
    if (sO.mCancelButton != 0) O(Op::Release, sO.mCancelButton);
    if (sO.mConnectList != 0) O(Op::Release, sO.mConnectList);
}

// ---- the Rust side and the sequences

static intptr_t RustHost(void*, Op inOp, intptr_t a, intptr_t b, intptr_t c) { return Host(inOp, a, b, c); }

struct Step { Call mCall; intptr_t a, b, c; };
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
    // the dialog's address as the ops on the dialog itself name it
    if (inRust) sRustDlg = ee_won_lobbydlg_new((void*)kDialog, Kind::Connect, RustHost);
    else memset(&sO, 0, sizeof(sO));
    for (size_t i = 0; i < inSteps.size(); i++) {
        const Step& s = inSteps[i];
        Line("call %lld %lld %lld %lld", (long long)s.mCall, s.a, s.b, s.c);
        intptr_t r = inRust ? ee_won_lobbydlg_call(sRustDlg, s.mCall, s.a, s.b, s.c) : OrigCall(s.mCall, s.a, s.b, s.c);
        Line("ret %lld", r);
    }
    Line("free");
    if (inRust) ee_won_lobbydlg_free(sRustDlg);
    else OrigFree();
    Line("gfree");
    ee_won_eegame_free(sGame);
    sGame = NULL;
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    // the table against the original SafeGetComponent arguments
    const EeWonLobbyDlgSlot* s0 = ee_won_lobbydlg_slot(Kind::Connect, 0);
    const EeWonLobbyDlgSlot* s1 = ee_won_lobbydlg_slot(Kind::Connect, 1);
    CHECK(s0 != NULL && strcmp(s0->name, "ConnectList") == 0 && s0->line == 61 && s0->ty == EeWonLobbyDlgType::List);
    CHECK(s1 != NULL && strcmp(s1->name, "CancelButton") == 0 && s1->line == 62 && s1->ty == EeWonLobbyDlgType::Button);
    CHECK(s0 != NULL && strcmp(s0->file, "EELobbyConnectDialog.cpp") == 0);
    CHECK(ee_won_lobbydlg_slot(Kind::Connect, 2) == NULL);
    unsigned long long aLines = 0;
    int aDiffer = 0;
    unsigned aStepSeed = 4711;
    const int kSequences = 30000;
    for (int n = 0; n < kSequences; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        std::vector<Step> aSteps;
        if (R(8)) aSteps.push_back(Step{Call::Init, kConfig, 0, 0});
        int aCount = 1 + (int)R(10);
        for (int i = 0; i < aCount; i++) {
            Step s = {Call::ConnectRun, kGame, 0, 0};
            switch (R(8)) {
            case 0: s.mCall = Call::Init; s.a = kConfig; break;
            case 1: s.mCall = Call::ConnectEnd; s.a = R(2); break;
            case 2: case 3: s.mCall = Call::ConnectSetConnected; s.a = R(4) ? R(9) : R(256); s.b = R(2); break;
            case 4: s.mCall = Call::Field; s.a = 6 + R(4); break;
            case 5: s.mCall = Call::Event; s.a = kEvent; s.b = R(5); s.c = R(2) ? 0x1000 + 16 * 61 : 0; break;
            case 6: s.mCall = Call::Slot; s.a = R(3); break;
            default: break;
            }
            aSteps.push_back(s);
        }
        // the slots' states (two thirds seated, as the old scripted answers) and some reserved names' slots
        std::vector<Setup> aSetup;
        for (int i = 0; i < 8; i++)
            aSetup.push_back(Setup{i, 0, R(3) ? 3 : (intptr_t)R(5)});
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
            std::string aKey = aHead == "ret" || aHead == "back" ? aHead
                                                                              : a[i].substr(0, a[i].find(' ', aSpace + 1));
            if (sSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && aNew) {
            printf("seq %d kind %d\n", n, (int)Kind::Connect);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (aDiffer) { sFailures++; printf("FAIL: %d of %d sequences differ\n", aDiffer, kSequences); }
    if (!aPrint)
        printf("won_eedlg_test: %d sequences, %llu trace lines, %zu distinct ops and calls; %d checks, %d failures\n",
               kSequences, aLines, sSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
