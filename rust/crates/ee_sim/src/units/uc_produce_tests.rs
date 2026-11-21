use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c), its answer from `answers` keyed by (op, a) or
/// `sequences`; the searches answer `placed` and record their arguments.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64, i64)>,
    answers: BTreeMap<(i32, u64), i64>,
    sequences: BTreeMap<(i32, u64), Vec<i64>>,
    finds: Vec<FindPlacement>,
    docks: Vec<bool>,
    placed: bool,
    standable: Vec<(i32, i32)>,
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
}

impl UcHost for Mock {
    #[allow(clippy::too_many_arguments)]
    fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
        assert_eq!((class, unit), (100, 1));
        self.log.push((op, a, b, c));
        if op == EE_UNITS_UC_GOAL_LOCATION {
            *f = [5.5, 6.5, 0.0, 0.0];
        }
        if let Some(values) = self.sequences.get_mut(&(op, a)) {
            if !values.is_empty() {
                return values.remove(0);
            }
        }
        *self.answers.get(&(op, a)).unwrap_or(&0)
    }
}

impl UcRust for Mock {
    fn find_placement(&mut self, p: &FindPlacement, m: &mut [[f32; 4]; 4]) -> bool {
        self.finds.push(*p);
        m[0][3] = 7.0;
        self.placed
    }
    fn dock_ship_placement(
        &mut self,
        _point: [f32; 3],
        _class: u64,
        _unit: u64,
        _face: bool,
        facing_only: bool,
        _m: &mut [[f32; 4]; 4],
    ) -> bool {
        self.docks.push(facing_only);
        self.placed
    }
    fn can_stand_on(&mut self, x: i32, y: i32, _unit: u64) -> bool {
        self.standable.contains(&(x, y))
    }
}

/// a class (40) that fits the population
fn base() -> Mock {
    Mock { placed: true, ..Default::default() }.answer(EE_UNITS_UC_POPULATION_LIMIT, 0, 10)
}

#[test]
fn no_room_in_the_population_produces_nothing() {
    let mut h = Mock::default().answer(EE_UNITS_UC_TOTAL_POPULATION, 0, 3);
    assert!(!produce_unit(&mut h, 100, 1, 40, false));
    assert_eq!(h.count(EE_UNITS_UC_CREATE_AT), 0);
}

#[test]
fn an_aircraft_is_loaded_into_the_air_traffic() {
    let mut h = base()
        .answer(EE_UNITS_UC_CLASS_IS_AIRCRAFT, 40, 1)
        .answer(EE_UNITS_UC_CLASS_AIR_TRAFFIC, 40, 2)
        .answer(EE_UNITS_UC_CLASS_WEIGHT, 40, 3)
        .sequence(EE_UNITS_UC_ATC_CARGO, 0, &[4, 8])
        .answer(EE_UNITS_UC_ATC_FREE_WEIGHT, 0, 3)
        .answer(EE_UNITS_UC_CREATE_AT, 40, 9);
    assert!(produce_unit(&mut h, 100, 1, 40, true));
    assert!(h.log.contains(&(EE_UNITS_UC_LOAD_CARGO, 9, 0, 0)));
    assert_eq!(h.count(EE_UNITS_UC_QUEUE_LEN), 0, "a loaded unit takes no goals");
    assert_eq!(h.count(EE_UNITS_UC_PLAY_NO_EFFECT), 1);
    // full: nothing
    let mut h = base()
        .answer(EE_UNITS_UC_CLASS_IS_AIRCRAFT, 40, 1)
        .answer(EE_UNITS_UC_CLASS_AIR_TRAFFIC, 40, 2)
        .answer(EE_UNITS_UC_CLASS_WEIGHT, 40, 5)
        .sequence(EE_UNITS_UC_ATC_CARGO, 0, &[4, 8]);
    assert!(!produce_unit(&mut h, 100, 1, 40, false));
}

#[test]
fn a_ship_docks_and_a_land_unit_is_placed_by_the_producer() {
    let mut h = base().answer(EE_UNITS_UC_CLASS_DOMAINS, 40, 4).answer(EE_UNITS_UC_CREATE_AT, 40, 9);
    assert!(produce_unit(&mut h, 100, 1, 40, false));
    assert_eq!(h.docks, [true]);
    assert_eq!(h.count(EE_UNITS_UC_ALIGN_YAW), 1);
    let mut h = base().answer(EE_UNITS_UC_PRODUCER_TILE_X, 0, 12).answer(EE_UNITS_UC_TILE_SIZE, 0, 2).answer(
        EE_UNITS_UC_CREATE_AT,
        40,
        9,
    );
    assert!(produce_unit(&mut h, 100, 1, 40, false));
    let find = h.finds[0];
    assert_eq!((find.x, find.size, find.rings, find.target, find.class), (12, 2, 5, 1, 40));
    assert_eq!(find.flags, [true, false, false, false], "a computer's search takes corners");
    // not placed: nothing
    let mut h = Mock { placed: false, ..base() };
    assert!(!produce_unit(&mut h, 100, 1, 40, false));
}

#[test]
fn the_queued_goals_are_handed_on() {
    // two queued goals: a build target (kept as such) and a location moved onto standable ground
    let mut h = base()
        .answer(EE_UNITS_UC_CREATE_AT, 40, 9)
        .answer(EE_UNITS_UC_QUEUED_GOAL, 0, 70)
        .sequence(EE_UNITS_UC_QUEUE_LEN, 0, &[2, 2, 2])
        .sequence(EE_UNITS_UC_QUEUE_GOAL, 0, &[71, 72])
        .answer(EE_UNITS_UC_GOAL_SUITS, 71, 1)
        .answer(EE_UNITS_UC_GOAL_KIND, 71, 4)
        .answer(EE_UNITS_UC_NEW_LOCATION, 0, 80)
        .answer(EE_UNITS_UC_GOAL_KIND, 80, 1)
        .answer(EE_UNITS_UC_LINE_LEN, 0, 2)
        .answer(EE_UNITS_UC_LINE_X, 0, 3)
        .answer(EE_UNITS_UC_OUTER_SPHERE, 9, i64::from(0.25f32.to_bits()))
        .answer(EE_UNITS_UC_GOAL_STATE, 9, 1);
    h.standable.push((3, 0));
    assert!(produce_unit(&mut h, 100, 1, 40, false));
    assert!(h.log.contains(&(EE_UNITS_UC_ADD_BUILD_TARGET, 9, 71, 0)));
    assert!(h.log.contains(&(EE_UNITS_UC_TILE_LINE, 9, 5, 6)), "the goal point's floor");
    let moved = h.finds.last().unwrap();
    assert_eq!((moved.unit, moved.rings, moved.flags), (9, 2, [true, true, true, false]));
    assert_eq!(h.count(EE_UNITS_UC_GOAL_SET_LOCATION), 1);
    assert!(h.log.contains(&(EE_UNITS_UC_QUEUE_GOAL_ON, 9, 80, 0)));
    assert_eq!(h.count(EE_UNITS_UC_QUEUE_GOAL_ON), 1);
    assert_eq!(h.count(EE_UNITS_UC_START_GOAL_ACTION), 1);
}

#[test]
fn a_citizens_attack_becomes_gathering() {
    let mut h = base()
        .answer(EE_UNITS_UC_CREATE_AT, 40, 9)
        .answer(EE_UNITS_UC_QUEUED_GOAL, 0, 70)
        .sequence(EE_UNITS_UC_QUEUE_LEN, 0, &[1, 1])
        .answer(EE_UNITS_UC_QUEUE_GOAL, 0, 71)
        .answer(EE_UNITS_UC_GOAL_SUITS, 71, 1)
        .answer(EE_UNITS_UC_GOAL_CLONE, 71, 81)
        .answer(EE_UNITS_UC_GOAL_KIND, 81, 2)
        .answer(EE_UNITS_UC_UNIT_CATEGORY, 9, 6)
        .answer(EE_UNITS_UC_ATTACK_TARGET, 81, 50)
        .answer(EE_UNITS_UC_CAN_GATHER, 9, 1)
        .answer(EE_UNITS_UC_NEW_GATHER, 50, 82);
    assert!(produce_unit(&mut h, 100, 1, 40, false));
    assert!(h.log.contains(&(EE_UNITS_UC_DELETE_GOAL, 81, 0, 0)));
    assert!(h.log.contains(&(EE_UNITS_UC_QUEUE_GOAL_ON, 9, 82, 0)));
    assert_eq!(h.count(EE_UNITS_UC_GATHER_BEHAVIOR), 1);
}
