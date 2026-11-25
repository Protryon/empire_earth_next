//! The units' render object's own bodies (was EEROUnits.cpp): the media's load and release (the bar rhombi, the line
//! batch, then ee_gfx ro_units.rs' textures and models), the frame's draws over the render lists' view (the models
//! and rings, then the labels), and the raster table's bar, screen-line and label entries. Each C++ call or member is
//! a case of EEROUnits.cpp's `EERoUnitsOp`; the passes themselves are ee_gfx's (units_lists.rs, ro_units.rs).
//!
//! Only a game with a window runs these (the tour, gates.sh): EE-SIM draws nothing.

use std::ffi::{c_void, CStr};
use std::sync::atomic::{AtomicBool, Ordering};

use ee_base::math::types::GeRectangle;
use ee_render::gfx::effect_obj::ee_gfx_effect_queues_draw;
use ee_render::gfx::effects::EeGfxColor;
use ee_render::gfx::ffi::ee_gfx_units_label_targets;
use ee_render::gfx::raster::{EeGfxRaster, EeGfxScreenLine};
use ee_render::gfx::rfil::EeGfxRfilFrame;
use ee_render::gfx::ro_units::{
    ee_gfx_ro_units_line_texture, ee_gfx_ro_units_load, ee_gfx_ro_units_release, EeGfxUnitsScene,
};
use ee_render::gfx::units_lists::ee_gfx_ro_units_draw_lists;

use super::{cptr, h, hd, hf, hp, pa, Owned, O, P};

/// The line batch's size (`new GELine[0x40]`).
const RU_LINES: i64 = 0x40;

/// `gUnitPanelActive` (0x5297a0): the current player has a selection or a viewed unit (EEROUserInterface's
/// FrameStarting sets it; the units' scene and the UI's z-buffer clears read it).
pub static RU_UNIT_PANEL_ACTIVE: AtomicBool = AtomicBool::new(false);

/// The rasterizer as the Rust draws call it (`gEERustRaster`, EERenderObject.cpp).
pub fn raster() -> *const EeGfxRaster {
    super::ro_raster::ee_gamecpp_ro_raster()
}

/// The render lists as ee_gfx reads them in place (ro_lists.rs' view, valid until the next fill).
pub fn lists_view() -> EeGfxRfilFrame {
    // SAFETY: a plain record of pointers and counts (C++'s `= {}`)
    let mut frame: EeGfxRfilFrame = unsafe { std::mem::zeroed() };
    super::ro_lists::view(&mut frame);
    frame
}

/// The raster table's `bar_set`: the bar rhombus `which` (0 the fill, 1 its back) given a rectangle and four
/// vertex colours, each kept when null.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ru_bar_set(which: u32, rect: *const GeRectangle, colors: *const EeGfxColor) {
    if !rect.is_null() {
        h(O::RuBarSetRect, 0, &[which as i64, pa(rect as P)]);
    }
    if !colors.is_null() {
        h(O::RuBarSetColors, 0, &[which as i64, pa(colors as P)]);
    }
}

/// The raster table's `screen_lines`: `count` lines of the batch in the line texture from u `start_u` to `end_u`,
/// drawn as one batch.
///
/// # Safety
/// `lines` holds `count` (at most 64) lines.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_ru_screen_lines(
    lines: *const EeGfxScreenLine,
    count: u32,
    start_u: f32,
    end_u: f32,
) -> i32 {
    for i in 0..count as usize {
        // SAFETY: the function's contract
        let line = unsafe { &*lines.add(i) };
        hf(O::RuLineCoordinates, 0, &[i as i64], &[start_u, end_u]);
        h(O::RuLineColor, 0, &[i as i64, cptr(&line.color)]);
        h(O::RuLineStart, 0, &[i as i64, cptr(&line.start)]);
        h(O::RuLineEnd, 0, &[i as i64, cptr(&line.end)]);
    }
    h(O::RuLinesDraw, 0, &[count as i64]) as i32
}

/// The raster table's `label`: a transparent centred text in the labels' font (`font` 0 the default, 1 Tahoma 12,
/// 2 Tahoma 16) and `color` at (`x`, `y`), drawn clipped by the targets ee_gfx gives for its height: the main game
/// form's `view` and `strip`, or the scenario editor's `view`.
///
/// # Safety
/// `text` is NUL-terminated; `view` and `strip` are valid.
#[allow(clippy::too_many_arguments)]
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_ru_label(
    text: *const u8,
    x: f32,
    y: f32,
    color: EeGfxColor,
    font: u32,
    view: *const GeRectangle,
    strip: *const GeRectangle,
    bottom: f32,
) {
    let label = Owned::new(O::RuTextNew, O::RoTextDelete);
    hf(O::RoTextSetColor, label.0, &[], &[color.red, color.green, color.blue]);
    h(O::RoTextSetTransparent, label.0, &[]);
    h(O::RoTextSetCentred, label.0, &[]);
    if font == 2 {
        h(O::RuTextFont16, label.0, &[]);
    } else if font == 1 {
        h(O::RuTextFont12, label.0, &[]);
    }
    // SAFETY: the function's contract
    let text = unsafe { CStr::from_ptr(text as *const std::ffi::c_char) };
    h(O::RoTextSetText, label.0, &[text.as_ptr() as usize as i64]);
    hf(O::RoTextSetLocation, label.0, &[], &[x, y]);
    let free_camera = crate::gamecpp::globals::get!(gFreeCamera);
    let height = if free_camera { 0.0 } else { hd(O::RuTextHeight, label.0, &[], &[]) };
    let targets = ee_gfx_units_label_targets(y, height, bottom, free_camera);
    if targets & 1 != 0 {
        h(O::RoMainGameDrawText, label.0, &[pa(view as P)]);
    }
    if targets & 2 != 0 {
        h(O::RoMainGameDrawText, label.0, &[pa(strip as P)]);
    }
    if targets & 4 != 0 {
        h(O::RoEditorDrawText, label.0, &[pa(view as P)]);
    }
}

/// The units' pass of the effects: queue 3 (`DrawPass5`), batched per texture.
extern "C" fn ru_units_effects(_: *mut c_void) {
    // SAFETY: the graphics thread's frame
    unsafe { ee_gfx_effect_queues_draw(5) };
}

/// The frame's render lists drawn by ee_gfx (the models and rings, or the labels) with the scene's settings.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ru_draw(labels: bool) {
    let frame = lists_view();
    // SAFETY: the address of gU4E9330 (a float)
    let grid_phase = super::globals::get!(gU4E9330);
    let scene = EeGfxUnitsScene {
        context: std::ptr::null_mut(),
        viewport: hp(O::RuMainViewport, 0, &[]) as *const _,
        orthographic: h(O::RoOrthographic, 0, &[]) != 0,
        cinematic: super::ro_views::snapshot().in_cinematic,
        collision_spheres: h(O::RuCollisionSpheres, 0, &[]) != 0,
        interpolate_frames: h(O::RuInterpolateFrames, 0, &[]) != 0,
        model_detail: h(O::RuModelDetail, 0, &[]) as i32,
        free_camera: crate::gamecpp::globals::get!(gFreeCamera),
        panel_active: RU_UNIT_PANEL_ACTIVE.load(Ordering::Relaxed),
        grid_phase,
        camera_yaw: super::render_object::camera_yaw(),
        window_width: h(O::RuWindowWidth, 0, &[]) as u32,
        window_height: h(O::RuWindowHeight, 0, &[]) as u32,
        units: std::ptr::null(),
        unit_count: 0,
        objects: std::ptr::null(),
        object_count: 0,
        units0: std::ptr::null(),
        units0_count: 0,
        units1: std::ptr::null(),
        units1_count: 0,
        selected_units: std::ptr::null(),
        selected_unit_count: 0,
        objects0: std::ptr::null(),
        objects0_count: 0,
        objects1: std::ptr::null(),
        objects1_count: 0,
        selected_objects: std::ptr::null(),
        selected_object_count: 0,
        selected_unit: 0,
        selected_object: 0,
        ghosts: std::ptr::null(),
        ghost_count: 0,
        debug_unit: 0,
        debug_text: std::ptr::null(),
        draw_effects: ru_units_effects,
    };
    // SAFETY: the raster table, the view and the scene live through the call
    unsafe { ee_gfx_ro_units_draw_lists(raster(), &frame, &scene, labels) };
}

/// EEROUnits::FrameDone: the unit labels (not in cinematics), then the effects' pass 3.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ru_frame_done() {
    if !super::ro_views::snapshot().in_cinematic {
        ee_gamecpp_ru_draw(true);
    }
    // SAFETY: the graphics thread's frame
    unsafe { ee_gfx_effect_queues_draw(3) };
}

/// EEROUnits::LoadMedia: the bar rhombi (the back first), ee_gfx's textures and models (the ring segments by the
/// model detail), and the line batch: green, in the line texture, u from 0 to 3.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ru_load_media() {
    h(O::RuBarNew, 0, &[1]);
    h(O::RuBarNew, 0, &[0]);
    h(O::Verify, 0, &[h(O::RuBarPresent, 0, &[1]), 1823]);
    h(O::Verify, 0, &[h(O::RuBarPresent, 0, &[0]), 1824]);
    // SAFETY: the raster table
    unsafe { ee_gfx_ro_units_load(raster(), h(O::RuModelDetail, 0, &[]) as i32) };
    h(O::RuLinesNew, 0, &[RU_LINES]);
    for i in 0..RU_LINES {
        h(O::RuLineInitColor, 0, &[i]);
        h(O::RuLineInitTexture, 0, &[i, ee_gfx_ro_units_line_texture() as usize as i64]);
        h(O::RuLineInitCoordinates, 0, &[i]);
    }
}

/// EEROUnits::ReleaseMedia: the bar rhombi, the line batch, then ee_gfx's textures, ring and selection models.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ru_release_media() {
    h(O::RuBarDelete, 0, &[1]);
    h(O::RuBarDelete, 0, &[0]);
    h(O::RuLinesDelete, 0, &[]);
    // SAFETY: the raster table
    unsafe { ee_gfx_ro_units_release(raster()) };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_bar_set_keeps_what_is_null() {
        // nothing to set: no C++ call (a host call in a test panics)
        ee_gamecpp_ru_bar_set(0, std::ptr::null(), std::ptr::null());
        assert_eq!(RU_LINES, 64);
        assert!(!RU_UNIT_PANEL_ACTIVE.load(Ordering::Relaxed));
    }

    #[test]
    #[should_panic(expected = "no C++")]
    fn the_lists_view_asks_the_cpp() {
        // (the extern "C" entries abort on a panic: the view is tested through its plain caller)
        lists_view();
    }
}
