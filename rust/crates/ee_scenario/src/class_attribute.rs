//! A class attribute's new value (`EEModifyUnitAttribute`, the Class-Attribute effect and its pending replay): a
//! player's upgrade value raised or lowered by an amount or a percent of the class's base value, clamped, by the
//! value's kind. The C++ picks the upgrade's field and the class's base by attribute and writes the answer back.

use std::os::raw::c_long;

use crate::aivar::lrintf;
use crate::ftol;

/// A `long` value clamped at 0 (reload time, work rate, objects carried, repair, carry amount, healing, power).
pub const EE_SCENARIO_CA_INT: u32 = 0;
/// The maximum hit points: as an int, then at least 1.
pub const EE_SCENARIO_CA_HIT_POINTS: u32 = 1;
/// The flight time: a fixed amount is in seconds (`(long)(amount / 0.001f + 0.5f)`).
pub const EE_SCENARIO_CA_FLIGHT_TIME: u32 = 2;
/// A `float` value clamped at 0 (attack, leadership, range, line of sight).
pub const EE_SCENARIO_CA_FLOAT: u32 = 3;
/// An armor: the bonus over the class's base changes, clamped at 0, and goes back on top of the base.
pub const EE_SCENARIO_CA_ARMOR: u32 = 4;
/// The maximum velocity: a percent unrounded, a fixed amount / 20, at least 0.0001.
pub const EE_SCENARIO_CA_VELOCITY: u32 = 5;
/// The animal AI flag (0/1; a percent amount is "above 0").
pub const EE_SCENARIO_CA_FLAG: u32 = 6;
/// The camera focus: in hundredths, no percent.
pub const EE_SCENARIO_CA_CAMERA: u32 = 7;

/// The new value: `value` for the int kinds and the flag, `fvalue` for the float kinds.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScenarioClassValue {
    pub value: c_long,
    pub fvalue: f32,
}

/// The new upgrade value of kind `kind` from the current one (`current` / `fcurrent`, by kind) and the class's base
/// (`base`, as the C++ converts it to `float`), raised (`add`) or lowered by `amount` (a percent of the base when
/// `percent`), each step as the original's arithmetic.
pub fn class_value(
    kind: u32,
    percent: bool,
    add: bool,
    amount: c_long,
    base: f32,
    current: c_long,
    fcurrent: f32,
) -> EeScenarioClassValue {
    let scaled = |a: c_long| lrintf(base * a as f32 * 0.01f32) as c_long;
    let mut out = EeScenarioClassValue::default();
    match kind {
        EE_SCENARIO_CA_INT | EE_SCENARIO_CA_HIT_POINTS | EE_SCENARIO_CA_FLIGHT_TIME => {
            let mut a = if percent {
                scaled(amount)
            } else if kind == EE_SCENARIO_CA_FLIGHT_TIME {
                ftol(amount as f32 / 0.001f32 + 0.5f32) as c_long
            } else {
                amount
            };
            if !add {
                a = a.wrapping_neg();
            }
            a = a.wrapping_add(current);
            if a < 0 {
                a = 0;
            }
            if kind == EE_SCENARIO_CA_HIT_POINTS && a <= 1 {
                a = 1;
            }
            out.value = a;
        }
        EE_SCENARIO_CA_FLOAT | EE_SCENARIO_CA_ARMOR => {
            let a = if percent { scaled(amount) } else { amount };
            let mut f = a as f32;
            if !add {
                f = -f;
            }
            f = if kind == EE_SCENARIO_CA_ARMOR { (fcurrent - base) + f } else { fcurrent + f };
            if f < 0.0 {
                f = 0.0;
            }
            out.fvalue = if kind == EE_SCENARIO_CA_ARMOR { f + base } else { f };
        }
        EE_SCENARIO_CA_VELOCITY => {
            let mut f = if percent { base * amount as f32 * 0.01f32 } else { amount as f32 / 20.0f32 };
            if !add {
                f = -f;
            }
            f += fcurrent;
            if f < 0.0001f32 {
                f = 0.0001f32;
            }
            out.fvalue = f;
        }
        EE_SCENARIO_CA_FLAG => {
            let mut a = if percent { (amount > 0) as c_long } else { amount };
            if !add {
                a = a.wrapping_neg();
            }
            a = a.wrapping_add((current != 0) as c_long);
            if a < 0 {
                a = 0;
            }
            out.value = (a > 0) as c_long;
        }
        EE_SCENARIO_CA_CAMERA => {
            let a = if add { amount } else { amount.wrapping_neg() };
            let mut t = a.wrapping_add(ftol(fcurrent * 100.0f32) as c_long);
            if t < 0 {
                t = 0;
            }
            out.fvalue = t as f32 * 0.01f32;
        }
        _ => {}
    }
    out
}

/// `EEChangeUnit`'s amount for one unit value: a percent of `base` (rounded as `UMFloatToLong`) when `percent`, then
/// added to (`mode` 0) or taken from (1) `current`, or the amount itself (2, exactly); with `clamp`, at least 0.
pub fn unit_amount(percent: bool, mode: c_long, amount: c_long, base: f32, current: c_long, clamp: bool) -> c_long {
    let mut a = if percent { lrintf(base * amount as f32 * 0.01f32) as c_long } else { amount };
    if mode == 0 {
        a = a.wrapping_add(current);
    } else if mode == 1 {
        a = current.wrapping_sub(a);
    }
    if clamp && a < 0 {
        a = 0;
    }
    a
}

#[cfg(test)]
mod tests {
    use super::*;

    fn int(kind: u32, percent: bool, add: bool, amount: c_long, base: f32, current: c_long) -> c_long {
        class_value(kind, percent, add, amount, base, current, 0.0).value
    }

    #[test]
    fn unit_amounts_by_mode() {
        assert_eq!(unit_amount(false, 0, 5, 0.0, 10, true), 15);
        assert_eq!(unit_amount(false, 1, 15, 0.0, 10, true), 0);
        assert_eq!(unit_amount(false, 1, 15, 0.0, 10, false), -5);
        assert_eq!(unit_amount(true, 2, 50, 30.0, 10, true), 15);
    }

    #[test]
    fn ints_add_subtract_and_clamp() {
        assert_eq!(int(EE_SCENARIO_CA_INT, false, true, 5, 0.0, 10), 15);
        assert_eq!(int(EE_SCENARIO_CA_INT, false, false, 50, 0.0, 10), 0);
        // 10 % of 125 is 12.5: rounded half to even
        assert_eq!(int(EE_SCENARIO_CA_INT, true, true, 10, 125.0, 100), 112);
        assert_eq!(int(EE_SCENARIO_CA_HIT_POINTS, false, false, 100, 0.0, 50), 1);
    }

    #[test]
    fn a_fixed_flight_time_is_in_seconds() {
        assert_eq!(int(EE_SCENARIO_CA_FLIGHT_TIME, false, true, 3, 0.0, 1000), 3000 + 1000);
        assert_eq!(int(EE_SCENARIO_CA_FLIGHT_TIME, true, true, 50, 2000.0, 0), 1000);
    }

    #[test]
    fn floats_and_armor() {
        assert_eq!(class_value(EE_SCENARIO_CA_FLOAT, false, false, 3, 0.0, 0, 2.5).fvalue, 0.0);
        assert_eq!(class_value(EE_SCENARIO_CA_FLOAT, true, true, 50, 7.0, 0, 7.0).fvalue, 11.0);
        // armor 5 over a base of 3: lowered by 4, the bonus 2 clamps at 0, back on the base
        assert_eq!(class_value(EE_SCENARIO_CA_ARMOR, false, false, 4, 3.0, 0, 5.0).fvalue, 3.0);
        assert_eq!(class_value(EE_SCENARIO_CA_ARMOR, false, true, 4, 3.0, 0, 5.0).fvalue, 9.0);
    }

    #[test]
    fn velocity_flag_and_camera() {
        assert_eq!(class_value(EE_SCENARIO_CA_VELOCITY, false, true, 10, 0.0, 0, 1.0).fvalue, 1.0 + 10.0 / 20.0);
        assert_eq!(class_value(EE_SCENARIO_CA_VELOCITY, false, false, 100, 0.0, 0, 1.0).fvalue, 0.0001);
        assert_eq!(class_value(EE_SCENARIO_CA_FLAG, true, true, 7, 0.0, 0, 0.0).value, 1);
        assert_eq!(class_value(EE_SCENARIO_CA_FLAG, false, false, 1, 0.0, 1, 0.0).value, 0);
        assert_eq!(class_value(EE_SCENARIO_CA_CAMERA, false, true, 25, 0.0, 0, 0.5).fvalue, 75.0 * 0.01);
    }
}
