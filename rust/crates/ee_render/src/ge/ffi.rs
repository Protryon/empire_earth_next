//! The C ABI of the Graphics Engine's terrain mesh queries and the physical model's triangle tests (the math's is
//! ee_base::math::ffi).

use std::ffi::c_long;
use std::ffi::c_ulong;

use crate::ge::physical;
use crate::ge::terrain::{edge_planes, GeTerrainView, GeTmRow};
use ee_base::math::types::Ge3DLine;
use ee_base::math::types::Ge3DPlane;
use ee_base::math::types::Ge3DPoint;
use ee_base::math::types::GeBoundingBox;
use ee_base::math::types::GeVector;

/// Writes `value` through `out` when there is one.
///
/// # Safety
/// `out` is null or valid for a write.
unsafe fn put<T>(out: *mut T, value: Option<T>) -> bool {
    match value {
        Some(v) => {
            // SAFETY: the caller's contract
            unsafe { *out = v };
            true
        }
        None => false,
    }
}

// ---- GETerrainMesh's queries ----

/// `GETerrainMesh::GetMeshZ`.
///
/// # Safety
/// `view` describes a live mesh; the tile under the position exists.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_terrain_z(view: *const GeTerrainView, x: f32, y: f32) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*view).mesh_z(x, y) }
}

/// `GETerrainMesh::GetMeshNormal`.
///
/// # Safety
/// As `ee_ge_terrain_z`; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_terrain_normal(view: *const GeTerrainView, x: f32, y: f32, out: *mut GeVector) {
    // SAFETY: the function's contract
    unsafe {
        let n = (*view).mesh_normal(x, y);
        *out = n;
    }
}

/// `GETerrainMesh::InitializePlanes`: writes the front, back, left and right planes to `out[0..4]`.
///
/// # Safety
/// `tile_rows` holds `tile_row_count` (at least one) rows; `out` is valid for four planes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_terrain_planes(tile_rows: *const GeTmRow, tile_row_count: c_ulong, out: *mut Ge3DPlane) {
    // SAFETY: the function's contract
    unsafe {
        let planes = edge_planes(tile_rows, tile_row_count);
        std::ptr::copy_nonoverlapping(planes.as_ptr(), out, 4);
    }
}

/// `GETerrainMesh::GetClosestTile(point, x, y)`.
///
/// # Safety
/// The pointers are valid; `view`'s planes are.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_terrain_closest_tile_point(
    view: *const GeTerrainView,
    point: *const Ge3DPoint,
    out_x: *mut c_long,
    out_y: *mut c_long,
) {
    // SAFETY: the function's contract
    unsafe {
        let (x, y) = (*view).closest_tile_to_point(*point);
        *out_x = x;
        *out_y = y;
    }
}

/// `GETerrainMesh::GetClosestTile(line, x, y)`.
///
/// # Safety
/// The pointers are valid; `view` describes a live mesh.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_terrain_closest_tile_line(
    view: *const GeTerrainView,
    line: *const Ge3DLine,
    out_x: *mut c_long,
    out_y: *mut c_long,
) {
    // SAFETY: the function's contract
    unsafe {
        let line = *line;
        let (x, y) = (*view).closest_tile_to_line(&line);
        *out_x = x;
        *out_y = y;
    }
}

/// `GETerrainMesh::Intersects(line, distance)`: `distance` is written whenever the C++ wrote it (a triangle's plane
/// met, hit or not).
///
/// # Safety
/// The pointers are valid; `view` describes a live mesh.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_terrain_intersects(
    view: *const GeTerrainView,
    line: *const Ge3DLine,
    distance: *mut f32,
) -> bool {
    // SAFETY: the function's contract
    unsafe {
        let line = *line;
        let (hit, d) = (*view).intersects(&line);
        put(distance, d);
        hit
    }
}

// ---- GEPhysicalModel's per-triangle tests ----

/// One triangle of `GEPhysicalModel::Intersects(location, model, frame, line, ...)`; `distance` is written whenever
/// the C++ wrote it (the triangle's plane was not parallel to the line).
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_physical_line_triangle(
    line: *const Ge3DLine,
    p1: *const Ge3DPoint,
    p2: *const Ge3DPoint,
    p3: *const Ge3DPoint,
    exact: bool,
    distance: *mut f32,
) -> bool {
    // SAFETY: the function's contract
    unsafe {
        let (hit, d) = physical::line_hits_triangle(&*line, *p1, *p2, *p3, exact);
        put(distance, d);
        hit
    }
}

/// `GEPhysicalModel.cpp`'s `ComputeTriangleBox`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_physical_triangle_box(
    p1: *const Ge3DPoint,
    p2: *const Ge3DPoint,
    p3: *const Ge3DPoint,
    out: *mut GeBoundingBox,
) {
    // SAFETY: the function's contract
    unsafe { *out = physical::triangle_box(*p1, *p2, *p3) };
}
