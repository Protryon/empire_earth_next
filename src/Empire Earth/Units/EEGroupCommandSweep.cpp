// New (port), TEST ONLY: the group command sweep (docs/RUST_PLAN.md R5b.4 leaf 4 (c)). No capture game sends a group
// command (EE-SIM's human idles; the computer players send only walls), so the Rust bodies of the group commands
// (rust/crates/ee_units/src/group.rs) are run here over the live world through the dry host (ee/EEUnitHost.h: every
// world call logged, nothing changed), every five game minutes, for every player and a set of selections and
// arguments; next to them the original bodies' statements, copied from EEMessage.cpp before the port with each world
// call made through the same host calls and every read of a unit made on the object (gWorld.GetUnit, the unit's
// class), run over the same dry host. The two logs must be equal (else EE-SIM stops, exit code 3); the USGC record
// holds the Rust body's log, so the capture's digest gates it like the others.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <map>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitClassIDs.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUnitGoal.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitHandle.h"
#include "ee/EEMap.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EETechTreeTechID.h"
#include "ee/EEUnitKind.h"
#include "ee/EESimCapture.h"
#include "ee/EEUnitHost.h"
#include "ee/EEUnitGoal.h"
#include "rust/ee_units.h"

namespace {

const uint32_t kNone = 0xffffffffu;

size_t Count(void* inGroup)
{
    return ee_units_host_group_ids(inGroup, NULL, 0);
}

uint32_t UnitAt(void* inGroup, long inIndex)
{
    std::vector<uint32_t> theIDs(Count(inGroup) + 1);
    size_t theCount = ee_units_host_group_ids(inGroup, &theIDs[0], theIDs.size());
    return inIndex >= 0 && (size_t)inIndex < theCount ? theIDs[inIndex] : kNone;
}

uint32_t IDOf(RTSUnit* inUnit)
{
    return inUnit != NULL ? static_cast<uint32_t>(inUnit->mUnitID) : kNone;
}

EEUnit* ObjectOf(uint32_t inID)
{
    EEUnit* theUnit;
    EEUnitHandleResolve(inID, theUnit);
    return theUnit;
}

// EEMAttackGround::Execute (0x4bb4be) as it was, each world call through the host
void OriginalAttackGround(EEPlayer* thePlayer, void* theGroup, float inX, float inY)
{
    GE3DPoint theTarget;
    uint32_t theUnit = Count(theGroup) > 0 ? UnitAt(theGroup, 0) : kNone;   // theGroup->GetFirstUnit()
    if (theUnit != kNone) {
        theTarget.SetX(inX);
        theTarget.SetY(inY);
        theTarget.SetZ(ee_units_host_map_height(inX, inY));   // gWorld.GetMap()->GetHeight(theTarget)
        if (theTarget.GetZ() < 0.0f)
            theTarget.SetZ(0.0f);
        for (unsigned long i = 1; i < 6; i++) {
            void* theActiveGroup = ee_units_host_active_group_new(thePlayer);
            ee_units_host_group_fill(theActiveGroup, theGroup, 0, (int32_t)i);
            if (Count(theActiveGroup) > 0) {
                ee_units_host_pending_push(thePlayer, theActiveGroup);
                ee_units_host_group_keep_ground_attackers(theActiveGroup);
                if (Count(theActiveGroup) > 0)
                    theUnit = UnitAt(theActiveGroup, ee_units_host_random(0, (int32_t)Count(theActiveGroup) - 1));
                else
                    theUnit = kNone;
                if (theUnit != kNone) {
                    ee_units_host_group_clear_goal(theActiveGroup);
                    EeGoalSpec theSpec;
                    memset(&theSpec, 0, sizeof(theSpec));
                    theSpec.ctor = EE_GOAL_CTOR_ATTACK_GROUND;   // EEUGRust::NewAttackGround(theTarget, true)
                    theSpec.unit_a = kNone;
                    theSpec.unit_b = kNone;
                    theSpec.point[0] = theTarget.GetX();
                    theSpec.point[1] = theTarget.GetY();
                    theSpec.point[2] = theTarget.GetZ();
                    theSpec.flag_a = 1;
                    ee_units_host_group_set_goal(theActiveGroup, ee_units_host_goal_new(&theSpec), 0);
                    ee_units_host_group_set_action(theActiveGroup, kActionAttackLocation);
                    ee_units_host_group_plan(theActiveGroup);
                    ee_units_host_send_group_command(theActiveGroup, kNone, thePlayer, 0xb9, 0);
                    ee_units_host_acknowledge_unit(thePlayer, theUnit);
                }
            } else
                ee_units_host_group_release(theActiveGroup);
        }
    }
}

// EEMPatrol::Execute (0x4c2cf2) as it was, each world call through the host, the units read on the objects
void OriginalPatrol(EEPlayer* thePlayer, void* theGroup, int32_t inTargetID)
{
    uint32_t theUnit = Count(theGroup) > 0 ? UnitAt(theGroup, 0) : kNone;   // theGroup->GetFirstUnit()
    unsigned long theTargetID = inTargetID;
    EEComplexUnit* theTarget = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(theTargetID));
    if (theUnit != kNone && inTargetID == -1) {
        // theUnit->FindNearestTown(), its patrol group's GetNearest
        uint8_t theFound = 0;
        uint32_t thePoint = ee_units_host_town_patrol_point(theUnit, &theFound);
        if (theFound)
            theTarget = thePoint != kNone ? static_cast<EEComplexUnit*>(ObjectOf(thePoint)) : NULL;
    }
    if (theTarget && theUnit != kNone) {
        if (ObjectOf(theUnit)->mUnitClass->mUnitType == kUnitTypePathPoint) {
            ee_units_host_patrol_add_unit_tile(theUnit, IDOf(theTarget), thePlayer);
        } else {
            void* thePatrolGroup = ee_units_host_unit_patrol_group(IDOf(theTarget));   // theTarget->mPatrolGroup
            for (unsigned long i = 1; i < 6; i++) {
                void* theActiveGroup = ee_units_host_active_group_new(thePlayer);
                ee_units_host_group_fill(theActiveGroup, theGroup, 0, (int32_t)i);
                if (Count(theActiveGroup) > 0) {
                    ee_units_host_pending_push(thePlayer, theActiveGroup);
                    ee_units_host_group_keep_mobile(theActiveGroup);
                    EeGoalSpec theSpec;   // new EEUGPatrol(thePatrolGroup, theTarget, true)
                    memset(&theSpec, 0, sizeof(theSpec));
                    theSpec.ctor = EE_GOAL_CTOR_PATROL_ROUTE;
                    theSpec.unit_a = IDOf(theTarget);
                    theSpec.unit_b = kNone;
                    theSpec.object = thePatrolGroup;
                    theSpec.flag_a = 1;
                    void* theGoal = ee_units_host_goal_new(&theSpec);
                    ee_units_host_group_clear_goal(theActiveGroup);
                    ee_units_host_group_set_goal(theActiveGroup, theGoal, 0);
                    ee_units_host_group_set_action(theActiveGroup, kActionMoveToNextPatrolPoint);
                    ee_units_host_group_plan(theActiveGroup);
                    ee_units_host_send_group_command(theActiveGroup, kNone, thePlayer, 0xba, 0);
                    if (Count(theActiveGroup) > 0)
                        ee_units_host_play_acknowledgement(thePlayer, theActiveGroup,
                                                           ee_units_host_random(0, (int32_t)Count(theActiveGroup) - 1));
                } else
                    ee_units_host_group_release(theActiveGroup);
            }
        }
    }
}

// EEMGatherResource::Execute (0x4bf0a3) as it was, each world call through the host, the target read on the object
void OriginalGather(EEPlayer* thePlayer, void* theGroup, int32_t inTargetID, bool inQueued)
{
    uint32_t theUnit = Count(theGroup) > 0 ? UnitAt(theGroup, 0) : kNone;   // theGroup->GetFirstUnit()
    EEUnit* theTarget = gWorld.GetUnit((unsigned long)inTargetID);
    if (theTarget && theUnit != kNone && theTarget->mResourceAmount > 0) {
        void* theActiveGroup = ee_units_host_active_group_new(thePlayer);
        ee_units_host_group_fill(theActiveGroup, theGroup, 0, 0);
        ee_units_host_pending_push(thePlayer, theActiveGroup);
        ee_units_host_group_set_queued(theActiveGroup, inQueued);   // theActiveGroup->mIsQueued = this->mIsQueued
        ee_units_host_group_spread_around_resource(theActiveGroup, IDOf(theTarget));
        if (Count(theActiveGroup)) {
            ee_units_host_group_clear_goal(theActiveGroup);
            EeGoalSpec theSpec;   // EEUGRust::NewGather(theTarget, NULL, true)
            memset(&theSpec, 0, sizeof(theSpec));
            theSpec.ctor = EE_GOAL_CTOR_GATHER;
            theSpec.unit_a = IDOf(theTarget);
            theSpec.unit_b = kNone;
            theSpec.flag_a = 1;
            ee_units_host_group_set_goal(theActiveGroup, ee_units_host_goal_new(&theSpec), inQueued);
            ee_units_host_group_set_action(theActiveGroup, kActionGatherResource);
            ee_units_host_group_plan(theActiveGroup);
            ee_units_host_send_group_command(theActiveGroup, IDOf(theTarget), thePlayer, 0xba, 0);
            if (Count(theActiveGroup) > 0)
                ee_units_host_play_acknowledgement(thePlayer, theActiveGroup,
                                                   ee_units_host_random(0, (int32_t)Count(theActiveGroup) - 1));
        }
    }
}

// EEMRepairUnit::Execute (0x4c3e31) as it was, each world call through the host, the target read on the object
void OriginalRepair(EEPlayer* thePlayer, void* theGroup, int32_t inTargetID, bool inQueued)
{
    std::vector<uint32_t> theUnits;
    std::vector<uint32_t> theRepairers;
    EEUnit* theTarget = gWorld.GetUnit((unsigned long)inTargetID);
    if (Count(theGroup) > 0 && theTarget) {   // theGroup->GetFirstUnit() && theTarget
        void* theActiveGroup = ee_units_host_active_group_new(thePlayer);
        ee_units_host_group_fill(theActiveGroup, theGroup, 0, 0);
        ee_units_host_pending_push(thePlayer, theActiveGroup);
        ee_units_host_group_spread_around_tile(theActiveGroup, (int64_t)theTarget->mTileX.Get(), (int64_t)theTarget->mTileY.Get());
        if (Count(theActiveGroup) > 0) {
            unsigned long i;
            for (i = 0; i < Count(theActiveGroup); i++)
                theUnits.push_back(UnitAt(theActiveGroup, (long)i));
            for (std::vector<uint32_t>::iterator it = theUnits.begin(); it != theUnits.end();) {
                if (inQueued) {
                    ee_units_host_unit_add_build_target(*it, IDOf(theTarget));
                    ++it;
                } else {
                    ee_units_host_unit_order_repair(*it, IDOf(theTarget));
                    theRepairers.push_back(*it);
                    it = theUnits.erase(it);
                }
            }
            for (i = 0; i < theRepairers.size(); i++)
                ee_units_host_group_add_member(theActiveGroup, theRepairers[i]);
            if (Count(theActiveGroup) > 0)
                ee_units_host_play_acknowledgement(thePlayer, theActiveGroup,
                                                   ee_units_host_random(0, (int32_t)Count(theActiveGroup) - 1));
        }
    }
}

// EEMAddPatrolPoint::Execute (0x4bae4b) as it was, each world call through the host, the first unit read on the object
void OriginalAddPatrolPoint(EEPlayer* thePlayer, void* theGroup, float inX, float inY)
{
    uint32_t theUnit = Count(theGroup) > 0 ? UnitAt(theGroup, 0) : kNone;   // theGroup->GetFirstUnit()
    if (theUnit != kNone) {
        if (ObjectOf(theUnit)->mUnitClass->mUnitType == kUnitTypePathPoint)
            ee_units_host_patrol_insert_point_at(theUnit, inX, inY, thePlayer);   // ->mPatrolGroup->InsertPatrolPointAt
        else {
            void* theRoute = ee_units_host_unit_goal_patrol_route(theUnit);   // EEGoalCast<EEUGPatrol>(GetGoal())
            if (theRoute)
                ee_units_host_patrol_add_point(theRoute, inX, inY, thePlayer);   // theGoal->mPatrolGroup->AddPoint
            else {
                for (unsigned long i = 1; i < 6; i++) {
                    void* theActiveGroup = ee_units_host_active_group_new(thePlayer);
                    ee_units_host_group_fill(theActiveGroup, theGroup, 0, (int32_t)i);
                    if (Count(theActiveGroup) > 0) {
                        ee_units_host_pending_push(thePlayer, theActiveGroup);
                        ee_units_host_group_keep_mobile(theActiveGroup);
                        ee_units_host_group_keep_non_citizens(theActiveGroup);
                        EeGoalSpec theSpec;   // new EEUGPatrol(true)
                        memset(&theSpec, 0, sizeof(theSpec));
                        theSpec.ctor = EE_GOAL_CTOR_PATROL;
                        theSpec.unit_a = kNone;
                        theSpec.unit_b = kNone;
                        theSpec.flag_a = 1;
                        void* theNewGoal = ee_units_host_goal_new(&theSpec);
                        // theNewGoal->mPatrolGroup->AddPoint(this->mPoint, ...), thePlayer->AddPatrolGroup(...)
                        ee_units_host_goal_patrol_add_point(theNewGoal, inX, inY, thePlayer);
                        ee_units_host_group_clear_goal(theActiveGroup);
                        ee_units_host_group_set_goal(theActiveGroup, theNewGoal, 0);
                        ee_units_host_group_set_action(theActiveGroup, kActionMoveToNextPatrolPoint);
                        ee_units_host_group_plan(theActiveGroup);
                        if (Count(theActiveGroup) > 0)
                            ee_units_host_play_acknowledgement(thePlayer, theActiveGroup,
                                                               ee_units_host_random(0, (int32_t)Count(theActiveGroup) - 1));
                    } else
                        ee_units_host_group_release(theActiveGroup);
                }
            }
        }
    } else
        ee_units_host_patrol_new_lone(thePlayer, inX, inY);   // new EEPPatrolGroup, AddPoint, AddPatrolGroup
}

// EEMAttackUnit::Execute (0x4bb869) as it was, each world call through the host, the target read on the object
void OriginalAttackUnit(EEPlayer* thePlayer, void* theGroup, int32_t inTargetID, bool inQueued, bool inConvert)
{
    EEUnit* theTarget = gWorld.GetUnit((unsigned long)inTargetID);
    uint32_t theUnit = Count(theGroup) > 0 ? UnitAt(theGroup, 0) : kNone;   // theGroup->GetFirstUnit()
    if (theUnit != kNone && theTarget && theTarget->mHitPoints > 0) {
        for (unsigned long i = 1; i < 6; i++) {
            void* theActiveGroup = ee_units_host_active_group_new(thePlayer);
            ee_units_host_group_fill(theActiveGroup, theGroup, 1, (int32_t)i);
            ee_units_host_group_set_queued(theActiveGroup, inQueued);
            if (Count(theActiveGroup) > 0) {
                ee_units_host_pending_push(thePlayer, theActiveGroup);
                ee_units_host_group_spread_around_unit(theActiveGroup, IDOf(theTarget), (int64_t)theTarget->mTileX.Get(),
                                                       (int64_t)theTarget->mTileY.Get());
                if (Count(theActiveGroup) > 0)
                    theUnit = UnitAt(theActiveGroup, ee_units_host_random(0, (int32_t)Count(theActiveGroup) - 1));
                else
                    theUnit = kNone;
                if (theUnit != kNone) {
                    ee_units_host_group_clear_goal(theActiveGroup);
                    EeGoalSpec theSpec;   // EEUGRust::NewAttackUnit(theGroup->GetFirstUnit(), theTarget, 1, this->mIsConvert)
                    memset(&theSpec, 0, sizeof(theSpec));
                    theSpec.ctor = EE_GOAL_CTOR_ATTACK_UNIT;
                    theSpec.unit_a = Count(theGroup) > 0 ? UnitAt(theGroup, 0) : kNone;
                    theSpec.unit_b = IDOf(theTarget);
                    theSpec.flag_a = 1;
                    theSpec.flag_b = inConvert ? 1 : 0;
                    void* theGoal = ee_units_host_goal_new(&theSpec);
                    if (theGoal != NULL)
                        ee_units_host_group_set_goal(theActiveGroup, theGoal, inQueued);
                    ee_units_host_group_set_action(theActiveGroup, kActionAttackEnemyUnit);
                    ee_units_host_group_plan(theActiveGroup);
                    ee_units_host_send_group_command(theActiveGroup, IDOf(theTarget), thePlayer, 0xb9, 0);
                    ee_units_host_acknowledge_unit(thePlayer, theUnit);
                }
            } else
                ee_units_host_group_release(theActiveGroup);
        }
    }
}

// EEMHealUnit::Execute (0x4bf9df) as it was, each world call through the host, the target read on the object
void OriginalHealUnit(EEPlayer* thePlayer, void* theGroup, int32_t inTargetID)
{
    EEUnit* theTarget = gWorld.GetUnit((unsigned long)inTargetID);
    if (theTarget) {
        uint32_t theComplexUnit = ee_units_host_unit_complex_id(IDOf(theTarget));   // theTarget->GetComplexUnit()
        if (theComplexUnit == kNone)
            return;   // (the port refuses it: the original's Plan dereferenced the NULL target)
        for (unsigned long i = 1; i < 6; i++) {
            void* theActiveGroup = ee_units_host_active_group_new(thePlayer);
            ee_units_host_group_fill(theActiveGroup, theGroup, 1, (int32_t)i);
            if (Count(theActiveGroup) > 0) {
                ee_units_host_pending_push(thePlayer, theActiveGroup);
                ee_units_host_group_clear_goal(theActiveGroup);
                EeGoalSpec theSpec;   // EEUGRust::NewHealUnit(theComplexUnit, true)
                memset(&theSpec, 0, sizeof(theSpec));
                theSpec.ctor = EE_GOAL_CTOR_HEAL_UNIT;
                theSpec.unit_a = theComplexUnit;
                theSpec.unit_b = kNone;
                theSpec.flag_a = 1;
                void* theGoal = ee_units_host_goal_new(&theSpec);
                if (theGoal != NULL)
                    ee_units_host_group_set_goal(theActiveGroup, theGoal, 0);
                ee_units_host_group_set_action(theActiveGroup, kActionHealUnit);
                ee_units_host_group_plan(theActiveGroup);
                if (Count(theActiveGroup) > 0)
                    ee_units_host_play_acknowledgement(thePlayer, theActiveGroup,
                                                       ee_units_host_random(0, (int32_t)Count(theActiveGroup) - 1));
            } else
                ee_units_host_group_release(theActiveGroup);
        }
    }
}

// EEMPathTo::Execute (0x4c281c) as it was, each world call through the host (the point's adjustment before the loop
// is one host call: the same statements moved, ee_units_host_path_to_point)
void OriginalPathTo(EEPlayer* thePlayer, void* theSelection, float inX, float inY, float inFacing, unsigned long inFlags)
{
    GE3DPoint theLocation;
    float theX = inX, theY = inY;   // this->mX, this->mY
    uint32_t theFirstUnit = Count(theSelection) > 0 ? UnitAt(theSelection, 0) : kNone;   // GetFirstUnit()
    if (theFirstUnit == kNone)
        return;
    float theZ = ee_units_host_path_to_point(theFirstUnit, &theX, &theY);
    theLocation.SetX(theX);
    theLocation.SetY(theY);
    theLocation.SetZ(theZ);
    for (unsigned long i = 1; i < 6; i++) {
        void* theGroup = ee_units_host_active_group_new(thePlayer);
        ee_units_host_group_fill(theGroup, theSelection, 0, (int32_t)i);
        if (Count(theGroup) > 0) {
            ee_units_host_pending_push(thePlayer, theGroup);
            ee_units_host_group_keep_mobile(theGroup);
            EeGoalSpec theSpec;   // EEUGRust::NewLocation(&theLocation, true, forced, rotating, facing, false, 0)
            memset(&theSpec, 0, sizeof(theSpec));
            theSpec.ctor = EE_GOAL_CTOR_LOCATION;
            theSpec.unit_a = kNone;
            theSpec.unit_b = kNone;
            theSpec.point[0] = theLocation.GetX();
            theSpec.point[1] = theLocation.GetY();
            theSpec.point[2] = theLocation.GetZ();
            theSpec.flag_a = (inFlags & 1) ? 1 : 0;
            theSpec.flag_b = (inFlags & 2) ? 1 : 0;
            theSpec.value = inFacing;
            theSpec.flag_c = (inFlags & 8) ? 1 : 0;   // theGoal->mUseGroupMovement
            void* theGoal = ee_units_host_goal_new(&theSpec);
            ee_units_host_group_reset_paths(theGroup);
            ee_units_host_group_clear_goal(theGroup);
            ee_units_host_group_set_goal(theGroup, theGoal, (inFlags & 4) ? 1 : 0);
            ee_units_host_group_set_action(theGroup, kActionPrepareToMove);
            ee_units_host_group_plan(theGroup);
            if (Count(theGroup) > 0)
                ee_units_host_play_acknowledgement(thePlayer, theGroup, ee_units_host_random(0, (int32_t)Count(theGroup) - 1));
            ee_units_host_send_group_command(theGroup, kNone, thePlayer, 0xb8, 0);
        } else
            ee_units_host_group_release(theGroup);
    }
}

// EEMDropoffResource::Execute (0x4bd93c) as it was, each world call through the host, the drop-off read on the object
void OriginalDropoffResource(EEPlayer* thePlayer, void* theGroup, int32_t inDropoffID)
{
    uint32_t theUnit = Count(theGroup) > 0 ? UnitAt(theGroup, 0) : kNone;   // theGroup->GetFirstUnit()
    EEComplexUnit* theDropoff = EEUnitCast<EEComplexUnit>(gWorld.GetUnit((unsigned long)inDropoffID));
    if (theUnit != kNone && theDropoff) {
        void* theActiveGroup = ee_units_host_active_group_new(thePlayer);
        ee_units_host_group_fill(theActiveGroup, theGroup, 0, 0);
        ee_units_host_pending_push(thePlayer, theActiveGroup);
        ee_units_host_group_spread_around_tile(theActiveGroup, (int64_t)theDropoff->mTileX.Get(), (int64_t)theDropoff->mTileY.Get());
        if (Count(theActiveGroup) > 0) {
            ee_units_host_group_clear_goal(theActiveGroup);
            EeGoalSpec theSpec;   // EEUGRust::NewGather(NULL, theDropoff, true)
            memset(&theSpec, 0, sizeof(theSpec));
            theSpec.ctor = EE_GOAL_CTOR_GATHER;
            theSpec.unit_a = kNone;
            theSpec.unit_b = IDOf(theDropoff);
            theSpec.flag_a = 1;
            ee_units_host_group_set_goal(theActiveGroup, ee_units_host_goal_new(&theSpec), 0);
            ee_units_host_group_set_action(theActiveGroup, kActionDropoffResource);
            ee_units_host_group_plan(theActiveGroup);
            if (Count(theActiveGroup) > 0)
                ee_units_host_play_acknowledgement(thePlayer, theActiveGroup,
                                                   ee_units_host_random(0, (int32_t)Count(theActiveGroup) - 1));
        }
    }
}

// EEMBuildWall::Execute (0x4bc12c) as it was, each world call through the host (the plan and the build are host calls)
void OriginalBuildWall(EEPlayer* thePlayer, void* theGroup, int32_t inTechnology, float inStartX, float inStartY,
                       float inEndX, float inEndY)
{
    void* thePieces = NULL;   // std::vector<EEUnit*> thePieces
    uintptr_t theCount = 0;
    if (Count(theGroup) > 0 && UnitAt(theGroup, 0) != kNone) {   // theGroup->GetFirstUnit()
        void* theActiveGroup = ee_units_host_active_group_new(thePlayer);
        ee_units_host_group_fill(theActiveGroup, theGroup, 0, 0);
        ee_units_host_pending_push(thePlayer, theActiveGroup);
        ee_units_host_group_keep_citizens(theActiveGroup);
        if (Count(theActiveGroup) > 0) {   // theActiveGroup->GetFirstUnit()
            thePieces = ee_units_host_wall_plan(thePlayer, inTechnology, inStartX, inStartY, inEndX, inEndY, &theCount);
            if (Count(theActiveGroup) > 0)
                ee_units_host_play_acknowledgement(thePlayer, theActiveGroup,
                                                   ee_units_host_random(0, (int32_t)Count(theActiveGroup) - 1));
        }
        if (theCount > 0)
            ee_units_host_wall_build(theActiveGroup, thePieces);
        if (thePieces != NULL)   // (the vector's end of life)
            ee_units_host_wall_release(thePieces);
    }
}

// EEMUnloadTransport::Execute (0x4c8615) as it was, each world call through the host (the goal's SetTransport and
// CanUnloadAtDestination, the class and cargo reads and the event are host calls)
void OriginalUnloadTransport(EEPlayer* thePlayer, void* theGroup, long inX, long inY)
{
    bool theCanUnload = false;
    if (Count(theGroup) == 0 || UnitAt(theGroup, 0) == kNone)   // theGroup->GetFirstUnit() == NULL
        return;
    for (unsigned long i = 1; i < 6; i++) {
        void* theActiveGroup = ee_units_host_active_group_new(thePlayer);
        ee_units_host_group_fill(theActiveGroup, theGroup, 1, (int32_t)i);
        if (Count(theActiveGroup) > 0) {
            ee_units_host_pending_push(thePlayer, theActiveGroup);
            ee_units_host_group_spread_for_unload(theActiveGroup, inX, inY);
            uint32_t theUnit = Count(theActiveGroup) > 0 ? UnitAt(theActiveGroup, 0) : kNone;   // GetFirstUnit()
            if (theUnit == kNone)
                continue;
            EeGoalSpec theSpec;   // EEUGRust::NewUnloadTransport(theUnit->GetComplexUnit(), x, y, false)
            memset(&theSpec, 0, sizeof(theSpec));
            theSpec.ctor = EE_GOAL_CTOR_UNLOAD_TRANSPORT;
            theSpec.unit_a = theUnit;
            theSpec.unit_b = kNone;
            theSpec.point[0] = (float)inX;
            theSpec.point[1] = (float)inY;
            void* theGoal = ee_units_host_goal_new(&theSpec);
            unsigned long theCount = Count(theActiveGroup);
            theCanUnload = false;
            for (unsigned long j = 0; j < theCount && !theCanUnload; j++) {
                theUnit = UnitAt(theActiveGroup, (long)j);
                theCanUnload = ee_units_host_unit_can_unload(theUnit) != 0;
            }
            if (theCanUnload) {
                ee_units_host_goal_set_transport(theGoal, theUnit);
                if (theCanUnload && ee_units_host_goal_can_unload(theGoal)) {
                    ee_units_host_group_clear_goal(theActiveGroup);
                    ee_units_host_group_set_goal(theActiveGroup, theGoal, 0);
                    ee_units_host_group_set_action(theActiveGroup, kActionUnloadTransport);
                    ee_units_host_group_plan(theActiveGroup);
                    ee_units_host_acknowledge_unit(thePlayer, theUnit);
                    continue;
                }
            }
            ee_units_host_goal_release(theGoal);   // delete theGoal
            if (ee_units_host_unit_can_unload(theUnit))
                ee_units_host_unload_warning(thePlayer, inX, inY);
            ee_units_host_group_clear_goal(theActiveGroup);
            EeGoalSpec theMove;   // EEUGRust::NewLocation((float)x, (float)y, 0.0f, false, true, false, 0.0f, false, 0)
            memset(&theMove, 0, sizeof(theMove));
            theMove.ctor = EE_GOAL_CTOR_LOCATION_XY;
            theMove.unit_a = kNone;
            theMove.unit_b = kNone;
            theMove.point[0] = (float)inX;
            theMove.point[1] = (float)inY;
            ee_units_host_group_set_goal(theActiveGroup, ee_units_host_goal_new(&theMove), 0);
            ee_units_host_group_set_action(theActiveGroup, kActionPrepareToMove);
            ee_units_host_group_plan(theActiveGroup);
            ee_units_host_send_group_command(theActiveGroup, kNone, thePlayer, 0xb8, 0);
            ee_units_host_acknowledge_unit(thePlayer, theUnit);
        } else
            ee_units_host_group_release(theActiveGroup);
    }
}

// a two-call host list (the count, then the ids)
void ReadFarms(uint32_t inUnit, std::vector<uint32_t>& outIDs)
{
    outIDs.resize(ee_units_host_free_farms(inUnit, NULL, 0));
    if (!outIDs.empty())
        ee_units_host_free_farms(inUnit, &outIDs[0], outIDs.size());
}

bool ReadDropSites(uint32_t inFarm, std::vector<uint32_t>& outIDs)
{
    uint8_t theFound = 0;
    outIDs.resize(ee_units_host_farm_drop_sites(inFarm, NULL, 0, &theFound));
    if (!outIDs.empty())
        ee_units_host_farm_drop_sites(inFarm, &outIDs[0], outIDs.size(), &theFound);
    return theFound != 0;
}

// EEMBuildUnit::Execute (0x4bbcc4) as it was, each world call through the host (the class read, the capitol count,
// CanProduce, EECanPlaceOnTile, EEMakeBuilding, the farm search and sort, the orders), the units read on the objects
void OriginalBuildUnit(EEPlayer* thePlayer, void* theGroup, int32_t inTechnology, long inX, long inY, bool inQueued)
{
    std::vector<uint32_t> theNearUnits;
    std::vector<uint32_t> theMovedUnits;
    if (Count(theGroup) > 0 && UnitAt(theGroup, 0) != kNone) {   // theGroup->GetFirstUnit()
        void* theActiveGroup = ee_units_host_active_group_new(thePlayer);
        ee_units_host_group_fill(theActiveGroup, theGroup, 0, 0);
        ee_units_host_pending_push(thePlayer, theActiveGroup);
        ee_units_host_group_keep_citizens(theActiveGroup);
        if (Count(theActiveGroup) > 0) {   // theActiveGroup->GetFirstUnit()
            uint8_t theFound = 0;
            long theUnitType = (long)ee_units_host_technology_unit_class(thePlayer, inTechnology, &theFound);
            if (!theFound)   // (the original read through the missing node; the Rust body refuses it)
                return;
            if (theUnitType != kUnitTypeCapitol || ee_units_host_player_num_capitols(thePlayer) == 0) {
                if (ee_units_host_can_produce(thePlayer, inTechnology)) {
                    if (ee_units_host_can_place_on_tile(thePlayer, theUnitType, inX, inY)) {
                        uint32_t theBuilding = ee_units_host_make_building(thePlayer, theUnitType, inX, inY);
                        bool theGranary = theUnitType == kUnitTypeGranary || theUnitType == kUnitTypeGranaryImproved;
                        if (theGranary) {
                            // do { EEFindFreeFarm(theBuilding->GetComplexUnit(), 1, &theCount) } while (a farm)
                            ReadFarms(ee_units_host_unit_complex_id(theBuilding), theNearUnits);
                            if (theNearUnits.size() > 0) {
                                EEUnit* theFirst = ObjectOf(UnitAt(theActiveGroup, 0));
                                ee_units_host_sort_farms(&theNearUnits[0], theNearUnits.size(), theFirst->mTileX.Get(),
                                                         theFirst->mTileY.Get(), 0);
                            }
                        }
                        unsigned long i = 0;
                        unsigned long j = 0;
                        for (; i < Count(theActiveGroup); i++, j++) {
                            unsigned long theNumUnits = Count(theActiveGroup);
                            uint32_t theWorker = UnitAt(theActiveGroup, (long)i);
                            if (inQueued) {
                                ee_units_host_unit_add_build_target(theWorker, theBuilding);
                                EEComplexUnit* theObject = static_cast<EEComplexUnit*>(ObjectOf(theWorker));
                                if (theObject->mGoalState.Get() && theObject->mGoalState->mKind != kGoalBuildRepair)
                                    ee_units_host_unit_start_action(theWorker, kActionIdle);
                            } else
                                ee_units_host_unit_order_build(theWorker, theBuilding);
                            if (theGranary) {
                                if (j < theNearUnits.size())
                                    ee_units_host_unit_add_farm_target(theWorker, theNearUnits[j]);
                                else if (theNearUnits.size() > 0)
                                    ee_units_host_unit_add_farm_target(
                                        theWorker, theNearUnits[ee_units_host_random(0, (int32_t)theNearUnits.size() - 1)]);
                            }
                            if (Count(theActiveGroup) != theNumUnits) {
                                i--;
                                theMovedUnits.push_back(theWorker);
                            }
                        }
                        for (i = 0; i < theMovedUnits.size(); i++)
                            ee_units_host_group_add_member(theActiveGroup, theMovedUnits[i]);
                        if (Count(theActiveGroup) > 0)
                            ee_units_host_play_acknowledgement(thePlayer, theActiveGroup,
                                                               ee_units_host_random(0, (int32_t)Count(theActiveGroup) - 1));
                    }
                }
            }
        }
    }
}

// EEMSmartFarmers::Execute (0x4c7739) as it was, each world call through the host (the farm search, the load goal's
// farm, the drop sites, CanTake, the sort, the orders), the units read on the objects
void OriginalSmartFarmers(EEPlayer* thePlayer, void* theSelection, int32_t inTargetID)
{
    std::vector<uint32_t> theFarms;
    std::vector<uint32_t> theOccupants;
    uint32_t theFirstUnit = Count(theSelection) > 0 ? UnitAt(theSelection, 0) : kNone;   // GetFirstUnit()
    long theTargetID = inTargetID;
    EEComplexUnit* theTarget = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(theTargetID));
    if (theFirstUnit == kNone || theTarget == NULL)
        return;
    void* theGroup = ee_units_host_active_group_new(thePlayer);
    ee_units_host_group_fill(theGroup, theSelection, 0, 0);
    ee_units_host_group_keep_citizens(theGroup);
    long theCount = (long)Count(theGroup);
    if (theCount > 0) {
        uint32_t theFarm;
        ReadFarms(IDOf(theTarget), theFarms);   // do { EEFindFreeFarm(theTarget, 1, &theIndex) } while (a farm)
        for (long i = 0; i < theCount; i++) {
            uint32_t theUnit = UnitAt(theGroup, i);
            theFarm = ee_units_host_unit_load_farm(theUnit);   // the load goal's farm
            if (theFarm != kNone) {
                theOccupants.clear();
                if (ReadDropSites(theFarm, theOccupants)) {
                    if (std::find(theOccupants.begin(), theOccupants.end(), IDOf(theTarget)) != theOccupants.end()
                        && ee_units_host_can_take(IDOf(theTarget), theUnit)) {
                        ee_units_host_unit_drop_gathered(theUnit);
                        ee_units_host_unit_start_action(theUnit, kActionMoveToStorage);
                        continue;
                    }
                } else
                    theFarm = kNone;
            }
            if (theFarm == kNone && theFarms.size() > 0) {
                EEUnit* theObject = ObjectOf(theUnit);
                ee_units_host_sort_farms(&theFarms[0], theFarms.size(), theObject->mTileX.Get(), theObject->mTileY.Get(), 1);
                theFarm = theFarms.back();
                theFarms.pop_back();
            }
            if (theFarm != kNone) {
                ee_units_host_work_farm(theFarm, theUnit);
                continue;
            }
            EEUnit* theObject = ObjectOf(theUnit);
            EeGoalSpec theSpec;
            memset(&theSpec, 0, sizeof(theSpec));
            theSpec.unit_a = kNone;
            theSpec.unit_b = kNone;
            long theAction;
            if (theObject->mResourceAmount > 0 && ee_units_host_can_take(IDOf(theTarget), theUnit)) {
                theSpec.ctor = EE_GOAL_CTOR_GATHER;   // EEUGRust::NewGather(NULL, theTarget, true)
                theSpec.unit_b = IDOf(theTarget);
                theSpec.flag_a = 1;
                theAction = kActionMoveToStorage;
            } else {
                theSpec.ctor = EE_GOAL_CTOR_LOCATION_2D;   // EEUGRust::NewLocation(GE2DPoint(the target's tile), true, true, ..)
                theSpec.point[0] = (float)theTarget->GetTileX();
                theSpec.point[1] = (float)theTarget->GetTileY();
                theAction = kActionPrepareToMove;
            }
            void* theGoal = ee_units_host_goal_new(&theSpec);
            if (theGoal != NULL)
                ee_units_host_unit_set_goal(theUnit, theGoal);
            ee_units_host_unit_start_action(theUnit, (int32_t)theAction);
        }
        if (Count(theGroup) > 0)
            ee_units_host_play_acknowledgement(thePlayer, theGroup, ee_units_host_random(0, (int32_t)Count(theGroup) - 1));
    }
    ee_units_host_pending_push(thePlayer, theGroup);
    ee_units_host_group_finish(theGroup);
}

// a cast goal's spec (EEUGRust::NewCastCalamity(inUnit, inTarget, x, y, the technology, true))
void* CastGoal(uint32_t inUnit, uint32_t inTarget, long inX, long inY, long inTechnology)
{
    EeGoalSpec theSpec;
    memset(&theSpec, 0, sizeof(theSpec));
    theSpec.ctor = EE_GOAL_CTOR_CAST_CALAMITY;
    theSpec.unit_a = inUnit;
    theSpec.unit_b = inTarget;
    theSpec.ints[0] = inX;
    theSpec.ints[1] = inY;
    theSpec.ints[2] = inTechnology;
    theSpec.flag_a = 1;
    return ee_units_host_goal_new(&theSpec);
}

// EEMCastCalamity::Execute (0x4bc483) as it was, each world call through the host (the node and its class, CanCastAt,
// Contains, the power and the cost, the caster's player, the split, the goals, the orders), the units read on the
// objects
void OriginalCastCalamity(EEPlayer* thePlayer, void* theSelection, int32_t inCasterID, int32_t inTargetID, int32_t inX,
                          int32_t inY, int32_t inTechnology)
{
    unsigned long i;
    EEUnit* theTarget = NULL;
    bool theCast = false;
    EEComplexUnit* theCaster = EEUnitCast<EEComplexUnit>(gWorld.GetUnit((unsigned long)inCasterID));
    if (theCaster == NULL || theCaster->mHitPoints <= 0)
        return;
    int32_t theClassEnum = 0;
    uint8_t theCastByAll = 0;
    void* theClass = ee_units_host_calamity_of(thePlayer, inTechnology, &theClassEnum, &theCastByAll);
    if (theClass == NULL)   // (no node: the original returned; no class: it read through NULL, the Rust body refuses)
        return;
    if (inTargetID != -1)
        theTarget = gWorld.GetUnit((unsigned long)inTargetID);
    uint32_t theTargetID = IDOf(theTarget);
    if (theClassEnum == 0x12) {
        if (ee_units_host_calamity_can_cast_at(theClass, IDOf(theCaster), inX, inY, theTargetID) != 0)
            return;
        void* theGroup = ee_units_host_active_group_new(thePlayer);
        ee_units_host_group_fill(theGroup, theSelection, 0, 0);
        ee_units_host_group_split_calamity_casters(theGroup, theClass, thePlayer, inTechnology, theTargetID);
        if (Count(theGroup) > 0) {
            std::vector<uint32_t> theUnits;
            void* theGoal = CastGoal(IDOf(theCaster), theTargetID, inX, inY, inTechnology);
            for (i = 0; i < Count(theGroup); i++)
                theUnits.push_back(UnitAt(theGroup, (long)i));
            for (std::vector<uint32_t>::iterator theIterator = theUnits.begin(); theIterator != theUnits.end(); theIterator++) {
                uint32_t theUnit = IDOf(ObjectOf(*theIterator)->GetComplexUnit());   // (*theIterator)->mComplexUnit
                ee_units_host_group_remove_unit(theGroup, theUnit);
                ee_units_host_unit_clear_goals(theUnit);
                void* theCopy = theGoal != NULL ? ee_units_host_goal_clone(theGoal) : NULL;
                if (theCopy != NULL)
                    ee_units_host_unit_set_goal(theUnit, theCopy);
                ee_units_host_unit_start_action(theUnit, kActionPrepareToMove);
            }
            if (theGoal != NULL)
                ee_units_host_goal_release(theGoal);   // delete theGoal
            ee_units_host_group_release(theGroup);
        }
        return;
    }
    void* theGroup = ee_units_host_active_group_new(ee_units_host_unit_player(IDOf(theCaster)));
    if (inTechnology != kTechTeleport && !theCastByAll) {
        unsigned long theCount = Count(theSelection);
        for (i = 0; i < theCount && !theCast; i++) {
            uint32_t theUnit = UnitAt(theSelection, (long)i);
            EEComplexUnit* theObject = static_cast<EEComplexUnit*>(ObjectOf(theUnit));
            long theResult = (long)ee_units_host_calamity_can_cast_at(theClass, theUnit, inX, inY, theTargetID);
            int64_t thePower;
            if (theObject->mHitPoints > 0 && theResult == 0
                && ee_units_host_technology_contains(thePlayer, inTechnology, theObject->mUnitClass->mUnitType)
                && (thePower = ee_units_host_unit_power(theUnit), thePower >= ee_units_host_calamity_cost(theClass))) {
                void* theGoal = CastGoal(theUnit, theTargetID, inX, inY, inTechnology);
                if (theGoal != NULL)
                    ee_units_host_unit_set_goal(theUnit, theGoal);
                if (theObject->mUnitClass->mUnitType == kUnitTypeCapitol || inTechnology == kTechConquistadorsSpell)
                    ee_units_host_unit_start_action(theUnit, kActionCalamityPreCast);
                else
                    ee_units_host_unit_start_action(theUnit, kActionPrepareToMove);
                ee_units_host_group_add_member(theGroup, theUnit);
                theCast = true;
            }
        }
    } else {
        for (i = 0; i < Count(theSelection); i++) {
            uint32_t theUnit = UnitAt(theSelection, (long)i);
            EEComplexUnit* theObject = static_cast<EEComplexUnit*>(ObjectOf(theUnit));
            if (theObject->mHitPoints > 0
                && ee_units_host_technology_contains(thePlayer, inTechnology, theObject->mUnitClass->mUnitType)
                && ee_units_host_calamity_can_cast_at(theClass, theUnit, inX, inY, theTargetID) == 0) {
                void* theGoal = CastGoal(theUnit, theTargetID, inX, inY, inTechnology);
                if (theGoal != NULL)
                    ee_units_host_unit_set_goal(theUnit, theGoal);
                ee_units_host_unit_start_action(theUnit, kActionPrepareToMove);
                ee_units_host_group_add_member(theGroup, theUnit);
            }
        }
    }
    if (theGroup != NULL)
        ee_units_host_pending_push(thePlayer, theGroup);
}

enum SweepBody {
    kSweepAttackGround = 1,
    kSweepPatrol = 2,
    kSweepGather = 3,
    kSweepRepair = 4,
    kSweepAddPatrolPoint = 5,
    kSweepAttackUnit = 6,
    kSweepHealUnit = 7,
    kSweepPathTo = 8,
    kSweepDropoffResource = 9,
    kSweepBuildWall = 10,
    kSweepUnloadTransport = 11,
    kSweepBuildUnit = 12,
    kSweepSmartFarmers = 13,
    kSweepCastCalamity = 14
};

struct SweepCase {
    unsigned long mBody;
    unsigned long mSelection;   // which selection (the record's)
    float mX, mY;               // Attack Ground's and Add Patrol Point's point
    int32_t mTarget;            // Patrol's, Gather's and Repair's target id
    bool mQueued;               // Gather's, Repair's and Attack Unit's
    bool mConvert;              // Attack Unit's
    float mFacing;              // Path To's
    unsigned long mFlags;       // Path To's: 1 forced, 2 rotating, 4 queued, 8 group movement
    float mEndX, mEndY;         // Build Wall's end (mX, mY its start; mTarget its technology)
    int32_t mTechnology;        // Build Unit's and Cast Calamity's (mX, mY their tile; mTarget Cast Calamity's target)
    int32_t mCaster;            // Cast Calamity's caster id
};

void RunBody(const SweepCase& inCase, EEPlayer* inPlayer, void* inSelection, bool inRust)
{
    switch (inCase.mBody) {
    case kSweepAttackGround:
        if (inRust)
            ee_units_attack_ground(inPlayer, inSelection, inCase.mX, inCase.mY);
        else
            OriginalAttackGround(inPlayer, inSelection, inCase.mX, inCase.mY);
        break;
    case kSweepPatrol:
        if (inRust)
            ee_units_patrol(inPlayer, inSelection, inCase.mTarget);
        else
            OriginalPatrol(inPlayer, inSelection, inCase.mTarget);
        break;
    case kSweepGather:
        if (inRust)
            ee_units_gather(inPlayer, inSelection, (uint32_t)inCase.mTarget, inCase.mQueued);
        else
            OriginalGather(inPlayer, inSelection, inCase.mTarget, inCase.mQueued);
        break;
    case kSweepAddPatrolPoint:
        if (inRust)
            ee_units_add_patrol_point(inPlayer, inSelection, inCase.mX, inCase.mY);
        else
            OriginalAddPatrolPoint(inPlayer, inSelection, inCase.mX, inCase.mY);
        break;
    case kSweepAttackUnit:
        if (inRust)
            ee_units_attack_unit(inPlayer, inSelection, (uint32_t)inCase.mTarget, inCase.mQueued, inCase.mConvert);
        else
            OriginalAttackUnit(inPlayer, inSelection, inCase.mTarget, inCase.mQueued, inCase.mConvert);
        break;
    case kSweepHealUnit:
        if (inRust)
            ee_units_heal_unit(inPlayer, inSelection, (uint32_t)inCase.mTarget);
        else
            OriginalHealUnit(inPlayer, inSelection, inCase.mTarget);
        break;
    case kSweepPathTo: {
        float theX = inCase.mX, theY = inCase.mY;
        if (inRust)
            ee_units_path_to(inPlayer, inSelection, &theX, &theY, inCase.mFacing, (inCase.mFlags & 1) != 0,
                             (inCase.mFlags & 2) != 0, (inCase.mFlags & 4) != 0, (inCase.mFlags & 8) != 0);
        else
            OriginalPathTo(inPlayer, inSelection, inCase.mX, inCase.mY, inCase.mFacing, inCase.mFlags);
        break;
    }
    case kSweepBuildWall:
        if (inRust)
            ee_units_build_wall(inPlayer, inSelection, inCase.mTarget, inCase.mX, inCase.mY, inCase.mEndX, inCase.mEndY);
        else
            OriginalBuildWall(inPlayer, inSelection, inCase.mTarget, inCase.mX, inCase.mY, inCase.mEndX, inCase.mEndY);
        break;
    case kSweepUnloadTransport:
        if (inRust)
            ee_units_unload_transport(inPlayer, inSelection, (int64_t)inCase.mX, (int64_t)inCase.mY);
        else
            OriginalUnloadTransport(inPlayer, inSelection, (long)inCase.mX, (long)inCase.mY);
        break;
    case kSweepBuildUnit:
        if (inRust)
            ee_units_build_unit(inPlayer, inSelection, inCase.mTechnology, (int64_t)inCase.mX, (int64_t)inCase.mY,
                                inCase.mQueued);
        else
            OriginalBuildUnit(inPlayer, inSelection, inCase.mTechnology, (long)inCase.mX, (long)inCase.mY, inCase.mQueued);
        break;
    case kSweepSmartFarmers:
        if (inRust)
            ee_units_smart_farmers(inPlayer, inSelection, (uint32_t)inCase.mTarget);
        else
            OriginalSmartFarmers(inPlayer, inSelection, inCase.mTarget);
        break;
    case kSweepCastCalamity:
        if (inRust)
            ee_units_cast_calamity(inPlayer, inSelection, (uint32_t)inCase.mCaster, inCase.mTarget, (int32_t)inCase.mX,
                                   (int32_t)inCase.mY, inCase.mTechnology);
        else
            OriginalCastCalamity(inPlayer, inSelection, inCase.mCaster, inCase.mTarget, (int32_t)inCase.mX, (int32_t)inCase.mY,
                                 inCase.mTechnology);
        break;
    case kSweepDropoffResource:
        if (inRust)
            ee_units_dropoff_resource(inPlayer, inSelection, (uint32_t)inCase.mTarget);
        else
            OriginalDropoffResource(inPlayer, inSelection, inCase.mTarget);
        break;
    case kSweepRepair:
        if (inRust)
            ee_units_repair(inPlayer, inSelection, (uint32_t)inCase.mTarget, inCase.mQueued);
        else
            OriginalRepair(inPlayer, inSelection, inCase.mTarget, inCase.mQueued);
        break;
    }
}

void RunCase(const SweepCase& inCase, unsigned long inPlayerIndex, EEPlayer* inPlayer, const std::vector<EEComplexUnit*>& inUnits)
{
    std::vector<uint32_t> theLogs[2];
    unsigned long theMismatches[2];
    for (int k = 0; k < 2; k++) {
        void* theSelection = EEUnitHostDryBegin(inUnits);
        RunBody(inCase, inPlayer, theSelection, k == 0);
        EEUnitHostDryEnd(theLogs[k], theMismatches[k]);
    }
    EESimCaptureRecord theRecord(0x43475355u);   // "USGC"
    theRecord.U32((unsigned int)inCase.mBody);
    theRecord.I32((int)inPlayerIndex);
    theRecord.U32((unsigned int)inCase.mSelection);
    theRecord.F32(inCase.mX);
    theRecord.F32(inCase.mY);
    theRecord.I32(inCase.mTarget);
    if (inCase.mBody == kSweepGather || inCase.mBody == kSweepRepair)
        theRecord.U32(inCase.mQueued ? 1u : 0u);
    if (inCase.mBody == kSweepAttackUnit)
        theRecord.U32((inCase.mQueued ? 1u : 0u) | (inCase.mConvert ? 2u : 0u));
    if (inCase.mBody == kSweepPathTo) {
        theRecord.U32((unsigned int)inCase.mFlags);
        theRecord.F32(inCase.mFacing);
    }
    if (inCase.mBody == kSweepBuildWall) {
        theRecord.F32(inCase.mEndX);
        theRecord.F32(inCase.mEndY);
    }
    if (inCase.mBody == kSweepBuildUnit)
        theRecord.U32((unsigned int)inCase.mTechnology | (inCase.mQueued ? 0x80000000u : 0u));
    if (inCase.mBody == kSweepCastCalamity) {
        theRecord.I32(inCase.mTechnology);
        theRecord.I32(inCase.mCaster);
    }
    theRecord.U32((unsigned int)inUnits.size());
    for (size_t i = 0; i < inUnits.size(); i++)
        theRecord.U32(IDOf(inUnits[i]));
    theRecord.U32((unsigned int)theLogs[0].size());
    if (!theLogs[0].empty())
        theRecord.Bytes(&theLogs[0][0], (unsigned int)(theLogs[0].size() * 4));
    theRecord.U32((unsigned int)theMismatches[0]);
    theRecord.Commit();
    if (theLogs[0] != theLogs[1] || theMismatches[0] != 0 || theMismatches[1] != 0) {
        size_t theAt = 0;
        while (theAt < theLogs[0].size() && theAt < theLogs[1].size() && theLogs[0][theAt] == theLogs[1][theAt])
            theAt++;
        fprintf(stderr, "EE-GROUP-COMMAND: body %lu player %lu selection %lu target %d: the Rust body's log (%u words) "
                "differs from the original's (%u words) at word %u; keep mismatches %lu/%lu\n", inCase.mBody,
                inPlayerIndex, inCase.mSelection, inCase.mTarget, (unsigned)theLogs[0].size(), (unsigned)theLogs[1].size(),
                (unsigned)theAt, theMismatches[0], theMismatches[1]);
        EESimCaptureRecord::Flush();
        exit(3);
    }
}

// the player's technologies whose node makes inUnitType (the first one), or -1
int32_t FindTechnology(EEPlayer* inPlayer, long inUnitType)
{
    EETechTree* theTree = inPlayer->mTechnologies;
    for (long e = 0; e < theTree->mEpochs.size(); e++) {
        std::vector<long> theIDs;
        theTree->mEpochs[e].GetNodeIDs(theIDs);
        for (size_t i = 0; i < theIDs.size(); i++) {
            EETechTreeNode* theNode = theTree->Get(theIDs[i]);
            if (theNode != NULL && theNode->mData != NULL && theNode->mData->mUnitClass == inUnitType)
                return (int32_t)theIDs[i];
        }
    }
    return -1;
}

// the player's calamity technologies: the first of each calamity class kind (class 0x12, cast by all the selected,
// any other) and Teleport when the tree has it
void FindCalamityTechnologies(EEPlayer* inPlayer, std::vector<int32_t>& outIDs)
{
    EETechTree* theTree = inPlayer->mTechnologies;
    int32_t theGroupCast = -1, theByAll = -1, theOther = -1;
    for (long e = 0; e < theTree->mEpochs.size(); e++) {
        std::vector<long> theIDs;
        theTree->mEpochs[e].GetNodeIDs(theIDs);
        for (size_t i = 0; i < theIDs.size(); i++) {
            EETechTreeNode* theNode = theTree->Get(theIDs[i]);
            if (theNode == NULL || theNode->mData == NULL || theNode->mData->mCalamity == 0 || theIDs[i] == kTechTeleport)
                continue;
            EECalamityClass* theClass =
                EECalamityManager::GetInstance()->GetCalamityClass((EECalamityEnum)theNode->mData->mCalamity);
            if (theClass == NULL)
                continue;
            int32_t& theSlot = theClass->mClassEnum == 0x12 ? theGroupCast : theClass->mIsCastByAllSelected ? theByAll : theOther;
            if (theSlot == -1)
                theSlot = (int32_t)theIDs[i];
        }
    }
    int32_t theFound[3] = {theGroupCast, theByAll, theOther};
    for (int k = 0; k < 3; k++)
        if (theFound[k] != -1)
            outIDs.push_back(theFound[k]);
    if (theTree->Get(kTechTeleport) != NULL)
        outIDs.push_back(kTechTeleport);
}

} // namespace

void EEGroupCommandSweep()
{
    if (!EESimCaptureRecord::Active())
        return;
    // the towns' hull reads (FindNearestTown) are the sweep's, not the game's: inside a scope, the HULL/AREA records
    // skip them (lle/U2DPolygon.h)
    EEGeomCaptureScope theScope;
    unsigned long theNumPlayers = gWorld.GetNumPlayers();
    // the first unit of any list with resources left (a mine, a tree, a herd animal)
    RTSUnit* theResource = NULL;
    for (unsigned long p = 0; p < theNumPlayers && theResource == NULL; p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        if (thePlayer == NULL)
            continue;
        for (RTSUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL && theResource == NULL; theUnit = theUnit->mNext)
            if (theUnit->mObjectKind.mValue >= EE_OBJECT_UNIT && static_cast<EEUnit*>(theUnit)->mResourceAmount > 0)
                theResource = theUnit;
    }
    for (unsigned long p = 0; p < theNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        if (thePlayer == NULL)
            continue;
        std::vector<EEComplexUnit*> theOwn;   // its list's complex units (a selection holds only those)
        EEComplexUnit* thePathPoint = NULL;
        for (RTSUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext) {
            if (theUnit->mObjectKind.mValue != EE_OBJECT_COMPLEX_UNIT)
                continue;
            EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(theUnit);
            theOwn.push_back(theComplex);
            if (thePathPoint == NULL && theComplex->mUnitClass.Get() != NULL && theComplex->mUnitClass->mUnitType == kUnitTypePathPoint)
                thePathPoint = theComplex;
        }
        std::vector<EEComplexUnit*> theReversed(theOwn.rbegin(), theOwn.rend());
        std::vector<EEComplexUnit*> theNone;
        std::vector<EEComplexUnit*> theFromPoint;
        if (thePathPoint != NULL) {
            theFromPoint.push_back(thePathPoint);
            theFromPoint.insert(theFromPoint.end(), theOwn.begin(), theOwn.end());
        }
        const std::vector<EEComplexUnit*>* theSelections[4] = {&theOwn, &theReversed, &theNone, &theFromPoint};
        // the targets: -1 (the nearest town's route), its first unit, the next player's first complex unit, its path
        // point, an id no unit has
        std::vector<int32_t> theTargets;
        theTargets.push_back(-1);
        if (!theOwn.empty())
            theTargets.push_back((int32_t)IDOf(theOwn[0]));
        EEPlayer* theNext = gWorld.GetPlayer((p + 1) % theNumPlayers);
        if (theNext != NULL)
            for (RTSUnit* theUnit = theNext->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext)
                if (theUnit->mObjectKind.mValue == EE_OBJECT_COMPLEX_UNIT) {
                    theTargets.push_back((int32_t)IDOf(theUnit));
                    break;
                }
        if (thePathPoint != NULL)
            theTargets.push_back((int32_t)IDOf(thePathPoint));
        theTargets.push_back(0x7e000001);
        std::vector<int32_t> theGatherTargets;
        if (theResource != NULL)
            theGatherTargets.push_back((int32_t)IDOf(theResource));
        if (!theOwn.empty())
            theGatherTargets.push_back((int32_t)IDOf(theOwn[0]));
        theGatherTargets.push_back(0x7e000001);
        for (unsigned long s = 0; s < 4; s++) {
            const std::vector<EEComplexUnit*>& theUnits = *theSelections[s];
            if (s == 3 && theUnits.empty())
                continue;
            SweepCase theCase;
            theCase.mSelection = s;
            theCase.mTarget = 0;
            theCase.mQueued = false;
            theCase.mConvert = false;
            theCase.mFacing = 0.0f;
            theCase.mFlags = 0;
            theCase.mEndX = 0.0f;
            theCase.mEndY = 0.0f;
            theCase.mTechnology = 0;
            theCase.mCaster = 0;
            // Attack Ground at the first unit's tile (else, or off the map: a unit inside another, the map's centre)
            // and at the map's centre (a point off the map's diamond has no terrain row: the UI never sends one)
            theCase.mBody = kSweepAttackGround;
            bool theOnMap = !theUnits.empty() && theUnits[0]->mTileX.Get() >= 0 && theUnits[0]->mTileY.Get() >= 0
                && gWorld.GetMap()->GetTile((unsigned long)theUnits[0]->mTileX.Get(), (unsigned long)theUnits[0]->mTileY.Get()) != NULL;
            float theCenter = (float)(gWorld.GetMap()->GetSize() / 2) + 0.5f;
            theCase.mX = theOnMap ? (float)theUnits[0]->mTileX.Get() + 0.5f : theCenter;
            theCase.mY = theOnMap ? (float)theUnits[0]->mTileY.Get() + 0.5f : theCenter;
            RunCase(theCase, p, thePlayer, theUnits);
            float theFirstX = theCase.mX, theFirstY = theCase.mY;
            theCase.mX = theCenter;
            theCase.mY = theCenter;
            RunCase(theCase, p, thePlayer, theUnits);
            // Add Patrol Point at the same two points (the selection with a path point first takes the point into its
            // route; the empty one makes a lone patrol)
            theCase.mBody = kSweepAddPatrolPoint;
            RunCase(theCase, p, thePlayer, theUnits);
            theCase.mX = theFirstX;
            theCase.mY = theFirstY;
            RunCase(theCase, p, thePlayer, theUnits);
            // Path To at the same two points, with no flag and with all four (facing 0.75)
            theCase.mBody = kSweepPathTo;
            for (int f = 0; f < 2; f++) {
                theCase.mFlags = f ? 0xfu : 0u;
                theCase.mFacing = f ? 0.75f : 0.0f;
                theCase.mX = theFirstX;
                theCase.mY = theFirstY;
                RunCase(theCase, p, thePlayer, theUnits);
                theCase.mX = theCenter;
                theCase.mY = theCenter;
                RunCase(theCase, p, thePlayer, theUnits);
            }
            theCase.mFlags = 0;
            theCase.mFacing = 0.0f;
            theCase.mBody = kSweepPatrol;
            theCase.mX = 0.0f;
            theCase.mY = 0.0f;
            for (size_t t = 0; t < theTargets.size(); t++) {
                theCase.mTarget = theTargets[t];
                RunCase(theCase, p, thePlayer, theUnits);
            }
            // Gather from the world's first resource with something left, from its first unit and from no unit;
            // Repair its first unit, the next player's and no unit; each plain and queued
            for (int q = 0; q < 2; q++) {
                theCase.mQueued = q != 0;
                theCase.mBody = kSweepGather;
                for (size_t t = 0; t < theGatherTargets.size(); t++) {
                    theCase.mTarget = theGatherTargets[t];
                    RunCase(theCase, p, thePlayer, theUnits);
                }
                theCase.mBody = kSweepRepair;
                for (size_t t = 1; t < theTargets.size(); t++) {
                    if (thePathPoint != NULL && theTargets[t] == (int32_t)IDOf(thePathPoint))
                        continue;
                    theCase.mTarget = theTargets[t];
                    RunCase(theCase, p, thePlayer, theUnits);
                }
                // Attack Unit: the next player's first unit, its own first unit, the resource, no unit; plain and
                // queued, the first target also as a conversion
                theCase.mBody = kSweepAttackUnit;
                std::vector<int32_t> theAttackTargets(theTargets.begin() + 1, theTargets.end());
                if (theResource != NULL)
                    theAttackTargets.push_back((int32_t)IDOf(theResource));
                for (size_t t = 0; t < theAttackTargets.size(); t++) {
                    if (thePathPoint != NULL && theAttackTargets[t] == (int32_t)IDOf(thePathPoint))
                        continue;
                    theCase.mTarget = theAttackTargets[t];
                    for (int c = 0; c < (t == 0 ? 2 : 1); c++) {
                        theCase.mConvert = c != 0;
                        RunCase(theCase, p, thePlayer, theUnits);
                    }
                }
                theCase.mConvert = false;
            }
            // Heal Unit: its own first unit, the next player's, no unit
            theCase.mBody = kSweepHealUnit;
            theCase.mQueued = false;
            for (size_t t = 1; t < theTargets.size(); t++) {
                if (thePathPoint != NULL && theTargets[t] == (int32_t)IDOf(thePathPoint))
                    continue;
                theCase.mTarget = theTargets[t];
                RunCase(theCase, p, thePlayer, theUnits);
            }
            // Dropoff Resource at its own first unit, the next player's, its path point and no unit
            theCase.mBody = kSweepDropoffResource;
            for (size_t t = 1; t < theTargets.size(); t++) {
                theCase.mTarget = theTargets[t];
                RunCase(theCase, p, thePlayer, theUnits);
            }
            // Build Wall from the first unit's tile (or the map's centre) eight tiles east, and a one-tile wall at the
            // centre (the dry host plans nothing: the technology is only logged)
            theCase.mBody = kSweepBuildWall;
            theCase.mTarget = 0x2a;
            theCase.mX = theFirstX;
            theCase.mY = theFirstY;
            theCase.mEndX = theFirstX + 8.0f;
            theCase.mEndY = theFirstY;
            RunCase(theCase, p, thePlayer, theUnits);
            theCase.mX = theCenter;
            theCase.mY = theCenter;
            theCase.mEndX = theCenter;
            theCase.mEndY = theCenter;
            RunCase(theCase, p, thePlayer, theUnits);
            theCase.mEndX = 0.0f;
            theCase.mEndY = 0.0f;
            // Unload Transport at the first unit's tile (or the map's centre) and at the centre
            theCase.mBody = kSweepUnloadTransport;
            theCase.mTarget = 0;
            theCase.mX = (float)(long)theFirstX;
            theCase.mY = (float)(long)theFirstY;
            RunCase(theCase, p, thePlayer, theUnits);
            theCase.mX = (float)(long)theCenter;
            theCase.mY = (float)(long)theCenter;
            RunCase(theCase, p, thePlayer, theUnits);
            // Build Unit: a granary, a house and a capitol (the technologies the player's tree makes them with) and a
            // technology it has no node of, at the first unit's tile and the centre, plain and queued (the dry host
            // lets every production and placement through)
            theCase.mBody = kSweepBuildUnit;
            int32_t theBuildTechs[4] = {FindTechnology(thePlayer, kUnitTypeGranary), FindTechnology(thePlayer, kUnitTypeHouse),
                                        FindTechnology(thePlayer, kUnitTypeCapitol), 0x7e00};
            for (int b = 0; b < 4; b++) {
                if (theBuildTechs[b] == -1)
                    continue;
                theCase.mTechnology = theBuildTechs[b];
                for (int q = 0; q < 2; q++) {
                    theCase.mQueued = q != 0;
                    theCase.mX = (float)(long)theFirstX;
                    theCase.mY = (float)(long)theFirstY;
                    RunCase(theCase, p, thePlayer, theUnits);
                    if (b == 0) {
                        theCase.mX = (float)(long)theCenter;
                        theCase.mY = (float)(long)theCenter;
                        RunCase(theCase, p, thePlayer, theUnits);
                    }
                }
            }
            theCase.mQueued = false;
            // Smart Farmers at its own first unit, the next player's, its path point, no unit and its first granary
            theCase.mBody = kSweepSmartFarmers;
            theCase.mX = 0.0f;
            theCase.mY = 0.0f;
            std::vector<int32_t> theFarmTargets(theTargets.begin() + 1, theTargets.end());
            for (RTSUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext) {
                if (theUnit->mObjectKind.mValue != EE_OBJECT_COMPLEX_UNIT)
                    continue;
                EEUnitClass* theClass = static_cast<EEComplexUnit*>(theUnit)->mUnitClass.Get();
                if (theClass != NULL && (theClass->mUnitType == kUnitTypeGranary || theClass->mUnitType == kUnitTypeGranaryImproved)) {
                    theFarmTargets.push_back((int32_t)IDOf(theUnit));
                    break;
                }
            }
            for (size_t t = 0; t < theFarmTargets.size(); t++) {
                theCase.mTarget = theFarmTargets[t];
                RunCase(theCase, p, thePlayer, theUnits);
            }
            // Cast Calamity: casters its first unit and its first unit with power; targets none and the next player's
            // first unit; the calamity technologies of its tree (one of each class kind, Teleport); at the first unit's
            // tile (the dry host answers every CanCastAt "can")
            theCase.mBody = kSweepCastCalamity;
            std::vector<int32_t> theCalamities;
            FindCalamityTechnologies(thePlayer, theCalamities);
            std::vector<int32_t> theCasters;
            if (!theOwn.empty())
                theCasters.push_back((int32_t)IDOf(theOwn[0]));
            for (size_t k = 0; k < theOwn.size(); k++)
                if (theOwn[k]->GetPower() > 0) {
                    theCasters.push_back((int32_t)IDOf(theOwn[k]));
                    break;
                }
            int32_t theCastTargets[2] = {-1, theTargets.size() > 2 ? theTargets[2] : -1};
            theCase.mX = (float)(long)theFirstX;
            theCase.mY = (float)(long)theFirstY;
            for (size_t c = 0; c < theCasters.size(); c++)
                for (int t = 0; t < 2; t++)
                    for (size_t k = 0; k < theCalamities.size(); k++) {
                        theCase.mCaster = theCasters[c];
                        theCase.mTarget = theCastTargets[t];
                        theCase.mTechnology = theCalamities[k];
                        RunCase(theCase, p, thePlayer, theUnits);
                    }
            theCase.mCaster = 0;
            theCase.mTechnology = 0;
        }
    }
}
