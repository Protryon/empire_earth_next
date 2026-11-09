//! The path goals' and modifiers' tests: a small world behind the views, logging every query, so each test pins both
//! the answer and the C++'s order of questions.

use crate::goals::{View, TERRAIN_CLIFF, TERRAIN_DEEP_WATER, TERRAIN_SPACE};
use crate::modifiers::{self, *};
use crate::path_goals::{self, *};
use crate::storage::AStarRect;
use std::ffi::c_void;

const W: Unit = 0x10 as Unit;
const MODIFIER: *mut c_void = 0x20 as *mut c_void;

/// A map of `rows` (x across, y down): '.' land, '~' deep water, 's' shallow water, '#' cliff, '*' space, 'X' a unit
/// the walker cannot pass, 'B' a step blocker on land, 'b' a step blocker on shallow water, 'W' a wall, 'T' a tree.
struct World {
    rows: Vec<&'static [u8]>,
    ai: Vec<Vec<i32>>,
    flags: Vec<Vec<i32>>,
    q: Vec<String>,
    rank: i32,
    epoch: i32,
    alone: bool,
    water_state: i32,
    sea_going: bool,
    wide: bool,
    ignores_formation: bool,
    regions_empty: bool,
    draws: Vec<f32>,
    crowd: i32,
    enterable: bool,
    mod_cost: i32,
}

impl World {
    fn new(rows: Vec<&'static [u8]>) -> World {
        let n = rows.len();
        World {
            rows,
            ai: vec![vec![0; n]; n],
            flags: vec![vec![0; n]; n],
            q: Vec::new(),
            rank: 0,
            epoch: 1,
            alone: true,
            water_state: 0,
            sea_going: false,
            wide: false,
            ignores_formation: false,
            regions_empty: false,
            draws: Vec::new(),
            crowd: 0,
            enterable: true,
            mod_cost: 7,
        }
    }
    fn n(&self) -> i32 {
        self.rows.len() as i32
    }
    fn at(&self, x: i32, y: i32) -> u8 {
        self.rows[y as usize][x as usize]
    }
    fn on(&self, x: i32, y: i32) -> bool {
        x >= 0 && y >= 0 && x < self.n() && y < self.n()
    }
}

impl View for World {
    fn has_tile(&mut self, x: i32, y: i32) -> bool {
        self.q.push(format!("has {x} {y}"));
        self.on(x, y)
    }
    fn terrain(&mut self, x: i32, y: i32) -> i32 {
        self.q.push(format!("terrain {x} {y}"));
        match self.at(x, y) {
            b'~' => TERRAIN_DEEP_WATER,
            b's' | b'b' => TERRAIN_SHALLOW_WATER,
            b'#' => TERRAIN_CLIFF,
            b'*' => TERRAIN_SPACE,
            _ => 0,
        }
    }
    fn is_edge(&mut self, x: i32, y: i32) -> bool {
        self.q.push(format!("edge {x} {y}"));
        x == 0 || y == 0 || x == self.n() - 1 || y == self.n() - 1
    }
    fn height_level(&mut self, _x: i32, _y: i32) -> i32 {
        unreachable!()
    }
    fn continent_id(&mut self, _x: i32, _y: i32) -> u16 {
        unreachable!()
    }
    fn choke_tile(&mut self, _index: i32) -> Option<u32> {
        unreachable!()
    }
}

impl ModView for World {
    fn ai_value(&mut self, x: i32, y: i32) -> i32 {
        self.q.push(format!("ai {x} {y}"));
        self.ai[y as usize][x as usize]
    }
    fn tile_flag(&mut self, x: i32, y: i32) -> i32 {
        self.q.push(format!("flag {x} {y}"));
        self.flags[y as usize][x as usize]
    }
    fn can_move_on(&mut self, walker: Unit, terrain: i32) -> bool {
        assert_eq!(walker, W);
        self.q.push(format!("move {terrain}"));
        terrain != TERRAIN_DEEP_WATER && terrain != TERRAIN_CLIFF && terrain != TERRAIN_SPACE
    }
    fn are_passable(&mut self, walker: Unit, x: i32, y: i32, skip: Skip) -> bool {
        assert_eq!(walker, W);
        self.q.push(format!("pass {x} {y} {skip:?}"));
        match self.at(x, y) {
            b'X' => false,
            b'W' => skip == Skip::Walls,
            b'T' => skip == Skip::Trees,
            _ => true,
        }
    }
    fn formation_rank(&mut self, _walker: Unit) -> i32 {
        self.q.push("rank".into());
        self.rank
    }
    fn step_blocker(&mut self, _walker: Unit, sea: bool, x: i32, y: i32) -> bool {
        self.q.push(format!("blocker {sea} {x} {y}"));
        matches!(self.at(x, y), b'B' | b'b')
    }
    fn unit_region_state(&mut self, x: i32, y: i32) -> i32 {
        self.q.push(format!("region {x} {y}"));
        if self.regions_empty {
            0
        } else {
            1
        }
    }
    fn epoch(&mut self, _walker: Unit) -> i32 {
        self.q.push("epoch".into());
        self.epoch
    }
    fn is_alone(&mut self, _walker: Unit) -> bool {
        self.q.push("alone".into());
        self.alone
    }
    fn water_state(&mut self, _walker: Unit) -> i32 {
        self.q.push("water".into());
        self.water_state
    }
    fn is_sea_going(&mut self, _walker: Unit) -> bool {
        self.q.push("sea".into());
        self.sea_going
    }
    fn has_hill(&mut self, x: i32, y: i32) -> bool {
        self.q.push(format!("hill {x} {y}"));
        x == 1 && y == 1
    }
    fn random_at_most(&mut self, chance: f32) -> bool {
        self.q.push(format!("draw {chance}"));
        self.draws.remove(0) <= chance
    }
}

impl PathView for World {
    fn crowd_cost(&mut self, _walker: Unit, x: i32, y: i32) -> i32 {
        self.q.push(format!("crowd {x} {y}"));
        self.crowd
    }
    fn is_wide(&mut self, _walker: Unit) -> bool {
        self.q.push("wide".into());
        self.wide
    }
    fn ignores_formation(&mut self, _walker: Unit) -> bool {
        self.q.push("ignores".into());
        self.ignores_formation
    }
    fn rank_probe_blocked(&mut self, _walker: Unit, continent_id: u16, x: i32, y: i32) -> bool {
        self.q.push(format!("probe {continent_id} {x} {y}"));
        self.on(x, y) && self.at(x, y) == b'#'
    }
    fn can_enter(&mut self, modifier: *mut c_void, x: i32, y: i32, at_goal: bool) -> bool {
        assert_eq!(modifier, MODIFIER);
        self.q.push(format!("enter {x} {y} {at_goal}"));
        self.enterable
    }
    fn modifier_cost(&mut self, modifier: *mut c_void, fx: i32, fy: i32, x: i32, y: i32, px: i32, py: i32) -> i32 {
        assert_eq!(modifier, MODIFIER);
        self.q.push(format!("mcost {fx} {fy} {x} {y} {px} {py}"));
        self.mod_cost
    }
}

fn rect(left: i32, top: i32, right: i32, bottom: i32) -> AStarRect {
    AStarRect { bottom, left, right, top }
}

fn step_goal(avoid_units: bool, is_bounded: bool) -> EeAStarStepGoal {
    EeAStarStepGoal { walker: W, avoid_units, is_bounded, goal_bounds: rect(0, 0, 0, 0) }
}

const OPEN: [&[u8]; 7] = [b".......", b".......", b".......", b".......", b".......", b".......", b"......."];

#[test]
fn tile_in_direction_is_the_cpp_switch() {
    let want = [(-2, 5), (-2, 3), (-2, 1), (0, 1), (0, 5), (2, 5), (2, 3), (2, 1), (0, 3)];
    for (d, w) in want.iter().enumerate() {
        assert_eq!(tile_in_direction(0, 3, d as i32, 2), *w);
    }
    assert_eq!(tile_in_direction(i32::MAX, 0, 6, 1), (i32::MIN, 0));
}

#[test]
fn step_cost_straight_diagonal_and_turns() {
    let mut v = World::new(OPEN.to_vec());
    let g = step_goal(false, false);
    // straight on, no turn: 10
    assert_eq!(step_cost(&mut v, &g, true, 2, 2, 3, 2, 1, 2), 10);
    assert_eq!(v.q, ["ai 3 2", "rank", "blocker true 3 2"]);
    // diagonal after a straight step: 14 + 6
    assert_eq!(step_cost(&mut v, &g, true, 2, 2, 3, 3, 1, 2), 20);
    // no parent (-10000s): always a turn
    assert_eq!(step_cost(&mut v, &g, false, 2, 2, 3, 2, -10000, -10000), 16);
}

#[test]
fn step_cost_blockers_multiply() {
    let rows: Vec<&'static [u8]> =
        vec![b".......", b".......", b"..B....", b"...B...", b".......", b".....B.", b"......."];
    let mut v = World::new(rows);
    // onto a blocker: x5
    let g = step_goal(false, false);
    assert_eq!(step_cost(&mut v, &g, false, 2, 3, 3, 3, 1, 3), 50);
    // avoiding units, margin 1: the blockers at (2, 2) and (3, 3) around (3, 3) (itself counts), not the from tile
    let g = step_goal(true, false);
    v.q.clear();
    assert_eq!(step_cost(&mut v, &g, false, 4, 4, 3, 3, 5, 5), 14 * 5 * 2 * 2);
    assert_eq!(&v.q[..4], ["ai 3 3", "rank", "blocker false 3 3", "epoch"]);
    assert_eq!(v.q[4], "region 3 3"); // the first region is not empty: no more region queries
    assert_eq!(v.q[5], "has 2 2");
    assert!(!v.q.iter().any(|q| q == "has 4 4")); // the from tile
                                                  // epoch 9 unbounded: margin 2, and the far corner (5, 5) counts x3
    v.epoch = 9;
    assert_eq!(step_cost(&mut v, &g, false, 4, 4, 3, 3, 5, 5), 14 * 5 * 2 * 2 * 3);
    // bounded: the blockers are looked at but cost nothing, and the margin stays 1
    let g = step_goal(true, true);
    assert_eq!(step_cost(&mut v, &g, false, 4, 4, 3, 3, 5, 5), 14 * 5);
    // every region empty: no tile is looked at
    v.regions_empty = true;
    v.q.clear();
    let g = step_goal(true, false);
    assert_eq!(step_cost(&mut v, &g, false, 4, 4, 3, 3, 5, 5), 14 * 5);
    assert_eq!(v.q.iter().filter(|q| q.starts_with("region")).count(), 5);
    assert!(!v.q.iter().any(|q| q.starts_with("has")));
    assert!(v.q.contains(&"region 1 1".to_string()) && v.q.contains(&"region 5 1".to_string()));
}

#[test]
fn step_cost_slope_and_rank() {
    let rows: Vec<&'static [u8]> =
        vec![b"~~~~~~~", b"~~~~~~~", b"~~~~~~~", b"~~~~~~~", b"~~~~~~.", b"~~~~~~~", b"~~~~~~~"];
    let mut v = World::new(rows);
    v.ai[3][3] = -4;
    // no rank: the slope only for a lone walker outside the goal bounds
    let mut g = step_goal(false, false);
    assert_eq!(step_cost(&mut v, &g, true, 2, 3, 3, 3, 1, 3), 14);
    v.alone = false;
    assert_eq!(step_cost(&mut v, &g, true, 2, 3, 3, 3, 1, 3), 10);
    v.alone = true;
    g.goal_bounds = rect(3, 3, 3, 3);
    assert_eq!(step_cost(&mut v, &g, true, 2, 3, 3, 3, 1, 3), 10);
    // ranked: the slope and x2, no probes
    v.rank = 2;
    v.q.clear();
    assert_eq!(step_cost(&mut v, &g, true, 2, 3, 3, 3, 1, 3), 28);
    assert!(!v.q.iter().any(|q| q.starts_with("has")));
    // ranked on flat water: probes (3, 5), (3, 1), (1, 3), (5, 3) are water, so no doubling
    v.ai[3][3] = 0;
    v.q.clear();
    assert_eq!(step_cost(&mut v, &g, true, 2, 3, 3, 3, 1, 3), 10);
    assert_eq!(v.q.iter().filter(|q| q.starts_with("terrain")).count(), 4);
    // a probe on land (6, 4) blocks: x2, and the probes stop there
    v.q.clear();
    assert_eq!(step_cost(&mut v, &g, true, 3, 4, 4, 4, 2, 4), 20);
    assert_eq!(v.q.last().unwrap(), "terrain 6 4");
    // a space walker on water is blocked by the first probe; an AI value blocks without asking the terrain
    assert_eq!(step_cost(&mut v, &g, false, 2, 3, 3, 3, 1, 3), 20);
    v.ai[5][3] = 1;
    v.q.clear();
    assert_eq!(step_cost(&mut v, &g, true, 2, 3, 3, 3, 1, 3), 20);
    assert_eq!(&v.q[v.q.len() - 2..], ["has 3 5", "ai 3 5"]);
    // bounded: no rank rules
    let g = step_goal(false, true);
    assert_eq!(step_cost(&mut v, &g, true, 2, 3, 3, 3, 1, 3), 10);
}

#[test]
fn passable_tests_in_order() {
    let rows: Vec<&'static [u8]> = vec![b".....", b".~X..", b".WTs.", b".....", b"....."];
    let mut v = World::new(rows);
    assert!(!is_passable(&mut v, W, 0, 2));
    assert_eq!(v.q, ["edge 0 2"]);
    assert!(!is_passable(&mut v, W, 1, 1)); // water
    assert!(!is_passable(&mut v, W, 2, 1)); // a unit
    assert!(is_passable(&mut v, W, 3, 2));
    // the water state against the tile flag
    v.flags[2][3] = -1;
    v.q.clear();
    assert!(is_passable(&mut v, W, 3, 2)); // state 0
    assert_eq!(v.q, ["edge 3 2", "terrain 3 2", "move 10", "pass 3 2 None", "flag 3 2", "water"]);
    v.water_state = 2;
    assert!(!is_passable(&mut v, W, 3, 2));
    v.flags[2][3] = 1;
    assert!(is_passable(&mut v, W, 3, 2));
    v.water_state = 3;
    assert!(!is_passable(&mut v, W, 3, 2));
    // the attacker asks the water state only when sea-going
    v.q.clear();
    assert!(attacker_can_enter(&mut v, W, 3, 2));
    assert_eq!(v.q.last().unwrap(), "sea");
    v.sea_going = true;
    assert!(!attacker_can_enter(&mut v, W, 3, 2));
    // walls and trees
    assert!(!is_passable(&mut v, W, 1, 2));
    assert!(walls_can_enter(&mut v, W, 1, 2));
    assert!(!walls_can_enter(&mut v, W, 2, 2));
    assert!(wall_pather_can_enter(&mut v, W, 2, 2));
    assert!(!wall_pather_can_enter(&mut v, W, 1, 2));
    // the pather takes shallow water without asking the walker's terrain
    v.q.clear();
    assert!(wall_pather_can_enter(&mut v, W, 3, 2));
    assert_eq!(v.q, ["edge 3 2", "terrain 3 2", "pass 3 2 Trees"]);
    assert_eq!(modifiers::straight_or_diagonal(1, 1, 2, 1), 10);
    assert_eq!(modifiers::straight_or_diagonal(1, 1, 2, 2), 14);
}

#[test]
fn storm_costs_and_draws() {
    let mut v = World::new(vec![b".....", b".#~s.", b".....", b".....", b"....."]);
    let mut storm = EeAStarStorm { snow: true, ..Default::default() };
    assert_eq!(storm_cost(&mut v, &storm, 1, 1), 1); // cliff
    assert_eq!(storm_cost(&mut v, &storm, 2, 1), 40); // deep water
    assert_eq!(storm_cost(&mut v, &storm, 3, 1), 5);
    assert_eq!(storm_cost(&mut v, &storm, 9, 9), 40); // off the map
    v.q.clear();
    assert_eq!(storm_cost(&mut v, &storm, 0, 0), 5);
    assert_eq!(v.q, ["has 0 0", "terrain 0 0", "hill 0 0"]);
    storm.snow = false;
    storm.rain = true;
    assert_eq!(storm_cost(&mut v, &storm, 2, 1), 1);
    assert_eq!(storm_cost(&mut v, &storm, 3, 1), 1);
    assert_eq!(storm_cost(&mut v, &storm, 0, 0), 40);
    storm.rain = false;
    assert_eq!(storm_cost(&mut v, &storm, 2, 1), 40);

    let mut v = World::new(OPEN.to_vec());
    let storm = EeAStarStorm {
        inner: rect(2, 2, 3, 3),
        outer: rect(1, 1, 4, 4),
        distance_squared: 4,
        start_x: 3,
        start_y: 3,
        ..Default::default()
    };
    assert!(storm_can_enter(&mut v, &storm, 3, 2)); // inner: no draw
    assert!(!storm_can_enter(&mut v, &storm, 9, 2)); // off the map: no draw
    assert!(v.q.iter().all(|q| !q.starts_with("draw")));
    v.draws = vec![0.83, 0.84, 0.685, 0.2326];
    assert!(storm_can_enter(&mut v, &storm, 1, 1)); // outer
    assert!(!storm_can_enter(&mut v, &storm, 4, 4));
    assert!(storm_can_enter(&mut v, &storm, 5, 3)); // near: 2 * 2 <= 4
    assert!(!storm_can_enter(&mut v, &storm, 6, 6)); // far
    let draws: Vec<_> = v.q.iter().filter(|q| q.starts_with("draw")).cloned().collect();
    assert_eq!(draws, ["draw 0.83", "draw 0.83", "draw 0.685", "draw 0.2325"]);
}

fn path_goal(bounded: bool, modifier: bool) -> EeAStarPathGoal {
    EeAStarPathGoal {
        bounded,
        bounds: rect(1, 1, 4, 4),
        goal_x: 4,
        goal_y: 4,
        walker: W,
        modifier: if modifier { MODIFIER } else { std::ptr::null_mut() },
        continent_id: 3,
        check_corners: true,
        accept_nearest: true,
        search_failed: false,
        took_nearest: false,
        revs: 0,
        max_revs: 100,
    }
}

#[test]
fn path_accept_default_modifier_and_window() {
    let rows: Vec<&'static [u8]> =
        vec![b".......", b".~.....", b"..X....", b".......", b".......", b".......", b"......."];
    let mut v = World::new(rows);
    let g = path_goal(false, false);
    assert!(!path_goals::accept_tile(&g, &mut v, 7, 0, false));
    assert_eq!(v.q, ["has 7 0"]);
    assert!(!path_goals::accept_tile(&g, &mut v, 0, 3, false)); // edge
    assert!(!path_goals::accept_tile(&g, &mut v, 1, 1, false)); // water
    assert!(!path_goals::accept_tile(&g, &mut v, 2, 2, false)); // a unit
    v.q.clear();
    assert!(path_goals::accept_tile(&g, &mut v, 5, 5, false));
    assert_eq!(v.q, ["has 5 5", "edge 5 5", "terrain 5 5", "move 0", "pass 5 5 None"]);
    // bounded: outside the window, nothing more is asked
    let b = path_goal(true, false);
    v.q.clear();
    assert!(!path_goals::accept_tile(&b, &mut v, 5, 5, false));
    assert_eq!(v.q, ["has 5 5"]);
    assert!(path_goals::accept_tile(&b, &mut v, 4, 4, false));
    // a modifier decides alone
    let m = path_goal(false, true);
    v.q.clear();
    v.enterable = false;
    assert!(!path_goals::accept_tile(&m, &mut v, 5, 5, false));
    assert_eq!(v.q, ["has 5 5", "enter 5 5 false"]);
}

#[test]
fn path_costs() {
    let rows: Vec<&'static [u8]> =
        vec![b".......", b".......", b".......", b".......", b"...#...", b".......", b"......."];
    let mut v = World::new(rows);
    v.crowd = 50;
    let g = path_goal(false, false);
    assert_eq!(path_goals::get_cost(&g, &mut v, 1, 1, 2, 1, None), 60);
    assert_eq!(v.q, ["crowd 2 1", "wide"]);
    // no crowd cost onto the goal tile
    assert_eq!(path_goals::get_cost(&g, &mut v, 3, 3, 4, 4, None), 14);
    // wide, ranked: probes (inX, inY + r), (inX, inY - r), (inX - r, inY), (inX + r, inY) until one is blocked
    v.wide = true;
    v.rank = 1;
    v.q.clear();
    assert_eq!(path_goals::get_cost(&g, &mut v, 1, 1, 2, 1, None), 60);
    assert_eq!(
        v.q,
        ["crowd 2 1", "wide", "rank", "ignores", "probe 3 2 2", "probe 3 2 0", "probe 3 1 1", "probe 3 3 1"]
    );
    v.q.clear();
    assert_eq!(path_goals::get_cost(&g, &mut v, 3, 2, 3, 3, None), 120); // (3, 4) is a cliff
    assert_eq!(v.q.len(), 5);
    v.ignores_formation = true;
    assert_eq!(path_goals::get_cost(&g, &mut v, 3, 2, 3, 3, None), 60);
    // with a modifier: its cost, the parent or -10000s
    let m = path_goal(false, true);
    v.q.clear();
    assert_eq!(path_goals::get_cost(&m, &mut v, 1, 1, 2, 1, Some((0, 1))), 7);
    assert_eq!(path_goals::get_cost(&m, &mut v, 1, 1, 2, 1, None), 7);
    assert_eq!(v.q, ["mcost 1 1 2 1 0 1", "mcost 1 1 2 1 -10000 -10000"]);
}

#[test]
fn path_goal_node_and_corners() {
    let rows: Vec<&'static [u8]> =
        vec![b".......", b".......", b".......", b".......", b"....X..", b".....X.", b"......."];
    let mut v = World::new(rows);
    let mut g = path_goal(false, false);
    assert!(path_goals::is_goal_node(&g, &mut v, None));
    assert!(!path_goals::is_goal_node(&g, &mut v, Some((2, 4))));
    assert!(path_goals::is_goal_node(&g, &mut v, Some((4, 4))));
    assert!(v.q.is_empty());
    // a neighbour of the (blocked) goal; a diagonal needs both corners: (3, 4)... corners of (3, 3): (2, 2) and (4, 4)
    assert!(path_goals::is_goal_node(&g, &mut v, Some((4, 3))));
    v.q.clear();
    assert!(!path_goals::is_goal_node(&g, &mut v, Some((3, 3))));
    assert_eq!(
        v.q.iter().filter(|q| q.starts_with("has")).cloned().collect::<Vec<_>>(),
        ["has 4 4", "has 2 2", "has 4 4"]
    );
    g.check_corners = false;
    assert!(path_goals::is_goal_node(&g, &mut v, Some((3, 3))));
    // an enterable goal: its neighbours do not end the search
    g.goal_x = 1;
    g.goal_y = 1;
    assert!(!path_goals::is_goal_node(&g, &mut v, Some((2, 2))));
}

#[test]
fn path_search_done_flags() {
    let mut v = World::new(OPEN.to_vec());
    let mut g = path_goal(false, false);
    path_goals::search_done(&mut g, &mut v, None, true);
    assert!(!g.search_failed && !g.took_nearest);
    path_goals::search_done(&mut g, &mut v, None, false);
    assert!(g.search_failed && g.took_nearest);
    // next to an enterable goal: not failed, until the revs reach the limit (-1: at once)
    let mut g = path_goal(false, false);
    path_goals::search_done(&mut g, &mut v, Some((3, 3)), false);
    assert!(!g.search_failed && g.took_nearest);
    g.revs = 100;
    path_goals::search_done(&mut g, &mut v, Some((1, 1)), false);
    assert!(g.search_failed);
    let mut g = path_goal(false, false);
    g.max_revs = -1;
    path_goals::search_done(&mut g, &mut v, Some((1, 1)), false);
    assert!(g.search_failed && !g.took_nearest);
}
