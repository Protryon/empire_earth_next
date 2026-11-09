//! The C ABI of the tile searches' storage and machine. The C++ storage classes (`EEAIAStarStorage` and the
//! classes over it) own one `EeAStarStorage` each and forward to it; the C++ machine's `Run` calls `ee_astar_start`
//! and `ee_astar_expand` with its goal behind `EeAStarCallbacks`.
//!
//! While `ee_astar_start` / `ee_astar_expand` run, the callbacks must not call any `ee_astar_*` function on the same
//! storage: what the goals read of the storage during a search comes in the callbacks' arguments.

use crate::area_goals::{
    self, AreaView, EeAStarBlocker, EeAStarContinentInfo, EeAStarPathAreaFill, EeAStarPathAreaInfo,
    EeAStarPathSectorFill,
};
use crate::goals::{self, EeAStarChokeFill, EeAStarContinentFill, EeAStarHillFill, View};
use crate::machine::{self, Host, MachineState};
use crate::modifiers::{self, EeAStarStepGoal, EeAStarStorm, ModView, Skip, Unit};
use crate::path_goals::{self, EeAStarPathGoal, PathView};
use crate::storage::{AStarRect, GridKind, List, Storage, StorageKind, NIL};
use std::ffi::c_void;

/// One storage (opaque to C++).
pub struct EeAStarStorage(Storage);

/// A node as the goals and the capture see it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarNodeView {
    pub x: i32,
    pub y: i32,
    pub cost: u32,
    pub path_cost: i32,
    pub estimate: i32,
    pub has_parent: bool,
    pub parent_x: i32,
    pub parent_y: i32,
}

/// The goal and the C++ machine's members (`machine::Host`), as C function pointers over `ctx`.
#[repr(C)]
pub struct EeAStarCallbacks {
    pub ctx: *mut c_void,
    pub accept_tile: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, flag: bool) -> bool,
    pub get_cost: extern "C" fn(
        ctx: *mut c_void,
        from_x: i32,
        from_y: i32,
        x: i32,
        y: i32,
        has_parent: bool,
        parent_x: i32,
        parent_y: i32,
    ) -> i32,
    pub estimate: extern "C" fn(ctx: *mut c_void, from_x: i32, from_y: i32, to_x: i32, to_y: i32) -> i32,
    pub is_goal_node: extern "C" fn(ctx: *mut c_void, has_node: bool, x: i32, y: i32) -> bool,
    pub search_done: extern "C" fn(ctx: *mut c_void, has_current: bool, x: i32, y: i32, has_open: bool),
    pub search_failed: extern "C" fn(ctx: *mut c_void) -> bool,
    pub accept_nearest: extern "C" fn(ctx: *mut c_void) -> bool,
    pub is_step_limit: extern "C" fn(ctx: *mut c_void, steps: i32) -> bool,
    pub is_rev_limit: extern "C" fn(ctx: *mut c_void, revs: i32) -> bool,
    pub is_cheaper: extern "C" fn(ctx: *mut c_void, cost: u32, old: u32) -> bool,
    /// the goal's start (`out[0..2]`) and goal (`out[2..4]`) tiles
    pub tiles: extern "C" fn(ctx: *mut c_void, out: *mut i32),
    pub get_state: extern "C" fn(ctx: *mut c_void, out: *mut MachineState),
    pub set_state: extern "C" fn(ctx: *mut c_void, state: *const MachineState),
}

struct CHost<'a>(&'a EeAStarCallbacks);

impl Host for CHost<'_> {
    fn accept_tile(&mut self, x: i32, y: i32, flag: bool) -> bool {
        (self.0.accept_tile)(self.0.ctx, x, y, flag)
    }
    fn get_cost(&mut self, from_x: i32, from_y: i32, x: i32, y: i32, parent: Option<(i32, i32)>) -> i32 {
        let (px, py) = parent.unwrap_or((0, 0));
        (self.0.get_cost)(self.0.ctx, from_x, from_y, x, y, parent.is_some(), px, py)
    }
    fn estimate(&mut self, from_x: i32, from_y: i32, to_x: i32, to_y: i32) -> i32 {
        (self.0.estimate)(self.0.ctx, from_x, from_y, to_x, to_y)
    }
    fn is_goal_node(&mut self, node: Option<(i32, i32)>) -> bool {
        let (x, y) = node.unwrap_or((0, 0));
        (self.0.is_goal_node)(self.0.ctx, node.is_some(), x, y)
    }
    fn search_done(&mut self, current: Option<(i32, i32)>, has_open: bool) {
        let (x, y) = current.unwrap_or((0, 0));
        (self.0.search_done)(self.0.ctx, current.is_some(), x, y, has_open)
    }
    fn search_failed(&mut self) -> bool {
        (self.0.search_failed)(self.0.ctx)
    }
    fn accept_nearest(&mut self) -> bool {
        (self.0.accept_nearest)(self.0.ctx)
    }
    fn is_step_limit(&mut self, steps: i32) -> bool {
        (self.0.is_step_limit)(self.0.ctx, steps)
    }
    fn is_rev_limit(&mut self, revs: i32) -> bool {
        (self.0.is_rev_limit)(self.0.ctx, revs)
    }
    fn is_cheaper(&mut self, cost: u32, old: u32) -> bool {
        (self.0.is_cheaper)(self.0.ctx, cost, old)
    }
    fn start_tile(&mut self) -> (i32, i32) {
        let mut t = [0i32; 4];
        (self.0.tiles)(self.0.ctx, t.as_mut_ptr());
        (t[0], t[1])
    }
    fn goal_tile(&mut self) -> (i32, i32) {
        let mut t = [0i32; 4];
        (self.0.tiles)(self.0.ctx, t.as_mut_ptr());
        (t[2], t[3])
    }
    fn state(&mut self) -> MachineState {
        let mut s = MachineState::default();
        (self.0.get_state)(self.0.ctx, &mut s);
        s
    }
    fn set_state(&mut self, state: &MachineState) {
        (self.0.set_state)(self.0.ctx, state)
    }
}

/// # Safety
/// `s` must come from `ee_astar_new` and not be in use elsewhere (no search running on it).
unsafe fn st<'a>(s: *mut EeAStarStorage) -> &'a mut Storage {
    // SAFETY: the caller's contract
    unsafe { &mut (*s).0 }
}

/// A new storage of the given kind (`StorageKind`: 0 hashed, 1 bounded, 2 terrain).
#[no_mangle]
pub extern "C" fn ee_astar_new(kind: StorageKind) -> *mut EeAStarStorage {
    Box::into_raw(Box::new(EeAStarStorage(Storage::new(kind))))
}

/// Frees a storage.
///
/// # Safety
/// `s` must come from `ee_astar_new` (or be null) and not be used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_free(s: *mut EeAStarStorage) {
    if !s.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(s) });
    }
}

/// `ResetNodes`.
///
/// # Safety
/// As `st`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_reset_nodes(s: *mut EeAStarStorage) {
    unsafe { st(s) }.reset_nodes();
}

/// `Clear`.
///
/// # Safety
/// As `st`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_clear(s: *mut EeAStarStorage) {
    unsafe { st(s) }.clear();
}

/// `Init`'s first half: the size, then `Clear`.
///
/// # Safety
/// As `st`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_init_begin(s: *mut EeAStarStorage, width: i32, height: i32) {
    unsafe { st(s) }.init_begin(width, height);
}

/// `Init`'s last half: empty hash tables.
///
/// # Safety
/// As `st`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_alloc_tables(s: *mut EeAStarStorage) {
    unsafe { st(s) }.alloc_tables();
}

/// A `PrepareGrid` slot (`GridKind`) over the storage's flags and node map; `resized`: the caller's own grid was
/// just allocated (the flood storages).
///
/// # Safety
/// As `st`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_prepare_grid(
    s: *mut EeAStarStorage,
    width: i32,
    height: i32,
    grid: GridKind,
    resized: bool,
) {
    unsafe { st(s) }.prepare_grid(width, height, grid, resized);
}

/// `MarkTile`.
///
/// # Safety
/// As `st`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_mark_tile(s: *mut EeAStarStorage, x: u32, y: u32, accept: bool) {
    unsafe { st(s) }.mark_tile(x, y, accept);
}

/// `GetFirstPathTile` (false: no path).
///
/// # Safety
/// As `st`; `x`, `y` valid for writes.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_first_path_tile(s: *mut EeAStarStorage, x: *mut i32, y: *mut i32) -> bool {
    match unsafe { st(s) }.first_path_tile() {
        Some((tx, ty)) => {
            unsafe {
                *x = tx;
                *y = ty;
            }
            true
        }
        None => false,
    }
}

/// `GetNextPathTile` (false and (0, 0) at the end).
///
/// # Safety
/// As `st`; `x`, `y` valid for writes.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_next_path_tile(s: *mut EeAStarStorage, x: *mut i32, y: *mut i32) -> bool {
    let r = unsafe { st(s) }.next_path_tile();
    let (tx, ty) = r.unwrap_or((0, 0));
    unsafe {
        *x = tx;
        *y = ty;
    }
    r.is_some()
}

fn view(s: &Storage, node: i32) -> EeAStarNodeView {
    let n = s.node(node);
    let (has_parent, parent_x, parent_y) =
        if n.parent != NIL { (true, s.node(n.parent).x, s.node(n.parent).y) } else { (false, 0, 0) };
    EeAStarNodeView {
        x: n.x,
        y: n.y,
        cost: n.cost,
        path_cost: n.path_cost,
        estimate: n.estimate,
        has_parent,
        parent_x,
        parent_y,
    }
}

/// The nodes of a list (`List`) in its order: writes up to `capacity` into `out`, returns the list's length.
///
/// # Safety
/// As `st`; `out` valid for `capacity` writes (or null with `capacity` 0).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_list(
    s: *mut EeAStarStorage,
    which: List,
    out: *mut EeAStarNodeView,
    capacity: u32,
) -> u32 {
    let s = unsafe { st(s) };
    let nodes = s.list(which);
    for (i, &node) in nodes.iter().take(capacity as usize).enumerate() {
        unsafe { *out.add(i) = view(s, node) };
    }
    nodes.len() as u32
}

/// The current node, the found path's node (`which` 0, 1); false when none.
///
/// # Safety
/// As `st`; `out` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_node(s: *mut EeAStarStorage, which: u32, out: *mut EeAStarNodeView) -> bool {
    let s = unsafe { st(s) };
    let node = if which == 0 { s.current } else { s.path };
    if node == NIL {
        return false;
    }
    unsafe { *out = view(s, node) };
    true
}

/// Whether an open node is left.
///
/// # Safety
/// As `st`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_has_open(s: *mut EeAStarStorage) -> bool {
    unsafe { st(s) }.has_open()
}

/// The storage's scalar members, read and written whole.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarFields {
    pub bounds: AStarRect,
    pub dirty: AStarRect,
    pub width: i32,
    pub height: i32,
    pub has_bounds: bool,
}

/// Reads the storage's scalar members.
///
/// # Safety
/// As `st`; `out` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_get_fields(s: *mut EeAStarStorage, out: *mut EeAStarFields) {
    let s = unsafe { st(s) };
    unsafe {
        *out = EeAStarFields {
            bounds: s.bounds,
            dirty: s.dirty,
            width: s.width,
            height: s.height,
            has_bounds: s.has_bounds,
        }
    };
}

/// Sets the bounds, the dirty bounds and the bounds flag (the size stays).
///
/// # Safety
/// As `st`; `fields` readable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_set_fields(s: *mut EeAStarStorage, fields: *const EeAStarFields) {
    let s = unsafe { st(s) };
    let f = unsafe { *fields };
    s.bounds = f.bounds;
    s.dirty = f.dirty;
    s.has_bounds = f.has_bounds;
}

/// The machine's `Start`.
///
/// # Safety
/// As `st`; `cb` valid, its functions callable and not re-entering this storage.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_start(s: *mut EeAStarStorage, cb: *const EeAStarCallbacks) -> bool {
    let s = unsafe { st(s) };
    machine::start(s, &mut CHost(unsafe { &*cb }))
}

/// The machine's `Expand`.
///
/// # Safety
/// As `ee_astar_start`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_expand(s: *mut EeAStarStorage, cb: *const EeAStarCallbacks) {
    let s = unsafe { st(s) };
    machine::expand(s, &mut CHost(unsafe { &*cb }))
}

/// A goal's view (`EEAIAStarView`), as C function pointers over `ctx`: the queries the flood goals make.
#[repr(C)]
pub struct EeAStarViewCallbacks {
    pub ctx: *mut c_void,
    pub has_tile: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> bool,
    pub terrain: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub is_edge: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> bool,
    pub height_level: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub continent_id: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> u16,
    /// a choke tile at `index` (`*serial` its area serial)
    pub choke_tile: extern "C" fn(ctx: *mut c_void, index: i32, serial: *mut u32) -> bool,
    // the modifiers' queries (`modifiers::ModView`)
    pub ai_value: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub tile_flag: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub can_move_on: extern "C" fn(ctx: *mut c_void, walker: Unit, terrain: i32) -> bool,
    pub are_passable: extern "C" fn(ctx: *mut c_void, walker: Unit, x: i32, y: i32, skip: Skip) -> bool,
    pub formation_rank: extern "C" fn(ctx: *mut c_void, walker: Unit) -> i32,
    pub step_blocker: extern "C" fn(ctx: *mut c_void, walker: Unit, sea: bool, x: i32, y: i32) -> bool,
    pub unit_region_state: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub epoch: extern "C" fn(ctx: *mut c_void, walker: Unit) -> i32,
    pub is_alone: extern "C" fn(ctx: *mut c_void, walker: Unit) -> bool,
    pub water_state: extern "C" fn(ctx: *mut c_void, walker: Unit) -> i32,
    pub is_sea_going: extern "C" fn(ctx: *mut c_void, walker: Unit) -> bool,
    pub has_hill: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> bool,
    pub random_at_most: extern "C" fn(ctx: *mut c_void, chance: f32) -> bool,
    // the path goals' queries (`path_goals::PathView`)
    pub crowd_cost: extern "C" fn(ctx: *mut c_void, walker: Unit, x: i32, y: i32) -> i32,
    pub is_wide: extern "C" fn(ctx: *mut c_void, walker: Unit) -> bool,
    pub ignores_formation: extern "C" fn(ctx: *mut c_void, walker: Unit) -> bool,
    pub rank_probe_blocked: extern "C" fn(ctx: *mut c_void, walker: Unit, continent_id: u16, x: i32, y: i32) -> bool,
    pub can_enter: extern "C" fn(ctx: *mut c_void, modifier: *mut c_void, x: i32, y: i32, at_goal: bool) -> bool,
    #[allow(clippy::type_complexity)]
    pub modifier_cost: extern "C" fn(
        ctx: *mut c_void,
        modifier: *mut c_void,
        from_x: i32,
        from_y: i32,
        x: i32,
        y: i32,
        parent_x: i32,
        parent_y: i32,
    ) -> i32,
    // the area fills' queries (`area_goals::AreaView`)
    pub has_units: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> bool,
    pub continent_at: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, out: *mut EeAStarContinentInfo),
    pub tile_value: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub path_area_at: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, out: *mut EeAStarPathAreaInfo),
    pub path_area: extern "C" fn(ctx: *mut c_void, id: u16, out: *mut EeAStarPathAreaInfo),
    pub stored_path_area_id: extern "C" fn(ctx: *mut c_void, storage: *const c_void, x: i32, y: i32) -> u16,
    pub resource_kind: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub has_blocker: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> bool,
    pub first_blocker_id: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub first_blocker: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, out: *mut EeAStarBlocker),
    pub first_obstacle_id: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub own_wall_first: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, player_id: i32) -> bool,
    pub area_gate_owner: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, player_id: i32) -> i32,
    pub sector_gate_owner: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, player_id: i32) -> i32,
    pub live_dock: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> bool,
    pub live_space_dock: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> bool,
}

struct CView<'a>(&'a EeAStarViewCallbacks);

impl View for CView<'_> {
    fn has_tile(&mut self, x: i32, y: i32) -> bool {
        (self.0.has_tile)(self.0.ctx, x, y)
    }
    fn terrain(&mut self, x: i32, y: i32) -> i32 {
        (self.0.terrain)(self.0.ctx, x, y)
    }
    fn is_edge(&mut self, x: i32, y: i32) -> bool {
        (self.0.is_edge)(self.0.ctx, x, y)
    }
    fn height_level(&mut self, x: i32, y: i32) -> i32 {
        (self.0.height_level)(self.0.ctx, x, y)
    }
    fn continent_id(&mut self, x: i32, y: i32) -> u16 {
        (self.0.continent_id)(self.0.ctx, x, y)
    }
    fn choke_tile(&mut self, index: i32) -> Option<u32> {
        let mut serial = 0u32;
        (self.0.choke_tile)(self.0.ctx, index, &mut serial).then_some(serial)
    }
}

impl ModView for CView<'_> {
    fn ai_value(&mut self, x: i32, y: i32) -> i32 {
        (self.0.ai_value)(self.0.ctx, x, y)
    }
    fn tile_flag(&mut self, x: i32, y: i32) -> i32 {
        (self.0.tile_flag)(self.0.ctx, x, y)
    }
    fn can_move_on(&mut self, walker: Unit, terrain: i32) -> bool {
        (self.0.can_move_on)(self.0.ctx, walker, terrain)
    }
    fn are_passable(&mut self, walker: Unit, x: i32, y: i32, skip: Skip) -> bool {
        (self.0.are_passable)(self.0.ctx, walker, x, y, skip)
    }
    fn formation_rank(&mut self, walker: Unit) -> i32 {
        (self.0.formation_rank)(self.0.ctx, walker)
    }
    fn step_blocker(&mut self, walker: Unit, sea: bool, x: i32, y: i32) -> bool {
        (self.0.step_blocker)(self.0.ctx, walker, sea, x, y)
    }
    fn unit_region_state(&mut self, x: i32, y: i32) -> i32 {
        (self.0.unit_region_state)(self.0.ctx, x, y)
    }
    fn epoch(&mut self, walker: Unit) -> i32 {
        (self.0.epoch)(self.0.ctx, walker)
    }
    fn is_alone(&mut self, walker: Unit) -> bool {
        (self.0.is_alone)(self.0.ctx, walker)
    }
    fn water_state(&mut self, walker: Unit) -> i32 {
        (self.0.water_state)(self.0.ctx, walker)
    }
    fn is_sea_going(&mut self, walker: Unit) -> bool {
        (self.0.is_sea_going)(self.0.ctx, walker)
    }
    fn has_hill(&mut self, x: i32, y: i32) -> bool {
        (self.0.has_hill)(self.0.ctx, x, y)
    }
    fn random_at_most(&mut self, chance: f32) -> bool {
        (self.0.random_at_most)(self.0.ctx, chance)
    }
}

impl AreaView for CView<'_> {
    fn has_units(&mut self, x: i32, y: i32) -> bool {
        (self.0.has_units)(self.0.ctx, x, y)
    }
    fn continent_at(&mut self, x: i32, y: i32) -> EeAStarContinentInfo {
        let mut out = EeAStarContinentInfo::default();
        (self.0.continent_at)(self.0.ctx, x, y, &mut out);
        out
    }
    fn tile_value(&mut self, x: i32, y: i32) -> i32 {
        (self.0.tile_value)(self.0.ctx, x, y)
    }
    fn path_area_at(&mut self, x: i32, y: i32) -> EeAStarPathAreaInfo {
        let mut out = EeAStarPathAreaInfo::default();
        (self.0.path_area_at)(self.0.ctx, x, y, &mut out);
        out
    }
    fn path_area(&mut self, id: u16) -> EeAStarPathAreaInfo {
        let mut out = EeAStarPathAreaInfo::default();
        (self.0.path_area)(self.0.ctx, id, &mut out);
        out
    }
    fn stored_path_area_id(&mut self, storage: *const c_void, x: i32, y: i32) -> u16 {
        (self.0.stored_path_area_id)(self.0.ctx, storage, x, y)
    }
    fn resource_kind(&mut self, x: i32, y: i32) -> i32 {
        (self.0.resource_kind)(self.0.ctx, x, y)
    }
    fn has_blocker(&mut self, x: i32, y: i32) -> bool {
        (self.0.has_blocker)(self.0.ctx, x, y)
    }
    fn first_blocker_id(&mut self, x: i32, y: i32) -> i32 {
        (self.0.first_blocker_id)(self.0.ctx, x, y)
    }
    fn first_blocker(&mut self, x: i32, y: i32) -> EeAStarBlocker {
        let mut out = EeAStarBlocker::default();
        (self.0.first_blocker)(self.0.ctx, x, y, &mut out);
        out
    }
    fn first_obstacle_id(&mut self, x: i32, y: i32) -> i32 {
        (self.0.first_obstacle_id)(self.0.ctx, x, y)
    }
    fn own_wall_first(&mut self, x: i32, y: i32, player_id: i32) -> bool {
        (self.0.own_wall_first)(self.0.ctx, x, y, player_id)
    }
    fn area_gate_owner(&mut self, x: i32, y: i32, player_id: i32) -> i32 {
        (self.0.area_gate_owner)(self.0.ctx, x, y, player_id)
    }
    fn sector_gate_owner(&mut self, x: i32, y: i32, player_id: i32) -> i32 {
        (self.0.sector_gate_owner)(self.0.ctx, x, y, player_id)
    }
    fn live_dock(&mut self, x: i32, y: i32) -> bool {
        (self.0.live_dock)(self.0.ctx, x, y)
    }
    fn live_space_dock(&mut self, x: i32, y: i32) -> bool {
        (self.0.live_space_dock)(self.0.ctx, x, y)
    }
}

impl PathView for CView<'_> {
    fn crowd_cost(&mut self, walker: Unit, x: i32, y: i32) -> i32 {
        (self.0.crowd_cost)(self.0.ctx, walker, x, y)
    }
    fn is_wide(&mut self, walker: Unit) -> bool {
        (self.0.is_wide)(self.0.ctx, walker)
    }
    fn ignores_formation(&mut self, walker: Unit) -> bool {
        (self.0.ignores_formation)(self.0.ctx, walker)
    }
    fn rank_probe_blocked(&mut self, walker: Unit, continent_id: u16, x: i32, y: i32) -> bool {
        (self.0.rank_probe_blocked)(self.0.ctx, walker, continent_id, x, y)
    }
    fn can_enter(&mut self, modifier: *mut c_void, x: i32, y: i32, at_goal: bool) -> bool {
        (self.0.can_enter)(self.0.ctx, modifier, x, y, at_goal)
    }
    fn modifier_cost(
        &mut self,
        modifier: *mut c_void,
        from_x: i32,
        from_y: i32,
        x: i32,
        y: i32,
        px: i32,
        py: i32,
    ) -> i32 {
        (self.0.modifier_cost)(self.0.ctx, modifier, from_x, from_y, x, y, px, py)
    }
}

/// `EEAIAStarContinentGoal::AcceptTile` over the goal's state.
///
/// # Safety
/// `fill` readable and writable; `view` valid, its functions callable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_accept(
    fill: *mut EeAStarContinentFill,
    view: *const EeAStarViewCallbacks,
    x: i32,
    y: i32,
) -> bool {
    let mut f = unsafe { *fill };
    let r = goals::continent_accept(&mut f, &mut CView(unsafe { &*view }), x, y);
    unsafe { *fill = f };
    r
}

/// `EEAIAStarHillGoal::AcceptTile` over the goal's state.
///
/// # Safety
/// As `ee_astar_continent_accept`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_hill_accept(
    fill: *mut EeAStarHillFill,
    view: *const EeAStarViewCallbacks,
    x: i32,
    y: i32,
) -> bool {
    let mut f = unsafe { *fill };
    let r = goals::hill_accept(&mut f, &mut CView(unsafe { &*view }), x, y);
    unsafe { *fill = f };
    r
}

/// `EEAIAStarChokePointGoal::AcceptTile` over the goal's state; `*serial` the tile's area serial when accepted.
///
/// # Safety
/// As `ee_astar_continent_accept`; `serial` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_choke_accept(
    fill: *mut EeAStarChokeFill,
    view: *const EeAStarViewCallbacks,
    x: i32,
    y: i32,
    serial: *mut u32,
) -> bool {
    let mut f = unsafe { *fill };
    let r = goals::choke_accept(&mut f, &mut CView(unsafe { &*view }), x, y);
    unsafe { *fill = f };
    match r {
        Some(s) => {
            unsafe { *serial = s };
            true
        }
        None => false,
    }
}

/// `EEAIPathGoalModifierBase::GetStepCost` (the sea and space modifiers' cost).
///
/// # Safety
/// `view` valid, its functions callable; `goal` readable.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_astar_mod_step_cost(
    view: *const EeAStarViewCallbacks,
    goal: *const EeAStarStepGoal,
    sea: bool,
    from_x: i32,
    from_y: i32,
    x: i32,
    y: i32,
    parent_x: i32,
    parent_y: i32,
) -> i32 {
    let goal = unsafe { *goal };
    modifiers::step_cost(&mut CView(unsafe { &*view }), &goal, sea, from_x, from_y, x, y, parent_x, parent_y)
}

/// `EEAIPathGoalModifierBase::IsPassable` (the sea and space modifiers' tile test).
///
/// # Safety
/// `view` valid, its functions callable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_mod_is_passable(
    view: *const EeAStarViewCallbacks,
    walker: Unit,
    x: i32,
    y: i32,
) -> bool {
    modifiers::is_passable(&mut CView(unsafe { &*view }), walker, x, y)
}

/// `EEAIBPGModifierEnemyIsAttackableByUnit::CanEnter`.
///
/// # Safety
/// As `ee_astar_mod_is_passable`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_mod_attacker_can_enter(
    view: *const EeAStarViewCallbacks,
    walker: Unit,
    x: i32,
    y: i32,
) -> bool {
    modifiers::attacker_can_enter(&mut CView(unsafe { &*view }), walker, x, y)
}

/// `EEAIBPGModifierWallsDoNotBlock::CanEnter`.
///
/// # Safety
/// As `ee_astar_mod_is_passable`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_mod_walls_can_enter(
    view: *const EeAStarViewCallbacks,
    walker: Unit,
    x: i32,
    y: i32,
) -> bool {
    modifiers::walls_can_enter(&mut CView(unsafe { &*view }), walker, x, y)
}

/// `EEAIGPGModifierComputerPlayerWallPather::CanEnter`.
///
/// # Safety
/// As `ee_astar_mod_is_passable`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_mod_wall_pather_can_enter(
    view: *const EeAStarViewCallbacks,
    walker: Unit,
    x: i32,
    y: i32,
) -> bool {
    modifiers::wall_pather_can_enter(&mut CView(unsafe { &*view }), walker, x, y)
}

/// `EEAIBPGModifierStormShaper::GetCost`.
///
/// # Safety
/// As `ee_astar_mod_is_passable`; `storm` readable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_mod_storm_cost(
    view: *const EeAStarViewCallbacks,
    storm: *const EeAStarStorm,
    x: i32,
    y: i32,
) -> i32 {
    modifiers::storm_cost(&mut CView(unsafe { &*view }), unsafe { &*storm }, x, y)
}

/// `EEAIBPGModifierStormShaper::CanEnter` (draws from the physics stream through the view).
///
/// # Safety
/// As `ee_astar_mod_storm_cost`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_mod_storm_can_enter(
    view: *const EeAStarViewCallbacks,
    storm: *const EeAStarStorm,
    x: i32,
    y: i32,
) -> bool {
    modifiers::storm_can_enter(&mut CView(unsafe { &*view }), unsafe { &*storm }, x, y)
}

/// The path goals' `AcceptTile`.
///
/// # Safety
/// `goal` readable; `view` valid, its functions callable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_path_accept(
    goal: *const EeAStarPathGoal,
    view: *const EeAStarViewCallbacks,
    x: i32,
    y: i32,
    at_goal: bool,
) -> bool {
    let goal = unsafe { *goal };
    path_goals::accept_tile(&goal, &mut CView(unsafe { &*view }), x, y, at_goal)
}

/// The path goals' `GetCost` (`has_parent`: the current node has a parent at (`parent_x`, `parent_y`)).
///
/// # Safety
/// As `ee_astar_path_accept`.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_astar_path_cost(
    goal: *const EeAStarPathGoal,
    view: *const EeAStarViewCallbacks,
    from_x: i32,
    from_y: i32,
    x: i32,
    y: i32,
    has_parent: bool,
    parent_x: i32,
    parent_y: i32,
) -> i32 {
    let goal = unsafe { *goal };
    let parent = has_parent.then_some((parent_x, parent_y));
    path_goals::get_cost(&goal, &mut CView(unsafe { &*view }), from_x, from_y, x, y, parent)
}

/// The path goals' `IsGoalNode` (without the bounded goal's `CheckNode`).
///
/// # Safety
/// As `ee_astar_path_accept`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_path_is_goal_node(
    goal: *const EeAStarPathGoal,
    view: *const EeAStarViewCallbacks,
    has_node: bool,
    x: i32,
    y: i32,
) -> bool {
    let goal = unsafe { *goal };
    path_goals::is_goal_node(&goal, &mut CView(unsafe { &*view }), has_node.then_some((x, y)))
}

/// The path goals' `SearchDone`: updates `goal`'s `search_failed` and `took_nearest`.
///
/// # Safety
/// `goal` readable and writable; `view` as `ee_astar_path_accept`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_path_search_done(
    goal: *mut EeAStarPathGoal,
    view: *const EeAStarViewCallbacks,
    has_current: bool,
    x: i32,
    y: i32,
    has_open: bool,
) {
    let mut g = unsafe { *goal };
    path_goals::search_done(&mut g, &mut CView(unsafe { &*view }), has_current.then_some((x, y)), has_open);
    unsafe { *goal = g };
}

/// `EEAIAStarPathAreaGoal::AcceptTile` over the goal's state.
///
/// # Safety
/// `fill` readable and writable; `view` valid, its functions callable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_path_area_accept(
    fill: *mut EeAStarPathAreaFill,
    view: *const EeAStarViewCallbacks,
    x: i32,
    y: i32,
) -> bool {
    let mut f = unsafe { *fill };
    let r = area_goals::path_area_accept(&mut f, &mut CView(unsafe { &*view }), x, y);
    unsafe { *fill = f };
    r
}

/// `EEAIAStarPathSectorGoal::AcceptTile` over the goal's state.
///
/// # Safety
/// As `ee_astar_path_area_accept`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_path_sector_accept(
    fill: *mut EeAStarPathSectorFill,
    view: *const EeAStarViewCallbacks,
    x: i32,
    y: i32,
) -> bool {
    let mut f = unsafe { *fill };
    let r = area_goals::path_sector_accept(&mut f, &mut CView(unsafe { &*view }), x, y);
    unsafe { *fill = f };
    r
}
