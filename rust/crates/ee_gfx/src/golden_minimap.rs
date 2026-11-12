//! The mini map's arithmetic against the unmodified C++ (`golden_minimap.txt`). A scratch driver
//! (`build/gx9_golden`, `driver_minimap.cpp`; PORT_LEARNINGS "R4.4, third round") #included `EEMiniMap.cpp` of the
//! commit before this work over the real GE math and `GEViewport`, with a zeroed `EEMap` and `EEMiniMap`, a fake
//! unit surface whose vtable slots log (25 `Lock`, 16 `GetMemoryAddress`, 11 `GetBytesPerRow`) and a logging
//! `EEUIMiniMap::Update`. From an LCG seeded 1 it drew 40 maps (`SetMap`'s four transformations and pixel sizes),
//! each with 10 viewports (`RefreshView`'s trapezoid, a tenth of them with no map attached) and 4 `ProjectToGround`
//! cases per viewport, and printed per case an FNV-1a 64 of its log. This test draws the same stream, runs each
//! case through `minimap.rs` and compares the digests.

use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge2DPoint, GeRectangle, GeTransformation};
use ee_ge::viewport::GeViewport;

use crate::golden_mouse::{check, Log, Rng};
use crate::minimap::*;

const GOLDEN: &str = include_str!("golden_minimap.txt");

fn transform(g: &mut Rng) -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    let y = g.f(-3.2, 3.2);
    let p = g.f(-1.5, 1.5);
    let r = if g.b(20) { g.f(-0.4, 0.4) } else { 0.0 };
    t.set_orientation(RotationOrder::Ypr, y, p, r);
    if g.b(10) {
        let q = if g.b(50) { 1.5707964 } else { -1.5707964 };
        t.set_orientation(RotationOrder::Ypr, 0.0, q, 0.0);
    }
    let x = g.f(-4.0, 268.0);
    let yy = g.f(-4.0, 268.0);
    let z = g.f(-2.0, 60.0);
    t.m[0][3] = x;
    t.m[1][3] = yy;
    t.m[2][3] = z;
    t
}

fn viewport(g: &mut Rng, log: &mut Log, vp: &mut GeViewport) {
    let w = g.pick(&[640.0, 800.0, 1024.0, 1920.0]);
    let h = g.pick(&[480.0, 600.0, 768.0, 1080.0]);
    let top = if g.b(30) { g.f(0.0, 60.0) } else { 0.0 };
    vp.set_view_rectangle(&GeRectangle { bottom: h, left: 0.0, right: w, top });
    let fov = g.f(0.3, 1.2);
    vp.set_field_of_view(fov);
    let front = g.f(0.1, 3.0);
    vp.set_front_clipping_distance(front);
    let rear = g.f(30.0, 190.0);
    vp.set_rear_clipping_distance(rear);
    let ow = g.f(10.0, 120.0);
    let oh = g.f(10.0, 90.0);
    vp.set_orthographic_rectangle(&GeRectangle { bottom: oh, left: -ow, right: ow, top: -oh });
    let camera = transform(g);
    vp.set_camera(&camera);
    log.t("vp");
    log.i(w as i64);
    log.i(h as i64);
    log.h(top);
}

fn log_t(log: &mut Log, t: &GeTransformation) {
    for r in 0..4 {
        for k in 0..4 {
            log.h(t.m[r][k]);
        }
    }
}

#[test]
fn minimap_matches_the_cpp_golden() {
    let mut golden = GOLDEN.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    let mut vp = GeViewport::default();
    let mut n = 0i64;
    let mut corners = [Ge2DPoint::default(); 4];
    let sides = [64.0, 128.0, 144.0, 160.0, 200.0, 256.0, 1.0, 40.0];
    for _ in 0..40 {
        let h = if g.b(60) { g.pick(&sides) as i32 } else { (1 + g.r() % 400) as i32 };
        let w = if g.b(50) {
            h
        } else if g.b(60) {
            g.pick(&sides) as i32
        } else {
            (1 + g.r() % 400) as i32
        };
        let size = if g.b(70) { h.max(w) } else { (g.r() % 420) as i32 };
        log.t("in");
        log.i(h as i64);
        log.i(w as i64);
        log.i(size as i64);
        let t = transforms(h, w, size);
        // the C++ glue: the grid's update, the unit surface locked
        log.t("uiupdate S25 S16 S11");
        log.t("out");
        log_t(&mut log, &t.map_to_pixel);
        log_t(&mut log, &t.pixel_to_map);
        log_t(&mut log, &t.map_to_cell);
        log_t(&mut log, &t.cell_to_map);
        log.i(t.pixel_width as i64);
        log.i(t.pixel_height as i64);
        check(&mut golden, "setmap", n, &mut log);
        n += 1;
        for _ in 0..10 {
            viewport(&mut g, &mut log, &mut vp);
            let ortho = g.b(20);
            log.i(ortho as i64);
            if g.b(90) {
                corners = view_trapezoid(&vp, ortho, &t.map_to_pixel).corners;
            }
            log.t("out");
            for c in &corners {
                log.p2(*c);
            }
            check(&mut golden, "refresh", n, &mut log);
            n += 1;
            for _ in 0..4 {
                let x = g.f(-100.0, 2000.0);
                let y = g.f(-100.0, 1200.0);
                let z = if g.b(50) { 0.0 } else { g.f(-5.0, 30.0) };
                let ortho = g.b(20);
                let out = project_to_ground(&vp, ortho, Ge2DPoint { x, y }, z);
                log.t("in");
                log.h(x);
                log.h(y);
                log.h(z);
                log.i(ortho as i64);
                log.t("out");
                log.p2(out);
                check(&mut golden, "project", n, &mut log);
                n += 1;
            }
        }
    }
    assert!(golden.next().is_none(), "golden longer than the replay");
}
