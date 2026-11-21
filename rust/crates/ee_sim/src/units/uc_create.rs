//! `EEUnitClass`'s unit creation, launch and death bodies over their C++ statements (the units' lane, round 7; were
//! EEUnitClass.cpp): `CreateUnitAtTile`/`CreateUnitAtPoint` (their shared body), `BaseCreateUnitAt`,
//! `BaseCreateUnit`, `ReplantFarm`, `CreateRemainsUnit`, `LaunchShot`, `LaunchFromTurret`, `UnitDied`,
//! `FindSafeLocation`, `MoveToTileCentre`, `BaseUnitCreated`. As uc_body.rs: each C++ statement one
//! `EE_UNITS_UC_*` case of `ee_units_uc_host`, asked in the original's order; the transformations are Rust buffers
//! (ee_base's `GeTransformation`, the functions the C++ inlines called), handed to a statement by address; the
//! launch is ee_combat's (launch.rs) as the C++ called it; the unit utilities' searches are their FFI functions
//! ([`UcCreateRust`]), with their notes.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::combat::launch::{launch, launch_index, EeCombatLaunchIn, EeCombatLaunchOut};
use crate::units::uc_body::*;
use crate::units::uc_produce::*;
use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{Ge3DPoint, GeTransformation};
use ee_base::math::um::{floor, um_arctan2};
use std::os::raw::c_long;

// ---- the statements (from 300; `class` the body's class, `unit` its unit, `a` an object, `b`, `c` values)
/// `class->mIsComplex`
pub const EE_UNITS_UC_IS_COMPLEX: i32 = 300;
/// `class->mPitchesWithTerrain`
pub const EE_UNITS_UC_PITCHES: i32 = 301;
/// `class->mRollsWithTerrain`
pub const EE_UNITS_UC_ROLLS: i32 = 302;
/// `EEUnit::Create(class, b, c, player a)`
pub const EE_UNITS_UC_SIMPLE_CREATE: i32 = 303;
/// `class->SimpleUnitCreated(unit a)`
pub const EE_UNITS_UC_SIMPLE_UNIT_CREATED: i32 = 304;
/// map `a`'s `GetGroundPoint(f[0..3], f[0..3])`
pub const EE_UNITS_UC_GROUND_POINT: i32 = 305;
/// `class->GroundLocation(f[0..3])`
pub const EE_UNITS_UC_GROUND_LOCATION: i32 = 306;
/// `f[0] = ` map `a`'s `GetTerrainPitch(the transformation at b)`
pub const EE_UNITS_UC_TERRAIN_PITCH: i32 = 307;
/// `f[0] = ` map `a`'s `GetTerrainRoll(the transformation at b)`
pub const EE_UNITS_UC_TERRAIN_ROLL: i32 = 308;
/// `class->GetNumGoalQueues()`
pub const EE_UNITS_UC_NUM_GOAL_QUEUES: i32 = 309;
/// `class->LevelGround(&map a->mTerrainMesh, b, c, &f[0])`
pub const EE_UNITS_UC_LEVEL_GROUND: i32 = 310;
/// `EEComplexUnit::Create(class, the transformation at a, player b, c)`
pub const EE_UNITS_UC_COMPLEX_CREATE: i32 = 311;
/// `class->UnitCreated(complex unit a)`
pub const EE_UNITS_UC_UNIT_CREATED: i32 = 312;
/// player `a`'s `mPendingUnits.push_back(unit b)`
pub const EE_UNITS_UC_PENDING_PUSH: i32 = 313;
/// unit `a`'s `Reference()`
pub const EE_UNITS_UC_REFERENCE: i32 = 314;
/// `++class->mNumUnits`
pub const EE_UNITS_UC_NUM_UNITS_INC: i32 = 315;
/// unit `a`'s `Dereference()`
pub const EE_UNITS_UC_DEREFERENCE: i32 = 316;
/// `gWorld.mMap`
pub const EE_UNITS_UC_WORLD_MAP: i32 = 317;
/// `gWorld.mPlayers[b]`
pub const EE_UNITS_UC_PLAYER_AT: i32 = 318;
/// `gFreeCamera`
pub const EE_UNITS_UC_FREE_CAMERA: i32 = 319;
/// player `a`'s `CheckButtonsPreloaded(class->mUnitType)`
pub const EE_UNITS_UC_CHECK_BUTTONS: i32 = 320;
/// complex unit `a`'s `mPlanetID = EEGetLandContinentOf(a)`
pub const EE_UNITS_UC_SET_PLANET: i32 = 321;
/// complex unit `a`'s `mOrientation = GEOrientation(f[0], f[1], f[2])`
pub const EE_UNITS_UC_SET_ORIENTATION: i32 = 322;
/// player `a`'s `LoadUnitClassMedia(class->mUnitType)`
pub const EE_UNITS_UC_LOAD_CLASS_MEDIA_OF: i32 = 323;
/// unit `a`'s `LoadMedia(b != 0)`
pub const EE_UNITS_UC_UNIT_LOAD_MEDIA: i32 = 324;
/// `gWorld.AddUnitEntry(unit a, unit b)`
pub const EE_UNITS_UC_ADD_UNIT_ENTRY: i32 = 325;
/// `class->CreateRunways(complex unit a, player b, c != 0)`
pub const EE_UNITS_UC_CREATE_RUNWAYS: i32 = 326;
/// `class->mAttachedEffect`
pub const EE_UNITS_UC_ATTACHED_EFFECT: i32 = 327;
/// `gEffectManager->CreateUnitEffect(unit a, class->mAttachedEffect, NULL, b != 0, 0, 0)`
pub const EE_UNITS_UC_ATTACHED_EFFECT_ON: i32 = 328;
/// `EEUnitStoreInsertUnlisted(unit a)`
pub const EE_UNITS_UC_INSERT_UNLISTED: i32 = 329;
/// `EEGetFarmType(player a)`
pub const EE_UNITS_UC_FARM_TYPE: i32 = 330;
/// `gUnitClassTable[b]`
pub const EE_UNITS_UC_CLASS_AT: i32 = 331;
/// class `a`'s `mTileSize`
pub const EE_UNITS_UC_CLASS_TILE_SIZE: i32 = 332;
/// player `a`'s `GetPlayerID()`
pub const EE_UNITS_UC_PLAYER_ID: i32 = 333;
/// player `a`'s `CanProduce(class b->GetTechnology(), 0x20, 1, false, 3, NULL)`
pub const EE_UNITS_UC_CAN_PRODUCE: i32 = 334;
/// class `a`'s `CreateUnit(b's low 32 bits, b's high 32 bits, c, false, NULL)`
pub const EE_UNITS_UC_CREATE_UNIT_FOR: i32 = 335;
/// unit `a`'s `SetHitPoints(b)`
pub const EE_UNITS_UC_SET_HIT_POINTS_OF: i32 = 336;
/// unit `a`'s `MoveTo(b, c, true)`
pub const EE_UNITS_UC_MOVE_TO: i32 = 337;
/// complex unit `a`'s `SetAnimation(b, -1, 0)`
pub const EE_UNITS_UC_SET_ANIMATION_OF: i32 = 340;
/// the transformation at `b` = complex unit `a`'s `mPhysicalModel.GetLocation()`
pub const EE_UNITS_UC_LOCATION_MATRIX: i32 = 341;
/// complex unit `a`'s `mPhysicalModel.SetLocation(the transformation at b)`
pub const EE_UNITS_UC_SET_PHYSICAL_LOCATION: i32 = 342;
/// `class->mRemainsClass`
pub const EE_UNITS_UC_REMAINS_CLASS: i32 = 343;
/// `unit->mPlayer->GetPlayerID()`
pub const EE_UNITS_UC_UNIT_PLAYER_ID: i32 = 344;
/// class `a`'s `CreateUnitAt(the transformation at b, c, false, NULL)`
pub const EE_UNITS_UC_CREATE_AT_FOR: i32 = 345;
/// class `a`'s `CreateUnitAt(the transformation at b, c, false, unit)`
pub const EE_UNITS_UC_CREATE_AT_BY: i32 = 346;
/// class `a`'s `CreateUnit(b's low 32 bits, b's high 32 bits, c, false, unit)`
pub const EE_UNITS_UC_CREATE_UNIT_BY: i32 = 347;
/// `class->mProjectileClass`
pub const EE_UNITS_UC_PROJECTILE_CLASS: i32 = 348;
/// `class->mCreationEffect`
pub const EE_UNITS_UC_CREATION_EFFECT: i32 = 349;
/// `gEffectManager->CreateUnitEffect(unit a, class->mCreationEffect, NULL, false, 0, 0)`
pub const EE_UNITS_UC_CREATION_EFFECT_ON: i32 = 350;
/// `gWorld.GetEffectManager()->CreateUnitEffect(unit a, class->mCreationEffect, NULL, false, 0, 0)`
pub const EE_UNITS_UC_WORLD_CREATION_EFFECT_ON: i32 = 351;
/// complex unit `a`'s `SetLauncherClass(class)`
pub const EE_UNITS_UC_SET_LAUNCHER: i32 = 352;
/// `EEUGRust::NewAttackUnit(complex unit a, unit b, false, false)`
pub const EE_UNITS_UC_NEW_ATTACK_UNIT: i32 = 353;
/// `f[0] = unit->mVelocity`
pub const EE_UNITS_UC_VELOCITY_FIELD: i32 = 354;
/// `f[0] = unit->GetVelocity()`
pub const EE_UNITS_UC_GET_VELOCITY: i32 = 355;
/// complex unit `a`'s `SetVelocity(f[0])`
pub const EE_UNITS_UC_SET_VELOCITY_OF: i32 = 356;
/// complex unit `a`'s `SetHeight(f[0])`
pub const EE_UNITS_UC_SET_HEIGHT_OF: i32 = 357;
/// class `a`'s `LaunchProjectile(complex unit b, GE3DPoint(f[0], f[1], f[2]))`
pub const EE_UNITS_UC_LAUNCH_PROJECTILE: i32 = 358;
/// `unit->GetModel()->mTagPoints[unit->mAnimation]`
pub const EE_UNITS_UC_TAG_LIST: i32 = 359;
/// tag point list `a`'s `GetNumTagPoints()`
pub const EE_UNITS_UC_TAG_COUNT: i32 = 360;
/// `unit->mIsSpecialAttack`
pub const EE_UNITS_UC_SPECIAL_ATTACK: i32 = 361;
/// `class->mHasSpecialAttack`
pub const EE_UNITS_UC_HAS_SPECIAL_ATTACK: i32 = 362;
/// `unit->mWeaponTagPoint`
pub const EE_UNITS_UC_WEAPON_TAG: i32 = 363;
/// `f[0] = unit->mStats->GetScale()`
pub const EE_UNITS_UC_STATS_SCALE: i32 = 364;
/// `f[0] = unit->mStats->mScale`
pub const EE_UNITS_UC_STATS_SCALE_FIELD: i32 = 365;
/// the shared chain's `clear()`
pub const EE_UNITS_UC_CHAIN_RESET: i32 = 366;
/// `EECombatTagPointChain(the animation's tag point list->GetTagPoint(c), unit->mPhysicalModel.GetFrame(),
/// launch b's point, the chain, launch b's chain)`
pub const EE_UNITS_UC_TAG_CHAIN: i32 = 367;
/// the chain's `data()` (its floats)
pub const EE_UNITS_UC_CHAIN_DATA: i32 = 368;
/// `EECombatChainCapture(a, launch input b, the chain, launch output c)`
pub const EE_UNITS_UC_CHAIN_CAPTURE: i32 = 369;
/// `gWorld.mMap->GetTile(b, c)`
pub const EE_UNITS_UC_GET_TILE: i32 = 370;
/// the closest tile to `f[0..3]` from (`b`, `c`): `gWorld.mMap->GetClosestTile`, into the tile scratch
pub const EE_UNITS_UC_CLOSEST_TILE: i32 = 371;
/// the tile scratch's x
pub const EE_UNITS_UC_TILE_OUT_X: i32 = 372;
/// the tile scratch's y
pub const EE_UNITS_UC_TILE_OUT_Y: i32 = 373;
/// `gUnitClassTable[gCivPowersManager.GetPowerRecord(kCivPowerFlamingArrows)->mUnitType]`
pub const EE_UNITS_UC_FLAMING_CLASS: i32 = 374;
/// `unit->CanAttackClass(unit a->GetUnitClass())`
pub const EE_UNITS_UC_CAN_ATTACK_CLASS: i32 = 375;
/// `unit->StartAction(b)`
pub const EE_UNITS_UC_START_ACTION_SELF: i32 = 376;
/// `unit->mIsSpecialAttack = false`
pub const EE_UNITS_UC_CLEAR_SPECIAL: i32 = 377;
/// complex unit `a`'s `GetUnitClass()`
pub const EE_UNITS_UC_CLASS_OF: i32 = 378;
/// `EEMovePassengers(complex unit a, unit, false)`
pub const EE_UNITS_UC_MOVE_PASSENGERS: i32 = 379;
/// `unit->GetModel()->FindTurretPart(the animation's tag point list->GetTagPoint(unit->mWeaponTagPoint)->mModel,
/// the part scratch)`
pub const EE_UNITS_UC_FIND_TURRET_PART: i32 = 380;
/// the part scratch
pub const EE_UNITS_UC_TURRET_PART: i32 = 381;
/// `f[0] = unit->mTurretAngles[b]`
pub const EE_UNITS_UC_TURRET_ANGLE: i32 = 382;
/// `class->mDeathClass`
pub const EE_UNITS_UC_DEATH_CLASS: i32 = 383;
/// `unit->mTile`
pub const EE_UNITS_UC_UNIT_TILE: i32 = 384;
/// `class->mDeathClass->CreateUnit(unit->GetTileX(), unit->GetTileY(), gWorld.mPlayers[0]->GetPlayerID(), 0, NULL)`
pub const EE_UNITS_UC_CREATE_DEATH_CLASS_UNIT: i32 = 385;
/// `class->mDeathEffect`
pub const EE_UNITS_UC_DEATH_EFFECT: i32 = 386;
/// `gEffectManager->CreateUnitEffect(unit, class->mDeathEffect, NULL, 0, 0, 0)`
pub const EE_UNITS_UC_DEATH_EFFECT_ON: i32 = 387;
/// `unit->mStats->mDeathSound`
pub const EE_UNITS_UC_DEATH_SOUND: i32 = 388;
/// `gSoundManager->Play(sound a->mSampleID, f[0], f[1], f[2], sound a->mLooping, NULL)`
pub const EE_UNITS_UC_PLAY_SOUND: i32 = 389;
/// `unit->SetCurrentPhysicsAction(b)`
pub const EE_UNITS_UC_SET_PHYSICS_ACTION: i32 = 390;
/// `unit->mUnitClass->mIsWall`
pub const EE_UNITS_UC_UNIT_IS_WALL: i32 = 391;
/// `unit->SetIsCommandable(false)`
pub const EE_UNITS_UC_NOT_COMMANDABLE: i32 = 392;
/// `unit->mPlayer->mComputerPlayer`
pub const EE_UNITS_UC_COMPUTER_PLAYER: i32 = 393;
/// `EEUGRust::NewLocation(GE2DPoint(f[0], f[1]), false, true, false, 0.0f, false, false)`
pub const EE_UNITS_UC_NEW_LOCATION_2D: i32 = 394;
/// `unit->SetGoal(goal a, 0, 0)`
pub const EE_UNITS_UC_SET_GOAL_SELF: i32 = 395;
/// `unit->mUnitClass->ActionChanged(unit)`
pub const EE_UNITS_UC_ACTION_CHANGED_SELF: i32 = 396;
/// `unit->mGoalState`
pub const EE_UNITS_UC_GOAL_STATE_SELF: i32 = 397;
/// `unit->mUnitClass->mCanFly`
pub const EE_UNITS_UC_CAN_FLY: i32 = 398;
/// `f[0] = unit->mHeight`
pub const EE_UNITS_UC_HEIGHT_FIELD: i32 = 399;
/// `f[0] = unit->mTile->mHeight`
pub const EE_UNITS_UC_TILE_HEIGHT: i32 = 400;
/// `f[0] = gWorldTable.GetFloatValue(b)`
pub const EE_UNITS_UC_WORLD_FLOAT: i32 = 401;
/// `EEUpdateUnitTile(the transformation at a, unit, the tile scratch)`
pub const EE_UNITS_UC_UPDATE_UNIT_TILE: i32 = 402;
/// `unit->SetHealSearchTimer(class->mHealDelay)`
pub const EE_UNITS_UC_SET_HEAL_SEARCH_TIMER: i32 = 403;
/// `unit->SetHitPoints(unit->mStats->mMaxHitPoints)`
pub const EE_UNITS_UC_SET_FULL_HIT_POINTS: i32 = 404;
/// `unit->SetLastHitPoints(unit->mStats->mMaxHitPoints)`
pub const EE_UNITS_UC_SET_FULL_LAST_HIT_POINTS: i32 = 405;
/// `unit->SetAmmunition(class->mAmmunition)`
pub const EE_UNITS_UC_SET_AMMUNITION: i32 = 406;
/// `unit->SetBehavior(class->mBehavior)`
pub const EE_UNITS_UC_SET_BEHAVIOR: i32 = 407;
/// `class->ActionChanged(unit)`
pub const EE_UNITS_UC_CLASS_ACTION_CHANGED: i32 = 408;

// ---- the bodies (`ee_units_uc_call`'s `what`)
pub const EE_UNITS_UC_DO_BASE_CREATE_UNIT_AT: i32 = 21;
pub const EE_UNITS_UC_DO_BASE_CREATE_UNIT: i32 = 22;
pub const EE_UNITS_UC_DO_REPLANT_FARM: i32 = 23;
pub const EE_UNITS_UC_DO_CREATE_REMAINS_UNIT: i32 = 24;
pub const EE_UNITS_UC_DO_LAUNCH_SHOT: i32 = 25;
pub const EE_UNITS_UC_DO_LAUNCH_FROM_TURRET: i32 = 26;
pub const EE_UNITS_UC_DO_UNIT_DIED: i32 = 27;
pub const EE_UNITS_UC_DO_FIND_SAFE_LOCATION: i32 = 28;
pub const EE_UNITS_UC_DO_MOVE_TO_TILE_CENTRE: i32 = 29;
pub const EE_UNITS_UC_DO_BASE_UNIT_CREATED: i32 = 30;

const UNIT_TYPE_PATH_POINT: i64 = 0xdf;
const UNIT_TYPE_SPACE_DOCK: i64 = 0x2d8;
const UNIT_TYPE_SPACE_TURRET: i64 = 0x302;
const UNIT_TYPE_ORBITAL_SPACE_STATION: i64 = 0x31f;
const UNIT_TYPE_AIRPORT: i64 = 0x42;
const UNIT_TYPE_SP15_CAPITAL_SHIP: i64 = 0x2da;
const UNIT_TYPE_CAPITAL_SHIP_YAMATO: i64 = 0x33c;
const UNIT_TYPE_X04_ROMAN_LEGIONNAIRE: i64 = 0x2dc;
const UNIT_TYPE_CP_ICBM: i64 = 0x2ee;
const UNIT_TYPE_ROBOTIC_FARM: i64 = 0x33a;
const CATEGORY_BUILDING: i64 = 2;
const CATEGORY_MINING_BUILDING: i64 = 0x18;
const CATEGORY_GATE: i64 = 0x19;
const CATEGORY_FORTRESS: i64 = 0x1d;
const CATEGORY_BOMB: i64 = 0xb;
const MOVEMENT_AIR: i64 = 1;
const ACTION_IDLE: i64 = 5000;
const ACTION_PREPARE_TO_MOVE: i64 = 8000;
/// `kWorldTileStrayDistance` (the world table's row)
const WORLD_TILE_STRAY_DISTANCE: i64 = 78;
/// `EE_COMBAT_TAG('C', 'B', 'L', 'S')`, `EE_COMBAT_TAG('C', 'B', 'L', 'T')`
const TAG_CBLS: u64 = u32::from_be_bytes(*b"CBLS") as u64;
const TAG_CBLT: u64 = u32::from_be_bytes(*b"CBLT") as u64;
/// `EE_COMBAT_LAUNCH_KIND_SHOT`, `EE_COMBAT_LAUNCH_KIND_TURRET`
const LAUNCH_KIND_SHOT: i32 = crate::combat::launch::EE_COMBAT_LAUNCH_KIND_SHOT;
const LAUNCH_KIND_TURRET: i32 = crate::combat::launch::EE_COMBAT_LAUNCH_KIND_TURRET;
/// the wall pieces' yaws (`kWallRotations`)
const WALL_ROTATIONS: [f32; 4] = [0.0, 3.1415927, -1.5707964, 1.5707964];
/// `3.1415927f` (the created unit's yaw)
const YAW_PI: f32 = 3.1415927;

/// The unit utilities' Rust these bodies call (their FFI functions, as the C++ called them) and the launch chain.
pub trait UcCreateRust: UcRust {
    /// `ee_units_uu_block_takes(player, unit_type, x, y, w, h, 0)`
    #[allow(clippy::too_many_arguments)]
    fn block_takes(&mut self, player: u64, unit_type: i32, x: i32, y: i32, w: i32, h: i32) -> bool;
    /// `ee_units_uu_escape_point(unit, &point)`: the point when one is found
    fn escape_point(&mut self, unit: u64) -> Option<[f32; 2]>;
    /// the shared chain (`EE_UNITS_UC_CHAIN_DATA`) as `count` matrices
    fn chain(&mut self, class: u64, unit: u64, count: i32) -> Vec<[f32; 16]>;
}

/// One body's statements on its class and unit.
pub(crate) struct S<'h, H: UcHost + ?Sized> {
    pub(crate) h: &'h mut H,
    pub(crate) class: u64,
    pub(crate) unit: u64,
}

impl<H: UcHost + ?Sized> S<'_, H> {
    pub(crate) fn s(&mut self, op: i32) -> i64 {
        self.h.uc(op, self.class, self.unit, 0, 0, 0, &mut [0.0; 4])
    }
    pub(crate) fn a(&mut self, op: i32, a: u64) -> i64 {
        self.h.uc(op, self.class, self.unit, a, 0, 0, &mut [0.0; 4])
    }
    pub(crate) fn b(&mut self, op: i32, b: i64) -> i64 {
        self.h.uc(op, self.class, self.unit, 0, b, 0, &mut [0.0; 4])
    }
    pub(crate) fn ab(&mut self, op: i32, a: u64, b: i64) -> i64 {
        self.h.uc(op, self.class, self.unit, a, b, 0, &mut [0.0; 4])
    }
    pub(crate) fn abc(&mut self, op: i32, a: u64, b: i64, c: i64) -> i64 {
        self.h.uc(op, self.class, self.unit, a, b, c, &mut [0.0; 4])
    }
    /// a statement with floats in and out
    pub(crate) fn f(&mut self, op: i32, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
        self.h.uc(op, self.class, self.unit, a, b, c, f)
    }
    /// a statement answering one float (`f[0]`)
    pub(crate) fn float(&mut self, op: i32, a: u64, b: i64) -> f32 {
        let mut f = [0.0; 4];
        self.h.uc(op, self.class, self.unit, a, b, 0, &mut f);
        f[0]
    }
    /// a statement taking one float (`f[0]`)
    pub(crate) fn with_float(&mut self, op: i32, a: u64, value: f32) -> i64 {
        let mut f = [value, 0.0, 0.0, 0.0];
        self.h.uc(op, self.class, self.unit, a, 0, 0, &mut f)
    }
    /// a statement taking (or answering) a point (`f[0..3]`)
    pub(crate) fn point(&mut self, op: i32, a: u64, b: i64, c: i64, p: &mut [f32; 3]) -> i64 {
        let mut f = [p[0], p[1], p[2], 0.0];
        let r = self.h.uc(op, self.class, self.unit, a, b, c, &mut f);
        p.copy_from_slice(&f[..3]);
        r
    }
}

/// A transformation buffer's address (a statement's object)
pub(crate) fn addr<T>(value: &mut T) -> u64 {
    value as *mut T as usize as u64
}

/// The tile pair a `CreateUnit` statement takes in `b`
fn tile_pair(x: i64, y: i64) -> i64 {
    (x as u32 as i64) | ((y as u32 as i64) << 32)
}

fn set_position(t: &mut GeTransformation, p: [f32; 3]) {
    t.m[0][3] = p[0];
    t.m[1][3] = p[1];
    t.m[2][3] = p[2];
}

fn position(t: &GeTransformation) -> [f32; 3] {
    [t.m[0][3], t.m[1][3], t.m[2][3]]
}

fn is_building_category(category: i64) -> bool {
    category == CATEGORY_BUILDING
        || category == CATEGORY_MINING_BUILDING
        || category == CATEGORY_GATE
        || category == CATEGORY_FORTRESS
}

fn reaches_planet(domains: i64, unit_type: i64) -> bool {
    domains & MOVEMENT_AIR != 0
        || unit_type == UNIT_TYPE_SPACE_DOCK
        || unit_type == UNIT_TYPE_SPACE_TURRET
        || unit_type == UNIT_TYPE_ORBITAL_SPACE_STATION
}

/// The pending units' entry: pushed, referenced
fn push_pending<H: UcHost + ?Sized>(s: &mut S<'_, H>, player: u64, unit: u64) {
    s.ab(EE_UNITS_UC_PENDING_PUSH, player, unit as i64);
    s.a(EE_UNITS_UC_REFERENCE, unit);
}

/// `CreateUnitAtTile`'s and `CreateUnitAtPoint`'s body: a unit of `class` for `player` at `point` (tile `x`, `y`)
/// with `heading`, grounded and levelled on `map` when one is given, queued in the player's pending units.
#[allow(clippy::too_many_arguments)]
pub fn create_at_point<H: UcHost + ?Sized>(
    h: &mut H,
    class: u64,
    player: u64,
    point: [f32; 3],
    x: i64,
    y: i64,
    map: u64,
    heading: f32,
) -> u64 {
    let mut s = S { h, class, unit: 0 };
    let unit;
    if s.s(EE_UNITS_UC_IS_COMPLEX) == 0 && s.s(EE_UNITS_UC_THIS_UNIT_TYPE) != UNIT_TYPE_PATH_POINT {
        unit = s.abc(EE_UNITS_UC_SIMPLE_CREATE, player, x, y) as u64;
        if map != 0 {
            s.a(EE_UNITS_UC_SIMPLE_UNIT_CREATED, unit);
        }
    } else {
        let mut t = GeTransformation::IDENTITY;
        set_position(&mut t, point);
        if map != 0 {
            let mut ground = point;
            s.point(EE_UNITS_UC_GROUND_POINT, map, 0, 0, &mut ground);
            s.point(EE_UNITS_UC_GROUND_LOCATION, 0, 0, 0, &mut ground);
            set_position(&mut t, ground);
            if !is_building_category(s.a(EE_UNITS_UC_CLASS_CATEGORY_FIELD, class)) {
                orient(&mut s, &mut t, map, heading);
            }
        }
        let goal_count = s.s(EE_UNITS_UC_NUM_GOAL_QUEUES);
        if map != 0 {
            let mut f = [t.m[2][3], 0.0, 0.0, 0.0];
            s.f(EE_UNITS_UC_LEVEL_GROUND, map, x, y, &mut f);
            t.m[2][3] = f[0];
        }
        let complex = s.abc(EE_UNITS_UC_COMPLEX_CREATE, addr(&mut t.m), player as i64, goal_count) as u64;
        unit = complex;
        if map != 0 {
            s.a(EE_UNITS_UC_UNIT_CREATED, complex);
        }
    }
    push_pending(&mut s, player, unit);
    s.s(EE_UNITS_UC_NUM_UNITS_INC);
    s.a(EE_UNITS_UC_DEREFERENCE, unit);
    unit
}

/// A unit's yaw `heading` with the terrain's pitch and roll where the class follows them; the pitch and roll
fn orient<H: UcHost + ?Sized>(s: &mut S<'_, H>, t: &mut GeTransformation, map: u64, heading: f32) -> (f32, f32) {
    let mut pitch = 0.0f32;
    let mut roll = 0.0f32;
    t.set_orientation(RotationOrder::Ypr, heading, 0.0, 0.0);
    if s.s(EE_UNITS_UC_PITCHES) != 0 {
        pitch = s.float(EE_UNITS_UC_TERRAIN_PITCH, map, addr(&mut t.m) as i64);
    }
    if s.s(EE_UNITS_UC_ROLLS) != 0 {
        roll = s.float(EE_UNITS_UC_TERRAIN_ROLL, map, addr(&mut t.m) as i64);
    }
    t.set_orientation(RotationOrder::Ypr, heading, pitch, roll);
    (pitch, roll)
}

/// The tail both creations share: the media, the pending entry, the world entry, an airport's runways and the
/// attached effect (`effect_flag` its keeps-unit argument)
#[allow(clippy::too_many_arguments)]
fn add_created<H: UcHost + ?Sized>(
    s: &mut S<'_, H>,
    player: u64,
    unit: u64,
    complex: u64,
    foundation: bool,
    creator: u64,
    effect_flag: i64,
) {
    if !foundation {
        s.a(EE_UNITS_UC_LOAD_CLASS_MEDIA_OF, player);
    }
    s.ab(EE_UNITS_UC_UNIT_LOAD_MEDIA, unit, i64::from(foundation));
    push_pending(s, player, unit);
    s.ab(EE_UNITS_UC_ADD_UNIT_ENTRY, unit, creator as i64);
    if s.s(EE_UNITS_UC_THIS_UNIT_TYPE) == UNIT_TYPE_AIRPORT {
        s.abc(EE_UNITS_UC_CREATE_RUNWAYS, complex, player as i64, i64::from(foundation));
    }
    if s.s(EE_UNITS_UC_ATTACHED_EFFECT) != 0 {
        s.ab(EE_UNITS_UC_ATTACHED_EFFECT_ON, unit, effect_flag);
    }
}

/// The buttons a mobile unit's class preloads
fn check_buttons<H: UcHost + ?Sized>(s: &mut S<'_, H>, class: u64, player: u64) {
    if s.a(EE_UNITS_UC_IS_LAND_BUILDING, class) == 0 && s.s(EE_UNITS_UC_FREE_CAMERA) == 0 {
        s.a(EE_UNITS_UC_CHECK_BUTTONS, player);
    }
}

/// `BaseCreateUnitAt(transformation, player, foundation, creator)`: a unit of the class at a transformation for a
/// player, added to the world (an airport gets its runways).
pub fn base_create_unit_at<H: UcHost + ?Sized>(
    h: &mut H,
    class: u64,
    transformation: [[f32; 4]; 4],
    player_id: i64,
    foundation: bool,
    creator: u64,
) -> u64 {
    let mut s = S { h, class, unit: 0 };
    let mut t = GeTransformation { m: transformation };
    let mut complex = 0u64;
    let player = s.b(EE_UNITS_UC_PLAYER_AT, player_id) as u64;
    check_buttons(&mut s, class, player);
    let unit;
    if s.s(EE_UNITS_UC_IS_COMPLEX) != 1 && s.s(EE_UNITS_UC_THIS_UNIT_TYPE) != UNIT_TYPE_PATH_POINT {
        let x = floor(t.m[0][3]);
        let y = floor(t.m[1][3]);
        unit = s.abc(EE_UNITS_UC_SIMPLE_CREATE, player, i64::from(x), i64::from(y)) as u64;
        s.a(EE_UNITS_UC_SIMPLE_UNIT_CREATED, unit);
    } else {
        let id = s.s(EE_UNITS_UC_NUM_GOAL_QUEUES);
        let x = floor(t.m[0][3]);
        let y = floor(t.m[1][3]);
        let map = s.s(EE_UNITS_UC_WORLD_MAP) as u64;
        let mut f = [t.m[2][3], 0.0, 0.0, 0.0];
        s.f(EE_UNITS_UC_LEVEL_GROUND, map, i64::from(x), i64::from(y), &mut f);
        t.m[2][3] = f[0];
        complex = s.abc(EE_UNITS_UC_COMPLEX_CREATE, addr(&mut t.m), player as i64, id) as u64;
        unit = complex;
        s.a(EE_UNITS_UC_UNIT_CREATED, complex);
        if reaches_planet(s.a(EE_UNITS_UC_CLASS_DOMAINS, class), s.s(EE_UNITS_UC_THIS_UNIT_TYPE)) {
            s.a(EE_UNITS_UC_SET_PLANET, complex);
        }
    }
    add_created(&mut s, player, unit, complex, foundation, creator, 1);
    s.s(EE_UNITS_UC_NUM_UNITS_INC);
    s.a(EE_UNITS_UC_DEREFERENCE, unit);
    unit
}

/// `BaseCreateUnit(x, y, player, foundation, creator)`: a unit of the class at a tile for a player (yaw pi), added
/// to the world (an airport gets its runways).
pub fn base_create_unit<H: UcHost + ?Sized>(
    h: &mut H,
    class: u64,
    x: i64,
    y: i64,
    player_id: i64,
    foundation: bool,
    creator: u64,
) -> u64 {
    let mut s = S { h, class, unit: 0 };
    let map = s.s(EE_UNITS_UC_WORLD_MAP) as u64;
    let mut point = [x as c_long as f32, y as c_long as f32, 0.0f32];
    let mut complex = 0u64;
    if x != -10000 && y != -10000 {
        point[0] += 0.5;
        point[1] += 0.5;
    }
    let player = s.b(EE_UNITS_UC_PLAYER_AT, player_id) as u64;
    check_buttons(&mut s, class, player);
    let unit;
    if s.s(EE_UNITS_UC_IS_COMPLEX) == 0 && s.s(EE_UNITS_UC_THIS_UNIT_TYPE) != UNIT_TYPE_PATH_POINT {
        unit = s.abc(EE_UNITS_UC_SIMPLE_CREATE, player, x, y) as u64;
        s.a(EE_UNITS_UC_SIMPLE_UNIT_CREATED, unit);
    } else {
        let mut t = GeTransformation::IDENTITY;
        let mut ground = point;
        let world_map = s.s(EE_UNITS_UC_WORLD_MAP) as u64;
        s.point(EE_UNITS_UC_GROUND_POINT, world_map, 0, 0, &mut ground);
        set_position(&mut t, ground);
        let mut pitch = 0.0f32;
        let mut roll = 0.0f32;
        if !is_building_category(s.a(EE_UNITS_UC_CLASS_CATEGORY_FIELD, class)) {
            (pitch, roll) = orient(&mut s, &mut t, map, YAW_PI);
        }
        let goal_count = s.s(EE_UNITS_UC_NUM_GOAL_QUEUES);
        let mut f = [t.m[2][3], 0.0, 0.0, 0.0];
        let world_map = s.s(EE_UNITS_UC_WORLD_MAP) as u64;
        s.f(EE_UNITS_UC_LEVEL_GROUND, world_map, x, y, &mut f);
        t.m[2][3] = f[0];
        complex = s.abc(EE_UNITS_UC_COMPLEX_CREATE, addr(&mut t.m), player as i64, goal_count) as u64;
        unit = complex;
        s.a(EE_UNITS_UC_UNIT_CREATED, complex);
        if reaches_planet(s.a(EE_UNITS_UC_CLASS_DOMAINS, class), s.s(EE_UNITS_UC_THIS_UNIT_TYPE)) {
            s.a(EE_UNITS_UC_SET_PLANET, complex);
        }
        let mut f = [YAW_PI, pitch, roll, 0.0];
        s.f(EE_UNITS_UC_SET_ORIENTATION, complex, 0, 0, &mut f);
    }
    add_created(&mut s, player, unit, complex, foundation, creator, 0);
    s.s(EE_UNITS_UC_NUM_UNITS_INC);
    s.a(EE_UNITS_UC_INSERT_UNLISTED, unit);
    s.a(EE_UNITS_UC_DEREFERENCE, unit);
    unit
}

/// `ReplantFarm(unit, player)`: the player's farms planted on the free plots of the 3x3 grid around the unit, each
/// with one hit point (a wall farm's pieces turned to their places).
pub fn replant_farm<H: UcCreateRust + ?Sized>(h: &mut H, class: u64, unit: u64, player: u64) {
    let farm_type = h.uc(EE_UNITS_UC_FARM_TYPE, class, unit, player, 0, 0, &mut [0.0; 4]);
    let center_x;
    let center_y;
    let size;
    let player_id;
    let farm_class;
    {
        let mut s = S { h: &mut *h, class, unit };
        farm_class = s.b(EE_UNITS_UC_CLASS_AT, farm_type) as u64;
        center_x = s.s(EE_UNITS_UC_PRODUCER_TILE_X);
        center_y = s.s(EE_UNITS_UC_PRODUCER_TILE_Y);
        size = s.a(EE_UNITS_UC_CLASS_TILE_SIZE, farm_class);
        player_id = s.a(EE_UNITS_UC_PLAYER_ID, player);
        if s.ab(EE_UNITS_UC_CAN_PRODUCE, player, farm_class as i64) == 0 {
            return;
        }
    }
    let offset: i64 = match size {
        1 => 1,
        2 => 2,
        3 => 3,
        4 => 4,
        5 => 10,
        _ => 0,
    };
    let left = center_x.wrapping_sub(offset) as c_long as i64;
    let mut x = left;
    let mut y = (offset + center_y) as c_long as i64;
    let mut row = 0i64;
    while row < 9 {
        for column in 0..3i64 {
            if (x != center_x || y != center_y)
                && h.block_takes(player, farm_type as i32, x as i32, y as i32, size as i32, size as i32)
            {
                let mut s = S { h: &mut *h, class, unit };
                let wall = s.abc(EE_UNITS_UC_CREATE_UNIT_FOR, farm_class, tile_pair(x, y), player_id) as u64;
                s.ab(EE_UNITS_UC_SET_HIT_POINTS_OF, wall, 1);
                s.abc(EE_UNITS_UC_MOVE_TO, wall, x, y);
                if farm_type == UNIT_TYPE_ROBOTIC_FARM {
                    let (piece, corner) = crate::units::ucb_release::wall_piece_part(row + column);
                    if corner {
                        let complex = s.a(EE_UNITS_UC_COMPLEX_OF, wall) as u64;
                        s.ab(EE_UNITS_UC_SET_ANIMATION_OF, complex, 0xb);
                    }
                    let mut t = GeTransformation::IDENTITY;
                    let complex = s.a(EE_UNITS_UC_COMPLEX_OF, wall) as u64;
                    s.ab(EE_UNITS_UC_LOCATION_MATRIX, complex, addr(&mut t.m) as i64);
                    t.set_orientation(RotationOrder::Ypr, WALL_ROTATIONS[piece as usize], 0.0, 0.0);
                    let complex = s.a(EE_UNITS_UC_COMPLEX_OF, wall) as u64;
                    s.ab(EE_UNITS_UC_SET_PHYSICAL_LOCATION, complex, addr(&mut t.m) as i64);
                }
            }
            x += size;
        }
        y -= size;
        x = left;
        row += 3;
    }
}

/// `CreateRemainsUnit(unit)`: the class's remains unit for the dead unit's player at a free spot near where it died.
pub fn create_remains_unit<H: UcRust + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut t = GeTransformation::IDENTITY;
    let remains;
    let tile;
    let size;
    let human;
    {
        let mut s = S { h: &mut *h, class, unit };
        s.ab(EE_UNITS_UC_LOCATION_MATRIX, unit, addr(&mut t.m) as i64);
        remains = s.s(EE_UNITS_UC_REMAINS_CLASS) as u64;
        if remains == 0 {
            return;
        }
        tile = (s.s(EE_UNITS_UC_PRODUCER_TILE_X), s.s(EE_UNITS_UC_PRODUCER_TILE_Y));
        size = s.s(EE_UNITS_UC_TILE_SIZE);
        human = s.s(EE_UNITS_UC_IS_HUMAN) != 0;
    }
    let p = FindPlacement {
        point: position(&t),
        unit: 0,
        class: remains,
        x: tile.0 as u32,
        y: tile.1 as u32,
        size: size as i32,
        rings: 1,
        flags: [!human, false, false, false],
        target: unit,
    };
    if !h.find_placement(&p, &mut t.m) {
        return;
    }
    if t.m[2][3] <= 0.0 || t.m[2][3].is_nan() {
        t.m[2][3] = 0.3;
    }
    let mut s = S { h, class, unit };
    let id = s.s(EE_UNITS_UC_UNIT_PLAYER_ID);
    s.abc(EE_UNITS_UC_CREATE_AT_FOR, remains, addr(&mut t.m) as i64, id);
}

fn flatten(t: &GeTransformation) -> [f32; 16] {
    let mut out = [0.0f32; 16];
    for (r, row) in t.m.iter().enumerate() {
        out[r * 4..r * 4 + 4].copy_from_slice(row);
    }
    out
}

fn unflatten(rows: &[f32; 16]) -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    for (r, row) in t.m.iter_mut().enumerate() {
        row.copy_from_slice(&rows[r * 4..r * 4 + 4]);
    }
    t
}

/// The launched projectile's set up (both launches' tail): the creation effect (`world_effect`: through the world's
/// effect manager), its launcher and owner, its attack goal, speed and height, then its class's launch
#[allow(clippy::too_many_arguments)]
fn arm_shot<H: UcHost + ?Sized>(
    s: &mut S<'_, H>,
    shot: u64,
    shot_class: u64,
    target: u64,
    location: [f32; 3],
    world_effect: bool,
    velocity_op: i32,
) {
    if s.s(EE_UNITS_UC_CREATION_EFFECT) != 0 {
        let op = if world_effect { EE_UNITS_UC_WORLD_CREATION_EFFECT_ON } else { EE_UNITS_UC_CREATION_EFFECT_ON };
        s.a(op, shot);
    }
    let complex = s.a(EE_UNITS_UC_COMPLEX_OF, shot) as u64;
    s.a(EE_UNITS_UC_SET_LAUNCHER, complex);
    let complex = s.a(EE_UNITS_UC_COMPLEX_OF, shot) as u64;
    s.a(EE_UNITS_UC_SET_OWNER, complex);
    let goal = if target != 0 {
        let complex = s.a(EE_UNITS_UC_COMPLEX_OF, shot) as u64;
        s.ab(EE_UNITS_UC_NEW_ATTACK_UNIT, complex, target as i64)
    } else {
        let mut p = location;
        s.point(EE_UNITS_UC_NEW_ATTACK_GROUND, 0, 0, 0, &mut p)
    };
    let complex = s.a(EE_UNITS_UC_COMPLEX_OF, shot) as u64;
    s.ab(EE_UNITS_UC_SET_GOAL, complex, goal);
    let velocity = s.float(velocity_op, 0, 0);
    let complex = s.a(EE_UNITS_UC_COMPLEX_OF, shot) as u64;
    s.with_float(EE_UNITS_UC_SET_VELOCITY_OF, complex, velocity);
    let mut here = [0.0f32; 3];
    let unit = s.unit;
    s.point(EE_UNITS_UC_LOCATION, unit, 0, 0, &mut here);
    let complex = s.a(EE_UNITS_UC_COMPLEX_OF, shot) as u64;
    s.with_float(EE_UNITS_UC_SET_HEIGHT_OF, complex, here[2]);
    let complex = s.a(EE_UNITS_UC_COMPLEX_OF, shot) as u64;
    let mut p = location;
    s.point(EE_UNITS_UC_LAUNCH_PROJECTILE, shot_class, complex as i64, 0, &mut p);
}

/// The launch over the chain (reset, filled at tag point `index` when there is one, read), captured as `tag`
fn chained_launch<H: UcCreateRust + ?Sized>(
    h: &mut H,
    class: u64,
    unit: u64,
    input: &mut EeCombatLaunchIn,
    index: i32,
    tag: u64,
) -> EeCombatLaunchOut {
    if index >= 0 {
        h.uc(EE_UNITS_UC_TAG_CHAIN, class, unit, 0, addr(input) as i64, i64::from(index), &mut [0.0; 4]);
    }
    let chain = h.chain(class, unit, input.chain);
    let mut out = launch(input, &chain);
    h.uc(EE_UNITS_UC_CHAIN_CAPTURE, class, unit, tag, addr(input) as i64, addr(&mut out) as i64, &mut [0.0; 4]);
    out
}

/// `LaunchShot(unit, target, location, cargo)`: a projectile (or the carried cargo) launched from the unit's weapon
/// tag point toward the target or ground point (the launch, its tile and the special attack's choices ee_combat's).
pub fn launch_shot<H: UcCreateRust + ?Sized>(
    h: &mut H,
    class: u64,
    unit: u64,
    target: u64,
    location: [f32; 3],
    cargo: u64,
) {
    let mut input = EeCombatLaunchIn::default();
    let mut shot_class;
    let mut created = 0u64;
    {
        let mut s = S { h: &mut *h, class, unit };
        let list = s.s(EE_UNITS_UC_TAG_LIST) as u64;
        shot_class = s.s(EE_UNITS_UC_PROJECTILE_CLASS) as u64;
        let unit_type = s.s(EE_UNITS_UC_PRODUCER_TYPE);
        input.kind = LAUNCH_KIND_SHOT;
        input.special = s.s(EE_UNITS_UC_SPECIAL_ATTACK) as i32;
        input.has_special = s.s(EE_UNITS_UC_HAS_SPECIAL_ATTACK) as i32;
        input.capital = i32::from(unit_type == UNIT_TYPE_SP15_CAPITAL_SHIP || unit_type == UNIT_TYPE_CAPITAL_SHIP_YAMATO);
        input.legionnaire = i32::from(unit_type == UNIT_TYPE_X04_ROMAN_LEGIONNAIRE);
        input.point_count = if list != 0 { s.a(EE_UNITS_UC_TAG_COUNT, list) as i32 } else { 0 };
        input.weapon_index = s.s(EE_UNITS_UC_WEAPON_TAG) as i32;
        input.building = i32::from(s.a(EE_UNITS_UC_CLASS_CATEGORY_FIELD, class) == CATEGORY_BUILDING);
        let projectile = s.s(EE_UNITS_UC_PROJECTILE_CLASS) as u64;
        input.turn =
            i32::from(projectile == 0 || s.a(EE_UNITS_UC_CLASS_CATEGORY_FIELD, projectile) != CATEGORY_BOMB);
        let mut here = GeTransformation::IDENTITY;
        s.ab(EE_UNITS_UC_LOCATION_MATRIX, unit, addr(&mut here.m) as i64);
        input.location = flatten(&here);
        input.scale = s.float(EE_UNITS_UC_STATS_SCALE, 0, 0);
        input.target = location;
        s.s(EE_UNITS_UC_CHAIN_RESET);
    }
    let index = launch_index(&input);
    let out = chained_launch(h, class, unit, &mut input, index, TAG_CBLS);
    let mut t = unflatten(&out.launch);
    let mut s = S { h, class, unit };
    let mut x = i64::from(out.tile[0]);
    let mut y = i64::from(out.tile[1]);
    if s.abc(EE_UNITS_UC_GET_TILE, 0, x, y) == 0 {
        let mut p = position(&t);
        s.point(EE_UNITS_UC_CLOSEST_TILE, 0, x, y, &mut p);
        x = s.s(EE_UNITS_UC_TILE_OUT_X);
        y = s.s(EE_UNITS_UC_TILE_OUT_Y);
    }
    if s.s(EE_UNITS_UC_SPECIAL_ATTACK) != 0 {
        if out.flaming != 0 {
            shot_class = s.s(EE_UNITS_UC_FLAMING_CLASS) as u64;
            let id = s.s(EE_UNITS_UC_UNIT_PLAYER_ID);
            created = s.abc(EE_UNITS_UC_CREATE_UNIT_BY, shot_class, tile_pair(x, y), id) as u64;
        }
        if s.a(EE_UNITS_UC_CAN_ATTACK_CLASS, target) == 0 {
            s.b(EE_UNITS_UC_START_ACTION_SELF, ACTION_IDLE);
        }
        s.s(EE_UNITS_UC_CLEAR_SPECIAL);
    }
    if cargo != 0 {
        created = cargo;
        shot_class = s.a(EE_UNITS_UC_CLASS_OF, cargo) as u64;
        s.abc(EE_UNITS_UC_MOVE_TO, cargo, x, y);
    }
    if created == 0 {
        let projectile = s.s(EE_UNITS_UC_PROJECTILE_CLASS) as u64;
        let id = s.s(EE_UNITS_UC_UNIT_PLAYER_ID);
        created = s.abc(EE_UNITS_UC_CREATE_UNIT_BY, projectile, tile_pair(x, y), id) as u64;
        shot_class = s.s(EE_UNITS_UC_PROJECTILE_CLASS) as u64;
    }
    if s.a(EE_UNITS_UC_PARATROOPER_BOMBER, unit) != 0 {
        let complex = s.a(EE_UNITS_UC_COMPLEX_OF, created) as u64;
        s.a(EE_UNITS_UC_MOVE_PASSENGERS, complex);
    }
    let complex = s.a(EE_UNITS_UC_COMPLEX_OF, created) as u64;
    s.ab(EE_UNITS_UC_SET_PHYSICAL_LOCATION, complex, addr(&mut t.m) as i64);
    arm_shot(&mut s, created, shot_class, target, location, true, EE_UNITS_UC_GET_VELOCITY);
}

/// `LaunchFromTurret(unit, target, location)`: the class's projectile fired from the unit's turret weapon point
/// (ee_combat's launch_from_turret); without a turret part, a plain `LaunchShot`.
pub fn launch_from_turret<H: UcCreateRust + ?Sized>(h: &mut H, class: u64, unit: u64, target: u64, location: [f32; 3]) {
    let mut input = EeCombatLaunchIn::default();
    let index;
    {
        let mut s = S { h: &mut *h, class, unit };
        index = s.s(EE_UNITS_UC_WEAPON_TAG);
        if s.s(EE_UNITS_UC_FIND_TURRET_PART) != 0 {
            drop(s);
            launch_shot(h, class, unit, target, location, 0);
            return;
        }
        let part = s.s(EE_UNITS_UC_TURRET_PART);
        input.kind = LAUNCH_KIND_TURRET;
        let mut here = GeTransformation::IDENTITY;
        s.ab(EE_UNITS_UC_LOCATION_MATRIX, unit, addr(&mut here.m) as i64);
        input.location = flatten(&here);
        input.scale = s.float(EE_UNITS_UC_STATS_SCALE_FIELD, 0, 0);
        input.yaw = s.float(EE_UNITS_UC_TURRET_ANGLE, 0, part);
        s.s(EE_UNITS_UC_CHAIN_RESET);
    }
    let out = chained_launch(h, class, unit, &mut input, index as i32, TAG_CBLT);
    let mut t = unflatten(&out.launch);
    let mut s = S { h, class, unit };
    let projectile = s.s(EE_UNITS_UC_PROJECTILE_CLASS) as u64;
    let id = s.s(EE_UNITS_UC_UNIT_PLAYER_ID);
    let shot = s.abc(EE_UNITS_UC_CREATE_AT_BY, projectile, addr(&mut t.m) as i64, id) as u64;
    let projectile = s.s(EE_UNITS_UC_PROJECTILE_CLASS) as u64;
    arm_shot(&mut s, shot, projectile, target, location, false, EE_UNITS_UC_VELOCITY_FIELD);
}

/// `UnitDied(unit)`: the death class spawned, the death effect and sound (not for ICBMs), the dying animation (not
/// for walls); the unit uncommandable, its path dropped.
pub fn unit_died<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    if s.s(EE_UNITS_UC_DEATH_CLASS) != 0 && s.s(EE_UNITS_UC_UNIT_TILE) != 0 {
        s.s(EE_UNITS_UC_CREATE_DEATH_CLASS_UNIT);
    }
    if s.s(EE_UNITS_UC_PRODUCER_TYPE) != UNIT_TYPE_CP_ICBM {
        if s.s(EE_UNITS_UC_DEATH_EFFECT) != 0 {
            s.s(EE_UNITS_UC_DEATH_EFFECT_ON);
        }
        let sound = s.s(EE_UNITS_UC_DEATH_SOUND) as u64;
        if sound != 0 {
            let mut f = [0.0f32; 4];
            s.f(EE_UNITS_UC_SOUND_PARAMETERS, sound, 0, 0, &mut f);
            s.f(EE_UNITS_UC_PLAY_SOUND, sound, 0, 0, &mut f);
        }
    }
    s.b(EE_UNITS_UC_SET_PHYSICS_ACTION, 2);
    if s.s(EE_UNITS_UC_UNIT_IS_WALL) == 0 {
        s.b(EE_UNITS_UC_SET_ANIMATION, 4);
    }
    s.s(EE_UNITS_UC_NOT_COMMANDABLE);
    s.s(EE_UNITS_UC_DROP_PATH);
}

/// `FindSafeLocation(unit)`: a computer player's unit sent to an escape point, when one is found.
pub fn find_safe_location<H: UcCreateRust + ?Sized>(h: &mut H, class: u64, unit: u64) {
    if h.uc(EE_UNITS_UC_COMPUTER_PLAYER, class, unit, 0, 0, 0, &mut [0.0; 4]) == 0 {
        return;
    }
    let Some(point) = h.escape_point(unit) else {
        return;
    };
    let mut s = S { h, class, unit };
    s.s(EE_UNITS_UC_DROP_PATH);
    let mut f = [point[0], point[1], 0.0, 0.0];
    let goal = s.f(EE_UNITS_UC_NEW_LOCATION_2D, 0, 0, 0, &mut f);
    s.a(EE_UNITS_UC_SET_GOAL_SELF, goal as u64);
    s.b(EE_UNITS_UC_START_ACTION_SELF, ACTION_PREPARE_TO_MOVE);
    s.s(EE_UNITS_UC_ACTION_CHANGED_SELF);
}

/// `MoveToTileCentre(unit)`: a goalless unit sent to the centre of its tile; with a goal, faced to the centre and
/// placed there (short of it by the stray distance).
pub fn move_to_tile_centre<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    if s.s(EE_UNITS_UC_GOAL_STATE_SELF) == 0 {
        s.b(EE_UNITS_UC_SET_PHYSICS_ACTION, 0);
        s.s(EE_UNITS_UC_DROP_PATH);
        let x = s.s(EE_UNITS_UC_PRODUCER_TILE_X) as c_long as f32 + 0.5;
        let y = s.s(EE_UNITS_UC_PRODUCER_TILE_Y) as c_long as f32 + 0.5;
        let z = if s.s(EE_UNITS_UC_CAN_FLY) == 1 {
            s.float(EE_UNITS_UC_HEIGHT_FIELD, 0, 0)
        } else {
            s.float(EE_UNITS_UC_TILE_HEIGHT, 0, 0)
        };
        let mut f = [x, y, z, 0.0];
        let goal = s.f(EE_UNITS_UC_NEW_LOCATION, 0, 0, 0, &mut f);
        s.a(EE_UNITS_UC_SET_GOAL_SELF, goal as u64);
    } else {
        let tx = s.s(EE_UNITS_UC_PRODUCER_TILE_X) as c_long as f32 + 0.5;
        let ty = s.s(EE_UNITS_UC_PRODUCER_TILE_Y) as c_long as f32 + 0.5;
        let mut here = [0.0f32; 3];
        s.point(EE_UNITS_UC_LOCATION, unit, 0, 0, &mut here);
        let target = [tx, ty, here[2]];
        let mut current = [0.0f32; 3];
        s.point(EE_UNITS_UC_LOCATION, unit, 0, 0, &mut current);
        let dx = target[0] - current[0];
        let heading = um_arctan2(-dx, target[1] - current[1]);
        let mut t = GeTransformation::IDENTITY;
        set_position(&mut t, target);
        t.set_orientation(RotationOrder::Ypr, heading, 0.0, 0.0);
        let mut f = [heading, 0.0, 0.0, 0.0];
        s.f(EE_UNITS_UC_SET_ORIENTATION, unit, 0, 0, &mut f);
        let depth = 1.0f32 - s.float(EE_UNITS_UC_WORLD_FLOAT, 0, WORLD_TILE_STRAY_DISTANCE);
        let p = t.apply_point(Ge3DPoint { x: 0.0, y: depth, z: 0.0 });
        set_position(&mut t, [p.x, p.y, p.z]);
        s.a(EE_UNITS_UC_UPDATE_UNIT_TILE, addr(&mut t.m));
    }
}

/// `BaseUnitCreated(unit)`: full hit points, ammunition, behaviour, heal timer, standing still and idle.
pub fn base_unit_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    s.s(EE_UNITS_UC_SET_HEAL_SEARCH_TIMER);
    s.s(EE_UNITS_UC_SET_FULL_HIT_POINTS);
    s.s(EE_UNITS_UC_SET_FULL_LAST_HIT_POINTS);
    s.a(EE_UNITS_UC_STOP, unit);
    s.s(EE_UNITS_UC_SET_AMMUNITION);
    s.s(EE_UNITS_UC_SET_BEHAVIOR);
    s.ab(EE_UNITS_UC_START_ACTION, unit, ACTION_IDLE);
    s.s(EE_UNITS_UC_CLASS_ACTION_CHANGED);
}

/// `class a`'s `mCategory` (the creations read the field, as `GetCategory()` does)
pub const EE_UNITS_UC_CLASS_CATEGORY_FIELD: i32 = 410;

/// Runs body `what` of `class` on `unit` (`a`, `b`, `c` and the floats its arguments, as `ee_units_uc_call`
/// documents them); the created unit for the creations, else 0.
#[allow(clippy::too_many_arguments)]
pub fn run<H: UcCreateRust + ?Sized>(
    h: &mut H,
    what: i32,
    class: u64,
    unit: u64,
    a: u64,
    b: i64,
    c: i64,
    f: [f32; 4],
    m: Option<[[f32; 4]; 4]>,
) -> Option<i64> {
    let p = [f[0], f[1], f[2]];
    Some(match what {
        EE_UNITS_UC_DO_BASE_CREATE_UNIT_AT => {
            base_create_unit_at(h, class, m.unwrap_or(GeTransformation::IDENTITY.m), b, c & 1 != 0, a) as i64
        }
        EE_UNITS_UC_DO_BASE_CREATE_UNIT => {
            base_create_unit(h, class, (b as i32) as i64, (b >> 32) as i32 as i64, c >> 1, c & 1 != 0, a) as i64
        }
        EE_UNITS_UC_DO_REPLANT_FARM => {
            replant_farm(h, class, unit, a);
            0
        }
        EE_UNITS_UC_DO_CREATE_REMAINS_UNIT => {
            create_remains_unit(h, class, unit);
            0
        }
        EE_UNITS_UC_DO_LAUNCH_SHOT => {
            launch_shot(h, class, unit, a, p, b as u64);
            0
        }
        EE_UNITS_UC_DO_LAUNCH_FROM_TURRET => {
            launch_from_turret(h, class, unit, a, p);
            0
        }
        EE_UNITS_UC_DO_UNIT_DIED => {
            unit_died(h, class, unit);
            0
        }
        EE_UNITS_UC_DO_FIND_SAFE_LOCATION => {
            find_safe_location(h, class, unit);
            0
        }
        EE_UNITS_UC_DO_MOVE_TO_TILE_CENTRE => {
            move_to_tile_centre(h, class, unit);
            0
        }
        EE_UNITS_UC_DO_BASE_UNIT_CREATED => {
            base_unit_created(h, class, unit);
            0
        }
        _ => return None,
    })
}

/// Every body of `ee_units_uc_call` (this module's, uc_anim.rs's, uc_media.rs's).
#[allow(clippy::too_many_arguments)]
pub fn dispatch<H: UcCreateRust + crate::units::uc_anim::UcRegions + ?Sized>(
    h: &mut H,
    what: i32,
    class: u64,
    unit: u64,
    a: u64,
    b: i64,
    c: i64,
    f: [f32; 4],
    m: Option<[[f32; 4]; 4]>,
) -> i64 {
    run(h, what, class, unit, a, b, c, f, m)
        .or_else(|| crate::units::uc_anim::run(h, what, class, unit, a, b, c, f))
        .or_else(|| crate::units::uc_media::run(h, what, class, unit, a, b, c))
        .unwrap_or(0)
}

#[cfg(not(test))]
pub(crate) mod game {
    use super::*;
    use crate::units::uc_body::game::Game;
    use std::os::raw::c_void;

    impl UcCreateRust for Game {
        fn block_takes(&mut self, player: u64, unit_type: i32, x: i32, y: i32, w: i32, h: i32) -> bool {
            crate::units::uu_ffi::ee_units_uu_block_takes(player, unit_type, x, y, w, h, 0) != 0
        }
        fn escape_point(&mut self, unit: u64) -> Option<[f32; 2]> {
            let mut point = [0.0f32; 2];
            // SAFETY: the point is live for the call; the unit is the body's, live
            let found = unsafe { crate::units::uu_ffi::ee_units_uu_escape_point(unit, &mut point) } != 0;
            found.then_some(point)
        }
        fn chain(&mut self, class: u64, unit: u64, count: i32) -> Vec<[f32; 16]> {
            if count <= 0 {
                return Vec::new();
            }
            let data = self.uc(EE_UNITS_UC_CHAIN_DATA, class, unit, 0, 0, 0, &mut [0.0; 4]) as usize as *const [f32; 16];
            // SAFETY: the host's chain holds `count` matrices (EECombatTagPointChain counted them in)
            unsafe { std::slice::from_raw_parts(data, count as usize) }.to_vec()
        }
    }

    /// `EEUnitClass`'s body `what` (`EE_UNITS_UC_DO_*` from 20) of `class` on `unit`: `a` an object, `b`, `c`
    /// values, `f` a point (three floats, or NULL), `m` a transformation (or NULL); the created unit or 0.
    ///
    /// # Safety
    /// `class` is a live `EEUnitClass`, `unit` a live `EEComplexUnit` (or NULL), `f` and `m` NULL or live, on the
    /// simulation thread.
    #[no_mangle]
    #[allow(clippy::too_many_arguments)]
    pub unsafe extern "C" fn ee_units_uc_call(
        what: i32,
        class: *mut c_void,
        unit: *mut c_void,
        a: u64,
        b: i64,
        c: i64,
        f: *const [f32; 3],
        m: *const [[f32; 4]; 4],
    ) -> i64 {
        // SAFETY: the caller's point and transformation
        let floats = if f.is_null() { [0.0; 4] } else { unsafe { [(*f)[0], (*f)[1], (*f)[2], 0.0] } };
        let matrix = if m.is_null() { None } else { Some(unsafe { *m }) };
        dispatch(&mut Game, what, class as usize as u64, unit as usize as u64, a, b, c, floats, matrix)
    }

    /// `CreateUnitAtTile`/`CreateUnitAtPoint` of `class`: a unit for `player` at (`x`, `y`) (its tile `tile_x`,
    /// `tile_y`) with `heading`, grounded on `map` when it is not NULL; the unit.
    ///
    /// # Safety
    /// `class` is a live `EEUnitClass`, `player` a live `EEPlayer`, `map` NULL or the live map, on the simulation
    /// thread.
    #[no_mangle]
    #[allow(clippy::too_many_arguments)]
    pub unsafe extern "C" fn ee_units_uc_create_at(
        class: *mut c_void,
        player: *mut c_void,
        x: f32,
        y: f32,
        tile_x: u32,
        tile_y: u32,
        map: *mut c_void,
        heading: f32,
    ) -> *mut c_void {
        let made = create_at_point(
            &mut Game,
            class as usize as u64,
            player as usize as u64,
            [x, y, 0.0],
            tile_x as c_long as i64,
            tile_y as c_long as i64,
            map as usize as u64,
            heading,
        );
        made as usize as *mut c_void
    }
}

#[cfg(test)]
#[path = "uc_create_tests.rs"]
mod tests;
