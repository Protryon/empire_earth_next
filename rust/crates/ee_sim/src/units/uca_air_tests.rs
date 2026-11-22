use super::*;
use crate::units::uc_mock::Mock;

impl UcaRust for Mock {
    fn path_toward(&mut self, unit: u64, from: [f32; 3], to: [f32; 3], snap: bool) {
        self.log.push((-10, unit, i64::from(snap), 0));
        self.floats_in.push((-10, [to[0], to[1], to[2], from[0]]));
    }
    fn add_waypoint_at(&mut self, unit: u64, p: [f32; 3], snap: bool) {
        self.log.push((-11, unit, i64::from(snap), 0));
        self.floats_in.push((-11, [p[0], p[1], p[2], 0.0]));
    }
    fn aim_point(&mut self, unit: u64, target: [f32; 3]) -> [f32; 3] {
        self.log.push((-12, unit, 0, 0));
        [target[0] + 1.0, target[1], target[2]]
    }
}

fn path_to(h: &Mock) -> [f32; 3] {
    let f = h.floats_of(-10)[0];
    [f[0], f[1], f[2]]
}

#[test]
fn a_location_goal_flies_at_cruise_height_to_its_landing_point() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCA_GOAL_STATE, 9)
        .answer(EE_UNITS_UCA_HAS_LOCATION, 1)
        .float(EE_UNITS_UCA_LOCATION_POINT, [4.0, 5.0, 6.0, 0.0])
        .float(EE_UNITS_UCA_CRUISE, [3.0, 0.0, 0.0, 0.0]);
    fly_to_goal(&mut h, 1, 2, [0.0; 3]);
    assert_eq!(h.floats_of(EE_UNITS_UCA_LANDING_POINT)[0], [4.0, 5.0, 3.0, 0.0]);
    assert_eq!(h.args(-10), [(2, 1, 0)]);
    assert!(!h.has(EE_UNITS_UCA_GOAL_CAST));
}

#[test]
fn an_explore_goal_without_a_target_finds_an_area_first() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCA_GOAL_STATE, 9)
        .sequence(EE_UNITS_UCA_GOAL_CAST, &[0, 9, 9])
        .answer(EE_UNITS_UCA_AIR_TRAFFIC_TYPE, 2);
    fly_to_goal(&mut h, 1, 2, [0.0; 3]);
    assert_eq!(
        h.args(EE_UNITS_UCA_START_ACTION),
        [(0, ACTION_FIND_UNEXPLORED_AREA, 0), (0, ACTION_CONTINUE_TO_TAXI, 0)]
    );
    assert!(h.at(EE_UNITS_UCA_GOAL_STEP) < h.at(-10));
    assert_eq!(h.count(EE_UNITS_UCA_EXPLORE_CENTER), 2);
}

#[test]
fn an_attack_on_a_simple_unit_flies_to_its_footprint_centre() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCA_GOAL_STATE, 9)
        .answer(EE_UNITS_UCA_ATTACK_TARGET, 30)
        .answer(EE_UNITS_UCA_TILE_SIZE_OF, 2)
        .answer(EE_UNITS_UCA_TILE_X_OF, 10)
        .answer(EE_UNITS_UCA_TILE_Y_OF, 20);
    fly_to_goal(&mut h, 1, 2, [0.0; 3]);
    assert_eq!(path_to(&h), [11.0, 21.0, 0.0]);
}

#[test]
fn ground_height_is_never_below_sea_level() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCA_GET_TILE, 0)
        .answer(EE_UNITS_UCA_CLOSEST_TILE, 5)
        .float(EE_UNITS_UCA_TILE_HEIGHT_OF, [-2.0, 0.0, 0.0, 0.0]);
    assert_eq!(ground_height(&mut h, 1, [-1.5, 2.5, 0.0]), 0.0);
    assert_eq!(h.args(EE_UNITS_UCA_GET_TILE), [(0, -2, 2)]);
    assert_eq!(h.args(EE_UNITS_UCA_TILE_HEIGHT_OF), [(5, 0, 0)]);
}

#[test]
fn the_carrier_boards_its_plane_in_order() {
    let mut h = Mock::default().answer(EE_UNITS_UCA_ATC, 7);
    let mut airport = 0;
    landable_op(&mut h, EE_UNITS_ACT_BOARD_CARRIER, 1, 2, 0, &mut airport);
    let ops: Vec<i32> =
        h.ops().into_iter().filter(|&op| op != EE_UNITS_UCA_LOCATION && op != EE_UNITS_UCA_GOAL_STATE).collect();
    assert_eq!(
        ops,
        [
            EE_UNITS_UCA_ATC,
            EE_UNITS_UCA_ATC_REMOVE_PLANE,
            EE_UNITS_UCA_HOME_LOAD,
            EE_UNITS_UCA_SET_HOME_BASE,
            EE_UNITS_UCA_SET_COMMANDABLE,
            EE_UNITS_UCA_SET_HEIGHT
        ]
    );
}

#[test]
fn the_nearest_airport_is_kept_for_the_next_operation() {
    let mut h = Mock::default().answer(EE_UNITS_UCA_FIND_NEAREST_AIRPORT, 44);
    let mut airport = 0;
    assert_eq!(landable_op(&mut h, EE_UNITS_ACT_FIND_NEAREST_AIRPORT, 1, 2, 0, &mut airport), 1);
    assert_eq!(airport, 44);
    landable_op(&mut h, EE_UNITS_ACT_SET_HOME_BASE_FOUND, 1, 2, 0, &mut airport);
    assert_eq!(h.args(EE_UNITS_UCA_SET_HOME_BASE), [(44, 0, 0)]);
}

#[test]
fn a_takeoff_without_a_home_base_finds_its_container_airport() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCA_ATC, 7)
        .sequence(EE_UNITS_UCA_HOME_BASE, &[0, 0])
        .answer(EE_UNITS_UCA_ATC_SLOT_COUNT, 2)
        .answer(EE_UNITS_UCA_CONTAINER, 51)
        .sequence(EE_UNITS_UCA_ATC_SLOT_AIRPORT, &[50, 51, 51]);
    let mut airport = 0;
    landable_op(&mut h, EE_UNITS_ACT_TAKEOFF, 1, 2, 0, &mut airport);
    assert_eq!(h.args(EE_UNITS_UCA_SET_HOME_BASE), [(51, 0, 0)]);
    assert!(h.has(EE_UNITS_UCA_RETURN_TO_AIRPORT));
    assert!(!h.has(EE_UNITS_UCA_SET_STATS_FLIGHT_TIME));
}

#[test]
fn a_takeoff_from_a_runway_sets_flight_ammunition_and_height() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCA_HOME_BASE, 50)
        .answer(EE_UNITS_UCA_AIR_TRAFFIC_TYPE, 3)
        .answer(EE_UNITS_UCA_HOME_FACING, 2)
        .float(EE_UNITS_UCA_HEIGHT_AT, [4.5, 0.0, 0.0, 0.0]);
    let mut airport = 0;
    landable_op(&mut h, EE_UNITS_ACT_TAKEOFF, 1, 2, 0, &mut airport);
    assert_eq!(h.args(EE_UNITS_UCA_TAKE_OFF_RUNWAY), [(0, 1, 0)]);
    assert!(h.has(EE_UNITS_UCA_SET_AMMUNITION));
    assert_eq!(h.args(EE_UNITS_UCA_SET_ANIMATION), [(0, 0xd, 0)]);
    assert_eq!(h.floats_of(EE_UNITS_UCA_SET_HEIGHT)[0][0], 4.5);
}

#[test]
fn a_secondary_target_near_the_first_is_taken() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCA_GOAL_CAST, 8)
        .answer(EE_UNITS_UCA_ATTACK_TARGET, 30)
        .answer(EE_UNITS_UCA_TILE_X_OF, 4)
        .answer(EE_UNITS_UCA_TILE_Y_OF, 5)
        .answer(EE_UNITS_UCA_FIND_TARGET_NEAR, 31)
        .answer(EE_UNITS_UCA_TARGETS_ENEMIES, 1);
    let mut airport = 0;
    landable_op(&mut h, EE_UNITS_ACT_SECONDARY_TARGET, 1, 2, 0, &mut airport);
    assert_eq!(h.args(EE_UNITS_UCA_FIND_TARGET_NEAR), [(0, 4, 5)]);
    assert_eq!(h.floats_of(EE_UNITS_UCA_FIND_TARGET_NEAR)[0][0], 3.0);
    assert_eq!(h.args(EE_UNITS_UCA_SET_ATTACK_TARGET), [(8, 31, 0)]);
    // none near: a move to the target's tile
    let mut h = Mock::default()
        .answer(EE_UNITS_UCA_GOAL_CAST, 8)
        .answer(EE_UNITS_UCA_ATTACK_TARGET, 30)
        .answer(EE_UNITS_UCA_TILE_X_OF, 4)
        .answer(EE_UNITS_UCA_TILE_Y_OF, 5)
        .answer(EE_UNITS_UCA_NEW_LOCATION, 60);
    landable_op(&mut h, EE_UNITS_ACT_SECONDARY_TARGET, 1, 2, 0, &mut airport);
    assert_eq!(h.floats_of(EE_UNITS_UCA_NEW_LOCATION)[0], [4.5, 5.5, 0.0, 0.0]);
    assert_eq!(h.args(EE_UNITS_UCA_SET_GOAL), [(60, 0, 0)]);
    assert!(h.at(EE_UNITS_UCA_START_ACTION) < h.at(EE_UNITS_UCA_ACTION_CHANGED));
}

#[test]
fn the_death_roll_copies_the_yaw_into_the_roll() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCA_AIRPLANE_MODEL, 1)
        .float(EE_UNITS_UCA_ANGULAR_VELOCITY, [0.5, 0.2, 0.1, 0.0]);
    let mut airport = 0;
    landable_op(&mut h, EE_UNITS_ACT_DEATH_ROLL, 1, 2, 0, &mut airport);
    assert_eq!(h.floats_of(EE_UNITS_UCA_COMPUTE_ROLL)[0][0], 0.5);
    assert_eq!(h.floats_of(EE_UNITS_UCA_SET_ANGULAR_VELOCITY)[0], [0.5, 0.2, 0.5, 0.0]);
}

#[test]
fn a_bomb_lands_on_a_target_right_below() {
    // a level plane at height 2 dropping straight onto (0, 0.?, 0): gravity 2 (half 1), speed 0
    let mut h = Mock::default()
        .answer(EE_UNITS_UCA_GOAL_CAST, 8)
        .float(EE_UNITS_UCA_GRAVITY, [2.0, 0.0, 0.0, 0.0])
        .float(EE_UNITS_UCA_STATS_SCALE, [1.0, 0.0, 0.0, 0.0])
        .float(EE_UNITS_UCA_FIND_DROP, [0.0, 0.0, -2.0, 0.0]);
    assert!(bomb_on_target(&mut h, 1, 2));
    // no attack goal: no bomb
    let mut h = Mock::default();
    assert!(!bomb_on_target(&mut h, 1, 2));
    assert!(!h.has(EE_UNITS_UCA_FIND_DROP));
}

#[test]
fn the_carrier_fills_with_its_best_fighter() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCA_PLAYER_ID, 3)
        .sequence(EE_UNITS_UCA_FIGHTER_USABLE, &[0, 1])
        .answer(EE_UNITS_UCA_FIGHTER_CLASS, 77)
        .answer(EE_UNITS_UCA_CLASS_AT, 88)
        .answer(EE_UNITS_UCA_CARGO_COUNT, 1)
        .answer(EE_UNITS_UCA_CAPACITY, 3)
        .answer(EE_UNITS_UCA_CREATE_CARRIED, 90);
    carrier_aircraft(&mut h, 2, -1);
    assert_eq!(h.args(EE_UNITS_UCA_FIGHTER_CLASS), [(0, 3, 1)]);
    assert_eq!(h.args(EE_UNITS_UCA_CLASS_AT), [(0, 77, 0)]);
    assert_eq!(h.count(EE_UNITS_UCA_LOAD_CARGO), 2);
    assert_eq!(h.args(EE_UNITS_UCA_CREATE_CARRIED)[0].2, 3);
}

#[test]
fn a_plane_leaving_flies_off_the_map() {
    let mut h = Mock::default().float(EE_UNITS_UCA_POSITION, [3.0, 4.0, 0.0, 0.0]).answer(EE_UNITS_UCA_CONVERT, 70);
    fly_off_map(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UCA_GET_TILE), [(0, 600, 800)]);
    assert_eq!(h.floats_of(EE_UNITS_UCA_NEW_LOCATION)[0], [600.5, 800.5, 0.0, 0.0]);
    assert_eq!(h.args(-11), [(70, 1, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_START_ACTION), [(70, ACTION_FLY, 0)]);
}

#[test]
fn the_air_animal_lands_two_waypoints_ahead() {
    let mut h = Mock::default().answer(EE_UNITS_UCA_ATC, 7).float(EE_UNITS_UCA_CRUISE, [4.0, 0.0, 0.0, 0.0]);
    assert!(air_animal_return(&mut h, 1, 2));
    let w = h.floats_of(-11);
    assert_eq!(w[0], [0.0, 3.0, 0.0, 0.0]);
    assert_eq!(w[1], [0.0, 7.0, 3.0, 0.0]);
    assert!(h.at(EE_UNITS_UCA_SET_ORIENTATION) < h.at(-10));
    let mut h = Mock::default();
    assert!(air_animal_return(&mut h, 1, 2));
    assert!(!h.has(EE_UNITS_UCA_DROP_PATH));
}

#[test]
fn the_spaceship_dies_without_an_airport() {
    let mut h = Mock::default();
    spaceship_holding(&mut h, 1, 2);
    assert!(h.has(EE_UNITS_UCA_KILL));
    assert!(!h.has(EE_UNITS_UCA_DROP_PATH));
    let mut h = Mock::default().answer(EE_UNITS_UCA_HOME_BASE, 5).answer(EE_UNITS_UCA_HOME_TILE_X, 2);
    spaceship_holding(&mut h, 1, 2);
    assert_eq!(path_to(&h)[0], 3.5);
    assert!(h.has(EE_UNITS_UCA_SET_HEIGHT));
}

#[test]
fn the_helicopter_aims_at_its_target() {
    let mut h = Mock::default().answer(EE_UNITS_UCA_GOAL_KIND, GOAL_ATTACK_UNIT);
    helicopter_path(&mut h, 1, 2);
    assert_eq!(h.args(-12), [(2, 0, 0)]);
    assert_eq!(h.floats_of(EE_UNITS_UCA_LANDING_POINT)[0], [1.0, 0.0, 0.0, 0.0]);
    assert_eq!(h.args(-10), [(2, 0, 0)]);
}

#[test]
fn the_opacity_chain_stops_at_a_part_already_set() {
    let mut h = Mock::default().sequence(EE_UNITS_UCA_NEXT_PART_OF, &[3, 4]);
    h.by_op_a.insert((EE_UNITS_UCA_OPACITY_OF, 4), 0);
    opacity_chain(&mut h, 2, 0.0);
    assert!(!h.has(EE_UNITS_UCA_SET_OPACITY_OF));
    let mut h = Mock::default().sequence(EE_UNITS_UCA_NEXT_PART_OF, &[3, 0]);
    opacity_chain(&mut h, 2, 0.5);
    assert_eq!(h.args(EE_UNITS_UCA_SET_OPACITY_OF), [(2, 0, 0), (3, 0, 0)]);
}
