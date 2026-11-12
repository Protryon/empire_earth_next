//! The selection rings' size choice and the area-effect ring's colours against the unmodified C++
//! (`golden_rings.txt`). A scratch driver (`build/gx9_golden`, `driver_rings.cpp`; PORT_LEARNINGS "R4.4, third
//! round") #included `EEROUnits.cpp` of the commit before this work over the real GE math, a fake rasterizer whose
//! slots log (43 `SetBlending`, 45 `SetCulling`, 39 `SetAlphaTestingRefValue`, 19 `Draw` with its model and sorted
//! flag), fake ring and selection models whose vertex array's slot 9 `SetColor` logs its colour and index, and a
//! `GEModel::PrepareTransforms` that logs the scale and the transformation set. From an LCG seeded 1 it drew 400
//! frames of up to six selected units (`DrawSelection`, then `DrawRings` over the same units) and printed per frame
//! an FNV-1a 64 of its log. This test draws the same stream, runs each frame through `units.rs` and the C++ glue
//! rewritten here and compares the digests.

use ee_ge::transform::RotationOrder;
use ee_ge::types::GeTransformation;

use crate::golden_mouse::{check, Log, Rng};
use crate::units::*;

const GOLDEN: &str = include_str!("golden_rings.txt");

const RING: [f32; 3] = [0.0, 1.0, 0.0];
const HIGHLIGHT: [f32; 3] = [0.537, 0.678, 1.0];
const SIZES: [f32; 8] = [0.75, 1.5, 100.0, 0.0, -1.0, 0.7499, 1.5001, 150.0];

struct Unit {
    t: GeTransformation,
    size: f32,
    domains: u32,
    strength: u32,
}

fn prep(log: &mut Log, model: u32, scale: f32, t: &GeTransformation) {
    log.t("prep");
    log.i(model as i64);
    log.h(scale);
    for r in 0..3 {
        for k in 0..4 {
            log.h(t.m[r][k]);
        }
    }
}

#[test]
fn rings_match_the_cpp_golden() {
    let mut golden = GOLDEN.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    for n in 0..400 {
        let count = g.r() % 7;
        let ortho = g.b(20) as i64;
        let yaw = g.f(-3.2, 3.2);
        log.t("in");
        log.i(count as i64);
        log.i(ortho);
        let mut units = vec![];
        for _ in 0..count {
            let mut t = GeTransformation::IDENTITY;
            let y = g.f(-3.2, 3.2);
            let p = g.f(-1.5, 1.5);
            t.set_orientation(RotationOrder::Ypr, y, p, 0.0);
            let x = g.f(-4.0, 74.0);
            let yy = g.f(-4.0, 74.0);
            let z = g.f(-2.0, 30.0);
            t.m[0][3] = x;
            t.m[1][3] = yy;
            t.m[2][3] = z;
            let size = if g.b(40) { g.pick(&SIZES) } else { g.f(-0.5, 20.0) };
            let domains = g.r() % 4;
            let strength = if g.b(30) { 0 } else { g.r() % 10 };
            log.h(size);
            log.i(domains as i64);
            log.i(strength as i64);
            units.push(Unit { t, size, domains, strength });
        }
        // DrawSelection's glue (no collision spheres)
        log.t("selection R43 R45 white R39");
        for u in &units {
            if !(u.size > 0.0) || (u.domains & 1) != 0 {
                continue;
            }
            let r = selection_ring(u.size);
            prep(&mut log, r.model + 1, r.scale, &u.t);
            log.t("R19");
            log.i(r.model as i64 + 1);
            log.i(ortho);
        }
        // DrawRings' glue over mUnits0 (keyed by the unit's index mod 3, insertion order within a key)
        log.t("rings R43 R45 white");
        let mut ring = GeTransformation::IDENTITY;
        ring.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
        let mut order: Vec<usize> = (0..units.len()).collect();
        order.sort_by_key(|i| i % 3);
        for i in order {
            let u = &units[i];
            if !(u.strength > 0 && u.size > 0.0) {
                continue;
            }
            ring.m[0][3] = u.t.m[0][3];
            ring.m[1][3] = u.t.m[1][3];
            ring.m[2][3] = u.t.m[2][3];
            let s = ring_segments(u.strength);
            let mut vertex = 0;
            for (c, segments) in [(RING, s.ring), (HIGHLIGHT, s.highlight)] {
                for _ in 0..segments * 3 {
                    log.t("A10.9");
                    log.h(c[0]);
                    log.h(c[1]);
                    log.h(c[2]);
                    log.i(vertex);
                    vertex += 1;
                }
            }
            prep(&mut log, 0, u.size, &ring);
            log.t("R19 0");
            log.i(ortho);
        }
        check(&mut golden, "rings", n, &mut log);
    }
    assert!(golden.next().is_none(), "golden longer than the replay");
}
