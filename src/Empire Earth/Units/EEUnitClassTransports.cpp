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

// ---- EEUCHelicopterTransport.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass).
// The transport
// helicopter's unit class: 0x21339d..0x213866 (after EEUCHelicopterPriest.cpp; EEUCLand.cpp follows). One unit per
// EEUC* class in alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's
// factory); slots +0xe4, +0x24, +0xf0, +0xf8, +0x74, +0x78, +0xa0, +0xb8 in address order.


// ---- EEUCFortress.cpp (until R5b.4 round 32; its class is now a kind of EEUnitClass). The fortress (a land
// transport that garrisons; unit type 0x313 is the teleporter): 0x21013b..0x21048f (after
// EEUCBuildingPriest.cpp; a globals chain opens the next unit at 0x21048f). Unit flags: the exe's module set. Its ctor
// is inline (EEUnitClass.cpp's factory).

// 0x4c8fa4 (.data, a word): the faith a teleporter spends per full unload (provisional name)
unsigned short gFortressUnloadCost = 100;


