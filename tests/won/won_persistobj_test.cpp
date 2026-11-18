// The lobby's saved settings as one Rust object (R6 WONLobby round 15): LobbyPersistentData and the Lobby* functions
// over it as they were (their statements copied before the move: won_persistobj_orig.h, namespace Orig, over the
// round's start containers, the real FileReader / FileWriter / WONFile, Blowfish, buffers and StringParser) against the
// game's inline forwards (WONLobbyPersistence.h) over ee_won_lobby's persistobj.rs, both in one stand-in world:
// friends are counted records, the resource strings fixed texts, the clock a counter, the machine id a constant, the
// click sound's mute and the config's LAN-number switch traced. Each side runs in its own directory (its files).
//
// Random sequences of every function (users, the 11 flags, the numbers, the texts, friends, ignored names, LAN names,
// direct connects, reconnect ids, filters and game options, the current user, saves, loads, restore, no lobby data,
// files cut, given another version or deleted), and after each step the whole observable state of both sides: every
// getter, every user's record, every friend with its references, the lists, the filters of the columns used, the
// reconnect ids of the addresses used, the traces and the bytes of every file. Exit code: the number of mismatches.
// `--print` prints the port's states over the first 40 sequences.
#include <windows.h>
#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONBlowfish.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "rust/won_lobby/persist.h"
#include "rust/won_lobby/lobbymap.h"
#include "rust/won_lobby/resource.h"
wchar_t gEmptyWideString[2]; // EEWonNet.cpp's (GetEmptyGUIStringData)

using namespace WONAPI;

// ---- the world both sides share
static std::string sTrace; // the click sound's mutes, the friends made and freed
static long sAppend = 1;   // the config's mAppendNumberToDefaultLanName (2: no config)
namespace Orig {
using namespace WONAPI;
// a friend: its name, id and references
class LobbyFriend : public RefCount {
public:
    LobbyFriend(const GUIString& inName, unsigned long inId) : mName(inName), mId(inId) { sLive++; sTrace += "f+" + std::to_string(inId) + " "; }
    ~LobbyFriend() { sLive--; sTrace += "f-" + std::to_string(mId) + " "; }
    GUIString GetName() const { return this->mName; }
    GUIString mName;
    unsigned long mId;
    static int sLive;
};
int LobbyFriend::sLive;
}
// each side's clock (time(NULL)): one tick per read
static long sOrigNow, sNewNow;
namespace Orig {
static time_t time(time_t*) { return (time_t)(1000000 + ++sOrigNow); }
struct CDKey { static DWORD GetMachineId(bool*) { return 0x01020304; } };
struct StandSound { void SetMuted(bool inMuted) { sTrace += inMuted ? "mute " : "unmute "; } };
static StandSound sClick;
struct Res { GUIString mString; StandSound* mSound; };
static Res gLobbyResources[2048];
static GUIString gEmptyGUIString;
struct LobbyConfig { bool mAppendNumberToDefaultLanName; };
static LobbyConfig sConfig;
static LobbyConfig* GetLobbyConfig() { sConfig.mAppendNumberToDefaultLanName = sAppend == 1; return sAppend == 2 ? NULL : &sConfig; }
class LobbyPersistentData;
struct Prv : RefCount { SmartPtr<LobbyPersistentData> mPersistentData; };
static SmartPtr<Prv> gLobbyData;
#include "won_persistobj_orig.h"
}

// ---- the new side: the game's forwards over the driver's object
static EeWonPersist* sNew;
EeWonPersist* WONAPI::LobbyPersist() { return sNew; }
static intptr_t NewHost(void*, EeWonPersistOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inOp) {
    case EeWonPersistOp::FriendNew: {
        Orig::LobbyFriend* f = new Orig::LobbyFriend(LobbyPersistString(reinterpret_cast<const uint32_t*>(a), (size_t)b), (unsigned long)c);
        f->AddRef();
        return (intptr_t)f;
    }
    case EeWonPersistOp::FriendRef:
        if (b) reinterpret_cast<Orig::LobbyFriend*>(a)->AddRef();
        else reinterpret_cast<Orig::LobbyFriend*>(a)->Release();
        return 0;
    case EeWonPersistOp::FriendName: {
        Orig::LobbyFriend* f = reinterpret_cast<Orig::LobbyFriend*>(a);
        *reinterpret_cast<size_t*>(b) = f->mName.length();
        return (intptr_t)f->mName.mData->mText;
    }
    case EeWonPersistOp::ResText: {
        const GUIString& t = Orig::gLobbyResources[a].mString;
        *reinterpret_cast<size_t*>(b) = t.length();
        return (intptr_t)t.mData->mText;
    }
    case EeWonPersistOp::MuteClick: Orig::sClick.SetMuted(a != 0); return 0;
    case EeWonPersistOp::AppendLanNumber: return sAppend != 0;
    case EeWonPersistOp::Now: return (intptr_t)(1000000 + ++sNewNow);
    }
    return 0;
}
static void NewMake()
{
    int32_t aKey[2] = {0x22fa655f, 0x01020304};
    EeWonPersistHost aHost = {NULL, NewHost};
    sNew = ee_won_persist_new(aHost, sizeof(wchar_t), reinterpret_cast<const uint8_t*>(aKey), sizeof(aKey));
}

// ---- running each side in its own directory
static std::string sRoot;
static void Side(bool inOrig) { _chdir((sRoot + (inOrig ? "/orig" : "/new")).c_str()); }

static unsigned long long sSeed = 0x2545f4914f6cdd1dull;
static unsigned Rand(unsigned n) { sSeed = sSeed * 6364136223846793005ull + 1442695040888963407ull; return (unsigned)((sSeed >> 33) % n); }

static const wchar_t* kNames[] = {L"Ann", L"ann", L"ANN", L"Bob", L"bob\x00e9", L"Bob\x00c9", L"Cy", L"", L"Zo\x263a", L"x\xd83d\xde00",
                                  L"Quick Play", L"Player", L"LAN", L"a b", L"Q"};
static const wchar_t* kValues[] = {L"Stone", L"stone", L"Iron", L"Atomic", L"Space", L"\"Iron Age\"", L"", L"x"};
static const wchar_t* kLists[] = {L"Stone, Iron", L" Atomic ,Space", L"\"a, b\", c", L"", L",x", L"Stone", L"a,,b", L"q, \"r"};
static GUIString Name() { return GUIString(kNames[Rand(sizeof kNames / sizeof *kNames)]); }
static GUIString ManyName() { wchar_t b[16]; swprintf(b, 16, L"u%u", Rand(90)); return GUIString(b); }
static IPAddr Addr()
{
    unsigned char six[6] = {0x1f, (unsigned char)(0x90 + Rand(2)), 10, 0, 0, (unsigned char)(1 + Rand(4))};
    IPAddr a;
    a.SetSixByte(six);
    return a;
}
static std::string Utf8(const GUIString& s) { return (std::string)s; }
static std::string Hex(const std::string& b)
{
    std::string s; char x[4];
    for (unsigned char c : b) { snprintf(x, sizeof x, "%02x", c); s += x; }
    return s;
}

// the whole observable state of one side
template <bool O> struct State;
static std::string Files()
{
    std::string s;
    std::vector<std::string> names;
    for (auto& e : std::filesystem::directory_iterator(".")) names.push_back(e.path().filename().string());
    std::sort(names.begin(), names.end());
    for (auto& n : names) {
        std::ifstream in(n, std::ios::binary);
        std::stringstream b; b << in.rdbuf();
        s += " " + n + "=" + Hex(b.str());
    }
    return s;
}
#define DUMP_BODY(NS)                                                                                                        \
    std::string s;                                                                                                           \
    char b[256];                                                                                                             \
    snprintf(b, sizeof b, "speed %u port %u adapter %ld tab '%s' lan '%s' server '%s' room '%s'\n", NS::LobbyGetConnectionSpeed(), \
             NS::LobbyGetLanBroadcastPort(), NS::LobbyGetNetworkAdapter(), Utf8(NS::LobbyGetTabSelect()).c_str(),           \
             Utf8(NS::LobbyGetLanName()).c_str(), Utf8(NS::LobbyGetLastServerName()).c_str(), Utf8(NS::LobbyGetLastRoomName()).c_str()); \
    s += b;                                                                                                                  \
    s += "flags";                                                                                                            \
    s += NS::LobbyGetFilterDirtyWords() ? " 1" : " 0"; s += NS::LobbyGetDoAutoAFK() ? "1" : "0";                             \
    s += NS::LobbyGetCrossPromotion() ? "1" : "0"; s += NS::LobbyGetLobbySoundEffects() ? "1" : "0";                         \
    s += NS::LobbyGetChatSoundEffects() ? "1" : "0"; s += NS::LobbyGetStagingSoundEffects() ? "1" : "0";                     \
    s += NS::LobbyGetLobbyMusic() ? "1" : "0"; s += NS::LobbyGetShowConfirmations() ? "1" : "0";                             \
    s += NS::LobbyGetAutoPingGames() ? "1" : "0"; s += NS::LobbyGetShowGamesInProgress() ? "1" : "0";                        \
    s += NS::LobbyGetAnonymousToFriends() ? "1" : "0";                                                                       \
    s += "\nusers";                                                                                                          \
    for (size_t i = 0; i < NS::LobbyGetUserCount(); i++) {                                                                   \
        snprintf(b, sizeof b, " '%s'@%ld", Utf8(NS::LobbyGetUserNameAt(i)).c_str(), NS::LobbyGetUserLastUseAt(i));          \
        s += b + UserOf<O>(NS::LobbyFindUser(NS::LobbyGetUserNameAt(i)));                                                    \
    }                                                                                                                        \
    s += "\ncurrent" + UserOf<O>(NS::LobbyGetCurrentUser());                                                                 \
    s += "\nfriends";                                                                                                        \
    for (size_t i = 0; i < NS::LobbyGetFriendCount(); i++) {                                                                 \
        Orig::LobbyFriend* f = (Orig::LobbyFriend*)NS::LobbyGetFriendAt(i);                                                  \
        snprintf(b, sizeof b, " %lu:'%s'/%d", f->mId, Utf8(f->mName).c_str(), (int)f->GetRefCount());                       \
        s += b;                                                                                                              \
    }                                                                                                                        \
    s += "\nignored";                                                                                                        \
    for (size_t i = 0; i < NS::LobbyGetIgnoredNameCount(); i++) s += " '" + Utf8(NS::LobbyGetIgnoredNameAt(i)) + "'";       \
    s += "\nlan";                                                                                                            \
    for (size_t i = 0; i < NS::LobbyGetLanNameCount(); i++) s += " '" + Utf8(NS::LobbyGetLanNameAt(i)) + "'";               \
    s += "\ndirect";                                                                                                         \
    for (size_t i = 0; i < NS::LobbyGetDirectConnectCount(); i++)                                                            \
        s += " " + Hex(std::string(NS::LobbyGetDirectConnectAddressAt(i).GetSixByte(), 6)) + "='" + Utf8(NS::LobbyGetDirectConnectNameAt(i)) + "'"; \
    s += "\nreconnect";                                                                                                      \
    for (unsigned p = 0; p < 2; p++)                                                                                         \
        for (unsigned h = 1; h <= 4; h++) {                                                                                  \
            unsigned char six[6] = {0x1f, (unsigned char)(0x90 + p), 10, 0, 0, (unsigned char)h};                            \
            IPAddr a; a.SetSixByte(six);                                                                                     \
            s += " " + std::to_string(NS::LobbyGetReconnectId(a));                                                           \
        }                                                                                                                    \
    s += "\nblocked";                                                                                                        \
    for (unsigned long k = 0; k < 6; k++) s += NS::LobbyIsCDKeyBlocked(k) ? " 1" : " 0";                                     \
    s += "\nfilters";                                                                                                        \
    for (const wchar_t* c : {L"Epoch", L"epoch", L"MapType", L"Map\x00e9", L"Empty", L"x"}) s += " " + Filter<O>(GUIString(c)); \
    return s;

template <bool O> static std::string UserOf(const void* u);
template <> std::string UserOf<true>(const void* v)
{
    const Orig::LobbyUserInfo* u = (const Orig::LobbyUserInfo*)v;
    if (u == NULL) return " -";
    char b[256];
    snprintf(b, sizeof b, " [%ld %ld %ld %ld '%s' '%s']", u->mUserId, u->mValue04, u->mValue08, u->mLastUseTime,
             Utf8(u->mUserName).c_str(), Utf8(u->mPassword).c_str());
    return b;
}
template <> std::string UserOf<false>(const void* v)
{
    const LobbyUserInfo* u = (const LobbyUserInfo*)v;
    if (u == NULL) return " -";
    char b[256];
    snprintf(b, sizeof b, " [%ld %ld %ld %ld '%s' '%s']", (long)ee_won_persist_user_number(u, 0), LobbyUserTOU(u, 0), LobbyUserTOU(u, 1),
             (long)ee_won_persist_user_number(u, 3), Utf8(LobbyUserText(u, 0)).c_str(), Utf8(LobbyUserText(u, 1)).c_str());
    return b;
}
template <bool O> static std::string Filter(const GUIString& c);
template <> std::string Filter<true>(const GUIString& c)
{
    Orig::LobbyOptionSet v = Orig::LobbyGetFilterValues(c);
    std::string s = std::to_string(v.size());
    for (size_t i = 0; i < v.size(); i++) s += ",'" + Utf8(v.ValueAt(i)) + "'";
    return s;
}
template <> std::string Filter<false>(const GUIString& c)
{
    std::string s = std::to_string(LobbyGetFilterCount(c));
    for (size_t i = 0; i < LobbyGetFilterCount(c); i++) s += ",'" + Utf8(LobbyGetFilterValueAt(c, i)) + "'";
    return s;
}
template <bool O> std::string Dump();
template <> std::string Dump<true>() { const bool O = true; DUMP_BODY(Orig) }
template <> std::string Dump<false>() { const bool O = false; DUMP_BODY(WONAPI) }
// the flags only the class could read
static std::string OrigFlags()
{
    Orig::LobbyPersistentData* d = Orig::GetLobbyPersistentData();
    std::string s = " internal";
    if (d == NULL) return s + " none";
    for (int k = 0; k <= 14; k++) s += d->GetFlag((Orig::LobbyPersistentData::Flag)k) ? "1" : "0";
    s += " user " + std::to_string(d->GetCurrentUserId());
    return s;
}
static std::string NewFlags()
{
    std::string s = " internal";
    if (sNew == NULL) return s + " none";
    for (int k = 0; k <= 14; k++) s += LobbyPC(LobbyPC_::Flag, k) ? "1" : "0";
    // the current user's id: the record's, or the LAN user's 0 (the original's GetCurrentUserId)
    return s;
}

static int sFailures, sChecks;
static bool sPrint;
static std::string sGold;
static void Compare(int inSeq, int inStep, const char* inWhat)
{
    sChecks++;
    std::string so, sn;
    std::string to = sTrace;
    Side(true);
    so = Dump<true>() + OrigFlags().substr(0, 24) + "\nfiles" + Files();
    Side(false);
    sn = Dump<false>() + NewFlags() + "\nfiles" + Files();
    if (sPrint && inSeq < 40) sGold += "seq " + std::to_string(inSeq) + " step " + std::to_string(inStep) + " " + inWhat + "\n" + sn + "\n";
    if (so != sn) {
        sFailures++;
        if (sFailures < 6) printf("MISMATCH seq %d step %d after %s\n--- original\n%s\n--- port\n%s\n", inSeq, inStep, inWhat, so.c_str(), sn.c_str());
    }
}

// one step on both sides: each in its directory, the trace compared
static void Step(int inSeq, int inStep)
{
    unsigned op = Rand(40);
    GUIString n1 = Rand(4) ? Name() : ManyName(), n2 = Name();
    unsigned long id = Rand(8);
    IPAddr addr = Addr();
    bool on = Rand(2) != 0;
    int value = (int)Rand(70000);
    GUIString col = Rand(3) ? GUIString(Rand(2) ? L"Epoch" : L"MapType") : GUIString(Rand(2) ? L"epoch" : L"Map\x00e9");
    GUIString val = kValues[Rand(sizeof kValues / sizeof *kValues)], list = kLists[Rand(sizeof kLists / sizeof *kLists)];
    int many = 1 + (int)Rand(Rand(4) ? 3 : 80);
    char what[64];
    snprintf(what, sizeof what, "op %u", op);
    std::string to, tn;
    unsigned long long aSeed = sSeed; // each side draws the same numbers
    for (int side = 0; side < 2; side++) {
        bool O = side == 0;
        sSeed = aSeed;
        Side(O);
        sTrace.clear();
#define BOTH(call) do { if (O) Orig::call; else WONAPI::call; } while (0)
        switch (op) {
        case 0: case 1:
            for (int i = 0; i < many; i++) {
                GUIString nm = many > 1 ? ManyName() : n1;
                if (O) Orig::LobbyAddUser(nm, n2, on && Rand(2)); else LobbyAddUser(nm, n2, on && Rand(2));
            }
            break;
        case 2:
            if (O) { Orig::LobbyUserInfo* u = on ? Orig::LobbyFindUser(n1) : NULL; Orig::LobbySetCurrentUser(u); }
            else { LobbyUserInfo* u = on ? LobbyFindUser(n1) : NULL; LobbySetCurrentUser(u); }
            break;
        case 3: {
            switch (Rand(11)) {
            case 0: BOTH(LobbySetFilterDirtyWords(on)); break;
            case 1: BOTH(LobbySetDoAutoAFK(on)); break;
            case 2: BOTH(LobbySetCrossPromotion(on)); break;
            case 3: BOTH(LobbySetLobbySoundEffects(on)); break;
            case 4: BOTH(LobbySetChatSoundEffects(on)); break;
            case 5: BOTH(LobbySetStagingSoundEffects(on)); break;
            case 6: BOTH(LobbySetLobbyMusic(on)); break;
            case 7: BOTH(LobbySetShowConfirmations(on)); break;
            case 8: BOTH(LobbySetAutoPingGames(on)); break;
            case 9: BOTH(LobbySetShowGamesInProgress(on)); break;
            default: BOTH(LobbySetAnonymousToFriends(on)); break;
            }
            break;
        }
        case 4: BOTH(LobbySetConnectionSpeed((unsigned short)value)); break;
        case 5: BOTH(LobbySetLanBroadcastPort((unsigned short)(8580 + Rand(10)))); break;
        case 6: BOTH(LobbySaveNetworkAdapter()); break;
        case 7: BOTH(LobbySaveGlobalPrefs(on)); break;
        case 8: BOTH(LobbySaveUserPrefs(on)); break;
        case 9: BOTH(LobbyLoadGlobalPrefs()); break;
        case 10: BOTH(LobbyRestoreDefaults()); break;
        case 11: BOTH(LobbySetTabSelect(n1)); break;
        case 12: BOTH(LobbySetLastRoom(n1, n2)); break;
        case 13: BOTH(LobbyAddFriend(n1, id)); break;
        case 14: BOTH(LobbyRemoveFriend(id)); break;
        case 15: BOTH(LobbySetNameIgnored(n1, on)); break;
        case 16: for (int i = 0; i < many; i++) BOTH(LobbySetLanName(many > 1 ? ManyName() : n1)); break;
        case 17: BOTH(LobbyRemoveLanName()); break;
        case 18: BOTH(LobbyAddDirectConnect(addr, n1)); break;
        case 19: BOTH(LobbyRemoveDirectConnect(addr)); break;
        case 20: BOTH(LobbySetReconnectId(addr, value - 35000)); break;
        case 21: BOTH(LobbyAddFilterValue(col, val)); break;
        case 22: BOTH(LobbyRemoveFilterValue(col, val)); break;
        case 23: BOTH(LobbyResetFilters()); break;
        case 24: BOTH(LobbyAddGameOption(col, list, on)); break;
        case 25: case 26: {
            // a file cut, given another version or deleted (the same on both sides)
            std::vector<std::string> names;
            for (auto& e : std::filesystem::directory_iterator(".")) names.push_back(e.path().filename().string());
            std::sort(names.begin(), names.end());
            if (names.empty()) break;
            std::string n = names[(size_t)(sSeed >> 40) % names.size()];
            std::ifstream in(n, std::ios::binary);
            std::stringstream b; b << in.rdbuf(); in.close();
            std::string bytes = b.str();
            unsigned how = (unsigned)(sSeed >> 20) % 4;
            if (how == 0) { std::filesystem::remove(n); break; }
            if (how == 1 && bytes.size() > 6) bytes[6] ^= 1;
            else bytes.resize(bytes.empty() ? 0 : (size_t)(sSeed >> 8) % bytes.size());
            std::ofstream out(n, std::ios::binary); out << bytes;
            break;
        }
        case 27: sAppend = Rand(3); break;
        case 28:
            // no lobby data for a while, then a new object (the constructor)
            if (O) {
                if (Orig::gLobbyData.get() != NULL) Orig::gLobbyData = NULL;
                else { Orig::gLobbyData = new Orig::Prv; Orig::gLobbyData->mPersistentData = new Orig::LobbyPersistentData; }
            } else {
                if (sNew != NULL) { ee_won_persist_free(sNew); sNew = NULL; }
                else NewMake();
            }
            break;
        case 29: BOTH(LobbySetLanName(GUIString(L""))); break;
        default: {
            // the queries (their answers are in the state)
            if (O) { Orig::LobbyIsNameIgnored(n1); Orig::LobbyFindFriend(id); }
            else { LobbyIsNameIgnored(n1); LobbyFindFriend(id); }
            break;
        }
        }
#undef BOTH
        (O ? to : tn) = sTrace;
    }
    sChecks++;
    if (to != tn) {
        sFailures++;
        if (sFailures < 6) printf("MISMATCH seq %d step %d trace after %s\n--- original\n%s\n--- port\n%s\n", inSeq, inStep, what, to.c_str(), tn.c_str());
    }
    Compare(inSeq, inStep, what);
}

int main(int argc, char** argv)
{
    sPrint = argc > 1 && !strcmp(argv[1], "--print");
    int seqs = sPrint ? 40 : (argc > 1 ? atoi(argv[1]) : 4000);
    char cwd[1024];
    _getcwd(cwd, sizeof cwd);
    sRoot = std::string(cwd) + "/persistobj_run";
    std::filesystem::remove_all(sRoot);
    std::filesystem::create_directories(sRoot + "/orig");
    std::filesystem::create_directories(sRoot + "/new");
    Orig::gLobbyResources[(int)EeWonLobbyRes::LobbyOptions_DefaultName_String].mString = GUIString(L"Player");
    Orig::gLobbyResources[(int)EeWonLobbyRes::LobbyGlobal_QuickPlay_String].mString = GUIString(L"Quick Play");
    Orig::gLobbyResources[(int)EeWonLobbyRes::LobbyGlobal_Lan_String].mString = GUIString(L"LAN");
    Orig::gLobbyResources[(int)EeWonLobbyRes::LobbyGlobal_ButtonClick_Sound].mSound = &Orig::sClick;
    for (int s = 0; s < seqs; s++) {
        // a fresh start: both directories empty, new objects
        _chdir(sRoot.c_str());
        for (const char* d : {"/orig", "/new"}) {
            std::filesystem::remove_all(sRoot + d);
            std::filesystem::create_directories(sRoot + d);
        }
        sAppend = 1;
        sTrace.clear();
        Side(true);
        Orig::gLobbyData = new Orig::Prv;
        Orig::gLobbyData->mPersistentData = new Orig::LobbyPersistentData;
        Side(false);
        if (sNew != NULL) ee_won_persist_free(sNew);
        NewMake();
        Compare(s, -1, "construction");
        int steps = 10 + (int)Rand(50);
        for (int i = 0; i < steps; i++) Step(s, i);
        Side(true);
        Orig::gLobbyData = NULL;
        Side(false);
        ee_won_persist_free(sNew);
        sNew = NULL;
        sChecks++;
        if (Orig::LobbyFriend::sLive != 0) {
            sFailures++;
            printf("seq %d: %d friends left\n", s, Orig::LobbyFriend::sLive);
            Orig::LobbyFriend::sLive = 0;
        }
    }
    _chdir(cwd);
    std::filesystem::remove_all(sRoot);
    if (sPrint) { fputs(sGold.c_str(), stdout); return 0; }
    printf("%d sequences, %d checks, %d mismatches\n", seqs, sChecks, sFailures);
    return sFailures;
}
