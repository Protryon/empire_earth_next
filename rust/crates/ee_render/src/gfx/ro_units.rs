//! The units' render object (`src/Empire Earth/Graphics Thread/EEROUnits.cpp`, vtable 0x441bbc): its media
//! (`LoadMedia` 0x516a97, `ReleaseMedia` 0x5185b5: the line and command textures, the area-effect ring and the three
//! selection rings), the scene pass (`RenderScene`, 0x518203: the object and unit selection rings and area-effect
//! rings, the map objects' and units' models, the buildings being placed, the air units' rings and the bars) and the
//! labels (`FrameDone`, 0x51852f's `DrawLabels`). The C++ hands the frame's render lists over as arrays of records
//! (`EeGfxUnitsScene`); the rhombi, the line batch and the labels' text are the raster table's.
//!
//! Drawn only (the renderer's thread).

use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

use crate::ge::viewport::GeViewport;
use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{Ge3DPoint, GeTransformation};

use crate::gfx::effects::EeGfxColor;
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::mesh::{
    EeGfxMeshTriangle, EeGfxMeshVertex, EE_GFX_MESH_COLOR, EE_GFX_MESH_COORD, EE_GFX_MESH_OPACITY, EE_GFX_MESH_POINT,
};
use crate::gfx::models::{area_effect_ring, selection_model, selection_segments};
use crate::gfx::raster::{
    EeGfxModel, EeGfxScreenLine, EeGfxTextureLoad, Raster, BLEND_INV_SRC_ALPHA, BLEND_ONE, BLEND_SRC_ALPHA, BLEND_ZERO,
    COMPARE_LESS, COMPARE_LESS_EQUAL, CULL_CW, STAGE_OP_MODULATE, ZBUFFER_TEST_AND_WRITE, ZBUFFER_TEST_ONLY,
};
use crate::gfx::units::{
    bar_origin, bar_sizes, label_clip, label_point, object_bar, object_bar_point, placement_lines, ring_segments,
    screen_point, selection_ring, unit_bar, unit_bar_point, EeGfxBarRects, EeGfxBarSizes, EeGfxPlacementLine,
};

const fn c(red: f32, green: f32, blue: f32) -> EeGfxColor {
    EeGfxColor { red, green, blue }
}
/// The rings' and bars' colours (0x5294c0..0x529418).
const RING_HIGHLIGHT_COLOR: EeGfxColor = c(0.537, 0.678, 1.0);
const RING_COLOR: EeGfxColor = c(0.0, 1.0, 0.0);
const SELECTION_LIGHT: EeGfxColor = c(1.0, 0.75, 0.5);
const SELECTION_MIDDLE: EeGfxColor = c(1.0, 0.56, 0.25);
const SELECTION_DARK: EeGfxColor = c(1.0, 0.375, 0.0);
const BAR_COLOR: EeGfxColor = c(0.0, 1.0, 0.0);
const BAR_BACK_COLOR: EeGfxColor = c(1.0, 0.0, 0.0);
const BAR_BORDER_COLOR: EeGfxColor = c(0.9, 0.9, 0.9);
const HEALTH_COLOR: EeGfxColor = c(0.0, 0.0, 0.7);
const HEALTH_BACK_COLOR: EeGfxColor = c(1.0, 1.0, 1.0);
const WHITE: EeGfxColor = c(1.0, 1.0, 1.0);
/// The unit's constants (0x441ba0..0x441bac): the tint's fog band, the least alpha reference, the opacity above which
/// a model writes z, the translucent ambient scale.
const FOG_DEPTH: f32 = 500.0;
const MIN_ALPHA_REF: f32 = 0.03;
const Z_WRITE_OPACITY: f32 = 0.6;
const TRANSLUCENT_AMBIENT_SCALE: f32 = 0.59;
/// The map objects face the camera turned by this (pi / 4).
const OBJECT_TURN: f32 = 0.7853982;
/// `kUnitCategoryAmbient`, `kUnitCategoryResource`: no collision sphere.
const CATEGORY_AMBIENT: i32 = 0;
const CATEGORY_RESOURCE: i32 = 1;
/// `kCullNone`; `kMipFilterNone`; `kAddressingWrap`.
const CULL_NONE: u32 = 0;
const MIP_FILTER_NONE: u32 = 0;
const ADDRESSING_WRAP: u32 = 2;
/// A line batch's size (`sLines`, `new GELine[0x40]`) and the most edges a placement has.
const LINE_BATCH: usize = 0x40;
const PLACEMENT_LINES: usize = 320;

/// One render unit (`EERenderUnit`) with what its passes read from its graphic objects and class.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGfxRenderUnit {
    /// `mTransformation`
    pub transformation: GeTransformation,
    /// `mPlayerColor`, `mTintColor`
    pub player_color: EeGfxColor,
    pub tint_color: EeGfxColor,
    /// `mSource` (`EEGraphicObject*`), its model and texture of `mAnimation`, that model's centre, its bar tag,
    /// `mIsDrawn`, `mAlphaReference` and `mOpacity`
    pub object: EeGfxModel,
    pub model: EeGfxModel,
    pub texture: EeGfxModel,
    pub center: Ge3DPoint,
    pub bar: Ge3DPoint,
    pub drawn: bool,
    pub alpha_ref: f32,
    pub object_opacity: f32,
    /// `mSource2` (null: none), its `mModel`, `mTexture`, `mIsDrawn`
    pub object2: EeGfxModel,
    pub model2: EeGfxModel,
    pub texture2: EeGfxModel,
    pub drawn2: bool,
    /// `mHealthBars`
    pub health_bars: *const f32,
    pub health_bar_count: usize,
    /// `mTurretAngles` with `mTurretModels`, `mOrbitalAngles` with `mOrbitalModels`, `mWheelAngles` with `mWheelModels`
    pub turret_angles: *const f32,
    pub turret_models: *const EeGfxModel,
    pub turret_count: usize,
    pub orbital_angles: *const f32,
    pub orbital_models: *const EeGfxModel,
    pub orbital_count: usize,
    pub wheel_angles: *const f32,
    pub wheel_models: *const EeGfxModel,
    pub wheel_count: usize,
    /// `mName` (NUL-terminated UTF-8)
    pub name: *const u8,
    pub scale: f32,
    pub selection_size: f32,
    pub opacity: f32,
    pub health: f32,
    pub fraction: f32,
    pub sub_model_offset_scale: f32,
    pub fog_scale: f32,
    /// `mInfo->GetInnerCollisionSphere()`
    pub inner_collision_sphere: f32,
    pub category: i32,
    pub command_type: i32,
    pub animation: i32,
    pub current_frame: u32,
    pub next_frame: u32,
    pub area_effect_strength: u32,
    /// `mInfo->mDomains & kMovementAir`
    pub air: bool,
    /// `mUnitClassIndex == kUnitTypeWaypoint`
    pub waypoint: bool,
    pub fogged: bool,
    pub tinted: bool,
    pub has_sub_model_offset: bool,
    pub second_full_opacity: bool,
}

/// A building being placed (`EERenderUnitRecord`).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGfxGhostRecord {
    pub unit: EeGfxRenderUnit,
    /// `mHeights[x][y]`, `mCells[x][y]`
    pub heights: [[f32; 12]; 12],
    pub cells: [[u8; 12]; 12],
    pub tile_x: i32,
    pub tile_y: i32,
    /// `mUnit.mGridSize`, `mUnit.mCanPlace`, `mUnit.mInfo->IsLandBuilding()`
    pub grid_size: i32,
    pub can_place: bool,
    pub land_building: bool,
}

/// One placed map object (`EERenderShadow`) with its graphic object's model, texture, bar tag, alpha reference and
/// opacity.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGfxRenderObject {
    pub location: Ge3DPoint,
    pub model: EeGfxModel,
    pub texture: EeGfxModel,
    pub bar: Ge3DPoint,
    pub alpha_ref: f32,
    pub object_opacity: f32,
    pub model_scale: f32,
    pub selection_size: f32,
    pub health: f32,
    pub opacity: f32,
    pub fraction: f32,
    pub current_frame: u32,
    pub next_frame: u32,
    pub fogged: bool,
}

/// The frame's lists (indices into `units` and `objects`) and settings.
#[repr(C)]
pub struct EeGfxUnitsScene {
    pub context: *mut c_void,
    /// the main viewport (`gU4E8F08`): read at each use, the passes change its depth range
    pub viewport: *const GeViewport,
    /// `gOrthographicView`, `gRenderSnapshot.mInCinematic`, `gShowCollisionSpheres`, `gInterpolateFrames`,
    /// `gModelDetail`, `gFreeCamera`, `gUnitPanelActive`
    pub orthographic: bool,
    pub cinematic: bool,
    pub collision_spheres: bool,
    pub interpolate_frames: bool,
    pub model_detail: i32,
    pub free_camera: bool,
    pub panel_active: bool,
    /// `gU4E9330` (the lines' texture phase), `gRenderEnvironment.mCameraYaw`
    pub grid_phase: f32,
    pub camera_yaw: f32,
    /// the game window's size
    pub window_width: u32,
    pub window_height: u32,
    /// `mRenderUnits[0..mRenderUnitCount]`, `mRenderShadows[0..mRenderShadowCount]`
    pub units: *const EeGfxRenderUnit,
    pub unit_count: usize,
    pub objects: *const EeGfxRenderObject,
    pub object_count: usize,
    /// `mUnits0`, `mUnits1`, `mSelectedUnits`, `mShadows0`, `mShadows1`, `mSelectedObjects` in their order
    pub units0: *const u32,
    pub units0_count: usize,
    pub units1: *const u32,
    pub units1_count: usize,
    pub selected_units: *const u32,
    pub selected_unit_count: usize,
    pub objects0: *const u32,
    pub objects0_count: usize,
    pub objects1: *const u32,
    pub objects1_count: usize,
    pub selected_objects: *const u32,
    pub selected_object_count: usize,
    /// `mSelectedUnit`, `mSelectedObject` (-1: none)
    pub selected_unit: i32,
    pub selected_object: i32,
    /// `mGhostRecords`
    pub ghosts: *const EeGfxGhostRecord,
    pub ghost_count: usize,
    /// the unit with the debug label (`gU529044`; -1: none) and that label (`gU529030`)
    pub debug_unit: i32,
    pub debug_text: *const u8,
    /// the effects of render queue 3, batched per texture (`DrawPass5`)
    pub draw_effects: extern "C" fn(context: *mut c_void),
}

/// The media (0x5294e0..0x529504).
const COMMAND0: usize = 0;
const COMMAND1: usize = 1;
const COMMAND2: usize = 2;
const LINE_TEXTURE: usize = 3;
const SELECTION_TEXTURE: usize = 4;
const AREA_TEXTURE: usize = 5;
const RING_MODEL: usize = 6;
const SELECTION0: usize = 7;
#[allow(clippy::declare_interior_mutable_const)]
const NONE: AtomicPtr<c_void> = AtomicPtr::new(std::ptr::null_mut());
static MEDIA: [AtomicPtr<c_void>; 10] = [NONE; 10];

fn get(i: usize) -> EeGfxModel {
    MEDIA[i].load(Ordering::Relaxed)
}
fn set(i: usize, m: EeGfxModel) {
    MEDIA[i].store(m, Ordering::Relaxed)
}

/// One of the command confirmation textures (0 unit, 1 path, 2 click; `sTexture4E0`..`sTexture4E8`).
pub fn command_texture(i: u32) -> EeGfxModel {
    get(COMMAND0 + (i as usize).min(2))
}

/// The placement lines' texture (`sDottedLineTexture`).
pub fn line_texture() -> EeGfxModel {
    get(LINE_TEXTURE)
}

fn vertex(set: u32) -> EeGfxMeshVertex {
    EeGfxMeshVertex { set, ..EeGfxMeshVertex::default() }
}

/// `LoadMedia` (0x516a97) but the rhombi and the line batch (the C++'s): the line texture, the command textures, the
/// selection and area-effect textures, the area-effect ring and the three selection rings at `detail`.
pub fn load_media<R: Raster>(x: &mut R, detail: i32) {
    let dotted = EeGfxTextureLoad {
        has_alpha: false,
        mip_maps: false,
        min_filter: 0,
        mag_filter: 0,
        mip_filter: MIP_FILTER_NONE,
        mip_bias: 0.0,
        reference: true,
        ..EeGfxTextureLoad::effect(ADDRESSING_WRAP)
    };
    set(LINE_TEXTURE, x.load_texture("textures\\DottedLine", &dotted));
    let load = EeGfxTextureLoad { reference: true, ..EeGfxTextureLoad::effect(ADDRESSING_WRAP) };
    set(COMMAND0, x.load_texture("textures\\UnitCommandConfirmation", &load));
    set(COMMAND1, x.load_texture("textures\\PathCommandConfirmation", &load));
    set(COMMAND2, x.load_texture("textures\\ClickCommandConfirmation", &load));
    let selection = x.load_texture("textures\\UnitSelection", &load);
    let area = x.load_texture("textures\\UnitAreaEffectCircle", &load);
    set(SELECTION_TEXTURE, selection);
    set(AREA_TEXTURE, area);
    debug_assert_eq!(load.alpha_mode, STAGE_OP_MODULATE);
    let ring = x.create_model(0xa, 0x1a, 0, "area effect", 1);
    let r = area_effect_ring();
    let vertices: Vec<_> = (0..0x1a)
        .map(|i| EeGfxMeshVertex {
            point: r.points[i],
            coord: r.coords[i],
            color: RING_HIGHLIGHT_COLOR,
            opacity: 1.0,
            ..vertex(EE_GFX_MESH_POINT | EE_GFX_MESH_COORD | EE_GFX_MESH_COLOR | EE_GFX_MESH_OPACITY)
        })
        .collect();
    let triangles: Vec<_> = r.indices.chunks(3).map(|t| EeGfxMeshTriangle { set: 1, v: [t[0], t[1], t[2]] }).collect();
    x.store_mesh(ring, &vertices, &triangles);
    x.material_ranges(ring, 0xa, 0x1a);
    x.model_texture(ring, area);
    x.model_normals(ring);
    x.model_bounds(ring);
    set(RING_MODEL, ring);
    let segments = selection_segments(detail);
    let colors = [SELECTION_LIGHT, SELECTION_MIDDLE, SELECTION_DARK];
    for k in 0..3 {
        let model = x.create_model(segments * 2, segments * 2, 0, "unit selection", 1);
        let s = selection_model(detail, k);
        let n = s.count as usize;
        let vertices: Vec<_> = (0..n)
            .map(|i| EeGfxMeshVertex {
                point: s.points[i],
                coord: s.coords[i],
                color: colors[s.colors[i] as usize],
                opacity: 1.0,
                ..vertex(EE_GFX_MESH_POINT | EE_GFX_MESH_COORD | EE_GFX_MESH_COLOR | EE_GFX_MESH_OPACITY)
            })
            .collect();
        let triangles: Vec<_> =
            s.indices[..n * 3].chunks(3).map(|t| EeGfxMeshTriangle { set: 1, v: [t[0], t[1], t[2]] }).collect();
        x.store_mesh(model, &vertices, &triangles);
        x.material_ranges(model, segments * 2, segments * 2);
        x.model_texture(model, selection);
        x.model_normals(model);
        x.model_bounds(model);
        set(SELECTION0 + k, model);
    }
}

/// `ReleaseMedia` (0x5185b5) but the rhombi and the line batch.
pub fn release_media<R: Raster>(x: &mut R) {
    for i in [SELECTION_TEXTURE, LINE_TEXTURE, AREA_TEXTURE, COMMAND0, COMMAND1, COMMAND2, RING_MODEL, SELECTION0, 8, 9]
    {
        let m = MEDIA[i].swap(std::ptr::null_mut(), Ordering::Relaxed);
        if m.is_null() {
            continue;
        }
        if i < RING_MODEL {
            x.texture_dereference(m);
        } else {
            x.model_dereference(m);
        }
    }
}

/// The scene's arrays as slices.
struct Lists<'a> {
    units: &'a [EeGfxRenderUnit],
    objects: &'a [EeGfxRenderObject],
    units0: &'a [u32],
    units1: &'a [u32],
    selected_units: &'a [u32],
    objects0: &'a [u32],
    objects1: &'a [u32],
    selected_objects: &'a [u32],
    ghosts: &'a [EeGfxGhostRecord],
}

/// A C++ array as a slice (empty for a null pointer).
///
/// # Safety
/// `p` holds `n` values while the slice lives.
unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the function's contract
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

impl EeGfxUnitsScene {
    /// # Safety
    /// The scene's arrays hold their counts.
    unsafe fn lists(&self) -> Lists<'_> {
        // SAFETY: the function's contract
        unsafe {
            Lists {
                units: slice(self.units, self.unit_count),
                objects: slice(self.objects, self.object_count),
                units0: slice(self.units0, self.units0_count),
                units1: slice(self.units1, self.units1_count),
                selected_units: slice(self.selected_units, self.selected_unit_count),
                objects0: slice(self.objects0, self.objects0_count),
                objects1: slice(self.objects1, self.objects1_count),
                selected_objects: slice(self.selected_objects, self.selected_object_count),
                ghosts: slice(self.ghosts, self.ghost_count),
            }
        }
    }
    /// The main viewport as it is now (a copy).
    fn viewport(&self) -> GeViewport {
        // SAFETY: the scene's viewport is the C++'s global, readable on the renderer's thread
        unsafe { *self.viewport }
    }
}

fn position(t: &GeTransformation) -> Ge3DPoint {
    Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] }
}

fn turned(yaw: f32, pitch: f32) -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    t.set_orientation(RotationOrder::Ypr, yaw, pitch, 0.0);
    t
}

/// A selection ring model drawn at `t` scaled by `scale`.
fn draw_ring<R: Raster>(x: &mut R, model: EeGfxModel, t: &GeTransformation, scale: f32) {
    x.model_transform(model, t);
    x.model_scale(model, scale);
    x.model_prepare(model);
    x.model_draw(model, 1.0);
}

/// `DrawSelectionRings` (0x514526): the small ring under each selected aircraft.
fn draw_selection_rings<R: Raster>(x: &mut R, l: &Lists) {
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_culling(CULL_NONE);
    x.lighting(EeGfxLighting::White);
    x.set_alpha_ref(0.0);
    for &i in l.selected_units {
        let u = &l.units[i as usize];
        if !(u.selection_size > 0.0) || !u.air {
            continue;
        }
        draw_ring(x, get(SELECTION0), &u.transformation, u.selection_size);
    }
}

/// `DrawRings` (0x51461b): each ringed unit's area-effect ring, one segment in the ring colour per strength point
/// and the rest in the highlight colour, turned to the camera.
fn draw_rings<R: Raster>(x: &mut R, l: &Lists, camera_yaw: f32) {
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    x.set_culling(CULL_NONE);
    x.lighting(EeGfxLighting::White);
    let mut t = turned(camera_yaw, 0.0);
    let ring = get(RING_MODEL);
    for &i in l.units0 {
        let u = &l.units[i as usize];
        if u.area_effect_strength > 0 && u.selection_size > 0.0 {
            let p = position(&u.transformation);
            t.m[0][3] = p.x;
            t.m[1][3] = p.y;
            t.m[2][3] = p.z;
            let s = ring_segments(u.area_effect_strength);
            let colored = |color| EeGfxMeshVertex { color, ..vertex(EE_GFX_MESH_COLOR) };
            let mut vertices = vec![colored(RING_COLOR); s.ring as usize * 3];
            vertices.extend(std::iter::repeat_n(colored(RING_HIGHLIGHT_COLOR), s.highlight as usize * 3));
            x.store_mesh(ring, &vertices, &[]);
            draw_ring(x, ring, &t, u.selection_size);
        }
    }
}

/// `DrawSelection` (0x515488): the ring (small, medium or large) under each selected ground unit, then with the
/// collision spheres on each unit's inner sphere.
fn draw_selection<R: Raster>(x: &mut R, l: &Lists, spheres: bool) {
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    x.set_culling(CULL_NONE);
    x.lighting(EeGfxLighting::White);
    x.set_alpha_ref(0.0);
    for &i in l.selected_units {
        let u = &l.units[i as usize];
        if !(u.selection_size > 0.0) || u.air {
            continue;
        }
        let r = selection_ring(u.selection_size);
        draw_ring(x, get(SELECTION0 + r.model as usize), &u.transformation, r.scale);
    }
    if !spheres {
        return;
    }
    for &i in l.units0 {
        let u = &l.units[i as usize];
        if u.category == CATEGORY_AMBIENT || u.category == CATEGORY_RESOURCE || u.animation == 5 || u.animation == 4 {
            continue;
        }
        if !(u.inner_collision_sphere > 0.0) {
            continue;
        }
        draw_ring(x, get(SELECTION0), &u.transformation, u.inner_collision_sphere);
    }
}

/// `DrawObjectSelection` (0x515b63): the ring at each selected object with a ring size.
fn draw_object_selection<R: Raster>(x: &mut R, l: &Lists) {
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    x.set_culling(CULL_NONE);
    x.lighting(EeGfxLighting::White);
    let mut t = GeTransformation::IDENTITY;
    for &i in l.selected_objects {
        let o = &l.objects[i as usize];
        t.m[0][3] = o.location.x;
        t.m[1][3] = o.location.y;
        t.m[2][3] = o.location.z;
        if o.selection_size > 0.0 {
            draw_ring(x, get(SELECTION0), &t, o.selection_size);
        }
    }
}

/// `DrawObjectModel` (0x515674): one map object at its frame, scale and place, alpha tested and z written by its
/// opacity.
fn draw_object_model<R: Raster>(x: &mut R, t: &mut GeTransformation, o: &EeGfxRenderObject) {
    if !(o.model_scale > 0.0) {
        return;
    }
    x.set_alpha_ref(if o.alpha_ref > o.opacity { MIN_ALPHA_REF } else { o.alpha_ref });
    x.set_zbuffer(if o.opacity > Z_WRITE_OPACITY { ZBUFFER_TEST_AND_WRITE } else { ZBUFFER_TEST_ONLY });
    x.model_opacity(o.model, o.opacity);
    x.model_own_texture(o.model, o.texture);
    x.model_frame(o.model, o.current_frame);
    x.model_tween(o.model, o.next_frame, o.fraction);
    x.model_scale(o.model, o.model_scale);
    t.m[0][3] = o.location.x;
    t.m[1][3] = o.location.y;
    t.m[2][3] = o.location.z;
    x.model_transform(o.model, t);
    x.model_prepare(o.model);
    x.model_draw(o.model, o.object_opacity);
}

/// `DrawObjectModels` (0x515a50): the map objects facing the camera, lit as fogged or not; translucent ones added
/// when the card cannot modulate texture alpha.
fn draw_object_models<R: Raster>(x: &mut R, l: &Lists, list: &[u32], camera_yaw: f32) {
    let modulate = x.can_modulate_alpha();
    x.lighting(EeGfxLighting::MapObject);
    let mut t = turned(camera_yaw - OBJECT_TURN, 0.0);
    for &i in list {
        let o = &l.objects[i as usize];
        x.lighting(if o.fogged { EeGfxLighting::DimUnit } else { EeGfxLighting::RestoreUnit });
        if modulate {
            x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
        } else if o.opacity < 1.0 {
            x.set_ambient_intensity(TRANSLUCENT_AMBIENT_SCALE * o.opacity);
            x.set_blending(BLEND_ONE, BLEND_ONE);
        }
        draw_object_model(x, &mut t, o);
    }
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
}

/// The fog band in front of a unit's centre (`DrawPlacement`'s red fog, `DrawUnitModels`' tint): the fog saved,
/// turned on in `color` from `scale` fog depths before the centre; the saved fog to put back.
fn fog_band<R: Raster>(
    x: &mut R,
    s: &EeGfxUnitsScene,
    u: &EeGfxRenderUnit,
    color: EeGfxColor,
    scale: f32,
) -> (u32, crate::gfx::raster::EeGfxFogAttributes) {
    let fog = x.get_fog();
    let attributes = x.get_fog_attributes();
    let center = u.transformation.apply_point(u.center);
    let camera = s.viewport().camera_inverse.apply_point(center);
    let start = camera.y - FOG_DEPTH * scale;
    let end = FOG_DEPTH + start;
    x.set_fog(1);
    x.set_fog_color(color);
    x.set_fog_range(start, end);
    (fog, attributes)
}

fn restore_fog<R: Raster>(x: &mut R, saved: (u32, crate::gfx::raster::EeGfxFogAttributes)) {
    x.set_fog(saved.0);
    x.set_fog_type(saved.1.kind);
    x.set_fog_color(saved.1.color);
    x.set_fog_range(saved.1.start, saved.1.end);
}

/// `DrawUnitModel` (0x515cc7): a unit's model with its frame textures, player colour, turning parts, gate offset,
/// frames and opacity (by the model detail), then its second graphic object's model.
fn draw_unit_model<R: Raster>(x: &mut R, s: &EeGfxUnitsScene, u: &EeGfxRenderUnit) {
    if !u.drawn || !(u.scale > 0.0) {
        return;
    }
    x.set_alpha_ref(if u.alpha_ref <= u.opacity { u.alpha_ref } else { MIN_ALPHA_REF });
    x.set_zbuffer(if u.opacity <= Z_WRITE_OPACITY { ZBUFFER_TEST_ONLY } else { ZBUFFER_TEST_AND_WRITE });
    x.group_color(u.object, u.player_color, u.animation as u32);
    let model = u.model;
    x.model_opacity(model, u.opacity);
    x.model_own_texture(model, u.texture);
    x.frame_textures(model, u.texture);
    // SAFETY: the record's arrays hold their counts for the frame
    let parts = unsafe {
        [
            (slice(u.turret_angles, u.turret_count), slice(u.turret_models, u.turret_count), false),
            (slice(u.orbital_angles, u.orbital_count), slice(u.orbital_models, u.orbital_count), false),
            (slice(u.wheel_angles, u.wheel_count), slice(u.wheel_models, u.wheel_count), true),
        ]
    };
    for (angles, models, pitch) in parts {
        for (&angle, &part) in angles.iter().zip(models) {
            let r = if pitch { turned(0.0, angle) } else { turned(angle, 0.0) };
            x.model_pre_transform(part, &r);
        }
    }
    let mut second = GeTransformation::IDENTITY;
    if u.has_sub_model_offset {
        second = x.sub_model_offset(model, u.sub_model_offset_scale);
    }
    x.model_frame(model, u.current_frame);
    x.model_tween(model, u.next_frame, if s.interpolate_frames { u.fraction } else { 0.0 });
    x.model_scale(model, u.scale);
    x.model_transform(model, &u.transformation);
    x.model_prepare(model);
    if u.waypoint {
        x.model_color(model, u.tint_color);
    }
    let opacity = match s.model_detail {
        0 => u.object_opacity,
        1 => u.object_opacity * 0.5,
        _ => 1.0,
    };
    x.model_draw(model, opacity);
    if u.object2.is_null() || !u.drawn2 || u.model2.is_null() {
        return;
    }
    let model2 = u.model2;
    x.model_texture(model2, u.texture2);
    x.group_color(u.object2, u.player_color, 0);
    let p = position(&u.transformation);
    second.m[0][3] = p.x;
    second.m[1][3] = p.y;
    second.m[2][3] = p.z;
    x.model_transform(model2, &second);
    x.model_scale(model2, u.scale);
    x.model_prepare(model2);
    x.model_opacity(model2, if u.second_full_opacity { 1.0 } else { u.opacity });
    x.model_draw(model2, u.object_opacity);
}

/// `DrawUnitModels` (0x516299): the units' models, tinted ones in a fog band of their tint; translucent ones added
/// when the card cannot modulate texture alpha.
fn draw_unit_models<R: Raster>(x: &mut R, s: &EeGfxUnitsScene, l: &Lists, list: &[u32]) {
    let modulate = x.can_modulate_alpha();
    x.lighting(EeGfxLighting::Unit);
    x.set_culling(CULL_CW);
    for &i in list {
        let u = &l.units[i as usize];
        let saved = if u.tinted { Some(fog_band(x, s, u, u.tint_color, u.fog_scale)) } else { None };
        x.lighting(if u.fogged { EeGfxLighting::DimUnit } else { EeGfxLighting::RestoreUnit });
        if modulate || u.opacity == 1.0 {
            x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
        } else {
            x.set_ambient_intensity(TRANSLUCENT_AMBIENT_SCALE * u.opacity);
            x.set_blending(BLEND_ONE, BLEND_ONE);
        }
        draw_unit_model(x, s, u);
        if let Some(saved) = saved {
            restore_fog(x, saved);
        }
    }
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
}

/// `DrawPlacement` (0x514cbc): a building being placed (fogged red where it cannot be), then for land buildings its
/// footprint's outline and blocked edges as screen lines, in batches of 64.
fn draw_placement<R: Raster>(x: &mut R, s: &EeGfxUnitsScene, g: &EeGfxGhostRecord) {
    let mut u = g.unit;
    u.opacity = 1.0;
    if u.scale != 0.0 {
        x.set_shading_source(0);
        x.lighting(EeGfxLighting::Unit);
        if g.can_place {
            draw_unit_model(x, s, &u);
        } else {
            let saved = fog_band(x, s, &u, c(1.0, 0.0, 0.0), 0.6);
            draw_unit_model(x, s, &u);
            restore_fog(x, saved);
        }
    }
    if !g.land_building {
        return;
    }
    x.lighting(EeGfxLighting::White);
    x.lighting(EeGfxLighting::FogOff);
    x.set_shading_source(1);
    x.depth_range(0.0, 0.0);
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_zcompare(COMPARE_LESS);
    let (start, end) = (s.grid_phase, s.grid_phase + 3.0);
    let mut lines = [EeGfxPlacementLine::default(); PLACEMENT_LINES];
    let vp = s.viewport();
    let n = placement_lines(&vp, s.orthographic, g.grid_size, g.tile_x, g.tile_y, &g.heights, &g.cells, &mut lines);
    let batch: Vec<_> = lines[..n]
        .iter()
        .map(|l| EeGfxScreenLine {
            color: if l.back != 0 { BAR_BACK_COLOR } else { BAR_COLOR },
            start: l.start,
            end: l.end,
        })
        .collect();
    for chunk in batch.chunks(LINE_BATCH) {
        x.screen_lines(chunk, start, end);
    }
    x.depth_range(0.0, 1.0);
    x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
    x.set_zcompare(COMPARE_LESS_EQUAL);
}

/// A bar rhombus's four vertex colours: `color` but the border colour at `border` (the fill's top right, its back's
/// top left).
fn bar_colors(color: EeGfxColor, border: usize) -> [EeGfxColor; 4] {
    let mut c = [color; 4];
    c[border] = BAR_BORDER_COLOR;
    c
}

/// `DrawBar` (0x5147f8): one bar of a unit at row `row` above its bar tag, the filled part in `color` and the rest in
/// `back`, each as two shaded rhombi.
#[allow(clippy::too_many_arguments)]
fn draw_bar<R: Raster>(
    x: &mut R,
    s: &EeGfxUnitsScene,
    sizes: &EeGfxBarSizes,
    u: &EeGfxRenderUnit,
    color: EeGfxColor,
    back: EeGfxColor,
    fraction: f32,
    row: i32,
) {
    let world = unit_bar_point(&u.transformation, u.bar, u.scale);
    let screen = screen_point(&s.viewport(), s.orthographic, world);
    let r: EeGfxBarRects = unit_bar(bar_origin(screen, row, sizes), fraction, sizes);
    let (fill_c, fill_b) = (bar_colors(color, 1), bar_colors(color, 0));
    let (rest_c, rest_b) = (bar_colors(back, 1), bar_colors(back, 0));
    match r.kind {
        0 => {
            x.bar_set(0, Some(&r.rects[0]), Some(&fill_c));
            x.bar_draw(0);
            x.bar_set(1, Some(&r.rects[1]), Some(&fill_b));
            x.bar_draw(1);
        }
        1 => {
            x.bar_set(0, Some(&r.rects[0]), Some(&fill_c));
            x.bar_set(1, Some(&r.rects[1]), Some(&fill_b));
            x.bar_draw(1);
            x.bar_draw(0);
            x.bar_set(0, Some(&r.rects[2]), Some(&rest_c));
            x.bar_set(1, Some(&r.rects[3]), Some(&rest_b));
            x.bar_draw(1);
            x.bar_draw(0);
        }
        _ => {
            x.bar_set(0, Some(&r.rects[0]), Some(&rest_c));
            x.bar_draw(0);
            x.bar_set(1, Some(&r.rects[1]), Some(&rest_b));
            x.bar_draw(1);
        }
    }
}

/// `DrawUnitBars` (0x515c3a): the health bar (when the unit has health) and one bar per extra fraction above it.
fn draw_unit_bars<R: Raster>(x: &mut R, s: &EeGfxUnitsScene, sizes: &EeGfxBarSizes, u: &EeGfxRenderUnit) {
    let mut row = 0;
    if u.health >= 0.0 {
        row = 1;
        draw_bar(x, s, sizes, u, HEALTH_COLOR, HEALTH_BACK_COLOR, u.health, 0);
    }
    // SAFETY: the record's arrays hold their counts for the frame
    for &f in unsafe { slice(u.health_bars, u.health_bar_count) } {
        draw_bar(x, s, sizes, u, BAR_COLOR, BAR_BACK_COLOR, f, row);
        row += 1;
    }
}

/// `DrawObjectBars` (0x5157b6): a selected object's health bar above it, the blending put back.
fn draw_object_bars<R: Raster>(x: &mut R, s: &EeGfxUnitsScene, sizes: &EeGfxBarSizes, o: &EeGfxRenderObject) {
    let (source, destination) = x.get_blending();
    x.set_blending(BLEND_ONE, BLEND_ZERO);
    x.bar_set(0, None, Some(&[BAR_COLOR; 4]));
    x.bar_set(1, None, Some(&[BAR_BACK_COLOR; 4]));
    let world = object_bar_point(o.location, o.bar, o.model_scale);
    let screen = screen_point(&s.viewport(), s.orthographic, world);
    let r = object_bar(screen, o.health, sizes);
    match r.kind {
        0 => {
            x.bar_set(0, Some(&r.rects[0]), None);
            x.bar_draw(0);
        }
        1 => {
            x.bar_set(0, Some(&r.rects[0]), None);
            x.bar_draw(0);
            x.bar_set(1, Some(&r.rects[1]), None);
            x.bar_draw(1);
        }
        _ => {
            x.bar_set(1, Some(&r.rects[0]), None);
            x.bar_draw(1);
        }
    }
    x.set_blending(source, destination);
}

/// `DrawBars` (0x516500): the bars sized from the window, of every selected unit and object and the pointed-at ones.
fn draw_bars<R: Raster>(x: &mut R, s: &EeGfxUnitsScene, l: &Lists) {
    x.set_blending(BLEND_ONE, BLEND_ZERO);
    x.set_culling(CULL_NONE);
    x.lighting(EeGfxLighting::White);
    let sizes = bar_sizes(s.window_width, s.window_height);
    for &i in l.selected_units {
        draw_unit_bars(x, s, &sizes, &l.units[i as usize]);
    }
    for &i in l.selected_objects {
        draw_object_bars(x, s, &sizes, &l.objects[i as usize]);
    }
    if s.selected_unit >= 0 {
        draw_unit_bars(x, s, &sizes, &l.units[s.selected_unit as usize]);
    }
    if s.selected_object >= 0 {
        draw_object_bars(x, s, &sizes, &l.objects[s.selected_object as usize]);
    }
}

/// `RenderScene` (0x518203): the selections and rings (not in cinematics), the object and unit models, the buildings
/// being placed, the air rings and the bars, then the effects' pass 5; the alpha reference put back.
pub fn render_scene<R: Raster>(x: &mut R, s: &EeGfxUnitsScene) {
    // SAFETY: the scene's arrays hold their counts for the call
    let l = unsafe { s.lists() };
    let alpha_ref = x.get_alpha_ref();
    x.lighting(EeGfxLighting::SceneFog);
    if !s.cinematic {
        x.depth_range(0.0, 0.0);
        x.set_zbuffer(ZBUFFER_TEST_ONLY);
        x.set_zcompare(COMPARE_LESS);
        x.set_shading_source(1);
        draw_object_selection(x, &l);
        draw_selection(x, &l, s.collision_spheres);
        draw_rings(x, &l, s.camera_yaw);
        x.depth_range(0.0, 1.0);
        x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
        x.set_zcompare(COMPARE_LESS_EQUAL);
    }
    x.set_shading_source(0);
    x.lighting(EeGfxLighting::SceneFog);
    draw_object_models(x, &l, l.objects0, s.camera_yaw);
    draw_unit_models(x, s, &l, l.units0);
    draw_unit_models(x, s, &l, l.units1);
    draw_object_models(x, &l, l.objects1, s.camera_yaw);
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    for g in l.ghosts {
        draw_placement(x, s, g);
    }
    if !s.cinematic {
        x.set_shading_source(1);
        draw_selection_rings(x, &l);
        x.depth_range(0.0, 0.0);
        x.set_zbuffer(ZBUFFER_TEST_ONLY);
        x.set_zcompare(COMPARE_LESS);
        draw_bars(x, s, &l);
        x.depth_range(0.0, 1.0);
        x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
        x.set_zcompare(COMPARE_LESS_EQUAL);
    }
    x.set_culling(CULL_NONE);
    (s.draw_effects)(s.context);
    x.set_alpha_ref(alpha_ref);
}

/// A NUL-terminated string's bytes (empty for null).
///
/// # Safety
/// `p` is null or NUL-terminated.
unsafe fn text<'a>(p: *const u8) -> &'a [u8] {
    if p.is_null() {
        &[]
    } else {
        // SAFETY: the function's contract
        unsafe { std::ffi::CStr::from_ptr(p.cast()).to_bytes() }
    }
}

/// `DrawLabels` (0x516692): the units' labels (a command's name starting with a blank at the unit, the debug label
/// above its unit's bars in its player's colour), clipped beside the main game form's bottom strip.
pub fn draw_labels<R: Raster>(x: &mut R, s: &EeGfxUnitsScene) {
    // SAFETY: the scene's arrays hold their counts for the call
    let l = unsafe { s.lists() };
    let vp = s.viewport();
    let clip = label_clip(vp.view_rectangle, s.window_width, s.window_height, s.free_camera, s.panel_active);
    let mut color = WHITE;
    for (i, u) in l.units.iter().enumerate() {
        let (label, point) = if u.command_type != 0 {
            // SAFETY: the record's name is NUL-terminated for the frame
            let name = unsafe { text(u.name) };
            if name.first() != Some(&b' ') {
                continue;
            }
            (name, screen_point(&vp, s.orthographic, position(&u.transformation)))
        } else if s.debug_unit == i as i32 {
            color = u.player_color;
            // SAFETY: the debug label is NUL-terminated for the frame
            let label = unsafe { text(s.debug_text) };
            let p = label_point(&vp, s.orthographic, &u.transformation, u.bar, u.scale, u.health_bar_count as u64);
            (label, p)
        } else {
            continue;
        };
        x.label(label, point.x, point.y, color, clip.font, &clip.view, &clip.strip, clip.bottom);
    }
}

/// `EEROUnits::LoadMedia`'s textures and models.
///
/// # Safety
/// `raster` is readable and its functions valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_ro_units_load(raster: *const crate::gfx::raster::EeGfxRaster, detail: i32) {
    // SAFETY: the function's contract
    let mut x = unsafe { &*raster };
    load_media(&mut x, detail);
}

/// `EEROUnits::ReleaseMedia`'s textures and models.
///
/// # Safety
/// `raster` is readable and its functions valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_ro_units_release(raster: *const crate::gfx::raster::EeGfxRaster) {
    // SAFETY: the function's contract
    let mut x = unsafe { &*raster };
    release_media(&mut x);
}

/// A command confirmation texture (0 unit, 1 path, 2 click).
#[no_mangle]
pub extern "C" fn ee_gfx_ro_units_command_texture(index: u32) -> EeGfxModel {
    command_texture(index)
}

/// The placement lines' texture.
#[no_mangle]
pub extern "C" fn ee_gfx_ro_units_line_texture() -> EeGfxModel {
    line_texture()
}

/// `EEROUnits::RenderScene`.
///
/// # Safety
/// `raster` and `scene` are readable, their functions valid and the scene's arrays hold their counts.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_ro_units_render(
    raster: *const crate::gfx::raster::EeGfxRaster,
    scene: *const EeGfxUnitsScene,
) {
    // SAFETY: the function's contract
    unsafe {
        let mut x = &*raster;
        render_scene(&mut x, &*scene);
    }
}

/// `EEROUnits::DrawLabels`.
///
/// # Safety
/// As `ee_gfx_ro_units_render`.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_ro_units_labels(
    raster: *const crate::gfx::raster::EeGfxRaster,
    scene: *const EeGfxUnitsScene,
) {
    // SAFETY: the function's contract
    unsafe {
        let mut x = &*raster;
        draw_labels(&mut x, &*scene);
    }
}

#[cfg(test)]
#[path = "ro_units_tests.rs"]
mod tests;
