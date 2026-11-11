// New (port): the world calls a Rust decision makes (rust/crates/ee_units/src/host.rs; docs/RUST_PLAN.md R5b.4
// leaf 5): a group's units as ids, goals built from a plain record (units by id, resolved through the registry) and
// handed back as opaque handles. Every call runs on the thread that called the decision (the physics thread, under
// the world's write lock).
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
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

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGExplore.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGHealUnit.h"
#include "ee/EEUGPatrol.h"
#include "ee/EEActiveGroup.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnitHandle.h"
#include "ee/EEUnitStore.h"
#include "rust/ee_rust.h"

extern "C" size_t ee_units_host_group_ids(void* inGroup, uint32_t* outIDs, size_t inCapacity)
{
    EEGroup* theGroup = static_cast<EEGroup*>(inGroup);
    size_t theCount = theGroup->mUnits.size();
    for (size_t i = 0; i < theCount && i < inCapacity; i++) {
        EEComplexUnit* theUnit = theGroup->mUnits[i];
        outIDs[i] = theUnit != NULL ? static_cast<uint32_t>(theUnit->mUnitID) : 0xffffffffu;
    }
    return theCount;
}

static EEUnit* UnitOf(uint32_t inID)
{
    EEUnit* theUnit;
    EEUnitHandleResolve(inID, theUnit);
    return theUnit;
}

static EEComplexUnit* ComplexOf(uint32_t inID)
{
    EEComplexUnit* theUnit;
    EEUnitHandleResolve(inID, theUnit);
    return theUnit;
}

extern "C" void* ee_units_host_goal_new(const EeGoalSpec* inSpec)
{
    bool theFlagA = inSpec->flag_a != 0;
    bool theFlagB = inSpec->flag_b != 0;
    switch (inSpec->ctor) {
    case EE_GOAL_CTOR_EXPLORE:
        return static_cast<EEUnitGoal*>(new EEUGExplore(theFlagA));
    case EE_GOAL_CTOR_ATTACK_GROUND:
        return static_cast<EEUnitGoal*>(new EEUGAttackGround(GE3DPoint(inSpec->point[0], inSpec->point[1], inSpec->point[2]), theFlagA));
    case EE_GOAL_CTOR_ATTACK_UNIT: {
        EEUnit* theTarget = UnitOf(inSpec->unit_b);
        if (theTarget == NULL)
            return NULL;
        return static_cast<EEUnitGoal*>(new EEUGAttackUnit(ComplexOf(inSpec->unit_a), theTarget, theFlagA, theFlagB));
    }
    case EE_GOAL_CTOR_GATHER:
        return static_cast<EEUnitGoal*>(new EEUGGather(UnitOf(inSpec->unit_a), UnitOf(inSpec->unit_b), theFlagA));
    case EE_GOAL_CTOR_HEAL_UNIT: {
        EEComplexUnit* theTarget = ComplexOf(inSpec->unit_a);
        if (theTarget == NULL)
            return NULL;
        return static_cast<EEUnitGoal*>(new EEUGHealUnit(theTarget, theFlagA));
    }
    case EE_GOAL_CTOR_PATROL:
        return static_cast<EEUnitGoal*>(new EEUGPatrol(theFlagA));
    default:
        return NULL;
    }
}

extern "C" uint8_t ee_units_host_goal_kind(void* inGoal)
{
    return static_cast<EEUnitGoal*>(inGoal)->mKind;
}

extern "C" void ee_units_host_goal_release(void* inGoal)
{
    delete static_cast<EEUnitGoal*>(inGoal);
}

// a group as Rust holds it: its EEGroup base (the address group_ids reads)
static EEActiveGroup* ActiveOf(void* inGroup)
{
    return static_cast<EEActiveGroup*>(static_cast<EEGroup*>(inGroup));
}

extern "C" void* ee_units_host_active_group_new(void* inPlayer)
{
    return static_cast<EEGroup*>(new EEActiveGroup(static_cast<EEPlayer*>(inPlayer)));
}

extern "C" void ee_units_host_group_fill(void* inGroup, void* inSelection, int32_t inSort, int32_t inSlot)
{
    ActiveOf(inGroup)->Fill(static_cast<EESelectedGroup*>(static_cast<EEGroup*>(inSelection)), inSort, inSlot);
}

extern "C" void ee_units_host_group_release(void* inGroup)
{
    ActiveOf(inGroup)->Dereference();
}

extern "C" void ee_units_host_pending_push(void* inPlayer, void* inGroup)
{
    static_cast<EEPlayer*>(inPlayer)->mPendingGroups.push_back(static_cast<EEGroup*>(inGroup));
}

extern "C" void ee_units_host_group_keep_mobile(void* inGroup)
{
    ActiveOf(inGroup)->KeepMobileUnits();
}

extern "C" void ee_units_host_group_clear_goal(void* inGroup)
{
    ActiveOf(inGroup)->ClearGoal();
}

extern "C" void ee_units_host_group_set_goal(void* inGroup, void* inGoal, uint8_t inQueued)
{
    ActiveOf(inGroup)->SetGoal(static_cast<EEUnitGoal*>(inGoal), inQueued != 0);
}

extern "C" void ee_units_host_group_set_action(void* inGroup, int32_t inAction)
{
    ActiveOf(inGroup)->mAction = inAction;
}

extern "C" void ee_units_host_group_plan(void* inGroup)
{
    ActiveOf(inGroup)->Plan();
}

extern "C" int32_t ee_units_host_random(int32_t inLow, int32_t inHigh)
{
    return (int32_t)gPhysics->GetRandom((long)inLow, (long)inHigh);
}

extern "C" void ee_units_host_play_acknowledgement(void* inPlayer, void* inGroup, int32_t inIndex)
{
    static_cast<EEPlayer*>(inPlayer)->PlayAcknowledgement(ActiveOf(inGroup)->GetUnit(inIndex));
}
