// TEST ONLY: the next engine's C++ test strategy (ee/EECPNext.h; docs/AI_LUA_PLAN.md §5 S1 "Proof"): a fixed boom
// opening from the Prehistoric toward the Copper Age that uses nothing but the view and the verbs. It proves the
// tactics layer; it is not a good AI (stage S3 writes the boom script). Stage S2 rewrites it in Lua and must play the
// same game. New in this port: no original address.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <cstring>

#include "lle/LowLevelEngine.h"
#include "lle/U2DPoint.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EECPTrace.h"
#include "ee/EECPNext.h"

static const long kTestCitizens[3] = {25, 40, 50};   // the citizens wanted in the Prehistoric, Stone and later
static const long kTestSoldiers[3] = {4, 8, 8};      // the mobile defence
static const long kTestTowers[3] = {2, 4, 4};
static const long kTestBoats = 6;
static const unsigned long kTestClaimMs = 10000;      // a claim and gather pass every 10 s
static const unsigned long kTestAttackMs = 10 * 60000;   // the one attack: this long into the Stone Age

// the gatherer shares in percent per epoch (food takes the rest): Prehistoric, Stone, later
static const long kTestShares[3][kNextResources] = {
    {0, 20, 0, 10, 15},
    {0, 22, 8, 12, 15},
    {0, 20, 10, 12, 15},
};

static long EECPTestTotal(const EECPNextBuildings* inEntry)
{
    return inEntry != NULL ? inEntry->finished + inEntry->unfinished + inEntry->queued : 0;
}

// the cheapest military unit the barracks make (the Prehistoric's clubman)
static long EECPTestSoldierClass(const EECPNextView& inView)
{
    long theBest = -1, theBestCost = 0;
    for (unsigned long k = 0; k < inView.costs.size(); k++) {
        const EECPNextClassCost& theCost = inView.costs[k];
        if (theCost.building || !theCost.military || theCost.producer != kUnitTypeBarracks)
            continue;
        long theSum = 0;
        for (long i = 0; i < kNextResources; i++)
            theSum += theCost.cost[i];
        if (theBest < 0 || theSum < theBestCost) {
            theBest = theCost.type;
            theBestCost = theSum;
        }
    }
    return theBest;
}

static long EECPTestTowerClass(const EECPNextView& inView)
{
    static const long kTowers[] = {kUnitTypeGuardTowerPaleo, kUnitTypeGuardTowerCopper, kUnitTypeGuardTowerBronze,
        kUnitTypeGuardTowerMiddle};
    for (unsigned long t = sizeof(kTowers) / sizeof(kTowers[0]); t > 0; t--)
        if (EECPNextFindCost(inView, kTowers[t - 1]) != NULL)
            return kTowers[t - 1];
    return -1;
}

// the nearest unclaimed site of a resource in the claim range with no danger (-1: none)
static long EECPTestNearestFree(const EECPNextView& inView, long inResource, bool inPatch, long inRange)
{
    long theBest = -1;
    for (unsigned long k = 0; k < inView.sites.size(); k++) {
        const EECPNextSite& theSite = inView.sites[k];
        if (theSite.claimed || theSite.resource != inResource || theSite.units == 0 || theSite.danger > 0
            || (inPatch && !theSite.patch) || theSite.distance > inRange)
            continue;
        if (theBest < 0 || theSite.distance < inView.sites[theBest].distance)
            theBest = (long)k;
    }
    return theBest;
}

static long EECPTestClaimed(const EECPNextView& inView, long inResource, bool inPatch)
{
    long theCount = 0;
    for (unsigned long k = 0; k < inView.sites.size(); k++) {
        const EECPNextSite& theSite = inView.sites[k];
        if (theSite.claimed && theSite.resource == inResource && theSite.units > 0 && (!inPatch || theSite.patch))
            theCount++;
    }
    return theCount;
}

void EECPNextTestStep(EEComputerPlayer* inPlayer, const EECPNextView& inView)
{
    EECPNextState* theState = EECPNextGetState(inPlayer);
    EECPNextTestMemory& theMemory = theState->mTest;
    theMemory.mStep++;
    if (inView.town_x < 0)
        return;   // no town centre: nothing to build on
    long theAge = inView.epoch < 2 ? inView.epoch : 2;
    const EECPNextClassCost* theCitizen = EECPNextFindCost(inView, kUnitTypeCitizen);

    // saving for the next epoch once the town has its citizens: the reserve holds the advance's cost, and the advance
    // is asked for only when it is paid
    bool theLast = inView.epoch >= inView.last_epoch;
    bool theSaving = !theLast && !inView.advancing && inView.citizens >= kTestCitizens[theAge] - 2;
    long theReserve[kNextResources];
    for (long i = 0; i < kNextResources; i++)
        theReserve[i] = theSaving ? inView.next_epoch_cost[i] : 0;
    if (memcmp(theReserve, inView.reserve, sizeof(theReserve)) != 0)
        EECPNextReserve(inPlayer, theReserve[0], theReserve[1], theReserve[2], theReserve[3], theReserve[4]);
    if (theSaving && inView.sol_missing == 0) {
        bool thePaid = true;
        for (long i = 0; i < kNextResources; i++)
            thePaid = thePaid && inView.stock[i] >= inView.next_epoch_cost[i];
        if (thePaid)
            EECPNextAdvance(inPlayer);
    }

    // citizens: two at a time from the town centres up to the epoch's count
    long theMaking = theCitizen != NULL ? theCitizen->in_production : 0;
    long theWaiting = theCitizen != NULL ? theCitizen->requested : 0;
    long thePlanned = inView.citizens + theMaking + theWaiting;
    if (theCitizen != NULL && theWaiting == 0 && theMaking < 2 * theCitizen->producers
        && thePlanned < kTestCitizens[theAge] && inView.population < inView.population_cap)
        EECPNextTrain(inPlayer, kUnitTypeCitizen, kTestCitizens[theAge] - thePlanned < 2 ? kTestCitizens[theAge] - thePlanned : 2);

    if (inView.time - theMemory.mLastClaim >= (long)kTestClaimMs || theMemory.mLastClaim == 0) {
        theMemory.mLastClaim = inView.time;
        // the gatherers in fixed shares; while saving, the shares follow what the advance still lacks
        long theLand = inView.citizens;
        long theWant[kNextResources] = {0, 0, 0, 0, 0};
        long theLacking[kNextResources] = {0, 0, 0, 0, 0}, theLackSum = 0;
        for (long i = 0; i < kNextResources; i++) {
            theLacking[i] = inView.next_epoch_cost[i] - inView.stock[i];
            if (theLacking[i] < 0)
                theLacking[i] = 0;
            theLackSum += theLacking[i];
        }
        long theOthers = 0;
        for (long i = 1; i < kNextResources; i++) {
            if (theSaving && theLackSum > 0)
                theWant[i] = theLacking[i] > 0 ? (theLand * theLacking[i] + theLackSum - 1) / theLackSum : 0;
            else
                theWant[i] = (theLand * kTestShares[theAge][i] + 50) / 100;
            theOthers += theWant[i];
        }
        theWant[0] = theLand > theOthers ? theLand - theOthers : 0;
        for (long i = 0; i < kNextResources; i++)
            if (theWant[i] != inView.wanted[i])
                EECPNextGather(inPlayer, i, theWant[i]);

        // claims: forage patches with room for the food gatherers (two at least, four at most), and a site of each
        // mineral the gatherers want; the claim grows when no patch is left in it
        long theRange = inView.claim_range;
        long thePatches = EECPTestClaimed(inView, 0, true);
        long thePatchRoom = 0;
        for (unsigned long k = 0; k < inView.sites.size(); k++)
            if (inView.sites[k].claimed && inView.sites[k].patch && inView.sites[k].units > 0)
                thePatchRoom += inView.sites[k].room;
        if (thePatches < 2 || (thePatches < 4 && thePatchRoom < theWant[0] - inView.gatherers[0] + 2)) {
            long theSite = EECPTestNearestFree(inView, 0, true, theRange);
            if (theSite >= 0)
                EECPNextClaim(inPlayer, theSite);
            else if (thePatches < 2 && theRange < kEECPClaimRangeMax)
                EECPNextClaimRange(inPlayer, theRange + kEECPClaimRangeStep);
        }
        for (long r = 2; r < kNextResources; r++) {
            if (theWant[r] == 0 || EECPTestClaimed(inView, r, false) > 0)
                continue;
            long theSite = EECPTestNearestFree(inView, r, false, theRange);
            if (theSite >= 0)
                EECPNextClaim(inPlayer, theSite);
        }

        // a settlement at a worked forage patch with no drop site at hand (one at a time, two at most)
        const EECPNextBuildings* theSettlements = EECPNextFindBuildings(inView, kUnitTypeSettlement);
        long theSettled = EECPTestTotal(theSettlements);
        bool theSettling = theSettlements != NULL && theSettlements->unfinished + theSettlements->queued > 0;
        if (!theSettling && theSettled < 2 && EECPNextFindCost(inView, kUnitTypeSettlement) != NULL)
            for (unsigned long k = 0; k < inView.sites.size(); k++) {
                const EECPNextSite& theSite = inView.sites[k];
                if (theSite.claimed && theSite.patch && !theSite.has_drop && theSite.danger == 0) {
                    EECPNextBuild(inPlayer, kUnitTypeSettlement, kNextNearSite, theSite.index);
                    break;
                }
            }

        // towers at the worked sites, one at a time
        long theTower = EECPTestTowerClass(inView);
        const EECPNextBuildings* theTowers = theTower >= 0 ? EECPNextFindBuildings(inView, theTower) : NULL;
        bool theTowerBusy = theTowers != NULL && theTowers->unfinished + theTowers->queued > 0;
        if (theTower >= 0 && !theTowerBusy && inView.citizens >= 15 && EECPTestTotal(theTowers) < kTestTowers[theAge])
            for (unsigned long k = 0; k < inView.sites.size(); k++) {
                const EECPNextSite& theSite = inView.sites[k];
                if (theSite.claimed && theSite.workers > 0 && !theSite.has_tower) {
                    EECPNextBuild(inPlayer, theTower, kNextNearSite, theSite.index);
                    break;
                }
            }

        // the next epoch's SOL: one building at a time that counts toward it and is not built yet (as the classic
        // ResearchCheapest: no town centres, settlements or farms), the cheapest first
        if (theSaving && inView.sol_missing > 0) {
            const EECPNextClassCost* theBest = NULL;
            long theBestCost = 0;
            bool theBuilding = false;
            for (unsigned long k = 0; k < inView.costs.size(); k++) {
                const EECPNextClassCost& theCost = inView.costs[k];
                if (!theCost.building || !theCost.counts_toward_epoch || theCost.type == kUnitTypeTownCenter
                    || theCost.type == kUnitTypeCapitol || theCost.type == kUnitTypeSettlement || theCost.type == kUnitTypeFarm
                    || theCost.type == kUnitTypeGranary)
                    continue;
                const EECPNextBuildings* theHave = EECPNextFindBuildings(inView, theCost.type);
                if (theHave != NULL && theHave->unfinished + theHave->queued > 0)
                    theBuilding = true;
                if (theHave != NULL && theHave->finished + theHave->unfinished + theHave->queued > 0)
                    continue;
                long theSum = 0;
                for (long i = 0; i < kNextResources; i++)
                    theSum += theCost.cost[i];
                if (theBest == NULL || theSum < theBestCost) {
                    theBest = &theCost;
                    theBestCost = theSum;
                }
            }
            if (theBest != NULL && !theBuilding)
                EECPNextBuild(inPlayer, theBest->type, kNextNearTown);
        }

        // from the Stone Age: a dock and fishing boats
        if (inView.epoch >= 1 && !theSaving)
            EECPNextFish(inPlayer, kTestBoats, 1);
    }

    // a barracks, then the mobile defence
    const EECPNextBuildings* theBarracks = EECPNextFindBuildings(inView, kUnitTypeBarracks);
    if (inView.citizens >= 12 && EECPTestTotal(theBarracks) == 0)
        EECPNextBuild(inPlayer, kUnitTypeBarracks, kNextNearTown);
    long theSoldier = EECPTestSoldierClass(inView);
    if (theBarracks != NULL && theBarracks->finished > 0 && theSoldier >= 0 && !theSaving) {
        const EECPNextClassCost* theCost = EECPNextFindCost(inView, theSoldier);
        long theHave = inView.army_count + (theCost != NULL ? theCost->requested + theCost->in_production : 0);
        if (theHave < kTestSoldiers[theAge])
            EECPNextTrain(inPlayer, theSoldier, kTestSoldiers[theAge] - theHave);
    }
    // the four soldiers guard the town; ten minutes into the Stone Age four of them attack the first enemy seen
    if (!theMemory.mDefended && inView.army_count >= 4) {
        theMemory.mDefended = 1;
        EECPNextDefend(inPlayer, -1, -1, 4);
    }
    if (!theMemory.mAttacked && inView.epoch >= 1 && inView.army_count >= 6
        && inView.time - inView.epoch_start >= (long)kTestAttackMs) {
        long theTarget = -1;
        for (unsigned long k = 0; k < inView.enemies.size() && theTarget < 0; k++)
            theTarget = inView.enemies[k].player;
        if (theTarget > 0) {
            theMemory.mAttacked = 1;
            EECPNextAttack(inPlayer, theTarget, 4);
        }
    }
}
