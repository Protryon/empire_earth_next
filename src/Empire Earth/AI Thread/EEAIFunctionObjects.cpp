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

#include "ee/EEAIFOUnitScreeners.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEMap.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPASGFlags.h"
#include "ee/EECPUnitRequirement.h"
#include "ee/EECPPlacement.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPUMGatherUnitsScreenBase.h"
#include "ee/EEAIFOUnitsToKillScoringBase.h"
#include <EEAIFunctionObjects.h>
#include "ee/EEUnitSetManager.h"


// The aircraft carrier gatherer's slots: the unit's first members (range start 0x29f57).
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

// Scores a citizen for disbanding: idle highest, then waiting gatherers plus a per-resource bonus (wood most),
// builders lowest
long EEAIFOUnitsToKillScoringBase::ScoreCitizen(EEComplexUnit* inUnit)
{
    EEUGRust* theGather = EEGatherGoalCast(inUnit->GetLoadGoal());
    long theScore = 10000;
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionIdle)
        theScore = 10070;
    else if (theGather != NULL) {
        if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionWaitForOpenPosition)
            theScore = 10060;
        if (theGather->GetGatherResource() != NULL) {
            switch (theGather->GetGatherResource()->mResourceType) {
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

// Creates the disband scorer for a player with the numbers of citizens, transports and military units to kill
EEAIFOUnitsToKillForMilitary::EEAIFOUnitsToKillForMilitary(EEComputerPlayer* inPlayer, long inCitizensToKill, long inTransportsToKill, long inMilitaryToKill)
{
    this->mPlayer = inPlayer;
    this->mCitizensToKill = inCitizensToKill;
    this->mTransportsToKill = inTransportsToKill;
    this->mMilitaryToKill = inMilitaryToKill;
}

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
    EECPGatherUnits(inPlayer, theGatherer);
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

// Returns the player's citizen surplus over its citizen share of the population limit
long EEAIFOUnitsToKillScoringBase::GetCitizensToKill(EEComputerPlayer* inPlayer)
{
    long theCount = 0;
    float theRatio = 0.0f;
    return EEAIFOUnitsToKillScoringBase::GetCitizenSurplus(inPlayer, theRatio, theCount);
}

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

// Orders unit class ids by epoch, latest first
bool EEAIFOUnitClassGreater::operator()(const long& inA, const long& inB)
{
    bool theResult = gUnitClassTable[inA]->mEpoch > gUnitClassTable[inB]->mEpoch;
    return theResult;
}

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
                            if (EECPCountOwn(this->mPlayer, theNode->GetUnitClass(0)) <= 0)
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
                        if (EECPCountOwn(this->GetPlayer(), theNode->GetUnitClass(0)) == 0) {
                            theResult = false;
                            return theResult;
                        }
                    }
                    if ((inClass->GetDomains() & kMovementAir) && theBuilder->GetCategory() == kUnitCategoryNavalBuilding && theBuilder->GetAirTrafficType()) {
                        EECPUGatheAircraftCarriersCount theGatherer(this->GetPlayer());
                        EECPGatherUnits(this->GetPlayer(), theGatherer);
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

// Adds a request's count to the requested total for any non-null class; returns whether it counted
bool EECPUMGatherUnitsScreenBase::CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest)
{
    bool theResult = inClass != NULL;
    if (theResult == true)
        this->mRequested += inRequest->mCount;
    return theResult;
}

// Counts any non-null unit; returns whether it counted
bool EECPUMGatherUnitsScreenBase::CountUnit(EEComplexUnit* inUnit)
{
    bool theResult = inUnit != NULL;
    if (theResult == true)
        this->mUnits++;
    return theResult;
}

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

// Creates a military unit counter for a player, counting units or population as asked
EECPUGatherMilitaryUnitsCount::EECPUGatherMilitaryUnitsCount(EEComputerPlayer* inPlayer, bool inByClass,
    bool inRequests, bool inCountPopulation)
    : EECPUMGatherUnitsScreenBase(inByClass, inRequests, false)
{
    this->mPlayer = inPlayer;
    this->mCountPopulation = inCountPopulation;
}

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

// Creates a transport counter for a player
EECPUGatherTransportUnitsCount::EECPUGatherTransportUnitsCount(EEComputerPlayer* inPlayer, bool inByClass,
    bool inRequests)
    : EECPUMGatherUnitsScreenBase(inByClass, inRequests, false)
{
    this->mPlayer = inPlayer;
}

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

// Adds a sector id to the path area's sorted id list unless it is already there (twin of the EEAIContinent
// id-list helpers)
void EEAIPathArea::AddSectorID(unsigned short inSectorID)
{
    if (!std::binary_search(this->mSectorIDs.begin(), this->mSectorIDs.end(), inSectorID)) {
        this->mSectorIDs.push_back(inSectorID);
        std::sort(this->mSectorIDs.begin(), this->mSectorIDs.end());
    }
}

// Returns the tech node's unit class at the index, or its default unit class past the end
// (out of line in this range)
long EETechTreeNode::GetUnitClass(unsigned long inIndex)
{
    if (inIndex < this->mData->mUnitClasses.size())
        return this->mData->mUnitClasses[inIndex];
    return this->mData->mDefaultUnitClass;
}

// the unit manager's GatherUnits without the unit manager (stage 6): the player's units by class (each class's units in
// the player's order), then what its land buildings make (one request each: the one being made and the queue)
void EECPGatherUnits(EEComputerPlayer* inPlayer, EECPUMGatherUnitsScreenBase& ioGatherer)
{
    if (ioGatherer.mByClass) {
        std::map<long, std::deque<EEComplexUnit*> > theUnits;
        for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            EEComplexUnit* c = theUnit->mComplexUnit;
            long theCategory = theUnit->mUnitClass->mCategory;
            if (c == NULL || !(theUnit->mHitPoints > 0) || theCategory == kUnitCategoryProjectile || theCategory == kUnitCategoryBomb
                || theCategory == kUnitCategoryMissile || theUnit->mUnitClass->mUnitType == kUnitTypePathPoint)
                continue;
            theUnits[theUnit->mUnitClass->mUnitType].push_back(c);
        }
        for (std::map<long, std::deque<EEComplexUnit*> >::iterator it = theUnits.begin(); it != theUnits.end(); ++it) {
            if (!ioGatherer.mByUnit)
                ioGatherer.CountUnits(gUnitClassTable[it->first], &it->second);
            else
                for (long i = (long)it->second.size() - 1; i >= 0; i--)
                    ioGatherer.CountUnit(it->second[i]);
        }
    }
    if (!ioGatherer.mRequests)
        return;
    EECPUMRequest theRequest;
    theRequest.mContinent = 0;
    theRequest.mTownID = 0;
    theRequest.mUnitClass = 0;
    theRequest.mCount = 0;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        if (!theComplex || !theComplex->mUnitClass->IsLandBuilding())
            continue;
        EETechTreeNode* theNode;
        long theClass;
        if (theComplex->mCurrentResearch != kTechNone && (theNode = inPlayer->GetTechTree()->Get(theComplex->mCurrentResearch)) != NULL
            && (theClass = theNode->mData->mUnitClass) > 0 && gUnitClassTable[theClass]) {
            theRequest.mCount = 1;
            theRequest.mUnitClass = theClass;
            ioGatherer.CountRequest(gUnitClassTable[theClass], &theRequest);
        }
        for (long k = theComplex->GetBuildQueueSize() - 1; k >= 0; k--) {
            theNode = inPlayer->GetTechTree()->Get(theComplex->mBuildQueue[k].mTechnology);
            if (!theNode || !((theClass = theNode->mData->mUnitClass) > 0) || !gUnitClassTable[theClass])
                continue;
            theRequest.mCount = 1;
            theRequest.mUnitClass = theClass;
            ioGatherer.CountRequest(gUnitClassTable[theClass], &theRequest);
        }
    }
}
