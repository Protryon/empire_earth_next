//! `UGraphSeries` and `UGraph` whole in Rust (LLE lane round 7): a set of named series of integer points that the
//! post-game statistics fill and `UIGraph`/`UILineGraph` draw.
//!
//! The C++ classes keep their names and are views: `UGraphSeries` holds one [`EeUtilSeries`] pointer, `UGraph` an
//! [`EeUtilGraph`] pointer and its three `UWideString`s (the title and the axis labels, C++ objects with a vtable).
//! A graph's series are a `Vec` of series pointers, so `UGraph::GetSeries` hands out the address of one slot as a
//! `const UGraphSeries&` (a class whose only member is that pointer), and `GetDataPoint` the address of a point,
//! laid out as `U2DPoint<long>` (x, y as the build's `long`). Like the C++ vector's, such a reference lasts until
//! the next change of its container.
//!
//! The arithmetic is the original's: `long` comparisons, the empty range 0x7fffffff..INT32_MIN, the data points
//! sorted by x then y (a total order: the sort's result is unique), `RemoveDataPoint`/`RemoveSeries` leaving the
//! range as it was, and `CombineGraph`'s value lookup with the round 1 fix (the found point's value is kept).

use std::ffi::{c_long, c_ulong};

/// `0x7fffffff`: the minimum of an empty range.
const EMPTY_MIN: c_long = 0x7fff_ffff;
/// `INT32_MIN` (0x80000000 in the 32-bit original): the maximum of an empty range.
const EMPTY_MAX: c_long = i32::MIN as c_long;

/// A data point, laid out as the C++ `U2DPoint<long>`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeUtilPoint {
    pub x: c_long,
    pub y: c_long,
}

/// A colour, laid out as the C++ `GEColor` (red, green, blue).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeUtilColor {
    pub red: f32,
    pub green: f32,
    pub blue: f32,
}

/// A range of x and y values (`GetMinX`.. `GetMaxY`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeUtilRange {
    pub min_x: c_long,
    pub max_x: c_long,
    pub min_y: c_long,
    pub max_y: c_long,
}

impl EeUtilRange {
    const EMPTY: EeUtilRange = EeUtilRange { min_x: EMPTY_MIN, max_x: EMPTY_MAX, min_y: EMPTY_MIN, max_y: EMPTY_MAX };

    /// Widens the range to include another (each bound on its own, as the C++'s four `if`s).
    fn widen(&mut self, other: &EeUtilRange) {
        if other.min_x < self.min_x {
            self.min_x = other.min_x;
        }
        if other.max_x > self.max_x {
            self.max_x = other.max_x;
        }
        if other.min_y < self.min_y {
            self.min_y = other.min_y;
        }
        if other.max_y > self.max_y {
            self.max_y = other.max_y;
        }
    }
}

/// A `UGraphSeries`: its points sorted by x then y, its colour, range and enabled flag.
#[derive(Clone, Debug, PartialEq)]
pub struct EeUtilSeries {
    points: Vec<EeUtilPoint>,
    color: EeUtilColor,
    range: EeUtilRange,
    enabled: bool,
}

impl Default for EeUtilSeries {
    /// `UGraphSeries()`: empty, enabled, white.
    fn default() -> Self {
        EeUtilSeries {
            points: Vec::new(),
            color: EeUtilColor { red: 1.0, green: 1.0, blue: 1.0 },
            range: EeUtilRange::EMPTY,
            enabled: true,
        }
    }
}

impl EeUtilSeries {
    /// `ClearAllDataPoints`.
    pub fn clear(&mut self) {
        self.points.clear();
        self.range = EeUtilRange::EMPTY;
    }

    /// `AddDataPoint`: replaces the last point when it has the same x, keeps the points sorted, recomputes the range.
    pub fn add_point(&mut self, point: EeUtilPoint) {
        if self.points.last().is_some_and(|last| last.x == point.x) {
            self.points.pop();
        }
        self.points.push(point);
        self.points.sort_by(|a, b| a.x.cmp(&b.x).then(a.y.cmp(&b.y)));
        self.recompute();
    }

    /// `RecomputeMinMaxValues`.
    fn recompute(&mut self) {
        self.range = EeUtilRange::EMPTY;
        for p in &self.points {
            let r = &mut self.range;
            if p.x < r.min_x {
                r.min_x = p.x;
            }
            if p.x > r.max_x {
                r.max_x = p.x;
            }
            if p.y < r.min_y {
                r.min_y = p.y;
            }
            if p.y > r.max_y {
                r.max_y = p.y;
            }
        }
    }

    pub fn points(&self) -> &[EeUtilPoint] {
        &self.points
    }

    pub fn range(&self) -> EeUtilRange {
        self.range
    }

    /// `UGGetValueAt`: the y of the last point at or before x; false when the series is empty or x is below its
    /// (stored) minimum. Past the last point, the last point's y.
    fn value_at(&self, x: c_long) -> Option<c_long> {
        if self.points.is_empty() || x < self.range.min_x {
            return None;
        }
        let mut next = EeUtilPoint::default();
        for (i, point) in self.points.iter().enumerate() {
            next = *self.points.get(i + 1).unwrap_or(point);
            if x >= point.x && x < next.x {
                return Some(point.y);
            }
        }
        Some(next.y)
    }
}

/// A `UGraph`'s series and range (its strings stay in the C++ object). The series are boxed: the C++ views hold
/// their addresses.
#[derive(Debug)]
pub struct EeUtilGraph {
    series: Vec<*mut EeUtilSeries>,
    range: EeUtilRange,
}

impl Default for EeUtilGraph {
    fn default() -> Self {
        EeUtilGraph { series: Vec::new(), range: EeUtilRange::EMPTY }
    }
}

impl Clone for EeUtilGraph {
    fn clone(&self) -> Self {
        EeUtilGraph { series: self.series.iter().map(|&s| boxed(self.at_ptr(s).clone())).collect(), range: self.range }
    }
}

impl Drop for EeUtilGraph {
    fn drop(&mut self) {
        self.clear();
    }
}

fn boxed(series: EeUtilSeries) -> *mut EeUtilSeries {
    Box::into_raw(Box::new(series))
}

impl EeUtilGraph {
    fn at_ptr(&self, series: *mut EeUtilSeries) -> &EeUtilSeries {
        // SAFETY: every pointer in `series` is a live box this graph owns
        unsafe { &*series }
    }

    pub fn len(&self) -> usize {
        self.series.len()
    }

    pub fn is_empty(&self) -> bool {
        self.series.is_empty()
    }

    pub fn series(&self, index: usize) -> &EeUtilSeries {
        self.at_ptr(self.series[index])
    }

    fn series_mut(&mut self, index: usize) -> &mut EeUtilSeries {
        // SAFETY: as `at_ptr`, and `&mut self` makes it unique
        unsafe { &mut *self.series[index] }
    }

    pub fn range(&self) -> EeUtilRange {
        self.range
    }

    /// `AddSeries`: a copy of the series; the range widens to include it.
    pub fn add_series(&mut self, series: &EeUtilSeries) {
        let range = series.range;
        self.series.push(boxed(series.clone()));
        self.range.widen(&range);
    }

    /// `ClearAllSeries`.
    pub fn clear(&mut self) {
        for s in self.series.drain(..) {
            // SAFETY: the graph owned the box
            drop(unsafe { Box::from_raw(s) });
        }
        self.range = EeUtilRange::EMPTY;
    }

    /// `RemoveSeries` (the range stays).
    pub fn remove_series(&mut self, index: usize) {
        let s = self.series.remove(index);
        // SAFETY: the graph owned the box
        drop(unsafe { Box::from_raw(s) });
    }

    pub fn set_enabled(&mut self, index: usize, enabled: bool) {
        self.series_mut(index).enabled = enabled;
    }

    /// `RecomputeMinMaxValues`.
    fn recompute(&mut self) {
        let mut range = EeUtilRange::EMPTY;
        for &s in &self.series {
            range.widen(&self.at_ptr(s).range);
        }
        self.range = range;
    }

    /// `operator=` (the series and the range; the strings are the C++'s).
    pub fn assign(&mut self, other: &EeUtilGraph) {
        *self = other.clone();
    }

    /// One series of `CombineGraph`: the sum of both series' values at every x either has (a missing value is 0).
    fn combined(first: &EeUtilSeries, second: &EeUtilSeries) -> EeUtilSeries {
        let mut xs = EeUtilSeries::default();
        for point in first.points.iter().chain(second.points.iter()) {
            if !xs.points.iter().any(|p| p.x == point.x) {
                xs.add_point(*point);
            }
        }
        let mut result = EeUtilSeries::default();
        for point in &xs.points {
            let y1 = first.value_at(point.x).unwrap_or(0);
            let y2 = second.value_at(point.x).unwrap_or(0);
            result.add_point(EeUtilPoint { x: point.x, y: y1.wrapping_add(y2) });
        }
        result
    }

    /// `PruneGraph`: every series reduced to one point per x, the highest y (the first such point in order).
    pub fn prune(&mut self) {
        let mut pruned = Vec::with_capacity(self.series.len());
        for &s in &self.series {
            let source = self.at_ptr(s);
            let mut series = source.clone();
            series.clear();
            let mut xs: Vec<c_long> = Vec::new();
            for point in &source.points {
                if xs.contains(&point.x) {
                    continue;
                }
                let mut best = *point;
                for other in &source.points {
                    if best.x == other.x && best.y < other.y {
                        best = *other;
                    }
                }
                xs.push(best.x);
                series.add_point(best);
            }
            pruned.push(series);
        }
        self.clear();
        for series in &pruned {
            self.add_series(series);
        }
        self.recompute();
    }
}

/// `CombineGraph(in, out)`: per series index of both, `out` gets the combined series; `out`'s range is then
/// recomputed. `out` may be either input (it only grows past the series read).
///
/// # Safety
/// The three pointers are live graphs; `this` and `other` are not changed.
unsafe fn combine(this: *const EeUtilGraph, other: *const EeUtilGraph, out: *mut EeUtilGraph) {
    let count = (*this).len().min((*other).len());
    for i in 0..count {
        let series = EeUtilGraph::combined((*this).series(i), (*other).series(i));
        (*out).add_series(&series);
    }
    (*out).recompute();
}

// ---- the C interface (lle/UGraph.h)

/// `UGraphSeries()`.
#[no_mangle]
pub extern "C" fn ee_util_series_new() -> *mut EeUtilSeries {
    boxed(EeUtilSeries::default())
}

/// The implicit copy constructor.
///
/// # Safety
/// `source` is a live series.
#[no_mangle]
pub unsafe extern "C" fn ee_util_series_clone(source: *const EeUtilSeries) -> *mut EeUtilSeries {
    boxed((*source).clone())
}

/// The destructor.
///
/// # Safety
/// `series` came from `ee_util_series_new`/`_clone` and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_util_series_free(series: *mut EeUtilSeries) {
    if !series.is_null() {
        drop(Box::from_raw(series));
    }
}

/// `operator=`.
///
/// # Safety
/// Both are live series.
#[no_mangle]
pub unsafe extern "C" fn ee_util_series_assign(series: *mut EeUtilSeries, source: *const EeUtilSeries) {
    if !std::ptr::eq(series, source) {
        *series = (*source).clone();
    }
}

/// `AddDataPoint`.
///
/// # Safety
/// `series` is a live series.
#[no_mangle]
pub unsafe extern "C" fn ee_util_series_add_point(series: *mut EeUtilSeries, x: c_long, y: c_long) {
    (*series).add_point(EeUtilPoint { x, y });
}

/// `ClearAllDataPoints`.
///
/// # Safety
/// `series` is a live series.
#[no_mangle]
pub unsafe extern "C" fn ee_util_series_clear(series: *mut EeUtilSeries) {
    (*series).clear();
}

/// `GetDataPoint`: the point's address (valid until the series changes).
///
/// # Safety
/// `series` is a live series; `index` is below its point count.
#[no_mangle]
pub unsafe extern "C" fn ee_util_series_point(series: *const EeUtilSeries, index: c_ulong) -> *const EeUtilPoint {
    &(&(*series).points)[index as usize]
}

/// `GetDataPointCount`.
///
/// # Safety
/// `series` is a live series.
#[no_mangle]
pub unsafe extern "C" fn ee_util_series_count(series: *const EeUtilSeries) -> c_ulong {
    (*series).points.len() as c_ulong
}

/// `RemoveDataPoint` (the range stays).
///
/// # Safety
/// `series` is a live series; `index` is below its point count.
#[no_mangle]
pub unsafe extern "C" fn ee_util_series_remove_point(series: *mut EeUtilSeries, index: c_ulong) {
    (*series).points.remove(index as usize);
}

/// `GetColor`: the colour's address (a `GEColor`).
///
/// # Safety
/// `series` is a live series.
#[no_mangle]
pub unsafe extern "C" fn ee_util_series_color(series: *const EeUtilSeries) -> *const EeUtilColor {
    &(*series).color
}

/// `SetColor`.
///
/// # Safety
/// `series` is a live series; `color` points at three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_util_series_set_color(series: *mut EeUtilSeries, color: *const EeUtilColor) {
    (*series).color = *color;
}

/// `GetIsEnabled`.
///
/// # Safety
/// `series` is a live series.
#[no_mangle]
pub unsafe extern "C" fn ee_util_series_enabled(series: *const EeUtilSeries) -> bool {
    (*series).enabled
}

/// `SetIsEnabled`.
///
/// # Safety
/// `series` is a live series.
#[no_mangle]
pub unsafe extern "C" fn ee_util_series_set_enabled(series: *mut EeUtilSeries, enabled: bool) {
    (*series).enabled = enabled;
}

/// `GetMinX`/`GetMaxX`/`GetMinY`/`GetMaxY`: the range's address.
///
/// # Safety
/// `series` is a live series.
#[no_mangle]
pub unsafe extern "C" fn ee_util_series_range(series: *const EeUtilSeries) -> *const EeUtilRange {
    &(*series).range
}

/// `UGraph()` (the strings are the C++'s).
#[no_mangle]
pub extern "C" fn ee_util_graph_new() -> *mut EeUtilGraph {
    Box::into_raw(Box::default())
}

/// The implicit copy constructor.
///
/// # Safety
/// `source` is a live graph.
#[no_mangle]
pub unsafe extern "C" fn ee_util_graph_clone(source: *const EeUtilGraph) -> *mut EeUtilGraph {
    Box::into_raw(Box::new((*source).clone()))
}

/// The destructor: the graph and its series.
///
/// # Safety
/// `graph` came from `ee_util_graph_new`/`_clone` and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_util_graph_free(graph: *mut EeUtilGraph) {
    if !graph.is_null() {
        drop(Box::from_raw(graph));
    }
}

/// `operator=` (series and range).
///
/// # Safety
/// Both are live graphs.
#[no_mangle]
pub unsafe extern "C" fn ee_util_graph_assign(graph: *mut EeUtilGraph, source: *const EeUtilGraph) {
    if !std::ptr::eq(graph, source) {
        (*graph).assign(&*source);
    }
}

/// `AddSeries`.
///
/// # Safety
/// `graph` and `series` are live (the series is not one of the graph's own: the C++ view copies it first).
#[no_mangle]
pub unsafe extern "C" fn ee_util_graph_add_series(graph: *mut EeUtilGraph, series: *const EeUtilSeries) {
    let copy = (*series).clone();
    (*graph).add_series(&copy);
}

/// `ClearAllSeries`.
///
/// # Safety
/// `graph` is a live graph.
#[no_mangle]
pub unsafe extern "C" fn ee_util_graph_clear(graph: *mut EeUtilGraph) {
    (*graph).clear();
}

/// `CombineGraph`.
///
/// # Safety
/// The three are live graphs (`out` may be either of the others).
#[no_mangle]
pub unsafe extern "C" fn ee_util_graph_combine(
    graph: *const EeUtilGraph,
    other: *const EeUtilGraph,
    out: *mut EeUtilGraph,
) {
    combine(graph, other, out);
}

/// `GetSeries`: the address of the series' slot, which the C++ reads as a `UGraphSeries` (valid until the graph
/// changes).
///
/// # Safety
/// `graph` is a live graph; `index` is below its series count.
#[no_mangle]
pub unsafe extern "C" fn ee_util_graph_series(graph: *const EeUtilGraph, index: c_ulong) -> *const *mut EeUtilSeries {
    &(&(*graph).series)[index as usize]
}

/// `GetSeriesCount`.
///
/// # Safety
/// `graph` is a live graph.
#[no_mangle]
pub unsafe extern "C" fn ee_util_graph_count(graph: *const EeUtilGraph) -> c_ulong {
    (*graph).len() as c_ulong
}

/// The range's address.
///
/// # Safety
/// `graph` is a live graph.
#[no_mangle]
pub unsafe extern "C" fn ee_util_graph_range(graph: *const EeUtilGraph) -> *const EeUtilRange {
    &(*graph).range
}

/// `PruneGraph`.
///
/// # Safety
/// `graph` is a live graph.
#[no_mangle]
pub unsafe extern "C" fn ee_util_graph_prune(graph: *mut EeUtilGraph) {
    (*graph).prune();
}

/// `RemoveSeries`.
///
/// # Safety
/// `graph` is a live graph; `index` is below its series count.
#[no_mangle]
pub unsafe extern "C" fn ee_util_graph_remove_series(graph: *mut EeUtilGraph, index: c_ulong) {
    (*graph).remove_series(index as usize);
}

/// `SetIsSeriesEnabled`.
///
/// # Safety
/// `graph` is a live graph; `index` is below its series count.
#[no_mangle]
pub unsafe extern "C" fn ee_util_graph_set_enabled(graph: *mut EeUtilGraph, index: c_ulong, enabled: bool) {
    (*graph).set_enabled(index as usize, enabled);
}

#[cfg(test)]
mod tests {
    use super::*;

    fn p(x: c_long, y: c_long) -> EeUtilPoint {
        EeUtilPoint { x, y }
    }

    #[test]
    fn add_point_sorts_and_replaces_the_last_x() {
        let mut s = EeUtilSeries::default();
        assert_eq!(s.range, EeUtilRange::EMPTY);
        s.add_point(p(5, 1));
        s.add_point(p(2, 7));
        s.add_point(p(5, 3)); // the last point (5, 1) has x 5: replaced
        assert_eq!(s.points, vec![p(2, 7), p(5, 3)]);
        s.add_point(p(2, 0)); // the last is (5, 3): kept, (2, 0) sorts first
        assert_eq!(s.points, vec![p(2, 0), p(2, 7), p(5, 3)]);
        assert_eq!(s.range, EeUtilRange { min_x: 2, max_x: 5, min_y: 0, max_y: 7 });
        s.points.remove(0);
        assert_eq!(s.range.min_y, 0, "RemoveDataPoint leaves the range");
    }

    #[test]
    fn value_at_keeps_the_found_point() {
        let mut s = EeUtilSeries::default();
        assert_eq!(s.value_at(0), None);
        s.add_point(p(0, 10));
        s.add_point(p(10, 20));
        s.add_point(p(20, 30));
        assert_eq!(s.value_at(-1), None);
        assert_eq!(s.value_at(0), Some(10));
        assert_eq!(s.value_at(9), Some(10));
        assert_eq!(s.value_at(10), Some(20));
        assert_eq!(s.value_at(25), Some(30));
    }

    #[test]
    fn combine_and_prune() {
        let mut a = EeUtilGraph::default();
        let mut b = EeUtilGraph::default();
        let mut s = EeUtilSeries::default();
        s.add_point(p(0, 1));
        s.add_point(p(10, 2));
        a.add_series(&s);
        s.clear();
        s.add_point(p(5, 100));
        b.add_series(&s);
        b.add_series(&s); // a has one series: one combined
        let mut out = EeUtilGraph::default();
        unsafe { combine(&a, &b, &mut out) };
        assert_eq!(out.len(), 1);
        assert_eq!(out.series(0).points, vec![p(0, 1), p(5, 101), p(10, 102)]);
        assert_eq!(out.range, EeUtilRange { min_x: 0, max_x: 10, min_y: 1, max_y: 102 });
        // combining into an input
        let a_ptr: *mut EeUtilGraph = &mut a;
        unsafe { combine(a_ptr, &b, a_ptr) };
        assert_eq!(a.len(), 2);

        let mut g = EeUtilGraph::default();
        let s = EeUtilSeries {
            points: vec![p(1, 5), p(1, 9), p(2, 3), p(1, 7)],
            color: EeUtilColor { red: 0.5, green: 1.0, blue: 1.0 },
            ..EeUtilSeries::default()
        };
        g.add_series(&s);
        g.prune();
        assert_eq!(g.series(0).points, vec![p(1, 9), p(2, 3)]);
        assert_eq!(g.series(0).color.red, 0.5);
        assert_eq!(g.range, EeUtilRange { min_x: 1, max_x: 2, min_y: 3, max_y: 9 });
    }

    #[test]
    fn graph_copies_are_deep() {
        let mut g = EeUtilGraph::default();
        g.add_series(&EeUtilSeries::default());
        let mut h = EeUtilGraph::default();
        h.assign(&g);
        h.set_enabled(0, false);
        assert!(g.series(0).enabled);
        assert!(!h.series(0).enabled);
        h.remove_series(0);
        assert!(h.is_empty());
        assert_eq!(h.range, EeUtilRange::EMPTY);
    }
}
