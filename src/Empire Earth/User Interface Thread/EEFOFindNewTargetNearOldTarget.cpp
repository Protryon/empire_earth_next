// C:\EEX\Empire Earth\User Interface Thread\EEFOFindNewTargetNearOldTarget.cpp (file name inferred: no __FILE__).
// The game's unit utilities: 0x23dd54..0x2544e9 (EEDSUIEvents.cpp follows). The range opens with a `$E` chain
// (481..483) and the out-of-line predicates its sorts expand; 0x241c2d builds an EEFOFindNewTargetNearOldTarget on
// the stack and 0x241e21 is that class's Virtual04/Virtual08; the rest are cdecl unit utilities (the original file is
// probably "EEUnitUtilities.cpp": EEUU* names, the assert "EEUUGetIsUnitClassTargetable"; the EEFO* classes are its
// local screens). 0x24f0e9..0x24f8f9 are its last functions (the EEHitPointUpgrader functor, the placement tests) and
// 0x24f932..0x2544e9 the template COMDATs its functions instantiate (rbegin/rend for 0x247bef, push_back<GE2DPoint>
// for 0x2481eb, stable_sort for 0x248709, the sorts of 0x241e49/0x246254/0x2481eb; the user templates 0x24fcd1/
// 0x2503d7 (EEUUSearchTiles), 0x250806, 0x250b99, 0x250d71 (an inline EEGetUnitsInArea), 0x25000c).
// 0x24e68f's two function-local static UWideStrings put their atexit dtors at 0x24e7c4/0x24e7cf (not a `$E` chain).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <typeinfo>
#include <vector>
#include <map>
#include <deque>
#include <algorithm>
#include <functional>
#include <mutex>

#include "lle/LowLevelEngine.h"
#include "lle/GEViewport.h"
#include "lle/GEModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/U2DSparseArray.h"
#include "lle/UResourceFile.h"
#include "lle/USingleton.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EECombatGoals.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEPhysics.h"
#include "ee/EECPTrace.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUGRust.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEDebugTrace.h"
#include "ee/EEGameVariant.h"
#include "ee/EEGhostUnit.h"
#include "ee/EECivilizationManager.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEFOFindNewTargetNearOldTarget.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEFOLocationScreenBase.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEMapDimensions.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEAIRandom.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUnitCollector.h"
#include "ee/EEStringIDs.h"
#include "rust/game/civilization.h"
#include "rust/game/gamecpp.h"
#include "rust/game/physics.h"
#include "rust/game/preload_media.h"
#include "rust/game/rts.h"
#include "rust/game/screens.h"
#include "rust/sim/tech.h"
#include "rust/sim/units.h"

// EEContagionHelper.cpp's collector over complex units (its only instantiation was EECPPlacement.cpp's)
template long EEGetUnitsInRectangle<EEAnyUnit, std::vector<EEComplexUnit*> >(const U2DRectangle<long>&, std::vector<EEComplexUnit*>&,
                                                                           const EEAnyUnit&, bool);
extern EEToHitTable gToHitTable;           // 0x5318e8 (as EECPAttackSubGroupUnit.cpp declares it)

// (the unit's named float constants, .rdata 0x449b54..0x449bc0: the Rust bodies' (ee_units uu_world.rs, uu_body.rs))
// (0x23dd54/0x23dd6e/0x23dd82, the dynamic initializers of the sound falloff factors gF564520/gF56451C/gF564518: ee_units
// uu_world.rs sound_parameters computes them)

// the unit utilities' handles (ee_units uu.rs): an object's address (a unit's as its EEUnit*)
static uint64_t UUHandle(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }
static EEUnit* UUUnit(uint64_t inUnit) { return (EEUnit*)(uintptr_t)inUnit; }
static EETile* UUTile(uint64_t inTile) { return (EETile*)(uintptr_t)inTile; }
// a float's bits and back (uu_body.rs passes floats as their bits)
template <class T> static T* UbObj(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }
// a tile list from Rust: its long pairs are U2DPoint<long>s (uu_host.rs long_pairs)
static const U2DPoint<long>* UUPoints(const long (*inTiles)[2]) { return reinterpret_cast<const U2DPoint<long>*>(inTiles); }

// TEST ONLY: the unit utilities' record (UUTL) is Rust's (ee_units uu_note.rs); the host's writes note their events
static long UUId(const EEUnit* inUnit) { return inUnit != NULL ? (long)((const RTSUnit*)inUnit)->mUnitID : -2; }
// the record's tick (the capture's state and the UUTL record itself are Rust's: uu_note.rs)
extern "C" uint32_t ee_units_uu_host_ticks() { return gPhysics != NULL ? (uint32_t)gPhysics->mNumTicksDone : 0; }

// (Units by id: EEUUUnitIDLess / EEUUUnitIDEqual are inline in ee/EEUnitCollector.h)

// (the TU's own sort-and-unique keys, highest unit id first: ee_units uu.rs sort_unique_by_id_descending)

// (0x63ddc1, EEUUTileNearerToUnit, the firing ring's sort order: ee_sim combat cg_world.rs)
// (0x63de0f, EEUUPointNearerToUnit, EEFindOpenSpot's sort order: ee_units uu_spots.rs find_open_spot)

// (0x63deae, EEUUTileNearerToPoint, EEGetSortedRingTiles' sort order: ee_units uu.rs sorted_ring_tiles)

// Accepts a tile that is on the map and on the screen's continent and path area (when set)
bool EEFOLocationScreenEnsureContinentAndPathOverlay::Screen(const U2DPoint<long>& inTile)
{
    return ee_units_lf_location_screen(UUHandle(this), inTile.mX, inTile.mY) != 0;   // uu_fill.rs
}

void EEWorkFarm(EEComplexUnit* inTarget, EEComplexUnit* inUnit)
{
    ee_units_ub_body(EE_UNITS_UB_DO_WORK_FARM, UUHandle(inTarget), UUHandle(inUnit), 0, 0, 0);   // uu_body.rs
}

// Shows the wall ghosts for a wall line of a technology and player
void EEPreviewWall(std::vector<EETileCoordinate>& inTiles, long inTechnology, long inPlayer)
{
    ee_units_ub_body(EE_UNITS_UB_DO_PREVIEW_WALL, UUHandle(&inTiles), 0, inTechnology, inPlayer, 0);   // uu_body.rs
}

// (0x63efa1, EEGetSeabedUnder: ee_units uu_world.rs seabed_under)

// Returns the foundation model a player uses for a building class of its size
EEModel* EEGetPlayerClassGraphic(EEPlayer* inPlayer, EEUnitClass* inClass)
{
    return (EEModel*)(uintptr_t)ee_units_ub_body(EE_UNITS_UB_DO_PLAYER_CLASS_GRAPHIC, UUHandle(inPlayer), UUHandle(inClass), 0, 0, 0);
}

// (EEFOFindNewTargetNearOldTarget's screen: inline in its header)

// Computes a moving unit's Doppler pitch shift against the camera
void EEGetDopplerPitch(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan, float* outPitch)
{
    ee_units_uu_doppler_pitch_of(UUHandle(inUnit), *outVolume, ::gWorld.mTransformation.GetX(), ::gWorld.mTransformation.GetY(),
                                 ::gWorld.mTransformation.GetZ(), outPitch);   // uu_adapt.rs (UUTL kind 83)
}

// (0x642489, EEGetUnitAngle, and the escape point's sort by it, EEUUAngleLess: ee_units uu_world.rs unit_angle)

// Tells whether a priest can convert the target
bool EECanPriestConvert(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    return ee_units_ub_body(EE_UNITS_UB_DO_CAN_PRIEST_CONVERT, UUHandle(inUnit), UUHandle(inTarget), 0, 0, 0) != 0;
}

// Tells whether a unit can attack a target from where it is; else finds the first tile of a path to a place
// it can (outTile)
bool EEIsAttackReachable(EEComplexUnit* inUnit, EEComplexUnit* inTarget, U2DPoint<long>* outTile)
{
    return ee_units_cgq_is_attack_reachable(UUHandle(inUnit), UUHandle(inTarget), &outTile->mX, &outTile->mY);   // cg_query.rs (CBXR)
}

// (The target search of 0x243d43, EEUUTargetSearch with its filter 0x24ffca and scoring 0x25000c, and the tile
// searches 0x24fcd1 / 0x2503d7, EEUUSearchTiles: ee_combat's search_target, search.rs; its constructor 0x643e0a is
// Search::new there. The world it asks is below.)

// (out of line: its callers declare it themselves)
bool EEGetBallisticPitch(EEComplexUnit* inUnit, EEUnitClassUpgrade* inStats, const GE3DPoint* inFrom, const GE3DPoint* inTo,
                float* outAngle)
{
    return ee_units_lf_ballistic_pitch(UUHandle(inUnit), UUHandle(inStats), (const float(*)[3])inFrom, (const float(*)[3])inTo, outAngle) != 0;   // uu_fill.rs
}

// (out of line: its callers declare it themselves)
EEUnit* EEFindNearbyTarget(EEComplexUnit* inUnit)
{
    return UUUnit(ee_units_ub_body(EE_UNITS_UB_DO_FIND_NEARBY_TARGET, UUHandle(inUnit), 0, 0, 0, 0));   // uu_body.rs
}

// (out of line: its callers declare it themselves)
EEUnit* EEFindTargetNear(EEComplexUnit* inUnit, long inX, long inY, float inScale)
{
    return UUUnit(ee_units_ub_body(EE_UNITS_UB_DO_FIND_TARGET_NEAR, UUHandle(inUnit), 0, inX, inY, (int64_t)UFloatBits(inScale)));   // uu_body.rs
}

// (out of line: its callers declare it themselves)
void EEUpgradeHitPoints(EEUnitClassUpgrade* inUpgrade, long inType, float inFactor, EEPlayer* inPlayer)
{
    ee_units_ub_body(EE_UNITS_UB_DO_UPGRADE_HIT_POINTS, UUHandle(inUpgrade), UUHandle(inPlayer), inType, (int64_t)UFloatBits(inFactor), 0);
}

// ---- the target searches (EESearchTarget, EESearchTargetAlongList, EEGetPursuerOf, EEFindNewTarget): their bodies,
// tests, inputs and CBST/CBSL/CBPU/CBFT records are ee_sim units uu_search.rs over ee_combat's walks; each C++
// statement they ask is one EE_UNITS_SR_* case here. Handles are the units' pointers.

static EEUnit* EESrUnit(uint64_t inHandle) { return (EEUnit*)(uintptr_t)inHandle; }
static EEComplexUnit* EESrComplex(uint64_t inHandle) { return (EEComplexUnit*)(uintptr_t)inHandle; }

// (EEIsLineToUnitBlocked's outputs for a search: ee_sim units uu_search.rs EeUnitsSrLine, the blocker's handle its
// EEUnit*)

extern "C" int64_t ee_units_sr_host(int32_t inOp, uint64_t inUnit, uint64_t inA, int64_t inC, float (*ioF)[4])
{
    switch (inOp) {
    case EE_UNITS_SR_SAME_PLAYER: return EESrUnit(inA)->mPlayer == EESrComplex(inUnit)->mPlayer;
    case EE_UNITS_SR_ENEMY: return EESrComplex(inUnit)->mPlayer->mEnemies[EESrUnit(inA)->mPlayer->GetPlayerID()] != 0;
    case EE_UNITS_SR_HIT_POINTS: return EESrComplex(inA)->mHitPoints;
    case EE_UNITS_SR_DISABLED: return EESrComplex(inA)->mIsDisabled;
    case EE_UNITS_SR_PROTECTED: return EESrComplex(inA)->IsProtected();
    case EE_UNITS_SR_ATTACKED_BY: return EECombatWorld(EE_COMBAT_WORLD_ATTACKED_BY, EESrComplex(inUnit), EESrUnit(inA)) != 0;
    case EE_UNITS_SR_CLASS: return UUHandle(EESrUnit(inA)->GetUnitClass());
    case EE_UNITS_SR_IS_FOUNDATION: return EESrComplex(inA)->IsFoundation();
    case EE_UNITS_SR_SEEN: return EECombatIsSeenBy(EESrComplex(inA), EESrComplex(inUnit)->GetPlayer(), true);
    case EE_UNITS_SR_OTHER_CONTINENT: return EESrComplex(inUnit)->IsOnOtherContinentThan(EESrComplex(inA));
    case EE_UNITS_SR_PART_LEFT_OUT: return EESrUnit(inA)->GetUnitClass()->IsPartLeftOut(EESrComplex((uint64_t)inC));
    case EE_UNITS_SR_CLASS_PRIEST: return UbObj<EEUnitClass>(inA)->IsPriest();
    case EE_UNITS_SR_CAN_PRIEST_CONVERT: return EECanPriestConvert(EESrComplex(inUnit), EESrComplex(inA));
    case EE_UNITS_SR_CAN_ATTACK_CLASS: return EESrComplex(inUnit)->CanAttackClass(UbObj<EEUnitClass>(inA));
    case EE_UNITS_SR_IGNORED_COUNT: return (int64_t)EESrComplex(inUnit)->mIgnoredTargets.size();
    case EE_UNITS_SR_IS_IGNORED: return EESrComplex(inUnit)->IsTargetIgnored(EESrComplex(inA), inC != 0);
    case EE_UNITS_SR_GOAL: return UUHandle(EESrComplex(inA)->GetGoal());
    case EE_UNITS_SR_ATTACK_CAST: return UUHandle(EEAttackUnitCast(UbObj<EEUnitGoal>(inA)));
    case EE_UNITS_SR_ATTACK_TARGET_COMPLEX: return UUHandle(UbObj<EEUGRust>(inA)->GetAttackTarget()->mComplexUnit);
    case EE_UNITS_SR_TECH_NODE: return UUHandle(EESrUnit(inA)->mPlayer->mTechnologies->Get(EESrUnit(inA)->GetUnitClass()->mTechnology));
    case EE_UNITS_SR_NODE_KIND: return UbObj<EETechTreeNode>(inA)->mData->mKind;
    case EE_UNITS_SR_LAUNCHER: return EESrComplex(inA)->mLauncherClass != NULL;
    case EE_UNITS_SR_GOAL_STATE: return UUHandle(EESrComplex(inA)->mGoalState);
    case EE_UNITS_SR_GATHER_CAST: return UUHandle(EEGatherGoalCast(UbObj<EEUnitGoal>(inA)));
    case EE_UNITS_SR_GATHER_RESOURCE: return UUHandle(UbObj<EEUGRust>(inA)->GetGatherResource());
    case EE_UNITS_SR_COMPLEX: return UUHandle(EESrUnit(inA)->mComplexUnit);
    case EE_UNITS_SR_LIST_NEXT: return UUHandle(static_cast<EEUnit*>(EESrUnit(inA)->mTileNode->mNext));
    case EE_UNITS_SR_LIST_PREV: return UUHandle(static_cast<EEUnit*>(EESrUnit(inA)->mTileNode->mPrev));
    case EE_UNITS_SR_DUMMY_HEAD: return RTSDummyHead(EESrUnit(inA)) != NULL;
    case EE_UNITS_SR_HIT_DISTANCE: (*ioF)[0] = EECombatHitDistanceSquared(EESrComplex(inUnit), EESrUnit(inA), (*ioF)[0], (*ioF)[1]); return 0;
    case EE_UNITS_SR_ATTACK_FACTOR: (*ioF)[0] = gToHitTable.GetAttackFactor(EESrComplex(inUnit)->GetUnitClass()->mAmmoType, EESrUnit(inA)->GetUnitClass(), EESrComplex(inUnit)->mPlayer->GetPlayerID(), EESrUnit(inA)->mPlayer->GetPlayerID()); return 0;
    case EE_UNITS_SR_LINE_BLOCKED: return EEIsLineToUnitBlocked(EESrComplex(inUnit), EESrUnit(inA), reinterpret_cast<EEUnit**>(&UbObj<EeUnitsSrLine>(inC)->blocker), &UbObj<EeUnitsSrLine>(inC)->x, &UbObj<EeUnitsSrLine>(inC)->y);
    case EE_UNITS_SR_REACHABLE: return EEIsAttackReachable(EESrComplex(inUnit), EESrComplex(inA), UbObj<U2DPoint<long> >(inC));
    case EE_UNITS_SR_IGNORE: EESrComplex(inUnit)->IgnoreTarget(EESrComplex(inA)); return 0;
    case EE_UNITS_SR_FOOTPRINT_TILE: EESrComplex(inUnit)->GetNearestFootprintTile(EESrUnit(inA), UbObj<long>(inC), UbObj<long>(inC) + 1); return 0;
    case EE_UNITS_SR_UNIT_ID: return (int64_t)EESrUnit(inA)->GetUnitID();
    case EE_UNITS_SR_ATTACK: (*ioF)[0] = EESrComplex(inUnit)->mStats->GetAttack(); return 0;
    case EE_UNITS_SR_LINE_OF_SIGHT: (*ioF)[0] = EESrComplex(inUnit)->GetLineOfSight(); return 0;
    case EE_UNITS_SR_MAX_VELOCITY: (*ioF)[0] = EESrComplex(inUnit)->mStats->GetMaxVelocity(); return 0;
    case EE_UNITS_SR_RANGE: (*ioF)[0] = EESrComplex(inUnit)->mStats->GetRange(); return 0;
    case EE_UNITS_SR_MIN_RANGE: (*ioF)[0] = EESrComplex(inUnit)->mUnitClass->mMinRange; return 0;
    case EE_UNITS_SR_MELEE: return EESrComplex(inUnit)->mUnitClass->mIsMelee;
    case EE_UNITS_SR_SHOOTS_HIGH: return EESrComplex(inUnit)->mUnitClass->mShootsHigh;
    case EE_UNITS_SR_OVER_WALLS: return EESrComplex(inUnit)->mStats->mCanFireOverWalls;
    case EE_UNITS_SR_TILE_X: return EESrComplex(inUnit)->mTileX;
    case EE_UNITS_SR_TILE_Y: return EESrComplex(inUnit)->mTileY;
    case EE_UNITS_SR_TICK_RANDOM: return (int32_t)gPhysics->mTickRandom;
    case EE_UNITS_SR_POSITION_X: (*ioF)[0] = EESrComplex(inUnit)->mPhysicalModel.GetLocation().GetX(); return 0;
    case EE_UNITS_SR_POSITION_Y: (*ioF)[0] = EESrComplex(inUnit)->mPhysicalModel.GetLocation().GetY(); return 0;
    case EE_UNITS_SR_UNIT_REGIONS: return UUHandle(gWorld.mMap->mUnitRegions);
    case EE_UNITS_SR_SCREEN: return UbObj<EEFOFindNewTargetScreenBase>(inA)->Virtual08(EESrComplex((uint64_t)inC));
    case EE_UNITS_SR_RETARGET: EERetargetAttack(EESrComplex(inUnit), UbObj<EEUnitGoal>(inA), EESrComplex((uint64_t)inC)); return 0;
    case EE_UNITS_SR_OWN_TECHNOLOGY: return UUHandle(EESrComplex(inUnit)->GetPlayer()->GetTechTree()->Get(EESrUnit(inA)->GetUnitClass()->GetTechnology()));
    case EE_UNITS_SR_TECHNOLOGY_KIND: return UbObj<EETechnology>(inA)->mData->mKind;
    }
    return 0;
}

// (0x250806, EEUUGetUnitsInArea, the units of an area's unit regions that a screen passes: ee_combat's units_in_area,
// called from EEFindNewTarget's body)

// Computes a sound's volume, pan and pitch as heard from the camera (silent off tiles the player sees)
// Volume, pan and pitch of a sound of inVolume at inLocation, as heard from the camera (outPitch 1; volume 0 off a
// tile the current player sees)
// (the camera's view and the arithmetic: ee_units uu_ffi.rs ee_units_uu_compute_sound, uu_world.rs sound_view and
// sound_parameters; inFalloff is unused, as in the original)
void EEComputeSoundParameters(float inVolume, const GE3DPoint& inLocation, float& outVolume, float& outPan,
                              float& outPitch, float)
{
    ee_units_uu_compute_sound(inVolume, (const float(*)[3])&inLocation.mX, &outVolume, &outPan, &outPitch);
}

// the view's reads (EE_UNITS_UU_SOUND_*): each the original forwarder's statement
static float UUSeenTile(EETile* inTile) { return inTile != NULL && inTile->IsSeenBy(::gWorld.GetCurrentPlayer()) ? 1.0f : 0.0f; }
extern "C" float ee_units_uu_host_sound_view(int32_t inWhich, const float (*inLocation)[3], float (*outValues)[4])
{
    switch (inWhich) {
    case EE_UNITS_UU_SOUND_SEEN: return UUSeenTile(::gWorld.mMap->GetTile(UMFloor((*inLocation)[0]), UMFloor((*inLocation)[1])));
    case EE_UNITS_UU_SOUND_CAMERA_POINT: ee_gamecpp_camera_point(&(*(const GE3DPoint*)*inLocation), &(*(GE3DPoint*)*outValues), &((*outValues)[3])); return 0.0f;
    case EE_UNITS_UU_SOUND_CAMERA: ::gWorld.mTransformation.GetPosition(*(GE3DPoint*)*outValues); return 0.0f;
    case EE_UNITS_UU_SOUND_Y_CLIP: return gViewport.GetYClippingPlane();
    case EE_UNITS_UU_SOUND_ZOOM: return ::gWorld.mCameraControl.GetZoom();
    }
    return 0.0f;
}

// Creates a paid building site (one hit point) of a unit type for a player at a tile
EEUnit* EEMakeBuilding(EEPlayer* inPlayer, long inUnitType, long inX, long inY)
{
    return UUUnit(ee_units_ub_body(EE_UNITS_UB_DO_MAKE_BUILDING, UUHandle(inPlayer), 0, inUnitType, inX, inY));   // uu_body.rs
}

// Replaces a unit with a new one of another class (pack/unpack): hit points in proportion, goals, groups
// and effects moved over
// inUnit replaced by a new unit of inClass for inPlayer (EEMPackUnpack): hit points in proportion, the goals, groups
// and effects moved over
EEComplexUnit* EEReplaceUnit(EEComplexUnit* inUnit, unsigned long inPlayer, EEUnitClass* inClass)
{
    return static_cast<EEComplexUnit*>(UUUnit(ee_units_ub_body(EE_UNITS_UB_DO_REPLACE_UNIT, UUHandle(inUnit), UUHandle(inClass), (int64_t)inPlayer, 0, 0)));
}

// (0x6450f1, EEClearUnderBuilding: ee_units uu_world.rs clear_under_building)

// Tells whether an enemy the unit may target is within its reach
bool EEIsEnemyWithinRange(EEComplexUnit* inUnit) { return ee_units_cgq_is_enemy_within_range(UUHandle(inUnit)); }  // cg_query.rs (CBQR)

// Finds the nearest free farm of the unit's player, ring by ring (the ring reached goes back)
EEComplexUnit* EEFindFreeFarm(EEComplexUnit* inUnit, bool inFreeOnly, long* ioRing)
{
    return static_cast<EEComplexUnit*>(UUUnit(ee_units_uu_find_free_farm_at(UUHandle(static_cast<EEUnit*>(inUnit)), inFreeOnly, ioRing)));   // uu_adapt.rs
}

// Runs a functor over a player's units (and its pending units when asked)
void EEForEachUnitOf(EEPlayer* inPlayer, EEUUUnitFunctor* inFunctor, bool inCalamities)
{
    ee_units_ub_body(EE_UNITS_UB_DO_FOR_EACH_UNIT_OF, UUHandle(inPlayer), UUHandle(inFunctor), inCalamities, 0, 0);   // uu_body.rs
}

// (the EETileCoordinate twin of 0x64697b is gone: one tile type)

// Tells whether a unit can heal a target (wounded enough, not an enemy, in its heal set)
// (new: the decision is ee_combat's can_heal over a record of pure reads; the unit set is looked up only for a set
// in the manager's range, which the original indexed unchecked)
bool EECanHeal(EEUnit* inUnit, EEComplexUnit* inTarget) { return ee_units_lf_can_heal(UUHandle(inUnit), UUHandle(inTarget)) != 0; }  // uu_fill.rs

// Returns a unit around the given one whose gather goal works on it (searched in a random rotation)
EEComplexUnit* EEGetWaitingGatherer(EEUnit* inUnit)
{
    return static_cast<EEComplexUnit*>(UUUnit(ee_units_ub_body(EE_UNITS_UB_DO_WAITING_GATHERER, UUHandle(inUnit), 0, 0, 0, 0)));
}

// (0x647125, EEGetEnemiesAround: ee_units uu_world.rs enemies_around)

// (0x647316, EECanReach: ee_combat's can_reach, called by leaves.rs in_contact_range)

// Tells whether a unit is close enough to a target to build or repair it
bool EEIsInWorkReach(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    return ee_units_cgq_in_work_reach(UUHandle(inUnit), UUHandle(inTarget));   // cg_query.rs (CBRW)
}

// Tells whether a unit touches a target (collision spheres; walls count as radius 1)
bool EEIsInContactRange(EEComplexUnit* inUnit, EEUnit* inTarget, float inDistanceSquared)
{
    return ee_units_cgq_in_contact_range(UUHandle(inUnit), UUHandle(inTarget), inDistanceSquared);   // cg_query.rs
}

// (the U2DPoint<long> twin of 0x648ecc is gone: one tile type)


// (the U2DPoint<long> twin of 0x64b556 is gone: one tile type)

// Picks the wall piece (straight, corner or slope) for every tile of a wall line from its neighbours and the
// terrain heights
void EEPickWallPieces(std::vector<EETileCoordinate>& inTiles, std::vector<long>& outCorners, EEMap* inMap)
{
    outCorners.resize(inTiles.size());
    ee_units_uu_pick_wall_pieces(UUHandle(inMap), &inTiles, outCorners.data());
}

// Tells whether a wall blocks a tile line (diagonal steps between two pieces count); reports the last free tile
// Whether a wall blocks the line from (inX1, inY1) to (inX2, inY2) (diagonal steps between two pieces count);
// outX/outY the last free tile
// Tells whether something on the tile line from a unit to a target blocks its shot
// Whether something on the tile line from inUnit to inTarget blocks inUnit's shot (the shot's height runs from
// inUnit's weapon point to the top of inTarget); outX/outY: the last tile reached (inferred)
// With EE_SIM_CAPTURE, a CBLB record: the call's units and tiles, the result, the last tile, the blocker (uu_line.rs)
bool EEIsLineToUnitBlocked(EEComplexUnit* inUnit, EEUnit* inTarget, EEUnit** outArg3, long* outArg4, long* outArg5)
{
    return ee_units_lf_line_blocked(UUHandle(inUnit), UUHandle(inTarget), (uint64_t*)outArg3, outArg4, outArg5) != 0;   // uu_line.rs
}

// The line of fire's statements (ee_sim units uu_line.rs: the walk is ee_combat's line_blocked, the shot's start
// ee_combat's weapon_height over the weapon tag point's data gathered here, CBWP and CBLB records there)
extern "C" int64_t ee_units_lf_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, float (*ioF)[4])
{
    switch (inOp) {
    case EE_UNITS_LF_TILE_X: return UUUnit(inA)->GetTileX();
    case EE_UNITS_LF_TILE_Y: return UUUnit(inA)->GetTileY();
    case EE_UNITS_LF_SKIPS_CHECK: return UUUnit(inA)->GetUnitClass()->mSkipsLineOfFireCheck;
    case EE_UNITS_LF_HIT_POINTS: return UUUnit(inA)->GetHitPoints();
    case EE_UNITS_LF_MELEE: return UUUnit(inA)->GetUnitClass()->mIsMelee;
    case EE_UNITS_LF_UNIT_ID: return (int64_t)UUUnit(inA)->GetUnitID();
    case EE_UNITS_LF_COMPLEX: return UUHandle(UUUnit(inA)->GetComplexUnit());
    case EE_UNITS_LF_LOCATION: memcpy(UbObj<float>(inC), UbObj<EEComplexUnit>(inA)->mPhysicalModel.GetLocation().mMatrix, 16 * sizeof(float)); return 0;
    case EE_UNITS_LF_MODEL: return UUHandle(UUUnit(inA)->GetModel());
    case EE_UNITS_LF_TAG_LIST: return UUHandle(UbObj<EEModel>(inA)->mTagPoints[UbObj<EEComplexUnit>(inB)->mAnimation]);
    case EE_UNITS_LF_FRAME: return (int64_t)UbObj<EEComplexUnit>(inA)->mPhysicalModel.GetFrame();
    case EE_UNITS_LF_WEAPON_INDEX: return (int64_t)UbObj<EEComplexUnit>(inA)->mWeaponTagPoint;
    case EE_UNITS_LF_SCALE: (*ioF)[0] = UbObj<EEComplexUnit>(inA)->mPhysicalModel.GetScale(); return 0;
    case EE_UNITS_LF_NUM_PARTS: return (int64_t)UbObj<EEModel>(inA)->GetNumParts();
    case EE_UNITS_LF_FIND_TURRET: return ee_preload_find_turret_part(UbObj<EEModel>(inA), UbObj<EETagPointList>(inB)->GetTagPoint((unsigned long)inC)->mModel, &(*UbObj<unsigned long>(inD)));
    case EE_UNITS_LF_TURRET_ANGLE: (*ioF)[0] = UbObj<EEComplexUnit>(inA)->mTurretAngles[inC]; return 0;
    case EE_UNITS_LF_LIST_POINT_AT: return UUHandle(*(UbObj<EETagPointList>(inA)->mPoints.begin() + inC));
    case EE_UNITS_LF_LIST_SIZE: return (int64_t)UbObj<EETagPointList>(inA)->mPoints.size();
    case EE_UNITS_LF_LIST_TAG_POINT: return UUHandle(UbObj<EETagPointList>(inA)->GetTagPoint((unsigned long)inC));
    case EE_UNITS_LF_BOX_TOP: *UbObj<GE3DPoint>(inD) = UbObj<EEModel>(inA)->mModels[UbObj<EEComplexUnit>(inB)->mAnimation]->GetBoundingBox((unsigned long)inC).GetTopRightBack(); return 0;
    case EE_UNITS_LF_POINT_LOCAL: *UbObj<GE3DPoint>(inD) = UbObj<EETagPoint>(inA)->mModel->GetTagPoint(UbObj<EETagPoint>(inA)->mIndex, (unsigned long)inC); return 0;
    case EE_UNITS_LF_POINT_MODEL: return UUHandle(UbObj<EETagPoint>(inA)->mModel);
    case EE_UNITS_LF_INTERNAL_TRANSFORM: memcpy(UbObj<float>(inD), UbObj<GEModel>(inA)->GetInternalTransform((unsigned long)inC).mMatrix, 16 * sizeof(float)); return 0;
    case EE_UNITS_LF_PARENT: return UUHandle(UbObj<GEModel>(inA)->GetParentModel());
    case EE_UNITS_LF_STATS_SCALE: (*ioF)[0] = UUUnit(inA)->mStats->GetScale(); return 0;
    case EE_UNITS_LF_BUILT: return UbObj<EEComplexUnit>(inA)->mIsBuilt;
    case EE_UNITS_LF_MAX_HIT_POINTS: return UbObj<EEComplexUnit>(inA)->mStats->mMaxHitPoints;
    case EE_UNITS_LF_Z: (*ioF)[0] = UbObj<EEComplexUnit>(inA)->mPhysicalModel.GetLocation().GetZ(); return 0;
    // uu_fill.rs's
    case EE_UNITS_LF_LAUNCH_SPEED: (*ioF)[0] = UbObj<EEUnitClassUpgrade>(inA)->mLaunchSpeed; return 0;
    case EE_UNITS_LF_PROJECTILE_ACCELERATION: (*ioF)[0] = UbObj<EEUnitClassUpgrade>(inA)->mUnitClass->mProjectileClass->GetAcceleration(); return 0;
    case EE_UNITS_LF_SHOOTS_HIGH: return UbObj<EEComplexUnit>(inA)->mUnitClass->mShootsHigh;
    case EE_UNITS_LF_STATS_OVER_WALLS: return UbObj<EEUnitClassUpgrade>(inA)->mCanFireOverWalls;
    case EE_UNITS_LF_FIRING_HIGH: return UbObj<EEComplexUnit>(inA)->mIsFiringHigh;
    case EE_UNITS_LF_ANIMATION: return (int64_t)UbObj<EEComplexUnit>(inA)->mAnimation;
    case EE_UNITS_LF_WONDER_SET: return (int64_t)UUUnit(inA)->mStats->mWonderUnitSet;
    case EE_UNITS_LF_REPAIRABLE: return UbObj<EEComplexUnit>(inA)->IsRepairable();
    case EE_UNITS_LF_HEAL_THRESHOLD: (*ioF)[0] = UUUnit(inA)->mUnitClass->GetHealThreshold(); return 0;
    case EE_UNITS_LF_PLAYER: return UUHandle(UUUnit(inA)->mPlayer);
    case EE_UNITS_LF_ALLIED: return UUUnit(inA)->mPlayer->mEnemies[UUUnit(inB)->mPlayer->GetPlayerID()] == 0;
    case EE_UNITS_LF_SET_COUNT: return (int64_t)gUnitSetManager.GetSize();
    case EE_UNITS_LF_SET_CONTAINS: return gUnitSetManager.Contains(UbObj<EEComplexUnit>(inA)->mUnitClass->mUnitType, (unsigned long)inC);
    case EE_UNITS_LF_SOUND_FRAME0: return (int64_t)UUUnit(inA)->GetModel()->mSound1Frames[0];
    case EE_UNITS_LF_TAG_POINT_COUNT: return (int64_t)UbObj<EETagPointList>(inA)->GetNumTagPoints();
    case EE_UNITS_LF_SCREEN_ON_MAP: return UbObj<EEFOLocationScreenEnsureContinentAndPathOverlay>(inA)->mB08;
    case EE_UNITS_LF_TILE_AT_LOCATION: return ::gWorld.mMap->GetTileAtLocation(U2DPoint<long>((long)inC, (long)inD)) != NULL;
    case EE_UNITS_LF_SCREEN_CONTINENT: return UbObj<EEFOLocationScreenEnsureContinentAndPathOverlay>(inA)->mContinentID;
    case EE_UNITS_LF_MAP_WIDTH: return (int64_t)gMapWidth;
    case EE_UNITS_LF_MAP_HEIGHT: return (int64_t)gMapHeight;
    case EE_UNITS_LF_CONTINENT_ID: return gContinentManager.GetContinentID((long)inC, (long)inD);
    case EE_UNITS_LF_SCREEN_AREA: return UbObj<EEFOLocationScreenEnsureContinentAndPathOverlay>(inA)->mAreaID;
    case EE_UNITS_LF_SCREEN_WATER: return UbObj<EEFOLocationScreenEnsureContinentAndPathOverlay>(inA)->mUseWaterAreas;
    case EE_UNITS_LF_CONTINENT_OF: return gContinentManager.GetContinent(U2DPoint<long>((long)inC, (long)inD), (long)inB);
    }
    return 0;
}

// Shapes a wall ghost as the piece for a corner
// (the ghost's SetAnimation reads no transformation: its facing is set first)
void EEOrientWallGhost(EEGhostUnit* inGhost, long inCorner, long inRaisedCorners)
{
    inGhost->SetAnimation(EEOrientWallPiece(inGhost->mTransformation, inCorner, inRaisedCorners));
}

// Returns the frame set of a wall piece for a corner (1..12) and sets its facing
long EEOrientWallPiece(GETransformation& ioTransformation, long inCorner, long)
{
    return ee_units_uu_orient_wall_piece(reinterpret_cast<EeGeTransformation*>(&ioTransformation), (int32_t)inCorner);   // uu_ffi.rs
}

// Moves a unit to a transformation, snapping to the closest tile when off the map
void EEUpdateUnitTile(GETransformation* inTransformation, EEComplexUnit* inUnit, long* outX, long* outY)
{
    ee_units_ub_body(EE_UNITS_UB_DO_UPDATE_UNIT_TILE, UUHandle(inTransformation), UUHandle(inUnit), (int64_t)UUHandle(outX), (int64_t)UUHandle(outY), 0);
}

// Converts a unit to another player: a converted priest may hand over a civ power or upgrade, the class
// follows the new owner, the new unit keeps hit points, actions and groups
// inUnit converted to player inPlayer (the priests, the scenario effect): a converted priest or prophet may hand over
// a civilization power (or a random upgrade) the new owner lacks; the class follows the new owner's powers; the new
// unit takes over its hit point share, actions and groups; inArg3 counts it in the statistics
EEComplexUnit* EEConvertUnit(EEComplexUnit* inUnit, long inPlayer, bool inIsConversion)
{
    return static_cast<EEComplexUnit*>(UUUnit(ee_units_ub_body(EE_UNITS_UB_DO_CONVERT_UNIT, UUHandle(inUnit), 0, inPlayer, inIsConversion, 0)));
}

// ---- 0x24f0e9..0x24f932: the rest of this TU's own functions (the unit map's "EEHitPointUpgrader.cpp" slice; the
// template COMDATs that follow, 0x24f932..0x2544e9, are instantiated by this file's functions, so the TU goes on) ----

// (EEHitPointUpgrader, the functor that rescaled the hit points of every unit of a type by a ratio (vtable 0x449bd8):
// Rust's walk, ee_sim units uu_body.rs upgrade_hit_points and hit_point_apply; units round 11)

// Tells whether a unit type can be placed at a location (the editor's own test in free camera mode)
bool EECanPlace(EEPlayer* inPlayer, long inType, GE3DPoint* inLocation, long inX, long inY, bool inSkipPlayerChecks, long inIgnoreOwner)
{
    return ee_units_uu_can_place_at(gFreeCamera, UUHandle(inPlayer), inType, (const float(*)[3])&inLocation->mX, inX, inY, inSkipPlayerChecks,
                                    inIgnoreOwner);   // uu_adapt.rs
}

// (0x64f6f7, EEIsEditorSpacingClear: ee_units uu_world.rs editor_spacing_clear)

// ---- the unit utilities' world (ee_units uu.rs over uu_host.rs): units, classes, players, tiles, maps and continents
// cross as their objects' addresses (a unit as its EEUnit*), the vectors as the C++ objects ----

extern "C" uint8_t ee_units_uu_host_is_edge_tile(int32_t inX, int32_t inY) { return ::gWorld.mMap->IsEdgeTile((long)inX, (long)inY); }
extern "C" int32_t ee_units_uu_host_raised_corners(uint64_t inMap, int32_t inX, int32_t inY)
{
    return (int32_t)((EEMap*)(uintptr_t)inMap)->GetNumRaisedCorners(inX, inY);
}
extern "C" float ee_units_uu_host_mesh_z(uint64_t inMap, int32_t inX, int32_t inY) { return ((EEMap*)(uintptr_t)inMap)->mTerrainMesh.GetZ(inX, inY); }
extern "C" float ee_units_uu_host_mesh_ground(float inX, float inY) { return ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(inX, inY); }

extern "C" uint8_t ee_units_uu_host_passable_by(uint64_t inUnit, uint64_t inBy)
{
    return UUUnit(inUnit)->IsPassableBy(inBy != 0 ? static_cast<EEComplexUnit*>(UUUnit(inBy)) : NULL);
}
extern "C" uint8_t ee_units_uu_host_is_foundation(uint64_t inUnit) { return UUUnit(inUnit)->IsFoundation(); }
extern "C" uint8_t ee_units_uu_host_can_move_on_terrain(uint64_t inClass, uint64_t inUnit, int32_t inTerrain)
{
    return ((EEUnitClass*)(uintptr_t)inClass)->CanMoveOnTerrain(UUUnit(inUnit), inTerrain);
}
extern "C" int32_t ee_units_uu_host_passable_flags(uint64_t inClass, uint64_t inUnit)
{
    return (int32_t)((EEUnitClass*)(uintptr_t)inClass)->GetPassableFlags(UUUnit(inUnit));
}
// (the surrounding tiles: uu_host.rs over ucn_noted.rs and uc_rest.rs, into a list of these)
extern "C" void* ee_units_uu_host_tiles_new() { return new std::vector<U2DPoint<long> >; }
extern "C" void ee_units_uu_host_tiles_delete(void* inTiles) { delete static_cast<std::vector<U2DPoint<long> >*>(inTiles); }
extern "C" uint64_t ee_units_uu_host_container_continent(uint64_t inContainer, uint64_t inUnit)
{
    return UUHandle(UUUnit(inContainer)->mUnitClass->GetContainerContinent(UUUnit(inContainer)->GetComplexUnit(), static_cast<EEComplexUnit*>(UUUnit(inUnit))));
}
// (the tile line: uu_host.rs over uu_line.rs)
extern "C" uint8_t ee_units_uu_host_reserved(uint64_t inGroup, const float* inPoint)
{
    return ((EEActiveGroup*)(uintptr_t)inGroup)->IsReserved(GE3DPoint(inPoint[0], inPoint[1], inPoint[2]));
}
extern "C" void ee_units_uu_host_ground_point(const float* inPoint, float* outPoint)
{
    ::gWorld.mMap->GetGroundPoint(GE3DPoint(inPoint[0], inPoint[1], inPoint[2]), *reinterpret_cast<GE3DPoint*>(outPoint));
}
extern "C" float ee_units_uu_host_terrain_tilt(const float* inMatrix, uint8_t inRoll)
{
    return inRoll ? ::gWorld.mMap->GetTerrainRoll(GETransformation((const float(*)[4])inMatrix))
                  : ::gWorld.mMap->GetTerrainPitch(GETransformation((const float(*)[4])inMatrix));
}
// a player's questions (uu_host.rs PLAYER_*; the unit set's in the player's place)
extern "C" int32_t ee_units_uu_host_player(int32_t inWhich, uint64_t inPlayer, int32_t inValue)
{
    switch (inWhich) {
    case 1: return ((EEPlayer*)(uintptr_t)inPlayer)->IsHuman();
    case 2: return gUnitSetManager.Contains((long)inPlayer, (unsigned long)inValue);
    case 3: return (int32_t)((EEPlayer*)(uintptr_t)inPlayer)->mActiveGroup->GetNumUnits();
    case 4: return ((EEPlayer*)(uintptr_t)inPlayer)->mActiveGroup->GetContinent();
    case 5: return (int32_t)((EEPlayer*)(uintptr_t)inPlayer)->GetCitizenPassability();
    }
    return 0;
}
extern "C" int32_t ee_units_uu_host_facing_quarter(int32_t inX, int32_t inY) { return (int32_t)EEGetFacingQuarter(inX, inY); }
extern "C" int32_t ee_units_uu_host_resource_users(uint64_t inUnit) { return (int32_t)::gWorld.GetResourceUsers(UUUnit(inUnit)); }
extern "C" int32_t ee_units_uu_host_random_long(int32_t inMin, int32_t inMax) { return (int32_t)gPhysics->GetRandom((long)inMin, (long)inMax); }
extern "C" float ee_units_uu_host_random_float(float inMin, float inMax) { return gPhysics->GetRandom(inMin, inMax); }
extern "C" void ee_units_uu_host_set_animation(uint64_t inComplex, int32_t inAnimation)
{
    static_cast<EEComplexUnit*>(UUUnit(inComplex))->SetAnimation(inAnimation, -1, 0);
}
extern "C" void ee_units_uu_host_set_transformation(uint64_t inComplex, const float* inMatrix)
{
    static_cast<EEComplexUnit*>(UUUnit(inComplex))->SetTransformation(GETransformation((const float(*)[4])inMatrix), true);
}

// a tile's questions: flat (the map's), visible to the player, a valid or a space tile of the terrain mesh
extern "C" uint8_t ee_units_uu_host_tile_test(int32_t inWhich, uint64_t inTile, uint64_t inPlayer, int32_t inX, int32_t inY)
{
    switch (inWhich) {
    case 0: return ::gWorld.mMap->IsTileFlat(inX, inY);
    case 1: return UUTile(inTile)->IsVisibleTo((EEPlayer*)(uintptr_t)inPlayer);
    case 2: return ::gWorld.mMap->mTerrainMesh.GetIsValidTile((unsigned long)(uint32_t)inX, (unsigned long)(uint32_t)inY);
    case 3: return ::gWorld.mMap->mTerrainMesh.GetIsSpaceTile((unsigned long)(uint32_t)inX, (unsigned long)(uint32_t)inY);
    }
    return 0;
}
extern "C" void ee_units_uu_host_center_offset(uint64_t inComplex, float* ioPoint)
{
    static_cast<EEComplexUnit*>(UUUnit(inComplex))->AddCenterOffset(ioPoint[0], ioPoint[1]);
}
// a tile list's questions: all in one area of the kind (inWhich 0), any shore tile
extern "C" uint8_t ee_units_uu_host_tiles_test(int32_t inWhich, const long (*inTiles)[2], uint32_t inCount, int32_t inKind)
{
    return inWhich == 0 ? gContinentManager.InSameArea(std::vector<U2DPoint<long> >(UUPoints(inTiles), UUPoints(inTiles) + inCount), inKind) : gContinentManager.HasShoreTile(std::vector<U2DPoint<long> >(UUPoints(inTiles), UUPoints(inTiles) + inCount));
}

// the spot searches' questions (ee_units uu_spots.rs): where a unit dropped onto inUnit stands (0), whether a piece is
// left out (1), the path area at (inX, inY) (2), a gate's two ends (3)
extern "C" int32_t ee_units_uu_host_spot(int32_t inWhich, uint64_t inUnit, int32_t inX, int32_t inY, float* outPoint)
{
    // (the drop point, the path area and the gate's ends: ee_sim units uu_world_ops.rs over these statements)
    switch (inWhich) {
    case 1: return UUUnit(inUnit)->mUnitClass->IsPartLeftOut(static_cast<EEComplexUnit*>(UUUnit(inUnit)));
    case 4: return UUUnit(inUnit)->GetHitPoints();
    case 5: return UUUnit(inUnit)->mComplexUnit != NULL;
    case 6: return UUUnit(inUnit)->GetModel()->mModel != NULL;
    case 7: *(GE3DPoint*)outPoint = UUUnit(inUnit)->GetModel()->mTopPoint; return 0;
    case 8: outPoint[0] = UUUnit(inUnit)->mComplexUnit->mPhysicalModel.GetScale(); return 0;
    case 9: UUUnit(inUnit)->mComplexUnit->mPhysicalModel.GetLocation().Apply(*(GE3DPoint*)outPoint, *(GE3DPoint*)(outPoint + 3)); return 0;
    case 10: UUUnit(inUnit)->GetPosition(*(GE3DPoint*)outPoint); return 0;
    case 12: return ((EEAIPathArea*)(uintptr_t)inUnit)->GetID();
    case 13: EEUnitClassOfKind<EE_CLASS_KIND_GATE>(UUUnit(inUnit)->mUnitClass)->GetLastLinkedCarrier(static_cast<EEComplexUnit*>(UUUnit(inUnit))->GetOutermostCarrier())->GetPosition(*(GE3DPoint*)outPoint); return 0;
    case 13030: *(GE3DPoint*)outPoint = UUUnit(inUnit)->GetModel()->mModels[UUUnit(inUnit)->mComplexUnit->GetAnimation()]->GetBoundingBox(UUUnit(inUnit)->mComplexUnit->mPhysicalModel.GetFrame()).GetTopRightBack(); return 0;   // a wall's model box top at its frame (EEIsTileTopAbove's wall rise)
    case 14: EEUnitClassOfKind<EE_CLASS_KIND_GATE>(UUUnit(inUnit)->mUnitClass)->GetLastLinked(static_cast<EEComplexUnit*>(UUUnit(inUnit))->GetOutermostCarrier())->GetPosition(*(GE3DPoint*)outPoint); return 0;
    }
    return 0;
}

// the vectors of tiles and units
// ---- the round-2 world (ee_units uu_world.rs over uu_host.rs): the questions and writes of the moved bodies ----

// (EEPlanWall's new piece, UUMakeWallPiece: ee_sim units uu_world_ops.rs make_wall_piece over the cases 43-56 below)

extern "C" uint64_t ee_units_uu_host_world(int32_t inWhich, uint64_t inA, uint64_t inB, uint64_t inC, int32_t inX, int32_t inY, float inF)
{
    switch (inWhich) {
    // (the build target, the idle restart, a tile's unit, the ambient clearing, the step aside, the editor's
    // transport and the wall piece: ee_sim units uu_world_ops.rs over the cases from 20)
    case 4: return ((EEPlayer*)(uintptr_t)inA)->CanProduce(((EEUnitClass*)(uintptr_t)inB)->GetTechnology(), 0x20, 1, true, 3, NULL);
    case 8:
        ee_combat_regions_footprint(::gWorld.mMap->mUnitRegions, inX, inY, (int32_t)inC, (uint64_t)(uintptr_t)static_cast<EEComplexUnit*>(UUUnit(inA)), inB != 0);
        return 0;
    case 9: return gWorld.mLoadingSavedGame;
    case 11: static_cast<EEComplexUnit*>(UUUnit(inA))->CanTake(static_cast<EEComplexUnit*>(UUUnit(inB)), (unsigned long)inX); return 0;
    case 12: return UUHandle(static_cast<EEUnit*>(static_cast<EEComplexUnit*>(UUUnit(inA))->mLastResource));
    case 13: {
        EEEffectHandle theHandle;
        ::gWorld.mEffectManager->CreateUnitEffect(static_cast<EEComplexUnit*>(UUUnit(inA)), EEGraphicEffect::Get(kGraphicEffectDisbandUnitEmitter), &theHandle, 1, 0, 0);
        return 0;
    }
    case 14: {   // the deposit (inA the container, inB the unit, inC near, inX the amount, inY the carried load, inF the bonus)
        EEComplexUnit* theCarrier = static_cast<EEComplexUnit*>(UUUnit(inB));
        ee_units_uu_note_event((int32_t)inX, (int32_t)((long)inC), (int32_t)3);   // TEST ONLY (UUTL)
        static_cast<EEComplexUnit*>(UUUnit(inA))->mPlayer->AddResource(theCarrier->mResourceType, inX);
        // TEST ONLY (docs/AI_RESEARCH.md §8.51, EE_AI_TRACE with EE_AI_EXP deposits): each deposit of a computer player,
        // to time a gatherer's loads (trace only: nothing here changes the game)
        if (gCPTraceOn) {
            EEComputerPlayer* theComputer = dynamic_cast<EEComputerPlayer*>(static_cast<EEComplexUnit*>(UUUnit(inA))->mPlayer);
            if (theComputer != NULL && EECPExperiment(theComputer, "deposits"))
                EE_CP_TRACE(theComputer, "deposit", "unit=%lu cls=%ld res=%lu carried=%ld amount=%ld bonus=%.3f store=%lu t=%lu",
                    (unsigned long)theCarrier->GetUnitID(), (long)theCarrier->mUnitClass->mUnitType, theCarrier->mResourceType, (long)inY,
                    (long)inX, (double)inF, (unsigned long)static_cast<EEComplexUnit*>(UUUnit(inA))->GetUnitID(), (unsigned long)gPhysics->mGameTime);
        }
        return 0;
    }
    case 15: static_cast<EEComplexUnit*>(UUUnit(inA))->mPlayer->mSavedGroups.Reset(inX, inY); return 0;
    case 18: return EEUnitClassOfKind<EE_CLASS_KIND_BASIC_CITIZEN>(UUUnit(inA)->GetUnitClass())->HasGatherPoint(static_cast<EEComplexUnit*>(UUUnit(inA)), UUUnit(inB));   // (its UCBD note)
    case 20: return (uint64_t)(int64_t)UUId(UUUnit(inA));
    case 21: static_cast<EEComplexUnit*>(UUUnit(inA))->AddBuildTarget(UUUnit(inB)); return 0;
    case 22: static_cast<EEComplexUnit*>(UUUnit(inA))->ClearGoals(0); return 0;
    case 23: static_cast<EEComplexUnit*>(UUUnit(inA))->StartAction(kActionIdle); return 0;
    case 24: static_cast<EEComplexUnit*>(UUUnit(inA))->mUnitClass->ActionChanged(static_cast<EEComplexUnit*>(UUUnit(inA))); return 0;
    case 25: UUTile(inA)->mUnits = UUTile(inA)->AddUnit(UUUnit(inB), UUTile(inA)->mUnits); return 0;
    case 26: UUTile(inA)->RemoveUnit(UUUnit(inB)); return 0;
    case 27: return EEUnitClassOfKind<EE_CLASS_KIND_AMBIENT>(UUUnit(inA)->mUnitClass) != NULL;
    case 28: return (uint64_t)(int64_t)UUUnit(inA)->mUnitClass->mUnitType;
    case 29: return UUUnit(inA)->mUnitClass->mIsDeletable;
    case 30: return (uint64_t)(int64_t)UUUnit(inA)->mResourceAmount;
    case 31: UUUnit(inA)->mResourceAmount = 0; return 0;
    case 32: UUUnit(inA)->Kill(false, false, false); return 0;
    case 33: UUUnit(inA)->mPlayer->KillUnit(UUUnit(inA), false); return 0;
    case 34: static_cast<EEComplexUnit*>(UUUnit(inA))->StepAside((EEPlayer*)(uintptr_t)inB, (EEUnitClass*)(uintptr_t)inC, inX, inY, inF != 0.0f); return 0;
    case 35: return (uint64_t)(int64_t)gUIManager->GetScenarioEditorForm()->GetUnitUnderMouse();
    case 36: return UUHandle(static_cast<EEUnit*>(::gWorld.GetUnit(inX)));
    case 37: return UUHandle(UUUnit(inA)->GetComplexUnit());
    case 38: return UUHandle(static_cast<EEComplexUnit*>(UUUnit(inA))->GetPlayer());
    case 39: return (uint64_t)(int64_t)UUUnit(inA)->mUnitClass->mTransportCapacity;
    case 40: return (uint64_t)(int64_t)static_cast<EEComplexUnit*>(UUUnit(inA))->mCargoRoom;
    case 41: return (uint64_t)(int64_t)gUnitClassTable[inX]->mWeight;
    case 42: return gUnitClassTable[inX]->CanEnter(static_cast<EEComplexUnit*>(UUUnit(inA))->mUnitClass);
    case 43: return UUHandle(((EEUnitClass*)(uintptr_t)inA)->CreateUnit(inX, inY, ((EEPlayer*)(uintptr_t)inB)->GetPlayerID(), inF != 0.0f, NULL));
    case 44: return (uint64_t)(int64_t)::gWorld.GetMap()->GetNumRaisedCorners(inX, inY);
    case 45: UUUnit(inA)->SetHitPoints(inX); return 0;
    case 46: return (uint64_t)UUUnit(inA)->mTile->GetVisibility(((EEPlayer*)(uintptr_t)inB)->GetPlayerID());
    case 47: gDoppelgangerManager.AddForUnseenUnit(static_cast<EEComplexUnit*>(UUUnit(inA))); return 0;
    case 48: ((EEPlayer*)(uintptr_t)inB)->Pay(((EEPlayer*)(uintptr_t)inB)->GetTechTree()->Get(((EEUnitClass*)(uintptr_t)inA)->GetTechnology()), 0x20, 1.0f); return 0;
    case 49: return static_cast<EEComplexUnit*>(UUUnit(inA))->mUnitClass->mIsWall && !static_cast<EEComplexUnit*>(UUUnit(inA))->mUnitClass->mIsTower;
    case 50: static_cast<EEComplexUnit*>(UUUnit(inA))->GetPlayer()->AddConstructionSite(static_cast<EEComplexUnit*>(UUUnit(inA))); return 0;
    case 51: return (uint64_t)(int64_t)UUUnit(inA)->GetStats()->mMaxHitPoints;
    case 52: static_cast<EEComplexUnit*>(UUUnit(inA))->SetBuilt(1, 0, 1); return 0;
    case 53: return ((EEPlayer*)(uintptr_t)inB)->GetTechTree()->Get(kTechWallSpace)->GetIsAvailable();
    case 54: EECreateSpaceWallEnergyWall(static_cast<EEComplexUnit*>(UUUnit(inA))); return 0;
    case 55: return static_cast<EEComplexUnit*>(UUUnit(inA))->mIsPaidFor;
    case 56: static_cast<EEComplexUnit*>(UUUnit(inA))->mIsPaidFor = true; return 0;
    case 57: return (uint64_t)(int64_t)UUUnit(inA)->mUnitClass->mCategory;
    case 58: return UUUnit(inA)->mComplexUnit->mIsBuilt;
    case 59: return UUHandle(gContinentManager.GetPathAreaAt(U2DPoint<long>(inX, inY)));
    }
    return 0;
}

extern "C" float ee_units_uu_host_world_f(int32_t inWhich, uint64_t inA, uint64_t inB, float inF)
{
    switch (inWhich) {
    case 1: return ee_units_cgq_in_gather_reach(inF, inA, inB) ? 1.0f : 0.0f;   // EEIsInGatherReach (cg_query.rs)
    // (EEIsTileTopAbove's wall rise: ee_sim units uu_world_ops.rs wall_rise over the cases from 5)
    case 4: return ::gWorld.GetGameVariant()->mDepositFactor;
    case 6: return UUUnit(inA)->mStats->GetScale();
    case 7: return (float)UUUnit(inA)->mComplexUnit->mHitPoints;
    case 8: return (float)UUUnit(inA)->mComplexUnit->mStats->mMaxHitPoints;
    }
    return 0.0f;
}

// (a tile vector's reads: uu_host.rs tiles_of, in place)
extern "C" void ee_units_uu_host_tiles_set(void* outTiles, const long (*inTiles)[2], uint32_t inCount)
{
    static_cast<std::vector<U2DPoint<long> >*>(outTiles)->assign(UUPoints(inTiles), UUPoints(inTiles) + inCount);
}
extern "C" uint32_t ee_units_uu_host_units_len(const void* inUnits) { return (uint32_t)((const std::vector<EEUnit*>*)inUnits)->size(); }
// (a handle is its unit's address: the vector's pointers are the handles' words)
static_assert(sizeof(EEUnit*) == sizeof(uint64_t), "a unit handle is its pointer");
extern "C" void ee_units_uu_host_units_get(const void* inUnits, uint64_t* outUnits)
{
    std::transform(((const std::vector<EEUnit*>*)inUnits)->begin(), ((const std::vector<EEUnit*>*)inUnits)->end(), outUnits, UUHandle);
}
extern "C" void ee_units_uu_host_units_set(void* outUnits, const uint64_t* inUnits, uint32_t inCount)
{
    ((std::vector<EEUnit*>*)outUnits)->assign(reinterpret_cast<EEUnit* const*>(inUnits), reinterpret_cast<EEUnit* const*>(inUnits) + inCount);
}

// Makes the units on the footprint of a unit of inClass placed at inTile (or of inUnit) step aside for it
void EEPlaceUnit(EEComplexUnit* inUnit, EEUnitClass* inClass, EEPlayer* inPlayer, const U2DPoint<long>& inTile, bool inArg)
{
    ee_units_uu_step_aside((uint64_t)(uintptr_t)inUnit, (uint64_t)(uintptr_t)inClass, (uint64_t)(uintptr_t)inPlayer, inTile.mX, inTile.mY, inArg);
}

// ---- the C++ of the unit utilities' Rust bodies (ee_units uu_body.rs; EE_UNITS_UB_*): one statement of the
// original bodies each, in their order; inA, inB objects (a unit as its EEUnit*), inC, inD, inE values, ioF floats

static_assert(sizeof(GE3DPoint) == 3 * sizeof(float), "a GE3DPoint is three floats");
static_assert(sizeof(GEVector) == 3 * sizeof(float), "a GEVector is three floats");

// the bodies' scratch: EEConvertUnit's strings, upgrade list and cost; EEFn24E68F's attribute, strings and string id;
// the unit and tile lists; EEPreviewWall's corners; the helicopter priest's direction; EEUpdateUnitTile's waypoint
// point; EEFindTargetNear's area; EEUpgradeHitPoints' functor
struct EEUbStrings {
    UWideString mFormat;
    UWideString mMessage;
    UWideString mName;
};
static EEUbStrings sUbStrings;
static std::vector<EECivUpgrade> sUbUpgrades;
static unsigned long sUbCost;
static unsigned long sUbAttribute[2];
static long sUbStringID;
static UWideString sUbCategoryString;
static UWideString sUbAttributeString;
static std::vector<EEUnit*> sUbUnits;
static std::vector<U2DPoint<long> > sUbTiles;
static std::vector<long> sUbCorners;
static GEVector sUbDirection;
static GE3DPoint sUbPoint;
static U2DRectangle<long> sUbArea;

// EEMovePassengers' place for the unloaded cargo (off the map)
static const float kUbOffMap[4][4] = {{1.0f, 0.0f, 0.0f, -10000.0f}, {0.0f, 1.0f, 0.0f, -10000.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}};

// EEFindNewTargetNearOld's search near the old target: EEFindNewTarget (uu_search.rs) with the original's local screen,
// here a temporary that lives through the call
static uint64_t UbScreen(const EEFOFindNewTargetScreenBase& inScreen) { return UUHandle(&inScreen); }
static bool UbFindNewTarget(EEComplexUnit* inTarget, EEComplexUnit* inUnit, long inX, long inY, long inRadius)
{
    return ee_units_sr_find_new_target(UbScreen(EEFOFindNewTargetNearOldTarget(inTarget, 25.0f)), UUHandle(inUnit->GetLoadGoal()), UUHandle(inUnit), inX, inY, inRadius, true, 0) != 0;
}

extern "C" int64_t ee_units_ub_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, int64_t inE, float (*ioF)[4])
{
    switch (inOp) {
    // EEConvertUnit's
    case EE_UNITS_UB_PLAYER_AT: return UUHandle(::gWorld.mPlayers[inC]);
    case EE_UNITS_UB_UNIT_CLASS: return UUHandle(UUUnit(inA)->GetUnitClass());
    case EE_UNITS_UB_CLASS_TYPE: return UbObj<EEUnitClass>(inA)->mUnitType;
    case EE_UNITS_UB_HAS_POWER: return gCivPowersManager.HasPower(UbObj<EEPlayer>(inA), (unsigned long)inC);
    case EE_UNITS_UB_IS_BUILT: return UUUnit(inA)->GetComplexUnit()->mIsBuilt;
    case EE_UNITS_UB_STRINGS_RESET: sUbStrings = EEUbStrings(); return 0;
    case EE_UNITS_UB_NUM_POWERS: return (unsigned long)ee_tech_civ_power_count();
    case EE_UNITS_UB_UNIT_HAS_POWER: return gCivPowersManager.HasPower(UUUnit(inA)->GetPlayer(), (unsigned long)inC);
    case EE_UNITS_UB_MAKE_POWER_UPGRADE: return (unsigned long)ee_tech_civ_power_upgrade((uint32_t)(EECivilizationManager::PowerLevel((unsigned long)inC)));
    case EE_UNITS_UB_POWER_NAME: gCivPowersManager.GetName((unsigned long)inC, sUbStrings.mName); return 0;
    case EE_UNITS_UB_UPGRADES_COPY: sUbUpgrades = UUUnit(inA)->GetPlayer()->mCivilization.GetPurchasedUpgrades(); return 0;
    case EE_UNITS_UB_UPGRADES_SHUFFLE: EEAIRandomShuffle(sUbUpgrades.begin(), sUbUpgrades.end()); return 0;
    case EE_UNITS_UB_UPGRADES_COUNT: return (int64_t)sUbUpgrades.size();
    case EE_UNITS_UB_HAS_UPGRADE: return EEHasCivilizationUpgrade(&UbObj<EEPlayer>(inA)->mCivilization, (EECivUpgrade)inC);
    case EE_UNITS_UB_UPGRADE_AT: return (int64_t)sUbUpgrades[(size_t)inC];
    case EE_UNITS_UB_UPGRADE_NAME: return EEFn24E68F((EECivUpgrade)inC, sUbStrings.mName);
    case EE_UNITS_UB_AVAILABLE_POINTS: return UbObj<EEPlayer>(inA)->mCivilization.GetAvailablePoints();
    case EE_UNITS_UB_CAN_AFFORD: return UbObj<EEPlayer>(inA)->mCivilization.CanAfford((EECivUpgrade)inC, &sUbCost, NULL);
    case EE_UNITS_UB_SET_POINTS_AND_MAX: UbObj<EEPlayer>(inA)->mCivilization.SetPointsAndMax(UbObj<EEPlayer>(inA)->mCivilization.GetMaxPoints() + sUbCost); return 0;
    case EE_UNITS_UB_BUY: ee_gamecpp_civ_buy(&UbObj<EEPlayer>(inA)->mCivilization, (EECivUpgrade)inC, UbObj<EEPlayer>(inA)->GetPlayerID(), true); return 0;
    case EE_UNITS_UB_SET_POINTS: UbObj<EEPlayer>(inA)->mCivilization.SetPoints((long)inC); return 0;
    case EE_UNITS_UB_CURRENT_PLAYER: return UUHandle(::gWorld.GetCurrentPlayer());
    case EE_UNITS_UB_ADAPTATION_FORMAT: gResource->GetString(IDS_ACQUIRED_THROUGH_ADAPTATION, sUbStrings.mFormat); return 0;
    case EE_UNITS_UB_ADAPTATION_MESSAGE: sprintf(sUbStrings.mMessage, sUbStrings.mFormat, (const char*)sUbStrings.mName); return 0;
    case EE_UNITS_UB_ADAPTATION_CHAT: gUserInterface->AddChatLogEntry(UUUnit(inA)->GetPlayer()->mColor, UUUnit(inA)->GetPlayer()->mName, sUbStrings.mMessage, gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 2, 0); return 0;
    case EE_UNITS_UB_CLASS_OF_TYPE: return UUHandle(gUnitClassTable[inC]);
    case EE_UNITS_UB_IS_CRUSADER: return UbObj<EEUnitClass>(inA)->IsCrusader();
    case EE_UNITS_UB_HIT_FRACTION: return (int64_t)UFloatBits((float)UUUnit(inA)->GetHitPoints() / (float)UUUnit(inA)->mStats->mMaxHitPoints);
    case EE_UNITS_UB_DESELECT: UUUnit(inA)->GetPlayer()->DeselectUnit(UUUnit(inA)->GetComplexUnit()); return 0;
    case EE_UNITS_UB_GROUP_CONTAINS: return UUUnit(inA)->GetPlayer()->mGroups[inC]->Contains(UUUnit(inA)->GetComplexUnit());
    case EE_UNITS_UB_GROUP_REMOVE: UUUnit(inA)->GetPlayer()->mGroups[inC]->RemoveUnit(UUUnit(inA)->GetComplexUnit()); return 0;
    case EE_UNITS_UB_SET_CLASS_MEDIA: UbObj<EEPlayer>(inA)->SetUnitClassMedia(UUUnit(inB)->GetUnitClass()->mUnitType, 0x7f); return 0;
    case EE_UNITS_UB_UNLOAD_CLASS_MEDIA: UbObj<EEPlayer>(inA)->UnloadUnitClassMedia(UUUnit(inB)->GetUnitClass()->mUnitType); return 0;
    case EE_UNITS_UB_COMPLEX_OF: return UUHandle(UUUnit(inA)->GetComplexUnit());
    case EE_UNITS_UB_CLEAR_GARRISON: UbObj<EEComplexUnit>(inA)->ClearGarrison(); return 0;
    case EE_UNITS_UB_IS_LAND_BUILDING: return UbObj<EEUnitClass>(inA)->IsLandBuilding();
    case EE_UNITS_UB_IS_WALL: return UbObj<EEUnitClass>(inA)->mIsWall;
    case EE_UNITS_UB_IS_TOWER: return UbObj<EEUnitClass>(inA)->mIsTower;
    case EE_UNITS_UB_START_ACTION: UbObj<EEComplexUnit>(inA)->StartAction((long)inC); return 0;
    case EE_UNITS_UB_BUILD_QUEUE_SIZE: return UbObj<EEComplexUnit>(inA)->GetBuildQueueSize();
    case EE_UNITS_UB_BUILD_QUEUE_CLEAR: UbObj<EEComplexUnit>(inA)->mBuildQueue.clear(); return 0;
    case EE_UNITS_UB_CATEGORY: return UbObj<EEUnitClass>(inA)->mCategory;
    case EE_UNITS_UB_SET_ANIMATION_OF: UbObj<EEComplexUnit>(inA)->SetAnimation(UUUnit(inB)->GetComplexUnit()->mAnimation, -1, 0); return 0;
    case EE_UNITS_UB_START_COMMAND_OF:
        UbObj<EEComplexUnit>(inA)->StartAction(UUUnit(inB)->GetComplexUnit()->mCommand != NULL ? UUUnit(inB)->GetComplexUnit()->mCommand->mType : 0);
        return 0;
    case EE_UNITS_UB_QUEUE_SWAP: ee_physics_queue_unit_swap(gPhysics, UbObj<EEComplexUnit>(inA), UbObj<EEComplexUnit>(inB)); return 0;
    case EE_UNITS_UB_PREVIOUS_PART: return UUHandle(UbObj<EEComplexUnit>(inA)->mPreviousPart);
    case EE_UNITS_UB_GATE_REPLACED: UUUnit(inA)->GetUnitClass()->GateReplaced(UUUnit(inA), UbObj<EEComplexUnit>(inB)); return 0;
    case EE_UNITS_UB_CLEAR_ALL_GOAL_QUEUES: UbObj<EEComplexUnit>(inA)->ClearAllGoalQueues(); return 0;
    case EE_UNITS_UB_CLEAR_BUILD_TARGETS: UbObj<EEComplexUnit>(inA)->ClearBuildTargets(); return 0;
    case EE_UNITS_UB_PLAY_EFFECT: UbObj<EEComplexUnit>(inA)->PlayEffect((long)inC); return 0;
    case EE_UNITS_UB_PRODUCTION_TIME: return UbObj<EEComplexUnit>(inA)->mProductionTime;
    case EE_UNITS_UB_CLEAR_PRODUCTION_TIME: UbObj<EEComplexUnit>(inA)->mProductionTime = 0; return 0;
    case EE_UNITS_UB_ACTION_CHANGED: UbObj<EEUnitClass>(inA)->ActionChanged(UbObj<EEComplexUnit>(inB)); return 0;
    case EE_UNITS_UB_KILL: UbObj<EEComplexUnit>(inA)->Kill((inC & 1) != 0, (inC & 2) != 0, (inC & 4) != 0); return 0;
    case EE_UNITS_UB_STATS_RESET: UbObj<EEPlayer>(inA)->mSavedGroups.Reset((long)inC, 1); return 0;
    case EE_UNITS_UB_ADD_DEAD_STATISTIC: return EEAddDeadUnitStatistic(UbObj<EEComplexUnit>(inA));
    case EE_UNITS_UB_UNIT_STATS_LOWER: UUUnit(inA)->GetPlayer()->mSavedGroups.Lower((long)inC, 1); return 0;
    case EE_UNITS_UB_UNIT_STATS_RESET: UUUnit(inA)->GetPlayer()->mSavedGroups.Reset((long)inC, 1); return 0;
    case EE_UNITS_UB_HAS_DAMAGE_EFFECT: return UbObj<EEUnitClass>(inA)->mDamageEffect != NULL;
    case EE_UNITS_UB_DAMAGE_PARAM2: return (int64_t)UFloatBits(UbObj<EEUnitClass>(inA)->mDamageEffect->mParam2);
    case EE_UNITS_UB_CREATE_DAMAGE_EFFECT:
        gWorld.GetEffectManager()->CreateUnitEffect(UbObj<EEComplexUnit>(inA), UbObj<EEComplexUnit>(inA)->GetUnitClass()->mDamageEffect, NULL, 0, 0, 0);
        return 0;
    // EEMakeUnitCopy's, EEReplaceUnit's, EEMakeBuilding's
    case EE_UNITS_UB_TILE_X: return UUUnit(inA)->GetTileX();
    case EE_UNITS_UB_TILE_Y: return UUUnit(inA)->GetTileY();
    case EE_UNITS_UB_IS_FOUNDATION: return UUUnit(inA)->IsFoundation();
    case EE_UNITS_UB_CREATE_UNIT: return UUHandle(UbObj<EEUnitClass>(inA)->CreateUnit((long)inC, (long)inD, (unsigned long)inE, inB != 0, NULL));
    case EE_UNITS_UB_COPY_FROM: UUUnit(inA)->CopyFrom(UUUnit(inB)); return 0;
    case EE_UNITS_UB_TILE_NODE: return UUHandle(UUUnit(inA)->mTileNode);
    case EE_UNITS_UB_SET_BUILT: UbObj<EEComplexUnit>(inA)->SetBuilt((long)inC, (long)inD, (long)inE); return 0;
    case EE_UNITS_UB_SET_HIT_POINTS: UUUnit(inA)->SetHitPoints((long)inC); return 0;
    case EE_UNITS_UB_MAX_HIT_POINTS: return UUUnit(inA)->mStats->mMaxHitPoints;
    case EE_UNITS_UB_PLAYER_OF: return UUHandle(UUUnit(inA)->GetPlayer());
    case EE_UNITS_UB_GROUP_AT: return UUHandle(UbObj<EEPlayer>(inA)->mGroups[inC]);
    case EE_UNITS_UB_GROUP_NUM_UNITS: return UbObj<EEGroup>(inA)->GetNumUnits();
    case EE_UNITS_UB_GROUP_UNIT: return UUHandle(UbObj<EEGroup>(inA)->GetUnit((unsigned long)inC));
    case EE_UNITS_UB_GROUP_ADD: UbObj<EEGroup>(inA)->Add(UbObj<EEComplexUnit>(inB)); return 0;
    case EE_UNITS_UB_DROP_UNSUITED_GOALS: UbObj<EEComplexUnit>(inA)->DropUnsuitedGoals(0); return 0;
    case EE_UNITS_UB_TECH_AVAILABLE: return UbObj<EEPlayer>(inA)->mTechnologies->Get((long)inC)->GetIsAvailable();
    case EE_UNITS_UB_NEXT_PART: return UUHandle(UbObj<EEComplexUnit>(inA)->mNextPart);
    case EE_UNITS_UB_ENERGY_WALL: gWorld.GetEffectManager()->CreateUnitEffect(UbObj<EEComplexUnit>(inA), EEGraphicEffect::Get(inC != 0 ? kGraphicEffectSpaceGateEnergyWall : kGraphicEffectSpaceWallEnergyWall), &UbObj<EEComplexUnit>(inA)->mFireEffect, 0, 0, 0); return 0;
    case EE_UNITS_UB_HIT_POINTS: return UUUnit(inA)->mHitPoints;
    case EE_UNITS_UB_TILE_VISIBILITY: return UUUnit(inA)->mTile->GetVisibility(UbObj<EEPlayer>(inB)->GetPlayerID());
    case EE_UNITS_UB_ADD_FOR_UNSEEN: gDoppelgangerManager.AddForUnseenUnit(UbObj<EEComplexUnit>(inA)); return 0;
    case EE_UNITS_UB_PAY_CLASS: UbObj<EEPlayer>(inA)->Pay(UbObj<EEPlayer>(inA)->GetTechTree()->Get(UbObj<EEUnitClass>(inB)->GetTechnology()), 0x20, 1.0f); return 0;
    case EE_UNITS_UB_IS_PAID_FOR: return UbObj<EEComplexUnit>(inA)->mIsPaidFor;
    case EE_UNITS_UB_SET_PAID_FOR: UbObj<EEComplexUnit>(inA)->mIsPaidFor = true; return 0;
    case EE_UNITS_UB_ADD_PENDING_UNITS: UbObj<EEPlayer>(inA)->AddPendingUnits(false, true); return 0;
    case EE_UNITS_UB_FLUSH_PLACEMENTS: ::gWorld.FlushQueuedPlacements(); return 0;
    case EE_UNITS_UB_REPLANT_FARMS: gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageReplantFarms, {UUUnit(inA)->mUnitID}), kSlotNone); return 0;
    case EE_UNITS_UB_PLAYER_ID: return UbObj<EEPlayer>(inA)->GetPlayerID();
    // EEReplaceQueuedTechnology's
    case EE_UNITS_UB_CLASS_TECHNOLOGY: return UbObj<EEUnitClass>(inA)->mTechnology;
    case EE_UNITS_UB_TECHNOLOGY: return UUHandle(UbObj<EEPlayer>(inA)->mTechnologies->Get((long)inC));
    case EE_UNITS_UB_TECH_NUM_CLASSES: return UbObj<EETechnology>(inA)->GetNumUnitClasses();
    case EE_UNITS_UB_TECH_CLASS: return UbObj<EETechnology>(inA)->GetUnitClass((unsigned long)inC);
    case EE_UNITS_UB_UNITS_OF_TYPE: return ee_units_uu_player_units_of_type((long)inC, UbObj<EEPlayer>(inA)->GetPlayerID(), &sUbUnits);
    case EE_UNITS_UB_LIST_UNIT: return UUHandle(sUbUnits[(size_t)inC]);
    case EE_UNITS_UB_REPLACE_RESEARCH: UbObj<EEComplexUnit>(inA)->ReplaceResearch((long)inC, (long)inD); return 0;
    // EEWorkFarm's, EERetargetAttack's, EEFindNewTargetNearOld's
    case EE_UNITS_UB_SET_GATHER_GOAL: UbObj<EEComplexUnit>(inA)->SetGoal(EEUGRust::NewGather(UbObj<EEComplexUnit>(inB), NULL, false), 0, 0); return 0;
    case EE_UNITS_UB_SET_REPAIR_GOAL: UbObj<EEComplexUnit>(inA)->SetGoal(EEUGRust::NewBuildRepair(UbObj<EEComplexUnit>(inB)->GetPlayer(), UbObj<EEComplexUnit>(inB), false), 0, 0); return 0;
    case EE_UNITS_UB_ATTACK_TARGET: return UUHandle(static_cast<EEUGRust*>(UbObj<EEUnitGoal>(inA))->GetAttackTarget()->mComplexUnit);
    case EE_UNITS_UB_GOAL_REPLACE_UNIT: UbObj<EEUnitGoal>(inA)->ReplaceUnit(UbObj<EEComplexUnit>(inB), UbObj<EEComplexUnit>((uint64_t)inC), NULL); return 0;
    case EE_UNITS_UB_CLEAR_COLLISION_COUNT: UbObj<EEComplexUnit>(inA)->mCollisionResolutionCount = 0.0f; return 0;
    case EE_UNITS_UB_CLEAR_BLOCKING: UbObj<EEComplexUnit>(inA)->SetBlockingUnit(NULL); return 0;
    case EE_UNITS_UB_COLLISION_STATE: return UbObj<EEComplexUnit>(inA)->mCollisionState;
    case EE_UNITS_UB_CLEAR_COLLISION_STATE: UbObj<EEComplexUnit>(inA)->mCollisionState = 0; return 0;
    case EE_UNITS_UB_DROP_PATH: UbObj<EEComplexUnit>(inA)->DropPath(); return 0;
    case EE_UNITS_UB_IGNORE_TARGET: UbObj<EEComplexUnit>(inA)->IgnoreTarget(UbObj<EEComplexUnit>(inB)); return 0;
    case EE_UNITS_UB_FIND_NEW_TARGET: return UbFindNewTarget(UbObj<EEComplexUnit>(inA), UbObj<EEComplexUnit>(inB), (long)inC, (long)inD, (long)inE);
    case EE_UNITS_UB_NO_TARGET_TRACE: EEDebugTrace(">>> SHAWN %d could not find a target.\n", UbObj<EEComplexUnit>(inA)->GetUnitClass()->GetName()); return 0;
    case EE_UNITS_UB_OUTERMOST_CARRIER: return UUHandle(UbObj<EEComplexUnit>(inA)->GetOutermostCarrier());
    case EE_UNITS_UB_PART_LEFT_OUT: return UbObj<EEUnitClass>(inA)->IsPartLeftOut(UbObj<EEComplexUnit>(inB));
    case EE_UNITS_UB_TARGET_IGNORED: return UbObj<EEComplexUnit>(inA)->IsTargetIgnored(UbObj<EEComplexUnit>(inB), inC != 0);
    case EE_UNITS_UB_LOAD_ATTACK_GOAL: return UUHandle(static_cast<EEUnitGoal*>(EEAttackUnitCast(UbObj<EEComplexUnit>(inA)->GetLoadGoal())));
    case EE_UNITS_UB_LAST_LINKED_CARRIER: return UUHandle(EEUnitClassOfKind<EE_CLASS_KIND_GATE>(UbObj<EEUnitClass>(inA))->GetLastLinkedCarrier(UbObj<EEComplexUnit>(inB)));
    case EE_UNITS_UB_LAST_LINKED: return UUHandle(EEUnitClassOfKind<EE_CLASS_KIND_GATE>(UbObj<EEUnitClass>(inA))->GetLastLinked(UbObj<EEComplexUnit>(inB)));
    case EE_UNITS_UB_DISTANCE_SQUARED_TO: return (int64_t)UFloatBits(UbObj<EEComplexUnit>(inA)->DistanceSquaredTo(UbObj<EEComplexUnit>(inB)));
    // EEGetHelicopterPriestDestination's
    case EE_UNITS_UB_CALAMITY_GOAL: return UUHandle(static_cast<EEUnitGoal*>(EECastCalamityGoalCast(UbObj<EEComplexUnit>(inA)->GetGoal())));
    case EE_UNITS_UB_GOAL_LOCATION: static_cast<EEUGRust*>(UbObj<EEUnitGoal>(inA))->GetGoalLocation(*reinterpret_cast<GE3DPoint*>((*ioF))); return 0;
    case EE_UNITS_UB_CALAMITY_TYPE: return static_cast<EEUGRust*>(UbObj<EEUnitGoal>(inA))->GetCalamityType();
    case EE_UNITS_UB_UNIT_TECHNOLOGY: return UUHandle(UUUnit(inA)->mPlayer->mTechnologies->Get((long)inC));
    case EE_UNITS_UB_CALAMITY_RANGE:
        return USingleton<EECalamityManager>::GetInstance()->GetCalamityClass((EECalamityEnum)UbObj<EETechnology>(inA)->mData->mCalamity)->mRange;
    case EE_UNITS_UB_POSITION: UbObj<EEComplexUnit>(inA)->GetPosition(*reinterpret_cast<GE3DPoint*>((*ioF))); return 0;
    case EE_UNITS_UB_DIRECTION_SET: sUbDirection = *reinterpret_cast<GEVector*>((*ioF)); return 0;
    case EE_UNITS_UB_DIRECTION_NORMALIZE: sUbDirection.Normalize(); return 0;
    case EE_UNITS_UB_DIRECTION_SCALE: sUbDirection.Scale((*ioF)[0], *reinterpret_cast<GEVector*>((*ioF))); return 0;
    case EE_UNITS_UB_SET_POINT: *UbObj<GE3DPoint>(inA) = *reinterpret_cast<GE3DPoint*>((*ioF)); return 0;
    case EE_UNITS_UB_POINT: *reinterpret_cast<GE3DPoint*>((*ioF)) = *UbObj<GE3DPoint>(inA); return 0;
    // EEUpdateUnitTile's
    case EE_UNITS_UB_TRANSFORMATION_POSITION: UbObj<GETransformation>(inA)->GetPosition(*reinterpret_cast<GE3DPoint*>((*ioF))); return 0;
    case EE_UNITS_UB_TRANSFORMATION_MATRIX: memcpy((void*)(uintptr_t)inC, UbObj<GETransformation>(inA)->mMatrix, sizeof(UbObj<GETransformation>(inA)->mMatrix)); return 0;
    case EE_UNITS_UB_LOCATION_MATRIX: memcpy((void*)(uintptr_t)inC, UbObj<EEComplexUnit>(inA)->mPhysicalModel.GetLocation().mMatrix, 64); return 0;
    case EE_UNITS_UB_SET_LONG: *UbObj<long>(inA) = (long)inC; return 0;
    case EE_UNITS_UB_READ_LONG: return *UbObj<long>(inA);
    case EE_UNITS_UB_TILE_EXISTS: return ::gWorld.mMap->GetTile((long)inC, (long)inD) != NULL;
    case EE_UNITS_UB_SET_TRANSFORMATION: UbObj<EEComplexUnit>(inA)->SetTransformation(*UbObj<GETransformation>(inB), true); return 0;
    case EE_UNITS_UB_CLOSEST_TILE: ::gWorld.mMap->GetClosestTile(reinterpret_cast<GE3DPoint*>((*ioF)), UbObj<long>((uint64_t)inC), UbObj<long>((uint64_t)inD)); return 0;
    case EE_UNITS_UB_SET_LOCATION: UbObj<EEComplexUnit>(inA)->mPhysicalModel.SetLocation(*UbObj<GETransformation>(inB)); return 0;
    case EE_UNITS_UB_MOVE_TO: UbObj<EEComplexUnit>(inA)->MoveTo((long)inC, (long)inD, true); return 0;
    case EE_UNITS_UB_WAYPOINT_COUNT: return (long)UbObj<EEComplexUnit>(inA)->mWaypoints.size();
    case EE_UNITS_UB_DOMAINS: return UbObj<EEUnitClass>(inA)->mDomains;
    case EE_UNITS_UB_NEXT_WAYPOINT_POINT: UbObj<EEComplexUnit>(inA)->GetNextWaypointPoint(sUbPoint); return 0;
    case EE_UNITS_UB_HEIGHT_AT: return (int64_t)UFloatBits(UbObj<EEComplexUnit>(inA)->mUnitClass->GetHeightAt(UbObj<EEComplexUnit>(inA), sUbPoint));
    case EE_UNITS_UB_HEIGHT: return (int64_t)UFloatBits(UbObj<EEComplexUnit>(inA)->mHeight);
    case EE_UNITS_UB_SET_HEIGHT: UbObj<EEComplexUnit>(inA)->mHeight = UBitsFloat(inC); return 0;
    // EEGetWaitingGatherer's
    case EE_UNITS_UB_SHUTDOWN: return UShutdownInProgress();
    case EE_UNITS_UB_UNIT_TILES: ee_units_uu_unit_tiles(1, inA, &sUbTiles); return 0;
    case EE_UNITS_UB_UNITS_ON_TILES: ee_units_uu_units_on(0, &sUbTiles, &sUbUnits); return 0;
    case EE_UNITS_UB_RANDOM_GENERATE: return (int64_t)UFloatBits(gPhysics->mRandom.Generate());
    case EE_UNITS_UB_LIST_SIZE: return (int64_t)sUbUnits.size();
    case EE_UNITS_UB_LISTS_CLEAR: return (sUbUnits.clear(), sUbTiles.clear(), 0);
    case EE_UNITS_UB_COMMAND: return UbObj<EEComplexUnit>(inA)->mCommand != NULL ? UbObj<EEComplexUnit>(inA)->mCommand->mType : 0;
    case EE_UNITS_UB_GOAL_STATE: return UUHandle(UbObj<EEComplexUnit>(inA)->mGoalState);
    case EE_UNITS_UB_GOAL_KIND: return UbObj<EEUnitGoal>(inA)->mKind;
    case EE_UNITS_UB_GATHER_GOAL: return UUHandle(static_cast<EEUnitGoal*>(EEGatherGoalCast(UbObj<EEUnitGoal>(inA))));
    case EE_UNITS_UB_GATHER_RESOURCE: return UUHandle(static_cast<EEUGRust*>(UbObj<EEUnitGoal>(inA))->GetGatherResource());
    // EEUpgradeHitPoints', the functor's and EEForEachUnitOf's
    case EE_UNITS_UB_UPGRADE_BASE_HIT_POINTS: return UbObj<EEUnitClassUpgrade>(inA)->mUnitClass->mHitPoints;
    case EE_UNITS_UB_UPGRADE_MAX_HIT_POINTS: return UbObj<EEUnitClassUpgrade>(inA)->GetMaxHitPoints();
    case EE_UNITS_UB_SET_UPGRADE_MAX_HIT_POINTS: UbObj<EEUnitClassUpgrade>(inA)->mMaxHitPoints = (long)inC; return 0;
    case EE_UNITS_UB_LOADING_SAVED_GAME: return gWorld.mLoadingSavedGame;
    case EE_UNITS_UB_FIRST_UNIT: return UUHandle(UbObj<EEPlayer>(inA)->mFirstUnit);
    case EE_UNITS_UB_NEXT_UNIT: return UUHandle(static_cast<EEUnit*>(UUUnit(inA)->mNext));
    case EE_UNITS_UB_APPLY_TO_UNIT: UbObj<EEUUUnitFunctor>(inA)->ApplyToUnit(UUUnit(inB)); return 0;
    case EE_UNITS_UB_APPLY: UbObj<EEUUUnitFunctor>(inA)->Apply(UbObj<EEComplexUnit>(inB)); return 0;
    case EE_UNITS_UB_NUM_PENDING: return UbObj<EEPlayer>(inA)->GetNumPendingUnits();
    case EE_UNITS_UB_PENDING_UNIT: return UUHandle(static_cast<EEUnit*>(static_cast<RTSUnit*>(ee_rts_player_pending_unit(UbObj<EEPlayer>(inA), (unsigned long)inC))));
    case EE_UNITS_UB_NUM_PLAYERS: return ::gWorld.mNumPlayers;
    // EEMovePassengers', EETakeOverLoad's, EEGetPlayerClassGraphic's
    case EE_UNITS_UB_NUM_CARGO: return UbObj<EEComplexUnit>(inA)->GetNumCargoUnits();
    case EE_UNITS_UB_UNLOAD_CARGO_AT: return UUHandle(UbObj<EEComplexUnit>(inA)->UnloadCargoAt((unsigned long)inC, GETransformation(kUbOffMap)));
    case EE_UNITS_UB_LOAD_CARGO: UbObj<EEComplexUnit>(inA)->LoadCargo(UbObj<EEComplexUnit>(inB), false, inC != 0); return 0;
    case EE_UNITS_UB_TAKE_GARRISON:
        UbObj<EEComplexUnit>(inA)->AddGarrison(UbObj<EEComplexUnit>(inB)->mGarrisonCount - UbObj<EEComplexUnit>(inA)->mUnitClass->mInitialGarrisonCount, UbObj<EEComplexUnit>(inB)->mGarrisonPopulation);
        return 0;
    case EE_UNITS_UB_TILE_SIZE: return UbObj<EEUnitClass>(inA)->mTileSize;
    case EE_UNITS_UB_TABLE_MODEL: return UUHandle(gUnitClassTable[inC]->mModel);
    case EE_UNITS_UB_UPGRADE_MODEL: return UUHandle(UbObj<EEPlayer>(inA)->mUnitClassUpgrades[inC]->mModel);
    // EEFindTargetNear's, EEFindNearbyTarget's, EECanBallisticHit's, EECanPriestConvert's
    case EE_UNITS_UB_RANGE: return (int64_t)UFloatBits(UUUnit(inA)->mStats->GetRange());
    case EE_UNITS_UB_LINE_OF_SIGHT: return (int64_t)UFloatBits(UbObj<EEComplexUnit>(inA)->GetLineOfSight());
    case EE_UNITS_UB_ATTACK: return (int64_t)UFloatBits(UUUnit(inA)->mStats->GetAttack());
    case EE_UNITS_UB_AMMO_TYPE: return UbObj<EEUnitClass>(inA)->mAmmoType;
    case EE_UNITS_UB_AREA_TOP: sUbArea.mTop = (long)inC; return 0;
    case EE_UNITS_UB_AREA_BOTTOM: sUbArea.mBottom = (long)inC; return 0;
    case EE_UNITS_UB_AREA_LEFT: sUbArea.mLeft = (long)inC; return 0;
    case EE_UNITS_UB_AREA_RIGHT: sUbArea.mRight = (long)inC; return 0;
    case EE_UNITS_UB_SUM_OTHERS_INFLUENCE: return ::gWorld.mInfluenceTable->SumOthersValueOnTiles((unsigned long)inC, sUbArea);
    case EE_UNITS_UB_SEARCH_TARGET: return UUHandle(EESearchTarget(UbObj<EEComplexUnit>(inA), &sUbArea, (long)inC, (long)inD, UBitsFloat(inE)));
    case EE_UNITS_UB_SEARCH_ALONG_LIST: return UUHandle(EESearchTargetAlongList(UbObj<EEComplexUnit>(inA), (long)inC, (long)inD, UBitsFloat(inE)));
    case EE_UNITS_UB_APPROACH_ATTACH_POINT: return EEApproachAttachPoint(UbObj<EEComplexUnit>(inA), *UbObj<GE3DPoint>(inB));
    case EE_UNITS_UB_OTHER_CONTINENT: return UbObj<EEComplexUnit>(inA)->IsOnOtherContinentThan(UbObj<EEComplexUnit>(inB));
    case EE_UNITS_UB_FIND_DROP_LOCATION: return UUHandle(EECombatFindDropLocation(UbObj<EEComplexUnit>(inA), UUUnit(inB), *UbObj<GE3DPoint>((uint64_t)inC)));
    case EE_UNITS_UB_BALLISTIC_PITCH:
        return EEGetBallisticPitch(UbObj<EEComplexUnit>(inA), UbObj<EEComplexUnit>(inA)->GetStats(), UbObj<GE3DPoint>(inB), UbObj<GE3DPoint>((uint64_t)inC), UbObj<float>((uint64_t)inD));
    case EE_UNITS_UB_SHOOTS_HIGH: return UbObj<EEUnitClass>(inA)->mShootsHigh;
    case EE_UNITS_UB_FIRES_OVER_WALLS: return UbObj<EEComplexUnit>(inA)->GetStats()->mCanFireOverWalls;
    case EE_UNITS_UB_LINE_TO_UNIT_BLOCKED:
        return EEIsLineToUnitBlocked(UbObj<EEComplexUnit>(inA), UUUnit(inB), UbObj<EEUnit*>((uint64_t)inC), UbObj<long>((uint64_t)inD), UbObj<long>((uint64_t)inE));
    case EE_UNITS_UB_IS_PRIEST: return UbObj<EEUnitClass>(inA)->IsPriest();
    case EE_UNITS_UB_CAN_CONVERT: return EECanConvert(UbObj<EEComplexUnit>(inA), UbObj<EEComplexUnit>(inB), *UbObj<bool>((uint64_t)inC));
    // EEPreviewWall's
    case EE_UNITS_UB_DROP_TILES: ee_units_uu_drop_tiles((int32_t)inC, UbObj<std::vector<EETileCoordinate> >(inA)); return 0;
    case EE_UNITS_UB_PICK_WALL_PIECES: EEPickWallPieces(*UbObj<std::vector<EETileCoordinate> >(inA), sUbCorners, ::gWorld.mMap); return 0;
    case EE_UNITS_UB_BUILD_WALL_GHOSTS: EEBuildWallGhosts(*UbObj<std::vector<EETileCoordinate> >(inA), sUbCorners, (long)inC, UbObj<EEPlayer>(inB), 0.0f); return 0;
    case EE_UNITS_UB_CORNERS_CLEAR: sUbCorners.clear(); return 0;
    // EEHasCivilizationUpgrade's, EEFn24E68F's
    case EE_UNITS_UB_PURCHASED_COUNT: return (int64_t)UbObj<EECivilization>(inA)->GetPurchasedUpgrades().size();
    case EE_UNITS_UB_PURCHASED_AT: return (int64_t)UbObj<EECivilization>(inA)->GetPurchasedUpgrades()[(size_t)inC];
    case EE_UNITS_UB_SPLIT_ATTRIBUTE: ee_screens_civm_split((EECivUpgrade)inC, &sUbAttribute[0], &sUbAttribute[1]); return 0;
    case EE_UNITS_UB_LOOKUP_LEVEL: gCivilizationManager.LookupLevel((EECivUpgrade)inC, &(*ioF)[0], &(*ioF)[1]); return 0;
    case EE_UNITS_UB_CATEGORY_NAME_ID: return ee_screens_civm_category_name_id((uint32_t)(sUbAttribute[0]), &sUbStringID);
    case EE_UNITS_UB_CATEGORY_STRING: return gResource->GetString(sUbStringID, sUbCategoryString);
    case EE_UNITS_UB_LEVEL_NAME_ID: sUbStringID = (long)ee_tech_civ_level_name_id((uint32_t)(sUbAttribute[1])); return 0;
    case EE_UNITS_UB_ATTRIBUTE_STRING: return gResource->GetString(sUbStringID, sUbAttributeString);
    case EE_UNITS_UB_IS_POWER_LEVEL: return ee_tech_civ_is_power_level((uint32_t)(sUbAttribute[1]));
    case EE_UNITS_UB_LEVEL_TEXT:
        sprintf(*UbObj<UWideString>(inA), "%s: %s %ld%%", (const char*)sUbCategoryString, (const char*)sUbAttributeString, (long)inC);
        return 0;
    case EE_UNITS_UB_POWER_TEXT: sprintf(*UbObj<UWideString>(inA), "%s", (const char*)sUbAttributeString); return 0;
    }
    return 0;
}
