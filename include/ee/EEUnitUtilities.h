// EEUnitUtilities.h (RECONSTRUCTION; file name inferred; every name provisional). The cdecl unit utilities
// of 0x241c2d..0x24f0e9 (EEFOFindNewTargetNearOldTarget.cpp per the unit map; probably one "EEUnitUtilities.cpp"
// with EEUU* names that opens at 0x23dd54) that no other header declares yet. The ones other units call are declared
// in ee/EEAreaUtilities.h, ee/EEUnit.h and the callers' headers; their signatures are kept.
// Requires lle/LowLevelEngine.h, lle/GE3DPoint.h, <vector>, ee/EEAreaUtilities.h.
// The utilities that are one call of their Rust bodies (ee_sim units uu_*.rs) are inline here.
#ifndef EE_EEUNITUTILITIES_H
#define EE_EEUNITUTILITIES_H
#include <stdint.h>
#include <vector>
#include "ee/EECivUpgrade.h"
#include "ee/EETileCoordinate.h"
#include "lle/LowLevelEngineCore.h"     // UFloatBits
#include "lle/U2DRectangle.h"
#include "rust/sim/units.h"

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
class GE3DPoint;
class GETransformation;
class UWideString;
// the utilities' handles (ee_units uu.rs): an object's address (a complex unit's EEUnit base is at its address)
inline uint64_t EEUUHandle(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }
inline EEUnit* EEUUUnit(uint64_t inUnit) { return (EEUnit*)(uintptr_t)inUnit; }
// Actual global23cd32 definition in EEWheel.cpp; no new helper body or credit.
bool EEGetRandomPointAhead(EEComplexUnit* inUnit, GE2DPoint* outPoint);

// (EEUUTileNearerToUnit, the firing ring's sort order: ee_sim combat cg_world.rs over uu.rs distance_squared_to_tile)

// 0x241bc9 (before the range): inUnit's attack goal moves to inTarget (provisional name)
// Points a unit's attack goal at a new target and restarts its attack
// inUnit's attack goal (inGoal is an EEUGAttackUnit: 0x241c2d's dynamic_cast) moves on to inTarget, and the unit starts
// its attack action (0x7dd) over
inline void EERetargetAttack(EEComplexUnit* inUnit, EEUnitGoal* inGoal, EEComplexUnit* inTarget)
{
    ee_units_ub_body(EE_UNITS_UB_DO_RETARGET_ATTACK, EEUUHandle(inUnit), EEUUHandle(inGoal), (int64_t)EEUUHandle(inTarget), 0, 0);   // uu_body.rs
}
// 0x241c2d: inUnit looks for a new target near inTarget (EEFOFindNewTargetNearOldTarget, 25 squared tiles)
// Finds a unit a new target near its old one: another piece of a linked (wall/gate) target, else the
// nearest living enemy within 5 tiles
inline void EEFindNewTargetNearOld(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    ee_units_ub_body(EE_UNITS_UB_DO_FIND_NEW_TARGET_NEAR_OLD, EEUUHandle(inUnit), EEUUHandle(inTarget), 0, 0, 0);   // uu_body.rs
}
// 0x245dd6: the nearest unit around inTile that passes inScreen; inUnit's goal attacks it (false: none)
// Gives a unit's goal the first enemy near a tile that passes a screen; false when none
// (uu_search.rs over ee_combat's units_in_area and choose_target)
inline bool EEFindNewTarget(EEFOFindNewTargetScreenBase* inScreen, EEUnitGoal* inGoal, EEComplexUnit* inUnit,
                EETileCoordinate* inTile, long inRadius, bool inArg6, long inArg7)
{
    return ee_units_sr_find_new_target(EEUUHandle(inScreen), EEUUHandle(inGoal), EEUUHandle(inUnit), inTile->GetX(), inTile->GetY(), inRadius, inArg6, inArg7) != 0;
}
// Tells whether a unit can hit a target from its attach point (a clear line to the drop location)
inline bool EECanBallisticHit(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    return ee_units_ub_body(EE_UNITS_UB_DO_CAN_BALLISTIC_HIT, EEUUHandle(inUnit), EEUUHandle(inTarget), 0, 0, 0) != 0;   // uu_body.rs
}

// Returns the tile of a unit's footprint nearest to a tile's centre and that squared distance
inline float EEGetFootprintDistance(EEComplexUnit* inUnit, long inX, long inY, long* outX, long* outY)  // 0x246ea2 (names.txt binds it)
{
    return ee_units_uu_footprint_distance(EEUUHandle(inUnit), inX, inY, outX, outY);   // uu_adapt.rs
}
// EEBlockTakes 0x243c7d: ee/EEAreaUtilities.h
// Returns the best target for a unit around a tile, scored by a target search
// The best target for inUnit around (inX, inY) (the tile search 0x24fcd1, or 0x2503d7 from its position for a
// class with +0x318), scored by an EEUUTargetSearch (uu_search.rs over ee_combat's search_target)
inline EEUnit* EESearchTarget(EEComplexUnit* inUnit, U2DRectangle<long>* inArea, long inX, long inY, float inScale)
{
    return EEUUUnit(ee_units_sr_search_target(EEUUHandle(inUnit), inArea->mLeft, inArea->mTop, inArea->mRight, inArea->mBottom, inX, inY, inScale));
}
// Returns a target near a unit (along the unit list for anti-air ammunition 0x3c)
EEUnit* EEFindNearbyTarget(EEComplexUnit* inUnit);
// Returns the best target for a unit around a tile along the x-sorted unit list, scored by its targeting table
// The best target for inUnit around (inX, inY) along the x-sorted unit list (both ways), scored by the targeting
// table row of its mode (the unit-list twin of EEUUTargetSearch) (uu_search.rs over ee_combat's search_along_list)
inline EEUnit* EESearchTargetAlongList(EEComplexUnit* inUnit, long inX, long inY, float inScale)
{
    return EEUUUnit(ee_units_sr_search_along_list(EEUUHandle(inUnit), inX, inY, inScale));   // uu_search.rs
}
// Returns the best target for a unit around a tile when the influence table shows enemies within its reach
EEUnit* EEFindTargetNear(EEComplexUnit* inUnit, long inX, long inY, float inScale);
// 0x148992 (cdecl): inPoint in camera space (outScreen) and its depth provisional name and meaning
void EEGetCameraPoint(const GE3DPoint& inPoint, GE3DPoint& outScreen, float& outDepth);
// Replaces a queued technology on every living unit of a player whose class makes it
inline void EEReplaceQueuedTechnology(EEPlayer* inPlayer, EEUnitClass* inClass, long inNewTechnology)
{
    ee_units_ub_body(EE_UNITS_UB_DO_REPLACE_QUEUED_TECHNOLOGY, EEUUHandle(inPlayer), EEUUHandle(inClass), inNewTechnology, 0, 0);
}
// Creates a copy of a unit (or a new class) for a player on its tile, in its state
inline EEUnit* EEMakeUnitCopy(EEComplexUnit* inUnit, unsigned long inPlayer, EEUnitClass* inClass)
{
    return EEUUUnit(ee_units_ub_body(EE_UNITS_UB_DO_MAKE_UNIT_COPY, EEUUHandle(inUnit), EEUUHandle(inClass), (int64_t)inPlayer, 0, 0));   // uu_body.rs
}
// 0x24f0e9.. (the rest of the TU) (provisional names)
// Raises a class upgrade's hit points by a share of its base, and the player's units of that type with it
// inUpgrade's hit points raised by inFactor of its class's base; inPlayer's units of type inType follow (inferred)
void EEUpgradeHitPoints(EEUnitClassUpgrade* inUpgrade, long inType, float inFactor, EEPlayer* inPlayer);  // (EEWorld.cpp declares it too)
// Gives a space wall its energy wall effect
inline void EECreateSpaceWallEnergyWall(EEComplexUnit* inUnit)  // 0x24f8c0 (after the range)
{
    ee_units_ub_body(EE_UNITS_UB_DO_SPACE_ENERGY_WALL, EEUUHandle(inUnit), 0, 0, 0, 0);   // uu_body.rs
}
// Gives a space gate its energy wall effect
inline void EECreateSpaceGateEnergyWall(EEComplexUnit* inUnit)  // 0x24f8f9 (after the range)
{
    ee_units_ub_body(EE_UNITS_UB_DO_SPACE_ENERGY_WALL, EEUUHandle(inUnit), 0, 1, 0, 0);   // uu_body.rs
}
// Returns the nearest unit that attacks or gathers from the unit
// (uu_search.rs over ee_combat's pursuer_of)
inline EEComplexUnit* EEGetPursuerOf(EEComplexUnit* inUnit)
{
    return (EEComplexUnit*)(uintptr_t)ee_units_sr_pursuer_of(EEUUHandle(inUnit));   // uu_search.rs
}
// Runs a functor over the units of every player but nature
inline void EEForEachPlayersUnit(EEUUUnitFunctor* inFunctor)
{
    ee_units_ub_body(EE_UNITS_UB_DO_FOR_EACH_PLAYERS_UNIT, EEUUHandle(inFunctor), 0, 0, 0, 0);   // uu_body.rs
}
// Returns the tile a distance away in a compass direction (tile form)
inline void EEGetNextTile(const EETileCoordinate& inTile, EETileCoordinate& outTile, long inDirection, long inDistance)  // 0x248ecc: the tile inDistance steps from inTile in inDirection (also in ee/EEE3DStorm.h)
{
    ee_units_uu_next_tile(inTile.mX, inTile.mY, inDirection, inDistance, &outTile.mX, &outTile.mY);   // uu_adapt.rs
}
// Returns the world position of a unit's current attach point; false (half a tile up) without one
// The world position of inUnit's current tag point (its model part's list, the unit's frame), scaled by its size;
// false (and the point half a tile up) when the part has no tag points (new: ee_combat's attach_point)
inline bool EEApproachAttachPoint(EEComplexUnit* inUnit, GE3DPoint& outPoint)
{
    return ee_units_lf_attach_point(EEUUHandle(inUnit), (float(*)[3])&outPoint) != 0;   // uu_fill.rs
}
// (PermuteValues, the generic permutation at 0x4eef9: ee_sim combat search.rs permute)
// Computes the launch pitch for a shot at its speed under gravity (low or high arc); false (45 degrees) when
// out of reach
// The launch angle for a shot from inFrom to inTo at inStats' speed under its projectile class's gravity (the low
// arc, or the high one for a class with +0x2e5 or a mortar-like unit); false (and PI/4) when it cannot reach
bool EEGetBallisticPitch(EEComplexUnit* inUnit, EEUnitClassUpgrade* inStats, const GE3DPoint* inFrom, const GE3DPoint* inTo,
                float* outAngle);  // 0x241a6a (before the range)
typedef U2DRectangle<long> EEAIAStarBounds;   // ee/EEAIAStarStorage.h
class URandom;
// 0x249201 is EEFOFindLocation (bool; its tile screen is the RTTI-named EEFOLocationScreenBase): declared in
// ee/EEFOLocationScreenBase.h (was EEFn249201 with an EEUUTileFilter* here)
// Returns where a unit casts its calamity from: 90% of the calamity's range from the target
inline void EEGetHelicopterPriestDestination(EEComplexUnit* inUnit, GE3DPoint& outPoint)
{
    ee_units_ub_body(EE_UNITS_UB_DO_HELICOPTER_PRIEST_DESTINATION, EEUUHandle(inUnit), EEUUHandle(&outPoint), 0, 0, 0);
}
// the sort-and-unique template (0x211ebc; defined in EEUCGate.cpp's range) (provisional name)
template <class V, class L, class E> void EESortUnique(V& ioValues, L inLess, E inEqual);
// Collects the tiles of a line, walking in from both ends (optionally the end tiles and 4-connected)
// The tiles of the line (inX1, inY1)-(inX2, inY2): a symmetric double-step rasteriser that walks in from both ends
// (inEnds: the two end tiles too; !inThin: the extra tiles that make the line 4-connected), in order from (inX1, inY1)
// (new: the rasteriser is ee_combat's tile_line)
inline void EEGetTileLine(long inX1, long inY1, long inX2, long inY2, std::vector<U2DPoint<long> >* outTiles, bool inEnds,
                bool inThin)  // the same body over EETileCoordinate (0x24b794)
{
    ee_units_lf_tile_line((int32_t)inX1, (int32_t)inY1, (int32_t)inX2, (int32_t)inY2, inEnds, inThin, outTiles);   // uu_line.rs
}
// (0x24b8ac EEGetPathLengthTo: ee/EEAreaUtilities.h)
// 0x2046fc (EEGhostUnit.cpp defines it): the wall ghosts of inTiles/inCorners for inPlayer
void EEBuildWallGhosts(std::vector<EETileCoordinate>& inTiles, std::vector<long>& inCorners, long inTechnology, EEPlayer* inPlayer,
                float inRotation);
class EEGhostUnit;
void EEOrientWallGhost(EEGhostUnit* inGhost, long inCorner, long inRaisedCorners);
long EEOrientWallPiece(GETransformation& ioTransformation, long inCorner, long inRaisedCorners);
// Builds the text of a civilization upgrade ("category: attribute percent%"); false when a string is missing
// The text of civilization upgrade inUpgrade ("<category>: <attribute> <percent>%"); false when a string is missing.
// Its two function-local static strings' atexit dtors follow it (0x24e7c4 / 0x24e7cf).
inline bool EEFn24E68F(EECivUpgrade inUpgrade, UWideString& outText)
{
    return ee_units_ub_body(EE_UNITS_UB_DO_UPGRADE_TEXT, 0, EEUUHandle(&outText), (int64_t)inUpgrade, 0, 0) != 0;   // uu_body.rs
}
class EECivilization;
// 0x24edcf: whether inValue is in the civilization's GetPurchasedUpgrades list (0x64edcf calls GetPurchasedUpgrades
// 0x12c738 on its first argument) (provisional name)
// Tells whether a civilization bought an upgrade
// Whether inValue is in inCivilization's mPurchasedUpgrades (read through the out-of-line GetPurchasedUpgrades 0x12c738, ICF with EECampaign::GetScenarios)
inline bool EEHasCivilizationUpgrade(EECivilization* inCivilization, EECivUpgrade inValue)
{
    return ee_units_ub_body(EE_UNITS_UB_DO_HAS_CIVILIZATION_UPGRADE, EEUUHandle(inCivilization), 0, (int64_t)inValue, 0, 0) != 0;
}
// Moves a container's cargo into the unit's care (also placing it on the map for a conversion)
inline void EEMovePassengers(EEComplexUnit* inUnit, EEComplexUnit* inContainer, bool inIsConversion)
{
    ee_units_ub_body(EE_UNITS_UB_DO_MOVE_PASSENGERS, EEUUHandle(inUnit), EEUUHandle(inContainer), inIsConversion, 0, 0);   // uu_body.rs
}
// Gives a unit another unit's load (as much as its class allows), emptying the other
inline void EETakeOverLoad(EEComplexUnit* inUnit, EEComplexUnit* inOther)
{
    ee_units_ub_body(EE_UNITS_UB_DO_TAKE_OVER_LOAD, EEUUHandle(inUnit), EEUUHandle(inOther), 0, 0, 0);   // uu_body.rs
}
// Tells whether a wall piece on a tile rises above a height
inline bool EEIsTileTopAbove(unsigned long inX, unsigned long inY, float inHeight, EEUnit** outArg4, bool inBare)
{
    return ee_units_uu_tile_top_above_at((long)inX, (long)inY, inHeight, reinterpret_cast<uint64_t*>(outArg4), inBare);   // uu_adapt.rs
}
#endif
