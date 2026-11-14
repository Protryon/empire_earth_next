use super::*;

const IDENTITY: [[f32; 4]; 4] = GeTransformation::IDENTITY.m;

fn part(angle: f32) -> EeUnitsTurretPart {
    EeUnitsTurretPart { position: [0.0, 0.0, 0.0], transform: IDENTITY, angle }
}

fn aim_at(x: f32, y: f32) -> EeUnitsTurretsIn {
    EeUnitsTurretsIn {
        aim: true,
        alive: true,
        turn_rate: 2.0,
        tick_seconds: 0.05,
        scale: 1.0,
        location: IDENTITY,
        target_x: x,
        target_y: y,
    }
}

#[test]
fn turning_back_stops_at_straight_ahead() {
    let input = EeUnitsTurretsIn { aim: false, ..aim_at(0.0, 0.0) };
    let mut parts = [part(0.25), part(-0.05), part(0.0), part(-0.3)];
    turn_turrets(&input, &mut parts);
    let angles: Vec<f32> = parts.iter().map(|p| p.angle).collect();
    assert_eq!(angles, vec![0.25 - 0.1, 0.0, 0.0, -0.3 + 0.1]);
}

#[test]
fn a_dead_unit_does_not_turn_back() {
    let input = EeUnitsTurretsIn { aim: false, alive: false, ..aim_at(0.0, 0.0) };
    let mut parts = [part(0.25)];
    turn_turrets(&input, &mut parts);
    assert_eq!(parts[0].angle, 0.25);
}

#[test]
fn a_target_straight_ahead_keeps_the_angle() {
    let mut parts = [part(0.0)];
    turn_turrets(&aim_at(0.0, 10.0), &mut parts);
    assert_eq!(parts[0].angle, 0.0);
}

#[test]
fn a_target_to_the_side_turns_by_the_step() {
    // +x is heading -pi/2 (UMArcTan2(-dx, dy)): one step of 2.0 * 0.05 toward it
    let mut parts = [part(0.0)];
    turn_turrets(&aim_at(10.0, 0.0), &mut parts);
    assert_eq!(parts[0].angle, -0.1);
    let mut parts = [part(0.0)];
    turn_turrets(&aim_at(-10.0, 0.0), &mut parts);
    assert_eq!(parts[0].angle, 0.1);
}

#[test]
fn a_small_turn_lands_on_the_heading() {
    // the angle left is under the step: the part turns all of it
    let mut parts = [part(0.0)];
    let input = EeUnitsTurretsIn { turn_rate: 100.0, ..aim_at(10.0, 0.0) };
    turn_turrets(&input, &mut parts);
    assert_eq!(parts[0].angle, um_arctan2(-1.0, 0.0));
}

#[test]
fn the_difference_wraps_the_short_way() {
    // facing just short of +pi, target just past -pi: the difference wraps and the turn crosses pi, then wraps back
    let mut parts = [part(3.1)];
    let input = EeUnitsTurretsIn { turn_rate: 2.0, ..aim_at(0.2, -10.0) };
    turn_turrets(&input, &mut parts);
    assert!(parts[0].angle < -3.0, "{}", parts[0].angle);
}

#[test]
fn the_unit_location_and_scale_move_the_part() {
    // a unit at (5, 5), its part 1 unit to its right scaled by 2: a target at (7, 9) is straight ahead of the part
    let mut location = IDENTITY;
    location[0][3] = 5.0;
    location[1][3] = 5.0;
    let input = EeUnitsTurretsIn { scale: 2.0, location, ..aim_at(7.0, 9.0) };
    let mut parts = [EeUnitsTurretPart { position: [1.0, 0.0, 0.0], ..part(0.0) }];
    turn_turrets(&input, &mut parts);
    assert_eq!(parts[0].angle, 0.0);
}

#[test]
fn the_ffi_turns_in_place_and_takes_no_parts() {
    let mut parts = [part(0.0)];
    let input = aim_at(10.0, 0.0);
    // SAFETY: valid pointers and count
    unsafe { ee_units_turn_turrets(&input, parts.as_mut_ptr(), 1) };
    assert_eq!(parts[0].angle, -0.1);
    // SAFETY: a count of 0 reads nothing
    unsafe { ee_units_turn_turrets(&input, std::ptr::null_mut(), 0) };
}
