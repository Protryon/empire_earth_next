// EEUnitClassGround.cpp (new, R5b.4 round 32): the ambient, resource, land and naval kinds' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>
#include <stdio.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEWheel.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitClassKinds.h"   // EEObj (the host)

// ---- EEUCAmbient.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The ambient object's
// unit class: 0x20b9fc..0x20bcf7 (after EEUCAirplane.cpp; EEUCBasicCitizen.cpp follows). Unit flags: the exe's module
// set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x04, +0x08, +0x64, +0x7c, +0x84, +0x98, +0xb8 in
// source order (unit type 0x234 sits on the water plane).

// For cattails, returns the tile where the view line through a point meets the water plane; otherwise sets nothing
// Adds the unit to every map tile of its footprint, its own tile first
// Removes the unit from every map tile of its footprint, its own tile first
// ---- EEUCResource.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The resource's unit
// class: 0x219c49..0x219d67 (after EEUCProjectile.cpp; EEUCSeaAnimal.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline (no new site in the range); +0x70/+0x98 are ICF-folded elsewhere.

// Sets up a new resource unit with the class's resource type and amount
// Adds the resource to every tile of its footprint (its own tile first), unless it is queued for deletion
// ---- EEUCLand.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The land unit class:
// 0x213866..0x213c60 (after EEUCHelicopterTransport.cpp; EEUCLandable.cpp follows). One unit per EEUC* class in
// alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots +0x78,
// +0x74, +0xe0 (the paratroopers), +0xbc, +0xb8 in address order, then EEComplexUnit::UpdateWheels's COMDAT after its
// first user VirtualB8.

// 0x20b6ff (EEUCAirplane.cpp): the plane's flight for inPlayer (provisional name)

// 0xde4a9 (EEE2DBattlefieldAmbient.cpp's range): the unit's battlefield ambience (provisional name)

// Sets up a new land unit: its class's resource (and amount when it has one) and, for the radio man, its power
// Calls a paratroop plane (class 0x2e1): it appears over the last map tile beyond the unit in +y and flies to the
// unit's goal location to unload its cargo
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
// Returns whether the ship may stop: already sinking, a slow request, or at the path's end tile with its speed reached
// Updates a fish resource: while its resources rot it loses one every 4 s; a rotting unit counts its rotting time down
// ---- EEUCSubmergedNaval.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The submarine's
// unit class (EEUCSeaAnimal shares most slots by ICF): 0x21ab66..0x21b163 (after EEUCSpaceTransport.cpp;
// EEUCTorpedo.cpp follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); slots
// +0x04, +0x08, +0x34, +0x30, +0x74, +0x84, +0x88, +0xa0, +0xb8 in address order.

// 0x23f27e (cdecl): moves a location to the depth a submerged unit may take at its tile (provisional name)

// Lifts a location that lies below the terrain mesh (the sea floor) up onto it
// Returns the tile where the view line through a point meets the submarine's cruise depth (or the sea
// floor where that is higher)
// Creates a submarine at a placement kept above the sea floor, then moves it to the depth it may take there
// Creates a submarine on a tile (yaw pi) kept above the sea floor, then moves it to the depth it may take there
// Adds the submarine to its tile's unit list
// Returns whether the submarine reached its waypoint: on the waypoint's tile with more to go, when its collision
// radius covers the distance left; otherwise when that distance is at most 0.05 (always in collision state 8)
// ---- EEUCSeaAnimal.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The sea animal's unit
// class: 0x219d67..0x219fe3 (after EEUCResource.cpp; EEUCSpaceBuilding.cpp follows). Unit flags: the exe's module
// set. Its ctor is inline. Data (.rdata, in order): the four named depth constants 0x447340..0x44734c (ucg_sea.rs).

template <class T> static T* EEUcgObject(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

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
    case EE_UNITS_UCG_TILE_ADD: EEUcgObject<EEMapTile>(inA)->mUnits = EEUcgObject<EEMapTile>(inA)->AddUnit(EEUcgObject<EEUnit>((uint64_t)inB), EEUcgObject<EEMapTile>(inA)->mUnits); return 0;
    case EE_UNITS_UCG_TILE_REMOVE: EEUcgObject<EEMapTile>(inA)->RemoveUnit(EEUcgObject<EEUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UCG_BASE_SIMPLE_CREATED: theClass->BaseSimpleUnitCreated(theSimple); return 0;
    case EE_UNITS_UCG_SET_RESOURCE_TYPE: theSimple->SetResourceType(theClass->mResourceType); return 0;
    case EE_UNITS_UCG_SET_RESOURCE_AMOUNT: theSimple->SetResourceAmount(theClass->mResourceAmount); return 0;
    case EE_UNITS_UCG_SET_CLASS_POWER: theUnit->SetPower(theUnit->mUnitClass->mPower); return 0;
    case EE_UNITS_UCG_MAP_TILE: return EEObj(::gWorld.GetMap()->GetTile((long)inB, (long)inC));
    case EE_UNITS_UCG_CREATE_PLANE: return EEObj(EEUcgObject<EEUnitClass>(inA)->CreateUnitAt(*EEUcgObject<GETransformation>((uint64_t)inB), 0, 0, theUnit)->mComplexUnit);
    case EE_UNITS_UCG_CARRIER_AIRCRAFT: EECreateCarrierAircraft(theOther, theUnit->mPlayer->mPlayerID); return 0;
    case EE_UNITS_UCG_NEW_UNLOAD_TRANSPORT: return EEObj(EEUGRust::NewUnloadTransport(theOther, (long)inB, (long)inC, false));
    case EE_UNITS_UCG_SET_FLIGHT_TIME_OF: theOther->SetFlightTime((unsigned long)inB); return 0;
    case EE_UNITS_UCG_WAYPOINT_COUNT: return theUnit->mWaypoints.size();
    case EE_UNITS_UCG_NEXT_WAYPOINT: theUnit->GetNextWaypointPoint(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCG_WORLD_LINE: memcpy(EEUcgObject<GE3DLine>(inA), &::gWorld.mLine, sizeof(GE3DLine)); return 0;
    case EE_UNITS_UCG_MESH_Z: f[0] = ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(f[0], f[1]); return 0;
    case EE_UNITS_UCG_GROUND_POINT: ::gWorld.mMap->GetGroundPoint(GE3DPoint(f[0], f[1], f[2]), *reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UCG_TERRAIN_Z: f[0] = ::gWorld.mMap->mTerrainMesh.GetZ((unsigned long)inB, (unsigned long)inC); return 0;
    case EE_UNITS_UCG_RANDOM_GENERATE: f[0] = gPhysics->mRandom.Generate(); return 0;
    case EE_UNITS_UCG_RANDOM_RANGE: f[0] = gPhysics->GetRandom(f[0], f[1]); return 0;
    case EE_UNITS_UCG_WHEEL_UPDATE: theUnit->mWheels[inB]->Update(theUnit); return 0;
    default: return ee_units_uc_host(inOp, inClass, inUnit, inA, inB, inC, ioF);
    }
}
