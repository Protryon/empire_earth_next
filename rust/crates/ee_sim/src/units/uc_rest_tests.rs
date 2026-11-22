use super::*;
use crate::units::uc_mock::Mock;

impl UcRestRust for Mock {
    fn path_toward(&mut self, unit: u64, _from: [f32; 3], to: [f32; 3], snap: bool) {
        self.log.push((-3, unit, to[2] as i64, i64::from(snap)));
    }
    fn aim_point(&mut self, unit: u64, _target: [f32; 3]) -> [f32; 3] {
        self.log.push((-4, unit, 0, 0));
        [1.0, 2.0, 3.0]
    }
    fn units_near(&mut self, _center: [f32; 3], radius: f32) -> Vec<u64> {
        self.log.push((-5, 0, radius as i64, 0));
        vec![11, 12, 13]
    }
    fn block_ring(&mut self, corners: i32, ring: [i32; 4], tiles: u64, append: bool) {
        self.rings.push((corners, ring, tiles, append));
    }
}

#[test]
fn a_helicopter_priest_aims_at_its_attack_and_lands() {
    let mut h = Mock::default().answer(EE_UNITS_UC_GOAL_STATE_SELF, 9).answer(EE_UNITS_UC_GOAL_KIND, GOAL_ATTACK_UNIT);
    helicopter_priest_path(&mut h, 1, 2);
    let ops = h.ops();
    let find = h.at(EE_UNITS_UC_R_FIND_DROP_LOCATION);
    assert_eq!(ops[find + 1], -4);
    assert!(h.at(EE_UNITS_UC_R_LANDING_POINT) < h.at(-3));
    assert_eq!(h.args(-3), [(2, 3, 0)]);
    let mut h = Mock::default().answer(EE_UNITS_UC_GOAL_STATE_SELF, 9).answer(EE_UNITS_UC_GOAL_KIND, GOAL_CAST_CALAMITY);
    helicopter_priest_path(&mut h, 1, 2);
    assert!(h.at(EE_UNITS_UC_R_GOAL_DESTINATION) < h.at(EE_UNITS_UC_R_PRIEST_DESTINATION));
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_GOAL_KIND, GOAL_EXPLORE)
        .float(EE_UNITS_UC_R_CRUISE_ALTITUDE, [7.0, 0.0, 0.0, 0.0]);
    helicopter_priest_path(&mut h, 1, 2);
    assert_eq!(h.floats_of(EE_UNITS_UC_R_LANDING_POINT)[0][2], 7.0);
}

#[test]
fn a_new_helicopter_priest_takes_its_height() {
    let mut h = Mock::default().float(EE_UNITS_UC_R_HEIGHT_AT, [4.5, 0.0, 0.0, 0.0]);
    helicopter_priest_created(&mut h, 1, 2);
    assert!(h.at(EE_UNITS_UC_LOCATION_MATRIX) < h.at(EE_UNITS_UC_CLASS_ACTION_CHANGED));
    assert!(h.at(EE_UNITS_UC_R_HEIGHT_AT) < h.at(EE_UNITS_UC_R_SET_TRANSFORMATION));
    assert_eq!(h.floats_of(EE_UNITS_UC_R_SET_HEIGHT)[0][0], 4.5);
}

#[test]
fn the_next_resource_is_searched_around_the_storage_first() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_GOAL_STATE_SELF, 9)
        .answer(EE_UNITS_UC_GOAL_KIND, GOAL_GATHER)
        .answer(EE_UNITS_UC_R_GATHER_RESOURCE, 30)
        .answer(EE_UNITS_UC_R_CAN_GET_TO_UNIT, 1)
        .float(EE_UNITS_UC_R_LINE_OF_SIGHT, [2.5, 0.0, 0.0, 0.0])
        .sequence(EE_UNITS_UC_R_FIND_RESOURCE_OF_KIND, &[0, 44]);
    assert_eq!(find_next_resource(&mut h, 1, 2, 5, 3, 77), 44);
    let args = h.args(EE_UNITS_UC_R_FIND_RESOURCE_OF_KIND);
    assert_eq!(args, [(30, 5, 77), (2, 5, 77)]);
    let f = h.floats_of(EE_UNITS_UC_R_FIND_RESOURCE_OF_KIND)[0];
    assert_eq!((f[0].to_bits(), f[1].to_bits()), (12, 3));
}

#[test]
fn the_elevation_factor_reads_the_launcher_and_heights() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_R_ELEVATION_LAUNCHER, 1)
        .answer(EE_UNITS_UC_R_ELEVATION_ATTACKER, 1)
        .answer(EE_UNITS_UC_R_ELEVATION_TARGET, 1)
        .float(EE_UNITS_UC_R_HEIGHT, [6.0, 0.0, 0.0, 0.0])
        .float(EE_UNITS_UC_R_ELEVATION, [1.25, 0.0, 0.0, 0.0]);
    assert_eq!(elevation_factor(&mut h, 1, 2, 3), 1.25);
    assert!(h.has(EE_UNITS_UC_R_LAUNCHER_CATEGORY));
    assert!(h.has(EE_UNITS_UC_R_HEIGHT));
    let rows: Vec<i64> = h.args(EE_UNITS_UC_R_WORLD_FLOAT).iter().map(|e| e.1).collect();
    assert_eq!(rows, [53, 54, 55, 56]);
    let mut h = Mock::default();
    elevation_factor(&mut h, 1, 2, 3);
    assert!(!h.has(EE_UNITS_UC_R_WORLD_FLOAT));
    assert!(h.has(EE_UNITS_UC_R_ELEVATION));
}

#[test]
fn a_simple_unit_sounds_at_its_tile_corner() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_R_TILE_X_OF, 4)
        .answer(EE_UNITS_UC_R_TILE_Y_OF, 5)
        .float(EE_UNITS_UC_R_TILE_HEIGHT_OF, [0.5, 0.0, 0.0, 0.0])
        .float(EE_UNITS_UC_R_COMPUTE_SOUND, [0.1, 0.2, 0.3, 0.0]);
    let (mut v, mut p, mut q) = (9.0f32, 9.0f32, 9.0f32);
    let outs = SoundOuts { volume: &mut v, pan: &mut p, pitch: &mut q };
    // SAFETY: local outs
    unsafe { sound(&mut h, EE_UNITS_UC_SOUND_BASE, 1, 2, 0, outs) };
    assert_eq!((v, p, q), (0.1, 0.2, 0.3));
    assert_eq!(h.floats_of(EE_UNITS_UC_R_COMPUTE_SOUND)[0], [4.0, 5.0, 0.5, 1.0]);
    let mut h = Mock::default().answer(EE_UNITS_UC_COMPLEX_OF, 8).answer(EE_UNITS_UC_R_HAS_TILE_NODE, 1);
    // SAFETY: local outs
    unsafe { sound(&mut h, EE_UNITS_UC_SOUND_HELICOPTER_PRIEST, 1, 2, 0, outs) };
    assert!(h.at(EE_UNITS_UC_R_DOPPLER_PITCH) < h.at(EE_UNITS_UC_R_IS_SEEN));
    assert_eq!(v, 0.0);
}

#[test]
fn the_class_table_references_and_releases() {
    let mut h = Mock::default().answer(EE_UNITS_UC_CLASS_UNIT_TYPE, 17);
    register_class(&mut h, 5);
    assert_eq!(h.args(EE_UNITS_UC_R_TABLE_SET), [(5, 17, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_R_TYPES_PUSH), [(0, 17, 0)]);
    let mut h = Mock::default();
    register_class(&mut h, 0);
    assert!(h.log.is_empty());
    let mut h =
        Mock::default().sequence(EE_UNITS_UC_R_CLASS_COUNT, &[2, 2, 2]).sequence(EE_UNITS_UC_R_TABLE_GET, &[0, 6]);
    release_classes(&mut h);
    assert_eq!(h.args(EE_UNITS_UC_R_CLASS_DEREFERENCE), [(6, 0, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_R_TABLE_SET), [(0, 0, 0), (0, 1, 0)]);
}

#[test]
fn small_unit_bodies() {
    let mut h = Mock::default().answer(EE_UNITS_UC_R_CAMOUFLAGE_TIMER, 0x8000_0005);
    set_camouflaged(&mut h, 2, false);
    assert_eq!(h.args(EE_UNITS_UC_R_SET_CAMOUFLAGE_TIMER), [(0, 5, 0)]);
    let mut h = Mock::default().answer(EE_UNITS_UC_R_UNIT_REMOVED, 0);
    unit_removed(&mut h, 1);
    assert!(h.has(EE_UNITS_UC_R_SET_GONE_TIME));
    let mut h = Mock::default().answer(EE_UNITS_UC_R_ACTIVE_GROUP, 3);
    inverse_max_velocity(&mut h, 2);
    assert!(!h.has(EE_UNITS_UC_R_STATS_INVERSE_VELOCITY));
    let mut h = Mock::default().answer(EE_UNITS_UC_R_ORBITAL_COUNT, 2);
    update_orbitals(&mut h, 2);
    assert_eq!(h.count(EE_UNITS_UC_R_ORBITAL_UPDATE), 2);
    let mut h = Mock::default().answer(EE_UNITS_UC_R_RTS_TILE_OF, 4);
    base_tile(&mut h, 1, 2, false);
    assert_eq!(h.args(EE_UNITS_UC_R_RTS_TILE_REMOVE), [(4, 2, 0)]);
    let mut h = Mock::default();
    assert_eq!(distance_3d(&mut h, 2, [1.0, 2.0, 2.0]), 9.0);
}

#[test]
fn the_parts_follow_their_links() {
    let mut h = Mock::default();
    h.by_op_a.insert((EE_UNITS_UC_R_NEXT_PART, 1), 2);
    h.by_op_a.insert((EE_UNITS_UC_R_NEXT_PART, 2), 3);
    h.by_op_a.insert((EE_UNITS_UC_R_PREVIOUS_PART, 3), 2);
    h.by_op_a.insert((EE_UNITS_UC_R_PREVIOUS_PART, 2), 1);
    assert_eq!(last_linked(&mut h, 1), 3);
    assert_eq!(fourth_part(&mut h, 1), 1);
    assert_eq!(second_part(&mut h, 1), 0);
    assert_eq!(first_part(&mut h, 3), 1);
}

#[test]
fn the_volcano_erupts_toward_a_random_point() {
    let mut h = Mock::default().answer(EE_UNITS_UCB_R_VOLCANO_CLASS, 0);
    assert!(erupt_volcano(&mut h, 1, 2));
    assert_eq!(
        h.ops(),
        [
            EE_UNITS_UCB_R_VOLCANO_CLASS,
            EE_UNITS_UCB_R_VOLCANO_VERIFY,
            EE_UNITS_UC_POSITION,
            EE_UNITS_UCB_R_RANDOM_LOCATION,
            EE_UNITS_UCB_R_ATTACK_GROUND
        ]
    );
}

#[test]
fn the_target_near_is_the_last_healable_reachable_unit() {
    let mut h =
        Mock::default().float(EE_UNITS_UC_R_LINE_OF_SIGHT, [6.0, 0.0, 0.0, 0.0]).answer(EE_UNITS_UC_R_CAN_HEAL, 1);
    h.by_op_a.insert((EE_UNITS_UC_R_CAN_REACH, 13), 0);
    h.by_op_a.insert((EE_UNITS_UC_R_CAN_REACH, 12), 1);
    let mut out = 99u64;
    // SAFETY: a local out
    assert!(unsafe { find_target_near(&mut h, 1, 2, 0.0, &mut out) });
    assert_eq!(out, 12);
    assert_eq!(h.args(-5), [(0, 6, 0)]);
}

#[test]
fn the_container_continent_scans_the_tiles_from_the_end() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_R_TILES_SIZE, 3)
        .answer(EE_UNITS_UC_R_CONTINENT_AT_TILE, 7)
        .answer(EE_UNITS_UC_R_MAP_TILE_AT_TILE, 8)
        .sequence(EE_UNITS_UC_R_CONTINENT_IS_LAND, &[0, 1]);
    assert_eq!(container_continent(&mut h, 1, 2, 0), 7);
    let tiles: Vec<i64> = h.args(EE_UNITS_UC_R_CONTINENT_AT_TILE).iter().map(|e| e.1).collect();
    assert_eq!(tiles, [2, 1]);
    assert!(!h.has(EE_UNITS_UC_R_CONTINENT_OF));
    assert_eq!(h.ops()[0], EE_UNITS_UC_R_TILES_CLEAR);
}

#[test]
fn a_free_tile_is_no_cliff_water_space_or_edge() {
    let mut h = Mock::default().answer(EE_UNITS_UC_GET_TILE, 4).answer(EE_UNITS_UC_R_TERRAIN_OF, 3);
    assert!(tile_free(&mut h, 1, 2));
    let mut h = Mock::default().answer(EE_UNITS_UC_GET_TILE, 4).answer(EE_UNITS_UC_R_TERRAIN_OF, TERRAIN_SPACE);
    assert!(!tile_free(&mut h, 1, 2));
    assert!(!h.has(EE_UNITS_UC_R_EDGE_TILE));
}

#[test]
fn a_unit_faces_its_target_tile_centre_when_simple() {
    let mut h = Mock::default().answer(EE_UNITS_UC_R_TILE_X_OF, 3).answer(EE_UNITS_UC_R_TILE_Y_OF, 4).answer(EE_UNITS_UC_R_FACES_POINT, 1);
    assert!(faces_target(&mut h, 1, 2, 9, true));
    assert_eq!(h.floats_of(EE_UNITS_UC_R_FACES_POINT)[0], [3.5, 4.5, 0.0, 0.0]);
    assert_eq!(h.args(EE_UNITS_UC_R_FACES_POINT), [(0, 1, 0)]);
}

#[test]
fn the_base_ring_empties_the_list_unless_appending() {
    let mut h = Mock::default().answer(EE_UNITS_UC_R_VEC_TILES_SIZE, 2).answer(EE_UNITS_UC_TILE_SIZE, 3);
    base_surrounding(&mut h, 1, 2, 77, 1, false);
    assert!(h.has(EE_UNITS_UC_R_VEC_TILES_CLEAR));
    assert_eq!(h.rings, [(1, [0, 0, 3, 3], 77, false)]);
    let mut h = Mock::default().answer(EE_UNITS_UC_R_VEC_TILES_SIZE, 2);
    base_surrounding(&mut h, 1, 2, 77, 0, true);
    assert!(!h.has(EE_UNITS_UC_R_VEC_TILES_SIZE));
}

#[test]
fn an_upgrade_copies_its_class_then_reapplies() {
    let mut h = Mock::default();
    upgrade_init(&mut h, 5);
    assert_eq!(h.count(EE_UNITS_UC_R_UPGRADE_ACKNOWLEDGE), 2);
    assert!(h.at(EE_UNITS_UC_R_UPGRADE_RANGE) < h.at(EE_UNITS_UC_R_UPGRADE_INIT));
    assert!(h.at(EE_UNITS_UC_R_UPGRADE_REAPPLY) < h.at(EE_UNITS_UC_R_UPGRADE_INIT_FINISH));
}
