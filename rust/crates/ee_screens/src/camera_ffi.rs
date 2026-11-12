//! The camera events' host over the C++ calls (`camera.rs`; `UI Events/EEScreens.cpp`) and the forms' grab scroll
//! records.

use core::ffi::c_long;
use std::sync::Mutex;

use crate::camera::{CameraGrab, CameraGrabs, CameraHost};
use crate::menus_ffi::Form;

/// The main game form's and the scenario editor's grab scrolls.
static GRABS: Mutex<CameraGrabs> = Mutex::new([CameraGrab { grabbing: false }; 2]);

/// One of the camera events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::camera::dispatch(host, &GRABS, event)
}

impl CameraHost for Form {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32 {
        self.op(op, a, b, 0, 0)
    }
}

/// A form's grab scroll (0 the main game form's, 1 the scenario editor's) made anew or released with its form: not
/// grabbing (`EECreateGrabScroll`, `EEReleaseGrabScroll`).
#[no_mangle]
pub extern "C" fn ee_screens_camera_grab_reset(form: u32) {
    crate::camera::reset_grab(&GRABS, form as usize);
}
