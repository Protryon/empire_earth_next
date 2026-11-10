//! The C ABI of `GEViewport`'s math (`viewport.rs`), `GEPhysicalModel::Intersects(viewport)` (`physical.rs`) and
//! `UpdateVisibleTerrain`'s plan (`terrain_visible.rs`). The viewport pointer is the C++ object itself
//! (`GERustViewport.h` pins the layout); the plan is a Rust-owned handle the C++ reads through `GeVisibleOut` and
//! frees with `ee_ge_terrain_visible_free`.

use std::ffi::c_ulong;

use crate::bitmap::{self, Stretch};
use crate::physical;
use crate::terrain_visible::{self, GeVisibleInput, GeVisiblePolygon, VisiblePlan};
use crate::types::{Ge2DPoint, Ge3DPlane, Ge3DPoint, GeBoundingBox, GeRectangle, GeTransformation};
use crate::viewport::GeViewport;

// ---- GEViewport ----

/// `GEViewport()`.
///
/// # Safety
/// `out` is valid for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_default(out: *mut GeViewport) {
    // SAFETY: the function's contract
    unsafe { *out = GeViewport::default() };
}

/// `GEViewport(view, orthographic, camera, front, rear, fov)`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_init(
    out: *mut GeViewport,
    view: *const GeRectangle,
    orthographic: *const GeRectangle,
    camera: *const GeTransformation,
    front: f32,
    rear: f32,
    field_of_view: f32,
) {
    // SAFETY: the function's contract
    unsafe { *out = GeViewport::new(*view, *orthographic, *camera, front, rear, field_of_view) };
}

/// `GEViewport::ComputeOrthographicMatrix`.
///
/// # Safety
/// `v` is a live viewport.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_orthographic_matrix(v: *mut GeViewport) {
    // SAFETY: the function's contract
    unsafe { (*v).compute_orthographic_matrix() };
}

/// `GEViewport::ComputePerspectiveMatrix`.
///
/// # Safety
/// `v` is a live viewport.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_perspective_matrix(v: *mut GeViewport) {
    // SAFETY: the function's contract
    unsafe { (*v).compute_perspective_matrix() };
}

/// `GEViewport::ComputePlanes`.
///
/// # Safety
/// `v` is a live viewport.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_planes(v: *mut GeViewport) {
    // SAFETY: the function's contract
    unsafe { (*v).compute_planes() };
}

/// `GEViewport::GetFrustrumWorldPoints(rectangle, points, orthographic)`: the eight corners into `out`.
///
/// # Safety
/// `v` and `rectangle` are valid; `out` holds eight points.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_frustum_points(
    v: *const GeViewport,
    rectangle: *const GeRectangle,
    orthographic: bool,
    out: *mut Ge3DPoint,
) {
    // SAFETY: the function's contract
    unsafe {
        let points = (*v).frustum_world_points(&*rectangle, orthographic);
        std::ptr::copy_nonoverlapping(points.as_ptr(), out, 8);
    }
}

/// `GEViewport::GetFrustrumWorldPlanes(rectangle, planes, orthographic)`: the six planes into `out`.
///
/// # Safety
/// `v` and `rectangle` are valid; `out` holds six planes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_frustum_planes(
    v: *const GeViewport,
    rectangle: *const GeRectangle,
    orthographic: bool,
    out: *mut Ge3DPlane,
) {
    // SAFETY: the function's contract
    unsafe {
        let planes = (*v).frustum_world_planes(&*rectangle, orthographic);
        std::ptr::copy_nonoverlapping(planes.as_ptr(), out, 6);
    }
}

/// `GEViewport::SetFieldOfView`.
///
/// # Safety
/// `v` is a live viewport.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_set_field_of_view(v: *mut GeViewport, field_of_view: f32) {
    // SAFETY: the function's contract
    unsafe { (*v).set_field_of_view(field_of_view) };
}

/// `GEViewport::SetFrontClippingDistance`.
///
/// # Safety
/// `v` is a live viewport.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_set_front(v: *mut GeViewport, distance: f32) {
    // SAFETY: the function's contract
    unsafe { (*v).set_front_clipping_distance(distance) };
}

/// `GEViewport::SetRearClippingDistance`.
///
/// # Safety
/// `v` is a live viewport.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_set_rear(v: *mut GeViewport, distance: f32) {
    // SAFETY: the function's contract
    unsafe { (*v).set_rear_clipping_distance(distance) };
}

/// `GEViewport::SetViewRectangle`.
///
/// # Safety
/// `v` is a live viewport; `rectangle` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_set_view_rectangle(v: *mut GeViewport, rectangle: *const GeRectangle) {
    // SAFETY: the function's contract (the rectangle is copied first)
    unsafe {
        let r = *rectangle;
        (*v).set_view_rectangle(&r);
    }
}

// ---- GEPhysicalModel ----

/// `GEPhysicalModel::Intersects(viewport, orthographic)` with the placed model's values.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_physical_visible(
    location: *const GeTransformation,
    scale: f32,
    internal: *const GeTransformation,
    center: *const Ge3DPoint,
    radius: f32,
    bbox: *const GeBoundingBox,
    viewport: *const GeViewport,
    orthographic: bool,
) -> bool {
    // SAFETY: the function's contract
    unsafe { physical::model_visible(&*location, scale, &*internal, *center, radius, &*bbox, &*viewport, orthographic) }
}

// ---- UpdateVisibleTerrain ----

/// The plan's arrays, borrowed from the handle until `ee_ge_terrain_visible_free`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct GeVisibleOut {
    /// The visible tile rows, three values each (y, first x, last x).
    pub row_count: usize,
    pub rows: *const std::ffi::c_long,
    pub bottom_left_x: c_ulong,
    pub bottom_left_y: c_ulong,
    pub tile_width: c_ulong,
    pub tile_height: c_ulong,
    pub num_visible_tiles: c_ulong,
    pub visible_poly_count: c_ulong,
    /// `side * side` flags each, `[a][b]` at `a * side + b`.
    pub terrain_visible: *const u8,
    pub water_visible: *const u8,
    /// The terrain materials' textures (0 for none), in list order.
    pub material_count: usize,
    pub materials: *const usize,
    pub terrain_polygon_count: usize,
    pub terrain_polygons: *const GeVisiblePolygon,
    pub water_polygon_count: usize,
    pub water_polygons: *const GeVisiblePolygon,
    pub edge_count: usize,
    pub edge_points: *const Ge3DPoint,
    pub cliff_count: usize,
    pub cliff_points: *const Ge3DPoint,
    /// Per cliff vertex, the terrain render vertex (`a`, `b`) it follows.
    pub cliff_links: *const u32,
}

/// `UpdateVisibleTerrain`'s plan for a viewport: returns the handle and fills `out` with its arrays.
///
/// # Safety
/// `input` describes a live mesh; `viewport` and `out` are valid. The handle must be freed with
/// `ee_ge_terrain_visible_free`, after which `out`'s pointers are dead.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_terrain_visible_plan(
    input: *const GeVisibleInput,
    viewport: *const GeViewport,
    orthographic: bool,
    out: *mut GeVisibleOut,
) -> *mut VisiblePlan {
    // SAFETY: the function's contract
    let plan = Box::new(unsafe { terrain_visible::update_visible_terrain(&*input, &*viewport, orthographic) });
    let view = GeVisibleOut {
        row_count: plan.rows.len() / 3,
        rows: plan.rows.as_ptr(),
        bottom_left_x: plan.bottom_left_x,
        bottom_left_y: plan.bottom_left_y,
        tile_width: plan.tile_width,
        tile_height: plan.tile_height,
        num_visible_tiles: plan.num_visible_tiles,
        visible_poly_count: plan.visible_poly_count,
        terrain_visible: plan.terrain_visible.as_ptr(),
        water_visible: plan.water_visible.as_ptr(),
        material_count: plan.materials.len(),
        materials: plan.materials.as_ptr(),
        terrain_polygon_count: plan.terrain_polygons.len(),
        terrain_polygons: plan.terrain_polygons.as_ptr(),
        water_polygon_count: plan.water_polygons.len(),
        water_polygons: plan.water_polygons.as_ptr(),
        edge_count: plan.edge_points.len(),
        edge_points: plan.edge_points.as_ptr(),
        cliff_count: plan.cliff_points.len(),
        cliff_points: plan.cliff_points.as_ptr(),
        cliff_links: plan.cliff_links.as_ptr(),
    };
    // SAFETY: the function's contract
    unsafe { *out = view };
    Box::into_raw(plan)
}

/// Frees a plan.
///
/// # Safety
/// `plan` came from `ee_ge_terrain_visible_plan` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_terrain_visible_free(plan: *mut VisiblePlan) {
    if !plan.is_null() {
        // SAFETY: the function's contract
        drop(unsafe { Box::from_raw(plan) });
    }
}

// ---- GEBitmap and GERhombus ----

/// `GEBitmap::ComputeRotation`'s corners (top left, top right, bottom left, bottom right) into `out`.
///
/// # Safety
/// `location` is valid; `out` holds four points.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bitmap_corners(
    location: *const Ge2DPoint,
    width: f32,
    height: f32,
    x_scale: f32,
    y_scale: f32,
    rotation: f32,
    out: *mut Ge2DPoint,
) {
    // SAFETY: the function's contract
    unsafe {
        let corners = bitmap::bitmap_corners(*location, width, height, x_scale, y_scale, rotation);
        std::ptr::copy_nonoverlapping(corners.as_ptr(), out, 4);
    }
}

/// `GERhombus::ComputeRotation`'s corners into `out`.
///
/// # Safety
/// `rectangle` is valid; `out` holds four points.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rhombus_corners(rectangle: *const GeRectangle, rotation: f32, out: *mut Ge2DPoint) {
    // SAFETY: the function's contract
    unsafe {
        let corners = bitmap::rhombus_corners(&*rectangle, rotation);
        std::ptr::copy_nonoverlapping(corners.as_ptr(), out, 4);
    }
}

/// `GEBitmap::ComputeStretchedCoordsAndScale`'s coordinates and scale.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bitmap_stretch(
    texture: *const GeRectangle,
    source: *const GeRectangle,
    destination: *const GeRectangle,
    width: c_ulong,
    height: c_ulong,
    drop: c_ulong,
    out: *mut Stretch,
) {
    // SAFETY: the function's contract
    unsafe { *out = bitmap::stretched_coords(&*texture, &*source, &*destination, width, height, drop) };
}
