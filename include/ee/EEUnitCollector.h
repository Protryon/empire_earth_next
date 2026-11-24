// EEUnitCollector.h (RECONSTRUCTION; file name inferred). The map unit-region collector template
// shared by EEAIArea.cpp (0xdcbe/0xde7f) and EEHerd.cpp (0xa509a), moved out of EEHerd.cpp. Includers provide gWorld,
// EEMap, RTSRegionTable and EEUnit (ee/EEWorld.h, ee/EEMap.h, ee/RTSRegionTable.h, ee/EEUnit.h).
#ifndef EE_EEUNITCOLLECTOR_H
#define EE_EEUNITCOLLECTOR_H

#include <vector>
#include <functional>
#include <algorithm>
#include <stdint.h>
#include "lle/U2DRectangle.h"
#include "rust/sim/combat.h"             // the map's unit regions (ee_combat regions.rs)

class EEUnit;

// Units by id: the collector's sort-and-unique compares mUnitID (0x23dd96 / 0x23ddab, out of line in
// EEFOFindNewTargetNearOldTarget.cpp, the TU's first functions; they take the pointers by value) (provisional names)
struct EEUUUnitIDLess {
    bool operator()(EEUnit* inFirst, EEUnit* inSecond) const;
};
struct EEUUUnitIDEqual {
    bool operator()(EEUnit* inFirst, EEUnit* inSecond) const;
};

// The sort-and-unique of the collector (cdecl templates 0xe0b9 and 0xe104, emitted in EEAIArea.cpp's range)
// (provisional name)
template <class V, class L, class E>
void EESortUnique(V& ioValues, L inLess, E inEqual)
{
    std::sort(ioValues.begin(), ioValues.end(), inLess);
    ioValues.erase(std::unique(ioValues.begin(), ioValues.end(), inEqual), ioValues.end());
}

// cdecl template 0xa509a (EEAIArea.cpp's 0xdcbe/0xde7f with another predicate): the units on the map's unit regions
// under inRectangle that inPredicate holds for; sorted and made unique when inUnique and a region held a unit of a
// class bigger than one region (+0x28c > 1). Returns their number. Provisional name (shared with EEAIArea.cpp: move to
// a header once both use it)
// The units of the regions under inRectangle (tiles) in the walk's order: column by column, each region from its
// last unit (ee_combat regions.rs walk; RTSRegionTable's inline walk)
inline std::vector<EEComplexUnit*> EEUnitsUnderRegions(const EeCombatUnitRegions* inTable, const U2DRectangle<long>& inRectangle)
{
    std::vector<EEComplexUnit*> theUnits(ee_combat_regions_walk(inTable, (int32_t)inRectangle.mLeft, (int32_t)inRectangle.mTop,
                                                                (int32_t)inRectangle.mRight, (int32_t)inRectangle.mBottom, NULL, 0));
    ee_combat_regions_walk(inTable, (int32_t)inRectangle.mLeft, (int32_t)inRectangle.mTop, (int32_t)inRectangle.mRight,
                           (int32_t)inRectangle.mBottom, (uint64_t*)theUnits.data(), theUnits.size());
    return theUnits;
}

template <class Pred, class V>
long EEGetUnitsInRectangle(const U2DRectangle<long>& inRectangle, V& outUnits, const Pred& inPredicate, bool inUnique)
{
    bool theMultiple = false;
    for (EEComplexUnit* theUnit : EEUnitsUnderRegions(gWorld.mMap->mUnitRegions, inRectangle)) {
        if (inRectangle.Contains(theUnit->GetTileX(), theUnit->GetTileY()) && inPredicate(theUnit)) {
            if (theUnit->GetUnitClass()->GetTileSize() > 1)
                theMultiple = true;
            outUnits.push_back(theUnit);
        }
    }
    if (inUnique && theMultiple)
        EESortUnique(outUnits, EEUUUnitIDLess(), EEUUUnitIDEqual());
    return outUnits.size();
}

#endif
