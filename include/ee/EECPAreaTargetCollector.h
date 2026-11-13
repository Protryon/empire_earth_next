// Original ASG area collector; generic definition moved unchanged from Helicopter.
// Requires complete EEWorld/EEMap/EEUnit, RTSRegionTable and EEAIAStarBounds declarations.
#ifndef EE_EECPAREATARGETCOLLECTOR_H
#define EE_EECPAREATARGETCOLLECTOR_H
#include <algorithm>
#include <vector>
#include "lle/U2DRectangle.h"
#include "ee/EEUnitCollector.h"         // EESortUnique (0xe0b9)

// The same algorithm as ee/EEUnitCollector.h's EEGetUnitsInRectangle (EEAIArea 0xdcbe/0xde7f): corner points of the
// region rectangle, the row start rebuilt per column, EESortUnique 0xe0b9 at the end (from 0x44cffc)
template <class Predicate>
long EECollectASGTargets(const EEAIAStarBounds& inBounds, std::vector<EEComplexUnit*>& outUnits,
                        Predicate& inPredicate, bool inUnique)
{
    EeCombatUnitRegions* theTable = gWorld.mMap->mUnitRegions;
    U2DRectangle<long> theRegions = EEUnitRegionRect(theTable, inBounds);
    U2DPoint<long> theEndX = theRegions.GetBottomRight();
    U2DPoint<long> theEndY = theRegions.GetBottomRight();
    bool theMultiple = false;
    U2DPoint<long> theStart = theRegions.GetTopLeft();
    for (long x = theStart.GetX(); x <= theEndX.GetX(); x++) {
        U2DPoint<long> theRowStart = theRegions.GetTopLeft();
        for (long y = theRowStart.mY; y <= theEndY.mY; y++) {
            EEUnitRegion theRegion(theTable, x, y);
            if (theRegion.Exists()) {
                for (long i = theRegion.size() - 1; i >= 0; i--) {
                    EEComplexUnit* theUnit = theRegion[i];
                    if (inBounds.Contains(theUnit->GetTileX(), theUnit->GetTileY()) &&
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
        EESortUnique(outUnits, EEUUUnitIDLess(), EEUUUnitIDEqual());   // by unit id (0x43f27), not pointer order
    return outUnits.size();
}

#endif
