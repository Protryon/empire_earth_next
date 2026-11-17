//! WONGUI's graphics context on the platform layer as Rust (R6 WON round 40): every drawing hook of the C++
//! `PlatGraphics` (WONLobbyPlatform.h), which draws into a display context's software surface (`surface.rs`) with
//! the results the GDI backend had: lines include both end points, rectangles cover x..x+width-1, fills are solid,
//! FillRectBlend is AlphaBlend with a constant alpha, polygons fill by the alternate rule with the pen's outline,
//! ellipses lie in the box x..x+width-1, and the clip is the surface's.
//!
//! On a surface of pixel scale k (DisplayContext::mPixelScale) the same shapes cover the pixels of their logical
//! pixels: a logical pixel x is the surface's pixels `pixels(x, k) .. pixels(x + 1, k) - 1`, lines and outlines are
//! one logical pixel (about k pixels) wide; at k = 1 every result is GDI's.
//!
//! The C++ `PlatGraphics` keeps its members (the borrowed surface, the scale; the colour is Graphics' `mColor`) and
//! each hook is one forwarder to `ee_won_plat_gfx` with them.

use crate::surface::{EeWonSurface, EeWonSurfaceDraw as D, EeWonSurfaceField as F};

/// `PlatGUIPixels(v, k)`: a logical coordinate or size at a pixel scale (the one rounding every part of the backend
/// and the game uses).
pub fn pixels(v: i32, k: f32) -> i32 {
    if k == 1.0 {
        v
    } else {
        (v as f32 * k + 0.5).floor() as i32
    }
}

/// A hook (`ee_won_plat_gfx`; its arguments in the hook's order).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonPlatGfx {
    /// `GetPixelHook(a, b)`: the pixel as a COLORREF (0x00BBGGRR).
    GetPixel = 1,
    /// `SetPixelHook(a, b, c)`.
    SetPixel,
    /// `DrawLineHook(a, b, c, d)`.
    Line,
    /// `DrawRectHook(a, b, c, d)`.
    Rect,
    /// `FillRectHook(a, b, c, d)`.
    FillRect,
    /// `FillRectBlendHook(a, b, c, d, e)`.
    FillRectBlend,
    /// `DrawEllipseHook(a, b, c, d)`.
    Ellipse,
    /// `FillEllipseHook(a, b, c, d)`.
    FillEllipse,
    /// `DrawArcHook(a, b, c, d, e, f)`.
    Arc,
    /// `ClearClipRectHook()`.
    ClearClip,
    /// `SetClipRectHook(a, b, c, d)`.
    SetClip,
}

/// The graphics' state as a hook sees it.
pub struct Gfx<'a> {
    pub s: &'a mut EeWonSurface,
    pub k: f32,
    /// Graphics' `mColor`.
    pub color: u32,
}

/// GDI's `Rectangle(x, y, x+width, y+height)` normalised: right and bottom exclusive.
fn normalize(x: &mut i32, y: &mut i32, w: &mut i32, h: &mut i32) -> bool {
    if *w < 0 {
        *x = x.wrapping_add(*w);
        *w = w.wrapping_neg();
    }
    if *h < 0 {
        *y = y.wrapping_add(*h);
        *h = h.wrapping_neg();
    }
    *w > 0 && *h > 0
}

/// The outline width of an ellipse or arc at a pixel scale: as many boxes, each inset by one more pixel, as make one
/// logical pixel.
fn outline_width(k: f32) -> i32 {
    ((k + 0.5).floor() as i32).max(1)
}

impl Gfx<'_> {
    fn px(&self, v: i32) -> i32 {
        pixels(v, self.k)
    }

    fn has_pixels(&mut self) -> bool {
        !self.s.pixels().is_null()
    }

    /// `PrepareSurface`: the current colour (GDI drops the top byte of a plain colour); false without pixels.
    fn prepare(&mut self) -> bool {
        if !self.has_pixels() {
            return false;
        }
        self.s.draw(D::SetColor, (self.color & 0x00FF_FFFF) as i32, 0, 0, 0, 0, 0);
        true
    }

    fn fill(&mut self, x: i32, y: i32, w: i32, h: i32) {
        self.s.draw(D::FillRect, x, y, w, h, 0, 0);
    }

    /// The pixels of the logical rectangle (a width or height <= 0 covers nothing), within the clip.
    fn fill_logical(&mut self, x: i32, y: i32, w: i32, h: i32) {
        if w <= 0 || h <= 0 {
            return;
        }
        let (l, t) = (self.px(x), self.px(y));
        let (r, b) = (self.px(x.wrapping_add(w)), self.px(y.wrapping_add(h)));
        self.fill(l, t, r.wrapping_sub(l), b.wrapping_sub(t));
    }

    /// A line from logical pixel (x1, y1) to (x2, y2), both included, one logical pixel wide: straight lines are
    /// the rectangles of their pixels; another steps along its longer axis in surface pixels, each step a span of
    /// one logical pixel's size across it, centred on the line between the end pixels' centres.
    fn scaled_line(&mut self, x1: i32, y1: i32, x2: i32, y2: i32) {
        if y1 == y2 {
            self.fill_logical(x1.min(x2), y1, x2.wrapping_sub(x1).wrapping_abs().wrapping_add(1), 1);
            return;
        }
        if x1 == x2 {
            self.fill_logical(x1, y1.min(y2), 1, y2.wrapping_sub(y1).wrapping_abs().wrapping_add(1));
            return;
        }
        let k = self.k;
        let x_major = x2.wrapping_sub(x1).wrapping_abs() >= y2.wrapping_sub(y1).wrapping_abs();
        let (mut a1, mut a2, mut b1, mut b2) = if x_major { (x1, x2, y1, y2) } else { (y1, y2, x1, x2) };
        if a2 < a1 {
            std::mem::swap(&mut a1, &mut a2);
            std::mem::swap(&mut b1, &mut b2);
        }
        let (start, end) = (self.px(a1), self.px(a2.wrapping_add(1)));
        for p in start..end {
            let mut t = ((p as f32 + 0.5) / k - 0.5 - a1 as f32) / a2.wrapping_sub(a1) as f32;
            // the original's two tests (a NaN stays NaN, as clamp would keep it too)
            #[allow(clippy::manual_clamp)]
            {
                t = if t < 0.0 {
                    0.0
                } else if t > 1.0 {
                    1.0
                } else {
                    t
                };
            }
            let b = b1 as f32 + t * b2.wrapping_sub(b1) as f32;
            let from = (b * k + 0.5).floor() as i32;
            let mut to = ((b + 1.0) * k + 0.5).floor() as i32;
            if to <= from {
                to = from.wrapping_add(1);
            }
            if x_major {
                self.fill(p, from, 1, to.wrapping_sub(from));
            } else {
                self.fill(from, p, to.wrapping_sub(from), 1);
            }
        }
    }

    fn line(&mut self, x1: i32, y1: i32, x2: i32, y2: i32) {
        if self.k == 1.0 {
            self.s.draw(D::Line, x1, y1, x2, y2, 0, 0);
        } else {
            self.scaled_line(x1, y1, x2, y2);
        }
    }

    fn clip(&self) -> (i32, i32, i32, i32) {
        (self.s.field(F::ClipX1), self.s.field(F::ClipY1), self.s.field(F::ClipX2), self.s.field(F::ClipY2))
    }

    /// A rectangle scaled to the surface's pixels (`x`, `y`, `w`, `h` in place).
    fn scale_rect(&self, x: &mut i32, y: &mut i32, w: &mut i32, h: &mut i32) {
        if self.k != 1.0 {
            let (l, t) = (self.px(*x), self.px(*y));
            *w = self.px(x.wrapping_add(*w)).wrapping_sub(l);
            *h = self.px(y.wrapping_add(*h)).wrapping_sub(t);
            *x = l;
            *y = t;
        }
    }

    /// One hook.
    #[allow(clippy::too_many_arguments)]
    pub fn hook(&mut self, op: EeWonPlatGfx, a: i32, b: i32, c: i32, d: i32, e: i32, f: i32) -> u32 {
        use EeWonPlatGfx as G;
        let (mut x, mut y, mut w, mut h) = (a, b, c, d);
        match op {
            G::GetPixel => {
                let p = self.s.pixel(self.px(a), self.px(b));
                return ((p & 0xff) << 16) | (p & 0xff00) | ((p >> 16) & 0xff);
            }
            G::SetPixel => {
                if !self.has_pixels() {
                    return 0;
                }
                let color = (c as u32 & 0x00FF_FFFF) as i32;
                if self.k == 1.0 {
                    self.s.draw(D::SetPixel, a, b, color, 0, 0, 0);
                } else {
                    self.s.draw(D::SetColor, color, 0, 0, 0, 0, 0);
                    self.fill_logical(a, b, 1, 1);
                }
            }
            G::Line => {
                if self.prepare() {
                    self.line(a, b, c, d);
                }
            }
            G::Rect => {
                if !self.prepare() || !normalize(&mut x, &mut y, &mut w, &mut h) {
                    return 0;
                }
                if self.k == 1.0 {
                    self.s.draw(D::DrawRect, x, y, w, h, 0, 0);
                    return 0;
                }
                self.fill_logical(x, y, w, 1);
                if h > 1 {
                    self.fill_logical(x, y.wrapping_add(h).wrapping_sub(1), w, 1);
                }
                if h > 2 {
                    self.fill_logical(x, y.wrapping_add(1), 1, h.wrapping_sub(2));
                    if w > 1 {
                        self.fill_logical(x.wrapping_add(w).wrapping_sub(1), y.wrapping_add(1), 1, h.wrapping_sub(2));
                    }
                }
            }
            G::FillRect => {
                if !self.prepare() || !normalize(&mut x, &mut y, &mut w, &mut h) {
                    return 0;
                }
                if self.k == 1.0 {
                    self.fill(x, y, w, h);
                } else {
                    self.fill_logical(x, y, w, h);
                }
            }
            G::FillRectBlend => self.blend(x, y, w, h, e),
            G::Ellipse | G::FillEllipse | G::Arc => {
                if !self.prepare() || !normalize(&mut x, &mut y, &mut w, &mut h) {
                    return 0;
                }
                let kind = match op {
                    G::Ellipse => D::Ellipse,
                    G::FillEllipse => D::FillEllipse,
                    _ => D::Arc,
                };
                if self.k == 1.0 {
                    self.s.draw(kind, x, y, w, h, e, f);
                    return 0;
                }
                self.scale_rect(&mut x, &mut y, &mut w, &mut h);
                if kind == D::FillEllipse {
                    self.s.draw(kind, x, y, w, h, 0, 0);
                    return 0;
                }
                let n = outline_width(self.k);
                let mut i = 0;
                while i < n && w.wrapping_sub(2 * i) > 0 && h.wrapping_sub(2 * i) > 0 {
                    self.s.draw(kind, x + i, y + i, w - 2 * i, h - 2 * i, e, f);
                    i += 1;
                }
            }
            G::ClearClip => self.s.draw(D::ClearClipRect, 0, 0, 0, 0, 0, 0),
            G::SetClip => {
                self.scale_rect(&mut x, &mut y, &mut w, &mut h);
                let (x1, y1, x2, y2) = self.clip();
                let (l, t) = (x.max(x1), y.max(y1));
                let r = x.wrapping_add(w).wrapping_sub(1).min(x2);
                let btm = y.wrapping_add(h).wrapping_sub(1).min(y2);
                self.s.draw(
                    D::SetClipRect,
                    l,
                    t,
                    r.wrapping_sub(l).wrapping_add(1),
                    btm.wrapping_sub(t).wrapping_add(1),
                    0,
                    0,
                );
            }
        }
        0
    }

    /// The current colour over the rectangle at `amount` / 255 (AlphaBlend, SourceConstantAlpha, no source alpha).
    fn blend(&mut self, mut x: i32, mut y: i32, mut w: i32, mut h: i32, amount: i32) {
        if !self.has_pixels() || w <= 0 || h <= 0 {
            return;
        }
        self.scale_rect(&mut x, &mut y, &mut w, &mut h);
        let (cx1, cy1, cx2, cy2) = self.clip();
        let (x1, y1) = (x.max(cx1), y.max(cy1));
        let x2 = x.wrapping_add(w).wrapping_sub(1).min(cx2);
        let y2 = y.wrapping_add(h).wrapping_sub(1).min(cy2);
        if x2 < x1 || y2 < y1 {
            return;
        }
        let alpha = amount as u32 & 0xff;
        let inverse = 255 - alpha;
        let (red, green, blue) = ((self.color >> 16) & 0xff, (self.color >> 8) & 0xff, self.color & 0xff);
        let pitch = self.s.field(F::PixelsPerRow) as i64;
        let Some(px) = self.s.words_mut() else {
            return;
        };
        for j in y1..=y2 {
            for i in x1..=x2 {
                let at = j as i64 * pitch + i as i64;
                let Some(p) = usize::try_from(at).ok().and_then(|at| px.get_mut(at)) else {
                    continue;
                };
                let v = *p;
                let r = (red * alpha + ((v >> 16) & 0xff) * inverse + 127) / 255;
                let g = (green * alpha + ((v >> 8) & 0xff) * inverse + 127) / 255;
                let b = (blue * alpha + (v & 0xff) * inverse + 127) / 255;
                *p = (v & 0xff00_0000) | (r << 16) | (g << 8) | b;
            }
        }
    }

    /// `DrawPolygonHook`: a closed outline (Polygon with the hollow brush).
    pub fn polygon(&mut self, pts: &[(i32, i32)]) {
        if !self.prepare() || pts.len() < 2 {
            return;
        }
        for i in 0..pts.len() {
            let (a, b) = (pts[i], pts[(i + 1) % pts.len()]);
            self.line(a.0, a.1, b.0, b.1);
        }
    }

    /// `FillPolygonHook`: the pixels whose centres lie inside by the alternate (even-odd) rule, then the pen's
    /// outline (at a pixel scale the fill runs over the surface's pixels with the corners scaled).
    pub fn fill_polygon(&mut self, outline: &[(i32, i32)]) {
        if !self.prepare() || outline.len() < 2 {
            return;
        }
        let pts: Vec<(i32, i32)> = if self.k != 1.0 {
            outline.iter().map(|&(x, y)| (self.px(x), self.px(y))).collect()
        } else {
            outline.to_vec()
        };
        let n = pts.len();
        let mut min_y = pts[0].1;
        let mut max_y = pts[0].1;
        for p in &pts[1..] {
            min_y = min_y.min(p.1);
            max_y = max_y.max(p.1);
        }
        min_y = min_y.max(self.s.field(F::ClipY1));
        max_y = max_y.min(self.s.field(F::ClipY2));
        let mut crossings: Vec<f64> = Vec::new();
        let mut y = min_y;
        while y <= max_y {
            let centre = y as f64 + 0.5;
            crossings.clear();
            for i in 0..n {
                let (a, b) = (pts[i], pts[(i + 1) % n]);
                if a.1 == b.1 {
                    continue;
                }
                let (low, high) = (a.1.min(b.1) as f64, a.1.max(b.1) as f64);
                if centre < low || centre >= high {
                    continue;
                }
                crossings.push(
                    a.0 as f64
                        + (centre - a.1 as f64) * (b.0.wrapping_sub(a.0)) as f64 / (b.1.wrapping_sub(a.1)) as f64,
                );
            }
            crossings.sort_by(|p, q| p.partial_cmp(q).unwrap_or(std::cmp::Ordering::Equal));
            let mut i = 0;
            while i + 1 < crossings.len() {
                let left = (crossings[i] - 0.5).ceil() as i32;
                let right = ((crossings[i + 1] - 0.5).ceil() as i32).wrapping_sub(1);
                if right >= left {
                    self.s.draw(D::HorzLine, left, right, y, 0, 0, 0);
                }
                i += 2;
            }
            if y == i32::MAX {
                break;
            }
            y += 1;
        }
        self.polygon(outline);
    }
}

/// A `PlatGraphics` hook on the surface `s` (null: the graphics has none) at pixel scale `scale`, with Graphics'
/// colour `color`; GetPixel's answer (0 for the others).
///
/// # Safety
/// `s` is null or a live surface no one else uses during the call.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_plat_gfx(
    s: *mut EeWonSurface,
    scale: f32,
    color: u32,
    op: EeWonPlatGfx,
    a: i32,
    b: i32,
    c: i32,
    d: i32,
    e: i32,
    f: i32,
) -> u32 {
    if s.is_null() {
        return 0;
    }
    // SAFETY: the contract
    let mut g = Gfx { s: unsafe { &mut *s }, k: scale, color };
    g.hook(op, a, b, c, d, e, f)
}

/// `DrawPolygonHook` (`fill` false) or `FillPolygonHook` over `count` points, `xy` their x and y in turn.
///
/// # Safety
/// `s` as `ee_won_plat_gfx`; `xy` holds `2 * count` ints (or count <= 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_gfx_polygon(
    s: *mut EeWonSurface,
    scale: f32,
    color: u32,
    fill: bool,
    xy: *const i32,
    count: i32,
) {
    if s.is_null() {
        return;
    }
    let pts: Vec<(i32, i32)> = if count > 0 && !xy.is_null() {
        // SAFETY: the contract
        let v = unsafe { std::slice::from_raw_parts(xy, count as usize * 2) };
        v.as_chunks::<2>().0.iter().map(|p| (p[0], p[1])).collect()
    } else {
        Vec::new()
    };
    // SAFETY: the contract
    let mut g = Gfx { s: unsafe { &mut *s }, k: scale, color };
    if fill {
        g.fill_polygon(&pts);
    } else {
        g.polygon(&pts);
    }
}

#[cfg(test)]
#[path = "platgfx_tests.rs"]
mod tests;
