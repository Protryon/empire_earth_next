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
#include "lle/U2DPoint.h"           // EETileArea::Extend/GetCenter use U2DPoint<long> (orchestrator fix)
#include "lle/U2DRectangle.h"       // EETileArea, EEAIAStarBounds (exe view: no statics)

#include "ee/EETileCoordinate.h"     // the tile coordinates (moved there)

// cdecl helpers EEMBuildUnit::Execute 0xbbcc4 calls with the message's (long) x/y (provisional names; types
// inferred; units per the map: EEHitPointUpgrader.cpp / EEFOFindNewTargetNearOldTarget.cpp)
bool EECanPlaceOnTile(EEPlayer* inPlayer, long inUnitType, long inX, long inY, bool inSkipPlayerChecks, long inIgnoreOwner);  // 0x24f1f8
EEUnit* EEMakeBuilding(EEPlayer* inPlayer, long inUnitType, long inX, long inY);  // 0x244c83
// cdecl helpers the heal goal calls (provisional names; types inferred)
bool EEIsSeenBy(EEUnit* inUnit, EEPlayer* inPlayer, bool inArg);   // 0x2479eb (EEUGAttackUnit passes its EEUnit* target)
// 0x242542 (EEActiveGroup::SpreadAroundUnit: a unit whose class IsPriest holds and the target's complex unit)
// (provisional name)
bool EECanPriestConvert(EEComplexUnit* inUnit, EEComplexUnit* inTarget);
// 0x243c7d (EEFOFindNewTargetNearOldTarget.cpp's range): whether a unit of type inUnitType fits in the inWidth x inHeight
// rectangle at (inX, inY) for inUnit's player (EECanPlaceOnTile on every tile) (provisional name)
bool EEBlockTakes(EEUnit* inUnit, long inUnitType, long inX, long inY, long inWidth, long inHeight, long inIgnoreOwner);
// 0x20d571 (EEUCBasicPriest.cpp): the faith a conversion of inUnit costs (provisional name)
long EEGetConversionCost(EEComplexUnit* inUnit);
// 0x20da86 (EEUCBasicPriest.cpp; EEActiveGroup::Plan: whether inUnit can convert inTarget now, outFlag: shielded)
// (provisional name)
bool EECanConvert(EEComplexUnit* inUnit, EEComplexUnit* inTarget, bool& outFlag);
bool EECanHeal(EEUnit* inUnit, EEComplexUnit* inTarget);    // 0x246bf2
// the build/repair goal's helpers (provisional names; types inferred)
EEComplexUnit* EEGetAttackPart(EEComplexUnit* inUnit, EEComplexUnit* inTarget);  // 0x246f93
bool EEIsInWorkReach(EEComplexUnit* inUnit, EEComplexUnit* inTarget);
// 0x24cbd7 (cdecl): the tile next to inTile in direction inDirection (1..8) (provisional name)
void EEGetSlotTile(const EETileCoordinate& inTile, long inDirection, EETileCoordinate& outTile);  // 0x24734c (can inUnit work on inTarget)
bool EEFindNextFreeTile(EEComplexUnit* inUnit, EEComplexUnit* inTarget, GE3DPoint& outLocation);  // 0x24899b
bool EEFindResourceSpot(EEComplexUnit* inUnit, EEComplexUnit* inTarget, GE3DPoint& outLocation);  // 0x248709 (EEUGGather::GetDestination with the resource; provisional name)
// 0x24b8ac (cdecl; a path distance from inUnit to ioTile, 10000 when none: EEUGGather::EvaluateCondition 0x1f4b; the
// body writes ioTile; the definition's types, one prototype) (provisional name)
float EEGetPathLengthTo(EEComplexUnit* inUnit, EETileCoordinate& ioTile, unsigned long inLimit, bool inWalls);
// 0x247772: the cast-calamity goal's in-range test at range 0 (inDistanceSquared from inUnit to inTarget)
bool EEIsInContactRange(EEComplexUnit* inUnit, EEUnit* inTarget, float inDistanceSquared);
// 0x245903 / 0x245a19 / 0x23eabc (cdecl): EEUGGather's 0xfbd handler (the units around inUnit, the first free one
// among theirs, then send inUnit there) (provisional names; types inferred)
bool EEGetFarmDropSites(EEComplexUnit* inUnit, std::vector<EEUnit*>& outUnits);
EEComplexUnit* EEFindFreeFarm(EEComplexUnit* inUnit, bool inFreeOnly, long* ioCount);  // bool: `cmp byte [ebp+0xc]`
void EEWorkFarm(EEComplexUnit* inTarget, EEComplexUnit* inUnit);
// EEUGAttackUnit.cpp's cdecl callees (provisional names; types inferred; the first is named by its assert)
bool EEUUGetIsUnitClassTargetable(EEUnit* inAttacker, EEUnitClass* inClass);   // 0x247848
void EEGetLeadPosition(EEComplexUnit* inAttacker, EEUnit* inTarget, GE3DPoint& outLocation);  // 0x24181a (moving targets)
void EEGetFiringPosition(EEComplexUnit* inAttacker, EEUnit* inTarget, GE3DPoint& outLocation);  // 0x241e49
bool EEIsAttackReachable(EEComplexUnit* inUnit, EEComplexUnit* inTarget, U2DPoint<long>* outTile);  // 0x242575
// 0x23f32a (cdecl; EECPAttackSubGroupUnit::ChooseTarget's: a tile to attack inTarget) (provisional name)
bool EEHasPathToTarget(EEComplexUnit* inUnit, EEComplexUnit* inTarget, U2DPoint<long>* outTile, bool inUseClosestTile, long inMaxSteps);  // bool: `cmp byte [ebp+0x14]`
// 0x24680c / 0x24240d (cdecl; EECPAttackSubGroupUnit::Flee's: a search window around inUnit; the heading from
// inFrom to inTo) (provisional name)
template <class T> class U2DRectangle;
typedef U2DRectangle<long> EEAIAStarBounds;   // ee/EEAIAStarStorage.h
bool EEGetAreaEffectBounds(long inArg1, EEComplexUnit* inUnit, EEAIAStarBounds* outBounds);
float EEGetHeadingBetween(GE3DPoint* inFrom, GE3DPoint* inTo);
bool EEIsInRangeToHit(EEComplexUnit* inUnit, EEUnit* inTarget);  // 0x24722d
bool EEIsLineToUnitBlocked(EEComplexUnit* inUnit, EEUnit* inTarget, EEUnit** outArg3, long* outArg4, long* outArg5);  // 0x24df23
bool EEIsEnemyWithinRange(EEComplexUnit* inUnit);  // 0x2454eb
// EEUGGather's helpers (cdecl; signatures from their call sites in EEUGGather.cpp) (provisional name)
bool EEIsWithinReach(EEComplexUnit* inUnit, EEUnit* inTarget);  // 0x24745d (inUnit at inTarget: the gather goal's arrival)
bool EEIsWithinReachOf(const GE3DPoint& inLocation, EEComplexUnit* inUnit, EEUnit* inTarget);  // 0x2475a0
void EEGetResourceSpots(EEUnit* inUnit, std::vector<U2DPoint<long> >& outTiles);  // 0x24c6fc (the tiles around inUnit)
// 0x8282 (cdecl, hidden result): a tile picked from inTiles by inArg (EECPAttackSubGroup::Virtual28) (provisional name)
inline U2DPoint<long> EEComputeTileCenterPoint(std::vector<EETileCoordinate>& inTiles, long inMustBeInList)
    { return U2DComputeCenterPoint(inTiles, (unsigned char)inMustBeInList != 0); }
class GERectangle;
// 0x241743 (cdecl): the bounds of inUnits into outBounds (EECPAttackSubGroup::GetUnitsCenter) (provisional name)
void EEGetSelectionBounds(std::vector<EEUnit*>& inUnits, GERectangle& outBounds, long inTerrainFilter);
// 0x246a26 / 0x24720e: the tiles of (and around) a footprint of inSize at (inX, inY) (EEUGGather::HasGatherRoom)
void EEGetSideTiles(long inX, long inY, long inSize, std::vector<U2DPoint<long> >& outTiles);
// 0x24cc5f: the gather-rate index of inResource (EEComplexUnit mStats->mL110[]; EEUGGather) (provisional name)
long EEGetResourceIndex(EEUnit* inResource);
void EEGetCornerTiles(long inCorners, long inX, long inY, long inSize, std::vector<U2DPoint<long> >& outTiles);
bool EEFindPlaceOnTile(EEComplexUnit* inUnit, EEUnitClass* inClass, long inX, long inY, long inSize, long inToX, long inToY,
                long inSquare, GE3DPoint* outLocation);  // 0x240ddb
long EEGetAttackFrameSet(EEComplexUnit* inUnit);
bool EEIsCloakedEnemyOf(EEUnit* inUnit, EEPlayer* inPlayer);  // 0x2478f7 (EEUnit::Fn229D73 with the current player; provisional name) // 0x23c9d9 (EEWheel.cpp: the unit's attack frame set, 0/8/0xb)

// 0x24b556: every tile of the square of inRadius around (inX, inY) (one less on the far side unless inInclusive)
void EEGetTilesInRadius(long inX, long inY, unsigned long inRadius, bool inInclusive,
                        std::vector<EETileCoordinate>& outTiles);
// 0x24c395: inPlayer's units of inUnitType (EEUnitClass::mUnitType); returns their number
long EEGetPlayerUnitsOfType(long inUnitType, long inPlayer, std::vector<EEUnit*>& outUnits);
// 0x24c22a: the units in inRadius of inCenter
long EEGetUnitsInRadius(GE2DPoint inCenter, float inRadius, std::vector<EEUnit*>& outUnits);
// 0x24bf01: the units within inRadius of inCenter; their number (the hurricane) (provisional name)
long EEGetUnitsNear(const GE3DPoint& inCenter, float inRadius, std::vector<EEUnit*>& outUnits);
// 0x2467bb / 0x248d05 (cdecl; the hurricane compares the first with the square of the second plus its radius)
// Provisional names
float EEGetDistanceSquared(EEComplexUnit* inUnit, EEComplexUnit* inOther);
float EEGetUnitRadius(EEComplexUnit* inUnit, EEComplexUnit* inOther);

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

// The volcano's units and tiles (cdecl; provisional names; argument types inferred)
// 0x24c146: the units inside inRectangle
long EEGetUnitsInRectangle(const GERectangle& inRectangle, std::vector<EEUnit*>& outUnits);
// 0x23f630: whether inUnit can be moved (the volcano passes 0 first)
bool EEIsUnitMovable(EEComplexUnit* inPlayer, EEComplexUnit* inUnit);
// 0x246bd0: the tiles around the inSize square at (inX, inY)
void EEGetSquareBlockRing(long inCorners, long inX, long inY, long inSize, std::vector<EETileCoordinate>& outTiles, bool inAppend);  // bool: EEUnitClass::Virtual44 0x234965 passes its bool through as a dword
// 0x241658: whether inUnit can stand on inTile
bool EECanStandOn(const EETileCoordinate& inTile, EEComplexUnit* inUnit);
// 0x24f86d: whether inPlayer may place a unit of class inUnitClass on inTile
// 0x246254: a place for a computer player's unit to move off to (EEUnitClass::VirtualC8) (provisional name)
bool EEGetEscapePoint(EEComplexUnit* inUnit, GE2DPoint& outLocation);
bool EEIsTileUnblocked(EEPlayer* inPlayer, long inUnitClass, EETile* inTile);
// 0x24edf6 (the volcano's Virtual30)
void EEStepAsideFromSite(EEUnit* inUnit, EEUnitClass* inClass, EEPlayer* inPlayer, EETileCoordinate& inTile, bool inForce);

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
    long DistanceSquared(const EETileCoordinate& inTile) const;   // 0x8a3f
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
    // 0x66b6d (EECPBuildManager.cpp; built without /Og): (continent, town) pairs by the distance of the continent's
    // centre (EEAIArea +0x44: its hull's centre) (provisional name)
    bool operator()(const std::pair<EEAIContinent*, EETown*>& inFirst,
        const std::pair<EEAIContinent*, EETown*>& inSecond) const;
    // 0x66be7 (EECPBuildManager.cpp; built without /Og): choke points by the distance of their centres (EEAIArea +0x44)
    // (provisional name)
    bool operator()(EEAIChokePoint* inFirst, EEAIChokePoint* inSecond) const;

    EETileCoordinate mCenter;
};

#endif
