//! `EEROTerrain`'s sky, clouds and grid mode against the unmodified C++ (`golden_sky.txt`). A scratch driver
//! (`build/gx11_golden`, `driver_sky.cpp`; PORT_LEARNINGS "R4.4, fifth round") #included `EEROTerrain.cpp` of the
//! commit before this work over the real GE math, with storing fake models as the sky dome, the star models and
//! the cloud layer, a raw `gWorld` whose map has the case's size and width, and `gSkyColors` filled as
//! `CreateSkyModel` fills it. From an LCG seeded 1 it drew 600 maps (sides 1 to 400, widths up to the side), each
//! with a time of day (a quarter on the 17 steps exactly, some just under 1) for `UpdateSkyColors` and a cloud
//! offset (below 1, past it, whole numbers) for `UpdateCloudLayer`, then every grid mode from -1 to 18 stepped both
//! ways, and printed per case an FNV-1a 64 of its log (the fog colour, the stored offset and the models' final
//! vertices). This test draws the same stream, runs each case through `terrain.rs` and compares the digests.

use ee_ge::types::Ge3DPoint;

use crate::effects::{EeGfxColor, EeGfxTexCoord};
use crate::golden_mouse::{check, Log, Rng};
use crate::terrain::{cloud_layer, grid_mode_step, sky_frame};

const GOLDEN: &str = include_str!("golden_sky.txt");

/// The sky colours' table (EEROTerrain.cpp's sSkyColorTable, 0x4416d4), as `gSkyColors` holds it.
const TABLE: [[f32; 3]; 68] = [
    [0.773, 0.255, 0.051],
    [0.698, 0.894, 0.984],
    [0.698, 0.894, 0.984],
    [0.698, 0.894, 0.984],
    [0.698, 0.894, 0.984],
    [0.698, 0.894, 0.984],
    [0.698, 0.894, 0.984],
    [0.698, 0.894, 0.984],
    [0.773, 0.255, 0.051],
    [0.047, 0.22, 0.373],
    [0.023, 0.11, 0.185],
    [0.023, 0.11, 0.185],
    [0.023, 0.11, 0.185],
    [0.023, 0.11, 0.185],
    [0.023, 0.11, 0.185],
    [0.047, 0.22, 0.373],
    [0.773, 0.255, 0.051],
    [0.941, 0.69, 0.259],
    [0.698, 0.894, 0.984],
    [0.698, 0.894, 0.984],
    [0.698, 0.894, 0.984],
    [0.698, 0.894, 0.984],
    [0.698, 0.894, 0.984],
    [0.698, 0.894, 0.984],
    [0.698, 0.894, 0.984],
    [0.941, 0.69, 0.259],
    [0.023, 0.11, 0.185],
    [0.008, 0.031, 0.125],
    [0.008, 0.031, 0.125],
    [0.008, 0.031, 0.125],
    [0.008, 0.031, 0.125],
    [0.008, 0.031, 0.125],
    [0.023, 0.11, 0.185],
    [0.941, 0.69, 0.259],
    [0.302, 0.267, 0.349],
    [0.2, 0.29, 0.659],
    [0.2, 0.29, 0.659],
    [0.2, 0.29, 0.659],
    [0.2, 0.29, 0.659],
    [0.2, 0.29, 0.659],
    [0.2, 0.29, 0.659],
    [0.2, 0.29, 0.659],
    [0.302, 0.267, 0.349],
    [0.008, 0.031, 0.125],
    [0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0],
    [0.008, 0.031, 0.125],
    [0.302, 0.267, 0.349],
    [0.008, 0.031, 0.125],
    [0.1, 0.15, 0.325],
    [0.1, 0.15, 0.325],
    [0.1, 0.15, 0.325],
    [0.1, 0.15, 0.325],
    [0.1, 0.15, 0.325],
    [0.1, 0.15, 0.325],
    [0.1, 0.15, 0.325],
    [0.008, 0.031, 0.125],
    [0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0],
    [0.0, 0.0, 0.0],
    [0.008, 0.031, 0.125],
];

/// One vertex of a fake model's dump: its set flags (point 8, coordinate 4, colour 2, opacity 1) and values.
fn vertex(log: &mut Log, i: usize, p: Option<Ge3DPoint>, t: Option<EeGfxTexCoord>, c: Option<EeGfxColor>) {
    log.t("v");
    log.i(i as i64);
    log.i(p.is_some() as i64 * 8 + t.is_some() as i64 * 4 + c.is_some() as i64 * 2);
    let p = p.unwrap_or_default();
    let t = t.unwrap_or_default();
    let c = c.unwrap_or_default();
    for f in [p.x, p.y, p.z, t.u, t.v, c.red, c.green, c.blue, 0.0] {
        log.h(f);
    }
}

#[test]
fn the_sky_matches_the_cpp_golden() {
    let mut colors = [EeGfxColor::default(); 68];
    for (c, t) in colors.iter_mut().zip(TABLE.iter()) {
        *c = EeGfxColor { red: t[0], green: t[1], blue: t[2] };
    }
    const SIZES: [i32; 8] = [16, 64, 100, 128, 200, 256, 1, 3];
    let mut golden = GOLDEN.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    let mut n = 0;
    for _ in 0..600 {
        let size = if g.b(30) { SIZES[(g.r() % 8) as usize] } else { (g.r() % 400) as i32 + 1 };
        let width = if g.b(20) { size / 2 } else { (g.r() % (size as u32 + 1)) as i32 };
        let mut t = if g.b(25) { (g.r() % 17) as f32 / 16.0 } else { g.f(0.0, 1.0) };
        if g.b(5) {
            t = 0.99999994;
        }
        log.t("sky");
        log.i(size as i64);
        log.i(width as i64);
        log.h(t);
        let f = sky_frame(t, &colors, size, width);
        for m in 0..3 {
            log.t("normals");
            log.i(m);
            log.t("bounds");
            log.i(m);
        }
        log.h(f.fog.red);
        log.h(f.fog.green);
        log.h(f.fog.blue);
        log.t("model");
        log.i(0);
        for i in 0..20 {
            vertex(&mut log, i, Some(f.sky_points[i]), None, Some(f.sky_colors[i]));
        }
        log.t("model");
        log.i(1);
        for i in 0..24 {
            vertex(&mut log, i, Some(f.outer_stars[i]), None, None);
        }
        log.t("model");
        log.i(2);
        for i in 0..24 {
            vertex(&mut log, i, Some(f.inner_stars[i]), None, None);
        }
        check(&mut golden, "sky", n, &mut log);
        n += 1;
        let mut o = if g.b(40) { g.f(0.0, 1.0) } else { g.f(0.0, 5.0) };
        if g.b(10) {
            o = (g.r() % 4) as f32;
        }
        log.t("cloud");
        log.i(size as i64);
        log.i(width as i64);
        log.h(o);
        let c = cloud_layer(size, width, o);
        log.t("normals");
        log.i(3);
        log.t("bounds");
        log.i(3);
        log.h(if c.wrapped { c.offset } else { o });
        log.t("model");
        log.i(3);
        for i in 0..8 {
            vertex(&mut log, i, Some(c.points[i]), Some(c.coords[i]), None);
        }
        check(&mut golden, "cloud", n, &mut log);
        n += 1;
    }
    for m in -1..19 {
        for b in 0..2 {
            log.t("grid");
            log.i(m as i64);
            log.i(b);
            log.i(grid_mode_step(m, b != 0) as i64);
            check(&mut golden, "grid", n, &mut log);
            n += 1;
        }
    }
    assert!(golden.next().is_none());
}
