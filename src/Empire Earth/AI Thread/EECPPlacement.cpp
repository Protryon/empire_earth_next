// C:\EEX\Empire Earth\AI Thread\EECPPlacement.cpp (file name inferred: no __FILE__; RTTI class
// EECPPlacement, vtable 0x43c050; its key "EECPPlacement-Data" names the class). A computer player's build
// manager: 0x66af0..0x6e8d4 (the class-static key's guarded chain first, then three distance predicates built without
// /Og, the members in address order; the deque/vector/heap/sort COMDATs close the unit; EECPAttackSubGroupUnit.cpp ends
// at 0x66af0 and EECPCivilizationManager.cpp's key chain starts at 0x6e8d4).
//
// AI lane B, round 2: the class is Rust's (ee_sim::cp::placement, rust/sim/cp.h): every search, CanBuildAt, the "unit
// near" tests, the site tiles and the classes seen (the Rust object mRust), UnitAdded, Reset, Restore, the epoch's
// classes, EECPCountOwn and EECPNextPlaceSearch. What stays here: the members other files read (mTownCenterTile,
// mPlayer), the forwarders with their TEST ONLY PLCE scopes, the town tile's Load and Save (U2DPoint's), the two
// EETileDistanceLess comparators other units sort with, and ee_cp_place_host: the world the Rust bodies ask.
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <initializer_list>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIContinent.h"
#include "ee/EETown.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EECPPlacement.h"
#include "ee/EECPTrace.h"
#include "ee/EECPNext.h"
#include "ee/EESimCapture.h"
#include "rust/sim/cp.h"
#include "rust/sim/ai.h"
#include "rust/sim/astar.h"
#include "rust/sim/units.h"

// ---------------------------------------------------------------------------------------------------------------
// The Rust bodies' context and world

static uint64_t EECPHandle(const void* inObject)
{
    return (uint64_t)(uintptr_t)inObject;
}

template <class T> static T* EECPObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>((uintptr_t)inHandle);
}

// The placement's world (ee_sim::cp::place_game's EE_CP_PLACE_* operations): the original's calls, one each
extern "C" int64_t ee_cp_place_host(const EeCpPlaceCtx* inContext, int32_t inOp, uint64_t inA, uint64_t inB,
    EeCpPlaceIo* io)
{
    EEComputerPlayer* thePlayer = static_cast<EEComputerPlayer*>(EECPObject<EEPlayer>(inContext->player));
    const int64_t* v = io->v;
    switch (inOp) {
    case EE_CP_PLACE_EPOCH:
        return thePlayer->mTechnologies->mCurrentEpoch;
    case EE_CP_PLACE_PASSABILITY:
        return thePlayer->GetCitizenPassability();
    case EE_CP_PLACE_RANGE: io->f[0] = thePlayer->mUnitClassUpgrades[(long)v[0]]->mRange;
        return 0;
    case EE_CP_PLACE_TOWN_FIELDS:
        return (int64_t)EECPHandle(static_cast<EeTownFields*>(EECPObject<EETown>(inA)));
    case EE_CP_PLACE_HAS_PATHFINDING:
        return gCivPowersManager.HasPower(thePlayer, kCivPowerPathfinding) ? 1 : 0;
    case EE_CP_PLACE_HAS_PLAYER0:
        return gWorld.mPlayers[0] != NULL ? 1 : 0;
    case EE_CP_PLACE_WORLD_UNIT:
        return (int64_t)EECPHandle(gWorld.GetUnit((unsigned long)v[0]));
    case EE_CP_PLACE_CLASS_COUNT:
        return (int64_t)EEUnitClassCount();
    case EE_CP_PLACE_CLASS_EXISTS:
        return gUnitClassTable[(long)v[0]] != NULL ? 1 : 0;
    case EE_CP_PLACE_CLASS_TILE_SIZE:
        return gUnitClassTable[(long)v[0]]->GetTileSize();
    case EE_CP_PLACE_CLASS_TERRAIN:
        return gUnitClassTable[(long)v[0]]->mTerrainTypes[(long)v[1]] ? 1 : 0;
    case EE_CP_PLACE_CLASS_TECHNOLOGY:
        return gUnitClassTable[(long)v[0]]->GetTechnology();
    case EE_CP_PLACE_HAS_MAP:
        return gWorld.mMap != NULL ? 1 : 0;
    case EE_CP_PLACE_MAP_FLAGS:
        return (int64_t)gWorld.mMap->mMapCharacteristics;
    case EE_CP_PLACE_IS_EDGE:
        return gWorld.mMap->IsEdgeTile((long)v[0], (long)v[1]) ? 1 : 0;
    case EE_CP_PLACE_LARGEST_CONTINENT:
        return gContinentManager.GetLargestContinent((long)v[0]);
    case EE_CP_PLACE_CONTINENT_FROM_ID:
        return (int64_t)EECPHandle(gContinentManager.GetContinentFromID((unsigned short)v[0]));
    case EE_CP_PLACE_CONTINENT_BY_ID:
        return (int64_t)EECPHandle(gContinentManager.GetContinentByID((unsigned short)v[0]));
    case EE_CP_PLACE_CONTINENT_AT:
        return (int64_t)EECPHandle(gContinentManager.GetContinentAt((long)v[0], (long)v[1]));
    case EE_CP_PLACE_CONTINENT_ID:
        return EECPObject<EEAIContinent>(inA)->GetID();
    case EE_CP_PLACE_CONTINENT_FLAG:
        return gContinentManager.AreAdjacent((long)v[0], (long)v[1]) ? 1 : 0;
    case EE_CP_PLACE_ARE_CONNECTED:
        return ee_astar_cmq_are_connected((uint64_t)(uintptr_t)(EECPObject<EEAIContinent>(inA)), (uint64_t)(uintptr_t)(EECPObject<EEAIContinent>(inB)), (long)v[0]) ? 1 : 0;
    case EE_CP_PLACE_TILE_CONTINENT:
        return gContinentManager.GetContinent(EETileCoordinate((long)v[0], (long)v[1]), kAIPathLandMass);
    case EE_CP_PLACE_UNIT_CONTINENT:
        return gContinentManager.GetContinent(EECPObject<EEUnit>(inA), kAIPathLandMass);
    case EE_CP_PLACE_CONTINENT_ID_AT:
        return gContinentManager.GetContinentID((long)v[0], (long)v[1]);
    case EE_CP_PLACE_CHOKE_AT:
        return gContinentManager.GetChokePointAt(EETileCoordinate((long)v[0], (long)v[1])) != NULL ? 1 : 0;
    case EE_CP_PLACE_HILL_AT:
        return gContinentManager.GetHillAt(EETileCoordinate((long)v[0], (long)v[1])) != NULL ? 1 : 0;
    case EE_CP_PLACE_PATH_AREA_AT:
        return (int64_t)EECPHandle(gContinentManager.GetPathAreaAt(EETileCoordinate((long)v[0], (long)v[1])));
    case EE_CP_PLACE_UNIT_PATH_AREA:
        return (int64_t)EECPHandle(gContinentManager.GetPathAreaAt(EECPObject<EEUnit>(inA)));
    case EE_CP_PLACE_FACING_QUARTER:
        return (long)ee_units_ucn_facing(2, (int32_t)((long)v[0]), (int32_t)((long)v[1]));
    case EE_CP_PLACE_SITE_THREAT:
        return ee_ai_next_site_threat(thePlayer, (long)v[0], (long)v[1], false, NULL) != 0 ? 1 : 0;
    case EE_CP_PLACE_RANDOM:
        return gPhysics->GetRandom(0L, (long)v[0]);
    case EE_CP_PLACE_SPACE_DOCK_OPEN:
        return EEIsSpaceDockSiteOpen((long)v[0], (long)v[1], thePlayer) ? 1 : 0;
    }
    return 0;
}

// (EECPPlacement's construction, destruction, Load and Save: Rust, ee_sim::cp player_game.rs)
// (the epoch's classes and the Lua verbs' placement search are Rust: ee_sim::ai units_world.rs)
