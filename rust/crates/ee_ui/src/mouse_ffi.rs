//! `extern "C"` for the mouse's dispatch (`User Interface/UIControl.cpp`; the form's pass is the form object's,
//! `formobj.rs`): the control is a C++ object, asked through the toolkit's shims (`lle_control.rs`) on each step, never cached.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_void};

use crate::lle_control;
use crate::lle_cpp::{Cpp, Ffi};
use crate::mouse::{self, ButtonQuery, ControlMouseCall, ControlMouseHost};

struct ControlRef {
    control: *mut c_void,
    location: *const c_void,
}

// the buttons, the button-down members and the handlers through the toolkit's shims (UIControl.cpp; lle_control.rs)
impl ControlMouseHost for ControlRef {
    fn button(&mut self, right: bool, q: ButtonQuery) -> bool {
        lle_control::button(&mut Ffi, right, q)
    }
    fn down(&self, right: bool) -> bool {
        let control = self.control as usize;
        if right {
            Ffi.control_right_down(control)
        } else {
            Ffi.control_left_down(control)
        }
    }
    fn set_down(&mut self, right: bool, down: bool) {
        let control = self.control as usize;
        if right {
            Ffi.control_set_right_down(control, down)
        } else {
            Ffi.control_set_left_down(control, down)
        }
    }
    fn call(&mut self, c: ControlMouseCall) -> c_long {
        lle_control::handler(&mut Ffi, self.control as usize, self.location as usize, c)
    }
}

/// `UIControl::UpdateMouse(*location)` (`location` a `GE2DPoint*`, handed back to `MouseMove`) -> its result.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_control_update_mouse(control: *mut c_void, location: *const c_void) -> c_long {
    mouse::control_update_mouse(&mut ControlRef { control, location })
}
