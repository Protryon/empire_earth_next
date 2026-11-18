// ServerListCtrl and its rows as lobbyctl.rs kind ServerList (R6 WONLobby round 13): the original ServerListCtrl.cpp
// (the statements copied from the C++ before the port: the control's methods, its rows' virtuals and column
// handlers, the numeric filter's parser, the config reader's column steps) with every call on a control, a row, a
// game, a text, a graphics context or a lobby global replaced by the host op that makes it (LobbyNetCtrlHost.cpp and
// the model's Rust objects run the same calls), against ee_won_lobby's srvlistctl.rs over one scripted host.
//
// Each of the sequences makes a game list, configures it (a game type, a column count, columns by name with or
// without a filter combo), then makes 1 to 14 random calls (InitComponent; every event the control handles, on its
// own members, on no component and on another one; its timer; its rows' paints, sizes, orders, clicks and selection
// tests; the members read back) and frees it. The host answers each op from a seeded generator (the same stream for
// both sides); a notifying clear or selection of the list, a notifying text or selection of a combo box re-enter the
// control with the matching event a third of the time (two deep at most). The traces must be the same.
//
// The first 60 sequences and every later one that makes an op or call none before made are
// rust/crates/ee_won_lobby/src/golden_srvlist.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include "rust/won_lobby/eeglue.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/modelobj.h"
#include "rust/won_lobby/resource.h"
#include "rust/won_lobby/srvlistctl.h"

typedef EeWonLobbyCtlOp Op;
typedef EeWonLobbyCtlCall Call;
typedef EeWonLobbyCtlEvent Ev;
typedef EeWonLobbyCtlItemField F;
typedef EeWonLobbyCtlRowCall RC;
typedef EeWonLobbyCtlCfg CF;
typedef EeWonModelField MF;
typedef EeWonModelCall MC;
typedef EeWonEeGlueCall GL;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sSeed) % n; }
static std::vector<std::string> sTrace;
static std::set<std::string> sSeen;

static void Line(const char* inFormat, long long a = 0, long long b = 0, long long c = 0, long long d = 0, long long e = 0)
{
    char aBuffer[200];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c, d, e);
    sTrace.push_back(aBuffer);
}

static const intptr_t kCtl = 0x1000, kRemove = 0x2000, kGame = 0x3000, kGames = 0x3900, kItem = 0x5000,
                      kFilterItem = 0x5800, kNewItem = 0x5900, kEvent = 0xE1, kOther = 0x77, kGfx = 0x6600,
                      kListArea = 0x6700, kCombo = 0x6800;
// the filter texts TextBytes answers (the numeric filter's tokens)
static const char* const kFilters[] = {"", "<5", "<=7", ">3", ">=12", "=4", "9", "-2", "+3", "<x", ">=", "=",
                                       "abc", "  <=5  >1 ", ">=-5 <+40", "<2147483647", ">2147483647", "=99999999999",
                                       "\t7", "<= 5", "5 full", "x y z"};
static const int kFilterCount = sizeof(kFilters) / sizeof(kFilters[0]);

static bool sRust;
static EeWonLobbyCtl* sRustCtl;
static int sDepth;
static intptr_t sNextRow, sNextText;
static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c);

static void Reenter(Ev inEvent, intptr_t inComponent)
{
    if (sDepth >= 2 || R(3) != 0) return;
    sDepth++;
    Line("reenter %lld %lld", (long long)inEvent, inComponent);
    if (sRust) ee_won_lobbyctl_call(sRustCtl, Call::Event, kEvent, (intptr_t)inEvent, inComponent);
    else OrigCall(Call::Event, kEvent, (intptr_t)inEvent, inComponent);
    Line("back");
    sDepth--;
}

static intptr_t ModelGet(intptr_t inField)
{
    switch ((MF)inField) {
    case MF::GameType: case MF::GameListType: return R(5);
    case MF::GameName: return 0x7100 + R(4);
    case MF::GameSkill: return (intptr_t)R(8) - 1;
    case MF::GameNumPlayers: return R(9);
    case MF::GameOpenSlots: return R(4) ? R(9) : 0;
    case MF::GamePing: return R(4) ? R(3000) : 0xfff0 + R(16);
    default: return R(2);
    }
}

static intptr_t Host(Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    intptr_t r = 0;
    switch (inOp) {
    case Op::Find: r = R(12) ? kCtl + b * 0x10 : 0; break;
    case Op::FindOptional: r = R(3) ? kRemove : 0; break;
    case Op::EventControlId: r = R(50); break;
    case Op::EventButton: r = R(3); break;
    case Op::EventX: r = R(60); break;
    case Op::EventGame: r = kGame + R(4) * 0x10; break;
    case Op::EventGames: r = kGames; break;
    case Op::EventItem: r = R(3) ? kFilterItem + R(3) : 0; break;
    case Op::MGet: r = ModelGet(b); break;
    case Op::MCall:
        switch ((MC)b) {
        case MC::GameJoinStatus: r = (intptr_t)R(3) - 1; break;
        case MC::GameListHasMore: r = R(4) != 0; break;
        case MC::GameListNext: r = kGame + R(5) * 0x10; break;
        default: r = R(2); break;
        }
        break;
    case Op::RowNew: r = sNextRow++; break;
    case Op::RowGame: r = kGame + R(5) * 0x10; break;
    case Op::RowAddrText: r = 0x7300 + R(3); break;
    case Op::ItemGet:
        switch ((F)b) {
        case F::Height: r = R(30); break;
        case F::Top: r = R(200); break;
        case F::Selected: case F::Selectable: r = R(2); break;
        case F::IconId: r = 0x171 + R(6); break;
        case F::SummaryHeight: r = R(40); break;
        default: r = R(10); break;
        }
        break;
    case Op::RowDetailsHeight: r = R(20); break;
    case Op::DefaultFont: r = 0x4400; break;
    case Op::DefaultFontMod: r = R(5) ? 0x4500 : 0; break;
    case Op::ListFont: r = 0x4600; break;
    case Op::FontHeight: r = R(15); break;
    case Op::ResImageHeight: r = R(20); break;
    case Op::ColumnWidth: r = R(50); break;
    case Op::ListGetSel: r = R(3) ? kItem + R(3) : 0; break;
    case Op::ListNumItems: r = R(6); break;
    case Op::ListItem: r = R(5) ? kItem + b : 0; break;
    case Op::ListItemPos: r = (intptr_t)R(4) - 1; break;
    case Op::ListVertOffset: case Op::ListViewHeight: r = R(100); break;
    case Op::ListItemAtY: r = R(5); break;
    case Op::ListPaintWidth: r = R(200); break;
    case Op::ListPaintColumn: r = (intptr_t)R(8) - 1; break;
    case Op::ListSortColumn: case Op::ListSortColumnPrev: r = R(7); break;
    case Op::GfxStandardColor: r = 0x100 + b; break;
    case Op::GfxSelFore: r = c ^ 0xffffff; break;
    case Op::GfxSelBack: r = 0x123456; break;
    case Op::GfxTextWidth: r = R(80); break;
    case Op::TextNew: r = sNextText++; break;
    case Op::TextCompareNoCase: case Op::GameCompareAddress: r = (intptr_t)R(3) - 1; break;
    case Op::TextFindNoCase: r = (intptr_t)R(4) - 1; break;
    case Op::TextFindRes: r = (intptr_t)R(5) - 2; break;
    case Op::TextLen: r = R(3); break;
    case Op::TextAt: r = R(3) == 0 ? ' ' : 'x'; break;
    case Op::TextBytes: {
        const char* s = kFilters[R(kFilterCount)];
        size_t n = strlen(s);
        memcpy((char*)b, s, n < (size_t)c ? n : (size_t)c);
        r = (intptr_t)n;
        // traced with the bytes (the replay writes them)
        sTrace.push_back("op " + std::to_string((long long)inOp) + " " + std::to_string((long long)a) + " '" + s +
                         "' = " + std::to_string((long long)r));
        return r;
    }
    case Op::TextItemIs: r = R(4) == 0; break;
    case Op::ComboEditText: r = 0x7400 + R(3); break;
    case Op::ComboItem: r = R(4) ? kFilterItem + b : 0; break;
    case Op::ComboCount: r = R(5); break;
    case Op::ComboItemPos: r = (intptr_t)R(4) - 1; break;
    case Op::ComboAddString: r = kNewItem; break;
    case Op::FilterIsAdd: r = R(2); break;
    case Op::FilterSelPos: r = R(4); break;
    case Op::FilterText: r = 0x7200 + R(3); break;
    case Op::FilterColumn: r = 0x7500; break;
    case Op::EeColumn: r = (intptr_t)R(3) - 1; break;
    case Op::PingsToDo: r = (intptr_t)R(4) - 1; break;
    case Op::BaseTimer: r = R(2); break;
    case Op::PrefGet: case Op::IsCombo: case Op::EventIsAdd: case Op::EventIsDelete: case Op::EventIsSelectedItem:
    case Op::EventDoubleClick: case Op::EventActiveSel:
        r = R(5) != 0; break;
    default: break;
    }
    // a made text is traced by its bytes, a buffer not at all (its address differs between the sides)
    if (inOp == Op::TextNew) {
        std::string aText((const char*)a, (size_t)b);
        sTrace.push_back("op " + std::to_string((long long)inOp) + " '" + aText + "' = " + std::to_string((long long)r));
    } else Line("op %lld %lld %lld %lld = %lld", (long long)inOp, a, b, c, r);
    if (inOp == Op::ListClear && b != 0) Reenter(Ev::ListSelChanged, kOther);
    if (inOp == Op::ListSetSel && c != 0) Reenter(Ev::ListSelChanged, kOther);
    if (inOp == Op::ComboSetText) Reenter(Ev::InputTextChanged, a);
    if (inOp == Op::ComboSetSel && c != 0) Reenter(Ev::ComboSelChanged, a);
    if (inOp == Op::ComboClear && b != 0) Reenter(Ev::ComboSelChanged, a);
    return r;
}
static intptr_t O(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return Host(inOp, a, b, c); }
static intptr_t RustHost(void*, Op inOp, intptr_t a, intptr_t b, intptr_t c) { return Host(inOp, a, b, c); }

// ---- the original classes, every call on another object replaced by its op

// the column handlers (the original's function pointers): a ColumnInfo's
enum Fn { None, Icon, Skill, GameName, Address, NumPlayers, Ping, Epoch, MapType };
struct ColumnInfo {
    Fn mFunc;
    intptr_t mFilterCombo;
    unsigned char mGameTypeFlags;
    std::string mColumnName;
    ColumnInfo() : mFunc(None), mFilterCombo(0), mGameTypeFlags(0xff) {}
};
struct NoCaseLess {
    bool operator()(const std::string& a, const std::string& b) const
    {
        for (size_t i = 0; i < a.size() && i < b.size(); i++) {
            int x = tolower((unsigned char)a[i]), y = tolower((unsigned char)b[i]);
            if (x != y) return x < y;
        }
        return a.size() < b.size();
    }
};
static std::map<std::string, ColumnInfo, NoCaseLess> sColumnMap;
static ColumnInfo* GetColumnInfo(const char* inName)
{
    ColumnInfo& aColumn = sColumnMap[inName];
    aColumn.mColumnName = inName;
    return &aColumn;
}
static void InitColumnMap()
{
    GetColumnInfo("Icon")->mFunc = Icon;
    GetColumnInfo("Skill")->mFunc = Skill;
    GetColumnInfo("GameName")->mFunc = GameName;
    ColumnInfo* aColumn = GetColumnInfo("Address");
    aColumn->mFunc = Address;
    aColumn->mGameTypeFlags = 4;
    GetColumnInfo("NumPlayers")->mFunc = NumPlayers;
    aColumn = GetColumnInfo("Ping");
    aColumn->mFunc = Ping;
    aColumn->mGameTypeFlags &= ~2;
    // the EE game's (EELobbyGlueHost.cpp's SetColumnHandlers)
    GetColumnInfo("Epoch")->mFunc = Epoch;
    GetColumnInfo("MapType")->mFunc = MapType;
}

typedef std::list<intptr_t> ServerDetailsList;
struct Orig {
    std::vector<ColumnInfo> mColumnInfo;
    int mGameType;
    bool mNeedFilter;
    int mFilterTimeAccum;
    int mNumColumns;
    intptr_t mBoldFont, mList, mJoinButton, mRefreshButton, mRemoveButton, mSummaryLabel;
    std::map<intptr_t, intptr_t> mServerDetailsMap; // the games' pointers in order (SmartPtr's operator<)
};
static Orig* sO;

static void Assign(intptr_t& ioMember, intptr_t inNew)
{
    if (ioMember == inNew) return;
    if (ioMember != 0) O(Op::Release, ioMember);
    ioMember = inNew;
    if (inNew != 0) O(Op::AddRef, inNew);
}
static void ReleasePtr(intptr_t& ioPtr) { if (ioPtr != 0) O(Op::Release, ioPtr); ioPtr = 0; }

static intptr_t Pt(int x, int y) { return (intptr_t)(uint32_t)x | ((intptr_t)(uint32_t)y << 32); }
static intptr_t Game(intptr_t inItem) { return O(Op::RowGame, inItem); }
static intptr_t Get(intptr_t inObj, MF inField) { return O(Op::MGet, inObj, (intptr_t)inField); }
static intptr_t Item(intptr_t inItem, F inField) { return O(Op::ItemGet, inItem, (intptr_t)inField); }
static void SetItem(intptr_t inItem, F inField, intptr_t v) { O(Op::ItemSet, inItem, (intptr_t)inField, v); }
static bool CanJoin(intptr_t inGame) { return (short)O(Op::MCall, inGame, (intptr_t)MC::GameJoinStatus, 0) == 0; }
static const int kGrayText = 6, kText = 14;
// the resource rows: ServerListCtrl_GameExpanded_Image, _GameNotExpanded_Image, _Querying_String, _QueryFailed_String,
// _NotFullFilter_String, _FullFilter_String, _OpenFilter_String, _StopRefresh_String, _RefreshList_String
static intptr_t Res(EeWonLobbyRes inRes) { return (intptr_t)inRes; }

// ServerDetailsItem
static void SetGame(intptr_t inItem, intptr_t inGame)
{
    O(Op::RowSetGame, inItem, inGame);
    if (Get(Game(inItem), MF::GameType) == 3) O(Op::RowSetAddrText, inItem);
}
static void CalcIcon(intptr_t t)
{
    if (Get(Game(t), MF::GameInProgress)) SetItem(t, F::IconId, 0x176);
    else if (Get(Game(t), MF::GameInviteOnly)) {
        bool aOwned = Get(Game(t), MF::GameInvited) || Get(Game(t), MF::GameIsHost);
        if (aOwned) SetItem(t, F::IconId, 0x171);
        else SetItem(t, F::IconId, 0x175);
    } else if (Get(Game(t), MF::GameAskToJoin)) SetItem(t, F::IconId, 0x173);
    else if (Get(Game(t), MF::GameHasPassword)) SetItem(t, F::IconId, 0x174);
    else SetItem(t, F::IconId, 0x172);
}
static int CompareFn(Fn f, intptr_t a, intptr_t b)
{
    switch (f) {
    case Icon: return (int)Item(a, F::IconId) - (int)Item(b, F::IconId);
    case Skill: return (int)Get(Game(a), MF::GameSkill) - (int)Get(Game(b), MF::GameSkill);
    case GameName: {
        intptr_t na = Get(Game(a), MF::GameName);
        return (int)O(Op::TextCompareNoCase, na, Get(Game(b), MF::GameName));
    }
    case Address: {
        intptr_t ga = Game(a);
        return (int)O(Op::GameCompareAddress, ga, Game(b));
    }
    case NumPlayers: {
        int result = (int)Get(Game(a), MF::GameNumPlayers) - (int)Get(Game(b), MF::GameNumPlayers);
        if (result == 0) result = (int)Get(Game(b), MF::GameOpenSlots) - (int)Get(Game(a), MF::GameOpenSlots);
        return result;
    }
    case Ping: return (int)(unsigned short)Get(Game(a), MF::GamePing) - (int)(unsigned short)Get(Game(b), MF::GamePing);
    case Epoch: return (int)O(Op::EeColumn, (intptr_t)GL::EpochCompare, a, b);
    case MapType: return (int)O(Op::EeColumn, (intptr_t)GL::MapTypeCompare, a, b);
    default: return 0;
    }
}
static bool IsStartOfNumber(char c)
{
    return ((static_cast<unsigned int>(static_cast<int>(c)) < 256 && isdigit(c)) || c == '-' || c == '+') ? 1 : 0;
}
// MSVC's atoi (the original's): out of range clamped
static int Atoi(const char* s)
{
    long long v = strtoll(s, NULL, 10);
    if (v > 2147483647LL) return 2147483647;
    if (v < -2147483647LL - 1) return (int)(-2147483647LL - 1);
    return (int)v;
}
static std::string TextString(intptr_t inText)
{
    char aBytes[1024];
    intptr_t n = O(Op::TextBytes, inText, (intptr_t)aBytes, sizeof(aBytes));
    return std::string(aBytes, (size_t)n);
}
static bool ParseIntFilter(intptr_t inFilter, int& outLower, int& outUpper)
{
    std::string aFilter = TextString(inFilter);
    char aBuffer[512];
    strncpy(aBuffer, aFilter.c_str(), 510);
    aBuffer[510] = 0; // (uninitialised in the original past a 510-byte text)
    aBuffer[511] = 0;
    bool aParsed = false;
    for (char* aToken = strtok(aBuffer, " "); aToken != NULL; aToken = strtok(NULL, " ")) {
        if (aToken[0] == '<') {
            if (aToken[1] == '=' && IsStartOfNumber(aToken[2])) { outUpper = Atoi(aToken + 2); aParsed = true; }
            else if (IsStartOfNumber(aToken[1])) { aParsed = true; outUpper = (int)((unsigned)Atoi(aToken + 1) - 1u); }
        } else if (aToken[0] == '>') {
            if (aToken[1] == '=' && IsStartOfNumber(aToken[2])) { aParsed = true; outLower = Atoi(aToken + 2); }
            else if (IsStartOfNumber(aToken[1])) { aParsed = true; outLower = (int)((unsigned)Atoi(aToken + 1) + 1u); }
        } else if (aToken[0] == '=') {
            if (IsStartOfNumber(aToken[1])) { outUpper = Atoi(aToken + 1); outLower = outUpper; aParsed = true; }
        } else if (IsStartOfNumber(aToken[0])) { outUpper = Atoi(aToken); outLower = outUpper; aParsed = true; }
    }
    return aParsed;
}
static void FilterFn(Fn f, intptr_t inFilter, ServerDetailsList& ioList)
{
    switch (f) {
    case Skill: {
        int aLower = -100000, aUpper = 100000;
        if (!ParseIntFilter(inFilter, aLower, aUpper)) return;
        for (ServerDetailsList::iterator it = ioList.begin(); it != ioList.end();) {
            intptr_t anItem = *it;
            if ((int)Get(Game(anItem), MF::GameSkill) < aLower || (int)Get(Game(anItem), MF::GameSkill) > aUpper) ioList.erase(it++);
            else ++it;
        }
        return;
    }
    case GameName:
        for (ServerDetailsList::iterator it = ioList.begin(); it != ioList.end();) {
            intptr_t aName = Get(Game(*it), MF::GameName);
            if ((int)O(Op::TextFindNoCase, aName, inFilter) >= 0) ++it;
            else ioList.erase(it++);
        }
        return;
    case Address:
        for (ServerDetailsList::iterator it = ioList.begin(); it != ioList.end();) {
            intptr_t anAddr = O(Op::RowAddrText, *it);
            if ((int)O(Op::TextFindNoCase, anAddr, inFilter) >= 0) ++it;
            else ioList.erase(it++);
        }
        return;
    case Ping: {
        int aLower = -100000, aUpper = 100000;
        if (!ParseIntFilter(inFilter, aLower, aUpper)) return;
        for (ServerDetailsList::iterator it = ioList.begin(); it != ioList.end();) {
            int aPing = 10 - (int)(unsigned short)Get(Game(*it), MF::GamePing) / 100;
            if (aPing < aLower || aPing > aUpper) ioList.erase(it++);
            else ++it;
        }
        return;
    }
    case NumPlayers: {
        bool aNotFull = false, aOpen = false, aFull = false;
        int aPos = (int)O(Op::TextFindRes, inFilter, Res(EeWonLobbyRes::ServerListCtrl_NotFullFilter_String));
        if (aPos == 0 || (aPos > 0 && O(Op::TextAt, inFilter, aPos - 1) == ' ')) aNotFull = true;
        aPos = (int)O(Op::TextFindRes, inFilter, Res(EeWonLobbyRes::ServerListCtrl_FullFilter_String));
        if (aPos == 0 || (aPos > 0 && O(Op::TextAt, inFilter, aPos - 1) == ' ')) aFull = true;
        aPos = (int)O(Op::TextFindRes, inFilter, Res(EeWonLobbyRes::ServerListCtrl_OpenFilter_String));
        if (aPos == 0 || (aPos > 0 && O(Op::TextAt, inFilter, aPos - 1) == ' ')) aOpen = true;
        if (aFull && aNotFull) { ioList.clear(); return; }
        int aLower = -100000, aUpper = 100000;
        if (!ParseIntFilter(inFilter, aLower, aUpper) && !aFull && !aNotFull && !aOpen) return;
        for (ServerDetailsList::iterator it = ioList.begin(); it != ioList.end();) {
            intptr_t anItem = *it;
            int aCount = (int)Get(Game(anItem), MF::GameNumPlayers);
            int aMax = (int)Get(Game(anItem), MF::GameOpenSlots);
            if (aMax == 0) aMax = 0x7ffffff;
            if (aCount < aLower || aCount > aUpper || (aFull && aCount < aMax) || (aNotFull && aCount >= aMax) ||
                (aOpen && !CanJoin(Game(anItem)))) ioList.erase(it++);
            else ++it;
        }
        return;
    }
    case Epoch: case MapType:
        for (ServerDetailsList::iterator it = ioList.begin(); it != ioList.end();) {
            if (O(Op::EeColumn, (intptr_t)(f == Epoch ? GL::EpochKeep : GL::MapTypeKeep), *it, inFilter) != 0) ++it;
            else ioList.erase(it++);
        }
        return;
    default: return;
    }
}
static void CalcDimensions(intptr_t t, intptr_t inList)
{
    intptr_t aFont = O(Op::DefaultFont, inList);
    CalcIcon(t);
    SetItem(t, F::SummaryHeight, (unsigned char)O(Op::FontHeight, aFont));
    unsigned short imageHeight = (unsigned short)O(Op::ResImageHeight, Res(EeWonLobbyRes::ServerListCtrl_GameExpanded_Image));
    if (imageHeight > Item(t, F::SummaryHeight)) SetItem(t, F::SummaryHeight, (unsigned char)imageHeight);
    SetItem(t, F::SummaryHeight, (unsigned char)(Item(t, F::SummaryHeight) + 2));
    SetItem(t, F::Height, Item(t, F::SummaryHeight));
    if (!Get(Game(t), MF::GameShowDetails)) return;
    int aHeight = (int)O(Op::RowDetailsHeight, t, inList);
    if (aHeight < (int)O(Op::FontHeight, aFont)) aHeight = (int)O(Op::FontHeight, aFont);
    SetItem(t, F::Height, (int)Item(t, F::Height) + aHeight);
}
static bool CanSelect(intptr_t t, intptr_t inList, int x, int y)
{
    return (Item(t, F::Selectable) && x > (int)O(Op::ColumnWidth, inList, 0) && y <= (int)Item(t, F::SummaryHeight)) ? 1 : 0;
}
static void MouseDown(intptr_t t, intptr_t inList, int x, int y, int inButton)
{
    if (inButton != 0) return;
    if (x > (int)O(Op::ColumnWidth, inList, 0) || y > (int)Item(t, F::SummaryHeight)) return;
    intptr_t aGame = Game(t);
    O(Op::MSet, aGame, (intptr_t)MF::GameShowDetails, !Get(Game(t), MF::GameShowDetails));
    O(Op::ListRefresh, inList, t);
    O(Op::ListEnsureVisible, inList, t);
    if (Get(Game(t), MF::GameShowDetails)) O(Op::ListDeliverPing, inList, Game(t));
}
static void DrawServerImage(intptr_t g, intptr_t inImage, intptr_t inItem, int x)
{
    int h = (int)Item(inItem, F::SummaryHeight);
    O(Op::GfxDrawResImage, g, inImage, Pt(x, (h - (int)O(Op::ResImageHeight, inImage)) / 2));
}
static void PaintFn(Fn f, intptr_t g, intptr_t inItem)
{
    switch (f) {
    case Icon:
        if (Get(Game(inItem), MF::GameShowDetails))
            DrawServerImage(g, Res(EeWonLobbyRes::ServerListCtrl_GameExpanded_Image), inItem, 15);
        else DrawServerImage(g, Res(EeWonLobbyRes::ServerListCtrl_GameNotExpanded_Image), inItem, 15);
        DrawServerImage(g, Item(inItem, F::IconId), inItem, 0);
        return;
    case Skill: {
        unsigned char aSkill = (unsigned char)Get(Game(inItem), MF::GameSkill);
        if (aSkill < 5) DrawServerImage(g, aSkill + 0x177, inItem, 0);
        return;
    }
    case GameName:
        O(Op::GfxSetFont, g, sO->mBoldFont);
        O(Op::GfxDrawText, g, Get(Game(inItem), MF::GameName), Pt(5, 0));
        return;
    case Address: O(Op::GfxDrawText, g, O(Op::RowAddrText, inItem), 0); return;
    case NumPlayers: {
        char aBuffer[50];
        if ((int)Get(Game(inItem), MF::GameOpenSlots) > 0) {
            // the arguments right to left (MSVC's order)
            int aMax = (int)Get(Game(inItem), MF::GameOpenSlots);
            int aNum = (int)Get(Game(inItem), MF::GameNumPlayers);
            sprintf(aBuffer, "%d/%d", aNum, aMax);
        } else sprintf(aBuffer, "%d", (int)Get(Game(inItem), MF::GameNumPlayers));
        intptr_t aText = O(Op::TextNew, (intptr_t)aBuffer, (intptr_t)strlen(aBuffer));
        int aWidth = (int)O(Op::ListPaintWidth, sO->mList);
        int aTextWidth = (int)O(Op::GfxTextWidth, g, aText);
        O(Op::GfxDrawText, g, aText, Pt((aWidth - aTextWidth) / 2, 0));
        O(Op::Release, aText);
        return;
    }
    case Ping: {
        unsigned short aPing = (unsigned short)Get(Game(inItem), MF::GamePing);
        O(Op::GfxDrawPing, g, aPing, Item(inItem, F::SummaryHeight));
        return;
    }
    case Epoch: O(Op::EeColumn, (intptr_t)GL::EpochPaint, g, inItem); return;
    case MapType: O(Op::EeColumn, (intptr_t)GL::MapTypePaint, g, inItem); return;
    default: return;
    }
}
static void Paint(intptr_t t, intptr_t g, intptr_t aList)
{
    int aColumn = (int)O(Op::ListPaintColumn, aList);
    unsigned long aColor;
    if (Get(Game(t), MF::GameInProgress)) aColor = (unsigned long)O(Op::GfxStandardColor, g, kGrayText);
    else aColor = (unsigned long)O(Op::GfxStandardColor, g, kText);
    if (Item(t, F::Selected)) aColor = (unsigned long)O(Op::GfxSelFore, g, aList, aColor);
    O(Op::GfxSetColor, g, aColor);
    O(Op::GfxSetFont, g, O(Op::ListFont, aList));
    if (aColumn == -1) {
        if (Item(t, F::Selected)) {
            O(Op::GfxSetColor, g, O(Op::GfxSelBack, g, aList));
            int w = (int)O(Op::ListPaintWidth, aList);
            O(Op::GfxFillRect, g, 0, Pt(w, (int)Item(t, F::SummaryHeight)));
        }
        if (!Get(Game(t), MF::GameShowDetails)) return;
        int aWidth = (int)O(Op::ColumnWidth, aList, 0);
        O(Op::GfxTranslate, g, aWidth, Item(t, F::SummaryHeight));
        if (Get(Game(t), MF::GameHaveDetails)) O(Op::RowPaintDetails, t, g, aList);
        else {
            O(Op::GfxSetFont, g, O(Op::ListFont, aList));
            O(Op::GfxSetSysColor, g, kText);
            if (O(Op::MCall, Game(t), (intptr_t)MC::GameQuerying, 0)) O(Op::GfxDrawResText, g, Res(EeWonLobbyRes::ServerListCtrl_Querying_String), 0);
            else O(Op::GfxDrawResText, g, Res(EeWonLobbyRes::ServerListCtrl_QueryFailed_String), 0);
        }
        O(Op::GfxTranslate, g, -aWidth, -(int)Item(t, F::SummaryHeight));
        return;
    }
    if (aColumn >= 0 && (size_t)aColumn < sO->mColumnInfo.size() && sO->mColumnInfo[aColumn].mFunc != None)
        PaintFn(sO->mColumnInfo[aColumn].mFunc, g, t);
}
static int Compare(intptr_t t, intptr_t anOther, intptr_t aList, intptr_t inFallback)
{
    if (t == anOther) return 0;
    int aSecondaryColumn = (int)O(Op::ListSortColumnPrev, aList);
    int aPrimary = (int)O(Op::ListSortColumn, aList);
    int aResult;
    if ((size_t)aPrimary < sO->mColumnInfo.size() && sO->mColumnInfo[aPrimary].mFunc != None) {
        aResult = CompareFn(sO->mColumnInfo[aPrimary].mFunc, t, anOther);
        if (aResult != 0) return aResult;
    }
    if ((size_t)aSecondaryColumn < sO->mColumnInfo.size() && sO->mColumnInfo[aSecondaryColumn].mFunc != None) {
        aResult = CompareFn(sO->mColumnInfo[aSecondaryColumn].mFunc, t, anOther);
        if (aResult != 0) return aResult;
    }
    return (int)inFallback;
}

// ServerListCtrl
static intptr_t GetSelGame()
{
    intptr_t aSelected = O(Op::ListGetSel, sO->mList);
    return aSelected == 0 ? 0 : Game(aSelected);
}
static void DoFilter(ServerDetailsList& ioList)
{
    intptr_t aSelectedGame = GetSelGame();
    if (aSelectedGame) O(Op::AddRef, aSelectedGame);
    for (unsigned int i = 0; i < sO->mColumnInfo.size(); ++i) {
        ColumnInfo aColumn = sO->mColumnInfo[i];
        bool aHasFilter = aColumn.mFunc != None && aColumn.mFunc != Icon;
        if (aHasFilter && aColumn.mFilterCombo != 0 && O(Op::TextLen, O(Op::ComboEditText, aColumn.mFilterCombo)) != 0)
            FilterFn(aColumn.mFunc, O(Op::ComboEditText, aColumn.mFilterCombo), ioList);
    }
    if (sO->mGameType != 3 && !O(Op::PrefGet, (intptr_t)EeWonLobbyCtlPref::ShowGamesInProgress))
        for (ServerDetailsList::iterator it = ioList.begin(); it != ioList.end();) {
            if (Get(Game(*it), MF::GameInProgress)) ioList.erase(it++);
            else ++it;
        }
    if (aSelectedGame) O(Op::Release, aSelectedGame);
}
static bool FitsFilter(intptr_t inItem)
{
    ServerDetailsList aList;
    aList.push_back(inItem);
    DoFilter(aList);
    return !aList.empty();
}
static void UpdateJoinButton()
{
    bool aEnabled = false;
    intptr_t aGame = GetSelGame();
    if (aGame != 0) aEnabled = CanJoin(aGame);
    O(Op::SetEnabled, sO->mJoinButton, aEnabled);
    if (sO->mRemoveButton != 0) O(Op::SetEnabled, sO->mRemoveButton, aGame != 0);
}
static void UpdateSummaryLabel()
{
    if (sO->mSummaryLabel == 0) return;
    intptr_t aTotal = (intptr_t)sO->mServerDetailsMap.size();
    if ((int)O(Op::PingsToDo) > 0) {
        O(Op::LabelSetPingSummary, sO->mSummaryLabel, O(Op::PingsToDo));
        O(Op::ButtonSetRes, sO->mRefreshButton, Res(EeWonLobbyRes::ServerListCtrl_StopRefresh_String));
    } else {
        O(Op::LabelSetListSummary, sO->mSummaryLabel, O(Op::ListNumItems, sO->mList), aTotal);
        O(Op::ButtonSetRes, sO->mRefreshButton, Res(EeWonLobbyRes::ServerListCtrl_RefreshList_String));
    }
}
static void AddEntries(const ServerDetailsList& inList)
{
    intptr_t aSelectedGame = GetSelGame();
    O(Op::ListBeginUpdate, sO->mList);
    O(Op::ListClear, sO->mList, 1);
    for (ServerDetailsList::const_iterator it = inList.begin(); it != inList.end(); ++it) {
        intptr_t aItem = *it;
        O(Op::ListInsert, sO->mList, aItem);
        O(Op::MSet, Game(aItem), (intptr_t)MF::GameFiltered, 0);
        if (Game(aItem) == aSelectedGame) {
            int aPos = (int)O(Op::ListItemPos, sO->mList, aItem);
            O(Op::ListSetSel, sO->mList, aPos, 1);
        }
    }
    O(Op::ListEndUpdate, sO->mList);
    UpdateJoinButton();
    UpdateSummaryLabel();
}
static void AddFilteredEntries(bool inPreserve, ServerDetailsList& ioList)
{
    sO->mNeedFilter = false;
    std::list<std::pair<intptr_t, int> > aOldItems;
    if (inPreserve) {
        int aOffset = (int)O(Op::ListVertOffset, sO->mList);
        int aBottom = (int)O(Op::ListVertOffset, sO->mList) + (int)O(Op::ListViewHeight, sO->mList);
        int anIndex = (int)O(Op::ListItemAtY, sO->mList, aOffset);
        intptr_t anItem = O(Op::ListItem, sO->mList, anIndex);
        if (anItem != 0 && ((int)Item(anItem, F::Height) + (int)Item(anItem, F::Top)) / 2 < aOffset) ++anIndex;
        while (true) {
            intptr_t anOldItem = O(Op::ListItem, sO->mList, anIndex++);
            if (anOldItem == 0) break;
            if ((int)Item(anOldItem, F::Top) > aBottom) break;
            aOldItems.push_back(std::pair<intptr_t, int>(anOldItem, (int)Item(anOldItem, F::Top) - aOffset));
        }
    }
    DoFilter(ioList);
    AddEntries(ioList);
    if (inPreserve) {
        for (std::list<std::pair<intptr_t, int> >::iterator it = aOldItems.begin(); it != aOldItems.end(); ++it) {
            intptr_t anItem = it->first;
            if ((int)O(Op::ListItemPos, sO->mList, anItem) >= 0) {
                O(Op::ListSetVertOffset, sO->mList, (int)Item(anItem, F::Top) - it->second);
                break;
            }
        }
    }
}
static void AddFilteredEntries_All(bool inPreserve)
{
    ServerDetailsList aList;
    for (std::map<intptr_t, intptr_t>::iterator it = sO->mServerDetailsMap.begin(); it != sO->mServerDetailsMap.end(); ++it) {
        O(Op::MSet, Game(it->second), (intptr_t)MF::GameFiltered, 1);
        aList.push_back(it->second);
    }
    AddFilteredEntries(inPreserve, aList);
}
static void AddFilteredEntries_List(bool inPreserve)
{
    ServerDetailsList aList;
    int aCount = (int)O(Op::ListNumItems, sO->mList);
    for (int i = 0; i < aCount; ++i) aList.push_back(O(Op::ListItem, sO->mList, i));
    AddFilteredEntries(inPreserve, aList);
}
static bool TimerEvent(int inDelta)
{
    if (sO->mNeedFilter) {
        sO->mFilterTimeAccum += inDelta;
        if (sO->mFilterTimeAccum >= 100 &&
            (sO->mFilterTimeAccum >= 500 || static_cast<unsigned int>(sO->mFilterTimeAccum) >= sO->mServerDetailsMap.size() / 4)) {
            AddFilteredEntries_All(true);
            sO->mNeedFilter = false;
        }
    }
    if (!sO->mNeedFilter) O(Op::ClearTimerFlag);
    return O(Op::BaseTimer, inDelta) != 0;
}
static bool HandleRefreshButton()
{
    if ((int)O(Op::PingsToDo) > 0) {
        O(Op::KillPings);
        AddFilteredEntries_List(true);
        return true;
    }
    return false;
}
static void HandleListItemClicked(intptr_t inEvent)
{
    if (!O(Op::EventIsSelectedItem, inEvent)) return;
    intptr_t aSelected = O(Op::ListGetSel, sO->mList);
    if (aSelected == 0) return;
    intptr_t aGame = Game(aSelected);
    if ((int)O(Op::EventButton, inEvent) == 2) O(Op::DeliverGameAction, aGame);
    else if (O(Op::EventDoubleClick, inEvent) && (int)O(Op::EventButton, inEvent) == 0 &&
             (int)O(Op::EventX, inEvent) >= (int)O(Op::ColumnWidth, sO->mList, 0) && CanJoin(aGame))
        O(Op::DeliverJoinGame, aGame);
}
// mServerDetailsMap[aGame]=anItem
static void MapPut(intptr_t inGame, intptr_t inItem)
{
    std::map<intptr_t, intptr_t>::iterator it = sO->mServerDetailsMap.find(inGame);
    if (it == sO->mServerDetailsMap.end()) {
        O(Op::AddRef, inGame);
        it = sO->mServerDetailsMap.insert(std::make_pair(inGame, (intptr_t)0)).first;
    }
    Assign(it->second, inItem);
}
static void MapErase(std::map<intptr_t, intptr_t>::iterator it)
{
    intptr_t aGame = it->first;
    ReleasePtr(it->second);
    O(Op::Release, aGame);
    sO->mServerDetailsMap.erase(it);
}
static void MapClear()
{
    while (!sO->mServerDetailsMap.empty()) MapErase(sO->mServerDetailsMap.begin());
}
static void HandleGameChanged(intptr_t inEvent)
{
    intptr_t aGame = O(Op::EventGame, inEvent);
    if ((int)Get(aGame, MF::GameType) != sO->mGameType) return;
    if (O(Op::EventIsAdd, inEvent)) {
        intptr_t anItem = O(Op::RowNew);
        SetGame(anItem, aGame);
        MapPut(aGame, anItem);
        bool aFits = FitsFilter(anItem);
        O(Op::MSet, aGame, (intptr_t)MF::GameFiltered, !aFits);
        if (aFits) O(Op::ListInsert, sO->mList, anItem);
        O(Op::Release, anItem);
    } else if (O(Op::EventIsDelete, inEvent)) {
        std::map<intptr_t, intptr_t>::iterator it = sO->mServerDetailsMap.find(aGame);
        if (it != sO->mServerDetailsMap.end()) {
            O(Op::ListRemove, sO->mList, it->second);
            MapErase(it);
        }
    } else {
        std::map<intptr_t, intptr_t>::iterator it = sO->mServerDetailsMap.find(aGame);
        if (it == sO->mServerDetailsMap.end()) return;
        intptr_t anItem = it->second;
        bool aVisible = (int)O(Op::ListItemPos, sO->mList, anItem) >= 0;
        bool aFits = FitsFilter(anItem);
        O(Op::MSet, Game(anItem), (intptr_t)MF::GameFiltered, !aFits);
        if (aVisible) {
            if (aFits) O(Op::ListRefresh, sO->mList, anItem);
            else O(Op::ListRemove, sO->mList, anItem);
        } else if (aFits) O(Op::ListInsert, sO->mList, anItem);
    }
    UpdateJoinButton();
    UpdateSummaryLabel();
}
static void HandleSyncGameList(intptr_t inEvent)
{
    intptr_t aGames = O(Op::EventGames, inEvent);
    if ((int)Get(aGames, MF::GameListType) != sO->mGameType) return;
    O(Op::MCall, aGames, (intptr_t)MC::GameListRewind, 0);
    MapClear();
    while (O(Op::MCall, aGames, (intptr_t)MC::GameListHasMore, 0)) {
        intptr_t aGame = O(Op::MCall, aGames, (intptr_t)MC::GameListNext, 0);
        intptr_t anItem = O(Op::RowNew);
        SetGame(anItem, aGame);
        MapPut(aGame, anItem);
        O(Op::Release, anItem);
    }
    AddFilteredEntries_All(false);
}
static void LoadFilterCombos()
{
    for (size_t i = 0; i < sO->mColumnInfo.size(); i++) {
        ColumnInfo aColumn = sO->mColumnInfo[i];
        intptr_t aCombo = aColumn.mFilterCombo;
        if (aCombo == 0) continue;
        O(Op::ComboClear, aCombo, 1);
        O(Op::ComboInsertNoFilter, aCombo, sO->mBoldFont);
        intptr_t aName = O(Op::TextNew, (intptr_t)aColumn.mColumnName.c_str(), (intptr_t)aColumn.mColumnName.size());
        O(Op::ComboInsertFilterRow, aCombo, aName, sO->mBoldFont);
        O(Op::ComboAddFilterValues, aCombo, aName);
        O(Op::Release, aName);
    }
}
static const int gServerFilterItemPos = 1;
static void HandleComboTextChanged(intptr_t inComponent)
{
    if (!O(Op::IsCombo, inComponent)) return;
    intptr_t aCombo = inComponent;
    sO->mNeedFilter = true;
    sO->mFilterTimeAccum = 0;
    O(Op::RequestTimer, 1);
    intptr_t aText = O(Op::ComboEditText, aCombo);
    intptr_t aFilter = O(Op::ComboItem, aCombo, gServerFilterItemPos);
    if (aFilter == 0) return;
    if (O(Op::TextLen, aText) == 0) {
        O(Op::FilterClear, aFilter);
        O(Op::ComboUpdateItem, aCombo, aFilter);
        return;
    }
    int aCount = (int)O(Op::ComboCount, aCombo);
    int aPos;
    for (aPos = 0; aPos < aCount; ++aPos) {
        if (O(Op::TextItemIs, O(Op::ComboItem, aCombo, aPos), aText)) {
            if (aPos <= gServerFilterItemPos) return;
            break;
        }
    }
    bool aIsAdd = aPos >= aCount;
    O(Op::FilterSet, aFilter, (intptr_t)(uint32_t)aPos | ((intptr_t)aIsAdd << 32), aText);
    O(Op::ComboUpdateItem, aCombo, aFilter);
}
static void HandleComboSelChanged(intptr_t inEvent, intptr_t inComponent)
{
    if (!O(Op::IsCombo, inComponent)) return;
    intptr_t aCombo = inComponent;
    int aPos = (int)O(Op::ComboItemPos, aCombo, O(Op::EventItem, inEvent));
    if (aPos > gServerFilterItemPos) return;
    if (aPos == 0 && O(Op::EventActiveSel, inEvent)) {
        O(Op::ComboSetText, aCombo, 0);
        return;
    }
    intptr_t aFilter = O(Op::ComboItem, aCombo, gServerFilterItemPos);
    if (O(Op::EventActiveSel, inEvent)) {
        if (O(Op::FilterIsAdd, aFilter)) {
            if (O(Op::TextLen, O(Op::FilterText, aFilter)) != 0) {
                intptr_t anItem = O(Op::ComboAddString, aCombo, O(Op::FilterText, aFilter));
                O(Op::ComboSetSel, aCombo, O(Op::ComboItemPos, aCombo, anItem), 1);
                intptr_t aColumn = O(Op::FilterColumn, aFilter);
                O(Op::AddFilterValue, aColumn, O(Op::FilterText, aFilter));
            } else O(Op::ComboSetText, aCombo, 0);
        } else if (O(Op::FilterSelPos, aFilter) > gServerFilterItemPos) {
            O(Op::ComboDeleteItem, aCombo, O(Op::FilterSelPos, aFilter));
            O(Op::ComboSetText, aCombo, 0);
            intptr_t aColumn = O(Op::FilterColumn, aFilter);
            O(Op::RemoveFilterValue, aColumn, O(Op::FilterText, aFilter));
        }
    } else {
        O(Op::ComboSetText, aCombo, O(Op::FilterText, aFilter));
        O(Op::ComboSelectEnd, aCombo);
    }
}
static void HandleComponentEvent(intptr_t inEvent, Ev inType, intptr_t inComponent)
{
    switch (inType) {
    case Ev::ButtonPressed:
        if (inComponent == sO->mJoinButton) {
            intptr_t aGame = GetSelGame();
            if (aGame != 0) O(Op::DeliverJoinGame, aGame);
            return;
        }
        if (inComponent == sO->mRefreshButton && HandleRefreshButton()) return;
        break;
    case Ev::ListSelChanged: UpdateJoinButton(); break;
    case Ev::InputTextChanged: HandleComboTextChanged(inComponent); return;
    case Ev::ComboSelChanged: HandleComboSelChanged(inEvent, inComponent); return;
    case Ev::ListItemClicked: HandleListItemClicked(inEvent); return;
    case Ev::GameChanged: HandleGameChanged(inEvent); return;
    case Ev::SyncGameList: HandleSyncGameList(inEvent); return;
    case Ev::SyncOptions: case Ev::FiltersChanged: LoadFilterCombos(); return;
    case Ev::RefilterAll: AddFilteredEntries_All(true); return;
    case Ev::RefilterShown: AddFilteredEntries_List(true); return;
    default: break;
    }
    O(Op::BaseEvent, inEvent);
}
static void SafeGet(intptr_t inConfig, intptr_t& ioMember, int inSlot) { Assign(ioMember, O(Op::Find, inConfig, inSlot)); }
static void InitComponent(intptr_t inConfig)
{
    SafeGet(inConfig, sO->mList, 103);
    Assign(sO->mBoldFont, O(Op::DefaultFontMod, sO->mList, 1, 0));
    SafeGet(inConfig, sO->mJoinButton, 104);
    SafeGet(inConfig, sO->mRefreshButton, 105);
    SafeGet(inConfig, sO->mSummaryLabel, 106);
    Assign(sO->mRemoveButton, O(Op::FindOptional, inConfig, 107));
    UpdateJoinButton();
    UpdateSummaryLabel();
    O(Op::SubscribeBroadcast, 1);
}
// ServerListCtrlConfig's steps on the control (the reads and the list's own calls stay in the reader)
static intptr_t Config(CF inCall, intptr_t a, intptr_t b)
{
    switch (inCall) {
    case CF::SetGameType: sO->mGameType = (int)a; return 0;
    case CF::SetNumColumns: {
        sO->mNumColumns = (int)a;
        size_t n = a < 0 ? 0 : (size_t)a;
        while (sO->mColumnInfo.size() > n) {
            if (sO->mColumnInfo.back().mFilterCombo) O(Op::Release, sO->mColumnInfo.back().mFilterCombo);
            sO->mColumnInfo.pop_back();
        }
        sO->mColumnInfo.resize(n);
        return 0;
    }
    case CF::ColumnCount: return (intptr_t)sO->mColumnInfo.size();
    case CF::SetList: Assign(sO->mList, a); return 0;
    case CF::List: return sO->mList;
    case CF::GameType: return sO->mGameType;
    case CF::AddColumn: {
        std::map<std::string, ColumnInfo, NoCaseLess>::iterator it = sColumnMap.find(std::string((const char*)a, (size_t)b));
        if (it == sColumnMap.end()) return -1;
        unsigned char aFlags = it->second.mGameTypeFlags;
        switch (sO->mGameType) {
        case 3: if (aFlags & 4) break; return 0;
        case 2: if (aFlags & 1) break;
        default: return 0;
        case 1: if (aFlags & 2) break; return 0;
        }
        static const char* const kOrder[] = {"Icon", "Skill", "GameName", "Address", "NumPlayers", "Ping", "Epoch", "MapType"};
        for (int i = 0; i < 8; i++) if (NoCaseLess()(kOrder[i], it->first) == NoCaseLess()(it->first, kOrder[i])) return 1 + i;
        return 0;
    }
    case CF::PushColumn: {
        static const char* const kOrder[] = {"Icon", "Skill", "GameName", "Address", "NumPlayers", "Ping", "Epoch", "MapType"};
        sO->mColumnInfo.push_back(sColumnMap[kOrder[a]]);
        if (b) O(Op::AddRef, b);
        sO->mColumnInfo.back().mFilterCombo = b;
        return 0;
    }
    case CF::SelGame: return GetSelGame();
    }
    return 0;
}

static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inCall) {
    case Call::Init: InitComponent(a); return 0;
    case Call::Event: HandleComponentEvent(a, (Ev)b, c); return 0;
    case Call::Timer: return TimerEvent((int)a);
    case Call::Row: {
        const intptr_t* args = (const intptr_t*)c;
        switch ((RC)a) {
        case RC::Paint: Paint(b, args[0], args[1]); return 0;
        case RC::CalcDimensions: CalcDimensions(b, args[0]); return 0;
        case RC::Compare: return Compare(b, args[0], args[1], args[2]);
        case RC::CanSelect: return CanSelect(b, args[0], (int)(int32_t)(args[1] & 0xffffffff), (int)(args[1] >> 32));
        case RC::MouseDown: MouseDown(b, args[0], (int)(int32_t)(args[1] & 0xffffffff), (int)(args[1] >> 32), (int)args[2]); return 0;
        case RC::SetGame: SetGame(b, args[0]); return 0;
        }
        return 0;
    }
    case Call::Cfg: return Config((CF)a, b, c);
    case Call::Field:
        switch (a) {
        case 0: return sO->mBoldFont;
        case 1: return sO->mList;
        case 2: return sO->mJoinButton;
        case 3: return sO->mRefreshButton;
        case 4: return sO->mRemoveButton;
        case 5: return sO->mSummaryLabel;
        case 110: return sO->mGameType;
        case 111: return sO->mNeedFilter;
        case 112: return sO->mFilterTimeAccum;
        case 113: return sO->mNumColumns;
        case 114: return (intptr_t)sO->mServerDetailsMap.size();
        case 115: return (intptr_t)sO->mColumnInfo.size();
        }
        return 0;
    default: return 0;
    }
}

static void OrigFree()
{
    // the members destroyed in reverse order: the rows (std::map's order: unobservable), the controls, the columns
    MapClear();
    ReleasePtr(sO->mSummaryLabel);
    ReleasePtr(sO->mRemoveButton);
    ReleasePtr(sO->mRefreshButton);
    ReleasePtr(sO->mJoinButton);
    ReleasePtr(sO->mList);
    ReleasePtr(sO->mBoldFont);
    for (size_t i = 0; i < sO->mColumnInfo.size(); i++)
        if (sO->mColumnInfo[i].mFilterCombo) O(Op::Release, sO->mColumnInfo[i].mFilterCombo);
}

// ---- the sequences

struct Step { Call mCall; intptr_t a, b, c; intptr_t args[3]; };

static std::vector<std::string> RunSide(bool inRust, unsigned inSeed, const std::vector<Step>& inSteps)
{
    sTrace.clear();
    sSeed = inSeed;
    sRust = inRust;
    sDepth = 0;
    sNextRow = 0x9000;
    sNextText = 0xA000;
    if (inRust) sRustCtl = ee_won_lobbyctl_new((void*)0x9090, EeWonLobbyCtlKind::ServerList, RustHost);
    else {
        sO = new Orig();
        sO->mGameType = 0;
        sO->mNumColumns = 4;
        sO->mFilterTimeAccum = 0;
        sO->mNeedFilter = false;
        sO->mBoldFont = sO->mList = sO->mJoinButton = sO->mRefreshButton = sO->mRemoveButton = sO->mSummaryLabel = 0;
    }
    for (size_t i = 0; i < inSteps.size(); i++) {
        const Step& s = inSteps[i];
        if (s.mCall == Call::Row)
            Line("call %lld %lld %lld %lld %lld", (long long)s.mCall, s.a, s.b, s.args[0], s.args[1]),
                sTrace.back() += " " + std::to_string((long long)s.args[2]);
        else if (s.mCall == Call::Cfg && (CF)s.a == CF::AddColumn)
            sTrace.push_back("call " + std::to_string((long long)s.mCall) + " " + std::to_string((long long)s.a) + " '" +
                             std::string((const char*)s.b, (size_t)s.c) + "'");
        else Line("call %lld %lld %lld %lld", (long long)s.mCall, s.a, s.b, s.c);
        intptr_t c = s.mCall == Call::Row ? (intptr_t)s.args : s.c;
        intptr_t r = inRust ? ee_won_lobbyctl_call(sRustCtl, s.mCall, s.a, s.b, c) : OrigCall(s.mCall, s.a, s.b, c);
        Line("ret %lld", r);
    }
    Line("free");
    if (inRust) ee_won_lobbyctl_free(sRustCtl);
    else { OrigFree(); delete sO; sO = NULL; }
    return sTrace;
}

static const char* const kColumnNames[] = {"Icon", "skill", "GAMENAME", "Address", "NumPlayers", "Ping", "Epoch",
                                           "maptype", "Nope", "Pin"};

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    // the column definitions on both sides
    InitColumnMap();
    ee_won_lobbyctl_init_columns();
    ee_won_lobbyctl_ee_column(0);
    ee_won_lobbyctl_ee_column(1);
    // the look-ups against the original SafeGetComponent arguments
    {
        const char* kNames[] = {"ServerList", "JoinGameButton", "RefreshListButton", "ServerListSummary", "RemoveButton"};
        const int kLines[] = {647, 650, 651, 652, 653};
        const EeWonLobbyCtlType kTypes[] = {EeWonLobbyCtlType::MultiList, EeWonLobbyCtlType::Button,
                                            EeWonLobbyCtlType::Button, EeWonLobbyCtlType::Label,
                                            EeWonLobbyCtlType::Component};
        for (int i = 0; i < 5; i++) {
            const EeWonLobbyCtlSlot* s = ee_won_lobbyctl_slot(103 + i);
            CHECK(s != NULL && strcmp(s->name, kNames[i]) == 0 && strcmp(s->file, "ServerListCtrl.cpp") == 0);
            CHECK(s != NULL && s->line == kLines[i] && s->ty == kTypes[i] && s->control_id == -1);
        }
    }
    unsigned long long aLines = 0;
    int aDiffer = 0;
    unsigned aStepSeed = 1213;
    int kSequences = 60000;
    if (argc > 2) kSequences = atoi(argv[2]);
    for (int n = 0; n < kSequences; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        std::vector<Step> aSteps;
        // the config reader: a game type, maybe a column count, the columns
        Step s0 = {Call::Cfg, (intptr_t)CF::SetGameType, (intptr_t)R(4), 0, {0, 0, 0}};
        aSteps.push_back(s0);
        if (R(4) == 0) { Step s = {Call::Cfg, (intptr_t)CF::SetNumColumns, (intptr_t)R(4), 0, {0, 0, 0}}; aSteps.push_back(s); }
        if (R(3)) { Step s = {Call::Cfg, (intptr_t)CF::SetList, kListArea, 0, {0, 0, 0}}; aSteps.push_back(s); }
        int aColumns = (int)R(8);
        for (int i = 0; i < aColumns; i++) {
            const char* aName = kColumnNames[R(10)];
            Step s = {Call::Cfg, (intptr_t)CF::AddColumn, (intptr_t)aName, (intptr_t)strlen(aName), {0, 0, 0}};
            aSteps.push_back(s);
            // the reader pushes a column AddColumn found for this game type (its result 1 + the definition)
            static const char* const kOrder[] = {"icon", "skill", "gamename", "address", "numplayers", "ping", "epoch", "maptype"};
            int aDef = -1;
            for (int k = 0; k < 8; k++) {
                std::string x(aName);
                for (size_t q = 0; q < x.size(); q++) x[q] = (char)tolower((unsigned char)x[q]);
                if (x == kOrder[k]) aDef = k;
            }
            if (aDef >= 0) {
                Step p = {Call::Cfg, (intptr_t)CF::PushColumn, aDef, R(3) ? kCombo + (intptr_t)i * 0x10 : 0, {0, 0, 0}};
                aSteps.push_back(p);
            }
        }
        if (R(6)) { Step s = {Call::Init, 0xC0, 0, 0, {0, 0, 0}}; aSteps.push_back(s); }
        int aCount = 1 + (int)R(14);
        for (int i = 0; i < aCount; i++) {
            Step s = {Call::Event, kEvent, 0, 0, {0, 0, 0}};
            static const Ev kEvents[] = {Ev::ButtonPressed, Ev::ListSelChanged, Ev::InputTextChanged, Ev::ComboSelChanged,
                                         Ev::ListItemClicked, Ev::GameChanged, Ev::GameChanged, Ev::GameChanged,
                                         Ev::SyncGameList, Ev::SyncOptions, Ev::FiltersChanged, Ev::RefilterAll,
                                         Ev::RefilterShown, Ev::Other, Ev::InputReturn};
            s.b = (intptr_t)kEvents[R(sizeof(kEvents) / sizeof(kEvents[0]))];
            switch (R(12)) {
            case 0: s.mCall = Call::Init; s.a = 0xC0; s.b = 0; break;
            case 1: s.mCall = Call::Timer; s.a = (intptr_t)R(700); s.b = 0; break;
            case 2: s.mCall = Call::Field; s.a = R(2) ? (intptr_t)R(6) : 110 + (intptr_t)R(6); s.b = 0; break;
            case 3: case 4: case 5: {
                s.mCall = Call::Row;
                s.a = (intptr_t)R(6);
                s.b = R(3) ? 0x9000 + (intptr_t)R(4) : kItem + (intptr_t)R(3);
                RC aCall = (RC)s.a;
                if (aCall == RC::Paint) { s.args[0] = kGfx; s.args[1] = kListArea; }
                else if (aCall == RC::CalcDimensions) s.args[0] = kListArea;
                else if (aCall == RC::Compare) { s.args[0] = R(5) ? 0x9000 + (intptr_t)R(4) : s.b; s.args[1] = kListArea; s.args[2] = (intptr_t)R(9) - 4; }
                else if (aCall == RC::SetGame) s.args[0] = kGame + (intptr_t)R(5) * 0x10;
                else { s.args[0] = kListArea; s.args[1] = Pt((int)R(70) - 5, (int)R(50) - 5); s.args[2] = R(3); }
                break;
            }
            case 6: s.mCall = Call::Cfg; s.a = (intptr_t)CF::SelGame; s.b = 0; break;
            default: break;
            }
            if (s.mCall == Call::Event) {
                unsigned w = R(6);
                if (w == 0) s.c = 0;
                else if (w == 1) s.c = kOther;
                else if (w == 2) s.c = kRemove;
                else if (w == 3) s.c = kCombo + (intptr_t)R(4) * 0x10;
                else s.c = kCtl + (103 + (intptr_t)R(5)) * 0x10;
            }
            aSteps.push_back(s);
        }
        aStepSeed = sSeed;
        sSeed = aSave;
        unsigned aSeed = 77u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunSide(false, aSeed, aSteps);
        std::vector<std::string> b = RunSide(true, aSeed, aSteps);
        aLines += a.size();
        sChecks++;
        if (a != b) {
            if (++aDiffer <= 3) {
                printf("sequence %d differs\n", n);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    const char* x = i < a.size() ? a[i].c_str() : "-";
                    const char* y = i < b.size() ? b[i].c_str() : "-";
                    if (strcmp(x, y) != 0) {
                        printf("  line %zu: C++ '%s' Rust '%s'\n", i, x, y);
                        for (size_t k = i > 6 ? i - 6 : 0; k < i; k++) printf("    %s\n", a[k].c_str());
                        break;
                    }
                }
            }
            continue;
        }
        bool aNew = n < 60;
        for (size_t i = 0; i < a.size(); i++) {
            // an op by its number (the model's, a row field's, a column's: and which), a call by its number and
            // (an event's, a row's, a config's) which
            long long t[5] = {0, 0, 0, 0, 0};
            char aWord[16] = "";
            int n = sscanf(a[i].c_str(), "%15s %lld %lld %lld %lld", aWord, &t[0], &t[1], &t[2], &t[3]);
            std::string aKey = aWord;
            if (n >= 2 && strcmp(aWord, "ret") != 0) aKey += " " + std::to_string(t[0]);
            if (strcmp(aWord, "op") == 0 && n >= 4 &&
                (t[0] == (long long)Op::MGet || t[0] == (long long)Op::MSet || t[0] == (long long)Op::MCall ||
                 t[0] == (long long)Op::ItemGet || t[0] == (long long)Op::ItemSet || t[0] == (long long)Op::EeColumn))
                aKey += " " + std::to_string(t[0] == (long long)Op::EeColumn ? t[1] : t[2]);
            if (strcmp(aWord, "call") == 0 && n >= 4 && t[0] == (long long)Call::Event) aKey += " " + std::to_string(t[2]);
            if (strcmp(aWord, "call") == 0 && n >= 4 && (t[0] == (long long)Call::Row || t[0] == (long long)Call::Cfg))
                aKey += " " + std::to_string(t[1]);
            if (sSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && aNew) {
            printf("seq %d\n", n);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (aDiffer) { sFailures++; printf("FAIL: %d of %d sequences differ\n", aDiffer, kSequences); }
    if (!aPrint)
        printf("won_srvlist_test: %d sequences, %llu trace lines, %zu distinct ops and calls; %d checks, %d failures\n",
               kSequences, aLines, sSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
