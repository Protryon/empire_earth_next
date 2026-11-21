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
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DPoint.h"

#include "ee/EEUnitKind.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPASGFlags.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEMap.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEPathManager.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEUnitStore.h"
#include "ee/EEPhysics.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousClient.h"
#include "ee/EEClient.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUGRust.h"
#include "ee/EEUGRust.h"
#include "ee/EEPPatrolGroup.h"
#include "lle/U2DSparseArray.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUnitVectorItem.h"
#include "lle/U2DRectangle.h"
#include "ee/EEFormation.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUnitClassKinds.h"   // EEObj
#include "ee/EECombatGoals.h"
#include "rust/sim/combat.h"   // ee_units_keeps_ground_attacker, ee_units_ag_*
#include "rust/sim/units.h"
#include "rust/sim/combat.h"   // ee_combat_regions_size
#include "rust/sim/units.h"

// Moves the unit to another active group, passing its reference from the old group to the new one (group_obj.rs)
void EEComplexUnit::SetActiveGroup(EEActiveGroup* inGroup)
{
    ee_units_group_do(inGroup, 22, this);
}

// Returns the unit's top speed: its active group's value when it is in one, else its stats'
// (was EECPEngineHelpers.cpp's, out of line after its first user there)
float EEComplexUnit::GetMaxVelocity()
{
    if (this->mActiveGroup)
        return this->mActiveGroup->GetMaxVelocity(this);
    return this->mStats->mMaxVelocity;
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
    EEUGRust* theGoal = (EEUGRust*)(uintptr_t)inA;
    EEUnitGoal* theOther = (EEUnitGoal*)(uintptr_t)inB;
    switch (inOp) {
    // (the goals' kinds, the gather and patrol goals' units, a unit's sphere and the regions' size: ag_host.rs)
    case EE_UNITS_AG_SEND: EESendGroupCommand(theGroup, (EEUnit*)(uintptr_t)inB, theGroup->GetPlayer()->GetPlayerID(), (long)inC, 0);
        return 0;
    case EE_UNITS_AG_ARRANGE: {
        EEFormation* theFormation = theGroup->GetFormation();
        std::vector<EEFormationSlot> theSlots;
        theFormation->mCenter = GE3DPoint(io->p[0], io->p[1], io->p[2]);
        if (inC) {
            theFormation->mHeading = io->f;
            theFormation->mOrientation.SetOrientationYPR(io->f, 0.0f, 0.0f);
        }
        theFormation->Arrange(theGroup, theSlots);
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
    case EE_UNITS_AG_FOOTPRINT: {
        long theX = (long)io->a, theY = (long)io->b;
        theUnit->GetNearestFootprintTile((EEUnit*)(uintptr_t)inB, &theX, &theY);
        io->a = theX;
        io->b = theY;
        return 0;
    }
    case EE_UNITS_AG_CAN_GET_TO:
        return gContinentManager.CanGetTo(theUnit, (long)inC, (long)inD);
    case EE_UNITS_AG_CAN_CONVERT: {
        bool theFlag = io->a != 0;
        bool theResult = EECanConvert(theUnit, (EEComplexUnit*)(uintptr_t)inB, theFlag);
        io->a = theFlag;
        return theResult;
    }
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
    case EE_UNITS_AG_RECT_CLOSEST: {
        U2DRectangle<float> theBounds;
        theBounds.mTop = io->p[0];
        theBounds.mLeft = io->p[1];
        theBounds.mBottom = io->p[2];
        theBounds.mRight = io->q[0];
        U2DPoint<float> theClosest = theBounds.GetClosestPoint(U2DPoint<float>(io->q[1], io->q[2]));
        io->p[0] = theClosest.mX;
        io->p[1] = theClosest.mY;
        return 0;
    }
    case EE_UNITS_AG_GOAL_LOCATION: {
        GE3DPoint theLocation;
        theGoal->GetGoalLocation(theLocation);
        io->p[0] = theLocation.mX; io->p[1] = theLocation.mY; io->p[2] = theLocation.mZ;
        return 0;
    }
    case EE_UNITS_AG_NEW_LOCATION: {
        GE3DPoint theLocation(io->p[0], io->p[1], io->p[2]);
        return EEObj(static_cast<EEUnitGoal*>(EEUGRust::NewLocation(&theLocation, (io->a & 1) != 0, (io->a & 2) != 0,
            (io->a & 4) != 0, io->f, (io->a & 8) != 0, (unsigned char)io->b)));
    }
    }
    return 0;
}
