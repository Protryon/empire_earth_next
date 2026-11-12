//! The effects' built meshes as values: a vertex record (which of point, normal, texture coordinate, colour and
//! opacity a builder set, and their values) and a triangle record, written by the Rust builders (`farm.rs`,
//! `volcano.rs`) in the original's order and stored by the C++ into the model's vertex and polygon arrays
//! (`EEEffect3D::StoreMesh`). Only the final state is stored: the original's setters write the arrays only, so a
//! later write to the same slot wins in both.
//!
//! A write past the arrays (an index the original would have stored out of bounds) is dropped and counted.

use ee_ge::types::{Ge3DPoint, GeVector};

use crate::effects::{EeGfxColor, EeGfxTexCoord};

/// `EeGfxMeshVertex::set`: the point was set.
pub const EE_GFX_MESH_POINT: u32 = 1;
/// The normal was set.
pub const EE_GFX_MESH_NORMAL: u32 = 2;
/// The texture coordinate was set.
pub const EE_GFX_MESH_COORD: u32 = 4;
/// The colour was set.
pub const EE_GFX_MESH_COLOR: u32 = 8;
/// The opacity was set.
pub const EE_GFX_MESH_OPACITY: u32 = 16;

/// One vertex of a built mesh: `set` names the values a builder wrote.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxMeshVertex {
    pub set: u32,
    pub point: Ge3DPoint,
    pub normal: GeVector,
    pub coord: EeGfxTexCoord,
    pub color: EeGfxColor,
    pub opacity: f32,
}

/// One triangle of a built mesh: `set` is non-zero once a builder wrote its three indices.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxMeshTriangle {
    pub set: u32,
    pub v: [u32; 3],
}

/// A builder's view of the C++'s output arrays.
pub struct Mesh<'a> {
    pub vertices: &'a mut [EeGfxMeshVertex],
    pub triangles: &'a mut [EeGfxMeshTriangle],
    /// Writes dropped because their index was past the arrays.
    pub dropped: u32,
}

impl<'a> Mesh<'a> {
    pub fn new(vertices: &'a mut [EeGfxMeshVertex], triangles: &'a mut [EeGfxMeshTriangle]) -> Mesh<'a> {
        Mesh { vertices, triangles, dropped: 0 }
    }

    fn vertex(&mut self, i: i64) -> Option<&mut EeGfxMeshVertex> {
        match usize::try_from(i).ok().and_then(|i| self.vertices.get_mut(i)) {
            Some(v) => Some(v),
            None => {
                self.dropped += 1;
                None
            }
        }
    }

    pub fn point(&mut self, i: i64, p: Ge3DPoint) {
        if let Some(v) = self.vertex(i) {
            v.set |= EE_GFX_MESH_POINT;
            v.point = p;
        }
    }

    pub fn normal(&mut self, i: i64, n: GeVector) {
        if let Some(v) = self.vertex(i) {
            v.set |= EE_GFX_MESH_NORMAL;
            v.normal = n;
        }
    }

    pub fn coord(&mut self, i: i64, u: f32, w: f32) {
        if let Some(v) = self.vertex(i) {
            v.set |= EE_GFX_MESH_COORD;
            v.coord = EeGfxTexCoord { u, v: w };
        }
    }

    pub fn color(&mut self, i: i64, c: EeGfxColor) {
        if let Some(v) = self.vertex(i) {
            v.set |= EE_GFX_MESH_COLOR;
            v.color = c;
        }
    }

    pub fn opacity(&mut self, i: i64, o: f32) {
        if let Some(v) = self.vertex(i) {
            v.set |= EE_GFX_MESH_OPACITY;
            v.opacity = o;
        }
    }

    /// Triangle `p`'s three vertex indices (as unsigned longs, the original's `SetVertexIndex` argument).
    pub fn triangle(&mut self, p: i64, a: i64, b: i64, c: i64) {
        match usize::try_from(p).ok().and_then(|p| self.triangles.get_mut(p)) {
            Some(t) => {
                t.set = 1;
                t.v = [a as u32, b as u32, c as u32];
            }
            None => self.dropped += 1,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn writes_past_the_arrays_are_dropped() {
        let mut v = [EeGfxMeshVertex::default(); 2];
        let mut t = [EeGfxMeshTriangle::default(); 1];
        let mut m = Mesh::new(&mut v, &mut t);
        m.point(1, Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 });
        m.opacity(1, 0.5);
        m.point(2, Ge3DPoint::default());
        m.opacity(-1, 0.5);
        m.triangle(0, 0, 1, 1);
        m.triangle(1, 0, 1, 1);
        assert_eq!(m.dropped, 3);
        assert_eq!(v[1].set, EE_GFX_MESH_POINT | EE_GFX_MESH_OPACITY);
        assert_eq!(v[0].set, 0);
        assert_eq!(t[0].v, [0, 1, 1]);
    }
}
