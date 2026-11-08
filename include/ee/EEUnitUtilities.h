// EEUnitUtilities.h (RECONSTRUCTION; file name inferred; every name provisional). The cdecl unit utilities
// of 0x241c2d..0x24f0e9 (EEFOFindNewTargetNearOldTarget.cpp per the unit map; probably one "EEUnitUtilities.cpp"
// with EEUU* names that opens at 0x23dd54) that no other header declares yet. The ones other units call are declared
// in ee/EEAreaUtilities.h, ee/EEUnit.h and the callers' headers; their signatures are kept.
// Requires lle/LowLevelEngine.h, lle/GE3DPoint.h, <vector>, ee/EEAreaUtilities.h.
#ifndef EE_EEUNITUTILITIES_H
#define EE_EEUNITUTILITIES_H
#include "ee/EECivUpgrade.h"

class EEComplexUnit;
class EEUnit;
class EEUnitClass;
class EEPlayer;
class EEUnitGoal;
class EEUUUnitFunctor;
class EEUGAttackUnit;
class EEFOFindNewTargetScreenBase;
class EEUnitClassUpgrade;
class GE2DPoint;
// Actual global23cd32 definition in EEWheel.cpp; no new helper body or credit.
bool EEGetRandomPointAhead(EEComplexUnit* inUnit, GE2DPoint* outPoint);
template <class T> class U2DRectangle;
template <class T> class U2DPoint;

// Orders tiles by their squared distance from mUnit (std::sort 0x24fa5e from 0x241e49; its operator() 0x23ddc1 is
// out of line in the range before 0x241c2d, built without /Og) (provisional name)
struct EEUUTileNearerToUnit {
    EEUUTileNearerToUnit(EEComplexUnit* inUnit) : mUnit(inUnit) {}
    bool operator()(const U2DPoint<long>& inFirst, const U2DPoint<long>& inSecond) const;   // 0x23ddc1
    EEComplexUnit* mUnit;
};

// 0x241bc9 (before the range): inUnit's attack goal moves to inTarget (provisional name)
void EERetargetAttack(EEComplexUnit* inUnit, EEUnitGoal* inGoal, EEComplexUnit* inTarget);
// 0x241299 (before the range): the tiles from which a unit of inSize at (inX, inY) is within inRange of inFrom
void EEGetSortedRingTiles(const GE3DPoint& inFrom, long inX, long inY, long inSize, long inRange, long inCorners,
                std::vector<U2DPoint<long> >& outTiles);
// 0x241c2d: inUnit looks for a new target near inTarget (EEFOFindNewTargetNearOldTarget, 25 squared tiles)
void EEFindNewTargetNearOld(EEComplexUnit* inUnit, EEComplexUnit* inTarget);
// 0x245dd6: the nearest unit around inTile that passes inScreen; inUnit's goal attacks it (false: none)
bool EEFindNewTarget(EEFOFindNewTargetScreenBase* inScreen, EEUnitGoal* inGoal, EEComplexUnit* inUnit,
                EETileCoordinate* inTile, long inRadius, bool inArg6, long inArg7);
bool EEGetWallSpot(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& outLocation);
bool EECanBallisticHit(EEComplexUnit* inUnit, EEUnit* inTarget);
// 0x241504 with the tiles as U2DPoint<long> (0x241e49; ee/EEUnit.h declares it over EETileCoordinate) (provisional
// name)
bool EEPickTileNear(std::vector<U2DPoint<long> >& inTiles, EEComplexUnit* inUnit, EEUnit* inTarget, bool inArg4,
                GE3DPoint& outLocation);

// 0x24286a and its two wrappers 0x24282e (true) / 0x24284c (false) inferred argument meanings
bool EEGetDockShipPlacement(GE3DPoint* inPoint, EEUnitClass* inClass, EEUnit* inUnit, bool inFacePoint, bool inArg5,
                GETransformation* outTransformation);
bool EEGetDockShipFacing(GE3DPoint* inPoint, EEUnitClass* inClass, EEUnit* inUnit, bool inFacePoint, GETransformation* outTransformation);
bool EEFindDockShipPlacement(GE3DPoint* inPoint, EEUnitClass* inClass, EEUnit* inUnit, bool inFacePoint, GETransformation* outTransformation);
// 0x20da86: EECanConvert (ee/EEAreaUtilities.h)
float EEGetAngleFromNorth(GE3DPoint* inFrom, GE3DPoint* inTo);
float EEGetSeaHeading(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo);
float EEGetUnitAngle(EEUnit* inFrom, EEUnit* inTo);
bool EEIsAttackedBy(EEComplexUnit* inUnit, EEUnit* inTarget);
// 0x243561 EEFindPlacement: declared in ee/EEUnit.h; the wrapper 0x24352f (no unit, inArg9 false) (provisional
// name)
bool EEFindPlacement(GE3DPoint* inPoint, EEUnitClass* inClass, unsigned long inX, unsigned long inY, long inSize,
                     long inRings, bool inCorners, bool inFacePoint, bool inArg11, GETransformation* outTransformation,
                     EEComplexUnit* inTarget);
// 0x24bb36: the tiles of ring inRing around the inSize square at (inX, inY)
void EEGetRingTiles(long inX, long inY, long inSize, long inRing, long inCorners,
                std::vector<U2DPoint<long> >& outTiles);
float EEGetFootprintDistance(EEComplexUnit* inUnit, long inX, long inY, long* outX, long* outY);  // 0x246ea2 (names.txt binds it)
char EEGetTileUnitStanding(EEUnit* inUnit, EEPlayer* inPlayer, long inUnitType, bool inIgnoreOwner);
// EEBlockTakes 0x243c7d: ee/EEAreaUtilities.h
bool EEPlayerBlockTakes(EEPlayer* inPlayer, long inUnitType, long inX, long inY, long inWidth, long inHeight, long inIgnoreOwner);
EEUnit* EESearchTarget(EEComplexUnit* inUnit, U2DRectangle<long>* inArea, long inX, long inY, float inScale);
EEUnit* EEFindNearbyTarget(EEComplexUnit* inUnit);
EEUnit* EESearchTargetAlongList(EEComplexUnit* inUnit, long inX, long inY, float inScale);
EEUnit* EEFindTargetNear(EEComplexUnit* inUnit, long inX, long inY, float inScale);
// 0x148992 (cdecl): inPoint in camera space (outScreen) and its depth provisional name and meaning
void EEGetCameraPoint(const GE3DPoint& inPoint, GE3DPoint& outScreen, float& outDepth);
extern float gF564518;                      // 0x564518 (sound: behind-the-camera falloff) (provisional name)
extern float gF56451C;                      // 0x56451c (sound: off-screen falloff) (provisional name)
extern float gF564520;                      // 0x564520 (sound: pan scale) (provisional name)
void EEReplaceQueuedTechnology(EEPlayer* inPlayer, EEUnitClass* inClass, long inNewTechnology);
EEUnit* EEMakeUnitCopy(EEComplexUnit* inUnit, unsigned long inPlayer, EEUnitClass* inClass);
bool EEClearUnderBuilding(long inX, long inY, long inSize);  // (3 arguments: the callee reads +8..+0x10 only)
// 0x23e530 / 0x23e598 (cdecl, before the range): the units on inTile in the way of a unit of inUnitType (outUnits) /
// whether they may be pushed aside (provisional names)
bool EEGetBlockingUnits(EEPlayer* inPlayer, long inUnitType, EETile* inTile, bool inIgnoreOwner, std::vector<EEUnit*>& outUnits);
bool EEGetMovableTileUnits(EEPlayer* inPlayer, long inUnitType, EETile* inTile, bool inIgnoreOwner, std::vector<EEUnit*>& inUnits);
// 0x24f0e9.. (the rest of the TU) (provisional names)
void EEUpgradeHitPoints(EEUnitClassUpgrade* inUpgrade, long inType, float inFactor, EEPlayer* inPlayer);  // (EEWorld.cpp declares it too)
bool EECanPlaceUnitOnTile(EEPlayer* inPlayer, EEComplexUnit* inUnit, long inUnitType, long inX, long inY, bool inSkipPlayerChecks,
                long inIgnoreOwner, bool& outFlag);
bool EEEditorCanPlace(EEPlayer* inPlayer, long inType, GE3DPoint* inLocation, long inX, long inY);
bool EEIsEditorSpacingClear(EEUnitClass* inClass, GE3DPoint* inLocation, long inX, long inY);
void EECreateSpaceWallEnergyWall(EEComplexUnit* inUnit);  // 0x24f8c0 (after the range)
void EECreateSpaceGateEnergyWall(EEComplexUnit* inUnit);  // 0x24f8f9 (after the range)
long EEGetTileDirection(long inX1, long inY1, long inX2, long inY2);
long EEGetDirectionTo(EEUnit* inFrom, EEUnit* inTo);
long EEGetTownCenterLastRing(long* outSize);
long EEGetUnitsOn(std::vector<U2DPoint<long> >& inTiles, std::vector<EEUnit*>& outUnits);  // returns the count
long EEGetUnitsOn(std::vector<EETileCoordinate>& inTiles, std::vector<EEUnit*>& outUnits);  // the same body (0x245a19 passes its EETileCoordinate tiles)
EEComplexUnit* EEGetPursuerOf(EEComplexUnit* inUnit);
bool EECanTargetClass(EEComplexUnit* inUnit, EEUnitClass* inClass);
void EEForEachPlayersUnit(EEUUUnitFunctor* inFunctor);
// 0x2467bb with the types its body reads (ee/EEAreaUtilities.h declares EEUnit* first) (provisional name)
float EEGetDistanceSquared(EEComplexUnit* inUnit, EEComplexUnit* inOther);
void EEGetEnemiesAround(EEComplexUnit* inUnit, std::vector<EEUnit*>& outAttackers);
long EETurnDirection(long inDirection, long inSteps, bool inClockwise);  // 0x24da82 (the definition and names.txt are (long, long, bool))
void EEGetNextTile(const EETileCoordinate& inTile, EETileCoordinate& outTile, long inDirection, long inDistance);  // 0x248ecc: the tile inDistance steps from inTile in inDirection (also in ee/EEE3DStorm.h)
void EEMoveInDirection(const GE2DPoint& inFrom, GE2DPoint& outTo, long inDirection, long inDistance);
// orders units by their angle from mUnit (std::sort 0x2509af from 0x246254) (provisional name)
struct EEUUAngleLess {
    EEUUAngleLess(EEComplexUnit* inUnit) : mUnit(inUnit) {}
    bool operator()(EEUnit* inFirst, EEUnit* inSecond) const;
    EEComplexUnit* mUnit;
};
bool EEApproachAttachPoint(EEComplexUnit* inUnit, GE3DPoint& outPoint);
void EEGetSquareRing(long inX, long inY, long inWidth, long inHeight, std::vector<U2DPoint<long> >& outTiles);
bool EEIsCornerBlocked(unsigned long inX1, unsigned long inY1, unsigned long inX2, unsigned long inY2);
float EEGetNearestFootprintTile(EEComplexUnit* inUnit, const GE3DPoint& inPoint, long* outX, long* outY);
// Genuine generic game permutation at original4eef9; nominal PermuteValues identity inferred.
// Independent callers prove vector<T*> instantiations, including nested region-vector pointers.
// Body: element access through operator[] each time, one temporary for the swaps (U2DPoint: its zeroing ctor first),
// the signed modulo of the size; exact for vector<EEUnit*> 0x4eef9 (SeaTransport), <ushort> 0x8909e and
// <U2DPoint<long> > 0x89273 (Personnel).
template <class Container> void PermuteValues(Container& ioValues, long inStart)
{
    typename Container::value_type theTemporary;
    if (ioValues.size() > 1) {
        long theCount = ioValues.size();
        long theIndex = inStart % (long)ioValues.size();
        long i;
        for (i = theCount - 1; i > 0; i--) {
            if (i != theIndex) {
                theTemporary = ioValues[i];
                ioValues[i] = ioValues[theIndex];
                ioValues[theIndex] = theTemporary;
            }
            i--;
            long theOpposite = theCount - theIndex - 1;
            if (i != theOpposite) {
                theTemporary = ioValues[i];
                ioValues[i] = ioValues[theOpposite];
                ioValues[theOpposite] = theTemporary;
            }
            theIndex = (theOpposite * 2 + 1) % theCount;
        }
        if (i == 0 && theIndex < theCount && theIndex != 0) {
            theTemporary = ioValues[0];
            ioValues[0] = ioValues[theIndex];
            ioValues[theIndex] = theTemporary;
        }
    }
}
void EEGetRingCorners(long inCorners, long inX, long inY, long inWidth, long inHeight, std::vector<U2DPoint<long> >& outTiles);
bool EECanReach(float inDistanceSquared, float inRadius, float inOtherRadius, float inScale);
bool EEIsInGatherReach(float inDistanceSquared, EEComplexUnit* inUnit, EEUnit* inTarget);
bool EEGetBallisticPitch(EEComplexUnit* inUnit, EEUnitClassUpgrade* inStats, const GE3DPoint* inFrom, const GE3DPoint* inTo, float* outAngle);  // 0x241a6a (before the range)
bool EEIsFootprintVisibleTo(EEUnit* inUnit, EEPlayer* inPlayer);
// 0x2479eb as names.txt binds it first (EEComplexUnit*; ee/EEAreaUtilities.h declares the EEUnit* alias)
bool EEIsSeenBy(EEUnit* inUnit, EEPlayer* inPlayer, bool inExplored);
bool EEIsSeenByOwnerOf(EEComplexUnit* inUnit, EEUnit* inOther);
// 0x247bef: the tile line of a wall through inTiles, joined to the walls of inClass's type at its ends (provisional
// name)
long EEExtendWallLine(std::vector<EETileCoordinate>& inTiles, EEPlayer* inPlayer, EEUnitClass* inClass,
                    std::vector<EETileCoordinate>& outTiles);
bool EEFindOpenSpot(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& outLocation);
void EEGetWallSideTiles(EEComplexUnit* inUnit, EEComplexUnit* inTarget, std::vector<EETileCoordinate>& ioTiles);
// Puts the tiles of path area mID first (std::stable_sort 0x250b65 from 0x248709; its operator() 0x23de5d is out of
// line before the range) (provisional name)
struct EEUUTileInPathArea {
    EEUUTileInPathArea(unsigned short inID) : mID(inID) {}
    bool operator()(const EETileCoordinate& inFirst, const EETileCoordinate& inSecond) const;   // 0x23de5d
    unsigned short mID;
};
// Orders points by inUnit's distance to them (DistanceSquared2D; std::sort 0x250aa5 from 0x2481eb; its operator()
// 0x23de0f is out of line before the range) (provisional name)
struct EEUUPointNearerToUnit {
    EEUUPointNearerToUnit(EEComplexUnit* inUnit) : mUnit(inUnit) {}
    bool operator()(const GE2DPoint& inFirst, const GE2DPoint& inSecond) const;   // 0x23de0f
    EEComplexUnit* mUnit;
};
// 0x24116b / 0x241504 with an EEUnit* target (0x248c7e passes its EEUnit*; ee/EEUnit.h declares EEComplexUnit*)
void EEGetTilesAroundUnit(EEComplexUnit* inUnit, EEUnit* inTarget, long inArg3, long inArg4, bool inArg5, std::vector<EETileCoordinate>& outTiles);
bool EEPickTileNear(std::vector<EETileCoordinate>& inTiles, EEComplexUnit* inUnit, EEUnit* inTarget, bool inArg4, GE3DPoint& outLocation);
float EEGetUnitRadius(EEComplexUnit* inUnit, EEComplexUnit* inOther);   // 0x248d05 with the types its body reads
void EEGetTileInDirection(long inX, long inY, long* outX, long* outY, long inDirection, long inDistance);
long EETurnAway(long inDirection, long inSpread);
template <class T> class U2DRectangle;
typedef U2DRectangle<long> EEAIAStarBounds;   // ee/EEAIAStarStorage.h
class URandom;
// 0x249201 is EEFOFindLocation (bool; its tile screen is the RTTI-named EEFOLocationScreenBase): declared in
// ee/EEFOLocationScreenBase.h (was EEFn249201 with an EEUUTileFilter* here)
unsigned long EEGetLeadUnitID(EEPlayer* inPlayer);
void EEGetAimPoint(EEComplexUnit* inUnit, GE3DPoint* inTarget, GE3DPoint* outPoint);
void EEGetHelicopterPriestDestination(EEComplexUnit* inUnit, GE3DPoint& outPoint);
// 0x248fe4 (EEAIAStarPathfindingGoalBase.cpp declares it locally) (provisional name)
void EEGetUnitTiles(EEUnit* inUnit, std::vector<U2DPoint<long> >& outTiles, float inMargin);
void EEGetFootprintTiles(EEUnit* inUnit, std::vector<U2DPoint<long> >& outTiles);  // the same body (0x248fe4's types)
void EEGetTilesNearPoint(const GE3DPoint& inCenter, float inRadius, std::vector<U2DPoint<long> >& outTiles);  // the same body (0x248fe4's types)
// the sort-and-unique template (0x211ebc; defined in EEUCGate.cpp's range) (provisional name)
template <class V, class L, class E> void EESortUnique(V& ioValues, L inLess, E inEqual);
void EEGetFanTiles(const GE2DPoint& inCenter, float inRadius, float inFrom, float inTo, long inSteps, std::vector<U2DPoint<long> >& outTiles);
EEUnit* EEGetWallAt(const EETileCoordinate& inTile, EEPlayer* inPlayer, long inUnitType);
void EEExtendWall(const EETileCoordinate& inFrom, const EETileCoordinate& inTo, EEPlayer* inPlayer, long inUnitType, std::vector<EETileCoordinate>& ioTiles);
void EEGetWallLine(const EETileCoordinate& inFrom, const EETileCoordinate& inTo, std::vector<EETileCoordinate>& outTiles);
void EEGetWallLineXY(const GE2DPoint& inFrom, const GE2DPoint& inTo, std::vector<EETileCoordinate>& outTiles);
void EEGetTileLine(long inX1, long inY1, long inX2, long inY2, std::vector<EETileCoordinate>* outTiles, bool inEnds, bool inThin);  // the same body over EETileCoordinate (0x24b794)
long EEGetUnitsOnTiles(std::vector<U2DPoint<long> >& inTiles, std::vector<EEUnit*>& outUnits);
// (0x24b8ac EEGetPathLengthTo: ee/EEAreaUtilities.h)
void EERemoveEdgeTiles(std::vector<U2DPoint<long> >& ioTiles);
long EEGetUnitsOnTiles(std::vector<EETileCoordinate>& inTiles, std::vector<EEUnit*>& outUnits);  // the same body over EETileCoordinate (0x24c003)
// 0x24c3df: the units on tile (inX, inY) (as ee/EEE3DEnergyWall.h declares it); returns their count
long EEGetUnitsOnTile(long inX, long inY, std::vector<EEUnit*>& outUnits);
// 0x2046fc (EEGhostUnit.cpp defines it): the wall ghosts of inTiles/inCorners for inPlayer
void EEBuildWallGhosts(std::vector<EETileCoordinate>& inTiles, std::vector<long>& inCorners, long inTechnology, EEPlayer* inPlayer,
                float inRotation);
long EEGetUnitsNearTile(const GE3DPoint& inCenter, float inRadius, std::vector<EEUnit*>& outUnits);
long EEGetUnitsInFan(const GE2DPoint& inCenter, float inRadius, float inFrom, float inTo, long inSteps, std::vector<EEUnit*>& outUnits);
// (0x250d71: EEGetUnitsInRectangle with ee/EEAreaUtilities.h's EEAnyUnit, the ee/EEUnitCollector.h template)
EEUnit* EEGetTileUnitOfType(const EETileCoordinate& inTile, unsigned long inUnitType);
long EEGetUnitsOfTypeOnTiles(std::vector<EETileCoordinate>& inTiles, EEPlayer* inPlayer, long inUnitType, std::vector<EEUnit*>& outUnits);
long EEGetUnitsOfTypeOnTiles(std::vector<U2DPoint<long> >& inTiles, EEPlayer* inPlayer, long inUnitType, std::vector<EEUnit*>& outUnits);  // the same body over U2DPoint<long> (0x247bef)
long EEGetWallUnitsOfTypeOnTiles(std::vector<EETileCoordinate>& inTiles, long inUnitType, std::vector<EEUnit*>& outUnits);
float EEJitterHeading(float inAngle, float inMin, float inMax);
class EEGhostUnit;
void EEOrientWallGhost(EEGhostUnit* inGhost, long inCorner, long inRaisedCorners);
long EEOrientWallPiece(GETransformation& ioTransformation, long inCorner, long inRaisedCorners);
bool EEFn24E68F(EECivUpgrade inUpgrade, UWideString& outText);
class EECivilization;
// 0x24edcf: whether inValue is in the civilization's GetPurchasedUpgrades list (0x64edcf calls GetPurchasedUpgrades
// 0x12c738 on its first argument) (provisional name)
bool EEHasCivilizationUpgrade(EECivilization* inCivilization, EECivUpgrade inValue);
void EEMovePassengers(EEComplexUnit* inUnit, EEComplexUnit* inContainer, bool inIsConversion);
void EETakeOverLoad(EEComplexUnit* inUnit, EEComplexUnit* inOther);
bool EEIsTileTopAbove(unsigned long inX, unsigned long inY, float inHeight, EEUnit** outArg4, bool inBare);
#endif
