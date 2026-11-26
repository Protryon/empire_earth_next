//! The engine's sparse point set (`U2DSparseArrayPointContainer`, `include/lle/U2DSparseArray.h`): `set` is the set,
//! this module its C ABI. The C++ class owns a handle (`EeSparseArray`) and its members are inline forwarders over
//! this module's header, rust/base/sparse.h, which names nothing else (the AI's areas, path sub sectors and the
//! explore goal hold their tiles in one; the AI's A* searches, ee_sim::astar, re-export it).

pub mod set;

use set::{PointList, SparseArray};
use std::ffi::{c_long, c_void};

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

/// A C++ `std::vector<U2DPoint<long>>` or `std::vector<long>` resized to `count` points or numbers: its data, as
/// `long`s (a point is two).
pub type EeSparseResize = unsafe extern "C" fn(*mut c_void, usize) -> *mut c_long;

/// `BuildPointList(list, step)` (`extremes` false) and `GetOnlyExtremeEdgePoints(list)` (true): the points into the
/// C++ vector `out`, which `resize` makes their count long.
///
/// # Safety
/// `s` a live set; `resize(out, n)` returns room for `n` points (`2 n` longs).
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_points_into(
    s: *mut EeSparseArray,
    extremes: bool,
    step: i32,
    out: *mut c_void,
    resize: EeSparseResize,
) {
    // SAFETY: the caller's contract
    let s = unsafe { &mut *s };
    let list = if extremes { PointList::Extremes } else { PointList::All(step) };
    s.set.points(list, &mut s.scratch);
    // SAFETY: the caller's contract
    let data = unsafe { resize(out, s.scratch.len()) };
    for (i, point) in s.scratch.iter().enumerate() {
        // SAFETY: `data` has room for the points
        unsafe {
            *data.add(2 * i) = point.x as c_long;
            *data.add(2 * i + 1) = point.y as c_long;
        }
    }
}

/// `Create(points)` (`create`) and `Insert(points)`: `count` points of a `std::vector<U2DPoint<long>>` (two `long`s
/// each, taken as their low 32 bits as the C++ did).
///
/// # Safety
/// `s` a live set, `points` `2 count` longs (NULL with 0).
#[no_mangle]
#[allow(clippy::unnecessary_cast)] // `long` is `i32` on Windows, `i64` on Linux and macOS: the low 32 bits
pub unsafe extern "C" fn ee_sparse_insert_longs(
    s: *mut EeSparseArray,
    points: *const c_long,
    count: usize,
    create: bool,
) {
    let points: Vec<EePoint> = (0..count)
        // SAFETY: the caller's contract
        .map(|i| unsafe { EePoint { x: *points.add(2 * i) as i32, y: *points.add(2 * i + 1) as i32 } })
        .collect();
    // SAFETY: the caller's contract
    unsafe { ee_sparse_insert_list(s, points.as_ptr(), points.len(), create) }
}

/// `GetRaw(raw)`: [`ee_sparse_raw`]'s numbers into the C++ vector `out`, which `resize` makes their count long.
///
/// # Safety
/// `s` a live set; `resize(out, n)` returns room for `n` longs.
#[no_mangle]
pub unsafe extern "C" fn ee_sparse_raw_into(s: *mut EeSparseArray, out: *mut c_void, resize: EeSparseResize) {
    // SAFETY: the caller's contract
    let s = unsafe { &mut *s };
    s.set.raw(&mut s.raw);
    // SAFETY: the caller's contract
    let data = unsafe { resize(out, s.raw.len()) };
    for (i, &number) in s.raw.iter().enumerate() {
        // SAFETY: `data` has room for the numbers
        unsafe { *data.add(i) = number as c_long };
    }
}

/// `SetRaw(raw)`: [`ee_sparse_set_raw`] over `count` longs (their low 32 bits).
///
/// # Safety
/// `s` a live set, `data` `count` longs (NULL with 0).
#[no_mangle]
#[allow(clippy::unnecessary_cast)] // `long` is `i32` on Windows, `i64` on Linux and macOS: the low 32 bits
pub unsafe extern "C" fn ee_sparse_set_raw_longs(s: *mut EeSparseArray, data: *const c_long, count: usize) -> bool {
    // SAFETY: the caller's contract
    let numbers: Vec<i32> = (0..count).map(|i| unsafe { *data.add(i) } as i32).collect();
    // SAFETY: the caller's contract
    unsafe { ee_sparse_set_raw(s, numbers.as_ptr(), numbers.len()) }
}

#[cfg(test)]
mod long_tests {
    use super::*;

    unsafe extern "C" fn resize(out: *mut c_void, count: usize) -> *mut c_long {
        let v = &mut *(out as *mut Vec<c_long>);
        v.resize(count, 0);
        v.as_mut_ptr()
    }

    /// A `std::vector<U2DPoint<long>>`'s resize: two longs a point.
    unsafe extern "C" fn resize_points(out: *mut c_void, count: usize) -> *mut c_long {
        resize(out, 2 * count)
    }

    #[test]
    fn the_long_forms_equal_the_point_forms() {
        unsafe {
            let s = ee_sparse_new();
            let input: Vec<c_long> = vec![3, 4, 5, 4, 3, 9, -2, 7];
            ee_sparse_insert_longs(s, input.as_ptr(), 4, true);
            let mut got: Vec<c_long> = vec![99];
            ee_sparse_points_into(s, false, 1, &mut got as *mut Vec<c_long> as *mut c_void, resize_points);
            let mut count = 0;
            let points = std::slice::from_raw_parts(ee_sparse_points(s, false, 1, &mut count), count).to_vec();
            assert_eq!(got.len(), 2 * points.len());
            for (i, p) in points.iter().enumerate() {
                assert_eq!((got[2 * i], got[2 * i + 1]), (p.x as c_long, p.y as c_long));
            }
            assert_eq!(ee_sparse_size(s), 4);
            let mut raw: Vec<c_long> = Vec::new();
            ee_sparse_raw_into(s, &mut raw as *mut Vec<c_long> as *mut c_void, resize);
            let mut n = 0;
            let expected = std::slice::from_raw_parts(ee_sparse_raw(s, &mut n), n).to_vec();
            assert_eq!(raw, expected.iter().map(|&x| x as c_long).collect::<Vec<_>>());
            let t = ee_sparse_new();
            assert!(ee_sparse_set_raw_longs(t, raw.as_ptr(), raw.len()));
            assert_eq!(ee_sparse_size(t), 4);
            ee_sparse_free(t);
            ee_sparse_free(s);
        }
    }
}
