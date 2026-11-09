//! The vertex layouts, the colour packing and the arrays (the C++ `DX7StandardVertexArray`, `DX7StdColorVertexArray`
//! and `DX7PolygonArray`, which remain as trampolines). The layouts are the GPU's: `Dx7Vertex` is `GXVERTEX`
//! (stride 0x20), `Dx7StdColorVertex` is `GXVERTEX_STD_COLOR` (stride 0x24).

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
