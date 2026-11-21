use super::*;
use crate::units::uc_mock::Mock;

#[test]
fn a_healed_unit_with_a_goal_is_picked_to_leave() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCB_CARGO_SIZE, 2)
        .sequence(EE_UNITS_UCB_CARGO_INDEX, &[70, 71])
        .sequence(EE_UNITS_UCB_HIT_POINTS_OF, &[5, 10])
        .answer(EE_UNITS_UCB_MAX_HIT_POINTS_OF, 10)
        .answer(EE_UNITS_UCB_GOAL_STATE_OF, 9)
        .answer(EE_UNITS_UCB_UNIT_ID_OF, 333);
    assert!(select_to_release(&mut h, 1, 2));
    assert_eq!(h.args(EE_UNITS_UCB_SET_EXITING), [(0, 333, 0)]);
    // a computer player's unit keeps its goals
    assert!(!h.has(EE_UNITS_UCB_CLEAR_QUEUE0));
}

#[test]
fn a_human_bomber_leaves_by_its_rally_queue() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCB_CARGO_SIZE, 1)
        .answer(EE_UNITS_UCB_CARGO_INDEX, 70)
        .answer(EE_UNITS_UCB_HUMAN_OF, 1)
        .answer(EE_UNITS_UC_CLASS_FAMILY, FAMILY_BOMBER)
        .answer(EE_UNITS_UC_PARATROOPER_BOMBER, 1)
        .answer(EE_UNITS_UC_QUEUED_GOAL, 5);
    assert!(select_to_release(&mut h, 1, 2));
    assert_eq!(h.args(EE_UNITS_UC_QUEUED_GOAL), [(0, 4, 0)]);
}

#[test]
fn nobody_leaves_an_empty_building() {
    let mut h = Mock::default();
    assert!(!select_to_release(&mut h, 1, 2));
    assert!(!naval_select_to_release(&mut h, 1, 2));
}

#[test]
fn a_human_ship_with_a_move_goal_leaves_the_dock() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_CARGO_COUNT, 1)
        .answer(EE_UNITS_UCB_CARGO_INDEX, 70)
        .answer(EE_UNITS_UCB_PLAYER_HUMAN_OF, 1)
        .answer(EE_UNITS_UCB_QUEUE_LEN_OF, 1)
        .answer(EE_UNITS_UCB_QUEUE_FRONT_OF, 44)
        .answer(EE_UNITS_UCB_HAS_LOCATION, 1)
        .answer(EE_UNITS_UCB_UNIT_ID_OF, 7);
    assert!(naval_select_to_release(&mut h, 1, 2));
    assert_eq!(h.args(EE_UNITS_UCB_SET_EXITING), [(0, 7, 0)]);
}

#[test]
fn no_exiting_unit_releases_nothing() {
    let mut h = Mock::default().answer(EE_UNITS_UCB_EXITING_ID, 0xffff_ffff);
    release_unit(&mut h, 1, 2);
    assert!(!h.has(EE_UNITS_UCB_UNLOAD_BY_ID));
}

#[test]
fn a_released_unit_without_goals_goes_back_in() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCB_EXITING_ID, 12)
        .answer(EE_UNITS_UCB_AIR_TRAFFIC_SELF, 1)
        .answer(EE_UNITS_UCB_UNLOAD_BY_ID, 70);
    release_unit(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UCB_LOAD_CARGO), [(70, 0, 0)]);
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_UCB_SET_EXITING);
    assert!(!h.has(EE_UNITS_UCB_SET_HOME_BASE));
}

#[test]
fn a_released_unit_copies_its_own_queue_and_takes_off() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCB_EXITING_ID, 12)
        .answer(EE_UNITS_UCB_AIR_TRAFFIC_SELF, 1)
        .answer(EE_UNITS_UCB_FACING, 1)
        .answer(EE_UNITS_UCB_UNLOAD_BY_ID, 70)
        .sequence(EE_UNITS_UCB_QUEUE_LEN_OF, &[2, 2, 2, 2])
        .answer(EE_UNITS_UC_GOAL_SUITS, 1)
        .answer(EE_UNITS_UC_GOAL_CLONE, 88);
    release_unit(&mut h, 1, 2);
    assert_eq!(h.floats_of(EE_UNITS_UC_SET_ORIENTATION)[0][0], std::f32::consts::PI);
    assert_eq!(h.args(EE_UNITS_UC_QUEUE_GOAL_ON), [(70, 88, 0), (70, 88, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_START_ACTION), [(70, ACTION_TAKEOFF, 0)]);
    assert!(h.at(EE_UNITS_UCB_SET_HOME_BASE) < h.at(EE_UNITS_UC_START_ACTION));
}

#[test]
fn a_paratrooper_rally_drops_where_the_last_tile_is_free() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCB_EXITING_ID, 12)
        .answer(EE_UNITS_UCB_AIR_TRAFFIC_SELF, 1)
        .answer(EE_UNITS_UCB_UNLOAD_BY_ID, 70)
        .answer(EE_UNITS_UCB_CATEGORY_OF, CATEGORY_AIRPLANE)
        .answer(EE_UNITS_UCB_FAMILY_OF, FAMILY_BOMBER)
        .answer(EE_UNITS_UCB_PARATROOPER_OF, 1)
        .answer(EE_UNITS_UC_QUEUED_GOAL, 5)
        .answer(EE_UNITS_UCB_PLAYER_HUMAN_OF, 1)
        .sequence(EE_UNITS_UCB_UNIT_QUEUE_LEN, &[2, 2, 2, 2, 2])
        .sequence(EE_UNITS_UCB_UNIT_QUEUE_GOAL, &[40, 41])
        .answer(EE_UNITS_UCB_TILE_FREE, 1)
        .answer(EE_UNITS_UCB_NEW_UNLOAD_TRANSPORT, 66)
        .answer(EE_UNITS_UC_NEW_LOCATION, 55);
    release_unit(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UC_QUEUE_GOAL_ON), [(70, 55, 0), (70, 66, 0)]);
    assert_eq!(h.args(EE_UNITS_UCB_UNIT_QUEUE_GOAL)[0], (0, 4, 0));
}

#[test]
fn an_airport_faces_its_quarter() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_THIS_UNIT_TYPE, UNIT_TYPE_AIRPORT)
        .answer(EE_UNITS_UCB_FACING_QUARTER, 2)
        .answer(EE_UNITS_UCB_RESOURCE_TYPE, 1);
    unit_created(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UCB_SET_BUILT), [(0, 0, 3)]);
    assert!(h.has(EE_UNITS_UCB_SET_RESOURCE_AMOUNT));
    assert_eq!(h.args(EE_UNITS_UCB_SET_FACING), [(0, 2, 0)]);
    assert!(h.has(EE_UNITS_UC_SET_PHYSICAL_LOCATION));
    // BaseUnitCreated's statements ran before the resource stock
    assert!(h.at(EE_UNITS_UC_SET_HEAL_SEARCH_TIMER) < h.at(EE_UNITS_UCB_SET_RESOURCE_TYPE));
}

#[test]
fn a_disabled_armed_building_skips_its_tick() {
    let mut h = Mock::default().answer(EE_UNITS_UCB_DISABLED, 1).answer(EE_UNITS_UC_HIT_POINTS, 3);
    update_armed(&mut h, 1, 2);
    assert!(!h.has(EE_UNITS_UC_UPDATE_ORBITALS));
}

#[test]
fn an_armed_building_ticks_and_rots() {
    let mut h =
        Mock::default().answer(EE_UNITS_UC_HAS_PHYSICS_MODEL, 1).answer(EE_UNITS_UC_COMMAND_KIND, ACTION_UNIT_ROTTING);
    update_armed(&mut h, 1, 2);
    assert!(h.has(EE_UNITS_UC_PHYSICS_STEP));
    assert!(h.has(EE_UNITS_UCB_UPDATE_TURRETS));
    assert!(h.has(EE_UNITS_UC_UPDATE_ROTTING));
    assert!(h.at(EE_UNITS_UC_UPDATE_TARGET_SEARCH_TIMER) < h.at(EE_UNITS_UC_COMMAND_KIND));
}

#[test]
fn a_converting_temple_keeps_its_conversion_effects() {
    let mut h = Mock::default().answer(EE_UNITS_UC_COMMAND_KIND, ACTION_CONVERT_ENEMY_UNIT);
    priest_update(&mut h, 1, 2);
    assert!(h.has(EE_UNITS_UCB_PRIEST_CONVERSION));
    assert!(!h.has(EE_UNITS_UCB_HIDE_CONVERSION));
    let mut h = Mock::default().answer(EE_UNITS_UCB_CONVERSION_START, 5);
    priest_update(&mut h, 1, 2);
    assert!(h.has(EE_UNITS_UCB_CLEAR_CONVERSION_START));
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_UCB_HIDE_CONVERSION);
}

#[test]
fn buildings_sit_just_above_the_ground_and_docks_on_the_water() {
    let mut m = GeTransformation::IDENTITY.m;
    m[2][3] = 2.0;
    let mut h = Mock::default().answer(EE_UNITS_UC_IS_COMPLEX, 1);
    create_unit_at(&mut h, 1, m, 3, false, 0);
    // the transformation BaseCreateUnitAt levels: its z read into the levelling
    assert_eq!(h.floats_of(EE_UNITS_UC_LEVEL_GROUND)[0][0], 2.0 + BUILDING_LIFT);
    let mut h = Mock::default().answer(EE_UNITS_UC_THIS_UNIT_TYPE, UNIT_TYPE_DOCK).answer(EE_UNITS_UC_IS_COMPLEX, 1);
    create_unit_at(&mut h, 1, m, 3, false, 0);
    assert_eq!(h.floats_of(EE_UNITS_UC_LEVEL_GROUND)[0][0], 0.0);
}

#[test]
fn an_off_map_building_gets_the_sentinel() {
    let mut h = Mock::default().answer(EE_UNITS_UC_IS_COMPLEX, 1);
    create_unit(&mut h, 1, 3, 4, 1, false, 0);
    assert!(!h.has(EE_UNITS_UC_GROUND_POINT));
}

#[test]
fn a_mine_ties_to_the_first_resource_it_can_gather() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCB_TILE_SIZE_FIELD, 2)
        .answer(EE_UNITS_UC_SIMPLE_CREATE, 50)
        .answer(EE_UNITS_UC_GET_TILE, 9)
        .sequence(EE_UNITS_UCB_TILE_UNITS, &[100])
        .answer(EE_UNITS_UCB_NODE_UNIT, 77)
        .answer(EE_UNITS_UCB_CAN_GATHER, 1);
    let mine = mining_create_unit_at(&mut h, 1, GeTransformation::IDENTITY.m, 2, false, 0);
    assert_eq!(mine, 50);
    assert_eq!(h.args(EE_UNITS_UCB_SET_MINE), [(50, 77, 0)]);
}

#[test]
fn an_airport_ring_runs_along_its_runway() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_THIS_UNIT_TYPE, UNIT_TYPE_AIRPORT)
        .answer(EE_UNITS_UCB_COMPLEX_FIELD_OF, 5)
        .answer(EE_UNITS_UCB_TILE_SIZE_OF, 3)
        .answer(EE_UNITS_UC_UNIT_TILE_X, 10)
        .answer(EE_UNITS_UC_UNIT_TILE_Y, 20)
        .answer(EE_UNITS_UCB_FACING_QUARTER_AT, 0);
    surrounding_tiles(&mut h, 1, 2, 99, 1, false);
    assert!(h.has(EE_UNITS_UCB_TILES_CLEAR));
    assert_eq!(h.rings, [(1, [10, 20, 3, 12], 99, false)]);
    let mut h = Mock::default();
    surrounding_tiles(&mut h, 1, 2, 99, 1, true);
    assert_eq!(h.args(EE_UNITS_UCB_BASE_SURROUNDING), [(99, 1, 1)]);
}

#[test]
fn wall_pieces_name_their_parts() {
    let parts: Vec<(i64, bool)> = (0..9).map(wall_piece_part).collect();
    assert_eq!(
        parts,
        [(3, true), (0, false), (0, true), (3, false), (1, false), (2, false), (1, true), (1, false), (2, true)]
    );
}
