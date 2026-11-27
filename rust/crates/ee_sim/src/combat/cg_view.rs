//! The combat goals' hosts' plain statements over the world views (the units' lane, round 13, second leg): the
//! statements of EECombatGoals.cpp's `ee_units_cg_host`, `EEQueryRead`, `EEQueryHandle`, `EEQueryAct` and
//! `EEWorldOp` that read or store one member of a Rust record (the unit's, its class's, its stats') or that called
//! a unit or class member whose inline body is one call of a Rust entry point, answered here before the C++ is
//! asked (cg_goal.rs and cg_query.rs for the first, reads.rs for the query host, world.rs and cg_world.rs for the
//! world host); their cases left the C++. The answers convert as the C++ returns did.
#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::combat::cg_goal::*;
use crate::combat::cg_query::*;
use crate::combat::cg_world::*;
use crate::combat::reads::*;
use crate::combat::world::*;
use crate::units::uc_view::{complex_mut, links};
use crate::world::view::{ClassView, StatsView, UnitView, WorldRead};
use crate::world_field;
use std::os::raw::{c_long, c_ulong, c_void};

fn p(h: u64) -> *mut c_void {
    h as usize as *mut c_void
}

/// `SetAnimation(part, -1, 0)` of the unit.
fn set_animation(unit: u64, part: i64) {
    use crate::units::cu_body::{game::ee_units_cu_body, EE_UNITS_CU_DO_SET_ANIMATION};
    // SAFETY: the member's call on the live unit
    unsafe { ee_units_cu_body(EE_UNITS_CU_DO_SET_ANIMATION, p(unit), part, -1, 0) };
}

/// `StartAction(action)` of the unit.
fn start_action(unit: u64, action: i64) {
    use crate::units::cu_body::{game::ee_units_cu_body, EE_UNITS_CU_DO_START_ACTION};
    // SAFETY: the member's call on the live unit
    unsafe { ee_units_cu_body(EE_UNITS_CU_DO_START_ACTION, p(unit), action, 0, 0) };
}

/// `unit->mUnitClass->ActionChanged(unit)`.
fn action_changed(u: &UnitView<'_>) -> Option<()> {
    let class = u.class()?.handle();
    let w = crate::units::uck_kinds::EE_UNITS_UCK_DO_ACTION_CHANGED;
    // SAFETY: the member's call on the live unit and its class
    unsafe { crate::units::uck_kinds::game::ee_units_uck_call(w, p(class), p(u.handle()), 0, 0, 0, 0.0) };
    Some(())
}

/// `ee_units_cg_host`'s statement `op` (`unit` the goal's unit, `a` an object, `b`, `c` values, `f` floats out).
pub(crate) fn cg_plain(op: i32, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> Option<i64> {
    // SAFETY: the goal's step runs inside the C++ call that handed over its live objects (the world held)
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let u = || unsafe { UnitView::of(&t, unit) };
    // SAFETY: as above
    let o = || unsafe { UnitView::of(&t, a) };
    Some(match op {
        EE_UNITS_CG_TARGET_COMPLEX => o()?.complex_unit_handle() as i64,
        EE_UNITS_CG_STATS_SCALE => {
            f[0] = u()?.stats()?.scale();
            0
        }
        EE_UNITS_CG_LINE_OF_SIGHT => {
            let u = u()?;
            let sight: f32 = world_field!(u.stats()?.fields(), mLineOfSight);
            f[0] = sight + u.sight();
            0
        }
        EE_UNITS_CG_CLASS => o()?.class().map_or(0, |k| k.handle() as i64),
        EE_UNITS_CG_PHYSICS_ACTION => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mCurrentPhysicsAction = b as c_ulong };
            0
        }
        EE_UNITS_CG_ANIMATION => world_field!(u()?.complex(), mAnimation) as i32 as i64,
        EE_UNITS_CG_SET_ANIMATION => {
            set_animation(unit, b as c_long as i64);
            0
        }
        EE_UNITS_CG_DROP_PATH => {
            // SAFETY: the member's call on the live unit
            unsafe { crate::units::waypoints::ee_units_waypoints_drop(unit) };
            0
        }
        EE_UNITS_CG_FRAME => u()?.model_frame() as i32 as i64,
        EE_UNITS_CG_FIRE_SHOT => {
            let class = u()?.class()?.handle();
            crate::units::ucn_noted::ee_units_ucn_fire_shot(class, unit);
            0
        }
        EE_UNITS_CG_FIRING_HIGH => i64::from(world_field!(u()?.complex(), mIsFiringHigh)),
        EE_UNITS_CG_TURRET_ANGLE => {
            let angles: *mut f32 = world_field!(u()?.complex(), mTurretAngles);
            // SAFETY: the unit's turret angle c (one per turret part)
            f[0] = unsafe { *angles.add(c as usize) };
            0
        }
        EE_UNITS_CG_LOCATION => {
            let rows = u()?.transform();
            // SAFETY: the caller's sixteen floats
            unsafe { std::ptr::write(b as usize as *mut [[f32; 4]; 4], rows) };
            0
        }
        _ => return None,
    })
}

/// `EEQueryRead`'s plain reads of `object`.
pub(crate) fn query_plain_read(t: &WorldRead, code: i32, object: u64) -> Option<u32> {
    // SAFETY: the query's handles are live units (the world held)
    let o = || unsafe { UnitView::of(t, object) };
    Some(match code {
        EE_COMBAT_Q_SCALE => o()?.model_scale().to_bits(),
        EE_COMBAT_Q_LINE_OF_SIGHT => {
            let o = o()?;
            let sight: f32 = world_field!(o.stats()?.fields(), mLineOfSight);
            (sight + o.sight()).to_bits()
        }
        EE_COMBAT_Q_CONTACT_X => o()?.contact_location()[0].to_bits(),
        EE_COMBAT_Q_CONTACT_Y => o()?.contact_location()[1].to_bits(),
        EE_COMBAT_Q_CONTACT_Z => o()?.contact_location()[2].to_bits(),
        EE_COMBAT_Q_PROJECTILE_CATEGORY => {
            let class = o()?.class()?;
            // SAFETY: the class's projectile class (0: none)
            unsafe { ClassView::of(t, class.projectile_class_handle()) }
                .map_or(EE_COMBAT_Q_NONE, |k| k.category() as u32)
        }
        EE_COMBAT_Q_SPARKS_EFFECT => u32::from(!world_field!(links(&o()?.class()?), mImpactSparksEffect).is_null()),
        _ => return None,
    })
}

/// `EEQueryHandle`'s plain reads of `object`.
pub(crate) fn query_plain_handle(code: i32, object: u64) -> Option<u64> {
    // SAFETY: as query_plain_read
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let o = || unsafe { UnitView::of(&t, object) };
    Some(match code {
        EE_COMBAT_H_COMPLEX => o().map_or(0, |u| u.complex_unit_handle()),
        EE_COMBAT_H_PREVIOUS_PART => o()?.previous_part_handle(),
        EE_COMBAT_H_OWNER => o()?.owner_handle(),
        // SAFETY: the shot's live unit
        EE_COMBAT_H_GOAL_TARGET => unsafe { crate::combat::cg_query::ee_units_cgq_shot_target(object) },
        _ => return None,
    })
}

/// `EEQueryAct`'s one-member stores on `object`.
pub(crate) fn query_plain_act(code: i32, object: u64, arg: i32) -> Option<()> {
    // SAFETY: as query_plain_read
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let o = unsafe { UnitView::of(&t, object) }?;
    let cx = complex_mut(&o);
    match code {
        // SAFETY: the unit's members
        EE_COMBAT_A_FIRING_HIGH => unsafe { (*cx).mIsFiringHigh = true },
        // SAFETY: as above
        EE_COMBAT_A_WEAPON_TAG => unsafe { (*cx).mWeaponTagPoint = arg as u32 as c_ulong },
        _ => return None,
    }
    Some(())
}

/// `EEWorldOp`'s plain statements (`object`, `other` the handles, `args` the block).
pub(crate) fn world_plain(code: i32, object: u64, other: u64, args: &EeCombatWorldArgs) -> Option<u64> {
    // SAFETY: the host's call runs on the simulation thread, which holds the world; its handles are live
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let u = || unsafe { UnitView::of(&t, object) };
    // SAFETY: as above
    let o = || unsafe { UnitView::of(&t, other) };
    // SAFETY: as above (args[0] a class)
    let k = || unsafe { ClassView::of(&t, args[0] as u64) };
    let bits = |v: f32| u64::from(v.to_bits());
    let long = |v: c_long| v as i64 as u64;
    Some(match code {
        EE_COMBAT_W_HAS_LAST_ATTACKER => u64::from(u()?.last_attacker_handle() != 0),
        EE_COMBAT_W_CLEAR_LAST_ATTACKER => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mLastAttacker = std::ptr::null_mut() };
            0
        }
        EE_COMBAT_W_HC_CATEGORY => u()?.class()?.category(),
        EE_COMBAT_W_HC_SPECIAL_ATTACK => u64::from(world_field!(u()?.complex(), mIsSpecialAttack)),
        EE_COMBAT_W_HC_LAUNCHER => u()?.launcher_class_handle(),
        EE_COMBAT_W_HC_UPGRADE_AREA => {
            // SAFETY: the stats the block names
            bits(world_field!(unsafe { StatsView::of(&t, args[0] as u64) }?.fields(), mAreaEffect))
        }
        EE_COMBAT_W_HC_CLASS_ATTACK => bits(world_field!(k()?.fields(), mAttack) as f32),
        EE_COMBAT_W_HC_CLASS_AREA => bits(world_field!(k()?.fields(), mAreaEffect)),
        EE_COMBAT_W_HC_CLASS_AMMO => long(world_field!(k()?.fields(), mAmmoType)),
        EE_COMBAT_W_HC_CLASS_WEAPON => long(world_field!(k()?.fields(), mWeapon)),
        EE_COMBAT_W_HC_CLASS_CATEGORY => k()?.category(),
        EE_COMBAT_W_HC_CLASS_FRIENDLY => bits(world_field!(k()?.fields(), mFriendlyDamageFactor)),
        EE_COMBAT_W_RING_TILE_X => o()?.tile_xy().0 as u64,
        EE_COMBAT_W_RING_TILE_Y => o()?.tile_xy().1 as u64,
        EE_COMBAT_W_RING_TILE_SIZE => o()?.class()?.tile_size() as u64,
        EE_COMBAT_W_ACTION_CHANGED => {
            action_changed(&u()?)?;
            0
        }
        EE_COMBAT_W_START_ACTION => {
            start_action(object, args[0] as c_ulong as c_long as i64);
            0
        }
        EE_COMBAT_W_IGNORE => {
            let w = crate::units::cu_small::EE_UNITS_CU_S_DO_IGNORE_TARGET;
            // SAFETY: the member's call on the live units
            unsafe { crate::units::cu_small::game::ee_units_cu_small(w, p(object), p(other), 0) };
            0
        }
        _ => return None,
    })
}
