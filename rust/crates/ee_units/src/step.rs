//! The smallest leaves of a unit's per-tick step (RUST_PLAN R5b.4 leaf 6, "the next three rounds" 1): the
//! countdowns a class's `Update` runs every tick (`EEComplexUnit::UpdateReloadTimer`, `UpdateTargetSearchTimer`,
//! `UpdateHealSearchTimer`, the rotting countdown) and `EEUnitClass::RegeneratePower`, over the unit store's record.
//!
//! Each leaf reads the unit's record and returns what the C++ writes back through the member's holder (which reports
//! it to the store again): no leaf writes the store itself, so the holder stays the one writer.
//!
//! Determinism (RUST_PLAN 2.1): the C++ `long` arithmetic as wrapping `i64` (the values are game milliseconds, far
//! from either width's limit, so 32 and 64-bit `long` agree); the heal timer's one `f32` subtraction and division in
//! the C++ order; no random draw.

use crate::store::EeUnitRecord;

/// `UpdateReloadTimer` / `UpdateTargetSearchTimer`: a timer above 0 counted down by the tick, not below 0; `None`
/// when it is not above 0 (the C++ returns without a write).
pub fn countdown(timer: i64, tick_ms: i64) -> Option<i64> {
    if timer <= 0 {
        return None;
    }
    Some(timer.wrapping_sub(tick_ms).max(0))
}

/// `UpdateHealSearchTimer`: seconds, counted down by `tick_time / 1000` (the tick in milliseconds as a float), not
/// below 0; `None` when it is not above 0 (NaN included, as the C++'s `!(v > 0)`).
pub fn heal_countdown(timer: f32, tick_time: f32) -> Option<f32> {
    // the C++'s `!(theValue > 0.0f)`: a NaN returns too
    if timer.partial_cmp(&0.0) != Some(std::cmp::Ordering::Greater) {
        return None;
    }
    let value = timer - tick_time / 1000.0f32;
    Some(if value < 0.0 { 0.0 } else { value })
}

/// The rotting countdown of a class's `Update` (`kActionUnitRotting`): the time left minus the tick, not below 0
/// (always written).
pub fn rot_countdown(time: i64, tick_ms: i64) -> i64 {
    time.wrapping_sub(tick_ms).max(0)
}

/// What `RegeneratePower` reads besides the unit's record: the unit's stats (`mStats->mPowerRegeneration`,
/// `->mPower`), the world's cinematic flag, the rules' interval and the tick.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeUnitsPowerIn {
    pub regeneration: i64,
    pub max_power: i64,
    pub in_cinematic: u8,
    pub interval: i64,
    pub tick_ms: i64,
}

/// What `RegeneratePower` writes: `write` 0 when it returns early (nothing written); else the power regeneration
/// timer, and the power when `set_power` is 1 (the timer ran out).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeUnitsPowerOut {
    pub write: u8,
    pub set_power: u8,
    pub power: i64,
    pub regen_timer: i64,
}

/// `EEUnitClass::RegeneratePower` over the unit's record (its power, its regeneration timer, whether it is built,
/// its class's land-building flag).
pub fn regenerate_power(r: &EeUnitRecord, inp: &EeUnitsPowerIn) -> EeUnitsPowerOut {
    let mut out = EeUnitsPowerOut::default();
    if inp.regeneration <= 0 || inp.in_cinematic != 0 || (r.land_building != 0 && r.built == 0) {
        return out;
    }
    out.write = 1;
    let mut remaining = r.power_regen_timer.wrapping_sub(inp.tick_ms);
    if remaining <= 0 {
        let mut value = inp.regeneration.wrapping_add(r.power);
        if value > inp.max_power {
            value = inp.max_power;
        }
        out.set_power = 1;
        out.power = value;
        remaining = inp.interval;
    }
    out.regen_timer = remaining;
    out
}

/// The leaves' reads of the store. A stepped unit can be out of the store (a killed unit leaves its player's list
/// before it leaves the stepped list: USCK counted up to 4 in seed 2's game), so the C++ hands the member's value
/// too: the record's is used when the record is that object's, and a record that differs from the member is
/// counted (`mismatches`: USCK adds them; the holders keep the two equal).
#[derive(Default)]
pub struct Reads {
    pub mismatches: u32,
    pub unrecorded: u32,
}

/// The record of `object` under `id`, if any.
fn record_of(store: &crate::store::Store, id: u32, object: u64) -> Option<&EeUnitRecord> {
    store.get(id).filter(|r| r.object == object)
}

/// The kinds of countdown (`ee_units_step_countdown`'s `which`).
pub const EE_UNITS_STEP_RELOAD: u32 = 1;
pub const EE_UNITS_STEP_TARGET_SEARCH: u32 = 2;

/// `UpdateReloadTimer` (`which` 1) / `UpdateTargetSearchTimer` (2) over the store: the new timer, `None` for no
/// write. `member` is the C++ member's value.
pub fn step_countdown(
    store: &crate::store::Store,
    reads: &mut Reads,
    id: u32,
    object: u64,
    which: u32,
    member: i64,
    tick_ms: i64,
) -> Option<i64> {
    let timer = match record_of(store, id, object) {
        Some(r) => {
            let v = if which == EE_UNITS_STEP_RELOAD { r.reload_timer } else { r.target_search_timer };
            reads.mismatches += u32::from(v != member);
            v
        }
        None => {
            reads.unrecorded += 1;
            member
        }
    };
    countdown(timer, tick_ms)
}

/// `UpdateHealSearchTimer` over the store (`member` the C++ member's value).
pub fn step_heal(
    store: &crate::store::Store,
    reads: &mut Reads,
    id: u32,
    object: u64,
    member: f32,
    tick_time: f32,
) -> Option<f32> {
    let timer = match record_of(store, id, object) {
        Some(r) => {
            reads.mismatches += u32::from(r.heal_search_timer != member.to_bits());
            f32::from_bits(r.heal_search_timer)
        }
        None => {
            reads.unrecorded += 1;
            member
        }
    };
    heal_countdown(timer, tick_time)
}

/// `RegeneratePower` over the store: `member` holds the C++ members' power, timer, built and land-building flags
/// (the record used instead when there is one).
pub fn step_power(
    store: &crate::store::Store,
    reads: &mut Reads,
    id: u32,
    object: u64,
    member: &EeUnitRecord,
    inp: &EeUnitsPowerIn,
) -> EeUnitsPowerOut {
    match record_of(store, id, object) {
        Some(r) => {
            let same = r.power == member.power
                && r.power_regen_timer == member.power_regen_timer
                && r.built == member.built
                && r.land_building == member.land_building;
            reads.mismatches += u32::from(!same);
            regenerate_power(r, inp)
        }
        None => {
            reads.unrecorded += 1;
            regenerate_power(member, inp)
        }
    }
}

/// The leaves' read counts since the process started (TEST ONLY: USCK reads them).
pub static READS: std::sync::Mutex<Reads> = std::sync::Mutex::new(Reads { mismatches: 0, unrecorded: 0 });

fn with_reads<R>(f: impl FnOnce(&mut Reads) -> R) -> R {
    let mut guard = READS.lock().unwrap_or_else(|e| e.into_inner());
    f(&mut guard)
}

/// `UpdateReloadTimer` / `UpdateTargetSearchTimer` (`which`: `EE_UNITS_STEP_*`) over the store: true and the new
/// timer in `out` when the C++ writes it.
///
/// # Safety
/// `out` points to a writable i64.
#[no_mangle]
pub unsafe extern "C" fn ee_units_step_countdown(
    id: u32,
    object: u64,
    which: u32,
    member: i64,
    tick_ms: i64,
    out: *mut i64,
) -> bool {
    let r =
        crate::store::with_store(|s| with_reads(|reads| step_countdown(s, reads, id, object, which, member, tick_ms)));
    match r {
        Some(v) => {
            // SAFETY: the caller's contract
            unsafe { *out = v };
            true
        }
        None => false,
    }
}

/// `UpdateHealSearchTimer` over the store: true and the new timer in `out` when the C++ writes it.
///
/// # Safety
/// `out` points to a writable f32.
#[no_mangle]
pub unsafe extern "C" fn ee_units_step_heal(id: u32, object: u64, member: f32, tick_time: f32, out: *mut f32) -> bool {
    let r = crate::store::with_store(|s| with_reads(|reads| step_heal(s, reads, id, object, member, tick_time)));
    match r {
        Some(v) => {
            // SAFETY: the caller's contract
            unsafe { *out = v };
            true
        }
        None => false,
    }
}

/// `RegeneratePower` over the store (`member_*`: the C++ members, used when the unit has no record).
///
/// # Safety
/// `inp` points to a valid input, `out` to a writable output.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_step_power(
    id: u32,
    object: u64,
    member_power: i64,
    member_timer: i64,
    member_built: u8,
    member_land_building: u8,
    inp: *const EeUnitsPowerIn,
    out: *mut EeUnitsPowerOut,
) {
    let member = EeUnitRecord {
        power: member_power,
        power_regen_timer: member_timer,
        built: member_built,
        land_building: member_land_building,
        ..Default::default()
    };
    // SAFETY: the caller's contract
    let inp = unsafe { *inp };
    let r = crate::store::with_store(|s| with_reads(|reads| step_power(s, reads, id, object, &member, &inp)));
    // SAFETY: the caller's contract
    unsafe { *out = r };
}

/// TEST ONLY: the leaves' reads whose record differed from the member, and those of units with no record.
///
/// # Safety
/// Both point to writable u32s.
#[no_mangle]
pub unsafe extern "C" fn ee_units_step_reads(out_mismatches: *mut u32, out_unrecorded: *mut u32) {
    let (m, u) = with_reads(|r| (r.mismatches, r.unrecorded));
    // SAFETY: the caller's contract
    unsafe {
        *out_mismatches = m;
        *out_unrecorded = u;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::store::Store;

    #[test]
    fn the_store_leaves_read_the_record_and_count_differences() {
        let mut s = Store::new();
        s.insert(EeUnitRecord {
            id: 7,
            object: 0x70,
            reload_timer: 500,
            target_search_timer: 0,
            heal_search_timer: 1.0f32.to_bits(),
            power: 10,
            power_regen_timer: 100,
            built: 1,
            ..Default::default()
        });
        let mut reads = Reads::default();
        assert_eq!(step_countdown(&s, &mut reads, 7, 0x70, EE_UNITS_STEP_RELOAD, 500, 125), Some(375));
        assert_eq!(step_countdown(&s, &mut reads, 7, 0x70, EE_UNITS_STEP_TARGET_SEARCH, 0, 125), None);
        assert_eq!((reads.mismatches, reads.unrecorded), (0, 0));
        // the record's value wins over a member that differs, and the difference is counted
        assert_eq!(step_countdown(&s, &mut reads, 7, 0x70, EE_UNITS_STEP_RELOAD, 900, 125), Some(375));
        assert_eq!(reads.mismatches, 1);
        // another object under the id, or no record: the member's value
        assert_eq!(step_countdown(&s, &mut reads, 7, 0x99, EE_UNITS_STEP_RELOAD, 900, 125), Some(775));
        assert_eq!(step_countdown(&s, &mut reads, 8, 0x80, EE_UNITS_STEP_RELOAD, 50, 125), Some(0));
        assert_eq!(reads.unrecorded, 2);
        assert_eq!(step_heal(&s, &mut reads, 7, 0x70, 1.0, 125.0), Some(1.0 - 125.0 / 1000.0));
        let inp = EeUnitsPowerIn { regeneration: 5, max_power: 100, in_cinematic: 0, interval: 5000, tick_ms: 125 };
        let member = EeUnitRecord { power: 10, power_regen_timer: 100, built: 1, ..Default::default() };
        let o = step_power(&s, &mut reads, 7, 0x70, &member, &inp);
        assert_eq!((o.write, o.set_power, o.power, o.regen_timer), (1, 1, 15, 5000));
        assert_eq!(reads.mismatches, 1);
    }

    #[test]
    fn countdowns_stop_at_zero_and_skip_idle_timers() {
        assert_eq!(countdown(0, 125), None);
        assert_eq!(countdown(-5, 125), None);
        assert_eq!(countdown(1000, 125), Some(875));
        assert_eq!(countdown(100, 125), Some(0));
        assert_eq!(countdown(125, 125), Some(0));
        assert_eq!(rot_countdown(0, 125), 0);
        assert_eq!(rot_countdown(300, 125), 175);
        assert_eq!(rot_countdown(-7, 125), 0);
    }

    #[test]
    fn the_heal_timer_counts_seconds_in_the_cs_float_order() {
        assert_eq!(heal_countdown(0.0, 125.0), None);
        assert_eq!(heal_countdown(-0.0, 125.0), None);
        assert_eq!(heal_countdown(f32::NAN, 125.0), None);
        // 2.0 - 125/1000 in f32: the division first, then the subtraction
        let want = 2.0f32 - 125.0f32 / 1000.0f32;
        assert_eq!(heal_countdown(2.0, 125.0).unwrap().to_bits(), want.to_bits());
        assert_eq!(heal_countdown(0.1, 125.0), Some(0.0));
        // a tick that is not a whole millisecond (mTickTime is a float)
        let want = 0.7f32 - 124.99f32 / 1000.0f32;
        assert_eq!(heal_countdown(0.7, 124.99).unwrap().to_bits(), want.to_bits());
    }

    fn unit(power: i64, timer: i64) -> EeUnitRecord {
        EeUnitRecord { power, power_regen_timer: timer, built: 1, ..Default::default() }
    }

    #[test]
    fn regenerate_power_follows_the_originals_branches() {
        let inp = EeUnitsPowerIn { regeneration: 5, max_power: 100, in_cinematic: 0, interval: 5000, tick_ms: 125 };
        // the timer runs
        let o = regenerate_power(&unit(40, 1000), &inp);
        assert_eq!((o.write, o.set_power, o.regen_timer), (1, 0, 875));
        // it runs out: the power up by the rate, the timer back to the interval
        let o = regenerate_power(&unit(40, 125), &inp);
        assert_eq!((o.write, o.set_power, o.power, o.regen_timer), (1, 1, 45, 5000));
        // capped at the maximum (even from above it)
        let o = regenerate_power(&unit(98, 0), &inp);
        assert_eq!((o.set_power, o.power), (1, 100));
        let o = regenerate_power(&unit(130, -50), &inp);
        assert_eq!((o.set_power, o.power), (1, 100));
        // no regeneration, a cinematic, an unbuilt land building: nothing written
        let none = EeUnitsPowerOut::default();
        assert_eq!(regenerate_power(&unit(40, 1000), &EeUnitsPowerIn { regeneration: 0, ..inp }), none);
        assert_eq!(regenerate_power(&unit(40, 1000), &EeUnitsPowerIn { in_cinematic: 1, ..inp }), none);
        let site = EeUnitRecord { land_building: 1, built: 0, ..unit(40, 1000) };
        assert_eq!(regenerate_power(&site, &inp), none);
        // a built land building regenerates
        let built = EeUnitRecord { land_building: 1, ..unit(40, 1000) };
        assert_eq!(regenerate_power(&built, &inp).write, 1);
    }
}
