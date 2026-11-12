//! The in-game HUD's status box (`User Interface Thread/EEUIFMGStatusBox.cpp`): the attribute texts the box shows
//! for the selected unit ("base+bonus": `GetAttributeText`), and the armor attributes' values (`UpdateArmorTexts`:
//! each armor kind's base from the unit's class, its bonus from the unit's stats over their own class, nudged away
//! from a rounding tie by 0.0001).
//!
//! The C++ keeps the unit, its class and stats (read under the world's read lock, as the UI thread updates the
//! forms), the icons and the labels; it hands the numbers over and formats and shows the result.

use core::ffi::{c_long, c_ulong};

use crate::settings::float_to_long;

/// `GetAttributeText`: the base and the bonus shown as "base+bonus", each rounded half up (`(long)(x + 0.5f)`, the
/// Windows build's conversion); a negative bonus is taken off the base and shows as 0.
pub fn attribute_text(base: f32, bonus: f32) -> (c_long, c_long) {
    let shown_bonus = if bonus >= 0.0 { float_to_long(bonus + 0.5) } else { 0 };
    let shown_base = if bonus < 0.0 { float_to_long(base + bonus + 0.5) } else { float_to_long(base + 0.5) };
    (shown_base, shown_bonus)
}

/// The armor kind each of the six armor attributes shows (`mAttributes[4..9]`): shock, arrow, pierce, gun, laser,
/// missile as the original's blocks read `mArmor`.
pub const ARMOR_KINDS: [u32; 6] = [0, 2, 1, 3, 5, 4];

/// One armor attribute's `GetAttributeText` arguments: the unit class's base armor (a `long`, through a `double`
/// as the original) and the stats' bonus over their own class's base (`stat - class + ±0.0001`, in `float`, the
/// nudge negative when the stat is below the unit class's base).
pub fn armor_attribute(stat: f32, class_base: c_long, stats_class_base: c_long) -> (f32, f32) {
    let base = class_base as f64;
    let nudge = if (stat as f64) < base { -0.0001f32 } else { 0.0001f32 };
    (base as f32, stat - stats_class_base as f32 + nudge)
}

/// What the status box's `Fill` and `Refresh` read of the selected unit: its class (`theClass`), its stats
/// (`theStats`), the stats' own class (`theStats->mUnitClass`: an upgraded unit's stats keep their class), the unit
/// and its complex unit. The C++ reads them under the world's read lock and hands them over.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensHudUnit {
    pub category: c_ulong,
    pub unit_type: c_ulong,
    /// Hit points: the class's, the unit's, the stats' maximum, the stats' class's.
    pub class_hit_points: c_long,
    pub hit_points: c_long,
    pub max_hit_points: c_long,
    pub stats_class_hit_points: c_long,
    /// Attack: the class's (a `long`), the stats' (a `float`), the stats' class's.
    pub class_attack: c_long,
    pub stats_attack: f32,
    pub stats_class_attack: c_long,
    pub class_range: f32,
    pub stats_range: f32,
    pub stats_class_range: f32,
    pub class_speed: f32,
    pub stats_speed: f32,
    pub stats_class_speed: f32,
    pub class_area_effect: f32,
    pub stats_area_effect: f32,
    pub stats_class_area_effect: f32,
    pub class_repair: c_long,
    pub stats_repair: c_long,
    pub stats_class_repair: c_long,
    /// The class's upgrade flags: range, area effect, repair, then the armor kinds in `mAttributes[4..9]`' order
    /// (shock, pierce, arrow, gun, missile, laser).
    pub upgrades_range: bool,
    pub upgrades_area_effect: bool,
    pub upgrades_repair: bool,
    pub upgrades_armor: [bool; 6],
    pub class_transport: c_long,
    pub class_flight_time: c_long,
    pub class_garrison: c_long,
    /// The unit's owner is the local player or not at war with it (read only when the class carries or garrisons).
    pub friendly: bool,
    /// The unit is a complex unit: the rest is its (zero otherwise).
    pub complex: bool,
    pub complex_class_transport: c_long,
    pub complex_class_garrison: c_long,
    pub cargo_room: c_long,
    pub flight_time: c_long,
    pub stats_flight_time: c_long,
    pub stats_class_flight_time: c_long,
    pub power: c_long,
    pub stats_power: c_long,
    pub stats_class_power: c_long,
    pub garrison_count: c_long,
    /// The box has its third and fourth bars (controls 0x33 and 0x32).
    pub two_bars: bool,
    pub resource_type: c_ulong,
    pub resource_amount: c_long,
    pub carry_amount: c_long,
    /// `gUsingCarbon`: the wood icon's variant.
    pub using_carbon: bool,
}

/// One attribute icon `Fill` shows: `mAttributes[slot] = icon` (-1: not shown, nothing else done), the icon's
/// bitmaps `bitmap` and `bitmap + 1` (`sIconBitmaps`) stretched over the icon rectangle of `rect_icon` (the armor
/// icons all take the first armor icon's, as the original), and its improvement.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensHudIcon {
    pub slot: u32,
    pub icon: c_long,
    pub rect_icon: c_long,
    pub bitmap: u32,
    pub improvement: c_long,
}

/// A group of attribute icons in the order the C++ shows them; `next` the next free icon after them.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensHudIcons {
    pub count: u32,
    pub next: c_long,
    pub icons: [EeScreensHudIcon; 6],
}

// The improvements (`ee/EEImprovementIDs.h`).
const AREA_EFFECT: c_long = 0;
const ATTACK: c_long = 1;
const ARROW_ARMOR: c_long = 2;
const GUN_ARMOR: c_long = 3;
const LASER_ARMOR: c_long = 4;
const MISSILE_ARMOR: c_long = 5;
const PIERCE_ARMOR: c_long = 6;
const SHOCK_ARMOR: c_long = 7;
const RANGE: c_long = 11;
const REPAIR: c_long = 12;
const SPEED: c_long = 14;
const TRANSPORT: c_long = 15;

// The unit categories and types the box tests (`ee/EEUnitClassIDs.h`).
const CATEGORY_RESOURCE: c_ulong = 1;
const CATEGORY_LAND_CITIZEN: c_ulong = 6;
const CATEGORY_NAVAL_CITIZEN: c_ulong = 0x16;
const CATEGORY_AIR_ANIMAL: c_ulong = 0x1c;
/// The flaming archers and the royal cuirassier (their power on the fourth bar).
const FLAMING_TYPES: [c_ulong; 4] = [0x2d6, 0x2d9, 0x2dd, 0xec];

fn group(start: c_long, entries: &[(u32, bool, u32, c_long)], armor: bool) -> EeScreensHudIcons {
    let mut out = EeScreensHudIcons::default();
    let mut next = start;
    for (k, &(slot, shown, bitmap, improvement)) in entries.iter().enumerate() {
        let icon = if shown { next } else { -1 };
        let rect_icon = if armor { start } else { icon };
        out.icons[k] = EeScreensHudIcon { slot, icon, rect_icon, bitmap, improvement };
        if shown {
            next += 1;
        }
    }
    out.count = entries.len() as u32;
    out.next = next;
    out
}

/// `Fill`'s first attribute icons, from icon 0: attack (a class attack, not a naval citizen), range, speed (a
/// moving class), area effect, repair.
pub fn fill_icons(unit: &EeScreensHudUnit) -> EeScreensHudIcons {
    group(
        0,
        &[
            (0, unit.class_attack > 0 && unit.category != CATEGORY_NAVAL_CITIZEN, 0, ATTACK),
            (1, unit.upgrades_range, 2, RANGE),
            (2, unit.class_speed > 0.0, 4, SPEED),
            (3, unit.upgrades_area_effect, 6, AREA_EFFECT),
            (10, unit.upgrades_repair, 20, REPAIR),
        ],
        false,
    )
}

/// `FillArmorAttributes`' icons from icon `start`: the armor kinds the class upgrades.
pub fn armor_icons(start: c_long, upgrades: [bool; 6]) -> EeScreensHudIcons {
    let kinds = [
        (8, SHOCK_ARMOR),
        (10, PIERCE_ARMOR),
        (12, ARROW_ARMOR),
        (14, GUN_ARMOR),
        (16, MISSILE_ARMOR),
        (18, LASER_ARMOR),
    ];
    let mut entries = [(0u32, false, 0u32, 0 as c_long); 6];
    for (k, &(bitmap, improvement)) in kinds.iter().enumerate() {
        entries[k] = (4 + k as u32, upgrades[k], bitmap, improvement);
    }
    group(start, &entries, true)
}

/// `Fill`'s transport icon at icon `start`: a carrying class whose owner is friendly.
pub fn transport_icon(start: c_long, unit: &EeScreensHudUnit) -> EeScreensHudIcons {
    group(start, &[(12, unit.class_transport > 0 && unit.friendly, 46, TRANSPORT)], false)
}

/// The bars `Fill` sets up: the hit points bar, the first complex bar (flight time on the second bar, or power on
/// the bar [`power_bar`] picks) and the garrison bar (the second bar), each with its range and current value.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensHudFillBars {
    pub hit_points: bool,
    /// 0 none, 1 flight time, 2 power.
    pub kind: u32,
    /// 0 the second bar (control 0x1f), 1 the third (0x33), 2 the fourth (0x32).
    pub bar: u32,
    pub max: c_long,
    pub current: c_long,
    pub garrison: bool,
    pub garrison_max: c_long,
    pub garrison_current: c_long,
}

/// The power bar: with the box's two extra bars, the flaming archers' and the royal cuirassier's on the fourth, a
/// garrisoning unit's on the third (`Fill` also asks for a friendly owner there), else the second.
pub fn power_bar(unit: &EeScreensHudUnit, garrisons: bool) -> u32 {
    if !unit.two_bars {
        0
    } else if FLAMING_TYPES.contains(&unit.unit_type) {
        2
    } else if garrisons {
        1
    } else {
        0
    }
}

/// `Fill`'s bars.
pub fn fill_bars(unit: &EeScreensHudUnit) -> EeScreensHudFillBars {
    let mut out = EeScreensHudFillBars {
        hit_points: unit.class_hit_points > 0 && unit.category != CATEGORY_RESOURCE,
        ..Default::default()
    };
    if !unit.complex {
        return out;
    }
    let garrisons = unit.class_garrison > 0 && unit.friendly;
    if unit.class_flight_time > 0 && unit.category != CATEGORY_AIR_ANIMAL {
        out.kind = 1;
        out.max = unit.stats_flight_time / 1000;
        out.current = unit.flight_time / 1000;
    } else if unit.stats_power > 0 {
        out.kind = 2;
        out.bar = power_bar(unit, garrisons);
        out.max = unit.stats_power;
        out.current = unit.power;
    }
    if garrisons {
        out.garrison = true;
        out.garrison_max = unit.class_garrison;
        out.garrison_current = unit.garrison_count;
    }
    out
}

/// What `Refresh` shows: each text's numbers (the C++ formats them: "%ld/%ld", "+ %ld", "%ld+%ld", "%ld") and
/// each bar's range and current value.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensHudRefresh {
    pub hit_points: bool,
    pub hit_points_text: [c_long; 2],
    pub hit_points_bonus: c_long,
    /// The attack, range, speed, area effect and repair icons' "base+bonus".
    pub attributes: [[c_long; 2]; 5],
    pub cargo: [c_long; 2],
    /// 0 none, 1 flight time (the second bar), 2 power (on `bar`, as [`power_bar`]).
    pub kind: u32,
    pub bar: u32,
    pub bar_text: [c_long; 2],
    pub bar_bonus: c_long,
    pub bar_max: c_long,
    pub bar_current: c_long,
    pub garrison: bool,
    pub garrison_text: [c_long; 2],
    pub garrison_max: c_long,
    pub garrison_current: c_long,
    /// The carried resource: shown, its icon's bitmap (-1: the icon left as it is), "%ld/%ld" of the amount and
    /// the capacity (a citizen) or "%ld" of the amount.
    pub resource: bool,
    pub resource_bitmap: c_long,
    pub resource_pair: bool,
    pub resource_text: [c_long; 2],
}

/// `(long)((float)v + 0.5f)`.
fn rounded(v: c_long) -> c_long {
    float_to_long(v as f32 + 0.5)
}

/// "+ %ld" of a bonus: `theBonus >= 0.0f ? (long)(theBonus + 0.5f) : 0`.
fn bonus(v: f32) -> c_long {
    if v >= 0.0 {
        float_to_long(v + 0.5)
    } else {
        0
    }
}

fn pair(base: f32, bonus: f32) -> [c_long; 2] {
    let (a, b) = attribute_text(base, bonus);
    [a, b]
}

fn nudge(below: bool) -> f32 {
    if below {
        -0.0001
    } else {
        0.0001
    }
}

/// `Refresh`'s texts and bars.
pub fn refresh(unit: &EeScreensHudUnit) -> EeScreensHudRefresh {
    let u = unit;
    let mut out = EeScreensHudRefresh { resource_bitmap: -1, ..Default::default() };
    if u.class_hit_points > 0 {
        out.hit_points = true;
        out.hit_points_text = [rounded(u.hit_points), rounded(u.max_hit_points)];
        out.hit_points_bonus = bonus(u.max_hit_points.wrapping_sub(u.stats_class_hit_points) as f32);
    }
    let attack = (u.stats_attack - u.stats_class_attack as f32) + nudge(u.stats_attack < u.class_attack as f32);
    out.attributes[0] = pair(u.class_attack as f32, attack);
    let range = (u.stats_range - u.stats_class_range) + nudge(u.stats_range < u.class_range);
    out.attributes[1] = pair(u.class_range, range);
    let speed = (u.stats_speed - u.stats_class_speed) * 20.0 + nudge(u.stats_speed < u.class_speed);
    out.attributes[2] = pair(u.class_speed * 20.0 + 0.0001, speed);
    let area =
        (u.stats_area_effect - u.stats_class_area_effect) * 10.0 + nudge(u.stats_area_effect < u.class_area_effect);
    out.attributes[3] = pair(u.class_area_effect * 10.0 + 0.0001, area);
    out.attributes[4] = pair(u.class_repair as f32, u.stats_repair.wrapping_sub(u.stats_class_repair) as f32);
    if u.complex {
        // the cargo: an `unsigned long` amount
        let amount = u.complex_class_transport.wrapping_sub(u.cargo_room) as c_ulong;
        out.cargo = [float_to_long(amount as f32 + 0.5), rounded(u.class_transport)];
        if u.class_flight_time > 0 {
            out.kind = 1;
            out.bar_text = [rounded(u.flight_time / 1000), rounded(u.stats_flight_time / 1000)];
            out.bar_current = u.flight_time / 1000;
            out.bar_max = u.stats_flight_time / 1000;
            out.bar_bonus = bonus((u.stats_flight_time.wrapping_sub(u.stats_class_flight_time) / 1000) as f32);
        } else if u.stats_power > 0 {
            out.kind = 2;
            out.bar = power_bar(u, u.class_garrison > 0);
            out.bar_text = [rounded(u.power), rounded(u.stats_power)];
            out.bar_current = u.power;
            out.bar_max = u.stats_power;
            out.bar_bonus = bonus(u.stats_power.wrapping_sub(u.stats_class_power) as f32);
        }
        if u.class_garrison > 0 {
            out.garrison = true;
            out.garrison_text = [rounded(u.garrison_count), rounded(u.complex_class_garrison)];
            out.garrison_current = u.garrison_count;
            out.garrison_max = u.complex_class_garrison;
        }
    }
    // the resource types: none 0, any 1, food 2 .. iron 6 (`ee/EEResourceType.h`)
    if u.resource_type != 0 && u.resource_type != 1 && u.resource_amount > 0 {
        out.resource = true;
        out.resource_bitmap = match u.resource_type {
            2 => 34,
            3 if u.using_carbon => 48,
            3 => 32,
            4 => 28,
            5 => 30,
            6 => 36,
            _ => -1,
        };
        if u.category == CATEGORY_LAND_CITIZEN || u.category == CATEGORY_NAVAL_CITIZEN {
            out.resource_pair = true;
            out.resource_text = [rounded(u.resource_amount), rounded(u.carry_amount)];
        } else {
            out.resource_text = [u.resource_amount, 0];
        }
    }
    out
}

#[cfg(test)]
mod golden;

#[cfg(test)]
mod golden_box;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn attribute_texts() {
        assert_eq!(attribute_text(12.0, 3.4), (12, 3));
        assert_eq!(attribute_text(12.0, 2.5), (12, 3));
        assert_eq!(attribute_text(12.0, -2.6), (9, 0));
        assert_eq!(attribute_text(f32::NAN, 1.0), (c_long::MIN, 1));
        assert_eq!(attribute_text(1.0, f32::NAN), (1, 0));
    }

    #[test]
    fn armor_nudges_away_from_ties() {
        let (base, bonus) = armor_attribute(5.0, 5, 5);
        assert_eq!((base, bonus), (5.0, 0.0001));
        let (_, below) = armor_attribute(4.0, 5, 5);
        assert_eq!(below, -1.0001);
        assert_eq!(ARMOR_KINDS.iter().sum::<u32>(), 15);
    }

    #[test]
    fn fill_shows_the_attributes_in_order() {
        let unit = EeScreensHudUnit {
            class_attack: 5,
            class_speed: 1.0,
            upgrades_repair: true,
            upgrades_armor: [true, false, true, false, false, true],
            class_transport: 4,
            friendly: true,
            ..Default::default()
        };
        let first = fill_icons(&unit);
        let shown: Vec<_> = first.icons[..5].iter().map(|e| (e.slot, e.icon)).collect();
        assert_eq!(shown, [(0, 0), (1, -1), (2, 1), (3, -1), (10, 2)]);
        assert_eq!(first.next, 3);
        let armor = armor_icons(first.next, unit.upgrades_armor);
        let shown: Vec<_> = armor.icons[..6].iter().map(|e| (e.icon, e.rect_icon, e.bitmap)).collect();
        assert_eq!(shown, [(3, 3, 8), (-1, 3, 10), (4, 3, 12), (-1, 3, 14), (-1, 3, 16), (5, 3, 18)]);
        let transport = transport_icon(armor.next, &unit);
        assert_eq!((transport.icons[0].slot, transport.icons[0].icon, transport.next), (12, 6, 7));
        let naval = EeScreensHudUnit { category: CATEGORY_NAVAL_CITIZEN, ..unit };
        assert_eq!(fill_icons(&naval).icons[0].icon, -1);
    }

    #[test]
    fn the_bars_and_the_power_bar() {
        let mut unit =
            EeScreensHudUnit { complex: true, stats_power: 50, power: 20, two_bars: true, ..Default::default() };
        assert_eq!(fill_bars(&unit).kind, 2);
        assert_eq!(fill_bars(&unit).bar, 0);
        unit.unit_type = 0x2d9;
        assert_eq!(fill_bars(&unit).bar, 2);
        unit.unit_type = 1;
        unit.class_garrison = 5;
        assert_eq!(fill_bars(&unit).bar, 0, "Fill asks for a friendly owner");
        assert_eq!(refresh(&unit).bar, 1, "Refresh does not");
        unit.friendly = true;
        assert_eq!(fill_bars(&unit).bar, 1);
        assert!(fill_bars(&unit).garrison);
        unit.class_flight_time = 3000;
        unit.stats_flight_time = 45999;
        unit.flight_time = 1999;
        let bars = fill_bars(&unit);
        assert_eq!((bars.kind, bars.max, bars.current), (1, 45, 1));
        unit.category = CATEGORY_AIR_ANIMAL;
        assert_eq!(fill_bars(&unit).kind, 2);
        assert_eq!(refresh(&unit).kind, 1, "Refresh shows an air animal's flight time");
    }

    #[test]
    fn refresh_texts() {
        let unit = EeScreensHudUnit {
            class_hit_points: 100,
            hit_points: 37,
            max_hit_points: 120,
            stats_class_hit_points: 100,
            complex: true,
            complex_class_transport: 5,
            cargo_room: 7,
            resource_type: 3,
            resource_amount: 9,
            carry_amount: 10,
            category: CATEGORY_LAND_CITIZEN,
            using_carbon: true,
            ..Default::default()
        };
        let plan = refresh(&unit);
        assert_eq!((plan.hit_points_text, plan.hit_points_bonus), ([37, 120], 20));
        // the cargo is an unsigned long: a negative amount wraps
        assert_eq!(plan.cargo[0], float_to_long((-2i64 as c_ulong) as f32 + 0.5));
        assert_eq!((plan.resource_bitmap, plan.resource_pair, plan.resource_text), (48, true, [9, 10]));
        let none = refresh(&EeScreensHudUnit { resource_type: 1, resource_amount: 9, ..unit });
        assert!(!none.resource);
    }
}
