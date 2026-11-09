//! `GEPhysicalModel`'s per-triangle tests against golden values from the real C++ (`golden_physical.txt`): a scratch
//! driver ran the original loop body of `Intersects(line)` (its `continue` as a return) and `ComputeTriangleBox` over
//! 40,000 triangles and lines from an LCG seeded 13 (lines through the first corner, parallel to the plane, aimed
//! inside the triangle, or anywhere; small integers for equal coordinates), printing each section's case count, an
//! FNV-1a digest of the outputs and its first four cases. The driver relinked over the Rust printed the same file.

use crate::physical::{line_hits_triangle, triangle_box};
use crate::types::{Ge3DLine, Ge3DPoint, GeVector};

const GOLDEN: &str = include_str!("golden_physical.txt");

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

    fn v(&mut self) -> f32 {
        if self.next() & 3 == 0 {
            return (self.next() % 5) as i32 as f32 - 2.0;
        }
        self.r(-4.0, 4.0)
    }

    fn p(&mut self) -> Ge3DPoint {
        let x = self.v();
        let y = self.v();
        let z = self.v();
        Ge3DPoint::new(x, y, z)
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

#[test]
fn every_case_matches_the_cpp() {
    let mut r = Rng(13);
    let mut tri = Section::new("pm_triangle");
    let mut boxes = Section::new("pm_box");
    for _ in 0..40000 {
        let p1 = r.p();
        let p2 = r.p();
        let p3 = r.p();
        let kind = r.next() % 6;
        let mut start = r.p();
        let dir = match kind {
            0 => {
                let s = r.r(-2.0, 2.0);
                let d = GeVector::new(p1.x - start.x, p1.y - start.y, p1.z - start.z);
                if r.next() & 1 == 0 {
                    start = Ge3DPoint::new(p1.x - d.x * s, p1.y - d.y * s, p1.z - d.z * s);
                }
                d
            }
            1 => GeVector::new(p2.x - p1.x, p2.y - p1.y, p2.z - p1.z),
            2 => {
                let a = r.r(0.0, 1.0);
                let b = r.r(0.0, 1.0 - a);
                let q = Ge3DPoint::new(
                    p1.x + (p2.x - p1.x) * a + (p3.x - p1.x) * b,
                    p1.y + (p2.y - p1.y) * a + (p3.y - p1.y) * b,
                    p1.z + (p2.z - p1.z) * a + (p3.z - p1.z) * b,
                );
                let s = r.r(0.5, 3.0);
                GeVector::new((q.x - start.x) * s, (q.y - start.y) * s, (q.z - start.z) * s)
            }
            _ => {
                let x = r.v();
                let y = r.v();
                let z = r.v();
                GeVector::new(x, y, z)
            }
        };
        let exact = r.next() & 1 != 0;
        let line = Ge3DLine { start, direction: dir };
        for v in [p1.x, p1.y, p1.z, p2.x, p2.y, p2.z, p3.x, p3.y, p3.z, start.x, start.y, start.z, dir.x, dir.y, dir.z]
        {
            tri.inputs.push(v.to_bits());
        }
        tri.inputs.push(u32::from(exact));
        let (hit, d) = line_hits_triangle(&line, p1, p2, p3, exact);
        tri.outputs.push(u32::from(hit));
        tri.outputs.push(d.unwrap_or(-12345.5).to_bits());
        tri.end();
        let b = triangle_box(p1, p2, p3);
        for v in [
            b.bottom_left_front.x,
            b.bottom_left_front.y,
            b.bottom_left_front.z,
            b.top_right_back.x,
            b.top_right_back.y,
            b.top_right_back.z,
            b.width,
            b.height,
            b.depth,
        ] {
            boxes.outputs.push(v.to_bits());
        }
        boxes.end();
    }
    let ours = tri.text() + &boxes.text();
    assert_eq!(ours, GOLDEN.replace("\r\n", "\n"));
}
