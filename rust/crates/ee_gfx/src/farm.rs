//! The farm field's meshes (`src/Empire Earth/Graphics Thread/EEE3DFarm.cpp`): `BuildGroundModel`'s ground (a quad
//! per tile on the terrain's heights and normals, two triangles each, the texture over the field) and
//! `BuildCrops`' crop rows (per tile row, as many rows of upright quads as the effect's first int parameter, on the
//! terrain's interpolated height). The C++ keeps the models (their creation, materials, the dummy sub-model, the
//! bounds, the placement and the textures) and stores these meshes into both the model and its draw copy.
//!
//! This is simulation state: `EEE3DFarm::Step` builds the crops in the simulation tick, sways their tops from the
//! points stored here and hands the ground model to the unit (`SetScaledModel`). RUST_PLAN 2.1: the C++'s
//! operations in its order, wrapping integers.

use ee_ge::types::{Ge3DPoint, GeVector};
use ee_ge::um;

use crate::mesh::Mesh;

/// `kFarmEpsilon`: the ground's lift over the terrain.
const FARM_EPSILON: f32 = 0.001;

/// The terrain under a field (`GETerrainMesh`'s point rows).
pub trait FarmTerrain {
    /// `GetZ(x, y)`.
    fn z(&mut self, x: u32, y: u32) -> f32;
    /// `*GetVertexNormal(x, y)`.
    fn normal(&mut self, x: u32, y: u32) -> GeVector;
    /// `GetMeshZ(x, y)`.
    fn mesh_z(&mut self, x: f32, y: f32) -> f32;
}

/// A field: its tile area (`mArea`), vertex row length (`mRowLength`) and the farm's tile size (as a float).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxFarmField {
    pub left: f32,
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
    pub row_length: i32,
    pub tile_size: f32,
}

/// The models' place (`SetPosition`'s x, y and z: the field's centre over the height at its bottom left).
pub type EeGfxFarmPlace = Ge3DPoint;

struct Frame {
    scale: f32,
    x_offset: f32,
    y_offset: f32,
    left: i32,
    right: i32,
    top: i32,
    bottom: i32,
    base_z: f32,
}

fn frame(f: &EeGfxFarmField, terrain: &mut dyn FarmTerrain) -> Frame {
    let scale = 1.0 / f.tile_size;
    let x_offset = (f.right - f.left + 1.0) * -0.5;
    let y_offset = (f.bottom - f.top + 1.0) * -0.5;
    let left = um::floor(f.left);
    let right = um::floor(f.right);
    let top = um::floor(f.top);
    let base_z = terrain.z(um::floor_unsigned(f.left), um::floor_unsigned(f.bottom));
    Frame { scale, x_offset, y_offset, left, right, top, bottom: um::floor(f.bottom), base_z }
}

fn place(f: &EeGfxFarmField, fr: &Frame) -> EeGfxFarmPlace {
    Ge3DPoint { x: f.left - fr.x_offset, y: f.top - fr.y_offset, z: fr.base_z }
}

/// `BuildGroundModel`'s mesh into `mesh` (`mRowLength`² vertices, two triangles per tile); returns the place.
pub fn ground(f: &EeGfxFarmField, terrain: &mut dyn FarmTerrain, mesh: &mut Mesh) -> EeGfxFarmPlace {
    let fr = frame(f, terrain);
    let rl = f.row_length;
    let step = 1.0 / (rl.wrapping_sub(1)) as f32;
    let mut vertex: i32 = 0;
    let mut polygon: i32 = 0;
    let mut v = 0.0f32;
    let mut y = fr.bottom;
    while y >= fr.top {
        let mut u = 0.0f32;
        let y1 = (y.wrapping_sub(fr.top).wrapping_add(1) as f32 + fr.y_offset) * fr.scale;
        let y0 = (y.wrapping_sub(fr.top) as f32 + fr.y_offset) * fr.scale;
        let mut x = fr.left;
        while x <= fr.right {
            let (xu, yu) = (x as u32, y as u32);
            let (x1, y1u) = (x.wrapping_add(1) as u32, y.wrapping_add(1) as u32);
            let z0 = terrain.z(xu, yu) - fr.base_z + FARM_EPSILON;
            let z1 = terrain.z(xu, y1u) - fr.base_z + FARM_EPSILON;
            let z2 = terrain.z(x1, y1u) - fr.base_z + FARM_EPSILON;
            let z3 = terrain.z(x1, yu) - fr.base_z + FARM_EPSILON;
            let n0 = terrain.normal(xu, yu);
            let n1 = terrain.normal(xu, y1u);
            let n2 = terrain.normal(x1, y1u);
            let n3 = terrain.normal(x1, yu);
            let xa = (x.wrapping_sub(fr.left) as f32 + fr.x_offset) * fr.scale;
            let xb = (x.wrapping_sub(fr.left).wrapping_add(1) as f32 + fr.x_offset) * fr.scale;
            let (a, b, c, d) = corners(vertex, rl);
            mesh.point(c, Ge3DPoint { x: xa, y: y0, z: z0 * fr.scale });
            mesh.point(a, Ge3DPoint { x: xa, y: y1, z: z1 * fr.scale });
            mesh.point(b, Ge3DPoint { x: xb, y: y1, z: z2 * fr.scale });
            mesh.point(d, Ge3DPoint { x: xb, y: y0, z: z3 * fr.scale });
            mesh.normal(c, n0);
            mesh.normal(a, n1);
            mesh.normal(b, n2);
            mesh.normal(d, n3);
            mesh.coord(a, u, v);
            mesh.coord(b, u + step, v);
            mesh.coord(c, u, v + step);
            mesh.coord(d, u + step, v + step);
            let p = polygon as i64;
            mesh.triangle(p, c, b, d);
            mesh.triangle(p + 1, c, a, b);
            polygon = polygon.wrapping_add(2);
            u += step;
            vertex = vertex.wrapping_add(1);
            x = x.wrapping_add(1);
        }
        vertex = vertex.wrapping_add(1);
        v += step;
        y = y.wrapping_sub(1);
    }
    place(f, &fr)
}

/// A tile's four vertices: `vertex`, `vertex + 1`, `vertex + row`, `vertex + row + 1` (the original's `long`s).
fn corners(vertex: i32, rl: i32) -> (i64, i64, i64, i64) {
    let c = vertex.wrapping_add(rl);
    (vertex as i64, vertex.wrapping_add(1) as i64, c as i64, c.wrapping_add(1) as i64)
}

/// `BuildCrops`' mesh into `mesh`: `rows` (the effect's first int parameter, as the original's unsigned long) crop
/// rows per tile row, each `height` (the effect's size) tall; returns the place.
pub fn crops(
    f: &EeGfxFarmField,
    rows: u32,
    height: f32,
    terrain: &mut dyn FarmTerrain,
    mesh: &mut Mesh,
) -> EeGfxFarmPlace {
    let mut normal = GeVector { x: 1.0, y: -1.0, z: 1.0 };
    let stalk_step = 1.0 / rows as f32;
    normal.normalize();
    let fr = frame(f, terrain);
    let rl = f.row_length;
    let mut vertex: i32 = 0;
    let mut polygon: i32 = 0;
    let mut y = fr.bottom;
    while y >= fr.top {
        let mut stalk_y = y as f32 - stalk_step + 1.0;
        for _ in 0..rows {
            let mut u = 0.0f32;
            let row_y = (fr.y_offset + stalk_y - fr.top as f32) * fr.scale;
            let mut x = fr.left;
            while x <= fr.right {
                let z0 = terrain.mesh_z(x as f32, stalk_y) - fr.base_z + FARM_EPSILON;
                let z1 = terrain.mesh_z(x.wrapping_add(1) as f32, stalk_y) - fr.base_z + FARM_EPSILON;
                let xa = (x.wrapping_sub(fr.left) as f32 + fr.x_offset) * fr.scale;
                let xb = (x.wrapping_sub(fr.left).wrapping_add(1) as f32 + fr.x_offset) * fr.scale;
                let (a, b, c, d) = corners(vertex, rl);
                for i in [a, b, c, d] {
                    mesh.opacity(i, 1.0);
                }
                mesh.point(c, Ge3DPoint { x: xa, y: row_y, z: z0 * fr.scale });
                mesh.point(a, Ge3DPoint { x: xa, y: row_y, z: (z0 + height) * fr.scale });
                mesh.point(b, Ge3DPoint { x: xb, y: row_y, z: (z1 + height) * fr.scale });
                mesh.point(d, Ge3DPoint { x: xb, y: row_y, z: z1 * fr.scale });
                for i in [c, a, b, d] {
                    mesh.normal(i, normal);
                }
                mesh.coord(a, u, 1.0);
                mesh.coord(b, u + 2.0, 1.0);
                mesh.coord(c, u, 0.0);
                mesh.coord(d, u + 2.0, 0.0);
                let p = polygon as i64;
                mesh.triangle(p, c, a, d);
                mesh.triangle(p + 1, a, b, d);
                polygon = polygon.wrapping_add(2);
                u += 2.0;
                vertex = vertex.wrapping_add(1);
                x = x.wrapping_add(1);
            }
            stalk_y -= stalk_step;
            vertex = vertex.wrapping_add(rl).wrapping_add(1);
        }
        y = y.wrapping_sub(1);
    }
    place(f, &fr)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::mesh::{EeGfxMeshTriangle, EeGfxMeshVertex, EE_GFX_MESH_POINT};

    struct Flat;
    impl FarmTerrain for Flat {
        fn z(&mut self, x: u32, y: u32) -> f32 {
            (x + y) as f32 * 0.5
        }
        fn normal(&mut self, _: u32, _: u32) -> GeVector {
            GeVector { x: 0.0, y: 0.0, z: 1.0 }
        }
        fn mesh_z(&mut self, x: f32, y: f32) -> f32 {
            (x + y) * 0.5
        }
    }

    fn field(size: i32) -> EeGfxFarmField {
        EeGfxFarmField {
            left: 10.0,
            top: 20.0,
            right: 10.0 + size as f32 - 1.0,
            bottom: 20.0 + size as f32 - 1.0,
            row_length: size + 1,
            tile_size: size as f32,
        }
    }

    #[test]
    fn the_ground_covers_every_vertex_once_per_tile_corner() {
        let f = field(3);
        let mut v = vec![EeGfxMeshVertex::default(); 16];
        let mut t = vec![EeGfxMeshTriangle::default(); 18];
        let mut m = Mesh::new(&mut v, &mut t);
        let at = ground(&f, &mut Flat, &mut m);
        assert_eq!(m.dropped, 0);
        assert!(v.iter().all(|x| x.set & EE_GFX_MESH_POINT != 0));
        assert!(t.iter().all(|x| x.set != 0));
        assert_eq!(at, Ge3DPoint { x: 11.5, y: 21.5, z: 16.0 });
        // the first tile is the bottom row's left one: its top-left vertex is 0, one unit of x per tile over 3
        assert_eq!(v[0].point.x, -0.5);
        assert_eq!(v[1].point.x, -0.5 / 3.0);
        assert_eq!(t[0].v, [4, 1, 5]);
    }

    #[test]
    fn the_crops_stand_on_the_terrain() {
        let f = field(2);
        let mut v = vec![EeGfxMeshVertex::default(); 2 * 3 * 2 * 2];
        let mut t = vec![EeGfxMeshTriangle::default(); 2 * 2 * 2 * 2];
        let mut m = Mesh::new(&mut v, &mut t);
        crops(&f, 2, 0.5, &mut Flat, &mut m);
        assert_eq!(m.dropped, 0);
        assert!(t.iter().all(|x| x.set != 0));
        // a quad's top is `height` over its foot
        assert_eq!((v[0].point.z - v[3].point.z) * 2.0, 0.5);
    }
}
