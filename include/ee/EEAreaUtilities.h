// EEAreaUtilities.h (RECONSTRUCTION; file name inferred; provisional names). Free functions of the Units group that
// collect map tiles and units around a point (0x24b556, 0x24c22a; cdecl). Requires lle/GE2DPoint.h, <vector>.
#ifndef EE_EEAREAUTILITIES_H
#define EE_EEAREAUTILITIES_H

class EEComplexUnit;
class EEPlayer;
class GE2DPoint;
class GE3DPoint;
class EEUnit;
class EEUnitClass;
#include <algorithm>                  // U2DComputeCenterPoint (EEComputeTileCenterPoint) uses std::find
#include "lle/U2DRectangle.h"       // EETileArea, EEAIAStarBounds (exe view: no statics)

#include "ee/EETileCoordinate.h"     // the tile coordinates (moved there)

// cdecl helpers EEMBuildUnit::Execute 0xbbcc4 calls with the message's (long) x/y (provisional names; types
// inferred; units per the map: EEHitPointUpgrader.cpp / EEFOFindNewTargetNearOldTarget.cpp)
EEUnit* EEMakeBuilding(EEPlayer* inPlayer, long inUnitType, long inX, long inY);
// cdecl helpers the heal goal calls (provisional names; types inferred)
// 0x242542 (EEActiveGroup::SpreadAroundUnit: a unit whose class IsPriest holds and the target's complex unit)
// (provisional name)
bool EECanPriestConvert(EEComplexUnit* inUnit, EEComplexUnit* inTarget);
// 0x20d571 (EEUCBasicPriest.cpp): the faith a conversion of inUnit costs (provisional name)
long EEGetConversionCost(EEComplexUnit* inUnit);
// 0x20da86 (EEUCBasicPriest.cpp; EEActiveGroup::Plan: whether inUnit can convert inTarget now, outFlag: shielded)
// (provisional name)
bool EECanConvert(EEComplexUnit* inUnit, EEComplexUnit* inTarget, bool& outFlag);
bool EECanHeal(EEUnit* inUnit, EEComplexUnit* inTarget);
bool EEIsInWorkReach(EEComplexUnit* inUnit, EEComplexUnit* inTarget);
// 0x247772: the cast-calamity goal's in-range test at range 0 (inDistanceSquared from inUnit to inTarget)
bool EEIsInContactRange(EEComplexUnit* inUnit, EEUnit* inTarget, float inDistanceSquared);
// 0x245903 / 0x245a19 / 0x23eabc (cdecl): EEUGGather's 0xfbd handler (the units around inUnit, the first free one
// among theirs, then send inUnit there) (provisional names; types inferred)
EEComplexUnit* EEFindFreeFarm(EEComplexUnit* inUnit, bool inFreeOnly, long* ioCount);  // bool: `cmp byte [ebp+0xc]`
void EEWorkFarm(EEComplexUnit* inTarget, EEComplexUnit* inUnit);
// EEUGAttackUnit.cpp's cdecl callees (provisional names; types inferred; the first is named by its assert)
bool EEIsAttackReachable(EEComplexUnit* inUnit, EEComplexUnit* inTarget, U2DPoint<long>* outTile);
// 0x24680c / 0x24240d (cdecl; EECPAttackSubGroupUnit::Flee's: a search window around inUnit; the heading from
// inFrom to inTo) (provisional name)
template <class T> class U2DRectangle;
typedef U2DRectangle<long> EEAIAStarBounds;   // ee/EEAIAStarStorage.h
bool EEIsLineToUnitBlocked(EEComplexUnit* inUnit, EEUnit* inTarget, EEUnit** outArg3, long* outArg4, long* outArg5);
bool EEIsEnemyWithinRange(EEComplexUnit* inUnit);
class GERectangle;
long EEGetAttackFrameSet(EEComplexUnit* inUnit);

class EEComplexUnit;
class FSFile;

// A rectangle of tiles: U2DRectangle<long> (the same layout and ctor store order; EECPAttackSubGroup::FindPosition
// calls its Extend 0x123d1 and GetCenter 0x61340 out of line, its Load 0x12e72 / Save 0x12ec4 are U2DRectangle<long>'s,
// EECPBuildManager::FindTowerSite grows one with Grow 0x19f9d) (typedef; provisional name)
typedef U2DRectangle<long> EETileArea;

// The unit collector's predicate that takes every unit (1 byte; the callers pass a local's address and the template's
// test compiles away) and the collector itself (cdecl template, defined in ee/EEUnitCollector.h; EEContagionHelper.cpp
// only declares it: its $E numbering has none of the map headers). Instances: 0x250d71 (EEGetUnitsInRadius;
// EEContagionHelper::GetUnitsInArea and EECPBuildManager call the EEComplexUnit* copy, ICF-folded) (provisional names)
struct EEAnyUnit {
    bool operator()(EEComplexUnit* inUnit) const { return true; }
};
template <class Pred, class V>
long EEGetUnitsInRectangle(const U2DRectangle<long>& inRectangle, V& outUnits, const Pred& inPredicate, bool inUnique);

class GERectangle;
class EEPlayer;
class EEUnitClass;
class EETile;

// Orders tiles by their distance from mCenter (the volcano sorts the free tiles around its square with it; its
// operator() is expanded in the sort COMDATs 0xb538d.., DistanceSquared is out of line at 0x8a3f, an AI unit's
// range). Provisional name; layout inferred (8 bytes, passed by value).
struct RTSMapTileRun;
class EEAIContinent;
class EEAIChokePoint;
class EETown;
#include <utility>   // std::pair (a forward declaration names another class under libc++'s std::__1)
template <class T> class U2DPoint;
struct EETileDistanceLess {
    EETileDistanceLess(long inX, long inY) : mCenter(inX, inY) {}
    // user-declared: the sort COMDATs copy the predicate field by field (0xb538d)
    EETileDistanceLess(const EETileDistanceLess& inOther) : mCenter(inOther.mCenter) {}
    // user-declared: without it VC6 never inlines a temporary's ctor built into a by-value argument slot (EECCVolcano
    // ClearArea 0xb4899, EEMBuildUnit::Execute 0xbbe73 build it in place at esp)
    ~EETileDistanceLess() {}
    EETileDistanceLess(const EETileCoordinate& inCenter) : mCenter(inCenter) {}
    long DistanceSquared(const EETileCoordinate& inTile) const;
    long DistanceSquared(EEUnit* inUnit) const;   // inline: the unit's tile (0x21e104 expands it twice) (provisional name)
    bool operator()(const EETileCoordinate& inFirst, const EETileCoordinate& inSecond) const
        { return this->DistanceSquared(inFirst) < this->DistanceSquared(inSecond); }
    // 0x21e104 (EEUGBuildRepair.cpp; built without /Og): the units by their tiles, the farther first (provisional name)
    bool operator()(EEUnit* inFirst, EEUnit* inSecond) const;
    // 0xc995c (an inline COMDAT among EEMessage.cpp's sort COMDATs, after its first user _Insertion_sort_1 0xc98ab):
    // units with +0x34c set first, then +0x344, then the nearer; it builds the second unit's tile through the
    // 2-reference ctor 0xaa817. Body not written (provisional name)
    bool operator()(EEComplexUnit* inFirst, EEComplexUnit* inSecond) const;
    // 0x9cad8 (EECPWallManager.cpp; built without /Og): two runs by the distance of their middle tiles (provisional
    // name)
    bool operator()(const RTSMapTileRun& inFirst, const RTSMapTileRun& inSecond) const;

    EETileCoordinate mCenter;
};

#endif
