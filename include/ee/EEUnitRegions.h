// New (port; docs/RUST_PLAN.md R5b.2, the fifteenth round): the map's unit regions are a Rust table
// (rust/crates/ee_combat regions.rs; was RTSRegionTable<EEComplexUnit*>, EEMap::mUnitRegions). Here the C++ callers'
// view of one region: its units as the table's slice, valid until the table next changes (as the original's vector
// was), and the sorted collection of an area's units.
#ifndef EE_EEUNITREGIONS_H
#define EE_EEUNITREGIONS_H

#include <stdint.h>
#include <vector>

#include "lle/U2DRectangle.h"
#include "rust/ee_combat.h"

class EEComplexUnit;

// region (inX, inY) of inTable (the coordinates unsigned, as the original's GetRegion): Exists() false off the grid
struct EEUnitRegion {
    const uint64_t* mUnits;
    int32_t mCount;
    EEUnitRegion(const EeCombatUnitRegions* inTable, long inX, long inY)
        : mUnits(ee_combat_regions_units(inTable, (uint32_t)inX, (uint32_t)inY, &mCount)) {}
    bool Exists() const { return this->mUnits != NULL; }
    long size() const { return this->mCount; }
    EEComplexUnit* operator[](long inIndex) const { return (EEComplexUnit*)(uintptr_t)this->mUnits[inIndex]; }
};

// RTSRegionTable::GetRegions (inline 0xe06f): inTiles' edges in regions
inline U2DRectangle<long> EEUnitRegionRect(const EeCombatUnitRegions* inTable, const U2DRectangle<long>& inTiles)
{
    long theSize = ee_combat_regions_size(inTable);
    U2DRectangle<long> theRegions;
    theRegions.mBottom = inTiles.mBottom / theSize;
    theRegions.mLeft = inTiles.mLeft / theSize;
    theRegions.mRight = inTiles.mRight / theSize;
    theRegions.mTop = inTiles.mTop / theSize;
    return theRegions;
}

// RTSRegionTable::CollectValues (0x6bf74): the units of the regions under inTiles, sorted by pointer, no duplicates
inline void EECollectRegionUnits(const EeCombatUnitRegions* inTable, const U2DRectangle<long>& inTiles,
                                 std::vector<EEComplexUnit*>& outUnits)
{
    outUnits.resize(ee_combat_regions_collect(inTable, (int32_t)inTiles.mLeft, (int32_t)inTiles.mTop,
                                              (int32_t)inTiles.mRight, (int32_t)inTiles.mBottom, NULL, 0));
    ee_combat_regions_collect(inTable, (int32_t)inTiles.mLeft, (int32_t)inTiles.mTop, (int32_t)inTiles.mRight,
                              (int32_t)inTiles.mBottom, (uint64_t*)outUnits.data(), outUnits.size());
}

#endif
