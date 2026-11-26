//! The buildings' unit bodies over their C++ statements (the units' lane, round 7; were EEUnitClassBuildings.cpp):
//! `BuildingSelectUnitToRelease`, `NavalBuildingSelectUnitToRelease`, `ReleaseUnit` (an air base's, dock's or
//! carrier's exiting unit placed, given its goals and sent off), `BuildingUnitCreated`, `UpdateArmed`,
//! `PriestVirtualB8` (the temple's tick), `BuildingCreateUnit`/`BuildingCreateUnitAt`, the mining and space
//! buildings' creations, `BuildingGetSurroundingTiles`, `EEGetWallPiecePart`. Each C++ statement one
//! `EE_UNITS_UCB_*` case of `ee_units_ucb_host` (EEUnitClassBuildings.cpp; the class host's statements,
//! `EE_UNITS_UC_*`, reach EEUnitClass.cpp's through it), in the original's order.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::uc_anim::*;
use crate::units::uc_body::*;
use crate::units::uc_create::*;
use crate::units::uc_produce::*;
use ee_base::math::transform::RotationOrder;
use ee_base::math::types::GeTransformation;
use ee_base::math::um::{floor, floor_unsigned, um_cos};
use std::f32::consts::{FRAC_PI_2, PI};
use std::os::raw::c_long;

// ---- the statements (from 1000; `unit` the building)
/// `unit->mCargo.size()`
pub const EE_UNITS_UCB_CARGO_SIZE: i32 = 1000;
/// `unit->mCargo[b]`
pub const EE_UNITS_UCB_CARGO_INDEX: i32 = 1001;
/// complex unit `a`'s `GetHitPoints()`
pub const EE_UNITS_UCB_HIT_POINTS_OF: i32 = 1002;
/// complex unit `a`'s `GetStats()->mMaxHitPoints`
pub const EE_UNITS_UCB_MAX_HIT_POINTS_OF: i32 = 1003;
/// complex unit `a`'s `GetPlayer()->IsHuman()`
pub const EE_UNITS_UCB_HUMAN_OF: i32 = 1004;
/// `unit->mGoalQueues[b].GetUnit()->mPlayer->mShiftMode`
pub const EE_UNITS_UCB_QUEUE_SHIFT_MODE: i32 = 1005;
/// complex unit `a`'s `mGoalState`
pub const EE_UNITS_UCB_GOAL_STATE_OF: i32 = 1006;
/// complex unit `a`'s `mUnitID`
pub const EE_UNITS_UCB_UNIT_ID_OF: i32 = 1007;
/// `unit->SetExitingUnitID(b)`
pub const EE_UNITS_UCB_SET_EXITING: i32 = 1008;
/// complex unit `a`'s `mIsDetachedFromAI`
pub const EE_UNITS_UCB_DETACHED: i32 = 1009;
/// complex unit `a`'s `mGoalQueues[0].Clear()`
pub const EE_UNITS_UCB_CLEAR_QUEUE0: i32 = 1010;
/// complex unit `a`'s `RefreshGoals()`
pub const EE_UNITS_UCB_REFRESH_GOALS: i32 = 1011;
/// complex unit `a`'s `RefreshActiveGoal()`
pub const EE_UNITS_UCB_REFRESH_ACTIVE: i32 = 1012;
/// complex unit `a`'s `GetQueueGoals(b).size()`
pub const EE_UNITS_UCB_QUEUE_LEN_OF: i32 = 1013;
/// complex unit `a`'s `mGoalQueues[b].GetFront()`
pub const EE_UNITS_UCB_QUEUE_FRONT_OF: i32 = 1014;
/// `EEExploreGoalCast(goal a)`
pub const EE_UNITS_UCB_EXPLORE_CAST: i32 = 1015;
/// `EEAttackUnitCast(goal a)`
pub const EE_UNITS_UCB_ATTACK_CAST: i32 = 1016;
/// `EEGoalHasLocation(goal a)`
pub const EE_UNITS_UCB_HAS_LOCATION: i32 = 1017;
/// complex unit `a`'s `mHitPoints`
pub const EE_UNITS_UCB_HIT_POINTS_FIELD_OF: i32 = 1018;
/// complex unit `a`'s `mStats->mMaxHitPoints`
pub const EE_UNITS_UCB_MAX_HIT_POINTS_FIELD_OF: i32 = 1019;
/// complex unit `a`'s `mPlayer->IsHuman()`
pub const EE_UNITS_UCB_PLAYER_HUMAN_OF: i32 = 1020;
/// `unit->mPlayer->mComputerPlayer`
pub const EE_UNITS_UCB_COMPUTER_PLAYER: i32 = 1021;
/// `unit->mExitingUnitID`
pub const EE_UNITS_UCB_EXITING_ID: i32 = 1022;
/// `unit->mUnitClass->mAirTrafficType`
pub const EE_UNITS_UCB_AIR_TRAFFIC_SELF: i32 = 1023;
/// `class->mTileSize`
pub const EE_UNITS_UCB_TILE_SIZE_FIELD: i32 = 1024;
/// `unit->mFacing`
pub const EE_UNITS_UCB_FACING: i32 = 1025;
/// `f[0] = ` the yaw of `unit->mPhysicalModel.GetLocation().ConvertToOrientation`
pub const EE_UNITS_UCB_LOCATION_YAW: i32 = 1026;
/// `unit->FindPassenger(b)`
pub const EE_UNITS_UCB_FIND_PASSENGER: i32 = 1027;
/// tag point list `a`'s `mPoints.empty()`
pub const EE_UNITS_UCB_TAGS_EMPTY: i32 = 1028;
/// tag point list `a`'s `Scale(the transformation at b, unit->mStats->GetScale())`
pub const EE_UNITS_UCB_TAGS_SCALE_STATS: i32 = 1029;
/// tag point list `a`'s `mPoints[0]->GetPoint(complex unit b->mPhysicalModel.GetFrame(), the transformation at c,
/// f[0..3])`
pub const EE_UNITS_UCB_TAG0_POINT: i32 = 1030;
/// complex unit `a`'s `mPhysicalModel.SetScale(a->mStats->GetScale() * ((f[0] * 0.5f) /
/// a->mUnitClass->GetCruiseAltitude() + 0.5f))`
pub const EE_UNITS_UCB_SET_CARGO_SCALE: i32 = 1031;
/// `unit->UnloadCargoByID(b, the transformation at a)`
pub const EE_UNITS_UCB_UNLOAD_BY_ID: i32 = 1032;
/// `unit->mUnitClass->mCategory`
pub const EE_UNITS_UCB_CATEGORY_SELF: i32 = 1033;
/// complex unit `a`'s `GetQueueGoals(b)[c]`
pub const EE_UNITS_UCB_QUEUE_GOAL_OF: i32 = 1034;
/// unit `a`'s `mHitPoints`
pub const EE_UNITS_UCB_UNIT_HIT_POINTS: i32 = 1035;
/// `unit->mUnitClass->mUnitType`
pub const EE_UNITS_UCB_UNIT_TYPE_SELF: i32 = 1036;
/// complex unit `a`'s `mUnitClass->mCategory`
pub const EE_UNITS_UCB_CATEGORY_OF: i32 = 1037;
/// complex unit `a`'s `mUnitClass->mFamily`
pub const EE_UNITS_UCB_FAMILY_OF: i32 = 1038;
/// complex unit `a`'s `mUnitClass->mIsParatrooperBomber`
pub const EE_UNITS_UCB_PARATROOPER_OF: i32 = 1039;
/// complex unit `a`'s `GetGoal()`
pub const EE_UNITS_UCB_GOAL_OF: i32 = 1040;
/// `unit->LoadCargo(complex unit a, false, 0)`
pub const EE_UNITS_UCB_LOAD_CARGO: i32 = 1041;
/// `ee_units_atc_remove_from_takeoff_queues(a->mPlayer->mAirTraffic[a->mUnitClass->mAirTrafficType - 1], a)`
pub const EE_UNITS_UCB_ATC_REMOVE_TAKEOFF: i32 = 1042;
/// `unit->mGoalQueues[b].mGoals.size()`
pub const EE_UNITS_UCB_UNIT_QUEUE_LEN: i32 = 1043;
/// `unit->mGoalQueues[b].mGoals[c]`
pub const EE_UNITS_UCB_UNIT_QUEUE_GOAL: i32 = 1044;
/// `EEIsTileFree(complex unit a, b, c)`
pub const EE_UNITS_UCB_TILE_FREE: i32 = 1045;
/// `EEUGRust::NewUnloadTransport(complex unit a, b, c, false)`
pub const EE_UNITS_UCB_NEW_UNLOAD_TRANSPORT: i32 = 1046;
/// complex unit `a`'s `mUnitClass->mUnitType`
pub const EE_UNITS_UCB_UNIT_TYPE_OF: i32 = 1047;
/// complex unit `a`'s `SetHomeBase(unit)`
pub const EE_UNITS_UCB_SET_HOME_BASE: i32 = 1048;
/// `unit->SetBuilt(b, c & 1, c >> 1)`
pub const EE_UNITS_UCB_SET_BUILT: i32 = 1049;
/// `unit->SetResourceType(class->mResourceType)`
pub const EE_UNITS_UCB_SET_RESOURCE_TYPE: i32 = 1051;
/// `unit->mResourceType`
pub const EE_UNITS_UCB_RESOURCE_TYPE: i32 = 1052;
/// `unit->SetResourceAmount(class->mResourceAmount)`
pub const EE_UNITS_UCB_SET_RESOURCE_AMOUNT: i32 = 1053;
/// `EEGetUnitFacingQuarter(unit)`
pub const EE_UNITS_UCB_FACING_QUARTER: i32 = 1054;
/// `unit->SetFacing(b)`
pub const EE_UNITS_UCB_SET_FACING: i32 = 1055;
/// `GetNavyYardFacing(unit->GetTileX(), unit->GetTileY())`
pub const EE_UNITS_UCB_NAVY_YARD_FACING: i32 = 1057;
/// `f[0] = EEGetSpaceDockYaw(unit->GetTileX() + 1, unit->GetTileY() + 1, unit->mUnitClass->GetTileSize())`
pub const EE_UNITS_UCB_SPACE_DOCK_YAW: i32 = 1058;
/// `unit->mIsDisabled`
pub const EE_UNITS_UCB_DISABLED: i32 = 1059;
/// `unit->mCurrentPhysicsAction`
pub const EE_UNITS_UCB_PHYSICS_ACTION: i32 = 1060;
/// `unit->UpdateTurrets()`
pub const EE_UNITS_UCB_UPDATE_TURRETS: i32 = 1061;
/// `unit->mTileX`
pub const EE_UNITS_UCB_TILE_X_FIELD: i32 = 1062;
/// `unit->UpdateHealSearchTimer()`
pub const EE_UNITS_UCB_UPDATE_HEAL_SEARCH: i32 = 1063;
/// `class->mIsMedic`
pub const EE_UNITS_UCB_IS_MEDIC: i32 = 1064;
/// `unit->mGoalState->Work(unit)`
pub const EE_UNITS_UCB_GOAL_WORK: i32 = 1065;
/// `unit->mStats->mRegeneration`
pub const EE_UNITS_UCB_REGENERATION: i32 = 1066;
/// `class->RegeneratePower(unit)`
pub const EE_UNITS_UCB_REGENERATE_POWER: i32 = 1068;
/// `class->BasicPriestUpdateConversion(unit)`
pub const EE_UNITS_UCB_PRIEST_CONVERSION: i32 = 1069;
/// `unit->mConversionStartTime`
pub const EE_UNITS_UCB_CONVERSION_START: i32 = 1070;
/// `unit->mConversionStartTime = 0`
pub const EE_UNITS_UCB_CLEAR_CONVERSION_START: i32 = 1071;
/// `unit->ReleaseConversionEffect()`
pub const EE_UNITS_UCB_RELEASE_CONVERSION: i32 = 1072;
/// `class->BasicPriestHideConversionEffects(unit)`
pub const EE_UNITS_UCB_HIDE_CONVERSION: i32 = 1073;
/// `class->mCruiseAltitude` (`f[0]`)
pub const EE_UNITS_UCB_CRUISE_ALTITUDE: i32 = 1075;
/// unit `a`'s `CanGather(unit b, b->GetTileX(), b->GetTileY(), 0, 0)`
pub const EE_UNITS_UCB_CAN_GATHER: i32 = 1076;
/// tile `a`'s `mUnits` (the first node)
pub const EE_UNITS_UCB_TILE_UNITS: i32 = 1077;
/// node `a`'s `mUnit`
pub const EE_UNITS_UCB_NODE_UNIT: i32 = 1078;
/// node `a`'s `mNext`
pub const EE_UNITS_UCB_NODE_NEXT: i32 = 1079;
/// unit `a`'s `mComplexUnit->SetMine(unit b)`
pub const EE_UNITS_UCB_SET_MINE: i32 = 1080;
/// complex unit `a`'s `mPreviousPart`
pub const EE_UNITS_UCB_PREVIOUS_PART_OF: i32 = 1081;
/// complex unit `a`'s `mUnitClass->GetTileSize()`
pub const EE_UNITS_UCB_TILE_SIZE_OF: i32 = 1082;
/// `EEGetFacingQuarter(b, c)`
pub const EE_UNITS_UCB_FACING_QUARTER_AT: i32 = 1083;
/// the tiles list `a`'s `clear()`
pub const EE_UNITS_UCB_TILES_CLEAR: i32 = 1085;
/// `class->BaseGetSurroundingTiles(unit, the list a, b, c != 0)`
pub const EE_UNITS_UCB_BASE_SURROUNDING: i32 = 1086;
/// complex unit `a`'s `mComplexUnit`
pub const EE_UNITS_UCB_COMPLEX_FIELD_OF: i32 = 1087;

// ---- the bodies (`ee_units_ucb_call`'s `what`)
pub const EE_UNITS_UCB_DO_SELECT_TO_RELEASE: i32 = 1;
pub const EE_UNITS_UCB_DO_NAVAL_SELECT_TO_RELEASE: i32 = 2;
pub const EE_UNITS_UCB_DO_RELEASE_UNIT: i32 = 3;
pub const EE_UNITS_UCB_DO_UNIT_CREATED: i32 = 4;
pub const EE_UNITS_UCB_DO_UPDATE_ARMED: i32 = 5;
pub const EE_UNITS_UCB_DO_PRIEST_UPDATE: i32 = 6;
pub const EE_UNITS_UCB_DO_CREATE_UNIT_AT: i32 = 7;
pub const EE_UNITS_UCB_DO_CREATE_UNIT: i32 = 8;
pub const EE_UNITS_UCB_DO_MINING_CREATE_UNIT_AT: i32 = 9;
pub const EE_UNITS_UCB_DO_MINING_CREATE_UNIT: i32 = 10;
pub const EE_UNITS_UCB_DO_SPACE_CREATE_UNIT_AT: i32 = 11;
pub const EE_UNITS_UCB_DO_SPACE_CREATE_UNIT: i32 = 12;
pub const EE_UNITS_UCB_DO_SURROUNDING_TILES: i32 = 13;

const FAMILY_AIRCRAFT: i64 = 5;
const FAMILY_SPACE_FIGHTER: i64 = 0x39;
const FAMILY_BOMBER: i64 = 0x14;
const ACTION_FIND_UNEXPLORED_AREA: i64 = 11002;
const ACTION_TAKEOFF: i64 = 1000;
const ACTION_ATTACK_ENEMY_UNIT: i64 = 2001;
const ACTION_ATTACK_LOCATION: i64 = 2002;
const ACTION_TRACK_ENEMY_UNIT: i64 = 2011;
const ACTION_WAIT_FOR_RELOAD: i64 = 2021;
const ACTION_UNIT_ROTTING: i64 = 6002;
const ACTION_RESOURCES_ROTTING: i64 = 6001;
const ACTION_CONVERT_ENEMY_UNIT: i64 = 2004;
const UNIT_TYPE_PARATROOPER_PLANE: i64 = 0x2e1;
const UNIT_TYPE_NAVY_YARD: i64 = 0x41;
const UNIT_TYPE_DOCK: i64 = 0x24;
const UNIT_TYPE_AIRPORT: i64 = 0x42;
const UNIT_TYPE_SPACE_DOCK: i64 = 0x2d8;
const UNIT_TYPE_CAMERA_OBJECT: i64 = 0x167;
const CATEGORY_NAVAL_BUILDING: i64 = 0x15;
const CATEGORY_SPACE_BUILDING: i64 = 0x20;
const CATEGORY_AIRPLANE: i64 = 3;
const CATEGORY_PLANET_SPACESHIP: i64 = 0x22;
const RESOURCE_NONE: i64 = 0;
const GOAL_HEAL_UNIT: i64 = 15;
/// `kBuildingLift` (a building sits just above the ground)
const BUILDING_LIFT: f32 = 0.001;
/// the yaws of the four map quarters (kQuarterYaws; the exiting unit's by its base's facing)
const FACING_YAWS: [f32; 4] = [0.0, PI, -FRAC_PI_2, FRAC_PI_2];
const HALF_PI: f32 = FRAC_PI_2;

/// The unit utilities' Rust the buildings call (their FFI functions, as the C++ called them).
pub trait UcbRust {
    /// `ee_units_uu_block_ring(corners, x, y, width, height, tiles, append, 0)`
    fn block_ring(&mut self, corners: i32, ring: [i32; 4], tiles: u64, append: bool);
}

/// The building's statements on its class and unit.
fn st<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) -> S<'_, H> {
    S { h, class, unit }
}

/// The queue a released aircraft's rally goal waits in (by its family): 1 fighters, 2 bombers, 4 paratroopers, 3
/// else
fn family_queue(family: i64, paratrooper: bool) -> i64 {
    if family == FAMILY_AIRCRAFT || family == FAMILY_SPACE_FIGHTER {
        1
    } else if family == FAMILY_BOMBER {
        if paratrooper {
            4
        } else {
            2
        }
    } else {
        3
    }
}

/// `BuildingSelectUnitToRelease(unit)`: the first fully healed garrisoned unit that should leave picked as the
/// exiting unit; whether one was.
pub fn select_to_release<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) -> bool {
    let mut s = st(h, class, unit);
    let mut result = false;
    let count = s.s(EE_UNITS_UCB_CARGO_SIZE) as u32;
    for i in 0..count {
        let cargo = s.b(EE_UNITS_UCB_CARGO_INDEX, i64::from(i)) as u64;
        if s.a(EE_UNITS_UCB_HIT_POINTS_OF, cargo) == s.a(EE_UNITS_UCB_MAX_HIT_POINTS_OF, cargo) {
            let mut leave = false;
            if s.a(EE_UNITS_UCB_HUMAN_OF, cargo) != 0 {
                let cargo_class = s.a(EE_UNITS_UC_CLASS_OF, cargo) as u64;
                let family = s.a(EE_UNITS_UC_CLASS_FAMILY, cargo_class);
                let queue = if family == FAMILY_BOMBER {
                    family_queue(family, s.a(EE_UNITS_UC_PARATROOPER_BOMBER, cargo) != 0)
                } else {
                    family_queue(family, false)
                };
                if s.b(EE_UNITS_UCB_QUEUE_SHIFT_MODE, queue) == 0 && s.b(EE_UNITS_UC_QUEUED_GOAL, queue) != 0 {
                    leave = true;
                }
            }
            if leave || s.a(EE_UNITS_UCB_GOAL_STATE_OF, cargo) != 0 {
                let id = s.a(EE_UNITS_UCB_UNIT_ID_OF, cargo);
                s.b(EE_UNITS_UCB_SET_EXITING, id);
                result = true;
                if s.a(EE_UNITS_UCB_GOAL_STATE_OF, cargo) != 0
                    && s.a(EE_UNITS_UCB_HUMAN_OF, cargo) != 0
                    && s.a(EE_UNITS_UCB_DETACHED, cargo) == 0
                {
                    s.a(EE_UNITS_UCB_CLEAR_QUEUE0, cargo);
                    s.a(EE_UNITS_UCB_REFRESH_GOALS, cargo);
                    s.a(EE_UNITS_UCB_REFRESH_ACTIVE, cargo);
                }
            }
            if s.ab(EE_UNITS_UCB_QUEUE_LEN_OF, cargo, 1) > 0 {
                let goal = s.ab(EE_UNITS_UCB_QUEUE_FRONT_OF, cargo, 1) as u64;
                let explore = s.a(EE_UNITS_UCB_EXPLORE_CAST, goal);
                if goal != 0 {
                    s.a(EE_UNITS_UCB_CLEAR_QUEUE0, cargo);
                    let id = s.a(EE_UNITS_UCB_UNIT_ID_OF, cargo);
                    s.b(EE_UNITS_UCB_SET_EXITING, id);
                    if explore != 0 {
                        s.ab(EE_UNITS_UC_START_ACTION, cargo, ACTION_FIND_UNEXPLORED_AREA);
                        s.a(EE_UNITS_UC_ACTION_CHANGED, cargo);
                    }
                    result = true;
                }
            }
        }
        if result {
            break;
        }
    }
    result
}

/// A rally goal a ship's or the dock's queue holds: an attack, move or explore goal
fn rally_goal<H: UcHost + ?Sized>(s: &mut S<'_, H>, goal: u64) -> bool {
    if s.a(EE_UNITS_UCB_ATTACK_CAST, goal) == 0
        && s.a(EE_UNITS_UCB_HAS_LOCATION, goal) == 0
        && s.a(EE_UNITS_UCB_EXPLORE_CAST, goal) == 0
    {
        return false;
    }
    goal != 0
}

/// `NavalBuildingSelectUnitToRelease(unit)`: the first fully healed docked ship that should leave picked as the
/// exiting unit; whether one was.
pub fn naval_select_to_release<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) -> bool {
    let mut s = st(h, class, unit);
    let mut result = false;
    let count = s.s(EE_UNITS_UC_CARGO_COUNT) as u32;
    let mut i = 0u32;
    while !result && i < count {
        let cargo = s.b(EE_UNITS_UCB_CARGO_INDEX, i64::from(i)) as u64;
        i += 1;
        if s.a(EE_UNITS_UCB_HIT_POINTS_FIELD_OF, cargo) != s.a(EE_UNITS_UCB_MAX_HIT_POINTS_FIELD_OF, cargo) {
            continue;
        }
        let mut ready = false;
        if s.a(EE_UNITS_UCB_PLAYER_HUMAN_OF, cargo) != 0 {
            if s.ab(EE_UNITS_UCB_QUEUE_LEN_OF, cargo, 1) > 0 {
                let goal = s.ab(EE_UNITS_UCB_QUEUE_FRONT_OF, cargo, 1) as u64;
                ready = rally_goal(&mut s, goal);
            }
            if !ready && s.ab(EE_UNITS_UCB_QUEUE_LEN_OF, unit, 1) > 0 {
                let goal = s.ab(EE_UNITS_UCB_QUEUE_FRONT_OF, unit, 1) as u64;
                ready = rally_goal(&mut s, goal);
            }
        }
        if ready || (s.s(EE_UNITS_UCB_COMPUTER_PLAYER) != 0 && s.a(EE_UNITS_UCB_GOAL_STATE_OF, cargo) != 0) {
            let id = s.a(EE_UNITS_UCB_UNIT_ID_OF, cargo);
            s.b(EE_UNITS_UCB_SET_EXITING, id);
            result = true;
        }
    }
    result
}

/// A copy of a rally goal for `cargo`: an attack it suits on a living target cloned, else a move to the target's
/// tile; another goal it suits cloned, else a move to its location (`position` keeps it)
fn copy_goal<H: UcHost + ?Sized>(s: &mut S<'_, H>, goal: u64, cargo: u64, position: &mut [f32; 3]) -> u64 {
    let attack = s.a(EE_UNITS_UCB_ATTACK_CAST, goal) as u64;
    if attack != 0 {
        if s.ab(EE_UNITS_UC_GOAL_SUITS, attack, cargo as i64) != 0 && s.a(EE_UNITS_UC_ATTACK_TARGET, attack) != 0 && {
            let target = s.a(EE_UNITS_UC_ATTACK_TARGET, attack) as u64;
            s.a(EE_UNITS_UCB_UNIT_HIT_POINTS, target) > 0
        } {
            s.a(EE_UNITS_UC_GOAL_CLONE, attack) as u64
        } else {
            let target = s.a(EE_UNITS_UC_ATTACK_TARGET, attack) as u64;
            let x = s.a(EE_UNITS_UC_UNIT_TILE_X, target) as c_long as f32;
            let target = s.a(EE_UNITS_UC_ATTACK_TARGET, attack) as u64;
            let y = s.a(EE_UNITS_UC_UNIT_TILE_Y, target) as c_long as f32;
            let mut f = [x, y, 0.0, 0.0];
            s.f(EE_UNITS_UC_NEW_LOCATION_2D, 0, 0, 0, &mut f) as u64
        }
    } else if s.ab(EE_UNITS_UC_GOAL_SUITS, goal, cargo as i64) != 0 {
        s.a(EE_UNITS_UC_GOAL_CLONE, goal) as u64
    } else {
        s.point(EE_UNITS_UC_GOAL_LOCATION, goal, 0, 0, position);
        let mut p = *position;
        s.point(EE_UNITS_UC_NEW_LOCATION, 0, 0, 0, &mut p) as u64
    }
}

/// A paratrooper plane's drop at `target`: an unload where the tile is free, else a move there
fn drop_goal<H: UcHost + ?Sized>(s: &mut S<'_, H>, cargo: u64, target: [f32; 3], last: bool) -> u64 {
    if last
        && s.abc(
            EE_UNITS_UCB_TILE_FREE,
            cargo,
            i64::from(floor_unsigned(target[0])),
            i64::from(floor_unsigned(target[1])),
        ) != 0
    {
        s.abc(EE_UNITS_UCB_NEW_UNLOAD_TRANSPORT, cargo, target[0] as c_long as i64, target[1] as c_long as i64) as u64
    } else {
        let mut p = target;
        s.point(EE_UNITS_UC_NEW_LOCATION, 0, 0, 0, &mut p) as u64
    }
}

/// `ReleaseUnit(unit)`: the exiting unit placed at the exit (runway, water or tag point), given the building's
/// rally goals or its own (else loaded back) and sent off from this home base.
pub fn release_unit<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = st(h, class, unit);
    let mut position = [0.0f32; 3];
    let mut t = GeTransformation::IDENTITY;
    s.ab(EE_UNITS_UC_LOCATION_MATRIX, unit, addr(&mut t.m) as i64);
    let mut queue = 0i64;
    let cargo_id = s.s(EE_UNITS_UCB_EXITING_ID) as u32;
    if cargo_id == u32::MAX {
        return;
    }
    let water = s.s(EE_UNITS_UCB_AIR_TRAFFIC_SELF) == 3;
    let size = s.s(EE_UNITS_UCB_TILE_SIZE_FIELD) as c_long as f32;
    position[0] = s.s(EE_UNITS_UC_PRODUCER_TILE_X) as c_long as f32 + size * 0.5;
    position[1] = s.s(EE_UNITS_UC_PRODUCER_TILE_Y) as c_long as f32 + size * 0.5;
    position[2] = if water { 0.0 } else { s.float(EE_UNITS_UC_TILE_HEIGHT, 0, 0) };
    let yaw;
    if s.s(EE_UNITS_UCB_AIR_TRAFFIC_SELF) == 1 || water {
        yaw = FACING_YAWS[(s.s(EE_UNITS_UCB_FACING) as u32).min(3) as usize];
        if water {
            position[0] -= 2.0 * um_cos(HALF_PI - yaw);
            position[1] += 2.0 * um_cos(yaw);
        }
    } else {
        yaw = s.float(EE_UNITS_UCB_LOCATION_YAW, 0, 0);
        let points = s.s(EE_UNITS_UC_TAG_LIST) as u64;
        let cargo = s.b(EE_UNITS_UCB_FIND_PASSENGER, i64::from(cargo_id)) as u64;
        if cargo != 0 {
            if points != 0 && s.a(EE_UNITS_UCB_TAGS_EMPTY, points) == 0 {
                s.ab(EE_UNITS_UCB_TAGS_SCALE_STATS, points, addr(&mut t.m) as i64);
                s.point(EE_UNITS_UCB_TAG0_POINT, points, cargo as i64, addr(&mut t.m) as i64, &mut position);
            }
            s.with_float(EE_UNITS_UCB_SET_CARGO_SCALE, cargo, position[2]);
        }
    }
    t.m[0][3] = position[0];
    t.m[1][3] = position[1];
    t.m[2][3] = position[2];
    t.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
    let cargo = s.ab(EE_UNITS_UCB_UNLOAD_BY_ID, addr(&mut t.m), i64::from(cargo_id)) as u64;
    let mut f = [yaw, 0.0, 0.0, 0.0];
    s.f(EE_UNITS_UC_SET_ORIENTATION, cargo, 0, 0, &mut f);
    s.a(EE_UNITS_UC_SET_PLANET, cargo);
    let category = s.s(EE_UNITS_UCB_CATEGORY_SELF);
    let carrier = (category == CATEGORY_NAVAL_BUILDING || category == CATEGORY_SPACE_BUILDING)
        && s.s(EE_UNITS_UCB_AIR_TRAFFIC_SELF) != 0;
    let mut goal = 0u64;
    if s.ab(EE_UNITS_UCB_QUEUE_LEN_OF, cargo, 1) != 0 {
        let mut i = 0i64;
        while i < s.ab(EE_UNITS_UCB_QUEUE_LEN_OF, cargo, 1) {
            let queued = s.abc(EE_UNITS_UCB_QUEUE_GOAL_OF, cargo, 1, i) as u64;
            let copy = copy_goal(&mut s, queued, cargo, &mut position);
            s.ab(EE_UNITS_UC_QUEUE_GOAL_ON, cargo, copy as i64);
            i += 1;
        }
    } else {
        if carrier {
            queue = 1;
        } else if s.s(EE_UNITS_UCB_UNIT_TYPE_SELF) == UNIT_TYPE_NAVY_YARD {
            queue = 2;
        } else {
            let cargo_category = s.a(EE_UNITS_UCB_CATEGORY_OF, cargo);
            if cargo_category == CATEGORY_AIRPLANE || cargo_category == CATEGORY_PLANET_SPACESHIP {
                let family = s.a(EE_UNITS_UCB_FAMILY_OF, cargo);
                queue = if family == FAMILY_BOMBER {
                    family_queue(family, s.a(EE_UNITS_UCB_PARATROOPER_OF, cargo) != 0)
                } else {
                    family_queue(family, false)
                };
            }
        }
        if queue != 0 {
            goal = s.b(EE_UNITS_UC_QUEUED_GOAL, queue) as u64;
        }
        if s.a(EE_UNITS_UCB_PLAYER_HUMAN_OF, cargo) == 0 || goal == 0 {
            if s.a(EE_UNITS_UCB_GOAL_OF, cargo) != 0 {
                goal = s.a(EE_UNITS_UCB_GOAL_OF, cargo) as u64;
            }
            if goal == 0 {
                s.a(EE_UNITS_UCB_LOAD_CARGO, cargo);
                s.a(EE_UNITS_UCB_ATC_REMOVE_TAKEOFF, cargo);
                s.b(EE_UNITS_UCB_SET_EXITING, -1);
                return;
            }
        }
        if s.a(EE_UNITS_UCB_DETACHED, cargo) == 0
            && (s.a(EE_UNITS_UCB_PLAYER_HUMAN_OF, cargo) != 0 || s.a(EE_UNITS_UCB_GOAL_OF, cargo) == 0)
        {
            s.a(EE_UNITS_UC_CLEAR_GOAL_QUEUE, cargo);
            let mut i = 0i64;
            while i < s.b(EE_UNITS_UCB_UNIT_QUEUE_LEN, queue) {
                let rally = s.abc(EE_UNITS_UCB_UNIT_QUEUE_GOAL, 0, queue, i) as u64;
                if queue == 4 {
                    if rally != 0 {
                        let mut target = [0.0f32; 3];
                        let last = i == s.b(EE_UNITS_UCB_UNIT_QUEUE_LEN, 4) - 1;
                        s.point(EE_UNITS_UC_GOAL_LOCATION, rally, 0, 0, &mut target);
                        goal = drop_goal(&mut s, cargo, target, last);
                    }
                } else {
                    goal = copy_goal(&mut s, rally, cargo, &mut position);
                }
                s.ab(EE_UNITS_UC_QUEUE_GOAL_ON, cargo, goal as i64);
                i += 1;
            }
        } else if s.a(EE_UNITS_UCB_PLAYER_HUMAN_OF, cargo) == 0
            && s.a(EE_UNITS_UCB_UNIT_TYPE_OF, cargo) == UNIT_TYPE_PARATROOPER_PLANE
            && s.a(EE_UNITS_UCB_GOAL_OF, cargo) != 0
        {
            let mut target = [0.0f32; 3];
            let own = s.a(EE_UNITS_UCB_GOAL_OF, cargo) as u64;
            s.point(EE_UNITS_UC_GOAL_LOCATION, own, 0, 0, &mut target);
            let copy = drop_goal(&mut s, cargo, target, true);
            s.ab(EE_UNITS_UC_QUEUE_GOAL_ON, cargo, copy as i64);
        }
    }
    s.a(EE_UNITS_UCB_SET_HOME_BASE, cargo);
    s.ab(EE_UNITS_UC_START_ACTION, cargo, ACTION_TAKEOFF);
    s.a(EE_UNITS_UC_ACTION_CHANGED, cargo);
    s.b(EE_UNITS_UCB_SET_EXITING, -1);
}

/// `BuildingUnitCreated(unit)`: a new building set up (built, its resource stock, regeneration); an airport turned
/// to its map quarter, a navy yard's facing, a space dock's yaw.
pub fn unit_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = st(h, class, unit);
    s.abc(EE_UNITS_UCB_SET_BUILT, 0, 0, 1 | (1 << 1));
    crate::units::uc_create::base_unit_created(s.h, class, unit);
    s.s(EE_UNITS_UCB_SET_RESOURCE_TYPE);
    if s.s(EE_UNITS_UCB_RESOURCE_TYPE) != RESOURCE_NONE {
        s.s(EE_UNITS_UCB_SET_RESOURCE_AMOUNT);
    }
    s.b(EE_UNITS_UC_SET_REGENERATION_TIMER, 1000);
    let unit_type = s.s(EE_UNITS_UC_THIS_UNIT_TYPE);
    if unit_type == UNIT_TYPE_AIRPORT {
        let mut t = GeTransformation::IDENTITY;
        s.ab(EE_UNITS_UC_LOCATION_MATRIX, unit, addr(&mut t.m) as i64);
        let quarter = s.s(EE_UNITS_UCB_FACING_QUARTER);
        s.b(EE_UNITS_UCB_SET_FACING, quarter);
        let yaw = FACING_YAWS[quarter as usize];
        t.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
        s.ab(EE_UNITS_UC_SET_PHYSICAL_LOCATION, unit, addr(&mut t.m) as i64);
    } else if unit_type == UNIT_TYPE_NAVY_YARD {
        let facing = s.s(EE_UNITS_UCB_NAVY_YARD_FACING);
        s.b(EE_UNITS_UCB_SET_FACING, facing);
    } else if unit_type == UNIT_TYPE_SPACE_DOCK {
        let mut t = GeTransformation::IDENTITY;
        s.ab(EE_UNITS_UC_LOCATION_MATRIX, unit, addr(&mut t.m) as i64);
        let yaw = s.float(EE_UNITS_UCB_SPACE_DOCK_YAW, 0, 0);
        t.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
        s.ab(EE_UNITS_UC_SET_PHYSICAL_LOCATION, unit, addr(&mut t.m) as i64);
    }
}

/// The attack frame's fire for the attacking commands (`track`: the tracking one too)
fn is_firing_command(command: i64, track: bool) -> bool {
    command == ACTION_ATTACK_ENEMY_UNIT
        || command == ACTION_ATTACK_LOCATION
        || command == ACTION_WAIT_FOR_RELOAD
        || (track && command == ACTION_TRACK_ENEMY_UNIT)
}

/// `UpdateArmed(unit)`: an armed building's tick (physics, turrets, timers, animation, sound), its fire on the
/// attack frame; disabled ones and standing foundations skipped.
pub fn update_armed<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = st(h, class, unit);
    let frame = s.s(EE_UNITS_UC_FRAME);
    if s.s(EE_UNITS_UCB_DISABLED) != 0 && s.s(EE_UNITS_UC_HIT_POINTS) > 0 {
        return;
    }
    if s.s(EE_UNITS_UC_IS_FOUNDATION) != 0 && s.s(EE_UNITS_UC_HIT_POINTS) > 0 && s.s(EE_UNITS_UCB_PHYSICS_ACTION) != 2 {
        return;
    }
    if s.s(EE_UNITS_UC_HAS_PHYSICS_MODEL) != 0 && s.s(EE_UNITS_UC_TILE_X) as c_long != -10000 {
        s.s(EE_UNITS_UC_PHYSICS_STEP);
        s.s(EE_UNITS_UCB_UPDATE_TURRETS);
    }
    s.s(EE_UNITS_UC_UPDATE_ORBITALS);
    s.s(EE_UNITS_UC_UPDATE_CAMOUFLAGE_TIMER);
    s.s(EE_UNITS_UC_UPDATE_RELOAD_TIMER);
    s.s(EE_UNITS_UC_UPDATE_TARGET_SEARCH_TIMER);
    let attack = crate::units::uc_anim::animate(s.h, class, unit);
    crate::units::uc_body::refresh_sound(s.h, class, unit);
    let command = s.s(EE_UNITS_UC_COMMAND_KIND);
    if is_firing_command(command, true) {
        crate::units::uc_anim::fire_on_attack_frame(s.h, class, unit, attack, frame);
    } else if command == ACTION_UNIT_ROTTING {
        s.s(EE_UNITS_UC_UPDATE_ROTTING);
    }
}

/// `PriestVirtualB8(unit)`: the temple's tick (its physics, effects, timers, animation, healing, regeneration,
/// power), its fire on the attack frame or its conversion; the conversion's effects hidden otherwise.
pub fn priest_update<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = st(h, class, unit);
    let frame = s.s(EE_UNITS_UC_FRAME);
    s.s(EE_UNITS_UC_KEEP_LAST_HIT_POINTS);
    if s.s(EE_UNITS_UC_HAS_PHYSICS_MODEL) != 0 && s.s(EE_UNITS_UCB_TILE_X_FIELD) as c_long != -10000 {
        s.s(EE_UNITS_UC_PHYSICS_STEP);
        s.s(EE_UNITS_UCB_UPDATE_TURRETS);
        s.s(EE_UNITS_UC_UPDATE_ORBITALS);
    }
    s.s(EE_UNITS_UC_UPDATE_EFFECTS);
    s.s(EE_UNITS_UC_UPDATE_RELOAD_TIMER);
    s.s(EE_UNITS_UC_UPDATE_TARGET_SEARCH_TIMER);
    s.s(EE_UNITS_UC_UPDATE_CAMOUFLAGE_TIMER);
    let attack = crate::units::uc_anim::animate(s.h, class, unit);
    crate::units::uc_body::refresh_sound(s.h, class, unit);
    if s.s(EE_UNITS_UCB_IS_MEDIC) != 0 {
        s.s(EE_UNITS_UCB_UPDATE_HEAL_SEARCH);
        let goal = s.s(EE_UNITS_UC_GOAL_STATE_SELF) as u64;
        if goal != 0 && s.a(EE_UNITS_UC_GOAL_KIND, goal) == GOAL_HEAL_UNIT {
            s.s(EE_UNITS_UCB_GOAL_WORK);
        }
    }
    if s.s(EE_UNITS_UCB_REGENERATION) > 0 {
        crate::units::uc_anim::regenerate(s.h, class, unit);
    }
    s.s(EE_UNITS_UC_UPDATE_DONE);
    s.s(EE_UNITS_UCB_REGENERATE_POWER);
    let command = s.s(EE_UNITS_UC_COMMAND_KIND);
    if is_firing_command(command, false) {
        crate::units::uc_anim::fire_on_attack_frame(s.h, class, unit, attack, frame);
    } else if command == ACTION_CONVERT_ENEMY_UNIT {
        s.s(EE_UNITS_UCB_PRIEST_CONVERSION);
        return;
    } else if command == ACTION_RESOURCES_ROTTING {
        return;
    } else if command == ACTION_UNIT_ROTTING {
        s.s(EE_UNITS_UC_UPDATE_ROTTING);
    }
    if s.s(EE_UNITS_UCB_CONVERSION_START) != 0 {
        s.s(EE_UNITS_UCB_CLEAR_CONVERSION_START);
    }
    s.s(EE_UNITS_UCB_RELEASE_CONVERSION);
    s.s(EE_UNITS_UCB_HIDE_CONVERSION);
}

/// `BaseCreateUnitAt(t, player, foundation, creator)` through the host
fn base_create_at<H: UcHost + ?Sized>(
    s: &mut S<'_, H>,
    t: &mut GeTransformation,
    player: i64,
    foundation: bool,
    creator: u64,
) -> u64 {
    crate::units::uc_create::base_create_unit_at(s.h, s.class, t.m, player, foundation, creator)
}

/// `BuildingCreateUnitAt(t, ...)`: docks and navy yards on the water plane, others (not the camera object) lifted
/// just above it.
pub fn create_unit_at<H: UcHost + ?Sized>(
    h: &mut H,
    class: u64,
    m: [[f32; 4]; 4],
    player: i64,
    foundation: bool,
    creator: u64,
) -> u64 {
    let mut s = st(h, class, 0);
    let mut t = GeTransformation { m };
    let unit_type = s.s(EE_UNITS_UC_THIS_UNIT_TYPE);
    if unit_type == UNIT_TYPE_DOCK || unit_type == UNIT_TYPE_NAVY_YARD {
        t.m[2][3] = 0.0;
    } else if unit_type != UNIT_TYPE_CAMERA_OBJECT {
        t.m[2][3] += BUILDING_LIFT;
    }
    base_create_at(&mut s, &mut t, player, foundation, creator)
}

/// `BuildingCreateUnit(x, y, ...)`: a building on a tile's centre (docks and navy yards at water level, off-map
/// tiles at the -10000 sentinel).
pub fn create_unit<H: UcHost + ?Sized>(
    h: &mut H,
    class: u64,
    x: i64,
    y: i64,
    player: i64,
    foundation: bool,
    creator: u64,
) -> u64 {
    let mut s = st(h, class, 0);
    let mut t = GeTransformation::IDENTITY;
    let unit_type = s.s(EE_UNITS_UC_THIS_UNIT_TYPE);
    let cx = x as c_long as f32 + 0.5;
    let cy = y as c_long as f32 + 0.5;
    if unit_type == UNIT_TYPE_DOCK || unit_type == UNIT_TYPE_NAVY_YARD {
        t.m[0][3] = cx;
        t.m[1][3] = cy;
        t.m[2][3] = 0.0;
    } else if s.abc(EE_UNITS_UC_GET_TILE, 0, x, y) != 0 {
        let mut ground = [cx, cy, 0.0f32];
        let map = s.s(EE_UNITS_UC_WORLD_MAP) as u64;
        s.point(EE_UNITS_UC_GROUND_POINT, map, 0, 0, &mut ground);
        ground[2] += BUILDING_LIFT;
        t.m[0][3] = ground[0];
        t.m[1][3] = ground[1];
        t.m[2][3] = ground[2];
    } else {
        t.m[0][3] = -10000.0;
        t.m[1][3] = -10000.0;
    }
    base_create_at(&mut s, &mut t, player, foundation, creator)
}

/// `MiningBuildingCreateUnitAt(t, ...)`: a mine (a building) tied to the first resource under its footprint it can
/// gather.
pub fn mining_create_unit_at<H: UcHost + ?Sized>(
    h: &mut H,
    class: u64,
    m: [[f32; 4]; 4],
    player: i64,
    foundation: bool,
    creator: u64,
) -> u64 {
    let x = i64::from(floor(m[0][3]));
    let y = i64::from(floor(m[1][3]));
    let size = h.uc(EE_UNITS_UCB_TILE_SIZE_FIELD, class, 0, 0, 0, 0, &mut [0.0; 4]);
    let mine = create_unit_at(h, class, m, player, foundation, creator);
    let mut s = st(h, class, 0);
    let mut found = false;
    let mut i = x;
    while i < x + size && !found {
        let mut j = y;
        while j < y + size && !found {
            let tile = s.abc(EE_UNITS_UC_GET_TILE, 0, i, j) as u64;
            j += 1;
            if tile == 0 {
                continue;
            }
            let mut node = s.a(EE_UNITS_UCB_TILE_UNITS, tile) as u64;
            while node != 0 && !found {
                let other = s.a(EE_UNITS_UCB_NODE_UNIT, node) as u64;
                if other != 0 && s.ab(EE_UNITS_UCB_CAN_GATHER, mine, other as i64) != 0 {
                    s.ab(EE_UNITS_UCB_SET_MINE, mine, other as i64);
                    found = true;
                }
                node = s.a(EE_UNITS_UCB_NODE_NEXT, node) as u64;
            }
        }
        i += 1;
    }
    mine
}

/// `MiningBuildingCreateUnit(x, y, ...)`: a mine on a tile, at the tile's height (0 off the map).
pub fn mining_create_unit<H: UcHost + ?Sized>(
    h: &mut H,
    class: u64,
    x: i64,
    y: i64,
    player: i64,
    foundation: bool,
    creator: u64,
) -> u64 {
    let mut t = GeTransformation::IDENTITY;
    let tile = h.uc(EE_UNITS_UC_GET_TILE, class, 0, 0, x, y, &mut [0.0; 4]) as u64;
    t.m[0][3] = x as c_long as f32;
    t.m[1][3] = y as c_long as f32;
    t.m[2][3] = if tile != 0 {
        let mut f = [0.0f32; 4];
        h.uc(EE_UNITS_UC_TILE_HEIGHT_OF, class, 0, tile, 0, 0, &mut f);
        f[0]
    } else {
        0.0
    };
    mining_create_unit_at(h, class, t.m, player, foundation, creator)
}

/// `SpaceBuildingCreateUnitAt(t, ...)` / `SpaceBuildingCreateUnit(x, y, ...)`: a space building at the cruise
/// altitude.
pub fn space_create_unit<H: UcHost + ?Sized>(
    h: &mut H,
    class: u64,
    t: &mut GeTransformation,
    player: i64,
    foundation: bool,
    creator: u64,
) -> u64 {
    let mut s = st(h, class, 0);
    t.m[2][3] = s.float(EE_UNITS_UCB_CRUISE_ALTITUDE, 0, 0);
    base_create_at(&mut s, t, player, foundation, creator)
}

/// `BuildingGetSurroundingTiles(unit, tiles, corners, append)`: the tiles around a building; an airport's include
/// its runway (four times its size toward its facing).
pub fn surrounding_tiles<H: UcHost + UcbRust + ?Sized>(
    h: &mut H,
    class: u64,
    unit: u64,
    tiles: u64,
    corners: i64,
    append: bool,
) {
    let mut s = st(h, class, unit);
    let (mut top, mut left, mut bottom, mut right) = (0.0f32, 0.0f32, 0.0f32, 0.0f32);
    if !append {
        s.a(EE_UNITS_UCB_TILES_CLEAR, tiles);
    }
    if s.s(EE_UNITS_UC_THIS_UNIT_TYPE) != UNIT_TYPE_AIRPORT {
        s.abc(EE_UNITS_UCB_BASE_SURROUNDING, tiles, corners, i64::from(append));
        return;
    }
    let mut first = s.a(EE_UNITS_UCB_COMPLEX_FIELD_OF, unit) as u64;
    loop {
        let previous = s.a(EE_UNITS_UCB_PREVIOUS_PART_OF, first) as u64;
        if previous == 0 {
            break;
        }
        first = previous;
    }
    let size = s.a(EE_UNITS_UCB_TILE_SIZE_OF, first) as c_long;
    let y = s.a(EE_UNITS_UC_UNIT_TILE_Y, first) as c_long;
    let x = s.a(EE_UNITS_UC_UNIT_TILE_X, first) as c_long;
    match s.abc(EE_UNITS_UCB_FACING_QUARTER_AT, 0, x as i64, y as i64) {
        0 => {
            left = x as f32;
            right = x.wrapping_add(size) as f32;
            bottom = y as f32;
            top = y.wrapping_add(size.wrapping_mul(4)) as f32;
        }
        1 => {
            left = x as f32;
            right = x.wrapping_add(size) as f32;
            bottom = y.wrapping_add(size.wrapping_mul(-3)) as f32;
            top = y.wrapping_add(size) as f32;
        }
        2 => {
            left = x as f32;
            right = x.wrapping_add(size.wrapping_mul(4)) as f32;
            bottom = y as f32;
            top = y.wrapping_add(size) as f32;
        }
        3 => {
            left = x.wrapping_add(size.wrapping_mul(-3)) as f32;
            right = x.wrapping_add(size) as f32;
            bottom = y as f32;
            top = y.wrapping_add(size) as f32;
        }
        _ => {}
    }
    let ring = [floor(left), floor(bottom), floor(right - left), floor(bottom - top).wrapping_neg()];
    s.h.block_ring(corners as i32, ring, tiles, append);
}

/// `EEGetWallPiecePart(index, flag)`: the model part of a wall piece kind (0..8) and whether it is a corner (0, 2,
/// 6, 8).
pub fn wall_piece_part(index: i64) -> (i64, bool) {
    let corner = index == 0 || index == 2 || index == 6 || index == 8;
    let part = match index {
        0 | 3 => 3,
        1 | 2 => 0,
        5 | 8 => 2,
        _ => 1,
    };
    (part, corner)
}

/// Runs body `what` of `class` on `unit` (`a`, `b`, `c` its arguments, `m` its transformation).
#[allow(clippy::too_many_arguments)]
pub fn run<H: UcHost + UcbRust + ?Sized>(
    h: &mut H,
    what: i32,
    class: u64,
    unit: u64,
    a: u64,
    b: i64,
    c: i64,
    m: Option<[[f32; 4]; 4]>,
) -> i64 {
    let m = m.unwrap_or(GeTransformation::IDENTITY.m);
    let tile = |b: i64| ((b as i32) as i64, ((b >> 32) as i32) as i64);
    match what {
        EE_UNITS_UCB_DO_SELECT_TO_RELEASE => i64::from(select_to_release(h, class, unit)),
        EE_UNITS_UCB_DO_NAVAL_SELECT_TO_RELEASE => i64::from(naval_select_to_release(h, class, unit)),
        EE_UNITS_UCB_DO_RELEASE_UNIT => {
            release_unit(h, class, unit);
            0
        }
        EE_UNITS_UCB_DO_UNIT_CREATED => {
            unit_created(h, class, unit);
            0
        }
        EE_UNITS_UCB_DO_UPDATE_ARMED => {
            update_armed(h, class, unit);
            0
        }
        EE_UNITS_UCB_DO_PRIEST_UPDATE => {
            priest_update(h, class, unit);
            0
        }
        EE_UNITS_UCB_DO_CREATE_UNIT_AT => create_unit_at(h, class, m, b, c != 0, a) as i64,
        EE_UNITS_UCB_DO_CREATE_UNIT => {
            let (x, y) = tile(b);
            create_unit(h, class, x, y, c >> 1, c & 1 != 0, a) as i64
        }
        EE_UNITS_UCB_DO_MINING_CREATE_UNIT_AT => mining_create_unit_at(h, class, m, b, c != 0, a) as i64,
        EE_UNITS_UCB_DO_MINING_CREATE_UNIT => {
            let (x, y) = tile(b);
            mining_create_unit(h, class, x, y, c >> 1, c & 1 != 0, a) as i64
        }
        EE_UNITS_UCB_DO_SPACE_CREATE_UNIT_AT => {
            let mut t = GeTransformation { m };
            space_create_unit(h, class, &mut t, b, c != 0, a) as i64
        }
        EE_UNITS_UCB_DO_SPACE_CREATE_UNIT => {
            let (x, y) = tile(b);
            let mut t = GeTransformation::IDENTITY;
            t.m[0][3] = x as c_long as f32;
            t.m[1][3] = y as c_long as f32;
            space_create_unit(h, class, &mut t, c >> 1, c & 1 != 0, a) as i64
        }
        EE_UNITS_UCB_DO_SURROUNDING_TILES => {
            surrounding_tiles(h, class, unit, a, b, c != 0);
            0
        }
        _ => 0,
    }
}

#[cfg(not(test))]
pub(crate) mod game {
    use super::*;
    use std::os::raw::c_void;

    extern "C" {
        /// the buildings' host (EEUnitClassBuildings.cpp; ucb_gate.rs's too)
        pub(crate) fn ee_units_ucb_host(
            op: i32,
            class: *mut c_void,
            unit: *mut c_void,
            a: u64,
            b: i64,
            c: i64,
            f: *mut [f32; 4],
        ) -> i64;
    }

    /// The buildings' host (`ee_units_ucb_host`; the class host's statements pass through it).
    struct GameB;

    impl UcbRust for GameB {
        fn block_ring(&mut self, corners: i32, ring: [i32; 4], tiles: u64, append: bool) {
            // SAFETY: the list is the caller's live tile vector
            unsafe {
                crate::units::uu_ffi::ee_units_uu_block_ring(
                    corners,
                    ring[0],
                    ring[1],
                    ring[2],
                    ring[3],
                    tiles as usize as *mut c_void,
                    u8::from(append),
                    0,
                )
            }
        }
    }

    impl UcHost for GameB {
        #[allow(clippy::too_many_arguments)]
        fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            if let Some(v) = crate::units::tech_cases::ucb(op, unit, a, b) {
                return v;
            }
            // SAFETY: the body runs inside the C++ member that handed over its live class and unit
            unsafe { ee_units_ucb_host(op, class as usize as *mut c_void, unit as usize as *mut c_void, a, b, c, f) }
        }
    }

    /// The buildings' body `what` (`EE_UNITS_UCB_DO_*`) of `class` on `unit`: `a` an object, `b`, `c` values, `m`
    /// a transformation (or NULL); the created unit, the choice, or 0.
    ///
    /// # Safety
    /// `class` is a live `EEUnitClass`, `unit` a live `EEComplexUnit` (or NULL), `m` NULL or live, on the
    /// simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ucb_call(
        what: i32,
        class: *mut c_void,
        unit: *mut c_void,
        a: u64,
        b: i64,
        c: i64,
        m: *const [[f32; 4]; 4],
    ) -> i64 {
        // SAFETY: the caller's transformation
        let matrix = if m.is_null() { None } else { Some(unsafe { *m }) };
        run(&mut GameB, what, class as usize as u64, unit as usize as u64, a, b, c, matrix)
    }

    /// `EEGetWallPiecePart(index, flag)`.
    ///
    /// # Safety
    /// `flag` is a live bool.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ucb_wall_piece_part(index: i32, flag: *mut bool) -> i32 {
        let (part, corner) = wall_piece_part(i64::from(index));
        // SAFETY: the caller's flag
        unsafe { *flag = corner };
        part as i32
    }
}

#[cfg(test)]
#[path = "ucb_release_tests.rs"]
mod tests;
