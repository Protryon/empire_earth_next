use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, ghost, a, b, c), answers from `answers` keyed by (op, a, b) (0 else);
/// a new ghost's handle counts up from 100.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, u64, i64, i64)>,
    answers: BTreeMap<(i32, u64, i64), i64>,
    next: u64,
}

impl GhostHost for Mock {
    fn ghost(&mut self, op: i32, ghost: u64, a: u64, b: i64, c: i64) -> i64 {
        self.log.push((op, ghost, a, b, c));
        if op == EE_UNITS_GHOST_NEW {
            self.next += 1;
            return (100 + self.next) as i64;
        }
        *self.answers.get(&(op, a, b)).unwrap_or(&0)
    }
}

impl Mock {
    fn count(&self, op: i32) -> usize {
        self.log.iter().filter(|e| e.0 == op).count()
    }
}

#[test]
fn an_airport_brings_three_runways() {
    let mut h = Mock::default();
    h.answers.insert((EE_UNITS_GHOST_CLASS_AT, 0, UNIT_TYPE_AIRPORT), 50);
    create(&mut h, UNIT_TYPE_AIRPORT, 9, 0.5, false);
    assert_eq!(h.count(EE_UNITS_GHOST_ADD), 4);
    assert_eq!(h.log[2].0, EE_UNITS_GHOST_LOCK_ENTER);
    assert_eq!(h.log.last().unwrap().0, EE_UNITS_GHOST_LOCK_LEAVE);
    // a turning class takes the rotation
    assert!(h.log.iter().any(|e| e.0 == EE_UNITS_GHOST_SET_YAW && e.3 == bits(0.5)));
}

#[test]
fn a_granary_brings_its_farms_and_robotic_pieces_turn() {
    let mut h = Mock::default();
    h.answers.insert((EE_UNITS_GHOST_CLASS_AT, 0, UNIT_TYPE_GRANARY), 50);
    h.answers.insert((EE_UNITS_GHOST_CLASS_TYPE, 50, 0), UNIT_TYPE_GRANARY);
    h.answers.insert((EE_UNITS_GHOST_CLASS_TILE_SIZE, 50, 0), 2);
    h.answers.insert((EE_UNITS_GHOST_FARM_TYPE, 9, 0), UNIT_TYPE_ROBOTIC_FARM);
    h.answers.insert((EE_UNITS_GHOST_CLASS_AT, 0, UNIT_TYPE_ROBOTIC_FARM), 60);
    h.answers.insert((EE_UNITS_GHOST_CLASS_TYPE, 60, 0), UNIT_TYPE_ROBOTIC_FARM);
    h.answers.insert((EE_UNITS_GHOST_WALL_PIECE_PART, 0, 5), 2);
    h.answers.insert((EE_UNITS_GHOST_CLASS_FIXED, 60, 0), 1);
    create(&mut h, UNIT_TYPE_GRANARY, 9, 0.5, false);
    // 3 x 3 - 1 farm ghosts and the granary's
    assert_eq!(h.count(EE_UNITS_GHOST_ADD), 9);
    // the fifth piece (index 4) asks for part 5 and turns to the third yaw
    assert!(h.log.iter().any(|e| e.0 == EE_UNITS_GHOST_SET_YAW && e.3 == bits(WALL_ROTATIONS[2])));
    let parts: Vec<i64> = h.log.iter().filter(|e| e.0 == EE_UNITS_GHOST_WALL_PIECE_PART).map(|e| e.3).collect();
    assert_eq!(parts, [0, 1, 2, 3, 5, 6, 7, 8]);
    // moved: no farms
    let mut h = Mock::default();
    h.answers.insert((EE_UNITS_GHOST_CLASS_TYPE, 0, 0), UNIT_TYPE_GRANARY);
    create(&mut h, UNIT_TYPE_GRANARY, 9, 0.5, true);
    assert_eq!(h.count(EE_UNITS_GHOST_ADD), 1);
}

#[test]
fn a_gate_has_three_turned_parts() {
    let mut h = Mock::default();
    create_gate(&mut h, 0.25, 50, 9);
    let parts: Vec<(i64, i64)> =
        h.log.iter().filter(|e| e.0 == EE_UNITS_GHOST_TURN_GATE_PART).map(|e| (e.3, e.4)).collect();
    assert_eq!(parts, [(2, 0), (2, 2), (2, 4)]);
    assert_eq!(h.count(EE_UNITS_GHOST_ADD), 3);
}

#[test]
fn wall_ghosts_stand_on_the_ground_of_their_tiles() {
    let mut h = Mock::default();
    h.answers.insert((EE_UNITS_GHOST_TILES_SIZE, 70, 0), 2);
    h.answers.insert((EE_UNITS_GHOST_TILES_AT, 70, 0), 3);
    h.answers.insert((EE_UNITS_GHOST_TILES_AT, 70, 1), 4);
    h.answers.insert((EE_UNITS_GHOST_MESH_Z, 0, bits(3.5)), bits(-1.0));
    build_wall_ghosts(&mut h, 70, 71, 12, 9, 0.0);
    // no tile is on the map in the mock: neither can be placed; the heights stay 0
    assert_eq!(h.count(EE_UNITS_GHOST_SET_CAN_PLACE), 2);
    assert!(h.log.iter().any(|e| e.0 == EE_UNITS_GHOST_SET_Z && e.3 == bits(0.0)));
    assert!(h.log.iter().any(|e| e.0 == EE_UNITS_GHOST_SET_CHANGED && e.3 == 1));
}
