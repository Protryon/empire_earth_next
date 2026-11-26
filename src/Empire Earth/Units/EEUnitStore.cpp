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
static bool OnMap(RTSUnit* inUnit)
{
    return inUnit->mTileNode != NULL && inUnit->mTileNode->mPrev != NULL;
}

// (EEUnitStoreLinked, Insert, InsertUnlisted, Remove, ClearPlayer, PlayerTable, EEUnitOrderReset and Fill: Rust,
// inline in ee/EEUnitStore.h; ee_sim units us_fill.rs over this file's host)

static RTSUnit* UsUnit(uint64_t inUnit) { return reinterpret_cast<RTSUnit*>(static_cast<uintptr_t>(inUnit)); }

extern "C" int64_t ee_units_us_host(int32_t inOp, uint64_t inA, int64_t inB, uint64_t inC, void* outData)
{
    switch (inOp) {
    case EE_UNITS_US_FILL: ee_units_store_fill(static_cast<EeUnitRecord*>(outData), inA, (uint32_t)inB, inC); return 0;
    case EE_UNITS_US_CHAIN_FIRST: return (int64_t)reinterpret_cast<uintptr_t>(gWorld.mUnits.mFirst);
    case EE_UNITS_US_CHAIN_NEXT: return (int64_t)reinterpret_cast<uintptr_t>(UsUnit(inA)->mTileNode->mNext);
    case EE_UNITS_US_CHECKSUM: return (uint32_t)static_cast<EEUnit*>(UsUnit(inA))->GetChecksum((unsigned long)inB);
    case EE_UNITS_US_ORDER_COUNT: return (int64_t)gWorld.GetNumUpdateUnits();
    case EE_UNITS_US_ORDER_ID: return gWorld.GetUpdateUnitID((size_t)inB);
    case EE_UNITS_US_TICK: return gPhysics != NULL ? (int64_t)gPhysics->mNumTicksDone : 0;
    case EE_UNITS_US_EXIT: exit(3);
    // us_fill.rs's
    case EE_UNITS_US_F_ON_MAP: return OnMap(UsUnit(inA));
    // the store's entry points (us_fill.rs)
    case EE_UNITS_US_B_IN_STORE: return UsUnit(inA)->mInUnitStore;
    case EE_UNITS_US_B_SET_IN_STORE: UsUnit(inA)->mInUnitStore = inB != 0; return 0;
    default: return 0;
    }
}
