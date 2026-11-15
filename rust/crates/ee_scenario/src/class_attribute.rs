//! A class attribute's change (`EEModifyUnitAttribute`, the Class-Attribute effect and its pending replay): a
//! player's upgrade value raised or lowered by an amount or a percent of the class's base value, clamped, by the
//! value's kind, then recorded as a pending effect. Which value and kind an attribute names and the arithmetic are
//! here; the upgrade's and class's fields are the world host's `EE_SCENARIO_WORLD_CA_*` (EESMEffect.cpp).

use std::os::raw::{c_long, c_ulong};

use crate::aivar::lrintf;
use crate::ftol;
use crate::host::{Host, EE_SCENARIO_READ_CAPTURE_SEED};

/// Whether player a has an upgrade of unit type b (`GetUpgrade(b) != NULL`; 1/0).
pub const EE_SCENARIO_WORLD_CA_UPGRADE: u32 = 193;
/// Player a's upgrade of type b: attribute c's current value (a `long`, a `float`'s bits, the animal AI flag).
pub const EE_SCENARIO_WORLD_CA_VALUE: u32 = 194;
/// Player a's upgrade of type b: attribute c's class base as the percent reads it (a `float`'s bits).
pub const EE_SCENARIO_WORLD_CA_BASE: u32 = 195;
/// Player a's upgrade of type b: attribute c set to d (a `long` or a `float`'s bits; the range and velocity through
/// their setters, the line of sight between its units' sight off and on, the animal AI flag into the herds too).
pub const EE_SCENARIO_WORLD_CA_SET: u32 = 196;
/// `AddV134Effect` of the Unit-Attribute effect for player a's upgrade of type b: attribute c, percent d, amount e,
/// mode f.
pub const EE_SCENARIO_WORLD_CA_PENDING: u32 = 197;
/// TEST ONLY: the SMCU record (kind 1) for player a (-1 none), type b (-1 no upgrade), attribute c (the percent flag
/// in bit 32), amount d, mode e, the seed before f.
pub const EE_SCENARIO_WORLD_CA_CAPTURE: u32 = 198;

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

/// The attribute's kind (`EE_SCENARIO_CA_*`), `None` for an attribute that changes nothing (only its pending effect).
fn kind_of(attribute: c_ulong) -> Option<u32> {
    Some(match attribute {
        20..=25 => EE_SCENARIO_CA_ARMOR,
        26 | 28 | 31 | 32 | 34 | 35 | 39 => EE_SCENARIO_CA_INT,
        27 | 30 | 36 | 37 => EE_SCENARIO_CA_FLOAT,
        29 => EE_SCENARIO_CA_FLIGHT_TIME,
        33 => EE_SCENARIO_CA_HIT_POINTS,
        38 => EE_SCENARIO_CA_VELOCITY,
        40 => EE_SCENARIO_CA_FLAG,
        41 => EE_SCENARIO_CA_CAMERA,
        _ => return None,
    })
}

/// `EEModifyUnitAttribute(attribute, percent, player, its upgrade of unit_type, amount, mode)`: `player` -1 for none,
/// `unit_type` -1 for no upgrade (nothing changes, no pending effect).
pub fn modify_unit_attribute<H: Host>(
    h: &mut H,
    player: i64,
    unit_type: i64,
    attribute: c_ulong,
    percent: bool,
    amount: c_long,
    mode: c_long,
) {
    let seed = h.read(EE_SCENARIO_READ_CAPTURE_SEED, 0, 0);
    let upgrade =
        player >= 0 && unit_type >= 0 && h.world(EE_SCENARIO_WORLD_CA_UPGRADE, [player, unit_type, 0, 0, 0, 0]) != 0;
    if upgrade {
        let at = [player, unit_type, attribute as i64];
        if let Some(kind) = kind_of(attribute) {
            let current = h.world(EE_SCENARIO_WORLD_CA_VALUE, [at[0], at[1], at[2], 0, 0, 0]);
            let base = if kind == EE_SCENARIO_CA_FLAG || kind == EE_SCENARIO_CA_CAMERA {
                0.0
            } else {
                f32::from_bits(h.world(EE_SCENARIO_WORLD_CA_BASE, [at[0], at[1], at[2], 0, 0, 0]) as u32)
            };
            let fcurrent = f32::from_bits(current as u32);
            let v = class_value(kind, percent, mode == 0, amount, base, current as c_long, fcurrent);
            let written = match kind {
                EE_SCENARIO_CA_INT | EE_SCENARIO_CA_HIT_POINTS | EE_SCENARIO_CA_FLIGHT_TIME | EE_SCENARIO_CA_FLAG => {
                    v.value as i64
                }
                _ => v.fvalue.to_bits() as i64,
            };
            h.world(EE_SCENARIO_WORLD_CA_SET, [at[0], at[1], at[2], written, 0, 0]);
        }
        let args = [player, unit_type, attribute as i64, percent as i64, amount as i64, mode as i64];
        h.world(EE_SCENARIO_WORLD_CA_PENDING, args);
    }
    if seed >= 0 {
        let attr = attribute as i64 | (percent as i64) << 32;
        h.world(
            EE_SCENARIO_WORLD_CA_CAPTURE,
            [player, if upgrade { unit_type } else { -1 }, attr, amount as i64, mode as i64, seed],
        );
    }
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

    /// An upgrade's values by attribute and the class's bases; logs the sets and pending effects.
    #[derive(Default)]
    struct World {
        values: std::collections::BTreeMap<i64, i64>,
        bases: std::collections::BTreeMap<i64, f32>,
        log: Vec<String>,
        no_upgrade: bool,
    }

    impl Host for World {
        fn read4(&mut self, op: u32, _a: i32, _b: i32, _c: i32, _d: i32) -> i64 {
            assert_eq!(op, EE_SCENARIO_READ_CAPTURE_SEED);
            -1
        }
        fn world(&mut self, op: u32, a: [i64; 6]) -> i64 {
            match op {
                EE_SCENARIO_WORLD_CA_UPGRADE => !self.no_upgrade as i64,
                EE_SCENARIO_WORLD_CA_VALUE => *self.values.get(&a[2]).unwrap_or(&0),
                EE_SCENARIO_WORLD_CA_BASE => self.bases.get(&a[2]).copied().unwrap_or(0.0).to_bits() as i64,
                EE_SCENARIO_WORLD_CA_SET => {
                    self.log.push(format!("set {} {}", a[2], a[3]));
                    0
                }
                EE_SCENARIO_WORLD_CA_PENDING => {
                    self.log.push(format!("pending {:?}", a));
                    0
                }
                _ => panic!("op {op}"),
            }
        }
    }

    #[test]
    fn a_class_attribute_by_its_kind_then_pending() {
        let mut w = World::default();
        w.values.insert(26, 1000);
        w.bases.insert(26, 2000.0);
        modify_unit_attribute(&mut w, 2, 7, 26, true, 10, 0);
        assert_eq!(w.log, ["set 26 1200", "pending [2, 7, 26, 1, 10, 0]"]);
        let mut w = World::default();
        w.values.insert(36, 4.0f32.to_bits() as i64);
        modify_unit_attribute(&mut w, 2, 7, 36, false, 1, 1);
        assert_eq!(w.log, [format!("set 36 {}", 3.0f32.to_bits()), "pending [2, 7, 36, 0, 1, 1]".to_string()]);
        // no change for an attribute of no kind, only its pending effect; nothing without an upgrade
        let mut w = World::default();
        modify_unit_attribute(&mut w, 2, 7, 5, false, 1, 0);
        assert_eq!(w.log, ["pending [2, 7, 5, 0, 1, 0]"]);
        let mut w = World { no_upgrade: true, ..Default::default() };
        modify_unit_attribute(&mut w, 2, 7, 26, false, 1, 0);
        modify_unit_attribute(&mut w, -1, 7, 26, false, 1, 0);
        assert!(w.log.is_empty());
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
