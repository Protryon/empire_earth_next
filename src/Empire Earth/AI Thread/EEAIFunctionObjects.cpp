// C:\EEX\Empire Earth\AI Thread\EEAIFunctionObjects.cpp (file name inferred: the .cpp of
// \eex\Empire Earth\AI Thread\EEAIFunctionObjects.h, whose assert is first emitted at 0x2ddfb). The AI's function
// objects: 0x29fcc..0x2bb72 (EEAIContinentManager.cpp ends at 0x29fcc with its COMDATs; the first member here is
// EEAIFOContinentScreenContinentsShareAdjacentID's slot +0x08; EEAIPathSectorRegionInformation.cpp starts at
// 0x2bb72). Members in address order; the set/map COMDATs close the unit.
#include <windows.h>
#include <vector>
#include <deque>
#include <set>
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
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGGather.h"
#include "ee/EEMap.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPILocation.h"
#include "ee/EECPIntelligence.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPUnitRequirement.h"
#include "ee/EECPCombatStrategy.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPUMGatherUnitsScreenBase.h"
#include "ee/EECPUnitManager.h"
#include "ee/EECPUnitSelector.h"
#include "ee/EEAIFOUnitsToKillScoringBase.h"
#include "ee/EECPMilitaryAidPackage.h"
#include "ee/EEAIFOSubGroupFilter.h"
#include <EEAIFunctionObjects.h>
#include "ee/EEUnitSetManager.h"

#include "ee/EECPExplorationManager.h"

// The aircraft carrier gatherer's slots: the unit's first members (range start 0x29f57).
// FUNCTION: EXE 0x429f57
// Adds a request's count to the requested total when its class is an aircraft carrier (naval building with air
// traffic); returns whether it counted
bool EECPUGatheAircraftCarriersCount::CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest)
{
    bool theResult = false;
    if (inClass && inClass->mCategory == kUnitCategoryNavalBuilding && inClass->mAirTrafficType) {
        this->mRequested += inRequest->mCount;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x429f7e
// Counts one unit when it is an aircraft carrier (naval building with air traffic); returns whether it counted
bool EECPUGatheAircraftCarriersCount::CountUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (inUnit && inUnit->mUnitClass->mCategory == kUnitCategoryNavalBuilding && inUnit->mUnitClass->mAirTrafficType) {
        this->mUnits++;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x429fa1
// Adds a class's unit list size to the unit total when the class is an aircraft carrier; returns whether it counted
bool EECPUGatheAircraftCarriersCount::CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits)
{
    bool theResult = false;
    if (inClass && inClass->mCategory == kUnitCategoryNavalBuilding && inClass->mAirTrafficType && inUnits) {
        this->mUnits += inUnits->size();
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x429fcc
// Returns true when the continent id is one of the two screened continents or is adjacent to both of them
// the two id tests as one bool (`bool theResult = a || b; if (!theResult) {..}`), the original's shape
bool EEAIFOContinentScreenContinentsShareAdjacentID::Test(unsigned short inID)
{
    bool theResult = inID == this->mContinent1 || inID == this->mContinent2;
    if (!theResult) {
        theResult = gContinentManager.mTileFlags[this->mContinent1 * gContinentManager.mFlagsWidth + inID];
        if (theResult)
            theResult = gContinentManager.mTileFlags[this->mContinent2 * gContinentManager.mFlagsWidth + inID];
    }
    return theResult;
}

// FUNCTION: EXE 0x42a019
// Scores a military unit for disbanding: minus its influence, raised for explorers, obsolete units and units
// below half (more below a fifth) health
long EEAIFOUnitsToKillScoringBase::ScoreMilitary(EEComplexUnit* inUnit)
{
    float theScore = (float)-inUnit->mUnitClass->mInfluence;
    if (inUnit->mActiveGoal != NULL && inUnit->mActiveGoal->mKind == kGoalExplore && theScore != 0.0f)
        theScore += UMAbs(theScore * 0.5f);
    if (inUnit->GetTechTreeNode() != NULL && inUnit->GetTechTreeNode()->GetIsObsolete() && theScore < 0.0f)
        theScore += UMAbs(theScore * 0.65f);
    long theMaximum = inUnit->mStats->mMaxHitPoints;
    if (theMaximum > 0) {
        float theHealth = (float)inUnit->mHitPoints / theMaximum;
        if (theHealth < 0.5f)
            theScore += UMAbs(theHealth < 0.2f ? theScore * 0.7f : theScore * 0.35f);
    }
    return UMCeil(theScore);
}

// FUNCTION: EXE 0x42a126
// Scores a transport for disbanding: 10000, more when parked unloading, idle or detached from the AI, plus its
// cargo count
long EEAIFOUnitsToKillScoringBase::ScoreTransport(EEComplexUnit* inUnit)
{
    long theScore = 10000;
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionUnloadTransport) {
        if (inUnit->mVelocity == 0.0f)
            theScore = 10500;
    } else if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionIdle)
        theScore = 10200;
    else if (inUnit->mIsDetachedFromAI)
        theScore = 10100;
    return inUnit->GetNumCargoUnits() + theScore;
}

// FUNCTION: EXE 0x42a193
// Scores a citizen for disbanding: idle highest, then waiting gatherers plus a per-resource bonus (wood most),
// builders lowest
long EEAIFOUnitsToKillScoringBase::ScoreCitizen(EEComplexUnit* inUnit)
{
    EEUGGather* theGather = dynamic_cast<EEUGGather*>(inUnit->GetLoadGoal());
    long theScore = 10000;
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionIdle)
        theScore = 10070;
    else if (theGather != NULL) {
        if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionWaitForOpenPosition)
            theScore = 10060;
        if (theGather->mResource != NULL) {
            switch (theGather->mResource->mResourceType) {
            case kResourceFood:
                theScore += 10;
                break;
            case kResourceWood:
                theScore += 40;
                break;
            case kResourceStone:
                theScore += 30;
                break;
            case kResourceGold:
                theScore += 15;
                break;
            case kResourceIron:
                theScore += 20;
                break;
            }
        }
    } else if (inUnit->mActiveGoal != NULL && inUnit->mActiveGoal->mKind == kGoalBuildRepair)
        theScore = 9950;
    return theScore;
}

// FUNCTION: EXE 0x42a246
// Creates the disband scorer for a player with the numbers of citizens, transports and military units to kill
EEAIFOUnitsToKillForMilitary::EEAIFOUnitsToKillForMilitary(EEComputerPlayer* inPlayer, long inCitizensToKill, long inTransportsToKill, long inMilitaryToKill)
{
    this->mPlayer = inPlayer;
    this->mCitizensToKill = inCitizensToKill;
    this->mTransportsToKill = inTransportsToKill;
    this->mMilitaryToKill = inMilitaryToKill;
}

// FUNCTION: EXE 0x42a271
// Returns whether a live, population-costing, uncontained unit may be disbanded: citizens while citizens remain
// to kill, non-transport military (not space carriers) while military remain; transports never
bool EEAIFOUnitsToKillForMilitary::CanKill(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (inUnit->mHitPoints > 0 && inUnit->mUnitClass->GetPopulationCost() > 0 && inUnit->mIsCollidable != 0
        && inUnit->mContainer == NULL) {
        if (inUnit->mIsCitizen)
            theResult = this->mCitizensToKill > 0;
        else if (!inUnit->mUnitClass->IsTransport() && inUnit->mUnitClass->mUnitType != kUnitTypeSp15SpaceCarrier && this->mMilitaryToKill > 0)
            theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x42a2d7
// Returns a unit's disband score as a citizen or as military; transports and space carriers score 0
long EEAIFOUnitsToKillForMilitary::GetKillScore(EEComplexUnit* inUnit)
{
    long theResult = 0;
    if (inUnit->mIsCitizen)
        theResult = this->ScoreCitizen(inUnit);
    else if (!inUnit->mUnitClass->IsTransport() && inUnit->mUnitClass->mUnitType != kUnitTypeSp15SpaceCarrier)
        theResult = this->ScoreMilitary(inUnit);
    return theResult;
}

// FUNCTION: EXE 0x42a31f
// Adds a request's count to the requested total when its class is a land or naval citizen; returns whether it
// counted
bool EECPUGatherCitizenUnitsCount::CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest)
{
    bool theResult = false;
    if (inClass != NULL && (inClass->mCategory == kUnitCategoryLandCitizen || inClass->mCategory == kUnitCategoryNavalCitizen)) {
        this->mRequested += inRequest->mCount;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x42a348
// Returns how many citizens (existing plus requested) the player has above its citizen share of the population
// limit; also returns the citizen ratio and count (0 when the limit is 0)
// NEAR: the original zeroes theCount (edi) after the gatherer and keeps theResult in ebx (declaring theCount later
// makes VC6 call the gatherer's ctor out of line: 70%)
long EEAIFOUnitsToKillScoringBase::GetCitizenSurplus(EEComputerPlayer* inPlayer, float& outRatio, long& outCount)
{
    float theRatio = 0.0f;
    float theFactor = inPlayer->mCitizenPercentage;
    long theResult = 0;
    long theCount = 0;
    EECPUGatherCitizenUnitsCount theGatherer(inPlayer);
    long theMaximum = inPlayer->GetPopulationLimit(0);
    inPlayer->mCivilizationManager->mUnitManager->GatherUnits(theGatherer);
    if (theMaximum > 0) {
        float theMaximumF = (float)theMaximum;
        theRatio = (float)(theGatherer.mUnits + theGatherer.mRequested) / theMaximumF;
        long theWanted = UMCeil(theMaximumF * theFactor);
        theCount = UMCeil(theMaximumF * theRatio);
        theResult = theCount - theWanted;
    }
    outCount = theCount;
    outRatio = theRatio;
    return theResult;
}

// FUNCTION: EXE 0x42a40f
// Returns the player's citizen surplus over its citizen share of the population limit
long EEAIFOUnitsToKillScoringBase::GetCitizensToKill(EEComputerPlayer* inPlayer)
{
    long theCount = 0;
    float theRatio = 0.0f;
    return EEAIFOUnitsToKillScoringBase::GetCitizenSurplus(inPlayer, theRatio, theCount);
}

// FUNCTION: EXE 0x42a432
// Counts one unit when it is a land or naval citizen; returns whether it counted
bool EECPUGatherCitizenUnitsCount::CountUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (inUnit != NULL && (inUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen || inUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen)) {
        this->mUnits++;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x42a457
// Adds a citizen class's unit list size to the unit total; returns whether it counted
bool EECPUGatherCitizenUnitsCount::CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits)
{
    bool theResult = false;
    if (inClass != NULL && inUnits != NULL && (inClass->mCategory == kUnitCategoryLandCitizen || inClass->mCategory == kUnitCategoryNavalCitizen)) {
        this->mUnits += inUnits->size();
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x42a486
// Records one available unit under its unit class, starting the class's count at 1 or incrementing it
void EEAIFOPAvailablePersonnelGatherer::AddUnit(const std::pair<long, EEUnit*>& inUnit)
{
    std::map<long, std::pair<long, EEUnitClass*> >::iterator theIterator = this->mClasses.find(inUnit.first);
    if (theIterator != this->mClasses.end())
        theIterator->second.first++;
    else
        this->mClasses.insert(std::make_pair(inUnit.first, std::make_pair(1L, inUnit.second->mUnitClass)));
}

// FUNCTION: EXE 0x42a4d9
// Takes one available unit of the class (dropping the class at 0); returns the class, or 0 when none is
long EEAIFOPAvailablePersonnelGatherer::TakeUnitClass(long inUnitClass)
{
    long theResult = 0;
    std::map<long, std::pair<long, EEUnitClass*> >::iterator theIterator = this->mClasses.find(inUnitClass);
    if (theIterator != this->mClasses.end()) {
        theResult = theIterator->first;
        theIterator->second.first--;
        if (theIterator->second.first <= 0)
            this->mClasses.erase(theIterator);
    }
    return theResult;
}

// FUNCTION: EXE 0x42a51e
// Takes one available unit of the first class in the unit category; returns that class, or 0 when none matches
long EEAIFOPAvailablePersonnelGatherer::TakeByCategory(long inCategory)
{
    std::map<long, std::pair<long, EEUnitClass*> >::iterator theIterator;
    long theResult = 0;
    for (theIterator = this->mClasses.begin(); theIterator != this->mClasses.end(); ++theIterator) {
        if (theIterator->second.second == 0 || theIterator->second.second->mCategory != inCategory)
            continue;
        theResult = theIterator->first;
        theIterator->second.first--;
        if (theIterator->second.first <= 0)
            this->mClasses.erase(theIterator);
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x42a57e
// Takes one available unit of the first class of the AI kind; returns that class, or 0 when none matches
long EEAIFOPAvailablePersonnelGatherer::TakeByKind(long inKind)
{
    std::map<long, std::pair<long, EEUnitClass*> >::iterator theIterator;
    long theResult = 0;
    for (theIterator = this->mClasses.begin(); theIterator != this->mClasses.end(); ++theIterator) {
        if (theIterator->second.second == 0 || theIterator->second.second->mAIKind != inKind)
            continue;
        theResult = theIterator->first;
        theIterator->second.first--;
        if (theIterator->second.first <= 0)
            this->mClasses.erase(theIterator);
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x42a5de
// Orders unit class ids by epoch, latest first
bool EEAIFOUnitClassGreater::operator()(const long& inA, const long& inB)
{
    bool theResult = gUnitClassTable[inA]->mEpoch > gUnitClassTable[inB]->mEpoch;
    return theResult;
}

// FUNCTION: EXE 0x42a60a
// Creates a sub group filter for a continent (0: any), allowing adjacent continents and packaged groups as asked
EEAIFOSubGroupFilter::EEAIFOSubGroupFilter(unsigned short inContinent, bool inAllowAdjacent, bool inAllowPackaged)
{
    this->mAllowAdjacent = inAllowAdjacent;
    this->mContinent = inContinent;
    this->mAllowPackaged = inAllowPackaged;
}

// FUNCTION: EXE 0x42a65d
// Returns whether an attack group has no continent
bool EEAIFOSubGroupFilter::operator()(EECPAttackGroup* inGroup)
{
    return inGroup->mContinentID == 0;
}

// FUNCTION: EXE 0x42a66d
// Accepts a non-empty sub group that is idle or defending, of an accepted kind, on (or next to) the continent,
// and not in an aid package unless packaged groups are allowed
bool EEAIFOSubGroupFilter::operator()(EECPAttackSubGroup* inGroup)
{
    bool theSkip = inGroup->GetNumUnits() <= 0 ||
        (inGroup->mTask.mIsActive && ((inGroup->mTask.mMode != kAGModeDefense && inGroup->mTask.mMode != kAGModeNone) ||
         inGroup->mTask.mType == kAGTaskTerritory || inGroup->VirtualBC()));
    bool result;
    if (theSkip)
        result = false;
    else {
        result = this->mKinds.find(inGroup->GetKind()) != this->mKinds.end();
        if (result && this->mContinent != 0) {
            unsigned short id = inGroup->GetMainContinent();
            result = this->mContinent == id;
            if (!result && this->mAllowAdjacent)
                result = gContinentManager.mTileFlags[this->mContinent * gContinentManager.mFlagsWidth + id];
        }
        if (result == true && !this->mAllowPackaged)
            result = this->mSubGroupIDs.find(inGroup->GetID()) == this->mSubGroupIDs.end();
    }
    return result;
}

// FUNCTION: EXE 0x42a752
// Replaces the filter's aid packages with the map's non-null packages and collects their sub group ids
void EEAIFOSubGroupFilter::SetPackages(std::map<long, EECPMilitaryAidPackage*>& inPackages)
{
    this->mPackages.clear();
    this->mSubGroupIDs.clear();
    std::map<long, EECPMilitaryAidPackage*>::iterator theIterator;
    EECPMilitaryAidPackage* thePackage;
    for (theIterator = inPackages.begin(); theIterator != inPackages.end(); ++theIterator) {
        thePackage = theIterator->second;
        if (thePackage == NULL)
            continue;
        this->mPackages.push_back(thePackage);
        for (long i = thePackage->GetNumUnits() - 1; i >= 0; i--)
            this->mSubGroupIDs.insert(thePackage->mSubGroupIDs[i]);
    }
}

// FUNCTION: EXE 0x42a7d2
// Accepts a moving, attacking, non-citizen class within the epoch span whose domain is allowed, whose tech is
// live and whose builder is live or present, and which passes that domain's favourite-unit test
bool EEAIFOFavoriteUnitScreener::Screen(EEUnitClass* inClass)
{
    bool theResult = false;
    EECPUSFavoriteUnitsScreen theScreen;
    theScreen.mAllowAir = this->mAllowAir;
    theScreen.mAllowSea = this->mAllowSea;
    theScreen.mAllowLand = this->mAllowLand;
    theScreen.mAllowSpace = this->mAllowSpace;
    if (inClass->mEpoch <= this->mEpoch && this->mEpoch - inClass->mEpoch <= this->mEpochSpan && inClass->mMaxVelocity != 0.0f &&
        inClass->mAttack > 0 && inClass->mCategory != kUnitCategoryLandCitizen && inClass->mCategory != kUnitCategoryNavalCitizen) {
        theResult = (inClass->mDomains & kMovementAir) ? this->mAllowAir :
                    ((inClass->mDomains & kMovementSea) ? this->mAllowSea : ((inClass->mDomains & kMovementSpace) ? this->mAllowSpace : this->mAllowLand));
        if (theResult) {
            theResult = false;
            EETechTreeNode* theNode = this->mPlayer->GetTechTree()->Get(inClass->GetTechnology());
            if (theNode && !theNode->GetIsObsolete() && !theNode->mScenEditorObsoleted) {
                EEUnitClass* theFirst = gUnitClassTable[theNode->GetUnitClass(0)];
                if (theFirst) {
                    EETechTreeNode* theFirstNode = this->mPlayer->GetTechTree()->Get(theFirst->GetTechnology());
                    if (theFirstNode) {
                        bool theAvailable = true;
                        if (theFirstNode->GetIsObsolete() || theFirstNode->mScenEditorObsoleted) {   // the builder node's flag (was the class node's)
                            std::deque<EEComplexUnit*>* theUnits =
                                this->mPlayer->mCivilizationManager->GetUnitManager()->GetUnits(theNode->GetUnitClass(0));
                            if (theUnits == NULL || theUnits->size() <= 0)
                                theAvailable = false;
                        }
                        if (!theAvailable)
                            theResult = false;
                        else if (inClass->mDomains & kMovementAir)
                            theResult = theScreen.IsFavoriteAir(inClass);
                        else if (inClass->mDomains & kMovementSea)
                            theResult = theScreen.IsFavoriteSea(inClass);
                        else if (inClass->mDomains & kMovementSpace)
                            theResult = theScreen.IsFavoriteSpace(inClass);
                        else
                            theResult = theScreen.IsFavoriteLand(inClass);
                    }
                }
            }
        }
    }
    return theResult;
}

// SYNTHETIC: EXE 0x42a978
// (EECPUSFavoriteUnitsScreen's scalar deleting destructor)

// FUNCTION: EXE 0x42a9bd
// Returns whether an air class is a favourite: an attacking mover that is no hero, healer, priest, special,
// transport, mine or support kind and is not in unit set 0x75
bool EECPUSFavoriteUnitsScreen::IsFavoriteAir(EEUnitClass* inClass)
{
    bool theResult = false;
    long theKind = inClass->mAIKind;
    if (inClass->mMaxVelocity != 0.0f && inClass->mAttack > 0) {
        if (theKind == kAIKindHero || inClass->mHasCalamityPowers || inClass->mConversionRate != 0.0f || inClass->IsPriest() || inClass->mAmmoType == 7 ||
            inClass->mIsMedic || theKind == kAIKindSpecial || inClass->IsTransport() || inClass->mFamily == kUnitFamilyMines)
            theResult = false;
        else {
            theResult = true;
            switch (inClass->mAIKind) {
            case kAIKindSpecial:
            case kAIKindArtillery:
            case kAIKindSiege:
            case kAIKindAntiSubmarineHelicopter:
            case kAIKindMechZeus:
                theResult = false;
                break;
            default:
                if (!gUnitSetManager.GetSet(0x75)->IsEmpty())
                    theResult = !gUnitSetManager.GetSet(0x75)->Contains(inClass->mUnitType);
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x42aab4
// Returns whether a sea class is a favourite: an attacking mover that is no hero, healer, priest, special,
// transport, mine, anti-air or support kind and is not in unit set 0x74
bool EECPUSFavoriteUnitsScreen::IsFavoriteSea(EEUnitClass* inClass)
{
    bool theResult = false;
    long theKind = inClass->mAIKind;
    if (inClass->mMaxVelocity != 0.0f && inClass->mAttack > 0) {
        if (theKind == kAIKindHero || inClass->mIsMedic || inClass->mHasCalamityPowers || inClass->mConversionRate != 0.0f || inClass->IsPriest() ||
            theKind == kAIKindSpecial || inClass->IsTransport() || inClass->mFamily == kUnitFamilyMines)
            theResult = false;
        else {
            theResult = true;
            switch (inClass->mAIKind) {
            case kAIKindSpecial:
            case kAIKindAntiAir:
            case kAIKindArtillery:
            case kAIKindSiege:
            case kAIKindAntiSubmarineHelicopter:
            case kAIKindMechZeus:
                theResult = false;
                break;
            default:
                if (!gUnitSetManager.GetSet(0x74)->IsEmpty())
                    theResult = !gUnitSetManager.GetSet(0x74)->Contains(inClass->mUnitType);
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x42aba7
// Returns whether a land class is a favourite: an attacking mover that is no hero, healer, anti-tank, siege,
// anti-air, artillery, transport or mine, not in unit set 0x73 (fliers: nor 0x75)
bool EECPUSFavoriteUnitsScreen::IsFavoriteLand(EEUnitClass* inClass)
{
    bool theResult = false;
    long theKind = inClass->mAIKind;
    if (inClass->mMaxVelocity != 0.0f && inClass->mAttack > 0) {
        if (theKind == kAIKindHero || inClass->mIsMedic || inClass->mHasCalamityPowers || inClass->mConversionRate != 0.0f || theKind == kAIKindAntiTank ||
            theKind == kAIKindSpecial || theKind == kAIKindSiege || theKind == kAIKindAntiAir || theKind == kAIKindArtillery || inClass->IsTransport() ||
            inClass->mFamily == kUnitFamilyMines)
            theResult = false;
        else {
            theResult = true;
            switch (inClass->mAIKind) {
            case kAIKindSpecial:
            case kAIKindAntiAir:
            case kAIKindArtillery:
            case kAIKindSiege:
            case kAIKindAntiSubmarineHelicopter:
            case kAIKindMechZeus:
                theResult = false;
                break;
            default:
                if (!gUnitSetManager.GetSet(0x73)->IsEmpty())
                    theResult = !gUnitSetManager.GetSet(0x73)->Contains(inClass->mUnitType);
                if (theResult == true && inClass->FliesOver()) {
                    if (!gUnitSetManager.GetSet(0x75)->IsEmpty())
                        theResult = !gUnitSetManager.GetSet(0x75)->Contains(inClass->mUnitType);
                }
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x42ad11
// Returns whether a space class is a favourite: an attacking mover that is no hero, healer, priest, special,
// transport, mine, anti-air or support kind and is not in unit set 0x98
bool EECPUSFavoriteUnitsScreen::IsFavoriteSpace(EEUnitClass* inClass)
{
    bool theResult = false;
    long theKind = inClass->mAIKind;
    if (inClass->mMaxVelocity != 0.0f && inClass->mAttack > 0) {
        if (theKind == kAIKindHero || inClass->mIsMedic || inClass->mHasCalamityPowers || inClass->mConversionRate != 0.0f || inClass->IsPriest() ||
            theKind == kAIKindSpecial || inClass->IsTransport() || inClass->mFamily == kUnitFamilyMines)
            theResult = false;
        else {
            theResult = true;
            switch (inClass->mAIKind) {
            case kAIKindSpecial:
            case kAIKindAntiAir:
            case kAIKindArtillery:
            case kAIKindSiege:
            case kAIKindAntiSubmarineHelicopter:
            case kAIKindMechZeus:
                theResult = false;
                break;
            default:
                if (!gUnitSetManager.GetSet(0x98)->IsEmpty())
                    theResult = !gUnitSetManager.GetSet(0x98)->Contains(inClass->mUnitType);
                break;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x42ae09
// Accepts a moving attack, defence or transport class of the current epoch or earlier whose tech is live, whose
// builder is live or present (carrier aircraft: a carrier exists or is requested), in a combat category
bool EEAIFOCurrentlyBuildableUnitScreener::Screen(EEUnitClass* inClass)
{
    bool theResult = false;
    if (inClass->GetEpoch() <= this->GetPlayer()->GetTechTree()->GetCurrentEpoch() && inClass->GetMaxVelocity() != 0.0f &&
        (inClass->GetIsAttackUnit() || inClass->GetIsDefenseUnit() || inClass->IsTransport())) {
        EETechTreeNode* theNode = this->GetPlayer()->GetTechTree()->Get(inClass->GetTechnology());
        if (theNode && !theNode->GetIsObsolete() && !theNode->GetScenEditorObsoleted()) {
            EEUnitClass* theBuilder = gUnitClassTable[theNode->GetUnitClass(0)];
            if (theBuilder) {
                EETechTreeNode* theBuilderNode = this->GetPlayer()->GetTechTree()->Get(theBuilder->GetTechnology());
                if (theBuilderNode) {
                    if (theBuilderNode->GetIsObsolete() || theBuilderNode->GetScenEditorObsoleted()) {   // the builder node's flag (was the class node's)
                        std::deque<EEComplexUnit*>* theUnits =
                            this->GetPlayer()->GetCivilizationManager()->GetUnitManager()->GetUnits(theNode->GetUnitClass(0));
                        if (theUnits == NULL || theUnits->size() == 0) {
                            theResult = false;
                            return theResult;
                        }
                    }
                    if ((inClass->GetDomains() & kMovementAir) && theBuilder->GetCategory() == kUnitCategoryNavalBuilding && theBuilder->GetAirTrafficType()) {
                        EECPUGatheAircraftCarriersCount theGatherer(this->GetPlayer());
                        this->GetPlayer()->GetCivilizationManager()->GetUnitManager()->GatherUnits(theGatherer);
                        theResult = theGatherer.GetUnits() + theGatherer.GetRequested() != 0;
                        if (!theResult)
                            return theResult;
                    }
                    switch (inClass->GetCategory()) {
                    case kUnitCategoryAirplane:
                    case kUnitCategoryNaval:
                    case kUnitCategoryLand:
                    case kUnitCategoryHelicopter:
                    case kUnitCategoryLand8:
                    case kUnitCategorySubmergedNaval:
                    case kUnitCategoryLandPriest:
                    case kUnitCategoryNavalPriest:
                    case kUnitCategoryLand15:
                    case kUnitCategoryLandTransport:
                    case kUnitCategoryHelicopterTransport:
                    case kUnitCategoryNavalTransport:
                    case kUnitCategoryNavalBuilding:
                    case kUnitCategoryLandMilitaryHero:
                    case kUnitCategorySpaceship:
                    case kUnitCategorySpaceBuilding:
                    case kUnitCategorySpaceTransport:
                    case kUnitCategoryPlanetSpaceship:
                        theResult = inClass->GetFamily() != kUnitFamilyMines;
                        break;
                    default:
                        theResult = false;
                        break;
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x42afdf
// Accepts a buildable non-citizen class from the last two epochs that costs 1 population, can explore, heals
// nothing and is no support, siege, building, hero or carrier kind
bool EEAIFOAcceptableComputerPlayerExplorerUnitScreener::Screen(EEUnitClass* inClass)
{
    bool theResult = EEAIFOCurrentlyBuildableUnitScreener::Screen(inClass);
    if (theResult) {
        if (inClass->mCategory == kUnitCategoryLandCitizen || inClass->mCategory == kUnitCategoryNavalCitizen ||
            inClass->mEpoch < this->mPlayer->GetTechTree()->mCurrentEpoch - 1 || inClass->GetPopulationCost() > 1 ||
            inClass->FindAction(kActionFindUnexploredArea) == NULL)
            theResult = false;
        else {
            theResult = !inClass->mHasCalamityPowers && inClass->mHealThreshold == 0.0f && !inClass->mIsMedic;
            theResult &= inClass->mUnitType != 0x2f4;
            if (theResult == true) {
                switch (inClass->mAIKind) {
                case kAIKindAntiTank:
                case kAIKindSpecial:
                case kAIKindFieldWeapon:
                case kAIKindAntiAir:
                case kAIKindBomber:
                case kAIKindAircraftCarrier:
                case kAIKindArtillery:
                case kAIKindSiege:
                case kAIKindFortification:
                case kAIKindBuilding:
                case kAIKindHero:
                case kAIKindSpaceCarrier:
                    theResult = false;
                    break;
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x42b0ba
// Orders EECPIntelligence's GetTargets list: higher score first, building-19 targets ahead; a building-19 and a
// other target pair orders the building first (struct in ee/EECPIntelligence.h)
bool EECPITargetScoreGreater::operator()(const std::pair<EEComplexUnit*, long>& inFirst,
                                         const std::pair<EEComplexUnit*, long>& inSecond)
{
    long theFirstCategory = inFirst.first->GetUnitClass()->mCategory;
    bool theResult;
    if (inSecond.first->GetUnitClass()->mCategory != kUnitCategoryBuilding19)
        theResult = theFirstCategory == kUnitCategoryBuilding19 || inFirst.second > inSecond.second;
    else
        theResult = theFirstCategory == kUnitCategoryBuilding19 && inFirst.second > inSecond.second;
    return theResult;
}

// FUNCTION: EXE 0x42b100
// Creates a strike-location scorer for a sub group starting from the given tile
EEAIFOBestRegionToAttack::EEAIFOBestRegionToAttack(EECPAttackSubGroup* inSubGroup, const EETileLocation& inTile)
    : mSubGroup(inSubGroup)
{
    this->mTile = inTile;
}

// FUNCTION: EXE 0x42b129
// Creates the land strike-location scorer for a sub group starting from the given tile
EEAIFOBestInfluenceRegionToAttack::EEAIFOBestInfluenceRegionToAttack(EECPAttackSubGroup* inSubGroup,
    const EETileLocation& inTile)
    : EEAIFOBestRegionToAttack(inSubGroup, inTile)
{
}

// FUNCTION: EXE 0x42b145
// Scores every location 0
float EEAIFOBestInfluenceRegionToAttack::operator()(const EECPILocation& inLocation)
{
    return 0.0f;
}

// (folded by ICF into 0x42b145) Scores every air strike location 0
float EEAIFOBestInfluenceRegionToAirStrike::operator()(const EECPILocation& inLocation)
{
    return 0.0f;
}

// FUNCTION: EXE 0x42b14e
// Creates the air strike scorer; a positive chance sets the strike count to chance * 10 (at least 1), else 1
// strike at chance 0
EEAIFOBestInfluenceRegionToAirStrike::EEAIFOBestInfluenceRegionToAirStrike(EECPAttackSubGroup* inSubGroup,
    const EETileLocation& inTile, float inChance)
    : EEAIFOBestRegionToAttack(inSubGroup, inTile), mChance(0.0f)
{
    this->mCount = 1;
    if (!(inChance > 0.0f))
        return;
    this->mCount = (long)(inChance * 10.0f);
    if (this->mCount == 0)
        this->mCount = 1;
    this->mChance = inChance;
}

// SYNTHETIC: EXE 0x42b1a4
// (EEAIFOBestRegionToAttack's scalar deleting destructor, shared with EEAIFOBestInfluenceRegionToAttack by ICF)

// FUNCTION: EXE 0x42b1c1
// Adds a request's count to the requested total for any non-null class; returns whether it counted
bool EECPUMGatherUnitsScreenBase::CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest)
{
    bool theResult = inClass != NULL;
    if (theResult == true)
        this->mRequested += inRequest->mCount;
    return theResult;
}

// FUNCTION: EXE 0x42b1da
// Counts any non-null unit; returns whether it counted
bool EECPUMGatherUnitsScreenBase::CountUnit(EEComplexUnit* inUnit)
{
    bool theResult = inUnit != NULL;
    if (theResult == true)
        this->mUnits++;
    return theResult;
}

// FUNCTION: EXE 0x42b1ec
// Adds a class's unit list size to the unit total; returns whether it counted
bool EECPUMGatherUnitsScreenBase::CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits)
{
    bool theResult = false;
    if (inClass != NULL && inUnits != NULL) {
        theResult = true;
        this->mUnits += inUnits->size();
    }
    return theResult;
}

// FUNCTION: EXE 0x42b207
// Creates a military unit counter for a player, counting units or population as asked
EECPUGatherMilitaryUnitsCount::EECPUGatherMilitaryUnitsCount(EEComputerPlayer* inPlayer, bool inByClass,
    bool inRequests, bool inCountPopulation)
    : EECPUMGatherUnitsScreenBase(inByClass, inRequests, false)
{
    this->mPlayer = inPlayer;
    this->mCountPopulation = inCountPopulation;
}

// FUNCTION: EXE 0x42b239
// Adds a military request (not unit type 0x2e5 or a space/naval building) to the requested total, in units or
// population (the type tests run only after the null and military tests pass)
bool EECPUGatherMilitaryUnitsCount::CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest)
{
    bool theResult = inClass != NULL && this->mPlayer->mCombatManager->IsMilitaryClass(inClass) ? true : false;
    theResult = theResult && inClass->mUnitType != 0x2e5 && inClass->mCategory != kUnitCategorySpaceBuilding
        && inClass->mCategory != kUnitCategoryNavalBuilding;
    if (theResult == true)
        this->mRequested += this->mCountPopulation ? inClass->GetPopulationCost() * inRequest->mCount : inRequest->mCount;
    return theResult;
}

// FUNCTION: EXE 0x42b2b2
// Counts a military unit (not unit type 0x2e5 or a space/naval building) as 1 or its population cost;
// the type tests run only after the null and military tests pass
bool EECPUGatherMilitaryUnitsCount::CountUnit(EEComplexUnit* inUnit)
{
    EEUnitClass* theClass;
    EECPCombatManager* theManager;
    bool theResult = inUnit != NULL && (theClass = inUnit->mUnitClass, theManager = this->mPlayer->mCombatManager,
        theManager->IsMilitaryClass(theClass)) ? true : false;
    theResult = theResult && inUnit->mUnitClass->mUnitType != 0x2e5 && inUnit->mUnitClass->mCategory != kUnitCategorySpaceBuilding
        && inUnit->mUnitClass->mCategory != kUnitCategoryNavalBuilding;
    if (theResult != true)
        return theResult;
    if (this->mCountPopulation)
        this->mUnits += inUnit->mUnitClass->GetPopulationCost();
    else
        this->mUnits++;
    return theResult;
}

// FUNCTION: EXE 0x42b323
// Adds a military class's unit list size (or that times its population cost) to the unit total (the type tests
// run only after the null and military tests pass)
bool EECPUGatherMilitaryUnitsCount::CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits)
{
    bool theResult = inClass != NULL && inUnits != NULL && this->mPlayer->mCombatManager->IsMilitaryClass(inClass) ? true : false;
    theResult = theResult && inClass->mUnitType != 0x2e5 && inClass->mCategory != kUnitCategorySpaceBuilding
        && inClass->mCategory != kUnitCategoryNavalBuilding;
    if (theResult == true)
        this->mUnits += this->mCountPopulation ? inClass->GetPopulationCost() * inUnits->size() : inUnits->size();
    return theResult;
}

// FUNCTION: EXE 0x42b3a7
// Creates a transport counter for a player
EECPUGatherTransportUnitsCount::EECPUGatherTransportUnitsCount(EEComputerPlayer* inPlayer, bool inByClass,
    bool inRequests)
    : EECPUMGatherUnitsScreenBase(inByClass, inRequests, false)
{
    this->mPlayer = inPlayer;
}

// FUNCTION: EXE 0x42b3d2
// Adds a transport request's count to the requested total; returns whether it counted
bool EECPUGatherTransportUnitsCount::CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest)
{
    bool theResult = false;
    if (inClass != NULL && inClass->IsTransport()) {
        this->mRequested += inRequest->mCount;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x42b3fc
// Counts one unit when it is a transport; returns whether it counted
bool EECPUGatherTransportUnitsCount::CountUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (inUnit != NULL && inUnit->mUnitClass->IsTransport()) {
        this->mUnits++;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x42b422
// Adds a transport class's unit list size to the unit total; returns whether it counted
bool EECPUGatherTransportUnitsCount::CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits)
{
    bool theResult = false;
    if (inClass != NULL && inClass->IsTransport() && inUnits != NULL) {
        this->mUnits += inUnits->size();
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x42b450
// Adds a request's count when its class has the wanted unit type or AI kind; returns whether it counted
bool EECPUGatherSpecialUnitsCount::CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest)
{
    bool theResult = false;
    if (inClass != NULL) {
        theResult = (this->mUnitType != 0 && inClass->mUnitType == this->mUnitType) || (this->mCategory != kAIKindNone && inClass->mAIKind == this->mCategory);
        if (theResult)
            this->mRequested += inRequest->mCount;
    }
    return theResult;
}

// FUNCTION: EXE 0x42b48b
// Counts one unit whose class has the wanted unit type or AI kind; returns whether it counted
bool EECPUGatherSpecialUnitsCount::CountUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (inUnit != NULL) {
        EEUnitClass* theClass = inUnit->mUnitClass;
        theResult = (this->mUnitType != 0 && theClass->mUnitType == this->mUnitType) || (this->mCategory != kAIKindNone && theClass->mAIKind == this->mCategory);
        if (theResult)
            this->mUnits++;
    }
    return theResult;
}

// FUNCTION: EXE 0x42b4c2
// Adds the unit list size of a class with the wanted unit type or AI kind; returns whether it counted
bool EECPUGatherSpecialUnitsCount::CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits)
{
    bool theResult = false;
    if (inClass != NULL && inUnits != NULL) {
        theResult = (this->mUnitType != 0 && inClass->mUnitType == this->mUnitType) || (this->mCategory != kAIKindNone && inClass->mAIKind == this->mCategory);
        if (theResult)
            this->mUnits += inUnits->size();
    }
    return theResult;
}

// FUNCTION: EXE 0x42b503
// Adds a hero request's count to the requested total; returns whether it counted
bool EECPUGatherHeroesCount::CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest)
{
    bool theResult = false;
    if (inClass != NULL) {
        theResult = inClass->mAIKind == kAIKindHero;
        if (theResult)
            this->mRequested += inRequest->mCount;
    }
    return theResult;
}

// FUNCTION: EXE 0x42b528
// Counts one unit when it is a hero; returns whether it counted
bool EECPUGatherHeroesCount::CountUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (inUnit != NULL) {
        theResult = inUnit->mUnitClass->mAIKind == kAIKindHero;
        if (theResult)
            this->mUnits++;
    }
    return theResult;
}

// FUNCTION: EXE 0x42b549
// Adds a hero class's unit list size to the unit total; returns whether it counted
bool EECPUGatherHeroesCount::CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits)
{
    bool theResult = false;
    if (inClass != NULL && inUnits != NULL) {
        theResult = inClass->mAIKind == kAIKindHero;
        if (theResult)
            this->mUnits += inUnits->size();
    }
    return theResult;
}

// TEMPLATE: EXE 0x42b574
// TEMPLATE: EXE 0x42b588
// TEMPLATE: EXE 0x42b5c2
// TEMPLATE: EXE 0x42b5ee
// TEMPLATE: EXE 0x42b628
// TEMPLATE: EXE 0x42b6de
// TEMPLATE: EXE 0x42b718
// TEMPLATE: EXE 0x42b789
// TEMPLATE: EXE 0x42ba37
// TEMPLATE: EXE 0x42ba69
// TEMPLATE: EXE 0x42ba92
// TEMPLATE: EXE 0x42babb
// TEMPLATE: EXE 0x42bae4
// FUNCTION: EXE 0x42baf4
// Adds a sector id to the path area's sorted id list unless it is already there (twin of the EEAIContinent
// id-list helpers)
void EEAIPathArea::AddSectorID(unsigned short inSectorID)
{
    if (!std::binary_search(this->mSectorIDs.begin(), this->mSectorIDs.end(), inSectorID)) {
        this->mSectorIDs.push_back(inSectorID);
        std::sort(this->mSectorIDs.begin(), this->mSectorIDs.end());
    }
}

// FUNCTION: EXE 0x42a995
// Returns the tech node's unit class at the index, or its default unit class past the end
// (out of line in this range)
long EETechTreeNode::GetUnitClass(unsigned long inIndex)
{
    if (inIndex < this->mData->mUnitClasses.size())
        return this->mData->mUnitClasses[inIndex];
    return this->mData->mDefaultUnitClass;
}
