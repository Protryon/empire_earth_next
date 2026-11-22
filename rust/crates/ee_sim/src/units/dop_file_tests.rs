use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c); answers from `answers` keyed by (op, b), a read failing at
/// the `fail_at`-th file operation.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64, i64)>,
    answers: BTreeMap<(i32, i64), i64>,
    fail_at: Option<usize>,
    file_ops: usize,
}

impl DopHost for Mock {
    fn dop(&mut self, op: i32, _unit: u64, a: u64, b: i64, c: i64) -> i64 {
        self.log.push((op, a, b, c));
        let file = matches!(
            op,
            EE_UNITS_DOP_BASE_LOAD
                | EE_UNITS_DOP_READ_MODEL_ID
                | EE_UNITS_DOP_READ_FIELD
                | EE_UNITS_DOP_BASE_SAVE
                | EE_UNITS_DOP_WRITE_MODEL_ID
                | EE_UNITS_DOP_WRITE_FIELD
        );
        if file {
            self.file_ops += 1;
            if self.fail_at == Some(self.file_ops) {
                return 5;
            }
            return 0;
        }
        *self.answers.get(&(op, b)).unwrap_or(&0)
    }
}

impl Mock {
    fn fields(&self, op: i32) -> Vec<i64> {
        self.log.iter().filter(|e| e.0 == op).map(|e| e.2).collect()
    }
}

#[test]
fn a_load_reads_the_fields_in_the_files_order() {
    let mut h = Mock::default();
    assert_eq!(load(&mut h, 1, 9, 0, 0xe6), 0);
    assert_eq!(h.fields(EE_UNITS_DOP_READ_FIELD), [0, 1, 2, 3, 4, 5, EE_UNITS_DOP_FIELD_FOUNDATION]);
    assert!(h.log.iter().all(|e| e.1 == 9));
    // before 0xe6: no foundation flag
    let mut h = Mock::default();
    assert_eq!(load(&mut h, 1, 9, 0, 0xe5), 0);
    assert_eq!(h.fields(EE_UNITS_DOP_READ_FIELD), [0, 1, 2, 3, 4, 5]);
    // the model's id is set even when its read fails, then the error returns
    let mut h = Mock { fail_at: Some(2), ..Default::default() };
    assert_eq!(load(&mut h, 1, 9, 0, 0xe6), 5);
    assert_eq!(h.log.last().unwrap().0, EE_UNITS_DOP_SET_MODEL_FROM_ID);
    // a failed frame read stops there
    let mut h = Mock { fail_at: Some(4), ..Default::default() };
    assert_eq!(load(&mut h, 1, 9, 0, 0xe6), 5);
    assert_eq!(h.fields(EE_UNITS_DOP_READ_FIELD), [0, 1]);
}

#[test]
fn a_save_writes_the_same_fields() {
    let mut h = Mock::default();
    assert_eq!(save(&mut h, 1, 9, 0, 0x118), 0);
    assert_eq!(h.fields(EE_UNITS_DOP_WRITE_FIELD), [0, 1, 2, 3, 4, 5, EE_UNITS_DOP_FIELD_FOUNDATION]);
    let mut h = Mock { fail_at: Some(1), ..Default::default() };
    assert_eq!(save(&mut h, 1, 9, 0, 0x118), 5);
    assert_eq!(h.log.len(), 1);
}

#[test]
fn a_complex_units_view_takes_its_model_and_frame() {
    let mut h = Mock::default();
    h.answers.insert((EE_UNITS_DOP_SEEN_COMPLEX, 0), 70);
    h.answers.insert((EE_UNITS_DOP_SEEN_IS_BUILT, 0), 1);
    initialize(&mut h, 1, 60, 2, 3);
    let ops: Vec<i32> = h.log.iter().map(|e| e.0).collect();
    assert_eq!(ops[0], EE_UNITS_DOP_SEEN_COMPLEX);
    assert_eq!(ops[1], EE_UNITS_DOP_BASE_INITIALIZE);
    assert!(ops.contains(&EE_UNITS_DOP_MARK_BUILT));
    assert!(ops.contains(&EE_UNITS_DOP_FRAME_OF));
    assert!(!ops.contains(&EE_UNITS_DOP_IDENTITY));
    assert_eq!(*ops.last().unwrap(), EE_UNITS_DOP_LOAD_MEDIA);
}

#[test]
fn a_simple_units_view_stands_built_at_its_tile_centre() {
    let mut h = Mock::default();
    h.answers.insert((EE_UNITS_DOP_HALF_SIZE, 0), bits(1.0));
    h.answers.insert((EE_UNITS_DOP_SEEN_TILE, 0), 4);
    h.answers.insert((EE_UNITS_DOP_SEEN_TILE, 1), 6);
    h.answers.insert((EE_UNITS_DOP_MESH_Z, bits(5.0)), bits(0.25));
    initialize(&mut h, 1, 60, 2, 3);
    let set = h.log.iter().find(|e| e.0 == EE_UNITS_DOP_SET_POSITION).unwrap();
    assert_eq!((set.1, set.2, set.3), (bits(0.25) as u64, bits(5.0), bits(7.0)));
    assert_eq!(h.log.iter().filter(|e| e.0 == EE_UNITS_DOP_IDENTITY).count(), 2);
}

#[test]
fn the_contacts_are_cleared_then_added_last_first() {
    let mut h = Mock::default();
    h.answers.insert((EE_UNITS_DOP_CONTACTS_COUNT, 0), 2);
    h.answers.insert((EE_UNITS_DOP_CONTACTS_VALID, 0), 1);
    h.answers.insert((EE_UNITS_DOP_CONTACTS_ID_COUNT, 0), 2);
    h.answers.insert((EE_UNITS_DOP_CONTACTS_UNIT, 0), 90);
    restore_loaded_contacts(&mut h);
    let ops: Vec<i32> = h.log.iter().map(|e| e.0).collect();
    let clear = ops.iter().position(|&o| o == EE_UNITS_DOP_CONTACTS_CLEAR).unwrap();
    let add = ops.iter().position(|&o| o == EE_UNITS_DOP_CONTACTS_ADD).unwrap();
    assert!(clear < add);
    assert_eq!(ops.iter().filter(|&&o| o == EE_UNITS_DOP_CONTACTS_CLEAR).count(), 1);
    let units: Vec<i64> = h.log.iter().filter(|e| e.0 == EE_UNITS_DOP_CONTACTS_UNIT).map(|e| e.3).collect();
    assert_eq!(units, [1, 0]);
}
