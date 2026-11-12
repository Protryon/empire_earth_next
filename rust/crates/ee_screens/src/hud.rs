//! The in-game HUD's status box (`User Interface Thread/EEUIFMGStatusBox.cpp`): the attribute texts the box shows
//! for the selected unit ("base+bonus": `GetAttributeText`), and the armor attributes' values (`UpdateArmorTexts`:
//! each armor kind's base from the unit's class, its bonus from the unit's stats over their own class, nudged away
//! from a rounding tie by 0.0001).
//!
//! The C++ keeps the unit, its class and stats (read under the world's read lock, as the UI thread updates the
//! forms), the icons and the labels; it hands the numbers over and formats and shows the result.

use core::ffi::c_long;

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

#[cfg(test)]
mod golden;

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
}
