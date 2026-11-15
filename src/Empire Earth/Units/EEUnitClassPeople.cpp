// EEUnitClassPeople.cpp (new, R5b.4 round 32): the citizens and priests' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <typeinfo>
#include <string.h>

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
#include "lle/GEBoundingBox.h"
#include "lle/GEModel.h"
#include "lle/SESoundManager.h"

#include "ee/EEUnitUpdate.h"
#include "ee/EEUnitKind.h"
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
#include "ee/EEUGRust.h"
#include "ee/EEAIRandom.h"
#include "ee/EEAIArea.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EERules.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPIntelligence.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEE2DBattlefieldAmbient.h"
#include "ee/EESoundObject.h"
#include "ee/EEEffectManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitClassKinds.h"
#include "rust/ee_units.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEUnitAction.h"
#include "ee/EESimCapture.h"
#include "rust/ee_combat.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EECombatFormulas.h"
#include "ee/EEUnitUtilities.h"

// ---- EEUCBasicCitizen.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The citizens'
// unit class: gathering (resource and storage searches), building and repairing. 0x20bcf7..0x20d571 (after
// EEUCAmbient.cpp; EEUCBasicPriest.cpp follows).

// the default epoch media (EERules.cpp EEDefaultEpochMedia) name these rows
static_assert(kSoundLightning == 0x176 && kSoundThunder2 == 0x1ba && kGraphicEffectLightningUnit == 0x15 &&
              kGraphicEffectDustEffect == 0xab && kGraphicEffectDisintegrationAreaEffectIntermediate == 0xec,
              "EERules.cpp's epoch media rows");

// 0x243c7d EEBlockTakes: ee/EEAreaUtilities.h

// 0x247021: the tiles around a footprint (inSizeX by inSizeY at (inX, inY)) (provisional name; arguments inferred)
void EEGetRingCorners(long inArg1, long inX, long inY, long inSizeX, long inSizeY, std::vector<U2DPoint<long> >& outTiles);

// 0x24697b: the tiles of a footprint (inSizeX by inSizeY at (inX, inY)) (provisional name; arguments inferred)
void EEGetSquareRing(long inX, long inY, long inSizeX, long inSizeY, std::vector<U2DPoint<long> >& outTiles);

// FUNCTION: EXE 0x60be58
// Searches rings 2..9 around the citizen for a tile on its continent where a unit of the given type fits; returns
// whether one was found
bool EEUnitClass::FindWoodSite(EEComplexUnit* inUnit, long inUnitType, U2DPoint<float>& outLocation)
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
        EEGetSquareRingTiles(theX, theY, theRadius, theTiles);
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
EEUnit* EEUnitClass::FindResourceOfKind(EEComplexUnit* inUnit, EEUnit* inCenter, EEUnit* inResource, long inRange,
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
                        if (theUnit->mTileNode != NULL)
                            theDistance = UMFloor(theUnit->mComplexUnit->DistanceSquared2D(theCenter));
                        else
                            theDistance = UMFloor(theUnit->GetTileDistanceSquared(inCenter));
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
                            theDistance = UMFloor(theUnit->mComplexUnit->DistanceSquared2D(theCenter));
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
EEUnit* EEUnitClass::FindNextResource(EEComplexUnit* inUnit, EEUnit* inResource, unsigned long inKind,
                                   std::vector<EEUnit*>* ioUnits)
{
    EEUnitGoal* theGoal = inUnit->mGoalState;
    long theRange = UMFloatToLong(inUnit->GetLineOfSight() * 5.0f);
    EEUnit* theResult;
    if (theGoal != NULL && theGoal->mKind == kGoalGather) {
        EEUnit* theStorage = static_cast<EEUGRust*>(theGoal)->GetGatherResource();
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
EEUnit* EEUnitClass::FindStorage(EEComplexUnit* inUnit, bool inAnyStorage)
{
    std::vector<EEUnit*> theUnits;
    EEUnit* theUnit = inUnit->GetPlayer()->mFirstUnit;
    bool theFarm = false;
    EEUnitGoal* theGoal = inUnit->mGoalState;
    EEComplexUnit* theResult = NULL;
    EEComplexUnit* theOwn = NULL;
    if (theGoal != NULL && theGoal->mKind == kGoalGather) {
        EEUGRust* theGather = EEGatherGoalCast(theGoal);
        if (!inAnyStorage) {
            theOwn = static_cast<EEComplexUnit*>(theGather->GetGatherStorage());
            if (theOwn != NULL && theOwn->GetHitPoints() > 0 && !theOwn->mIsDisabled)
                theResult = theOwn;
        }
        EEUnit* theResource = theGather->GetGatherResource();
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
void EEUnitClass::FindGatherPoint(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY, long inSizeX,
                                long inSizeY, GE3DPoint& outLocation)
{
    std::vector<U2DPoint<long> > theTiles;
    EEUnitClass* theClass = EEUnitClassOfKind<EE_CLASS_KIND_BASIC_CITIZEN>(inUnit->GetUnitClass());
    EEUGRust* theGather = EEGatherGoalCast(inUnit->GetGoal());
    EEUGRust* theBuild = EEBuildRepairGoalCast(inUnit->GetGoal());
    long theUnitX = inUnit->mTileX;
    long theUnitY = inUnit->mTileY;
    EEUnit* theFarm;
    if (theGather != NULL && theGather->IsGathering() && (theFarm = theGather->GetGatherResource()) != NULL
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
    if (theBuild != NULL && theBuild->GetBuildTargetClass()->mIsWall) {
        it = theTiles.begin();
        while (it != theTiles.end()) {
            EEMapTile* theTile = gWorld.mMap->GetTile(it->GetX(), it->GetY());
            bool theErased = false;
            if (theTile != NULL) {
                for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && !theErased; theNode = theNode->mNext) {
                    if (theNode->mUnit != NULL && theNode->mUnit->mUnitClass->mUnitType == theBuild->GetBuildTargetType()) {
                        it = theTiles.erase(it);
                        theErased = true;
                    }
                }
            }
            if (!theErased)
                ++it;
        }
    }
    if (theTiles.size() != 0) {
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
    } else {
        outLocation.SetX((float)(long)inX);
        outLocation.SetY((float)(long)inY);
        outLocation.SetZ(gWorld.mMap->GetTile(inX, inY)->GetHeight());
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
    // the epoch's removal media (epochs.yaml citizen; the original's switch on the epoch: lightning and dust up to the
    // bronze age, a thunder for every second unit; disintegration later, a thunder for every third up to the
    // industrial age, a lower pitch in the atomic ages)
    const EEEpochMedia& theMedia = EEGetRules().Epoch(inUnit->mPlayer->mTechnologies->mCurrentEpoch);
    theSound = EESoundObject::Get(theMedia.mRemovalSound);
    for (long theEffect : theMedia.mRemovalEffects)
        gWorld.GetEffectManager()->CreatePlacedEffect(thePosition, EEGraphicEffect::Get(theEffect), &theHandle);
    if (theSound == NULL)
        return;
    EEComputeSoundParameters(1.0f, thePosition, theVolume, thePan, thePitch, kUnitEffectSoundFalloff);
    if (theMedia.mRemovalPitchOffset != 0.0f)
        thePitch = max(-1.0f, thePitch + theMedia.mRemovalPitchOffset);
    theSound->Play(theVolume, thePan, thePitch);
    if (theMedia.mRemovalThunderEvery == 0 || inUnit->mUnitID % theMedia.mRemovalThunderEvery != 0)
        return;
    theSound = EESoundObject::Get(theMedia.mRemovalThunder);
    if (theSound == NULL)
        return;
    EEComputeSoundParameters(1.0f, thePosition, theVolume, thePan, thePitch, kUnitEffectSoundFalloff);
    theSound->Play(theVolume, thePan, thePitch);
}

// FUNCTION: EXE 0x60d10e
// Answers the citizen triggers: starts the next queued build, build queue empty, smart task not assigned (true when
// it has a goal), goal is repair; gather and build triggers go to the active goal (2 when not the main goal)
long EEUnitClass::BasicCitizenEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    bool theTrue = false;
    long theResult = 0;
    switch (inEvent) {
    default:
        theResult = this->BaseEvaluateCondition(inUnit, inEvent);
        break;
    case kTriggerBuildingInBuildQueue:
        if ((long)inUnit->mCitizenBuildQueue.size() > 0) {
            EEUnit* theSite = inUnit->PopBuildTarget();
            if (theSite != NULL) {
                inUnit->SetGoal(EEUGRust::NewBuildRepair(inUnit->GetPlayer(), theSite, true), 0, 0);
                inUnit->StartAction(kActionMoveToRepairSite);
                theTrue = true;
            }
        }
        break;
    case kTriggerBuildQueueEmpty:
        theTrue = inUnit->mCitizenBuildQueue.size() == 0;
        break;
    case kTriggerSmartTaskNotAssigned:
        // True while the citizen still has a goal, as in the original: "not assigned" means the smart task gave it
        // no new job, so it still holds its finished build goal. A smart task that found work has already moved the
        // citizen out of TaskSmartCitizen (FindNextWork starts an action), so this only fires for the leftovers, whose
        // Idle clears the stale goal. Inverting it (== NULL) left every such citizen in TaskSmartCitizen for good,
        // still in its work animation beside the finished building.
        theTrue = inUnit->mGoalState != NULL;
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
bool EEUnitClass::IsTileHeld(EEMapTile* inTile)
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

// FUNCTION: EXE 0x60d361
// Runs the work timer (working the gather or repair goal when it expires), then finishes a due unit research
// (instant build: at once); a kind-3 tech tied to the Bundeswehr power goes through EEPlayUnitEffect
void EEUnitClass::UpdateCitizen(EEComplexUnit* inUnit)
{
    long theTimer = inUnit->mWorkTimer;
    long theStep = 0;
    if (theTimer > 0) {
        theStep = gPhysics->mTickTimeMs;
        if (theStep == 0)
            theStep = 1;
        theStep = theTimer - theStep;
        if (theStep > 0)
            inUnit->SetWorkTimer(theStep);
        else
            inUnit->SetWorkTimer(0);
    }
    if (inUnit->mWorkTimer == 0) {
        long theAction = inUnit->mCommand ? inUnit->mCommand->mType : 0;
        switch (theAction) {
        case kActionRepairBuilding:
        case kActionGatherResource:
            if (inUnit->mGoalState != NULL)
                inUnit->mGoalState->Work(inUnit);
            theTimer = inUnit->mWorkTimer;
            if (theTimer > 0) {
                inUnit->SetWorkTimer(inUnit->mWorkTimer > -theStep ? inUnit->mWorkTimer + theStep : 1);
            }
            break;
        }
    }
    inUnit->UpdateFanaticismColor();
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
bool EEUnitClass::HasGatherPoint(EEComplexUnit* inUnit, EEUnit* inResource)
{
    GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
    long theX = inResource->mTileX;
    long theY = inResource->mTileY;
    this->FindGatherPoint(inUnit, theX, theY, inResource->mUnitClass->GetTileSize(), inResource->mUnitClass->GetTileSize(), theLocation);
    return !(UMFloor(theLocation.GetX()) == theX && UMFloor(theLocation.GetY()) == theY);
}

// ---- EEUCLandCitizen.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The land citizen's unit
// class: 0x216c48..0x216d4b (after EEUCLandable.cpp; EEUCLandMilitaryHero.cpp follows). One unit per EEUC* class in
// alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); VirtualB8,
// then EEComplexUnit::ContinueProduction after its first user.

// FUNCTION: EXE 0x616d2d
// Starts the next production when nothing is being produced and the build queue is not empty
void EEComplexUnit::ContinueProduction()
{
    if (this->mProductionTime == 0 && this->GetBuildQueueSize() != 0)
        this->StartProduction();
}

// ---- EEUCNavalCitizen.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The naval citizen's
// unit class (a fishing boat): 0x218490..0x21880c (after EEUCNavalBuilding.cpp; EEUCNavalPriest.cpp's out-of-line
// ctor 0x21880c follows). Every slot of ee/EEUCNavalCitizen.h in that range is here: +0x08 (ICF-shared with
// EEUCNaval/EEUCNavalBuilding/EEUCNavalTransport), +0x0c, +0x30 (ICF-shared), +0x74, +0xa0, +0xb8. The ctor is
// inline (EEUnitClass.cpp's factory). Unit flags: the exe's module set.

// FUNCTION: EXE 0x618490
// Returns the tile under a location along the camera line, at sea level
void EEUnitClass::NavalCitizenLocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
{
    GE3DPoint thePoint;
    GE3DLine theLine(::gWorld.mLine);
    theLine.SetStart(inLocation);
    float theT;
    theLine.SolveForZ(0.0f, theT, thePoint);
    outX = UMFloor(thePoint.GetX());
    outY = UMFloor(thePoint.GetY());
}

// FUNCTION: EXE 0x618539
// Thins the boat's path to every n-th point and both ends when inSmooth is set (n: inner collision sphere, min 1)
void EEUnitClass::NavalCitizenAdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth)
{
    std::vector<GE3DPoint> thePath;
    long theCount = ioPath.size();
    long theStep = UMFloatToLong(this->mInnerCollisionSphere);
    if (inSmooth != true)
        return;
    thePath.reserve(ioPath.size());
    if (theStep <= 0)
        theStep = 1;
    for (long i = 0; i < theCount; i++) {
        if (i == theCount - 1 || i == 0 || i % theStep == 0)
            thePath.push_back(ioPath[i]);
    }
    ioPath = thePath;
}

// FUNCTION: EXE 0x618605
// Creates a fishing boat on a tile at sea level
EEUnit* EEUnitClass::NavalCitizenCreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    GETransformation theTransformation;
    theTransformation.SetX((float)inX);
    theTransformation.SetY((float)inY);
    theTransformation.SetZ(0.0f);
    return this->BaseCreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// FUNCTION: EXE 0x618664
// Answers dying done: true once the boat's transformed bounding box lies wholly below the water line
long EEUnitClass::NavalCitizenEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    GE3DPoint thePosition;
    GEBoundingBox theBox;
    GEBoundingBox theTransformedBox;
    EEComplexUnit* theUnit = inUnit;
    const GEPhysicalModel& thePhysicalModel = theUnit->mPhysicalModel;
    GEModel* theModel = thePhysicalModel.GetModel();
    long theResult = 0;
    if (inEvent != kTriggerDyingAnimationFinished)
        theResult = this->BasicCitizenEvaluateCondition(theUnit, inEvent);
    else {
        theUnit->GetPosition(thePosition);
        GETransformation theTransformation(false);
        thePhysicalModel.GetLocation().Apply(theModel->GetInternalTransform(thePhysicalModel.GetFrame()),
                                             theTransformation);
        float theScale = thePhysicalModel.GetScale();
        theTransformation.Scale(theScale, theScale, theScale);
        theTransformation.SetX(thePosition.GetX());
        theTransformation.SetY(thePosition.GetY());
        theTransformation.SetZ(thePosition.GetZ());
        theBox = theModel->GetBoundingBox(theUnit->mPhysicalModel.GetFrame());
        theBox.Transform(theTransformation, theTransformedBox);
        if (theTransformedBox.GetTopRightBack().GetZ() < 0.0f)
            theResult = 1;
    }
    return theResult;
}

// ---- EEUCBasicPriest.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The priests' common
// unit class (conversion): 0x20d571..0x20e2cb (after EEUCBasicCitizen.cpp; EEUCBomb.cpp follows). Unit flags: the
// exe's module set. Order: the conversion cost (static), UpdateConversion (command 0x7d4), ConvertTarget (the convert), Virtual74,
// Virtual78, the can-convert test (static), VirtualA8, VirtualA0, VirtualB4, VirtualB8, then EEComplexUnit's
// helpers that VirtualB8 calls (COMDATs after their first user: inline in the real header? written out of line
// here), ShowConversionEffects, HideConversionEffects.

void EEComputeSoundParameters(float inVolume, const GE3DPoint& inLocation, float& outVolume, float& outPan,
                              float& outPitch, float inFactor);

static const float kBasicPriestNoHeight = -10000.0f;   // 0x446fe0 (provisional name)

static const float kBasicPriestSoundFactor = 4.0f;     // 0x446fe4 (the conversion sound's distance factor)

// FUNCTION: EXE 0x60d571
// Returns the faith a conversion by a unit costs: the world table's entry 46, at most the unit's own power (0 if none)
long EEGetConversionCost(EEComplexUnit* inUnit)
{
    EeCombatConversionCostIn theIn = {(int32_t)gWorldTable.GetLongValue(kWorldConversionFaithCost),
                                      (int32_t)inUnit->mStats->mPower};
    return EECombatConversionCost(theIn);
}

// FUNCTION: EXE 0x60d59f
// Works a conversion with enough faith: stops at an ally or own unit, shows the effects and warns the target's owner,
// then after the minimum time tries once a second (resistance, owner factor, rate) and converts on success
void EEUnitClass::BasicPriestUpdateConversion(EEComplexUnit* inUnit)
{
    if (inUnit->GetPower() < EEGetConversionCost(inUnit))
        return;
    EEComplexUnit* theTarget = static_cast<EEComplexUnit*>(EEAttackUnitCast(inUnit->GetGoal())->GetAttackTarget());
    if (theTarget->mPlayer == inUnit->mPlayer || inUnit->mPlayer->mEnemies[theTarget->mPlayer->GetPlayerID()] == 0) {
        inUnit->ReleaseConversionEffect();
        inUnit->ClearGoals(0);
        this->BasicPriestHideConversionEffects(inUnit);
        if (inUnit->mGoalState == NULL)
            inUnit->StartAction(kActionIdle);
        this->ActionChanged(inUnit);
        return;
    }
    if (UIsNone32(inUnit->mConversionEffect.mValue)) {
        EEEffectManager* theManager = gWorld.GetEffectManager();
        EEEffectHandle theHandle;
        if (theManager->CreateUnitEffect(theTarget, EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator), &theHandle, 0, 0, 0) == 0 &&
            inUnit->mConversionEffect != theHandle)
            inUnit->mConversionEffect = theHandle;
        this->BasicPriestShowConversionEffects(inUnit);
        EETileLocation theFrom(inUnit->mTileX, inUnit->mTileY);
        EETileLocation theTile(theTarget->mTileX, theTarget->mTileY);
        theTarget->mPlayer->mEvents.AddPlayerEvent(theTile, theFrom, 1, theTarget->GetPlayer(), -1, false);
    }
    if (::gWorld.mInCinematic)
        return;
    if (gPhysics->mGameTime - inUnit->mConversionStartTime < (unsigned long)this->mMinConversionTime)
        return;
    EeCombatConversionIn theIn;
    theIn.work_timer = (int32_t)inUnit->mWorkTimer;
    theIn.tick_seconds = gPhysics->mTickSeconds;
    theIn.resistance = theTarget->mStats->mConversionResistance;
    theIn.adaptation = theIn.resistance == 1.0f && gCivPowersManager.HasPower(inUnit->GetPlayer(), kCivPowerAdaptation) &&
                       !inUnit->mUnitClass->IsCrusader() && inUnit->mUnitClass->mCategory != kUnitCategoryBuildingPriest &&
                       theTarget->mUnitClass->mUnitType == kUnitTypeTownCenter;
    theIn.rate = inUnit->mStats->mConversionRate;
    theIn.factor = theTarget->mPlayer->mConversionFactor;
    EeCombatConversionOut theOut = EECombatConversion(theIn);
    long theTime = theOut.timer;
    if (theOut.due) {
        if (gPhysics->mRandom.Generate() <= theOut.threshold &&
            (theTarget->mTileNode == NULL || !theTarget->GetComplexUnit()->mIsDisabled || theTarget->GetComplexUnit()->IsProtected()))
            this->BasicPriestConvertTarget(inUnit, theTarget);
        else
            inUnit->SetWorkTimer(theOut.miss_timer);
    } else
        inUnit->SetWorkTimer(theTime);
}

// FUNCTION: EXE 0x60d7f2
// Converts the target to the priest's player (with area conversion, also every convertible visible unit nearby),
// spends the faith and plays the converted unit's creation sound for the two players involved
void EEUnitClass::BasicPriestConvertTarget(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    bool theFlag;
    std::vector<EEUnit*> theUnits;
    GE3DPoint theLocation;
    EEPlayer* theOldPlayer = inTarget->mPlayer;
    inUnit->ReleaseConversionEffect();
    this->BasicPriestHideConversionEffects(inUnit);
    EEComplexUnit* theConverted = EEConvertUnit(inTarget, inUnit->mPlayer->GetPlayerID(), 1);
    EEUnitClassUpgrade* theStats = theConverted->mStats;
    if (inUnit->mPlayer->mAreaConversion == true) {
        const GETransformation& theConvertedLocation = theConverted->mPhysicalModel.GetLocation();
        theLocation.SetX(theConvertedLocation.GetX());
        theLocation.SetY(theConvertedLocation.GetY());
        theLocation.SetZ(theConvertedLocation.GetZ());
        EEGetUnitsNear(theLocation, gWorldTable.GetFloatValue(kWorldAreaConversionRadius), theUnits);
        for (long i = theUnits.size() - 1; i >= 0; i--) {
            EEUnit* theUnit = theUnits[i];
            if (theUnit->GetComplexUnit() != NULL && ((EEMapTile*)theUnit->mTile)->IsSeenBy(inUnit->GetPlayer()) == true) {
                if (EECanConvert(inUnit, theUnit->GetComplexUnit(), theFlag))
                    EEConvertUnit(static_cast<EEComplexUnit*>(theUnit), inUnit->mPlayer->GetPlayerID(), 1);
            }
        }
    }
    inUnit->SetPower(inUnit->mPower - EEGetConversionCost(inUnit));
    if (inUnit->mConversionStartTime != 0)
        inUnit->mConversionStartTime = 0;
    EESoundObject* theSound = theStats->mCreationSound;
    if (theSound == NULL)
        return;
    EEPlayer* theCurrent = ::gWorld.mPlayers[::gWorld.mCurrentPlayer];
    if (inUnit->mPlayer == theCurrent || theOldPlayer == theCurrent)
        gSoundManager->Play(theSound->mSampleID, 1.0f, 0.0f, 1.0f, false, NULL);
}

// FUNCTION: EXE 0x60da86
// Returns whether a priest can convert a target now (enemy, resistance, faith, priests and buildings rules, the
// temple's range); outFlag is set when the target is shielded
// (new: the decision is ee_combat's can_convert over a record of pure reads, gathered here; the damage table is
// read only for a valid ammo column, the shield and the building priest's distance always: none has side effects)
bool EECanConvert(EEComplexUnit* inPriest, EEComplexUnit* inTarget, bool& outFlag)
{
    long theCost = EEGetConversionCost(inPriest);
    EEUnitClass* theClass = inTarget->mUnitClass;
    EEUnitClass* thePriestClass = inPriest->mUnitClass;
    EeCombatCanConvertIn in = {};
    in.crusader = thePriestClass->IsCrusader();
    in.building_priest = thePriestClass->mCategory == kUnitCategoryBuildingPriest;
    in.adaptation = gCivPowersManager.HasPower(inPriest->GetPlayer(), kCivPowerAdaptation);
    in.same_player = inTarget->mPlayer == inPriest->mPlayer;
    in.enemy = inPriest->mPlayer->mEnemies[inTarget->mPlayer->GetPlayerID()] != 0;
    in.conversion_rate = inPriest->mStats->mConversionRate;
    in.target_disabled = inTarget->mIsDisabled;
    in.target_protected = inTarget->IsProtected();
    in.resistance = inTarget->mStats->mConversionResistance;
    in.target_town_center = theClass->mUnitType == kUnitTypeTownCenter;
    in.target_settlement = theClass->mUnitType == kUnitTypeSettlement;
    in.target_hit_points = (int32_t)inTarget->mHitPoints;
    in.attackable = theClass->CanBeAttacked();
    in.damage = thePriestClass->mAmmoType >= 0 ? EEDamageFactor(theClass->mFamily, thePriestClass->mAmmoType) : 0.0f;
    in.same_category = theClass->mCategory == thePriestClass->mCategory;
    in.converts_priests = inPriest->mStats->mConvertsPriests;
    in.power = (int32_t)inPriest->GetPower();
    in.cost = (int32_t)theCost;
    in.target_land_priest = theClass->mCategory == kUnitCategoryLandPriest;
    in.converts_buildings = inPriest->mStats->mConvertsBuildings;
    in.target_land_building = theClass->IsLandBuilding();
    in.at_copper = inPriest->mPlayer->mTechnologies->mCurrentEpoch >= kTechEpochCopper;
    in.priest_x = inPriest->mPhysicalModel.GetLocation().GetX();
    in.priest_y = inPriest->mPhysicalModel.GetLocation().GetY();
    in.priest_z = inPriest->mPhysicalModel.GetLocation().GetZ();
    in.target_x = inTarget->mPhysicalModel.GetLocation().GetX();
    in.target_y = inTarget->mPhysicalModel.GetLocation().GetY();
    in.target_z = inTarget->mPhysicalModel.GetLocation().GetZ();
    in.range = inPriest->mStats->mRange;
    in.priest_cyber = thePriestClass->mIsCyber;
    in.target_cyber = theClass->mIsCyber;
    in.shielded = inTarget->HasEffect(4);
    EeCombatCanConvertOut theOut = ee_combat_can_convert(&in);
    if (theOut.flag_written)
        outFlag = theOut.flag != 0;
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'C', 'N'), in, theOut);
    return theOut.result != 0;
}

// FUNCTION: EXE 0x60dd0a
// Loads the conversion indicator effect and the class's media
void EEUnitClass::BasicPriestLoadMedia(EEPlayer* inPlayer)
{
    EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator)->Load(false);
    this->BaseLoadMedia(inPlayer);
}

// FUNCTION: EXE 0x60dd2d
// Answers the conversion triggers (attacker convertible, enough faith, goal still valid, goal is or is not a
// conversion, target converted); the faith check uses the attacker's conversion cost
long EEUnitClass::BasicPriestEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    switch (inEvent) {
    default:
        theResult = this->BaseEvaluateCondition(inUnit, inEvent);
        break;
    case kTriggerAttackerIsLegalConversionTarget: {
        bool theFlag;
        theResult = inUnit->mLastAttacker != NULL && EECanConvert(inUnit, static_cast<EEComplexUnit*>(inUnit->mLastAttacker), theFlag) ? 1 : 0;
        break;
    }
    case kTriggerHasEnoughManaForConversion:
        theResult = inUnit->mLastAttacker != NULL && inUnit->GetPower() >= EEGetConversionCost(inUnit) ? 1 : 0;
        break;
    case kTriggerConvertGoalStillValid:
        if (inUnit->mGoalState != NULL) {
            EEUGRust* theGoal = EEAttackUnitCast(inUnit->GetGoal());
            if (theGoal != NULL && theGoal->GetAttackTarget() != NULL &&
                (theGoal->GetAttackTarget()->mTileNode == NULL || !theGoal->GetAttackTarget()->GetComplexUnit()->mIsDisabled ||
                 theGoal->GetAttackTarget()->GetComplexUnit()->IsProtected()))
                theResult = 1;
            else
                theResult = 0;
        }
        break;
    case kTriggerGoalIsNotConversion: {
        EEUnitGoal* theGoal = inUnit->mGoalState;
        if (theGoal == NULL || theGoal->mKind != kGoalAttackUnit ||
            (inUnit->GetUnitClass()->IsCrusader() && !static_cast<EEUGRust*>(theGoal)->IsConvertAttack()))
            theResult = 1;
        break;
    }
    case kTriggerGoalIsConversion:
        if (inUnit->mGoalState != NULL) {
            EEUGRust* theGoal = EEAttackUnitCast(inUnit->GetGoal());
            if (theGoal != NULL && (!inUnit->GetUnitClass()->IsCrusader() || theGoal->IsConvertAttack()))
                theResult = 1;
        }
        break;
    case kTriggerEnemyUnitConverted:
        if (inUnit->mGoalState != NULL) {
            EEUGRust* theGoal = EEAttackUnitCast(inUnit->GetGoal());
            if (theGoal != NULL) {
                EEPlayer* thePlayer = theGoal->GetAttackTarget()->GetPlayer();
                if (thePlayer == inUnit->GetPlayer() || inUnit->GetPlayer()->mEnemies[thePlayer->GetPlayerID()] == 0)
                    theResult = 1;
            }
        }
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x60dee4
// Unloads the conversion indicator effect and the class's media
void EEUnitClass::BasicPriestUnloadMedia(EEPlayer* inPlayer)
{
    EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator)->Unload();
    this->BaseUnloadMedia(inPlayer);
}

// FUNCTION: EXE 0x60e086
// Updates each of the unit's orbitals
void EEComplexUnit::UpdateOrbitals()
{
    unsigned long theCount = this->mNumOrbitals;
    if (theCount != 0) {
        for (unsigned long i = 0; i < theCount; i++)
            this->mOrbitals[i]->Update();
    }
}

// FUNCTION: EXE 0x60e0b2
// Counts the reload timer down by the tick, not below 0
// New (R5b.4 leaf 6): the countdown is Rust over the unit store (ee_units step.rs step_countdown); the write is the
// holder's. TEST ONLY: while the capture runs, USTM records it
void EEComplexUnit::UpdateReloadTimer()
{
    long theBefore = this->mReloadTimer;
    int64_t theTime = 0;
    bool theWritten = ee_units_step_countdown(static_cast<uint32_t>(this->mUnitID), reinterpret_cast<uintptr_t>(this),
                                              EE_UNITS_STEP_RELOAD, theBefore, gPhysics->mTickTimeMs, &theTime);
    if (theWritten)
        this->SetReloadTimer((long)theTime);
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (USTM)
        uint32_t theWords[4] = {(uint32_t)theBefore, (uint32_t)gPhysics->mTickTimeMs, theWritten ? 1u : 0u,
                                (uint32_t)this->mReloadTimer};
        EEUnitStepNote(kUnitStepReload, this->mUnitID, theWords, 4);
    }
}

// FUNCTION: EXE 0x60e0db
// Counts the target search timer down by the tick, not below 0
// New (R5b.4 leaf 6): Rust over the unit store, as UpdateReloadTimer
void EEComplexUnit::UpdateTargetSearchTimer()
{
    long theBefore = this->mTargetSearchTimer;
    int64_t theTime = 0;
    bool theWritten = ee_units_step_countdown(static_cast<uint32_t>(this->mUnitID), reinterpret_cast<uintptr_t>(this),
                                              EE_UNITS_STEP_TARGET_SEARCH, theBefore, gPhysics->mTickTimeMs, &theTime);
    if (theWritten)
        this->SetTargetSearchTimer((long)theTime);
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (USTM)
        uint32_t theWords[4] = {(uint32_t)theBefore, (uint32_t)gPhysics->mTickTimeMs, theWritten ? 1u : 0u,
                                (uint32_t)this->mTargetSearchTimer};
        EEUnitStepNote(kUnitStepTargetSearch, this->mUnitID, theWords, 4);
    }
}

// FUNCTION: EXE 0x60e104
// Counts the heal search timer (seconds) down by the tick, not below 0
static uint32_t HealBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

// New (R5b.4 leaf 6): Rust over the unit store (ee_units step.rs step_heal: the same f32 operations in order)
void EEComplexUnit::UpdateHealSearchTimer()
{
    float theBefore = this->mHealSearchTimer;
    float theTime = 0.0f;
    bool theWritten = ee_units_step_heal(static_cast<uint32_t>(this->mUnitID), reinterpret_cast<uintptr_t>(this),
                                         theBefore, gPhysics->mTickTime, &theTime);
    if (theWritten)
        this->SetHealSearchTimer(theTime);
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (USTM)
        uint32_t theWords[4] = {HealBits(theBefore), HealBits(gPhysics->mTickTime), theWritten ? 1u : 0u,
                                HealBits(this->mHealSearchTimer)};
        EEUnitStepNote(kUnitStepHealSearch, this->mUnitID, theWords, 4);
    }
}

// New (R5b.4 leaf 6): the rotting countdown of the classes' Updates (kActionUnitRotting; 19 inline copies of it were
// in the classes' Update bodies): the time left minus the tick, not below 0 (Rust over the unit store: ee_units step.rs
// step_rotting), written through the holder; returns the time (the callers that rot linked parts give them the same).
// TEST ONLY: while the capture runs, USTM records it
long EEComplexUnit::UpdateRottingAwayTime()
{
    long theBefore = this->mRottingAwayTime;
    long theTime = (long)ee_units_step_rotting(static_cast<uint32_t>(this->mUnitID), reinterpret_cast<uintptr_t>(this),
                                               theBefore, gPhysics->mTickTimeMs);
    this->SetRottingAwayTime(theTime);
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (USTM)
        uint32_t theWords[3] = {(uint32_t)theBefore, (uint32_t)gPhysics->mTickTimeMs, (uint32_t)this->mRottingAwayTime};
        EEUnitStepNote(kUnitStepRotting, this->mUnitID, theWords, 3);
    }
    return theTime;
}

// New (R5b.4 leaf 6): the resources' rotting of the classes' Updates (kActionResourcesRotting; 3 inline copies: the
// animals, the fish, the default): the time plus the tick; past 4 s the resource amount loses one per whole 4 s (not
// below 0) and the time keeps the rest (Rust over the unit store: ee_units step.rs step_resources_rotting). TEST ONLY:
// while the capture runs, USTM records it
void EEComplexUnit::UpdateResourcesRotting()
{
    long theBefore = this->mRottingAwayTime;
    long theAmountBefore = this->mResourceAmount;
    int64_t theTime = 0, theAmount = 0;
    bool theLost = ee_units_step_resources_rotting(static_cast<uint32_t>(this->mUnitID), reinterpret_cast<uintptr_t>(this),
                                                   theBefore, theAmountBefore, gPhysics->mTickTimeMs, &theTime, &theAmount);
    if (theLost)
        this->SetResourceAmount((long)theAmount);
    this->SetRottingAwayTime((long)theTime);
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (USTM)
        uint32_t theWords[5] = {(uint32_t)theBefore, (uint32_t)theAmountBefore, (uint32_t)gPhysics->mTickTimeMs,
                                (uint32_t)this->mRottingAwayTime, (uint32_t)this->mResourceAmount};
        EEUnitStepNote(kUnitStepResourcesRotting, this->mUnitID, theWords, 5);
    }
}

// FUNCTION: EXE 0x60e14c
// Turns the unit's turrets when its model has parts
void EEComplexUnit::UpdateTurrets()
{
    if (this->GetModel()->GetNumParts() != 0)
        this->mUnitClass->TurnTurrets(this);
}

// FUNCTION: EXE 0x60e16a
// For a temple not already showing them, starts the tower swirl and smoke effects and plays the tower convert sound
void EEUnitClass::BasicPriestShowConversionEffects(EEComplexUnit* inUnit)
{
    if (this->mCategory != kUnitCategoryBuildingPriest || inUnit->mCloaked != 0)
        return;
    EEEffectHandle theHandle;
    if (gWorld.GetEffectManager()->CreateUnitEffect(inUnit, EEGraphicEffect::Get(kGraphicEffectPriestTowerSwirlEffect), &theHandle, 0, 0, 0) == 0)
        inUnit->mCloaked = theHandle.mValue;
    if (gWorld.GetEffectManager()->CreateUnitEffect(inUnit, EEGraphicEffect::Get(kGraphicEffectPriestTowerSmokeGenerator), &theHandle, 0, 0, 0) == 0)
        inUnit->SetGrowTime(theHandle.mValue);
    EESoundObject* theSound = EESoundObject::Get(kSoundPriestTowerConvert);
    GE3DPoint thePosition;
    inUnit->GetPosition(thePosition);
    float theVolume;
    float thePan;
    float thePitch;
    EEComputeSoundParameters(theSound->mVolume, thePosition, theVolume, thePan, thePitch, kBasicPriestSoundFactor);
    gSoundManager->Play(theSound->mSampleID, theVolume, thePan, thePitch, false, NULL);
}

// FUNCTION: EXE 0x60e274
// For a temple, destroys the tower swirl and smoke effects
void EEUnitClass::BasicPriestHideConversionEffects(EEComplexUnit* inUnit)
{
    if (this->mCategory != kUnitCategoryBuildingPriest)
        return;
    EEEffectHandle theHandle;
    if ((long)inUnit->mCloaked > 0) {
        theHandle.mValue = inUnit->mCloaked;
        gWorld.GetEffectManager()->DestroyEffect(theHandle);
        inUnit->mCloaked = 0;
    }
    if (!(inUnit->mGrowTime > 0))
        return;
    theHandle.mValue = inUnit->mGrowTime;
    gWorld.GetEffectManager()->DestroyEffect(theHandle);
    inUnit->SetGrowTime(0);
}

// ---- EEUCLandPriest.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The land priest's unit
// class: 0x216d57..0x216de3 (EEUCLandTransport.cpp's slots follow; the unit map's 0x216d57..0x21880c block is one
// unit per class in alphabetical order: LandPriest, LandTransport, MiningBuilding, Missile, Naval, NavalBuilding,
// NavalCitizen). Unit flags: the exe's module set. Its ctor is followed by EEUnitClass's inline +0x24/+0x68/+0x6c
// (every unit class's vtable points at these copies), then its own slots. vtable 0x447060.

// TEMPLATE: EXE 0x616d77
// EEUnitClass::GetHeightAt (inline in ee/EEUnit.h)

// TEMPLATE: EXE 0x616d81
// EEUnitClass::Virtual68 (inline in ee/EEUnit.h)

// TEMPLATE: EXE 0x616d8d
// EEUCLandPriest::Virtual6C (inline in ee/EEUCLandPriest.h)

// FUNCTION: EXE 0x616d94
// Gets the sound parameters the common way; silent when the priest is not seen
void EEUnitClass::LandPriestGetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                               float* outPitch)
{
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    this->BaseGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    if (theUnit != NULL && !EEIsSeen(theUnit))
        *outVolume = 0.0f;
}

// ---- EEUCNavalPriest.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The naval priest's
// unit class: 0x21880c..0x218a45 (after EEUCNavalCitizen's slots; EEUCNavalTransport.cpp's +0x04 0x218a45 follows).
// Unit flags: the exe's module set. Its ctor is out of line (vtable 0x44724c), followed by the ??_G every priest class
// shares (EEUCBasicPriest's inline dtor), then its own slots +0x0c, +0x34, +0xa0.

// SYNTHETIC: EXE 0x61882c
// EEUCNavalPriest::`scalar deleting destructor' (EEUCBasicPriest's inline dtor; ICF: every priest class)

// FUNCTION: EXE 0x61884e
// Thins the priest ship's path to every n-th point and both ends when inSmooth is set (n: its inner collision sphere)
void EEUnitClass::NavalPriestAdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth)
{
    std::vector<GE3DPoint> thePath;
    long theCount = ioPath.size();
    long theStep = UMFloatToLong(this->mInnerCollisionSphere);
    if (inSmooth != true)
        return;
    thePath.reserve(ioPath.size());
    // a collision sphere that rounds to 0 keeps every point (it used to divide by zero); as in
    // EEUnitClass::AdjustPath
    if (theStep == 0)
        theStep = 1;
    for (long i = 0; i < theCount; i++) {
        if (i == theCount - 1 || i == 0 || i % theStep == 0)
            thePath.push_back(ioPath[i]);
    }
    ioPath = thePath;
}

// FUNCTION: EXE 0x618909
// Creates a priest ship at a placement on the water plane (z 0)
EEUnit* EEUnitClass::NavalPriestCreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                      EEUnit* inCreator)
{
    GETransformation theTransformation(inTransformation);
    theTransformation.SetX(theTransformation.GetX());
    theTransformation.SetZ(0.0f);
    return this->BaseCreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// FUNCTION: EXE 0x618942
// Answers dying done: true once the ship's transformed bounding box lies wholly below the water line
long EEUnitClass::NavalPriestEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GEBoundingBox theBox;
    GEBoundingBox theTransformedBox;
    EEComplexUnit* theUnit = inUnit;
    GEPhysicalModel& theModel = theUnit->mPhysicalModel;
    GEModel* theGEModel = theModel.GetModel();
    long theResult = 0;
    if (inEvent != kTriggerDyingAnimationFinished) {
        theResult = this->BasicPriestEvaluateCondition(theUnit, inEvent);
    } else {
        GETransformation theTransformation(false);
        theUnit->GetPosition(thePosition);
        theModel.GetLocation().Apply(theGEModel->GetInternalTransform(theModel.GetFrame()), theTransformation);
        theTransformation.Scale(theModel.GetScale());
        theTransformation.SetX(thePosition.GetX());
        theTransformation.SetY(thePosition.GetY());
        theTransformation.SetZ(thePosition.GetZ());
        theBox = theGEModel->GetBoundingBox(theUnit->mPhysicalModel.GetFrame());
        theBox.Transform(theTransformation, theTransformedBox);
        if (theTransformedBox.GetTopRightBack().GetZ() < 0.0f)
            theResult = 1;
    }
    return theResult;
}

// ---- EEUCHelicopterPriest.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The priest
// helicopter's unit class: 0x212edf..0x21339d (after EEUCHelicopter.cpp; EEUCHelicopterTransport.cpp follows). One unit
// per EEUC* class in alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's
// factory); slots +0x24, +0x2c, +0x6c (EEUCLandPriest's inline twin is ICF-folded into it), +0x74, +0x78, +0xa0 in
// source order.

// 0x23ca26 (cdecl; EEUCHelicopter.cpp declares it the same way; 0x249560 / 0x2495f5: ee/EEUnitUtilities.h)
// (provisional name)
void EEFindLandingPoint(EEComplexUnit* inUnit, GE3DPoint* ioTarget);

// FUNCTION: EXE 0x6130b3
// Gets the sound parameters with a Doppler pitch; silent while the helicopter is on the map but not seen
void EEUnitClass::HelicopterPriestGetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                                     float* outPitch)
{
    this->BaseGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    EEGetDopplerPitch(inUnit, inSound, outVolume, outPan, outPitch);
    if (inUnit->mTileNode != NULL && !EEIsSeen(inUnit->mComplexUnit))
        *outVolume = 0.0f;
}

// New (R5b.4, the fourteenth round): EEUCHelicopterPriest::ActionChanged's path to the goal when preparing to move
// (the original's statements; EEUnitUpdate.cpp's EE_UNITS_ACT_HELICOPTER_PATH calls it)
void EEHelicopterPriestPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    EEUnitClass* theClass = inClass;
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    EEUnitGoal* theGoal = inUnit->mGoalState;
    GE3DPoint theTarget;
    GE3DPoint theEnemy;
    EEUnit* theUnit;
    if (theGoal->mKind == kGoalAttackUnit) {
        EEFindDropLocation(inUnit, &theUnit, &theEnemy, NULL);
        EEGetAimPoint(inUnit, &theEnemy, &theTarget);
    } else if (theGoal->mKind == kGoalCastCalamity) {
        theGoal->GetDestination(inUnit, theTarget);
        EEGetHelicopterPriestDestination(inUnit, theTarget);
    } else if (theGoal->mKind == kGoalHealUnit)
        theGoal->GetGoalLocation(theTarget);
    else
        theGoal->GetDestination(inUnit, theTarget);
    if (theGoal->mKind == kGoalExplore)
        theTarget.SetZ(theClass->mCruiseAltitude);
    EEFindLandingPoint(inUnit, &theTarget);
    theClass->PathToward(inUnit, thePosition, theTarget, false);
}

// FUNCTION: EXE 0x6132c0
// Sets up a new priest helicopter as a priest, then lifts it to its flying height
void EEUnitClass::HelicopterPriestUnitCreated(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    this->BaseUnitCreated(inUnit);
    thePosition.SetX(theTransformation.GetX());
    thePosition.SetY(theTransformation.GetY());
    thePosition.SetZ(theTransformation.GetZ());
    float theZ = this->GetHeightAt(inUnit, thePosition);
    theTransformation.SetZ(theZ);
    inUnit->SetTransformation(theTransformation, true);
    inUnit->SetHeight(theZ);
}

// FUNCTION: EXE 0x613344
// Answers the dying-finished (below its tile's height) and move-prepared (has waypoints) triggers; others the
// priest's way
long EEUnitClass::HelicopterPriestEvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    bool theFlag = false;
    switch (inEvent) {
    case kTriggerDyingAnimationFinished:
        if (inUnit->mPhysicalModel.GetLocation().GetZ() < inUnit->mTile->GetHeight())
            theFlag = true;
        else
            theFlag = false;
        break;
    case kTriggerMovePreparationComplete:
        theFlag = (long)inUnit->mWaypoints.size() > 0;
        break;
    default:
        theResult = this->BasicPriestEvaluateCondition(inUnit, inEvent);
        break;
    }
    if (theFlag)
        theResult = 1;
    return theResult;
}

