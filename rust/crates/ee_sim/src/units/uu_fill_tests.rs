use super::*;
use std::collections::HashMap;

/// A recording mock: each statement (op, a, b, c); answers by (op, a), else by op.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, u64, i64)>,
    by_object: HashMap<(i32, u64), i64>,
    by_op: HashMap<i32, i64>,
    floats: HashMap<i32, f32>,
    capture: bool,
    records: Vec<Record>,
}

impl LfHost for Mock {
    fn lf(&mut self, op: i32, a: u64, b: u64, c: i64, _d: i64, f: &mut [f32; 4]) -> i64 {
        self.log.push((op, a, b, c));
        if let Some(v) = self.floats.get(&op) {
            f[0] = *v;
        }
        self.by_object.get(&(op, a)).or_else(|| self.by_op.get(&op)).copied().unwrap_or(0)
    }
    fn tile_top_above(&mut self, _x: i32, _y: i32, _height: f32, _bare: bool) -> (bool, u64) {
        (false, 0)
    }
    fn capture(&mut self) -> bool {
        self.capture
    }
    fn commit(&mut self, record: &Record) {
        self.records.push(record.clone());
    }
}

impl Mock {
    fn codes(&self) -> Vec<i32> {
        self.log.iter().map(|l| l.0).collect()
    }
}

#[test]
fn tags_are_first_letter_low() {
    assert_eq!(TAG_CBBP, 0x5042_4243);
    assert_eq!(TAG_CBCH & 0xff, u32::from(b'C'));
    assert_eq!(TAG_CBAP >> 24, u32::from(b'P'));
}

#[test]
fn ballistic_high_arc_over_walls_needs_animation_8() {
    let mut m = Mock { capture: true, ..Default::default() };
    m.floats.insert(EE_UNITS_LF_LAUNCH_SPEED, 10.0);
    m.floats.insert(EE_UNITS_LF_PROJECTILE_ACCELERATION, 1.0);
    m.by_op.insert(EE_UNITS_LF_STATS_OVER_WALLS, 1);
    m.by_op.insert(EE_UNITS_LF_FIRING_HIGH, 1);
    m.by_op.insert(EE_UNITS_LF_ANIMATION, 8);
    let (reached, angle) = ballistic_pitch(&mut m, 1, 2, [0.0; 3], [3.0, 0.0, 0.0]);
    let i = EeCombatBallisticIn {
        from_x: 0.0,
        from_y: 0.0,
        from_z: 0.0,
        to_x: 3.0,
        to_y: 0.0,
        to_z: 0.0,
        speed: 10.0,
        acceleration: 1.0,
        high: 1,
    };
    let out = ballistic(&i);
    assert_eq!((reached, angle), (out.reached != 0, out.angle));
    assert_eq!(m.records[0].tag, TAG_CBBP);
    assert_eq!(m.records[0].data.len(), 9 * 4 + 8);
    // the class shoots high: the rest is not asked
    let mut m = Mock::default();
    m.by_op.insert(EE_UNITS_LF_SHOOTS_HIGH, 1);
    ballistic_pitch(&mut m, 1, 2, [0.0; 3], [3.0, 0.0, 0.0]);
    assert!(!m.codes().contains(&EE_UNITS_LF_STATS_OVER_WALLS));
}

#[test]
fn can_heal_asks_the_set_only_in_range() {
    let mut m = Mock::default();
    m.by_op.insert(EE_UNITS_LF_WONDER_SET, 5);
    m.by_op.insert(EE_UNITS_LF_SET_COUNT, 3);
    can_heal_target(&mut m, 1, 2);
    assert!(!m.codes().contains(&EE_UNITS_LF_SET_CONTAINS));
    let mut m = Mock { capture: true, ..Default::default() };
    m.by_op.insert(EE_UNITS_LF_WONDER_SET, 2);
    m.by_op.insert(EE_UNITS_LF_SET_COUNT, 3);
    m.by_op.insert(EE_UNITS_LF_SET_CONTAINS, 1);
    m.by_op.insert(EE_UNITS_LF_REPAIRABLE, 1);
    m.by_op.insert(EE_UNITS_LF_ALLIED, 1);
    m.by_object.insert((EE_UNITS_LF_HIT_POINTS, 2), 10);
    m.by_object.insert((EE_UNITS_LF_MAX_HIT_POINTS, 2), 100);
    m.floats.insert(EE_UNITS_LF_HEAL_THRESHOLD, 0.9);
    let healed = can_heal_target(&mut m, 1, 2);
    assert!(m.log.contains(&(EE_UNITS_LF_SET_CONTAINS, 2, 0, 2)));
    let w: Vec<i32> = m.records[0].data.chunks(4).map(|c| i32::from_le_bytes([c[0], c[1], c[2], c[3]])).collect();
    assert_eq!(w.len(), 10);
    assert_eq!(w[9], i32::from(healed));
}

#[test]
fn attach_point_walks_the_chain_at_sound_frame_0() {
    let mut m = Mock { capture: true, ..Default::default() };
    m.by_op.insert(EE_UNITS_LF_MODEL, 30);
    m.by_op.insert(EE_UNITS_LF_TAG_LIST, 40);
    m.by_op.insert(EE_UNITS_LF_TAG_POINT_COUNT, 2);
    m.by_op.insert(EE_UNITS_LF_WEAPON_INDEX, 1);
    m.by_op.insert(EE_UNITS_LF_LIST_TAG_POINT, 50);
    m.by_op.insert(EE_UNITS_LF_SOUND_FRAME0, 7);
    m.by_object.insert((EE_UNITS_LF_POINT_MODEL, 50), 60);
    approach_attach_point(&mut m, 1);
    assert!(m.log.contains(&(EE_UNITS_LF_LIST_TAG_POINT, 40, 0, 1)));
    assert!(m.log.contains(&(EE_UNITS_LF_POINT_LOCAL, 50, 0, 7)));
    assert_eq!(m.codes().iter().filter(|&&c| c == EE_UNITS_LF_INTERNAL_TRANSFORM).count(), 1);
    assert_eq!(m.records[0].tag, TAG_CBAP);
    // no tag points: no chain
    let mut m = Mock::default();
    approach_attach_point(&mut m, 1);
    assert!(!m.codes().contains(&EE_UNITS_LF_LIST_TAG_POINT));
}

#[test]
fn location_screen_checks_in_order() {
    let mut m = Mock::default();
    m.by_op.insert(EE_UNITS_LF_SCREEN_ON_MAP, 1);
    assert!(!location_screen(&mut m, 9, 1, 1));
    assert_eq!(m.codes(), vec![EE_UNITS_LF_SCREEN_ON_MAP, EE_UNITS_LF_TILE_AT_LOCATION]);
    let mut m = Mock::default();
    m.by_op.insert(EE_UNITS_LF_SCREEN_AREA, 4);
    m.by_op.insert(EE_UNITS_LF_MAP_WIDTH, 10);
    m.by_op.insert(EE_UNITS_LF_MAP_HEIGHT, 10);
    m.by_op.insert(EE_UNITS_LF_SCREEN_WATER, 1);
    m.by_op.insert(EE_UNITS_LF_CONTINENT_OF, 4);
    assert!(location_screen(&mut m, 9, 1, 1));
    assert_eq!(m.log.last().unwrap(), &(EE_UNITS_LF_CONTINENT_OF, 0, 2, 1));
    // a negative tile is off the grid (the unsigned comparison)
    assert!(!location_screen(&mut m, 9, -1, 1));
    let mut m = Mock::default();
    assert!(location_screen(&mut m, 9, 1, 1));
}
