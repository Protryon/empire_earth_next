// C:\EEX\Empire Earth\AI Thread\EECPIntelligence.cpp (file name inferred: no __FILE__; RTTI classes
// EECPIntelligencePlayerFile (vtable 0x43c214), EEAIFOIntelligenceAIFamilyAndUnitTypeSorter (0x43c21c) and
// EECPIntelligence (0x43c224)). 0x80c69..0x84d8d: EECPILocation.cpp ends at 0x80c69; EECPMilitaryAidPackage's ctor
// starts at 0x84d8d. The player file's members first, then the intelligence's, then the map/vector/sort COMDATs
// (0x81ee2..0x84d8d).
#include <windows.h>
#include <vector>
#include <set>
#include <map>
#include <math.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/LowLevelEngineCore.h"
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
#include "lle/FSRestorable.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGGather.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EETeam.h"
#include "ee/EEVictoryCondition.h"
#include "ee/EEVCWonder.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EETown.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EECPIArmy.h"
#include "ee/EECPILocation.h"
#include "ee/EEDeleteVectorContents.h"
#include "ee/EECPIntelligence.h"

// The orders of GetTargets' (unit, score) list (sort/unique COMDATs 0x82b7a/0x82c3a/0x82ca6; empty predicates: they
// get a dword of garbage). Their operator()s are out of line: 0x83e53 and 0x83491 here, the score order's ICF-folded
// into a twin at 0x2b0ba. (provisional names)
struct EECPITargetRegionLess {
    bool operator()(const std::pair<EEComplexUnit*, long>& inFirst, const std::pair<EEComplexUnit*, long>& inSecond);
};

struct EECPITargetRegionEqual {
    bool operator()(const std::pair<EEComplexUnit*, long>& inFirst, const std::pair<EEComplexUnit*, long>& inSecond);
};

// EECPITargetScoreGreater: ee/EECPIntelligence.h
//
//

// mFamilies' order in Update: the most common family first (inline: _Unguarded_insert 0x83878 compares mCount)
struct EECPIFamilyCountGreater {
    bool operator()(const EECPIFamilyCount& inFirst, const EECPIFamilyCount& inSecond) const
    {
        return inFirst.mCount > inSecond.mCount;
    }
};

// The resource units by id (0x80f9b sorts them, then std::unique drops the duplicates; its sort family 0x82a43.. expands
// the compare: unsigned +0x24). (provisional name)
struct EECPIUnitIDLess {
    bool operator()(EEUnit* inFirst, EEUnit* inSecond) const { return inFirst->GetUnitID() < inSecond->GetUnitID(); }
};

// FUNCTION: EXE 0x480c69
// Creates the intelligence file one player keeps on another player, empty and due for an update
EECPIntelligencePlayerFile::EECPIntelligencePlayerFile(EEPlayer* inOwner, EEPlayer* inPlayer)
    : mOwner(inOwner)
{
    this->mPlayer = inPlayer;
    this->mL64 = 0;
    this->mL6C = 0;
    this->mL68 = 0;
    this->mNextUpdate = 0;
    this->Reset();
}

// SYNTHETIC: EXE 0x480cd5
// (the scalar deleting destructor)

// FUNCTION: EXE 0x480cf1
// Destroys the file, deleting its armies
EECPIntelligencePlayerFile::~EECPIntelligencePlayerFile()
{
    this->DeleteArmies();
}

// FUNCTION: EXE 0x480d19
// Adds a mobile land combat unit to the army it is near, starts a new army if none, or merges all near armies
// into one (picked by comparing tile counts against strengths) and adds it there
void EECPIntelligencePlayerFile::AddToArmies(EEComplexUnit* inUnit)
{
    long i;
    long j;
    long k;
    std::vector<EECPIArmy*> theNear;
    EECPIArmy* theArmy;
    EEUnitClass* theClass = inUnit->GetUnitClass();
    if ((!(theClass->mDomains & kMovementLand)) || theClass->mCategory == kUnitCategoryLandCitizen || theClass->mCategory == kUnitCategoryNavalCitizen || theClass->mAttack == 0
        || theClass->mMaxVelocity == 0.0f)
        return;
    for (i = this->mArmies.size() - 1; i >= 0; i--) {
        if (this->mArmies[i]->IsNear(inUnit))
            theNear.push_back(this->mArmies[i]);
    }
    if (theNear.size() == 0) {
        theArmy = new EECPIArmy;
        theArmy->AddUnit(inUnit);
        this->mArmies.push_back(theArmy);
    } else if (theNear.size() == 1) {
        theArmy = theNear[0];
        theArmy->AddUnit(inUnit);
    } else {
        EECPIArmy* theBiggest = theNear.back();
        long theMax = theBiggest->mStrength;
        for (j = theNear.size() - 2; j >= 0; j--) {
            if (theNear[j]->mStrength > theMax) {   // strength with strength (was the tile count)
                theBiggest = theNear[j];
                theMax = theBiggest->mStrength;
            }
        }
        for (k = theNear.size() - 1; k >= 0; k--) {
            if (theNear[k] == theBiggest)
                continue;
            theBiggest->Merge(theNear[k]);
            std::vector<EECPIArmy*>::iterator theIt = std::find(this->mArmies.begin(), this->mArmies.end(), theNear[k]);
            if (theIt != this->mArmies.end()) {
                theArmy = *theIt;
                this->mArmies.erase(theIt);
                delete theArmy;
            }
            theNear[k] = NULL;
        }
        theBiggest->AddUnit(inUnit);
    }
}

// FUNCTION: EXE 0x480f9b
// Every 30 s (or when forced) recounts the player's living units by family, type and domain, their bounds,
// attack, gathered resources, wonder-victory progress and weighted strength; rebuilds armies if asked
void EECPIntelligencePlayerFile::Update(EEComputerPlayer* inPlayer, bool inForce, bool inArmies)
{
    EEUnit* theUnit;
    EEUnit* theNext;
    EEComplexUnit* theComplex;
    EEComplexUnit* theContainer;
    EEUnit* theResource;
    unsigned long t;
    unsigned long theValue;
    long thePercent;
    long r;
    long a;
    std::vector<EEUnit*> theResources;
    EETileCoordinate theTile(0, 0);
    if (!inForce && !(gPhysics->mGameTime > this->mNextUpdate))
        return;
    this->Reset();
    this->mTotalAttack = 0;
    theUnit = this->mPlayer->mFirstUnit;
    if (theUnit != NULL) {
        this->mBounds.mLeft = theUnit->mTileX;
        this->mBounds.mRight = theUnit->mTileX;
        this->mBounds.mTop = theUnit->mTileY;
        this->mBounds.mBottom = theUnit->mTileY;
    }
    while (theUnit != NULL) {
        theNext = static_cast<EEUnit*>(theUnit->mNext);
        if (theUnit->mHitPoints > 0) {
            this->AddUnitClass(theUnit);
            this->mTotalAttack += (long)theUnit->mStats->mAttack;
            theComplex = theUnit->mComplexUnit;
            if (theComplex != NULL) {
                if (inArmies)
                    this->AddToArmies(theComplex);
                if (theComplex->mContainer == NULL) {
                    if (theUnit->mTileX != -10000)
                        theTile = theUnit->GetTile();
                    else {
                        theTile.mY = UMFloor(theUnit->mComplexUnit->mLastMapPosition.mY);
                        theTile.mX = UMFloor(theUnit->mComplexUnit->mLastMapPosition.mX);
                    }
                } else {
                    theContainer = theUnit->mComplexUnit->mContainer;
                    if (theContainer->mTileX != -10000)
                        theTile = theContainer->GetTile();
                    else {
                        theTile.mY = UMFloor(theUnit->mComplexUnit->mContainer->mLastMapPosition.mY);
                        theTile.mX = UMFloor(theUnit->mComplexUnit->mContainer->mLastMapPosition.mX);
                    }
                }
                this->mBounds.Extend(theTile);
                if (theUnit->GetUnitClass()->mCategory == kUnitCategoryLandCitizen && theComplex->mGoalState != NULL
                    && theComplex->mGoalState->mKind == kGoalGather) {
                    theResource = dynamic_cast<EEUGGather*>(theComplex->mGoalState)->mResource;
                    if (theResource != NULL)
                        theResources.push_back(theResource);
                }
            }
        }
        theUnit = theNext;
    }
    std::sort(this->mFamilies.begin(), this->mFamilies.end(), EECPIFamilyCountGreater());
    std::sort(this->mUnitTypes.begin(), this->mUnitTypes.end(), EEAIFOIntelligenceAIFamilyAndUnitTypeSorter(this));
    this->mVictoryPercent = 0;
    for (t = 0; t < gWorld.GetNumTeams(); t++) {
        if (!gWorld.mTeams[t].ContainsPlayer(this->mPlayer->GetPlayerID()) || gWorld.mTeams[t].mVictoryCondition == NULL
            || !gWorld.mTeams[t].mVictoryCondition->GetWonderCountdown(theValue))
            continue;
        thePercent = (long)((float)theValue / gWorldTable.GetLongValue(0x22) * 100.0f);
        if (thePercent > this->mVictoryPercent)
            this->mVictoryPercent = thePercent;
    }
    std::sort(theResources.begin(), theResources.end(), EECPIUnitIDLess());
    theResources.erase(std::unique(theResources.begin(), theResources.end()), theResources.end());
    for (r = theResources.size() - 1; r >= 0; r--)
        this->mResources[theResources[r]->mResourceType]++;
    this->mStrength = (this->mNumSpaceUnits + this->mNumLandUnits + this->mNumAirUnits + this->mNumSeaUnits) * inPlayer->GetMilitaryCountWeight() + inPlayer->GetWorkerCountWeight() * this->mNumGatherers;
    this->mNextUpdate = gPhysics->mGameTime + 30000;
    for (a = this->mArmies.size() - 1; a >= 0; a--)
        this->mArmies[a]->EEAIArea::UpdateHull();
}

// SYNTHETIC: EXE 0x48141b
// (EEAIFOIntelligenceAIFamilyAndUnitTypeSorter's scalar deleting destructor)

// FUNCTION: EXE 0x481438
// Deletes every army in the file
void EECPIntelligencePlayerFile::DeleteArmies()
{
    DeleteVectorContents(this->mArmies);
}

// FUNCTION: EXE 0x481443
// Clears all counts, strengths, bounds, family and type lists and resource tallies, and deletes the armies
void EECPIntelligencePlayerFile::Reset()
{
    this->mFamilies.clear();
    this->mAirStrength = 0;
    this->mNumAirUnits = 0;
    this->mBounds.Clear();
    this->mStrength = 0.0f;
    this->mLandStrength = 0;
    this->mNumLandUnits = 0;
    this->mNumBuildings = 0;
    this->mL8C = 0;
    this->mTotalAttack = 0;
    this->mSeaStrength = 0;
    this->mNumSeaUnits = 0;
    this->mSpaceStrength = 0;
    this->mNumSpaceUnits = 0;
    this->mLB0 = 0;
    this->mUnitTypes.clear();
    this->mNumGatherers = 0;
    this->mVictoryPercent = 0;
    this->DeleteArmies();
    for (long i = 0; i < 7; i++)
        this->mResources[i] = 0;
}

// FUNCTION: EXE 0x4814bd
// Returns the value times 2.5
// The const 0x43c074; callers EECPASGAir 0x441dbd and EECPAttackSubGroup::TakeRequestedUnits (provisional name)
float EECPIntelligencePlayerFile::GetScanRange(float inValue)
{
    return inValue * 2.5f;
}

// FUNCTION: EXE 0x4814ca
// Returns how many of the player's units belong to the AI family (0 if none)
long EECPIntelligencePlayerFile::GetFamilyCount(long inFamily)
{
    long theResult = 0;
    const std::vector<EECPIFamilyCount>& theFamilies = this->mFamilies;
    for (long i = this->mFamilies.size() - 1; i >= 0; i--) {
        if (theFamilies[i].mFamily == inFamily) {
            theResult = theFamilies[i].mCount;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x481500
// Lists the player's living units on the map that can move on water (or land) terrain, optionally only those
// on the given continent
void EECPIntelligencePlayerFile::GetUnits(std::vector<EEComplexUnit*>& outUnits, unsigned short inContinent, bool inWater)
{
    EEUnit* theUnit = this->mPlayer->mFirstUnit;
    EEMap* theMap = gWorld.mMap;
    long theKind = inWater ? 1 : 12;
    outUnits.clear();
    while (theUnit != NULL) {
        EEUnit* theNext = static_cast<EEUnit*>(theUnit->mNext);
        if (theUnit->GetComplexUnit() != NULL && theUnit->mHitPoints > 0
            && theUnit->GetUnitClass()->CanMoveOnTerrain(static_cast<EEComplexUnit*>(theUnit), theKind)
            && theMap->GetTile(theUnit->GetTileX(), theUnit->mTileY) != NULL
            && (inContinent == 0 || inContinent == gContinentManager.GetContinentID(theUnit->GetTileX(), theUnit->mTileY)))
            outUnits.push_back(theUnit->GetComplexUnit());
        theUnit = theNext;
    }
}

// FUNCTION: EXE 0x4815bc
// Lists the player's living units on the map, optionally only those on the given continent
void EECPIntelligencePlayerFile::GetAllUnits(unsigned short inContinent, std::vector<EEComplexUnit*>& outUnits)
{
    EEUnit* theUnit = this->mPlayer->mFirstUnit;
    EEMap* theMap = gWorld.mMap;
    outUnits.clear();
    while (theUnit != NULL) {
        EEUnit* theNext = static_cast<EEUnit*>(theUnit->mNext);
        if (theUnit->GetComplexUnit() != NULL && theUnit->mHitPoints > 0
            && theMap->GetTile(theUnit->GetTileX(), theUnit->mTileY) != NULL
            && (inContinent == 0 || inContinent == gContinentManager.GetContinentID(theUnit->GetTileX(), theUnit->mTileY)))
            outUnits.push_back(theUnit->GetComplexUnit());
        theUnit = theNext;
    }
}

// FUNCTION: EXE 0x481654
// Lists attack locations against the player: one building (or, with none, one unit) per influence region,
// best first by enemy influence, plus the town value on our continent when the towns are on different ones
void EECPIntelligencePlayerFile::GetTargets(std::vector<EECPILocation>& outLocations, long inArg)
{
    std::vector<EEComplexUnit*> theUnits;
    std::vector<std::pair<EEComplexUnit*, long> > theScores;
    std::vector<EECPILocation> theLocations;
    EECPILocation theLocation;
    EEUnit* theUnit = this->mPlayer->mFirstUnit;
    bool theArg1;
    bool theArg2;
    EETown* theTown = this->mPlayer->GetLargestTown(&theArg1);
    EETown* theOwnTown = this->mOwner->GetLargestTown(&theArg2);
    bool theDifferentContinent;
    if (theTown != NULL && theOwnTown != NULL && theTown->mContinent != theOwnTown->mContinent)
        theDifferentContinent = true;
    else
        theDifferentContinent = false;
    while (theUnit != NULL) {
        EEUnit* theNext = static_cast<EEUnit*>(theUnit->mNext);
        if (theUnit->GetComplexUnit() != NULL && theUnit->mTileX != -10000) {
            if (theUnit->GetUnitClass()->IsLandBuilding())
                theScores.push_back(std::make_pair(theUnit->GetComplexUnit(), 0));
            if (theScores.empty())
                theUnits.push_back(theUnit->GetComplexUnit());
        }
        theUnit = theNext;
    }
    if (theScores.empty()) {
        for (long i = theUnits.size() - 1; i >= 0; i--)
            theScores.push_back(std::make_pair(theUnits[i], 0));
    }
    std::sort(theScores.begin(), theScores.end(), EECPITargetRegionLess());
    std::vector<std::pair<EEComplexUnit*, long> >::iterator theEnd =
        std::unique(theScores.begin(), theScores.end(), EECPITargetRegionEqual());
    if (theEnd != theScores.end())
        theScores.erase(theEnd, theScores.end());
    if (theScores.size() != 0) {
        for (long j = theScores.size() - 1; j >= 0; j--) {
            long theValue = this->GetInfluence(theScores[j].first->mTileX, theScores[j].first->mTileY, 2);
            if (theDifferentContinent
                && gContinentManager.GetContinentID(theScores[j].first->mTileX, theScores[j].first->mTileY)
                       == theOwnTown->mContinent
                && theTown->mValue > 0)
                theValue += theTown->mValue;
            theScores[j].second = theValue;
        }
        std::sort(theScores.begin(), theScores.end(), EECPITargetScoreGreater());
        for (long k = 0; k < (long)theScores.size(); k++) {
            theLocation.SetUnit(theScores[k].first);
            outLocations.push_back(theLocation);
        }
    }
}

// FUNCTION: EXE 0x4819c0
// Sums the player's influence over the region holding the tile and the regions within the radius around it
long EECPIntelligencePlayerFile::GetInfluence(long inX, long inY, long inRadius)
{
    std::vector<U2DPoint<long> > theRegions;
    EEInfluenceTable* theTable = gWorld.mInfluenceTable;
    unsigned long thePlayerID = this->mPlayer->mPlayerID;
    long theSum = 0;
    long theX = inX / theTable->mRegionSize;
    long theY = inY / theTable->mRegionSize;
    Fn2492C2(theX, theY, inRadius, theRegions);
    theRegions.push_back(U2DPoint<long>(theX, theY));
    for (long i = theRegions.size() - 1; i >= 0; i--)
        theSum += theTable->GetValue(thePlayerID, theRegions[i].GetX(), theRegions[i].GetY());
    return theSum;
}

// FUNCTION: EXE 0x481aaa
// Counts the unit under its AI family, adding the family if new
void EECPIntelligencePlayerFile::AddFamily(EEUnit* inUnit)
{
    long theFamily = inUnit->GetUnitClass()->mAIKind;
    for (long i = this->mFamilies.size() - 1; i >= 0; i--) {
        if (this->mFamilies[i].mFamily == theFamily) {
            this->mFamilies[i].mCount++;
            return;
        }
    }
    EECPIFamilyCount theEntry;
    theEntry.mFamily = theFamily;
    theEntry.mCount = 1;
    this->mFamilies.push_back(theEntry);
}

// FUNCTION: EXE 0x481b0e
// Counts the unit under its unit type; a combat-capable non-citizen also adds its influence to the air, sea,
// space or land strength and count
void EECPIntelligencePlayerFile::AddUnitType(EEUnit* inUnit)
{
    EEUnitClass* theClass = inUnit->GetUnitClass();
    long theFamily = theClass->mAIKind;
    unsigned long theType = theClass->mUnitType;
    long i;
    for (i = this->mUnitTypes.size() - 1; i >= 0; i--) {
        if (this->mUnitTypes[i].mUnitType == theType)
            break;
    }
    if (i >= 0)
        this->mUnitTypes[i].mCount++;
    else {
        EECPIUnitTypeCount theCount;
        theCount.mCount = 1;
        theCount.mUnitType = theType;
        theCount.mFamily = theFamily;
        this->mUnitTypes.push_back(theCount);
    }
    if (theClass->mCategory == kUnitCategoryLandCitizen || theClass->mCategory == kUnitCategoryNavalCitizen
        || (!(theClass->mAttack > 0) && !theClass->mHasCalamityPowers && !theClass->mIsMedic && theClass->mConversionRate == 0.0f))
        return;
    if (theClass->mDomains & kMovementAir) {
        this->mAirStrength += theClass->mInfluence;
        this->mNumAirUnits++;
    } else if (theClass->mDomains & kMovementSea) {
        this->mSeaStrength += theClass->mInfluence;
        this->mNumSeaUnits++;
    } else if (theClass->mDomains & kMovementSpace) {
        this->mSpaceStrength += theClass->mInfluence;
        this->mNumSpaceUnits++;
    } else {
        this->mLandStrength += theClass->mInfluence;
        this->mNumLandUnits++;
    }
}

// FUNCTION: EXE 0x481c42
// Counts a unit: its family, then citizens as gatherers, non-wall buildings as buildings, and military
// families by unit type
void EECPIntelligencePlayerFile::AddUnitClass(EEUnit* inUnit)
{
    unsigned long theKind = inUnit->GetUnitClass()->mFamily;
    this->AddFamily(inUnit);
    EEUnitClass* theClass = inUnit->GetUnitClass();
    bool theHero = theClass->mCategory == kUnitCategoryLandCitizen || theClass->mCategory == kUnitCategoryNavalCitizen;
    if (theHero)
        this->mNumGatherers++;
    else {
        switch (theKind) {
        case kUnitFamilyBuilding:
            if (!theClass->mIsWall)
                this->mNumBuildings++;
            break;
        case kUnitFamilyTank:
        case kUnitFamilyAntiTank: case kUnitFamilyAircraft: case kUnitFamilyPriest: case kUnitFamilySubmarine: case kUnitFamilyHero: case kUnitFamilyShip: case kUnitFamilySpearThrower: case kUnitFamilyMachines: case kUnitFamilyHumanMusket: case kUnitFamilyHumanSword: case kUnitFamilyMountedArcher: case kUnitFamilyLancer: case kUnitFamilyHelicopter:
        case kUnitFamilyCuriassier: case kUnitFamilyBomber: case kUnitFamilyBattleship:
        case kUnitFamilyHuman:
        case kUnitFamilyMechMinotaur: case kUnitFamilyMechMedium: case kUnitFamilyMechZeus: case kUnitFamilyMechCyclops: case kUnitFamilyHumanArcher: case kUnitFamilyRam: case kUnitFamilySiege: case kUnitFamilyLandAA: case kUnitFamilyMechApollo: case kUnitFamilyTankStrong:
        case kUnitFamilyCitizen: case kUnitFamilyTowers: case kUnitFamilyHumanMachineGun: case kUnitFamilyMedievalFieldWeapon: case kUnitFamilyHumanSpear: case kUnitFamilyMountedSpear: case kUnitFamilyAtomicBomber: case kUnitFamilyMechUnderwater: case kUnitFamilyWalls: case kUnitFamilyViking:
        case kUnitFamilyAABuilding: case kUnitFamilyShipGalley: case kUnitFamilyWarElephant: case kUnitFamilyElephantArcher: case kUnitFamilyAircraftCarrier:
        case kUnitFamilyEliteGuard: case kUnitFamilyArtillery: case kUnitFamilyCavalryArcher: case kUnitFamilyICBM: case kUnitFamilySpaceship: case kUnitFamilySpaceFighter: case kUnitFamilySpaceCorvette: case kUnitFamilyTowerSpaceTurret:
            this->AddUnitType(inUnit);
            break;
        }
    }
}

// FUNCTION: EXE 0x481cd0
// Creates the computer player's intelligence with no player files
EECPIntelligence::EECPIntelligence(EEComputerPlayer* inPlayer)
{
    this->mPlayer = inPlayer;
}

// SYNTHETIC: EXE 0x481cfc
// (the scalar deleting destructor)

// FUNCTION: EXE 0x481d18
// Destroys the intelligence, deleting every player file
EECPIntelligence::~EECPIntelligence()
{
    std::map<unsigned long, EECPIntelligencePlayerFile*>::iterator theIt;
    for (theIt = this->mFiles.begin(); theIt != this->mFiles.end(); theIt++)
        delete theIt->second;
}

// FUNCTION: EXE 0x481d60
// Returns the file on the player, creating and updating one if there is none
EECPIntelligencePlayerFile* EECPIntelligence::GetFile(EEPlayer* inPlayer)
{
    std::map<unsigned long, EECPIntelligencePlayerFile*>::iterator theIt = this->mFiles.find(inPlayer->GetPlayerID());
    if (theIt != this->mFiles.end())
        return theIt->second;
    return this->UpdateFile(this->mPlayer, inPlayer);
}

// FUNCTION: EXE 0x481da1
// Finds or creates the file on the player, updates it if due (without armies) and returns it
EECPIntelligencePlayerFile* EECPIntelligence::UpdateFile(EEComputerPlayer* inOwner, EEPlayer* inPlayer)
{
    EECPIntelligencePlayerFile* theFile;
    std::map<unsigned long, EECPIntelligencePlayerFile*>::iterator theIt = this->mFiles.find(inPlayer->GetPlayerID());
    theFile = theIt != this->mFiles.end() ? theIt->second : NULL;
    if (theFile == NULL) {
        theFile = new EECPIntelligencePlayerFile(inOwner, inPlayer);
        this->mFiles.insert(std::map<unsigned long, EECPIntelligencePlayerFile*>::value_type(inPlayer->mPlayerID, theFile));
    }
    theFile->Update(inOwner, false, false);
    return theFile;
}

// FUNCTION: EXE 0x481e30
// Returns a random player the given player is at war with, or player 0 (nature) if there is none
EEPlayer* EECPIntelligence::GetRandomEnemy(EEPlayer* inPlayer)
{
    std::vector<EEPlayer*> theCandidates;
    EEPlayer* thePlayer = NULL;
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        thePlayer = gWorld.mPlayers[i];
        if (thePlayer != inPlayer && inPlayer->mEnemies[thePlayer->mPlayerID] != 0)
            theCandidates.push_back(thePlayer);
        thePlayer = NULL;
    }
    if (theCandidates.size() > 0)
        thePlayer = theCandidates[gPhysics->Fn82D66(theCandidates)];
    if (thePlayer == NULL)
        thePlayer = gWorld.mPlayers[0];
    return thePlayer;
}

// The out-of-line members that sit among the COMDATs (inline in the real headers).

// FUNCTION: EXE 0x482b30
// Returns the player's most valuable town, or NULL; never sets outArg
EETown* EEPlayer::GetLargestTown(bool* outArg)
{
    EETown* theTown = NULL;
    const std::vector<EETown*>& theTowns = this->mTowns;
    for (long i = this->mTowns.size() - 1; i >= 0; i--) {
        EETown* theOther = theTowns[i];
        if (theTown == NULL || (theOther != NULL && theOther->mValue > theTown->mValue))
            theTown = theTowns[i];
    }
    return theTown;
}

// FUNCTION: EXE 0x483977
// Orders unit-type counts: by family, the family later in the file's family list first; within a family,
// the smaller count first
bool EEAIFOIntelligenceAIFamilyAndUnitTypeSorter::operator()(const EECPIUnitTypeCount& inFirst,
                                                              const EECPIUnitTypeCount& inSecond)
{
    bool theResult = false;
    const std::vector<EECPIFamilyCount>& theFamilies = this->mFile->mFamilies;
    unsigned long theNumFamilies = theFamilies.size();
    long theFirstFamily = inFirst.mFamily;
    long theSecondFamily = inSecond.mFamily;
    if (theFirstFamily != theSecondFamily) {
        for (unsigned long i = 0; i < theNumFamilies; i++) {
            if (theFirstFamily == theFamilies[i].mFamily) {
                theResult = false;
                break;
            }
            if (theSecondFamily == theFamilies[i].mFamily) {
                theResult = true;
                break;
            }
        }
    } else
        theResult = inSecond.mCount > inFirst.mCount;
    return theResult;
}

// FUNCTION: EXE 0x483491
// Returns true if both units stand in the same influence region
bool EECPITargetRegionEqual::operator()(const std::pair<EEComplexUnit*, long>& inFirst,
                                        const std::pair<EEComplexUnit*, long>& inSecond)
{
    EETileCoordinate theFirstTile = inFirst.first->GetTile();
    long theSize = gWorld.mInfluenceTable->mRegionSize;
    U2DPoint<long> theFirst;
    theFirst.SetX(theFirstTile.mX / theSize);
    theFirst.SetY(theFirstTile.mY / theSize);
    EETileCoordinate theSecondTile = inSecond.first->GetTile();
    long theSecondSize = gWorld.mInfluenceTable->mRegionSize;
    U2DPoint<long> theSecond;
    theSecond.SetX(theSecondTile.mX / theSecondSize);
    theSecond.SetY(theSecondTile.mY / theSecondSize);
    return theFirst == theSecond;
}

// FUNCTION: EXE 0x483e53
// Orders two units by the influence region they stand in
bool EECPITargetRegionLess::operator()(const std::pair<EEComplexUnit*, long>& inFirst,
                                       const std::pair<EEComplexUnit*, long>& inSecond)
{
    EETileCoordinate theFirstTile = inFirst.first->GetTile();
    EEInfluenceTable* theTable = gWorld.mInfluenceTable;
    U2DPoint<long> theFirst(theFirstTile.mX / theTable->mRegionSize, theFirstTile.mY / theTable->mRegionSize);
    EETileCoordinate theSecondTile = inSecond.first->GetTile();
    U2DPoint<long> theSecond(theSecondTile.mX / theTable->mRegionSize, theSecondTile.mY / theTable->mRegionSize);
    return theFirst < theSecond;
}

// EECPITargetScoreGreater::operator() is out of line in EEAIFunctionObjects.cpp (0x42b0ba)
//
//
//
//
//
//
//
//
//
//
//
//
//

// The map/vector/sort COMDATs, paired with the original by template matching (0x82a43: the sort of the
// gatherers' resources)
// TEMPLATE: EXE 0x481ee2
// TEMPLATE: EXE 0x481f0e
// TEMPLATE: EXE 0x481f3a
// TEMPLATE: EXE 0x481f75
// TEMPLATE: EXE 0x481fa1
// TEMPLATE: EXE 0x481fdc
// TEMPLATE: EXE 0x482007
// TEMPLATE: EXE 0x4820bd
// TEMPLATE: EXE 0x4820f7
// TEMPLATE: EXE 0x482123
// TEMPLATE: EXE 0x48217d
// TEMPLATE: EXE 0x4821c1
// TEMPLATE: EXE 0x482211
// TEMPLATE: EXE 0x482255
// TEMPLATE: EXE 0x4822c6
// TEMPLATE: EXE 0x482597
// TEMPLATE: EXE 0x482834
// TEMPLATE: EXE 0x482866
// TEMPLATE: EXE 0x482926
// TEMPLATE: EXE 0x482a1d
// TEMPLATE: EXE 0x482a32
// TEMPLATE: EXE 0x482b00
// TEMPLATE: EXE 0x482b7a
// TEMPLATE: EXE 0x482c3a
// TEMPLATE: EXE 0x482ca6
// TEMPLATE: EXE 0x482db4
// TEMPLATE: EXE 0x482e62
// TEMPLATE: EXE 0x482e8d
// TEMPLATE: EXE 0x482eaf
// TEMPLATE: EXE 0x482ef6
// TEMPLATE: EXE 0x482f0d
// TEMPLATE: EXE 0x482f36
// TEMPLATE: EXE 0x483057
// TEMPLATE: EXE 0x48306a
// TEMPLATE: EXE 0x48306d
// TEMPLATE: EXE 0x4830a0
// TEMPLATE: EXE 0x4831ac
// (?_Unguarded_insertion_sort@std@@YAXV?$_Ptrit@UEECPIUnitTypeCount@@HPAU1@AAU1@PAU1@AAU1@@1@0VEEAIFOIntelligenceAIFamilyAndUnitTypeSorter@@PAUEECPIUnitTypeCount@@@Z)

// TEMPLATE: EXE 0x4832f0
// TEMPLATE: EXE 0x483320
// TEMPLATE: EXE 0x483349
// TEMPLATE: EXE 0x483421
// TEMPLATE: EXE 0x483455
// TEMPLATE: EXE 0x4834e8
// TEMPLATE: EXE 0x483537
// TEMPLATE: EXE 0x483560
// TEMPLATE: EXE 0x483638
// TEMPLATE: EXE 0x48371f
// TEMPLATE: EXE 0x483759
// TEMPLATE: EXE 0x48379a
// TEMPLATE: EXE 0x483878
// TEMPLATE: EXE 0x4839e8
// TEMPLATE: EXE 0x483a2f
// TEMPLATE: EXE 0x483a7b
// (?_Unguarded_partition@std@@YA?AV?$_Ptrit@UEECPIUnitTypeCount@@HPAU1@AAU1@PAU1@AAU1@@1@V21@0VEEAIFOIntelligenceAIFamilyAndUnitTypeSorter@@PAUEECPIUnitTypeCount@@@Z)

// TEMPLATE: EXE 0x483d61
// TEMPLATE: EXE 0x483da1
// TEMPLATE: EXE 0x483eb1
// TEMPLATE: EXE 0x483eeb
// TEMPLATE: EXE 0x483f2c
// TEMPLATE: EXE 0x484043
// TEMPLATE: EXE 0x48407f
// TEMPLATE: EXE 0x4840c1
// TEMPLATE: EXE 0x484173
// TEMPLATE: EXE 0x4841ad
// TEMPLATE: EXE 0x4841ee
// TEMPLATE: EXE 0x484305
// TEMPLATE: EXE 0x4843a5
// TEMPLATE: EXE 0x4843ce
// TEMPLATE: EXE 0x484469
// TEMPLATE: EXE 0x48449c
// TEMPLATE: EXE 0x4844c1
// TEMPLATE: EXE 0x484505
// TEMPLATE: EXE 0x48452e
// TEMPLATE: EXE 0x484579
// TEMPLATE: EXE 0x4845a2
// TEMPLATE: EXE 0x4845e5
// TEMPLATE: EXE 0x484630
// TEMPLATE: EXE 0x48474b
// TEMPLATE: EXE 0x4848d1
// TEMPLATE: EXE 0x484959
// TEMPLATE: EXE 0x4849af
// TEMPLATE: EXE 0x484a37
// TEMPLATE: EXE 0x484a8d
// TEMPLATE: EXE 0x484c02
// TEMPLATE: EXE 0x484c41
// TEMPLATE: EXE 0x484ca1
// TEMPLATE: EXE 0x484cde
// TEMPLATE: EXE 0x484d3e

// Template and inline COMDATs this unit's code instantiates (the bodies come from the headers; each was
// compared against the original by name)
// TEMPLATE: EXE 0x481a71
// TEMPLATE: EXE 0x482d89
// TEMPLATE: EXE 0x48366c
// TEMPLATE: EXE 0x4838a7
// TEMPLATE: EXE 0x4843f3
// TEMPLATE: EXE 0x484820
// TEMPLATE: EXE 0x4848a2

// Template and inline COMDATs this unit already emits (compared by name)
// TEMPLATE: EXE 0x482a43
// TEMPLATE: EXE 0x48321d
// TEMPLATE: EXE 0x483be8
// TEMPLATE: EXE 0x483c22
// TEMPLATE: EXE 0x483c63
// TEMPLATE: EXE 0x484d7b
// COMDATs named after EECPIUnitTypeCount/EECPIFamilyCount's copy members
// TEMPLATE: EXE 0x482dd9
// TEMPLATE: EXE 0x482e0a
// TEMPLATE: EXE 0x482e32
// TEMPLATE: EXE 0x482ed9
// TEMPLATE: EXE 0x48300e
// TEMPLATE: EXE 0x483204
// TEMPLATE: EXE 0x483ba0
// TEMPLATE: EXE 0x484341
// TEMPLATE: EXE 0x484659
// TEMPLATE: EXE 0x4846d4
// TEMPLATE: EXE 0x48476c
// TEMPLATE: EXE 0x484ad8
// TEMPLATE: EXE 0x484b2e
// TEMPLATE: EXE 0x484ba6
// TEMPLATE: EXE 0x482d66
// TEMPLATE: EXE 0x484b20
