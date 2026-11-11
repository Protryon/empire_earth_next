//! The units' render object's geometry against the unmodified C++ (`golden_rounits.txt`). A scratch driver
//! (`build/gfx8_golden`, `driver_rounits.cpp`; PORT_LEARNINGS "R4.4, second round") #included `EEROUnits.cpp` of
//! the commit before this work, over the real GE math and `GEViewport`, a fake rasterizer and fake vertex arrays
//! whose virtual slots log (slot 43 `SetBlending`, 45 `SetCulling`, 17 `Draw(GERhombus*)`, a vertex array's 9
//! `SetColor` with its colour and index) and a `GERhombus::ComputeRotation` that logs the rectangle just set. From an
//! LCG seeded 1 it drew 24 viewports with 60 `GetScreenPoint` cases (perspective and orthographic), 20 `DrawBars`
//! sizes, 60 `DrawUnitBars` (a `DrawBar` per row) and 40 `DrawObjectBars` each, and printed per case an FNV-1a 64 of
//! its log. This test draws the same stream, runs each case through `units.rs` and the C++ glue rewritten here (the
//! rhombi's rectangles, colours and draws in the original's order) and compares the digests.

use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge2DPoint, Ge3DPoint, GeRectangle, GeTransformation};
use ee_ge::viewport::GeViewport;

use crate::golden_mouse::{check, Log, Rng};
use crate::units::*;

const GOLDEN: &str = include_str!("golden_rounits.txt");

const HEALTH: [f32; 3] = [0.0, 0.0, 0.7];
const HEALTH_BACK: [f32; 3] = [1.0, 1.0, 1.0];
const BAR: [f32; 3] = [0.0, 1.0, 0.0];
const BAR_BACK: [f32; 3] = [1.0, 0.0, 0.0];
const BORDER: [f32; 3] = [0.9, 0.9, 0.9];

struct Case {
    g: Rng,
    log: Log,
    vp: GeViewport,
    sizes: EeGfxBarSizes,
    /// gOrthographicView: the last screen case's (the bars project with it too)
    ortho: bool,
}

impl Case {
    fn p3(&mut self, lx: f32, hx: f32, ly: f32, hy: f32, lz: f32, hz: f32) -> Ge3DPoint {
        let x = self.g.f(lx, hx);
        let y = self.g.f(ly, hy);
        let z = self.g.f(lz, hz);
        Ge3DPoint::new(x, y, z)
    }
    fn transform(&mut self) -> GeTransformation {
        let mut t = GeTransformation::IDENTITY;
        let y = self.g.f(-3.2, 3.2);
        let p = self.g.f(-1.5, 1.5);
        let r = if self.g.b(20) { self.g.f(-0.4, 0.4) } else { 0.0 };
        t.set_orientation(RotationOrder::Ypr, y, p, r);
        if self.g.b(10) {
            let p = if self.g.b(50) { 1.5707964 } else { -1.5707964 };
            t.set_orientation(RotationOrder::Ypr, 0.0, p, 0.0);
        }
        let pos = self.p3(-4.0, 68.0, -4.0, 68.0, -2.0, 30.0);
        t.m[0][3] = pos.x;
        t.m[1][3] = pos.y;
        t.m[2][3] = pos.z;
        t
    }
    fn viewport(&mut self) {
        let w = self.g.pick(&[640.0, 800.0, 1024.0, 1920.0]);
        let h = self.g.pick(&[480.0, 600.0, 768.0, 1080.0]);
        let top = if self.g.b(30) { self.g.f(0.0, 60.0) } else { 0.0 };
        self.vp.set_view_rectangle(&GeRectangle { bottom: h, left: 0.0, right: w, top });
        let fov = self.g.f(0.3, 1.2);
        self.vp.set_field_of_view(fov);
        let front = self.g.f(0.1, 3.0);
        self.vp.set_front_clipping_distance(front);
        let rear = self.g.f(30.0, 90.0);
        self.vp.set_rear_clipping_distance(rear);
        let ow = self.g.f(10.0, 120.0);
        let oh = self.g.f(10.0, 90.0);
        self.vp.set_orthographic_rectangle(&GeRectangle { bottom: oh, left: -ow, right: ow, top: -oh });
        let camera = self.transform();
        self.vp.set_camera(&camera);
        self.log.t("vp");
        self.log.i(w as i64);
        self.log.i(h as i64);
        self.log.h(top);
    }
    fn fraction(&mut self) -> f32 {
        if self.g.b(50) {
            self.g.pick(&[0.0, 1.0, -1.0, 0.5, 1.5, 0.001, 0.999, 0.04])
        } else {
            self.g.f(-0.2, 1.2)
        }
    }
    fn window(&mut self) -> (u32, u32) {
        let ws = [640.0, 800.0, 1024.0, 1920.0, 2560.0, 3840.0, 1.0];
        let w = if self.g.b(70) { self.g.pick(&ws) as u32 } else { self.g.r() % 5000 };
        let h = if self.g.b(70) { self.g.pick(&ws) as u32 } else { self.g.r() % 3000 };
        (w, h)
    }
    fn p(&mut self, p: Ge3DPoint) {
        self.log.h(p.x);
        self.log.h(p.y);
        self.log.h(p.z);
    }
    fn sizes_in(&mut self) {
        self.log.i(self.sizes.width as i64);
        self.log.h(self.sizes.height);
        self.log.h(self.sizes.step);
    }
    // DrawBars with empty lists: the state calls, then the sizes
    fn draw_bars(&mut self, w: u32, h: u32) {
        self.log.t("R43 R45 white");
        self.sizes = bar_sizes(w, h);
    }
    // the rhombus glue: SetRectangle (ComputeRotation logs), SetColorPerVertex, Draw
    fn rect(&mut self, who: &str, r: &GeRectangle) {
        self.log.t(who);
        self.log.t("rect");
        self.log.h(r.left);
        self.log.h(r.top);
        self.log.h(r.right);
        self.log.h(r.bottom);
    }
    fn colours(&mut self, who: &str, c: [[f32; 3]; 4]) {
        let array = if who == "fg" { 0 } else { 1 };
        for (k, c) in c.iter().enumerate() {
            self.log.t(&format!("V{array}.9"));
            self.log.h(c[0]);
            self.log.h(c[1]);
            self.log.h(c[2]);
            self.log.i(k as i64);
        }
    }
    fn draw(&mut self, who: &str) {
        self.log.t("R17");
        self.log.t(who);
    }
    // DrawBar's glue over units.rs
    #[allow(clippy::too_many_arguments)]
    fn draw_bar(
        &mut self,
        t: &GeTransformation,
        bar: Ge3DPoint,
        scale: f32,
        c: [f32; 3],
        back: [f32; 3],
        f: f32,
        row: i32,
    ) {
        let world = unit_bar_point(t, bar, scale);
        let screen = screen_point(&self.vp, self.ortho, world);
        let origin = bar_origin(screen, row, &self.sizes);
        let b = unit_bar(origin, f, &self.sizes);
        let r = b.rects;
        match b.kind {
            0 => {
                self.rect("fg", &r[0]);
                self.colours("fg", [c, BORDER, c, c]);
                self.draw("fg");
                self.rect("bg", &r[1]);
                self.colours("bg", [BORDER, c, c, c]);
                self.draw("bg");
            }
            1 => {
                self.rect("fg", &r[0]);
                self.colours("fg", [c, BORDER, c, c]);
                self.rect("bg", &r[1]);
                self.colours("bg", [BORDER, c, c, c]);
                self.draw("bg");
                self.draw("fg");
                self.rect("fg", &r[2]);
                self.colours("fg", [back, BORDER, back, back]);
                self.rect("bg", &r[3]);
                self.colours("bg", [BORDER, back, back, back]);
                self.draw("bg");
                self.draw("fg");
            }
            _ => {
                self.rect("fg", &r[0]);
                self.colours("fg", [back, BORDER, back, back]);
                self.draw("fg");
                self.rect("bg", &r[1]);
                self.colours("bg", [BORDER, back, back, back]);
                self.draw("bg");
            }
        }
    }
}

#[test]
fn rounits_match_the_cpp_golden() {
    let mut golden = GOLDEN.lines();
    let mut c = Case {
        g: Rng(1),
        log: Log::default(),
        vp: GeViewport::default(),
        sizes: EeGfxBarSizes::default(),
        ortho: false,
    };
    let mut n = 0i64;
    let mut bar = Ge3DPoint::default();
    for _ in 0..24 {
        c.viewport();
        check(&mut golden, "viewport", n, &mut c.log);
        n += 1;
        for _ in 0..60 {
            let ortho = c.g.b(30);
            c.ortho = ortho;
            let p = if c.g.b(10) {
                Ge3DPoint::new(c.vp.camera.m[0][3], c.vp.camera.m[1][3], c.vp.camera.m[2][3])
            } else {
                c.p3(-10.0, 74.0, -10.0, 74.0, -5.0, 20.0)
            };
            c.log.t("in");
            c.log.i(ortho as i64);
            c.p(p);
            let s: Ge2DPoint = screen_point(&c.vp, ortho, p);
            c.log.t("out");
            c.log.h(s.x);
            c.log.h(s.y);
            check(&mut golden, "screen", n, &mut c.log);
            n += 1;
        }
        for _ in 0..20 {
            let (w, h) = c.window();
            c.log.t("in");
            c.log.i(w as i64);
            c.log.i(h as i64);
            c.draw_bars(w, h);
            c.log.t("out");
            c.sizes_in();
            check(&mut golden, "sizes", n, &mut c.log);
            n += 1;
        }
        for _ in 0..60 {
            if c.g.b(30) {
                let (w, h) = c.window();
                c.draw_bars(w, h);
            }
            let t = c.transform();
            bar = c.p3(-2.0, 2.0, -2.0, 2.0, -1.0, 4.0);
            let scale = if c.g.b(20) { 1.0 } else { c.g.f(0.1, 3.0) };
            let health = c.fraction();
            let count = c.g.r() % 4;
            let bars: Vec<f32> = (0..count).map(|_| c.fraction()).collect();
            c.log.t("in");
            c.sizes_in();
            for r in 0..4 {
                for k in 0..4 {
                    c.log.h(t.m[r][k]);
                }
            }
            c.p(bar);
            c.log.h(scale);
            c.log.h(health);
            for f in &bars {
                c.log.h(*f);
            }
            // DrawUnitBars' glue
            let mut row = 0u32;
            if health >= 0.0 {
                row = 1;
                c.draw_bar(&t, bar, scale, HEALTH, HEALTH_BACK, health, 0);
            }
            for f in &bars {
                c.draw_bar(&t, bar, scale, BAR, BAR_BACK, *f, row as i32);
                row += 1;
            }
            c.log.t("out");
            c.log.i((row as f32 * 5.0) as i64);
            check(&mut golden, "unitbars", n, &mut c.log);
            n += 1;
        }
        for _ in 0..40 {
            let location = c.p3(-4.0, 68.0, -4.0, 68.0, -1.0, 8.0);
            bar = c.p3(-2.0, 2.0, -2.0, 2.0, -1.0, 4.0);
            let scale = if c.g.b(20) { 1.0 } else { c.g.f(0.1, 3.0) };
            let health = c.fraction();
            c.log.t("in");
            c.sizes_in();
            c.p(location);
            c.p(bar);
            c.log.h(scale);
            c.log.h(health);
            // DrawObjectBars' glue
            c.log.t("R43");
            c.colours("fg", [BAR; 4]);
            c.colours("bg", [BAR_BACK; 4]);
            let screen = screen_point(&c.vp, c.ortho, object_bar_point(location, bar, scale));
            let b = object_bar(screen, health, &c.sizes);
            match b.kind {
                0 => {
                    c.rect("fg", &b.rects[0]);
                    c.draw("fg");
                }
                1 => {
                    c.rect("fg", &b.rects[0]);
                    c.draw("fg");
                    c.rect("bg", &b.rects[1]);
                    c.draw("bg");
                }
                _ => {
                    c.rect("bg", &b.rects[0]);
                    c.draw("bg");
                }
            }
            c.log.t("R43");
            check(&mut golden, "objectbars", n, &mut c.log);
            n += 1;
        }
    }
    let _ = bar;
    assert!(golden.next().is_none(), "golden longer than the replay");
}
