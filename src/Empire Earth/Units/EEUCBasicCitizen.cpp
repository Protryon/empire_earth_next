// C:\EEX\Empire Earth\Units\EEUCBasicCitizen.cpp (file name inferred: no __FILE__; the class name). The citizens'
// unit class: gathering (resource and storage searches), building and repairing. 0x20bcf7..0x20d571 (after
// EEUCAmbient.cpp; EEUCBasicPriest.cpp follows).
#include <windows.h>
#include <stdio.h>
#include <vector>
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
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMapDimensions.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEAIRandom.h"
#include "ee/EEAIArea.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPIntelligence.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEE2DBattlefieldAmbient.h"
#include "ee/EESoundObject.h"
#include "ee/EEEffectManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUCBasicCitizen.h"

// 0x243c7d EEBlockTakes: ee/EEAreaUtilities.h


// 0x247021: the tiles around a footprint (inSizeX by inSizeY at (inX, inY)) (provisional name; arguments inferred)
void EEGetRingCorners(long inArg1, long inX, long inY, long inSizeX, long inSizeY, std::vector<U2DPoint<long> >& outTiles);
// 0x24697b: the tiles of a footprint (inSizeX by inSizeY at (inX, inY)) (provisional name; arguments inferred)
void EEGetSquareRing(long inX, long inY, long inSizeX, long inSizeY, std::vector<U2DPoint<long> >& outTiles);

// FUNCTION: EXE 0x60bcf7
// Steps the citizen's goal for gather, build and repair actions (smart tasks for humans only); with no goal, finds a
// storage for a carried load; empties the load when dying or rotting; then uses the common handling
void EEUCBasicCitizen::ActionChanged(EEComplexUnit* inUnit)
{
    EEUnitGoal* theGoal = inUnit->mGoalState;
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionRepairBuilding:
    case kActionGatherResource:
    case kActionAddResourceToStorage:
    case kActionFindNewResource:
    case kActionWaitForBuildSiteToClear:
    case kActionConvertToLocationGoal:
    case kActionMoveToWaitingArea:
    case kActionNotifyWaitingCitizen:
    case kActionCheckForOpenPosition:
    case kActionWaitForOpenPosition:
    case kActionFindFarm:
    case kActionMoveToResource:
    case kActionMoveToStorage:
    case kActionMoveToRepairSite:
        if (theGoal != NULL)
            theGoal->Step(inUnit);
        return;
    case kActionFindNewStorage:
    case kActionFindStorage:
        if (theGoal != NULL)
            theGoal->Step(inUnit);
        else if (inUnit->mResourceAmount > 0) {
            EEUnit* theStorage = this->FindStorage(inUnit, (inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionFindNewStorage);
            if (theStorage != NULL)
                inUnit->SetGoal(new EEUGGather(NULL, theStorage, false), 0, 0);
        }
        return;
    case kActionTaskSmartCitizen:
        if (theGoal != NULL && inUnit->mPlayer->IsHuman())
            theGoal->Step(inUnit);
        return;
    case kActionResourcesRotting:
        if (inUnit->mUnitClass->mBloodStainEffect != NULL)
            EEPlaceTrackDecal(inUnit);
        inUnit->SetResourceAmount(0);
        break;
    case kActionDying:
        inUnit->SetResourceAmount(0);
        break;
    case kActionIdle:
        inUnit->SetProductionTime(0);
        break;
    }
    EEUnitClass::ActionChanged(inUnit);
}

// FUNCTION: EXE 0x60be58
// Searches rings 2..9 around the citizen for a tile on its continent where a unit of the given type fits; returns
// whether one was found
bool EEUCBasicCitizen::FindWoodSite(EEComplexUnit* inUnit, long inUnitType, U2DPoint<float>& outLocation)
{
    std::vector<EETileCoordinate> theTiles;
    EEUnitClass* theClass = gUnitClassTable[inUnitType];
    bool theFound = false;
    long theX = inUnit->mTileX;
    long theY = inUnit->mTileY;
    long theRadius = 2;
    do {
        if (theFound)
            break;
        Fn2492C2(theX, theY, theRadius, theTiles);
        for (std::vector<EETileCoordinate>::iterator it = theTiles.begin(); it != theTiles.end() && !theFound; ++it) {
            if (!((unsigned long)it->GetX() < gMapWidth) || !((unsigned long)it->GetY() < gMapHeight))
                continue;
            unsigned short theContinent = gContinentManager.GetContinent(*it, kAIPathLandMass);
            if (gContinentManager.GetContinent(inUnit, kAIPathLandMass) != theContinent
                || !EEBlockTakes(inUnit, inUnitType, it->GetX(), it->GetY(), theClass->GetTileSize(), theClass->GetTileSize(), 0))
                continue;
            theFound = true;
            outLocation.SetX((float)it->GetX());
            outLocation.SetY((float)it->GetY());
        }
        theRadius++;
    } while (theRadius < 10);
    return theFound;
}

// FUNCTION: EXE 0x60bf63
// Returns whether a unit matches the wanted resource: any for kind 1, else the same sort as inResource (farm, animal
// or fish, forage bush) or the wanted resource type; animals only when inFarms
bool EEIsResourceOfKind(EEUnit* inUnit, EEUnit* inResource, unsigned long inKind, bool inFarms)
{
    bool theTree;
    if (inResource != NULL && (inResource->mUnitClass->mFamily == kUnitFamilyAnimal || inResource->mUnitClass->mFamily == kUnitFamilyFish))
        theTree = true;
    else
        theTree = false;
    bool theFarm;
    if (inResource != NULL && inResource->mUnitClass->mUnitType == kUnitTypeFarm)
        theFarm = true;
    else
        theFarm = false;
    bool theFish;
    if (inResource != NULL && inResource->mUnitClass->mUnitType == kUnitTypeResForageBush)
        theFish = true;
    else
        theFish = false;
    return (inKind == 1 || inKind == 2 && theFarm && inUnit->mUnitClass->mUnitType == kUnitTypeFarm
            || theTree && (inUnit->mUnitClass->mFamily == kUnitFamilyAnimal || inUnit->mUnitClass->mFamily == kUnitFamilyFish)
            || theFish && inUnit->mUnitClass->mUnitType == kUnitTypeResForageBush || inKind != 2 && inUnit->mResourceType == inKind)
           && (inFarms || inUnit->mUnitClass->mFamily != kUnitFamilyAnimal);
}

// FUNCTION: EXE 0x60c026
// Returns the nearest gatherable resource of a kind within inRange rings of a centre unit, not in ioUnits (tiles
// shuffled per ring; another path area costs 400; a dead animal wins at once; boats look for naval resources)
EEUnit* EEUCBasicCitizen::FindResourceOfKind(EEComplexUnit* inUnit, EEUnit* inCenter, EEUnit* inResource, long inRange,
                                   unsigned long inKind, std::vector<EEUnit*>* ioUnits, bool inFarms)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<U2DPoint<long> > theInner;
    std::vector<U2DPoint<long> > theOuter;
    long theX = inCenter->GetTileX();
    long theY = inCenter->GetTileY();
    long theSize = inCenter->GetUnitClass()->GetTileSize();
    long theBest = inRange * inRange * 2;
    EEUnit* theResult = NULL;
    long i = 0;
    GE2DPoint theCenter(theSize * 0.5f + theX, inCenter->GetUnitClass()->GetTileSize() * 0.5f + theY);
    if (inRange > 0) {
        do {
            if (theBest == 0)
                break;
            EEGetSideTiles(theX, theY, theSize, theInner);
            EEGetRingCorners(1, theX, theY, theSize, theSize, theOuter);
            theSize += 2;
            theX--;
            theY--;
            theTiles.clear();
            theTiles.insert(theTiles.end(), theInner.begin(), theInner.end());
            theTiles.insert(theTiles.end(), theOuter.begin(), theOuter.end());
            EEAIRandomShuffle(theTiles.begin(), theTiles.end());
            for (std::vector<U2DPoint<long> >::iterator it = theTiles.begin(); it != theTiles.end(); ++it) {
                long theTileX = it->GetX();
                long theTileY = it->GetY();
                EEMapTile* theTile = gWorld.mMap->GetTile(theTileX, theTileY);
                if (theTile == NULL)
                    continue;
                RTSTileUnitNode* theNode = theTile->mUnits;
                if (this->mCategory == kUnitCategoryLandCitizen) {
                    for (; theNode != NULL; theNode = theNode->mNext) {
                        EEUnit* theUnit = theNode->mUnit;
                        if (theUnit == NULL || theUnit == inCenter || !EEIsResourceOfKind(theUnit, inResource, inKind, inFarms)
                            || !inUnit->CanGather(theUnit, theTileX, theTileY, false, false)
                            || std::find(ioUnits->begin(), ioUnits->end(), theUnit) != ioUnits->end()
                            || !((unsigned long)theTileX < gMapWidth) || !((unsigned long)theTileY < gMapHeight)
                            || !gContinentManager.CanGetToUnit(inUnit, theUnit))
                            continue;
                        GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
                        long theUnitX = theUnit->GetTileX();
                        long theUnitY = theUnit->GetTileY();
                        this->FindGatherPoint(inUnit, theUnitX, theUnitY, theUnit->GetUnitClass()->GetTileSize(), theUnit->GetUnitClass()->GetTileSize(),
                                 theLocation);
                        if (UMFloor(theLocation.GetX()) == theUnitX && UMFloor(theLocation.GetY()) == theUnitY)
                            continue;
                        long theDistance;
                        if (theUnit->mTileNode == NULL)
                            theDistance = UMFloor(theUnit->GetTileDistanceSquared(inCenter));
                        else
                            theDistance = UMFloor(theUnit->GetComplexUnit()->DistanceSquared2D(theCenter));
                        EETileLocation theFrom(inUnit->GetTileX(), inUnit->GetTileY());
                        EETileLocation theTo(UMFloor(theLocation.GetX()), UMFloor(theLocation.GetY()));
                        // area ids, not pointers (the original: GetPathAreaIDAt call at 0x60c403, inline id at 0x60c458, cmp bx, ax)
                        if (gContinentManager.GetPathAreaIDAt(theFrom) != gContinentManager.GetPathAreaIDAt(theTo))
                            theDistance += 400;
                        if (theUnit->GetUnitClass()->mFamily == kUnitFamilyAnimal && theUnit->GetHitPoints() == 0)
                            theDistance = 0;
                        if (theDistance < theBest) {
                            theResult = theUnit;
                            theBest = theDistance;
                        }
                    }
                } else {
                    for (; theNode != NULL; theNode = theNode->mNext) {
                        EEUnit* theUnit = theNode->mUnit;
                        if (theUnit == NULL || theUnit == inCenter || (inKind != 1 && theUnit->mResourceType != inKind)
                            || theUnit->GetUnitClass()->GetCategory() != kUnitCategoryNaval
                            || (!inFarms && theUnit->GetUnitClass()->mFamily == kUnitFamilyAnimal)
                            || !inUnit->CanGather(theUnit, theTileX, theTileY, false, false)
                            || !gContinentManager.CanGetToUnit(inUnit, theUnit) || !this->HasGatherPoint(inUnit, theUnit))
                            continue;
                        long theDistance;
                        if (theUnit->mTileNode == NULL)
                            theDistance = UMFloor(theUnit->GetTileDistanceSquared(inCenter));
                        else
                            theDistance = UMFloor(theUnit->GetComplexUnit()->DistanceSquared2D(theCenter));
                        if (theDistance < theBest) {
                            theResult = theUnit;
                            theBest = theDistance;
                        }
                    }
                }
            }
            i++;
        } while (i < inRange);
    }
    return theResult;
}

// TEMPLATE: EXE 0x60c607
// The squared distance from the unit's position to inPoint, y difference first (out of line, 7 callers)
// FUNCTION: EXE 0x60c631
// Finds the next resource of a kind within five times the citizen's sight: around its gather goal's resource first,
// then around the citizen
EEUnit* EEUCBasicCitizen::FindNextResource(EEComplexUnit* inUnit, EEUnit* inResource, unsigned long inKind,
                                   std::vector<EEUnit*>* ioUnits)
{
    EEUnitGoal* theGoal = inUnit->mGoalState;
    long theRange = UMFloatToLong(inUnit->GetLineOfSight() * 5.0f);
    EEUnit* theResult;
    if (theGoal != NULL && theGoal->mKind == kGoalGather) {
        EEUnit* theStorage = static_cast<EEUGGather*>(theGoal)->mResource;
        if (theStorage != NULL && gContinentManager.CanGetToUnit(theStorage, inUnit)) {
            theResult = this->FindResourceOfKind(inUnit, theStorage, inResource, theRange, inKind, ioUnits, true);
            if (theResult != NULL)
                return theResult;
        }
    }
    return this->FindResourceOfKind(inUnit, inUnit, inResource, theRange, inKind, ioUnits, true);
}

// the range from a farm to its storage within which a citizen counts it as the storage's farm (0x446fdc)
// (provisional name)
static const float kCitizenFarmRange = 20.0f;

// FUNCTION: EXE 0x60c6b3
// Returns where the citizen drops its load: its goal's storage, else a farm's nearest drop site within 20, else the
// owner's nearest built drop site that takes the load (docks and navy yards for boats)
EEUnit* EEUCBasicCitizen::FindStorage(EEComplexUnit* inUnit, bool inAnyStorage)
{
    std::vector<EEUnit*> theUnits;
    EEUnit* theUnit = inUnit->GetPlayer()->mFirstUnit;
    bool theFarm = false;
    EEUnitGoal* theGoal = inUnit->mGoalState;
    EEComplexUnit* theResult = NULL;
    EEComplexUnit* theOwn = NULL;
    if (theGoal != NULL && theGoal->mKind == kGoalGather) {
        EEUGGather* theGather = dynamic_cast<EEUGGather*>(theGoal);
        if (!inAnyStorage) {
            theOwn = static_cast<EEComplexUnit*>(theGather->mStorage);
            if (theOwn != NULL && theOwn->GetHitPoints() > 0 && !theOwn->mIsDisabled)
                theResult = theOwn;
        }
        EEUnit* theResource = theGather->mResource;
        if (theResource != NULL && theResource->GetUnitClass()->GetUnitType() == kUnitTypeFarm) {
            EEGetFarmDropSites(theResource->GetComplexUnit(), theUnits);
            for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); ++it) {
                unsigned long theValue = inUnit->mResourceType;
                if (!(*it)->GetComplexUnit()->CanTake(inUnit, theValue)) {
                    inUnit->SetResourceAmount(0);
                    inUnit->SetResourceType((*it)->mResourceType);
                }
                EEUnit* theStorage = *it;
                if (!(theStorage->GetHitPoints() > 0) || !theStorage->GetComplexUnit()->mIsBuilt
                    || theStorage->GetComplexUnit()->mIsDisabled)
                    continue;
                // keep the storage picked so far unless this drop site is nearer
                if (theResult != NULL && inUnit->DistanceSquaredToUnit(theStorage) >= inUnit->DistanceSquaredToUnit(theResult))
                    continue;
                if ((*it)->GetComplexUnit()->DistanceSquaredToUnit(theResource) <= kCitizenFarmRange) {
                    theFarm = true;
                    theResult = (*it)->GetComplexUnit();
                }
            }
        }
    }
    if (inUnit->GetUnitClass()->GetCategory() == kUnitCategoryLandCitizen) {
        if (!theFarm && theUnit != NULL) {
            do {
                EEComplexUnit* theStorage = theUnit->GetComplexUnit();
                if (theStorage != NULL && theStorage->GetHitPoints() > 0 && !theStorage->mIsDisabled
                    && theStorage->GetUnitClass()->IsDropSite() && theStorage != theOwn
                    && theStorage->CanTake(inUnit, inUnit->mResourceType) && theStorage->mIsBuilt) {
                    if (theResult == NULL || inUnit->DistanceSquaredToUnit(theUnit) < inUnit->DistanceSquaredTo(theResult))
                        theResult = theStorage;
                }
                theUnit = static_cast<EEUnit*>(theUnit->mNext);
            } while (theUnit != NULL);
        }
    } else {
        for (; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            EEComplexUnit* theStorage = theUnit->GetComplexUnit();
            if (theStorage != NULL && theStorage->GetHitPoints() > 0 && !theStorage->mIsDisabled
                && (theStorage->GetUnitClass()->GetUnitType() == kUnitTypeDock || theStorage->GetUnitClass()->GetUnitType() == kUnitTypeNavyYard)
                && theStorage != theOwn && theStorage->CanTake(inUnit, inUnit->mResourceType) && theStorage->mIsBuilt) {
                if (theResult == NULL || inUnit->DistanceSquaredToUnit(theUnit) < inUnit->DistanceSquaredTo(theResult))
                    theResult = theStorage;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x60c932
// Finds where the citizen gathers or builds at a footprint: the free tile nearest it (a farm's own tiles when farming,
// corners when needed, not on a wall piece of the type being built), else the footprint's corner; z from the map
void EEUCBasicCitizen::FindGatherPoint(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY, long inSizeX,
                                long inSizeY, GE3DPoint& outLocation)
{
    std::vector<U2DPoint<long> > theTiles;
    EEUCBasicCitizen* theClass = dynamic_cast<EEUCBasicCitizen*>(inUnit->GetUnitClass());
    EEUGGather* theGather = dynamic_cast<EEUGGather*>(inUnit->GetGoal());
    EEUGBuildRepair* theBuild = dynamic_cast<EEUGBuildRepair*>(inUnit->GetGoal());
    long theUnitX = inUnit->mTileX;
    long theUnitY = inUnit->mTileY;
    EEUnit* theFarm;
    if (theGather != NULL && theGather->mGathering && (theFarm = theGather->mResource) != NULL
        && theFarm->mUnitClass->mUnitType == kUnitTypeFarm) {
        for (long x = theFarm->mTileX; x < theFarm->mUnitClass->GetTileSize() + theFarm->mTileX; x++)
            for (long y = theFarm->mTileY; y < theFarm->mUnitClass->GetTileSize() + theFarm->mTileY; y++)
                theTiles.push_back(U2DPoint<long>(x, y));
    } else
        EEGetSquareRing(inX, inY, inSizeX, inSizeY, theTiles);
    std::vector<U2DPoint<long> >::iterator it = theTiles.begin();
    while (it != theTiles.end()) {
        EEMapTile* theTile = gWorld.mMap->GetTile(it->GetX(), it->GetY());
        if (theTile == NULL || theClass->IsTileHeld(theTile) || theTile->mTerrainType == kTerrainCliff)
            it = theTiles.erase(it);
        else
            ++it;
    }
    EEMapTile* theCenter = gWorld.mMap->GetTile(inX, inY);
    bool theWater;
    if (theCenter != NULL && (theCenter->mTerrainType == kTerrainDeepWater || theCenter->mTerrainType == kTerrainShallowWater))
        theWater = true;
    else
        theWater = false;
    if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind != kGoalGather && (theWater || theTiles.size() == 0)) {
        EEGetRingCorners(0, inX, inY, inSizeX, inSizeY, theTiles);
        it = theTiles.begin();
        while (it != theTiles.end()) {
            EEMapTile* theTile = gWorld.mMap->GetTile(it->GetX(), it->GetY());
            if (theTile == NULL || theClass->IsTileHeld(theTile)
                || theWater && (theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater) || theTile->mTerrainType == kTerrainCliff)
                it = theTiles.erase(it);
            else
                ++it;
        }
    }
    if (theBuild != NULL && theBuild->mTargetClass->mIsWall) {
        it = theTiles.begin();
        while (it != theTiles.end()) {
            EEMapTile* theTile = gWorld.mMap->GetTile(it->GetX(), it->GetY());
            bool theErased = false;
            if (theTile != NULL) {
                for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && !theErased; theNode = theNode->mNext) {
                    if (theNode->mUnit != NULL && theNode->mUnit->mUnitClass->mUnitType == theBuild->mTargetType) {
                        it = theTiles.erase(it);
                        theErased = true;
                    }
                }
            }
            if (!theErased)
                ++it;
        }
    }
    if (theTiles.size() == 0) {
        outLocation.SetX((float)(long)inX);
        outLocation.SetY((float)(long)inY);
        outLocation.SetZ(gWorld.mMap->GetTile(inX, inY)->GetHeight());
    } else {
        std::vector<U2DPoint<long> >::iterator theNearest = theTiles.begin();
        long theBest = (theUnitX - theNearest->GetX()) * (theUnitX - theNearest->GetX())
                       + (theUnitY - theNearest->GetY()) * (theUnitY - theNearest->GetY());
        for (it = theTiles.begin() + 1; it != theTiles.end(); ++it) {
            long theDistance = (theUnitY - it->GetY()) * (theUnitY - it->GetY())
                               + (theUnitX - it->GetX()) * (theUnitX - it->GetX());
            if (theDistance < theBest) {
                theNearest = it;
                theBest = theDistance;
            }
        }
        outLocation.SetX((float)theNearest->GetX() + 0.5f);
        outLocation.SetY((float)theNearest->GetY() + 0.5f);
        outLocation.SetZ(gWorld.mMap->GetTile(theNearest->GetX(), theNearest->GetY())->GetHeight());
    }
}

// FUNCTION: EXE 0x60ccb8
// Researches a tech on a unit (idling it unless only goal kinds 1, 2, 7, 10 are queued); if that kills the unit,
// plays the epoch's lightning or disintegration effects and thunder at its position
// (EETechTree::Research for inUnit)
static const float kUnitEffectSoundFalloff = 4.0f;  // 0x446fd8 (EEComputeSoundParameters's last argument)

void EEPlayUnitEffect(EEComplexUnit* inUnit, long inEffect)
{
    GE3DPoint thePosition;
    EEEffectHandle theHandle = {0};
    bool theStop = false;
    float thePan = 1.0f;
    float theVolume = 1.0f;
    float thePitch = 1.0f;
    EESoundObject* theSound;
    if (inUnit->mHitPoints <= 0)
        return;
    for (long i = inUnit->mGoalQueues[0].mGoals.size() - 1; i >= 0 && !theStop; i--) {
        unsigned char theKind = (*(inUnit->mGoalQueues.begin() + 0)).mGoals[i]->mKind;
        if (theKind == 0 || (theKind > kGoalAttackUnit && theKind != kGoalLoadTransport && theKind != kGoalExplore))
            theStop = true;
    }
    inUnit->SetResourceAmount(0);
    if (theStop == true) {
        inUnit->StartAction(kActionIdle);
        inUnit->mUnitClass->ActionChanged(inUnit);
    }
    inUnit->PlayEffect(100000);
    inUnit->SetProductionTime(0);
    inUnit->mPlayer->mTechnologies->Research(inEffect, inUnit);
    if (inUnit->mHitPoints > 0)
        return;
    inUnit->GetPosition(thePosition);
    long theEpoch = inUnit->mPlayer->mTechnologies->mCurrentEpoch;
    switch (theEpoch) {
    case kTechEpochPrehistoric:
    case kTechEpochStone:
    case kTechEpochCopper:
    case kTechEpochBronze:
        theSound = EESoundObject::Get(kSoundLightning);
        gWorld.GetEffectManager()->CreatePlacedEffect(thePosition, EEGraphicEffect::Get(kGraphicEffectLightningUnit), &theHandle);
        gWorld.GetEffectManager()->CreatePlacedEffect(thePosition, EEGraphicEffect::Get(kGraphicEffectDustEffect), &theHandle);
        gWorld.GetEffectManager()->CreatePlacedEffect(thePosition, EEGraphicEffect::Get(kGraphicEffectDustEffect), &theHandle);
        gWorld.GetEffectManager()->CreatePlacedEffect(thePosition, EEGraphicEffect::Get(kGraphicEffectDustEffect), &theHandle);
        if (theSound == NULL)
            return;
        EEComputeSoundParameters(1.0f, thePosition, theVolume, thePan, thePitch, kUnitEffectSoundFalloff);
        theSound->Play(theVolume, thePan, thePitch);
        if (inUnit->mUnitID & 1)
            return;
        break;
    case kTechEpochDark:
    case kTechEpochMiddle:
    case kTechEpochRenaissance:
    case kTechEpochImperial:
    case kTechEpochIndustrial:
        theSound = EESoundObject::Get(kSoundLightning);
        gWorld.GetEffectManager()->CreatePlacedEffect(thePosition, EEGraphicEffect::Get(kGraphicEffectDisintegrationAreaEffectIntermediate), &theHandle);
        gWorld.GetEffectManager()->CreatePlacedEffect(thePosition, EEGraphicEffect::Get(kGraphicEffectDustEffect), &theHandle);
        if (theSound == NULL)
            return;
        EEComputeSoundParameters(1.0f, thePosition, theVolume, thePan, thePitch, kUnitEffectSoundFalloff);
        theSound->Play(theVolume, thePan, thePitch);
        if (inUnit->mUnitID % 3 != 0)
            return;
        break;
    case kTechEpochAtomicWW1:
    case kTechEpochAtomicWW2:
    case kTechEpochAtomicModern:
        theSound = EESoundObject::Get(kSoundLightning);
        gWorld.GetEffectManager()->CreatePlacedEffect(thePosition, EEGraphicEffect::Get(kGraphicEffectDisintegrationAreaEffectIntermediate), &theHandle);
        gWorld.GetEffectManager()->CreatePlacedEffect(thePosition, EEGraphicEffect::Get(kGraphicEffectDustEffect), &theHandle);
        if (theSound == NULL)
            return;
        EEComputeSoundParameters(1.0f, thePosition, theVolume, thePan, thePitch, kUnitEffectSoundFalloff);
        thePitch = max(-1.0f, thePitch - 0.2f);
        theSound->Play(theVolume, thePan, thePitch);
        return;
    default:
        theSound = EESoundObject::Get(kSoundLightning);
        gWorld.GetEffectManager()->CreatePlacedEffect(thePosition, EEGraphicEffect::Get(kGraphicEffectDisintegrationAreaEffectIntermediate), &theHandle);
        gWorld.GetEffectManager()->CreatePlacedEffect(thePosition, EEGraphicEffect::Get(kGraphicEffectDustEffect), &theHandle);
        if (theSound == NULL)
            return;
        EEComputeSoundParameters(1.0f, thePosition, theVolume, thePan, thePitch, kUnitEffectSoundFalloff);
        theSound->Play(theVolume, thePan, thePitch);
        return;
    }
    theSound = EESoundObject::Get(kSoundThunder2);
    if (theSound == NULL)
        return;
    EEComputeSoundParameters(1.0f, thePosition, theVolume, thePan, thePitch, kUnitEffectSoundFalloff);
    theSound->Play(theVolume, thePan, thePitch);
}

// FUNCTION: EXE 0x60d10e
// Answers the citizen triggers: starts the next queued build, build queue empty, smart task not assigned (true when
// it has a goal), goal is repair; gather and build triggers go to the active goal (2 when not the main goal)
long EEUCBasicCitizen::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    bool theTrue = false;
    long theResult = 0;
    switch (inEvent) {
    default:
        theResult = EEUnitClass::EvaluateCondition(inUnit, inEvent);
        break;
    case kTriggerBuildingInBuildQueue:
        if ((long)inUnit->mCitizenBuildQueue.size() > 0) {
            EEUnit* theSite = inUnit->PopBuildTarget();
            if (theSite != NULL) {
                inUnit->SetGoal(new EEUGBuildRepair(inUnit->GetPlayer(), theSite, true), 0, 0);
                inUnit->StartAction(kActionMoveToRepairSite);
                theTrue = true;
            }
        }
        break;
    case kTriggerBuildQueueEmpty:
        theTrue = inUnit->mCitizenBuildQueue.size() == 0;
        break;
    case kTriggerSmartTaskNotAssigned:
        theTrue = inUnit->mGoalState == NULL;
        break;
    case kTriggerGoalIsRepairSite:
        if (inUnit->mActiveGoal != NULL && inUnit->mActiveGoal->mKind == kGoalBuildRepair)
            theTrue = true;
        else
            theTrue = false;
        break;
    case kTriggerBuildingDestroyed:
    case kTriggerBuildingRepaired:
    case kTriggerResourceCapacityFilled:
    case kTriggerResourceCapacityEmpty:
    case kTriggerNewResourceFound:
    case kTriggerResourceDepleted:
    case kTriggerResourceInRange:
    case kTriggerStorageInRange:
    case kTriggerStorageFound:
    case kTriggerRepairSiteInRange:
    case kTriggerStorageDestroyed:
    case kTriggerBuildSiteClear:
    case kTriggerBuildSiteVisible:
    case kTriggerBuildSiteValid:
    case kTriggerResourceAccessible:
    case kTriggerWaitingAreaInRange:
    case kTriggerResourceTargetIsTree:
    case kTriggerOpenPositionAvailable:
    case kTriggerGoalIsBuild:
    case kTriggerGoalIsResource:
    case kTriggerGoalIsStorage:
    case kTriggerGoalIsWaitingArea: {
        EEUnitGoal* theGoal = inUnit->mActiveGoal;
        if (theGoal != NULL && theGoal->EvaluateCondition(inUnit, inEvent))
            theResult = (theGoal != inUnit->mGoalState) + 1;
        break;
    }
    }
    if (theTrue)
        theResult = 1;
    return theResult;
}

// FUNCTION: EXE 0x60d2dc
// Returns whether a tile holds a unit that blocks a citizen (not a farm, robotic farm, path point, ambient or
// foundation)
bool EEUCBasicCitizen::IsTileHeld(EEMapTile* inTile)
{
    bool theResult = false;
    for (RTSTileUnitNode* theNode = inTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
        EEUnit* theUnit = theNode->mUnit;
        if (theUnit == NULL)
            continue;
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        long theType = theUnit->mUnitClass->mUnitType;
        if (theType != kUnitTypeFarm && theType != kUnitTypeRoboticFarm && theType != kUnitTypePathPoint && theUnit->mUnitClass->mCategory != kUnitCategoryAmbient
            && (theComplex == NULL || !theComplex->IsFoundation())) {
            theResult = true;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x60d336
// Updates a living citizen's water ripples when the effect detail allows them
void EEUCBasicCitizen::UpdateEffects(EEComplexUnit* inUnit)
{
    if (this->mRippleEffect != NULL && inUnit->mHitPoints != 0 && (unsigned long)this->mRippleEffect->mDetailLevel <= gEffectDetail)
        this->UpdateRipples(inUnit);
}

// FUNCTION: EXE 0x60d361
// Runs the work timer (working the gather or repair goal when it expires), then finishes a due unit research
// (instant build: at once); a kind-3 tech tied to the Bundeswehr power goes through EEPlayUnitEffect
void EEUCBasicCitizen::UpdateCitizen(EEComplexUnit* inUnit)
{
    long theTimer = inUnit->mWorkTimer;
    long theStep = 0;
    if (theTimer > 0) {
        theStep = gPhysics->mTickTimeMs;
        if (theStep == 0)
            theStep = 1;
        theStep = theTimer - theStep;
        if (theStep < 1)
            inUnit->SetWorkTimer(0);
        else
            inUnit->SetWorkTimer(theStep);
    }
    if (inUnit->mWorkTimer == 0) {
        long theAction = inUnit->mCommand ? inUnit->mCommand->mType : 0;
        if (theAction == kActionRepairBuilding || theAction == kActionGatherResource) {
            if (inUnit->mGoalState != NULL)
                inUnit->mGoalState->Work(inUnit);
            theTimer = inUnit->mWorkTimer;
            if (theTimer > 0) {
                if (-theStep < theTimer)
                    theStep = theTimer + theStep;
                else
                    theStep = 1;
                inUnit->SetWorkTimer(theStep);
            }
        }
    }
    inUnit->Fn1FA98C();
    if (inUnit->mCurrentResearch == kTechNone)
        return;
    long theFrame = gPhysics->mTickTimeMs;
    if (theFrame == 0)
        theFrame = 1;
    long theLeft = inUnit->mProductionTime - theFrame;
    if (inUnit->mPlayer->mInstantBuild) {
        if (theLeft < 0)
            goto done;
        theLeft = -1;
    }
    if (theLeft > 0)
        return;
done:
    inUnit->SetProductionTime(0);
    EETechTreeNode* theNode = inUnit->mPlayer->mTechnologies->Get(inUnit->mCurrentResearch);
    if (theNode != NULL && !theNode->mData->mMakesUnit) {
        long theTechnology = inUnit->mCurrentResearch;
        bool theResearch = true;
        EECivPower thePower;
        if (theNode->mData->mKind == 3 && gCivPowersManager.GetPower(kCivPowerBundeswehr, thePower)
            && thePower.mTechnology == theNode->mData->mPrerequisite1) {
            EEPlayUnitEffect(inUnit, theTechnology);
            theResearch = false;
        }
        inUnit->PlayEffect(100000);
        inUnit->SetProductionTime(0);
        if (theResearch)
            inUnit->mPlayer->mTechnologies->Research(theTechnology, inUnit);
    }
}

// FUNCTION: EXE 0x60d4f4
// Returns whether the citizen has a gather point beside a resource (not just the resource's own corner tile)
bool EEUCBasicCitizen::HasGatherPoint(EEComplexUnit* inUnit, EEUnit* inResource)
{
    GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
    long theX = inResource->mTileX;
    long theY = inResource->mTileY;
    this->FindGatherPoint(inUnit, theX, theY, inResource->mUnitClass->GetTileSize(), inResource->mUnitClass->GetTileSize(), theLocation);
    return !(UMFloor(theLocation.GetX()) == theX && UMFloor(theLocation.GetY()) == theY);
}
