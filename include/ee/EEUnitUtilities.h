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
typedef EEUnitGoal EEUGRust;
class EEFOFindNewTargetScreenBase;
class EEUnitClassUpgrade;
class GE2DPoint;
// Actual global23cd32 definition in EEWheel.cpp; no new helper body or credit.
bool EEGetRandomPointAhead(EEComplexUnit* inUnit, GE2DPoint* outPoint);
template <class T> class U2DRectangle;
template <class T> class U2DPoint;

// (EEUUTileNearerToUnit, the firing ring's sort order: ee_sim combat cg_world.rs over uu.rs distance_squared_to_tile)

// 0x241bc9 (before the range): inUnit's attack goal moves to inTarget (provisional name)
void EERetargetAttack(EEComplexUnit* inUnit, EEUnitGoal* inGoal, EEComplexUnit* inTarget);
// 0x241c2d: inUnit looks for a new target near inTarget (EEFOFindNewTargetNearOldTarget, 25 squared tiles)
void EEFindNewTargetNearOld(EEComplexUnit* inUnit, EEComplexUnit* inTarget);
// 0x245dd6: the nearest unit around inTile that passes inScreen; inUnit's goal attacks it (false: none)
bool EEFindNewTarget(EEFOFindNewTargetScreenBase* inScreen, EEUnitGoal* inGoal, EEComplexUnit* inUnit,
                EETileCoordinate* inTile, long inRadius, bool inArg6, long inArg7);
bool EECanBallisticHit(EEComplexUnit* inUnit, EEUnit* inTarget);

float EEGetFootprintDistance(EEComplexUnit* inUnit, long inX, long inY, long* outX, long* outY);  // 0x246ea2 (names.txt binds it)
// EEBlockTakes 0x243c7d: ee/EEAreaUtilities.h
EEUnit* EESearchTarget(EEComplexUnit* inUnit, U2DRectangle<long>* inArea, long inX, long inY, float inScale);
EEUnit* EEFindNearbyTarget(EEComplexUnit* inUnit);
EEUnit* EESearchTargetAlongList(EEComplexUnit* inUnit, long inX, long inY, float inScale);
EEUnit* EEFindTargetNear(EEComplexUnit* inUnit, long inX, long inY, float inScale);
// 0x148992 (cdecl): inPoint in camera space (outScreen) and its depth provisional name and meaning
void EEGetCameraPoint(const GE3DPoint& inPoint, GE3DPoint& outScreen, float& outDepth);
void EEReplaceQueuedTechnology(EEPlayer* inPlayer, EEUnitClass* inClass, long inNewTechnology);
EEUnit* EEMakeUnitCopy(EEComplexUnit* inUnit, unsigned long inPlayer, EEUnitClass* inClass);
// 0x24f0e9.. (the rest of the TU) (provisional names)
void EEUpgradeHitPoints(EEUnitClassUpgrade* inUpgrade, long inType, float inFactor, EEPlayer* inPlayer);  // (EEWorld.cpp declares it too)
void EECreateSpaceWallEnergyWall(EEComplexUnit* inUnit);  // 0x24f8c0 (after the range)
void EECreateSpaceGateEnergyWall(EEComplexUnit* inUnit);  // 0x24f8f9 (after the range)
EEComplexUnit* EEGetPursuerOf(EEComplexUnit* inUnit);
void EEForEachPlayersUnit(EEUUUnitFunctor* inFunctor);
void EEGetNextTile(const EETileCoordinate& inTile, EETileCoordinate& outTile, long inDirection, long inDistance);  // 0x248ecc: the tile inDistance steps from inTile in inDirection (also in ee/EEE3DStorm.h)
bool EEApproachAttachPoint(EEComplexUnit* inUnit, GE3DPoint& outPoint);
// (PermuteValues, the generic permutation at 0x4eef9: ee_sim combat search.rs permute)
bool EEGetBallisticPitch(EEComplexUnit* inUnit, EEUnitClassUpgrade* inStats, const GE3DPoint* inFrom, const GE3DPoint* inTo, float* outAngle);  // 0x241a6a (before the range)
typedef U2DRectangle<long> EEAIAStarBounds;   // ee/EEAIAStarStorage.h
class URandom;
// 0x249201 is EEFOFindLocation (bool; its tile screen is the RTTI-named EEFOLocationScreenBase): declared in
// ee/EEFOLocationScreenBase.h (was EEFn249201 with an EEUUTileFilter* here)
void EEGetHelicopterPriestDestination(EEComplexUnit* inUnit, GE3DPoint& outPoint);
// the sort-and-unique template (0x211ebc; defined in EEUCGate.cpp's range) (provisional name)
template <class V, class L, class E> void EESortUnique(V& ioValues, L inLess, E inEqual);
void EEGetTileLine(long inX1, long inY1, long inX2, long inY2, std::vector<EETileCoordinate>* outTiles, bool inEnds, bool inThin);  // the same body over EETileCoordinate (0x24b794)
// (0x24b8ac EEGetPathLengthTo: ee/EEAreaUtilities.h)
// 0x2046fc (EEGhostUnit.cpp defines it): the wall ghosts of inTiles/inCorners for inPlayer
void EEBuildWallGhosts(std::vector<EETileCoordinate>& inTiles, std::vector<long>& inCorners, long inTechnology, EEPlayer* inPlayer,
                float inRotation);
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
