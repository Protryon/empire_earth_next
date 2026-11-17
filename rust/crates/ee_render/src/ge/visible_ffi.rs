//! The C ABI of `GEViewport`'s math (`viewport.rs`), `GEPhysicalModel::Intersects(viewport)` (`physical.rs`).
//! The viewport pointer is the C++ object itself (`GERustViewport.h` pins the layout).
//! (`UpdateVisibleTerrain`'s plan is the terrain mesh's own, `tmobj.rs`.)

use crate::ge::physical;
use crate::ge::viewport::GeViewport;
use ee_base::math::types::{Ge3DPlane, Ge3DPoint, GeBoundingBox, GeRectangle, GeTransformation};

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
