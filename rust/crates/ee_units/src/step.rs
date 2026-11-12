//! The smallest leaves of a unit's per-tick step (RUST_PLAN R5b.4 leaf 6, "the next three rounds" 1): the
//! countdowns a class's `Update` runs every tick (`EEComplexUnit::UpdateReloadTimer`, `UpdateTargetSearchTimer`,
//! `UpdateHealSearchTimer`, the rotting countdowns `UpdateRottingAwayTime` and `UpdateResourcesRotting`),
//! `EEUnitClass::RegeneratePower` and `UpdateCamouflageTimer`'s arithmetic (the timers as a decision whose events the
//! C++ applies: the mini map moves, the cloak chain's setters, the writes; the cloak fade), over the unit store's
//! record.
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
pub(crate) fn record_of(store: &crate::store::Store, id: u32, object: u64) -> Option<&EeUnitRecord> {
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

pub(crate) fn with_reads<R>(f: impl FnOnce(&mut Reads) -> R) -> R {
    let mut guard = READS.lock().unwrap_or_else(|e| e.into_inner());
    f(&mut guard)
}

/// Counts one read: `Some(differs)` for a unit with a record (whether it differed from the members), `None` for one
/// without.
pub(crate) fn count_read(differs: Option<bool>) {
    with_reads(|r| match differs {
        Some(d) => r.mismatches += u32::from(d),
        None => r.unrecorded += 1,
    })
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

// ---- the eleventh round: the rotting countdowns, the camouflage timers, the cloak fade

/// The resources' rotting (`kActionResourcesRotting`): the time plus the tick; past 4 s the resource amount loses one
/// per whole 4 s (not below 0) and the time keeps the rest. Returns the time and, past 4 s, the amount.
pub fn resources_rot(time: i64, amount: i64, tick_ms: i64) -> (i64, Option<i64>) {
    let mut t = time.wrapping_add(tick_ms);
    let mut a = None;
    if t > 4000 {
        let mut v = amount.wrapping_sub(t / 4000);
        if v < 0 {
            v = 0;
        }
        a = Some(v);
        t %= 4000;
    }
    (t, a)
}

/// `UpdateRottingAwayTime` over the store (`member`: the C++ member's time): the new time.
pub fn step_rotting(
    store: &crate::store::Store,
    reads: &mut Reads,
    id: u32,
    object: u64,
    member: i64,
    tick_ms: i64,
) -> i64 {
    let time = match record_of(store, id, object) {
        Some(r) => {
            reads.mismatches += u32::from(r.rotting_away_time != member);
            r.rotting_away_time
        }
        None => {
            reads.unrecorded += 1;
            member
        }
    };
    rot_countdown(time, tick_ms)
}

/// `UpdateResourcesRotting` over the store (`member_*`: the C++ members' time and resource amount).
pub fn step_resources_rotting(
    store: &crate::store::Store,
    reads: &mut Reads,
    id: u32,
    object: u64,
    member_time: i64,
    member_amount: i64,
    tick_ms: i64,
) -> (i64, Option<i64>) {
    let (time, amount) = match record_of(store, id, object) {
        Some(r) => {
            reads.mismatches += u32::from(r.rotting_away_time != member_time || r.resource_amount != member_amount);
            (r.rotting_away_time, r.resource_amount)
        }
        None => {
            reads.unrecorded += 1;
            (member_time, member_amount)
        }
    };
    resources_rot(time, amount, tick_ms)
}

/// The camouflage decision's events (`EeUnitsCamoEvent::code`), in the order the C++ applies them: a mini map move,
/// made only when the unit is then a cloaked enemy of the local player (`EEIsCloakedEnemyOf`, the C++'s test at the
/// event), from nowhere to its tile (`SHOW`) or back (`HIDE`); the chain setters; the camouflaged bit; the two timers'
/// writes (`value`).
pub const EE_UNITS_CAMO_SHOW: u32 = 1;
pub const EE_UNITS_CAMO_HIDE: u32 = 2;
pub const EE_UNITS_CAMO_CLOAKABLE_OFF: u32 = 3;
pub const EE_UNITS_CAMO_CLOAKABLE_ON: u32 = 4;
pub const EE_UNITS_CAMO_CLOAKED_OFF: u32 = 5;
pub const EE_UNITS_CAMO_CLOAKED_ON: u32 = 6;
pub const EE_UNITS_CAMO_CAMOUFLAGED_OFF: u32 = 7;
pub const EE_UNITS_CAMO_CAMOUFLAGED_ON: u32 = 8;
pub const EE_UNITS_CAMO_SET_TIMER: u32 = 9;
pub const EE_UNITS_CAMO_SET_SECOND: u32 = 10;
/// The most events one call makes (two blocks of five).
pub const EE_UNITS_CAMO_MAX_EVENTS: usize = 16;

/// One event of the camouflage decision.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeUnitsCamoEvent {
    pub code: u32,
    pub value: u32,
}

fn ev(code: u32, value: u32) -> EeUnitsCamoEvent {
    EeUnitsCamoEvent { code, value }
}

/// The original's `long theTime = <unsigned long> - gPhysics->mTickTimeMs` in the Windows build's 32-bit `long`s
/// (the unsigned difference, read signed).
fn left(timer: u32, tick_ms: i64) -> i32 {
    timer.wrapping_sub(tick_ms as u32) as i32
}

/// The losing of the camouflage without vital buildings (both blocks' first branch).
fn drop_camouflage(out: &mut Vec<EeUnitsCamoEvent>) {
    out.push(ev(EE_UNITS_CAMO_SHOW, 0));
    out.push(ev(EE_UNITS_CAMO_CLOAKABLE_OFF, 0));
    out.push(ev(EE_UNITS_CAMO_CLOAKED_OFF, 0));
    out.push(ev(EE_UNITS_CAMO_SET_TIMER, 0));
    out.push(ev(EE_UNITS_CAMO_SET_SECOND, 0));
}

/// `EEUnitClass::UpdateCamouflageTimer`'s timers (0x63978b's first part): the camouflage timer (its top bit the
/// camouflaged flag) counts down to the unit's camouflage, the second timer (`mU32C`) to its end; a player without
/// vital buildings loses both. `no_vital`: `mNumVitalBuildings < 1` (no event changes it). Returns the events in the
/// C++'s statement order.
pub fn camouflage(timer: u32, second: u32, no_vital: bool, tick_ms: i64) -> Vec<EeUnitsCamoEvent> {
    let mut out = Vec::new();
    let mut second = second;
    let remaining = timer & 0x7fff_ffff;
    if remaining > 0 {
        if no_vital {
            drop_camouflage(&mut out);
            second = 0;
        } else {
            let t = left(remaining, tick_ms);
            if t > 0 {
                out.push(ev(EE_UNITS_CAMO_SET_TIMER, t as u32));
                out.push(ev(EE_UNITS_CAMO_CAMOUFLAGED_OFF, 0));
            } else {
                out.push(ev(EE_UNITS_CAMO_SET_TIMER, 0));
                out.push(ev(EE_UNITS_CAMO_CAMOUFLAGED_ON, 0));
                out.push(ev(EE_UNITS_CAMO_CLOAKABLE_ON, 0));
                out.push(ev(EE_UNITS_CAMO_CLOAKED_ON, 0));
                out.push(ev(EE_UNITS_CAMO_SET_SECOND, 0));
                out.push(ev(EE_UNITS_CAMO_HIDE, 0));
                second = 0;
            }
        }
    }
    if second > 0 {
        if no_vital {
            drop_camouflage(&mut out);
        } else {
            let t = left(second, tick_ms);
            if t > 0 {
                out.push(ev(EE_UNITS_CAMO_SET_SECOND, t as u32));
            } else {
                out.push(ev(EE_UNITS_CAMO_SHOW, 0));
                out.push(ev(EE_UNITS_CAMO_SET_SECOND, 0));
                out.push(ev(EE_UNITS_CAMO_CLOAKABLE_OFF, 0));
                out.push(ev(EE_UNITS_CAMO_CLOAKED_OFF, 0));
            }
        }
    }
    out
}

/// `kCloakFadeMaxOpacity`, `kCloakFadeMinOpacity`, `kCloakFadeTime` (EEUnitClass.cpp 0x447ad4, 0x447ad8, 0x447ad0).
const CLOAK_FADE_MAX: f32 = 1.0;
const CLOAK_FADE_MIN: f32 = 0.6;
const CLOAK_FADE_TIME: f32 = 750.0;

/// `EEUnitClass::UpdateCamouflageTimer`'s cloak opacity (after `UpdateCloakDetection`): toward the maximum when not
/// cloaked, toward the minimum when cloaked, by `tick * range * rate` in the C++'s order; a powered capitol of the
/// local player's side with the cloaking calamity (`pulse`, the C++'s test) pulses with the game time instead.
#[allow(clippy::assign_op_pattern)] // the C++'s statements as written (`o = x + o`)
pub fn cloak_fade(opacity: f32, cloaked: bool, tick_time: f32, pulse: bool, game_time: i64) -> f32 {
    // the statics sCloakFadeRate (1 / kCloakFadeTime) and sCloakFadeOpacityRange (max - min), f32 as in the C++
    let rate = 1.0f32 / CLOAK_FADE_TIME;
    let range = CLOAK_FADE_MAX - CLOAK_FADE_MIN;
    let mut o = opacity;
    if !cloaked {
        if o < CLOAK_FADE_MAX {
            o = tick_time * range * rate + o;
        }
        if o > CLOAK_FADE_MAX {
            o = CLOAK_FADE_MAX;
        }
    } else {
        if o > CLOAK_FADE_MIN {
            o = o - tick_time * range * rate;
        }
        if o < CLOAK_FADE_MIN {
            o = CLOAK_FADE_MIN;
        }
    }
    if pulse {
        let mut t = game_time % 2000;
        if t >= 1000 {
            t = 1999 - t;
        }
        o = t as f32 * 0.000_333_333_33f32 + 0.2f32;
    }
    o
}

/// `UpdateCamouflageTimer`'s timers over the store (`member_timer`: the C++ member's camouflage timer; the second
/// timer is not in the store).
#[allow(clippy::too_many_arguments)]
pub fn step_camouflage(
    store: &crate::store::Store,
    reads: &mut Reads,
    id: u32,
    object: u64,
    member_timer: u32,
    second: u32,
    no_vital: bool,
    tick_ms: i64,
) -> Vec<EeUnitsCamoEvent> {
    let timer = match record_of(store, id, object) {
        Some(r) => {
            reads.mismatches += u32::from(r.camouflage_timer != i64::from(member_timer));
            r.camouflage_timer as u32
        }
        None => {
            reads.unrecorded += 1;
            member_timer
        }
    };
    camouflage(timer, second, no_vital, tick_ms)
}

/// `UpdateRottingAwayTime` over the store: the new time (the C++ writes it through the holder).
#[no_mangle]
pub extern "C" fn ee_units_step_rotting(id: u32, object: u64, member: i64, tick_ms: i64) -> i64 {
    crate::store::with_store(|s| with_reads(|reads| step_rotting(s, reads, id, object, member, tick_ms)))
}

/// `UpdateResourcesRotting` over the store: the new time, and true with the new amount in `out_amount` past 4 s.
///
/// # Safety
/// Both outputs point to writable i64s.
#[no_mangle]
pub unsafe extern "C" fn ee_units_step_resources_rotting(
    id: u32,
    object: u64,
    member_time: i64,
    member_amount: i64,
    tick_ms: i64,
    out_time: *mut i64,
    out_amount: *mut i64,
) -> bool {
    let (t, a) = crate::store::with_store(|s| {
        with_reads(|reads| step_resources_rotting(s, reads, id, object, member_time, member_amount, tick_ms))
    });
    // SAFETY: the caller's contract
    unsafe {
        *out_time = t;
        if let Some(a) = a {
            *out_amount = a;
        }
    }
    a.is_some()
}

/// `UpdateCamouflageTimer`'s timers over the store: the events in `out` (room for `EE_UNITS_CAMO_MAX_EVENTS`), their
/// count returned.
///
/// # Safety
/// `out` points to `EE_UNITS_CAMO_MAX_EVENTS` writable events.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_step_camouflage(
    id: u32,
    object: u64,
    member_timer: u32,
    second: u32,
    no_vital: u8,
    tick_ms: i64,
    out: *mut EeUnitsCamoEvent,
) -> u32 {
    let events = crate::store::with_store(|s| {
        with_reads(|reads| step_camouflage(s, reads, id, object, member_timer, second, no_vital != 0, tick_ms))
    });
    let n = events.len().min(EE_UNITS_CAMO_MAX_EVENTS);
    for (i, e) in events.iter().take(n).enumerate() {
        // SAFETY: the caller's contract (n <= EE_UNITS_CAMO_MAX_EVENTS)
        unsafe { *out.add(i) = *e };
    }
    n as u32
}

/// `UpdateCamouflageTimer`'s cloak opacity (the pulse test and the game time handed in).
#[no_mangle]
pub extern "C" fn ee_units_step_cloak_fade(
    opacity: f32,
    cloaked: u8,
    tick_time: f32,
    pulse: u8,
    game_time: i64,
) -> f32 {
    cloak_fade(opacity, cloaked != 0, tick_time, pulse != 0, game_time)
}

#[cfg(test)]
mod round11_tests {
    use super::*;
    use crate::store::Store;

    #[test]
    fn resources_rot_by_whole_four_seconds() {
        assert_eq!(resources_rot(0, 50, 125), (125, None));
        assert_eq!(resources_rot(3875, 50, 125), (4000, None));
        assert_eq!(resources_rot(3900, 50, 125), (25, Some(49)));
        assert_eq!(resources_rot(8000, 50, 125), (125, Some(48)));
        assert_eq!(resources_rot(3900, 0, 125), (25, Some(0)));
    }

    #[test]
    fn the_rotting_leaves_read_the_record() {
        let mut s = Store::new();
        s.insert(EeUnitRecord {
            id: 3,
            object: 0x30,
            rotting_away_time: 1000,
            resource_amount: 7,
            ..Default::default()
        });
        let mut reads = Reads::default();
        assert_eq!(step_rotting(&s, &mut reads, 3, 0x30, 1000, 125), 875);
        assert_eq!(step_rotting(&s, &mut reads, 3, 0x30, 50, 125), 875);
        assert_eq!(reads.mismatches, 1);
        assert_eq!(step_rotting(&s, &mut reads, 4, 0x40, 50, 125), 0);
        assert_eq!(reads.unrecorded, 1);
        assert_eq!(step_resources_rotting(&s, &mut reads, 3, 0x30, 1000, 7, 3125), (125, Some(6)));
    }

    fn codes(v: &[EeUnitsCamoEvent]) -> Vec<(u32, u32)> {
        v.iter().map(|e| (e.code, e.value)).collect()
    }

    #[test]
    fn camouflage_counts_down_then_cloaks() {
        assert!(camouflage(0, 0, false, 125).is_empty());
        // the camouflaged bit alone is no time
        assert!(camouflage(0x8000_0000, 0, true, 125).is_empty());
        assert_eq!(codes(&camouflage(1000, 0, false, 125)), vec![(9, 875), (7, 0)]);
        assert_eq!(codes(&camouflage(0x8000_0000 | 1000, 0, false, 125)), vec![(9, 875), (7, 0)]);
        // ran out: camouflaged, cloaked, the second timer cleared, hidden from the mini map
        assert_eq!(codes(&camouflage(125, 500, false, 125)), vec![(9, 0), (8, 0), (4, 0), (6, 0), (10, 0), (2, 0)]);
    }

    #[test]
    fn camouflage_is_lost_without_vital_buildings() {
        assert_eq!(codes(&camouflage(1000, 0, true, 125)), vec![(1, 0), (3, 0), (5, 0), (9, 0), (10, 0)]);
        // both timers: the first block clears the second, so the second block does nothing
        assert_eq!(codes(&camouflage(1000, 700, true, 125)), vec![(1, 0), (3, 0), (5, 0), (9, 0), (10, 0)]);
        assert_eq!(codes(&camouflage(0, 700, true, 125)), vec![(1, 0), (3, 0), (5, 0), (9, 0), (10, 0)]);
    }

    #[test]
    fn the_second_timer_ends_the_cloak() {
        assert_eq!(codes(&camouflage(0, 700, false, 125)), vec![(10, 575)]);
        assert_eq!(codes(&camouflage(0, 100, false, 125)), vec![(1, 0), (10, 0), (3, 0), (5, 0)]);
        // a second timer past 2^31 reads negative in the 32-bit long: it ends
        assert_eq!(codes(&camouflage(0, 0x9000_0000, false, 125)), vec![(1, 0), (10, 0), (3, 0), (5, 0)]);
    }

    #[test]
    fn the_cloak_fades_in_the_cs_float_order() {
        let rate = 1.0f32 / 750.0f32;
        let range = 1.0f32 - 0.6f32;
        let want = 125.0f32 * range * rate + 0.7f32;
        assert_eq!(cloak_fade(0.7, false, 125.0, false, 0).to_bits(), want.to_bits());
        assert_eq!(cloak_fade(0.99, false, 125.0, false, 0), 1.0);
        let want = 0.9f32 - 125.0f32 * range * rate;
        assert_eq!(cloak_fade(0.9, true, 125.0, false, 0).to_bits(), want.to_bits());
        assert_eq!(cloak_fade(0.61, true, 125.0, false, 0), 0.6);
        // the capitol's pulse: a triangle over 2 s
        assert_eq!(
            cloak_fade(0.7, true, 125.0, true, 4500).to_bits(),
            (500.0f32 * 0.000_333_333_33f32 + 0.2).to_bits()
        );
        assert_eq!(
            cloak_fade(0.7, true, 125.0, true, 5500).to_bits(),
            (499.0f32 * 0.000_333_333_33f32 + 0.2).to_bits()
        );
    }

    #[test]
    fn the_camouflage_leaf_reads_the_record() {
        let mut s = Store::new();
        s.insert(EeUnitRecord { id: 5, object: 0x50, camouflage_timer: 1000, ..Default::default() });
        let mut reads = Reads::default();
        assert_eq!(codes(&step_camouflage(&s, &mut reads, 5, 0x50, 1000, 0, false, 125)), vec![(9, 875), (7, 0)]);
        assert_eq!((reads.mismatches, reads.unrecorded), (0, 0));
        let mut out = [EeUnitsCamoEvent::default(); EE_UNITS_CAMO_MAX_EVENTS];
        // SAFETY: room for the most events
        let n = unsafe { ee_units_step_camouflage(9, 0x90, 1000, 0, 1, 125, out.as_mut_ptr()) };
        assert_eq!(n, 5);
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
