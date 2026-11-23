use super::*;
use std::collections::BTreeMap;

/// A recording callback: each operation (op, index, the io's i, f, b on the way in); answers from `ints`, `floats`
/// keyed by (op, index) into the io, `results` as the result (0 else).
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u32, i64, f32, u8)>,
    ints: BTreeMap<(i32, u32), i64>,
    floats: BTreeMap<(i32, u32), f32>,
    results: BTreeMap<(i32, u32), i32>,
}

impl Mock {
    fn int(mut self, op: i32, index: u32, v: i64) -> Self {
        self.ints.insert((op, index), v);
        self
    }
    fn float(mut self, op: i32, index: u32, v: f32) -> Self {
        self.floats.insert((op, index), v);
        self
    }
    fn result(mut self, op: i32, index: u32, r: i32) -> Self {
        self.results.insert((op, index), r);
        self
    }
    fn ops(&self) -> Vec<(i32, u32)> {
        self.log.iter().map(|e| (e.0, e.1)).collect()
    }
    fn writes(&self) -> Vec<(i32, i64, f32, u8)> {
        self.log
            .iter()
            .filter(|e| {
                matches!(
                    e.0,
                    EE_UNITS_UNIT_SAVE_WRITE_LONG
                        | EE_UNITS_UNIT_SAVE_WRITE_ULONG
                        | EE_UNITS_UNIT_SAVE_WRITE_FLOAT
                        | EE_UNITS_UNIT_SAVE_WRITE_BOOL
                )
            })
            .map(|e| (e.0, e.2, e.3, e.4))
            .collect()
    }
}

impl SaveHost for Mock {
    fn op(&mut self, op: i32, index: u32, io: &mut EeUnitsUnitSaveIo) -> i32 {
        self.log.push((op, index, io.i, io.f, io.b));
        if let Some(v) = self.ints.get(&(op, index)) {
            io.i = *v;
            io.b = u8::from(*v != 0);
        }
        if let Some(v) = self.floats.get(&(op, index)) {
            io.f = *v;
        }
        *self.results.get(&(op, index)).unwrap_or(&0)
    }
}

fn run(m: &mut Mock, op: i32, index: u32, io: &mut EeUnitsUnitSaveIo) -> i32 {
    DopSaveHost { inner: m }.op(op, index, io)
}

const L: i32 = EE_UNITS_UNIT_SAVE_WRITE_LONG;
const UL: i32 = EE_UNITS_UNIT_SAVE_WRITE_ULONG;
const F: i32 = EE_UNITS_UNIT_SAVE_WRITE_FLOAT;
const B: i32 = EE_UNITS_UNIT_SAVE_WRITE_BOOL;

#[test]
fn other_operations_pass_through() {
    let mut m = Mock::default().result(EE_UNITS_UNIT_SAVE_PART, EE_UNITS_UNIT_SAVE_PART_COLOR, 3);
    let mut io = EeUnitsUnitSaveIo::default();
    assert_eq!(run(&mut m, EE_UNITS_UNIT_SAVE_PART, EE_UNITS_UNIT_SAVE_PART_COLOR, &mut io), 3);
    assert_eq!(m.ops(), vec![(EE_UNITS_UNIT_SAVE_PART, EE_UNITS_UNIT_SAVE_PART_COLOR)]);
}

#[test]
fn the_citizen_queue_writes_its_valid_entries() {
    let mut m = Mock::default()
        .int(EE_UNITS_DOPS_CITIZEN_QUEUE_SIZE, 0, 3)
        .int(EE_UNITS_DOPS_CITIZEN_VALID, 0, 1)
        .int(EE_UNITS_DOPS_CITIZEN_VALID, 2, 1)
        .int(EE_UNITS_DOPS_CITIZEN_ID, 0, 70)
        .int(EE_UNITS_DOPS_CITIZEN_ID, 2, 72);
    let r = run(&mut m, EE_UNITS_UNIT_SAVE_SECTION, EE_UNITS_UNIT_SAVE_SECTION_CITIZEN_QUEUE, &mut Default::default());
    assert_eq!(r, 0);
    assert_eq!(m.writes(), vec![(L, 2, 0.0, 0), (UL, 70, 0.0, 0), (UL, 72, 0.0, 0)]);
}

#[test]
fn the_waypoints_stop_at_the_first_error() {
    let mut m = Mock::default()
        .int(EE_UNITS_DOPS_WAYPOINT_COUNT, 0, 2)
        .float(EE_UNITS_DOPS_WAYPOINT_DISTANCE, 0, 1.5)
        .float(EE_UNITS_DOPS_WAYPOINT_START_DISTANCE, 0, 2.5)
        .result(EE_UNITS_DOPS_WAYPOINT_SAVE_LOCATION, 1, 9);
    let r = run(&mut m, EE_UNITS_UNIT_SAVE_SECTION, EE_UNITS_UNIT_SAVE_SECTION_WAYPOINTS, &mut Default::default());
    assert_eq!(r, 9);
    assert_eq!(m.writes(), vec![(L, 2, 0.0, 0), (F, 0, 1.5, 0), (F, 0, 2.5, 0)]);
    let saves: Vec<u32> = m.log.iter().filter(|e| e.0 == EE_UNITS_DOPS_WAYPOINT_SAVE_LOCATION).map(|e| e.1).collect();
    assert_eq!(saves, vec![0, 1]);
}

#[test]
fn turrets_goal_queues_and_ignored_targets() {
    let mut m = Mock::default().int(EE_UNITS_DOPS_MODEL_PARTS, 0, 2).float(EE_UNITS_DOPS_TURRET_ANGLE, 0, 0.25).float(
        EE_UNITS_DOPS_TURRET_ANGLE,
        1,
        0.5,
    );
    run(&mut m, EE_UNITS_UNIT_SAVE_SECTION, EE_UNITS_UNIT_SAVE_SECTION_TURRET_ANGLES, &mut Default::default());
    assert_eq!(m.writes(), vec![(L, 2, 0.0, 0), (F, 0, 0.25, 0), (F, 0, 0.5, 0)]);

    let mut m = Mock::default().int(EE_UNITS_DOPS_GOAL_QUEUE_COUNT, 0, 3).result(EE_UNITS_DOPS_GOAL_QUEUE_SAVE, 1, 4);
    let r = run(&mut m, EE_UNITS_UNIT_SAVE_SECTION, EE_UNITS_UNIT_SAVE_SECTION_GOAL_QUEUES, &mut Default::default());
    assert_eq!(r, 4);
    assert_eq!(m.writes(), vec![(UL, 3, 0.0, 0)]);
    assert_eq!(m.log.iter().filter(|e| e.0 == EE_UNITS_DOPS_GOAL_QUEUE_SAVE).count(), 2);

    let mut m = Mock::default()
        .int(EE_UNITS_DOPS_IGNORED_COUNT, 0, 1)
        .int(EE_UNITS_DOPS_IGNORED_TILE_X, 0, 1)
        .int(EE_UNITS_DOPS_IGNORED_TILE_Y, 0, 2)
        .int(EE_UNITS_DOPS_IGNORED_FROM_X, 0, 3)
        .int(EE_UNITS_DOPS_IGNORED_FROM_Y, 0, 4)
        .int(EE_UNITS_DOPS_IGNORED_UNIT_ID, 0, 5)
        .int(EE_UNITS_DOPS_IGNORED_TIME, 0, 6);
    run(&mut m, EE_UNITS_UNIT_SAVE_SECTION, EE_UNITS_UNIT_SAVE_SECTION_IGNORED_TARGETS, &mut Default::default());
    let w: Vec<(i32, i64)> = m.writes().iter().map(|w| (w.0, w.1)).collect();
    assert_eq!(w, vec![(UL, 1), (L, 1), (L, 2), (L, 3), (L, 4), (UL, 5), (UL, 6)]);
}

#[test]
fn the_path_part_and_the_herd() {
    let mut m = Mock::default().int(EE_UNITS_DOPS_PATH_GOAL, 0, 1);
    run(&mut m, EE_UNITS_UNIT_SAVE_PART, EE_UNITS_UNIT_SAVE_PART_PATH, &mut Default::default());
    assert_eq!(
        m.ops(),
        vec![(EE_UNITS_DOPS_TILE_RESET, 0), (EE_UNITS_DOPS_PATH_GOAL, 0), (B, 0), (EE_UNITS_DOPS_TILE_SAVE, 0)]
    );
    assert_eq!(m.writes(), vec![(B, 0, 0.0, 1)]);
    let mut m = Mock::default();
    assert_eq!(run(&mut m, EE_UNITS_UNIT_SAVE_PART, EE_UNITS_UNIT_SAVE_PART_HERD_HAS_UNIT, &mut Default::default()), 0);
    assert_eq!(m.ops(), vec![(EE_UNITS_DOPS_HAS_HERD, 0)]);
}

#[test]
fn a_path_without_a_goal_is_queued_pending() {
    let mut m = Mock::default();
    run(&mut m, EE_UNITS_UNIT_LOAD_PART, EE_UNITS_UNIT_LOAD_PART_PATH, &mut Default::default());
    assert_eq!(
        m.ops(),
        vec![
            (EE_UNITS_DOPS_TILE_RESET, 0),
            (EE_UNITS_PLAYER_SAVE_READ_BOOL, 0),
            (EE_UNITS_DOPS_TILE_LOAD, 0),
            (EE_UNITS_DOPS_ADD_PENDING, 0)
        ]
    );
    let mut m = Mock::default().int(EE_UNITS_PLAYER_SAVE_READ_BOOL, 0, 1);
    run(&mut m, EE_UNITS_UNIT_LOAD_PART, EE_UNITS_UNIT_LOAD_PART_PATH, &mut Default::default());
    assert!(!m.ops().contains(&(EE_UNITS_DOPS_ADD_PENDING, 0)));
    let mut m = Mock::default().result(EE_UNITS_DOPS_LOCATION_LOAD, 0, 2);
    assert_eq!(run(&mut m, EE_UNITS_UNIT_LOAD_PART, EE_UNITS_UNIT_LOAD_PART_LOCATION, &mut Default::default()), 2);
    assert!(!m.ops().contains(&(EE_UNITS_DOPS_SET_LOCATION, 0)));
}

#[test]
fn the_scale_and_action_sets() {
    // a scenario's unit takes its class's scale
    let mut m = Mock::default().int(EE_UNITS_DOPS_LOADING_SCENARIO, 0, 1).int(EE_UNITS_DOPS_HAS_CLASS, 0, 1).float(
        EE_UNITS_DOPS_CLASS_SCALE,
        0,
        1.25,
    );
    let mut io = EeUnitsUnitSaveIo { f: 0.5, ..Default::default() };
    run(&mut m, EE_UNITS_UNIT_LOAD_SET, EE_UNITS_UNIT_LOAD_SET_SCALE, &mut io);
    assert_eq!(m.log.last().map(|e| (e.0, e.3)), Some((EE_UNITS_DOPS_SET_SCALE, 1.25)));
    // a zero scale without a class is 1
    let mut m = Mock::default();
    run(&mut m, EE_UNITS_UNIT_LOAD_SET, EE_UNITS_UNIT_LOAD_SET_SCALE, &mut Default::default());
    assert_eq!(m.log.last().map(|e| (e.0, e.3)), Some((EE_UNITS_DOPS_SET_SCALE, 1.0)));
    // a saved action 0 the class knows starts as idle
    let mut m = Mock::default().int(EE_UNITS_DOPS_HAS_CLASS, 0, 1).result(EE_UNITS_DOPS_FINDS_ACTION, 0, 1);
    run(&mut m, EE_UNITS_UNIT_LOAD_SET, EE_UNITS_UNIT_LOAD_SET_ACTION, &mut Default::default());
    assert_eq!(m.log.last().map(|e| (e.0, e.2)), Some((EE_UNITS_DOPS_START_ACTION, 5000)));
    let mut m = Mock::default().int(EE_UNITS_DOPS_HAS_OWNER, 0, 1);
    run(&mut m, EE_UNITS_UNIT_LOAD_SET, EE_UNITS_UNIT_LOAD_SET_OWNER_CLEAR, &mut Default::default());
    assert_eq!(
        m.ops(),
        vec![(EE_UNITS_DOPS_HAS_OWNER, 0), (EE_UNITS_DOPS_DEREFERENCE_OWNER, 0), (EE_UNITS_DOPS_CLEAR_OWNER, 0)]
    );
    let mut m = Mock::default();
    run(&mut m, EE_UNITS_UNIT_LOAD_SET, EE_UNITS_UNIT_LOAD_SET_LAST_ATTACKER_CLEAR, &mut Default::default());
    assert_eq!(m.ops(), vec![(EE_UNITS_DOPS_HAS_LAST_ATTACKER, 0), (EE_UNITS_DOPS_CLEAR_LAST_ATTACKER, 0)]);
    // a set this host does not do passes through
    let mut m = Mock::default();
    run(&mut m, EE_UNITS_UNIT_LOAD_SET, EE_UNITS_UNIT_LOAD_SET_TURRETS, &mut Default::default());
    assert_eq!(m.ops(), vec![(EE_UNITS_UNIT_LOAD_SET, EE_UNITS_UNIT_LOAD_SET_TURRETS)]);
}

#[test]
fn the_load_sections() {
    let mut m =
        Mock::default().int(EE_UNITS_PLAYER_SAVE_READ_LONG, 0, 1).float(EE_UNITS_PLAYER_SAVE_READ_FLOAT, 0, 3.0);
    let r = run(&mut m, EE_UNITS_UNIT_LOAD_SECTION, EE_UNITS_UNIT_LOAD_SECTION_WAYPOINTS, &mut Default::default());
    assert_eq!(r, 0);
    let ops: Vec<i32> = m.ops().iter().map(|o| o.0).collect();
    assert_eq!(
        ops,
        vec![
            EE_UNITS_DOPS_WAYPOINTS_CLEAR,
            EE_UNITS_PLAYER_SAVE_READ_LONG,
            EE_UNITS_DOPS_WAYPOINT_LOAD_LOCATION,
            EE_UNITS_PLAYER_SAVE_READ_FLOAT,
            EE_UNITS_DOPS_SET_WAYPOINT_DISTANCE,
            EE_UNITS_PLAYER_SAVE_READ_FLOAT,
            EE_UNITS_DOPS_SET_WAYPOINT_START_DISTANCE,
            EE_UNITS_DOPS_WAYPOINT_PUSH,
        ]
    );
    // two saved queues, the unit has one: the second goes to the spare queue; then the effects
    let mut m = Mock::default().int(EE_UNITS_PLAYER_SAVE_READ_ULONG, 0, 2).int(EE_UNITS_DOPS_GOAL_QUEUE_COUNT, 0, 1);
    run(&mut m, EE_UNITS_UNIT_LOAD_SECTION, EE_UNITS_UNIT_LOAD_SECTION_GOAL_QUEUES, &mut Default::default());
    let ops: Vec<i32> = m.ops().iter().map(|o| o.0).collect();
    assert_eq!(
        ops,
        vec![
            EE_UNITS_PLAYER_SAVE_READ_ULONG,
            EE_UNITS_DOPS_GOAL_QUEUE_COUNT,
            EE_UNITS_DOPS_GOAL_QUEUE_LOAD,
            EE_UNITS_DOPS_GOAL_QUEUE_COUNT,
            EE_UNITS_DOPS_SPARE_QUEUE_LOAD,
            EE_UNITS_DOPS_READ_CONVERSION_EFFECT,
            EE_UNITS_DOPS_READ_HEAL_EFFECT,
        ]
    );
}
