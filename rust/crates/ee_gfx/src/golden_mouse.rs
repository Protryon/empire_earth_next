//! The pointer's arithmetic against the unmodified C++ (`golden_mouse.txt`). A scratch driver
//! (`build/gfx8_golden`, `driver_mouse.cpp`; PORT_LEARNINGS "R4.4, second round") compiled `RTSMouse.cpp` of the
//! commit before this work with the real `GE2DPoint`/`GERectangle` over a fake mouse device (its axes and wheel
//! return the case's values, each read logged) and a logging `ESDebugOut`. From an LCG seeded 1 it drew 1,500
//! ClampPosition cases (directly, through the inline `SetBounds` and `SetPosition`) and 2,500 `Update` cases (moves
//! from large to small, frozen, no wheel), the unit's smallest-move statics running on across them, and printed
//! per case an FNV-1a 64 of its log. This test draws the same stream, runs each case through `mouse.rs` and the
//! C++ glue rewritten here (the reads in their order, the debug lines, the wheel) and compares the digests.

use std::fmt::Write as _;

use ee_ge::types::{Ge2DPoint, GeRectangle};

use crate::mouse::*;

const GOLDEN: &str = include_str!("golden_mouse.txt");

pub(crate) struct Rng(pub u32);
impl Rng {
    pub fn r(&mut self) -> u32 {
        self.0 = self.0.wrapping_mul(1664525).wrapping_add(1013904223);
        self.0 >> 8
    }
    pub fn f(&mut self, lo: f32, hi: f32) -> f32 {
        lo + (hi - lo) * ((self.r() & 0xffff) as f32) / 65535.0
    }
    pub fn b(&mut self, pct: u32) -> bool {
        self.r() % 100 < pct
    }
    pub fn pick(&mut self, v: &[f32]) -> f32 {
        v[(self.r() % v.len() as u32) as usize]
    }
}

#[derive(Default)]
pub(crate) struct Log(pub String);
impl Log {
    pub fn h(&mut self, f: f32) {
        write!(self.0, " {:08x}", f.to_bits()).unwrap();
    }
    pub fn i(&mut self, v: i64) {
        write!(self.0, " {v}").unwrap();
    }
    pub fn t(&mut self, t: &str) {
        self.0.push(' ');
        self.0.push_str(t);
    }
    pub fn p2(&mut self, p: Ge2DPoint) {
        self.h(p.x);
        self.h(p.y);
    }
    pub fn rect(&mut self, r: &GeRectangle) {
        self.h(r.left);
        self.h(r.top);
        self.h(r.right);
        self.h(r.bottom);
    }
}

pub(crate) fn digest(line: &str) -> u64 {
    let mut h: u64 = 0xcbf29ce484222325;
    for b in line.bytes() {
        h ^= b as u64;
        h = h.wrapping_mul(0x100000001b3);
    }
    h
}

/// Compares one case's log with the golden's next line.
pub(crate) fn check(golden: &mut std::str::Lines, kind: &str, n: i64, log: &mut Log) {
    let line = format!("{kind} {n}{}", log.0);
    log.0.clear();
    let want = golden.next().expect("golden too short");
    assert_eq!(format!("{kind} {n} {:016x}", digest(&line)), want, "case {kind} {n}: the replay's log is\n{line}");
}

fn coord(g: &mut Rng, lo: f32, hi: f32, edge0: f32, edge1: f32) -> f32 {
    match g.r() % 10 {
        0 => edge0,
        1 => edge1,
        2 => edge1 - 1.0,
        3 => edge1 - 0.5,
        4 => edge0 - 0.25,
        _ => g.f(lo, hi),
    }
}

fn random_bounds(g: &mut Rng) -> GeRectangle {
    let l = if g.b(50) { 0.0 } else { g.f(-50.0, 50.0) };
    let t = if g.b(50) { 0.0 } else { g.f(-50.0, 50.0) };
    let w = [800.0, 1024.0, 1920.0, 0.0, 1.0, 2.5];
    let rw = if g.b(60) { g.pick(&w) } else { g.f(-10.0, 2000.0) };
    let rh = if g.b(60) { g.pick(&w) } else { g.f(-10.0, 1500.0) };
    GeRectangle { left: l, top: t, right: l + rw, bottom: t + rh }
}

fn random_point(g: &mut Rng, r: &GeRectangle) -> Ge2DPoint {
    let x = coord(g, r.left - 200.0, r.right + 200.0, r.left, r.right);
    let y = coord(g, r.top - 200.0, r.bottom + 200.0, r.top, r.bottom);
    Ge2DPoint { x, y }
}

fn random_move(g: &mut Rng, i: i64) -> i32 {
    if g.b(15) {
        return 0;
    }
    let floor = (100 - i / 3).max(1);
    let mut m = floor + (g.r() % 200) as i64;
    if i > 300 && g.b(10) {
        m = 1 + (g.r() % 3) as i64;
    }
    (if g.b(50) { -m } else { m }) as i32
}

#[test]
fn mouse_matches_the_cpp_golden() {
    let mut golden = GOLDEN.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    let mut n = 0i64;
    for _ in 0..1500 {
        let r = random_bounds(&mut g);
        let p = random_point(&mut g, &r);
        let how = g.r() % 3;
        log.t("in");
        log.i(how as i64);
        log.rect(&r);
        log.p2(p);
        let out = clamp_position(p, &r);
        log.t("out");
        log.p2(out);
        log.rect(&r);
        check(&mut golden, "clamp", n, &mut log);
        n += 1;
    }
    let (mut min_x, mut min_y) = (100i32, 100i32);
    for i in 0..2500i64 {
        let mut r = random_bounds(&mut g);
        if g.b(70) {
            r.left = 0.0;
            r.top = 0.0;
            r.right = g.f(320.0, 2560.0);
            r.bottom = g.f(240.0, 1600.0);
        }
        let p = random_point(&mut g, &r);
        let d = [1920.0, 1080.0, 2560.0, 1440.0, 800.0, 600.0, 1.0, 3840.0];
        let dw = g.pick(&d) as u32;
        let dh = g.pick(&d) as u32;
        let sens = if g.b(60) { g.pick(&[1.0, 0.5, 2.0, 0.0]) } else { g.f(0.05, 4.0) };
        let frozen = g.b(15);
        let ax = random_move(&mut g, i);
        let ay = random_move(&mut g, i);
        let az = (g.r() % 7) as i32 - 3;
        let has_wheel = !g.b(10);
        log.t("in");
        log.rect(&r);
        log.p2(p);
        log.i(dw as i64);
        log.i(dh as i64);
        log.h(sens);
        log.i(frozen as i64);
        log.i(ax as i64);
        log.i(ay as i64);
        log.i(az as i64);
        log.i(has_wheel as i64);
        // RTSMouse::Update's glue
        let mut position = p;
        if !frozen {
            log.t("N 0");
            log.t("N 1");
            let input = EeGfxMouseInput {
                position: p,
                bounds: r,
                desktop_width: dw,
                desktop_height: dh,
                sensitivity: sens,
                move_x: ax,
                move_y: ay,
            };
            let m = update_move(&input, &mut min_x, &mut min_y);
            if m.min_x_changed {
                log.t(&format!("D!MinXValue_now_{min_x}_"));
            }
            if m.min_y_changed {
                log.t(&format!("D!MinYValue_now_{min_y}_"));
            }
            if m.moved {
                position = m.position;
            }
        }
        let wheel = if has_wheel {
            log.t("V 2");
            az as i64 * 3 + 1
        } else {
            0
        };
        log.t("out");
        log.p2(position);
        log.i(wheel);
        check(&mut golden, "update", n, &mut log);
        n += 1;
    }
    assert!(golden.next().is_none(), "golden longer than the replay");
}
