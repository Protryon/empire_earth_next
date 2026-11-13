//! The explore goal whole in Rust (RUST_PLAN R5b.4, the seventeenth round): `EEUGExplore`'s state (the bad tiles,
//! the tile being explored, the search radius, the two no-areas flags) and every body (`Step` with the next tile's
//! search, the triggers, the checksum, the texts, `Save`, `Load`) by the goal's object. The C++ class is gone: an
//! explore goal is an `EEUGRust` of kind `kGoalExplore` (ee/EEUGRust.h) over the Rust goals' host (rust_goal.rs).
//!
//! The bad tiles are the same point set as the original's `U2DSparseArrayPointContainer` (ee_astar's
//! `SparseArray`: the save lists them in its order). The map's tiles, the path requests, the continents, the AI's
//! shuffle and the physics draws stay C++ host operations, made in the original's order.
//!
//! Determinism (RUST_PLAN 2.1): the search radius is the original's `unsigned long` (wrapping), the path area's
//! diagonal `(float)sqrt((double)...)` of 32-bit products, `UMFloatToLong` is `lrintf`.

use std::collections::BTreeMap;
use std::sync::Mutex;

use ee_astar::continent_grid::EePoint;
use ee_astar::sparse::{PointList, SparseArray};

use crate::physics::lrintf;
use crate::rust_goal::{base_condition, EeUnitsGoalIo, GoalHost};

/// `kActionFindUnexploredArea`; the triggers the goal answers
const ACTION_FIND_UNEXPLORED_AREA: i64 = 11002;
const TRIGGER_NO_UNEXPLORED_AREAS_LEFT: i64 = 8009;
const TRIGGER_SEARCHING_FOR_AREA_TO_EXPLORE: i64 = 11003;
/// `kUnitFamilyShip`
const FAMILY_SHIP: i64 = 9;
/// a tile off the map
const TILE_NONE: i32 = -10000;
/// a group of this size or more searches no ring and keeps looking for unexplored areas
const GROUP_LARGE: i64 = 5;

/// One explore goal's state.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
struct Explore {
    bad: SparseArray,
    x: i32,
    y: i32,
    radius: u32,
    no_unexplored: bool,
    no_nonvisible: bool,
}

static EXPLORES: Mutex<BTreeMap<u64, Explore>> = Mutex::new(BTreeMap::new());

/// A copy of the goal's state (no lock held across a host call).
fn get(goal: u64) -> Explore {
    let goals = EXPLORES.lock().unwrap_or_else(|e| e.into_inner());
    goals.get(&goal).cloned().unwrap_or_default()
}

fn put(goal: u64, state: Explore) {
    let mut goals = EXPLORES.lock().unwrap_or_else(|e| e.into_inner());
    goals.insert(goal, state);
}

/// The map's, the paths' and the players' calls (the FFI adapter; the tests' mock).
pub trait ExploreHost: GoalHost {
    /// the unit's player's `IsHuman()`, `mSeesAll`
    fn player_human(&mut self, unit: u64) -> bool;
    fn player_sees_all(&mut self, unit: u64) -> bool;
    /// the unit's `mContainer` (0: none)
    fn container(&mut self, unit: u64) -> u64;
    /// the unit's active group's `GetNumUnits()` (-1: no group)
    fn group_units(&mut self, unit: u64) -> i64;
    /// the unit's path area's hull bounds (top, bottom, left, right; none: no area)
    fn path_area_bounds(&mut self, unit: u64) -> Option<[i32; 4]>;
    /// the unit's `GetLineOfSight()`
    fn line_of_sight(&mut self, unit: u64) -> f32;
    /// `EEGetSquareRingTiles(x, y, ring)` shuffled by `EEAIRandomShuffle`
    fn ring_tiles(&mut self, x: i32, y: i32, ring: i32) -> Vec<(i32, i32)>;
    /// whether the ring search passes the tile over: no tile or an edge tile, visible to the unit's player, or an
    /// air unit (that cannot move on space terrain 12 on a space map)
    fn tile_screen(&mut self, unit: u64, x: i32, y: i32) -> bool;
    /// the unit's active group has taken the tile's centre (`IsCellTaken`; false without a group)
    fn cell_taken(&mut self, unit: u64, x: i32, y: i32) -> bool;
    /// `EECanStandOn(tile, unit)`
    fn can_stand(&mut self, unit: u64, x: i32, y: i32) -> bool;
    /// the path request from `from` to `to` with the unit's terrain flags ends in the tile's path sub sector (or one
    /// whose links to it are blocked)
    fn path_reachable(&mut self, unit: u64, from: (i32, i32), to: (i32, i32)) -> bool;
    /// the path area's id at a tile (0: none)
    fn path_area_id(&mut self, x: i32, y: i32) -> u16;
    /// `gMapWidth`, `gMapHeight` and the map's two diamond sizes (`mL195614`, `mL195610`)
    fn map_dimensions(&mut self) -> [i32; 4];
    /// a random point of the unit's continent (a random continent for an air unit or a space fighter); none
    /// without one
    fn continent_point(&mut self, unit: u64) -> Option<(i32, i32)>;
    /// `gPhysics->GetRandom(low, high)`
    fn random(&mut self, low: i32, high: i32) -> i32;
    /// a nearby target (`EEFindNearbyTarget`) noticed by the unit's player
    fn notice_foreign(&mut self, unit: u64);
    /// the unit's class's `mFamily`; whether its player owns the Lighthouse at Alexandria
    fn unit_family(&mut self, unit: u64) -> i64;
    fn owns_lighthouse(&mut self, unit: u64) -> bool;
}

/// `EEUGExplore(inArg)`: no tile searched yet.
pub fn create(goal: u64) {
    put(goal, Explore::default());
}

pub fn destroy(goal: u64) {
    EXPLORES.lock().unwrap_or_else(|e| e.into_inner()).remove(&goal);
}

/// The tile being explored.
pub fn tile(goal: u64) -> (i32, i32) {
    let e = get(goal);
    (e.x, e.y)
}

fn point(x: i32, y: i32) -> EePoint {
    EePoint { x, y }
}

/// The path area's diagonal or 30: the ring search's widest radius.
fn widest(unit: u64, host: &mut dyn ExploreHost) -> u32 {
    if host.group_units(unit) < GROUP_LARGE {
        if let Some([top, bottom, left, right]) = host.path_area_bounds(unit) {
            let a = bottom.wrapping_sub(top);
            let b = right.wrapping_sub(left);
            let distance = (f64::from(b.wrapping_mul(b)) + f64::from(a.wrapping_mul(a))).sqrt() as f32;
            return lrintf(distance) as u32;
        }
    }
    30
}

/// `FindNextTile`: an unseen, reachable tile on rings round the unit that widen by 5 per call up to the path area's
/// size (false while rings remain), else a random continent point; a nearby foreigner noticed, the group told.
fn find_next_tile(e: &mut Explore, unit: u64, host: &mut dyn ExploreHost) -> bool {
    if !host.player_human(unit) {
        e.no_unexplored = true;
    }
    if !e.no_nonvisible {
        e.no_nonvisible = host.player_sees_all(unit);
    }
    let mut found = false;
    if !e.no_nonvisible && !e.no_unexplored {
        let (x, y) = host.unit_tile(unit);
        let (mut cx, mut cy) = (x as i32, y as i32);
        if cx == TILE_NONE || cy == TILE_NONE {
            let container = host.container(unit);
            if container != 0 {
                let (x, y) = host.unit_tile(container);
                (cx, cy) = (x as i32, y as i32);
            }
        }
        if (cx, cy) != (e.x, e.y) {
            e.bad.insert(point(e.x, e.y));
        }
        let maximum = widest(unit, host);
        let sight = lrintf(host.line_of_sight(unit)) as u32;
        e.radius = e.radius.wrapping_add(5);
        if e.radius.wrapping_add(sight) > maximum {
            e.radius = maximum.wrapping_sub(sight);
        }
        let mut ring = e.radius.wrapping_sub(5).wrapping_add(sight);
        while !found && ring < e.radius.wrapping_add(sight) {
            let tiles = host.ring_tiles(cx, cy, ring as i32);
            for &(tx, ty) in tiles.iter().rev() {
                (e.x, e.y) = (tx, ty);
                if host.tile_screen(unit, tx, ty) {
                    continue;
                }
                if e.bad.contains(point(tx, ty)) || host.cell_taken(unit, tx, ty) {
                    continue;
                }
                if !host.can_stand(unit, tx, ty) {
                    e.bad.insert(point(tx, ty));
                    continue;
                }
                if !host.path_reachable(unit, (cx, cy), (tx, ty))
                    && host.path_area_id(tx, ty) != host.path_area_id(cx, cy)
                {
                    e.bad.insert(point(tx, ty));
                    continue;
                }
                found = true;
                break;
            }
            ring = ring.wrapping_add(1);
        }
        if !found && ring < maximum {
            return false;
        }
    }
    if !found {
        if host.group_units(unit) < GROUP_LARGE {
            e.no_unexplored = true;
        }
        let (ux, uy) = host.unit_tile(unit);
        let [width, height, across, down] = host.map_dimensions();
        if (ux as u32) < width as u32 && (uy as u32) < height as u32 {
            if let Some((x, y)) = host.continent_point(unit) {
                (e.x, e.y) = (x, y);
            }
        } else {
            let rx = host.random(0, across.wrapping_sub(1));
            let ry = host.random(0, down.wrapping_sub(1));
            e.x = ry.wrapping_add(rx);
            e.y = ry.wrapping_add(down.wrapping_sub(rx));
        }
    }
    e.radius = 0;
    host.notice_foreign(unit);
    host.tell_group(unit, [e.x as f32 + 0.5, e.y as f32 + 0.5, 0.0]);
    true
}

/// `Step`: on find-unexplored-area, the next tile and the path attempts reset; a ship whose player owns the
/// Lighthouse at Alexandria skips the ring search.
pub fn step(goal: u64, unit: u64, host: &mut dyn ExploreHost) {
    if host.unit_command(unit) != ACTION_FIND_UNEXPLORED_AREA {
        return;
    }
    let mut e = get(goal);
    if !e.no_unexplored && host.unit_family(unit) == FAMILY_SHIP {
        let owns = host.owns_lighthouse(unit);
        e.no_unexplored = owns;
        e.no_nonvisible = owns;
    }
    find_next_tile(&mut e, unit, host);
    put(goal, e);
    host.reset_path_attempts(goal);
}

/// `EvaluateCondition`: no unexplored areas left (never), searching (while the radius is not 0); the others the
/// base goal's.
pub fn evaluate(goal: u64, event: i64) -> bool {
    match event {
        TRIGGER_NO_UNEXPLORED_AREAS_LEFT => false,
        TRIGGER_SEARCHING_FOR_AREA_TO_EXPLORE => get(goal).radius != 0,
        _ => base_condition(event),
    }
}

/// The checksum (the original adds no base goal's): the bad tiles' count, the tile, the radius, the flags.
pub fn checksum(goal: u64) -> i64 {
    let e = get(goal);
    let sum = (e.bad.size() as u32)
        .wrapping_add(e.x as u32)
        .wrapping_add(e.y as u32)
        .wrapping_add(e.radius)
        .wrapping_add(u32::from(e.no_unexplored))
        .wrapping_add(u32::from(e.no_nonvisible));
    i64::from(sum as i32)
}

/// The out-of-synch dump.
pub fn dump(goal: u64, host: &mut dyn ExploreHost) {
    let e = get(goal);
    host.append(&format!("OUT OF SYNCH DUMP:  EEUGExplore Bad Tile Array Size {}\n", e.bad.size()));
    host.append(&format!("OUT OF SYNCH DUMP:  EEUGExplore Tile Location X: {}\n", e.x));
    host.append(&format!("OUT OF SYNCH DUMP:  EEUGExplore Tile Location Y: {}\n", e.y));
    host.append(&format!("OUT OF SYNCH DUMP:  EEUGExplore Search Radius {}\n", e.radius as i32));
    host.append(&format!("OUT OF SYNCH DUMP:  EEUGExplore No UnExplored Areas flag {}\n", i32::from(e.no_unexplored)));
    host.append(&format!("OUT OF SYNCH DUMP:  EEUGExplore No NonVisible Areas flag {}\n", i32::from(e.no_nonvisible)));
}

/// `U2DPoint<long>::Save`
fn save_point(x: i32, y: i32, host: &mut dyn ExploreHost) -> i32 {
    if !host.file_open() {
        return 4;
    }
    let result = host.write_i32(x);
    if result != 0 {
        return result;
    }
    host.write_i32(y)
}

/// `U2DPoint<long>::Load` (the fields read so far kept)
fn load_point(x: &mut i32, y: &mut i32, host: &mut dyn ExploreHost) -> i32 {
    if !host.file_open() {
        return 4;
    }
    let result = host.read_i32(x);
    if result != 0 {
        return result;
    }
    host.read_i32(y)
}

/// `Save`'s part after the base goal's: the bad tiles (count and points), the tile, the flags, the radius; the first
/// failing status.
pub fn save(goal: u64, host: &mut dyn ExploreHost) -> i32 {
    let e = get(goal);
    let mut points = Vec::new();
    e.bad.points(PointList::All(1), &mut points);
    let mut result = host.write_u32(points.len() as u32);
    for p in &points {
        if result != 0 {
            break;
        }
        result = save_point(p.x, p.y, host);
    }
    if result == 0 {
        result = save_point(e.x, e.y, host);
        if result == 0 {
            result = host.write_bool(e.no_unexplored);
            if result == 0 {
                result = host.write_bool(e.no_nonvisible);
                if result == 0 {
                    result = host.write_u32(e.radius);
                }
            }
        }
    }
    result
}

/// `Load`'s part after the base goal's: the bad tiles, the tile, the flags, the radius; the first failing status
/// (a point is inserted even when its read failed, as the original did).
pub fn load(goal: u64, host: &mut dyn ExploreHost) -> i32 {
    let mut e = get(goal);
    let mut count = 0u32;
    let mut result = host.read_u32(&mut count);
    e.bad.clear();
    let (mut px, mut py) = (0i32, 0i32);
    for _ in 0..count {
        if result != 0 {
            put(goal, e);
            return result;
        }
        result = load_point(&mut px, &mut py, host);
        e.bad.insert(point(px, py));
    }
    if result == 0 {
        result = load_point(&mut e.x, &mut e.y, host);
        if result == 0 {
            result = host.read_bool(&mut e.no_unexplored);
            if result == 0 {
                result = host.read_bool(&mut e.no_nonvisible);
                if result == 0 {
                    result = host.read_u32(&mut e.radius);
                }
            }
        }
    }
    put(goal, e);
    result
}

/// The goal's body `op` (rust_goal.rs `EE_UNITS_GOAL_*`).
pub fn body(op: i32, goal: u64, a: u64, b: u64, io: &mut EeUnitsGoalIo, host: &mut dyn ExploreHost) -> i64 {
    use crate::rust_goal::*;
    match op {
        EE_UNITS_GOAL_NEW_EXPLORE => create(goal),
        EE_UNITS_GOAL_DESTROY => destroy(goal),
        EE_UNITS_GOAL_CLONE => {
            let copy = host.new_goal(GOAL_EXPLORE, b != 0);
            create(copy);
            return copy as i64;
        }
        EE_UNITS_GOAL_CHECKSUM => return checksum(goal),
        EE_UNITS_GOAL_DUMP => dump(goal, host),
        EE_UNITS_GOAL_DEBUG_TEXT => host.append("Exploring"),
        EE_UNITS_GOAL_LOCATION | EE_UNITS_GOAL_DESTINATION => {
            let (x, y) = tile(goal);
            let p = host.tile_location(i64::from(x), i64::from(y));
            io.f[..3].copy_from_slice(&p);
            if op == EE_UNITS_GOAL_DESTINATION {
                host.tell_group(a, p);
            }
        }
        EE_UNITS_GOAL_START_ACTION => return ACTION_FIND_UNEXPLORED_AREA,
        EE_UNITS_GOAL_START => host.start_action(a, ACTION_FIND_UNEXPLORED_AREA),
        EE_UNITS_GOAL_STEP => step(goal, a, host),
        EE_UNITS_GOAL_SAVE => return i64::from(save(goal, host)),
        EE_UNITS_GOAL_LOAD => return i64::from(load(goal, host)),
        EE_UNITS_GOAL_EVALUATE => return i64::from(evaluate(goal, b as i64)),
        EE_UNITS_GOAL_EXPLORE_TILE => {
            let (x, y) = tile(goal);
            io.i[0] = i64::from(x);
            io.i[1] = i64::from(y);
        }
        EE_UNITS_GOAL_EXPLORE_SET_TILE => {
            let mut e = get(goal);
            (e.x, e.y) = (io.i[0] as i32, io.i[1] as i32);
            put(goal, e);
        }
        _ => {}
    }
    0
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::rust_goal::*;

    /// A square map of tiles, each with a screen flag, a stand flag, a reachable flag and an area id; a log.
    #[derive(Default)]
    struct World {
        log: Vec<String>,
        text: String,
        human: bool,
        sees_all: bool,
        group: i64,
        tile: (i64, i64),
        sight: f32,
        bounds: Option<[i32; 4]>,
        /// tiles passed over, refused to stand on, unreachable (with another area id)
        screened: Vec<(i32, i32)>,
        no_stand: Vec<(i32, i32)>,
        unreachable: Vec<(i32, i32)>,
        other_area: Vec<(i32, i32)>,
        continent: Option<(i32, i32)>,
        draws: Vec<i32>,
        family: i64,
        lighthouse: bool,
        command: i64,
        file: Vec<i64>,
        reads: Vec<i64>,
        open: bool,
    }

    impl GoalHost for World {
        fn reference(&mut self, _o: u64, _g: bool) {}
        fn dereference(&mut self, _o: u64, _g: bool) {}
        fn unit_id(&mut self, u: u64) -> u32 {
            u as u32
        }
        fn unit_tile(&mut self, u: u64) -> (i64, i64) {
            if u == 0xc0 {
                (7, 7)
            } else {
                self.tile
            }
        }
        fn unit_valid(&mut self, _u: u64) -> bool {
            true
        }
        fn unit_command(&mut self, _u: u64) -> i64 {
            self.command
        }
        fn reset_path_attempts(&mut self, _g: u64) {
            self.log.push("attempts 0".into());
        }
        fn start_action(&mut self, u: u64, a: i64) {
            self.log.push(format!("start {u:x} {a}"));
        }
        fn find_unit(&mut self, id: u32, _c: bool) -> u64 {
            u64::from(id)
        }
        fn unit_position(&mut self, _u: u64) -> [f32; 3] {
            [0.0; 3]
        }
        fn tile_location(&mut self, x: i64, y: i64) -> [f32; 3] {
            [x as f32, y as f32, 1.0]
        }
        fn tell_group(&mut self, _u: u64, p: [f32; 3]) {
            self.log.push(format!("tell {p:?}"));
        }
        fn set_physics_action(&mut self, _u: u64, _a: i64) {}
        fn set_animation(&mut self, _u: u64, _a: i64) {}
        fn drop_path(&mut self, _u: u64) {}
        fn new_goal(&mut self, _k: u8, _a: bool) -> u64 {
            0xe0_0f00
        }
        fn file_open(&mut self) -> bool {
            self.open
        }
        fn write_u32(&mut self, v: u32) -> i32 {
            self.file.push(i64::from(v));
            0
        }
        fn write_i32(&mut self, v: i32) -> i32 {
            self.file.push(i64::from(v));
            0
        }
        fn write_bool(&mut self, v: bool) -> i32 {
            self.file.push(i64::from(v));
            0
        }
        fn read_u32(&mut self, v: &mut u32) -> i32 {
            if self.reads.is_empty() {
                return 5;
            }
            *v = self.reads.remove(0) as u32;
            0
        }
        fn read_i32(&mut self, v: &mut i32) -> i32 {
            if self.reads.is_empty() {
                return 5;
            }
            *v = self.reads.remove(0) as i32;
            0
        }
        fn read_bool(&mut self, v: &mut bool) -> i32 {
            if self.reads.is_empty() {
                return 5;
            }
            *v = self.reads.remove(0) != 0;
            0
        }
        fn append(&mut self, t: &str) {
            self.text.push_str(t);
        }
        fn append_class_name(&mut self, _u: u64) {}
    }

    impl ExploreHost for World {
        fn player_human(&mut self, _u: u64) -> bool {
            self.human
        }
        fn player_sees_all(&mut self, _u: u64) -> bool {
            self.sees_all
        }
        fn container(&mut self, _u: u64) -> u64 {
            0xc0
        }
        fn group_units(&mut self, _u: u64) -> i64 {
            self.group
        }
        fn path_area_bounds(&mut self, _u: u64) -> Option<[i32; 4]> {
            self.bounds
        }
        fn line_of_sight(&mut self, _u: u64) -> f32 {
            self.sight
        }
        fn ring_tiles(&mut self, x: i32, y: i32, ring: i32) -> Vec<(i32, i32)> {
            self.log.push(format!("ring {ring}"));
            // the ring's tiles (no shuffle): the top row, from the left
            (0..=2 * ring).map(|i| (x - ring + i, y + ring)).collect()
        }
        fn tile_screen(&mut self, _u: u64, x: i32, y: i32) -> bool {
            self.screened.contains(&(x, y))
        }
        fn cell_taken(&mut self, _u: u64, _x: i32, _y: i32) -> bool {
            false
        }
        fn can_stand(&mut self, _u: u64, x: i32, y: i32) -> bool {
            !self.no_stand.contains(&(x, y))
        }
        fn path_reachable(&mut self, _u: u64, _f: (i32, i32), t: (i32, i32)) -> bool {
            !self.unreachable.contains(&t)
        }
        fn path_area_id(&mut self, x: i32, y: i32) -> u16 {
            u16::from(self.other_area.contains(&(x, y)))
        }
        fn map_dimensions(&mut self) -> [i32; 4] {
            [100, 100, 140, 140]
        }
        fn continent_point(&mut self, _u: u64) -> Option<(i32, i32)> {
            self.log.push("continent".into());
            self.continent
        }
        fn random(&mut self, low: i32, high: i32) -> i32 {
            self.log.push(format!("random {low} {high}"));
            self.draws.remove(0)
        }
        fn notice_foreign(&mut self, _u: u64) {
            self.log.push("notice".into());
        }
        fn unit_family(&mut self, _u: u64) -> i64 {
            self.family
        }
        fn owns_lighthouse(&mut self, _u: u64) -> bool {
            self.lighthouse
        }
    }

    fn human() -> World {
        World {
            human: true,
            group: -1,
            tile: (50, 50),
            sight: 2.0,
            command: ACTION_FIND_UNEXPLORED_AREA,
            continent: Some((60, 61)),
            open: true,
            ..Default::default()
        }
    }

    #[test]
    fn the_ring_search_widens_and_picks_the_last_good_tile() {
        let goal = 0xe0_0001u64;
        create(goal);
        let mut w = human();
        // the first ring: radius 5 + sight 2 > 30? no: rings 2..7; ring 2's top row from the right: (52, 52) first
        w.screened = vec![(52, 52)];
        w.no_stand = vec![(51, 52)];
        w.unreachable = vec![(50, 52)];
        w.other_area = vec![(50, 52)];
        step(goal, 0x900, &mut w);
        // (0, 0) differs from the centre: a bad tile; (51, 52) refused, (50, 52) unreachable in another area: bad
        assert_eq!(tile(goal), (49, 52));
        let e = get(goal);
        assert!(e.bad.contains(point(0, 0)) && e.bad.contains(point(51, 52)) && e.bad.contains(point(50, 52)));
        assert_eq!(e.radius, 0);
        assert_eq!(w.log, ["ring 2", "notice", "tell [49.5, 52.5, 0.0]", "attempts 0"]);
        assert!(evaluate(goal, TRIGGER_UNIT_FACING_NOT_REQUIRED));
        assert!(!evaluate(goal, TRIGGER_SEARCHING_FOR_AREA_TO_EXPLORE));
        assert!(!evaluate(goal, TRIGGER_NO_UNEXPLORED_AREAS_LEFT));
        destroy(goal);
    }

    #[test]
    fn nothing_found_waits_for_wider_rings_then_falls_back() {
        let goal = 0xe0_0002u64;
        create(goal);
        let mut w = human();
        // every tile passed over: rings 2..7 searched, 7 < the widest (30): false, the radius kept
        w.screened = (0..200).flat_map(|x| (0..200).map(move |y| (x, y))).collect();
        step(goal, 0x900, &mut w);
        assert_eq!(get(goal).radius, 5);
        assert!(evaluate(goal, TRIGGER_SEARCHING_FOR_AREA_TO_EXPLORE));
        assert_eq!(w.log.len(), 6);
        assert_eq!(w.log[5], "attempts 0");
        // the path area's diagonal: 3-4-5 -> 5; the radius is held to 5 - 2 = 3, the rings 0..5 reach it: the
        // fallback (the continent), no unexplored areas any more (a group under 5)
        w.bounds = Some([0, 3, 0, 4]);
        w.log.clear();
        step(goal, 0x900, &mut w);
        assert_eq!(tile(goal), (60, 61));
        let e = get(goal);
        assert!(e.no_unexplored);
        assert_eq!(e.radius, 0);
        assert_eq!(w.log[w.log.len() - 4..], ["continent", "notice", "tell [60.5, 61.5, 0.0]", "attempts 0"]);
        // a computer player never searches rings; off the map: two physics draws
        let other = 0xe0_0003u64;
        create(other);
        let mut w = human();
        w.human = false;
        w.tile = (-10000, -10000);
        w.draws = vec![3, 4];
        step(other, 0x900, &mut w);
        assert_eq!(w.log[..2], ["random 0 139", "random 0 139"]);
        assert_eq!(tile(other), (7, 4 + 140 - 3));
        // not the explore action: nothing
        w.command = 1;
        w.log.clear();
        step(other, 0x900, &mut w);
        assert!(w.log.is_empty());
        destroy(goal);
        destroy(other);
    }

    #[test]
    fn a_ship_with_the_lighthouse_and_a_container() {
        let goal = 0xe0_0004u64;
        create(goal);
        let mut w = human();
        w.family = FAMILY_SHIP;
        w.lighthouse = true;
        step(goal, 0x900, &mut w);
        assert_eq!(w.log[0], "continent");
        let e = get(goal);
        assert!(e.no_unexplored && e.no_nonvisible);
        // in a container: its tile is the centre (no bad tile when the old tile is the container's)
        let other = 0xe0_0005u64;
        create(other);
        let mut w = human();
        w.tile = (-10000, 3);
        let mut e = get(other);
        (e.x, e.y) = (7, 7);
        put(other, e);
        step(other, 0x900, &mut w);
        assert!(!get(other).bad.contains(point(7, 7)));
        assert_eq!(w.log[0], "ring 2");
        destroy(goal);
        destroy(other);
    }

    #[test]
    fn save_load_checksum_texts() {
        let goal = 0xe0_0006u64;
        create(goal);
        let mut e = get(goal);
        for p in [(3, 4), (3, 5), (9, 1)] {
            e.bad.insert(point(p.0, p.1));
        }
        (e.x, e.y, e.radius, e.no_unexplored) = (11, -12, 5, true);
        put(goal, e);
        let mut w = human();
        assert_eq!(save(goal, &mut w), 0);
        // the columns from the last
        assert_eq!(w.file, [3, 9, 1, 3, 4, 3, 5, 11, -12, 1, 0, 5]);
        assert_eq!(checksum(goal), 3 + 11 - 12 + 5 + 1);
        let other = 0xe0_0007u64;
        create(other);
        w.reads = w.file.clone();
        assert_eq!(load(other, &mut w), 0);
        assert_eq!(get(other), get(goal));
        // a short file: the failing status; a closed file: 4
        let third = 0xe0_0008u64;
        create(third);
        w.reads = vec![2, 1, 1, 2];
        assert_eq!(load(third, &mut w), 5);
        assert_eq!(get(third).bad.size(), 2);
        w.open = false;
        w.reads = vec![0];
        assert_eq!(load(third, &mut w), 4);
        dump(goal, &mut w);
        assert_eq!(
            w.text,
            "OUT OF SYNCH DUMP:  EEUGExplore Bad Tile Array Size 3\n\
             OUT OF SYNCH DUMP:  EEUGExplore Tile Location X: 11\n\
             OUT OF SYNCH DUMP:  EEUGExplore Tile Location Y: -12\n\
             OUT OF SYNCH DUMP:  EEUGExplore Search Radius 5\n\
             OUT OF SYNCH DUMP:  EEUGExplore No UnExplored Areas flag 1\n\
             OUT OF SYNCH DUMP:  EEUGExplore No NonVisible Areas flag 0\n"
        );
        let mut io = EeUnitsGoalIo::default();
        w.text.clear();
        body(EE_UNITS_GOAL_DEBUG_TEXT, goal, 0, 0, &mut io, &mut w);
        assert_eq!(w.text, "Exploring");
        body(EE_UNITS_GOAL_LOCATION, goal, 0, 0, &mut io, &mut w);
        assert_eq!(io.f[..3], [11.0, -12.0, 1.0]);
        body(EE_UNITS_GOAL_EXPLORE_TILE, goal, 0, 0, &mut io, &mut w);
        assert_eq!(io.i[..2], [11, -12]);
        io.i[..2].copy_from_slice(&[4, 5]);
        body(EE_UNITS_GOAL_EXPLORE_SET_TILE, other, 0, 0, &mut io, &mut w);
        assert_eq!(tile(other), (4, 5));
        // the copy starts afresh
        let copy = body(EE_UNITS_GOAL_CLONE, goal, 0, 0, &mut io, &mut w) as u64;
        assert_eq!(get(copy), Explore::default());
        w.log.clear();
        body(EE_UNITS_GOAL_START, goal, 0x900, 0, &mut io, &mut w);
        assert_eq!(w.log, ["start 900 11002"]);
        for g in [goal, other, third, copy] {
            destroy(g);
        }
    }
}
