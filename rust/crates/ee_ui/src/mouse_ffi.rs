//! `extern "C"` for the mouse's dispatch (`User Interface/UIControl.cpp`, `UIForm.cpp`): the control, the form,
//! the input system's entities and the events are C++ objects, asked through `ee_uicpp_control_mouse` and
//! `ee_uicpp_form_mouse_*` on each step, never cached (the six mouse entities are the pointers the C++ handed back).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_int, c_long, c_void};

use crate::mouse::{
    self, ButtonQuery, Control, ControlMouseCall, ControlMouseHost, EntityQuery, FormMouseEvent, FormMouseHost,
    MouseEntity,
};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_int, c_long, c_void};

    extern "C" {
        // call: 0..3 the left button's JustDoublePressed, JustUnpressed, GetValue != 0, JustPressed; 4..7 the right
        // button's; 8 mLeftButtonDown, 9 mRightButtonDown; 10 mLeftButtonDown = arg, 11 mRightButtonDown = arg;
        // 12 + ControlMouseCall: the handler (MouseMove(*location))
        pub fn ee_uicpp_control_mouse(control: *mut c_void, location: *const c_void, call: c_int, arg: c_int) -> c_long;
        // which: 0 mMouseCaptured, 1 mMouseOverControl
        pub fn ee_uicpp_form_mouse_control(form: *mut c_void, which: c_int) -> *mut c_void;
        // call: 0 control->UpdateMouse(mMouseLocation), 1 UIManager::GetMouseWheel() != NULL,
        // 2 UIManager::GetMouseWheelScrollAmount(), 3 control->MouseWheelTurned(arg)
        pub fn ee_uicpp_form_mouse_call(form: *mut c_void, call: c_int, control: *mut c_void, arg: c_long) -> c_long;
        // gISMouse->GetEntity(the MouseEntity's index)
        pub fn ee_uicpp_form_mouse_entity(which: c_int) -> *mut c_void;
        // an entity asked an EntityQuery
        pub fn ee_uicpp_mouse_entity_ask(entity: *mut c_void, query: c_int) -> bool;
        // whether the FormMouseEvent member is set (process false), or its ProcessEvent(form, NULL)
        pub fn ee_uicpp_form_mouse_event(form: *mut c_void, event: c_int, process: bool) -> c_long;
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

struct FormRef {
    form: *mut c_void,
    entities: [*mut c_void; 6],
}

impl FormRef {
    fn control(&self, which: c_int) -> Option<Control> {
        let c = unsafe { sys::ee_uicpp_form_mouse_control(self.form, which) };
        (!c.is_null()).then_some(c as Control)
    }
    fn call(&self, call: c_int, c: Control, arg: c_long) -> c_long {
        unsafe { sys::ee_uicpp_form_mouse_call(self.form, call, c as *mut c_void, arg) }
    }
}

impl FormMouseHost for FormRef {
    fn captured(&self) -> Option<Control> {
        self.control(0)
    }
    fn mouse_over(&self) -> Option<Control> {
        self.control(1)
    }
    fn control_update(&mut self, c: Control) -> c_long {
        self.call(0, c, 0)
    }
    fn wheel(&self) -> bool {
        self.call(1, 0, 0) != 0
    }
    fn wheel_amount(&mut self) -> c_long {
        self.call(2, 0, 0)
    }
    fn wheel_turned(&mut self, c: Control, amount: c_long) -> c_long {
        self.call(3, c, amount)
    }
    fn entity(&mut self, e: MouseEntity) -> bool {
        let p = unsafe { sys::ee_uicpp_form_mouse_entity(e as c_int) };
        self.entities[e as usize] = p;
        !p.is_null()
    }
    fn ask(&mut self, e: MouseEntity, q: EntityQuery) -> bool {
        unsafe { sys::ee_uicpp_mouse_entity_ask(self.entities[e as usize], q as c_int) }
    }
    fn has_event(&self, ev: FormMouseEvent) -> bool {
        unsafe { sys::ee_uicpp_form_mouse_event(self.form, ev as c_int, false) != 0 }
    }
    fn process(&mut self, ev: FormMouseEvent) -> c_long {
        unsafe { sys::ee_uicpp_form_mouse_event(self.form, ev as c_int, true) }
    }
}

/// `UIForm::UpdateMouse` -> its result.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_update_mouse(form: *mut c_void) -> c_long {
    mouse::form_update_mouse(&mut FormRef { form, entities: [core::ptr::null_mut(); 6] })
}
