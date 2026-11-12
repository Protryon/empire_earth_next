//! The volcano's mesh (`EEE3DVolcano::BuildModel`, `src/Empire Earth/Graphics Thread/EEE3DVolcano.cpp`): each tile of
//! its area split 3 x 3 on the terrain's heights, coloured by the terrain's colours and light and blended into the
//! volcano image's colours near the centre, faded out towards the edges; the cone's heights from the image's alpha,
//! blended into the terrain away from the middle. The C++ keeps the image's surface (its creation, load and lock),
//! the model (its creation, material, normals, bounds and texture), the placement and the bounding radius; it hands
//! Rust the locked pixels and the terrain's questions, and stores the mesh.
//!
//! This is the renderer's except for the centre's z: the original leaves `mCenter` at the last vertex's height, and
//! `mCenter` is saved (`EEE3DVolcano::Save`), so the build returns it. RUST_PLAN 2.1: the C++'s operations in its
//! order.

use ee_ge::pixel;
use ee_ge::types::{Ge2DPoint, Ge3DPoint};
use ee_ge::um;

use crate::effects::EeGfxColor;
use crate::mesh::Mesh;

/// The image's pixel scale (0x43f7dc, 1 / 255).
const PIXEL_SCALE: f32 = 1.0 / 255.0;
/// The terrain colour blend.
const COLOR_SCALE: f32 = 0.65;
const COLOR_BIAS: f32 = 0.35;
const SIXTH: f32 = 1.0 / 6.0;
const THIRD: f32 = 1.0 / 3.0;
/// The image's side (`textures\Volcano.png`, 19 x 19).
pub const EE_GFX_VOLCANO_IMAGE_SIDE: usize = 19;

/// The terrain under a volcano.
pub trait VolcanoTerrain {
    /// `mTerrainMesh.GetZ(x, y)`.
    fn z(&mut self, x: u32, y: u32) -> f32;
    /// `EETerrainTable::Get(mTerrainMesh.GetTextureSetID(x, y))->mColor`.
    fn color(&mut self, x: u32, y: u32) -> EeGfxColor;
    /// `mTerrainMesh.GetIsValidTile(x, y)`.
    fn valid_tile(&mut self, x: u32, y: u32) -> bool;
    /// `EEMap::GetVertexColor(x, y, corner)`.
    fn light(&mut self, x: i32, y: i32, corner: i32) -> EeGfxColor;
}

/// A volcano: its area (`mArea`) and centre (`mCenter`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxVolcanoArea {
    pub left: f32,
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
    pub center: Ge3DPoint,
}

/// The mesh's sizes: the vertices a side, the vertices and the triangles.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeGfxVolcanoSizes {
    pub side: i32,
    pub vertices: i32,
    pub polygons: i32,
}

/// What a build leaves: the highest terrain height under the area, the centre's z after it and the writes dropped
/// past the arrays.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxVolcanoBuilt {
    pub maximum: f32,
    pub center_z: f32,
    pub dropped: u32,
}

/// The sizes of a volcano `width` tiles wide (`mArea.GetWidth()`).
pub fn sizes(width: f32) -> EeGfxVolcanoSizes {
    let tiles = um::floor(width);
    let side = tiles.wrapping_mul(3).wrapping_add(1);
    EeGfxVolcanoSizes { side, vertices: side.wrapping_mul(side), polygons: tiles.wrapping_mul(tiles).wrapping_mul(18) }
}

/// The original's clamp: a -0 comes out +0 (`f32::clamp` keeps it).
#[allow(clippy::manual_clamp)]
fn clamp01(v: f32) -> f32 {
    if v >= 1.0 {
        1.0
    } else if v <= 0.0 {
        0.0
    } else {
        v
    }
}

fn tinted(c: EeGfxColor, light: EeGfxColor) -> EeGfxColor {
    EeGfxColor {
        red: light.red * c.red * COLOR_SCALE + COLOR_BIAS,
        green: c.green * light.green * COLOR_SCALE + COLOR_BIAS,
        blue: c.blue * light.blue * COLOR_SCALE + COLOR_BIAS,
    }
}

fn blended(b: f32, image: EeGfxColor, cell: EeGfxColor) -> EeGfxColor {
    let rest = 1.0 - b;
    EeGfxColor {
        red: b * image.red + cell.red * rest,
        green: b * image.green + cell.green * rest,
        blue: b * image.blue + cell.blue * rest,
    }
}

/// `BuildModel`'s image: the locked `textures\Volcano.png` (`row` pixels a row, 19 rows of 19) into the colours
/// (white where the image does not reach, `GEColor`'s default) and the cone's heights (`mHeights`, the effect's
/// size times the alpha) of a mesh `side` vertices a side; returns the writes dropped past the arrays.
pub fn image(
    height_scale: f32,
    side: i32,
    pixels: &[u32],
    row: usize,
    colors: &mut [EeGfxColor],
    heights: &mut [f32],
) -> u32 {
    let mut dropped = 0u32;
    colors.fill(EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 });
    let vertices = colors.len().min(heights.len());
    let side = side.max(0) as usize;
    for i in 0..EE_GFX_VOLCANO_IMAGE_SIDE {
        for j in 0..EE_GFX_VOLCANO_IMAGE_SIDE {
            let value = pixels.get(i + j * row).copied().unwrap_or(0) as std::os::raw::c_ulong;
            let index = i + j * side;
            if index >= vertices {
                dropped += 1;
                continue;
            }
            colors[index] = EeGfxColor {
                red: pixel::unpack_red(0, value) as i32 as f32 * PIXEL_SCALE,
                green: pixel::unpack_green(0, value) as i32 as f32 * PIXEL_SCALE,
                blue: pixel::unpack_blue(0, value) as i32 as f32 * PIXEL_SCALE,
            };
            heights[index] = pixel::unpack_alpha(0, value) as i32 as f32 * height_scale * PIXEL_SCALE;
        }
    }
    dropped
}

/// `BuildModel`'s mesh over the image's `colors` and `heights` (`image`); `points` and `heights` are the effect's
/// `mPoints` and `mHeights` (one per vertex).
pub fn build(
    a: &EeGfxVolcanoArea,
    colors: &[EeGfxColor],
    terrain: &mut dyn VolcanoTerrain,
    points: &mut [Ge3DPoint],
    heights: &mut [f32],
    mesh: &mut Mesh,
) -> EeGfxVolcanoBuilt {
    let mut dropped = 0u32;
    let mut center = a.center;
    let mut maximum = 1.1754944e-38f32;
    let mut tex_v = 0.0f32;
    let right = um::floor(a.right);
    let bottom = um::floor(a.bottom);
    let side = sizes(a.right - a.left).side;
    let vertices = points.len().min(heights.len());
    let side_u = side.max(0) as usize;
    let top_point = Ge3DPoint { x: center.x, y: a.top + 1.0, z: 0.0 };
    let fade = 1.0 / center.distance_to(top_point) * 4.0;
    let fade2 = 1.0 / center.distance_to(top_point) * 4.0;
    let color_at =
        |colors: &[EeGfxColor], i: i32, dropped: &mut u32| match usize::try_from(i).ok().and_then(|i| colors.get(i)) {
            Some(c) => *c,
            None => {
                *dropped += 1;
                EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 }
            }
        };
    let mut cell_colors = [[EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 }; 4]; 4];
    let mut cell_heights = [[0.0f32; 4]; 4];
    let mut y = um::floor(a.top);
    let mut row_base: i32 = 0;
    let row_step = side.wrapping_mul(3);
    let mut vertex: i32 = 0;
    let mut polygon: i32 = 0;
    while bottom < y {
        let mut x = um::floor(a.left);
        let mut tex_u = 0.0f32;
        let mut cell_base: i32 = 0;
        while x < right {
            let (xu, yu) = (x as u32, y as u32);
            let (x1, y1) = (x.wrapping_add(1) as u32, y.wrapping_add(1) as u32);
            let h00 = terrain.z(xu, yu);
            let h01 = terrain.z(xu, y1);
            let h11 = terrain.z(x1, y1);
            let h10 = terrain.z(x1, yu);
            for h in [h00, h01, h11, h10] {
                if maximum <= h {
                    maximum = h;
                }
            }
            let c0 = terrain.color(xu, yu);
            let c1 = if terrain.valid_tile(xu, y1) { terrain.color(xu, y1) } else { c0 };
            let c2 = if terrain.valid_tile(x1, y1) { terrain.color(x1, y1) } else { c0 };
            let c3 = if terrain.valid_tile(x1, yu) { terrain.color(x1, yu) } else { c0 };
            let l2 = terrain.light(x, y, 2);
            let l0 = terrain.light(x, y, 0);
            let l1 = terrain.light(x, y, 1);
            let l3 = terrain.light(x, y, 3);
            let c0 = tinted(c0, l2);
            let c1 = tinted(c1, l0);
            let c2 = tinted(c2, l1);
            let c3 = tinted(c3, l3);
            for r in 0..=3usize {
                let t = r as f32 * THIRD;
                let s = 1.0 - t;
                let left = EeGfxColor {
                    red: c0.red * t + s * c1.red,
                    green: c1.green * s + c0.green * t,
                    blue: c1.blue * s + c0.blue * t,
                };
                let rc = EeGfxColor {
                    red: c3.red * t + s * c2.red,
                    green: c3.green * t + c2.green * s,
                    blue: c3.blue * t + c2.blue * s,
                };
                let lh = s * h01 + t * h00;
                let rh = s * h11 + t * h10;
                for c in 0..=3usize {
                    let rw = c as f32 * THIRD;
                    let w = 1.0 - rw;
                    cell_colors[c][r] = EeGfxColor {
                        red: rc.red * rw + left.red * w,
                        green: rc.green * rw + left.green * w,
                        blue: rc.blue * rw + left.blue * w,
                    };
                    cell_heights[c][r] = lh * w + rw * rh;
                }
            }
            let tile_x = x as f32;
            let mut base = vertex;
            let mut color_row = row_base.wrapping_add(cell_base);
            let tile_y = y as f32 + 1.0;
            for r in 0..3usize {
                let next = r + 1;
                let row_t = r as f32 * THIRD;
                let mut color_next = side.wrapping_add(color_row);
                let mut color_index = color_row;
                let mut next_polygon = polygon.wrapping_add(1);
                let next_y = tile_y - next as f32 * THIRD;
                let next_v = next as f32 * SIXTH + tex_v;
                let mut index = base;
                for c in 1..=3usize {
                    index = index.wrapping_add(1);
                    let prev = c - 1;
                    let col_t = prev as f32 * THIRD;
                    let col_x = c as f32 * THIRD + tile_x;
                    let p1 = Ge3DPoint { x: col_t + tile_x, y: next_y, z: cell_heights[prev][next] };
                    let p2 = Ge3DPoint { x: col_t + tile_x, y: tile_y - row_t, z: cell_heights[prev][r] };
                    let p3 = Ge3DPoint { x: col_x, y: tile_y - row_t, z: cell_heights[c][r] };
                    let p4 = Ge3DPoint { x: col_x, y: next_y, z: cell_heights[c][next] };
                    let i1 = side.wrapping_sub(1).wrapping_add(index);
                    let i2 = base;
                    let i3 = index;
                    let i4 = side.wrapping_add(index);
                    for (i, p) in [(i1, p1), (i2, p2), (i3, p3), (i4, p4)] {
                        match usize::try_from(i).ok().and_then(|i| points.get_mut(i)) {
                            Some(q) => *q = p,
                            None => dropped += 1,
                        }
                        mesh.point(i as i64, p);
                    }
                    mesh.triangle(polygon as i64, i1 as i64, i2 as i64, i4 as i64);
                    mesh.triangle(next_polygon as i64, i2 as i64, i3 as i64, i4 as i64);
                    mesh.coord(i2 as i64, col_t * 0.5 + tex_u, row_t * 0.5 + tex_v);
                    mesh.coord(i3 as i64, c as f32 * SIXTH + tex_u, row_t * 0.5 + tex_v);
                    mesh.coord(i1 as i64, col_t * 0.5 + tex_u, next_v);
                    mesh.coord(i4 as i64, c as f32 * SIXTH + tex_u, next_v);
                    let o1 = clamp01(4.0 - center.distance_to(p1) * fade);
                    let o2 = clamp01(4.0 - center.distance_to(p2) * fade);
                    let o3 = clamp01(4.0 - center.distance_to(p3) * fade);
                    let o4 = clamp01(4.0 - center.distance_to(p4) * fade);
                    mesh.opacity(i1 as i64, o1);
                    mesh.opacity(i2 as i64, o2);
                    mesh.opacity(i3 as i64, o3);
                    mesh.opacity(i4 as i64, o4);
                    center.z = p1.z;
                    let b1 = clamp01(4.0 - center.distance_to(p1) * fade2 - 1.0);
                    center.z = p2.z;
                    let b2 = clamp01(4.0 - center.distance_to(p2) * fade2 - 1.0);
                    center.z = p3.z;
                    let b3 = clamp01(4.0 - center.distance_to(p3) * fade2 - 1.0);
                    center.z = p4.z;
                    let b4 = clamp01(4.0 - center.distance_to(p4) * fade2 - 1.0);
                    let k0 = blended(b1, color_at(colors, color_next, &mut dropped), cell_colors[prev][next]);
                    let k1 = blended(b2, color_at(colors, color_index, &mut dropped), cell_colors[prev][r]);
                    color_index = color_index.wrapping_add(1);
                    let k2 = blended(b3, color_at(colors, color_index, &mut dropped), cell_colors[c][r]);
                    color_next = color_next.wrapping_add(1);
                    let k3 = blended(b4, color_at(colors, color_next, &mut dropped), cell_colors[c][next]);
                    mesh.color(i1 as i64, k0);
                    mesh.color(i2 as i64, k1);
                    mesh.color(i3 as i64, k2);
                    mesh.color(i4 as i64, k3);
                    polygon = polygon.wrapping_add(2);
                    next_polygon = next_polygon.wrapping_add(2);
                    base = base.wrapping_add(1);
                }
                color_row = color_row.wrapping_add(side);
                base = base.wrapping_sub(3).wrapping_add(side);
            }
            vertex = vertex.wrapping_add(3);
            tex_u += 0.5;
            cell_base = cell_base.wrapping_add(3);
            x = x.wrapping_add(1);
        }
        tex_v += 0.5;
        y = y.wrapping_sub(1);
        vertex = vertex.wrapping_add(1).wrapping_add(side.wrapping_mul(2));
        row_base = row_base.wrapping_add(row_step);
    }
    let half = (side as f32 - 1.0) * 0.5;
    let middle = Ge2DPoint { x: half, y: half };
    let scale = 1.0 / middle.distance_to_squared(Ge2DPoint { x: half, y: 0.0 });
    for i in 0..side_u {
        for j in 0..side_u {
            let index = i + j * side_u;
            if index >= vertices {
                dropped += 1;
                continue;
            }
            let mut blend = middle.distance_to_squared(Ge2DPoint { x: j as f32, y: i as f32 }) * scale;
            if blend > 1.0 {
                blend = 1.0;
            }
            heights[index] = blend * points[index].z + (1.0 - blend) * (maximum + heights[index]);
        }
    }
    EeGfxVolcanoBuilt { maximum, center_z: center.z, dropped: dropped + mesh.dropped }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::mesh::{EeGfxMeshTriangle, EeGfxMeshVertex, EE_GFX_MESH_COLOR, EE_GFX_MESH_POINT};

    struct Slope;
    impl VolcanoTerrain for Slope {
        fn z(&mut self, x: u32, y: u32) -> f32 {
            x as f32 * 0.25 + y as f32 * 0.125
        }
        fn color(&mut self, _: u32, _: u32) -> EeGfxColor {
            EeGfxColor { red: 0.5, green: 0.25, blue: 1.0 }
        }
        fn valid_tile(&mut self, x: u32, _: u32) -> bool {
            x.is_multiple_of(2)
        }
        fn light(&mut self, _: i32, _: i32, corner: i32) -> EeGfxColor {
            EeGfxColor { red: 1.0, green: corner as f32 * 0.25, blue: 1.0 }
        }
    }

    #[test]
    fn a_six_tile_volcano_covers_its_grid() {
        let s = sizes(6.0);
        assert_eq!(s, EeGfxVolcanoSizes { side: 19, vertices: 361, polygons: 648 });
        let a = EeGfxVolcanoArea {
            left: 37.0,
            top: 53.0,
            right: 43.0,
            bottom: 47.0,
            center: Ge3DPoint { x: 40.0, y: 51.0, z: 0.0 },
        };
        let pixels: Vec<u32> = (0..19 * 20).map(|i| (i as u32).wrapping_mul(0x01030507)).collect();
        let mut points = vec![Ge3DPoint::default(); 361];
        let mut heights = vec![0.0; 361];
        let mut v = vec![EeGfxMeshVertex::default(); 361];
        let mut t = vec![EeGfxMeshTriangle::default(); 648];
        let mut m = Mesh::new(&mut v, &mut t);
        let mut colors = vec![EeGfxColor::default(); 361];
        assert_eq!(image(4.0, s.side, &pixels, 20, &mut colors, &mut heights), 0);
        assert_eq!(colors[0].red, 0.0);
        let r = build(&a, &colors, &mut Slope, &mut points, &mut heights, &mut m);
        assert_eq!(r.dropped, 0);
        assert!(v
            .iter()
            .all(|x| x.set & (EE_GFX_MESH_POINT | EE_GFX_MESH_COLOR) == EE_GFX_MESH_POINT | EE_GFX_MESH_COLOR));
        assert!(t.iter().all(|x| x.set != 0));
        // the highest corner: x 43, y 54
        assert_eq!(r.maximum, 43.0 * 0.25 + 54.0 * 0.125);
        assert_eq!(r.center_z, points[t[647].v[2] as usize].z);
        // the corners of the grid are the terrain's (the blend is held at 1 there)
        assert_eq!(heights[0], points[0].z);
    }
}
