use super::*;
use std::collections::HashMap;

/// A recording mock: each statement (op, unit, a, c); answers by (op, a), else by op.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, u64, i64)>,
    by_object: HashMap<(i32, u64), i64>,
    by_op: HashMap<i32, i64>,
    floats: HashMap<i32, f32>,
    targetable: Vec<(u64, u64)>,
    capture: bool,
    records: Vec<Record>,
}

impl Mock {
    fn set(&mut self, op: i32, a: u64, v: i64) {
        self.by_object.insert((op, a), v);
    }
    fn codes(&self) -> Vec<i32> {
        self.log.iter().map(|l| l.0).collect()
    }
}

impl SrHost for Mock {
    fn sr(&mut self, op: i32, unit: u64, a: u64, c: i64, f: &mut [f32; 4]) -> i64 {
        self.log.push((op, unit, a, c));
        if let Some(v) = self.floats.get(&op) {
            f[0] = *v;
        }
        if op == EE_UNITS_SR_FOOTPRINT_TILE {
            // SAFETY: the search's two longs
            unsafe { *(c as usize as *mut [c_long; 2]) = [3, 4] };
        }
        if op == EE_UNITS_SR_UNIT_ID {
            return a as i64 * 10;
        }
        self.by_object.get(&(op, a)).or_else(|| self.by_op.get(&op)).copied().unwrap_or(0)
    }
    fn targetable(&mut self, unit: u64, class: u64) -> bool {
        self.targetable.push((unit, class));
        true
    }
    fn targeting_table(&mut self) -> [i32; 30] {
        [7; 30]
    }
    fn regions_size(&mut self, _regions: u64) -> i32 {
        16
    }
    fn capture(&mut self) -> bool {
        self.capture
    }
    fn commit(&mut self, record: &Record) {
        self.records.push(record.clone());
    }
}

const UNIT: u64 = 100;

fn words(r: &Record) -> Vec<i32> {
    r.data.chunks(4).map(|c| i32::from_le_bytes([c[0], c[1], c[2], c[3]])).collect()
}

#[test]
fn tags_are_first_letter_low() {
    assert_eq!(TAG_CBST, 0x5453_4243);
    assert_eq!(TAG_CBSL & 0xff, u32::from(b'C'));
    assert_eq!(TAG_CBPU >> 24, u32::from(b'U'));
    assert_eq!(TAG_CBFT, u32::from_le_bytes(*b"CBFT"));
}

#[test]
fn region_candidate_short_circuits_in_order() {
    let mut m = Mock::default();
    m.set(EE_UNITS_SR_ENEMY, 5, 1);
    m.set(EE_UNITS_SR_HIT_POINTS, 5, 10);
    m.set(EE_UNITS_SR_DISABLED, 5, 1);
    let mut s = search(&mut m, Kind::Regions, UNIT);
    assert!(!s.test(EE_COMBAT_SEARCH_TEST_CANDIDATE, 5));
    assert_eq!(
        m.codes(),
        vec![
            EE_UNITS_SR_SAME_PLAYER,
            EE_UNITS_SR_ENEMY,
            EE_UNITS_SR_HIT_POINTS,
            EE_UNITS_SR_DISABLED,
            EE_UNITS_SR_PROTECTED
        ]
    );
    let mut m = Mock::default();
    m.set(EE_UNITS_SR_SAME_PLAYER, 5, 1);
    let mut s = search(&mut m, Kind::Regions, UNIT);
    assert!(!s.test(EE_COMBAT_SEARCH_TEST_CANDIDATE, 5));
    assert_eq!(m.codes(), vec![EE_UNITS_SR_SAME_PLAYER]);
}

#[test]
fn region_and_list_priests_read_different_classes() {
    let mut m = Mock::default();
    let mut s = search(&mut m, Kind::Regions, UNIT);
    s.test(EE_COMBAT_SEARCH_TEST_PRIEST, 5);
    assert_eq!(m.log[0], (EE_UNITS_SR_CLASS, UNIT, 5, 0));
    let mut m = Mock::default();
    let mut s = search(&mut m, Kind::AlongList, UNIT);
    s.test(EE_COMBAT_SEARCH_TEST_PRIEST, 5);
    assert_eq!(m.log[0], (EE_UNITS_SR_CLASS, UNIT, UNIT, 0));
}

#[test]
fn list_tests_guard_a_missing_complex_unit() {
    let mut m = Mock::default();
    let mut s = search(&mut m, Kind::AlongList, UNIT);
    assert!(!s.test(EE_COMBAT_SEARCH_TEST_FOUNDATION, 5));
    assert!(!s.test(EE_COMBAT_SEARCH_TEST_ATTACKS_UNIT, 5));
    assert!(!s.test(EE_COMBAT_SEARCH_TEST_PURSUES, 5));
    assert!(m.codes().iter().all(|&c| c == EE_UNITS_SR_COMPLEX));
    // the ignored list's size first
    let mut m = Mock::default();
    let mut s = search(&mut m, Kind::AlongList, UNIT);
    assert!(!s.test(EE_COMBAT_SEARCH_TEST_IGNORED, 5));
    assert_eq!(m.codes(), vec![EE_UNITS_SR_IGNORED_COUNT]);
}

#[test]
fn list_tech_kind_and_pursues() {
    let mut m = Mock::default();
    m.set(EE_UNITS_SR_TECH_NODE, 5, 50);
    m.set(EE_UNITS_SR_NODE_KIND, 50, 6);
    let mut s = search(&mut m, Kind::AlongList, UNIT);
    assert!(s.test(EE_COMBAT_SEARCH_TEST_TECH_KIND_6, 5));
    assert!(!s.test(EE_COMBAT_SEARCH_TEST_TECH_KIND_6, 6));
    let mut m = Mock::default();
    m.set(EE_UNITS_SR_COMPLEX, 5, 55);
    m.set(EE_UNITS_SR_GOAL_STATE, 55, 70);
    m.set(EE_UNITS_SR_GATHER_CAST, 70, 71);
    m.set(EE_UNITS_SR_GATHER_RESOURCE, 71, UNIT as i64);
    let mut s = search(&mut m, Kind::Pursuer, UNIT);
    assert!(s.test(EE_COMBAT_SEARCH_TEST_PURSUES, 5));
    assert_eq!(
        m.codes(),
        vec![
            EE_UNITS_SR_COMPLEX,
            EE_UNITS_SR_LAUNCHER,
            EE_UNITS_SR_GOAL_STATE,
            EE_UNITS_SR_GOAL_STATE,
            EE_UNITS_SR_ATTACK_CAST,
            EE_UNITS_SR_GATHER_CAST,
            EE_UNITS_SR_GATHER_RESOURCE
        ]
    );
}

#[test]
fn callbacks_marshal_their_outputs() {
    let mut m = Mock::default();
    m.floats.insert(EE_UNITS_SR_HIT_DISTANCE, 2.5);
    m.set(EE_UNITS_SR_COMPLEX, 5, 55);
    let mut s = search(&mut m, Kind::AlongList, UNIT);
    assert_eq!(s.hit_distance_squared(5, 1.0, 2.0), 2.5);
    assert_eq!(s.footprint_tile(5), (3, 4));
    s.reachable(5);
    s.reachable(5);
    s.ignore(5);
    // the list search's reach tile is one, kept across calls; the ignore takes the complex unit
    let reach: Vec<_> = m.log.iter().filter(|l| l.0 == EE_UNITS_SR_REACHABLE).collect();
    assert_eq!((reach[0].2, reach[0].3), (55, reach[1].3));
    assert_eq!(*m.log.last().unwrap(), (EE_UNITS_SR_IGNORE, UNIT, 55, 0));
    let mut m = Mock::default();
    let mut s = search(&mut m, Kind::Regions, UNIT);
    s.ignore(5);
    s.list_next(5, true);
    assert_eq!(m.codes(), vec![EE_UNITS_SR_IGNORE, EE_UNITS_SR_LIST_PREV]);
}

#[test]
fn search_target_records_cbst() {
    let mut m = Mock { capture: true, ..Default::default() };
    m.floats.insert(EE_UNITS_SR_RANGE, 4.0);
    m.by_op.insert(EE_UNITS_SR_MELEE, 1);
    m.by_op.insert(EE_UNITS_SR_TICK_RANDOM, 9);
    let best = search_target(&mut m, UNIT, [1, 2, 3, 4], 5, 6, 0.5, &mut |i, _r, calls| {
        assert_eq!((i.left, i.bottom, i.region_size, i.range, i.table[0]), (1, 4, 16, 4.0, 7));
        calls.trace(7, EE_COMBAT_SEARCH_TRACE_BEST, 33);
        EeCombatSearchOut { best: 7, score: 33, done: 1 }
    });
    assert_eq!(best, 7);
    let r = &m.records[0];
    assert_eq!(r.tag, TAG_CBST);
    let w = words(r);
    assert_eq!(w[0], 1000);
    assert_eq!((w[1], w[2], f32::from_bits(w[3] as u32)), (5, 6, 0.5));
    assert_eq!(&w[4..8], &[1, 2, 3, 4]);
    assert_eq!((w[8], w[9], w[10]), (1, 9, 1));
    assert_eq!(&w[11..14], &[70, EE_COMBAT_SEARCH_TRACE_BEST, 33]);
    assert_eq!(&w[14..], &[70, 33, 1]);
}

#[test]
fn no_record_and_no_trace_without_the_capture() {
    let mut m = Mock::default();
    search_along_list(&mut m, UNIT, 1, 2, 1.0, &mut |_i, _r, calls| {
        calls.trace(7, 0, 0);
        EeCombatSearchOut::default()
    });
    assert!(m.records.is_empty());
    assert!(!m.codes().contains(&EE_UNITS_SR_UNIT_ID));
}

#[test]
fn along_list_records_the_scaled_range() {
    let mut m = Mock { capture: true, ..Default::default() };
    m.floats.insert(EE_UNITS_SR_RANGE, 4.0);
    search_along_list(&mut m, UNIT, 1, 2, 0.5, &mut |_i, _r, _calls| EeCombatSearchOut::default());
    let w = words(&m.records[0]);
    assert_eq!(f32::from_bits(w[3] as u32), 2.0);
    assert_eq!(&w[4..], &[0, -1, 0]);
}

#[test]
fn pursuer_returns_its_complex_unit() {
    let mut m = Mock { capture: true, ..Default::default() };
    m.by_op.insert(EE_UNITS_SR_TILE_X, 3);
    m.set(EE_UNITS_SR_COMPLEX, 8, 88);
    let p = pursuer_of(&mut m, UNIT, &mut |u, x, y, calls| {
        assert_eq!((u, x, y), (UNIT, 3, 0));
        calls.trace(8, 4, 99);
        8
    });
    assert_eq!(p, 88);
    let w = words(&m.records[0]);
    assert_eq!(w, vec![1000, 3, 0, 1, 880, 4, 880]);
}

#[test]
fn find_new_target_retargets_the_choice() {
    let mut m = Mock { capture: true, ..Default::default() };
    m.set(EE_UNITS_SR_DISABLED, 2, 1);
    let found = find_new_target(&mut m, 40, 41, UNIT, (10, 20), 3, false, 1, &mut |i, _r, calls| {
        assert_eq!((i.left, i.top, i.right, i.bottom), (7, 17, 13, 23));
        assert!(calls.test(TARGET_TEST_DISABLED, 2));
        assert!(!calls.screen(5));
        (vec![1, 2], Some(1))
    });
    assert!(found);
    assert!(m.log.contains(&(EE_UNITS_SR_SCREEN, UNIT, 40, 5)));
    assert!(m.log.contains(&(EE_UNITS_SR_RETARGET, UNIT, 41, 2)));
    let w = words(&m.records[0]);
    assert_eq!(w, vec![1000, 10, 20, 3, 0, 1, 2, 10, 20, 1, 20]);
    let mut m = Mock::default();
    assert!(!find_new_target(&mut m, 40, 41, UNIT, (0, 0), 1, true, 0, &mut |_i, _r, _c| (vec![], None)));
    assert!(!m.codes().contains(&EE_UNITS_SR_RETARGET));
}

#[test]
fn new_target_ignored_passes_the_argument() {
    let mut m = Mock::default();
    m.by_op.insert(EE_UNITS_SR_IGNORED_COUNT, 1);
    let mut t = NewTarget { h: &mut m, screen: 0, unit: UNIT, ignore_argument: 5 };
    t.test(TARGET_TEST_IGNORED, 9);
    assert_eq!(m.log[1], (EE_UNITS_SR_IS_IGNORED, UNIT, 9, 1));
}
