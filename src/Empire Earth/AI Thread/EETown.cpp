// C:\EEX\Empire Earth\AI Thread\EETown.cpp (file name inferred: no __FILE__; RTTI class EETown > EEAIArea,
// vtable 0x43c57c). A player's town: 0xabbaf..0xac7b9 (EEPathManager.cpp's COMDATs end at 0xabbaf, EEAEExtraDamageFunctor's
// ctor starts at 0xac7b9).
// Port (the AI Thread cluster, round 2): the bodies are Rust's (ee_sim::cp town.rs); this file is the town's host
// (ee_cp_town_host: the units, the area the town is, the continent manager, the patrol groups) and IsTownUnit.
#include <windows.h>
#include <vector>
#include <set>
#include <map>
#include <math.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EETown.h"

static void* EETownSelf(EETown* inTown)
{
    return static_cast<void*>(inTown);
}

static void* EETownArea(EETown* inTown)
{
    return static_cast<void*>(static_cast<EEAIArea*>(inTown));
}

#define EETOWN_ARGS EETownSelf(this), EETownArea(this), static_cast<EeTownFields*>(this)

// Creates a player's town with the given id: an empty town area with no patrol group, not near water
EETown::EETown(EEPlayer* inPlayer, long inTownID) : EeTownFields(), mPlayer(inPlayer), mPatrolGroup(NULL)
{
    ee_cp_town_construct(EETOWN_ARGS, inTownID);
}

EETown::~EETown()
{
    this->Reset();
}

bool EETown::AddUnit(EEUnit* inUnit)
{
    return ee_cp_town_add_unit(EETOWN_ARGS, (uint64_t)(uintptr_t)inUnit);
}

bool EETown::AddTile(const U2DPoint<long>& inTile)
{
    return ee_cp_town_add_tile(EETOWN_ARGS, inTile.mX, inTile.mY);
}

void EETown::Merge(EETown* inOther)
{
    ee_cp_town_merge(EETOWN_ARGS, (uint64_t)(uintptr_t)inOther, static_cast<EeTownFields*>(inOther));
}

void EETown::Reset()
{
    ee_cp_town_reset(EETOWN_ARGS);
}

bool EETown::UpdateNearWater()
{
    return ee_cp_town_update_near_water(EETOWN_ARGS);
}

// a Rust point as the U2DPoint it is
static U2DPoint<long> EETownPoint(const EeHerdPoint& inPoint)
{
    return U2DPoint<long>(inPoint.mX, inPoint.mY);
}

U2DPoint<long> EETown::GetRandomPoint()
{
    return EETownPoint(ee_cp_town_random_point(EETOWN_ARGS));
}

bool EETown::RemoveUnit(EEUnit* inUnit)
{
    return ee_cp_town_remove_unit(EETOWN_ARGS, (uint64_t)(uintptr_t)inUnit);
}

void EETown::RefreshPatrolGroup()
{
    ee_cp_town_refresh_patrol_group(EETOWN_ARGS);
}

bool EETown::RemoveHullTile(const U2DPoint<long>& inTile)
{
    return ee_cp_town_remove_hull_tile(EETOWN_ARGS, inTile.mX, inTile.mY);
}

bool EETown::CanTake(EEUnit* inUnit)
{
    return ee_cp_town_can_take(EETOWN_ARGS, (uint64_t)(uintptr_t)inUnit);
}

// NEAR: theResult is kept in memory where the original keeps it in bl (the zero register of the NULL tests)
// Returns whether a unit is a placed, immobile, non-ambient land building on a land continent (walls only when
// a human player owns them)
bool EETown::IsTownUnit(EEUnit* inUnit)
{
    return ee_cp_town_is_town_unit((uint64_t)(uintptr_t)inUnit);
}

// a tile as one word (x high, y low; ee_sim::cp town_world.rs unpack)
static int64_t EETownPack(const EETileCoordinate& inTile)
{
    return (int64_t)(((uint64_t)(uint32_t)inTile.mX << 32) | (uint32_t)inTile.mY);
}

extern "C" int64_t ee_cp_town_host(void* inTown, int32_t inOp, uint64_t inA, uint64_t inB, TownIo* io)
{
    EETown* me = static_cast<EETown*>(inTown);
    EEUnit* theUnit = reinterpret_cast<EEUnit*>((uintptr_t)inA);
    (void)inB;
    switch (inOp) {
    case EE_CP_TOWN_IS_TOWN_UNIT:
        return EETown::IsTownUnit(theUnit) ? 1 : 0;
    case EE_CP_TOWN_PLAYER:
        return (int64_t)(uintptr_t)me->mPlayer;
    case EE_CP_TOWN_HAS_PATROL:
        return me->mPatrolGroup != NULL ? 1 : 0;
    case EE_CP_TOWN_SET_PATROL: EEPGSetPatrolPoints(me->mPatrolGroup, std::vector<U2DPoint<long> >((const U2DPoint<long>*)(uintptr_t)io->v[0], (const U2DPoint<long>*)(uintptr_t)io->v[0] + io->v[1]), me->mPlayer->GetPlayerID());
        return 0;
    case EE_CP_TOWN_FLAGS:
        return (gLoadingSavedGame ? 1 : 0) | (gWorldClearing ? 2 : 0);
    case EE_CP_TOWN_NODE_KIND:
        return theUnit->GetTechTreeNode() != NULL ? (int64_t)theUnit->GetTechTreeNode()->mData->mKind : -1;
    case EE_CP_TOWN_AREA_TILE:
        return EETownPack(theUnit->FindAreaTile(true, 1));
    case EE_CP_TOWN_SET_PATROL_GROUP: me->mPatrolGroup = reinterpret_cast<EEPPatrolGroup*>((uintptr_t)inA);
        return 0;
    case EE_CP_TOWN_ADD_PATROL_GROUP: me->mPlayer->AddPatrolGroup(me->mPatrolGroup);
        return 0;
    case EE_CP_TOWN_PATROL_GROUP:
        return (int64_t)(uintptr_t)me->mPatrolGroup;
    case EE_CP_TOWN_HUMAN_OWNER:
        return theUnit->mPlayer->mHumanPlayer != NULL ? 1 : 0;
    case EE_CP_TOWN_MAP_SIZE:
        return (int64_t)(((uint64_t)(uint32_t)gMapHeight << 32) | (uint32_t)gMapWidth);
    }
    return 0;
}
