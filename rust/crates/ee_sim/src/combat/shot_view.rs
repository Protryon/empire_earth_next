//! The shots' plain statements over the world views (the units' lane, round 13): each `EE_COMBAT_SHOT_*` read or
//! operation of EEUnitClassShots.cpp's `EEShotOp` and `EEShotOp64` that reads or stores one member of a Rust record
//! (the unit's, its class's, its stats', its tile's, its player's, the physics') or calls a Rust entry point the
//! unit's or class's inline member called, answered here (over the host's `unit` and `class`) before the C++ host
//! is asked; their cases left the C++ switches. The answers convert as the C++ did (`(int32_t)` of a `long`, a
//! float's bits, a bool 0 or 1).
#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::combat::shots::*;
use crate::units::uc_view::{complex_mut, links, stats_links};
use crate::world::layout::layout;
use crate::world::view::{ClassView, TileView, UnitView, WorldRead, WorldView};
use crate::world_field;
use std::os::raw::{c_long, c_ulong, c_void};

/// A read or an operation of `EEShotOp` on the shot `unit` of `class` (None for the C++ host's).
pub(crate) fn shot_op(unit: u64, class: u64, op: i32, arg: i32) -> Option<i32> {
    use crate::units::cu_body::game::ee_units_cu_body;
    use crate::units::cu_body::*;
    use crate::units::cu_cargo::*;
    // SAFETY: the shot's plan runs inside the C++ member that handed over its live unit and class (the world held)
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let u = || unsafe { UnitView::of(&t, unit) };
    // SAFETY: as above
    let k = || unsafe { ClassView::of(&t, class) };
    let p = |h: u64| h as usize as *mut c_void;
    let null = |q: *mut c_void| i32::from(!q.is_null());
    Some(match op {
        // ---- reads
        EE_COMBAT_SHOT_R_CLASS_ROTTING_TIME => world_field!(k()?.fields(), mRottingTime) as i32,
        EE_COMBAT_SHOT_R_TILE_HEIGHT => u()?.tile()?.height().to_bits() as i32,
        EE_COMBAT_SHOT_R_SPLASH_EFFECT => null(world_field!(links(&k()?), mSplashEffect).cast()),
        EE_COMBAT_SHOT_R_DEATH_EFFECT => null(world_field!(links(&k()?), mDeathEffect).cast()),
        EE_COMBAT_SHOT_R_DEATH_SOUND => null(world_field!(stats_links(&u()?.stats()?), mDeathSound).cast()),
        EE_COMBAT_SHOT_R_CARGO => u()?.cargo_len() as c_ulong as i32,
        EE_COMBAT_SHOT_R_LINE_OF_SIGHT => {
            let u = u()?;
            let sight: f32 = world_field!(u.stats()?.fields(), mLineOfSight);
            (sight + u.sight()).to_bits() as i32
        }
        EE_COMBAT_SHOT_R_COMPLEX_GOAL_KIND => {
            let goal = u()?.complex_unit()?.goal_handle();
            if goal == 0 {
                return None;
            }
            // SAFETY: a live goal's kind byte
            i32::from(unsafe { std::ptr::read((goal as usize + layout().goal_kind as usize) as *const u8) })
        }
        EE_COMBAT_SHOT_R_REMAINS => null(world_field!(links(&k()?), mRemainsClass).cast()),
        EE_COMBAT_SHOT_R_IS_PROJECTILE => i32::from(world_field!(k()?.fields(), mIsProjectile)),
        EE_COMBAT_SHOT_R_POPULATION_COST => world_field!(u()?.class()?.fields(), mWeight) as i32,
        EE_COMBAT_SHOT_R_PHYSICS_MODEL => null(world_field!(links(&k()?), mPhysicsModel).cast()),
        EE_COMBAT_SHOT_R_TICK => WorldView::global(&t)?.tick_ms() as i32,
        // ---- operations
        EE_COMBAT_SHOT_SET_ANIMATION => {
            // SAFETY: the member's call
            unsafe { ee_units_cu_body(EE_UNITS_CU_DO_SET_ANIMATION, p(unit), i64::from(arg), -1, 0) };
            0
        }
        EE_COMBAT_SHOT_SET_ROTTING => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mRottingAwayTime = arg as c_long };
            0
        }
        EE_COMBAT_SHOT_PHYSICS_ACTION => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mCurrentPhysicsAction = arg as c_ulong };
            0
        }
        EE_COMBAT_SHOT_DROP_PATH => {
            // SAFETY: the member's call on the live unit
            unsafe { crate::units::waypoints::ee_units_waypoints_drop(unit) };
            0
        }
        EE_COMBAT_SHOT_KILL_CARGO => {
            // SAFETY: the member's call
            unsafe { ee_units_cu_body(EE_UNITS_CU_DO_KILL_CARGO, p(unit), 0, 0, 0) };
            0
        }
        EE_COMBAT_SHOT_FIRE_SHOT => {
            crate::units::ucn_noted::ee_units_ucn_fire_shot(class, unit);
            0
        }
        EE_COMBAT_SHOT_SET_HIT_POINTS => {
            let w = crate::units::eu_body::EE_UNITS_EU_DO_SET_HIT_POINTS;
            // SAFETY: the member's call
            unsafe { crate::units::eu_body::game::ee_units_eu_body(w, p(unit), i64::from(arg), 0, 0) };
            0
        }
        EE_COMBAT_SHOT_START_ACTION => {
            // SAFETY: the member's call (`(unsigned long)inArg` into a long: the same 32 bits)
            unsafe { ee_units_cu_body(EE_UNITS_CU_DO_START_ACTION, p(unit), arg as c_long as i64, 0, 0) };
            0
        }
        EE_COMBAT_SHOT_ACTION_CHANGED => {
            let w = crate::units::uck_kinds::EE_UNITS_UCK_DO_ACTION_CHANGED;
            // SAFETY: the member's call
            unsafe { crate::units::uck_kinds::game::ee_units_uck_call(w, p(class), p(unit), 0, 0, 0, 0.0) };
            0
        }
        EE_COMBAT_SHOT_POPULATION => {
            let saved = u()?.player()?.saved().ptr() as *mut crate::world::player_fields::EeUnitsPlayerSaved;
            // SAFETY: the unit's player's member
            unsafe { (*saved).mPopulationCount -= arg as c_long };
            0
        }
        EE_COMBAT_SHOT_ORBITALS => {
            let w = crate::units::uc_rest::EE_UNITS_UC_DO_R_UPDATE_ORBITALS;
            let n = std::ptr::null_mut();
            // SAFETY: the member's call
            unsafe { crate::units::uc_rest::game::ee_units_uc_rest(w, n, p(unit), 0, 0, 0, std::ptr::null_mut()) };
            0
        }
        EE_COMBAT_SHOT_EFFECTS => {
            let w = crate::units::uck_kinds::EE_UNITS_UCK_DO_UPDATE_EFFECTS;
            // SAFETY: the member's call
            unsafe { crate::units::uck_kinds::game::ee_units_uck_call(w, p(class), p(unit), 0, 0, 0, 0.0) };
            0
        }
        EE_COMBAT_SHOT_SET_OPACITY => {
            let mut value = f32::from_bits(arg as u32);
            let w = crate::units::uca_air::EE_UNITS_UCA_DO_OPACITY_CHAIN;
            let n = std::ptr::null_mut();
            // SAFETY: the member's call
            unsafe { crate::units::uca_air::game::ee_units_uca_call(w, n, p(unit), 0, 0, 0, &mut value) };
            0
        }
        EE_COMBAT_SHOT_SET_FLIGHT_TIME => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mFlightTime = arg as c_long };
            0
        }
        EE_COMBAT_SHOT_SET_CLOAKED => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mCloaked = arg as c_long as c_ulong };
            0
        }
        _ => return None,
    })
}

/// A statement of `EEShotOp64` on the shot `unit` (`a` an object, `f` floats out; None for the C++ host's).
pub(crate) fn shot_x(unit: u64, op: i32, a: u64, f: &mut [f32; 4]) -> Option<i64> {
    // SAFETY: as shot_op
    let t = unsafe { WorldRead::begin() };
    // SAFETY: the live unit the plan handed over
    let o = || unsafe { UnitView::of(&t, a) };
    Some(match op {
        EE_COMBAT_SHOT_X_UNIT_TYPE_OF => o()?.class()?.unit_type() as i64,
        EE_COMBAT_SHOT_X_TILE_X_OF => o()?.tile_xy().0,
        EE_COMBAT_SHOT_X_TILE_Y_OF => o()?.tile_xy().1,
        EE_COMBAT_SHOT_X_COMPLEX_OF => o()?.complex_unit_handle() as i64,
        EE_COMBAT_SHOT_X_CONTAINER_OF => o()?.container_handle() as i64,
        EE_COMBAT_SHOT_X_TILE_HEIGHT => {
            // SAFETY: a live map tile
            f[0] = unsafe { TileView::of(&t, a) }?.height();
            0
        }
        // SAFETY: the shot's live unit
        EE_COMBAT_SHOT_X_GOAL_STATE => unsafe { UnitView::of(&t, unit) }?.goal_handle() as i64,
        EE_COMBAT_SHOT_X_DEATH_SOUND => {
            // SAFETY: as above
            let s = unsafe { UnitView::of(&t, unit) }?.stats()?;
            world_field!(stats_links(&s), mDeathSound) as usize as i64
        }
        EE_COMBAT_SHOT_X_LOCATION => {
            // SAFETY: as above
            f[..3].copy_from_slice(&unsafe { UnitView::of(&t, unit) }?.location());
            0
        }
        _ => return None,
    })
}
