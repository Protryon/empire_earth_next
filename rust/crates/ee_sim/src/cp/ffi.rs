//! The C ABI of the computer player's Rust (`rust/sim/cp.h`).

// `c_ulong` is 32 bits on Windows and 64 elsewhere: the casts to the 32-bit words are needed there
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;

use std::os::raw::{c_long, c_ulong};

use super::fields::EeCpFields;
use super::file::SaveFile;
use super::init::{self, EeCpBehavior};
use super::legacy_load;
use super::save::{self, EeCpLoaded};
use crate::units::player_save::EeUnitsPlayerSaveFn;

/// The last save version whose computer players hold the deleted C++ managers' blocks in the original's layout.
pub const EE_CP_LEGACY_LAST_VERSION: u32 = 250;

/// Steps over a retail file's combat manager block (`EECPCombatManager::Load` at 0.228-0.250).
///
/// # Safety
/// `host` must serve the file primitives on `file` (`EESaveFilePrimitive` over an `FSFile*`).
#[no_mangle]
pub unsafe extern "C" fn ee_cp_skip_legacy_combat(
    file: *mut c_void,
    host: EeUnitsPlayerSaveFn,
    major: u32,
    minor: u32,
) -> i32 {
    // SAFETY: the caller's contract
    let mut f = unsafe { SaveFile::new(file, host) };
    legacy_load::skip_combat_manager(&mut f, major, minor)
}

/// Steps over a retail file's civilization manager block and its resource, build, research, unit and wall managers;
/// `town_center` (x, y) gets the build manager's town centre tile, read in place.
///
/// # Safety
/// As [`ee_cp_skip_legacy_combat`]; `town_center` points to two `int32_t`s.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_skip_legacy_civilization(
    file: *mut c_void,
    host: EeUnitsPlayerSaveFn,
    major: u32,
    minor: u32,
    town_center: *mut i32,
) -> i32 {
    // SAFETY: the caller's contract
    let (mut f, tile) = unsafe { (SaveFile::new(file, host), &mut *(town_center as *mut [i32; 2])) };
    legacy_load::skip_civilization_manager(&mut f, major, minor, tile)
}

/// Steps over a retail file's exploration manager block.
///
/// # Safety
/// As [`ee_cp_skip_legacy_combat`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_skip_legacy_exploration(
    file: *mut c_void,
    host: EeUnitsPlayerSaveFn,
    major: u32,
    minor: u32,
) -> i32 {
    // SAFETY: the caller's contract
    let mut f = unsafe { SaveFile::new(file, host) };
    legacy_load::skip_exploration_manager(&mut f, major, minor)
}

// ---- the record's bodies (cp::init, cp::save, cp::player)

/// `EEComputerPlayer::Init`'s values (both constructors).
///
/// # Safety
/// `p` is the computer player's record; `behaviors` points to `gCPBehaviors` (129 records).
#[no_mangle]
pub unsafe extern "C" fn ee_cp_init(
    p: *mut EeCpFields,
    behaviors: *const EeCpBehavior,
    difficulty: c_long,
    player_id: c_ulong,
    cheat_cost: bool,
) {
    // SAFETY: the caller's contract
    unsafe { init::init(p, behaviors, difficulty, player_id, cheat_cost) }
}

/// `EEComputerPlayer::Load` after `EEPlayer::Load` (its result `result`).
///
/// # Safety
/// `host` serves the file's primitives and `cp::save`'s operations on `ctx`; `p` is the player's record; `out` is
/// valid.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_load(
    ctx: *mut c_void,
    host: EeUnitsPlayerSaveFn,
    p: *mut EeCpFields,
    major: u32,
    minor: u32,
    result: i32,
    out: *mut EeCpLoaded,
) -> i32 {
    // SAFETY: the caller's contract
    unsafe { save::load(ctx, host, p, major, minor, result, &mut *out) }
}

/// `EEComputerPlayer::Save` after `EEPlayer::Save` (its result `result`).
///
/// # Safety
/// As [`ee_cp_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_save(
    ctx: *mut c_void,
    host: EeUnitsPlayerSaveFn,
    p: *const EeCpFields,
    major: u32,
    minor: u32,
    game_time: u32,
    result: i32,
) -> i32 {
    // SAFETY: the caller's contract
    unsafe { save::save(ctx, host, p, major, minor, game_time, result) }
}

/// `GetAttackDelayModifier`.
///
/// # Safety
/// `p` is a computer player's record.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_attack_delay_modifier(p: *const EeCpFields) -> f32 {
    // SAFETY: the caller's contract
    unsafe { (*p).attack_delay_modifier() }
}

/// `GetAttackFrequencyModifier`.
///
/// # Safety
/// `p` is a computer player's record.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_attack_frequency_modifier(p: *const EeCpFields) -> f32 {
    // SAFETY: the caller's contract
    unsafe { (*p).attack_frequency_modifier() }
}

/// `GetUnitCostModifier`.
///
/// # Safety
/// `p` is a computer player's record.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_unit_cost_modifier(p: *const EeCpFields) -> f32 {
    // SAFETY: the caller's contract
    unsafe { (*p).unit_cost_modifier() }
}

/// `GetDMAttackFrequencyModifier`.
///
/// # Safety
/// `p` is a computer player's record.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_dm_attack_frequency_modifier(p: *const EeCpFields) -> f32 {
    // SAFETY: the caller's contract
    unsafe { (*p).dm_attack_frequency_modifier() }
}

/// `GetDMInitialAttackModifier`.
///
/// # Safety
/// `p` is a computer player's record.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_dm_initial_attack_modifier(p: *const EeCpFields) -> f32 {
    // SAFETY: the caller's contract
    unsafe { (*p).dm_initial_attack_modifier() }
}

/// `GetPopLimitAdjustment`.
///
/// # Safety
/// `p` is a computer player's record.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pop_limit_adjustment(p: *const EeCpFields) -> f32 {
    // SAFETY: the caller's contract
    unsafe { (*p).pop_limit_adjustment() }
}

/// `GetIslandExpansionTimer`.
///
/// # Safety
/// `p` is a computer player's record.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_island_expansion_timer(p: *const EeCpFields) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { (*p).island_expansion_timer() }
}

/// `ScaleCost`.
///
/// # Safety
/// `p` is a computer player's record.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_scale_cost(p: *const EeCpFields, cost: c_long) -> c_long {
    // SAFETY: the caller's contract
    unsafe { (*p).scale_cost(cost) }
}

/// The scenario effects' setters (`what`: 0 `SetDMAttackFrequencyModifier`, 1 `SetDMInitialAttackModifier`, 2
/// `SetIslandExpansionTimer` with `value` a whole number of ms) for the player's difficulty.
///
/// # Safety
/// `p` is a computer player's record.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_set_by_difficulty(p: *mut EeCpFields, what: u32, value: f32, timer: c_ulong) {
    // SAFETY: the caller's contract
    let p = unsafe { &mut *p };
    match what {
        0 => p.set_dm_attack_frequency_modifier(value),
        1 => p.set_dm_initial_attack_modifier(value),
        _ => p.set_island_expansion_timer(timer),
    }
}

/// The game's generator for [`ee_cp_set_next_attack_time`]: `GetRandom(min, max)`.
pub type EeCpRandomFn = unsafe extern "C" fn(ctx: *mut c_void, min: c_long, max: c_long) -> c_long;

/// `SetNextAttackTime`.
///
/// # Safety
/// `p` is a computer player's record; `random` accepts `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_set_next_attack_time(
    p: *mut EeCpFields,
    time: c_ulong,
    epochs: c_long,
    deathmatch: bool,
    random: EeCpRandomFn,
    ctx: *mut c_void,
) {
    // SAFETY: the caller's contract (the generator touches no record field)
    unsafe {
        let mut draw = |lo: c_long, hi: c_long| random(ctx, lo, hi);
        (*p).set_next_attack_time(time, epochs, deathmatch, &mut draw)
    }
}

/// `RestoreManagers`' field steps: `what` 0 rebases the next attack time to `game_time`, 1 the next expansion time,
/// 2 the bounds, drop-off distance and regroup (`ok`: the restores succeeded), 3 the pre-0.229 timers
/// (`game_time` the minor, `ok` major > 0).
///
/// # Safety
/// `p` is a computer player's record.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_restore_fields(p: *mut EeCpFields, what: u32, game_time: c_ulong, ok: bool) {
    // SAFETY: the caller's contract
    let p = unsafe { &mut *p };
    match what {
        0 => p.rebase_attack_time(game_time),
        1 => p.rebase_expansion_time(game_time),
        2 => p.after_restore(ok),
        _ => p.old_expansion_timers(u32::from(ok), game_time as u32),
    }
}
