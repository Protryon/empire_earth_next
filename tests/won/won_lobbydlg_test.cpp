// The lobby's eight small dialogs as one Rust object (R6 WONLobby round 1): the original LanNameDialog.cpp,
// PasswordDialog.cpp, AddFriendDialog.cpp, DirectConnectDialog.cpp, LanPortDialog.cpp, CreateRoomDialog.cpp,
// CreateGameDialog.cpp and AdminActionDialog.cpp (the statements copied from the C++ before the port, every call on
// a control, the dialog's base, a lobby game or a resource string replaced by the host op that makes it:
// LobbyDialogHost.cpp runs the same calls) against ee_won's lobbydlg.rs over one scripted host.
//
// Each of 20,000 sequences makes a dialog of a random kind, its InitComponent, then 12 random inward calls of that
// kind (Reset with every argument, the events of every type on every control and on others, the input checks, the
// port, game type and game, the admin action's getters, a second InitComponent now and then) and frees it. The host
// answers every op from a seeded generator (the same stream for both sides); an input's text set with its event
// re-enters the dialog's HandleComponentEvent half of the time, as the input box fires it. The traces (every op with
// its arguments and answer, every call's result, the controls released at the end) must be the same.
//
// The pure reads one statement made as a call's arguments (CheckInput's `create_enabled(name, shown, password)`)
// are made left to right on both sides; the original's order was the compiler's and changed nothing.
//
// The first 60 sequences and every later one that makes an op or call none before made are
// rust/crates/ee_won_lobby/src/golden_lobbydlg.txt, which the Rust tests replay; `--print` prints them.
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

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sSeed) % n; }
static std::vector<std::string> sTrace;
static std::set<std::string> sSeen;

static void Line(const char* inFormat, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    char aBuffer[160];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, (long long)a, (long long)b, (long long)c, (long long)d);
    sTrace.push_back(aBuffer);
}

static const intptr_t kConfig = 0x77;
static const intptr_t kEvent = 0xE1;
static const intptr_t kTarget = 0x5000;
static const intptr_t kGame = 0x9000;
static intptr_t Control(int inSlot) { return 0x1000 + 16 * inSlot; }

// one side's re-entry: the dialog's HandleComponentEvent for an input's text-changed event
typedef void (*ReenterFn)(intptr_t inComponent);
static ReenterFn sReenter;

// the scripted host: the answer to an op, the same stream for both sides
static intptr_t Host(Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    intptr_t r = 0;
    switch (inOp) {
    case Op::Find: {
        const EeWonLobbyDlgSlot* aSlot = reinterpret_cast<const EeWonLobbyDlgSlot*>(b);
        char aBuffer[160];
        snprintf(aBuffer, sizeof(aBuffer), "op %d %lld %s %s %d %d", (int)inOp, (long long)a, aSlot->name,
                 aSlot->file, aSlot->line, (int)aSlot->ty);
        // the control's address from its name's place in its kind's table (the test's world: one control a name)
        int aSlotIndex = (int)(c);
        (void)aSlotIndex;
        r = 0;
        for (const char* p = aSlot->name; *p; p++) r = r * 31 + *p;
        r = Control((int)(((uintptr_t)r) % 64));
        snprintf(aBuffer + strlen(aBuffer), sizeof(aBuffer) - strlen(aBuffer), " = %lld", (long long)r);
        sTrace.push_back(aBuffer);
        return r;
    }
    case Op::IsVisible: case Op::ButtonIsDisabled: case Op::ButtonIsChecked: r = R(2); break;
    case Op::InputTextLength: case Op::InputShownLength: r = R(3); break;
    case Op::InputToInt: { static const intptr_t kInts[] = {0, 4999, 5000, 6000, 65535, 65536, 7777, -1};
        r = kInts[R(8)]; break; }
    case Op::ComboGetSel: r = (intptr_t)R(7) - 1; break;
    case Op::GameCreate: r = kGame; break;
    default: r = 0; break;
    }
    Line("op %lld %lld %lld %lld", (intptr_t)inOp, a, b, c);
    char aBuffer[40];
    snprintf(aBuffer, sizeof(aBuffer), " = %lld", (long long)r);
    sTrace.back() += aBuffer;
    bool aFires = (inOp == Op::InputClear && b != 0) || (inOp == Op::InputSetNumber && c != 0) ||
                  (inOp == Op::InputSetEmpty && b != 0);
    if (aFires && R(2) == 0 && sReenter != NULL) {
        Line("reenter %lld", a, 0, 0, 0);
        sReenter(a);
    }
    return r;
}

// ---- the original classes, every call on another object replaced by its op

struct Orig {
    Kind mKind;
    intptr_t mCtl[10];
    unsigned short mGamePort; // LanPortDialog (unset by its constructor: 0 here, as the Rust)
    int mGameType;            // CreateGameDialog (0)
    int mActionType;          // AdminActionDialog (unset: 0)
    bool mIsReasonOptional;   // AdminActionDialog (unset: false)
};
static Orig sOrig;

static intptr_t O(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return Host(inOp, a, b, c); }

static void OrigFind(int inMember, int inTableIndex)
{
    // SafeGetComponent(aConfig, this->mX, "Name", __FILE__, line): the SmartPtr assignment holds the new one, then
    // releases the old one
    const EeWonLobbyDlgSlot* aSlot = nullptr;
    (void)inTableIndex;
    intptr_t aNew = 0;
    // the table is the Rust's (names, files, lines); the order of look-ups and members is this driver's own
    extern const EeWonLobbyDlgSlot* SlotOf(Kind, int);
    aSlot = SlotOf(sOrig.mKind, inMember);
    aNew = O(Op::Find, kConfig, (intptr_t)aSlot);
    intptr_t anOld = sOrig.mCtl[inMember];
    sOrig.mCtl[inMember] = aNew;
    if (anOld != 0)
        O(Op::Release, anOld);
}

static void OrigCheckInput();
static void OrigUpdatePasswordVisibility();
static void OrigHandleComponentEvent(intptr_t inEvent, int inType, intptr_t inComponent);

// AdminActionDialog::UpdateDoActionButton
static void OrigUpdateDoActionButton()
{
    if (O(Op::InputTextLength, sOrig.mCtl[2]) != 0 || sOrig.mIsReasonOptional)
        O(Op::SetEnabled, sOrig.mCtl[6], 1);
    else
        O(Op::SetEnabled, sOrig.mCtl[6], 0);
}

static void OrigInit()
{
    switch (sOrig.mKind) {
    case Kind::LanName: case Kind::Password: case Kind::AddFriend:
        OrigFind(0, 0); OrigFind(1, 1); OrigFind(2, 2);
        break;
    case Kind::DirectConnect:
        OrigFind(0, 0); OrigFind(1, 1); OrigFind(2, 2); OrigFind(3, 3);
        break;
    case Kind::LanPort:
        OrigFind(0, 0);
        O(Op::InputSetNumeric, sOrig.mCtl[0]);
        OrigFind(1, 1); OrigFind(2, 2);
        break;
    case Kind::CreateRoom:
        for (int i = 0; i < 5; i++) OrigFind(i, i);
        OrigCheckInput();
        break;
    case Kind::CreateGame:
        for (int i = 0; i < 9; i++) OrigFind(i, i);
        break;
    case Kind::AdminAction:
        // ExplanationHeader, Explanation, DurationHeader, DurationCombo, TimeSegmentCombo, DoActionButton,
        // CancelButton, ApplyToRoomRadio, ApplyToServerRadio, TitleLabel
        for (int i = 1; i < 10; i++) OrigFind(i, i);
        OrigFind(0, 0);
        break;
    }
}

static void OrigCheckInput()
{
    intptr_t* m = sOrig.mCtl;
    switch (sOrig.mKind) {
    case Kind::AddFriend: // this->mOkButton->SetEnabled(ee_won_dlg_name_enabled(this->mNameInput->mText.length()))
        O(Op::SetEnabled, m[1], ee_won_dlg_name_enabled((uint32_t)O(Op::InputShownLength, m[0])));
        break;
    case Kind::LanPort: {
        int aPort = (int)O(Op::InputToInt, m[0]);
        if (ee_won_dlg_lan_port_ok(aPort, sOrig.mGamePort)) {
            if (O(Op::ButtonIsDisabled, m[2]))
                O(Op::SetEnabled, m[2], 1);
        }
        else if (!O(Op::ButtonIsDisabled, m[2]))
            O(Op::SetEnabled, m[2], 0);
        break;
    }
    case Kind::CreateRoom: {
        uint32_t aName = (uint32_t)O(Op::InputShownLength, m[0]);
        bool aShown = O(Op::IsVisible, m[1]) != 0;
        uint32_t aPassword = (uint32_t)O(Op::InputShownLength, m[1]);
        O(Op::SetEnabled, m[4], ee_won_dlg_create_enabled(aName, aShown, aPassword));
        break;
    }
    case Kind::CreateGame: {
        uint32_t aName = (uint32_t)O(Op::InputTextLength, m[0]);
        bool aShown = O(Op::IsVisible, m[2]) != 0;
        uint32_t aPassword = (uint32_t)O(Op::InputShownLength, m[2]);
        O(Op::SetEnabled, m[8], ee_won_dlg_create_enabled(aName, aShown, aPassword));
        break;
    }
    case Kind::AdminAction:
        OrigUpdateDoActionButton();
        break;
    default:
        break;
    }
}

static void OrigUpdatePasswordVisibility()
{
    intptr_t* m = sOrig.mCtl;
    if (sOrig.mKind == Kind::CreateRoom) {
        O(Op::SetVisible, m[1], O(Op::ButtonIsChecked, m[3]));
        O(Op::SetVisible, m[2], O(Op::ButtonIsChecked, m[3]));
        OrigCheckInput();
    }
    else if (sOrig.mKind == Kind::CreateGame) {
        O(Op::SetVisible, m[2], O(Op::ButtonIsChecked, m[5]));
        O(Op::SetVisible, m[3], O(Op::ButtonIsChecked, m[5]));
        OrigCheckInput();
    }
}

static void OrigHandleComponentEvent(intptr_t inEvent, int inType, intptr_t inComponent)
{
    intptr_t* m = sOrig.mCtl;
    const int kText = (int)EeWonLobbyDlgEvent::InputTextChanged, kButton = (int)EeWonLobbyDlgEvent::ButtonPressed,
              kCombo = (int)EeWonLobbyDlgEvent::ComboSelChanged;
    switch (sOrig.mKind) {
    case Kind::AddFriend:
        if (inType == kText) OrigCheckInput();
        else O(Op::BaseEvent, inEvent);
        return;
    case Kind::LanPort:
        if (inType == kText && inComponent == m[0]) OrigCheckInput();
        else O(Op::BaseEvent, inEvent);
        return;
    case Kind::CreateRoom:
        if (inType == kButton && inComponent == m[3]) OrigUpdatePasswordVisibility();
        else if (inType == kText) OrigCheckInput();
        else O(Op::BaseEvent, inEvent);
        return;
    case Kind::CreateGame:
        if (inType == kButton)
            OrigUpdatePasswordVisibility();
        else if (inType == kText) {
            OrigCheckInput();
            return;
        }
        else if (inType == kCombo && inComponent == m[1]) {
            OrigCheckInput();
            return;
        }
        O(Op::BaseEvent, inEvent);
        return;
    case Kind::AdminAction:
        if (inType == kText && inComponent == m[2]) {
            OrigUpdateDoActionButton();
            return;
        }
        O(Op::BaseEvent, inEvent);
        return;
    default: // LanName, Password, DirectConnect: the dialog's own handler
        O(Op::BaseEvent, inEvent);
        return;
    }
}

static void OrigAdminReset(int inAction, intptr_t inTarget)
{
    intptr_t* m = sOrig.mCtl;
    typedef EeWonLobbyDlgString S;
    sOrig.mActionType = inAction;
    S anAction = S::Empty;
    bool showDuration = false, enableReason = false, showScope = false; // unset in the original for another action
    O(Op::ButtonSetCheck, m[9], 0, 0);
    O(Op::ButtonSetCheck, m[8], 1, 0);
    O(Op::InputSetEmpty, m[2], 0);
    O(Op::ComboSetSel, m[4], 1, 0);
    O(Op::ComboSetSel, m[5], 0, 0);
    switch (sOrig.mActionType) {
    case 15: anAction = S::Warn; showDuration = false; enableReason = true; sOrig.mIsReasonOptional = false; showScope = false; break;
    case 8: anAction = S::Mute; showDuration = true; enableReason = true; sOrig.mIsReasonOptional = true; showScope = true; break;
    case 14: anAction = S::UnMute; showDuration = false; enableReason = false; sOrig.mIsReasonOptional = true; showScope = true; break;
    case 7: anAction = S::Kick; showDuration = false; enableReason = true; sOrig.mIsReasonOptional = true; showScope = false; break;
    case 2: anAction = S::Ban; showDuration = true; enableReason = true; sOrig.mIsReasonOptional = false; showScope = true; break;
    }
    O(Op::SetVisible, m[3], showDuration);
    O(Op::SetVisible, m[4], showDuration);
    O(Op::SetVisible, m[5], showDuration);
    O(Op::SetVisible, m[9], showScope);
    O(Op::SetVisible, m[8], showScope);
    O(Op::SetEnabled, m[2], enableReason);
    if (enableReason) O(Op::RequestFocus, m[2]);
    OrigUpdateDoActionButton();
    O(Op::AdminTitle, m[0], (intptr_t)anAction, inTarget);
    if (sOrig.mIsReasonOptional) O(Op::LabelSetString, m[1], (intptr_t)S::ExplanationOpt);
    else O(Op::LabelSetString, m[1], (intptr_t)S::ExplanationReq);
    O(Op::ButtonSetString, m[6], (intptr_t)anAction);
    O(Op::Relayout);
}

static void OrigReset(intptr_t a, intptr_t b)
{
    intptr_t* m = sOrig.mCtl;
    switch (sOrig.mKind) {
    case Kind::LanName: case Kind::Password: // Clear(false); RequestFocus()
        O(Op::InputClear, m[0], 0);
        O(Op::RequestFocus, m[0]);
        break;
    case Kind::AddFriend: // Clear(true); RequestFocus()
        O(Op::InputClear, m[0], 1);
        O(Op::RequestFocus, m[0]);
        break;
    case Kind::DirectConnect:
        O(Op::RequestFocus, m[0]);
        O(Op::InputClear, m[0], 0);
        O(Op::InputClear, m[1], 0);
        break;
    case Kind::LanPort:
        O(Op::RequestFocus, m[0]);
        OrigCheckInput();
        break;
    case Kind::CreateRoom:
        O(Op::RequestFocus, m[0]);
        if (a != 0) {
            O(Op::InputClear, m[0], 0);
            O(Op::InputClear, m[1], 0);
            O(Op::ButtonSetCheck, m[3], 0, 0);
            OrigUpdatePasswordVisibility();
        }
        OrigCheckInput();
        break;
    case Kind::CreateGame:
        O(Op::RequestFocus, m[0]);
        if (a != 0) {
            O(Op::ButtonSetCheck, m[4], 1, 0);
            O(Op::ComboSetSel, m[1], 0, 0);
            O(Op::InputClear, m[0], 0);
            O(Op::InputClear, m[2], 0);
            OrigUpdatePasswordVisibility();
        }
        OrigCheckInput();
        break;
    case Kind::AdminAction:
        OrigAdminReset((int)a, b);
        break;
    }
}

static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c)
{
    intptr_t* m = sOrig.mCtl;
    switch (inCall) {
    case Call::Init: OrigInit(); return 0;
    case Call::Event: OrigHandleComponentEvent(a, (int)b, c); return 0;
    case Call::Reset: OrigReset(a, b); return 0;
    case Call::Slot: return a >= 0 && a < 10 ? m[a] : 0;
    case Call::CheckInput: OrigCheckInput(); return 0;
    case Call::UpdatePasswordVisibility: OrigUpdatePasswordVisibility(); return 0;
    case Call::SetPort: { // sprintf "%d" of the unsigned short; SetText(.., true); SelectRange(0, -1)
        unsigned short aPort = (unsigned short)a;
        O(Op::InputSetNumber, m[0], aPort, 1);
        O(Op::InputSelectRange, m[0], 0, -1);
        return 0;
    }
    case Call::SetGamePort: {
        unsigned short aPort = (unsigned short)a;
        sOrig.mGamePort = aPort;
        O(Op::LabelSetNumber, m[1], aPort);
        return 0;
    }
    case Call::GetPort: return (unsigned short)O(Op::InputToInt, m[0]);
    case Call::SetGameType: {
        sOrig.mGameType = (int)a;
        bool aInternet = ee_won_dlg_internet_access((int)a);
        if (O(Op::ButtonIsChecked, m[6]) || O(Op::ButtonIsChecked, m[7]))
            O(Op::ButtonSetCheck, m[4], 1, 0);
        O(Op::SetVisible, m[6], aInternet);
        O(Op::SetVisible, m[7], aInternet);
        return 0;
    }
    case Call::GetGame: {
        intptr_t aGame = O(Op::GameCreate, 0, sOrig.mGameType);
        O(Op::GameSetName, aGame, m[0]);
        if (O(Op::ButtonIsChecked, m[5]))
            O(Op::GameSetPassword, aGame, m[2]);
        O(Op::GameSetAskToJoin, aGame, O(Op::ButtonIsChecked, m[7]));
        O(Op::GameSetInviteOnly, aGame, O(Op::ButtonIsChecked, m[6]));
        O(Op::GameSetSkill, aGame, ee_won_dlg_skill_level((int)O(Op::ComboGetSel, m[1])));
        return aGame;
    }
    case Call::GetPassword: return O(Op::ButtonIsChecked, m[3]) ? m[1] : 0;
    case Call::ApplyToServer: return O(Op::ButtonIsChecked, m[9]);
    case Call::NumSeconds: {
        unsigned long aSeconds = 3600;
        switch ((int)O(Op::ComboGetSel, m[4])) {
            case 0: aSeconds = 1; break;
            case 1: aSeconds = 5; break;
            case 2: aSeconds = 10; break;
            case 3: aSeconds = 30; break;
            case 4: aSeconds = 0; break;
        }
        switch ((int)O(Op::ComboGetSel, m[5])) {
            case 2: aSeconds *= 24;
            case 1: aSeconds *= 60;
            case 0: aSeconds *= 60;
        }
        return (intptr_t)(uint32_t)aSeconds;
    }
    case Call::Field:
        switch (a) {
        case 0: return sOrig.mGamePort;
        case 1: return sOrig.mGameType;
        case 2: return sOrig.mActionType;
        case 3: return sOrig.mIsReasonOptional;
        }
        return 0;
    }
    return 0;
}

static int SlotCount(Kind inKind)
{
    switch (inKind) {
    case Kind::DirectConnect: return 4;
    case Kind::CreateRoom: return 5;
    case Kind::CreateGame: return 9;
    case Kind::AdminAction: return 10;
    default: return 3;
    }
}

// the original members' SafeGetComponent arguments, in member order (copied from the eight units)
struct OrigSlot { const char* mName; int mLine; int mType; }; // 0 InputBox, 1 Button, 2 Label, 3 ComboBox
static const OrigSlot kOrigSlots[8][10] = {
    {{"LanNameInput", 31, 0}, {"OkButton", 32, 1}, {"CancelButton", 33, 1}},
    {{"PasswordInput", 24, 0}, {"OkButton", 25, 1}, {"CancelButton", 26, 1}},
    {{"NameInput", 25, 0}, {"OkButton", 26, 1}, {"CancelButton", 27, 1}},
    {{"NameInput", 25, 0}, {"AddressInput", 26, 0}, {"AddButton", 27, 1}, {"CancelButton", 28, 1}},
    {{"PortInput", 32, 0}, {"GamePortLabel", 35, 2}, {"OKButton", 36, 1}},
    {{"RoomNameInput", 25, 0}, {"PasswordInput", 26, 0}, {"PasswordLabel", 27, 2}, {"PasswordCheck", 28, 1},
     {"CreateButton", 29, 1}},
    {{"GameNameInput", 34, 0}, {"SkillCombo", 35, 3}, {"PasswordInput", 36, 0}, {"PasswordLabel", 37, 2},
     {"OpenToAllRadio", 38, 1}, {"PasswordRadio", 39, 1}, {"InviteOnlyRadio", 40, 1}, {"AskToJoinRadio", 41, 1},
     {"CreateButton", 42, 1}},
    {{"TitleLabel", 114, 2}, {"ExplanationHeader", 105, 2}, {"Explanation", 106, 0}, {"DurationHeader", 107, 2},
     {"DurationCombo", 108, 3}, {"TimeSegmentCombo", 109, 3}, {"DoActionButton", 110, 1}, {"CancelButton", 111, 1},
     {"ApplyToRoomRadio", 112, 1}, {"ApplyToServerRadio", 113, 1}},
};

// the Rust table's entries, read through a Rust dialog's Find op, checked against kOrigSlots
static const EeWonLobbyDlgSlot* sSlots[8][10];
const EeWonLobbyDlgSlot* SlotOf(Kind inKind, int inMember) { return sSlots[(int)inKind][inMember]; }

static void OrigFree()
{
    // the SmartPtr members destroyed in reverse member order
    for (int i = SlotCount(sOrig.mKind) - 1; i >= 0; i--) {
        intptr_t c = sOrig.mCtl[i];
        sOrig.mCtl[i] = 0;
        if (c != 0) O(Op::Release, c);
    }
}

// ---- the Rust side

static EeWonLobbyDlg* sRust;
static intptr_t RustHost(void*, Op inOp, intptr_t a, intptr_t b, intptr_t c) { return Host(inOp, a, b, c); }
static void RustReenter(intptr_t inComponent)
{
    ee_won_lobbydlg_call(sRust, Call::Event, kEvent, (intptr_t)EeWonLobbyDlgEvent::InputTextChanged, inComponent);
}
static void OrigReenter(intptr_t inComponent)
{
    OrigHandleComponentEvent(kEvent, (int)EeWonLobbyDlgEvent::InputTextChanged, inComponent);
}

// learns the table: a Rust dialog's Init passes each slot to Find; the members' order is Slot's
static const EeWonLobbyDlgSlot* sLearnFound[10];
static int sLearnCount;
static intptr_t LearnHost(void*, Op inOp, intptr_t, intptr_t b, intptr_t)
{
    if (inOp == Op::Find) {
        sLearnFound[sLearnCount] = reinterpret_cast<const EeWonLobbyDlgSlot*>(b);
        return 0x100 + sLearnCount++;
    }
    return 0;
}

static void LearnTables()
{
    for (int k = 0; k < 8; k++) {
        sLearnCount = 0;
        EeWonLobbyDlg* d = ee_won_lobbydlg_new(NULL, (Kind)k, LearnHost);
        ee_won_lobbydlg_call(d, Call::Init, kConfig, 0, 0);
        for (int m = 0; m < SlotCount((Kind)k); m++) {
            intptr_t aPtr = ee_won_lobbydlg_call(d, Call::Slot, m, 0, 0);
            sSlots[k][m] = sLearnFound[aPtr - 0x100];
        }
        ee_won_lobbydlg_free(d);
    }
}

// ---- the sequences

struct Step { Call mCall; intptr_t a, b, c; };

static Step RandomStep(Kind inKind)
{
    Step s = {Call::CheckInput, 0, 0, 0};
    int n = SlotCount(inKind);
    switch (R(12)) {
    case 0: if (R(4) == 0) { s.mCall = Call::Init; s.a = kConfig; break; } // fall through
    case 1: case 2: {
        s.mCall = Call::Event; s.a = kEvent; s.b = R(4);
        s.c = R(4) == 0 ? 0x4444 : Control(0) + 0; // replaced below by a member's address
        if (s.c != 0x4444) s.c = -1 - (intptr_t)R(n); // a member, resolved at the call
        break;
    }
    case 3: case 4: {
        s.mCall = Call::Reset;
        if (inKind == Kind::AdminAction) { static const int kActions[] = {15, 8, 14, 7, 2, 3}; s.a = kActions[R(6)]; s.b = kTarget; }
        else s.a = R(2);
        break;
    }
    case 5: s.mCall = Call::CheckInput; break;
    case 6: s.mCall = Call::Slot; s.a = R(11); break;
    case 7: s.mCall = Call::Field; s.a = R(4); break;
    default:
        switch (inKind) {
        case Kind::LanPort: {
            static const intptr_t kPorts[] = {5000, 4999, 6000, 7777, 65535, 0x12345};
            Call kCalls[] = {Call::SetPort, Call::SetGamePort, Call::GetPort};
            s.mCall = kCalls[R(3)]; s.a = kPorts[R(6)]; break;
        }
        case Kind::CreateRoom: {
            Call kCalls[] = {Call::UpdatePasswordVisibility, Call::GetPassword};
            s.mCall = kCalls[R(2)]; break;
        }
        case Kind::CreateGame: {
            Call kCalls[] = {Call::UpdatePasswordVisibility, Call::SetGameType, Call::GetGame};
            s.mCall = kCalls[R(3)]; s.a = R(5); break;
        }
        case Kind::AdminAction: {
            Call kCalls[] = {Call::ApplyToServer, Call::NumSeconds};
            s.mCall = kCalls[R(2)]; break;
        }
        default: s.mCall = Call::Slot; s.a = R(4); break;
        }
    }
    return s;
}

static std::vector<std::string> RunSide(bool inRust, Kind inKind, unsigned inSeed, const std::vector<Step>& inSteps)
{
    sTrace.clear();
    sSeed = inSeed;
    if (inRust) {
        sRust = ee_won_lobbydlg_new((void*)0xD1A, inKind, RustHost);
        sReenter = RustReenter;
    }
    else {
        memset(&sOrig, 0, sizeof(sOrig));
        sOrig.mKind = inKind;
        sReenter = OrigReenter;
    }
    for (size_t i = 0; i < inSteps.size(); i++) {
        Step s = inSteps[i];
        if (s.mCall == Call::Event && s.c < 0) // a member's address (or 0 before the look-up)
            s.c = inRust ? ee_won_lobbydlg_call(sRust, Call::Slot, -1 - s.c, 0, 0) : sOrig.mCtl[-1 - s.c];
        Line("call %lld %lld %lld %lld", (intptr_t)s.mCall, s.a, s.b, s.c);
        intptr_t r = inRust ? ee_won_lobbydlg_call(sRust, s.mCall, s.a, s.b, s.c) : OrigCall(s.mCall, s.a, s.b, s.c);
        Line("ret %lld", r, 0, 0, 0);
    }
    Line("free", 0, 0, 0, 0);
    if (inRust) { ee_won_lobbydlg_free(sRust); sRust = NULL; }
    else OrigFree();
    sReenter = NULL;
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    LearnTables();
    // every table entry is a distinct slot
    for (int k = 0; k < 8; k++)
        for (int m = 0; m < SlotCount((Kind)k); m++) {
            CHECK(sSlots[k][m] != NULL);
            if (sSlots[k][m] == NULL) continue;
            CHECK(strcmp(sSlots[k][m]->name, kOrigSlots[k][m].mName) == 0);
            CHECK(sSlots[k][m]->line == kOrigSlots[k][m].mLine);
            CHECK((int)sSlots[k][m]->ty == kOrigSlots[k][m].mType);
        }
    // the Field and Slot of a fresh object
    {
        EeWonLobbyDlg* d = ee_won_lobbydlg_new(NULL, Kind::CreateGame, NULL);
        CHECK(ee_won_lobbydlg_call(d, Call::Field, 1, 0, 0) == 0);
        CHECK(ee_won_lobbydlg_call(d, Call::Slot, 0, 0, 0) == 0);
        ee_won_lobbydlg_free(d);
    }
    unsigned long long aLines = 0;
    int aDiffer = 0, aPrinted = 0;
    unsigned aStepSeed = 12345;
    for (int n = 0; n < 20000; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        Kind aKind = (Kind)R(8);
        std::vector<Step> aSteps;
        aSteps.push_back(Step{Call::Init, kConfig, 0, 0});
        for (int i = 0; i < 12; i++) aSteps.push_back(RandomStep(aKind));
        aStepSeed = sSeed;
        sSeed = aSave;
        unsigned aSeed = 777u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunSide(false, aKind, aSeed, aSteps);
        std::vector<std::string> b = RunSide(true, aKind, aSeed, aSteps);
        aLines += a.size();
        sChecks++;
        if (a != b) {
            aDiffer++;
            if (aDiffer <= 3) {
                printf("sequence %d (kind %d) differs\n", n, (int)aKind);
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
            std::string aKey = a[i].substr(0, a[i].find(' ', a[i].find(' ') + 1));
            if (sSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && aNew) {
            printf("seq %d kind %d\n", n, (int)aKind);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
            aPrinted++;
        }
    }
    if (aDiffer) { sFailures++; printf("FAIL: %d of 20000 sequences differ\n", aDiffer); }
    if (!aPrint)
        printf("won_lobbydlg_test: 20000 sequences, %llu trace lines, %zu distinct ops and calls; %d checks, %d failures\n",
               aLines, sSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
