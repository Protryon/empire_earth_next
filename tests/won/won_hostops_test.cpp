// The control hosts' cases R6 WONLobby round 29 made Rust (ee_won_lobby hostops.rs) against the deleted code as it
// was (won_hostops_orig.h), over one stand-in world (won_hostops_world.h) that logs every call. The Rust's calls
// out are answered by World below: the C++ host's new one-statement cases, their statements copied. Each case
// compares the two traces.
//
// - DrawLobbyPing: every ping 0..0xffff at seven heights, with no config, then configs with ceilings 1..65535 (one
//   taken while the ceiling is 5000, as the original's), the searching and failed pictures present or missing and
//   of several heights. (A zero ceiling is not run: the original divided by it.)
// - the game list's row counts, ping and list summaries, the filter row's set and clear, the filter values, the
//   room tree's join (a non-row: rows.rs answers neither read) and creations, the public flag, the event items, the
//   numbers of the labels and combos, the saved game dialog: each over the boundary numbers, the texts, the configs
//   and event types, then the objects' state (texts, the filter's members, the dialog's references).
//
// The exit code is the number of mismatches.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include "rust/won_lobby/hostops.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/resource.h"
#include "rust/won_lobby/rows.h"
#include "won_hostops_world.h"
#include "won_hostops_orig.h"

using namespace WonWorld;
typedef EeWonLobbyCtlOp Op;

static int Lo(intptr_t v) { return (int)(int32_t)(v & 0xffffffff); }
static int Hi(intptr_t v) { return (int)(v >> 32); }
static Image* ResImage(intptr_t inRes) { return gLobbyResources[inRes].mImage.get(); }

static GUIString Txt(intptr_t inData) { return inData != 0 ? GUIString((GUIStringData*)inData) : GUIString(); }
static const GUIString& ResText(intptr_t inRes) { return gLobbyResources[inRes].mString; }

// the host's cases the Rust calls (LobbyNetCtrlHost.cpp's and LobbyCtrlHost.cpp's, as they are)
static intptr_t World(void* inCtlArg, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    Graphics* g = reinterpret_cast<Graphics*>(a);
    Container* inCtl = static_cast<Container*>(inCtlArg);
    ComboBox* aCombo = reinterpret_cast<ComboBox*>(a);
    Label* aLabel = reinterpret_cast<Label*>(a);
    WonHostopsOrig::FilterListItem* aFilter = reinterpret_cast<WonHostopsOrig::FilterListItem*>(a);
    TextListItem* aText = static_cast<TextListItem*>(reinterpret_cast<ListItem*>(a));
    ComponentEvent* anEvent = reinterpret_cast<ComponentEvent*>(a);
    switch (inOp) {
    // the host's older cases the moved ones call
    case Op::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case Op::TextNew: {
        GUIStringData* aData = new GUIStringData(reinterpret_cast<const char*>(a), (unsigned short)b);
        aData->AddRef();
        return (intptr_t)aData;
    }
    case Op::ItemSetText: aText->mText = Txt(b); return 0;
    case Op::ItemSetTextRes: aText->mText = ResText(b); return 0;
    case Op::LabelSetData: aLabel->SetText(Txt(b), true); return 0;
    case Op::ComboAddString: return (intptr_t)aCombo->AddString(Txt(b), -1);
    // the new ones
    case Op::ConfigPublic: return reinterpret_cast<LobbyConfig*>(a)->mPublic ? 1 : 0;
    case Op::ItemTextAppend: aText->mText.append(Txt(b));
        return 0;
    case Op::FilterSetSelPos: aFilter->mSelPos = (int)b;
        return 0;
    case Op::FilterSetIsAdd: aFilter->mIsAdd = b != 0;
        return 0;
    case Op::FilterSetText: aFilter->mFilterText = Txt(b);
        return 0;
    case Op::FilterClearText: aFilter->mFilterText = GUIString("");
        return 0;
    case Op::LabelSetFormatRes: reinterpret_cast<Label*>(a)->SetText(LobbyFormatString(ResText(b), Txt(c)), true);
        return 0;
    case Op::LabelSetFormat2Res: reinterpret_cast<Label*>(a)->SetText(LobbyFormatString2(ResText(b), Txt(reinterpret_cast<const intptr_t*>(c)[0]), Txt(reinterpret_cast<const intptr_t*>(c)[1])), true);
        return 0;
    case Op::FilterCount: return (intptr_t)LobbyGetFilterCount(Txt(a));
    case Op::ComboAddFilterValue: aCombo->AddString(LobbyGetFilterValueAt(Txt(b), (size_t)c), -1);
        return 0;
    case Op::DeliverJoinRoomAt: inCtl->DeliverEvent(JoinRoomEvent(IPAddr::FromValue(ee_won_ipaddr_clone((const EeWonIpAddr*)a)), (unsigned long)b), false);
        return 0;
    case Op::DeliverCreateRoomAt: inCtl->DeliverEvent(CreateRoomEvent(IPAddr::FromValue(ee_won_ipaddr_clone((const EeWonIpAddr*)a))), false);
        return 0;
    case Op::DeliverCreateRoomAny: inCtl->DeliverEvent(CreateRoomEvent(IPAddr()), false);
        return 0;
    case Op::EvType: return anEvent->mType;
    case Op::EvComboItem: return (intptr_t) static_cast<ComboSelChangedEvent*>(anEvent)->mItem.get();
    case Op::EvTreeItem: return (intptr_t) static_cast<TreeItemOpenedEvent*>(anEvent)->mItem.get();
    case Op::EvListItem: return (intptr_t) static_cast<ListItemClickedEvent*>(anEvent)->mItem.get();
    case Op::LabelSetPercentText: reinterpret_cast<Label*>(a)->SetText(LobbyFormatString(EE_LOBBY_STRING(gEELobbyPlayerOptionsCtrl_TransferPercent), Txt(b)));
        return 0;
    case Op::FileDialogGet: return (intptr_t)EELobbyGetFileDialog();
    case Op::FileDialogListSaves: reinterpret_cast<EELobbyFileDialog*>(a)->ListFiles(EELobbyFn2988E9(), ".ees");
        return 0;
    case Op::DoDialog: return LobbyDoDialog(reinterpret_cast<EELobbyFileDialog*>(a));
    case Op::ResImage: return (intptr_t)ResImage(a);
    case Op::ImageHeight: return reinterpret_cast<Image*>(a)->GetHeight();
    case Op::GfxSetColor: g->SetColor((unsigned long)b); return 0;
    case Op::GfxFillRect: g->FillRect(Lo(b), Hi(b), Lo(c), Hi(c)); return 0;
    case Op::GfxDrawImage: g->DrawImage(reinterpret_cast<Image*>(b), Lo(c), Hi(c)); return 0;
    case Op::LobbyConfigGet: return (intptr_t)GetLobbyConfig();
    case Op::ConfigPingCeiling: return reinterpret_cast<LobbyConfig*>(a)->mPingCeiling;
    default: Log("unexpected op %lld", (long long)inOp); return 0;
    }
}

static int sChecks, sFailures;
static std::map<std::string, int> sFailedKinds;
static std::vector<std::string> RunOrig(void (*inRun)())
{
    sTrace.clear();
    inRun();
    std::vector<std::string> aTrace;
    aTrace.swap(sTrace);
    return aTrace;
}
static void Compare(const char* inWhat, long long inA, long long inB, const std::vector<std::string>& o,
                    const std::vector<std::string>& r)
{
    sChecks++;
    if (o == r) return;
    sFailedKinds[std::string(inWhat) + " " + std::to_string(std::string(inWhat) == "op" ? inA : 0)]++;
    if (sFailures++ < 20) {
        printf("FAIL %s %lld %lld:\n", inWhat, inA, inB);
        for (size_t i = 0; i < o.size() || i < r.size(); i++)
            printf("  %-40s | %s\n", i < o.size() ? o[i].c_str() : "", i < r.size() ? r[i].c_str() : "");
    }
}

// ---- DrawLobbyPing

static Graphics sGraphics;
static Font sFont;
static Image sSearching, sFailed;

static void PingCase(unsigned short inPing, int inHeight)
{
    sTrace.clear();
    WonHostopsOrig::DrawLobbyPing(&sGraphics, inPing, inHeight, 0, 0);
    std::vector<std::string> o;
    o.swap(sTrace);
    intptr_t r = ee_won_lobby_hostops_ctl_op(NULL, Op::GfxDrawPing, (intptr_t)&sGraphics, inPing, inHeight, World);
    if (r == INTPTR_MIN) Log("not answered");
    std::vector<std::string> aRust;
    aRust.swap(sTrace);
    Compare("ping", inPing, inHeight, o, aRust);
}

static void Ping()
{
    sGraphics.mFont = &sFont;
    sSearching.mId = 1;
    sFailed.mId = 2;
    static const int kHeights[] = {-3, 0, 1, 2, 5, 12, 31};
    static const unsigned short kCeilings[] = {1, 2, 7, 10, 100, 999, 1000, 4999, 5000, 5001, 20000, 65535};
    // the scenarios: 0 no config throughout; 1.. a config from the first call; the pictures by turn
    for (int aScenario = 0; aScenario <= (int)(sizeof(kCeilings) / sizeof(kCeilings[0])); aScenario++) {
        WonHostopsOrig::sLobbyPingCeiling = 5000;
        ee_won_lobby_hostops_reset();
        LobbyConfig aConfig;
        if (aScenario > 0) aConfig.mPingCeiling = kCeilings[aScenario - 1];
        sConfig = aScenario > 0 ? &aConfig : NULL;
        sSearching.mHeight = aScenario % 9;
        sFailed.mHeight = 20 - aScenario;
        gLobbyResources[(int)EeWonLobbyRes::LobbyGlobal_Searching_Image].mImage = aScenario % 3 == 1 ? NULL : &sSearching;
        gLobbyResources[(int)EeWonLobbyRes::ServerListCtrl_PingFailed_Image].mImage = aScenario % 4 == 2 ? NULL : &sFailed;
        for (int p = 0; p <= 0xffff; p += (p < 300 || p > 0xff00 ? 1 : 7))
            for (int h : kHeights) PingCase((unsigned short)p, h);
        // a config's later ceiling is not taken
        if (aConfig.mPingCeiling != 5000) {
            aConfig.mPingCeiling = 3;
            for (int p = 0; p < 2000; p += 13) PingCase((unsigned short)p, 12);
        }
    }
    sConfig = NULL;
}

// ---- the other cases: each on objects made alike for the two sides, then their state

struct Objects {
    Container mCtl;
    Label mLabel;
    ComboBox mCombo;
    TextListItem mItem;
    WonHostopsOrig::FilterListItem mFilter{GUIString("Map"), NULL};
    ComboSelChangedEvent mCombo1;
    TreeItemOpenedEvent mTree1;
    ListItemClickedEvent mList1;
    ListItem mRows[3];
    Objects()
    {
        mCtl.mId = 11;
        mLabel.mId = 12;
        mCombo.mId = 13;
        mItem.mText = GUIString("old");
        mFilter.mText = GUIString("ftext");
        mFilter.mFilterText = GUIString("typed");
        mFilter.mIsAdd = true;
        mFilter.mSelPos = 4;
        for (int i = 0; i < 3; i++) mRows[i].mId = 20 + i;
        mCombo1.mItem = &mRows[0];
        mTree1.mItem = &mRows[1];
        mList1.mItem = &mRows[2];
    }
    void Dump(intptr_t inResult)
    {
        // an event's item as its row's id
        for (int i = 0; i < 3; i++)
            if (inResult == (intptr_t)&mRows[i]) inResult = 1000 + mRows[i].mId;
        Log("result %lld", (long long)inResult);
        Log(("item " + mItem.mText.s).c_str());
        Log(("filter " + mFilter.mText.s + " | " + mFilter.mFilterText.s + " | " + mFilter.mColumnName.s + " %lld %lld").c_str(),
            mFilter.mIsAdd, mFilter.mSelPos);
    }
};
// the argument kinds: an object of the world, or the value itself
enum Arg { A_None, A_Label, A_Combo, A_Item, A_Filter, A_Event };
static intptr_t ArgOf(Objects& o, int inKind, intptr_t inValue)
{
    switch (inKind) {
    case A_Label: return (intptr_t)&o.mLabel;
    case A_Combo: return (intptr_t)&o.mCombo;
    case A_Item: return (intptr_t)&o.mItem;
    case A_Filter: return (intptr_t)&o.mFilter;
    case A_Event: return inValue == 0 ? (intptr_t)&o.mCombo1 : inValue == 1 ? (intptr_t)&o.mTree1 : (intptr_t)&o.mList1;
    default: return inValue;
    }
}
static GUIStringData* sTexts[3];

typedef intptr_t (*OrigFn)(Container*, EeWonLobbyCtlOp, intptr_t, intptr_t, intptr_t);
// `a` is the object of the kind (an event by `e`), or `av` itself
static void Case(OrigFn inOrig, Op inOp, int inKind, intptr_t av, intptr_t b, intptr_t c, int inEventType = 0)
{
    Objects o, r;
    o.mCombo1.mType = o.mTree1.mType = o.mList1.mType = inEventType;
    r.mCombo1.mType = r.mTree1.mType = r.mList1.mType = inEventType;
    sTrace.clear();
    o.Dump(inOrig(&o.mCtl, inOp, ArgOf(o, inKind, av), b, c));
    std::vector<std::string> aOrig;
    aOrig.swap(sTrace);
    r.Dump(ee_won_lobby_hostops_ctl_op(&r.mCtl, inOp, ArgOf(r, inKind, av), b, c, World));
    std::vector<std::string> aRust;
    aRust.swap(sTrace);
    Compare("op", (long long)inOp, (long long)b, aOrig, aRust);
}
static intptr_t Pack(int lo, int hi) { return (intptr_t)(((uint64_t)(uint32_t)hi << 32) | (uint32_t)lo); }

static void Others()
{
    static const int kNumbers[] = {0, 1, -1, 7, 42, 99, 100, 12345, -98765, 2147483647, (int)0x80000000};
    sTexts[0] = new GUIStringData("");
    sTexts[1] = new GUIStringData("Room");
    sTexts[2] = new GUIStringData("A \"quoted\" name");
    gLobbyResources[(int)EeWonLobbyRes::ServerListCtrl_ListPingSummary_String].mString = GUIString("pings %1");
    gLobbyResources[(int)EeWonLobbyRes::ServerListCtrl_ListSummary_String].mString = GUIString("%1 of %2");
    gLobbyResources[(int)EeWonLobbyRes::ServerListCtrl_AddRemove_String].mString = GUIString("Add/Remove...");
    gLobbyResources[(int)EeWonLobbyRes::ServerListCtrl_Add_String].mString = GUIString("Add (\"");
    gLobbyResources[(int)EeWonLobbyRes::ServerListCtrl_Remove_String].mString = GUIString("Remove (\"");
    for (int n : kNumbers) {
        for (int t = 0; t < 3; t++) {
            for (int aShown : {0, 1, -1, 0x10000})
                Case(WonHostopsOrig::Net, Op::ItemSetTextCount, A_Item, 0, (intptr_t)sTexts[t], Pack(n, aShown));
            for (int aAdd : {0, 1, 2, -1})
                Case(WonHostopsOrig::Net, Op::FilterSet, A_Filter, 0, Pack(n, aAdd), (intptr_t)sTexts[t]);
        }
        Case(WonHostopsOrig::Net, Op::LabelSetPingSummary, A_Label, 0, n, 0);
        for (int m : kNumbers) Case(WonHostopsOrig::Net, Op::LabelSetListSummary, A_Label, 0, n, m);
        Case(WonHostopsOrig::Ctl, Op::LabelSetNumber, A_Label, 0, n, 0);
        Case(WonHostopsOrig::Ctl, Op::ComboAddNumber, A_Combo, 0, n, 0);
        Case(WonHostopsOrig::Ctl, Op::LabelSetPercent, A_Label, 0, n, 0);
    }
    Case(WonHostopsOrig::Net, Op::FilterClear, A_Filter, 0, 0, 0);
    for (int aCount = 0; aCount < 4; aCount++) {
        sFilterValues.clear();
        for (int i = 0; i < aCount; i++) sFilterValues.push_back(std::string(1, (char)('a' + i)));
        for (int t = 0; t < 3; t++) Case(WonHostopsOrig::Net, Op::ComboAddFilterValues, A_Combo, 0, (intptr_t)sTexts[t], 0);
    }
    // not a row (rows.rs answers neither of its reads) and the two room creations
    for (intptr_t v : {(intptr_t)0, (intptr_t)0x1234}) {
        Case(WonHostopsOrig::Small, Op::DeliverJoinRoom, A_None, v, 0, 0);
        Case(WonHostopsOrig::Small, Op::DeliverCreateRoom, A_None, v, 0, 0);
    }
    for (int aConfig = 0; aConfig < 3; aConfig++) {
        LobbyConfig aConfigObject;
        aConfigObject.mPublic = aConfig == 2;
        sConfig = aConfig > 0 ? &aConfigObject : NULL;
        Case(WonHostopsOrig::Small, Op::ConfigIsPublic, A_None, 0, 0, 0);
    }
    sConfig = NULL;
    for (int aType = -1; aType < 40; aType++)
        for (int e = 0; e < 3; e++) Case(WonHostopsOrig::Net, Op::EventItem, A_Event, e, 0, 0, aType);
    for (int aDialog = 0; aDialog < 2; aDialog++) {
        for (int aResult : {-1, 0, 100001, 100002}) {
            // the dialog is the world's own (the glue's): its references after each side's run
            EELobbyFileDialog aOwned;
            aOwned.mId = 14;
            sDialogResult = aResult;
            sFileDialog = aDialog ? &aOwned : NULL;
            sTrace.clear();
            Log("result %lld", (long long)WonHostopsOrig::Ctl(NULL, Op::ChooseSavedGame, 0, 0, 0));
            Log("refs %lld", aOwned.mRefs);
            std::vector<std::string> aOrig;
            aOrig.swap(sTrace);
            Log("result %lld", (long long)ee_won_lobby_hostops_ctl_op(NULL, Op::ChooseSavedGame, 0, 0, 0, World));
            Log("refs %lld", aOwned.mRefs);
            std::vector<std::string> aRust;
            aRust.swap(sTrace);
            Compare("saved game dialog", aDialog, aResult, aOrig, aRust);
        }
    }
    sFileDialog = NULL;
}

int main()
{
    Ping();
    Others();
    for (const auto& k : sFailedKinds) printf("mismatching: %s x %d\n", k.first.c_str(), k.second);
    printf("won_hostops_test: %d cases, %d mismatches\n", sChecks, sFailures);
    return sFailures;
}
