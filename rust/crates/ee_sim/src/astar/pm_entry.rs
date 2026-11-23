//! The path manager's two bounded searches as their callers make them (the paths lane's round 1; were
//! `EEPathManager::FindPath` and `FindBoundedPath` in EEPathManager.cpp): each one its PMRQ record (kinds 3 and 4:
//! the inputs, then the outputs, in the C++'s word order) around the manager's call (27, 28). The results are Rust
//! lists; the C++ callers left (the storm, the classic_lua wall planner) take them through `ee_path_find_path`.

use crate::astar::continent_grid::EePoint;
use crate::astar::path_splice::EePathPoint;
#[cfg(not(test))]
use crate::astar::pm::{global, EePathIo, EePathManager};
#[cfg(not(test))]
use std::ffi::{c_long, c_void};

/// A bounded tile search's window (`EEAIAStarBounds`: top, left, bottom, right).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EePathWindow {
    pub top: i32,
    pub left: i32,
    pub bottom: i32,
    pub right: i32,
}

/// The PMRQ words of a search's inputs and outputs (pm_capture.rs's open scope).
pub(crate) struct Words(pub Vec<u32>);

impl Words {
    pub fn i(&mut self, v: i32) -> &mut Self {
        self.0.push(v as u32);
        self
    }
    pub fn tile(&mut self, x: i32, y: i32) -> &mut Self {
        self.i(x).i(y)
    }
    pub fn tiles(&mut self, t: &[EePoint]) -> &mut Self {
        self.i(t.len() as i32);
        for p in t {
            self.tile(p.x, p.y);
        }
        self
    }
    pub fn points(&mut self, p: &[EePathPoint]) -> &mut Self {
        self.i(p.len() as i32);
        for q in p {
            self.0.extend([q.x.to_bits(), q.y.to_bits(), q.z.to_bits()]);
        }
        self
    }
}

/// `FindPath`'s input words: the unit's id, the ends, the window (top, left, bottom, right), the kind, the step
/// limit, whether there is a modifier and whether the visited tiles are asked.
#[allow(clippy::too_many_arguments)]
pub(crate) fn find_path_inputs(
    unit_id: u32,
    from: (i32, i32),
    to: (i32, i32),
    b: EePathWindow,
    kind: i32,
    revs: i32,
    modifier: bool,
    visited: bool,
) -> Vec<u32> {
    let mut w = Words(Vec::new());
    w.i(unit_id as i32).tile(from.0, from.1).tile(to.0, to.1).i(b.top).i(b.left).i(b.bottom).i(b.right);
    w.i(kind).i(revs).i(modifier as i32).i(visited as i32);
    w.0
}

/// `FindBoundedPath`'s input words: the unit's id, the kind, the ends, the margin and the (empty) path.
pub(crate) fn bounded_inputs(unit_id: u32, kind: i32, from: (i32, i32), to: (i32, i32), margin: i32) -> Vec<u32> {
    let mut w = Words(Vec::new());
    w.i(unit_id as i32).i(kind).tile(from.0, from.1).tile(to.0, to.1).i(margin).points(&[]);
    w.0
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::astar::pm::ee_path_manager_call;
    use crate::astar::pm_capture::{ee_path_capture_close, ee_path_capture_open, ee_path_capture_word};
    use crate::world::{UnitView, WorldRead};

    /// `EEPathCaptureUnitID`: the unit's id, 0xffffffff for none.
    fn unit_id(unit: u64) -> u32 {
        // SAFETY: the simulation thread holds the world; 0 or a live unit
        let t = unsafe { WorldRead::begin() };
        unsafe { UnitView::of(&t, unit) }.map_or(0xffff_ffff, |u| u.id())
    }

    fn put(words: &[u32]) {
        for &w in words {
            ee_path_capture_word(w);
        }
    }

    /// `FindPath(unit, map, from, to, window, path, kind, revs, modifier, visited)`: the path, and the visited tiles
    /// when `visited`.
    ///
    /// # Safety
    /// `p` the live manager, `unit` 0 or a live complex unit, `map` the live map, `modifier` 0 or a live modifier.
    #[allow(clippy::too_many_arguments)]
    pub unsafe fn find_path_on(
        p: *mut EePathManager,
        unit: u64,
        map: u64,
        from: (i32, i32),
        to: (i32, i32),
        b: EePathWindow,
        kind: i32,
        revs: i32,
        modifier: u64,
        visited: bool,
    ) -> (Vec<EePoint>, Vec<EePoint>) {
        // SAFETY: the caller's contract
        let capture = unsafe { ee_path_capture_open(p, 3) };
        if capture {
            put(&find_path_inputs(unit_id(unit), from, to, b, kind, revs, modifier != 0, visited));
        }
        let mut path: Vec<EePoint> = Vec::new();
        let mut seen: Vec<EePoint> = Vec::new();
        let seen_handle = if visited { &mut seen as *mut Vec<EePoint> as u64 } else { 0 };
        let mut io =
            EePathIo { h: [unit, map, &mut path as *mut Vec<EePoint> as u64, seen_handle], ..Default::default() };
        let v = [from.0, from.1, to.0, to.1, b.bottom, b.left, b.right, b.top, kind, revs];
        for (k, x) in v.iter().enumerate() {
            io.i[k] = i64::from(*x);
        }
        io.i[10] = modifier as i64;
        // SAFETY: as above
        unsafe { ee_path_manager_call(p, 27, &mut io) };
        if capture {
            let mut w = Words(Vec::new());
            w.tiles(&path);
            if visited {
                w.tiles(&seen);
            }
            put(&w.0);
        }
        // SAFETY: as above
        unsafe { ee_path_capture_close(p) };
        (path, seen)
    }

    /// [`find_path_on`] of the game's manager.
    ///
    /// # Safety
    /// As [`find_path_on`].
    #[allow(clippy::too_many_arguments)]
    pub unsafe fn find_path(
        unit: u64,
        map: u64,
        from: (i32, i32),
        to: (i32, i32),
        b: EePathWindow,
        kind: i32,
        revs: i32,
        modifier: u64,
    ) -> Vec<EePoint> {
        // SAFETY: the caller's contract
        unsafe { find_path_on(global(), unit, map, from, to, b, kind, revs, modifier, false) }.0
    }

    /// `FindBoundedPath(unit, map, kind, from, to, path, margin)`: whether it reached `to`, and the path (start first).
    ///
    /// # Safety
    /// `unit` 0 or a live complex unit, `map` the live map.
    pub unsafe fn find_bounded_path(
        unit: u64,
        map: u64,
        kind: i32,
        from: (i32, i32),
        to: (i32, i32),
        margin: i32,
    ) -> (bool, Vec<EePathPoint>) {
        let p = global();
        // SAFETY: the game's live manager
        let capture = unsafe { ee_path_capture_open(p, 4) };
        if capture {
            put(&bounded_inputs(unit_id(unit), kind, from, to, margin));
        }
        let mut path: Vec<EePathPoint> = Vec::new();
        let mut io = EePathIo::default();
        io.h[0] = unit;
        io.h[1] = map;
        let v = [from.0, from.1, to.0, to.1, kind, margin];
        for (k, x) in v.iter().enumerate() {
            io.i[k] = i64::from(*x);
        }
        io.i[6] = &mut path as *mut Vec<EePathPoint> as i64;
        // SAFETY: as above
        let reached = unsafe { ee_path_manager_call(p, 28, &mut io) } != 0;
        if capture {
            let mut w = Words(Vec::new());
            w.i(reached as i32).points(&path);
            put(&w.0);
        }
        // SAFETY: as above
        unsafe { ee_path_capture_close(p) };
        (reached, path)
    }
}

#[cfg(not(test))]
pub use game::*;

/// `EEPathManager::FindPath` for the C++ callers: `from`, `to` the ends, `bounds` the window (a
/// `U2DRectangle<long>`); `clear(path)` then the path's tiles pushed into `path`, and when `visited` is not null the
/// visited tiles appended to it.
///
/// # Safety
/// `p` the live manager; the handles as [`find_path_on`]'s; the points and the rectangle readable; the lists the
/// callbacks' own.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments, clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64
pub unsafe extern "C" fn ee_path_find_path_into(
    p: *mut EePathManager,
    unit: u64,
    map: u64,
    from: *const crate::astar::hull::EeHullPoint,
    to: *const crate::astar::hull::EeHullPoint,
    bounds: *const crate::astar::area_geom::EeHullRect,
    kind: c_long,
    revs: c_long,
    modifier: u64,
    path: *mut c_void,
    visited: *mut c_void,
    push: extern "C" fn(*mut c_void, i32, i32),
    clear: extern "C" fn(*mut c_void),
) {
    // SAFETY: the caller's contract
    let (from, to, b) = unsafe { (*from, *to, *bounds) };
    let window =
        EePathWindow { top: b.top as i32, left: b.left as i32, bottom: b.bottom as i32, right: b.right as i32 };
    let (tiles, seen) = unsafe {
        find_path_on(
            p,
            unit,
            map,
            (from.x as i32, from.y as i32),
            (to.x as i32, to.y as i32),
            window,
            kind as i32,
            revs as i32,
            modifier,
            !visited.is_null(),
        )
    };
    clear(path);
    for t in &tiles {
        push(path, t.x, t.y);
    }
    if !visited.is_null() {
        for t in &seen {
            push(visited, t.x, t.y);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn find_paths_inputs_in_the_cpp_order() {
        let b = EePathWindow { top: 1, left: 2, bottom: 3, right: 4 };
        let w = find_path_inputs(0xffff_ffff, (5, 6), (7, 8), b, 0xb, 200, true, false);
        assert_eq!(w, vec![0xffff_ffff, 5, 6, 7, 8, 1, 2, 3, 4, 0xb, 200, 1, 0]);
        let w = find_path_inputs(9, (-1, 0), (0, 0), b, 4, 0x6e, false, true);
        assert_eq!(&w[..3], &[9, (-1i32) as u32, 0]);
        assert_eq!(&w[11..], &[0, 1]);
    }

    #[test]
    fn bounded_inputs_end_with_the_empty_path() {
        assert_eq!(bounded_inputs(3, 2, (1, 1), (4, 5), 50), vec![3, 2, 1, 1, 4, 5, 50, 0]);
    }

    #[test]
    fn the_outputs_words() {
        let mut w = Words(Vec::new());
        w.tiles(&[EePoint { x: 1, y: -2 }]).points(&[EePathPoint { x: 1.5, y: 0.0, z: -0.25 }]);
        assert_eq!(w.0, vec![1, 1, (-2i32) as u32, 1, 1.5f32.to_bits(), 0, (-0.25f32).to_bits()]);
    }
}
