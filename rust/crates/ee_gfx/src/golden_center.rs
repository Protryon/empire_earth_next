//! `EEUIMiniMap::CenterCamera` against the unmodified C++ (`golden_center.txt`). A scratch driver
//! (`build/gx10_golden`, `driver_uiminimap.cpp`; PORT_LEARNINGS "R4.4, fourth round") #included `EEUIMiniMap.cpp`
//! of the commit before this work with a fake window (its size), a fake `UIControl::GetMouseLocation` (the case's
//! point), `GERectangle::Contains` as `GEBitmap.cpp` has it and a logging `EEUserInterface::PostAction` (the
//! action's mode and point). From an LCG seeded 1 it drew 2,000 cases (window sizes down to 1 pixel, bounds of no
//! width or height, the mouse over, on the edge of and off the map's image) and printed per case an FNV-1a 64 of
//! its log. This test draws the same stream, runs each case through `minimap.rs` and compares the digests.

use ee_ge::types::{Ge2DPoint, GeRectangle};

use crate::golden_mouse::{check, Log, Rng};
use crate::minimap::center_point;

const GOLDEN: &str = include_str!("golden_center.txt");

#[test]
fn center_camera_matches_the_cpp_golden() {
    const WIDTHS: [u32; 8] = [640, 800, 1024, 1280, 1600, 1920, 2560, 1];
    const HEIGHTS: [u32; 8] = [480, 600, 768, 1024, 1080, 1200, 1440, 1];
    let mut golden = GOLDEN.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    for n in 0..2000 {
        let w = WIDTHS[(g.r() % 8) as usize];
        let h = HEIGHTS[(g.r() % 8) as usize];
        let ox = g.f(0.0, 1800.0);
        let oy = g.f(0.0, 1200.0);
        let left = g.f(-20.0, 40.0);
        let top = g.f(-20.0, 40.0);
        let right = left + if g.b(5) { 0.0 } else { g.f(1.0, 200.0) };
        let bottom = top + if g.b(5) { 0.0 } else { g.f(1.0, 160.0) };
        let bounds = GeRectangle { left, top, right, bottom };
        let sx = w as f32 * 0.00125;
        let sy = h as f32 * 0.0016666667;
        let mut mx = ox + g.f(left - 40.0, right + 40.0) * sx;
        let mut my = oy + g.f(top - 40.0, bottom + 40.0) * sy;
        if g.b(10) {
            mx = ox + left * sx;
        }
        if g.b(10) {
            my = oy + bottom * sy;
        }
        log.t("in");
        log.i(w as i64);
        log.i(h as i64);
        for v in [ox, oy, left, top, right, bottom, mx, my] {
            log.h(v);
        }
        let p = center_point(Ge2DPoint { x: mx, y: my }, Ge2DPoint { x: ox, y: oy }, w, h, &bounds);
        log.t("post");
        log.i(0);
        log.p2(p);
        check(&mut golden, "center", n, &mut log);
    }
    assert!(golden.next().is_none());
}
