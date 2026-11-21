//! The status box's host (`EEUIFMGStatusBox::HostOp`, `EE_SCREENS_BOX_OP_*`) and its icons' unload and reload in
//! Rust over one-statement C++ shims (UI lane round 8; the shims are `ee_screenscpp_sb_*` in
//! `User Interface Thread/EEUIFMGStatusBox.cpp`). `call` is the host's record of one status box call (the box, the
//! unit, the complex unit, the held text, LoadIcons' description and textures). The ops `hud_direct.rs` answers
//! from the record itself never get here.

use core::ffi::c_long;

use crate::screens::game_cpp::{Cpp, EeScreensShimText};
use crate::screens::status_box::*;

/// The three unit improvement textures (`LoadIcons`' index 1..3; 0 is unused).
const SB_TEXTURE_NAMES: [&[u8]; 4] =
    [b"", b"textures\\unit improvements_01", b"textures\\unit improvements_02", b"textures\\unit upgrades_03"];

/// The two f32s of one host argument (status_box.rs `pack`: the first in the low 32 bits).
fn sb_pair(v: i64) -> (f32, f32) {
    (f32::from_bits(v as u64 as u32), f32::from_bits((v as u64 >> 32) as u32))
}

/// The status box texture description (`LoadIcons`' and `ReloadIcons`'): a mip level dropped on screens up to 800
/// pixels wide.
fn sb_drop_mip(c: &mut impl Cpp) -> i32 {
    (c.sb_primary_width() <= 800) as i32
}

/// One status box host op on the call record `call` (0 for an op the host does not know, as the C++'s).
pub(crate) fn call(c: &mut impl Cpp, call: usize, op: u32, a: i64, b: i64, cc: i64) -> i64 {
    match op {
        EE_SCREENS_BOX_OP_QUEUED => {
            if c.sb_upgrade_queued(call, a) != b as c_long as i64 {
                c.sb_control_set_dirty(call, a);
            }
            c.sb_upgrade_set_queued(call, a, b);
        }
        EE_SCREENS_BOX_OP_ATTRIBUTE => return c.sb_upgrade_attribute(call, a),
        EE_SCREENS_BOX_OP_SET_ATTRIBUTE => c.sb_upgrade_set_attribute(call, a, b),
        EE_SCREENS_BOX_OP_COST_LABEL => {
            if b < 0 {
                c.sb_upgrade_cost_label_clear(call, a)
            } else {
                c.sb_upgrade_cost_label(call, a, b)
            }
        }
        EE_SCREENS_BOX_OP_NAME => {
            // Fill (a 0) compares the stats' name with the pooled empty string, Refresh with ""
            c.sb_suffix_clear();
            let empty = if a == 0 { c.sb_name_empty_fill(call) } else { c.sb_name_empty(call) };
            if empty {
                c.sb_text_class_name(call);
            } else {
                c.sb_text_stats_name(call);
            }
            let gate = if a == 0 { c.sb_is_gate_fill(call) } else { c.sb_is_gate(call) };
            if gate {
                if c.sb_gate_locked(call) {
                    c.sb_suffix_locked();
                } else {
                    c.sb_suffix_unlocked();
                }
                c.sb_text_append_suffix(call);
            }
        }
        EE_SCREENS_BOX_OP_CIV => c.sb_text_civ(call),
        EE_SCREENS_BOX_OP_BAR_ICON => {
            let bar = c.sb_bar_control(call, a);
            let bitmap = c.sb_bitmap(b);
            let rectangle = c.sb_icon_rect(b);
            set_icon_bitmap(c, bar, bitmap, rectangle);
        }
        EE_SCREENS_BOX_OP_BAR_RANGE => c.sb_bar_range(call, a, b),
        EE_SCREENS_BOX_OP_BAR_VALUE => c.sb_bar_value(call, a, b),
        EE_SCREENS_BOX_OP_BAR_COLOR => c.sb_bar_color(call, a, if b == 1 || b == 2 { b as u32 } else { 0 }),
        EE_SCREENS_BOX_OP_BAR_ENABLED => c.sb_bar_enabled(call, a, b != 0),
        EE_SCREENS_BOX_OP_STRETCH => c.sb_stretch(call, a, b, cc),
        EE_SCREENS_BOX_OP_SET_BITMAP => {
            if b == 0 {
                c.sb_button_set_normal(call, a, cc)
            } else {
                c.sb_button_set_pressed(call, a, cc)
            }
        }
        EE_SCREENS_BOX_OP_BITMAP_IS => {
            return if b == 0 { c.sb_button_normal_is(call, a, cc) } else { c.sb_button_pressed_is(call, a, cc) } as i64
        }
        EE_SCREENS_BOX_OP_PORTRAIT => {
            if a != 0 {
                c.sb_portrait_set_unit(call)
            } else {
                c.sb_portrait_clear(call)
            }
        }
        EE_SCREENS_BOX_OP_PORTRAIT_UPDATE => c.sb_portrait_update(call),
        EE_SCREENS_BOX_OP_VERIFY => {
            if a == 367 {
                c.sb_verify_armor_count()
            } else {
                c.sb_verify_count()
            }
        }
        EE_SCREENS_BOX_OP_BACKGROUND => {
            let background = c.sb_main_background(a as u32);
            if background != 0 {
                c.sb_background_set_visible(background, b != 0);
            }
        }
        EE_SCREENS_BOX_OP_VALUE_DIFFERS => {
            return if b == 0 { c.sb_value_differs(call, a) } else { c.sb_value2_differs(call, a) } as i64
        }
        EE_SCREENS_BOX_OP_SET_VALUE => {
            if b == 0 {
                c.sb_set_value(call, a)
            } else {
                c.sb_set_value2(call, a)
            }
        }
        EE_SCREENS_BOX_OP_UPDATE_COST => c.sb_upgrade_update_cost(call, a),
        EE_SCREENS_BOX_OP_ARMOR => {
            return match b {
                1 => c.sb_class_armor(call, a),
                2 => c.sb_stats_class_armor(call, a),
                _ => c.sb_stats_armor(call, a).to_bits() as i64,
            }
        }
        EE_SCREENS_BOX_OP_UNIT_TYPE => return c.sb_complex_unit_type(call),
        EE_SCREENS_BOX_OP_COUNT_QUEUED => return c.sb_count_queued(call, a, b),
        EE_SCREENS_BOX_OP_NEW_BITMAP => c.sb_new_bitmap(a),
        EE_SCREENS_BOX_OP_DESCRIPTION => {
            let drop = sb_drop_mip(c);
            c.sb_set_description(call, drop);
        }
        EE_SCREENS_BOX_OP_LOAD_TEXTURE => {
            let description = c.sb_description(call);
            let name = SB_TEXTURE_NAMES.get(a as usize).copied().unwrap_or(b"");
            let texture = c.texture_load(EeScreensShimText::of(name), description);
            c.texture_alpha_modulate(texture);
            c.texture_color_modulate(texture);
            c.texture_min_filter(texture, 1);
            c.texture_mag_filter(texture, 1);
            c.texture_mip_point(texture);
            c.texture_mip_bias(texture, -0.75);
            c.texture_clamp(texture);
            c.sb_set_texture(call, a, texture);
        }
        EE_SCREENS_BOX_OP_SET_RECT => {
            let ((left, top), (right, bottom)) = (sb_pair(b), sb_pair(cc));
            c.sb_set_rect(a, left, top, right, bottom);
        }
        EE_SCREENS_BOX_OP_TEXTURE_SIZE => {
            let width = c.sb_texture_width(call, a) as u64;
            return (width << 32 | c.sb_texture_height(call, a) as u64) as i64;
        }
        EE_SCREENS_BOX_OP_BITMAP_TEXTURE => {
            let ((u0, v0), (u1, v1)) = (sb_pair(b), sb_pair(cc));
            c.sb_bitmap_set_texture(a & 0xff, call, a >> 8);
            c.sb_bitmap_set_coordinates(a & 0xff, u0, v0, u1, v1);
        }
        EE_SCREENS_BOX_OP_BITMAP_SCALE => {
            let (x, y) = sb_pair(b);
            c.sb_bitmap_scale(a, x, y);
        }
        EE_SCREENS_BOX_OP_ICONS_LOADED => c.sb_icons_set_loaded(true),
        _ => {}
    }
    0
}

/// The original's `EEUIFMGStatusBar::SetIconBitmap` (bar, bitmap and texture rectangle as addresses): the icon shows the bitmap,
/// stretched to the icon area; the bar is dirty.
fn set_icon_bitmap(c: &mut impl Cpp, bar: usize, bitmap: usize, rectangle: usize) {
    c.sb_bar_bitmap(bar, bitmap);
    c.sb_bar_stretch(bar, bitmap, rectangle);
    c.sb_bar_dirty(bar);
}

/// `EEUIFMGStatusBox::UnloadIcons`: the 50 attribute icon bitmaps released.
pub(crate) fn unload_icons(c: &mut impl Cpp) {
    for icon in 0..50 {
        if c.sb_bitmap(icon) != 0 {
            c.sb_bitmap_dereference(icon);
            c.sb_bitmap_clear(icon);
        }
    }
    c.sb_icons_set_loaded(false);
}

/// `EEUIFMGStatusBox::ReloadIcons`: the three textures reloaded (after a device loss); the first error.
pub(crate) fn reload_icons(c: &mut impl Cpp) -> i64 {
    let drop = sb_drop_mip(c);
    c.sb_set_reload_description(drop);
    for index in 1..4 {
        let r = c.sb_reload(index);
        if r != 0 {
            return r;
        }
    }
    0
}

#[cfg(not(test))]
mod ffi {
    use crate::screens::game_cpp::Ffi;

    /// `EEUIFMGStatusBox::UnloadIcons`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sb_unload_icons() {
        super::unload_icons(&mut Ffi)
    }
    /// `EEUIFMGStatusBox::ReloadIcons`: the first error.
    #[no_mangle]
    pub extern "C" fn ee_screens_sb_reload_icons() -> i64 {
        super::reload_icons(&mut Ffi)
    }
}

#[cfg(test)]
#[path = "status_box_calls_tests.rs"]
mod tests;
