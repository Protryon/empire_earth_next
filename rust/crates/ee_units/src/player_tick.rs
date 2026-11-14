//! The player's tick and reset over its saved record: `EEPlayer::UpdatePlayer` (0x56be3f) and the record's part of
//! `EEPlayer::Reset` (0x565813).
//!
//! `UpdatePlayer` ([`update_player`]) is one sequence over a host callback ([`EeUnitsPlayerTickFn`], its own
//! operation numbers): the selections' validation (one C++ operation: no decision in it), the herds' updates and
//! the end of a herd in state 0xb, the air traffic and the events (one operation), the queued improvements' timers
//! (each entry's count down, the instant build's cap, the improvement applied when it runs out, the next one
//! started or the entry erased) and the wonders' healing (the timer in the record, the heal amount from the world
//! table and the epoch, each healed unit's hit points capped at its maximum). The map of queued improvements and
//! its iterator stay C++: the host walks it in key order as the original did and erases where it erased.
//!
//! [`reset`] sets the record's fields as `Reset` did, in two parts around the events' clear (the trade, refunds and
//! vital buildings were zeroed after it).
//!
//! Determinism: the original's `float` operations in order (the timers less the tick as `float`, the heal product
//! `(epoch / step) * factor * amount` in `float`, truncated as `cvttss2si`), its `long` sums wrapping, no random
//! draw.

#![allow(non_snake_case)]

use std::ffi::c_void;
use std::os::raw::c_ulong;

use crate::player_save::{EeUnitsPlayerSaveIo, EeUnitsPlayerSaved};

/// The host: `(ctx, op, index, io) -> the operation's result`.
pub type EeUnitsPlayerTickFn = unsafe extern "C" fn(*mut c_void, i32, u32, *mut EeUnitsPlayerSaveIo) -> i32;

/// the viewed units released when only the player holds them, the active, selected, saved and running groups
/// validated
pub const EE_UNITS_PLAYER_TICK_VALIDATE: i32 = 1;
/// `mHerds.size()` (the result)
pub const EE_UNITS_PLAYER_TICK_HERD_COUNT: i32 = 2;
/// `gPhysics->mGameTime` into `io.i`
pub const EE_UNITS_PLAYER_TICK_GAME_TIME: i32 = 3;
/// herd `index`: its `mNextUpdateTime` into `io.i`, `mIsActive` into `io.b`
pub const EE_UNITS_PLAYER_TICK_HERD_DUE: i32 = 4;
/// herd `index`'s `Update(io.i)`
pub const EE_UNITS_PLAYER_TICK_HERD_UPDATE: i32 = 5;
/// herd `index`'s `mState` (the result)
pub const EE_UNITS_PLAYER_TICK_HERD_STATE: i32 = 6;
/// `KillHerdUnits(mHerds[index], 0)`
pub const EE_UNITS_PLAYER_TICK_HERD_KILL: i32 = 7;
/// `UpdateAirTraffic()`, then `mEvents.TickEvents(this, gPhysics->mTickTime)`
pub const EE_UNITS_PLAYER_TICK_AIR_AND_EVENTS: i32 = 8;
/// the walk of `mQueuedImprovements`: to its first entry; the result 1 with the key in `io.i` and the float in
/// `io.f`, 0 at the end
pub const EE_UNITS_PLAYER_TICK_QUEUED_FIRST: i32 = 9;
/// to the next entry (as FIRST)
pub const EE_UNITS_PLAYER_TICK_QUEUED_NEXT: i32 = 10;
/// the current entry erased, to the one after it (as FIRST)
pub const EE_UNITS_PLAYER_TICK_QUEUED_ERASE: i32 = 11;
/// the current entry's float `= io.f`
pub const EE_UNITS_PLAYER_TICK_QUEUED_SET: i32 = 12;
/// `gPhysics->GetTickTime()` into `io.f`
pub const EE_UNITS_PLAYER_TICK_TICK_TIME: i32 = 13;
/// `mInstantBuild` (the result)
pub const EE_UNITS_PLAYER_TICK_INSTANT_BUILD: i32 = 14;
/// `ApplyImprovement(key, the list's front, false)`, the front erased, the float 0
pub const EE_UNITS_PLAYER_TICK_QUEUED_APPLY: i32 = 15;
/// `AdvanceImprovement(io.i)` (the result)
pub const EE_UNITS_PLAYER_TICK_QUEUED_ADVANCE: i32 = 16;
/// `mHealsUnits` (the result)
pub const EE_UNITS_PLAYER_TICK_HEALS_UNITS: i32 = 17;
/// `gPhysics->mTickTimeMs` into `io.i`
pub const EE_UNITS_PLAYER_TICK_TICK_MS: i32 = 18;
/// the heal's numbers by `index`: 0 `kWorldWonderHealAmount`, 1 the tech tree's `mCurrentEpoch`, 2
/// `kWorldWonderHealEpochStep` into `io.i`; 3 `kWorldWonderHealEpochFactor` into `io.f`
pub const EE_UNITS_PLAYER_TICK_HEAL_VALUE: i32 = 19;
/// the player's unit list: from `io.i` 0 its first unit, else the unit after unit `io.i`; that unit into `io.i` (0
/// at the end), the result 1 when the healed unit set holds its class's type
pub const EE_UNITS_PLAYER_TICK_UNIT_NEXT: i32 = 20;
/// unit `io.i`'s `mHitPoints` (`index` 0) or its stats' `mMaxHitPoints` (1) (the result)
pub const EE_UNITS_PLAYER_TICK_UNIT_HIT_POINTS: i32 = 21;
/// unit `io.i`'s `SetHitPoints(index as long)`
pub const EE_UNITS_PLAYER_TICK_UNIT_SET_HIT_POINTS: i32 = 22;

/// `kPlayerHealInterval` (ms)
pub const EE_UNITS_PLAYER_HEAL_INTERVAL: f32 = 10000.0;

/// The player's C++ side for its tick: one call per operation.
pub trait PlayerTickHost {
    fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32;

    fn op(&mut self, op: i32, index: u32) -> i32 {
        self.call(op, index, &mut EeUnitsPlayerSaveIo::default())
    }
    fn query(&mut self, op: i32, index: u32) -> EeUnitsPlayerSaveIo {
        let mut io = EeUnitsPlayerSaveIo::default();
        self.call(op, index, &mut io);
        io
    }
    /// One step of the queued improvements' walk: the entry (key, float), or None at the end.
    fn queued(&mut self, op: i32) -> Option<(i64, f32)> {
        let mut io = EeUnitsPlayerSaveIo::default();
        (self.call(op, 0, &mut io) != 0).then_some((io.i, io.f))
    }
}

/// x86's `cvttss2si`: the truncation, the integer indefinite (`i32::MIN`) for NaN and out of range.
fn truncate(value: f32) -> i32 {
    if value.is_nan() || !(-2_147_483_648.0..2_147_483_648.0).contains(&value) {
        i32::MIN
    } else {
        value as i32
    }
}

/// The wonders' heal amount: `amount + (long)((epoch / step) * factor * amount)`.
pub fn heal_amount(amount: i32, epoch: i32, step: i32, factor: f32) -> i32 {
    let product = epoch.wrapping_div(step) as f32 * factor * amount as f32;
    amount.wrapping_add(truncate(product))
}

/// The record's `mHealTimer` behind its raw pointer.
struct HealTimer(*mut EeUnitsPlayerSaved);

impl HealTimer {
    fn get(&self) -> f32 {
        // SAFETY: update_player's contract; a field read, no reference kept
        unsafe { (*self.0).mHealTimer }
    }
    fn set(&self, value: f32) {
        // SAFETY: as get
        unsafe { (*self.0).mHealTimer = value }
    }
}

/// The herds from the last: each one due and active updated at the game time, each one in state 0xb ended.
fn tick_herds<H: PlayerTickHost + ?Sized>(h: &mut H) {
    let mut herd = h.op(EE_UNITS_PLAYER_TICK_HERD_COUNT, 0).wrapping_sub(1);
    while herd >= 0 {
        let time = h.query(EE_UNITS_PLAYER_TICK_GAME_TIME, 0).i as u32;
        let due = h.query(EE_UNITS_PLAYER_TICK_HERD_DUE, herd as u32);
        if (due.i as u32) <= time && due.b != 0 {
            let mut io = EeUnitsPlayerSaveIo { i: i64::from(time), ..Default::default() };
            h.call(EE_UNITS_PLAYER_TICK_HERD_UPDATE, herd as u32, &mut io);
        }
        if h.op(EE_UNITS_PLAYER_TICK_HERD_STATE, herd as u32) as u32 == 0xb {
            h.op(EE_UNITS_PLAYER_TICK_HERD_KILL, herd as u32);
        }
        herd -= 1;
    }
}

/// The queued improvements in key order: a running timer counts down by the tick (capped at 1 with instant build)
/// and when it runs out the list's first improvement is applied; then an entry whose queue `AdvanceImprovement`
/// finds empty is erased.
fn tick_queued<H: PlayerTickHost + ?Sized>(h: &mut H) {
    let mut entry = h.queued(EE_UNITS_PLAYER_TICK_QUEUED_FIRST);
    while let Some((key, timer)) = entry {
        if timer != 0.0 {
            let mut timer = timer - h.query(EE_UNITS_PLAYER_TICK_TICK_TIME, 0).f;
            if h.op(EE_UNITS_PLAYER_TICK_INSTANT_BUILD, 0) != 0 && timer > 1.0 {
                timer = 1.0;
            }
            let mut io = EeUnitsPlayerSaveIo { f: timer, ..Default::default() };
            h.call(EE_UNITS_PLAYER_TICK_QUEUED_SET, 0, &mut io);
            if timer > 0.0 {
                entry = h.queued(EE_UNITS_PLAYER_TICK_QUEUED_NEXT);
                continue;
            }
            h.op(EE_UNITS_PLAYER_TICK_QUEUED_APPLY, 0);
        }
        let mut io = EeUnitsPlayerSaveIo { i: key, ..Default::default() };
        let erase = h.call(EE_UNITS_PLAYER_TICK_QUEUED_ADVANCE, 0, &mut io) != 0;
        entry = h.queued(if erase { EE_UNITS_PLAYER_TICK_QUEUED_ERASE } else { EE_UNITS_PLAYER_TICK_QUEUED_NEXT });
    }
}

/// The wonders' healing: when the heal timer runs out, every living unit of a healed type gains the heal amount up
/// to its maximum, and the timer restarts.
fn tick_heal<H: PlayerTickHost + ?Sized>(h: &mut H, timer: &HealTimer) {
    let mut ticks = h.query(EE_UNITS_PLAYER_TICK_TICK_MS, 0).i as i32;
    if ticks <= 0 {
        ticks = 0;
    }
    if h.op(EE_UNITS_PLAYER_TICK_HEALS_UNITS, 0) == 0 {
        return;
    }
    let mut time = timer.get() - ticks as f32;
    if time <= 0.0 {
        let amount = h.query(EE_UNITS_PLAYER_TICK_HEAL_VALUE, 0).i as i32;
        let epoch = h.query(EE_UNITS_PLAYER_TICK_HEAL_VALUE, 1).i as i32;
        let step = h.query(EE_UNITS_PLAYER_TICK_HEAL_VALUE, 2).i as i32;
        let factor = h.query(EE_UNITS_PLAYER_TICK_HEAL_VALUE, 3).f;
        let heal = heal_amount(amount, epoch, step, factor);
        let mut unit: i64 = 0;
        loop {
            let mut io = EeUnitsPlayerSaveIo { i: unit, ..Default::default() };
            let healed = h.call(EE_UNITS_PLAYER_TICK_UNIT_NEXT, 0, &mut io) != 0;
            unit = io.i;
            if unit == 0 {
                break;
            }
            if !healed {
                continue;
            }
            let mut at = EeUnitsPlayerSaveIo { i: unit, ..Default::default() };
            let hit_points = h.call(EE_UNITS_PLAYER_TICK_UNIT_HIT_POINTS, 0, &mut at);
            let max = h.call(EE_UNITS_PLAYER_TICK_UNIT_HIT_POINTS, 1, &mut at);
            let mut value = hit_points.wrapping_add(heal);
            if value > max {
                value = max;
            }
            if hit_points > 0 {
                h.call(EE_UNITS_PLAYER_TICK_UNIT_SET_HIT_POINTS, value as u32, &mut at);
            }
        }
        time = EE_UNITS_PLAYER_HEAL_INTERVAL;
    }
    timer.set(time);
}

/// `UpdatePlayer` over the host and the player's record.
///
/// # Safety
/// `rec` points to the player's record for the whole call, written meanwhile only by the C++ the host runs.
pub unsafe fn update_player<H: PlayerTickHost + ?Sized>(h: &mut H, rec: *mut EeUnitsPlayerSaved) {
    h.op(EE_UNITS_PLAYER_TICK_VALIDATE, 0);
    tick_herds(h);
    h.op(EE_UNITS_PLAYER_TICK_AIR_AND_EVENTS, 0);
    tick_queued(h);
    tick_heal(h, &HealTimer(rec));
}

/// `Reset`'s fields of the record: part 0 those set before the events' clear (`player_id` makes `mEnemies`: every
/// other player an enemy), part 1 those zeroed after it.
pub fn reset(r: &mut EeUnitsPlayerSaved, player_id: u32, part: u32) {
    if part == 1 {
        r.mResourceTrade = [0; 5];
        r.mInsuranceRefunds = [0; 5];
        r.mNumVitalBuildings = 0;
        return;
    }
    for cap in &mut r.mResourceCaps[2..7] {
        *cap = 0;
    }
    for (player, enemy) in r.mEnemies.iter_mut().enumerate() {
        *enemy = c_ulong::from(player_id != player as u32);
    }
    r.mHighGroundBonus = 0.0;
    r.mPopulationCount = 0;
    r.mEndOfGameCount = 0;
    r.mPopLimit = 0;
    r.mNumCapitols = 0;
    r.mSharedLOS = true;
    r.mChatType = 1;
    r.mGameResult = 0;
    r.mHealTimer = 0.0;
    r.mDisconnected = false;
    r.mCivilizationLocked = false;
    r.mCivilizationChosen = false;
    r.mCivUpgradesPurchased = false;
    r.mFreeUnitCounter = 0;
    r.mFreeUnitInterval = 1;
    r.mTributes = [0; 10];
    r.mSendChatMessage = [true; 17];
    r.mCombatState = 0;
    r.mScenarioVariables = [0; 10];
}

struct FnHost {
    ctx: *mut c_void,
    f: EeUnitsPlayerTickFn,
}

impl PlayerTickHost for FnHost {
    fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        // SAFETY: the C++ callback's contract
        unsafe { (self.f)(self.ctx, op, index, io) }
    }
}

/// `EEPlayer::UpdatePlayer` (see [`update_player`]).
///
/// # Safety
/// `rec` is the player's record (as [`update_player`]); `f` handles every operation with `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_update(
    ctx: *mut c_void,
    f: EeUnitsPlayerTickFn,
    rec: *mut EeUnitsPlayerSaved,
) {
    // SAFETY: the caller's contract
    unsafe { update_player(&mut FnHost { ctx, f }, rec) };
}

/// `EEPlayer::Reset`'s fields of the record (see [`reset`]).
///
/// # Safety
/// `rec` points to the record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_reset(rec: *mut EeUnitsPlayerSaved, player_id: u32, part: u32) {
    // SAFETY: the caller's contract
    unsafe { reset(&mut *rec, player_id, part) };
}

#[cfg(test)]
#[path = "player_tick_tests.rs"]
mod tests;
