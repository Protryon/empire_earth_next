//! The dry host's log over the mock host (its reads): the words the C++ dry host wrote, in its order.

use super::*;
use crate::units::host::tests::{Call, MockHost};
use crate::units::host::{EE_GOAL_CTOR_EXPLORE, EE_GOAL_CTOR_HEAL_UNIT};

fn dry() -> DryHost<MockHost> {
    DryHost { game: MockHost::default() }
}

fn player() -> HostRef {
    HostRef(7 as *mut c_void)
}

#[test]
fn begin_and_end_frame_the_run() {
    assert!(!active());
    let selection = begin(&[5, 6]);
    assert!(active());
    let mut h = dry();
    let mut ids = Vec::new();
    h.group_ids(selection, &mut ids);
    assert_eq!(ids, vec![5, 6]);
    assert_eq!(h.player_selection(player()), selection);
    let (log, mismatches) = end();
    assert_eq!((log, mismatches), (vec![PLAYER_SELECTION, 7], 0));
    assert!(!active());
}

#[test]
fn groups_get_tags_and_their_calls_log_them() {
    let selection = begin(&[]);
    let mut h = dry();
    let g = h.active_group_new(player());
    assert_ne!(g, selection);
    h.group_set_action(g, 3);
    h.group_plan(g);
    h.pending_push(player(), g);
    h.group_add_unit(g, 0xffff_fff0);
    let mut ids = Vec::new();
    h.group_ids(g, &mut ids);
    // a unit no id resolves is kept as none
    assert_eq!(ids, vec![EE_UNIT_ID_NONE]);
    h.group_remove_unit(g, 0xffff_fff0);
    h.group_ids(g, &mut ids);
    assert!(ids.is_empty());
    let (log, _) = end();
    assert_eq!(
        log,
        vec![
            NEW_GROUP,
            1,
            SET_ACTION,
            1,
            3,
            PLAN,
            1,
            PENDING,
            7,
            1,
            ADD_UNIT,
            1,
            0xffff_fff0,
            REMOVE_UNIT,
            1,
            0xffff_fff0
        ]
    );
    assert!(h.game.calls.is_empty(), "no world call");
}

#[test]
fn goals_are_records_of_their_arguments() {
    begin(&[]);
    let mut h = dry();
    let mut spec = EeGoalSpec::new(EE_GOAL_CTOR_EXPLORE);
    spec.flag_a = 1;
    let goal = h.goal_new(&spec).expect("an explore goal is made");
    assert_eq!(h.goal_kind(&goal), 10);
    let clone = h.goal_clone(&goal).unwrap();
    assert_ne!(clone, goal);
    h.goal_release(clone);
    // a heal goal of a unit no id resolves is not made
    let heal = EeGoalSpec::new(EE_GOAL_CTOR_HEAL_UNIT);
    assert!(h.goal_new(&heal).is_none());
    let (log, _) = end();
    let none = EE_UNIT_ID_NONE;
    let mut expected = vec![LOG_GOAL_NEW, EE_GOAL_CTOR_EXPLORE, none, none, 0, 0, 0, 1, 0, 0, none, 1];
    expected.extend([CLONE, EE_GOAL_CTOR_EXPLORE, LOG_GOAL_RELEASE, EE_GOAL_CTOR_EXPLORE]);
    expected.extend([LOG_GOAL_NEW, EE_GOAL_CTOR_HEAL_UNIT, none, none, 0, 0, 0, 0, 0, 0, none, 0]);
    assert_eq!(log, expected);
}

#[test]
fn the_draws_walk_the_range_without_drawing() {
    begin(&[]);
    let mut h = dry();
    assert_eq!([h.random(2, 4), h.random(2, 4), h.random(2, 4), h.random(2, 4)], [2, 3, 4, 2]);
    assert_eq!(h.random(5, 1), 5);
    let (log, _) = end();
    assert_eq!(&log[..4], &[RANDOM, 2, 4, 2]);
    assert_eq!(&log[log.len() - 4..], &[RANDOM, 5, 1, 5]);
    assert!(h.game.calls.is_empty());
}

#[test]
fn reads_go_to_the_game_and_are_logged() {
    begin(&[]);
    let mut h = dry();
    h.game.height = 2.5;
    h.game.town_point = Some(42);
    assert_eq!(h.map_height(1.0, 2.0), 2.5);
    assert_eq!(h.town_patrol_point(9), Some(42));
    let (log, _) = end();
    assert_eq!(log, vec![MAP_HEIGHT, 1.0f32.to_bits(), 2.0f32.to_bits(), 2.5f32.to_bits(), TOWN_POINT, 9, 1, 42]);
    assert_eq!(h.game.calls, vec![Call::MapHeight(1.0f32.to_bits(), 2.0f32.to_bits()), Call::TownPoint(9)]);
}

#[test]
fn the_dry_answers_never_ask_the_game() {
    begin(&[]);
    let mut h = dry();
    assert!(h.can_produce(player(), 12));
    assert_eq!(h.make_building(player(), 3, 4, 5), DRY_BUILDING);
    let mut farms = Vec::new();
    h.free_farms(EE_UNIT_ID_NONE, &mut farms);
    assert_eq!(farms, DRY_FARMS.to_vec());
    assert_eq!(h.wall_plan(player(), 1, (0.0, 0.0), (1.0, 1.0)).1, 2);
    let (log, _) = end();
    assert_eq!(&log[..5], &[CAN_PRODUCE, 7, 12, MAKE_BUILDING, 7]);
    assert_eq!(&log[8..13], &[FREE_FARMS, EE_UNIT_ID_NONE, 2, DRY_FARMS[0], DRY_FARMS[1]]);
    assert!(h.game.calls.is_empty());
}

#[test]
fn a_load_goal_the_dry_host_made_reads_its_carrier() {
    begin(&[]);
    let mut h = dry();
    let goal = h.load_goal_new(12, 8, &[1, 2], true);
    assert_eq!(h.load_goal_fields(goal), (true, 8, 9));
    let odd = h.load_goal_new(12, 9, &[], false);
    assert!(!h.load_goal_fields(odd).0);
    let (log, _) = end();
    assert_eq!(&log[..8], &[NEW_LOAD, 12, 8, 1, 2, 1, 2, LOAD_FIELDS]);
    assert_eq!(&log[8..12], &[CTOR_NEW_LOAD, 1, 8, 9]);
}

#[test]
fn a_keep_counts_the_disagreements_of_the_two_tests() {
    // the store holds none of the units: its test keeps none, the objects' (the tests' reads) keep every one
    let selection = begin(&[0x5eed_0003, 0x5eed_0004]);
    let mut h = dry();
    h.group_keep_ground_attackers(selection);
    let mut ids = Vec::new();
    h.group_ids(selection, &mut ids);
    assert!(ids.is_empty());
    let (log, mismatches) = end();
    assert_eq!((log, mismatches), (vec![KEEP_GROUND, 0, 0], 2));
}
