//! The farm field (kind 0x29) whole in Rust on `EEE3DRust` (`EEE3DFarm`, 0xee5c0..0xf1327, deleted): a ground model
//! over the farm's tiles on the terrain's heights and normals (`farm.rs`'s mesh), crop rows as its sub-model once the
//! farm is built, their tops swaying around a shared circle in the simulation tick, and two draw copies (ground and
//! crops) the queue refreshes and the draw renders at the farm unit's opacity. The models are made, filled and placed
//! through the raster table (`create_model`, `create_sub_model`, `store_mesh`, `material_ranges`, `copy_vertices`);
//! the record keeps their handles (`farm`), the C++ releases them with the effect. The C++ keeps `IsTileVisible` (the
//! farm's tiles or a doppelganger) and `ElevationChanged`'s rectangle test (then `Rebuild`).
//!
//! Step, Load, Save and the builds are simulation-side (`Step` builds the crops, sways them and hands the ground model
//! to the unit): RUST_PLAN 2.1, the C++'s f32 operations in its order, wrapping integers.

use std::ffi::c_void;
use std::sync::Mutex;

use ee_ge::types::{Ge3DPoint, GeTransformation, GeVector};
use ee_ge::um;

use crate::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::effect_state::{identity, load_point, save_point, EeGfxEffectRecord, EffectFile};
use crate::effect_volcano::{load_area, save_area};
use crate::farm::{self, EeGfxFarmField};
use crate::ffi_mesh::{CppFarmTerrain, EeGfxFarmTerrain};
use crate::lighting::EeGfxLighting;
use crate::mesh::{EeGfxMeshTriangle, EeGfxMeshVertex, Mesh};
use crate::raster::{EeGfxModel, Raster, BLEND_INV_SRC_ALPHA, BLEND_SRC_ALPHA, ZBUFFER_TEST_AND_WRITE};
use crate::volcano::EeGfxVolcanoArea;

/// `kEffect3DFarm`
pub const EE_GFX_EFFECT_KIND_FARM: i32 = 0x29;

const OK: i32 = 0;
/// `GetTileX()` of a unit off the map
const OFF_MAP: i32 = -10000;

/// 0x4bd920: the crop variant of a tile by (x / 2 % 4, y / 2 % 4).
const TILE_VARIANT: [[i32; 4]; 4] = [[0, 1, 2, 4], [4, 3, 0, 2], [1, 2, 4, 0], [2, 0, 3, 1]];
/// 0x4bd960: the ground texture by variant (`GetCropKind`).
const CROP_KIND: [u32; 5] = [0, 10, 6, 3, 1];
/// 0x4bd974: the crop texture by variant (`GetCropFrame`).
const CROP_FRAME: [u32; 5] = [13, 7, 8, 11, 9];

/// The farm's state.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxEffectFarm {
    /// `mArea` (the farm's tiles) and `mCenter` (saved)
    pub area: EeGfxVolcanoArea,
    /// `mGroundModel` ("farm ground model"; its sub-model 0: the crops), referenced
    pub ground: EeGfxModel,
    /// `mGroundCopy`, `mCropsCopy`: the draw copies, referenced
    pub ground_copy: EeGfxModel,
    pub crops_copy: EeGfxModel,
    /// `mPhase`: 0..1, the sway step
    pub phase: f32,
    /// `mSwayA` (odd vertices), `mSwayB` (even): 0..63
    pub sway_a: i32,
    pub sway_b: i32,
    /// `mRowLength`: the farm's tile size + 1
    pub row_length: i32,
    /// `mBB4` (saved; cleared by Load)
    pub bb4: bool,
    /// `mRebuild`: the crops are due (saved)
    pub rebuild: bool,
    /// `mHasCrops`: the farm is built
    pub has_crops: bool,
    /// `mDrawCrops`, `mDoppelganger`: what the queue decided for the draw
    pub draw_crops: bool,
    pub doppelganger: bool,
    /// the port's (R5a.3): the unit's draw opacity and whether the field is drawn, taken by the queue
    pub draw_opacity: f32,
    pub draw_shown: bool,
}

impl Default for EeGfxEffectFarm {
    fn default() -> EeGfxEffectFarm {
        EeGfxEffectFarm {
            area: EeGfxVolcanoArea::default(),
            ground: std::ptr::null_mut(),
            ground_copy: std::ptr::null_mut(),
            crops_copy: std::ptr::null_mut(),
            phase: 0.0,
            sway_a: 0,
            sway_b: 0,
            row_length: 0,
            bb4: false,
            rebuild: false,
            has_crops: false,
            draw_crops: false,
            doppelganger: false,
            draw_opacity: 1.0,
            draw_shown: false,
        }
    }
}

/// What the farm reads of its unit.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeGfxFarmUnit {
    /// `GetTileX()`, `GetTileY()`
    pub tile_x: i32,
    pub tile_y: i32,
    /// `GetClass()->GetTileSize()`
    pub tile_size: i32,
    /// the complex unit's `mIsBuilt`
    pub built: bool,
    /// the complex unit's `mCloaked`: the crop variant (0..4)
    pub cloaked: i32,
}

/// The shared sway circle (`sCircle`, `sCircleBuilt`): built once, by the first farm to step.
static CIRCLE: Mutex<(bool, Vec<(f32, f32)>)> = Mutex::new((false, Vec::new()));

/// `BuildCircle` (0x4f01cb): 64 points of radius `radius / size`.
fn build_circle(size: f32, radius: f32) {
    let mut c = CIRCLE.lock().unwrap_or_else(|e| e.into_inner());
    c.0 = true;
    let radius = 1.0 / size * radius;
    let mut angle = 0.0f32;
    while angle < 6.283_185_5 {
        let x = um::um_cos(angle) * radius;
        let y = um::um_cos(1.570_796_4 - angle) * radius;
        c.1.push((x, y));
        angle += 0.098_174_77;
    }
}

/// A flat terrain for hosts that answer no farm (the tests').
pub(crate) unsafe extern "C" fn flat_z(_: *mut c_void, _: u32, _: u32) -> f32 {
    0.0
}
pub(crate) unsafe extern "C" fn flat_normal(_: *mut c_void, _: u32, _: u32) -> GeVector {
    GeVector { x: 0.0, y: 0.0, z: 1.0 }
}
pub(crate) unsafe extern "C" fn flat_mesh_z(_: *mut c_void, _: f32, _: f32) -> f32 {
    0.0
}

/// The flat terrain.
pub(crate) fn flat_terrain() -> EeGfxFarmTerrain {
    EeGfxFarmTerrain { context: std::ptr::null_mut(), z: flat_z, normal: flat_normal, mesh_z: flat_mesh_z }
}

/// The record's farm.
///
/// # Safety
/// `r` is a live record; no other reference to it lives during the use.
unsafe fn f<'a>(r: *mut EeGfxEffectRecord) -> &'a mut EeGfxEffectFarm {
    // SAFETY: the function's contract
    unsafe { &mut (*r).farm }
}

/// The crop frame of the unit's variant (`GetCropFrame`; its `mCloaked`, 0..4 as the farm sets it: the original read
/// past its table for another value, the first entry here).
fn crop_frame(u: &EeGfxFarmUnit) -> u32 {
    usize::try_from(u.cloaked).ok().and_then(|i| CROP_FRAME.get(i)).copied().unwrap_or(CROP_FRAME[0])
}

/// `PlaceArea` (0x4f0247): the tile rectangle, centre and vertex row length from the unit's tile and size.
fn place_area(a: &mut EeGfxEffectFarm, u: &EeGfxFarmUnit) {
    let x = u.tile_x as f32;
    let y = u.tile_y as f32;
    let size = u.tile_size as f32;
    a.area.left = x;
    a.area.top = y;
    a.area.right = size + x - 1.0;
    a.area.bottom = size + y - 1.0;
    let half = u.tile_size.wrapping_add(1) / 2;
    a.area.center.x = half as f32 + u.tile_x as f32;
    a.area.center.y = half as f32 + u.tile_y as f32;
    a.row_length = u.tile_size.wrapping_add(1);
}

/// `HasMoved` (0x4f0aa1): the unit's centre tile is not the field's centre.
fn has_moved(a: &EeGfxEffectFarm, u: &EeGfxFarmUnit) -> bool {
    let half = u.tile_size.wrapping_add(1) / 2;
    um::floor(a.area.center.x) != half.wrapping_add(u.tile_x)
        || um::floor(a.area.center.y) != half.wrapping_add(u.tile_y)
}

/// The field `farm.rs` builds over.
fn field(a: &EeGfxEffectFarm, u: &EeGfxFarmUnit) -> EeGfxFarmField {
    EeGfxFarmField {
        left: a.area.left,
        top: a.area.top,
        right: a.area.right,
        bottom: a.area.bottom,
        row_length: a.row_length,
        tile_size: u.tile_size as f32,
    }
}

/// An identity transformation at `p` (`SetPosition`).
fn at(p: Ge3DPoint) -> GeTransformation {
    let mut t = identity();
    t.m[0][3] = p.x;
    t.m[1][3] = p.y;
    t.m[2][3] = p.z;
    t
}

/// A copy filled with invisible placeholder triangles: polygon 0 holds the last triple's indices (the original set
/// polygon 0 every time); each triple's points and opacities within the `count` vertices (the original wrote past
/// the end when `count` is not a multiple of 3: `EEFarmPlaceholder`'s bounds).
fn placeholders(count: i32, polygons: u32) -> (Vec<EeGfxMeshVertex>, Vec<EeGfxMeshTriangle>) {
    let mut vertices = vec![EeGfxMeshVertex::default(); count.max(0) as usize];
    let mut triangles = vec![EeGfxMeshTriangle::default(); polygons as usize];
    let mut mesh = Mesh::new(&mut vertices, &mut triangles);
    let points = [
        Ge3DPoint { x: 0.0, y: 0.0, z: 0.0 },
        Ge3DPoint { x: 0.0001, y: 0.0, z: 0.0 },
        Ge3DPoint { x: 0.0, y: 0.0001, z: 0.0 },
    ];
    let mut i: i32 = 0;
    while i < count {
        let first = i as i64;
        mesh.triangle(0, first, first + 1, first + 2);
        for (k, p) in points.iter().enumerate() {
            if i.wrapping_add(k as i32) < count {
                mesh.point(first + k as i64, *p);
                mesh.opacity(first + k as i64, 0.0);
            }
        }
        i = i.wrapping_add(3);
    }
    (vertices, triangles)
}

/// `BuildGroundModels` (0x4f04ee): the ground and crop draw copies, sized for the field, filled with placeholders.
fn build_copies<R: Raster>(a: &mut EeGfxEffectFarm, g: &EeGfxGraphicFacts, x: &mut R) {
    let width = a.area.right - a.area.left;
    let rl = a.row_length;
    let polygons = um::floor((width + 1.0) * um::floor(width + 1.0) as f32).wrapping_mul(2) as u32;
    let vertices = rl.wrapping_mul(rl);
    a.ground_copy = x.create_model(polygons, vertices as u32, 0, "graphics farm ground model", 1);
    x.model_reference(a.ground_copy);
    let (v, t) = placeholders(vertices, polygons);
    x.store_mesh(a.ground_copy, &v, &t);
    x.material_ranges(a.ground_copy, polygons, vertices as u32);
    x.model_bounds(a.ground_copy);
    let w = um::floor(width + 1.0);
    let rows = g.int_param1;
    let polygons = w.wrapping_mul(w).wrapping_mul(rows).wrapping_mul(2) as u32;
    let vertices = rows.wrapping_mul(rl).wrapping_mul(w).wrapping_mul(2);
    a.crops_copy = x.create_model(polygons, vertices as u32, 0, "graphics farm stalk model", 1);
    x.model_reference(a.crops_copy);
    let (v, t) = placeholders(vertices, polygons);
    x.store_mesh(a.crops_copy, &v, &t);
    x.material_ranges(a.crops_copy, polygons, vertices as u32);
    x.model_bounds(a.crops_copy);
}

/// `BuildGroundModel` (0x4ee7cd): the ground mesh over the field's tiles (also into the draw copy), placed; the farm's
/// crop variant from its first drop site's tile; the ground texture.
fn build_ground<H: Effect3DHost, R: Raster>(r: *mut EeGfxEffectRecord, h: &mut H, x: &mut R) -> i32 {
    let u = h.farm_unit();
    // SAFETY: the caller's contract; no reference to the record is kept across a host call
    let a = unsafe { f(r) };
    let size = u.tile_size as f32;
    let width = um::floor(a.area.right - a.area.left + 1.0);
    let polygons = width.wrapping_mul(width).wrapping_mul(2) as u32;
    let vertices = a.row_length.wrapping_mul(a.row_length) as u32;
    if !a.ground.is_null() {
        x.model_dereference(a.ground);
    }
    a.ground = x.create_model(polygons, vertices, 1, "farm ground model", 0);
    x.model_reference(a.ground);
    let dummy = x.create_sub_model(1, 3, a.ground);
    x.model_reference(dummy);
    let (v, t) = placeholders(3, 1);
    x.store_mesh(dummy, &v, &t);
    x.material_ranges(dummy, 1, 3);
    x.material_ranges(a.ground, polygons, vertices);
    x.material_ranges(a.ground_copy, polygons, vertices);
    let mut v = vec![EeGfxMeshVertex::default(); vertices as usize];
    let mut t = vec![EeGfxMeshTriangle::default(); polygons as usize];
    let place = farm::ground(&field(a, &u), &mut CppFarmTerrain(h.farm_terrain()), &mut Mesh::new(&mut v, &mut t));
    x.store_mesh(a.ground, &v, &t);
    x.store_mesh(a.ground_copy, &v, &t);
    x.model_dereference(dummy);
    x.model_bounds(a.ground);
    x.model_bounds(a.ground_copy);
    let transformation = at(place);
    x.model_transform(a.ground_copy, &transformation);
    x.model_scale(a.ground_copy, size);
    x.model_prepare(a.ground_copy);
    let ground = a.ground;
    // SAFETY: as above
    unsafe { (*r).transformation = transformation };
    h.move_to();
    let radius = x.bounding_radius(ground) * size;
    // SAFETY: as above
    unsafe { (*r).bounding_radius = radius };
    let (mut tx, mut ty) = (0, 0);
    let kind = if h.farm_drop_site(&mut tx, &mut ty) {
        TILE_VARIANT[((tx >> 1) & 3) as usize][((ty >> 1) & 3) as usize]
    } else {
        0
    };
    h.farm_set_variant(kind);
    let texture = h.texture_at(CROP_KIND[kind as usize]);
    // SAFETY: as above
    let a = unsafe { f(r) };
    x.model_texture(a.ground, texture);
    x.model_texture(a.ground_copy, texture);
    OK
}

/// `BuildCrops` (0x4ef5fa): the crop rows as the ground's sub-model (as many rows a tile row as the effect's first int
/// parameter), also into the draw copy, textured with the variant's crop frame.
fn build_crops<H: Effect3DHost, R: Raster>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut H, x: &mut R) {
    let u = h.farm_unit();
    // SAFETY: the caller's contract; no reference to the record is kept across a host call
    let a = unsafe { f(r) };
    let size = u.tile_size as f32;
    let rows = g.int_param1 as u32;
    let width = um::floor(a.area.right - a.area.left + 1.0) as u32;
    let polygons = width.wrapping_mul(width).wrapping_mul(rows).wrapping_mul(2);
    let vertices = width.wrapping_mul(a.row_length as u32).wrapping_mul(rows).wrapping_mul(2);
    let model = x.create_sub_model(polygons, vertices, a.ground);
    x.material_ranges(model, polygons, vertices);
    x.material_ranges(a.crops_copy, polygons, vertices);
    let mut v = vec![EeGfxMeshVertex::default(); vertices as usize];
    let mut t = vec![EeGfxMeshTriangle::default(); polygons as usize];
    let place =
        farm::crops(&field(a, &u), rows, g.size, &mut CppFarmTerrain(h.farm_terrain()), &mut Mesh::new(&mut v, &mut t));
    x.store_mesh(model, &v, &t);
    x.store_mesh(a.crops_copy, &v, &t);
    x.model_bounds(model);
    x.model_bounds(a.crops_copy);
    let transformation = at(place);
    x.model_transform(a.crops_copy, &transformation);
    x.model_scale(a.crops_copy, size);
    x.model_prepare(a.crops_copy);
    let crops_copy = a.crops_copy;
    let frame = crop_frame(&h.farm_unit());
    let texture = h.texture_at(frame);
    x.texture_wrap(texture);
    x.model_texture(model, texture);
    x.model_texture(crops_copy, texture);
}

/// `SetUp` (0x4f0c04): the graphic, the ground model, the sway reset; crops once the farm is built.
fn set_up<H: Effect3DHost, R: Raster>(r: *mut EeGfxEffectRecord, h: &mut H, x: &mut R) -> i32 {
    let mut result = h.load_media();
    if result == OK {
        result = build_ground(r, h, x);
    }
    let built = h.farm_unit().built;
    // SAFETY: the caller's contract
    let a = unsafe { f(r) };
    a.has_crops = built;
    a.sway_a = 0;
    a.phase = 0.0;
    a.sway_b = 0;
    result
}

/// The field placed again on its unit, set up, and the crops built if the farm is (the free camera's move,
/// `ElevationChanged`).
fn rebuild<H: Effect3DHost, R: Raster>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut H, x: &mut R) {
    let u = h.farm_unit();
    // SAFETY: the caller's contract
    place_area(unsafe { f(r) }, &u);
    set_up(r, h, x);
    if h.farm_unit().built {
        build_crops(r, g, h, x);
    }
}

/// The farm's virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub unsafe fn farm_effect<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept across a host call
    match op {
        // 0x4ee64c: the area, the flags, the draw copies
        EeGfxEffect3DOp::Create => {
            let u = h.farm_unit();
            let a = unsafe { f(r) };
            place_area(a, &u);
            a.ground = std::ptr::null_mut();
            a.ground_copy = std::ptr::null_mut();
            a.crops_copy = std::ptr::null_mut();
            a.bb4 = false;
            a.rebuild = true;
            a.has_crops = false;
            a.draw_crops = false;
            a.doppelganger = false;
            a.draw_opacity = 1.0;
            a.draw_shown = false;
            build_copies(a, g, x);
            OK
        }
        EeGfxEffect3DOp::SetUp => set_up(r, h, x),
        EeGfxEffect3DOp::Rebuild => {
            rebuild(r, g, h, x);
            OK
        }
        // 0x4f0f26: after the base's pointers, the area clamped to the farm's size, the models rebuilt
        EeGfxEffect3DOp::Repair => {
            let u = h.farm_unit();
            let a = unsafe { f(r) };
            let size = u.tile_size as f32;
            a.row_length = u.tile_size.wrapping_add(1);
            if a.area.right - a.area.left > size - 1.0 && a.area.bottom - a.area.top > size - 1.0 {
                a.area.right = a.area.left + size - 1.0;
                a.area.top = a.area.bottom - size + 1.0;
            }
            build_copies(a, g, x);
            let result = build_ground(r, h, x);
            let built = h.farm_unit().built;
            unsafe { f(r) }.has_crops = built;
            if built {
                build_crops(r, g, h, x);
                unsafe { f(r) }.rebuild = false;
            } else {
                unsafe { f(r) }.rebuild = true;
            }
            unsafe { f(r) }.phase = 0.0;
            result
        }
        // 0x4f02e7: the draw copies refreshed, queue 0, the draw's reads of the unit
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            let a = unsafe { f(r) };
            if result != OK || a.ground_copy.is_null() || a.crops_copy.is_null() {
                return 10;
            }
            let built = h.farm_doppelganger();
            let a = unsafe { f(r) };
            if built >= 0 {
                a.draw_crops = built != 0;
                a.doppelganger = true;
            } else {
                a.doppelganger = false;
                a.draw_crops = a.has_crops;
            }
            if a.has_crops {
                let crops = x.sub_model(a.ground, 0);
                x.copy_vertices(crops, a.crops_copy, false);
                x.model_bounds(a.crops_copy);
            }
            x.copy_vertices(a.ground, a.ground_copy, true);
            x.model_bounds(a.ground_copy);
            h.queue(0);
            let opacity = h.farm_opacity(unsafe { f(r) }.draw_opacity);
            unsafe { f(r) }.draw_opacity = opacity;
            let u = h.farm_unit();
            if g.free_camera && has_moved(unsafe { f(r) }, &u) && u.tile_x != OFF_MAP && u.tile_y != OFF_MAP {
                rebuild(r, g, h, x);
            }
            let u = h.farm_unit();
            unsafe { f(r) }.draw_shown =
                (g.unit_hit_points > 0 || !g.free_camera) && u.tile_x != OFF_MAP && u.tile_y != OFF_MAP;
            OK
        }
        // 0x4f0ce2: the ground and the crops at the unit's opacity
        EeGfxEffect3DOp::DrawPass2 => {
            let a = *unsafe { f(r) };
            if !a.draw_shown {
                return 0;
            }
            let (source, destination) = x.get_blending();
            let zbuffer = x.get_zbuffer();
            let alpha_ref = x.get_alpha_ref();
            x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
            x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
            x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
            x.set_shading_source(0);
            x.lighting(if a.doppelganger { EeGfxLighting::DimUnit } else { EeGfxLighting::Terrain });
            x.model_opacity(a.ground_copy, a.draw_opacity);
            x.model_opacity(a.crops_copy, a.draw_opacity);
            let mut result = x.model_draw(a.ground_copy, 1.0);
            if a.draw_crops {
                result = x.model_draw(a.crops_copy, 1.0);
            }
            x.set_alpha_ref(alpha_ref);
            x.set_zbuffer(zbuffer);
            x.set_blending(source, destination);
            x.set_shading_source(1);
            result
        }
        // 0x4f1043: 7 for a dead farm without a doppelganger; the crops built and swayed; the ground to the unit
        EeGfxEffect3DOp::Step => {
            let u = h.farm_unit();
            {
                let built = CIRCLE.lock().unwrap_or_else(|e| e.into_inner()).0;
                if !built {
                    build_circle(u.tile_size as f32, g.param3);
                }
            }
            if g.unit_hit_points <= 0 && !h.farm_doppelganger_any() {
                return 7;
            }
            let a = unsafe { f(r) };
            if a.rebuild && u.built {
                build_crops(r, g, h, x);
                unsafe { f(r) }.rebuild = false;
            }
            let a = unsafe { f(r) };
            a.has_crops = u.built;
            if a.has_crops {
                let crops = x.sub_model(a.ground, 0);
                if !h.farm_loops(crop_frame(&u)) {
                    let a = unsafe { f(r) };
                    a.phase = g.tick_time / g.param2 + a.phase;
                    if a.phase > 1.0 {
                        a.sway_b = a.sway_b.wrapping_add(1);
                        a.sway_a = a.sway_a.wrapping_sub(1);
                        a.phase -= 1.0;
                        if a.sway_a < 0 {
                            a.sway_a = 0x3f;
                        }
                        a.sway_a %= 64;
                        a.sway_b %= 64;
                        let circle = CIRCLE.lock().unwrap_or_else(|e| e.into_inner());
                        let span = (a.row_length as u32).wrapping_mul(2);
                        let count = x.vertex_count(crops);
                        for i in 0..count {
                            if i.checked_rem(span).unwrap_or(0) > u.tile_size as u32 {
                                continue;
                            }
                            let mut point = x.vertex_point(crops, i);
                            let base = x.vertex_point(crops, (a.row_length as u32).wrapping_add(i));
                            let index = if i & 1 == 1 { a.sway_a } else { a.sway_b };
                            let (cx, cy) = circle.1[index as usize];
                            point.x = base.x + cx;
                            point.y = base.y + cy;
                            x.set_vertex_point(crops, i, &point);
                        }
                        x.model_bounds(crops);
                    }
                }
            }
            h.set_scaled_model(unsafe { f(r) }.ground);
            OK
        }
        _ => 0,
    }
}

/// `Load` (0x4f0c71; the graphic reloaded, the area, centre and flags; one flag cleared) or `Save` (0x4f0fe9) after
/// the base's, whose result both ignore.
pub fn farm_file<H: Effect3DHost, F: EffectFile>(load: bool, r: &mut EeGfxEffectRecord, f: &mut F, h: &mut H) -> i32 {
    let a = &mut r.farm;
    if load {
        h.load_media();
        let mut res = load_area(f, &mut a.area);
        if res == OK {
            res = load_point(f, &mut a.area.center);
        }
        if res == OK {
            res = f.read_bool(&mut a.rebuild);
        }
        if res == OK {
            res = f.read_bool(&mut a.bb4);
            a.bb4 = false;
        }
        res
    } else {
        let mut res = save_area(f, &a.area);
        if res == OK {
            res = save_point(f, &a.area.center);
        }
        if res == OK {
            res = f.write_bool(a.rebuild);
        }
        if res == OK {
            res = f.write_bool(a.bb4);
        }
        res
    }
}

#[cfg(test)]
#[path = "effect_farm_tests.rs"]
mod tests;
