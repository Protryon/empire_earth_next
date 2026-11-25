//! The rasterizer's render states set by Rust directly (game core lane, round 13): the Graphics Thread's host
//! cases that only called a `GERasterizer` member forwarding to the rasterizer's Rust objects (lle/GERasterizer.h:
//! `SetFog`, `SetLighting`, `SetZBuffer`, ..., `ClearZBuffer`, `Clear`, `GetFog`) are calls of those objects'
//! functions here. [`rast`] asks the host once for the current rasterizer's two objects and the screen
//! (`gRasterizer`, `gScreenIndex`); each method is the member's one statement. Only a game with a window draws (the
//! tour). The textures' stage setters and `Reference` likewise (`tex_*`), and the
//! terrain mesh's reads (`mesh_*`).

use std::ffi::{c_long, c_ulong, c_void};

use ee_base::math::types::GeRectangle;
use ee_render::dx7::draw_ffi::{
    ee_dx7r_draw_grid_lines, ee_dx7r_draw_terrain, ee_dx7r_draw_terrain_grid, ee_dx7r_draw_terrain_highlight,
    ee_dx7r_draw_water, ee_dx7r_light_terrain, ee_dx7r_transform_to_camera,
};
use ee_render::dx7::rastobj::{
    ee_dx7r_card_flag, ee_dx7r_clear, ee_dx7r_remove_all_lights, ee_dx7r_set, ee_dx7r_set_blending, ee_dx7r_set_color,
    ee_dx7r_set_culling, ee_dx7r_set_float, ee_dx7r_set_fog_range, ee_dx7r_set_viewport, EeDx7RastColor, EeDx7RastSet,
    EeDx7Rasterizer,
};
use ee_render::ge::cppobj;
use ee_render::ge::rastobj::{
    ee_ge_rast_screen, ee_ge_rast_screen_set, EeGeRastColor, EeGeRastScreen, EeGeRastState, EeGeRasterizer,
};
pub(crate) use ee_render::ge::texobj_tex::EeGeTexStageField as TexStage;
use ee_render::ge::texobj_tex::{ee_ge_tex_set_stage, ee_ge_tex_set_stage_bias, EeGeTexture};
use ee_render::ge::tmobj::{ee_ge_tm_get, ee_ge_tm_mesh_z, ee_ge_tm_visible_row_parts, EeGeTerrainMesh, EeGeTmVal};
use ee_render::gestate::{GEBlendFactor, GECompareFunction, GECullMode, GEZBufferMode};
use ee_render::geval::viewport::GeViewport;

use super::{h, hp, O};

/// `kZBufferDisabled`.
pub(crate) const Z_BUFFER_DISABLED: u32 = GEZBufferMode::kZBufferDisabled as u32;
/// `kZBufferTestOnly`.
pub(crate) const Z_BUFFER_TEST_ONLY: u32 = GEZBufferMode::kZBufferTestOnly as u32;
/// `kCompareLessEqual`.
pub(crate) const COMPARE_LESS_EQUAL: u32 = GECompareFunction::kCompareLessEqual as u32;
/// `kCullNone`.
pub(crate) const CULL_NONE: u32 = GECullMode::kCullNone as u32;
/// `kBlendSrcAlpha`.
pub(crate) const BLEND_SRC_ALPHA: u32 = GEBlendFactor::kBlendSrcAlpha as u32;
/// `kBlendInvSrcAlpha`.
pub(crate) const BLEND_INV_SRC_ALPHA: u32 = GEBlendFactor::kBlendInvSrcAlpha as u32;

/// The current rasterizer's Rust objects (`mDX7`, `mRust`) and the current screen.
#[derive(Clone, Copy)]
pub(crate) struct Rast {
    dx7: *mut EeDx7Rasterizer,
    ge: *mut EeGeRasterizer,
    screen: c_ulong,
}

/// `gRasterizer` on `gScreenIndex`.
pub(crate) fn rast() -> Rast {
    let mut ge: *mut c_void = std::ptr::null_mut();
    let dx7 = hp(O::RrObjects, 0, &[&mut ge as *mut *mut c_void as usize as i64]);
    Rast { dx7: dx7 as *mut EeDx7Rasterizer, ge: ge as *mut EeGeRasterizer, screen: h(O::RrScreen, 0, &[]) as c_ulong }
}

impl Rast {
    fn screen_state(self) -> &'static EeGeRastScreen {
        // SAFETY: the live rasterizer's screen record (read before the next state call)
        unsafe { &*ee_ge_rast_screen(self.ge, self.screen) }
    }
    fn set(self, which: EeDx7RastSet, value: u32) {
        // SAFETY: the live rasterizer, on the graphics thread
        unsafe { ee_dx7r_set(self.dx7, which, value as c_ulong, self.screen) }
    }
    fn set_color(self, which: EeDx7RastColor, rgb: &[f32]) {
        let color = EeGeRastColor { red: rgb[0], green: rgb[1], blue: rgb[2] };
        // SAFETY: the live rasterizer; the colour outlives the call
        unsafe { ee_dx7r_set_color(self.dx7, which, &color, self.screen) }
    }
    /// `SetFog`.
    pub(crate) fn set_fog(self, state: u32) {
        self.set(EeDx7RastSet::Fog, state)
    }
    /// `GetFog`.
    pub(crate) fn fog(self) -> u32 {
        self.screen_state().fog_state as u32
    }
    /// `SetFogColor(GEColor(rgb))`.
    pub(crate) fn set_fog_color(self, rgb: &[f32]) {
        self.set_color(EeDx7RastColor::Fog, rgb)
    }
    /// `SetFogRange`.
    pub(crate) fn set_fog_range(self, start: f32, end: f32) {
        // SAFETY: the live rasterizer
        unsafe { ee_dx7r_set_fog_range(self.dx7, start, end, self.screen) }
    }
    /// `SetFogType` (only recorded in the screen's state).
    pub(crate) fn set_fog_type(self, kind: u32) {
        // SAFETY: the live rasterizer
        unsafe { ee_ge_rast_screen_set(self.ge, self.screen, EeGeRastState::FogType, kind as isize) }
    }
    /// `SetProjectionMode` (only recorded in the screen's state).
    pub(crate) fn set_projection_mode(self, mode: u32) {
        // SAFETY: the live rasterizer
        unsafe { ee_ge_rast_screen_set(self.ge, self.screen, EeGeRastState::ProjectionMode, mode as isize) }
    }
    /// `SetAmbientColor(GEColor(rgb))`.
    pub(crate) fn set_ambient_color(self, rgb: &[f32]) {
        self.set_color(EeDx7RastColor::Ambient, rgb)
    }
    /// `SetAmbientIntensity`.
    pub(crate) fn set_ambient_intensity(self, intensity: f32) {
        // SAFETY: the live rasterizer
        unsafe { ee_dx7r_set_float(self.dx7, true, intensity, self.screen) }
    }
    /// `SetAlphaTestingRefValue`.
    pub(crate) fn set_alpha_ref(self, value: f32) {
        // SAFETY: the live rasterizer
        unsafe { ee_dx7r_set_float(self.dx7, false, value, self.screen) }
    }
    /// `GetAlphaTestingRefValue`.
    pub(crate) fn alpha_ref(self) -> f32 {
        self.screen_state().alpha_testing_ref_value
    }
    /// `RemoveAllLights`.
    pub(crate) fn remove_all_lights(self) {
        // SAFETY: the live rasterizer
        unsafe { ee_dx7r_remove_all_lights(self.dx7, self.screen) };
    }
    /// `SetLighting`.
    pub(crate) fn set_lighting(self, state: u32) {
        self.set(EeDx7RastSet::Lighting, state)
    }
    /// `SetShadingColorSource`.
    pub(crate) fn set_shading_color_source(self, source: u32) {
        self.set(EeDx7RastSet::ShadingColorSource, source)
    }
    /// `SetZBuffer`.
    pub(crate) fn set_z_buffer(self, mode: u32) {
        self.set(EeDx7RastSet::ZBuffer, mode)
    }
    /// `SetZCompare`.
    pub(crate) fn set_z_compare(self, compare: u32) {
        self.set(EeDx7RastSet::ZCompare, compare)
    }
    /// `SetCulling` (every screen's).
    pub(crate) fn set_culling(self, mode: u32) {
        // SAFETY: the live rasterizer
        unsafe { ee_dx7r_set_culling(self.dx7, mode as c_ulong) }
    }
    /// `SetBlending`.
    pub(crate) fn set_blending(self, source: u32, destination: u32) {
        // SAFETY: the live rasterizer
        unsafe { ee_dx7r_set_blending(self.dx7, source as c_ulong, destination as c_ulong, self.screen) }
    }
    /// `ClearZBuffer(z, rectangle)`.
    pub(crate) fn clear_z(self, z: f32, rectangle: &GeRectangle) {
        let r = rectangle as *const GeRectangle as *const [f32; 4];
        // SAFETY: the live rasterizer; a GERectangle is four floats
        unsafe { ee_dx7r_clear(self.dx7, r, std::ptr::null(), z, self.screen) };
    }
    /// `Clear(GEColor(0, 0, 0), rectangle)`.
    pub(crate) fn clear_black(self, rectangle: &GeRectangle) {
        let black = EeGeRastColor { red: 0.0, green: 0.0, blue: 0.0 };
        let r = rectangle as *const GeRectangle as *const [f32; 4];
        // SAFETY: the live rasterizer; a GERectangle is four floats
        unsafe { ee_dx7r_clear(self.dx7, r, &black, 0.0, self.screen) };
    }
    /// `GetDirtyRectangleSupport`.
    pub(crate) fn dirty_rectangle_support(self) -> bool {
        // SAFETY: the live rasterizer
        unsafe { ee_dx7r_card_flag(self.dx7, self.screen, true) }
    }
    /// `Light(mesh)`: the visible terrain's vertices lit.
    pub(crate) fn light_terrain(self, mesh: *const EeGeTerrainMesh) {
        // SAFETY: the live rasterizer and a live map's mesh
        unsafe { ee_dx7r_light_terrain(self.dx7, self.screen, mesh.cast_mut()) }
    }
    /// `Draw(mesh, orthographic)`: the visible terrain.
    pub(crate) fn draw_terrain(self, mesh: *const EeGeTerrainMesh, orthographic: bool) {
        // SAFETY: the live rasterizer and a live map's mesh
        unsafe { ee_dx7r_draw_terrain(self.dx7, self.screen, mesh.cast_mut(), orthographic) };
    }
    /// `DrawWater(mesh, orthographic)`.
    pub(crate) fn draw_water(self, mesh: *const EeGeTerrainMesh, orthographic: bool) {
        // SAFETY: the live rasterizer and a live map's mesh
        unsafe { ee_dx7r_draw_water(self.dx7, self.screen, mesh.cast_mut(), orthographic) };
    }
    /// `DrawTerrainGrid(mesh, orthographic)`.
    pub(crate) fn draw_terrain_grid(self, mesh: *const EeGeTerrainMesh, orthographic: bool) {
        // SAFETY: the live rasterizer and a live map's mesh
        unsafe { ee_dx7r_draw_terrain_grid(self.dx7, self.screen, mesh.cast_mut(), orthographic) };
    }
    /// `DrawTerrainHighlight(mesh, rectangle, orthographic, colour, opacity)`: `rectangle` is a `U2DRectangle<long>`'s
    /// members (bottom, left, right, top).
    pub(crate) fn draw_terrain_highlight(
        self,
        mesh: *const EeGeTerrainMesh,
        rectangle: &[c_long; 4],
        color: &[f32; 3],
        opacity: f32,
        orthographic: bool,
    ) {
        // SAFETY: the live rasterizer, a live map's mesh; the rectangle and colour outlive the call
        unsafe {
            ee_dx7r_draw_terrain_highlight(
                self.dx7,
                self.screen,
                mesh.cast_mut(),
                rectangle.as_ptr(),
                color.as_ptr(),
                opacity,
                orthographic,
            )
        };
    }
    /// `DrawGridLines(mesh, runs, texture, width, orthographic)`: each run is four longs.
    pub(crate) fn draw_grid_lines(
        self,
        mesh: *const EeGeTerrainMesh,
        runs: &[[c_long; 4]],
        texture: usize,
        width: f32,
        orthographic: bool,
    ) {
        // SAFETY: the live rasterizer, a live map's mesh; the runs outlive the call
        unsafe {
            ee_dx7r_draw_grid_lines(
                self.dx7,
                self.screen,
                mesh.cast_mut(),
                runs.as_ptr(),
                runs.len(),
                texture as *mut c_void,
                width,
                orthographic,
            )
        };
    }
    /// `TransformToCamera(mesh, viewport, scale, water_only, orthographic)`.
    pub(crate) fn transform_to_camera(
        self,
        mesh: *const EeGeTerrainMesh,
        viewport: *const GeViewport,
        scale: f32,
        water_only: bool,
        orthographic: bool,
    ) {
        // SAFETY: the live rasterizer, a live map's mesh and viewport
        unsafe {
            ee_dx7r_transform_to_camera(self.dx7, mesh.cast_mut(), viewport.cast(), scale, water_only, orthographic)
        }
    }
    /// `SetViewport(viewport)`.
    pub(crate) fn set_viewport(self, viewport: *const GeViewport) {
        // SAFETY: the live rasterizer and viewport
        unsafe { ee_dx7r_set_viewport(self.dx7, viewport.cast(), self.screen) }
    }
    /// `&GetViewport()`: the screen's viewport.
    pub(crate) fn viewport(self) -> *mut GeViewport {
        // SAFETY: the live rasterizer's screen record
        unsafe { &mut (*ee_ge_rast_screen(self.ge, self.screen)).viewport }
    }
}

// ---- the viewports (GEViewport's inline members over its Rust record)

/// `&gViewport`: the game's viewport (its storage is Rust's: global_objects.rs).
pub(crate) fn game_viewport() -> *mut GeViewport {
    std::ptr::addr_of_mut!(super::global_objects::gViewport).cast()
}

/// `&gU4E8F08`: the frame's copy of the game's viewport (the units', the picker's and the command units').
pub(crate) fn frame_viewport() -> *mut GeViewport {
    hp(O::RuMainViewport, 0, &[]) as *mut GeViewport
}

/// `viewport.GetViewRectangle()`.
pub(crate) fn view_rectangle(viewport: *const GeViewport) -> GeRectangle {
    // SAFETY: a live viewport
    unsafe { (*viewport).view_rectangle }
}

/// `viewport.GetRearClippingDistance()`.
pub(crate) fn rear_clipping(viewport: *const GeViewport) -> f32 {
    // SAFETY: a live viewport
    unsafe { (*viewport).rear_clipping_distance }
}

// ---- a texture's stage (GETexture's inline setters over its Rust object; the stage argument is unused)

/// `texture->SetStage<Field>Mode(mode, -1)`.
pub(crate) fn tex_stage(texture: usize, field: TexStage, mode: i64) {
    // SAFETY: a live GETexture (its Rust object after the reference count)
    unsafe { ee_ge_tex_set_stage(cppobj::counted::<EeGeTexture>(texture), field, mode as c_ulong) }
}

/// `texture->SetStageMipMapBias(bias, -1)`.
pub(crate) fn tex_mip_bias(texture: usize, bias: f32) {
    // SAFETY: a live GETexture
    unsafe { ee_ge_tex_set_stage_bias(cppobj::counted::<EeGeTexture>(texture), bias) }
}

/// `texture->Reference()`.
pub(crate) fn tex_reference(texture: usize) {
    // SAFETY: a live GETexture
    unsafe { cppobj::reference(texture) }
}

// ---- the terrain mesh (GETerrainMesh's inline members over its Rust object)

/// A map's terrain mesh (`map->mTerrainMesh`'s Rust object).
pub(crate) fn mesh_of(map: usize) -> *const EeGeTerrainMesh {
    hp(O::MpMeshRust, map, &[]) as *const EeGeTerrainMesh
}

/// The world's terrain mesh (`gWorld.mMap->mTerrainMesh`).
pub(crate) fn world_mesh() -> *const EeGeTerrainMesh {
    mesh_of(hp(O::MpWorldMap, 0, &[]))
}

/// `GetMeshZ(x, y)`.
pub(crate) fn mesh_z(mesh: *const EeGeTerrainMesh, x: f32, y: f32) -> f32 {
    // SAFETY: a live map's mesh
    unsafe { ee_ge_tm_mesh_z(mesh, x, y) }
}

/// `GetNumVisibleTileRows()`.
pub(crate) fn mesh_visible_rows(mesh: *const EeGeTerrainMesh) -> usize {
    // SAFETY: a live map's mesh
    unsafe { ee_ge_tm_get(mesh, EeGeTmVal::VisibleTileRows) as usize }
}

/// `GetVisibleTileRow(row)`: its y, first and last x.
pub(crate) fn mesh_visible_row(mesh: *const EeGeTerrainMesh, row: usize) -> [c_long; 3] {
    let mut out: [c_long; 3] = [0; 3];
    let [y, first, last] = &mut out;
    // SAFETY: a live map's mesh; row is below the count
    unsafe { ee_ge_tm_visible_row_parts(mesh, row as c_ulong, y, first, last) };
    out
}

/// `NumSpaceCliffTextures()`.
pub(crate) fn mesh_cliff_textures(mesh: *const EeGeTerrainMesh) -> usize {
    // SAFETY: a live map's mesh
    unsafe { ee_ge_tm_get(mesh, EeGeTmVal::SpaceCliffTextureCount) as usize }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn states_are_the_headers() {
        // include/rust/render/gestate.h
        assert_eq!((Z_BUFFER_DISABLED, Z_BUFFER_TEST_ONLY), (0, 2));
        assert_eq!(COMPARE_LESS_EQUAL, 2);
        assert_eq!(CULL_NONE, 0);
        assert_eq!((BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA), (4, 5));
    }

    #[test]
    fn a_rectangle_is_four_floats() {
        // ee_dx7r_clear reads the GERectangle as float[4]
        assert_eq!(std::mem::size_of::<GeRectangle>(), std::mem::size_of::<[f32; 4]>());
        assert_eq!(std::mem::align_of::<GeRectangle>(), std::mem::align_of::<[f32; 4]>());
    }
}
