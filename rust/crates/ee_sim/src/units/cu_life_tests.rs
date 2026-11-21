use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c), its answer from `answers` keyed by (op, a, b) (0 else);
/// `sequences` answer each ask of (op, a, b) in turn; `floats` fill f for an op.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64, i64, [f32; 4])>,
    answers: BTreeMap<(i32, u64, i64), i64>,
    sequences: BTreeMap<(i32, u64, i64), Vec<i64>>,
    floats: BTreeMap<i32, [f32; 4]>,
}

impl Mock {
    fn answer(mut self, op: i32, a: u64, b: i64, value: i64) -> Self {
        self.answers.insert((op, a, b), value);
        self
    }
    fn sequence(mut self, op: i32, a: u64, b: i64, values: &[i64]) -> Self {
        self.sequences.insert((op, a, b), values.to_vec());
        self
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
    fn count(&self, op: i32) -> usize {
        self.log.iter().filter(|e| e.0 == op).count()
    }
    fn asked(&self, op: i32, a: u64, b: i64) -> bool {
        self.log.iter().any(|e| e.0 == op && e.1 == a && e.2 == b)
    }
    fn at(&self, op: i32) -> usize {
        self.log.iter().position(|e| e.0 == op).unwrap()
    }
}

impl CuHost for Mock {
    fn cu(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
        assert_eq!(unit, 1);
        self.log.push((op, a, b, c, *f));
        if let Some(v) = self.floats.get(&op) {
            *f = *v;
        }
        if let Some(values) = self.sequences.get_mut(&(op, a, b)) {
            if !values.is_empty() {
                return values.remove(0);
            }
        }
        *self.answers.get(&(op, a, b)).unwrap_or(&0)
    }
}

#[test]
fn copy_from_clones_the_goals_and_ends_with_the_base_copy() {
    let other = 7;
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_SAME_TILE_SIZE, other, 0, 1)
        .sequence(EE_UNITS_CU_UNIT_GOAL_QUEUE_COUNT, other, 0, &[1, 1])
        .answer(EE_UNITS_CU_UNIT_GOAL_QUEUE_COUNT, 1, 0, 1)
        .sequence(EE_UNITS_CU_UNIT_GOAL_COUNT, other, 0, &[1, 1])
        .answer(EE_UNITS_CU_UNIT_GOAL_AT, other, 0, 40)
        .answer(EE_UNITS_CU_GOAL_KIND_OF, 40, 0, GOAL_UNLOAD_TRANSPORT)
        .answer(EE_UNITS_CU_SAME_PLAYER, other, 0, 1)
        .answer(EE_UNITS_CU_GOAL_CLONE, 40, 0, 41)
        .answer(EE_UNITS_CU_CONTAINER, 0, 0, 90)
        .answer(EE_UNITS_CU_BEHAVIOR, 0, 0, 5);
    copy_from(&mut h, 1, other, 8);
    assert_eq!(h.count(EE_UNITS_CU_COPY_LOCATION), 1);
    assert!(h.asked(EE_UNITS_CU_SET_UNLOAD_TRANSPORT, 40, 0));
    assert!(h.asked(EE_UNITS_CU_QUEUE_PUSH_GOAL, 41, 0));
    assert!(h.asked(EE_UNITS_CU_GOAL_SET_QUEUE_OWNER, 41, 0));
    assert!(h.asked(EE_UNITS_CU_REPLACE_CARGO_WITH_THIS, 90, other as i64));
    // the three record copies in order, the first with its input
    let copies: Vec<i64> = h.log.iter().filter(|e| e.0 == EE_UNITS_CU_COMPLEX_COPY).map(|e| e.2).collect();
    assert_eq!(copies, [0, 1, 2]);
    assert!(h.at(EE_UNITS_CU_MOVE_PASSENGERS) < h.at(EE_UNITS_CU_GOAL_CLONE));
    assert_eq!(h.log.last().map(|e| (e.0, e.1)), Some((EE_UNITS_CU_UNIT_COPY_FROM, 8)));
    // no human: the behaviour is not tested against the table
    assert_eq!(h.count(EE_UNITS_CU_BEHAVIOR_IS), 0);
}

#[test]
fn a_humans_copy_drops_harass() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_PLAYER_IS_HUMAN, 0, 0, 1)
        .answer(EE_UNITS_CU_BEHAVIOR_IS, 0, BEHAVIOR_HARASS, 1)
        .answer(EE_UNITS_CU_HAS_CLASS, 0, 0, 1)
        .answer(EE_UNITS_CU_CLASS_BEHAVIOR, 0, 0, 33)
        .sequence(EE_UNITS_CU_BEHAVIOR, 0, 0, &[0, 33]);
    copy_from(&mut h, 1, 7, 7);
    assert!(h.asked(EE_UNITS_CU_SET_BEHAVIOR, 0, 0));
    assert!(h.asked(EE_UNITS_CU_SET_BEHAVIOR, 33, 0));
    assert_eq!(h.count(EE_UNITS_CU_BEHAVIOR_IS), 1);
}

#[test]
fn reset_draws_the_heading_and_clears_every_field() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_HAS_CLASS, 0, 0, 1)
        .answer(EE_UNITS_CU_HAS_PLAYER, 0, 0, 1)
        .answer(EE_UNITS_CU_RANDOM, 0, 0, bits(0.5))
        .answer(EE_UNITS_CU_STATS_RELOAD_TIME, 0, 0, 1500);
    reset(&mut h, 1, 4, true);
    assert_eq!(h.count(EE_UNITS_CU_RANDOM), 1);
    assert_eq!(h.count(EE_UNITS_CU_CLEAR_FIELD), 22);
    assert!(h.asked(EE_UNITS_CU_RESIZE_GOAL_QUEUES, 0, 4));
    assert!(h.at(EE_UNITS_CU_RANDOM) < h.at(EE_UNITS_CU_COMPLEX_RESET));
    assert!(h.at(EE_UNITS_CU_RELEASE_HELD_UNITS) > h.at(EE_UNITS_CU_RESIZE_GOAL_QUEUES));
    // no random heading: no draw, no class: the default behaviour
    let mut h = Mock::default().answer(EE_UNITS_CU_BEHAVIOR_TABLE, 0, BEHAVIOR_DEFAULT, 9);
    reset(&mut h, 1, 1, false);
    assert_eq!(h.count(EE_UNITS_CU_RANDOM), 0);
    assert_eq!(h.count(EE_UNITS_CU_STATS_RELOAD_TIME), 0);
    assert!(h.asked(EE_UNITS_CU_SET_BEHAVIOR, 9, 0));
}

#[test]
fn init_offsets_a_large_units_placement() {
    let mut h = Mock::default()
        .sequence(EE_UNITS_CU_PLACEMENT_X, 0, 0, &[bits(10.25), bits(10.25)])
        .sequence(EE_UNITS_CU_PLACEMENT_Y, 0, 0, &[bits(20.75), bits(20.75)])
        .answer(EE_UNITS_CU_TILE_SIZE, 0, 0, 3)
        .answer(EE_UNITS_CU_CLASS_TILE_SIZE_OF, 50, 0, 3)
        .answer(EE_UNITS_CU_CLASS_CATEGORY_OF, 50, 0, CATEGORY_LAND_CITIZEN)
        .answer(EE_UNITS_CU_UPGRADE_AREA_TEMPLATE, 60, 2, 12)
        .answer(EE_UNITS_CU_CREATE_AREA_EFFECT, 0, 12, 70);
    init(&mut h, 1, 50, 55, 60, 4);
    assert!(h.asked(EE_UNITS_CU_PLACEMENT_SET_X, 0, bits(11.25)));
    assert!(h.asked(EE_UNITS_CU_PLACEMENT_SET_Y, 0, bits(21.75)));
    assert!(h.at(EE_UNITS_CU_PLACEMENT_COPY) < h.at(EE_UNITS_CU_UNIT_INIT));
    assert!(h.at(EE_UNITS_CU_PLACEMENT_SET_Y) < h.at(EE_UNITS_CU_PM_SET_PLACEMENT));
    assert!(h.asked(EE_UNITS_CU_SET_IS_CITIZEN, 0, 1));
    assert!(h.asked(EE_UNITS_CU_AREA_EFFECTS_ADD, 70, 0));
    assert!(h.asked(EE_UNITS_CU_EFFECT_DEREFERENCE, 70, 0));
    assert!(h.at(EE_UNITS_CU_UNIT_INIT) < h.at(EE_UNITS_CU_COMPLEX_RESET));
    assert!(h.at(EE_UNITS_CU_COMPLEX_RESET) < h.at(EE_UNITS_CU_INIT_COLLIDABLE));
}

#[test]
fn free_node_deletes_each_orbital_and_wheel() {
    let mut h = Mock::default()
        .sequence(EE_UNITS_CU_NUM_ORBITALS, 0, 0, &[2, 2, 2])
        .sequence(EE_UNITS_CU_NUM_WHEELS, 0, 0, &[1, 1]);
    free_node(&mut h, 1);
    assert_eq!(h.count(EE_UNITS_CU_DELETE_ORBITAL), 2);
    assert_eq!(h.count(EE_UNITS_CU_DELETE_WHEEL), 1);
    assert_eq!(h.ops().last(), Some(&EE_UNITS_CU_UNIT_FREE_NODE));
}

#[test]
fn swap_graphic_keeps_the_animation_progress() {
    let (old, new) = (20u64, 30u64);
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_ANIMATION, 0, 0, 1)
        .answer(EE_UNITS_CU_PM_FRAME, 0, 0, 15)
        .answer(EE_UNITS_CU_GRAPHIC_START_FRAME, old, 1, 10)
        .answer(EE_UNITS_CU_GRAPHIC_FRAME_COUNT, old, 1, 10)
        .answer(EE_UNITS_CU_GRAPHIC_START_FRAME, new, 1, 100)
        .answer(EE_UNITS_CU_GRAPHIC_FRAME_COUNT, new, 1, 20)
        .answer(EE_UNITS_CU_GRAPHIC_LAST_FRAME, new, 1, 119)
        .answer(EE_UNITS_CU_FLOAT_TO_LONG, 0, bits(10.0), 10)
        .answer(EE_UNITS_CU_GRAPHIC_ORBITAL_COUNT, new, 0, 1)
        .answer(EE_UNITS_CU_NUM_ORBITALS, 0, 0, 1);
    swap_graphic(&mut h, 1, old, new);
    assert!(h.asked(EE_UNITS_CU_SET_ANIMATION_FRAME, 0, 110));
    assert!(h.asked(EE_UNITS_CU_SET_NEXT_FRAME, 0, 111));
    assert!(h.asked(EE_UNITS_CU_SCRATCH_ORBITAL_KEEP_ANGLE, 0, 0));
    assert!(h.asked(EE_UNITS_CU_SET_NUM_ORBITALS, 0, 1));
    assert_eq!(h.count(EE_UNITS_CU_DELETE_ORBITAL), 1);
}

#[test]
fn clear_goals_stops_at_a_suiting_goal() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_GOAL_QUEUE_COUNT, 0, 0, 1)
        .sequence(EE_UNITS_CU_QUEUE_GOAL_COUNT, 0, 0, &[2, 1])
        .answer(EE_UNITS_CU_QUEUE_FRONT_GOAL, 0, 0, 44)
        .answer(EE_UNITS_CU_GOAL_SUITS, 44, 0, 1);
    clear_goals(&mut h, 1, 0);
    assert_eq!(h.count(EE_UNITS_CU_QUEUE_ABANDON_FRONT), 1);
    assert_eq!(h.count(EE_UNITS_CU_CLEAR_ACTIVE_GOAL), 1);
    assert!(h.asked(EE_UNITS_CU_GOAL_START, 44, 0));
    assert_eq!(h.count(EE_UNITS_CU_REFRESH_GOALS), 2);
    // a queue past the count: only the resource cleared
    let mut h = Mock::default();
    clear_goals(&mut h, 1, 3);
    assert_eq!(h.ops(), [EE_UNITS_CU_CLEAR_GATHERED_RESOURCE, EE_UNITS_CU_GOAL_QUEUE_COUNT]);
}

#[test]
fn step_aside_sends_an_idle_unit_to_a_free_ring_tile() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_HAS_COMMAND, 0, 0, 1)
        .answer(EE_UNITS_CU_COMMAND_KIND, 0, 0, ACTION_IDLE)
        .answer(EE_UNITS_CU_STATS_MAX_VELOCITY, 0, 0, bits(2.0))
        .answer(EE_UNITS_CU_PLAYER_IS_FRIEND, 9, 0, 1)
        .answer(EE_UNITS_CU_CLASS_TILE_SIZE_OF, 50, 0, 1)
        .sequence(EE_UNITS_CU_SCRATCH_TILES_SIZE, 0, 0, &[16, 15])
        .sequence(EE_UNITS_CU_SCRATCH_TILES_BACK_X, 0, 0, &[3, 4])
        .sequence(EE_UNITS_CU_SCRATCH_TILES_BACK_Y, 0, 0, &[5, 6])
        .answer(EE_UNITS_CU_NEW_LOCATION_GOAL, 0, 0, 77);
    h.answers.insert((EE_UNITS_CU_CAN_STAND_ON, 0, 4), 1);
    assert!(step_aside(&mut h, 1, 9, 50, 10, 10, false));
    // the ring of a 1-tile footprint: 4 x 4 = 16 tiles
    assert_eq!(h.count(EE_UNITS_CU_SCRATCH_TILES_PUSH), 16);
    assert!(h.at(EE_UNITS_CU_SCRATCH_TILES_SHUFFLE) > h.at(EE_UNITS_CU_SCRATCH_TILES_PUSH));
    let goal = h.log.iter().find(|e| e.0 == EE_UNITS_CU_NEW_LOCATION_GOAL).unwrap();
    assert_eq!((goal.4[0], goal.4[1]), (4.5, 6.5));
    assert!(h.asked(EE_UNITS_CU_SET_GOAL, 77, 0));
    assert!(h.asked(EE_UNITS_CU_START_ACTION, 0, ACTION_PREPARE_TO_MOVE));
    // an enemy's footprint does not move it
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_HAS_COMMAND, 0, 0, 1)
        .answer(EE_UNITS_CU_COMMAND_KIND, 0, 0, ACTION_IDLE)
        .answer(EE_UNITS_CU_STATS_MAX_VELOCITY, 0, 0, bits(2.0));
    assert!(!step_aside(&mut h, 1, 9, 50, 10, 10, false));
    assert_eq!(h.count(EE_UNITS_CU_SCRATCH_TILES_PUSH), 0);
}

#[test]
fn is_next_to_tile_searches_the_ring_then_the_corners() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_GET_TILE_X, 0, 0, 5)
        .answer(EE_UNITS_CU_TILE_Y, 0, 0, 5)
        .sequence(EE_UNITS_CU_SCRATCH_POINTS_SIZE, 0, 0, &[1, 1, 1, 1])
        .sequence(EE_UNITS_CU_SCRATCH_POINT_X, 0, 0, &[6, 4])
        .sequence(EE_UNITS_CU_SCRATCH_POINT_Y, 0, 0, &[4]);
    assert!(is_next_to_tile(&mut h, 1, 4, 4));
    assert_eq!(h.count(EE_UNITS_CU_SCRATCH_RING_CORNERS), 1);
    let mut h = Mock::default().answer(EE_UNITS_CU_GET_TILE_X, 0, 0, 5).answer(EE_UNITS_CU_TILE_Y, 0, 0, 5);
    assert!(is_next_to_tile(&mut h, 1, 5, 5));
    assert_eq!(h.count(EE_UNITS_CU_SCRATCH_SQUARE_RING), 0);
}

#[test]
fn the_fanaticism_colour_pulses_once_a_tick() {
    let mut c = FanaticismColors::START;
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_IS_CLOAKED, 0, 0, 1)
        .answer(EE_UNITS_CU_GAME_TIME, 0, 0, 1000)
        .answer(EE_UNITS_CU_CLOAKED, 0, 0, 1250)
        .answer(EE_UNITS_CU_FANATICISM_PERIOD, 0, 0, 500)
        .answer(EE_UNITS_CU_FANATICISM_PARAM, 0, 1, bits(1.0));
    update_fanaticism_color(&mut h, 1, &mut c);
    // phase 250 of 500: half way
    assert_eq!(c.color, [0.5, 0.0, 0.0]);
    assert_eq!(c.time, 1000);
    let set = h.log.iter().find(|e| e.0 == EE_UNITS_CU_SET_COLOR).unwrap();
    assert_eq!(set.4[0], 0.5);
    // the same tick again: no new reading, the colour set again
    update_fanaticism_color(&mut h, 1, &mut c);
    assert_eq!(h.count(EE_UNITS_CU_FANATICISM_PERIOD), 1);
    assert_eq!(h.count(EE_UNITS_CU_SET_COLOR), 2);
    // run out: the tint cleared
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_IS_CLOAKED, 0, 0, 1)
        .answer(EE_UNITS_CU_GAME_TIME, 0, 0, 2000)
        .answer(EE_UNITS_CU_CLOAKED, 0, 0, 1250);
    update_fanaticism_color(&mut h, 1, &mut c);
    assert_eq!(h.count(EE_UNITS_CU_CLEAR_TINT), 1);
    assert_eq!(h.count(EE_UNITS_CU_SET_COLOR), 0);
}

#[test]
fn the_checksum_sums_the_selected_parts_as_longs() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_HIT_POINTS, 0, 0, 10)
        .answer(EE_UNITS_CU_TILE_Y, 0, 0, 3)
        .answer(EE_UNITS_CU_GET_TILE_X, 0, 0, 4)
        .answer(EE_UNITS_CU_CHECKSUM_LOCATION, 0, 0, bits(-1.0))
        .answer(EE_UNITS_CU_HAS_ACTIVE_GROUP, 0, 0, 1)
        .answer(EE_UNITS_CU_UNIT_CHECKSUM, 0, 3, 100);
    let x = i64::from((-1.0f32).to_bits() as i32);
    assert_eq!(get_checksum(&mut h, 1, 3), (100 + 17 + 1 + x) as c_long as i64);
    // the goals and effects only with their flags
    assert_eq!(h.count(EE_UNITS_CU_GOAL_QUEUE_COUNT), 0);
    let mut h = Mock::default()
        .sequence(EE_UNITS_CU_GOAL_QUEUE_COUNT, 0, 0, &[1, 1])
        .answer(EE_UNITS_CU_QUEUE_GOAL_COUNT, 0, 0, 2)
        .answer(EE_UNITS_CU_GOAL_CHECKSUM, 0, 0, 5)
        .sequence(EE_UNITS_CU_AREA_EFFECT_COUNT, 0, 0, &[1, 1])
        .answer(EE_UNITS_CU_AREA_EFFECT_CHECKSUM, 0, 0, 7);
    assert_eq!(get_checksum(&mut h, 1, 0x28), 17);
}
