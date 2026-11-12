// New (port): the unit store's C++ side (ee/EEUnitStore.h; docs/RUST_PLAN.md R5b.4 leaf 5): the holders' write
// reports, the insert's seed, the removal, the box selection's pick, and the TEST ONLY consistency check and pick
// records. The records are Rust's
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
#include "ee/EEUnitClassIDs.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitAction.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
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

static uint32_t FloatBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

// the command's type as the checksum reads it (mCommand ? mCommand->mType : 0)
static int64_t CommandOf(EEComplexUnit* inUnit)
{
    EEUnitAction* theCommand = inUnit->mCommand.Get();
    return theCommand != NULL ? (int64_t)theCommand->mType : 0;
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
            Set(theUnit, EE_UNIT_FIELD_DOMAINS, theClass != NULL ? (int64_t)theClass->mDomains : 0);
            Set(theUnit, EE_UNIT_FIELD_LAND_BUILDING, theClass != NULL && theClass->IsLandBuilding() ? 1 : 0);
            Set(theUnit, EE_UNIT_FIELD_CAN_ATTACK_GROUND, theClass != NULL && theClass->mCanAttackGround ? 1 : 0);
        }
        break;
    }
    case kUnitFieldResourceAmount: {
        EEUnit* theUnit = OwnerOf<EEUnit>(inMember, offsetof(EEUnit, mResourceAmount));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_RESOURCE_AMOUNT, (int64_t)theUnit->mResourceAmount.Get());
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
    case kUnitFieldCommandable: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mIsCommandable));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_COMMANDABLE, theUnit->mIsCommandable.Get() ? 1 : 0);
        break;
    }
    case kUnitFieldFormationSlot: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mFormationSlot));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_FORMATION_SLOT, (int64_t)theUnit->mFormationSlot.Get());
        break;
    }
    case kUnitFieldLocation: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mPhysicalModel));
        if (theUnit->mInUnitStore) {
            const GETransformation& theLocation = theUnit->mPhysicalModel.GetLocation();
            ee_units_store_set_location(static_cast<uint32_t>(theUnit->mUnitID), reinterpret_cast<uintptr_t>(theUnit),
                                        FloatBits(theLocation.GetX()), FloatBits(theLocation.GetY()), FloatBits(theLocation.GetZ()));
        }
        break;
    }
    case kUnitFieldHeight: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mHeight));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_HEIGHT, (int64_t)FloatBits(theUnit->mHeight.Get()));
        break;
    }
    case kUnitFieldTimesUpdated: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mTimesUpdated));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_TIMES_UPDATED, (int64_t)theUnit->mTimesUpdated.Get());
        break;
    }
    case kUnitFieldActiveGroup: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mActiveGroup));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_ACTIVE_GROUP, theUnit->mActiveGroup.Get() != NULL ? 1 : 0);
        break;
    }
    case kUnitFieldCommand: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mCommand));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_COMMAND, CommandOf(theUnit));
        break;
    }
    case kUnitFieldReloadTimer: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mReloadTimer));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_RELOAD_TIMER, (int64_t)theUnit->mReloadTimer.Get());
        break;
    }
    case kUnitFieldTargetSearchTimer: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mTargetSearchTimer));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_TARGET_SEARCH_TIMER, (int64_t)theUnit->mTargetSearchTimer.Get());
        break;
    }
    case kUnitFieldHealSearchTimer: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mHealSearchTimer));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_HEAL_SEARCH_TIMER, (int64_t)FloatBits(theUnit->mHealSearchTimer.Get()));
        break;
    }
    case kUnitFieldNextTriggerTime: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mNextTriggerTime));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_NEXT_TRIGGER_TIME, (int64_t)theUnit->mNextTriggerTime.Get());
        break;
    }
    case kUnitFieldRottingAwayTime: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mRottingAwayTime));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_ROTTING_AWAY_TIME, (int64_t)theUnit->mRottingAwayTime.Get());
        break;
    }
    case kUnitFieldPower: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mPower));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_POWER, (int64_t)theUnit->mPower.Get());
        break;
    }
    case kUnitFieldPowerRegenTimer: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mPowerRegenTimer));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_POWER_REGEN_TIMER, (int64_t)theUnit->mPowerRegenTimer.Get());
        break;
    }
    case kUnitFieldCamouflageTimer: {
        EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mCamouflageTimer));
        if (theUnit->mInUnitStore)
            Set(theUnit, EE_UNIT_FIELD_CAMOUFLAGE_TIMER, (int64_t)theUnit->mCamouflageTimer.Get());
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

// in the world's unit chain (gWorld.mUnits: x-sorted, through mTileNode; RTSUnit::Link puts the previous link, the
// chain's head or a unit, in mPrev, RTSUnit::Unlink clears it)
static bool OnMap(RTSUnit* inUnit)
{
    return inUnit->mTileNode != NULL && inUnit->mTileNode->mPrev != NULL;
}

void EEUnitStoreLinked(RTSUnit* inUnit)
{
    if (inUnit->mInUnitStore)
        Set(inUnit, EE_UNIT_FIELD_ON_MAP, OnMap(inUnit) ? 1 : 0);
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
    outRecord.on_map = OnMap(inUnit) ? 1 : 0;
    outRecord.goal_kind = -1;
    outRecord.category = -1;
    outRecord.unit_type = -1;
    if (inUnit->mObjectKind.mValue >= EE_OBJECT_UNIT) {
        EEUnit* theUnit = static_cast<EEUnit*>(inUnit);
        outRecord.hit_points = (int64_t)theUnit->mHitPoints.Get();
        outRecord.resource_amount = (int64_t)theUnit->mResourceAmount.Get();
        outRecord.complex = (uint8_t)ComplexOf(theUnit);
        EEUnitClass* theClass = theUnit->mUnitClass.Get();
        if (theClass != NULL) {
            outRecord.category = (int64_t)theClass->mCategory;
            outRecord.unit_type = (int64_t)theClass->mUnitType;
            outRecord.domains = theClass->mDomains;
            outRecord.land_building = theClass->IsLandBuilding() ? 1 : 0;
            outRecord.can_attack_ground = theClass->mCanAttackGround ? 1 : 0;
        }
    }
    if (inUnit->mObjectKind.mValue == EE_OBJECT_COMPLEX_UNIT) {
        EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
        outRecord.built = theUnit->mIsBuilt.Get() ? 1 : 0;
        outRecord.goal_kind = (int32_t)GoalKind(theUnit->mGoalState.Get());
        outRecord.commandable = theUnit->mIsCommandable.Get() ? 1 : 0;
        outRecord.formation_slot = (int64_t)theUnit->mFormationSlot.Get();
        const GETransformation& theLocation = theUnit->mPhysicalModel.GetLocation();
        outRecord.location[0] = FloatBits(theLocation.GetX());
        outRecord.location[1] = FloatBits(theLocation.GetY());
        outRecord.location[2] = FloatBits(theLocation.GetZ());
        outRecord.height = FloatBits(theUnit->mHeight.Get());
        outRecord.times_updated = (int64_t)theUnit->mTimesUpdated.Get();
        outRecord.active_group = theUnit->mActiveGroup.Get() != NULL ? 1 : 0;
        outRecord.command = CommandOf(theUnit);
        outRecord.reload_timer = (int64_t)theUnit->mReloadTimer.Get();
        outRecord.target_search_timer = (int64_t)theUnit->mTargetSearchTimer.Get();
        outRecord.heal_search_timer = FloatBits(theUnit->mHealSearchTimer.Get());
        outRecord.next_trigger_time = (int64_t)theUnit->mNextTriggerTime.Get();
        outRecord.rotting_away_time = (int64_t)theUnit->mRottingAwayTime.Get();
        outRecord.power = (int64_t)theUnit->mPower.Get();
        outRecord.power_regen_timer = (int64_t)theUnit->mPowerRegenTimer.Get();
        outRecord.camouflage_timer = (int64_t)theUnit->mCamouflageTimer.Get();
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

void EEUnitOrderPush(unsigned long inID)
{
    ee_units_order_push(static_cast<uint32_t>(inID));
}

void EEUnitOrderErase(unsigned long inID)
{
    ee_units_order_erase(static_cast<uint32_t>(inID));
}

void EEUnitOrderClear()
{
    ee_units_order_clear();
}

// the deque's ids (the handles' own: a freed unit's handle resolves to NULL but keeps its id)
static void OrderIDs(std::vector<uint32_t>& outIDs)
{
    outIDs.clear();
    for (size_t i = 0; i < gWorld.mUpdateUnits.size(); i++)
        outIDs.push_back(gWorld.mUpdateUnits[i].GetSavedID());
}

void EEUnitOrderReset()
{
    std::vector<uint32_t> theIDs;
    OrderIDs(theIDs);
    ee_units_order_reset(theIDs.empty() ? NULL : &theIDs[0], theIDs.size());
}

static uint64_t sStepDigest = 0xcbf29ce484222325ull;
static uint32_t sStepCalls = 0;

static void StepFNV(uint32_t inWord)
{
    for (int i = 0; i < 4; i++) {
        sStepDigest ^= (inWord >> (8 * i)) & 0xff;
        sStepDigest *= 0x100000001b3ull;
    }
}

void EEUnitStepNote(unsigned int inKind, unsigned long inID, const uint32_t* inWords, unsigned int inCount)
{
    if (!EESimCaptureRecord::Active())
        return;
    StepFNV(inKind);
    StepFNV(static_cast<uint32_t>(inID));
    for (unsigned int i = 0; i < inCount; i++)
        StepFNV(inWords[i]);
    sStepCalls++;
}

void EEUnitStepCommit()
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord theRecord(0x4d545355u);   // "USTM"
    theRecord.U32((unsigned int)gPhysics->mNumTicksDone);
    theRecord.U32(sStepCalls);
    theRecord.U32((unsigned int)(sStepDigest & 0xffffffffu));
    theRecord.U32((unsigned int)(sStepDigest >> 32));
    theRecord.Commit();
    sStepDigest = 0xcbf29ce484222325ull;
    sStepCalls = 0;
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
    // the world's unit chain (the out-of-sync checksum's walk) against the records' on-the-map flags, and the
    // checksum's flag 1 over it (EEComplexUnit::GetChecksum(1)) against the store's sum; a mismatch counts like a
    // field's (so the USCK record, and its digest, stay the same while there is none)
    static std::vector<uint32_t> sChainIDs;
    static std::vector<uint64_t> sChainObjects;
    sChainIDs.clear();
    sChainObjects.clear();
    unsigned long theFlag1 = 0;
    unsigned long theFlag2 = 0;
    for (RTSUnit* theUnit = gWorld.mUnits.mFirst; theUnit != NULL; theUnit = theUnit->mTileNode->mNext) {
        sChainIDs.push_back(static_cast<uint32_t>(theUnit->mUnitID));
        sChainObjects.push_back(reinterpret_cast<uintptr_t>(theUnit));
        theFlag1 += static_cast<EEUnit*>(theUnit)->GetChecksum(1);
        theFlag2 += static_cast<EEUnit*>(theUnit)->GetChecksum(2);
    }
    uint32_t theOffMap = ee_units_store_check_on_map(sChainIDs.empty() ? NULL : &sChainIDs[0],
                                                     sChainObjects.empty() ? NULL : &sChainObjects[0], sChainIDs.size());
    unsigned long theStoreFlag1 = (unsigned long)ee_units_store_checksum_flag1();
    unsigned long theStoreFlag2 = (unsigned long)ee_units_store_checksum_flag2();
    if (theOffMap != 0 || theStoreFlag1 != theFlag1 || theStoreFlag2 != theFlag2) {
        fprintf(stderr, "EE-UNIT-STORE: the unit chain (%zu units) and the store's on-the-map records differ: %u; flag 1 "
                "%08lx over the chain, %08lx over the store; flag 2 %08lx, %08lx\n", sChainIDs.size(), theOffMap,
                (unsigned long)(uint32_t)theFlag1, (unsigned long)(uint32_t)theStoreFlag1,
                (unsigned long)(uint32_t)theFlag2, (unsigned long)(uint32_t)theStoreFlag2);
        theCheck.mismatches += theOffMap != 0 ? theOffMap : 1;
    }
    // the stepped units' deque against the Rust list (ee_units order.rs), in order; a difference counts like a field's
    static std::vector<uint32_t> sOrderIDs;
    OrderIDs(sOrderIDs);
    uint32_t theUnrecorded = 0;
    uint32_t theOrderAt = ee_units_order_check(sOrderIDs.empty() ? NULL : &sOrderIDs[0], sOrderIDs.size(), &theUnrecorded);
    if (theOrderAt != 0) {
        fprintf(stderr, "EE-UNIT-STORE: the stepped units (%zu) and the Rust list (%u) differ at index %u\n",
                sOrderIDs.size(), ee_units_order_count(), theOrderAt - 1);
        theCheck.mismatches += 1;
    }
    static uint32_t sMostUnrecorded = 0;
    if (theUnrecorded > sMostUnrecorded) {   // TEST ONLY: stepped units with no store record (the step's leaves read it)
        sMostUnrecorded = theUnrecorded;
        fprintf(stderr, "EE-UNIT-ORDER: %u stepped units without a store record at %lu (tick %lu)\n", theUnrecorded,
                inWhere, gPhysics != NULL ? (unsigned long)gPhysics->mNumTicksDone : 0ul);
    }
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

// the box selection's pick over the unit store (ee_units select.rs: the loop of EEPlayer::SelectUnits 0x56a658 over the
// records in place of the objects; the player is its list's object, as the store records it)
size_t EESelectPick(EEPlayer* inPlayer, const uint32_t* inIDs, size_t inCount, uint32_t* outIDs)
{
    return ee_units_select_pick(reinterpret_cast<uintptr_t>(static_cast<RTSPlayer*>(inPlayer)), inIDs, inCount, outIDs);
}

void EESelectPickCapture(unsigned long inCase, EEPlayer* inPlayer, const uint32_t* inIDs, size_t inCount,
                         const uint32_t* inPicked, size_t inPickedCount)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord theRecord(0x4b505355u);   // "USPK"
    theRecord.U32((unsigned int)inCase);
    theRecord.I32((int)gWorld.GetPlayerIndex(inPlayer));
    theRecord.U32((unsigned int)inCount);
    theRecord.Bytes(inIDs, (unsigned int)(inCount * 4));
    theRecord.U32((unsigned int)inPickedCount);
    theRecord.Bytes(inPicked, (unsigned int)(inPickedCount * 4));
    theRecord.Commit();
}

static void PickCase(unsigned long inCase, EEPlayer* inPlayer, const std::vector<uint32_t>& inIDs)
{
    std::vector<uint32_t> thePicked(inIDs.size() + 1);
    const uint32_t* theIDs = inIDs.empty() ? NULL : &inIDs[0];
    size_t theCount = EESelectPick(inPlayer, theIDs, inIDs.size(), &thePicked[0]);
    EESelectPickCapture(inCase, inPlayer, theIDs, inIDs.size(), &thePicked[0], theCount);
}

void EESelectPickSweep()
{
    if (!EESimCaptureRecord::Active())
        return;
    unsigned long theNumPlayers = gWorld.GetNumPlayers();
    for (unsigned long p = 0; p < theNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        if (thePlayer == NULL)
            continue;
        std::vector<uint32_t> theOwn;
        for (RTSUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext)
            theOwn.push_back(static_cast<uint32_t>(theUnit->mUnitID));
        // 1: every unit of its list in the list's order; 2: reversed, then again (every id twice)
        PickCase(1, thePlayer, theOwn);
        std::vector<uint32_t> theTwice(theOwn.rbegin(), theOwn.rend());
        theTwice.insert(theTwice.end(), theOwn.begin(), theOwn.end());
        PickCase(2, thePlayer, theTwice);
        // 3: ids that are not its units (the next player's, the ids after its own, none, 0), then its own
        std::vector<uint32_t> theOthers;
        EEPlayer* theNext = gWorld.GetPlayer((p + 1) % theNumPlayers);
        if (theNext != NULL)
            for (RTSUnit* theUnit = theNext->mFirstUnit; theUnit != NULL && theOthers.size() < 200; theUnit = theUnit->mNext)
                theOthers.push_back(static_cast<uint32_t>(theUnit->mUnitID));
        for (size_t i = 0; i < theOwn.size() && i < 100; i++)
            theOthers.push_back(theOwn[i] + 1);
        theOthers.push_back(0xffffffffu);
        theOthers.push_back(0);
        theOthers.insert(theOthers.end(), theOwn.begin(), theOwn.end());
        PickCase(3, thePlayer, theOthers);
        // 4: the selections the game sends (at most 63 ids)
        for (size_t i = 0; i < theOwn.size(); i += 63) {
            size_t theEnd = i + 63 < theOwn.size() ? i + 63 : theOwn.size();
            std::vector<uint32_t> theChunk(theOwn.begin() + i, theOwn.begin() + theEnd);
            PickCase(4, thePlayer, theChunk);
        }
    }
}

// EEActiveGroup::Fill's test over the unit store (ee_units group.rs: the loop of EEActiveGroup::Fill 0x5ed9d4 with
// EEActiveGroup::IsInFormationSlot 0x5edb21 over the records in place of the objects)
size_t EEFillMembers(EEComplexUnit* const* inUnits, size_t inCount, long inSlot, uint32_t* outIndices)
{
    if (inCount == 0)
        return 0;
    std::vector<uint32_t> theIDs(inCount);
    for (size_t i = 0; i < inCount; i++)
        theIDs[i] = static_cast<uint32_t>(inUnits[i]->mUnitID);
    return ee_units_fill_members(&theIDs[0], inCount, (int64_t)inSlot, outIndices);
}

void EEFillCapture(unsigned long inCase, EEPlayer* inPlayer, EEComplexUnit* const* inUnits, size_t inCount, long inSlot,
                   const uint32_t* inIndices, size_t inTaken)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord theRecord(0x4c465355u);   // "USFL"
    theRecord.U32((unsigned int)inCase);
    theRecord.I32((int)gWorld.GetPlayerIndex(inPlayer));
    theRecord.I32((int)inSlot);
    theRecord.U32((unsigned int)inCount);
    for (size_t i = 0; i < inCount; i++)
        theRecord.U32(static_cast<uint32_t>(inUnits[i]->mUnitID));
    theRecord.U32((unsigned int)inTaken);
    theRecord.Bytes(inIndices, (unsigned int)(inTaken * 4));
    theRecord.Commit();
}

static void FillCase(unsigned long inCase, EEPlayer* inPlayer, const std::vector<EEComplexUnit*>& inUnits, long inSlot)
{
    std::vector<uint32_t> theIndices(inUnits.size() + 1);
    EEComplexUnit* const* theUnits = inUnits.empty() ? NULL : &inUnits[0];
    size_t theTaken = EEFillMembers(theUnits, inUnits.size(), inSlot, &theIndices[0]);
    EEFillCapture(inCase, inPlayer, theUnits, inUnits.size(), inSlot, &theIndices[0], theTaken);
}

void EEFillSweep()
{
    if (!EESimCaptureRecord::Active())
        return;
    unsigned long theNumPlayers = gWorld.GetNumPlayers();
    for (unsigned long p = 0; p < theNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        if (thePlayer == NULL)
            continue;
        std::vector<EEComplexUnit*> theOwn;   // its list's complex units (a selection holds only those)
        for (RTSUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext)
            if (theUnit->mObjectKind.mValue == EE_OBJECT_COMPLEX_UNIT)
                theOwn.push_back(static_cast<EEComplexUnit*>(theUnit));
        std::vector<EEComplexUnit*> theReversed(theOwn.rbegin(), theOwn.rend());
        // every slot (0..5) and one past them, the list in its order (1) and reversed (2)
        for (long theSlot = 0; theSlot <= 6; theSlot++) {
            FillCase(1, thePlayer, theOwn, theSlot);
            FillCase(2, thePlayer, theReversed, theSlot);
        }
    }
}
