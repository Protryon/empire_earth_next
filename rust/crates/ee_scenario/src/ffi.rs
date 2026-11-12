//! The C ABI the scenario manager's C++ classes call (`include/rust/ee_scenario.h`; the host in
//! `Scenario Manager/EEScenarioHost.cpp`). Plain records in and out; the world through the host's reads, and no
//! Rust reference lives across one (a read may run a condition's or an object's `Evaluate`, which calls back here).

use crate::area::{self, EeScenarioAreaRecord};
use crate::condition::{self, EeScenarioCondition};
use crate::host::EeScenarioHost;
use crate::object::{self, EeScenarioObjectFilter};
use crate::trigger::{
    self, EeScenarioConditionEntry, EeScenarioEffectEntry, EeScenarioTimedEffect, EeScenarioTriggerState,
};

/// The most players a choice can list (`EEScenarioManager::ChoosePlayer`'s out buffer; the world has at most 17).
pub const EE_SCENARIO_MAX_PLAYERS: usize = 64;

/// A copy of the caller's table.
///
/// # Safety
/// `host` must be valid for a read.
unsafe fn host_of(host: *const EeScenarioHost) -> EeScenarioHost {
    // SAFETY: the caller's contract
    unsafe { EeScenarioHost { ctx: (*host).ctx, read: (*host).read, players: (*host).players } }
}

// SAFETY (every function taking pointers below): the caller passes valid, aligned pointers for the documented reads
// and writes, as the C++ host does from its own locals; the host table's callbacks are callable with its context.

/// `EESMCondition::Compare`.
#[no_mangle]
pub extern "C" fn ee_scenario_compare(value: u32, op: u32, target: u32) -> bool {
    condition::compare(value, op, target)
}

/// `EESMCondition::Evaluate`.
///
/// # Safety
/// `host` and `condition` must be valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_condition_evaluate(
    host: *const EeScenarioHost,
    condition: *const EeScenarioCondition,
) -> bool {
    // SAFETY: the caller's contract; the record is copied before the first read
    let (mut host, condition) = unsafe { (host_of(host), *condition) };
    condition::evaluate(&mut host, &condition)
}

/// `EESMCondition::GetPlayerValue`.
///
/// # Safety
/// `host` must be valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_player_value(host: *const EeScenarioHost, player: i32, kind: u32) -> u32 {
    // SAFETY: the caller's contract
    let mut host = unsafe { host_of(host) };
    condition::player_value(&mut host, player, kind)
}

/// `EEScenarioManager::ChoosePlayer`: the chosen player (-1: none or a group); the players into `out_players` (room
/// for [`EE_SCENARIO_MAX_PLAYERS`]), their number into `out_count`; `listed`: whether the caller takes them (the
/// capture record's).
///
/// # Safety
/// `host` must be valid for the call, `out_players` for `EE_SCENARIO_MAX_PLAYERS` writes, `out_count` for one.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_choose_player(
    host: *const EeScenarioHost,
    kind: i32,
    single: bool,
    listed: bool,
    out_players: *mut i32,
    out_count: *mut usize,
) -> i32 {
    // SAFETY: the caller's contract
    let mut host = unsafe { host_of(host) };
    let (result, players) = condition::choose_player_recorded(&mut host, kind, single, listed);
    assert!(players.len() <= EE_SCENARIO_MAX_PLAYERS, "a player choice of {} players", players.len());
    // SAFETY: the caller's contract; at most EE_SCENARIO_MAX_PLAYERS
    unsafe {
        std::ptr::copy_nonoverlapping(players.as_ptr(), out_players, players.len());
        *out_count = players.len();
    }
    result
}

/// `EESMTrigger::Evaluate(inTime)`: updates `state`; the effects to schedule (`AddTimedEffect` in order, with the
/// trigger's cinematic flag) into `out` (room for `effect_count`), their number into `out_count`.
///
/// # Safety
/// `host` must be valid for the call, `state` for a read and a write, `conditions` and `effects` for their counts of
/// reads (either may be null when its count is 0), `out` for `effect_count` writes, `out_count` for one.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_trigger_evaluate(
    host: *const EeScenarioHost,
    state: *mut EeScenarioTriggerState,
    in_world_cinematic: bool,
    conditions: *const EeScenarioConditionEntry,
    condition_count: usize,
    effects: *const EeScenarioEffectEntry,
    effect_count: usize,
    time: f32,
    out: *mut EeScenarioTimedEffect,
    out_count: *mut usize,
) {
    // SAFETY: the caller's contract; the entries are copied before the first read
    let (mut host, mut s, conditions, effects) = unsafe {
        (
            host_of(host),
            *state,
            if condition_count == 0 {
                Vec::new()
            } else {
                std::slice::from_raw_parts(conditions, condition_count).to_vec()
            },
            if effect_count == 0 { Vec::new() } else { std::slice::from_raw_parts(effects, effect_count).to_vec() },
        )
    };
    let mut timed = Vec::new();
    trigger::evaluate(&mut host, &mut s, in_world_cinematic, &conditions, &effects, time, &mut timed);
    assert!(timed.len() <= effect_count);
    // SAFETY: the caller's contract; at most one timed effect per effect entry
    unsafe {
        *state = s;
        std::ptr::copy_nonoverlapping(timed.as_ptr(), out, timed.len());
        *out_count = timed.len();
    }
}

/// `EESMArea::ContainsTile`.
///
/// # Safety
/// `host` and `area` must be valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_area_contains_tile(
    host: *const EeScenarioHost,
    area: *const EeScenarioAreaRecord,
    x: i32,
    y: i32,
) -> bool {
    // SAFETY: the caller's contract; the record is copied before the first read
    let (mut host, area) = unsafe { (host_of(host), *area) };
    area::contains_tile(&mut host, &area, x, y)
}

/// `EESMArea::ContainsUnit` of the host's unit (not called for no unit).
///
/// # Safety
/// `host` and `area` must be valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_area_contains_unit(
    host: *const EeScenarioHost,
    area: *const EeScenarioAreaRecord,
) -> bool {
    // SAFETY: the caller's contract; the record is copied before the first read
    let (mut host, area) = unsafe { (host_of(host), *area) };
    area::contains_unit(&mut host, &area)
}

/// `EESMArea::GetCenterTile`: the centre into `out` (x, y); `area`'s cached centre updated.
///
/// # Safety
/// `host` must be valid for the call, `area` for a read and a write, `out` for two writes.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_area_center_tile(
    host: *const EeScenarioHost,
    area: *mut EeScenarioAreaRecord,
    out: *mut [i32; 2],
) {
    // SAFETY: the caller's contract; the record is copied before the first read and written back after the last
    let (mut host, mut a) = unsafe { (host_of(host), *area) };
    let (x, y) = area::center_tile(&mut host, &mut a);
    // SAFETY: the caller's contract
    unsafe {
        *area = a;
        *out = [x, y];
    }
}

/// `EESMObject::AcceptsClass`.
///
/// # Safety
/// `host` and `filter` must be valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_object_accepts_class(
    host: *const EeScenarioHost,
    filter: *const EeScenarioObjectFilter,
    class: u32,
) -> bool {
    // SAFETY: the caller's contract; the record is copied before the first read
    let (mut host, filter) = unsafe { (host_of(host), *filter) };
    object::accepts_class(&mut host, &filter, class)
}

/// `EESMObject::PickUnitType`.
///
/// # Safety
/// `host` and `filter` must be valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_object_pick_unit_type(
    host: *const EeScenarioHost,
    filter: *const EeScenarioObjectFilter,
) -> i32 {
    // SAFETY: the caller's contract; the record is copied before the first read
    let (mut host, filter) = unsafe { (host_of(host), *filter) };
    object::pick_unit_type(&mut host, &filter)
}
