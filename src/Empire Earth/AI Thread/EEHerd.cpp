// C:\EEX\Empire Earth\AI Thread\EEHerd.cpp (file name inferred: no __FILE__; RTTI class EEHerd > EEAIArea,
// vtable 0x43c2e8; the save key "EEHerd-SaveLoadData"). A herd of wild animals as an AI area: 0xa1910..0xa543d
// (EECPWallManager/EEHerd boundary 0xa1910; EEHumanPlayer's ctor starts at 0xa543d).
// Port (the AI Thread cluster, round 2): the herd's bodies are Rust's (ee_sim::cp herd.rs); the class forwards to
// them, and this file is the herd's host (ee_cp_herd_host): the units, the goals, the formations, the map, the
// continent manager and the area the herd is.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAnimal.h"
#include "ee/EEMedia.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEFormation.h"
#include "ee/EEUGRust.h"
#include "ee/EEHerd.h"
#include "ee/EESaveGameFiles.h"

const UString EEHerd::sData("EEHerd-SaveLoadData");
// 0x4e5e40, right after sData (0x4e5e30 + 0x10) in the original's initialised .data, value 0; its only use clears it
// (EEWorld.cpp). Owner by placement only (EECalamity's statics start at 0x4e5e48): PROVISIONAL
long gHerdUnreadValue = 0;

EEHerd::~EEHerd()
{
    ee_cp_herd_clear(this, this);
    ee_cp_herd_free(this);
}

static_assert(sizeof(EeHerdPoint) == sizeof(U2DPoint<long>), "a herd point is a U2DPoint<long>");

// Returns whether a unit is a placed animal or fish of a known animal type
bool EEHerd::IsAnimalOnMap(EEUnit* inUnit)
{
    return (inUnit->mUnitClass->mFamily == kUnitFamilyAnimal || inUnit->mUnitClass->mFamily == kUnitFamilyFish) &&
        EEAnimal::Find(inUnit->mUnitClass->mUnitType) && inUnit->mTileX != -10000 && inUnit->mTileY != -10000;
}


void EEHerd::GetUnits(std::vector<EEComplexUnit*>& outUnits)
{
    for (size_t i = 0, n = ee_cp_herd_unit_count(this); i < n; i++) {
        EEComplexUnit* theUnit;
        EEUnitHandleResolve(ee_cp_herd_unit_handle(this, i), theUnit);
        outUnits.push_back(theUnit);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The herd's host (ee_sim::cp herd.rs EE_CP_HERD_*, herd_world.rs): `a`, `b` units (EEComplexUnit*), herds, animals,
// goals, formations or media; one statement each (the unit's record, the formations, CanStandOn, the goals, the birth
// and the units in a rectangle are Rust's: herd_world.rs)

static EEComplexUnit* EEHerdUnit(uint64_t inHandle) { return reinterpret_cast<EEComplexUnit*>((uintptr_t)inHandle); }

static uint64_t EEHerdHandle(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }

// a point as one word (x high, y low; ee_sim::cp town_world.rs unpack)
static int64_t EEHerdPack(const U2DPoint<long>& inPoint) { return (int64_t)(((uint64_t)(uint32_t)inPoint.mX << 32) | (uint32_t)inPoint.mY); }

// The area operations of the random point (EETown::GetRandomPoint 0xac203, the herd's by ICF; ee_sim::cp herd.rs
// random_area_point) the Rust records do not answer: the draws, the base point, the map's tile
extern "C" int64_t ee_cp_area_host(void* inArea, int32_t inOp, EeHerdIo* io)
{
    EEAIArea* me = static_cast<EEAIArea*>(inArea);
    const int64_t* v = io->v;
    switch (inOp) {
    case EE_CP_HERD_RANDOM_LONG: return gPhysics->GetRandom((long)v[0], (long)v[1]);
    case EE_CP_HERD_BASE_POINT: return EEHerdPack(me->AreaRandomPoint());
    case EE_CP_HERD_MAP_TILE_AT: return gWorld.mMap->GetTileAtLocation(U2DPoint<long>((long)v[0], (long)v[1])) != NULL ? 1 : 0;
    }
    return 0;
}

// (the area's, the continent manager's, the registry's and the players' reads are Rust's: herd_game.rs world_op)
extern "C" int64_t ee_cp_herd_host(void* inHerd, int32_t inOp, uint64_t inA, uint64_t inB, EeHerdIo* io)
{
    EEHerd* me = static_cast<EEHerd*>(inHerd);
    EEComplexUnit* theUnit = EEHerdUnit(inA);
    EEFormation* theFormation = reinterpret_cast<EEFormation*>((uintptr_t)inB);
    EEUGRust* theGoal = reinterpret_cast<EEUGRust*>((uintptr_t)inB);
    const int64_t* v = io->v;
    switch (inOp) {
    case EE_CP_HERD_REF: theUnit->mReferenceCount++;
        return 0;
    case EE_CP_HERD_DEREF: theUnit->Dereference();
        return 0;
    case EE_CP_HERD_SET_HERD: theUnit->SetHerd(reinterpret_cast<EEHerd*>((uintptr_t)inB));
        return 0;
    case EE_CP_HERD_SET_GRAZE: theUnit->SetGrazeTime((unsigned long)v[0]);
        return 0;
    case EE_CP_HERD_SET_GROWTH: theUnit->SetGrowth(io->f[0]);
        return 0;
    case EE_CP_HERD_SET_GROW_TIME: theUnit->SetGrowTime((unsigned long)v[0]);
        return 0;
    case EE_CP_HERD_RANDOM_FLOAT: io->f[0] = gPhysics->GetRandom(io->f[0], io->f[1]);
        return 0;
    case EE_CP_HERD_RANDOM: io->f[0] = gPhysics->GetRandom();
        return 0;
    case EE_CP_HERD_GENERATE: io->f[0] = gPhysics->mRandom.Generate();
        return 0;
    case EE_CP_HERD_AREA_POINT: return EEHerdPack(reinterpret_cast<EEAIArea*>((uintptr_t)inA)->GetRandomPoint());
    case EE_CP_HERD_MAP_EDGE: return (int64_t)EEHerdHandle(gContinentManager.GetRandomContinent(kContinentCategoryMapEdge, gPhysics->mRandom));
    case EE_CP_HERD_UNIT_WAYPOINTS: return (int64_t)theUnit->mWaypoints.size();
    case EE_CP_HERD_UNIT_MODEL: return (int64_t)EEHerdHandle((EEMedia*)theUnit->mStats->mModel);
    case EE_CP_HERD_FORM_NEW: return (int64_t)EEHerdHandle(EEFormation::Create(6));
    case EE_CP_HERD_FORM_DEREF: theFormation->Dereference();
        return 0;
    case EE_CP_HERD_PASSABLE_BY: return reinterpret_cast<EEUnit*>((uintptr_t)inB)->IsPassableBy(theUnit) ? 1 : 0;
    case EE_CP_HERD_CAN_MOVE_ON: return theUnit->GetUnitClass()->CanMoveOnTerrain(theUnit, (long)v[0]) ? 1 : 0;
    case EE_CP_HERD_TILE_AT: return (int64_t)EEHerdHandle(gWorld.mMap->GetTileAtLocation(U2DPoint<long>((long)v[0], (long)v[1])));
    case EE_CP_HERD_ATTACKER_NULL: theUnit->mLastAttacker = NULL;
        return 0;
    case EE_CP_HERD_RELEASE_HELD: theUnit->mHolder->ReleaseHeld(theUnit);
        return 0;
    case EE_CP_HERD_HOLDER_NULL: theUnit->mHolder = NULL;
        return 0;
    case EE_CP_HERD_GOAL_AT: return (int64_t)EEHerdHandle(theUnit->mGoalQueues[0].mGoals[(size_t)v[0]]);
    case EE_CP_HERD_GOAL_IS_LOCATION: return EELocationGoalCast(theGoal) != NULL ? 1 : 0;
    case EE_CP_HERD_GOAL_POINT: *reinterpret_cast<GE3DPoint*>(io->f) = theGoal->GetLocation();
        return 0;
    case EE_CP_HERD_NEW_LOCATION:
        return (int64_t)EEHerdHandle(EEUGRust::NewLocation(io->f[0], io->f[1], io->f[2], v[0] != 0, v[1] != 0, v[2] != 0, io->f[3], false, false));
    case EE_CP_HERD_NEW_LOCATION_AT:
        return (int64_t)EEHerdHandle(EEUGRust::NewLocation(reinterpret_cast<GE3DPoint*>(io->f), v[0] != 0, v[1] != 0, v[2] != 0, io->f[3], false, false));
    case EE_CP_HERD_NEW_ATTACK: return (int64_t)EEHerdHandle(EEUGRust::NewAttackUnit(theUnit, static_cast<EEUnit*>(EEHerdUnit(inB)), true, false));
    case EE_CP_HERD_SET_GOAL: theUnit->SetGoal(theGoal, v[0] != 0, 0);
        return 0;
    case EE_CP_HERD_START_ACTION: theUnit->StartAction((long)v[0]);
        return 0;
    case EE_CP_HERD_ACTION_CHANGED: theUnit->mUnitClass->ActionChanged(theUnit);
        return 0;
    case EE_CP_HERD_DELETE_GOAL: delete theGoal;
        return 0;
    case EE_CP_HERD_ANIMAL_CLASS: return (int64_t)EEHerdHandle(reinterpret_cast<EEAnimal*>((uintptr_t)inA)->GetUnitClass());
    case EE_CP_HERD_NEWBORN_GROWTH: io->f[0] = reinterpret_cast<EEAnimal*>((uintptr_t)inA)->GetNewbornGrowth();
        return 0;
    case EE_CP_HERD_CREATE_UNIT:
        return (int64_t)EEHerdHandle(reinterpret_cast<EEUnitClass*>((uintptr_t)inA)->CreateUnit((long)v[0], (long)v[1], (unsigned long)v[2], 0, NULL));
    case EE_CP_HERD_TO_COMPLEX: return (int64_t)EEHerdHandle(dynamic_cast<EEComplexUnit*>(reinterpret_cast<EEUnit*>((uintptr_t)inA)));
    case EE_CP_HERD_SET_TARGET_SEARCH: theUnit->SetTargetSearchTimer((long)v[0]);
        return 0;
    case EE_CP_HERD_HOLD_UNIT: theUnit->HoldUnit(EEHerdUnit(inB));
        return 0;
    case EE_CP_HERD_MEDIA_LOAD: reinterpret_cast<EEMedia*>((uintptr_t)inA)->LoadMedia();
        return 0;
    case EE_CP_HERD_MEDIA_UNLOAD: reinterpret_cast<EEMedia*>((uintptr_t)inA)->UnloadMedia();
        return 0;
    case EE_CP_HERD_ANIMAL: return (int64_t)EEHerdHandle(EEAnimal::Get((long)v[0]));
    case EE_CP_HERD_ANIMAL_FIELDS: return (int64_t)EEHerdHandle(static_cast<EeDataAnimalFields*>(reinterpret_cast<EEAnimal*>((uintptr_t)inA)));
    case EE_CP_HERD_ANIMAL_MEDIA: return (int64_t)EEHerdHandle(reinterpret_cast<EEAnimal*>((uintptr_t)inA)->mMedia);
    case EE_CP_HERD_FLEE: theUnit->mUnitClass->Flee(theUnit);
        return 0;
    case EE_CP_HERD_DAMAGE_FACTOR: io->f[0] = EEDamageFactor((long)v[0], (long)v[1]);
        return 0;
    case EE_CP_HERD_HELD: return (int64_t)EEHerdHandle(theUnit->mHeldUnits[(size_t)v[0]]);
    case EE_CP_HERD_WAYPOINT: memcpy(io->f, &theUnit->mWaypoints[(size_t)v[0]].mLocation.mX, 3 * sizeof(float));
        return 0;
    case EE_CP_HERD_POSITION: theUnit->GetPosition(*reinterpret_cast<GE3DPoint*>(io->f));
        return 0;
    case EE_CP_HERD_DISTANCE: io->f[0] = theUnit->DistanceSquaredTo(EEHerdUnit(inB));
        return 0;
    case EE_CP_HERD_MAP_Z: io->f[0] = gWorld.mMap->mTerrainMesh.GetZ((long)v[0], (long)v[1]);
        return 0;
    case EE_CP_HERD_MAP_SIZE: {
        io->v[0] = (int64_t)gMapWidth;
        io->v[1] = (int64_t)gMapHeight;
        io->v[2] = gWorld.GetMap()->GetSize();
        return 0;
    }
    case EE_CP_HERD_FIELDS_OF: return (int64_t)EEHerdHandle(static_cast<EeHerdFields*>(reinterpret_cast<EEHerd*>((uintptr_t)inA)));
    case EE_CP_HERD_GOALS: return (int64_t)theUnit->mGoalQueues[0].mGoals.size();
    case EE_CP_HERD_FLAGS: return (gWorld.mLoadingSavedGame ? 1 : 0) | (gWorld.mClearing ? 2 : 0);
    case EE_CP_HERD_PLAYER: return (int64_t)EEHerdHandle(gWorld.GetPlayer((unsigned long)v[0]));
    case EE_CP_HERD_UNIT_BY_ID: return (int64_t)EEHerdHandle(dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)v[0]))));
    case EE_CP_HERD_SAVE_FLAGS:
        return (EESaveHasLiveState((unsigned long)v[0], (unsigned long)v[1]) ? 1 : 0)
            | (EESaveHasContinentRegions((unsigned long)v[0], (unsigned long)v[1]) ? 2 : 0);
    case EE_CP_HERD_ANIMAL_ON_MAP: return EEHerd::IsAnimalOnMap(theUnit) ? 1 : 0;
    }
    return ee_cp_area_host(static_cast<EEAIArea*>(me), inOp, io);
}
