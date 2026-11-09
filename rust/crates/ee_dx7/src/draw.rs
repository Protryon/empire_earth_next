//! The rasterizer's draw paths in Rust: the shared host seam and small value types. C++ keeps `DX7Rasterizer` and the
//! GE objects; each `DX7Rasterizer::Draw*` forwards here, and the steps that touch C++ objects (the rasterizer, a
//! `GETexture`, the device) go through [`DrawHost`] so that the algorithms test with a recorder and no game.
//!
//! No Rust reference to C++-owned memory lives across a host call: vertex memory is reached through raw pointers
//! written or read between calls, and `ScreenState` is only touched inside the host. Golden: `golden_draw.txt`
//! (printed by a scratch run of the original C++ bodies).
#![allow(clippy::unnecessary_cast)] // c_ulong/c_long differ per target

use core::ffi::{c_long, c_void};

use crate::vertex::{float_to_long, Dx7StdColorVertex};

/// A C++ object seen from Rust.
pub type Handle = *mut c_void;

/// `GERasterizer::kErrorOK` style results are plain `long`s; DX7Rasterizer never inspects them beyond returning them.
pub type LongResult = c_long;

/// The `GXERR_WASSTILLDRAWING` every draw retries on.
pub(crate) const WAS_STILL_DRAWING: i32 = (0x8876_0000u32 | 540) as i32;

pub(crate) const PT_POINTLIST: u32 = 1;
pub(crate) const PT_LINELIST: u32 = 2;
pub(crate) const PT_TRIANGLELIST: u32 = 4;

pub(crate) const TS_WORLD: u32 = 1;
pub(crate) const TS_VIEW: u32 = 2;
pub(crate) const TS_PROJECTION: u32 = 3;

/// `GETransformation`: row-major, translation in the last column.
pub type Transform = [[f32; 4]; 4];

/// `GXMATRIX`: 16 floats in D3D order.
pub type Matrix = [f32; 16];

/// `GXMATERIAL` (diffuse, ambient, specular, emissive, power).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Material {
    pub diffuse: [f32; 4],
    pub ambient: [f32; 4],
    pub specular: [f32; 4],
    pub emissive: [f32; 4],
    pub power: f32,
}

impl Material {
    /// The white material `Draw3DLine`, `DrawGridLines` and `DrawModelHierarchy` start from.
    pub const WHITE: Material =
        Material { diffuse: [1.0; 4], ambient: [1.0; 4], specular: [0.0; 4], emissive: [0.0; 4], power: 0.0 };
}

pub const IDENTITY: Transform =
    [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]];

/// `ConvertToD3DMatrix`: the transpose (`_ij = element(j-1, i-1)`).
pub fn to_d3d(t: &Transform) -> Matrix {
    let mut m = [0.0f32; 16];
    let mut k = 0;
    for column in 0..4 {
        for row in t {
            m[k] = row[column];
            k += 1;
        }
    }
    m
}

/// The fixed view transformation of the 3D world draws: identity with the Y and Z axes swapped.
pub fn y_up_to_z_up() -> Transform {
    let mut t = IDENTITY;
    t[1][1] = 0.0;
    t[1][2] = 1.0;
    t[2][1] = 1.0;
    t[2][2] = 0.0;
    t
}

/// `GETransformation::Apply(point)`, in the C++ operand order (bit-exact f32).
#[inline]
pub fn apply_point(t: &Transform, p: [f32; 3]) -> [f32; 3] {
    [
        p[2] * t[0][2] + p[1] * t[0][1] + p[0] * t[0][0] + t[0][3],
        p[0] * t[1][0] + p[2] * t[1][2] + p[1] * t[1][1] + t[1][3],
        p[0] * t[2][0] + p[2] * t[2][2] + p[1] * t[2][1] + t[2][3],
    ]
}

/// `GETransformation::Apply(vector)`.
#[inline]
pub fn apply_vector(t: &Transform, v: [f32; 3]) -> [f32; 3] {
    [
        v[2] * t[0][2] + v[1] * t[0][1] + v[0] * t[0][0],
        v[0] * t[1][0] + v[2] * t[1][2] + v[1] * t[1][1],
        v[0] * t[2][0] + v[2] * t[2][2] + v[1] * t[2][1],
    ]
}

/// `GETransformation::Apply(transformation)`: the composition `this x other`; the bottom row comes from `b`.
pub fn compose(a: &Transform, b: &Transform) -> Transform {
    let mut o = [[0.0f32; 4]; 4];
    for r in 0..3 {
        o[r][0] = a[r][0] * b[0][0] + a[r][1] * b[1][0] + a[r][2] * b[2][0];
        o[r][1] = a[r][0] * b[0][1] + a[r][1] * b[1][1] + a[r][2] * b[2][1];
        o[r][2] = a[r][0] * b[0][2] + a[r][1] * b[1][2] + a[r][2] * b[2][2];
        o[r][3] = a[r][0] * b[0][3] + a[r][1] * b[1][3] + a[r][2] * b[2][3] + a[r][3];
    }
    o[3] = b[3];
    o
}

/// `GEVector::Normalize`: (0,0,0) when the magnitude is within 0.0001 of zero, else scaled by its reciprocal.
pub fn normalize(v: [f32; 3]) -> [f32; 3] {
    let magnitude = (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]).sqrt();
    if (-0.0001..=0.0001).contains(&magnitude) {
        [0.0, 0.0, 0.0]
    } else {
        let scale = 1.0 / magnitude;
        [v[0] * scale, v[1] * scale, v[2] * scale]
    }
}

/// `GEColor::Normalize`: clamp to 0..1 (NaN stays).
#[inline]
pub fn clamp_unit(c: f32) -> f32 {
    c.clamp(0.0, 1.0)
}

/// `UMFloor`: round to nearest, then down when above the input.
#[inline]
pub fn um_floor(v: f32) -> c_long {
    let r = float_to_long(v) as c_long;
    if (r as f32) > v {
        r - 1
    } else {
        r
    }
}

/// `UMCeil`.
#[inline]
pub fn um_ceil(v: f32) -> c_long {
    let r = float_to_long(v) as c_long;
    if (r as f32) < v {
        r + 1
    } else {
        r
    }
}

/// The shared rasterizer services every draw path needs. One host call at a time: implementations may reach C++.
pub trait DrawHost {
    fn enter(&mut self);
    fn leave(&mut self);
    fn set_2d_mode(&mut self);
    fn set_3d_mode(&mut self);
    fn set_texture(&mut self, texture: Handle);
    fn set_shading_color_source(&mut self, source: u32);
    /// `LockVB`: the vertex buffer's base pointer (the caller offsets by [`DrawHost::vb_position`]).
    fn lock_vb(&mut self, count: u32) -> *mut Dx7StdColorVertex;
    fn unlock_vb(&mut self) -> LongResult;
    fn vb_position(&self) -> u32;
    fn advance_vb(&mut self, count: u32);
    /// `DrawPrimitiveVB` once: the caller retries on [`WAS_STILL_DRAWING`].
    fn draw_primitive(&mut self, primitive: u32, start: u32, count: u32) -> i32;
    /// `DrawIndexedPrimitiveVB` once.
    fn draw_indexed(&mut self, primitive: u32, start: u32, count: u32, indices: *const u16, index_count: u32) -> i32;
    fn set_transform(&mut self, kind: u32, matrix: &Matrix) -> LongResult;
    fn set_render_state(&mut self, state: u32, value: u32) -> LongResult;
    fn set_material(&mut self, material: &Material);
    /// `PlatSleep(0)`.
    fn yield_now(&mut self);
    /// `m32C[1] +=`: the drawn-triangle counter.
    fn add_triangles(&mut self, count: u32);
}

/// `DrawPrimitiveVB` with the C++ `WASSTILLDRAWING` retry loop.
pub fn draw_primitive_retry(h: &mut impl DrawHost, primitive: u32, start: u32, count: u32) {
    while h.draw_primitive(primitive, start, count) == WAS_STILL_DRAWING {
        h.yield_now();
    }
}

/// `DrawIndexedPrimitiveVB` with the retry loop.
pub fn draw_indexed_retry(h: &mut impl DrawHost, primitive: u32, start: u32, count: u32, indices: *const u16, n: u32) {
    while h.draw_indexed(primitive, start, count, indices, n) == WAS_STILL_DRAWING {
        h.yield_now();
    }
}

/// The world/view/projection setup shared by the 3D world draws (`Draw3DLine`, the terrain and water draws): identity
/// world, the fixed Y/Z swapped view, then the projection chosen by `orthographic`. Returns the view call's result
/// (`Draw(terrain)` and `DrawWater` keep it).
pub fn set_world_view_projection(
    h: &mut impl DrawHost,
    perspective: &Transform,
    orthographic: &Transform,
    ortho: bool,
) -> LongResult {
    h.set_transform(TS_WORLD, &to_d3d(&IDENTITY));
    let view = h.set_transform(TS_VIEW, &to_d3d(&y_up_to_z_up()));
    h.set_transform(TS_PROJECTION, &to_d3d(if ortho { orthographic } else { perspective }));
    view
}

#[cfg(test)]
pub(crate) mod testing {
    //! Golden-file helper shared by the draw tests: `golden_draw.txt`, printed by a scratch run of the original C++
    //! bodies, split in `=== name` sections, and the recording base host.
    use super::*;

    pub const GOLDEN: &str = include_str!("golden_draw.txt");

    pub fn golden_section(name: &str) -> Vec<String> {
        let header = format!("=== {name}");
        let mut lines = GOLDEN.lines();
        for line in &mut lines {
            if line == header {
                let mut out = Vec::new();
                for l in lines {
                    if l.starts_with("=== ") {
                        break;
                    }
                    out.push(l.trim_end_matches('\r').to_string());
                }
                return out;
            }
        }
        panic!("golden section {name} missing");
    }

    /// Bits of a float, as the golden file prints them.
    pub fn b(f: f32) -> String {
        format!("{:08x}", f.to_bits())
    }

    /// The vertex buffer, the log and the object names, as the scratch generator's stubs had them.
    pub struct Rec {
        pub log: Vec<String>,
        pub vb: Vec<Dx7StdColorVertex>,
        pub position: u32,
        pub polys: u32,
        pub busy: u32,
    }

    pub fn tex_name(h: Handle, names: &[(Handle, &str)]) -> String {
        if h.is_null() {
            return "NULL".into();
        }
        names.iter().find(|(p, _)| *p == h).map(|(_, n)| n.to_string()).unwrap_or_else(|| "?".into())
    }

    impl Rec {
        pub fn new() -> Rec {
            Rec { log: Vec::new(), vb: vec![Dx7StdColorVertex::default(); 0x960 + 8], position: 0, polys: 0, busy: 0 }
        }

        pub fn dump_v(&self, i: u32) -> String {
            let v = &self.vb[i as usize];
            format!(
                "  v {} {} {} {} {} {} {:08x} {} {}",
                b(v.x),
                b(v.y),
                b(v.z),
                b(v.nx),
                b(v.ny),
                b(v.nz),
                v.color,
                b(v.tu),
                b(v.tv)
            )
        }

        pub fn dump_range(&mut self, start: u32, count: u32) {
            if count <= 30 {
                for i in 0..count {
                    let s = self.dump_v(start + i);
                    self.log.push(s);
                }
                return;
            }
            for i in 0..6 {
                let s = self.dump_v(start + i);
                self.log.push(s);
            }
            let mut h: u32 = 2166136261;
            for i in 0..count {
                let v = self.vb[(start + i) as usize];
                let words: [u32; 9] = [
                    v.x.to_bits(),
                    v.y.to_bits(),
                    v.z.to_bits(),
                    v.nx.to_bits(),
                    v.ny.to_bits(),
                    v.nz.to_bits(),
                    v.color,
                    v.tu.to_bits(),
                    v.tv.to_bits(),
                ];
                for w in words {
                    h ^= w;
                    h = h.wrapping_mul(16777619);
                }
            }
            self.log.push(format!("  vsum {h:08x}"));
        }

        pub fn end_line(&self) -> String {
            format!("pos {} polys {}", self.position, self.polys)
        }
    }

    /// The default [`DrawHost`] recorder: every method logs the golden-format line the C++ stubs printed.
    macro_rules! impl_draw_host_for_rec {
        ($ty:ty, $names:ident) => {
            impl DrawHost for $ty {
                fn enter(&mut self) {
                    self.rec.log.push("enter".into());
                }
                fn leave(&mut self) {
                    self.rec.log.push("leave".into());
                }
                fn set_2d_mode(&mut self) {
                    self.rec.log.push("set2d".into());
                }
                fn set_3d_mode(&mut self) {
                    self.rec.log.push("set3d".into());
                }
                fn set_texture(&mut self, texture: Handle) {
                    let n = crate::draw::testing::tex_name(texture, &self.$names);
                    self.rec.log.push(format!("settex {n}"));
                }
                fn set_shading_color_source(&mut self, source: u32) {
                    self.rec.log.push(format!("shadesrc {source}"));
                }
                fn lock_vb(&mut self, count: u32) -> *mut Dx7StdColorVertex {
                    if count > 0x960 - self.rec.position {
                        self.rec.position = 0;
                    }
                    self.rec.log.push(format!("lock {count}"));
                    self.rec.vb.as_mut_ptr()
                }
                fn unlock_vb(&mut self) -> LongResult {
                    self.rec.log.push("unlock".into());
                    11
                }
                fn vb_position(&self) -> u32 {
                    self.rec.position
                }
                fn advance_vb(&mut self, count: u32) {
                    self.rec.position += count;
                }
                fn draw_primitive(&mut self, primitive: u32, start: u32, count: u32) -> i32 {
                    if self.rec.busy > 0 {
                        self.rec.busy -= 1;
                        self.rec.log.push("drawprim-busy".into());
                        return crate::draw::WAS_STILL_DRAWING;
                    }
                    self.rec.log.push(format!("drawprim {primitive} VB {start} {count}"));
                    self.rec.dump_range(start, count);
                    0
                }
                fn draw_indexed(&mut self, primitive: u32, start: u32, count: u32, indices: *const u16, n: u32) -> i32 {
                    if self.rec.busy > 0 {
                        self.rec.busy -= 1;
                        self.rec.log.push("drawprim-busy".into());
                        return crate::draw::WAS_STILL_DRAWING;
                    }
                    let mut line = format!("drawidx {primitive} VB {start} {count}");
                    for i in 0..n {
                        let v = unsafe { *indices.add(i as usize) };
                        line.push_str(&format!(" {v}"));
                    }
                    self.rec.log.push(line);
                    self.rec.dump_range(start, count);
                    0
                }
                fn set_transform(&mut self, kind: u32, matrix: &Matrix) -> LongResult {
                    let mut line = format!("transform {kind}");
                    for f in matrix {
                        line.push_str(&format!(" {}", crate::draw::testing::b(*f)));
                    }
                    self.rec.log.push(line);
                    7
                }
                fn set_render_state(&mut self, state: u32, value: u32) -> LongResult {
                    self.rec.log.push(format!("rs {state} {value}"));
                    8
                }
                fn set_material(&mut self, m: &Material) {
                    let mut line = String::from("material");
                    for f in m.diffuse.iter().chain(&m.ambient).chain(&m.specular).chain(&m.emissive) {
                        line.push_str(&format!(" {}", crate::draw::testing::b(*f)));
                    }
                    line.push_str(&format!(" {}", crate::draw::testing::b(m.power)));
                    self.rec.log.push(line);
                }
                fn yield_now(&mut self) {
                    self.rec.log.push("sleep 0".into());
                }
                fn add_triangles(&mut self, count: u32) {
                    self.rec.polys += count;
                }
            }
        };
    }
    pub(crate) use impl_draw_host_for_rec;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn um_floor_and_ceil_follow_the_rounded_value() {
        assert_eq!(um_floor(2.5), 2); // rounds to 2 (even), not above the input
        assert_eq!(um_floor(3.5), 3); // rounds to 4, above the input: down
        assert_eq!(um_floor(-2.5), -3);
        assert_eq!(um_ceil(2.5), 3);
        assert_eq!(um_ceil(3.5), 4);
        assert_eq!(um_ceil(-2.5), -2);
    }

    #[test]
    fn to_d3d_is_the_transpose() {
        let t: Transform =
            [[1.0, 2.0, 3.0, 4.0], [5.0, 6.0, 7.0, 8.0], [9.0, 10.0, 11.0, 12.0], [13.0, 14.0, 15.0, 16.0]];
        assert_eq!(to_d3d(&t), [1.0, 5.0, 9.0, 13.0, 2.0, 6.0, 10.0, 14.0, 3.0, 7.0, 11.0, 15.0, 4.0, 8.0, 12.0, 16.0]);
    }

    #[test]
    fn compose_copies_the_second_bottom_row() {
        let mut b = IDENTITY;
        b[3] = [5.0, 6.0, 7.0, 8.0];
        let c = compose(&IDENTITY, &b);
        assert_eq!(c[3], b[3]);
        assert_eq!(c[0], [1.0, 0.0, 0.0, 0.0]);
    }

    #[test]
    fn normalize_snaps_tiny_vectors_to_zero() {
        assert_eq!(normalize([0.0, 0.00005, 0.0]), [0.0, 0.0, 0.0]);
        let n = normalize([3.0, 0.0, 4.0]);
        assert!((n[0] - 0.6).abs() < 1e-6 && (n[2] - 0.8).abs() < 1e-6);
    }

    #[test]
    fn clamp_unit_clamps_and_keeps_in_range_values() {
        assert_eq!(clamp_unit(2.0), 1.0);
        assert_eq!(clamp_unit(-0.5), 0.0);
        assert_eq!(clamp_unit(0.25), 0.25);
    }

    #[test]
    fn apply_uses_the_translation_column() {
        let mut t = IDENTITY;
        t[0][3] = 1.0;
        t[1][3] = 2.0;
        t[2][3] = 3.0;
        assert_eq!(apply_point(&t, [1.0, 1.0, 1.0]), [2.0, 3.0, 4.0]);
        assert_eq!(apply_vector(&t, [1.0, 1.0, 1.0]), [1.0, 1.0, 1.0]);
    }
}
