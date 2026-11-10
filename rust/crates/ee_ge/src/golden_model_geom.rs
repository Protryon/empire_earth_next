//! `GEModel`'s geometry against golden values from the real C++ (`golden_model_geom.txt`). The scratch driver of
//! R4.3's model reader (PORT_LEARNINGS.md "R4.3, third part") built 500 models in memory with the original `GEModel.cpp` (one to six triangles, three to
//! eight vertices, up to three tag points, one or two materials, one to three frames, an LCG seeded 11 drawing the
//! points, a repeated point now and then, normals, tag points and names, transforms, the graphics scale and frame),
//! ran `ComputeBoundingVolumes`, `ComputeModelNormals`, `PrepareTransforms`, `Verify`, `SortTagPoints` and
//! `RemoveScale` in that order and printed a digest of the state after each. This test builds the same models and
//! runs the C++'s loops over `model_geom`'s functions (what the forwarders in `GEModel.cpp` do). The driver relinked
//! over the forwarders printed the same lines, and so did it over every model of the game data, sub-models included.

use crate::model_geom::frame_is_sound;
use crate::model_geom::{
    center, frame_box, graphics_transform, radius, scale_removal, tag_point_order, vertex_normals,
};
use crate::types::{Ge3DPoint, GeBoundingBox, GeTransformation, GeVector};

const GOLDEN: &str = include_str!("golden_model_geom.txt");

/// FNV-1a 64, the driver's digest.
struct Fnv(u64);

impl Fnv {
    fn new() -> Fnv {
        Fnv(0xcbf2_9ce4_8422_2325)
    }
    fn b(&mut self, bytes: &[u8]) {
        for &c in bytes {
            self.0 ^= c as u64;
            self.0 = self.0.wrapping_mul(0x100000001b3);
        }
    }
    fn u(&mut self, v: u64) {
        self.b(&v.to_le_bytes());
    }
    fn f(&mut self, v: f32) {
        self.u(v.to_bits() as u64);
    }
    fn s(&mut self, text: &[u8]) {
        self.u(text.len() as u64);
        self.b(text);
    }
}

struct Gen(u32);

impl Gen {
    fn n(&mut self) -> u32 {
        self.0 = self.0.wrapping_mul(1664525).wrapping_add(1013904223);
        self.0
    }
    fn r(&mut self, n: u32) -> u32 {
        self.n() % n
    }
    fn f(&mut self) -> f32 {
        (self.n() >> 8) as f32 * (1.0 / 16777216.0) * 8.0 - 4.0
    }
    fn transform(&mut self) -> GeTransformation {
        let mut t = GeTransformation::IDENTITY;
        for r in 0..3 {
            for c in 0..4 {
                t.m[r][c] = self.f();
            }
        }
        t.m[3] = [0.0, 0.0, 0.0, 1.0];
        t
    }
}

/// A driver vertex: point, normal, texture coordinate, colour, opacity.
type Vertex = [f32; 12];

struct Model {
    frames: Vec<Vec<Vertex>>,
    tags: Vec<Vec<[f32; 3]>>,
    internal: Vec<GeTransformation>,
    indices: Vec<u32>,
    ranges: Vec<[u32; 3]>,
    names: Vec<Vec<u8>>,
    pre: GeTransformation,
    post: GeTransformation,
    scale: f32,
    current: usize,
    boxes: Vec<GeBoundingBox>,
    radii: Vec<f32>,
    center: Ge3DPoint,
    graphics: GeTransformation,
}

fn point(v: &Vertex) -> Ge3DPoint {
    Ge3DPoint::new(v[0], v[1], v[2])
}

impl Model {
    fn generate(g: &mut Gen) -> Model {
        const TAGS: [&[u8]; 6] = [b"", b"a", b"b", b"Z", b"\xe9t", b"aa"];
        let p = 1 + g.r(6);
        let v = 3 + g.r(6);
        let t = g.r(4);
        let m = 1 + g.r(2);
        let fr = 1 + g.r(3);
        let mut model = Model {
            frames: Vec::new(),
            tags: Vec::new(),
            internal: Vec::new(),
            indices: Vec::new(),
            ranges: Vec::new(),
            names: Vec::new(),
            pre: GeTransformation::IDENTITY,
            post: GeTransformation::IDENTITY,
            scale: 1.0,
            current: 0,
            boxes: vec![GeBoundingBox::new(Ge3DPoint::default(), Ge3DPoint::new(1.0, 1.0, 1.0)); fr as usize],
            radii: vec![0.0; fr as usize],
            center: Ge3DPoint::default(),
            graphics: GeTransformation::IDENTITY,
        };
        for _ in 0..fr {
            let mut vertices = vec![[0.0f32; 12]; v as usize];
            for i in 0..v as usize {
                if i > 0 && g.r(8) == 0 {
                    let w = g.r(i as u32) as usize;
                    let copy = [vertices[w][0], vertices[w][1], vertices[w][2]];
                    vertices[i][0..3].copy_from_slice(&copy);
                } else {
                    for k in 0..3 {
                        vertices[i][k] = g.f();
                    }
                }
                for k in 3..6 {
                    vertices[i][k] = g.f();
                }
            }
            model.frames.push(vertices);
            let mut tags = Vec::new();
            for _ in 0..t {
                let x = g.f();
                let y = g.f();
                let z = g.f();
                tags.push([x, y, z]);
            }
            model.tags.push(tags);
            let internal = g.transform();
            model.internal.push(internal);
        }
        for _ in 0..p * 3 {
            let i = g.r(v - 1);
            model.indices.push(i);
        }
        let mut start = 0;
        for k in 0..m {
            let n = if k + 1 == m { p - start } else { g.r(p - start + 1) };
            let vertex = g.r(2);
            model.ranges.push([start, n, vertex]);
            start += n;
        }
        for _ in 0..t {
            model.names.push(TAGS[g.r(6) as usize].to_vec());
        }
        model.pre = g.transform();
        model.post = g.transform();
        model.scale = g.f() + 5.0;
        model.current = g.r(fr) as usize;
        model
    }

    fn points(&self, f: usize) -> Vec<Ge3DPoint> {
        self.frames[f].iter().map(point).collect()
    }

    fn volumes(&self, d: &mut Fnv) {
        for (b, &r) in self.boxes.iter().zip(&self.radii) {
            let p = [b.bottom_left_front, b.top_right_back];
            for c in p {
                for v in [c.x, c.y, c.z] {
                    d.f(v);
                }
            }
            for v in [b.width, b.height, b.depth, r] {
                d.f(v);
            }
        }
        for v in [self.center.x, self.center.y, self.center.z] {
            d.f(v);
        }
    }

    fn arrays(&self, d: &mut Fnv) {
        for (vertices, tags) in self.frames.iter().zip(&self.tags) {
            for v in vertices {
                for x in v {
                    d.b(&x.to_le_bytes());
                }
            }
            for t in tags {
                for &x in t {
                    d.f(x);
                }
            }
        }
    }

    fn run(&mut self, name: &str) -> String {
        let (mut d1, mut d2, mut d3, mut d4, mut d5, mut d6) =
            (Fnv::new(), Fnv::new(), Fnv::new(), Fnv::new(), Fnv::new(), Fnv::new());
        let frames = self.frames.len();
        // ComputeBoundingVolumes
        for f in 0..frames {
            self.boxes[f] = frame_box(&self.points(f));
        }
        self.center = center(&self.boxes);
        for f in 0..frames {
            self.radii[f] = radius(self.center, &self.points(f));
        }
        self.volumes(&mut d1);
        // ComputeModelNormals
        for f in 0..frames {
            let normals = vertex_normals(&self.points(f), &self.indices);
            for (v, n) in self.frames[f].iter_mut().zip(normals) {
                v[3..6].copy_from_slice(&[n.x, n.y, n.z]);
            }
        }
        self.arrays(&mut d2);
        // PrepareTransforms
        self.graphics = graphics_transform(&self.internal[self.current], &self.pre, &self.post, self.scale);
        for row in self.graphics.m {
            for v in row {
                d3.f(v);
            }
        }
        // Verify
        let sound = (0..frames).filter(|&f| !frame_is_sound(&self.points(f), &self.indices, &self.ranges)).count() == 0;
        d4.u(sound as u64);
        // SortTagPoints
        let first: Vec<i8> = self.names.iter().map(|n| n.first().copied().unwrap_or(0) as i8).collect();
        for (i, minimum) in tag_point_order(&first).into_iter().enumerate() {
            let minimum = minimum as usize;
            if minimum == i {
                continue;
            }
            self.names.swap(i, minimum);
            for tags in self.tags.iter_mut() {
                tags.swap(i, minimum);
            }
        }
        for n in &self.names {
            d5.s(n);
        }
        self.arrays(&mut d5);
        // RemoveScale
        for f in 0..frames {
            let r = scale_removal(&self.internal[f], &GeTransformation::IDENTITY, 1.0, 1.0, 1.0);
            if f == 0 {
                self.center = r.scale.apply_point(self.center);
            }
            self.radii[f] *= r.largest;
            self.boxes[f] = self.boxes[f].transform(&r.scale);
            for v in self.frames[f].iter_mut() {
                let p = r.scale.apply_point(point(v));
                v[0..3].copy_from_slice(&[p.x, p.y, p.z]);
                let n = r.scale.apply_vector(GeVector::new(v[3], v[4], v[5]));
                v[3..6].copy_from_slice(&[n.x, n.y, n.z]);
            }
            for t in self.tags[f].iter_mut() {
                let p = r.scale.apply_point(Ge3DPoint::new(t[0], t[1], t[2]));
                *t = [p.x, p.y, p.z];
            }
            self.internal[f] = r.unscaled;
        }
        self.volumes(&mut d6);
        self.arrays(&mut d6);
        for t in &self.internal {
            for row in t.m {
                for v in row {
                    d6.f(v);
                }
            }
        }
        let _ = d4;
        format!("{name} geom {:016x} {:016x} {:016x} {} {:016x} {:016x}", d1.0, d2.0, d3.0, sound as i32, d5.0, d6.0)
    }
}

fn lines(count: usize) -> Vec<String> {
    let mut g = Gen(11);
    (0..count).map(|i| Model::generate(&mut g).run(&format!("geom{i}"))).collect()
}

#[test]
fn geometry_equals_the_cpp() {
    let lines = lines(500);
    if let Ok(path) = std::env::var("EE_GE_MODEL_GEOM_LINES") {
        let text = std::fs::read_to_string(path).unwrap();
        for (k, (a, b)) in text.lines().zip(&lines).enumerate() {
            assert_eq!(a, b, "case {k}");
        }
    }
    let mut d = Fnv::new();
    for l in &lines {
        d.b(l.as_bytes());
        d.b(b"\n");
    }
    let sound = lines.iter().filter(|l| l.contains(" 1 ")).count();
    let mut want = format!("# model_geometry {} sound {} {:016x}\n", lines.len(), sound, d.0);
    for l in lines.iter().take(4) {
        want += l;
        want += "\n";
    }
    assert_eq!(want, GOLDEN.replace("\r\n", "\n"));
}
