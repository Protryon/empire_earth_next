use super::*;
use crate::units::uc_mock::Mock;

#[test]
fn a_projectile_above_the_ground_has_not_hit() {
    // a projectile launched high, still above the ground: the test reads the ground and stops
    let mut h = Mock::default()
        .answer(EE_UNITS_UCS_IMPACT_FLIGHT_TIME, 2000)
        .float(EE_UNITS_UCS_START_Z, [10.0, 0.0, 0.0, 0.0])
        .float(EE_UNITS_UCS_POSITION, [4.5, 5.5, 3.0, 0.0])
        .answer(EE_UNITS_UCS_TILE_PRESENT, 1)
        .float(EE_UNITS_UCS_GROUND_POINT, [4.5, 5.5, 1.0, 0.0]);
    assert!(!projectile_ground(&mut h, 1, 2));
    assert_eq!(h.args(EE_UNITS_UCS_TILE_PRESENT), [(0, 4, 5)]);
    assert_eq!(h.args(EE_UNITS_UCS_CAPTURE_IMPACT)[0].1, 0);
    assert!(!h.has(EE_UNITS_UCS_ATTACK_GROUND));
}

#[test]
fn a_projectile_on_the_ground_attacks_it() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCS_IMPACT_FLIGHT_TIME, 2000)
        .float(EE_UNITS_UCS_START_Z, [10.0, 0.0, 0.0, 0.0])
        .float(EE_UNITS_UCS_POSITION, [4.5, 5.5, 0.5, 0.0])
        .answer(EE_UNITS_UCS_TILE_PRESENT, 1)
        .float(EE_UNITS_UCS_GROUND_POINT, [4.5, 5.5, 1.0, 0.0]);
    assert!(projectile_ground(&mut h, 1, 2));
    assert_eq!(h.floats_of(EE_UNITS_UCS_ATTACK_GROUND)[0], [4.5, 5.5, 1.0, 0.0]);
    assert!(h.at(EE_UNITS_UCS_CAPTURE_IMPACT) < h.at(EE_UNITS_UCS_ATTACK_GROUND));
}

#[test]
fn the_water_tests_are_captured_as_cbwi() {
    let mut h = Mock::default();
    missile_water(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UCS_CAPTURE_IMPACT)[0].1, 1);
    let mut h = Mock::default();
    projectile_water(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UCS_CAPTURE_IMPACT)[0].1, 1);
    let mut h = Mock::default();
    torpedo_ground(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UCS_CAPTURE_IMPACT)[0].1, 0);
}

#[test]
fn a_missile_without_a_goal_reads_its_record_in_order() {
    let mut h = Mock::default().answer(EE_UNITS_UCS_IMPACT_FLIGHT_TIME, 5);
    missile_ground(&mut h, 1, 2);
    let ops = h.ops();
    assert_eq!(
        &ops[..4],
        [
            EE_UNITS_UCS_GOAL_STATE,
            EE_UNITS_UCS_LOCATION,
            EE_UNITS_UCS_IMPACT_FLIGHT_TIME,
            EE_UNITS_UCS_LAUNCHER_CP_BASE
        ]
    );
    assert!(!h.has(EE_UNITS_UCS_GOAL_KIND));
}
