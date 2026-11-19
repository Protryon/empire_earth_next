//! TEST ONLY: the combat formulas' sweep of the simulation capture (was EESim.cpp's `EECombatCaptureSweep`; the
//! formulas are ee_sim::combat's). The computer players' games never regenerate, heal or convert, and seldom test the
//! range to hit, so with EE_SIM_CAPTURE on, every five game minutes, this calls those formulas (and the reach and
//! damage effect tests) over each player's newest 64 living units and their neighbours: real stats, distances and
//! timers, plus fixed variations. It reads the world through the views and writes nothing back; each call's record
//! is the C++ wrappers' (ee/EECombatFormulas.h: the tag, the input's bytes, the output's).
//!
//! `EECanHeal` (CBCH: no game heals) runs over each pair both ways, a unit and itself, and a unit and the previous
//! player's newest unit: a host call (the C++ records it). A unit's position (`GetPosition`, virtual) and the
//! projectile class's acceleration are host reads.

use std::ffi::c_void;
use std::os::raw::c_long;

use ee_sim::app::capture;
use ee_sim::combat::ffi::*;
use ee_sim::combat::*;
use ee_sim::world::{UnitView, WorldRead, WorldView};
use ee_sim::world_field;

/// `EECanHeal(a, b)` (the C++ records CBCH)
pub const EE_SIM_DRIVER_COMBAT_CAN_HEAL: u32 = 1;
/// `a->GetPosition` into `out[0..3]`
pub const EE_SIM_DRIVER_COMBAT_POSITION: u32 = 2;
/// The projectile class of the stats `a`'s class: 1 and its `GetAcceleration()` in `out[0]`, or 0 without one
pub const EE_SIM_DRIVER_COMBAT_PROJECTILE: u32 = 3;

/// The sweep's inputs from the C++: the physics' tick, the rules' and the world table's numbers, the host.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeSimDriverCombatSweep {
    /// `gPhysics->mTickTimeMs`
    pub tick_ms: i32,
    /// `gPhysics->mTickSeconds`
    pub tick_seconds: f32,
    /// `EEGetRules().mCombat.mRegenerationInterval`
    pub regeneration_interval: i32,
    /// `gWorldTable.GetLongValue(kWorldConversionFaithCost)`
    pub conversion_faith_cost: i32,
    pub ctx: *mut c_void,
    /// one `EE_SIM_DRIVER_COMBAT_*` call over two handles; its answer
    pub host: unsafe extern "C" fn(ctx: *mut c_void, op: u32, a: u64, b: u64, out: *mut f32) -> i32,
}

fn tag(name: &[u8; 4]) -> u32 {
    u32::from_le_bytes(*name)
}

fn bytes<T>(value: &T) -> &[u8] {
    // SAFETY: a plain record of 4-byte fields (no padding): its bytes
    unsafe { std::slice::from_raw_parts((value as *const T).cast::<u8>(), std::mem::size_of::<T>()) }
}

/// One call's record, as `EECombatCapture` writes it.
fn record<I, O>(name: &[u8; 4], input: &I, output: &O) {
    if !capture::ee_app_capture_active() {
        return;
    }
    let mut payload = bytes(input).to_vec();
    payload.extend_from_slice(bytes(output));
    capture::commit(tag(name), &payload);
}

/// `EEUnit::DistanceSquaredTo`: the squared map distance, y difference first.
pub fn distance_squared(a: [f32; 3], b: [f32; 3]) -> f32 {
    let dy = a[1] - b[1];
    let dx = a[0] - b[0];
    dx * dx + dy * dy
}

struct Host<'a>(&'a EeSimDriverCombatSweep);

impl Host<'_> {
    fn call(&self, op: u32, a: u64, b: u64, out: &mut [f32; 3]) -> i32 {
        // SAFETY: the C++'s host over its own world
        unsafe { (self.0.host)(self.0.ctx, op, a, b, out.as_mut_ptr()) }
    }
    fn can_heal(&self, a: u64, b: u64) {
        self.call(EE_SIM_DRIVER_COMBAT_CAN_HEAL, a, b, &mut [0.0; 3]);
    }
    fn position(&self, a: u64) -> [f32; 3] {
        let mut out = [0.0; 3];
        self.call(EE_SIM_DRIVER_COMBAT_POSITION, a, 0, &mut out);
        out
    }
    fn projectile(&self, stats: u64) -> Option<f32> {
        let mut out = [0.0; 3];
        (self.call(EE_SIM_DRIVER_COMBAT_PROJECTILE, stats, 0, &mut out) != 0).then_some(out[0])
    }
}

/// The formulas over the pair (a, b): a's stats, b's stats, the distance.
fn pair(h: &Host, a: UnitView, b: UnitView, other: Option<u64>) {
    let i = h.0;
    let (Some(s), Some(t)) = (a.stats(), b.stats()) else { return };
    let (Some(ac), Some(bc)) = (a.class(), b.class()) else { return };
    h.can_heal(a.handle(), b.handle());
    h.can_heal(b.handle(), a.handle());
    h.can_heal(a.handle(), a.handle());
    if let Some(o) = other {
        h.can_heal(a.handle(), o);
    }
    let sf = s.fields();
    let tf = t.fields();
    let af = ac.fields();
    let bf = bc.fields();
    let hit_points = a.hit_points() as i32;
    let max = s.max_hit_points() as i32;
    let complex = a.complex();
    let regeneration_timer: c_long = world_field!(complex, mRegenerationTimer);
    let work_timer: c_long = world_field!(complex, mWorkTimer);
    let regeneration: c_long = world_field!(sf, mRegeneration);
    let timers = [regeneration_timer as i32, 0, i.tick_ms, 1000];
    let hits = [hit_points, 0, max, max / 3 + 1];
    for timer in timers {
        let t_in = EeCombatTimerIn { timer, tick_ms: i.tick_ms, interval: i.regeneration_interval };
        // SAFETY (each formula call): a local record
        record(b"CBTM", &t_in, &unsafe { ee_combat_timer(&t_in) });
        for hp in hits {
            let mut r = EeCombatRegenerateIn {
                timer,
                tick_ms: i.tick_ms,
                hit_points: hp,
                regeneration: regeneration as i32,
                max_hit_points: max,
                interval: i.regeneration_interval,
            };
            record(b"CBRG", &r, &unsafe { ee_combat_regenerate(&r) });
            r.regeneration = 3;
            record(b"CBRG", &r, &unsafe { ee_combat_regenerate(&r) });
        }
    }
    let healing: c_long = world_field!(tf, mHealingPoints);
    let t_max = t.max_hit_points() as i32;
    for hp in hits {
        let mut heal = EeCombatHealIn { hit_points: hp, healing: healing as i32, max_hit_points: max };
        record(b"CBHL", &heal, &unsafe { ee_combat_heal(&heal) });
        heal.healing = 7;
        record(b"CBHL", &heal, &unsafe { ee_combat_heal(&heal) });
        let e = EeCombatDamageEffectIn {
            old_hit_points: hp,
            hit_points: hp.wrapping_sub(t_max / 4),
            max_hit_points: max,
            threshold: 0.5,
        };
        record(b"CBDE", &e, &unsafe { ee_combat_damage_effect(&e) });
    }
    for elapsed in [0u32, 900, 999, 1000] {
        let mut step = EeCombatHealStepIn { elapsed, tick_ms: i.tick_ms };
        record(b"CBHS", &step, &unsafe { ee_combat_heal_step(&step) });
        step.tick_ms = 0;
        record(b"CBHS", &step, &unsafe { ee_combat_heal_step(&step) });
    }
    let power: c_long = world_field!(sf, mPower);
    let cost = EeCombatConversionCostIn { cost: i.conversion_faith_cost, power: power as i32 };
    record(b"CBCC", &cost, &unsafe { ee_combat_conversion_cost(&cost) });
    let resistance: f32 = world_field!(tf, mConversionResistance);
    let rate: f32 = world_field!(sf, mConversionRate);
    let factor: f32 = b.player().map(|p| world_field!(p.fields(), mConversionFactor)).unwrap_or(0.0);
    for work in [work_timer as i32, 950, 999, -5] {
        for adaptation in 0..2 {
            let mut v = EeCombatConversionIn {
                work_timer: work,
                tick_seconds: i.tick_seconds,
                resistance,
                adaptation,
                rate,
                factor,
            };
            record(b"CBCV", &v, &unsafe { ee_combat_conversion(&v) });
            v.resistance = 1.0;
            v.rate = 0.4;
            record(b"CBCV", &v, &unsafe { ee_combat_conversion(&v) });
        }
    }
    let distance = distance_squared(a.location(), b.location());
    let heal_range: f32 = world_field!(af, mHealRange);
    let range: f32 = world_field!(sf, mRange);
    for r in [heal_range, range] {
        record(b"CBHR", &[distance, r], &ee_combat_within_range(distance, r));
    }
    let b_inner: f32 = world_field!(bf, mInnerCollisionSphere);
    for crusader in 0..2 {
        let g = EeCombatRangeIn {
            range,
            heal_range,
            crusader,
            distance_squared: distance,
            building: bc.land_building() as i32,
            inner_sphere: b_inner,
        };
        record(b"CBRH", &g, &unsafe { ee_combat_range(&g) });
    }
    let a_inner: f32 = world_field!(af, mInnerCollisionSphere);
    for flags in 0..4 {
        let q = EeCombatReachIn {
            distance_squared: distance,
            radius: a_inner,
            other_radius: b_inner,
            scale: 0.0,
            flag_a: flags & 1,
            flag_b: flags >> 1,
        };
        record(b"CBRW", &q, &unsafe { ee_combat_work_reach(&q) });
        record(b"CBRT", &q, &unsafe { ee_combat_gather_reach(&q) });
    }
    if let Some(acceleration) = h.projectile(s.handle()) {
        let from = h.position(a.handle());
        let to = h.position(b.handle());
        let speed: f32 = world_field!(sf, mLaunchSpeed);
        for high in 0..2 {
            let p = EeCombatBallisticIn {
                from_x: from[0],
                from_y: from[1],
                from_z: from[2],
                to_x: to[0],
                to_y: to[1],
                to_z: to[2],
                speed,
                acceleration,
                high,
            };
            record(b"CBBP", &p, &unsafe { ee_combat_ballistic(&p) });
        }
    }
}

/// The sweep (EE-SIM's capture, every five game minutes, under the world).
///
/// # Safety
/// `input` points to a valid record whose host answers for the world the caller holds.
#[no_mangle]
pub unsafe extern "C" fn ee_sim_driver_combat_sweep(input: *const EeSimDriverCombatSweep) {
    // SAFETY: the caller's contract
    let h = Host(unsafe { &*input });
    // SAFETY: the caller holds the world
    let token = unsafe { WorldRead::begin() };
    let Some(world) = WorldView::global(&token) else { return };
    let mut other: Option<u64> = None;
    for p in 1..world.num_players() as usize {
        let Some(player) = world.player(p) else { continue };
        let all: Vec<UnitView> = player
            .units()
            .filter(|u| u.hit_points() > 0 && u.stats().is_some())
            .filter_map(|u| u.complex_unit())
            .collect();
        let first = all.len().saturating_sub(64);
        for k in first..all.len().saturating_sub(1) {
            pair(&h, all[k], all[k + 1], other);
        }
        if let Some(last) = all.last() {
            other = Some(last.handle());
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tags_and_distance() {
        assert_eq!(tag(b"CBTM"), 0x4d54_4243);
        assert_eq!(distance_squared([3.0, 4.0, 9.0], [0.0, 0.0, -2.0]), 25.0);
        let r = EeCombatTimerIn { timer: 1, tick_ms: 2, interval: 3 };
        assert_eq!(bytes(&r), &[1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0]);
        assert_eq!(bytes(&[1.0f32, 2.0]).len(), 8);
    }
}
