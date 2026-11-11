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
