use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c, d); its answer from `answers` keyed by (op, a, b, c) (0
/// else); `floats` set `f[0]` for an op.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, u64, i64, i64)>,
    answers: BTreeMap<(i32, u64, u64, i64), i64>,
    floats: BTreeMap<i32, f32>,
    rust: Vec<&'static str>,
}

impl Mock {
    fn answer(mut self, op: i32, a: u64, b: u64, c: i64, v: i64) -> Self {
        self.answers.insert((op, a, b, c), v);
        self
    }
    fn float(mut self, op: i32, v: f32) -> Self {
        self.floats.insert(op, v);
        self
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
    fn asked(&self, op: i32) -> Vec<(i32, u64, u64, i64, i64)> {
        self.log.iter().filter(|e| e.0 == op).copied().collect()
    }
}

impl UhHost for Mock {
    fn uh(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, f: &mut [f32; 4]) -> i64 {
        self.log.push((op, a, b, c, d));
        if let Some(v) = self.floats.get(&op) {
            f[0] = *v;
        }
        *self.answers.get(&(op, a, b, c)).unwrap_or(&0)
    }
    fn pg_nearest(&mut self, _group: u64, x: i32, y: i32) -> u64 {
        self.rust.push("nearest");
        (x * 100 + y) as u64
    }
    fn pg_new(&mut self) -> u64 {
        self.rust.push("new");
        70
    }
    fn pg_add_point(&mut self, _group: u64, _x: f32, _y: f32, _player: u32) {
        self.rust.push("add");
    }
    fn pg_dereference(&mut self, _group: u64) {
        self.rust.push("deref");
    }
    fn plan_wall(&mut self, _start: (f32, f32), _end: (f32, f32), _class: u64, _player: u64, _pieces: u64) {
        self.rust.push("plan");
    }
    fn farm_drop_sites(&mut self, _farm: u64, _sites: u64) -> bool {
        self.rust.push("sites");
        true
    }
}

#[test]
fn the_town_patrol_point_is_the_routes_nearest() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UH_NEAREST_TOWN, 5, 0, 0, 40)
        .answer(EE_UNITS_UH_TOWN_PATROL_GROUP, 0, 40, 0, 41)
        .answer(EE_UNITS_UH_TILE_X, 5, 0, 0, 3)
        .answer(EE_UNITS_UH_TILE_Y, 5, 0, 0, 4)
        .answer(EE_UNITS_UH_OBJECT_ID, 0, 304, 0, 77);
    assert_eq!(town_patrol_point(&mut h, 5), Some(77));
    let mut h = Mock::default();
    assert_eq!(town_patrol_point(&mut h, 5), None);
}

#[test]
fn orders_run_their_statements_in_order() {
    let mut h = Mock::default();
    unit_order_repair(&mut h, 1, 2);
    assert_eq!(h.ops(), vec![EE_UNITS_UH_ORDER_REPAIR, EE_UNITS_UH_START_ACTION, EE_UNITS_UH_ACTION_CHANGED]);
    assert_eq!(h.log[1].3, 5000);
    let mut h = Mock::default();
    unit_add_build_target(&mut h, 1, 2);
    assert_eq!(h.ops(), vec![EE_UNITS_UH_ADD_BUILD_TARGET, EE_UNITS_UH_SET_SHIFT_QUEUED]);
    let mut h = Mock::default();
    patrol_new_lone(&mut h, 9, 1.0, 2.0);
    assert_eq!(h.rust, vec!["new", "add", "deref"]);
    assert_eq!(h.asked(EE_UNITS_UH_ADD_PATROL_GROUP), vec![(EE_UNITS_UH_ADD_PATROL_GROUP, 9, 70, 0, 0)]);
}

#[test]
fn a_point_off_the_map_goes_to_the_closest_tile() {
    // (-0.7, 2.5): tile (0, 2) missing; the closest is (1, 2) at height -1
    let mut h = Mock::default()
        .answer(EE_UNITS_UH_CLOSEST_TILE, 0, 0, 0, 50)
        .answer(EE_UNITS_UH_X, 0, 0, 0, 1)
        .answer(EE_UNITS_UH_Y, 0, 0, 0, 2)
        .float(EE_UNITS_UH_TILE_HEIGHT, -1.0);
    assert_eq!(path_to_point(&mut h, 3, -0.7, 2.5), (1.5, 2.5, 0.0));
    let first = h.asked(EE_UNITS_UH_MAP_TILE)[0];
    assert_eq!((first.3, first.4), (0, 2));
}

#[test]
fn a_blocking_building_moves_the_point_to_its_footprint() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UH_MAP_TILE, 0, 0, 4, 60)
        .answer(EE_UNITS_UH_TILE_UNITS, 0, 60, 0, 61)
        .answer(EE_UNITS_UH_NODE_BLOCKS, 3, 61, 0, 1)
        .answer(EE_UNITS_UH_X, 0, 0, 0, 6)
        .answer(EE_UNITS_UH_Y, 0, 0, 0, 7)
        .answer(EE_UNITS_UH_MAP_TILE, 0, 0, 6, 62)
        .float(EE_UNITS_UH_TILE_HEIGHT, 2.0);
    assert_eq!(path_to_point(&mut h, 3, 4.2, 5.9), (6.5, 7.5, 2.0));
    assert!(h.asked(EE_UNITS_UH_NODE_NEXT).is_empty());
    let footprint = h.asked(EE_UNITS_UH_NEAREST_FOOTPRINT)[0];
    assert_eq!((footprint.3, footprint.4), (4, 5));
}

#[test]
fn farms_lists_and_sorting() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UH_COMPLEX, 8, 0, 0, 80)
        .answer(EE_UNITS_UH_FIND_FREE_FARM, 0, 80, 0, 81)
        .answer(EE_UNITS_UH_OBJECT_ID, 0, 81, 0, 9);
    // the mock finds the same farm forever: stop it after two
    let mut out = Vec::new();
    h.answers.insert((EE_UNITS_UH_FIND_FREE_FARM, 0, 80, 0), 81);
    let mut calls = 0;
    // a host whose search ends: count the asks through a wrapper
    struct Ending<'a>(&'a mut Mock, &'a mut i32);
    impl UhHost for Ending<'_> {
        fn uh(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, f: &mut [f32; 4]) -> i64 {
            if op == EE_UNITS_UH_FIND_FREE_FARM {
                *self.1 += 1;
                if *self.1 > 2 {
                    return 0;
                }
            }
            self.0.uh(op, a, b, c, d, f)
        }
        fn pg_nearest(&mut self, g: u64, x: i32, y: i32) -> u64 {
            self.0.pg_nearest(g, x, y)
        }
        fn pg_new(&mut self) -> u64 {
            self.0.pg_new()
        }
        fn pg_add_point(&mut self, g: u64, x: f32, y: f32, p: u32) {
            self.0.pg_add_point(g, x, y, p)
        }
        fn pg_dereference(&mut self, g: u64) {
            self.0.pg_dereference(g)
        }
        fn plan_wall(&mut self, s: (f32, f32), e: (f32, f32), c: u64, p: u64, l: u64) {
            self.0.plan_wall(s, e, c, p, l)
        }
        fn farm_drop_sites(&mut self, f: u64, s: u64) -> bool {
            self.0.farm_drop_sites(f, s)
        }
    }
    free_farms(&mut Ending(&mut h, &mut calls), 8, &mut out);
    assert_eq!(out, vec![9, 9]);
    assert_eq!(h.asked(EE_UNITS_UH_RESET_RING).len(), 1);
    // an unknown farm leaves the order
    let mut h = Mock::default().answer(EE_UNITS_UH_COMPLEX, 1, 0, 0, 10);
    let mut farms = [1, 2];
    sort_farms(&mut h, &mut farms, 0, 0, true);
    assert_eq!(farms, [1, 2]);
    assert!(h.asked(EE_UNITS_UH_SORT_FARMS).is_empty());
    let mut h = Mock::default()
        .answer(EE_UNITS_UH_COMPLEX, 1, 0, 0, 10)
        .answer(EE_UNITS_UH_COMPLEX, 2, 0, 0, 20)
        .answer(EE_UNITS_UH_UNITS_ID, 0, 0, 0, 2)
        .answer(EE_UNITS_UH_UNITS_ID, 0, 0, 1, 1);
    sort_farms(&mut h, &mut farms, 3, 4, true);
    assert_eq!(farms, [2, 1]);
    assert_eq!(h.asked(EE_UNITS_UH_SORT_FARMS), vec![(EE_UNITS_UH_SORT_FARMS, 0, 1, 3, 4)]);
}

#[test]
fn drop_sites_use_a_list_of_their_own() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UH_NEW_PIECES, 0, 0, 0, 90)
        .answer(EE_UNITS_UH_PIECES_SIZE, 0, 90, 0, 1)
        .answer(EE_UNITS_UH_PIECE_ID, 0, 90, 0, 33);
    let mut out = vec![5];
    assert!(farm_drop_sites(&mut h, 4, &mut out));
    assert_eq!(out, vec![33]);
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_UH_DELETE_PIECES);
}

#[test]
fn the_garrison_rally_load() {
    let mut h = Mock::default().answer(EE_UNITS_UH_NEW_GARRISON_LOAD, 6, 0, 0, 60);
    assert_eq!(garrison_rally_load(&mut h, 6), 0);
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_UH_DELETE_GOAL);
    let mut h = Mock::default()
        .answer(EE_UNITS_UH_NEW_GARRISON_LOAD, 6, 0, 0, 60)
        .answer(EE_UNITS_UH_LOAD_VALID, 0, 60, 0, 1)
        .answer(EE_UNITS_UH_GOAL_CLONE, 0, 60, 0, 61);
    assert_eq!(garrison_rally_load(&mut h, 6), 61);
    assert_eq!(h.asked(EE_UNITS_UH_C_START_ACTION)[0].3, ACTION_PICKED_UP_LOADABLE_UNITS);
}

#[test]
fn load_goals_traits_and_research() {
    let mut h = Mock::default().answer(EE_UNITS_UH_GOAL_STATE, 3, 0, 0, 30).answer(EE_UNITS_UH_GOAL_KIND, 0, 30, 0, 4);
    assert_eq!(unit_load_goal(&mut h, 3, GOAL_LOAD_GARRISON), 0);
    let mut h = Mock::default().answer(EE_UNITS_UH_IS_TRANSPORT, 3, 0, 0, 1).float(EE_UNITS_UH_MAX_VELOCITY, 1.0);
    assert_eq!(unit_load_traits(&mut h, 3), EE_UNITS_LOAD_TRANSPORT | EE_UNITS_LOAD_MOVES);
    let mut h = Mock::default()
        .answer(EE_UNITS_UH_TREE_NODE, 9, 0, 2, 50)
        .answer(EE_UNITS_UH_NODE_KIND, 0, 50, 0, 3)
        .answer(EE_UNITS_UH_IS_EPOCH, 0, 0, 2, 1);
    assert_eq!(research_node(&mut h, 9, 2), (EE_UNITS_RESEARCH_FOUND | EE_UNITS_RESEARCH_EPOCH, 3));
    let mut h = Mock::default()
        .answer(EE_UNITS_UH_UNITS_SIZE, 0, 0, 0, 1)
        .answer(EE_UNITS_UH_UNITS_ID, 0, 0, 0, 4)
        .answer(EE_UNITS_UH_REJECTED_SIZE, 0, 0, 0, 1)
        .answer(EE_UNITS_UH_REJECTED_ID, 0, 0, 0, 5);
    assert_eq!(transport_filter(&mut h, 1, &[4, 5]), (vec![4], vec![5]));
}

#[test]
fn the_cheats_units() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UH_CREATE_AT, 0, 0, 7, 40)
        .answer(EE_UNITS_UH_HAS_TILE_NODE, 0, 40, 0, 1)
        .answer(EE_UNITS_UH_UPGRADE_CREATION_SOUND, 0, 0, 7, 41)
        .answer(EE_UNITS_UH_CURRENT_PLAYER_ID, 0, 0, 0, 2);
    hack_create_unit(&mut h, 7, 1.0, 2.0, 2);
    assert_eq!(
        h.ops(),
        vec![
            EE_UNITS_UH_RESET_TRANSFORMATION,
            EE_UNITS_UH_GROUND_POINT,
            EE_UNITS_UH_FACE_PI,
            EE_UNITS_UH_POSITION_AT_GROUND,
            EE_UNITS_UH_CREATE_AT,
            EE_UNITS_UH_HAS_TILE_NODE,
            EE_UNITS_UH_CLEAR_RELOAD,
            EE_UNITS_UH_FULL_POWER,
            EE_UNITS_UH_UPGRADE_CREATION_SOUND,
            EE_UNITS_UH_CURRENT_PLAYER_ID,
            EE_UNITS_UH_PLAY_SOUND,
        ]
    );
    let mut h = Mock::default().answer(EE_UNITS_UH_CURRENT_PLAYER_ID, 0, 0, 0, 1);
    hack_create_building(&mut h, 7, 3, 4, 2);
    assert_eq!(h.asked(EE_UNITS_UH_CREATE_BUILDING), vec![(EE_UNITS_UH_CREATE_BUILDING, 7, 2, 3, 4)]);
    assert!(h.asked(EE_UNITS_UH_STATS_CREATION_SOUND).is_empty());
}

#[test]
fn calamities_and_ghosts() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UH_TECH_NODE, 9, 0, 3, 50)
        .answer(EE_UNITS_UH_NODE_CALAMITY_CLASS, 0, 50, 0, 51)
        .answer(EE_UNITS_UH_CLASS_ENUM, 0, 51, 0, 6);
    assert_eq!(calamity_of(&mut h, 9, 3), (51, 6, false));
    let mut h = Mock::default();
    calamity_can_cast_at(&mut h, 51, 1, -1, 2, NONE);
    let cast = h.asked(EE_UNITS_UH_CAN_CAST_AT)[0];
    assert_eq!((cast.3 as u32 as i32, (cast.3 >> 32) as i32, cast.4), (-1, 2, i64::from(NONE)));
    let mut h = Mock::default()
        .answer(EE_UNITS_UH_UNIT_TILE, 4, 0, 0, 40)
        .answer(EE_UNITS_UH_UNIT_OF, 4, 0, 0, 44)
        .answer(EE_UNITS_UH_TILE_DOPPELGANGERS, 0, 40, 0, 2)
        .answer(EE_UNITS_UH_TILE_DOPPELGANGER_UNIT, 0, 40, 1, 44);
    assert!(unit_ghost_seen(&mut h, 4));
    let mut h = Mock::default();
    assert!(!unit_ghost_seen(&mut h, 4));
    assert_eq!(map_height(&mut Mock::default().float(EE_UNITS_UH_MAP_HEIGHT, 3.5), 1.0, 2.0), 3.5);
}
