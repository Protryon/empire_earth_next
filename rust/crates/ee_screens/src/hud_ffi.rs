//! `extern "C"` for the status box's attribute texts (`hud.rs`; `User Interface Thread/EEUIFMGStatusBox.cpp`):
//! plain values in and out, no host calls.
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_long;

use crate::hud;

/// `GetAttributeText`: the base and the bonus "base+bonus" shows.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_hud_attribute_text(
    base: f32,
    bonus: f32,
    out_base: *mut c_long,
    out_bonus: *mut c_long,
) {
    let (a, b) = hud::attribute_text(base, bonus);
    out_base.write(a);
    out_bonus.write(b);
}

/// The armor kind armor attribute `slot` (0..5: `mAttributes[4 + slot]`) shows; 0 past them.
#[no_mangle]
pub extern "C" fn ee_screens_hud_armor_kind(slot: u32) -> u32 {
    hud::ARMOR_KINDS.get(slot as usize).copied().unwrap_or(0)
}

/// One armor attribute's `GetAttributeText` arguments from the stats' armor, the unit class's base and the stats'
/// own class's base.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_hud_armor_attribute(
    stat: f32,
    class_base: c_long,
    stats_class_base: c_long,
    out_base: *mut f32,
    out_bonus: *mut f32,
) {
    let (a, b) = hud::armor_attribute(stat, class_base, stats_class_base);
    out_base.write(a);
    out_bonus.write(b);
}
