// New (port): the unit store's C++ side (ee/EEUnitStore.h; docs/RUST_PLAN.md R5b.4 leaf 5): the holders' write
// reports, the insert's seed, the removal, and the TEST ONLY consistency check. The records are Rust's
// (rust/crates/ee_units/src/store.rs).
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

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESimCapture.h"
#include "ee/EEUnitStore.h"
#include "rust/ee_rust.h"

// the holders' owners from their offsets: the unit classes derive singly (RTSUnit -> EEUnit -> EEComplexUnit), so an
// offsetof is the member's place in every object of the class
#if defined(__clang__)
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#endif

template <class C>
static C* OwnerOf(const void* inMember, size_t inOffset)
{
    return reinterpret_cast<C*>(const_cast<char*>(static_cast<const char*>(inMember)) - inOffset);
}

static void Set(RTSUnit* inUnit, uint32_t inField, int64_t inValue)
{
    ee_units_store_set(static_cast<uint32_t>(inUnit->mUnitID), reinterpret_cast<uintptr_t>(inUnit), inField, inValue);
}

static int64_t GoalKind(EEUnitGoal* inGoal)
{
    return inGoal != NULL ? (int64_t)inGoal->mKind : -1;
}

static int64_t ComplexOf(EEUnit* inUnit)
{
    EEComplexUnit* theComplex = inUnit->mComplexUnit.Get();
    if (theComplex == NULL)
        return EE_UNIT_COMPLEX_NONE;
    return static_cast<EEUnit*>(theComplex) == inUnit ? EE_UNIT_COMPLEX_SELF : EE_UNIT_COMPLEX_OTHER;
}

void EEUnitStoreWrote(unsigned int inField, const void* inMember)
{
    switch (inField) {
    case kUnitFieldTileX: {
        RTSUnit* theUnit = OwnerOf<RTSUnit>(inMember, offsetof(RTSUnit, mTileX));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_TILE_X, (int64_t)theUnit->mTileX.Get());
        break;
    }
    case kUnitFieldTileY: {
        RTSUnit* theUnit = OwnerOf<RTSUnit>(inMember, offsetof(RTSUnit, mTileY));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_TILE_Y, (int64_t)theUnit->mTileY.Get());
        break;
    }
    case kUnitFieldValid: {
        RTSUnit* theUnit = OwnerOf<RTSUnit>(inMember, offsetof(RTSUnit, mIsValid));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_VALID, theUnit->mIsValid.Get() ? 1 : 0);
        break;
    }
    case kUnitFieldHitPoints: {
        EEUnit* theUnit = OwnerOf<EEUnit>(inMember, offsetof(EEUnit, mHitPoints));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_HIT_POINTS, (int64_t)theUnit->mHitPoints.Get());
        break;
    }
    case kUnitFieldUnitClass: {
        EEUnit* theUnit = OwnerOf<EEUnit>(inMember, offsetof(EEUnit, mUnitClass));
        if (theUnit->mInUnitStore) {
            EEUnitClass* theClass = theUnit->mUnitClass.Get();
            Set(theUnit, EE_UNIT_FIELD_CATEGORY, theClass != NULL ? (int64_t)theClass->mCategory : -1);
            Set(theUnit, EE_UNIT_FIELD_UNIT_TYPE, theClass != NULL ? (int64_t)theClass->mUnitType : -1);
        }
        break;
    }
    case kUnitFieldComplex: {
        EEUnit* theUnit = OwnerOf<EEUnit>(inMember, offsetof(EEUnit, mComplexUnit));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_COMPLEX, ComplexOf(theUnit));
        break;
    }
    case kUnitFieldBuilt: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mIsBuilt));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_BUILT, theUnit->mIsBuilt.Get() ? 1 : 0);
        break;
    }
    case kUnitFieldGoal: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mGoalState));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_GOAL_KIND, GoalKind(theUnit->mGoalState.Get()));
        break;
    }
    default:
        break;
    }
}

// the record of a unit as its object is now (owner: the list's player id; inPlayer: that player object)
static void Fill(RTSUnit* inUnit, unsigned long inOwner, const void* inPlayer, EeUnitRecord& outRecord)
{
    memset(&outRecord, 0, sizeof(outRecord));
    outRecord.id = static_cast<uint32_t>(inUnit->mUnitID);
    outRecord.owner = static_cast<uint32_t>(inOwner);
    outRecord.object = reinterpret_cast<uintptr_t>(inUnit);
    outRecord.player = reinterpret_cast<uintptr_t>(inPlayer);
    outRecord.object_kind = inUnit->mObjectKind.mValue;
    outRecord.valid = inUnit->mIsValid.Get() ? 1 : 0;
    outRecord.tile_x = (int64_t)inUnit->mTileX.Get();
    outRecord.tile_y = (int64_t)inUnit->mTileY.Get();
    outRecord.goal_kind = -1;
    outRecord.category = -1;
    outRecord.unit_type = -1;
    if (inUnit->mObjectKind.mValue >= EE_OBJECT_UNIT) {
        EEUnit* theUnit = static_cast<EEUnit*>(inUnit);
        outRecord.hit_points = (int64_t)theUnit->mHitPoints.Get();
        outRecord.complex = (uint8_t)ComplexOf(theUnit);
        EEUnitClass* theClass = theUnit->mUnitClass.Get();
        if (theClass != NULL) {
            outRecord.category = (int64_t)theClass->mCategory;
            outRecord.unit_type = (int64_t)theClass->mUnitType;
        }
    }
    if (inUnit->mObjectKind.mValue == EE_OBJECT_COMPLEX_UNIT) {
        EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
        outRecord.built = theUnit->mIsBuilt.Get() ? 1 : 0;
        outRecord.goal_kind = (int32_t)GoalKind(theUnit->mGoalState.Get());
    }
}

void EEUnitStoreInsert(RTSUnit* inUnit, unsigned long inOwner, const RTSPlayer* inPlayer)
{
    EeUnitRecord theRecord;
    Fill(inUnit, inOwner, inPlayer, theRecord);
    ee_units_store_insert(&theRecord);
    inUnit->mInUnitStore = true;
}

void EEUnitStoreRemove(RTSUnit* inUnit)
{
    if (!inUnit->mInUnitStore)
        return;
    ee_units_store_remove(static_cast<uint32_t>(inUnit->mUnitID), reinterpret_cast<uintptr_t>(inUnit));
    inUnit->mInUnitStore = false;
}

void EEUnitStoreClearPlayer(const RTSPlayer* inPlayer)
{
    ee_units_store_clear_player(reinterpret_cast<uintptr_t>(inPlayer));
}

unsigned long EEUnitStoreCheck(unsigned long inWhere)
{
    static std::vector<EeUnitRecord> sObjects;
    sObjects.clear();
    unsigned long theListed = 0;
    for (unsigned long i = 0; i < 17; i++) {
        EEPlayer* thePlayer = gWorld.mPlayers[i];
        if (thePlayer == NULL)
            continue;
        unsigned long theCount = 0;
        for (RTSUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext) {
            EeUnitRecord theRecord;
            Fill(theUnit, thePlayer->mPlayerID, static_cast<RTSPlayer*>(thePlayer), theRecord);
            sObjects.push_back(theRecord);
            theCount++;
        }
        theListed += theCount;
    }
    EeUnitStoreCheck theCheck;
    ee_units_store_check(sObjects.empty() ? NULL : &sObjects[0], sObjects.size(), &theCheck);
    EESimCaptureRecord theRecord(0x4b435355u);   // "USCK"
    theRecord.U32((unsigned int)inWhere);
    theRecord.U32(theCheck.compared);
    theRecord.U32(theCheck.mismatches);
    theRecord.Commit();
    if (theCheck.mismatches != 0) {
        fprintf(stderr, "EE-UNIT-STORE: %u of %u units differ from the store (%u records) at %lu: first id 0x%08x field %u\n",
                theCheck.mismatches, theCheck.compared, theCheck.records, inWhere, theCheck.first_id, theCheck.first_field);
        EESimCaptureRecord::Flush();
        exit(3);
    }
    return theListed;
}
