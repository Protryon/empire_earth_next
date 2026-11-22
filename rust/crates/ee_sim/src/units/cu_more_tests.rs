use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c, f in); its answer from `answers` keyed by (op, a) and its
/// floats out from `floats` keyed by op.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64, i64, [f32; 4])>,
    answers: BTreeMap<(i32, u64), i64>,
    floats: BTreeMap<i32, [f32; 4]>,
    active: bool,
    records: Vec<Record>,
}

impl Mock {
    fn answer(mut self, op: i32, a: u64, v: i64) -> Self {
        self.answers.insert((op, a), v);
        self
    }
    fn float(mut self, op: i32, f: [f32; 4]) -> Self {
        self.floats.insert(op, f);
        self
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
    fn asked(&self, op: i32) -> (i32, u64, i64, i64, [f32; 4]) {
        *self.log.iter().find(|e| e.0 == op).unwrap()
    }
}

impl CuHost for Mock {
    fn cu(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
        assert_eq!(unit, 1);
        self.log.push((op, a, b, c, *f));
        if let Some(v) = self.floats.get(&op) {
            *f = *v;
        }
        *self.answers.get(&(op, a)).unwrap_or(&0)
    }
}

impl CuMoreHost for Mock {
    fn capture_active(&mut self) -> bool {
        self.active
    }
    fn commit(&mut self, r: &Record) {
        self.records.push(r.clone());
    }
}

#[test]
fn the_tqrs_tag_is_the_capture_tags_order() {
    assert_eq!(TQRS, 0x5352_5154);
}

#[test]
fn queue_research_records_tqrs_around_the_body() {
    let mut h = Mock::default();
    queue_research(&mut h, 1, 7, true);
    assert_eq!(h.ops(), vec![EE_UNITS_CUM_QUEUE_UNCAPTURED]);
    assert!(h.records.is_empty());
    let mut h = Mock { active: true, ..Default::default() }
        .answer(EE_UNITS_CUM_PLAYER_ID, 0, 2)
        .answer(EE_UNITS_CUM_UNIT_ID, 0, 30)
        .answer(EE_UNITS_CUM_UNIT_TYPE, 0, 40)
        .answer(EE_UNITS_CUM_BUILD_QUEUE_SIZE, 0, 1);
    queue_research(&mut h, 1, 7, false);
    let words: Vec<u32> = [2u32, 30, 40, 7, 0, 1, 1].to_vec();
    let bytes: Vec<u8> = words.iter().flat_map(|w| w.to_le_bytes()).collect();
    assert_eq!(h.records[0].data, bytes);
    let at = h.ops().iter().position(|o| *o == EE_UNITS_CUM_QUEUE_UNCAPTURED).unwrap();
    assert_eq!(h.ops()[at - 1], EE_UNITS_CUM_BUILD_QUEUE_SIZE);
    assert_eq!(h.ops()[at + 1], EE_UNITS_CUM_BUILD_QUEUE_SIZE);
}

#[test]
fn turrets_offsets_and_heights() {
    let mut h = Mock::default();
    resize_turret_angles(&mut h, 1, 2);
    assert_eq!(
        h.ops(),
        vec![EE_UNITS_CUM_TURRETS_DELETE, EE_UNITS_CUM_TURRETS_NEW, EE_UNITS_CUM_TURRET_ZERO, EE_UNITS_CUM_TURRET_ZERO]
    );
    let mut h = Mock::default();
    resize_turret_angles(&mut h, 1, 0);
    assert_eq!(h.ops(), vec![EE_UNITS_CUM_TURRETS_DELETE, EE_UNITS_CUM_TURRETS_NULL]);
    let mut h = Mock::default().answer(EE_UNITS_CUM_TILE_SIZE, 0, 3);
    assert_eq!(add_center_offset(&mut h, 1, 2.0, 5.0), (3.5, 6.5));
    let mut h = Mock::default();
    assert_eq!(add_center_offset(&mut h, 1, 2.0, 5.0), (2.5, 5.5));
    // half built: half the scaled top
    let mut h = Mock::default()
        .float(EE_UNITS_CUM_POINT_OUT, [0.0, 0.0, 4.0, 0.0])
        .float(EE_UNITS_CUM_STATS_SCALE, [1.5, 0.0, 0.0, 0.0])
        .answer(EE_UNITS_CUM_HIT_POINTS, 0, 5)
        .answer(EE_UNITS_CUM_MAX_HIT_POINTS, 0, 10);
    assert_eq!(effect_height(&mut h, 1, true), 3.0);
    assert_eq!(h.asked(EE_UNITS_CUM_EFFECT_TOP).2, 1);
}

#[test]
fn facing_a_point_and_a_square() {
    let mut h = Mock::default().float(EE_UNITS_CUM_LOCATION_OUT, [1.0, 1.0, 0.0, 0.0]);
    face_toward(&mut h, 1, [1.0, 3.0, 0.0]);
    let ypr = h.asked(EE_UNITS_CUM_SET_YPR).4;
    assert_eq!(ypr[0], um_arctan2(-0.0, 2.0));
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_CUM_APPLY_TRANSFORMATION);
    // a 3x3 square at (4, 4): its centre tile (5, 5), facing from (5.5, 1.5)
    let mut h = Mock::default().float(EE_UNITS_CUM_LOCATION_OUT, [5.5, 1.5, 0.0, 0.0]);
    face_toward_square(&mut h, 1, 4, 4, 3);
    let ypr = h.asked(EE_UNITS_CUM_SET_YPR).4;
    assert_eq!(ypr[0], um_arctan2(-0.0, 4.0));
}

#[test]
fn facing_a_target() {
    // a large target: its square
    let mut h = Mock::default().answer(EE_UNITS_CUM_TILE_SIZE_OF, 9, 2).answer(EE_UNITS_CUM_TILE_X_OF, 9, 3).answer(
        EE_UNITS_CUM_TILE_Y_OF,
        9,
        4,
    );
    face_target(&mut h, 1, 9);
    assert!(h.ops().contains(&EE_UNITS_CUM_SET_YPR));
    assert!(!h.ops().contains(&EE_UNITS_CUM_HAS_MODEL));
    // a modelled target: its top point scaled in x and y
    let mut h = Mock::default()
        .answer(EE_UNITS_CUM_HAS_MODEL, 9, 1)
        .float(EE_UNITS_CUM_TOP_POINT_OF, [1.0, 2.0, 3.0, 0.0])
        .float(EE_UNITS_CUM_SCALE_OF, [2.0, 0.0, 0.0, 0.0]);
    face_target(&mut h, 1, 9);
    assert_eq!(h.asked(EE_UNITS_CUM_APPLY_OF).4, [2.0, 4.0, 3.0, 0.0]);
    assert!(!h.ops().contains(&EE_UNITS_CUM_POSITION_OF));
}

#[test]
fn placing_on_the_ground() {
    let mut h = Mock::default()
        .float(EE_UNITS_CUM_VECTOR_OUT, [0.0, 1.0, 0.0, 0.0])
        .answer(EE_UNITS_CUM_ROLLS, 0, 1)
        .float(EE_UNITS_CUM_TERRAIN_ROLL, [0.25, 0.0, 0.0, 0.0]);
    place_on_ground(&mut h, 1, [1.0, 2.0, 3.0]);
    assert_eq!(h.asked(EE_UNITS_CUM_GROUND_POINT).4, [1.0, 2.0, 3.0, 0.0]);
    assert_eq!(h.asked(EE_UNITS_CUM_SET_YPR).4, [um_arctan2(-0.0, 1.0), 0.0, 0.25, 0.0]);
    assert!(!h.ops().contains(&EE_UNITS_CUM_TERRAIN_PITCH));
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_CUM_SET_LOCATION);
}
