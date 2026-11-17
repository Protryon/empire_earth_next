//! The ground shadows' render object (`src/Empire Earth/Graphics Thread/EEROGroundShadows.cpp`, vtable 0x4413f0):
//! the shared "StaticShadow" quad (`CreateShadowModel`, 0x511df8, made and released through the raster table) and the
//! scene pass (`RenderScene`, 0x51207f): white lighting and the scene fog, alpha blending with the z buffer tested
//! only, no culling; the placed objects' shadows (`DrawShadows`, 0x5121c8) and the units' (`DrawUnitShadows`,
//! 0x5122bb) when ground shadows are on, then the effects' ground pass (queue 0, the C++'s), and the alpha reference
//! put back. The C++ hands the frame's render lists (`EERenderLists::mShadows2`, `mUnits2`) over as arrays.
//!
//! Drawn only (the renderer's thread).

use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

use crate::ge::transform::RotationOrder;
use crate::ge::types::{Ge3DPoint, GeTransformation};
use crate::ge::um;

use crate::gfx::effects::{EeGfxColor, EeGfxTexCoord};
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::mesh::{EeGfxMeshTriangle, EeGfxMeshVertex, EE_GFX_MESH_COORD, EE_GFX_MESH_POINT};
use crate::gfx::raster::{EeGfxModel, Raster, BLEND_INV_SRC_ALPHA, BLEND_SRC_ALPHA, ZBUFFER_TEST_ONLY};

/// `kShadowHeight`: the shadows' lift over the ground (.rdata 0x4413e4).
const SHADOW_HEIGHT: f32 = 0.01;
/// `kShadowFade`: a unit's shadow shrinks by this per unit of height (.rdata 0x4413e8).
const SHADOW_FADE: f32 = 0.1;
/// `kUnitCategorySubmergedNaval`: casts no shadow.
const CATEGORY_SUBMERGED_NAVAL: i32 = 0xa;
/// `kCullNone`
const CULL_NONE: u32 = 0;

/// One placed object's shadow (`EERenderLists::mShadows2`: its texture key and `EERenderShadow`).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGfxShadow {
    pub texture: EeGfxModel,
    /// `mLocation`
    pub location: Ge3DPoint,
    /// `mScale` (drawn when positive)
    pub scale: f32,
    /// `mOpacity`
    pub opacity: f32,
}

/// One unit's shadow (`EERenderLists::mUnits2`'s `EERenderUnit`, its source's shadow model and texture, its class's
/// scale).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGfxUnitShadow {
    /// `mTransformation`
    pub transformation: GeTransformation,
    /// `mSource->mShadowModel` (null: the shared quad)
    pub model: EeGfxModel,
    /// `mSource->mShadowTexture`
    pub texture: EeGfxModel,
    /// `mCategory`
    pub category: i32,
    /// `mShadowScale` (drawn when positive)
    pub shadow_scale: f32,
    /// `mGroundZ`
    pub ground_z: f32,
    /// `mScale`
    pub scale: f32,
    /// `gUnitClassTable[mUnitClassIndex]->GetScale()`
    pub class_scale: f32,
    /// `mOpacity`
    pub opacity: f32,
}

/// The frame's lists and the C++'s answers for one `RenderScene`.
#[repr(C)]
pub struct EeGfxShadowScene {
    pub context: *mut c_void,
    pub shadows: *const EeGfxShadow,
    pub shadow_count: usize,
    pub units: *const EeGfxUnitShadow,
    pub unit_count: usize,
    /// `gDrawGroundShadows`
    pub draw_shadows: bool,
    /// `gRenderSnapshot.mIsSpaceMap`
    pub space_map: bool,
    /// the terrain mesh's water info of a tile, -1 off the mesh (`GetIsValidTile`, `GetWaterInfo`)
    pub tile_water: extern "C" fn(context: *mut c_void, x: u32, y: u32) -> i32,
    /// the effects of render queue 0, batched per texture (`BeginEffectBatches`, `DrawPass2`, `FlushEffectBatches`,
    /// `EndEffectBatches`)
    pub draw_effects: extern "C" fn(context: *mut c_void),
}

/// The shared quad (0x529098), referenced while it lives.
static SHADOW_MODEL: AtomicPtr<c_void> = AtomicPtr::new(std::ptr::null_mut());

/// `CreateShadowModel` (0x511df8): a unit square in the ground plane, white, opaque, two triangles.
pub fn create_shadow_model<R: Raster>(x: &mut R) -> EeGfxModel {
    let model = x.create_model(2, 4, 0, "StaticShadow", 1);
    x.model_reference(model);
    let corners = [(-0.5, 0.5, 0.0, 0.0), (0.5, 0.5, 1.0, 0.0), (-0.5, -0.5, 0.0, 1.0), (0.5, -0.5, 1.0, 1.0)];
    let mut vertices = [EeGfxMeshVertex::default(); 4];
    for (v, &(px, py, u, w)) in vertices.iter_mut().zip(corners.iter()) {
        v.set = EE_GFX_MESH_POINT | EE_GFX_MESH_COORD;
        v.point = Ge3DPoint { x: px, y: py, z: 0.0 };
        v.coord = EeGfxTexCoord { u, v: w };
    }
    let triangles = [EeGfxMeshTriangle { set: 1, v: [0, 3, 1] }, EeGfxMeshTriangle { set: 1, v: [0, 2, 3] }];
    x.store_mesh(model, &vertices, &triangles);
    x.model_color(model, EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 });
    x.model_material_opacity(model, 1.0);
    x.material_ranges(model, 2, 4);
    x.model_normals(model);
    x.model_bounds(model);
    SHADOW_MODEL.store(model, Ordering::Relaxed);
    model
}

/// `ReleaseShadowModel` (0x512513).
pub fn release_shadow_model<R: Raster>(x: &mut R) {
    let model = SHADOW_MODEL.swap(std::ptr::null_mut(), Ordering::Relaxed);
    if !model.is_null() {
        x.model_dereference(model);
    }
}

/// `RenderScene` (0x51207f) over the shared quad `quad`.
pub fn render_scene<R: Raster>(
    x: &mut R,
    quad: EeGfxModel,
    shadows: &[EeGfxShadow],
    units: &[EeGfxUnitShadow],
    s: &EeGfxShadowScene,
) {
    x.lighting(EeGfxLighting::White);
    x.lighting(EeGfxLighting::SceneFog);
    let alpha_ref = x.get_alpha_ref();
    x.set_alpha_ref(0.0);
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_shading_source(0);
    x.set_culling(CULL_NONE);
    if s.draw_shadows {
        draw_shadows(x, quad, shadows);
        draw_unit_shadows(x, quad, units, |p| over_space(s, p));
    }
    (s.draw_effects)(s.context);
    x.set_alpha_ref(alpha_ref);
}

/// `DrawShadows` (0x5121c8): each shadow with a positive scale as the quad just above its location.
pub fn draw_shadows<R: Raster>(x: &mut R, quad: EeGfxModel, shadows: &[EeGfxShadow]) {
    let mut t = GeTransformation::IDENTITY;
    for s in shadows {
        if s.scale > 0.0 {
            t.m[0][3] = s.location.x;
            t.m[1][3] = s.location.y;
            t.m[2][3] = s.location.z + SHADOW_HEIGHT;
            x.model_transform(quad, &t);
            x.model_scale(quad, s.scale);
            x.model_prepare(quad);
            x.model_texture(quad, s.texture);
            x.model_opacity(quad, s.opacity);
            x.model_draw(quad, 1.0);
        }
    }
}

/// `DrawUnitShadows` (0x5122bb): each unit's shadow (its own model or the quad) on the ground under it, turned to its
/// heading and shrunk by its height (none from 10 up); not for submerged units or units over space (`over_space`).
pub fn draw_unit_shadows<R: Raster>(
    x: &mut R,
    quad: EeGfxModel,
    units: &[EeGfxUnitShadow],
    mut over_space: impl FnMut(&Ge3DPoint) -> bool,
) {
    let mut t = GeTransformation::IDENTITY;
    for u in units {
        let casts = u.shadow_scale > 0.0;
        if u.category == CATEGORY_SUBMERGED_NAVAL || !casts {
            continue;
        }
        let m = &u.transformation.m;
        if over_space(&Ge3DPoint { x: m[0][3], y: m[1][3], z: m[2][3] }) {
            continue;
        }
        let model = if u.model.is_null() { quad } else { u.model };
        t.set_orientation(RotationOrder::Ypr, um::um_arctan2(m[1][0], m[0][0]), 0.0, 0.0);
        t.m[0][3] = m[0][3];
        t.m[1][3] = m[1][3];
        t.m[2][3] = SHADOW_HEIGHT + u.ground_z;
        x.model_transform(model, &t);
        let mut fade = 1.0 - (m[2][3] - u.ground_z) * SHADOW_FADE;
        // `min(1.0f, theFade)` (the macro: `1 < fade ? 1 : fade`)
        if 1.0 < fade {
            fade = 1.0;
        }
        // a unit 10 or more units above the ground casts no shadow (a negative scale would mirror it)
        if fade > 0.0 {
            let scale = u.scale / u.class_scale * u.shadow_scale;
            x.model_scale(model, scale * fade);
            x.model_opacity(model, u.opacity);
            x.model_texture(model, u.texture);
            x.model_prepare(model);
            x.model_draw(model, 1.0);
        }
    }
}

/// `GetIsOverSpace` (0x51246b): on a space map, a space tile (water info 0x80) or off the mesh.
fn over_space(s: &EeGfxShadowScene, p: &Ge3DPoint) -> bool {
    if !s.space_map {
        return false;
    }
    let water = (s.tile_water)(s.context, um::floor_unsigned(p.x), um::floor_unsigned(p.y));
    water < 0 || water == 0x80
}

/// The shared quad (null before `create_shadow_model`).
pub fn shadow_model() -> EeGfxModel {
    SHADOW_MODEL.load(Ordering::Relaxed)
}

/// `EEROGroundShadows::CreateShadowModel`.
///
/// # Safety
/// `raster` is readable and its functions valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_ground_shadows_create(raster: *const crate::gfx::raster::EeGfxRaster) {
    // SAFETY: the function's contract
    let mut x = unsafe { &*raster };
    create_shadow_model(&mut x);
}

/// `EEROGroundShadows::ReleaseShadowModel`.
///
/// # Safety
/// `raster` is readable and its functions valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_ground_shadows_release(raster: *const crate::gfx::raster::EeGfxRaster) {
    // SAFETY: the function's contract
    let mut x = unsafe { &*raster };
    release_shadow_model(&mut x);
}

/// `EEROGroundShadows::RenderScene` over the frame's lists.
///
/// # Safety
/// `raster` and `scene` are readable and their functions valid; `scene`'s arrays hold their counts.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_ground_shadows_render(
    raster: *const crate::gfx::raster::EeGfxRaster,
    scene: *const EeGfxShadowScene,
) {
    // SAFETY: the function's contract
    unsafe {
        let mut x = &*raster;
        let s = &*scene;
        let shadows = if s.shadow_count == 0 { &[][..] } else { std::slice::from_raw_parts(s.shadows, s.shadow_count) };
        let units = if s.unit_count == 0 { &[][..] } else { std::slice::from_raw_parts(s.units, s.unit_count) };
        render_scene(&mut x, shadow_model(), shadows, units, s);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::gfx::raster::LogRaster;

    fn unit(z: f32, ground: f32) -> EeGfxUnitShadow {
        let mut t = GeTransformation::IDENTITY;
        t.m[0][3] = 3.5;
        t.m[1][3] = 4.5;
        t.m[2][3] = z;
        EeGfxUnitShadow {
            transformation: t,
            model: std::ptr::null_mut(),
            texture: std::ptr::null_mut(),
            category: 1,
            shadow_scale: 0.5,
            ground_z: ground,
            scale: 2.0,
            class_scale: 4.0,
            opacity: 0.75,
        }
    }

    #[test]
    fn the_quad_is_a_white_unit_square() {
        let mut x = LogRaster::default();
        let q = create_shadow_model(&mut x);
        assert_eq!(x.model(q).points[3], Ge3DPoint { x: 0.5, y: -0.5, z: 0.0 });
        assert_eq!(x.model(q).triangles, vec![[0, 3, 1], [0, 2, 3]]);
        assert_eq!(x.calls[0], "create StaticShadow 2 4 0 1");
        assert_eq!(shadow_model(), q);
        release_shadow_model(&mut x);
        assert!(shadow_model().is_null());
        assert_eq!(x.calls.last().unwrap(), "dereference 1");
    }

    #[test]
    fn a_unit_shadow_shrinks_with_height_and_vanishes_at_ten() {
        let mut x = LogRaster::default();
        draw_unit_shadows(&mut x, std::ptr::null_mut(), &[unit(5.0, 0.0), unit(12.0, 2.0), unit(1.0, 3.0)], |_| false);
        // 0.5 * 0.5 at height 5; at 10 nothing but the transform; below the ground the fade is 1
        let scales: Vec<_> = x.calls.iter().filter(|c| c.starts_with("scale")).cloned().collect();
        assert_eq!(scales, vec!["scale 0.125", "scale 0.25"]);
        assert_eq!(x.calls.iter().filter(|c| c.starts_with("draw")).count(), 2);
    }

    #[test]
    fn submerged_units_and_units_over_space_cast_none() {
        let mut x = LogRaster::default();
        let mut sub = unit(0.0, 0.0);
        sub.category = CATEGORY_SUBMERGED_NAVAL;
        draw_unit_shadows(&mut x, std::ptr::null_mut(), &[sub, unit(0.0, 0.0)], |_| true);
        assert!(x.calls.is_empty());
    }

    #[test]
    fn only_shadows_with_a_scale_are_drawn() {
        let mut x = LogRaster::default();
        let s = |scale| EeGfxShadow {
            texture: std::ptr::null_mut(),
            location: Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 },
            scale,
            opacity: 0.5,
        };
        draw_shadows(&mut x, std::ptr::null_mut(), &[s(0.0), s(2.0)]);
        assert_eq!(x.calls, vec!["transform 1 2 3.01", "scale 2", "prepare", "texture", "opacity 0.5", "draw 1"]);
    }
}
