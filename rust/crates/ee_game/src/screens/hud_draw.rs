//! The HUD controls' 2D draws on the Graphics Engine's draw list (`ee_render::ge::list2d`; the HUD 2D lane,
//! round 1).
//!
//! Each HUD control's drawing was one op of its class (`RustOp`) whose C++ case made the rasterizer's draw of a GE
//! object the class owned (`GetRasterizer()->Draw(mBar, GetScreenIndex())`) or built on its stack (the icon bar's
//! marks). In the FFI adapters, below the hosts' traits (the replay goldens unchanged), those ops are answered here:
//! - the primitives the C++ class owned are [`Shape`]s owned here, per control ([`shapes`], made by the class's
//!   constructor, freed by `EEUIRustControl`'s destructor: [`ee_screens_hud_draw_free`]);
//! - a bitmap the C++ class still owns (its texture and face are C++ calls) is named by its `GEBitmap*`;
//! - the commands go on the draw list on the game's manager's rasterizer and screen (`ee_ui::draw2d::target`, the
//!   control's `GetRasterizer()` and `GetScreenIndex()`: the game has one manager).

use core::ffi::c_void;
use std::collections::BTreeMap;
use std::sync::Mutex;

use ee_base::math::types::{Ge2DPoint, GeRectangle};
use ee_render::ge::list2d::{self, Cmd, Color, Shape};

/// A control's draw objects.
#[derive(Default)]
pub struct Draws {
    /// The primitives the control owns (its bars).
    pub shapes: Vec<Shape>,
    /// The `GEBitmap*`s it names (the C++ class owns them).
    pub bitmaps: Vec<usize>,
}

/// The objects of every control that has some (the UI thread's; behind a lock only to be a `static`).
struct Registry(BTreeMap<usize, Draws>);

// SAFETY: the shapes are made, drawn and freed on the UI thread; the lock serialises any other access
unsafe impl Send for Registry {}

static REGISTRY: Mutex<Registry> = Mutex::new(Registry(BTreeMap::new()));

/// `control`'s draw objects, `f` run over them (None for a control with none).
pub fn with<R>(control: *mut c_void, f: impl FnOnce(&mut Draws) -> R) -> Option<R> {
    let mut r = REGISTRY.lock().unwrap_or_else(|e| e.into_inner());
    r.0.get_mut(&(control as usize)).map(f)
}

/// Records `control`'s draw objects (its constructor's).
pub fn put(control: *mut c_void, draws: Draws) {
    REGISTRY.lock().unwrap_or_else(|e| e.into_inner()).0.insert(control as usize, draws);
}

/// `EEUIRustControl`'s destructor: the control's draw objects freed (nothing for a control with none).
#[no_mangle]
pub extern "C" fn ee_screens_hud_draw_free(control: *mut c_void) {
    let draws = REGISTRY.lock().unwrap_or_else(|e| e.into_inner()).0.remove(&(control as usize));
    drop(draws);
}

/// Draws `cmd` on the game's manager's target -> its result (0 off the game).
pub fn draw(cmd: Cmd) -> i64 {
    match ee_ui::draw2d::target() {
        Some(t) => list2d::draw(t, &cmd) as i64,
        None => 0,
    }
}

/// `n` rhombi of a colour and opacity (a constructor's bars).
pub fn rhombi(n: usize, color: Color, opacity: f32) -> Vec<Shape> {
    (0..n)
        .filter_map(|_| {
            let mut s = Shape::rhombus()?;
            s.set_color(color).set_opacity(opacity);
            Some(s)
        })
        .collect()
}

/// A rectangle from an op's floats (left, top, right, bottom).
pub fn rect(f: &[f32]) -> GeRectangle {
    GeRectangle { bottom: f[3], left: f[0], right: f[2], top: f[1] }
}

/// A shape's rectangle into an op's floats (left, top, right, bottom).
pub fn put_rect(shape: &Shape, f: &mut [f32]) {
    let r = shape.rectangle();
    f[..4].copy_from_slice(&[r.left, r.top, r.right, r.bottom]);
}

/// A temporary rhombus of a colour over a rectangle, opaque, drawn (`GERhombus r; r.SetColor; r.SetRectangle;
/// Draw(&r)`).
pub fn fill(color: Color, r: GeRectangle) -> i64 {
    let Some(mut s) = Shape::rhombus() else { return 0 };
    s.set_color(color).set_rectangle(r);
    draw(Cmd::Rhombus(s.handle()))
}

/// A temporary line from `start` to `end` in a colour drawn (`GELine l; l.SetStartPoint; l.SetEndPoint; l.SetColor;
/// Draw(&l, 1)`).
pub fn line(start: Ge2DPoint, end: Ge2DPoint, color: Color) -> i64 {
    let Some(mut s) = Shape::line() else { return 0 };
    s.set_point(0, start).set_point(1, end).set_color(color);
    draw(Cmd::Lines(s.handle(), 1))
}

// ---- the classes' ops

/// The ping meter's (`ping.rs`): BAR (bar, f) its rectangle set, its colour green; DRAW (bar) drawn.
pub fn ping(control: *mut c_void, op: u32, a: i64, f: &mut [f32]) -> Option<i64> {
    use crate::screens::ping::{EE_SCREENS_PING_BAR, EE_SCREENS_PING_DRAW};
    match op {
        EE_SCREENS_PING_BAR => with(control, |d| {
            if let Some(s) = d.shapes.get_mut(a as usize) {
                s.set_rectangle(rect(f)).set_color([0.0, 1.0, 0.0]);
            }
            0
        }),
        EE_SCREENS_PING_DRAW => {
            let handle = with(control, |d| d.shapes.get(a as usize).map(Shape::handle))??;
            Some(draw(Cmd::Rhombus(handle)))
        }
        _ => None,
    }
}

/// The ping meter's five bars (white, opaque, empty: `GERhombus mBars[5]`), made before its constructor places them.
pub fn ping_init(control: *mut c_void) {
    let shapes = (0..5).filter_map(|_| Shape::rhombus()).collect();
    put(control, Draws { shapes, bitmaps: Vec::new() });
}

/// The portrait's (`portrait.rs`, the class's own op numbers): its bars' rectangles (BAR, SET_BAR: 0 the first, 1 the
/// second, 2 the background) and the draws (DRAW: 0 the portrait bitmap, 1 the first bar, 2 the second, 3 the
/// background).
pub fn portrait(control: *mut c_void, op: u32, a: i64, f: &mut [f32]) -> Option<i64> {
    use crate::screens::portrait::{EE_SCREENS_PT_OP_BAR, EE_SCREENS_PT_OP_DRAW, EE_SCREENS_PT_OP_SET_BAR};
    match op {
        EE_SCREENS_PT_OP_BAR => with(control, |d| {
            if let Some(s) = d.shapes.get(a as usize) {
                put_rect(s, f);
            }
            0
        }),
        EE_SCREENS_PT_OP_SET_BAR => with(control, |d| {
            if let Some(s) = d.shapes.get_mut(a as usize) {
                s.set_rectangle(rect(f));
            }
            0
        }),
        EE_SCREENS_PT_OP_DRAW => {
            let cmd = with(control, |d| match a {
                0 => d.bitmaps.first().map(|&b| Cmd::Bitmap(b)),
                1..=3 => d.shapes.get(a as usize - 1).map(|s| Cmd::Rhombus(s.handle())),
                _ => None,
            })??;
            Some(draw(cmd))
        }
        _ => None,
    }
}

/// `EEUICPortrait`'s constructor's draw objects: its bitmap (`mPortrait`, the class's), the research bar (green),
/// the improvement bar (yellow) and the background (red, over the portrait's rectangle), all half opaque.
///
/// # Safety
/// `bitmap` is the portrait's live `GEBitmap*`; `rect` four floats (left, top, right, bottom).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_portrait_draw_init(portrait: *mut c_void, bitmap: *mut c_void, rect: *const f32) {
    // SAFETY: the caller's contract
    let r = unsafe { core::slice::from_raw_parts(rect, 4) };
    let mut shapes = Vec::new();
    for color in [[0.15, 1.0, 0.15], [1.0, 1.0, 0.15], [1.0, 0.15, 0.15]] {
        if let Some(mut s) = Shape::rhombus() {
            s.set_color(color).set_opacity(0.5);
            shapes.push(s);
        }
    }
    if let Some(background) = shapes.get_mut(2) {
        background.set_rectangle(self::rect(r));
    }
    put(portrait, Draws { shapes, bitmaps: vec![bitmap as usize] });
}

/// The icon bar's (`icon_bar.rs`, the class's own op numbers): RHOMBUS (0 black, 1 red, 2 green) over f[0..4] and
/// LINE (red, f[0..2] to f[2..4]), each a temporary primitive drawn.
pub fn icon_bar(op: u32, a: i64, f: &[f32]) -> Option<i64> {
    use crate::screens::icon_bar::{EE_SCREENS_IB_OP_LINE, EE_SCREENS_IB_OP_RHOMBUS};
    match op {
        EE_SCREENS_IB_OP_RHOMBUS => {
            fill([if a == 1 { 1.0 } else { 0.0 }, if a == 2 { 1.0 } else { 0.0 }, 0.0], rect(f));
            Some(0)
        }
        EE_SCREENS_IB_OP_LINE => {
            Some(line(Ge2DPoint { x: f[0], y: f[1] }, Ge2DPoint { x: f[2], y: f[3] }, [1.0, 0.0, 0.0]))
        }
        _ => None,
    }
}
