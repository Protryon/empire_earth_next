//! `GEBitmap` as a whole Rust object (R4.3, ninth part): a textured quad's location, frame size, rotation, scales,
//! vertex array and per-screen textures, with every method of GEBitmap.cpp (the constructor's quad, the destructor's
//! releases, the corners, the stretched coordinates, the flips, the per-vertex colours, opacities, depths and texture
//! coordinates, `SetTexture`) and the header's accessors.
//!
//! As `modobj.rs`: the C++ `GEBitmap` stays the type the UI, the renderer and the fonts use; its methods are one-line
//! forwarders and its only member is this object. Every call the original made on another object (the rasterizer,
//! the vertex array, the textures) is one host operation (`EeGeBmpOp`, `GEBitmap::Host`), in the original's order.
//! Floats cross the host as their bits. The corners and the stretch are `bitmap.rs`'s.

use std::ffi::{c_ulong, c_void};

use crate::bitmap;
use crate::types::{Ge2DPoint, Ge3DPoint, GeRectangle, GeVector};

/// The calls a bitmap makes on other objects. `obj` is the object called; `a`, `b` as each says.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeBmpOp {
    /// `GERasterizer::GetActiveRasterizer()`.
    RasterActive = 1,
    /// `obj->GetScreenCount()`.
    RasterScreenCount,
    /// `obj->CreateVertexArray(a, b)`.
    RasterVertexArray,
    /// `delete (GEVertexArray*)obj`.
    VertexDelete,
    /// `obj->GetColor(*(GEColor*)b, a)`.
    VertexGetColor,
    /// `obj->SetColor(*(const GEColor*)b, a)`.
    VertexSetColor,
    /// `obj->GetPoint(*(GE3DPoint*)b, a)`.
    VertexGetPoint,
    /// `obj->SetPoint(*(const GE3DPoint*)b, a)`.
    VertexSetPoint,
    /// `obj->SetNormal(*(const GEVector*)b, a)`.
    VertexSetNormal,
    /// `obj->GetOpacity(a)`, as its bits.
    VertexGetOpacity,
    /// `obj->SetOpacity(b's bits, a)`.
    VertexSetOpacity,
    /// `obj->GetTextureCoordinate(*(GETextureCoordinate*)b, a)`.
    VertexGetUv,
    /// `obj->SetTextureCoordinate(*(const GETextureCoordinate*)b, a)`.
    VertexSetUv,
    /// `obj->Reference()` (a texture).
    TexReference,
    /// `obj->Dereference()` (GETexture's own).
    TexDereference,
    /// `obj->GetFrameWidth()`, as its bits.
    TexFrameWidth,
    /// `obj->GetFrameHeight()`, as its bits.
    TexFrameHeight,
    /// `obj->GetWidth()`.
    TexWidth,
    /// `obj->GetHeight()`.
    TexHeight,
    /// `obj->GetNumMipLevelsToDrop()`.
    TexDropLevels,
    /// `obj->CopyFrom(*(const GEVertexArray*)b)` (a rhombus's assignment, `prim.rs`).
    VertexCopyFrom,
}

/// The host: `owner` is the C++ bitmap of the object calling.
pub type EeGeBmpHostFn =
    unsafe extern "C" fn(owner: *mut c_void, op: EeGeBmpOp, obj: *mut c_void, a: isize, b: isize) -> isize;

/// A float of the header's accessors (`ee_ge_bmp_getf`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeBmpFloat {
    /// `mRotation`.
    Rotation = 1,
    /// `mXScale`.
    XScale,
    /// `mYScale`.
    YScale,
    /// `GetWidth()`: `mXScale * mWidth`.
    Width,
    /// `GetHeight()`: `mYScale * mHeight`.
    Height,
}

/// The texture coordinates of a flip or the constructor, per vertex.
type Uvs = [[f32; 2]; 4];
const UVS_PLAIN: Uvs = [[0.0, 0.0], [1.0, 0.0], [0.0, 1.0], [1.0, 1.0]];
const UVS_FLIP_HORIZONTALLY: Uvs = [[1.0, 0.0], [0.0, 0.0], [1.0, 1.0], [0.0, 1.0]];
const UVS_FLIP_VERTICALLY: Uvs = [[0.0, 1.0], [1.0, 1.0], [0.0, 0.0], [1.0, 0.0]];

/// The bitmap.
pub struct EeGeBitmap {
    host: EeGeBmpHostFn,
    /// The C++ bitmap (`this`).
    owner: usize,
    location: Ge2DPoint,
    vertices: usize,
    /// `mTextures`, one per screen of the rasterizer at construction.
    textures: Vec<usize>,
    height: f32,
    width: f32,
    rotation: f32,
    x_scale: f32,
    y_scale: f32,
}

/// # Safety
/// `p` is a live object from `ee_ge_bmp_new`.
unsafe fn bm<'a>(p: *mut EeGeBitmap) -> &'a mut EeGeBitmap {
    // SAFETY: the caller's contract; the reference is not held across a host call
    unsafe { &mut *p }
}

impl EeGeBitmap {
    /// A host call made by bitmap `p`.
    ///
    /// # Safety
    /// `p` is live; the operation's arguments are as `EeGeBmpOp` describes.
    unsafe fn op(p: *mut Self, op: EeGeBmpOp, obj: usize, a: isize, b: isize) -> isize {
        // SAFETY: the caller's contract (host and owner are copied out before the call)
        let (host, owner) = unsafe { (bm(p).host, bm(p).owner) };
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
            let vertices = bm(p).vertices;
            Self::op(p, op, vertices, i as isize, arg as isize)
        }
    }

    /// A float a host call answered as its bits.
    ///
    /// # Safety
    /// As `op`.
    unsafe fn float(p: *mut Self, op: EeGeBmpOp, obj: usize, a: isize) -> f32 {
        // SAFETY: the caller's contract
        f32::from_bits(unsafe { Self::op(p, op, obj, a, 0) } as u32)
    }

    /// `GEBitmap()`: one texture slot per screen of the active rasterizer, a four-vertex quad at the origin facing
    /// -z, white, opaque, with the whole texture, unscaled and unrotated; then its corners.
    ///
    /// # Safety
    /// `host` serves `owner` from now on.
    pub unsafe fn create(host: EeGeBmpHostFn, owner: usize) -> *mut Self {
        let p = Box::into_raw(Box::new(Self {
            host,
            owner,
            location: Ge2DPoint::default(),
            vertices: 0,
            textures: Vec::new(),
            height: 0.0,
            width: 0.0,
            rotation: 0.0,
            x_scale: 1.0,
            y_scale: 1.0,
        }));
        // SAFETY: `p` is live; every host call is a statement of its own
        unsafe {
            let rasterizer = Self::op(p, EeGeBmpOp::RasterActive, 0, 0, 0) as usize;
            let screens = Self::op(p, EeGeBmpOp::RasterScreenCount, rasterizer, 0, 0) as c_ulong;
            bm(p).textures = vec![0; screens as usize];
            let vertices = Self::op(p, EeGeBmpOp::RasterVertexArray, rasterizer, 1, 4) as usize;
            bm(p).vertices = vertices;
            let point = Ge3DPoint::default();
            for i in 0..4 {
                Self::vertex(p, EeGeBmpOp::VertexSetPoint, i, &point);
            }
            let normal = GeVector::new(0.0, 0.0, -1.0);
            for i in 0..4 {
                Self::vertex(p, EeGeBmpOp::VertexSetNormal, i, &normal);
            }
            let white = [1.0f32; 3];
            for i in 0..4 {
                Self::vertex(p, EeGeBmpOp::VertexSetColor, i, &white);
            }
            for i in 0..4 {
                Self::op(p, EeGeBmpOp::VertexSetOpacity, vertices, i, 1.0f32.to_bits() as isize);
            }
            Self::set_uvs(p, &UVS_PLAIN);
            Self::compute_rotation(p);
        }
        p
    }

    /// The destructor: the vertex array deleted, every screen's texture released; the object freed.
    ///
    /// # Safety
    /// `p` is live; it is freed.
    pub unsafe fn destroy(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            let vertices = bm(p).vertices;
            if vertices != 0 {
                Self::op(p, EeGeBmpOp::VertexDelete, vertices, 0, 0);
                bm(p).vertices = 0;
            }
            let mut i = 0;
            while i < bm(p).textures.len() {
                let texture = bm(p).textures[i];
                if texture != 0 {
                    Self::op(p, EeGeBmpOp::TexDereference, texture, 0, 0);
                }
                i += 1;
            }
            drop(Box::from_raw(p));
        }
    }

    /// `ComputeRotation`: the four corners from the location, frame size, scales and rotation, into the vertices'
    /// x and y (their z kept).
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn compute_rotation(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            let b = bm(p);
            let corners = bitmap::bitmap_corners(b.location, b.width, b.height, b.x_scale, b.y_scale, b.rotation);
            for (i, c) in corners.iter().enumerate() {
                let mut point = Ge3DPoint::default();
                Self::vertex(p, EeGeBmpOp::VertexGetPoint, i, &mut point as *mut Ge3DPoint);
                point.x = c.x;
                point.y = c.y;
                Self::vertex(p, EeGeBmpOp::VertexSetPoint, i, &point);
            }
        }
    }

    /// The texture coordinates of the four vertices in vertex order.
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn set_uvs(p: *mut Self, uvs: &Uvs) {
        for (i, uv) in uvs.iter().enumerate() {
            // SAFETY: the caller's contract
            unsafe { Self::vertex(p, EeGeBmpOp::VertexSetUv, i, uv.as_ptr()) };
        }
    }

    /// `ComputeStretchedCoordsAndScale`: the screen's texture's source rectangle mapped onto texture coordinates
    /// (`bitmap.rs`), set positionally as the original (the third value is the bottom left corner's), then the
    /// scale.
    ///
    /// # Safety
    /// `p` is live; the screen's texture is set.
    pub unsafe fn stretch(
        p: *mut Self,
        texture: &GeRectangle,
        source: &GeRectangle,
        destination: &GeRectangle,
        screen: c_ulong,
    ) {
        // SAFETY: the caller's contract
        unsafe {
            let t = bm(p).textures.get(screen as usize).copied().unwrap_or(0);
            let width = Self::op(p, EeGeBmpOp::TexWidth, t, 0, 0) as c_ulong;
            let height = Self::op(p, EeGeBmpOp::TexHeight, t, 0, 0) as c_ulong;
            let drop = Self::op(p, EeGeBmpOp::TexDropLevels, t, 0, 0) as c_ulong;
            let s = bitmap::stretched_coords(texture, source, destination, width, height, drop);
            Self::set_uvs(p, &s.uv);
            Self::set_xy_scale(p, s.x_scale, s.y_scale);
        }
    }

    /// `SetTexture`: nothing for the same texture; else the old one released, the new one referenced and its frame
    /// size taken; then the corners.
    ///
    /// # Safety
    /// `p` is live; `texture` a texture or 0.
    pub unsafe fn set_texture(p: *mut Self, texture: usize, screen: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let Some(old) = bm(p).textures.get(screen as usize).copied() else { return };
            if old == texture {
                return;
            }
            if old != 0 {
                Self::op(p, EeGeBmpOp::TexDereference, old, 0, 0);
            }
            bm(p).textures[screen as usize] = texture;
            if texture != 0 {
                Self::op(p, EeGeBmpOp::TexReference, texture, 0, 0);
                let height = Self::float(p, EeGeBmpOp::TexFrameHeight, texture, 0);
                bm(p).height = height;
                let width = Self::float(p, EeGeBmpOp::TexFrameWidth, texture, 0);
                bm(p).width = width;
            }
            Self::compute_rotation(p);
        }
    }

    /// `SetXYScale` (the header's).
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn set_xy_scale(p: *mut Self, x: f32, y: f32) {
        // SAFETY: the caller's contract
        unsafe {
            bm(p).x_scale = x;
            bm(p).y_scale = y;
            Self::compute_rotation(p);
        }
    }

    /// `SetZPerVertex`: the four depths, the x and y kept.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn set_z_per_vertex(p: *mut Self, z: [f32; 4]) {
        // SAFETY: the caller's contract
        unsafe {
            let mut points = [Ge3DPoint::default(); 4];
            for (i, point) in points.iter_mut().enumerate() {
                Self::vertex(p, EeGeBmpOp::VertexGetPoint, i, point as *mut Ge3DPoint);
            }
            for (point, z) in points.iter_mut().zip(z) {
                point.z = z;
            }
            for (i, point) in points.iter().enumerate() {
                Self::vertex(p, EeGeBmpOp::VertexSetPoint, i, point);
            }
        }
    }

    /// The four vertices' points (`GetLocationPerVertex`, `GetZPerVertex`).
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn points(p: *mut Self) -> [Ge3DPoint; 4] {
        let mut points = [Ge3DPoint::default(); 4];
        for (i, point) in points.iter_mut().enumerate() {
            // SAFETY: the caller's contract
            unsafe { Self::vertex(p, EeGeBmpOp::VertexGetPoint, i, point as *mut Ge3DPoint) };
        }
        points
    }
}

/// Four out (or in) pointers in vertex order: top left, top right, then the original's third and fourth.
pub type EeGeBmpQuad = [*mut c_void; 4];

/// `GEBitmap()`.
///
/// # Safety
/// `host` serves `owner` until `ee_ge_bmp_destroy`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_new(host: EeGeBmpHostFn, owner: *mut c_void) -> *mut EeGeBitmap {
    // SAFETY: the caller's contract
    unsafe { EeGeBitmap::create(host, owner as usize) }
}

/// `~GEBitmap`.
///
/// # Safety
/// `p` is live (freed).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_destroy(p: *mut EeGeBitmap) {
    // SAFETY: the caller's contract
    unsafe { EeGeBitmap::destroy(p) }
}

/// `ComputeStretchedCoordsAndScale(texture, source, destination, screen)`.
///
/// # Safety
/// `p` is live; the rectangles are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_stretch(
    p: *mut EeGeBitmap,
    texture: *const GeRectangle,
    source: *const GeRectangle,
    destination: *const GeRectangle,
    screen: c_ulong,
) {
    // SAFETY: the caller's contract
    unsafe { EeGeBitmap::stretch(p, &*texture, &*source, &*destination, screen) }
}

/// `FlipHorizontally` (`vertical` false) or `FlipVertically`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_flip(p: *mut EeGeBitmap, vertical: bool) {
    // SAFETY: the caller's contract
    unsafe { EeGeBitmap::set_uvs(p, if vertical { &UVS_FLIP_VERTICALLY } else { &UVS_FLIP_HORIZONTALLY }) }
}

/// `GetColorPerVertex` (`out` four `GEColor*`).
///
/// # Safety
/// `p` is live; the outputs are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_colors(p: *mut EeGeBitmap, out: *const EeGeBmpQuad) {
    // SAFETY: the caller's contract
    unsafe {
        for (i, o) in (*out).iter().enumerate() {
            EeGeBitmap::vertex(p, EeGeBmpOp::VertexGetColor, i, *o);
        }
    }
}

/// `SetColorPerVertex` (`colors` four `const GEColor*`; `SetColor` passes one four times).
///
/// # Safety
/// `p` is live; the inputs are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_set_colors(p: *mut EeGeBitmap, colors: *const EeGeBmpQuad) {
    // SAFETY: the caller's contract
    unsafe {
        for (i, c) in (*colors).iter().enumerate() {
            EeGeBitmap::vertex(p, EeGeBmpOp::VertexSetColor, i, *c);
        }
    }
}

/// `GetLocationPerVertex` (`out` four `GE2DPoint*`): the four points read, then their x and y written.
///
/// # Safety
/// `p` is live; the outputs are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_locations(p: *mut EeGeBitmap, out: *const EeGeBmpQuad) {
    // SAFETY: the caller's contract
    unsafe {
        let points = EeGeBitmap::points(p);
        for (o, point) in (*out).iter().zip(points) {
            *(*o as *mut Ge2DPoint) = Ge2DPoint { x: point.x, y: point.y };
        }
    }
}

/// `GetZPerVertex` (`out` four `float*`).
///
/// # Safety
/// `p` is live; the outputs are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_depths(p: *mut EeGeBitmap, out: *const EeGeBmpQuad) {
    // SAFETY: the caller's contract
    unsafe {
        let points = EeGeBitmap::points(p);
        for (o, point) in (*out).iter().zip(points) {
            *(*o as *mut f32) = point.z;
        }
    }
}

/// `GetOpacityPerVertex` (`out` four `float*`).
///
/// # Safety
/// `p` is live; the outputs are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_opacities(p: *mut EeGeBitmap, out: *const EeGeBmpQuad) {
    // SAFETY: the caller's contract
    unsafe {
        let vertices = bm(p).vertices;
        for (i, o) in (*out).iter().enumerate() {
            *(*o as *mut f32) = EeGeBitmap::float(p, EeGeBmpOp::VertexGetOpacity, vertices, i as isize);
        }
    }
}

/// `SetOpacityPerVertex` (`SetOpacity` passes one four times).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_set_opacities(p: *mut EeGeBitmap, opacities: *const [f32; 4]) {
    // SAFETY: the caller's contract
    unsafe {
        let vertices = bm(p).vertices;
        for (i, o) in (*opacities).iter().enumerate() {
            EeGeBitmap::op(p, EeGeBmpOp::VertexSetOpacity, vertices, i as isize, o.to_bits() as isize);
        }
    }
}

/// `GetTextureCoordinatesPerVertex` (`out` four `GETextureCoordinate*`).
///
/// # Safety
/// `p` is live; the outputs are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_uvs(p: *mut EeGeBitmap, out: *const EeGeBmpQuad) {
    // SAFETY: the caller's contract
    unsafe {
        for (i, o) in (*out).iter().enumerate() {
            EeGeBitmap::vertex(p, EeGeBmpOp::VertexGetUv, i, *o);
        }
    }
}

/// `SetTextureCoordinates` (`uvs` four `GETextureCoordinate*`).
///
/// # Safety
/// `p` is live; the inputs are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_set_uvs(p: *mut EeGeBitmap, uvs: *const EeGeBmpQuad) {
    // SAFETY: the caller's contract
    unsafe {
        for (i, uv) in (*uvs).iter().enumerate() {
            EeGeBitmap::vertex(p, EeGeBmpOp::VertexSetUv, i, *uv);
        }
    }
}

/// `SetZPerVertex` (`SetZ` passes one four times).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_set_depths(p: *mut EeGeBitmap, z: *const [f32; 4]) {
    // SAFETY: the caller's contract
    unsafe { EeGeBitmap::set_z_per_vertex(p, *z) }
}

/// `SetLocation` (the header's).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_set_location(p: *mut EeGeBitmap, location: *const Ge2DPoint) {
    // SAFETY: the caller's contract
    unsafe {
        bm(p).location = *location;
        EeGeBitmap::compute_rotation(p);
    }
}

/// `SetRotation`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_set_rotation(p: *mut EeGeBitmap, rotation: f32) {
    // SAFETY: the caller's contract
    unsafe {
        bm(p).rotation = rotation;
        EeGeBitmap::compute_rotation(p);
    }
}

/// `SetXYScale` (the header's; `SetScale` passes one twice).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_set_xy_scale(p: *mut EeGeBitmap, x: f32, y: f32) {
    // SAFETY: the caller's contract
    unsafe { EeGeBitmap::set_xy_scale(p, x, y) }
}

/// `SetTexture(texture, screen)`.
///
/// # Safety
/// `p` is live; `texture` a texture or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_set_texture(p: *mut EeGeBitmap, texture: *mut c_void, screen: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeGeBitmap::set_texture(p, texture as usize, screen) }
}

/// A float of the header's accessors.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_getf(p: *mut EeGeBitmap, field: EeGeBmpFloat) -> f32 {
    // SAFETY: the caller's contract
    let b = unsafe { bm(p) };
    match field {
        EeGeBmpFloat::Rotation => b.rotation,
        EeGeBmpFloat::XScale => b.x_scale,
        EeGeBmpFloat::YScale => b.y_scale,
        EeGeBmpFloat::Width => b.x_scale * b.width,
        EeGeBmpFloat::Height => b.y_scale * b.height,
    }
}

/// `GetLocation()`: the location inside the object.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_location(p: *mut EeGeBitmap) -> *const Ge2DPoint {
    // SAFETY: the caller's contract
    unsafe { &bm(p).location }
}

/// `GetTexture(screen)` (NULL past the screens: the original read past its array).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_texture(p: *mut EeGeBitmap, screen: c_ulong) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { bm(p).textures.get(screen as usize).copied().unwrap_or(0) as *mut c_void }
}

/// `GetVertexArray()`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_bmp_vertices(p: *mut EeGeBitmap) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { bm(p).vertices as *mut c_void }
}

#[cfg(test)]
#[path = "bmpobj_tests.rs"]
mod tests;
