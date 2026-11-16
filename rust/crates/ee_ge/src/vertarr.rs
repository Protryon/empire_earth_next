//! The vertex layouts, the colour packing and the arrays: the C++ `GEVertexArray` (type 0 [`StandardVertexArray`],
//! type 1 [`StdColorVertexArray`]) and `GEPolygonArray` ([`PolygonArray`]), whose objects these are. The layouts are
//! the GPU's: `Dx7Vertex` is `GXVERTEX` (stride 0x20), `Dx7StdColorVertex` is `GXVERTEX_STD_COLOR` (stride 0x24).
//! Moved from ee_dx7 in R4.3 part 17, when the renderer's DX7StandardVertexArray, DX7StdColorVertexArray and
//! DX7PolygonArray trampolines were merged into the two GE classes (ee_dx7 re-exports this module as `vertex`).

use std::ffi::{c_ulong, c_void};

/// `GXVERTEX`: position, normal, one texture coordinate.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Dx7Vertex {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub nx: f32,
    pub ny: f32,
    pub nz: f32,
    pub tu: f32,
    pub tv: f32,
}

/// `GXVERTEX_STD_COLOR`: position, normal, a packed ARGB colour, one texture coordinate.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Dx7StdColorVertex {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub nx: f32,
    pub ny: f32,
    pub nz: f32,
    pub color: u32,
    pub tu: f32,
    pub tv: f32,
}

const _: () = assert!(core::mem::size_of::<Dx7Vertex>() == 0x20);
const _: () = assert!(core::mem::size_of::<Dx7StdColorVertex>() == 0x24);

/// `UMFloatToLong`: round to nearest, ties to even (x87 `fistp` / `lrintf` in the default mode). An unrepresentable
/// value gives the x87 "integer indefinite", `i32::MIN` (x86-64 `lrintf` too; only garbage colours reach this).
#[inline]
pub fn float_to_long(value: f32) -> i32 {
    let rounded = value.round_ties_even();
    if (-2_147_483_648.0..2_147_483_648.0).contains(&rounded) {
        rounded as i32
    } else {
        i32::MIN
    }
}

/// `GXRGBA(r, g, b, a)` on `UMFloatToLong` values: unclamped, each component shifted into place and wrapped to 32 bits
/// exactly as the C++ macro's `unsigned long` arithmetic did (components above 255 bleed into the next byte).
#[inline]
pub const fn rgba(r: i32, g: i32, b: i32, a: i32) -> u32 {
    ((a as u32) << 24) | ((r as u32) << 16) | ((g as u32) << 8) | (b as u32)
}

/// `GXRGB_MAKE(r, g, b)`: as [`rgba`], with no alpha.
#[inline]
pub const fn rgb_make(r: i32, g: i32, b: i32) -> u32 {
    ((r as u32) << 16) | ((g as u32) << 8) | (b as u32)
}

/// The 1/255 of `DX7StdColorVertexArray`'s colour reads.
const K_SCALE: f32 = 1.0 / 255.0;

/// A `GEVertexArray` of [`Dx7Vertex`]: no colour (reads give white, opacity 1; writes of colour and opacity are
/// ignored).
pub struct StandardVertexArray {
    vertices: Vec<Dx7Vertex>,
}

impl StandardVertexArray {
    pub fn new(size: u32) -> Self {
        StandardVertexArray { vertices: vec![Dx7Vertex::default(); size as usize] }
    }

    pub fn len(&self) -> u32 {
        self.vertices.len() as u32
    }

    pub fn is_empty(&self) -> bool {
        self.vertices.is_empty()
    }

    pub fn as_mut_ptr(&mut self) -> *mut Dx7Vertex {
        self.vertices.as_mut_ptr()
    }

    pub fn vertices(&self) -> &[Dx7Vertex] {
        &self.vertices
    }

    /// `CopyFrom`: as many vertices as both arrays hold.
    pub fn copy_from(&mut self, other: &StandardVertexArray) {
        let n = self.vertices.len().min(other.vertices.len());
        self.vertices[..n].copy_from_slice(&other.vertices[..n]);
    }

    /// Reads give white whatever the vertex.
    pub fn color(&self, _index: u32) -> [f32; 3] {
        [1.0, 1.0, 1.0]
    }

    pub fn normal(&self, index: u32) -> [f32; 3] {
        let v = &self.vertices[index as usize];
        [v.nx, v.ny, v.nz]
    }

    pub fn opacity(&self, _index: u32) -> f32 {
        1.0
    }

    pub fn point(&self, index: u32) -> [f32; 3] {
        let v = &self.vertices[index as usize];
        [v.x, v.y, v.z]
    }

    pub fn texture_coordinate(&self, index: u32) -> [f32; 2] {
        let v = &self.vertices[index as usize];
        [v.tu, v.tv]
    }

    pub fn set_normal(&mut self, n: [f32; 3], index: u32) {
        let v = &mut self.vertices[index as usize];
        (v.nx, v.ny, v.nz) = (n[0], n[1], n[2]);
    }

    pub fn set_point(&mut self, p: [f32; 3], index: u32) {
        let v = &mut self.vertices[index as usize];
        (v.x, v.y, v.z) = (p[0], p[1], p[2]);
    }

    pub fn set_texture_coordinate(&mut self, uv: [f32; 2], index: u32) {
        let v = &mut self.vertices[index as usize];
        (v.tu, v.tv) = (uv[0], uv[1]);
    }

    /// `SetVertex`: colour and opacity are ignored by this array.
    pub fn set_vertex(&mut self, p: [f32; 3], n: [f32; 3], uv: [f32; 2], index: u32) {
        self.vertices[index as usize] =
            Dx7Vertex { x: p[0], y: p[1], z: p[2], nx: n[0], ny: n[1], nz: n[2], tu: uv[0], tv: uv[1] };
    }

    /// `SizeOf`: the vertex data plus 8.
    pub fn size_of(&self) -> u32 {
        (self.vertices.len() as u32).wrapping_mul(0x20).wrapping_add(8)
    }
}

/// A `GEVertexArray` of [`Dx7StdColorVertex`].
pub struct StdColorVertexArray {
    vertices: Vec<Dx7StdColorVertex>,
}

impl StdColorVertexArray {
    pub fn new(size: u32) -> Self {
        StdColorVertexArray { vertices: vec![Dx7StdColorVertex::default(); size as usize] }
    }

    pub fn len(&self) -> u32 {
        self.vertices.len() as u32
    }

    pub fn is_empty(&self) -> bool {
        self.vertices.is_empty()
    }

    pub fn as_mut_ptr(&mut self) -> *mut Dx7StdColorVertex {
        self.vertices.as_mut_ptr()
    }

    pub fn vertices(&self) -> &[Dx7StdColorVertex] {
        &self.vertices
    }

    pub fn copy_from(&mut self, other: &StdColorVertexArray) {
        let n = self.vertices.len().min(other.vertices.len());
        self.vertices[..n].copy_from_slice(&other.vertices[..n]);
    }

    /// `GetColor`: the red, green and blue bytes as 0..1.
    pub fn color(&self, index: u32) -> [f32; 3] {
        let c = self.vertices[index as usize].color;
        [((c >> 16) & 0xff) as f32 * K_SCALE, ((c >> 8) & 0xff) as f32 * K_SCALE, (c & 0xff) as f32 * K_SCALE]
    }

    pub fn normal(&self, index: u32) -> [f32; 3] {
        let v = &self.vertices[index as usize];
        [v.nx, v.ny, v.nz]
    }

    /// `GetOpacity`: the alpha byte as 0..1.
    pub fn opacity(&self, index: u32) -> f32 {
        (self.vertices[index as usize].color >> 24) as f32 * K_SCALE
    }

    pub fn point(&self, index: u32) -> [f32; 3] {
        let v = &self.vertices[index as usize];
        [v.x, v.y, v.z]
    }

    pub fn texture_coordinate(&self, index: u32) -> [f32; 2] {
        let v = &self.vertices[index as usize];
        [v.tu, v.tv]
    }

    /// `SetColor`: unclamped, keeping the alpha byte.
    pub fn set_color(&mut self, c: [f32; 3], index: u32) {
        let v = &mut self.vertices[index as usize];
        v.color = (v.color & 0xff00_0000)
            | rgb_make(float_to_long(c[0] * 255.0), float_to_long(c[1] * 255.0), float_to_long(c[2] * 255.0));
    }

    pub fn set_normal(&mut self, n: [f32; 3], index: u32) {
        let v = &mut self.vertices[index as usize];
        (v.nx, v.ny, v.nz) = (n[0], n[1], n[2]);
    }

    /// `SetOpacity`: the alpha byte (unclamped), keeping the colour bits below it.
    pub fn set_opacity(&mut self, opacity: f32, index: u32) {
        let v = &mut self.vertices[index as usize];
        v.color = ((float_to_long(opacity * 255.0) as u32) << 24) | (v.color & 0x00ff_ffff);
    }

    pub fn set_point(&mut self, p: [f32; 3], index: u32) {
        let v = &mut self.vertices[index as usize];
        (v.x, v.y, v.z) = (p[0], p[1], p[2]);
    }

    pub fn set_texture_coordinate(&mut self, uv: [f32; 2], index: u32) {
        let v = &mut self.vertices[index as usize];
        (v.tu, v.tv) = (uv[0], uv[1]);
    }

    /// `SetVertex`: every field, with the opacity as alpha.
    pub fn set_vertex(&mut self, c: [f32; 3], p: [f32; 3], n: [f32; 3], uv: [f32; 2], opacity: f32, index: u32) {
        self.vertices[index as usize] = Dx7StdColorVertex {
            x: p[0],
            y: p[1],
            z: p[2],
            nx: n[0],
            ny: n[1],
            nz: n[2],
            color: rgba(
                float_to_long(c[0] * 255.0),
                float_to_long(c[1] * 255.0),
                float_to_long(c[2] * 255.0),
                float_to_long(opacity * 255.0),
            ),
            tu: uv[0],
            tv: uv[1],
        };
    }

    /// `SizeOf`: the vertex data plus 8.
    pub fn size_of(&self) -> u32 {
        (self.vertices.len() as u32).wrapping_mul(0x24).wrapping_add(8)
    }
}

/// A `GEPolygonArray` of 16-bit triangle indices (`DrawIndexedPrimitive`'s WORD list).
pub struct PolygonArray {
    indices: Vec<u16>,
    size: u32,
}

impl PolygonArray {
    pub fn new(size: u32) -> Self {
        PolygonArray { indices: vec![0; size as usize * 3], size }
    }

    pub fn size(&self) -> u32 {
        self.size
    }

    pub fn as_mut_ptr(&mut self) -> *mut u16 {
        self.indices.as_mut_ptr()
    }

    pub fn indices(&self) -> &[u16] {
        &self.indices
    }

    /// `GetVertexIndex1..3`: `corner` 0..2 of triangle `polygon`.
    pub fn vertex_index(&self, polygon: u32, corner: u32) -> u32 {
        u32::from(self.indices[polygon as usize * 3 + corner as usize])
    }

    /// `SetVertexIndex1..3`: stored as 16 bits (the high bits are dropped).
    pub fn set_vertex_index(&mut self, polygon: u32, corner: u32, index: u32) {
        self.indices[polygon as usize * 3 + corner as usize] = index as u16;
    }

    pub fn set_vertex_indices(&mut self, polygon: u32, i1: u32, i2: u32, i3: u32) {
        let p = polygon as usize * 3;
        self.indices[p] = i1 as u16;
        self.indices[p + 1] = i2 as u16;
        self.indices[p + 2] = i3 as u16;
    }

    /// `SizeOf`: the index buffer plus 12 bytes.
    pub fn size_of(&self) -> u32 {
        (self.indices.len() as u32).wrapping_mul(2).wrapping_add(12)
    }
}

// ---- the C++ GEVertexArray's and GEPolygonArray's object (R4.3 part 17: the renderer's DX7StandardVertexArray,
// DX7StdColorVertexArray and DX7PolygonArray trampolines merged into the two GE classes)

/// A `GEVertexArray`'s Rust array: type 0 standard (`GXVERTEX`), 1 coloured (`GXVERTEX_STD_COLOR`).
pub enum EeGeVertexArray {
    Standard(StandardVertexArray),
    Color(StdColorVertexArray),
}

/// A `GEPolygonArray`'s Rust array.
pub struct EeGePolygonArray(pub PolygonArray);

/// A vertex field of `ee_ge_va_get` / `ee_ge_va_set` (three floats, two for `Uv`, one for `Opacity`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeVaField {
    Color = 1,
    Normal,
    Opacity,
    Point,
    Uv,
}

/// `new GEVertexArray(type, count)`: type 1 coloured, any other standard (the rasterizer makes only 0 and 1).
#[no_mangle]
pub extern "C" fn ee_ge_va_new(kind: u32, size: u32) -> *mut EeGeVertexArray {
    Box::into_raw(Box::new(if kind == 1 {
        EeGeVertexArray::Color(StdColorVertexArray::new(size))
    } else {
        EeGeVertexArray::Standard(StandardVertexArray::new(size))
    }))
}

/// # Safety
/// `a` is from `ee_ge_va_new` and not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_va_destroy(a: *mut EeGeVertexArray) {
    // SAFETY: the caller's contract
    drop(unsafe { Box::from_raw(a) });
}

/// The vertex memory (GX and the draws read it in place).
///
/// # Safety
/// `a` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_va_vertices(a: *mut EeGeVertexArray) -> *mut c_void {
    // SAFETY: the caller's contract
    match unsafe { &mut *a } {
        EeGeVertexArray::Standard(v) => v.as_mut_ptr() as *mut c_void,
        EeGeVertexArray::Color(v) => v.as_mut_ptr() as *mut c_void,
    }
}

/// `GetType()`.
///
/// # Safety
/// `a` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_va_type(a: *const EeGeVertexArray) -> c_ulong {
    // SAFETY: the caller's contract
    match unsafe { &*a } {
        EeGeVertexArray::Standard(_) => 0,
        EeGeVertexArray::Color(_) => 1,
    }
}

/// `GetSize()`, or with `size_of` `SizeOf()`.
///
/// # Safety
/// `a` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_va_size(a: *const EeGeVertexArray, size_of: bool) -> c_ulong {
    // SAFETY: the caller's contract
    let n = match unsafe { &*a } {
        EeGeVertexArray::Standard(v) => {
            if size_of {
                v.size_of()
            } else {
                v.len()
            }
        }
        EeGeVertexArray::Color(v) => {
            if size_of {
                v.size_of()
            } else {
                v.len()
            }
        }
    };
    n as c_ulong
}

/// `CopyFrom(b)`: as many vertices as both hold (the original cast `b` to its own class: arrays of two types
/// copy nothing here, where the original misread the other's vertices).
///
/// # Safety
/// Both are live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_va_copy_from(a: *mut EeGeVertexArray, b: *const EeGeVertexArray) {
    // SAFETY: the caller's contract
    match unsafe { (&mut *a, &*b) } {
        (EeGeVertexArray::Standard(x), EeGeVertexArray::Standard(y)) => x.copy_from(y),
        (EeGeVertexArray::Color(x), EeGeVertexArray::Color(y)) => x.copy_from(y),
        _ => {}
    }
}

/// `Get<field>(out, index)` into `out` (its floats).
///
/// # Safety
/// `a` is live, `index` one of its vertices; `out` holds the field's floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_va_get(a: *const EeGeVertexArray, field: EeGeVaField, index: u32, out: *mut f32) {
    // SAFETY: the caller's contract
    let a = unsafe { &*a };
    let values: Vec<f32> = match (a, field) {
        (EeGeVertexArray::Standard(v), EeGeVaField::Color) => v.color(index).to_vec(),
        (EeGeVertexArray::Color(v), EeGeVaField::Color) => v.color(index).to_vec(),
        (EeGeVertexArray::Standard(v), EeGeVaField::Normal) => v.normal(index).to_vec(),
        (EeGeVertexArray::Color(v), EeGeVaField::Normal) => v.normal(index).to_vec(),
        (EeGeVertexArray::Standard(v), EeGeVaField::Opacity) => vec![v.opacity(index)],
        (EeGeVertexArray::Color(v), EeGeVaField::Opacity) => vec![v.opacity(index)],
        (EeGeVertexArray::Standard(v), EeGeVaField::Point) => v.point(index).to_vec(),
        (EeGeVertexArray::Color(v), EeGeVaField::Point) => v.point(index).to_vec(),
        (EeGeVertexArray::Standard(v), EeGeVaField::Uv) => v.texture_coordinate(index).to_vec(),
        (EeGeVertexArray::Color(v), EeGeVaField::Uv) => v.texture_coordinate(index).to_vec(),
    };
    // SAFETY: `out` holds the field's floats
    unsafe { std::ptr::copy_nonoverlapping(values.as_ptr(), out, values.len()) };
}

/// `Set<field>(value, index)` from `value` (its floats); a standard array ignores colour and opacity.
///
/// # Safety
/// `a` is live, `index` one of its vertices; `value` holds the field's floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_va_set(a: *mut EeGeVertexArray, field: EeGeVaField, index: u32, value: *const f32) {
    // SAFETY: the caller's contract
    unsafe {
        let v3 = || [*value, *value.add(1), *value.add(2)];
        let v2 = || [*value, *value.add(1)];
        match (&mut *a, field) {
            (EeGeVertexArray::Standard(_), EeGeVaField::Color | EeGeVaField::Opacity) => {}
            (EeGeVertexArray::Color(v), EeGeVaField::Color) => v.set_color(v3(), index),
            (EeGeVertexArray::Color(v), EeGeVaField::Opacity) => v.set_opacity(*value, index),
            (EeGeVertexArray::Standard(v), EeGeVaField::Normal) => v.set_normal(v3(), index),
            (EeGeVertexArray::Color(v), EeGeVaField::Normal) => v.set_normal(v3(), index),
            (EeGeVertexArray::Standard(v), EeGeVaField::Point) => v.set_point(v3(), index),
            (EeGeVertexArray::Color(v), EeGeVaField::Point) => v.set_point(v3(), index),
            (EeGeVertexArray::Standard(v), EeGeVaField::Uv) => v.set_texture_coordinate(v2(), index),
            (EeGeVertexArray::Color(v), EeGeVaField::Uv) => v.set_texture_coordinate(v2(), index),
        }
    }
}

/// `SetVertex(colour, point, normal, uv, opacity, index)` (three, three, three and two floats).
///
/// # Safety
/// `a` is live, `index` one of its vertices; the pointers hold their floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_va_set_vertex(
    a: *mut EeGeVertexArray,
    c: *const f32,
    p: *const f32,
    n: *const f32,
    uv: *const f32,
    opacity: f32,
    index: u32,
) {
    // SAFETY: the caller's contract
    unsafe {
        let v3 = |q: *const f32| [*q, *q.add(1), *q.add(2)];
        match &mut *a {
            EeGeVertexArray::Standard(v) => v.set_vertex(v3(p), v3(n), [*uv, *uv.add(1)], index),
            EeGeVertexArray::Color(v) => v.set_vertex(v3(c), v3(p), v3(n), [*uv, *uv.add(1)], opacity, index),
        }
    }
}

/// `new GEPolygonArray(count)`.
#[no_mangle]
pub extern "C" fn ee_ge_pa_new(size: u32) -> *mut EeGePolygonArray {
    Box::into_raw(Box::new(EeGePolygonArray(PolygonArray::new(size))))
}

/// # Safety
/// `p` is from `ee_ge_pa_new` and not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_pa_destroy(p: *mut EeGePolygonArray) {
    // SAFETY: the caller's contract
    drop(unsafe { Box::from_raw(p) });
}

/// The 16-bit index memory (GX and the draws read it in place).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_pa_indices(p: *mut EeGePolygonArray) -> *mut u16 {
    // SAFETY: the caller's contract
    unsafe { (*p).0.as_mut_ptr() }
}

/// `GetSize()`, or with `size_of` `SizeOf()`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_pa_size(p: *const EeGePolygonArray, size_of: bool) -> c_ulong {
    // SAFETY: the caller's contract
    let p = unsafe { &(*p).0 };
    (if size_of { p.size_of() } else { p.size() }) as c_ulong
}

/// `GetVertexIndex{corner + 1}(polygon)`.
///
/// # Safety
/// `p` is live; `polygon` one of its triangles.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_pa_index(p: *const EeGePolygonArray, polygon: u32, corner: u32) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { (*p).0.vertex_index(polygon, corner) as c_ulong }
}

/// `SetVertexIndex{corner + 1}(polygon, index)` (16 bits kept).
///
/// # Safety
/// `p` is live; `polygon` one of its triangles.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_pa_set_index(p: *mut EeGePolygonArray, polygon: u32, corner: u32, index: c_ulong) {
    // SAFETY: the caller's contract
    #[allow(clippy::unnecessary_cast)] // c_ulong is u64 on LP64
    unsafe {
        (*p).0.set_vertex_index(polygon, corner, index as u32)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // Golden values: printed by a scratch C++ generator (not checked in): `lrintf` and the GX.h macros on these inputs,
    // compiled with clang-cl -O1, the same evaluation as the shipped C++.
    #[test]
    fn float_to_long_rounds_ties_to_even_like_lrintf() {
        for (input, expected) in [
            (0.0f32, 0),
            (0.5, 0),
            (1.5, 2),
            (2.5, 2),
            (-0.5, 0),
            (-1.5, -2),
            (-2.5, -2),
            (254.5, 254),
            (255.0, 255),
            (127.49999, 127),
            (1_048_576.5, 1_048_576),
            (2_147_483_500.0, 2_147_483_520),
        ] {
            assert_eq!(float_to_long(input), expected, "{input}");
        }
    }

    #[test]
    fn float_to_long_out_of_range_is_x87_indefinite() {
        assert_eq!(float_to_long(3e20), i32::MIN);
        assert_eq!(float_to_long(-3e20), i32::MIN);
        assert_eq!(float_to_long(f32::NAN), i32::MIN);
    }

    #[test]
    fn color_packing_goldens() {
        assert_eq!(rgba(255, 0, 128, 64), 0x40ff_0080);
        assert_eq!(rgba(300, 256, 0, 255), 0xff00_0000 | (300 << 16) | (256 << 8));
        assert_eq!(rgba(-1, 0, 0, 255), 0xffff_0000);
        assert_eq!(rgb_make(12, 34, 56), 0x000c_2238);
        assert_eq!(rgb_make(256, 0, 0), 0x0100_0000);
    }

    #[test]
    fn standard_array_reads_and_ignores_colour() {
        let mut a = StandardVertexArray::new(3);
        assert_eq!(a.len(), 3);
        assert_eq!(a.color(1), [1.0, 1.0, 1.0]);
        assert_eq!(a.opacity(1), 1.0);
        a.set_point([1.0, 2.0, 3.0], 1);
        a.set_normal([4.0, 5.0, 6.0], 1);
        a.set_texture_coordinate([7.0, 8.0], 1);
        assert_eq!(a.point(1), [1.0, 2.0, 3.0]);
        assert_eq!(a.normal(1), [4.0, 5.0, 6.0]);
        assert_eq!(a.texture_coordinate(1), [7.0, 8.0]);
        a.set_vertex([9.0, 8.0, 7.0], [6.0, 5.0, 4.0], [3.0, 2.0], 2);
        assert_eq!(a.point(2), [9.0, 8.0, 7.0]);
        assert_eq!(a.size_of(), 3 * 32 + 8);
    }

    #[test]
    fn standard_copy_takes_the_shorter() {
        let mut a = StandardVertexArray::new(2);
        let mut b = StandardVertexArray::new(4);
        b.set_point([1.0, 1.0, 1.0], 0);
        b.set_point([2.0, 2.0, 2.0], 1);
        b.set_point([3.0, 3.0, 3.0], 3);
        a.copy_from(&b);
        assert_eq!(a.point(1), [2.0, 2.0, 2.0]);
        let mut c = StandardVertexArray::new(4);
        c.set_point([9.0, 9.0, 9.0], 3);
        c.copy_from(&a);
        assert_eq!(c.point(1), [2.0, 2.0, 2.0]);
        assert_eq!(c.point(3), [9.0, 9.0, 9.0], "beyond the shorter array untouched");
    }

    #[test]
    fn color_array_packing_goldens() {
        let mut a = StdColorVertexArray::new(4);
        a.set_vertex([1.0, 0.5, 0.25], [1.0, 2.0, 3.0], [0.0, 1.0, 0.0], [0.25, 0.75], 0.5, 0);
        // RGBA(lrintf(255), lrintf(127.5)=128, lrintf(63.75)=64, lrintf(127.5)=128)
        assert_eq!(a.vertices()[0].color, 0x80ff_8040);
        assert_eq!(a.point(0), [1.0, 2.0, 3.0]);
        assert_eq!(a.normal(0), [0.0, 1.0, 0.0]);
        assert_eq!(a.texture_coordinate(0), [0.25, 0.75]);
        let k: f32 = 1.0 / 255.0;
        assert_eq!(a.color(0), [255.0 * k, 128.0 * k, 64.0 * k]);
        assert_eq!(a.opacity(0), 128.0 * k);
        // SetColor keeps alpha; SetOpacity keeps colour; both unclamped in the same wrap
        a.set_color([2.0, 0.0, 0.0], 0);
        assert_eq!(a.vertices()[0].color, 0x8000_0000 | (510u32 << 16));
        a.set_opacity(0.0, 0);
        assert_eq!(a.vertices()[0].color, 0x00fe_0000, "the opacity write keeps only the low 24 bits");
        assert_eq!(a.size_of(), 4 * 0x24 + 8);
    }

    #[test]
    fn polygon_indices_are_16_bit_and_size_goldens() {
        let mut p = PolygonArray::new(2);
        p.set_vertex_indices(1, 1, 2, 3);
        assert_eq!(p.vertex_index(1, 0), 1);
        assert_eq!(p.vertex_index(1, 2), 3);
        p.set_vertex_index(0, 1, 0x1_2345);
        assert_eq!(p.vertex_index(0, 1), 0x2345, "stored as 16 bits");
        assert_eq!(p.size(), 2);
        assert_eq!(p.size_of(), 2 * 3 * 2 + 12);
    }
}
