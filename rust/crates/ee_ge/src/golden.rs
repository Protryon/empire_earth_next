//! Golden values from the real C++ (`golden_math.txt`): a scratch program linked the unmodified GE math units and
//! `UMath.cpp` (the game build's objects, clang-cl -O1) and ran every function over inputs from a small LCG; for each
//! section it printed the case count, an FNV-1a digest of every output bit pattern, and the first six cases in full.
//! These tests draw the same inputs in the same order, call the C ABI the C++ forwarders call, and compare.
//!
//! Outputs the C++ leaves unwritten keep a sentinel (-12345.5), so "not written" is pinned too; the aliased calls
//! (an output that is also an input) are cases of their own.

use std::collections::HashMap;
use std::ffi::c_void;

use crate::ffi::*;
use crate::types::*;
use crate::vector::QUANT_COUNT;

const GOLDEN: &str = include_str!("golden_math.txt");
pub(crate) const SENTINEL: f32 = -12345.5;
const N: usize = 20000;
const SPECIALS: [f32; 18] = [
    0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 0.0001, -0.0001, 0.00001, 2.0, 3.1415927, -3.1415927, 1.5707964, 6.2831855, 100.0,
    0.9995, 0.99951, 1e-20,
];

/// The generator's LCG and draws, statement for statement.
struct Rng(u32);

impl Rng {
    fn next(&mut self) -> u32 {
        self.0 = self.0.wrapping_mul(1664525).wrapping_add(1013904223);
        self.0
    }

    fn unit(&mut self) -> f32 {
        (self.next() >> 8) as f32 * (1.0 / 16777216.0)
    }

    fn v(&mut self, lo: f32, hi: f32) -> f32 {
        let draw = self.next();
        if draw & 15 == 0 {
            return SPECIALS[((draw >> 4) as usize) % SPECIALS.len()];
        }
        let unit = self.unit();
        lo + unit * (hi - lo)
    }

    fn point(&mut self, r: f32) -> Ge3DPoint {
        let x = self.v(-r, r);
        let y = self.v(-r, r);
        let z = self.v(-r, r);
        Ge3DPoint { x, y, z }
    }

    fn vector(&mut self, r: f32) -> GeVector {
        let x = self.v(-r, r);
        let y = self.v(-r, r);
        let z = self.v(-r, r);
        GeVector { x, y, z }
    }

    fn point2(&mut self, r: f32) -> Ge2DPoint {
        let x = self.v(-r, r);
        let y = self.v(-r, r);
        Ge2DPoint { x, y }
    }

    fn matrix(&mut self, r: f32) -> GeTransformation {
        let mut t = GeTransformation::default();
        for row in t.m.iter_mut() {
            for e in row.iter_mut() {
                *e = self.v(-r, r);
            }
        }
        t
    }

    fn rigid(&mut self) -> GeTransformation {
        let mut t = GeTransformation::IDENTITY;
        let y = self.v(-7.0, 7.0);
        let p = self.v(-7.0, 7.0);
        let r = self.v(-7.0, 7.0);
        // SAFETY: a valid pointer
        unsafe { ee_ge_t_set_orientation(&mut t, 4, y, p, r) };
        t.m[0][3] = self.v(-500.0, 500.0);
        t.m[1][3] = self.v(-500.0, 500.0);
        t.m[2][3] = self.v(-50.0, 50.0);
        t
    }

    fn transformation(&mut self) -> GeTransformation {
        if self.next() & 1 != 0 {
            self.rigid()
        } else {
            self.matrix(2.0)
        }
    }
}

/// One section: the first cases as text, an FNV-1a digest of all outputs.
pub(crate) struct Section {
    name: &'static str,
    count: usize,
    hash: u64,
    inputs: Vec<u32>,
    outputs: Vec<u32>,
    lines: String,
    shown: usize,
}

impl Section {
    pub(crate) fn new(name: &'static str) -> Self {
        Section {
            name,
            count: 0,
            hash: 0xcbf2_9ce4_8422_2325,
            inputs: Vec::new(),
            outputs: Vec::new(),
            lines: String::new(),
            shown: 6,
        }
    }

    pub(crate) fn i(&mut self, v: f32) {
        self.inputs.push(v.to_bits());
    }

    pub(crate) fn iu(&mut self, v: u32) {
        self.inputs.push(v);
    }

    pub(crate) fn o(&mut self, v: f32) {
        self.outputs.push(v.to_bits());
    }

    pub(crate) fn ou(&mut self, v: u32) {
        self.outputs.push(v);
    }

    pub(crate) fn ip(&mut self, p: Ge3DPoint) {
        self.i(p.x);
        self.i(p.y);
        self.i(p.z);
    }

    pub(crate) fn iv(&mut self, v: GeVector) {
        self.i(v.x);
        self.i(v.y);
        self.i(v.z);
    }

    pub(crate) fn op(&mut self, p: Ge3DPoint) {
        self.o(p.x);
        self.o(p.y);
        self.o(p.z);
    }

    pub(crate) fn ov(&mut self, v: GeVector) {
        self.o(v.x);
        self.o(v.y);
        self.o(v.z);
    }

    pub(crate) fn im(&mut self, t: &GeTransformation) {
        for row in t.m {
            for e in row {
                self.i(e);
            }
        }
    }

    pub(crate) fn om(&mut self, t: &GeTransformation) {
        for row in t.m {
            for e in row {
                self.o(e);
            }
        }
    }

    pub(crate) fn oq(&mut self, q: &GeQuaternion) {
        self.o(q.x);
        self.o(q.y);
        self.o(q.z);
        self.o(q.w);
    }

    pub(crate) fn end(&mut self) {
        for v in &self.outputs {
            for k in 0..4 {
                self.hash ^= u64::from((v >> (8 * k)) & 0xff);
                self.hash = self.hash.wrapping_mul(0x0000_0100_0000_01b3);
            }
        }
        if self.count < self.shown {
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

    pub(crate) fn text(&self) -> String {
        format!("# {} {} {:016x}\n{}", self.name, self.count, self.hash, self.lines)
    }
}

/// The golden sections by name, as text.
fn golden_sections() -> HashMap<String, String> {
    let mut out: HashMap<String, String> = HashMap::new();
    let mut current = None;
    for line in GOLDEN.lines() {
        if let Some(header) = line.strip_prefix("# ") {
            let name = header.split(' ').next().unwrap().to_string();
            out.insert(name.clone(), format!("{line}\n"));
            current = Some(name);
        } else if let Some(name) = &current {
            let text = out.get_mut(name).unwrap();
            text.push_str(line);
            text.push('\n');
        }
    }
    out
}

/// Runs the sections in the generator's order (they share one random stream) and returns their text.
fn run_all() -> Vec<Section> {
    let mut r = Rng(1);
    let mut all = Vec::new();

    let mut s = Section::new("umcos");
    for _ in 0..200_000 {
        let a = r.v(-20.0, 20.0);
        s.i(a);
        s.o(ee_ge_um_cos(a));
        s.o(crate::um::um_sin(a));
        s.end();
    }
    all.push(s);

    let mut s = Section::new("umarccos");
    for _ in 0..200_000 {
        let a = r.v(-1.2, 1.2);
        s.i(a);
        s.o(ee_ge_um_arccos(a));
        s.end();
    }
    all.push(s);

    let mut s = Section::new("umarctan");
    for _ in 0..200_000 {
        let a = if r.next() & 1 != 0 { r.v(-3.0, 3.0) } else { r.v(-1e6, 1e6) };
        s.i(a);
        s.o(ee_ge_um_arctan(a));
        s.end();
    }
    all.push(s);

    let mut s = Section::new("umarctan2");
    for _ in 0..200_000 {
        let y = r.v(-100.0, 100.0);
        let x = r.v(-100.0, 100.0);
        s.i(y);
        s.i(x);
        s.o(ee_ge_um_arctan2(y, x));
        s.end();
    }
    all.push(s);

    let mut s = Section::new("umtan");
    for _ in 0..200_000 {
        let a = r.v(-10.0, 10.0);
        s.i(a);
        s.o(ee_ge_um_tan(a));
        s.end();
    }
    all.push(s);

    let mut s = Section::new("umquadratic");
    for _ in 0..N {
        let a = r.v(-10.0, 10.0);
        let b = r.v(-10.0, 10.0);
        let c = r.v(-10.0, 10.0);
        let (mut r1, mut r2) = (SENTINEL, SENTINEL);
        // SAFETY: valid pointers
        let ok = unsafe { ee_ge_um_quadratic(a, b, c, &mut r1, &mut r2) };
        s.i(a);
        s.i(b);
        s.i(c);
        s.ou(ok as u32);
        s.o(r1);
        s.o(r2);
        s.end();
    }
    all.push(s);

    // ---- GE2DPoint
    let mut s = Section::new("2d_direction");
    for _ in 0..N {
        let p = r.point2(50.0);
        let a = r.point2(50.0);
        let b = r.point2(50.0);
        for v in [p.x, p.y, a.x, a.y, b.x, b.y] {
            s.i(v);
        }
        // SAFETY: valid pointers
        s.o(unsafe { ee_ge_2d_direction(&p, &a, &b) });
        s.end();
    }
    all.push(s);

    let mut s = Section::new("2d_distance");
    for _ in 0..N {
        let p = r.point2(500.0);
        let a = r.point2(500.0);
        for v in [p.x, p.y, a.x, a.y] {
            s.i(v);
        }
        // SAFETY: valid pointers
        unsafe {
            s.o(ee_ge_2d_distance(&p, &a));
            s.o(ee_ge_2d_distance_squared(&p, &a));
        }
        s.end();
    }
    all.push(s);

    let mut s = Section::new("2d_rotate");
    for _ in 0..N {
        let p = r.point2(50.0);
        let c = r.point2(50.0);
        let a = r.v(-7.0, 7.0);
        for v in [p.x, p.y, c.x, c.y, a] {
            s.i(v);
        }
        // SAFETY: valid pointers, some of them equal (the C ABI allows it)
        unsafe {
            let mut o = Ge2DPoint { x: SENTINEL, y: SENTINEL };
            ee_ge_2d_rotate(&p, a, &c, &mut o);
            s.o(o.x);
            s.o(o.y);
            let mut q = p;
            let qp: *mut Ge2DPoint = &mut q;
            ee_ge_2d_rotate(qp, a, &c, qp);
            s.o(q.x);
            s.o(q.y);
            let mut rr = p;
            let rp: *mut Ge2DPoint = &mut rr;
            ee_ge_2d_rotate(rp, a, rp, rp);
            s.o(rr.x);
            s.o(rr.y);
            let mut w = p;
            let wp: *mut Ge2DPoint = &mut w;
            ee_ge_2d_rotate(&p, a, wp, wp);
            s.o(w.x);
            s.o(w.y);
        }
        s.end();
    }
    all.push(s);

    unsafe extern "C" fn push2(context: *mut c_void, point: *const Ge2DPoint) {
        // SAFETY: the context is the test's Vec, the point valid for the call
        unsafe { (*(context as *mut Vec<Ge2DPoint>)).push(*point) };
    }
    unsafe extern "C" fn push3(context: *mut c_void, point: *const Ge3DPoint) {
        // SAFETY: as push2
        unsafe { (*(context as *mut Vec<Ge3DPoint>)).push(*point) };
    }
    unsafe extern "C" fn push_rect(context: *mut c_void, rectangle: *const GeRectangle) {
        // SAFETY: as push2
        unsafe { (*(context as *mut Vec<GeRectangle>)).push(*rectangle) };
    }

    let mut s = Section::new("2d_interpolate");
    for _ in 0..5000 {
        let p = r.point2(20.0);
        let mut q = r.point2(20.0);
        // (a point within 0.0001 of p but not equal reads an uninitialized end in the C++: not drawn)
        if r.next() & 15 == 0 {
            q.x = p.x + 0.00005;
        }
        if r.next() & 7 == 0 {
            q.x = p.x;
        }
        if r.next() & 7 == 0 {
            q = p;
        }
        let step = 0.05 + r.unit() * 4.95; // no specials: a step of 0 or below never ends
        for v in [p.x, p.y, q.x, q.y, step] {
            s.i(v);
        }
        let mut out: Vec<Ge2DPoint> = Vec::new();
        // SAFETY: valid pointers; the callback pushes onto `out`
        unsafe { ee_ge_2d_interpolate(&p, &q, step, push2, &mut out as *mut _ as *mut c_void) };
        s.ou(out.len() as u32);
        for e in out {
            s.o(e.x);
            s.o(e.y);
        }
        s.end();
    }
    all.push(s);

    // ---- GEVector
    let mut s = Section::new("vec_cross");
    for _ in 0..N {
        let a = r.vector(10.0);
        let b = r.vector(10.0);
        s.iv(a);
        s.iv(b);
        // SAFETY: valid pointers, some of them equal (the C ABI allows it)
        unsafe {
            let mut o = GeVector { x: SENTINEL, y: SENTINEL, z: SENTINEL };
            ee_ge_vector_cross(&a, &b, &mut o);
            s.ov(o);
            let mut a1 = a;
            let a1p: *mut GeVector = &mut a1;
            ee_ge_vector_cross(a1p, &b, a1p);
            s.ov(a1);
            let mut b1 = b;
            let b1p: *mut GeVector = &mut b1;
            ee_ge_vector_cross(&a, b1p, b1p);
            s.ov(b1);
            let mut a2 = a;
            let a2p: *mut GeVector = &mut a2;
            ee_ge_vector_cross(a2p, a2p, a2p);
            s.ov(a2);
            s.o(ee_ge_vector_magnitude(&a));
            s.o(ee_ge_vector_magnitude_squared(&a));
        }
        s.end();
    }
    all.push(s);

    let mut table = vec![GeVector::default(); QUANT_COUNT];
    // SAFETY: a buffer of the table's size
    unsafe { ee_ge_vector_quantized_table(table.as_mut_ptr(), QUANT_COUNT) };
    let mut s = Section::new("vec_quant_table");
    for (i, v) in table.iter().enumerate() {
        s.iu(i as u32);
        s.ov(*v);
        s.end();
    }
    all.push(s);

    let mut s = Section::new("vec_quant");
    for _ in 0..100_000 {
        let mut a = r.vector(5.0);
        if r.next() & 31 == 0 {
            a = GeVector::default();
        }
        if r.next() & 15 == 0 {
            a.z = 0.0;
        }
        if r.next() & 15 == 0 {
            a.x = 0.0;
        }
        s.iv(a);
        // SAFETY: a valid pointer
        s.ou(unsafe { ee_ge_vector_quantized_index(&a) } as u32);
        s.end();
    }
    all.push(s);

    // ---- GE3DPoint
    let mut s = Section::new("3d_distance");
    for _ in 0..N {
        let a = r.point(500.0);
        let b = r.point(500.0);
        s.ip(a);
        s.ip(b);
        // SAFETY: valid pointers
        unsafe {
            s.o(ee_ge_3d_distance(&a, &b));
            s.o(ee_ge_3d_distance_squared(&a, &b));
        }
        s.end();
    }
    all.push(s);

    let mut s = Section::new("3d_interpolate");
    for _ in 0..5000 {
        let a = r.point(20.0);
        let mut b = r.point(20.0);
        if r.next() & 7 == 0 {
            b = a;
        }
        if r.next() & 7 == 0 {
            b.x = a.x;
        }
        if r.next() & 7 == 0 {
            b.y = a.y;
        }
        let step = 0.05 + r.unit() * 4.95; // no specials: a step of 0 or below never ends
        s.ip(a);
        s.ip(b);
        s.i(step);
        let mut out: Vec<Ge3DPoint> = Vec::new();
        // SAFETY: valid pointers; the callback pushes onto `out`
        unsafe { ee_ge_3d_interpolate(&a, &b, step, push3, &mut out as *mut _ as *mut c_void) };
        s.ou(out.len() as u32);
        for e in out {
            s.op(e);
        }
        s.end();
    }
    all.push(s);

    // ---- GE3DLine
    let mut s = Section::new("line_point");
    for _ in 0..N {
        let start = r.point(20.0);
        let direction = r.vector(3.0);
        let l = Ge3DLine { start, direction };
        let p = r.point(20.0);
        let tol = r.v(0.0, 30.0);
        s.ip(start);
        s.iv(direction);
        s.ip(p);
        s.i(tol);
        let mut t = SENTINEL;
        // SAFETY: valid pointers
        s.ou(unsafe { ee_ge_line_intersects_point(&l, &p, tol, &mut t) } as u32);
        s.o(t);
        s.end();
    }
    all.push(s);

    let mut s = Section::new("line_line");
    for _ in 0..N {
        let a_start = r.point(10.0);
        let mut a_dir = r.vector(3.0);
        let mut b_start = r.point(10.0);
        let mut b_dir = r.vector(3.0);
        if r.next() & 3 == 0 {
            a_dir.z = 0.0;
            b_dir.z = 0.0;
        }
        if r.next() & 7 == 0 {
            b_dir = a_dir;
        }
        if r.next() & 7 == 0 {
            b_start = a_start;
        }
        let a = Ge3DLine { start: a_start, direction: a_dir };
        let b = Ge3DLine { start: b_start, direction: b_dir };
        s.ip(a_start);
        s.iv(a_dir);
        s.ip(b_start);
        s.iv(b_dir);
        // SAFETY: valid pointers
        unsafe {
            let (mut t, mut u) = (SENTINEL, SENTINEL);
            s.ou(ee_ge_line_intersects_line(&a, &b, &mut t, &mut u) as u32);
            s.o(t);
            s.o(u);
            let (mut t, mut u) = (SENTINEL, SENTINEL);
            s.ou(ee_ge_line_intersects_2d(&a, &b, &mut t, &mut u) as u32);
            s.o(t);
            s.o(u);
        }
        s.end();
    }
    all.push(s);

    let mut s = Section::new("line_plane");
    for _ in 0..N {
        let start = r.point(20.0);
        let mut direction = r.vector(3.0);
        let pp = r.point(20.0);
        let mut pn = r.vector(1.0);
        if r.next() & 7 == 0 {
            direction.z = 0.0;
            pn.z = 0.0;
        }
        let l = Ge3DLine { start, direction };
        let pl = Ge3DPlane { point: pp, normal: pn };
        s.ip(start);
        s.iv(direction);
        s.ip(pp);
        s.iv(pn);
        let mut t = SENTINEL;
        // SAFETY: valid pointers
        s.ou(unsafe { ee_ge_line_intersects_plane(&l, &pl, &mut t) } as u32);
        s.o(t);
        let x = r.v(-20.0, 20.0);
        s.i(x);
        let mut o = Ge3DPoint { x: SENTINEL, y: SENTINEL, z: SENTINEL };
        for axis in 0..3 {
            let mut t = SENTINEL;
            // SAFETY: valid pointers (a null point asks for the parameter only)
            s.ou(unsafe { ee_ge_line_solve(&l, axis, x, &mut t, std::ptr::null_mut()) } as u32);
            s.o(t);
            let mut t = SENTINEL;
            // SAFETY: valid pointers
            s.ou(unsafe { ee_ge_line_solve(&l, axis, x, &mut t, &mut o) } as u32);
            s.o(t);
            s.op(o);
        }
        s.end();
    }
    all.push(s);

    // ---- GE3DPlane
    let mut s = Section::new("plane");
    for _ in 0..N {
        let a = r.point(20.0);
        let b = r.point(20.0);
        let mut c = r.point(20.0);
        if r.next() & 15 == 0 {
            c = b;
        }
        s.ip(a);
        s.ip(b);
        s.ip(c);
        let mut p = Ge3DPlane::default();
        // SAFETY: valid pointers
        unsafe { ee_ge_plane_define(&a, &b, &c, &mut p) };
        s.op(p.point);
        s.ov(p.normal);
        let qp = r.point(20.0);
        let mut qn = r.vector(1.0);
        if r.next() & 7 == 0 {
            qn = p.normal;
        }
        let q = Ge3DPlane { point: qp, normal: qn };
        s.ip(qp);
        s.iv(qn);
        let mut l = Ge3DLine {
            start: Ge3DPoint { x: SENTINEL, y: SENTINEL, z: SENTINEL },
            direction: GeVector { x: SENTINEL, y: SENTINEL, z: SENTINEL },
        };
        // SAFETY: valid pointers
        s.ou(unsafe { ee_ge_plane_intersects(&p, &q, &mut l) } as u32);
        s.op(l.start);
        s.ov(l.direction);
        let mut x = r.point(20.0);
        if r.next() & 7 == 0 {
            x = a;
        }
        s.ip(x);
        // SAFETY: valid pointers
        unsafe {
            for test in 0..5 {
                s.ou(ee_ge_plane_side(&q, &x, test) as u32);
            }
            s.ou(ee_ge_plane_side(&p, &x, 4) as u32);
        }
        s.end();
    }
    all.push(s);

    // ---- GETransformation
    let mut s = Section::new("t_apply");
    for _ in 0..N {
        let a = r.transformation();
        let b = r.transformation();
        s.im(&a);
        s.im(&b);
        // SAFETY: valid pointers, some of them equal (the C ABI allows it)
        unsafe {
            let mut o = GeTransformation::default();
            ee_ge_t_compose(&a, &b, &mut o);
            s.om(&o);
            let mut a1 = a;
            let a1p: *mut GeTransformation = &mut a1;
            ee_ge_t_compose(a1p, &b, a1p);
            s.om(&a1);
            let mut b1 = b;
            let b1p: *mut GeTransformation = &mut b1;
            ee_ge_t_compose(&a, b1p, b1p);
            s.om(&b1);
        }
        let ls = r.point(20.0);
        let ld = r.vector(3.0);
        let l = Ge3DLine { start: ls, direction: ld };
        s.ip(ls);
        s.iv(ld);
        let mut lo = Ge3DLine::default();
        // SAFETY: valid pointers
        unsafe { ee_ge_t_apply_line(&a, &l, &mut lo) };
        s.op(lo.start);
        s.ov(lo.direction);
        let pp = r.point(20.0);
        let pn = r.vector(1.0);
        let p = Ge3DPlane { point: pp, normal: pn };
        s.ip(pp);
        s.iv(pn);
        let mut po = Ge3DPlane::default();
        // SAFETY: valid pointers
        unsafe { ee_ge_t_apply_plane(&a, &p, &mut po) };
        s.op(po.point);
        s.ov(po.normal);
        s.end();
    }
    all.push(s);

    let mut s = Section::new("t_angles");
    for _ in 0..N {
        let a = r.transformation();
        s.im(&a);
        let mut o = GeOrientation { yaw: SENTINEL, pitch: SENTINEL, roll: SENTINEL };
        // SAFETY: valid pointers
        unsafe {
            ee_ge_t_orientation(&a, &mut o);
            s.o(o.yaw);
            s.o(o.pitch);
            s.o(o.roll);
            s.o(ee_ge_t_yaw(&a));
            s.o(ee_ge_t_pitch(&a));
            s.o(ee_ge_t_roll(&a));
        }
        let p = r.point(500.0);
        let b = r.transformation();
        s.ip(p);
        s.im(&b);
        // SAFETY: valid pointers
        unsafe {
            s.o(ee_ge_t_distance_to_point(&a, &p));
            s.o(ee_ge_t_distance_to(&a, &b));
            s.o(ee_ge_t_distance_squared_to_point(&a, &p));
            s.o(ee_ge_t_distance_squared_to(&a, &b));
        }
        s.end();
    }
    all.push(s);

    let mut s = Section::new("t_inverse");
    for _ in 0..N {
        let a = r.transformation();
        s.im(&a);
        // SAFETY: valid pointers, two of them equal (the C ABI allows it)
        unsafe {
            let mut o = GeTransformation::default();
            ee_ge_t_make_inverse(&a, &mut o);
            s.om(&o);
            let mut a1 = a;
            let a1p: *mut GeTransformation = &mut a1;
            ee_ge_t_make_inverse(a1p, a1p);
            s.om(&a1);
            let mut n = a;
            ee_ge_t_normalize(&mut n);
            s.om(&n);
        }
        s.end();
    }
    all.push(s);

    let mut s = Section::new("t_lookat");
    for _ in 0..N {
        let e = r.point(50.0);
        let mut t = r.point(50.0);
        let k = r.next() & 7;
        if k == 0 {
            t.x = e.x;
            t.y = e.y;
        }
        if k == 1 {
            t.x = e.x + 0.00001;
            t.y = e.y;
        }
        if k == 2 {
            t = e;
        }
        s.ip(e);
        s.ip(t);
        let mut m = r.matrix(1.0);
        s.im(&m);
        // SAFETY: valid pointers
        unsafe { ee_ge_t_make_look_at(&mut m, &e, &t) };
        s.om(&m);
        s.end();
    }
    all.push(s);

    let mut s = Section::new("t_rotate");
    for _ in 0..N {
        let a = r.transformation();
        let y = r.v(-7.0, 7.0);
        let p = r.v(-7.0, 7.0);
        let rr = r.v(-7.0, 7.0);
        s.im(&a);
        s.i(y);
        s.i(p);
        s.i(rr);
        // SAFETY: valid pointers
        unsafe {
            let mut b = a;
            ee_ge_t_revolve(&mut b, y, p, rr);
            s.om(&b);
            b = a;
            ee_ge_t_rotate(&mut b, y, p, rr);
            s.om(&b);
            b = a;
            ee_ge_t_scale(&mut b, y, p, rr);
            s.om(&b);
            for order in 0..6 {
                b = a;
                ee_ge_t_set_orientation(&mut b, order, y, p, rr);
                s.om(&b);
            }
        }
        s.end();
    }
    all.push(s);

    // ---- GEQuaternion, GEOrientation
    let mut s = Section::new("quat");
    for _ in 0..N {
        let a = r.transformation();
        s.im(&a);
        let mut q = GeQuaternion::default();
        // SAFETY: valid pointers
        unsafe { ee_ge_quat_from(&a, &mut q) };
        s.oq(&q);
        let mut b = r.matrix(3.0);
        s.im(&b);
        // SAFETY: valid pointers
        unsafe { ee_ge_quat_to(&q, &mut b) };
        s.om(&b);
        let x = r.v(-2.0, 2.0);
        let y = r.v(-2.0, 2.0);
        let z = r.v(-2.0, 2.0);
        let w = r.v(-2.0, 2.0);
        let mut rq = GeQuaternion { x, y, z, w };
        s.i(x);
        s.i(y);
        s.i(z);
        s.i(w);
        let mut n = rq;
        // SAFETY: a valid pointer
        unsafe { ee_ge_quat_normalize(&mut n) };
        s.oq(&n);
        let t = r.v(-0.5, 1.5);
        s.i(t);
        if r.next() & 7 == 0 {
            rq = q;
        }
        // SAFETY: valid pointers, two of them equal in the last call (the C ABI allows it)
        unsafe {
            let mut o = GeQuaternion::default();
            ee_ge_quat_slerp(&q, &n, t, &mut o);
            s.oq(&o);
            ee_ge_quat_slerp(&q, &rq, t, &mut o);
            s.oq(&o);
            let mut q1 = q;
            let q1p: *mut GeQuaternion = &mut q1;
            ee_ge_quat_slerp(q1p, &n, t, q1p);
            s.oq(&q1);
        }
        let ey = r.v(-50.0, 50.0);
        let ep = r.v(-50.0, 50.0);
        let er = r.v(-50.0, 50.0);
        let mut e = GeOrientation { yaw: ey, pitch: ep, roll: er };
        s.i(ey);
        s.i(ep);
        s.i(er);
        // SAFETY: a valid pointer
        unsafe { ee_ge_orientation_normalize(&mut e) };
        s.o(e.yaw);
        s.o(e.pitch);
        s.o(e.roll);
        s.end();
    }
    all.push(s);

    // ---- GEBoundingBox
    let bbox = |a: Ge3DPoint, b: Ge3DPoint| {
        let mut x = GeBoundingBox { bottom_left_front: a, top_right_back: b, ..GeBoundingBox::default() };
        // SAFETY: a valid pointer
        unsafe { ee_ge_bbox_compute_dimensions(&mut x) };
        x
    };
    let mut s = Section::new("bbox");
    for _ in 0..N {
        let a = r.point(20.0);
        let b = r.point(20.0);
        s.ip(a);
        s.ip(b);
        let x = bbox(a, b);
        s.op(x.bottom_left_front);
        s.op(x.top_right_back);
        s.o(x.width);
        s.o(x.height);
        s.o(x.depth);
        let ya = r.point(20.0);
        let yb = r.point(20.0);
        let y = bbox(ya, yb);
        s.ip(ya);
        s.ip(yb);
        let mut p = r.point(22.0);
        if r.next() & 7 == 0 {
            p = x.top_right_back;
        }
        s.ip(p);
        // SAFETY: valid pointers
        unsafe {
            s.ou(ee_ge_bbox_contains(&x, &p) as u32);
            s.ou(ee_ge_bbox_intersects(&x, &y) as u32);
        }
        let ls = r.point(30.0);
        let mut ld = r.vector(3.0);
        if r.next() & 3 == 0 {
            ld.y = 0.0;
        }
        let l = Ge3DLine { start: ls, direction: ld };
        s.ip(ls);
        s.iv(ld);
        let mut d = SENTINEL;
        // SAFETY: valid pointers
        s.ou(unsafe { ee_ge_bbox_intersects_line(&x, &l, &mut d) } as u32);
        s.o(d);
        let t = r.transformation();
        s.im(&t);
        let mut o = bbox(Ge3DPoint::default(), Ge3DPoint::new(1.0, 1.0, 1.0));
        // SAFETY: valid pointers
        unsafe { ee_ge_bbox_transform(&x, &t, &mut o) };
        s.op(o.bottom_left_front);
        s.op(o.top_right_back);
        s.o(o.width);
        s.o(o.height);
        s.o(o.depth);
        s.end();
    }
    all.push(s);

    // ---- GERectangle
    let mut s = Section::new("rect");
    for _ in 0..N {
        let mut v = [0.0f32; 8];
        for e in v.iter_mut() {
            *e = r.v(-20.0, 20.0);
        }
        if r.next() & 3 == 0 {
            for k in [0, 4] {
                if v[k] > v[k + 2] {
                    v.swap(k, k + 2);
                }
                if v[k + 1] > v[k + 3] {
                    v.swap(k + 1, k + 3);
                }
            }
        }
        let a = GeRectangle::new(v[0], v[1], v[2], v[3]);
        let b = GeRectangle::new(v[4], v[5], v[6], v[7]);
        for e in v {
            s.i(e);
        }
        let out_rect = |s: &mut Section, r: &GeRectangle| {
            s.o(r.left);
            s.o(r.top);
            s.o(r.right);
            s.o(r.bottom);
        };
        let mut o = GeRectangle::new(SENTINEL, SENTINEL, SENTINEL, SENTINEL);
        // SAFETY: valid pointers
        s.ou(unsafe { ee_ge_rect_and(&a, &b, &mut o) } as u32);
        out_rect(&mut s, &o);
        let mut c = a;
        // SAFETY: valid pointers
        s.ou(unsafe { ee_ge_rect_and_in_place(&mut c, &b) } as u32);
        out_rect(&mut s, &c);
        let existing = r.next() & 1;
        s.iu(existing);
        let mut pieces: Vec<GeRectangle> =
            vec![GeRectangle::new(SENTINEL, SENTINEL, SENTINEL, SENTINEL); existing as usize];
        // SAFETY: valid pointers; the callback pushes onto `pieces`
        unsafe {
            ee_ge_rect_remove_intersecting(&a, &b, pieces.len(), push_rect, &mut pieces as *mut _ as *mut c_void)
        };
        s.ou(pieces.len() as u32);
        for e in &pieces {
            out_rect(&mut s, e);
        }
        s.end();
    }
    all.push(s);

    all
}

#[test]
fn every_section_matches_the_cpp() {
    let golden = golden_sections();
    let sections = run_all();
    let mut failures = Vec::new();
    for s in &sections {
        let expected = golden.get(s.name).unwrap_or_else(|| panic!("no golden section {}", s.name));
        if &s.text() != expected {
            failures.push(format!("--- {}\nexpected:\n{}got:\n{}", s.name, expected, s.text()));
        }
    }
    assert_eq!(golden.len(), sections.len(), "the golden file has other sections");
    assert!(failures.is_empty(), "{} sections differ:\n{}", failures.len(), failures.join("\n"));
}
