use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c), its answer from `answers` keyed by (op, a) (0 else).
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64, i64)>,
    answers: BTreeMap<(i32, u64), i64>,
    /// answers that change with each ask of (op, a): popped front first
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
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
    fn count(&self, op: i32) -> usize {
        self.log.iter().filter(|e| e.0 == op).count()
    }
}

impl CuHost for Mock {
    fn cu(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
        assert_eq!(unit, 1);
        self.log.push((op, a, b, c));
        if op == EE_UNITS_CU_SOUND_PARAMETERS {
            f[0] = 0.25;
        }
        if op == EE_UNITS_CU_PLAY_FRAME_SOUND {
            assert_eq!(f[0], 0.25);
        }
        if let Some(values) = self.sequences.get_mut(&(op, a)) {
            if !values.is_empty() {
                return values.remove(0);
            }
        }
        *self.answers.get(&(op, a)).unwrap_or(&0)
    }
}

impl crate::units::cu_cargo::CuCargoHost for Mock {
    fn find_placement(&mut self, _p: &crate::units::cu_cargo::Placement, _m: &mut [[f32; 4]; 4]) -> bool {
        unreachable!("no body here searches")
    }
}

#[test]
fn a_dead_unit_is_not_killed_again() {
    let mut h = Mock::default();
    kill(&mut h, 1, true, false, false);
    assert_eq!(h.ops(), [EE_UNITS_CU_HIT_POINTS]);
}

#[test]
fn kill_releases_the_world_in_order() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_HIT_POINTS, 0, 10)
        .answer(EE_UNITS_CU_UNIT_SET, 0, 50)
        .answer(EE_UNITS_CU_SET_HAS_TYPE, 50, 1)
        .sequence(EE_UNITS_CU_NUM_PLAYERS, 0, &[2, 2, 2])
        .answer(EE_UNITS_CU_PLAYER_AT, 0, 60)
        .answer(EE_UNITS_CU_ENEMY_STATE, 60, 1)
        .answer(EE_UNITS_CU_SWAP_KIND, 0, 0)
        .answer(EE_UNITS_CU_CATEGORY, 0, 0x19)
        .answer(EE_UNITS_CU_ADD_DEAD_STATISTIC, 0, 3)
        .answer(EE_UNITS_CU_LAST_ATTACKER, 0, 70)
        .sequence(EE_UNITS_CU_HOLDER, 0, &[80, 80])
        .answer(EE_UNITS_CU_AIR_TRAFFIC_TYPE, 0, 2)
        .answer(EE_UNITS_CU_NEXT_PART, 0, 90)
        .sequence(EE_UNITS_CU_GOAL_QUEUE_COUNT, 0, &[2, 2, 2]);
    kill(&mut h, 1, true, false, false);
    assert_eq!(h.count(EE_UNITS_CU_UPDATE_FREE_UNIT_POWER), 2);
    assert!(h.log.contains(&(EE_UNITS_CU_STATS_RESET, 0, 1, 0)));
    assert!(h.log.contains(&(EE_UNITS_CU_STATS_LOWER, 0, 3, 0)));
    assert!(h.log.contains(&(EE_UNITS_CU_DEREFERENCE, 70, 0, 0)));
    assert!(h.log.contains(&(EE_UNITS_CU_DEREFERENCE, 80, 0, 0)));
    assert!(h.log.contains(&(EE_UNITS_CU_ATC_REMOVE_CARGO, 0, 2, 0)));
    assert!(h.log.contains(&(EE_UNITS_CU_START_ACTION, 0, 6000, 0)));
    assert!(h.log.contains(&(EE_UNITS_CU_KILL_PART, 90, 1, 0)));
    assert_eq!(h.count(EE_UNITS_CU_CLEAR_GOAL_QUEUE), 2);
    assert_eq!(h.count(EE_UNITS_CU_CANCEL_QUEUED_PRODUCTION), 1);
    assert_eq!(h.count(EE_UNITS_CU_CREATE_DEATH_UNIT), 0);
    // the order of the releases
    let ops = h.ops();
    let at = |op| ops.iter().position(|&o| o == op).unwrap();
    assert!(at(EE_UNITS_CU_UNIT_TAKEN) < at(EE_UNITS_CU_SWAP_KIND));
    assert!(at(EE_UNITS_CU_HOLDER_RELEASE) < at(EE_UNITS_CU_CLEAR_HOLDER_FIELD));
    assert!(at(EE_UNITS_CU_CANCEL_PATHS) < at(EE_UNITS_CU_DROP_PATH));
    assert!(at(EE_UNITS_CU_START_ACTION) < at(EE_UNITS_CU_ACTION_CHANGED));
    assert_eq!(ops.last(), Some(&EE_UNITS_CU_UNIT_KILL));
    assert_eq!(h.log.last().unwrap().2, 1);
}

#[test]
fn a_research_keeping_kill_suppresses_the_death_units_and_keeps_production() {
    let mut h = Mock::default().answer(EE_UNITS_CU_HIT_POINTS, 0, 10).answer(EE_UNITS_CU_HAS_DEATH_SPAWN, 0, 1).answer(
        EE_UNITS_CU_SWAP_KIND,
        0,
        1,
    );
    kill(&mut h, 1, false, true, false);
    assert_eq!(h.count(EE_UNITS_CU_CREATE_DEATH_UNIT), 0);
    assert_eq!(h.count(EE_UNITS_CU_CANCEL_QUEUED_PRODUCTION), 0);
    assert_eq!(h.count(EE_UNITS_CU_ADD_DEAD_STATISTIC), 0);
    assert!(h.log.contains(&(EE_UNITS_CU_START_ACTION, 0, 6003, 0)));
    assert!(h.log.contains(&(EE_UNITS_CU_UNIT_TAKEN, 0, 1, 0)));
    assert_eq!(h.log.last().unwrap().2, 2);
}

#[test]
fn the_insurance_pays_a_covered_unit() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_HIT_POINTS, 0, 10)
        .answer(EE_UNITS_CU_HAS_INSURANCE, 0, 1)
        .answer(EE_UNITS_CU_GET_INSURANCE, 0, 1)
        .answer(EE_UNITS_CU_INSURANCE_COVERS, 0, 1)
        .answer(EE_UNITS_CU_UNIT_TYPE, 0, 0x2a);
    kill(&mut h, 1, true, false, false);
    assert_eq!(h.count(EE_UNITS_CU_PAY_INSURANCE), 0, "an unbuilt farm is not covered");
    let mut h = h.answer(EE_UNITS_CU_IS_BUILT, 0, 1);
    kill(&mut h, 1, true, false, false);
    // (the power record is the host's to fill: the mock leaves its amount 0)
    assert!(h.log.contains(&(EE_UNITS_CU_PAY_INSURANCE, 0, bits(0.0), 0)));
}

#[test]
fn invalidate_releases_every_reference() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_CURRENT_RESEARCH, 0, 5)
        .sequence(EE_UNITS_CU_BUILD_TARGET_COUNT, 0, &[2, 2, 2])
        .sequence(EE_UNITS_CU_PATROL_GROUP, 0, &[7, 0])
        .answer(EE_UNITS_CU_NEXT_PART, 0, 9)
        .answer(EE_UNITS_CU_IS_LAND_BUILDING, 0, 1);
    invalidate(&mut h, 1);
    assert!(h.log.contains(&(EE_UNITS_CU_PLAY_EFFECT, 0, 100000, 0)));
    assert_eq!(h.count(EE_UNITS_CU_BUILD_TARGET_DEREFERENCE), 2);
    assert_eq!(h.count(EE_UNITS_CU_PATROL_REMOVE), 1);
    assert_eq!(h.count(EE_UNITS_CU_PATROL_DEREFERENCE), 0);
    assert!(h.log.contains(&(EE_UNITS_CU_DEREFERENCE, 9, 0, 0)));
    assert_eq!(h.count(EE_UNITS_CU_CLEAR_NEXT_PART), 1);
    assert_eq!(h.count(EE_UNITS_CU_CLEAR_MINE), 0);
    assert_eq!(h.count(EE_UNITS_CU_CLEAR_OWNER), 2);
    // the held units released by cu_world.rs (no held unit here: the list cleared)
    assert_eq!(h.count(crate::units::cu_world::EE_UNITS_CU_HELD_CLEAR), 1);
    assert_eq!(h.count(EE_UNITS_CU_UNLOAD_BUTTONS), 0, "an unbuilt building keeps its buttons");
    assert_eq!(h.ops().last(), Some(&EE_UNITS_CU_CLEAR_FOUNDATION));
}

#[test]
fn start_production_starts_the_first_available_entry() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_IS_BUILT, 0, 1)
        .answer(EE_UNITS_CU_QUEUE_FRONT_TECHNOLOGY, 0, 300)
        .answer(EE_UNITS_CU_TECH_NODE, 0, 40)
        .answer(EE_UNITS_CU_NODE_AVAILABLE, 40, 1)
        .answer(EE_UNITS_CU_CAN_PRODUCE, 0, 1)
        .answer(EE_UNITS_CU_NODE_MAKES_UNIT, 40, 1)
        .answer(EE_UNITS_CU_NODE_UNIT_CLASS, 40, 12)
        .answer(EE_UNITS_CU_CLASS_TABLE, 0, 120)
        .answer(EE_UNITS_CU_BUILD_TIME_SCALE, 0, bits(1.5))
        .answer(EE_UNITS_CU_NODE_BUILD_TIME, 40, 7);
    start_production(&mut h, 1);
    assert!(h.log.contains(&(EE_UNITS_CU_PLAY_EFFECT, 0, 300, 0)));
    assert!(h.log.contains(&(EE_UNITS_CU_SET_PRODUCTION_TIME, 0, 10, 0)), "7 * 1.5 rounds to even");
    assert_eq!(h.count(EE_UNITS_CU_UI_UPDATE_JIT), 0);
    assert_eq!(h.ops().last(), Some(&EE_UNITS_CU_QUEUE_POP_FRONT));
    assert_eq!(h.count(EE_UNITS_CU_RANDOM), 0, "no draw for a class other than the citizen");
}

#[test]
fn start_production_refunds_unavailable_entries() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_IS_BUILT, 0, 1)
        .sequence(EE_UNITS_CU_QUEUE_FRONT_TECHNOLOGY, 0, &[300, 301])
        .sequence(EE_UNITS_CU_TECH_NODE, 0, &[40, 41])
        .sequence(EE_UNITS_CU_QUEUE_EMPTY, 0, &[0, 1, 1]);
    start_production(&mut h, 1);
    assert_eq!(h.count(EE_UNITS_CU_REFUND_TECHNOLOGY), 2);
    assert!(h.log.contains(&(EE_UNITS_CU_REFUND_TECHNOLOGY, 41, bits(1.0), 0)));
    assert_eq!(h.count(EE_UNITS_CU_UI_UPDATE_JIT), 2);
}

#[test]
fn set_built_walks_the_parts() {
    let mut h = Mock::default().answer(EE_UNITS_CU_EE_PLAYER, 1, 60).answer(EE_UNITS_CU_PART_NEXT, 1, 2).answer(
        EE_UNITS_CU_PART_UNIT_TYPE,
        2,
        0x34,
    );
    set_built(&mut h, 1, 1, 1, 1);
    assert!(h.log.contains(&(EE_UNITS_CU_PART_UNIT_COMPLETED, 1, 60, 0)));
    assert_eq!(h.count(EE_UNITS_CU_PART_REPLANT), 0, "the second part has no player");
    assert_eq!(h.count(EE_UNITS_CU_PART_LOAD_BUTTONS), 2);
    assert_eq!(h.count(EE_UNITS_CU_PART_PAID), 2);
    let mut h = Mock::default();
    set_built(&mut h, 1, 0, 1, 0);
    assert_eq!(
        h.ops(),
        [EE_UNITS_CU_EE_PLAYER, EE_UNITS_CU_PART_SET_BUILT, EE_UNITS_CU_PART_NEXT],
        "unbuilding stores the flag only"
    );
}

#[test]
fn a_dying_unit_refuses_other_actions() {
    let mut h = Mock::default().answer(EE_UNITS_CU_HAS_COMMAND, 0, 1).answer(EE_UNITS_CU_COMMAND_TYPE, 0, 6001);
    start_action(&mut h, 1, 2000);
    assert_eq!(h.count(EE_UNITS_CU_SET_COMMAND), 0);
    start_action(&mut h, 1, 6005);
    assert!(h.log.contains(&(EE_UNITS_CU_SET_COMMAND, 0, 6005, 0)));
    let mut h = h.answer(EE_UNITS_CU_IS_LAND_BUILDING, 0, 1);
    start_action(&mut h, 1, 2000);
    assert!(h.log.contains(&(EE_UNITS_CU_SET_COMMAND, 0, 2000, 0)));
}

#[test]
fn set_animation_plays_the_frames_sound() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_MODEL, 0, 30)
        .answer(EE_UNITS_CU_MODEL_START_FRAME, 30, 4)
        .answer(EE_UNITS_CU_MODEL_LAST_FRAME, 30, 9)
        .answer(EE_UNITS_CU_PM_FRAME, 0, 4)
        .answer(EE_UNITS_CU_MODEL_SOUND2_FRAME, 30, 4)
        .answer(EE_UNITS_CU_MODEL_SOUND1_FRAME, 30, 3)
        .answer(EE_UNITS_CU_MODEL_SOUND2, 30, 44);
    set_animation(&mut h, 1, 3, -1, false);
    assert!(h.log.contains(&(EE_UNITS_CU_PM_SET_FRAME, 0, 4, 0)));
    assert!(h.log.contains(&(EE_UNITS_CU_SET_NEXT_FRAME, 0, 5, 0)));
    assert!(h.log.contains(&(EE_UNITS_CU_PLAY_FRAME_SOUND, 44, 0, 0)));
    // the same death animation again: nothing
    let mut h = Mock::default().answer(EE_UNITS_CU_ANIMATION, 0, 0xd);
    set_animation(&mut h, 1, 0xd, 2, false);
    assert_eq!(h.ops(), [EE_UNITS_CU_MODEL, EE_UNITS_CU_ANIMATION]);
}

#[test]
fn insurance_notices_pass_the_thresholds() {
    let mut h = Mock::default()
        .sequence(EE_UNITS_CU_PLAYER, 0, &[60, 60])
        .answer(EE_UNITS_CU_CLASS_TECH_NODE, 60, 40)
        .answer(EE_UNITS_CU_CURRENT_PLAYER, 0, 60)
        // food 90 -> 150 (passes 100: shown 100), wood 0 -> 10 (under 20: shown as is), the rest unchanged
        .sequence(EE_UNITS_CU_REFUND_TOTAL, 60, &[90, 0, 0, 0, 0, 90, 0, 0, 0, 0, 150, 10, 0, 0, 0]);
    pay_insurance(&mut h, 1, 0.5);
    use crate::units::cu_small::EE_UNITS_CU_S_INSURANCE_RESOURCE_NAME;
    let resources: Vec<i64> =
        h.log.iter().filter(|e| e.0 == EE_UNITS_CU_S_INSURANCE_RESOURCE_NAME).map(|e| e.2).collect();
    assert_eq!(resources, vec![2, 3]);
    assert!(h.log.contains(&(EE_UNITS_CU_PLAYER_REFUND_TECHNOLOGY, 40, bits(0.5), 0)));
    assert_eq!(h.count(EE_UNITS_CU_DISBAND_EFFECT), 1);
}

#[test]
fn contacts_are_mutual_and_in_one_layer() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_INNER_SPHERE, 0, bits(0.5))
        .answer(EE_UNITS_CU_LOCATION_X, 0, bits(10.5))
        .answer(EE_UNITS_CU_LOCATION_Y, 0, bits(20.5))
        .answer(EE_UNITS_CU_MAP_TILE, 0, 5)
        .answer(EE_UNITS_CU_TILE_FIRST_NODE, 5, 100)
        .answer(EE_UNITS_CU_NODE_COMPLEX, 100, 7)
        .answer(EE_UNITS_CU_OTHER_COLLIDABLE, 7, 1)
        .answer(EE_UNITS_CU_OTHER_HIT_POINTS, 7, 5)
        .answer(EE_UNITS_CU_OTHER_CLASS, 7, 70);
    find_contacts(&mut h, 1);
    // a radius of 0.5 covers tiles 10..=11 x 20..=21
    assert_eq!(h.count(EE_UNITS_CU_MAP_TILE), 4);
    assert!(h.log.contains(&(EE_UNITS_CU_MAP_TILE, 0, 10, 20)));
    assert!(h.log.contains(&(EE_UNITS_CU_MAP_TILE, 0, 11, 21)));
    assert!(h.log.contains(&(EE_UNITS_CU_ADD_CONTACT, 1, 7, 0)));
    assert!(h.log.contains(&(EE_UNITS_CU_ADD_CONTACT, 7, 1, 0)));
    // another layer: no contact
    let mut h = h.answer(EE_UNITS_CU_FLIES_OVER, 70, 1);
    h.log.clear();
    find_contacts(&mut h, 1);
    assert_eq!(h.count(EE_UNITS_CU_ADD_CONTACT), 0);
}

#[test]
fn floor_is_the_originals() {
    assert_eq!(floor(10.0), 10);
    assert_eq!(floor(10.5), 10);
    assert_eq!(floor(11.5), 11);
    assert_eq!(floor(-0.5), -1);
    assert_eq!(floor(-1.5), -2);
}
