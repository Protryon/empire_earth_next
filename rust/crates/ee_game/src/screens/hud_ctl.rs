//! The game's own HUD controls whole on their Rust records (R6 game screens round 29): the lobby's ping meter
//! (`EEUICPing`, [`crate::ping`]), a chat line (`EEUICChatLine`, [`crate::chat_line`]), the box selection
//! (`EEUICSelectUnits`, [`crate::select_units`]) and the right-drag command (`EEUICCommandUnits`,
//! [`crate::command_units`]). Each C++ class is a `UIControl` (through `EEUIRustControl`, `User Interface
//! Thread/EEUIRustControl.cpp`) whose own members are a Rust record (`EeScreens*Fields`, the class's protected base,
//! the members under their C++ names); the C++ objects a control owns (texts, timers, rhombi, lines, the model,
//! the click and lasso actions) stay C++ members, reached through the host.
//!
//! The host is one entry, `ee_screenscpp_hud_ctl(control, op, a, b, f)`: the ops below 32 are any control's
//! (`EEUIRustControl::HostOp`: the toolkit's virtuals and base members, the mouse, the keyboard, the view), the
//! others each class's own (`RustOp`). `f` carries an op's floats (rectangles as left, top, right, bottom). The
//! records are reached by address and read or written afresh around every op: an op can re-enter the control
//! (`UpdateMouse` calls the virtual `MouseMove`).

// ---- any control's ops (EEUIRustControl::HostOp)
/// (dirty) `SetDirty`.
pub const EE_SCREENS_HC_SET_DIRTY: u32 = 0;
/// (mode) `SetDisplayMode` (the virtual).
pub const EE_SCREENS_HC_SET_DISPLAY_MODE: u32 = 1;
/// `SetRectangle(f[0..4])` (the virtual).
pub const EE_SCREENS_HC_SET_RECTANGLE: u32 = 2;
/// `SetBgRectangle(f[0..4])`.
pub const EE_SCREENS_HC_SET_BG_RECTANGLE: u32 = 3;
/// `SetBgOpacity(f[0])`.
pub const EE_SCREENS_HC_SET_BG_OPACITY: u32 = 4;
/// -> `UIControl::CopyData()`.
pub const EE_SCREENS_HC_COPY_BASE: u32 = 5;
/// -> `UIControl::DrawPolygons()`.
pub const EE_SCREENS_HC_DRAW_BASE: u32 = 6;
/// -> `UIControl::DrawTextA()`.
pub const EE_SCREENS_HC_TEXT_BASE: u32 = 7;
/// -> `UIControl::MouseLButtonDoublePressed()`.
pub const EE_SCREENS_HC_DOUBLE_BASE: u32 = 8;
/// `CaptureMouse()`.
pub const EE_SCREENS_HC_CAPTURE_MOUSE: u32 = 9;
/// `CaptureKeyboard()`.
pub const EE_SCREENS_HC_CAPTURE_KEYBOARD: u32 = 10;
/// `ReleaseCapturedMouse()`.
pub const EE_SCREENS_HC_RELEASE_MOUSE: u32 = 11;
/// `ReleaseCapturedKeyboard()`.
pub const EE_SCREENS_HC_RELEASE_KEYBOARD: u32 = 12;
/// `UpdateMouse(f[0..2])` (calls the virtual `MouseMove` back).
pub const EE_SCREENS_HC_UPDATE_MOUSE: u32 = 13;
/// -> f[0..4] `GetRectangle()`.
pub const EE_SCREENS_HC_RECT: u32 = 14;
/// -> f[0..4] `GetCopiedDrawRectangle()`.
pub const EE_SCREENS_HC_COPIED_RECT: u32 = 15;
/// -> `GetDisplayMode()`.
pub const EE_SCREENS_HC_DISPLAY_MODE: u32 = 16;
/// -> the keyboard's modifier state.
pub const EE_SCREENS_HC_MODIFIERS: u32 = 17;
/// -> f[0..4] the game view's rectangle (`gViewport`).
pub const EE_SCREENS_HC_VIEW_RECT: u32 = 18;
/// The mouse's bounds set to f[0..4].
pub const EE_SCREENS_HC_MOUSE_BOUNDS: u32 = 19;
/// The mouse's bounds set to the game window.
pub const EE_SCREENS_HC_MOUSE_BOUNDS_WINDOW: u32 = 20;
/// The form's `RedrawIntersectControls(f[0..4])`.
pub const EE_SCREENS_HC_FORM_REDRAW: u32 = 21;
/// The manager's `AddDirtyRectangle(f[0..4], form)`.
pub const EE_SCREENS_HC_ADD_DIRTY: u32 = 22;
/// -> whether the game view's rectangle contains the control's mouse location.
pub const EE_SCREENS_HC_VIEW_HAS_MOUSE: u32 = 23;
/// -> the game window's width.
pub const EE_SCREENS_HC_WINDOW_WIDTH: u32 = 24;
/// A class's own ops from here (`EE_SCREENS_HC_CLASS + op` reaches its `RustOp` as `op`; the portrait's, the icon
/// bar's).
pub const EE_SCREENS_HC_CLASS: u32 = 64;

/// `kDisplayModeHidden`, `kDisplayModeShown`.
pub const EE_SCREENS_HC_HIDDEN: i64 = 0;
pub const EE_SCREENS_HC_SHOWN: i64 = 1;

/// A control's calls out.
pub trait HudHost {
    /// An `EE_SCREENS_HC_*` op (or a class's own) with its arguments and floats; its answer.
    fn op(&mut self, op: u32, a: i64, b: i64, f: &mut [f32]) -> i64;
}

/// The ops' common shapes.
pub(crate) trait HudOps: HudHost {
    /// An op without floats.
    fn call(&mut self, op: u32, a: i64, b: i64) -> i64 {
        self.op(op, a, b, &mut [])
    }
    /// An op answering a rectangle.
    fn rect(&mut self, op: u32) -> EeScreensRect {
        let mut f = [0.0; 4];
        self.op(op, 0, 0, &mut f);
        EeScreensRect::from_ltrb(f)
    }
    /// An op taking a rectangle.
    fn with_rect(&mut self, op: u32, a: i64, r: EeScreensRect) -> i64 {
        self.op(op, a, 0, &mut r.ltrb())
    }
    /// An op answering a point.
    fn point(&mut self, op: u32) -> EeScreensPoint {
        let mut f = [0.0; 2];
        self.op(op, 0, 0, &mut f);
        EeScreensPoint { x: f[0], y: f[1] }
    }
}

impl<H: HudHost + ?Sized> HudOps for H {}

/// `GERectangle` in its member order (the records' rectangles are read as `GERectangle`s by the C++).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensRect {
    pub bottom: f32,
    pub left: f32,
    pub right: f32,
    pub top: f32,
}

impl EeScreensRect {
    pub fn new(left: f32, top: f32, right: f32, bottom: f32) -> Self {
        EeScreensRect { bottom, left, right, top }
    }
    pub fn from_ltrb(f: [f32; 4]) -> Self {
        Self::new(f[0], f[1], f[2], f[3])
    }
    pub fn ltrb(&self) -> [f32; 4] {
        [self.left, self.top, self.right, self.bottom]
    }
    pub(crate) fn ge(&self) -> ee_base::math::types::GeRectangle {
        ee_base::math::types::GeRectangle { bottom: self.bottom, left: self.left, right: self.right, top: self.top }
    }
    pub(crate) fn of_ge(r: ee_base::math::types::GeRectangle) -> Self {
        EeScreensRect { bottom: r.bottom, left: r.left, right: r.right, top: r.top }
    }
}

/// `GE2DPoint`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensPoint {
    pub x: f32,
    pub y: f32,
}

impl EeScreensPoint {
    pub(crate) fn ge(&self) -> ee_base::math::types::Ge2DPoint {
        ee_base::math::types::Ge2DPoint { x: self.x, y: self.y }
    }
    pub(crate) fn of_ge(p: ee_base::math::types::Ge2DPoint) -> Self {
        EeScreensPoint { x: p.x, y: p.y }
    }
}

/// `GE3DPoint`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensPoint3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

/// A record in a C++ control, reached by address: each access a fresh read or write (no reference is held across
/// an op).
pub struct Rec<T>(*mut T);

impl<T> Rec<T> {
    /// # Safety
    /// `p` is a live record for as long as the value is used.
    pub unsafe fn new(p: *mut T) -> Self {
        Rec(p)
    }
    pub fn get<R>(&self, f: impl FnOnce(&T) -> R) -> R {
        // SAFETY: live (the constructor's contract); the borrow ends before any op
        f(unsafe { &*self.0 })
    }
    pub fn set(&self, f: impl FnOnce(&mut T)) {
        // SAFETY: as get
        f(unsafe { &mut *self.0 })
    }
}

/// The x64 build's float to `unsigned long` conversion (`cvttss2si` to 64 bits, the low 32 kept): NaN and values
/// outside the 64-bit range give 0, negative values wrap.
pub(crate) fn cvtt_u32(value: f32) -> u32 {
    if value.is_nan() || !(-9.223_372e18..9.223_372e18).contains(&value) {
        0
    } else {
        value as i64 as u32
    }
}

/// The C++ host (`ee_screenscpp_hud_ctl`) for the control `cpp`.
#[cfg(not(test))]
pub(crate) struct Cpp(pub *mut core::ffi::c_void);

#[cfg(not(test))]
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_hud_ctl(ctl: *mut c_void, op: u32, a: i64, b: i64, f: *mut f32) -> i64;
    }
}

#[cfg(not(test))]
impl HudHost for Cpp {
    fn op(&mut self, op: u32, a: i64, b: i64, f: &mut [f32]) -> i64 {
        if let Some(r) = crate::screens::hud_direct::control(self.0, op, a, f) {
            return r;
        }
        // SAFETY: the control is live while its member runs; f's length covers the op's floats
        unsafe { sys::ee_screenscpp_hud_ctl(self.0, op, a, b, f.as_mut_ptr()) }
    }
}

#[cfg(test)]
pub(crate) mod replay;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cvtt_wraps_and_zeroes_as_x64() {
        assert_eq!(cvtt_u32(2.9), 2);
        assert_eq!(cvtt_u32(-1.5), u32::MAX);
        assert_eq!(cvtt_u32(f32::NAN), 0);
        assert_eq!(cvtt_u32(1e19), 0);
        assert_eq!(cvtt_u32(4_294_967_296.0), 0);
        assert_eq!(cvtt_u32(4_294_967_295.0), 0);
    }

    #[test]
    fn rect_keeps_gerectangle_order() {
        let r = EeScreensRect::new(1.0, 2.0, 3.0, 4.0);
        assert_eq!(r.ltrb(), [1.0, 2.0, 3.0, 4.0]);
        assert_eq!((r.bottom, r.left, r.right, r.top), (4.0, 1.0, 3.0, 2.0));
        assert_eq!(core::mem::size_of::<EeScreensRect>(), 16);
    }
}
