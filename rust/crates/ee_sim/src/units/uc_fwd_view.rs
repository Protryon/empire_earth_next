//! The class bodies' hosts' statements that were one call of a unit or class member whose inline body (ee/EEUnit.h)
//! is one call of a Rust entry point (the units' lane, round 13): the entry point is called here with the arguments
//! the member passed on, before the C++ host is asked; their cases left the hosts' switches. As the members: a
//! `long` truncated, an `unsigned long` zero-extended, a bool 0 or 1, the base records' addresses for the
//! record-taking entry points (`SetVelocity`, `GetNumGoalQueues`, `CanMoveOnTerrain`), the transformation's x and y
//! (its last column) for `SetTransformation`.

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::uc_view::{Ctx, Kind};
use std::os::raw::{c_long, c_ulong, c_void};
use std::ptr::{null, null_mut};

use crate::units::cls_dispatch::*;
use crate::units::cls_status_host::*;
use crate::units::cu_body::*;
use crate::units::cu_cargo::*;
use crate::units::cu_life::*;
use crate::units::cu_misc::*;
use crate::units::cu_more::*;
use crate::units::cu_small::*;
use crate::units::cu_world::*;
use crate::units::eu_body::*;
use crate::units::u12a_small::*;
use crate::units::uc_anim::*;
use crate::units::uc_body::*;
use crate::units::uc_create::*;
use crate::units::uc_media::*;
use crate::units::uc_produce::*;
use crate::units::uc_rest::*;
use crate::units::uca_air::*;
use crate::units::ucb_gate::*;
use crate::units::ucb_release::*;
use crate::units::ucg_ground::*;
use crate::units::ucg_sea::*;
use crate::units::uck_kinds::*;
use crate::units::ucp_people::*;
use crate::units::ucs_impact::*;
use crate::units::ucs_shots::*;
use crate::units::ucx_transports::*;

use crate::units::cu_body::game::ee_units_cu_body;
use crate::units::cu_more::game::ee_units_cu_more;
use crate::units::cu_small::game::ee_units_cu_small;
use crate::units::eu_body::game::{ee_units_eu_body, ee_units_eu_can_gather};
use crate::units::u12a_small::game::ee_units_cu_tiny;
use crate::units::uc_create::game::ee_units_uc_call;
use crate::units::uc_produce::game::ee_units_uc_produce;
use crate::units::uc_rest::game::ee_units_uc_rest;
use crate::units::ucb_release::game::ee_units_ucb_call;
use crate::units::uck_dispatch::game::ee_units_uck_dispatch;
use crate::units::uck_kinds::game::ee_units_uck_call;
use crate::units::ucp_people::game::ee_units_ucp_call;

fn p(h: u64) -> *mut c_void {
    h as usize as *mut c_void
}

fn long(v: i64) -> i64 {
    v as c_long as i64
}

/// `SetVelocity(v)` of the complex unit `unit` (`ee_units_cu_set_velocity(this, this, v)`: its two records).
fn set_velocity(x: &Ctx<'_>, unit: u64, v: f32) -> Option<i64> {
    // SAFETY: the live unit the body handed over
    let u = unsafe { crate::world::view::UnitView::of(x.t, unit) }?;
    let (f, c) = (u.fields().ptr(), u.complex().ptr() as *mut _);
    // SAFETY: the unit's two records, as the member passed them
    unsafe { crate::units::cu_inline::ee_units_cu_set_velocity(f, c, v) };
    Some(0)
}

/// The unit's `mStats->GetMaxVelocity()`.
fn max_velocity(x: &Ctx<'_>) -> Option<f32> {
    Some(crate::world_field!(x.u()?.stats()?.fields(), mMaxVelocity))
}

/// `SetTransformation(*m, true)` of the complex unit `unit`.
///
/// # Safety
/// `m` is a live transformation.
unsafe fn set_transformation(unit: u64, m: u64) -> i64 {
    // SAFETY: the caller's transformation (its rows)
    let rows = unsafe { &*(m as usize as *const [[f32; 4]; 4]) };
    // SAFETY: the member's call
    unsafe { ee_units_cu_tiny(EE_UNITS_CU_T_DO_SET_TRANSFORMATION, p(unit), p(m), 1, 0, rows[0][3], rows[1][3], 0.0) };
    0
}

/// The statement `op` of the host `kind` when it is such a call.
pub(crate) fn fwd_view(kind: Kind, x: &Ctx<'_>, op: i32, b: i64, c: i64, f: &mut [f32; 4]) -> Option<i64> {
    let (class, unit, a) = (x.class, x.unit, x.a);
    let (k, u, o, n) = (p(class), p(unit), p(a), null_mut::<c_void>());
    // SAFETY: the Rust entry points the C++ members called, with the live class, unit and objects the body handed
    // over, as the members passed them on
    unsafe {
        let v = match (kind, op) {
            // the class host's (every kind's host passes them on)
            (_, EE_UNITS_UC_DROP_PATH) => {
                crate::units::waypoints::ee_units_waypoints_drop(unit);
                0
            }
            (_, EE_UNITS_UC_SET_ANIMATION) => ee_units_cu_body(EE_UNITS_CU_DO_SET_ANIMATION, u, long(b), -1, 0) * 0,
            (_, EE_UNITS_UC_LOAD_CARGO) => ee_units_cu_body(EE_UNITS_CU_DO_LOAD_CARGO, u, a as i64, 1, 0) * 0,
            (_, EE_UNITS_UC_R_SET_TRANSFORMATION) => set_transformation(unit, a),
            (_, EE_UNITS_UC_R_BASE_LOAD_MEDIA) => {
                ee_units_uc_call(EE_UNITS_UC_DO_BASE_LOAD_MEDIA, k, n, a, 0, 0, null(), null()) * 0
            }
            (_, EE_UNITS_UC_R_BASE_UNLOAD_MEDIA) => {
                ee_units_uc_call(EE_UNITS_UC_DO_BASE_UNLOAD_MEDIA, k, n, a, 0, 0, null(), null()) * 0
            }
            (_, EE_UNITS_UC_R_CAN_REACH) => i64::from(ee_units_cu_small(EE_UNITS_CU_S_DO_CAN_REACH_UNIT, u, o, 0) != 0),
            (_, EE_UNITS_UC_R_FACES_POINT) => {
                i64::from(crate::units::ucn_noted::ee_units_ucn_faces_point(class, unit, f[0], f[1], b != 0))
            }
            (_, EE_UNITS_UC_CLASS_FLIES_OVER) => {
                i64::from(ee_units_uck_dispatch(EE_UNITS_BODY_FLIES_OVER as usize, o, n, 0, 0, 0, null_mut()) != 0)
            }
            (_, EE_UNITS_UC_IS_LAND_BUILDING) => i64::from(
                ee_units_uck_dispatch(EE_UNITS_BODY_IS_LAND_BUILDING as usize, o, n, 0, 0, 0, null_mut()) != 0,
            ),
            (_, EE_UNITS_UC_CLOAKABLE_CHAIN) => {
                crate::units::player_rest::game::ee_units_cloak_chain(a, 0, true);
                0
            }
            (_, EE_UNITS_UC_CLEAR_GOAL_QUEUE) => ee_units_cu_body(EE_UNITS_CU_DO_CLEAR_GOAL_QUEUE, o, 0, 0, 0) * 0,
            (_, EE_UNITS_UC_PRODUCE_UNIT) => i64::from(ee_units_uc_produce(k, u, o, true)),
            (_, EE_UNITS_UC_PLAY_NO_EFFECT) => ee_units_cu_body(EE_UNITS_CU_DO_PLAY_EFFECT, u, 100000, 0, 0) * 0,
            (_, EE_UNITS_UC_UPDATE_ORBITALS) => {
                ee_units_uc_rest(EE_UNITS_UC_DO_R_UPDATE_ORBITALS, n, u, 0, 0, 0, null_mut()) * 0
            }
            (_, EE_UNITS_UC_UPDATE_CAMOUFLAGE_TIMER) => {
                ee_units_st_camouflage(class, unit);
                0
            }
            (_, EE_UNITS_UC_UPDATE_EFFECTS) => {
                ee_units_uck_call(EE_UNITS_UCK_DO_UPDATE_EFFECTS, k, u, 0, 0, 0, 0.0) * 0
            }
            (_, EE_UNITS_UC_UPDATE_RELOAD_TIMER) => ee_units_ucp_call(EE_UNITS_UCP_DO_RELOAD_TIMER, n, u, 0, 0) * 0,
            (_, EE_UNITS_UC_UPDATE_TARGET_SEARCH_TIMER) => {
                ee_units_ucp_call(EE_UNITS_UCP_DO_TARGET_SEARCH_TIMER, n, u, 0, 0) * 0
            }
            (_, EE_UNITS_UC_UPDATE_RESOURCES_ROTTING) | (Kind::Ground, EE_UNITS_UCG_RESOURCES_ROTTING) => {
                ee_units_ucp_call(EE_UNITS_UCP_DO_RESOURCES_ROTTING, n, u, 0, 0) * 0
            }
            (_, EE_UNITS_UC_STOP) => set_velocity(x, a, 0.0)?,
            (_, EE_UNITS_UC_SET_OWNER) => ee_units_cu_tiny(EE_UNITS_CU_T_DO_SET_OWNER, o, u, 0, 0, 0.0, 0.0, 0.0) * 0,
            (_, EE_UNITS_UC_START_ACTION) => ee_units_cu_body(EE_UNITS_CU_DO_START_ACTION, o, long(b), 0, 0) * 0,
            (_, EE_UNITS_UC_SET_GOAL) => {
                crate::units::goals::ee_units_unit_set_goal(a, b as u64, false, 0);
                0
            }
            (_, EE_UNITS_UC_SET_GOAL_SELF) => {
                crate::units::goals::ee_units_unit_set_goal(unit, a, false, 0);
                0
            }
            (_, EE_UNITS_UC_SIMPLE_UNIT_CREATED) => {
                ee_units_uck_dispatch(EE_UNITS_BODY_SIMPLE_UNIT_CREATED as usize, k, n, a, 0, 0, null_mut()) * 0
            }
            (_, EE_UNITS_UC_GROUND_LOCATION) => {
                ee_units_uck_dispatch(EE_UNITS_BODY_GROUND_LOCATION as usize, k, n, 0, 0, 0, f.as_mut_ptr()) * 0
            }
            (_, EE_UNITS_UC_NUM_GOAL_QUEUES) => {
                i64::from(crate::units::cls_answers::ee_units_cls_num_goal_queues(x.cls()?.fields().ptr()))
            }
            (_, EE_UNITS_UC_UNIT_CREATED) => {
                ee_units_uck_dispatch(EE_UNITS_BODY_UNIT_CREATED as usize, k, o, 0, 0, 0, null_mut()) * 0
            }
            (_, EE_UNITS_UC_SET_HIT_POINTS_OF) => ee_units_eu_body(EE_UNITS_EU_DO_SET_HIT_POINTS, o, long(b), 0, 0) * 0,
            (_, EE_UNITS_UC_SET_HIT_POINTS_SELF) => {
                ee_units_eu_body(EE_UNITS_EU_DO_SET_HIT_POINTS, u, long(b), 0, 0) * 0
            }
            (_, EE_UNITS_UC_SET_FULL_HIT_POINTS) => {
                let max = x.u()?.stats()?.max_hit_points();
                ee_units_eu_body(EE_UNITS_EU_DO_SET_HIT_POINTS, u, max, 0, 0) * 0
            }
            (_, EE_UNITS_UC_MOVE_TO) => ee_units_eu_body(EE_UNITS_EU_DO_MOVE_TO, o, long(b), long(c), 1) * 0,
            (_, EE_UNITS_UC_SET_ANIMATION_OF) | (Kind::Buildings, EE_UNITS_UCB_SET_ANIMATION_OF) => {
                ee_units_cu_body(EE_UNITS_CU_DO_SET_ANIMATION, o, long(b), -1, 0) * 0
            }
            (_, EE_UNITS_UC_SET_VELOCITY_OF) => set_velocity(x, a, f[0])?,
            (_, EE_UNITS_UC_START_ACTION_SELF)
            | (Kind::Air, EE_UNITS_UCA_START_ACTION)
            | (Kind::Buildings, EE_UNITS_UCB_START_ACTION) => {
                ee_units_cu_body(EE_UNITS_CU_DO_START_ACTION, u, long(b), 0, 0) * 0
            }
            (_, EE_UNITS_UC_CLASS_ACTION_CHANGED)
            | (Kind::Air, EE_UNITS_UCA_ACTION_CHANGED)
            | (Kind::Buildings, EE_UNITS_UCB_ACTION_CHANGED) => {
                ee_units_uck_call(EE_UNITS_UCK_DO_ACTION_CHANGED, k, u, 0, 0, 0, 0.0) * 0
            }
            (_, EE_UNITS_UC_ACTION_CHANGED) => {
                let of = x.o()?.class()?.handle();
                ee_units_uck_call(EE_UNITS_UCK_DO_ACTION_CHANGED, p(of), o, 0, 0, 0, 0.0) * 0
            }
            (_, EE_UNITS_UC_ACTION_CHANGED_SELF) => {
                let of = x.u()?.class()?.handle();
                ee_units_uck_call(EE_UNITS_UCK_DO_ACTION_CHANGED, p(of), u, 0, 0, 0, 0.0) * 0
            }
            (_, EE_UNITS_UC_SET_ANIMATION_FRAME) => {
                ee_units_cu_small(EE_UNITS_CU_S_DO_SET_ANIMATION_FRAME, u, n, b as c_ulong as i64) * 0
            }
            (_, EE_UNITS_UC_RELEASE_SOUND) => ee_units_cu_small(EE_UNITS_CU_S_DO_RELEASE_SOUND, u, n, 0) * 0,
            (_, EE_UNITS_UC_SET_SOUND) => ee_units_cu_small(EE_UNITS_CU_S_DO_SET_SOUND, u, o, 0) * 0,
            (_, EE_UNITS_UC_FIRE_SHOT) => {
                crate::units::ucn_noted::ee_units_ucn_fire_shot(class, unit);
                0
            }
            // the air classes'
            (Kind::Air, EE_UNITS_UCA_APPROACH_CARRIER) => {
                crate::units::ucn_noted::ee_units_ucn_approach_carrier(class, unit, b != 0);
                0
            }
            (Kind::Air, EE_UNITS_UCA_SET_HOME_BASE) => {
                ee_units_cu_body(EE_UNITS_CU_DO_SET_HOME_BASE, u, a as i64, 0, 0) * 0
            }
            (Kind::Air, EE_UNITS_UCA_TAKE_OFF_CARRIER) => {
                crate::units::ucn_noted::ee_units_ucn_take_off_from_carrier(class, unit);
                0
            }
            (Kind::Air, EE_UNITS_UCA_APPROACH_RUNWAY) => {
                crate::units::ucn_noted::ee_units_ucn_approach_runway(
                    class,
                    unit,
                    b as c_long as i32,
                    c as c_long as i32,
                );
                0
            }
            (Kind::Air, EE_UNITS_UCA_TAKE_OFF_RUNWAY) => {
                crate::units::ucn_noted::ee_units_ucn_take_off_from_runway(
                    class,
                    unit,
                    b as c_long as i32,
                    c as c_long as i32,
                );
                0
            }
            (Kind::Air, EE_UNITS_UCA_CLEAR_GOALS) | (Kind::People, EE_UNITS_UCP_CLEAR_GOALS) => {
                ee_units_cu_body(EE_UNITS_CU_DO_CLEAR_GOALS, u, 0, 0, 0) * 0
            }
            (Kind::Air, EE_UNITS_UCA_KILL) => ee_units_cu_body(EE_UNITS_CU_DO_KILL, u, 1, 0, 0) * 0,
            (Kind::Air, EE_UNITS_UCA_RETURN_TO_AIRPORT) => {
                crate::units::ucn_noted::ee_units_ucn_return_to_airport(class, unit);
                0
            }
            (Kind::Air, EE_UNITS_UCA_SET_ANIMATION) => {
                ee_units_cu_body(EE_UNITS_CU_DO_SET_ANIMATION, u, long(b), -1, 0) * 0
            }
            (Kind::Air, EE_UNITS_UCA_LOAD_CARGO) => ee_units_cu_body(EE_UNITS_CU_DO_LOAD_CARGO, u, a as i64, 1, 0) * 0,
            (Kind::Air, EE_UNITS_UCA_DROP_PATH_OF) => {
                crate::units::waypoints::ee_units_waypoints_drop(a);
                0
            }
            (Kind::Air, EE_UNITS_UCA_DROP_PATH) => {
                crate::units::waypoints::ee_units_waypoints_drop(unit);
                0
            }
            // the buildings'
            (Kind::Buildings, EE_UNITS_UCB_SET_TRANSFORMATION_OF) => set_transformation(a, b as u64),
            (Kind::Buildings, EE_UNITS_UCB_GET_PART) => {
                crate::units::ucn_noted::ee_units_ucn_gate_part(class, a, b as c_long) as i64
            }
            (Kind::Buildings, EE_UNITS_UCB_PART_LEFT_OUT) => i64::from(
                ee_units_uck_dispatch(EE_UNITS_BODY_IS_PART_LEFT_OUT as usize, k, o, 0, 0, 0, null_mut()) != 0,
            ),
            (Kind::Buildings, EE_UNITS_UCB_GATE_BASE_SURROUNDING) => {
                ee_units_uc_rest(EE_UNITS_UC_DO_R_BASE_SURROUNDING, k, o, b as u64, long(c), 1, null_mut()) * 0
            }
            (Kind::Buildings, EE_UNITS_UCB_BASE_SURROUNDING) => {
                ee_units_uc_rest(EE_UNITS_UC_DO_R_BASE_SURROUNDING, k, u, a, long(b), i64::from(c != 0), null_mut()) * 0
            }
            (Kind::Buildings, EE_UNITS_UCB_BUILDING_UNIT_CREATED) => {
                ee_units_ucb_call(EE_UNITS_UCB_DO_UNIT_CREATED, k, u, 0, 0, 0, null()) * 0
            }
            (Kind::Buildings, EE_UNITS_UCB_SPACE_SET_BUILT) => {
                ee_units_cu_body(EE_UNITS_CU_DO_SET_BUILT, u, 1, 0, 1) * 0
            }
            (Kind::Buildings, EE_UNITS_UCB_SET_BUILT) => {
                ee_units_cu_body(EE_UNITS_CU_DO_SET_BUILT, u, long(b), long(c & 1), long(c >> 1)) * 0
            }
            (Kind::Buildings, EE_UNITS_UCB_REFRESH_GOALS) => {
                crate::units::goals::ee_units_unit_refresh_goals(a);
                0
            }
            (Kind::Buildings, EE_UNITS_UCB_REFRESH_ACTIVE) => {
                crate::units::goals::ee_units_unit_refresh_active_goal(a);
                0
            }
            (Kind::Buildings, EE_UNITS_UCB_FIND_PASSENGER) => {
                ee_units_cu_body(EE_UNITS_CU_DO_FIND_PASSENGER, u, b as c_ulong as i64, 0, 0)
            }
            (Kind::Buildings, EE_UNITS_UCB_UNLOAD_BY_ID) => {
                ee_units_cu_body(EE_UNITS_CU_DO_UNLOAD_CARGO_BY_ID, u, b as c_ulong as i64, a as i64, 0)
            }
            (Kind::Buildings, EE_UNITS_UCB_LOAD_CARGO) => {
                ee_units_cu_body(EE_UNITS_CU_DO_LOAD_CARGO, u, a as i64, 0, 0) * 0
            }
            (Kind::Buildings, EE_UNITS_UCB_SET_HOME_BASE) => {
                ee_units_cu_body(EE_UNITS_CU_DO_SET_HOME_BASE, o, unit as i64, 0, 0) * 0
            }
            (Kind::Buildings, EE_UNITS_UCB_UPDATE_HEAL_SEARCH) => {
                ee_units_ucp_call(EE_UNITS_UCP_DO_HEAL_SEARCH_TIMER, n, u, 0, 0) * 0
            }
            (Kind::Buildings, EE_UNITS_UCB_REGENERATE_POWER) => {
                ee_units_st_power(class, unit);
                0
            }
            (Kind::Buildings, EE_UNITS_UCB_PRIEST_CONVERSION) => {
                ee_units_ucp_call(EE_UNITS_UCP_DO_UPDATE_CONVERSION, k, u, 0, 0) * 0
            }
            (Kind::Buildings, EE_UNITS_UCB_RELEASE_CONVERSION) | (Kind::People, EE_UNITS_UCP_RELEASE_CONVERSION) => {
                ee_units_cu_body(EE_UNITS_CU_DO_RELEASE_CONVERSION_EFFECT, u, 0, 0, 0) * 0
            }
            (Kind::Buildings, EE_UNITS_UCB_HIDE_CONVERSION) => {
                ee_units_ucp_call(EE_UNITS_UCP_DO_HIDE_CONVERSION, k, u, 0, 0) * 0
            }
            (Kind::Buildings, EE_UNITS_UCB_CAN_GATHER) => {
                // SAFETY: the resource the body handed over
                let (tx, ty) = crate::world::view::UnitView::of(x.t, b as u64)?.tile_xy();
                i64::from(ee_units_eu_can_gather(o, p(b as u64), tx, ty, 0, 0) != 0)
            }
            // the ground classes'
            (Kind::Ground, EE_UNITS_UCG_SET_TRANSFORMATION) => set_transformation(unit, a),
            (Kind::Ground, EE_UNITS_UCG_BASE_CREATE_AT) => {
                let m = a as usize as *const [[f32; 4]; 4];
                let w = EE_UNITS_UC_DO_BASE_CREATE_UNIT_AT;
                ee_units_uc_call(w, k, n, c as u64, long(b >> 1), b & 1, null(), m)
            }
            (Kind::Ground, EE_UNITS_UCG_SET_LOCATION_OF) => {
                ee_units_cu_tiny(EE_UNITS_CU_T_DO_SET_LOCATION, o, null(), 1, 0, f[0], f[1], f[2]) * 0
            }
            // the people's
            (Kind::People, EE_UNITS_UCP_UPDATE_FANATICISM) => {
                ee_units_cu_body(EE_UNITS_CU_DO_UPDATE_FANATICISM_COLOR, u, 0, 0, 0) * 0
            }
            // the shots'
            (Kind::Shots, EE_UNITS_UCS_PLACE_ON_GROUND) => {
                ee_units_cu_more(EE_UNITS_CUM_DO_PLACE_ON_GROUND, u, 0, 0, 0, f[0], f[1], f[2], null_mut(), null_mut());
                0
            }
            (Kind::Shots, EE_UNITS_UCS_CAN_MOVE) => {
                let me = x.u()?;
                let terrain = me.tile()?.terrain_type();
                let stats = me.stats().map_or(null(), |s| s.fields().ptr());
                let classf = x.cls()?.fields().ptr();
                i64::from(
                    crate::units::cls_answers::ee_units_cls_can_move_on_terrain(classf, stats, terrain as i32) != 0,
                )
            }
            (Kind::Shots, EE_UNITS_UCS_FULL_SPEED) => set_velocity(x, unit, max_velocity(x)?)?,
            (Kind::Shots, EE_UNITS_UCS_UNLOAD_NOW) => {
                ee_units_cu_body(EE_UNITS_CU_DO_UNLOAD_CARGO_NOW, u, a as i64, 0, 0) * 0
            }
            (Kind::Shots, EE_UNITS_UCS_LAUNCH_SHOT) => {
                let at = f.as_ptr() as *const [f32; 3];
                ee_units_uc_call(EE_UNITS_UC_DO_LAUNCH_SHOT, k, u, a, b, 0, at, null()) * 0
            }
            // the transports'
            (Kind::Transports, EE_UNITS_UCX_UNLOAD_AT_TILE) => {
                ee_units_cu_body(EE_UNITS_CU_DO_UNLOAD_CARGO_AT_TILE, u, long(b), long(c), i64::from(a != 0)) * 0
            }
            (Kind::Transports, EE_UNITS_UCX_UNLOAD_NOW_OF) => {
                ee_units_cu_body(EE_UNITS_CU_DO_UNLOAD_CARGO_NOW, o, b, 0, 0) * 0
            }
            (Kind::Transports, EE_UNITS_UCX_SET_TRANSFORMATION) => set_transformation(unit, a),
            (Kind::Transports, EE_UNITS_UCX_SET_BUILT) => ee_units_cu_body(EE_UNITS_CU_DO_SET_BUILT, u, 0, 1, 1) * 0,
            _ => return None,
        };
        Some(v)
    }
}
