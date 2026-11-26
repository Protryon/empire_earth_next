//! The goals' host's plain statements over the world views (the units' lane, round 13, second leg): each
//! `EE_UNITS_GOAL_HOST_*` statement of EEUGRust.cpp's `GoalOp` that reads or stores one member of a Rust record
//! (the unit's, its class's, its stats', its player's, the world's) or that called a unit or class member whose
//! inline body is one call of a Rust entry point, answered here before the C++ table is asked (the three places
//! that call it: rust_goal.rs `OpHost`, goal_split.rs `FfiSplit`, combat goal_objects.rs `TableHost`); their cases
//! left `GoalOp`. `a` and `b` are the operation's handles, `io` its block; the conversions are the C++ returns'.
#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::cu_body::game::ee_units_cu_body;
use crate::units::cu_body::*;
use crate::units::cu_cargo::*;
use crate::units::cu_life::*;
use crate::units::cu_misc::*;
use crate::units::cu_world::*;
use crate::units::goal_split::*;
use crate::units::rust_goal::*;
use crate::units::uc_view::{complex_mut, stats_links, word};
use crate::world::layout::layout;
use crate::world::unit_fields::EeUnitsUnitFields;
use crate::world::view::{ClassView, PlayerView, UnitView, WorldRead, WorldView};
use crate::world_field;
use std::os::raw::{c_long, c_ulong, c_void};

/// `kResourceAny`
const RESOURCE_ANY: u64 = 1;

/// The statement `op` of the goals' host when it is a plain one (None for the C++ table's).
pub(crate) fn goal_view(op: i32, a: u64, b: u64, io: &mut EeUnitsGoalIo) -> Option<i64> {
    // SAFETY: a goal body runs inside the C++ call that handed out the host, which holds the world; the handles are
    // the live objects the C++ dereferenced as such
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let u = || unsafe { UnitView::of(&t, a) };
    // SAFETY: as above
    let o = || unsafe { UnitView::of(&t, b) };
    let p = |h: u64| h as usize as *mut c_void;
    let i0 = io.i[0];
    Some(match op {
        EE_UNITS_GOAL_HOST_X_UNIT_PLAYER_ID => i64::from(u()?.player()?.id()),
        EE_UNITS_GOAL_HOST_X_CLASS_DOMAINS => i64::from(world_field!(u()?.class()?.fields(), mDomains)),
        EE_UNITS_GOAL_HOST_X_CLASS_UNIT_TYPE => u()?.class()?.unit_type() as i64,
        EE_UNITS_GOAL_HOST_X_CLASS_TABLE => {
            ClassView::by_index(&t, i0 as c_long as usize).map_or(0, |k| k.handle() as i64)
        }
        // SAFETY: a live class
        EE_UNITS_GOAL_HOST_X_CLASS_TECHNOLOGY => unsafe { ClassView::of(&t, a) }?.technology(),
        EE_UNITS_GOAL_HOST_X_UNIT_PLAYER => u()?.player().map_or(0, |q| q.handle() as i64),
        EE_UNITS_GOAL_HOST_X_PLAYER_HAS_TECH => {
            // SAFETY: a live player's pointer member
            i64::from(unsafe { word(PlayerView::of(&t, a)?.handle(), layout().player_technologies) } != 0)
        }
        EE_UNITS_GOAL_HOST_X_CURRENT_PLAYER => {
            let w = WorldView::global(&t)?;
            w.player(w.current_player_index() as usize).map_or(0, |q| q.handle() as i64)
        }
        EE_UNITS_GOAL_HOST_X_CREATION_SOUND => {
            world_field!(stats_links(&u()?.stats()?), mCreationSound) as usize as i64
        }
        EE_UNITS_GOAL_HOST_X_NEXT_PART => u()?.next_part_handle() as i64,
        EE_UNITS_GOAL_HOST_X_COMPUTER_PLAYER => {
            // SAFETY: the unit's player's pointer member
            unsafe { word(u()?.player()?.handle(), layout().player_computer_player) as i64 }
        }
        EE_UNITS_GOAL_HOST_X_GATHER_TIME => {
            let times: [c_long; 7] = world_field!(u()?.stats()?.fields(), mGatherTimes);
            *times.get(i0 as usize)? as i64
        }
        EE_UNITS_GOAL_HOST_X_SET_WORK_TIMER => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mWorkTimer = i0 as c_long };
            0
        }
        EE_UNITS_GOAL_HOST_X_ADD_RESOURCE_AMOUNT => {
            let f = u()?.fields().ptr() as *mut EeUnitsUnitFields;
            // SAFETY: the unit's member
            unsafe { (*f).mResourceAmount += 1 };
            0
        }
        EE_UNITS_GOAL_HOST_HAS_CARGO => {
            let u = u()?;
            i64::from((0..u.cargo_len()).any(|i| u.cargo_handle(i) == b))
        }
        EE_UNITS_GOAL_HOST_X_CAN_TAKE => {
            let kind = o()?.resource_type();
            i64::from(kind == RESOURCE_ANY || kind == i0 as c_ulong as u64)
        }
        // the one-call members
        EE_UNITS_GOAL_HOST_X_START_ACTION => {
            let action = i0 as c_ulong as c_long as i64;
            // SAFETY: the member's call
            unsafe { ee_units_cu_body(EE_UNITS_CU_DO_START_ACTION, p(a), action, 0, 0) };
            0
        }
        EE_UNITS_GOAL_HOST_DROP_PATH => {
            // SAFETY: as above
            unsafe { crate::units::waypoints::ee_units_waypoints_drop(a) };
            0
        }
        EE_UNITS_GOAL_HOST_FIRE_CARGO => {
            let class = u()?.class()?.handle();
            crate::units::ucn_noted::ee_units_ucn_fire_shot(class, a);
            0
        }
        EE_UNITS_GOAL_HOST_ACTION_CHANGED => {
            let class = u()?.class()?.handle();
            let w = crate::units::uck_kinds::EE_UNITS_UCK_DO_ACTION_CHANGED;
            // SAFETY: as above
            unsafe { crate::units::uck_kinds::game::ee_units_uck_call(w, p(class), p(a), 0, 0, 0, 0.0) };
            0
        }
        // SAFETY: as above (ClearGoals' `(long)inB`)
        EE_UNITS_GOAL_HOST_CLEAR_GOALS => {
            done(unsafe { ee_units_cu_body(EE_UNITS_CU_DO_CLEAR_GOALS, p(a), long(b as i64), 0, 0) })
        }
        // SAFETY: as above (LoadCargo returns a long)
        EE_UNITS_GOAL_HOST_LOAD_CARGO => {
            long(unsafe { ee_units_cu_body(EE_UNITS_CU_DO_LOAD_CARGO, p(a), b as i64, 0, 0) })
        }
        EE_UNITS_GOAL_HOST_GARRISON_UNIT => {
            // SAFETY: as above
            i64::from(unsafe { ee_units_cu_body(EE_UNITS_CU_DO_GARRISON_UNIT, p(a), b as i64, 0, 0) } != 0)
        }
        // SAFETY: as above
        EE_UNITS_GOAL_HOST_X_SET_BUILT => done(unsafe { ee_units_cu_body(EE_UNITS_CU_DO_SET_BUILT, p(a), 1, 1, 1) }),
        // SAFETY: as above
        EE_UNITS_GOAL_HOST_PLAY_EFFECT => {
            done(unsafe { ee_units_cu_body(EE_UNITS_CU_DO_PLAY_EFFECT, p(a), long(i0), 0, 0) })
        }
        // SAFETY: as above
        EE_UNITS_GOAL_HOST_X_KILL => done(unsafe { ee_units_cu_body(EE_UNITS_CU_DO_KILL, p(a), 0, 0, 0) }),
        EE_UNITS_GOAL_HOST_X_FULL_HIT_POINTS => {
            let max = u()?.stats()?.max_hit_points();
            set_hit_points(a, max)
        }
        EE_UNITS_GOAL_HOST_SET_HIT_POINTS => set_hit_points(a, long(i0)),
        EE_UNITS_GOAL_HOST_FACE_TARGET => {
            let w = crate::units::cu_more::EE_UNITS_CUM_DO_FACE_TARGET;
            let n = std::ptr::null_mut();
            // SAFETY: as above
            unsafe { crate::units::cu_more::game::ee_units_cu_more(w, p(a), b as i64, 0, 0, 0.0, 0.0, 0.0, n, n) };
            0
        }
        EE_UNITS_GOAL_HOST_X_PART_FINISHED => {
            let w = crate::units::cu_small::EE_UNITS_CU_S_DO_PART_FINISHED;
            // SAFETY: as above
            unsafe { crate::units::cu_small::game::ee_units_cu_small(w, p(a), std::ptr::null_mut(), 0) };
            0
        }
        EE_UNITS_GOAL_HOST_SET_GATHERED => {
            // SAFETY: as above
            done(unsafe { ee_units_cu_body(EE_UNITS_CU_DO_SET_GATHERED_RESOURCE, p(a), b as i64, 0, 0) })
        }
        EE_UNITS_GOAL_HOST_RELEASE_FIRE => {
            // SAFETY: as above
            done(unsafe { ee_units_cu_body(EE_UNITS_CU_DO_RELEASE_FIRE_EFFECT, p(a), 0, 0, 0) })
        }
        EE_UNITS_GOAL_HOST_X_SET_LAST_RESOURCE => {
            // SAFETY: as above
            done(unsafe { ee_units_cu_body(EE_UNITS_CU_DO_SET_LAST_RESOURCE, p(a), b as i64, 0, 0) })
        }
        EE_UNITS_GOAL_HOST_X_DEPLETE_ONE => {
            let w = crate::units::eu_body::EE_UNITS_EU_DO_DEPLETE_RESOURCE;
            // SAFETY: as above
            done(unsafe { crate::units::eu_body::game::ee_units_eu_body(w, p(a), 1, 0, 0) })
        }
        _ => return None,
    })
}

fn long(v: i64) -> i64 {
    v as c_long as i64
}

/// A statement's answer: 0 (the member returned nothing).
fn done(_: i64) -> i64 {
    0
}

/// `SetHitPoints(value)` of the unit `unit`.
fn set_hit_points(unit: u64, value: i64) -> i64 {
    let w = crate::units::eu_body::EE_UNITS_EU_DO_SET_HIT_POINTS;
    // SAFETY: the member's call on the live unit
    unsafe { crate::units::eu_body::game::ee_units_eu_body(w, unit as usize as *mut c_void, value, 0, 0) };
    0
}

/// `ee_units_gt_host`'s plain statements (the tree gathering's: `a` a unit or a tile node, `b` a unit; EEUGRust.cpp).
pub(crate) fn gt_view(op: i32, a: u64, b: u64, c: i64, d: i64) -> Option<i64> {
    use crate::units::uu_gather_tree::*;
    // SAFETY: the gathering runs inside the C++ caller's hold of the world; the handles are live
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let u = || unsafe { UnitView::of(&t, a) };
    Some(match op {
        EE_UNITS_GT_RESOURCE_AMOUNT => u()?.resource_amount(),
        EE_UNITS_GT_RESOURCE_TYPE => u()?.resource_type() as i64,
        EE_UNITS_GT_CLEAR_RESOURCE_AMOUNT => {
            let f = u()?.fields().ptr() as *mut EeUnitsUnitFields;
            // SAFETY: the unit's member
            unsafe { (*f).mResourceAmount = 0 };
            0
        }
        EE_UNITS_GT_TILE_X => u()?.tile_xy().0,
        EE_UNITS_GT_TILE_Y => u()?.tile_xy().1,
        EE_UNITS_GT_LINE_OF_SIGHT => {
            let u = u()?;
            let sight: f32 = world_field!(u.stats()?.fields(), mLineOfSight);
            i64::from((sight + u.sight()).to_bits())
        }
        EE_UNITS_GT_PLAYER => u()?.player().map_or(0, |q| q.handle() as i64),
        // `CanTake(b, kResourceWood)` asks the unit's own resource type
        EE_UNITS_GT_CAN_TAKE => {
            let kind = u()?.resource_type();
            i64::from(kind == RESOURCE_ANY || kind == 3)
        }
        EE_UNITS_GT_MAP_TILE => {
            let map = WorldView::global(&t)?.map()?;
            map.tile(c as c_long as i64, d as c_long as i64).map_or(0, |k| k.handle() as i64)
        }
        // SAFETY: a live tile's node list
        EE_UNITS_GT_FIRST_NODE => unsafe { word(a, layout().tile_units) as i64 },
        // SAFETY: a live tile node's members
        EE_UNITS_GT_NODE_UNIT => unsafe { word(a, layout().unit_node_unit) as i64 },
        // SAFETY: as above
        EE_UNITS_GT_NEXT_NODE => unsafe { word(a, layout().unit_node_next) as i64 },
        EE_UNITS_GT_SET_GATHERED => {
            // SAFETY: the member's call on the live unit
            done(unsafe {
                ee_units_cu_body(EE_UNITS_CU_DO_SET_GATHERED_RESOURCE, a as usize as *mut c_void, b as i64, 0, 0)
            })
        }
        EE_UNITS_GT_FAMILY => u()?.class()?.family() as i64,
        _ => return None,
    })
}
