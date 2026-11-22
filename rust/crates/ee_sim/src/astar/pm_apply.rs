//! The path manager's host operations on a unit, its goals and a request (the paths lane's round 1; were
//! EEPathManager.cpp's host cases and `RequestUnitPaths`): a found graph path given to the unit (location goals) or
//! to the request (its tiles and length), the request count, the path-done flag, the capture's unit facts and
//! `ClampTile`. The C++ calls they make are one-statement shims: the units' `ee_units_wp_host` (waypoints.rs), the
//! request's path and the map's closest tile EEPathManager.cpp's host cases below.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

// (170 was the request's path set: request.rs)
/// map `h1`'s `GetClosestTile` of the point of float bits (`i0`, `i1`) -> (`i0`, `i1`)
pub const EE_PATH_CLOSEST_TILE: i32 = 171;

/// `kGoalLocation`, `kGoalAttackUnit`; `kActionPrepareToMove`
#[cfg_attr(test, allow(dead_code))]
const GOAL_LOCATION: u8 = 1;
#[cfg_attr(test, allow(dead_code))]
const GOAL_ATTACK_UNIT: u8 = 2;
#[cfg_attr(test, allow(dead_code))]
const ACTION_PREPARE_TO_MOVE: i64 = 8000;

/// A found graph path's request part: the tiles from the goal's end (the path's last sub sector first) and the
/// summed distance (`UMAbs` of each tile's offsets from the last, the start's offsets from it first).
pub fn request_path(locations: &[(i32, i32)], from: Option<(i32, i32)>) -> (Vec<(i32, i32)>, i32) {
    let mut tiles = Vec::new();
    let mut distance = 0i32;
    let mut last = (0, 0);
    if let Some(&l) = locations.last() {
        last = l;
        tiles.push(l);
        if let Some(f) = from {
            distance = f.0.wrapping_sub(l.0).wrapping_abs().wrapping_add(f.1.wrapping_sub(l.1).wrapping_abs());
        }
    }
    let n = locations.len() as i64;
    let mut i = n - 2;
    while i >= 0 {
        let t = locations[i as usize];
        tiles.push(t);
        distance = distance.wrapping_add(
            t.0.wrapping_sub(last.0).wrapping_abs().wrapping_add(t.1.wrapping_sub(last.1).wrapping_abs()),
        );
        i -= 1;
    }
    (tiles, distance)
}

/// How many location goals `RequestUnitPaths` gives a unit for a path of `n` sub sectors (`(long)n - 2`, none below).
pub fn unit_goal_count(n: usize) -> usize {
    (n as i64 - 2).max(0) as usize
}

#[cfg(not(test))]
pub use game::*;

#[cfg(not(test))]
mod game {
    use std::os::raw::c_long;

    use super::*;
    use crate::astar::cm::EeAStarCmTable;
    use crate::astar::pm::EePathIo;
    use crate::units::goal_obj;
    use crate::units::unit_fields::EeUnitsComplexUnitFields;
    use crate::units::waypoints as wp;
    use crate::world::{MapView, UnitView, WorldRead, WorldView};

    type Raw<'a> = &'a mut dyn FnMut(i32, &mut EePathIo) -> i64;

    fn token() -> WorldRead {
        // SAFETY: the manager's calls run on the simulation thread, which holds the world
        unsafe { WorldRead::begin() }
    }

    /// Sub sector `id`'s location (`GetPathSubSector(id)->GetLocation()`).
    fn sub_location(id: i32) -> (i32, i32) {
        let r = crate::astar::world_view::cm().expect("the continent manager");
        let h = r.tables().tables[EeAStarCmTable::PathSubSectors as usize][id as u16 as usize];
        // SAFETY: a listed sub sector is a live object
        let l = unsafe { crate::astar::cm_io::sub(h).location };
        (l.x as i32, l.y as i32)
    }

    fn goal_of(unit: u64) -> u64 {
        let t = token();
        // SAFETY: a live complex unit
        unsafe { UnitView::of(&t, unit) }.map_or(0, |u| u.goal_handle())
    }

    /// `EEPathManager::RequestUnitPaths(unit, request, path, from)`: the found graph path's sub sector ids `path` (the
    /// goal's first) given to the unit as location goals (before a pending attack, or replacing its goal), or to the
    /// request as tiles.
    ///
    /// # Safety
    /// `unit` 0 or a live complex unit; `request` 0 or a live request; `from` 0 or a live sub sector.
    pub unsafe fn request_unit_paths(unit: u64, request: u64, path: &[i32], from: u64) {
        if unit != 0 {
            let goal = goal_of(unit);
            // SAFETY: the unit's live goal (each read below of a live goal)
            let kind = if goal != 0 { unsafe { goal_obj::base(goal) }.kind } else { 0 };
            let location = goal != 0 && kind == GOAL_LOCATION;
            if goal != 0 {
                unsafe { goal_obj::base(goal) }.b1d = true;
            }
            let forced = !location || crate::units::location::get(goal).forced_move;
            let queued = wp::host(wp::EE_UNITS_WP_QUEUE_LENGTH, unit, 0);
            let attack = !location && goal != 0 && kind == GOAL_ATTACK_UNIT && !unsafe { goal_obj::base(goal) }.b1c;
            if attack {
                if queued <= 1 {
                    return;
                }
            } else {
                if queued == 0 {
                    return;
                }
                let back = wp::host(wp::EE_UNITS_WP_QUEUE_BACK, unit, 0) as u64;
                if unsafe { goal_obj::base(back) }.kind == GOAL_LOCATION
                    && crate::units::location::get(back).auto_waypoint
                {
                    return;
                }
            }
            let on_tile = {
                let t = token();
                unsafe { UnitView::of(&t, unit) }.is_some_and(|u| u.tile().is_some())
            };
            if path.len() > 1 && on_tile {
                let mut set = false;
                for &id in &path[..unit_goal_count(path.len())] {
                    let tile = sub_location(id);
                    let current = goal_of(unit);
                    let arg = current != 0 && unsafe { goal_obj::base(current) }.b1c;
                    let g = wp::new_auto_waypoint([tile.0 as f32 + 0.5, tile.1 as f32 + 0.5, 0.0], arg, forced);
                    if !attack {
                        wp::host(wp::EE_UNITS_WP_SET_GOAL, unit, g);
                        set = true;
                    } else {
                        wp::host(wp::EE_UNITS_WP_INSERT_GOAL, unit, g);
                        wp::host(wp::EE_UNITS_WP_REFRESH_GOALS, unit, 0);
                        wp::host(wp::EE_UNITS_WP_REFRESH_ACTIVE_GOAL, unit, 0);
                    }
                }
                wp::host(wp::EE_UNITS_WP_REFRESH_GOALS, unit, 0);
                wp::host(wp::EE_UNITS_WP_REFRESH_ACTIVE_GOAL, unit, 0);
                if set {
                    wp::host(wp::EE_UNITS_WP_START_ACTION, unit, ACTION_PREPARE_TO_MOVE as u64);
                    wp::host(wp::EE_UNITS_WP_ACTION_CHANGED, unit, 0);
                }
            }
        } else if request != 0 {
            let locations: Vec<(i32, i32)> = path.iter().map(|&id| sub_location(id)).collect();
            let start = (from != 0).then(|| {
                // SAFETY: the caller's contract
                let l = unsafe { crate::astar::cm_io::sub(from).location };
                (l.x as i32, l.y as i32)
            });
            let (tiles, length) = request_path(&locations, start);
            // SAFETY: the caller's live request
            unsafe { crate::astar::request::set_path(request, &tiles, length) };
        }
    }

    fn complex_mut(u: &UnitView<'_>) -> *mut EeUnitsComplexUnitFields {
        u.complex().ptr() as *mut EeUnitsComplexUnitFields
    }

    /// `EE_PATH_UNIT_COUNT_REQUEST`: the unit's request counted; whether it goes into the history (a computer
    /// player's or player 0's, not detached from the AI, not a part).
    ///
    /// # Safety
    /// `unit` a live complex unit.
    pub unsafe fn count_request(unit: u64) -> i64 {
        {
            let t = token();
            let Some(u) = (unsafe { UnitView::of(&t, unit) }) else { return 0 };
            let c = complex_mut(&u);
            // SAFETY: the unit's own record
            unsafe { (*c).mNumPathRequests = (*c).mNumPathRequests.wrapping_add(1) };
        }
        let human = wp::host(wp::EE_UNITS_WP_IS_HUMAN, unit, 0) != 0;
        let t = token();
        let Some(u) = (unsafe { UnitView::of(&t, unit) }) else { return 0 };
        let player_zero = u.player().is_some_and(|p| p.id() == 0);
        let detached = crate::world_field!(u.complex(), mIsDetachedFromAI);
        i64::from((!human || player_zero) && !detached && u.previous_part_handle() == 0)
    }

    /// `EE_PATH_UNIT_PATH_DONE`: the path-done flag set, the bounded tiles dropped.
    ///
    /// # Safety
    /// `unit` a live complex unit.
    pub unsafe fn path_done(unit: u64) {
        {
            let t = token();
            if let Some(u) = unsafe { UnitView::of(&t, unit) } {
                // SAFETY: the unit's own record
                unsafe { (*complex_mut(&u)).mIsPathDone = true };
            }
        }
        wp::host(wp::EE_UNITS_WP_CLEAR_BOUNDED, unit, 0);
    }

    /// `EE_PATH_UNIT_CAPTURE` (TEST ONLY): whether the goal is a location goal and its place's float bits in
    /// `io.i[1..3]`; the first goal queue's length.
    ///
    /// # Safety
    /// `unit` a live complex unit.
    pub unsafe fn capture_facts(unit: u64, io: &mut EePathIo) -> i64 {
        let goal = goal_of(unit);
        let location = goal != 0 && unsafe { goal_obj::base(goal) }.kind == GOAL_LOCATION;
        io.i[0] = i64::from(location);
        if location {
            let at = crate::units::location::get(goal).location;
            io.i[1] = i64::from(at[0].to_bits());
            io.i[2] = i64::from(at[1].to_bits());
        }
        wp::host(wp::EE_UNITS_WP_QUEUE_LENGTH, unit, 0)
    }

    /// `ClampTile(map, tile)`: a tile off the map moved to the map's tile closest to its center (`map` 0: the
    /// world's).
    #[allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64
    pub fn clamp(raw: Raw<'_>, map: u64, x: i64, y: i64) -> (i64, i64) {
        let (map, on) = {
            let t = token();
            let map =
                if map != 0 { map } else { WorldView::global(&t).and_then(|w| w.map()).map_or(0, |m| m.handle()) };
            // SAFETY: the live map of the operation
            (map, unsafe { MapView::of(&t, map) }.is_some_and(|m| m.tile(x, y).is_some()))
        };
        if on {
            return (x, y);
        }
        let (fx, fy) = ((x as c_long) as f32 + 0.5, (y as c_long) as f32 + 0.5);
        // the point (x, y, 0) as a GE3DPoint in i2.. (three floats); the C++ writes the tile's `long`s at i0, i1
        let mut io = EePathIo::default();
        io.h[1] = map;
        io.i[2] = i64::from(fx.to_bits()) | (i64::from(fy.to_bits()) << 32);
        raw(EE_PATH_CLOSEST_TILE, &mut io);
        // SAFETY: the C++ wrote a `long` at the start of each of i0 and i1
        unsafe {
            let at = io.i.as_ptr();
            (*(at as *const std::ffi::c_long) as i64, *(at.add(1) as *const std::ffi::c_long) as i64)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_request_path_is_last_first_with_the_distances_from_the_last() {
        // the ids' locations, the goal's first: the path runs from the last back to the first
        let l = [(10, 10), (7, 9), (3, 2)];
        let (tiles, d) = request_path(&l, Some((1, 1)));
        assert_eq!(tiles, vec![(3, 2), (7, 9), (10, 10)]);
        // |1-3|+|1-2| = 3, then each from the last (3, 2): (4 + 7) + (7 + 8)
        assert_eq!(d, 3 + 11 + 15);
        let (tiles, d) = request_path(&l, None);
        assert_eq!((tiles.len(), d), (3, 26));
        assert_eq!(request_path(&[], Some((5, 5))), (vec![], 0));
        assert_eq!(request_path(&[(4, 4)], Some((5, 6))), (vec![(4, 4)], 3));
    }

    #[test]
    fn a_unit_gets_a_goal_per_sub_sector_but_the_last_two() {
        assert_eq!(unit_goal_count(0), 0);
        assert_eq!(unit_goal_count(1), 0);
        assert_eq!(unit_goal_count(2), 0);
        assert_eq!(unit_goal_count(5), 3);
    }
}
