use super::*;
use crate::units::uc_mock::Mock;

impl UcgRust for Mock {
    fn path_toward(&mut self, unit: u64, _from: [f32; 3], _to: [f32; 3], snap: bool) {
        self.log.push((-3, unit, i64::from(snap), 0));
    }
}

#[test]
fn a_footprint_is_walked_from_its_own_tile() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_TILE_SIZE, 2)
        .answer(EE_UNITS_UCG_TILE_X_OF, 5)
        .answer(EE_UNITS_UCG_TILE_Y_OF, 6)
        .answer(EE_UNITS_UC_GET_TILE, 9);
    ambient_tiles(&mut h, 1, 2, true);
    let tiles: Vec<(i64, i64)> = h.args(EE_UNITS_UC_GET_TILE).iter().map(|e| (e.1, e.2)).collect();
    assert_eq!(tiles, [(5, 6), (5, 7), (6, 6), (6, 7)]);
    assert_eq!(h.count(EE_UNITS_UCG_TILE_ADD), 4);
    let mut h = Mock::default().answer(EE_UNITS_UC_TILE_SIZE, 1).answer(EE_UNITS_UC_GET_TILE, 9);
    ambient_tiles(&mut h, 1, 2, false);
    assert_eq!(h.args(EE_UNITS_UCG_TILE_REMOVE), [(9, 2, 0)]);
}

#[test]
fn a_resource_queued_for_deletion_stays_off_its_tiles() {
    let mut h = Mock::default().answer(EE_UNITS_UC_TILE_SIZE, 1).answer(EE_UNITS_UCG_QUEUED_FOR_DELETE, 1);
    resource_add(&mut h, 1, 2);
    assert!(!h.has(EE_UNITS_UC_GET_TILE));
}

#[test]
fn new_units_take_the_class_resource() {
    let mut h = Mock::default().answer(EE_UNITS_UCG_RESOURCE_TYPE, 2);
    resource_created(&mut h, 1, 7);
    assert_eq!(h.args(EE_UNITS_UCG_SET_RESOURCE_AMOUNT), [(7, 0, 0)]);
    let mut h = Mock::default().answer(EE_UNITS_UCG_UNIT_TYPE, UNIT_TYPE_RADIO_MAN);
    land_created(&mut h, 1, 2);
    assert!(!h.has(EE_UNITS_UCG_SET_RESOURCE_AMOUNT));
    assert!(h.has(EE_UNITS_UCG_SET_CLASS_POWER));
    assert!(h.at(EE_UNITS_UC_CLASS_ACTION_CHANGED) < h.at(EE_UNITS_UCG_SET_RESOURCE_TYPE));
}

#[test]
fn the_paratroop_plane_comes_in_beyond_the_map() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCG_TILE_Y_OF, 3)
        .sequence(EE_UNITS_UCG_MAP_TILE, &[1, 1, 0])
        .answer(EE_UNITS_UCG_CREATE_PLANE, 40)
        .answer(EE_UNITS_UCG_NEW_UNLOAD_TRANSPORT, 41)
        .float(EE_UNITS_UC_GOAL_LOCATION, [7.5, 8.5, 0.0, 0.0]);
    call_paratroop_plane(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UCG_NEW_UNLOAD_TRANSPORT), [(40, 7, 8)]);
    assert_eq!(h.args(EE_UNITS_UC_QUEUE_GOAL_ON), [(40, 41, 0)]);
    assert_eq!(h.args(-3), [(40, 1, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_START_ACTION), [(40, ACTION_FLY, 0)]);
}

#[test]
fn a_new_ship_sits_at_sea_level() {
    let mut h = Mock::default();
    naval_created(&mut h, 1, 2);
    assert!(h.at(EE_UNITS_UC_LOCATION_MATRIX) < h.at(EE_UNITS_UC_SET_HEAL_SEARCH_TIMER));
    assert!(h.at(EE_UNITS_UCG_SET_TRANSFORMATION) > h.at(EE_UNITS_UC_CLASS_ACTION_CHANGED));
}

#[test]
fn a_ship_arrives_slow_or_at_its_last_tile() {
    let mut h = Mock::default();
    assert!(naval_arrived(&mut h, 1, 2, 0.01));
    let mut h = Mock::default()
        .answer(EE_UNITS_UCG_WAYPOINT_COUNT, 2)
        .float(EE_UNITS_UCG_NEXT_WAYPOINT, [0.5, 0.5, 3.0, 0.0])
        .float(EE_UNITS_UCG_COLLISION_RADIUS, [1.0, 0.0, 0.0, 0.0]);
    assert!(naval_arrived(&mut h, 1, 2, 0.5));
    assert!(!naval_arrived(&mut h, 1, 2, 2.0));
    let mut h = Mock::default().float(EE_UNITS_UCG_NEXT_WAYPOINT, [0.5, 0.5, 3.0, 0.0]);
    assert!(!naval_arrived(&mut h, 1, 2, 0.5));
}

#[test]
fn a_fish_rots_by_its_command() {
    let mut h = Mock::default().answer(EE_UNITS_UC_COMMAND_KIND, ACTION_RESOURCES_ROTTING);
    update_resource_unit(&mut h, 1, 2);
    assert_eq!(h.ops(), [EE_UNITS_UC_UPDATE_ORBITALS, EE_UNITS_UC_COMMAND_KIND, EE_UNITS_UCG_RESOURCES_ROTTING]);
}
