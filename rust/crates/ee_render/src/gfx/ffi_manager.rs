//! The effect manager as a Rust object (`EEEffectManager`, ee/EEEffectManager.h: the C++ class is its shell, an
//! `FSRestorable` for the saves): its containers (`effect_store.rs`), the goal markers (`goal_markers.rs`), the
//! creation entry points (`effect_manager.rs`' plan, the constructor, the registration), the destruction queue,
//! `ClearEffects`, `RepairPointers`, `Load`, `Save`, `Update`, the unit swaps and the terrain changes over the
//! effects as Rust objects (`effect_obj.rs`), and the TEST ONLY captures of every update (EFMU, EFST).
//!
//! The constructors are `effect_ctor.rs`'; the goal markers' questions and the damage controller's tick facts are
//! the C++'s. No store borrow is held across an effect's call: a
//! step, a set-up or a load creates and destroys effects in between.

// the C longs' widths differ by platform (a cast to the same type on Windows)
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;

use ee_base::math::types::{Ge3DLine, Ge3DPoint, GeTransformation};
use ee_sim::world::{UnitView, WorldRead};

use crate::gfx::effect_manager::{self, EeGfxEffectEntry, ManagerHost, StoreAccess};
use crate::gfx::effect_obj::{self as obj, EffectPtr};
use crate::gfx::effect_reads::GraphicEffect;
use crate::gfx::effect_state::{self, EeGfxDamagedTick, EffectFile};
use crate::gfx::effect_store::{self, EffectStore};
use crate::gfx::ffi_effect::{EeGfxEffectFile, File};
use crate::gfx::goal_markers::{EeGfxGoalHost, EeGfxGoalMarkers};

/// The capture's tags (ee/EESimCapture.h).
pub(crate) const fn tag(t: &[u8; 4]) -> u32 {
    t[0] as u32 | (t[1] as u32) << 8 | (t[2] as u32) << 16 | (t[3] as u32) << 24
}
pub(crate) const EFMU: u32 = tag(b"EFMU");
pub(crate) const EFST: u32 = tag(b"EFST");

/// The capture's calls (`EeGfxManagerWorld::capture`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGfxCaptureOp {
    /// `EESimCaptureRecord::Active()`
    Active = 0,
    /// `Keeps(tag)`
    Keeps = 1,
    /// a record of `tag` with the `len` bytes of `data`, committed
    Commit = 2,
    /// `Only(tags, len)` (`data`: the tags' words; 0: all)
    Only = 3,
    /// `Exclusive(tag != 0)`
    Exclusive = 4,
}

/// What the manager asks the C++ besides the effects' world (`EEEffectHost.cpp`).
#[repr(C)]
pub struct EeGfxManagerWorld {
    /// the damage controller's tick facts (the unit's damage points, health, cloak)
    pub damaged_tick: extern "C" fn(effect: EffectPtr) -> EeGfxDamagedTick,
    /// `gEffectDetail` (`set` >= 0: set it first)
    pub effect_detail: extern "C" fn(set: i64) -> u32,
    /// the game's manager (`gEffectManager`'s Rust object; null: none)
    pub current: extern "C" fn() -> *mut EeGfxEffectManager,
    /// the goal markers' questions (their effect calls are answered here)
    pub goals: EeGfxGoalHost,
    /// the capture (`EESimCaptureRecord`)
    pub capture: extern "C" fn(op: EeGfxCaptureOp, tag: u32, data: *const u8, len: u32) -> u32,
    /// an `FSMemoryFile` as the effects' file (TEST ONLY: EFST and the sweep), its bytes, its rewind and its end
    pub memory_file: extern "C" fn() -> EeGfxEffectFile,
    pub memory_file_data: extern "C" fn(context: *mut c_void, len: *mut u32) -> *const u8,
    pub memory_file_rewind: extern "C" fn(context: *mut c_void),
    pub memory_file_free: extern "C" fn(context: *mut c_void),
    /// the save's version (`kSaveGameMajorVersion`, `kSaveGameMinorVersion`)
    pub save_major: u32,
    pub save_minor: u32,
}

static WORLD: std::sync::atomic::AtomicPtr<EeGfxManagerWorld> = std::sync::atomic::AtomicPtr::new(std::ptr::null_mut());

/// Registers the manager's world (once; later registrations replace it: the tests').
///
/// # Safety
/// `world` lives for the rest of the program.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_manager_world_set(world: *const EeGfxManagerWorld) {
    WORLD.store(world as *mut EeGfxManagerWorld, std::sync::atomic::Ordering::Release);
}

pub(crate) fn mworld() -> &'static EeGfxManagerWorld {
    let w = WORLD.load(std::sync::atomic::Ordering::Acquire);
    assert!(!w.is_null(), "ee_gfx: the effect manager's world is registered (EEEffectHost.cpp)");
    // SAFETY: a registered world lives for the rest of the program
    unsafe { &*w }
}

/// An entry point's arguments (the manager's `Create*` methods): the graphic effect, the units, the points, the
/// transformation, the line, the tiles (x, y pairs) and the unit effect's flags.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGfxEffectArgs {
    pub entry: EeGfxEffectEntry,
    /// an `EEGraphicEffect*`
    pub graphic_effect: u64,
    /// the unit (`EEUnit*`) and, between units, the other one
    pub unit: u64,
    pub to_unit: u64,
    /// the point, and between points or from a unit, the end
    pub at: Ge3DPoint,
    pub to: Ge3DPoint,
    pub transformation: GeTransformation,
    pub line: Ge3DLine,
    pub tiles: *const i32,
    pub tile_count: u32,
    /// `CreateUnitEffect`'s: the light glow keeps its unit, the emitter's tag point, the confirmation's goal queue
    /// (and `CreateEffect`'s goal queue)
    pub keeps_unit: bool,
    pub tag_point: i32,
    pub goal_index: u32,
}

impl Default for EeGfxEffectArgs {
    fn default() -> Self {
        EeGfxEffectArgs {
            entry: EeGfxEffectEntry::Unit,
            graphic_effect: 0,
            unit: 0,
            to_unit: 0,
            at: Ge3DPoint::default(),
            to: Ge3DPoint::default(),
            transformation: GeTransformation::IDENTITY,
            line: Ge3DLine::default(),
            tiles: std::ptr::null(),
            tile_count: 0,
            keeps_unit: false,
            tag_point: 0,
            goal_index: 0,
        }
    }
}

/// The manager: its containers (the effects by handle, the list, the destruction queue, the last handle) and the
/// current player's goal markers.
pub struct EeGfxEffectManager {
    store: EffectStore,
    goals: *mut EeGfxGoalMarkers,
}

struct Store(*mut EeGfxEffectManager);

impl StoreAccess for Store {
    fn with<R>(&mut self, f: impl FnOnce(&mut EffectStore) -> R) -> R {
        // SAFETY: a live manager; this borrow ends before any effect's call
        f(unsafe { &mut (*self.0).store })
    }
}

fn ep(e: effect_store::EffectPtr) -> EffectPtr {
    e as EffectPtr
}

/// The effects as the manager calls them (the file and the save's versions for `Load` and `Save`).
struct Objects {
    m: *mut EeGfxEffectManager,
    file: *const EeGfxEffectFile,
    major: u32,
    minor: u32,
}

impl Objects {
    fn of(m: *mut EeGfxEffectManager) -> Objects {
        Objects { m, file: std::ptr::null(), major: 0, minor: 0 }
    }
}

fn record<'a>(e: effect_store::EffectPtr) -> &'a mut effect_state::EeGfxEffectRecord {
    // SAFETY: a live effect's record, used for one field
    unsafe { &mut *(*ep(e)).record }
}

fn detail() -> u64 {
    u64::from((mworld().effect_detail)(-1))
}

impl ManagerHost for Objects {
    fn kind(&mut self, e: effect_store::EffectPtr) -> i32 {
        record(e).kind
    }
    fn handle(&mut self, e: effect_store::EffectPtr) -> u32 {
        record(e).handle
    }
    fn set_handle(&mut self, e: effect_store::EffectPtr, handle: u32) {
        record(e).handle = handle
    }
    fn saved(&mut self, e: effect_store::EffectPtr) -> bool {
        record(e).saved
    }
    fn set_saved(&mut self, e: effect_store::EffectPtr, saved: bool) {
        record(e).saved = saved
    }
    fn reference(&mut self, e: effect_store::EffectPtr) {
        obj::ee_gfx_effect_reference(ep(e))
    }
    fn release(&mut self, e: effect_store::EffectPtr) {
        // SAFETY: a live effect; the store's reference
        unsafe { obj::ee_gfx_effect_release(ep(e)) }
    }
    fn set_up(&mut self, e: effect_store::EffectPtr) -> i32 {
        obj::ee_gfx_effect_set_up(ep(e))
    }
    fn step(&mut self, e: effect_store::EffectPtr) -> i32 {
        step(ep(e))
    }
    fn repair(&mut self, e: effect_store::EffectPtr) -> i32 {
        obj::ee_gfx_effect_repair(ep(e))
    }
    fn above_detail(&mut self, e: effect_store::EffectPtr) -> bool {
        // SAFETY: a live effect
        let g = GraphicEffect(unsafe { (*ep(e)).graphic_effect } as usize);
        // (unsigned long)mDetailLevel > gEffectDetail
        u64::from(g.fields().mDetailLevel as u32) > detail()
    }
    fn new_loaded(&mut self, kind: u32) -> effect_store::EffectPtr {
        new_loaded(kind) as effect_store::EffectPtr
    }
    fn load<F: EffectFile>(&mut self, e: effect_store::EffectPtr, _file: &mut F) -> i32 {
        // SAFETY: the manager's own file (the one `file` wraps)
        unsafe { obj::ee_gfx_effect_load(ep(e), self.file, self.major, self.minor) }
    }
    fn save<F: EffectFile>(&mut self, e: effect_store::EffectPtr, _file: &mut F) -> i32 {
        // SAFETY: as `load`
        unsafe { obj::ee_gfx_effect_save(ep(e), self.file, self.major, self.minor) }
    }
    fn goals_update(&mut self) {
        let host = goal_host();
        // SAFETY: the manager's live markers
        unsafe { crate::gfx::goal_markers::ee_gfx_goals_update((*self.m).goals, &host) }
    }
    fn goals_load<F: EffectFile>(&mut self, _file: &mut F) -> i32 {
        0
    }
    fn goals_repair(&mut self) -> i32 {
        0
    }
}

/// A load-time effect of a saved kind (the original's Load switch: `effect_kinds.rs`; null for a kind without one).
fn new_loaded(kind: u32) -> EffectPtr {
    if !crate::gfx::effect_kinds::ee_gfx_effect_loadable(kind as i32) {
        return std::ptr::null_mut();
    }
    let e = obj::ee_gfx_effect_new();
    obj::ee_gfx_effect_begin(e, kind as i32);
    e
}

/// A load-time effect of `kind` (null for a kind without one).
#[no_mangle]
pub extern "C" fn ee_gfx_effect_new_loaded(kind: u32) -> EffectPtr {
    new_loaded(kind)
}

// ---- the damage controller's step (its children through the game's manager)

extern "C" fn damaged_create(context: *mut c_void, secondary: bool, index: u32) -> u32 {
    let e = context as EffectPtr;
    // SAFETY: the live controller
    let (unit, g) = unsafe { ((*e).unit, GraphicEffect((*e).graphic_effect as usize)) };
    let args = EeGfxEffectArgs {
        entry: EeGfxEffectEntry::Unit,
        graphic_effect: g.linked(if secondary { 1 } else { 0 }),
        unit,
        tag_point: index as i32,
        ..EeGfxEffectArgs::default()
    };
    let mut handle = 0u32;
    create(current(), &args, &mut handle);
    handle
}

extern "C" fn damaged_destroy(_context: *mut c_void, handle: u32) {
    destroy(current(), handle);
}

/// `Step`: the damage controller's (one child per damage tag point, more as the health drops; 7 when it ends), the
/// others' by their class.
fn step(e: EffectPtr) -> i32 {
    // SAFETY: a live effect's record
    if unsafe { (*(*e).record).kind } != obj::kind::DAMAGED {
        return obj::ee_gfx_effect_call(e, crate::gfx::effect_rising::EeGfxEffect3DOp::Step, std::ptr::null_mut());
    }
    let tick = (mworld().damaged_tick)(e);
    let host = crate::gfx::ffi_effect::EeGfxDamagedHost {
        context: e as *mut c_void,
        create: damaged_create,
        destroy: damaged_destroy,
    };
    // SAFETY: the live effect's record; the host lives for the call
    unsafe { crate::gfx::ffi_effect::ee_gfx_effect_damaged_step((*e).record, &tick, &host) }
}

// ---- the goal markers' effects

pub(crate) fn current() -> *mut EeGfxEffectManager {
    (mworld().current)()
}

/// The graphic effects of the markers (ee/EEGraphicEffect.h): the basic marching line, the waypoint indicator and
/// the rally flags of a building, a fighter, a bomber, an atomic bomber and a paratrooper.
const GOAL_EFFECTS: [u32; 7] = [0x60, 0x62, 0xd2, 0xd4, 0xd5, 0xd6, 0xf5];

/// `EEGraphicalGoal`'s constructor: type 0 a marching line, 1 a waypoint icon, 2..6 the rally flags, at the origin,
/// shown only to `player` and not saved; its handle (0: none).
extern "C" fn goal_create(_: *mut c_void, kind: u32, player: u32) -> u32 {
    let mut handle = 0u32;
    if kind < 7 {
        let g = (obj::world().graphic_effect_by_id)(GOAL_EFFECTS[kind as usize]);
        let entry = if kind == 0 { EeGfxEffectEntry::Between } else { EeGfxEffectEntry::Placed };
        let args = EeGfxEffectArgs { entry, graphic_effect: g, ..EeGfxEffectArgs::default() };
        create(current(), &args, &mut handle);
    }
    let e = find(current(), handle);
    if !e.is_null() {
        // SAFETY: a live effect's record
        let r = unsafe { &mut *(*e).record };
        r.saved = false;
        r.local_only = true;
        r.player = player as i32;
    }
    handle
}

extern "C" fn goal_destroy(_: *mut c_void, handle: u32) {
    destroy(current(), handle);
}

/// A line's effect to its ends (`SetFrom`, `SetTo`), its transformation at `from` (`MoveToTransformation`).
extern "C" fn goal_move_line(_: *mut c_void, handle: u32, from: *const Ge3DPoint, to: *const Ge3DPoint) {
    let e = find(current(), handle);
    if e.is_null() {
        return;
    }
    // SAFETY: the markers' points
    let (from, to) = unsafe { (*from, *to) };
    obj::ee_gfx_effect_set_end(e, false, from);
    obj::ee_gfx_effect_set_end(e, true, to);
    let mut t = GeTransformation::IDENTITY;
    t.m[0][3] = from.x;
    t.m[1][3] = from.y;
    t.m[2][3] = from.z;
    obj::ee_gfx_effect_set_transformation(e, t);
}

extern "C" fn goal_move_icon(_: *mut c_void, handle: u32, to: *const Ge3DPoint) {
    let e = find(current(), handle);
    if !e.is_null() {
        // SAFETY: the marker's point
        obj::ee_gfx_effect_set_location(e, unsafe { *to });
    }
}

fn goal_host() -> EeGfxGoalHost {
    // SAFETY: a bitwise copy of the registered table (function pointers)
    let mut h = unsafe { std::ptr::read(&mworld().goals) };
    h.create = goal_create;
    h.destroy = goal_destroy;
    h.move_line = goal_move_line;
    h.move_icon = goal_move_icon;
    h
}

// ---- the C interface

/// A new manager: no effects, no queued destructions, no markers (`EEEffectManager`'s constructor).
#[no_mangle]
pub extern "C" fn ee_gfx_manager_new() -> *mut EeGfxEffectManager {
    Box::into_raw(Box::new(EeGfxEffectManager {
        store: EffectStore::new(),
        goals: crate::gfx::goal_markers::ee_gfx_goals_new(),
    }))
}

/// The destructor: every effect released (`ClearEffects`), then the markers (each marker's effect destroyed
/// through the game's manager), then the containers.
///
/// # Safety
/// `m` is null or a live manager, not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_manager_free(m: *mut EeGfxEffectManager) {
    if m.is_null() {
        return;
    }
    ee_gfx_manager_clear(m);
    let host = goal_host();
    // SAFETY: the function's contract
    unsafe {
        crate::gfx::goal_markers::ee_gfx_goals_free((*m).goals, &host);
        drop(Box::from_raw(m));
    }
}

/// The detail test and the living units an entry point's kind needs, then the effect made, set up and registered
/// under a new handle (`out_handle`, may be null); the plan's result or the set-up's.
pub(crate) fn create(m: *mut EeGfxEffectManager, args: &EeGfxEffectArgs, out_handle: *mut u32) -> i32 {
    let g = GraphicEffect(args.graphic_effect as usize).fields();
    let detail_ok = u64::from(g.mDetailLevel as u32) <= detail();
    let alive = |unit: u64| {
        // SAFETY: an entry point's unit is live
        unit != 0
            && unsafe {
                let t = WorldRead::begin();
                UnitView::of(&t, unit).expect("an entry's unit").complex_unit_handle() != 0
            }
    };
    let plan = effect_manager::plan(args.entry, g.mKind as i32, detail_ok, alive(args.unit), alive(args.to_unit));
    if !plan.create {
        return plan.result;
    }
    let e = crate::gfx::effect_ctor::construct(args);
    let (result, handle) =
        effect_manager::create(&mut Store(m), &mut Objects::of(m), e as effect_store::EffectPtr, plan.saved);
    if let Some(handle) = handle {
        if !out_handle.is_null() {
            // SAFETY: the caller's handle
            unsafe { *out_handle = handle };
        }
    }
    result
}

/// An entry point (`CreateUnitEffect` .. `CreateWeatherEffect`): its result; the new effect's handle into
/// `out_handle` (may be null).
///
/// # Safety
/// `m` is a live manager; `args` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_manager_create(
    m: *mut EeGfxEffectManager,
    args: *const EeGfxEffectArgs,
    out_handle: *mut u32,
) -> i32 {
    // SAFETY: the function's contract
    create(m, unsafe { &*args }, out_handle)
}

fn store_of<'a>(m: *const EeGfxEffectManager) -> &'a EffectStore {
    // SAFETY: the C++ passes a live manager
    unsafe { &(*m).store }
}

pub(crate) fn find(m: *mut EeGfxEffectManager, handle: u32) -> EffectPtr {
    if m.is_null() || handle == 0 {
        return std::ptr::null_mut();
    }
    // SAFETY: a live manager
    ep(unsafe { (*m).store.find(handle) })
}

pub(crate) fn destroy(m: *mut EeGfxEffectManager, handle: u32) {
    if !m.is_null() {
        // SAFETY: a live manager
        unsafe { (*m).store.queue_destroy(handle) }
    }
}

/// `GetEffect`: the effect under the handle (null: none).
#[no_mangle]
pub extern "C" fn ee_gfx_manager_find(m: *mut EeGfxEffectManager, handle: u32) -> EffectPtr {
    find(m, handle)
}

/// `DestroyEffect`: the handle queued for the next update.
#[no_mangle]
pub extern "C" fn ee_gfx_manager_destroy(m: *mut EeGfxEffectManager, handle: u32) {
    destroy(m, handle)
}

/// The effect list's first effect (newest first; null: none).
#[no_mangle]
pub extern "C" fn ee_gfx_manager_first(m: *const EeGfxEffectManager) -> EffectPtr {
    ep(store_of(m).head())
}

/// The effect after `e` in the list (null: none).
#[no_mangle]
pub extern "C" fn ee_gfx_manager_next(m: *const EeGfxEffectManager, e: EffectPtr) -> EffectPtr {
    ep(store_of(m).next(e as effect_store::EffectPtr))
}

/// The index's size, or the destruction queue's.
#[no_mangle]
pub extern "C" fn ee_gfx_manager_count(m: *const EeGfxEffectManager, destroyed: bool) -> u32 {
    let s = store_of(m);
    (if destroyed { s.destroyed_len() } else { s.count() }) as u32
}

/// `ClearEffects`: every effect, in handle order, released, removed and unindexed.
#[no_mangle]
pub extern "C" fn ee_gfx_manager_clear(m: *mut EeGfxEffectManager) {
    effect_manager::clear(&mut Store(m), &mut Objects::of(m))
}

/// `RepairPointers`: each effect's, dropping those whose unit is gone or that are above the detail setting; the
/// first other error.
#[no_mangle]
pub extern "C" fn ee_gfx_manager_repair(m: *mut EeGfxEffectManager) -> i32 {
    effect_manager::repair(&mut Store(m), &mut Objects::of(m))
}

/// `Load`: the handle counter, each effect by kind, the markers, the destruction queue; the first read error.
///
/// # Safety
/// `m` is a live manager; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_manager_load(
    m: *mut EeGfxEffectManager,
    file: *const EeGfxEffectFile,
    major: u32,
    minor: u32,
) -> i32 {
    // SAFETY: the function's contract
    let mut f = File(unsafe { &*file });
    effect_manager::load(&mut Store(m), &mut Objects { m, file, major, minor }, &mut f, major, minor)
}

/// `Save`: the handle counter, the saved effects (kind, then data), the destruction queue.
///
/// # Safety
/// `m` is a live manager; `file` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_manager_save(
    m: *mut EeGfxEffectManager,
    file: *const EeGfxEffectFile,
    major: u32,
    minor: u32,
) -> i32 {
    // SAFETY: the function's contract
    let mut f = File(unsafe { &*file });
    effect_manager::save(&mut Store(m), &mut Objects { m, file, major, minor }, &mut f)
}

/// `Update`: the goal markers, the queued destructions, every effect's step (removing those that finish), then the
/// TEST ONLY captures (EFMU, EFST).
#[no_mangle]
pub extern "C" fn ee_gfx_manager_update(m: *mut EeGfxEffectManager) {
    effect_manager::update(&mut Store(m), &mut Objects::of(m));
    capture_manager(m);
    capture_states(m);
}

/// `ReplaceUnit`: every effect on `old` moves to `new`.
#[no_mangle]
pub extern "C" fn ee_gfx_manager_replace_unit(m: *mut EeGfxEffectManager, old: u64, new: u64) {
    let mut e = ee_gfx_manager_first(m);
    while !e.is_null() {
        obj::ee_gfx_effect_replace_unit(e, old, new);
        e = ee_gfx_manager_next(m, e);
    }
}

/// `ElevationChanged`: every effect told that the terrain changed in the rectangle (the next one taken first).
#[no_mangle]
pub extern "C" fn ee_gfx_manager_elevation_changed(
    m: *mut EeGfxEffectManager,
    left: i64,
    top: i64,
    right: i64,
    bottom: i64,
) {
    let mut e = ee_gfx_manager_first(m);
    while !e.is_null() {
        let next = ee_gfx_manager_next(m, e);
        obj::ee_gfx_effect_elevation_changed(e, left, top, right, bottom);
        e = next;
    }
}

// ---- TEST ONLY: the captures (tools/sim_capture.py EFMU, EFST)

pub(crate) fn active() -> bool {
    (mworld().capture)(EeGfxCaptureOp::Active, 0, std::ptr::null(), 0) != 0
}

/// A record of `tag`: the words, then the bytes.
pub(crate) fn commit(tag: u32, words: &[u32], bytes: &[u8]) {
    let mut data: Vec<u8> = words.iter().flat_map(|w| w.to_le_bytes()).collect();
    data.extend_from_slice(bytes);
    (mworld().capture)(EeGfxCaptureOp::Commit, tag, data.as_ptr(), data.len() as u32);
}

/// FNV-1a 64 of `data` from `hash`.
pub(crate) fn fnv(mut hash: u64, data: &[u8]) -> u64 {
    for &b in data {
        hash ^= u64::from(b);
        hash = hash.wrapping_mul(0x100000001b3);
    }
    hash
}

pub(crate) const FNV_START: u64 = 0xcbf29ce484222325;

/// EFMU: the manager's state after every Update, folded (`ffi_mesh.rs`' capture words).
fn capture_manager(m: *mut EeGfxEffectManager) {
    if !active() {
        return;
    }
    // SAFETY: a live manager
    let s = unsafe { &(*m).store };
    let words = crate::gfx::ffi_mesh::capture_words(s, &|e| {
        let r = record(e);
        (r.kind as u32, r.handle)
    });
    commit(EFMU, &words, &[]);
}

/// A memory file's bytes (TEST ONLY).
pub(crate) fn memory_bytes(f: &EeGfxEffectFile) -> Vec<u8> {
    let mut len = 0u32;
    let data = (mworld().memory_file_data)(f.context, &mut len);
    if data.is_null() || len == 0 {
        return Vec::new();
    }
    // SAFETY: the memory file's `len` bytes
    unsafe { std::slice::from_raw_parts(data, len as usize) }.to_vec()
}

/// EFST: every effect's state after every Update: the effects' random seed, then per effect of the index in handle
/// order its handle, kind, player, local-only and saved flags and a saved effect's own Save bytes, folded into the
/// count, the length and an FNV-1a 64.
fn capture_states(m: *mut EeGfxEffectManager) {
    let w = mworld();
    if !active() || (w.capture)(EeGfxCaptureOp::Keeps, EFST, std::ptr::null(), 0) == 0 {
        return;
    }
    let file = (w.memory_file)();
    let mut f = File(&file);
    let mut count = 0u32;
    let mut cursor = 0u64;
    while let Some((handle, e)) = store_of(m).at_or_after(cursor) {
        cursor = u64::from(handle) + 1;
        let r = record(e);
        let (kind, player, local_only, saved) = (r.kind, r.player, r.local_only, r.saved);
        f.write_u32(handle);
        f.write_i32(kind);
        f.write_i32(player);
        f.write_bool(local_only);
        f.write_bool(saved);
        if saved {
            // SAFETY: a live effect; the memory file
            unsafe { obj::ee_gfx_effect_save(ep(e), &file, w.save_major, w.save_minor) };
        }
        count += 1;
    }
    let bytes = memory_bytes(&file);
    (w.memory_file_free)(file.context);
    let hash = fnv(FNV_START, &bytes);
    let words = [
        crate::gfx::ffi_effect::ee_gfx_effect_random_seed(),
        count,
        bytes.len() as u32,
        hash as u32,
        (hash >> 32) as u32,
    ];
    commit(EFST, &words, &[]);
}

/// The game's manager's entry point (the effects' own creations: the linked effects); its result.
pub(crate) fn create_current(args: &EeGfxEffectArgs) -> i32 {
    create(current(), args, std::ptr::null_mut())
}

/// An entry point on `m` (the sweep's).
pub(crate) fn create_with(m: *mut EeGfxEffectManager, args: &EeGfxEffectArgs, out_handle: &mut u32) -> i32 {
    create(m, args, out_handle)
}

#[cfg(test)]
#[path = "ffi_manager_tests.rs"]
mod tests;
