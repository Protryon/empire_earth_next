//! The aircraft's and spaceships' class bodies over their C++ statements (the units' lane, round 8; were
//! EEUnitClassAir.cpp): `FlyToGoal`, `GetGroundHeight`, `EELandableActionOperation`, `LandableUnitCreated`,
//! `IsBombOnTarget` (its UCBD note stays C++), `EECreateCarrierAircraft`, `FlyOffMap`, `AirAnimalReturnToAirport`,
//! `EEAirAnimalPathToGoal`, `EEAirAnimalDeathRoll`, `AirAnimalUnitCreated`, `EEPlanetSpaceshipHoldingPattern`,
//! `EEHelicopterPathToGoal`, `EEComplexUnit::SetOpacityChain`. Each C++ statement is one `EE_UNITS_UCA_*` case of
//! `ee_units_uca_host` (EEUnitClassAir.cpp; the class host's statements, `EE_UNITS_UC_*`, reach EEUnitClass.cpp's
//! through it), asked in the original's order; the choices, loops and arithmetic are here.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::actions::*;
use crate::units::cls_air::runway_direction;
use crate::units::uc_body::*;
use crate::units::uc_create::*;
use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{Ge3DPoint, GeTransformation, GeVector};
use ee_base::math::um::{floor, sqrt, um_arccos, um_arctan2, um_cos};
use std::os::raw::c_long;

// ---- the statements (2000..2199)
/// `f[0..3]` = the unit's location's X, Y, Z
pub const EE_UNITS_UCA_LOCATION: i32 = 2000;
/// `unit->mGoalState`
pub const EE_UNITS_UCA_GOAL_STATE: i32 = 2001;
/// `EEGoalHasLocation(goal a)`
pub const EE_UNITS_UCA_HAS_LOCATION: i32 = 2002;
/// `EERustGoalCast(goal a, b)`
pub const EE_UNITS_UCA_GOAL_CAST: i32 = 2003;
/// `f[0..3] = EELocationGoalPoint(goal a)`
pub const EE_UNITS_UCA_LOCATION_POINT: i32 = 2004;
/// goal `a`'s `GetPatrolTarget()->GetTileX()`
pub const EE_UNITS_UCA_PATROL_TILE_X: i32 = 2005;
/// goal `a`'s `GetPatrolTarget()->GetTileY()`
pub const EE_UNITS_UCA_PATROL_TILE_Y: i32 = 2006;
/// goal `a`'s `GetExploreTileCenter(f[0..3])`
pub const EE_UNITS_UCA_EXPLORE_CENTER: i32 = 2007;
/// `unit->StartAction(b)`
pub const EE_UNITS_UCA_START_ACTION: i32 = 2008;
/// goal `a`'s `Step(unit)`
pub const EE_UNITS_UCA_GOAL_STEP: i32 = 2009;
/// `unit->mUnitClass->mAirTrafficType`
pub const EE_UNITS_UCA_AIR_TRAFFIC_TYPE: i32 = 2010;
/// `f[0..3] = ` goal `a`'s `GetGroundTarget()`
pub const EE_UNITS_UCA_GROUND_TARGET: i32 = 2011;
/// goal `a`'s `GetGoalLocation(f[0..3])`
pub const EE_UNITS_UCA_GOAL_LOCATION: i32 = 2012;
/// goal `a`'s `GetAttackTarget()`
pub const EE_UNITS_UCA_ATTACK_TARGET: i32 = 2013;
/// unit `a`'s `mComplexUnit`
pub const EE_UNITS_UCA_COMPLEX_OF: i32 = 2014;
/// `unit->mUnitClass->mProjectileClass != NULL`
pub const EE_UNITS_UCA_HAS_PROJECTILE: i32 = 2015;
/// `EECombatFindDropLocation(unit, NULL, f[0..3])`
pub const EE_UNITS_UCA_FIND_DROP: i32 = 2016;
/// `f[0..3]` = complex unit `a`'s location's X, Y, Z
pub const EE_UNITS_UCA_LOCATION_OF: i32 = 2017;
/// unit `a`'s `mUnitClass->GetTileSize()`
pub const EE_UNITS_UCA_TILE_SIZE_OF: i32 = 2018;
/// unit `a`'s `GetTileX()`
pub const EE_UNITS_UCA_TILE_X_OF: i32 = 2019;
/// unit `a`'s `GetTileY()`
pub const EE_UNITS_UCA_TILE_Y_OF: i32 = 2020;
/// `f[0] = class->GetCruiseAltitude()`
pub const EE_UNITS_UCA_CRUISE: i32 = 2021;
/// `EEFindLandingPoint(unit, f[0..3])`
pub const EE_UNITS_UCA_LANDING_POINT: i32 = 2022;
/// `::gWorld.mMap->GetTile(b, c)`
pub const EE_UNITS_UCA_GET_TILE: i32 = 2023;
/// `::gWorld.mMap->GetClosestTile(f[0..3], the scratch x, y)`
pub const EE_UNITS_UCA_CLOSEST_TILE: i32 = 2024;
/// `f[0] = ` tile `a`'s `GetHeight()`
pub const EE_UNITS_UCA_TILE_HEIGHT_OF: i32 = 2025;
/// `class->ApproachCarrier(unit, b != 0)`
pub const EE_UNITS_UCA_APPROACH_CARRIER: i32 = 2026;
/// `unit->mPlayer->mAirTraffic[unit->mUnitClass->mAirTrafficType - 1]`
pub const EE_UNITS_UCA_ATC: i32 = 2027;
/// `ee_units_atc_remove_plane(atc a, unit->mHomeBase, unit)`
pub const EE_UNITS_UCA_ATC_REMOVE_PLANE: i32 = 2028;
/// `unit->mHomeBase->LoadCargo(unit, 1, 0)`
pub const EE_UNITS_UCA_HOME_LOAD: i32 = 2029;
/// `unit->SetHomeBase(complex unit a)`
pub const EE_UNITS_UCA_SET_HOME_BASE: i32 = 2030;
/// `unit->SetIsCommandable(b != 0)`
pub const EE_UNITS_UCA_SET_COMMANDABLE: i32 = 2031;
/// `unit->SetHeight(f[0])`
pub const EE_UNITS_UCA_SET_HEIGHT: i32 = 2032;
/// `class->TakeOffFromCarrier(unit)`
pub const EE_UNITS_UCA_TAKE_OFF_CARRIER: i32 = 2033;
/// `ee_units_atc_find_nearest_airport(atc a, unit)`
pub const EE_UNITS_UCA_FIND_NEAREST_AIRPORT: i32 = 2034;
/// `unit->mHomeBase`
pub const EE_UNITS_UCA_HOME_BASE: i32 = 2035;
/// `unit->mHomeBase->mFacing`
pub const EE_UNITS_UCA_HOME_FACING: i32 = 2036;
/// `class->ApproachRunway(unit, b, c)`
pub const EE_UNITS_UCA_APPROACH_RUNWAY: i32 = 2037;
/// `unit->mBehavior->mCanTargetEnemies`
pub const EE_UNITS_UCA_TARGETS_ENEMIES: i32 = 2038;
/// `EEFindTargetNear(unit, b, c, f[0])`
pub const EE_UNITS_UCA_FIND_TARGET_NEAR: i32 = 2039;
/// goal `a`'s `SetAttackTarget(unit b)`
pub const EE_UNITS_UCA_SET_ATTACK_TARGET: i32 = 2040;
/// `unit->ClearGoals(0)`
pub const EE_UNITS_UCA_CLEAR_GOALS: i32 = 2041;
/// goal `a`'s `GetB1C()`
pub const EE_UNITS_UCA_GOAL_B1C: i32 = 2042;
/// `EEUGRust::NewLocation(f[0], f[1], f[2], b != 0, true, false, 0.0f, false, false)`
pub const EE_UNITS_UCA_NEW_LOCATION: i32 = 2043;
/// `unit->SetGoal(goal a, 0, 0)`
pub const EE_UNITS_UCA_SET_GOAL: i32 = 2044;
/// goal `a`'s `GetStartAction()`
pub const EE_UNITS_UCA_GOAL_START_ACTION: i32 = 2045;
/// `class->ActionChanged(unit)`
pub const EE_UNITS_UCA_ACTION_CHANGED: i32 = 2046;
/// `ee_units_atc_slot_count(atc a)`
pub const EE_UNITS_UCA_ATC_SLOT_COUNT: i32 = 2047;
/// `unit->Kill(true, false, false)`
pub const EE_UNITS_UCA_KILL: i32 = 2048;
/// `unit->mContainer`
pub const EE_UNITS_UCA_CONTAINER: i32 = 2049;
/// `ee_units_atc_slot_airport(atc a, b)`
pub const EE_UNITS_UCA_ATC_SLOT_AIRPORT: i32 = 2050;
/// `class->LandableReturnToAirport(unit)`
pub const EE_UNITS_UCA_RETURN_TO_AIRPORT: i32 = 2051;
/// `class->TakeOffFromRunway(unit, b, c)`
pub const EE_UNITS_UCA_TAKE_OFF_RUNWAY: i32 = 2052;
/// `unit->SetFlightTime(unit->GetStats()->mFlightTime)`
pub const EE_UNITS_UCA_SET_STATS_FLIGHT_TIME: i32 = 2053;
/// `class->mIsParatrooperBomber`
pub const EE_UNITS_UCA_PARATROOPER_BOMBER: i32 = 2054;
/// `unit->SetAmmunition(class->mAmmunition)`
pub const EE_UNITS_UCA_SET_AMMUNITION: i32 = 2055;
/// `unit->SetAnimation(b, -1, 0)`
pub const EE_UNITS_UCA_SET_ANIMATION: i32 = 2056;
/// `unit->GetNextWaypointPoint(f[0..3])`
pub const EE_UNITS_UCA_NEXT_WAYPOINT: i32 = 2057;
/// `f[0] = class->GetHeightAt(unit, f[0..3])`
pub const EE_UNITS_UCA_HEIGHT_AT: i32 = 2058;
/// `ee_units_atc_queue_request(atc a, unit->mHomeBase, unit, 1)`
pub const EE_UNITS_UCA_ATC_QUEUE_LANDING: i32 = 2059;
/// `unit->SetGoal(EEUGRust::NewAttackGround(f[0..3], goal a->GetB1C()), 0, 0)`
pub const EE_UNITS_UCA_ATTACK_GROUND_GOAL: i32 = 2060;
/// `f[0] = unit->mHeight`
pub const EE_UNITS_UCA_HEIGHT: i32 = 2061;
/// `unit->mHeight = f[0]`
pub const EE_UNITS_UCA_SET_HEIGHT_FIELD: i32 = 2062;
/// `unit->SetInitialContactLocation(f[0..3])`
pub const EE_UNITS_UCA_SET_CONTACT: i32 = 2063;
/// `unit->mInitialContactLocation != f[0..3]`
pub const EE_UNITS_UCA_CONTACT_DIFFERS: i32 = 2064;
/// `unit->mInitialContactLocation = f[0..3]`
pub const EE_UNITS_UCA_SET_CONTACT_FIELD: i32 = 2065;
/// `f[0] = unit->mInitialContactHeading`
pub const EE_UNITS_UCA_CONTACT_HEADING: i32 = 2066;
/// `unit->mInitialContactHeading = f[0]`
pub const EE_UNITS_UCA_SET_CONTACT_HEADING: i32 = 2067;
/// goal `a`'s `mKind`
pub const EE_UNITS_UCA_GOAL_KIND: i32 = 2068;
/// `f[0] = ` complex unit `a`'s `mVelocity`
pub const EE_UNITS_UCA_VELOCITY_OF: i32 = 2069;
/// `f[0] = unit->GetStats()->mRange`
pub const EE_UNITS_UCA_RANGE: i32 = 2070;
/// `f[0] = EECombatHitDistanceSquared(unit, unit a, -10000.0f, -10000.0f)`
pub const EE_UNITS_UCA_HIT_DISTANCE: i32 = 2071;
/// goal `a`'s `GetDestination(unit, f[0..3])`
pub const EE_UNITS_UCA_DESTINATION: i32 = 2072;
/// `class->mPhysicsModel != NULL && class->mPhysicsModel->IsAirplane()`
pub const EE_UNITS_UCA_AIRPLANE_MODEL: i32 = 2073;
/// `unit->SetF220(class->mPhysicsModel->ComputeRoll(unit, f[0]))`
pub const EE_UNITS_UCA_COMPUTE_ROLL: i32 = 2074;
/// `f[0..3] = unit->mAngularVelocity`'s yaw, pitch, roll
pub const EE_UNITS_UCA_ANGULAR_VELOCITY: i32 = 2075;
/// `unit->mAngularVelocity = GEOrientation(f[0], f[1], f[2])`
pub const EE_UNITS_UCA_SET_ANGULAR_VELOCITY: i32 = 2076;
/// `ee_units_atc_add_cargo_weight(unit->GetPlayer()->mAirTraffic[unit->GetUnitClass()->mAirTrafficType - 1], unit)`
pub const EE_UNITS_UCA_ATC_ADD_WEIGHT: i32 = 2077;
/// the transformation at `a` = the unit's location
pub const EE_UNITS_UCA_LOCATION_MATRIX: i32 = 2078;
/// `f[0] = unit->GetStats()->GetScale()`
pub const EE_UNITS_UCA_STATS_SCALE: i32 = 2079;
/// `unit->GetModel()->mTagPoints[unit->mAnimation]`
pub const EE_UNITS_UCA_TAG_LIST: i32 = 2080;
/// tag list `a`'s `mPoints.size()`
pub const EE_UNITS_UCA_TAG_COUNT: i32 = 2081;
/// tag list `a`'s `mPoints[unit->mWeaponTagPoint]->GetOffset(unit->mPhysicalModel.GetFrame(), f[0..3])`
pub const EE_UNITS_UCA_TAG_OFFSET: i32 = 2082;
/// `f[0] = class->mProjectileClass->mAcceleration`
pub const EE_UNITS_UCA_GRAVITY: i32 = 2083;
/// `f[0] = unit->GetStats()->mLaunchSpeed`
pub const EE_UNITS_UCA_LAUNCH_SPEED: i32 = 2084;
/// `f[0] = unit->GetStats()->mAreaEffect`
pub const EE_UNITS_UCA_AREA_EFFECT: i32 = 2085;
/// `unit->mAmmunition`
pub const EE_UNITS_UCA_AMMUNITION: i32 = 2086;
/// `unit->mPlayer->mPlayerID`
pub const EE_UNITS_UCA_PLAYER_ID: i32 = 2087;
/// `::gWorld.mPlayers[b]`'s fighter tech `c` (Watchman, Guardian, Sentinel) available and allowed
pub const EE_UNITS_UCA_FIGHTER_USABLE: i32 = 2088;
/// `::gWorld.mPlayers[b]`'s fighter tech `c`'s `mData->mUnitClass`
pub const EE_UNITS_UCA_FIGHTER_CLASS: i32 = 2089;
/// `gUnitClassTable[b]`
pub const EE_UNITS_UCA_CLASS_AT: i32 = 2090;
/// `unit->GetNumCargoUnits()`
pub const EE_UNITS_UCA_CARGO_COUNT: i32 = 2091;
/// `unit->mUnitClass->mTransportCapacity`
pub const EE_UNITS_UCA_CAPACITY: i32 = 2092;
/// class `a`'s `CreateUnitAt(the transformation at b, c, 0, unit)->mComplexUnit`
pub const EE_UNITS_UCA_CREATE_CARRIED: i32 = 2093;
/// `unit->LoadCargo(complex unit a, 1, 0)`
pub const EE_UNITS_UCA_LOAD_CARGO: i32 = 2094;
/// `unit->GetPosition(f[0..3])`
pub const EE_UNITS_UCA_POSITION: i32 = 2095;
/// `EEConvertUnit(unit, 0, 0)`
pub const EE_UNITS_UCA_CONVERT: i32 = 2096;
/// complex unit `a`'s `SetFlightTime(b)`
pub const EE_UNITS_UCA_SET_FLIGHT_TIME_OF: i32 = 2097;
/// complex unit `a`'s `DropPath()`
pub const EE_UNITS_UCA_DROP_PATH_OF: i32 = 2098;
/// `unit->DropPath()`
pub const EE_UNITS_UCA_DROP_PATH: i32 = 2099;
/// `f[0] = unit->mTile->GetHeight()`
pub const EE_UNITS_UCA_TILE_HEIGHT: i32 = 2100;
/// `unit->mOrientation = GEOrientation(f[0], f[1], f[2])`
pub const EE_UNITS_UCA_SET_ORIENTATION: i32 = 2101;
/// `f[0] = class->mCruiseAltitude`
pub const EE_UNITS_UCA_CRUISE_FIELD: i32 = 2103;
/// `f[0] = ` complex unit `a`'s `mOpacity`
pub const EE_UNITS_UCA_OPACITY_OF: i32 = 2104;
/// complex unit `a`'s `mOpacity = f[0]`
pub const EE_UNITS_UCA_SET_OPACITY_OF: i32 = 2105;
/// complex unit `a`'s `mNextPart`
pub const EE_UNITS_UCA_NEXT_PART_OF: i32 = 2106;
/// `unit->mHomeBase->GetTileX()`
pub const EE_UNITS_UCA_HOME_TILE_X: i32 = 2107;
/// `unit->mHomeBase->GetTileY()`
pub const EE_UNITS_UCA_HOME_TILE_Y: i32 = 2108;

// ---- the bodies (`ee_units_uca_call`'s `what`)
pub const EE_UNITS_UCA_DO_FLY_TO_GOAL: i32 = 1;
pub const EE_UNITS_UCA_DO_GROUND_HEIGHT: i32 = 2;
pub const EE_UNITS_UCA_DO_LANDABLE_OP: i32 = 3;
pub const EE_UNITS_UCA_DO_LANDABLE_CREATED: i32 = 4;
pub const EE_UNITS_UCA_DO_BOMB_ON_TARGET: i32 = 5;
pub const EE_UNITS_UCA_DO_CARRIER_AIRCRAFT: i32 = 6;
pub const EE_UNITS_UCA_DO_FLY_OFF_MAP: i32 = 7;
pub const EE_UNITS_UCA_DO_AIR_ANIMAL_RETURN: i32 = 8;
pub const EE_UNITS_UCA_DO_AIR_ANIMAL_PATH: i32 = 9;
pub const EE_UNITS_UCA_DO_AIR_ANIMAL_DEATH_ROLL: i32 = 10;
pub const EE_UNITS_UCA_DO_AIR_ANIMAL_CREATED: i32 = 11;
pub const EE_UNITS_UCA_DO_SPACESHIP_HOLDING: i32 = 12;
pub const EE_UNITS_UCA_DO_HELICOPTER_PATH: i32 = 13;
pub const EE_UNITS_UCA_DO_OPACITY_CHAIN: i32 = 14;

const GOAL_LOCATION: i64 = 1;
const GOAL_ATTACK_UNIT: i64 = 2;
const GOAL_ATTACK_GROUND: i64 = 3;
const GOAL_UNLOAD_TRANSPORT: i64 = 8;
const GOAL_PATROL: i64 = 9;
const GOAL_EXPLORE: i64 = 10;
const ACTION_TAKEOFF: i64 = 1000;
const ACTION_CONTINUE_TO_TAXI: i64 = 1003;
const ACTION_FLY: i64 = 1006;
const ACTION_FIND_UNEXPLORED_AREA: i64 = 11002;
/// "no tile" (`-10000`)
const NO_TILE: c_long = -10000;
/// the carrier's default aircraft class (no fighter tech)
const CARRIER_DEFAULT_CLASS: i64 = 0x1f;
/// `1.5707964f`
const HALF_PI: f32 = 1.570_796_4;

/// The Rust a body calls (cls_move.rs's path and waypoint, the unit utilities' aim point).
pub trait UcaRust {
    /// `ee_units_mv_path_toward(unit, from, to, snap)`
    fn path_toward(&mut self, unit: u64, from: [f32; 3], to: [f32; 3], snap: bool);
    /// `ee_units_mv_add_waypoint_at(unit, x, y, z, snap)`
    fn add_waypoint_at(&mut self, unit: u64, p: [f32; 3], snap: bool);
    /// `ee_units_uu_aim_point(unit, target, out)`
    fn aim_point(&mut self, unit: u64, target: [f32; 3]) -> [f32; 3];
}

fn point<H: UcHost + ?Sized>(s: &mut S<'_, H>, op: i32, a: u64) -> [f32; 3] {
    let mut p = [0.0f32; 3];
    s.point(op, a, 0, 0, &mut p);
    p
}

/// `FlyToGoal(point, unit)`: a path from the point to the goal's location at cruising height (an explore goal
/// without a target picks a new area first), the landing point found first.
pub fn fly_to_goal<H: UcHost + UcaRust + ?Sized>(h: &mut H, class: u64, unit: u64, from: [f32; 3]) {
    let mut s = S { h: &mut *h, class, unit };
    let mut target = [0.0f32; 3];
    let goal = s.s(EE_UNITS_UCA_GOAL_STATE) as u64;
    let cast = |s: &mut S<'_, H>, kind: i64| s.ab(EE_UNITS_UCA_GOAL_CAST, goal, kind) as u64;
    if s.a(EE_UNITS_UCA_HAS_LOCATION, goal) != 0 {
        let p = point(&mut s, EE_UNITS_UCA_LOCATION_POINT, goal);
        target[0] = p[0];
        target[1] = p[1];
    } else if cast(&mut s, GOAL_PATROL) != 0 {
        let patrol = cast(&mut s, GOAL_PATROL);
        target[0] = s.a(EE_UNITS_UCA_PATROL_TILE_X, patrol) as c_long as f32 + 0.5;
        target[1] = s.a(EE_UNITS_UCA_PATROL_TILE_Y, patrol) as c_long as f32 + 0.5;
    } else if cast(&mut s, GOAL_EXPLORE) != 0 {
        let explore = cast(&mut s, GOAL_EXPLORE);
        s.point(EE_UNITS_UCA_EXPLORE_CENTER, explore, 0, 0, &mut target);
        if target[0] <= 0.5 && target[1] <= 0.5 {
            s.b(EE_UNITS_UCA_START_ACTION, ACTION_FIND_UNEXPLORED_AREA);
            s.a(EE_UNITS_UCA_GOAL_STEP, explore);
            s.point(EE_UNITS_UCA_EXPLORE_CENTER, explore, 0, 0, &mut target);
            let kind = s.s(EE_UNITS_UCA_AIR_TRAFFIC_TYPE);
            s.b(
                EE_UNITS_UCA_START_ACTION,
                if kind == 2 || kind == 4 { ACTION_CONTINUE_TO_TAXI } else { ACTION_TAKEOFF },
            );
        }
    } else if cast(&mut s, GOAL_ATTACK_GROUND) != 0 {
        let ground = cast(&mut s, GOAL_ATTACK_GROUND);
        let p = point(&mut s, EE_UNITS_UCA_GROUND_TARGET, ground);
        target[0] = p[0];
        target[1] = p[1];
    } else if cast(&mut s, GOAL_UNLOAD_TRANSPORT) != 0 {
        let unload = cast(&mut s, GOAL_UNLOAD_TRANSPORT);
        s.point(EE_UNITS_UCA_GOAL_LOCATION, unload, 0, 0, &mut target);
    } else {
        let attack = cast(&mut s, GOAL_ATTACK_UNIT);
        let enemy = s.a(EE_UNITS_UCA_ATTACK_TARGET, attack) as u64;
        let complex = s.a(EE_UNITS_UCA_COMPLEX_OF, enemy) as u64;
        if s.s(EE_UNITS_UCA_HAS_PROJECTILE) != 0 {
            s.point(EE_UNITS_UCA_FIND_DROP, 0, 0, 0, &mut target);
        } else if complex != 0 {
            let p = point(&mut s, EE_UNITS_UCA_LOCATION_OF, complex);
            target[0] = p[0];
            target[1] = p[1];
        } else {
            let size = s.a(EE_UNITS_UCA_TILE_SIZE_OF, enemy) as c_long as f32;
            target[0] = size * 0.5 + s.a(EE_UNITS_UCA_TILE_X_OF, enemy) as c_long as f32;
            let size = s.a(EE_UNITS_UCA_TILE_SIZE_OF, enemy) as c_long as f32;
            target[1] = size * 0.5 + s.a(EE_UNITS_UCA_TILE_Y_OF, enemy) as c_long as f32;
        }
    }
    target[2] = s.float(EE_UNITS_UCA_CRUISE, 0, 0);
    s.point(EE_UNITS_UCA_LANDING_POINT, 0, 0, 0, &mut target);
    h.path_toward(unit, from, target, true);
}

/// `GetGroundHeight(point, out)`: the ground height under a point (the closest tile's off the map; 0 under water).
pub fn ground_height<H: UcHost + ?Sized>(h: &mut H, class: u64, p: [f32; 3]) -> f32 {
    let mut s = S { h, class, unit: 0 };
    let mut tile = s.abc(EE_UNITS_UCA_GET_TILE, 0, i64::from(floor(p[0])), i64::from(floor(p[1]))) as u64;
    if tile == 0 {
        let mut q = p;
        tile = s.point(EE_UNITS_UCA_CLOSEST_TILE, 0, 0, 0, &mut q) as u64;
    }
    let height = s.float(EE_UNITS_UCA_TILE_HEIGHT_OF, tile, 0);
    if height < 0.0 {
        0.0
    } else {
        height
    }
}

fn set_height<H: UcHost + ?Sized>(s: &mut S<'_, H>, value: f32) {
    s.with_float(EE_UNITS_UCA_SET_HEIGHT, 0, value);
}

fn approach_runway<H: UcHost + ?Sized>(s: &mut S<'_, H>) {
    let (dx, dy) = runway_direction(s.s(EE_UNITS_UCA_HOME_FACING) as u32);
    s.abc(EE_UNITS_UCA_APPROACH_RUNWAY, 0, i64::from(dx), i64::from(dy));
}

/// `EELandableActionOperation(op, class, unit, a, airport)`: the landing aircraft's `ActionChanged` operations that
/// are more than a call (`airport`: FIND_NEAREST_AIRPORT's result, for SET_HOME_BASE_FOUND).
pub fn landable_op<H: UcHost + UcaRust + ?Sized>(
    h: &mut H,
    op: i32,
    class: u64,
    unit: u64,
    a: i64,
    airport: &mut u64,
) -> i64 {
    let mut s = S { h: &mut *h, class, unit };
    let position = point(&mut s, EE_UNITS_UCA_LOCATION, 0);
    let goal = s.s(EE_UNITS_UCA_GOAL_STATE) as u64;
    let runway = |s: &mut S<'_, H>| {
        let kind = s.s(EE_UNITS_UCA_AIR_TRAFFIC_TYPE);
        kind == 1 || kind == 3
    };
    match op {
        EE_UNITS_ACT_APPROACH_CARRIER => {
            s.b(EE_UNITS_UCA_APPROACH_CARRIER, i64::from(a != 0));
        }
        EE_UNITS_ACT_BOARD_CARRIER => {
            let atc = s.s(EE_UNITS_UCA_ATC) as u64;
            s.a(EE_UNITS_UCA_ATC_REMOVE_PLANE, atc);
            s.s(EE_UNITS_UCA_HOME_LOAD);
            s.a(EE_UNITS_UCA_SET_HOME_BASE, 0);
            s.b(EE_UNITS_UCA_SET_COMMANDABLE, 1);
            set_height(&mut s, 0.0);
        }
        EE_UNITS_ACT_TAKE_OFF_FROM_CARRIER => {
            s.s(EE_UNITS_UCA_TAKE_OFF_CARRIER);
        }
        EE_UNITS_ACT_FIND_NEAREST_AIRPORT => {
            let atc = s.s(EE_UNITS_UCA_ATC) as u64;
            *airport = s.a(EE_UNITS_UCA_FIND_NEAREST_AIRPORT, atc) as u64;
            return i64::from(*airport != 0);
        }
        EE_UNITS_ACT_SET_HOME_BASE_FOUND => {
            s.a(EE_UNITS_UCA_SET_HOME_BASE, *airport);
        }
        EE_UNITS_ACT_APPROACH_LANDING => {
            if runway(&mut s) {
                approach_runway(&mut s);
            } else {
                s.b(EE_UNITS_UCA_APPROACH_CARRIER, i64::from(a != 0));
            }
        }
        EE_UNITS_ACT_SECONDARY_TARGET => secondary_target(&mut s, goal),
        EE_UNITS_ACT_TAKEOFF => {
            let atc = s.s(EE_UNITS_UCA_ATC) as u64;
            if s.s(EE_UNITS_UCA_HOME_BASE) == 0 {
                if s.a(EE_UNITS_UCA_ATC_SLOT_COUNT, atc) as u32 == 0 {
                    s.s(EE_UNITS_UCA_KILL);
                } else {
                    if s.s(EE_UNITS_UCA_CONTAINER) == 0 {
                        let base = s.ab(EE_UNITS_UCA_ATC_SLOT_AIRPORT, atc, 0) as u64;
                        s.a(EE_UNITS_UCA_SET_HOME_BASE, base);
                    } else {
                        let mut i: u32 = 0;
                        while i < s.a(EE_UNITS_UCA_ATC_SLOT_COUNT, atc) as u32 {
                            let container = s.s(EE_UNITS_UCA_CONTAINER);
                            if container == s.ab(EE_UNITS_UCA_ATC_SLOT_AIRPORT, atc, i64::from(i)) {
                                let base = s.ab(EE_UNITS_UCA_ATC_SLOT_AIRPORT, atc, i64::from(i)) as u64;
                                s.a(EE_UNITS_UCA_SET_HOME_BASE, base);
                            }
                            i += 1;
                        }
                    }
                    s.s(EE_UNITS_UCA_RETURN_TO_AIRPORT);
                }
            } else if runway(&mut s) {
                let (dx, dy) = runway_direction(s.s(EE_UNITS_UCA_HOME_FACING) as u32);
                s.abc(EE_UNITS_UCA_TAKE_OFF_RUNWAY, 0, i64::from(dx), i64::from(dy));
            } else {
                s.s(EE_UNITS_UCA_TAKE_OFF_CARRIER);
            }
            if s.s(EE_UNITS_UCA_HOME_BASE) == 0 {
                return 0;
            }
            s.s(EE_UNITS_UCA_SET_STATS_FLIGHT_TIME);
            if s.s(EE_UNITS_UCA_PARATROOPER_BOMBER) == 0 {
                s.s(EE_UNITS_UCA_SET_AMMUNITION);
            }
            s.b(EE_UNITS_UCA_SET_ANIMATION, 0xd);
            let mut p = position;
            if s.point(EE_UNITS_UCA_NEXT_WAYPOINT, 0, 0, 0, &mut p) as c_long != 0 {
                return 0;
            }
            let height = s.float_point(EE_UNITS_UCA_HEIGHT_AT, p);
            set_height(&mut s, height);
            return 0;
        }
        EE_UNITS_ACT_QUEUE_LANDING => {
            let atc = s.s(EE_UNITS_UCA_ATC) as u64;
            return s.a(EE_UNITS_UCA_ATC_QUEUE_LANDING, atc);
        }
        EE_UNITS_ACT_BOMBER_GROUND_GOAL => {
            let mut target = [0.0f32; 3];
            s.point(EE_UNITS_UCA_FIND_DROP, 0, 0, 0, &mut target);
            s.point(EE_UNITS_UCA_ATTACK_GROUND_GOAL, goal, 0, 0, &mut target);
        }
        EE_UNITS_ACT_INITIAL_ATTACK_HEIGHT => {
            let height = s.float(EE_UNITS_UCA_HEIGHT, 0, 0);
            if height < s.float(EE_UNITS_UCA_CRUISE, 0, 0) && s.float(EE_UNITS_UCA_CRUISE, 0, 0) != height {
                let cruise = s.float(EE_UNITS_UCA_CRUISE, 0, 0);
                s.with_float(EE_UNITS_UCA_SET_HEIGHT_FIELD, 0, cruise);
            }
        }
        EE_UNITS_ACT_CONTACT_HERE => {
            let mut p = position;
            if a != 0 {
                s.point(EE_UNITS_UCA_SET_CONTACT, 0, 0, 0, &mut p);
            } else if s.point(EE_UNITS_UCA_CONTACT_DIFFERS, 0, 0, 0, &mut p) != 0 {
                s.point(EE_UNITS_UCA_SET_CONTACT_FIELD, 0, 0, 0, &mut p);
            }
            if s.float(EE_UNITS_UCA_CONTACT_HEADING, 0, 0) != 0.0 {
                s.with_float(EE_UNITS_UCA_SET_CONTACT_HEADING, 0, 0.0);
            }
        }
        EE_UNITS_ACT_LANDABLE_PATH => {
            let mut target = [0.0f32; 3];
            if s.a(EE_UNITS_UCA_GOAL_KIND, goal) == GOAL_ATTACK_UNIT {
                let enemy = s.point(EE_UNITS_UCA_FIND_DROP, 0, 0, 0, &mut target) as u64;
                let complex = s.a(EE_UNITS_UCA_COMPLEX_OF, enemy) as u64;
                if complex != 0 {
                    let velocity = s.float(EE_UNITS_UCA_VELOCITY_OF, complex, 0);
                    if velocity < -0.0001 || velocity > 0.0001 {
                        let range = s.float(EE_UNITS_UCA_RANGE, 0, 0);
                        if s.float(EE_UNITS_UCA_HIT_DISTANCE, enemy, 0) > range * range {
                            s.point(EE_UNITS_UCA_DESTINATION, goal, 0, 0, &mut target);
                        }
                    }
                }
            } else {
                s.point(EE_UNITS_UCA_DESTINATION, goal, 0, 0, &mut target);
                if s.a(EE_UNITS_UCA_GOAL_KIND, goal) == GOAL_EXPLORE {
                    target[2] = s.float(EE_UNITS_UCA_CRUISE, 0, 0);
                }
            }
            h.path_toward(unit, position, target, true);
        }
        EE_UNITS_ACT_DEATH_ROLL => {
            if s.s(EE_UNITS_UCA_AIRPLANE_MODEL) != 0 {
                let yaw = point(&mut s, EE_UNITS_UCA_ANGULAR_VELOCITY, 0)[0];
                s.with_float(EE_UNITS_UCA_COMPUTE_ROLL, 0, yaw);
            }
            roll_to_yaw(&mut s);
        }
        _ => {}
    }
    0
}

/// `mAngularVelocity`'s roll set to its yaw
fn roll_to_yaw<H: UcHost + ?Sized>(s: &mut S<'_, H>) {
    let mut o = point(s, EE_UNITS_UCA_ANGULAR_VELOCITY, 0);
    o[2] = o[0];
    s.point(EE_UNITS_UCA_SET_ANGULAR_VELOCITY, 0, 0, 0, &mut o);
}

/// SECONDARY_TARGET: an enemy near the attack target (or the unit when the target is off the map) when the unit may
/// target enemies; else a move to the target's tile at cruising height, or no goal.
fn secondary_target<H: UcHost + ?Sized>(s: &mut S<'_, H>, goal: u64) {
    let attack = s.ab(EE_UNITS_UCA_GOAL_CAST, goal, GOAL_ATTACK_UNIT) as u64;
    if attack == 0 {
        return;
    }
    let target = s.a(EE_UNITS_UCA_ATTACK_TARGET, attack) as u64;
    if target == 0 {
        s.s(EE_UNITS_UCA_CLEAR_GOALS);
        return;
    }
    let mut x = s.a(EE_UNITS_UCA_TILE_X_OF, target) as c_long;
    let y;
    if x == NO_TILE {
        let unit = s.unit;
        x = s.a(EE_UNITS_UCA_TILE_X_OF, unit) as c_long;
        y = s.a(EE_UNITS_UCA_TILE_Y_OF, unit) as c_long;
    } else {
        y = s.a(EE_UNITS_UCA_TILE_Y_OF, target) as c_long;
    }
    let mut f = [3.0, 0.0, 0.0, 0.0];
    let enemy = s.f(EE_UNITS_UCA_FIND_TARGET_NEAR, 0, i64::from(x), i64::from(y), &mut f) as u64;
    if enemy != 0 && s.s(EE_UNITS_UCA_TARGETS_ENEMIES) != 0 {
        s.ab(EE_UNITS_UCA_SET_ATTACK_TARGET, attack, enemy as i64);
        return;
    }
    let target = s.a(EE_UNITS_UCA_ATTACK_TARGET, attack) as u64;
    if s.a(EE_UNITS_UCA_TILE_X_OF, target) as c_long != NO_TILE {
        let target = s.a(EE_UNITS_UCA_ATTACK_TARGET, attack) as u64;
        let tx = s.a(EE_UNITS_UCA_TILE_X_OF, target) as c_long as f32 + 0.5;
        let target = s.a(EE_UNITS_UCA_ATTACK_TARGET, attack) as u64;
        let ty = s.a(EE_UNITS_UCA_TILE_Y_OF, target) as c_long as f32 + 0.5;
        let cruise = s.float(EE_UNITS_UCA_CRUISE, 0, 0);
        let b1c = s.a(EE_UNITS_UCA_GOAL_B1C, attack);
        let mut f = [tx, ty, cruise, 0.0];
        let moving = s.f(EE_UNITS_UCA_NEW_LOCATION, 0, b1c, 0, &mut f) as u64;
        s.a(EE_UNITS_UCA_SET_GOAL, moving);
        let action = s.a(EE_UNITS_UCA_GOAL_START_ACTION, moving);
        s.b(EE_UNITS_UCA_START_ACTION, action);
        s.s(EE_UNITS_UCA_ACTION_CHANGED);
        return;
    }
    s.s(EE_UNITS_UCA_CLEAR_GOALS);
}

impl<H: UcHost + ?Sized> S<'_, H> {
    /// a statement taking a point (`f[0..3]`) and answering a float (`f[0]`)
    fn float_point(&mut self, op: i32, p: [f32; 3]) -> f32 {
        let mut f = [p[0], p[1], p[2], 0.0];
        self.h.uc(op, self.class, self.unit, 0, 0, 0, &mut f);
        f[0]
    }
}

/// `LandableUnitCreated(unit)`: the plane's weight on its air traffic control, on the ground.
pub fn landable_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    base_unit_created(&mut *h, class, unit);
    let mut s = S { h, class, unit };
    s.s(EE_UNITS_UCA_ATC_ADD_WEIGHT);
    set_height(&mut s, 0.0);
}

/// `AirAnimalUnitCreated(unit)`: on the ground (height 0).
pub fn air_animal_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    base_unit_created(&mut *h, class, unit);
    let mut s = S { h, class, unit };
    set_height(&mut s, 0.0);
}

/// `IsBombOnTarget(unit)`: whether a bomb dropped now lands within reach of the attack target along its ballistic
/// arc (reach: half the area effect for the last bomb on a moving target, else 0.25 for the last bomb or none).
pub fn bomb_on_target<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) -> bool {
    let mut s = S { h, class, unit };
    let mut t = GeTransformation::IDENTITY;
    s.a(EE_UNITS_UCA_LOCATION_MATRIX, addr(&mut t.m));
    let tags = s.s(EE_UNITS_UCA_TAG_LIST) as u64;
    let goal = s.s(EE_UNITS_UCA_GOAL_STATE) as u64;
    let attack = s.ab(EE_UNITS_UCA_GOAL_CAST, goal, GOAL_ATTACK_UNIT);
    let gravity = s.float(EE_UNITS_UCA_GRAVITY, 0, 0);
    let speed = s.float(EE_UNITS_UCA_LAUNCH_SPEED, 0, 0);
    if attack == 0 {
        return false;
    }
    let mut offset = [0.0f32; 3];
    if tags == 0 || s.a(EE_UNITS_UCA_TAG_COUNT, tags) == 0 {
        offset = [0.0, 0.0, 0.5];
    } else {
        s.point(EE_UNITS_UCA_TAG_OFFSET, tags, 0, 0, &mut offset);
    }
    for v in &mut offset {
        *v *= s.float(EE_UNITS_UCA_STATS_SCALE, 0, 0);
    }
    let mut start = t.apply_point(Ge3DPoint { x: offset[0], y: offset[1], z: offset[2] });
    let mut target = [0.0f32; 3];
    let enemy = s.point(EE_UNITS_UCA_FIND_DROP, 0, 0, 0, &mut target) as u64;
    let target = Ge3DPoint { x: target[0], y: target[1], z: target[2] };
    let mut reach = s.float(EE_UNITS_UCA_AREA_EFFECT, 0, 0);
    let complex = if enemy != 0 { s.a(EE_UNITS_UCA_COMPLEX_OF, enemy) as u64 } else { 0 };
    let moving = complex != 0 && {
        let v = s.float(EE_UNITS_UCA_VELOCITY_OF, complex, 0);
        v < -0.0001 || v > 0.0001
    };
    if moving && reach > 0.5 {
        if s.s(EE_UNITS_UCA_AMMUNITION) as c_long == 1 {
            reach *= 0.5;
        } else if reach == 0.0 {
            reach = 0.25;
        }
    } else if s.s(EE_UNITS_UCA_AMMUNITION) as c_long == 1 || reach == 0.0 {
        reach = 0.25;
    }
    let mut velocity = GeVector { x: 0.0, y: 1.0, z: 0.0 };
    let mut heading = t.apply_vector(velocity);
    if heading.z > 0.0 {
        heading.z = 0.0;
    }
    let b = -(heading.z * speed);
    let half = gravity * 0.5;
    let discriminant = b * b - (target.z - start.z) * half * 4.0;
    if !(discriminant >= 0.0) {
        return false;
    }
    let root = sqrt(discriminant);
    let other = (-b - root) / (half + half);
    let mut time = (root - b) / (half + half);
    if other > time {
        time = other;
    }
    let yaw = um_arctan2(-heading.x, heading.y);
    let angle = um_arccos(-heading.z) - HALF_PI;
    velocity.y = um_cos(angle) * time * speed;
    velocity.z = heading.z * time * speed - time * time * gravity * 0.5;
    t.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
    let heading = t.apply_vector(velocity);
    start.x += heading.x;
    start.y += heading.y;
    start.z += heading.z;
    reach * reach >= start.distance_to_squared(target)
}

/// `EECreateCarrierAircraft(unit, player)`: a new carrier filled with aircraft of the best of three fighter techs its
/// player has (else class 0x1f).
pub fn carrier_aircraft<H: UcHost + ?Sized>(h: &mut H, unit: u64, player: c_long) {
    let mut s = S { h, class: 0, unit };
    let mut t = GeTransformation::IDENTITY;
    let mut id = player;
    if id == -1 {
        id = s.s(EE_UNITS_UCA_PLAYER_ID) as c_long;
    }
    let mut kind = CARRIER_DEFAULT_CLASS;
    for tech in 0..3 {
        if s.abc(EE_UNITS_UCA_FIGHTER_USABLE, 0, i64::from(id), tech) != 0 {
            kind = s.abc(EE_UNITS_UCA_FIGHTER_CLASS, 0, i64::from(id), tech);
            break;
        }
    }
    let class = s.b(EE_UNITS_UCA_CLASS_AT, kind) as u64;
    t.m[0][3] = -10000.0;
    t.m[1][3] = -10000.0;
    let mut i = s.s(EE_UNITS_UCA_CARGO_COUNT) as c_long;
    while i < s.s(EE_UNITS_UCA_CAPACITY) as c_long {
        let aircraft = s.abc(EE_UNITS_UCA_CREATE_CARRIED, class, addr(&mut t.m) as i64, i64::from(id)) as u64;
        s.a(EE_UNITS_UCA_LOAD_CARGO, aircraft);
        i = i.wrapping_add(1);
    }
}

/// `FlyOffMap(unit)`: the leaving plane handed to player 0, fuelled for 1000 s and flown out 1000 tiles along its
/// direction from the map origin.
pub fn fly_off_map<H: UcHost + UcaRust + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h: &mut *h, class, unit };
    let position = point(&mut s, EE_UNITS_UCA_POSITION, 0);
    let mut direction = GeVector { x: position[0], y: position[1], z: 0.0 };
    direction.normalize();
    let mut x = floor(direction.x * 1000.0) as c_long;
    let mut y = floor(direction.y * 1000.0) as c_long;
    if s.abc(EE_UNITS_UCA_GET_TILE, 0, i64::from(x), i64::from(y)) != 0 {
        x = NO_TILE;
        y = NO_TILE;
    }
    let plane = s.s(EE_UNITS_UCA_CONVERT) as u64;
    s.ab(EE_UNITS_UCA_SET_FLIGHT_TIME_OF, plane, 1_000_000);
    let (fx, fy) = (x as f32 + 0.5, y as f32 + 0.5);
    let mut f = [fx, fy, 0.0, 0.0];
    let goal = s.f(EE_UNITS_UCA_NEW_LOCATION, 0, 1, 0, &mut f);
    s.a(EE_UNITS_UCA_DROP_PATH_OF, plane);
    let cruise = s.float(EE_UNITS_UCA_CRUISE, 0, 0);
    h.add_waypoint_at(plane, [fx, fy, cruise], true);
    let mut s = S { h, class, unit };
    s.ab(EE_UNITS_UC_SET_GOAL, plane, goal);
    s.ab(EE_UNITS_UC_START_ACTION, plane, ACTION_FLY);
    s.a(EE_UNITS_UC_ACTION_CHANGED, plane);
}

/// `AirAnimalReturnToAirport(unit)`: lands the animal like a plane, two waypoints ahead (on the ground, then at 3/4
/// of cruise altitude) and a flight there; true.
pub fn air_animal_return<H: UcHost + UcaRust + ?Sized>(h: &mut H, class: u64, unit: u64) -> bool {
    let mut s = S { h: &mut *h, class, unit };
    let mut t = GeTransformation::IDENTITY;
    s.a(EE_UNITS_UCA_LOCATION_MATRIX, addr(&mut t.m));
    if unit == 0 || s.s(EE_UNITS_UCA_ATC) == 0 {
        return true;
    }
    s.s(EE_UNITS_UCA_DROP_PATH);
    let p = t.apply_point(Ge3DPoint { x: 0.0, y: 3.0, z: 0.0 });
    let ground = s.float(EE_UNITS_UCA_TILE_HEIGHT, 0, 0);
    h.add_waypoint_at(unit, [p.x, p.y, ground], true);
    let mut s = S { h: &mut *h, class, unit };
    let p = t.apply_point(Ge3DPoint { x: 0.0, y: 7.0, z: 0.0 });
    let mut height = s.float(EE_UNITS_UCA_CRUISE, 0, 0) * 0.75;
    if s.float(EE_UNITS_UCA_TILE_HEIGHT, 0, 0) > height {
        height += s.float(EE_UNITS_UCA_TILE_HEIGHT, 0, 0);
    }
    h.add_waypoint_at(unit, [p.x, p.y, height], true);
    let mut s = S { h: &mut *h, class, unit };
    let o = t.orientation();
    let mut f = [o.yaw, o.pitch, o.roll];
    s.point(EE_UNITS_UCA_SET_ORIENTATION, 0, 0, 0, &mut f);
    fly_to_goal(h, class, unit, [p.x, p.y, height]);
    true
}

/// `EEAirAnimalPathToGoal(class, unit)`: the path to the goal when preparing to move.
pub fn air_animal_path<H: UcHost + UcaRust + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h: &mut *h, class, unit };
    let position = point(&mut s, EE_UNITS_UCA_LOCATION, 0);
    let mut target = [0.0f32; 3];
    let goal = s.s(EE_UNITS_UCA_GOAL_STATE) as u64;
    if s.a(EE_UNITS_UCA_GOAL_KIND, goal) == GOAL_ATTACK_UNIT {
        let enemy = s.point(EE_UNITS_UCA_FIND_DROP, 0, 0, 0, &mut target) as u64;
        let range = s.float(EE_UNITS_UCA_RANGE, 0, 0);
        if s.float(EE_UNITS_UCA_HIT_DISTANCE, enemy, 0) > range * range {
            s.point(EE_UNITS_UCA_DESTINATION, goal, 0, 0, &mut target);
        }
    } else {
        s.point(EE_UNITS_UCA_DESTINATION, goal, 0, 0, &mut target);
    }
    if s.a(EE_UNITS_UCA_GOAL_KIND, goal) == GOAL_EXPLORE {
        target[2] = s.float(EE_UNITS_UCA_CRUISE, 0, 0);
    }
    h.path_toward(unit, position, target, true);
}

/// `EEAirAnimalDeathRoll(class, unit)`: the dying spin.
pub fn air_animal_death_roll<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    let yaw = point(&mut s, EE_UNITS_UCA_ANGULAR_VELOCITY, 0)[0];
    s.with_float(EE_UNITS_UCA_COMPUTE_ROLL, 0, yaw);
    roll_to_yaw(&mut s);
}

/// `EEPlanetSpaceshipHoldingPattern(class, unit)`: the holding pattern over the home airport (dies without one).
pub fn spaceship_holding<H: UcHost + UcaRust + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h: &mut *h, class, unit };
    let atc = s.s(EE_UNITS_UCA_ATC) as u64;
    let mut position = point(&mut s, EE_UNITS_UCA_LOCATION, 0);
    if s.s(EE_UNITS_UCA_HOME_BASE) == 0 {
        if s.a(EE_UNITS_UCA_ATC_SLOT_COUNT, atc) as u32 > 0 {
            let base = s.ab(EE_UNITS_UCA_ATC_SLOT_AIRPORT, atc, 0) as u64;
            s.a(EE_UNITS_UCA_SET_HOME_BASE, base);
        } else {
            s.s(EE_UNITS_UCA_KILL);
        }
    }
    if s.s(EE_UNITS_UCA_HOME_BASE) == 0 {
        return;
    }
    s.s(EE_UNITS_UCA_DROP_PATH);
    let x = s.s(EE_UNITS_UCA_HOME_TILE_X) as c_long as f32 + 1.5;
    let y = s.s(EE_UNITS_UCA_HOME_TILE_Y) as c_long as f32 + 1.5;
    let z = s.float(EE_UNITS_UCA_CRUISE, 0, 0);
    h.path_toward(unit, position, [x, y, z], true);
    let mut s = S { h, class, unit };
    s.b(EE_UNITS_UCA_SET_COMMANDABLE, 0);
    if s.point(EE_UNITS_UCA_NEXT_WAYPOINT, 0, 0, 0, &mut position) as c_long == 0 {
        let height = s.float_point(EE_UNITS_UCA_HEIGHT_AT, position);
        set_height(&mut s, height);
    }
}

/// `EEHelicopterPathToGoal(class, unit)`: the path to the goal when preparing to move (an attack aims at the drop
/// location), the landing point found first.
pub fn helicopter_path<H: UcHost + UcaRust + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h: &mut *h, class, unit };
    let position = point(&mut s, EE_UNITS_UCA_LOCATION, 0);
    let mut target = [0.0f32; 3];
    let goal = s.s(EE_UNITS_UCA_GOAL_STATE) as u64;
    if s.a(EE_UNITS_UCA_GOAL_KIND, goal) == GOAL_ATTACK_UNIT {
        let mut enemy = [0.0f32; 3];
        s.point(EE_UNITS_UCA_FIND_DROP, 0, 0, 0, &mut enemy);
        target = h.aim_point(unit, enemy);
    } else {
        s.point(EE_UNITS_UCA_DESTINATION, goal, 0, 0, &mut target);
    }
    let mut s = S { h: &mut *h, class, unit };
    if s.a(EE_UNITS_UCA_GOAL_KIND, goal) == GOAL_EXPLORE {
        target[2] = s.float(EE_UNITS_UCA_CRUISE_FIELD, 0, 0);
    }
    s.point(EE_UNITS_UCA_LANDING_POINT, 0, 0, 0, &mut target);
    h.path_toward(unit, position, target, false);
}

/// `EEComplexUnit::SetOpacityChain(value)`: the opacity of the unit and the parts linked after it (stops at the first
/// already at the value).
pub fn opacity_chain<H: UcHost + ?Sized>(h: &mut H, unit: u64, value: f32) {
    let mut s = S { h, class: 0, unit };
    let mut part = unit;
    loop {
        if s.float(EE_UNITS_UCA_OPACITY_OF, part, 0) == value {
            return;
        }
        s.with_float(EE_UNITS_UCA_SET_OPACITY_OF, part, value);
        part = s.a(EE_UNITS_UCA_NEXT_PART_OF, part) as u64;
        if part == 0 {
            return;
        }
    }
}

/// Runs body `what` of `class` on `unit` (`b` its argument, `p` its floats); the answer, or 0. (The landable
/// operation is [`landable_op`].)
pub fn run<H: UcHost + UcaRust + ?Sized>(h: &mut H, what: i32, class: u64, unit: u64, b: i64, p: &mut [f32; 3]) -> i64 {
    match what {
        EE_UNITS_UCA_DO_FLY_TO_GOAL => fly_to_goal(h, class, unit, *p),
        EE_UNITS_UCA_DO_GROUND_HEIGHT => p[0] = ground_height(h, class, *p),
        EE_UNITS_UCA_DO_LANDABLE_CREATED => landable_created(h, class, unit),
        EE_UNITS_UCA_DO_BOMB_ON_TARGET => return i64::from(bomb_on_target(h, class, unit)),
        EE_UNITS_UCA_DO_CARRIER_AIRCRAFT => carrier_aircraft(h, unit, b as c_long),
        EE_UNITS_UCA_DO_FLY_OFF_MAP => fly_off_map(h, class, unit),
        EE_UNITS_UCA_DO_AIR_ANIMAL_RETURN => return i64::from(air_animal_return(h, class, unit)),
        EE_UNITS_UCA_DO_AIR_ANIMAL_PATH => air_animal_path(h, class, unit),
        EE_UNITS_UCA_DO_AIR_ANIMAL_DEATH_ROLL => air_animal_death_roll(h, class, unit),
        EE_UNITS_UCA_DO_AIR_ANIMAL_CREATED => air_animal_created(h, class, unit),
        EE_UNITS_UCA_DO_SPACESHIP_HOLDING => spaceship_holding(h, class, unit),
        EE_UNITS_UCA_DO_HELICOPTER_PATH => helicopter_path(h, class, unit),
        EE_UNITS_UCA_DO_OPACITY_CHAIN => opacity_chain(h, unit, p[0]),
        _ => {}
    }
    0
}

#[cfg(not(test))]
mod game {
    use super::*;
    use std::os::raw::c_void;

    extern "C" {
        fn ee_units_uca_host(
            op: i32,
            class: *mut c_void,
            unit: *mut c_void,
            a: u64,
            b: i64,
            c: i64,
            f: *mut [f32; 4],
        ) -> i64;
    }

    /// The air classes' host (`ee_units_uca_host`; the class host's statements pass through it).
    struct GameA;

    impl UcHost for GameA {
        #[allow(clippy::too_many_arguments)]
        fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            // SAFETY: the body runs inside the C++ member that handed over its live class and unit
            unsafe { ee_units_uca_host(op, class as usize as *mut c_void, unit as usize as *mut c_void, a, b, c, f) }
        }
    }

    impl UcaRust for GameA {
        fn path_toward(&mut self, unit: u64, from: [f32; 3], to: [f32; 3], snap: bool) {
            crate::units::cls_move_host::ee_units_mv_path_toward(
                unit,
                from[0],
                from[1],
                from[2],
                to[0],
                to[1],
                to[2],
                u8::from(snap),
            );
        }
        fn add_waypoint_at(&mut self, unit: u64, p: [f32; 3], snap: bool) {
            crate::units::cls_move_host::ee_units_mv_add_waypoint_at(unit, p[0], p[1], p[2], u8::from(snap));
        }
        fn aim_point(&mut self, unit: u64, target: [f32; 3]) -> [f32; 3] {
            let mut out = [0.0f32; 3];
            // SAFETY: two live points
            unsafe { crate::units::uu_ffi::ee_units_uu_aim_point(unit, &target, &mut out) };
            out
        }
    }

    /// The air classes' body `what` (`EE_UNITS_UCA_DO_*`) of `class` on `unit`, with its arguments `a`, `b`, `c` and
    /// its floats `p` (three, in and out; NULL for none). For `EE_UNITS_UCA_DO_LANDABLE_OP`: `a` the airport slot's
    /// address, `b` the operation, `c` its argument; `EE_UNITS_UCA_DO_GROUND_HEIGHT`: `a` the output's address. The
    /// answer, or 0.
    ///
    /// # Safety
    /// `class` is a live `EEUnitClass` (or NULL where the body takes none), `unit` a live `EEComplexUnit` (or NULL),
    /// `p` three floats (one for the opacity chain) or NULL, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_uca_call(
        what: i32,
        class: *mut c_void,
        unit: *mut c_void,
        a: u64,
        b: i64,
        c: i64,
        p: *mut f32,
    ) -> i64 {
        let mut f = [0.0f32; 3];
        if !p.is_null() {
            // the opacity chain hands one float, the others a point
            let n = if what == EE_UNITS_UCA_DO_OPACITY_CHAIN { 1 } else { 3 };
            // SAFETY: the caller's floats
            unsafe { f[..n].copy_from_slice(std::slice::from_raw_parts(p, n)) };
        }
        let (class, unit) = (class as usize as u64, unit as usize as u64);
        if what == EE_UNITS_UCA_DO_LANDABLE_OP {
            // SAFETY: the caller's airport slot (an `EEComplexUnit*`)
            let slot = unsafe { &mut *(a as usize as *mut u64) };
            return landable_op(&mut GameA, b as i32, class, unit, c, slot);
        }
        let r = run(&mut GameA, what, class, unit, b, &mut f);
        if what == EE_UNITS_UCA_DO_GROUND_HEIGHT {
            // SAFETY: the caller's output float
            unsafe { *(a as usize as *mut f32) = f[0] };
        }
        r
    }
}

#[cfg(test)]
#[path = "uca_air_tests.rs"]
mod tests;
