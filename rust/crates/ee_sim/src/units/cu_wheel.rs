//! EEWheel.cpp's arithmetic (the units' lane, round 9): `EEWheel::Update`'s roll and `EEGetAttackFrameSet`'s choice,
//! over the values the C++ reads (each body one call).

/// `kMinimumWheelDistance`: the least share of the top speed or turn rate that rolls a wheel
const MINIMUM_WHEEL_DISTANCE: f32 = 0.1;
/// the original's pi (`3.1415927f`)
const PI: f32 = 3.141_592_7;

/// The unit's travel this tick that rolls the wheel.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeUnitsWheelIn {
    /// the wheel's `mAngle`
    pub angle: f32,
    /// the unit's `mPhysicalModel.GetScale()` and the wheel's `mRadius`
    pub scale: f32,
    pub radius: f32,
    /// the unit's `mCurrentPhysicsAction`
    pub mode: u32,
    /// the unit's `mVelocity` and `mAngularVelocity.mYaw`
    pub speed: f32,
    pub yaw: f32,
    /// the unit's stats' `GetInverseMaxVelocity()`, `GetInverseMaxAngularVelocity()`, `GetMaxVelocity()`
    pub inverse_max_velocity: f32,
    pub inverse_max_angular_velocity: f32,
    pub max_velocity: f32,
    /// `gPhysics->GetTickSeconds()`
    pub tick_seconds: f32,
}

/// `EEWheel::Update(unit)`: the wheel's new angle, rolled back by the unit's travel this tick (speed or turn rate,
/// whichever is the larger share of its top) over its scaled radius, in physics actions 0, 4 and 5 above a tenth of
/// the top; wrapped by a full turn. The angle unchanged otherwise.
pub fn wheel_angle(w: &EeUnitsWheelIn) -> f32 {
    let circumference = w.scale * w.radius;
    if !(circumference < -0.0001) && !(circumference > 0.0001) {
        return w.angle;
    }
    if w.mode != 0 && w.mode != 4 && w.mode != 5 {
        return w.angle;
    }
    let distance = w.inverse_max_velocity * if w.speed < 0.0 { -w.speed } else { w.speed };
    let mut turn = w.inverse_max_angular_velocity * if w.yaw < 0.0 { -w.yaw } else { w.yaw };
    if distance > turn {
        turn = distance;
    }
    if !(turn >= MINIMUM_WHEEL_DISTANCE) {
        return w.angle;
    }
    let mut angle = w.angle - w.tick_seconds * w.max_velocity * turn / circumference;
    while angle < -PI {
        angle += 2.0 * PI;
    }
    angle
}

/// `kUnitTypeX04RomanLegionnaire`, `kUnitTypeSp15CapitalShip`, `kUnitTypeCapitalShipYamato`
const SPECIAL_ATTACK_TYPES: [u32; 3] = [0x2dc, 0x2da, 0x33c];

/// `EEGetAttackFrameSet(unit)`: 0xb for the special attack of Roman Legionnaires and capital ships, 8 when it fires
/// high over walls, else 0.
pub fn attack_frame_set(special_attack: bool, unit_type: u32, fires_over_walls: bool, firing_high: bool) -> i32 {
    if special_attack && SPECIAL_ATTACK_TYPES.contains(&unit_type) {
        return 0xb;
    }
    if fires_over_walls && firing_high {
        return 8;
    }
    0
}

/// `EEWheel::Update`'s new angle (see [`wheel_angle`]).
#[no_mangle]
pub extern "C" fn ee_units_cu_wheel_angle(input: EeUnitsWheelIn) -> f32 {
    wheel_angle(&input)
}

/// `EEGetAttackFrameSet`'s choice (see [`attack_frame_set`]).
#[no_mangle]
pub extern "C" fn ee_units_cu_attack_frame_set(special_attack: bool, unit_type: u32, fires_over_walls: bool, firing_high: bool) -> i32 {
    attack_frame_set(special_attack, unit_type, fires_over_walls, firing_high)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn moving() -> EeUnitsWheelIn {
        EeUnitsWheelIn {
            angle: 0.5,
            scale: 2.0,
            radius: 0.25,
            mode: 0,
            speed: -3.0,
            yaw: 0.1,
            inverse_max_velocity: 0.25,
            inverse_max_angular_velocity: 1.0,
            max_velocity: 4.0,
            tick_seconds: 0.1,
        }
    }

    #[test]
    fn a_moving_wheel_rolls_back_by_its_share_of_the_top_speed() {
        let w = moving();
        // the speed's share 0.75 beats the turn's 0.1: 0.5 - 0.1 * 4 * 0.75 / 0.5
        assert_eq!(wheel_angle(&w), 0.5 - 0.1f32 * 4.0 * 0.75 / 0.5);
    }

    #[test]
    fn the_angle_wraps_by_a_full_turn() {
        let w = EeUnitsWheelIn { angle: -3.0, tick_seconds: 1.0, ..moving() };
        let a = wheel_angle(&w);
        assert!(a >= -PI && a < PI, "{a}");
        assert_eq!(a, -3.0f32 - 1.0 * 4.0 * 0.75 / 0.5 + 2.0 * PI);
    }

    #[test]
    fn no_roll_without_radius_in_other_modes_or_below_a_tenth() {
        assert_eq!(wheel_angle(&EeUnitsWheelIn { radius: 0.0, ..moving() }), 0.5);
        assert_eq!(wheel_angle(&EeUnitsWheelIn { mode: 2, ..moving() }), 0.5);
        assert_ne!(wheel_angle(&EeUnitsWheelIn { mode: 5, ..moving() }), 0.5);
        assert_eq!(wheel_angle(&EeUnitsWheelIn { speed: 0.2, yaw: 0.05, ..moving() }), 0.5);
    }

    #[test]
    fn attack_frame_sets() {
        assert_eq!(attack_frame_set(true, 0x2dc, true, true), 0xb);
        assert_eq!(attack_frame_set(true, 0x33c, false, false), 0xb);
        assert_eq!(attack_frame_set(true, 5, true, true), 8);
        assert_eq!(attack_frame_set(false, 0x2da, true, false), 0);
        assert_eq!(attack_frame_set(false, 0x2da, false, true), 0);
    }
}
