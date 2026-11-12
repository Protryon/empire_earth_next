// The lobby's data model (LobbyModel.cpp): its lists' decisions, the original's statements (copied from the C++
// before the port, the objects replaced by what they held) against ee_won's model module through its C ABI.
// - SyncClients / SyncGroups: the original's walk over two std::maps against the Rust plan applied the way the port
//   applies it, over every pair of id subsets of 0..7 and random larger maps: the same map afterwards and the same
//   creations and refreshes in the same order, with the same populations. The original's insertion hint started at
//   begin() and dangled once the first entry was erased (undefined behaviour: this STL loops forever on it); both
//   walks here, and the port, start it at end(), which a map never invalidates (a hint never changes the content);
// - AddClientToGroup / RemoveClientFromGroup's chat count, GetNumClients;
// - every client flag and member flag value, every room flag value, the friend's status and locate mark;
// - every ping value, the stale rule, IsSameGame, the list's key, every join status's text, AreAllReady over every
//   member set of up to four, the directory's data objects.
// Prints one line per failed check; the exit code is the number of failed checks.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <iterator>
#include <wchar.h>
#include "rust/ee_rust.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed = 12345;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }

// ---- the merge

// an entry of the lobby's map: what it was created or refreshed from
struct Entry {
    unsigned short mId;
    int mVersion;
};
// an entry of the server's map (its id is its key, as the routing code builds it)
struct Info {
    unsigned short mId;
    int mVersion;
    unsigned long mFlags;
    unsigned long mMembers;
};
typedef std::map<unsigned short, Entry> EntryMap;
typedef std::map<unsigned short, Info> InfoMap;

struct Log {
    std::vector<int> mEvents; // +id: created, -id-1: refreshed
    int mChat, mTotal;
    Log() : mChat(0), mTotal(0) {}
    bool operator==(const Log& o) const { return mEvents == o.mEvents && mChat == o.mChat && mTotal == o.mTotal; }
};

static bool IsChat(unsigned long inFlags) { return ((inFlags >> 11) & 1) ? true : false; }

// LobbyClientList::SyncClients and LobbyServer::SyncGroups's walk (the original's statements; the group's member
// count is the info's, the population summed as SyncGroups summed it)
static void OriginalSync(EntryMap& ioMap, const InfoMap& inMap, Log& outLog, bool inGroups)
{
    EntryMap::iterator aClient = ioMap.begin();
    EntryMap::iterator anInsert = ioMap.end(); // the original: begin(), a hint left dangling by the first erase (UB; see the header)
    InfoMap::const_iterator anInfo = inMap.begin();
    while (aClient != ioMap.end() && anInfo != inMap.end()) {
        if (anInfo->first > aClient->first) ioMap.erase(aClient++);
        else if (anInfo->first < aClient->first) {
            Entry aNew = { anInfo->second.mId, anInfo->second.mVersion };
            anInsert = ioMap.insert(anInsert, EntryMap::value_type(aNew.mId, aNew));
            outLog.mEvents.push_back(aNew.mId);
            if (inGroups) {
                if (IsChat(anInfo->second.mFlags)) outLog.mChat += anInfo->second.mMembers;
                outLog.mTotal += anInfo->second.mMembers;
            }
            ++anInfo;
        } else {
            aClient->second.mVersion = anInfo->second.mVersion;
            outLog.mEvents.push_back(-(int)aClient->first - 1);
            if (inGroups) {
                if (IsChat(anInfo->second.mFlags)) outLog.mChat += anInfo->second.mMembers;
                outLog.mTotal += anInfo->second.mMembers;
            }
            ++aClient; ++anInfo;
        }
    }
    if (inGroups) { while (aClient != ioMap.end()) ioMap.erase(aClient++); }
    else ioMap.erase(aClient, ioMap.end());
    while (anInfo != inMap.end()) {
        Entry aNew = { anInfo->second.mId, anInfo->second.mVersion };
        anInsert = ioMap.insert(anInsert, EntryMap::value_type(aNew.mId, aNew));
        outLog.mEvents.push_back(aNew.mId);
        if (inGroups) {
            if (IsChat(anInfo->second.mFlags)) outLog.mChat += anInfo->second.mMembers;
            outLog.mTotal += anInfo->second.mMembers;
        }
        ++anInfo;
    }
}

// the port's: the plan applied in order (LobbyModel.cpp's ApplySync)
static void PortSync(EntryMap& ioMap, const InfoMap& inMap, Log& outLog, bool inGroups)
{
    std::vector<unsigned short> anExisting, anIncoming;
    std::vector<const Info*> anInfos;
    for (EntryMap::iterator it = ioMap.begin(); it != ioMap.end(); ++it) anExisting.push_back(it->first);
    for (InfoMap::const_iterator it = inMap.begin(); it != inMap.end(); ++it) {
        anIncoming.push_back(it->first);
        anInfos.push_back(&it->second);
    }
    std::vector<EeWonSyncStep> aPlan(anExisting.size() + anIncoming.size() + 1);
    size_t aCount = ee_won_model_sync_plan(anExisting.empty() ? NULL : &anExisting[0], anExisting.size(),
                                           anIncoming.empty() ? NULL : &anIncoming[0], anIncoming.size(), &aPlan[0]);
    EeWonPopulation aPop;
    memset(&aPop, 0, sizeof(aPop));
    EntryMap::iterator aClient = ioMap.begin();
    EntryMap::iterator anInsert = ioMap.end();
    for (size_t k = 0; k < aCount; k++) {
        const EeWonSyncStep& s = aPlan[k];
        switch (s.kind) {
        case 0: ioMap.erase(aClient++); break;
        case 1: {
            const Info* anInfo = anInfos[s.incoming];
            Entry aNew = { anInfo->mId, anInfo->mVersion };
            anInsert = ioMap.insert(anInsert, EntryMap::value_type(anIncoming[s.incoming], aNew));
            outLog.mEvents.push_back(aNew.mId);
            if (inGroups) ee_won_model_population(&aPop, 0, (uint32_t)anInfo->mFlags, (uint32_t)anInfo->mMembers);
            break;
        }
        case 2: {
            const Info* anInfo = anInfos[s.incoming];
            aClient->second.mVersion = anInfo->mVersion;
            outLog.mEvents.push_back(-(int)aClient->first - 1);
            if (inGroups) ee_won_model_population(&aPop, 0, (uint32_t)anInfo->mFlags, (uint32_t)anInfo->mMembers);
            ++aClient;
            break;
        }
        case 3: ioMap.erase(aClient, ioMap.end()); aClient = ioMap.end(); break;
        }
    }
    outLog.mChat = aPop.chat;
    outLog.mTotal = aPop.total;
}

static bool SameMap(const EntryMap& a, const EntryMap& b)
{
    if (a.size() != b.size()) return false;
    for (EntryMap::const_iterator x = a.begin(), y = b.begin(); x != a.end(); ++x, ++y)
        if (x->first != y->first || x->second.mId != y->second.mId || x->second.mVersion != y->second.mVersion)
            return false;
    return true;
}

static void CheckSync(const EntryMap& inStart, const InfoMap& inInfos, bool inGroups)
{
    EntryMap a = inStart, b = inStart;
    Log la, lb;
    OriginalSync(a, inInfos, la, inGroups);
    PortSync(b, inInfos, lb, inGroups);
    CHECK(SameMap(a, b));
    CHECK(la == lb);
}

static void TestSync()
{
    for (int g = 0; g < 2; g++)
        for (unsigned a = 0; a < 256; a++)
            for (unsigned b = 0; b < 256; b++) {
                EntryMap aStart;
                InfoMap anInfos;
                for (unsigned short k = 0; k < 8; k++) {
                    if (a >> k & 1) { Entry e = { k, (int)(k * 10) }; aStart[k] = e; }
                    if (b >> k & 1) {
                        Info i = { k, (int)(k * 10 + 1 + (a & 3)), (unsigned long)((a ^ b) >> k & 1) << 11,
                                   (unsigned long)(k + b % 5) };
                        anInfos[k] = i;
                    }
                }
                CheckSync(aStart, anInfos, g != 0);
            }
    for (int n = 0; n < 2000; n++) {
        EntryMap aStart;
        InfoMap anInfos;
        int aSize = Rand() % 60, anInfoSize = Rand() % 60;
        for (int k = 0; k < aSize; k++) {
            unsigned short anId = (unsigned short)(Rand() % 100 + (n & 1 ? 65400 : 0));
            Entry e = { anId, (int)Rand() };
            aStart[anId] = e;
        }
        for (int k = 0; k < anInfoSize; k++) {
            unsigned short anId = (unsigned short)(Rand() % 100 + (n & 1 ? 65400 : 0));
            Info i = { anId, (int)Rand(), (unsigned long)Rand() << (Rand() % 6), (unsigned long)(Rand() % 300) };
            anInfos[anId] = i;
        }
        CheckSync(aStart, anInfos, (n & 2) != 0);
    }
}

// ---- the counts

static void TestPopulation()
{
    for (int aStart = -3; aStart < 4; aStart++)
        for (unsigned long aFlags = 0; aFlags < 0x1000; aFlags += 0x7f) {
            unsigned long f = aFlags | ((aStart & 1) ? 0x800 : 0);
            // AddClientToGroup
            int aChat = aStart;
            if (IsChat(f)) ++aChat;
            EeWonPopulation p = { aStart, 77 };
            ee_won_model_population(&p, 1, (uint32_t)f, 0);
            CHECK(p.chat == aChat && p.total == 77);
            // RemoveClientFromGroup (a client was removed)
            aChat = aStart;
            if (IsChat(f)) {
                --aChat;
                if (aChat < 0) aChat = 0;
            }
            EeWonPopulation q = { aStart, 77 };
            ee_won_model_population(&q, 2, (uint32_t)f, 0);
            CHECK(q.chat == aChat && q.total == 77);
        }
    for (int c = 0; c < 2; c++)
        for (unsigned aLive = 0; aLive < 5; aLive++)
            for (int aListed = -1; aListed < 3; aListed++) {
                int aWant = c ? (int)aLive : aListed; // GetNumClients
                CHECK(ee_won_model_server_num_clients(c != 0, aLive, aListed) == aWant);
            }
}

// ---- the flags

static void TestFlags()
{
    for (unsigned long aFlags = 0; aFlags < 0x100; aFlags++)
        for (unsigned aChat = 0; aChat < 8; aChat++)
            for (unsigned aGame = 0; aGame < 8; aGame++) {
                unsigned long f = aFlags | (aFlags << 8);
                EeWonClientStatus s = ee_won_model_client_status((uint32_t)f, (uint8_t)aChat, (uint8_t)aGame);
                CHECK(s.anonymous == ((f & 0x40) ? true : false));        // IsAnonymous
                CHECK(s.away == ((f & 8) ? true : false));                // IsAway
                CHECK(s.admin == ((f & 0x20) ? true : false));            // IsAdmin
                CHECK(s.moderator == ((f & 1) ? true : false));           // IsModerator
                // IsMuted
                bool aMutedGame = (f & 2) ? true : ((aGame & 2) ? true : false);
                bool aMutedChat = (f & 2) ? true : ((aChat & 2) ? true : false);
                CHECK(s.muted_game == aMutedGame);
                CHECK(s.muted_chat == aMutedChat);
            }
    for (unsigned long f = 0; f < 0x2000; f++) {
        unsigned long g = f | ((f & 0x1000) ? 0xffff0000ul : 0);
        EeWonGroupKind k = ee_won_model_group_kind((uint32_t)g);
        bool aPassword = (g >> 2) & 1, anAsk = (g >> 8) & 1, anInvite = (g >> 7) & 1;
        bool aChat = ((g >> 11) & 1) ? true : false;
        bool aGameGroup = ((g >> 11) & 1) ? false : true;
        CHECK(k.password == aPassword);
        CHECK(k.ask_to_join == anAsk);
        CHECK(k.invite_only == anInvite);
        CHECK(k.chat == aChat && !k.chat == aGameGroup);
        CHECK(k.public_chat == (aChat && !aPassword && !anInvite && !anAsk)); // IsPublicChatGroup
    }
    for (int c = 0; c < 2; c++)
        for (int g = 0; g < 2; g++) {
            int flags = 0; // SetStatusFromClient
            if (c) flags = 1;
            if (g) flags |= 2;
            CHECK(ee_won_model_friend_status(c != 0, g != 0) == flags);
        }
    for (unsigned f = 0; f < 256; f++) {
        unsigned char aFlags = (unsigned char)f;
        aFlags &= ~4; // EndLocate
        CHECK(ee_won_model_friend_end_locate((uint8_t)f) == aFlags);
    }
    CHECK(ee_won_model_friend_start_locate() == 4); // StartLocate
}

// ---- the games

static void TestGames()
{
    for (unsigned p = 0; p < 0x10000; p++) {
        unsigned short aPing = (unsigned short)p;
        CHECK(ee_won_model_game_pinging(aPing) == (aPing == 0xfffd || aPing == 0xfffc)); // IsPinging
        CHECK(ee_won_model_game_querying(aPing) == (aPing == 0xfffc));                   // IsQueryingDetails
    }
    // RemoveStaleGames: now - it->second->mLastSeenTime > inMaxAge (time_t, long, long)
    long long aNows[] = { 0, 29, 30, 31, 1000, 0x7fffffffLL, 0x80000000LL, 0x1000000000LL };
    long aSeen[] = { 0, 1, 970, 999, 1000, 0x7fffffffL, -5 };
    long anAges[] = { 0, 30, 300, -1 };
    for (size_t i = 0; i < sizeof(aNows) / sizeof(aNows[0]); i++)
        for (size_t j = 0; j < sizeof(aSeen) / sizeof(aSeen[0]); j++)
            for (size_t k = 0; k < sizeof(anAges) / sizeof(anAges[0]); k++) {
                long long now = aNows[i];
                bool aStale = now - aSeen[j] > anAges[k];
                CHECK(ee_won_model_game_stale(now, (int32_t)aSeen[j], (int32_t)anAges[k]) == aStale);
            }
    for (int a = 0; a < 5; a++)
        for (int b = 0; b < 5; b++)
            for (int g = 0; g < 4; g++)
                for (int s = 0; s < 2; s++) {
                    unsigned short ga = (unsigned short)(g & 1), gb = (unsigned short)(g >> 1);
                    // IsSameGame
                    bool aSame;
                    if (b != a) aSame = false;
                    else if (a == 2) aSame = gb == ga;
                    else aSame = s != 0;
                    CHECK(ee_won_model_same_game(a, b, ga, gb, s != 0) == aSame);
                }
    for (int t = -1; t < 5; t++) CHECK(ee_won_model_game_list_by_id(t) == (t == 2)); // mType==2
    // GetStatusString's switch: the index of its string
    for (int s = -32768; s < 32768; s++) {
        int aWant;
        switch ((short)s) {
        case 0: aWant = 0; break;
        case 2: aWant = 1; break;
        case 3: aWant = 2; break;
        case 4: aWant = 3; break;
        case 5: aWant = 4; break;
        case 6: aWant = 5; break;
        case 7: aWant = 6; break;
        case 8: aWant = 7; break;
        default: aWant = -1;
        }
        CHECK(ee_won_model_status_text((int16_t)s) == aWant);
    }
    // AreAllReady over every member set of up to four
    for (int n = 0; n <= 4; n++) {
        int aTotal = 1 << (3 * n);
        for (int c = 0; c < aTotal; c++) {
            EeWonMemberReady m[4];
            bool aWant = true;
            for (int k = 0; k < n; k++) {
                int b = c >> (3 * k);
                m[k].has_player = b & 1;
                m[k].game_captain = (b >> 1) & 1;
                m[k].ready = (b >> 2) & 1;
                if (m[k].has_player && !m[k].game_captain && !m[k].ready) { aWant = false; break; }
            }
            CHECK(ee_won_model_all_ready(n ? m : NULL, (size_t)n) == aWant);
        }
    }
}

// ---- the directory's data objects (ReadDirEntity)

static void TestDirObjects()
{
    const char* aKinds[] = { "_C", "_L", "_c", "C", "_CC", "", "_N" };
    for (size_t k = 0; k < sizeof(aKinds) / sizeof(aKinds[0]); k++)
        for (int aLength = -1; aLength < 5; aLength++) {
            std::string aType = aKinds[k];
            unsigned char aData[4] = { 0x34, 0x12, 0x99, 0x01 };
            bool aHasData = aLength >= 0;
            // the original's tests
            int aWant = 0;
            unsigned short aCount = 0;
            if (aType == "_C" && aHasData && aLength == 2) {
                memcpy(&aCount, aData, 2);
                aWant = 1;
            } else if (aType == "_L") aWant = 2;
            uint16_t aGot = 0;
            int aKind = ee_won_model_dir_object(reinterpret_cast<const uint8_t*>(aType.data()), aType.length(),
                                                aHasData ? aData : NULL, aHasData ? (size_t)aLength : 0, &aGot);
            CHECK(aKind == aWant);
            if (aWant == 1) CHECK(aGot == aCount);
        }
}

// ---- round 17: the lobby's containers (LobbyModel's client, room, game and server maps, the server's address
// names; LobbyPersistentData's users, ids, ignored names, friends, options, reconnect ids, LAN names and direct
// connects; LobbyOptionSet) as the original's std::map / std::set statements with the original comparators: an id
// (unsigned short / unsigned long, or long), IPAddr::operator< (the Rust value's compare, as the class calls it),
// GUIStringLessNoCase (GUIStringData::CompareNoCase) and GUIString::operator< (Compare) over wchar_t units,
// StringLessNoCase (StringCompareNoCase) over bytes, and LobbyOptionSet's Add / Erase. Each step's result is a
// line, compared with rust/crates/ee_won/src/golden_lobbymap.txt (recorded over these std containers before the
// move; `--print` prints the lines). Keys are named by their index into the candidate lists below, which the Rust
// replay (lobbymap.rs) repeats.

static std::vector<std::string> sGolden;
static void Golden(const std::string& inLine) { sGolden.push_back(inLine); }
static std::string Num(long long inValue)
{
    char aBuffer[32];
    snprintf(aBuffer, sizeof(aBuffer), "%lld", inValue);
    return aBuffer;
}

// the candidates: ids, addresses (host order, port), wide texts, narrow texts
static const unsigned long kMapIds[] = { 0, 1, 2, 3, 7, 10, 0xfffe, 0xffff, 0x10000, 0x7fffffff, 0x80000000u, 0xffffffffu };
static const long kMapSigned[] = { 0, 1, -1, 2, -2, 100, -100, 0x7fffffff, -0x7fffffff - 1, 5 };
static const unsigned long kMapHosts[] = { 0x7f000001u, 0x0a000001u, 0xc0a80105u, 0x0a000001u, 0x7f000001u, 0, 0xffffffffu, 0x01020304u };
static const unsigned short kMapPorts[] = { 8585, 8585, 15100, 15101, 1, 0, 65535, 0x0100 };
static const wchar_t* const kMapWide[] = { L"Alpha", L"alpha", L"ALPHA", L"Beta", L"beta2", L"", L"Gamma", L"gAmma",
                                           L"Zed", L"a", L"A", L"_x", L"[x", L"zz", L"Alph", L"Alpha ", L"\x00e9t\x00e9" };
static const size_t kMapWideCount = sizeof(kMapWide) / sizeof(kMapWide[0]);
static const size_t kMapWideNoCase = kMapWideCount - 1; // the no-case maps' candidates: ASCII (the C locale's towupper)
static const char* const kMapNarrow[] = { "Server.won.net", "server.WON.net", "a", "B", "_c", "[d", "", "Zz", "\xc3\xa9",
                                          "server.won.net2", "b" };
static const size_t kMapNarrowCount = sizeof(kMapNarrow) / sizeof(kMapNarrow[0]);

// the key kinds (the Rust EeWonLobbyKeyKind's order)
enum MapKind { kUnsigned, kSigned, kAddr, kNoCase, kText, kNarrowNoCase, kKinds };
static size_t CandidateCount(int inKind)
{
    switch (inKind) {
    case kUnsigned: return sizeof(kMapIds) / sizeof(kMapIds[0]);
    case kSigned: return sizeof(kMapSigned) / sizeof(kMapSigned[0]);
    case kAddr: return sizeof(kMapHosts) / sizeof(kMapHosts[0]);
    case kNoCase: return kMapWideNoCase;
    case kText: return kMapWideCount;
    default: return kMapNarrowCount;
    }
}

// a key: the candidate's index, compared as the original's comparator compares the candidate
struct MapKey {
    int mKind;
    int mIndex;
};
static EeWonIpAddr* sMapAddrs[sizeof(kMapHosts) / sizeof(kMapHosts[0])];
static int MapCompare(const MapKey& a, const MapKey& b)
{
    switch (a.mKind) {
    case kUnsigned: {
        unsigned long x = kMapIds[a.mIndex], y = kMapIds[b.mIndex];
        return x < y ? -1 : x > y ? 1 : 0;
    }
    case kSigned: {
        long x = kMapSigned[a.mIndex], y = kMapSigned[b.mIndex];
        return x < y ? -1 : x > y ? 1 : 0;
    }
    case kAddr: return ee_won_ipaddr_compare(sMapAddrs[a.mIndex], sMapAddrs[b.mIndex]);
    case kNoCase:
    case kText: {
        const wchar_t* x = kMapWide[a.mIndex];
        const wchar_t* y = kMapWide[b.mIndex];
        return ee_won_guistring_op(a.mKind == kNoCase ? EeWonGuiStringOp::CompareNoCase : EeWonGuiStringOp::Compare,
                                   x, (uint16_t)wcslen(x), y, (uint16_t)wcslen(y), 0, sizeof(wchar_t));
    }
    default: {
        const char* x = kMapNarrow[a.mIndex];
        const char* y = kMapNarrow[b.mIndex];
        return ee_won_compare_no_case(reinterpret_cast<const uint8_t*>(x), strlen(x),
                                      reinterpret_cast<const uint8_t*>(y), strlen(y));
    }
    }
}
struct MapLess {
    bool operator()(const MapKey& a, const MapKey& b) const { return MapCompare(a, b) < 0; }
};
typedef std::map<MapKey, int, MapLess> OrigMap;

static std::string Dump(const OrigMap& inMap)
{
    std::string s = Num((long long)inMap.size()) + ":";
    for (OrigMap::const_iterator it = inMap.begin(); it != inMap.end(); ++it)
        s += " " + Num(it->first.mIndex) + "=" + Num(it->second);
    return s;
}
static long long Index(const OrigMap& inMap, OrigMap::const_iterator inItr)
{
    return inItr == inMap.end() ? -1 : (long long)std::distance(inMap.begin(), inItr);
}

// one map's script: insert (insert(value_type(k, 0))), set (m[k] = v), find, erase by key, lower and upper bound,
// erase at a position, erase from a position to the end (SyncClients's tail), clear, copy, dump
static void MapScript(int inKind, unsigned inSeed)
{
    unsigned r = inSeed * 2654435761u + (unsigned)inKind;
    OrigMap m;
    OrigMap aCopy;
    const std::string aHead = "map " + Num(inKind) + " " + Num(inSeed) + " ";
    for (int aStep = 0; aStep < 160; aStep++) {
        r = r * 1103515245u + 12345u;
        unsigned anOp = (r >> 8) % 20;
        MapKey k = { inKind, (int)((r >> 16) % CandidateCount(inKind)) };
        int v = (int)((r >> 24) % 100);
        std::string s;
        if (anOp < 5) {
            std::pair<OrigMap::iterator, bool> p = m.insert(OrigMap::value_type(k, 0));
            s = "ins " + Num(k.mIndex) + " -> " + Num(Index(m, p.first)) + " " + Num(p.second);
        } else if (anOp < 9) {
            m[k] = v;
            s = "set " + Num(k.mIndex) + " " + Num(v) + " -> " + Num(Index(m, m.find(k)));
        } else if (anOp < 12) {
            s = "find " + Num(k.mIndex) + " -> " + Num(Index(m, m.find(k)));
        } else if (anOp < 14) {
            s = "erase " + Num(k.mIndex) + " -> " + Num((long long)m.erase(k));
        } else if (anOp == 14) {
            s = "lb " + Num(k.mIndex) + " -> " + Num(Index(m, m.lower_bound(k)));
        } else if (anOp == 15) {
            s = "ub " + Num(k.mIndex) + " -> " + Num(Index(m, m.upper_bound(k)));
        } else if (anOp == 16) {
            if (m.empty()) s = "eraseat none";
            else {
                size_t i = (r >> 16) % m.size();
                OrigMap::iterator it = m.begin();
                std::advance(it, i);
                m.erase(it);
                s = "eraseat " + Num((long long)i);
            }
        } else if (anOp == 17) {
            size_t i = m.empty() ? 0 : (r >> 16) % (m.size() + 1);
            if ((r >> 28) % 4 == 0) {
                OrigMap::iterator it = m.begin();
                std::advance(it, i);
                m.erase(it, m.end());
                s = "erasefrom " + Num((long long)i);
            } else s = "dump";
        } else if (anOp == 18) {
            if ((r >> 28) % 8 == 0) {
                m.clear();
                s = "clear";
            } else {
                aCopy = m;
                s = "copy";
            }
        } else {
            s = "copied " + Dump(aCopy);
        }
        Golden(aHead + s + " | " + Dump(m));
    }
}

// LobbyOptionSet's statements (Add, Erase, clear) over the wide candidates, GUIString::operator< as the index
struct WideLess {
    bool operator()(int a, int b) const
    {
        MapKey x = { kText, a }, y = { kText, b };
        return MapCompare(x, y) < 0;
    }
};
struct OrigOptionSet {
    typedef std::list<int> StringList;
    typedef std::map<int, StringList::iterator, WideLess> StringMap;
    StringMap mIndex;
    StringList mValues;
    void Add(int inValue)
    {
        std::pair<StringMap::iterator, bool> aResult = this->mIndex.insert(StringMap::value_type(inValue, this->mValues.end()));
        if (!aResult.second)
            return;
        this->mValues.push_back(inValue);
        StringList::iterator anItr = this->mValues.end();
        --anItr;
        aResult.first->second = anItr;
    }
    bool Erase(int inValue)
    {
        StringMap::iterator anItr = this->mIndex.find(inValue);
        if (anItr != this->mIndex.end()) {
            this->mValues.erase(anItr->second);
            this->mIndex.erase(anItr);
            return true;
        }
        return false;
    }
    void clear()
    {
        this->mIndex.clear();
        this->mValues.clear();
    }
};
static void OptionScript(unsigned inSeed)
{
    unsigned r = inSeed * 2246822519u;
    OrigOptionSet o;
    const std::string aHead = "opt " + Num(inSeed) + " ";
    for (int aStep = 0; aStep < 120; aStep++) {
        r = r * 1103515245u + 12345u;
        unsigned anOp = (r >> 8) % 10;
        int v = (int)((r >> 16) % kMapWideCount);
        std::string s;
        if (anOp < 5) {
            size_t aBefore = o.mValues.size();
            o.Add(v);
            s = "add " + Num(v) + " -> " + Num(o.mValues.size() != aBefore);
        } else if (anOp < 9) {
            s = "erase " + Num(v) + " -> " + Num(o.Erase(v));
        } else {
            o.clear();
            s = "clear";
        }
        s += " |";
        for (OrigOptionSet::StringList::iterator it = o.mValues.begin(); it != o.mValues.end(); ++it)
            s += " " + Num(*it);
        Golden(aHead + s);
    }
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_lobbymap.txt";
}

// ---- the same scripts over the Rust container (lobbymap.rs) through its C ABI, as WONLobbyMap.h calls it: the
// lines must be the golden's

static std::vector<std::string> sPortGolden;
static const EeWonLobbyKeyKind kPortKinds[] = { EeWonLobbyKeyKind::Unsigned, EeWonLobbyKeyKind::Signed,
                                                EeWonLobbyKeyKind::Addr, EeWonLobbyKeyKind::NoCase,
                                                EeWonLobbyKeyKind::Text, EeWonLobbyKeyKind::NarrowNoCase };
static EeWonLobbyKey PortKey(int inKind, int i)
{
    EeWonLobbyKey k = { 0, NULL, NULL, 0, 0 };
    switch (inKind) {
    case kUnsigned: k.num = kMapIds[i]; break;
    case kSigned: k.num = (uint64_t)(int64_t)kMapSigned[i]; break;
    case kAddr: k.addr = sMapAddrs[i]; break;
    case kNoCase:
    case kText:
        k.text = kMapWide[i];
        k.count = wcslen(kMapWide[i]);
        k.unit_size = sizeof(wchar_t);
        break;
    default:
        k.text = kMapNarrow[i];
        k.count = strlen(kMapNarrow[i]);
        k.unit_size = 1;
        break;
    }
    return k;
}
// the candidate the stored key at a position is (its spelling)
static int PortKeyIndex(int inKind, const EeWonLobbyMap* m, size_t inAt)
{
    for (size_t i = 0; i < CandidateCount(inKind == kNoCase ? kText : inKind); i++) {
        switch (inKind) {
        case kUnsigned: if (ee_won_lobbymap_key_num(m, inAt) == kMapIds[i]) return (int)i; break;
        case kSigned: if ((int64_t)ee_won_lobbymap_key_num(m, inAt) == kMapSigned[i]) return (int)i; break;
        case kAddr: if (ee_won_ipaddr_compare(ee_won_lobbymap_key_addr(m, inAt), sMapAddrs[i]) == 0) return (int)i; break;
        case kNoCase:
        case kText: {
            size_t n = 0;
            const uint32_t* u = static_cast<const uint32_t*>(ee_won_lobbymap_key_text(m, inAt, &n));
            const wchar_t* w = kMapWide[i];
            bool aSame = n == wcslen(w);
            for (size_t j = 0; aSame && j < n; j++) aSame = u[j] == (uint32_t)w[j];
            if (aSame) return (int)i;
            break;
        }
        default: {
            size_t n = 0;
            const char* b = static_cast<const char*>(ee_won_lobbymap_key_text(m, inAt, &n));
            if (n == strlen(kMapNarrow[i]) && memcmp(b, kMapNarrow[i], n) == 0) return (int)i;
            break;
        }
        }
    }
    return -1;
}
static std::string PortDump(int inKind, const EeWonLobbyMap* m)
{
    size_t n = ee_won_lobbymap_len(m);
    std::string s = Num((long long)n) + ":";
    for (size_t i = 0; i < n; i++)
        s += " " + Num(PortKeyIndex(inKind, m, i)) + "=" + Num(ee_won_lobbymap_num(m, i, 0));
    return s;
}
static long long PortIndex(const EeWonLobbyMap* m, size_t i) { return i < ee_won_lobbymap_len(m) ? (long long)i : -1; }

static void MapScriptPort(int inKind, unsigned inSeed)
{
    unsigned r = inSeed * 2654435761u + (unsigned)inKind;
    EeWonLobbyMapHooks aNoHooks = { NULL, NULL };
    EeWonLobbyMap* m = ee_won_lobbymap_new(kPortKinds[inKind], aNoHooks);
    EeWonLobbyMap* aCopy = ee_won_lobbymap_new(kPortKinds[inKind], aNoHooks);
    const std::string aHead = "map " + Num(inKind) + " " + Num(inSeed) + " ";
    for (int aStep = 0; aStep < 160; aStep++) {
        r = r * 1103515245u + 12345u;
        unsigned anOp = (r >> 8) % 20;
        int ki = (int)((r >> 16) % CandidateCount(inKind));
        EeWonLobbyKey k = PortKey(inKind, ki);
        int v = (int)((r >> 24) % 100);
        size_t aSize = ee_won_lobbymap_len(m);
        std::string s;
        if (anOp < 5) {
            bool aNew = false;
            size_t i = ee_won_lobbymap_insert(m, &k, &aNew);
            s = "ins " + Num(ki) + " -> " + Num((long long)i) + " " + Num(aNew);
        } else if (anOp < 9) {
            size_t i = ee_won_lobbymap_insert(m, &k, NULL);
            ee_won_lobbymap_set_num(m, i, 0, v);
            s = "set " + Num(ki) + " " + Num(v) + " -> " + Num((long long)i);
        } else if (anOp < 12) {
            s = "find " + Num(ki) + " -> " + Num((long long)ee_won_lobbymap_find(m, &k));
        } else if (anOp < 14) {
            s = "erase " + Num(ki) + " -> " + Num(ee_won_lobbymap_erase(m, &k));
        } else if (anOp == 14) {
            s = "lb " + Num(ki) + " -> " + Num(PortIndex(m, ee_won_lobbymap_lower_bound(m, &k)));
        } else if (anOp == 15) {
            s = "ub " + Num(ki) + " -> " + Num(PortIndex(m, ee_won_lobbymap_upper_bound(m, &k)));
        } else if (anOp == 16) {
            if (aSize == 0) s = "eraseat none";
            else {
                size_t i = (r >> 16) % aSize;
                ee_won_lobbymap_erase_at(m, i);
                s = "eraseat " + Num((long long)i);
            }
        } else if (anOp == 17) {
            size_t i = aSize == 0 ? 0 : (r >> 16) % (aSize + 1);
            if ((r >> 28) % 4 == 0) {
                ee_won_lobbymap_erase_from(m, i);
                s = "erasefrom " + Num((long long)i);
            } else s = "dump";
        } else if (anOp == 18) {
            if ((r >> 28) % 8 == 0) {
                ee_won_lobbymap_clear(m);
                s = "clear";
            } else {
                ee_won_lobbymap_assign(aCopy, m);
                s = "copy";
            }
        } else {
            s = "copied " + PortDump(inKind, aCopy);
        }
        sPortGolden.push_back(aHead + s + " | " + PortDump(inKind, m));
    }
    ee_won_lobbymap_free(m);
    ee_won_lobbymap_free(aCopy);
}
static void OptionScriptPort(unsigned inSeed)
{
    unsigned r = inSeed * 2246822519u;
    EeWonLobbyMapHooks aNoHooks = { NULL, NULL };
    EeWonLobbyMap* o = ee_won_lobbymap_new(EeWonLobbyKeyKind::InOrder, aNoHooks);
    const std::string aHead = "opt " + Num(inSeed) + " ";
    for (int aStep = 0; aStep < 120; aStep++) {
        r = r * 1103515245u + 12345u;
        unsigned anOp = (r >> 8) % 10;
        int v = (int)((r >> 16) % kMapWideCount);
        EeWonLobbyKey k = PortKey(kText, v);
        std::string s;
        if (anOp < 5) {
            bool aNew = false;
            ee_won_lobbymap_insert(o, &k, &aNew);
            s = "add " + Num(v) + " -> " + Num(aNew);
        } else if (anOp < 9) {
            s = "erase " + Num(v) + " -> " + Num(ee_won_lobbymap_erase(o, &k));
        } else {
            ee_won_lobbymap_clear(o);
            s = "clear";
        }
        s += " |";
        for (size_t i = 0; i < ee_won_lobbymap_len(o); i++)
            s += " " + Num(PortKeyIndex(kText, o, i));
        sPortGolden.push_back(aHead + s);
    }
    ee_won_lobbymap_free(o);
}

static void TestContainers(bool inPrint)
{
    for (size_t i = 0; i < sizeof(kMapHosts) / sizeof(kMapHosts[0]); i++) {
        sMapAddrs[i] = ee_won_ipaddr_new();
        ee_won_ipaddr_set_host_order(sMapAddrs[i], (uint32_t)kMapHosts[i], kMapPorts[i]);
    }
    for (int aKind = 0; aKind < kKinds; aKind++)
        for (unsigned aSeed = 1; aSeed <= 6; aSeed++) {
            MapScript(aKind, aSeed);
            MapScriptPort(aKind, aSeed);
        }
    for (unsigned aSeed = 1; aSeed <= 6; aSeed++) {
        OptionScript(aSeed);
        OptionScriptPort(aSeed);
    }
    CHECK(sPortGolden == sGolden);
    for (size_t i = 0; i < sGolden.size() && i < sPortGolden.size(); i++)
        if (sGolden[i] != sPortGolden[i] && sFailures < 40) {
            printf("  port line %u: std '%s', rust '%s'\n", (unsigned)(i + 1), sGolden[i].c_str(), sPortGolden[i].c_str());
            sFailures++;
        }
    for (size_t i = 0; i < sizeof(kMapHosts) / sizeof(kMapHosts[0]); i++)
        ee_won_ipaddr_free(sMapAddrs[i]);
    if (inPrint) {
        for (size_t i = 0; i < sGolden.size(); i++)
            printf("%s\n", sGolden[i].c_str());
        return;
    }
    FILE* aFile = fopen(GoldenPath().c_str(), "rb");
    CHECK(aFile != NULL);
    if (aFile == NULL)
        return;
    std::vector<std::string> aWant;
    static char aLine[65536];
    while (fgets(aLine, sizeof(aLine), aFile)) {
        size_t n = strlen(aLine);
        while (n && (aLine[n - 1] == '\n' || aLine[n - 1] == '\r'))
            aLine[--n] = 0;
        aWant.push_back(aLine);
    }
    fclose(aFile);
    CHECK(aWant.size() == sGolden.size());
    for (size_t i = 0; i < aWant.size() && i < sGolden.size(); i++) {
        CHECK(aWant[i] == sGolden[i]);
        if (aWant[i] != sGolden[i] && sFailures < 40)
            printf("  golden line %u: want '%s', got '%s'\n", (unsigned)(i + 1), aWant[i].c_str(), sGolden[i].c_str());
    }
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    TestContainers(aPrint);
    if (aPrint)
        return 0;
    TestSync();
    TestPopulation();
    TestFlags();
    TestGames();
    TestDirObjects();
    printf("won_model_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
