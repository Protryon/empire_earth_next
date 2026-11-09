//! `GEModel::Load` against golden values from the real C++ (`golden_model.txt`). A scratch driver linked the original
//! text of 'GEModel Load.cpp', 'GEModel Legacy.cpp' and `GEModel.cpp` (the allocation) with arrays of its own
//! behind a fake rasterizer, an `FSMemoryFile` over each case and every allocation zeroed, and printed per case the
//! load's result, the file position after it and an FNV-1a digest of the whole model it left: counts, name, centre,
//! every LOD's array, every material, the texture and tag point names, and per frame the radius, transform, box,
//! vertex array and tag points (an array shared with an earlier frame as that frame's number), then the
//! sub-models. The cases are 600 synthetic models from an LCG seeded 7 (two thirds current format, a third
//! version 1.0-1.3, sub-models one level deep), each whole, with every odd frame shared with the frame before, and
//! cut at 12 random lengths. This test draws the same stream, reads each case with `model_file::load`, stores the
//! result into a model laid out as the C++ allocates it (what `GEModel::StoreLoaded` does) and hashes it the
//! driver's way. The same driver relinked over the forwarders printed the same lines, and so did it over every
//! model of the game data (863 files, whole, shared and cut: 45,447 cases).
//!
//! Writes past an array's end are dropped: the C++ conversion places a polygon listed twice past the end, and
//! the driver's arrays drop such writes (Rust drops them before they reach the array).

use crate::io::BufferIo;
use crate::model_file::{load, CemModel, SharedFrames};

const GOLDEN: &str = include_str!("golden_model.txt");

pub(crate) struct Gen {
    s: u32,
    b: Vec<u8>,
}

impl Gen {
    pub fn new(seed: u32) -> Gen {
        Gen { s: seed, b: Vec::new() }
    }
    /// The next model's bytes.
    pub fn model_bytes(&mut self) -> Vec<u8> {
        self.b.clear();
        self.model(0);
        self.b.clone()
    }
    fn n(&mut self) -> u32 {
        self.s = self.s.wrapping_mul(1664525).wrapping_add(1013904223);
        self.s
    }
    pub fn r(&mut self, n: u32) -> u32 {
        self.n() % n
    }
    fn f(&mut self) -> f32 {
        (self.n() >> 8) as f32 * (1.0 / 16777216.0) * 8.0 - 4.0
    }
    fn u8(&mut self, v: u32) {
        self.b.push(v as u8);
    }
    fn u16(&mut self, v: u32) {
        self.u8(v & 0xff);
        self.u8((v >> 8) & 0xff);
    }
    fn u32(&mut self, v: u32) {
        self.u16(v & 0xffff);
        self.u16(v >> 16);
    }
    fn f32(&mut self, v: f32) {
        self.u32(v.to_bits());
    }
    fn fs(&mut self, n: u32) {
        for _ in 0..n {
            let v = self.f();
            self.f32(v);
        }
    }
    fn str(&mut self) {
        const NAMES: [&[u8]; 6] = [b"", b"player color", b"a", b"bc", b"x\0y", b"player colour"];
        let k = self.r(6) as usize;
        let n = NAMES[k].len() as u32;
        if n == 0 && self.r(2) == 0 {
            self.u32(0);
            return;
        }
        self.u32(n + 1);
        for &c in NAMES[k] {
            self.u8(c as u32);
        }
        self.u8(0);
    }
    fn uv(&mut self) {
        const UV: [f32; 3] = [0.0, 0.5, 1.0];
        let a = self.r(3) as usize;
        let c = self.r(3) as usize;
        self.f32(UV[a]);
        self.f32(UV[c]);
    }
    fn header(&mut self, major: u32, minor: u32) {
        for c in b"SSMF" {
            self.u8(*c as u32);
        }
        self.u16(major);
        self.u16(minor);
    }
    fn current(&mut self, depth: u32) {
        let minor = self.r(4);
        self.header(2, minor);
        let p = 1 + self.r(4);
        let v = 3 + self.r(5);
        let t = self.r(3);
        let m = 1 + self.r(2);
        let fr = 1 + self.r(3);
        let s = if depth < 1 { self.r(2) } else { 0 };
        let l = 1 + self.r(3);
        for x in [p, v, t, m, fr, s, l] {
            self.u32(x);
        }
        self.str();
        self.fs(3);
        for lod in 0..l {
            let c = if lod == 0 { p } else { 1 + self.r(p) };
            self.u32(c);
            for _ in 0..c * 3 {
                let i = self.r(v + 1);
                self.u32(i);
            }
        }
        for _ in 0..m {
            self.str();
            let x = self.r(4);
            self.u32(x);
            for _ in 0..l {
                let a = self.r(p);
                self.u32(a);
                let b = self.r(p + 1);
                self.u32(b);
            }
            let a = self.r(v);
            self.u32(a);
            let b = self.r(v + 1);
            self.u32(b);
            self.str();
        }
        for _ in 0..t {
            self.str();
        }
        for _ in 0..fr {
            self.fs(1);
            for _ in 0..v {
                self.fs(6);
                self.uv();
            }
            self.fs(3 * t);
            self.fs(16);
            self.fs(6);
        }
        for _ in 0..s {
            self.model(depth + 1);
        }
    }
    fn legacy(&mut self, depth: u32) {
        let minor = self.r(4);
        self.header(1, minor);
        let fr = 1 + self.r(3);
        let m = 1 + self.r(3);
        let pts = 1 + self.r(5);
        let pol = 1 + self.r(4);
        let g = self.r(3);
        let v = 1 + self.r(6);
        let (mut t, mut s) = (0, 0);
        for x in [fr, m, pts, pol, g, v] {
            self.u32(x);
        }
        if minor != 0 {
            t = self.r(3);
            self.u32(t);
        }
        if minor >= 2 {
            s = if depth < 1 { self.r(2) } else { 0 };
            self.u32(s);
        }
        self.str();
        self.fs(3);
        let flag = self.r(2);
        self.u8(flag);
        if minor >= 3 {
            for _ in 0..pts {
                let x = self.r(9);
                self.u32(x);
            }
        }
        for _ in 0..pol {
            for _ in 0..3 {
                let x = self.r(v);
                self.u32(x);
                self.uv();
                self.fs(3);
                self.fs(4);
            }
        }
        for _ in 0..g {
            self.str();
            let n = self.r(3);
            self.u32(n);
            for _ in 0..n {
                let x = self.r(pol);
                self.u32(x);
            }
        }
        for _ in 0..m {
            let n = self.r(3);
            self.u32(n);
            for _ in 0..n {
                let x = self.r(pol);
                self.u32(x);
            }
            let tex = self.r(2);
            self.u8(tex);
            if tex != 0 {
                self.str();
                let x = self.r(5);
                self.u32(x);
            }
        }
        for _ in 0..v {
            let x = self.r(pts);
            self.u32(x);
            self.fs(1);
        }
        if minor != 0 {
            for _ in 0..t {
                self.str();
            }
        }
        for _ in 0..fr {
            self.fs(1);
            self.fs(3 * pts);
            for _ in 0..v {
                if minor < 3 {
                    self.fs(3);
                } else {
                    let x = self.r(11000);
                    self.u16(x);
                }
            }
            if minor < 3 {
                self.fs(3 * pol);
            }
            if minor >= 1 {
                self.fs(3 * t);
            }
            if minor >= 2 {
                self.fs(16);
            }
            if minor >= 3 {
                self.fs(6);
            }
        }
        for _ in 0..s {
            self.model(depth + 1);
        }
    }
    fn model(&mut self, depth: u32) {
        if self.r(3) == 0 {
            self.legacy(depth)
        } else {
            self.current(depth)
        }
    }
}

pub(crate) struct Fnv(pub u64);

impl Fnv {
    pub fn new() -> Fnv {
        Fnv(0xcbf2_9ce4_8422_2325)
    }
    pub fn b(&mut self, bytes: &[u8]) {
        for &c in bytes {
            self.0 ^= c as u64;
            self.0 = self.0.wrapping_mul(0x100000001b3);
        }
    }
    pub fn u(&mut self, v: u64) {
        self.b(&v.to_le_bytes());
    }
    pub fn f(&mut self, v: f32) {
        self.u(v.to_bits() as u64);
    }
    pub fn s(&mut self, text: &[u8]) {
        self.u(text.len() as u64);
        self.b(text);
    }
}

/// A vertex of the driver's arrays: point, normal, texture coordinate, colour, opacity.
type Vertex = [f32; 12];

struct Material {
    name: Vec<u8>,
    texture_index: u32,
    ranges: Vec<(u32, u32)>,
    vertex: (u32, u32),
}

/// A model as `GEModel` holds it after `AllocateInternalData` and `StoreLoaded` (`None` for a null array).
#[derive(Default)]
struct Model {
    counts: [u32; 7],
    shared: usize,
    name: Vec<u8>,
    center: [f32; 3],
    lods: Option<Vec<Option<Vec<u32>>>>,
    materials: Vec<Material>,
    texture_filenames: Option<Vec<Vec<u8>>>,
    tag_point_names: Option<Vec<Vec<u8>>>,
    radii: Vec<f32>,
    transforms: Vec<[f32; 16]>,
    boxes: Vec<[f32; 9]>,
    /// Per frame, the array it points at (an index into `arrays`), and the same for the tag points.
    vertex_arrays: Vec<Option<usize>>,
    arrays: Vec<Vec<Vertex>>,
    frame_tag_points: Vec<Option<usize>>,
    tag_point_arrays: Vec<Vec<[f32; 3]>>,
    sub_models: Option<Vec<Option<Model>>>,
    allocated: bool,
}

fn store(cem: &CemModel, shared: SharedFrames) -> Model {
    // GEModel(GEModelManager*): one LOD, every other count zero (the driver zeroes the allocation)
    let mut m = Model { counts: [0, 0, 0, 0, 0, 0, 1], ..Model::default() };
    let Some(c) = cem.counts else {
        return m;
    };
    m.allocated = true;
    m.counts = [c.polygon, c.vertex, c.tag_point, c.material, c.frame, c.sub_model, c.lod];
    m.shared = shared.0.len();
    let frames = c.frame as usize;
    m.vertex_arrays = vec![None; frames];
    m.frame_tag_points = vec![None; frames];
    for f in 0..frames {
        if shared.get(f as u32).is_none() {
            m.arrays.push(vec![[0.0; 12]; c.vertex as usize]);
            m.vertex_arrays[f] = Some(m.arrays.len() - 1);
            if c.tag_point > 0 {
                m.tag_point_arrays.push(vec![[0.0; 3]; c.tag_point as usize]);
                m.frame_tag_points[f] = Some(m.tag_point_arrays.len() - 1);
            }
        }
    }
    for f in 0..frames {
        if let Some(offset) = shared.get(f as u32) {
            let target = (f as i64 + offset as i64) as usize;
            m.vertex_arrays[f] = m.vertex_arrays[target];
            m.frame_tag_points[f] = m.frame_tag_points[target];
        }
    }
    let mut lods: Vec<Option<Vec<u32>>> = vec![None; c.lod as usize];
    lods[0] = Some(vec![0; c.polygon as usize * 3]);
    m.transforms = vec![crate::model_file::tests::IDENTITY; frames];
    m.boxes = vec![[0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]; frames];
    m.radii = vec![0.0; frames];
    m.materials = (0..c.material)
        .map(|_| Material { name: Vec::new(), texture_index: 0, ranges: vec![(0, 0); c.lod as usize], vertex: (0, 0) })
        .collect();
    m.texture_filenames = Some(vec![Vec::new(); c.material as usize]);
    m.tag_point_names = if c.tag_point > 0 { Some(vec![Vec::new(); c.tag_point as usize]) } else { None };
    m.name = cem.name.clone();
    for (k, &v) in cem.center.iter().enumerate() {
        m.center[k] = v;
    }
    for (i, lod) in cem.lods.iter().enumerate() {
        if let Some(count) = lod.created {
            lods[i] = Some(vec![0; count as usize * 3]);
        }
        if lod.indices.is_empty() {
            continue;
        }
        // indices follow only a count that was read, so the array exists
        let array = lods[i].as_mut().unwrap();
        for (k, &index) in lod.indices.iter().enumerate() {
            if let Some(slot) = array.get_mut(k) {
                *slot = index;
            }
        }
    }
    m.lods = Some(lods);
    let l = c.lod as usize;
    for (i, mat) in cem.materials.iter().enumerate() {
        let t = &mut m.materials[i];
        if !mat.name.is_empty() {
            t.name = mat.name.clone();
        }
        for (k, &v) in mat.values.iter().enumerate() {
            if k == 0 {
                t.texture_index = v;
            } else if k <= 2 * l {
                if (k - 1) % 2 == 0 {
                    t.ranges[(k - 1) / 2].0 = v;
                } else {
                    t.ranges[(k - 1) / 2].1 = v;
                }
            } else if k == 2 * l + 1 {
                t.vertex.0 = v;
            } else {
                t.vertex.1 = v;
            }
        }
        m.texture_filenames.as_mut().unwrap()[i] = mat.texture_filename.clone();
    }
    if let Some(names) = m.tag_point_names.as_mut() {
        for (i, n) in cem.tag_point_names.iter().enumerate() {
            names[i] = n.clone();
        }
    }
    for (i, f) in cem.frames.iter().enumerate() {
        if let Some(r) = f.radius {
            m.radii[i] = r;
        }
        if let Some(a) = m.vertex_arrays[f.target as usize] {
            let array = &mut m.arrays[a];
            for (k, p) in f.points.iter().enumerate() {
                array[k][0..3].copy_from_slice(&[p.x, p.y, p.z]);
            }
            for (k, n) in f.normals.iter().enumerate() {
                array[k][3..6].copy_from_slice(&[n.x, n.y, n.z]);
            }
            for (k, uv) in f.uvs.iter().enumerate() {
                array[k][6..8].copy_from_slice(uv);
            }
        }
        if let Some(t) = m.frame_tag_points[f.target as usize] {
            for (k, p) in f.tag_points.iter().enumerate() {
                m.tag_point_arrays[t][k] = [p.x, p.y, p.z];
            }
        }
        for (k, &v) in f.transform.iter().enumerate() {
            m.transforms[i][k] = v;
        }
        let b = &mut m.boxes[i];
        for (k, &v) in f.bbox.iter().enumerate() {
            b[k] = v;
        }
        if f.bbox.len() == 6 {
            b[6] = b[3] - b[0];
            b[7] = b[5] - b[2];
            b[8] = b[4] - b[1];
        }
    }
    let mut subs: Vec<Option<Model>> = (0..c.sub_model).map(|_| None).collect();
    for (i, s) in cem.sub_models.iter().enumerate() {
        subs[i] = Some(store(s, shared));
    }
    m.sub_models = Some(subs);
    m
}

fn dump(m: &Model, d: &mut Fnv) {
    for &c in &m.counts {
        d.u(c as u64);
    }
    d.u(m.shared as u64);
    d.s(&m.name);
    for &v in &m.center {
        d.f(v);
    }
    if !m.allocated {
        return;
    }
    for lod in m.lods.as_ref().unwrap() {
        d.u(lod.is_some() as u64);
        if let Some(a) = lod {
            d.u(a.len() as u64);
            for &i in a {
                d.u(i as u64);
            }
        }
    }
    for t in &m.materials {
        d.u(1);
        d.s(&t.name);
        for v in [1.0, 1.0, 1.0, 1.0] {
            d.f(v);
        }
        d.u(t.texture_index as u64);
        d.u(t.ranges.len() as u64);
        for &(a, b) in &t.ranges {
            d.u(a as u64);
            d.u(b as u64);
        }
        d.u(t.vertex.0 as u64);
        d.u(t.vertex.1 as u64);
    }
    for n in m.texture_filenames.as_ref().unwrap() {
        d.s(n);
    }
    if let Some(names) = &m.tag_point_names {
        for n in names {
            d.s(n);
        }
    }
    for i in 0..m.counts[4] as usize {
        d.f(m.radii[i]);
        for &v in &m.transforms[i] {
            d.f(v);
        }
        for &v in &m.boxes[i] {
            d.f(v);
        }
        let same = (0..i).find(|&j| m.vertex_arrays[j] == m.vertex_arrays[i]).unwrap_or(i);
        d.u(same as u64);
        d.u(m.vertex_arrays[i].is_some() as u64);
        if let (Some(a), true) = (m.vertex_arrays[i], same == i) {
            d.u(0);
            d.u(m.arrays[a].len() as u64);
            for v in &m.arrays[a] {
                for &x in v {
                    d.b(&x.to_le_bytes());
                }
            }
        }
        let same = (0..i).find(|&j| m.frame_tag_points[j] == m.frame_tag_points[i]).unwrap_or(i);
        d.u(same as u64);
        d.u(m.frame_tag_points[i].is_some() as u64);
        if let (Some(t), true) = (m.frame_tag_points[i], same == i) {
            for p in &m.tag_point_arrays[t] {
                for &x in p {
                    d.f(x);
                }
            }
        }
    }
    for s in m.sub_models.as_ref().unwrap() {
        d.u(s.is_some() as u64);
        if let Some(s) = s {
            d.u(1);
            dump(s, d);
        }
    }
}

fn run_case(name: &str, bytes: &[u8], shared: &[(u32, i32)]) -> String {
    let mut io = BufferIo::reader(bytes.to_vec());
    let cem = load(&mut io, SharedFrames(shared));
    let model = store(&cem, SharedFrames(shared));
    let mut d = Fnv(0xcbf2_9ce4_8422_2325);
    d.u(cem.result as u32 as u64);
    d.u(io.position as u64);
    dump(&model, &mut d);
    format!("{name} r{:x} pos{} {:016x}", cem.result as u32, io.position, d.0)
}

fn frame_count(b: &[u8]) -> u32 {
    if b.len() < 32 {
        return 0;
    }
    let major = u16::from_le_bytes([b[4], b[5]]);
    let at = if major >= 2 { 24 } else { 8 };
    u32::from_le_bytes(b[at..at + 4].try_into().unwrap())
}

fn cases(count: usize) -> Vec<String> {
    let mut g = Gen { s: 7, b: Vec::new() };
    let mut out = Vec::new();
    for i in 0..count {
        g.b.clear();
        g.model(0);
        let bytes = g.b.clone();
        let name = format!("synth{i}");
        out.push(run_case(&name, &bytes, &[]));
        let frames = frame_count(&bytes);
        if frames >= 2 {
            let shared: Vec<(u32, i32)> = (1..frames).step_by(2).map(|f| (f, -1)).collect();
            out.push(run_case(&format!("{name}+shared"), &bytes, &shared));
        }
        for _ in 0..12 {
            let cut = g.r(bytes.len() as u32) as usize;
            out.push(run_case(&format!("synth{i}@{cut}"), &bytes[..cut], &[]));
        }
    }
    out
}

fn section(lines: &[String]) -> String {
    let mut d = Fnv(0xcbf2_9ce4_8422_2325);
    for l in lines {
        d.b(l.as_bytes());
        d.b(b"\n");
    }
    let ok = lines.iter().filter(|l| l.contains(" r0 ")).count();
    let mut s = format!("# model_load {} ok {} {:016x}\n", lines.len(), ok, d.0);
    for l in lines.iter().take(6) {
        s += l;
        s += "\n";
    }
    s
}

#[test]
fn loads_equal_the_cpp() {
    let lines = cases(600);
    // EE_GE_MODEL_LINES=<file>: compare line by line with a driver's output (the first difference)
    if let Ok(path) = std::env::var("EE_GE_MODEL_LINES") {
        let text = std::fs::read_to_string(path).unwrap();
        for (k, (a, b)) in text.lines().zip(&lines).enumerate() {
            assert_eq!(a, b, "case {k}");
        }
    }
    assert_eq!(section(&lines), GOLDEN.replace("\r\n", "\n"));
}
