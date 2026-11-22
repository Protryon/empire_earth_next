use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, b, c, d), its answer from `answers` keyed by (op, b) (0 else); `floats`
/// set `io.f` for (op, b).
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64, i64)>,
    answers: BTreeMap<(i32, u64), i64>,
    floats: BTreeMap<(i32, u64), f32>,
    location: [f32; 3],
    point: [f32; 3],
    seen_f: Vec<f32>,
}

impl Mock {
    fn answer(mut self, op: i32, b: u64, value: i64) -> Self {
        self.answers.insert((op, b), value);
        self
    }
    fn float(mut self, op: i32, b: u64, value: f32) -> Self {
        self.floats.insert((op, b), value);
        self
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
    fn asked(&self, op: i32) -> Option<&(i32, u64, i64, i64)> {
        self.log.iter().find(|e| e.0 == op)
    }
}

impl UaiHost for Mock {
    fn uai(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, io: &mut EeUnitsCondIo) -> i64 {
        assert_eq!(a, 7);
        self.log.push((op, b, c, d));
        self.seen_f.push(io.f);
        if let Some(f) = self.floats.get(&(op, b)) {
            io.f = *f;
        }
        if op == EE_UNITS_UAI_CONTACT_LOCATION || op == EE_UNITS_UAI_POINT {
            io.p = self.point;
        }
        *self.answers.get(&(op, b)).unwrap_or(&0)
    }
    fn uai_location(&mut self, _unit: u64) -> [f32; 3] {
        self.location
    }
}

fn run(h: &mut Mock, op: i32, b: u64, c: i64) -> (Option<i64>, EeUnitsCondIo) {
    let mut io = EeUnitsCondIo::default();
    let v = compound(h, op, 7, b, c, &mut io);
    (v, io)
}

#[test]
fn other_operations_are_not_answered() {
    let mut h = Mock::default();
    assert_eq!(run(&mut h, EE_UNITS_COND_MOVE_FREELY, 0, 0).0, None);
    assert!(h.log.is_empty());
}

#[test]
fn attack_nearby_waits_for_the_search_timer() {
    let mut h = Mock::default().answer(EE_UNITS_UAI_SEARCH_TIMER, 0, 5);
    assert!(!attack_nearby(&mut h, 7, 0));
    assert_eq!(h.ops(), vec![EE_UNITS_UAI_SEARCH_TIMER]);
}

#[test]
fn attack_nearby_near_the_initial_contact_keeps_a_move_goal() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UAI_GOAL_KIND, 40, 1)
        .answer(EE_UNITS_UAI_SEARCH_TIMER, 0, 9)
        .float(EE_UNITS_UAI_CONTACT_HEADING, 0, 0.5)
        .answer(EE_UNITS_UAI_RETURNS_TO_CONTACT, 0, 1)
        .answer(EE_UNITS_UAI_FIND_TARGET_NEAR, 0, 55)
        .answer(EE_UNITS_UAI_IS_LAND_BUILDING, 55, 0)
        .answer(EE_UNITS_UAI_NEW_ATTACK, 55, 66);
    h.point = [3.7, -0.5, 0.0];
    assert!(attack_nearby(&mut h, 7, 40));
    let near = *h.asked(EE_UNITS_UAI_FIND_TARGET_NEAR).unwrap();
    assert_eq!((near.2, near.3), (3, -1));
    let set = *h.asked(EE_UNITS_UAI_SET_GOAL).unwrap();
    assert_eq!((set.1, set.2), (66, 1));
    assert_eq!(
        h.ops(),
        vec![
            EE_UNITS_UAI_GOAL_KIND,
            EE_UNITS_UAI_GOAL_FORCED_MOVE,
            EE_UNITS_UAI_SEARCH_TIMER,
            EE_UNITS_UAI_CONTACT_HEADING,
            EE_UNITS_UAI_RETURNS_TO_CONTACT,
            EE_UNITS_UAI_GOAL_B1C,
            EE_UNITS_UAI_CONTACT_LOCATION,
            EE_UNITS_UAI_FIND_TARGET_NEAR,
            EE_UNITS_UAI_GOAL_STATE,
            EE_UNITS_UAI_CATEGORY,
            EE_UNITS_UAI_NEW_ATTACK,
            EE_UNITS_UAI_SET_GOAL,
            EE_UNITS_UAI_NOTICE_FOREIGN,
            EE_UNITS_UAI_RESTART_SEARCH,
        ]
    );
    // the range was 1.0
    let at = h.ops().iter().position(|o| *o == EE_UNITS_UAI_FIND_TARGET_NEAR).unwrap();
    assert_eq!(h.seen_f[at], 1.0);
}

#[test]
fn a_priest_does_not_attack_a_unit_it_cannot_convert() {
    let mut h = Mock::default()
        .float(EE_UNITS_UAI_CONTACT_HEADING, 0, NO_HEADING)
        .answer(EE_UNITS_UAI_FIND_NEARBY_TARGET, 0, 55)
        .answer(EE_UNITS_UAI_GOAL_STATE, 0, 30)
        .answer(EE_UNITS_UAI_GOAL_KIND, 30, 2)
        .answer(EE_UNITS_UAI_IS_LAND_BUILDING, 55, 1)
        .float(EE_UNITS_UAI_ATTACK_OF, 55, 2.0)
        .answer(EE_UNITS_UAI_CATEGORY, 0, CATEGORY_BUILDING_PRIEST)
        .answer(EE_UNITS_UAI_AS_COMPLEX, 55, 56)
        .answer(EE_UNITS_UAI_CAN_CONVERT, 56, 1)
        .answer(EE_UNITS_UAI_CONVERT_FLAG, 0, 1);
    assert!(!attack_nearby(&mut h, 7, 0));
    assert!(h.asked(EE_UNITS_UAI_NEW_ATTACK).is_none());
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_UAI_RESTART_SEARCH);
}

#[test]
fn an_attack_on_a_defenceless_building_is_kept() {
    let mut h = Mock::default()
        .float(EE_UNITS_UAI_CONTACT_HEADING, 0, NO_HEADING)
        .answer(EE_UNITS_UAI_FIND_NEARBY_TARGET, 0, 55)
        .answer(EE_UNITS_UAI_GOAL_STATE, 0, 30)
        .answer(EE_UNITS_UAI_GOAL_KIND, 30, 2)
        .answer(EE_UNITS_UAI_IS_LAND_BUILDING, 55, 1)
        .float(EE_UNITS_UAI_ATTACK_OF, 55, 0.0);
    assert!(!attack_nearby(&mut h, 7, 0));
    assert!(h.asked(EE_UNITS_UAI_CATEGORY).is_none());
}

#[test]
fn the_obstacle_wait_rounds_as_the_original() {
    // 1.0 * 4000 = 4000; melee attacking: 2000; count 2000.5 > 2000
    let mut h = Mock::default()
        .answer(EE_UNITS_UAI_ACTIVE_GOAL, 0, 30)
        .float(EE_UNITS_UAI_RANDOM, 0, 1.0)
        .answer(EE_UNITS_UAI_WAIT_SCALE, 0, 4000)
        .answer(EE_UNITS_UAI_IS_MELEE, 0, 1)
        .float(EE_UNITS_UAI_VELOCITY, 0, 1.0)
        .answer(EE_UNITS_UAI_GOAL_KIND, 30, 2)
        .float(EE_UNITS_UAI_COLLISION_COUNT, 0, 2000.5);
    assert_eq!(run(&mut h, EE_UNITS_COND_OBSTACLE, 0, 0).0, Some(1));
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_UAI_CLEAR_COLLISION_COUNT);
    let mut h = Mock::default().float(EE_UNITS_UAI_RANDOM, 0, 1.0).answer(EE_UNITS_UAI_WAIT_SCALE, 0, 4000).float(
        EE_UNITS_UAI_COLLISION_COUNT,
        0,
        4000.0,
    );
    assert_eq!(run(&mut h, EE_UNITS_COND_OBSTACLE, 0, 0).0, Some(0));
    let at = h.ops().iter().position(|o| *o == EE_UNITS_UAI_RANDOM).unwrap();
    assert_eq!(h.log[at].0, EE_UNITS_UAI_RANDOM);
    let mut h = Mock::default().answer(EE_UNITS_UAI_HAS_BLOCKING, 0, 1);
    assert_eq!(run(&mut h, EE_UNITS_COND_OBSTACLE, 0, 0).0, Some(1));
    assert_eq!(h.ops(), vec![EE_UNITS_UAI_HAS_BLOCKING]);
}

#[test]
fn flags_and_ranges() {
    let mut h = Mock::default().answer(EE_UNITS_UAI_CAN_FLEE, 0, 1).answer(EE_UNITS_UAI_RETURNS_TO_CONTACT, 0, 1);
    assert_eq!(run(&mut h, EE_UNITS_COND_BEHAVIOR, 0, 0).0, Some(10));
    let mut h = Mock::default().answer(EE_UNITS_UAI_HAS_HUMAN, 0, 1);
    assert_eq!(run(&mut h, EE_UNITS_COND_OWNER, 0, 0).0, Some(2));
    let mut h = Mock::default().float(EE_UNITS_UAI_DEFAULT_RESPONSE_RANGE, 0, 12.0);
    let (v, io) = run(&mut h, EE_UNITS_COND_RESPONSE_RANGE, 0, 0);
    assert_eq!((v, io.f), (Some(0), 12.0));
    let mut h = Mock::default().answer(EE_UNITS_UAI_HERD, 0, 9).answer(EE_UNITS_UAI_HERD_GROUP_FLEE, 9, 1);
    assert_eq!(run(&mut h, EE_UNITS_COND_HERD, 0, 0).0, Some(9));
    let mut h = Mock::default().answer(EE_UNITS_UAI_ENEMIES_NEAR, 4, 1);
    assert_eq!(run(&mut h, EE_UNITS_COND_MEMBER, 4, 3).0, Some(1));
}

#[test]
fn off_map_tests_floor_the_location() {
    let mut h = Mock { location: [2.6, 5.2, 0.0], ..Default::default() };
    assert_eq!(run(&mut h, EE_UNITS_COND_OFF_MAP, 0, 0).0, Some(1));
    let t = *h.asked(EE_UNITS_UAI_VALID_TILE).unwrap();
    assert_eq!((t.2, t.3), (2, 5));
    let mut h = Mock::default().answer(EE_UNITS_UAI_STRAY, 0, 1);
    assert_eq!(run(&mut h, EE_UNITS_COND_OFF_MAP, 0, 2).0, Some(1));
}

#[test]
fn drop_location_and_ground_use_a_cleared_point() {
    let mut h = Mock { point: [1.0, 2.0, 3.0], ..Default::default() }.answer(EE_UNITS_UAI_FIND_DROP, 0, 44);
    let (v, io) = run(&mut h, EE_UNITS_COND_DROP_LOCATION, 0, 0);
    assert_eq!((v, io.p), (Some(44), [1.0, 2.0, 3.0]));
    assert_eq!(h.ops(), vec![EE_UNITS_UAI_CLEAR_POINT, EE_UNITS_UAI_FIND_DROP, EE_UNITS_UAI_POINT]);
    let mut h = Mock::default().float(EE_UNITS_UAI_POINT_Z, 0, -1.5);
    let (_, io) = run(&mut h, EE_UNITS_COND_GROUND_Z, 0, 0);
    assert_eq!(io.f, -1.5);
}

#[test]
fn the_build_queue_starts_the_next_site() {
    let mut h =
        Mock::default().answer(EE_UNITS_UAI_POP_BUILD_TARGET, 0, 31).answer(EE_UNITS_UAI_NEW_BUILD_REPAIR, 31, 32);
    assert_eq!(run(&mut h, EE_UNITS_COND_BUILD_QUEUE, 0, 1).0, Some(1));
    let set = *h.asked(EE_UNITS_UAI_SET_GOAL).unwrap();
    assert_eq!((set.1, set.2), (32, 0));
    assert_eq!(h.asked(EE_UNITS_UAI_START_ACTION).unwrap().2, 8017);
    let mut h = Mock::default().answer(EE_UNITS_UAI_CITIZEN_QUEUE_SIZE, 0, 3);
    assert_eq!(run(&mut h, EE_UNITS_COND_BUILD_QUEUE, 0, 0).0, Some(3));
}

#[test]
fn the_attack_goal_flags() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UAI_ATTACK_GOAL, 0, 20)
        .answer(EE_UNITS_UAI_ATTACK_TARGET, 20, 21)
        .answer(EE_UNITS_UAI_ON_MAP, 21, 1)
        .answer(EE_UNITS_UAI_DISABLED, 21, 1)
        .answer(EE_UNITS_UAI_STANCE, 21, 1)
        .answer(EE_UNITS_UAI_CONVERT_ATTACK, 20, 1);
    assert_eq!(run(&mut h, EE_UNITS_COND_ATTACK_GOAL, 0, 0).0, Some(5));
    let mut h = Mock::default();
    assert_eq!(run(&mut h, EE_UNITS_COND_ATTACK_GOAL, 0, 0).0, Some(0));
}

#[test]
fn a_launch_drops_a_vanished_exiting_unit() {
    let mut h = Mock::default().answer(EE_UNITS_UAI_EXITING_ID, 0, 12).answer(EE_UNITS_UAI_WORLD_UNIT, 0, 13);
    h.answers.insert((EE_UNITS_UAI_COMPLEX_OF, 13), 14);
    assert_eq!(run(&mut h, EE_UNITS_COND_LAUNCH, 0, 1).0, Some(0));
    assert_eq!(h.asked(EE_UNITS_UAI_SET_EXITING).unwrap().2, -1);
    assert_eq!(h.asked(EE_UNITS_UAI_REMOVE_PLANE).unwrap().1, 14);
    let mut h = Mock::default().answer(EE_UNITS_UAI_EXITING_ID, 0, -1).answer(EE_UNITS_UAI_TRAFFIC_QUEUE_LENGTH, 0, 2);
    assert_eq!(run(&mut h, EE_UNITS_COND_LAUNCH, 0, 0).0, Some(1));
}
