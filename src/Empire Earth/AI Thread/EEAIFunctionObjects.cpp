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
#include <EEAIFunctionObjects.h>
#include "ee/EEUnitSetManager.h"


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
