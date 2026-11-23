use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c), its answer from `answers` keyed by (op, a) (0 else).
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64, i64)>,
    answers: BTreeMap<(i32, u64), i64>,
}

impl Mock {
    fn answer(mut self, op: i32, a: u64, v: i64) -> Self {
        self.answers.insert((op, a), v);
        self
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
}

impl EumHost for Mock {
    fn eum(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64) -> i64 {
        assert_eq!(unit, 1);
        self.log.push((op, a, b, c));
        *self.answers.get(&(op, a)).unwrap_or(&0)
    }
}

#[test]
fn remove_unloads_in_the_background_unless_shutting_down() {
    let mut h = Mock::default();
    remove(&mut h, 1, true);
    assert_eq!(
        h.ops(),
        vec![
            EE_UNITS_EUM_UNIT_REMOVED,
            EE_UNITS_EUM_SHUTDOWN,
            EE_UNITS_EUM_CLEARING,
            EE_UNITS_EUM_POST_UNLOAD,
            EE_UNITS_EUM_ZERO_HIT_POINTS,
            EE_UNITS_EUM_HAS_COMPLEX,
            EE_UNITS_EUM_RTS_INVALIDATE,
        ]
    );
    let mut h = Mock::default().answer(EE_UNITS_EUM_SHUTDOWN, 0, 1).answer(EE_UNITS_EUM_HAS_COMPLEX, 0, 1);
    remove(&mut h, 1, true);
    assert!(h.ops().contains(&EE_UNITS_EUM_UNLOAD_MEDIA));
    assert!(!h.ops().contains(&EE_UNITS_EUM_CLEARING));
    assert!(h.ops().contains(&EE_UNITS_EUM_DEACTIVATE));
    let mut h = Mock::default();
    remove(&mut h, 1, false);
    assert!(!h.ops().contains(&EE_UNITS_EUM_SHUTDOWN));
}

#[test]
fn copy_from_scales_the_health_fraction() {
    // 7 of 10 hit points onto a maximum of 15: 10.5 rounds to even, 10
    let mut h = Mock::default()
        .answer(EE_UNITS_EUM_AS_UNIT, 9, 8)
        .answer(EE_UNITS_EUM_UPGRADE_MAX_HIT_POINTS, 8, 10)
        .answer(EE_UNITS_EUM_UPGRADE_MAX_HIT_POINTS, 1, 15)
        .answer(EE_UNITS_EUM_HIT_POINTS_OF, 8, 7);
    copy_from(&mut h, 1, 9);
    let set = h.log.iter().find(|e| e.0 == EE_UNITS_EUM_SET_HIT_POINTS_FIELD).unwrap();
    assert_eq!(set.2, 10);
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_EUM_RTS_COPY_FROM);
}

#[test]
fn the_inits() {
    let mut h = Mock::default();
    init_at(&mut h, 1, 5, -2, 3, 6);
    assert_eq!(h.log[0], (EE_UNITS_EUM_INITIALIZE, 0, 0, 0));
    assert_eq!(h.ops()[1..8].len(), 7);
    assert!(h.ops().contains(&EE_UNITS_EUM_REFERENCE_ONE));
    assert!(h.ops().contains(&EE_UNITS_EUM_SET_STATS));
    assert_eq!(*h.log.last().unwrap(), (EE_UNITS_EUM_MOVE_TO_TILE, 0, -2, 3));
    let mut h = Mock::default().answer(EE_UNITS_EUM_REFERENCE_COUNT, 0, 2);
    init_complex(&mut h, 1, 0, 6);
    assert_eq!(h.log[0], (EE_UNITS_EUM_INITIALIZE, 0, 1, 0));
    assert!(!h.ops().contains(&EE_UNITS_EUM_REFERENCE_ONE));
    assert!(!h.ops().contains(&EE_UNITS_EUM_SET_STATS));
    assert!(h.ops().contains(&EE_UNITS_EUM_SET_COMPLEX_SELF));
    // the packed tile of the FFI
    let mut h = Mock::default();
    run(&mut h, EE_UNITS_EUM_DO_INIT_AT, 1, 5, i64::from(-2i32 as u32) | 3 << 32, 6);
    assert_eq!(*h.log.last().unwrap(), (EE_UNITS_EUM_MOVE_TO_TILE, 0, -2, 3));
}

#[test]
fn regrowing_a_forest() {
    // a citizen's cut: radius and count drawn, then the renewal at time + draw
    let mut h = Mock::default()
        .answer(EE_UNITS_EUM_FOREST, 0, 40)
        .answer(EE_UNITS_EUM_FOREST_DUE, 40, 1)
        .answer(EE_UNITS_EUM_FOREST_TILES, 40, 2)
        .answer(EE_UNITS_EUM_CATEGORY_OF, 7, 6)
        .answer(EE_UNITS_EUM_GAME_TIME, 0, 1000);
    regrow_forest(&mut h, 1, 7);
    let randoms: Vec<(i64, i64)> = h.log.iter().filter(|e| e.0 == EE_UNITS_EUM_RANDOM).map(|e| (e.2, e.3)).collect();
    assert_eq!(randoms, vec![(0, 2), (1, 2), (125_000, 275_000)]);
    assert_eq!(h.log.iter().find(|e| e.0 == EE_UNITS_EUM_SET_NEXT_RENEWAL).unwrap().2, 1000);
    // not due
    let mut h = Mock::default().answer(EE_UNITS_EUM_FOREST, 0, 40);
    regrow_forest(&mut h, 1, 0);
    assert!(!h.ops().contains(&EE_UNITS_EUM_REGROW));
    // carbon rock
    let mut h = Mock::default().answer(EE_UNITS_EUM_CARBON_ROCK, 0, 1);
    regrow_forest(&mut h, 1, 0);
    assert_eq!(h.ops(), vec![EE_UNITS_EUM_CARBON_ROCK]);
}

#[test]
fn the_last_attacker_moves_its_reference() {
    let mut h = Mock::default().answer(EE_UNITS_EUM_LAST_ATTACKER, 0, 4);
    set_last_attacker(&mut h, 1, 5);
    assert_eq!(
        h.ops(),
        vec![
            EE_UNITS_EUM_LAST_ATTACKER,
            EE_UNITS_EUM_DEREFERENCE_LAST_ATTACKER,
            EE_UNITS_EUM_SET_LAST_ATTACKER,
            EE_UNITS_EUM_REFERENCE_OF
        ]
    );
    let mut h = Mock::default().answer(EE_UNITS_EUM_LAST_ATTACKER, 0, 4);
    set_last_attacker(&mut h, 1, 4);
    assert_eq!(h.ops(), vec![EE_UNITS_EUM_LAST_ATTACKER]);
    let mut h = Mock::default();
    load_media(&mut h, 1, true);
    assert_eq!(h.ops(), vec![EE_UNITS_EUM_POST_LOAD]);
}

#[test]
fn clear_and_last_dereference_in_the_originals_order() {
    struct R(Vec<i32>);
    impl EumHost for R {
        fn eum(&mut self, op: i32, _unit: u64, _a: u64, _b: i64, _c: i64) -> i64 {
            self.0.push(op);
            0
        }
    }
    let mut r = R(vec![]);
    clear(&mut r, 1);
    last_dereference(&mut r, 1);
    assert_eq!(
        r.0,
        [
            EE_UNITS_EUM_RESET_EE_UNIT,
            EE_UNITS_EUM_RTS_RESET,
            EE_UNITS_EUM_SET_INVALID,
            EE_UNITS_EUM_FREE_NODE,
            EE_UNITS_EUM_ZERO_REFERENCES,
            EE_UNITS_EUM_DELETE
        ]
    );
}

#[test]
fn the_constructors_and_the_destructor_in_the_originals_order() {
    struct R(Vec<(i32, i64)>);
    impl EumHost for R {
        fn eum(&mut self, op: i32, _unit: u64, _a: u64, b: i64, _c: i64) -> i64 {
            self.0.push((op, b));
            0
        }
    }
    let mut r = R(vec![]);
    construct(&mut r, 1);
    assert_eq!(
        r.0,
        [
            (EE_UNITS_EUM_SET_OBJECT_KIND, 1),
            (EE_UNITS_EUM_RTS_RESET, 0),
            (EE_UNITS_EUM_RESET_EE_UNIT, 0),
            (EE_UNITS_EUM_ZERO_REFERENCES, 0)
        ]
    );
    let mut r = R(vec![]);
    construct_complex(&mut r, 1);
    assert_eq!(r.0[0], (EE_UNITS_EUM_SET_OBJECT_KIND, 2));
    assert_eq!(r.0[1].0, EE_UNITS_EUM_CX_ZERO_DISABLED_REASON);
    assert_eq!(r.0[4].0, EE_UNITS_EUM_CX_RESET_COMPLEX);
    let mut r = R(vec![]);
    destroy(&mut r, 1);
    assert_eq!(r.0, [(EE_UNITS_EUM_SET_OBJECT_KIND, 1), (EE_UNITS_EUM_FREE_NODE, 0)]);
}
