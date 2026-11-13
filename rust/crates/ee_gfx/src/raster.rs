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
pub(crate) const BLEND_ONE: u32 = 1;
pub(crate) const BLEND_SRC_ALPHA: u32 = 4;
pub(crate) const BLEND_INV_SRC_ALPHA: u32 = 5;
/// `kCompareLess`, `kCompareLessEqual` (the z compare).
pub(crate) const COMPARE_LESS: u32 = 1;
pub(crate) const COMPARE_LESS_EQUAL: u32 = 2;

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
}
