//! The rasterizer as Rust draws call it: one C++ table (`EeGfxRaster`, filled once in `EERenderObject.cpp` over
//! `gRasterizer` and the current screen `gScreenIndex`) of the `GERasterizer` state and draw calls the effects' and
//! render objects' draw passes make, the capture's lighting set-ups and fog factor, and a model's draw by handle
//! (`GEModel*`). Rust code draws through the `Raster` trait: the table in the game, a log in the tests.
//!
//! Drawn only (the renderer's thread).

use std::ffi::c_void;

use ee_ge::types::{Ge3DPoint, GeTransformation};

use crate::effects::EeGfxColor;
use crate::lighting::EeGfxLighting;

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

/// The C++'s rasterizer calls, each on the current screen.
#[repr(C)]
pub struct EeGfxRaster {
    pub set_alpha_ref: extern "C" fn(value: f32),
    pub get_fog: extern "C" fn() -> u32,
    pub set_fog: extern "C" fn(state: u32),
    pub get_ambient: extern "C" fn() -> EeGfxColor,
    pub set_ambient: extern "C" fn(color: EeGfxColor),
    pub set_zbuffer: extern "C" fn(mode: u32),
    pub set_blending: extern "C" fn(source: u32, destination: u32),
    pub set_shading_source: extern "C" fn(source: u32),
    pub set_alpha_testing: extern "C" fn(state: u32),
    /// one of the capture's lighting set-ups (`SetWhiteLighting` ...)
    pub lighting: extern "C" fn(kind: EeGfxLighting),
    /// `GetFogFactor` (it reads the fog's attributes and turns the fog off when it is on)
    pub fog_factor: extern "C" fn(location: *const Ge3DPoint, fog: u32) -> f32,
    pub model_opacity: extern "C" fn(model: EeGfxModel, opacity: f32),
    pub model_transform: extern "C" fn(model: EeGfxModel, transformation: *const GeTransformation),
    pub model_scale: extern "C" fn(model: EeGfxModel, scale: f32),
    pub model_prepare: extern "C" fn(model: EeGfxModel),
    /// `Draw(model, screen, opacity, gOrthographicView)`
    pub model_draw: extern "C" fn(model: EeGfxModel, opacity: f32) -> i32,
    /// `GetMaterial(0)->SetTexture(texture, screen)`
    pub model_texture: extern "C" fn(model: EeGfxModel, texture: EeGfxModel),
    /// adds `delta` to the v of every texture coordinate of vertex array 0, keeping the old ones
    pub model_scroll_v: extern "C" fn(model: EeGfxModel, delta: f32),
    /// puts back the texture coordinates `model_scroll_v` kept
    pub model_restore_v: extern "C" fn(model: EeGfxModel),
    /// `GetMaterial(0)->SetColor(color)`
    pub model_color: extern "C" fn(model: EeGfxModel, color: EeGfxColor),
    /// the model's own `SetTexture(texture, screen)` (not its material's)
    pub model_own_texture: extern "C" fn(model: EeGfxModel, texture: EeGfxModel),
    /// the render snapshot's camera transformation (`gRenderSnapshot.mCamera`)
    pub camera: extern "C" fn() -> GeTransformation,
    /// the shared standing plane model (`gEffect2DModel`)
    pub standing_plane: extern "C" fn() -> EeGfxModel,
    /// the main viewport's depth range, then the viewport set on the rasterizer (`gU4E8F08`)
    pub depth_range: extern "C" fn(near: f32, far: f32),
    pub set_zcompare: extern "C" fn(mode: u32),
    /// `GetMaterial(0)->SetOpacity(opacity)`
    pub model_material_opacity: extern "C" fn(model: EeGfxModel, opacity: f32),
    /// a copy of `transformation` scaled by `GETransformation::Scale(x, y, z)`, then the model's post-model transform
    pub model_transform_scaled:
        extern "C" fn(model: EeGfxModel, transformation: *const GeTransformation, x: f32, y: f32, z: f32),
    /// `SetGraphicsCurrentFrame(frame)`
    pub model_frame: extern "C" fn(model: EeGfxModel, frame: u32),
    /// `SetGraphicsPreModelTransform(transformation)`
    pub model_pre_transform: extern "C" fn(model: EeGfxModel, transformation: *const GeTransformation),
    /// adds `delta` to the u of every texture coordinate of vertex array 0, keeping the old ones
    pub model_scroll_u: extern "C" fn(model: EeGfxModel, delta: f32),
    /// puts back the texture coordinates `model_scroll_u` kept
    pub model_restore_u: extern "C" fn(model: EeGfxModel),
    /// `GetCulling()`, `SetCulling(mode)`
    pub get_culling: extern "C" fn() -> u32,
    pub set_culling: extern "C" fn(mode: u32),
    /// adds `delta` to the v of every texture coordinate of vertex array 0 for good (nothing kept)
    pub model_shift_v: extern "C" fn(model: EeGfxModel, delta: f32),
    /// `AddGroundQuad(center, color, yaw, size, heights, brightness, alpha reference)` (four heights)
    pub ground_quad: extern "C" fn(
        center: *const Ge3DPoint,
        color: EeGfxColor,
        yaw: f32,
        size: f32,
        heights: *const f32,
        brightness: f32,
        alpha_ref: f32,
    ),
    /// `SetFrame(frame)` of a texture (`GETexture*`)
    pub texture_frame: extern "C" fn(texture: EeGfxModel, frame: u32),
    /// the camera's yaw (`gRenderSnapshot.mCameraYaw`)
    pub camera_yaw: extern "C" fn() -> f32,
    /// `GetZBuffer`, `GetZCompare`
    pub get_zbuffer: extern "C" fn() -> u32,
    pub get_zcompare: extern "C" fn() -> u32,
    /// `AddGroundStrip(from, to, width, color, offset, size, alpha, brightness, alpha reference)`
    pub ground_strip: extern "C" fn(
        from: *const Ge3DPoint,
        to: *const Ge3DPoint,
        width: f32,
        color: EeGfxColor,
        offset_u: f32,
        offset_v: f32,
        size_u: f32,
        size_v: f32,
        alpha: f32,
        brightness: f32,
        alpha_ref: f32,
    ),
    /// `SetGraphicsNextFrame(next)` then `SetFractionalFramePosition(fraction)` (the tween between two frames)
    pub model_tween: extern "C" fn(model: EeGfxModel, next: u32, fraction: f32),
    /// `GetScreenPoint`: the world point's place on the screen (the camera, then the main viewport)
    pub screen_point: extern "C" fn(point: *const Ge3DPoint, x: *mut f32, y: *mut f32),
    /// a `GEText` in Tahoma 10, transparent and centred at (`x`, `y`) in `color`, its text the NUL-terminated UTF-8
    /// `text`, drawn by `DrawClippedText` clipped to the viewport's view rectangle on the main game form (the scenario
    /// editor's under a free camera)
    pub draw_text: extern "C" fn(x: f32, y: f32, text: *const u8, color: EeGfxColor),
    /// the tracer beam's stretch (`EEE3DTracerBeam::DrawPass6`, 0xfca9b): vertex array 0's points and texture
    /// coordinates kept, then y stretched to `length` and x to `width`, u scrolled by `scroll` (`beam.rs`); each
    /// sub-model's internal y kept and stretched; `ComputeBoundingVolumes`
    pub model_stretch: extern "C" fn(model: EeGfxModel, length: f32, width: f32, scroll: f32),
    /// puts back the vertices and sub-model ys `model_stretch` kept
    pub model_unstretch: extern "C" fn(model: EeGfxModel),
    /// vertex array 0's size (`GetVertexArray(0)->GetSize()`)
    pub vertex_count: extern "C" fn(model: EeGfxModel) -> u32,
    /// vertex array 0's point `index` (`GetPoint`)
    pub vertex_point: extern "C" fn(model: EeGfxModel, index: u32) -> Ge3DPoint,
    /// vertex array 0's point `index` set (`SetPoint`)
    pub set_vertex_point: extern "C" fn(model: EeGfxModel, index: u32, point: *const Ge3DPoint),
    /// `ComputeBoundingVolumes`, `ComputeModelNormals`
    pub model_bounds: extern "C" fn(model: EeGfxModel),
    pub model_normals: extern "C" fn(model: EeGfxModel),
    /// one effect quad (`EE_GFX_SPRITE_GLOW`, `_GROUND`, `_BLENDED`: `AddGlowSprite`, `AddGroundSprite`,
    /// `AddBlendedSprite` of `EEEffect.cpp`) at `position` turned by `rotation`
    pub sprite: extern "C" fn(
        kind: u32,
        position: *const Ge3DPoint,
        color: EeGfxColor,
        rotation: f32,
        size: f32,
        alpha: f32,
        brightness: f32,
        alpha_ref: f32,
    ),
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
}

impl Raster for &EeGfxRaster {
    fn set_alpha_ref(&mut self, value: f32) {
        (self.set_alpha_ref)(value)
    }
    fn get_fog(&mut self) -> u32 {
        (self.get_fog)()
    }
    fn set_fog(&mut self, state: u32) {
        (self.set_fog)(state)
    }
    fn get_ambient(&mut self) -> EeGfxColor {
        (self.get_ambient)()
    }
    fn set_ambient(&mut self, color: EeGfxColor) {
        (self.set_ambient)(color)
    }
    fn set_zbuffer(&mut self, mode: u32) {
        (self.set_zbuffer)(mode)
    }
    fn set_blending(&mut self, source: u32, destination: u32) {
        (self.set_blending)(source, destination)
    }
    fn set_shading_source(&mut self, source: u32) {
        (self.set_shading_source)(source)
    }
    fn set_alpha_testing(&mut self, state: u32) {
        (self.set_alpha_testing)(state)
    }
    fn lighting(&mut self, kind: EeGfxLighting) {
        (self.lighting)(kind)
    }
    fn fog_factor(&mut self, location: &Ge3DPoint, fog: u32) -> f32 {
        (self.fog_factor)(location, fog)
    }
    fn model_opacity(&mut self, model: EeGfxModel, opacity: f32) {
        (self.model_opacity)(model, opacity)
    }
    fn model_transform(&mut self, model: EeGfxModel, transformation: &GeTransformation) {
        (self.model_transform)(model, transformation)
    }
    fn model_scale(&mut self, model: EeGfxModel, scale: f32) {
        (self.model_scale)(model, scale)
    }
    fn model_prepare(&mut self, model: EeGfxModel) {
        (self.model_prepare)(model)
    }
    fn model_draw(&mut self, model: EeGfxModel, opacity: f32) -> i32 {
        (self.model_draw)(model, opacity)
    }
    fn model_texture(&mut self, model: EeGfxModel, texture: EeGfxModel) {
        (self.model_texture)(model, texture)
    }
    fn model_scroll_v(&mut self, model: EeGfxModel, delta: f32) {
        (self.model_scroll_v)(model, delta)
    }
    fn model_restore_v(&mut self, model: EeGfxModel) {
        (self.model_restore_v)(model)
    }
    fn model_color(&mut self, model: EeGfxModel, color: EeGfxColor) {
        (self.model_color)(model, color)
    }
    fn model_own_texture(&mut self, model: EeGfxModel, texture: EeGfxModel) {
        (self.model_own_texture)(model, texture)
    }
    fn camera(&mut self) -> GeTransformation {
        (self.camera)()
    }
    fn standing_plane(&mut self) -> EeGfxModel {
        (self.standing_plane)()
    }
    fn depth_range(&mut self, near: f32, far: f32) {
        (self.depth_range)(near, far)
    }
    fn set_zcompare(&mut self, mode: u32) {
        (self.set_zcompare)(mode)
    }
    fn model_material_opacity(&mut self, model: EeGfxModel, opacity: f32) {
        (self.model_material_opacity)(model, opacity)
    }
    fn model_transform_scaled(&mut self, model: EeGfxModel, transformation: &GeTransformation, x: f32, y: f32, z: f32) {
        (self.model_transform_scaled)(model, transformation, x, y, z)
    }
    fn model_frame(&mut self, model: EeGfxModel, frame: u32) {
        (self.model_frame)(model, frame)
    }
    fn model_pre_transform(&mut self, model: EeGfxModel, transformation: &GeTransformation) {
        (self.model_pre_transform)(model, transformation)
    }
    fn model_scroll_u(&mut self, model: EeGfxModel, delta: f32) {
        (self.model_scroll_u)(model, delta)
    }
    fn model_restore_u(&mut self, model: EeGfxModel) {
        (self.model_restore_u)(model)
    }
    fn get_culling(&mut self) -> u32 {
        (self.get_culling)()
    }
    fn set_culling(&mut self, mode: u32) {
        (self.set_culling)(mode)
    }
    fn model_shift_v(&mut self, model: EeGfxModel, delta: f32) {
        (self.model_shift_v)(model, delta)
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
        (self.ground_quad)(center, color, yaw, size, heights.as_ptr(), brightness, alpha_ref)
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
        (self.ground_strip)(from, to, width, color, offset.0, offset.1, size.0, size.1, alpha, brightness, alpha_ref)
    }
    fn texture_frame(&mut self, texture: EeGfxModel, frame: u32) {
        (self.texture_frame)(texture, frame)
    }
    fn camera_yaw(&mut self) -> f32 {
        (self.camera_yaw)()
    }
    fn get_zbuffer(&mut self) -> u32 {
        (self.get_zbuffer)()
    }
    fn get_zcompare(&mut self) -> u32 {
        (self.get_zcompare)()
    }
    fn model_tween(&mut self, model: EeGfxModel, next: u32, fraction: f32) {
        (self.model_tween)(model, next, fraction)
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
        (self.model_stretch)(model, length, width, scroll)
    }
    fn model_unstretch(&mut self, model: EeGfxModel) {
        (self.model_unstretch)(model)
    }
    fn vertex_count(&mut self, model: EeGfxModel) -> u32 {
        (self.vertex_count)(model)
    }
    fn vertex_point(&mut self, model: EeGfxModel, index: u32) -> Ge3DPoint {
        (self.vertex_point)(model, index)
    }
    fn set_vertex_point(&mut self, model: EeGfxModel, index: u32, point: &Ge3DPoint) {
        (self.set_vertex_point)(model, index, point)
    }
    fn model_bounds(&mut self, model: EeGfxModel) {
        (self.model_bounds)(model)
    }
    fn model_normals(&mut self, model: EeGfxModel) {
        (self.model_normals)(model)
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
        (self.sprite)(kind, position, color, rotation, size, alpha, brightness, alpha_ref)
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
    fn vertex_count(&mut self, _model: EeGfxModel) -> u32 {
        self.vertices.len() as u32
    }
    fn vertex_point(&mut self, _model: EeGfxModel, index: u32) -> Ge3DPoint {
        self.vertices[index as usize]
    }
    fn set_vertex_point(&mut self, _model: EeGfxModel, index: u32, p: &Ge3DPoint) {
        self.vertices[index as usize] = *p;
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
}
