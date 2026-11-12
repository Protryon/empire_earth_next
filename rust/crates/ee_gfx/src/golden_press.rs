//! `EEUIMiniMap::MouseRButtonDown` against the unmodified C++ (`golden_press.txt`). A scratch driver
//! (`build/gx11_golden`, `driver_rbutton.cpp`; PORT_LEARNINGS "R4.4, fifth round") #included `EEUIMiniMap.cpp` of
//! the commit before this work with a fake window (its size), a fake keyboard (its modifier state), a fake
//! `UIControl::GetMouseLocation` (the case's point), a logging `UIControl::CaptureMouse`, `GERectangle::Contains`
//! as `GEBitmap.cpp` has it and a logging `EEUserInterface::PostAction` (the action's mode, point and modifiers),
//! and called the press by its decorated name (it is virtual; the zeroed control has no vtable). From an LCG
//! seeded 1 it drew 2,000 cases (the centre driver's stream plus inverted bounds, where the press's two tests per
//! axis differ from the centre's `else if`, and the modifiers) and printed per case an FNV-1a 64 of its log. This
//! test draws the same stream, runs each case through `minimap.rs` and compares the digests.

use ee_ge::types::{Ge2DPoint, GeRectangle};

use crate::golden_mouse::{check, Log, Rng};
use crate::minimap::{center_point, press_point};

const GOLDEN: &str = include_str!("golden_press.txt");

#[test]
fn the_press_matches_the_cpp_golden() {
    const WIDTHS: [u32; 8] = [640, 800, 1024, 1280, 1600, 1920, 2560, 1];
    const HEIGHTS: [u32; 8] = [480, 600, 768, 1024, 1080, 1200, 1440, 1];
    let mut golden = GOLDEN.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    let mut differ = 0;
    for n in 0..2000 {
        let w = WIDTHS[(g.r() % 8) as usize];
        let h = HEIGHTS[(g.r() % 8) as usize];
        let ox = g.f(0.0, 1800.0);
        let oy = g.f(0.0, 1200.0);
        let mut left = g.f(-20.0, 40.0);
        let mut top = g.f(-20.0, 40.0);
        let mut right = left + if g.b(5) { 0.0 } else { g.f(1.0, 200.0) };
        let mut bottom = top + if g.b(5) { 0.0 } else { g.f(1.0, 160.0) };
        if g.b(8) {
            std::mem::swap(&mut left, &mut right);
        }
        if g.b(8) {
            std::mem::swap(&mut top, &mut bottom);
        }
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
        let mods = g.r() % 4;
        log.t("in");
        log.i(w as i64);
        log.i(h as i64);
        for v in [ox, oy, left, top, right, bottom, mx, my] {
            log.h(v);
        }
        log.i(mods as i64);
        let mouse = Ge2DPoint { x: mx, y: my };
        let origin = Ge2DPoint { x: ox, y: oy };
        let p = press_point(mouse, origin, w, h, &bounds);
        if p != center_point(mouse, origin, w, h, &bounds) {
            differ += 1;
        }
        log.t("post");
        log.i(1);
        log.p2(p);
        log.i(mods as i64);
        log.t("capture");
        log.i(0);
        check(&mut golden, "rbutton", n, &mut log);
    }
    assert!(golden.next().is_none());
    // the stream reaches the clamp where the two functions part
    assert!(differ > 20, "{differ}");
}
