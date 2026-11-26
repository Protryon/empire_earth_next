//! The class host's scratch values, kept here (the units' lane, round 14): EEUnitClass.cpp's file statics that one
//! statement filled and the next read back (the three combat formulas' outputs, Heal's effect handle, the closest
//! tile, the turret part) are this thread's cells, and the statements that only stored or read them are answered
//! here before the C++ host is asked. A statement that still calls C++ is passed on to `ee_units_uc_host` with the
//! cell's address in an operand the original left unused; the ones whose callee is Rust call it from here.
//! [`uc_view`](super::uc_view::uc_view) asks [`scratch_view`] after the kinds' own statements.

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::cell::Cell;
use std::os::raw::{c_long, c_ulong, c_void};

use crate::combat::captured::{ee_combat_heal_captured, ee_combat_regenerate_captured, ee_combat_timer_captured};
use crate::combat::{
    EeCombatHealIn, EeCombatHealOut, EeCombatRegenerateIn, EeCombatRegenerateOut, EeCombatTimerIn, EeCombatTimerOut,
};
use crate::units::uc_anim::*;
use crate::units::uc_body::game::ee_units_uc_host;
use crate::units::uc_body::*;
use crate::units::uc_create::*;
use crate::units::uc_produce::*;
use crate::units::uc_view::Ctx;
use crate::world::unit_fields::EeUnitsEffectHandle;
use crate::world::view::UnitView;
use crate::world_field;
use ee_base::math::transform::RotationOrder;
use ee_base::math::types::GeTransformation;

extern "C" {
    // `EEAreaEffect::Covers`'s body (ee_game screens area_effect_ffi.rs)
    fn ee_screens_ae_covers(effect: usize, player: u64, unit: usize, x: f32, y: f32) -> bool;
}

thread_local! {
    /// `EECombatTimer`'s, `EECombatHeal`'s and `EECombatRegenerate`'s last outputs (each read right after it)
    static TIMER: Cell<EeCombatTimerOut> = Cell::new(EeCombatTimerOut::default());
    static HEAL: Cell<EeCombatHealOut> = Cell::new(EeCombatHealOut::default());
    static REGENERATE: Cell<EeCombatRegenerateOut> = Cell::new(EeCombatRegenerateOut::default());
    /// Heal's effect handle (the effect manager fills it; 0 when it makes none)
    static HANDLE: Cell<EeUnitsEffectHandle> = const { Cell::new(EeUnitsEffectHandle { mValue: 0 }) };
    /// the closest tile's x and y (`GetClosestTile`'s and `EEUpdateUnitTile`'s outputs)
    static TILE: Cell<[c_long; 2]> = const { Cell::new([0; 2]) };
    /// `FindTurretPart`'s part index
    static PART: Cell<c_ulong> = const { Cell::new(0) };
}

fn host(x: &Ctx<'_>, op: i32, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
    let p = |h: u64| h as usize as *mut c_void;
    // SAFETY: the C++ host's statement with the live class, unit and objects the body handed over
    unsafe { ee_units_uc_host(op, p(x.class), p(x.unit), a, b, c, f) }
}

/// `(int32_t)` of a `long` member, as the formulas' inputs took them.
fn i(v: c_long) -> i32 {
    v as i32
}

/// The statement `op` when it is one of the scratch values'.
pub(crate) fn scratch_view(x: &Ctx<'_>, op: i32, b: i64, c: i64, f: &mut [f32; 4]) -> Option<i64> {
    let interval = || crate::tech::civ::rules().combat.regeneration_interval;
    Some(match op {
        // HealGarrison's timer: EECombatTimer({mRegenerationTimer, gPhysics->mTickTimeMs, the rules' interval})
        EE_UNITS_UC_COMBAT_TIMER => {
            let u = x.u()?;
            let input = EeCombatTimerIn {
                timer: i(world_field!(u.complex(), mRegenerationTimer)),
                tick_ms: x.world()?.tick_ms() as c_long as i32,
                interval: interval(),
            };
            // SAFETY: a valid record
            let out = unsafe { ee_combat_timer_captured(&input) };
            TIMER.set(out);
            i64::from(out.due)
        }
        EE_UNITS_UC_TIMER_VALUE => i64::from(TIMER.get().timer),
        // a garrisoned unit a's heal: EECombatHeal({a->GetHitPoints(), mStats->mHealingPoints, a's max hit points})
        EE_UNITS_UC_COMBAT_HEAL => {
            let (u, o) = (x.u()?, x.o()?);
            let input = EeCombatHealIn {
                hit_points: o.hit_points() as i32,
                healing: i(world_field!(u.stats()?.fields(), mHealingPoints)),
                max_hit_points: o.stats()?.max_hit_points() as i32,
            };
            // SAFETY: a valid record
            let out = unsafe { ee_combat_heal_captured(&input) };
            HEAL.set(out);
            i64::from(out.set)
        }
        EE_UNITS_UC_HEAL_HIT_POINTS => i64::from(HEAL.get().hit_points),
        // Regenerate's: EECombatRegenerate({mRegenerationTimer, the tick, mHitPoints, the stats' regeneration and
        // maximum, the rules' interval})
        EE_UNITS_UC_COMBAT_REGENERATE => {
            let u = x.u()?;
            let stats = u.stats()?;
            let input = EeCombatRegenerateIn {
                timer: i(world_field!(u.complex(), mRegenerationTimer)),
                tick_ms: x.world()?.tick_ms() as c_long as i32,
                hit_points: u.hit_points() as i32,
                regeneration: i(world_field!(stats.fields(), mRegeneration)),
                max_hit_points: stats.max_hit_points() as i32,
                interval: interval(),
            };
            // SAFETY: a valid record
            let out = unsafe { ee_combat_regenerate_captured(&input) };
            REGENERATE.set(out);
            i64::from(out.set)
        }
        EE_UNITS_UC_REGENERATE_HIT_POINTS => i64::from(REGENERATE.get().hit_points),
        EE_UNITS_UC_REGENERATE_TIMER => i64::from(REGENERATE.get().timer),
        // Heal's handle: reset, filled by the effect's creation (its address in c), read
        EE_UNITS_UC_RESET_HANDLE => {
            HANDLE.set(EeUnitsEffectHandle { mValue: 0 });
            0
        }
        EE_UNITS_UC_HEAL_BETWEEN | EE_UNITS_UC_HEAL_ON_TARGET => {
            host(x, op, x.a, b, HANDLE.with(|h| h.as_ptr() as usize as i64), f)
        }
        EE_UNITS_UC_HANDLE => i64::from(HANDLE.get().mValue),
        // the closest tile of the point f (from b, c) into the cell (its address in a), then its x and y
        EE_UNITS_UC_CLOSEST_TILE => {
            TILE.set([b as c_long, c as c_long]);
            host(x, op, TILE.with(|t| t.as_ptr() as usize as u64), b, c, f)
        }
        EE_UNITS_UC_TILE_OUT_X => TILE.get()[0] as i64,
        EE_UNITS_UC_TILE_OUT_Y => TILE.get()[1] as i64,
        // `EEUpdateUnitTile(a, theUnit, &x, &y)` (its body is Rust: uu_body.rs)
        EE_UNITS_UC_UPDATE_UNIT_TILE => {
            let tile = TILE.with(|t| t.as_ptr() as usize);
            let y = tile + std::mem::size_of::<c_long>();
            // SAFETY: the body's own entry point, with the transformation and unit the member passed on
            unsafe {
                crate::units::uu_body::game::ee_units_ub_body(
                    crate::units::uu_body::EE_UNITS_UB_DO_UPDATE_UNIT_TILE,
                    x.a,
                    x.unit,
                    tile as i64,
                    y as i64,
                    0,
                )
            };
            0
        }
        // the turret part into the cell (its address in a), then the part
        EE_UNITS_UC_FIND_TURRET_PART => host(x, op, PART.with(|p| p.as_ptr() as usize as u64), b, c, f),
        EE_UNITS_UC_TURRET_PART => PART.get() as i64,
        // `CreateUnitAt(*b, theUnit->GetPlayer()->GetPlayerID(), false, theUnit)`: CREATE_AT_BY's statement
        EE_UNITS_UC_CREATE_AT => {
            let player = x.u()?.player()?.id() as c_long as i64;
            host(x, EE_UNITS_UC_CREATE_AT_BY, x.a, b, player, f)
        }
        // `theOther->CanGather(b, b->GetTileX(), b->GetTileY(), false, true)` (its body is Rust: eu_body.rs)
        EE_UNITS_UC_CAN_GATHER => {
            // SAFETY: the resource the body handed over
            let (tx, ty) = unsafe { UnitView::of(x.t, b as u64) }?.tile_xy();
            let p = |h: u64| h as usize as *mut c_void;
            // SAFETY: the member's call
            i64::from(unsafe { crate::units::eu_body::game::ee_units_eu_can_gather(p(x.a), p(b as u64), tx, ty, 0, 1) } != 0)
        }
        // effect a's `Covers(owner's player id, unit b, the owner's location's x and y)` (its owner `mUnit` from the
        // C++ host)
        EE_UNITS_UC_EFFECT_COVERS => {
            let owner = host(x, EE_UNITS_UC_EFFECT_OWNER, x.a, 0, 0, &mut [0.0; 4]) as u64;
            // SAFETY: the effect's live owner
            let o = unsafe { UnitView::of(x.t, owner) }?;
            let (player, at) = (u64::from(o.player()?.id()), o.location());
            // SAFETY: the member's call on the live effect and unit
            i64::from(unsafe { ee_screens_ae_covers(x.a as usize, player, b as usize, at[0], at[1]) })
        }
        // ProduceUnit's ship and flier: *c = unit a's location at the placement *b's yaw (no pitch or roll), set
        EE_UNITS_UC_ALIGN_YAW => {
            let o = x.o()?;
            // SAFETY: the body's two transformations (its buffers)
            let (placement, adjusted) =
                unsafe { (&*(b as usize as *const GeTransformation), &mut *(c as usize as *mut GeTransformation)) };
            *adjusted = GeTransformation { m: o.transform() };
            let yaw = placement.orientation().yaw;
            adjusted.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
            let p = |h: u64| h as usize as *mut c_void;
            // SAFETY: `SetTransformation(adjusted, true)`'s entry point (u12a_small.rs)
            unsafe {
                crate::units::u12a_small::game::ee_units_cu_tiny(
                    crate::units::u12a_small::EE_UNITS_CU_T_DO_SET_TRANSFORMATION,
                    p(x.a),
                    c as usize as *const c_void,
                    1,
                    0,
                    adjusted.m[0][3],
                    adjusted.m[1][3],
                    0.0,
                )
            };
            0
        }
        _ => return None,
    })
}
