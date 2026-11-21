use super::*;
use crate::units::uc_mock::Mock;

#[test]
fn a_simple_unit_is_created_on_its_tile_and_queued() {
    let mut h = Mock::default().answer(EE_UNITS_UC_SIMPLE_CREATE, 70);
    let made = create_at_point(&mut h, 1, 50, [3.5, 4.5, 0.0], 3, 4, 9, 0.0);
    assert_eq!(made, 70);
    assert_eq!(
        h.ops(),
        [
            EE_UNITS_UC_IS_COMPLEX,
            EE_UNITS_UC_THIS_UNIT_TYPE,
            EE_UNITS_UC_SIMPLE_CREATE,
            EE_UNITS_UC_SIMPLE_UNIT_CREATED,
            EE_UNITS_UC_PENDING_PUSH,
            EE_UNITS_UC_REFERENCE,
            EE_UNITS_UC_NUM_UNITS_INC,
            EE_UNITS_UC_DEREFERENCE,
        ]
    );
    assert_eq!(h.args(EE_UNITS_UC_SIMPLE_CREATE), [(50, 3, 4)]);
    assert_eq!(h.args(EE_UNITS_UC_PENDING_PUSH), [(50, 70, 0)]);
}

#[test]
fn a_complex_unit_is_grounded_turned_and_levelled_on_the_map() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_IS_COMPLEX, 1)
        .answer(EE_UNITS_UC_PITCHES, 1)
        .float(EE_UNITS_UC_GROUND_POINT, [3.5, 4.5, 2.0, 0.0])
        .float(EE_UNITS_UC_GROUND_LOCATION, [3.5, 4.5, 2.25, 0.0])
        .float(EE_UNITS_UC_LEVEL_GROUND, [2.5, 0.0, 0.0, 0.0])
        .answer(EE_UNITS_UC_COMPLEX_CREATE, 80);
    let made = create_at_point(&mut h, 1, 50, [3.5, 4.5, 0.0], 3, 4, 9, 1.0);
    assert_eq!(made, 80);
    assert!(h.has(EE_UNITS_UC_TERRAIN_PITCH));
    assert!(!h.has(EE_UNITS_UC_TERRAIN_ROLL));
    assert_eq!(h.floats_of(EE_UNITS_UC_LEVEL_GROUND)[0][0], 2.25);
    assert!(h.at(EE_UNITS_UC_LEVEL_GROUND) < h.at(EE_UNITS_UC_COMPLEX_CREATE));
    assert!(h.at(EE_UNITS_UC_COMPLEX_CREATE) < h.at(EE_UNITS_UC_UNIT_CREATED));
}

#[test]
fn base_create_unit_faces_pi_and_lists_the_shot() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_IS_COMPLEX, 1)
        .answer(EE_UNITS_UC_COMPLEX_CREATE, 80)
        .answer(EE_UNITS_UC_THIS_UNIT_TYPE, UNIT_TYPE_AIRPORT)
        .answer(EE_UNITS_UC_ATTACHED_EFFECT, 5)
        .answer(EE_UNITS_UC_CLASS_DOMAINS, 1);
    let made = base_create_unit(&mut h, 1, 3, 4, 2, false, 0);
    assert_eq!(made, 80);
    assert_eq!(h.floats_of(EE_UNITS_UC_GROUND_POINT)[0][..2], [3.5, 4.5]);
    assert_eq!(h.floats_of(EE_UNITS_UC_SET_ORIENTATION)[0][0], YAW_PI);
    assert!(h.has(EE_UNITS_UC_SET_PLANET));
    assert_eq!(h.args(EE_UNITS_UC_CREATE_RUNWAYS), [(80, 0, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_ATTACHED_EFFECT_ON), [(80, 0, 0)]);
    let tail = &h.ops()[h.at(EE_UNITS_UC_NUM_UNITS_INC)..];
    assert_eq!(tail, [EE_UNITS_UC_NUM_UNITS_INC, EE_UNITS_UC_INSERT_UNLISTED, EE_UNITS_UC_DEREFERENCE]);
}

#[test]
fn base_create_unit_at_floors_the_point_and_keeps_the_foundation_unloaded() {
    let mut m = GeTransformation::IDENTITY.m;
    m[0][3] = -0.5;
    m[1][3] = 7.25;
    let mut h = Mock::default().answer(EE_UNITS_UC_IS_COMPLEX, 0).answer(EE_UNITS_UC_SIMPLE_CREATE, 70);
    base_create_unit_at(&mut h, 1, m, 2, true, 0);
    assert_eq!(h.args(EE_UNITS_UC_SIMPLE_CREATE)[0].1, -1);
    assert_eq!(h.args(EE_UNITS_UC_SIMPLE_CREATE)[0].2, 7);
    assert!(!h.has(EE_UNITS_UC_LOAD_CLASS_MEDIA_OF));
    assert_eq!(h.args(EE_UNITS_UC_UNIT_LOAD_MEDIA), [(70, 1, 0)]);
    assert!(!h.has(EE_UNITS_UC_INSERT_UNLISTED));
}

#[test]
fn replant_farm_plants_the_eight_plots_around_the_unit() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_FARM_TYPE, 0x2a)
        .answer(EE_UNITS_UC_CLASS_TILE_SIZE, 2)
        .answer(EE_UNITS_UC_PRODUCER_TILE_X, 10)
        .answer(EE_UNITS_UC_PRODUCER_TILE_Y, 20)
        .answer(EE_UNITS_UC_CAN_PRODUCE, 1);
    h.takes = true;
    replant_farm(&mut h, 1, 2, 3);
    // the 3x3 grid from (8, 22) by steps of 2; the centre plot (10, 20) is skipped
    assert_eq!(h.count(EE_UNITS_UC_CREATE_UNIT_FOR), 8);
    let first = h.args(EE_UNITS_UC_MOVE_TO)[0];
    assert_eq!((first.1, first.2), (8, 22));
    assert!(!h.has(EE_UNITS_UC_SET_PHYSICAL_LOCATION));
}

#[test]
fn replant_farm_stops_when_the_player_cannot_produce() {
    let mut h = Mock::default().answer(EE_UNITS_UC_CLASS_TILE_SIZE, 1);
    h.takes = true;
    replant_farm(&mut h, 1, 2, 3);
    assert!(!h.has(EE_UNITS_UC_CREATE_UNIT_FOR));
}

#[test]
fn a_wall_farm_turns_its_pieces() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_FARM_TYPE, UNIT_TYPE_ROBOTIC_FARM)
        .answer(EE_UNITS_UC_CLASS_TILE_SIZE, 1)
        .answer(EE_UNITS_UC_CAN_PRODUCE, 1);
    h.takes = true;
    replant_farm(&mut h, 1, 2, 3);
    // the corner pieces (0, 2, 6, 8) are animated; the centre (4) is the unit's own tile
    assert_eq!(h.count(EE_UNITS_UC_SET_ANIMATION_OF), 4);
    assert_eq!(h.count(EE_UNITS_UC_SET_PHYSICAL_LOCATION), 8);
}

#[test]
fn remains_rise_to_a_minimum_height() {
    let mut h = Mock::default().answer(EE_UNITS_UC_REMAINS_CLASS, 40).answer(EE_UNITS_UC_IS_HUMAN, 1);
    h.placement = true;
    create_remains_unit(&mut h, 1, 2);
    assert_eq!(h.placements[0].flags, [false, false, false, false]);
    assert_eq!(h.placements[0].target, 2);
    assert!(h.has(EE_UNITS_UC_CREATE_AT_FOR));
}

#[test]
fn a_shot_is_created_where_the_launch_puts_it_and_armed() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_PROJECTILE_CLASS, 30)
        .answer(EE_UNITS_UC_GET_TILE, 1)
        .answer(EE_UNITS_UC_CREATE_UNIT_BY, 90)
        .answer(EE_UNITS_UC_COMPLEX_OF, 91)
        .answer(EE_UNITS_UC_NEW_ATTACK_UNIT, 92)
        .answer(EE_UNITS_UC_CREATION_EFFECT, 1);
    launch_shot(&mut h, 1, 2, 60, [5.0, 6.0, 0.0], 0);
    assert!(h.has(EE_UNITS_UC_CHAIN_CAPTURE));
    assert_eq!(h.args(EE_UNITS_UC_CHAIN_CAPTURE)[0].0, TAG_CBLS);
    assert!(h.has(EE_UNITS_UC_WORLD_CREATION_EFFECT_ON));
    assert_eq!(h.args(EE_UNITS_UC_SET_GOAL), [(91, 92, 0)]);
    assert!(h.has(EE_UNITS_UC_GET_VELOCITY));
    assert!(h.at(EE_UNITS_UC_SET_HEIGHT_OF) < h.at(EE_UNITS_UC_LAUNCH_PROJECTILE));
    assert_eq!(h.args(EE_UNITS_UC_LAUNCH_PROJECTILE)[0].0, 30);
    assert!(!h.has(EE_UNITS_UC_CLOSEST_TILE));
}

#[test]
fn a_carried_cargo_is_launched_instead_of_a_new_shot() {
    let mut h = Mock::default().answer(EE_UNITS_UC_CLASS_OF, 33).answer(EE_UNITS_UC_COMPLEX_OF, 77);
    launch_shot(&mut h, 1, 2, 0, [5.0, 6.0, 0.0], 77);
    assert!(h.has(EE_UNITS_UC_CLOSEST_TILE));
    assert!(!h.has(EE_UNITS_UC_CREATE_UNIT_BY));
    assert_eq!(h.args(EE_UNITS_UC_LAUNCH_PROJECTILE)[0].0, 33);
    assert!(h.has(EE_UNITS_UC_NEW_ATTACK_GROUND));
}

#[test]
fn without_a_turret_part_the_turret_launch_is_a_plain_shot() {
    let mut h = Mock::default().answer(EE_UNITS_UC_FIND_TURRET_PART, 1).answer(EE_UNITS_UC_GET_TILE, 1);
    launch_from_turret(&mut h, 1, 2, 0, [1.0, 2.0, 0.0]);
    assert!(h.has(EE_UNITS_UC_TAG_LIST));
    assert_eq!(h.args(EE_UNITS_UC_CHAIN_CAPTURE)[0].0, TAG_CBLS);
}

#[test]
fn the_turret_launch_fires_from_the_part() {
    let mut h = Mock::default().answer(EE_UNITS_UC_TURRET_PART, 2).answer(EE_UNITS_UC_CREATE_AT_BY, 90);
    launch_from_turret(&mut h, 1, 2, 0, [1.0, 2.0, 0.0]);
    assert_eq!(h.args(EE_UNITS_UC_TURRET_ANGLE), [(0, 2, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_CHAIN_CAPTURE)[0].0, TAG_CBLT);
    assert!(h.has(EE_UNITS_UC_CREATION_EFFECT));
    assert!(h.has(EE_UNITS_UC_VELOCITY_FIELD));
}

#[test]
fn a_dying_unit_spawns_plays_and_stops() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_DEATH_CLASS, 5)
        .answer(EE_UNITS_UC_UNIT_TILE, 6)
        .answer(EE_UNITS_UC_DEATH_SOUND, 7)
        .float(EE_UNITS_UC_SOUND_PARAMETERS, [0.5, 0.25, 1.0, 0.0]);
    unit_died(&mut h, 1, 2);
    assert!(h.has(EE_UNITS_UC_CREATE_DEATH_CLASS_UNIT));
    assert_eq!(h.floats_of(EE_UNITS_UC_PLAY_SOUND)[0][..3], [0.5, 0.25, 1.0]);
    assert_eq!(h.args(EE_UNITS_UC_SET_ANIMATION), [(0, 4, 0)]);
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_UC_DROP_PATH);
}

#[test]
fn an_icbm_dies_silently() {
    let mut h =
        Mock::default().answer(EE_UNITS_UC_PRODUCER_TYPE, UNIT_TYPE_CP_ICBM).answer(EE_UNITS_UC_UNIT_IS_WALL, 1);
    unit_died(&mut h, 1, 2);
    assert!(!h.has(EE_UNITS_UC_DEATH_SOUND));
    assert!(!h.has(EE_UNITS_UC_SET_ANIMATION));
}

#[test]
fn a_safe_location_is_sought_by_computer_players_only() {
    let mut h = Mock { escape: Some([1.0, 2.0]), ..Default::default() };
    find_safe_location(&mut h, 1, 2);
    assert_eq!(h.ops(), [EE_UNITS_UC_COMPUTER_PLAYER]);
    let mut h = Mock::default().answer(EE_UNITS_UC_COMPUTER_PLAYER, 9).answer(EE_UNITS_UC_NEW_LOCATION_2D, 44);
    h.escape = Some([1.0, 2.0]);
    find_safe_location(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UC_START_ACTION_SELF), [(0, ACTION_PREPARE_TO_MOVE, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_SET_GOAL_SELF), [(44, 0, 0)]);
}

#[test]
fn a_goalless_unit_walks_to_its_tile_centre() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_PRODUCER_TILE_X, 4)
        .answer(EE_UNITS_UC_PRODUCER_TILE_Y, 5)
        .float(EE_UNITS_UC_TILE_HEIGHT, [1.5, 0.0, 0.0, 0.0]);
    move_to_tile_centre(&mut h, 1, 2);
    assert_eq!(h.floats_of(EE_UNITS_UC_NEW_LOCATION)[0][..3], [4.5, 5.5, 1.5]);
}

#[test]
fn a_unit_with_a_goal_steps_toward_its_tile_centre() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_GOAL_STATE_SELF, 3)
        .answer(EE_UNITS_UC_PRODUCER_TILE_X, 4)
        .answer(EE_UNITS_UC_PRODUCER_TILE_Y, 5)
        .float(EE_UNITS_UC_LOCATION, [4.5, 4.0, 0.0, 0.0])
        .float(EE_UNITS_UC_WORLD_FLOAT, [0.25, 0.0, 0.0, 0.0]);
    move_to_tile_centre(&mut h, 1, 2);
    assert_eq!(h.floats_of(EE_UNITS_UC_SET_ORIENTATION)[0][0], 0.0);
    assert!(h.has(EE_UNITS_UC_UPDATE_UNIT_TILE));
}

#[test]
fn a_created_unit_is_set_up_in_order() {
    let mut h = Mock::default();
    base_unit_created(&mut h, 1, 2);
    assert_eq!(
        h.ops(),
        [
            EE_UNITS_UC_SET_HEAL_SEARCH_TIMER,
            EE_UNITS_UC_SET_FULL_HIT_POINTS,
            EE_UNITS_UC_SET_FULL_LAST_HIT_POINTS,
            EE_UNITS_UC_STOP,
            EE_UNITS_UC_SET_AMMUNITION,
            EE_UNITS_UC_SET_BEHAVIOR,
            EE_UNITS_UC_START_ACTION,
            EE_UNITS_UC_CLASS_ACTION_CHANGED,
        ]
    );
}

#[test]
fn tile_pairs_round_trip_negative_tiles() {
    let p = tile_pair(-10000, 7);
    assert_eq!(p as i32, -10000);
    assert_eq!((p >> 32) as i32, 7);
}
