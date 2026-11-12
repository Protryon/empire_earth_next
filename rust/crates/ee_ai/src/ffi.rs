//! The C ABI (`EECPNextLua.cpp`, `EECPNextUnits.cpp`, `EECPEngine.cpp`, `EECPNext.cpp`). The C++ keeps its objects
//! and hands plain records over: a save block's state as a view of its fields (pointers to its own lists, valid
//! during the call), a decode as a Rust object whose view the C++ copies back and then frees. No Rust reference
//! outlives a call; the one callback (a script's file exists) runs inside `ee_ai_manifest_read`.

use std::ffi::{c_char, c_void};

use crate::manifest::{self, Manifest};
use crate::names::{self, Names};
use crate::next_save::{self, EeAiNextEvent, EeAiNextSpot, Group, NextSave, OrderResult, RESOURCES, RING};
use crate::seat;
use crate::units_save::{self, EeAiUnitsEvent, EeAiUnitsLimits, EeAiUnitsMark, EeAiUnitsRemembered, UnitsSave};

/// Bytes the other side owns (`data` may be NULL when `len` is 0).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAiBytes {
    pub data: *const u8,
    pub len: usize,
}

impl EeAiBytes {
    const EMPTY: EeAiBytes = EeAiBytes { data: std::ptr::null(), len: 0 };
    fn of(b: &[u8]) -> EeAiBytes {
        EeAiBytes { data: b.as_ptr(), len: b.len() }
    }
    /// Safety: `data` points to `len` readable bytes (or `len` is 0).
    unsafe fn get<'a>(&self) -> &'a [u8] {
        slice(self.data, self.len)
    }
}

/// Safety: `p` points to `n` readable values (or `n` is 0).
unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if n == 0 || p.is_null() {
        &[]
    } else {
        std::slice::from_raw_parts(p, n)
    }
}

/// Writes `b` into `out` when `cap` holds it -> its length.
unsafe fn give(b: &[u8], out: *mut u8, cap: usize) -> usize {
    if !out.is_null() && cap >= b.len() {
        std::ptr::copy_nonoverlapping(b.as_ptr(), out, b.len());
    }
    b.len()
}

// ---------------------------------------------------------------------------------------------------------------
// "EENS"

/// An order's result in a next state's view.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAiNextResult {
    pub verb: EeAiBytes,
    pub code: i32,
    pub reason: i32,
    pub count: i32,
    pub group: i32,
}

/// A group in a next state's view (`aims` as many as `units`).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAiNextGroup {
    pub fields: [i32; 14],
    pub units: *const i32,
    pub aims: *const i32,
    pub unit_count: usize,
    pub skips: *const i32,
    pub skip_count: usize,
}

/// The next engine's state as the save block holds it (next_save.rs; `version` set by a decode, not read by the
/// encode). `income_ring` is [60][5] by step, then resource.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAiNextSave {
    pub version: i32,
    pub claim_range: i32,
    pub reserve: [i32; 5],
    pub food_prefer: i32,
    pub food_land: i32,
    pub epoch: i32,
    pub epoch_start: u32,
    pub next_repair_time: u32,
    pub income_total: [i32; 5],
    pub income_ring: [i32; 300],
    pub income_steps: i32,
    pub seen_army: [i32; 64],
    pub seen_strength: [i32; 64],
    pub last_seen: [u32; 64],
    pub seen_town_x: [i32; 64],
    pub seen_town_y: [i32; 64],
    pub events: *const EeAiNextEvent,
    pub event_count: usize,
    pub test: [i32; 4],
    pub script: EeAiBytes,
    pub started: bool,
    pub failed: bool,
    pub results: *const EeAiNextResult,
    pub result_count: usize,
    pub memory: EeAiBytes,
    pub next_group: i32,
    pub groups: *const EeAiNextGroup,
    pub group_count: usize,
    pub spots: *const EeAiNextSpot,
    pub spot_count: usize,
    pub towers: *const EeAiNextSpot,
    pub tower_count: usize,
    pub seen_citizens: [i32; 64],
    pub army_x: [i32; 64],
    pub army_y: [i32; 64],
    pub threats: *const EeAiNextSpot,
    pub threat_count: usize,
    pub threats_time: i32,
    pub explore: *const EeAiNextSpot,
    pub explore_count: usize,
}

/// Safety: every pointer of `v` points to its count of values.
unsafe fn next_from_view(v: &EeAiNextSave) -> NextSave {
    let mut ring = [[0; RESOURCES]; RING];
    for (k, step) in ring.iter_mut().enumerate() {
        step.copy_from_slice(&v.income_ring[k * RESOURCES..(k + 1) * RESOURCES]);
    }
    NextSave {
        version: v.version,
        claim_range: v.claim_range,
        reserve: v.reserve,
        food_prefer: v.food_prefer,
        food_land: v.food_land,
        epoch: v.epoch,
        epoch_start: v.epoch_start,
        next_repair_time: v.next_repair_time,
        income_total: v.income_total,
        income_ring: ring,
        income_steps: v.income_steps,
        seen_army: v.seen_army,
        seen_strength: v.seen_strength,
        last_seen: v.last_seen,
        seen_town_x: v.seen_town_x,
        seen_town_y: v.seen_town_y,
        events: slice(v.events, v.event_count).to_vec(),
        test: v.test,
        script: v.script.get().to_vec(),
        started: v.started,
        failed: v.failed,
        results: slice(v.results, v.result_count)
            .iter()
            .map(|r| OrderResult {
                verb: r.verb.get().to_vec(),
                code: r.code,
                reason: r.reason,
                count: r.count,
                group: r.group,
            })
            .collect(),
        memory: v.memory.get().to_vec(),
        next_group: v.next_group,
        groups: slice(v.groups, v.group_count)
            .iter()
            .map(|g| Group {
                fields: g.fields,
                units: slice(g.units, g.unit_count).to_vec(),
                aims: slice(g.aims, g.unit_count).to_vec(),
                skips: slice(g.skips, g.skip_count).to_vec(),
            })
            .collect(),
        spots: slice(v.spots, v.spot_count).to_vec(),
        towers: slice(v.towers, v.tower_count).to_vec(),
        seen_citizens: v.seen_citizens,
        army_x: v.army_x,
        army_y: v.army_y,
        threats: slice(v.threats, v.threat_count).to_vec(),
        threats_time: v.threats_time,
        explore: slice(v.explore, v.explore_count).to_vec(),
    }
}

/// A decoded next state: the state and the view of it the C++ reads.
pub struct EeAiNextDecoded {
    state: NextSave,
    results: Vec<EeAiNextResult>,
    groups: Vec<EeAiNextGroup>,
    view: EeAiNextSave,
}

fn next_decoded(state: NextSave) -> Box<EeAiNextDecoded> {
    let mut d = Box::new(EeAiNextDecoded {
        state,
        results: Vec::new(),
        groups: Vec::new(),
        // SAFETY: a record of integers, bools and pointers, all zero is a valid value (filled below)
        view: unsafe { std::mem::zeroed() },
    });
    let s = &d.state;
    let results: Vec<EeAiNextResult> = s
        .results
        .iter()
        .map(|r| EeAiNextResult {
            verb: EeAiBytes::of(&r.verb),
            code: r.code,
            reason: r.reason,
            count: r.count,
            group: r.group,
        })
        .collect();
    let groups: Vec<EeAiNextGroup> = s
        .groups
        .iter()
        .map(|g| EeAiNextGroup {
            fields: g.fields,
            units: g.units.as_ptr(),
            aims: g.aims.as_ptr(),
            unit_count: g.units.len(),
            skips: g.skips.as_ptr(),
            skip_count: g.skips.len(),
        })
        .collect();
    let mut ring = [0; RING * RESOURCES];
    for (k, step) in s.income_ring.iter().enumerate() {
        ring[k * RESOURCES..(k + 1) * RESOURCES].copy_from_slice(step);
    }
    let view = EeAiNextSave {
        version: s.version,
        claim_range: s.claim_range,
        reserve: s.reserve,
        food_prefer: s.food_prefer,
        food_land: s.food_land,
        epoch: s.epoch,
        epoch_start: s.epoch_start,
        next_repair_time: s.next_repair_time,
        income_total: s.income_total,
        income_ring: ring,
        income_steps: s.income_steps,
        seen_army: s.seen_army,
        seen_strength: s.seen_strength,
        last_seen: s.last_seen,
        seen_town_x: s.seen_town_x,
        seen_town_y: s.seen_town_y,
        events: s.events.as_ptr(),
        event_count: s.events.len(),
        test: s.test,
        script: EeAiBytes::of(&s.script),
        started: s.started,
        failed: s.failed,
        results: results.as_ptr(),
        result_count: results.len(),
        memory: EeAiBytes::of(&s.memory),
        next_group: s.next_group,
        groups: groups.as_ptr(),
        group_count: groups.len(),
        spots: s.spots.as_ptr(),
        spot_count: s.spots.len(),
        towers: s.towers.as_ptr(),
        tower_count: s.towers.len(),
        seen_citizens: s.seen_citizens,
        army_x: s.army_x,
        army_y: s.army_y,
        threats: s.threats.as_ptr(),
        threat_count: s.threats.len(),
        threats_time: s.threats_time,
        explore: s.explore.as_ptr(),
        explore_count: s.explore.len(),
    };
    // the vectors' buffers do not move when the vectors move into the box
    d.results = results;
    d.groups = groups;
    d.view = view;
    d
}

/// The "EENS" block of a state into `out` when `cap` holds it -> the block's length (call with `cap` 0 to size it).
///
/// # Safety
/// `view` points to a state whose pointers point to their counts of values; `out` to `cap` writable bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_save_encode(view: *const EeAiNextSave, out: *mut u8, cap: usize) -> usize {
    give(&next_save::encode(&next_from_view(&*view)), out, cap)
}

/// The state of an "EENS" block (NULL: not one; `max_results`: the results kept, `kNextMaxOrders`). Read it with
/// `ee_ai_next_decoded_view`, free it with `ee_ai_next_decoded_free`.
///
/// # Safety
/// `data` points to `len` readable bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_save_decode(
    data: *const u8,
    len: usize,
    max_results: usize,
) -> *mut EeAiNextDecoded {
    match next_save::decode(slice(data, len), max_results) {
        Some(s) => Box::into_raw(next_decoded(s)),
        None => std::ptr::null_mut(),
    }
}

/// The view of a decoded state (valid until it is freed).
///
/// # Safety
/// `d` is a live result of `ee_ai_next_save_decode`.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_decoded_view(d: *const EeAiNextDecoded) -> *const EeAiNextSave {
    &(*d).view
}

/// Frees a decoded state (NULL: nothing).
///
/// # Safety
/// `d` is NULL or a live result of `ee_ai_next_save_decode`, not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_decoded_free(d: *mut EeAiNextDecoded) {
    if !d.is_null() {
        drop(Box::from_raw(d));
    }
}

// ---------------------------------------------------------------------------------------------------------------
// "EENU"

/// The per-unit interface's state as its save block holds it (units_save.rs): the lists in the C++ containers'
/// order; `forward` is (old id, new id, game ms) triples, `hurt` (place, game ms) pairs, `trades` (minute, cell,
/// lost, dealt) quadruples, each count in entries.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAiUnitsSave {
    pub version: i32,
    pub held: *const i32,
    pub held_count: usize,
    pub events: *const EeAiUnitsEvent,
    pub event_count: usize,
    pub forward: *const i32,
    pub forward_count: usize,
    pub memory: *const EeAiUnitsRemembered,
    pub memory_count: usize,
    pub memory_ms: i32,
    pub memory_next: i32,
    pub reflex_naval_flee: bool,
    pub reflex_danger: bool,
    pub micro_next: i32,
    pub hurt: *const i32,
    pub hurt_count: usize,
    pub marks: *const EeAiUnitsMark,
    pub mark_count: usize,
    pub trades: *const i32,
    pub trade_count: usize,
}

/// Safety: every pointer of `v` points to its count of entries.
unsafe fn units_from_view(v: &EeAiUnitsSave) -> UnitsSave {
    UnitsSave {
        version: v.version,
        held: slice(v.held, v.held_count).to_vec(),
        events: slice(v.events, v.event_count).to_vec(),
        forward: slice(v.forward, v.forward_count * 3).as_chunks::<3>().0.to_vec(),
        memory: slice(v.memory, v.memory_count).to_vec(),
        memory_ms: v.memory_ms,
        memory_next: v.memory_next,
        reflex_naval_flee: v.reflex_naval_flee,
        reflex_danger: v.reflex_danger,
        micro_next: v.micro_next,
        hurt: slice(v.hurt, v.hurt_count * 2).as_chunks::<2>().0.to_vec(),
        marks: slice(v.marks, v.mark_count).to_vec(),
        trades: slice(v.trades, v.trade_count * 4).as_chunks::<4>().0.to_vec(),
    }
}

/// A decoded per-unit state and its view.
pub struct EeAiUnitsDecoded {
    state: UnitsSave,
    view: EeAiUnitsSave,
}

/// The "EENU" block of a state into `out` when `cap` holds it -> the block's length (call with `cap` 0 to size it).
///
/// # Safety
/// `view` points to a state whose pointers point to their counts of entries; `out` to `cap` writable bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_save_encode(view: *const EeAiUnitsSave, out: *mut u8, cap: usize) -> usize {
    give(&units_save::encode(&units_from_view(&*view)), out, cap)
}

/// The state of an "EENU" block (NULL: not one). Read it with `ee_ai_units_decoded_view`, free it with
/// `ee_ai_units_decoded_free`.
///
/// # Safety
/// `data` points to `len` readable bytes, `limits` to the limits.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_save_decode(
    data: *const u8,
    len: usize,
    limits: *const EeAiUnitsLimits,
) -> *mut EeAiUnitsDecoded {
    let Some(state) = units_save::decode(slice(data, len), &*limits) else {
        return std::ptr::null_mut();
    };
    let view = EeAiUnitsSave {
        version: state.version,
        held: state.held.as_ptr(),
        held_count: state.held.len(),
        events: state.events.as_ptr(),
        event_count: state.events.len(),
        forward: state.forward.as_ptr().cast(),
        forward_count: state.forward.len(),
        memory: state.memory.as_ptr(),
        memory_count: state.memory.len(),
        memory_ms: state.memory_ms,
        memory_next: state.memory_next,
        reflex_naval_flee: state.reflex_naval_flee,
        reflex_danger: state.reflex_danger,
        micro_next: state.micro_next,
        hurt: state.hurt.as_ptr().cast(),
        hurt_count: state.hurt.len(),
        marks: state.marks.as_ptr(),
        mark_count: state.marks.len(),
        trades: state.trades.as_ptr().cast(),
        trade_count: state.trades.len(),
    };
    // the vectors' buffers do not move when the state moves into the box
    Box::into_raw(Box::new(EeAiUnitsDecoded { state, view }))
}

/// The view of a decoded per-unit state (valid until it is freed).
///
/// # Safety
/// `d` is a live result of `ee_ai_units_save_decode`.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_decoded_view(d: *const EeAiUnitsDecoded) -> *const EeAiUnitsSave {
    &(*d).view
}

/// Frees a decoded per-unit state (NULL: nothing).
///
/// # Safety
/// `d` is NULL or a live result of `ee_ai_units_save_decode`, not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_decoded_free(d: *mut EeAiUnitsDecoded) {
    if !d.is_null() {
        let d = Box::from_raw(d);
        let _ = &d.state;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The manifest and the seats

/// An AI of a manifest read (the bytes are the manifest's, valid until it is freed).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAiManifestEntry {
    pub id: EeAiBytes,
    pub name: EeAiBytes,
    pub script: EeAiBytes,
    pub description: EeAiBytes,
    pub random: bool,
    pub sees_all: bool,
    pub fills_garrison: bool,
}

/// A manifest read: its entries and log lines.
pub struct EeAiManifest(Manifest);

/// Whether a script module's file exists (`ctx` is the caller's).
pub type EeAiScriptExists = unsafe extern "C" fn(ctx: *mut c_void, module: *const u8, len: usize) -> bool;

/// Reads the manifest document `text` of the file `path` (manifest.rs) -> the entries and log lines, freed with
/// `ee_ai_manifest_free`. `exists` is called for each entry that gets that far, inside this call.
///
/// # Safety
/// `text` and `path` point to their lengths of bytes; `exists` is callable with `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_manifest_read(
    text: EeAiBytes,
    path: EeAiBytes,
    exists: EeAiScriptExists,
    ctx: *mut c_void,
) -> *mut EeAiManifest {
    let m = manifest::read(text.get(), path.get(), &mut |module| exists(ctx, module.as_ptr(), module.len()));
    Box::into_raw(Box::new(EeAiManifest(m)))
}

/// The entries of a manifest read.
///
/// # Safety
/// `m` is a live manifest.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_manifest_count(m: *const EeAiManifest) -> usize {
    (*m).0.entries.len()
}

/// Entry `k` of a manifest read (false: none).
///
/// # Safety
/// `m` is a live manifest; `out` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_manifest_entry(m: *const EeAiManifest, k: usize, out: *mut EeAiManifestEntry) -> bool {
    let m = &*m;
    let Some(e) = m.0.entries.get(k) else {
        return false;
    };
    *out = EeAiManifestEntry {
        id: EeAiBytes::of(&e.id),
        name: EeAiBytes::of(&e.name),
        script: EeAiBytes::of(&e.script),
        description: EeAiBytes::of(&e.description),
        random: e.random,
        sees_all: e.sees_all,
        fills_garrison: e.fills_garrison,
    };
    true
}

/// Whether the document had its "ais" array (false: the read stopped at the document, its one log line says why).
///
/// # Safety
/// `m` is a live manifest.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_manifest_listed(m: *const EeAiManifest) -> bool {
    (*m).0.listed
}

/// The log lines of a manifest read.
///
/// # Safety
/// `m` is a live manifest.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_manifest_log_count(m: *const EeAiManifest) -> usize {
    (*m).0.log.len()
}

/// Log line `k` of a manifest read (empty: none).
///
/// # Safety
/// `m` is a live manifest.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_manifest_log(m: *const EeAiManifest, k: usize) -> EeAiBytes {
    let m = &*m;
    m.0.log.get(k).map_or(EeAiBytes::EMPTY, |l| EeAiBytes::of(l))
}

/// Frees a manifest read (NULL: nothing).
///
/// # Safety
/// `m` is NULL or a live manifest, not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_manifest_free(m: *mut EeAiManifest) {
    if !m.is_null() {
        drop(Box::from_raw(m));
    }
}

/// `EECPChoiceByName` over the manifest's ids (seat.rs) -> the choice, -1 for none.
///
/// # Safety
/// `name` and each of the `count` ids point to their bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_choice_by_name(name: EeAiBytes, ids: *const EeAiBytes, count: usize) -> i64 {
    let ids: Vec<&[u8]> = slice(ids, count).iter().map(|b| b.get()).collect();
    seat::choice_by_name(name.get(), &ids)
}

/// `EECPEngineResolve` (seat.rs): `overrides` EE_AI_ENGINE's choices (count 0: none), `random` each manifest
/// entry's flag.
///
/// # Safety
/// `overrides` and `random` point to their counts of values.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_engine_resolve(
    choice: u64,
    seed: u64,
    slot: u64,
    player_id: u64,
    overrides: *const i64,
    override_count: usize,
    random: *const bool,
    random_count: usize,
) -> u64 {
    seat::resolve(choice, seed, slot, player_id, slice(overrides, override_count), slice(random, random_count))
}

// ---------------------------------------------------------------------------------------------------------------
// The name tables

/// `EECPNextCodeName`.
#[no_mangle]
pub extern "C" fn ee_ai_code_name(code: i64) -> *const c_char {
    names::code_name(code).as_ptr()
}

/// `EECPNextReasonName`.
#[no_mangle]
pub extern "C" fn ee_ai_reason_name(reason: i64) -> *const c_char {
    names::reason_name(reason).as_ptr()
}

/// An event kind's name: "?" for an unknown kind in the trace (`trace`), "unknown" in the script's view.
#[no_mangle]
pub extern "C" fn ee_ai_event_name(kind: i64, trace: bool) -> *const c_char {
    names::event_name(kind, if trace { c"?" } else { c"unknown" }).as_ptr()
}

/// Resource `index`'s name (0..4: food, wood, stone, gold, iron; NULL past them).
#[no_mangle]
pub extern "C" fn ee_ai_resource_name(index: usize) -> *const c_char {
    names::RESOURCES.get(index).map_or(std::ptr::null(), |r| r.as_ptr())
}

/// A names table (names.rs).
pub struct EeAiNames(Names);

/// The names table of `count` rows (key, data id) with the prefix of a key without an id; freed with
/// `ee_ai_names_free`.
///
/// # Safety
/// `keys` and `ids` point to `count` values, `prefix` and each id to their bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_names_new(
    keys: *const i64,
    ids: *const EeAiBytes,
    count: usize,
    prefix: EeAiBytes,
) -> *mut EeAiNames {
    let keys = slice(keys, count);
    let ids = slice(ids, count);
    let rows = keys.iter().zip(ids).map(|(&k, id)| (k, id.get().to_vec()));
    Box::into_raw(Box::new(EeAiNames(Names::new(rows, prefix.get()))))
}

/// The name of a key: the table's bytes, or "<prefix><key>" written into `buf` (room for `cap`; truncated past it).
/// `*out_len` is its length.
///
/// # Safety
/// `t` is a live table, `buf` `cap` writable bytes, `out_len` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_names_name_of(
    t: *const EeAiNames,
    key: i64,
    buf: *mut u8,
    cap: usize,
    out_len: *mut usize,
) -> *const u8 {
    match (*t).0.name_of(key) {
        std::borrow::Cow::Borrowed(b) => {
            *out_len = b.len();
            b.as_ptr()
        }
        std::borrow::Cow::Owned(b) => {
            let n = b.len().min(cap);
            std::ptr::copy_nonoverlapping(b.as_ptr(), buf, n);
            *out_len = n;
            buf
        }
    }
}

/// The key of a name, -1 for none.
///
/// # Safety
/// `t` is a live table, `name` points to its bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_names_key_of(t: *const EeAiNames, name: EeAiBytes) -> i64 {
    (*t).0.key_of(name.get())
}

/// Frees a names table (NULL: nothing).
///
/// # Safety
/// `t` is NULL or a live table, not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_names_free(t: *mut EeAiNames) {
    if !t.is_null() {
        drop(Box::from_raw(t));
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::next_save::GROUP_FIELDS;

    #[test]
    fn next_view_round_trip() {
        let mut s = NextSave { claim_range: 30, threats_time: 5, ..NextSave::default() };
        s.income_ring[59][4] = 77;
        s.groups.push(Group { fields: [1; GROUP_FIELDS], units: vec![4, 5], aims: vec![0, 6], skips: vec![] });
        s.results.push(OrderResult { verb: b"build".to_vec(), code: 2, reason: 3, count: 0, group: 0 });
        let bytes = next_save::encode(&s);
        unsafe {
            let d = ee_ai_next_save_decode(bytes.as_ptr(), bytes.len(), 200);
            assert!(!d.is_null());
            let v = &*ee_ai_next_decoded_view(d);
            assert_eq!(v.income_ring[59 * 5 + 4], 77);
            assert_eq!(next_from_view(v), s);
            let n = ee_ai_next_save_encode(v, std::ptr::null_mut(), 0);
            let mut out = vec![0u8; n];
            assert_eq!(ee_ai_next_save_encode(v, out.as_mut_ptr(), n), n);
            assert_eq!(out, bytes);
            ee_ai_next_decoded_free(d);
            assert!(ee_ai_next_save_decode(bytes.as_ptr(), bytes.len() - 1, 200).is_null());
        }
    }

    #[test]
    fn units_view_round_trip() {
        let s = UnitsSave {
            version: 3,
            held: vec![1, 2],
            forward: vec![[1, 2, 3], [4, 5, 6]],
            hurt: vec![[7, 8]],
            trades: vec![[1, 2, 3, 4]],
            ..UnitsSave::default()
        };
        let limits = EeAiUnitsLimits {
            max_events: 512,
            max_remembered: 4000,
            max_trades: 1024,
            event_kind_min: 1,
            event_kind_max: 7,
        };
        let bytes = units_save::encode(&s);
        unsafe {
            let d = ee_ai_units_save_decode(bytes.as_ptr(), bytes.len(), &limits);
            let v = &*ee_ai_units_decoded_view(d);
            assert_eq!(units_from_view(v), s);
            let mut out = vec![0u8; bytes.len()];
            assert_eq!(ee_ai_units_save_encode(v, out.as_mut_ptr(), out.len()), bytes.len());
            assert_eq!(out, bytes);
            ee_ai_units_decoded_free(d);
        }
    }

    unsafe extern "C" fn all_exist(_: *mut c_void, _: *const u8, _: usize) -> bool {
        true
    }

    #[test]
    fn manifest_and_names() {
        let text = br#"{"ais": [{"id": "boom", "script": "boom", "random": true}, {"id": "x"}]}"#;
        unsafe {
            let m = ee_ai_manifest_read(EeAiBytes::of(text), EeAiBytes::of(b"p"), all_exist, std::ptr::null_mut());
            assert_eq!(ee_ai_manifest_count(m), 1);
            let mut e: EeAiManifestEntry = std::mem::zeroed();
            assert!(ee_ai_manifest_entry(m, 0, &mut e));
            assert_eq!(e.name.get(), b"boom");
            assert!(e.random);
            assert!(!ee_ai_manifest_entry(m, 1, &mut e));
            assert_eq!(ee_ai_manifest_log_count(m), 1);
            assert!(ee_ai_manifest_log(m, 0).get().starts_with(b"p: ais[1]: script '' is not a module name"));
            ee_ai_manifest_free(m);
            let ids = [EeAiBytes::of(b"boom")];
            assert_eq!(ee_ai_choice_by_name(EeAiBytes::of(b"boom"), ids.as_ptr(), 1), 2);
            assert_eq!(ee_ai_engine_resolve(2, 0, 0, 1, std::ptr::null(), 0, [true].as_ptr(), 1), 2);
            assert_eq!(std::ffi::CStr::from_ptr(ee_ai_reason_name(5)).to_bytes(), b"unknown_class");
            assert_eq!(std::ffi::CStr::from_ptr(ee_ai_event_name(9, true)).to_bytes(), b"?");
            assert_eq!(std::ffi::CStr::from_ptr(ee_ai_resource_name(4)).to_bytes(), b"iron");
            assert!(ee_ai_resource_name(5).is_null());
            let keys = [3i64, 4];
            let names = [EeAiBytes::of(b"citizen"), EeAiBytes::of(b"b_house")];
            let t = ee_ai_names_new(keys.as_ptr(), names.as_ptr(), 2, EeAiBytes::of(b"dbobjects_"));
            let mut buf = [0u8; 48];
            let mut n = 0;
            let p = ee_ai_names_name_of(t, 4, buf.as_mut_ptr(), buf.len(), &mut n);
            assert_eq!(slice(p, n), b"b_house");
            let p = ee_ai_names_name_of(t, 40, buf.as_mut_ptr(), buf.len(), &mut n);
            assert_eq!(slice(p, n), b"dbobjects_40");
            assert_eq!(ee_ai_names_key_of(t, EeAiBytes::of(b"citizen")), 3);
            ee_ai_names_free(t);
        }
    }
}
