//! The 2D draw list (the HUD 2D lane, round 1): the bitmaps, rhombi, lines, points and texts the toolkit, the HUD
//! and the screens draw, recorded as [`Cmd`]s on a [`Target`] (a rasterizer and its screen) and executed by the
//! rasterizer's own draws.
//!
//! The objects are the Graphics Engine's (each a Rust object: `bmpobj.rs`, `prim.rs`, `smallobj.rs`), named by the
//! C++ pointer every draw takes (`GEBitmap*`, `GERhombus*`, a `GELine`/`GEPoint` array, `GEText*`; a primitive's
//! C++ object is its Rust object's pointer alone, `cppobj.rs`), so a command made in Rust draws exactly what the C++
//! `GERasterizer::Draw` of the same object drew:
//! - on a DX7 rasterizer ([`ee_ge_draw2d_direct`] registers each one) the bitmap, rhombus, line and point draws are
//!   `ee_dx7r_draw_*` (`dx7/draw2d.rs`), the bodies `DX7Rasterizer::Draw` forwards to;
//! - every other draw (a text: the GX surface and the font bitmap are the C++'s; any draw on another rasterizer,
//!   e.g. the store gate's) goes through the executor the C++ registers ([`ee_ge_draw2d_init`]), one virtual
//!   `GERasterizer::Draw` per command.
//!
//! [`Shape`] is a rhombus, line or point made and owned in Rust with the C++ object's layout (its one pointer), for
//! the draws whose C++ built a temporary primitive (the error form's buttons, a background's colour, the icon bar's
//! marks). Nothing here is reached by a replay: the goldens' hosts never register an executor, and without one
//! [`ready`] is false and a [`Shape`] cannot be made.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong, c_void};
use std::collections::BTreeMap;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Mutex;

use ee_base::math::types::{Ge2DPoint, GeRectangle};

use crate::ge::bmpobj::{EeGeBitmap, EeGeBmpHostFn, EeGeBmpOp};
use crate::ge::cppobj;
use crate::ge::prim::{
    ee_ge_prim_destroy, ee_ge_prim_new, ee_ge_prim_new_line, ee_ge_prim_place, ee_ge_prim_rectangle,
    ee_ge_prim_set_opacity, ee_ge_prim_vertex, EeGePrim, EeGePrimKind,
};

/// What a command draws.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeDraw2DKind {
    /// `Draw(GEBitmap*, screen)`.
    Bitmap = 1,
    /// `Draw(GERhombus*, screen)`.
    Rhombus,
    /// `Draw(GELine*, count, screen)`.
    Lines,
    /// `Draw(GEPoint*, count, screen)`.
    Points,
    /// `Draw(GEText*, clip, surface)`: `surface` null is the screen's secondary surface (`UIManager::GetDrawingSurface`).
    TextClipped,
    /// `Draw(GEText*, surface)`.
    Text,
}

/// One command as the executor reads it.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGeDraw2D {
    pub kind: EeGeDraw2DKind,
    /// The C++ object (`GEBitmap*`, `GERhombus*`, `GELine*`, `GEPoint*`, `GEText*`).
    pub object: *mut c_void,
    /// The lines' or points' count.
    pub count: c_ulong,
    /// The text's clip (`TextClipped`).
    pub clip: GeRectangle,
    /// The text's surface (`GESurface*`; null: the secondary surface of the screen).
    pub surface: *mut c_void,
}

/// The C++ executor: one `GERasterizer::Draw` of `cmd` on `rasterizer`'s `screen` -> its result.
pub type EeGeDraw2DExecFn =
    Option<unsafe extern "C" fn(rasterizer: *mut c_void, screen: c_ulong, cmd: *const EeGeDraw2D) -> c_long>;

/// The primitives' vertex host (`GEVertexHost`, an `EeGeBmpHostFn`; spelled out for cbindgen).
pub type EeGeDraw2DVertexHost =
    Option<unsafe extern "C" fn(owner: *mut c_void, op: EeGeBmpOp, obj: *mut c_void, a: isize, b: isize) -> isize>;

static EXEC: AtomicUsize = AtomicUsize::new(0);
static VERTEX_HOST: AtomicUsize = AtomicUsize::new(0);
/// `GERasterizer*` -> its `EeDx7Rasterizer*` (the DX7 rasterizers alive).
static DIRECT: Mutex<BTreeMap<usize, usize>> = Mutex::new(BTreeMap::new());

/// The game's executor and the primitives' vertex host (`GEVertexHost`), set once by the C++ (`GERasterizer`'s
/// constructor).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_draw2d_init(exec: EeGeDraw2DExecFn, vertex_host: EeGeDraw2DVertexHost) {
    EXEC.store(exec.map_or(0, |f| f as usize), Ordering::Release);
    VERTEX_HOST.store(vertex_host.map_or(0, |f| f as usize), Ordering::Release);
}

/// `rasterizer` (a `GERasterizer*`) draws through `dx7` (its `EeDx7Rasterizer*`); null `dx7` forgets it (the
/// rasterizer's destructor).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_draw2d_direct(rasterizer: *mut c_void, dx7: *mut c_void) {
    let mut d = DIRECT.lock().unwrap_or_else(|e| e.into_inner());
    if dx7.is_null() {
        d.remove(&(rasterizer as usize));
    } else {
        d.insert(rasterizer as usize, dx7 as usize);
    }
}

/// Whether the game's executor is set (never in a cargo test: the replays keep their hosts' draw ops).
pub fn ready() -> bool {
    EXEC.load(Ordering::Acquire) != 0
}

fn exec() -> EeGeDraw2DExecFn {
    let f = EXEC.load(Ordering::Acquire);
    // SAFETY: stored from an `EeGeDraw2DExecFn` in `ee_ge_draw2d_init`
    (f != 0).then(|| unsafe {
        core::mem::transmute::<usize, unsafe extern "C" fn(*mut c_void, c_ulong, *const EeGeDraw2D) -> c_long>(f)
    })
}

fn vertex_host() -> Option<EeGeBmpHostFn> {
    let f = VERTEX_HOST.load(Ordering::Acquire);
    // SAFETY: stored from an `EeGeBmpHostFn` in `ee_ge_draw2d_init`
    (f != 0).then(|| unsafe { core::mem::transmute::<usize, EeGeBmpHostFn>(f) })
}

fn direct_of(rasterizer: usize) -> usize {
    DIRECT.lock().map(|d| d.get(&rasterizer).copied().unwrap_or(0)).unwrap_or(0)
}

/// Where a command draws: a `GERasterizer*` and its screen index.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Target {
    pub rasterizer: usize,
    pub screen: c_ulong,
}

/// One 2D draw.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Cmd {
    /// A `GEBitmap*`.
    Bitmap(usize),
    /// A `GERhombus*`.
    Rhombus(usize),
    /// A `GELine` array and its count.
    Lines(usize, u32),
    /// A `GEPoint` array and its count.
    Points(usize, u32),
    /// A `GEText*` clipped to the rectangle, on a surface (0: the screen's secondary surface).
    TextClipped { text: usize, clip: GeRectangle, surface: usize },
    /// A `GEText*` on a surface.
    Text { text: usize, surface: usize },
}

impl Cmd {
    fn record(&self) -> EeGeDraw2D {
        let zero = GeRectangle { bottom: 0.0, left: 0.0, right: 0.0, top: 0.0 };
        let (kind, object, count, clip, surface) = match *self {
            Cmd::Bitmap(o) => (EeGeDraw2DKind::Bitmap, o, 0, zero, 0),
            Cmd::Rhombus(o) => (EeGeDraw2DKind::Rhombus, o, 0, zero, 0),
            Cmd::Lines(o, n) => (EeGeDraw2DKind::Lines, o, n, zero, 0),
            Cmd::Points(o, n) => (EeGeDraw2DKind::Points, o, n, zero, 0),
            Cmd::TextClipped { text, clip, surface } => (EeGeDraw2DKind::TextClipped, text, 0, clip, surface),
            Cmd::Text { text, surface } => (EeGeDraw2DKind::Text, text, 0, zero, surface),
        };
        EeGeDraw2D {
            kind,
            object: object as *mut c_void,
            count: count as c_ulong,
            clip,
            surface: surface as *mut c_void,
        }
    }
}

/// The DX7 rasterizer's own draw of a bitmap, rhombus, line or point command (`DX7Rasterizer::Draw`'s bodies).
#[cfg(not(test))]
fn draw_direct(dx7: usize, screen: c_ulong, cmd: &Cmd) -> Option<c_long> {
    use crate::dx7::draw_ffi::{ee_dx7r_draw_bitmap, ee_dx7r_draw_lines, ee_dx7r_draw_points, ee_dx7r_draw_rhombus};
    use crate::dx7::rastobj::EeDx7Rasterizer;
    let r = dx7 as *mut EeDx7Rasterizer;
    // SAFETY: a registered (live) DX7 rasterizer; the objects are live C++ GE objects (the caller's)
    unsafe {
        match *cmd {
            Cmd::Bitmap(b) => {
                let rust: *mut EeGeBitmap = cppobj::counted(b);
                Some(ee_dx7r_draw_bitmap(r, screen, rust.cast()))
            }
            Cmd::Rhombus(o) => Some(ee_dx7r_draw_rhombus(r, screen, o as *mut c_void)),
            Cmd::Lines(o, n) => Some(ee_dx7r_draw_lines(r, screen, o as *mut c_void, n as c_ulong)),
            Cmd::Points(o, n) => Some(ee_dx7r_draw_points(r, screen, o as *mut c_void, n as c_ulong)),
            Cmd::TextClipped { .. } | Cmd::Text { .. } => None,
        }
    }
}

#[cfg(test)]
fn draw_direct(_dx7: usize, _screen: c_ulong, _cmd: &Cmd) -> Option<c_long> {
    None
}

/// Executes one command on the target: its draw's result (0 without the game's executor).
pub fn draw(t: Target, cmd: &Cmd) -> c_long {
    let dx7 = direct_of(t.rasterizer);
    if dx7 != 0 {
        if let Some(r) = draw_direct(dx7, t.screen, cmd) {
            return r;
        }
    }
    let record = cmd.record();
    match exec() {
        // SAFETY: the executor is the C++'s; the record lives for the call
        Some(f) => unsafe { f(t.rasterizer as *mut c_void, t.screen, &record) },
        None => 0,
    }
}

/// Commands recorded on one target, executed in order by [`DrawList::flush`].
#[derive(Debug)]
pub struct DrawList {
    pub target: Target,
    cmds: Vec<Cmd>,
}

impl DrawList {
    pub fn new(target: Target) -> DrawList {
        DrawList { target, cmds: Vec::new() }
    }

    pub fn push(&mut self, cmd: Cmd) -> &mut Self {
        self.cmds.push(cmd);
        self
    }

    pub fn commands(&self) -> &[Cmd] {
        &self.cmds
    }

    /// Draws the commands in order and empties the list -> the last draw's result (0 for none).
    pub fn flush(&mut self) -> c_long {
        let mut r = 0;
        for cmd in self.cmds.drain(..) {
            r = draw(self.target, &cmd);
        }
        r
    }
}

/// A colour (`GEColor`: red, green, blue).
pub type Color = [f32; 3];

/// A rhombus, line or point made in Rust: a C++ `GERhombus`/`GELine`/`GEPoint` in layout (the Rust object's pointer
/// in a cell of its own, whose address is the C++ object's), destroyed on drop.
pub struct Shape {
    cell: Box<*mut EeGePrim>,
    vertices: u32,
}

impl Shape {
    fn make(make: impl FnOnce(EeGeBmpHostFn, *mut c_void) -> *mut EeGePrim, vertices: u32) -> Option<Shape> {
        let host = vertex_host()?;
        let mut cell = Box::new(core::ptr::null_mut());
        let owner = &mut *cell as *mut *mut EeGePrim as *mut c_void;
        *cell = make(host, owner);
        Some(Shape { cell, vertices })
    }

    /// `GERhombus()`: white, opaque, empty (None without the game's vertex host).
    pub fn rhombus() -> Option<Shape> {
        // SAFETY: the host is the game's GEVertexHost
        Self::make(|h, o| unsafe { ee_ge_prim_new(h, o, EeGePrimKind::Rhombus) }, 4)
    }

    /// `GELine()`.
    pub fn line() -> Option<Shape> {
        // SAFETY: as rhombus
        Self::make(|h, o| unsafe { ee_ge_prim_new(h, o, EeGePrimKind::Line) }, 2)
    }

    /// `GELine(start, end, start colour, end colour, opacity)`.
    pub fn line_between(start: Ge2DPoint, end: Ge2DPoint, from: Color, to: Color, opacity: f32) -> Option<Shape> {
        // SAFETY: as rhombus; the points and colours live for the call
        Self::make(
            |h, o| unsafe {
                ee_ge_prim_new_line(
                    h,
                    o,
                    &start,
                    &end,
                    from.as_ptr() as *const c_void,
                    to.as_ptr() as *const c_void,
                    opacity,
                )
            },
            2,
        )
    }

    /// The C++ object's address (`GERhombus*`, `GELine*`).
    pub fn handle(&self) -> usize {
        &*self.cell as *const *mut EeGePrim as usize
    }

    /// `SetColor`: every vertex.
    pub fn set_color(&mut self, color: Color) -> &mut Self {
        for i in 0..self.vertices {
            // SAFETY: a live primitive; the colour lives for the call
            unsafe {
                ee_ge_prim_vertex(*self.cell, EeGeBmpOp::VertexSetColor, i as c_ulong, color.as_ptr() as *mut c_void)
            };
        }
        self
    }

    /// `SetOpacity`: every vertex.
    pub fn set_opacity(&mut self, opacity: f32) -> &mut Self {
        for i in 0..self.vertices {
            // SAFETY: a live primitive
            unsafe { ee_ge_prim_set_opacity(*self.cell, i as c_ulong, opacity) };
        }
        self
    }

    /// `GERhombus::SetRectangle` (unrotated).
    pub fn set_rectangle(&mut self, rect: GeRectangle) -> &mut Self {
        // SAFETY: a live rhombus; the rectangle lives for the call
        unsafe { ee_ge_prim_place(*self.cell, &rect, 0.0) };
        self
    }

    /// `GERhombus::GetRectangle`.
    pub fn rectangle(&self) -> GeRectangle {
        // SAFETY: a live rhombus
        unsafe { *ee_ge_prim_rectangle(*self.cell) }
    }

    /// `SetStartPoint` / `SetEndPoint` (a line's vertex 0 / 1).
    pub fn set_point(&mut self, index: u32, point: Ge2DPoint) -> &mut Self {
        // SAFETY: a live line
        unsafe { crate::ge::prim::ee_ge_prim_set_xy(*self.cell, index as c_ulong, &point) };
        self
    }
}

impl Drop for Shape {
    fn drop(&mut self) {
        // SAFETY: made by ee_ge_prim_new*, destroyed once
        unsafe { ee_ge_prim_destroy(*self.cell) };
    }
}

// ---- the GE objects' members by their C++ pointer (`GERhombus*`, `GELine*`, `GEBitmap*`), as the C++ inlines make
// them: the callers' C++ objects (a cell holding the primitive's Rust object; a reference-counted bitmap)

fn prim_of(handle: usize) -> *mut EeGePrim {
    // SAFETY: the callers' contract: `handle` is a live C++ primitive (one pointer, its Rust object)
    unsafe { *(handle as *const *mut EeGePrim) }
}

fn bitmap_of(handle: usize) -> *mut EeGeBitmap {
    // SAFETY: the callers' contract: `handle` is a live GEBitmap
    unsafe { cppobj::counted(handle) }
}

/// `SetColor` of a rhombus (`vertices` 4) or a line (2): every vertex.
///
/// # Safety
/// `handle` is a live C++ primitive.
pub unsafe fn prim_set_color(handle: usize, vertices: u32, color: Color) {
    for i in 0..vertices {
        // SAFETY: the caller's contract; the colour lives for the call
        unsafe {
            ee_ge_prim_vertex(prim_of(handle), EeGeBmpOp::VertexSetColor, i as c_ulong, color.as_ptr() as *mut c_void)
        };
    }
}

/// `SetOpacity` of a rhombus (4) or a line (2).
///
/// # Safety
/// As [`prim_set_color`].
pub unsafe fn prim_set_opacity(handle: usize, vertices: u32, opacity: f32) {
    for i in 0..vertices {
        // SAFETY: the caller's contract
        unsafe { ee_ge_prim_set_opacity(prim_of(handle), i as c_ulong, opacity) };
    }
}

/// `GERhombus::SetRectangle`.
///
/// # Safety
/// `handle` is a live `GERhombus`.
pub unsafe fn rhombus_set_rectangle(handle: usize, rect: GeRectangle) {
    // SAFETY: the caller's contract
    unsafe { ee_ge_prim_place(prim_of(handle), &rect, 0.0) };
}

/// `GERhombus::GetRectangle`.
///
/// # Safety
/// As [`rhombus_set_rectangle`].
pub unsafe fn rhombus_rectangle(handle: usize) -> GeRectangle {
    // SAFETY: the caller's contract
    unsafe { *ee_ge_prim_rectangle(prim_of(handle)) }
}

/// `*target = *source` of two `GERhombus`es.
///
/// # Safety
/// Both are live.
pub unsafe fn rhombus_assign(target: usize, source: usize) {
    // SAFETY: the caller's contract
    unsafe { crate::ge::prim::ee_ge_prim_assign(prim_of(target), prim_of(source)) };
}

/// `GELine::SetStartPoint` (0) / `SetEndPoint` (1).
///
/// # Safety
/// `handle` is a live `GELine`.
pub unsafe fn line_set_point(handle: usize, index: u32, point: Ge2DPoint) {
    // SAFETY: the caller's contract
    unsafe { crate::ge::prim::ee_ge_prim_set_xy(prim_of(handle), index as c_ulong, &point) };
}

/// `GEBitmap::SetLocation`.
///
/// # Safety
/// `handle` is a live `GEBitmap`.
pub unsafe fn bitmap_set_location(handle: usize, point: Ge2DPoint) {
    // SAFETY: the caller's contract
    unsafe { crate::ge::bmpobj::ee_ge_bmp_set_location(bitmap_of(handle), &point) };
}

/// `GEBitmap::SetXYScale`.
///
/// # Safety
/// As [`bitmap_set_location`].
pub unsafe fn bitmap_set_scale(handle: usize, x: f32, y: f32) {
    // SAFETY: the caller's contract
    unsafe { crate::ge::bmpobj::ee_ge_bmp_set_xy_scale(bitmap_of(handle), x, y) };
}

/// `GEBitmap::GetXScale`, `GetYScale`.
///
/// # Safety
/// As [`bitmap_set_location`].
pub unsafe fn bitmap_scale(handle: usize) -> (f32, f32) {
    use crate::ge::bmpobj::{ee_ge_bmp_getf, EeGeBmpFloat};
    // SAFETY: the caller's contract
    unsafe {
        (
            ee_ge_bmp_getf(bitmap_of(handle), EeGeBmpFloat::XScale),
            ee_ge_bmp_getf(bitmap_of(handle), EeGeBmpFloat::YScale),
        )
    }
}

/// `GEBitmap::ComputeStretchedCoordsAndScale(texture, source, destination, screen)`.
///
/// # Safety
/// As [`bitmap_set_location`].
pub unsafe fn bitmap_stretch(
    handle: usize,
    texture: GeRectangle,
    source: GeRectangle,
    destination: GeRectangle,
    screen: c_ulong,
) {
    // SAFETY: the caller's contract; the rectangles live for the call
    unsafe { crate::ge::bmpobj::ee_ge_bmp_stretch(bitmap_of(handle), &texture, &source, &destination, screen) };
}

/// `GEBitmap::SetOpacity`: every corner.
///
/// # Safety
/// As [`bitmap_set_location`].
pub unsafe fn bitmap_set_opacity(handle: usize, opacity: f32) {
    // SAFETY: the caller's contract
    unsafe { crate::ge::bmpobj::ee_ge_bmp_set_opacities(bitmap_of(handle), &[opacity; 4]) };
}

// ---- the C++'s immediates: a temporary primitive drawn once (the C++ built it on its stack)

/// A rectangle filled with a colour at an opacity (`GERhombus r; r.SetColor; r.SetRectangle; r.SetOpacity;
/// Draw(&r)`) -> the draw's result.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_draw2d_fill(
    rasterizer: *mut c_void,
    screen: c_ulong,
    rect: *const GeRectangle,
    color: *const f32,
    opacity: f32,
) -> c_long {
    // SAFETY: the caller's: a live rectangle and colour
    let (rect, color) = unsafe { (*rect, [*color, *color.add(1), *color.add(2)]) };
    fill(Target { rasterizer: rasterizer as usize, screen }, rect, color, opacity)
}

/// [`ee_ge_draw2d_fill`] in Rust.
pub fn fill(t: Target, rect: GeRectangle, color: Color, opacity: f32) -> c_long {
    let Some(mut r) = Shape::rhombus() else { return 0 };
    r.set_color(color).set_rectangle(rect).set_opacity(opacity);
    draw(t, &Cmd::Rhombus(r.handle()))
}

/// One line from `start` to `end` in a colour, opaque (`GELine l; l.SetStartPoint; l.SetEndPoint; l.SetColor;
/// Draw(&l, 1)`) -> the draw's result.
pub fn line(t: Target, start: Ge2DPoint, end: Ge2DPoint, color: Color) -> c_long {
    let Some(mut l) = Shape::line() else { return 0 };
    l.set_point(0, start).set_point(1, end).set_color(color);
    draw(t, &Cmd::Lines(l.handle(), 1))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn without_the_executor_nothing_draws() {
        assert!(!ready());
        assert!(Shape::rhombus().is_none());
        let t = Target { rasterizer: 0x1000, screen: 0 };
        assert_eq!(draw(t, &Cmd::Rhombus(0x2000)), 0);
        let mut l = DrawList::new(t);
        l.push(Cmd::Bitmap(1)).push(Cmd::Lines(2, 3));
        assert_eq!(l.commands().len(), 2);
        assert_eq!(l.flush(), 0);
        assert!(l.commands().is_empty());
    }

    #[test]
    fn records_name_the_kind_object_and_count() {
        let clip = GeRectangle { bottom: 4.0, left: 1.0, right: 3.0, top: 2.0 };
        let r = Cmd::TextClipped { text: 7, clip, surface: 0 }.record();
        assert_eq!(r.kind, EeGeDraw2DKind::TextClipped);
        assert_eq!(r.object as usize, 7);
        assert_eq!(r.clip, clip);
        assert!(r.surface.is_null());
        let r = Cmd::Points(9, 5).record();
        assert_eq!((r.kind, r.object as usize, r.count), (EeGeDraw2DKind::Points, 9, 5));
        let r = Cmd::Text { text: 3, surface: 11 }.record();
        assert_eq!((r.kind, r.surface as usize), (EeGeDraw2DKind::Text, 11));
    }

    #[test]
    fn direct_registry_adds_and_forgets() {
        unsafe { ee_ge_draw2d_direct(0x5000 as *mut c_void, 0x6000 as *mut c_void) };
        assert_eq!(direct_of(0x5000), 0x6000);
        unsafe { ee_ge_draw2d_direct(0x5000 as *mut c_void, core::ptr::null_mut()) };
        assert_eq!(direct_of(0x5000), 0);
    }
}
