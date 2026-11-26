// EEUnitClassTransports.cpp (new, R5b.4 round 32): the transports' bodies of EEUnitClass, one class keyed by its
// kind (ee/EEUnitKind.h). Each EEUC* class was a subclass with these bodies as its overrides; they now run
// behind EEUnitClassKinds.cpp's dispatchers. Each section keeps its old file's notes and addresses.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"

#include "ee/EEPlayer.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUGRust.h"

// ---- EEUCTransport.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The transports' base
// unit class: 0x21b27a..0x21c9e1 (after EEUCTorpedo.cpp). Unit flags: the exe's module set. Its ctor is out of line in
// EEUnitClass.cpp (0x23629e); slots +0xe0, +0xe4, +0xe8, the loading helpers (two EEComplexUnit distance inlines among
// them, first emitted here), +0xf4, +0xfc, +0xf8, +0x78, +0x100, +0x104, +0xb8 in address order. The untagged
// functions 0x21c9e1..0x21ce6c before EEUGAttackGround.cpp's key chain are not placed yet (another unit's?).

// unit-local constants 0x44736c (10000: EEGetPathLengthTo's no path) and 0x447370 (25): cls_transport.rs

// The transports' tile searches are Rust (R5b.4 round 33: ee_units cls_transport.rs; the dispatchers in
// EEUnitClassKinds.cpp call it). CanLoad (0x61b613) and CanUnloadNear (0x61b83f) stay members for the load and unload
// goals' host (EEUGRust.cpp).
// The class bodies' host (ee_units cls_host.rs; EE_UNITS_CLS_*): the continent manager's lists, the unit utilities'
// leaves that keep their records, the path lengths and a unit's reads and trial location
int64_t EEUnitClassMoveOp(int32_t inOp, EEComplexUnit* inUnit, uint64_t inB, int64_t inC, int64_t inD, EeUnitsClsIo* io);

extern "C" int64_t ee_units_cls_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, EeUnitsClsIo* ioIo)
{
    EEComplexUnit* theUnit = (EEComplexUnit*)(uintptr_t)inA;
    std::vector<U2DPoint<long> > theTiles;
    if (inOp >= EE_UNITS_CLS_MV_CLOSEST_TILE)
        return EEUnitClassMoveOp(inOp, theUnit, inB, inC, inD, ioIo);   // EEUnitClass.cpp
    // (the cargo, container, command, home base, turn speeds, projectile category and map size: cls_host.rs)
    switch (inOp) {
    case EE_UNITS_CLS_NEXT_TO_AREA: return gContinentManager.IsNextToArea((long)inC, (long)inD, (unsigned short)inA);
    case EE_UNITS_CLS_SHORE_IN_REGION: ((EEAIContinent*)(uintptr_t)inA)->GetShoreInRegion((unsigned short)inB, theTiles, (long)inC); break;
    case EE_UNITS_CLS_SHORE_NEXT_TO: ((EEAIContinent*)(uintptr_t)inA)->GetShoreNextTo((unsigned short)inB, theTiles); break;
    case EE_UNITS_CLS_ADJACENT_IDS: {
        std::vector<unsigned short> theIDs;
        gContinentManager.GetAdjacentIDs(theIDs, (unsigned short)inB);
        for (size_t i = 0; i < theIDs.size(); i++)
            theTiles.push_back(U2DPoint<long>(theIDs[i], 0));
        break;
    }
    case EE_UNITS_CLS_TILES_IN_RADIUS: ee_units_uu_tiles_in_radius((long)inC, (long)inD, (uint32_t)(2), true, &theTiles); break;
    case EE_UNITS_CLS_LINE_TILES: U2DPoint<long>((long)inC, (long)inD).GetLineTiles(U2DPoint<long>((long)ioIo->a, (long)ioIo->b), 1.0f, theTiles); break;
    case EE_UNITS_CLS_SET_LOCATION: theUnit->SetLocation(GE3DPoint(ioIo->p[0], ioIo->p[1], ioIo->p[2]), false); return 0;
    case EE_UNITS_CLS_SURROUNDING: theUnit->mUnitClass->GetSurroundingTiles(theUnit, theTiles, 0, false); break;
    // the waypoints' operations: Rust (cls_host.rs waypoint_op)
    case EE_UNITS_CLS_BOMB_ON_TARGET: return ((EEUnitClass*)(uintptr_t)inB)->IsBombOnTarget(theUnit);
    case EE_UNITS_CLS_AWAY_FROM_GOAL: return ((EEUnitClass*)(uintptr_t)inB)->IsAwayFromGoal(theUnit);
    case EE_UNITS_CLS_GOAL_HAS_LOCATION: return EEGoalHasLocation(theUnit->GetGoal());
    case EE_UNITS_CLS_QUEUE_GOALS: return (int64_t)theUnit->GetQueueGoals((unsigned long)inC).size();
    case EE_UNITS_CLS_TILES_NEAR_POINT: ee_units_uu_get_tiles_near_point((const float(*)[3])ioIo->p, ioIo->f, &theTiles); break;
    case EE_UNITS_CLS_IS_SPACE_MAP: return ::gWorld.mMap->mTerrainMesh.GetIsSpaceMap();
    case EE_UNITS_CLS_ADD_WAYPOINT: ((EEUnitClass*)(uintptr_t)inB)->AddWaypointAt(theUnit, ioIo->p[0], ioIo->p[1], ioIo->p[2], inC != 0); return 0;
    }
    for (size_t i = 0; i < theTiles.size(); i++)
        ee_units_cls_push((EeUnitsClsListHandle*)ioIo->list, (int32_t)theTiles[i].mX, (int32_t)theTiles[i].mY);
    return 0;
}

// Finds where cargo leaves the transport: a tile around it for a unit that cannot move, else a free placement
// near the location (water allowed for non-land units); returns whether one was found
// Sets up a new transport as any unit, then gives it its class's resource type and amount
// Unloads the transport's cargo at a tile (siege towers with the tower flag set)
// ---- EEUCLandTransport.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The land
// transport's unit class: 0x216de3..0x217085 (after EEUCLandPriest.cpp; EEUCMiningBuilding.cpp follows). Unit flags:
// the exe's module set. Its ctor is inline (vtable 0x448930); the slots +0xe4, +0xf4, +0xf8, +0x8c, +0xb8 in address
// order. +0x8c and +0xf4 are also EEUCFortress's (ICF).

// Unloads all cargo at once when the transport is badly hurt
// ---- EEUCHelicopterTransport.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The transport
// helicopter's unit class: 0x21339d..0x213866 (after EEUCHelicopterPriest.cpp; EEUCLand.cpp follows). One unit per
// EEUC* class in alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's
// factory); slots +0xe4, +0x24, +0xf0, +0xf8, +0x74, +0x78, +0xa0, +0xb8 in address order.

// 0x23ca26 (cdecl; EEUCHelicopter.cpp declares it the same way) (provisional name)

// New (R5b.4, the fourteenth round): EEUCHelicopterTransport::ActionChanged's path to the goal when preparing to move
// (the original's statements; EEUnitUpdate.cpp's EE_UNITS_ACT_HELICOPTER_PATH calls it)
void EEHelicopterTransportPathToGoal(EEUnitClass* inClass, EEComplexUnit* inUnit) { ee_units_ucx_call(EE_UNITS_UCX_DO_HELICOPTER_PATH, inClass, inUnit, 0, 0, 0); }  // ucx_transports.rs

// Sets up a new transport helicopter as a transport, then lifts it to its flying height
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
// Adds the transport to its own tile only
// Removes the transport from its own tile only
// ---- EEUCFortress.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The fortress (a land
// transport that garrisons; unit type 0x313 is the teleporter): 0x21013b..0x21048f (after
// EEUCBuildingPriest.cpp; a globals chain opens the next unit at 0x21048f). Unit flags: the exe's module set. Its ctor
// is inline (EEUnitClass.cpp's factory).

// 0x4c8fa4 (.data, a word): the faith a teleporter spends per full unload (provisional name)
unsigned short gFortressUnloadCost = 100;

// Returns whether the player's population has room for the unit's cost
// Sets up a new fortress as built, with a 1 s regeneration timer
// Unloads at a tile; a teleporter needs 100 faith and spends it in proportion to the cargo unloaded, else warns
// its owner (event 0x274e)
// the boarding location's tiles (ucx_transports.rs)
static std::vector<EETileCoordinate> sUcxTiles;

template <class T> static T* EEUcxObject(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

// The C++ of the transports' Rust bodies (ee_units ucx_transports.rs; EE_UNITS_UCX_*): one statement of the original
// bodies each, in their order; the class host's statements (EE_UNITS_UC_*) pass through to EEUnitClass.cpp's
extern "C" int64_t ee_units_ucx_host(int32_t inOp, void* inClass, void* inUnit, uint64_t inA, int64_t inB, int64_t inC,
                                     float (*ioF)[4])
{
    EEUnitClass* theClass = static_cast<EEUnitClass*>(inClass);
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEComplexUnit* theOther = EEUcxObject<EEComplexUnit>(inA);
    EEUnit* theSimple = EEUcxObject<EEUnit>(inA);
    float* f = *ioF;
    switch (inOp) {
    case EE_UNITS_UCX_TILES_CLEAR: sUcxTiles.clear(); return 0;
    case EE_UNITS_UCX_TILES_AROUND: ee_units_uu_tiles_around((uint64_t)(uintptr_t)theUnit, inA, 1, 0, 0, &sUcxTiles); return 0;
    case EE_UNITS_UCX_PICK_TILE_NEAR: return ee_units_uu_pick_tile_near(&sUcxTiles, (uint64_t)(uintptr_t)theUnit, inA, 0, EEUcxObject<float[3]>((uint64_t)inB));
    case EE_UNITS_UCX_SET_POINT: *EEUcxObject<GE3DPoint>((uint64_t)inB) = GE3DPoint(f[0], f[1], f[2]); return 0;
    case EE_UNITS_UCX_SET_RESOURCE_TYPE: theSimple->SetResourceType(theClass->mResourceType); return 0;
    case EE_UNITS_UCX_SET_RESOURCE_AMOUNT: theSimple->SetResourceAmount(theClass->mResourceAmount); return 0;
    case EE_UNITS_UCX_COMPLEX_FIELD_OF: return (uint64_t)(uintptr_t)theSimple->mComplexUnit;
    case EE_UNITS_UCX_CARGO_OF: return (uint64_t)(uintptr_t)theOther->GetCargo((unsigned long)inB);
    case EE_UNITS_UCX_HEIGHT_AT: f[0] = theClass->GetHeightAt(theUnit, GE3DPoint(f[0], f[1], f[2])); return 0;
    case EE_UNITS_UCX_TILE_ADD: EEUcxObject<EEMapTile>(inA)->mUnits = EEUcxObject<EEMapTile>(inA)->AddUnit(EEUcxObject<EEUnit>((uint64_t)inB), EEUcxObject<EEMapTile>(inA)->mUnits); return 0;
    case EE_UNITS_UCX_TILE_REMOVE: EEUcxObject<EEMapTile>(inA)->RemoveUnit(EEUcxObject<EEUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UCX_TOTAL_POPULATION: return theUnit->mPlayer->GetTotalPopulation(false, false);
    case EE_UNITS_UCX_UNLOAD_COST: return gFortressUnloadCost;
    case EE_UNITS_UCX_SET_POWER: theUnit->SetPower((long)inB); return 0;
    case EE_UNITS_UCX_UNLOAD_EVENT: theUnit->mPlayer->mEvents.AddEvent(U2DPoint<long>((long)inB, (long)inC), 2, theUnit->GetPlayer(), 0x274e, false); return 0;
    case EE_UNITS_UCX_DESTINATION: EEUcxObject<EEUnitGoal>(inA)->GetDestination(theUnit, *reinterpret_cast<GE3DPoint*>(f)); return 0;
    default: return ee_units_uc_host(inOp, inClass, inUnit, inA, inB, inC, ioF);
    }
}
