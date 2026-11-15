//! `GELine`, `GEPoint` and `GERhombus` as one whole Rust object (R4.3, fifteenth part): a primitive of one, two or
//! four vertices in a rasterizer vertex array, the per-screen textures of a line and a point, and a rhombus's
//! rectangle and rotation, with every method of GELine.cpp, GEPoint.cpp and GERhombus.cpp (the constructors' vertices,
//! the destructors' releases, `SetTexture`, `ComputeRotation`) and the headers' per-vertex members.
//!
//! As `bmpobj.rs`: the C++ classes stay the types the UI, the HUD and the renderer use (`GELine` arrays are drawn as
//! arrays); their only member is this object and their methods are one-line forwarders. Every call the originals made
//! on another object (the rasterizer, the vertex array, the textures) is one operation of the bitmap's host
//! (`EeGeBmpOp`, `GEVertexHost`), in the original's order. The rhombus's corners are `bitmap.rs`'s.

use std::ffi::{c_ulong, c_void};

use crate::bitmap;
use crate::bmpobj::{EeGeBmpHostFn, EeGeBmpOp};
use crate::types::{Ge2DPoint, Ge3DPoint, GeRectangle, GeVector};

/// Which class a default constructor builds.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGePrimKind {
    /// `GELine()`: two white opaque vertices at the origin facing -z, texture coordinates (0, 0) and (1, 1).
    Line = 1,
    /// `GEPoint()`: one opaque vertex, texture coordinate (0, 0).
    Point,
    /// `GERhombus()`: four white opaque vertices facing -z, an empty rectangle, no rotation.
    Rhombus,
}

/// The primitive.
pub struct EeGePrim {
    host: EeGeBmpHostFn,
    /// The C++ object (`this`).
    owner: usize,
    /// `mVertices`.
    vertices: usize,
    /// `mTextures`, one per screen of the rasterizer at construction (empty for a rhombus).
    textures: Vec<usize>,
    /// A rhombus's `mRectangle`.
    rectangle: GeRectangle,
    /// A rhombus's `mRotation`.
    rotation: f32,
}

/// # Safety
/// `p` is a live object from one of the constructors.
unsafe fn pr<'a>(p: *mut EeGePrim) -> &'a mut EeGePrim {
    // SAFETY: the caller's contract; the reference is not held across a host call
    unsafe { &mut *p }
}

impl EeGePrim {
    /// A host call made by primitive `p`.
    ///
    /// # Safety
    /// `p` is live; the operation's arguments are as `EeGeBmpOp` describes.
    unsafe fn op(p: *mut Self, op: EeGeBmpOp, obj: usize, a: isize, b: isize) -> isize {
        // SAFETY: the caller's contract (host and owner are copied out before the call)
        let (host, owner) = unsafe { (pr(p).host, pr(p).owner) };
        // SAFETY: the host's contract
        unsafe { host(owner as *mut c_void, op, obj as *mut c_void, a, b) }
    }

    /// A vertex array call on vertex `i` with a pointer argument.
    ///
    /// # Safety
    /// `p` is live; `arg` points at what `op` reads or writes.
    unsafe fn vertex<T>(p: *mut Self, op: EeGeBmpOp, i: usize, arg: *const T) -> isize {
        // SAFETY: the caller's contract
        unsafe {
            let vertices = pr(p).vertices;
            Self::op(p, op, vertices, i as isize, arg as isize)
        }
    }

    /// `SetOpacity(opacity, i)` on the vertex array.
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn set_opacity(p: *mut Self, i: usize, opacity: f32) {
        // SAFETY: the caller's contract
        unsafe {
            let vertices = pr(p).vertices;
            Self::op(p, EeGeBmpOp::VertexSetOpacity, vertices, i as isize, opacity.to_bits() as isize);
        }
    }

    /// The object, its screens' texture slots (a line and a point) and its vertex array of `count` coloured vertices
    /// from the active rasterizer, in the constructors' order.
    ///
    /// # Safety
    /// `host` serves `owner` from now on.
    unsafe fn start(host: EeGeBmpHostFn, owner: usize, count: isize, textured: bool) -> *mut Self {
        let p = Box::into_raw(Box::new(Self {
            host,
            owner,
            vertices: 0,
            textures: Vec::new(),
            rectangle: GeRectangle::default(),
            rotation: 0.0,
        }));
        // SAFETY: `p` is live; every host call is a statement of its own
        unsafe {
            let rasterizer = Self::op(p, EeGeBmpOp::RasterActive, 0, 0, 0) as usize;
            if textured {
                let screens = Self::op(p, EeGeBmpOp::RasterScreenCount, rasterizer, 0, 0) as c_ulong;
                pr(p).textures = vec![0; screens as usize];
            }
            let vertices = Self::op(p, EeGeBmpOp::RasterVertexArray, rasterizer, 1, count) as usize;
            pr(p).vertices = vertices;
        }
        p
    }

    /// `GELine()`, `GEPoint()` or `GERhombus()`.
    ///
    /// # Safety
    /// `host` serves `owner` until `destroy`.
    pub unsafe fn create(host: EeGeBmpHostFn, owner: usize, kind: EeGePrimKind) -> *mut Self {
        let origin = Ge3DPoint::default();
        let normal = GeVector::new(0.0, 0.0, -1.0);
        let white = [1.0f32; 3];
        // SAFETY: the caller's contract; each host call is a statement of its own
        unsafe {
            match kind {
                EeGePrimKind::Line => {
                    let p = Self::start(host, owner, 2, true);
                    Self::vertex(p, EeGeBmpOp::VertexSetPoint, 0, &origin);
                    Self::vertex(p, EeGeBmpOp::VertexSetPoint, 1, &origin);
                    Self::vertex(p, EeGeBmpOp::VertexSetNormal, 0, &normal);
                    Self::vertex(p, EeGeBmpOp::VertexSetNormal, 1, &normal);
                    Self::vertex(p, EeGeBmpOp::VertexSetColor, 0, &white);
                    Self::vertex(p, EeGeBmpOp::VertexSetColor, 1, &white);
                    Self::set_opacity(p, 0, 1.0);
                    Self::set_opacity(p, 1, 1.0);
                    Self::vertex(p, EeGeBmpOp::VertexSetUv, 0, &[0.0f32, 0.0]);
                    Self::vertex(p, EeGeBmpOp::VertexSetUv, 1, &[1.0f32, 1.0]);
                    p
                }
                EeGePrimKind::Point => {
                    let p = Self::start(host, owner, 1, true);
                    Self::set_opacity(p, 0, 1.0);
                    Self::vertex(p, EeGeBmpOp::VertexSetUv, 0, &[0.0f32, 0.0]);
                    p
                }
                EeGePrimKind::Rhombus => {
                    let p = Self::start(host, owner, 4, false);
                    for i in 0..4 {
                        Self::vertex(p, EeGeBmpOp::VertexSetPoint, i, &origin);
                    }
                    for i in 0..4 {
                        Self::vertex(p, EeGeBmpOp::VertexSetNormal, i, &normal);
                    }
                    for i in 0..4 {
                        Self::vertex(p, EeGeBmpOp::VertexSetColor, i, &white);
                    }
                    for i in 0..4 {
                        Self::set_opacity(p, i, 1.0);
                    }
                    pr(p).rotation = 0.0;
                    Self::compute_rotation(p);
                    p
                }
            }
        }
    }

    /// `GELine(start, end, start colour, end colour, opacity)`.
    ///
    /// # Safety
    /// As `create`; the colours are three floats each.
    pub unsafe fn create_line(
        host: EeGeBmpHostFn,
        owner: usize,
        start: Ge2DPoint,
        end: Ge2DPoint,
        start_color: *const f32,
        end_color: *const f32,
        opacity: f32,
    ) -> *mut Self {
        // SAFETY: the caller's contract
        unsafe {
            let p = Self::start(host, owner, 2, true);
            let start = Ge3DPoint { x: start.x, y: start.y, z: 0.0 };
            let end = Ge3DPoint { x: end.x, y: end.y, z: 0.0 };
            Self::vertex(p, EeGeBmpOp::VertexSetPoint, 0, &start);
            Self::vertex(p, EeGeBmpOp::VertexSetPoint, 1, &end);
            Self::vertex(p, EeGeBmpOp::VertexSetColor, 0, start_color);
            Self::vertex(p, EeGeBmpOp::VertexSetColor, 1, end_color);
            Self::set_opacity(p, 0, opacity);
            Self::set_opacity(p, 1, opacity);
            Self::vertex(p, EeGeBmpOp::VertexSetUv, 0, &[0.0f32, 0.0]);
            Self::vertex(p, EeGeBmpOp::VertexSetUv, 1, &[1.0f32, 1.0]);
            p
        }
    }

    /// `GEPoint(point, colour, opacity)`.
    ///
    /// # Safety
    /// As `create`; the colour is three floats.
    pub unsafe fn create_point(
        host: EeGeBmpHostFn,
        owner: usize,
        point: Ge2DPoint,
        color: *const f32,
        opacity: f32,
    ) -> *mut Self {
        // SAFETY: the caller's contract
        unsafe {
            let p = Self::start(host, owner, 1, true);
            let point = Ge3DPoint { x: point.x, y: point.y, z: 0.0 };
            Self::vertex(p, EeGeBmpOp::VertexSetPoint, 0, &point);
            Self::vertex(p, EeGeBmpOp::VertexSetColor, 0, color);
            Self::set_opacity(p, 0, opacity);
            Self::vertex(p, EeGeBmpOp::VertexSetUv, 0, &[0.0f32, 0.0]);
            p
        }
    }

    /// The destructors: the vertex array deleted, then every screen's texture released; the object freed.
    ///
    /// # Safety
    /// `p` is live; it is freed.
    pub unsafe fn destroy(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            let vertices = pr(p).vertices;
            Self::op(p, EeGeBmpOp::VertexDelete, vertices, 0, 0);
            let mut i = 0;
            while i < pr(p).textures.len() {
                let texture = pr(p).textures[i];
                if texture != 0 {
                    Self::op(p, EeGeBmpOp::TexDereference, texture, 0, 0);
                }
                i += 1;
            }
            drop(Box::from_raw(p));
        }
    }

    /// `SetTexture(texture, screen)` of a line or a point: the old one released, the new one referenced (the same
    /// texture too, as the original).
    ///
    /// # Safety
    /// `p` is live; `texture` a texture or 0.
    pub unsafe fn set_texture(p: *mut Self, texture: usize, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let Some(old) = pr(p).textures.get(screen as usize).copied() else { return };
            if old != 0 {
                Self::op(p, EeGeBmpOp::TexDereference, old, 0, 0);
            }
            pr(p).textures[screen as usize] = texture;
            if texture != 0 {
                Self::op(p, EeGeBmpOp::TexReference, texture, 0, 0);
            }
        }
    }

    /// A rhombus's `ComputeRotation`: the corners from the rectangle and rotation into the vertices' x and y (their
    /// z kept).
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn compute_rotation(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            let corners = bitmap::rhombus_corners(&pr(p).rectangle, pr(p).rotation);
            let mut points = [Ge3DPoint::default(); 4];
            for (i, c) in corners.iter().enumerate() {
                Self::vertex(p, EeGeBmpOp::VertexGetPoint, i, &mut points[i] as *mut Ge3DPoint);
                points[i].x = c.x;
                points[i].y = c.y;
                Self::vertex(p, EeGeBmpOp::VertexSetPoint, i, &points[i]);
            }
        }
    }
}

/// The default constructors (`GELine()`, `GEPoint()`, `GERhombus()`).
///
/// # Safety
/// `host` serves `owner` until `ee_ge_prim_destroy`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_new(host: EeGeBmpHostFn, owner: *mut c_void, kind: EeGePrimKind) -> *mut EeGePrim {
    // SAFETY: the caller's contract
    unsafe { EeGePrim::create(host, owner as usize, kind) }
}

/// `GELine(start, end, start colour, end colour, opacity)` (the colours as `const GEColor*`).
///
/// # Safety
/// As `ee_ge_prim_new`; the pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_new_line(
    host: EeGeBmpHostFn,
    owner: *mut c_void,
    start: *const Ge2DPoint,
    end: *const Ge2DPoint,
    start_color: *const c_void,
    end_color: *const c_void,
    opacity: f32,
) -> *mut EeGePrim {
    // SAFETY: the caller's contract
    unsafe { EeGePrim::create_line(host, owner as usize, *start, *end, start_color.cast(), end_color.cast(), opacity) }
}

/// `GEPoint(point, colour, opacity)` (the colour as `const GEColor*`).
///
/// # Safety
/// As `ee_ge_prim_new`; the pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_new_point(
    host: EeGeBmpHostFn,
    owner: *mut c_void,
    point: *const Ge2DPoint,
    color: *const c_void,
    opacity: f32,
) -> *mut EeGePrim {
    // SAFETY: the caller's contract
    unsafe { EeGePrim::create_point(host, owner as usize, *point, color.cast(), opacity) }
}

/// The destructors.
///
/// # Safety
/// `p` is live (freed).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_destroy(p: *mut EeGePrim) {
    // SAFETY: the caller's contract
    unsafe { EeGePrim::destroy(p) }
}

/// One vertex array call on vertex `i` (`op` one of the `Vertex*` operations with a pointer argument: the colours,
/// points, normals and texture coordinates the headers read and write).
///
/// # Safety
/// `p` is live; `arg` points at what `op` reads or writes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_vertex(p: *mut EeGePrim, op: EeGeBmpOp, i: c_ulong, arg: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { EeGePrim::vertex(p, op, i as usize, arg) };
}

/// `GetOpacity(i)` of the vertex array.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_opacity(p: *mut EeGePrim, i: c_ulong) -> f32 {
    // SAFETY: the caller's contract
    unsafe {
        let vertices = pr(p).vertices;
        f32::from_bits(EeGePrim::op(p, EeGeBmpOp::VertexGetOpacity, vertices, i as isize, 0) as u32)
    }
}

/// `SetOpacity(opacity, i)` of the vertex array.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_set_opacity(p: *mut EeGePrim, i: c_ulong, opacity: f32) {
    // SAFETY: the caller's contract
    unsafe { EeGePrim::set_opacity(p, i as usize, opacity) }
}

/// `SetStartPoint`/`SetEndPoint`/`SetPoint`: vertex `i` at (x, y, 0).
///
/// # Safety
/// `p` is live; `point` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_set_xy(p: *mut EeGePrim, i: c_ulong, point: *const Ge2DPoint) {
    // SAFETY: the caller's contract
    unsafe {
        let point = Ge3DPoint { x: (*point).x, y: (*point).y, z: 0.0 };
        EeGePrim::vertex(p, EeGeBmpOp::VertexSetPoint, i as usize, &point);
    }
}

/// `GEPoint::GetPoint`: vertex `i`'s x and y.
///
/// # Safety
/// `p` is live; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_xy(p: *mut EeGePrim, i: c_ulong, out: *mut Ge2DPoint) {
    // SAFETY: the caller's contract
    unsafe {
        let mut point = Ge3DPoint::default();
        EeGePrim::vertex(p, EeGeBmpOp::VertexGetPoint, i as usize, &mut point as *mut Ge3DPoint);
        (*out).x = point.x;
        (*out).y = point.y;
    }
}

/// `SetTexture(texture, screen)`.
///
/// # Safety
/// `p` is live; `texture` a texture or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_set_texture(p: *mut EeGePrim, texture: *mut c_void, screen: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeGePrim::set_texture(p, texture as usize, screen) }
}

/// `GetTexture(screen)` (NULL past the screens: the original read past its array).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_texture(p: *const EeGePrim, screen: c_ulong) -> *mut c_void {
    // SAFETY: the caller's contract
    let prim = unsafe { &*p };
    prim.textures.get(screen as usize).copied().unwrap_or(0) as *mut c_void
}

/// `GetVertexArray()`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_vertices(p: *const EeGePrim) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { (*p).vertices as *mut c_void }
}

/// A rhombus's `GetRectangle()`: the rectangle inside the object.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_rectangle(p: *const EeGePrim) -> *const GeRectangle {
    // SAFETY: the caller's contract
    unsafe { &(*p).rectangle }
}

/// A rhombus's `SetRectangle` (`rectangle` set) or `SetRotation` (`rectangle` NULL), then `ComputeRotation`.
///
/// # Safety
/// `p` is live; `rectangle` is valid or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_place(p: *mut EeGePrim, rectangle: *const GeRectangle, rotation: f32) {
    // SAFETY: the caller's contract
    unsafe {
        if rectangle.is_null() {
            pr(p).rotation = rotation;
        } else {
            pr(p).rectangle = *rectangle;
        }
        EeGePrim::compute_rotation(p);
    }
}

/// A rhombus's `SetZPerVertex` (`SetZ` passes one four times): the four points read, their depths set, written back.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_set_depths(p: *mut EeGePrim, z: *const [f32; 4]) {
    // SAFETY: the caller's contract
    unsafe {
        let mut points = [Ge3DPoint::default(); 4];
        for (i, point) in points.iter_mut().enumerate() {
            EeGePrim::vertex(p, EeGeBmpOp::VertexGetPoint, i, point as *mut Ge3DPoint);
        }
        for (point, z) in points.iter_mut().zip(*z) {
            point.z = z;
        }
        for (i, point) in points.iter().enumerate() {
            EeGePrim::vertex(p, EeGeBmpOp::VertexSetPoint, i, point);
        }
    }
}

/// A rhombus's `operator=`: the vertices copied (`CopyFrom`), then the rectangle and the rotation.
///
/// # Safety
/// `p` and `from` are live rhombi.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_prim_assign(p: *mut EeGePrim, from: *const EeGePrim) {
    // SAFETY: the caller's contract
    unsafe {
        let (vertices, source) = (pr(p).vertices, (*from).vertices);
        EeGePrim::op(p, EeGeBmpOp::VertexCopyFrom, vertices, 0, source as isize);
        let (rectangle, rotation) = ((*from).rectangle, (*from).rotation);
        pr(p).rectangle = rectangle;
        pr(p).rotation = rotation;
    }
}

#[cfg(test)]
#[path = "prim_tests.rs"]
mod tests;
