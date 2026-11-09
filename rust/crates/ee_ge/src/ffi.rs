//! The C ABI of the Graphics Engine's math (`include/rust/ee_rust.h`), called by the C++ members that forward here
//! (`src/Low-Level Engine/Graphics Engine/GE*.cpp`, `Utilities/UMath.cpp`). The pointers are the C++ objects
//! themselves (`this`, in and out arguments, which may be the same object): every function copies its inputs before
//! it writes an output, except where the C++ stores as it goes (`ee_ge_vector_cross`, `ee_ge_t_make_inverse`), which
//! repeat the C++'s order of loads and stores.

use std::ffi::{c_long, c_ulong, c_void};

use crate::io::{CppIo, EeGeFieldIo};
use crate::terrain::{edge_planes, GeTerrainView, GeTmRow};
use crate::terrain_mesh::{self, GeTerrainFog, GeTerrainLoad};
use crate::transform::{make_inverse_raw, RotationOrder};
use crate::types::{
    Ge2DPoint, Ge3DLine, Ge3DPlane, Ge3DPoint, GeBoundingBox, GeOrientation, GeQuaternion, GeRectangle,
    GeTransformation, GeVector,
};
use crate::um;
use crate::vector::{build_quantized_table, cross_product_raw, quantized_index, QUANT_COUNT};

/// Receives one point of an interpolation (`std::vector::push_back` on the C++ side).
pub type EeGePush2D = unsafe extern "C" fn(context: *mut c_void, point: *const Ge2DPoint);
/// Receives one point of a 3D interpolation.
pub type EeGePush3D = unsafe extern "C" fn(context: *mut c_void, point: *const Ge3DPoint);
/// Receives one rectangle of a split.
pub type EeGePushRectangle = unsafe extern "C" fn(context: *mut c_void, rectangle: *const GeRectangle);

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

// ---- UMath ----

/// `UMCos`.
#[no_mangle]
pub extern "C" fn ee_ge_um_cos(angle: f32) -> f32 {
    um::um_cos(angle)
}

/// `UMArcCos`.
#[no_mangle]
pub extern "C" fn ee_ge_um_arccos(value: f32) -> f32 {
    um::um_arccos(value)
}

/// `UMArcTan`.
#[no_mangle]
pub extern "C" fn ee_ge_um_arctan(value: f32) -> f32 {
    um::um_arctan(value)
}

/// `UMArcTan2`.
#[no_mangle]
pub extern "C" fn ee_ge_um_arctan2(y: f32, x: f32) -> f32 {
    um::um_arctan2(y, x)
}

/// `UMTan`.
#[no_mangle]
pub extern "C" fn ee_ge_um_tan(angle: f32) -> f32 {
    um::um_tan(angle)
}

/// `UMQuadraticEquation`: false (roots unwritten) when a and b are 0.
///
/// # Safety
/// `root1` and `root2` are valid for writes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_um_quadratic(a: f32, b: f32, c: f32, root1: *mut f32, root2: *mut f32) -> bool {
    match um::um_quadratic(a, b, c) {
        Some((r1, r2)) => {
            // SAFETY: the function's contract
            unsafe {
                *root1 = r1;
                *root2 = r2;
            }
            true
        }
        None => false,
    }
}

// ---- GE2DPoint, GE3DPoint ----

/// `GE2DPoint::GetDirection`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_2d_direction(p: *const Ge2DPoint, from: *const Ge2DPoint, to: *const Ge2DPoint) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*p).direction(*from, *to) }
}

/// `GE2DPoint::DistanceTo`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_2d_distance(p: *const Ge2DPoint, q: *const Ge2DPoint) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*p).distance_to(*q) }
}

/// `GE2DPoint::DistanceToSquared`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_2d_distance_squared(p: *const Ge2DPoint, q: *const Ge2DPoint) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*p).distance_to_squared(*q) }
}

/// `GE2DPoint::RotatePoint(angle, centre, out)` (and the in-place form with `out == p`).
///
/// # Safety
/// The pointers are valid; they may be equal.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_2d_rotate(
    p: *const Ge2DPoint,
    angle: f32,
    center: *const Ge2DPoint,
    out: *mut Ge2DPoint,
) {
    // SAFETY: the function's contract (inputs copied before the store)
    unsafe {
        let r = (*p).rotated(angle, *center);
        *out = r;
    }
}

/// `GE2DPoint::InterpolateTo`: each point goes to `push` (the caller clears its vector first).
///
/// # Safety
/// The pointers are valid; `push` may be called with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_2d_interpolate(
    from: *const Ge2DPoint,
    to: *const Ge2DPoint,
    step: f32,
    push: EeGePush2D,
    context: *mut c_void,
) {
    // SAFETY: the function's contract; Rust holds no reference into C++ memory across `push`
    unsafe {
        let (from, to) = (*from, *to);
        from.interpolate_to(to, step, |point| push(context, &point));
    }
}

/// `GE3DPoint::DistanceTo`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_3d_distance(p: *const Ge3DPoint, q: *const Ge3DPoint) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*p).distance_to(*q) }
}

/// `GE3DPoint::DistanceToSquared`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_3d_distance_squared(p: *const Ge3DPoint, q: *const Ge3DPoint) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*p).distance_to_squared(*q) }
}

/// `GE3DPoint::InterpolateTo`.
///
/// # Safety
/// The pointers are valid; `push` may be called with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_3d_interpolate(
    from: *const Ge3DPoint,
    to: *const Ge3DPoint,
    step: f32,
    push: EeGePush3D,
    context: *mut c_void,
) {
    // SAFETY: the function's contract; Rust holds no reference into C++ memory across `push`
    unsafe {
        let (from, to) = (*from, *to);
        from.interpolate_to(to, step, |point| push(context, &point));
    }
}

// ---- GEVector ----

/// `GEVector::GetMagnitude`.
///
/// # Safety
/// `v` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_vector_magnitude(v: *const GeVector) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*v).magnitude() }
}

/// `GEVector::GetMagnitudeSquared`.
///
/// # Safety
/// `v` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_vector_magnitude_squared(v: *const GeVector) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*v).magnitude_squared() }
}

/// `GEVector::CrossProduct(in, out)`, storing each component before computing the next as the C++ does.
///
/// # Safety
/// The pointers are valid; any of them may be equal.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_vector_cross(v: *const GeVector, other: *const GeVector, out: *mut GeVector) {
    // SAFETY: the function's contract
    unsafe { cross_product_raw(v, other, out) }
}

/// Fills `QuantizedVectorArray::mVectors` (`count` must be 41 * 246).
///
/// # Safety
/// `out` is valid for `count` vectors.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_vector_quantized_table(out: *mut GeVector, count: usize) {
    assert_eq!(count, QUANT_COUNT, "the quantized-vector table has 41 x 246 entries");
    // SAFETY: the function's contract
    let table = unsafe { std::slice::from_raw_parts_mut(out, count) };
    build_quantized_table(table);
}

/// `GEVector::GetQuantizedVersion` as an index into the table, -1 for the zero vector, -2 for no face
/// (`vector::QUANT_ZERO`, `vector::QUANT_NONE`).
///
/// # Safety
/// `v` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_vector_quantized_index(v: *const GeVector) -> i32 {
    // SAFETY: the function's contract
    quantized_index(unsafe { *v })
}

// ---- GE3DLine, GE3DPlane ----

/// `GE3DLine::Intersects(point, tolerance, t)`: `t` written only on a hit.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_line_intersects_point(
    line: *const Ge3DLine,
    point: *const Ge3DPoint,
    tolerance: f32,
    t: *mut f32,
) -> bool {
    // SAFETY: the function's contract
    unsafe { put(t, (*line).intersects_point(*point, tolerance)) }
}

/// `GE3DLine::Intersects(line, t, lineT)`: the parameters are written whenever a branch computed them.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_line_intersects_line(
    line: *const Ge3DLine,
    other: *const Ge3DLine,
    t: *mut f32,
    line_t: *mut f32,
) -> bool {
    // SAFETY: the function's contract
    unsafe {
        let hit = (*line).intersects_line(&*other);
        if let Some((a, b)) = hit.params {
            *t = a;
            *line_t = b;
        }
        hit.hit
    }
}

/// `GE3DLine::IntersectsIn2D`: the parameters written only when the lines are not parallel.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_line_intersects_2d(
    line: *const Ge3DLine,
    other: *const Ge3DLine,
    t: *mut f32,
    line_t: *mut f32,
) -> bool {
    // SAFETY: the function's contract
    unsafe {
        match (*line).intersects_in_2d(&*other) {
            Some((a, b)) => {
                *t = a;
                *line_t = b;
                true
            }
            None => false,
        }
    }
}

/// `GE3DLine::Intersects(plane, t)`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_line_intersects_plane(
    line: *const Ge3DLine,
    plane: *const Ge3DPlane,
    t: *mut f32,
) -> bool {
    // SAFETY: the function's contract
    unsafe { put(t, (*line).intersects_plane(&*plane)) }
}

/// `GE3DLine::SolveForX/Y/Z` (`axis` 0, 1, 2), with the point when `point` is not null; nothing written when the
/// line runs parallel to the axis.
///
/// # Safety
/// `line` and `t` are valid; `point` is null or valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_line_solve(
    line: *const Ge3DLine,
    axis: u32,
    value: f32,
    t: *mut f32,
    point: *mut Ge3DPoint,
) -> bool {
    // SAFETY: the function's contract
    unsafe {
        let line = *line;
        if point.is_null() {
            let solved = match axis {
                0 => line.solve_for_x(value),
                1 => line.solve_for_y(value),
                _ => line.solve_for_z(value),
            };
            put(t, solved)
        } else {
            let solved = match axis {
                0 => line.solve_for_x_point(value),
                1 => line.solve_for_y_point(value),
                _ => line.solve_for_z_point(value),
            };
            match solved {
                Some((a, p)) => {
                    *t = a;
                    *point = p;
                    true
                }
                None => false,
            }
        }
    }
}

/// `GE3DPlane::Define(p1, p2, p3)`.
///
/// # Safety
/// The pointers are valid; `out` may be the storage of an input.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_plane_define(
    p1: *const Ge3DPoint,
    p2: *const Ge3DPoint,
    p3: *const Ge3DPoint,
    out: *mut Ge3DPlane,
) {
    // SAFETY: the function's contract (inputs copied before the store)
    unsafe {
        let plane = Ge3DPlane::through(*p1, *p2, *p3);
        *out = plane;
    }
}

/// `GE3DPlane::Intersects(plane, line)`: the line written only when the planes are not parallel.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_plane_intersects(
    plane: *const Ge3DPlane,
    other: *const Ge3DPlane,
    out: *mut Ge3DLine,
) -> bool {
    // SAFETY: the function's contract
    unsafe { put(out, (*plane).intersects(&*other)) }
}

/// `GE3DPlane::IsPoint*`: `test` 0 above, 1 below, 2 on or above, 3 on or below, 4 on.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_plane_side(plane: *const Ge3DPlane, point: *const Ge3DPoint, test: u32) -> bool {
    // SAFETY: the function's contract
    let (plane, point) = unsafe { (*plane, *point) };
    match test {
        0 => plane.is_point_above(point),
        1 => plane.is_point_below(point),
        2 => plane.is_point_on_or_above(point),
        3 => plane.is_point_on_or_below(point),
        _ => plane.is_point_on(point),
    }
}

// ---- GETransformation, GEOrientation, GEQuaternion ----

/// `GETransformation::Apply(transformation, out)`.
///
/// # Safety
/// The pointers are valid; `out` may equal either input.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_compose(
    a: *const GeTransformation,
    b: *const GeTransformation,
    out: *mut GeTransformation,
) {
    // SAFETY: the function's contract (inputs copied before the store)
    unsafe {
        let c = (*a).compose(&*b);
        *out = c;
    }
}

/// `GETransformation::Apply(line, out)`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_apply_line(t: *const GeTransformation, line: *const Ge3DLine, out: *mut Ge3DLine) {
    // SAFETY: the function's contract
    unsafe {
        let l = (*t).apply_line(&*line);
        *out = l;
    }
}

/// `GETransformation::Apply(plane, out)`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_apply_plane(t: *const GeTransformation, plane: *const Ge3DPlane, out: *mut Ge3DPlane) {
    // SAFETY: the function's contract
    unsafe {
        let p = (*t).apply_plane(&*plane);
        *out = p;
    }
}

/// `GETransformation::ConvertToOrientation`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_orientation(t: *const GeTransformation, out: *mut GeOrientation) {
    // SAFETY: the function's contract
    unsafe {
        let o = (*t).orientation();
        *out = o;
    }
}

/// `GETransformation::GetYaw`.
///
/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_yaw(t: *const GeTransformation) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*t).yaw() }
}

/// `GETransformation::GetPitch`.
///
/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_pitch(t: *const GeTransformation) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*t).pitch() }
}

/// `GETransformation::GetRoll`.
///
/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_roll(t: *const GeTransformation) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*t).roll() }
}

/// `GETransformation::DistanceTo(point)`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_distance_to_point(t: *const GeTransformation, p: *const Ge3DPoint) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*t).distance_to_point(*p) }
}

/// `GETransformation::DistanceSquaredTo(point)`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_distance_squared_to_point(t: *const GeTransformation, p: *const Ge3DPoint) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*t).distance_squared_to_point(*p) }
}

/// `GETransformation::DistanceTo(transformation)`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_distance_to(t: *const GeTransformation, other: *const GeTransformation) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*t).distance_to(&*other) }
}

/// `GETransformation::DistanceSquaredTo(transformation)`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_distance_squared_to(
    t: *const GeTransformation,
    other: *const GeTransformation,
) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*t).distance_squared_to(&*other) }
}

/// `GETransformation::MakeInverse(out)`, in the C++'s store order (an output equal to `t` included).
///
/// # Safety
/// The pointers are valid; they may be equal.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_make_inverse(t: *const GeTransformation, out: *mut GeTransformation) {
    // SAFETY: the function's contract
    unsafe { make_inverse_raw(t, out) }
}

/// `GETransformation::MakeLookAtTransform(eye, target)`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_make_look_at(
    t: *mut GeTransformation,
    eye: *const Ge3DPoint,
    target: *const Ge3DPoint,
) {
    // SAFETY: the function's contract (inputs copied before the store)
    unsafe {
        let (eye, target) = (*eye, *target);
        let mut m = *t;
        m.make_look_at(eye, target);
        *t = m;
    }
}

/// `GETransformation::Normalize`.
///
/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_normalize(t: *mut GeTransformation) {
    // SAFETY: the function's contract
    unsafe { (*t).normalize() }
}

/// `GETransformation::Revolve(yaw, pitch, roll)`.
///
/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_revolve(t: *mut GeTransformation, yaw: f32, pitch: f32, roll: f32) {
    // SAFETY: the function's contract
    unsafe { (*t).revolve(yaw, pitch, roll) }
}

/// `GETransformation::Rotate(yaw, pitch, roll)`.
///
/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_rotate(t: *mut GeTransformation, yaw: f32, pitch: f32, roll: f32) {
    // SAFETY: the function's contract
    unsafe { (*t).rotate(yaw, pitch, roll) }
}

/// `GETransformation::Scale(x, y, z)`.
///
/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_scale(t: *mut GeTransformation, x: f32, y: f32, z: f32) {
    // SAFETY: the function's contract
    unsafe { (*t).scale(x, y, z) }
}

/// `GETransformation::SetOrientation{PRY,PYR,RPY,RYP,YPR,YRP}` (`order` 0..5 in that order).
///
/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_t_set_orientation(
    t: *mut GeTransformation,
    order: u32,
    yaw: f32,
    pitch: f32,
    roll: f32,
) {
    let order = match order {
        0 => RotationOrder::Pry,
        1 => RotationOrder::Pyr,
        2 => RotationOrder::Rpy,
        3 => RotationOrder::Ryp,
        4 => RotationOrder::Ypr,
        _ => RotationOrder::Yrp,
    };
    // SAFETY: the function's contract
    unsafe { (*t).set_orientation(order, yaw, pitch, roll) }
}

/// `GEOrientation::Normalize`.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_orientation_normalize(o: *mut GeOrientation) {
    // SAFETY: the function's contract
    unsafe { (*o).normalize() }
}

/// `GEQuaternion::ConvertFrom(transformation)`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_quat_from(t: *const GeTransformation, out: *mut GeQuaternion) {
    // SAFETY: the function's contract
    unsafe {
        let q = GeQuaternion::from_transformation(&*t);
        *out = q;
    }
}

/// `GEQuaternion::ConvertTo(transformation)`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_quat_to(q: *const GeQuaternion, out: *mut GeTransformation) {
    // SAFETY: the function's contract
    unsafe {
        let t = (*q).to_transformation();
        *out = t;
    }
}

/// `GEQuaternion::Normalize`.
///
/// # Safety
/// `q` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_quat_normalize(q: *mut GeQuaternion) {
    // SAFETY: the function's contract
    unsafe { (*q).normalize() }
}

/// `GEQuaternion::Slerp(target, t, out)`.
///
/// # Safety
/// The pointers are valid; `out` may equal either input.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_quat_slerp(
    q: *const GeQuaternion,
    target: *const GeQuaternion,
    t: f32,
    out: *mut GeQuaternion,
) {
    // SAFETY: the function's contract (inputs copied before the store)
    unsafe {
        let r = (*q).slerp(&*target, t);
        *out = r;
    }
}

// ---- GEBoundingBox, GERectangle ----

/// `GEBoundingBox::ComputeDimensions`.
///
/// # Safety
/// `b` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bbox_compute_dimensions(b: *mut GeBoundingBox) {
    // SAFETY: the function's contract
    unsafe { (*b).compute_dimensions() }
}

/// `GEBoundingBox::Contains`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bbox_contains(b: *const GeBoundingBox, p: *const Ge3DPoint) -> bool {
    // SAFETY: the function's contract
    unsafe { (*b).contains(*p) }
}

/// `GEBoundingBox::Intersects(box)`.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bbox_intersects(b: *const GeBoundingBox, other: *const GeBoundingBox) -> bool {
    // SAFETY: the function's contract
    unsafe { (*b).intersects(&*other) }
}

/// `GEBoundingBox::Intersects(line, distance)`: the distance is always written.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bbox_intersects_line(
    b: *const GeBoundingBox,
    line: *const Ge3DLine,
    distance: *mut f32,
) -> bool {
    // SAFETY: the function's contract
    unsafe {
        let (hit, d) = (*b).intersects_line(&*line);
        *distance = d;
        hit
    }
}

/// `GEBoundingBox::Transform(transformation, out)`.
///
/// # Safety
/// The pointers are valid; `out` may equal `b`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bbox_transform(
    b: *const GeBoundingBox,
    t: *const GeTransformation,
    out: *mut GeBoundingBox,
) {
    // SAFETY: the function's contract (inputs copied before the store)
    unsafe {
        let o = (*b).transform(&*t);
        *out = o;
    }
}

/// `GERectangle::And(rectangle)`.
///
/// # Safety
/// The pointers are valid; they may be equal.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rect_and_in_place(r: *mut GeRectangle, other: *const GeRectangle) -> bool {
    // SAFETY: the function's contract (inputs copied before the store)
    unsafe {
        let other = *other;
        let mut rect = *r;
        let result = rect.and_in_place(&other);
        *r = rect;
        result
    }
}

/// `GERectangle::And(rectangle, out)`: `out` written only when they overlap.
///
/// # Safety
/// The pointers are valid; `out` may equal either input.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rect_and(
    r: *const GeRectangle,
    other: *const GeRectangle,
    out: *mut GeRectangle,
) -> bool {
    // SAFETY: the function's contract (inputs copied before the store)
    unsafe { put(out, (*r).and(&*other)) }
}

/// `GERectangle::RemoveIntersectingRectangle`: each piece goes to `push`; `existing` is the output vector's size
/// before the call.
///
/// # Safety
/// The pointers are valid; `push` may be called with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_rect_remove_intersecting(
    r: *const GeRectangle,
    other: *const GeRectangle,
    existing: usize,
    push: EeGePushRectangle,
    context: *mut c_void,
) {
    // SAFETY: the function's contract; Rust holds no reference into C++ memory across `push`
    unsafe {
        let (r, other) = (*r, *other);
        r.remove_intersecting(&other, existing, |piece| push(context, &piece));
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

// ---- GETerrainMesh's edits, fog of war and file codec ----

/// `GETerrainMesh::ComputeVertexNormal` before the quantization (the C++ stores `GetQuantizedVersion()` of `out`).
///
/// # Safety
/// `point_rows` holds `point_row_count` live rows, the point exists and `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_terrain_vertex_normal(
    point_rows: *const GeTmRow,
    point_row_count: c_ulong,
    x: c_ulong,
    y: c_ulong,
    out: *mut GeVector,
) {
    // SAFETY: the function's contract
    unsafe { *out = terrain_mesh::vertex_normal(point_rows, point_row_count, x, y) };
}

/// `GETerrainMesh::ComputeShorelines`: the tile's new shoreline bits for `SetWaterInfo`, or -1 to leave it alone.
///
/// # Safety
/// `view` describes a live mesh and the tile exists.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_terrain_shoreline(
    view: *const GeTerrainView,
    space_map: bool,
    x: c_ulong,
    y: c_ulong,
) -> i32 {
    // SAFETY: the function's contract
    unsafe { (*view).shoreline(space_map, x, y).map_or(-1, i32::from) }
}

/// `GETerrainMesh::UpdateFogOfWar`: writes the visible points' visibility and the two shares; false (nothing
/// written) when no tile row is visible.
///
/// # Safety
/// `view` and `fog` describe a live mesh; the out pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_terrain_fog(
    view: *const GeTerrainView,
    fog: *const GeTerrainFog,
    out_water: *mut f32,
    out_space: *mut f32,
) -> bool {
    // SAFETY: the function's contract
    unsafe {
        match terrain_mesh::fog_of_war(&*view, &*fog) {
            Some((water, space)) => {
                *out_water = water;
                *out_space = space;
                true
            }
            None => false,
        }
    }
}

/// `GETerrainMesh::Save`.
///
/// # Safety
/// `io` is a live field stream; `view` and `point_rows` describe a live mesh.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_terrain_save(
    io: *const EeGeFieldIo,
    view: *const GeTerrainView,
    point_row_count: c_ulong,
    width: c_long,
    height: c_long,
    fractal_seed: c_ulong,
) -> c_long {
    // SAFETY: the function's contract
    unsafe { terrain_mesh::save(&mut CppIo(*io), &*view, point_row_count, width, height, fractal_seed) }
}

/// `GETerrainMesh::Load`'s map size (each read through the member's old value).
///
/// # Safety
/// `io` is a live field stream; the pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_terrain_load_size(
    io: *const EeGeFieldIo,
    width: *mut c_long,
    height: *mut c_long,
) -> c_long {
    // SAFETY: the function's contract
    unsafe { terrain_mesh::load_size(&mut CppIo(*io), &mut *width, &mut *height) }
}

/// `GETerrainMesh::Load` after `CreateFlatMesh`: the tiles, the points and the fractal seed. `valid_sets[id]` is
/// nonzero for a registered texture set.
///
/// # Safety
/// `io` is a live field stream; `view` and `point_rows` describe a live, writable mesh; `valid_sets` holds 256
/// bytes; `state` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_terrain_load_body(
    io: *const EeGeFieldIo,
    view: *const GeTerrainView,
    point_row_count: c_ulong,
    valid_sets: *const u8,
    state: *mut GeTerrainLoad,
) -> c_long {
    // SAFETY: the function's contract
    unsafe {
        terrain_mesh::load_body(
            &mut CppIo(*io),
            &*view,
            point_row_count,
            &*(valid_sets as *const [u8; 256]),
            &mut *state,
        )
    }
}

