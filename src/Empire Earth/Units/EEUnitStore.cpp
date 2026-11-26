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

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "rust/sim/app.h"

// the holders' owners from their offsets: the unit classes derive singly (RTSUnit -> EEUnit -> EEComplexUnit), so an
// offsetof is the member's place in every object of the class
#if defined(__clang__)
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#endif

template <class C>
static C* OwnerOf(const void* inMember, size_t inOffset) { return reinterpret_cast<C*>(const_cast<char*>(static_cast<const char*>(inMember)) - inOffset); }

void EEUnitStoreMoved(const void* inModel)
{
    EEComplexUnit* theUnit = OwnerOf<EEComplexUnit>(inModel, offsetof(EEComplexUnit, mPhysicalModel));
    if (theUnit->mInUnitStore) {
        const GETransformation& theLocation = theUnit->mPhysicalModel.GetLocation();
        ee_units_store_set_location(static_cast<uint32_t>(theUnit->mUnitID), reinterpret_cast<uintptr_t>(theUnit),
                                    UFloatBits(theLocation.GetX()), UFloatBits(theLocation.GetY()), UFloatBits(theLocation.GetZ()));
    }
}

// in the world's unit chain (gWorld.mUnits: x-sorted, through mTileNode; RTSUnit::Link puts the previous link, the
// chain's head or a unit, in mPrev, RTSUnit::Unlink clears it)
static bool OnMap(RTSUnit* inUnit) { return inUnit->mTileNode != NULL && inUnit->mTileNode->mPrev != NULL; }

// (EEUnitStoreLinked, Insert, InsertUnlisted, Remove, ClearPlayer, PlayerTable, EEUnitOrderReset and Fill: Rust,
// inline in ee/EEUnitStore.h; ee_sim units us_fill.rs over this file's host)

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
    case EE_UNITS_US_FILL: ee_units_store_fill(static_cast<EeUnitRecord*>(outData), inA, (uint32_t)inB, inC); return 0;
    case EE_UNITS_US_UNIT_PLAYER: return (int64_t)reinterpret_cast<uintptr_t>(UsUnit(inA)->mObjectKind.mValue >= EE_OBJECT_UNIT ? static_cast<EEUnit*>(UsUnit(inA))->mPlayer : NULL);
    case EE_UNITS_US_CHAIN_FIRST: return (int64_t)reinterpret_cast<uintptr_t>(gWorld.mUnits.mFirst);
    case EE_UNITS_US_CHAIN_NEXT: return (int64_t)reinterpret_cast<uintptr_t>(UsUnit(inA)->mTileNode->mNext);
    case EE_UNITS_US_UNIT_ID: return (uint32_t)UsUnit(inA)->mUnitID;
    case EE_UNITS_US_CHECKSUM: return (uint32_t)static_cast<EEUnit*>(UsUnit(inA))->GetChecksum((unsigned long)inB);
    case EE_UNITS_US_ORDER_COUNT: return (int64_t)ee_app_active_list(0, NULL, 0);
    case EE_UNITS_US_ORDER_ID: return ee_app_active_at(0, (size_t)inB);
    case EE_UNITS_US_TICK: return gPhysics != NULL ? (int64_t)gPhysics->mNumTicksDone : 0;
    case EE_UNITS_US_EXIT: exit(3);
    // us_fill.rs's
    case EE_UNITS_US_F_OBJECT_KIND: return UsUnit(inA)->mObjectKind.mValue;
    case EE_UNITS_US_F_ON_MAP: return OnMap(UsUnit(inA));
    case EE_UNITS_US_F_STATS: return (int64_t)reinterpret_cast<uintptr_t>(static_cast<EEUnit*>(UsUnit(inA))->mStats);
    case EE_UNITS_US_F_LOCATION: return UFloatBits(inB == 0 ? static_cast<EEComplexUnit*>(UsUnit(inA))->mPhysicalModel.GetLocation().GetX() : inB == 1 ? static_cast<EEComplexUnit*>(UsUnit(inA))->mPhysicalModel.GetLocation().GetY() : static_cast<EEComplexUnit*>(UsUnit(inA))->mPhysicalModel.GetLocation().GetZ());
    case EE_UNITS_US_F_FIELDS: return inB == 0 ? (int64_t)reinterpret_cast<uintptr_t>(static_cast<const EeUnitsRtsUnitFields*>(UsUnit(inA))) : inB == 1 ? (int64_t)reinterpret_cast<uintptr_t>(static_cast<const EeUnitsUnitFields*>(static_cast<EEUnit*>(UsUnit(inA)))) : (int64_t)reinterpret_cast<uintptr_t>(static_cast<const EeUnitsComplexUnitFields*>(static_cast<EEComplexUnit*>(UsUnit(inA))));
    // the store's entry points (us_fill.rs)
    case EE_UNITS_US_B_IN_STORE: return UsUnit(inA)->mInUnitStore;
    case EE_UNITS_US_B_SET_IN_STORE: UsUnit(inA)->mInUnitStore = inB != 0; return 0;
    case EE_UNITS_US_B_RAW_PLAYER: return (int64_t)reinterpret_cast<uintptr_t>(UsUnit(inA)->mPlayer);
    case EE_UNITS_US_B_ENEMY: return (uint32_t)UsPlayer(inA)->mEnemies[inB];
    default: return 0;
    }
}
