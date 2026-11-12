//! `DrawPlacement`'s footprint edges and `DrawLabels`' clip rectangles against the unmodified C++
//! (`golden_placement.txt`, `golden_labels.txt`). Two scratch drivers (`build/gx10_golden`; PORT_LEARNINGS "R4.4,
//! fourth round") #included `EEROUnits.cpp` of the commit before this work over the real GE math, `GEViewport` and
//! `UWideString`. `driver_placement.cpp`: a rasterizer whose slots log (the line draw its first line and count), 64
//! `GELine`s whose vertex arrays log every `SetPoint`, `SetColor` and `SetTextureCoordinate`, a unit class answering
//! `IsLandBuilding` (slot 21); 40 viewports with 30 footprints each (sizes 0 to 11, the cells all usable, none or a
//! random share). `driver_labels.cpp`: fake fonts (`ComputeTextExtent` sets a height by font: 14, 12, 16), a logging
//! `UIForm::DrawClippedText`; 40 viewports with 30 frames of up to six labelled units, most near the bottom strip's
//! top. From an LCG seeded 1 each printed per case an FNV-1a 64 of its log; these tests draw the same streams, run
//! each case through `units.rs` and the C++ glue rewritten here and compare the digests.

use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge2DPoint, Ge3DPoint, GeRectangle, GeTransformation};
use ee_ge::viewport::GeViewport;

use crate::golden_mouse::{check, Log, Rng};
use crate::units::*;

const PLACEMENT: &str = include_str!("golden_placement.txt");
const LABELS: &str = include_str!("golden_labels.txt");

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

/// `NearStrip`: a point on the ray of a screen point near the bottom strip's top.
fn near_strip(g: &mut Rng, vp: &GeViewport, ortho: bool, height: f32) -> Ge3DPoint {
    let sx = g.f(-50.0, 2000.0);
    let sy = height * 0.7135417 + g.f(-40.0, 40.0);
    let depth = g.f(0.05, 0.95);
    let scale = 1.0 / vp.half_width;
    let near = Ge3DPoint::new((sx - vp.x_center) * scale, -((sy - vp.y_center) * scale), 0.0);
    let far = Ge3DPoint::new(near.x, near.y, 1.0);
    let (a, b) = if !ortho {
        (
            vp.denormalize_perspective_point(near, vp.front_clipping_distance),
            vp.denormalize_perspective_point(far, vp.rear_clipping_distance),
        )
    } else {
        (vp.denormalize_orthographic_point(near), vp.denormalize_orthographic_point(far))
    };
    Ge3DPoint::new((b.x - a.x) * depth + a.x, (b.y - a.y) * depth + a.y, (b.z - a.z) * depth + a.z)
}

struct LabelUnit {
    command: i64,
    name: &'static str,
    transformation: GeTransformation,
    bar: Ge3DPoint,
    scale: f32,
    bars: u64,
    colour: [f32; 3],
}

#[test]
fn labels_match_the_cpp_golden() {
    const NAMES: [&str; 5] = ["", " Rally", "Rally", " ", " a"];
    const LABEL_TEXTS: [&str; 3] = ["", "debug", "x y z"];
    const WIDTHS: [u32; 8] = [640, 800, 1024, 1279, 1280, 1599, 1600, 1920];
    const HEIGHTS: [u32; 6] = [480, 600, 768, 1024, 1080, 1200];
    // the fake fonts' ids (1 default, 2 Tahoma 12, 3 Tahoma 16) and text heights
    const FONT_IDS: [i64; 3] = [1, 2, 3];
    const FONT_HEIGHTS: [f32; 3] = [14.0, 12.0, 16.0];
    let mut golden = LABELS.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    let mut vp = GeViewport::default();
    let mut n = 0i64;
    for _ in 0..40 {
        viewport(&mut g, &mut log, &mut vp);
        check(&mut golden, "viewport", n, &mut log);
        n += 1;
        for _ in 0..30 {
            let ortho = g.b(20);
            let free = g.b(15);
            let panel = g.b(50);
            let w = WIDTHS[(g.r() % 8) as usize];
            let h = HEIGHTS[(g.r() % 6) as usize];
            let count = g.r() % 7;
            let debug = g.r() % 8;
            let label = LABEL_TEXTS[(g.r() % 3) as usize];
            log.t("in");
            for v in [ortho as i64, free as i64, panel as i64, w as i64, h as i64, count as i64, debug as i64] {
                log.i(v);
            }
            let mut units = vec![];
            for _ in 0..count {
                let command = if g.b(50) { 0 } else { 1 + (g.r() % 3) as i64 };
                let name = NAMES[(g.r() % 5) as usize];
                let p = if g.b(70) {
                    near_strip(&mut g, &vp, ortho, h as f32)
                } else {
                    let x = g.f(-5.0, 75.0);
                    let y = g.f(-5.0, 75.0);
                    let z = g.f(-3.0, 30.0);
                    Ge3DPoint::new(x, y, z)
                };
                let mut t = transform(&mut g);
                t.m[0][3] = p.x;
                t.m[1][3] = p.y;
                t.m[2][3] = p.z;
                let bx = g.f(-1.0, 1.0);
                let by = g.f(-1.0, 1.0);
                let bz = g.f(0.0, 4.0);
                let scale = g.f(0.2, 3.0);
                let bars = (g.r() % 4) as u64;
                let r = g.f(0.0, 1.0);
                let gg = g.f(0.0, 1.0);
                let bb = g.f(0.0, 1.0);
                log.i(command);
                log.i(name.len() as i64);
                log.h(p.x);
                log.h(p.y);
                log.h(p.z);
                log.i(bars as i64);
                units.push(LabelUnit {
                    command,
                    name,
                    transformation: t,
                    bar: Ge3DPoint::new(bx, by, bz),
                    scale,
                    bars,
                    colour: [r, gg, bb],
                });
            }
            // the C++ glue: DrawLabels over units.rs
            let clip = label_clip(vp.view_rectangle, w, h, free, panel);
            let font = clip.font as usize;
            let mut colour = [1.0f32, 1.0, 1.0];
            for (i, u) in units.iter().enumerate() {
                let (point, text_len);
                if u.command != 0 {
                    if !u.name.is_empty() && u.name.starts_with(' ') {
                        point = screen_point(
                            &vp,
                            ortho,
                            Ge3DPoint::new(
                                u.transformation.m[0][3],
                                u.transformation.m[1][3],
                                u.transformation.m[2][3],
                            ),
                        );
                        text_len = u.name.len();
                    } else {
                        continue;
                    }
                } else if debug as usize == i {
                    point = label_point(&vp, ortho, &u.transformation, u.bar, u.scale, u.bars);
                    colour = u.colour;
                    text_len = label.len();
                } else {
                    continue;
                }
                let targets = label_targets(point.y, FONT_HEIGHTS[font], clip.bottom, free);
                let draw = |log: &mut Log, form: i64, r: &GeRectangle| {
                    log.t("clip");
                    log.i(form);
                    log.rect(r);
                    log.p2(point);
                    for c in colour {
                        log.h(c);
                    }
                    log.i(FONT_IDS[font]);
                    log.i(1);
                    log.i(1);
                    log.i(text_len as i64);
                };
                if targets & 1 != 0 {
                    draw(&mut log, 10, &clip.view);
                }
                if targets & 2 != 0 {
                    draw(&mut log, 10, &clip.strip);
                }
                if targets & 4 != 0 {
                    draw(&mut log, 11, &clip.view);
                }
            }
            check(&mut golden, "labels", n, &mut log);
            n += 1;
        }
    }
    assert!(golden.next().is_none());
}

#[test]
fn a_label_point_sits_above_the_bars() {
    let vp = GeViewport::default();
    let t = GeTransformation::IDENTITY;
    let a = label_point(&vp, false, &t, Ge3DPoint::new(0.0, 5.0, 1.0), 1.0, 0);
    let b = label_point(&vp, false, &t, Ge3DPoint::new(0.0, 5.0, 1.0), 1.0, 2);
    assert_eq!(a.y - b.y, 10.0);
    assert_eq!(a.x, b.x);
    let _ = Ge2DPoint::default();
}
