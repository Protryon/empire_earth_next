//! `EEE3DVolcano::BuildModel` against the unmodified C++ (`golden_volcano.txt`). A scratch driver
//! (`build/gx12_golden`, `driver_volcano.cpp`; PORT_LEARNINGS "R4.4, sixth round") #included `EEE3DVolcano.cpp` of
//! the commit before this work over the real GE math, with storing fake models (`fakemesh.h`), a fake screen and
//! surface (fake vtables) handing out the case's 19 x 19 image, terrain point and tile rows of the driver's
//! (salted heights, random texture sets, rows cut short or empty), a logging `EETerrainTable::Get` (eight colours)
//! and `GetVertexColor` (a salted colour by tile and corner). From an LCG seeded 1 it drew 150 volcanoes (whole or
//! quarter-tile places, a random size and centre height), built each and printed an FNV-1a 64 of the whole log and
//! one of the Rust-built part (`vmesh`, the lines kept here: the model's final vertices and triangles, `mPoints`,
//! `mHeights` and the centre's z). This test draws the same stream, builds with `volcano.rs` and compares.

use ee_ge::types::Ge3DPoint;

use crate::effects::EeGfxColor;
use crate::golden_farm::dump_mesh;
use crate::golden_mouse::{check, Log, Rng};
use crate::mesh::{EeGfxMeshTriangle, EeGfxMeshVertex, Mesh};
use crate::volcano::{build, image, sizes, EeGfxVolcanoArea, VolcanoTerrain};

const GOLDEN: &str = include_str!("golden_volcano.txt");
const MAP: usize = 64;

struct Terrain {
    salt: u32,
    counts: Vec<u32>,
    ids: Vec<u8>,
}

impl VolcanoTerrain for Terrain {
    fn z(&mut self, x: u32, y: u32) -> f32 {
        (x.wrapping_mul(7).wrapping_add(y.wrapping_mul(13)).wrapping_add(self.salt) % 29) as f32 * 0.0625
            + y as f32 * 0.03125
    }
    fn color(&mut self, x: u32, y: u32) -> EeGfxColor {
        let i = self.ids[y as usize * MAP + x as usize] as f32;
        EeGfxColor { red: 0.125 * i, green: 1.0 - 0.0625 * i, blue: 0.5 + 0.03125 * i }
    }
    fn valid_tile(&mut self, x: u32, y: u32) -> bool {
        (y as usize) < MAP && x < self.counts[y as usize]
    }
    fn light(&mut self, x: i32, y: i32, corner: i32) -> EeGfxColor {
        let h = (x.wrapping_mul(31).wrapping_add(y.wrapping_mul(17)).wrapping_add(corner.wrapping_mul(7)) as u32)
            .wrapping_add(self.salt);
        EeGfxColor {
            red: (h % 11) as f32 * 0.09375,
            green: (h % 7) as f32 * 0.125 + 0.125,
            blue: (h % 5) as f32 * 0.25,
        }
    }
}

fn place(g: &mut Rng) -> f32 {
    let whole = (8 + g.r() % 44) as f32;
    whole + if g.b(50) { 0.25 * (g.r() % 4) as f32 } else { 0.0 }
}

#[test]
fn the_volcano_matches_the_cpp_golden() {
    let mut golden = GOLDEN.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    let mut quarters = 0;
    for n in 0..150 {
        let salt = g.r();
        let x = place(&mut g);
        let y = place(&mut g);
        quarters += (x.fract() != 0.0) as u32;
        let left = (x - 3.0).floor() as u32;
        let top = (y + 3.0).floor() as u32;
        let mut counts = vec![0u32; MAP];
        let mut ids = vec![0u8; MAP * MAP];
        for (row, count) in counts.iter_mut().enumerate() {
            let k = g.r() % 4;
            let row = row as u32;
            *count = if k == 1 {
                left + 6
            } else if k == 2 && (row < top - 6 || row > top) {
                0
            } else {
                MAP as u32
            };
            for c in 0..MAP {
                ids[row as usize * MAP + c] = (g.r() % 8) as u8;
            }
        }
        let row = 19 + (g.r() % 6) as usize;
        let pixels: Vec<u32> = (0..row * 19).map(|_| g.r().wrapping_mul(2654435761)).collect();
        let _screen = g.r();
        let height_scale = g.f(0.5, 6.0);
        let _graphic = g.b(80);
        let (l, t, r, b) = (x - 3.0, y + 3.0, x + 3.0, y - 3.0);
        let center_z = if g.b(50) { g.f(0.0, 4.0) } else { 0.0 };
        let area = EeGfxVolcanoArea {
            left: l,
            top: t,
            right: r,
            bottom: b,
            center: Ge3DPoint { x: (r - l) * 0.5 + l, y: (b - t) * 0.5 + t + 1.0, z: center_z },
        };
        let s = sizes(r - l);
        let count = s.vertices as usize;
        let mut points = vec![Ge3DPoint::default(); count];
        let mut heights = vec![0.0f32; count];
        let mut colors = vec![EeGfxColor::default(); count];
        assert_eq!(image(height_scale, s.side, &pixels, row, &mut colors, &mut heights), 0);
        let mut v = vec![EeGfxMeshVertex::default(); count];
        let mut tr = vec![EeGfxMeshTriangle::default(); s.polygons as usize];
        let mut mesh = Mesh::new(&mut v, &mut tr);
        let built = build(&area, &colors, &mut Terrain { salt, counts, ids }, &mut points, &mut heights, &mut mesh);
        assert_eq!(built.dropped, 0);
        dump_mesh(&mut log, &v, &tr, Ge3DPoint::default());
        // the driver dumps no place for the volcano: drop the replay's
        let cut = log.0.rfind(" place").unwrap();
        log.0.truncate(cut);
        log.t("points");
        for p in &points {
            log.h(p.x);
            log.h(p.y);
            log.h(p.z);
        }
        log.t("heights");
        for &h in &heights {
            log.h(h);
        }
        log.t("center");
        log.h(built.center_z);
        check(&mut golden, "vmesh", n, &mut log);
    }
    assert!(golden.next().is_none());
    assert!(quarters > 30);
}
