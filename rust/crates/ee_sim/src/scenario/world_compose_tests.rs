//! The composed world operations as statement streams: each the C++ sequence's statements in its order.

use std::collections::HashMap;

use super::*;

/// The statements a composed operation runs (op and its first arguments), answering from a table (0 otherwise).
fn run(op: u32, a: [i64; 6], answers: &[(u32, i64)]) -> (Option<i64>, Vec<(u32, Vec<i64>)>) {
    let table: HashMap<u32, i64> = answers.iter().copied().collect();
    let mut seen = Vec::new();
    let mut cpp = |op: u32, args: [i64; 6]| {
        let used = args.iter().rposition(|&v| v != 0).map_or(0, |i| i + 1);
        seen.push((op, args[..used].to_vec()));
        *table.get(&op).unwrap_or(&0)
    };
    let r = compose(&mut cpp, op, a);
    (r, seen)
}

fn ops(seen: &[(u32, Vec<i64>)]) -> Vec<u32> {
    seen.iter().map(|s| s.0).collect()
}

#[test]
fn an_operation_the_cpp_answers_is_not_composed() {
    let (r, seen) = run(EE_SCENARIO_WORLD_UNIT, [1, 2, 0, 0, 0, 0], &[]);
    assert_eq!(r, None);
    assert!(seen.is_empty());
}

#[test]
fn a_task_preparation_detaches_enables_and_copies_the_class_deletable_flag() {
    let (r, seen) = run(EE_SCENARIO_WORLD_TASK_PREPARE, [7, 0, 0, 0, 0, 0], &[(EE_SCENARIO_WORLD_UNIT, 1)]);
    assert_eq!(r, Some(0));
    assert_eq!(
        seen,
        vec![
            (EE_SCENARIO_WX_DETACH, vec![7]),
            (EE_SCENARIO_WORLD_SET_DISABLED, vec![7]),
            (EE_SCENARIO_WORLD_SET_COMMANDABLE, vec![7, 1]),
            (EE_SCENARIO_WORLD_UNIT, vec![7, EE_SCENARIO_UV_CLASS_DELETABLE]),
            (EE_SCENARIO_WORLD_SET_DELETABLE, vec![7, 1]),
        ]
    );
}

#[test]
fn a_gather_makes_its_goal_after_clearing_the_old_one_then_sets_acts_and_plans() {
    let (_, seen) = run(EE_SCENARIO_WORLD_GROUP_GATHER, [5, 0, 0, 0, 0, 0], &[]);
    assert_eq!(
        ops(&seen),
        vec![
            EE_SCENARIO_WX_GROUP_SPREAD_AT_UNIT,
            EE_SCENARIO_WX_GROUP_CLEAR_GOAL,
            EE_SCENARIO_WX_GOAL_GATHER,
            EE_SCENARIO_WX_GROUP_SET_GOAL,
            EE_SCENARIO_WX_GROUP_ACTION,
            EE_SCENARIO_WX_GROUP_PLAN,
        ]
    );
    assert_eq!(seen[4].1, vec![4004]);
}

#[test]
fn a_gather_test_searches_both_kinds_before_testing_either() {
    let (r, seen) = run(
        EE_SCENARIO_WORLD_GROUP_CAN_GATHER,
        [3, 0, 0, 0, 0, 0],
        &[(EE_SCENARIO_WX_FOUND, 1), (EE_SCENARIO_WX_FOUND_CAN_GATHER, 0)],
    );
    assert_eq!(r, Some(0));
    assert_eq!(
        ops(&seen),
        vec![
            EE_SCENARIO_WX_GROUP_FIND,
            EE_SCENARIO_WX_GROUP_FIND_NAVAL,
            EE_SCENARIO_WX_FOUND,
            EE_SCENARIO_WX_FOUND_CAN_GATHER,
            EE_SCENARIO_WX_FOUND,
            EE_SCENARIO_WX_FOUND_CAN_GATHER,
        ]
    );
    assert_eq!(seen[5].1, vec![1, 3]);
}

#[test]
fn a_load_goal_reuses_a_current_goal_of_its_kind() {
    let (r, seen) = run(
        EE_SCENARIO_WORLD_LOAD_GOAL,
        [1, 9, 0, 0, 0, 0],
        &[(EE_SCENARIO_WORLD_UNIT, 1), (EE_SCENARIO_WX_GOAL_KIND, GOAL_LOAD_TRANSPORT)],
    );
    assert_eq!(r, Some(0));
    assert_eq!(ops(&seen)[2..], [EE_SCENARIO_WX_GOAL_CAST_TRANSPORT, EE_SCENARIO_WX_GOAL_ADD_LOAD]);
    let (r, seen) = run(EE_SCENARIO_WORLD_LOAD_GOAL, [0, 9, 0, 0, 0, 0], &[]);
    assert_eq!(r, Some(1));
    assert_eq!(seen.last().unwrap(), &(EE_SCENARIO_WX_GOAL_NEW_LOAD, vec![GOAL_LOAD_GARRISON, 9]));
}

#[test]
fn an_unload_falls_back_to_a_move_when_the_transport_cannot_unload() {
    let (_, seen) = run(EE_SCENARIO_WORLD_UNLOAD, [4, 10, 11, 0, 0, 0], &[(EE_SCENARIO_WORLD_UNIT, 1)]);
    assert_eq!(
        ops(&seen),
        vec![
            EE_SCENARIO_WX_UGOAL_UNLOAD,
            EE_SCENARIO_WORLD_UNIT,
            EE_SCENARIO_WORLD_UNIT,
            EE_SCENARIO_WX_UGOAL_CAN_UNLOAD,
            EE_SCENARIO_WX_UGOAL_DELETE,
            EE_SCENARIO_WORLD_CLEAR_GOALS,
            EE_SCENARIO_WX_UGOAL_POINT,
            EE_SCENARIO_WX_UNIT_SET_GOAL,
            EE_SCENARIO_WX_START_ACTION,
            EE_SCENARIO_WX_ACTION_CHANGED,
        ]
    );
}

#[test]
fn a_kill_holds_the_unit_and_removes_it_from_its_player_when_it_left_the_map() {
    let (_, seen) = run(EE_SCENARIO_WORLD_KILL, [2, 1, 0, 0, 0, 0], &[]);
    assert_eq!(
        ops(&seen),
        vec![EE_SCENARIO_WX_HOLD, EE_SCENARIO_WX_HELD_KILL, EE_SCENARIO_WX_HELD_ON_NODE, EE_SCENARIO_WX_HELD_PLAYER_KILL]
    );
    let (_, seen) = run(EE_SCENARIO_WORLD_KILL, [2, 1, 0, 0, 0, 0], &[(EE_SCENARIO_WX_HELD_ON_NODE, 1)]);
    assert_eq!(seen.len(), 3);
}

#[test]
fn the_facing_test_compares_the_heading_within_one_degree() {
    // the location's elements (1, 0) and (0, 0) both 1: the heading is atan2(1, 1)
    let element = [(EE_SCENARIO_WX_LOCATION_AT, bits(1.0))];
    let heading = um_arctan2(1.0, 1.0);
    let near = run(EE_SCENARIO_WORLD_FACE_TEST, [1, bits(heading + 0.01), 0, 0, 0, 0], &element);
    assert_eq!(near.0, Some(0));
    let far = run(EE_SCENARIO_WORLD_FACE_TEST, [1, bits(heading + 0.1), 0, 0, 0, 0], &element);
    assert_eq!(far.0, Some(1));
    assert_eq!(far.1[0].1, vec![1, 1]); // the element (1, 0) first
}

#[test]
fn a_lock_sets_only_what_differs() {
    let (_, seen) = run(
        EE_SCENARIO_WORLD_LOCK,
        [6, 1, 0, 0, 0, 0],
        &[(EE_SCENARIO_WX_LOCKED, 1), (EE_SCENARIO_WX_GATE_SECOND, 1), (EE_SCENARIO_WX_GATE_FOURTH, 0)],
    );
    assert_eq!(
        ops(&seen),
        vec![
            EE_SCENARIO_WX_LOCKED,
            EE_SCENARIO_WX_GATE_SECOND,
            EE_SCENARIO_WX_PART_LOCKED,
            EE_SCENARIO_WX_PART_SET_LOCKED,
            EE_SCENARIO_WX_GATE_FOURTH,
        ]
    );
}

#[test]
fn the_placed_effect_point_is_the_tile_centre_on_the_ground_or_the_water() {
    let (_, seen) = run(
        EE_SCENARIO_WORLD_GFX_PLACED,
        [3, 4, 0, 0, 0, 0],
        &[(EE_SCENARIO_WX_TILE_GET_HEIGHT, bits(2.5)), (EE_SCENARIO_WX_TILE_WATER, 1)],
    );
    let point = seen.iter().find(|s| s.0 == EE_SCENARIO_WX_POINT_SET).unwrap();
    assert_eq!(point.1, vec![bits(3.5), bits(4.5)]); // z 0 for water
}

#[test]
fn the_graphic_effect_kinds_answer_their_flags() {
    let weather = run(EE_SCENARIO_WORLD_GFX_BEGIN, [1, 0, 0, 0, 0, 0], &[(EE_SCENARIO_WX_GFX_GET, 1), (EE_SCENARIO_WX_GFX_KIND, 0x3d)]);
    assert_eq!(weather.0, Some(EE_SCENARIO_GFX_WEATHER));
    let pair = run(EE_SCENARIO_WORLD_GFX_BEGIN, [1, 0, 0, 0, 0, 0], &[(EE_SCENARIO_WX_GFX_GET, 1), (EE_SCENARIO_WX_GFX_KIND, 0x21)]);
    assert_eq!(pair.0, Some(EE_SCENARIO_GFX_PAIR));
    let none = run(EE_SCENARIO_WORLD_GFX_BEGIN, [1, 0, 0, 0, 0, 0], &[]);
    assert_eq!(none.0, Some(-1));
}

#[test]
fn the_flare_tile_is_the_units_or_its_containers() {
    let own = run(
        EE_SCENARIO_WORLD_FLARE_UNIT_TILE,
        [1, 0, 0, 0, 0, 0],
        &[
            (EE_SCENARIO_WX_UNIT_HAS_COMPLEX, 1),
            (EE_SCENARIO_WX_UNIT_ON_TILE, 1),
            (EE_SCENARIO_WX_UNIT_TILE_X, 3),
            (EE_SCENARIO_WX_UNIT_TILE_Y, -2),
        ],
    );
    assert_eq!(own.0, Some(pack(3, -2)));
    let held = run(
        EE_SCENARIO_WORLD_FLARE_UNIT_TILE,
        [1, 0, 0, 0, 0, 0],
        &[(EE_SCENARIO_WX_UNIT_HAS_COMPLEX, 1), (EE_SCENARIO_WX_UNIT_IN_CONTAINER, 1)],
    );
    assert_eq!(held.0, Some(-1));
}

#[test]
fn a_town_patrol_without_a_target_does_nothing_more() {
    let (r, seen) = run(EE_SCENARIO_WORLD_STANCE_TOWN_PATROL, [1, 0, 0, 0, 0, 0], &[(EE_SCENARIO_WX_TOWN_FIND, 1)]);
    assert_eq!(r, Some(0));
    assert_eq!(ops(&seen), vec![EE_SCENARIO_WX_TOWN_FIND, EE_SCENARIO_WX_TOWN_PATROL]);
}

#[test]
fn a_class_value_is_its_flags_and_nothing_for_a_missing_class() {
    let (r, _) = run(EE_SCENARIO_WORLD_CLASS, [5, 0, 0, 0, 0, 0], &[(EE_SCENARIO_WX_CLASS_COUNT, 3)]);
    assert_eq!(r, Some(0));
    let answers = [
        (EE_SCENARIO_WX_CLASS_COUNT, 10),
        (EE_SCENARIO_WX_CLASS_EXISTS, 1),
        (EE_SCENARIO_WX_CLASS_SEA, 1),
        (EE_SCENARIO_WX_CLASS_CATEGORY, 4),
    ];
    let (r, _) = run(EE_SCENARIO_WORLD_CLASS, [5, 0, 0, 0, 0, 0], &answers);
    assert_eq!(r, Some(EE_SCENARIO_CLASS_EXISTS | EE_SCENARIO_CLASS_SEA | EE_SCENARIO_CLASS_NOT_LAND));
}

#[test]
fn a_tech_add_of_an_epoch_raises_the_range_and_makes_it_available() {
    let (_, seen) = run(
        EE_SCENARIO_WORLD_TECH_ADD,
        [2, 0, 0, 0, 0, 0],
        &[(EE_SCENARIO_WX_EFFECT_TECH, 3), (EE_SCENARIO_WX_EPOCH_COUNT, 15), (EE_SCENARIO_WX_TREE_END_EPOCH, 1)],
    );
    assert_eq!(
        ops(&seen)[3..],
        [EE_SCENARIO_WX_TREE_RESET_RANGE, EE_SCENARIO_WX_TECH_MAKE_AVAILABLE]
    );
    let (_, seen) = run(
        EE_SCENARIO_WORLD_TECH_ADD,
        [2, 0, 0, 0, 0, 0],
        &[(EE_SCENARIO_WX_EFFECT_TECH, 300), (EE_SCENARIO_WX_EPOCH_COUNT, 15), (EE_SCENARIO_WX_TECH_KIND, 4)],
    );
    assert!(ops(&seen).contains(&EE_SCENARIO_WX_TREE_WONDERS_ZERO));
    assert_eq!(*ops(&seen).last().unwrap(), EE_SCENARIO_WX_TECH_RESEARCH);
}

#[test]
fn a_placement_in_an_area_restores_the_objects_checks() {
    let (_, seen) = run(
        EE_SCENARIO_WORLD_AREA_PLACE,
        [8, 0, 0, 0, 0, 0],
        &[(EE_SCENARIO_WX_OBJ_IN_AREA_CHECK, 5), (EE_SCENARIO_WX_OBJ_AREA, 6), (EE_SCENARIO_WX_OBJ_NEAR_CHECK, 7)],
    );
    let tail: Vec<(u32, Vec<i64>)> = seen[seen.len() - 3..].to_vec();
    assert_eq!(
        tail,
        vec![
            (EE_SCENARIO_WX_OBJ_SET_IN_AREA_CHECK, vec![5]),
            (EE_SCENARIO_WX_OBJ_SET_AREA, vec![6]),
            (EE_SCENARIO_WX_OBJ_SET_NEAR_CHECK, vec![7]),
        ]
    );
}

#[test]
fn a_created_aircraft_faces_the_map_centre() {
    let mut seen = Vec::new();
    let mut cpp = |op: u32, args: [i64; 6]| {
        seen.push((op, args[0]));
        match op {
            EE_SCENARIO_WC_FIND_PLACEMENT | EE_SCENARIO_WC_AIR | EE_SCENARIO_WC_CREATE => 1,
            EE_SCENARIO_WC_MAP_SIZE => 100,
            EE_SCENARIO_WC_LOCATION_X => bits(50.0),
            EE_SCENARIO_WC_LOCATION_Y => bits(10.0),
            _ => 0,
        }
    };
    create_unit(&mut cpp, 1, 2, true, 0);
    let orient = seen.iter().find(|s| s.0 == EE_SCENARIO_WC_T_ORIENT).unwrap();
    // the centre is straight up the y axis: heading atan2(-0, 1)
    assert_eq!(float(orient.1), um_arctan2(-0.0, 1.0));
    assert!(seen.iter().any(|s| s.0 == EE_SCENARIO_WC_PUSH_CREATED));
}
