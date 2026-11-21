//! `EEComplexUnit`'s world bodies over its C++ members (the units' lane, round 7; were EEComplexUnit.cpp): the
//! condition target, the unit swaps, the sight's move, passability, reach and continents, the area tile and the
//! nearest town, the move to a location, growth, the area effects, the squadron goals, the build queue's pop,
//! fanaticism, the held units, contacts and the reference-counted links. Every C++ statement of the originals is
//! one operation of [`CuHost::cu`] (the game's: `ee_units_cu_host`, EEComplexUnit.cpp, one statement a case), asked
//! in the original's order. `unit` is the `EEComplexUnit*`, `a` an object, `b`, `c` values (floats as their bits).

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::units::cu_body::*;
use crate::units::cu_cargo::*;
use crate::units::cu_life::*;
use crate::units::cu_misc::*;

// ---- the statements (`op`; `a` an object)
/// unit `a`'s `EEAttackUnitCast(GetGoal())` (0: none)
pub const EE_UNITS_CU_ATTACK_GOAL_OF: i32 = 700;
/// attack goal `a`'s `GetAttackTarget() == this`
pub const EE_UNITS_CU_ATTACK_TARGET_IS_THIS: i32 = 701;
/// unit `a`'s `IsInReach(this)`
pub const EE_UNITS_CU_IN_REACH_OF: i32 = 702;
/// `GetUnitClass()->mShootsHigh`
pub const EE_UNITS_CU_SHOOTS_HIGH: i32 = 703;
/// `GetStats()->mCanFireOverWalls`
pub const EE_UNITS_CU_FIRES_OVER_WALLS: i32 = 704;
/// `EEIsLineToUnitBlocked(unit a, this, ...)`
pub const EE_UNITS_CU_LINE_FROM_BLOCKED: i32 = 705;
/// unit `a`'s `EEAttackGroundCast(GetGoal())` (0: none)
pub const EE_UNITS_CU_GROUND_GOAL_OF: i32 = 706;
/// ground attack goal `a`'s `GetGroundTarget()`'s x, y (b 0, 1; its bits)
pub const EE_UNITS_CU_GROUND_TARGET: i32 = 707;
/// unit `a`'s `mStats->mAreaEffect` (its bits)
pub const EE_UNITS_CU_AREA_EFFECT_OF: i32 = 708;
/// `mCargo[b]->Dereference()`
pub const EE_UNITS_CU_CARGO_DEREFERENCE_AT: i32 = 709;
/// `LinkPart(a)`
pub const EE_UNITS_CU_LINK_PART_TO: i32 = 710;
/// `gWorld.DeactivateUnit(this)`
pub const EE_UNITS_CU_DEACTIVATE_THIS: i32 = 711;
/// unit `a`'s goal (`mGoalQueues[b].mGoals[c]`) `ReplaceUnit(the pair's old, new, this)`: the pair the replacements at
/// the scratch's index set by `EE_UNITS_CU_SET_PAIR`
pub const EE_UNITS_CU_GOAL_REPLACE_UNIT: i32 = 712;
/// the replacements' (the `std::vector<EEPUnitSwap>` at a) size
pub const EE_UNITS_CU_PAIRS_SIZE: i32 = 713;
/// the replacements' (at a) pair b's old (c 0) or new (c 1) unit
pub const EE_UNITS_CU_PAIR_UNIT: i32 = 714;
/// `mGoalQueues[b].mGoals.size() <= c || ...` the goal at c is not `a` any more
pub const EE_UNITS_CU_GOAL_CHANGED: i32 = 715;
/// `mGoalQueues[b].mGoals[c]`
pub const EE_UNITS_CU_QUEUE_GOAL_AT: i32 = 716;
/// goal `a`'s `ReplaceUnit((EEUnit*)b, (EEUnit*)c, this)`
pub const EE_UNITS_CU_GOAL_REPLACE: i32 = 717;
/// `mCargo.size()`
pub const EE_UNITS_CU_CARGO_SIZE: i32 = 718;
/// `ReplaceUnit((EEComplexUnit*)a, (EEComplexUnit*)b)`
pub const EE_UNITS_CU_REPLACE_UNIT: i32 = 719;
/// `AddBuildTarget((EEUnit*)a)`
pub const EE_UNITS_CU_ADD_BUILD_TARGET_UNIT: i32 = 720;
/// the sight can move: `mPlayer != NULL && mPlayer->mSaveVisMap == true && GetLineOfSight() > 0.0f &&
/// gSightUpdatesEnabled`
pub const EE_UNITS_CU_SIGHT_READY: i32 = 721;
/// `SightOn(b, c)`
pub const EE_UNITS_CU_SIGHT_ON: i32 = 722;
/// `SightOff(b, c)`
pub const EE_UNITS_CU_SIGHT_OFF: i32 = 723;
/// `EEMoveSightCircle(f[0], f[1], f[2], f[3], GetLineOfSight(), mPlayer)`
pub const EE_UNITS_CU_MOVE_SIGHT_CIRCLE: i32 = 724;
/// complex unit `a`'s `mIsDisabled && !IsProtected()`
pub const EE_UNITS_CU_UNIT_DISABLED_UNPROTECTED: i32 = 725;
/// `EEUnitClassOfKind<EE_CLASS_KIND_GATE>(GetUnitClass())->IsSidePart(this)`
pub const EE_UNITS_CU_GATE_IS_SIDE_PART: i32 = 726;
/// unit `a`'s player is not this unit's and is its enemy
pub const EE_UNITS_CU_HOSTILE: i32 = 727;
/// `mIsLocked`
pub const EE_UNITS_CU_IS_LOCKED: i32 = 728;
/// `GetUnitClass()->mBlocksTile`
pub const EE_UNITS_CU_BLOCKS_TILE: i32 = 729;
/// `IsFoundation()`
pub const EE_UNITS_CU_IS_FOUNDATION_NOW: i32 = 730;
/// `GetUnitClass()->mIsMelee`
pub const EE_UNITS_CU_IS_MELEE: i32 = 731;
/// `GetUnitClass()->mProjectileClass`
pub const EE_UNITS_CU_PROJECTILE_CLASS: i32 = 732;
/// the scratch destination `= GE3DPoint((float)b, (float)c, gWorld.mMap->GetTile(b, c)->mHeight)`
pub const EE_UNITS_CU_SET_DESTINATION: i32 = 733;
/// `EEApproachAttachPoint(this, the scratch point = GE3DPoint(0.0f, 0.0f, 0.0f))`
pub const EE_UNITS_CU_APPROACH_ATTACH: i32 = 734;
/// `GetPosition(the scratch position = GE3DPoint(0.0f, 0.0f, 0.0f))`
pub const EE_UNITS_CU_GET_POSITION: i32 = 735;
/// `EEGetBallisticPitch(this, GetStats(), &the scratch position, &the scratch destination, &the scratch pitch)`
pub const EE_UNITS_CU_BALLISTIC_PITCH: i32 = 736;
/// `EEIsLineBlocked(GetTileX(), GetTileY(), b, c, ...)`
pub const EE_UNITS_CU_LINE_BLOCKED: i32 = 737;
/// `DistanceSquaredToTileXY(b, c)` (its bits)
pub const EE_UNITS_CU_DISTANCE_SQUARED_TO_TILE: i32 = 738;
/// `gContinentManager.CanGetTo(this, b, c)`
pub const EE_UNITS_CU_CAN_GET_TO: i32 = 739;
/// `gWorld.mMap->mTerrainMesh.GetIsSpaceMap()`
pub const EE_UNITS_CU_IS_SPACE_MAP: i32 = 740;
/// `gContinentManager.GetContinent(this, kAIPathNonSpace)`
pub const EE_UNITS_CU_CONTINENT_NON_SPACE: i32 = 741;
/// `gContinentManager.GetContinent(EETileCoordinate(b, c), kAIPathNonSpace)`
pub const EE_UNITS_CU_TILE_CONTINENT_NON_SPACE: i32 = 742;
/// complex unit `a`'s `mPlanetID`
pub const EE_UNITS_CU_UNIT_PLANET: i32 = 743;
/// unit `a`'s `GetTileY()`
pub const EE_UNITS_CU_UNIT_TILE_Y: i32 = 744;
/// `gContinentManager.GetContinentAt(unit a)`
pub const EE_UNITS_CU_CONTINENT_AT: i32 = 745;
/// unit `a`'s `mUnitClass->mTileSize`
pub const EE_UNITS_CU_UNIT_TILE_SIZE: i32 = 746;
/// continent `a`'s `GetCategory()`
pub const EE_UNITS_CU_CONTINENT_CATEGORY: i32 = 747;
/// `gMapWidth`, `gMapHeight` (b 0, 1)
pub const EE_UNITS_CU_MAP_SIZE: i32 = 748;
/// `gContinentManager.GetContinentID(b, c)`
pub const EE_UNITS_CU_CONTINENT_ID: i32 = 749;
/// `gContinentManager.GetContinentByID(b)`
pub const EE_UNITS_CU_CONTINENT_BY_ID: i32 = 750;
/// unit `a`'s `GetTile()`'s x, y (b 0, 1)
pub const EE_UNITS_CU_UNIT_TILE: i32 = 751;
/// `mPlayer->GetNumTowns()`
pub const EE_UNITS_CU_NUM_TOWNS: i32 = 752;
/// `mPlayer->mTowns[b]`
pub const EE_UNITS_CU_TOWN_AT: i32 = 753;
/// town `a`'s `Contains(U2DPoint<long>(b, c))`
pub const EE_UNITS_CU_TOWN_CONTAINS: i32 = 754;
/// town `a`'s hull outline point's squared distance from (b, c)
pub const EE_UNITS_CU_TOWN_DISTANCE: i32 = 755;
/// `mTile`
pub const EE_UNITS_CU_TILE_POINTER: i32 = 756;
/// `mIsCollidable`
pub const EE_UNITS_CU_IS_COLLIDABLE: i32 = 757;
/// `MoveTo(b, c, a != 0)`
pub const EE_UNITS_CU_MOVE_TO_UPDATE: i32 = 758;
/// `mWaypoints.size()`
pub const EE_UNITS_CU_WAYPOINT_COUNT: i32 = 759;
/// `GetNextWaypointPoint(the scratch point = GE3DPoint(0.0f, 0.0f, 0.0f))`
pub const EE_UNITS_CU_NEXT_WAYPOINT_POINT: i32 = 760;
/// `SetHeight(GetUnitClass()->GetHeightAt(this, the scratch point))`
pub const EE_UNITS_CU_SET_HEIGHT_AT_POINT: i32 = 761;
/// `HasSwimAnimation()`
pub const EE_UNITS_CU_HAS_SWIM_ANIMATION: i32 = 762;
/// `IsSwimming()`
pub const EE_UNITS_CU_IS_SWIMMING: i32 = 763;
/// `SetAnimation(b, -1, 0)`
pub const EE_UNITS_CU_SET_ANIMATION_STATE: i32 = 764;
/// `mGrowth` (its bits)
pub const EE_UNITS_CU_GROWTH: i32 = 765;
/// `mGrowth = the float of the bits b`
pub const EE_UNITS_CU_SET_GROWTH: i32 = 766;
/// `GetStats()->GetScale()` (its bits)
pub const EE_UNITS_CU_STATS_SCALE: i32 = 767;
/// `mCollisionRadius` (its bits)
pub const EE_UNITS_CU_COLLISION_RADIUS: i32 = 768;
/// `mCollisionRadius = the float of the bits b`
pub const EE_UNITS_CU_SET_COLLISION_RADIUS: i32 = 769;
/// `mNumAreaEffects = 0`
pub const EE_UNITS_CU_ZERO_NUM_AREA_EFFECTS: i32 = 770;
/// `GetStats()->mAreaEffectTemplates[b]`
pub const EE_UNITS_CU_STATS_AREA_TEMPLATE: i32 = 771;
/// `mNumAreaEffects++`
pub const EE_UNITS_CU_COUNT_AREA_EFFECT: i32 = 772;
/// `gCivPowersManager.HasPower(GetPlayer(), kCivPowerParatroopers)`
pub const EE_UNITS_CU_HAS_PARATROOPERS: i32 = 773;
/// `GetQueuedGoal(b)`
pub const EE_UNITS_CU_QUEUED_GOAL: i32 = 774;
/// `QueueGoal(goal a, b)`
pub const EE_UNITS_CU_QUEUE_GOAL: i32 = 775;
/// `mAreaEffects.GetSize()`
pub const EE_UNITS_CU_AREA_EFFECTS_SIZE: i32 = 776;
/// `mAreaEffects.mItems[b]->GetRadius()` (its bits)
pub const EE_UNITS_CU_AREA_EFFECT_RADIUS: i32 = 777;
/// `gAEManager.CollectEffects(the float of the bits b, the location's x and y, c, &mAreaEffects, this)`
pub const EE_UNITS_CU_COLLECT_EFFECTS: i32 = 778;
/// `mPlayer->mPlayerID`
pub const EE_UNITS_CU_PLAYER_ID: i32 = 779;
/// `mAffectingAreaEffects.mItems[b]->mTemplate->mKind`
pub const EE_UNITS_CU_AFFECTING_KIND: i32 = 780;
/// `*(EEGraphicEffect**)a = mAffectingAreaEffects.mItems[b]->mTemplate->mGraphicEffect`
pub const EE_UNITS_CU_SET_OUT_EFFECT: i32 = 781;
/// `mCitizenBuildQueue.front()`
pub const EE_UNITS_CU_CITIZEN_FRONT: i32 = 782;
/// `mCitizenBuildQueue.pop_front()`
pub const EE_UNITS_CU_CITIZEN_POP_FRONT: i32 = 783;
/// unit `a`'s `mIsValid`
pub const EE_UNITS_CU_UNIT_VALID: i32 = 784;
/// `mCloaked = gPhysics->mGameTime + gWorldTable.GetLongValue(kWorldFanaticism)`
pub const EE_UNITS_CU_START_FANATICISM: i32 = 785;
/// `gWorldTable.GetFloatValue(kWorldFanaticism)` (its bits)
pub const EE_UNITS_CU_FANATICISM_COST: i32 = 786;
/// `SetHitPoints(b)`
pub const EE_UNITS_CU_SET_HIT_POINTS: i32 = 787;
/// `mIsDeletable`
pub const EE_UNITS_CU_IS_DELETABLE: i32 = 788;
/// `GetPlayer()->GetTechTree()->Get(kTechCitizen)->Contains(GetUnitClass()->GetUnitType())`
pub const EE_UNITS_CU_CITIZEN_TECH_HAS_TYPE: i32 = 789;
/// `mPlayer->mNumCapitols`
pub const EE_UNITS_CU_NUM_CAPITOLS: i32 = 790;
/// the index of unit `a` in `mHeldUnits` (`std::find`; -1: not there)
pub const EE_UNITS_CU_HELD_FIND: i32 = 791;
/// `mHeldUnits.erase(mHeldUnits.begin() + b)`
pub const EE_UNITS_CU_HELD_ERASE: i32 = 792;
/// `mHeldUnits.size()`
pub const EE_UNITS_CU_HELD_COUNT: i32 = 793;
/// `mHeldUnits[b]->mHolder == this`
pub const EE_UNITS_CU_HELD_HOLDER_IS_THIS: i32 = 794;
/// `mHeldUnits[b]->ClearHolder()`
pub const EE_UNITS_CU_HELD_CLEAR_HOLDER: i32 = 795;
/// `mHeldUnits[b]->Dereference()`
pub const EE_UNITS_CU_HELD_DEREFERENCE: i32 = 796;
/// `mHeldUnits.clear()`
pub const EE_UNITS_CU_HELD_CLEAR: i32 = 797;
/// `mCitizenBuildQueue.front()->Dereference()`
pub const EE_UNITS_CU_CITIZEN_FRONT_DEREFERENCE: i32 = 798;
/// `mContacts.mHead`
pub const EE_UNITS_CU_CONTACTS_HEAD: i32 = 799;
/// contact node `a`'s `mNext`
pub const EE_UNITS_CU_CONTACT_NEXT: i32 = 800;
/// contact node `a`'s `mData->RemoveContactUnit(this)`
pub const EE_UNITS_CU_CONTACT_REMOVE_THIS: i32 = 801;
/// contact node `a`'s `mData->Dereference()`
pub const EE_UNITS_CU_CONTACT_DEREFERENCE: i32 = 802;
/// `mContacts.Clear()`
pub const EE_UNITS_CU_CONTACTS_CLEAR: i32 = 803;
/// `mContacts.Find(unit a)`
pub const EE_UNITS_CU_CONTACTS_FIND: i32 = 804;
/// `mContacts.Remove(node a)`
pub const EE_UNITS_CU_CONTACTS_REMOVE: i32 = 805;
/// `mAffectingAreaEffects.mItems.size()` is EE_UNITS_CU_AFFECTING_COUNT; its `[b]->RemovedFrom(this)`
pub const EE_UNITS_CU_AFFECTING_REMOVED_FROM: i32 = 806;
/// `UIsNone32(mConversionEffect.mValue)`
pub const EE_UNITS_CU_CONVERSION_IS_NONE: i32 = 807;
/// `gEffectManager->DestroyEffect(the scratch handle = mConversionEffect)`
pub const EE_UNITS_CU_DESTROY_CONVERSION_EFFECT: i32 = 808;
/// `mHolder = a`
pub const EE_UNITS_CU_SET_HOLDER_FIELD: i32 = 809;
/// `mMine = a`
pub const EE_UNITS_CU_SET_MINE_FIELD: i32 = 810;
/// `mBlockingUnit`
pub const EE_UNITS_CU_BLOCKING_UNIT: i32 = 811;
/// `mBlockingUnit = a`
pub const EE_UNITS_CU_SET_BLOCKING_FIELD: i32 = 812;
/// `mLastResource`
pub const EE_UNITS_CU_LAST_RESOURCE: i32 = 813;
/// `mLastResource = a`
pub const EE_UNITS_CU_SET_LAST_RESOURCE_FIELD: i32 = 814;
/// `mHomeBase = a`
pub const EE_UNITS_CU_SET_HOME_BASE_FIELD: i32 = 815;
/// `mPatrolGroup = a`
pub const EE_UNITS_CU_SET_PATROL_GROUP_FIELD: i32 = 816;
/// `ee_units_pg_reference(a)`
pub const EE_UNITS_CU_PATROL_REFERENCE: i32 = 817;
/// `GetNumCargoUnits()`
pub const EE_UNITS_CU_NUM_CARGO_UNITS: i32 = 818;
/// `GetCargo(b)->mUnitID`
pub const EE_UNITS_CU_CARGO_ID_AT: i32 = 819;
/// `UnloadCargoAt(b, *(const GETransformation*)a)`
pub const EE_UNITS_CU_UNLOAD_CARGO_AT_INDEX: i32 = 820;
/// `gWorldTable.GetFloatValue(kWorldReloadTimeRandomRange)`, `GetFloatValue2(...)` (b 0, 1; its bits)
pub const EE_UNITS_CU_RELOAD_RANGE: i32 = 821;
/// `gPhysics->GetRandom(the floats of the bits b, c)` (its bits)
pub const EE_UNITS_CU_RANDOM_BETWEEN: i32 = 822;
/// `SetReloadTimer(b)`
pub const EE_UNITS_CU_SET_RELOAD_TIMER: i32 = 823;

// ---- the bodies (`ee_units_cu_body`'s `what`)
pub const EE_UNITS_CU_DO_CONDITION_TARGET: i32 = 80;
pub const EE_UNITS_CU_DO_REPLACE_UNIT: i32 = 81;
pub const EE_UNITS_CU_DO_REPLACE_UNITS: i32 = 82;
pub const EE_UNITS_CU_DO_IS_PASSABLE_BY: i32 = 84;
pub const EE_UNITS_CU_DO_CAN_REACH_TILE: i32 = 85;
pub const EE_UNITS_CU_DO_IS_ON_OTHER_CONTINENT: i32 = 86;
pub const EE_UNITS_CU_DO_IS_ON_OTHER_CONTINENT_THAN: i32 = 87;
pub const EE_UNITS_CU_DO_FIND_AREA_TILE: i32 = 88;
pub const EE_UNITS_CU_DO_FIND_NEAREST_TOWN: i32 = 89;
pub const EE_UNITS_CU_DO_MOVE_TO_LOCATION: i32 = 90;
pub const EE_UNITS_CU_DO_SET_GROWTH: i32 = 91;
pub const EE_UNITS_CU_DO_MAKE_AREA_EFFECTS: i32 = 92;
pub const EE_UNITS_CU_DO_QUEUE_SQUADRON_GOALS: i32 = 93;
pub const EE_UNITS_CU_DO_SET_SQUADRON_GOALS: i32 = 94;
pub const EE_UNITS_CU_DO_APPLY_AREA_EFFECTS: i32 = 95;
pub const EE_UNITS_CU_DO_GET_EFFECT: i32 = 96;
pub const EE_UNITS_CU_DO_POP_BUILD_TARGET: i32 = 97;
pub const EE_UNITS_CU_DO_GO_FANATIC: i32 = 98;
pub const EE_UNITS_CU_DO_CAN_DELETE: i32 = 99;
pub const EE_UNITS_CU_DO_RELEASE_HELD: i32 = 100;
pub const EE_UNITS_CU_DO_RELEASE_HELD_UNITS: i32 = 101;
pub const EE_UNITS_CU_DO_CLEAR_BUILD_TARGETS: i32 = 102;
pub const EE_UNITS_CU_DO_CLEAR_CONTACTS: i32 = 103;
pub const EE_UNITS_CU_DO_REMOVE_CONTACT_UNIT: i32 = 104;
pub const EE_UNITS_CU_DO_REMOVE_AFFECTING: i32 = 105;
pub const EE_UNITS_CU_DO_RELEASE_CONVERSION_EFFECT: i32 = 106;
pub const EE_UNITS_CU_DO_SET_HOLDER: i32 = 107;
pub const EE_UNITS_CU_DO_SET_MINE: i32 = 108;
pub const EE_UNITS_CU_DO_SET_BLOCKING_UNIT: i32 = 109;
pub const EE_UNITS_CU_DO_SET_LAST_RESOURCE: i32 = 110;
pub const EE_UNITS_CU_DO_SET_HOME_BASE: i32 = 111;
pub const EE_UNITS_CU_DO_SET_PATROL_GROUP: i32 = 112;
pub const EE_UNITS_CU_DO_FIND_PASSENGER: i32 = 113;
pub const EE_UNITS_CU_DO_UNLOAD_CARGO_BY_ID: i32 = 114;
pub const EE_UNITS_CU_DO_SET_RANDOM_RELOAD_TIMER: i32 = 115;

/// `kUnitCategoryGate`, `kUnitCategoryProjectile`, `kUnitCategoryBomb`
const CATEGORY_GATE: i64 = 0x19;
const CATEGORY_PROJECTILE: i64 = 9;
const CATEGORY_BOMB: i64 = 0xb;
/// `kActionClosed`
const ACTION_CLOSED: i64 = 15001;
/// `kUnitTypeX04TrojanHorse`, the space types that keep their planet, `kUnitTypeSp15SpaceFighter`
const UNIT_TYPE_X04_TROJAN_HORSE: i64 = 0xd1;
const UNIT_TYPE_SPACE_DOCK: i64 = 0x2d8;
const UNIT_TYPE_SPACE_TURRET: i64 = 0x302;
const UNIT_TYPE_ORBITAL_SPACE_STATION: i64 = 0x31f;
const UNIT_TYPE_SP15_SPACE_FIGHTER: i64 = 0x2f6;
/// `kMovementAir`, `kMovementSpace`
const MOVEMENT_AIR: i64 = 1;
const MOVEMENT_SPACE: i64 = 8;
/// the sight's off-map tile
const OFF_MAP: i64 = -10000;
/// the walk and swim animations
const ANIMATION_WALK: i64 = 2;
const ANIMATION_SWIM: i64 = 0xd;

struct S<'h, H: CuHost + ?Sized> {
    h: &'h mut H,
    unit: u64,
}

impl<H: CuHost + ?Sized> S<'_, H> {
    fn q(&mut self, op: i32) -> i64 {
        self.h.cu(op, self.unit, 0, 0, 0, &mut [0.0; 4])
    }
    fn qa(&mut self, op: i32, a: u64) -> i64 {
        self.h.cu(op, self.unit, a, 0, 0, &mut [0.0; 4])
    }
    fn qb(&mut self, op: i32, b: i64) -> i64 {
        self.h.cu(op, self.unit, 0, b, 0, &mut [0.0; 4])
    }
    fn qab(&mut self, op: i32, a: u64, b: i64) -> i64 {
        self.h.cu(op, self.unit, a, b, 0, &mut [0.0; 4])
    }
    fn qbc(&mut self, op: i32, b: i64, c: i64) -> i64 {
        self.h.cu(op, self.unit, 0, b, c, &mut [0.0; 4])
    }
    fn qabc(&mut self, op: i32, a: u64, b: i64, c: i64) -> i64 {
        self.h.cu(op, self.unit, a, b, c, &mut [0.0; 4])
    }
    fn yes(&mut self, op: i32) -> bool {
        self.q(op) != 0
    }
    fn obj(&mut self, op: i32) -> u64 {
        self.q(op) as u64
    }
    fn float(&mut self, op: i32) -> f32 {
        f32::from_bits(self.q(op) as u32)
    }
}

fn bits(v: f32) -> i64 {
    i64::from(v.to_bits())
}

fn float(b: i64) -> f32 {
    f32::from_bits(b as u32)
}

/// `GetConditionTarget()`: the last attacker while it still attacks this unit (in reach with a clear line, or a
/// ground attack covering it); otherwise it is forgotten. The attacker or 0.
pub fn condition_target<H: CuHost + ?Sized>(h: &mut H, unit: u64) -> u64 {
    let s = &mut S { h, unit };
    let last = s.obj(EE_UNITS_CU_LAST_ATTACKER);
    if last == 0 {
        return 0;
    }
    let attack = s.qa(EE_UNITS_CU_ATTACK_GOAL_OF, last) as u64;
    if attack != 0 {
        if s.qa(EE_UNITS_CU_ATTACK_TARGET_IS_THIS, attack) != 0
            && s.qa(EE_UNITS_CU_IN_REACH_OF, last) != 0
            && (s.yes(EE_UNITS_CU_SHOOTS_HIGH)
                || s.yes(EE_UNITS_CU_FIRES_OVER_WALLS)
                || s.qa(EE_UNITS_CU_LINE_FROM_BLOCKED, last) == 0)
        {
            return last;
        }
    } else {
        let ground = s.qa(EE_UNITS_CU_GROUND_GOAL_OF, last) as u64;
        if ground != 0 {
            let dx = s.float(EE_UNITS_CU_LOCATION_X) - float(s.qab(EE_UNITS_CU_GROUND_TARGET, ground, 0));
            let dy = s.float(EE_UNITS_CU_LOCATION_Y) - float(s.qab(EE_UNITS_CU_GROUND_TARGET, ground, 1));
            let radius = float(s.qa(EE_UNITS_CU_AREA_EFFECT_OF, last));
            if dx * dx + dy * dy <= radius * radius {
                return last;
            }
        }
    }
    let last = s.obj(EE_UNITS_CU_LAST_ATTACKER);
    if last != 0 {
        s.qa(EE_UNITS_CU_DEREFERENCE, last);
    }
    s.q(EE_UNITS_CU_CLEAR_LAST_ATTACKER);
    s.obj(EE_UNITS_CU_LAST_ATTACKER)
}

/// `ReplaceUnit(old, new)`: the old unit in the cargo replaced (references moved, the new one's container this
/// unit), and this unit relinked to the new one if the old one was its previous part.
pub fn replace_unit<H: CuHost + ?Sized>(h: &mut H, unit: u64, old: u64, new: u64) {
    let s = &mut S { h, unit };
    let mut i = s.q(EE_UNITS_CU_CARGO_SIZE) - 1;
    while i >= 0 {
        if s.qb(EE_UNITS_CU_CARGO_AT, i) as u64 != old {
            i -= 1;
            continue;
        }
        s.qb(EE_UNITS_CU_CARGO_DEREFERENCE_AT, i);
        s.qab(EE_UNITS_CU_CARGO_SET_AT, new, i);
        s.qa(EE_UNITS_CU_REFERENCE, new);
        let container = s.qa(EE_UNITS_CU_UNIT_CONTAINER, new) as u64;
        if container != unit {
            if container != 0 {
                s.qa(EE_UNITS_CU_DEREFERENCE, container);
            }
            s.qa(EE_UNITS_CU_SET_CONTAINER_THIS, new);
            s.q(EE_UNITS_CU_REFERENCE_THIS);
        }
        break;
    }
    if s.obj(EE_UNITS_CU_PREVIOUS_PART) == old {
        s.qa(EE_UNITS_CU_LINK_PART_TO, new);
        s.q(EE_UNITS_CU_DEACTIVATE_THIS);
    }
}

/// `ReplaceUnits(pairs)`: the replaced units (old -> new) swapped in the goals, the cargo and parts, and the build
/// queue; `pairs` the `const std::vector<EEPUnitSwap>*`.
pub fn replace_units<H: CuHost + ?Sized>(h: &mut H, unit: u64, pairs: u64) {
    let count = h.cu(EE_UNITS_CU_PAIRS_SIZE, unit, pairs, 0, 0, &mut [0.0; 4]);
    if count == 0 {
        return;
    }
    let pair = |h: &mut H, k: i64, which: i64| h.cu(EE_UNITS_CU_PAIR_UNIT, unit, pairs, k, which, &mut [0.0; 4]);
    let mut i = 0;
    while i < h.cu(EE_UNITS_CU_GOAL_QUEUE_COUNT, unit, 0, 0, 0, &mut [0.0; 4]) {
        let mut j = 0;
        while j < h.cu(EE_UNITS_CU_QUEUE_GOAL_COUNT, unit, 0, i, 0, &mut [0.0; 4]) {
            let goal = h.cu(EE_UNITS_CU_QUEUE_GOAL_AT, unit, 0, i, j, &mut [0.0; 4]) as u64;
            for k in 0..count {
                let (old, new) = (pair(h, k, 0), pair(h, k, 1));
                h.cu(EE_UNITS_CU_GOAL_REPLACE, unit, goal, old, new, &mut [0.0; 4]);
                if h.cu(EE_UNITS_CU_GOAL_CHANGED, unit, goal, i, j, &mut [0.0; 4]) != 0 {
                    break;
                }
            }
            j += 1;
        }
        i += 1;
    }
    if h.cu(EE_UNITS_CU_CARGO_SIZE, unit, 0, 0, 0, &mut [0.0; 4]) > 0 || h.cu(EE_UNITS_CU_CONTAINER, unit, 0, 0, 0, &mut [0.0; 4]) != 0 {
        for k in 0..count {
            let (old, new) = (pair(h, k, 0), pair(h, k, 1));
            replace_unit(h, unit, old as u64, new as u64);
        }
    }
    let mut targets = Vec::new();
    loop {
        let target = pop_build_target(h, unit);
        if target == 0 {
            break;
        }
        targets.push(target);
    }
    for mut target in targets {
        for k in 0..count {
            if pair(h, k, 0) as u64 == target {
                target = pair(h, k, 1) as u64;
                break;
            }
        }
        h.cu(EE_UNITS_CU_ADD_BUILD_TARGET_UNIT, unit, target, 0, 0, &mut [0.0; 4]);
    }
}

/// `SightMoved(old x, old y, new x, new y)`: the line of sight moved from the old tile to the new one (added or
/// removed when either is off the map); the tiles as the original's `unsigned long`s read as `long`s (the off-map
/// tile is -10000).
pub fn sight_moved<H: CuHost + ?Sized>(h: &mut H, unit: u64, ox: i64, oy: i64, nx: i64, ny: i64) {
    let s = &mut S { h, unit };
    let large = s.q(EE_UNITS_CU_TILE_SIZE) > 1;
    if !s.yes(EE_UNITS_CU_SIGHT_READY) {
        return;
    }
    if ox == OFF_MAP {
        s.qbc(EE_UNITS_CU_SIGHT_ON, nx, ny);
        return;
    }
    if nx != OFF_MAP {
        if s.q(EE_UNITS_CU_HIT_POINTS) > 0 {
            let offset = if !large { 0.5 } else { s.q(EE_UNITS_CU_TILE_SIZE) as c_long as f32 * 0.5 };
            let at = |v: i64| v as c_long as f32 + offset;
            let mut f = [at(ox), at(oy), at(nx), at(ny)];
            s.h.cu(EE_UNITS_CU_MOVE_SIGHT_CIRCLE, unit, 0, 0, 0, &mut f);
            return;
        }
        s.qbc(EE_UNITS_CU_SIGHT_ON, nx, ny);
    }
    s.qbc(EE_UNITS_CU_SIGHT_OFF, ox, oy);
}

/// `IsPassableBy(other)`: whether a unit may pass this gate (closed and locked rules, the Trojan horse) or tile
/// blocker.
pub fn is_passable_by<H: CuHost + ?Sized>(h: &mut H, unit: u64, other: u64) -> bool {
    let s = &mut S { h, unit };
    if s.q(EE_UNITS_CU_CATEGORY) == CATEGORY_GATE && s.q(EE_UNITS_CU_HIT_POINTS) > 0 {
        let mut carrier = unit;
        loop {
            let previous = s.qa(EE_UNITS_CU_UNIT_PREVIOUS_PART, carrier) as u64;
            if previous == 0 {
                break;
            }
            carrier = previous;
        }
        if carrier != 0 && s.qa(EE_UNITS_CU_UNIT_DISABLED_UNPROTECTED, carrier) != 0 {
            return false;
        }
        if s.yes(EE_UNITS_CU_GATE_IS_SIDE_PART) {
            return false;
        }
        let command = if s.yes(EE_UNITS_CU_HAS_COMMAND) { s.q(EE_UNITS_CU_COMMAND_KIND) } else { 0 };
        if command == ACTION_CLOSED {
            if other == 0 {
                return false;
            }
            if (s.qa(EE_UNITS_CU_HOSTILE, other) != 0 || s.yes(EE_UNITS_CU_IS_LOCKED))
                && (s.qa(EE_UNITS_CU_HOSTILE, other) == 0 || s.qa(EE_UNITS_CU_UNIT_UNIT_TYPE, other) != UNIT_TYPE_X04_TROJAN_HORSE)
            {
                return false;
            }
        }
    } else if s.yes(EE_UNITS_CU_BLOCKS_TILE) && !s.yes(EE_UNITS_CU_IS_FOUNDATION_NOW) && s.q(EE_UNITS_CU_HIT_POINTS) > 0 {
        return false;
    }
    true
}

/// `IsOnOtherContinent(x, y)`: out of reach: unreachable for air, else on another planet of a space map.
pub fn is_on_other_continent<H: CuHost + ?Sized>(h: &mut H, unit: u64, x: i64, y: i64) -> bool {
    let s = &mut S { h, unit };
    if s.q(EE_UNITS_CU_CLASS_DOMAINS) & MOVEMENT_AIR != 0 {
        return s.qbc(EE_UNITS_CU_CAN_GET_TO, x, y) == 0;
    }
    s.yes(EE_UNITS_CU_IS_SPACE_MAP)
        && s.q(EE_UNITS_CU_CLASS_DOMAINS) & MOVEMENT_SPACE == 0
        && s.q(EE_UNITS_CU_UNIT_TYPE) != UNIT_TYPE_SPACE_TURRET
        && s.q(EE_UNITS_CU_CONTINENT_NON_SPACE) != s.qbc(EE_UNITS_CU_TILE_CONTINENT_NON_SPACE, x, y)
}

/// `IsOnOtherContinentThan(other)`: whether another unit is on another continent (space docks and turrets by their
/// planet).
pub fn is_on_other_continent_than<H: CuHost + ?Sized>(h: &mut H, unit: u64, other: u64) -> bool {
    let x = h.cu(EE_UNITS_CU_UNIT_TILE_X, unit, other, 0, 0, &mut [0.0; 4]);
    let y = h.cu(EE_UNITS_CU_UNIT_TILE_Y, unit, other, 0, 0, &mut [0.0; 4]);
    if !is_on_other_continent(h, unit, x, y) {
        return false;
    }
    let s = &mut S { h, unit };
    let kind = s.qa(EE_UNITS_CU_UNIT_UNIT_TYPE, other);
    if kind == UNIT_TYPE_SPACE_DOCK || kind == UNIT_TYPE_SPACE_TURRET || kind == UNIT_TYPE_ORBITAL_SPACE_STATION {
        let planet = s.qa(EE_UNITS_CU_UNIT_PLANET, other);
        return s.q(EE_UNITS_CU_CONTINENT_NON_SPACE) != planet;
    }
    true
}

/// `CanReachTile(x, y)`: whether the unit can attack a tile from where it stands (melee: next to it; ballistic: a
/// clear arc; else in range and on the same continent).
pub fn can_reach_tile<H: CuHost + ?Sized>(h: &mut H, unit: u64, x: i64, y: i64) -> bool {
    if h.cu(EE_UNITS_CU_IS_MELEE, unit, 0, 0, 0, &mut [0.0; 4]) != 0 {
        return is_next_to_tile(h, unit, x, y);
    }
    let projectile = h.cu(EE_UNITS_CU_PROJECTILE_CLASS, unit, 0, 0, 0, &mut [0.0; 4]) as u64;
    if projectile != 0 {
        let category = h.cu(EE_UNITS_CU_CLASS_CATEGORY_OF, unit, projectile, 0, 0, &mut [0.0; 4]);
        if category == CATEGORY_PROJECTILE || category == CATEGORY_BOMB {
            h.cu(EE_UNITS_CU_SET_DESTINATION, unit, 0, x, y, &mut [0.0; 4]);
            if is_on_other_continent(h, unit, x, y) {
                return false;
            }
            let s = &mut S { h, unit };
            s.q(EE_UNITS_CU_APPROACH_ATTACH);
            s.q(EE_UNITS_CU_GET_POSITION);
            if !s.yes(EE_UNITS_CU_BALLISTIC_PITCH) {
                return false;
            }
            if s.yes(EE_UNITS_CU_FIRES_OVER_WALLS) {
                return true;
            }
            return s.qbc(EE_UNITS_CU_LINE_BLOCKED, x, y) == 0;
        }
    }
    let range = f32::from_bits(h.cu(EE_UNITS_CU_STATS_RANGE, unit, 0, 0, 0, &mut [0.0; 4]) as u32);
    let distance = f32::from_bits(h.cu(EE_UNITS_CU_DISTANCE_SQUARED_TO_TILE, unit, 0, x, y, &mut [0.0; 4]) as u32);
    if distance > range * range {
        return false;
    }
    !is_on_other_continent(h, unit, x, y)
}

/// `FindAreaTile(land, range)`: the unit's (or its container's) tile, or for a large unit not on a continent of the
/// wanted kind the first tile around it on one. (x, y).
pub fn find_area_tile<H: CuHost + ?Sized>(h: &mut H, unit: u64, land: bool, range: i64) -> (i64, i64) {
    let s = &mut S { h, unit };
    let mut u = s.obj(EE_UNITS_CU_CONTAINER);
    if u == 0 {
        u = unit;
    }
    let continent = s.qa(EE_UNITS_CU_CONTINENT_AT, u) as u64;
    let size = s.qa(EE_UNITS_CU_UNIT_TILE_SIZE, u);
    let mut result = (s.qab(EE_UNITS_CU_UNIT_TILE, u, 0), s.qab(EE_UNITS_CU_UNIT_TILE, u, 1));
    let flag = i64::from(land);
    if size > 1 && (continent == 0 || s.qa(EE_UNITS_CU_CONTINENT_CATEGORY, continent) != flag) {
        let end_x = s.q(EE_UNITS_CU_GET_TILE_X) + size + range;
        let end_y = s.q(EE_UNITS_CU_TILE_Y) + size + range;
        let mut x = s.q(EE_UNITS_CU_GET_TILE_X) - range;
        while x < end_x {
            let mut y = s.q(EE_UNITS_CU_TILE_Y) - range;
            while y < end_y {
                // `(unsigned long)x < gMapWidth`
                if (x as c_ulong as u64) >= s.qb(EE_UNITS_CU_MAP_SIZE, 0) as c_ulong as u64
                    || (y as c_ulong as u64) >= s.qb(EE_UNITS_CU_MAP_SIZE, 1) as c_ulong as u64
                {
                    y += 1;
                    continue;
                }
                let id = s.qbc(EE_UNITS_CU_CONTINENT_ID, x, y);
                let other = if id != 0 { s.qb(EE_UNITS_CU_CONTINENT_BY_ID, id) as u64 } else { 0 };
                if other == 0 || s.qa(EE_UNITS_CU_CONTINENT_CATEGORY, other) != flag {
                    y += 1;
                    continue;
                }
                result = (x, y);
                x = end_x;
                break;
            }
            x += 1;
        }
    }
    result
}

/// `FindNearestTown()`: the player's town containing the unit's tile, else the town whose outline is nearest.
pub fn find_nearest_town<H: CuHost + ?Sized>(h: &mut H, unit: u64) -> u64 {
    let s = &mut S { h, unit };
    let (x, y) = (s.q(EE_UNITS_CU_GET_TILE_X), s.q(EE_UNITS_CU_TILE_Y));
    let mut result = 0u64;
    let mut best: c_long = 0x7fff_ffff;
    let count = s.q(EE_UNITS_CU_NUM_TOWNS);
    for i in 0..count {
        let town = s.qb(EE_UNITS_CU_TOWN_AT, i) as u64;
        if s.qabc(EE_UNITS_CU_TOWN_CONTAINS, town, x, y) != 0 {
            result = town;
            break;
        }
        let distance = s.qabc(EE_UNITS_CU_TOWN_DISTANCE, town, x, y) as c_long;
        if distance < best {
            result = town;
            best = distance;
        }
    }
    result
}

/// `MoveToLocation(x, y, update_tiles)`: the unit to the tile under a point, its contacts, flight height and swim
/// animation refreshed.
pub fn move_to_location<H: CuHost + ?Sized>(h: &mut H, unit: u64, x: f32, y: f32, update: bool) {
    let s = &mut S { h, unit };
    let old_tile = s.obj(EE_UNITS_CU_TILE_POINTER);
    let tx = s.qb(EE_UNITS_CU_UM_FLOOR, bits(x));
    let ty = s.qb(EE_UNITS_CU_UM_FLOOR, bits(y));
    let moved = s.yes(EE_UNITS_CU_IS_COLLIDABLE) && (tx != s.q(EE_UNITS_CU_GET_TILE_X) || ty != s.q(EE_UNITS_CU_TILE_Y));
    s.qabc(EE_UNITS_CU_MOVE_TO_UPDATE, u64::from(update), tx, ty);
    if moved {
        s.q(EE_UNITS_CU_CLEAR_CONTACTS);
        crate::units::cu_body::find_contacts(s.h, unit);
    }
    if old_tile != s.obj(EE_UNITS_CU_TILE_POINTER)
        && s.q(EE_UNITS_CU_WAYPOINT_COUNT) != 0
        && (s.q(EE_UNITS_CU_CLASS_DOMAINS) & MOVEMENT_AIR != 0 || s.q(EE_UNITS_CU_UNIT_TYPE) == UNIT_TYPE_SP15_SPACE_FIGHTER)
    {
        s.q(EE_UNITS_CU_NEXT_WAYPOINT_POINT);
        s.q(EE_UNITS_CU_SET_HEIGHT_AT_POINT);
    }
    if !s.yes(EE_UNITS_CU_HAS_SWIM_ANIMATION) {
        return;
    }
    if !s.yes(EE_UNITS_CU_IS_SWIMMING) && s.q(EE_UNITS_CU_ANIMATION) == ANIMATION_WALK {
        s.qb(EE_UNITS_CU_SET_ANIMATION_STATE, ANIMATION_SWIM);
    } else if s.yes(EE_UNITS_CU_IS_SWIMMING) && s.q(EE_UNITS_CU_ANIMATION) == ANIMATION_SWIM {
        s.qb(EE_UNITS_CU_SET_ANIMATION_STATE, ANIMATION_WALK);
    }
}

/// `SetGrowth(scale)`: the growth scale, scaling the model and the collision radius (at least 0.05).
pub fn set_growth<H: CuHost + ?Sized>(h: &mut H, unit: u64, scale: f32) {
    let s = &mut S { h, unit };
    if scale == 1.0 && s.float(EE_UNITS_CU_GROWTH) == 1.0 {
        return;
    }
    s.qb(EE_UNITS_CU_SET_GROWTH, bits(scale));
    if scale >= 1.0 {
        let model_scale = s.q(EE_UNITS_CU_STATS_SCALE);
        s.qb(EE_UNITS_CU_PM_SET_SCALE, model_scale);
        let sphere = s.q(EE_UNITS_CU_INNER_SPHERE);
        s.qb(EE_UNITS_CU_SET_COLLISION_RADIUS, sphere);
    } else {
        let model_scale = s.float(EE_UNITS_CU_STATS_SCALE) * scale;
        s.qb(EE_UNITS_CU_PM_SET_SCALE, bits(model_scale));
        if s.float(EE_UNITS_CU_INNER_SPHERE) > 0.0 {
            let radius = s.float(EE_UNITS_CU_INNER_SPHERE) * s.float(EE_UNITS_CU_GROWTH);
            s.qb(EE_UNITS_CU_SET_COLLISION_RADIUS, bits(radius));
            if s.float(EE_UNITS_CU_COLLISION_RADIUS) < 0.05 {
                s.qb(EE_UNITS_CU_SET_COLLISION_RADIUS, bits(0.05));
            }
        }
    }
}

/// `MakeAreaEffects()`: the unit's area effects made again from the four templates of its stats.
pub fn make_area_effects<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let s = &mut S { h, unit };
    s.q(EE_UNITS_CU_AREA_EFFECTS_CLEAR);
    s.q(EE_UNITS_CU_AREA_EFFECTS_ITEMS_CLEAR);
    s.q(EE_UNITS_CU_ZERO_NUM_AREA_EFFECTS);
    for i in 0..4 {
        let template = s.qb(EE_UNITS_CU_STATS_AREA_TEMPLATE, i);
        if template <= 0 {
            continue;
        }
        let effect = s.qb(EE_UNITS_CU_CREATE_AREA_EFFECT, template) as u64;
        s.qa(EE_UNITS_CU_AREA_EFFECTS_ADD, effect);
        s.qa(EE_UNITS_CU_EFFECT_DEREFERENCE, effect);
        s.q(EE_UNITS_CU_COUNT_AREA_EFFECT);
    }
}

/// The squadron queues: 1..3, 1..4 with the paratroopers power.
fn last_squadron<H: CuHost + ?Sized>(s: &mut S<'_, H>) -> i64 {
    if s.yes(EE_UNITS_CU_HAS_PARATROOPERS) {
        4
    } else {
        3
    }
}

/// `QueueSquadronGoals(goal, clone)`: the goal (or clones of it) queued on the squadron queues.
pub fn queue_squadron_goals<H: CuHost + ?Sized>(h: &mut H, unit: u64, goal: u64, clone: bool) {
    let s = &mut S { h, unit };
    let last = last_squadron(s);
    for i in 1..=last {
        let g = if !clone && i <= 1 { goal } else { s.qa(EE_UNITS_CU_GOAL_CLONE, goal) as u64 };
        if s.qb(EE_UNITS_CU_QUEUED_GOAL, i) != 0 {
            s.qab(EE_UNITS_CU_QUEUE_GOAL, g, i);
        } else {
            s.qabc(EE_UNITS_CU_SET_GOAL, g, 0, i);
        }
    }
}

/// `SetSquadronGoals(goal, clone)`: the squadron queues' goals replaced by the goal or clones of it.
pub fn set_squadron_goals<H: CuHost + ?Sized>(h: &mut H, unit: u64, goal: u64, clone: bool) {
    let last = last_squadron(&mut S { h: &mut *h, unit });
    for i in 1..=last {
        clear_goals(h, unit, i);
        let s = &mut S { h: &mut *h, unit };
        s.qb(EE_UNITS_CU_CLEAR_GOAL_QUEUE, i);
        if !clone && i <= 1 {
            s.qabc(EE_UNITS_CU_SET_GOAL, goal, 0, i);
        } else {
            let g = s.qa(EE_UNITS_CU_GOAL_CLONE, goal) as u64;
            s.qabc(EE_UNITS_CU_SET_GOAL, g, 0, i);
        }
    }
}

/// `ApplyAreaEffects()`: a built unit's area effects applied to the units within its largest effect's radius.
pub fn apply_area_effects<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let s = &mut S { h, unit };
    let mut radius = 0.0f32;
    let player = s.q(EE_UNITS_CU_PLAYER_ID);
    if s.q(EE_UNITS_CU_AREA_EFFECTS_SIZE) <= 0 || !s.yes(EE_UNITS_CU_IS_BUILT) {
        return;
    }
    let mut k = 0;
    while k < s.q(EE_UNITS_CU_AREA_EFFECT_COUNT) {
        let r = float(s.qb(EE_UNITS_CU_AREA_EFFECT_RADIUS, k));
        if r > radius {
            radius = r;
        }
        k += 1;
    }
    s.qbc(EE_UNITS_CU_COLLECT_EFFECTS, bits(radius), player);
}

/// `GetEffect(kind, out)`: the graphic effect of the first area effect of a kind acting on the unit into `out`
/// (an `EEGraphicEffect**`); whether found.
pub fn get_effect<H: CuHost + ?Sized>(h: &mut H, unit: u64, kind: i64, out: u64) -> bool {
    let s = &mut S { h, unit };
    let mut k = 0;
    while k < s.q(EE_UNITS_CU_AFFECTING_COUNT) {
        if s.qb(EE_UNITS_CU_AFFECTING_KIND, k) == kind {
            s.qab(EE_UNITS_CU_SET_OUT_EFFECT, out, k);
            return true;
        }
        k += 1;
    }
    false
}

/// `PopBuildTarget()`: the citizen's build queue popped until a still-valid unit comes out (0 when it runs out).
pub fn pop_build_target<H: CuHost + ?Sized>(h: &mut H, unit: u64) -> u64 {
    let s = &mut S { h, unit };
    let mut result = 0u64;
    while result == 0 && s.q(EE_UNITS_CU_BUILD_TARGET_COUNT) > 0 {
        let target = s.obj(EE_UNITS_CU_CITIZEN_FRONT);
        s.q(EE_UNITS_CU_CITIZEN_POP_FRONT);
        if s.qa(EE_UNITS_CU_UNIT_VALID, target) != 0 {
            result = target;
        }
        s.qa(EE_UNITS_CU_DEREFERENCE, target);
    }
    result
}

/// `GoFanatic()`: the unit fanatic for a while, for a share of its hit points (at least 1 left), tinted.
pub fn go_fanatic<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let s = &mut S { h, unit };
    if s.yes(EE_UNITS_CU_IS_CLOAKED) {
        return;
    }
    s.q(EE_UNITS_CU_START_FANATICISM);
    let cost = s.float(EE_UNITS_CU_FANATICISM_COST);
    let mut hit_points = ((1.0 - cost) * s.q(EE_UNITS_CU_HIT_POINTS) as c_long as f32) as c_long as i64;
    if hit_points < 1 {
        hit_points = 1;
    }
    s.qb(EE_UNITS_CU_SET_HIT_POINTS, hit_points);
    s.qb(EE_UNITS_CU_SET_TINTED, 1);
}

/// `CanDelete()`: whether the unit may be deleted (a built unit of the citizen tech's types not with one capitol).
pub fn can_delete<H: CuHost + ?Sized>(h: &mut H, unit: u64) -> bool {
    let s = &mut S { h, unit };
    let mut result = s.yes(EE_UNITS_CU_IS_DELETABLE);
    if result && s.yes(EE_UNITS_CU_CITIZEN_TECH_HAS_TYPE) {
        result = !(s.yes(EE_UNITS_CU_IS_BUILT) && s.q(EE_UNITS_CU_NUM_CAPITOLS) == 1);
    }
    result
}

/// `ReleaseHeld(other)`: a unit removed from those this one holds; whether it was held.
pub fn release_held<H: CuHost + ?Sized>(h: &mut H, unit: u64, other: u64) -> bool {
    let s = &mut S { h, unit };
    let index = s.qa(EE_UNITS_CU_HELD_FIND, other);
    if index < 0 {
        return false;
    }
    s.qb(EE_UNITS_CU_HELD_ERASE, index);
    s.qa(EE_UNITS_CU_DEREFERENCE, other);
    true
}

/// `ReleaseHeldUnits()`: every held unit released, those this unit holds detached.
pub fn release_held_units<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let s = &mut S { h, unit };
    let mut i = s.q(EE_UNITS_CU_HELD_COUNT) - 1;
    while i >= 0 {
        if s.qb(EE_UNITS_CU_HELD_HOLDER_IS_THIS, i) != 0 {
            s.qb(EE_UNITS_CU_HELD_CLEAR_HOLDER, i);
        }
        s.qb(EE_UNITS_CU_HELD_DEREFERENCE, i);
        i -= 1;
    }
    s.q(EE_UNITS_CU_HELD_CLEAR);
}

/// `ClearBuildTargets()`: the citizen's build queue emptied, each building released.
pub fn clear_build_targets<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let s = &mut S { h, unit };
    while s.q(EE_UNITS_CU_BUILD_TARGET_COUNT) != 0 {
        s.q(EE_UNITS_CU_CITIZEN_FRONT_DEREFERENCE);
        s.q(EE_UNITS_CU_CITIZEN_POP_FRONT);
    }
    s.q(EE_UNITS_CU_CLEAR_SHIFT_QUEUED);
}

/// `ClearContacts()`: the contact list emptied, this unit removed from each contact's list too.
pub fn clear_contacts<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let s = &mut S { h, unit };
    let mut node = s.obj(EE_UNITS_CU_CONTACTS_HEAD);
    while node != 0 {
        s.qa(EE_UNITS_CU_CONTACT_REMOVE_THIS, node);
        s.qa(EE_UNITS_CU_CONTACT_DEREFERENCE, node);
        node = s.qa(EE_UNITS_CU_CONTACT_NEXT, node) as u64;
    }
    s.q(EE_UNITS_CU_CONTACTS_CLEAR);
}

/// `RemoveContactUnit(other)`: a unit removed from this unit's contact list.
pub fn remove_contact_unit<H: CuHost + ?Sized>(h: &mut H, unit: u64, other: u64) {
    let s = &mut S { h, unit };
    let node = s.qa(EE_UNITS_CU_CONTACTS_FIND, other) as u64;
    if node != 0 {
        s.qa(EE_UNITS_CU_CONTACTS_REMOVE, node);
        s.qa(EE_UNITS_CU_DEREFERENCE, other);
    }
}

/// `RemoveAffectingAreaEffects()`: the unit taken out of every area effect acting on it, the list emptied.
pub fn remove_affecting_area_effects<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let s = &mut S { h, unit };
    let mut k = 0;
    while k < s.q(EE_UNITS_CU_AFFECTING_COUNT) {
        s.qb(EE_UNITS_CU_AFFECTING_REMOVED_FROM, k);
        k += 1;
    }
    s.q(EE_UNITS_CU_AFFECTING_CLEAR);
    s.q(EE_UNITS_CU_AFFECTING_ITEMS_CLEAR);
}

/// `ReleaseConversionEffect()`: the conversion graphic effect destroyed, if any.
pub fn release_conversion_effect<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let s = &mut S { h, unit };
    if !s.yes(EE_UNITS_CU_CONVERSION_IS_NONE) {
        s.q(EE_UNITS_CU_DESTROY_CONVERSION_EFFECT);
        s.q(EE_UNITS_CU_RESET_CONVERSION_EFFECT);
    }
}

/// `SetHolder(other)`: the unit that holds this one, the references swapped.
pub fn set_holder<H: CuHost + ?Sized>(h: &mut H, unit: u64, other: u64) {
    let s = &mut S { h, unit };
    let holder = s.obj(EE_UNITS_CU_HOLDER);
    if holder == other {
        return;
    }
    if holder != 0 {
        s.qa(EE_UNITS_CU_DEREFERENCE, holder);
    }
    s.qa(EE_UNITS_CU_SET_HOLDER_FIELD, other);
    s.qa(EE_UNITS_CU_REFERENCE, other);
}

/// `SetMine(other)`: the unit's mine (reference counted).
pub fn set_mine<H: CuHost + ?Sized>(h: &mut H, unit: u64, other: u64) {
    let s = &mut S { h, unit };
    let mine = s.obj(EE_UNITS_CU_MINE);
    if mine != 0 {
        s.qa(EE_UNITS_CU_DEREFERENCE, mine);
    }
    s.qa(EE_UNITS_CU_SET_MINE_FIELD, other);
    if other != 0 {
        s.qa(EE_UNITS_CU_REFERENCE, other);
    }
}

/// `SetBlockingUnit(other)` and `SetLastResource(other)`: a reference-counted link (`get`, `set`).
fn set_link<H: CuHost + ?Sized>(h: &mut H, unit: u64, other: u64, get: i32, set: i32) {
    let s = &mut S { h, unit };
    let old = s.obj(get);
    if old != 0 {
        s.qa(EE_UNITS_CU_DEREFERENCE, old);
        s.qa(set, 0);
    }
    if other != 0 {
        s.qa(set, other);
        s.qa(EE_UNITS_CU_REFERENCE, other);
    }
}

/// `SetHomeBase(base)`: the unit's home base (reference counted).
pub fn set_home_base<H: CuHost + ?Sized>(h: &mut H, unit: u64, base: u64) {
    let s = &mut S { h, unit };
    let old = s.obj(EE_UNITS_CU_HOME_BASE);
    if old != 0 {
        s.qa(EE_UNITS_CU_DEREFERENCE, old);
    }
    if base != 0 {
        s.qa(EE_UNITS_CU_REFERENCE, base);
    }
    s.qa(EE_UNITS_CU_SET_HOME_BASE_FIELD, base);
}

/// `SetPatrolGroup(group)`: the unit's patrol group (reference counted).
pub fn set_patrol_group<H: CuHost + ?Sized>(h: &mut H, unit: u64, group: u64) {
    let s = &mut S { h, unit };
    if s.obj(EE_UNITS_CU_PATROL_GROUP) != 0 {
        s.q(EE_UNITS_CU_PATROL_DEREFERENCE);
    }
    s.qa(EE_UNITS_CU_SET_PATROL_GROUP_FIELD, group);
    if group != 0 {
        s.qa(EE_UNITS_CU_PATROL_REFERENCE, group);
    }
}

/// `FindPassenger(id)`: the cargo unit of an id, or 0.
pub fn find_passenger<H: CuHost + ?Sized>(h: &mut H, unit: u64, id: i64) -> u64 {
    let s = &mut S { h, unit };
    let count = s.q(EE_UNITS_CU_NUM_CARGO_UNITS);
    for i in 0..count {
        if id == s.qb(EE_UNITS_CU_CARGO_ID_AT, i) {
            return s.qb(EE_UNITS_CU_CARGO_AT, i) as u64;
        }
    }
    0
}

/// `UnloadCargoByID(id, location)`: the cargo unit of an id unloaded at the location (a `GETransformation*`);
/// what `UnloadCargoAt` answers, or 0.
pub fn unload_cargo_by_id<H: CuHost + ?Sized>(h: &mut H, unit: u64, id: i64, location: u64) -> u64 {
    let s = &mut S { h, unit };
    let count = s.q(EE_UNITS_CU_NUM_CARGO_UNITS);
    for i in 0..count {
        if id == s.qb(EE_UNITS_CU_CARGO_ID_AT, i) {
            return s.qab(EE_UNITS_CU_UNLOAD_CARGO_AT_INDEX, location, i) as u64;
        }
    }
    0
}

/// `SetRandomReloadTimer`'s function statics: the world table's range, read once.
static RELOAD_RANGE: std::sync::Mutex<Option<(i64, i64)>> = std::sync::Mutex::new(None);

/// `SetRandomReloadTimer(reload)`: the reload timer set to a reload time scaled by a random factor of the world
/// table's range (read once per process, as the original's statics).
pub fn set_random_reload_timer<H: CuHost + ?Sized>(h: &mut H, unit: u64, reload: i64, range: &mut Option<(i64, i64)>) {
    let s = &mut S { h, unit };
    if range.is_none() {
        let minimum = s.qb(EE_UNITS_CU_RELOAD_RANGE, 0);
        let maximum = s.qb(EE_UNITS_CU_RELOAD_RANGE, 1);
        *range = Some((minimum, maximum));
    }
    let (minimum, maximum) = range.unwrap_or_default();
    let factor = float(s.qbc(EE_UNITS_CU_RANDOM_BETWEEN, minimum, maximum));
    let timer = s.qb(EE_UNITS_CU_FLOAT_TO_LONG, bits(factor * reload as c_long as f32));
    s.qb(EE_UNITS_CU_SET_RELOAD_TIMER, timer);
}

/// Runs body `what` (`EE_UNITS_CU_DO_*` of this module) on `unit`; None for another module's.
pub fn run<H: CuHost + ?Sized>(h: &mut H, what: i32, unit: u64, a: i64, b: i64, c: i64) -> Option<i64> {
    let (ua, ub) = (a as u64, b as u64);
    Some(match what {
        EE_UNITS_CU_DO_CONDITION_TARGET => condition_target(h, unit) as i64,
        EE_UNITS_CU_DO_REPLACE_UNIT => {
            replace_unit(h, unit, ua, ub);
            0
        }
        EE_UNITS_CU_DO_REPLACE_UNITS => {
            replace_units(h, unit, ua);
            0
        }
        EE_UNITS_CU_DO_IS_PASSABLE_BY => i64::from(is_passable_by(h, unit, ua)),
        EE_UNITS_CU_DO_CAN_REACH_TILE => i64::from(can_reach_tile(h, unit, a, b)),
        EE_UNITS_CU_DO_IS_ON_OTHER_CONTINENT => i64::from(is_on_other_continent(h, unit, a, b)),
        EE_UNITS_CU_DO_IS_ON_OTHER_CONTINENT_THAN => i64::from(is_on_other_continent_than(h, unit, ua)),
        EE_UNITS_CU_DO_FIND_AREA_TILE => {
            let (x, y) = find_area_tile(h, unit, a != 0, b);
            i64::from(x as u32) | i64::from(y as u32) << 32
        }
        EE_UNITS_CU_DO_FIND_NEAREST_TOWN => find_nearest_town(h, unit) as i64,
        EE_UNITS_CU_DO_MOVE_TO_LOCATION => {
            move_to_location(h, unit, float(a), float(b), c != 0);
            0
        }
        EE_UNITS_CU_DO_SET_GROWTH => {
            set_growth(h, unit, float(a));
            0
        }
        EE_UNITS_CU_DO_MAKE_AREA_EFFECTS => {
            make_area_effects(h, unit);
            0
        }
        EE_UNITS_CU_DO_QUEUE_SQUADRON_GOALS => {
            queue_squadron_goals(h, unit, ua, b != 0);
            0
        }
        EE_UNITS_CU_DO_SET_SQUADRON_GOALS => {
            set_squadron_goals(h, unit, ua, b != 0);
            0
        }
        EE_UNITS_CU_DO_APPLY_AREA_EFFECTS => {
            apply_area_effects(h, unit);
            0
        }
        EE_UNITS_CU_DO_GET_EFFECT => i64::from(get_effect(h, unit, a, ub)),
        EE_UNITS_CU_DO_POP_BUILD_TARGET => pop_build_target(h, unit) as i64,
        EE_UNITS_CU_DO_GO_FANATIC => {
            go_fanatic(h, unit);
            0
        }
        EE_UNITS_CU_DO_CAN_DELETE => i64::from(can_delete(h, unit)),
        EE_UNITS_CU_DO_RELEASE_HELD => i64::from(release_held(h, unit, ua)),
        EE_UNITS_CU_DO_RELEASE_HELD_UNITS => {
            release_held_units(h, unit);
            0
        }
        EE_UNITS_CU_DO_CLEAR_BUILD_TARGETS => {
            clear_build_targets(h, unit);
            0
        }
        EE_UNITS_CU_DO_CLEAR_CONTACTS => {
            clear_contacts(h, unit);
            0
        }
        EE_UNITS_CU_DO_REMOVE_CONTACT_UNIT => {
            remove_contact_unit(h, unit, ua);
            0
        }
        EE_UNITS_CU_DO_REMOVE_AFFECTING => {
            remove_affecting_area_effects(h, unit);
            0
        }
        EE_UNITS_CU_DO_RELEASE_CONVERSION_EFFECT => {
            release_conversion_effect(h, unit);
            0
        }
        EE_UNITS_CU_DO_SET_HOLDER => {
            set_holder(h, unit, ua);
            0
        }
        EE_UNITS_CU_DO_SET_MINE => {
            set_mine(h, unit, ua);
            0
        }
        EE_UNITS_CU_DO_SET_BLOCKING_UNIT => {
            set_link(h, unit, ua, EE_UNITS_CU_BLOCKING_UNIT, EE_UNITS_CU_SET_BLOCKING_FIELD);
            0
        }
        EE_UNITS_CU_DO_SET_LAST_RESOURCE => {
            set_link(h, unit, ua, EE_UNITS_CU_LAST_RESOURCE, EE_UNITS_CU_SET_LAST_RESOURCE_FIELD);
            0
        }
        EE_UNITS_CU_DO_SET_HOME_BASE => {
            set_home_base(h, unit, ua);
            0
        }
        EE_UNITS_CU_DO_SET_PATROL_GROUP => {
            set_patrol_group(h, unit, ua);
            0
        }
        EE_UNITS_CU_DO_FIND_PASSENGER => find_passenger(h, unit, a) as i64,
        EE_UNITS_CU_DO_UNLOAD_CARGO_BY_ID => unload_cargo_by_id(h, unit, a, ub) as i64,
        EE_UNITS_CU_DO_SET_RANDOM_RELOAD_TIMER => {
            let mut range = *RELOAD_RANGE.lock().unwrap_or_else(|e| e.into_inner());
            set_random_reload_timer(h, unit, a, &mut range);
            *RELOAD_RANGE.lock().unwrap_or_else(|e| e.into_inner()) = range;
            0
        }
        _ => return None,
    })
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::cu_body::game::Game;
    use std::os::raw::c_void;

    /// `EEComplexUnit::SightMoved(old x, old y, new x, new y)` on `unit` (the tiles as `long`s).
    ///
    /// # Safety
    /// `unit` is a live `EEComplexUnit`, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_cu_sight_moved(unit: *mut c_void, ox: i64, oy: i64, nx: i64, ny: i64) {
        sight_moved(&mut Game, unit as usize as u64, ox, oy, nx, ny);
    }
}

#[cfg(test)]
#[path = "cu_world_tests.rs"]
mod tests;
