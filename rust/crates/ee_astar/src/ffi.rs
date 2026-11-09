//! The C ABI of the tile searches' storage and machine. The C++ storage classes (`EEAIAStarStorage` and the
//! classes over it) own one `EeAStarStorage` each and forward to it; the C++ machine's `Run` calls `ee_astar_start`
//! and `ee_astar_expand` with its goal behind `EeAStarCallbacks`.
//!
//! While `ee_astar_start` / `ee_astar_expand` run, the callbacks must not call any `ee_astar_*` function on the same
//! storage: what the goals read of the storage during a search comes in the callbacks' arguments.

use crate::machine::{self, Host, MachineState};
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
