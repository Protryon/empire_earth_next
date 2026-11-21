use super::*;
use std::collections::BTreeMap;

/// A recording host: each statement asked (op, a, b, c, f), its answer from `answers` keyed by (op, a, b) (0 else);
/// `sequences` answer each ask of (op, a, b) in turn.
#[derive(Default)]
struct Mock {
    log: Vec<(i32, u64, i64, i64, [f32; 4])>,
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
    fn count(&self, op: i32) -> usize {
        self.log.iter().filter(|e| e.0 == op).count()
    }
    fn asked(&self, op: i32, a: u64, b: i64) -> bool {
        self.log.iter().any(|e| e.0 == op && e.1 == a && e.2 == b)
    }
    fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
}

impl CuHost for Mock {
    fn cu(&mut self, op: i32, _unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
        self.log.push((op, a, b, c, *f));
        if let Some(values) = self.sequences.get_mut(&(op, a, b)) {
            if !values.is_empty() {
                return values.remove(0);
            }
        }
        *self.answers.get(&(op, a, b)).unwrap_or(&0)
    }
}

const U: u64 = 1;

#[test]
fn the_condition_target_is_kept_while_it_attacks_in_reach() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_LAST_ATTACKER, 0, 0, 20)
        .answer(EE_UNITS_CU_ATTACK_GOAL_OF, 20, 0, 21)
        .answer(EE_UNITS_CU_ATTACK_TARGET_IS_THIS, 21, 0, 1)
        .answer(EE_UNITS_CU_IN_REACH_OF, 20, 0, 1);
    assert_eq!(condition_target(&mut h, U), 20);
    assert_eq!(h.count(EE_UNITS_CU_LINE_FROM_BLOCKED), 1);
    // out of reach: forgotten
    let mut h = Mock::default()
        .sequence(EE_UNITS_CU_LAST_ATTACKER, 0, 0, &[20, 20, 0])
        .answer(EE_UNITS_CU_ATTACK_GOAL_OF, 20, 0, 21)
        .answer(EE_UNITS_CU_ATTACK_TARGET_IS_THIS, 21, 0, 1);
    assert_eq!(condition_target(&mut h, U), 0);
    assert!(h.asked(EE_UNITS_CU_DEREFERENCE, 20, 0));
    assert_eq!(h.count(EE_UNITS_CU_CLEAR_LAST_ATTACKER), 1);
}

#[test]
fn a_ground_attack_covering_the_unit_keeps_its_attacker() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_LAST_ATTACKER, 0, 0, 20)
        .answer(EE_UNITS_CU_GROUND_GOAL_OF, 20, 0, 22)
        .answer(EE_UNITS_CU_LOCATION_X, 0, 0, bits(3.0))
        .answer(EE_UNITS_CU_LOCATION_Y, 0, 0, bits(4.0))
        .answer(EE_UNITS_CU_AREA_EFFECT_OF, 20, 0, bits(5.0));
    assert_eq!(condition_target(&mut h, U), 20);
}

#[test]
fn replace_unit_swaps_the_cargo_and_the_part() {
    let (old, new) = (30u64, 31u64);
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_CARGO_SIZE, 0, 0, 2)
        .answer(EE_UNITS_CU_CARGO_AT, 0, 1, old as i64)
        .answer(EE_UNITS_CU_UNIT_CONTAINER, new, 0, 40)
        .answer(EE_UNITS_CU_PREVIOUS_PART, 0, 0, old as i64);
    replace_unit(&mut h, U, old, new);
    assert!(h.asked(EE_UNITS_CU_CARGO_DEREFERENCE_AT, 0, 1));
    assert!(h.asked(EE_UNITS_CU_CARGO_SET_AT, new, 1));
    assert!(h.asked(EE_UNITS_CU_DEREFERENCE, 40, 0));
    assert_eq!(h.count(EE_UNITS_CU_REFERENCE_THIS), 1);
    assert!(h.asked(EE_UNITS_CU_LINK_PART_TO, new, 0));
    assert_eq!(h.count(EE_UNITS_CU_DEACTIVATE_THIS), 1);
}

#[test]
fn replace_units_renames_the_build_targets() {
    let pairs = 50;
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_PAIRS_SIZE, pairs, 0, 1)
        .answer(EE_UNITS_CU_PAIR_UNIT, pairs, 0, 60)
        .sequence(EE_UNITS_CU_BUILD_TARGET_COUNT, 0, 0, &[2, 1, 0])
        .sequence(EE_UNITS_CU_CITIZEN_FRONT, 0, 0, &[60, 61])
        .answer(EE_UNITS_CU_UNIT_VALID, 60, 0, 1)
        .answer(EE_UNITS_CU_UNIT_VALID, 61, 0, 1);
    // the pair's new unit (c = 1) answers by the sequence of (op, pairs, 0) too: give it 70 after the old's 60
    h.sequences.insert((EE_UNITS_CU_PAIR_UNIT, pairs, 0), vec![60, 70, 60]);
    replace_units(&mut h, U, pairs);
    let added: Vec<u64> = h.log.iter().filter(|e| e.0 == EE_UNITS_CU_ADD_BUILD_TARGET_UNIT).map(|e| e.1).collect();
    assert_eq!(added, [70, 61]);
}

#[test]
fn the_sight_moves_or_is_added_and_removed() {
    let mut h = Mock::default().answer(EE_UNITS_CU_SIGHT_READY, 0, 0, 1).answer(EE_UNITS_CU_HIT_POINTS, 0, 0, 5);
    sight_moved(&mut h, U, 2, 3, 4, 5);
    let m = h.log.iter().find(|e| e.0 == EE_UNITS_CU_MOVE_SIGHT_CIRCLE).unwrap();
    assert_eq!(m.4, [2.5, 3.5, 4.5, 5.5]);
    let mut h = Mock::default().answer(EE_UNITS_CU_SIGHT_READY, 0, 0, 1);
    sight_moved(&mut h, U, OFF_MAP, OFF_MAP, 4, 5);
    assert_eq!(h.ops(), [EE_UNITS_CU_TILE_SIZE, EE_UNITS_CU_SIGHT_READY, EE_UNITS_CU_SIGHT_ON]);
    // a dead unit: on at the new tile, off at the old
    let mut h = Mock::default().answer(EE_UNITS_CU_SIGHT_READY, 0, 0, 1);
    sight_moved(&mut h, U, 2, 3, 4, 5);
    assert_eq!(&h.ops()[2..], [EE_UNITS_CU_HIT_POINTS, EE_UNITS_CU_SIGHT_ON, EE_UNITS_CU_SIGHT_OFF]);
}

#[test]
fn a_closed_gate_lets_only_friends_and_trojans_pass() {
    let gate = || {
        Mock::default()
            .answer(EE_UNITS_CU_CATEGORY, 0, 0, CATEGORY_GATE)
            .answer(EE_UNITS_CU_HIT_POINTS, 0, 0, 10)
            .answer(EE_UNITS_CU_HAS_COMMAND, 0, 0, 1)
            .answer(EE_UNITS_CU_COMMAND_KIND, 0, 0, ACTION_CLOSED)
    };
    assert!(is_passable_by(&mut gate(), U, 5));
    assert!(!is_passable_by(&mut gate().answer(EE_UNITS_CU_HOSTILE, 5, 0, 1), U, 5));
    let mut h = gate().answer(EE_UNITS_CU_HOSTILE, 5, 0, 1).answer(EE_UNITS_CU_UNIT_UNIT_TYPE, 5, 0, UNIT_TYPE_X04_TROJAN_HORSE);
    assert!(is_passable_by(&mut h, U, 5));
    assert!(!is_passable_by(&mut gate(), U, 0));
    // a tile blocker that stands
    let mut h = Mock::default().answer(EE_UNITS_CU_BLOCKS_TILE, 0, 0, 1).answer(EE_UNITS_CU_HIT_POINTS, 0, 0, 1);
    assert!(!is_passable_by(&mut h, U, 5));
}

#[test]
fn the_reach_of_a_ranged_unit_is_its_range_on_its_continent() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_STATS_RANGE, 0, 0, bits(3.0))
        .answer(EE_UNITS_CU_DISTANCE_SQUARED_TO_TILE, 0, 4, bits(9.0));
    assert!(can_reach_tile(&mut h, U, 4, 4));
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_STATS_RANGE, 0, 0, bits(3.0))
        .answer(EE_UNITS_CU_DISTANCE_SQUARED_TO_TILE, 0, 4, bits(9.5));
    assert!(!can_reach_tile(&mut h, U, 4, 4));
    // a ballistic unit needs its pitch
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_PROJECTILE_CLASS, 0, 0, 80)
        .answer(EE_UNITS_CU_CLASS_CATEGORY_OF, 80, 0, CATEGORY_BOMB)
        .answer(EE_UNITS_CU_BALLISTIC_PITCH, 0, 0, 1);
    assert!(can_reach_tile(&mut h, U, 4, 4));
    assert!(h.asked(EE_UNITS_CU_LINE_BLOCKED, 0, 4));
}

#[test]
fn an_air_unit_is_on_another_continent_when_it_cannot_get_there() {
    let mut h = Mock::default().answer(EE_UNITS_CU_CLASS_DOMAINS, 0, 0, MOVEMENT_AIR);
    assert!(is_on_other_continent(&mut h, U, 1, 2));
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_IS_SPACE_MAP, 0, 0, 1)
        .answer(EE_UNITS_CU_CONTINENT_NON_SPACE, 0, 0, 3)
        .answer(EE_UNITS_CU_TILE_CONTINENT_NON_SPACE, 0, 1, 4);
    assert!(is_on_other_continent(&mut h, U, 1, 2));
}

#[test]
fn a_large_unit_looks_for_a_tile_of_its_kind() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_UNIT_TILE_SIZE, U, 0, 2)
        .answer(EE_UNITS_CU_UNIT_TILE, U, 0, 10)
        .answer(EE_UNITS_CU_UNIT_TILE, U, 1, 10)
        .answer(EE_UNITS_CU_GET_TILE_X, 0, 0, 10)
        .answer(EE_UNITS_CU_TILE_Y, 0, 0, 10)
        .answer(EE_UNITS_CU_MAP_SIZE, 0, 0, 100)
        .answer(EE_UNITS_CU_MAP_SIZE, 0, 1, 100)
        .answer(EE_UNITS_CU_CONTINENT_ID, 0, 10, 7)
        .answer(EE_UNITS_CU_CONTINENT_BY_ID, 0, 7, 90)
        .answer(EE_UNITS_CU_CONTINENT_CATEGORY, 90, 0, 1);
    // the first tile of x 10 (y from 9) on a land continent
    assert_eq!(find_area_tile(&mut h, U, true, 1), (10, 9));
    // a one-tile unit keeps its tile
    let mut h = Mock::default().answer(EE_UNITS_CU_UNIT_TILE, U, 0, 3).answer(EE_UNITS_CU_UNIT_TILE, U, 1, 4);
    assert_eq!(find_area_tile(&mut h, U, true, 1), (3, 4));
}

#[test]
fn the_nearest_town_is_the_containing_one_or_the_closest() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_NUM_TOWNS, 0, 0, 3)
        .answer(EE_UNITS_CU_TOWN_AT, 0, 0, 100)
        .answer(EE_UNITS_CU_TOWN_AT, 0, 1, 101)
        .answer(EE_UNITS_CU_TOWN_AT, 0, 2, 102)
        .answer(EE_UNITS_CU_TOWN_DISTANCE, 100, 0, 50)
        .answer(EE_UNITS_CU_TOWN_DISTANCE, 101, 0, 20)
        .answer(EE_UNITS_CU_TOWN_DISTANCE, 102, 0, 30);
    assert_eq!(find_nearest_town(&mut h, U), 101);
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_NUM_TOWNS, 0, 0, 2)
        .answer(EE_UNITS_CU_TOWN_AT, 0, 1, 101)
        .answer(EE_UNITS_CU_TOWN_CONTAINS, 101, 0, 1);
    assert_eq!(find_nearest_town(&mut h, U), 101);
}

#[test]
fn a_move_refreshes_the_contacts_and_the_swim_animation() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_UM_FLOOR, 0, bits(3.5), 3)
        .answer(EE_UNITS_CU_IS_COLLIDABLE, 0, 0, 1)
        .answer(EE_UNITS_CU_HAS_SWIM_ANIMATION, 0, 0, 1)
        .answer(EE_UNITS_CU_ANIMATION, 0, 0, ANIMATION_WALK);
    move_to_location(&mut h, U, 3.5, 0.0, true);
    assert!(h.asked(EE_UNITS_CU_MOVE_TO_UPDATE, 1, 3));
    assert_eq!(h.count(EE_UNITS_CU_CLEAR_CONTACTS), 1);
    assert!(h.asked(EE_UNITS_CU_SET_ANIMATION_STATE, 0, ANIMATION_SWIM));
}

#[test]
fn growth_scales_the_model_and_keeps_a_minimum_radius() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_STATS_SCALE, 0, 0, bits(2.0))
        .answer(EE_UNITS_CU_INNER_SPHERE, 0, 0, bits(0.5))
        .answer(EE_UNITS_CU_GROWTH, 0, 0, bits(0.05))
        .answer(EE_UNITS_CU_COLLISION_RADIUS, 0, 0, bits(0.025));
    set_growth(&mut h, U, 0.05);
    assert!(h.asked(EE_UNITS_CU_PM_SET_SCALE, 0, bits(2.0 * 0.05)));
    assert!(h.asked(EE_UNITS_CU_SET_COLLISION_RADIUS, 0, bits(0.5 * 0.05)));
    assert!(h.asked(EE_UNITS_CU_SET_COLLISION_RADIUS, 0, bits(0.05)));
    let mut h = Mock::default().answer(EE_UNITS_CU_GROWTH, 0, 0, bits(1.0));
    set_growth(&mut h, U, 1.0);
    assert_eq!(h.count(EE_UNITS_CU_SET_GROWTH), 0);
}

#[test]
fn the_squadron_goals_are_cloned_past_the_first() {
    let mut h = Mock::default().answer(EE_UNITS_CU_GOAL_CLONE, 9, 0, 10).answer(EE_UNITS_CU_QUEUED_GOAL, 0, 2, 1);
    queue_squadron_goals(&mut h, U, 9, false);
    assert!(h.asked(EE_UNITS_CU_SET_GOAL, 9, 0));
    assert!(h.asked(EE_UNITS_CU_QUEUE_GOAL, 10, 2));
    assert_eq!(h.count(EE_UNITS_CU_GOAL_CLONE), 2);
    let mut h = Mock::default().answer(EE_UNITS_CU_HAS_PARATROOPERS, 0, 0, 1);
    set_squadron_goals(&mut h, U, 9, true);
    assert_eq!(h.count(EE_UNITS_CU_CLEAR_GOAL_QUEUE), 4);
    assert_eq!(h.count(EE_UNITS_CU_GOAL_CLONE), 4);
}

#[test]
fn the_area_effects_use_the_largest_radius() {
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_AREA_EFFECTS_SIZE, 0, 0, 2)
        .answer(EE_UNITS_CU_IS_BUILT, 0, 0, 1)
        .answer(EE_UNITS_CU_PLAYER_ID, 0, 0, 3)
        .sequence(EE_UNITS_CU_AREA_EFFECT_COUNT, 0, 0, &[2, 2, 2])
        .answer(EE_UNITS_CU_AREA_EFFECT_RADIUS, 0, 0, bits(4.0))
        .answer(EE_UNITS_CU_AREA_EFFECT_RADIUS, 0, 1, bits(6.0));
    apply_area_effects(&mut h, U);
    assert!(h.log.iter().any(|e| e.0 == EE_UNITS_CU_COLLECT_EFFECTS && e.2 == bits(6.0) && e.3 == 3));
}

#[test]
fn the_build_queue_pops_past_invalid_units() {
    let mut h = Mock::default()
        .sequence(EE_UNITS_CU_BUILD_TARGET_COUNT, 0, 0, &[2, 1])
        .sequence(EE_UNITS_CU_CITIZEN_FRONT, 0, 0, &[60, 61])
        .answer(EE_UNITS_CU_UNIT_VALID, 61, 0, 1);
    assert_eq!(pop_build_target(&mut h, U), 61);
    assert_eq!(h.count(EE_UNITS_CU_DEREFERENCE), 2);
}

#[test]
fn fanaticism_costs_hit_points_but_leaves_one() {
    let mut h = Mock::default().answer(EE_UNITS_CU_FANATICISM_COST, 0, 0, bits(0.99)).answer(EE_UNITS_CU_HIT_POINTS, 0, 0, 50);
    go_fanatic(&mut h, U);
    assert!(h.asked(EE_UNITS_CU_SET_HIT_POINTS, 0, 1));
    let mut h = Mock::default().answer(EE_UNITS_CU_FANATICISM_COST, 0, 0, bits(0.5)).answer(EE_UNITS_CU_HIT_POINTS, 0, 0, 50);
    go_fanatic(&mut h, U);
    assert!(h.asked(EE_UNITS_CU_SET_HIT_POINTS, 0, 25));
    assert!(h.asked(EE_UNITS_CU_SET_TINTED, 0, 1));
}

#[test]
fn links_move_their_references() {
    let mut h = Mock::default().answer(EE_UNITS_CU_BLOCKING_UNIT, 0, 0, 5);
    set_link(&mut h, U, 6, EE_UNITS_CU_BLOCKING_UNIT, EE_UNITS_CU_SET_BLOCKING_FIELD);
    assert_eq!(
        h.ops(),
        [
            EE_UNITS_CU_BLOCKING_UNIT,
            EE_UNITS_CU_DEREFERENCE,
            EE_UNITS_CU_SET_BLOCKING_FIELD,
            EE_UNITS_CU_SET_BLOCKING_FIELD,
            EE_UNITS_CU_REFERENCE
        ]
    );
    let mut h = Mock::default().answer(EE_UNITS_CU_HOLDER, 0, 0, 6);
    set_holder(&mut h, U, 6);
    assert_eq!(h.log.len(), 1);
    let mut h = Mock::default().answer(EE_UNITS_CU_HELD_FIND, 7, 0, 2);
    assert!(release_held(&mut h, U, 7));
    assert!(h.asked(EE_UNITS_CU_HELD_ERASE, 0, 2));
}

#[test]
fn the_reload_range_is_read_once() {
    let mut range = None;
    let mut h = Mock::default()
        .answer(EE_UNITS_CU_RELOAD_RANGE, 0, 0, bits(0.5))
        .answer(EE_UNITS_CU_RELOAD_RANGE, 0, 1, bits(1.5))
        .answer(EE_UNITS_CU_RANDOM_BETWEEN, 0, bits(0.5), bits(2.0))
        .answer(EE_UNITS_CU_FLOAT_TO_LONG, 0, bits(2.0 * 100.0), 200);
    set_random_reload_timer(&mut h, U, 100, &mut range);
    set_random_reload_timer(&mut h, U, 100, &mut range);
    assert_eq!(h.count(EE_UNITS_CU_RELOAD_RANGE), 2);
    assert_eq!(h.count(EE_UNITS_CU_SET_RELOAD_TIMER), 2);
    assert!(h.asked(EE_UNITS_CU_SET_RELOAD_TIMER, 0, 200));
}
