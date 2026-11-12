//! The effects' quads and fog factor against the unmodified C++ (`golden_quads.txt`). A scratch driver
//! (`build/gx10_golden`, `driver_quads.cpp`; PORT_LEARNINGS "R4.4, fourth round") #included `EEEffect.cpp` of the
//! commit before this work over the real GE math and `GEViewport`, a rasterizer whose slots log (their integer
//! arguments) and whose one screen state holds the case's fog, and the two batches' vertex arrays as logging
//! `GEVertexArray`s (`SetVertex` logs every argument). From an LCG seeded 1 it drew 40 viewports with 50 cases each:
//! one of the five quads (or the fog factor alone) with the batches' counts near full, so the flushes run, and
//! printed per case an FNV-1a 64 of its log. This test draws the same stream, runs each case through `effects.rs`
//! and the C++ glue rewritten here (the counts, the flushes' rasterizer calls, the alpha references) and compares
//! the digests.

use ee_ge::types::Ge3DPoint;
use ee_ge::viewport::GeViewport;

use crate::effects::*;
use crate::golden_mouse::{check, Log, Rng};
use crate::golden_placement::viewport;

const GOLDEN: &str = include_str!("golden_quads.txt");

struct Batch {
    count: u32,
    reference: f32,
}

/// `FlushEffectBatch`'s (camera) or `FlushEffectGroundBatch`'s rasterizer calls, as the driver logs them.
fn flush(log: &mut Log, camera: bool, batch: &mut Batch, fog_state: u32, culling: u32) {
    log.t("R45 0");
    if camera {
        log.t("R51 0 0");
    }
    log.t("R39 0 R64 2 0 R59 1 0");
    if camera {
        log.t("R43 1 1 white");
    } else {
        log.t("R43 4 5 prelit");
    }
    log.t("R13 p");
    log.i((batch.count / 2) as i64);
    if camera {
        log.t("R51");
        log.i(fog_state as i64);
        log.i(0);
    }
    log.t("R45");
    log.i(culling as i64);
    batch.count = 0;
    batch.reference = 1.0;
}

fn add(log: &mut Log, id: i64, batch: &mut Batch, quad: &EeGfxEffectQuad) {
    for k in 0..4 {
        log.t("V");
        log.i(id);
        log.i((batch.count + k as u32) as i64);
        for v in [quad.color.red, quad.color.green, quad.color.blue] {
            log.h(v);
        }
        let (p, n, c) = (quad.points[k], quad.normal, quad.coords[k]);
        for v in [p.x, p.y, p.z, n.x, n.y, n.z, c.u, c.v, quad.opacities[k]] {
            log.h(v);
        }
    }
    batch.count += 4;
}

#[test]
fn quads_match_the_cpp_golden() {
    const COUNTS: [u32; 6] = [0, 4, 0x7f8, 0x7fc, 0x800, 0x100];
    let mut golden = GOLDEN.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    let mut vp = GeViewport::default();
    let mut n = 0i64;
    for _ in 0..40 {
        viewport(&mut g, &mut log, &mut vp);
        check(&mut golden, "viewport", n, &mut log);
        n += 1;
        for _ in 0..50 {
            let kind = g.r() % 6;
            let ortho = g.b(20);
            let state = if g.b(60) { 1 } else { g.r() % 3 };
            let start = g.f(-5.0, 40.0);
            let len = if g.b(10) { 0.0 } else { g.f(0.5, 80.0) };
            let fog =
                EeGfxEffectFog { state, start, end: start + len, scale: if len != 0.0 { 1.0 / len } else { 1.0 } };
            let _fog_type = g.r() % 4;
            let culling = g.r() % 3;
            let yaw = g.f(-3.2, 3.2);
            let pitch = g.f(-1.6, 0.2);
            let mut camera = Batch { count: COUNTS[(g.r() % 6) as usize], reference: 0.0 };
            let mut ground = Batch { count: COUNTS[(g.r() % 6) as usize], reference: 0.0 };
            camera.reference = g.f(0.0, 1.0);
            ground.reference = g.f(0.0, 1.0);
            let px = g.f(-5.0, 75.0);
            let py = g.f(-5.0, 75.0);
            let pz = g.f(-3.0, 30.0);
            let position = Ge3DPoint::new(px, py, pz);
            let red = g.f(0.0, 1.0);
            let green = g.f(0.0, 1.0);
            let blue = g.f(0.0, 1.0);
            let color = EeGfxColor { red, green, blue };
            let rotation = g.f(-7.0, 7.0);
            let size = if g.b(5) { 0.0 } else { g.f(0.01, 12.0) };
            let alpha = g.f(0.0, 1.0);
            let brightness = g.f(0.0, 2.0);
            let reference = g.f(0.0, 1.0);
            log.t("in");
            for v in [kind as i64, ortho as i64, state as i64, camera.count as i64, ground.count as i64] {
                log.i(v);
            }
            // the C++ glue: the batch's flush when full, its alpha reference, the quad, the count
            let begin = |log: &mut Log, cam: bool, batch: &mut Batch| {
                if batch.count == 0x800 {
                    flush(log, cam, batch, state, culling);
                }
                if reference < batch.reference {
                    batch.reference = reference;
                }
            };
            match kind {
                0 => {
                    let f = fog_factor(position, &fog, &vp.camera_inverse);
                    begin(&mut log, true, &mut camera);
                    let q = glow_sprite(position, color, rotation, size, alpha, brightness, f, yaw, pitch);
                    add(&mut log, 1, &mut camera, &q);
                }
                1 => {
                    begin(&mut log, false, &mut ground);
                    let q = ground_sprite(position, color, rotation, size, [alpha; 4], brightness);
                    add(&mut log, 2, &mut ground, &q);
                }
                2 => {
                    let mut heights = [0.0f32; 4];
                    for h in &mut heights {
                        *h = g.f(0.0, 1.0);
                    }
                    begin(&mut log, false, &mut ground);
                    let q = ground_sprite(position, color, rotation, size, heights, brightness);
                    add(&mut log, 2, &mut ground, &q);
                }
                3 => {
                    begin(&mut log, false, &mut ground);
                    let q = blended_sprite(position, color, rotation, size, alpha, brightness, yaw, pitch);
                    add(&mut log, 2, &mut ground, &q);
                }
                4 => {
                    let tx = if g.b(10) { px } else { px + g.f(-6.0, 6.0) };
                    let ty = if g.b(10) { py } else { py + g.f(-6.0, 6.0) };
                    let tz = pz + g.f(-3.0, 3.0);
                    let height = g.f(0.0, 5.0);
                    let ou = g.f(-1.0, 1.0);
                    let ov = g.f(-1.0, 1.0);
                    let su = g.f(-1.0, 2.0);
                    let sv = g.f(-1.0, 2.0);
                    begin(&mut log, false, &mut ground);
                    let q = ground_strip(
                        position,
                        Ge3DPoint::new(tx, ty, tz),
                        height,
                        color,
                        EeGfxTexCoord { u: ou, v: ov },
                        EeGfxTexCoord { u: su, v: sv },
                        alpha,
                        brightness,
                    );
                    add(&mut log, 2, &mut ground, &q);
                }
                _ => log.h(fog_factor(position, &fog, &vp.camera_inverse)),
            }
            log.i(camera.count as i64);
            log.h(camera.reference);
            log.i(ground.count as i64);
            log.h(ground.reference);
            check(&mut golden, "quad", n, &mut log);
            n += 1;
        }
    }
    assert!(golden.next().is_none());
}
