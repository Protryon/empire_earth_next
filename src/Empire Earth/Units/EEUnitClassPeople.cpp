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
#include "ee/EEAIFOUnitScreeners.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEE2DBattlefieldAmbient.h"
#include "ee/EESoundObject.h"
#include "ee/EEEffectManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitClassKinds.h"
#include "rust/sim/combat.h"
#include "rust/sim/units.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEUnitAction.h"
#include "ee/EESimCapture.h"
#include "rust/sim/combat.h"
#include "rust/sim/units.h"
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

// Returns the nearest gatherable resource of a kind within inRange rings of a centre unit, not in ioUnits (tiles
// shuffled per ring; another path area costs 400; a dead animal wins at once; boats look for naval resources)
// (ee_units citizen.rs)
EEUnit* EEUnitClass::FindResourceOfKind(EEComplexUnit* inUnit, EEUnit* inCenter, EEUnit* inResource, long inRange,
                                   unsigned long inKind, std::vector<EEUnit*>* ioUnits, bool inFarms)
{
    EEUCNote theNote(41); theNote.C(this).U(inUnit).U(inCenter).U(inResource).I(inRange).I((long)inKind).Us(*ioUnits).I(inFarms);   // TEST ONLY (UCBD)
    EEUnit* theResult = (EEUnit*)(uintptr_t)ee_units_cz_find_resource_of_kind((uint32_t)this->mCategory, EEObj(inUnit),
        EEObj(inCenter), EEObj(inResource), (int32_t)inRange, (uint32_t)inKind,
        ioUnits->empty() ? NULL : reinterpret_cast<const uint64_t*>(ioUnits->data()), ioUnits->size(), inFarms);
    theNote.U(theResult);   // TEST ONLY (UCBD)
    return theResult;
}

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

// Returns where the citizen drops its load: its goal's storage, else a farm's nearest drop site within 20, else the
// owner's nearest built drop site that takes the load (docks and navy yards for boats) (ee_units citizen.rs)
EEUnit* EEUnitClass::FindStorage(EEComplexUnit* inUnit, bool inAnyStorage)
{
    EEUCNote theNote(43); theNote.C(this).U(inUnit).I(inAnyStorage).I(inUnit->mResourceAmount).I((long)inUnit->mResourceType);   // TEST ONLY (UCBD)
    EEUnit* theResult = (EEUnit*)(uintptr_t)ee_units_cz_find_storage(EEObj(inUnit), inAnyStorage);
    theNote.U(theResult).I(inUnit->mResourceAmount).I((long)inUnit->mResourceType);   // TEST ONLY (UCBD)
    return theResult;
}

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

// Returns whether the citizen has a gather point beside a resource (not just the resource's own corner tile)
// (ee_units citizen.rs)
bool EEUnitClass::HasGatherPoint(EEComplexUnit* inUnit, EEUnit* inResource)
{
    EEUCNote theNote(45); theNote.C(this).U(inUnit).U(inResource);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_cz_has_gather_point(EEObj(inUnit), EEObj(inResource)) != 0);
}

// The citizens' searches' host (ee_units citizen.rs, citizen_host.rs; EE_UNITS_CZ_*): the units' reads, the noted
// unit utilities, the AI's shuffle and the world tests, each as the original called it
static void EECzTilesIn(const EeUnitsCzIo* inIo, std::vector<U2DPoint<long> >& outTiles)
{
    for (size_t i = 0; i < inIo->tiles_len; i++)
        outTiles.push_back(U2DPoint<long>(inIo->tiles[i][0], inIo->tiles[i][1]));
}

static void EECzTilesOut(EeUnitsCzIo* ioIo, const std::vector<U2DPoint<long> >& inTiles)
{
    for (size_t i = 0; i < inTiles.size(); i++)
        ee_units_cz_push_tile((EeUnitsCzList*)ioIo->list, (int32_t)inTiles[i].mX, (int32_t)inTiles[i].mY);
}

extern "C" int64_t ee_units_cz_host(int32_t inOp, uint64_t inA, uint64_t inB, EeUnitsCzIo* io)
{
    EEUnit* theUnit = (EEUnit*)(uintptr_t)inA;
    EEMapTile* theTile = (EEMapTile*)(uintptr_t)inA;
    EEUGRust* theGoal = (EEUGRust*)(uintptr_t)inA;
    const int64_t* v = io->v;
    std::vector<U2DPoint<long> > theTiles;
    switch (inOp) {
    case EE_UNITS_CZ_UNIT: {
        EeUnitsCzUnit& o = io->unit;
        EEUnitClass* c = theUnit->mUnitClass;
        EEComplexUnit* x = theUnit->mComplexUnit;
        o.obj = inA;
        o.class_obj = EEObj(c);
        o.complex = EEObj(x);
        o.player = EEObj(theUnit->mPlayer);
        o.goal_state = x != NULL ? EEObj(x->mGoalState) : 0;
        o.goal_kind = x != NULL && x->mGoalState != NULL ? (int32_t)x->mGoalState->mKind : -1;
        o.tile_x = (int32_t)theUnit->mTileX;
        o.tile_y = (int32_t)theUnit->mTileY;
        o.hit_points = (int32_t)theUnit->GetHitPoints();
        o.resource_type = (uint32_t)theUnit->mResourceType;
        o.on_map = theUnit->mTileNode != NULL;
        o.disabled = x != NULL && x->mIsDisabled;
        o.built = x != NULL && x->mIsBuilt;
        o.foundation = x != NULL && x->IsFoundation();
        if (c != NULL) {
            o.category = (uint32_t)c->mCategory;
            o.unit_type = (uint32_t)c->mUnitType;
            o.family = (uint32_t)c->mFamily;
            o.tile_size = (int32_t)c->GetTileSize();
            o.drop_site = c->IsDropSite();
        }
        return 0;
    }
    case EE_UNITS_CZ_CLASS_TILE_SIZE:
        return gUnitClassTable[(size_t)v[0]]->GetTileSize();
    case EE_UNITS_CZ_MAP_SIZE:
        io->v[0] = gMapWidth;
        io->v[1] = gMapHeight;
        return 0;
    case EE_UNITS_CZ_SQUARE_RING_TILES: {
        std::vector<EETileCoordinate> theRing;
        EEGetSquareRingTiles((long)v[0], (long)v[1], (long)v[2], theRing);
        EECzTilesOut(io, theRing);
        return 0;
    }
    case EE_UNITS_CZ_SIDE_TILES:
        EECzTilesIn(io, theTiles);
        EEGetSideTiles((long)v[0], (long)v[1], (long)v[2], theTiles);
        EECzTilesOut(io, theTiles);
        return 0;
    case EE_UNITS_CZ_RING_CORNERS:
        EECzTilesIn(io, theTiles);
        EEGetRingCorners((long)v[0], (long)v[1], (long)v[2], (long)v[3], (long)v[4], theTiles);
        EECzTilesOut(io, theTiles);
        return 0;
    case EE_UNITS_CZ_SQUARE_RING:
        EECzTilesIn(io, theTiles);
        EEGetSquareRing((long)v[0], (long)v[1], (long)v[2], (long)v[3], theTiles);
        EECzTilesOut(io, theTiles);
        return 0;
    case EE_UNITS_CZ_SHUFFLE:
        EECzTilesIn(io, theTiles);
        EEAIRandomShuffle(theTiles.begin(), theTiles.end());
        EECzTilesOut(io, theTiles);
        return 0;
    case EE_UNITS_CZ_TILE_CONTINENT:
        return gContinentManager.GetContinent(EETileCoordinate((long)v[0], (long)v[1]), kAIPathLandMass);
    case EE_UNITS_CZ_UNIT_CONTINENT:
        return gContinentManager.GetContinent(theUnit, kAIPathLandMass);
    case EE_UNITS_CZ_BLOCK_TAKES:
        return EEBlockTakes(theUnit, (long)v[0], (long)v[1], (long)v[2], (long)v[3], (long)v[3], 0);
    case EE_UNITS_CZ_TILE:
        return EEObj(gWorld.mMap->GetTile((long)v[0], (long)v[1]));
    case EE_UNITS_CZ_TILE_UNITS:
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext)
            ee_units_cz_push_unit((EeUnitsCzList*)io->list, EEObj(theNode->mUnit));
        return 0;
    case EE_UNITS_CZ_TERRAIN:
        return theTile->mTerrainType;
    case EE_UNITS_CZ_HEIGHT:
        io->f[0] = theTile->GetHeight();
        return 0;
    case EE_UNITS_CZ_CAN_GATHER:
        return ((EEComplexUnit*)theUnit)->CanGather((EEUnit*)(uintptr_t)inB, (long)v[0], (long)v[1], false, false);
    case EE_UNITS_CZ_CAN_GET_TO_UNIT:
        return gContinentManager.CanGetToUnit(theUnit, (EEUnit*)(uintptr_t)inB);
    case EE_UNITS_CZ_DISTANCE_2D:
        io->f[0] = theUnit->mComplexUnit->DistanceSquared2D(GE2DPoint(io->f[0], io->f[1]));
        return 0;
    case EE_UNITS_CZ_TILE_DISTANCE:
        io->f[0] = theUnit->GetTileDistanceSquared((EEUnit*)(uintptr_t)inB);
        return 0;
    case EE_UNITS_CZ_PATH_AREA:
        return gContinentManager.GetPathAreaIDAt(EETileLocation((long)v[0], (long)v[1]));
    case EE_UNITS_CZ_GATHER_STORAGE:
        return EEObj(theGoal->GetGatherStorage());
    case EE_UNITS_CZ_GATHER_RESOURCE:
        return EEObj(theGoal->GetGatherResource());
    case EE_UNITS_CZ_IS_GATHERING:
        return theGoal->IsGathering();
    case EE_UNITS_CZ_BUILD_IS_WALL:
        return theGoal->GetBuildTargetClass()->mIsWall;
    case EE_UNITS_CZ_BUILD_TYPE:
        return (int64_t)theGoal->GetBuildTargetType();
    case EE_UNITS_CZ_FARM_DROP_SITES: {
        std::vector<EEUnit*> theSites;
        EEGetFarmDropSites((EEComplexUnit*)theUnit, theSites);
        for (size_t i = 0; i < theSites.size(); i++)
            ee_units_cz_push_unit((EeUnitsCzList*)io->list, EEObj(theSites[i]));
        return 0;
    }
    case EE_UNITS_CZ_CAN_TAKE:
        return ((EEComplexUnit*)theUnit)->CanTake((EEComplexUnit*)(uintptr_t)inB, (unsigned long)v[0]);
    case EE_UNITS_CZ_EMPTY_LOAD:
        theUnit->SetResourceAmount(0);
        theUnit->SetResourceType((unsigned long)v[0]);
        return 0;
    case EE_UNITS_CZ_DISTANCE_TO_UNIT:
        io->f[0] = ((EEComplexUnit*)theUnit)->DistanceSquaredToUnit((EEUnit*)(uintptr_t)inB);
        return 0;
    case EE_UNITS_CZ_DISTANCE_TO:
        io->f[0] = ((EEComplexUnit*)theUnit)->DistanceSquaredTo((EEComplexUnit*)(uintptr_t)inB);
        return 0;
    case EE_UNITS_CZ_PLAYER_UNITS:
        for (EEUnit* u = ((EEPlayer*)(uintptr_t)inA)->mFirstUnit; u != NULL; u = static_cast<EEUnit*>(u->mNext))
            ee_units_cz_push_unit((EeUnitsCzList*)io->list, EEObj(u));
        return 0;
    }
    return 0;
}

// ---- EEUCLandCitizen.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The land citizen's unit
// class: 0x216c48..0x216d4b (after EEUCLandable.cpp; EEUCLandMilitaryHero.cpp follows). One unit per EEUC* class in
// alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); VirtualB8,
// then EEComplexUnit::ContinueProduction after its first user.

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

// Creates a fishing boat on a tile at sea level
EEUnit* EEUnitClass::NavalCitizenCreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    GETransformation theTransformation;
    theTransformation.SetX((float)inX);
    theTransformation.SetY((float)inY);
    theTransformation.SetZ(0.0f);
    return this->BaseCreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// ---- EEUCBasicPriest.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The priests' common
// unit class (conversion): 0x20d571..0x20e2cb (after EEUCBasicCitizen.cpp; EEUCBomb.cpp follows). Unit flags: the
// exe's module set. Order: the conversion cost (static), UpdateConversion (command 0x7d4), ConvertTarget (the convert), Virtual74,
// Virtual78, the can-convert test (static), VirtualA8, VirtualA0, VirtualB4, VirtualB8, then EEComplexUnit's
// helpers that VirtualB8 calls (COMDATs after their first user: inline in the real header? written out of line
// here), ShowConversionEffects, HideConversionEffects.

void EEComputeSoundParameters(float inVolume, const GE3DPoint& inLocation, float& outVolume, float& outPan,
                              float& outPitch, float inFactor);

static const float kBasicPriestNoHeight = -10000.0f;

static const float kBasicPriestSoundFactor = 4.0f;     // 0x446fe4 (the conversion sound's distance factor)

// Returns the faith a conversion by a unit costs: the world table's entry 46, at most the unit's own power (0 if none)
long EEGetConversionCost(EEComplexUnit* inUnit)
{
    EeCombatConversionCostIn theIn = {(int32_t)gWorldTable.GetLongValue(kWorldConversionFaithCost),
                                      (int32_t)inUnit->mStats->mPower};
    return EECombatConversionCost(theIn);
}

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

// Loads the conversion indicator effect and the class's media
void EEUnitClass::BasicPriestLoadMedia(EEPlayer* inPlayer)
{
    EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator)->Load(false);
    this->BaseLoadMedia(inPlayer);
}

// Unloads the conversion indicator effect and the class's media
void EEUnitClass::BasicPriestUnloadMedia(EEPlayer* inPlayer)
{
    EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator)->Unload();
    this->BaseUnloadMedia(inPlayer);
}

// Updates each of the unit's orbitals
void EEComplexUnit::UpdateOrbitals()
{
    unsigned long theCount = this->mNumOrbitals;
    if (theCount != 0) {
        for (unsigned long i = 0; i < theCount; i++)
            this->mOrbitals[i]->Update();
    }
}

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

// Turns the unit's turrets when its model has parts
void EEComplexUnit::UpdateTurrets()
{
    if (this->GetModel()->GetNumParts() != 0)
        this->mUnitClass->TurnTurrets(this);
}

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

// Creates a priest ship at a placement on the water plane (z 0)
EEUnit* EEUnitClass::NavalPriestCreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                      EEUnit* inCreator)
{
    GETransformation theTransformation(inTransformation);
    theTransformation.SetX(theTransformation.GetX());
    theTransformation.SetZ(0.0f);
    return this->BaseCreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
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

