//! More of `EEScreens.cpp`'s C++ in Rust over one-statement shims (UI lane round 10): the aircraft hot keys'
//! target (the render viewport's centre on the map, else its tile's centre), the hot keys' kept units and the
//! forms' list and combo box selections.

use core::ffi::{c_long, c_ulong};

use crate::screens::game_cpp::{Cpp, EeScreensShimLongOut as LongOut, EeScreensShimULongOut as ULongOut};

/// `kErrorFailed`.
const ERROR_FAILED: i32 = 9;

/// The aircraft hot keys' target: the point under the render viewport's centre, else its tile's centre.
pub(crate) fn hot_key_target<C: Cpp + ?Sized>(c: &mut C) -> [f32; 2] {
    c.hk_target_reset();
    if !c.hk_location_at() {
        let (mut x, mut y): (c_long, c_long) = (0, 0);
        c.hk_tile_at(LongOut::of(&mut x), LongOut::of(&mut y));
        return [x as f32 + 0.5, y as f32 + 0.5];
    }
    [c.hk_target_x(), c.hk_target_y()]
}

/// The snapshot's complex units kept for `HK_CENTER`.
pub(crate) fn hot_key_keep<C: Cpp + ?Sized>(c: &mut C, units: &[u64]) {
    c.hk_units_clear();
    for &unit in units {
        c.hk_units_push(unit);
    }
}

/// A form's control's selection (`list`: a string list, else a combo box): the result (failed without the
/// control) and the selection (0 then).
pub(crate) fn selection<C: Cpp + ?Sized>(c: &mut C, form: usize, control: u32, list: bool) -> (i32, u32) {
    let ctl = c.sel_control(form, control);
    let mut selection: c_ulong = 0;
    let mut result = ERROR_FAILED;
    if ctl != 0 && list {
        result = c.sel_list(ctl, ULongOut::of(&mut selection));
    } else if ctl != 0 {
        result = c.sel_combo(ctl, ULongOut::of(&mut selection));
    }
    (result, selection as u32)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn target_on_the_map_or_its_tile() {
        let mut fake = Fake::default();
        fake.out("hk_tile_at", 3).out("hk_tile_at", 4);
        assert_eq!(hot_key_target(&mut fake), [3.5, 4.5]);
        let mut fake = Fake::default();
        fake.answer("hk_location_at", &[1]).answer("hk_target_x", &[7]).answer("hk_target_y", &[8]);
        assert_eq!(hot_key_target(&mut fake), [7.0, 8.0]);
        assert_eq!(fake.calls(), "hk_target_reset()\nhk_location_at()\nhk_target_x()\nhk_target_y()");
        let mut fake = Fake::default();
        hot_key_keep(&mut fake, &[5, 6]);
        assert_eq!(fake.calls(), "hk_units_clear()\nhk_units_push(5)\nhk_units_push(6)");
    }

    #[test]
    fn selections() {
        let mut fake = Fake::default();
        assert_eq!(selection(&mut fake, 1, 2, true), (9, 0));
        fake.answer("sel_control", &[5, 5]).out("sel_list", 3).out("sel_combo", 4);
        assert_eq!(selection(&mut fake, 1, 2, true), (0, 3));
        assert_eq!(selection(&mut fake, 1, 2, false), (0, 4));
        assert_eq!(
            fake.calls(),
            "sel_control(1, 2)\nsel_control(1, 2)\nsel_list(5, out)\nsel_control(1, 2)\nsel_combo(5, out)"
        );
    }
}
