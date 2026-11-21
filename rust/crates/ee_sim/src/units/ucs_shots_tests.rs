use super::*;
use crate::units::uc_mock::Mock;

#[test]
fn a_projectile_starts_at_full_speed_advancing() {
    let mut h = Mock::default().float(EE_UNITS_UC_LOCATION, [1.0, 2.0, 3.0, 0.0]);
    projectile_created(&mut h, 1, 2);
    assert_eq!(h.floats_of(EE_UNITS_UCS_SET_CONTACT_LOCATION)[0][..3], [1.0, 2.0, 3.0]);
    let tail = &h.ops()[h.at(EE_UNITS_UCS_FULL_SPEED)..];
    assert_eq!(
        tail,
        [
            EE_UNITS_UCS_FULL_SPEED,
            EE_UNITS_UC_SET_ANIMATION,
            EE_UNITS_UC_START_ACTION_SELF,
            EE_UNITS_UC_CLASS_ACTION_CHANGED,
        ]
    );
    assert!(h.at(EE_UNITS_UC_CLASS_ACTION_CHANGED) > h.at(EE_UNITS_UC_SET_HEAL_SEARCH_TIMER));
}

#[test]
fn an_icbm_waits_stopped() {
    let mut h = Mock::default().answer(EE_UNITS_UCS_UNIT_TYPE, UNIT_TYPE_CP_ICBM);
    missile_created(&mut h, 1, 2);
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_UCS_UNIT_TYPE);
    assert!(!h.has(EE_UNITS_UC_START_ACTION_SELF));
}

#[test]
fn a_projectile_launches_at_the_ballistic_pitch() {
    let mut h =
        Mock::default().answer(EE_UNITS_UCS_BALLISTIC_PITCH, 1).float(EE_UNITS_UCS_LAUNCH_SPEED, [10.0, 0.0, 0.0, 0.0]);
    assert!(projectile_launch(&mut h, 1, 2, [5.0, 5.0, 0.0]));
    assert_eq!(h.args(EE_UNITS_UCS_SET_FLIGHT_TIME), [(0, 0, 0)]);
    // the identity faces y: yaw 0; the pitch 0 (the host wrote none): full launch speed
    assert_eq!(h.floats_of(EE_UNITS_UC_SET_VELOCITY_OF)[0][0], 10.0);
    assert!(h.has(EE_UNITS_UC_UPDATE_UNIT_TILE));
}

#[test]
fn a_bomb_drops_pitched_down_by_its_tilt() {
    let mut h = Mock::default()
        .float(EE_UNITS_UCS_UPGRADE_LAUNCH_SPEED, [4.0, 0.0, 0.0, 0.0])
        .float(EE_UNITS_UCS_LAUNCH_PITCH, [-0.25, 0.0, 0.0, 0.0]);
    assert!(bomb_launch(&mut h, 1, 2));
    let pitch = h.floats_of(EE_UNITS_UC_SET_ORIENTATION)[0][1];
    assert_eq!(pitch, um_arccos(-0.0) - HALF_PI - 0.25);
    assert_eq!(h.floats_of(EE_UNITS_UC_SET_VELOCITY_OF)[0][0], um_cos(pitch) * 4.0);
}

#[test]
fn a_missile_waits_its_launch_delay() {
    let mut h = Mock::default();
    assert!(missile_launch(&mut h, 1, 2));
    assert_eq!(
        h.ops()[h.at(EE_UNITS_UC_UPDATE_UNIT_TILE)..],
        [EE_UNITS_UC_UPDATE_UNIT_TILE, EE_UNITS_UCS_SET_LAUNCH_DELAY, EE_UNITS_UCS_SET_LIFETIME, EE_UNITS_UC_STOP]
    );
}

#[test]
fn a_still_projectile_leaves_no_trail() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_HIT_POINTS, 1)
        .answer(EE_UNITS_UCS_HAS_TRAIL, 1)
        .answer(EE_UNITS_UCS_TRAIL_DETAILED, 1);
    projectile_effects(&mut h, 1, 2);
    assert!(!h.has(EE_UNITS_UC_TRAIL_TIMER));
    let mut h = Mock::default();
    projectile_effects(&mut h, 1, 2);
    assert_eq!(h.ops(), [EE_UNITS_UC_HIT_POINTS]);
}

#[test]
fn an_sas_bomb_blinks_its_target() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCS_UNIT_TYPE, UNIT_TYPE_BOMB_FOR_SAS_EXPLOSIVE_EXPERT)
        .answer(EE_UNITS_UC_GOAL_STATE_SELF, 5)
        .answer(EE_UNITS_UC_GOAL_KIND, GOAL_ATTACK_UNIT)
        .answer(EE_UNITS_UCS_COMPLEX_CAST, 8)
        .answer(EE_UNITS_UCS_FLIGHT_TIME, 1700);
    bomb_update(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UCS_SET_TINTED), [(8, 1, 0)]);
    assert!(h.has(EE_UNITS_UCS_SET_BLINK_COLOR));
    assert_eq!(*h.ops().last().unwrap(), EE_UNITS_UCS_PROJECTILE_UPDATE);
}

#[test]
fn a_carrier_drops_its_first_bomb() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UC_CARGO_COUNT, 2)
        .answer(EE_UNITS_UCS_FIND_DROP, 30)
        .answer(EE_UNITS_UCS_FIRST_CARGO, 40);
    launch_stored(&mut h, 1, 2);
    assert_eq!(h.args(EE_UNITS_UCS_LAUNCH_SHOT), [(30, 40, 0)]);
    assert_eq!(h.args(EE_UNITS_UC_START_ACTION), [(40, ACTION_ADVANCE, 0)]);
}

#[test]
fn a_unit_on_a_large_targets_footprint_is_on_it() {
    let mut h = Mock::default()
        .answer(EE_UNITS_UCS_COMPLEX_FIELD, 50)
        .answer(EE_UNITS_UCS_TILE_OF, 7)
        .answer(EE_UNITS_UCS_TILE_SIZE_OF, 2)
        .sequence(EE_UNITS_UC_GET_TILE, &[1, 2, 7]);
    assert!(on_target_tiles(&mut h, 2, 3));
    assert_eq!(h.count(EE_UNITS_UC_GET_TILE), 3);
    let mut h = Mock::default().sequence(EE_UNITS_UCS_TILE_OF, &[7, 8]);
    assert!(!on_target_tiles(&mut h, 2, 3));
}
