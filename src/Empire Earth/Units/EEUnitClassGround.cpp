// EEUnitClassGround.cpp (new, R5b.4 round 32): the ambient, resource, land and naval kinds' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>
#include <stdio.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEWheel.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitClassKinds.h"   // EEObj (the host)
#include "rust/sim/units.h"

// ---- EEUCAmbient.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The ambient object's
// unit class: 0x20b9fc..0x20bcf7 (after EEUCAirplane.cpp; EEUCBasicCitizen.cpp follows). Unit flags: the exe's module
// set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x04, +0x08, +0x64, +0x7c, +0x84, +0x98, +0xb8 in
// source order (unit type 0x234 sits on the water plane).

// For cattails, returns the tile where the view line through a point meets the water plane; otherwise sets nothing
void EEUnitClass::AmbientLocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
{
    GE3DPoint thePoint;
    GE3DLine theLine(::gWorld.mLine);
    if (this->mUnitType != kUnitTypeAmbCattails1)
        return;
    theLine.SetStart(inLocation);
    float theT;
    theLine.SolveForZ(0.0f, theT, thePoint);
    outX = UMFloor(thePoint.GetX());
    outY = UMFloor(thePoint.GetY());
}

// Adds the unit to every map tile of its footprint, its own tile first
void EEUnitClass::AmbientAddToTile(EEUnit* inUnit)
{
    ee_units_ucg_call(EE_UNITS_UCG_DO_AMBIENT_ADD, this, NULL, EEObj(inUnit), 0.0f);   // ucg_ground.rs
}

// Removes the unit from every map tile of its footprint, its own tile first
void EEUnitClass::AmbientRemoveFromTile(EEUnit* inUnit)
{
    ee_units_ucg_call(EE_UNITS_UCG_DO_AMBIENT_REMOVE, this, NULL, EEObj(inUnit), 0.0f);   // ucg_ground.rs
}

// ---- EEUCResource.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The resource's unit
// class: 0x219c49..0x219d67 (after EEUCProjectile.cpp; EEUCSeaAnimal.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline (no new site in the range); +0x70/+0x98 are ICF-folded elsewhere.

// Sets up a new resource unit with the class's resource type and amount
void EEUnitClass::ResourceSimpleUnitCreated(EEUnit* inUnit)
{
    ee_units_ucg_call(EE_UNITS_UCG_DO_RESOURCE_CREATED, this, NULL, EEObj(inUnit), 0.0f);   // ucg_ground.rs
}

// Adds the resource to every tile of its footprint (its own tile first), unless it is queued for deletion
void EEUnitClass::ResourceAddToTile(EEUnit* inUnit)
{
    ee_units_ucg_call(EE_UNITS_UCG_DO_RESOURCE_ADD, this, NULL, EEObj(inUnit), 0.0f);   // ucg_ground.rs
}

// ---- EEUCLand.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The land unit class:
// 0x213866..0x213c60 (after EEUCHelicopterTransport.cpp; EEUCLandable.cpp follows). One unit per EEUC* class in
// alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x78,
// +0x74, +0xe0 (the paratroopers), +0xbc, +0xb8 in address order, then EEComplexUnit::UpdateWheels's COMDAT after its
// first user VirtualB8.

// 0x20b6ff (EEUCAirplane.cpp): the plane's flight for inPlayer (provisional name)
void EECreateCarrierAircraft(EEComplexUnit* inUnit, long inPlayer);

// 0xde4a9 (EEE2DBattlefieldAmbient.cpp's range): the unit's battlefield ambience (provisional name)

// Sets up a new land unit: its class's resource (and amount when it has one) and, for the radio man, its power
void EEUnitClass::LandUnitCreated(EEComplexUnit* inUnit)
{
    ee_units_ucg_call(EE_UNITS_UCG_DO_LAND_CREATED, this, inUnit, 0, 0.0f);   // ucg_ground.rs
}

// Calls a paratroop plane (class 0x2e1): it appears over the last map tile beyond the unit in +y and flies to the
// unit's goal location to unload its cargo
void EEUnitClass::LandCallParatroopPlane(EEComplexUnit* inUnit)
{
    ee_units_ucg_call(EE_UNITS_UCG_DO_CALL_PARATROOP_PLANE, this, inUnit, 0, 0.0f);   // ucg_ground.rs
}

// Updates each of the unit's wheels
void EEComplexUnit::UpdateWheels()
{
    unsigned long theCount = this->mNumWheels;
    for (unsigned long i = 0; i < theCount; i++)
        this->mWheels[i]->Update(this);
}

// ---- EEUCLandMilitaryHero.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The military
// hero's unit class: 0x216d4b..0x216d57 (after EEUCLandCitizen.cpp; EEUCLandPriest.cpp follows). One unit per EEUC*
// class in alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); its
// only out-of-line slot is VirtualB8 (the others are EEUCLand's by ICF).

// ---- EEUCNaval.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The warships' unit class:
// 0x217c6d..0x218158 (after EEUCMissile.cpp; EEUCNavalBuilding.cpp follows), slots +0x0c, +0x74, +0x78, +0x88, +0xa0
// and +0xb8 of ee/EEUCNaval.h plus the helper UpdateResourceUnit. The ctor is inline (EEUnitClass.cpp's factory). Unit flags:
// the exe's module set.
// Virtual0C 0x217c6d forwards (inUnit, ioPath, inArg) to the cdecl 0x23df77 (declared here).


// Sets up a new ship at sea level (z 0), with the class's resource type and amount
void EEUnitClass::NavalUnitCreated(EEComplexUnit* inUnit)
{
    ee_units_ucg_call(EE_UNITS_UCG_DO_NAVAL_CREATED, this, inUnit, 0, 0.0f);   // ucg_ground.rs
}

// Returns whether the ship may stop: already sinking, a slow request, or at the path's end tile with its speed reached
bool EEUnitClass::NavalHasArrived(EEComplexUnit* inUnit, float inValue)
{
    return ee_units_ucg_call(EE_UNITS_UCG_DO_NAVAL_ARRIVED, this, inUnit, 0, inValue) != 0;   // ucg_ground.rs
}

// Updates a fish resource: while its resources rot it loses one every 4 s; a rotting unit counts its rotting time down
void EEUnitClass::UpdateResourceUnit(EEComplexUnit* inUnit)
{
    ee_units_ucg_call(EE_UNITS_UCG_DO_UPDATE_RESOURCE_UNIT, this, inUnit, 0, 0.0f);   // ucg_ground.rs
}

// ---- EEUCSubmergedNaval.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The submarine's
// unit class (EEUCSeaAnimal shares most slots by ICF): 0x21ab66..0x21b163 (after EEUCSpaceTransport.cpp;
// EEUCTorpedo.cpp follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots
// +0x04, +0x08, +0x34, +0x30, +0x74, +0x84, +0x88, +0xa0, +0xb8 in address order.

// 0x23f27e (cdecl): moves a location to the depth a submerged unit may take at its tile (provisional name)

// Lifts a location that lies below the terrain mesh (the sea floor) up onto it
void EEUnitClass::SubmergedNavalGroundLocation(GE3DPoint& ioLocation)
{
    GETerrainMesh& theMesh = ::gWorld.GetMap()->mTerrainMesh;
    float theGround = theMesh.GetMeshZ(ioLocation.GetX(), ioLocation.GetY());
    if (theGround >= ioLocation.GetZ())
        ioLocation.SetZ(theGround);
}

// Returns the tile where the view line through a point meets the submarine's cruise depth (or the sea
// floor where that is higher)
void EEUnitClass::SubmergedNavalLocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
{
    GE3DPoint thePoint;
    GE3DPoint theGround;
    GE3DLine theLine(::gWorld.mLine);
    float theT;
    float theZ = -this->GetCruiseAltitude();
    theLine.SetStart(inLocation);
    EEMap* theMap = ::gWorld.mMap;
    theLine.SolveForZ(theZ, theT, thePoint);
    theMap->GetGroundPoint(thePoint, theGround);
    if (theGround.GetZ() > theZ)
        theLine.SolveForZ(theGround.GetZ(), theT, thePoint);
    outX = UMFloor(thePoint.GetX());
    outY = UMFloor(thePoint.GetY());
}

// Creates a submarine at a placement kept above the sea floor, then moves it to the depth it may take there
EEUnit* EEUnitClass::SubmergedNavalCreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                         EEUnit* inCreator)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GETransformation theTransformation(inTransformation);
    thePosition.SetX(theTransformation.GetX());
    thePosition.SetY(theTransformation.GetY());
    thePosition.SetZ(theTransformation.GetZ());
    this->GroundLocation(thePosition);
    theTransformation.SetX(thePosition.GetX());
    theTransformation.SetY(thePosition.GetY());
    theTransformation.SetZ(thePosition.GetZ());
    EEUnit* theUnit = this->BaseCreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
    thePosition.SetZ(ee_units_uu_seabed(0, (uint64_t)(uintptr_t)theUnit->mComplexUnit, (const float(*)[3])&thePosition.mX));
    theUnit->mComplexUnit->SetLocation(thePosition, true);
    return theUnit;
}

// Creates a submarine on a tile (yaw pi) kept above the sea floor, then moves it to the depth it may take there
EEUnit* EEUnitClass::SubmergedNavalCreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GETransformation theTransformation(false);
    float theZ = ::gWorld.mMap->mTerrainMesh.GetZ(inX, inY);
    thePosition.SetX((float)inX);
    thePosition.SetY((float)inY);
    thePosition.SetZ(theZ);
    this->GroundLocation(thePosition);
    theTransformation.SetX(thePosition.GetX());
    theTransformation.SetY(thePosition.GetY());
    theTransformation.SetZ(thePosition.GetZ());
    theTransformation.SetOrientationYPR(2.0f * 1.5707964f, 0.0f, 0.0f);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    EEUnit* theUnit = this->BaseCreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
    thePosition.SetZ(ee_units_uu_seabed(0, (uint64_t)(uintptr_t)theUnit->mComplexUnit, (const float(*)[3])&thePosition.mX));
    theUnit->mComplexUnit->SetLocation(thePosition, true);
    return theUnit;
}

// Adds the submarine to its tile's unit list
void EEUnitClass::SubmergedNavalAddToTile(EEUnit* inUnit)
{
    EEUnit* theUnit = inUnit->mComplexUnit;
    EEMapTile* theTile = ::gWorld.mMap->GetTile(inUnit->mTileX, inUnit->mTileY);
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(theUnit, theTile->mUnits);
}

// Returns whether the submarine reached its waypoint: on the waypoint's tile with more to go, when its collision
// radius covers the distance left; otherwise when that distance is at most 0.05 (always in collision state 8)
bool EEUnitClass::SubmergedNavalHasArrived(EEComplexUnit* inUnit, float inValue)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    if (inUnit->mCollisionState == 8)
        return true;
    unsigned long theCount = inUnit->mWaypoints.size();
    inUnit->GetNextWaypointPoint(thePosition);
    if (inUnit->GetTileX() == UMFloor(thePosition.GetX()) && inUnit->GetTileY() == UMFloor(thePosition.GetY())) {
        if (theCount > 1
            || (inUnit->GetQueueGoals(0).size() > 1 && inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalLocation)) {
            return !(inUnit->mCollisionRadius < inValue);
        }
    }
    return inValue <= 0.05f;
}

// ---- EEUCSeaAnimal.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The sea animal's unit
// class: 0x219d67..0x219fe3 (after EEUCResource.cpp; EEUCSpaceBuilding.cpp follows). Unit flags: the exe's module
// set. Its ctor is inline. Data (.rdata, in order): the four named depth constants 0x447340..0x44734c.

// the depths a sea animal swims at (provisional names)
const float kSeaAnimalBottomClearance = 0.09f;

const float kSeaAnimalMaximumStep = 0.48f;

const float kSeaAnimalMinimumDepth = -0.6f;

const float kSeaAnimalMaximumDepthFactor = 0.81f;

// Gives each path point (from the end back) a depth one random step toward a random depth: dolphins step 0.2..1.2
// and may dive deeper, others step up to 0.48; kept under -0.1 (dolphins) or -0.6 and 0.09 above the sea floor
void EEUnitClass::SeaAnimalAdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth)
{
    const GETerrainMesh& theMesh = ::gWorld.mMap->mTerrainMesh;
    float theDepth = inUnit->mPhysicalModel.GetLocation().GetZ();
    bool theSurfaced = true;
    for (long i = ioPath.size() - 1; i >= 0; i--) {
        float theGround = theMesh.GetMeshZ(ioPath[i].GetX(), ioPath[i].GetY());
        float theTarget;
        bool theDive = this->GetUnitType() == kUnitTypeResDolphin;
        if (theDive) {
            theDive = i != 0;
            if (theDive) {
                theDive = !theSurfaced;
                if (theDive)
                    theDive = gPhysics->mRandom.Generate() < 0.35f;
            }
        }
        if (!theDive) {
            theTarget = gPhysics->GetRandom(0.1f, kSeaAnimalMaximumDepthFactor) * theGround;
            theTarget = min(kSeaAnimalMinimumDepth, theTarget);
        } else {
            theTarget = gPhysics->GetRandom(0.45f, 0.85f) * theGround;
            theSurfaced = true;
        }
        if (theSurfaced)
            theSurfaced = false;
        float theDistance = UMAbs(theDepth - theTarget);
        float theLimit;
        if (this->GetUnitType() == kUnitTypeResDolphin) {
            float theStep = gPhysics->GetRandom(0.2f, 1.2f);
            if (theDepth < theTarget)
                theDepth += theStep;
            else
                theDepth -= theStep;
            theDistance = UMAbs(theDistance - theStep);
            theLimit = -0.1f;
        } else {
            float theStep = gPhysics->GetRandom(0.2f, kSeaAnimalMaximumStep);
            if (theDepth < theTarget)
                theDepth += theStep;
            else
                theDepth -= theStep;
            theDistance = UMAbs(theDistance - theStep);
            theLimit = kSeaAnimalMinimumDepth;
        }
        theDepth = min(theLimit, theDepth);
        if (theDepth <= theGround + kSeaAnimalBottomClearance)
            theDepth = theGround + kSeaAnimalBottomClearance;
        ioPath[i].SetZ(theDepth);
    }
}

// the sea animals' AdjustPath body for ee_sim::units waypoints.rs (the depths in place)
extern "C" void ee_units_wp_host_sea_animal_path(uint64_t inClass, uint64_t inUnit, float (*ioPoints)[3], uint32_t inCount)
{
    std::vector<GE3DPoint> thePath;
    for (uint32_t i = 0; i < inCount; i++)
        thePath.push_back(GE3DPoint(ioPoints[i][0], ioPoints[i][1], ioPoints[i][2]));
    ((EEUnitClass*)(uintptr_t)inClass)->SeaAnimalAdjustPath((EEComplexUnit*)(uintptr_t)inUnit, thePath, false);
    for (uint32_t i = 0; i < inCount; i++)
        ioPoints[i][2] = thePath[i].mZ;
}

template <class T> static T* EEUcgObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

// The C++ of the ground classes' Rust bodies (ee_units ucg_ground.rs; EE_UNITS_UCG_*): one statement of the original
// bodies each, in their order; the class host's statements (EE_UNITS_UC_*) pass through to EEUnitClass.cpp's
extern "C" int64_t ee_units_ucg_host(int32_t inOp, void* inClass, void* inUnit, uint64_t inA, int64_t inB, int64_t inC,
                                     float (*ioF)[4])
{
    EEUnitClass* theClass = static_cast<EEUnitClass*>(inClass);
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEComplexUnit* theOther = EEUcgObject<EEComplexUnit>(inA);
    EEUnit* theSimple = EEUcgObject<EEUnit>(inA);
    float* f = *ioF;
    switch (inOp) {
    case EE_UNITS_UCG_TILE_X_OF: return theSimple->mTileX;
    case EE_UNITS_UCG_TILE_Y_OF: return theSimple->mTileY;
    case EE_UNITS_UCG_TILE_ADD: EEUcgObject<EEMapTile>(inA)->mUnits = EEUcgObject<EEMapTile>(inA)->AddUnit(EEUcgObject<EEUnit>((uint64_t)inB), EEUcgObject<EEMapTile>(inA)->mUnits); return 0;
    case EE_UNITS_UCG_TILE_REMOVE: EEUcgObject<EEMapTile>(inA)->RemoveUnit(EEUcgObject<EEUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UCG_QUEUED_FOR_DELETE: return theSimple->mIsQueuedForDelete;
    case EE_UNITS_UCG_BASE_SIMPLE_CREATED: theClass->BaseSimpleUnitCreated(theSimple); return 0;
    case EE_UNITS_UCG_SET_RESOURCE_TYPE: theSimple->SetResourceType(theClass->mResourceType); return 0;
    case EE_UNITS_UCG_RESOURCE_TYPE: return theSimple->mResourceType;
    case EE_UNITS_UCG_SET_RESOURCE_AMOUNT: theSimple->SetResourceAmount(theClass->mResourceAmount); return 0;
    case EE_UNITS_UCG_UNIT_TYPE: return theUnit->mUnitClass->mUnitType;
    case EE_UNITS_UCG_SET_CLASS_POWER: theUnit->SetPower(theUnit->mUnitClass->mPower); return 0;
    case EE_UNITS_UCG_MAP_TILE: return EEObj(::gWorld.GetMap()->GetTile((long)inB, (long)inC));
    case EE_UNITS_UCG_CRUISE_ALTITUDE_OF: f[0] = EEUcgObject<EEUnitClass>(inA)->mCruiseAltitude; return 0;
    case EE_UNITS_UCG_CREATE_PLANE: return EEObj(EEUcgObject<EEUnitClass>(inA)->CreateUnitAt(*EEUcgObject<GETransformation>((uint64_t)inB), 0, 0, theUnit)->mComplexUnit);
    case EE_UNITS_UCG_CARRIER_AIRCRAFT: EECreateCarrierAircraft(theOther, theUnit->mPlayer->mPlayerID); return 0;
    case EE_UNITS_UCG_NEW_UNLOAD_TRANSPORT: return EEObj(EEUGRust::NewUnloadTransport(theOther, (long)inB, (long)inC, false));
    case EE_UNITS_UCG_SET_FLIGHT_TIME_OF: theOther->SetFlightTime((unsigned long)inB); return 0;
    case EE_UNITS_UCG_SET_TRANSFORMATION: theUnit->SetTransformation(*EEUcgObject<GETransformation>(inA), true); return 0;
    case EE_UNITS_UCG_COLLISION_STATE: return theUnit->mCollisionState;
    case EE_UNITS_UCG_WAYPOINT_COUNT: return theUnit->mWaypoints.size();
    case EE_UNITS_UCG_NEXT_WAYPOINT: theUnit->GetNextWaypointPoint(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCG_COLLISION_RADIUS: f[0] = theUnit->mCollisionRadius; return 0;
    case EE_UNITS_UCG_RESOURCES_ROTTING: theUnit->UpdateResourcesRotting(); return 0;
    default: return ee_units_uc_host(inOp, inClass, inUnit, inA, inB, inC, ioF);
    }
}
