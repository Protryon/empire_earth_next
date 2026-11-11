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

int main()
{
    TestSync();
    TestPopulation();
    TestFlags();
    TestGames();
    TestDirObjects();
    printf("won_model_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
