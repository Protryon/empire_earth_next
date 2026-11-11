//! The path manager's path splicing (`src/Empire Earth/AI Thread/EEPathManager.cpp`), line by line: where a new
//! path is cut at the unit's tile (`TrimPath` 0x4a75f4), how the unit's old waypoints are appended to it
//! (`AppendWaypoints` 0x4a777f), where a detour joins it (`JoinPaths` 0x4a8bf4 and `FinishFailedPath`'s search for
//! the detour's start, 0x4a7fd3), the searches' step budgets (`SetGraphRevs` 0x4a6010, `SetPathRevs` 0x4a6157)
//! and the bounded searches' revolutions and bounds (`TryShortPath` 0x4a78c1, `FinishFailedPath`). The C++ keeps
//! the control flow, the machines, the path history and every call on the unit (`DropPath`, `TakePath`), and calls
//! these for the decisions and the point lists.
//!
//! Part of the deterministic game (RUST_PLAN 2.1): the points are compared by their tiles through `UMFloor`
//! (`lrintf`, then one down when that went up), `long` is `i32`, the loops walk as the C++'s.

use crate::sectors::lrintf;

/// A path point (`GE3DPoint`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EePathPoint {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

/// `UMFloor`: rounds to nearest, then steps down when that went up.
pub fn um_floor(value: f32) -> i32 {
    let result = lrintf(value);
    if (result as f32) > value {
        result.wrapping_sub(1)
    } else {
        result
    }
}

fn tile(p: &EePathPoint) -> (i32, i32) {
    (um_floor(p.x), um_floor(p.y))
}

/// `TrimPath`'s cut: the last index above `min(waypoints, path.len())` whose tile is the unit's (`unit_x`,
/// `unit_y` its location), searched from the end; the C++ erases the path from there and drops the unit's path.
pub fn trim_index(path: &[EePathPoint], waypoints: usize, unit_x: f32, unit_y: f32) -> Option<usize> {
    let first = waypoints.min(path.len());
    let unit = (um_floor(unit_x), um_floor(unit_y));
    (first + 1..path.len()).rev().find(|&i| tile(&path[i]) == unit)
}

/// `AppendWaypoints` after `TrimPath` did not cut: the unit's last waypoint when the path does not end on its
/// tile, then its other waypoints from the second last to the first. Returns whether the path is not empty (the
/// C++ then drops the unit's path).
pub fn append_waypoints(path: &mut Vec<EePathPoint>, waypoints: &[EePathPoint]) -> bool {
    if let (Some(last), Some(waypoint)) = (path.last(), waypoints.last()) {
        if tile(last) != tile(waypoint) {
            path.push(*waypoint);
        }
    }
    if waypoints.len() >= 2 {
        for i in (0..waypoints.len() - 1).rev() {
            path.push(waypoints[i]);
        }
    }
    !path.is_empty()
}

/// `JoinPaths`: when the detour starts on the tile of the point `count` from the path's end, the path up to that
/// point and then the detour; `None` otherwise (and for an empty detour). `count` is at least 1 and at most the
/// path's length at every call.
pub fn join_paths(path: &[EePathPoint], detour: &[EePathPoint], count: usize) -> Option<Vec<EePathPoint>> {
    let start = detour.first()?;
    let at = path.len().checked_sub(count)?;
    if tile(&path[at]) != tile(start) {
        return None;
    }
    let mut joined = Vec::with_capacity(path.len() + detour.len());
    joined.extend_from_slice(&path[..at]);
    joined.extend_from_slice(detour);
    Some(joined)
}

/// `FinishFailedPath`'s second join: the count from the path's end of the point among its first ten (searched
/// from the tenth down) on the detour's first tile, `None` without one (or without a detour).
pub fn detour_join_count(path: &[EePathPoint], detour: &[EePathPoint]) -> Option<usize> {
    let start = tile(detour.first()?);
    (0..path.len().min(10)).rev().find(|&i| tile(&path[i]) == start).map(|i| path.len() - i)
}

/// `TryShortPath`'s revolutions: 4 more than the queued searches (11 from 11 on), plus the Manhattan distance,
/// divided by 20 above 20 (unsigned, as the C++'s).
pub fn short_path_revs(queued: i32, from: (i32, i32), to: (i32, i32)) -> i32 {
    let mut distance = from.0.wrapping_sub(to.0).unsigned_abs().wrapping_add(from.1.wrapping_sub(to.1).unsigned_abs());
    if distance > 20 {
        distance /= 20;
    }
    let base = if queued > 10 { 11 } else { queued.wrapping_add(4) };
    base.wrapping_add(distance as i32)
}

/// `SetGraphRevs`' step budget: the distance times 250 (up to 15 tiles), 75 (up to 80), 90 (up to 160) or 110; at
/// most 40 per tile when the goal sub sector's kind is not allowed; 3 when the computer citizen asked for the same
/// tile within 4 s (`repeated`, from the path history the C++ walks).
pub fn graph_revs(distance: i32, kind_allowed: bool, repeated: bool) -> i32 {
    let mut factor = if distance <= 0xf {
        0xfa
    } else if distance <= 0x50 {
        0x4b
    } else if distance <= 0xa0 {
        0x5a
    } else {
        0x6e
    };
    if !kind_allowed && factor >= 0x28 {
        factor = 0x28;
    }
    if repeated {
        factor = 3;
    }
    distance.wrapping_mul(factor)
}

/// `SetPathRevs`' step budget: the distance times 375 (up to 15 tiles), 140, or 145 (over 80); at most 40 per tile
/// when a unit that is not amphibious goes to another continent.
pub fn path_revs(distance: i32, amphibious: bool, same_continent: bool) -> i32 {
    let mut factor = if distance <= 0xf {
        0x177
    } else if distance > 0x50 {
        0x8c + 5
    } else {
        0x8c
    };
    if !amphibious && !same_continent && factor >= 0x28 {
        factor = 0x28;
    }
    distance.wrapping_mul(factor)
}

/// A search's bounds (`EEAIAStarBounds`: top, left, bottom, right).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EePathBounds {
    pub top: i32,
    pub left: i32,
    pub bottom: i32,
    pub right: i32,
}

/// The bounds `margin` tiles around `from`, clipped to 0 and the map's last tile (`map_size - 1`): `TryShortPath`
/// with `revs + 1`, `FinishFailedPath` with 26.
pub fn bounds_around(from: (i32, i32), margin: i32, map_size: i32) -> EePathBounds {
    let last = map_size.wrapping_sub(1);
    let low = |v: i32| if v.wrapping_sub(margin) > 0 { v.wrapping_sub(margin) } else { 0 };
    let high = |v: i32| if v.wrapping_add(margin) < last { v.wrapping_add(margin) } else { last };
    EePathBounds { top: low(from.1), left: low(from.0), bottom: high(from.1), right: high(from.0) }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn p(x: f32, y: f32) -> EePathPoint {
        EePathPoint { x, y, z: 0.0 }
    }

    #[test]
    fn floor_follows_the_rounded_value() {
        assert_eq!(um_floor(2.5), 2);
        assert_eq!(um_floor(3.5), 3);
        assert_eq!(um_floor(2.999), 2);
        assert_eq!(um_floor(-0.5), -1);
        assert_eq!(um_floor(-1.0), -1);
        assert_eq!(um_floor(7.0), 7);
    }

    #[test]
    fn trim_cuts_at_the_last_point_on_the_unit_tile_above_the_waypoints() {
        let path = [p(1.5, 1.5), p(2.5, 2.5), p(3.5, 3.5), p(2.2, 2.9), p(5.5, 5.5)];
        assert_eq!(trim_index(&path, 0, 2.7, 2.1), Some(3));
        // index 3 is not above 3 waypoints
        assert_eq!(trim_index(&path, 3, 2.7, 2.1), None);
        assert_eq!(trim_index(&path, 1, 1.0, 1.0), None, "index 0 is never cut");
        assert_eq!(trim_index(&[], 0, 1.0, 1.0), None);
    }

    #[test]
    fn append_adds_the_last_waypoint_off_the_path_end_then_the_rest_backwards() {
        let mut path = vec![p(1.5, 1.5)];
        let waypoints = [p(7.5, 7.5), p(8.5, 8.5), p(9.5, 9.5)];
        assert!(append_waypoints(&mut path, &waypoints));
        assert_eq!(path, vec![p(1.5, 1.5), p(9.5, 9.5), p(8.5, 8.5), p(7.5, 7.5)]);
        // ending on the last waypoint's tile: not appended again
        let mut path = vec![p(9.1, 9.9)];
        append_waypoints(&mut path, &waypoints);
        assert_eq!(path, vec![p(9.1, 9.9), p(8.5, 8.5), p(7.5, 7.5)]);
        // an empty path takes the others only
        let mut empty = Vec::new();
        assert!(append_waypoints(&mut empty, &waypoints));
        assert_eq!(empty, vec![p(8.5, 8.5), p(7.5, 7.5)]);
        let mut none = Vec::new();
        assert!(!append_waypoints(&mut none, &[p(1.0, 1.0)]));
    }

    #[test]
    fn join_and_the_detour_count() {
        let path = [p(0.5, 0.5), p(1.5, 1.5), p(2.5, 2.5), p(3.5, 3.5)];
        let detour = [p(2.9, 2.1), p(4.5, 4.5)];
        assert_eq!(join_paths(&path, &detour, 2), Some(vec![p(0.5, 0.5), p(1.5, 1.5), p(2.9, 2.1), p(4.5, 4.5)]));
        assert_eq!(join_paths(&path, &detour, 1), None);
        assert_eq!(join_paths(&path, &[], 2), None);
        assert_eq!(detour_join_count(&path, &detour), Some(2));
        assert_eq!(detour_join_count(&path, &[p(9.0, 9.0)]), None);
        // only the first ten points are searched, from the tenth down
        let long: Vec<_> = (0..12).map(|i| p(i as f32 + 0.5, 0.5)).collect();
        assert_eq!(detour_join_count(&long, &[p(11.5, 0.5)]), None);
        assert_eq!(detour_join_count(&long, &[p(9.5, 0.5)]), Some(3));
    }

    #[test]
    fn step_budgets() {
        assert_eq!(graph_revs(15, true, false), 15 * 250);
        assert_eq!(graph_revs(16, true, false), 16 * 75);
        assert_eq!(graph_revs(160, true, false), 160 * 90);
        assert_eq!(graph_revs(161, true, false), 161 * 110);
        assert_eq!(graph_revs(20, false, false), 20 * 40);
        assert_eq!(graph_revs(20, false, true), 20 * 3);
        assert_eq!(path_revs(15, false, true), 15 * 375);
        assert_eq!(path_revs(80, true, false), 80 * 140);
        assert_eq!(path_revs(81, true, true), 81 * 145);
        assert_eq!(path_revs(81, false, false), 81 * 40);
        assert_eq!(path_revs(0, false, false), 0);
    }

    #[test]
    fn revs_and_bounds() {
        assert_eq!(short_path_revs(0, (5, 5), (8, 9)), 4 + 7);
        assert_eq!(short_path_revs(12, (0, 0), (30, 30)), 11 + 3);
        assert_eq!(short_path_revs(10, (0, 0), (20, 0)), 14 + 20);
        assert_eq!(bounds_around((3, 50), 26, 64), EePathBounds { top: 24, left: 0, bottom: 63, right: 29 });
        assert_eq!(bounds_around((30, 30), 26, 64), EePathBounds { top: 4, left: 4, bottom: 56, right: 56 });
        // exactly reaching an edge takes the edge
        assert_eq!(bounds_around((26, 37), 26, 64), EePathBounds { top: 11, left: 0, bottom: 63, right: 52 });
    }
}
