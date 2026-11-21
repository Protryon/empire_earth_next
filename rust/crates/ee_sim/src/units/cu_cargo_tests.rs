use super::*;
use crate::units::cu_body::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c), its answer from `answers` keyed by (op, a) or
/// `sequences` (popped front first); the placement searches answer `placed`.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64, i64)>,
    answers: BTreeMap<(i32, u64), i64>,
    sequences: BTreeMap<(i32, u64), Vec<i64>>,
    placements: Vec<Placement>,
    placed: bool,
}

impl Mock {
    fn answer(mut self, op: i32, a: u64, value: i64) -> Self {
        self.answers.insert((op, a), value);
        self
    }
    fn sequence(mut self, op: i32, a: u64, values: &[i64]) -> Self {
        self.sequences.insert((op, a), values.to_vec());
        self
    }
    fn count(&self, op: i32) -> usize {
        self.log.iter().filter(|e| e.0 == op).count()
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
}

impl CuHost for Mock {
    fn cu(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64, _f: &mut [f32; 4]) -> i64 {
        assert_eq!(unit, 1);
        self.log.push((op, a, b, c));
        if let Some(values) = self.sequences.get_mut(&(op, a)) {
            if !values.is_empty() {
                return values.remove(0);
            }
        }
        *self.answers.get(&(op, a)).unwrap_or(&0)
    }
}

impl CuCargoHost for Mock {
    fn find_placement(&mut self, p: &Placement, m: &mut [[f32; 4]; 4]) -> bool {
        assert_eq!(m[0][0], 1.0, "the search starts from the identity");
        self.placements.push(*p);
        self.placed
    }
}

#[test]
fn update_jit_releases_the_research_and_the_queue_from_the_back() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_CURRENT_NODE, 0, 40)
        .sequence(EE_UNITS_CU_QUEUE_SIZE, 0, &[2, 1, 0])
        .sequence(EE_UNITS_CU_QUEUE_BACK_TECHNOLOGY, 0, &[301, 300])
        .answer(EE_UNITS_CU_TECH_NODE, 0, 41)
        .answer(EE_UNITS_CU_NODE_CLASS_INDEX, 41, 5)
        .answer(EE_UNITS_CU_CLASS_TABLE, 0, 50)
        .answer(EE_UNITS_CU_CLASS_AI_KIND, 50, 0x1b)
        .sequence(EE_UNITS_CU_HEROES_IN_PRODUCTION, 0, &[2, 2, 1, 1]);
    update_jit(&mut h, 1);
    // the current research is a technology (class index 0): allowed again, the JIT list told
    assert!(h.log.contains(&(EE_UNITS_CU_SET_NODE_ALLOWED, 40, 1, 0)));
    let heroes: Vec<i64> = h.log.iter().filter(|e| e.0 == EE_UNITS_CU_SET_HEROES_IN_PRODUCTION).map(|e| e.2).collect();
    assert_eq!(heroes, [1, 0]);
    assert_eq!(h.count(EE_UNITS_CU_REFUND_TECHNOLOGY), 2);
    assert_eq!(h.count(EE_UNITS_CU_QUEUE_POP_BACK), 2);
    assert_eq!(h.ops().last(), Some(&EE_UNITS_CU_UI_UPDATE_JIT));
}

#[test]
fn remove_research_keeps_the_rest_in_order() {
    let mut h = Mock::default()
        .sequence(EE_UNITS_CU_QUEUE_SIZE, 0, &[3, 3, 2, 1, 0])
        .sequence(EE_UNITS_CU_QUEUE_FRONT_TECHNOLOGY, 0, &[5, 7, 5])
        .sequence(EE_UNITS_CU_QUEUE_FRONT_FLAG, 0, &[0, 1, 0]);
    remove_research(&mut h, 1, 5, 0);
    // the first 5 removed, the second kept (not remove-all)
    let pushed: Vec<(i64, i64)> =
        h.log.iter().filter(|e| e.0 == EE_UNITS_CU_QUEUE_PUSH_BACK).map(|e| (e.2, e.3)).collect();
    assert_eq!(pushed, [(7, 1), (5, 0)]);
    assert_eq!(h.count(EE_UNITS_CU_REFUND_ONE), 1);
    assert_eq!(h.count(EE_UNITS_CU_IS_LAND_BUILDING), 0, "a removed entry ends it");
}

#[test]
fn replace_research_renames_or_drops_from_the_back() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_CURRENT_RESEARCH, 0, 5)
        .answer(EE_UNITS_CU_QUEUE_SIZE, 0, 3)
        .sequence(EE_UNITS_CU_QUEUE_TECHNOLOGY_AT, 0, &[5, 6, 5]);
    replace_research(&mut h, 1, 5, 100000);
    assert!(h.log.contains(&(EE_UNITS_CU_SET_PRODUCTION_TIME_FIELD, 0, 0, 0)));
    let erased: Vec<i64> = h.log.iter().filter(|e| e.0 == EE_UNITS_CU_QUEUE_ERASE_AT).map(|e| e.2).collect();
    assert_eq!(erased, [2, 0]);
}

#[test]
fn queue_research_pays_and_counts_a_hero_within_the_limit() {
    let base = || {
        Mock::default()
            .answer(EE_UNITS_CU_TECH_NODE, 0, 40)
            .answer(EE_UNITS_CU_CAN_AFFORD, 40, 1)
            .answer(EE_UNITS_CU_NODE_ALLOWED, 40, 1)
            .answer(EE_UNITS_CU_NODE_CLASS_INDEX, 40, 5)
            .answer(EE_UNITS_CU_CLASS_TABLE, 0, 50)
            .answer(EE_UNITS_CU_CLASS_AI_KIND, 50, 0x1b)
            .answer(EE_UNITS_CU_HERO_LIMIT, 0, 2)
    };
    let mut h = base().answer(EE_UNITS_CU_HERO_COUNT, 0, 2);
    queue_research(&mut h, 1, 300, false);
    assert_eq!(h.count(EE_UNITS_CU_PAY), 0, "the hero limit is reached");
    let mut h = base().answer(EE_UNITS_CU_HERO_COUNT, 0, 1).answer(EE_UNITS_CU_JIT_MANUFACTURING, 0, 1);
    queue_research(&mut h, 1, 300, true);
    assert!(h.log.contains(&(EE_UNITS_CU_QUEUE_PUSH_FRONT, 0, 300, 1)));
    assert!(h.log.contains(&(EE_UNITS_CU_SET_HEROES_IN_PRODUCTION, 0, 1, 0)));
    assert_eq!(h.count(EE_UNITS_CU_PAY), 1);
    assert_eq!(h.count(EE_UNITS_CU_SET_NODE_ALLOWED), 0, "a unit stays allowed");
}

#[test]
fn load_cargo_takes_a_living_unit_that_fits() {
    let mut h = Mock::default().answer(EE_UNITS_CU_UNIT_WEIGHT, 9, 3).answer(EE_UNITS_CU_CARGO_ROOM, 0, 2);
    assert_eq!(load_cargo(&mut h, 1, 9, false, false), 7);
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_UNIT_WEIGHT, 9, 3)
        .answer(EE_UNITS_CU_CARGO_ROOM, 0, 5)
        .answer(EE_UNITS_CU_UNIT_HIT_POINTS, 9, 10)
        .answer(EE_UNITS_CU_OFF_MAP_INFO, 9, 70)
        .answer(EE_UNITS_CU_IS_TRANSPORT, 0, 1)
        .answer(EE_UNITS_CU_UNIT_TYPE, 0, 0x2a0);
    assert_eq!(load_cargo(&mut h, 1, 9, false, true), 0);
    assert!(h.log.contains(&(EE_UNITS_CU_SET_CARGO_ROOM, 0, 2, 0)));
    assert!(h.log.contains(&(EE_UNITS_CU_SET_OFF_MAP_REASON, 70, 6, 0)), "a fortress' reason");
    assert!(h.log.contains(&(EE_UNITS_CU_CARGO_ADDED, 9, 1, 0)));
    assert_eq!(h.count(EE_UNITS_CU_REFERENCE_THIS), 1);
}

#[test]
fn garrison_and_kill_cargo_count_the_dead() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_UNIT_WEIGHT, 9, 1)
        .answer(EE_UNITS_CU_GARRISON_CAPACITY, 0, 4)
        .answer(EE_UNITS_CU_UNIT_DEAD_STATISTIC, 9, 2);
    assert!(garrison_unit(&mut h, 1, 9));
    assert!(h.log.contains(&(EE_UNITS_CU_STATS_LOWER, 0, 2, 0)));
    let mut h = Mock::default()
        .sequence(EE_UNITS_CU_CARGO_COUNT, 0, &[1, 1, 0])
        .answer(EE_UNITS_CU_CARGO_AT, 0, 9)
        .answer(EE_UNITS_CU_UNIT_CATEGORY, 9, 2);
    kill_cargo(&mut h, 1);
    assert!(h.log.contains(&(EE_UNITS_CU_KILL_UNIT, 9, 0, 0)));
    assert!(h.log.contains(&(EE_UNITS_CU_UNIT_STATS_RESET, 9, 1, 0)));
}

#[test]
fn release_and_replace_cargo() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_CARGO_COUNT, 0, 2)
        .sequence(EE_UNITS_CU_CARGO_AT, 0, &[8, 9, 9])
        .answer(EE_UNITS_CU_UNIT_WEIGHT, 9, 2)
        .answer(EE_UNITS_CU_CARGO_ROOM, 0, 1);
    release_cargo(&mut h, 1, 9);
    assert!(h.log.contains(&(EE_UNITS_CU_CARGO_ERASE_AT, 0, 0, 0)));
    assert!(h.log.contains(&(EE_UNITS_CU_SET_CARGO_ROOM, 0, 3, 0)));
    assert_eq!(h.ops().last(), Some(&EE_UNITS_CU_UNIT_CLEAR_CONTAINER));
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_CARGO_COUNT, 0, 1)
        .answer(EE_UNITS_CU_CARGO_AT, 0, 8)
        .answer(EE_UNITS_CU_UNIT_WEIGHT, 8, 2)
        .answer(EE_UNITS_CU_UNIT_WEIGHT, 7, 5);
    replace_cargo(&mut h, 1, 8, 7);
    assert!(h.log.contains(&(EE_UNITS_CU_CARGO_SET_AT, 7, 0, 0)));
    let rooms: Vec<i64> = h.log.iter().filter(|e| e.0 == EE_UNITS_CU_SET_CARGO_ROOM).map(|e| e.2).collect();
    assert_eq!(rooms, [2, -3, 0]);
}

#[test]
fn unloading_at_a_tile_searches_each_cargo_from_the_back() {
    let mut h = Mock { placed: true, ..Default::default() }
        .answer(EE_UNITS_CU_CARGO_COUNT, 0, 2)
        .answer(EE_UNITS_CU_CARGO_AT, 0, 9)
        .answer(EE_UNITS_CU_UNIT_HIT_POINTS, 9, 3)
        .answer(EE_UNITS_CU_STATS_RANGE, 0, i64::from(2.5f32.to_bits()));
    unload_cargo_at_tile(&mut h, 1, 10, 11, true);
    assert_eq!(h.placements.len(), 2);
    assert_eq!(h.placements[0].rings, 2, "2.5 rounds to even");
    assert!(h.placements[0].corners && h.placements[0].include_center);
    let unloaded: Vec<i64> = h.log.iter().filter(|e| e.0 == EE_UNITS_CU_UNLOAD_CARGO_AT).map(|e| e.2).collect();
    assert_eq!(unloaded, [1, 0]);
    assert!(h.log.contains(&(EE_UNITS_CU_ADD_EVENT, 0, -1, 0)), "still aboard: the event");
}

#[test]
fn unloading_now_needs_the_cargo_aboard() {
    let mut h = Mock::default().answer(EE_UNITS_CU_CARGO_COUNT, 0, 1).answer(EE_UNITS_CU_CARGO_AT, 0, 8);
    assert!(!unload_cargo_now(&mut h, 1, 9));
    let mut h = Mock::default().answer(EE_UNITS_CU_CARGO_COUNT, 0, 1).answer(EE_UNITS_CU_CARGO_AT, 0, 9);
    assert!(unload_cargo_now(&mut h, 1, 9), "a dead cargo is released");
    assert!(h.log.contains(&(EE_UNITS_CU_RELEASE_CARGO, 9, 0, 0)));
}

#[test]
fn unload_cargo_at_answers_a_still_referenced_cargo() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_CARGO_AT, 0, 9)
        .answer(EE_UNITS_CU_UNIT_HIT_POINTS, 9, 3)
        .answer(EE_UNITS_CU_UNIT_TILE_X, 9, 4)
        .answer(EE_UNITS_CU_UNIT_REFERENCE_COUNT, 9, 2);
    let m = [[0.0f32; 4]; 4];
    assert_eq!(unload_cargo_at(&mut h, 1, 0, m.as_ptr() as u64), 9);
    assert!(h.log.contains(&(EE_UNITS_CU_ACTIVATE_UNIT, 9, 0, 0)));
    assert!(h.log.contains(&(EE_UNITS_CU_UNIT_START_ACTION, 9, 5000, 0)));
    assert_eq!(h.count(EE_UNITS_CU_UNIT_CLEAR_CONTAINER), 1);
}

#[test]
fn order_repair_puts_the_target_first() {
    let mut h = Mock::default().answer(EE_UNITS_CU_BUILD_QUEUE_FIND, 9, 2);
    order_repair(&mut h, 1, 9);
    assert!(h.log.contains(&(EE_UNITS_CU_BUILD_QUEUE_ERASE_AT, 0, 2, 0)));
    assert_eq!(h.count(EE_UNITS_CU_REFERENCE), 0, "a queued target is referenced already");
    assert_eq!(h.ops().last(), Some(&EE_UNITS_CU_CLEAR_SHIFT_QUEUED));
    let mut h = Mock::default().answer(EE_UNITS_CU_BUILD_QUEUE_FIND, 9, -1).answer(EE_UNITS_CU_CONTACT_MISSING, 9, 1);
    add_build_target(&mut h, 1, 9);
    add_contact(&mut h, 1, 9);
    assert_eq!(h.count(EE_UNITS_CU_REFERENCE), 2);
}
