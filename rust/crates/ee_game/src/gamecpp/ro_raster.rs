//! The rasterizer table the Rust draws call and the units' opacity host in Rust (game core lane, round 11; were
//! EERenderObject.cpp's `gEERustRaster` and its lambdas, EERasterCreateModel, EERasterCreateSubModel,
//! EERasterStoreMesh, EEEffect.cpp's batch draw, EEROUnits.cpp's bar draw, and `EEGetUnitOpacity` with its
//! `EEOpacityHost`). Each C++ call is a case of EERenderObject.cpp's `EECore9OpA` (the bar's EEROUnits.cpp's, the
//! batch's EEEffectHost.cpp's); the C++ that hands the table to ee_gfx asks [`ee_gamecpp_ro_raster`]. Only a game with
//! a window draws (the tour).

use std::ffi::{c_long, c_ulong, c_void};

use ee_base::math::types::{Ge3DPoint, GeRectangle, GeTransformation};
use ee_render::ge::viewport::GeViewport;
use ee_render::gfx::effects::{EeGfxColor, EeGfxEffectQuad};
use ee_render::gfx::fill::{ee_gfx_fill_unit_opacity, EeGfxOpacityHost};
use ee_render::gfx::lighting::EeGfxLighting;
use ee_render::gfx::mesh::{EeGfxMeshTriangle, EeGfxMeshVertex};
use ee_render::gfx::raster::{EeGfxModel, EeGfxRaster, EeGfxScreenLine, EeGfxTextureLoad};

use super::effect_host::ee_gamecpp_fx_batch_quad;
use super::render_object::{
    ee_gamecpp_ro_depth_range, ee_gamecpp_ro_draw_text, ee_gamecpp_ro_fog_factor, ee_gamecpp_ro_lighting,
    ee_gamecpp_ro_load_texture, ee_gamecpp_ro_rear_clipping, ee_gamecpp_ro_screen_line, ee_gamecpp_ro_screen_point,
};
use super::ro_units::{ee_gamecpp_ru_bar_set, ee_gamecpp_ru_label, ee_gamecpp_ru_screen_lines};
use super::{h, hd, hf, hp, pa, ptr, O, P};

extern "C" fn lighting(kind: EeGfxLighting) {
    ee_gamecpp_ro_lighting(kind as u32);
}
extern "C" fn fog_factor(location: *const Ge3DPoint, fog: u32) -> f32 {
    // SAFETY: the caller's point (three floats)
    unsafe { ee_gamecpp_ro_fog_factor(location as *const f32, fog) }
}
extern "C" fn model_draw(model: EeGfxModel, opacity: f32) -> i32 {
    hf(O::RrModelDraw, model as P, &[], &[opacity]) as i32
}
extern "C" fn camera() -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    h(O::RrCamera, 0, &[ptr(&mut t)]);
    t
}
extern "C" fn camera_yaw() -> f32 {
    hd(O::RrCameraYaw, 0, &[], &[])
}
extern "C" fn screen_point(point: *const Ge3DPoint, x: *mut f32, y: *mut f32) {
    // SAFETY: the caller's point and outputs
    unsafe { ee_gamecpp_ro_screen_point(point as *const f32, x, y) }
}
extern "C" fn draw_text(x: f32, y: f32, text: *const u8, color: EeGfxColor) {
    // SAFETY: the caller's NUL-terminated text
    unsafe { ee_gamecpp_ro_draw_text(x, y, text, color.red, color.green, color.blue) }
}
extern "C" fn create_model(polygons: u32, vertices: u32, sub_models: u32, name: *const u8, flags: u32) -> EeGfxModel {
    let a = [polygons as i64, vertices as i64, sub_models as i64, name as usize as i64, flags as i64];
    hp(O::RrCreateModel, 0, &a) as EeGfxModel
}
extern "C" fn create_sub_model(polygons: u32, vertices: u32, parent: EeGfxModel) -> EeGfxModel {
    hp(O::RrCreateSubModel, 0, &[polygons as i64, vertices as i64, pa(parent as P)]) as EeGfxModel
}
extern "C" fn store_mesh(
    model: EeGfxModel,
    vertices: *const EeGfxMeshVertex,
    vertex_count: usize,
    triangles: *const EeGfxMeshTriangle,
    triangle_count: usize,
) {
    let a = [vertices as usize as i64, vertex_count as i64, triangles as usize as i64, triangle_count as i64];
    h(O::FxStoreMesh, model as P, &a);
}
extern "C" fn load_texture(name: *const u8, load: *const EeGfxTextureLoad) -> EeGfxModel {
    // SAFETY: the caller's name and load
    unsafe { ee_gamecpp_ro_load_texture(name, load as *const c_void) }
}
extern "C" fn draw_sky(model: EeGfxModel, sorted: bool) -> i32 {
    h(O::RrDrawSky, model as P, &[sorted as i64]) as i32
}
extern "C" fn texture_32bit() -> bool {
    h(O::RrTexture32, 0, &[]) != 0
}
extern "C" fn bar_set(which: u32, rect: *const GeRectangle, colors: *const EeGfxColor) {
    ee_gamecpp_ru_bar_set(which, rect, colors);
}
extern "C" fn bar_draw(which: u32) -> i32 {
    h(O::RrBarDraw, 0, &[which as i64]) as i32
}
extern "C" fn screen_lines(lines: *const EeGfxScreenLine, count: u32, start_u: f32, end_u: f32) -> i32 {
    // SAFETY: the caller's lines
    unsafe { ee_gamecpp_ru_screen_lines(lines, count, start_u, end_u) }
}
#[allow(clippy::too_many_arguments)]
extern "C" fn label(
    text: *const u8,
    x: f32,
    y: f32,
    color: EeGfxColor,
    font: u32,
    view: *const GeRectangle,
    strip: *const GeRectangle,
    bottom: f32,
) {
    // SAFETY: the caller's text and rectangles
    unsafe { ee_gamecpp_ru_label(text, x, y, color, font, view, strip, bottom) }
}
extern "C" fn camera_inverse() -> GeTransformation {
    // SAFETY: the main viewport's camera inverse
    unsafe { *(hp(O::PkMainCameraInverse, 0, &[]) as *const GeTransformation) }
}
extern "C" fn camera_pitch() -> f32 {
    hd(O::RrCameraPitch, 0, &[], &[])
}
extern "C" fn batch_quad(which: u32, quad: *const EeGfxEffectQuad, index: u32) {
    ee_gamecpp_fx_batch_quad(which, quad, index);
}
extern "C" fn batch_draw(which: u32, count: u32, texture: EeGfxModel) -> i32 {
    h(O::RrBatchDraw, texture as P, &[which as i64, count as i64]) as i32
}
extern "C" fn screen() -> u32 {
    h(O::RrScreen, 0, &[]) as u32
}
extern "C" fn rasterizer(base: *mut *mut c_void) -> *mut c_void {
    hp(O::RrObjects, 0, &[base as usize as i64]) as *mut c_void
}

/// The rasterizer as the Rust draws call it, on the current screen (was `gEERustRaster`).
static RASTER: EeGfxRaster = EeGfxRaster {
    lighting,
    fog_factor,
    model_draw,
    camera,
    depth_range: ee_gamecpp_ro_depth_range,
    camera_yaw,
    screen_point,
    draw_text,
    create_model,
    create_sub_model,
    store_mesh,
    load_texture,
    screen_line: ee_gamecpp_ro_screen_line,
    rear_clipping: ee_gamecpp_ro_rear_clipping,
    draw_sky,
    texture_32bit,
    bar_set,
    bar_draw,
    screen_lines,
    label,
    camera_inverse,
    camera_pitch,
    batch_quad,
    batch_draw,
    screen,
    rasterizer,
};

/// The raster table (the C++ hands it to ee_gfx's plane models and ground shadows).
#[no_mangle]
pub extern "C" fn ee_gamecpp_ro_raster() -> *const EeGfxRaster {
    &RASTER
}

// ---- the units' opacity (ee_gfx fill.rs EeGfxOpacityHost; was EERenderObject.cpp's EEOpacity*)

extern "C" fn opacity_own(_: *mut c_void, complex: u64, translucent: bool, opacity: *mut f32) -> bool {
    h(O::RrOpacityOwn, complex as P, &[translucent as i64, opacity as usize as i64]) != 0
}
extern "C" fn opacity_land_building(_: *mut c_void, class: u64) -> bool {
    h(O::UcIsLandBuilding, class as P, &[]) != 0
}
extern "C" fn opacity_model_center(_: *mut c_void, unit: u64, index: c_ulong) -> Ge3DPoint {
    let mut c = Ge3DPoint::default();
    h(O::RrModelCenter, unit as P, &[index as i64, ptr(&mut c)]);
    c
}
extern "C" fn opacity_model_radius(_: *mut c_void, unit: u64, index: c_ulong, frame: c_ulong) -> f32 {
    hd(O::RrModelRadius, unit as P, &[index as i64, frame as i64], &[])
}
extern "C" fn opacity_physical_scale(_: *mut c_void, complex: u64) -> f32 {
    hd(O::RrPhysicalScale, complex as P, &[], &[])
}
extern "C" fn opacity_physical_frame(_: *mut c_void, complex: u64) -> c_ulong {
    h(O::RrPhysicalFrame, complex as P, &[]) as c_ulong
}
extern "C" fn opacity_mesh_z(_: *mut c_void, x: f32, y: f32) -> f32 {
    hd(O::MpMeshZ, hp(O::MpWorldMap, 0, &[]), &[], &[x, y])
}

/// EERenderObject::GetUnitOpacity: a unit's opacity this frame (see-through modes, its own fade, buildings fading in
/// front of the near clipping plane); whether it is drawn translucent.
pub(super) fn unit_opacity(unit: P, show_hidden: c_long, opacity: *mut f32) -> bool {
    let host = EeGfxOpacityHost {
        context: std::ptr::null_mut(),
        own: opacity_own,
        land_building: opacity_land_building,
        model_center: opacity_model_center,
        model_radius: opacity_model_radius,
        physical_scale: opacity_physical_scale,
        physical_frame: opacity_physical_frame,
        mesh_z: opacity_mesh_z,
    };
    let viewport = hp(O::FwViewport, 0, &[]) as *const GeViewport;
    let cinematic = h(O::RrCinematic, 0, &[]) != 0;
    let orthographic = h(O::MpOrthographic, 0, &[]) != 0;
    // SAFETY: the main viewport, the host for the call, the caller's opacity
    unsafe { ee_gfx_fill_unit_opacity(unit as u64, show_hidden, cinematic, orthographic, viewport, &host, opacity) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_table_is_one_static() {
        assert_eq!(ee_gamecpp_ro_raster(), &RASTER as *const EeGfxRaster);
    }
}
