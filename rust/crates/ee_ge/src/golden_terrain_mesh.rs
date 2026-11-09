//! `GETerrainMesh`'s vertex normals, shorelines, fog of war and file codec against golden values from the real C++
//! (`golden_terrain_mesh.txt`). A scratch driver compiled the original function text of `ComputeVertexNormal`,
//! `ComputeShorelines`, `UpdateFogOfWar`, `Save` and `Load` alone, over meshes built by `CreateFlatMesh` with heights,
//! colours, tiles and visibility drawn from an LCG seeded 11, and an `FSMemoryFile`-like file (a read with too few
//! bytes left reads nothing). `GEVector::GetQuantizedVersion` was replaced by a copy, so the normals are the
//! unquantized vectors. Each section prints its case count, an FNV-1a digest of the outputs and its first four cases
//! (more than 24 outputs print as their count). This test draws the same stream and calls the same Rust the
//! forwarders call; the driver relinked over the forwarders printed the same file.

// `c_ulong` is `u32` here but `u64` on LP64: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_long, c_ulong};

use crate::io::{BufferIo, FieldIo};
use crate::terrain::{edge_planes, GeTerrainView, GeTmPoint, GeTmRow};
use crate::terrain_mesh::{
    fog_of_war, load_body, load_size, save, vertex_normal, GeTerrainFog, GeTerrainLoad, GeTmTile,
};
use crate::types::{Ge3DPlane, GeVector};

const GOLDEN: &str = include_str!("golden_terrain_mesh.txt");
const SENTINEL: f32 = -12345.5;
const SIDE: usize = 257;

struct Rng(u32);

impl Rng {
    fn next(&mut self) -> u32 {
        self.0 = self.0.wrapping_mul(1664525).wrapping_add(1013904223);
        self.0
    }

    fn r(&mut self, lo: f32, hi: f32) -> f32 {
        let unit = (self.next() >> 8) as f32 * (1.0 / 16777216.0);
        lo + unit * (hi - lo)
    }
}

struct Section {
    name: &'static str,
    count: usize,
    hash: u64,
    inputs: Vec<u32>,
    outputs: Vec<u32>,
    lines: String,
}

impl Section {
    fn new(name: &'static str) -> Self {
        Section { name, count: 0, hash: 0xcbf2_9ce4_8422_2325, inputs: vec![], outputs: vec![], lines: String::new() }
    }

    fn iu(&mut self, v: u32) {
        self.inputs.push(v);
    }

    fn o(&mut self, v: f32) {
        self.outputs.push(v.to_bits());
    }

    fn ou(&mut self, v: u32) {
        self.outputs.push(v);
    }

    fn end(&mut self) {
        for v in &self.outputs {
            for k in 0..4 {
                self.hash ^= u64::from((v >> (8 * k)) & 0xff);
                self.hash = self.hash.wrapping_mul(0x0000_0100_0000_01b3);
            }
        }
        if self.count < 4 {
            self.lines.push_str(self.name);
            for v in &self.inputs {
                self.lines.push_str(&format!(" {v:08x}"));
            }
            self.lines.push_str(" ->");
            if self.outputs.len() <= 24 {
                for v in &self.outputs {
                    self.lines.push_str(&format!(" {v:08x}"));
                }
            } else {
                self.lines.push_str(&format!(" ({})", self.outputs.len()));
            }
            self.lines.push('\n');
        }
        self.count += 1;
        self.inputs.clear();
        self.outputs.clear();
    }

    fn text(&self) -> String {
        format!("# {} {} {:016x}\n{}", self.name, self.count, self.hash, self.lines)
    }
}

/// A mesh as `CreateFlatMesh` leaves it: the diamond's rows, every height 0, every tile empty; plus the members the
/// functions read and write.
struct Mesh {
    points: Vec<Vec<GeTmPoint>>,
    tiles: Vec<Vec<GeTmTile>>,
    point_rows: Vec<GeTmRow>,
    tile_rows: Vec<GeTmRow>,
    planes: [Ge3DPlane; 4],
    normals: Vec<Vec<GeVector>>,
    width: c_long,
    height: c_long,
    space_tile_count: c_ulong,
    minimum_z: f32,
    maximum_z: f32,
    fractal_seed: c_ulong,
}

/// `CreateFlatMesh`'s row spans: the start falls to 0 (held `hold` rows) then rises, the end rises to the last row
/// (held as long) then falls.
fn spans(count: c_ulong, start: c_ulong, end: c_ulong, hold: u32) -> Vec<(c_ulong, c_ulong)> {
    let (mut start, mut end, mut falling, mut rising, mut bottom, mut top) = (start, end, true, true, 0, 0);
    let mut out = Vec::new();
    for _ in 0..count {
        out.push((start, end - start + 1));
        if falling {
            if start != 0 {
                start -= 1;
            } else {
                bottom += 1;
                falling = bottom != hold;
            }
        } else {
            start += 1;
        }
        if rising {
            if end != count - 1 {
                end += 1;
            } else {
                top += 1;
                rising = top != hold;
            }
        } else {
            end -= 1;
        }
    }
    out
}

impl Mesh {
    fn flat(width: c_long, height: c_long) -> Mesh {
        let (w, h) = (width as c_ulong, height as c_ulong);
        let tile_spans = spans(w + h, h - 1, h, 1);
        let point_spans = spans(w + h + 1, h - 1, h + 1, 2);
        let tiles: Vec<Vec<GeTmTile>> =
            tile_spans.iter().map(|&(_, n)| vec![GeTmTile::default(); n as usize]).collect();
        let points: Vec<Vec<GeTmPoint>> = point_spans
            .iter()
            .map(|&(_, n)| vec![GeTmPoint { z: 0.0, normal: std::ptr::null(), color_table_index: 0 }; n as usize])
            .collect();
        let tile_rows: Vec<GeTmRow> = tile_spans
            .iter()
            .zip(&tiles)
            .map(|(&(start, count), t)| GeTmRow { start, count, items: t.as_ptr() as *const u8 })
            .collect();
        let point_rows = point_spans
            .iter()
            .zip(&points)
            .map(|(&(start, count), p)| GeTmRow { start, count, items: p.as_ptr() as *const u8 })
            .collect();
        // SAFETY: at least one tile row
        let planes = unsafe { edge_planes(tile_rows.as_ptr(), tile_rows.len() as c_ulong) };
        let normals = point_spans.iter().map(|&(_, n)| vec![GeVector::new(0.0, 0.0, 1.0); n as usize]).collect();
        Mesh {
            points,
            tiles,
            point_rows,
            tile_rows,
            planes,
            normals,
            width,
            height,
            space_tile_count: 0,
            minimum_z: 0.0,
            maximum_z: 0.0,
            fractal_seed: 0x1234,
        }
    }

    fn view(&self) -> GeTerrainView {
        GeTerrainView {
            point_rows: self.point_rows.as_ptr(),
            tile_rows: self.tile_rows.as_ptr(),
            tile_row_count: self.tile_rows.len() as c_ulong,
            planes: self.planes.as_ptr(),
        }
    }

    /// `ComputeVertexNormal` (the driver's copy in place of the quantization).
    fn compute_normal(&mut self, j: usize, i: usize) {
        let x = self.point_rows[j].start + i as c_ulong;
        // SAFETY: a point of the mesh
        self.normals[j][i] =
            unsafe { vertex_normal(self.point_rows.as_ptr(), self.point_rows.len() as c_ulong, x, j as c_ulong) };
    }

    /// A tile, through its row pointer as the Rust under test writes it.
    fn tile(&mut self, j: usize, i: usize) -> &mut GeTmTile {
        // SAFETY: a tile of the mesh
        unsafe { &mut *(self.tile_rows[j].items as *mut GeTmTile).add(i) }
    }

    fn out(&self, s: &mut Section) {
        s.ou(self.width as u32);
        s.ou(self.height as u32);
        s.ou(self.space_tile_count as u32);
        s.o(self.minimum_z);
        s.o(self.maximum_z);
        s.ou(self.fractal_seed as u32);
        for r in &self.tile_rows {
            for i in 0..r.count as usize {
                // SAFETY: a tile of the row
                let t = unsafe { *(r.items as *const GeTmTile).add(i) };
                s.ou(u32::from(t.texture_set_id) | u32::from(t.water_info) << 8);
            }
        }
        for (j, r) in self.point_rows.iter().enumerate() {
            for i in 0..r.count as usize {
                // SAFETY: a point of the row
                let p = unsafe { *(r.items as *const GeTmPoint).add(i) };
                s.o(p.z);
                s.ou(u32::from(p.color_table_index));
                let n = self.normals[j][i];
                s.o(n.x);
                s.o(n.y);
                s.o(n.z);
            }
        }
    }
}

/// The driver's `Fill`: heights and colours by point row, then the tiles.
fn fill(r: &mut Rng, width: c_long, height: c_long, space: bool) -> Mesh {
    let mut m = Mesh::flat(width, height);
    for j in 0..m.points.len() {
        for i in 0..m.points[j].len() {
            let k = r.next();
            let z = if k & 3 == 0 { 0.0 } else { r.r(-5.0, 30.0) };
            let c = r.next() as u8;
            // SAFETY: a point of the row
            let p = unsafe { &mut *(m.point_rows[j].items as *mut GeTmPoint).add(i) };
            p.z = z;
            p.color_table_index = c;
        }
    }
    for j in 0..m.tiles.len() {
        for i in 0..m.tiles[j].len() {
            let k = r.next();
            let w = (k >> 9) & 7;
            let water = match w {
                0 if space => 0x80,
                1..=3 => 1,
                _ => 0,
            };
            let t = m.tile(j, i);
            t.reference_count = ((k >> 2) % 4) as u16;
            t.texture_set_id = ((k >> 5) % 7) as u8;
            t.water_info = water;
            if water & 0x80 != 0 {
                m.space_tile_count += 1;
            }
        }
    }
    m
}

/// `GETerrainMesh::Load` as the forwarder composes it, into a fresh mesh with texture sets 0..4 registered.
fn load(io: &mut BufferIo) -> (c_long, Option<Mesh>, c_long, c_long) {
    let (mut width, mut height) = (77, 66);
    let result = load_size(io, &mut width, &mut height);
    if result != 0 {
        return (result, None, width, height);
    }
    let mut m = Mesh::flat(width, height);
    let mut valid = [0u8; 256];
    valid[..5].fill(1);
    let mut state = GeTerrainLoad {
        space_tile_count: 0,
        minimum_z: 0.0,
        maximum_z: 0.0,
        fractal_seed: m.fractal_seed,
        compute_fractal: false,
    };
    // SAFETY: the mesh's rows, writable
    let result = unsafe { load_body(io, &m.view(), m.point_rows.len() as c_ulong, &valid, &mut state) };
    m.space_tile_count = state.space_tile_count;
    m.minimum_z = state.minimum_z;
    m.maximum_z = state.maximum_z;
    m.fractal_seed = state.fractal_seed;
    for j in 0..m.points.len() {
        for i in 0..m.points[j].len() {
            m.compute_normal(j, i);
        }
    }
    (result, Some(m), width, height)
}

fn run_all() -> Vec<Section> {
    let mut r = Rng(11);
    let sizes: [(c_long, c_long); 6] = [(3, 2), (8, 5), (20, 20), (41, 13), (1, 1), (12, 30)];
    let mut normal = Section::new("tmm_normal");
    let mut shore = Section::new("tmm_shore");
    let mut fog = Section::new("tmm_fog");
    let mut saves = Section::new("tmm_save");
    let mut loads = Section::new("tmm_load");
    let mut terrain_visible = vec![0u8; SIDE * SIDE];
    let mut water_visible = vec![0u8; SIDE * SIDE];
    let mut terrain_vis = vec![0.0f32; SIDE * SIDE];
    let mut water_vis = vec![0.0f32; SIDE * SIDE];
    for (s, &(w, h)) in sizes.iter().enumerate() {
        let mut m = fill(&mut r, w, h, s & 1 == 0);
        for j in 0..m.points.len() {
            for i in 0..m.points[j].len() {
                normal.iu((m.point_rows[j].start + i as c_ulong) as u32);
                normal.iu(j as u32);
                m.compute_normal(j, i);
                let n = m.normals[j][i];
                normal.o(n.x);
                normal.o(n.y);
                normal.o(n.z);
                normal.end();
            }
        }
        let space_map = m.space_tile_count > 0;
        for j in 0..m.tiles.len() {
            for i in 0..m.tiles[j].len() {
                let x = m.tile_rows[j].start + i as c_ulong;
                shore.iu(x as u32);
                shore.iu(j as u32);
                // SAFETY: a tile of the mesh
                let v = unsafe { m.view().shoreline(space_map, x, j as c_ulong) };
                let t = m.tile(j, i);
                if let Some(v) = v {
                    t.water_info = (t.water_info & 0x80) | v; // SetWaterInfo
                }
                let info = t.water_info;
                shore.ou(u32::from(info));
                shore.end();
            }
        }
        for trial in 0..8 {
            let span = (w + h) as u32;
            let blx = r.next() % (span + 2);
            let bly = r.next() % (span + 2);
            let tw = r.next() % 40;
            let th = r.next() % 40;
            let tiles = if r.next() & 7 == 0 { 0 } else { r.next() % 300 };
            for i in 0..(tw + 2) as usize {
                for j in 0..(th + 2) as usize {
                    terrain_visible[i * SIDE + j] = u8::from(r.next() & 3 != 0);
                    water_visible[i * SIDE + j] = u8::from(r.next() & 1 != 0);
                    terrain_vis[i * SIDE + j] = SENTINEL;
                    water_vis[i * SIDE + j] = SENTINEL;
                }
            }
            for v in [blx, bly, tw, th, tiles] {
                fog.iu(v);
            }
            let f = GeTerrainFog {
                visible_row_count: usize::from(trial != 5),
                bottom_left_x: blx as c_ulong,
                bottom_left_y: bly as c_ulong,
                tile_width: tw as c_ulong,
                tile_height: th as c_ulong,
                num_visible_tiles: tiles as c_ulong,
                side: SIDE,
                terrain_point_visible: terrain_visible.as_ptr(),
                water_point_visible: water_visible.as_ptr(),
                terrain_visibility: terrain_vis.as_mut_ptr(),
                water_visibility: water_vis.as_mut_ptr(),
                vertex_stride: 4,
                all_visible: trial == 4,
            };
            // SAFETY: the arrays cover the window
            let shares = unsafe { fog_of_war(&m.view(), &f) }.unwrap_or((SENTINEL, SENTINEL));
            for i in 0..(tw + 2) as usize {
                for j in 0..(th + 2) as usize {
                    fog.o(terrain_vis[i * SIDE + j]);
                    fog.o(water_vis[i * SIDE + j]);
                }
            }
            fog.o(shares.0);
            fog.o(shares.1);
            fog.end();
        }
        m.fractal_seed = r.next() as c_ulong;
        let mut whole = BufferIo::writer();
        saves.iu(0xffff_ffff);
        // SAFETY: the mesh's rows
        let result = unsafe { save(&mut whole, &m.view(), m.point_rows.len() as c_ulong, w, h, m.fractal_seed) };
        saves.ou(result as u32);
        saves.ou(whole.bytes.len() as u32);
        for &b in &whole.bytes {
            saves.ou(u32::from(b));
        }
        saves.end();
        let length = whole.bytes.len();
        for limit in [0, 3, 7, 9, length / 2, length - 5, length - 1] {
            let mut f = BufferIo { write_limit: limit, ..BufferIo::writer() };
            saves.iu(limit as u32);
            // SAFETY: as above
            let result = unsafe { save(&mut f, &m.view(), m.point_rows.len() as c_ulong, w, h, m.fractal_seed) };
            saves.ou(result as u32);
            saves.ou(f.bytes.len() as u32);
            saves.end();
        }
        for cut in [0, 2, 6, 8, 9, 11, length / 2, length - 4, length - 2, length] {
            let mut f = BufferIo::reader(whole.bytes[..cut].to_vec());
            loads.iu(cut as u32);
            let (result, mesh, width, height) = load(&mut f);
            loads.ou(result as u32);
            loads.ou(f.position as u32);
            match mesh {
                Some(b) => b.out(&mut loads),
                None => {
                    loads.ou(width as u32);
                    loads.ou(height as u32);
                }
            }
            loads.end();
        }
    }
    vec![normal, shore, fog, saves, loads]
}

#[test]
fn every_section_matches_the_cpp() {
    let ours: String = run_all().iter().map(Section::text).collect();
    let theirs = GOLDEN.replace("\r\n", "\n");
    for (a, b) in ours.lines().zip(theirs.lines()) {
        assert_eq!(a, b);
    }
    assert_eq!(ours, theirs);
}

#[test]
fn the_io_trait_reads_what_it_wrote() {
    let mut io = BufferIo::writer();
    assert_eq!(io.write_i32(-2), 0);
    let mut back = BufferIo::reader(io.bytes);
    let mut v = 0;
    assert_eq!(back.read_i32(&mut v), 0);
    assert_eq!(v, -2);
}
