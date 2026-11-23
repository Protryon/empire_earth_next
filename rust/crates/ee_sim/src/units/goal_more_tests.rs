use super::*;
use crate::units::goal_split::UnitRead;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, i[0], f[0..3]), its answer from `answers` keyed by (op, i[0])
/// (0 by default); X_GOAL_POINT_OUT writes `point`.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, u64, i64, [f32; 3])>,
    answers: BTreeMap<(i32, i64), i64>,
    point: [f32; 3],
}

impl Mock {
    fn answer(mut self, op: i32, i0: i64, value: i64) -> Self {
        self.answers.insert((op, i0), value);
        self
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
}

impl GoalSplitHost for Mock {
    fn raw(&mut self, op: i32, a: u64, b: u64, io: &mut EeUnitsGoalIo) -> i64 {
        self.log.push((op, a, b, io.i[0], [io.f[0], io.f[1], io.f[2]]));
        if op == EE_UNITS_GOAL_HOST_X_GOAL_POINT_OUT {
            io.f[..3].copy_from_slice(&self.point);
        }
        *self.answers.get(&(op, io.i[0])).unwrap_or(&0)
    }
    fn read(&mut self, _unit: u64, _what: UnitRead) -> i64 {
        0
    }
    fn position(&mut self, unit: u64) -> [f32; 3] {
        [unit as f32, 2.0, 3.0]
    }
    fn deposit_load(&mut self, _unit: u64, _container: u64) {}
    fn resource_index(&mut self, _unit: u64) -> i64 {
        0
    }
}

#[test]
fn other_operations_are_not_split() {
    let mut h = Mock::default();
    assert_eq!(split_more(&mut h, EE_UNITS_GOAL_HOST_CAN_LOAD, 1, 2, &mut EeUnitsGoalIo::default()), None);
    assert!(h.log.is_empty());
}

#[test]
fn boarding_hands_the_out_point_in_and_back() {
    let mut h = Mock { point: [7.0, 8.0, 9.0], ..Default::default() };
    let mut io = EeUnitsGoalIo::default();
    io.f[..6].copy_from_slice(&[1.0, 2.0, 3.0, 4.0, 5.0, 6.0]);
    assert_eq!(split_more(&mut h, EE_UNITS_GOAL_HOST_BOARDING, 10, 11, &mut io), Some(0));
    assert_eq!(&io.f[..6], &[1.0, 2.0, 3.0, 7.0, 8.0, 9.0]);
    assert_eq!(h.log[0], (EE_UNITS_GOAL_HOST_X_SET_GOAL_POINT, 0, 0, 0, [4.0, 5.0, 6.0]));
    assert_eq!(h.log[1], (EE_UNITS_GOAL_HOST_X_BOARDING_FIND, 10, 11, 0, [1.0, 2.0, 3.0]));
    assert_eq!(h.log[2].0, EE_UNITS_GOAL_HOST_X_GOAL_POINT_OUT);
}

#[test]
fn surrounding_tiles_go_onto_the_sink() {
    let mut h = Mock::default()
        .answer(EE_UNITS_GOAL_HOST_X_TILES_COUNT, 0, 2)
        .answer(EE_UNITS_GOAL_HOST_X_TILE_X, 0, 5)
        .answer(EE_UNITS_GOAL_HOST_X_TILE_Y, 0, 6)
        .answer(EE_UNITS_GOAL_HOST_X_TILE_X, 1, -1)
        .answer(EE_UNITS_GOAL_HOST_X_TILE_Y, 1, 7);
    let mut tiles: Vec<(i32, i32)> = Vec::new();
    let mut io = EeUnitsGoalIo { sink: (&mut tiles as *mut Vec<(i32, i32)>).cast(), ..Default::default() };
    split_more(&mut h, EE_UNITS_GOAL_HOST_SURROUNDING_TILES, 3, 0, &mut io);
    assert_eq!(tiles, vec![(5, 6), (-1, 7)]);
    assert_eq!(h.log[1].0, EE_UNITS_GOAL_HOST_X_SURROUNDING_FILL);
    assert_eq!(h.log[1].1, 3);
}

#[test]
fn resource_searches_take_the_handed_units() {
    let passed = [21u64, 22];
    let mut h = Mock::default().answer(EE_UNITS_GOAL_HOST_X_NEXT_RESOURCE, 4, 99);
    let mut io = EeUnitsGoalIo { list: passed.as_ptr(), list_len: 2, ..Default::default() };
    io.i[0] = 4;
    assert_eq!(split_more(&mut h, EE_UNITS_GOAL_HOST_NEXT_RESOURCE, 1, 2, &mut io), Some(99));
    assert_eq!(
        h.ops(),
        vec![
            EE_UNITS_GOAL_HOST_X_UNITS_CLEAR,
            EE_UNITS_GOAL_HOST_X_UNITS_PUSH,
            EE_UNITS_GOAL_HOST_X_UNITS_PUSH,
            EE_UNITS_GOAL_HOST_X_NEXT_RESOURCE
        ]
    );
    assert_eq!((h.log[1].2, h.log[2].2), (21, 22));
    let mut h = Mock::default();
    let mut io = EeUnitsGoalIo::default();
    io.i[..4].copy_from_slice(&[3, 2, 1, 77]);
    split_more(&mut h, EE_UNITS_GOAL_HOST_RESOURCE_OF_KIND, 1, 2, &mut io);
    assert_eq!(h.ops(), vec![EE_UNITS_GOAL_HOST_X_UNITS_CLEAR, EE_UNITS_GOAL_HOST_X_RESOURCE_OF_KIND]);
    assert_eq!(h.log[1].3, 3);
}

#[test]
fn the_mining_beam_starts_at_the_resource() {
    let mut h = Mock::default();
    split_more(&mut h, EE_UNITS_GOAL_HOST_MINING_BEAM, 1, 6, &mut EeUnitsGoalIo::default());
    assert_eq!(h.log[0], (EE_UNITS_GOAL_HOST_X_SET_GOAL_POINT, 0, 0, 0, [6.0, 2.0, 3.0]));
    assert_eq!(
        &h.ops()[1..],
        &[
            EE_UNITS_GOAL_HOST_X_RESET_EFFECT,
            EE_UNITS_GOAL_HOST_X_MINING_BEAM_EFFECT,
            EE_UNITS_GOAL_HOST_X_SET_FIRE_EFFECT
        ]
    );
}

#[test]
fn the_rendezvous_units() {
    let units = [31u64];
    let mut h = Mock::default();
    complexes_in(&mut h, &EeUnitsGoalIo { list: units.as_ptr(), list_len: 1, ..Default::default() });
    assert_eq!(h.ops(), vec![EE_UNITS_GOAL_HOST_X_COMPLEXES_CLEAR, EE_UNITS_GOAL_HOST_X_COMPLEXES_PUSH]);
}
