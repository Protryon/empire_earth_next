//! The engine's sparse point set (`U2DSparseArrayPointContainer`, `src/Low-Level Engine/Utilities/U2DSparseArray.cpp`,
//! LLE 0x100549b6..0x10056369), line by line: one sorted run list per x column, the columns in an array starting
//! at `offset`. The AI's areas (continents, path areas, hills, forests, choke points), the path sub sectors and the
//! explore goal's bad tiles hold their tiles in one. The C++ class keeps its name and vtable and owns a handle to
//! this (`ffi::EeSparseArray`); its column lists and their node pool are gone.
//!
//! Part of the deterministic game (RUST_PLAN 2.1): every list keeps the C++'s order (the point lists walk the
//! columns from the last to the first, each column's runs from the lowest), `long` is `i32` (wrapping, as the
//! Windows build's), and the random picks draw from the caller's `URandom` in the C++'s order and number, with
//! the C++'s float expression `(long)(Generate() * (max + 1 - min) + min)`.

use crate::continent_grid::EePoint;

/// One run of a column: the y values `start..=end` (`U2DSparseArrayPointContainerNode`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct Run {
    start: i32,
    end: i32,
}

impl Run {
    fn single(y: i32) -> Run {
        Run { start: y, end: y }
    }
    fn contains(&self, y: i32) -> bool {
        y >= self.start && y <= self.end
    }
    fn size(&self) -> i32 {
        self.end.wrapping_sub(self.start).wrapping_add(1)
    }
    fn is_empty(&self) -> bool {
        self.start > self.end
    }
    fn should_add(&self, y: i32) -> bool {
        y.wrapping_add(1) >= self.start && y <= self.end.wrapping_add(1)
    }
    fn add(&mut self, y: i32) {
        if y.wrapping_add(1) == self.start {
            self.start = y;
        } else if y == self.end.wrapping_add(1) {
            self.end = y;
        }
    }
    fn should_merge(&self, other: &Run) -> bool {
        self.start <= other.end.wrapping_add(1) && self.end.wrapping_add(1) >= other.start
    }
    fn merge(&mut self, other: &Run) {
        self.start = self.start.min(other.start);
        self.end = self.end.max(other.end);
    }
    /// `Remove`: shrinks the run at an end; false for a y inside it.
    fn remove(&mut self, y: i32) -> bool {
        if self.start == y && self.end == y {
            self.end = self.end.wrapping_sub(1);
            true
        } else if self.start == y {
            self.start = self.start.wrapping_add(1);
            true
        } else if self.end == y {
            self.end = self.end.wrapping_sub(1);
            true
        } else {
            false
        }
    }
}

/// One column (`U2DSparseArrayPointContainerListHeader`): its runs in order and its point count.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
struct Column {
    runs: Vec<Run>,
    size: i32,
}

/// `(long)(Generate() * (max + 1 - min) + min)`, `max` when that reaches `max + 1`: the C++'s
/// `GetRandomIntegerBetween`/`GetRandomNumberBetween` (one body), in float as the C++ evaluates it.
fn random_between(random: &mut dyn FnMut() -> f32, min: i32, max: i32) -> i32 {
    let limit = max.wrapping_add(1);
    let r = random();
    let result = (r * (limit.wrapping_sub(min) as f32) + min as f32) as i32;
    if result == limit {
        max
    } else {
        result
    }
}

impl Column {
    fn contains(&self, y: i32) -> bool {
        for run in &self.runs {
            if run.contains(y) {
                return true;
            }
            if run.start > y {
                break;
            }
        }
        false
    }

    /// `Insert` (the caller made sure the y is new).
    fn insert(&mut self, y: i32) {
        let mut i = 0;
        let mut inserted = false;
        while i < self.runs.len() {
            if self.runs[i].should_add(y) {
                self.runs[i].add(y);
                if i > 0 && self.runs[i - 1].should_merge(&self.runs[i]) {
                    let run = self.runs[i];
                    self.runs[i - 1].merge(&run);
                    self.runs.remove(i);
                    i -= 1;
                }
                if i + 1 < self.runs.len() && self.runs[i].should_merge(&self.runs[i + 1]) {
                    let next = self.runs[i + 1];
                    self.runs[i].merge(&next);
                    self.runs.remove(i + 1);
                }
                inserted = true;
                break;
            }
            if self.runs[i].start > y {
                self.runs.insert(i, Run::single(y));
                inserted = true;
                break;
            }
            i += 1;
        }
        if !inserted {
            self.runs.push(Run::single(y));
        }
        self.size = self.size.wrapping_add(1);
    }

    fn remove(&mut self, y: i32) -> bool {
        for i in 0..self.runs.len() {
            if self.runs[i].contains(y) {
                if self.runs[i].remove(y) {
                    if self.runs[i].is_empty() {
                        self.runs.remove(i);
                    }
                } else {
                    let start = self.runs[i].start;
                    self.runs.insert(i, Run { start, end: y.wrapping_sub(1) });
                    self.runs[i + 1].start = y.wrapping_add(1);
                }
                self.size = self.size.wrapping_sub(1);
                return true;
            }
        }
        false
    }

    fn element(&self, x: i32, index: i32) -> EePoint {
        let mut count = 0i32;
        for run in &self.runs {
            let next = count.wrapping_add(run.size());
            if index < next {
                return EePoint { x, y: run.start.wrapping_sub(count).wrapping_add(index) };
            }
            count = next;
        }
        EePoint::default()
    }

    fn random_point(&self, x: i32, random: &mut dyn FnMut() -> f32) -> EePoint {
        let index = random_between(random, 0, (self.runs.len() as i32).wrapping_sub(1));
        // the C++ walks min(index, count) nodes from the head; a column is never empty
        let run = self.runs[(index.max(0) as usize).min(self.runs.len() - 1)];
        EePoint { x, y: random_between(random, run.start, run.end) }
    }
}

/// The point set (`U2DSparseArrayPointContainer`).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct SparseArray {
    columns: Vec<Option<Column>>,
    offset: i32,
    count: i32,
}

/// Which point list `points` builds.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PointList {
    /// `BuildPointList(step)`: every `step`-th y of each run
    All(i32),
    /// `GetOnlyExtremeEdgePoints`: each column's lowest and highest point
    Extremes,
}

impl SparseArray {
    pub fn new() -> SparseArray {
        SparseArray::default()
    }

    /// `Size()`: the point count.
    pub fn size(&self) -> i32 {
        self.count
    }

    /// `Clear`.
    pub fn clear(&mut self) {
        self.columns.clear();
        self.offset = 0;
        self.count = 0;
    }

    fn column(&self, x: i32) -> Option<&Column> {
        let index = x.wrapping_sub(self.offset);
        if index >= 0 && (index as usize) < self.columns.len() {
            self.columns[index as usize].as_ref()
        } else {
            None
        }
    }

    /// `Contains`.
    pub fn contains(&self, p: EePoint) -> bool {
        self.column(p.x).is_some_and(|c| c.contains(p.y))
    }

    /// `ExpandArrayForPoint`: grows the column array to cover `x`.
    fn expand(&mut self, x: i32) {
        if !self.columns.is_empty() {
            let shift = self.offset.wrapping_sub(x);
            if shift > 0 {
                let mut columns = vec![None; self.columns.len() + shift as usize];
                for (i, c) in self.columns.drain(..).enumerate() {
                    columns[i + shift as usize] = c;
                }
                self.columns = columns;
                self.offset = x;
            } else if shift < 0 {
                let count = (x as i64 - self.offset as i64 - self.columns.len() as i64 + 1) as usize;
                self.columns.resize(self.columns.len() + count, None);
            }
        } else {
            self.offset = x;
            self.columns.resize(1, None);
        }
    }

    /// `InsertIntoBucket` with `Insert`'s count: adds the point to its column.
    fn insert_into_bucket(&mut self, index: usize, p: EePoint) {
        match &mut self.columns[index] {
            slot @ None => {
                let mut column = Column::default();
                column.insert(p.y);
                *slot = Some(column);
                self.count = self.count.wrapping_add(1);
            }
            Some(column) => {
                if !column.contains(p.y) {
                    column.insert(p.y);
                    self.count = self.count.wrapping_add(1);
                }
            }
        }
    }

    /// `Insert(point)`: a point left of the array with x <= 0 is ignored, as the C++ ignored it.
    pub fn insert(&mut self, p: EePoint) {
        let index = p.x.wrapping_sub(self.offset);
        if index < 0 && p.x <= 0 {
            return;
        }
        if index < 0 || index as usize >= self.columns.len() {
            self.expand(p.x);
            let index = p.x.wrapping_sub(self.offset) as usize;
            self.insert_into_bucket(index, p);
        } else {
            self.insert_into_bucket(index as usize, p);
        }
    }

    /// `Insert(list)`: the points from the last to the first.
    pub fn insert_all(&mut self, points: &[EePoint]) {
        for &p in points.iter().rev() {
            self.insert(p);
        }
    }

    /// `Create`: the set rebuilt from `points`, the column array sized from their x range.
    pub fn create(&mut self, points: &[EePoint]) {
        self.clear();
        let Some(&last) = points.last() else { return };
        let (mut min, mut max) = (last.x, last.x);
        for p in points.iter().rev().skip(1) {
            if p.x < min {
                min = p.x;
            }
            if p.x > max {
                max = p.x;
            }
        }
        self.offset = min;
        self.columns.resize((max as i64 - min as i64 + 1) as usize, None);
        self.insert_all(points);
    }

    /// `Remove`: deletes the column it empties; false when the point was not there.
    pub fn remove(&mut self, p: EePoint) -> bool {
        let index = p.x.wrapping_sub(self.offset);
        if index < 0 || index as usize >= self.columns.len() {
            return false;
        }
        let slot = &mut self.columns[index as usize];
        let Some(column) = slot else { return false };
        if !column.remove(p.y) {
            return false;
        }
        self.count = self.count.wrapping_sub(1);
        if column.runs.is_empty() {
            *slot = None;
        }
        true
    }

    /// `BuildPointList` / `GetOnlyExtremeEdgePoints` into `out` (cleared first): the columns from the last.
    pub fn points(&self, list: PointList, out: &mut Vec<EePoint>) {
        out.clear();
        for i in (0..self.columns.len()).rev() {
            let Some(column) = &self.columns[i] else { continue };
            let x = self.offset.wrapping_add(i as i32);
            match list {
                PointList::All(step) => {
                    for run in &column.runs {
                        let mut y = run.start;
                        while y <= run.end {
                            out.push(EePoint { x, y });
                            y = y.wrapping_add(step);
                        }
                    }
                }
                PointList::Extremes => {
                    if let (Some(first), Some(last)) = (column.runs.first(), column.runs.last()) {
                        out.push(EePoint { x, y: first.start });
                        out.push(EePoint { x, y: last.end });
                    }
                }
            }
        }
    }

    /// `operator[]`: the point at `index` over the columns from the first, (0, 0) out of range.
    pub fn get(&self, index: i32) -> EePoint {
        let mut count = 0i32;
        for (i, column) in self.columns.iter().enumerate() {
            let Some(column) = column else { continue };
            if index < count.wrapping_add(column.size) {
                return column.element(self.offset.wrapping_add(i as i32), index.wrapping_sub(count));
            }
            count = count.wrapping_add(column.size);
        }
        EePoint::default()
    }

    /// `CountContainerNodes`: the runs over all columns.
    pub fn run_count(&self) -> i32 {
        self.columns.iter().flatten().map(|c| c.runs.len() as i32).fold(0, i32::wrapping_add)
    }

    /// `GetClosestPoint`: the point itself when it is in the set (or the set is empty), else the set's point at the
    /// least squared distance, the last of the point list's ties (walked from its end, `<=`).
    pub fn closest_point(&self, p: EePoint) -> EePoint {
        if self.count <= 0 || self.contains(p) {
            return p;
        }
        let mut points = Vec::new();
        self.points(PointList::All(1), &mut points);
        let distance = |q: EePoint| {
            let (dx, dy) = (p.x.wrapping_sub(q.x), p.y.wrapping_sub(q.y));
            dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy))
        };
        let Some(&last) = points.last() else { return p };
        let mut result = last;
        let mut best = distance(result);
        for &q in points.iter().rev().skip(1) {
            let d = distance(q);
            if d <= best {
                result = q;
                best = d;
            }
        }
        result
    }

    /// `GetRandomPoint`: up to 16 random columns until one exists, then a random run and y in it; after 16 misses
    /// the first point; (0, 0) for no columns.
    pub fn random_point(&self, random: &mut dyn FnMut() -> f32) -> EePoint {
        if self.columns.is_empty() {
            return EePoint::default();
        }
        for _ in 0..16 {
            let index = random_between(random, 0, (self.columns.len() as i32).wrapping_sub(1));
            if index >= 0 && (index as usize) < self.columns.len() {
                if let Some(column) = &self.columns[index as usize] {
                    return column.random_point(self.offset.wrapping_add(index), random);
                }
            }
        }
        self.get(0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn pt(x: i32, y: i32) -> EePoint {
        EePoint { x, y }
    }

    fn runs(s: &SparseArray, x: i32) -> Vec<(i32, i32)> {
        s.column(x).map(|c| c.runs.iter().map(|r| (r.start, r.end)).collect()).unwrap_or_default()
    }

    #[test]
    fn insert_merges_runs_in_order() {
        let mut s = SparseArray::new();
        for y in [5, 7, 3, 6, 4, 10] {
            s.insert(pt(2, y));
        }
        assert_eq!(runs(&s, 2), vec![(3, 7), (10, 10)]);
        assert_eq!(s.size(), 6);
        s.insert(pt(2, 6));
        assert_eq!(s.size(), 6, "a duplicate leaves the count");
        assert_eq!(s.run_count(), 2);
    }

    #[test]
    fn insert_left_of_zero_is_ignored() {
        let mut s = SparseArray::new();
        s.insert(pt(5, 1));
        s.insert(pt(0, 1));
        s.insert(pt(-2, 1));
        assert_eq!(s.size(), 1);
        assert!(!s.contains(pt(0, 1)));
        s.insert(pt(3, 1));
        assert_eq!(s.offset, 3);
        assert_eq!(s.columns.len(), 3);
        s.insert(pt(9, 0));
        assert_eq!(s.columns.len(), 7);
        // an empty set takes x = 0
        let mut e = SparseArray::new();
        e.insert(pt(0, 4));
        assert!(e.contains(pt(0, 4)));
    }

    #[test]
    fn remove_splits_shrinks_and_drops_columns() {
        let mut s = SparseArray::new();
        s.create(&[pt(1, 1), pt(1, 2), pt(1, 3), pt(1, 4), pt(3, 9)]);
        assert!(s.remove(pt(1, 3)));
        assert_eq!(runs(&s, 1), vec![(1, 2), (4, 4)]);
        assert!(s.remove(pt(1, 4)));
        assert!(s.remove(pt(1, 1)));
        assert_eq!(runs(&s, 1), vec![(2, 2)]);
        assert!(!s.remove(pt(1, 7)));
        assert!(s.remove(pt(3, 9)));
        assert!(s.column(3).is_none());
        assert_eq!(s.columns.len(), 3, "the array keeps its size");
        assert_eq!(s.size(), 1);
    }

    #[test]
    fn point_lists_walk_columns_from_the_last() {
        let mut s = SparseArray::new();
        s.create(&[pt(4, 1), pt(2, 5), pt(2, 6), pt(2, 9), pt(4, 2), pt(4, 3)]);
        let mut out = Vec::new();
        s.points(PointList::All(1), &mut out);
        assert_eq!(out, vec![pt(4, 1), pt(4, 2), pt(4, 3), pt(2, 5), pt(2, 6), pt(2, 9)]);
        s.points(PointList::All(2), &mut out);
        assert_eq!(out, vec![pt(4, 1), pt(4, 3), pt(2, 5), pt(2, 9)]);
        s.points(PointList::Extremes, &mut out);
        assert_eq!(out, vec![pt(4, 1), pt(4, 3), pt(2, 5), pt(2, 9)]);
        // operator[] walks from the first column
        assert_eq!(
            (0..6).map(|i| s.get(i)).collect::<Vec<_>>(),
            vec![pt(2, 5), pt(2, 6), pt(2, 9), pt(4, 1), pt(4, 2), pt(4, 3)]
        );
        assert_eq!(s.get(6), pt(0, 0));
        // a single-run column gives its run's ends
        let mut one = SparseArray::new();
        one.insert(pt(1, 7));
        one.points(PointList::Extremes, &mut out);
        assert_eq!(out, vec![pt(1, 7), pt(1, 7)]);
    }

    #[test]
    fn closest_point_keeps_the_last_tie_from_the_end() {
        let mut s = SparseArray::new();
        s.create(&[pt(1, 1), pt(3, 1)]);
        // list: (3,1), (1,1); from the end: (1,1) first, then (3,1) ties with <=
        assert_eq!(s.closest_point(pt(2, 1)), pt(3, 1));
        assert_eq!(s.closest_point(pt(1, 1)), pt(1, 1));
        assert_eq!(SparseArray::new().closest_point(pt(5, 5)), pt(5, 5));
    }

    #[test]
    fn random_point_draws_as_the_cpp() {
        let mut s = SparseArray::new();
        s.create(&[pt(2, 1), pt(2, 2), pt(2, 6), pt(5, 3)]);
        // columns 2..=5, index (long)(r * 4): 0.3 -> x 3 (no column), 0.75 -> x 5; its one run: (long)(0.9 * 1),
        // then y = (long)(0.5 * 1 + 3)
        let mut draws = vec![0.3f32, 0.75, 0.9, 0.5].into_iter();
        let mut next = || draws.next().unwrap();
        assert_eq!(s.random_point(&mut next), pt(5, 3));
        assert!(draws.next().is_none(), "four draws");
        // column x 2 has runs (1,2) and (6,6): 0.1 -> x 2, run (long)(0.6 * 2) = 1, y 6
        let mut draws = vec![0.1f32, 0.6, 0.2].into_iter();
        let mut next = || draws.next().unwrap();
        assert_eq!(s.random_point(&mut next), pt(2, 6));
        // all misses: 16 draws, then the first point
        let mut gaps = SparseArray::new();
        gaps.create(&[pt(1, 4), pt(9, 2)]);
        let mut n = 0;
        let mut miss = || {
            n += 1;
            0.5
        };
        assert_eq!(gaps.random_point(&mut miss), pt(1, 4));
        assert_eq!(n, 16);
        // r near 1 reaching max + 1 gives max
        let mut top = || 0.999_999_9f32;
        assert_eq!(random_between(&mut top, 0, 3), 3);
        let mut empty_draws = || panic!("no draw for an empty set");
        assert_eq!(SparseArray::new().random_point(&mut empty_draws), pt(0, 0));
    }
}
