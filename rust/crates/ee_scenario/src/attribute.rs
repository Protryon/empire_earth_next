//! `EESMObject::GetUnitAttribute`: a unit's value for an object's attribute filter (and a message's `/#OA` tag):
//! the current value, the bonus of its upgrade over the class, or the total, optionally as a percentage of a
//! capacity. The 474-line switch is a table here: each attribute names the unit's value, how it is computed and
//! which capacity its percentage takes; the unit's members come through one read
//! (`EE_SCENARIO_READ_UNIT_FIELD`, a field number per member: `long`s and `bool`s as integers, `float`s as bits).
//! The arithmetic is the original's in `f32` (`theResult * 100.0f / theCapacity`, the C cast's truncation `ftol`).

use crate::aivar::lrintf;
use crate::ftol;
use crate::host::Host;

/// The context unit's member `b` (`EE_SCENARIO_UNIT_*`): an integer, or a `float`'s bits for the float fields.
pub const EE_SCENARIO_READ_UNIT_FIELD: u32 = 69;

// the unit (`inUnit`), its complex unit (`GetComplexUnit()`), its upgrade (`GetStats()`)
pub const EE_SCENARIO_UNIT_HAS_COMPLEX: i32 = 0;
pub const EE_SCENARIO_UNIT_HAS_STATS: i32 = 1;
pub const EE_SCENARIO_UNIT_HAS_CLASS: i32 = 2;
pub const EE_SCENARIO_UNIT_HIT_POINTS: i32 = 3;
pub const EE_SCENARIO_UNIT_RESOURCE_TYPE: i32 = 4;
pub const EE_SCENARIO_UNIT_RESOURCE_AMOUNT: i32 = 5;
pub const EE_SCENARIO_UNIT_FLIGHT_TIME: i32 = 6;
pub const EE_SCENARIO_UNIT_CARGO: i32 = 7;
pub const EE_SCENARIO_UNIT_GARRISON: i32 = 8;
pub const EE_SCENARIO_UNIT_AMMUNITION: i32 = 9;
pub const EE_SCENARIO_UNIT_PAUSED: i32 = 10;
pub const EE_SCENARIO_UNIT_RELOAD_TIMER: i32 = 11;
/// float
pub const EE_SCENARIO_UNIT_SCALE: i32 = 12;
pub const EE_SCENARIO_UNIT_POWER: i32 = 13;
pub const EE_SCENARIO_UNIT_ROTTING: i32 = 14;
pub const EE_SCENARIO_UNIT_BUILT: i32 = 15;
/// `mUnitVariables[b - this]`, 3 of them.
pub const EE_SCENARIO_UNIT_VARIABLE: i32 = 16;
// the upgrade's (`EEUnitClassUpgrade`)
pub const EE_SCENARIO_STATS_MAX_HIT_POINTS: i32 = 20;
pub const EE_SCENARIO_STATS_CARRY: i32 = 21;
pub const EE_SCENARIO_STATS_FLIGHT_TIME: i32 = 22;
pub const EE_SCENARIO_STATS_MAX_CARRY: i32 = 23;
pub const EE_SCENARIO_STATS_RELOAD: i32 = 24;
/// float
pub const EE_SCENARIO_STATS_SCALE: i32 = 25;
pub const EE_SCENARIO_STATS_POWER: i32 = 26;
/// float `mArmor[b - this]`, 6 of them.
pub const EE_SCENARIO_STATS_ARMOR: i32 = 27;
/// float
pub const EE_SCENARIO_STATS_ATTACK: i32 = 33;
pub const EE_SCENARIO_STATS_WORK_RATE: i32 = 34;
/// float
pub const EE_SCENARIO_STATS_LEADERSHIP: i32 = 35;
pub const EE_SCENARIO_STATS_REPAIR: i32 = 36;
pub const EE_SCENARIO_STATS_HEALING: i32 = 37;
/// float
pub const EE_SCENARIO_STATS_RANGE: i32 = 38;
/// float
pub const EE_SCENARIO_STATS_LINE_OF_SIGHT: i32 = 39;
/// float
pub const EE_SCENARIO_STATS_VELOCITY: i32 = 40;
pub const EE_SCENARIO_STATS_INITIALIZED: i32 = 41;
/// float
pub const EE_SCENARIO_STATS_CAMERA_FOCUS: i32 = 42;
// a class's (`EEUnitClass`): the unit's (`GetUnitClass()`), or the upgrade's (`mUnitClass`) at + EE_SCENARIO_OF_STATS
pub const EE_SCENARIO_CLASS_GARRISON: i32 = 50;
pub const EE_SCENARIO_CLASS_AMMUNITION: i32 = 51;
pub const EE_SCENARIO_CLASS_ROTTING: i32 = 52;
pub const EE_SCENARIO_CLASS_POWER: i32 = 53;
/// `mArmor[b - this]` (`long`), 6 of them.
pub const EE_SCENARIO_CLASS_ARMOR: i32 = 54;
pub const EE_SCENARIO_CLASS_RELOAD: i32 = 60;
pub const EE_SCENARIO_CLASS_ATTACK: i32 = 61;
pub const EE_SCENARIO_CLASS_WORK_RATE: i32 = 62;
pub const EE_SCENARIO_CLASS_FLIGHT_TIME: i32 = 63;
pub const EE_SCENARIO_CLASS_LEADERSHIP: i32 = 64;
pub const EE_SCENARIO_CLASS_MAX_CARRY: i32 = 65;
pub const EE_SCENARIO_CLASS_REPAIR: i32 = 66;
pub const EE_SCENARIO_CLASS_HIT_POINTS: i32 = 67;
pub const EE_SCENARIO_CLASS_CARRY: i32 = 68;
pub const EE_SCENARIO_CLASS_HEALING: i32 = 69;
/// float
pub const EE_SCENARIO_CLASS_RANGE: i32 = 70;
/// float
pub const EE_SCENARIO_CLASS_LINE_OF_SIGHT: i32 = 71;
/// float
pub const EE_SCENARIO_CLASS_VELOCITY: i32 = 72;
pub const EE_SCENARIO_CLASS_TRANSPORT: i32 = 73;
pub const EE_SCENARIO_CLASS_WEIGHT: i32 = 74;
pub const EE_SCENARIO_CLASS_TILE_SIZE: i32 = 75;
/// float (`GetCruiseAltitude()`)
pub const EE_SCENARIO_CLASS_CRUISE: i32 = 76;
/// float
pub const EE_SCENARIO_CLASS_MIN_RANGE: i32 = 77;
pub const EE_SCENARIO_CLASS_AIR: i32 = 78;
pub const EE_SCENARIO_CLASS_LAND: i32 = 79;
pub const EE_SCENARIO_CLASS_SEA: i32 = 80;
pub const EE_SCENARIO_CLASS_FORTRESS: i32 = 81;
/// Added to a class field: the upgrade's class (`GetStats()->mUnitClass`) instead of the unit's.
pub const EE_SCENARIO_OF_STATS: i32 = 100;

/// `kResourceFood` .. `kResourceIron` for the attributes Stone, Gold, Wood, Food, Iron (2..6).
const RESOURCES: [i64; 5] = [4, 5, 3, 2, 6];

/// The unit's reads.
struct UnitFields<'a> {
    h: &'a mut dyn Host,
}

impl UnitFields<'_> {
    fn int(&mut self, field: i32) -> i64 {
        self.h.read(EE_SCENARIO_READ_UNIT_FIELD, field, 0)
    }
    /// A `long` (32 bits in the original).
    fn long(&mut self, field: i32) -> i32 {
        self.int(field) as i32
    }
    fn float(&mut self, field: i32) -> f32 {
        f32::from_bits(self.int(field) as u32)
    }
    fn has(&mut self, field: i32) -> bool {
        self.int(field) != 0
    }
}

/// `(long)(theResult * 100.0f / theCapacity)`.
fn percent_of(result: i32, capacity: f32) -> i32 {
    ftol(result as f32 * 100.0f32 / capacity)
}

/// The current values' percentage: of a capacity `cap` (as a float), 100 at or without it, the value itself at 0.
fn current_percent(result: i32, cap_float: f32, cap: i32) -> i32 {
    if cap_float > 0.0 {
        if result == 0 {
            return 0;
        }
        if result != cap {
            return percent_of(result, cap_float);
        }
    }
    100
}

/// The totals' percentage over the unit's class value `of` (> its threshold), 100 otherwise.
fn total_percent(result: i32, above: bool, of: f32) -> i32 {
    if above {
        percent_of(result, of)
    } else {
        100
    }
}

/// `EESMObject::GetUnitAttribute(attribute, percent, the host's unit, 0, 0)` for a unit that is not NULL.
pub fn unit_attribute(host: &mut dyn Host, attribute: u32, percent: bool) -> i32 {
    let mut u = UnitFields { h: host };
    let complex = u.has(EE_SCENARIO_UNIT_HAS_COMPLEX);
    let stats = u.has(EE_SCENARIO_UNIT_HAS_STATS);
    let class = u.has(EE_SCENARIO_UNIT_HAS_CLASS);
    // a current value with its capacity's percentage (`ok`: the capacity's owner is there)
    let current = |u: &mut UnitFields, result: i32, ok: bool, cap: i32| -> i32 {
        if percent && ok {
            let value = u.long(cap);
            current_percent(result, value as f32, value)
        } else {
            result
        }
    };
    // a total or bonus over an integer field of the unit's class
    let over_int = |u: &mut UnitFields, result: i32, field: i32| -> i32 {
        if percent {
            let of = u.long(field);
            total_percent(result, of > 0, of as f32)
        } else {
            result
        }
    };
    let over_float = |u: &mut UnitFields, result: i32, field: i32| -> i32 {
        if percent {
            let of = u.float(field);
            total_percent(result, of > 0.0, of)
        } else {
            result
        }
    };
    const B: i32 = EE_SCENARIO_OF_STATS;
    match attribute {
        0 => {
            let r = u.long(EE_SCENARIO_UNIT_HIT_POINTS);
            current(&mut u, r, stats, EE_SCENARIO_STATS_MAX_HIT_POINTS)
        }
        1..=6 => {
            if attribute >= 2
                && u.int(EE_SCENARIO_UNIT_RESOURCE_TYPE) as u32 as i64 != RESOURCES[attribute as usize - 2]
            {
                return 0;
            }
            let r = u.long(EE_SCENARIO_UNIT_RESOURCE_AMOUNT);
            current(&mut u, r, stats, EE_SCENARIO_STATS_CARRY)
        }
        7..=10 | 12 | 14 | 15 if !complex => 0,
        7 => {
            let r = ftol(u.long(EE_SCENARIO_UNIT_FLIGHT_TIME) as f32 / 1000.0);
            current(&mut u, r, stats, EE_SCENARIO_STATS_FLIGHT_TIME)
        }
        8 => {
            let r = u.long(EE_SCENARIO_UNIT_CARGO);
            current(&mut u, r, stats, EE_SCENARIO_STATS_MAX_CARRY)
        }
        9 => {
            let r = u.long(EE_SCENARIO_UNIT_GARRISON);
            current(&mut u, r, class, EE_SCENARIO_CLASS_GARRISON)
        }
        10 => {
            let r = u.long(EE_SCENARIO_UNIT_AMMUNITION);
            if percent && class {
                // the capacity zero-extended before the conversion; the comparison as longs
                let cap = u.long(EE_SCENARIO_CLASS_AMMUNITION);
                current_percent(r, cap as u32 as f32, cap)
            } else {
                r
            }
        }
        11 | 16 => {
            if complex {
                u.long(if attribute == 11 { EE_SCENARIO_UNIT_PAUSED } else { EE_SCENARIO_UNIT_BUILT })
            } else {
                0
            }
        }
        12 => {
            let r = u.long(EE_SCENARIO_UNIT_RELOAD_TIMER);
            current(&mut u, r, stats, EE_SCENARIO_STATS_RELOAD)
        }
        13 => {
            if complex {
                let r = ftol(u.float(EE_SCENARIO_UNIT_SCALE));
                if percent && stats {
                    let scale = u.float(EE_SCENARIO_STATS_SCALE);
                    if scale > 0.0 {
                        if r == 0 {
                            return 0;
                        }
                        if r != ftol(scale) {
                            return percent_of(r, scale);
                        }
                    }
                    return 100;
                }
                r
            } else if stats {
                lrintf(u.float(EE_SCENARIO_STATS_SCALE))
            } else {
                1
            }
        }
        14 => {
            let r = u.long(EE_SCENARIO_UNIT_POWER);
            current(&mut u, r, stats, EE_SCENARIO_STATS_POWER)
        }
        15 => {
            let r = u.long(EE_SCENARIO_UNIT_ROTTING);
            current(&mut u, r, class, EE_SCENARIO_CLASS_ROTTING)
        }
        17..=19 => {
            if complex {
                u.long(EE_SCENARIO_UNIT_VARIABLE + attribute as i32 - 17)
            } else {
                0
            }
        }
        // the armors' bonus (a float less a long, cut) and total (the float cut)
        20..=25 | 42..=47 => {
            let i = if attribute >= 42 { attribute - 42 } else { attribute - 20 } as i32;
            let mut v = u.float(EE_SCENARIO_STATS_ARMOR + i);
            if attribute < 42 {
                v -= u.long(EE_SCENARIO_CLASS_ARMOR + i + B) as f32;
            }
            over_int(&mut u, ftol(v), EE_SCENARIO_CLASS_ARMOR + i)
        }
        26 | 48 => {
            let mut r = u.long(EE_SCENARIO_STATS_RELOAD);
            if attribute == 26 {
                r = r.wrapping_sub(u.long(EE_SCENARIO_CLASS_RELOAD + B));
            }
            over_int(&mut u, r, EE_SCENARIO_CLASS_RELOAD)
        }
        27 | 49 => {
            let mut v = u.float(EE_SCENARIO_STATS_ATTACK);
            if attribute == 27 {
                v -= u.long(EE_SCENARIO_CLASS_ATTACK + B) as f32;
            }
            over_int(&mut u, ftol(v), EE_SCENARIO_CLASS_ATTACK)
        }
        28 | 50 => {
            let mut r = u.long(EE_SCENARIO_STATS_WORK_RATE);
            if attribute == 28 {
                r = r.wrapping_sub(u.long(EE_SCENARIO_CLASS_WORK_RATE + B));
            }
            over_int(&mut u, r, EE_SCENARIO_CLASS_WORK_RATE)
        }
        29 | 51 => {
            let mut r = u.long(EE_SCENARIO_STATS_FLIGHT_TIME);
            if attribute == 29 {
                r = r.wrapping_sub(u.long(EE_SCENARIO_CLASS_FLIGHT_TIME + B));
            }
            let r = ftol(r as f32 / 1000.0);
            if percent {
                let of = u.long(EE_SCENARIO_CLASS_FLIGHT_TIME);
                total_percent(r, of > 0, of as f32 / 1000.0)
            } else {
                r
            }
        }
        30 | 52 => {
            let mut v = u.float(EE_SCENARIO_STATS_LEADERSHIP);
            if attribute == 30 {
                v -= u.long(EE_SCENARIO_CLASS_LEADERSHIP + B) as f32;
            }
            over_int(&mut u, ftol(v), EE_SCENARIO_CLASS_LEADERSHIP)
        }
        31 | 53 => {
            let mut r = u.long(EE_SCENARIO_STATS_MAX_CARRY);
            if attribute == 31 {
                r = r.wrapping_sub(u.long(EE_SCENARIO_CLASS_MAX_CARRY + B));
            } else if r == 0 {
                let r = u.long(EE_SCENARIO_CLASS_GARRISON);
                return if percent { 100 } else { r };
            }
            over_int(&mut u, r, EE_SCENARIO_CLASS_MAX_CARRY)
        }
        32 | 54 | 33 | 55 | 34 | 56 | 35 | 57 => {
            let (stat, of) = match attribute {
                32 | 54 => (EE_SCENARIO_STATS_REPAIR, EE_SCENARIO_CLASS_REPAIR),
                33 | 55 => (EE_SCENARIO_STATS_MAX_HIT_POINTS, EE_SCENARIO_CLASS_HIT_POINTS),
                34 | 56 => (EE_SCENARIO_STATS_CARRY, EE_SCENARIO_CLASS_CARRY),
                _ => (EE_SCENARIO_STATS_HEALING, EE_SCENARIO_CLASS_HEALING),
            };
            let mut r = u.long(stat);
            if attribute < 42 {
                r = r.wrapping_sub(u.long(of + B));
            }
            over_int(&mut u, r, of)
        }
        36 | 58 | 37 | 59 => {
            let (stat, of) = if attribute == 36 || attribute == 58 {
                (EE_SCENARIO_STATS_RANGE, EE_SCENARIO_CLASS_RANGE)
            } else {
                (EE_SCENARIO_STATS_LINE_OF_SIGHT, EE_SCENARIO_CLASS_LINE_OF_SIGHT)
            };
            let mut v = u.float(stat);
            if attribute < 42 {
                v -= u.float(of + B);
            }
            over_float(&mut u, ftol(v), of)
        }
        38 | 60 => {
            let mut v = u.float(EE_SCENARIO_STATS_VELOCITY);
            if attribute == 38 {
                v -= u.float(EE_SCENARIO_CLASS_VELOCITY + B);
            }
            let r = ftol(v * 20.0);
            if percent {
                let of = u.float(EE_SCENARIO_CLASS_VELOCITY);
                total_percent(r, of > 0.0001, of * 20.0)
            } else {
                r
            }
        }
        39 | 61 => {
            let mut r = u.long(EE_SCENARIO_STATS_POWER);
            if attribute == 39 {
                r = r.wrapping_sub(u.long(EE_SCENARIO_CLASS_POWER + B));
            }
            if percent {
                let of = u.long(EE_SCENARIO_CLASS_POWER);
                // Power+ compares the long, Total Power its float
                total_percent(r, if attribute == 39 { of > 0 } else { of as f32 > 0.0 }, of as f32)
            } else {
                r
            }
        }
        40 => u.long(EE_SCENARIO_STATS_INITIALIZED),
        41 => ftol(u.float(EE_SCENARIO_STATS_CAMERA_FOCUS) * 100.0),
        64..=72 => {
            let r = match attribute {
                64 => u.long(EE_SCENARIO_CLASS_TRANSPORT),
                65 => u.long(EE_SCENARIO_CLASS_WEIGHT),
                66 => u.long(EE_SCENARIO_CLASS_TILE_SIZE),
                67 => ftol(u.float(EE_SCENARIO_CLASS_CRUISE)),
                68 => ftol(u.float(EE_SCENARIO_CLASS_MIN_RANGE)),
                _ => u.has(EE_SCENARIO_CLASS_AIR + attribute as i32 - 69) as i32,
            };
            if percent {
                100
            } else {
                r
            }
        }
        73 | 74 if percent => 100,
        _ => 0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;

    /// A unit as its fields (missing ones read 0).
    struct Mock(BTreeMap<i32, i64>);

    impl Host for Mock {
        fn read4(&mut self, op: u32, a: i32, _b: i32, _c: i32, _d: i32) -> i64 {
            assert_eq!(op, EE_SCENARIO_READ_UNIT_FIELD);
            *self.0.get(&a).unwrap_or(&0)
        }
    }

    fn f(v: f32) -> i64 {
        v.to_bits() as i64
    }

    fn unit(extra: &[(i32, i64)]) -> Mock {
        let mut m = BTreeMap::new();
        for &(k, v) in
            [(EE_SCENARIO_UNIT_HAS_COMPLEX, 1), (EE_SCENARIO_UNIT_HAS_STATS, 1), (EE_SCENARIO_UNIT_HAS_CLASS, 1)]
                .iter()
                .chain(extra)
        {
            m.insert(k, v);
        }
        Mock(m)
    }

    #[test]
    fn hit_points_and_their_percentage() {
        let mut m = unit(&[(EE_SCENARIO_UNIT_HIT_POINTS, 30), (EE_SCENARIO_STATS_MAX_HIT_POINTS, 90)]);
        assert_eq!(unit_attribute(&mut m, 0, false), 30);
        assert_eq!(unit_attribute(&mut m, 0, true), 33);
        m.0.insert(EE_SCENARIO_UNIT_HIT_POINTS, 90);
        assert_eq!(unit_attribute(&mut m, 0, true), 100);
        m.0.insert(EE_SCENARIO_UNIT_HIT_POINTS, 0);
        assert_eq!(unit_attribute(&mut m, 0, true), 0);
        // no capacity: 100 even at 0
        m.0.insert(EE_SCENARIO_STATS_MAX_HIT_POINTS, 0);
        assert_eq!(unit_attribute(&mut m, 0, true), 100);
    }

    #[test]
    fn resources_by_type() {
        let mut m = unit(&[(EE_SCENARIO_UNIT_RESOURCE_TYPE, 5), (EE_SCENARIO_UNIT_RESOURCE_AMOUNT, 40)]);
        assert_eq!(unit_attribute(&mut m, 1, false), 40);
        assert_eq!(unit_attribute(&mut m, 3, false), 40); // gold
        assert_eq!(unit_attribute(&mut m, 2, false), 0); // stone
    }

    #[test]
    fn complex_values_need_a_complex_unit() {
        let mut m = unit(&[(EE_SCENARIO_UNIT_FLIGHT_TIME, 2500), (EE_SCENARIO_UNIT_BUILT, 1)]);
        assert_eq!(unit_attribute(&mut m, 7, false), 2);
        assert_eq!(unit_attribute(&mut m, 16, false), 1);
        m.0.insert(EE_SCENARIO_UNIT_HAS_COMPLEX, 0);
        assert_eq!(unit_attribute(&mut m, 7, false), 0);
        assert_eq!(unit_attribute(&mut m, 16, false), 0);
        // the scale without a complex unit: the upgrade's, rounded; 1 without an upgrade
        m.0.insert(EE_SCENARIO_STATS_SCALE, f(2.5));
        assert_eq!(unit_attribute(&mut m, 13, false), 2);
        m.0.insert(EE_SCENARIO_UNIT_HAS_STATS, 0);
        assert_eq!(unit_attribute(&mut m, 13, false), 1);
    }

    #[test]
    fn armor_bonus_and_total() {
        let mut m = unit(&[
            (EE_SCENARIO_STATS_ARMOR + 1, f(7.9)),
            (EE_SCENARIO_CLASS_ARMOR + 1 + EE_SCENARIO_OF_STATS, 3),
            (EE_SCENARIO_CLASS_ARMOR + 1, 4),
        ]);
        assert_eq!(unit_attribute(&mut m, 21, false), 4); // 7.9 - 3 cut
        assert_eq!(unit_attribute(&mut m, 43, false), 7);
        assert_eq!(unit_attribute(&mut m, 43, true), 175); // 7 * 100 / 4
        m.0.insert(EE_SCENARIO_CLASS_ARMOR + 1, 0);
        assert_eq!(unit_attribute(&mut m, 43, true), 100);
    }

    #[test]
    fn velocity_flight_and_carry() {
        let mut m = unit(&[
            (EE_SCENARIO_STATS_VELOCITY, f(1.5)),
            (EE_SCENARIO_CLASS_VELOCITY, f(1.0)),
            (EE_SCENARIO_STATS_FLIGHT_TIME, 30000),
            (EE_SCENARIO_CLASS_FLIGHT_TIME, 20000),
            (EE_SCENARIO_CLASS_GARRISON, 12),
        ]);
        assert_eq!(unit_attribute(&mut m, 60, false), 30);
        assert_eq!(unit_attribute(&mut m, 60, true), 150);
        assert_eq!(unit_attribute(&mut m, 51, true), 150);
        // no objects carried: the class's garrison, or 100
        assert_eq!(unit_attribute(&mut m, 53, false), 12);
        assert_eq!(unit_attribute(&mut m, 53, true), 100);
    }

    #[test]
    fn class_values_and_placeholders() {
        let mut m = unit(&[
            (EE_SCENARIO_CLASS_TILE_SIZE, 3),
            (EE_SCENARIO_CLASS_SEA, 1),
            (EE_SCENARIO_CLASS_MIN_RANGE, f(2.75)),
        ]);
        assert_eq!(unit_attribute(&mut m, 66, false), 3);
        assert_eq!(unit_attribute(&mut m, 66, true), 100);
        assert_eq!(unit_attribute(&mut m, 71, false), 1);
        assert_eq!(unit_attribute(&mut m, 68, false), 2);
        assert_eq!(unit_attribute(&mut m, 62, true), 0);
        assert_eq!(unit_attribute(&mut m, 73, true), 100);
        assert_eq!(unit_attribute(&mut m, 74, false), 0);
        assert_eq!(unit_attribute(&mut m, 200, true), 0);
    }

    #[test]
    fn percentages_cut_toward_zero_with_the_c_cast() {
        assert_eq!(percent_of(-1, 3.0), -33);
        assert_eq!(percent_of(i32::MAX, 0.5), i32::MIN);
    }
}
