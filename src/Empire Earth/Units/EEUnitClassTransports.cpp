// EEUnitClassTransports.cpp (new, R5b.4 round 32): the transports' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <set>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DPolygon.h"
#include "lle/U2DSparseArray.h"

#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEAirTrafficControl.h"

// ---- EEUCTransport.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The transports' base
// unit class: 0x21b27a..0x21c9e1 (after EEUCTorpedo.cpp). Unit flags: the exe's module set. Its ctor is out of line in
// EEUnitClass.cpp (0x23629e); slots +0xe0, +0xe4, +0xe8, the loading helpers (two EEComplexUnit distance inlines among
// them, first emitted here), +0xf4, +0xfc, +0xf8, +0x78, +0x100, +0x104, +0xb8 in address order. The untagged
// functions 0x21c9e1..0x21ce6c before EEUGAttackGround.cpp's key chain are not placed yet (another unit's?).

// unit-local constants 0x44736c (10000: EEGetPathLengthTo's no path) and 0x447370 (25): cls_transport.rs

// The transports' tile searches are Rust (R5b.4 round 33: ee_units cls_transport.rs; the dispatchers in
// EEUnitClassKinds.cpp call it). CanLoad (0x61b613) and CanUnloadNear (0x61b83f) stay members for the load and unload
// goals' host (EEUGRust.cpp).
bool EEUnitClass::CanLoad(EEComplexUnit* inTransport, EEComplexUnit* inUnit)
{
    EEUCNote theNote(8); theNote.C(this).U(inTransport).U(inUnit);   // TEST ONLY (UCBD)
    return theNote.R(ee_units_tr_can_load(EEObj(this), EEObj(inTransport), EEObj(inUnit)));
}

bool EEUnitClass::CanUnloadNear(EEComplexUnit* inTransport, EEComplexUnit* inCargo,
                             U2DPoint<long>* ioUnloadTile, U2DPoint<long>* ioFirstCargoTile,
                             U2DPoint<long>* inDestination)
{
    EEUCNote theNote(9); theNote.C(this).U(inTransport).U(inCargo).T(*ioUnloadTile).T(*ioFirstCargoTile).T(*inDestination);   // TEST ONLY (UCBD)
    bool theResult = theNote.R(ee_units_tr_can_unload_near(EEObj(this), EEObj(inTransport), EEObj(inCargo),
        (const long(*)[2])ioUnloadTile, (const long(*)[2])ioFirstCargoTile, (const long(*)[2])inDestination));
    theNote.S(inCargo);   // TEST ONLY (UCBD)
    return theResult;
}

// The class bodies' host (ee_units cls_host.rs; EE_UNITS_CLS_*): the continent manager's lists, the unit utilities'
// leaves that keep their records, the path lengths and a unit's reads and trial location
extern "C" int64_t ee_units_cls_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, EeUnitsClsIo* ioIo)
{
    EEComplexUnit* theUnit = (EEComplexUnit*)(uintptr_t)inA;
    std::vector<U2DPoint<long> > theTiles;
    switch (inOp) {
    case EE_UNITS_CLS_NEXT_TO_AREA:
        return gContinentManager.IsNextToArea((long)inC, (long)inD, (unsigned short)inA);
    case EE_UNITS_CLS_SHORE_IN_REGION:
        ((EEAIContinent*)(uintptr_t)inA)->GetShoreInRegion((unsigned short)inB, theTiles, (long)inC);
        break;
    case EE_UNITS_CLS_SHORE_NEXT_TO:
        ((EEAIContinent*)(uintptr_t)inA)->GetShoreNextTo((unsigned short)inB, theTiles);
        break;
    case EE_UNITS_CLS_ADJACENT_IDS: {
        std::vector<unsigned short> theIDs;
        gContinentManager.GetAdjacentIDs(theIDs, (unsigned short)inB);
        for (size_t i = 0; i < theIDs.size(); i++)
            theTiles.push_back(U2DPoint<long>(theIDs[i], 0));
        break;
    }
    case EE_UNITS_CLS_BLOCKED: {
        std::vector<EEUnit*> theUnits;
        return EEGetBlockingUnits((EEPlayer*)(uintptr_t)inA, (long)inC, (EETile*)(uintptr_t)inB, 0, theUnits);
    }
    case EE_UNITS_CLS_TILES_IN_RADIUS:
        EEGetTilesInRadius((long)inC, (long)inD, 2, true, theTiles);
        break;
    case EE_UNITS_CLS_LINE_TILES:
        U2DPoint<long>((long)inC, (long)inD).GetLineTiles(U2DPoint<long>((long)ioIo->a, (long)ioIo->b), 1.0f, theTiles);
        break;
    case EE_UNITS_CLS_PATH_LENGTH: {
        EETileCoordinate theTile((long)ioIo->a, (long)ioIo->b);
        ioIo->f = EEGetPathLengthTo(theUnit, theTile, 0x10, inB != 0);
        ioIo->a = theTile.mX;
        ioIo->b = theTile.mY;
        return 0;
    }
    case EE_UNITS_CLS_TILE_LOCATION: {
        GE3DPoint thePoint;
        ::gWorld.mMap->GetTileLocation((long)inC, (long)inD, thePoint);
        ioIo->p[0] = thePoint.mX; ioIo->p[1] = thePoint.mY; ioIo->p[2] = thePoint.mZ;
        return 0;
    }
    case EE_UNITS_CLS_SET_LOCATION:
        theUnit->SetLocation(GE3DPoint(ioIo->p[0], ioIo->p[1], ioIo->p[2]), false);
        return 0;
    case EE_UNITS_CLS_SURROUNDING:
        theUnit->mUnitClass->GetSurroundingTiles(theUnit, theTiles, 0, false);
        break;
    case EE_UNITS_CLS_LOAD_GOAL_TILE: {
        EEUGRust* theGoal = EELoadTransportGoalCast(theUnit->GetGoal());
        if (theGoal == NULL)
            return 0;
        ioIo->a = theGoal->GetLoadUnitTile().mX;
        ioIo->b = theGoal->GetLoadUnitTile().mY;
        return 1;
    }
    case EE_UNITS_CLS_CARGO:
        return EEObj(theUnit->mCargo[(size_t)inC]);
    case EE_UNITS_CLS_CONTAINER:
        return EEObj(theUnit->GetContainer());
    case EE_UNITS_CLS_WAYPOINTS:
        return (int64_t)theUnit->mWaypoints.size();
    case EE_UNITS_CLS_NEXT_WAYPOINT: {
        GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
        long theAnswer = theUnit->GetNextWaypointPoint(thePoint);
        ioIo->p[0] = thePoint.mX; ioIo->p[1] = thePoint.mY; ioIo->p[2] = thePoint.mZ;
        return theAnswer;
    }
    case EE_UNITS_CLS_COMMAND_TYPE:
        return theUnit->mCommand ? theUnit->mCommand->mType : 0;
    case EE_UNITS_CLS_GROUND_HEIGHT: {
        GE3DPoint thePoint(ioIo->p[0], ioIo->p[1], ioIo->p[2]);
        ((EEUnitClass*)(uintptr_t)inB)->GetGroundHeight(thePoint, &ioIo->f);
        return 0;
    }
    case EE_UNITS_CLS_BOMB_ON_TARGET:
        return ((EEUnitClass*)(uintptr_t)inB)->IsBombOnTarget(theUnit);
    case EE_UNITS_CLS_AWAY_FROM_GOAL:
        return ((EEUnitClass*)(uintptr_t)inB)->IsAwayFromGoal(theUnit);
    case EE_UNITS_CLS_PROJECTILE_CATEGORY: {
        EEUnitClass* theProjectile = ((EEUnitClass*)(uintptr_t)inB)->mProjectileClass;
        return theProjectile != NULL ? (int64_t)theProjectile->mCategory : -1;
    }
    case EE_UNITS_CLS_ATTACK_TARGET_ALIVE: {
        EEUGRust* theAttack = EEAttackUnitCast(theUnit->GetGoal());
        return theAttack != NULL && theAttack->GetAttackTarget() != NULL && theAttack->GetAttackTarget()->mHitPoints > 0;
    }
    case EE_UNITS_CLS_GOAL_HAS_LOCATION:
        return EEGoalHasLocation(theUnit->GetGoal());
    case EE_UNITS_CLS_REQUEST_QUEUED: {
        EEAirTrafficControl* theControl = theUnit->mPlayer->mAirTraffic[theUnit->GetUnitClass()->mAirTrafficType - 1];
        return theControl->GetQueuePosition(theUnit, 1) >= 0 || theControl->GetQueuePosition(theUnit, 0) >= 0;
    }
    case EE_UNITS_CLS_QUEUE_GOALS:
        return (int64_t)theUnit->GetQueueGoals((unsigned long)inC).size();
    case EE_UNITS_CLS_TURN_SPEEDS:
        ioIo->p[0] = theUnit->mStats->GetMaxVelocity();
        ioIo->p[1] = theUnit->mStats->GetMaxAngularVelocity();
        return 0;
    case EE_UNITS_CLS_TILES_NEAR_POINT:
        EEGetTilesNearPoint(GE3DPoint(ioIo->p[0], ioIo->p[1], ioIo->p[2]), ioIo->f, theTiles);
        break;
    case EE_UNITS_CLS_IS_SPACE_MAP:
        return ::gWorld.mMap->mTerrainMesh.GetIsSpaceMap();
    case EE_UNITS_CLS_MAP_SIZE:
        return ::gWorld.mMap->GetSize();
    case EE_UNITS_CLS_HULL_CENTER:
        ioIo->a = ((EEAIContinent*)(uintptr_t)inA)->GetHullCenter().mX;
        ioIo->b = ((EEAIContinent*)(uintptr_t)inA)->GetHullCenter().mY;
        return 0;
    case EE_UNITS_CLS_UNITS_NEAR: {
        std::vector<EEUnit*> theUnits;
        EEGetUnitsNear(GE3DPoint(ioIo->p[0], ioIo->p[1], ioIo->p[2]), ioIo->f, theUnits);
        for (size_t i = 0; i < theUnits.size(); i++)
            ee_units_cls_push((EeUnitsClsListHandle*)ioIo->list, (int32_t)EEObj(theUnits[i]), (int32_t)(EEObj(theUnits[i]) >> 32));
        return 0;
    }
    case EE_UNITS_CLS_HOME_BASE:
        return EEObj(theUnit->mHomeBase);
    case EE_UNITS_CLS_FLIGHT_TARGET: {   // GetNextFlightState's goal target (0x615ccf)
        EEUnitGoal* theGoal = theUnit->mGoalState;
        GE3DPoint theTarget;
        if (theGoal == NULL)
            return 0;
        switch (theGoal->mKind) {
        case kGoalLocation:
        case kGoalAttackGround:
            theTarget = static_cast<EEUGRust*>(theGoal)->GetGroundTarget();
            break;
        case kGoalAttackUnit: {
            EEUnit* theOther = static_cast<EEUGRust*>(theGoal)->GetAttackTarget();
            if (theOther == NULL)
                return 0;
            if (theOther->mComplexUnit != NULL) {
                const GETransformation& theLocation = theOther->mComplexUnit->mPhysicalModel.GetLocation();
                theTarget.SetXYZ(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
            } else
                theTarget.SetXYZ((float)theOther->GetTileX() + 0.5f, (float)theOther->GetTileY() + 0.5f, theOther->mTile->GetHeight());
            break;
        }
        case kGoalPatrol: {
            const GETransformation& theLocation = static_cast<EEUGRust*>(theGoal)->GetPatrolTarget()->mPhysicalModel.GetLocation();
            theTarget.SetXYZ(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
            break;
        }
        case kGoalExplore:
            static_cast<EEUGRust*>(theGoal)->GetExploreTileCenter(theTarget);
            break;
        default:
            return 0;
        }
        ioIo->p[0] = theTarget.mX; ioIo->p[1] = theTarget.mY; ioIo->p[2] = theTarget.mZ;
        return 1;
    }
    case EE_UNITS_CLS_DROP_PATH:
        theUnit->DropPath();
        return 0;
    case EE_UNITS_CLS_ADD_WAYPOINT:
        ((EEUnitClass*)(uintptr_t)inB)->AddWaypointAt(theUnit, ioIo->p[0], ioIo->p[1], ioIo->p[2], inC != 0);
        return 0;
    case EE_UNITS_CLS_PATH_TOWARD: {
        GE3DPoint theFrom(ioIo->p[0], ioIo->p[1], ioIo->p[2]);
        GE3DPoint theTo(ioIo->q[0], ioIo->q[1], ioIo->q[2]);
        ((EEUnitClass*)(uintptr_t)inB)->PathToward(theUnit, theFrom, theTo, inC != 0);
        return 0;
    }
    case EE_UNITS_CLS_FLY_TO_GOAL: {
        GE3DPoint thePoint(ioIo->p[0], ioIo->p[1], ioIo->p[2]);
        ((EEUnitClass*)(uintptr_t)inB)->FlyToGoal(thePoint, theUnit);
        return 0;
    }
    case EE_UNITS_CLS_RUNWAY_GOAL: {   // ApproachRunway's move goal (0x615f7d)
        GE3DPoint thePoint(ioIo->p[0], ioIo->p[1], ioIo->p[2]);
        EEUGRust* theMove = EEUGRust::NewLocation(&thePoint, false, true, false, 0.0f, false, false);
        bool theKeep = theUnit->mGoalState != NULL && theUnit->mGoalState->mKind == kGoalExplore;
        if (!theKeep)
            theUnit->ClearGoalQueue(0);
        theUnit->SetGoal(theMove, theKeep, 0);
        return 0;
    }
    }
    for (size_t i = 0; i < theTiles.size(); i++)
        ee_units_cls_push((EeUnitsClsListHandle*)ioIo->list, (int32_t)theTiles[i].mX, (int32_t)theTiles[i].mY);
    return 0;
}

// Returns the squared 3D distance from the unit to a point
float EEComplexUnit::DistanceSquared3D(const GE3DPoint& inPoint)
{
    float theDZ = this->mPhysicalModel.GetLocation().GetZ() - inPoint.GetZ();
    float theDY = this->mPhysicalModel.GetLocation().GetY() - inPoint.GetY();
    float theDX = this->mPhysicalModel.GetLocation().GetX() - inPoint.GetX();
    return theDX * theDX + theDY * theDY + theDZ * theDZ;
}

// Finds where cargo leaves the transport: a tile around it for a unit that cannot move, else a free placement
// near the location (water allowed for non-land units); returns whether one was found
bool EEUnitClass::TransportFindBoardingLocation(EEComplexUnit* inTransport, EEComplexUnit* inCargo,
                              GE3DPoint& inLocation, GE3DPoint& outLocation)
{
    std::vector<EETileCoordinate> theTiles;
    GETransformation theTransformation;
    GE3DPoint thePosition;
    bool theAllowWater = (inCargo->mUnitClass->mDomains & kMovementLand) == 0;
    bool theResult;
    if (inCargo->mStats->mMaxVelocity <= 0.0f) {
        EEGetTilesAroundUnit(inTransport, inCargo, 1, 0, 0, theTiles);
        theResult = EEPickTileNear(theTiles, inTransport, inCargo, 0, outLocation);
    } else {
        inTransport->GetPosition(thePosition);
        long theSize = inCargo->mUnitClass->mTileSize;
        unsigned long theY = UMFloorUnsigned(inLocation.GetY());
        unsigned long theX = UMFloorUnsigned(inLocation.GetX());
        if (EEFindPlacement(&thePosition, inTransport, NULL, theX, theY, theSize, 1, false,
                            theAllowWater, false, true, &theTransformation, NULL)) {
            theResult = true;
            outLocation = theTransformation.GetPosition();
        } else {
            theResult = false;
            outLocation = inLocation;
        }
    }
    return theResult;
}

// Sets up a new transport as any unit, then gives it its class's resource type and amount
void EEUnitClass::TransportUnitCreated(EEComplexUnit* inUnit)
{
    this->BaseUnitCreated(inUnit);
    inUnit->SetResourceType(this->mResourceType);
    if (inUnit->mResourceType != kResourceNone)
        inUnit->SetResourceAmount(this->mResourceAmount);
}

// Unloads the transport's cargo at a tile (siege towers with the tower flag set)
void EEUnitClass::TransportUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    bool theFlag;
    if (this->GetUnitType() == kUnitTypeSiege04Tower || this->GetUnitType() == kUnitTypeSiege06HeavyTower)
        theFlag = true;
    else
        theFlag = false;
    inTransport->UnloadCargoAtTile(inX, inY, theFlag);
}

// ---- EEUCLandTransport.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The land
// transport's unit class: 0x216de3..0x217085 (after EEUCLandPriest.cpp; EEUCMiningBuilding.cpp follows). Unit flags:
// the exe's module set. Its ctor is inline (vtable 0x448930); the slots +0xe4, +0xf4, +0xf8, +0x8c, +0xb8 in address
// order. +0x8c and +0xf4 are also EEUCFortress's (ICF).

// Unloads all cargo at once when the transport is badly hurt
void EEUnitClass::LandTransportBadlyHurt(EEUnit* inUnit)
{
    std::vector<EEComplexUnit*> theCargo;
    EEComplexUnit* theTransport = inUnit->mComplexUnit;
    EEComplexUnit* theUnit;
    unsigned long theCount = theTransport->GetNumCargoUnits();
    for (unsigned long i = 0; i < theCount; i++) {
        theUnit = theTransport->GetCargo(i);
        if (theUnit != NULL)
            theCargo.push_back(theUnit);
    }
    unsigned long theSize = theCargo.size();
    for (unsigned long j = 0; j < theSize; j++)
        theTransport->UnloadCargoNow(theCargo[j]);
}

// ---- EEUCHelicopterTransport.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The transport
// helicopter's unit class: 0x21339d..0x213866 (after EEUCHelicopterPriest.cpp; EEUCLand.cpp follows). One unit per
// EEUC* class in alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's
// factory); slots +0xe4, +0x24, +0xf0, +0xf8, +0x74, +0x78, +0xa0, +0xb8 in address order.

// 0x23ca26 (cdecl; EEUCHelicopter.cpp declares it the same way) (provisional name)
void EEFindLandingPoint(EEComplexUnit* inUnit, GE3DPoint* ioTarget);

// New (R5b.4, the fourteenth round): EEUCHelicopterTransport::ActionChanged's path to the goal when preparing to move
// (the original's statements; EEUnitUpdate.cpp's EE_UNITS_ACT_HELICOPTER_PATH calls it)
void EEHelicopterTransportPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    EEUnitClass* theClass = inClass;
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    EEUnitGoal* theGoal = inUnit->mGoalState;
    GE3DPoint theTarget;
    theGoal->GetDestination(inUnit, theTarget);
    if (theGoal->mKind == kGoalExplore)
        theTarget.SetZ(theClass->mCruiseAltitude);
    EEFindLandingPoint(inUnit, &theTarget);
    theClass->PathToward(inUnit, thePosition, theTarget, false);
}

// Sets up a new transport helicopter as a transport, then lifts it to its flying height
void EEUnitClass::HelicopterTransportUnitCreated(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    this->TransportUnitCreated(inUnit);
    thePosition.SetX(theTransformation.GetX());
    thePosition.SetY(theTransformation.GetY());
    thePosition.SetZ(theTransformation.GetZ());
    float theZ = this->GetHeightAt(inUnit, thePosition);
    theTransformation.SetZ(theZ);
    inUnit->SetTransformation(theTransformation, true);
    inUnit->SetHeight(theZ);
}

// ---- EEUCNavalTransport.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The naval
// transport's unit class: 0x218a45..0x218e34 (after EEUCNavalPriest.cpp; EEUCPlanetSpaceship.cpp's +0x74 0x218e34
// follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory; vtable 0x448718); its
// slots +0x04, +0xe8, +0xf8, +0xfc, +0x88, +0x104, +0xb8 in source order (Virtual08/0C/30/34/A0/E4/F4 are other
// units' copies by ICF).


// ---- EEUCSpaceTransport.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The space
// transport's unit class: 0x21a6e0..0x21ab66 (after EEUCSpaceship.cpp; EEUCSubmergedNaval.cpp's +0x04 0x21ab66
// follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory). Slots +0xe4, +0x24,
// +0x2c, +0xf4, +0xf8, EEUCTransport::ContinentsConnect, +0x78, +0x84, +0x98, +0xa0 in address order; +0x24/+0x2c/+0x78/+0xa0
// call the spaceship's bodies (helpers ICF-folded into EEUCSpaceship.cpp's slots).

// Sets up a new space transport at its flying height, with the class's resource type and amount
void EEUnitClass::SpaceTransportUnitCreated(EEComplexUnit* inUnit)
{
    this->SpaceshipVirtual78(inUnit);
    inUnit->SetResourceType(this->mResourceType);
    if (inUnit->mResourceType != kResourceNone)
        inUnit->SetResourceAmount(this->mResourceAmount);
}

// Adds the transport to its own tile only
void EEUnitClass::SpaceTransportAddToTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(theUnit, theTile->mUnits);
}

// Removes the transport from its own tile only
void EEUnitClass::SpaceTransportRemoveFromTile(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = theMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->RemoveUnit(theUnit);
}

void EEUnitClass::SpaceshipVirtual2C(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                              float* outPitch)
{
    this->BaseGetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    EEGetDopplerPitch(inUnit, inSound, outVolume, outPan, outPitch);
}

void EEUnitClass::SpaceshipVirtual78(EEComplexUnit* inUnit)
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

// ---- EEUCFortress.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The fortress (a land
// transport that garrisons; unit type 0x313 is the teleporter): 0x21013b..0x21048f (after
// EEUCBuildingPriest.cpp; a globals chain opens the next unit at 0x21048f). Unit flags: the exe's module set. Its ctor
// is inline (EEUnitClass.cpp's factory).

// 0x4c8fa4 (.data, a word): the faith a teleporter spends per full unload (provisional name)
unsigned short gFortressUnloadCost = 100;

// Returns whether the player's population has room for the unit's cost
bool EEUnitClass::FortressHasRoom(EEComplexUnit* inUnit, EEComplexUnit* inCargo)
{
    EEPlayer* thePlayer = inUnit->mPlayer;
    EEUnitClass* theClass = inCargo->mUnitClass;
    bool theResult = thePlayer->GetTotalPopulation(false, false) + theClass->GetPopulationCost() <= thePlayer->GetPopulationLimit(0);
    return theResult;
}

// Sets up a new fortress as built, with a 1 s regeneration timer
void EEUnitClass::FortressUnitCreated(EEComplexUnit* inUnit)
{
    inUnit->SetBuilt(0, 1, 1);
    this->BaseUnitCreated(inUnit);
    inUnit->SetRegenerationTimer(1000);
}

// Unloads at a tile; a teleporter needs 100 faith and spends it in proportion to the cargo unloaded, else warns
// its owner (event 0x274e)
void EEUnitClass::FortressUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    if (inTransport->mUnitClass->mUnitType == kUnitTypeTeleporter) {
        if (inTransport->GetPower() >= gFortressUnloadCost) {
            unsigned long theBefore = inTransport->mUnitClass->mTransportCapacity - inTransport->mCargoRoom;
            this->TransportUnloadAt(inTransport, inX, inY);
            long theCapacity = inTransport->mUnitClass->mTransportCapacity;
            unsigned long theAfter = theCapacity - inTransport->mCargoRoom;
            if (theBefore != theAfter)
            {
                float theFraction = (float)(theBefore - theAfter) / theCapacity;
                inTransport->SetPower(inTransport->GetPower() - UMFloatToLong(theFraction * gFortressUnloadCost));
            }
        } else {
            U2DPoint<long> theTile(inX, inY);
            inTransport->mPlayer->mEvents.AddEvent(theTile, 2, inTransport->GetPlayer(), 0x274e, false);
        }
    } else
        this->TransportUnloadAt(inTransport, inX, inY);
}

