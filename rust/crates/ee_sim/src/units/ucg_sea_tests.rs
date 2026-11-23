use super::*;
use crate::units::uc_mock::Mock;

impl UcgSeaRust for Mock {
    fn seabed(&mut self, unit: u64, at: [f32; 3]) -> f32 {
        self.log.push((-5, unit, at[2] as i64, 0));
        -3.0
    }
}

#[test]
fn only_cattails_find_their_water_tile() {
    let mut h = Mock::default().answer(EE_UNITS_UC_THIS_UNIT_TYPE, 7);
    let mut out: [c_long; 2] = [-1, -1];
    let tile = OutTile { x: &mut out[0], y: &mut out[1] };
    // SAFETY: a local out tile
    unsafe { ambient_location_to_tile(&mut h, 1, [2.5, 3.5, 4.0], tile) };
    assert_eq!(out, [-1, -1]);
    assert!(!h.has(EE_UNITS_UCG_WORLD_LINE));
    let mut h = Mock::default().answer(EE_UNITS_UC_THIS_UNIT_TYPE, UNIT_TYPE_AMB_CATTAILS_1);
    // SAFETY: a local out tile (the mock's line has no direction: the point stays at the origin)
    unsafe { ambient_location_to_tile(&mut h, 1, [2.5, 3.5, 4.0], tile) };
    assert_eq!(out, [0, 0]);
    assert!(h.has(EE_UNITS_UCG_WORLD_LINE));
}

#[test]
fn a_submarine_stays_over_the_sea_floor() {
    let mut h = Mock::default().float(EE_UNITS_UCG_MESH_Z, [-2.0, 0.0, 0.0, 0.0]);
    let mut p = [1.0, 2.0, -5.0];
    submerged_ground_location(&mut h, 1, &mut p);
    assert_eq!(p, [1.0, 2.0, -2.0]);
    assert_eq!(h.floats_of(EE_UNITS_UCG_MESH_Z)[0][..2], [1.0, 2.0]);
    let mut p = [1.0, 2.0, -1.0];
    submerged_ground_location(&mut h, 1, &mut p);
    assert_eq!(p[2], -1.0);
}

#[test]
fn a_submarine_tile_asks_the_ground_under_the_cruise_depth() {
    let mut h = Mock::default()
        .float(EE_UNITS_UCG_CRUISE_ALTITUDE_OF, [1.0, 0.0, 0.0, 0.0])
        .float(EE_UNITS_UCG_GROUND_POINT, [0.0, 0.0, 0.5, 0.0]);
    let mut out: [c_long; 2] = [-1, -1];
    let tile = OutTile { x: &mut out[0], y: &mut out[1] };
    // SAFETY: a local out tile
    unsafe { submerged_location_to_tile(&mut h, 1, [6.5, 7.5, 3.0], tile) };
    assert_eq!(out, [0, 0]);
    assert_eq!(h.ops(), [EE_UNITS_UCG_CRUISE_ALTITUDE_OF, EE_UNITS_UCG_WORLD_LINE, EE_UNITS_UCG_GROUND_POINT]);
}

#[test]
fn a_new_submarine_is_grounded_then_put_on_the_seabed() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCG_BASE_CREATE_AT, 40)
        .answer(EE_UNITS_UC_COMPLEX_OF, 41)
        .float(EE_UNITS_UC_GROUND_LOCATION, [1.0, 2.0, -0.5, 0.0]);
    let mut m = [[0.0f32; 4]; 4];
    m[0][3] = 1.0;
    m[1][3] = 2.0;
    let unit = submerged_create_unit_at(&mut h, 1, &m, -1, true, 9);
    assert_eq!(unit, 40);
    assert_eq!(h.args(EE_UNITS_UCG_BASE_CREATE_AT)[0].1, -1);
    assert_eq!(h.args(EE_UNITS_UCG_BASE_CREATE_AT)[0].2, 9);
    assert_eq!(h.args(-5), [(41, 0, 0)]);
    assert_eq!(h.floats_of(EE_UNITS_UCG_SET_LOCATION_OF)[0][..3], [1.0, 2.0, -3.0]);
    assert_eq!(h.args(EE_UNITS_UCG_SET_LOCATION_OF)[0].0, 41);

    let mut h =
        Mock::default().answer(EE_UNITS_UCG_BASE_CREATE_AT, 40).float(EE_UNITS_UCG_TERRAIN_Z, [-4.0, 0.0, 0.0, 0.0]);
    submerged_create_unit(&mut h, 1, 3, 4, 2, false, 0);
    assert_eq!(h.args(EE_UNITS_UCG_TERRAIN_Z), [(0, 3, 4)]);
    assert_eq!(h.args(EE_UNITS_UCG_BASE_CREATE_AT)[0].1, 4);
    assert!(h.at(EE_UNITS_UC_GROUND_LOCATION) < h.at(EE_UNITS_UCG_BASE_CREATE_AT));
}

#[test]
fn a_submarine_joins_its_tile_as_its_complex_unit() {
    let mut h = Mock::default().answer(EE_UNITS_UC_COMPLEX_OF, 8).answer(EE_UNITS_UC_GET_TILE, 5);
    submerged_add_to_tile(&mut h, 1, 7);
    assert_eq!(h.args(EE_UNITS_UCG_TILE_ADD), [(5, 8, 0)]);
    let mut h = Mock::default().answer(EE_UNITS_UC_COMPLEX_OF, 8);
    submerged_add_to_tile(&mut h, 1, 7);
    assert!(!h.has(EE_UNITS_UCG_TILE_ADD));
}

#[test]
fn a_submarine_arrives_when_slow_or_sinking() {
    let mut h = Mock::default().answer(EE_UNITS_UCG_COLLISION_STATE, 8);
    assert!(submerged_has_arrived(&mut h, 1, 2, 9.0));
    let mut h = Mock::default().answer(EE_UNITS_UC_PRODUCER_TILE_X, 3);
    assert!(submerged_has_arrived(&mut h, 1, 2, 0.05));
    assert!(!submerged_has_arrived(&mut h, 1, 2, 0.06));
    let mut h = Mock::default()
        .answer(EE_UNITS_UCG_WAYPOINT_COUNT, 2)
        .float(EE_UNITS_UCG_COLLISION_RADIUS, [1.0, 0.0, 0.0, 0.0]);
    assert!(submerged_has_arrived(&mut h, 1, 2, 1.0));
    assert!(!submerged_has_arrived(&mut h, 1, 2, 1.5));
}

#[test]
fn a_fish_path_stays_between_the_surface_and_the_floor() {
    let mut h = Mock::default()
        .float(EE_UNITS_UCG_MESH_Z, [-2.0, 0.0, 0.0, 0.0])
        .float(EE_UNITS_UCG_RANDOM_RANGE, [0.4, 0.0, 0.0, 0.0]);
    let mut path = [[1.0, 1.0, 0.0], [2.0, 2.0, 0.0]];
    sea_animal_adjust_path(&mut h, 1, 2, &mut path);
    // from depth 0: target min(-0.6, -0.8) = -0.8, step 0.4 down: -0.4, capped at -0.6
    assert_eq!(path[1][2], -0.6);
    assert_eq!(path[0][2], -1.0);
    assert!(!h.has(EE_UNITS_UCG_RANDOM_GENERATE));
    assert_eq!(h.count(EE_UNITS_UCG_RANDOM_RANGE), 4);
}

#[test]
fn a_dolphin_may_dive_after_its_first_point() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_THIS_UNIT_TYPE, UNIT_TYPE_RES_DOLPHIN)
        .float(EE_UNITS_UCG_MESH_Z, [-10.0, 0.0, 0.0, 0.0])
        .float(EE_UNITS_UCG_RANDOM_GENERATE, [0.1, 0.0, 0.0, 0.0])
        .float(EE_UNITS_UCG_RANDOM_RANGE, [0.5, 0.0, 0.0, 0.0]);
    let mut path = [[1.0, 1.0, 0.0], [2.0, 2.0, 0.0], [3.0, 3.0, 0.0]];
    sea_animal_adjust_path(&mut h, 1, 2, &mut path);
    // the last point never dives (surfaced), the middle one draws, the first never draws
    assert_eq!(h.count(EE_UNITS_UCG_RANDOM_GENERATE), 1);
    assert_eq!(h.floats_of(EE_UNITS_UCG_RANDOM_RANGE)[2][..2], [0.45, 0.85]);
    assert_eq!(h.floats_of(EE_UNITS_UCG_RANDOM_RANGE)[1][..2], [0.2, 1.2]);
}

#[test]
fn each_wheel_updates() {
    let mut h = Mock::default().answer(EE_UNITS_UCG_WHEEL_COUNT, 3);
    update_wheels(&mut h, 2);
    assert_eq!(h.args(EE_UNITS_UCG_WHEEL_UPDATE), [(0, 0, 0), (0, 1, 0), (0, 2, 0)]);
}

#[test]
fn the_water_and_cruise_tiles() {
    let mut h = Mock::default().float(EE_UNITS_UCG_CRUISE_ALTITUDE_OF, [3.0, 0.0, 0.0, 0.0]);
    let mut out: [c_long; 2] = [-1, -1];
    let tile = OutTile { x: &mut out[0], y: &mut out[1] };
    // SAFETY: a local out tile
    unsafe { location_to_tile_at(&mut h, 1, [6.5, 7.5, 3.0], tile, true) };
    assert!(h.has(EE_UNITS_UCG_CRUISE_ALTITUDE_OF));
    let mut h = Mock::default();
    // SAFETY: a local out tile
    unsafe { location_to_tile_at(&mut h, 1, [6.5, 7.5, 3.0], tile, false) };
    assert!(!h.has(EE_UNITS_UCG_CRUISE_ALTITUDE_OF));
    assert_eq!(out, [0, 0]);
}

#[test]
fn water_units_are_made_on_the_water_plane() {
    let mut h = Mock::default().answer(EE_UNITS_UCG_BASE_CREATE_AT, 40);
    assert_eq!(naval_citizen_create_unit(&mut h, 1, 3, 4, 2, true, 9), 40);
    assert_eq!(h.args(EE_UNITS_UCG_BASE_CREATE_AT)[0].1, 5);
    let mut m = [[0.0f32; 4]; 4];
    m[2][3] = 5.0;
    assert_eq!(naval_priest_create_unit_at(&mut h, 1, &m, 2, false, 9), 40);
}

#[test]
fn docks_sit_on_the_water_and_buildings_just_above_the_ground() {
    let mut p = [1.0f32, 2.0, 3.0];
    let mut h = Mock::default().answer(EE_UNITS_UC_THIS_UNIT_TYPE, UNIT_TYPE_DOCK);
    building_ground_location(&mut h, 1, &mut p);
    assert_eq!(p[2], 0.0);
    let mut h = Mock::default();
    building_ground_location(&mut h, 1, &mut p);
    assert_eq!(p[2], BUILDING_LIFT);
}
