//! The camera, scroll and zoom events whole: the original `EEUIECameraEvents.cpp` (the mouse move switch around the
//! free camera's rotation, its revolve and rotate), `EEUIEScrollZoomEvents.cpp` (the middle button's grab scroll, the
//! arrow keys' scroll, the bracket keys' zoom) and `EEUIEWheelZoom.cpp`, over a host ([`CameraHost`]) whose calls are
//! each one camera, mouse or settings call (`UI Events/EEScreens.cpp`). The main game form and the scenario editor's
//! form hang them on their keys, wheel and middle button.
//!
//! The grab scroll is the one event with state: per form (the main game form's, the scenario editor's) whether the
//! middle button is held, a [`CameraGrab`] record here; the mouse move event it saved while held (a C++ event
//! object) stays with the host, which hands the form's mouse move events to the grab and back.

use core::ffi::c_long;
use std::sync::Mutex;

use crate::main_game::{EE_SCREENS_GAME_MG_CAMERA_MODE, EE_SCREENS_GAME_MG_SET_CAMERA_MODE};

// ---- the events (EEScreenEvent::mEvent)

/// The grave key's press and release around the free camera's rotation (`EEUIEDisableMouseMove`,
/// `EEUIEEnableMouseMove`).
pub const EE_SCREENS_CAM_DISABLE_MOUSE_MOVE: u32 = 0x0b40;
pub const EE_SCREENS_CAM_ENABLE_MOUSE_MOVE: u32 = 0x0b41;
/// The free camera's revolve (the scenario editor's) and rotate (both forms', the grave key held).
pub const EE_SCREENS_CAM_REVOLVE: u32 = 0x0b42;
pub const EE_SCREENS_CAM_ROTATE: u32 = 0x0b43;
/// The arrow keys (`EEUIEKeyboardScroll(0..3)`: up, down, left, right).
pub const EE_SCREENS_CAM_SCROLL_UP: u32 = 0x0b44;
pub const EE_SCREENS_CAM_SCROLL_DOWN: u32 = 0x0b45;
pub const EE_SCREENS_CAM_SCROLL_LEFT: u32 = 0x0b46;
pub const EE_SCREENS_CAM_SCROLL_RIGHT: u32 = 0x0b47;
/// The bracket keys (`EEUIEHotKeyZoom(0)` in, `(1)` out).
pub const EE_SCREENS_CAM_ZOOM_IN: u32 = 0x0b48;
pub const EE_SCREENS_CAM_ZOOM_OUT: u32 = 0x0b49;
/// The mouse wheel.
pub const EE_SCREENS_CAM_WHEEL_ZOOM: u32 = 0x0b4a;
/// The middle button's grab scroll of the main game form and of the scenario editor's form (`EECreateGrabScroll`).
pub const EE_SCREENS_CAM_GRAB_MAIN: u32 = 0x0b4b;
pub const EE_SCREENS_CAM_GRAB_EDITOR: u32 = 0x0b4c;

pub fn is_camera_event(event: u32) -> bool {
    (EE_SCREENS_CAM_DISABLE_MOUSE_MOVE..=EE_SCREENS_CAM_GRAB_EDITOR).contains(&event)
}

// ---- the host's ops (ee_screenscpp_call on the event's form)

pub const EE_SCREENS_GAME_CAM_FREEZE: u32 = 0x280; // (frozen) gMouse->mIsFrozen
pub const EE_SCREENS_GAME_CAM_MOUSE_PRESENT: u32 = 0x281; // (axis) -> the mouse has the entity
pub const EE_SCREENS_GAME_CAM_MOUSE_VALUE: u32 = 0x282; // (axis, normalized) -> its GetValue / GetNormalizedValue
pub const EE_SCREENS_GAME_CAM_FREE: u32 = 0x283; // -> gFreeCamera
pub const EE_SCREENS_GAME_CAM_REVOLVE: u32 = 0x284; // (x, y) the camera's Revolve(x, y, 0)
pub const EE_SCREENS_GAME_CAM_ROTATE: u32 = 0x285; // (x, y) Rotate(x, y, 0)
pub const EE_SCREENS_GAME_CAM_ZOOM: u32 = 0x286; // -> the camera's zoom
pub const EE_SCREENS_GAME_CAM_GRAB_SPEED: u32 = 0x287; // -> the settings' grab scroll speed
pub const EE_SCREENS_GAME_CAM_SCROLL: u32 = 0x288; // (dx, dy) Scroll
pub const EE_SCREENS_GAME_CAM_ZOOM_ENABLED: u32 = 0x289; // (wheel) -> gZoomEnabled (wheel) or the settings' (keys)
pub const EE_SCREENS_GAME_CAM_CINEMATIC: u32 = 0x28a; // -> gWorld.mInCinematic
pub const EE_SCREENS_GAME_CAM_RESET_ZOOM: u32 = 0x28b; // ResetZoom
pub const EE_SCREENS_GAME_CAM_CHANGED: u32 = 0x28c; // SetChanged
pub const EE_SCREENS_GAME_CAM_SET_ZOOM: u32 = 0x28d; // (zoom) SetZoom
pub const EE_SCREENS_GAME_CAM_MAX_ZOOM: u32 = 0x28e; // -> gCameraMaxZoom (the port's farther zoom-out)
pub const EE_SCREENS_GAME_CAM_COS: u32 = 0x28f; // (angle) -> UMCos
pub const EE_SCREENS_OP_CAM_GRAB_BEGIN: u32 = 0x290; // (form) the form's mouse move event saved, the grab's set
pub const EE_SCREENS_OP_CAM_GRAB_END: u32 = 0x291; // (form) the saved one set back and released

/// The mouse's entities (`EE_SCREENS_GAME_CAM_MOUSE_*`).
const AXIS_X: u32 = 0;
const AXIS_Y: u32 = 1;
const MIDDLE_BUTTON: u32 = 2;
const WHEEL: u32 = 3;
/// The camera's modes: 2 free, 3 the zoom's own (the zoom events return it to 0 outside a cinematic).
const MODE_FREE: u32 = 2;
const MODE_ZOOM: u32 = 3;
const HALF_DEGREE: f32 = 0.008726646;
const KEYBOARD_SCROLL_FACTOR: f32 = 0.05;
const GRAB_SCROLL_FACTOR: f32 = 0.07;
const ZOOM_STEP_MINIMUM: f32 = 0.0035;
const ZOOM_STEP_SCALE: f32 = 0.006_499_999_7;
const HALF_PI: f32 = 1.570_796_4;
const WHEEL_ZOOM_STEP: f32 = 0.05;

/// A form's grab scroll: whether its middle button is held (the original's `EEUIEGrabScroll::mIsGrabbing`).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct CameraGrab {
    pub grabbing: bool,
}

/// The two forms' grab scrolls (0 the main game form's, 1 the scenario editor's).
pub type CameraGrabs = [CameraGrab; 2];

/// The camera's, the mouse's and the settings' calls of the camera events.
pub trait CameraHost {
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` with up to two arguments and an answer.
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32;
}

fn op<H: CameraHost + ?Sized>(h: &mut H, op: u32) -> u32 {
    h.call(op, 0, 0)
}
fn float<H: CameraHost + ?Sized>(h: &mut H, op: u32) -> f32 {
    f32::from_bits(h.call(op, 0, 0))
}
fn value<H: CameraHost + ?Sized>(h: &mut H, axis: u32, normalized: bool) -> i32 {
    h.call(EE_SCREENS_GAME_CAM_MOUSE_VALUE, axis, normalized as u32) as i32
}
/// `<windows.h>`'s `max` and `min` (the original's: `a > b ? a : b`, `a < b ? a : b`).
fn max(a: f32, b: f32) -> f32 {
    if a > b {
        a
    } else {
        b
    }
}
fn min(a: f32, b: f32) -> f32 {
    if a < b {
        a
    } else {
        b
    }
}

/// Mode 3 outside a cinematic: the camera back to mode 0 at the default zoom (the zoom events' first step).
fn leave_zoom_mode<H: CameraHost + ?Sized>(h: &mut H) -> bool {
    if op(h, EE_SCREENS_GAME_CAM_CINEMATIC) == 0 && op(h, EE_SCREENS_GAME_MG_CAMERA_MODE) == MODE_ZOOM {
        h.call(EE_SCREENS_GAME_MG_SET_CAMERA_MODE, 0, 0);
        op(h, EE_SCREENS_GAME_CAM_RESET_ZOOM);
        op(h, EE_SCREENS_GAME_CAM_CHANGED);
        return true;
    }
    false
}

/// `EEUIERevolveCamera` / `EEUIERotateCamera`: the free camera turned by the mouse's motion, half a degree a unit
/// (the revolve inverted; the rotate inverted at zoom 0).
fn turn<H: CameraHost + ?Sized>(h: &mut H, revolve: bool) {
    let x = value(h, AXIS_X, false);
    let y = value(h, AXIS_Y, false);
    let (mut ax, mut ay) = if revolve {
        // (the original negates the integer, then converts it)
        (x.wrapping_neg() as f32 * HALF_DEGREE, y.wrapping_neg() as f32 * HALF_DEGREE)
    } else {
        (x as f32 * HALF_DEGREE, y as f32 * HALF_DEGREE)
    };
    if !revolve && float(h, EE_SCREENS_GAME_CAM_ZOOM) == 0.0 {
        ax = -ax;
        ay = -ay;
    }
    if op(h, EE_SCREENS_GAME_MG_CAMERA_MODE) == MODE_FREE && op(h, EE_SCREENS_GAME_CAM_FREE) != 0 {
        let call = if revolve { EE_SCREENS_GAME_CAM_REVOLVE } else { EE_SCREENS_GAME_CAM_ROTATE };
        h.call(call, ax.to_bits(), ay.to_bits());
    }
}

/// `EEUIEKeyboardScroll`: one step up, down, left or right, scaled by the zoom and the grab scroll speed.
fn keyboard_scroll<H: CameraHost + ?Sized>(h: &mut H, direction: u32) {
    let speed = float(h, EE_SCREENS_GAME_CAM_GRAB_SPEED);
    let zoom = float(h, EE_SCREENS_GAME_CAM_ZOOM);
    let (mut dx, mut dy) = (0.0f32, 0.0f32);
    match direction {
        0 => dy = (speed + 0.5) * (zoom + 0.5) * KEYBOARD_SCROLL_FACTOR,
        1 => dy = -((zoom + 0.5) * ((speed + 0.5) * KEYBOARD_SCROLL_FACTOR)),
        2 => dx = -((zoom + 0.5) * ((speed + 0.5) * KEYBOARD_SCROLL_FACTOR)),
        _ => dx = (speed + 0.5) * (zoom + 0.5) * KEYBOARD_SCROLL_FACTOR,
    }
    h.call(EE_SCREENS_GAME_CAM_SCROLL, dx.to_bits(), dy.to_bits());
}

/// `EEUIEHotKeyZoom`: one step in or out (the step grows with the zoom), clamped to 0..the farthest zoom; in mode 3
/// outside a cinematic the camera's mode left instead; nothing with the zoom off.
fn hot_key_zoom<H: CameraHost + ?Sized>(h: &mut H, out: bool) {
    let mut zoom = float(h, EE_SCREENS_GAME_CAM_ZOOM);
    let angle = zoom * HALF_PI;
    let cos = f32::from_bits(h.call(EE_SCREENS_GAME_CAM_COS, (HALF_PI - angle).to_bits(), 0));
    let mut step = min(max(cos, 0.0), 1.0);
    if h.call(EE_SCREENS_GAME_CAM_ZOOM_ENABLED, 0, 0) == 0 || leave_zoom_mode(h) {
        return;
    }
    step = step * ZOOM_STEP_SCALE + ZOOM_STEP_MINIMUM;
    zoom = if out { zoom + step } else { zoom - step };
    zoom = min(max(zoom, 0.0), float(h, EE_SCREENS_GAME_CAM_MAX_ZOOM));
    h.call(EE_SCREENS_GAME_CAM_SET_ZOOM, zoom.to_bits(), 0);
}

/// `EEUIEWheelZoom`: the wheel's motion zooms (clamped to 0..the farthest zoom); in mode 3 outside a cinematic the
/// camera's mode left instead; nothing with the zoom off or no wheel.
fn wheel_zoom<H: CameraHost + ?Sized>(h: &mut H) {
    let mut zoom = float(h, EE_SCREENS_GAME_CAM_ZOOM);
    if h.call(EE_SCREENS_GAME_CAM_ZOOM_ENABLED, 1, 0) == 0
        || h.call(EE_SCREENS_GAME_CAM_MOUSE_PRESENT, WHEEL, 0) == 0
        || leave_zoom_mode(h)
    {
        return;
    }
    zoom -= value(h, WHEEL, true) as f32 * WHEEL_ZOOM_STEP;
    let farthest = float(h, EE_SCREENS_GAME_CAM_MAX_ZOOM);
    if zoom < 0.0 {
        zoom = 0.0;
    } else if zoom > farthest {
        zoom = farthest;
    }
    h.call(EE_SCREENS_GAME_CAM_SET_ZOOM, zoom.to_bits(), 0);
}

/// `EEUIEGrabScroll`: while the middle button is held the camera scrolls by the mouse's motion (scaled by the zoom
/// and the grab speed), the grab taking the form's mouse move events with the cursor frozen; the release gives them
/// back. Its form's record is locked only around its own reads and writes.
fn grab_scroll<H: CameraHost + ?Sized>(h: &mut H, grabs: &Mutex<CameraGrabs>, form: usize) {
    if h.call(EE_SCREENS_GAME_CAM_MOUSE_PRESENT, MIDDLE_BUTTON, 0) == 0 {
        return;
    }
    let grabbing = grabs.lock().unwrap()[form].grabbing;
    if grabbing {
        let x = value(h, AXIS_X, false);
        let speed = float(h, EE_SCREENS_GAME_CAM_GRAB_SPEED);
        let zoom = float(h, EE_SCREENS_GAME_CAM_ZOOM);
        let dx = x as f32 * (zoom + 0.5) * (speed + 0.5) * GRAB_SCROLL_FACTOR;
        let y = value(h, AXIS_Y, false);
        let dy = -(y as f32 * (speed + 0.5) * (zoom + 0.5) * GRAB_SCROLL_FACTOR);
        if dx != 0.0 || dy != 0.0 {
            h.call(EE_SCREENS_GAME_CAM_SCROLL, dx.to_bits(), dy.to_bits());
        }
        if value(h, MIDDLE_BUTTON, false) == 0 {
            grabs.lock().unwrap()[form].grabbing = false;
            h.call(EE_SCREENS_OP_CAM_GRAB_END, form as u32, 0);
            h.call(EE_SCREENS_GAME_CAM_FREEZE, 0, 0);
        }
    } else if value(h, MIDDLE_BUTTON, false) == 1 {
        grabs.lock().unwrap()[form].grabbing = true;
        h.call(EE_SCREENS_OP_CAM_GRAB_BEGIN, form as u32, 0);
        h.call(EE_SCREENS_GAME_CAM_FREEZE, 1, 0);
    }
}

/// One of the camera events; `None` for another form's, else its `ProcessEvent` result (always 0).
pub fn dispatch<H: CameraHost + ?Sized>(h: &mut H, grabs: &Mutex<CameraGrabs>, event: u32) -> Option<c_long> {
    match event {
        EE_SCREENS_CAM_DISABLE_MOUSE_MOVE | EE_SCREENS_CAM_ENABLE_MOUSE_MOVE => {
            if op(h, EE_SCREENS_GAME_MG_CAMERA_MODE) == MODE_FREE {
                h.call(EE_SCREENS_GAME_CAM_FREEZE, (event == EE_SCREENS_CAM_DISABLE_MOUSE_MOVE) as u32, 0);
            }
        }
        EE_SCREENS_CAM_REVOLVE => turn(h, true),
        EE_SCREENS_CAM_ROTATE => turn(h, false),
        EE_SCREENS_CAM_SCROLL_UP..=EE_SCREENS_CAM_SCROLL_RIGHT => keyboard_scroll(h, event - EE_SCREENS_CAM_SCROLL_UP),
        EE_SCREENS_CAM_ZOOM_IN | EE_SCREENS_CAM_ZOOM_OUT => hot_key_zoom(h, event == EE_SCREENS_CAM_ZOOM_OUT),
        EE_SCREENS_CAM_WHEEL_ZOOM => wheel_zoom(h),
        EE_SCREENS_CAM_GRAB_MAIN | EE_SCREENS_CAM_GRAB_EDITOR => {
            grab_scroll(h, grabs, (event - EE_SCREENS_CAM_GRAB_MAIN) as usize)
        }
        _ => return None,
    }
    Some(0)
}

/// A form's grab made anew or released with its form: not grabbing.
pub fn reset_grab(grabs: &Mutex<CameraGrabs>, form: usize) {
    if let Some(grab) = grabs.lock().unwrap().get_mut(form) {
        *grab = CameraGrab::default();
    }
}

#[cfg(test)]
mod golden;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn events() {
        assert!(is_camera_event(EE_SCREENS_CAM_DISABLE_MOUSE_MOVE) && is_camera_event(EE_SCREENS_CAM_GRAB_EDITOR));
        assert!(!is_camera_event(0x0b3f) && !is_camera_event(0x0b4d));
    }

    #[test]
    fn the_macros_keep_their_first_argument_on_a_tie_and_drop_a_nan() {
        assert_eq!(max(f32::NAN, 0.0), 0.0);
        assert_eq!(min(f32::NAN, 1.0), 1.0);
        assert_eq!(max(-0.0, 0.0).to_bits(), 0.0f32.to_bits());
    }

    #[test]
    fn a_reset_grab_is_not_grabbing() {
        let grabs = Mutex::new([CameraGrab { grabbing: true }; 2]);
        reset_grab(&grabs, 1);
        assert!(grabs.lock().unwrap()[0].grabbing && !grabs.lock().unwrap()[1].grabbing);
        reset_grab(&grabs, 7);
    }
}
