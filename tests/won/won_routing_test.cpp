// RoutingLogic's decisions (RoutingLogic.cpp): the original's statements (copied from the C++ before the port, the
// objects replaced by the state they answered) against ee_won's routing module through its C ABI: GetRoomSpec and
// GetGroup over every id and room pair, SelectGame's ping ceiling and its per-game rank over skills, pings, limits
// and ceilings, ShouldLocateFriend over its inputs and wrapping times, GetDefaultServer over every list of up to
// four servers, the anonymous and dirty-word flags, CanAddFriend. Prints one line per failed check; the exit code
// is the number of failed checks.
#include <stdio.h>
#include <string.h>
#include <vector>
#include "rust/ee_won.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static bool B(int inBits, int inBit) { return ((inBits >> inBit) & 1) != 0; }

static void TestRooms()
{
    for (int c = -1; c < 4; c++)
        for (int g = -1; g < 4; g++)
            for (int anId = 0; anId < 4; anId++) {
                int aWant = 0; // GetRoomSpec(id): a group of -1 is no group
                if (c >= 0 && c == anId) aWant = 5;
                else if (g >= 0 && g == anId) aWant = 6;
                CHECK(ee_won_routing_room_spec((uint16_t)anId, c, g) == aWant);
            }
    for (int s = 0; s < 64; s++) {
        int aWant = 0; // GetGroup
        if (s & 1) aWant = 1;
        else if (s & 2) aWant = 2;
        CHECK(ee_won_routing_group_of_spec(s) == aWant);
    }
}

static void TestQuickPlay()
{
    unsigned short aCeilings[] = { 0, 1, 9, 1000, 0xf000, 0xf001, 0xfffe, 0xffff };
    for (int c = 0; c < 2; c++)
        for (size_t k = 0; k < sizeof(aCeilings) / sizeof(aCeilings[0]); k++) {
            unsigned short aPingCeiling = 10000; // SelectGame
            if (c) {
                aPingCeiling = aCeilings[k];
                if (aPingCeiling == 0 || aPingCeiling > 0xf000) aPingCeiling = 0xf000;
            }
            CHECK(ee_won_routing_ping_ceiling(c != 0, aCeilings[k]) == aPingCeiling);
        }
    unsigned short aPings[] = { 0, 1, 40, 999, 1000, 1001, 9999, 10000, 0xefff, 0xf000, 0xfffc, 0xfffd, 0xfffe, 0xffff };
    unsigned short aLimits[] = { 0, 1000, 0xffff };
    unsigned short aCeils[] = { 10000, 0xf000, 50 };
    for (int f = 0; f < 8; f++)
        for (size_t p = 0; p < sizeof(aPings) / sizeof(aPings[0]); p++)
            for (int aSkill = -2; aSkill < 6; aSkill++)
                for (int aWanted = -1; aWanted < 5; aWanted++)
                    for (size_t l = 0; l < 3; l++)
                        for (size_t ce = 0; ce < 3; ce++) {
                            bool password = B(f, 0), ask = B(f, 1), canJoin = !B(f, 2);
                            unsigned short aPing = aPings[p];
                            int anAction = 0, aRankOut = 0;
                            do { // SelectGame's loop body
                                if (password || ask || !canJoin) break;
                                if (aPing < aCeils[ce]) {
                                    int aRank;
                                    if (aSkill == aWanted) aRank = 0;
                                    else if (aSkill == 0) aRank = 10000;
                                    else if (aSkill + 1 == aWanted) aRank = 20000;
                                    else if (aSkill - 1 == aWanted) aRank = 30000;
                                    else break;
                                    if (aPing < aLimits[l]) {
                                        aRank += aPing;
                                        anAction = 1;
                                        aRankOut = aRank;
                                    }
                                } else if (aPing == 0xfffe) {
                                    anAction = 2;
                                }
                            } while (0);
                            EeWonQuickPlay q = ee_won_routing_quick_play_rank(password, ask, canJoin, aPing, aSkill,
                                                                              aWanted, aLimits[l], aCeils[ce]);
                            CHECK(q.action == anAction);
                            if (anAction == 1) CHECK(q.rank == aRankOut);
                        }
}

static void TestFriendsAndServers()
{
    unsigned long aTimes[] = { 0, 1, 29, 30, 31, 100, 0x7fffffff, 0x80000000, 0xffffffe2, 0xffffffff };
    for (int f = 0; f < 8; f++)
        for (size_t n = 0; n < 4; n++)
            for (size_t i = 0; i < 10; i++)
                for (size_t j = 0; j < 10; j++) {
                    bool locating = B(f, 0), here = B(f, 1), connected = B(f, 2);
                    unsigned long inNow = aTimes[i], aLast = aTimes[j];
                    bool aWant; // ShouldLocateFriend (mFlags & 4, the current server's address, the server count)
                    do {
                        if (locating) { aWant = false; break; }
                        if (connected && here) { aWant = false; break; }
                        if (n == 0) { aWant = false; break; }
                        if (n == 1 && connected) { aWant = false; break; }
                        aWant = static_cast<long>(inNow - aLast) >= 30 ? true : false;
                    } while (0);
                    CHECK(ee_won_routing_should_locate_friend(locating, connected && here, n, connected,
                                                              (uint32_t)inNow, (uint32_t)aLast) == aWant);
                }
    // GetDefaultServer over every list of up to four servers (last used, clients 0..2, language)
    for (int n = 0; n <= 4; n++) {
        int aTotal = 1;
        for (int k = 0; k < n; k++) aTotal *= 12;
        for (int c = 0; c < aTotal; c++) {
            EeWonServerPick s[4];
            int v = c;
            for (int k = 0; k < n; k++) {
                int d = v % 12;
                v /= 12;
                s[k].last_used = d == 11;
                s[k].clients = d % 3;
                s[k].language = (d / 3) & 1;
            }
            int aWant = -1;
            int aBest = -1, aPreferred = -1;
            for (int k = 0; k < n; k++) {
                if (s[k].last_used) { aWant = k; break; }
                if (aBest < 0 || s[k].clients > s[aBest].clients) aBest = k;
                if (s[k].language) {
                    if (aPreferred < 0 || s[k].clients > s[aPreferred].clients) aPreferred = k;
                }
            }
            if (aWant < 0) aWant = aPreferred >= 0 ? aPreferred : aBest;
            CHECK(ee_won_routing_default_server(n ? s : NULL, (size_t)n) == aWant);
        }
    }
    for (int c = 0; c < 2; c++) {
        uint32_t aMask, aFlags;
        unsigned long flags = 0; // HandleAnonymousModeChange
        if (c) flags |= 0x40;
        ee_won_routing_client_flags(0, c != 0, &aMask, &aFlags);
        CHECK(aMask == 0x40 && aFlags == flags);
        flags = 0; // HandleDirtyWordFilterChange
        if ((c != 0) != true) flags |= 4;
        ee_won_routing_client_flags(1, c != 0, &aMask, &aFlags);
        CHECK(aMask == 4 && aFlags == flags);
    }
    for (size_t n = 250; n < 260; n++) CHECK(ee_won_routing_can_add_friend(n) == !(n >= 255)); // CanAddFriend
}

int main()
{
    TestRooms();
    TestQuickPlay();
    TestFriendsAndServers();
    printf("won_routing_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
