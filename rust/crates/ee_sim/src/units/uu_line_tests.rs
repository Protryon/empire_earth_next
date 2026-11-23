use super::*;
use std::collections::HashMap;

/// A recording mock: each statement (op, a, b, c); answers by (op, a), else by op.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, u64, i64)>,
    by_object: HashMap<(i32, u64), i64>,
    by_op: HashMap<i32, i64>,
    floats: HashMap<i32, f32>,
    walls: Vec<(i32, i32)>,
    above: bool,
    capture: bool,
    records: Vec<Record>,
}

impl LfHost for Mock {
    fn lf(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, f: &mut [f32; 4]) -> i64 {
        self.log.push((op, a, b, c));
        if let Some(v) = self.floats.get(&op) {
            f[0] = *v;
        }
        match op {
            EE_UNITS_LF_LOCATION => {
                // SAFETY: the body's 16 floats
                let m = unsafe { &mut *(c as usize as *mut [f32; 16]) };
                m[11] = 2.0;
            }
            EE_UNITS_LF_POINT_LOCAL | EE_UNITS_LF_BOX_TOP => {
                // SAFETY: the body's point
                unsafe { *(d as usize as *mut [f32; 3]) = [0.5, 0.25, 1.5] };
            }
            EE_UNITS_LF_INTERNAL_TRANSFORM => {
                // SAFETY: the body's 16 floats
                let m = unsafe { &mut *(d as usize as *mut [f32; 16]) };
                *m = [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0];
            }
            EE_UNITS_LF_UNIT_ID => return a as i64 * 10,
            _ => {}
        }
        self.by_object.get(&(op, a)).or_else(|| self.by_op.get(&op)).copied().unwrap_or(0)
    }
    fn tile_top_above(&mut self, x: i32, y: i32, _height: f32, _bare: bool) -> (bool, u64) {
        self.walls.push((x, y));
        (self.above, 77)
    }
    fn capture(&mut self) -> bool {
        self.capture
    }
    fn commit(&mut self, record: &Record) {
        self.records.push(record.clone());
    }
}

fn words(r: &Record) -> Vec<i32> {
    r.data.chunks(4).map(|c| i32::from_le_bytes([c[0], c[1], c[2], c[3]])).collect()
}

#[test]
fn tags_are_first_letter_low() {
    assert_eq!(TAG_CBLB, 0x424c_4243);
    assert_eq!(TAG_CBWP & 0xff, u32::from(b'C'));
    assert_eq!(TAG_CBTL >> 24, u32::from(b'L'));
}

#[test]
fn tile_line_records_its_input_and_tiles() {
    let mut m = Mock { capture: true, ..Default::default() };
    let tiles = tile_line_recorded(&mut m, 0, 0, 3, 0, true, false);
    assert_eq!(tiles, tile_line(0, 0, 3, 0, true, false));
    let w = words(&m.records[0]);
    assert_eq!(&w[..7], &[0, 0, 3, 0, 1, 0, tiles.len() as i32]);
    assert_eq!(w.len(), 7 + 2 * tiles.len());
    let mut m = Mock::default();
    tile_line_recorded(&mut m, 0, 0, 3, 0, true, false);
    assert!(m.records.is_empty());
}

#[test]
fn a_near_or_skipping_shot_asks_nothing_of_the_walls() {
    let mut m = Mock { capture: true, ..Default::default() };
    m.by_op.insert(EE_UNITS_LF_HIT_POINTS, 5);
    let (blocked, blocker, _, _) = line_to_unit_blocked(&mut m, 1, 2);
    assert!(!blocked);
    assert_eq!(blocker, None);
    assert!(m.walls.is_empty());
    let w = words(&m.records[0]);
    assert_eq!(&w[..2], &[10, 20]);
    assert_eq!(&w[w.len() - 2..], &[0, -1]);
}

#[test]
fn a_blocking_wall_is_the_blocker() {
    let mut m = Mock { capture: true, above: true, ..Default::default() };
    m.by_object.insert((EE_UNITS_LF_TILE_X, 2), 6);
    m.by_op.insert(EE_UNITS_LF_HIT_POINTS, 5);
    m.by_op.insert(EE_UNITS_LF_MELEE, 1);
    let (blocked, blocker, _, _) = line_to_unit_blocked(&mut m, 1, 2);
    assert!(blocked);
    assert_eq!(blocker, Some(77));
    assert!(!m.walls.is_empty());
    // the melee shot is bare: no weapon point, no target top
    assert!(!m.log.iter().any(|l| l.0 == EE_UNITS_LF_MODEL));
    let w = words(m.records.iter().find(|r| r.tag == TAG_CBLB).unwrap());
    assert_eq!(&w[w.len() - 2..], &[1, 770]);
    // the bare start height's CBWP and the tile line's CBTL come first
    assert_eq!(m.records.iter().map(|r| r.tag).collect::<Vec<_>>(), vec![TAG_CBWP, TAG_CBTL, TAG_CBLB]);
}

#[test]
fn the_weapon_point_walks_the_model_chain() {
    let mut m = Mock::default();
    m.by_op.insert(EE_UNITS_LF_COMPLEX, 1);
    m.by_op.insert(EE_UNITS_LF_MODEL, 30);
    m.by_op.insert(EE_UNITS_LF_TAG_LIST, 40);
    m.by_op.insert(EE_UNITS_LF_WEAPON_INDEX, 0);
    m.by_op.insert(EE_UNITS_LF_LIST_SIZE, 1);
    m.by_op.insert(EE_UNITS_LF_LIST_TAG_POINT, 50);
    m.by_object.insert((EE_UNITS_LF_POINT_MODEL, 50), 60);
    m.by_object.insert((EE_UNITS_LF_PARENT, 60), 61);
    let h = start_height(&mut m, 1, false);
    let codes: Vec<i32> = m.log.iter().map(|l| l.0).collect();
    assert_eq!(codes.iter().filter(|&&c| c == EE_UNITS_LF_INTERNAL_TRANSFORM).count(), 2);
    assert!(!codes.contains(&EE_UNITS_LF_BOX_TOP));
    assert!(h.is_finite());
    // no list: the box
    let mut m = Mock::default();
    m.by_op.insert(EE_UNITS_LF_MODEL, 30);
    start_height(&mut m, 1, false);
    assert!(m.log.iter().any(|l| l.0 == EE_UNITS_LF_BOX_TOP));
}

#[test]
fn a_turret_reads_its_angle_when_found() {
    let mut m = Mock::default();
    m.by_op.insert(EE_UNITS_LF_NUM_PARTS, 2);
    m.by_op.insert(EE_UNITS_LF_FIND_TURRET, 0);
    m.floats.insert(EE_UNITS_LF_TURRET_ANGLE, 0.5);
    start_height(&mut m, 1, false);
    let codes: Vec<i32> = m.log.iter().map(|l| l.0).collect();
    assert!(codes.contains(&EE_UNITS_LF_TURRET_ANGLE));
    assert!(codes.contains(&EE_UNITS_LF_LIST_POINT_AT));
    let mut m = Mock::default();
    m.by_op.insert(EE_UNITS_LF_NUM_PARTS, 2);
    m.by_op.insert(EE_UNITS_LF_FIND_TURRET, 1);
    start_height(&mut m, 1, false);
    assert!(!m.log.iter().any(|l| l.0 == EE_UNITS_LF_TURRET_ANGLE));
}

#[test]
fn target_top_needs_a_complex_unit() {
    let mut m = Mock::default();
    assert_eq!(target_top(&mut m, 2).present, 0);
    m.by_op.insert(EE_UNITS_LF_COMPLEX, 3);
    m.by_op.insert(EE_UNITS_LF_HIT_POINTS, 7);
    m.floats.insert(EE_UNITS_LF_Z, 4.0);
    let t = target_top(&mut m, 2);
    assert_eq!((t.present, t.box_top, t.hit_points, t.z), (1, 1.5, 7, 4.0));
}
