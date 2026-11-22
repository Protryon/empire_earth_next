use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c), its answer from `answers` keyed by (op, a, b) (0 else);
/// `sequences` answer each ask of (op, a, b) in turn.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64, i64)>,
    answers: BTreeMap<(i32, u64, i64), i64>,
    sequences: BTreeMap<(i32, u64, i64), Vec<i64>>,
}

impl Mock {
    fn answer(mut self, op: i32, a: u64, b: i64, value: i64) -> Self {
        self.answers.insert((op, a, b), value);
        self
    }
    fn sequence(mut self, op: i32, a: u64, b: i64, values: &[i64]) -> Self {
        self.sequences.insert((op, a, b), values.to_vec());
        self
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
    fn count(&self, op: i32) -> usize {
        self.log.iter().filter(|e| e.0 == op).count()
    }
    fn at(&self, op: i32) -> usize {
        self.log.iter().position(|e| e.0 == op).unwrap()
    }
}

impl EuHost for Mock {
    fn eu(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64) -> i64 {
        assert_eq!(unit, 1);
        self.log.push((op, a, b, c));
        if let Some(values) = self.sequences.get_mut(&(op, a, b)) {
            if !values.is_empty() {
                return values.remove(0);
            }
        }
        *self.answers.get(&(op, a, b)).unwrap_or(&0)
    }
}

#[test]
fn a_move_within_a_region_keeps_the_region() {
    let mut h = Mock::default()
        .answer(EE_UNITS_EU_TILE_X, 0, 0, 10)
        .answer(EE_UNITS_EU_TILE_Y, 0, 0, 10)
        .answer(EE_UNITS_EU_TILE, 0, 0, 77)
        .answer(EE_UNITS_EU_CLASS_TILE_SIZE, 0, 0, 1)
        .answer(EE_UNITS_EU_REGIONS_SIZE, 0, 0, 8)
        .answer(EE_UNITS_EU_HAS_MAP, 0, 0, 1)
        .answer(EE_UNITS_EU_MAP_TILE_EXISTS, 0, 11, 1)
        .answer(EE_UNITS_EU_HAS_INFLUENCE, 0, 0, 1);
    move_to(&mut h, 1, 11, 10, true);
    assert_eq!(h.count(EE_UNITS_EU_REMOVE_FROM_REGION), 0);
    assert_eq!(h.count(EE_UNITS_EU_ADD_TO_REGION), 0);
    assert_eq!(h.count(EE_UNITS_EU_REMOVE_FROM_TILE), 1);
    assert_eq!(h.count(EE_UNITS_EU_ADD_TO_TILE), 1);
    assert!(h.at(EE_UNITS_EU_SET_TILE_Y) < h.at(EE_UNITS_EU_SET_TILE_X));
    assert!(h.at(EE_UNITS_EU_SIGHT_MOVED) < h.at(EE_UNITS_EU_SET_TILE_X));
    assert_eq!(h.ops().last(), Some(&EE_UNITS_EU_RESORT));
}

#[test]
fn a_move_into_another_region_updates_the_influence() {
    let mut h = Mock::default()
        .answer(EE_UNITS_EU_TILE_X, 0, 0, 7)
        .answer(EE_UNITS_EU_TILE, 0, 0, 77)
        .answer(EE_UNITS_EU_CLASS_TILE_SIZE, 0, 0, 1)
        .answer(EE_UNITS_EU_REGIONS_SIZE, 0, 0, 8)
        .answer(EE_UNITS_EU_HAS_MAP, 0, 0, 1)
        .answer(EE_UNITS_EU_HAS_INFLUENCE, 0, 0, 1);
    move_to(&mut h, 1, 8, 0, true);
    assert_eq!(h.count(EE_UNITS_EU_INFLUENCE_REMOVE), 1);
    assert!(h.log.contains(&(EE_UNITS_EU_INFLUENCE_ADD, 0, 8, 0)));
    // the same tile: nothing
    let mut h = Mock::default().answer(EE_UNITS_EU_TILE_X, 0, 0, 3).answer(EE_UNITS_EU_TILE_Y, 0, 0, 4);
    move_to(&mut h, 1, 3, 4, true);
    assert_eq!(h.count(EE_UNITS_EU_SET_TILE_X), 0);
    // without tile updates: the tile only
    let mut h = Mock::default().answer(EE_UNITS_EU_HAS_MAP, 0, 0, 1);
    move_to(&mut h, 1, 3, 4, false);
    assert_eq!(h.count(EE_UNITS_EU_SIGHT_MOVED), 0);
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_EU_SET_TILE);
}

#[test]
fn a_simple_units_death_plays_its_effects_and_reports_it() {
    let mut h = Mock::default()
        .answer(EE_UNITS_EU_HIT_POINTS, 0, 0, 5)
        .answer(EE_UNITS_EU_HAS_DEATH_EFFECT, 0, 0, 1)
        .answer(EE_UNITS_EU_HAS_DEATH_SOUND, 0, 0, 1)
        .answer(EE_UNITS_EU_UNIT_TYPE, 0, 0, UNIT_TYPE_FARM)
        .answer(EE_UNITS_EU_CAMERA_UNIT_IS_THIS, 0, 1, 1);
    kill(&mut h, 1, true, false, true);
    assert_eq!(h.count(EE_UNITS_EU_DEATH_EFFECT), 1);
    assert!(h.at(EE_UNITS_EU_DEATH_SOUND_PARAMETERS) < h.at(EE_UNITS_EU_DEATH_SOUND_PLAY));
    assert!(h.log.contains(&(EE_UNITS_EU_SET_RESOURCE_AMOUNT, 0, 0, 0)));
    assert_eq!(h.count(EE_UNITS_EU_CLEAR_CAMERA_UNIT_B), 1);
    assert_eq!(h.count(EE_UNITS_EU_CLEAR_CAMERA_UNIT), 0);
    assert_eq!(h.log.last(), Some(&(EE_UNITS_EU_UNIT_TAKEN, 0, 1, 0)));
    // dead already: nothing more
    let mut h = Mock::default();
    kill(&mut h, 1, true, false, false);
    assert_eq!(h.ops(), [EE_UNITS_EU_TILE, EE_UNITS_EU_HIT_POINTS]);
}

#[test]
fn a_citizen_gathers_its_own_repaired_farm_only() {
    let farm = 9;
    let base = || {
        Mock::default()
            .answer(EE_UNITS_EU_CARRIES, 0, 0, 1)
            .answer(EE_UNITS_EU_RESOURCE_AMOUNT_OF, farm, 0, 10)
            .answer(EE_UNITS_EU_RESOURCE_TYPE_OF, farm, 0, 2)
            .answer(EE_UNITS_EU_CATEGORY_OF, 0, 0, CATEGORY_LAND_CITIZEN)
            .answer(EE_UNITS_EU_UNIT_TYPE_OF, farm, 0, UNIT_TYPE_FARM)
            .answer(EE_UNITS_EU_SAME_PLAYER, farm, 0, 1)
            .answer(EE_UNITS_EU_IS_BUILT_OF, farm, 0, 1)
    };
    assert!(!can_gather(&mut base(), 1, farm, 0, 0, true, false));
    assert!(can_gather(&mut base().answer(EE_UNITS_EU_FULL_HEALTH_OF, farm, 0, 1), 1, farm, 0, 0, true, false));
    // not visible: its tile asked
    let mut h = base().answer(EE_UNITS_EU_HAS_MAP, 0, 0, 1);
    assert!(!can_gather(&mut h, 1, farm, 4, 5, false, false));
    assert_eq!(h.count(EE_UNITS_EU_MAP_TILE_EXISTS), 1);
}

#[test]
fn a_citizen_gathers_a_dead_animal_or_an_enemy() {
    let other = 9;
    let base = || {
        Mock::default()
            .answer(EE_UNITS_EU_CARRIES, 0, 0, 1)
            .answer(EE_UNITS_EU_RESOURCE_AMOUNT_OF, other, 0, 10)
            .answer(EE_UNITS_EU_RESOURCE_TYPE_OF, other, 0, 3)
            .answer(EE_UNITS_EU_CATEGORY_OF, 0, 0, CATEGORY_LAND_CITIZEN)
            .answer(EE_UNITS_EU_HIT_POINTS_OF, other, 0, 4)
    };
    // a living friend: no
    assert!(!can_gather(&mut base().answer(EE_UNITS_EU_SAME_PLAYER, other, 0, 1), 1, other, 0, 0, true, false));
    // an enemy: yes
    assert!(can_gather(&mut base().answer(EE_UNITS_EU_ENEMY_OF, other, 0, 1), 1, other, 0, 0, true, false));
    // on deep water: no
    assert!(!can_gather(
        &mut base().answer(EE_UNITS_EU_TERRAIN_OF, other, 0, TERRAIN_DEEP_WATER),
        1,
        other,
        0,
        0,
        true,
        false
    ));
}

#[test]
fn the_nearest_footprint_tile_walks_the_edge() {
    // a 3-tile unit at (10, 10); this unit at (14, 11): the nearest edge tile is (12, 11), distance 4
    let other = 9;
    let mut h = Mock::default()
        .answer(EE_UNITS_EU_TILE_DISTANCE_SQUARED, other, 0, i64::from(17.0f32.to_bits()))
        .answer(EE_UNITS_EU_TILE_SIZE_OF, other, 0, 3)
        .answer(EE_UNITS_EU_TILE_X, other, 0, 10)
        .answer(EE_UNITS_EU_TILE_Y, other, 0, 10)
        .answer(EE_UNITS_EU_TILE_X, 0, 0, 14)
        .answer(EE_UNITS_EU_TILE_Y, 0, 0, 11);
    assert_eq!(nearest_footprint_tile(&mut h, 1, other), (4.0, 12, 11));
    // a one-tile unit: its tile and the plain distance
    let mut h = Mock::default().answer(EE_UNITS_EU_TILE_DISTANCE_SQUARED, other, 0, i64::from(2.0f32.to_bits()));
    assert_eq!(nearest_footprint_tile(&mut h, 1, other), (2.0, 0, 0));
}

#[test]
fn the_area_tile_of_a_large_unit_is_found_around_it() {
    let mut h = Mock::default()
        .answer(EE_UNITS_EU_CLASS_TILE_SIZE, 0, 0, 2)
        .answer(EE_UNITS_EU_TILE_X, 0, 0, 5)
        .answer(EE_UNITS_EU_TILE_Y, 0, 0, 5)
        .answer(EE_UNITS_EU_MAP_SIZE, 0, 0, 50)
        .answer(EE_UNITS_EU_MAP_SIZE, 0, 1, 50)
        .sequence(EE_UNITS_EU_CONTINENT_ID, 0, 4, &[0, 3])
        .answer(EE_UNITS_EU_CONTINENT_BY_ID, 0, 3, 80);
    assert_eq!(find_area_tile(&mut h, 1, false, 1), (4, 5));
}

#[test]
fn hit_points_set_and_an_exhausted_resource_leaves() {
    let mut h = Mock::default().answer(EE_UNITS_EU_HIT_POINTS, 0, 0, 5);
    set_hit_points(&mut h, 1, 5);
    assert_eq!(h.count(EE_UNITS_EU_SET_HIT_POINTS), 0);
    let mut h = Mock::default().answer(EE_UNITS_EU_CATEGORY, 0, 0, CATEGORY_RESOURCE);
    deplete_resource(&mut h, 1, 10);
    assert_eq!(
        h.ops(),
        [
            EE_UNITS_EU_TAKE_RESOURCE,
            EE_UNITS_EU_RESOURCE_AMOUNT,
            EE_UNITS_EU_SET_RESOURCE_AMOUNT,
            EE_UNITS_EU_CATEGORY,
            EE_UNITS_EU_CLOAKED_ENEMY,
            EE_UNITS_EU_MINIMAP_MOVE,
            EE_UNITS_EU_UNIT_TAKEN,
            EE_UNITS_EU_KILL_UNIT
        ]
    );
}
