//! `GEPhysicalModel`'s walks (`physical_walk.rs`) against golden values from the real C++
//! (`golden_physical_walk.txt`). A scratch driver (`build/physical_golden`, PORT_LEARNINGS "R4.3, fifth part")
//! compiled the unmodified `GEPhysicalModel.cpp` with the engine's math units over fake models (`GEModel` objects
//! whose materials, LODs, polygon arrays, per-frame vertex arrays, internal transforms, radii, boxes and sub-models
//! come from an LCG seeded 31; the vertex and polygon arrays are fakes behind `GEVertexArray`'s and
//! `GEPolygonArray`'s virtuals) and ran 4 worlds of 12 top models with 3,000 cases each: a placed model against a
//! line (`Intersects(line, lod, distance, exact)`, every LOD, exact or not, scales 1, 0, 0.00005 and random) and a
//! model scaled per axis against the placed model (`Intersects(location, model, frame, line, exact, lod, x, y, z)`:
//! the model against model walk, its sub-models, the exact retry). This test builds the same models from the same
//! stream and replays every case through the Rust walks; the driver relinked over the forwarders printed the same
//! file.

#![allow(clippy::unnecessary_cast, clippy::needless_range_loop)]

use std::ffi::c_ulong;

use crate::physical_walk::{line_walk, scaled_walk, GePhysicalMaterial, Models, Placed};
use crate::transform::RotationOrder;
use crate::types::{Ge3DLine, Ge3DPoint, GeBoundingBox, GeTransformation, GeVector};

const GOLDEN: &str = include_str!("golden_physical_walk.txt");
const WORLDS: u32 = 4;
const TOPS: u32 = 12;
const CASES: u32 = 3000;

struct Rng(u32);

impl Rng {
    fn next(&mut self) -> u32 {
        self.0 = self.0.wrapping_mul(1664525).wrapping_add(1013904223);
        self.0
    }

    fn r(&mut self, lo: f32, hi: f32) -> f32 {
        let u = (self.next() >> 8) as f32 * (1.0 / 16777216.0);
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
}

impl Section {
    fn new(name: &'static str) -> Self {
        Section { name, count: 0, hash: 0xcbf2_9ce4_8422_2325, inputs: vec![], outputs: vec![], lines: String::new() }
    }

    fn fold(h: &mut u64, v: u32) {
        for k in 0..4 {
            *h ^= u64::from((v >> (8 * k)) & 0xff);
            *h = h.wrapping_mul(0x0000_0100_0000_01b3);
        }
    }

    fn end(&mut self) {
        for &v in &self.outputs {
            Self::fold(&mut self.hash, v);
        }
        if self.count < 4 {
            self.lines.push_str(self.name);
            for v in &self.inputs {
                self.lines.push_str(&format!(" {v:08x}"));
            }
            self.lines.push_str(" ->");
            for v in &self.outputs {
                self.lines.push_str(&format!(" {v:08x}"));
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

#[derive(Clone, Default)]
struct FakeMaterial {
    vertex_start: u32,
    vertex_count: u32,
    starts: Vec<u32>,
    lengths: Vec<u32>,
}

#[derive(Clone, Default)]
struct FakeModel {
    parent: bool,
    lods: u32,
    materials: Vec<FakeMaterial>,
    polygons: Vec<Vec<[u32; 3]>>,
    vertices: Vec<Vec<Ge3DPoint>>,
    internal: Vec<GeTransformation>,
    radii: Vec<f32>,
    boxes: Vec<GeBoundingBox>,
    center: Ge3DPoint,
    subs: Vec<usize>,
}

struct World(Vec<FakeModel>);

fn ypr(yaw: f32, pitch: f32, roll: f32) -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    t.set_orientation(RotationOrder::Ypr, yaw, pitch, roll);
    t
}

fn set_position(t: &mut GeTransformation, x: f32, y: f32, z: f32) {
    t.m[0][3] = x;
    t.m[1][3] = y;
    t.m[2][3] = z;
}

/// The driver's `Generate`: a model (and its sub-models, depth below 2) with `frames` frames
fn generate(r: &mut Rng, w: &mut World, depth: u32, frames: u32, parent: bool) -> usize {
    let mut m = FakeModel { parent, ..Default::default() };
    m.lods = 1 + r.next() % 4;
    let mats = 1 + r.next() % 3;
    let mut total = 0;
    for _ in 0..mats {
        let count = 3 + r.next() % 10;
        m.materials.push(FakeMaterial { vertex_start: total, vertex_count: count, ..Default::default() });
        total += count;
    }
    m.polygons = vec![Vec::new(); m.lods as usize];
    for lod in 0..m.lods as usize {
        for k in 0..mats as usize {
            let mut count = if r.next() & 1 != 0 { 1 + r.next() % 8 } else { 30 + r.next() % 40 };
            if r.next().is_multiple_of(8) {
                count = 0;
            }
            m.materials[k].starts.push(m.polygons[lod].len() as u32);
            m.materials[k].lengths.push(count);
            let vc = m.materials[k].vertex_count;
            for _ in 0..count {
                let a = r.next() % vc;
                let b = r.next() % vc;
                let c = r.next() % vc;
                m.polygons[lod].push([a, b, c]);
            }
        }
    }
    for _ in 0..frames {
        let mut points = Vec::new();
        for _ in 0..total {
            let x = r.r(-3.0, 3.0);
            let y = r.r(-3.0, 3.0);
            let z = r.r(-3.0, 3.0);
            points.push(Ge3DPoint::new(x, y, z));
        }
        m.vertices.push(points);
        let mut t = GeTransformation::IDENTITY;
        if r.next() & 1 != 0 {
            let yaw = r.r(0.0, 6.3);
            let pitch = r.r(-1.0, 1.0);
            let roll = r.r(-1.0, 1.0);
            t = ypr(yaw, pitch, roll);
            let x = r.r(-2.0, 2.0);
            let y = r.r(-2.0, 2.0);
            let z = r.r(-2.0, 2.0);
            set_position(&mut t, x, y, z);
        }
        m.internal.push(t);
        m.radii.push(r.r(0.5, 7.0));
        let bx = r.r(-4.0, 0.0);
        let by = r.r(-4.0, 0.0);
        let bz = r.r(-4.0, 0.0);
        let tx = r.r(0.0, 4.0);
        let ty = r.r(0.0, 4.0);
        let tz = r.r(0.0, 4.0);
        m.boxes.push(GeBoundingBox::new(Ge3DPoint::new(bx, by, bz), Ge3DPoint::new(tx, ty, tz)));
    }
    let cx = r.r(-1.0, 1.0);
    let cy = r.r(-1.0, 1.0);
    let cz = r.r(-1.0, 1.0);
    m.center = Ge3DPoint::new(cx, cy, cz);
    let index = w.0.len();
    w.0.push(m);
    if depth < 2 {
        let n = r.next() % 3;
        for _ in 0..n {
            let s = generate(r, w, depth + 1, frames, true);
            w.0[index].subs.push(s);
        }
    }
    index
}

impl Models for World {
    type M = usize;
    fn is_null(&self, m: usize) -> bool {
        m == usize::MAX
    }
    fn has_parent(&self, m: usize) -> bool {
        self.0[m].parent
    }
    fn internal_transform(&self, m: usize, frame: c_ulong) -> GeTransformation {
        self.0[m].internal[frame as usize]
    }
    fn center(&self, m: usize) -> Ge3DPoint {
        self.0[m].center
    }
    fn radius(&self, m: usize, frame: c_ulong) -> f32 {
        self.0[m].radii[frame as usize]
    }
    fn bbox(&self, m: usize, frame: c_ulong) -> GeBoundingBox {
        self.0[m].boxes[frame as usize]
    }
    fn material_count(&self, m: usize) -> c_ulong {
        self.0[m].materials.len() as c_ulong
    }
    fn material(&self, m: usize, index: c_ulong, lod: c_ulong) -> GePhysicalMaterial {
        let mat = &self.0[m].materials[index as usize];
        GePhysicalMaterial {
            vertex_start: mat.vertex_start as c_ulong,
            polygon_start: mat.starts[lod as usize] as c_ulong,
            polygon_length: mat.lengths[lod as usize] as c_ulong,
        }
    }
    fn triangle(
        &self,
        m: usize,
        frame: c_ulong,
        lod: c_ulong,
        polygon: c_ulong,
        vertex_start: c_ulong,
    ) -> [Ge3DPoint; 3] {
        let model = &self.0[m];
        let p = model.polygons[lod as usize][polygon as usize];
        let v = &model.vertices[frame as usize];
        [0, 1, 2].map(|k| v[(p[k] as c_ulong + vertex_start) as usize])
    }
    fn sub_model_count(&self, m: usize) -> c_ulong {
        self.0[m].subs.len() as c_ulong
    }
    fn sub_model(&self, m: usize, index: c_ulong) -> usize {
        self.0[m].subs[index as usize]
    }
    fn lod_count(&self, m: usize) -> c_ulong {
        self.0[m].lods as c_ulong
    }
    fn polygon_count(&self, m: usize, lod: c_ulong) -> c_ulong {
        self.0[m]
            .materials
            .iter()
            .map(|k| k.lengths[lod as usize] as c_ulong)
            .fold(0, |a: c_ulong, b| a.wrapping_add(b))
    }
    fn retry_line(
        &self,
        location: &GeTransformation,
        model: usize,
        frame: c_ulong,
        scale: f32,
        line: &Ge3DLine,
        lod: c_ulong,
    ) -> bool {
        // the C++'s temporary GEPhysicalModel: Intersects(line, lod, distance, true)
        let mut distance = 0.0f32;
        line_walk(self, scale, location, model, frame, line, lod, &mut distance, true)
    }
}

fn placement(r: &mut Rng, near: Option<Ge3DPoint>) -> GeTransformation {
    let yaw = r.r(0.0, 6.3);
    let pitch = r.r(-0.5, 0.5);
    let roll = r.r(-0.5, 0.5);
    let mut t = ypr(yaw, pitch, roll);
    let (bx, by, bz) = near.map_or((0.0, 0.0, 1.5), |p| (p.x, p.y, p.z));
    let x = bx + r.r(-6.0, 6.0);
    let y = by + r.r(-6.0, 6.0);
    let z = bz + r.r(-1.5, 1.5);
    set_position(&mut t, x, y, z);
    t
}

fn run() -> String {
    let mut r = Rng(31);
    let mut line_section = Section::new("pw_line");
    let mut scaled_section = Section::new("pw_scaled");
    let mut counts = Section::new("pw_counts");
    let mut tally = [0u32; 6];
    for world in 0..WORLDS {
        let mut w = World(Vec::new());
        let mut tops = Vec::new();
        let mut frames = Vec::new();
        for _ in 0..TOPS {
            let f = 1 + r.next() % 3;
            tops.push(generate(&mut r, &mut w, 0, f, false));
            frames.push(f);
        }
        for k in 0..CASES {
            let ia = (r.next() % TOPS) as usize;
            let ib = (r.next() % TOPS) as usize;
            let location = placement(&mut r, None);
            let mode = r.next() % 6;
            let scale = match mode {
                0 => 1.0,
                1 => 0.0,
                2 => 0.00005,
                _ => r.r(0.3, 3.0),
            };
            let frame = r.next() % frames[ia];
            let at = Ge3DPoint::new(location.m[0][3], location.m[1][3], location.m[2][3]);
            let sx = r.r(-15.0, 15.0);
            let sy = r.r(-15.0, 15.0);
            let sz = r.r(-2.0, 8.0);
            let ax = at.x + r.r(-4.0, 4.0);
            let ay = at.y + r.r(-4.0, 4.0);
            let az = at.z + r.r(-2.0, 2.0);
            let length = r.r(0.3, 2.0);
            let line = Ge3DLine {
                start: Ge3DPoint::new(sx, sy, sz),
                direction: GeVector::new((ax - sx) * length, (ay - sy) * length, (az - sz) * length),
            };
            let lod = r.next() % 3;
            let exact = r.next() & 1 != 0;
            let mut distance = -12345.0f32;
            let hit = line_walk(
                &w,
                scale,
                &location,
                tops[ia],
                frame as c_ulong,
                &line,
                lod as c_ulong,
                &mut distance,
                exact,
            );
            line_section.inputs.extend([world, k, ia as u32, frame, lod, exact as u32, scale.to_bits()]);
            line_section.outputs.extend([hit as u32, distance.to_bits()]);
            tally[if hit { lod as usize } else { 3 }] += 1;
            line_section.end();

            let other_location = placement(&mut r, Some(at));
            let (x, y, z) = match r.next() % 4 {
                0 => (1.0, 1.0, 1.0),
                1 => {
                    let u = r.r(0.3, 3.0);
                    (u, u, u)
                }
                2 => {
                    let x = r.r(0.3, 3.0);
                    let y = r.r(0.3, 3.0);
                    let z = r.r(0.3, 3.0);
                    (x, y, z)
                }
                _ => {
                    let x = r.r(0.3, 3.0);
                    let z = r.r(0.3, 3.0);
                    (x, 0.0, z)
                }
            };
            let other_frame = r.next() % frames[ib];
            let lod2 = r.next() % 3;
            let exact2 = r.next() & 1 != 0;
            let this = Placed { location, model: tops[ia], scale, frame: frame as c_ulong };
            let hit2 = scaled_walk(
                &w,
                &this,
                &other_location,
                tops[ib],
                other_frame as c_ulong,
                &line,
                exact2,
                lod2 as c_ulong,
                x,
                y,
                z,
            );
            scaled_section.inputs.extend([world, k, ia as u32, ib as u32, other_frame, lod2, exact2 as u32]);
            scaled_section.outputs.push(hit2 as u32);
            tally[if hit2 { 4 } else { 5 }] += 1;
            scaled_section.end();
        }
    }
    counts.outputs.extend(tally);
    counts.end();
    line_section.text() + &scaled_section.text() + &counts.text()
}

/// The hits per section (line hits at LOD 0, 1, 2, line misses, scaled hits, scaled misses), from the golden
fn golden_counts() -> Vec<u32> {
    let line = GOLDEN.lines().find(|l| l.starts_with("pw_counts")).expect("the counts line");
    line.split("->").nth(1).unwrap().split_whitespace().map(|v| u32::from_str_radix(v, 16).unwrap()).collect()
}

#[test]
fn walks_match_the_cpp() {
    let ours = run();
    let golden: String = GOLDEN.replace("\r\n", "\n");
    for (a, b) in ours.lines().zip(golden.lines()) {
        assert_eq!(a, b);
    }
    assert_eq!(ours.lines().count(), golden.lines().count());
}

#[test]
fn the_cases_reach_every_branch() {
    // a digest of all misses would pass for a broken walk: every LOD hits, and so does the scaled walk
    let c = golden_counts();
    assert_eq!(c.len(), 6);
    for (k, &n) in c.iter().enumerate() {
        assert!(n > 100, "count {k} is {n}");
    }
}
