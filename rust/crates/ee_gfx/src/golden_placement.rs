//! `DrawPlacement`'s footprint edges against the unmodified C++ (`golden_placement.txt`). A scratch driver
//! (`build/gx10_golden`; PORT_LEARNINGS "R4.4, fourth round") #included `EEROUnits.cpp` of the commit before this work
//! over the real GE math and `GEViewport`. `driver_placement.cpp`: a rasterizer whose slots log (the line draw its
//! first line and count), 64 `GELine`s whose vertex arrays log every `SetPoint`, `SetColor` and
//! `SetTextureCoordinate`, a unit class answering `IsLandBuilding` (slot 21); 40 viewports with 30 footprints each
//! (sizes 0 to 11, the cells all usable, none or a random share). From an LCG seeded 1 it printed per case an FNV-1a
//! 64 of its log; this test draws the same stream, runs each case through `units.rs` and the C++ glue rewritten here
//! and compares the digests.

use ee_ge::transform::RotationOrder;
use ee_ge::types::{GeRectangle, GeTransformation};
use ee_ge::viewport::GeViewport;

use crate::golden_mouse::{check, Log, Rng};
use crate::units::*;

const PLACEMENT: &str = include_str!("golden_placement.txt");

/// The drivers' `RandomTransform`: yaw, pitch, a fifth of the time a roll, then the position.
pub(crate) fn transform(g: &mut Rng) -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    let y = g.f(-3.2, 3.2);
    let p = g.f(-1.5, 1.5);
    let r = if g.b(20) { g.f(-0.4, 0.4) } else { 0.0 };
    t.set_orientation(RotationOrder::Ypr, y, p, r);
    let x = g.f(-4.0, 74.0);
    let yy = g.f(-4.0, 74.0);
    let z = g.f(-2.0, 30.0);
    t.m[0][3] = x;
    t.m[1][3] = yy;
    t.m[2][3] = z;
    t
}

/// The drivers' `RandomViewport` (`gU4E8F08`).
pub(crate) fn viewport(g: &mut Rng, log: &mut Log, vp: &mut GeViewport) {
    let w = g.pick(&[640.0, 800.0, 1024.0, 1920.0]);
    let h = g.pick(&[480.0, 600.0, 768.0, 1080.0]);
    let top = if g.b(30) { g.f(0.0, 60.0) } else { 0.0 };
    vp.set_view_rectangle(&GeRectangle { bottom: h, left: 0.0, right: w, top });
    let fov = g.f(0.3, 1.2);
    vp.set_field_of_view(fov);
    let front = g.f(0.1, 3.0);
    vp.set_front_clipping_distance(front);
    let rear = g.f(30.0, 90.0);
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

/// A vertex array's logged call: `kind id index` and the floats.
fn vertex(log: &mut Log, kind: &str, id: usize, index: i64, values: &[f32]) {
    log.t(kind);
    log.i(id as i64);
    log.i(index);
    for &v in values {
        log.h(v);
    }
}

#[test]
fn placement_matches_the_cpp_golden() {
    let mut golden = PLACEMENT.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    let mut vp = GeViewport::default();
    let mut heights = [[0.0f32; 12]; 12];
    let mut cells = [[0u8; 12]; 12];
    let mut lines = [EeGfxPlacementLine::default(); 320];
    let mut n = 0i64;
    for _ in 0..40 {
        viewport(&mut g, &mut log, &mut vp);
        check(&mut golden, "viewport", n, &mut log);
        n += 1;
        for _ in 0..30 {
            let ortho = g.b(20);
            let land = g.b(90);
            let size = if g.b(10) { (g.r() % 3) as i32 } else { 1 + (g.r() % 11) as i32 };
            let tile_x = (g.r() % 80) as i32 - 5;
            let tile_y = (g.r() % 80) as i32 - 5;
            let u = g.f(0.0, 4.0);
            let mode = g.r() % 4;
            for x in 0..12 {
                for y in 0..12 {
                    heights[x][y] = g.f(-1.0, 8.0);
                    cells[x][y] = match mode {
                        0 => 0,
                        1 => 1,
                        _ => g.b(if mode == 2 { 30 } else { 70 }) as u8,
                    };
                }
            }
            log.t("in");
            log.i(ortho as i64);
            log.i(land as i64);
            log.i(size as i64);
            log.i(tile_x as i64);
            log.i(tile_y as i64);
            log.h(u);
            log.t("K21");
            if land {
                log.t("white fogoff R59 1 0 R62 p 0 R64 2 0 R65 1 0");
                let count = placement_lines(&vp, ortho, size, tile_x, tile_y, &heights, &cells, &mut lines);
                let mut batch = 0usize;
                for l in &lines[..count] {
                    let colour = if l.back != 0 { [1.0, 0.0, 0.0] } else { [0.0, 1.0, 0.0] };
                    vertex(&mut log, "T", batch, 0, &[u, 0.0]);
                    vertex(&mut log, "T", batch, 1, &[u + 3.0, 0.0]);
                    vertex(&mut log, "C", batch, 0, &colour);
                    vertex(&mut log, "C", batch, 1, &colour);
                    vertex(&mut log, "P", batch, 0, &[l.start.x, l.start.y, 0.0]);
                    vertex(&mut log, "P", batch, 1, &[l.end.x, l.end.y, 0.0]);
                    batch += 1;
                    if batch == 64 {
                        log.t("R20 lines 0 64 0");
                        batch = 0;
                    }
                }
                if batch > 0 {
                    log.t("R20 lines 0");
                    log.i(batch as i64);
                    log.i(0);
                }
                log.t("R62 p 0 R64 3 0 R65 2 0");
            }
            log.h(1.0);
            check(&mut golden, "placement", n, &mut log);
            n += 1;
        }
    }
    assert!(golden.next().is_none());
}
