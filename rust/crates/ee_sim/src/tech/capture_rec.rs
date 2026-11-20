//! TEST ONLY: the tech tree's plain records of the simulation capture (ee/EETechCapture.h; was EETechCapture.cpp's
//! `EETechCaptureNode`, `Epoch`, `Tree`, `Cost`, `Rescale`, `EECalamityCapture` and the TRTE, EVUN and TNWK
//! collections): the node's, epoch's and tree's state from their Rust records, the records made of their arguments,
//! and the steps a ResearchToEpoch, an effect's unit walk and a tree walk collect before their record.
#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64

use std::cell::RefCell;
use std::os::raw::{c_long, c_ulong};
use std::sync::Mutex;

use crate::app::capture;
use crate::tech::civ::{
    ee_tech_civ_category, ee_tech_civ_category_count, ee_tech_civ_category_id, ee_tech_civ_category_level,
    ee_tech_civ_civilization, ee_tech_civ_civilization_count, ee_tech_civ_count_in_category, ee_tech_civ_era,
    ee_tech_civ_era_count, ee_tech_civ_extra_cost, ee_tech_civ_find_upgrade_key, ee_tech_civ_from_legacy,
    ee_tech_civ_is_power_level, ee_tech_civ_level_count, ee_tech_civ_level_name_id, ee_tech_civ_power,
    ee_tech_civ_power_count, ee_tech_civ_power_level, ee_tech_civ_power_upgrade, ee_tech_civ_to_legacy,
    ee_tech_civ_upgrade_key, ee_tech_civ_upgrades, EeTechCivEntry, EeTechCivPower, EeTechCivUpgrade,
};
use crate::tech::nodes::{EeTechNodeDataFields, EeTechNodeFields, EeTechTreeFields};

/// The bytes of a node's state ([`ee_tech_capture_node`])
pub const EE_TECH_CAPTURE_NODE_SIZE: u32 = 17 * 4;
/// The bytes an epoch adds to its node's state ([`ee_tech_capture_epoch`])
pub const EE_TECH_CAPTURE_EPOCH_SIZE: u32 = 3 * 4;
/// The bytes of a tree's state ([`ee_tech_capture_tree`])
pub const EE_TECH_CAPTURE_TREE_SIZE: u32 = 5 * 4;

pub(crate) fn tag(name: &[u8; 4]) -> u32 {
    u32::from_le_bytes(*name)
}

#[derive(Default)]
pub(crate) struct Bytes(pub(crate) Vec<u8>);

impl Bytes {
    pub(crate) fn i32(&mut self, v: i64) -> &mut Self {
        self.0.extend_from_slice(&(v as i32).to_le_bytes());
        self
    }
    pub(crate) fn u32(&mut self, v: u32) -> &mut Self {
        self.0.extend_from_slice(&v.to_le_bytes());
        self
    }
    pub(crate) fn f32(&mut self, v: f32) -> &mut Self {
        self.0.extend_from_slice(&v.to_le_bytes());
        self
    }
}

/// A node's state: id, epoch, obsolete epoch, the flags (researched, obsolete, available, editor-obsoleted,
/// allowed), the research delay, the prerequisite flags (an epoch's are never set nor read: 0), the tree's
/// current/start/end epoch and epochs before wonders (-1 without a tree), the epoch count, the record's kind, its
/// flags (makes a unit, counts toward the epoch, available with the epoch, researched with it) and the four
/// prerequisites (-1 without a record).
pub(crate) fn node_bytes(
    node: &EeTechNodeFields,
    data: Option<&EeTechNodeDataFields>,
    tree: Option<&EeTechTreeFields>,
    epoch_count: c_long,
) -> Bytes {
    let mut r = Bytes::default();
    r.i32(data.map_or(-1, |d| d.mID as i64)).i32(node.mEpoch as i64).i32(node.mObsoleteEpoch as i64);
    r.u32(
        node.mIsResearched as u32
            | (node.mIsObsolete as u32) << 1
            | (node.mIsAvailable as u32) << 2
            | (node.mScenEditorObsoleted as u32) << 3
            | (node.mIsAllowed as u32) << 4,
    );
    r.i32(node.mResearchDelay as i64);
    let is_epoch = data.is_some_and(|d| d.mID >= 0 && d.mID < epoch_count);
    r.u32(if is_epoch { 0 } else { node.mPrerequisiteFlags as u32 });
    match tree {
        Some(t) => r
            .i32(t.mCurrentEpoch as i64)
            .i32(t.mStartEpoch as i64)
            .i32(t.mEndEpoch as i64)
            .i32(t.mEpochsBeforeWonders as i64),
        None => r.i32(-1).i32(-1).i32(-1).i32(-1),
    };
    r.i32(epoch_count as i64);
    match data {
        Some(d) => r
            .i32(d.mKind as i64)
            .u32(
                d.mMakesUnit as u32
                    | (d.mCountsTowardEpoch as u32) << 1
                    | (d.mAvailableWithEpoch as u32) << 2
                    | (d.mResearchedWithEpoch as u32) << 3,
            )
            .i32(d.mPrerequisite1 as i64)
            .i32(d.mPrerequisite2 as i64)
            .i32(d.mPrerequisite3 as i64)
            .i32(d.mPrerequisite4 as i64),
        None => r.i32(-1).u32(0).i32(-1).i32(-1).i32(-1).i32(-1),
    };
    r
}

pub(crate) fn tree_bytes(tree: &EeTechTreeFields, player_index: c_long) -> Bytes {
    let mut r = Bytes::default();
    r.i32(player_index as i64)
        .i32(tree.mCurrentEpoch as i64)
        .i32(tree.mStartEpoch as i64)
        .i32(tree.mEndEpoch as i64)
        .i32(tree.mEpochsBeforeWonders as i64);
    r
}

/// Writes [`node_bytes`] to `out` ([`EE_TECH_CAPTURE_NODE_SIZE`] bytes); returns their count.
///
/// # Safety
/// `node` is live; `data` and `tree` are live or null; `out` holds [`EE_TECH_CAPTURE_NODE_SIZE`] bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_capture_node(
    node: *const EeTechNodeFields,
    data: *const EeTechNodeDataFields,
    tree: *const EeTechTreeFields,
    epoch_count: c_long,
    out: *mut u8,
) -> u32 {
    // SAFETY: the caller's contract
    let r = unsafe { node_bytes(&*node, data.as_ref(), tree.as_ref(), epoch_count) };
    // SAFETY: the caller's buffer
    unsafe { std::ptr::copy_nonoverlapping(r.0.as_ptr(), out, r.0.len()) };
    r.0.len() as u32
}

/// An epoch's counters after its node's state: the required and current SOL, whether it counts SOL.
///
/// # Safety
/// `node` is live; `out` holds [`EE_TECH_CAPTURE_EPOCH_SIZE`] bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_capture_epoch(node: *const EeTechNodeFields, out: *mut u8) -> u32 {
    // SAFETY: the caller's live node
    let n = unsafe { &*node };
    let mut r = Bytes::default();
    r.i32(n.mRequiredSOL as i64).i32(n.mCurrentSOL as i64).u32(n.mIsCountingSOL as u32);
    // SAFETY: the caller's buffer
    unsafe { std::ptr::copy_nonoverlapping(r.0.as_ptr(), out, r.0.len()) };
    r.0.len() as u32
}

/// A tree's state: the player index, the current, start and end epoch, the epochs before wonders.
///
/// # Safety
/// `tree` is live; `out` holds [`EE_TECH_CAPTURE_TREE_SIZE`] bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_capture_tree(
    tree: *const EeTechTreeFields,
    player_index: c_long,
    out: *mut u8,
) -> u32 {
    // SAFETY: the caller's contract
    let r = tree_bytes(unsafe { &*tree }, player_index);
    // SAFETY: the caller's buffer
    unsafe { std::ptr::copy_nonoverlapping(r.0.as_ptr(), out, r.0.len()) };
    r.0.len() as u32
}

/// TCST: a node's (kind 0) or an epoch's (1) GetCost: the kind, the player index, the node's id, the tree's epoch
/// (-1 without a tree), the resource, the unit class, the scale and the cost.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn ee_tech_capture_cost(
    kind: c_long,
    player_index: c_long,
    id: c_long,
    epoch: c_long,
    resource: c_long,
    unit_class: c_long,
    scale: f32,
    cost: c_long,
) {
    let mut r = Bytes::default();
    r.i32(kind as i64).i32(player_index as i64).i32(id as i64).i32(epoch as i64);
    r.i32(resource as i64).i32(unit_class as i64).f32(scale).i32(cost as i64);
    capture::commit(tag(b"TCST"), &r.0);
}

/// EVHP: one unit's hit points (0) or flight time (1) rescaled by an effect.
#[no_mangle]
pub extern "C" fn ee_tech_capture_rescale(
    what: c_long,
    unit: c_ulong,
    value: c_long,
    old_maximum: c_ulong,
    new_maximum: c_long,
    amount: f32,
) {
    let mut r = Bytes::default();
    r.i32(what as i64).u32(unit as u32).i32(value as i64).u32(old_maximum as u32).i32(new_maximum as i64).f32(amount);
    capture::commit(tag(b"EVHP"), &r.0);
}

/// CALC: a calamity created (0 global, 1 location, 2 unit): class, caster, target, x, y, player, result.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn ee_tech_capture_calamity(
    kind: c_long,
    class: c_ulong,
    caster: c_ulong,
    target: c_ulong,
    x: c_long,
    y: c_long,
    player: c_ulong,
    result: bool,
) {
    let mut r = Bytes::default();
    r.i32(kind as i64).u32(class as u32).u32(caster as u32).u32(target as u32);
    r.i32(x as i64).i32(y as i64).u32(player as u32).u32(result as u32);
    capture::commit(tag(b"CALC"), &r.0);
}

// ---- the collections: TRTE's epoch steps, EVUN's units, TNWK's walks (per thread: the media loader asks too)

static EPOCH_STEPS: Mutex<Vec<i32>> = Mutex::new(Vec::new());
static UNIT_STEPS: Mutex<(Vec<u8>, u32)> = Mutex::new((Vec::new(), 0));

thread_local! {
    static WALKS: RefCell<Vec<Vec<i32>>> = const { RefCell::new(Vec::new()) };
}

/// TRTE: one step of ResearchToEpoch (0 unlock, 1 research all, 2 research self, 3 research kind 7, 4 show the
/// name); a negative step clears the collection.
#[no_mangle]
pub extern "C" fn ee_tech_capture_epoch_step(step: c_long, epoch: c_long) {
    if !capture::ee_app_capture_active() {
        return;
    }
    let mut s = EPOCH_STEPS.lock().unwrap();
    if step < 0 {
        s.clear();
        return;
    }
    s.push(step as i32);
    s.push(epoch as i32);
}

/// TRTE's head (the epoch, the tree's epoch count, whether the player is the local one, the collected steps) into
/// `out`, the collection cleared; the caller appends the tree's and the epochs' states. Returns the byte count
/// (`out` null: the count only, nothing cleared).
///
/// # Safety
/// `out` is null or holds the count's bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_tech_capture_research_to_epoch(
    epoch: c_long,
    epochs: c_long,
    local: bool,
    out: *mut u8,
) -> u32 {
    let mut s = EPOCH_STEPS.lock().unwrap();
    let mut r = Bytes::default();
    r.i32(epoch as i64).i32(epochs as i64).u32(local as u32).u32((s.len() / 2) as u32);
    for &v in s.iter() {
        r.i32(v as i64);
    }
    if !out.is_null() {
        // SAFETY: the caller's buffer of the count's bytes
        unsafe { std::ptr::copy_nonoverlapping(r.0.as_ptr(), out, r.0.len()) };
        s.clear();
    }
    r.0.len() as u32
}

/// EVUN: one unit an effect walks: its id, unit type, hit points, the flags (complex, built, on a tile, a part),
/// its flight time, whether the effect applies and the value written.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn ee_tech_capture_unit_step(
    id: c_ulong,
    unit_type: c_long,
    hit_points: c_long,
    flags: u32,
    flight_time: c_long,
    applies: bool,
    value: c_long,
) {
    if !capture::ee_app_capture_active() {
        return;
    }
    let mut s = UNIT_STEPS.lock().unwrap();
    for v in
        [id as i32, unit_type as i32, hit_points as i32, flags as i32, flight_time as i32, applies as i32, value as i32]
    {
        s.0.extend_from_slice(&v.to_le_bytes());
    }
    s.1 += 1;
}

/// EVUN's record of the walk whose units were collected (`inLoop`: 0 sight off, 1 sight on, 2 hit points, 3 pending
/// hit points, 4 flight time, 5 pending flight time, 6 area effects, 7 conversion).
#[no_mangle]
pub extern "C" fn ee_tech_capture_unit_walk(walk: c_long, effect: c_long, unit_type: c_long, a: c_long, b: c_long) {
    if !capture::ee_app_capture_active() {
        return;
    }
    let mut s = UNIT_STEPS.lock().unwrap();
    let mut r = Bytes::default();
    r.i32(walk as i64).i32(effect as i64).i32(unit_type as i64).i32(a as i64).i32(b as i64).u32(s.1);
    r.0.extend_from_slice(&s.0);
    capture::commit(tag(b"EVUN"), &r.0);
    s.0.clear();
    s.1 = 0;
}

/// TNWK: a tree walk begins (its steps nest per thread).
#[no_mangle]
pub extern "C" fn ee_tech_capture_walk_begin() {
    if capture::ee_app_capture_active() {
        WALKS.with(|w| w.borrow_mut().push(Vec::new()));
    }
}

/// TNWK: one step of the innermost walk (a node's id and what the walk did).
#[no_mangle]
pub extern "C" fn ee_tech_capture_walk_step(id: c_long, action: c_long) {
    if capture::ee_app_capture_active() {
        WALKS.with(|w| {
            if let Some(v) = w.borrow_mut().last_mut() {
                v.push(id as i32);
                v.push(action as i32);
            }
        });
    }
}

/// TNWK's (TNMT's, for the walks other threads ask: GetBuildTime, kind 6, and NeedsUnitMedia, 10) head: the kind,
/// the player index, two arguments, the result and the innermost walk's steps (taken off). Returns the bytes in a
/// buffer the caller frees with [`ee_tech_capture_free`]; the caller appends the node's and tree's states and
/// commits it under [`ee_tech_capture_walk_tag`].
#[no_mangle]
pub extern "C" fn ee_tech_capture_walk(
    kind: c_long,
    player_index: c_long,
    a: c_long,
    b: c_long,
    result: c_long,
    out_size: &mut u32,
) -> *mut u8 {
    let steps = WALKS.with(|w| w.borrow_mut().pop()).unwrap_or_default();
    let mut r = Bytes::default();
    r.i32(kind as i64)
        .i32(player_index as i64)
        .i32(a as i64)
        .i32(b as i64)
        .i32(result as i64)
        .u32((steps.len() / 2) as u32);
    for v in steps {
        r.i32(v as i64);
    }
    *out_size = r.0.len() as u32;
    Box::into_raw(r.0.into_boxed_slice()) as *mut u8
}

/// The tag of a walk's record: TNMT for the walks other threads ask too (kinds 6 and 10), else TNWK.
#[no_mangle]
pub extern "C" fn ee_tech_capture_walk_tag(kind: c_long) -> u32 {
    if kind == 6 || kind == 10 {
        tag(b"TNMT")
    } else {
        tag(b"TNWK")
    }
}

/// Frees a buffer [`ee_tech_capture_walk`] returned.
///
/// # Safety
/// `p` and `size` are a buffer and its size from [`ee_tech_capture_walk`].
#[no_mangle]
pub unsafe extern "C" fn ee_tech_capture_free(p: *mut u8, size: u32) {
    if !p.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(std::ptr::slice_from_raw_parts_mut(p, size as usize)) });
    }
}

// ---- CVMG: the civilization manager's and the powers' data and every query over it (was EECivCapture.cpp's
// CaptureManager; the premade civilizations' files, loaded through the C++, are the record's tail)

fn bytes_of<T>(v: &T) -> &[u8] {
    // SAFETY: a plain record's bytes
    unsafe { std::slice::from_raw_parts(v as *const T as *const u8, std::mem::size_of::<T>()) }
}

/// A text as the C++'s Text: its strlen, then its bytes.
fn text(r: &mut Bytes, t: &[u8]) {
    let t = &t[..t.iter().position(|&b| b == 0).unwrap_or(t.len())];
    r.u32(t.len() as u32);
    r.0.extend_from_slice(t);
}

unsafe extern "C" fn sink_bytes(ctx: *mut std::ffi::c_void, bytes: *const u8, length: usize) {
    // SAFETY: the caller's live vector and bytes
    unsafe { *(ctx as *mut Vec<u8>) = std::slice::from_raw_parts(bytes, length).to_vec() };
}

unsafe extern "C" fn sink_upgrade(ctx: *mut std::ffi::c_void, upgrade: EeTechCivUpgrade) {
    // SAFETY: the caller's live vector
    unsafe { (*(ctx as *mut Vec<EeTechCivUpgrade>)).push(upgrade) };
}

fn find_key(key: &[u8]) -> (bool, EeTechCivUpgrade) {
    let mut back: EeTechCivUpgrade = 0xdeadbeef;
    // SAFETY: the key's bytes and a writable output
    let ok = unsafe { ee_tech_civ_find_upgrade_key(key.as_ptr(), key.len(), &mut back) };
    (ok, back)
}

fn civ_manager_bytes() -> Bytes {
    let mut r = Bytes::default();
    let count = ee_tech_civ_category_count();
    r.u32(count);
    for i in 0..=count {
        // one past: GetCategory's false
        // SAFETY: a category in place (or null)
        let entry = unsafe { ee_tech_civ_category(i).as_ref() };
        r.u32(entry.is_some() as u32);
        if let Some(e) = entry {
            r.0.extend_from_slice(bytes_of(e));
        }
        r.u32(entry.is_some() as u32).i32(entry.map_or(-7, |e| e.mNameID as i64));
        let levels = ee_tech_civ_level_count(i);
        r.u32(levels);
        for k in 0..=levels + 1 {
            let (mut v, mut b) = (-7.0f32, -7.0f32);
            // SAFETY: writable outputs
            let ok = unsafe { ee_tech_civ_category_level(i, k, &mut v, &mut b) };
            r.u32(ok as u32).f32(v).f32(b);
        }
        let mut id: Vec<u8> = Vec::new();
        // SAFETY: the sink takes a Vec<u8>
        unsafe { ee_tech_civ_category_id(i, &mut id as *mut Vec<u8> as *mut std::ffi::c_void, sink_bytes) };
        text(&mut r, &id);
    }
    let civs: Vec<EeTechCivEntry> = (0..ee_tech_civ_civilization_count())
        // SAFETY: a civilization in place
        .map(|i| unsafe { *ee_tech_civ_civilization(i) })
        .collect();
    r.u32(civs.len() as u32);
    for c in &civs {
        r.0.extend_from_slice(bytes_of(c));
    }
    r.u32(ee_tech_civ_era_count());
    for e in 0..ee_tech_civ_era_count() {
        let era = ee_tech_civ_era(e);
        r.i32(era as i64);
        let of_type: Vec<&EeTechCivEntry> = civs.iter().filter(|c| c.mEraNameID == era).collect();
        r.u32(of_type.len() as u32);
        for c in of_type {
            r.i32(c.mIndex as i64);
        }
    }
    // the upgrades: values, costs, keys both ways, legacy ids, counts
    let mut upgrades: Vec<EeTechCivUpgrade> = Vec::new();
    // SAFETY: the sink takes a Vec<EeTechCivUpgrade>
    unsafe { ee_tech_civ_upgrades(&mut upgrades as *mut Vec<EeTechCivUpgrade> as *mut std::ffi::c_void, sink_upgrade) };
    r.u32(upgrades.len() as u32);
    for &u in &upgrades {
        r.u32(u);
    }
    let mut some: Vec<EeTechCivUpgrade> = Vec::new();
    for &u in &upgrades {
        let (mut v, mut b) = (-7.0f32, -7.0f32);
        // SAFETY: writable outputs
        let ok = unsafe { ee_tech_civ_category_level(u >> 16, u & 0xffff, &mut v, &mut b) };
        r.u32(ok as u32).f32(v).f32(b);
        r.i32(ee_tech_civ_extra_cost(u) as i64);
        // SAFETY: the list's upgrades
        r.i32(unsafe { ee_tech_civ_count_in_category(u, some.as_ptr(), some.len()) } as i64);
        some.push(u);
        let mut key: Vec<u8> = Vec::new();
        // SAFETY: the sink takes a Vec<u8>
        let keyed =
            unsafe { ee_tech_civ_upgrade_key(u, &mut key as *mut Vec<u8> as *mut std::ffi::c_void, sink_bytes) };
        r.u32(keyed as u32);
        text(&mut r, &key);
        let (ok, back) = find_key(&key);
        r.u32(ok as u32).u32(back);
        let mut legacy = 0u32;
        // SAFETY: a writable output
        let fits = unsafe { ee_tech_civ_to_legacy(u, &mut legacy) };
        r.u32(fits as u32).u32(legacy).u32(ee_tech_civ_from_legacy(legacy));
    }
    // keys that are not upgrades
    for key in [&b""[..], b".", b"nokey", b"special_powers.", b".attack", b"x.attack", b"special_powers.x"] {
        let (ok, back) = find_key(key);
        r.u32(ok as u32).u32(back);
    }
    // the levels: names, powers
    for level in 0..=0x50u32 {
        r.i32(ee_tech_civ_level_name_id(level) as i64);
        r.u32(ee_tech_civ_is_power_level(level) as u32);
        r.u32(ee_tech_civ_power_level(level));
        r.u32(ee_tech_civ_power_upgrade(level));
        r.u32(((level & 7) << 16).wrapping_add(level & 0xffff));
    }
    // the powers
    r.u32(ee_tech_civ_power_count());
    for id in 0x20..=0x42 {
        let mut power = EeTechCivPower::default();
        // SAFETY: a writable output
        let ok = unsafe { ee_tech_civ_power(id, &mut power) };
        r.u32(ok as u32);
        r.0.extend_from_slice(bytes_of(&power));
    }
    r
}

/// CVMG's head (the manager's and the powers' data and queries) in a buffer the caller frees with
/// [`ee_tech_capture_free`]; the caller appends the premade civilizations' files and commits it.
#[no_mangle]
pub extern "C" fn ee_tech_capture_civ_manager(out_size: &mut u32) -> *mut u8 {
    let r = civ_manager_bytes();
    *out_size = r.0.len() as u32;
    Box::into_raw(r.0.into_boxed_slice()) as *mut u8
}

#[cfg(test)]
mod tests {
    use super::*;

    fn words(b: &[u8]) -> Vec<i32> {
        b.chunks(4).map(|c| i32::from_le_bytes(c.try_into().unwrap())).collect()
    }

    #[test]
    fn a_node_s_state() {
        let mut n = EeTechNodeFields {
            mIsResearched: true,
            mIsAllowed: true,
            mPrerequisiteFlags: 6,
            mEpoch: 3,
            mObsoleteEpoch: 14,
            mResearchDelay: 2,
            ..Default::default()
        };
        let mut d = EeTechNodeDataFields {
            mID: 1004,
            mKind: 3,
            mMakesUnit: true,
            mResearchedWithEpoch: true,
            mPrerequisite1: 1001,
            mPrerequisite2: -1,
            mPrerequisite3: -1,
            mPrerequisite4: -1,
            ..Default::default()
        };
        let t = EeTechTreeFields {
            mCurrentEpoch: 4,
            mStartEpoch: 1,
            mEndEpoch: 14,
            mEpochsBeforeWonders: 1,
            ..Default::default()
        };
        let mut out = [0u8; EE_TECH_CAPTURE_NODE_SIZE as usize];
        // SAFETY: live records and a buffer of the size
        let n_bytes = unsafe { ee_tech_capture_node(&n, &d, &t, 15, out.as_mut_ptr()) };
        assert_eq!(n_bytes, EE_TECH_CAPTURE_NODE_SIZE);
        assert_eq!(words(&out), [1004, 3, 14, 0x11, 2, 6, 4, 1, 14, 1, 15, 3, 9, 1001, -1, -1, -1]);
        // an epoch's prerequisite flags read 0; no record and no tree: -1s
        d.mID = 2;
        let r = node_bytes(&n, Some(&d), None, 15);
        assert_eq!(words(&r.0)[5..10], [0, -1, -1, -1, -1]);
        let r = node_bytes(&n, None, None, 15);
        assert_eq!(words(&r.0)[0], -1);
        assert_eq!(words(&r.0)[11..], [-1, 0, -1, -1, -1, -1]);
        let mut e = [0u8; EE_TECH_CAPTURE_EPOCH_SIZE as usize];
        n.mRequiredSOL = 9;
        n.mCurrentSOL = 4;
        n.mIsCountingSOL = true;
        // SAFETY: a live record and a buffer of the size
        unsafe { ee_tech_capture_epoch(&n, e.as_mut_ptr()) };
        assert_eq!(words(&e), [9, 4, 1]);
        let mut tr = [0u8; EE_TECH_CAPTURE_TREE_SIZE as usize];
        // SAFETY: as above
        unsafe { ee_tech_capture_tree(&t, 2, tr.as_mut_ptr()) };
        assert_eq!(words(&tr), [2, 4, 1, 14, 1]);
    }

    #[test]
    fn the_collections() {
        // no capture in a test: the steps are not collected
        ee_tech_capture_epoch_step(1, 3);
        // SAFETY: null asks for the count
        assert_eq!(unsafe { ee_tech_capture_research_to_epoch(5, 15, true, std::ptr::null_mut()) }, 16);
        EPOCH_STEPS.lock().unwrap().extend([1, 3, 4, 3]);
        let mut out = [0u8; 32];
        // SAFETY: a buffer of the count
        assert_eq!(unsafe { ee_tech_capture_research_to_epoch(5, 15, true, out.as_mut_ptr()) }, 32);
        assert_eq!(words(&out), [5, 15, 1, 2, 1, 3, 4, 3]);
        assert!(EPOCH_STEPS.lock().unwrap().is_empty());
        WALKS.with(|w| w.borrow_mut().push(vec![1004, 2]));
        let mut size = 0;
        let p = ee_tech_capture_walk(3, 1, 7, 8, 9, &mut size);
        // SAFETY: the buffer and its size
        let b = unsafe { std::slice::from_raw_parts(p, size as usize) }.to_vec();
        unsafe { ee_tech_capture_free(p, size) };
        assert_eq!(words(&b), [3, 1, 7, 8, 9, 1, 1004, 2]);
        assert_eq!(ee_tech_capture_walk_tag(6), tag(b"TNMT"));
        assert_eq!(ee_tech_capture_walk_tag(3), tag(b"TNWK"));
    }

    #[test]
    fn the_civ_manager_head() {
        // whatever tables the tests loaded: the head starts with the category count, and each category's known
        // flag; it always ends with the 0x23 powers' records (a flag and 152 bytes each); under civ_tests.rs's lock (they
        // load and clear the tables)
        let _g = crate::tech::civ_tests::locked();
        let r = civ_manager_bytes();
        assert_eq!(words(&r.0[..4])[0] as u32, ee_tech_civ_category_count());
        assert!(r.0.len() >= 4 + 0x51 * 20 + 4 + 0x23 * (4 + std::mem::size_of::<EeTechCivPower>()));
        assert_eq!(std::mem::size_of::<EeTechCivPower>(), 152);
        assert_eq!(std::mem::size_of::<EeTechCivEntry>(), 120);
        let mut b = Bytes::default();
        text(&mut b, b"ab\0cd");
        assert_eq!(b.0, [2, 0, 0, 0, b'a', b'b']);
        assert_eq!(find_key(b"nokey"), (false, 0xdeadbeef));
        let mut size = 0;
        let p = ee_tech_capture_civ_manager(&mut size);
        assert_eq!(size as usize, r.0.len());
        // SAFETY: the buffer and its size
        unsafe { ee_tech_capture_free(p, size) };
    }
}
