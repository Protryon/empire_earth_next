//! The engine's sparse point set (`U2DSparseArrayPointContainer`, `include/lle/U2DSparseArray.h`): `set` is the set,
//! this module its C ABI. The C++ class owns a handle (`EeSparseArray`) and its members are inline forwarders over
//! this module's header, rust/base/sparse.h, which names nothing else (the AI's areas, path sub sectors and the
//! explore goal hold their tiles in one; the AI's A* searches, ee_sim::astar, re-export it).

pub mod set;

use set::{PointList, SparseArray};
use std::ffi::c_void;

/// A tile (`U2DPoint<long>`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EePoint {
    pub x: i32,
    pub y: i32,
}

/// One `U2DSparseArrayPointContainer`'s points (opaque to C++), with the scratch list its point lists come back in.
#[derive(Clone, Default)]
pub struct EeSparseArray {
    pub set: SparseArray,
    scratch: Vec<EePoint>,
    raw: Vec<i32>,
}

/// A new, empty set.
#[no_mangle]
pub extern "C" fn ee_sparse_new() -> *mut EeSparseArray {
    Box::into_raw(Box::default())
}

/// A copy of `s` (its points; the copy owns its own).
///
/// # Safety
/// `s` from `ee_sparse_new`/`ee_sparse_clone`, not freed.
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_clone(s: *const EeSparseArray) -> *mut EeSparseArray {
    // SAFETY: the caller's contract
    let s = unsafe { &*s };
    Box::into_raw(Box::new(EeSparseArray { set: s.set.clone(), scratch: Vec::new(), raw: Vec::new() }))
}

/// The set as numbers (`SparseArray::raw`: a saved game's continent manager), its length in `count`.
///
/// # Safety
/// `s` a live set, `count` valid; the pointer lives until the next call on `s`.
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_raw(s: *mut EeSparseArray, count: *mut usize) -> *const i32 {
    // SAFETY: the caller's contract
    let s = unsafe { &mut *s };
    s.set.raw(&mut s.raw);
    // SAFETY: the caller's contract
    unsafe { *count = s.raw.len() };
    s.raw.as_ptr()
}

/// The set from `ee_sparse_raw`'s numbers; false (the set then empty) when they are not such a list.
///
/// # Safety
/// `s` a live set, `data` `count` numbers (NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_set_raw(s: *mut EeSparseArray, data: *const i32, count: usize) -> bool {
    // SAFETY: the caller's contract
    let s = unsafe { &mut *s };
    let data = if count == 0 || data.is_null() {
        &[][..]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(data, count) }
    };
    s.set.set_raw(data)
}

/// Releases a set.
///
/// # Safety
/// `s` from `ee_sparse_new`/`ee_sparse_clone`, not freed yet (NULL is ignored).
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_free(s: *mut EeSparseArray) {
    if !s.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(s) });
    }
}

/// `Size()`.
///
/// # Safety
/// `s` a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_size(s: *const EeSparseArray) -> i32 {
    // SAFETY: the caller's contract
    unsafe { &*s }.set.size()
}

/// `Clear()`.
///
/// # Safety
/// `s` a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_clear(s: *mut EeSparseArray) {
    // SAFETY: the caller's contract
    unsafe { &mut *s }.set.clear();
}

/// `Contains(point)`.
///
/// # Safety
/// `s` a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_contains(s: *const EeSparseArray, x: i32, y: i32) -> bool {
    // SAFETY: the caller's contract
    unsafe { &*s }.set.contains(EePoint { x, y })
}

/// `Insert(point)`.
///
/// # Safety
/// `s` a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_insert(s: *mut EeSparseArray, x: i32, y: i32) {
    // SAFETY: the caller's contract
    unsafe { &mut *s }.set.insert(EePoint { x, y });
}

/// `Insert(list)` (`create` false) or `Create(list)` (`create` true) over `count` points.
///
/// # Safety
/// `s` a live set, `points` valid for `count` (any pointer when `count` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_insert_list(
    s: *mut EeSparseArray,
    points: *const EePoint,
    count: usize,
    create: bool,
) {
    let points = if count == 0 || points.is_null() {
        &[][..]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(points, count) }
    };
    // SAFETY: the caller's contract
    let set = &mut unsafe { &mut *s }.set;
    if create {
        set.create(points);
    } else {
        set.insert_all(points);
    }
}

/// `Remove(point)`.
///
/// # Safety
/// `s` a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_remove(s: *mut EeSparseArray, x: i32, y: i32) -> bool {
    // SAFETY: the caller's contract
    unsafe { &mut *s }.set.remove(EePoint { x, y })
}

/// `BuildPointList(step)` (`extremes` false) or `GetOnlyExtremeEdgePoints` (true): the list in the set's scratch,
/// its length in `count`.
///
/// # Safety
/// `s` a live set, `count` valid; the pointer lives until the next call on `s`.
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_points(
    s: *mut EeSparseArray,
    extremes: bool,
    step: i32,
    count: *mut usize,
) -> *const EePoint {
    // SAFETY: the caller's contract
    let s = unsafe { &mut *s };
    let list = if extremes { PointList::Extremes } else { PointList::All(step) };
    s.set.points(list, &mut s.scratch);
    // SAFETY: the caller's contract
    unsafe { *count = s.scratch.len() };
    s.scratch.as_ptr()
}

/// `operator[](index)`.
///
/// # Safety
/// `s` a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_get(s: *const EeSparseArray, index: i32) -> EePoint {
    // SAFETY: the caller's contract
    unsafe { &*s }.set.get(index)
}

/// `CountContainerNodes()`.
///
/// # Safety
/// `s` a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_run_count(s: *const EeSparseArray) -> i32 {
    // SAFETY: the caller's contract
    unsafe { &*s }.set.run_count()
}

/// `GetClosestPoint(point)`.
///
/// # Safety
/// `s` a live set.
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_closest_point(s: *const EeSparseArray, x: i32, y: i32) -> EePoint {
    // SAFETY: the caller's contract
    unsafe { &*s }.set.closest_point(EePoint { x, y })
}

/// `GetRandomPoint(random)`: `generate(random)` is the caller's `URandom::Generate()`, called in the C++'s order.
///
/// # Safety
/// `s` a live set; `generate` must not reach `s`.
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_random_point(
    s: *const EeSparseArray,
    generate: extern "C" fn(*mut c_void) -> f32,
    random: *mut c_void,
) -> EePoint {
    // SAFETY: the caller's contract
    let set = &unsafe { &*s }.set;
    set.random_point(&mut || generate(random))
}
