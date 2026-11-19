// New (port): the unit store's C++ side (ee/EEUnitStore.h; docs/RUST_PLAN.md R5b.4 leaf 5): the location's write
// reports, the insert's seed, the removal, the box selection's pick, and the TEST ONLY consistency check and pick
// records. The records are Rust's
// (rust/crates/ee_sim/src/units/store.rs).
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
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitStore.h"
#include "rust/sim/units.h"

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

static uint32_t FloatBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

void EEUnitStoreWrote(unsigned int inField, const void* inMember)
{
    if (inField != kUnitFieldLocation)
        return;
    EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inMember, offsetof(EEComplexUnit, mPhysicalModel));
    if (theUnit->mInUnitStore) {
        const GETransformation& theLocation = theUnit->mPhysicalModel.GetLocation();
        ee_units_store_set_location(static_cast<uint32_t>(theUnit->mUnitID), reinterpret_cast<uintptr_t>(theUnit),
                                    FloatBits(theLocation.GetX()), FloatBits(theLocation.GetY()), FloatBits(theLocation.GetZ()));
    }
}

// in the world's unit chain (gWorld.mUnits: x-sorted, through mTileNode; RTSUnit::Link puts the previous link, the
// chain's head or a unit, in mPrev, RTSUnit::Unlink clears it)
static bool OnMap(RTSUnit* inUnit)
{
    return inUnit->mTileNode != NULL && inUnit->mTileNode->mPrev != NULL;
}

void EEGoalQueueReport(EEUnitVectorItem* inQueue)
{
    EeGoalQueueEntry theEntries[16];
    std::vector<EeGoalQueueEntry> theMore;
    size_t theCount = inQueue->mGoals.size();
    EeGoalQueueEntry* theList = theEntries;
    if (theCount > 16) {
        theMore.resize(theCount);
        theList = &theMore[0];
    }
    for (size_t i = 0; i < theCount; i++) {
        EEUnitGoal* theGoal = inQueue->mGoals[i];
        theList[i].id = theGoal != NULL ? (uint32_t)theGoal->mID : 0;
        theList[i].kind = theGoal != NULL ? (uint8_t)theGoal->mKind : 0;
        theList[i].goal = reinterpret_cast<uintptr_t>(theGoal);
    }
    EeGoalQueueTimer theTimer;
    theTimer.start = (uint32_t)inQueue->mStartTime;
    theTimer.time = (uint32_t)inQueue->mTime;
    theTimer.started = inQueue->mStarted ? 1 : 0;
    ee_units_goal_queue_report(reinterpret_cast<uintptr_t>(inQueue), reinterpret_cast<uintptr_t>(inQueue->mUnit), theList,
                               theCount, theTimer);
}

void EEGoalQueueDropped(EEUnitVectorItem* inQueue)
{
    ee_units_goal_queue_drop(reinterpret_cast<uintptr_t>(inQueue));
}

void EEGoalDeque::Changed()
{
    EEGoalQueueReport(OwnerOf<EEUnitVectorItem>(this, offsetof(EEUnitVectorItem, mGoals)));
}

void EEUnitStoreLinked(RTSUnit* inUnit)
{
    if (inUnit->mInUnitStore)
        Set(inUnit, EE_UNIT_FIELD_ON_MAP, OnMap(inUnit) ? 1 : 0);
}

// the record of a unit as its object is now (owner: the list's player id; inPlayer: that player object; the values
// read from its records and pointer members are the store's: ee_units_store_set_fields)
static void Fill(RTSUnit* inUnit, unsigned long inOwner, const void* inPlayer, EeUnitRecord& outRecord)
{
    memset(&outRecord, 0, sizeof(outRecord));
    outRecord.id = static_cast<uint32_t>(inUnit->mUnitID);
    outRecord.owner = static_cast<uint32_t>(inOwner);
    outRecord.object = reinterpret_cast<uintptr_t>(inUnit);
    outRecord.player = reinterpret_cast<uintptr_t>(inPlayer);
    outRecord.object_kind = inUnit->mObjectKind.mValue;
    outRecord.on_map = OnMap(inUnit) ? 1 : 0;
    if (inUnit->mObjectKind.mValue >= EE_OBJECT_UNIT)
        outRecord.stats = reinterpret_cast<uintptr_t>(static_cast<EEUnit*>(inUnit)->mStats);
    if (inUnit->mObjectKind.mValue == EE_OBJECT_COMPLEX_UNIT) {
        const GETransformation& theLocation = static_cast<EEComplexUnit*>(inUnit)->mPhysicalModel.GetLocation();
        outRecord.location[0] = FloatBits(theLocation.GetX());
        outRecord.location[1] = FloatBits(theLocation.GetY());
        outRecord.location[2] = FloatBits(theLocation.GetZ());
    }
    // the object's Rust records, and the plain values the store reads from them
    ee_units_store_set_fields(&outRecord, inUnit, inUnit->mObjectKind.mValue >= EE_OBJECT_UNIT ? static_cast<EEUnit*>(inUnit) : NULL,
                              inUnit->mObjectKind.mValue == EE_OBJECT_COMPLEX_UNIT ? static_cast<EEComplexUnit*>(inUnit) : NULL);
}

void EEUnitStoreInsertUnlisted(RTSUnit* inUnit)
{
    if (inUnit == NULL || inUnit->mInUnitStore || inUnit->mUnitID != 0xffffffff)
        return;
    EEPlayer* thePlayer = inUnit->mObjectKind.mValue >= EE_OBJECT_UNIT ? static_cast<EEUnit*>(inUnit)->mPlayer : NULL;
    EeUnitRecord theRecord;
    Fill(inUnit, thePlayer != NULL ? thePlayer->mPlayerID : 0, static_cast<const RTSPlayer*>(thePlayer), theRecord);
    ee_units_store_insert_unlisted(&theRecord);
    inUnit->mInUnitStore = true;
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
    ee_units_player_drop(reinterpret_cast<uintptr_t>(inPlayer));
}

// a player's id and stances as words (the table's key is its RTSPlayer object, as the unit records')
static void PlayerWords(const EEPlayer* inPlayer, uint32_t* outEnemies)
{
    for (unsigned long i = 0; i < 17; i++)
        outEnemies[i] = (uint32_t)inPlayer->mEnemies[i];
}

void EEUnitStorePlayerTable(const EEPlayer* inPlayer)
{
    uint32_t theEnemies[17];
    PlayerWords(inPlayer, theEnemies);
    ee_units_player_set(reinterpret_cast<uintptr_t>(static_cast<const RTSPlayer*>(inPlayer)), (uint32_t)inPlayer->mPlayerID,
                        theEnemies);
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
    for (size_t i = 0; i < gWorld.GetNumUpdateUnits(); i++)
        outIDs.push_back(gWorld.GetUpdateUnitID(i));
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

bool EEUnitStepNoting()
{
    return EESimCaptureRecord::Active();
}

static uint64_t sStateDigest = 0xcbf29ce484222325ull;
static uint32_t sStateCalls = 0;

static void StateFNV(uint32_t inWord)
{
    for (int i = 0; i < 4; i++) {
        sStateDigest ^= (inWord >> (8 * i)) & 0xff;
        sStateDigest *= 0x100000001b3ull;
    }
}

void EEUnitStateNote(unsigned int inKind, unsigned long inID, const uint32_t* inWords, unsigned int inCount)
{
    if (!EESimCaptureRecord::Active())
        return;
    StateFNV(inKind);
    StateFNV(static_cast<uint32_t>(inID));
    for (unsigned int i = 0; i < inCount; i++)
        StateFNV(inWords[i]);
    sStateCalls++;
}

static std::vector<uint32_t> sUpdateWords;

void EEUnitUpdateNote(int inOperation, int64_t inA, int64_t inB)
{
    if (!EESimCaptureRecord::Active())
        return;
    sUpdateWords.push_back(static_cast<uint32_t>(inOperation));
    sUpdateWords.push_back(static_cast<uint32_t>(inA));
    sUpdateWords.push_back(static_cast<uint32_t>(inB));
}

void EEUnitUpdateCommit(unsigned long inID)
{
    if (sUpdateWords.empty())
        return;
    EEUnitStateNote(kUnitStateUpdate, inID, sUpdateWords.data(), (unsigned int)sUpdateWords.size());
    sUpdateWords.clear();
}

static std::vector<uint32_t> sActionWords;
static unsigned int sActionDepth = 0;
static unsigned long sActionID = 0;

void EEUnitActionNote(int inOperation, int64_t inA, int64_t inB)
{
    if (!EESimCaptureRecord::Active())
        return;
    sActionWords.push_back(static_cast<uint32_t>(inOperation));
    sActionWords.push_back(static_cast<uint32_t>(inA));
    sActionWords.push_back(static_cast<uint32_t>(inB));
}

EEUnitActionScope::EEUnitActionScope(unsigned long inID)
{
    if (sActionDepth++ == 0)
        sActionID = inID;
}

EEUnitActionScope::~EEUnitActionScope()
{
    if (--sActionDepth != 0 || sActionWords.empty())
        return;
    EEUnitStateNote(kUnitStateActionChanged, sActionID, sActionWords.data(), (unsigned int)sActionWords.size());
    sActionWords.clear();
}

void EEUnitStepDiffers(const char* inLeaf, unsigned long inID, long inRust, long inOriginal)
{
    fprintf(stderr, "EE-UNIT-STEP: %s of unit 0x%08lx: Rust %ld, the original %ld\n", inLeaf, inID, inRust, inOriginal);
    EESimCaptureRecord::Flush();
    exit(3);
}

void EEUnitStepCommit()
{
    if (!EESimCaptureRecord::Active())
        return;
    // the leaves' store reads that differed from the members (the holders keep them equal): a USCK mismatch
    uint32_t theMismatches = 0, theUnrecorded = 0;
    ee_units_step_reads(&theMismatches, &theUnrecorded);
    if (theMismatches != 0)
        EEUnitStepDiffers("a store read (records differing from the members)", 0, (long)theMismatches, 0);
    EESimCaptureRecord theRecord(0x4d545355u);   // "USTM"
    theRecord.U32((unsigned int)gPhysics->mNumTicksDone);
    theRecord.U32(sStepCalls);
    theRecord.U32((unsigned int)(sStepDigest & 0xffffffffu));
    theRecord.U32((unsigned int)(sStepDigest >> 32));
    theRecord.Commit();
    sStepDigest = 0xcbf29ce484222325ull;
    sStepCalls = 0;
    EESimCaptureRecord theState(0x54535355u);   // "USST"
    theState.U32((unsigned int)gPhysics->mNumTicksDone);
    theState.U32(sStateCalls);
    theState.U32((unsigned int)(sStateDigest & 0xffffffffu));
    theState.U32((unsigned int)(sStateDigest >> 32));
    theState.Commit();
    sStateDigest = 0xcbf29ce484222325ull;
    sStateCalls = 0;
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
    // the unlisted units' records (the shots, a scenario's path points) against their objects; a difference counts
    // like a field's (the compared count stays the listed units', so the USCK record is unchanged while there is none)
    {
        static std::vector<uint64_t> sUnlisted;
        static std::vector<EeUnitRecord> sUnlistedRecords;
        size_t theCount = ee_units_store_unlisted(NULL, 0);
        sUnlisted.resize(theCount);
        if (theCount != 0)
            ee_units_store_unlisted(&sUnlisted[0], theCount);
        sUnlistedRecords.clear();
        for (size_t i = 0; i < theCount; i++) {
            RTSUnit* theUnit = reinterpret_cast<RTSUnit*>(static_cast<uintptr_t>(sUnlisted[i]));
            EEPlayer* thePlayer = theUnit->mObjectKind.mValue >= EE_OBJECT_UNIT ? static_cast<EEUnit*>(theUnit)->mPlayer : NULL;
            EeUnitRecord theRecord;
            Fill(theUnit, thePlayer != NULL ? thePlayer->mPlayerID : 0, static_cast<const RTSPlayer*>(thePlayer), theRecord);
            sUnlistedRecords.push_back(theRecord);
        }
        uint32_t theUnlistedBad = ee_units_store_check_unlisted(sUnlistedRecords.empty() ? NULL : &sUnlistedRecords[0],
                                                                sUnlistedRecords.size(), &theCheck);
        if (theUnlistedBad != 0) {
            fprintf(stderr, "EE-UNIT-STORE: %u of the %zu unlisted units differ from the store\n", theUnlistedBad, theCount);
            theCheck.mismatches += theUnlistedBad;
        }
    }
    // every listed complex unit's goal queues (their containers in order, their goals' ids) against the Rust mirror
    // (ee_units goals.rs); a difference counts like a field's
    {
        static std::vector<uint64_t> sContainers;
        static std::vector<uint32_t> sLens;
        static std::vector<EeGoalQueueTimer> sTimers;
        static std::vector<uint32_t> sIDs;
        uint32_t theBad = 0;
        for (unsigned long p = 0; p < 17; p++) {
            EEPlayer* thePlayer = gWorld.mPlayers[p];
            if (thePlayer == NULL)
                continue;
            for (RTSUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext) {
                if (theUnit->mObjectKind.mValue != EE_OBJECT_COMPLEX_UNIT)
                    continue;
                EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(theUnit);
                sContainers.clear();
                sLens.clear();
                sTimers.clear();
                sIDs.clear();
                for (size_t q = 0; q < theComplex->mGoalQueues.size(); q++) {
                    EEUnitVectorItem& theQueue = theComplex->mGoalQueues[q];
                    sContainers.push_back(reinterpret_cast<uintptr_t>(&theQueue));
                    sLens.push_back((uint32_t)theQueue.mGoals.size());
                    EeGoalQueueTimer theTimer;
                    theTimer.start = (uint32_t)theQueue.mStartTime;
                    theTimer.time = (uint32_t)theQueue.mTime;
                    theTimer.started = theQueue.mStarted ? 1 : 0;
                    sTimers.push_back(theTimer);
                    for (size_t i = 0; i < theQueue.mGoals.size(); i++)
                        sIDs.push_back(theQueue.mGoals[i] != NULL ? (uint32_t)theQueue.mGoals[i]->mID : 0);
                }
                // (a queue's mUnit is its unit: the mirror's unit is the container's mUnit)
                uint64_t theOwner = theComplex->mGoalQueues.empty() ? reinterpret_cast<uintptr_t>(theComplex)
                                                                    : reinterpret_cast<uintptr_t>(theComplex->mGoalQueues[0].mUnit);
                theBad += ee_units_goal_queue_check_unit(theOwner, sContainers.empty() ? NULL : &sContainers[0],
                                                         sLens.empty() ? NULL : &sLens[0],
                                                         sTimers.empty() ? NULL : &sTimers[0], sContainers.size(),
                                                         sIDs.empty() ? NULL : &sIDs[0], sIDs.size());
            }
        }
        if (theBad != 0) {
            fprintf(stderr, "EE-UNIT-STORE: %u goal queues differ from the Rust mirror\n", theBad);
            theCheck.mismatches += theBad;
        }
    }
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
    // the world's players' diplomacy tables (ee_units players.rs); a difference counts like a field's
    {
        uint64_t theObjects[17];
        uint32_t theIDs[17];
        uint32_t theEnemies[17 * 17];
        size_t theCount = 0;
        for (unsigned long i = 0; i < 17; i++) {
            EEPlayer* thePlayer = gWorld.mPlayers[i];
            if (thePlayer == NULL)
                continue;
            theObjects[theCount] = reinterpret_cast<uintptr_t>(static_cast<RTSPlayer*>(thePlayer));
            theIDs[theCount] = (uint32_t)thePlayer->mPlayerID;
            PlayerWords(thePlayer, &theEnemies[17 * theCount]);
            theCount++;
        }
        uint32_t theDiffer = ee_units_player_check(theObjects, theIDs, theEnemies, theCount);
        if (theDiffer != 0) {
            fprintf(stderr, "EE-UNIT-STORE: %u of the %zu players' diplomacy tables differ from the store\n", theDiffer, theCount);
            theCheck.mismatches += theDiffer;
        }
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

// New (R5b.2 combat, ninth round): the fills behind the combat reads' store bridges (ee_rust's ee_combat_store_unit_of,
// ee_combat_store_player_of) for a unit out of every list (a killed unit a goal still names) or a player without a
// table: the same record and words as the store's, from the live object (its mPlayer as the list's player)
extern "C" void ee_combat_host_unit_record(uint64_t inObject, EeUnitRecord* outRecord)
{
    RTSUnit* theUnit = reinterpret_cast<RTSUnit*>(static_cast<uintptr_t>(inObject));
    EEPlayer* thePlayer = theUnit->mPlayer;
    Fill(theUnit, thePlayer != NULL ? thePlayer->mPlayerID : 0, static_cast<const RTSPlayer*>(thePlayer), *outRecord);
}

extern "C" void ee_combat_host_player_table(uint64_t inObject, uint32_t* outID, uint32_t* outEnemies)
{
    const EEPlayer* thePlayer = reinterpret_cast<const EEPlayer*>(static_cast<uintptr_t>(inObject));
    *outID = (uint32_t)thePlayer->mPlayerID;
    PlayerWords(thePlayer, outEnemies);
}
