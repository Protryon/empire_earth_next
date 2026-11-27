//! TEST ONLY: the scenario capture records' entry points (the hooks of ee/EEScenarioCapture.h forward here from
//! EEScenarioCapture.cpp) over the C++ reads `ee_scenariocpp_capture` and `ee_scenariocpp_capture_f`. Absent from a
//! test binary (sm_capture.rs's tests use a table world).

#![cfg(not(test))]
// c_long is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_char, c_void};
use std::os::raw::{c_long, c_ulong};

use crate::scenario::sm_capture::{
    self, CaptureWorld, EeScenarioCapObjectBefore, EeScenarioCapTriggerBefore, EeScenarioCapVersion,
};

extern "C" {
    /// One read (`EE_SCENARIO_CAP_*`; EEScenarioCapture.cpp).
    fn ee_scenariocpp_capture(op: u32, p: *const c_void, a: i64, b: i64) -> i64;
    /// One float read.
    fn ee_scenariocpp_capture_f(op: u32, p: *const c_void) -> f64;
}

struct Game;

impl CaptureWorld for Game {
    fn read(&mut self, op: u32, p: u64, a: i64, b: i64) -> i64 {
        use crate::scenario::{entry, world_views};
        // SAFETY: the reads take the addresses the earlier reads answered (live objects for the hook's call); the
        // world's and the records' members are the views' (world_views.rs capture)
        unsafe {
            if op == crate::scenario::sm_capture::EE_SCENARIO_CAP_UNIT {
                return entry::unit_by_id(a) as i64;
            }
            world_views::capture(op, p, a, &entry::element_record)
                .unwrap_or_else(|| ee_scenariocpp_capture(op, p as usize as *const c_void, a, b))
        }
    }
    fn readf(&mut self, op: u32, p: u64) -> f32 {
        // SAFETY: as above
        unsafe {
            crate::scenario::world_views::capture_f(op, p, &crate::scenario::entry::element_record)
                .unwrap_or_else(|| ee_scenariocpp_capture_f(op, p as usize as *const c_void) as f32)
        }
    }
}

fn h(p: *const c_void) -> u64 {
    p as usize as u64
}

// SAFETY (every function below): the C++ hooks pass live objects of the documented classes, on the simulation
// thread, while the capture records.

/// SMCE (`condition` an `EESMCondition*`).
#[no_mangle]
pub extern "C" fn ee_scenario_cap_condition(condition: *const c_void, result: bool, seed: u32) {
    sm_capture::condition(&mut Game, h(condition), result, seed);
}

/// The trigger's state before its Evaluate (`trigger` an `EESMTrigger*`).
#[no_mangle]
pub extern "C" fn ee_scenario_cap_trigger_before(trigger: *const c_void) -> EeScenarioCapTriggerBefore {
    sm_capture::trigger_before(&mut Game, h(trigger))
}

/// SMTE.
///
/// # Safety
/// `before` points to the state [`ee_scenario_cap_trigger_before`] answered.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_cap_trigger(
    trigger: *const c_void,
    before: *const EeScenarioCapTriggerBefore,
    time: f32,
    seed: u32,
) {
    // SAFETY: the caller's contract
    let before = unsafe { *before };
    sm_capture::trigger(&mut Game, h(trigger), &before, time, seed);
}

/// SMOB's state before (`object` an `EESMObject*`).
#[no_mangle]
pub extern "C" fn ee_scenario_cap_object_begin(object: *const c_void) -> EeScenarioCapObjectBefore {
    sm_capture::object_begin(&mut Game, h(object))
}

/// SMOB.
///
/// # Safety
/// `before` points to the state [`ee_scenario_cap_object_begin`] answered.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_cap_object_end(
    object: *const c_void,
    include_dead: bool,
    result: bool,
    before: *const EeScenarioCapObjectBefore,
) {
    // SAFETY: the caller's contract
    let before = unsafe { *before };
    sm_capture::object_end(&mut Game, h(object), include_dead, result, &before);
}

/// SMCP (`players` null or a `std::vector<long>*`).
#[no_mangle]
pub extern "C" fn ee_scenario_cap_players(
    kind: c_long,
    single: bool,
    result: c_long,
    players: *const c_void,
    seed: u32,
) {
    sm_capture::players(&mut Game, kind as i32, single, result as i32, h(players), seed);
}

/// SMAR (`area` an `EESMArea*`).
#[no_mangle]
pub extern "C" fn ee_scenario_cap_area(
    area: *const c_void,
    kind: c_long,
    a: c_long,
    b: c_long,
    x: c_long,
    y: c_long,
    hash: u64,
) {
    sm_capture::area(&mut Game, h(area), kind as i32, a as i32, b as i32, x as i32, y as i32, hash);
}

/// SMEX and SMEW (`effect` an `EESMEffect*`; `version` the save version, major in the high 32 bits).
#[no_mangle]
pub extern "C" fn ee_scenario_cap_effect(
    effect: *const c_void,
    cinematic: bool,
    seed: u32,
    version: EeScenarioCapVersion,
) {
    sm_capture::effect(&mut Game, h(effect), cinematic, seed, version);
}

/// SMSV (`manager` the `EEScenarioManager*`).
#[no_mangle]
pub extern "C" fn ee_scenario_cap_saved(
    manager: *const c_void,
    kind: c_long,
    result: c_long,
    version: EeScenarioCapVersion,
) {
    sm_capture::saved(&mut Game, h(manager), kind as i32, result as i32, version);
}

/// SMUP (and every 120th round SMSV).
#[no_mangle]
pub extern "C" fn ee_scenario_cap_update(manager: *const c_void, human: bool, version: EeScenarioCapVersion) {
    sm_capture::update(&mut Game, h(manager), human, version);
}

/// SMPA.
#[no_mangle]
pub extern "C" fn ee_scenario_cap_player_attribute(
    player: c_long,
    attribute: c_ulong,
    mode: c_long,
    amount: c_long,
    seed: u32,
    version: EeScenarioCapVersion,
) {
    sm_capture::player_attribute(&mut Game, player, attribute, mode, amount, seed, version);
}

/// SMTK (the texts as C strings).
///
/// # Safety
/// `text` and `result` are null or C strings.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_cap_tokens(text: *const c_char, result: *const c_char, seed: u32) {
    // SAFETY: the caller's contract
    unsafe { sm_capture::tokens(&mut Game, text, result, seed) };
}

/// SMFP (`object` an `EESMObject*`).
#[no_mangle]
pub extern "C" fn ee_scenario_cap_placement(
    object: *const c_void,
    unit_type: c_ulong,
    random: bool,
    result: bool,
    x: f32,
    y: f32,
    seed: u32,
) {
    sm_capture::placement(&mut Game, h(object), unit_type, random, result, x, y, seed);
}

/// SMCU (`args` seven `long`s; `unit` an `EEUnit*`, `player` an `EEPlayer*`, `upgrade` an `EEUnitClassUpgrade*`,
/// each null for none).
///
/// # Safety
/// `args` points to seven `long`s.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_scenario_cap_unit_change(
    kind: c_long,
    args: *const c_long,
    unit: *const c_void,
    player: *const c_void,
    upgrade: *const c_void,
    seed: u32,
    version: EeScenarioCapVersion,
) {
    // SAFETY: the caller's contract
    let args = unsafe { *(args as *const [c_long; 7]) };
    sm_capture::unit_change(&mut Game, kind, &args, h(unit), h(player), h(upgrade), seed, version);
}
