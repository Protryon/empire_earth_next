//! `extern "C"` for the mouse's dispatch (`User Interface/UIControl.cpp`; the form's pass is the form object's,
//! `formobj.rs`): the control is a C++ object, asked through `ee_uicpp_control_mouse` on each step, never cached.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_int, c_long, c_void};

use crate::mouse::{self, ButtonQuery, ControlMouseCall, ControlMouseHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_int, c_long, c_void};

    extern "C" {
        // call: 0..3 the left button's JustDoublePressed, JustUnpressed, GetValue != 0, JustPressed; 4..7 the right
        // button's; 8 mLeftButtonDown, 9 mRightButtonDown; 10 mLeftButtonDown = arg, 11 mRightButtonDown = arg;
        // 12 + ControlMouseCall: the handler (MouseMove(*location))
        pub fn ee_uicpp_control_mouse(control: *mut c_void, location: *const c_void, call: c_int, arg: c_int)
            -> c_long;
    }
}

struct ControlRef {
    control: *mut c_void,
    location: *const c_void,
}

impl ControlRef {
    fn ask(&self, call: c_int, arg: c_int) -> c_long {
        unsafe { sys::ee_uicpp_control_mouse(self.control, self.location, call, arg) }
    }
}

impl ControlMouseHost for ControlRef {
    fn button(&mut self, right: bool, q: ButtonQuery) -> bool {
        self.ask(if right { 4 } else { 0 } + q as c_int, 0) != 0
    }
    fn down(&self, right: bool) -> bool {
        self.ask(if right { 9 } else { 8 }, 0) != 0
    }
    fn set_down(&mut self, right: bool, down: bool) {
        self.ask(if right { 11 } else { 10 }, down as c_int);
    }
    fn call(&mut self, c: ControlMouseCall) -> c_long {
        self.ask(12 + c as c_int, 0)
    }
}

/// `UIControl::UpdateMouse(*location)` (`location` a `GE2DPoint*`, handed back to `MouseMove`) -> its result.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_control_update_mouse(control: *mut c_void, location: *const c_void) -> c_long {
    mouse::control_update_mouse(&mut ControlRef { control, location })
}
