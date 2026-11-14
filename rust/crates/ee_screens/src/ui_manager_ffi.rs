//! `extern "C"` for the game's UI manager (`ui_manager.rs`). The host is `ee_screenscpp_ui_manager`
//! (`User Interface Thread/EEUIManager.cpp`) on the host's record of one call (the manager, the names, the mouse).

use core::ffi::c_void;

use crate::ui_manager;
use crate::ui_thread::UiHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_ui_manager(call: *mut c_void, op: u32, a: i64, b: i64, c: i64) -> i64;
    }
}

struct ManagerHost(*mut c_void);

impl UiHost for ManagerHost {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        unsafe { sys::ee_screenscpp_ui_manager(self.0, op, a, b, c) }
    }
}

/// `EEUIManager`'s constructor's work after its base, on the host's call record `call`.
#[no_mangle]
pub extern "C" fn ee_screens_ui_manager_construct(call: *mut c_void) {
    ui_manager::construct(&mut ManagerHost(call));
}

/// `EEUIManager::StartScenarioEditor` on the host's call record `call`.
#[no_mangle]
pub extern "C" fn ee_screens_ui_manager_start_editor(call: *mut c_void) {
    ui_manager::start_scenario_editor(&mut ManagerHost(call));
}

/// `EEUIManager::UpdateKeyboard` on the host's call record `call`: its result.
#[no_mangle]
pub extern "C" fn ee_screens_ui_manager_update_keyboard(call: *mut c_void) -> i64 {
    ui_manager::update_keyboard(&mut ManagerHost(call))
}

/// `EEUIManager::UpdateMouse` on the host's call record `call`: its result.
#[no_mangle]
pub extern "C" fn ee_screens_ui_manager_update_mouse(call: *mut c_void) -> i64 {
    ui_manager::update_mouse(&mut ManagerHost(call))
}
