//! The C ABI the scenario manager's C++ classes call (`include/rust/ee_scenario.h`; the host in
//! `Scenario Manager/EEScenarioHost.cpp`). Plain records in and out; the world through the host's reads, and no
//! Rust reference lives across one (a read may run a condition's or an object's `Evaluate`, which calls back here).

use std::os::raw::{c_long, c_ulong};

use crate::scenario::area::EeScenarioAreaRecord;
use crate::scenario::attribute;
use crate::scenario::condition::{self, EeScenarioCondition};
use crate::scenario::create;
use crate::scenario::effect;
use crate::scenario::fields::{
    self, EeScenarioAreaSave, EeScenarioConditionFields, EeScenarioEffectFields, EeScenarioObjectFields,
    EeScenarioTriggerFields,
};
use crate::scenario::host::EeScenarioHost;
use crate::scenario::manager::{self, EeScenarioManagerFields, EeScenarioTimedEntry, EeScenarioTimedQueue};
use crate::scenario::object::{self, EeScenarioObjectFilter};
use crate::scenario::objeval::{self, ObjectTable};
use crate::scenario::placement;
use crate::scenario::player;
use crate::scenario::round::{self, EeScenarioRoundHost};
use crate::scenario::save;
use crate::scenario::stream::EeScenarioStream;
use crate::scenario::tokens;

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
        EeScenarioHost {
            ctx: h.ctx,
            read: h.read,
            players: h.players,
            write: h.write,
            tiles: h.tiles,
            world: h.world,
            object: h.object,
        }
    }
}

// SAFETY (every function taking pointers below): the caller passes valid, aligned pointers for the documented reads
// and writes, as the C++ host does from its own locals; the host table's callbacks are callable with its context.

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

/// `EESMDialogMessage::Load` (`save` false) or `::Save`: the first error (save.rs `message_io`).
///
/// # Safety
/// `stream` must be valid for the call, `numbers`' pointers for a read and a write.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_message_io(
    stream: *const EeScenarioStream,
    numbers: *const save::EeScenarioMessageNumbers,
    major: c_ulong,
    minor: c_ulong,
    save: bool,
) -> i32 {
    // SAFETY: the caller's contract
    let (mut s, p) = unsafe { (stream_of(stream), &*numbers) };
    // SAFETY: as above: the members' addresses, copied in and written back once
    let mut n = unsafe {
        save::MessageNumbers {
            scroll_speed: *p.scroll_speed,
            unit_type: *p.unit_type,
            duration: *p.duration,
            unit_id: *p.unit_id,
            cinematic: *p.cinematic,
        }
    };
    let r = save::message_io(&mut s, &mut n, major, minor, save);
    if !save {
        // SAFETY: as above
        unsafe {
            *p.scroll_speed = n.scroll_speed;
            *p.unit_type = n.unit_type;
            *p.duration = n.duration;
            *p.unit_id = n.unit_id;
            *p.cinematic = n.cinematic;
        }
    }
    r
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

/// `EEScenarioManager::ShowDialogMessage` (round.rs `show_dialog`) over the round's host.
///
/// # Safety
/// As [`ee_scenario_manager_update`].
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_show_dialog(host: *const EeScenarioRoundHost) {
    // SAFETY: the caller's contract
    let mut h = unsafe {
        EeScenarioRoundHost { ctx: (*host).ctx, fields: (*host).fields, queue: (*host).queue, op: (*host).op }
    };
    round::show_dialog(&mut h);
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

/// `EESMEffect::Execute`'s body (effect.rs): the effect's numbers `fields` (copied on entry), whether it runs in a
/// cinematic.
///
/// # Safety
/// `host` must be valid for the call, `fields` for a read.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_effect_execute(
    host: *const EeScenarioHost,
    fields: *const EeScenarioEffectFields,
    cinematic: bool,
) {
    // SAFETY: the caller's contract; the numbers are copied before the first call
    let (mut h, f) = unsafe { (host_of(host), *fields) };
    effect::execute(&mut h, &f, cinematic);
}

/// `EEChangeUnit(attribute, percent, unit, minimum, maximum, random, mode)` (unit_change.rs) on the host context's
/// unit.
///
/// # Safety
/// `host` must be a valid host table whose context holds the unit, for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_change_unit(
    host: *const EeScenarioHost,
    attribute: c_ulong,
    percent: bool,
    minimum: c_long,
    maximum: c_ulong,
    random: bool,
    mode: c_long,
) {
    // SAFETY: the caller's contract
    let mut h = unsafe { host_of(host) };
    crate::scenario::unit_change::change_unit(&mut h, attribute, percent, minimum, maximum, random, mode);
}
