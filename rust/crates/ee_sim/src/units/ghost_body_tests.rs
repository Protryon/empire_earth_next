use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, ghost, a, b, c), its answer from `answers` keyed by (op, ghost) or,
/// for the tile reads, (op, b, c) through `tiles`; the footprint and heights written are kept.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, u64, i64, i64)>,
    answers: BTreeMap<(i32, u64), i64>,
    /// (x, y) -> terrain of the tiles that exist
    tiles: BTreeMap<(i64, i64), i64>,
    /// the cells where EECanPlace says no
    blocked: Vec<(i64, i64)>,
    footprint: BTreeMap<(i64, i64), bool>,
    heights: BTreeMap<i64, f32>,
    can_place: BTreeMap<u64, bool>,
}

impl Mock {
    fn answer(mut self, op: i32, ghost: u64, value: i64) -> Self {
        self.answers.insert((op, ghost), value);
        self
    }
    fn land(mut self, x0: i64, x1: i64, y0: i64, y1: i64, terrain: i64) -> Self {
        for x in x0..=x1 {
            for y in y0..=y1 {
                self.tiles.insert((x, y), terrain);
            }
        }
        self
    }
    fn count(&self, op: i32) -> usize {
        self.log.iter().filter(|e| e.0 == op).count()
    }
}

impl GhostHost for Mock {
    fn ghost(&mut self, op: i32, ghost: u64, a: u64, b: i64, c: i64) -> i64 {
        self.log.push((op, ghost, a, b, c));
        match op {
            EE_UNITS_GHOST_TILE_EXISTS => i64::from(self.tiles.contains_key(&(b, c))),
            EE_UNITS_GHOST_TILE_TERRAIN => self.tiles[&(b, c)],
            EE_UNITS_GHOST_TERRAIN_Z => bits(2.0),
            EE_UNITS_GHOST_CAN_PLACE_AT => i64::from(!self.blocked.contains(&(b, c))),
            EE_UNITS_GHOST_IS_LAND_TERRAIN => i64::from(b == 0),
            EE_UNITS_GHOST_SET_FOOTPRINT => {
                self.footprint.insert((b, c), a != 0);
                0
            }
            EE_UNITS_GHOST_SET_HEIGHT => {
                self.heights.insert(b, float(c));
                0
            }
            EE_UNITS_GHOST_SET_CAN_PLACE => {
                self.can_place.insert(ghost, b != 0);
                0
            }
            EE_UNITS_GHOST_CAN_PLACE => i64::from(*self.can_place.get(&ghost).unwrap_or(&false)),
            EE_UNITS_GHOST_UM_CEIL => bits(float(b).ceil()),
            _ => *self.answers.get(&(op, ghost)).unwrap_or(&0),
        }
    }
}

const G: u64 = 7;

#[test]
fn the_footprint_offset_follows_the_size() {
    for (size, offset) in [(1, 1.0), (3, 2.0), (4, 2.5)] {
        let mut h = Mock::default().answer(EE_UNITS_GHOST_TILE_SIZE, G, size);
        assert_eq!(footprint_offset(&mut h, G), offset);
    }
}

#[test]
fn a_plain_building_fills_its_square_and_heights() {
    // a 2-tile building at (10, 10): the square 9..=11, its last row and column only for the heights
    let mut h = Mock::default()
        .answer(EE_UNITS_GHOST_TILE_SIZE, G, 2)
        .answer(EE_UNITS_GHOST_TILE_X, G, 10)
        .answer(EE_UNITS_GHOST_TILE_Y, G, 10)
        .land(9, 11, 9, 11, 0);
    h.tiles.insert((10, 10), TERRAIN_SHALLOW_WATER);
    fill_square(&mut h, G);
    assert_eq!(h.can_place[&G], true);
    assert_eq!(h.footprint.len(), 9);
    assert_eq!(h.heights[&(1 * ROW + 1)], 0.0);
    assert_eq!(h.heights[&0], 2.0);
    // a blocked inner cell refuses it; one on the outer edge does not
    let mut h = Mock::default()
        .answer(EE_UNITS_GHOST_TILE_SIZE, G, 2)
        .answer(EE_UNITS_GHOST_TILE_X, G, 10)
        .answer(EE_UNITS_GHOST_TILE_Y, G, 10)
        .land(9, 11, 9, 11, 0);
    h.blocked.push((11, 11));
    fill_square(&mut h, G);
    assert_eq!(h.can_place[&G], true);
    h.blocked.push((9, 9));
    fill_square(&mut h, G);
    assert_eq!(h.can_place[&G], false);
}

#[test]
fn a_dock_needs_three_land_rim_tiles() {
    // a 1-tile dock at (5, 5): offset 1, the square 4..=6, the inner cell (5, 5)
    let base = || {
        Mock::default()
            .answer(EE_UNITS_GHOST_UNIT_TYPE, G, UNIT_TYPE_DOCK)
            .answer(EE_UNITS_GHOST_TILE_SIZE, G, 1)
            .answer(EE_UNITS_GHOST_TILE_X, G, 5)
            .answer(EE_UNITS_GHOST_TILE_Y, G, 5)
            .land(4, 6, 4, 6, TERRAIN_DEEP_WATER)
    };
    let mut h = base();
    h.tiles.insert((4, 4), 0);
    h.tiles.insert((4, 5), 0);
    fill_square(&mut h, G);
    assert_eq!(h.can_place[&G], false);
    assert_eq!(h.footprint[&(0, 0)], false);
    let mut h = base();
    for t in [(4, 4), (4, 5), (4, 6)] {
        h.tiles.insert(t, 0);
    }
    fill_square(&mut h, G);
    assert_eq!(h.can_place[&G], true);
    assert_eq!(h.footprint[&(0, 0)], true);
}

#[test]
fn a_space_dock_turns_to_its_yaw() {
    let mut h = Mock::default()
        .answer(EE_UNITS_GHOST_UNIT_TYPE, G, UNIT_TYPE_SPACE_DOCK)
        .answer(EE_UNITS_GHOST_TILE_SIZE, G, 3)
        .answer(EE_UNITS_GHOST_SPACE_DOCK_SITE_OPEN, G, 1)
        .answer(EE_UNITS_GHOST_SPACE_DOCK_YAW, G, bits(0.75));
    fill_square(&mut h, G);
    assert_eq!(h.can_place[&G], true);
    // offset 2: the inner 3 x 3
    assert_eq!(h.footprint.values().filter(|v| **v).count(), 9);
    assert!(h.log.iter().any(|e| e.0 == EE_UNITS_GHOST_SET_YAW && e.3 == bits(0.75)));
}

#[test]
fn a_mining_building_needs_its_overlap_unit() {
    let mut h = Mock::default()
        .answer(EE_UNITS_GHOST_CATEGORY, G, CATEGORY_MINING_BUILDING)
        .answer(EE_UNITS_GHOST_TILE_SIZE, G, 3)
        .answer(EE_UNITS_GHOST_TILE_FIRST_NODE, G, 40);
    h.answers.insert((EE_UNITS_GHOST_NODE_IN_SET, G), 1);
    fill_square(&mut h, G);
    assert_eq!(h.can_place[&G], true);
    assert_eq!(h.footprint.len(), 9);
    assert_eq!(h.count(EE_UNITS_GHOST_NODE_NEXT), 1);
}

#[test]
fn the_preview_moves_each_ghost_and_refills_it() {
    let mut h = Mock::default()
        .answer(EE_UNITS_GHOST_VIEW_CONTAINS, 0, 1)
        .answer(EE_UNITS_GHOST_NUM_GHOSTS, 0, 1)
        .answer(EE_UNITS_GHOST_GHOST_AT, 0, G as i64)
        .answer(EE_UNITS_GHOST_POINT_UNIT_OR_TILE, 0, 1)
        .answer(EE_UNITS_GHOST_TILE_SIZE, G, 1)
        .answer(EE_UNITS_GHOST_IS_LAND_BUILDING, G, 1)
        .land(0, 20, 0, 20, 0);
    preview_placement(&mut h, 99);
    let ops: Vec<i32> = h.log.iter().map(|e| e.0).collect();
    assert_eq!(ops[0], EE_UNITS_GHOST_POSITION_RESET);
    assert_eq!(ops[1], EE_UNITS_GHOST_LOCK_ENTER);
    assert_eq!(*ops.last().unwrap(), EE_UNITS_GHOST_LOCK_LEAVE);
    assert!(h.log.iter().any(|e| e.0 == EE_UNITS_GHOST_SET_CHANGED && e.3 == 1));
    assert_eq!(h.count(EE_UNITS_GHOST_POSITION_FROM_TRANSFORM), 1);
    assert_eq!(h.count(EE_UNITS_GHOST_LOCATION_TO_TILE), 1);
    // off the view: nothing moves
    let mut h = Mock::default();
    preview_placement(&mut h, 99);
    assert_eq!(h.count(EE_UNITS_GHOST_GHOST_AT), 0);
}

#[test]
fn the_gate_preview_takes_a_marked_walls_direction() {
    let mut h = Mock::default()
        .answer(EE_UNITS_GHOST_VIEW_CONTAINS, 0, 1)
        .answer(EE_UNITS_GHOST_POINT_TILE_XY, 0, 0)
        .answer(EE_UNITS_GHOST_TILE_FIRST_NODE, 0, 30)
        .answer(EE_UNITS_GHOST_NODE_UNIT, 0, 31)
        .answer(EE_UNITS_GHOST_UNIT_IS_WALL, 0, 1)
        .answer(EE_UNITS_GHOST_UNIT_TINTED, 0, 1)
        .answer(EE_UNITS_GHOST_UNIT_GATE_DIRECTION, 0, 3)
        .land(0, 3, 0, 3, 0);
    let mut d = 1;
    preview_gate(&mut h, 99, &mut d);
    assert_eq!(d, 3);
    assert_eq!(h.count(EE_UNITS_GHOST_TURN_GATE_PART), 3);
    let parts: Vec<i64> = h.log.iter().filter(|e| e.0 == EE_UNITS_GHOST_TURN_GATE_PART).map(|e| e.4).collect();
    assert_eq!(parts, [2, 0, 4]);
    assert!(h.can_place.values().all(|v| *v));
}
