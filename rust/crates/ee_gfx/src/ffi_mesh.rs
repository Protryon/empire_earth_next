//! The C ABI of the effects' mesh builders (`farm.rs`, `volcano.rs`) and the tracer beam (`beam.rs`), called by
//! `EEE3DFarm.cpp`, `EEE3DVolcano.cpp` and `EEE3DTracerBeam.cpp`. The terrain callbacks run while Rust holds no
//! reference to a C++ object: the outputs are the C++'s own arrays, which the callbacks do not touch.

use std::ffi::c_void;

use ee_ge::types::{Ge3DPoint, GeVector};

use crate::beam::{self, EeGfxBeamAim, EeGfxBeamVertex};
use crate::effects::EeGfxColor;
use crate::farm::{self, EeGfxFarmField, FarmTerrain};
use crate::mesh::{EeGfxMeshTriangle, EeGfxMeshVertex, Mesh};
use crate::volcano::{self, EeGfxVolcanoArea, EeGfxVolcanoBuilt, EeGfxVolcanoSizes, VolcanoTerrain};

/// A slice over `count` elements at `p` (empty for a null pointer or a zero count).
///
/// # Safety
/// A non-null `p` points at `count` writable elements.
unsafe fn slice_mut<'a, T>(p: *mut T, count: usize) -> &'a mut [T] {
    if p.is_null() || count == 0 {
        &mut []
    } else {
        // SAFETY: the function's contract
        unsafe { std::slice::from_raw_parts_mut(p, count) }
    }
}

/// The terrain under a farm field, as C++ callbacks over `context` (the `GETerrainMesh`).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeGfxFarmTerrain {
    pub context: *mut c_void,
    /// `GetZ(x, y)`
    pub z: unsafe extern "C" fn(context: *mut c_void, x: u32, y: u32) -> f32,
    /// `*GetVertexNormal(x, y)`
    pub normal: unsafe extern "C" fn(context: *mut c_void, x: u32, y: u32) -> GeVector,
    /// `GetMeshZ(x, y)`
    pub mesh_z: unsafe extern "C" fn(context: *mut c_void, x: f32, y: f32) -> f32,
}

struct CppFarmTerrain(EeGfxFarmTerrain);

impl FarmTerrain for CppFarmTerrain {
    fn z(&mut self, x: u32, y: u32) -> f32 {
        // SAFETY: the callback's contract (the C++ mesh outlives the call)
        unsafe { (self.0.z)(self.0.context, x, y) }
    }
    fn normal(&mut self, x: u32, y: u32) -> GeVector {
        // SAFETY: as above
        unsafe { (self.0.normal)(self.0.context, x, y) }
    }
    fn mesh_z(&mut self, x: f32, y: f32) -> f32 {
        // SAFETY: as above
        unsafe { (self.0.mesh_z)(self.0.context, x, y) }
    }
}

/// `EEE3DFarm::BuildGroundModel`'s mesh into `vertices` and `triangles`; returns the models' place.
///
/// # Safety
/// `field` is valid; the arrays hold `vertex_count` and `triangle_count` writable records (or are null).
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_farm_ground(
    field: *const EeGfxFarmField,
    terrain: EeGfxFarmTerrain,
    vertices: *mut EeGfxMeshVertex,
    vertex_count: usize,
    triangles: *mut EeGfxMeshTriangle,
    triangle_count: usize,
) -> Ge3DPoint {
    // SAFETY: the function's contract (the field copied first)
    unsafe {
        let f = *field;
        let mut mesh = Mesh::new(slice_mut(vertices, vertex_count), slice_mut(triangles, triangle_count));
        farm::ground(&f, &mut CppFarmTerrain(terrain), &mut mesh)
    }
}

/// `EEE3DFarm::BuildCrops`' mesh: `rows` crop rows per tile row, `height` tall; returns the models' place.
///
/// # Safety
/// As `ee_gfx_farm_ground`.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_farm_crops(
    field: *const EeGfxFarmField,
    rows: u32,
    height: f32,
    terrain: EeGfxFarmTerrain,
    vertices: *mut EeGfxMeshVertex,
    vertex_count: usize,
    triangles: *mut EeGfxMeshTriangle,
    triangle_count: usize,
) -> Ge3DPoint {
    // SAFETY: the function's contract (the field copied first)
    unsafe {
        let f = *field;
        let mut mesh = Mesh::new(slice_mut(vertices, vertex_count), slice_mut(triangles, triangle_count));
        farm::crops(&f, rows, height, &mut CppFarmTerrain(terrain), &mut mesh)
    }
}

/// The terrain under a volcano, as C++ callbacks over `context` (the `EEMap`).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeGfxVolcanoTerrain {
    pub context: *mut c_void,
    /// `mTerrainMesh.GetZ(x, y)`
    pub z: unsafe extern "C" fn(context: *mut c_void, x: u32, y: u32) -> f32,
    /// `EETerrainTable::Get(mTerrainMesh.GetTextureSetID(x, y))->mColor`
    pub color: unsafe extern "C" fn(context: *mut c_void, x: u32, y: u32) -> EeGfxColor,
    /// `mTerrainMesh.GetIsValidTile(x, y)`
    pub valid_tile: unsafe extern "C" fn(context: *mut c_void, x: u32, y: u32) -> bool,
    /// `GetVertexColor(x, y, corner)`
    pub light: unsafe extern "C" fn(context: *mut c_void, x: i32, y: i32, corner: i32) -> EeGfxColor,
}

struct CppVolcanoTerrain(EeGfxVolcanoTerrain);

impl VolcanoTerrain for CppVolcanoTerrain {
    fn z(&mut self, x: u32, y: u32) -> f32 {
        // SAFETY: the callback's contract (the C++ map outlives the call)
        unsafe { (self.0.z)(self.0.context, x, y) }
    }
    fn color(&mut self, x: u32, y: u32) -> EeGfxColor {
        // SAFETY: as above
        unsafe { (self.0.color)(self.0.context, x, y) }
    }
    fn valid_tile(&mut self, x: u32, y: u32) -> bool {
        // SAFETY: as above
        unsafe { (self.0.valid_tile)(self.0.context, x, y) }
    }
    fn light(&mut self, x: i32, y: i32, corner: i32) -> EeGfxColor {
        // SAFETY: as above
        unsafe { (self.0.light)(self.0.context, x, y, corner) }
    }
}

/// The volcano mesh's sizes for an area `width` wide.
#[no_mangle]
pub extern "C" fn ee_gfx_volcano_sizes(width: f32) -> EeGfxVolcanoSizes {
    volcano::sizes(width)
}

/// `EEE3DVolcano::BuildModel`'s image: `pixels` is the locked image, `row` pixels a row (19 rows of at least 19),
/// into `colors` and `heights` (`mHeights`; `count` each) of a mesh `side` vertices a side; returns the writes
/// dropped.
///
/// # Safety
/// `pixels` holds `18 row + 19` pixels (or is null); the arrays hold `count` each (or are null).
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_volcano_image(
    height_scale: f32,
    side: i32,
    pixels: *const u32,
    row: usize,
    colors: *mut EeGfxColor,
    heights: *mut f32,
    count: usize,
) -> u32 {
    let n = volcano::EE_GFX_VOLCANO_IMAGE_SIDE;
    // SAFETY: the function's contract
    unsafe {
        let image =
            if pixels.is_null() || row < n { &[][..] } else { std::slice::from_raw_parts(pixels, row * (n - 1) + n) };
        volcano::image(height_scale, side, image, row, slice_mut(colors, count), slice_mut(heights, count))
    }
}

/// `EEE3DVolcano::BuildModel`'s mesh over the image's `colors` and `heights`; `points` and `heights` are `mPoints`
/// and `mHeights` (`count` each).
///
/// # Safety
/// `area` is valid; the arrays hold their counts (or are null).
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_volcano_build(
    area: *const EeGfxVolcanoArea,
    colors: *const EeGfxColor,
    terrain: EeGfxVolcanoTerrain,
    points: *mut Ge3DPoint,
    heights: *mut f32,
    count: usize,
    vertices: *mut EeGfxMeshVertex,
    vertex_count: usize,
    triangles: *mut EeGfxMeshTriangle,
    triangle_count: usize,
) -> EeGfxVolcanoBuilt {
    // SAFETY: the function's contract (the area copied first)
    unsafe {
        let a = *area;
        let colors: &[EeGfxColor] =
            if colors.is_null() || count == 0 { &[] } else { std::slice::from_raw_parts(colors, count) };
        let mut mesh = Mesh::new(slice_mut(vertices, vertex_count), slice_mut(triangles, triangle_count));
        volcano::build(
            &a,
            colors,
            &mut CppVolcanoTerrain(terrain),
            slice_mut(points, count),
            slice_mut(heights, count),
            &mut mesh,
        )
    }
}

/// `EEE3DTracerBeam::Aim`'s beam between `start` and `end` at `age` (the effect's params 5 and 6 grow and shrink
/// it).
///
/// # Safety
/// `start` and `end` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_beam_aim(
    start: *const Ge3DPoint,
    end: *const Ge3DPoint,
    age: f32,
    grow: f32,
    shrink: f32,
) -> EeGfxBeamAim {
    // SAFETY: the function's contract
    unsafe { beam::aim(*start, *end, age, grow, shrink) }
}

/// `EEE3DTracerBeam::DrawPass6`'s stretch of `count` vertices in place (length, width, texture scroll).
///
/// # Safety
/// `vertices` holds `count` records (or is null).
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_beam_stretch(
    vertices: *mut EeGfxBeamVertex,
    count: usize,
    length: f32,
    width: f32,
    scroll: f32,
) {
    // SAFETY: the function's contract
    beam::stretch(unsafe { slice_mut(vertices, count) }, length, width, scroll);
}

/// `DrawPass6`'s stretch of a sub-model's internal y.
#[no_mangle]
pub extern "C" fn ee_gfx_beam_stretch_y(y: f32, length: f32) -> f32 {
    beam::stretch_y(y, length)
}

/// `CreateHorizontalPlaneModel`'s (`standing` false) or `CreateStandingPlaneModel`'s mesh.
///
/// # Safety
/// `vertices` holds 4 writable records and `triangles` 2.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_effect_plane(
    standing: bool,
    vertices: *mut EeGfxMeshVertex,
    triangles: *mut EeGfxMeshTriangle,
) {
    let (v, t) = crate::effects::plane_arrays(standing);
    // SAFETY: the function's contract
    unsafe {
        std::ptr::copy_nonoverlapping(v.as_ptr(), vertices, 4);
        std::ptr::copy_nonoverlapping(t.as_ptr(), triangles, 2);
    }
}
