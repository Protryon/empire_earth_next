//! The window layer's decisions, without the SDL calls (those are in `ee_plat_sdl`).

/// `PlatNativeWindow`
#[repr(C)]
#[derive(Clone, Copy)]
pub struct PlatNativeWindow {
    pub kind: i32,
    pub display: *mut std::ffi::c_void,
    pub window: *mut std::ffi::c_void,
}

/// `PLAT_WAIT_INFINITE`
pub const PLAT_WAIT_INFINITE: u32 = 0xFFFF_FFFF;

pub const PLAT_WINDOW_RESIZABLE: u32 = 0x1;
pub const PLAT_WINDOW_FULLSCREEN: u32 = 0x2;
pub const PLAT_WINDOW_HIDDEN: u32 = 0x4;
pub const PLAT_WINDOW_HIGH_DPI: u32 = 0x8;
pub const PLAT_WINDOW_BORDERLESS: u32 = 0x10;
pub const PLAT_WINDOW_TOPMOST: u32 = 0x20;

pub const SDL_WINDOW_FULLSCREEN: u64 = 0x0000_0000_0000_0001;
pub const SDL_WINDOW_HIDDEN: u64 = 0x0000_0000_0000_0008;
pub const SDL_WINDOW_BORDERLESS: u64 = 0x0000_0000_0000_0010;
pub const SDL_WINDOW_RESIZABLE: u64 = 0x0000_0000_0000_0020;
pub const SDL_WINDOW_HIGH_PIXEL_DENSITY: u64 = 0x0000_0000_0000_2000;
pub const SDL_WINDOW_ALWAYS_ON_TOP: u64 = 0x0000_0000_0001_0000;
pub const SDL_WINDOW_METAL: u64 = 0x0000_0000_2000_0000;

/// `SDL_WINDOWPOS_UNDEFINED`
pub const SDL_WINDOWPOS_UNDEFINED: i32 = 0x1FFF_0000;

/// SDL window flags of `PLAT_WINDOW_*`; `metal` on macOS.
pub fn sdl_window_flags(flags: u32, metal: bool) -> u64 {
    let mut out = 0u64;
    if flags & PLAT_WINDOW_RESIZABLE != 0 {
        out |= SDL_WINDOW_RESIZABLE;
    }
    if flags & PLAT_WINDOW_FULLSCREEN != 0 {
        out |= SDL_WINDOW_FULLSCREEN;
    }
    if flags & PLAT_WINDOW_HIDDEN != 0 {
        out |= SDL_WINDOW_HIDDEN;
    }
    if flags & PLAT_WINDOW_HIGH_DPI != 0 {
        out |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }
    if flags & PLAT_WINDOW_BORDERLESS != 0 {
        out |= SDL_WINDOW_BORDERLESS;
    }
    if flags & PLAT_WINDOW_TOPMOST != 0 {
        out |= SDL_WINDOW_ALWAYS_ON_TOP;
    }
    if metal {
        out |= SDL_WINDOW_METAL;
    }
    out
}

/// `PlatWindowSetSize`: a full screen window keeps the display's size (the game's mode is its render size, which GX
/// scales to the window), and a fixed window keeps the player's.
pub fn set_size_applies(fullscreen: bool, fixed_size: bool, sdl_flags: u64) -> bool {
    !(fullscreen || fixed_size || (sdl_flags & SDL_WINDOW_FULLSCREEN) != 0)
}

/// One step of `PlatWindowWaitEvent`'s loop: `None` when the timeout has passed (the loop's last poll),
/// otherwise the milliseconds for `SDL_WaitEventTimeout`, -1 for an infinite wait.
pub fn wait_timeout(timeout: u32, start: u32, now: u32) -> Option<i32> {
    if timeout == PLAT_WAIT_INFINITE {
        return Some(-1);
    }
    let elapsed = now.wrapping_sub(start);
    if elapsed >= timeout {
        None
    } else {
        Some(timeout.wrapping_sub(elapsed) as i32)
    }
}

/// `SDL_GetDesktopDisplayMode` to physical pixels: `w * density` truncated, a density of 0 or less counts as 1.
pub fn display_pixels(w: i32, h: i32, pixel_density: f32) -> (i32, i32) {
    let density = if pixel_density > 0.0 { pixel_density } else { 1.0 };
    ((w as f32 * density) as i32, (h as f32 * density) as i32)
}

/// `PlatMouseGetState` with a window: the desktop position to client pixels.
pub fn global_to_client(x: f32, y: f32, window_x: i32, window_y: i32, scale: f32) -> (f32, f32) {
    ((x - window_x as f32) * scale, (y - window_y as f32) * scale)
}

/// `PlatMouseWarp`: client pixels to window coordinates; a density of 0 or less counts as 1.
pub fn warp_target(x: i32, y: i32, scale: f32) -> (f32, f32) {
    let s = if scale <= 0.0 { 1.0 } else { scale };
    (x as f32 / s, y as f32 / s)
}

/// `PlatMouseGetState`: SDL's mask bits (L, M, R as 1, 2, 4 in `SDL_GetGlobalMouseState`) to bits 0, 1, 2.
pub fn mouse_buttons(mask: u32) -> u32 {
    let mut out = 0;
    if mask & 1 != 0 {
        out |= 1;
    }
    if mask & 2 != 0 {
        out |= 2;
    }
    if mask & 4 != 0 {
        out |= 4;
    }
    out
}

/// `PlatCursorSetSystem`'s index: out-of-range cursors are the arrow.
pub fn system_cursor_index(cursor: u32) -> usize {
    let i = cursor as usize;
    if i < 6 {
        i
    } else {
        0
    }
}

/// `PLAT_CURSOR_*` to `SDL_SYSTEM_CURSOR_*`: arrow, hand, I-beam, size W-E, size N-S, size all.
pub const SYSTEM_CURSORS: [i32; 6] = [0, 11, 1, 7, 8, 9]; // DEFAULT, POINTER, TEXT, EW_RESIZE, NS_RESIZE, MOVE

/// `SDL_MESSAGEBOX_*` of `PLAT_MESSAGE_*`.
pub const fn message_box_flags(flags: u32) -> u32 {
    if flags & 0x10 != 0 {
        0x10 // SDL_MESSAGEBOX_ERROR
    } else {
        0x40 // SDL_MESSAGEBOX_INFORMATION
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn window_flags_map_one_to_one() {
        assert_eq!(sdl_window_flags(0, false), 0);
        assert_eq!(sdl_window_flags(PLAT_WINDOW_RESIZABLE, false), SDL_WINDOW_RESIZABLE);
        assert_eq!(sdl_window_flags(PLAT_WINDOW_FULLSCREEN, false), SDL_WINDOW_FULLSCREEN);
        assert_eq!(sdl_window_flags(PLAT_WINDOW_HIDDEN, false), SDL_WINDOW_HIDDEN);
        assert_eq!(sdl_window_flags(PLAT_WINDOW_HIGH_DPI, false), SDL_WINDOW_HIGH_PIXEL_DENSITY);
        assert_eq!(sdl_window_flags(PLAT_WINDOW_BORDERLESS, false), SDL_WINDOW_BORDERLESS);
        assert_eq!(sdl_window_flags(PLAT_WINDOW_TOPMOST, false), SDL_WINDOW_ALWAYS_ON_TOP);
        assert_eq!(sdl_window_flags(0x3f, true), 0x2001_2039);
    }

    #[test]
    fn resize_is_refused_for_fullscreen_and_fixed() {
        assert!(set_size_applies(false, false, 0));
        assert!(!set_size_applies(true, false, 0));
        assert!(!set_size_applies(false, true, 0));
        assert!(!set_size_applies(false, false, SDL_WINDOW_FULLSCREEN));
        assert!(set_size_applies(false, false, SDL_WINDOW_RESIZABLE));
    }

    #[test]
    fn wait_timeouts() {
        assert_eq!(wait_timeout(PLAT_WAIT_INFINITE, 100, 200), Some(-1));
        assert_eq!(wait_timeout(50, 100, 100), Some(50));
        assert_eq!(wait_timeout(50, 100, 149), Some(1));
        assert_eq!(wait_timeout(50, 100, 150), None);
        assert_eq!(wait_timeout(0, 100, 100), None); // a zero wait polls once
                                                     // the millisecond clock wraps
        assert_eq!(wait_timeout(50, u32::MAX - 10, 5), Some(50 - 16));
    }

    #[test]
    fn display_pixels_uses_the_density() {
        assert_eq!(display_pixels(1920, 1080, 1.0), (1920, 1080));
        assert_eq!(display_pixels(1512, 982, 2.0), (3024, 1964));
        assert_eq!(display_pixels(100, 100, 0.0), (100, 100));
        assert_eq!(display_pixels(100, 100, -1.0), (100, 100));
        assert_eq!(display_pixels(101, 100, 1.5), (151, 150));
    }

    #[test]
    fn mouse_arithmetic() {
        assert_eq!(global_to_client(110.0, 220.0, 100, 200, 2.0), (20.0, 40.0));
        assert_eq!(warp_target(100, 50, 2.0), (50.0, 25.0));
        assert_eq!(warp_target(100, 50, 0.0), (100.0, 50.0));
        assert_eq!(warp_target(100, 50, -2.0), (100.0, 50.0));
    }

    #[test]
    fn button_bits_are_left_middle_right() {
        assert_eq!(mouse_buttons(0), 0);
        assert_eq!(mouse_buttons(1), 1);
        assert_eq!(mouse_buttons(2), 2);
        assert_eq!(mouse_buttons(4), 4);
        assert_eq!(mouse_buttons(7 | 16), 7);
    }

    #[test]
    fn system_cursor_clamps_and_table_is_sdl_values() {
        assert_eq!(system_cursor_index(0), 0);
        assert_eq!(system_cursor_index(5), 5);
        assert_eq!(system_cursor_index(6), 0);
        assert_eq!(system_cursor_index(u32::MAX), 0);
        // SDL_mouse.h's enum values, in PLAT_CURSOR_* order: arrow, hand, I-beam, size WE, size NS, size all
        assert_eq!(SYSTEM_CURSORS, [0, 11, 1, 7, 8, 9]);
    }

    #[test]
    fn native_window_layout_matches_the_c_header() {
        use std::mem::{offset_of, size_of};
        let golden = include_str!("golden_window.txt");
        let get = |name: &str| -> usize {
            golden
                .lines()
                .filter_map(|l| l.strip_prefix("pl "))
                .find_map(|l| l.strip_prefix(name).map(|r| r.trim().parse().unwrap()))
                .unwrap()
        };
        assert_eq!(size_of::<PlatNativeWindow>(), get("sizeof_PlatNativeWindow "));
        assert_eq!(offset_of!(PlatNativeWindow, kind), get("PlatNativeWindow.mKind "));
        assert_eq!(offset_of!(PlatNativeWindow, display), get("PlatNativeWindow.mDisplay "));
        assert_eq!(offset_of!(PlatNativeWindow, window), get("PlatNativeWindow.mWindow "));
    }

    #[test]
    fn message_box_flag_mapping() {
        assert_eq!(message_box_flags(0), 0x40);
        assert_eq!(message_box_flags(0x10), 0x10);
        assert_eq!(message_box_flags(0x40), 0x40);
    }
}
