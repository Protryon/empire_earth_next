//! `GETerrainMesh`'s queries against golden values from the real C++ (`golden_terrain.txt`): a scratch driver ran the
//! original query functions over diamond meshes built as `CreateFlatMesh` builds them, with heights and normals drawn
//! from an LCG seeded 7 (the same `Section` format and digest as `golden.rs`). This test builds the same meshes from
//! the same stream and calls the C ABI.

// `c_ulong` is `u32` here but `u64` on LP64: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_long, c_ulong};

use crate::ffi::*;
use crate::golden::{Section, SENTINEL};
use crate::terrain::{GeTerrainView, GeTmPoint, GeTmRow};
use crate::types::*;

const GOLDEN: &str = include_str!("golden_terrain.txt");

struct Rng(u32);

impl Rng {
    fn next(&mut self) -> u32 {
        self.0 = self.0.wrapping_mul(1664525).wrapping_add(1013904223);
        self.0
    }

    fn unit(&mut self) -> f32 {
        (self.next() >> 8) as f32 * (1.0 / 16777216.0)
    }

    fn r(&mut self, lo: f32, hi: f32) -> f32 {
        let unit = self.unit();
        lo + unit * (hi - lo)
    }
}

struct Mesh {
    _points: Vec<Vec<GeTmPoint>>,
    point_rows: Vec<GeTmRow>,
    tile_rows: Vec<GeTmRow>,
    planes: [Ge3DPlane; 4],
}

impl Mesh {
    fn view(&self) -> GeTerrainView {
        GeTerrainView {
            point_rows: self.point_rows.as_ptr(),
            tile_rows: self.tile_rows.as_ptr(),
            tile_row_count: self.tile_rows.len() as c_ulong,
            planes: self.planes.as_ptr(),
        }
    }
}

/// One edge of the diamond moving: down to 0 (then held for `hold` rows), then up again; the other up to the last
/// index (held as long), then down.
struct Edges {
    start: c_ulong,
    end: c_ulong,
    falling: bool,
    rising: bool,
    bottom: u32,
    top: u32,
}

impl Edges {
    fn step(&mut self, last: c_ulong, hold: u32) {
        if self.falling {
            if self.start != 0 {
                self.start -= 1;
            } else {
                self.bottom += 1;
                if self.bottom == hold {
                    self.falling = false;
                }
            }
        } else {
            self.start += 1;
        }
        if self.rising {
            if self.end != last {
                self.end += 1;
            } else {
                self.top += 1;
                if self.top == hold {
                    self.rising = false;
                }
            }
        } else {
            self.end -= 1;
        }
    }
}

/// `CreateFlatMesh`'s rows (the tile rows turn at once, the point rows after two rows at the edge), as the driver
/// builds them, with the heights and normals drawn.
fn build(r: &mut Rng, width: c_ulong, height: c_ulong, normals: &[GeVector]) -> Mesh {
    let tile_row_count = width + height;
    let mut e = Edges { start: height - 1, end: height, falling: true, rising: true, bottom: 0, top: 0 };
    let mut tile_rows = Vec::new();
    for _ in 0..tile_row_count {
        tile_rows.push(GeTmRow { start: e.start, count: e.end - e.start + 1, items: std::ptr::null() });
        e.step(tile_row_count - 1, 1);
    }
    let point_row_count = width + height + 1;
    let mut e = Edges { start: height - 1, end: height + 1, falling: true, rising: true, bottom: 0, top: 0 };
    let mut points = Vec::new();
    let mut ranges = Vec::new();
    for _ in 0..point_row_count {
        let count = e.end - e.start + 1;
        let mut row = Vec::new();
        for _ in 0..count {
            let k = r.next();
            let z = if k & 3 == 0 { 0.0 } else { r.r(-5.0, 30.0) };
            let normal = &normals[(r.next() as usize) % normals.len()] as *const GeVector;
            row.push(GeTmPoint { z, normal, color_table_index: 0 });
        }
        points.push(row);
        ranges.push((e.start, count));
        e.step(point_row_count - 1, 2);
    }
    let point_rows = points
        .iter()
        .zip(&ranges)
        .map(|(row, &(start, count))| GeTmRow { start, count, items: row.as_ptr() as *const u8 })
        .collect();
    let mut planes = [Ge3DPlane::default(); 4];
    // SAFETY: the tile rows and room for four planes
    unsafe { ee_ge_terrain_planes(tile_rows.as_ptr(), tile_row_count, planes.as_mut_ptr()) };
    Mesh { _points: points, point_rows, tile_rows, planes }
}

#[test]
fn terrain_queries_match_the_cpp() {
    let mut r = Rng(7);
    let mut normals = Vec::new();
    for _ in 0..64 {
        let x = r.r(-1.0, 1.0);
        let y = r.r(-1.0, 1.0);
        let z = r.r(0.0, 1.0);
        normals.push(GeVector::new(x, y, z));
    }
    let mut planes = Section::new("tm_planes").showing(4);
    let mut z = Section::new("tm_z").showing(4);
    let mut closest = Section::new("tm_closest").showing(4);
    let mut lines = Section::new("tm_line").showing(4);
    let mut hits = 0;
    for (width, height) in [(3, 2), (8, 5), (20, 20), (41, 13)] {
        let m = build(&mut r, width, height, &normals);
        let v = m.view();
        planes.iu(width as u32);
        planes.iu(height as u32);
        for p in m.planes {
            planes.op(p.point);
            planes.ov(p.normal);
        }
        planes.end();
        let rows = m.tile_rows.len() as u32;
        let span = (width + height) as f32;
        for _ in 0..4000 {
            let y = r.next() % rows;
            let row = m.tile_rows[y as usize];
            let x = row.start as u32 + r.next() % row.count as u32;
            let k = r.next() & 7;
            let fx = if k == 0 { 0.0 } else { r.unit() };
            let fy = if k == 1 {
                0.0
            } else if k == 2 {
                fx
            } else {
                r.unit()
            };
            let px = x as f32 + fx;
            let py = y as f32 + fy;
            z.i(px);
            z.i(py);
            let mut n = GeVector::new(SENTINEL, SENTINEL, SENTINEL);
            // SAFETY: a valid tile of the mesh
            unsafe {
                z.o(ee_ge_terrain_z(&v, px, py));
                ee_ge_terrain_normal(&v, px, py, &mut n);
            }
            z.ov(n);
            z.end();
        }
        for _ in 0..4000 {
            let x = r.r(-10.0, span + 10.0);
            let y = r.r(-10.0, span + 10.0);
            let h = r.r(-5.0, 50.0);
            closest.i(x);
            closest.i(y);
            closest.i(h);
            let (mut ox, mut oy): (c_long, c_long) = (-77, -77);
            // SAFETY: valid pointers
            unsafe { ee_ge_terrain_closest_tile_point(&v, &Ge3DPoint::new(x, y, h), &mut ox, &mut oy) };
            closest.ou(ox as u32);
            closest.ou(oy as u32);
            closest.end();
        }
        for _ in 0..3000 {
            let sx = r.r(-5.0, span + 5.0);
            let sy = r.r(-5.0, span + 5.0);
            let sz = r.r(10.0, 120.0);
            let mut dx = r.r(-30.0, 30.0);
            let dy = r.r(-30.0, 30.0);
            let dz = r.r(-150.0, -5.0);
            if r.next() & 7 == 0 {
                dx = 0.0;
            }
            let l = Ge3DLine { start: Ge3DPoint::new(sx, sy, sz), direction: GeVector::new(dx, dy, dz) };
            for e in [sx, sy, sz, dx, dy, dz] {
                lines.i(e);
            }
            let (mut ox, mut oy): (c_long, c_long) = (-77, -77);
            let mut d = SENTINEL;
            // SAFETY: valid pointers
            let hit = unsafe {
                ee_ge_terrain_closest_tile_line(&v, &l, &mut ox, &mut oy);
                ee_ge_terrain_intersects(&v, &l, &mut d)
            };
            hits += hit as u32;
            lines.ou(ox as u32);
            lines.ou(oy as u32);
            lines.ou(hit as u32);
            lines.o(d);
            lines.end();
        }
    }
    // the lines hit the terrain often enough to exercise the walk and the triangle test
    assert!(hits > 1000, "{hits} hits");
    let text: String = [planes, z, closest, lines].iter().map(|s| s.text()).collect();
    if text != GOLDEN {
        let first = text.lines().zip(GOLDEN.lines()).find(|(a, b)| a != b);
        panic!("terrain sections differ; first different line: {first:?}");
    }
}
