//! cls_move.rs's tests: the movement bodies over a mock world (tiles with heights and units, units with their
//! records, waypoints and goals), each checked against the C++ body's statements worked by hand.

use super::*;
use crate::units::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields};
use std::collections::BTreeMap;

struct MockUnit {
    rts: Box<EeUnitsRtsUnitFields>,
    cx: Box<EeUnitsComplexUnitFields>,
    class: u64,
    location: [f32; 3],
    player_id: i32,
    max_velocity: f32,
    waypoints: Vec<Ge3DPoint>,
    goal: u64,
    active_goal: u64,
    blocking: u64,
    last_attacker: u64,
    passable: bool,
}

#[derive(Default, Clone)]
struct MockGoal {
    kind: i32,
    location: Ge3DPoint,
    b1c: bool,
    b1d: bool,
    auto_waypoint: bool,
    group_path_state: u8,
    path_attempts: i32,
    sidestep: u32,
    target: u64,
}

#[derive(Default)]
struct Mock {
    /// boxed: the records' addresses stay put while the list grows
    #[allow(clippy::vec_box)]
    classes: Vec<Box<EeUnitsUnitClassFields>>,
    units: Vec<MockUnit>,
    goals: Vec<MockGoal>,
    /// tile heights (absent: off the map)
    tiles: BTreeMap<(i32, i32), f32>,
    /// units standing on a tile
    on_tile: BTreeMap<(i32, i32), Vec<u64>>,
    /// tiles nobody can stand on
    walls: Vec<Tile>,
    /// the bounded path the path manager answers
    path: Vec<Ge3DPoint>,
    pursuer: u64,
    retarget: bool,
    /// the statements, in order
    log: Vec<String>,
}

const CLASS: u64 = 0x100;
const GOAL: u64 = 0x200;

impl Mock {
    fn map(w: i32, h: i32) -> Mock {
        let mut m = Mock::default();
        for x in 0..w {
            for y in 0..h {
                m.tiles.insert((x, y), 0.5);
            }
        }
        m
    }
    fn class(&mut self, f: impl FnOnce(&mut EeUnitsUnitClassFields)) -> u64 {
        let mut c =
            EeUnitsUnitClassFields { mInnerCollisionSphere: 0.5, mDomains: MV_MOVEMENT_LAND, ..Default::default() };
        f(&mut c);
        self.classes.push(Box::new(c));
        CLASS + self.classes.len() as u64 - 1
    }
    fn add(&mut self, x: f32, y: f32, class: u64) -> u64 {
        self.units.push(MockUnit {
            rts: Box::new(EeUnitsRtsUnitFields { mTileX: floor(x) as _, mTileY: floor(y) as _, ..Default::default() }),
            cx: Box::default(),
            class,
            location: [x, y, 0.0],
            player_id: 1,
            max_velocity: 1.0,
            waypoints: Vec::new(),
            goal: 0,
            active_goal: 0,
            blocking: 0,
            last_attacker: 0,
            passable: true,
        });
        self.units.len() as u64
    }
    fn goal(&mut self, g: MockGoal) -> u64 {
        self.goals.push(g);
        GOAL + self.goals.len() as u64 - 1
    }
    fn u(&mut self, unit: u64) -> &mut MockUnit {
        &mut self.units[unit as usize - 1]
    }
    fn g(&mut self, goal: u64) -> &mut MockGoal {
        &mut self.goals[(goal - GOAL) as usize]
    }
    fn say(&mut self, s: String) {
        self.log.push(s);
    }
}

impl MoveHost for Mock {
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit {
        if obj == 0 {
            return EeUnitsUuUnit::default();
        }
        let class = self.u(obj).class;
        let fields: *const EeUnitsUnitClassFields = &*self.classes[(class - CLASS) as usize];
        let u = self.u(obj);
        EeUnitsUuUnit {
            obj,
            rts: &*u.rts,
            cx: &*u.cx,
            class_fields: fields,
            class_obj: class,
            player_id: u.player_id,
            location: u.location,
            stats_max_velocity: u.max_velocity,
            ..Default::default()
        }
    }
    fn class(&mut self, class: u64) -> *const EeUnitsUnitClassFields {
        &*self.classes[(class - CLASS) as usize]
    }
    fn position(&mut self, obj: u64) -> Ge3DPoint {
        let l = self.u(obj).location;
        Ge3DPoint { x: l[0], y: l[1], z: l[2] }
    }
    fn location(&mut self, unit: u64) -> GeTransformation {
        let l = self.u(unit).location;
        let mut t = GeTransformation::default();
        for i in 0..4 {
            t.m[i][i] = 1.0;
        }
        t.m[0][3] = l[0];
        t.m[1][3] = l[1];
        t.m[2][3] = l[2];
        t
    }
    fn map_tile(&mut self, x: i32, y: i32) -> u64 {
        if self.tiles.contains_key(&(x, y)) {
            (((x as u64) << 16) | y as u64) + 1
        } else {
            0
        }
    }
    fn tile_height(&mut self, tile: u64) -> f32 {
        let t = tile - 1;
        self.tiles[&(((t >> 16) as i32), (t & 0xffff) as i32)]
    }
    fn tile_units(&mut self, tile: u64) -> Vec<u64> {
        let t = tile - 1;
        self.on_tile.get(&(((t >> 16) as i32), (t & 0xffff) as i32)).cloned().unwrap_or_default()
    }
    fn closest_tile_height(&mut self, _p: Ge3DPoint) -> Option<f32> {
        Some(2.0)
    }
    fn passable_by(&mut self, obj: u64, _by: u64) -> bool {
        self.u(obj).passable
    }
    fn can_stand_on(&mut self, tile: Tile, _unit: u64) -> bool {
        self.tiles.contains_key(&(tile[0], tile[1])) && !self.walls.contains(&tile)
    }
    fn tile_top_above(&mut self, _x: i32, _y: i32) -> bool {
        false
    }
    fn unit_tiles(&mut self, unit: u64, _margin: f32) -> Vec<Tile> {
        let r = &self.u(unit).rts;
        vec![[r.mTileX as i32, r.mTileY as i32], [r.mTileX as i32 + 1, r.mTileY as i32]]
    }
    fn tile_line(&mut self, from: Tile, to: Tile, _ends: bool, _thin: bool) -> Vec<Tile> {
        vec![from, to]
    }
    fn move_in_direction(&mut self, x: f32, y: f32, direction: i32, distance: i32) -> [f32; 2] {
        [x + (direction * distance) as f32, y]
    }
    fn turn_away(&mut self, direction: i32, spread: bool) -> i32 {
        self.say(format!("turn_away {direction} {spread}"));
        (direction + 4) % 8
    }
    fn pursuer_of(&mut self, _unit: u64) -> u64 {
        self.pursuer
    }
    fn direction_to(&mut self, _from: u64, _to: u64) -> i32 {
        6
    }
    fn line_of_sight(&mut self, _unit: u64) -> f32 {
        3.0
    }
    fn max_velocity(&mut self, unit: u64) -> f32 {
        self.u(unit).max_velocity
    }
    fn max_angular_velocity(&mut self, _unit: u64) -> f32 {
        1.0
    }
    fn random_1_2(&mut self) -> i32 {
        self.say("random".into());
        2
    }
    fn goal(&mut self, unit: u64) -> u64 {
        self.u(unit).goal
    }
    fn active_goal(&mut self, unit: u64) -> u64 {
        self.u(unit).active_goal
    }
    fn blocking_unit(&mut self, unit: u64) -> u64 {
        self.u(unit).blocking
    }
    fn last_attacker(&mut self, unit: u64) -> u64 {
        self.u(unit).last_attacker
    }
    fn clear_last_attacker(&mut self, unit: u64) {
        self.u(unit).last_attacker = 0;
        self.say("clear_last_attacker".into());
    }
    fn goal_kind(&mut self, goal: u64) -> i32 {
        self.g(goal).kind
    }
    fn goal_location(&mut self, goal: u64) -> Ge3DPoint {
        self.g(goal).location
    }
    fn goal_destination(&mut self, goal: u64, _unit: u64) -> Ge3DPoint {
        self.g(goal).location
    }
    fn goal_flag(&mut self, goal: u64, b1d: bool) -> bool {
        if b1d {
            self.g(goal).b1d
        } else {
            self.g(goal).b1c
        }
    }
    fn add_path_attempts(&mut self, goal: u64, n: i32) {
        self.g(goal).path_attempts += n;
    }
    fn sidestep_count(&mut self, goal: u64) -> u32 {
        self.g(goal).sidestep
    }
    fn set_sidestep_count(&mut self, goal: u64, n: u32) {
        self.g(goal).sidestep = n;
    }
    fn attack_target(&mut self, goal: u64) -> u64 {
        self.g(goal).target
    }
    fn auto_waypoint(&mut self, goal: u64) -> bool {
        self.g(goal).auto_waypoint
    }
    fn group_path_state(&mut self, goal: u64) -> u8 {
        self.g(goal).group_path_state
    }
    fn set_group_path_state(&mut self, goal: u64, state: u8) {
        self.g(goal).group_path_state = state;
    }
    fn gathers_wood(&mut self, _goal: u64) -> bool {
        false
    }
    fn ignore_target(&mut self, _unit: u64, target: u64) {
        self.say(format!("ignore {target}"));
    }
    fn melee_retarget(&mut self, _unit: u64, _target: u64, _attack: u64) -> bool {
        self.retarget
    }
    fn set_blocking_unit(&mut self, unit: u64, blocker: u64) {
        self.u(unit).blocking = blocker;
        self.say(format!("blocking {blocker}"));
    }
    fn drop_path(&mut self, unit: u64) {
        self.u(unit).waypoints.clear();
        self.say("drop_path".into());
    }
    fn add_waypoint(&mut self, unit: u64, p: Ge3DPoint, snap: bool) {
        self.u(unit).waypoints.push(p);
        self.say(format!("waypoint {} {} {} {snap}", p.x, p.y, p.z));
    }
    fn waypoints(&mut self, unit: u64) -> i32 {
        self.u(unit).waypoints.len() as i32
    }
    fn waypoint(&mut self, unit: u64, index: i32) -> Ge3DPoint {
        self.u(unit).waypoints[index as usize]
    }
    fn second_waypoint(&mut self, unit: u64) -> Option<Ge3DPoint> {
        self.u(unit).waypoints.get(1).copied()
    }
    fn next_waypoint(&mut self, _unit: u64) {
        self.say("next_waypoint".into());
    }
    fn next_waypoint_point(&mut self, unit: u64) -> Ge3DPoint {
        self.u(unit).waypoints.first().copied().unwrap_or_default()
    }
    fn queue_goals(&mut self, _unit: u64) -> i32 {
        1
    }
    fn distance_squared_3d(&mut self, unit: u64, p: Ge3DPoint) -> f32 {
        let l = self.u(unit).location;
        Ge3DPoint { x: l[0], y: l[1], z: l[2] }.distance_to_squared(p)
    }
    fn clear_bounded_tiles(&mut self, _unit: u64) {
        self.say("bounded clear".into());
    }
    fn push_bounded_tile(&mut self, _unit: u64, tile: Tile) {
        self.say(format!("bounded {} {}", tile[0], tile[1]));
    }
    fn find_bounded_path(&mut self, _unit: u64, kind: i32, to: Tile) -> Vec<Ge3DPoint> {
        self.say(format!("find {kind} {} {}", to[0], to[1]));
        self.path.clone()
    }
    fn merge_waypoints(&mut self, _unit: u64, _path: &mut Vec<Ge3DPoint>) {
        self.say("merge".into());
    }
    fn set_velocity(&mut self, _unit: u64, v: f32) {
        self.say(format!("velocity {v}"));
    }
    fn start_action(&mut self, _unit: u64, action: i32) {
        self.say(format!("action {action}"));
    }
    fn action_changed(&mut self, class: u64, _unit: u64) {
        self.say(format!("changed {class:#x}"));
    }
    fn update_water_state(&mut self, _unit: u64) {
        self.say("water".into());
    }
    fn cancel_paths(&mut self, _unit: u64) {
        self.say("cancel".into());
    }
    fn set_initial_contact_location(&mut self, _unit: u64, p: Ge3DPoint) {
        self.say(format!("contact {} {}", p.x, p.y));
    }
    fn request_path(&mut self, _unit: u64, goal: u64, to: Tile, revs: i32, graph: bool, arg8: bool) {
        self.g(goal).path_attempts += 1;
        self.say(format!("request {} {} {revs} {graph} {arg8}", to[0], to[1]));
    }
    fn flee_goal(&mut self, _unit: u64, to: [f32; 2], keep: bool) {
        self.say(format!("flee {} {} {keep}", to[0], to[1]));
    }
}

fn has(m: &Mock, s: &str) -> bool {
    m.log.iter().any(|l| l == s)
}

#[test]
fn find_path_on_the_goal_tile_arrives() {
    let mut m = Mock::map(10, 10);
    let c = m.class(|_| {});
    let u = m.add(2.5, 2.5, c);
    let g = m.goal(MockGoal {
        kind: MV_GOAL_LOCATION,
        location: Ge3DPoint { x: 2.2, y: 2.8, z: 0.0 },
        ..Default::default()
    });
    m.u(u).goal = g;
    m.u(u).cx.mCollisionResolutionCount = 3.0;
    find_path(&mut m, c, u);
    assert!(m.u(u).cx.mHasArrived);
    assert_eq!(m.u(u).cx.mCollisionState, 8);
    assert_eq!(m.u(u).cx.mCollisionResolutionCount, 0.0);
    assert_eq!(m.log, ["blocking 0"]);
}

#[test]
fn find_path_takes_a_bounded_path_ending_at_the_tile() {
    let mut m = Mock::map(10, 10);
    let c = m.class(|_| {});
    let u = m.add(2.5, 2.5, c);
    let g = m.goal(MockGoal {
        kind: MV_GOAL_LOCATION,
        location: Ge3DPoint { x: 5.5, y: 2.5, z: 0.0 },
        ..Default::default()
    });
    m.u(u).goal = g;
    m.path = vec![
        Ge3DPoint { x: 2.5, y: 2.5, z: 0.0 },
        Ge3DPoint { x: 3.5, y: 2.5, z: 0.0 },
        Ge3DPoint { x: 4.5, y: 2.5, z: 0.0 },
        Ge3DPoint { x: 5.2, y: 2.4, z: 0.0 },
    ];
    find_path(&mut m, c, u);
    // the goal tile is the destination (no waypoints): index -1 takes the unit's position, tile (2, 2)
    assert!(has(&m, "find 1 2 2"));
    assert!(has(&m, "bounded 5 2"));
    assert_eq!(m.u(u).cx.mCollisionState, 2);
}

#[test]
fn find_path_with_waypoints_replaces_them_with_the_path() {
    let mut m = Mock::map(10, 10);
    let c = m.class(|_| {});
    let u = m.add(2.5, 2.5, c);
    m.u(u).waypoints = vec![Ge3DPoint { x: 5.5, y: 2.5, z: 0.0 }];
    m.path = vec![
        Ge3DPoint { x: 2.5, y: 2.5, z: 0.0 },
        Ge3DPoint { x: 3.5, y: 2.5, z: 0.0 },
        Ge3DPoint { x: 5.1, y: 2.9, z: 0.0 },
    ];
    find_path(&mut m, c, u);
    // the last point is replaced by the waypoint; the first (the unit's tile) and the last are skipped
    let i = m.log.iter().position(|l| l == "merge").expect("merged");
    assert_eq!(m.log[i + 1], "drop_path");
    assert_eq!(m.log[i + 2], "waypoint 3.5 2.5 0 false");
    assert_eq!(m.log.len(), i + 3);
    assert_eq!(m.u(u).cx.mCollisionState, 1);
    assert!(!m.u(u).cx.mHasArrived);
}

#[test]
fn find_path_without_a_path_waits_then_idles() {
    let mut m = Mock::map(10, 10);
    let c = m.class(|_| {});
    let u = m.add(2.5, 2.5, c);
    m.u(u).waypoints = vec![Ge3DPoint { x: 6.5, y: 2.5, z: 0.0 }];
    find_path(&mut m, c, u);
    assert_eq!(m.u(u).cx.mCollisionState, 2);
    m.log.clear();
    find_path(&mut m, c, u);
    assert_eq!(m.u(u).cx.mCollisionState, 0);
    assert_eq!(m.u(u).cx.mCurrentPhysicsAction, 6);
    let tail: Vec<_> = m.log.iter().rev().take(4).rev().cloned().collect();
    assert_eq!(tail, ["blocking 0", "velocity 0", "action 5000", format!("changed {c:#x}").as_str()]);
}

#[test]
fn find_path_air_index_skips_near_waypoints() {
    let mut m = Mock::map(20, 20);
    let c = m.class(|c| c.mDomains = MV_MOVEMENT_AIR);
    let u = m.add(2.5, 2.5, c);
    m.u(u).waypoints = vec![
        Ge3DPoint { x: 3.5, y: 2.5, z: 0.0 },
        Ge3DPoint { x: 4.5, y: 2.5, z: 0.0 },
        Ge3DPoint { x: 12.5, y: 2.5, z: 0.0 },
    ];
    find_path(&mut m, c, u);
    // the first waypoint 6 or more away (squared 36) is the third: tile (12, 2)
    assert!(has(&m, "find 1 12 2"));
}

#[test]
fn melee_sidestep_takes_the_freer_side() {
    let mut m = Mock::map(20, 20);
    let c = m.class(|c| {
        c.mIsMelee = true;
        c.mInnerCollisionSphere = 1.0;
    });
    let wall_class = m.class(|_| {});
    let u = m.add(5.5, 5.5, c);
    let target = m.add(7.5, 5.5, wall_class);
    let g = m.goal(MockGoal {
        kind: MV_GOAL_LOCATION,
        location: Ge3DPoint { x: 7.5, y: 5.5, z: 0.0 },
        b1c: true,
        ..Default::default()
    });
    let a = m.goal(MockGoal { kind: MV_GOAL_ATTACK_UNIT, target, ..Default::default() });
    m.u(u).goal = g;
    m.u(u).active_goal = a;
    m.u(u).cx.mVelocity = 1.0;
    // the side: (-(0), 2, 0) normalized * 2 = (0, 2): left is y + 2, right y - 2; a unit on the left
    m.on_tile.insert((5, 7), vec![target]);
    find_path(&mut m, c, u);
    assert!(has(&m, "waypoint 5.5 3.5 0 false"));
    assert!(has(&m, "waypoint 7.5 3.5 0 false"));
    assert_eq!(m.g(g).path_attempts, 8);
    assert_eq!(m.g(a).sidestep, 1);
}

#[test]
fn melee_sidestep_retargets_first() {
    let mut m = Mock::map(20, 20);
    let c = m.class(|c| c.mIsMelee = true);
    let other = m.class(|_| {});
    let u = m.add(5.5, 5.5, c);
    let target = m.add(6.5, 5.5, other);
    let g = m.goal(MockGoal {
        kind: MV_GOAL_LOCATION,
        location: Ge3DPoint { x: 6.5, y: 5.5, z: 0.0 },
        ..Default::default()
    });
    let a = m.goal(MockGoal { kind: MV_GOAL_ATTACK_UNIT, target, sidestep: 0, ..Default::default() });
    m.u(u).goal = g;
    m.u(u).active_goal = a;
    m.u(u).cx.mVelocity = 1.0;
    m.retarget = true;
    find_path(&mut m, c, u);
    assert_eq!(m.log, ["drop_path".to_string(), format!("ignore {target}")]);
    assert_eq!(m.g(a).sidestep, 0);
}

#[test]
fn next_waypoint_notes_a_blocked_tile() {
    let mut m = Mock::map(10, 10);
    let c = m.class(|_| {});
    let u = m.add(2.5, 2.5, c);
    let b = m.add(4.5, 2.5, c);
    m.u(b).passable = false;
    m.on_tile.insert((4, 2), vec![b]);
    m.u(u).waypoints = vec![Ge3DPoint { x: 3.5, y: 2.5, z: 0.0 }, Ge3DPoint { x: 4.5, y: 2.5, z: 0.0 }];
    next_waypoint(&mut m, c, u);
    assert_eq!(m.log, [format!("blocking {b}"), "next_waypoint".to_string()]);
    assert_eq!(m.u(u).cx.mCollisionState, 2);
}

#[test]
fn next_waypoint_cut_corners() {
    let mut m = Mock::map(10, 10);
    let c = m.class(|_| {});
    let u = m.add(2.5, 2.5, c);
    let b = m.add(2.5, 3.5, c);
    m.u(b).passable = false;
    m.on_tile.insert((2, 3), vec![b]);
    m.u(u).waypoints = vec![Ge3DPoint { x: 3.5, y: 3.5, z: 0.0 }, Ge3DPoint { x: 3.5, y: 3.5, z: 0.0 }];
    // one corner only: not blocked
    next_waypoint(&mut m, c, u);
    assert_eq!(m.log, ["next_waypoint"]);
    // the second corner off the map: blocked by the first corner's unit
    m.tiles.remove(&(3, 2));
    m.log.clear();
    next_waypoint(&mut m, c, u);
    assert_eq!(m.log, [format!("blocking {b}"), "next_waypoint".to_string()]);
}

#[test]
fn has_arrived_cases() {
    let mut m = Mock::map(10, 10);
    let c = m.class(|c| c.mInnerCollisionSphere = 0.5);
    let u = m.add(2.5, 2.5, c);
    assert!(!base_has_arrived(&mut m, c, u, 0.01));
    m.u(u).waypoints = vec![Ge3DPoint { x: 2.6, y: 2.6, z: 0.0 }];
    assert!(base_has_arrived(&mut m, c, u, 0.01));
    assert!(!base_has_arrived(&mut m, c, u, 0.3));
    m.u(u).waypoints.push(Ge3DPoint { x: 5.0, y: 5.0, z: 0.0 });
    assert!(base_has_arrived(&mut m, c, u, 0.3));
    assert!(!base_has_arrived(&mut m, c, u, 0.6));
    m.u(u).cx.mCollisionState = 8;
    assert!(base_has_arrived(&mut m, c, u, 9.0));
}

#[test]
fn add_waypoint_at_heights() {
    let mut m = Mock::map(4, 4);
    let c = m.class(|_| {});
    let u = m.add(1.5, 1.5, c);
    m.tiles.insert((1, 1), -1.0);
    add_waypoint_at(&mut m, u, 2.5, 2.5, 1.0, false);
    add_waypoint_at(&mut m, u, 1.5, 1.5, 1.0, false);
    add_waypoint_at(&mut m, u, 9.5, 1.5, 1.0, false);
    add_waypoint_at(&mut m, u, 9.5, 1.5, 1.0, true);
    assert_eq!(m.log, ["waypoint 2.5 2.5 1.5 false", "waypoint 1.5 1.5 1 false", "waypoint 9.5 1.5 3 true"]);
}

#[test]
fn path_toward_steps_at_cruise_altitude() {
    let mut m = Mock::map(20, 20);
    let c = m.class(|c| c.mCruiseAltitude = 2.0);
    let u = m.add(1.5, 1.5, c);
    let from = Ge3DPoint { x: 1.0, y: 1.0, z: 0.0 };
    let to = Ge3DPoint { x: 7.0, y: 1.0, z: 0.0 };
    path_toward(&mut m, u, from, to, false);
    // step 2 (velocity 1 * 2 / angular 1): the line's points but the first, 0.5 + 2 above the ground
    let mut points = Vec::new();
    from.interpolate_to(to, 2.0, |p| points.push(p));
    assert!(points.len() > 1);
    let want: Vec<String> = points.iter().skip(1).map(|p| format!("waypoint {} {} 2.5 false", p.x, p.y)).collect();
    assert_eq!(m.log, want);
    m.log.clear();
    path_toward(&mut m, u, from, Ge3DPoint { x: 1.001, y: 1.0, z: 0.25 }, true);
    assert_eq!(m.log, ["waypoint 1.001 1 0.75 true"]);
}

#[test]
fn path_to_goal_requests_or_cancels() {
    let mut m = Mock::map(20, 20);
    let c = m.class(|_| {});
    let u = m.add(1.5, 1.5, c);
    let g = m.goal(MockGoal {
        kind: MV_GOAL_LOCATION,
        location: Ge3DPoint { x: 1.5, y: 1.5, z: 0.0 },
        group_path_state: 1,
        ..Default::default()
    });
    m.u(u).goal = g;
    path_to_goal(&mut m, c, u);
    assert_eq!(m.log, ["drop_path", "cancel"]);
    assert_eq!(m.u(u).cx.mCurrentPhysicsAction, 4);
    m.g(g).location = Ge3DPoint { x: 8.5, y: 3.5, z: 0.0 };
    m.u(u).cx.mInitialContactHeading = MV_HEADING_UNSET;
    m.log.clear();
    path_to_goal(&mut m, c, u);
    assert_eq!(m.log, ["drop_path", "contact 1.5 1.5", "request 8 3 15 true false"]);
    assert_eq!(m.u(u).cx.mInitialContactHeading, 0.0);
    assert_eq!(m.g(g).group_path_state, 2);
    m.log.clear();
    m.g(g).auto_waypoint = true;
    path_to_goal(&mut m, c, u);
    assert_eq!(m.log, ["drop_path", "request 8 3 -1 false true"]);
}

#[test]
fn flee_runs_or_idles() {
    let mut m = Mock::map(20, 20);
    let c = m.class(|_| {});
    let u = m.add(5.5, 5.5, c);
    flee(&mut m, c, u);
    assert_eq!(m.log, ["action 5000".to_string(), format!("changed {c:#x}")]);
    let e = m.add(6.5, 5.5, c);
    m.pursuer = e;
    m.log.clear();
    flee(&mut m, c, u);
    // distance lrintf(3 * 1.5) = 4 (half to even), direction 6 turned to 2: x 5.5 + 8 = 13.5
    assert_eq!(
        m.log,
        [
            "turn_away 6 true".to_string(),
            "flee 13.5 5.5 false".to_string(),
            "action 8000".to_string(),
            format!("changed {c:#x}")
        ]
    );
    // a computer animal (player 0) draws its distance first
    m.u(u).player_id = 0;
    m.log.clear();
    flee(&mut m, c, u);
    assert_eq!(m.log[0], "random");
}

#[test]
fn flee_attacker_releases_the_attacker() {
    let mut m = Mock::map(20, 20);
    let c = m.class(|_| {});
    let u = m.add(5.5, 5.5, c);
    let e = m.add(6.5, 5.5, c);
    m.u(u).last_attacker = e;
    // every spot a wall: idle
    for x in 0..20 {
        m.walls.push([x, 5]);
    }
    flee_attacker(&mut m, c, u);
    assert_eq!(m.log.last().map(String::as_str), Some("clear_last_attacker"));
    assert!(has(&m, "action 5000"));
    assert_eq!(m.u(u).last_attacker, 0);
}
