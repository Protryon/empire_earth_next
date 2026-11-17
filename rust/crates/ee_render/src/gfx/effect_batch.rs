//! The effects' two quad batches and shared plane models (`src/Empire Earth/Graphics Thread/EEEffect.cpp`,
//! 0x10269c..0x103f64): the camera batch (0x4e8db8: `AddGlowSprite`'s additive, camera-facing quads) and the ground
//! batch (0x4e8da8: `AddGroundSprite`, `AddGroundQuad`, `AddBlendedSprite`, `AddGroundStrip`), each a texture, the
//! smallest alpha reference of its quads and a vertex count, drawn in one call per 0x800 vertices; `GetEffectFogFactor`;
//! the plane models (`CreateHorizontalPlaneModel` 0x10281f, `CreateStandingPlaneModel` 0x102b1c, their releases).
//! The C++ keeps the vertex and polygon arrays (`batch_quad` sets a quad's four vertices, `batch_draw` draws a batch).
//!
//! Drawn only (the renderer's thread): nothing here is simulation state.

use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};
use std::sync::Mutex;

use ee_base::math::types::{Ge3DPoint, GeTransformation};

use crate::gfx::effects::{self, EeGfxColor, EeGfxEffectFog, EeGfxEffectQuad, EeGfxTexCoord};
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::raster::{
    EeGfxModel, Raster, BLEND_INV_SRC_ALPHA, BLEND_ONE, BLEND_SRC_ALPHA, EE_GFX_SPRITE_GLOW, EE_GFX_SPRITE_GROUND,
    ZBUFFER_TEST_ONLY,
};

/// The batches `batch_quad` and `batch_draw` name: the camera batch, the ground batch.
pub const EE_GFX_BATCH_CAMERA: u32 = 0;
pub const EE_GFX_BATCH_GROUND: u32 = 1;
/// A batch's vertices (0x200 quads: its polygon array holds two triangles a quad).
pub const EE_GFX_BATCH_VERTICES: u32 = 0x800;

/// `kCullNone`.
const CULL_NONE: u32 = 0;
/// `SetShadingColorSource(1)`.
const SHADING_VERTEX: u32 = 1;

/// One batch's state (the original's static: zero until the first `begin`).
#[derive(Clone, Copy, Debug, PartialEq)]
struct Batch {
    /// `GETexture*` (referenced by `begin`)
    texture: usize,
    alpha_ref: f32,
    count: u32,
}

const EMPTY: Batch = Batch { texture: 0, alpha_ref: 0.0, count: 0 };
static BATCHES: Mutex<[Batch; 2]> = Mutex::new([EMPTY; 2]);

fn get(which: u32) -> Batch {
    BATCHES.lock().unwrap_or_else(|e| e.into_inner())[which as usize]
}

fn with(which: u32, f: impl FnOnce(&mut Batch)) {
    f(&mut BATCHES.lock().unwrap_or_else(|e| e.into_inner())[which as usize])
}

/// `BeginEffectBatches` (0x102724): both batches empty with `texture`, referenced once for each.
pub fn begin<R: Raster + ?Sized>(x: &mut R, texture: EeGfxModel) {
    with(EE_GFX_BATCH_CAMERA, |b| b.texture = texture as usize);
    with(EE_GFX_BATCH_GROUND, |b| b.texture = texture as usize);
    if !texture.is_null() {
        x.texture_reference(texture);
        x.texture_reference(texture);
    }
    for which in [EE_GFX_BATCH_CAMERA, EE_GFX_BATCH_GROUND] {
        with(which, |b| b.count = 0);
    }
    for which in [EE_GFX_BATCH_CAMERA, EE_GFX_BATCH_GROUND] {
        with(which, |b| b.alpha_ref = 1.0);
    }
}

/// `EndEffectBatches` (0x102f20): both references dropped through the camera batch's texture.
pub fn end<R: Raster + ?Sized>(x: &mut R) {
    let texture = get(EE_GFX_BATCH_CAMERA).texture as EeGfxModel;
    if !texture.is_null() {
        x.texture_dereference(texture);
        x.texture_dereference(texture);
    }
}

/// `FlushEffectBatch` (0x103db7, the camera batch: additive, no fog, no culling, z test only, white light) and
/// `FlushEffectGroundBatch` (0x103ea6: alpha-blended, prelit), each then empty.
pub fn flush<R: Raster + ?Sized>(x: &mut R, which: u32) {
    let b = get(which);
    let culling = x.get_culling();
    x.set_culling(CULL_NONE);
    let camera = which == EE_GFX_BATCH_CAMERA;
    let fog = if camera {
        let fog = x.get_fog();
        x.set_fog(0);
        fog
    } else {
        0
    };
    x.set_alpha_ref(b.alpha_ref);
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_shading_source(SHADING_VERTEX);
    if camera {
        x.set_blending(BLEND_ONE, BLEND_ONE);
        x.lighting(EeGfxLighting::White);
    } else {
        x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
        x.lighting(EeGfxLighting::Prelit);
    }
    x.batch_draw(which, b.count, b.texture as EeGfxModel);
    if camera {
        x.set_fog(fog);
    }
    x.set_culling(culling);
    with(which, |b| {
        b.count = 0;
        b.alpha_ref = 1.0;
    });
}

/// `FlushEffectBatches` (0x103f64): what the ground batch and then the camera batch still hold.
pub fn flush_all<R: Raster + ?Sized>(x: &mut R) {
    if get(EE_GFX_BATCH_GROUND).count != 0 {
        flush(x, EE_GFX_BATCH_GROUND);
    }
    if get(EE_GFX_BATCH_CAMERA).count != 0 {
        flush(x, EE_GFX_BATCH_CAMERA);
    }
}

/// One quad into a batch: drawn first when full, its alpha reference kept when smaller, its four vertices set.
fn add<R: Raster + ?Sized>(x: &mut R, which: u32, alpha_ref: f32, quad: impl FnOnce(&mut R) -> EeGfxEffectQuad) {
    let mut index = get(which).count;
    if index == EE_GFX_BATCH_VERTICES {
        flush(x, which);
        index = 0;
    }
    with(which, |b| {
        if alpha_ref < b.alpha_ref {
            b.alpha_ref = alpha_ref;
        }
    });
    let quad = quad(x);
    x.batch_quad(which, &quad, index);
    with(which, |b| b.count += 4);
}

/// `GetEffectFogFactor` (0x1030c8) under the rasterizer's fog state `fog`: its attributes read only when linear.
pub fn fog_factor<R: Raster + ?Sized>(x: &mut R, point: &Ge3DPoint, fog: u32) -> f32 {
    let mut state = EeGfxEffectFog { state: 0, start: 0.0, end: 0.0, scale: 0.0 };
    if fog == 1 {
        let a = x.get_fog_attributes();
        state = EeGfxEffectFog { state: 1, start: a.start, end: a.end, scale: a.scale };
    }
    let inverse: GeTransformation = x.camera_inverse();
    effects::fog_factor(*point, &state, &inverse)
}

/// `AddGlowSprite` (0x10317d), `AddGroundSprite` (0x1033f1) and `AddBlendedSprite` (0x10387b): the raster table's
/// `sprite` by its kind.
#[allow(clippy::too_many_arguments)]
pub fn sprite<R: Raster + ?Sized>(
    x: &mut R,
    kind: u32,
    position: &Ge3DPoint,
    color: EeGfxColor,
    rotation: f32,
    size: f32,
    alpha: f32,
    brightness: f32,
    alpha_ref: f32,
) {
    let p = *position;
    if kind == EE_GFX_SPRITE_GLOW {
        let state = x.get_fog();
        let fog = fog_factor(x, position, state);
        add(x, EE_GFX_BATCH_CAMERA, alpha_ref, |x| {
            let (yaw, pitch) = (x.camera_yaw(), x.camera_pitch());
            effects::glow_sprite(p, color, rotation, size, alpha, brightness, fog, yaw, pitch)
        });
    } else if kind == EE_GFX_SPRITE_GROUND {
        add(x, EE_GFX_BATCH_GROUND, alpha_ref, |_| {
            effects::ground_sprite(p, color, rotation, size, [alpha; 4], brightness)
        });
    } else {
        add(x, EE_GFX_BATCH_GROUND, alpha_ref, |x| {
            let (yaw, pitch) = (x.camera_yaw(), x.camera_pitch());
            effects::blended_sprite(p, color, rotation, size, alpha, brightness, yaw, pitch)
        });
    }
}

/// `AddGroundQuad` (0x103634): a ground quad with an opacity a corner.
#[allow(clippy::too_many_arguments)]
pub fn ground_quad<R: Raster + ?Sized>(
    x: &mut R,
    center: &Ge3DPoint,
    color: EeGfxColor,
    yaw: f32,
    size: f32,
    heights: &[f32; 4],
    brightness: f32,
    alpha_ref: f32,
) {
    let (c, h) = (*center, *heights);
    add(x, EE_GFX_BATCH_GROUND, alpha_ref, |_| effects::ground_sprite(c, color, yaw, size, h, brightness));
}

/// `AddGroundStrip` (0x103ac8): an upright strip between two points.
#[allow(clippy::too_many_arguments)]
pub fn ground_strip<R: Raster + ?Sized>(
    x: &mut R,
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
    let (f, t) = (*from, *to);
    let origin = EeGfxTexCoord { u: offset.0, v: offset.1 };
    let extent = EeGfxTexCoord { u: size.0, v: size.1 };
    add(x, EE_GFX_BATCH_GROUND, alpha_ref, |_| {
        effects::ground_strip(f, t, width, color, origin, extent, alpha, brightness)
    });
}

/// The shared plane models: [0] on the ground facing up (0x4e8df4), [1] standing facing -y (0x4e8df8, the 2D
/// effects' quad), each referenced.
static PLANES: [AtomicPtr<c_void>; 2] = [AtomicPtr::new(std::ptr::null_mut()), AtomicPtr::new(std::ptr::null_mut())];

/// A shared plane model (null before `create_planes`).
pub fn plane_model(standing: bool) -> EeGfxModel {
    PLANES[standing as usize].load(Ordering::Acquire)
}

/// `CreatePlaneModels` (0x1030ab): the horizontal plane, then the standing one, each replacing (dereferencing) the
/// last: a model of two polygons and four vertices made, referenced, its mesh stored, material 0 over all of it, its
/// bounding volumes.
pub fn create_planes<R: Raster + ?Sized>(x: &mut R) {
    for standing in [false, true] {
        let old = PLANES[standing as usize].swap(std::ptr::null_mut(), Ordering::AcqRel);
        if !old.is_null() {
            x.model_dereference(old);
        }
        let model = x.create_model(2, 4, 0, "EEEffect::sHorizontalPlane", 1);
        x.model_reference(model);
        let (vertices, triangles) = effects::plane_arrays(standing);
        x.store_mesh(model, &vertices, &triangles);
        x.material_ranges(model, 2, 4);
        x.model_bounds(model);
        PLANES[standing as usize].store(model, Ordering::Release);
    }
}

/// `ReleasePlaneModels` (0x104275): the horizontal plane, then the standing one, dereferenced.
pub fn release_planes<R: Raster + ?Sized>(x: &mut R) {
    for standing in [false, true] {
        let old = PLANES[standing as usize].swap(std::ptr::null_mut(), Ordering::AcqRel);
        if !old.is_null() {
            x.model_dereference(old);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::gfx::raster::{LogRaster, EE_GFX_SPRITE_BLENDED};

    /// The batches and planes are the process's: one test at a time.
    static LOCK: Mutex<()> = Mutex::new(());

    const WHITE: EeGfxColor = EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 };

    #[test]
    fn a_full_batch_is_drawn_before_its_next_quad() {
        let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
        let mut x = LogRaster::default();
        begin(&mut x, 7 as EeGfxModel);
        let p = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
        for i in 0..0x200 {
            sprite(&mut x, EE_GFX_SPRITE_GROUND, &p, WHITE, 0.0, 1.0, 1.0, 1.0, if i == 5 { 0.25 } else { 0.5 });
        }
        assert_eq!(get(EE_GFX_BATCH_GROUND).count, 0x800);
        assert!(!x.calls.iter().any(|c| c.starts_with("batch draw")));
        sprite(&mut x, EE_GFX_SPRITE_BLENDED, &p, WHITE, 0.0, 1.0, 1.0, 1.0, 0.75);
        let draw = x.calls.iter().position(|c| c.starts_with("batch draw")).unwrap();
        assert_eq!(x.calls[draw], "batch draw 1 2048 7");
        assert_eq!(x.calls[draw - 5], "alpharef 0.25");
        assert_eq!(x.calls[draw + 1..draw + 3], ["culling 0", "batch quad 1 0"]);
        assert_eq!(get(EE_GFX_BATCH_GROUND), Batch { texture: 7, alpha_ref: 0.75, count: 4 });
        flush_all(&mut x);
        end(&mut x);
        let tail: Vec<&str> = x.calls.iter().rev().take(2).map(|s| s.as_str()).collect();
        assert_eq!(tail, ["texture dereference 7", "texture dereference 7"]);
        assert_eq!(x.calls.iter().filter(|c| *c == "texture reference 7").count(), 2);
    }

    #[test]
    fn the_camera_batch_draws_additive_without_fog() {
        let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
        let mut x = LogRaster { fog: 1, culling: 2, ..LogRaster::default() };
        x.fog_attributes.start = 100.0;
        begin(&mut x, std::ptr::null_mut());
        sprite(&mut x, EE_GFX_SPRITE_GLOW, &Ge3DPoint::default(), WHITE, 0.0, 1.0, 1.0, 1.0, 0.5);
        x.calls.clear();
        flush_all(&mut x);
        assert_eq!(
            x.calls,
            [
                "culling 0",
                "fog 0",
                "alpharef 0.5",
                "zbuffer 2",
                "shading 1",
                "blend 1 1",
                "lighting White",
                "batch draw 0 4 0",
                "fog 1",
                "culling 2"
            ]
        );
        assert_eq!(get(EE_GFX_BATCH_CAMERA).count, 0);
        end(&mut x);
        assert!(!x.calls.iter().any(|c| c.starts_with("texture")));
    }

    #[test]
    fn the_glow_fades_in_linear_fog() {
        let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
        let mut x = LogRaster::default();
        x.fog_attributes.start = 10.0;
        x.fog_attributes.end = 20.0;
        x.fog_attributes.scale = 0.1;
        let at = Ge3DPoint { x: 0.0, y: 15.0, z: 0.0 };
        assert_eq!(fog_factor(&mut x, &at, 1), 0.5);
        assert_eq!(fog_factor(&mut x, &at, 0), 1.0);
    }

    #[test]
    fn the_planes_replace_and_release_their_models() {
        let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
        let mut x = LogRaster::default();
        create_planes(&mut x);
        let (ground, standing) = (plane_model(false), plane_model(true));
        assert_eq!((ground as usize, standing as usize), (1, 2));
        assert_eq!(x.model(ground).points[0], Ge3DPoint { x: -0.5, y: 0.5, z: 0.0 });
        assert_eq!(x.model(standing).triangles[1], [2, 1, 3]);
        create_planes(&mut x);
        release_planes(&mut x);
        assert!(plane_model(false).is_null() && plane_model(true).is_null());
        let derefs: Vec<&String> = x.calls.iter().filter(|c| c.starts_with("dereference")).collect();
        assert_eq!(derefs, ["dereference 1", "dereference 2", "dereference 3", "dereference 4"]);
    }

    #[test]
    fn a_strip_and_a_quad_go_to_the_ground_batch() {
        let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
        let mut x = LogRaster::default();
        begin(&mut x, std::ptr::null_mut());
        let p = Ge3DPoint::default();
        ground_quad(&mut x, &p, WHITE, 0.0, 1.0, &[1.0; 4], 1.0, 0.5);
        ground_strip(&mut x, &p, &p, 1.0, WHITE, (0.0, 0.0), (1.0, 1.0), 1.0, 1.0, 0.25);
        assert_eq!(get(EE_GFX_BATCH_GROUND), Batch { texture: 0, alpha_ref: 0.25, count: 8 });
        assert_eq!(x.calls, ["batch quad 1 0", "batch quad 1 4"]);
        flush_all(&mut x);
    }
}
