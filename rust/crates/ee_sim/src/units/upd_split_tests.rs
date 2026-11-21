use super::*;
use std::collections::HashMap;

/// A recording mock: each statement and its arguments; answers by code (and by the first argument for the parts).
#[derive(Default)]
struct Mock {
    log: Vec<(i32, i64, i64)>,
    answers: HashMap<i32, i64>,
    parts: HashMap<i64, i64>,
}

impl UpdSplitHost for Mock {
    fn act(&mut self, code: i32, a: i64, b: i64) -> i64 {
        self.log.push((code, a, b));
        if code == EE_UNITS_UPD_X_NEXT_PART_OF {
            return self.parts.get(&a).copied().unwrap_or(0);
        }
        self.answers.get(&code).copied().unwrap_or(0)
    }
}

impl Mock {
    fn codes(&self) -> Vec<i32> {
        self.log.iter().map(|l| l.0).collect()
    }
}

#[test]
fn other_codes_pass() {
    let mut m = Mock::default();
    assert_eq!(split(&mut m, EE_UNITS_ACT_RELEASE_HEAL_EFFECT, 0), None);
    assert!(m.log.is_empty());
}

#[test]
fn open_fraction_sets_the_unit_and_its_links() {
    let mut m = Mock::default();
    split(&mut m, EE_UNITS_UPD_SET_OPEN_FRACTION, 7);
    assert_eq!(
        m.log,
        vec![
            (EE_UNITS_UPD_X_OPEN_FRACTION, 7, 0),
            (EE_UNITS_UPD_X_CARRIER_OPEN_FRACTION, 7, 0),
            (EE_UNITS_UPD_X_LINKED_OPEN_FRACTION, 7, 0)
        ]
    );
}

#[test]
fn kill_walks_the_parts_after_each_kill() {
    let mut m = Mock::default();
    m.answers.insert(EE_UNITS_UPD_X_UNIT, 10);
    m.parts.insert(10, 11);
    m.parts.insert(11, 12);
    split(&mut m, EE_UNITS_ACT_KILL_UNIT_AND_PARTS, 0);
    let kills: Vec<i64> = m.log.iter().filter(|l| l.0 == EE_UNITS_UPD_X_KILL_UNIT).map(|l| l.1).collect();
    assert_eq!(kills, vec![10, 11, 12]);
    assert_eq!(m.codes().last(), Some(&EE_UNITS_UPD_X_NEXT_PART_OF));
}

#[test]
fn part_actions_reread_the_part() {
    let mut m = Mock::default();
    m.answers.insert(EE_UNITS_UPD_X_UNIT, 10);
    m.parts.insert(10, 11);
    m.parts.insert(11, 12);
    split(&mut m, EE_UNITS_ACT_GATE_PARTS_ACTION, 3);
    let starts: Vec<(i64, i64)> =
        m.log.iter().filter(|l| l.0 == EE_UNITS_UPD_X_START_ACTION_OF).map(|l| (l.1, l.2)).collect();
    assert_eq!(starts, vec![(11, 3), (12, 3)]);
    let mut m = Mock::default();
    m.answers.insert(EE_UNITS_UPD_X_UNIT, 10);
    m.parts.insert(10, 11);
    split(&mut m, EE_UNITS_ACT_NEXT_PART_ACTION, 4);
    assert_eq!(m.log[2], (EE_UNITS_UPD_X_START_ACTION_OF, 11, 4));
    assert_eq!(m.log[4], (EE_UNITS_UPD_X_ACTION_CHANGED_OF, 11, 0));
    let mut m = Mock::default();
    m.answers.insert(EE_UNITS_UPD_X_UNIT, 10);
    split(&mut m, EE_UNITS_ACT_START_IDLE, 0);
    assert_eq!(m.log[1..], [(EE_UNITS_UPD_X_START_ACTION_OF, 10, 5000), (EE_UNITS_UPD_X_ACTION_CHANGED_OF, 10, 0)]);
}

#[test]
fn healee_and_storage() {
    let mut m = Mock::default();
    m.answers.insert(EE_UNITS_UPD_X_FIND_HEALEE, 1);
    assert_eq!(split(&mut m, EE_UNITS_ACT_FIND_HEALEE, 0), Some(1));
    assert_eq!(m.codes(), vec![EE_UNITS_UPD_X_CLEAR_HEALEE, EE_UNITS_UPD_X_FIND_HEALEE]);
    let mut m = Mock::default();
    split(&mut m, EE_UNITS_ACT_FIND_STORAGE_GOAL, 1);
    assert_eq!(m.log, vec![(EE_UNITS_UPD_X_FIND_STORAGE, 1, 0)]);
    m.answers.insert(EE_UNITS_UPD_X_FIND_STORAGE, 55);
    split(&mut m, EE_UNITS_ACT_FIND_STORAGE_GOAL, 1);
    assert_eq!(m.log[2], (EE_UNITS_UPD_X_GATHER_AT, 55, 0));
}

#[test]
fn sounds_take_the_parameters_then_play() {
    let mut m = Mock::default();
    m.answers.insert(EE_UNITS_UPD_X_DEATH_SOUND, 9);
    split(&mut m, EE_UNITS_ACT_DEATH_SOUND, 0);
    assert_eq!(m.codes(), vec![EE_UNITS_UPD_X_DEATH_SOUND, EE_UNITS_UPD_X_SOUND_PARAMETERS, EE_UNITS_UPD_X_PLAY_SAMPLE]);
    assert_eq!(m.log[1].1, 9);
    assert_eq!(m.log[1].2, m.log[2].2);
    let mut m = Mock::default();
    split(&mut m, EE_UNITS_ACT_GATE_SOUND, 0);
    assert_eq!(m.codes(), vec![EE_UNITS_UPD_X_GATE_SOUND]);
    m.answers.insert(EE_UNITS_UPD_X_GATE_SOUND, 3);
    split(&mut m, EE_UNITS_ACT_GATE_SOUND, 0);
    assert_eq!(*m.codes().last().unwrap(), EE_UNITS_UPD_X_PLAY_SOUND);
}

#[test]
fn retarget_only_with_a_goal_target_and_building() {
    let mut m = Mock::default();
    split(&mut m, EE_UNITS_ACT_RETARGET_NEAREST_BUILDING, 0);
    assert_eq!(m.codes(), vec![EE_UNITS_UPD_X_ATTACK_GOAL]);
    m.answers.insert(EE_UNITS_UPD_X_ATTACK_GOAL, 4);
    m.answers.insert(EE_UNITS_UPD_X_ATTACK_TARGET, 5);
    m.answers.insert(EE_UNITS_UPD_X_NEAREST_TARGET_BUILDING, 6);
    m.log.clear();
    split(&mut m, EE_UNITS_ACT_RETARGET_NEAREST_BUILDING, 0);
    assert_eq!(m.log.last(), Some(&(EE_UNITS_UPD_X_SET_ATTACK_TARGET, 4, 6)));
    assert_eq!(m.log[3], (EE_UNITS_UPD_X_NEAREST_TARGET_BUILDING, 5, 0));
}

#[test]
fn heights_and_takeoff() {
    let mut m = Mock::default();
    split(&mut m, EE_UNITS_ACT_HEIGHT_HERE, 0);
    assert_eq!(m.codes(), vec![EE_UNITS_UPD_X_LOCATION, EE_UNITS_UPD_X_SET_HEIGHT_AT]);
    let mut m = Mock::default();
    m.answers.insert(EE_UNITS_UPD_X_NEXT_WAYPOINT_POINT, 1);
    split(&mut m, EE_UNITS_ACT_HEIGHT_AT_NEXT_WAYPOINT, 0);
    assert_eq!(m.codes(), vec![EE_UNITS_UPD_X_LOCATION, EE_UNITS_UPD_X_NEXT_WAYPOINT_POINT]);
    let mut m = Mock::default();
    m.answers.insert(EE_UNITS_UPD_X_EXITING_PASSENGER, 8);
    split(&mut m, EE_UNITS_ACT_QUEUE_TAKEOFF, 0);
    assert_eq!(m.log[1], (EE_UNITS_UPD_X_QUEUE_TAKEOFF, 8, 0));
}

#[test]
fn aircraft_dispatch() {
    let mut m = Mock::default();
    split(&mut m, EE_UNITS_ACT_DEATH_ROLL, 0);
    split(&mut m, EE_UNITS_ACT_DEATH_ROLL, 2);
    split(&mut m, EE_UNITS_ACT_HELICOPTER_PATH, 1);
    split(&mut m, EE_UNITS_ACT_HELICOPTER_PATH, 2);
    split(&mut m, EE_UNITS_ACT_HOLDING_PATTERN, 1);
    m.answers.insert(EE_UNITS_UPD_X_LANDABLE, 7);
    assert_eq!(split(&mut m, EE_UNITS_ACT_TAKEOFF, 3), Some(7));
    assert_eq!(
        m.log,
        vec![
            (EE_UNITS_UPD_X_AIR_ANIMAL_DEATH_ROLL, 0, 0),
            (EE_UNITS_UPD_X_LANDABLE, i64::from(EE_UNITS_ACT_DEATH_ROLL), 2),
            (EE_UNITS_UPD_X_HELICOPTER_TRANSPORT_PATH, 0, 0),
            (EE_UNITS_UPD_X_HELICOPTER_PRIEST_PATH, 0, 0),
            (EE_UNITS_UPD_X_SPACESHIP_HOLDING, 0, 0),
            (EE_UNITS_UPD_X_LANDABLE, i64::from(EE_UNITS_ACT_TAKEOFF), 3),
        ]
    );
}
