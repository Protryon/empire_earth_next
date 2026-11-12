//! The units' render object's fixed models (`EEROUnits::LoadMedia`, `src/Empire Earth/Graphics Thread/EEROUnits.cpp`):
//! the area-effect ring's 26 vertices and 10 triangles (a hexagon's six spokes turned by -2 pi / 14 from the
//! centre, the texture over the unit square) and the three selection rings (a strip of `segments` quads, the
//! segment count and the colour pattern by the model detail). The C++ keeps the models: it creates them, stores
//! these values with the vertex and polygon arrays' setters, sets the materials and textures and computes the
//! normals and bounds.
//!
//! This is the renderer's, not the simulation's: nothing here is simulation state.

use ee_ge::types::{Ge3DPoint, GeTransformation};

use crate::effects::EeGfxTexCoord;

/// The ring's step (0x441be4, -2 pi / 14).
pub(crate) const RING_STEP: f32 = -0.44879895;
/// The selection rings' widths by size (0x4be748).
pub(crate) const SELECTION_WIDTHS: [f32; 3] = [0.08, 0.05, 0.025];
/// The most vertices (and triangles) a selection ring has: ten segments (model detail 2 and above).
pub const EE_GFX_SELECTION_MAX_VERTICES: usize = 20;
/// Three indices per triangle.
pub const EE_GFX_SELECTION_MAX_INDICES: usize = 60;

/// The area-effect ring: its points and texture coordinates by vertex and its triangles' vertex indices.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxRingModel {
    pub points: [Ge3DPoint; 26],
    pub coords: [EeGfxTexCoord; 26],
    pub indices: [u32; 30],
}

/// One selection ring: `count` vertices (twice the segments) and as many triangles; `colors` names each vertex's
/// colour (0: the light one, 0x5294a0; 1: the middle one, 0x529490; 2: `gSelectionRingDark`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxSelectionModel {
    pub count: u32,
    pub points: [Ge3DPoint; EE_GFX_SELECTION_MAX_VERTICES],
    pub coords: [EeGfxTexCoord; EE_GFX_SELECTION_MAX_VERTICES],
    pub indices: [u32; EE_GFX_SELECTION_MAX_INDICES],
    pub colors: [u32; EE_GFX_SELECTION_MAX_VERTICES],
}

/// The ring's triangles (LoadMedia's 30 `SetVertexIndex` calls).
const RING_INDICES: [u32; 30] =
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 25, 25, 22, 23, 25, 23, 24];
/// The ring's vertices at the centre, at +x and at -x.
const RING_ZERO: [usize; 10] = [0, 3, 6, 9, 12, 15, 18, 25, 22, 23];
/// The vertex pairs at each turn of the spoke, in the original's order.
const RING_TURNS: [(usize, usize); 6] = [(13, 20), (7, 14), (1, 8), (2, 4), (5, 10), (11, 16)];

/// `LoadMedia`'s area-effect ring.
pub fn area_effect_ring() -> EeGfxRingModel {
    let mut points = [Ge3DPoint::default(); 26];
    for i in RING_ZERO {
        points[i] = Ge3DPoint { x: 0.0, y: 0.0, z: 0.0 };
    }
    points[19] = Ge3DPoint { x: 0.5, y: 0.0, z: 0.0 };
    points[24] = Ge3DPoint { x: 0.5, y: 0.0, z: 0.0 };
    points[17] = Ge3DPoint { x: -0.5, y: 0.0, z: 0.0 };
    points[21] = Ge3DPoint { x: -0.5, y: 0.0, z: 0.0 };
    let start = Ge3DPoint { x: 0.5, y: 0.0, z: 0.0 };
    let mut t = GeTransformation::IDENTITY;
    for (a, b) in RING_TURNS {
        t.revolve(RING_STEP, 0.0, 0.0);
        let p = t.apply_point(start);
        points[a] = p;
        points[b] = p;
    }
    let mut coords = [EeGfxTexCoord::default(); 26];
    for (c, p) in coords.iter_mut().zip(points.iter()) {
        *c = EeGfxTexCoord { u: p.x + 0.5, v: -(p.y - 0.5) };
    }
    EeGfxRingModel { points, coords, indices: RING_INDICES }
}

/// The selection rings' segments by model detail (6 at full, 8 at half, else 10).
pub fn selection_segments(detail: i32) -> u32 {
    if detail == 0 {
        6
    } else if detail == 1 {
        8
    } else {
        10
    }
}

/// The selection rings' colour pattern by model detail (one entry per vertex).
fn selection_colors(detail: i32) -> &'static [u32] {
    match detail {
        0 => &[0, 0, 2, 2, 2, 2, 0, 0, 2, 2, 2, 2],
        1 => &[0, 0, 1, 1, 2, 2, 1, 1, 0, 0, 1, 1, 2, 2, 1, 1],
        _ => &[0, 0, 1, 1, 2, 2, 2, 2, 1, 1, 0, 0, 1, 1, 2, 2, 2, 2, 1, 1],
    }
}

/// `LoadMedia`'s selection ring of size `size` (0..2: its width) at model detail `detail`: the inner and outer
/// points turned by 2 pi / segments (the turns accumulate in one transformation), the texture's v 0 inside and 1
/// outside, two triangles per segment closing on the first pair.
pub fn selection_model(detail: i32, size: usize) -> EeGfxSelectionModel {
    let segments = selection_segments(detail);
    let angle = 6.2831855f32 / segments as f32;
    let count = segments * 2;
    let mut m = EeGfxSelectionModel {
        count,
        points: [Ge3DPoint::default(); EE_GFX_SELECTION_MAX_VERTICES],
        coords: [EeGfxTexCoord::default(); EE_GFX_SELECTION_MAX_VERTICES],
        indices: [0; EE_GFX_SELECTION_MAX_INDICES],
        colors: [0; EE_GFX_SELECTION_MAX_VERTICES],
    };
    let inner = Ge3DPoint { x: 0.0, y: 0.5, z: 0.0 };
    let outer = Ge3DPoint { x: 0.0, y: 0.5 - SELECTION_WIDTHS[size], z: 0.0 };
    m.points[0] = inner;
    m.points[1] = outer;
    let mut t = GeTransformation::IDENTITY;
    for i in 1..segments as usize {
        t.revolve(angle, 0.0, 0.0);
        m.points[2 * i] = t.apply_point(inner);
        m.points[2 * i + 1] = t.apply_point(outer);
    }
    for (i, c) in m.coords.iter_mut().take(count as usize).enumerate() {
        *c = EeGfxTexCoord { u: 0.5, v: (i & 1) as f32 };
    }
    let mut polygon = 0usize;
    let mut vertex = 0u32;
    while (polygon as u32) < count - 2 {
        m.indices[polygon * 3..polygon * 3 + 3].copy_from_slice(&[vertex, vertex + 2, vertex + 1]);
        polygon += 1;
        m.indices[polygon * 3..polygon * 3 + 3].copy_from_slice(&[vertex + 1, vertex + 2, vertex + 3]);
        polygon += 1;
        vertex += 2;
    }
    m.indices[polygon * 3..polygon * 3 + 3].copy_from_slice(&[vertex, 0, vertex + 1]);
    polygon += 1;
    m.indices[polygon * 3..polygon * 3 + 3].copy_from_slice(&[vertex + 1, 0, 1]);
    let colors = selection_colors(detail);
    m.colors[..colors.len()].copy_from_slice(colors);
    m
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_ring_covers_every_vertex_in_the_unit_square() {
        let r = area_effect_ring();
        for (p, c) in r.points.iter().zip(r.coords.iter()) {
            assert!(p.x.abs() <= 0.5 + 1e-6 && p.y.abs() <= 0.5 + 1e-6 && p.z == 0.0, "{p:?}");
            assert!((0.0..=1.0).contains(&c.u) && (0.0..=1.0).contains(&c.v), "{c:?}");
        }
        assert!(r.indices.iter().all(|&i| i < 26));
        // the last spoke is six steps (-6 pi / 7) round: below the x axis
        assert!(r.points[11].y < 0.0 && r.points[11] == r.points[16]);
    }

    #[test]
    fn the_selection_rings_close_their_strip() {
        for detail in [0, 1, 2, 5] {
            for size in 0..3 {
                let m = selection_model(detail, size);
                let n = m.count as usize;
                assert_eq!(n, 2 * selection_segments(detail) as usize);
                assert_eq!(selection_colors(detail).len(), n);
                assert!(m.indices[..n * 3].iter().all(|&i| (i as usize) < n));
                assert_eq!(&m.indices[n * 3 - 6..n * 3], &[n as u32 - 2, 0, n as u32 - 1, n as u32 - 1, 0, 1]);
                for i in 0..n / 2 {
                    let r = (m.points[2 * i].x.powi(2) + m.points[2 * i].y.powi(2)).sqrt();
                    assert!((r - 0.5).abs() < 1e-5, "{r}");
                }
            }
        }
    }
}
