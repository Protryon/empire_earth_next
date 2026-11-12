//! `EEE3DEarthquake`'s crack against the unmodified C++ (`golden_quake.txt`). A scratch driver (`build/gx11_golden`,
//! `driver_quake.cpp`; PORT_LEARNINGS "R4.4, fifth round") #included `EEE3DEarthquake.cpp` of the commit before
//! this work over the real GE math, with a logging `GEModelManager::CreateModel` whose model has storing vertex and
//! polygon arrays, a logging `EEMap::GetGroundPoint` (z = x / 4 - y / 8 + 3), a logging
//! `GETerrainMesh::GetIsSpaceTile` (a salted hash of the tile), `EEEffect3D::Queue` answering the case's value and
//! logging `PrepareTransforms` and `GEMaterial::SetTexture`. From an LCG seeded 1 it drew 300 cracks (0 to 14
//! segments, a random yaw and place), built each and queued it up to five times with its front anywhere from
//! behind the start to past the end, and printed per call an FNV-1a 64 of its log (every ground and space
//! question, the hold time, the growing flag, the queue entry and the model's final state). This test draws the
//! same stream, replays the C++ glue (the ground questions in their order) around `quake.rs` and compares.

use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge3DPoint, GeTransformation};
use ee_ge::um::floor_unsigned;

use crate::golden_mouse::{check, Log, Rng};
use crate::quake::{build_rows, hold_time, queue_rows, segment_polygons, EeGfxQuakeRow, EE_GFX_QUAKE_ROW_U};

const GOLDEN: &str = include_str!("golden_quake.txt");

#[derive(Clone, Copy, Default)]
struct Vertex {
    p: Ge3DPoint,
    u: f32,
    v: f32,
}

fn ground(log: &mut Log, p: Ge3DPoint) -> Ge3DPoint {
    log.t("ground");
    log.h(p.x);
    log.h(p.y);
    log.h(p.z);
    Ge3DPoint { x: p.x, y: p.y, z: p.x * 0.25 - p.y * 0.125 + 3.0 }
}

fn space(log: &mut Log, x: u32, y: u32, salt: u32) -> bool {
    log.t("space");
    log.i(x as i64);
    log.i(y as i64);
    x.wrapping_mul(31).wrapping_add(y.wrapping_mul(17)).wrapping_add(salt).is_multiple_of(23)
}

fn dump(log: &mut Log, verts: &[Vertex], polys: &[u32], segments: u32) {
    log.t("model");
    for (i, x) in verts.iter().enumerate() {
        log.t("v");
        log.i(i as i64);
        log.i(12);
        for f in [x.p.x, x.p.y, x.p.z, x.u, x.v, 0.0, 0.0, 0.0, 0.0] {
            log.h(f);
        }
    }
    log.t("polys");
    for &i in polys {
        log.i(i as i64);
    }
    log.t("mat");
    log.i(0);
    log.i(segments as i64 * 8);
    log.i(0);
    log.i(verts.len() as i64);
}

#[test]
fn the_crack_matches_the_cpp_golden() {
    let mut golden = GOLDEN.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    let mut n = 0;
    let mut holds = 0;
    for _ in 0..300 {
        let has_object = g.b(80);
        let screen = g.r() % 2;
        let yaw = g.f(-3.2, 3.2);
        let mut t = GeTransformation::IDENTITY;
        t.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
        t.m[0][3] = g.f(0.0, 200.0);
        t.m[1][3] = g.f(0.0, 200.0);
        let segments = g.r() % 15;
        let length = segments as f32 - if g.b(50) { g.f(0.0, 0.999) } else { 0.0 };
        let speed = g.f(0.0005, 0.01);
        let mut growing = g.b(70);
        let salt = g.r();
        log.t("in");
        log.i(segments as i64);
        log.h(yaw);
        log.h(length);
        log.h(speed);
        log.i(growing as i64);
        log.i(has_object as i64);
        log.i(screen as i64);
        // BuildModel
        let count = ((segments + 1) * 5) as usize;
        log.t("create");
        for v in [segments as i64 * 8, count as i64, 1, 0, 1, 0] {
            log.i(v);
        }
        log.t("earthquake model");
        log.i(1);
        let mut verts = vec![Vertex::default(); count];
        let mut point = Ge3DPoint::default();
        for (i, v) in verts.iter_mut().take(5).enumerate() {
            point.x = i as f32 - 2.0;
            let gp = ground(&mut log, t.apply_point(point));
            point.z = gp.z;
            v.p = point;
        }
        for (i, v) in verts.iter_mut().take(5).enumerate() {
            v.u = EE_GFX_QUAKE_ROW_U[i];
            v.v = 0.0;
        }
        let mut polys = Vec::new();
        let mut rows = vec![0.0f32; segments as usize];
        build_rows(&mut rows);
        for s in 0..segments as usize {
            polys.extend_from_slice(&segment_polygons(s as u32));
            for i in 0..5 {
                verts[s * 5 + 5 + i].u = EE_GFX_QUAKE_ROW_U[i];
                verts[s * 5 + 5 + i].v = rows[s];
            }
        }
        log.t("normals");
        log.i(0);
        log.t("bounds");
        log.i(0);
        log.t("prep");
        for r in 0..3 {
            for k in 0..4 {
                log.h(t.m[r][k]);
            }
        }
        log.t("mattex");
        log.i(0x1000);
        log.i(screen as i64);
        log.i(0);
        dump(&mut log, &verts, &polys, segments);
        check(&mut golden, "build", n, &mut log);
        n += 1;
        let calls = 1 + g.r() % 5;
        for _ in 0..calls {
            let d = if g.b(15) { (g.r() % (segments + 2)) as f32 } else { g.f(-0.5, segments as f32 + 1.0) };
            let base = if g.b(10) { 10 } else { 0 };
            log.t("queue");
            log.h(d);
            log.i(base);
            log.i(growing as i64);
            log.t("base");
            let mut hold = -1.0f32;
            if base == 0 {
                let mut over_space = false;
                let mut qrows = vec![EeGfxQuakeRow::default(); segments as usize];
                queue_rows(d, &mut qrows);
                for (s, row) in qrows.iter().enumerate() {
                    let mut point = Ge3DPoint { x: 0.0, y: row.y, z: 0.0 };
                    for i in 0..5 {
                        point.z = 0.0;
                        point.x = i as f32 - 2.0;
                        let gp = ground(&mut log, t.apply_point(point));
                        point.z = gp.z + 0.01;
                        verts[s * 5 + 5 + i].p = point;
                        if i == 2 && space(&mut log, floor_unsigned(gp.x), floor_unsigned(gp.y), salt) {
                            over_space = true;
                        }
                    }
                    for i in 0..5 {
                        verts[s * 5 + 5 + i].u = EE_GFX_QUAKE_ROW_U[i];
                        verts[s * 5 + 5 + i].v = row.front_v;
                    }
                }
                if growing && over_space {
                    hold = hold_time(length, d, speed);
                    growing = false;
                    holds += 1;
                }
                log.t("normals");
                log.i(0);
                log.t("bounds");
                log.i(0);
            }
            log.i(base);
            log.h(hold);
            log.i(growing as i64);
            if base == 0 {
                log.i(0);
                log.i(if has_object { [0x1000, 0x2000][screen as usize] } else { 0 });
                log.i(1);
            }
            dump(&mut log, &verts, &polys, segments);
            check(&mut golden, "queue", n, &mut log);
            n += 1;
        }
    }
    assert!(golden.next().is_none());
    assert!(holds > 10, "{holds}");
}
