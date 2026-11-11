//! The continent manager's last loops over its objects, line by line, with the objects behind a host the C++
//! implements: `GetTilesNextTo` (0x422f5f, with `EEGetSquareRingTiles` 0x6492c2's ring order), `PruneChokePoints`
//! (0x424cc0: a choke point touching the map edge whose areas all touch it too goes), and `CreateScenarioAreas`'
//! choice of the scenario areas and their tile rectangles (0x4214bf). The geometry the prune asks (the areas'
//! closest points, on `U2DSparseArray`/`U2DPolygon<long>`) and the objects it deletes stay C++.
//!
//! Part of the deterministic game (RUST_PLAN 2.1): the C++'s loop orders (from the last index down), its
//! questions in its order (the host may answer from the live objects), `long` as `i32`, the float rectangle's
//! sides truncated as the C++'s `(long)` casts.

use crate::continent_grid::EePoint;

/// `EEGetSquareRingTiles(x, y, radius)`: the row at `y + radius` from the left, then the sides from `y + radius - 1`
/// down (the left tile, then the right one), then the row at `y - radius` from the left.
pub fn square_ring(x: i32, y: i32, radius: i32) -> Vec<EePoint> {
    let side = radius.wrapping_mul(2).wrapping_add(1);
    let inner = radius.wrapping_mul(2).wrapping_sub(1);
    let left = x.wrapping_sub(radius);
    let mut out = Vec::with_capacity((inner.wrapping_add(side).max(0) as usize) * 2);
    for i in 0..side {
        out.push(EePoint { x: i.wrapping_add(left), y: y.wrapping_add(radius) });
    }
    let mut row = y.wrapping_sub(1).wrapping_add(radius);
    for _ in 0..inner.max(0) {
        out.push(EePoint { x: left, y: row });
        out.push(EePoint { x: x.wrapping_add(radius), y: row });
        row = row.wrapping_sub(1);
    }
    for i in 0..side {
        out.push(EePoint { x: left.wrapping_add(i), y: y.wrapping_sub(radius) });
    }
    out
}

/// `GetTilesNextTo(x, y, id)`: true when the tile is on continent `id`; else, when its continent borders `id`
/// (`flags[flags_width * its id + id]`), the ring's tiles on the map and on `id` (from the ring's last tile) and
/// whether there are any. `continent(x, y)` is the manager's `GetContinentID`.
#[allow(clippy::too_many_arguments)] // the C++'s inputs, as the FFI hands them
pub fn tiles_next_to(
    x: i32,
    y: i32,
    id: u16,
    width: u32,
    height: u32,
    flags: &[bool],
    flags_width: i32,
    continent: &mut dyn FnMut(i32, i32) -> u16,
    out: &mut Vec<EePoint>,
) -> bool {
    out.clear();
    let own = continent(x, y);
    if own == id {
        return true;
    }
    let index = flags_width.wrapping_mul(own as i32).wrapping_add(id as i32);
    if index < 0 || !flags.get(index as usize).copied().unwrap_or(false) {
        return false;
    }
    for t in square_ring(x, y, 1).into_iter().rev() {
        if t.x < 0 || t.y < 0 {
            continue;
        }
        if (t.x as u32) < width && (t.y as u32) < height && continent(t.x, t.y) == id {
            out.push(t);
        }
    }
    !out.is_empty()
}

/// `kContinentCategoryMapEdge`.
pub const CATEGORY_MAP_EDGE: i32 = 5;

/// What `PruneChokePoints` asks of the manager's objects.
pub trait PruneHost {
    /// Whether choke point `choke` (an index in the manager's list) exists.
    fn choke_exists(&mut self, choke: usize) -> bool;
    /// The size of its `mAreaSerials`.
    fn area_count(&mut self, choke: usize) -> usize;
    /// Its `k`th area (`GetArea(serial)`): -2 for none, the continent's index when the area is one of the manager's
    /// continents, else -1.
    fn area(&mut self, choke: usize, k: usize) -> i32;
    /// Whether map edge continent `edge` (an index) comes within one tile of the choke point (`k` None) or of its
    /// `k`th area: `GetClosestPoints`, then the squared distance at most 1.
    fn near(&mut self, edge: usize, choke: usize, k: Option<usize>) -> bool;
    /// Removes and deletes choke point `choke` (`RemoveArea`, the list's slot cleared).
    fn remove(&mut self, choke: usize);
}

/// `PruneChokePoints`: `categories` the continents' categories by index (-1 for none), `chokes` the choke point
/// list's length. A choke point that a map edge continent touches goes when its areas touch the map edge
/// continents (other than themselves) at least `areas - 1` times and at least once.
pub fn prune_choke_points(categories: &[i32], chokes: usize, host: &mut dyn PruneHost) {
    let edges: Vec<usize> = (0..categories.len()).rev().filter(|&i| categories[i] == CATEGORY_MAP_EDGE).collect();
    for j in (0..chokes).rev() {
        if !host.choke_exists(j) {
            continue;
        }
        for i in (0..edges.len()).rev() {
            if !host.near(edges[i], j, None) {
                continue;
            }
            let areas = host.area_count(j);
            let mut count: i64 = 0;
            for k in (0..areas).rev() {
                let area = host.area(j, k);
                if area == -2 {
                    continue;
                }
                for n in (0..edges.len()).rev() {
                    if area >= 0 && area as usize == edges[n] {
                        continue;
                    }
                    if host.near(edges[n], j, Some(k)) {
                        count += 1;
                    }
                }
            }
            // `theCount >= mAreaSerials.size() - 1`: unsigned, so no areas never prunes (and count is 0 then)
            if count > 0 && count >= areas as i64 - 1 {
                host.remove(j);
            }
            break;
        }
    }
}

/// A scenario area as `CreateScenarioAreas` reads it: whether it exists and is set, and its rectangle's sides.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScenarioArea {
    pub set: bool,
    pub top: f32,
    pub bottom: f32,
    pub left: f32,
    pub right: f32,
    pub height: f32,
    pub width: f32,
}

/// One rectangle area to create: the scenario area's index, its id, its tile rectangle.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScenarioAreaPlan {
    pub index: i32,
    pub id: u16,
    pub top: i32,
    pub bottom: i32,
    pub left: i32,
    pub right: i32,
}

/// `CreateScenarioAreas`' choice: the set areas with a positive height and width, from the last; ids from 1. The
/// C++ sizes its list to their count + 1 and creates them in this order.
pub fn scenario_area_plan(areas: &[EeScenarioArea]) -> Vec<EeScenarioAreaPlan> {
    let mut id: u16 = 1;
    let mut out = Vec::new();
    for i in (0..areas.len()).rev() {
        let a = &areas[i];
        // the C++'s `!(h > 0)`: a NaN side skips the area too
        #[allow(clippy::neg_cmp_op_on_partial_ord)]
        let skip = !a.set || !(a.height > 0.0) || !(a.width > 0.0);
        if skip {
            continue;
        }
        out.push(EeScenarioAreaPlan {
            index: i as i32,
            id,
            top: a.top as i32,
            bottom: a.bottom as i32,
            left: a.left as i32,
            right: a.right as i32,
        });
        id = id.wrapping_add(1);
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn pt(x: i32, y: i32) -> EePoint {
        EePoint { x, y }
    }

    #[test]
    fn ring_order() {
        assert_eq!(
            square_ring(5, 5, 1),
            vec![pt(4, 6), pt(5, 6), pt(6, 6), pt(4, 5), pt(6, 5), pt(4, 4), pt(5, 4), pt(6, 4)]
        );
        let r2 = square_ring(0, 0, 2);
        assert_eq!(r2.len(), 16);
        assert_eq!(r2[5], pt(-2, 1));
        assert_eq!(r2[6], pt(2, 1));
        assert_eq!(r2[15], pt(2, -2));
    }

    #[test]
    fn next_to_walks_the_ring_backwards() {
        // a 4x4 map: continent 1 on x < 2, continent 2 on x >= 2; 1 and 2 adjacent, 3 adjacent to nothing
        let mut ids = |x: i32, y: i32| {
            if x as u32 >= 4 || y as u32 >= 4 {
                0
            } else if x < 2 {
                1
            } else {
                2
            }
        };
        let w = 4;
        let mut flags = vec![false; w * w];
        flags[w + 2] = true;
        flags[2 * w + 1] = true;
        let mut out = Vec::new();
        assert!(tiles_next_to(1, 1, 1, 4, 4, &flags, w as i32, &mut ids, &mut out));
        assert!(out.is_empty());
        assert!(tiles_next_to(2, 1, 1, 4, 4, &flags, w as i32, &mut ids, &mut out));
        assert_eq!(out, vec![pt(1, 0), pt(1, 1), pt(1, 2)]);
        // off-map ring tiles are skipped
        assert!(tiles_next_to(2, 0, 1, 4, 4, &flags, w as i32, &mut ids, &mut out));
        assert_eq!(out, vec![pt(1, 0), pt(1, 1)]);
        // not adjacent: nothing, false
        assert!(!tiles_next_to(2, 1, 3, 4, 4, &flags, w as i32, &mut ids, &mut out));
        // adjacent continents but not this tile
        assert!(!tiles_next_to(3, 1, 1, 4, 4, &flags, w as i32, &mut ids, &mut out));
    }

    struct Host {
        exists: Vec<bool>,
        areas: Vec<Vec<i32>>,
        near_choke: Vec<(usize, usize)>,
        near_area: Vec<(usize, usize, usize)>,
        log: Vec<String>,
    }

    impl PruneHost for Host {
        fn choke_exists(&mut self, choke: usize) -> bool {
            self.exists[choke]
        }
        fn area_count(&mut self, choke: usize) -> usize {
            self.areas[choke].len()
        }
        fn area(&mut self, choke: usize, k: usize) -> i32 {
            self.areas[choke][k]
        }
        fn near(&mut self, edge: usize, choke: usize, k: Option<usize>) -> bool {
            self.log.push(format!("near {edge} {choke} {k:?}"));
            match k {
                None => self.near_choke.contains(&(edge, choke)),
                Some(k) => self.near_area.contains(&(edge, choke, k)),
            }
        }
        fn remove(&mut self, choke: usize) {
            self.log.push(format!("remove {choke}"));
        }
    }

    #[test]
    fn prune_asks_in_the_cpp_order_and_removes() {
        // continents 0 (land), 1 and 3 map edges, 2 none
        let categories = [1, CATEGORY_MAP_EDGE, -1, CATEGORY_MAP_EDGE];
        let mut host = Host {
            exists: vec![true, false, true],
            // choke 0: areas continent 0 and edge 3 (the latter skipped against itself); choke 2: one area
            areas: vec![vec![0, 3], vec![], vec![-2]],
            near_choke: vec![(1, 0), (3, 2)],
            near_area: vec![(1, 0, 0)],
            log: Vec::new(),
        };
        prune_choke_points(&categories, 3, &mut host);
        assert_eq!(
            host.log,
            vec![
                // choke 2: edges walked from the end of [3, 1]: edge 1 first
                "near 1 2 None",
                "near 3 2 None",
                // its one area is missing: count 0, kept
                // choke 0: edge 1 is near; areas from the last (edge 3: only edge 1 asked), then area 0
                "near 1 0 None",
                "near 1 0 Some(1)",
                "near 1 0 Some(0)",
                "near 3 0 Some(0)",
                "remove 0",
            ]
        );
    }

    #[test]
    fn scenario_areas_from_the_last() {
        let a = |set, side: f32| EeScenarioArea {
            set,
            top: 1.9,
            bottom: side,
            left: 2.0,
            right: side + 0.5,
            height: side - 1.9,
            width: side - 1.5,
        };
        let plan = scenario_area_plan(&[a(true, 5.0), a(false, 5.0), a(true, 1.0), a(true, 4.2)]);
        assert_eq!(
            plan,
            vec![
                EeScenarioAreaPlan { index: 3, id: 1, top: 1, bottom: 4, left: 2, right: 4 },
                EeScenarioAreaPlan { index: 0, id: 2, top: 1, bottom: 5, left: 2, right: 5 },
            ]
        );
    }
}
