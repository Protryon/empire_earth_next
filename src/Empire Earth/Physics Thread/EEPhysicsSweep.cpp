// New (port), TEST ONLY: the host of the two capture sweeps whose bodies are Rust (the units' lane, round 5): the
// units sweep of EE-SIM --unit-sweep (ee_sim units unit_sweep.rs, the capture's units profile) and the active
// groups' sweep (ag_sweep.rs, the groups profile, its AGPL records). The Rust reads the world through the views;
// this host makes and produces the units, makes the groups and goals, and writes the AGPL record.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DPolygon.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUGRust.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEFormation.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EECombatFormulas.h"
#include "ee/EESimCapture.h"
#include "rust/sim/units.h"

namespace {

template <class T> T* Of(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

void Point(EESimCaptureRecord& r, const GE3DPoint& inPoint)
{
    r.F32(inPoint.mX);
    r.F32(inPoint.mY);
    r.F32(inPoint.mZ);
}

void RecordGoal(EESimCaptureRecord& r, EEUnitGoal* inGoal)
{
    r.I32(inGoal != NULL ? (int)inGoal->mKind : -1);
    if (inGoal == NULL)
        return;
    r.U32((unsigned int)inGoal->mID);
    GE3DPoint theLocation;
    inGoal->GetGoalLocation(theLocation);
    Point(r, theLocation);
}

// the AGPL record: the group as left, every unit's goal, queues, command, formation place and group, one draw
void Record(long inCase, EEPlayer* inPlayer, EEActiveGroup* inGroup, EEComplexUnit* const* inUnits, size_t inCount,
            unsigned int inAnswers)
{
    EESimCaptureRecord r(EE_COMBAT_TAG('A', 'G', 'P', 'L'));
    r.I32((int)inCase);
    r.U32((unsigned int)inPlayer->GetPlayerID());
    r.U32(inAnswers);
    r.U32((unsigned int)inGroup->GetNumUnits());
    for (unsigned long i = 0; i < inGroup->GetNumUnits(); i++)
        r.U32((unsigned int)inGroup->GetUnit(i)->mUnitID);
    r.F32s(&inGroup->mFields.mSpeed, 17);
    r.U32(inGroup->mFields.mMovesTogether);
    r.U32((unsigned int)inPlayer->GetNumPendingGroups());
    for (size_t i = 0; i < inCount; i++) {
        EEComplexUnit* u = inUnits[i];
        r.U32((unsigned int)u->mUnitID);
        r.I32((int)u->mHitPoints);
        RecordGoal(r, u->mGoalState);
        r.U32((unsigned int)u->mGoalQueues.size());
        for (size_t q = 0; q < u->mGoalQueues.size(); q++) {
            r.U32((unsigned int)u->mGoalQueues[q].mGoals.size());
            for (size_t k = 0; k < u->mGoalQueues[q].mGoals.size(); k++)
                RecordGoal(r, u->mGoalQueues[q].mGoals[k]);
        }
        r.U32((unsigned int)u->GetCommandType());
        r.U32((unsigned int)u->mFormationSlot);
        r.I32((int)u->mFormationRank);
        r.U32(u->mIsSpecialAttack);
        r.U32(u->mActiveGroup == inGroup);
        r.U32(u->mActiveGroup != NULL);
    }
    r.I32((int)gPhysics->GetRandom(0L, 0x3fffffffL));
    r.Commit();
}

EEUnitGoal* Goal(uint64_t inKind, uint64_t inUnit, uint64_t inTarget, int64_t inFlags, const float* inPoint)
{
    GE3DPoint thePoint(inPoint[0], inPoint[1], inPoint[2]);
    switch (inKind) {
    case EE_UNITS_SWEEP_GOAL_MOVE: {
        uint32_t theBits = (uint32_t)((uint64_t)inFlags >> 32);
        float theYaw;
        memcpy(&theYaw, &theBits, sizeof(theYaw));
        EEUGRust* theGoal = static_cast<EEUGRust*>(EEUGRust::NewLocation(&thePoint, true, true, theYaw != 0.0f, theYaw, false, 0));
        theGoal->SetUseGroupMovement((inFlags & 1) != 0);
        return theGoal;
    }
    case EE_UNITS_SWEEP_GOAL_ATTACK_UNIT: return EEUGRust::NewAttackUnit(Of<EEComplexUnit>(inUnit), Of<EEUnit>(inTarget), true, false);
    case EE_UNITS_SWEEP_GOAL_ATTACK_GROUND: return EEUGRust::NewAttackGround(thePoint, true);
    case EE_UNITS_SWEEP_GOAL_GATHER: return EEUGRust::NewGather(Of<EEUnit>(inUnit), Of<EEUnit>(inTarget), true);
    case EE_UNITS_SWEEP_GOAL_HEAL: return EEUGRust::NewHealUnit(Of<EEComplexUnit>(inUnit), true);
    case EE_UNITS_SWEEP_GOAL_EXPLORE: return EEUGRust::NewExplore(true);
    case EE_UNITS_SWEEP_GOAL_UNLOAD:
        return EEUGRust::NewUnloadTransport(Of<EEComplexUnit>(inUnit), (long)thePoint.mX, (long)thePoint.mY, true);
    case EE_UNITS_SWEEP_GOAL_PATROL:
        return EEUGRust::NewPatrol(Of<EEPPatrolGroup>(inTarget),
                                   static_cast<EEComplexUnit*>(ee_units_pg_last(Of<EEPPatrolGroup>(inTarget))), true);
    default: return NULL;
    }
}

}   // namespace

extern "C" int64_t ee_units_sweep_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, const float* inPoint)
{
    switch (inOp) {
    case EE_UNITS_SWEEP_OP_CLASS_INFO: {
        EEUnitClass* theClass = Of<EEUnitClass>(inA);
        int64_t theKind = 0xff;   // its gPhysicsModels[] index (the singletons' order: 0 land .. 11 spaceship)
        for (long i = 0; i < 12; i++)
            if (theClass->mPhysicsModel != NULL && gPhysicsModels[i] == theClass->mPhysicsModel)
                theKind = i;
        return (theClass->IsBuilding() ? 1 : 0) | (theClass->IsLandBuilding() ? 2 : 0) | (theClass->IsAircraft() ? 4 : 0) | theKind << 8;
    }
    case EE_UNITS_SWEEP_OP_CREATE: {
        EEPlayer* thePlayer = Of<EEPlayer>(inB);
        EEUnit* theUnit = Of<EEUnitClass>(inA)->CreateUnit((long)inC, (long)inD, thePlayer->GetPlayerID(), false, NULL);
        if (theUnit == NULL)
            return 0;
        // its id first (the player's insert gives it): finishing it activates it, and the active list holds ids
        thePlayer->AddPendingUnits(false, true);
        gWorld.FlushQueuedPlacements();
        if (theUnit->mTileNode != NULL)
            theUnit->GetComplexUnit()->SetBuilt(1, 0, 1);
        gWorld.mMap->PlaceUnitOnGround(theUnit);
        return theUnit->mUnitID;
    }
    case EE_UNITS_SWEEP_OP_PRODUCE: {
        EEComplexUnit* theProducer = Of<EEComplexUnit>(inA);
        bool theDone = theProducer->mUnitClass->ProduceUnit(theProducer, Of<EEUnitClass>(inB), false);
        theProducer->mPlayer->AddPendingUnits(false, true);
        gWorld.FlushQueuedPlacements();
        return theDone;
    }
    case EE_UNITS_SWEEP_OP_ORDER: {
        EEComplexUnit* theUnit = Of<EEComplexUnit>(inA);
        EEUnitGoal* theGoal;
        if (inC != 0) {
            theGoal = EEUnitGoal::NewAttackUnit(theUnit, Of<EEUnit>(inB), true, false);
        } else {
            GE3DPoint theAt;
            Of<EEUnit>(inB)->GetPosition(theAt);
            theGoal = EEUnitGoal::NewLocation(&theAt, false, true, false, 0.0f, false, 0);
        }
        theUnit->SetGoal(theGoal, 0, 0);
        return 0;
    }
    case EE_UNITS_SWEEP_OP_NEW_GROUP: {
        EEActiveGroup* theGroup = EEActiveGroup::New(Of<EEPlayer>(inA));
        const uint64_t* theUnits = Of<const uint64_t>(inB);
        for (int64_t i = 0; i < inC; i++) {
            EEComplexUnit* theUnit = Of<EEComplexUnit>(theUnits[i]);
            if (theUnit->mHitPoints > 0 && theUnit->mIsValid)
                theGroup->AddMember(theUnit, true);
        }
        EEFormation* theFormation = EEFormation::Create((long)inD);
        if (theFormation != NULL) {
            theGroup->SetFormation(theFormation);
            theFormation->Dereference();
        }
        return (int64_t)(uintptr_t)theGroup;
    }
    case EE_UNITS_SWEEP_OP_RUN: {
        EEActiveGroup* theGroup = Of<EEActiveGroup>(inA);
        if (inC & 2)
            theGroup->mFields.mIsSpecialAttack = true;
        theGroup->SetGoal(Of<EEUnitGoal>(inB), (inC & 1) != 0);
        if (inD >= 0) {   // the commands' way: the action, into the pending groups, Plan
            theGroup->mFields.mAction = (long)inD;
            theGroup->GetPlayer()->AddPendingGroup(theGroup);
            theGroup->Plan();
        }
        return 0;
    }
    case EE_UNITS_SWEEP_OP_GOAL:
        if (inA == EE_UNITS_SWEEP_GOAL_FIND_PATROL) {
            EEPlayer* thePlayer = Of<EEPlayer>(inB);
            for (unsigned long t = 0; t < thePlayer->mTowns.size(); t++) {
                EETown* theTown = thePlayer->mTowns[t];
                if (theTown != NULL && theTown->mPatrolGroup != NULL && ee_units_pg_last(theTown->mPatrolGroup) != NULL)
                    return (int64_t)(uintptr_t)theTown->mPatrolGroup;
            }
            return 0;
        }
        return (int64_t)(uintptr_t)Goal(inA, inB, (uint64_t)inC, inD, inPoint);
    case EE_UNITS_SWEEP_OP_RECORD: {
        uint32_t theAnswers;
        memcpy(&theAnswers, &inPoint[0], sizeof(theAnswers));
        Record((long)(int32_t)(inA & 0xffffffffu), Of<EEPlayer>(inB), Of<EEActiveGroup>((uint64_t)inC),
               Of<EEComplexUnit* const>((uint64_t)inD), (size_t)(inA >> 32), theAnswers);
        return 0;
    }
    case EE_UNITS_SWEEP_OP_TELL: {
        GE3DPoint thePoint;
        Of<EEComplexUnit>(inB)->GetPosition(thePoint);
        Of<EEActiveGroup>(inA)->TellGroup(Of<EEComplexUnit>(inB), thePoint);
        return 0;
    }
    default:
        return 0;
    }
}
