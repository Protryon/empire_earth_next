//! `EEROUnits::LoadMedia`'s ring models against the unmodified C++ (`golden_media.txt`). A scratch driver
//! (`build/gx11_golden`, `driver_media.cpp`; PORT_LEARNINGS "R4.4, fifth round") #included `EEROUnits.cpp` of the
//! commit before this work over the real GE math, with a logging `GEModelManager::CreateModel` whose models have
//! storing vertex and polygon arrays (real subclasses; a read of an unset slot logs), logging texture loads, stage
//! calls, `GEMaterial::SetTexture` and the normal and bound computations. For model detail 0, 1, 2, 3, -1 and 7 (and
//! screens 0 and 1) it ran `LoadMedia` once and printed two FNV-1a 64 lines: the calls (the textures, lines and
//! creations: checked by the driver's relink only) and the four models' final state (every vertex's point,
//! texture coordinate, colour and opacity, the triangles and the material's ranges). This test rebuilds the model
//! lines from `models.rs` and the unit's colours and compares the digests.

use crate::effects::{EeGfxColor, EeGfxTexCoord};
use crate::golden_mouse::{digest, Log};
use crate::models::{area_effect_ring, selection_model};
use ee_ge::types::Ge3DPoint;

const GOLDEN: &str = include_str!("golden_media.txt");

const RING_HIGHLIGHT: EeGfxColor = EeGfxColor { red: 0.537, green: 0.678, blue: 1.0 };
const SELECTION_COLORS: [EeGfxColor; 3] = [
    EeGfxColor { red: 1.0, green: 0.75, blue: 0.5 },
    EeGfxColor { red: 1.0, green: 0.56, blue: 0.25 },
    EeGfxColor { red: 1.0, green: 0.375, blue: 0.0 },
];

fn vertex(log: &mut Log, i: usize, p: Ge3DPoint, t: EeGfxTexCoord, c: EeGfxColor) {
    log.t("v");
    log.i(i as i64);
    log.i(15);
    for f in [p.x, p.y, p.z, t.u, t.v, c.red, c.green, c.blue, 1.0] {
        log.h(f);
    }
}

fn material(log: &mut Log, polygons: u32, vertices: u32) {
    log.t("mat");
    log.i(0);
    log.i(polygons as i64);
    log.i(0);
    log.i(vertices as i64);
}

#[test]
fn the_ring_models_match_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let mut n = 0;
    for detail in [0, 1, 2, 3, -1, 7] {
        for _screen in 0..2 {
            let mut log = Log::default();
            log.t("model");
            let r = area_effect_ring();
            for i in 0..26 {
                vertex(&mut log, i, r.points[i], r.coords[i], RING_HIGHLIGHT);
            }
            log.t("polys");
            for i in r.indices {
                log.i(i as i64);
            }
            material(&mut log, 10, 26);
            for size in 0..3 {
                let m = selection_model(detail, size);
                log.t("model");
                for i in 0..m.count as usize {
                    vertex(&mut log, i, m.points[i], m.coords[i], SELECTION_COLORS[m.colors[i] as usize]);
                }
                log.t("polys");
                for &i in &m.indices[..m.count as usize * 3] {
                    log.i(i as i64);
                }
                material(&mut log, m.count, m.count);
            }
            let line = format!("models {n}{}", log.0);
            let want = lines.iter().find(|l| l.starts_with(&format!("models {n} "))).expect("golden line");
            assert_eq!(&format!("models {n} {:016x}", digest(&line)), want, "detail {detail}: {line}");
            n += 1;
        }
    }
    assert_eq!(n, 12);
}
