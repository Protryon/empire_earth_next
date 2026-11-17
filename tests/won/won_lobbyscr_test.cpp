// The lobby's screens as lobby dialog kinds (R6 WONLobby round 3): the original RoomDialog.cpp, TOUDialog.cpp,
// BadUserDialog.cpp and ChooseNetworkAdapterDialog.cpp (the statements copied from the C++ before the port, every
// call on a control, a list, the dialog's base or a lobby global replaced by the host op that makes it:
// LobbyDialogHost.cpp runs the same calls) against ee_won's lobbydlg.rs kinds Room, Tou, BadUser and ChooseAdapter
// over one scripted host.
//
// Each of 20,000 sequences makes a dialog of a random kind of the four, its InitComponent, then 12 random inward
// calls (Reset, events of every type including the HTTP document event on members and on other components, the
// bad user list's fill and ids, the selected adapter, the fields, a second InitComponent now and then) and frees it.
// The host answers every op from a seeded generator (the same stream for both sides): the HTTP document's type, the
// selections' count, the adapters (none to three, named or not, one of them perhaps the local host, a handle of 0),
// the selected row. The traces (every op with its arguments and answer, every call's result, the controls released
// at the end) must be the same.
//
// The first 60 sequences and every later one that makes an op or call none before made are
// rust/crates/ee_won_lobby/src/golden_lobbyscr.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include "rust/won_lobby/dialogs.h"
#include "rust/won_lobby/lobbydlg.h"

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
static const intptr_t kDialog = 0xD1A;
static const intptr_t kUsers = 0x8800;
static const intptr_t kIds = 0x8900;
static const intptr_t kAddresses = 0x7000;
static intptr_t Control(int inSlot) { return 0x1000 + 16 * inSlot; }
// the adapters' and the local host's addresses: none, loopback, two private ones (one negative as a 32-bit long)
static const intptr_t kHosts[] = {0, 0x0100007f, 0x0a000001, -1062731775};

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
        for (const char* p = aSlot->name; *p; p++) r = r * 31 + *p;
        r = Control((int)(((uintptr_t)r) % 64));
        snprintf(aBuffer + strlen(aBuffer), sizeof(aBuffer) - strlen(aBuffer), " = %lld", (long long)r);
        sTrace.push_back(aBuffer);
        return r;
    }
    case Op::IsVisible: case Op::ButtonIsChecked: r = R(2); break;
    case Op::HttpDocType: r = R(3); break;
    case Op::ListHasMore: r = R(3) != 0; break;
    case Op::ListNextSel: r = 0x6000 + 16 * R(4); break;
    case Op::BadUserId: r = R(100000); break;
    case Op::LocalHost: case Op::AdapterHandle: case Op::AdapterHost: r = kHosts[R(4)]; break;
    case Op::AdaptersGet: r = kAddresses; break;
    case Op::AdaptersCount: r = R(4); break;
    case Op::AdapterNameLength: r = 5 * R(2); break;
    case Op::ListNumItems: r = 1 + R(5); break;
    case Op::ListGetSel: r = R(2) ? 0x6100 : 0; break;
    default: r = 0; break;
    }
    Line("op %lld %lld %lld %lld", (intptr_t)inOp, a, b, c);
    char aBuffer[40];
    snprintf(aBuffer, sizeof(aBuffer), " = %lld", (long long)r);
    sTrace.back() += aBuffer;
    return r;
}

// ---- the original classes, every call on another object replaced by its op

struct Orig {
    Kind mKind;
    intptr_t mCtl[10];
    bool mNeedGenHTML; // TOUDialog (false from its constructor)
    bool mIsSimple;    // BadUserDialog (false from its constructor)
};
static Orig sOrig;

static intptr_t O(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return Host(inOp, a, b, c); }

extern const EeWonLobbyDlgSlot* SlotOf(Kind, int);

static void OrigFind(int inMember)
{
    // SafeGetComponent(aConfig, this->mX, "Name", __FILE__, line): the SmartPtr assignment holds the new one, then
    // releases the old one
    intptr_t aNew = O(Op::Find, kConfig, (intptr_t)SlotOf(sOrig.mKind, inMember));
    intptr_t anOld = sOrig.mCtl[inMember];
    sOrig.mCtl[inMember] = aNew;
    if (anOld != 0)
        O(Op::Release, anOld);
}

// TOUDialog::Reset
static void OrigTOUReset()
{
    intptr_t* m = sOrig.mCtl;
    if (sOrig.mNeedGenHTML) {
        sOrig.mNeedGenHTML = false;
        O(Op::BrowserShowTou, m[0]); // this->mBrowser->ShowFiles(LobbyGetSysTOUPath(), LobbyGetGameTOUPath(), false)
    }
    O(Op::BrowserTop, m[0]);              // this->mBrowser->GetHTMLView()->SetOffsets(0,0)
    O(Op::ButtonSetCheck, m[1], 0, 0);    // this->mAgreeCheck->SetCheck(false,false)
    O(Op::SetEnabled, m[2], 0);           // this->mAcceptButton->SetEnabled(false)
}

// ChooseNetworkAdapterDialog::Reset
static void OrigAdapterReset()
{
    intptr_t aList = sOrig.mCtl[0];
    O(Op::ListBeginUpdate, aList);
    O(Op::ListClear, aList, 0);
    O(Op::AdapterAddDefault, aList); // InsertItem(new NetworkAdapterListItem(0),-1); SetString(0,0,Default,NULL)
    if (O(Op::LocalHost) == 0) O(Op::ListSetSel, aList, 0, 0);
    intptr_t anAddresses = O(Op::AdaptersGet); // std::list<LobbyNetAddress>; LobbyGetNetAddresses(anAddresses,true)
    intptr_t aCount = O(Op::AdaptersCount, anAddresses);
    for (intptr_t i = 0; i < aCount; i++) {
        intptr_t aHandle = O(Op::AdapterHandle, anAddresses, i); // aNet.mHandle (IPAddr anAddress(aNet.mHandle,0))
        bool anUnnamed = O(Op::AdapterNameLength, anAddresses, i) == 0; // GUIString aName(aNet.mName); length()==0
        O(Op::AdapterInsertRow, aList, aHandle); // InsertItem(new NetworkAdapterListItem(aNet.mHandle),-1)
        if (anUnnamed) O(Op::AdapterSetUnknownName, aList);  // SetString(n-1,0,Unknown,NULL)
        else O(Op::AdapterSetName, aList, anAddresses, i);   // SetString(n-1,0,aName,NULL)
        O(Op::AdapterSetAddress, aList, aHandle);            // SetString(n-1,1,anAddress.GetHostString(true),NULL)
        if (O(Op::LocalHost) == aHandle) O(Op::ListSetSel, aList, O(Op::ListNumItems, aList) - 1, 1);
    }
    O(Op::AdaptersFree, anAddresses); // the local list's destruction
    O(Op::ListEndUpdate, aList);
}

static void OrigInit()
{
    intptr_t* m = sOrig.mCtl;
    switch (sOrig.mKind) {
    case Kind::Room: break; // RoomDialog::InitComponent did nothing
    case Kind::Tou:
        for (int i = 0; i < 4; i++) OrigFind(i);
        O(Op::ButtonSetControlId, m[2], 1);  // this->mAcceptButton->mControlId=1
        O(Op::ButtonSetControlId, m[3], -1); // this->mRejectButton->mControlId=-1
        OrigTOUReset();
        O(Op::SubscribeBroadcast, kDialog, 1); // this->SubscribeToBroadcast(1)
        break;
    case Kind::BadUser:
        for (int i = 0; i < 4; i++) OrigFind(i);
        break;
    case Kind::ChooseAdapter:
        OrigFind(0); OrigFind(1);
        break;
    default: break;
    }
}

static void OrigHandleComponentEvent(intptr_t inEvent, int inType, intptr_t inComponent)
{
    intptr_t* m = sOrig.mCtl;
    if (sOrig.mKind == Kind::Tou) {
        if (inType == (int)EeWonLobbyDlgEvent::GotHttpDocument) {
            // HandleHTTPDocEvent: inEvent->mOp->GetNum(EE_WON_HTTPDOC_TYPE)==HTTPDocType_TOU
            if (O(Op::HttpDocType, inEvent) == 1) sOrig.mNeedGenHTML = true;
            return;
        }
        if (inType == (int)EeWonLobbyDlgEvent::ButtonPressed && inComponent == m[1]) {
            O(Op::SetEnabled, m[2], O(Op::ButtonIsChecked, m[1]));
            return;
        }
    }
    // RoomDialog, BadUserDialog and ChooseNetworkAdapterDialog: the dialog's own handler
    O(Op::BaseEvent, inEvent);
}

static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c)
{
    intptr_t* m = sOrig.mCtl;
    switch (inCall) {
    case Call::Init: OrigInit(); return 0;
    case Call::Event: OrigHandleComponentEvent(a, (int)b, c); return 0;
    case Call::Reset:
        if (sOrig.mKind == Kind::Tou) OrigTOUReset();
        else if (sOrig.mKind == Kind::ChooseAdapter) OrigAdapterReset();
        return 0;
    case Call::Slot: return a >= 0 && a < 10 ? m[a] : 0;
    case Call::Field:
        switch (a) {
        case 5: return sOrig.mNeedGenHTML;
        case 6: return sOrig.mIsSimple;
        }
        return 0;
    case Call::SetUserList: {
        if (sOrig.mKind != Kind::BadUser) return 0;
        bool inSimple = b != 0;
        sOrig.mIsSimple = inSimple;
        O(Op::SetVisible, m[2], !inSimple);
        O(Op::SetVisible, m[3], inSimple);
        intptr_t aList = sOrig.mIsSimple ? m[1] : m[0];
        O(Op::SetVisible, aList, 1);
        O(Op::ListBeginUpdate, aList);
        O(Op::ListClear, aList, 1);
        O(Op::BadUsersInsert, aList, a, inSimple); // for each: InsertItem(new BadUserItem(*anItr,inSimple),-1)
        O(Op::ListEndUpdate, aList);
        return 0;
    }
    case Call::GetIds: {
        if (sOrig.mKind != Kind::BadUser) return 0;
        intptr_t aList = sOrig.mIsSimple ? m[1] : m[0];
        O(Op::IdsClear, a);
        O(Op::ListRewind, aList);
        while (O(Op::ListHasMore, aList)) {
            intptr_t anItem = O(Op::ListNextSel, aList);
            intptr_t anId = O(Op::BadUserId, anItem);
            O(Op::IdsPush, a, anId);
        }
        return 0;
    }
    case Call::SelectedAddress: {
        if (sOrig.mKind != Kind::ChooseAdapter) return 0;
        intptr_t anItem = O(Op::ListGetSel, m[0]);
        uint32_t aHost = anItem == 0 ? 0 : (uint32_t)O(Op::AdapterHost, anItem);
        return (intptr_t)ee_won_dlg_selected_address(anItem != 0, aHost, (uint32_t)O(Op::LocalHost));
    }
    default: return 0;
    }
}

static int SlotCount(Kind inKind)
{
    switch (inKind) {
    case Kind::Tou: case Kind::BadUser: return 4;
    case Kind::ChooseAdapter: return 2;
    default: return 0;
    }
}

// the original members' SafeGetComponent arguments, in member order (copied from the four units)
struct OrigSlot { const char* mName; const char* mFile; int mLine; int mType; };
static const OrigSlot kOrigSlots[4][4] = {
    {},
    {{"Browser", "TOUDialog.cpp", 30, (int)EeWonLobbyDlgType::Browser},
     {"AgreeCheck", "TOUDialog.cpp", 31, (int)EeWonLobbyDlgType::Button},
     {"AcceptButton", "TOUDialog.cpp", 32, (int)EeWonLobbyDlgType::Button},
     {"RejectButton", "TOUDialog.cpp", 33, (int)EeWonLobbyDlgType::Button}},
    {{"UserList", "BadUserDialog.cpp", 157, (int)EeWonLobbyDlgType::MultiList},
     {"SimpleUserList", "BadUserDialog.cpp", 158, (int)EeWonLobbyDlgType::List},
     {"UserListBox", "BadUserDialog.cpp", 159, (int)EeWonLobbyDlgType::Component},
     {"SimpleUserListBox", "BadUserDialog.cpp", 160, (int)EeWonLobbyDlgType::Component}},
    {{"AdapterList", "ChooseNetworkAdapterDialog.cpp", 60, (int)EeWonLobbyDlgType::MultiList},
     {"ActivateButton", "ChooseNetworkAdapterDialog.cpp", 61, (int)EeWonLobbyDlgType::Button}},
};
static const Kind kKinds[4] = {Kind::Room, Kind::Tou, Kind::BadUser, Kind::ChooseAdapter};
static int KindIndex(Kind inKind) { return (int)inKind - (int)Kind::Room; }

// the Rust table's entries, read through a Rust dialog's Find op, checked against kOrigSlots
static const EeWonLobbyDlgSlot* sSlots[4][10];
const EeWonLobbyDlgSlot* SlotOf(Kind inKind, int inMember) { return sSlots[KindIndex(inKind)][inMember]; }

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

// learns the table: a Rust dialog's Init passes each slot to Find; the members' order is Slot's
static const EeWonLobbyDlgSlot* sLearnFound[10];
static int sLearnCount;
static intptr_t LearnHost(void*, Op inOp, intptr_t, intptr_t b, intptr_t)
{
    if (inOp == Op::Find) {
        sLearnFound[sLearnCount] = reinterpret_cast<const EeWonLobbyDlgSlot*>(b);
        return 0x100 + sLearnCount++;
    }
    if (inOp == Op::AdaptersCount || inOp == Op::ListHasMore) return 0;
    return 0;
}

static void LearnTables()
{
    for (int k = 0; k < 4; k++) {
        sLearnCount = 0;
        EeWonLobbyDlg* d = ee_won_lobbydlg_new(NULL, kKinds[k], LearnHost);
        ee_won_lobbydlg_call(d, Call::Init, kConfig, 0, 0);
        CHECK(sLearnCount == SlotCount(kKinds[k]));
        for (int m = 0; m < SlotCount(kKinds[k]); m++) {
            intptr_t aPtr = ee_won_lobbydlg_call(d, Call::Slot, m, 0, 0);
            sSlots[k][m] = aPtr >= 0x100 && aPtr < 0x100 + sLearnCount ? sLearnFound[aPtr - 0x100] : NULL;
        }
        ee_won_lobbydlg_free(d);
    }
}

// ---- the sequences

struct Step { Call mCall; intptr_t a, b, c; };

static Step RandomStep(Kind inKind)
{
    Step s = {Call::Reset, 0, 0, 0};
    int n = SlotCount(inKind);
    switch (R(12)) {
    case 0: if (R(4) == 0) { s.mCall = Call::Init; s.a = kConfig; break; } // fall through
    case 1: case 2: case 3: {
        s.mCall = Call::Event; s.a = kEvent; s.b = R(5);
        s.c = (R(4) == 0 || n == 0) ? 0x4444 : -1 - (intptr_t)R(n); // a member, resolved at the call
        break;
    }
    case 4: case 5: s.mCall = Call::Reset; break;
    case 6: s.mCall = Call::Slot; s.a = R(11); break;
    case 7: s.mCall = Call::Field; s.a = R(7); break;
    default: {
        // the kind's own calls, now and then another kind's (which does nothing)
        static const Call kOwn[4][2] = {{Call::Reset, Call::Reset}, {Call::Reset, Call::Event},
                                        {Call::SetUserList, Call::GetIds},
                                        {Call::SelectedAddress, Call::SelectedAddress}};
        static const Call kAny[] = {Call::SetUserList, Call::GetIds, Call::SelectedAddress};
        if (R(8) == 0) s.mCall = kAny[R(3)];
        else s.mCall = kOwn[KindIndex(inKind)][R(2)];
        if (s.mCall == Call::SetUserList) { s.a = kUsers; s.b = R(2); }
        else if (s.mCall == Call::GetIds) s.a = kIds;
        else if (s.mCall == Call::Event) { s.a = kEvent; s.b = (intptr_t)EeWonLobbyDlgEvent::GotHttpDocument; s.c = 0x4444; }
        break;
    }
    }
    return s;
}

static std::vector<std::string> RunSide(bool inRust, Kind inKind, unsigned inSeed, const std::vector<Step>& inSteps)
{
    sTrace.clear();
    sSeed = inSeed;
    if (inRust)
        sRust = ee_won_lobbydlg_new((void*)kDialog, inKind, RustHost);
    else {
        memset(&sOrig, 0, sizeof(sOrig));
        sOrig.mKind = inKind;
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
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    LearnTables();
    for (int k = 0; k < 4; k++)
        for (int m = 0; m < SlotCount(kKinds[k]); m++) {
            CHECK(sSlots[k][m] != NULL);
            if (sSlots[k][m] == NULL) continue;
            CHECK(strcmp(sSlots[k][m]->name, kOrigSlots[k][m].mName) == 0);
            CHECK(strcmp(sSlots[k][m]->file, kOrigSlots[k][m].mFile) == 0);
            CHECK(sSlots[k][m]->line == kOrigSlots[k][m].mLine);
            CHECK((int)sSlots[k][m]->ty == kOrigSlots[k][m].mType);
        }
    unsigned long long aLines = 0;
    int aDiffer = 0;
    unsigned aStepSeed = 4242;
    for (int n = 0; n < 20000; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        Kind aKind = kKinds[R(4)];
        std::vector<Step> aSteps;
        aSteps.push_back(Step{Call::Init, kConfig, 0, 0});
        for (int i = 0; i < 12; i++) aSteps.push_back(RandomStep(aKind));
        aStepSeed = sSeed;
        sSeed = aSave;
        unsigned aSeed = 991u + (unsigned)n * 7919u;
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
        }
    }
    if (aDiffer) { sFailures++; printf("FAIL: %d of 20000 sequences differ\n", aDiffer); }
    if (!aPrint)
        printf("won_lobbyscr_test: 20000 sequences, %llu trace lines, %zu distinct ops and calls; %d checks, %d failures\n",
               aLines, sSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
