//! The C ABI of `GEViewport`'s math (`viewport.rs`). The viewport pointer is the C++ object itself (lle/GERust.h; the
//! layout is pinned in `GERustViewport.h`).

use crate::geval::viewport::GeViewport;
use ee_base::math::ffi::EeGePush3D;
use ee_base::math::types::{Ge3DPlane, Ge3DPoint, GeRectangle, GeTransformation};
use std::ffi::c_void;

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

/// `GEViewport::GetFrustrumWorldPoints(rectangle, points, orthographic)`: the eight corners appended through `push`.
///
/// # Safety
/// `v` and `rectangle` are valid; `push` may be called with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_frustum_points_push(
    v: *const GeViewport,
    rectangle: *const GeRectangle,
    orthographic: bool,
    push: EeGePush3D,
    context: *mut c_void,
) {
    // SAFETY: the function's contract; the points are computed before the first push
    unsafe {
        let points = (*v).frustum_world_points(&*rectangle, orthographic);
        for point in &points {
            push(context, point);
        }
    }
}

/// `GEViewport::DenormalizePerspectivePoint`.
///
/// # Safety
/// The pointers are valid (`out` may be `p`).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_denormalize_perspective(
    v: *const GeViewport,
    p: *const Ge3DPoint,
    w: f32,
    out: *mut Ge3DPoint,
) {
    // SAFETY: the function's contract
    unsafe { *out = (*v).denormalize_perspective_point(*p, w) };
}

/// `GEViewport::NormalizeOrthographicCameraPoint` (`perspective` false) and `NormalizePerspectiveCameraPoint`: the
/// camera-space point scaled to the screen (x, z) and the depth range (y); `out_w` 1 or the point's y.
///
/// # Safety
/// The pointers are valid (`out` may be `p`).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_normalize_camera_point(
    v: *const GeViewport,
    p: *const Ge3DPoint,
    perspective: bool,
    out: *mut Ge3DPoint,
    out_w: *mut f32,
) {
    // SAFETY: the function's contract
    unsafe {
        let (point, w) = (*v).normalize_camera_point(*p, perspective);
        *out = point;
        *out_w = w;
    }
}

/// `GEViewport::SetCamera`.
///
/// # Safety
/// `v` is a live viewport; `camera` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_set_camera(v: *mut GeViewport, camera: *const GeTransformation) {
    // SAFETY: the function's contract (the camera is copied first)
    unsafe {
        let c = *camera;
        (*v).set_camera(&c);
    }
}

/// `GEViewport::SetOrthographicRectangle`.
///
/// # Safety
/// `v` is a live viewport; `rectangle` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_set_orthographic_rectangle(v: *mut GeViewport, rectangle: *const GeRectangle) {
    // SAFETY: the function's contract (the rectangle is copied first)
    unsafe {
        let r = *rectangle;
        (*v).set_orthographic_rectangle(&r);
    }
}

/// `GEViewport::SetDepthRange`.
///
/// # Safety
/// `v` is a live viewport.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_viewport_set_depth_range(v: *mut GeViewport, near: f32, far: f32) {
    // SAFETY: the function's contract
    unsafe {
        (*v).near_depth = near;
        (*v).far_depth = far;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe extern "C" fn collect(context: *mut c_void, point: *const Ge3DPoint) {
        // SAFETY: the test passes a vector and valid points
        unsafe { (*(context as *mut Vec<Ge3DPoint>)).push(*point) };
    }

    #[test]
    fn the_pushed_frustum_points_are_the_eight_corners_in_order() {
        let r = GeRectangle { bottom: 100.0, left: 0.0, right: 200.0, top: 0.0 };
        let mut v = GeViewport::default();
        // SAFETY: a live viewport and a valid rectangle
        unsafe { ee_ge_viewport_set_view_rectangle(&mut v, &r) };
        let mut got: Vec<Ge3DPoint> = Vec::new();
        // SAFETY: valid pointers and a matching context
        unsafe { ee_ge_viewport_frustum_points_push(&v, &r, false, collect, (&mut got as *mut Vec<Ge3DPoint>).cast()) };
        let bits =
            |p: &[Ge3DPoint]| p.iter().map(|q| [q.x.to_bits(), q.y.to_bits(), q.z.to_bits()]).collect::<Vec<_>>();
        assert_eq!(bits(&got), bits(&v.frustum_world_points(&r, false)));
        assert!(got.iter().all(|q| q.x.is_finite() && q.y.is_finite() && q.z.is_finite()));
    }

    #[test]
    fn camera_points_normalize_per_projection() {
        let mut v = GeViewport {
            orthographic_scale: 2.0,
            orthographic_depth_scale: 3.0,
            orthographic_depth_offset: 0.5,
            perspective_scale: 4.0,
            perspective_depth_scale: 5.0,
            perspective_depth_offset: -1.0,
            ..GeViewport::default()
        };
        let p = Ge3DPoint::new(1.0, 2.0, 3.0);
        let (mut out, mut w) = (Ge3DPoint::default(), 0.0f32);
        // SAFETY: valid pointers
        unsafe { ee_ge_viewport_normalize_camera_point(&v, &p, false, &mut out, &mut w) };
        assert_eq!((out, w), (Ge3DPoint::new(2.0, 6.0, 6.5), 1.0));
        // SAFETY: valid pointers
        unsafe { ee_ge_viewport_normalize_camera_point(&v, &p, true, &mut out, &mut w) };
        assert_eq!((out, w), (Ge3DPoint::new(4.0, 12.0, 9.0), 2.0));
        // SAFETY: valid pointers
        unsafe { ee_ge_viewport_set_depth_range(&mut v, 0.25, 0.75) };
        assert_eq!((v.near_depth, v.far_depth), (0.25, 0.75));
    }
}
