use super::*;
use crate::units::player_areas::EE_UNITS_PLAYER_HERDS;

/// Runs `op` with `args` over answers by operation (each answer used once, in order), logging the calls.
/// The calls a split made: each operation with its first two arguments.
type Log = Vec<(i32, Vec<i64>)>;

fn run(op: i32, args: &[i64], answers: &[(i32, i64)]) -> (Option<i64>, EeUnitsPlayerIo, Log) {
    let mut log = Vec::new();
    let mut io = EeUnitsPlayerIo::default();
    io.i[..args.len()].copy_from_slice(args);
    let mut queue: Vec<(i32, i64)> = answers.to_vec();
    let mut f = |op: i32, io: &mut EeUnitsPlayerIo| {
        log.push((op, io.i[..2].to_vec()));
        let v = match queue.iter().position(|&(o, _)| o == op) {
            Some(k) => queue.remove(k).1,
            None => 0,
        };
        io.f[0] = v as f32 / 4.0;
        v
    };
    let r = split(op, &mut io, &mut f);
    (r, io, log)
}

fn ops(log: &[(i32, Vec<i64>)]) -> Vec<i32> {
    log.iter().map(|l| l.0).collect()
}

#[test]
fn a_class_past_the_table_or_missing_is_none() {
    assert_eq!(run(EE_UNITS_PLAYER_CLASS, &[5], &[(EE_UNITS_PLAYER_X_CLASS_COUNT, 5)]).0, Some(0));
    assert_eq!(run(EE_UNITS_PLAYER_CLASS, &[4], &[(EE_UNITS_PLAYER_X_CLASS_COUNT, 5)]).0, Some(0));
    let (r, io, _) = run(
        EE_UNITS_PLAYER_CLASS,
        &[4],
        &[
            (EE_UNITS_PLAYER_X_CLASS_COUNT, 5),
            (EE_UNITS_PLAYER_X_CLASS_AT, 0x70),
            (EE_UNITS_PLAYER_X_CLASS_TECH, 1025),
            (EE_UNITS_PLAYER_X_CLASS_WEIGHT, 2),
            (EE_UNITS_PLAYER_X_CLASS_POPULATION, 3),
            (EE_UNITS_PLAYER_X_CLASS_TYPE, TYPE_TELEPORTER),
            (EE_UNITS_PLAYER_X_CLASS_AI_KIND, 1),
            (EE_UNITS_PLAYER_X_CLASS_IMPROVEMENTS, 0x90),
        ],
    );
    assert_eq!(r, Some(1));
    assert_eq!(io.i[..6], [1025, 2, 3, 1, 0, 1]);
}

#[test]
fn improvements_read_the_table_and_the_upgrade() {
    let base = [
        (EE_UNITS_PLAYER_X_CLASS_COUNT, 9),
        (EE_UNITS_PLAYER_X_CLASS_AT, 0x70),
        (EE_UNITS_PLAYER_X_CLASS_IMPROVEMENTS, 0x90),
        (EE_UNITS_PLAYER_X_UPGRADE_AT, 0x99),
    ];
    let mut answers = base.to_vec();
    answers.extend([(EE_UNITS_PLAYER_X_TABLE_STEP, 2), (EE_UNITS_PLAYER_X_TABLE_COST_FACTOR, 6)]);
    answers.extend([(EE_UNITS_PLAYER_X_UP_LEVEL, 3), (EE_UNITS_PLAYER_X_UP_CAN_UPGRADE, 1)]);
    answers.extend([(EE_UNITS_PLAYER_X_UP_COST, 40), (EE_UNITS_PLAYER_X_UP_TOTAL, 90)]);
    answers.extend([(EE_UNITS_PLAYER_X_UP_LEVEL, 3), (EE_UNITS_PLAYER_X_TABLE_MAX_LEVEL, 5)]);
    answers.push((EE_UNITS_PLAYER_X_POINTS_MAX, 20));
    let (r, io, _) = run(EE_UNITS_PLAYER_IMPROVEMENT, &[2, 7], &answers);
    assert_eq!(r, Some(1));
    assert_eq!(io.i[..6], [1, 40, 90, 3, 5, 20]);
    assert_eq!(io.f, [0.5, 1.5]);
    // improvement 12 has no step
    let (_, io, log) = run(EE_UNITS_PLAYER_IMPROVEMENT, &[2, 12], &answers);
    assert_eq!(io.f[0], 0.0);
    assert!(!ops(&log).contains(&EE_UNITS_PLAYER_X_TABLE_STEP));
    // the level alone
    let mut answers = base.to_vec();
    answers.push((EE_UNITS_PLAYER_X_UP_LEVEL, 4));
    let (r, io, log) = run(EE_UNITS_PLAYER_LEVEL, &[2, 7], &answers);
    assert_eq!((r, io.i[0]), (Some(1), 4));
    assert_eq!(ops(&log).last(), Some(&EE_UNITS_PLAYER_X_UP_LEVEL));
    // no upgrade
    assert_eq!(run(EE_UNITS_PLAYER_IMPROVEMENT_COST, &[2, 7], &base[..3]).0, Some(0));
}

#[test]
fn the_exploration_pulse_passes_the_amount() {
    let (_, _, log) = run(EE_UNITS_PLAYER_PULSE_EXPLORATION, &[0x40], &[(EE_UNITS_PLAYER_X_EXPLORATION_AMOUNT, 8)]);
    assert_eq!(ops(&log), vec![EE_UNITS_PLAYER_X_EXPLORATION_AMOUNT, EE_UNITS_PLAYER_X_PULSE_SIGHT]);
    assert_eq!(log[1].1[0], 0x40);
}

#[test]
fn a_new_area_is_listed_or_deleted() {
    let (r, _, log) = run(
        EE_UNITS_PLAYER_AREA_NEW,
        &[EE_UNITS_PLAYER_TOWNS, 0x40],
        &[(EE_UNITS_PLAYER_X_TOWN_NEW, 0x80), (EE_UNITS_PLAYER_AREA_ADD, 1)],
    );
    assert_eq!(r, Some(1));
    assert_eq!(
        ops(&log),
        vec![
            EE_UNITS_PLAYER_X_TOWN_NEW,
            EE_UNITS_PLAYER_X_TOWN_SERIAL,
            EE_UNITS_PLAYER_X_TOWN_ADD_AREA,
            EE_UNITS_PLAYER_AREA_ADD,
            EE_UNITS_PLAYER_X_TOWNS_PUSH
        ]
    );
    let (r, _, log) =
        run(EE_UNITS_PLAYER_AREA_NEW, &[EE_UNITS_PLAYER_HERDS, 0x40], &[(EE_UNITS_PLAYER_X_HERD_NEW, 0x80)]);
    assert_eq!(r, Some(0));
    assert_eq!(log[0], (EE_UNITS_PLAYER_X_HERD_NEW, vec![0x40, 0]));
    assert_eq!(ops(&log).last(), Some(&EE_UNITS_PLAYER_X_HERD_DELETE));
}

#[test]
fn a_merged_town_kills_its_patrol_points() {
    let (r, _, log) = run(
        EE_UNITS_PLAYER_TOWN_MERGE,
        &[0x10, 0x20],
        &[
            (EE_UNITS_PLAYER_AREA_COUNT, 2),
            (EE_UNITS_PLAYER_AREA_AT, 0x10),
            (EE_UNITS_PLAYER_AREA_AT, 0x20),
            (EE_UNITS_PLAYER_X_TOWN_PATROL_OF, 0x77),
            (EE_UNITS_PLAYER_X_PG_LAST, 0x55),
        ],
    );
    assert_eq!(r, Some(0x77));
    let o = ops(&log);
    assert_eq!(o[..2], [EE_UNITS_PLAYER_X_TOWN_MERGE, EE_UNITS_PLAYER_X_TOWN_REMOVE_AREA]);
    assert!(log.contains(&(EE_UNITS_PLAYER_X_TOWNS_ERASE, vec![1, 0])));
    let kill = o.iter().position(|&x| x == EE_UNITS_PLAYER_X_POINT_KILL).unwrap();
    assert_eq!(
        o[kill - 2..kill + 2],
        [
            EE_UNITS_PLAYER_X_PG_REMOVE,
            EE_UNITS_PLAYER_X_POINT_UNPATROL,
            EE_UNITS_PLAYER_X_POINT_KILL,
            EE_UNITS_PLAYER_X_POINT_KILL_UNIT
        ]
    );
    assert_eq!(o[o.len() - 2..], [EE_UNITS_PLAYER_X_PG_REFERENCE, EE_UNITS_PLAYER_X_TOWN_DELETE]);
    // a town not listed stays (the original's early return)
    let (r, _, log) = run(EE_UNITS_PLAYER_TOWN_MERGE, &[0x10, 0x20], &[]);
    assert_eq!(r, Some(0));
    assert!(!ops(&log).contains(&EE_UNITS_PLAYER_X_TOWN_DELETE));
}

#[test]
fn the_patrols_move_to_the_town_group() {
    let old = [0x61i64, 0x62];
    let (_, _, log) = run(
        EE_UNITS_PLAYER_TOWN_REPATROL,
        &[0x10, old.as_ptr() as i64, 2],
        &[
            (EE_UNITS_PLAYER_X_TOWN_PATROL_OF, 0x70),
            (EE_UNITS_PLAYER_X_FIRST_UNIT, 0x100),
            (EE_UNITS_PLAYER_X_COMPLEX, 0x100),
            (EE_UNITS_PLAYER_X_PATROL_GOAL, 0x300),
            (EE_UNITS_PLAYER_X_GOAL_PATROL_GROUP, 0x62),
        ],
    );
    assert!(log.contains(&(EE_UNITS_PLAYER_X_GOAL_SET_PATROL, vec![0x300, 0x70])));
    assert_eq!(log.iter().filter(|l| l.0 == EE_UNITS_PLAYER_X_PG_DEREFERENCE).count(), 2);
    // no town group: nothing
    assert_eq!(run(EE_UNITS_PLAYER_TOWN_REPATROL, &[0x10, old.as_ptr() as i64, 2], &[]).2.len(), 1);
}

#[test]
fn the_tiles() {
    let (_, io, _) =
        run(EE_UNITS_PLAYER_AREA_TILE, &[0x40], &[(EE_UNITS_PLAYER_X_FIND_AREA_TILE, (3 << 32) | 0xffff_fffe)]);
    assert_eq!(io.i[..2], [3, -2]);
    let (_, io, _) = run(
        EE_UNITS_PLAYER_ASSIGN_TILE,
        &[0x40],
        &[
            (EE_UNITS_PLAYER_X_TILE_X, OFF_MAP),
            (EE_UNITS_PLAYER_X_LAST_FLOOR_X, 7),
            (EE_UNITS_PLAYER_X_LAST_FLOOR_Y, 8),
        ],
    );
    assert_eq!(io.i[..2], [7, 8]);
    let (_, _, log) = run(EE_UNITS_PLAYER_TOWN_PATROL, &[0x10], &[(EE_UNITS_PLAYER_X_TOWN_PATROL_OF, 1)]);
    assert_eq!(log.len(), 1);
    let (_, _, log) = run(EE_UNITS_PLAYER_AREA_DROP, &[EE_UNITS_PLAYER_HERDS, 2], &[(EE_UNITS_PLAYER_AREA_AT, 0x44)]);
    assert_eq!(
        ops(&log)[1..],
        [EE_UNITS_PLAYER_X_HERD_REMOVE_AREA, EE_UNITS_PLAYER_X_HERDS_ERASE, EE_UNITS_PLAYER_X_HERD_DELETE]
    );
    assert_eq!(run(41, &[], &[]).0, None);
}

#[test]
fn an_improvement_is_paid_on_its_class_or_none() {
    let (_, _, log) =
        run(crate::units::player::EE_UNITS_PLAYER_PAY_IMPROVEMENT, &[5, 3], &[(EE_UNITS_PLAYER_X_CLASS_COUNT, 5)]);
    assert_eq!(log.last(), Some(&(EE_UNITS_PLAYER_X_PAY_IMPROVEMENT, vec![0, 3])));
}
