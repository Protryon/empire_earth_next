use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c), its answer from `answers` keyed by (op, a, b) (0 else);
/// `sequences` answer each ask of (op, a, b) in turn.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64, i64)>,
    answers: BTreeMap<(i32, u64, i64), i64>,
    sequences: BTreeMap<(i32, u64, i64), Vec<i64>>,
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
}

impl DopHost for Mock {
    fn dop(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64) -> i64 {
        assert_eq!(unit, 1);
        self.log.push((op, a, b, c));
        if let Some(values) = self.sequences.get_mut(&(op, a, b)) {
            if !values.is_empty() {
                return values.remove(0);
            }
        }
        *self.answers.get(&(op, a, b)).unwrap_or(&0)
    }
}

#[test]
fn a_failed_base_repair_returns_at_once() {
    let mut h = Mock::default().answer(EE_UNITS_DOP_BASE_REPAIR, 0, 0, 7);
    assert_eq!(repair_pointers(&mut h, 1, 0, 0x100), 7);
    assert_eq!(h.ops(), [EE_UNITS_DOP_BASE_REPAIR]);
}

#[test]
fn a_classless_unit_forgets_its_links_and_repairs_its_goals() {
    let mut h = Mock::default()
        .answer(EE_UNITS_DOP_COMMAND_IS_NULL, 0, 0, 1)
        .answer(EE_UNITS_DOP_HELD_COUNT, 0, 0, 2)
        .sequence(EE_UNITS_DOP_GOAL_QUEUE_COUNT, 0, 0, &[1, 1])
        .sequence(EE_UNITS_DOP_GOAL_COUNT, 0, 0, &[2, 2, 2])
        .answer(EE_UNITS_DOP_GOAL_AT, 0, 0, 40)
        .answer(EE_UNITS_DOP_GOAL_REPAIR, 40, 0, 5);
    // no class: no idle action; the last goal's repair is the result and its queue is cleared
    assert_eq!(repair_pointers(&mut h, 1, 0, 0x100), 5);
    assert_eq!(h.count(EE_UNITS_DOP_START_ACTION), 0);
    assert_eq!(h.count(EE_UNITS_DOP_SET_HELD), 2);
    assert!(h.asked(EE_UNITS_DOP_SET_HELD, 0, 1));
    assert!(h.asked(EE_UNITS_DOP_SET_HELD, 0, 0));
    assert_eq!(h.count(EE_UNITS_DOP_GOAL_REPAIR), 2);
    assert!(h.asked(EE_UNITS_DOP_GOAL_QUEUE_CLEAR, 0, 0));
    assert!(h.asked(EE_UNITS_DOP_SET_CURRENT_RESEARCH, 0, TECH_NONE));
    assert_eq!(h.count(EE_UNITS_DOP_CITIZEN_COUNT), 0);
}

fn classed() -> Mock {
    Mock::default()
        .answer(EE_UNITS_DOP_HAS_CLASS, 0, 0, 1)
        .answer(EE_UNITS_DOP_PATROL_IS_UNSET, 0, 0, 1)
        .answer(EE_UNITS_DOP_OWNER_IS_UNSET, 0, 0, 1)
        .answer(EE_UNITS_DOP_MINE_IS_UNSET, 0, 0, 1)
        .answer(EE_UNITS_DOP_HOLDER_IS_UNSET, 0, 0, 1)
        .answer(EE_UNITS_DOP_CURRENT_RESEARCH, 0, 0, TECH_NONE)
        .answer(EE_UNITS_DOP_BEHAVIOR_ID, 0, 0, 2)
        .answer(EE_UNITS_DOP_BEHAVIOR_GET, 0, 2, 900)
        .answer(EE_UNITS_DOP_BEHAVIOR, 0, 0, 900)
        .answer(EE_UNITS_DOP_HIT_POINTS, 0, 0, 10)
}

#[test]
fn the_plain_unit_runs_every_step_in_order() {
    let mut h = classed();
    assert_eq!(repair_pointers(&mut h, 1, 0, 0x100), 0);
    let ops = h.ops();
    let at = |op: i32| ops.iter().position(|&o| o == op).unwrap();
    // the original's order of the sections
    let order = [
        EE_UNITS_DOP_CITIZEN_COUNT,
        EE_UNITS_DOP_CARGO_ASSIGN,
        EE_UNITS_DOP_PATROL_IS_UNSET,
        EE_UNITS_DOP_PREVIOUS_PART_ID,
        EE_UNITS_DOP_CONTAINER_ID,
        EE_UNITS_DOP_SET_PRODUCTION_TIME,
        EE_UNITS_DOP_BUILD_QUEUE_ASSIGN,
        EE_UNITS_DOP_GATHERED_LINK,
        EE_UNITS_DOP_LAST_ATTACKER_ID,
        EE_UNITS_DOP_OWNER_IS_UNSET,
        EE_UNITS_DOP_MINE_IS_UNSET,
        EE_UNITS_DOP_MODEL_PARTS,
        EE_UNITS_DOP_PAST_LAST_FRAME,
        EE_UNITS_DOP_OWN_TECH_NODE,
        EE_UNITS_DOP_HOLDER_IS_UNSET,
        EE_UNITS_DOP_BEHAVIOR_ID,
        EE_UNITS_DOP_HAS_PLAYER,
        EE_UNITS_DOP_LAST_RESOURCE_LINK,
        EE_UNITS_DOP_MAKE_AREA_EFFECTS,
        EE_UNITS_DOP_REFRESH_GOALS,
        EE_UNITS_DOP_REFRESH_ACTIVE_GOAL,
        EE_UNITS_DOP_CLASS_ROTTING_TIME,
        EE_UNITS_DOP_CATEGORY,
    ];
    for w in order.windows(2) {
        assert!(at(w[0]) < at(w[1]), "{} before {}", w[0], w[1]);
    }
    assert_eq!(h.count(EE_UNITS_DOP_ADD_STRANDED), 0);
    assert_eq!(h.count(EE_UNITS_DOP_SET_BEHAVIOR), 1);
    assert!(h.asked(EE_UNITS_DOP_SET_BEHAVIOR, 900, 0));
}

#[test]
fn a_missing_patrol_group_is_status_3_and_a_lost_owner_strands_the_unit() {
    let mut h = classed()
        .answer(EE_UNITS_DOP_PATROL_IS_UNSET, 0, 0, 0)
        .answer(EE_UNITS_DOP_OWNER_IS_UNSET, 0, 0, 0)
        .answer(EE_UNITS_DOP_OWNER_ID, 0, 0, 77);
    assert_eq!(repair_pointers(&mut h, 1, 0, 0x100), 3);
    assert!(h.asked(EE_UNITS_DOP_WORLD_COMPLEX, 0, 77));
    assert_eq!(h.count(EE_UNITS_DOP_ADD_STRANDED), 1);
    assert_eq!(h.count(EE_UNITS_DOP_PATROL_REFERENCE), 0);
}

#[test]
fn a_failed_first_queue_idles_the_unit() {
    let mut h = classed()
        .sequence(EE_UNITS_DOP_GOAL_QUEUE_COUNT, 0, 0, &[1, 1])
        .sequence(EE_UNITS_DOP_GOAL_COUNT, 0, 0, &[1, 1])
        .answer(EE_UNITS_DOP_GOAL_AT, 0, 0, 40)
        .answer(EE_UNITS_DOP_GOAL_REPAIR, 40, 0, 2)
        .answer(EE_UNITS_DOP_GOAL_KIND, 40, 0, GOAL_PATROL);
    // the patrol goal's failure: repatrolled, queue cleared, idle, physics action 3; the status carries to the end
    assert_eq!(repair_pointers(&mut h, 1, 0, 0x100), 2);
    assert_eq!(h.count(EE_UNITS_DOP_ADD_REPATROL), 1);
    assert!(h.asked(EE_UNITS_DOP_GOAL_QUEUE_CLEAR, 0, 0));
    assert!(h.asked(EE_UNITS_DOP_START_ACTION, 0, ACTION_IDLE));
    assert!(h.asked(EE_UNITS_DOP_SET_PHYSICS_ACTION, 0, PHYSICS_ACTION_REPAIRED));
}

#[test]
fn the_turret_angles_follow_the_model() {
    let mut h = classed()
        .answer(EE_UNITS_DOP_MODEL_PARTS, 0, 0, 3)
        .answer(EE_UNITS_DOP_NUM_TURRET_ANGLES, 0, 0, 1)
        .answer(EE_UNITS_DOP_TURRET_ANGLE, 0, 0, 0x3f80_0000);
    repair_pointers(&mut h, 1, 0, 0x100);
    assert!(h.asked(EE_UNITS_DOP_SCRATCH_TURRETS_NEW, 0, 3));
    let sets: Vec<_> = h.log.iter().filter(|e| e.0 == EE_UNITS_DOP_SCRATCH_TURRET_SET).map(|e| (e.2, e.3)).collect();
    assert_eq!(sets, [(0, 0x3f80_0000), (1, 0), (2, 0)]);
    assert_eq!(h.count(EE_UNITS_DOP_TURRET_ANGLE), 1);
    assert_eq!(h.count(EE_UNITS_DOP_TURRETS_ASSIGN), 1);
}

#[test]
fn old_saves_drop_harass_and_auto_waypoints_are_popped() {
    let mut h = classed()
        .answer(EE_UNITS_DOP_BEHAVIOR_ID, 0, 0, BEHAVIOR_HARASS)
        .sequence(EE_UNITS_DOP_BEHAVIOR, 0, 0, &[0, 0])
        .answer(EE_UNITS_DOP_CLASS_BEHAVIOR, 0, 0, 0)
        .answer(EE_UNITS_DOP_DEFAULT_BEHAVIOR, 0, 0, 905)
        .sequence(EE_UNITS_DOP_GOAL_QUEUE_COUNT, 0, 0, &[1, 1])
        .sequence(EE_UNITS_DOP_GOAL_COUNT, 0, 0, &[1, 1, 3])
        .answer(EE_UNITS_DOP_GOAL_AT, 0, 0, 40)
        .answer(EE_UNITS_DOP_GOAL_KIND, 40, 0, GOAL_LOCATION)
        .answer(EE_UNITS_DOP_LOCATION_AUTO, 40, 0, 1)
        .answer(EE_UNITS_DOP_FIRST_QUEUE_BACK, 0, 0, 41)
        .sequence(EE_UNITS_DOP_IS_AUTO_WAYPOINT_GOAL, 41, 0, &[1, 0]);
    repair_pointers(&mut h, 1, 0, 0xd0);
    assert!(h.asked(EE_UNITS_DOP_SET_BEHAVIOR, 0, 0));
    assert!(h.asked(EE_UNITS_DOP_SET_BEHAVIOR, 905, 0));
    assert_eq!(h.count(EE_UNITS_DOP_FIRST_QUEUE_POP_BACK), 1);
    // minor 0xd0 <= 0xca? no: the deletable flag stays
    assert_eq!(h.count(EE_UNITS_DOP_DELETABLE_FROM_CLASS), 0);
}

#[test]
fn the_copied_constants_match_the_headers_values() {
    assert_eq!(ACTION_IDLE, 5000);
    assert_eq!(GOAL_BUILD_REPAIR, 4);
    assert_eq!(CATEGORY_NAVAL_CITIZEN, 0x16);
}
