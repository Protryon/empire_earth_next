//! `EEChangeUnit`: one unit's attribute (hit points, a resource, flight time, garrison, ammunition, animation pause,
//! reload time, scale, power, rotting time, construction, the three unit variables) raised, lowered or set by a fixed,
//! random or percent amount (the Unit-Attribute effect, a Create effect's attribute check). The random draws, the
//! clamps and which value changes are here; the unit's reads and writes are the world's calls on the C++ context's
//! unit (`EE_SCENARIO_WORLD_CU_*`, answered in `Scenario Manager/EESMEffect.cpp`).

use std::os::raw::{c_long, c_ulong};

use crate::aivar::lrintf;
use crate::class_attribute::unit_amount;
use crate::ftol;
use crate::host::Host;

/// `gPhysics->GetRandom(a, b)` (both inclusive; the physics stream).
pub const EE_SCENARIO_WORLD_CU_RANDOM: u32 = 190;
/// The context unit's value `a` (`EE_SCENARIO_CU_*`; floats as their bits).
pub const EE_SCENARIO_WORLD_CU_VALUE: u32 = 191;
/// The context unit's value `a` set to `b` (`EE_SCENARIO_CU_*`, the writable ones; the C++ statement of each).
pub const EE_SCENARIO_WORLD_CU_SET: u32 = 192;

/// `inUnit != NULL`.
pub const EE_SCENARIO_CU_EXISTS: i64 = 0;
/// `GetComplexUnit() != NULL`.
pub const EE_SCENARIO_CU_COMPLEX: i64 = 1;
/// `GetHitPoints()`; set: `SetHitPoints(b)`.
pub const EE_SCENARIO_CU_HIT_POINTS: i64 = 2;
/// `GetStats()->mMaxHitPoints`.
pub const EE_SCENARIO_CU_MAX_HIT_POINTS: i64 = 3;
/// `GetUnitClass()->GetCategory()`.
pub const EE_SCENARIO_CU_CATEGORY: i64 = 4;
/// `GetStats()->mCarryAmount`.
pub const EE_SCENARIO_CU_CARRY: i64 = 5;
/// `mResourceAmount`; set: `SetResourceAmount(b)`.
pub const EE_SCENARIO_CU_RESOURCE_AMOUNT: i64 = 6;
/// Whether the complex unit gathers (`mCommand != NULL && mCommand->mType == kActionGatherResource`; 0 without one).
pub const EE_SCENARIO_CU_GATHERING: i64 = 7;
/// The complex unit's `mResourceType`; set: the unit's `SetResourceType(b)`.
pub const EE_SCENARIO_CU_RESOURCE_TYPE: i64 = 8;
/// `GetStats()->mFlightTime`.
pub const EE_SCENARIO_CU_STATS_FLIGHT_TIME: i64 = 9;
/// The complex unit's `mFlightTime`; set: `SetFlightTime(b)`.
pub const EE_SCENARIO_CU_FLIGHT_TIME: i64 = 10;
/// The class's `mGarrisonCapacity`.
pub const EE_SCENARIO_CU_GARRISON_CAPACITY: i64 = 11;
/// `mGarrisonCount`; set: `AddGarrison(b, mGarrisonPopulation)` (b the change).
pub const EE_SCENARIO_CU_GARRISON: i64 = 12;
/// The class's `mAmmunition`.
pub const EE_SCENARIO_CU_CLASS_AMMUNITION: i64 = 13;
/// `mAmmunition` (unsigned); set: `mAmmunition = b`.
pub const EE_SCENARIO_CU_AMMUNITION: i64 = 14;
/// The class's `GetScale()` (bits).
pub const EE_SCENARIO_CU_CLASS_SCALE: i64 = 15;
/// The physical model's `GetScale()` (bits); set: `SetScale(b's float)`.
pub const EE_SCENARIO_CU_SCALE: i64 = 16;
/// `GetStats()->mReloadTime`.
pub const EE_SCENARIO_CU_STATS_RELOAD: i64 = 17;
/// `mReloadTimer`; set: `mReloadTimer = b`.
pub const EE_SCENARIO_CU_RELOAD_TIMER: i64 = 18;
/// `GetStats()->mPower`.
pub const EE_SCENARIO_CU_STATS_POWER: i64 = 19;
/// `mPower`; set: `mPower = b`.
pub const EE_SCENARIO_CU_POWER: i64 = 20;
/// The class's `mRottingTime`.
pub const EE_SCENARIO_CU_CLASS_ROTTING: i64 = 21;
/// `mRottingAwayTime`; set: `mRottingAwayTime = b`.
pub const EE_SCENARIO_CU_ROTTING: i64 = 22;
/// `mUnitVariables[0]` (and + 1, + 2 for the other two); set: the variable = b.
pub const EE_SCENARIO_CU_VARIABLE: i64 = 23;
/// Set only: `SetAnimation(mAnimation, mPhysicalModel.GetFrame(), b != 0)`.
pub const EE_SCENARIO_CU_PAUSED: i64 = 26;
/// Set only: `SetBuilt(b != 0, 1, 1)`.
pub const EE_SCENARIO_CU_BUILT: i64 = 27;
/// Set only: a full garrison's wait (a load goal over no units queued, `StartAction(kActionWaitForGarrisonCost)`,
/// its class's `ActionChanged`).
pub const EE_SCENARIO_CU_GARRISON_WAIT: i64 = 28;
/// Set only: `Kill(0, 0, 0)` (b 0) or, for a resource, `SetResourceAmount(0)`, `UnitTaken`, `KillUnit` (b 1).
pub const EE_SCENARIO_CU_KILL: i64 = 29;

const CATEGORY_RESOURCE: i64 = 1;
const MODE_INCREASE: c_long = 0;
const MODE_DECREASE: c_long = 1;

fn value<H: Host>(h: &mut H, field: i64) -> i64 {
    h.world(EE_SCENARIO_WORLD_CU_VALUE, [field, 0, 0, 0, 0, 0])
}

fn set<H: Host>(h: &mut H, field: i64, v: i64) {
    h.world(EE_SCENARIO_WORLD_CU_SET, [field, v, 0, 0, 0, 0]);
}

fn random<H: Host>(h: &mut H, low: c_long, high: c_long) -> c_long {
    h.world(EE_SCENARIO_WORLD_CU_RANDOM, [low as i64, high as i64, 0, 0, 0, 0]) as c_long
}

fn float(bits: i64) -> f32 {
    f32::from_bits(bits as u32)
}

/// The amount `EEChangeUnit` starts from: a draw in `[minimum, maximum]` when `maximum` is set, else `minimum`;
/// `random` draws again, from `[0, minimum]`, or (a percent) from a range that skips `minimum..=maximum`.
fn amount<H: Host>(h: &mut H, percent: bool, minimum: c_long, maximum: c_ulong, random_amount: bool) -> c_long {
    let mut a = if maximum != 0 { random(h, minimum, maximum as c_long) } else { minimum };
    if random_amount {
        if !percent {
            a = random(h, 0, minimum);
        } else {
            let rest = (if maximum > 100 { 0 } else { (100 as c_ulong).wrapping_sub(maximum) }) as c_long;
            a = random(h, 0, minimum.wrapping_add(rest));
            if a >= minimum {
                // `theAmount += 1 + (inMaximum - inMinimum)` in unsigned long
                let step = (1 as c_ulong).wrapping_add(maximum.wrapping_sub(minimum as c_ulong));
                a = (a as c_ulong).wrapping_add(step) as c_long;
            }
        }
    }
    a
}

/// `EEChangeUnit(attribute, percent, unit, minimum, maximum, random, mode)` on the host's context unit.
#[allow(clippy::too_many_arguments)]
pub fn change_unit<H: Host>(
    h: &mut H,
    attribute: c_ulong,
    percent: bool,
    minimum: c_long,
    maximum: c_ulong,
    random_amount: bool,
    mode: c_long,
) {
    let mut a = amount(h, percent, minimum, maximum, random_amount);
    if value(h, EE_SCENARIO_CU_EXISTS) == 0 {
        return; // the original read the stats of no unit for a resource; every other attribute tested the unit
    }
    let complex = value(h, EE_SCENARIO_CU_COMPLEX) != 0;
    // the reload timer, power and rotting time: (base, current) after the switch
    let mut counter: Option<(i64, i64)> = None;
    match attribute {
        0 => {
            let hp = value(h, EE_SCENARIO_CU_HIT_POINTS) as c_long;
            if hp > 0 {
                let max = value(h, EE_SCENARIO_CU_MAX_HIT_POINTS) as c_long;
                a = unit_amount(percent, mode, a, max as f32, hp, false);
                if a < 1 {
                    let resource = value(h, EE_SCENARIO_CU_CATEGORY) == CATEGORY_RESOURCE;
                    set(h, EE_SCENARIO_CU_KILL, resource as i64);
                } else {
                    set(h, EE_SCENARIO_CU_HIT_POINTS, a as i64);
                }
            }
        }
        1..=6 => {
            let carry = value(h, EE_SCENARIO_CU_CARRY) as c_long;
            let current = value(h, EE_SCENARIO_CU_RESOURCE_AMOUNT) as c_long;
            a = unit_amount(percent, mode, a, carry as f32, current, true);
            if attribute == 1 {
                // any resource: a random type unless the unit is gathering, which keeps the type it gathers
                set(h, EE_SCENARIO_CU_RESOURCE_AMOUNT, a as i64);
                if !complex || value(h, EE_SCENARIO_CU_GATHERING) == 0 {
                    let kind = random(h, 2, 6);
                    set(h, EE_SCENARIO_CU_RESOURCE_TYPE, kind as i64);
                }
                return;
            }
            let state: i64 = match attribute {
                2 => 4,
                3 => 5,
                4 => 3,
                5 => 2,
                _ => 6,
            };
            if complex && value(h, EE_SCENARIO_CU_GATHERING) != 0 && value(h, EE_SCENARIO_CU_RESOURCE_TYPE) != state {
                return;
            }
            set(h, EE_SCENARIO_CU_RESOURCE_AMOUNT, a as i64);
            set(h, EE_SCENARIO_CU_RESOURCE_TYPE, state);
        }
        7 if complex => {
            let time = if percent {
                let stats = value(h, EE_SCENARIO_CU_STATS_FLIGHT_TIME) as c_long;
                lrintf(stats as f32 * a as f32 * 0.01f32) as c_long
            } else {
                ftol(a as f32 / 0.001f32 + 0.5f32) as c_long
            };
            let current = value(h, EE_SCENARIO_CU_FLIGHT_TIME) as c_long;
            set(h, EE_SCENARIO_CU_FLIGHT_TIME, unit_amount(false, mode, time, 0.0, current, true) as i64);
        }
        9 if complex => {
            let capacity = value(h, EE_SCENARIO_CU_GARRISON_CAPACITY) as c_long;
            if capacity > 0 {
                let count = value(h, EE_SCENARIO_CU_GARRISON) as c_long;
                a = unit_amount(percent, mode, a, capacity as f32, count, true);
                let mut delta = a.wrapping_sub(count);
                if capacity < count.wrapping_add(delta) {
                    delta = capacity.wrapping_sub(count);
                }
                set(h, EE_SCENARIO_CU_GARRISON, delta as i64);
                if value(h, EE_SCENARIO_CU_GARRISON) as c_long == capacity {
                    set(h, EE_SCENARIO_CU_GARRISON_WAIT, 0);
                }
            }
        }
        10 if complex => {
            // the class's ammunition as `(float)(unsigned long)`, the unit's as `(long)`
            let class = value(h, EE_SCENARIO_CU_CLASS_AMMUNITION) as c_long as c_ulong;
            let current = value(h, EE_SCENARIO_CU_AMMUNITION) as c_ulong;
            a = unit_amount(percent, mode, a, class as f32, current as c_long, true);
            if current != a as c_ulong {
                set(h, EE_SCENARIO_CU_AMMUNITION, a as i64);
            }
        }
        11 if complex => set(h, EE_SCENARIO_CU_PAUSED, (a != 0) as i64),
        12 if complex => {
            counter = Some((EE_SCENARIO_CU_STATS_RELOAD, EE_SCENARIO_CU_RELOAD_TIMER));
        }
        13 if complex => {
            let mut scale =
                if percent { float(value(h, EE_SCENARIO_CU_CLASS_SCALE)) * a as f32 * 0.01f32 } else { a as f32 };
            if mode == MODE_INCREASE {
                scale += float(value(h, EE_SCENARIO_CU_SCALE));
            } else if mode == MODE_DECREASE {
                scale = float(value(h, EE_SCENARIO_CU_SCALE)) - scale;
            }
            if scale <= 0.0 {
                scale = 0.001;
            }
            set(h, EE_SCENARIO_CU_SCALE, scale.to_bits() as i64);
        }
        14 if complex => {
            counter = Some((EE_SCENARIO_CU_STATS_POWER, EE_SCENARIO_CU_POWER));
        }
        15 if complex => {
            counter = Some((EE_SCENARIO_CU_CLASS_ROTTING, EE_SCENARIO_CU_ROTTING));
        }
        16 if complex => set(h, EE_SCENARIO_CU_BUILT, (a != 0) as i64),
        17..=19 if complex => {
            let field = EE_SCENARIO_CU_VARIABLE + (attribute - 17) as i64;
            let current = value(h, field) as c_long;
            set(h, field, unit_amount(false, mode, a, 0.0, current, true) as i64);
        }
        _ => {}
    }
    if let Some((base, field)) = counter {
        let base = value(h, base) as c_long;
        let current = value(h, field) as c_long;
        a = unit_amount(percent, mode, a, base as f32, current, true);
        if current != a {
            set(h, field, a as i64);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;

    /// One unit's values, the draws the world hands out in order, and the log of the calls.
    #[derive(Default)]
    struct World {
        values: BTreeMap<i64, i64>,
        draws: Vec<i64>,
        log: Vec<String>,
    }

    impl Host for World {
        fn read4(&mut self, _op: u32, _a: i32, _b: i32, _c: i32, _d: i32) -> i64 {
            0
        }
        fn world(&mut self, op: u32, args: [i64; 6]) -> i64 {
            match op {
                EE_SCENARIO_WORLD_CU_RANDOM => {
                    self.log.push(format!("random {} {}", args[0], args[1]));
                    self.draws.remove(0)
                }
                EE_SCENARIO_WORLD_CU_VALUE => *self.values.get(&args[0]).unwrap_or(&0),
                EE_SCENARIO_WORLD_CU_SET => {
                    self.log.push(format!("set {} {}", args[0], args[1]));
                    if args[0] == EE_SCENARIO_CU_GARRISON {
                        *self.values.entry(args[0]).or_insert(0) += args[1];
                    } else {
                        self.values.insert(args[0], args[1]);
                    }
                    0
                }
                _ => 0,
            }
        }
    }

    fn unit(values: &[(i64, i64)]) -> World {
        let mut w = World::default();
        w.values.insert(EE_SCENARIO_CU_EXISTS, 1);
        w.values.insert(EE_SCENARIO_CU_COMPLEX, 1);
        for &(k, v) in values {
            w.values.insert(k, v);
        }
        w
    }

    #[test]
    fn the_draws_come_first_and_skip_the_percent_range() {
        let mut w = unit(&[(EE_SCENARIO_CU_HIT_POINTS, 50), (EE_SCENARIO_CU_MAX_HIT_POINTS, 100)]);
        w.draws = vec![30, 40];
        change_unit(&mut w, 0, true, 20, 60, true, 2);
        // [20, 60], then [0, 20 + 40]: 40 >= 20 skips past 60 (40 + 41), 81 % of 100
        assert_eq!(w.log, ["random 20 60", "random 0 60", "set 2 81"]);
        let mut w = unit(&[]);
        w.values.insert(EE_SCENARIO_CU_EXISTS, 0);
        w.draws = vec![3];
        change_unit(&mut w, 0, false, 5, 0, true, 0);
        assert_eq!(w.log, ["random 0 5"]);
    }

    #[test]
    fn hit_points_kill_below_one() {
        let mut w = unit(&[(EE_SCENARIO_CU_HIT_POINTS, 10), (EE_SCENARIO_CU_CATEGORY, CATEGORY_RESOURCE)]);
        change_unit(&mut w, 0, false, 20, 0, false, 1);
        assert_eq!(w.log, ["set 29 1"]);
        let mut w = unit(&[(EE_SCENARIO_CU_HIT_POINTS, 0)]);
        change_unit(&mut w, 0, false, 20, 0, false, 0);
        assert!(w.log.is_empty());
    }

    #[test]
    fn resources_keep_a_gathered_type() {
        let mut w = unit(&[(EE_SCENARIO_CU_RESOURCE_AMOUNT, 10)]);
        w.draws = vec![4];
        change_unit(&mut w, 1, false, 5, 0, false, 0);
        assert_eq!(w.log, ["set 6 15", "random 2 6", "set 8 4"]);
        let mut w = unit(&[(EE_SCENARIO_CU_GATHERING, 1), (EE_SCENARIO_CU_RESOURCE_TYPE, 3)]);
        change_unit(&mut w, 2, false, 5, 0, false, 2);
        assert!(w.log.is_empty());
        let mut w = unit(&[(EE_SCENARIO_CU_GATHERING, 1), (EE_SCENARIO_CU_RESOURCE_TYPE, 4)]);
        change_unit(&mut w, 2, false, 5, 0, false, 2);
        assert_eq!(w.log, ["set 6 5", "set 8 4"]);
    }

    #[test]
    fn flight_time_garrison_and_ammunition() {
        let mut w = unit(&[(EE_SCENARIO_CU_FLIGHT_TIME, 1000)]);
        change_unit(&mut w, 7, false, 3, 0, false, 0);
        assert_eq!(w.log, ["set 10 4000"]);
        let mut w = unit(&[(EE_SCENARIO_CU_GARRISON_CAPACITY, 5), (EE_SCENARIO_CU_GARRISON, 2)]);
        change_unit(&mut w, 9, false, 10, 0, false, 0);
        assert_eq!(w.log, ["set 12 3", "set 28 0"]);
        let mut w = unit(&[(EE_SCENARIO_CU_AMMUNITION, 4)]);
        change_unit(&mut w, 10, false, 4, 0, false, 2);
        assert!(w.log.is_empty());
    }

    #[test]
    fn scale_counters_and_variables() {
        let mut w = unit(&[(EE_SCENARIO_CU_SCALE, 1.0f32.to_bits() as i64)]);
        change_unit(&mut w, 13, false, 3, 0, false, 1);
        assert_eq!(w.log, [format!("set 16 {}", 0.001f32.to_bits())]);
        let mut w = unit(&[(EE_SCENARIO_CU_STATS_POWER, 200), (EE_SCENARIO_CU_POWER, 50)]);
        change_unit(&mut w, 14, true, 10, 0, false, 0);
        assert_eq!(w.log, ["set 20 70"]);
        let mut w = unit(&[(EE_SCENARIO_CU_VARIABLE + 2, 7)]);
        change_unit(&mut w, 19, false, 9, 0, false, 1);
        assert_eq!(w.log, ["set 25 0"]);
        let mut w = unit(&[]);
        w.values.insert(EE_SCENARIO_CU_COMPLEX, 0);
        change_unit(&mut w, 16, false, 1, 0, false, 2);
        assert!(w.log.is_empty());
    }
}
