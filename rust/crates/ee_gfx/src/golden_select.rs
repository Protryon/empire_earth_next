//! The box selection's rectangle against the unmodified C++ (`golden_select.txt`). A scratch driver
//! (`build/gx10_golden`, `driver_select.cpp`; PORT_LEARNINGS "R4.4, fourth round") #included `EEUICSelectUnits.cpp`
//! of the commit before this work over the real GE math (`GE2DPoint::DistanceTo` and `GERectangle::And` through
//! `ee_ge`), built the control with its own constructor over a no-op `UIControl` one, and logged every member it
//! calls (`SetRectangle` and `SetDisplayMode` store what they set), the dirty rectangles, the mouse's bounds, the
//! lasso's and the click's selections, the lines' points and the rasterizer's draw. From an LCG seeded 1 it drew 600
//! presses with up to four moves each (inside, on an edge of and off the game view), a copy and a draw after half
//! of them, and the release, the control's state running on across the cases, and printed per case an FNV-1a 64 of
//! its log. This test draws the same stream, runs each case through `select.rs` and the C++ glue rewritten here and
//! compares the digests.

use ee_ge::types::{Ge2DPoint, GeRectangle};

use crate::golden_mouse::{check, Log, Rng};
use crate::select::*;

const GOLDEN: &str = include_str!("golden_select.txt");

/// The driver's `RandomPoint`: inside the view (a fifth at a half pixel), outside, or on an edge.
fn random_point(g: &mut Rng, v: &GeRectangle) -> Ge2DPoint {
    let k = g.r() % 10;
    let (mut x, mut y);
    if k < 6 {
        x = g.f(v.left, v.right);
        y = g.f(v.top, v.bottom);
        if g.b(20) {
            x = x.floor() + 0.5;
        }
        if g.b(20) {
            y = y.floor() + 0.5;
        }
    } else if k < 8 {
        x = g.f(v.left - 300.0, v.right + 300.0);
        y = g.f(v.top - 300.0, v.bottom + 300.0);
    } else {
        let e = g.r() % 4;
        x = g.f(v.left, v.right);
        y = g.f(v.top, v.bottom);
        match e {
            0 => x = v.left,
            1 => x = v.right,
            2 => y = v.top,
            _ => y = v.bottom - g.f(0.0, 1.5),
        }
    }
    Ge2DPoint { x, y }
}

#[derive(Default)]
struct Control {
    rect: GeRectangle,
    mode: u32,
    start: Ge2DPoint,
    end: Ge2DPoint,
    dragging: bool,
    drawn: bool,
    copied: GeRectangle,
}

impl Control {
    fn set_rect(&mut self, log: &mut Log, r: GeRectangle) {
        log.t("setrect");
        log.rect(&r);
        self.rect = r;
    }
    fn set_mode(&mut self, log: &mut Log, mode: u32) {
        log.t("mode");
        log.i(mode as i64);
        self.mode = mode;
    }
}

#[test]
fn the_box_selection_matches_the_cpp_golden() {
    let mut golden = GOLDEN.lines();
    let mut g = Rng(1);
    let mut log = Log::default();
    let mut c = Control::default();
    for n in 0..600 {
        let w = g.pick(&[640.0, 800.0, 1024.0, 1920.0]);
        let h = g.pick(&[480.0, 600.0, 768.0, 1080.0]);
        let top = if g.b(30) { g.f(0.0, 60.0) } else { 0.0 };
        let bottom = if g.b(50) { h } else { h * 0.7135417 };
        let view = GeRectangle { left: 0.0, top, right: w, bottom };
        let modifiers = g.r() % 4;
        c.dragging = false;
        c.drawn = false;
        c.mode = 0;
        let press = random_point(&mut g, &view);
        log.t("in");
        for v in [w, h, top, bottom, press.x, press.y] {
            log.h(v);
        }
        // StartSelection
        c.start = start_point(press);
        log.t("bounds");
        log.rect(&view);
        c.set_rect(&mut log, GeRectangle { left: c.start.x, top: c.start.y, right: c.start.x, bottom: c.start.y });
        log.t("capkey capmouse relkey");
        c.set_mode(&mut log, 1);
        log.t("update");
        log.p2(press);
        for _ in 0..g.r() % 5 {
            let to = random_point(&mut g, &view);
            log.t("move");
            log.p2(to);
            // MouseMove
            let old = c.end;
            c.end = drag_end(to, &view);
            if !c.dragging && c.mode == 1 && starts_drag(c.start, c.end) {
                c.dragging = true;
            }
            if old != c.end {
                let r = drag_box(c.start, c.end);
                c.set_rect(&mut log, r);
                log.t("dirty");
                log.rect(&c.rect);
                log.t("redraw");
                log.rect(&c.rect);
            }
            log.p2(c.end);
            log.i(c.dragging as i64);
            if g.b(50) {
                // CopyData
                log.t("copy");
                if c.dragging {
                    c.copied = c.rect;
                    c.copied.and_in_place(&view);
                    log.t("dirty");
                    log.rect(&c.copied);
                    log.t("redraw");
                    log.rect(&c.copied);
                    c.drawn = true;
                } else {
                    c.drawn = false;
                }
                log.t("basecopy");
                log.i(c.drawn as i64);
                log.rect(&c.copied);
                // DrawPolygons
                log.t("draw");
                if c.drawn {
                    for (i, line) in box_lines(&c.copied).points.iter().enumerate() {
                        for (k, p) in line.iter().enumerate() {
                            log.t("P");
                            log.i(i as i64);
                            log.i(k as i64);
                            log.p2(*p);
                            log.h(0.0);
                        }
                    }
                    log.t("R20 1 8 0");
                }
                log.t("basedraw");
            }
        }
        let mouse = random_point(&mut g, &view);
        log.t("up");
        log.p2(mouse);
        // MouseLButtonUp
        if c.dragging {
            c.drawn = false;
            let r = release_box(c.start, c.end);
            log.t("lasso");
            log.rect(&r);
            log.i(modifiers as i64);
            log.i(u32::MAX as i64);
            log.i(0);
        } else if mouse.x >= view.left && mouse.x <= view.right && mouse.y >= view.top && mouse.y <= view.bottom {
            log.t("click");
            log.i(modifiers as i64);
        }
        log.t("bounds");
        log.rect(&GeRectangle::default());
        c.set_mode(&mut log, 0);
        log.t("relmouse");
        c.dragging = false;
        log.p2(c.start);
        check(&mut golden, "select", n, &mut log);
    }
    assert!(golden.next().is_none());
}
