// New (port), TEST ONLY: the host of the two capture sweeps whose bodies are Rust (the units' lane, round 5): the
// units sweep of EE-SIM --unit-sweep (ee_sim units unit_sweep.rs, the capture's units profile) and the active
// groups' sweep (ag_sweep.rs, the groups profile, its AGPL records). The Rust reads the world through the views and
// writes the AGPL record (the world lane, round 2); each case here is one call the sweeps make: a unit made or
// produced, a group or goal made, a group's or unit's member call.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUGRust.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEFormation.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"

namespace {

template <class T> T* Of(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

}   // namespace

// ee_sim units unit_sweep.rs EE_UNITS_SWEEP_OP_* (the point: three floats a call reads or writes, the fourth a move's
// yaw)
extern "C" int64_t ee_units_sweep_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, float* ioPoint)
{
    GE3DPoint& thePoint = *reinterpret_cast<GE3DPoint*>(ioPoint);
    switch (inOp) {
    case EE_UNITS_SWEEP_OP_IS_BUILDING: return Of<EEUnitClass>(inA)->IsBuilding() ? 1 : 0;
    case EE_UNITS_SWEEP_OP_IS_LAND_BUILDING: return Of<EEUnitClass>(inA)->IsLandBuilding() ? 1 : 0;
    case EE_UNITS_SWEEP_OP_IS_AIRCRAFT: return Of<EEUnitClass>(inA)->IsAircraft() ? 1 : 0;
    case EE_UNITS_SWEEP_OP_MODEL_KIND: return Of<EEUnitClass>(inA)->mPhysicsModel != NULL ? Of<EEUnitClass>(inA)->mPhysicsModel->mKind : 0xff;
    case EE_UNITS_SWEEP_OP_CREATE_UNIT: return (int64_t)(uintptr_t)Of<EEUnitClass>(inA)->CreateUnit((long)inC, (long)inD, Of<EEPlayer>(inB)->GetPlayerID(), false, NULL);
    case EE_UNITS_SWEEP_OP_ADD_PENDING: Of<EEPlayer>(inA)->AddPendingUnits(false, true); return 0;
    case EE_UNITS_SWEEP_OP_FLUSH_PLACEMENTS: gWorld.FlushQueuedPlacements(); return 0;
    case EE_UNITS_SWEEP_OP_SET_BUILT: Of<EEUnit>(inA)->GetComplexUnit()->SetBuilt(1, 0, 1); return 0;
    case EE_UNITS_SWEEP_OP_PLACE_ON_GROUND: gWorld.mMap->PlaceUnitOnGround(Of<EEUnit>(inA)); return 0;
    case EE_UNITS_SWEEP_OP_PRODUCE_UNIT: return Of<EEComplexUnit>(inA)->mUnitClass->ProduceUnit(Of<EEComplexUnit>(inA), Of<EEUnitClass>(inB), false) ? 1 : 0;
    case EE_UNITS_SWEEP_OP_NEW_ATTACK_ORDER: return (int64_t)(uintptr_t)EEUnitGoal::NewAttackUnit(Of<EEComplexUnit>(inA), Of<EEUnit>(inB), true, false);
    case EE_UNITS_SWEEP_OP_POSITION: Of<EEUnit>(inA)->GetPosition(thePoint); return 0;
    case EE_UNITS_SWEEP_OP_NEW_LOCATION_ORDER: return (int64_t)(uintptr_t)EEUnitGoal::NewLocation(&thePoint, false, true, false, 0.0f, false, 0);
    case EE_UNITS_SWEEP_OP_SET_GOAL: Of<EEComplexUnit>(inA)->SetGoal(Of<EEUnitGoal>(inB), 0, 0); return 0;
    case EE_UNITS_SWEEP_OP_GROUP_NEW: return (int64_t)(uintptr_t)EEActiveGroup::New(Of<EEPlayer>(inA));
    case EE_UNITS_SWEEP_OP_ADD_MEMBER: Of<EEActiveGroup>(inA)->AddMember(Of<EEComplexUnit>(inB), true); return 0;
    case EE_UNITS_SWEEP_OP_FORMATION_CREATE: return (int64_t)(uintptr_t)EEFormation::Create((long)inA);
    case EE_UNITS_SWEEP_OP_SET_FORMATION: Of<EEActiveGroup>(inA)->SetFormation(Of<EEFormation>(inB)); return 0;
    case EE_UNITS_SWEEP_OP_FORMATION_DEREFERENCE: Of<EEFormation>(inA)->Dereference(); return 0;
    case EE_UNITS_SWEEP_OP_GROUP_SET_GOAL: Of<EEActiveGroup>(inA)->SetGoal(Of<EEUnitGoal>(inB), inC != 0); return 0;
    case EE_UNITS_SWEEP_OP_ADD_PENDING_GROUP: Of<EEActiveGroup>(inA)->GetPlayer()->AddPendingGroup(Of<EEActiveGroup>(inA)); return 0;
    case EE_UNITS_SWEEP_OP_PLAN: Of<EEActiveGroup>(inA)->Plan(); return 0;
    case EE_UNITS_SWEEP_OP_TOWN_PATROL: return (int64_t)(uintptr_t)Of<EETown>(inA)->mPatrolGroup;
    case EE_UNITS_SWEEP_OP_RANDOM: return gPhysics->GetRandom(0L, 0x3fffffffL);
    case EE_UNITS_SWEEP_OP_GOAL_LOCATION: Of<EEUnitGoal>(inA)->GetGoalLocation(thePoint); return 0;
    case EE_UNITS_SWEEP_OP_GROUP_MOVEMENT: static_cast<EEUGRust*>(Of<EEUnitGoal>(inA))->SetUseGroupMovement(inB != 0); return 0;
    case EE_UNITS_SWEEP_OP_PENDING_GROUPS: return (int64_t)Of<EEPlayer>(inA)->GetNumPendingGroups();
    case EE_UNITS_SWEEP_OP_TELL: Of<EEActiveGroup>(inA)->TellGroup(Of<EEComplexUnit>(inB), thePoint); return 0;
    // the goals (ag_sweep.rs EE_UNITS_SWEEP_GOAL_*): unit a, target b (a unit or a patrol group), the point
    case EE_UNITS_SWEEP_OP_GOAL_MOVE: return (int64_t)(uintptr_t)EEUGRust::NewLocation(&thePoint, true, true, ioPoint[3] != 0.0f, ioPoint[3], false, 0);
    case EE_UNITS_SWEEP_OP_GOAL_ATTACK_UNIT: return (int64_t)(uintptr_t)EEUGRust::NewAttackUnit(Of<EEComplexUnit>(inA), Of<EEUnit>(inB), true, false);
    case EE_UNITS_SWEEP_OP_GOAL_ATTACK_GROUND: return (int64_t)(uintptr_t)EEUGRust::NewAttackGround(thePoint, true);
    case EE_UNITS_SWEEP_OP_GOAL_GATHER: return (int64_t)(uintptr_t)EEUGRust::NewGather(Of<EEUnit>(inA), Of<EEUnit>(inB), true);
    case EE_UNITS_SWEEP_OP_GOAL_HEAL: return (int64_t)(uintptr_t)EEUGRust::NewHealUnit(Of<EEComplexUnit>(inA), true);
    case EE_UNITS_SWEEP_OP_GOAL_EXPLORE: return (int64_t)(uintptr_t)EEUGRust::NewExplore(true);
    case EE_UNITS_SWEEP_OP_GOAL_UNLOAD: return (int64_t)(uintptr_t)EEUGRust::NewUnloadTransport(Of<EEComplexUnit>(inA), (long)thePoint.mX, (long)thePoint.mY, true);
    case EE_UNITS_SWEEP_OP_GOAL_PATROL: return (int64_t)(uintptr_t)EEUGRust::NewPatrol(Of<EEPPatrolGroup>(inB), static_cast<EEComplexUnit*>(ee_units_pg_last(Of<EEPPatrolGroup>(inB))), true);
    default: return 0;
    }
}
