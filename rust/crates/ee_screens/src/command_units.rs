//! The main game view's right-drag command (`EEUICCommandUnits`) whole: the right press notes the map point under
//! the mouse and whether a selected unit takes a facing, the drag past 2% of the window turns a facing arrow toward
//! the ground point under the mouse, the release sends the command with the dragged facing (or, for units that take
//! one, facing away from the group's centre). The record is the class's members but the click action, the arrow's
//! model and texture (C++ objects: the arrow's draw and the ground point's projection are host ops). The scenario
//! editor's subclass (`EESEUICCommandUnits`, `mEditor` set) is the same control: its drag rotates the selected units,
//! its right click without a drag deletes the unit under the mouse.

use ee_render::ge::types::Ge2DPoint;
use ee_render::ge::um::{lrintf, um_arctan2};

use crate::hud_ctl::{
    EeScreensPoint, EeScreensPoint3, EeScreensRect, HudHost, HudOps, Rec, EE_SCREENS_HC_CAPTURE_KEYBOARD,
    EE_SCREENS_HC_CAPTURE_MOUSE, EE_SCREENS_HC_COPY_BASE, EE_SCREENS_HC_DRAW_BASE, EE_SCREENS_HC_FORM_REDRAW,
    EE_SCREENS_HC_HIDDEN, EE_SCREENS_HC_MODIFIERS, EE_SCREENS_HC_RECT, EE_SCREENS_HC_RELEASE_KEYBOARD,
    EE_SCREENS_HC_RELEASE_MOUSE, EE_SCREENS_HC_SET_DISPLAY_MODE, EE_SCREENS_HC_SET_RECTANGLE, EE_SCREENS_HC_SHOWN,
    EE_SCREENS_HC_UPDATE_MOUSE, EE_SCREENS_HC_WINDOW_WIDTH,
};

/// The facing arrow drawn at f[1..4] turned by f[0], over everything (the rasterizer's state restored after).
pub const EE_SCREENS_CU_DRAW_ARROW: u32 = 70;
/// The ground point under the mouse at f[0..2] at the height f[2] -> f[0..3].
pub const EE_SCREENS_CU_GROUND: u32 = 71;
/// The unit or tile under the point f[0..2] -> whether there is one; f[2..5] (in: the point to keep) its map point.
pub const EE_SCREENS_CU_POINT_AT: u32 = 72;
/// -> f[0..2] the centre of the selected group's bounds.
pub const EE_SCREENS_CU_GROUP_CENTER: u32 = 73;
/// (modifiers, flags `EE_SCREENS_CU_EXECUTE_*`) the click action's `Execute(f[0], f[1], ..., f[2], ...)`.
pub const EE_SCREENS_CU_EXECUTE: u32 = 74;
/// -> the selected group's unit count.
pub const EE_SCREENS_CU_GROUP_COUNT: u32 = 75;
/// (i) -> the group's unit i's class's category.
pub const EE_SCREENS_CU_CATEGORY: u32 = 76;
/// The editor's: its rotation action posted with the facing f[0] (`EESEARotateUnits::Rotate`).
pub const EE_SCREENS_CU_ROTATE: u32 = 77;
/// The editor's: -> the unit under the mouse (`EEUnit*`, referenced; 0 for none).
pub const EE_SCREENS_CU_PICK: u32 = 78;
/// The editor's: (unit) its deletion (`EESEADeleteUnit` of its id: posted when it exists).
pub const EE_SCREENS_CU_DELETE: u32 = 79;
/// The editor's: (unit) released.
pub const EE_SCREENS_CU_UNIT_RELEASE: u32 = 82;

/// `ee_screens_command_units`' members (`hud_ctl_ffi.rs`).
pub const EE_SCREENS_CU_DO_INIT: u32 = 0;
pub const EE_SCREENS_CU_DO_COPY_DATA: u32 = 1;
pub const EE_SCREENS_CU_DO_DRAW: u32 = 2;
pub const EE_SCREENS_CU_DO_MOUSE_MOVE: u32 = 3;
pub const EE_SCREENS_CU_DO_MOUSE_R_UP: u32 = 4;
pub const EE_SCREENS_CU_DO_START: u32 = 5;

/// [`EE_SCREENS_CU_EXECUTE`]'s flags: a unit or tile was hit, a facing is set, the facing was dragged.
pub const EE_SCREENS_CU_EXECUTE_HIT: i64 = 1;
pub const EE_SCREENS_CU_EXECUTE_FACING: i64 = 2;
pub const EE_SCREENS_CU_EXECUTE_DRAGGED: i64 = 4;

/// The drag that sets a facing, as a fraction of the window's width.
const DRAG_FRACTION: f32 = 0.02;

/// The unit categories that take a facing (naval, land, citizens, helicopters, submarines, priests, heroes,
/// transports, spaceships, naval buildings).
const FACING_CATEGORIES: [i64; 18] =
    [4, 5, 6, 7, 0xa, 0xd, 0xe, 0x16, 8, 0xf, 0x17, 0x10, 0x11, 0x12, 0x21, 0x1f, 0x1a, 0x15];

/// `EEUICCommandUnits`' members (its protected base) but the click action, the model and the texture.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Debug, Default)]
pub struct EeScreensCommandUnitsFields {
    /// The press (rounded).
    pub mDownLocation: EeScreensPoint,
    /// The map point under the press.
    pub mPosition: EeScreensPoint3,
    pub mFacing: f32,
    /// `mFacing` as of the last `CopyData`.
    pub mDrawnFacing: f32,
    /// A selected unit takes a facing.
    pub mCanFace: bool,
    /// The drag went past the threshold (or the release set a facing).
    pub mHasFacing: bool,
    /// `mHasFacing` as of the last `CopyData`.
    pub mDrawnHasFacing: bool,
    /// The scenario editor's control (`EESEUICCommandUnits`; port addition: the subclass's overrides are this).
    pub mEditor: bool,
}

type R = Rec<EeScreensCommandUnitsFields>;

/// `ComputeAngle`: the heading from one point to another in the ground plane.
fn angle(from: EeScreensPoint3, to: EeScreensPoint3) -> f32 {
    um_arctan2(-(to.x - from.x), to.y - from.y)
}

/// The constructor's part: no facing.
pub fn init(rec: &R) {
    rec.set(|r| {
        r.mFacing = 0.0;
        r.mCanFace = false;
        r.mHasFacing = false;
    });
}

/// `CopyData`: whether and where the arrow is drawn.
pub fn copy_data(rec: &R, h: &mut impl HudHost) -> i64 {
    rec.set(|r| {
        r.mDrawnHasFacing = r.mHasFacing;
        r.mDrawnFacing = r.mFacing;
    });
    h.call(EE_SCREENS_HC_COPY_BASE, 0, 0)
}

/// `DrawPolygons`: the arrow when the snapshot has a facing, then the base's.
pub fn draw(rec: &R, h: &mut impl HudHost) -> i64 {
    if rec.get(|r| r.mDrawnHasFacing) {
        let (facing, p) = rec.get(|r| (r.mDrawnFacing, r.mPosition));
        h.op(EE_SCREENS_CU_DRAW_ARROW, 0, 0, &mut [facing, p.x, p.y, p.z]);
    }
    h.call(EE_SCREENS_HC_DRAW_BASE, 0, 0)
}

/// `MouseMove`: once the drag passes the threshold (for units that take a facing), the facing turns toward the
/// ground point under the mouse.
pub fn mouse_move(rec: &R, h: &mut impl HudHost, x: f32, y: f32) -> i64 {
    let threshold = h.call(EE_SCREENS_HC_WINDOW_WIDTH, 0, 0) as u32 as f32 * DRAG_FRACTION;
    let down = rec.get(|r| r.mDownLocation);
    let distance = Ge2DPoint { x, y }.distance_to_squared(down.ge());
    let mut f = [x, y, rec.get(|r| r.mPosition.z)];
    h.op(EE_SCREENS_CU_GROUND, 0, 0, &mut f);
    let target = EeScreensPoint3 { x: f[0], y: f[1], z: f[2] };
    if rec.get(|r| r.mCanFace && !r.mHasFacing) {
        rec.set(|r| r.mHasFacing = threshold * threshold < distance);
    }
    if rec.get(|r| r.mHasFacing) {
        let facing = angle(rec.get(|r| r.mPosition), target);
        rec.set(|r| r.mFacing = facing);
    }
    // the editor's: the selected units turn with the drag
    if rec.get(|r| r.mEditor && r.mHasFacing) {
        h.op(EE_SCREENS_CU_ROTATE, 0, 0, &mut [rec.get(|r| r.mFacing)]);
    }
    0
}

/// `MouseRButtonUp`: the command sent at the press's point with the facing, then hidden. The editor's: without a
/// drag, the unit under the mouse deleted; then hidden.
pub fn mouse_r_up(rec: &R, h: &mut impl HudHost) -> i64 {
    if rec.get(|r| r.mEditor) {
        if !rec.get(|r| r.mHasFacing) {
            let unit = h.call(EE_SCREENS_CU_PICK, 0, 0);
            if unit != 0 {
                h.call(EE_SCREENS_CU_DELETE, unit, 0);
                h.call(EE_SCREENS_CU_UNIT_RELEASE, unit, 0);
            }
        }
        h.call(EE_SCREENS_HC_SET_DISPLAY_MODE, EE_SCREENS_HC_HIDDEN, 0);
        h.call(EE_SCREENS_HC_RELEASE_MOUSE, 0, 0);
        return 0;
    }
    let modifiers = h.call(EE_SCREENS_HC_MODIFIERS, 0, 0);
    let dragged = rec.get(|r| r.mHasFacing);
    let down = rec.get(|r| r.mDownLocation);
    let mut f = [down.x, down.y, 0.0, 0.0, 0.0];
    let hit = h.op(EE_SCREENS_CU_POINT_AT, 0, 0, &mut f) != 0;
    let point = [f[2], f[3]];
    if rec.get(|r| r.mCanFace && !r.mHasFacing) {
        let c = h.point(EE_SCREENS_CU_GROUP_CENTER);
        let p = rec.get(|r| r.mPosition);
        let facing = angle(EeScreensPoint3 { x: c.x, y: c.y, z: p.z }, p);
        rec.set(|r| {
            r.mFacing = facing;
            r.mHasFacing = true;
        });
    }
    let (has, facing) = rec.get(|r| (r.mHasFacing, r.mFacing));
    let flags = if hit { EE_SCREENS_CU_EXECUTE_HIT } else { 0 }
        | if has { EE_SCREENS_CU_EXECUTE_FACING } else { 0 }
        | if dragged { EE_SCREENS_CU_EXECUTE_DRAGGED } else { 0 };
    h.op(EE_SCREENS_CU_EXECUTE, modifiers, flags, &mut [point[0], point[1], facing]);
    h.call(EE_SCREENS_HC_SET_DISPLAY_MODE, EE_SCREENS_HC_HIDDEN, 0);
    h.call(EE_SCREENS_HC_RELEASE_MOUSE, 0, 0);
    0
}

/// `StartCommand`: on a unit or tile, the drag starts there (noting whether a selected unit takes a facing);
/// elsewhere the command is sent at once without a target. The editor's: then any unit may be turned.
pub fn start(rec: &R, h: &mut impl HudHost, x: f32, y: f32) {
    start_command(rec, h, x, y);
    if rec.get(|r| r.mEditor) {
        rec.set(|r| r.mCanFace = true);
    }
}

fn start_command(rec: &R, h: &mut impl HudHost, x: f32, y: f32) {
    let modifiers = h.call(EE_SCREENS_HC_MODIFIERS, 0, 0);
    let count = h.call(EE_SCREENS_CU_GROUP_COUNT, 0, 0) as u32;
    let p = rec.get(|r| r.mPosition);
    let mut f = [x, y, p.x, p.y, p.z];
    let hit = h.op(EE_SCREENS_CU_POINT_AT, 0, 0, &mut f) != 0;
    rec.set(|r| r.mPosition = EeScreensPoint3 { x: f[2], y: f[3], z: f[4] });
    if !hit {
        h.op(EE_SCREENS_CU_EXECUTE, modifiers, 0, &mut [0.0, 0.0, 0.0]);
        return;
    }
    let mut can_face = false;
    let mut i = 0;
    while i < count && !can_face {
        can_face = FACING_CATEGORIES.contains(&h.call(EE_SCREENS_CU_CATEGORY, i as i64, 0));
        i += 1;
    }
    let down = EeScreensPoint { x: lrintf(x) as f32, y: lrintf(y) as f32 };
    rec.set(|r| {
        r.mCanFace = can_face;
        r.mHasFacing = false;
        r.mDownLocation = down;
    });
    h.with_rect(EE_SCREENS_HC_SET_RECTANGLE, 0, EeScreensRect::new(down.x, down.y, down.x, down.y));
    let now = h.rect(EE_SCREENS_HC_RECT);
    h.with_rect(EE_SCREENS_HC_FORM_REDRAW, 0, now);
    h.call(EE_SCREENS_HC_CAPTURE_KEYBOARD, 0, 0);
    h.call(EE_SCREENS_HC_CAPTURE_MOUSE, 0, 0);
    h.call(EE_SCREENS_HC_RELEASE_KEYBOARD, 0, 0);
    h.op(EE_SCREENS_HC_UPDATE_MOUSE, 0, 0, &mut [x, y]);
    h.call(EE_SCREENS_HC_SET_DISPLAY_MODE, EE_SCREENS_HC_SHOWN, 0);
}

#[cfg(test)]
mod tests;
