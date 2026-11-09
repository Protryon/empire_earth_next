// EEUnitCollector.h (RECONSTRUCTION; file name inferred). The map unit-region collector template
// shared by EEAIArea.cpp (0xdcbe/0xde7f) and EEHerd.cpp (0xa509a), moved out of EEHerd.cpp. Includers provide gWorld,
// EEMap, RTSRegionTable and EEUnit (ee/EEWorld.h, ee/EEMap.h, ee/RTSRegionTable.h, ee/EEUnit.h).
#ifndef EE_EEUNITCOLLECTOR_H
#define EE_EEUNITCOLLECTOR_H

#include <vector>
#include <functional>
#include <algorithm>

class EEUnit;

// Units by id: the collector's sort-and-unique compares mUnitID (0x23dd96 / 0x23ddab, out of line in
// EEFOFindNewTargetNearOldTarget.cpp, the TU's first functions; they take the pointers by value) (provisional names)
struct EEUUUnitIDLess {
    bool operator()(EEUnit* inFirst, EEUnit* inSecond) const;
};
struct EEUUUnitIDEqual {
    bool operator()(EEUnit* inFirst, EEUnit* inSecond) const;
};

// cdecl template 0xe104 (EEAIArea.cpp's range): ioValues sorted, the end of its unique run (provisional name)
template <class V, class L, class E>
typename V::iterator EESortedUnique(V& ioValues, L inLess, E inEqual)
{
    std::sort(ioValues.begin(), ioValues.end(), inLess);
    typename V::iterator theEnd;
    theEnd = std::unique(ioValues.begin(), ioValues.end(), inEqual);
    return theEnd;
}

// The sort-and-unique of the collector (cdecl template 0xe0b9, emitted in EEAIArea.cpp's range) (provisional name)
template <class V, class L, class E>
void EESortUnique(V& ioValues, L inLess, E inEqual)
{
    typename V::iterator theEnd = EESortedUnique(ioValues, inLess, inEqual);
    ioValues.erase(theEnd, ioValues.end());
}

// cdecl template 0xa509a (EEAIArea.cpp's 0xdcbe/0xde7f with another predicate): the units on the map's unit regions
// under inRectangle that inPredicate holds for; sorted and made unique when inUnique and a region held a unit of a
// class bigger than one region (+0x28c > 1). Returns their number. Provisional name (shared with EEAIArea.cpp: move to
// a header once both use it)
template <class Pred, class V>
long EEGetUnitsInRectangle(const U2DRectangle<long>& inRectangle, V& outUnits, const Pred& inPredicate, bool inUnique)
{
    U2DRectangle<long> theRegions = gWorld.mMap->mUnitRegions->GetRegions(inRectangle);
    RTSRegionTable<EEComplexUnit*>* theTable = gWorld.mMap->mUnitRegions;
    U2DPoint<long> theEndX = theRegions.GetBottomRight();
    U2DPoint<long> theEndY = theRegions.GetBottomRight();
    bool theMultiple = false;
    U2DPoint<long> theStart = theRegions.GetTopLeft();
    for (long x = theStart.mX; x <= theEndX.mX; x++) {
        U2DPoint<long> theRowStart = theRegions.GetTopLeft();
        for (long y = theRowStart.mY; y <= theEndY.mY; y++) {
            RTSRegion<EEComplexUnit*>* theRegion = theTable->GetRegion(x, y);
            if (theRegion) {
                for (long i = theRegion->GetCount() - 1; i >= 0; i--) {
                    EEComplexUnit* theUnit = theRegion->GetAt(i);
                    if (inRectangle.Contains(theUnit->GetTileX(), theUnit->GetTileY()) &&
                        inPredicate(theUnit)) {
                        if (theUnit->GetUnitClass()->GetTileSize() > 1)
                            theMultiple = true;
                        outUnits.push_back(theUnit);
                    }
                }
            }
        }
    }
    if (inUnique && theMultiple)
        EESortUnique(outUnits, EEUUUnitIDLess(), EEUUUnitIDEqual());
    return outUnits.size();
}

#endif
