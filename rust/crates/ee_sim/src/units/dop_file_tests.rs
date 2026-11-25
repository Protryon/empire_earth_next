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

/// A file of bytes: reads from the front (0x12 on a short read, the value kept), writes appended.
#[derive(Default)]
struct Bytes {
    data: Vec<u8>,
    out: Vec<u8>,
}

impl Bytes {
    fn take<const N: usize>(&mut self) -> Option<[u8; N]> {
        if self.data.len() < N {
            return None;
        }
        let bytes: [u8; N] = self.data[..N].try_into().ok()?;
        self.data.drain(..N);
        Some(bytes)
    }
}

impl DopFile for Bytes {
    fn read_i32(&mut self, v: &mut i32) -> i32 {
        self.take::<4>().map_or(0x12, |b| {
            *v = i32::from_le_bytes(b);
            0
        })
    }
    fn read_u32(&mut self, v: &mut u32) -> i32 {
        self.take::<4>().map_or(0x12, |b| {
            *v = u32::from_le_bytes(b);
            0
        })
    }
    fn read_f32(&mut self, v: &mut f32) -> i32 {
        self.take::<4>().map_or(0x12, |b| {
            *v = f32::from_le_bytes(b);
            0
        })
    }
    fn read_u8(&mut self, v: &mut u8) -> i32 {
        self.take::<1>().map_or(0x12, |b| {
            *v = b[0];
            0
        })
    }
    fn write_i32(&mut self, v: i32) -> i32 {
        self.out.extend_from_slice(&v.to_le_bytes());
        0
    }
    fn write_u32(&mut self, v: u32) -> i32 {
        self.out.extend_from_slice(&v.to_le_bytes());
        0
    }
    fn write_f32(&mut self, v: f32) -> i32 {
        self.out.extend_from_slice(&v.to_le_bytes());
        0
    }
    fn write_u8(&mut self, v: u8) -> i32 {
        self.out.push(v);
        0
    }
}

const FILE_ORDER: [i64; 7] = [0, 1, 2, 3, 4, 5, EE_UNITS_DOP_FIELD_FOUNDATION];

fn sample() -> EeUnitsComplexDopFields {
    EeUnitsComplexDopFields {
        mModel: 0x40 as *mut _,
        mAnimation: 3,
        mScale: 1.5,
        mPackedHitPoints: -0x7fff_ff9c, // the built flag and 100 hit points
        mTileX: -4,
        mTileY: 90,
        mFrame: 7,
        mIsFoundation: true,
    }
}

#[test]
fn the_fields_written_are_the_fields_read_back() {
    let r = sample();
    let mut f = Bytes::default();
    for field in FILE_ORDER {
        assert_eq!(write_field(&mut f, &r, field), 0);
    }
    // a long and an unsigned long are four bytes each, the float four, the flag one
    assert_eq!(f.out.len(), 6 * 4 + 1);
    assert_eq!(&f.out[..4], &(-0x7fff_ff9ci32).to_le_bytes());
    assert_eq!(f.out[24], 1);
    let mut back = EeUnitsComplexDopFields { mModel: r.mModel, ..Default::default() };
    f.data = std::mem::take(&mut f.out);
    for field in FILE_ORDER {
        assert_eq!(read_field(&mut f, &mut back, field), 0);
    }
    assert_eq!(back, r);
}

#[test]
fn a_short_read_keeps_the_member_and_passes_the_status_on() {
    let mut r = sample();
    let mut f = Bytes::default();
    assert_eq!(read_field(&mut f, &mut r, EE_UNITS_DOP_FIELD_TILE_X), 0x12);
    assert_eq!(read_field(&mut f, &mut r, EE_UNITS_DOP_FIELD_FOUNDATION), 0x12);
    assert_eq!(r, sample());
}

#[test]
fn a_copy_takes_each_named_member() {
    let other = sample();
    let mut r = EeUnitsComplexDopFields::default();
    copy_field(&mut r, &other, EE_UNITS_DOP_FIELD_FRAME);
    assert_eq!((r.mFrame, r.mAnimation), (7, 0));
    for field in [EE_UNITS_DOP_FIELD_MODEL, 2, 0, 3, 4, 5, EE_UNITS_DOP_FIELD_FOUNDATION] {
        copy_field(&mut r, &other, field);
    }
    assert_eq!(r, other);
}
