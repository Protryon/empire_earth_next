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
#include "ee/EEUnitKind.h"
#include "ee/EESimCapture.h"
#include "ee/EEUnitHost.h"
#include "rust/ee_rust.h"

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
                    theSpec.ctor = EE_GOAL_CTOR_ATTACK_GROUND;   // new EEUGAttackGround(theTarget, true)
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
            EeGoalSpec theSpec;   // new EEUGGather(theTarget, NULL, true)
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
                    EeGoalSpec theSpec;   // new EEUGAttackUnit(theGroup->GetFirstUnit(), theTarget, 1, this->mIsConvert)
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
                EeGoalSpec theSpec;   // new EEUGHealUnit(theComplexUnit, true)
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
            EeGoalSpec theSpec;   // new EEUGLocation(&theLocation, true, forced, rotating, facing, false, 0)
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
            EeGoalSpec theSpec;   // new EEUGGather(NULL, theDropoff, true)
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

enum SweepBody {
    kSweepAttackGround = 1,
    kSweepPatrol = 2,
    kSweepGather = 3,
    kSweepRepair = 4,
    kSweepAddPatrolPoint = 5,
    kSweepAttackUnit = 6,
    kSweepHealUnit = 7,
    kSweepPathTo = 8,
    kSweepDropoffResource = 9
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
        }
    }
}
