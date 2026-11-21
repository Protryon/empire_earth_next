use super::*;
use crate::units::uc_mock::Mock;

impl UcxRust for Mock {
    fn landing_point(&mut self, unit: u64, target: &mut [f32; 3]) {
        self.log.push((-4, unit, 0, 0));
        target[2] = 9.0;
    }
}

#[test]
fn a_cargo_that_cannot_move_leaves_by_a_tile_around() {
    let mut h = Mock::default().answer(EE_UNITS_UCX_PICK_TILE_NEAR, 1);
    assert!(boarding_location(&mut h, 1, 2, 3, [4.5, 5.5, 0.0], 77));
    assert_eq!(h.args(EE_UNITS_UCX_PICK_TILE_NEAR), [(3, 77, 0)]);
    assert!(h.placements.is_empty());
}

#[test]
fn a_moving_cargo_takes_a_free_placement_or_the_location() {
    let mut h = Mock::default()
        .float(EE_UNITS_UCX_MAX_VELOCITY_OF, [1.0, 0.0, 0.0, 0.0])
        .answer(EE_UNITS_UCX_TILE_SIZE_OF, 1)
        .answer(EE_UNITS_UCX_DOMAINS_OF, 4);
    h.placement = true;
    assert!(boarding_location(&mut h, 1, 2, 3, [4.5, 5.5, 0.0], 77));
    assert_eq!((h.placements[0].x, h.placements[0].y), (4, 5));
    assert_eq!(h.placements[0].flags, [false, true, false, true]);
    // the mock's placement leaves z at -1
    assert_eq!(h.floats_of(EE_UNITS_UCX_SET_POINT)[0][2], -1.0);
    let mut h = Mock::default().float(EE_UNITS_UCX_MAX_VELOCITY_OF, [1.0, 0.0, 0.0, 0.0]);
    assert!(!boarding_location(&mut h, 1, 2, 3, [4.5, 5.5, 0.25], 77));
    assert_eq!(h.floats_of(EE_UNITS_UCX_SET_POINT)[0][..3], [4.5, 5.5, 0.25]);
}

#[test]
fn siege_towers_unload_with_the_tower_flag() {
    let mut h = Mock::default().answer(EE_UNITS_UC_CLASS_UNIT_TYPE, UNIT_TYPE_SIEGE06_HEAVY_TOWER);
    unload_at(&mut h, 1, 2, 3, 4);
    assert_eq!(h.args(EE_UNITS_UCX_UNLOAD_AT_TILE), [(1, 3, 4)]);
}

#[test]
fn a_badly_hurt_transport_unloads_everyone() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCX_COMPLEX_FIELD_OF, 9)
        .answer(EE_UNITS_UCX_CARGO_COUNT_OF, 3)
        .sequence(EE_UNITS_UCX_CARGO_OF, &[11, 0, 13]);
    badly_hurt(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UCX_UNLOAD_NOW_OF), [(9, 11, 0), (9, 13, 0)]);
}

#[test]
fn a_transport_helicopter_lifts_off_after_its_set_up() {
    let mut h = Mock::default().float(EE_UNITS_UCX_HEIGHT_AT, [6.0, 0.0, 0.0, 0.0]);
    helicopter_created(&mut h, 1, 2);
    assert!(h.at(EE_UNITS_UC_LOCATION_MATRIX) < h.at(EE_UNITS_UC_SET_HEAL_SEARCH_TIMER));
    assert!(h.at(EE_UNITS_UCX_SET_RESOURCE_TYPE) < h.at(EE_UNITS_UCX_HEIGHT_AT));
    assert_eq!(h.floats_of(EE_UNITS_UC_SET_HEIGHT_OF)[0][0], 6.0);
    let mut h = Mock::default();
    space_created(&mut h, 1, 2);
    assert!(h.at(EE_UNITS_UCX_SET_TRANSFORMATION) < h.at(EE_UNITS_UCX_SET_RESOURCE_TYPE));
}

#[test]
fn the_helicopter_paths_to_its_landing_point() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_GOAL_STATE_SELF, 5)
        .answer(EE_UNITS_UC_GOAL_KIND, GOAL_EXPLORE)
        .float(EE_UNITS_UCX_CRUISE_ALTITUDE, [3.0, 0.0, 0.0, 0.0]);
    helicopter_path(&mut h, 1, 2);
    assert!(h.has(EE_UNITS_UCX_CRUISE_ALTITUDE));
    assert_eq!(h.args(-4), [(2, 0, 0)]);
    assert_eq!(h.args(-3), [(2, 0, 0)]);
}

#[test]
fn a_space_transport_sits_on_its_own_tile() {
    let mut h = Mock::default().answer(EE_UNITS_UCX_COMPLEX_FIELD_OF, 8).answer(EE_UNITS_UC_GET_TILE, 4);
    space_tile(&mut h, 1, 2, true);
    space_tile(&mut h, 1, 2, false);
    assert_eq!(h.args(EE_UNITS_UCX_TILE_ADD), [(4, 8, 0)]);
    assert_eq!(h.args(EE_UNITS_UCX_TILE_REMOVE), [(4, 8, 0)]);
}

#[test]
fn a_fortress_has_room_within_the_limit() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCX_TOTAL_POPULATION, 48)
        .answer(EE_UNITS_UC_CLASS_POPULATION_COST, 2)
        .answer(EE_UNITS_UC_POPULATION_LIMIT, 50);
    assert!(fortress_has_room(&mut h, 1, 2, 3));
    let mut h = Mock::default().answer(EE_UNITS_UCX_TOTAL_POPULATION, 49).answer(EE_UNITS_UC_CLASS_POPULATION_COST, 2);
    assert!(!fortress_has_room(&mut h, 1, 2, 3));
    let mut h = Mock::default();
    fortress_created(&mut h, 1, 2);
    assert_eq!(h.ops()[0], EE_UNITS_UCX_SET_BUILT);
    assert_eq!(h.args(EE_UNITS_UC_SET_REGENERATION_TIMER), [(0, 1000, 0)]);
}

#[test]
fn a_teleporter_spends_faith_by_the_cargo_unloaded() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCX_TYPE_OF, UNIT_TYPE_TELEPORTER)
        .answer(EE_UNITS_UCX_UNLOAD_COST, 100)
        .answer(EE_UNITS_UCX_POWER, 250)
        .answer(EE_UNITS_UCX_TRANSPORT_CAPACITY, 8)
        .sequence(EE_UNITS_UC_CARGO_ROOM, &[2, 6]);
    fortress_unload_at(&mut h, 1, 2, 3, 4);
    // 6 aboard, then 2: 4 / 8 of 100 faith
    assert_eq!(h.args(EE_UNITS_UCX_SET_POWER), [(0, 200, 0)]);
    let mut h =
        Mock::default().answer(EE_UNITS_UCX_TYPE_OF, UNIT_TYPE_TELEPORTER).answer(EE_UNITS_UCX_UNLOAD_COST, 100);
    fortress_unload_at(&mut h, 1, 2, 3, 4);
    assert_eq!(h.args(EE_UNITS_UCX_UNLOAD_EVENT), [(0, 3, 4)]);
    assert!(!h.has(EE_UNITS_UCX_UNLOAD_AT_TILE));
    let mut h = Mock::default();
    fortress_unload_at(&mut h, 1, 2, 3, 4);
    assert_eq!(h.args(EE_UNITS_UCX_UNLOAD_AT_TILE), [(0, 3, 4)]);
}
