//! `DX7Screen` whole (`Common/DX7Screen.cpp`, deleted): the display modes, the desktop rectangle, the name, the
//! DirectDraw device, the current mode and the flags, the statics `sScreens`, `sScreensCaptured` and
//! `sFirstCaptureWasAttachedToDesktop`, and every method of the class over [`crate::screen`]'s algorithms. The C++
//! class keeps its vtable (the game calls it through `GEScreen`), `CreateAWindow` (the platform window calls) and one
//! member, the object; every call on another C++ object (the name's `UString`, the surfaces, the probe window) is one
//! host operation ([`EeDx7ObjOp`]).
//!
//! No reference to the object lives across a host call: [`ScreenRef`] holds the raw pointer and re-reads the object
//! per call (creating a surface or the probe window calls into C++).
#![allow(clippy::missing_safety_doc, clippy::not_unsafe_ptr_arg_deref, clippy::unnecessary_cast)] // c_ulong differs per target

use core::ffi::c_ulong;
use std::sync::Mutex;

use crate::dx7::api::*;
#[cfg(not(test))]
use crate::dx7::screen::WindowParams;
use crate::dx7::screen::{self, CaptureState, ScreenHost, VideoMode};
use crate::dx7::surfobj::{EeDx7ObjHostFn, EeDx7ObjOp, EeDx7Surface};

/// The screen.
pub struct EeDx7Screen {
    host: EeDx7ObjHostFn,
    /// The C++ screen (`this`).
    owner: H,
    /// `mModes`.
    pub modes: Vec<VideoMode>,
    /// `mDesktopRectangle` (a `GERectangle`: bottom, left, right, top).
    pub desktop: [f32; 4],
    /// `mName` (a C++ `UString*`).
    pub name: H,
    /// `mDirectDraw`.
    pub dd: H,
    /// `mMonitor` (always NULL).
    pub monitor: H,
    /// `mCurrentModeIndex`.
    pub current_mode: c_ulong,
    pub attached_to_desktop: bool,
    pub full_screen: bool,
    pub shares_primary: bool,
    pub can_mag_filter: bool,
    pub can_min_filter: bool,
    pub device32: bool,
    pub texture32: bool,
}

/// `sScreens`: the C++ screens (`DX7Screen*`) in creation order.
static SCREENS: Mutex<Vec<usize>> = Mutex::new(Vec::new());
/// `sScreensCaptured` and `sFirstCaptureWasAttachedToDesktop`.
static CAPTURE: Mutex<CaptureState> = Mutex::new(CaptureState { captured: 0, first_attached: false });

/// # Safety
/// `p` is live; the reference is not held across a host call.
unsafe fn sc<'a>(p: *mut EeDx7Screen) -> &'a mut EeDx7Screen {
    // SAFETY: the caller's contract
    unsafe { &mut *p }
}

/// `sScreens[index]` (NULL past the list).
pub fn screen_at(index: usize) -> H {
    SCREENS.lock().unwrap().get(index).map_or(NULL, |s| *s as H)
}

/// `sScreens.size()`.
pub fn screen_count() -> usize {
    SCREENS.lock().unwrap().len()
}

/// The capture statics.
pub fn capture_state() -> CaptureState {
    *CAPTURE.lock().unwrap()
}

/// A fresh process's capture statics (the tests).
#[cfg(test)]
pub(crate) fn reset_capture_state() {
    *CAPTURE.lock().unwrap() = CaptureState::default();
}

/// The object as the [`ScreenHost`] of the algorithms.
pub struct ScreenRef(pub *mut EeDx7Screen);

impl ScreenRef {
    /// # Safety
    /// The object is live; the operation's arguments are as [`EeDx7ObjOp`] describes.
    unsafe fn op(&self, op: EeDx7ObjOp, obj: H, a: isize, b: isize) -> isize {
        // SAFETY: the caller's contract (the host is copied out before the call)
        let host = unsafe { sc(self.0).host };
        // SAFETY: the host's contract
        unsafe { host(op, obj, a, b) }
    }

    /// The object of a `DX7Surface`.
    fn surface(&self, surface: H) -> *mut EeDx7Surface {
        // SAFETY: the object is live; `surface` is one this screen made
        unsafe { self.op(EeDx7ObjOp::SurfaceObject, surface, 0, 0) as *mut EeDx7Surface }
    }
}

impl ScreenHost for ScreenRef {
    fn new_surface(&mut self, gx_surface: H, desc: &SurfaceDescription) -> H {
        // SAFETY: the object is live
        unsafe {
            let owner = sc(self.0).owner;
            self.op(EeDx7ObjOp::SurfaceNew, gx_surface, desc as *const SurfaceDescription as isize, owner as isize) as H
        }
    }
    fn push_attached(&mut self, parent: H, child: H) {
        let p = self.surface(parent);
        // SAFETY: the surface object is live
        unsafe { (*p).attached.push(child) };
    }
    fn surface_gx(&mut self, surface: H) -> H {
        // SAFETY: the surface object is live
        unsafe { (*self.surface(surface)).gx }
    }
    fn surface_dereference(&mut self, surface: H) {
        // SAFETY: the object is live
        unsafe { self.op(EeDx7ObjOp::SurfaceDereference, surface, 0, 0) };
    }
    fn set_filter_caps(&mut self, min_filter: bool, mag_filter: bool) {
        // SAFETY: the object is live
        let s = unsafe { sc(self.0) };
        s.can_min_filter = min_filter;
        s.can_mag_filter = mag_filter;
    }
    fn push_mode(&mut self, mode: VideoMode) {
        // SAFETY: the object is live
        unsafe { sc(self.0).modes.push(mode) };
    }
    fn set_current_mode(&mut self, index: u32) {
        // SAFETY: the object is live
        unsafe { sc(self.0).current_mode = index as c_ulong };
    }
    fn create_probe_window(&mut self) -> (i32, H) {
        let mut window: H = NULL;
        // SAFETY: the object is live
        let result = unsafe {
            let owner = sc(self.0).owner;
            self.op(EeDx7ObjOp::ProbeWindow, owner, &mut window as *mut H as isize, 0)
        };
        (result as i32, window)
    }
    fn capture_window<G: Gx, O: Host>(&mut self, gx: &mut G, host: &mut O, window: H) -> i32 {
        // SAFETY: the object is live
        unsafe { EeDx7Screen::capture(self.0, gx, host, window, false) }
    }
    fn release_window<G: Gx, O: Host>(&mut self, gx: &mut G, host: &mut O, window: H) -> i32 {
        // SAFETY: the object is live
        unsafe { EeDx7Screen::release(self.0, gx, host, window) }
    }
    fn delete_window(&mut self, window: H) {
        // SAFETY: the object is live
        unsafe { self.op(EeDx7ObjOp::WindowDelete, window, 0, 0) };
    }
    fn dispatch_window_events(&mut self) {
        // SAFETY: the object is live
        unsafe { self.op(EeDx7ObjOp::DispatchEvents, NULL, 0, 0) };
    }
    fn mode_count(&mut self) -> u32 {
        // SAFETY: the object is live
        unsafe { sc(self.0).modes.len() as u32 }
    }
    fn mode(&mut self, index: u32) -> VideoMode {
        // the probe reads mode `count` when no 800 x 600 x 16 mode exists: past the vector in the original, zeros here
        // SAFETY: the object is live
        unsafe { sc(self.0).modes.get(index as usize).copied().unwrap_or_default() }
    }
    fn set_32bit_support(&mut self, device: bool, texture: bool) {
        // SAFETY: the object is live
        let s = unsafe { sc(self.0) };
        s.device32 = device;
        s.texture32 = texture;
    }
}

impl EeDx7Screen {
    /// `DX7Screen(inMonitor, inDirectDraw, inName, inAttachedToDesktop, inSharesPrimary)`'s fields and name; the
    /// device queries follow in [`Self::start`] (once the C++ holds the object).
    ///
    /// # Safety
    /// `name` is a `UString*` the host copies.
    pub unsafe fn create(
        host: EeDx7ObjHostFn,
        owner: H,
        monitor: H,
        dd: H,
        name: H,
        attached_to_desktop: bool,
        shares_primary: bool,
    ) -> *mut Self {
        // SAFETY: the host's contract
        let name = unsafe { host(EeDx7ObjOp::NameNew, name, 0, 0) as H };
        Box::into_raw(Box::new(EeDx7Screen {
            host,
            owner,
            modes: Vec::new(),
            desktop: [0.0; 4],
            name,
            dd,
            monitor,
            current_mode: 0,
            attached_to_desktop,
            full_screen: false,
            shares_primary,
            can_mag_filter: false,
            can_min_filter: false,
            device32: false,
            texture32: false,
        }))
    }

    /// The rest of the constructor: lists the display modes, finds the current one, probes 32-bit support (not on
    /// macOS: GX renders and samples 32-bit everywhere, and a probe window took the key window from the full screen
    /// game window), registers the screen.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn start<G: Gx, O: Host>(p: *mut Self, gx: &mut G, host: &mut O) {
        // SAFETY: the caller's contract
        let (dd, owner) = unsafe { (sc(p).dd, sc(p).owner) };
        screen::initialize(gx, &mut ScreenRef(p), dd);
        if cfg!(target_os = "macos") {
            ScreenRef(p).set_32bit_support(true, true);
        } else {
            screen::test_32bit_support(gx, host, &mut ScreenRef(p), dd);
        }
        SCREENS.lock().unwrap().push(owner as usize);
    }

    /// `~DX7Screen`: frees the name, leaves the screen list, releases the device, then frees the object.
    ///
    /// # Safety
    /// `p` was made by [`Self::create`] and is not used again.
    pub unsafe fn destroy<G: Gx>(p: *mut Self, gx: &mut G) {
        // SAFETY: the caller's contract
        unsafe {
            let (host, name, owner, dd) = (sc(p).host, sc(p).name, sc(p).owner as usize, sc(p).dd);
            host(EeDx7ObjOp::NameDelete, name, 0, 0);
            {
                let mut screens = SCREENS.lock().unwrap();
                if let Some(i) = screens.iter().position(|s| *s == owner) {
                    screens.remove(i);
                }
            }
            gx.dd_release(dd);
            drop(Box::from_raw(p));
        }
    }

    /// `Capture`: takes the screen for `window` (a `GEWindow*`), full-screen exclusive or windowed.
    ///
    /// # Safety
    /// `p` is live; `window` is a live `GEWindow`.
    pub unsafe fn capture<G: Gx, O: Host>(p: *mut Self, gx: &mut G, host: &mut O, window: H, exclusive: bool) -> i32 {
        // SAFETY: the caller's contract
        unsafe {
            let os = ScreenRef(p).op(EeDx7ObjOp::WindowOs, window, 0, 0) as H;
            let mut state = capture_state();
            let (dd, attached) = (sc(p).dd, sc(p).attached_to_desktop);
            let mut full = sc(p).full_screen;
            let result = screen::capture(gx, host, dd, os, exclusive, attached, &mut state, &mut full);
            sc(p).full_screen = full;
            *CAPTURE.lock().unwrap() = state;
            result
        }
    }

    /// `Release`: back to the normal cooperative level; uncounts the capture.
    ///
    /// # Safety
    /// `p` is live; `window` is a live `GEWindow`.
    pub unsafe fn release<G: Gx, O: Host>(p: *mut Self, gx: &mut G, host: &mut O, window: H) -> i32 {
        // SAFETY: the caller's contract
        unsafe {
            let os = ScreenRef(p).op(EeDx7ObjOp::WindowOs, window, 0, 0) as H;
            let mut state = capture_state();
            let mut full = sc(p).full_screen;
            let result = screen::release(gx, host, sc(p).dd, os, &mut state, &mut full);
            sc(p).full_screen = full;
            CAPTURE.lock().unwrap().captured = state.captured;
            result
        }
    }

    /// `CreateSurface`.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn create_surface<G: Gx, O: Host>(
        p: *mut Self,
        gx: &mut G,
        host: &mut O,
        description: &mut SurfaceDescription,
        out: &mut H,
    ) -> i32 {
        // SAFETY: the caller's contract
        let dd = unsafe { sc(p).dd };
        screen::create_surface(gx, host, &mut ScreenRef(p), dd, description, out)
    }

    /// `GetDesktopRectangle`: 0, 0 to the current mode's size.
    ///
    /// # Safety
    /// `p` is live; its current mode is one of its modes.
    pub unsafe fn desktop_rectangle(p: *mut Self) -> *const f32 {
        // SAFETY: the caller's contract
        let s = unsafe { sc(p) };
        let mode = s.modes[s.current_mode as usize];
        s.desktop = [mode.mHeight as f32, 0.0, mode.mWidth as f32, 0.0];
        s.desktop.as_ptr()
    }

    /// `SetMode`: a full-screen screen's display mode; records the index (even on failure).
    ///
    /// # Safety
    /// `p` is live; `index` is one of its modes.
    pub unsafe fn set_mode<G: Gx, O: Host>(p: *mut Self, gx: &mut G, host: &mut O, index: c_ulong) -> i32 {
        // SAFETY: the caller's contract
        unsafe {
            let (dd, full, mode) = (sc(p).dd, sc(p).full_screen, sc(p).modes[index as usize]);
            let (result, update) = screen::set_mode(gx, host, dd, full, &mode);
            if update {
                sc(p).current_mode = index;
            }
            result
        }
    }
}

#[cfg(not(test))]
pub use ffi::*;

#[cfg(not(test))]
mod ffi {
    use core::ffi::c_void;

    use super::*;
    use crate::dx7::gx::{RawGx, RawHost};

    /// The screen's fields and name (`name` a `UString*`); [`ee_dx7sc_start`] follows.
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_new(
        host: EeDx7ObjHostFn,
        owner: *mut c_void,
        monitor: *mut c_void,
        dd: *mut c_void,
        name: *const c_void,
        attached_to_desktop: bool,
        shares_primary: bool,
    ) -> *mut EeDx7Screen {
        unsafe { EeDx7Screen::create(host, owner, monitor, dd, name as H, attached_to_desktop, shares_primary) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_start(p: *mut EeDx7Screen) {
        unsafe { EeDx7Screen::start(p, &mut RawGx, &mut RawHost) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_destroy(p: *mut EeDx7Screen) {
        unsafe { EeDx7Screen::destroy(p, &mut RawGx) }
    }

    /// `Capture` (`window` a `GEWindow*`).
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_capture(p: *mut EeDx7Screen, window: *mut c_void, exclusive: bool) -> i32 {
        unsafe { EeDx7Screen::capture(p, &mut RawGx, &mut RawHost, window, exclusive) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_release(p: *mut EeDx7Screen, window: *mut c_void) -> i32 {
        unsafe { EeDx7Screen::release(p, &mut RawGx, &mut RawHost, window) }
    }

    /// `CreateSurface`: the description is the C++ reference argument (its updates kept).
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_create_surface(
        p: *mut EeDx7Screen,
        description: *mut SurfaceDescription,
        out: *mut *mut c_void,
    ) -> i32 {
        let mut local = unsafe { *description };
        let mut surface = NULL;
        let result = unsafe { EeDx7Screen::create_surface(p, &mut RawGx, &mut RawHost, &mut local, &mut surface) };
        unsafe {
            *description = local;
            *out = surface;
        }
        result
    }

    /// `GetDesktopRectangle`: a `GERectangle`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_desktop_rectangle(p: *mut EeDx7Screen) -> *const f32 {
        unsafe { EeDx7Screen::desktop_rectangle(p) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_restore_all(p: *mut EeDx7Screen) -> i32 {
        screen::restore_all(&mut RawGx, &mut RawHost, unsafe { sc(p).dd })
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_set_mode(p: *mut EeDx7Screen, index: c_ulong) -> i32 {
        unsafe { EeDx7Screen::set_mode(p, &mut RawGx, &mut RawHost, index) }
    }

    /// `&mModes[index]` (unchecked, as the vector's index).
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_mode(p: *mut EeDx7Screen, index: c_ulong) -> *const VideoMode {
        unsafe { sc(p).modes.as_ptr().wrapping_add(index as usize) }
    }

    /// `GetCurrentMode`: `&mModes[mCurrentModeIndex]`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_current_mode(p: *mut EeDx7Screen) -> *const VideoMode {
        unsafe { sc(p).modes.as_ptr().wrapping_add(sc(p).current_mode as usize) }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_current_mode_index(p: *mut EeDx7Screen) -> c_ulong {
        unsafe { sc(p).current_mode }
    }

    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_mode_count(p: *mut EeDx7Screen) -> c_ulong {
        unsafe { sc(p).modes.len() as c_ulong }
    }

    /// `*mName` (a `UString*`).
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_name(p: *mut EeDx7Screen) -> *const c_void {
        unsafe { sc(p).name }
    }

    /// `mDirectDraw`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_direct_draw(p: *mut EeDx7Screen) -> *mut c_void {
        unsafe { sc(p).dd }
    }

    /// The flags by number: 0 attached to the desktop, 1 full screen, 2 shares the primary, 3 mag filter, 4 min filter,
    /// 5 32-bit device, 6 32-bit textures.
    #[no_mangle]
    pub unsafe extern "C" fn ee_dx7sc_flag(p: *mut EeDx7Screen, which: u32) -> bool {
        let s = unsafe { sc(p) };
        [
            s.attached_to_desktop,
            s.full_screen,
            s.shares_primary,
            s.can_mag_filter,
            s.can_min_filter,
            s.device32,
            s.texture32,
        ][which as usize]
    }

    /// `CreateAWindow`'s choice of platform call, size and flags (`sim`: the headless simulation).
    #[no_mangle]
    #[allow(clippy::too_many_arguments)]
    pub unsafe extern "C" fn ee_dx7sc_window_params(
        sim: bool,
        style: u32,
        width: i32,
        height: i32,
        display_width: i32,
        display_height: i32,
        x: f32,
        y: f32,
        out: *mut WindowParams,
    ) {
        unsafe { *out = screen::window_params(sim, style, width, height, display_width, display_height, x, y) };
    }

    /// `sScreens[index]` and `sScreens.size()`.
    #[no_mangle]
    pub extern "C" fn ee_dx7sc_screen(index: c_ulong) -> *mut c_void {
        screen_at(index as usize)
    }

    #[no_mangle]
    pub extern "C" fn ee_dx7sc_screen_count() -> c_ulong {
        screen_count() as c_ulong
    }
}

#[cfg(test)]
#[path = "scrobj_tests.rs"]
mod tests;

#[cfg(test)]
#[path = "scrobj_replay.rs"]
mod replay;
