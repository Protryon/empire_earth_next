//! The settings form's display questions in Rust over one-statement shims (UI lane round 12; were `EEScreens.cpp`'s
//! `ee_screenscpp_modes`, `_current_mode` and `_game_size`): a rasterizer's screen's video modes (the display devices'
//! table, `gDisplayRasterizers`), a screen's current mode, the game's window size at a percentage.
#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64

use core::ffi::c_ulong;

use crate::screens::game_cpp::{Cpp, EeScreensShimULongOut};
use crate::screens::settings::EeScreensVideoMode;

/// The video modes of screen `screen` (by its rasterizer index) of rasterizer `rasterizer` (by its index): the first
/// matching screen's, in the table's order; none without one.
pub(crate) fn modes<C: Cpp + ?Sized>(c: &mut C, rasterizer: c_ulong, screen: c_ulong) -> Vec<EeScreensVideoMode> {
    for r in 0..c.disp_rasterizer_count() {
        if c.disp_rasterizer_index(r) as c_ulong != rasterizer {
            continue;
        }
        for s in 0..c.disp_screen_count(r) {
            if c.disp_screen_index(r, s) as c_ulong != screen {
                continue;
            }
            return (0..c.disp_mode_count(r, s))
                .map(|m| EeScreensVideoMode {
                    width: c.disp_mode_width(r, s, m) as c_ulong,
                    height: c.disp_mode_height(r, s, m) as c_ulong,
                    refresh_rate: c.disp_mode_refresh(r, s, m) as c_ulong,
                    bit_count: c.disp_mode_bits(r, s, m) as c_ulong,
                })
                .collect();
        }
    }
    Vec::new()
}

/// The screen's current video mode.
pub(crate) fn current_mode<C: Cpp + ?Sized>(c: &mut C, screen: c_ulong) -> EeScreensVideoMode {
    let screen = screen as u32;
    EeScreensVideoMode {
        width: c.disp_current_width(screen) as c_ulong,
        height: c.disp_current_height(screen) as c_ulong,
        refresh_rate: c.disp_current_refresh(screen) as c_ulong,
        bit_count: c.disp_current_bits(screen) as c_ulong,
    }
}

/// `GXGameSize(percent, windowed)` -> (width, height).
pub(crate) fn game_size<C: Cpp + ?Sized>(c: &mut C, percent: c_ulong, windowed: bool) -> (c_ulong, c_ulong) {
    let (mut w, mut h): (c_ulong, c_ulong) = (0, 0);
    c.disp_game_size(percent as u32, windowed, EeScreensShimULongOut::of(&mut w), EeScreensShimULongOut::of(&mut h));
    (w, h)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn the_first_matching_screen_gives_the_modes() {
        let mut fake = Fake::default();
        fake.answer("disp_rasterizer_count", &[2]).answer("disp_rasterizer_index", &[0, 1]);
        fake.answer("disp_screen_count", &[1]).answer("disp_screen_index", &[3]).answer("disp_mode_count", &[2]);
        fake.answer("disp_mode_width", &[800, 1024]).answer("disp_mode_height", &[600, 768]);
        let m = modes(&mut fake, 1, 3);
        assert_eq!(m.len(), 2);
        assert_eq!((m[1].width, m[1].height), (1024, 768));
        let mut fake = Fake::default();
        fake.answer("disp_rasterizer_count", &[1]).answer("disp_rasterizer_index", &[1]);
        fake.answer("disp_screen_count", &[1]).answer("disp_screen_index", &[0]);
        assert!(modes(&mut fake, 1, 3).is_empty());
    }

    #[test]
    fn the_current_mode_and_the_game_size() {
        let mut fake = Fake::default();
        fake.answer("disp_current_width", &[1280]).answer("disp_current_bits", &[32]);
        let m = current_mode(&mut fake, 0);
        assert_eq!((m.width, m.bit_count), (1280, 32));
        let mut fake = Fake::default();
        fake.out("disp_game_size", 640).out("disp_game_size", 480);
        assert_eq!(game_size(&mut fake, 50, true), (640, 480));
    }
}
