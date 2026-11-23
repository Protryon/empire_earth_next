use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c), its answer from `answers` keyed by op (0 else).
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64, i64)>,
    answers: BTreeMap<i32, i64>,
}

impl Mock {
    fn answer(mut self, op: i32, v: i64) -> Self {
        self.answers.insert(op, v);
        self
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
}

impl DopHost for Mock {
    fn dop(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64) -> i64 {
        assert_eq!(unit, 1);
        self.log.push((op, a, b, c));
        *self.answers.get(&op).unwrap_or(&0)
    }
}

#[test]
fn reset_and_clear() {
    let mut h = Mock::default();
    reset(&mut h, 1);
    assert_eq!(h.ops(), (EE_UNITS_DOPX_ZERO_SCALE..=EE_UNITS_DOPX_NOT_FOUNDATION).collect::<Vec<_>>());
    let mut h = Mock::default().answer(EE_UNITS_DOPX_HAS_MODEL, 1);
    clear(&mut h, 1);
    assert_eq!(&h.ops()[..3], &[EE_UNITS_DOPX_BASE_CLEAR, EE_UNITS_DOPX_HAS_MODEL, EE_UNITS_DOPX_UNLOAD_MODEL]);
    assert_eq!(h.ops().len(), 11);
}

#[test]
fn repair_pointers_puts_the_doppelganger_on_its_tile_or_frees_it() {
    let mut h = Mock::default().answer(EE_UNITS_DOPX_MODEL_SAVED_ID, 33).answer(EE_UNITS_DOPX_MEDIA, 40);
    assert_eq!(repair_pointers(&mut h, 1, 2, 3), 0);
    assert_eq!(h.log[2], (EE_UNITS_DOPX_BASE_REPAIR, 0, 2, 3));
    assert_eq!(h.log[3], (EE_UNITS_DOPX_MEDIA, 0, 33, 0));
    assert_eq!(
        &h.ops()[4..],
        &[EE_UNITS_DOPX_LOAD_MEDIA, EE_UNITS_DOPX_SET_MODEL, EE_UNITS_DOPX_CLAMP_FRAME, EE_UNITS_DOPX_ADD_TO_TILES]
    );
    let mut h = Mock::default();
    repair_pointers(&mut h, 1, 2, 3);
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_DOPX_FREE);
    let mut h = Mock::default().answer(EE_UNITS_DOPX_BASE_REPAIR, 5);
    repair_pointers(&mut h, 1, 2, 3);
    assert_eq!(
        h.ops(),
        vec![EE_UNITS_DOPX_MODEL_SAVED_ID, EE_UNITS_DOPX_CLEAR_MODEL, EE_UNITS_DOPX_BASE_REPAIR, EE_UNITS_DOPX_FREE]
    );
}

#[test]
fn finish_load_marks_an_old_saves_foundation() {
    let mut h = Mock::default().answer(EE_UNITS_DOPX_OLD_FOUNDATION, 1).answer(EE_UNITS_DOPX_BASE_FINISH_LOAD, 7);
    assert_eq!(finish_load(&mut h, 1, 0, 200), 7);
    assert_eq!(
        h.ops(),
        vec![
            EE_UNITS_DOPX_FINISH_FRACTIONS,
            EE_UNITS_DOPX_OLD_FOUNDATION,
            EE_UNITS_DOPX_UNLOAD_CLASS_MEDIA,
            EE_UNITS_DOPX_POST_LOAD_CLASS,
            EE_UNITS_DOPX_BASE_FINISH_LOAD
        ]
    );
    let mut h = Mock::default().answer(EE_UNITS_DOPX_OLD_FOUNDATION, 1);
    finish_load(&mut h, 1, 0, 300);
    assert_eq!(h.ops(), vec![EE_UNITS_DOPX_FINISH_FRACTIONS, EE_UNITS_DOPX_BASE_FINISH_LOAD]);
    assert_eq!(h.log[1], (EE_UNITS_DOPX_BASE_FINISH_LOAD, 0, 0, 300));
}
