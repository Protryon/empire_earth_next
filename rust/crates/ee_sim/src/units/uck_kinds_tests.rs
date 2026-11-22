use super::*;
use crate::units::uc_mock::Mock;

#[test]
fn action_changed_runs_its_kind_or_the_shots_members() {
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 13);
    action_changed(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UCK_ACTION_CHANGED), [(0, i64::from(EE_UNITS_ACTION_LAND), 0)]);
    assert_eq!(h.args(EE_UNITS_UCK_BODY), [(0, EE_UNITS_BODY_ACTION_CHANGED as i64, 0)]);
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 17);
    action_changed(&mut h, 1, 2);
    assert!(h.has(EE_UNITS_UCK_PROJECTILE_ACTION_CHANGED));
    assert!(!h.has(EE_UNITS_UCK_ACTION_CHANGED));
}

#[test]
fn update_runs_a_sequence_or_a_member() {
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 22);
    update(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UCK_UPDATE), [(0, i64::from(EE_UNITS_UPDATE_NAVAL), 0)]);
    // the temple's priest update only once built, dead or in physics action 2
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 4);
    update(&mut h, 1, 2);
    assert!(!h.has(EE_UNITS_UCK_PRIEST_UPDATE));
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 4).answer(EE_UNITS_UCK_PRIEST_UPDATES, 1);
    update(&mut h, 1, 2);
    assert!(h.has(EE_UNITS_UCK_PRIEST_UPDATE));
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 0);
    update(&mut h, 1, 2);
    assert_eq!(h.ops(), [EE_UNITS_UCK_BODY]);
}

#[test]
fn effects_run_on_their_conditions() {
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 4).answer(EE_UNITS_UCK_RIPPLES_DUE_4, 1);
    update_effects(&mut h, 1, 2);
    assert_eq!(h.ops(), [EE_UNITS_UCK_BODY, EE_UNITS_UCK_RIPPLES_DUE_4, EE_UNITS_UCK_UPDATE_RIPPLES]);
    let mut h = Mock::default().answer(EE_UNITS_UCK_BODY, 6);
    update_effects(&mut h, 1, 2);
    assert!(!h.has(EE_UNITS_UCK_UPDATE_TRAIL));
}

#[test]
fn the_shots_answer_their_own_conditions() {
    let mut h = Mock::default().answer(EE_UNITS_UCK_CONDITION, -2).answer(EE_UNITS_UCK_MISSILE_CONDITION, 7);
    assert_eq!(evaluate_condition(&mut h, 1, 2, 30), 7);
    assert_eq!(h.args(EE_UNITS_UCK_MISSILE_CONDITION), [(0, 30, 0)]);
    let mut h = Mock::default().answer(EE_UNITS_UCK_CONDITION, 1);
    assert_eq!(evaluate_condition(&mut h, 1, 2, 30), 1);
}

#[test]
fn arrival_is_the_deepest_kinds_test() {
    let mut h = Mock::default().answer(EE_UNITS_UCK_KINDS, (EE_CLASS_KIND_AIR_ANIMAL | EE_CLASS_KIND_LANDABLE) as i64);
    has_arrived(&mut h, 1, 2, 0.5);
    assert_eq!(h.args(EE_UNITS_UCK_LANDING_ARRIVED), [(0, 1, 0)]);
    assert_eq!(h.floats_of(EE_UNITS_UCK_LANDING_ARRIVED)[0][0], 0.5);
    let mut h =
        Mock::default().answer(EE_UNITS_UCK_KINDS, EE_CLASS_KIND_NAVAL as i64).answer(EE_UNITS_UCK_NAVAL_ARRIVED, 1);
    assert!(has_arrived(&mut h, 1, 2, 0.5));
    let mut h = Mock::default();
    has_arrived(&mut h, 1, 2, 0.5);
    assert!(h.has(EE_UNITS_UCK_BASE_ARRIVED));
}

#[test]
fn transports_have_room_and_buildings_release() {
    let mut h = Mock::default().answer(EE_UNITS_UCK_KINDS, EE_CLASS_KIND_LAND_TRANSPORT as i64);
    assert!(has_room(&mut h, 1, 2, 3));
    let mut h = Mock::default().answer(EE_UNITS_UCK_KINDS, EE_CLASS_KIND_FORTRESS as i64);
    assert!(!has_room(&mut h, 1, 2, 3));
    assert_eq!(h.args(EE_UNITS_UCK_FORTRESS_HAS_ROOM), [(3, 0, 0)]);
    let mut h = Mock::default()
        .answer(EE_UNITS_UCK_KINDS, (EE_CLASS_KIND_BUILDING | EE_CLASS_KIND_NAVAL_BUILDING) as i64)
        .answer(EE_UNITS_UCK_NAVAL_SELECT, 1);
    assert!(select_to_release(&mut h, 1, 2));
    assert!(!h.has(EE_UNITS_UCK_BUILDING_SELECT));
    assert!(!select_to_release(&mut Mock::default(), 1, 2));
}

#[test]
fn boarding_and_surrounding_tiles_by_kind() {
    let mut h = Mock::default();
    assert!(!boarding_location(&mut h, 1, 2, 3, 4, 5));
    let mut h = Mock::default()
        .answer(EE_UNITS_UCK_KINDS, EE_CLASS_KIND_TRANSPORT as i64)
        .answer(EE_UNITS_UCK_BOARDING_LOCATION, 1);
    assert!(boarding_location(&mut h, 1, 2, 3, 4, 5));
    assert_eq!(h.args(EE_UNITS_UCK_BOARDING_LOCATION), [(3, 4, 5)]);
    let mut h = Mock::default().answer(EE_UNITS_UCK_KINDS, (EE_CLASS_KIND_GATE | EE_CLASS_KIND_BUILDING) as i64);
    surrounding_tiles(&mut h, 1, 2, 9, 1, 0);
    assert_eq!(h.args(EE_UNITS_UCK_GATE_SURROUNDING), [(9, 1, 0)]);
}
