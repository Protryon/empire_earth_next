//! `UpdateVisibleTerrain`'s plan, `GEViewport`'s math and `GEPhysicalModel::Intersects(viewport)` against golden
//! values from the real C++ (`golden_visible.txt`). A scratch driver (`build/visible_golden`, PORT_LEARNINGS "R4.3,
//! fifth part") compiled the unmodified `GETerrainMesh.cpp` and `GEViewport.cpp` with the engine's math units over
//! fake textures (a pool of 48 reference-counted stand-ins, addresses in index order) and built 40 meshes from an LCG
//! seeded 17 (flat, hilly with water, space with shorelines, hilly with random texture sets; diamonds from 1 x 1 to
//! 96 x 96 tiles) with 24 cameras each (perspective and orthographic, near and far, aimed at a map tile), printing per
//! section its case count, an FNV-1a digest of every output and one line per case (every case for the visible
//! terrain; a case over the polygon arrays' capacity is `skip`). This test draws the same stream, builds the same
//! meshes and viewports through the Rust, and prints the same text; the driver relinked over the forwarders printed
//! the same file. The driver's `refs` section (the fake textures' reference counts after each call) is the C++
//! fill's and is not replayed here. Three sections after the meshes pin `GEBitmap`'s and `GERhombus`'s corners and
//! the stretched texture coordinates (`bitmap.rs`), from the driver's copies of those function bodies.

// `c_ulong` is `u32` here but `u64` on LP64: the casts are needed there
#![allow(clippy::unnecessary_cast, clippy::needless_range_loop)]

use std::ffi::{c_long, c_ulong};

use crate::bitmap::{bitmap_corners, rhombus_corners, stretched_coords};
use crate::physical::model_visible;
use crate::terrain::{edge_planes, GeTerrainView, GeTmPoint, GeTmRow};
use crate::terrain_mesh::{flat_mesh, water_texture_indices, GeTmTile};
use crate::terrain_visible::{
    update_visible_terrain, GeTextureSetView, GeVisibleInput, VisiblePlan, TERRAIN_POLYGON_CAPACITY,
    WATER_POLYGON_CAPACITY,
};
use crate::transform::RotationOrder;
use crate::types::{Ge2DPoint, Ge3DPlane, Ge3DPoint, GeBoundingBox, GeRectangle, GeTransformation, GeVector};
use crate::viewport::GeViewport;

const GOLDEN: &str = include_str!("golden_visible.txt");
const POOL: u32 = 48;
const SIDE: usize = 257;
const MESHES: u32 = 40;
const CAMS: u32 = 24;
const SIZES: [c_long; 13] = [1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96];

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
        let u = self.unit();
        lo + u * (hi - lo)
    }
}

struct Section {
    name: &'static str,
    count: usize,
    hash: u64,
    inputs: Vec<u32>,
    outputs: Vec<u32>,
    lines: String,
    showing: usize,
    per_case: bool,
}

impl Section {
    fn new(name: &'static str, per_case: bool) -> Self {
        Section {
            name,
            count: 0,
            hash: 0xcbf2_9ce4_8422_2325,
            inputs: vec![],
            outputs: vec![],
            lines: String::new(),
            showing: 4,
            per_case,
        }
    }

    fn iu(&mut self, v: u32) {
        self.inputs.push(v);
    }

    fn ou(&mut self, v: u32) {
        self.outputs.push(v);
    }

    fn o(&mut self, v: f32) {
        self.outputs.push(v.to_bits());
    }

    fn op(&mut self, p: Ge3DPoint) {
        self.o(p.x);
        self.o(p.y);
        self.o(p.z);
    }

    fn ov(&mut self, v: GeVector) {
        self.o(v.x);
        self.o(v.y);
        self.o(v.z);
    }

    fn fold(h: &mut u64, v: u32) {
        for k in 0..4 {
            *h ^= u64::from((v >> (8 * k)) & 0xff);
            *h = h.wrapping_mul(0x0000_0100_0000_01b3);
        }
    }

    fn end(&mut self, note: Option<&str>) {
        let mut case_hash = 0xcbf2_9ce4_8422_2325u64;
        for &v in &self.outputs {
            Self::fold(&mut self.hash, v);
            Self::fold(&mut case_hash, v);
        }
        if self.per_case || self.count < self.showing {
            self.lines.push_str(self.name);
            for v in &self.inputs {
                self.lines.push_str(&format!(" {v:08x}"));
            }
            self.lines.push_str(" ->");
            if let Some(n) = note {
                self.lines.push(' ');
                self.lines.push_str(n);
            }
            if note.is_some() && self.outputs.is_empty() {
            } else if self.outputs.len() <= 24 {
                for v in &self.outputs {
                    self.lines.push_str(&format!(" {v:08x}"));
                }
            } else {
                self.lines.push_str(&format!(" ({}) {case_hash:016x}", self.outputs.len()));
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

// ---- the driver's value noise
fn hash2(x: u32, y: u32, seed: u32) -> u32 {
    let mut h = x.wrapping_mul(0x9E37_79B1) ^ y.wrapping_add(seed).wrapping_mul(0x85EB_CA77);
    h ^= h >> 15;
    h = h.wrapping_mul(0xC2B2_AE3D);
    h ^= h >> 13;
    h
}

fn unit_of(h: u32) -> f32 {
    (h >> 8) as f32 * (1.0 / 16777216.0)
}

fn noise(x: c_long, y: c_long, cell: c_long, seed: u32) -> f32 {
    let cx = x / cell;
    let cy = y / cell;
    let fx = (x - cx * cell) as f32 / cell as f32;
    let fy = (y - cy * cell) as f32 / cell as f32;
    let v00 = unit_of(hash2(cx as u32, cy as u32, seed));
    let v10 = unit_of(hash2((cx + 1) as u32, cy as u32, seed));
    let v01 = unit_of(hash2(cx as u32, (cy + 1) as u32, seed));
    let v11 = unit_of(hash2((cx + 1) as u32, (cy + 1) as u32, seed));
    let a = v00 + (v10 - v00) * fx;
    let b = v01 + (v11 - v01) * fx;
    a + (b - a) * fy
}

/// A texture handle of pool index `k`: the fake textures sit in an array, so their addresses rise with the index.
fn handle(k: u32) -> usize {
    ((k + 1) * 16) as usize
}

fn id_of(h: usize) -> u32 {
    (h / 16) as u32
}

struct TextureSet {
    textures: Vec<usize>,
    width: c_ulong,
    height: c_ulong,
    coverage: c_ulong,
}

/// The mesh the driver builds, with the members the plan reads.
struct Mesh {
    points: Vec<Vec<GeTmPoint>>,
    tiles: Vec<Vec<GeTmTile>>,
    point_rows: Vec<GeTmRow>,
    tile_rows: Vec<GeTmRow>,
    planes: [Ge3DPlane; 4],
    edges: [c_long; 4],
    space_tile_count: c_ulong,
    sets: Vec<TextureSet>,
    cliffs: Vec<usize>,
    water_grid: Vec<c_ulong>,
    water_material_count: usize,
    /// `mTerrainPolygons`' texture coordinates, which a skirt polygon leaves as they were.
    uv_slots: Vec<[[f32; 2]; 3]>,
}

impl Mesh {
    fn flat(width: c_long, height: c_long) -> Mesh {
        let layout = flat_mesh(width, height);
        let tiles: Vec<Vec<GeTmTile>> =
            layout.tile_rows.iter().map(|&(s, e)| vec![GeTmTile::default(); (e - s + 1) as usize]).collect();
        let points: Vec<Vec<GeTmPoint>> = layout
            .point_rows
            .iter()
            .map(|&(s, e)| {
                vec![GeTmPoint { z: 0.0, normal: std::ptr::null(), color_table_index: 0 }; (e - s + 1) as usize]
            })
            .collect();
        let tile_rows: Vec<GeTmRow> = layout
            .tile_rows
            .iter()
            .zip(&tiles)
            .map(|(&(s, e), t)| GeTmRow { start: s, count: e - s + 1, items: t.as_ptr() as *const u8 })
            .collect();
        let point_rows = layout
            .point_rows
            .iter()
            .zip(&points)
            .map(|(&(s, e), p)| GeTmRow { start: s, count: e - s + 1, items: p.as_ptr() as *const u8 })
            .collect();
        // SAFETY: at least one tile row
        let planes = unsafe { edge_planes(tile_rows.as_ptr(), tile_rows.len() as c_ulong) };
        Mesh {
            points,
            tiles,
            point_rows,
            tile_rows,
            planes,
            edges: [layout.left_edge, layout.top_edge, layout.right_edge, layout.bottom_edge],
            space_tile_count: 0,
            sets: vec![],
            cliffs: vec![],
            water_grid: vec![],
            water_material_count: 0,
            uv_slots: vec![[[0.0; 2]; 3]; TERRAIN_POLYGON_CAPACITY],
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

    fn tile(&mut self, x: c_long, y: c_long) -> &mut GeTmTile {
        let start = self.tile_rows[y as usize].start;
        &mut self.tiles[y as usize][(x as c_ulong - start) as usize]
    }

    /// `SetIsWater(x, y, true)`.
    fn set_water(&mut self, x: c_long, y: c_long) {
        if self.space_tile_count == 0 {
            let t = self.tile(x, y);
            t.water_info &= 0x80;
            t.water_info |= 1;
        }
    }

    /// `SetIsSpace(x, y, true)`'s bit and count (the shorelines are recomputed by the driver's full pass).
    fn set_space(&mut self, x: c_long, y: c_long) {
        let t = self.tile(x, y);
        if t.water_info & 0x80 == 0 {
            t.water_info |= 0x80;
            self.space_tile_count += 1;
        }
    }

    /// `ComputeShorelines` over every tile, in row order.
    fn compute_shorelines(&mut self) {
        let view = self.view();
        let space_map = self.space_tile_count > 0;
        for y in 0..self.tile_rows.len() {
            let row = self.tile_rows[y];
            for i in 0..row.count as usize {
                let x = row.start + i as c_ulong;
                // SAFETY: a tile of the mesh
                if let Some(bits) = unsafe { view.shoreline(space_map, x, y as c_ulong) } {
                    let t = &mut self.tiles[y][i];
                    t.water_info = (t.water_info & 0x80) | bits;
                }
            }
        }
    }

    fn set_views(&self) -> Vec<GeTextureSetView> {
        self.sets
            .iter()
            .map(|s| GeTextureSetView {
                textures: s.textures.as_ptr(),
                width: s.width,
                height: s.height,
                coverage: s.coverage,
            })
            .collect()
    }
}

/// The driver's `BuildMesh`: the diamond, heights, texture sets, tiles, water or space, the cliff textures and the
/// water state. Returns the mesh and its kind.
fn build_mesh(r: &mut Rng) -> (Mesh, u32) {
    let w = SIZES[(r.next() % 13) as usize];
    let h = SIZES[(r.next() % 13) as usize];
    let kind = r.next() % 4;
    let mut m = Mesh::flat(w, h);
    if kind != 0 {
        let cell = (2 + r.next() % 6) as c_long;
        let amp = r.r(2.0, 25.0);
        let base = r.r(-6.0, 4.0);
        let seed = r.next();
        for y in 0..m.point_rows.len() {
            let row = m.point_rows[y];
            for i in 0..row.count as usize {
                let x = (row.start + i as c_ulong) as c_long;
                m.points[y][i].z = base + amp * noise(x, y as c_long, cell, seed);
            }
        }
    }
    let nsets = 1 + r.next() % 4;
    for _ in 0..nsets {
        let gw = 1 + r.next() % 4;
        let gh = 1 + r.next() % 4;
        let cov = 1 + r.next() % 6;
        let mut textures = vec![0usize; (gw * gh) as usize];
        for i in 0..gw {
            for j in 0..gh {
                textures[(i * gh + j) as usize] = handle(r.next() % POOL);
            }
        }
        m.sets.push(TextureSet { textures, width: gw as c_ulong, height: gh as c_ulong, coverage: cov as c_ulong });
    }
    {
        let gw = 1 + r.next() % 3;
        let gh = 1 + r.next() % 3;
        let cov = 1 + r.next() % 4;
        let mut textures = vec![0usize; (gw * gh) as usize];
        for i in 0..gw {
            for j in 0..gh {
                textures[(i * gh + j) as usize] = handle(r.next() % POOL);
            }
        }
        m.sets.push(TextureSet { textures, width: gw as c_ulong, height: gh as c_ulong, coverage: cov as c_ulong });
        m.water_material_count = (gw * gh) as usize;
        let _scale = r.r(0.5, 3.0);
        m.water_grid = water_texture_indices((gh * gw) as c_long).into_iter().flatten().collect();
    }
    let mode = r.next() % 3;
    let cell2 = (2 + r.next() % 5) as c_long;
    let seed2 = r.next();
    let ida = r.next() % nsets;
    let idb = r.next() % nsets;
    for y in 0..m.tile_rows.len() {
        let row = m.tile_rows[y];
        for i in 0..row.count as usize {
            let x = (row.start + i as c_ulong) as c_long;
            let id = if mode == 1 {
                r.next() % nsets
            } else if mode == 2 {
                if noise(x, y as c_long, cell2, seed2) > 0.5 {
                    ida
                } else {
                    idb
                }
            } else {
                0
            };
            m.tiles[y][i].texture_set_id = id as u8;
        }
    }
    if kind == 1 || kind == 2 {
        let fraction = r.r(0.1, 0.6);
        let cell3 = (2 + r.next() % 5) as c_long;
        let seed3 = r.next();
        for y in 0..m.tile_rows.len() {
            let row = m.tile_rows[y];
            for i in 0..row.count as usize {
                let x = (row.start + i as c_ulong) as c_long;
                if noise(x, y as c_long, cell3, seed3) < fraction {
                    if kind == 1 {
                        m.set_water(x, y as c_long);
                    } else {
                        m.set_space(x, y as c_long);
                    }
                }
            }
        }
        m.compute_shorelines();
    }
    for _ in 0..13 {
        let t = handle(r.next() % POOL);
        m.cliffs.push(t);
    }
    (m, kind)
}

fn rect(l: f32, t: f32, r: f32, b: f32) -> GeRectangle {
    GeRectangle { bottom: b, left: l, right: r, top: t }
}

/// The driver's `BuildViewport`: a camera aimed at a point of the mesh and its viewport; whether it is orthographic.
fn build_viewport(r: &mut Rng, cam: u32, m: &Mesh) -> (GeTransformation, GeViewport, bool) {
    let ty = r.next() % m.tile_rows.len() as u32;
    let row = m.tile_rows[ty as usize];
    let tx = row.start as u32 + r.next() % row.count as u32;
    let tz = r.r(-2.0, 12.0);
    let yaw = r.r(0.0, 6.2831853);
    let pitch = -r.r(0.06, 1.5);
    let is_far = cam % 7 == 6;
    let distance = if is_far { r.r(60.0, 260.0) } else { r.r(4.0, 90.0) };
    let ortho = cam % 5 == 4;
    let mut t = GeTransformation::IDENTITY;
    t.set_orientation(RotationOrder::Ypr, yaw, pitch, 0.0);
    let dir = t.apply_vector(GeVector::new(0.0, 1.0, 0.0));
    let px = tx as f32 - dir.x * distance;
    let py = ty as f32 - dir.y * distance;
    let pz = tz - dir.z * distance;
    t.m[0][3] = px;
    t.m[1][3] = py;
    t.m[2][3] = pz;
    let front = r.r(0.1, 3.0);
    let rear = if ortho {
        85.0
    } else if is_far {
        r.r(100.0, 300.0)
    } else {
        r.r(15.0, 120.0)
    };
    let fov = r.r(0.35, 1.3);
    let which = r.next() % 3;
    let view = match which {
        0 => rect(0.0, 0.0, 1024.0, 768.0),
        1 => rect(0.0, 0.0, 640.0, 480.0),
        _ => rect(128.0, 64.0, 896.0, 704.0),
    };
    let ow = (50 + r.next() % 850) as c_long;
    let oh = (50 + r.next() % 850) as c_long;
    let ortho_rect = rect((-ow / 2) as f32, (-oh / 2) as f32, (ow / 2) as f32, (oh / 2) as f32);
    (t, GeViewport::new(view, ortho_rect, t, front, rear, fov), ortho)
}

fn dump_viewport(s: &mut Section, v: &GeViewport) {
    // SAFETY: the viewport is 0x288 bytes of f32s
    let words = unsafe { std::slice::from_raw_parts(v as *const GeViewport as *const u32, 0x288 / 4) };
    for &w in words {
        s.ou(w);
    }
}

fn pack_bits(s: &mut Section, bits: &[u8]) {
    let mut word = 0u32;
    for (k, &b) in bits.iter().enumerate() {
        if b != 0 {
            word |= 1 << (k % 32);
        }
        if k % 32 == 31 || k + 1 == bits.len() {
            s.ou(word);
            word = 0;
        }
    }
}

/// The driver's `DumpVisible` over a plan (the C++ mesh after the call); `None` when a polygon array overflowed.
fn dump_visible(s: &mut Section, m: &mut Mesh, p: &VisiblePlan) -> Option<String> {
    // the C++ wrote the polygon slots it had, overflow or not
    for (k, poly) in p.terrain_polygons.iter().enumerate().take(TERRAIN_POLYGON_CAPACITY) {
        if poly.uv_set != 0 {
            m.uv_slots[k] = poly.uv;
        }
    }
    let (tp, wp) = (p.terrain_polygons.len(), p.water_polygons.len());
    if tp > TERRAIN_POLYGON_CAPACITY || wp > WATER_POLYGON_CAPACITY {
        return None;
    }
    s.ou((p.rows.len() / 3) as u32);
    for v in &p.rows {
        s.ou(*v as u32);
    }
    s.ou(p.bottom_left_x as u32);
    s.ou(p.bottom_left_y as u32);
    s.ou(p.tile_width as u32);
    s.ou(p.tile_height as u32);
    s.ou(p.num_visible_tiles as u32);
    s.ou(p.visible_poly_count as u32);
    pack_bits(s, &p.terrain_visible);
    pack_bits(s, &p.water_visible);
    s.ou(p.materials.len() as u32);
    for &h in &p.materials {
        s.ou(id_of(h));
    }
    s.ou(tp as u32);
    for (k, poly) in p.terrain_polygons.iter().enumerate() {
        for v in &poly.vertices {
            s.ou(v.kind);
            s.ou(v.a);
            s.ou(v.b);
        }
        for a in poly.alpha {
            s.o(a);
        }
        for uv in m.uv_slots[k] {
            s.o(uv[0]);
            s.o(uv[1]);
        }
    }
    s.ou(wp as u32);
    for poly in &p.water_polygons {
        for v in &poly.vertices {
            s.ou(v.kind);
            s.ou(v.a);
            s.ou(v.b);
        }
        for a in poly.alpha {
            s.o(a);
        }
        for uv in poly.uv {
            s.o(uv[0]);
            s.o(uv[1]);
        }
    }
    for material in 0..p.materials.len() as u32 {
        let list: Vec<u32> = p
            .terrain_polygons
            .iter()
            .enumerate()
            .filter(|(_, q)| q.material == material)
            .map(|(k, _)| k as u32)
            .collect();
        s.ou(list.len() as u32);
        for k in list {
            s.ou(k);
        }
    }
    s.ou(m.water_material_count as u32);
    for material in 0..m.water_material_count as u32 {
        let list: Vec<u32> = p
            .water_polygons
            .iter()
            .enumerate()
            .filter(|(_, q)| q.material == material)
            .map(|(k, _)| k as u32)
            .collect();
        s.ou(list.len() as u32);
        for k in list {
            s.ou(k);
        }
    }
    s.ou(p.edge_points.len() as u32);
    for &e in &p.edge_points {
        s.op(e);
    }
    s.ou(p.cliff_points.len() as u32);
    for (k, &c) in p.cliff_points.iter().enumerate() {
        s.op(c);
        s.ou(p.cliff_links[2 * k]);
        s.ou(p.cliff_links[2 * k + 1]);
    }
    Some(format!(
        "rows={} tiles={} mats={} tp={} wp={} edge={} cliff={}",
        p.rows.len() / 3,
        p.num_visible_tiles,
        p.materials.len(),
        tp,
        wp,
        p.edge_points.len(),
        p.cliff_points.len()
    ))
}

/// The golden file's sections by name.
fn golden_sections() -> Vec<(String, String)> {
    let mut out: Vec<(String, String)> = Vec::new();
    for line in GOLDEN.lines() {
        if let Some(rest) = line.strip_prefix("# ") {
            let name = rest.split(' ').next().unwrap().to_string();
            out.push((name, format!("{line}\n")));
        } else if let Some(last) = out.last_mut() {
            last.1.push_str(line);
            last.1.push('\n');
        }
    }
    out
}

fn check(section: &Section, golden: &[(String, String)]) {
    let text = section.text();
    let want =
        &golden.iter().find(|(n, _)| n == section.name).unwrap_or_else(|| panic!("no section {}", section.name)).1;
    if &text != want {
        let first = text.lines().zip(want.lines()).enumerate().find(|(_, (a, b))| a != b);
        panic!("section {} differs; first different line: {first:?}", section.name);
    }
}

#[test]
fn the_visible_terrain_the_viewport_and_the_model_test_match_the_cpp() {
    let golden = golden_sections();
    let mut r = Rng(17);
    let mut vp = Section::new("vp", false);
    let mut vp_points = Section::new("vp_points", false);
    let mut vp_set = Section::new("vp_set", false);
    let mut vis = Section::new("vis", true);
    let mut pm = Section::new("pm_vis", false);
    {
        let mut def = Section::new("vp_default", false);
        dump_viewport(&mut def, &GeViewport::default());
        def.end(None);
        check(&def, &golden);
    }
    let mut seen_cliffs = 0;
    for m in 0..MESHES {
        let (mut mesh, kind) = build_mesh(&mut r);
        for c in 0..CAMS {
            let (_camera, viewport, ortho) = build_viewport(&mut r, c, &mesh);
            vp.iu(m);
            vp.iu(c);
            dump_viewport(&mut vp, &viewport);
            vp.end(None);
            {
                vp_points.iu(m);
                vp_points.iu(c);
                vp_points.iu(ortho as u32);
                for p in viewport.frustum_world_points(&viewport.view_rectangle, ortho) {
                    vp_points.op(p);
                }
                for p in viewport.frustum_world_points(&viewport.view_rectangle, !ortho) {
                    vp_points.op(p);
                }
                for plane in viewport.frustum_world_planes(&viewport.view_rectangle, ortho) {
                    vp_points.op(plane.point);
                    vp_points.ov(plane.normal);
                }
                vp_points.end(None);
            }
            {
                let mut copy = viewport;
                let which = r.next() % 6;
                let a = r.r(0.05, 2.0);
                let b = r.r(0.5, 200.0);
                let rw = (100 + r.next() % 900) as c_long;
                let rh = (100 + r.next() % 700) as c_long;
                let rectangle = rect(0.0, 0.0, rw as f32, rh as f32);
                match which {
                    0 => copy.set_view_rectangle(&rectangle),
                    1 => copy.set_field_of_view(a),
                    2 => copy.set_front_clipping_distance(b),
                    3 => copy.set_rear_clipping_distance(b),
                    4 => copy.set_orthographic_rectangle(&rectangle),
                    _ => {
                        let mut t2 = GeTransformation::IDENTITY;
                        t2.set_orientation(RotationOrder::Ypr, a, -b * 0.01, 0.0);
                        t2.m[0][3] = b;
                        t2.m[1][3] = a;
                        t2.m[2][3] = b;
                        copy.set_camera(&t2);
                    }
                }
                vp_set.iu(m);
                vp_set.iu(c);
                vp_set.iu(which);
                dump_viewport(&mut vp_set, &copy);
                vp_set.end(None);
            }
            let times = if c % 3 == 2 { 2 } else { 1 };
            for t in 0..times {
                let sets = mesh.set_views();
                let input = GeVisibleInput {
                    view: mesh.view(),
                    point_row_count: mesh.point_rows.len() as c_ulong,
                    left_edge: mesh.edges[0],
                    top_edge: mesh.edges[1],
                    right_edge: mesh.edges[2],
                    bottom_edge: mesh.edges[3],
                    space_tile_count: mesh.space_tile_count,
                    texture_sets: sets.as_ptr(),
                    texture_set_count: sets.len(),
                    space_cliff_textures: mesh.cliffs.as_ptr(),
                    space_cliff_texture_count: mesh.cliffs.len(),
                    water_texture_indices: mesh.water_grid.as_ptr(),
                    side: SIDE as c_ulong,
                };
                // SAFETY: the input describes this test's mesh
                let plan = unsafe { update_visible_terrain(&input, &viewport, ortho) };
                seen_cliffs += plan.cliff_points.len();
                vis.iu(m);
                vis.iu(c);
                vis.iu(t);
                vis.iu(kind);
                match dump_visible(&mut vis, &mut mesh, &plan) {
                    Some(note) => vis.end(Some(&note)),
                    None => vis.end(Some("skip")),
                }
            }
            for q in 0..2u32 {
                let mut loc = GeTransformation::IDENTITY;
                let ly = r.r(0.0, 6.3);
                let lp = r.r(-1.5, 1.5);
                let lr = r.r(-1.5, 1.5);
                loc.set_orientation(RotationOrder::Ypr, ly, lp, lr);
                let hx = viewport.camera.x() + r.r(-30.0, 30.0);
                let hy = viewport.camera.y() + r.r(-30.0, 30.0);
                let hz = r.r(-5.0, 15.0);
                loc.m[0][3] = hx;
                loc.m[1][3] = hy;
                loc.m[2][3] = hz;
                let scale = if r.next() & 1 != 0 { 1.0 } else { r.r(0.3, 3.0) };
                let mut internal = GeTransformation::IDENTITY;
                if r.next() & 1 != 0 {
                    let iy = r.r(0.0, 6.3);
                    let ip = r.r(-1.0, 1.0);
                    internal.set_orientation(RotationOrder::Ypr, iy, ip, 0.0);
                    let ix = r.r(-2.0, 2.0);
                    let iyy = r.r(-2.0, 2.0);
                    let iz = r.r(-2.0, 2.0);
                    internal.m[0][3] = ix;
                    internal.m[1][3] = iyy;
                    internal.m[2][3] = iz;
                }
                let cx = r.r(-2.0, 2.0);
                let cy = r.r(-2.0, 2.0);
                let cz = r.r(-2.0, 2.0);
                let center = Ge3DPoint::new(cx, cy, cz);
                let radius = r.r(0.2, 8.0);
                let bx = r.r(-3.0, 0.0);
                let by = r.r(-3.0, 0.0);
                let bz = r.r(-3.0, 0.0);
                let tx2 = r.r(0.0, 3.0);
                let ty2 = r.r(0.0, 3.0);
                let tz2 = r.r(0.0, 3.0);
                let bbox = GeBoundingBox::new(Ge3DPoint::new(bx, by, bz), Ge3DPoint::new(tx2, ty2, tz2));
                let o = if q == 1 { !ortho } else { ortho };
                let hit = model_visible(&loc, scale, &internal, center, radius, &bbox, &viewport, o);
                pm.iu(m);
                pm.iu(c);
                pm.iu(q);
                pm.iu(o as u32);
                pm.ou(hit as u32);
                pm.end(None);
            }
        }
    }
    assert!(seen_cliffs > 0, "no space map grew cliffs");
    // the bitmaps and rhombi
    let mut bm = Section::new("bm_rot", false);
    let mut rh = Section::new("rh_rot", false);
    let mut st = Section::new("bm_stretch", false);
    for _ in 0..3000 {
        let lx = r.r(-50.0, 1000.0);
        let ly = r.r(-50.0, 800.0);
        let w = r.r(1.0, 512.0);
        let h = r.r(1.0, 512.0);
        let xs = if r.next() & 3 != 0 { r.r(0.1, 3.0) } else { 1.0 };
        let ys = if r.next() & 3 != 0 { r.r(0.1, 3.0) } else { 1.0 };
        let rot = if r.next() & 1 != 0 { r.r(-6.3, 6.3) } else { 0.0 };
        let corners = bitmap_corners(Ge2DPoint::new(lx, ly), w, h, xs, ys, rot);
        for v in [lx, ly, w, h, xs, ys, rot] {
            bm.iu(v.to_bits());
        }
        for c in corners {
            bm.o(c.x);
            bm.o(c.y);
        }
        bm.end(None);
    }
    for _ in 0..3000 {
        let l = r.r(-50.0, 1000.0);
        let t = r.r(-50.0, 800.0);
        let rr = l + r.r(0.0, 400.0);
        let b = t + r.r(0.0, 300.0);
        let rot = if r.next() & 1 != 0 { r.r(-6.3, 6.3) } else { 0.0 };
        let corners = rhombus_corners(&rect(l, t, rr, b), rot);
        for v in [l, t, rr, b, rot] {
            rh.iu(v.to_bits());
        }
        for c in corners {
            rh.o(c.x);
            rh.o(c.y);
        }
        rh.end(None);
    }
    for _ in 0..3000 {
        let w = 1u32 << (3 + r.next() % 8);
        let h = 1u32 << (3 + r.next() % 8);
        let drop = r.next() % 3;
        let tl = (r.next() % 64) as f32;
        let tt = (r.next() % 64) as f32;
        let tr = tl + (r.next() % 512) as f32;
        let tb = tt + (r.next() % 512) as f32;
        let sl = tl + (r.next() % 64) as f32;
        let stp = tt + (r.next() % 64) as f32;
        let sr = sl + (r.next() % 256) as f32;
        let sb = stp + (r.next() % 256) as f32;
        let dl = r.r(-10.0, 100.0);
        let dt = r.r(-10.0, 100.0);
        let dr = dl + r.r(1.0, 600.0);
        let db = dt + r.r(1.0, 600.0);
        let s = stretched_coords(
            &rect(tl, tt, tr, tb),
            &rect(sl, stp, sr, sb),
            &rect(dl, dt, dr, db),
            w as c_ulong,
            h as c_ulong,
            drop as c_ulong,
        );
        st.iu(w);
        st.iu(h);
        st.iu(drop);
        for v in [tl, tt, tr, tb, sl, stp, sr, sb, dl, dt, dr, db] {
            st.iu(v.to_bits());
        }
        for uv in s.uv {
            st.o(uv[0]);
            st.o(uv[1]);
        }
        st.o(s.x_scale);
        st.o(s.y_scale);
        st.end(None);
    }
    check(&bm, &golden);
    check(&rh, &golden);
    check(&st, &golden);
    check(&vp, &golden);
    check(&vp_points, &golden);
    check(&vp_set, &golden);
    check(&vis, &golden);
    check(&pm, &golden);
}
