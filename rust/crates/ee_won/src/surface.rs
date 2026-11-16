//! WONGUI's software raster surface as a whole Rust object (R6 WON round 37): the WON SDK's Surface.cpp
//! (`SurfaceBase` and its `Surface<PixelType>` template, `Surface32` and `Surface16`), the pixel buffer every lobby
//! window, offscreen image and native image draws into (WONLobbyPlatform.h: the port's Graphics, images and fonts
//! draw into these surfaces). The surface holds its format, size, row pitch, pixels, clip rectangle and colour, and
//! every drawing method: rectangles, horizontal and vertical lines, the line with its clipped walk, the midpoint
//! circle, ellipse and arc, pixels, the copies from another surface (with a transparency mask) and the duplicate.
//!
//! The C++ `SurfaceBase` (include/ee/wonapi/WONSurface.h) is a reference-counted shell over this object; its
//! methods are one-line forwarders. The pixels stay where the C++ reads and writes them directly
//! (`GetPixelsBase`: PlatGraphics' blends, PlatNativeImage's draws, PlatGUIFont's glyphs): one buffer, `u32`-aligned,
//! rows `bytes_per_row` bytes apart, never moved but by `create`, `release` and the drop.
//!
//! Ints wrap as the original's did (the game builds with -fwrapv); the ellipse and arc walks are the original's
//! `double` arithmetic in its order, their points truncated to int as C converts them. Where the original wrote
//! through a pointer outside the buffer (a source rectangle outside the source, a pixel past a clip rectangle the
//! caller never clamped) this writes nothing and reads 0. The original's commented-out `DrawString` (over an `SFont`
//! this SDK never had) and its unused `my_round` were not code; they are not ported.

use std::ffi::c_void;
use std::sync::atomic::{AtomicI32, Ordering};

/// `SurfaceFormat`.
pub const EE_WON_SURFACE_32: i32 = 0;
pub const EE_WON_SURFACE_555: i32 = 1;
pub const EE_WON_SURFACE_565: i32 = 2;

/// A surface's field (`ee_won_surface_get`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonSurfaceField {
    /// `GetSurfaceFormat()`.
    Format = 1,
    Width,
    Height,
    BytesPerRow,
    PixelsPerRow,
    /// `GetPixelSize()` (4 or 2).
    PixelSize,
    ClipX1,
    ClipY1,
    ClipX2,
    ClipY2,
    /// `GetColor()` (the pixel value: a 16-bit surface's converted colour).
    Color,
}

/// A drawing method (`ee_won_surface_draw`; its arguments in the original's order).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonSurfaceDraw {
    /// `SetColor(a)`.
    SetColor = 1,
    /// `FillRect(a, b, c, d)`.
    FillRect,
    /// `DrawRect(a, b, c, d)`.
    DrawRect,
    /// `DrawHorzLine(a, b, c)` (x1, x2, y).
    HorzLine,
    /// `DrawVertLine(a, b, c)` (x, y1, y2).
    VertLine,
    /// `DrawLine(a, b, c, d)`.
    Line,
    /// `DrawCircle(a, b, c)`.
    Circle,
    /// `FillCircle(a, b, c)`.
    FillCircle,
    /// `DrawEllipse(a, b, c, d)`.
    Ellipse,
    /// `FillEllipse(a, b, c, d)`.
    FillEllipse,
    /// `DrawArc(a, b, c, d, e, f)`.
    Arc,
    /// `PutPixel(a, b)`.
    PutPixel,
    /// `SetPixel(a, b, c)`.
    SetPixel,
    /// `SetClipRect(a, b, c, d)`.
    SetClipRect,
    /// `ClearClipRect()`.
    ClearClipRect,
}

/// `SetDefaultSurfaceFormat`'s format (`gDefaultSurfaceFormat`).
static DEFAULT_FORMAT: AtomicI32 = AtomicI32::new(EE_WON_SURFACE_32);

/// `SurfaceColorFrom32`: a 0x00RRGGBB colour in the format's pixel value.
pub fn color_from_32(c: u32, format: i32) -> u32 {
    match format {
        EE_WON_SURFACE_555 => ((c >> 3) & 0x1f) | (((c >> 11) & 0x1f) << 5) | (((c >> 19) & 0x1f) << 10),
        EE_WON_SURFACE_565 => ((c >> 3) & 0x1f) | (((c >> 10) & 0x3f) << 5) | (((c >> 19) & 0x1f) << 11),
        _ => c,
    }
}

/// `SURFACE_555_TO_32` / `SURFACE_565_TO_32`: a pixel value back as a 0x00RRGGBB colour.
pub fn color_to_32(p: u32, format: i32) -> u32 {
    match format {
        EE_WON_SURFACE_555 => ((p & 0x1f) << 3) | (((p >> 5) & 0x1f) << 11) | (((p >> 10) & 0x1f) << 19),
        EE_WON_SURFACE_565 => ((p & 0x1f) << 3) | (((p >> 5) & 0x3f) << 10) | (((p >> 11) & 0x1f) << 19),
        _ => p,
    }
}

/// A pixel type (`unsigned short` or `DWORD`).
trait Px: Copy {
    fn of(v: u32) -> Self;
    fn get(self) -> u32;
}
impl Px for u32 {
    fn of(v: u32) -> Self {
        v
    }
    fn get(self) -> u32 {
        self
    }
}
impl Px for u16 {
    fn of(v: u32) -> Self {
        v as u16
    }
    fn get(self) -> u32 {
        self as u32
    }
}

/// The clip rectangle and the row pitch: what the walks read.
#[derive(Clone, Copy, Debug, Default)]
struct Clip {
    x1: i32,
    y1: i32,
    x2: i32,
    y2: i32,
}

impl Clip {
    fn not_clipped(&self, x: i32, y: i32) -> bool {
        x >= self.x1 && y >= self.y1 && x <= self.x2 && y <= self.y2
    }
    /// `CalcClipCode`.
    fn code(&self, x: i32, y: i32) -> i32 {
        let mut c = 0;
        if x < self.x1 {
            c |= 1;
        }
        if y < self.y1 {
            c |= 2;
        }
        if x > self.x2 {
            c |= 4;
        }
        if y > self.y2 {
            c |= 8;
        }
        c
    }
    /// `DoClipRect(x, y, width, height)`.
    fn rect(&self, x: &mut i32, y: &mut i32, w: &mut i32, h: &mut i32) {
        let mut x2 = x.wrapping_add(*w).wrapping_sub(1);
        let mut y2 = y.wrapping_add(*h).wrapping_sub(1);
        if *x < self.x1 {
            *x = self.x1;
        }
        if x2 > self.x2 {
            x2 = self.x2;
        }
        if *y < self.y1 {
            *y = self.y1;
        }
        if y2 > self.y2 {
            y2 = self.y2;
        }
        *w = x2.wrapping_sub(*x).wrapping_add(1);
        *h = y2.wrapping_sub(*y).wrapping_add(1);
    }
    /// `DoClipRect(x, y, left, top, width, height)`: the source corner follows a clipped destination corner.
    #[allow(clippy::too_many_arguments)]
    fn rect_src(&self, x: &mut i32, y: &mut i32, left: &mut i32, top: &mut i32, w: &mut i32, h: &mut i32) {
        let mut x2 = x.wrapping_add(*w).wrapping_sub(1);
        let mut y2 = y.wrapping_add(*h).wrapping_sub(1);
        if *x < self.x1 {
            *left = left.wrapping_add(self.x1.wrapping_sub(*x));
            *x = self.x1;
        }
        if x2 > self.x2 {
            x2 = self.x2;
        }
        if *y < self.y1 {
            *top = top.wrapping_add(self.y1.wrapping_sub(*y));
            *y = self.y1;
        }
        if y2 > self.y2 {
            y2 = self.y2;
        }
        *w = x2.wrapping_sub(*x).wrapping_add(1);
        *h = y2.wrapping_sub(*y).wrapping_add(1);
    }
}

/// The drawing walks over one pixel type: the pixels, the row pitch in pixels, the clip and the colour.
struct Canvas<'a, P: Px> {
    px: &'a mut [P],
    ppr: i32,
    clip: Clip,
    color: P,
}

/// An index `y * ppr + x` into the pixels (None outside the buffer).
fn at(len: usize, ppr: i32, x: i32, y: i32) -> Option<usize> {
    let i = (y as i64) * (ppr as i64) + x as i64;
    if i >= 0 && (i as u64) < len as u64 {
        Some(i as usize)
    } else {
        None
    }
}

impl<P: Px> Canvas<'_, P> {
    fn put_index(&mut self, i: i64) {
        if i >= 0 && (i as u64) < self.px.len() as u64 {
            self.px[i as usize] = self.color;
        }
    }
    fn put_raw(&mut self, x: i32, y: i32, v: P) {
        if let Some(i) = at(self.px.len(), self.ppr, x, y) {
            self.px[i] = v;
        }
    }
    /// `n` pixels from (x, y) in the colour (the part inside the buffer).
    fn span(&mut self, x: i32, y: i32, n: i32) {
        let i = (y as i64) * (self.ppr as i64) + x as i64;
        let (from, to) = (i.max(0), (i + n as i64).min(self.px.len() as i64));
        if from < to {
            let c = self.color;
            self.px[from as usize..to as usize].fill(c);
        }
    }
    /// `PutPixel(x, y)`.
    fn put(&mut self, x: i32, y: i32) {
        if self.clip.not_clipped(x, y) {
            let c = self.color;
            self.put_raw(x, y, c);
        }
    }
    /// `PutPixel` of a `double` point, converted to int as C converts it (toward zero).
    fn put_f(&mut self, x: f64, y: f64) {
        self.put(x as i32, y as i32);
    }

    fn fill_rect(&mut self, mut x: i32, mut y: i32, mut w: i32, mut h: i32) {
        self.clip.rect(&mut x, &mut y, &mut w, &mut h);
        if w <= 0 || h <= 0 {
            return;
        }
        for i in 0..h {
            self.span(x, y.wrapping_add(i), w);
        }
    }

    fn draw_rect(&mut self, x: i32, y: i32, w: i32, h: i32) {
        let x2 = x.wrapping_add(w).wrapping_sub(1);
        let y2 = y.wrapping_add(h).wrapping_sub(1);
        if w == 1 {
            self.vert_line(x, y, y2);
            return;
        }
        if h == 1 {
            self.horz_line(x, x2, y);
            return;
        }
        self.horz_line(x, x2, y);
        self.horz_line(x, x2, y2);
        self.vert_line(x, y, y2);
        self.vert_line(x2, y, y2);
    }

    fn horz_line(&mut self, mut x1: i32, mut x2: i32, y: i32) {
        if y < self.clip.y1 || y > self.clip.y2 {
            return;
        }
        if x2 < x1 {
            std::mem::swap(&mut x1, &mut x2);
        }
        if x1 < self.clip.x1 {
            x1 = self.clip.x1;
        }
        if x2 > self.clip.x2 {
            x2 = self.clip.x2;
        }
        if x2 < x1 {
            return;
        }
        self.span(x1, y, x2.wrapping_sub(x1).wrapping_add(1));
    }

    fn vert_line(&mut self, x: i32, mut y1: i32, mut y2: i32) {
        if x < self.clip.x1 || x > self.clip.x2 {
            return;
        }
        if y2 < y1 {
            std::mem::swap(&mut y1, &mut y2);
        }
        if y1 < self.clip.y1 {
            y1 = self.clip.y1;
        }
        if y2 > self.clip.y2 {
            y2 = self.clip.y2;
        }
        if y2 < y1 {
            return;
        }
        let n = y2.wrapping_sub(y1).wrapping_add(1);
        for i in 0..n {
            let c = self.color;
            self.put_raw(x, y1.wrapping_add(i), c);
        }
    }

    /// `DrawLine`: the vertical and horizontal cases, the clipped walk when an end is outside the clip (nothing when
    /// both are outside on one side), else the walk along the longer axis with both ends drawn.
    fn line(&mut self, x1: i32, y1: i32, x2: i32, y2: i32) {
        if x1 == x2 {
            self.vert_line(x1, y1, y2);
            return;
        }
        if y1 == y2 {
            self.horz_line(x1, x2, y1);
            return;
        }
        let c1 = self.clip.code(x1, y1);
        let c2 = self.clip.code(x2, y2);
        if c1 != 0 || c2 != 0 {
            if c1 & c2 == 0 {
                self.line_clip(x1, y1, x2, y2);
            }
            return;
        }
        let mut accum = 0i32;
        let mut width = x2.wrapping_sub(x1);
        let mut height = y2.wrapping_sub(y1);
        let xinc: i64 = if width > 0 { 1 } else { -1 };
        let yinc: i64 = if height > 0 { self.ppr as i64 } else { -(self.ppr as i64) };
        if width < 0 {
            width = width.wrapping_neg();
        }
        if height < 0 {
            height = height.wrapping_neg();
        }
        let mut p = (y1 as i64) * (self.ppr as i64) + x1 as i64;
        if width < height {
            for _ in 0..height {
                self.put_index(p);
                p += yinc;
                accum = accum.wrapping_add(width);
                if accum >= height {
                    p += xinc;
                    accum = accum.wrapping_sub(height);
                }
            }
        } else {
            for _ in 0..width {
                self.put_index(p);
                p += xinc;
                accum = accum.wrapping_add(height);
                if accum >= width {
                    p += yinc;
                    accum = accum.wrapping_sub(width);
                }
            }
        }
        self.put_index(p);
    }

    /// `DrawLineClip`: the same walk, each point drawn only inside the clip.
    fn line_clip(&mut self, x1: i32, y1: i32, x2: i32, y2: i32) {
        let mut accum = 0i32;
        let mut width = x2.wrapping_sub(x1);
        let mut height = y2.wrapping_sub(y1);
        let xi = if width > 0 { 1 } else { -1 };
        let yi = if height > 0 { 1 } else { -1 };
        if width < 0 {
            width = width.wrapping_neg();
        }
        if height < 0 {
            height = height.wrapping_neg();
        }
        let (mut x, mut y) = (x1, y1);
        if width < height {
            for _ in 0..height {
                self.put(x, y);
                y = y.wrapping_add(yi);
                accum = accum.wrapping_add(width);
                if accum < height {
                    continue;
                }
                x = x.wrapping_add(xi);
                accum = accum.wrapping_sub(height);
            }
        } else {
            for _ in 0..width {
                self.put(x, y);
                x = x.wrapping_add(xi);
                accum = accum.wrapping_add(height);
                if accum < width {
                    continue;
                }
                y = y.wrapping_add(yi);
                accum = accum.wrapping_sub(width);
            }
        }
        self.put(x, y);
    }

    /// The midpoint circle's next step (`d` the decision variable).
    fn circle_step(d: &mut i32, x1: &mut i32, y1: &mut i32) {
        if *d > 0 {
            *d = d.wrapping_add(8i32.wrapping_mul(y1.wrapping_sub(*x1))).wrapping_add(4);
            *x1 = x1.wrapping_sub(1);
            *y1 = y1.wrapping_add(1);
        } else {
            *d = d.wrapping_add(8i32.wrapping_mul(*y1)).wrapping_add(4);
            *y1 = y1.wrapping_add(1);
        }
    }

    fn circle(&mut self, x: i32, y: i32, r: i32) {
        let (mut x1, mut y1) = (r, 0i32);
        let mut d = 5i32.wrapping_sub(4i32.wrapping_mul(r));
        while x1 >= y1 {
            self.put(x.wrapping_add(x1), y.wrapping_sub(y1));
            self.put(x.wrapping_add(y1), y.wrapping_sub(x1));
            self.put(x.wrapping_sub(y1), y.wrapping_sub(x1));
            self.put(x.wrapping_sub(x1), y.wrapping_sub(y1));
            self.put(x.wrapping_sub(x1), y.wrapping_add(y1));
            self.put(x.wrapping_sub(y1), y.wrapping_add(x1));
            self.put(x.wrapping_add(y1), y.wrapping_add(x1));
            self.put(x.wrapping_add(x1), y.wrapping_add(y1));
            Self::circle_step(&mut d, &mut x1, &mut y1);
        }
    }

    fn fill_circle(&mut self, x: i32, y: i32, r: i32) {
        let (mut x1, mut y1) = (r, 0i32);
        let mut d = 5i32.wrapping_sub(4i32.wrapping_mul(r));
        while x1 > y1 {
            self.horz_line(x.wrapping_sub(x1), x.wrapping_add(x1), y.wrapping_sub(y1));
            self.horz_line(x.wrapping_sub(x1), x.wrapping_add(x1), y.wrapping_add(y1));
            self.horz_line(x.wrapping_sub(y1), x.wrapping_add(y1), y.wrapping_sub(x1));
            self.horz_line(x.wrapping_sub(y1), x.wrapping_add(y1), y.wrapping_add(x1));
            Self::circle_step(&mut d, &mut x1, &mut y1);
        }
    }

    /// The midpoint ellipse walk in the original's `double`s: `first` for each point of the slope's first region
    /// (|slope| < 1, from (a, 0)), `second` for each of the rest down to x 0. The points are relative to the
    /// centre `(cx, cy)`, which the callbacks get too.
    fn ellipse_walk(
        &mut self,
        x: i32,
        y: i32,
        width: i32,
        height: i32,
        mut first: impl FnMut(&mut Self, f64, f64, f64, f64),
        mut second: impl FnMut(&mut Self, f64, f64, f64, f64),
    ) {
        let a = (width.wrapping_sub(1)) as f64 / 2.0;
        let b = (height.wrapping_sub(1)) as f64 / 2.0;
        let a2 = a * a;
        let b2 = b * b;
        let cx = x as f64 + a;
        let cy = y as f64 + b;
        let mut x1 = a;
        let mut y1 = 0.0f64;
        let mut d = 4.0 * a2 - 4.0 * b2 * a + b2;
        while a2 * y1 < b2 * x1 {
            first(self, cx, cy, x1, y1);
            if d > 0.0 {
                x1 -= 1.0;
                y1 += 1.0;
                d = d + 8.0 * (a2 * y1 - b2 * x1) + 4.0 * a2;
            } else {
                y1 += 1.0;
                d = d + 8.0 * a2 * y1 + 4.0 * a2;
            }
        }
        d = d + 4.0 * (-a2 * y1 - b2 * x1 - a2 + b2) + a2 - b2;
        while x1 >= 0.0 {
            second(self, cx, cy, x1, y1);
            if d < 0.0 {
                x1 -= 1.0;
                y1 += 1.0;
                d = d + 8.0 * (a2 * y1 - b2 * x1) + 4.0 * b2;
            } else {
                x1 -= 1.0;
                d = d - 8.0 * b2 * x1 + 4.0 * b2;
            }
        }
    }

    fn ellipse(&mut self, x: i32, y: i32, width: i32, height: i32) {
        let four = |s: &mut Self, cx: f64, cy: f64, x1: f64, y1: f64| {
            s.put_f(cx + x1, cy - y1);
            s.put_f(cx - x1, cy - y1);
            s.put_f(cx - x1, cy + y1);
            s.put_f(cx + x1, cy + y1);
        };
        self.ellipse_walk(x, y, width, height, four, four);
    }

    fn fill_ellipse(&mut self, x: i32, y: i32, width: i32, height: i32) {
        let two = |s: &mut Self, cx: f64, cy: f64, x1: f64, y1: f64| {
            s.horz_line((cx - x1) as i32, (cx + x1) as i32, (cy - y1) as i32);
            s.horz_line((cx - x1) as i32, (cx + x1) as i32, (cy + y1) as i32);
        };
        self.ellipse_walk(x, y, width, height, two, two);
    }

    fn arc(&mut self, x: i32, y: i32, width: i32, height: i32, start: i32, arc: i32) {
        if arc >= 360 {
            self.ellipse(x, y, width, height);
            return;
        }
        let o = Octants::new(width, height, start, arc);
        self.ellipse_walk(
            x,
            y,
            width,
            height,
            |s, cx, cy, x1, y1| {
                if o.0[0].include(x1, y1) {
                    s.put_f(cx + x1, cy - y1);
                }
                if o.0[3].include(-x1, y1) {
                    s.put_f(cx - x1, cy - y1);
                }
                if o.0[4].include(-x1, -y1) {
                    s.put_f(cx - x1, cy + y1);
                }
                if o.0[7].include(x1, -y1) {
                    s.put_f(cx + x1, cy + y1);
                }
            },
            |s, cx, cy, x1, y1| {
                if o.0[1].include(x1, y1) {
                    s.put_f(cx + x1, cy - y1);
                }
                if o.0[2].include(-x1, y1) {
                    s.put_f(cx - x1, cy - y1);
                }
                if o.0[5].include(-x1, -y1) {
                    s.put_f(cx - x1, cy + y1);
                }
                if o.0[6].include(x1, -y1) {
                    s.put_f(cx + x1, cy + y1);
                }
            },
        );
    }
}

/// `OctantEndPoint`: which points of one octant of the ellipse an arc draws (its limits on the octant's moving
/// axis; the arc's two ends where they fall in it). The original left the limits it did not set uninitialised and
/// never read them; they are 0 here.
#[derive(Clone, Copy, Debug, Default)]
struct OctantEnd {
    x1: f64,
    y1: f64,
    x2: f64,
    y2: f64,
    octant: i32,
    inverse: bool,
}

const BIG_VAL: f64 = 100000.0;

impl OctantEnd {
    fn include_all(&mut self) {
        match self.octant {
            0 | 7 => {
                self.y1 = -BIG_VAL;
                self.y2 = BIG_VAL;
            }
            1 | 2 => {
                self.x1 = BIG_VAL;
                self.x2 = -BIG_VAL;
            }
            3 | 4 => {
                self.y1 = BIG_VAL;
                self.y2 = -BIG_VAL;
            }
            5 | 6 => {
                self.x1 = -BIG_VAL;
                self.x2 = BIG_VAL;
            }
            _ => {}
        }
    }
    fn include_none(&mut self) {
        self.include_all();
        self.x1 *= -1.0;
        self.y1 *= -1.0;
        self.x2 *= -1.0;
        self.y2 *= -1.0;
    }
    fn include(&self, x: f64, y: f64) -> bool {
        let v = match self.octant {
            0 | 7 => y >= self.y1 && y <= self.y2,
            1 | 2 => x <= self.x1 && x >= self.x2,
            3 | 4 => y <= self.y1 && y >= self.y2,
            5 | 6 => x >= self.x1 && x <= self.x2,
            _ => false,
        };
        v != self.inverse
    }
}

/// The eight octants of `DrawArc`'s ellipse.
struct Octants([OctantEnd; 8]);

impl Octants {
    fn new(width: i32, height: i32, start: i32, arc: i32) -> Self {
        // the original's `static const double PI = 3.14159`, not the library's
        #[allow(clippy::approx_constant)]
        const PI: f64 = 3.14159;
        let mut start = start;
        if start < 0 {
            start = start.wrapping_add(360) % 360;
        } else if start > 360 {
            start %= 360;
        }
        let mut inverse = false;
        let mut end = start.wrapping_add(arc);
        if end > 360 {
            end -= 360;
            std::mem::swap(&mut start, &mut end);
            inverse = true;
        }
        let a = (width.wrapping_sub(1)) as f64 / 2.0;
        let b = (height.wrapping_sub(1)) as f64 / 2.0;
        let xe1 = a * (PI * start as f64 / 180.0).cos();
        let ye1 = b * (PI * start as f64 / 180.0).sin();
        let xe2 = a * (PI * end as f64 / 180.0).cos();
        let ye2 = b * (PI * end as f64 / 180.0).sin();
        let mut o = [OctantEnd::default(); 8];
        let (mut cs, mut ce) = (0i32, 45i32);
        for (i, e) in o.iter_mut().enumerate() {
            e.octant = i as i32;
            e.inverse = inverse;
            if ce < start || cs > end {
                e.include_none();
            } else {
                e.include_all();
                if cs < start {
                    e.x1 = xe1;
                    e.y1 = ye1;
                }
                if ce > end {
                    e.x2 = xe2;
                    e.y2 = ye2;
                }
            }
            cs += 45;
            ce += 45;
        }
        Octants(o)
    }
}

/// The Rust surface.
pub struct EeWonSurface {
    format: i32,
    /// `Surface32` (else `Surface16`).
    wide: bool,
    /// The pixels (`mImageBase`); None before `create` and after `release` (the original's NULL).
    buf: Option<Vec<u32>>,
    width: i32,
    height: i32,
    bytes_per_row: i32,
    pixels_per_row: i32,
    clip: Clip,
    /// `mColor` (truncated to the pixel type).
    color: u32,
}

impl EeWonSurface {
    /// `GetNewSurface(format)`: a `Surface32` for the 32-bit format, a `Surface16` for 555 and 565, None for
    /// another.
    pub fn new(format: i32) -> Option<Self> {
        let wide = match format {
            EE_WON_SURFACE_32 => true,
            EE_WON_SURFACE_555 | EE_WON_SURFACE_565 => false,
            _ => return None,
        };
        Some(Self::blank(format, wide))
    }

    /// The constructor's state: no pixels, size 0, the clip empty, colour 0.
    fn blank(format: i32, wide: bool) -> Self {
        let mut s = EeWonSurface {
            format,
            wide,
            buf: None,
            width: 0,
            height: 0,
            bytes_per_row: 0,
            pixels_per_row: 0,
            clip: Clip::default(),
            color: 0,
        };
        s.clear_clip();
        s
    }

    pub fn pixel_size(&self) -> i32 {
        if self.wide {
            4
        } else {
            2
        }
    }

    /// `Create`: the old pixels freed, rows padded to four bytes, the new pixels 0 (the original's were whatever
    /// `new` gave; the port's only caller cleared them), the clip the whole surface.
    pub fn create(&mut self, width: i32, height: i32) {
        self.release();
        let mut bpr = width.wrapping_mul(self.pixel_size());
        if bpr % 4 != 0 {
            bpr = bpr.wrapping_add(4 - bpr % 4);
        }
        self.bytes_per_row = bpr;
        self.pixels_per_row = bpr / self.pixel_size();
        let bytes = (bpr as i64) * (height as i64);
        let words = if bytes > 0 { ((bytes + 3) / 4) as usize } else { 0 };
        self.buf = Some(vec![0u32; words]);
        self.width = width;
        self.height = height;
        self.clear_clip();
    }

    /// `Free`: the pixels gone, the size and pitch 0 (the pixels per row kept, as the original kept them).
    pub fn release(&mut self) {
        self.buf = None;
        self.width = 0;
        self.height = 0;
        self.bytes_per_row = 0;
        self.clear_clip();
    }

    pub fn set_clip(&mut self, x: i32, y: i32, w: i32, h: i32) {
        let c = &mut self.clip;
        c.x1 = x;
        c.y1 = y;
        c.x2 = x.wrapping_add(w).wrapping_sub(1);
        c.y2 = y.wrapping_add(h).wrapping_sub(1);
        if c.x1 < 0 {
            c.x1 = 0;
        }
        if c.y1 < 0 {
            c.y1 = 0;
        }
        if c.x2 >= self.width {
            c.x2 = self.width.wrapping_sub(1);
        }
        if c.y2 >= self.height {
            c.y2 = self.height.wrapping_sub(1);
        }
    }

    pub fn clear_clip(&mut self) {
        self.clip = Clip { x1: 0, y1: 0, x2: self.width.wrapping_sub(1), y2: self.height.wrapping_sub(1) };
    }

    pub fn field(&self, f: EeWonSurfaceField) -> i32 {
        match f {
            EeWonSurfaceField::Format => self.format,
            EeWonSurfaceField::Width => self.width,
            EeWonSurfaceField::Height => self.height,
            EeWonSurfaceField::BytesPerRow => self.bytes_per_row,
            EeWonSurfaceField::PixelsPerRow => self.pixels_per_row,
            EeWonSurfaceField::PixelSize => self.pixel_size(),
            EeWonSurfaceField::ClipX1 => self.clip.x1,
            EeWonSurfaceField::ClipY1 => self.clip.y1,
            EeWonSurfaceField::ClipX2 => self.clip.x2,
            EeWonSurfaceField::ClipY2 => self.clip.y2,
            EeWonSurfaceField::Color => self.color as i32,
        }
    }

    /// The pixels' address (`GetPixelsBase`), null without pixels.
    pub fn pixels(&mut self) -> *mut c_void {
        match &mut self.buf {
            Some(b) => b.as_mut_ptr() as *mut c_void,
            None => std::ptr::null_mut(),
        }
    }

    /// The pixels as 32-bit words (None without pixels): a 32-bit surface's pixels, `pixels_per_row` to a row.
    pub fn words_mut(&mut self) -> Option<&mut [u32]> {
        self.buf.as_deref_mut()
    }

    /// The pixel bytes (empty without pixels).
    pub fn bytes(&self) -> &[u8] {
        match &self.buf {
            // SAFETY: a u32 buffer viewed as its bytes
            Some(b) => unsafe { std::slice::from_raw_parts(b.as_ptr() as *const u8, b.len() * 4) },
            None => &[],
        }
    }

    fn canvas<P: Px>(&mut self) -> Canvas<'_, P> {
        let px: &mut [P] = match &mut self.buf {
            Some(b) => {
                let n = b.len() * 4 / std::mem::size_of::<P>();
                // SAFETY: the u32 buffer viewed as its u16 or u32 pixels (u32-aligned, `n` of them)
                unsafe { std::slice::from_raw_parts_mut(b.as_mut_ptr() as *mut P, n) }
            }
            None => &mut [],
        };
        Canvas { px, ppr: self.pixels_per_row, clip: self.clip, color: P::of(self.color) }
    }

    /// One drawing method (`EeWonSurfaceDraw` with its arguments).
    #[allow(clippy::too_many_arguments)]
    pub fn draw(&mut self, op: EeWonSurfaceDraw, a: i32, b: i32, c: i32, d: i32, e: i32, f: i32) {
        use EeWonSurfaceDraw as D;
        match op {
            D::SetColor => {
                let v = color_from_32(a as u32, self.format);
                self.color = if self.wide { v } else { v as u16 as u32 };
            }
            D::SetClipRect => self.set_clip(a, b, c, d),
            D::ClearClipRect => self.clear_clip(),
            D::SetPixel => {
                if self.clip.not_clipped(a, b) {
                    let v = color_from_32(c as u32, self.format);
                    if self.wide {
                        self.canvas::<u32>().put_raw(a, b, v);
                    } else {
                        self.canvas::<u16>().put_raw(a, b, v as u16);
                    }
                }
            }
            _ => {
                if self.wide {
                    Self::walk(&mut self.canvas::<u32>(), op, a, b, c, d, e, f);
                } else {
                    Self::walk(&mut self.canvas::<u16>(), op, a, b, c, d, e, f);
                }
            }
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn walk<P: Px>(cv: &mut Canvas<'_, P>, op: EeWonSurfaceDraw, a: i32, b: i32, c: i32, d: i32, e: i32, f: i32) {
        use EeWonSurfaceDraw as D;
        match op {
            D::FillRect => cv.fill_rect(a, b, c, d),
            D::DrawRect => cv.draw_rect(a, b, c, d),
            D::HorzLine => cv.horz_line(a, b, c),
            D::VertLine => cv.vert_line(a, b, c),
            D::Line => cv.line(a, b, c, d),
            D::Circle => cv.circle(a, b, c),
            D::FillCircle => cv.fill_circle(a, b, c),
            D::Ellipse => cv.ellipse(a, b, c, d),
            D::FillEllipse => cv.fill_ellipse(a, b, c, d),
            D::Arc => cv.arc(a, b, c, d, e, f),
            D::PutPixel => cv.put(a, b),
            _ => {}
        }
    }

    /// `GetPixel(x, y)`: 0 outside the surface.
    pub fn pixel(&mut self, x: i32, y: i32) -> u32 {
        if x < 0 || y < 0 || x >= self.width || y >= self.height {
            return 0;
        }
        if self.wide {
            let cv = self.canvas::<u32>();
            at(cv.px.len(), cv.ppr, x, y).map_or(0, |i| cv.px[i].get())
        } else {
            let cv = self.canvas::<u16>();
            at(cv.px.len(), cv.ppr, x, y).map_or(0, |i| cv.px[i].get())
        }
    }

    /// `Duplicate`: a surface of the same format and size with these pixels.
    pub fn duplicate(&self) -> EeWonSurface {
        let mut s = EeWonSurface::blank(self.format, self.wide);
        s.create(self.width, self.height);
        let n = (self.bytes_per_row as i64 * self.height as i64).max(0) as usize;
        if let (Some(to), Some(from)) = (&mut s.buf, &self.buf) {
            let k = n.min(to.len() * 4).min(from.len() * 4) / 4;
            to[..k].copy_from_slice(&from[..k]);
        }
        s
    }

    /// `CopySurface(src, mask, x, y, left, top, width, height)`: the source rectangle at (left, top) to (x, y),
    /// clipped; with a mask (one bit per source pixel, rows padded to bytes, a set bit transparent) the masked
    /// pixels keep the destination. The source's pixels are read as this surface's pixel type with the source's
    /// row pitch, as the original read them; `src` None copies within this surface (pixel by pixel in the
    /// original's order).
    ///
    /// # Safety
    /// `mask` is null or covers the source's rows (`(width + 7) / 8` bytes each).
    #[allow(clippy::too_many_arguments)]
    pub unsafe fn copy(
        &mut self,
        src: Option<&EeWonSurface>,
        mask: *const u8,
        x: i32,
        y: i32,
        left: i32,
        top: i32,
        w: i32,
        h: i32,
    ) {
        let (src_w, src_h, src_ppr, src_bytes): (i32, i32, i32, Option<&[u8]>) = match src {
            Some(s) => (s.width, s.height, s.pixels_per_row, Some(s.bytes())),
            None => (self.width, self.height, self.pixels_per_row, None),
        };
        // SAFETY: the contract
        unsafe {
            if self.wide {
                Self::copy_px::<u32>(
                    &mut self.canvas::<u32>(),
                    src_bytes,
                    src_w,
                    src_h,
                    src_ppr,
                    mask,
                    [x, y, left, top, w, h],
                );
            } else {
                Self::copy_px::<u16>(
                    &mut self.canvas::<u16>(),
                    src_bytes,
                    src_w,
                    src_h,
                    src_ppr,
                    mask,
                    [x, y, left, top, w, h],
                );
            }
        }
    }

    unsafe fn copy_px<P: Px>(
        cv: &mut Canvas<'_, P>,
        src: Option<&[u8]>,
        src_w: i32,
        src_h: i32,
        src_ppr: i32,
        mask: *const u8,
        r: [i32; 6],
    ) {
        let [mut x, mut y, mut left, mut top, mut w, mut h] = r;
        cv.clip.rect_src(&mut x, &mut y, &mut left, &mut top, &mut w, &mut h);
        if w <= 0 || h <= 0 {
            return;
        }
        let size = std::mem::size_of::<P>();
        let read = |cv: &Canvas<'_, P>, i: i64| -> P {
            match src {
                Some(b) => {
                    if i < 0 || (i as u64 + 1) * size as u64 > b.len() as u64 {
                        return P::of(0);
                    }
                    let o = i as usize * size;
                    let mut v = 0u32;
                    for (k, byte) in b[o..o + size].iter().enumerate() {
                        v |= (*byte as u32) << (8 * k);
                    }
                    P::of(v)
                }
                None => {
                    if i >= 0 && (i as u64) < cv.px.len() as u64 {
                        cv.px[i as usize]
                    } else {
                        P::of(0)
                    }
                }
            }
        };
        let src_row0 = src_ppr as i64 * top as i64 + left as i64;
        let dst_row0 = cv.ppr as i64 * y as i64 + x as i64;
        let mbpr: i64 = (src_w / 8 + if src_w % 8 != 0 { 1 } else { 0 }) as i64;
        let mask_len = mbpr * src_h as i64;
        let mask_start = 1i32.wrapping_shl((7 - left % 8) as u32) as u8;
        let mask_row0 = mbpr * top as i64 + (left / 8) as i64;
        for i in 0..h as i64 {
            let s = src_row0 + i * src_ppr as i64;
            let d = dst_row0 + i * cv.ppr as i64;
            let mut mp = mask_row0 + i * mbpr;
            let mut m = mask_start;
            for j in 0..w as i64 {
                let skip = !mask.is_null() && {
                    // SAFETY: the contract (the mask covers the source's rows); outside them nothing is read
                    let byte = if mp >= 0 && mp < mask_len { unsafe { *mask.add(mp as usize) } } else { 0 };
                    byte & m != 0
                };
                if !skip {
                    let v = read(cv, s + j);
                    let di = d + j;
                    if di >= 0 && (di as u64) < cv.px.len() as u64 {
                        cv.px[di as usize] = v;
                    }
                }
                if !mask.is_null() {
                    m >>= 1;
                    if m == 0 {
                        m = 128;
                        mp += 1;
                    }
                }
            }
        }
    }

    /// The BITMAPINFO header's fields the original's constructor and `Create` set under `WIN32_NOT_XBOX`
    /// (`BlitToDC`'s `SetDIBitsToDevice`): width, height (negative: top-down), planes, bit count, compression
    /// (0 BI_RGB, 3 BI_BITFIELDS) and the three colour masks of a 16-bit surface.
    pub fn bitmap_info(&self) -> [i32; 8] {
        let (bits, compression, masks) = if self.wide {
            (32, 0, [0, 0, 0])
        } else {
            (16, if self.format == EE_WON_SURFACE_565 { 3 } else { 0 }, [0xf800, 0x7e0, 0x1f])
        };
        [self.width, self.height.wrapping_neg(), 1, bits, compression, masks[0], masks[1], masks[2]]
    }
}

// ---- the C entry points (the C++ SurfaceBase's forwarders)

/// `SurfaceBase::GetNewSurface(format)`: a new surface, or null for an unknown format.
#[no_mangle]
pub extern "C" fn ee_won_surface_new(format: i32) -> *mut EeWonSurface {
    match EeWonSurface::new(format) {
        Some(s) => Box::into_raw(Box::new(s)),
        None => std::ptr::null_mut(),
    }
}

/// `GetNewSurface(gDefaultSurfaceFormat)`.
#[no_mangle]
pub extern "C" fn ee_won_surface_new_default() -> *mut EeWonSurface {
    ee_won_surface_new(DEFAULT_FORMAT.load(Ordering::Relaxed))
}

/// `SetDefaultSurfaceFormat(format)`.
#[no_mangle]
pub extern "C" fn ee_won_surface_set_default_format(format: i32) {
    DEFAULT_FORMAT.store(format, Ordering::Relaxed);
}

/// `SURFACE_32_TO_555` / `SURFACE_32_TO_565` (`SurfaceColorFrom32`).
#[no_mangle]
pub extern "C" fn ee_won_surface_color_from32(c: u32, format: i32) -> u32 {
    color_from_32(c, format)
}

/// `SURFACE_555_TO_32` / `SURFACE_565_TO_32`.
#[no_mangle]
pub extern "C" fn ee_won_surface_color_to32(p: u32, format: i32) -> u32 {
    color_to_32(p, format)
}

/// The surface's destruction (its pixels freed).
///
/// # Safety
/// `s` is null or from `ee_won_surface_new` / `ee_won_surface_duplicate`, not freed before.
#[no_mangle]
pub unsafe extern "C" fn ee_won_surface_free(s: *mut EeWonSurface) {
    if !s.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(s) });
    }
}

/// `Create(width, height)`.
///
/// # Safety
/// `s` is a live surface.
#[no_mangle]
pub unsafe extern "C" fn ee_won_surface_create(s: *mut EeWonSurface, width: i32, height: i32) {
    // SAFETY: the contract
    unsafe { (*s).create(width, height) }
}

/// `Free()`.
///
/// # Safety
/// `s` is a live surface.
#[no_mangle]
pub unsafe extern "C" fn ee_won_surface_release(s: *mut EeWonSurface) {
    // SAFETY: the contract
    unsafe { (*s).release() }
}

/// A field (`EeWonSurfaceField`).
///
/// # Safety
/// `s` is a live surface.
#[no_mangle]
pub unsafe extern "C" fn ee_won_surface_get(s: *const EeWonSurface, f: EeWonSurfaceField) -> i32 {
    // SAFETY: the contract
    unsafe { (*s).field(f) }
}

/// `GetPixelsBase()`.
///
/// # Safety
/// `s` is a live surface.
#[no_mangle]
pub unsafe extern "C" fn ee_won_surface_pixels(s: *mut EeWonSurface) -> *mut c_void {
    // SAFETY: the contract
    unsafe { (*s).pixels() }
}

/// A drawing method (`EeWonSurfaceDraw` with its arguments in the original's order).
///
/// # Safety
/// `s` is a live surface.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_surface_draw(
    s: *mut EeWonSurface,
    op: EeWonSurfaceDraw,
    a: i32,
    b: i32,
    c: i32,
    d: i32,
    e: i32,
    f: i32,
) {
    // SAFETY: the contract
    unsafe { (*s).draw(op, a, b, c, d, e, f) }
}

/// `GetPixel(x, y)`.
///
/// # Safety
/// `s` is a live surface.
#[no_mangle]
pub unsafe extern "C" fn ee_won_surface_pixel(s: *mut EeWonSurface, x: i32, y: i32) -> u32 {
    // SAFETY: the contract
    unsafe { (*s).pixel(x, y) }
}

/// `CopySurface(src, mask, x, y, left, top, width, height)` (`mask` null: the copy without a mask).
///
/// # Safety
/// `s` and `src` are live surfaces (possibly the same); `mask` is null or covers the source's rows.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_surface_copy(
    s: *mut EeWonSurface,
    src: *const EeWonSurface,
    mask: *const u8,
    x: i32,
    y: i32,
    left: i32,
    top: i32,
    width: i32,
    height: i32,
) {
    // SAFETY: the contract; the source's pixels are copied out before the destination is borrowed
    unsafe {
        if std::ptr::eq(s as *const EeWonSurface, src) {
            (*s).copy(None, mask, x, y, left, top, width, height);
        } else {
            (*s).copy(Some(&*src), mask, x, y, left, top, width, height);
        }
    }
}

/// `Duplicate()`: a new surface.
///
/// # Safety
/// `s` is a live surface.
#[no_mangle]
pub unsafe extern "C" fn ee_won_surface_duplicate(s: *const EeWonSurface) -> *mut EeWonSurface {
    // SAFETY: the contract
    Box::into_raw(Box::new(unsafe { (*s).duplicate() }))
}

/// The BITMAPINFO header's fields (`EeWonSurface::bitmap_info`) into `out` (eight ints).
///
/// # Safety
/// `s` is a live surface, `out` points at eight ints.
#[no_mangle]
pub unsafe extern "C" fn ee_won_surface_bitmap_info(s: *const EeWonSurface, out: *mut i32) {
    // SAFETY: the contract
    unsafe {
        let v = (*s).bitmap_info();
        std::ptr::copy_nonoverlapping(v.as_ptr(), out, 8);
    }
}

#[cfg(test)]
#[path = "surface_tests.rs"]
mod tests;
