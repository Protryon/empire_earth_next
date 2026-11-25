//! The player's tick and reset over its saved record: `EEPlayer::UpdatePlayer` (0x56be3f) and the record's part of
//! `EEPlayer::Reset` (0x565813).
//!
//! `UpdatePlayer` ([`update_player`]) is one sequence over a host callback ([`EeUnitsPlayerTickFn`], its own
//! operation numbers): the selections' validation (one C++ operation: no decision in it), the herds' updates and
//! the end of a herd in state 0xb, the air traffic and the events (one operation), the queued improvements' timers
//! (each entry's count down, the instant build's cap, the improvement applied when it runs out, the next one
//! started or the entry erased) and the wonders' healing (the timer in the record, the heal amount from the world
//! table and the epoch, each healed unit's hit points capped at its maximum). The queued improvements are the
//! record's Rust queue (player.rs), walked here in key order; the improvement applied and `AdvanceImprovement` are
//! host calls.
//!
//! [`reset`] sets the record's fields as `Reset` did, in two parts around the events' clear (the trade, refunds and
//! vital buildings were zeroed after it).
//!
//! Determinism: the original's `float` operations in order (the timers less the tick as `float`, the heal product
//! `(epoch / step) * factor * amount` in `float`, truncated as `cvttss2si`), its `long` sums wrapping, no random
//! draw.

#![allow(non_snake_case)]
// `c_ulong` is 32 bits on Windows and 64 elsewhere
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::ops::Bound::{Excluded, Unbounded};
use std::os::raw::c_ulong;

use crate::units::player::EeUnitsPlayerFields;

use crate::units::player_save::{EeUnitsPlayerSaveIo, EeUnitsPlayerSaved};

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
/// `gPhysics->GetTickTime()` into `io.f`
pub const EE_UNITS_PLAYER_TICK_TICK_TIME: i32 = 13;
/// `ApplyImprovement(io.i, index, false)` (the queue's first improvement of unit type `io.i`)
pub const EE_UNITS_PLAYER_TICK_QUEUED_APPLY: i32 = 15;
/// `AdvanceImprovement(io.i)` (the result)
pub const EE_UNITS_PLAYER_TICK_QUEUED_ADVANCE: i32 = 16;
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
// the compound operations' one-statement parts (the groups lane, round 2: [`split`])
/// `mViewedUnit` is there and held by the player only
pub const EE_UNITS_PLAYER_TICK_X_VIEWED_ALONE: i32 = 40;
/// `UpdateSelection(NULL, true)`
pub const EE_UNITS_PLAYER_TICK_X_UNVIEW: i32 = 41;
/// `mSelectedViewedUnit` is there and held by the player only
pub const EE_UNITS_PLAYER_TICK_X_SELECTED_VIEWED_ALONE: i32 = 42;
/// `SetSelectedViewedUnit(NULL)`
pub const EE_UNITS_PLAYER_TICK_X_SELECTED_UNVIEW: i32 = 43;
/// `ee_units_groups_player(this, 4)` (group_obj.rs validate_player)
pub const EE_UNITS_PLAYER_TICK_X_VALIDATE_GROUPS: i32 = 44;
/// `UpdateAirTraffic()`
pub const EE_UNITS_PLAYER_TICK_X_AIR: i32 = 45;
/// `mEvents.TickEvents(this, gPhysics->mTickTime)`
pub const EE_UNITS_PLAYER_TICK_X_EVENTS: i32 = 46;
/// `kWorldWonderHealAmount` into `io.i`
pub const EE_UNITS_PLAYER_TICK_X_HEAL_AMOUNT: i32 = 47;
/// the tech tree's `mCurrentEpoch` into `io.i`
pub const EE_UNITS_PLAYER_TICK_X_EPOCH: i32 = 48;
/// `kWorldWonderHealEpochStep` into `io.i`
pub const EE_UNITS_PLAYER_TICK_X_HEAL_STEP: i32 = 49;
/// `kWorldWonderHealEpochFactor` into `io.f`
pub const EE_UNITS_PLAYER_TICK_X_HEAL_FACTOR: i32 = 50;
/// the first unit (`io.i` 0) or the one after unit `io.i`, into `io.i`
pub const EE_UNITS_PLAYER_TICK_X_NEXT_UNIT: i32 = 51;
/// `mHealedUnitSet->Contains(unit io.i's class's mUnitType)`
pub const EE_UNITS_PLAYER_TICK_X_HEALED: i32 = 52;

/// Operation `op` over the one-statement operations of `f` when it is a compound one (its answer); `None` for the
/// others. The bodies and their tests ask the compound ones.
pub(crate) fn split(
    op: i32,
    index: u32,
    io: &mut EeUnitsPlayerSaveIo,
    f: &mut dyn FnMut(i32, u32, &mut EeUnitsPlayerSaveIo) -> i32,
) -> Option<i32> {
    let ask = |f: &mut dyn FnMut(i32, u32, &mut EeUnitsPlayerSaveIo) -> i32, op: i32| {
        f(op, 0, &mut EeUnitsPlayerSaveIo::default())
    };
    match op {
        EE_UNITS_PLAYER_TICK_VALIDATE => {
            if ask(f, EE_UNITS_PLAYER_TICK_X_VIEWED_ALONE) != 0 {
                ask(f, EE_UNITS_PLAYER_TICK_X_UNVIEW);
            }
            if ask(f, EE_UNITS_PLAYER_TICK_X_SELECTED_VIEWED_ALONE) != 0 {
                ask(f, EE_UNITS_PLAYER_TICK_X_SELECTED_UNVIEW);
            }
            ask(f, EE_UNITS_PLAYER_TICK_X_VALIDATE_GROUPS);
            Some(0)
        }
        EE_UNITS_PLAYER_TICK_AIR_AND_EVENTS => {
            ask(f, EE_UNITS_PLAYER_TICK_X_AIR);
            ask(f, EE_UNITS_PLAYER_TICK_X_EVENTS);
            Some(0)
        }
        EE_UNITS_PLAYER_TICK_HEAL_VALUE => {
            let x = match index {
                0 => EE_UNITS_PLAYER_TICK_X_HEAL_AMOUNT,
                1 => EE_UNITS_PLAYER_TICK_X_EPOCH,
                2 => EE_UNITS_PLAYER_TICK_X_HEAL_STEP,
                _ => EE_UNITS_PLAYER_TICK_X_HEAL_FACTOR,
            };
            Some(f(x, 0, io))
        }
        EE_UNITS_PLAYER_TICK_UNIT_NEXT => {
            f(EE_UNITS_PLAYER_TICK_X_NEXT_UNIT, 0, io);
            Some(if io.i != 0 && f(EE_UNITS_PLAYER_TICK_X_HEALED, 0, io) != 0 { 1 } else { 0 })
        }
        _ => None,
    }
}

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

/// The queued improvements in key order (the record's Rust queue): a running timer counts down by the tick (capped
/// at 1 with instant build) and when it runs out the list's first improvement is applied (dropped, the timer 0);
/// then an entry whose queue `AdvanceImprovement` finds empty is erased. The walk goes on to the next key after each
/// host call, so the C++ (`ApplyImprovement`, the virtual `AdvanceImprovement`: player.rs) may use the queue.
///
/// # Safety
/// `fields` is the player's record with its queue for the call.
unsafe fn tick_queued<H: PlayerTickHost + ?Sized>(h: &mut H, fields: *mut EeUnitsPlayerFields) {
    // SAFETY: the caller's contract; each access takes the queue again (no reference across a host call)
    let queued = unsafe { (*fields).mQueued };
    let mut cursor: Option<c_ulong> = None;
    loop {
        // SAFETY: as above
        let next = unsafe {
            let q = &*queued;
            match cursor {
                None => q.map.iter().next(),
                Some(k) => q.map.range((Excluded(k), Unbounded)).next(),
            }
            .map(|(k, e)| (*k, e.0))
        };
        let Some((key, timer)) = next else { break };
        cursor = Some(key);
        if timer != 0.0 {
            let mut timer = timer - h.query(EE_UNITS_PLAYER_TICK_TICK_TIME, 0).f;
            // SAFETY: as above
            if unsafe { (*fields).mInstantBuild } && timer > 1.0 {
                timer = 1.0;
            }
            // SAFETY: as above
            let front = unsafe {
                (*queued).entry(key).0 = timer;
                (*queued).entry(key).1.first().copied()
            };
            if timer > 0.0 {
                continue;
            }
            if let Some(front) = front {
                let mut io = EeUnitsPlayerSaveIo { i: key as i64, ..Default::default() };
                h.call(EE_UNITS_PLAYER_TICK_QUEUED_APPLY, front as u32, &mut io);
            }
            // SAFETY: as above
            if let Some(e) = unsafe { (*queued).map.get_mut(&key) } {
                if front.is_some() && !e.1.is_empty() {
                    e.1.remove(0);
                }
                e.0 = 0.0;
            }
        }
        let mut io = EeUnitsPlayerSaveIo { i: key as i64, ..Default::default() };
        if h.call(EE_UNITS_PLAYER_TICK_QUEUED_ADVANCE, 0, &mut io) != 0 {
            // SAFETY: as above
            unsafe { (*queued).map.remove(&key) };
        }
    }
}

/// The wonders' healing: when the heal timer runs out, every living unit of a healed type gains the heal amount up
/// to its maximum, and the timer restarts.
fn tick_heal<H: PlayerTickHost + ?Sized>(h: &mut H, timer: &HealTimer, heals: bool) {
    let mut ticks = h.query(EE_UNITS_PLAYER_TICK_TICK_MS, 0).i as i32;
    if ticks <= 0 {
        ticks = 0;
    }
    if !heals {
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

/// `UpdatePlayer` over the host and the player's records.
///
/// # Safety
/// `rec` and `fields` point to the player's records (with its queue) for the whole call, written meanwhile only by
/// the C++ the host runs.
pub unsafe fn update_player<H: PlayerTickHost + ?Sized>(
    h: &mut H,
    rec: *mut EeUnitsPlayerSaved,
    fields: *mut EeUnitsPlayerFields,
) {
    h.op(EE_UNITS_PLAYER_TICK_VALIDATE, 0);
    tick_herds(h);
    h.op(EE_UNITS_PLAYER_TICK_AIR_AND_EVENTS, 0);
    // SAFETY: the caller's contract
    unsafe { tick_queued(h, fields) };
    // SAFETY: as above (a field read)
    let heals = unsafe { (*fields).mHealsUnits };
    tick_heal(h, &HealTimer(rec), heals);
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
    // port (the player lane, round 2; a deliberate change): all seven words, where the original set 2..6 and left 0
    // and 1 (never written) holding the memory the player was made in, which `EEPlayer::GetChecksum` sums
    r.mResourceCaps = [0; 7];
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
    /// the player: its herds' count and a herd's kill answered here (player_lists.rs, player_rest.rs)
    player: u64,
}

impl PlayerTickHost for FnHost {
    fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        if op == EE_UNITS_PLAYER_TICK_UNIT_HIT_POINTS {
            // the unit's `mHitPoints` (index 0) or its stats' `mMaxHitPoints`, through the world views (lane P);
            // SAFETY: the C++ hands the player's live unit, and holds the world while the tick runs
            let t = unsafe { crate::world::WorldRead::begin() };
            // SAFETY: as above
            if let Some(u) = unsafe { crate::world::UnitView::of(&t, io.i as u64) } {
                let v = if index == 0 { u.hit_points() } else { u.stats().map_or(0, |s| s.max_hit_points()) };
                return v as i32;
            }
        }
        #[cfg(not(test))]
        {
            use crate::units::player_lists::{player_op, ListOp, EE_UNITS_LISTS_HERDS};
            use crate::units::player_rest::{game::ee_units_player_rest, EE_UNITS_PLAYER_REST_KILL_HERD_UNITS};
            match op {
                EE_UNITS_PLAYER_TICK_HERD_COUNT => return player_op(self.player, EE_UNITS_LISTS_HERDS, ListOp::Len) as i32,
                EE_UNITS_PLAYER_TICK_HERD_KILL => {
                    let herd = player_op(self.player, EE_UNITS_LISTS_HERDS, ListOp::At(index as usize));
                    let player = self.player as usize as *mut c_void;
                    // SAFETY: the live player and its herd
                    unsafe { ee_units_player_rest(player, EE_UNITS_PLAYER_REST_KILL_HERD_UNITS, herd, 0, 0) };
                    return 0;
                }
                _ => {}
            }
        }
        let (ctx, f) = (self.ctx, self.f);
        // SAFETY (both): the C++ callback's contract
        let mut call = |op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo| unsafe { f(ctx, op, index, io) };
        // the compound operations as the C++'s one-statement ones ([`split`])
        if let Some(r) = split(op, index, io, &mut call) {
            return r;
        }
        call(op, index, io)
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
    fields: *mut EeUnitsPlayerFields,
) {
    let player = (fields as usize).wrapping_sub(crate::world::layout::layout().player_fields as usize) as u64;
    // SAFETY: the caller's contract
    unsafe { update_player(&mut FnHost { ctx, f, player }, rec, fields) };
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

#[cfg(test)]
mod split_tests {
    use super::*;

    fn run(op: i32, index: u32, i: i64, answers: &[(i32, i32, i64)]) -> (Option<i32>, i64, Vec<i32>) {
        let mut log = Vec::new();
        let mut queue = answers.to_vec();
        let mut f = |op: i32, _index: u32, io: &mut EeUnitsPlayerSaveIo| {
            log.push(op);
            match queue.iter().position(|a| a.0 == op) {
                Some(k) => {
                    let (_, r, v) = queue.remove(k);
                    io.i = v;
                    r
                }
                None => 0,
            }
        };
        let mut io = EeUnitsPlayerSaveIo { i, ..Default::default() };
        let r = split(op, index, &mut io, &mut f);
        (r, io.i, log)
    }

    #[test]
    fn the_tick_operations_split_in_the_original_order() {
        let (_, _, log) =
            run(EE_UNITS_PLAYER_TICK_VALIDATE, 0, 0, &[(EE_UNITS_PLAYER_TICK_X_SELECTED_VIEWED_ALONE, 1, 0)]);
        assert_eq!(
            log,
            vec![
                EE_UNITS_PLAYER_TICK_X_VIEWED_ALONE,
                EE_UNITS_PLAYER_TICK_X_SELECTED_VIEWED_ALONE,
                EE_UNITS_PLAYER_TICK_X_SELECTED_UNVIEW,
                EE_UNITS_PLAYER_TICK_X_VALIDATE_GROUPS,
            ]
        );
        assert_eq!(
            run(EE_UNITS_PLAYER_TICK_AIR_AND_EVENTS, 0, 0, &[]).2,
            vec![EE_UNITS_PLAYER_TICK_X_AIR, EE_UNITS_PLAYER_TICK_X_EVENTS]
        );
        assert_eq!(run(EE_UNITS_PLAYER_TICK_HEAL_VALUE, 2, 0, &[]).2, vec![EE_UNITS_PLAYER_TICK_X_HEAL_STEP]);
        assert_eq!(run(EE_UNITS_PLAYER_TICK_HEAL_VALUE, 3, 0, &[]).2, vec![EE_UNITS_PLAYER_TICK_X_HEAL_FACTOR]);
        // the next unit, healed; the end of the list asks no set
        let r = run(
            EE_UNITS_PLAYER_TICK_UNIT_NEXT,
            0,
            0,
            &[(EE_UNITS_PLAYER_TICK_X_NEXT_UNIT, 0, 0x40), (EE_UNITS_PLAYER_TICK_X_HEALED, 1, 0x40)],
        );
        assert_eq!((r.0, r.1), (Some(1), 0x40));
        let r = run(EE_UNITS_PLAYER_TICK_UNIT_NEXT, 0, 0x40, &[(EE_UNITS_PLAYER_TICK_X_NEXT_UNIT, 0, 0)]);
        assert_eq!((r.0, r.1, r.2.len()), (Some(0), 0, 1));
        assert_eq!(run(EE_UNITS_PLAYER_TICK_HERD_COUNT, 0, 0, &[]).0, None);
    }
}
