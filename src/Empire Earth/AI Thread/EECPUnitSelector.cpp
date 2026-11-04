// C:\EEX\Empire Earth\AI Thread\EECPUnitSelector.cpp (file name inferred: no __FILE__; RTTI class
// EECPUnitSelector, vtable 0x43c29c; the trace string "...away in EECPUnitSelector::AllocateAlreadyDefinedUnits..."
// names the class). A computer player's unit selector: 0x979ff..0x9c833 as ONE unit: the unit
// map's second unit "EECPUSGatherUnitsFromDefendingGroups.cpp" 0x9b126..0x9c833 does not exist; 0x9b126..0x9ba62 are
// EECPUnitSelector members (Select 0x9a64a calls 0x9b2e9/0x9b497/0x9b949, AllocateAlreadyDefinedUnits calls
// 0x9b4fa/0x9b569/0x9b817/0x9b86b) with the inline function-object COMDATs 0x9b24a/0x9b2cc after their first user, and
// 0x9ba62..0x9c833 are the unit's template COMDATs (map clear, for_each, sort, unique). EECPWallManager.cpp's class-static
// `$E` chain starts at 0x9c833. EECPUnitManager.cpp ends at 0x979ff (the selector's ctor is the first function).
#include <windows.h>
#include <vector>
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

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEDebugTrace.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPUnitRequirement.h"
#include "ee/EECPCombatStrategy.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPIntelligence.h"
#include "ee/EECPUnitSelector.h"
#include "ee/EECPDefenseCommander.h"
#include "ee/EECPPersonnel.h"
#include "ee/EEUnitSetManager.h"

// The for_each function objects (the COMDATs 0x9bb40/0x9bb5d take them by value and return them) (provisional names)
struct EECPUSDereferenceUnit {
    void operator()(EEUnit* inUnit) { inUnit->Dereference(); }
};

struct EECPUSReferenceUnit {
    void operator()(EEUnit* inUnit) { inUnit->Reference(); }
};

// FilterByStrategy's remove_if predicate (COMDAT 0x9bf7f): a non-transport class that lacks the flag of the strategy's kind
// (0: mIsAttackUnit, 1: mIsDefenseUnit) (provisional name)
struct EECPUSClassLacksFlag {
    EECPUSClassLacksFlag(long inKind) : mKind(inKind) {}
    bool operator()(const long& inClass) const
    {
        bool theResult = false;
        EEUnitClass* theClass = gUnitClassTable[inClass];
        if (!theClass->IsTransport()) {
            if (this->mKind == 0)
                theResult = theClass->mIsAttackUnit == false;
            else if (this->mKind == 1)
                theResult = theClass->mIsDefenseUnit == false;
        }
        return theResult;
    }

    long mKind;
};

// AllocateCounterUnits's sort predicate over its (family, weight) pairs (COMDATs 0x9ba80/0x9bff6: `cmp [val+4], [first+4]; jl`):
// the lighter family first (provisional name)
struct EECPUSFamilyWeightLess {
    bool operator()(const std::pair<long, long>& inLeft, const std::pair<long, long>& inRight) const
    {
        return inLeft.second < inRight.second;
    }
};

#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIRandom.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EETown.h"
#include "ee/EEAIFOUnitsToKillScoringBase.h"
#include "ee/EECPUMGatherUnitsScreenBase.h"
#include <EEAIFunctionObjects.h>
#include "ee/EEArtificialIntelligence.h"
#include "ee/EECivPowersManager.h"
#include "ee/EECPUnitManager.h"
#include "ee/EETeam.h"
#include "ee/EEGlobalConstants.h"

// FUNCTION: EXE 0x4979ff
// Creates the unit selector for a computer player
EECPUnitSelector::EECPUnitSelector(EEComputerPlayer* inPlayer)
{
    this->mPlayer = inPlayer;
}

// SYNTHETIC: EXE 0x497a11
// (the scalar deleting destructor)

// FUNCTION: EXE 0x497a2d
// Destroys the unit selector (nothing to release)
EECPUnitSelector::~EECPUnitSelector()
{
}

// FUNCTION: EXE 0x497a34
// Fills a fixed requirement: per remaining unit takes a class of its kind, class or category; mobile combat classes
// join the group, buildings are queued (at the group's tile when on the map), anything else is traced and dropped
void EECPUnitSelector::AllocateAlreadyDefinedUnits(EECPUnitSelectionGroup* inGroup, EECPUnitRequirement* inRequirement)
{
    EECPCombatStrategy* theStrategy = inGroup->mStrategy;
    long theClassID = 0;
    for (long i = theStrategy->mNumRemaining - 1; i >= 0; i--) {
        long theUnitClass = inRequirement->mUnitKind;
        if (theUnitClass != 0) {
            theClassID = inGroup->mGatherer.TakeByKind(theUnitClass);
            if (theClassID == 0)
                theClassID = this->GetRandomOfKind(theUnitClass, inGroup->mClassesByKind);
        } else if (inRequirement->mUnitClass != 0) {
            if (this->IsAvailable(inRequirement->mUnitClass, inGroup->mGatherer))
                theClassID = inRequirement->mUnitClass;
            if (theClassID == 0 && this->IsCandidate(inRequirement->mUnitClass, inGroup->mClassesByKind))
                theClassID = inRequirement->mUnitClass;
        } else {
            long theType = inRequirement->mCategory;
            if (theType != 0x23) {
                theClassID = inGroup->mGatherer.TakeByCategory(theType);
                if (theClassID == 0)
                    theClassID = this->GetRandomOfCategory(theType, inGroup->mClassesByKind);
            }
        }
        if (theClassID != 0) {
            EEUnitClass* theClass = gUnitClassTable[theClassID];
            if (theClass->mMaxVelocity > 0.0f && (theClass->mIsAttackUnit || theClass->mIsDefenseUnit || theClass->IsTransport())) {
                if ((theClass->mDomains & kMovementAir) && !theClass->FliesOver())
                    theStrategy->mNumAir = max(theStrategy->mNumAir - 1, 0);
                else if (theClass->mDomains & kMovementSea)
                    theStrategy->mNumSea = max(theStrategy->mNumSea - 1, 0);
                else if ((theClass->mDomains & kMovementLand) || theClass->FliesOver())
                    theStrategy->mNumLand = max(theStrategy->mNumLand - 1, 0);
                else if (theClass->mDomains & kMovementSpace)
                    theStrategy->mNumSpace = max(theStrategy->mNumSpace - 1, 0);
                inGroup->mSelected.push_back(theClassID);
            } else if (theClass->IsLandBuilding()) {
                if (gWorld.mMap->GetTileAtLocation(inGroup->mLocation.mTile))
                    this->mPlayer->mCivilizationManager->GetBuildManager()->QueueBuildingAt(theClassID, inGroup->mLocation.mTile.mX,
                                                                               inGroup->mLocation.mTile.mY);
                else
                    this->mPlayer->mCivilizationManager->GetBuildManager()->QueueBuilding(theClassID, 1, 1);
            } else
                EEDebugTrace("!Throwing Object %s(ID:%d) away in EECPUnitSelector::AllocateAlreadyDefinedUnits since its not a building and doesn't have either velocity or use for ai attack/use for ai defense checked.\n", theClass->GetName(), theClassID);
        }
        theStrategy->DecrementRemaining();
    }
}

// FUNCTION: EXE 0x497c53
// Lowers the strategy's remaining unit count by one, not below 0
// (both callers call it out of line)
void EECPCombatStrategy::DecrementRemaining()
{
    this->mNumRemaining = this->mNumRemaining - 1 > 0 ? this->mNumRemaining - 1 : 0;
}

// NEAR: the original calls the vector begin()/operator+ and DecrementRemaining out of line (more inline pressure than ours); the iterator is default-constructed at the top
// FUNCTION: EXE 0x497c61
// Fills a held-units requirement from the units the group holds: the most common unit types first (no heroes,
// priests, prophets or naval buildings), within the domain counts; moves them into the group's unit IDs
void EECPUnitSelector::AllocateHeldUnits(EECPUnitSelectionGroup* inGroup, EECPUnitRequirement* inRequirement)
{
    std::map<long, long> theCounts;
    std::vector<long> theClasses;
    std::map<long, long>::iterator it;
    EECPCombatStrategy* theStrategy = inGroup->mStrategy;
    long theWanted = inRequirement->mNumLeft;
    long the34 = theStrategy->mNumAir;
    long the38 = theStrategy->mNumSea;
    long the3C = theStrategy->mNumLand;
    long the40 = theStrategy->mNumSpace;
    std::vector<EEUnit*>& theUnits = inGroup->mUnits;
    for (long i = theUnits.size() - 1; i >= 0; i--) {
        EEUnitClass* theClass = (theUnits[i])->GetClass();
        if (theClass->mAIKind == kAIKindHero || theClass->mUnitType == kUnitTypePriest || theClass->mUnitType == kUnitTypeProphet ||
            theClass->mCategory == kUnitCategoryNavalBuilding)
            continue;
        it = theCounts.find((theUnits[i])->GetClass()->mUnitType);
        if (it != theCounts.end())
            it->second++;
        else
            theCounts.insert(std::map<long, long>::value_type((theUnits[i])->GetClass()->mUnitType, 1));
    }
    for (long j = 0; j <= 8; j++) {
        if (theWanted <= 0 || theCounts.size() == 0)
            break;
        std::map<long, long>::iterator theBest = theCounts.begin();
        it = theBest;
        while (++it != theCounts.end()) {
            if (it->second > theBest->second)
                theBest = it;
        }
        EEUnitClass* theClass = gUnitClassTable[theBest->first];
        long theTaken = 0;
        for (long k = min(theBest->second, theWanted) - 1; k >= 0; k--, theTaken++) {
            if ((theClass->mDomains & kMovementAir) && !theClass->FliesOver()) {
                if (the34 <= 0)
                    break;
                the34--;
            } else if (theClass->mDomains & kMovementSea) {
                if (the38 <= 0)
                    break;
                the38--;
            } else if (theClass->mDomains & kMovementSpace) {
                if (the40 <= 0)
                    break;
                the40--;
            } else {
                if (the3C <= 0)
                    break;
                the3C--;
            }
            theClasses.push_back(theClass->mUnitType);
        }
        theWanted -= theTaken;
        if (theWanted < 0)
            theWanted = 0;
        theCounts.erase(theBest);
    }
    for (long m = theClasses.size() - 1; m >= 0; m--) {
        for (long n = inGroup->mUnits.size() - 1; n >= 0; n--) {
            if ((inGroup->mUnits[n])->GetClass()->mUnitType != theClasses[m])
                continue;
            EEUnit* theUnit = inGroup->mUnits[n];
            if (theUnit != NULL) {
                EEUnitClass* theClass = theUnit->GetClass();
                if (theClass->mMaxVelocity > 0.0f && (theClass->mIsAttackUnit || theClass->mIsDefenseUnit || theClass->IsTransport())) {
                    inGroup->mUnitIDs.push_back(theUnit->mUnitID);
                    if ((theClass->mDomains & kMovementAir) && !theClass->FliesOver())
                        inGroup->mStrategy->mNumAir = max(inGroup->mStrategy->mNumAir - 1, 0);
                    else if (theClass->mDomains & kMovementSea)
                        inGroup->mStrategy->mNumSea = max(inGroup->mStrategy->mNumSea - 1, 0);
                    else if (theClass->mDomains & kMovementSpace)
                        inGroup->mStrategy->mNumSpace = max(inGroup->mStrategy->mNumSpace - 1, 0);
                    else
                        inGroup->mStrategy->mNumLand = max(inGroup->mStrategy->mNumLand - 1, 0);
                    inRequirement->mNumLeft--;
                    theStrategy->DecrementRemaining();
                    inGroup->mUnits[n]->Dereference();
                    inGroup->mUnits.erase(inGroup->mUnits.begin() + n);
                }
            }
            break;
        }
    }
    if (!(inRequirement->mNumLeft > 0))
        return;
    long theRest = theStrategy->mNumRemaining - inRequirement->mNumLeft;
    if (theRest < 0)
        theRest = 0;
    theStrategy->mNumRemaining = theRest;
}

// FUNCTION: EXE 0x4980a0
// Fills each domain's count with the player's favorite air/sea/space/land type or a random available class that
// passes the favorite screen; the air, sea and space counts are then added to land even when they were filled
void EECPUnitSelector::AllocateFavoriteUnits(EECPUnitSelectionGroup* inGroup)
{
    EECPUSFavoriteUnitsScreen theScreen;
    std::vector<long> theKind34, theKind38, theKind3C, theKind40;
    EECPCombatStrategy* theStrategy = inGroup->mStrategy;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    if (!theStrategy->mAddFavorites)
        return;
    this->SplitByDomain(inGroup, theKind34, theKind38, theKind3C, theKind40);
    if (theStrategy->mNumAir > 0) {
        long theFavorite = inGroup->mPlayer->mFavoriteAirType;
        EEUnitClass* theClass = gUnitClassTable[theFavorite];
        EETechTreeNode* theNode;
        if (theClass == NULL || (theClass->mDomains & kMovementAir) == 0 ||
            (theNode = theTree->Get(theClass->mTechnology)) == NULL || !theNode->GetIsAvailable()) {
            for (long i = theKind34.size() - 1; i >= 0; --i) {
                EEUnitClass* theCandidate = gUnitClassTable[theKind34[i]];
                EETechTreeNode* theCandidateNode;
                if (theCandidate == NULL ||
                    (theCandidateNode = theTree->Get(theCandidate->mTechnology)) == NULL ||
                    !theCandidateNode->GetIsAvailable() || !theScreen.IsFavoriteAir(theCandidate))
                    theKind34.erase(theKind34.begin() + i);
            }
            if (theKind34.empty())
                theFavorite = 0;
            else {
                theFavorite = *(theKind34.begin() + gPhysics->GetRandom(0L, (long)theKind34.size() - 1));
                for (long i = theStrategy->mNumAir - 1; i >= 0; --i)
                    inGroup->mSelected.push_back(theFavorite);
                theStrategy->mNumAir = 0;
            }
        } else if (theFavorite != 0) {
            for (long i = theStrategy->mNumAir - 1; i >= 0; --i)
                inGroup->mSelected.push_back(theFavorite);
            theStrategy->mNumAir = 0;
        }
        theStrategy->mNumLand += theStrategy->mNumAir;
        theStrategy->mNumAir = 0;
    }
    if (theStrategy->mNumSea > 0) {
        long theFavorite = inGroup->mPlayer->mFavoriteSeaType;
        EEUnitClass* theClass = gUnitClassTable[theFavorite];
        EETechTreeNode* theNode;
        if (theClass == NULL || (theClass->mDomains & kMovementSea) == 0 ||
            (theNode = theTree->Get(theClass->mTechnology)) == NULL || !theNode->GetIsAvailable()) {
            for (long i = theKind38.size() - 1; i >= 0; --i) {
                EEUnitClass* theCandidate = gUnitClassTable[theKind38[i]];
                EETechTreeNode* theCandidateNode;
                if (theCandidate == NULL ||
                    (theCandidateNode = theTree->Get(theCandidate->mTechnology)) == NULL ||
                    !theCandidateNode->GetIsAvailable() || !theScreen.IsFavoriteSea(theCandidate))
                    theKind38.erase(theKind38.begin() + i);
            }
            if (theKind38.empty())
                theFavorite = 0;
            else {
                theFavorite = theKind38[gPhysics->GetRandom(0L, (long)theKind38.size() - 1)];
                for (long i = theStrategy->mNumSea - 1; i >= 0; --i)
                    inGroup->mSelected.push_back(theFavorite);
                theStrategy->mNumSea = 0;
            }
        } else if (theFavorite != 0) {
            for (long i = theStrategy->mNumSea - 1; i >= 0; --i)
                inGroup->mSelected.push_back(theFavorite);
            theStrategy->mNumSea = 0;
        }
        theStrategy->mNumLand += theStrategy->mNumSea;
        theStrategy->mNumSea = 0;
    }
    if (theStrategy->mNumSpace > 0) {
        long theFavorite = inGroup->mPlayer->mFavoriteSpaceType;
        EEUnitClass* theClass = gUnitClassTable[theFavorite];
        EETechTreeNode* theNode;
        if (theClass == NULL || (theClass->mDomains & kMovementSpace) == 0 ||
            (theNode = theTree->Get(theClass->mTechnology)) == NULL || !theNode->GetIsAvailable()) {
            for (long i = theKind40.size() - 1; i >= 0; --i) {
                EEUnitClass* theCandidate = gUnitClassTable[theKind40[i]];
                EETechTreeNode* theCandidateNode;
                if (theCandidate == NULL ||
                    (theCandidateNode = theTree->Get(theCandidate->mTechnology)) == NULL ||
                    !theCandidateNode->GetIsAvailable() || !theScreen.IsFavoriteSpace(theCandidate))
                    theKind40.erase(theKind40.begin() + i);
            }
            if (theKind40.empty())
                theFavorite = 0;
            else {
                theFavorite = *(theKind40.begin() + gPhysics->GetRandom(0L, (long)theKind40.size() - 1));
                for (long i = theStrategy->mNumSpace - 1; i >= 0; --i)
                    inGroup->mSelected.push_back(theFavorite);
                theStrategy->mNumSpace = 0;
            }
        } else if (theFavorite != 0) {
            for (long i = theStrategy->mNumSpace - 1; i >= 0; --i)
                inGroup->mSelected.push_back(theFavorite);
            theStrategy->mNumSpace = 0;
        }
        theStrategy->mNumLand += theStrategy->mNumSpace;
        theStrategy->mNumSpace = 0;
    }
    if (theStrategy->mNumLand > 0) {
        long theFavorite = inGroup->mPlayer->mFavoriteLandType;
        EEUnitClass* theClass = gUnitClassTable[theFavorite];
        EETechTreeNode* theNode;
        if (theClass == NULL || (theClass->mDomains & kMovementLand) == 0 ||
            (theNode = theTree->Get(theClass->mTechnology)) == NULL || !theNode->GetIsAvailable()) {
            for (long i = theKind3C.size() - 1; i >= 0; --i) {
                EEUnitClass* theCandidate = gUnitClassTable[theKind3C[i]];
                EETechTreeNode* theCandidateNode;
                if (theCandidate == NULL ||
                    (theCandidateNode = theTree->Get(theCandidate->mTechnology)) == NULL ||
                    !theCandidateNode->GetIsAvailable() || !theScreen.IsFavoriteLand(theCandidate))
                    theKind3C.erase(theKind3C.begin() + i);
            }
            if (theKind3C.empty())
                theFavorite = 0;
            else {
                theFavorite = *(theKind3C.begin() + gPhysics->GetRandom(0L, (long)theKind3C.size() - 1));
                for (long i = theStrategy->mNumLand - 1; i >= 0; --i)
                    inGroup->mSelected.push_back(theFavorite);
            }
        } else if (theFavorite != 0) {
            for (long i = theStrategy->mNumLand - 1; i >= 0; --i)
                inGroup->mSelected.push_back(theFavorite);
        }
        theStrategy->mNumLand = 0;
    }
}

// NEAR: the original keeps `this` and the element offset in frame slots and the 0x28 divisor in ebx (allocation)
// FUNCTION: EXE 0x498731
// Runs each of the strategy's requirements through its allocator (counter, held, special, already defined), then
// adds favorite units when the strategy asks for them
void EECPUnitSelector::AllocateRequirements(EECPUnitSelectionGroup* inGroup)
{
    EECPCombatStrategy* theStrategy = inGroup->mStrategy;
    for (unsigned long i = 0; i < theStrategy->mRequirements.size(); i++) {
        switch (theStrategy->mRequirements[i].mKind) {
        case kRequirementCounter:
            this->AllocateCounterUnits(inGroup, &theStrategy->mRequirements[i]);
            break;
        case kRequirementHeld:
            this->AllocateHeldUnits(inGroup, &theStrategy->mRequirements[i]);
            break;
        case kRequirementSpecial:
            this->AllocateSpecialUnits(inGroup, &theStrategy->mRequirements[i]);
            break;
        case kRequirementAlreadyDefined:
            this->AllocateAlreadyDefinedUnits(inGroup, &theStrategy->mRequirements[i]);
            break;
        }
    }
    if (theStrategy->mAddFavorites)
        this->AllocateFavoriteUnits(inGroup);
}

// FUNCTION: EXE 0x4987c7
// Fills a counter requirement: weighs unit families by the target's unit types, then per domain picks from a few
// random gathered classes weighted by family (special classes with mSpecializedUnitChance); siege share capped
// Requirement kind 0. NEAR: the original rounds the counts up through a float (fild/__ftol after the ceiling) and
// expands GetRandom() in the last two kinds
void EECPUnitSelector::AllocateCounterUnits(EECPUnitSelectionGroup* inGroup, EECPUnitRequirement* inRequirement)
{
    std::vector<std::pair<long, long> > theFamilies;
    std::map<long, long> theUnused;
    std::vector<long> theKind34;
    std::vector<long> theKind38;
    std::vector<long> theKind3C;
    std::vector<long> theKind40;
    std::vector<long> theChoices;
    std::vector<long> theSpecials;
    std::map<long, long> theWeights;
    std::map<long, long>::iterator it;
    EECPIntelligencePlayerFile* theFile = inGroup->mTargetFile;
    EECPCombatStrategy* theStrategy = inGroup->mStrategy;
    long theCount = 0;
    long theMaximum;
    long theClassID;
    long i;
    long j;
    this->SplitByDomain(inGroup, theKind34, theKind38, theKind3C, theKind40);
    for (i = theFile->mUnitTypes.size() - 1; i >= 0; i--) {
        EEUnitClass* theClass = gUnitClassTable[(theFile->mUnitTypes[i]).mUnitType];
        if (theClass != NULL)
            this->WeighCounters(theClass, theWeights, inGroup->mClassesByKind);
    }
    for (it = theWeights.begin(); it != theWeights.end(); ++it)
        theFamilies.push_back(std::pair<long, long>(it->first, it->second));
    std::sort(theFamilies.begin(), theFamilies.end(), EECPUSFamilyWeightLess());
    if (theFamilies.size() == 0)
        return;

    if (theStrategy->mNumAir > 0) {
        theMaximum = gPhysics->GetRandom(3L, 5L);
        EEAIRandomShuffle(theKind34.begin(), theKind34.end());
        for (i = theKind34.size() - 1; i >= 0; i--) {
            EEUnitClass* theClass = gUnitClassTable[*(theKind34.begin() + i)];
            if (theClass == NULL)
                continue;
            if (theClass->mAIKind == kAIKindSpecial)
                theSpecials.push_back(theClass->mUnitType);
            else if (theCount < theMaximum) {
                for (j = theFamilies.size() - 1; j >= 0; j--) {
                    if (theClass->mAIKind != (*(theFamilies.begin() + j)).first)
                        continue;
                    theChoices.insert(theChoices.end(), (*(theFamilies.begin() + j)).second, theClass->mUnitType);
                    theCount++;
                    break;
                }
            }
        }
        if (theChoices.size() > 0) {
            float theFactor = ((gWorld.mMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (gWorld.mMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap) ?
                this->mPlayer->mRPSSelectionIslands : this->mPlayer->mRPSSelectionNonIslands;
            for (long n = UMCeil(theStrategy->mNumAir * theFactor) - 1; n >= 0; n--) {
                if (theSpecials.size() > 0 && gPhysics->GetRandom() <= this->mPlayer->mSpecializedUnitChance)
                    theClassID = theSpecials[gPhysics->GetRandom(0L, (long)theSpecials.size() - 1)];
                else if ((theClassID = inGroup->mGatherer.TakeUnitClass(*theChoices.begin())) == 0)
                    theClassID = *(theChoices.begin() + gPhysics->GetRandom(0L, (long)theChoices.size() - 1));
                if (theClassID != 0) {
                    inGroup->mSelected.push_back(theClassID);
                    theStrategy->mNumAir = max(theStrategy->mNumAir - 1, 0);
                }
            }
        }
    }
    theChoices.clear();
    theSpecials.clear();

    theCount = 0;
    if (theStrategy->mNumSea > 0) {
        theMaximum = gPhysics->GetRandom(3L, 5L);
        EEAIRandomShuffle(theKind38.begin(), theKind38.end());
        for (i = theKind38.size() - 1; i >= 0; i--) {
            EEUnitClass* theClass = gUnitClassTable[*(theKind38.begin() + i)];
            if (theClass == NULL)
                continue;
            if (theClass->mAIKind == kAIKindSpecial)
                theSpecials.push_back(theClass->mUnitType);
            else if (theCount < theMaximum) {
                for (j = theFamilies.size() - 1; j >= 0; j--) {
                    if (theClass->mAIKind != (*(theFamilies.begin() + j)).first)
                        continue;
                    theChoices.insert(theChoices.end(), (*(theFamilies.begin() + j)).second, theClass->mUnitType);
                    theCount++;
                    break;
                }
            }
        }
        if (theChoices.size() > 0) {
            float theFactor = ((gWorld.mMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (gWorld.mMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap) ?
                this->mPlayer->mRPSSelectionIslands : this->mPlayer->mRPSSelectionNonIslands;
            for (long n = UMCeil(theStrategy->mNumSea * theFactor) - 1; n >= 0; n--) {
                if (theSpecials.size() > 0 && gPhysics->GetRandom() <= this->mPlayer->mSpecializedUnitChance)
                    theClassID = theSpecials[gPhysics->GetRandom(0L, (long)theSpecials.size() - 1)];
                else if ((theClassID = inGroup->mGatherer.TakeUnitClass(*theChoices.begin())) == 0)
                    theClassID = *(theChoices.begin() + gPhysics->GetRandom(0L, (long)theChoices.size() - 1));
                if (theClassID != 0) {
                    inGroup->mSelected.push_back(theClassID);
                    theStrategy->mNumSea = max(theStrategy->mNumSea - 1, 0);
                }
            }
        }
    }
    theChoices.clear();
    theSpecials.clear();

    theCount = 0;
    if (theStrategy->mNumSpace > 0) {
        theMaximum = gPhysics->GetRandom(1L, 3L);
        EEAIRandomShuffle(theKind40.begin(), theKind40.end());
        for (i = theKind40.size() - 1; i >= 0; i--) {
            EEUnitClass* theClass = gUnitClassTable[*(theKind40.begin() + i)];
            if (theClass == NULL)
                continue;
            if (theClass->mAIKind == kAIKindSpecial)
                theSpecials.push_back(theClass->mUnitType);
            else if (theCount < theMaximum) {
                for (j = theFamilies.size() - 1; j >= 0; j--) {
                    if (theClass->mAIKind != (*(theFamilies.begin() + j)).first)
                        continue;
                    theChoices.insert(theChoices.end(), (*(theFamilies.begin() + j)).second, theClass->mUnitType);
                    theCount++;
                    break;
                }
            }
        }
        if (theChoices.size() > 0) {
            float theFactor = ((gWorld.mMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (gWorld.mMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap) ?
                this->mPlayer->mRPSSelectionIslands : this->mPlayer->mRPSSelectionNonIslands;
            for (long n = UMCeil(theStrategy->mNumSpace * theFactor) - 1; n >= 0; n--) {
                if (theSpecials.size() > 0 && gPhysics->mRandom.Generate() <= this->mPlayer->mSpecializedUnitChance)
                    theClassID = *(theSpecials.begin() + gPhysics->GetRandom(0L, (long)theSpecials.size() - 1));
                else if ((theClassID = inGroup->mGatherer.TakeUnitClass(*theChoices.begin())) == 0)
                    theClassID = theChoices[gPhysics->GetRandom(0L, (long)theChoices.size() - 1)];
                if (theClassID == 0)
                    continue;
                // each class goes in as often as its unit type weighs (the original compares the weight as an
                // unsigned short: `cmp ax, di; jbe`)
                unsigned short theWeight = (unsigned short)EEAIGetUnitTypeWeight(theClassID);
                for (unsigned short w = 0; w < theWeight; w++)
                    inGroup->mSelected.push_back(theClassID);
                theStrategy->mNumSpace = max(theStrategy->mNumSpace - 1, 0);
            }
        }
    }
    theChoices.clear();
    theSpecials.clear();

    theCount = 0;
    if (!(theStrategy->mNumLand > 0))
        return;
    // families 0x10 and 0x14 drop out after the player's mSiegeWeaponPercentage share of the count
    long theLimit = UMCeil(this->mPlayer->mSiegeWeaponPercentage * theStrategy->mNumLand);
    long theLimited = 0;
    theMaximum = gPhysics->GetRandom(2L, 6L);
    EEAIRandomShuffle(theKind3C.begin(), theKind3C.end());
    for (i = theKind3C.size() - 1; i >= 0; i--) {
        EEUnitClass* theClass = gUnitClassTable[*(theKind3C.begin() + i)];
        if (theClass == NULL)
            continue;
        if (theClass->mAIKind == kAIKindSpecial)
            theSpecials.push_back(theClass->mUnitType);
        else if (theCount < theMaximum) {
            for (j = theFamilies.size() - 1; j >= 0; j--) {
                if (theClass->mAIKind != (*(theFamilies.begin() + j)).first)
                    continue;
                if (theClass->mIsMedic && !theClass->mIsCyber && !theClass->mB30C)
                    theChoices.push_back(theClass->mUnitType);
                else {
                    theChoices.insert(theChoices.end(), (*(theFamilies.begin() + j)).second,
                                      theClass->mUnitType);
                    theCount++;
                }
                if (theClass->mAIKind == kAIKindSiege || theClass->mAIKind == kAIKindArtillery) {
                    if (++theLimited >= theLimit)
                        theFamilies.erase(theFamilies.begin() + j);
                }
                break;
            }
        }
    }
    if (theChoices.size() != 0) {
        float theFactor = ((gWorld.mMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (gWorld.mMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap) ?
            this->mPlayer->mRPSSelectionIslands : this->mPlayer->mRPSSelectionNonIslands;
        for (long n = UMCeil(theStrategy->mNumLand * theFactor) - 1; n >= 0; n--) {
            if (theSpecials.size() > 0 && gPhysics->mRandom.Generate() <= this->mPlayer->mSpecializedUnitChance)
                theClassID = theSpecials[gPhysics->GetRandom(0L, (long)theSpecials.size() - 1)];
            else if ((theClassID = inGroup->mGatherer.TakeUnitClass(*theChoices.begin())) == 0)
                theClassID = theChoices[gPhysics->GetRandom(0L, (long)theChoices.size() - 1)];
            if (theClassID != 0) {
                inGroup->mSelected.push_back(theClassID);
                theStrategy->mNumLand = max(theStrategy->mNumLand - 1, 0);
            }
        }
    }
}

// FUNCTION: EXE 0x4995ea
// Fills a special requirement: maybe a paratrooper plane, then priests, prophets, heroes and carriers by the
// player's chances and population caps, taking the group's own units first, then gathered classes
// Requirement kind 2. Unit 0x2e1 needs civ power 0x34 and comes half the time; each special kind is counted with an
// EECPUGatherSpecialUnitsCount against a ceiling of the player's maximum.
// NEAR: the original expands GetRandom() in the last two blocks
void EECPUnitSelector::AllocateSpecialUnits(EECPUnitSelectionGroup* inGroup, EECPUnitRequirement* inRequirement)
{
    EECPUGatherSpecialUnitsCount theGatherer(inGroup->mPlayer, 0, 0);
    std::map<long, std::vector<long>*>::iterator it;
    EECPCombatStrategy* theStrategy = inGroup->mStrategy;
    long theMaxUnits = this->mPlayer->GetPopulationLimit(0);
    long theClassID = 0;
    long theWanted;
    long theLimit;
    bool theAvailable;
    long i;
    long j;
    long k;
    if (theStrategy->mNumLand >= 4 && theStrategy->mNumAir > 0 && (gWorld.mMap->mMapCharacteristics & kRMMapPlanetsMap) != kRMMapPlanetsMap &&
        gCivPowersManager.HasPower(this->mPlayer, kCivPowerParatroopers) &&
        this->mPlayer->mTechnologies->Get(gUnitClassTable[kUnitTypeParatrooperPlane]->mTechnology)->GetIsAvailable() && gPhysics->GetRandom() <= 0.5f) {
        inGroup->mSelected.push_back(0x2e1);
        theStrategy->mNumLand -= 4;
        theStrategy->mNumAir = max(theStrategy->mNumAir - 1, 0);
    }
    if (theStrategy->mNumLand > 0) {
        // unit type 1
        if (gPhysics->GetRandom() <= this->mPlayer->mPriestChance &&
            (theStrategy->mKind == 4 || theStrategy->mKind == 5 || (gUnitClassTable[kUnitTypePriest]->mIsAttackUnit && theStrategy->mKind == 0) ||
             (gUnitClassTable[kUnitTypePriest]->mIsDefenseUnit && theStrategy->mKind == 1))) {
            theWanted = gPhysics->GetRandom(1L, max(min(this->mPlayer->mPriestMaxPerGroup, theStrategy->mNumLand - 1), 1L));
            theGatherer.mUnits = 0;
            theGatherer.mRequested = 0;
            theGatherer.mCategory = kAIKindNone;
            theGatherer.mUnitType = kUnitTypePriest;
            this->mPlayer->mCivilizationManager->mUnitManager->GatherUnits(theGatherer);
            theLimit = UMCeil(this->mPlayer->mPriestPopPercentage * theMaxUnits);
            if (theGatherer.mUnits + theGatherer.mRequested < theLimit) {
                theWanted = min(theWanted, theLimit - theGatherer.mRequested - theGatherer.mUnits);
                if (theWanted > 0) {
                    theAvailable = inGroup->mGatherer.TakeUnitClass(kUnitTypePriest) == kUnitTypePriest;
                    if (!theAvailable) {
                        for (i = inGroup->mUnits.size() - 1; i >= 0; i--) {
                            if (theWanted <= 0)
                                break;
                            if ((*(inGroup->mUnits.begin() + i))->GetClass()->mUnitType != kUnitTypePriest)
                                continue;
                            inGroup->mUnitIDs.push_back((*(inGroup->mUnits.begin() + i))->mUnitID);
                            theStrategy->mNumLand = max(theStrategy->mNumLand - 1, 0);
                            inGroup->mUnits[i]->Dereference();
                            inGroup->mUnits.erase(inGroup->mUnits.begin() + i);
                            theWanted--;
                        }
                        if (theWanted > 0) {
                            for (it = inGroup->mClassesByKind.begin(); it != inGroup->mClassesByKind.end() && !theAvailable;
                                 ++it) {
                                for (j = it->second->size() - 1; j >= 0 && !theAvailable; j--)
                                    theAvailable = (*it->second)[j] == 1;
                            }
                        }
                    }
                    if (theAvailable) {
                        for (k = theWanted - 1; k >= 0; k--) {
                            inGroup->mSelected.push_back(1);
                            inGroup->mStrategy->mNumLand = max(inGroup->mStrategy->mNumLand - 1, 0);
                        }
                    }
                }
            }
        }
        theGatherer.mUnits = 0;
        theGatherer.mRequested = 0;
        theGatherer.mUnitType = 0;
        theGatherer.mCategory = kAIKindNone;

        // unit type 0xd8
        if (theStrategy->mNumUnits > 1 && gPhysics->GetRandom() <= this->mPlayer->mProphetChance &&
            (theStrategy->mKind == 4 || theStrategy->mKind == 5 ||
             (gUnitClassTable[kUnitTypeProphet]->mIsAttackUnit && theStrategy->mKind == 0) ||
             (gUnitClassTable[kUnitTypeProphet]->mIsDefenseUnit && theStrategy->mKind == 1))) {
            theWanted = gPhysics->GetRandom(1L, max(min(this->mPlayer->mProphetMaxPerGroup, theStrategy->mNumLand - 1), 1L));
            theGatherer.mUnits = 0;
            theGatherer.mRequested = 0;
            theGatherer.mCategory = kAIKindNone;
            theGatherer.mUnitType = kUnitTypeProphet;
            this->mPlayer->mCivilizationManager->mUnitManager->GatherUnits(theGatherer);
            theLimit = UMCeil(this->mPlayer->mProphetPopPercentage * theMaxUnits);
            if (theGatherer.mUnits + theGatherer.mRequested < theLimit) {
                theWanted = min(theWanted, theLimit - theGatherer.mRequested - theGatherer.mUnits);
                if (theWanted > 0) {
                    theAvailable = inGroup->mGatherer.TakeUnitClass(kUnitTypeProphet) == kUnitTypeProphet;
                    if (!theAvailable) {
                        for (i = inGroup->mUnits.size() - 1; i >= 0; i--) {
                            if (theWanted <= 0)
                                break;
                            if (inGroup->mUnits[i]->GetClass()->mUnitType != kUnitTypeProphet)
                                continue;
                            inGroup->mUnitIDs.push_back(inGroup->mUnits[i]->mUnitID);
                            theStrategy->mNumLand = max(theStrategy->mNumLand - 1, 0);
                            inGroup->mUnits[i]->Dereference();
                            inGroup->mUnits.erase(inGroup->mUnits.begin() + i);
                            theWanted--;
                        }
                        if (theWanted > 0) {
                            for (it = inGroup->mClassesByKind.begin(); it != inGroup->mClassesByKind.end() && !theAvailable;
                                 ++it) {
                                for (j = it->second->size() - 1; j >= 0 && !theAvailable; j--)
                                    theAvailable = (*it->second)[j] == 0xd8;
                            }
                        }
                    }
                    if (theAvailable) {
                        for (k = theWanted - 1; k >= 0; k--) {
                            inGroup->mStrategy->mNumLand = max(inGroup->mStrategy->mNumLand - 1, 0);
                            inGroup->mSelected.push_back(0xd8);
                        }
                    }
                }
            }
        }
        theGatherer.mUnits = 0;
        theGatherer.mRequested = 0;
        theGatherer.mUnitType = 0;
        theGatherer.mCategory = kAIKindNone;

        // category 0x1b
        if (theStrategy->mNumUnits > 1 && gPhysics->mRandom.Generate() <= this->mPlayer->mHeroChance && gUnitClassTable[kUnitTypeH24HannibalMorale] != NULL &&
            (theStrategy->mKind == 4 || theStrategy->mKind == 5 ||
             (gUnitClassTable[kUnitTypeH24HannibalMorale]->mIsAttackUnit && theStrategy->mKind == 0) ||
             (gUnitClassTable[kUnitTypeH24HannibalMorale]->mIsDefenseUnit && theStrategy->mKind == 1))) {
            theWanted = gPhysics->GetRandom(1L, max(min(this->mPlayer->mHeroMaxPerGroup, theStrategy->mNumLand - 1), 1L));
            theGatherer.mUnits = 0;
            theGatherer.mRequested = 0;
            theGatherer.mUnitType = 0;
            theGatherer.mCategory = kAIKindHero;
            this->mPlayer->mCivilizationManager->mUnitManager->GatherUnits(theGatherer);
            theLimit = UMCeil(this->mPlayer->mHeroPopPercentage * theMaxUnits);
            if (theGatherer.mUnits + theGatherer.mRequested < theLimit &&
                (gWorld.mGameOptions.mIsScenarioGame || this->mPlayer->GetHeroCount(true, true) < this->mPlayer->mHeroLimit)) {
                theWanted = min(theWanted, theLimit - theGatherer.mRequested - theGatherer.mUnits);
                if (theWanted > 0) {
                    theClassID = inGroup->mGatherer.TakeByKind(0x1b);
                    if (theClassID != 0) {
                        inGroup->mSelected.push_back(theClassID);
                        theWanted--;
                    }
                    if (theWanted > 0) {
                        for (i = inGroup->mUnits.size() - 1; i >= 0; i--) {
                            if (theWanted <= 0)
                                break;
                            if (inGroup->mUnits[i]->GetClass()->mAIKind != kAIKindHero)
                                continue;
                            inGroup->mUnitIDs.push_back(inGroup->mUnits[i]->mUnitID);
                            theStrategy->mNumLand = max(theStrategy->mNumLand - 1, 0);
                            inGroup->mUnits[i]->Dereference();
                            inGroup->mUnits.erase(inGroup->mUnits.begin() + i);
                            theWanted--;
                        }
                        if (theWanted > 0 && inGroup->mClassesByKind.find(0x1b) != inGroup->mClassesByKind.end()) {
                            theClassID = this->GetRandomOfKind(kAIKindHero, inGroup->mClassesByKind);
                            if (theClassID != 0) {
                                for (k = theWanted - 1; k >= 0; k--) {
                                    inGroup->mStrategy->mNumLand = max(inGroup->mStrategy->mNumLand - 1, 0);
                                    inGroup->mSelected.push_back(theClassID);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    theGatherer.mUnits = 0;
    theGatherer.mRequested = 0;
    theGatherer.mUnitType = 0;
    theGatherer.mCategory = kAIKindNone;

    // category 0xf
    if (!(theStrategy->mNumSea > 0) || !(theStrategy->mNumUnits > 1) || !(gPhysics->mRandom.Generate() <= this->mPlayer->mCarrierChance) ||
        gUnitClassTable[kUnitTypeS11EnterpriseAircraftCarrier] == NULL ||
        (theStrategy->mKind != 4 && theStrategy->mKind != 5 && (!gUnitClassTable[kUnitTypeS11EnterpriseAircraftCarrier]->mIsAttackUnit || theStrategy->mKind != 0) &&
         (!gUnitClassTable[kUnitTypeS11EnterpriseAircraftCarrier]->mIsDefenseUnit || theStrategy->mKind != 1)))
        return;
    theWanted = gPhysics->GetRandom(1L, max(min(this->mPlayer->mCarrierMaxPerGroup, theStrategy->mNumSea - 1), 1L));
    theGatherer.mUnits = 0;
    theGatherer.mRequested = 0;
    theGatherer.mUnitType = 0;
    theGatherer.mCategory = kAIKindAircraftCarrier;
    this->mPlayer->mCivilizationManager->mUnitManager->GatherUnits(theGatherer);
    theLimit = UMCeil(this->mPlayer->mCarrierPopPercentage * theMaxUnits);
    if (!(theGatherer.mUnits + theGatherer.mRequested < theLimit))
        return;
    theWanted = min(theWanted, theLimit - theGatherer.mRequested - theGatherer.mUnits);
    if (theWanted <= 0)
        return;
    theClassID = inGroup->mGatherer.TakeByKind(0xf);
    if (theClassID != 0) {
        inGroup->mSelected.push_back(theClassID);
        theWanted--;
    }
    if (theWanted <= 0)
        return;
    for (i = inGroup->mUnits.size() - 1; i >= 0; i--) {
        if (theWanted <= 0)
            break;
        if (inGroup->mUnits[i]->GetClass()->mAIKind != kAIKindAircraftCarrier)
            continue;
        inGroup->mUnitIDs.push_back(inGroup->mUnits[i]->mUnitID);
        theStrategy->mNumSea = max(theStrategy->mNumSea - 1, 0);
        inGroup->mUnits[i]->Dereference();
        inGroup->mUnits.erase(inGroup->mUnits.begin() + i);
        theWanted--;
    }
    if (theWanted <= 0 || inGroup->mClassesByKind.find(0xf) == inGroup->mClassesByKind.end())
        return;
    theClassID = this->GetRandomOfKind(kAIKindAircraftCarrier, inGroup->mClassesByKind);
    if (theClassID == 0)
        return;
    for (k = theWanted - 1; k >= 0; k--) {
        inGroup->mStrategy->mNumSea = max(inGroup->mStrategy->mNumSea - 1, 0);
        inGroup->mSelected.push_back(theClassID);
    }
}

// FUNCTION: EXE 0x49a1d1
// Moves the counts of domains without candidate classes to one random usable domain (air, sea with over 100
// water tiles, land, space with over 100 space tiles), else to land
void EECPUnitSelector::RedistributeDomainCounts(EECPUnitSelectionGroup* inGroup)
{
    std::vector<long> k34, k38, k3C, k40, choices;
    EECPCombatStrategy* s = inGroup->mStrategy;
    long old34=s->mNumAir, old38=s->mNumSea, old3C=s->mNumLand, old40=s->mNumSpace;
    this->SplitByDomain(inGroup,k34,k38,k3C,k40);
    long pool=0;
    if(s->mNumLand>0 && k3C.size() == 0) { pool=old3C; s->mNumLand=0; }
    if(s->mNumSea>0 && k38.size() == 0) { pool+=old38; s->mNumSea=0; }
    if(s->mNumAir>0 && k34.size() == 0) { pool+=old34; s->mNumAir=0; }
    if(s->mNumSpace>0 && k40.size() == 0) { pool+=old40; s->mNumSpace=0; }
    if(pool==0) return;
    if(k34.size() != 0 && s->mAirShare>0.0f) choices.push_back(0);
    if(k38.size() != 0 && s->mSeaShare>0.0f && gContinentManager.mNumWaterTiles>100) choices.push_back(2);
    if(k3C.size() != 0 && s->mLandShare>0.0f) choices.push_back(1);
    if(k40.size() != 0 && s->mSpaceShare>0.0f && gContinentManager.mNumSpaceTiles>100) choices.push_back(3);
    if(choices.size() != 0) {
        EEAIRandomShuffle(choices.begin(),choices.end());
        switch(choices.back()) {
        case 0: s->mNumAir=pool+old34; return;
        case 2: s->mNumSea=pool+old38; return;
        case 3: s->mNumSpace=pool+old40; return;
        }
    }
    s->mNumLand=pool+old3C;
}

// FUNCTION: EXE 0x49a44f
// Sorts the group's candidate classes into air, sea, land and space lists (no transports, priests, prophets,
// heroes or land buildings)
void EECPUnitSelector::SplitByDomain(EECPUnitSelectionGroup* inGroup, std::vector<long>& outKind34,
                               std::vector<long>& outKind38, std::vector<long>& outKind3C,
                               std::vector<long>& outKind40)
{
    std::map<long, std::vector<long>*>::iterator it;
    for (it = inGroup->mClassesByKind.begin(); it != inGroup->mClassesByKind.end(); ++it) {
        for (long i = it->second->size() - 1; i >= 0; i--) {
            long theType = (*it->second)[i];
            EEUnitClass* theClass = gUnitClassTable[theType];
            if (theClass == NULL || theClass->IsTransport() || theClass->GetUnitType() == kUnitTypePriest || theClass->IsLandBuilding() ||
                theClass->GetUnitType() == kUnitTypeProphet || theClass->mAIKind == kAIKindHero)
                continue;
            if ((theClass->mDomains & kMovementAir) && !theClass->FliesOver())
                outKind34.push_back(theClass->GetUnitType());
            else if (theClass->mDomains & kMovementSea)
                outKind38.push_back(theClass->GetUnitType());
            else if ((theClass->mDomains & kMovementLand) || theClass->FliesOver())
                outKind3C.push_back(theClass->GetUnitType());
            else if (theClass->mDomains & kMovementSpace)
                outKind40.push_back(theClass->GetUnitType());
        }
    }
}

// FUNCTION: EXE 0x49a5a9
// Frees the group's candidate class lists and releases the units it holds
void EECPUnitSelector::FreeUnits(EECPUnitSelectionGroup* inGroup)
{
    this->FreeClassesByKind(inGroup->mClassesByKind);
    std::for_each(inGroup->mUnits.begin(), inGroup->mUnits.end(), EECPUSDereferenceUnit());
    inGroup->mUnits.clear();
}

// FUNCTION: EXE 0x49a5fc
// Deletes every class list in the map and empties it
void EECPUnitSelector::FreeClassesByKind(std::map<long, std::vector<long>*>& ioMap)
{
    std::map<long, std::vector<long>*>::iterator it;
    for (it = ioMap.begin(); it != ioMap.end(); ++it)
        delete it->second;
    ioMap.clear();
}

// FUNCTION: EXE 0x49a64a
// Selects the units of a combat group: gathers candidates, sizes the group, shares it out by domain, fills the
// strategy's requirements and frees the work lists
void EECPUnitSelector::Select(EECPUnitSelectionGroup* inGroup)
{
    this->GatherCandidates(inGroup);
    this->GetIntelligenceFiles(inGroup);
    this->ComputeGroupSize(inGroup);
    this->ComputeDomainShares(inGroup);
    this->FilterByStrategy(inGroup);
    this->RedistributeDomainCounts(inGroup);
    this->AllocateRequirements(inGroup);
    this->FreeUnits(inGroup);
}

// FUNCTION: EXE 0x49a695
// Sets the strategy's space, air, sea and land shares (unless they sum to exactly 100) from the epoch, map,
// towns and both players' strengths, then lets the strategy compute its counts
void EECPUnitSelector::ComputeDomainShares(EECPUnitSelectionGroup* inGroup)
{
    EECPCombatStrategy* s=inGroup->mStrategy;
    float remaining=1.0f;
    if(s->mSpaceShare+(s->mAirShare+(s->mSeaShare+s->mLandShare))!=100.0f) {
        if(inGroup->mPlayer->GetTechTree()->mCurrentEpoch>=kTechEpochSpace) {
            inGroup->mStrategy->mSpaceShare=!gWorld.mMap->mTerrainMesh.GetIsSpaceMap()?0.0f:this->GetDomainShare(inGroup,false);
            remaining=1.0f-inGroup->mStrategy->mSpaceShare;
        }
        EEComputerPlayer* player=inGroup->mPlayer;
        float result=0.0f;
        if(player->GetTechTree()->mCurrentEpoch>=kTechEpochAtomicWW1) {
            bool calculate=true;
            if((gWorld.mMap->mMapCharacteristics&kRMMapPlanetsMap)==kRMMapPlanetsMap && inGroup->mTask.mIsActive) {
                calculate=false;
                if(!player->mTowns.empty()) {
                    EEAIContinent* continent=gContinentManager.GetContinentAt(inGroup->mTask.mLocation.mTile.GetX(),inGroup->mTask.mLocation.mTile.mY);
                    if(continent->mID==player->mTowns.back()->mContinent && continent->mCategory!=kContinentCategorySpace) calculate=true;
                }
            }
            if(calculate) {
                EEPlayer* target=inGroup->mTarget;
                float weight=0.0f;
                if(!target->mTowns.empty() && player->GetNumTowns()!=0) {
                    if(target->mTowns.back()->mContinent!=player->mTowns.back()->mContinent && !gWorld.mMap->mTerrainMesh.GetIsSpaceMap())
                        weight=gPhysics->GetRandom(0.1f,0.3f);
                    if(target->mTowns.size()<=player->mTowns.size() &&
                       (float)player->mTowns.back()->mValue*1.5f<(float)target->mTowns.back()->mValue)
                        weight+=gPhysics->GetRandom(0.2f,0.4f);
                }
                long targetCount=inGroup->mTargetFile->GetFamilyCount(kAIKindAntiAir);
                long ownCount=inGroup->mPlayerFile->GetFamilyCount(kAIKindAntiAir);
                if(ownCount<targetCount) weight+=gPhysics->GetRandom(0.05f,0.2f);
                if(weight<=0.0f) {
                    if((gWorld.mMap->mMapCharacteristics&kRMMapPlanetsMap)==kRMMapPlanetsMap) weight=gPhysics->GetRandom(0.0f,0.1f);
                    else weight=gPhysics->GetRandom(0.2f,0.35f);
                }
                result=gPhysics->GetRandom(0.9f,1.35f)*weight;
                if(result>1.0f) result=0.95f;
                else if(result<0.001f) result=0.0f;
                EECPIntelligencePlayerFile* file=inGroup->mTargetFile;
                long total=file->mSpaceStrength+file->mAirStrength+file->mLandStrength+file->mSeaStrength;
                if(total!=0) {
                    float share=(float)file->mAirStrength/(float)total;
                    if(share>0.0f && result>0.0f) {
                        result=(share-result)*this->mPlayer->mNavalUnitWeight+result;
                        if(result>=1.0f) result=1.0f;
                        if(0.0f>result) result=0.0f;
                    }
                }
                if(result>remaining) result=remaining;
            }
            inGroup->mStrategy->mAirShare=result;
            remaining-=inGroup->mStrategy->mAirShare;
        }
        float second=this->GetDomainShare(inGroup,true);
        if(second>remaining) second=remaining;
        inGroup->mStrategy->mSeaShare=second;
        float third=remaining-second;
        if(third<0.0001f) third=0.0f;
        inGroup->mStrategy->mLandShare=third;
    }
    inGroup->mStrategy->Calculate(inGroup->mPlayerFile,inGroup->mTargetFile);
}

// FUNCTION: EXE 0x49ab2f
// Returns the share of the group for the sea (inSea) or space, from the domain's tile share, own vs target
// strength and a random factor; with no target strength there it uses the own unit count of that domain
float EECPUnitSelector::GetDomainShare(EECPUnitSelectionGroup* inGroup, bool inSea)
{
    float theResult = 0.0f;
    if (gContinentManager.GetContinentByID(gContinentManager.GetLargestContinent(inSea == false ? 4 : 2)) != NULL) {
        float theShare = 0.0f;
        long theLand = gContinentManager.mNumWaterTiles;
        long theSea = gContinentManager.mNumSpaceTiles;
        long theTotal = theLand + theSea + gContinentManager.mNumLandTiles;
        if (theTotal > 0) {
            EECPIntelligencePlayerFile* theTarget = inGroup->mTargetFile;
            EECPIntelligencePlayerFile* theOwn = inGroup->mPlayerFile;
            float theTargetStrength = inSea ? (float)theTarget->mSeaStrength : (float)theTarget->mSpaceStrength;
            float theOwnStrength = inSea ? (float)theOwn->mSeaStrength : (float)theOwn->mSpaceStrength;
            if (theTarget->mSpaceStrength + theTarget->mAirStrength + theTarget->mLandStrength + theTarget->mSeaStrength != 0)
                theShare = theTargetStrength
                    / (float)(theTarget->mSpaceStrength + theTarget->mAirStrength + theTarget->mLandStrength + theTarget->mSeaStrength);
            float theRatio;
            if (theTargetStrength > 0.0f) {
                if (theOwnStrength != 0.0f)
                    theRatio = theOwnStrength / theTargetStrength;
                else
                    theRatio = 1.0f - theShare;
            } else
                theRatio = inSea ? (float)theOwn->mNumSeaUnits : (float)theOwn->mNumSpaceUnits;
            float theFraction = (inSea ? (float)theLand : (float)theSea) / theTotal;
            if (theFraction > kHeightEpsilon) {
                theResult = gPhysics->GetRandom(0.85f, 1.15f) * theFraction;
                if (theResult > 1.0f)
                    theResult = theFraction;
                if (theResult > kHeightEpsilon) {
                    if (theRatio >= 1.0f)
                        theResult = theResult / theRatio;
                    else if (theRatio > kHeightEpsilon)
                        theResult = (1.0f - theRatio) * theResult;
                    else
                        theResult = gPhysics->mRandom.Generate() * theResult;
                }
            }
            if (theShare > 0.0f && theResult > 0.0f) {
                theResult = this->mPlayer->mAirUnitWeight * (theShare - theResult) + theResult;
                if (theResult >= 1.0f)
                    theResult = 1.0f;
                if (0.0f > theResult)
                    theResult = 0.0f;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x49ad60
// Sets the strategy's unit count from a random attack/defense share of the target's strength (or its own count);
// what the units chosen for deletion do not cover becomes a held-units requirement in front
// Units to delete: the citizens' surplus, or all but five of the unit manager's workers when the target's wonder
// team has enough wonders
void EECPUnitSelector::ComputeGroupSize(EECPUnitSelectionGroup* inGroup)
{
    EEAIFOUnitsToKillForMilitary theScorer(this->mPlayer, 0, 0, 0);
    EECPUnitRequirement theRequirement;
    float theStrength = inGroup->mTargetFile->mStrength;
    unsigned long theWonders = 0;
    long theCount;
    long theNew;
    long theCitizens = EEAIFOUnitsToKillScoringBase::GetCitizensToKill(this->mPlayer);
    if (theCitizens > 0)
        theScorer.mCitizensToKill = theCitizens;
    if (inGroup->mTask.mIsActive && inGroup->mTask.mUnit != NULL && !inGroup->mTask.mUnit->mPlayer->mObserverMode &&
        inGroup->mTask.mUnit->GetTechTreeNode() != NULL && inGroup->mTask.mUnit->GetTechTreeNode()->mData->mKind == 4) {
        EETeam& theTeam = gWorld.mTeams[inGroup->mTask.mUnit->mPlayer->GetTeamIndex()];
        for (long i = theTeam.GetNumPlayers() - 1; i >= 0; i--)
            theWonders += gWorld.mPlayers[theTeam.mPlayers[i]]->GetNumWonderUnits();
        if (theWonders >= gWorld.mNumberOfWondersToWin) {
            long theExtra = this->mPlayer->mCivilizationManager->mUnitManager->GetNumWorkers() - 5;
            theScorer.mCitizensToKill = theExtra > 0 ? theExtra : 0;
        }
    }
    theScorer.mCount = theScorer.mTransportsToKill + theScorer.mCitizensToKill;
    long theKilled = this->mPlayer->ChooseUnitsToKill(theScorer, false) + this->mPlayer->mCombatManager->GetUnitsAvailable();
    float theShare;
    if (inGroup->mStrategy->mKind == 1 || inGroup->mStrategy->mKind == 5)
        theShare = gPhysics->GetRandom(this->mPlayer->mDefenseGroupSizeMin * 0.01f, this->mPlayer->mDefenseGroupSizeMax * 0.01f);
    else
        theShare = gPhysics->GetRandom(this->mPlayer->mAttackGroupSizeMin * 0.01f, this->mPlayer->mAttackGroupSizeMax * 0.01f);
    EECPCombatStrategy* theStrategy = inGroup->mStrategy;
    bool theAdjust = true;
    if (theStrategy->mNumUnits <= 0) {
        theCount = (long)(theShare * theStrength);
        if (theCount > 0) {
            if (theStrategy->mKind == 0 || theStrategy->mKind == 4)
                theCount = (long)(this->mPlayer->GetAttackGroupModifier() * theCount);
        } else
            theCount = 1;
        if (this->mPlayer->IsDeathmatch() && (inGroup->mStrategy->mKind == 0 || inGroup->mStrategy->mKind == 4) &&
            (unsigned long)gPhysics->mGameTime <= this->mPlayer->mDMEarlyAttackTime)
            theCount = UMCeil(this->mPlayer->mDMEarlyAttackScale * theCount);
    } else {
        theCount = theStrategy->mNumUnits;
        if (theStrategy->mKind == 4 || theStrategy->mKind == 5 ||
            (theStrategy->GetNumRequirements() > 0 && (inGroup->mStrategy->mRequirements[0]).mKind == kRequirementAlreadyDefined))
            theAdjust = false;
    }
    if (theAdjust && theCount > theKilled) {
        if (theKilled > 0) {
            theNew = theCount - theKilled;
            long theShort = (long)inGroup->mUnits.size() - theNew;
            if (theShort < 0)
                theCount += theShort;
            if (theNew >= (long)inGroup->mUnits.size())
                theNew = inGroup->mUnits.size();
        } else {
            if (theCount >= (long)inGroup->mUnits.size())
                theCount = inGroup->mUnits.size();
            inGroup->mStrategy->mRequirements.clear();
            inGroup->mStrategy->mAddFavorites = false;
            theNew = theCount;
        }
        if (theNew > 0) {
            theRequirement.mKind = kRequirementHeld;
            theRequirement.mNumWanted = theNew;
            theRequirement.mNumLeft = theNew;
            inGroup->mStrategy->mRequirements.insert(inGroup->mStrategy->mRequirements.begin(), theRequirement);
        }
    }
    if (theCount <= 0)
        theCount = 0;
    inGroup->mStrategy->mNumUnits = theCount;
    inGroup->mStrategy->mNumRemaining = theCount;
}

// NEAR: the original expands the units' begin() and calls its operator+ out of line (ours expands both); frame 0xc larger
// FUNCTION: EXE 0x49b126
// Adds attack-capable units from the player's defending groups to the group (skipping units it already has) and
// references them
void EECPUnitSelector::GatherDefendingUnits(EECPUnitSelectionGroup* inGroup)
{
    EECPUSGatherUnitsFromDefendingGroups theGatherer(this->mPlayer, true, false);
    this->mPlayer->mCombatManager->mDefenseCommander->CollectDefendingUnits(theGatherer, inGroup->mUnits);
    for (long i = inGroup->mUnits.size() - 1; i >= 0; i--) {
        if (std::find(inGroup->mUnitIDs.begin(), inGroup->mUnitIDs.end(), (inGroup->mUnits[i])->GetUnitID()) !=
            inGroup->mUnitIDs.end())
            inGroup->mUnits.erase(inGroup->mUnits.begin() + i);
    }
    std::for_each(inGroup->mUnits.begin(), inGroup->mUnits.end(), EECPUSReferenceUnit());
}

// NEAR: the original keeps the result in bl (ours al); register allocation only
// FUNCTION: EXE 0x49b24a
// Accepts a unit with the wanted attack/defense flags that is in no attack subgroup, or in one on mission 0, 1
// or 3 (not in transit or carrying) whose group has no continent
// (EECPUSGatherUnitsFromDefendingGroups::operator(): its COMDAT follows its first user GatherDefendingUnits)
bool EECPUSGatherUnitsFromDefendingGroups::operator()(EEUnit* inUnit)
{
    EEUnitClass* theClass = inUnit->GetClass();
    bool theResult = true;
    if (this->mForAttack)
        theResult = theClass->mIsAttackUnit;
    if (theResult && this->mForDefense)
        theResult = theClass->mIsDefenseUnit;
    if (theResult) {
        EECPAttackSubGroup* theSubGroup = this->mPlayer->mCombatManager->mAttackCommander->FindHolder(inUnit);
        if (theSubGroup != NULL) {
            if (theSubGroup->mState != kASGStateInTransit && theSubGroup->mState != kASGStateCarryCargo && theSubGroup->mMission >= 0 &&
                (theSubGroup->mMission <= 1 || theSubGroup->mMission == 3))
                theResult = theSubGroup->mGroup->mContinentID == 0;
            else
                theResult = false;
        }
    }
    return theResult;
}

// SYNTHETIC: EXE 0x49b2cc
// (EEAIFOGatherUnits's scalar deleting destructor, shared by ICF with EECPUSGatherUnitsFromDefendingGroups's)

// FUNCTION: EXE 0x49b2e9
// Gathers the available personnel and the buildable classes by line; attack strategies also take defending
// units; then keeps the best classes of each line
void EECPUnitSelector::GatherCandidates(EECPUnitSelectionGroup* inGroup)
{
    this->mPlayer->mCombatManager->mPersonnel->Gather(inGroup->mGatherer);
    EEAIFOCurrentlyBuildableUnitScreener theScreener(this->mPlayer);
    this->mPlayer->GetScreenedTypesByLine(inGroup->mClassesByKind, theScreener);
    if (inGroup->mStrategy->mKind == 0 || inGroup->mStrategy->mKind == 4)
        this->GatherDefendingUnits(inGroup);
    this->KeepBestClasses(inGroup->mClassesByKind);
}

// NEAR: the original passes a temporary copy of mCounterKind1/mCounterKind2/mL1DC to find (a key-type conversion: the members may be unsigned) and builds the first pair out of line
// FUNCTION: EXE 0x49b34e
// Adds 3, 2 and 1 to the family weights of the class's first, second and third counter kinds that have candidates
void EECPUnitSelector::WeighCounters(EEUnitClass* inClass, std::map<long, long>& ioCounts,
                               std::map<long, std::vector<long>*>& inMap)
{
    if (inClass == NULL)
        return;
    std::map<long, std::vector<long>*>::iterator it;
    std::map<long, long>::iterator theCount;
    if (inClass->mCounterKind1 != 0) {
        it = inMap.find(inClass->mCounterKind1);
        if (it != inMap.end()) {
            theCount = ioCounts.find(it->first);
            if (theCount != ioCounts.end())
                theCount->second += 3;
            else
                ioCounts.insert(std::make_pair(it->first, 3));
        }
    }
    if (inClass->mCounterKind2 != 0) {
        it = inMap.find(inClass->mCounterKind2);
        if (it != inMap.end()) {
            theCount = ioCounts.find(it->first);
            if (theCount != ioCounts.end())
                theCount->second += 2;
            else
                ioCounts.insert(std::make_pair(it->first, 2));
        }
    }
    if (inClass->mCounterKind3 == 0)
        return;
    it = inMap.find(inClass->mCounterKind3);
    if (it == inMap.end())
        return;
    theCount = ioCounts.find(it->first);
    if (theCount != ioCounts.end())
        theCount->second++;
    else
        ioCounts.insert(std::make_pair(it->first, 1));
}

// FUNCTION: EXE 0x49b497
// Fetches and updates the intelligence files of the target and of the group's player
void EECPUnitSelector::GetIntelligenceFiles(EECPUnitSelectionGroup* inGroup)
{
    inGroup->mTargetFile = this->mPlayer->mCombatManager->mIntelligence->GetFile(inGroup->mTarget);
    inGroup->mTargetFile->Update(this->mPlayer, 0, 0);
    inGroup->mPlayerFile = this->mPlayer->mCombatManager->mIntelligence->GetFile(inGroup->mPlayer);
    inGroup->mPlayerFile->Update(this->mPlayer, 0, 0);
}

// FUNCTION: EXE 0x49b4fa
// Returns whether the class is among the candidate classes of any kind
bool EECPUnitSelector::IsCandidate(long inClass, std::map<long, std::vector<long>*>& inMap)
{
    std::map<long, std::vector<long>*>::iterator it;
    bool theFound = false;
    for (it = inMap.begin(); it != inMap.end(); ++it) {
        for (long i = it->second->size() - 1; i >= 0; i--) {
            if ((*it->second)[i] != inClass)
                continue;
            theFound = true;
            it = inMap.end();
            break;
        }
    }
    return theFound;
}

// NEAR: the original loads mPlayer->mTechnologies after the argument (scheduling only)
// FUNCTION: EXE 0x49b569
// Returns whether the class's technology is available, or else whether the personnel gatherer holds the class
bool EECPUnitSelector::IsAvailable(long inClass, EEAIFOPAvailablePersonnelGatherer& inGatherer)
{
    EETechTreeNode* theNode =
        this->mPlayer->GetTechTree()->Get(gUnitClassTable[inClass] ? gUnitClassTable[inClass]->mTechnology : 100000);
    bool theResult = theNode != NULL ? theNode->GetIsAvailable() : false;
    if (!theResult)
        theResult = inGatherer.mClasses.find(inClass) != inGatherer.mClasses.end();
    return theResult;
}

// FUNCTION: EXE 0x49b5ca
// Sorts each line's classes and cuts it at the first drop in epoch, keeping the cut transports and unit-set
// 0x80 classes at the end
void EECPUnitSelector::KeepBestClasses(std::map<long, std::vector<long>*>& ioMap)
{
    EEUnitSet* theSet = gUnitSetManager.mSets.size() > 0x80 ? gUnitSetManager.mSets[0x80] : NULL;
    std::map<long, std::vector<long>*>::iterator it;
    std::vector<long> theMoved;
    for (it = ioMap.begin(); it != ioMap.end(); ++it) {
        std::vector<long>* theClasses = it->second;
        if (theClasses->size() == 0)
            continue;
        std::sort(theClasses->begin(), theClasses->end(), EEAIFOUnitClassGreater());
        long theLevel = 0;
        long i;
        for (i = 0; i < theClasses->size(); i++) {
            if (gUnitClassTable[(*theClasses)[i]]->mEpoch < theLevel)
                break;
            theLevel = gUnitClassTable[(*theClasses)[i]]->mEpoch;
        }
        for (long j = i; j < theClasses->size(); j++) {
            if (gUnitClassTable[(*theClasses)[j]]->IsTransport() || theSet->Contains((*theClasses)[j]))
                theMoved.push_back((*theClasses)[j]);
        }
        if (i < theClasses->size())
            theClasses->erase(theClasses->begin() + i, theClasses->end());
        if (theMoved.size() != 0) {
            theClasses->insert(theClasses->end(), theMoved.begin(), theMoved.end());
            theMoved.clear();
        }
    }
}

// FUNCTION: EXE 0x49b817
// Returns a random candidate class of the kind, or 0
long EECPUnitSelector::GetRandomOfKind(long inKind, std::map<long, std::vector<long>*>& inMap)
{
    std::map<long, std::vector<long>*>::iterator it = inMap.find(inKind);
    long theResult = 0;
    if (it != inMap.end())
        theResult = (*it->second)[gPhysics->GetRandom(0L, (long)it->second->size() - 1)];
    return theResult;
}

// NEAR: the original calls the list's begin() + i operator+ out of line (budget)
// FUNCTION: EXE 0x49b86b
// Returns a random candidate class of the category, or 0
long EECPUnitSelector::GetRandomOfCategory(long inCategory, std::map<long, std::vector<long>*>& inMap)
{
    std::map<long, std::vector<long>*>::iterator it;
    long i;
    long theClass;
    std::vector<long> theClasses;
    long theResult = 0;
    for (it = inMap.begin(); it != inMap.end(); ++it) {
        for (i = it->second->size() - 1; i >= 0; i--) {
            theClass = (*it->second)[i];
            if (gUnitClassTable[theClass] != NULL && gUnitClassTable[theClass]->mCategory == inCategory)
                theClasses.push_back(theClass);
        }
    }
    if (theClasses.size() != 0)
        theResult = theClasses[gPhysics->GetRandom(0L, (long)theClasses.size() - 1)];
    return theResult;
}

// NEAR: one compare scheduled before a store (99%)
// FUNCTION: EXE 0x49b949
// Except for strategy kinds 4 and 5, drops candidate classes without the strategy's attack/defense flag
// (transports kept) and deletes emptied lines
void EECPUnitSelector::FilterByStrategy(EECPUnitSelectionGroup* inGroup)
{
    std::map<long, std::vector<long>*>::iterator it;
    std::vector<long>::iterator theEnd;
    if (inGroup->mStrategy->mKind == 4 || inGroup->mStrategy->mKind == 5)
        return;
    it = inGroup->mClassesByKind.begin();
    while (it != inGroup->mClassesByKind.end()) {
        std::vector<long>* theClasses = it->second;
        if (theClasses->size() != 0) {
            theEnd = std::remove_if(theClasses->begin(), theClasses->end(),
                                                                EECPUSClassLacksFlag(inGroup->mStrategy->mKind));
            if (theEnd != theClasses->end())
                theClasses->erase(theEnd, theClasses->end());
        }
        if (theClasses->size() == 0) {
            it = inGroup->mClassesByKind.erase(it);
            delete theClasses;
        } else
            ++it;
    }
}

// Inline and template COMDATs this unit emits (first users here):
// (0x49ba76 EEDebugTrace is an inline COMDAT of this unit too; it waits for AllocateAlreadyDefinedUnits. DecrementRemaining
// 0x497c53 is defined above)
// TEMPLATE: EXE 0x49ba62
// TEMPLATE: EXE 0x49ba76
// TEMPLATE: EXE 0x49ba80
// TEMPLATE: EXE 0x49bb40
// TEMPLATE: EXE 0x49bb5d
// TEMPLATE: EXE 0x49bb76
// TEMPLATE: EXE 0x49bbb8
// TEMPLATE: EXE 0x49bc75
// TEMPLATE: EXE 0x49bcf7
// TEMPLATE: EXE 0x49bd20
// TEMPLATE: EXE 0x49bdf8
// TEMPLATE: EXE 0x49be2c
// TEMPLATE: EXE 0x49be55
// TEMPLATE: EXE 0x49bf28
// TEMPLATE: EXE 0x49bf59
// TEMPLATE: EXE 0x49bf7f
// TEMPLATE: EXE 0x49bfc2
// TEMPLATE: EXE 0x49bff6
// TEMPLATE: EXE 0x49c092
// TEMPLATE: EXE 0x49c0cc
// TEMPLATE: EXE 0x49c10d
// TEMPLATE: EXE 0x49c1f9
// TEMPLATE: EXE 0x49c229
// TEMPLATE: EXE 0x49c2d4
// TEMPLATE: EXE 0x49c30e
// TEMPLATE: EXE 0x49c34f
// TEMPLATE: EXE 0x49c460
// TEMPLATE: EXE 0x49c529
// TEMPLATE: EXE 0x49c56d
// TEMPLATE: EXE 0x49c667
// TEMPLATE: EXE 0x49c6d4
// TEMPLATE: EXE 0x49c7ae
// TEMPLATE: EXE 0x49c7ff

// Template and inline COMDATs this unit already emits (compared by name)
// TEMPLATE: EXE 0x49c490
// TEMPLATE: EXE 0x49c4db
// TEMPLATE: EXE 0x49c504
// TEMPLATE: EXE 0x49c596
// TEMPLATE: EXE 0x49c611
// TEMPLATE: EXE 0x49c727
// TEMPLATE: EXE 0x49c771
