// The computer player's reflexes (stage 6, docs/AI_LUA_VERBS.md §5): the citizen militia, the danger zones and the
// gatherers' flight from them, the naval flight. They act on the damage events (EEComputerPlayer::DamageTaken) and
// were the original resource manager's (EECPResourceManager.cpp, deleted with the managers); ai.reflex turns them off
// and on for a seat. New in this port: no original address.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/USingleton.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIFOUnitScreeners.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPUnitTileDistanceLess.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEUGLoadContainer.h"
#include "ee/EEUGLoadGarrison.h"
#include "ee/EEUGLoadTransport.h"
#include "ee/EEUGUnloadTransport.h"
#include "ee/UReferenceContainer.h"
#include "ee/EEUCBasicCitizen.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EECPCombatManager.h"
#include "ee/EEWheel.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EECPTrace.h"
#include "ee/EECPNextUnits.h"
#include "ee/EECPNext.h"
#include "ee/EECPPlacement.h"
#include "ee/EECPReflexes.h"

// New (docs/AI_RESEARCH.md §8.22): gatherers run from a harassing raid. Where a human player's unit (a computer
// player's only with the experiment "flee") hits a citizen (EEComputerPlayer::DamageTaken -> EECPRMNoteDanger) is a danger zone for
// kDangerMs: the gatherers within kFleeRange tiles of it go back towards the town centre and onto the idle list, and
// FindResourceTarget sends nobody to a resource within kDangerRange tiles of it until it expires.
static const long kFleeRange = 8;
static const long kDangerRange = 10;
static const unsigned long kDangerMs = 30000;
static const long kHomeRange = 12;
static const long kMaxRaiders = 2;
static const long kDefendRange = 30;
static const unsigned long kDefenders = 4;

static bool EECPRMInDanger(EEComputerPlayer* inPlayer, long inX, long inY, long inRange);

// New (docs/AI_RESEARCH.md §8.25; aipersonalities.yaml military.militia, EE_AI_EXP "nomilitia" turns it off): the town's answer to an attack group on its citizens
// when no army does it. The citizens within kMilitiaRange tiles of the hit one, with the soldiers within
// kDefendRange that are not attacking, fight when their strength (summed hit points x summed attack) is at least
// kMilitiaOdds times the attackers' within kMilitiaRange; else the citizens run to the town centre, or when the
// attack is there to the drop site farthest from it. At most once every kMilitiaMs per player.
static const long kMilitiaRange = 10;
static const long kMilitiaTowerRange = 12;   // military.militia_towers
static const float kMilitiaOdds = 1.5f;
static const unsigned long kMilitiaMs = 4000;

static bool EECPRMIsSoldier(EEUnit* inUnit)
{
    if (inUnit->mComplexUnit == NULL || !(inUnit->mHitPoints > 0) || inUnit->mStats->mAttack == 0.0f
        || inUnit->mUnitClass->IsLandBuilding())
        return false;
    long theCategory = inUnit->mUnitClass->mCategory;
    return theCategory == kUnitCategoryLand || theCategory == kUnitCategoryLandMilitaryHero;
}

static bool EECPRMNear(EEUnit* inUnit, long inX, long inY, long inRange)
{
    long theDX = inUnit->GetTileX() - inX, theDY = inUnit->GetTileY() - inY;
    return theDX * theDX + theDY * theDY <= inRange * inRange;
}

static void EECPRMMilitia(EEComputerPlayer* inPlayer, EEUnit* inCitizen, unsigned long inAttacker)
{
    static unsigned long sLast[64];
    EEComputerPlayer* thePlayer = inPlayer;
    unsigned long theID = thePlayer->mPlayerID < 64 ? thePlayer->mPlayerID : 63;
    if (sLast[theID] != 0 && gPhysics->mGameTime >= sLast[theID] && gPhysics->mGameTime - sLast[theID] < kMilitiaMs)
        return;
    sLast[theID] = gPhysics->mGameTime;
    long theX = inCitizen->GetTileX(), theY = inCitizen->GetTileY();
    std::vector<EEComplexUnit*> theRaiders;
    float theRaiderHP = 0.0f, theRaiderAttack = 0.0f;
    for (EEUnit* theUnit = gWorld.mPlayers[inAttacker]->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        if (EECPRMIsSoldier(theUnit) && EECPRMNear(theUnit, theX, theY, kMilitiaRange)) {
            theRaiders.push_back(theUnit->mComplexUnit);
            theRaiderHP += (float)theUnit->mHitPoints;
            theRaiderAttack += theUnit->mStats->mAttack;
        }
    if (theRaiders.empty())
        return;
    std::vector<EEComplexUnit*> theCitizens, theSoldiers;
    float theHP = 0.0f, theAttack = 0.0f;
    for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        // (ai.reflex{held = true}: the script's held citizens join, lib/verbs.lua)
        bool theHeldCitizen = theComplex != NULL && theUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen
            && EECPUnitsReflexOn(thePlayer, "held") && EECPUnitsHolds(thePlayer, theComplex);
        if (theComplex == NULL || !(theUnit->mHitPoints > 0) || theComplex->mIsDisabled
            || (theComplex->mIsDetachedFromAI && !theHeldCitizen) || theComplex->mContainer != NULL)
            continue;
        if (theUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen && EECPRMNear(theUnit, theX, theY, kMilitiaRange)
            && theComplex->mCitizenBuildQueue.size() == 0) {
            theCitizens.push_back(theComplex);
            theHP += (float)theUnit->mHitPoints;
            theAttack += theUnit->mStats->mAttack;
        } else if (EECPRMIsSoldier(theUnit) && EECPRMNear(theUnit, theX, theY, kDefendRange)
                   && (theComplex->mActiveGoal == NULL || theComplex->mActiveGoal->mKind != kGoalAttackUnit)) {
            theSoldiers.push_back(theComplex);
            theHP += (float)theUnit->mHitPoints;
            theAttack += theUnit->mStats->mAttack;
        }
    }
    // military.militia_towers (docs/AI_RESEARCH.md §8.26; the owner: towers are strong until the Bronze Age, a boom
    // lives behind them): the own built towers within kMilitiaTowerRange tiles count in the defence, and a raid the
    // town cannot fight sends the citizens to the nearest tower within kDefendRange instead of the far drop site
    bool theTowerRule = EECPMilitiaTowersOn(thePlayer);
    EEUnit* theTower = NULL;
    if (theTowerRule) {
        long theBest = -1;
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (theUnit->mComplexUnit == NULL || !theUnit->mUnitClass->mIsTower || !(theUnit->mHitPoints > 0)
                || !theUnit->mComplexUnit->mIsBuilt || theUnit->mStats->mAttack == 0.0f)
                continue;
            if (EECPRMNear(theUnit, theX, theY, kMilitiaTowerRange)) {
                theHP += (float)theUnit->mHitPoints;
                theAttack += theUnit->mStats->mAttack;
            }
            long theDX = theUnit->GetTileX() - inCitizen->GetTileX(), theDY = theUnit->GetTileY() - inCitizen->GetTileY();
            long theDistance = theDX * theDX + theDY * theDY;
            if (theDistance <= kDefendRange * kDefendRange && (theBest < 0 || theDistance < theBest)) {
                theBest = theDistance;
                theTower = theUnit;
            }
        }
    }
    bool theFight = theHP * theAttack >= kMilitiaOdds * theRaiderHP * theRaiderAttack;
    if (theFight) {
        // the soldiers first, then only as many of the nearest citizens as the odds need
        std::vector<EEComplexUnit*> theAll(theSoldiers);
        float theNeedHP = 0.0f, theNeedAttack = 0.0f;
        for (unsigned long i = 0; i < theSoldiers.size(); i++) {
            theNeedHP += (float)theSoldiers[i]->mHitPoints;
            theNeedAttack += theSoldiers[i]->mStats->mAttack;
        }
        std::vector<std::pair<long, EEComplexUnit*> > theByDistance;
        for (unsigned long i = 0; i < theCitizens.size(); i++) {
            long theDX = theCitizens[i]->GetTileX() - theX, theDY = theCitizens[i]->GetTileY() - theY;
            theByDistance.push_back(std::pair<long, EEComplexUnit*>(theDX * theDX + theDY * theDY, theCitizens[i]));
        }
        std::stable_sort(theByDistance.begin(), theByDistance.end(),
            [](const std::pair<long, EEComplexUnit*>& x, const std::pair<long, EEComplexUnit*>& y) { return x.first < y.first; });
        theCitizens.clear();
        for (unsigned long i = 0; i < theByDistance.size()
             && theNeedHP * theNeedAttack < kMilitiaOdds * theRaiderHP * theRaiderAttack; i++) {
            theCitizens.push_back(theByDistance[i].second);
            theAll.push_back(theByDistance[i].second);
            theNeedHP += (float)theByDistance[i].second->mHitPoints;
            theNeedAttack += theByDistance[i].second->mStats->mAttack;
        }
        for (unsigned long i = 0; i < theAll.size(); i++) {
            EEComplexUnit* theUnit = theAll[i];
            EEComplexUnit* theTarget = NULL;
            long theBest = 0;
            for (unsigned long r = 0; r < theRaiders.size(); r++) {
                long theDX = theRaiders[r]->GetTileX() - theUnit->GetTileX(), theDY = theRaiders[r]->GetTileY() - theUnit->GetTileY();
                long theDistance = theDX * theDX + theDY * theDY;
                if (theTarget == NULL || theDistance < theBest) {
                    theTarget = theRaiders[r];
                    theBest = theDistance;
                }
            }
            theUnit->Reference();
            theUnit->ClearGoalQueue(0);
            theUnit->SetGoal(new EEUGAttackUnit(theUnit, theTarget, false, false), 0, 0);
            theUnit->StartAction(kActionPrepareToMove);
            theUnit->GetUnitClass()->ActionChanged(theUnit);
            theUnit->Dereference();
        }
    } else {
        EECPPlacement* theBuildManager = inPlayer->mPlacement;
        bool theHaveHome = theBuildManager != NULL && theBuildManager->mTownCenterTile.mX != -10000;
        GE2DPoint theHome(0.0f, 0.0f);
        bool theRun = false;
        if (theTower != NULL && !EECPRMNear(theTower, theX, theY, 3)) {
            theHome = GE2DPoint((float)theTower->GetTileX(), (float)theTower->GetTileY());
            theRun = true;
        } else if (theHaveHome && !EECPRMNear(inCitizen, theBuildManager->mTownCenterTile.mX, theBuildManager->mTownCenterTile.mY, kHomeRange)) {
            theHome = GE2DPoint((float)theBuildManager->mTownCenterTile.mX, (float)theBuildManager->mTownCenterTile.mY);
            theRun = true;
        } else {
            // the attack is at the town centre: to the drop site farthest from it
            EEComplexUnit* theRefuge = NULL;
            long theFarthest = -1;
            for (unsigned long c = 0; c < thePlayer->GetNumResourceCenters(); c++) {
                EECPResourceCenter* theCenter = thePlayer->GetResourceCenter(c);
                if (theCenter == NULL || theCenter->mUnit == NULL || !theCenter->mUnit->mUnitClass->IsLandBuilding()
                    || !(theCenter->mUnit->mHitPoints > 0))
                    continue;
                long theDX = theCenter->mUnit->GetTileX() - theX, theDY = theCenter->mUnit->GetTileY() - theY;
                long theDistance = theDX * theDX + theDY * theDY;
                if (theDistance > theFarthest) {
                    theFarthest = theDistance;
                    theRefuge = theCenter->mUnit;
                }
            }
            if (theRefuge != NULL && theFarthest > kMilitiaRange * kMilitiaRange) {
                theHome = GE2DPoint((float)theRefuge->GetTileX(), (float)theRefuge->GetTileY());
                theRun = true;
            }
        }
        if (!theRun)
            theCitizens.clear();   // nowhere to run: they keep working
        for (unsigned long i = 0; i < theCitizens.size(); i++) {
            EEComplexUnit* theUnit = theCitizens[i];
            theUnit->Reference();
            theUnit->ClearGoalQueue(0);
            theUnit->SetGoal(new EEUGLocation(theHome, true, true, false, 0.0f, false, false), 0, 0);
            theUnit->StartAction(kActionPrepareToMove);
            theUnit->GetUnitClass()->ActionChanged(theUnit);
            theUnit->Dereference();
        }
        if (!theCitizens.empty())
            inPlayer->mDangerZones.push_back(std::pair<U2DPoint<long>, unsigned long>(U2DPoint<long>(theX, theY), gPhysics->mGameTime));
    }
    EE_CP_TRACE(thePlayer, "militia", "%s citizens=%lu soldiers=%lu raiders=%lu odds=%.2f tower=%d", theFight ? "fight" : "run",
                (unsigned long)theCitizens.size(), (unsigned long)theSoldiers.size(), (unsigned long)theRaiders.size(),
                theRaiderHP * theRaiderAttack > 0.0f ? theHP * theAttack / (theRaiderHP * theRaiderAttack) : 0.0f, theTower != NULL ? 1 : 0);
}

static const long kNavalFleeRange = 12;      // the own units round the victim that leave with it
static const unsigned long kNavalFleeMs = 4000;

void EECPRMNavalFlee(EEComputerPlayer* inPlayer, EEUnit* inVictim, EEComplexUnit* inAttacker)
{
    static unsigned long sLast[64];
    EEComputerPlayer* thePlayer = inPlayer;
    if (inAttacker == NULL || !(inAttacker->mHitPoints > 0))
        return;
    long theAttackerCategory = inAttacker->mUnitClass->mCategory;
    if (theAttackerCategory != kUnitCategoryNaval)
        return;   // warships only (not boats, not land units on the shore)
    unsigned long theID = thePlayer->mPlayerID < 64 ? thePlayer->mPlayerID : 63;
    if (sLast[theID] != 0 && gPhysics->mGameTime >= sLast[theID] && gPhysics->mGameTime - sLast[theID] < kNavalFleeMs)
        return;
    sLast[theID] = gPhysics->mGameTime;
    long theX = inVictim->GetTileX(), theY = inVictim->GetTileY();
    bool theBoat = inVictim->mUnitClass->mCategory == kUnitCategoryNavalCitizen;
    GE2DPoint theRefuge(0.0f, 0.0f);
    if (theBoat) {
        // the own built dock farthest from the attacker (the nearest one is often where it shoots)
        std::vector<EEUnit*> theDocks;
        EEGetPlayerUnitsOfType(kUnitTypeDock, thePlayer->mPlayerID, theDocks);
        long theBest = -1;
        for (unsigned long k = 0; k < theDocks.size(); k++) {
            if (theDocks[k]->mComplexUnit == NULL || !theDocks[k]->mComplexUnit->mIsBuilt || !(theDocks[k]->mHitPoints > 0))
                continue;
            long theDX = theDocks[k]->GetTileX() - inAttacker->GetTileX(), theDY = theDocks[k]->GetTileY() - inAttacker->GetTileY();
            if (theDX * theDX + theDY * theDY > theBest) {
                theBest = theDX * theDX + theDY * theDY;
                theRefuge = GE2DPoint((float)theDocks[k]->GetTileX(), (float)theDocks[k]->GetTileY());
            }
        }
        if (theBest < 0)
            return;
    } else {
        EECPPlacement* theBuild = inPlayer->mPlacement;
        if (theBuild == NULL || theBuild->mTownCenterTile.mX == -10000)
            return;
        theRefuge = GE2DPoint((float)theBuild->mTownCenterTile.mX, (float)theBuild->mTownCenterTile.mY);
    }
    unsigned long theCount = 0;
    for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        if (theComplex == NULL || !(theUnit->mHitPoints > 0) || theComplex->mIsDisabled || theComplex->mIsDetachedFromAI
            || theComplex->mContainer != NULL || !EECPRMNear(theUnit, theX, theY, kNavalFleeRange))
            continue;
        long theCategory = theUnit->mUnitClass->mCategory;
        bool theLeaves;
        if (theBoat)
            theLeaves = theCategory == kUnitCategoryNavalCitizen;
        else   // the land units that cannot answer a ship: citizens and melee soldiers
            theLeaves = theCategory == kUnitCategoryLandCitizen
                || ((theCategory == kUnitCategoryLand || theCategory == kUnitCategoryLandMilitaryHero) && theUnit->mStats->mRange < 3.0f);
        if (!theLeaves)
            continue;
        theComplex->Reference();
        theComplex->ClearGoalQueue(0);
        theComplex->SetGoal(new EEUGLocation(theRefuge, true, true, false, 0.0f, false, false), 0, 0);
        theComplex->StartAction(kActionPrepareToMove);
        theComplex->GetUnitClass()->ActionChanged(theComplex);
        theComplex->Dereference();
        theCount++;
    }
    if (theCount > 0)
        inPlayer->mDangerZones.push_back(std::pair<U2DPoint<long>, unsigned long>(U2DPoint<long>(theX, theY), gPhysics->mGameTime));
    EE_CP_TRACE(thePlayer, "navalflee", "%s units=%lu", theBoat ? "boats" : "land", theCount);
}

void EECPRMNoteDanger(EEComputerPlayer* inPlayer, EEUnit* inCitizen, unsigned long inAttacker)
{
    // a harassing raid only (kMaxRaiders soldiers of the attacker's within kDangerRange): against an attack group
    // running costs more work than it saves lives (docs/AI_RESEARCH.md §8.22)
    if (inAttacker == 0 || inAttacker >= gWorld.mNumPlayers || gWorld.mPlayers[inAttacker] == NULL)
        return;
    if (EECPMilitiaOn(inPlayer) && EECPUnitsReflexOn(inPlayer, "militia")) {
        EECPRMMilitia(inPlayer, inCitizen, inAttacker);   // (ai.reflex{militia = false}: a script's own militia instead)
        return;
    }
    if (!gWorld.mPlayers[inAttacker]->IsHuman() && !EECPExperiment(inPlayer, "flee"))
        return;
    long theRaiders = 0;
    for (EEUnit* theUnit = gWorld.mPlayers[inAttacker]->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mComplexUnit == NULL || !(theUnit->mHitPoints > 0) || theUnit->mStats->mAttack == 0.0f
            || theUnit->mUnitClass->IsLandBuilding())
            continue;
        long theCategory = theUnit->mUnitClass->mCategory;
        if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen || theCategory == kUnitCategoryAmbient
            || theCategory == kUnitCategoryResource || theCategory == kUnitCategoryProjectile)
            continue;
        long theDX = theUnit->GetTileX() - inCitizen->GetTileX(), theDY = theUnit->GetTileY() - inCitizen->GetTileY();
        if (theDX * theDX + theDY * theDY <= kDangerRange * kDangerRange && ++theRaiders > kMaxRaiders)
            return;
    }
    EECPPlacement* theBuildManager = inPlayer->mPlacement;
    if (theBuildManager != NULL && theBuildManager->mTownCenterTile.mX != -10000) {
        long theDX = inCitizen->GetTileX() - theBuildManager->mTownCenterTile.mX;
        long theDY = inCitizen->GetTileY() - theBuildManager->mTownCenterTile.mY;
        if (theDX * theDX + theDY * theDY < kHomeRange * kHomeRange)
            return;   // at home: the town's defence answers, the gatherers keep working
    }
    bool theNew = !EECPRMInDanger(inPlayer, inCitizen->GetTileX(), inCitizen->GetTileY(), kDangerRange);
    inPlayer->mDangerZones.push_back(std::pair<U2DPoint<long>, unsigned long>(
        U2DPoint<long>(inCitizen->GetTileX(), inCitizen->GetTileY()), gPhysics->mGameTime));
    // a new zone: up to kDefenders of the nearest soldiers within kDefendRange tiles that are not attacking already
    // go for the raider (the town defence plans its aid by town threat, on a timer: a raider on an outer patch went
    // unanswered for minutes)
    EEComplexUnit* theRaider = inCitizen->mComplexUnit != NULL ? inCitizen->mComplexUnit->mLastAttacker : NULL;
    if (!theNew || theRaider == NULL || !(theRaider->mHitPoints > 0))
        return;
    EEComputerPlayer* thePlayer = inPlayer;
    std::vector<std::pair<long, EEComplexUnit*> > theNear;
    for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        if (theComplex == NULL || !(theUnit->mHitPoints > 0) || theUnit->mStats->mAttack == 0.0f
            || theUnit->mUnitClass->IsLandBuilding() || theComplex->mIsDisabled || theComplex->mIsDetachedFromAI)
            continue;
        long theCategory = theUnit->mUnitClass->mCategory;
        if (theCategory != kUnitCategoryLand && theCategory != kUnitCategoryLandMilitaryHero)
            continue;
        if (theComplex->mActiveGoal != NULL && theComplex->mActiveGoal->mKind == kGoalAttackUnit)
            continue;
        long theDX = theUnit->GetTileX() - theRaider->GetTileX(), theDY = theUnit->GetTileY() - theRaider->GetTileY();
        long theDistance = theDX * theDX + theDY * theDY;
        if (theDistance <= kDefendRange * kDefendRange)
            theNear.push_back(std::pair<long, EEComplexUnit*>(theDistance, theComplex));
    }
    std::stable_sort(theNear.begin(), theNear.end(),
                     [](const std::pair<long, EEComplexUnit*>& a, const std::pair<long, EEComplexUnit*>& b) { return a.first < b.first; });
    unsigned long theSent = 0;
    for (unsigned long k = 0; k < theNear.size() && theSent < kDefenders; k++, theSent++) {
        EEComplexUnit* theUnit = theNear[k].second;
        theUnit->ClearGoalQueue(0);
        theUnit->SetGoal(new EEUGAttackUnit(theUnit, theRaider, false, false), 0, 0);
        theUnit->StartAction(kActionPrepareToMove);
        theUnit->GetUnitClass()->ActionChanged(theUnit);
    }
    EE_CP_TRACE(thePlayer, "raid.defend", "sent=%lu near=%lu raider=%lu", theSent, (unsigned long)theNear.size(),
                (unsigned long)theRaider->GetUnitID());
}

static bool EECPRMInDanger(EEComputerPlayer* inPlayer, long inX, long inY, long inRange)
{
    for (unsigned long z = 0; z < inPlayer->mDangerZones.size(); z++) {
        long theDX = inPlayer->mDangerZones[z].first.mX - inX, theDY = inPlayer->mDangerZones[z].first.mY - inY;
        if (theDX * theDX + theDY * theDY <= inRange * inRange)
            return true;
    }
    return false;
}

bool EECPRMResourceInDanger(EEComputerPlayer* inPlayer, long inX, long inY)
{
    return EECPRMInDanger(inPlayer, inX, inY, kDangerRange);
}

void EECPRMFleeRaids(EEComputerPlayer* inPlayer)
{
    EEComputerPlayer* thePlayer = inPlayer;
    EECPPlacement* theBuildManager = inPlayer->mPlacement;
    std::vector<std::pair<U2DPoint<long>, unsigned long> >& theZones = inPlayer->mDangerZones;
    for (unsigned long z = 0; z < theZones.size();)
        if (gPhysics->mGameTime - theZones[z].second > kDangerMs)
            theZones.erase(theZones.begin() + z);
        else
            z++;
    if (theZones.empty() || theBuildManager == NULL || theBuildManager->mTownCenterTile.mX == -10000)
        return;
    GE2DPoint theHome((float)theBuildManager->mTownCenterTile.mX, (float)theBuildManager->mTownCenterTile.mY);
    unsigned long theFled = 0;
    std::vector<EEComplexUnit*> theRun;
    bool theHeld = EECPUnitsReflexOn(thePlayer, "held");
    for (EEUnit* theCitizen = thePlayer->mFirstUnit; theCitizen != NULL; theCitizen = static_cast<EEUnit*>(theCitizen->mNext)) {
        EEComplexUnit* theUnit = theCitizen->mComplexUnit;
        // (the runners' gatherers: the land citizens at work, a script's held ones with ai.reflex{held = true})
        if (theUnit == NULL || theUnit->GetUnitClass()->mCategory != kUnitCategoryLandCitizen || theUnit->mIsDisabled
            || !(theUnit->mHitPoints > 0) || dynamic_cast<EEUGGather*>(theUnit->GetLoadGoal()) == NULL
            || (theUnit->mIsDetachedFromAI && !(theHeld && EECPUnitsHolds(thePlayer, theUnit))))
            continue;
        if (EECPRMInDanger(inPlayer, theUnit->GetTileX(), theUnit->GetTileY(), kFleeRange))
            theRun.push_back(theUnit);
    }
    for (unsigned long k = 0; k < theRun.size(); k++) {
        EEComplexUnit* theUnit = theRun[k];
        theUnit->Reference();
        theUnit->ClearGoalQueue(0);
        theUnit->SetGoal(new EEUGLocation(theHome, true, true, false, 0.0f, false, false), 0, 0);
        theUnit->StartAction(kActionPrepareToMove);
        theUnit->GetUnitClass()->ActionChanged(theUnit);
        theUnit->Dereference();
        theFled++;
    }
    if (theFled > 0)
        EE_CP_TRACE(thePlayer, "flee", "n=%lu zones=%lu", theFled, (unsigned long)theZones.size());
}

