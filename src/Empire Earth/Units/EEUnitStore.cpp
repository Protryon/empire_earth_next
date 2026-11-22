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

// TEST ONLY: the check's body is Rust (the units' lane, round 8: ee_units us_check.rs), its world reads this host's
// statements (EE_UNITS_US_*)
unsigned long EEUnitStoreCheck(unsigned long inWhere)
{
    return (unsigned long)ee_units_us_check(inWhere);
}

static RTSUnit* UsUnit(uint64_t inUnit) { return reinterpret_cast<RTSUnit*>(static_cast<uintptr_t>(inUnit)); }
static EEPlayer* UsPlayer(uint64_t inPlayer) { return reinterpret_cast<EEPlayer*>(static_cast<uintptr_t>(inPlayer)); }

extern "C" int64_t ee_units_us_host(int32_t inOp, uint64_t inA, int64_t inB, uint64_t inC, void* outData)
{
    switch (inOp) {
    case EE_UNITS_US_PLAYER: return (int64_t)reinterpret_cast<uintptr_t>(gWorld.mPlayers[inB]);
    case EE_UNITS_US_PLAYER_ID: return UsPlayer(inA)->mPlayerID;
    case EE_UNITS_US_PLAYER_RTS: return (int64_t)reinterpret_cast<uintptr_t>(static_cast<RTSPlayer*>(UsPlayer(inA)));
    case EE_UNITS_US_FIRST_UNIT: return (int64_t)reinterpret_cast<uintptr_t>(UsPlayer(inA)->mFirstUnit);
    case EE_UNITS_US_NEXT_UNIT: return (int64_t)reinterpret_cast<uintptr_t>(UsUnit(inA)->mNext);
    case EE_UNITS_US_FILL: Fill(UsUnit(inA), (unsigned long)inB, reinterpret_cast<const void*>(static_cast<uintptr_t>(inC)), *static_cast<EeUnitRecord*>(outData)); return 0;
    case EE_UNITS_US_UNIT_PLAYER: return (int64_t)reinterpret_cast<uintptr_t>(UsUnit(inA)->mObjectKind.mValue >= EE_OBJECT_UNIT ? static_cast<EEUnit*>(UsUnit(inA))->mPlayer : NULL);
    case EE_UNITS_US_CHAIN_FIRST: return (int64_t)reinterpret_cast<uintptr_t>(gWorld.mUnits.mFirst);
    case EE_UNITS_US_CHAIN_NEXT: return (int64_t)reinterpret_cast<uintptr_t>(UsUnit(inA)->mTileNode->mNext);
    case EE_UNITS_US_UNIT_ID: return (uint32_t)UsUnit(inA)->mUnitID;
    case EE_UNITS_US_CHECKSUM: return (uint32_t)static_cast<EEUnit*>(UsUnit(inA))->GetChecksum((unsigned long)inB);
    case EE_UNITS_US_PLAYER_WORDS: PlayerWords(UsPlayer(inA), static_cast<uint32_t*>(outData)); return 0;
    case EE_UNITS_US_ORDER_COUNT: return (int64_t)gWorld.GetNumUpdateUnits();
    case EE_UNITS_US_ORDER_ID: return gWorld.GetUpdateUnitID((size_t)inB);
    case EE_UNITS_US_TICK: return gPhysics != NULL ? (int64_t)gPhysics->mNumTicksDone : 0;
    case EE_UNITS_US_EXIT: exit(3);
    default: return 0;
    }
}

// the box selection's pick over the unit store (ee_units select.rs: the loop of EEPlayer::SelectUnits 0x56a658 over the
// records in place of the objects; the player is its list's object, as the store records it)
size_t EESelectPick(EEPlayer* inPlayer, const uint32_t* inIDs, size_t inCount, uint32_t* outIDs)
{
    return ee_units_select_pick(reinterpret_cast<uintptr_t>(static_cast<RTSPlayer*>(inPlayer)), inIDs, inCount, outIDs);
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
