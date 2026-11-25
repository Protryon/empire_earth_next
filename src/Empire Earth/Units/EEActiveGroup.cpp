// C:\EEX\Empire Earth\Units\EEActiveGroup.cpp (file name inferred: no __FILE__). The group the unit commands act on:
// 0x1ebc47..0x1f1f19. port (the groups lane, round 1): the group is a Rust object (ee_units group_obj.rs; its bodies
// ag.rs); this unit keeps the active group bodies' host (ee_units_ag_host: the units' and goals' C++ calls), the
// unit's link to its group and the speeds a unit reads through its group.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <set>
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEMap.h"
#include "ee/EEPhysics.h"
#include "lle/NESynchronousClient.h"
#include "ee/EEClient.h"
#include "ee/EECalamityClass.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEFormation.h"
#include "ee/EEUnitClassKinds.h"   // EEObj
#include "ee/EECombatGoals.h"

// Moves the unit to another active group, passing its reference from the old group to the new one (group_obj.rs)
void EEComplexUnit::SetActiveGroup(EEActiveGroup* inGroup)
{
    ee_units_group_do(inGroup, 22, this);
}

// Returns the unit's top speed: its active group's value when it is in one, else its stats'
// (was EECPEngineHelpers.cpp's, out of line after its first user there)
float EEComplexUnit::GetMaxVelocity()
{
    return this->mActiveGroup ? this->mActiveGroup->GetMaxVelocity(this) : this->mStats->mMaxVelocity;
}

// A unit's stat, or the group's when the group moves together (its mean speed, acceleration, turn speed, turn
// acceleration and their inverses)
float EEActiveGroup::GetMaxVelocity(EEComplexUnit* inUnit)
{
    return this->mFields.mMovesTogether ? this->mFields.mSpeed : inUnit->mStats->GetMaxVelocity();
}

float EEActiveGroup::GetAcceleration(EEComplexUnit* inUnit)
{
    return this->mFields.mMovesTogether ? this->mFields.mAcceleration : inUnit->mStats->GetAcceleration();
}

float EEActiveGroup::GetMaxAngularVelocity(EEComplexUnit* inUnit)
{
    return this->mFields.mMovesTogether ? this->mFields.mTurnSpeed : inUnit->mStats->GetMaxAngularVelocity();
}

float EEActiveGroup::GetAngularAcceleration(EEComplexUnit* inUnit)
{
    return this->mFields.mMovesTogether ? this->mFields.mTurnAcceleration : inUnit->mStats->mAngularAcceleration;
}

float EEActiveGroup::GetInverseMaxVelocity(EEComplexUnit* inUnit)
{
    return this->mFields.mMovesTogether ? this->mFields.mInverseSpeed : inUnit->mStats->mInverseMaxVelocity;
}

float EEActiveGroup::GetInverseAcceleration(EEComplexUnit* inUnit)
{
    return this->mFields.mMovesTogether ? this->mFields.mInverseAcceleration : inUnit->mStats->mInverseAcceleration;
}

float EEActiveGroup::GetInverseAngularAcceleration(EEComplexUnit* inUnit)
{
    return this->mFields.mMovesTogether ? this->mFields.mInverseTurnAcceleration : inUnit->mStats->mInverseAngularAcceleration;
}

// The active group bodies' host (ee_units ag_host.rs; EE_UNITS_AG_*): the group's and its units' reads, the goals'
// constructors and accessors, and every statement with a world effect, each as the original made it
extern "C" int64_t ee_units_ag_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, EeUnitsAgIo* io)
{
    EEActiveGroup* theGroup = (EEActiveGroup*)(uintptr_t)inA;
    EEComplexUnit* theUnit = (EEComplexUnit*)(uintptr_t)inA;
    EEUnitGoal* theOther = (EEUnitGoal*)(uintptr_t)inB;
    switch (inOp) {
    // (the goals' kinds, the gather and patrol goals' units, a unit's sphere and the regions' size: ag_host.rs)
    case EE_UNITS_AG_SEND: EESendGroupCommand(theGroup, (EEUnit*)(uintptr_t)inB, theGroup->GetPlayer()->GetPlayerID(), (long)inC, 0);
        return 0;
    case EE_UNITS_AG_FORMATION_CENTER: theGroup->GetFormation()->mCenter = *reinterpret_cast<GE3DPoint*>(io->p);
        return 0;
    case EE_UNITS_AG_FORMATION_HEADING: theGroup->GetFormation()->mHeading = io->f;
        return 0;
    case EE_UNITS_AG_FORMATION_ORIENT: theGroup->GetFormation()->mOrientation.SetOrientationYPR(io->f, 0.0f, 0.0f);
        return 0;
    case EE_UNITS_AG_ARRANGE: {
        std::vector<EEFormationSlot> theSlots;
        theGroup->GetFormation()->Arrange(theGroup, theSlots);
        for (size_t i = 0; i < theSlots.size(); i++)
            ee_units_ag_push((EeUnitsAgListHandle*)io->list, EEObj(theSlots[i].mUnit),
                             (const float(*)[3])&theSlots[i].mPosition.mX, theSlots[i].mHeading);
        return 0;
    }
    case EE_UNITS_AG_CAP_FACTOR: io->f = gWorldTable.GetFloatValue(kWorldGroupSpeedCapFactor);
        return 0;
    // (EE_UNITS_AG_UNIT's reads, the group's record and members, the units' formation places and special attack,
    // the command type, the cargo count, the map's tiles and their units are Rust's in place: ag_host.rs)
    case EE_UNITS_AG_QUEUED_GOAL:
        return EEObj(theUnit->GetQueuedGoal((unsigned long)inC));
    case EE_UNITS_AG_CLEAR_QUEUE: theUnit->ClearGoalQueue((unsigned long)inC);
        return 0;
    case EE_UNITS_AG_CLEAR_GOALS: theUnit->ClearGoals((long)inC);
        return 0;
    case EE_UNITS_AG_QUEUE_GOAL: theUnit->QueueGoal(theOther, (long)inC);
        return 0;
    case EE_UNITS_AG_SET_GOAL: theUnit->SetGoal(theOther, 0, (long)inC);
        return 0;
    case EE_UNITS_AG_ADD_TIME: theUnit->mGoalQueues[1].AddTime(1);
        return 0;
    case EE_UNITS_AG_CLEAR_GATHERED: theUnit->SetGatheredResource(NULL);
        return 0;
    case EE_UNITS_AG_CAN_TAKE:
        return theUnit->CanTake((EEComplexUnit*)(uintptr_t)inB, (unsigned long)inC);
    case EE_UNITS_AG_CAN_GET_TO:
        return gContinentManager.CanGetTo(theUnit, (long)inC, (long)inD);
    case EE_UNITS_AG_CAN_CONVERT:
        return EECanConvert(theUnit, (EEComplexUnit*)(uintptr_t)inB, *static_cast<bool*>(io->list));
    case EE_UNITS_AG_CAN_ATTACK_CLASS:
        return theUnit->CanAttackClass((EEUnitClass*)(uintptr_t)inB);
    case EE_UNITS_AG_HAS_ACTION:
        return ((EEUnitClass*)(uintptr_t)inA)->FindAction((long)inC) != NULL;
    case EE_UNITS_AG_PASSABLE:
        return ((EEUnit*)(uintptr_t)inA)->IsPassableBy((EEComplexUnit*)(uintptr_t)inB);
    case EE_UNITS_AG_SEEN_BY:
        return EECombatIsSeenBy((EEUnit*)(uintptr_t)inA, (EEPlayer*)(uintptr_t)inB, true);
    case EE_UNITS_AG_PRIEST_CONVERT:
        return EECanPriestConvert(theUnit, (EEComplexUnit*)(uintptr_t)inB);
    // the compound statements' parts (ag_host.rs)
    case EE_UNITS_AG_QUEUE_ADD_ONLY: theUnit->mGoalQueues[(size_t)inC].Add(theOther);
        return 0;
    case EE_UNITS_AG_REFRESH_GOALS: theUnit->RefreshGoals();
        return 0;
    case EE_UNITS_AG_REFRESH_ACTIVE_GOAL: theUnit->RefreshActiveGoal();
        return 0;
    case EE_UNITS_AG_START_ACTION: theUnit->StartAction((long)inC);
        return 0;
    case EE_UNITS_AG_ACTION_CHANGED: theUnit->GetUnitClass()->ActionChanged(theUnit);
        return 0;
    case EE_UNITS_AG_QUEUE_SQUADRON: theUnit->QueueSquadronGoals(theOther, inC != 0);
        return 0;
    case EE_UNITS_AG_SET_SQUADRON: theUnit->SetSquadronGoals(theOther, inC != 0);
        return 0;
    case EE_UNITS_AG_POWER: return theUnit->GetPower();
    case EE_UNITS_AG_CALAMITY_COST: return ((EECalamityClass*)(uintptr_t)inA)->GetCost();
    case EE_UNITS_AG_NODE_HAS_TYPE: return ((EETechTreeNode*)(uintptr_t)inA)->Contains(((EEComplexUnit*)(uintptr_t)inB)->mUnitClass->mUnitType);
    case EE_UNITS_AG_RANDOM: return gPhysics->GetRandom((long)inC, (long)inD);
    case EE_UNITS_AG_TILE_PRESENT: return ::gWorld.mMap->GetTile(UMFloorUnsigned(io->p[0]), UMFloorUnsigned(io->p[1])) != NULL;
    case EE_UNITS_AG_TILE_HEIGHT: io->f = ::gWorld.mMap->GetTile(UMFloorUnsigned(io->p[0]), UMFloorUnsigned(io->p[1]))->GetHeight();
        return 0;
    case EE_UNITS_AG_TILE_TERRAIN: return ::gWorld.mMap->GetTile(UMFloorUnsigned(io->p[0]), UMFloorUnsigned(io->p[1]))->mTerrainType;

    // (the footprint tile, the rectangle's closest point, the goal's location and the new location goal: ag_host.rs)
    }
    return 0;
}
