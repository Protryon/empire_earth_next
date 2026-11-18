//! The toolkit's draws on the Graphics Engine's 2D draw list (`ee_render::ge::list2d`; the HUD 2D lane, round 1).
//!
//! A whole object's draw was one op on its C++ object (`BitmapDraw`, `RhombusDraw`, `LineDraw`, `FormBitmapDraw`,
//! `FormRhombusDraw`, `FormTextDraw`, `BgBitmapDraw`), whose host made the rasterizer's draw of the object on the
//! manager's screen (`GetForm()->GetManager()->GetRasterizer()->Draw(object, GetScreen())`). In the game the
//! command goes on the draw list instead, on the game's manager's rasterizer and screen ([`target`]); a replay (no
//! executor registered: [`list2d::ready`] is false) still makes the op, so the goldens trace it unchanged.

use core::ffi::{c_long, c_ulong, c_void};

use ee_base::math::types::{Ge2DPoint, GeRectangle};
use ee_render::ge::list2d::{self, Cmd, Shape, Target};

use crate::direct;
use crate::draw;
use crate::managerobj::EeUiManagerCall;
use crate::rect::UiRect;

/// The game's manager's rasterizer and screen (the game makes one manager, `gUIManager`; every form is on it).
pub fn target() -> Option<Target> {
    let manager = direct::manager_of(core::ptr::null_mut());
    if manager.is_null() {
        return None;
    }
    let rasterizer = direct::manager_call(manager, EeUiManagerCall::Rasterizer, 0, 0, 0) as usize;
    let screen = direct::manager_call(manager, EeUiManagerCall::Screen, 0, 0, 0) as core::ffi::c_ulong;
    (rasterizer != 0).then_some(Target { rasterizer, screen })
}

/// `cmd` on the draw list in the game -> its draw's result; else `op()` (the replays' op).
pub fn or_op(cmd: Cmd, op: impl FnOnce() -> isize) -> isize {
    if list2d::ready() {
        if let Some(t) = target() {
            return list2d::draw(t, &cmd) as isize;
        }
    }
    op()
}

/// A `GEBitmap*` (`BitmapDraw`, `FormBitmapDraw`, `BgBitmapDraw`).
pub fn bitmap(bitmap: isize, op: impl FnOnce() -> isize) -> isize {
    or_op(Cmd::Bitmap(bitmap as usize), op)
}

/// A `GERhombus*` (`RhombusDraw`, `FormRhombusDraw`).
pub fn rhombus(rhombus: isize, op: impl FnOnce() -> isize) -> isize {
    or_op(Cmd::Rhombus(rhombus as usize), op)
}

/// One `GELine*` (`LineDraw`).
pub fn line(line: isize, op: impl FnOnce() -> isize) -> isize {
    or_op(Cmd::Lines(line as usize, 1), op)
}

/// A `GEText*` clipped to `clip` on the manager's drawing surface (`FormTextDraw`).
pub fn text(text: isize, clip: UiRect, op: impl FnOnce() -> isize) -> c_long {
    let clip = GeRectangle { bottom: clip.bottom, left: clip.left, right: clip.right, top: clip.top };
    or_op(Cmd::TextClipped { text: text as usize, clip, surface: 0 }, op) as c_long
}

// ---- the GE objects' members (`GERhombus*`, `GELine*`, `GEBitmap*` by their C++ pointer): in the game the Rust
// object's own (`list2d.rs`, as the C++ inline made the call); in a replay the op

fn ge(r: UiRect) -> GeRectangle {
    GeRectangle { bottom: r.bottom, left: r.left, right: r.right, top: r.top }
}

/// Whether the GE objects are reached directly (the game) for a live `handle`.
fn direct(handle: isize) -> bool {
    handle != 0 && list2d::ready()
}

/// `RhombusSetRect`: `SetRectangle(r)`.
pub fn rhombus_set_rect(rhombus: isize, r: UiRect, op: impl FnOnce() -> isize) {
    if direct(rhombus) {
        // SAFETY: a live GERhombus (the caller's object)
        unsafe { list2d::rhombus_set_rectangle(rhombus as usize, ge(r)) }
    } else {
        op();
    }
}

/// `RhombusSetColor`: `SetColor(GEColor(c))`.
pub fn rhombus_set_color(rhombus: isize, c: [f32; 3], op: impl FnOnce() -> isize) {
    if direct(rhombus) {
        // SAFETY: as rhombus_set_rect
        unsafe { list2d::prim_set_color(rhombus as usize, 4, c) }
    } else {
        op();
    }
}

/// `RhombusSetOpacity`: `SetOpacity(o)`.
pub fn rhombus_set_opacity(rhombus: isize, o: f32, op: impl FnOnce() -> isize) {
    if direct(rhombus) {
        // SAFETY: as rhombus_set_rect
        unsafe { list2d::prim_set_opacity(rhombus as usize, 4, o) }
    } else {
        op();
    }
}

/// `RhombusRect`: `GetRectangle()`.
pub fn rhombus_rect(rhombus: isize, op: impl FnOnce() -> UiRect) -> UiRect {
    if direct(rhombus) {
        // SAFETY: as rhombus_set_rect
        let r = unsafe { list2d::rhombus_rectangle(rhombus as usize) };
        UiRect { bottom: r.bottom, left: r.left, right: r.right, top: r.top }
    } else {
        op()
    }
}

/// `RhombusAssign` / `RhombusCopy`: `*target = *source`.
pub fn rhombus_assign(target: isize, source: isize, op: impl FnOnce() -> isize) {
    if direct(target) && source != 0 {
        // SAFETY: two live GERhombus
        unsafe { list2d::rhombus_assign(target as usize, source as usize) }
    } else {
        op();
    }
}

/// `LineSetPoints`: `SetStartPoint(p[0], p[1])`, `SetEndPoint(p[2], p[3])`.
pub fn line_set_points(line: isize, p: [f32; 4], op: impl FnOnce() -> isize) {
    if direct(line) {
        // SAFETY: a live GELine
        unsafe {
            list2d::line_set_point(line as usize, 0, Ge2DPoint { x: p[0], y: p[1] });
            list2d::line_set_point(line as usize, 1, Ge2DPoint { x: p[2], y: p[3] });
        }
    } else {
        op();
    }
}

/// `LineSetColor`: `SetColor(GEColor(c))`.
pub fn line_set_color(line: isize, c: [f32; 3], op: impl FnOnce() -> isize) {
    if direct(line) {
        // SAFETY: a live GELine
        unsafe { list2d::prim_set_color(line as usize, 2, c) }
    } else {
        op();
    }
}

/// `LineSetOpacity`: `SetOpacity(o)`.
pub fn line_set_opacity(line: isize, o: f32, op: impl FnOnce() -> isize) {
    if direct(line) {
        // SAFETY: a live GELine
        unsafe { list2d::prim_set_opacity(line as usize, 2, o) }
    } else {
        op();
    }
}

/// `BitmapSetLocation`: `SetLocation(GE2DPoint(x, y))`.
pub fn bitmap_set_location(bitmap: isize, x: f32, y: f32, op: impl FnOnce() -> isize) {
    if direct(bitmap) {
        // SAFETY: a live GEBitmap
        unsafe { list2d::bitmap_set_location(bitmap as usize, Ge2DPoint { x, y }) }
    } else {
        op();
    }
}

/// `BitmapSetScale`: `SetXYScale(x, y)`.
pub fn bitmap_set_scale(bitmap: isize, x: f32, y: f32, op: impl FnOnce() -> isize) {
    if direct(bitmap) {
        // SAFETY: a live GEBitmap
        unsafe { list2d::bitmap_set_scale(bitmap as usize, x, y) }
    } else {
        op();
    }
}

/// `BitmapScale`: `GetXScale()`, `GetYScale()`.
pub fn bitmap_scale(bitmap: isize, op: impl FnOnce() -> crate::rect::UiPoint) -> crate::rect::UiPoint {
    if direct(bitmap) {
        // SAFETY: a live GEBitmap
        let (x, y) = unsafe { list2d::bitmap_scale(bitmap as usize) };
        crate::rect::UiPoint { x, y }
    } else {
        op()
    }
}

/// `BitmapSetOpacity`: `SetOpacity(o)`.
pub fn bitmap_set_opacity(bitmap: isize, o: f32, op: impl FnOnce() -> isize) {
    if direct(bitmap) {
        // SAFETY: a live GEBitmap
        unsafe { list2d::bitmap_set_opacity(bitmap as usize, o) }
    } else {
        op();
    }
}

/// `UIControl::DrawBorder(rect, thickness)`: a bevelled border, dark top-left and light bottom-right (swapped for
/// border type 1), the rows on top, then the bottom, left and right ones ([`draw::border_line`]), each one line of
/// one `GELine` at the control's copied border opacity -> the last draw's result (`kErrorOK` for no border).
///
/// # Safety
/// `light` and `dark` are `GEColor`s (three floats); `rasterizer` is the control's.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_draw_border(
    rasterizer: *mut c_void,
    screen: c_ulong,
    rect: UiRect,
    thickness: c_ulong,
    opacity: f32,
    border_type: c_ulong,
    light: *const f32,
    dark: *const f32,
) -> c_long {
    if thickness == 0 {
        return 0;
    }
    // SAFETY: the caller's contract
    let (light, dark) = unsafe { ([*light, *light.add(1), *light.add(2)], [*dark, *dark.add(1), *dark.add(2)]) };
    let Some(mut line) = Shape::line() else { return 0 };
    line.set_opacity(opacity);
    let t = Target { rasterizer: rasterizer as usize, screen };
    let mut result = 0;
    for side in 0..4 {
        for i in 0..thickness {
            let plan = draw::border_line(&rect, side, i, border_type);
            line.set_color(if plan.light { light } else { dark })
                .set_point(0, Ge2DPoint { x: plan.start.x, y: plan.start.y })
                .set_point(1, Ge2DPoint { x: plan.end.x, y: plan.end.y });
            result = list2d::draw(t, &Cmd::Lines(line.handle(), 1));
        }
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_replay_makes_the_op() {
        let mut made = 0;
        assert_eq!(
            bitmap(5, || {
                made += 1;
                7
            }),
            7
        );
        assert_eq!(rhombus(5, || 8), 8);
        assert_eq!(line(5, || 9), 9);
        let r = UiRect { bottom: 1.0, left: 2.0, right: 3.0, top: 4.0 };
        assert_eq!(text(5, r, || -1), -1);
        assert_eq!(made, 1);
    }
}
