use super::*;
use crate::units::cu_cargo::Placement;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b), its answer from `answers` keyed by (op, a) or `sequences`.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64)>,
    answers: BTreeMap<(i32, u64), i64>,
    sequences: BTreeMap<(i32, u64), Vec<i64>>,
}

impl Mock {
    fn answer(mut self, op: i32, a: u64, value: i64) -> Self {
        self.answers.insert((op, a), value);
        self
    }
    fn sequence(mut self, op: i32, a: u64, values: &[i64]) -> Self {
        self.sequences.insert((op, a), values.to_vec());
        self
    }
    fn count(&self, op: i32) -> usize {
        self.log.iter().filter(|e| e.0 == op).count()
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
}

impl CuHost for Mock {
    fn cu(&mut self, op: i32, unit: u64, a: u64, b: i64, _c: i64, f: &mut [f32; 4]) -> i64 {
        assert_eq!(unit, 1);
        self.log.push((op, a, b));
        if op == EE_UNITS_CU_RANDOM_POINT_AHEAD {
            f[0] = 3.0;
        }
        if op == EE_UNITS_CU_FLEE_GOAL {
            assert_eq!(f[0], 3.0, "the point ahead is the goal's");
        }
        if let Some(values) = self.sequences.get_mut(&(op, a)) {
            if !values.is_empty() {
                return values.remove(0);
            }
        }
        *self.answers.get(&(op, a)).unwrap_or(&0)
    }
}

impl CuCargoHost for Mock {
    fn find_placement(&mut self, _p: &Placement, _m: &mut [[f32; 4]; 4]) -> bool {
        unreachable!()
    }
}

#[test]
fn taking_off_the_map_walks_the_parts_and_the_viewers() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_PART_NEXT, 1, 2)
        .answer(EE_UNITS_CU_NUM_PLAYERS, 0, 3)
        .answer(EE_UNITS_CU_PLAYER_AT, 0, 70)
        .answer(EE_UNITS_CU_PLAYER_VIEWS, 2, 1)
        .answer(EE_UNITS_CU_HAS_GOAL_STATE, 0, 1);
    take_off_map(&mut h, 1);
    assert_eq!(h.count(EE_UNITS_CU_PART_SET_AWAY), 2);
    // players 1 and 2 of three, for each of the two parts
    assert_eq!(h.count(EE_UNITS_CU_PLAYER_VIEWS), 4);
    assert_eq!(h.count(EE_UNITS_CU_PLAYER_UPDATE_SELECTION), 2, "the second part is viewed");
    assert!(h.log.contains(&(EE_UNITS_CU_START_ACTION, 0, 5000)));
    assert_eq!(h.ops().last(), Some(&EE_UNITS_CU_RELEASE_FIRE_EFFECT));
}

#[test]
fn the_water_state_by_level_or_kind() {
    let state = |h: &Mock| h.log.iter().filter(|e| e.0 == EE_UNITS_CU_SET_WATER_STATE).map(|e| e.2).collect::<Vec<_>>();
    let base = || {
        Mock::default().answer(EE_UNITS_CU_CLASS_DOMAINS, 0, 4).answer(EE_UNITS_CU_TILE, 0, 5).answer(
            EE_UNITS_CU_TILE_TERRAIN,
            5,
            10,
        )
    };
    let mut h = base().answer(EE_UNITS_CU_TILE_LEVEL, 5, 1).answer(EE_UNITS_CU_WATER_KIND, 0, 2);
    update_water_state(&mut h, 1, true);
    assert_eq!(state(&h), [2]);
    let mut h = base().answer(EE_UNITS_CU_WATER_KIND, 0, 2);
    update_water_state(&mut h, 1, true);
    assert_eq!(state(&h), [3]);
    let mut h = base().answer(EE_UNITS_CU_TILE_LEVEL, 5, -1).answer(EE_UNITS_CU_WATER_KIND, 0, 1);
    update_water_state(&mut h, 1, true);
    assert_eq!(state(&h), [0]);
    let mut h = base().answer(EE_UNITS_CU_TILE_TERRAIN, 5, 3);
    update_water_state(&mut h, 1, true);
    assert!(state(&h).is_empty(), "land keeps its state");
    let mut h = base().answer(EE_UNITS_CU_WATER_KIND, 0, 1);
    update_water_state(&mut h, 1, false);
    assert_eq!(state(&h), [2]);
    let mut h = Mock::default();
    update_water_state(&mut h, 1, false);
    assert_eq!(state(&h), [0]);
}

#[test]
fn load_media_copies_the_models_orbitals_and_wheels() {
    let mut h = Mock::default().answer(EE_UNITS_CU_MODEL, 0, 30).answer(EE_UNITS_CU_MODEL_ORBITAL_COUNT, 30, 2).answer(
        EE_UNITS_CU_IS_LAND_BUILDING,
        0,
        1,
    );
    load_media(&mut h, 1, false);
    assert_eq!(h.count(EE_UNITS_CU_LOAD_BUTTONS), 0, "a land building loads no buttons here");
    assert_eq!(h.count(EE_UNITS_CU_NEW_ORBITAL), 2);
    assert_eq!(h.count(EE_UNITS_CU_CLEAR_WHEELS), 1);
    let mut h = Mock::default();
    load_media(&mut h, 1, true);
    assert_eq!(h.ops().last(), Some(&EE_UNITS_CU_PM_SET_FOUNDATION_MODEL));
}

#[test]
fn disabling_an_air_base_and_the_time_warp() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_AIR_TRAFFIC_TYPE, 0, 2)
        .answer(EE_UNITS_CU_IS_AIR_BASE, 0, 1)
        .sequence(EE_UNITS_CU_DISABLED, 0, &[0, 1]);
    set_disabled(&mut h, 1, true, 0x1a);
    assert!(h.log.contains(&(EE_UNITS_CU_ATC_REMOVE_AIRPORT, 0, 2)));
    assert_eq!(h.count(EE_UNITS_CU_TIME_WARP_COLOR), 1);
    let mut h = Mock::default();
    set_disabled(&mut h, 1, false, 0);
    assert_eq!(h.ops(), [EE_UNITS_CU_DISABLED], "no change");
}

#[test]
fn the_camouflage_clock() {
    let base = || {
        Mock::default()
            .answer(EE_UNITS_CU_HAS_CAMOUFLAGE, 0, 1)
            .answer(EE_UNITS_CU_CAMOUFLAGE_SET, 0, 40)
            .answer(EE_UNITS_CU_SET_HAS_TYPE, 40, 1)
            .answer(EE_UNITS_CU_VITAL_BUILDINGS, 0, 1)
            .answer(EE_UNITS_CU_CAMOUFLAGE_TIMING, 0, 77)
    };
    let mut h = base();
    update_camouflage(&mut h, 1, 5000);
    assert!(h.log.contains(&(EE_UNITS_CU_SET_CAMOUFLAGE_TIMER, 0, 77)), "going idle starts the clock");
    let mut h = base().answer(EE_UNITS_CU_VITAL_BUILDINGS, 0, 0);
    update_camouflage(&mut h, 1, 5000);
    assert_eq!(h.count(EE_UNITS_CU_CLEAR_CLOAKED), 1);
    let mut h = Mock::default();
    update_camouflage(&mut h, 1, 5000);
    assert_eq!(h.ops(), [EE_UNITS_CU_HAS_CAMOUFLAGE, EE_UNITS_CU_SET_CAMOUFLAGE_TIMER, EE_UNITS_CU_SET_U32C]);
}

#[test]
fn a_released_resource_calls_its_waiting_gatherer() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_GATHERED, 0, 9)
        .answer(EE_UNITS_CU_GATHERED_VALID, 0, 1)
        .answer(EE_UNITS_CU_HAS_COMMAND, 0, 1)
        .answer(EE_UNITS_CU_WAITING_GATHERER, 0, 8);
    set_gathered_resource(&mut h, 1, 7);
    assert!(h.log.contains(&(crate::units::cu_cargo::EE_UNITS_CU_UNIT_START_ACTION, 8, 8008)));
    assert!(h.log.contains(&(EE_UNITS_CU_DEREFERENCE, 9, 0)));
    assert_eq!(h.ops().last(), Some(&EE_UNITS_CU_ADD_RESOURCE_USER));
}

#[test]
fn play_effect_switches_the_research_media() {
    let mut h = Mock::default().answer(EE_UNITS_CU_CURRENT_RESEARCH, 0, 300).answer(EE_UNITS_CU_WORLD_CLEARING, 0, 1);
    play_effect(&mut h, 1, 100000);
    assert_eq!(h.count(EE_UNITS_CU_UNLOAD_TECHNOLOGY_MEDIA), 1);
    assert_eq!(h.count(EE_UNITS_CU_POST_LOAD_TECHNOLOGY), 0);
    assert!(h.log.contains(&(EE_UNITS_CU_SET_CURRENT_RESEARCH, 0, 100000)));
}

#[test]
fn halt_flee_link_and_clear() {
    let mut h = Mock::default().answer(EE_UNITS_CU_CIRCLES, 0, 1);
    halt(&mut h, 1);
    assert!(h.log.contains(&(EE_UNITS_CU_START_ACTION, 0, 1008)));
    let mut h = Mock::default().answer(EE_UNITS_CU_RANDOM_POINT_AHEAD, 0, 1);
    flee_after_calamity(&mut h, 1, 0);
    assert!(h.log.contains(&(EE_UNITS_CU_START_ACTION, 0, 8000)));
    let mut h = Mock::default().answer(EE_UNITS_CU_PREVIOUS_PART, 0, 4).answer(EE_UNITS_CU_PREVIOUS_NEXT, 0, 1);
    link_part(&mut h, 1, 6);
    assert_eq!(h.count(EE_UNITS_CU_DEREFERENCE), 2);
    assert!(h.log.contains(&(EE_UNITS_CU_SET_PREVIOUS_PART, 6, 0)));
    let mut h =
        Mock::default().sequence(EE_UNITS_CU_FIRST_QUEUE_LEN, 0, &[2, 2, 2]).answer(EE_UNITS_CU_HAS_CLASS, 0, 1);
    clear_goal_queue(&mut h, 1, 0);
    assert_eq!(h.count(EE_UNITS_CU_ABANDON_GOAL), 2);
    assert_eq!(h.ops().last(), Some(&EE_UNITS_CU_REFRESH_ACTIVE_GOAL));
}
