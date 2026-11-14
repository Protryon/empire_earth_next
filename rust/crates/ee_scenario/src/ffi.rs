//! The C ABI the scenario manager's C++ classes call (`include/rust/ee_scenario.h`; the host in
//! `Scenario Manager/EEScenarioHost.cpp`). Plain records in and out; the world through the host's reads, and no
//! Rust reference lives across one (a read may run a condition's or an object's `Evaluate`, which calls back here).

use std::os::raw::{c_long, c_ulong};

use crate::aivar::{self, EeScenarioAiWrite, EE_SCENARIO_AI_MAX_WRITES};
use crate::area::{self, EeScenarioAreaRecord};
use crate::attribute;
use crate::condition::{self, EeScenarioCondition};
use crate::create;
use crate::fields::{
    self, EeScenarioAreaSave, EeScenarioConditionFields, EeScenarioEffectFields, EeScenarioObjectFields,
    EeScenarioTriggerFields,
};
use crate::host::{EeScenarioHost, Host};
use crate::manager::{self, EeScenarioManagerFields, EeScenarioTimedEntry, EeScenarioTimedQueue};
use crate::object::{self, EeScenarioObjectFilter};
use crate::objeval::{self, ObjectTable};
use crate::placement;
use crate::player;
use crate::round::{self, EeScenarioRoundHost};
use crate::save;
use crate::stream::EeScenarioStream;
use crate::tokens;
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
    unsafe {
        let h = &*host;
        EeScenarioHost { ctx: h.ctx, read: h.read, players: h.players, write: h.write, tiles: h.tiles }
    }
}

// SAFETY (every function taking pointers below): the caller passes valid, aligned pointers for the documented reads
// and writes, as the C++ host does from its own locals; the host table's callbacks are callable with its context.

/// `EESMCondition::Compare`.
#[no_mangle]
pub extern "C" fn ee_scenario_compare(value: u32, op: u32, target: u32) -> bool {
    condition::compare(value, op, target)
}

/// `EESMCondition::Evaluate` of the condition's numbers.
///
/// # Safety
/// `host` and `condition` must be valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_condition_evaluate(
    host: *const EeScenarioHost,
    condition: *const EeScenarioConditionFields,
) -> bool {
    // SAFETY: the caller's contract; the record is copied before the first read
    let (mut host, condition) = unsafe { (host_of(host), EeScenarioCondition::from(&*condition)) };
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
    filter: *const EeScenarioObjectFields,
    class: u32,
) -> bool {
    // SAFETY: the caller's contract; the record is copied before the first read
    let (mut host, filter) = unsafe { (host_of(host), EeScenarioObjectFilter::from(&*filter)) };
    object::accepts_class(&mut host, &filter, class)
}

/// `EESMObject::PickUnitType`.
///
/// # Safety
/// `host` and `filter` must be valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_object_pick_unit_type(
    host: *const EeScenarioHost,
    filter: *const EeScenarioObjectFields,
) -> i32 {
    // SAFETY: the caller's contract; the record is copied before the first read
    let (mut host, filter) = unsafe { (host_of(host), EeScenarioObjectFilter::from(&*filter)) };
    object::pick_unit_type(&mut host, &filter)
}

/// A copy of the caller's stream table.
///
/// # Safety
/// `stream` must be valid for a read.
unsafe fn stream_of(stream: *const EeScenarioStream) -> EeScenarioStream {
    // SAFETY: the caller's contract
    unsafe { EeScenarioStream { ctx: (*stream).ctx, io: (*stream).io } }
}

/// `EESMEffect::Reset`'s numbers.
#[no_mangle]
pub extern "C" fn ee_scenario_effect_defaults() -> EeScenarioEffectFields {
    fields::effect_defaults()
}

/// `EESMCondition`'s constructor's numbers.
#[no_mangle]
pub extern "C" fn ee_scenario_condition_defaults() -> EeScenarioConditionFields {
    fields::condition_defaults()
}

/// `EESMObject`'s constructor's numbers.
#[no_mangle]
pub extern "C" fn ee_scenario_object_defaults() -> EeScenarioObjectFields {
    fields::object_defaults()
}

// The saves (save.rs): the numbers are copied in, read or written through the stream (each value into or out of
// the copy), and copied back after the last call; the stream's callbacks reach only the object's strings and list.

/// `EESMEffect::Load`: the first error.
///
/// # Safety
/// `stream` must be valid for the call, `fields` for a read and a write.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_effect_load(
    stream: *const EeScenarioStream,
    fields: *mut EeScenarioEffectFields,
    major: c_ulong,
    minor: c_ulong,
) -> i32 {
    // SAFETY: the caller's contract
    let (mut s, mut f) = unsafe { (stream_of(stream), *fields) };
    let r = save::effect_load(&mut s, &mut f, major, minor);
    // SAFETY: the caller's contract
    unsafe { *fields = f };
    r
}

/// `EESMEffect::Save`: the first error.
///
/// # Safety
/// `stream` must be valid for the call, `fields` for a read.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_effect_save(
    stream: *const EeScenarioStream,
    fields: *const EeScenarioEffectFields,
    major: c_ulong,
    minor: c_ulong,
) -> i32 {
    // SAFETY: the caller's contract
    let (mut s, f) = unsafe { (stream_of(stream), *fields) };
    save::effect_save(&mut s, &f, major, minor)
}

/// `EESMCondition::Load`: the first error.
///
/// # Safety
/// `stream` must be valid for the call, `fields` for a read and a write.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_condition_load(
    stream: *const EeScenarioStream,
    fields: *mut EeScenarioConditionFields,
) -> i32 {
    // SAFETY: the caller's contract
    let (mut s, mut f) = unsafe { (stream_of(stream), *fields) };
    let r = save::condition_load(&mut s, &mut f);
    // SAFETY: the caller's contract
    unsafe { *fields = f };
    r
}

/// `EESMCondition::Save`: the first error.
///
/// # Safety
/// `stream` must be valid for the call, `fields` for a read.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_condition_save(
    stream: *const EeScenarioStream,
    fields: *const EeScenarioConditionFields,
) -> i32 {
    // SAFETY: the caller's contract
    let (mut s, f) = unsafe { (stream_of(stream), *fields) };
    save::condition_save(&mut s, &f)
}

/// `EESMObject::Load`: the first error.
///
/// # Safety
/// `stream` must be valid for the call, `fields` for a read and a write.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_object_load(
    stream: *const EeScenarioStream,
    fields: *mut EeScenarioObjectFields,
    major: c_ulong,
    minor: c_ulong,
) -> i32 {
    // SAFETY: the caller's contract
    let (mut s, mut f) = unsafe { (stream_of(stream), *fields) };
    let r = save::object_load(&mut s, &mut f, major, minor);
    // SAFETY: the caller's contract
    unsafe { *fields = f };
    r
}

/// `EESMObject::Save`: the first error.
///
/// # Safety
/// `stream` must be valid for the call, `fields` for a read.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_object_save(
    stream: *const EeScenarioStream,
    fields: *const EeScenarioObjectFields,
    minor: c_ulong,
) -> i32 {
    // SAFETY: the caller's contract
    let (mut s, f) = unsafe { (stream_of(stream), *fields) };
    save::object_save(&mut s, &f, minor)
}

/// The effect's `Renumber*` of a kind (`EE_SCENARIO_RENUMBER_*`; its object list's entries:
/// [`ee_scenario_renumber_ids`]).
///
/// # Safety
/// `fields` must be valid for a read and a write.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_effect_renumber(
    fields: *mut EeScenarioEffectFields,
    kind: u32,
    old: c_ulong,
    new: c_ulong,
) {
    // SAFETY: the caller's contract
    unsafe { fields::effect_renumber(&mut *fields, kind, old, new) }
}

/// The condition's `Renumber*` of a kind.
///
/// # Safety
/// `fields` must be valid for a read and a write.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_condition_renumber(
    fields: *mut EeScenarioConditionFields,
    kind: u32,
    old: c_ulong,
    new: c_ulong,
) {
    // SAFETY: the caller's contract
    unsafe { fields::condition_renumber(&mut *fields, kind, old, new) }
}

/// The object's `Renumber*` of a kind.
///
/// # Safety
/// `fields` must be valid for a read and a write.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_object_renumber(
    fields: *mut EeScenarioObjectFields,
    kind: u32,
    old: c_ulong,
    new: c_ulong,
) {
    // SAFETY: the caller's contract
    unsafe { fields::object_renumber(&mut *fields, kind, old, new) }
}

/// The same upkeep of each of `count` indices (an effect's object list).
///
/// # Safety
/// `ids` must be valid for `count` reads and writes (any pointer when `count` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_renumber_ids(ids: *mut c_ulong, count: usize, old: c_ulong, new: c_ulong) {
    if count == 0 {
        return;
    }
    // SAFETY: the caller's contract
    for id in unsafe { std::slice::from_raw_parts_mut(ids, count) } {
        fields::renumber(id, old, new);
    }
}

/// `EESMEffect::SetAIVariable`'s stores for the effect's behaviour, in order, into `out` (room for
/// [`EE_SCENARIO_AI_MAX_WRITES`]), their number into `out_count`; whether the applied effect is kept for a saved game.
///
/// # Safety
/// `fields` must be valid for a read, `out` for `EE_SCENARIO_AI_MAX_WRITES` writes, `out_count` for one.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_ai_variable(
    fields: *const EeScenarioEffectFields,
    out: *mut EeScenarioAiWrite,
    out_count: *mut usize,
) -> bool {
    // SAFETY: the caller's contract
    let f = unsafe { *fields };
    let mut writes = Vec::new();
    let deferred = aivar::ai_variable(&f, &mut writes);
    assert!(writes.len() <= EE_SCENARIO_AI_MAX_WRITES);
    // SAFETY: the caller's contract; at most EE_SCENARIO_AI_MAX_WRITES
    unsafe {
        std::ptr::copy_nonoverlapping(writes.as_ptr(), out, writes.len());
        *out_count = writes.len();
    }
    deferred
}

/// `EESMTrigger::Load`: the first error.
///
/// # Safety
/// `stream` must be valid for the call, `fields` for a read and a write.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_trigger_load(
    stream: *const EeScenarioStream,
    fields: *mut EeScenarioTriggerFields,
) -> i32 {
    // SAFETY: the caller's contract
    let (mut s, mut f) = unsafe { (stream_of(stream), *fields) };
    let r = save::trigger_load(&mut s, &mut f);
    // SAFETY: the caller's contract
    unsafe { *fields = f };
    r
}

/// `EESMTrigger::Save`: the first error.
///
/// # Safety
/// `stream` must be valid for the call, `fields` for a read.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_trigger_save(
    stream: *const EeScenarioStream,
    fields: *const EeScenarioTriggerFields,
) -> i32 {
    // SAFETY: the caller's contract
    let (mut s, f) = unsafe { (stream_of(stream), *fields) };
    save::trigger_save(&mut s, &f)
}

/// `EESMArea::Load` (`save` false) or `Save` of the area's values: the first error.
///
/// # Safety
/// `stream` must be valid for the call, `area` for a read and a write.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_area_io(
    stream: *const EeScenarioStream,
    area: *mut EeScenarioAreaSave,
    save: bool,
) -> i32 {
    // SAFETY: the caller's contract
    let (mut s, mut a) = unsafe { (stream_of(stream), *area) };
    let r = if save { save::area_save(&mut s, &a) } else { save::area_load(&mut s, &mut a) };
    // SAFETY: the caller's contract
    unsafe { *area = a };
    r
}

/// `EESMMediaStrings::Load` (`save` false) or `Save`: the `count` strings in order, the first error.
///
/// # Safety
/// `stream` must be valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_strings_io(stream: *const EeScenarioStream, count: u32, save: bool) -> i32 {
    // SAFETY: the caller's contract
    let mut s = unsafe { stream_of(stream) };
    save::strings(&mut s, count, save)
}

// The manager (manager.rs): its numbers are the record the C++ class derives from; the timed effects' queue is
// owned here, the C++ holding the pointer from `ee_scenario_timed_new` to `ee_scenario_timed_free`.

/// A new, empty timed effects' queue.
#[no_mangle]
pub extern "C" fn ee_scenario_timed_new() -> *mut EeScenarioTimedQueue {
    Box::into_raw(Box::default())
}

/// Frees a queue of `ee_scenario_timed_new` (null: nothing).
///
/// # Safety
/// `queue` must be null or a queue of `ee_scenario_timed_new` not freed before.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_timed_free(queue: *mut EeScenarioTimedQueue) {
    if !queue.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(queue) });
    }
}

/// The queue for one short call (no C++ runs while it is borrowed).
///
/// # Safety
/// `queue` must be a live queue of `ee_scenario_timed_new`.
unsafe fn queue_of<'a>(queue: *mut EeScenarioTimedQueue) -> &'a mut EeScenarioTimedQueue {
    // SAFETY: the caller's contract
    unsafe { &mut *queue }
}

/// The number of timed effects.
///
/// # Safety
/// `queue` must be a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_timed_count(queue: *mut EeScenarioTimedQueue) -> usize {
    // SAFETY: the caller's contract
    unsafe { queue_of(queue) }.entries.len()
}

/// Timed effect `index` (a copy; `index` must be below the count).
///
/// # Safety
/// `queue` must be a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_timed_at(queue: *mut EeScenarioTimedQueue, index: usize) -> EeScenarioTimedEntry {
    // SAFETY: the caller's contract
    unsafe { queue_of(queue) }.entries[index]
}

/// `EEScenarioManager::AddTimedEffect`.
///
/// # Safety
/// `queue` must be a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_timed_add(
    queue: *mut EeScenarioTimedQueue,
    effect: c_long,
    time: c_ulong,
    cinematic: bool,
) {
    // SAFETY: the caller's contract
    unsafe { queue_of(queue) }.add(effect, time, cinematic);
}

/// `EEScenarioManager::RemoveTimedEffects`.
///
/// # Safety
/// `queue` must be a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_timed_remove_effect(queue: *mut EeScenarioTimedQueue, effect: c_long) {
    // SAFETY: the caller's contract
    unsafe { queue_of(queue) }.remove_effect(effect);
}

/// `EEScenarioManager::EraseTimedEffect` (`index` below the count).
///
/// # Safety
/// `queue` must be a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_timed_erase(queue: *mut EeScenarioTimedQueue, index: usize) {
    // SAFETY: the caller's contract
    unsafe { queue_of(queue) }.entries.remove(index);
}

/// Empties the queue (`SortEffects`).
///
/// # Safety
/// `queue` must be a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_timed_clear(queue: *mut EeScenarioTimedQueue) {
    // SAFETY: the caller's contract
    unsafe { queue_of(queue) }.entries.clear();
}

/// `RenumberEffects`' timed part.
///
/// # Safety
/// `queue` must be a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_timed_renumber(queue: *mut EeScenarioTimedQueue, old: c_ulong, new: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { queue_of(queue) }.renumber(old, new);
}

/// `RemapEffects`' timed part: the map as `count` pairs `olds[i]` -> `news[i]`.
///
/// # Safety
/// `queue` must be a live queue; `olds` and `news` valid for `count` reads (either may be null when it is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_timed_remap(
    queue: *mut EeScenarioTimedQueue,
    olds: *const c_ulong,
    news: *const c_ulong,
    count: usize,
) {
    // SAFETY: the caller's contract
    let (olds, news) = unsafe {
        if count == 0 {
            (&[][..], &[][..])
        } else {
            (std::slice::from_raw_parts(olds, count), std::slice::from_raw_parts(news, count))
        }
    };
    // SAFETY: the caller's contract
    unsafe { queue_of(queue) }.remap(olds, news);
}

/// `SetTimedEffectHasRun`: the effect's `mEffects` index (`found` false: not in the list).
///
/// # Safety
/// `queue` must be a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_timed_set_has_run(
    queue: *mut EeScenarioTimedQueue,
    found: bool,
    effect: c_ulong,
    flag: bool,
) {
    // SAFETY: the caller's contract
    unsafe { queue_of(queue) }.set_has_run(found.then_some(effect), flag);
}

/// `EndCinematic`'s timed part (`shift` = `(long)elapsed + 3000`).
///
/// # Safety
/// `queue` must be a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_timed_end_cinematic(queue: *mut EeScenarioTimedQueue, shift: c_long) {
    // SAFETY: the caller's contract
    unsafe { queue_of(queue) }.end_cinematic(shift);
}

/// `EEScenarioManager::Reset`'s numbers and the queue emptied.
///
/// # Safety
/// `fields` must be valid for a read and a write, `queue` a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_manager_reset(
    fields: *mut EeScenarioManagerFields,
    queue: *mut EeScenarioTimedQueue,
) {
    // SAFETY: the caller's contract
    unsafe { manager::reset(&mut *fields, queue_of(queue)) };
}

/// `EEScenarioManager::Load` after its `Reset`: the first error. The numbers are read into a copy stored back after
/// the last callback; each timed effect is appended to the queue as it is read.
///
/// # Safety
/// `stream` must be valid for the call, `fields` for a read and a write, `queue` a live queue no callback touches.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_manager_load(
    stream: *const EeScenarioStream,
    fields: *mut EeScenarioManagerFields,
    queue: *mut EeScenarioTimedQueue,
    minor: c_ulong,
) -> i32 {
    // SAFETY: the caller's contract
    let (mut s, mut f) = unsafe { (stream_of(stream), *fields) };
    let r = manager::load(&mut s, &mut f, minor, &mut |e| {
        // SAFETY: the caller's contract; the borrow ends with the push
        unsafe { queue_of(queue) }.entries.push(e)
    });
    // SAFETY: the caller's contract
    unsafe { *fields = f };
    r
}

/// `EEScenarioManager::Save`: the first error.
///
/// # Safety
/// `stream` must be valid for the call, `fields` for a read, `queue` a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_manager_save(
    stream: *const EeScenarioStream,
    fields: *const EeScenarioManagerFields,
    queue: *mut EeScenarioTimedQueue,
    minor: c_ulong,
) -> i32 {
    // SAFETY: the caller's contract; the entries are copied before the first callback
    let (mut s, f, timed) = unsafe { (stream_of(stream), *fields, queue_of(queue).entries.clone()) };
    manager::save(&mut s, &f, &timed, minor)
}

/// `EEScenarioManager::Update` (round.rs): the clocks and, every 500 ms while a human plays, the trigger round.
///
/// # Safety
/// `host` must be valid for the call: its `fields` and `queue` the live manager's, reached only between callbacks.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_manager_update(host: *const EeScenarioRoundHost) {
    // SAFETY: the caller's contract
    let mut h = unsafe {
        EeScenarioRoundHost { ctx: (*host).ctx, fields: (*host).fields, queue: (*host).queue, op: (*host).op }
    };
    round::update(&mut h);
}

/// `EESMEffect::SetPlayerAttribute` (player.rs): its writes through the host's `write`.
///
/// # Safety
/// `host` must be valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_set_player_attribute(
    host: *const EeScenarioHost,
    player: c_long,
    attribute: c_ulong,
    mode: c_long,
    amount: c_long,
) {
    // SAFETY: the caller's contract
    let mut h = unsafe { host_of(host) };
    player::set_player_attribute(&mut h, player, attribute, mode, amount);
}

/// `EESMObject::Evaluate`'s body (objeval.rs): the object's units by the host's operations, its numbers through
/// `fields`.
///
/// # Safety
/// `host` must be valid for the call, `fields` the live object's numbers.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_object_evaluate(
    host: *const EeScenarioHost,
    fields: *mut EeScenarioObjectFields,
    include_dead: bool,
) -> bool {
    // SAFETY: the caller's contract
    let mut t = ObjectTable { host: unsafe { host_of(host) }, fields };
    objeval::evaluate(&mut t, include_dead)
}

/// `EESMEffect::ExpandTokens` (tokens.rs): `text` (a C string) with its tags expanded, a new C string the caller frees
/// with [`ee_scenario_text_free`].
///
/// # Safety
/// `host` must be valid for the call, `text` a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_expand_tokens(
    host: *const EeScenarioHost,
    text: *const std::os::raw::c_char,
) -> *mut std::os::raw::c_char {
    // SAFETY: the caller's contract
    let (mut h, bytes) = unsafe { (host_of(host), std::ffi::CStr::from_ptr(text).to_bytes()) };
    let out = tokens::expand_tokens(&mut h, bytes);
    std::ffi::CString::new(out).unwrap_or_default().into_raw()
}

/// Frees a text [`ee_scenario_expand_tokens`] returned (NULL is ignored).
///
/// # Safety
/// `text` must come from `ee_scenario_expand_tokens` and not be freed before.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_text_free(text: *mut std::os::raw::c_char) {
    if !text.is_null() {
        // SAFETY: the caller's contract: a CString's raw pointer
        drop(unsafe { std::ffi::CString::from_raw(text) });
    }
}

/// `EESMObject::GetUnitAttribute` for the host's unit (not NULL; attribute.rs): its members through
/// `EE_SCENARIO_READ_UNIT_FIELD`.
///
/// # Safety
/// `host` must be valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_unit_attribute(
    host: *const EeScenarioHost,
    attribute: u32,
    percent: bool,
) -> c_long {
    // SAFETY: the caller's contract
    let mut h = unsafe { host_of(host) };
    attribute::unit_attribute(&mut h, attribute, percent) as c_long
}

/// `EESMObject::FindPlacement`'s body (placement.rs) for the object's numbers `fields` and its area's record (`area`:
/// NULL for no area): `EE_SCENARIO_PLACEMENT_*`; `location` (x, y) written as the original wrote it.
///
/// # Safety
/// `host` must be valid for the call, `fields` (and `area` unless NULL) for a read, `location` for a read and a write.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_find_placement(
    host: *const EeScenarioHost,
    fields: *const EeScenarioObjectFields,
    area: *const EeScenarioAreaRecord,
    unit_type: u32,
    try_random: bool,
    location: *mut [f32; 2],
) -> u32 {
    // SAFETY: the caller's contract; the records and the location are copied before the first read
    let (mut h, f, a, mut at) = unsafe { (host_of(host), *fields, area.as_ref().copied(), *location) };
    let result = placement::find_placement(&mut h, &f, a.as_ref(), unit_type, try_random, &mut at);
    // SAFETY: the caller's contract
    unsafe { *location = at };
    result
}

/// `EESMArea::GetTiles`' rebuild of the area's cache (area.rs tiles): each tile through `EE_SCENARIO_WRITE_TILE`.
///
/// # Safety
/// `host` and `area` must be valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_area_tiles(host: *const EeScenarioHost, area: *const EeScenarioAreaRecord) {
    // SAFETY: the caller's contract; the record is copied before the first read
    let (mut h, a) = unsafe { (host_of(host), *area) };
    for (x, y) in area::tiles(&mut h, &a) {
        h.write(area::EE_SCENARIO_WRITE_TILE, 0, x as i64, y as i64, 0);
    }
}

/// `EESMEffect::CreateUnits` over object `object` with its numbers `fields` (create.rs): whether it created units;
/// each unit through `EE_SCENARIO_WRITE_CREATE_UNIT`.
///
/// # Safety
/// `host` must be valid for the call, `fields` for a read.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_create_units(
    host: *const EeScenarioHost,
    object: i32,
    fields: *const EeScenarioObjectFields,
) -> bool {
    // SAFETY: the caller's contract; the numbers are copied before the first read
    let (mut h, f) = unsafe { (host_of(host), *fields) };
    create::create_units(&mut h, object, &f)
}
