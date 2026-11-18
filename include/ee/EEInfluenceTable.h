// EEInfluenceTable.h (RECONSTRUCTION; file name inferred). Per map region (9x9 tiles) and player, the
// summed +0x244 value (EEUnitClass::mInfluence) of the units there, and the total (the original: RTTI EEInfluenceTable >
// RTSRegionTable<std::pair<unsigned long, long> >, vtable 0x43c41c; EEInfluenceTable.cpp 0xa5589..0xa5c31).
// port (lane B of the AI Thread): the table is Rust's (ee_sim::cp::influence: its values and every body); this class
// is its shim, keeping mRegionSize for the readers that divide tile coordinates by it. EEWorld::CreateInfluenceTable
// news it with (the map's size, mNumPlayers). Requires lle/U2DRectangle.h.
#ifndef EE_EEINFLUENCETABLE_H
#define EE_EEINFLUENCETABLE_H

#include <stdint.h>
#include "rust/sim/cp.h"

class EEUnit;
class EEWorld;
extern EEWorld gWorld;

class EEInfluenceTable {
public:
    // regions per side: the ceiling of inMapSize / 9; every entry 0
    EEInfluenceTable(unsigned long inMapSize, unsigned long inNumPlayers)
        : mTable(ee_cp_influence_new((uint32_t)inMapSize, 9, (uint32_t)inNumPlayers)), mRegionSize(9),
          mNumPlayers(inNumPlayers) {}
    ~EEInfluenceTable() { ee_cp_influence_free(this->mTable); }
    // 0xa56f6: the sum of GetOthersValue, the start corner clipped at 0
    long SumOthersValue(unsigned long inPlayer, const U2DRectangle<long>& inRect)
        { return ee_cp_influence_sum(this->mTable, 1, (uint32_t)inPlayer, Rect(inRect)); }
    // 0x24495a (EEFOFindNewTargetNearOldTarget.cpp): SumOthersValue over the regions under inTiles
    long SumOthersValueOnTiles(unsigned long inPlayer, const U2DRectangle<long>& inTiles);
    // 0xa5839 / 0xa58a7: the unit's class influence added to (or, clamped at 0, taken from) its owner's and the
    // total entry of the region of (inX, inY)
    void AddUnit(EEUnit* inUnit, long inX, long inY)
        { ee_cp_influence_unit(this->mTable, true, (uint64_t)(uintptr_t)inUnit, (int32_t)inX, (int32_t)inY); }
    void RemoveUnit(EEUnit* inUnit, long inX, long inY)
        { ee_cp_influence_unit(this->mTable, false, (uint64_t)(uintptr_t)inUnit, (int32_t)inX, (int32_t)inY); }
    // 0xa598d: over a rectangle of regions, its start corner clipped at 0
    long SumEnemyValue(unsigned long inPlayer, const U2DRectangle<long>& inRect, bool inWithFirst)
        { return ee_cp_influence_enemies(this->mTable, (uint64_t)(uintptr_t)&gWorld, (uint32_t)inPlayer, Rect(inRect), inWithFirst, true); }
    // inPlayer's value in region (inX, inY): 0 off the grid and for a player past the entries (the route planner
    // passes -1 for a task without a player)
    long GetValue(unsigned long inPlayer, unsigned long inX, unsigned long inY)
        { return ee_cp_influence_value(this->mTable, 0, (uint32_t)inPlayer, (uint32_t)inX, (uint32_t)inY); }
    long GetValue(unsigned long inPlayer, const U2DPoint<long>& inRegion)
        { return this->GetValue(inPlayer, (unsigned long)inRegion.mX, (unsigned long)inRegion.mY); }
    // inline (COMDAT 0xe06f, the original RTSRegionTable's): inTiles' edges in regions
    U2DRectangle<long> GetRegions(const U2DRectangle<long>& inTiles)
    {
        U2DRectangle<long> theRegions;
        theRegions.mBottom = inTiles.mBottom / this->mRegionSize;
        theRegions.mLeft = inTiles.mLeft / this->mRegionSize;
        theRegions.mRight = inTiles.mRight / this->mRegionSize;
        theRegions.mTop = inTiles.mTop / this->mRegionSize;
        return theRegions;
    }

    EeCpInfluence* mTable;                  // the Rust table
    long mRegionSize;                       // tiles per region side (the callers divide tile coordinates by it)
    unsigned long mNumPlayers;              // (ctor argument 2)

private:
    static EeCpRect Rect(const U2DRectangle<long>& inRect)
    {
        EeCpRect theRect = {(int32_t)inRect.mLeft, (int32_t)inRect.mTop, (int32_t)inRect.mRight, (int32_t)inRect.mBottom};
        return theRect;
    }
    EEInfluenceTable(const EEInfluenceTable&);
    EEInfluenceTable& operator=(const EEInfluenceTable&);
};

#endif
