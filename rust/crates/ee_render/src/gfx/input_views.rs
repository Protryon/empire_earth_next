//! The game's mouse (`gMouse`, an `EEMouse`: RTSMouse.h) and main viewport (`gViewport`, Empire Earth.cpp) for the
//! Rust callers (the UI lane's; the graphics + LLE cluster's third round). The C++ objects stay the C++'s (their
//! users are C++ and read them in place); each registers the addresses of its members here when it is made
//! (`EEMouse`'s constructor, `EEGraphics`'), and the accessors read and write those members as the C++'s inline
//! accessors do. Before the registration (or after the mouse's destruction) they answer None.
//!
//! The members are read as the C++ reads them, without a lock: the mouse's position is the input thread's
//! (`RTSMouse::Update` under `mInputLock`); a reader on another thread may see a position a frame old, as the C++
//! readers do.

use std::ffi::c_long;
use std::sync::Mutex;

use ee_base::math::types::{Ge2DPoint, GeRectangle};

use crate::ge::viewport::GeViewport;
use crate::gfx::mouse;

/// Where the mouse's members are (`&gMouse->mPosition`, ...).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGfxMouseView {
    pub position: *mut Ge2DPoint,
    pub bounds: *mut GeRectangle,
    pub wheel: *mut c_long,
    pub pointer: *mut c_long,
    pub overlay: *mut c_long,
    pub frozen: *mut bool,
    pub shift_mode: *mut bool,
    pub sensitivity: *mut f32,
}

// SAFETY: the addresses of a live C++ object's members, used only through the accessors below (the C++'s own reads
// cross threads alike)
unsafe impl Send for EeGfxMouseView {}

struct ViewportAt(*mut GeViewport);
// SAFETY: as above
unsafe impl Send for ViewportAt {}

static MOUSE: Mutex<Option<EeGfxMouseView>> = Mutex::new(None);
static VIEWPORT: Mutex<Option<ViewportAt>> = Mutex::new(None);

/// The mouse's members registered (NULL: the mouse is gone).
///
/// # Safety
/// `view` is NULL or its pointers are a live mouse's members until the next call.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_input_mouse_set(view: *const EeGfxMouseView) {
    // SAFETY: the caller's contract
    *MOUSE.lock().unwrap() = (!view.is_null()).then(|| unsafe { *view });
}

/// The main viewport registered (NULL: none).
///
/// # Safety
/// `viewport` is NULL or a `GEViewport` that lives until the next call.
#[no_mangle]
pub unsafe extern "C" fn ee_gfx_main_viewport_set(viewport: *mut GeViewport) {
    *VIEWPORT.lock().unwrap() = (!viewport.is_null()).then_some(ViewportAt(viewport));
}

fn mouse() -> Option<EeGfxMouseView> {
    *MOUSE.lock().unwrap()
}

fn read<T: Copy>(p: *mut T) -> T {
    // SAFETY: a registered member (the registration's contract); volatile as the C++'s volatile members
    unsafe { std::ptr::read_volatile(p) }
}

/// `gMouse->mPosition`.
pub fn mouse_position() -> Option<Ge2DPoint> {
    mouse().map(|m| read(m.position))
}

/// `gMouse->SetPosition(position)`: the position, clamped into the bounds (`ClampPosition`).
pub fn set_mouse_position(position: Ge2DPoint) {
    if let Some(m) = mouse() {
        let p = mouse::clamp_position(position, &read(m.bounds));
        // SAFETY: the registered member
        unsafe { std::ptr::write_volatile(m.position, p) };
    }
}

/// `gMouse->mBounds`.
pub fn mouse_bounds() -> Option<GeRectangle> {
    mouse().map(|m| read(m.bounds))
}

/// `gMouse->mWheel`.
pub fn mouse_wheel() -> Option<c_long> {
    mouse().map(|m| read(m.wheel))
}

/// `gMouse->mPointer` (the pointer picture: RTSMouse.cpp's and EEMouse.cpp's tables).
pub fn mouse_pointer() -> Option<c_long> {
    mouse().map(|m| read(m.pointer))
}

/// `gMouse->mPointer = pointer`.
pub fn set_mouse_pointer(pointer: c_long) {
    if let Some(m) = mouse() {
        // SAFETY: the registered (volatile) member
        unsafe { std::ptr::write_volatile(m.pointer, pointer) };
    }
}

/// `gMouse->mOverlay`.
pub fn mouse_overlay() -> Option<c_long> {
    mouse().map(|m| read(m.overlay))
}

/// `gMouse->mIsFrozen`.
pub fn mouse_frozen() -> Option<bool> {
    mouse().map(|m| read(m.frozen))
}

/// `gMouse->mShiftMode`.
pub fn mouse_shift_mode() -> Option<bool> {
    mouse().map(|m| read(m.shift_mode))
}

/// `gMouse->mSensitivity`.
pub fn mouse_sensitivity() -> Option<f32> {
    mouse().map(|m| read(m.sensitivity))
}

/// A copy of `gViewport` (its rectangles, planes, camera and clipping distances).
pub fn main_viewport() -> Option<GeViewport> {
    VIEWPORT.lock().unwrap().as_ref().map(|v| read(v.0))
}

#[cfg(test)]
#[path = "input_views_tests.rs"]
mod tests;
