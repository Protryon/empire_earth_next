//! The main game view's box selection (`EEUICSelectUnits`) whole: the press starts a drag confined to the game view,
//! the drag grows a white box over a black shadow (drawn from the snapshot clipped to the view), the release
//! selects the units in the box (or the unit under a plain click), a double click selects the clicked unit's type
//! on screen. The box's geometry is `ee_gfx`'s `select.rs`. The record is the class's members but the lines and the
//! click and lasso actions (C++ objects, reached through the host). The scenario editor's subclass reads the record.

use ee_render::gfx::select;

use crate::hud_ctl::{
    EeScreensPoint, EeScreensRect, HudHost, HudOps, Rec, EE_SCREENS_HC_ADD_DIRTY, EE_SCREENS_HC_CAPTURE_KEYBOARD,
    EE_SCREENS_HC_CAPTURE_MOUSE, EE_SCREENS_HC_COPY_BASE, EE_SCREENS_HC_DISPLAY_MODE, EE_SCREENS_HC_DOUBLE_BASE,
    EE_SCREENS_HC_DRAW_BASE, EE_SCREENS_HC_FORM_REDRAW, EE_SCREENS_HC_HIDDEN, EE_SCREENS_HC_MODIFIERS,
    EE_SCREENS_HC_MOUSE_BOUNDS, EE_SCREENS_HC_MOUSE_BOUNDS_WINDOW, EE_SCREENS_HC_RECT, EE_SCREENS_HC_RELEASE_KEYBOARD,
    EE_SCREENS_HC_RELEASE_MOUSE, EE_SCREENS_HC_SET_DISPLAY_MODE, EE_SCREENS_HC_SET_RECTANGLE, EE_SCREENS_HC_SHOWN,
    EE_SCREENS_HC_UPDATE_MOUSE, EE_SCREENS_HC_VIEW_HAS_MOUSE, EE_SCREENS_HC_VIEW_RECT,
};

/// The eight lines set from f[0..32] (start x, y, end x, y each) and drawn -> the draw's result.
pub const EE_SCREENS_SU_DRAW_LINES: u32 = 60;
/// -> the unit under the mouse (referenced; 0: none).
pub const EE_SCREENS_SU_PICK: u32 = 61;
/// (unit) -> whether it is alive and the local player's.
pub const EE_SCREENS_SU_OWN_LIVE: u32 = 62;
/// (unit, modifiers) the lasso selects the unit's type in the game view.
pub const EE_SCREENS_SU_LASSO_TYPE: u32 = 63;
/// (unit) dereferenced.
pub const EE_SCREENS_SU_DEREF: u32 = 64;
/// (modifiers) the lasso selects every unit in the box f[0..4].
pub const EE_SCREENS_SU_LASSO_BOX: u32 = 65;
/// (modifiers) the click action selects at the mouse.
pub const EE_SCREENS_SU_CLICK: u32 = 66;
/// The trigger tab's: (unit) -> whether it is part of a complex unit.
pub const EE_SCREENS_SU_HAS_COMPLEX: u32 = 67;
/// The trigger tab's: the unit or tile under the control's mouse location -> f[0..2] its map point (x, y).
pub const EE_SCREENS_SU_POINT_AT: u32 = 68;
/// The trigger tab's: (flag) the lasso's `SelectInRectangle` over f[0..4].
pub const EE_SCREENS_SU_LASSO_IN_RECT: u32 = 69;
/// The trigger tab's: the click action's `Execute(f[0], f[1], true, 1, false, 0, false)`.
pub const EE_SCREENS_SU_CLICK_EXECUTE: u32 = 80;
/// The trigger tab's: -> the control's `MouseLButtonDown()`.
pub const EE_SCREENS_SU_L_DOWN: u32 = 81;

/// `ee_screens_select_units`' members (`hud_ctl_ffi.rs`).
pub const EE_SCREENS_SU_DO_COPY_DATA: u32 = 0;
pub const EE_SCREENS_SU_DO_DRAW: u32 = 1;
pub const EE_SCREENS_SU_DO_DOUBLE_CLICK: u32 = 2;
pub const EE_SCREENS_SU_DO_MOUSE_L_UP: u32 = 3;
pub const EE_SCREENS_SU_DO_MOUSE_MOVE: u32 = 4;
pub const EE_SCREENS_SU_DO_START: u32 = 5;
pub const EE_SCREENS_SU_DO_R_DOWN: u32 = 6;
pub const EE_SCREENS_SU_DO_R_UP: u32 = 7;

/// `EEUICSelectUnits`' members (its protected base) but the lines and the actions.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Debug, Default)]
pub struct EeScreensSelectUnitsFields {
    /// The box as of the last `CopyData`, clipped to the game view.
    pub mRA0: EeScreensRect,
    /// The press (rounded).
    pub mDragStart: EeScreensPoint,
    /// The mouse (rounded, clamped to the game view).
    pub mDragEnd: EeScreensPoint,
    /// The drag went far enough from the press.
    pub mDragging: bool,
    /// The box is drawn.
    pub mD1: bool,
    /// The trigger tab's control (`EESEUICTriggerSelectUnits`; port addition: the subclass's overrides are this).
    pub mTrigger: bool,
    /// The trigger tab's: the right button is down (its press and release drive the left's).
    pub mRightButton: bool,
}

type R = Rec<EeScreensSelectUnitsFields>;

/// The release's common end: the mouse free in the window, hidden, not dragging.
fn finish(rec: &R, h: &mut impl HudHost) {
    h.call(EE_SCREENS_HC_MOUSE_BOUNDS_WINDOW, 0, 0);
    h.call(EE_SCREENS_HC_SET_DISPLAY_MODE, EE_SCREENS_HC_HIDDEN, 0);
    h.call(EE_SCREENS_HC_RELEASE_MOUSE, 0, 0);
    rec.set(|r| r.mDragging = false);
}

/// `CopyData`: while dragging, the box clipped to the view is snapshotted and what it covers redrawn.
pub fn copy_data(rec: &R, h: &mut impl HudHost) -> i64 {
    if rec.get(|r| r.mDragging) {
        let mut ra0 = h.rect(EE_SCREENS_HC_RECT).ge();
        let view = h.rect(EE_SCREENS_HC_VIEW_RECT).ge();
        ra0.and_in_place(&view);
        let ra0 = EeScreensRect::of_ge(ra0);
        rec.set(|r| r.mRA0 = ra0);
        h.with_rect(EE_SCREENS_HC_ADD_DIRTY, 0, ra0);
        h.with_rect(EE_SCREENS_HC_FORM_REDRAW, 0, ra0);
        rec.set(|r| r.mD1 = true);
    } else {
        rec.set(|r| r.mD1 = false);
    }
    h.call(EE_SCREENS_HC_COPY_BASE, 0, 0)
}

/// `DrawPolygons`: the snapshot's eight lines, then the base's.
pub fn draw(rec: &R, h: &mut impl HudHost) -> i64 {
    if rec.get(|r| r.mD1) {
        let lines = select::box_lines(&rec.get(|r| r.mRA0).ge());
        let mut f = [0.0; 32];
        for (i, l) in lines.points.iter().enumerate() {
            f[i * 4..i * 4 + 4].copy_from_slice(&[l[0].x, l[0].y, l[1].x, l[1].y]);
        }
        h.op(EE_SCREENS_SU_DRAW_LINES, 0, 0, &mut f);
    }
    h.call(EE_SCREENS_HC_DRAW_BASE, 0, 0)
}

/// `MouseLButtonDoublePressed`: on one of the player's living units, its type selected in the view; else the
/// base's. The trigger tab's: a unit of a complex unit under the mouse has its type selected in the view; then the
/// click ends.
pub fn double_click(rec: &R, h: &mut impl HudHost) -> i64 {
    if rec.get(|r| r.mTrigger) {
        let unit = h.call(EE_SCREENS_SU_PICK, 0, 0);
        if unit != 0 {
            if h.call(EE_SCREENS_SU_HAS_COMPLEX, unit, 0) != 0 {
                h.call(EE_SCREENS_SU_LASSO_TYPE, unit, 1);
            }
            h.call(EE_SCREENS_SU_DEREF, unit, 0);
        }
        finish(rec, h);
        return 0;
    }
    let modifiers = h.call(EE_SCREENS_HC_MODIFIERS, 0, 0);
    let unit = h.call(EE_SCREENS_SU_PICK, 0, 0);
    let result = if unit != 0 && h.call(EE_SCREENS_SU_OWN_LIVE, unit, 0) != 0 {
        h.call(EE_SCREENS_SU_LASSO_TYPE, unit, modifiers);
        finish(rec, h);
        0
    } else {
        h.call(EE_SCREENS_HC_DOUBLE_BASE, 0, 0)
    };
    if unit != 0 {
        h.call(EE_SCREENS_SU_DEREF, unit, 0);
    }
    result
}

/// `MouseLButtonUp`: the units in the dragged box selected, or the unit under a plain click in the view.
pub fn mouse_l_up(rec: &R, h: &mut impl HudHost) -> i64 {
    if rec.get(|r| r.mTrigger) {
        return trigger_l_up(rec, h);
    }
    let modifiers = h.call(EE_SCREENS_HC_MODIFIERS, 0, 0);
    if rec.get(|r| r.mDragging) {
        rec.set(|r| r.mD1 = false);
        let (start, end) = rec.get(|r| (r.mDragStart.ge(), r.mDragEnd.ge()));
        let r = EeScreensRect::of_ge(select::release_box(start, end));
        h.with_rect(EE_SCREENS_SU_LASSO_BOX, modifiers, r);
    } else if h.call(EE_SCREENS_HC_VIEW_HAS_MOUSE, 0, 0) != 0 {
        h.call(EE_SCREENS_SU_CLICK, modifiers, 0);
    }
    finish(rec, h);
    0
}

/// The trigger tab's `MouseLButtonUp`: the dragged box added to the selection (the right button: its selected units
/// deselected), or at the mouse in the view the unit added (the right button: the click's deselection at the map
/// point); then the click ends, the right button up.
fn trigger_l_up(rec: &R, h: &mut impl HudHost) -> i64 {
    let mut point = [0.0f32; 2];
    h.op(EE_SCREENS_SU_POINT_AT, 0, 0, &mut point);
    let right = rec.get(|r| r.mRightButton);
    if rec.get(|r| r.mDragging) {
        rec.set(|r| r.mD1 = false);
        let (start, end) = rec.get(|r| (r.mDragStart.ge(), r.mDragEnd.ge()));
        let r = EeScreensRect::of_ge(select::release_box(start, end));
        if right {
            h.with_rect(EE_SCREENS_SU_LASSO_IN_RECT, 2, r);
        } else {
            h.with_rect(EE_SCREENS_SU_LASSO_BOX, 1, r);
        }
    } else if h.call(EE_SCREENS_HC_VIEW_HAS_MOUSE, 0, 0) != 0 {
        if right {
            h.op(EE_SCREENS_SU_CLICK_EXECUTE, 0, 0, &mut point);
        } else {
            h.call(EE_SCREENS_SU_CLICK, 1, 0);
        }
    }
    finish(rec, h);
    rec.set(|r| r.mRightButton = false);
    0
}

/// The trigger tab's `MouseRButtonDown`: the left press's, the right button noted.
pub fn r_down(rec: &R, h: &mut impl HudHost) -> i64 {
    rec.set(|r| r.mRightButton = true);
    h.call(EE_SCREENS_SU_L_DOWN, 0, 0)
}

/// The trigger tab's `MouseRButtonUp`: after a right press, the left release's; else nothing.
pub fn r_up(rec: &R, h: &mut impl HudHost) -> i64 {
    if !rec.get(|r| r.mRightButton) {
        return 0;
    }
    let result = mouse_l_up(rec, h);
    rec.set(|r| r.mRightButton = false);
    result
}

/// `MouseMove`: the drag end follows the mouse (clamped to the view); the drag starts past the threshold; a moved
/// end resizes the box and redraws it.
pub fn mouse_move(rec: &R, h: &mut impl HudHost, x: f32, y: f32) -> i64 {
    let old = rec.get(|r| r.mDragEnd);
    let view = h.rect(EE_SCREENS_HC_VIEW_RECT).ge();
    let end = EeScreensPoint::of_ge(select::drag_end(EeScreensPoint { x, y }.ge(), &view));
    rec.set(|r| r.mDragEnd = end);
    if !rec.get(|r| r.mDragging)
        && h.call(EE_SCREENS_HC_DISPLAY_MODE, 0, 0) == EE_SCREENS_HC_SHOWN
        && select::starts_drag(rec.get(|r| r.mDragStart).ge(), end.ge())
    {
        rec.set(|r| r.mDragging = true);
    }
    if old == end {
        return 0;
    }
    let (start, end) = rec.get(|r| (r.mDragStart.ge(), r.mDragEnd.ge()));
    h.with_rect(EE_SCREENS_HC_SET_RECTANGLE, 0, EeScreensRect::of_ge(select::drag_box(start, end)));
    let now = h.rect(EE_SCREENS_HC_RECT);
    h.with_rect(EE_SCREENS_HC_ADD_DIRTY, 0, now);
    let now = h.rect(EE_SCREENS_HC_RECT);
    h.with_rect(EE_SCREENS_HC_FORM_REDRAW, 0, now);
    0
}

/// `StartSelection`: a drag from the press, the mouse confined to the game view.
pub fn start(rec: &R, h: &mut impl HudHost, x: f32, y: f32) {
    let start = EeScreensPoint::of_ge(select::start_point(EeScreensPoint { x, y }.ge()));
    rec.set(|r| r.mDragStart = start);
    let view = h.rect(EE_SCREENS_HC_VIEW_RECT);
    h.with_rect(EE_SCREENS_HC_MOUSE_BOUNDS, 0, view);
    h.with_rect(EE_SCREENS_HC_SET_RECTANGLE, 0, EeScreensRect::new(start.x, start.y, start.x, start.y));
    h.call(EE_SCREENS_HC_CAPTURE_KEYBOARD, 0, 0);
    h.call(EE_SCREENS_HC_CAPTURE_MOUSE, 0, 0);
    h.call(EE_SCREENS_HC_RELEASE_KEYBOARD, 0, 0);
    h.call(EE_SCREENS_HC_SET_DISPLAY_MODE, EE_SCREENS_HC_SHOWN, 0);
    h.op(EE_SCREENS_HC_UPDATE_MOUSE, 0, 0, &mut [x, y]);
}

#[cfg(test)]
mod tests;
