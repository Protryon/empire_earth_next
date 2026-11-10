//! `GEPhysicalModel`'s walks (`GEPhysicalModel.cpp`): a line against a placed model and its sub-models (bounding
//! sphere, box, triangles: 0x10020e84), a model scaled per axis against this placed model (0x10021404) and this
//! placed model against another model (bounding spheres, boxes, triangle boxes at a reduced LOD: 0x1002179e).
//!
//! The models stay C++ (`GEModel` with its materials, LODs, sub-models and the vertex and polygon arrays behind
//! virtuals): the walks read them through [`Models`], which the C++ fills with a callback table
//! ([`EeGePhysicalModels`]) over `GEModel*` handles. Units test projectile lines against target models with these
//! (`EEUnitClass.cpp`), so this is simulation code (RUST_PLAN 2.1): the C++'s `f32` operations in its order, its
//! `unsigned long` index arithmetic (done by the C++ callbacks where it indexes), its `max`/`min` macros, the
//! distance written exactly where the C++ wrote its reference.

use std::ffi::{c_ulong, c_void};

use crate::physical::{line_hits_triangle, triangle_box};
use crate::types::{Ge3DLine, Ge3DPoint, GeBoundingBox, GeTransformation};
use crate::um::lrintf;

/// `kLODFraction` (0x10097b78).
const LOD_FRACTION: f32 = 0.2;

/// A material's ranges at one LOD: `GetVertexStartingIndex()`, `GetPolygonStartingIndex(lod)`,
/// `GetPolygonRangeLength(lod)`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GePhysicalMaterial {
    pub vertex_start: c_ulong,
    pub polygon_start: c_ulong,
    pub polygon_length: c_ulong,
}

/// What the walks read of a model (`GEModel`'s accessors).
pub trait Models {
    type M: Copy;
    fn is_null(&self, m: Self::M) -> bool;
    fn has_parent(&self, m: Self::M) -> bool;
    fn internal_transform(&self, m: Self::M, frame: c_ulong) -> GeTransformation;
    fn center(&self, m: Self::M) -> Ge3DPoint;
    fn radius(&self, m: Self::M, frame: c_ulong) -> f32;
    fn bbox(&self, m: Self::M, frame: c_ulong) -> GeBoundingBox;
    fn material_count(&self, m: Self::M) -> c_ulong;
    fn material(&self, m: Self::M, index: c_ulong, lod: c_ulong) -> GePhysicalMaterial;
    /// The three corners of polygon `polygon` of LOD `lod` in frame `frame`'s vertex array (the polygon array's
    /// vertex indices plus `vertex_start`, as the C++ adds them).
    fn triangle(
        &self,
        m: Self::M,
        frame: c_ulong,
        lod: c_ulong,
        polygon: c_ulong,
        vertex_start: c_ulong,
    ) -> [Ge3DPoint; 3];
    fn sub_model_count(&self, m: Self::M) -> c_ulong;
    fn sub_model(&self, m: Self::M, index: c_ulong) -> Self::M;
    fn lod_count(&self, m: Self::M) -> c_ulong;
    /// `GetPolygonCount(lod)`: the materials' range lengths summed.
    fn polygon_count(&self, m: Self::M, lod: c_ulong) -> c_ulong;
    /// The scaled walk's exact retry: a `GEPhysicalModel(model, location, scale)` at `frame` asked
    /// `Intersects(line, lod, distance, true)`. The C++ builds that placed model (it references and releases the
    /// model, which may reach its manager), so it stays a call into the host.
    #[allow(clippy::too_many_arguments)]
    fn retry_line(
        &self,
        location: &GeTransformation,
        model: Self::M,
        frame: c_ulong,
        scale: f32,
        line: &Ge3DLine,
        lod: c_ulong,
    ) -> bool;
}

/// The placed model the walks run on (`this`): location, model, scale, frame.
#[derive(Clone, Copy, Debug)]
pub struct Placed<M> {
    pub location: GeTransformation,
    pub model: M,
    pub scale: f32,
    pub frame: c_ulong,
}

fn nonzero(v: f32) -> bool {
    v < -0.0001 || v > 0.0001
}

/// `location x internal(frame)`, then for a top model (no parent) scaled by `scale` (unless 1) with the location's
/// translation put back.
fn placed_transform<A: Models>(
    a: &A,
    location: &GeTransformation,
    model: A::M,
    frame: c_ulong,
    scale: (f32, f32, f32),
) -> GeTransformation {
    let mut t = location.compose(&a.internal_transform(model, frame));
    if !a.has_parent(model) && (scale.0 != 1.0 || scale.1 != 1.0 || scale.2 != 1.0) {
        t.scale(scale.0, scale.1, scale.2);
        for i in 0..3 {
            t.m[i][3] = location.m[i][3];
        }
    }
    t
}

/// `Intersects(location, model, frame, line, lod, distance, exact)` with `scale` the placed model's: the bounding
/// sphere, then (LOD > 0) the box, then (LOD > 1) every triangle of LOD 0; then the sub-models until a hit. With
/// `exact` each hit must lie on the segment (0..1). `distance` is written where the C++ wrote its reference.
#[allow(clippy::too_many_arguments)]
pub fn line_walk<A: Models>(
    a: &A,
    scale: f32,
    location: &GeTransformation,
    model: A::M,
    frame: c_ulong,
    line: &Ge3DLine,
    lod: c_ulong,
    distance: &mut f32,
    exact: bool,
) -> bool {
    let mut result = false;
    if !nonzero(scale) || a.is_null(model) {
        return result;
    }
    let transform = placed_transform(a, location, model, frame, (scale, scale, scale));
    let center = transform.apply_point(a.center(model));
    match line.intersects_point(center, a.radius(model, frame) * scale) {
        Some(d) => {
            *distance = d;
            result = true;
        }
        None => result = false,
    }
    if exact {
        result = result && *distance >= 0.0 && *distance <= 1.0;
    }
    if result && lod > 0 {
        let b = a.bbox(model, frame).transform(&transform);
        let (hit, d) = b.intersects_line(line);
        *distance = d;
        result = hit;
        if exact {
            result = result && *distance >= 0.0 && *distance <= 1.0;
        }
        if result && lod > 1 {
            result = false;
            let count = a.material_count(model);
            let mut i: c_ulong = 0;
            while i < count && !result {
                let mat = a.material(model, i, 0);
                let end = mat.polygon_start.wrapping_add(mat.polygon_length);
                let mut j = mat.polygon_start;
                while j < end && !result {
                    let [p1, p2, p3] = a.triangle(model, frame, 0, j, mat.vertex_start);
                    let (hit, d) = line_hits_triangle(
                        line,
                        transform.apply_point(p1),
                        transform.apply_point(p2),
                        transform.apply_point(p3),
                        exact,
                    );
                    if let Some(d) = d {
                        *distance = d;
                    }
                    result = hit;
                    j = j.wrapping_add(1);
                }
                i = i.wrapping_add(1);
            }
        }
    }
    let subs = a.sub_model_count(model);
    let mut k: c_ulong = 0;
    while k < subs && !result {
        result = line_walk(a, scale, &transform, a.sub_model(model, k), frame, line, lod, distance, exact);
        k = k.wrapping_add(1);
    }
    result
}

/// `max` and `min` (the Windows macros the C++ expands): `a > b ? a : b`, `a < b ? a : b`.
fn max_macro(a: f32, b: f32) -> f32 {
    if a > b {
        a
    } else {
        b
    }
}
fn min_macro(a: f32, b: f32) -> f32 {
    if a < b {
        a
    } else {
        b
    }
}

/// `Intersects(location, model, frame, line, exact, lod, x, y, z)`: `model` at `location`, scaled per axis, against
/// the placed model `this` (its inverse built from the scaled transform, the largest scale as the other's sphere
/// scale), then its sub-models; a top model with no hit in exact mode retries as a line test at the smallest scale.
#[allow(clippy::too_many_arguments)]
pub fn scaled_walk<A: Models>(
    a: &A,
    this: &Placed<A::M>,
    location: &GeTransformation,
    model: A::M,
    frame: c_ulong,
    line: &Ge3DLine,
    exact: bool,
    lod: c_ulong,
    x: f32,
    y: f32,
    z: f32,
) -> bool {
    let subs = a.sub_model_count(model);
    let mut result = false;
    if nonzero(this.scale) && nonzero(x) && nonzero(y) && nonzero(z) {
        let t = placed_transform(a, location, model, frame, (x, y, z));
        let ix = 1.0 / (x * x);
        let iy = 1.0 / (y * y);
        let iz = 1.0 / (z * z);
        let mut inverse = GeTransformation::default();
        inverse.m[0] = [t.m[0][0] * ix, t.m[1][0] * iy, t.m[2][0] * iz, 0.0];
        inverse.m[1] = [t.m[0][1] * ix, t.m[1][1] * iy, t.m[2][1] * iz, 0.0];
        inverse.m[2] = [t.m[0][2] * ix, t.m[1][2] * iy, t.m[2][2] * iz, 0.0];
        inverse.m[3] = [0.0, 0.0, 0.0, 1.0];
        let origin = inverse.apply_point(Ge3DPoint::new(-t.m[0][3], -t.m[1][3], -t.m[2][3]));
        inverse.m[0][3] = origin.x;
        inverse.m[1][3] = origin.y;
        inverse.m[2][3] = origin.z;
        let maximum = max_macro(x, y);
        result =
            model_walk(a, this, &this.location, this.model, &t, &inverse, max_macro(maximum, z), model, frame, lod);
        let mut i: c_ulong = 0;
        while !result && i < subs {
            result = scaled_walk(a, this, &t, a.sub_model(model, i), frame, line, exact, lod, x, y, z);
            i = i.wrapping_add(1);
        }
    }
    if result || !exact || a.has_parent(model) {
        return result;
    }
    let mut s = x;
    s = min_macro(y, s);
    s = min_macro(z, s);
    if nonzero(s) {
        result = a.retry_line(location, model, frame, s, line, lod);
    }
    result
}

/// The LOD the triangle-box test reads: 80 % of the LOD count (`UMFloatToLong`), the last when that is the count,
/// then down while it has fewer than 50 polygons.
fn reduced_lod<A: Models>(a: &A, m: A::M) -> c_ulong {
    let count = a.lod_count(m);
    let mut lod = lrintf((1.0f32 - LOD_FRACTION) * count as f32) as c_ulong;
    if lod == count {
        lod = count.wrapping_sub(1);
    }
    while lod > 0 && a.polygon_count(m, lod) < 50 {
        lod -= 1;
    }
    lod
}

/// `Intersects(location, model, otherLocation, otherInverse, scale, otherModel, frame, lod)`: this placed model's
/// `model` (at this model's frame and scale) against `other` (at `other_frame`, sphere scale `other_scale`): the
/// bounding spheres, then (LOD > 0) this box in the other's space against the other's box, then (LOD > 1) every
/// triangle's box at the reduced LODs against the other's triangle boxes (computed during the first triangle);
/// then this model's sub-models until a hit.
#[allow(clippy::too_many_arguments)]
pub fn model_walk<A: Models>(
    a: &A,
    this: &Placed<A::M>,
    location: &GeTransformation,
    model: A::M,
    other_location: &GeTransformation,
    other_inverse: &GeTransformation,
    other_scale: f32,
    other: A::M,
    other_frame: c_ulong,
    lod: c_ulong,
) -> bool {
    let subs = a.sub_model_count(model);
    let mut first = true;
    let mut result;
    let frame = this.frame;
    let transform = placed_transform(a, location, model, frame, (this.scale, this.scale, this.scale));
    let relative = other_inverse.compose(&transform);
    let radius = a.radius(model, frame) * this.scale;
    let point = transform.apply_point(a.center(model));
    let mut distance = a.radius(other, other_frame) * other_scale;
    let other_point = other_location.apply_point(a.center(other));
    distance = distance + radius;
    if point.distance_to_squared(other_point) > distance * distance {
        result = false;
    } else {
        result = true;
        if lod > 0 {
            let b = a.bbox(model, frame).transform(&relative);
            result = a.bbox(other, other_frame).intersects(&b);
            if result && lod > 1 {
                let this_lod = reduced_lod(a, model);
                let other_lod = reduced_lod(a, other);
                let mut boxes: Vec<Vec<GeBoundingBox>> = Vec::new();
                result = false;
                let count = a.material_count(model);
                let mut i: c_ulong = 0;
                while i < count && !result {
                    let mat = a.material(model, i, this_lod);
                    let end = mat.polygon_start.wrapping_add(mat.polygon_length);
                    let mut j = mat.polygon_start;
                    while j < end && !result {
                        let [p1, p2, p3] = a.triangle(model, frame, this_lod, j, mat.vertex_start);
                        let tri =
                            triangle_box(relative.apply_point(p1), relative.apply_point(p2), relative.apply_point(p3));
                        let other_count = a.material_count(other);
                        if first {
                            boxes.resize(other_count as usize, Vec::new());
                        }
                        result = false;
                        let mut k: c_ulong = 0;
                        while k < other_count && !result {
                            let om = a.material(other, k, other_lod);
                            let other_end = om.polygon_start.wrapping_add(om.polygon_length);
                            if first {
                                boxes[k as usize].resize(om.polygon_length as usize, GeBoundingBox::default());
                            }
                            let mut l = om.polygon_start;
                            while l < other_end && !result {
                                let slot = l.wrapping_sub(om.polygon_start) as usize;
                                if first {
                                    let [q1, q2, q3] = a.triangle(other, other_frame, other_lod, l, om.vertex_start);
                                    boxes[k as usize][slot] = triangle_box(q1, q2, q3);
                                }
                                result = boxes[k as usize][slot].intersects(&tri);
                                l = l.wrapping_add(1);
                            }
                            k = k.wrapping_add(1);
                        }
                        first = false;
                        j = j.wrapping_add(1);
                    }
                    i = i.wrapping_add(1);
                }
            }
        }
    }
    let mut m: c_ulong = 0;
    while !result && m < subs {
        result = model_walk(
            a,
            this,
            &transform,
            a.sub_model(model, m),
            other_location,
            other_inverse,
            other_scale,
            other,
            other_frame,
            lod,
        );
        m = m.wrapping_add(1);
    }
    result
}

// ---- the C ABI: the C++ fills the table over GEModel* handles (GEPhysicalModel.cpp)

/// The callback table over `GEModel*` handles (`const GEModel*` as `*const c_void`).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeGePhysicalModels {
    pub has_parent: unsafe extern "C" fn(model: *const c_void) -> bool,
    pub internal_transform: unsafe extern "C" fn(model: *const c_void, frame: c_ulong, out: *mut GeTransformation),
    pub center: unsafe extern "C" fn(model: *const c_void, out: *mut Ge3DPoint),
    pub radius: unsafe extern "C" fn(model: *const c_void, frame: c_ulong) -> f32,
    pub bbox: unsafe extern "C" fn(model: *const c_void, frame: c_ulong, out: *mut GeBoundingBox),
    pub material_count: unsafe extern "C" fn(model: *const c_void) -> c_ulong,
    pub material:
        unsafe extern "C" fn(model: *const c_void, index: c_ulong, lod: c_ulong, out: *mut GePhysicalMaterial),
    pub triangle: unsafe extern "C" fn(
        model: *const c_void,
        frame: c_ulong,
        lod: c_ulong,
        polygon: c_ulong,
        vertex_start: c_ulong,
        out: *mut Ge3DPoint,
    ),
    pub sub_model_count: unsafe extern "C" fn(model: *const c_void) -> c_ulong,
    pub sub_model: unsafe extern "C" fn(model: *const c_void, index: c_ulong) -> *const c_void,
    pub lod_count: unsafe extern "C" fn(model: *const c_void) -> c_ulong,
    pub polygon_count: unsafe extern "C" fn(model: *const c_void, lod: c_ulong) -> c_ulong,
    /// `GEPhysicalModel theModel(model, location, scale); theModel.SetFrame(frame);
    /// theModel.Intersects(line, lod, distance, true)`.
    pub retry_line: unsafe extern "C" fn(
        model: *const c_void,
        location: *const GeTransformation,
        frame: c_ulong,
        scale: f32,
        line: *const Ge3DLine,
        lod: c_ulong,
    ) -> bool,
}

/// The table, copied out of the host (its functions are the C++'s statics).
struct Table<'a>(&'a EeGePhysicalModels);

// SAFETY (every method): the table's functions take the handles the C++ passed in or returned
impl Models for Table<'_> {
    type M = *const c_void;
    fn is_null(&self, m: Self::M) -> bool {
        m.is_null()
    }
    fn has_parent(&self, m: Self::M) -> bool {
        unsafe { (self.0.has_parent)(m) }
    }
    fn internal_transform(&self, m: Self::M, frame: c_ulong) -> GeTransformation {
        let mut out = GeTransformation::default();
        unsafe { (self.0.internal_transform)(m, frame, &mut out) };
        out
    }
    fn center(&self, m: Self::M) -> Ge3DPoint {
        let mut out = Ge3DPoint::default();
        unsafe { (self.0.center)(m, &mut out) };
        out
    }
    fn radius(&self, m: Self::M, frame: c_ulong) -> f32 {
        unsafe { (self.0.radius)(m, frame) }
    }
    fn bbox(&self, m: Self::M, frame: c_ulong) -> GeBoundingBox {
        let mut out = GeBoundingBox::default();
        unsafe { (self.0.bbox)(m, frame, &mut out) };
        out
    }
    fn material_count(&self, m: Self::M) -> c_ulong {
        unsafe { (self.0.material_count)(m) }
    }
    fn material(&self, m: Self::M, index: c_ulong, lod: c_ulong) -> GePhysicalMaterial {
        let mut out = GePhysicalMaterial::default();
        unsafe { (self.0.material)(m, index, lod, &mut out) };
        out
    }
    fn triangle(
        &self,
        m: Self::M,
        frame: c_ulong,
        lod: c_ulong,
        polygon: c_ulong,
        vertex_start: c_ulong,
    ) -> [Ge3DPoint; 3] {
        let mut out = [Ge3DPoint::default(); 3];
        unsafe { (self.0.triangle)(m, frame, lod, polygon, vertex_start, out.as_mut_ptr()) };
        out
    }
    fn sub_model_count(&self, m: Self::M) -> c_ulong {
        unsafe { (self.0.sub_model_count)(m) }
    }
    fn sub_model(&self, m: Self::M, index: c_ulong) -> Self::M {
        unsafe { (self.0.sub_model)(m, index) }
    }
    fn lod_count(&self, m: Self::M) -> c_ulong {
        unsafe { (self.0.lod_count)(m) }
    }
    fn polygon_count(&self, m: Self::M, lod: c_ulong) -> c_ulong {
        unsafe { (self.0.polygon_count)(m, lod) }
    }
    fn retry_line(
        &self,
        location: &GeTransformation,
        model: Self::M,
        frame: c_ulong,
        scale: f32,
        line: &Ge3DLine,
        lod: c_ulong,
    ) -> bool {
        // copies: the host's call runs the line walk again through this crate
        let location = *location;
        let line = *line;
        unsafe { (self.0.retry_line)(model, &location, frame, scale, &line, lod) }
    }
}

/// `GEPhysicalModel::Intersects(location, model, frame, line, lod, distance, exact)`; `scale` is the placed model's.
///
/// # Safety
/// `table` holds valid callbacks; `model` is a live `GEModel*` or null; the other pointers are valid.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ge_physical_line_walk(
    table: *const EeGePhysicalModels,
    scale: f32,
    location: *const GeTransformation,
    model: *const c_void,
    frame: c_ulong,
    line: *const Ge3DLine,
    lod: c_ulong,
    distance: *mut f32,
    exact: bool,
) -> bool {
    // SAFETY: the function's contract. The inputs are copied and the distance goes back at the end, so no Rust
    // reference into the C++'s objects lives across the callbacks.
    unsafe {
        let table = *table;
        let location = *location;
        let line = *line;
        let mut d = *distance;
        let hit = line_walk(&Table(&table), scale, &location, model, frame, &line, lod, &mut d, exact);
        *distance = d;
        hit
    }
}

/// `GEPhysicalModel::Intersects(location, model, frame, line, exact, lod, x, y, z)` on the placed model
/// (`this_location`, `this_model`, `this_scale`, `this_frame`).
///
/// # Safety
/// As `ee_ge_physical_line_walk`; `model` and `this_model` are live `GEModel*`.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ge_physical_scaled_walk(
    table: *const EeGePhysicalModels,
    this_location: *const GeTransformation,
    this_model: *const c_void,
    this_scale: f32,
    this_frame: c_ulong,
    location: *const GeTransformation,
    model: *const c_void,
    frame: c_ulong,
    line: *const Ge3DLine,
    exact: bool,
    lod: c_ulong,
    x: f32,
    y: f32,
    z: f32,
) -> bool {
    // SAFETY: the function's contract
    unsafe {
        let table = *table;
        let this = Placed { location: *this_location, model: this_model, scale: this_scale, frame: this_frame };
        let (location, line) = (*location, *line);
        scaled_walk(&Table(&table), &this, &location, model, frame, &line, exact, lod, x, y, z)
    }
}

/// `GEPhysicalModel::Intersects(location, model, otherLocation, otherInverse, scale, otherModel, frame, lod)` on
/// the placed model (`this_location`, `this_model`, `this_scale`, `this_frame`).
///
/// # Safety
/// As `ee_ge_physical_line_walk`; the models are live `GEModel*`.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ge_physical_model_walk(
    table: *const EeGePhysicalModels,
    this_location: *const GeTransformation,
    this_model: *const c_void,
    this_scale: f32,
    this_frame: c_ulong,
    location: *const GeTransformation,
    model: *const c_void,
    other_location: *const GeTransformation,
    other_inverse: *const GeTransformation,
    other_scale: f32,
    other: *const c_void,
    other_frame: c_ulong,
    lod: c_ulong,
) -> bool {
    // SAFETY: the function's contract
    unsafe {
        let table = *table;
        let this = Placed { location: *this_location, model: this_model, scale: this_scale, frame: this_frame };
        let (location, other_location, other_inverse) = (*location, *other_location, *other_inverse);
        model_walk(
            &Table(&table),
            &this,
            &location,
            model,
            &other_location,
            &other_inverse,
            other_scale,
            other,
            other_frame,
            lod,
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_macros_keep_their_operand_order() {
        // `a > b ? a : b`: a NaN on either side picks the second operand, as the C++ macro does
        assert_eq!(max_macro(2.0, 1.0), 2.0);
        assert_eq!(max_macro(1.0, 2.0), 2.0);
        assert!(max_macro(1.0, f32::NAN).is_nan());
        assert_eq!(max_macro(f32::NAN, 1.0), 1.0);
        assert_eq!(min_macro(f32::NAN, 1.0), 1.0);
        assert!(min_macro(1.0, f32::NAN).is_nan());
    }

    #[test]
    fn a_tiny_scale_counts_as_none() {
        assert!(!nonzero(0.0));
        assert!(!nonzero(0.00005));
        assert!(!nonzero(-0.0001));
        assert!(nonzero(0.00011));
        assert!(nonzero(-0.5));
    }

    #[test]
    fn eighty_percent_of_the_lods_rounds_half_to_even() {
        // (1 - 0.2) * n through lrintf: 0.8 * 5 = 4, 0.8 * 1 = 1 (then the count itself means the last LOD)
        assert_eq!(lrintf((1.0f32 - LOD_FRACTION) * 5.0), 4);
        assert_eq!(lrintf((1.0f32 - LOD_FRACTION) * 1.0), 1);
        assert_eq!(lrintf((1.0f32 - LOD_FRACTION) * 3.0), 2);
    }
}
