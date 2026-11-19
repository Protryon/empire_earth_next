//! The rasterizer as Rust draws call it: one C++ table (`EeGfxRaster`, filled once in `EERenderObject.cpp` over
//! `gRasterizer` and the current screen `gScreenIndex`) of the `GERasterizer` state and draw calls the effects' and
//! render objects' draw passes make, the capture's lighting set-ups and fog factor, and a model's draw by handle
//! (`GEModel*`). Rust code draws through the `Raster` trait: the table in the game, a log in the tests.
//!
//! Drawn only (the renderer's thread).

use std::ffi::c_void;

use ee_base::math::types::{Ge2DPoint, Ge3DPoint, GeRectangle, GeTransformation};

use crate::gfx::effects::{EeGfxColor, EeGfxEffectQuad};
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::mesh::{EeGfxMeshTriangle, EeGfxMeshVertex};
use crate::gfx::raster_direct;

/// `kZBufferTestOnly`, `kZBufferTestAndWrite` (GERenderState.h).
pub(crate) const ZBUFFER_TEST_ONLY: u32 = 2;
pub(crate) const ZBUFFER_TEST_AND_WRITE: u32 = 3;
/// `kBlendOne`, `kBlendSrcAlpha`, `kBlendInvSrcAlpha`.
pub(crate) const BLEND_ZERO: u32 = 0;
pub(crate) const BLEND_ONE: u32 = 1;
pub(crate) const BLEND_SRC_ALPHA: u32 = 4;
pub(crate) const BLEND_INV_SRC_ALPHA: u32 = 5;
/// `kCompareLess`, `kCompareLessEqual` (the z compare).
pub(crate) const COMPARE_LESS: u32 = 1;
pub(crate) const COMPARE_LESS_EQUAL: u32 = 2;
/// `kCullCW` (the culling).
pub(crate) const CULL_CW: u32 = 1;
/// The effect quads of `sprite`: `AddGlowSprite` (the camera batch), `AddGroundSprite`, `AddBlendedSprite`.
pub const EE_GFX_SPRITE_GLOW: u32 = 0;
pub const EE_GFX_SPRITE_GROUND: u32 = 1;
pub const EE_GFX_SPRITE_BLENDED: u32 = 2;

/// A model to draw (`GEModel*`), opaque to Rust.
pub type EeGfxModel = *mut c_void;

/// `kStageOpModulate` (a texture stage's colour and alpha operations).
pub(crate) const STAGE_OP_MODULATE: u32 = 2;
/// `kMipFilterPoint`.
pub(crate) const MIP_FILTER_POINT: u32 = 1;
/// `kAddressingClamp`.
pub(crate) const ADDRESSING_CLAMP: u32 = 0;

/// A texture's load (`GETextureDescription`: not 3D-renderable, no transparency) and the stage settings made on it
/// after (`SetStageAlphaMode`, `SetStageColorMode`, `SetStageMinFilterMode`, `SetStageMagFilterMode`,
/// `SetStageMipFilterMode`, `SetStageMipMapBias`, `SetStageTextureAddressingMode`, each on every stage: -1).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxTextureLoad {
    /// `mPixelDepth` (16 or 32)
    pub pixel_depth: u32,
    /// `mNumMipLevelsToDrop`
    pub mip_levels_to_drop: u32,
    /// `mFrameFormat`
    pub frame_format: u32,
    /// `mSurfaceVolatility`
    pub volatility: u32,
    /// `mHasAlpha`, `mCreateMIPMaps`
    pub has_alpha: bool,
    pub mip_maps: bool,
    pub alpha_mode: u32,
    pub color_mode: u32,
    pub min_filter: u32,
    pub mag_filter: u32,
    pub mip_filter: u32,
    pub mip_bias: f32,
    pub addressing: u32,
    /// the texture is referenced once loaded (`Reference`)
    pub reference: bool,
}

impl EeGfxTextureLoad {
    /// The effects' common load: 16 bits, alpha and MIP maps, modulated, bilinear with point MIP filtering biased by -1,
    /// `addressing` as given.
    pub fn effect(addressing: u32) -> EeGfxTextureLoad {
        EeGfxTextureLoad {
            pixel_depth: 16,
            mip_levels_to_drop: 0,
            frame_format: 0,
            volatility: 1,
            has_alpha: true,
            mip_maps: true,
            alpha_mode: STAGE_OP_MODULATE,
            color_mode: STAGE_OP_MODULATE,
            min_filter: 1,
            mag_filter: 1,
            mip_filter: MIP_FILTER_POINT,
            mip_bias: -1.0,
            addressing,
            reference: false,
        }
    }
}

/// The C++'s rasterizer calls, each on the current screen.
#[repr(C)]
pub struct EeGfxRaster {
    /// one of the capture's lighting set-ups (`SetWhiteLighting` ...)
    pub lighting: extern "C" fn(kind: EeGfxLighting),
    /// `GetFogFactor` (it reads the fog's attributes and turns the fog off when it is on)
    pub fog_factor: extern "C" fn(location: *const Ge3DPoint, fog: u32) -> f32,
    /// `Draw(model, screen, opacity, gOrthographicView)`
    pub model_draw: extern "C" fn(model: EeGfxModel, opacity: f32) -> i32,
    /// the render snapshot's camera transformation (`gRenderSnapshot.mCamera`)
    pub camera: extern "C" fn() -> GeTransformation,
    /// the main viewport's depth range, then the viewport set on the rasterizer (`gU4E8F08`)
    pub depth_range: extern "C" fn(near: f32, far: f32),
    /// the camera's yaw (`gRenderSnapshot.mCameraYaw`)
    pub camera_yaw: extern "C" fn() -> f32,
    /// `GetScreenPoint`: the world point's place on the screen (the camera, then the main viewport)
    pub screen_point: extern "C" fn(point: *const Ge3DPoint, x: *mut f32, y: *mut f32),
    /// a `GEText` in Tahoma 10, transparent and centred at (`x`, `y`) in `color`, its text the NUL-terminated UTF-8
    /// `text`, drawn by `DrawClippedText` clipped to the viewport's view rectangle on the main game form (the scenario
    /// editor's under a free camera)
    pub draw_text: extern "C" fn(x: f32, y: f32, text: *const u8, color: EeGfxColor),
    /// `gModelManager->CreateModel(polygons, vertices, 1, 0, 1, sub_models, name, flags)`, its name NUL-terminated
    /// UTF-8 (not referenced)
    pub create_model:
        extern "C" fn(polygons: u32, vertices: u32, sub_models: u32, name: *const u8, flags: u32) -> EeGfxModel,
    /// `gModelManager->CreateSubModel(polygons, vertices, 1, 0, 1, 0, 0, 0, parent)` (not referenced)
    pub create_sub_model: extern "C" fn(polygons: u32, vertices: u32, parent: EeGfxModel) -> EeGfxModel,
    /// a built mesh's set values into vertex array 0 and polygon array 0 (`EEEffectMesh::Store`)
    pub store_mesh: extern "C" fn(
        model: EeGfxModel,
        vertices: *const EeGfxMeshVertex,
        vertex_count: usize,
        triangles: *const EeGfxMeshTriangle,
        triangle_count: usize,
    ),
    /// `gTextureManager->LoadTexture(name, description)` (the name NUL-terminated UTF-8), then the stage settings and
    /// the reference `load` asks for; the texture (`GETexture*`)
    pub load_texture: extern "C" fn(name: *const u8, load: *const EeGfxTextureLoad) -> EeGfxModel,
    /// a `GELine` from (`x0`, `y0`) to (`x1`, `y1`) on the screen, its opacity `opacity0` to `opacity1`
    /// (`SetOpacityPerVertex`), drawn (`Draw(&line, 1, screen)`); the draw's result
    pub screen_line: extern "C" fn(x0: f32, y0: f32, x1: f32, y1: f32, opacity0: f32, opacity1: f32) -> i32,

    /// the main viewport's rear clipping distance set to `distance` (`gU4E8F08.SetRearClippingDistance`; not set on the
    /// rasterizer); the old one
    pub rear_clipping: extern "C" fn(distance: f32) -> f32,
    /// `DrawSky(model, screen, 1.0f, sorted)`
    pub draw_sky: extern "C" fn(model: EeGfxModel, sorted: bool) -> i32,
    /// the screen's `Get32BitTextureSupport`
    pub texture_32bit: extern "C" fn() -> bool,
    /// the bar rhombus `which` (0 the fill, 1 its back): its rectangle (null: kept) and four vertex colours
    /// (`SetColorPerVertex`; null: kept)
    pub bar_set: extern "C" fn(which: u32, rect: *const GeRectangle, colors: *const EeGfxColor),
    /// the bar rhombus `which` drawn (`Draw(rhombus, screen)`)
    pub bar_draw: extern "C" fn(which: u32) -> i32,
    /// `count` (at most 64) screen lines in the line texture from u `start_u` to `end_u`, drawn as one batch
    pub screen_lines: extern "C" fn(lines: *const EeGfxScreenLine, count: u32, start_u: f32, end_u: f32) -> i32,
    /// a unit's label (`EEROUnits::DrawLabels`, 0x516692): a transparent centred `GEText` in the labels' font (`font`:
    /// 0 the default, 1 Tahoma 12, 2 Tahoma 16) and `color`, its text the NUL-terminated UTF-8 `text`, at (`x`, `y`),
    /// drawn clipped by the targets `label_targets` gives for its height (none under the free camera): the main game
    /// form's `view` and `strip`, or the scenario editor's `view`
    pub label: extern "C" fn(
        text: *const u8,
        x: f32,
        y: f32,
        color: EeGfxColor,
        font: u32,
        view: *const GeRectangle,
        strip: *const GeRectangle,
        bottom: f32,
    ),
    /// the main viewport's camera inverse (`gU4E8F08.GetCameraInverse()`), the render snapshot's camera pitch
    pub camera_inverse: extern "C" fn() -> GeTransformation,
    pub camera_pitch: extern "C" fn() -> f32,
    /// an effect batch's (`effect_batch.rs`: 0 the camera batch, 1 the ground batch) four vertices from `index` on
    /// (`SetVertex(color, point, normal, coordinate, opacity, index)` each)
    pub batch_quad: extern "C" fn(which: u32, quad: *const EeGfxEffectQuad, index: u32),
    /// an effect batch drawn: `count` vertices, `count / 2` polygons of the batches' polygon array, in `texture`
    /// (`Draw(polygons, n, vertices, count, texture, screen, gOrthographicView)`)
    pub batch_draw: extern "C" fn(which: u32, count: u32, texture: EeGfxModel) -> i32,
    /// `gScreenIndex` (the screen the models' and materials' textures are set for: raster_direct.rs)
    pub screen: extern "C" fn() -> u32,
    /// `gRasterizer`'s Rust objects: the DX7 rasterizer's (returned) and its base's (`*base`) (raster_direct.rs makes
    /// the state calls on them)
    pub rasterizer: extern "C" fn(base: *mut *mut c_void) -> *mut c_void,
}

impl EeGfxRaster {
    fn rast(&self) -> raster_direct::Rast {
        let mut ge = std::ptr::null_mut();
        let dx7 = (self.rasterizer)(&mut ge);
        raster_direct::Rast { dx7: dx7 as _, ge: ge as _, screen: (self.screen)() as std::ffi::c_ulong }
    }
}

/// The rasterizer's fog (`GERFogAttributes`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxFogAttributes {
    pub kind: u32,
    pub color: EeGfxColor,
    pub start: f32,
    pub end: f32,
    /// `mScale` (the linear fade a unit of distance)
    pub scale: f32,
}

/// One screen line of a batch (`screen_lines`): its colour and ends.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxScreenLine {
    pub color: EeGfxColor,
    pub start: Ge2DPoint,
    pub end: Ge2DPoint,
}

/// The rasterizer as the draws call it.
pub trait Raster {
    fn set_alpha_ref(&mut self, value: f32);
    fn get_fog(&mut self) -> u32;
    fn set_fog(&mut self, state: u32);
    fn get_ambient(&mut self) -> EeGfxColor;
    fn set_ambient(&mut self, color: EeGfxColor);
    fn set_zbuffer(&mut self, mode: u32);
    fn set_blending(&mut self, source: u32, destination: u32);
    fn set_shading_source(&mut self, source: u32);
    fn set_alpha_testing(&mut self, state: u32);
    fn lighting(&mut self, kind: EeGfxLighting);
    fn fog_factor(&mut self, location: &Ge3DPoint, fog: u32) -> f32;
    fn model_opacity(&mut self, model: EeGfxModel, opacity: f32);
    fn model_transform(&mut self, model: EeGfxModel, transformation: &GeTransformation);
    fn model_scale(&mut self, model: EeGfxModel, scale: f32);
    fn model_prepare(&mut self, model: EeGfxModel);
    fn model_draw(&mut self, model: EeGfxModel, opacity: f32) -> i32;
    fn model_texture(&mut self, model: EeGfxModel, texture: EeGfxModel);
    fn model_scroll_v(&mut self, model: EeGfxModel, delta: f32);
    fn model_restore_v(&mut self, model: EeGfxModel);
    fn model_color(&mut self, model: EeGfxModel, color: EeGfxColor);
    fn model_own_texture(&mut self, model: EeGfxModel, texture: EeGfxModel);
    fn camera(&mut self) -> GeTransformation;
    fn standing_plane(&mut self) -> EeGfxModel;
    fn depth_range(&mut self, near: f32, far: f32);
    fn set_zcompare(&mut self, mode: u32);
    fn model_material_opacity(&mut self, model: EeGfxModel, opacity: f32);
    fn model_transform_scaled(&mut self, model: EeGfxModel, transformation: &GeTransformation, x: f32, y: f32, z: f32);
    fn model_frame(&mut self, model: EeGfxModel, frame: u32);
    fn model_pre_transform(&mut self, model: EeGfxModel, transformation: &GeTransformation);
    fn model_scroll_u(&mut self, model: EeGfxModel, delta: f32);
    fn model_restore_u(&mut self, model: EeGfxModel);
    fn get_culling(&mut self) -> u32;
    fn set_culling(&mut self, mode: u32);
    fn model_shift_v(&mut self, model: EeGfxModel, delta: f32);
    #[allow(clippy::too_many_arguments)]
    fn ground_quad(
        &mut self,
        center: &Ge3DPoint,
        color: EeGfxColor,
        yaw: f32,
        size: f32,
        heights: &[f32; 4],
        brightness: f32,
        alpha_ref: f32,
    );
    #[allow(clippy::too_many_arguments)]
    fn ground_strip(
        &mut self,
        from: &Ge3DPoint,
        to: &Ge3DPoint,
        width: f32,
        color: EeGfxColor,
        offset: (f32, f32),
        size: (f32, f32),
        alpha: f32,
        brightness: f32,
        alpha_ref: f32,
    );
    fn texture_frame(&mut self, texture: EeGfxModel, frame: u32);
    fn camera_yaw(&mut self) -> f32;
    fn get_zbuffer(&mut self) -> u32;
    fn get_zcompare(&mut self) -> u32;
    fn model_tween(&mut self, model: EeGfxModel, next: u32, fraction: f32);
    fn screen_point(&mut self, point: &Ge3DPoint) -> (f32, f32);
    /// `text` without a NUL
    fn draw_text(&mut self, x: f32, y: f32, text: &str, color: EeGfxColor);
    fn model_stretch(&mut self, model: EeGfxModel, length: f32, width: f32, scroll: f32);
    fn model_unstretch(&mut self, model: EeGfxModel);
    fn vertex_count(&mut self, model: EeGfxModel) -> u32;
    fn vertex_point(&mut self, model: EeGfxModel, index: u32) -> Ge3DPoint;
    fn set_vertex_point(&mut self, model: EeGfxModel, index: u32, point: &Ge3DPoint);
    fn model_bounds(&mut self, model: EeGfxModel);
    fn model_normals(&mut self, model: EeGfxModel);
    #[allow(clippy::too_many_arguments)]
    fn sprite(
        &mut self,
        kind: u32,
        position: &Ge3DPoint,
        color: EeGfxColor,
        rotation: f32,
        size: f32,
        alpha: f32,
        brightness: f32,
        alpha_ref: f32,
    );
    fn get_alpha_ref(&mut self) -> f32;
    fn get_blending(&mut self) -> (u32, u32);
    fn create_model(&mut self, polygons: u32, vertices: u32, sub_models: u32, name: &str, flags: u32) -> EeGfxModel;
    fn create_sub_model(&mut self, polygons: u32, vertices: u32, parent: EeGfxModel) -> EeGfxModel;
    fn model_reference(&mut self, model: EeGfxModel);
    fn model_dereference(&mut self, model: EeGfxModel);
    fn sub_model(&mut self, model: EeGfxModel, index: u32) -> EeGfxModel;
    fn store_mesh(&mut self, model: EeGfxModel, vertices: &[EeGfxMeshVertex], triangles: &[EeGfxMeshTriangle]);
    fn material_ranges(&mut self, model: EeGfxModel, polygons: u32, vertices: u32);
    fn bounding_radius(&mut self, model: EeGfxModel) -> f32;
    fn copy_vertices(&mut self, from: EeGfxModel, to: EeGfxModel, normals: bool);
    fn texture_wrap(&mut self, texture: EeGfxModel);
    fn load_texture(&mut self, name: &str, load: &EeGfxTextureLoad) -> EeGfxModel;
    fn screen_line(&mut self, from: (f32, f32), to: (f32, f32), opacity0: f32, opacity1: f32) -> i32;

    fn get_alpha_testing(&mut self) -> u32;
    fn rear_clipping(&mut self, distance: f32) -> f32;
    fn draw_sky(&mut self, model: EeGfxModel, sorted: bool) -> i32;
    fn texture_32bit(&mut self) -> bool;
    fn can_modulate_alpha(&mut self) -> bool;
    fn set_ambient_intensity(&mut self, value: f32);
    fn get_fog_attributes(&mut self) -> EeGfxFogAttributes;
    fn set_fog_type(&mut self, kind: u32);
    fn set_fog_color(&mut self, color: EeGfxColor);
    fn set_fog_range(&mut self, start: f32, end: f32);
    fn frame_textures(&mut self, model: EeGfxModel, texture: EeGfxModel);
    fn group_color(&mut self, object: EeGfxModel, color: EeGfxColor, part: u32);
    fn sub_model_offset(&mut self, model: EeGfxModel, scale: f32) -> GeTransformation;
    fn bar_set(&mut self, which: u32, rect: Option<&GeRectangle>, colors: Option<&[EeGfxColor; 4]>);
    fn bar_draw(&mut self, which: u32) -> i32;
    fn screen_lines(&mut self, lines: &[EeGfxScreenLine], start_u: f32, end_u: f32) -> i32;
    /// `text` without a NUL
    #[allow(clippy::too_many_arguments)]
    fn label(
        &mut self,
        text: &[u8],
        x: f32,
        y: f32,
        color: EeGfxColor,
        font: u32,
        view: &GeRectangle,
        strip: &GeRectangle,
        bottom: f32,
    );
    fn texture_dereference(&mut self, texture: EeGfxModel);
    fn camera_inverse(&mut self) -> GeTransformation;
    fn camera_pitch(&mut self) -> f32;
    fn texture_reference(&mut self, texture: EeGfxModel);
    fn batch_quad(&mut self, which: u32, quad: &EeGfxEffectQuad, index: u32);
    fn batch_draw(&mut self, which: u32, count: u32, texture: EeGfxModel) -> i32;
}

impl Raster for &EeGfxRaster {
    fn set_alpha_ref(&mut self, value: f32) {
        self.rast().set_alpha_ref(value)
    }
    fn get_fog(&mut self) -> u32 {
        self.rast().fog()
    }
    fn set_fog(&mut self, state: u32) {
        self.rast().set_fog(state)
    }
    fn get_ambient(&mut self) -> EeGfxColor {
        self.rast().ambient()
    }
    fn set_ambient(&mut self, color: EeGfxColor) {
        self.rast().set_ambient(color)
    }
    fn set_zbuffer(&mut self, mode: u32) {
        self.rast().set_zbuffer(mode)
    }
    fn set_blending(&mut self, source: u32, destination: u32) {
        self.rast().set_blending(source, destination)
    }
    fn set_shading_source(&mut self, source: u32) {
        self.rast().set_shading_source(source)
    }
    fn set_alpha_testing(&mut self, state: u32) {
        self.rast().set_alpha_testing(state)
    }
    fn lighting(&mut self, kind: EeGfxLighting) {
        (self.lighting)(kind)
    }
    fn fog_factor(&mut self, location: &Ge3DPoint, fog: u32) -> f32 {
        (self.fog_factor)(location, fog)
    }
    fn model_opacity(&mut self, model: EeGfxModel, opacity: f32) {
        raster_direct::opacity(model, opacity)
    }
    fn model_transform(&mut self, model: EeGfxModel, transformation: &GeTransformation) {
        raster_direct::transform(model, transformation)
    }
    fn model_scale(&mut self, model: EeGfxModel, scale: f32) {
        raster_direct::scale(model, scale)
    }
    fn model_prepare(&mut self, model: EeGfxModel) {
        raster_direct::prepare(model)
    }
    fn model_draw(&mut self, model: EeGfxModel, opacity: f32) -> i32 {
        (self.model_draw)(model, opacity)
    }
    fn model_texture(&mut self, model: EeGfxModel, texture: EeGfxModel) {
        raster_direct::material_texture(model, texture, (self.screen)())
    }
    fn model_scroll_v(&mut self, model: EeGfxModel, delta: f32) {
        raster_direct::scroll_v(model, delta)
    }
    fn model_restore_v(&mut self, model: EeGfxModel) {
        raster_direct::restore_v(model)
    }
    fn model_color(&mut self, model: EeGfxModel, color: EeGfxColor) {
        raster_direct::color(model, color)
    }
    fn model_own_texture(&mut self, model: EeGfxModel, texture: EeGfxModel) {
        raster_direct::own_texture(model, texture, (self.screen)())
    }
    fn camera(&mut self) -> GeTransformation {
        (self.camera)()
    }
    fn standing_plane(&mut self) -> EeGfxModel {
        crate::gfx::effect_batch::plane_model(true)
    }
    fn depth_range(&mut self, near: f32, far: f32) {
        (self.depth_range)(near, far)
    }
    fn set_zcompare(&mut self, mode: u32) {
        self.rast().set_zcompare(mode)
    }
    fn model_material_opacity(&mut self, model: EeGfxModel, opacity: f32) {
        raster_direct::material_opacity(model, opacity)
    }
    fn model_transform_scaled(&mut self, model: EeGfxModel, transformation: &GeTransformation, x: f32, y: f32, z: f32) {
        raster_direct::transform_scaled(model, transformation, x, y, z)
    }
    fn model_frame(&mut self, model: EeGfxModel, frame: u32) {
        raster_direct::frame(model, frame)
    }
    fn model_pre_transform(&mut self, model: EeGfxModel, transformation: &GeTransformation) {
        raster_direct::pre_transform(model, transformation)
    }
    fn model_scroll_u(&mut self, model: EeGfxModel, delta: f32) {
        raster_direct::scroll_u(model, delta)
    }
    fn model_restore_u(&mut self, model: EeGfxModel) {
        raster_direct::restore_u(model)
    }
    fn get_culling(&mut self) -> u32 {
        self.rast().culling()
    }
    fn set_culling(&mut self, mode: u32) {
        self.rast().set_culling(mode)
    }
    fn model_shift_v(&mut self, model: EeGfxModel, delta: f32) {
        raster_direct::shift_v(model, delta)
    }
    fn ground_quad(
        &mut self,
        center: &Ge3DPoint,
        color: EeGfxColor,
        yaw: f32,
        size: f32,
        heights: &[f32; 4],
        brightness: f32,
        alpha_ref: f32,
    ) {
        crate::gfx::effect_batch::ground_quad(self, center, color, yaw, size, heights, brightness, alpha_ref)
    }
    fn ground_strip(
        &mut self,
        from: &Ge3DPoint,
        to: &Ge3DPoint,
        width: f32,
        color: EeGfxColor,
        offset: (f32, f32),
        size: (f32, f32),
        alpha: f32,
        brightness: f32,
        alpha_ref: f32,
    ) {
        crate::gfx::effect_batch::ground_strip(self, from, to, width, color, offset, size, alpha, brightness, alpha_ref)
    }
    fn texture_frame(&mut self, texture: EeGfxModel, frame: u32) {
        raster_direct::texture_frame(texture, frame)
    }
    fn camera_yaw(&mut self) -> f32 {
        (self.camera_yaw)()
    }
    fn get_zbuffer(&mut self) -> u32 {
        self.rast().zbuffer()
    }
    fn get_zcompare(&mut self) -> u32 {
        self.rast().zcompare()
    }
    fn model_tween(&mut self, model: EeGfxModel, next: u32, fraction: f32) {
        raster_direct::tween(model, next, fraction)
    }
    fn screen_point(&mut self, point: &Ge3DPoint) -> (f32, f32) {
        let (mut x, mut y) = (0.0, 0.0);
        (self.screen_point)(point, &mut x, &mut y);
        (x, y)
    }
    fn draw_text(&mut self, x: f32, y: f32, text: &str, color: EeGfxColor) {
        let mut bytes = Vec::with_capacity(text.len() + 1);
        bytes.extend_from_slice(text.as_bytes());
        bytes.push(0);
        (self.draw_text)(x, y, bytes.as_ptr(), color)
    }
    fn model_stretch(&mut self, model: EeGfxModel, length: f32, width: f32, scroll: f32) {
        raster_direct::stretch(model, length, width, scroll)
    }
    fn model_unstretch(&mut self, model: EeGfxModel) {
        raster_direct::unstretch(model)
    }
    fn vertex_count(&mut self, model: EeGfxModel) -> u32 {
        raster_direct::vertex_count(model)
    }
    fn vertex_point(&mut self, model: EeGfxModel, index: u32) -> Ge3DPoint {
        raster_direct::vertex_point(model, index)
    }
    fn set_vertex_point(&mut self, model: EeGfxModel, index: u32, point: &Ge3DPoint) {
        raster_direct::set_vertex_point(model, index, point)
    }
    fn model_bounds(&mut self, model: EeGfxModel) {
        raster_direct::bounds(model)
    }
    fn model_normals(&mut self, model: EeGfxModel) {
        raster_direct::normals(model)
    }
    fn sprite(
        &mut self,
        kind: u32,
        position: &Ge3DPoint,
        color: EeGfxColor,
        rotation: f32,
        size: f32,
        alpha: f32,
        brightness: f32,
        alpha_ref: f32,
    ) {
        crate::gfx::effect_batch::sprite(self, kind, position, color, rotation, size, alpha, brightness, alpha_ref)
    }
    fn get_alpha_ref(&mut self) -> f32 {
        self.rast().alpha_ref()
    }
    fn get_blending(&mut self) -> (u32, u32) {
        self.rast().blending()
    }
    fn create_model(&mut self, polygons: u32, vertices: u32, sub_models: u32, name: &str, flags: u32) -> EeGfxModel {
        let mut bytes = Vec::with_capacity(name.len() + 1);
        bytes.extend_from_slice(name.as_bytes());
        bytes.push(0);
        (self.create_model)(polygons, vertices, sub_models, bytes.as_ptr(), flags)
    }
    fn create_sub_model(&mut self, polygons: u32, vertices: u32, parent: EeGfxModel) -> EeGfxModel {
        (self.create_sub_model)(polygons, vertices, parent)
    }
    fn model_reference(&mut self, model: EeGfxModel) {
        raster_direct::reference(model)
    }
    fn model_dereference(&mut self, model: EeGfxModel) {
        raster_direct::dereference(model)
    }
    fn sub_model(&mut self, model: EeGfxModel, index: u32) -> EeGfxModel {
        raster_direct::sub_model(model, index)
    }
    fn store_mesh(&mut self, model: EeGfxModel, vertices: &[EeGfxMeshVertex], triangles: &[EeGfxMeshTriangle]) {
        (self.store_mesh)(model, vertices.as_ptr(), vertices.len(), triangles.as_ptr(), triangles.len())
    }
    fn material_ranges(&mut self, model: EeGfxModel, polygons: u32, vertices: u32) {
        raster_direct::material_ranges(model, polygons, vertices)
    }
    fn bounding_radius(&mut self, model: EeGfxModel) -> f32 {
        raster_direct::bounding_radius(model)
    }
    fn copy_vertices(&mut self, from: EeGfxModel, to: EeGfxModel, normals: bool) {
        raster_direct::copy_vertices(from, to, normals)
    }
    fn texture_wrap(&mut self, texture: EeGfxModel) {
        raster_direct::texture_wrap(texture)
    }
    fn load_texture(&mut self, name: &str, load: &EeGfxTextureLoad) -> EeGfxModel {
        let mut bytes = Vec::with_capacity(name.len() + 1);
        bytes.extend_from_slice(name.as_bytes());
        bytes.push(0);
        (self.load_texture)(bytes.as_ptr(), load)
    }
    fn screen_line(&mut self, from: (f32, f32), to: (f32, f32), opacity0: f32, opacity1: f32) -> i32 {
        (self.screen_line)(from.0, from.1, to.0, to.1, opacity0, opacity1)
    }

    fn get_alpha_testing(&mut self) -> u32 {
        self.rast().alpha_testing()
    }
    fn rear_clipping(&mut self, distance: f32) -> f32 {
        (self.rear_clipping)(distance)
    }
    fn draw_sky(&mut self, model: EeGfxModel, sorted: bool) -> i32 {
        (self.draw_sky)(model, sorted)
    }
    fn texture_32bit(&mut self) -> bool {
        (self.texture_32bit)()
    }
    fn can_modulate_alpha(&mut self) -> bool {
        self.rast().can_modulate_alpha()
    }
    fn set_ambient_intensity(&mut self, value: f32) {
        self.rast().set_ambient_intensity(value)
    }
    fn get_fog_attributes(&mut self) -> EeGfxFogAttributes {
        self.rast().fog_attributes()
    }
    fn set_fog_type(&mut self, kind: u32) {
        self.rast().set_fog_type(kind)
    }
    fn set_fog_color(&mut self, color: EeGfxColor) {
        self.rast().set_fog_color(color)
    }
    fn set_fog_range(&mut self, start: f32, end: f32) {
        self.rast().set_fog_range(start, end)
    }
    fn frame_textures(&mut self, model: EeGfxModel, texture: EeGfxModel) {
        raster_direct::frame_textures(model, texture, (self.screen)())
    }
    fn group_color(&mut self, object: EeGfxModel, color: EeGfxColor, part: u32) {
        raster_direct::group_color(object, color, part)
    }
    fn sub_model_offset(&mut self, model: EeGfxModel, scale: f32) -> GeTransformation {
        raster_direct::sub_model_offset(model, scale)
    }
    fn bar_set(&mut self, which: u32, rect: Option<&GeRectangle>, colors: Option<&[EeGfxColor; 4]>) {
        let r = rect.map_or(std::ptr::null(), |r| r as *const GeRectangle);
        let c = colors.map_or(std::ptr::null(), |c| c.as_ptr());
        (self.bar_set)(which, r, c)
    }
    fn bar_draw(&mut self, which: u32) -> i32 {
        (self.bar_draw)(which)
    }
    fn screen_lines(&mut self, lines: &[EeGfxScreenLine], start_u: f32, end_u: f32) -> i32 {
        (self.screen_lines)(lines.as_ptr(), lines.len() as u32, start_u, end_u)
    }
    fn label(
        &mut self,
        text: &[u8],
        x: f32,
        y: f32,
        color: EeGfxColor,
        font: u32,
        view: &GeRectangle,
        strip: &GeRectangle,
        bottom: f32,
    ) {
        let mut bytes = Vec::with_capacity(text.len() + 1);
        bytes.extend_from_slice(text);
        bytes.push(0);
        (self.label)(bytes.as_ptr(), x, y, color, font, view, strip, bottom)
    }
    fn texture_dereference(&mut self, texture: EeGfxModel) {
        raster_direct::texture_dereference(texture)
    }
    fn camera_inverse(&mut self) -> GeTransformation {
        (self.camera_inverse)()
    }
    fn camera_pitch(&mut self) -> f32 {
        (self.camera_pitch)()
    }
    fn texture_reference(&mut self, texture: EeGfxModel) {
        raster_direct::reference(texture)
    }
    fn batch_quad(&mut self, which: u32, quad: &EeGfxEffectQuad, index: u32) {
        (self.batch_quad)(which, quad, index)
    }
    fn batch_draw(&mut self, which: u32, count: u32, texture: EeGfxModel) -> i32 {
        (self.batch_draw)(which, count, texture)
    }
}

/// A rasterizer that logs every call (the tests').
#[cfg(test)]
#[derive(Default)]
pub struct LogRaster {
    pub calls: Vec<String>,
    pub fog: u32,
    pub ambient: EeGfxColor,
    pub fog_value: f32,
    pub camera: GeTransformation,
    pub culling: u32,
    /// vertex array 0's points
    pub vertices: Vec<Ge3DPoint>,
    /// the alpha reference `get_alpha_ref` answers
    pub alpha_ref: f32,
    /// the models made (`create_model`, `create_sub_model`): their handles are 1-based indices
    pub models: Vec<LogModel>,
    /// what `get_alpha_testing`, `texture_32bit`, `can_modulate_alpha`, `get_fog_attributes` answer
    pub alpha_testing: u32,
    pub texture_32bit: bool,
    pub can_modulate: bool,
    pub fog_attributes: EeGfxFogAttributes,
    /// the rear clipping distance `rear_clipping` sets
    pub rear: f32,
}

/// A model the log rasterizer made: its vertex array 0's points, normals and opacities, its triangles, sub-models.
#[cfg(test)]
#[derive(Clone, Debug, Default)]
pub struct LogModel {
    pub name: String,
    pub points: Vec<Ge3DPoint>,
    pub normals: Vec<ee_base::math::types::GeVector>,
    pub opacities: Vec<f32>,
    pub triangles: Vec<[u32; 3]>,
    pub subs: Vec<EeGfxModel>,
}

#[cfg(test)]
impl LogRaster {
    /// The model behind a handle `create_model` gave.
    pub fn model(&mut self, model: EeGfxModel) -> &mut LogModel {
        &mut self.models[model as usize - 1]
    }
    fn make(&mut self, name: &str, polygons: u32, vertices: u32) -> EeGfxModel {
        self.models.push(LogModel {
            name: name.into(),
            points: vec![Ge3DPoint::default(); vertices as usize],
            normals: vec![ee_base::math::types::GeVector::default(); vertices as usize],
            opacities: vec![1.0; vertices as usize],
            triangles: vec![[0; 3]; polygons as usize],
            subs: Vec::new(),
        });
        self.models.len() as EeGfxModel
    }
}

#[cfg(test)]
impl Raster for LogRaster {
    fn set_alpha_ref(&mut self, value: f32) {
        self.calls.push(format!("alpharef {value}"));
    }
    fn get_fog(&mut self) -> u32 {
        self.fog
    }
    fn set_fog(&mut self, state: u32) {
        self.calls.push(format!("fog {state}"));
    }
    fn get_ambient(&mut self) -> EeGfxColor {
        self.ambient
    }
    fn set_ambient(&mut self, c: EeGfxColor) {
        self.calls.push(format!("ambient {} {} {}", c.red, c.green, c.blue));
    }
    fn set_zbuffer(&mut self, mode: u32) {
        self.calls.push(format!("zbuffer {mode}"));
    }
    fn set_blending(&mut self, source: u32, destination: u32) {
        self.calls.push(format!("blend {source} {destination}"));
    }
    fn set_shading_source(&mut self, source: u32) {
        self.calls.push(format!("shading {source}"));
    }
    fn set_alpha_testing(&mut self, state: u32) {
        self.calls.push(format!("alphatest {state}"));
    }
    fn lighting(&mut self, kind: EeGfxLighting) {
        self.calls.push(format!("lighting {kind:?}"));
    }
    fn fog_factor(&mut self, l: &Ge3DPoint, fog: u32) -> f32 {
        self.calls.push(format!("fogfactor {} {} {} {fog}", l.x, l.y, l.z));
        self.fog_value
    }
    fn model_opacity(&mut self, _model: EeGfxModel, opacity: f32) {
        self.calls.push(format!("opacity {opacity}"));
    }
    fn model_transform(&mut self, _model: EeGfxModel, t: &GeTransformation) {
        self.calls.push(format!("transform {} {} {}", t.m[0][3], t.m[1][3], t.m[2][3]));
    }
    fn model_scale(&mut self, _model: EeGfxModel, scale: f32) {
        self.calls.push(format!("scale {scale}"));
    }
    fn model_prepare(&mut self, _model: EeGfxModel) {
        self.calls.push("prepare".into());
    }
    fn model_draw(&mut self, _model: EeGfxModel, opacity: f32) -> i32 {
        self.calls.push(format!("draw {opacity}"));
        0
    }
    fn model_texture(&mut self, _model: EeGfxModel, _texture: EeGfxModel) {
        self.calls.push("texture".into());
    }
    fn model_scroll_v(&mut self, _model: EeGfxModel, delta: f32) {
        self.calls.push(format!("scroll {delta}"));
    }
    fn model_restore_v(&mut self, _model: EeGfxModel) {
        self.calls.push("restore".into());
    }
    fn model_color(&mut self, _model: EeGfxModel, c: EeGfxColor) {
        self.calls.push(format!("color {} {} {}", c.red, c.green, c.blue));
    }
    fn model_own_texture(&mut self, _model: EeGfxModel, _texture: EeGfxModel) {
        self.calls.push("own texture".into());
    }
    fn camera(&mut self) -> GeTransformation {
        self.camera
    }
    fn standing_plane(&mut self) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn depth_range(&mut self, near: f32, far: f32) {
        self.calls.push(format!("depth {near} {far}"));
    }
    fn set_zcompare(&mut self, mode: u32) {
        self.calls.push(format!("zcompare {mode}"));
    }
    fn model_material_opacity(&mut self, _model: EeGfxModel, opacity: f32) {
        self.calls.push(format!("material opacity {opacity}"));
    }
    fn model_transform_scaled(&mut self, _model: EeGfxModel, t: &GeTransformation, x: f32, y: f32, z: f32) {
        self.calls.push(format!("transform {} {} {} scaled {x} {y} {z}", t.m[0][3], t.m[1][3], t.m[2][3]));
    }
    fn model_frame(&mut self, _model: EeGfxModel, frame: u32) {
        self.calls.push(format!("frame {frame}"));
    }
    fn model_pre_transform(&mut self, _model: EeGfxModel, t: &GeTransformation) {
        self.calls.push(format!("pre {}", t.m[0][0]));
    }
    fn model_scroll_u(&mut self, _model: EeGfxModel, delta: f32) {
        self.calls.push(format!("scroll u {delta}"));
    }
    fn model_restore_u(&mut self, _model: EeGfxModel) {
        self.calls.push("restore u".into());
    }
    fn get_culling(&mut self) -> u32 {
        self.culling
    }
    fn set_culling(&mut self, mode: u32) {
        self.calls.push(format!("culling {mode}"));
    }
    fn model_shift_v(&mut self, _model: EeGfxModel, delta: f32) {
        self.calls.push(format!("shift v {delta}"));
    }
    fn ground_quad(
        &mut self,
        c: &Ge3DPoint,
        k: EeGfxColor,
        yaw: f32,
        size: f32,
        heights: &[f32; 4],
        brightness: f32,
        alpha_ref: f32,
    ) {
        self.calls.push(format!(
            "quad {} {} {} color {} {} {} yaw {yaw} size {size} heights {heights:?} {brightness} {alpha_ref}",
            c.x, c.y, c.z, k.red, k.green, k.blue
        ));
    }
    fn ground_strip(
        &mut self,
        a: &Ge3DPoint,
        b: &Ge3DPoint,
        width: f32,
        k: EeGfxColor,
        offset: (f32, f32),
        size: (f32, f32),
        alpha: f32,
        brightness: f32,
        alpha_ref: f32,
    ) {
        self.calls.push(format!(
            "strip {} {} {} to {} {} {} width {width} color {} {} {} offset {offset:?} size {size:?} {alpha} {brightness} {alpha_ref}",
            a.x, a.y, a.z, b.x, b.y, b.z, k.red, k.green, k.blue
        ));
    }
    fn texture_frame(&mut self, _texture: EeGfxModel, frame: u32) {
        self.calls.push(format!("frame {frame}"));
    }
    fn camera_yaw(&mut self) -> f32 {
        0.25
    }
    fn get_zbuffer(&mut self) -> u32 {
        0
    }
    fn get_zcompare(&mut self) -> u32 {
        0
    }
    fn model_tween(&mut self, _model: EeGfxModel, next: u32, fraction: f32) {
        self.calls.push(format!("tween {next} {fraction}"));
    }
    fn screen_point(&mut self, p: &Ge3DPoint) -> (f32, f32) {
        self.calls.push(format!("screen {} {} {}", p.x, p.y, p.z));
        (p.x * 10.0, p.y * 10.0)
    }
    fn draw_text(&mut self, x: f32, y: f32, text: &str, k: EeGfxColor) {
        self.calls.push(format!("text {x} {y} {text} color {} {} {}", k.red, k.green, k.blue));
    }
    fn model_stretch(&mut self, _model: EeGfxModel, length: f32, width: f32, scroll: f32) {
        self.calls.push(format!("stretch {length} {width} {scroll}"));
    }
    fn model_unstretch(&mut self, _model: EeGfxModel) {
        self.calls.push("unstretch".into());
    }
    // a made model's own points; `vertices` for any other (null) model
    fn vertex_count(&mut self, model: EeGfxModel) -> u32 {
        if model.is_null() {
            self.vertices.len() as u32
        } else {
            self.model(model).points.len() as u32
        }
    }
    fn vertex_point(&mut self, model: EeGfxModel, index: u32) -> Ge3DPoint {
        if model.is_null() {
            self.vertices[index as usize]
        } else {
            self.model(model).points[index as usize]
        }
    }
    fn set_vertex_point(&mut self, model: EeGfxModel, index: u32, p: &Ge3DPoint) {
        if model.is_null() {
            self.vertices[index as usize] = *p;
        } else {
            self.model(model).points[index as usize] = *p;
        }
        self.calls.push(format!("point {index} {} {} {}", p.x, p.y, p.z));
    }
    fn model_bounds(&mut self, _model: EeGfxModel) {
        self.calls.push("bounds".into());
    }
    fn model_normals(&mut self, _model: EeGfxModel) {
        self.calls.push("normals".into());
    }
    fn sprite(
        &mut self,
        kind: u32,
        p: &Ge3DPoint,
        k: EeGfxColor,
        rotation: f32,
        size: f32,
        alpha: f32,
        brightness: f32,
        alpha_ref: f32,
    ) {
        self.calls.push(format!(
            "sprite {kind} {} {} {} color {} {} {} rotation {rotation} size {size} {alpha} {brightness} {alpha_ref}",
            p.x, p.y, p.z, k.red, k.green, k.blue
        ));
    }
    fn get_alpha_ref(&mut self) -> f32 {
        self.alpha_ref
    }
    fn get_blending(&mut self) -> (u32, u32) {
        (7, 8)
    }
    fn create_model(&mut self, polygons: u32, vertices: u32, sub_models: u32, name: &str, flags: u32) -> EeGfxModel {
        self.calls.push(format!("create {name} {polygons} {vertices} {sub_models} {flags}"));
        self.make(name, polygons, vertices)
    }
    fn create_sub_model(&mut self, polygons: u32, vertices: u32, parent: EeGfxModel) -> EeGfxModel {
        self.calls.push(format!("create sub {polygons} {vertices} of {}", parent as usize));
        let sub = self.make("sub", polygons, vertices);
        let subs = &mut self.model(parent).subs;
        if subs.is_empty() {
            subs.push(sub);
        } else {
            subs[0] = sub;
        }
        sub
    }
    fn model_reference(&mut self, model: EeGfxModel) {
        self.calls.push(format!("reference {}", model as usize));
    }
    fn model_dereference(&mut self, model: EeGfxModel) {
        self.calls.push(format!("dereference {}", model as usize));
    }
    fn sub_model(&mut self, model: EeGfxModel, index: u32) -> EeGfxModel {
        self.model(model).subs[index as usize]
    }
    fn store_mesh(&mut self, model: EeGfxModel, vertices: &[EeGfxMeshVertex], triangles: &[EeGfxMeshTriangle]) {
        self.calls.push(format!("store {} {} {}", model as usize, vertices.len(), triangles.len()));
        let m = self.model(model);
        for (i, v) in vertices.iter().enumerate() {
            if v.set & crate::gfx::mesh::EE_GFX_MESH_POINT != 0 {
                m.points[i] = v.point;
            }
            if v.set & crate::gfx::mesh::EE_GFX_MESH_NORMAL != 0 {
                m.normals[i] = v.normal;
            }
            if v.set & crate::gfx::mesh::EE_GFX_MESH_OPACITY != 0 {
                m.opacities[i] = v.opacity;
            }
        }
        for (i, t) in triangles.iter().enumerate() {
            if t.set != 0 {
                m.triangles[i] = t.v;
            }
        }
    }
    fn material_ranges(&mut self, model: EeGfxModel, polygons: u32, vertices: u32) {
        self.calls.push(format!("ranges {} {polygons} {vertices}", model as usize));
    }
    fn bounding_radius(&mut self, model: EeGfxModel) -> f32 {
        self.calls.push(format!("radius {}", model as usize));
        2.0
    }
    fn copy_vertices(&mut self, from: EeGfxModel, to: EeGfxModel, normals: bool) {
        self.calls.push(format!("copy {} {} {normals}", from as usize, to as usize));
        let (points, n) = (self.model(from).points.clone(), self.model(from).normals.clone());
        let t = self.model(to);
        t.points[..points.len()].copy_from_slice(&points);
        if normals {
            t.normals[..n.len()].copy_from_slice(&n);
        }
    }
    fn texture_wrap(&mut self, _texture: EeGfxModel) {
        self.calls.push("wrap".into());
    }
    fn load_texture(&mut self, name: &str, load: &EeGfxTextureLoad) -> EeGfxModel {
        self.calls.push(format!(
            "texture {name} {} {} {} {} {} {}",
            load.pixel_depth, load.has_alpha, load.min_filter, load.mip_filter, load.addressing, load.reference
        ));
        0x7700 as EeGfxModel
    }
    fn screen_line(&mut self, from: (f32, f32), to: (f32, f32), opacity0: f32, opacity1: f32) -> i32 {
        self.calls.push(format!("line {} {} {} {} {opacity0} {opacity1}", from.0, from.1, to.0, to.1));
        0
    }

    fn get_alpha_testing(&mut self) -> u32 {
        self.alpha_testing
    }
    fn rear_clipping(&mut self, distance: f32) -> f32 {
        self.calls.push(format!("rear {distance}"));
        std::mem::replace(&mut self.rear, distance)
    }
    fn draw_sky(&mut self, model: EeGfxModel, sorted: bool) -> i32 {
        self.calls.push(format!("sky {} {sorted}", model as usize));
        0
    }
    fn texture_32bit(&mut self) -> bool {
        self.texture_32bit
    }
    fn can_modulate_alpha(&mut self) -> bool {
        self.can_modulate
    }
    fn set_ambient_intensity(&mut self, value: f32) {
        self.calls.push(format!("ambient intensity {value}"));
    }
    fn get_fog_attributes(&mut self) -> EeGfxFogAttributes {
        self.fog_attributes
    }
    fn set_fog_type(&mut self, kind: u32) {
        self.calls.push(format!("fog type {kind}"));
    }
    fn set_fog_color(&mut self, c: EeGfxColor) {
        self.calls.push(format!("fog color {} {} {}", c.red, c.green, c.blue));
    }
    fn set_fog_range(&mut self, start: f32, end: f32) {
        self.calls.push(format!("fog range {start} {end}"));
    }
    fn frame_textures(&mut self, model: EeGfxModel, texture: EeGfxModel) {
        self.calls.push(format!("frame textures {} {}", model as usize, texture as usize));
    }
    fn group_color(&mut self, object: EeGfxModel, c: EeGfxColor, part: u32) {
        self.calls.push(format!("group {} {} {} {} {part}", object as usize, c.red, c.green, c.blue));
    }
    fn sub_model_offset(&mut self, model: EeGfxModel, scale: f32) -> GeTransformation {
        self.calls.push(format!("offset {} {scale}", model as usize));
        GeTransformation::IDENTITY
    }
    fn bar_set(&mut self, which: u32, rect: Option<&GeRectangle>, colors: Option<&[EeGfxColor; 4]>) {
        if let Some(r) = rect {
            self.calls.push(format!("bar {which} rect {} {} {} {}", r.left, r.top, r.right, r.bottom));
        }
        if let Some(c) = colors {
            self.calls.push(format!("bar {which} colors {} {} {}", c[0].green, c[1].green, c[2].green));
        }
    }
    fn bar_draw(&mut self, which: u32) -> i32 {
        self.calls.push(format!("bar {which} draw"));
        0
    }
    fn screen_lines(&mut self, lines: &[EeGfxScreenLine], start_u: f32, end_u: f32) -> i32 {
        self.calls.push(format!("lines {} {start_u} {end_u}", lines.len()));
        0
    }
    fn label(
        &mut self,
        text: &[u8],
        x: f32,
        y: f32,
        c: EeGfxColor,
        font: u32,
        _view: &GeRectangle,
        _strip: &GeRectangle,
        bottom: f32,
    ) {
        let t = String::from_utf8_lossy(text);
        self.calls.push(format!("label {t} {x} {y} {} {font} {bottom}", c.red));
    }
    fn texture_dereference(&mut self, texture: EeGfxModel) {
        self.calls.push(format!("texture dereference {}", texture as usize));
    }
    fn camera_inverse(&mut self) -> GeTransformation {
        GeTransformation::IDENTITY
    }
    fn camera_pitch(&mut self) -> f32 {
        0.0
    }
    fn texture_reference(&mut self, texture: EeGfxModel) {
        self.calls.push(format!("texture reference {}", texture as usize));
    }
    fn batch_quad(&mut self, which: u32, _quad: &EeGfxEffectQuad, index: u32) {
        self.calls.push(format!("batch quad {which} {index}"));
    }
    fn batch_draw(&mut self, which: u32, count: u32, texture: EeGfxModel) -> i32 {
        self.calls.push(format!("batch draw {which} {count} {}", texture as usize));
        0
    }
}
