use super::*;
use crate::units::uc_mock::Mock;

/// A mock whose unit vector `v` lists `units`
fn with_units(mut h: Mock, v: i64, units: &[i64]) -> Mock {
    h.by_op_a.insert((EE_UNITS_UCB_UNITS_SIZE, v as u64), units.len() as i64);
    h.sequences.insert(EE_UNITS_UCB_UNITS_AT, units.to_vec());
    h
}

#[test]
fn a_gate_fits_on_five_built_walls_on_level_tiles() {
    let h = Mock::default()
        .answer(EE_UNITS_UCB_TILES_VEC, 90)
        .answer(EE_UNITS_UCB_BUILT_ALIVE, 1)
        .answer(EE_UNITS_UCB_TILES_LEVEL, 1);
    let mut h = with_units(h, 70, &[1, 2, 3, 4, 5]);
    assert!(wall_gate_fit(&mut h, [3, 4], 2, 0x30, 70));
    assert_eq!(h.args(EE_UNITS_UCB_WALL_GATE_TILES), [(90, pair(3, 4), 2)]);
    assert_eq!(h.args(EE_UNITS_UCB_UNITS_OF_TYPE_ON_TILES), [(90, 0x30, 70)]);
    assert!(!h.has(EE_UNITS_UCB_UNITS_CLEAR));
    // four walls: no gate, the list emptied
    let h = Mock::default().answer(EE_UNITS_UCB_BUILT_ALIVE, 1);
    let mut h = with_units(h, 70, &[1, 2, 3, 4]);
    assert!(!wall_gate_fit(&mut h, [3, 4], 2, 0x30, 70));
    assert_eq!(h.args(EE_UNITS_UCB_UNITS_CLEAR), [(70, 0, 0)]);
    assert!(!h.has(EE_UNITS_UCB_TILES_LEVEL));
}

#[test]
fn a_dead_wall_stops_the_check() {
    let h = Mock::default().sequence(EE_UNITS_UCB_BUILT_ALIVE, &[1, 0, 1]);
    let mut h = with_units(h, 70, &[1, 2, 3, 4, 5]);
    assert!(!wall_gate_fit(&mut h, [0, 0], 1, 1, 70));
    assert_eq!(h.count(EE_UNITS_UCB_BUILT_ALIVE), 2);
}

#[test]
fn a_gate_is_built_from_the_five_walls() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCB_UNITS_VEC, 60)
        .answer(EE_UNITS_UCB_PLAYER_AT, 5)
        .answer(EE_UNITS_UCB_TECH_CLASS, 6)
        .answer(EE_UNITS_UCB_CLASS_IS_WALL, 1)
        .answer(EE_UNITS_UCB_WALL_GATE_FIT, 1)
        .sequence(EE_UNITS_UCB_HIT_RATIO, &[])
        .sequence(EE_UNITS_UCB_REPLACE_UNIT, &[20, 21, 22, 23, 24])
        .sequence(EE_UNITS_UCB_MESH_Z, &[]);
    h.by_op_a.insert((EE_UNITS_UCB_UNITS_SIZE, 60), 6);
    h.sequences.insert(EE_UNITS_UCB_UNITS_AT, vec![10, 11, 12, 13, 14, 15]);
    h.floats.insert(EE_UNITS_UCB_HIT_RATIO, [0.5, 0.0, 0.0, 0.0]);
    build_gate(&mut h, [7, 8], 1, 99, 3);
    assert_eq!(h.count(EE_UNITS_UCB_REPLACE_UNIT), 5);
    assert_eq!(h.args(EE_UNITS_UCB_TURN_GATE_PART).iter().map(|e| e.2).collect::<Vec<_>>(), [0, 1, 2, 3, 4]);
    let links: Vec<(u64, i64)> = h.args(EE_UNITS_UCB_LINK_PART).iter().map(|e| (e.0, e.1)).collect();
    assert_eq!(links, [(24, 20), (20, 23), (23, 21), (21, 22)]);
    let actions: Vec<(u64, i64)> = h.args(EE_UNITS_UCB_START_ACTION_OF).iter().map(|e| (e.0, e.1)).collect();
    assert_eq!(
        actions,
        [(24, ACTION_IDLE), (20, ACTION_IDLE), (23, ACTION_CLOSED), (21, ACTION_CLOSED), (22, ACTION_CLOSED)]
    );
    assert_eq!(h.floats_of(EE_UNITS_UCB_SET_HEALTH)[0][0], 0.5);
    assert_eq!(h.args(EE_UNITS_UCB_SET_HEALTH)[0].0, 22);
    assert!(!h.has(EE_UNITS_UCB_FLUSH_PLACEMENTS));
    assert!(!h.has(EE_UNITS_UCB_ENERGY_WALL));
}

#[test]
fn no_wall_no_gate() {
    let mut h = Mock::default().answer(EE_UNITS_UCB_UNITS_VEC, 60);
    build_gate(&mut h, [7, 8], 1, 99, 3);
    assert!(!h.has(EE_UNITS_UCB_WALL_GATE_FIT));
    assert!(!h.has(EE_UNITS_UCB_REPLACE_UNIT));
}

#[test]
fn a_replaced_gate_piece_takes_its_four_pieces_along() {
    let mut h = Mock::default()
        .sequence(EE_UNITS_UCB_REPLACE_UNIT, &[30, 31, 33, 34])
        .answer(EE_UNITS_UCB_COMMAND_TYPE_OF, 15002);
    gate_replaced(&mut h, 1, 2, 32);
    assert_eq!(h.args(EE_UNITS_UCB_GET_PART).iter().map(|e| e.1).collect::<Vec<_>>(), [0, 1, 3, 4]);
    let links: Vec<(u64, i64)> = h.args(EE_UNITS_UCB_LINK_PART).iter().map(|e| (e.0, e.1)).collect();
    assert_eq!(links, [(34, 30), (30, 33), (33, 31), (31, 32)]);
    let actions: Vec<(u64, i64)> = h.args(EE_UNITS_UCB_START_ACTION_OF).iter().map(|e| (e.0, e.1)).collect();
    assert_eq!(actions, [(34, ACTION_IDLE), (30, ACTION_IDLE), (33, 15002), (31, 15002)]);
}

#[test]
fn the_middle_wall_of_the_one_fitting_direction_is_highlighted() {
    let mut h = Mock::default()
        .sequence(EE_UNITS_UCB_UNITS_VEC, &[70, 71, 72, 73, 74, 75])
        .sequence(EE_UNITS_UCB_FIT_ON_UNIT, &[0, 1, 0, 0]);
    // the clear's list (70) is empty; the player's walls (71) one; side 2 (73) four walls
    h.by_op_a.insert((EE_UNITS_UCB_UNITS_SIZE, 71), 1);
    h.by_op_a.insert((EE_UNITS_UCB_UNITS_SIZE, 73), 4);
    h.sequences.insert(EE_UNITS_UCB_UNITS_AT, vec![40, 50, 51, 52, 53]);
    h.sequences.insert(EE_UNITS_UCB_COMPLEX_FIELD, vec![51]);
    highlight_gate_walls(&mut h, 9, 0x30);
    assert_eq!(h.args(EE_UNITS_UCB_SET_COLOR), [(51, 1, 0)]);
    assert_eq!(h.args(EE_UNITS_UCB_SET_GATE_DIRECTION_OF), [(51, 2, 0)]);
    assert_eq!(h.args(EE_UNITS_UCB_FIT_ON_UNIT)[1], (40, pair(2, 0x30), 73));
}

#[test]
fn the_gate_wall_is_the_first_tinted_wall_on_the_tile() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCB_MAP_TILE, 5)
        .answer(EE_UNITS_UCB_TILE_NODES, 6)
        .sequence(EE_UNITS_UCB_GATE_NODE_UNIT, &[0, 8])
        .sequence(EE_UNITS_UCB_GATE_NODE_NEXT, &[7, 0])
        .answer(EE_UNITS_UCB_TINTED_WALL, 1)
        .answer(EE_UNITS_UCB_GATE_DIRECTION_OF, 3);
    assert_eq!(find_gate_wall(&mut h, [1, 2]), Some(3));
    assert_eq!(find_gate_wall(&mut Mock::default(), [1, 2]), None);
}

#[test]
fn the_nearest_target_building_is_found_ring_by_ring() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCB_TILES_VEC, 90)
        .sequence(EE_UNITS_UCB_TILES_SIZE, &[0, 1])
        .answer(EE_UNITS_UCB_MAP_TILE, 5)
        .answer(EE_UNITS_UCB_TILE_NODES, 6)
        .answer(EE_UNITS_UCB_GATE_NODE_UNIT, 8)
        .answer(EE_UNITS_UCB_TARGET_BUILDING, 1);
    assert_eq!(nearest_target_building(&mut h, 1, 2), 8);
    assert_eq!(h.args(EE_UNITS_UCB_RING).iter().map(|e| e.2).collect::<Vec<_>>(), [1, 2]);
}

#[test]
fn a_gate_lists_the_tiles_of_its_pieces_not_left_out() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCB_COMPLEX_OF, 3)
        .sequence(EE_UNITS_UCB_PREVIOUS_PART, &[2, 0])
        .sequence(EE_UNITS_UCB_NEXT_PART_OF, &[3, 0])
        .sequence(EE_UNITS_UCB_PART_LEFT_OUT, &[0, 1]);
    gate_surrounding(&mut h, 1, 3, 77, 1, false);
    assert_eq!(h.args(EE_UNITS_UCB_GATE_TILES_CLEAR), [(77, 0, 0)]);
    assert_eq!(h.args(EE_UNITS_UCB_GATE_BASE_SURROUNDING), [(2, 77, 1)]);
    assert!(h.at(EE_UNITS_UCB_GATE_BASE_SURROUNDING) < h.at(EE_UNITS_UCB_SORT_UNIQUE));
}

#[test]
fn the_chain_head_gate_starts_closed() {
    let mut h = Mock::default().answer(EE_UNITS_UCB_CHAIN_HEAD, 1);
    gate_unit_created(&mut h, 1, 2);
    assert_eq!(
        h.ops(),
        [
            EE_UNITS_UCB_BUILDING_UNIT_CREATED,
            EE_UNITS_UCB_CHAIN_HEAD,
            EE_UNITS_UCB_START_ACTION,
            EE_UNITS_UCB_ACTION_CHANGED
        ]
    );
}

#[test]
fn a_space_building_arrives_on_its_waypoint_tile() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCB_SELF_TILE_X, 3)
        .answer(EE_UNITS_UCB_SELF_TILE_Y, 4)
        .float(EE_UNITS_UCB_NEXT_WAYPOINT, [3.5, 4.5, 0.0, 0.0]);
    assert!(space_arrived(&mut h, 1, 2, 1.0));
    assert!(space_arrived(&mut Mock::default(), 1, 2, 0.05));
    let mut h = Mock::default().float(EE_UNITS_UCB_NEXT_WAYPOINT, [3.5, 4.5, 0.0, 0.0]);
    assert!(!space_arrived(&mut h, 1, 2, 1.0));
}
