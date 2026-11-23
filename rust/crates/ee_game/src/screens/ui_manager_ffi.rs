//! `extern "C"` for the game's UI manager (`ui_manager.rs`). The host is `ee_screenscpp_ui_manager`
//! (`User Interface Thread/EEUIManager.cpp`) on the host's record of one call (the manager, the names, the mouse).

use core::ffi::c_void;

use crate::screens::ui_manager;
use crate::screens::ui_objects::{self, EeScreensUiManagerFields};
use crate::screens::ui_thread::UiHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_um_engine_count() -> i64;
        pub fn ee_screenscpp_um_music(call: *mut c_void);
        pub fn ee_screenscpp_um_citizens() -> i64;
        pub fn ee_screenscpp_um_post_f9();
        pub fn ee_screenscpp_um_base_keyboard(call: *mut c_void) -> i64;
        pub fn ee_screenscpp_um_base_mouse(call: *mut c_void) -> i64;
    }
}

struct ManagerHost(*mut c_void);

impl UiHost for ManagerHost {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        if op == ui_manager::EE_SCREENS_UM_OP_FORMS {
            crate::screens::ui_objects_ffi::manager_forms();
            return 0;
        }
        // the cases that are Rust's over the C++ shims (ui_manager_calls.rs), else the C++ host
        if let Some(r) =
            crate::screens::ui_manager_calls::call(&mut crate::screens::game_cpp::Ffi, self.0 as usize, op, a, c)
        {
            return r;
        }
        // SAFETY: the host's live call record (the mouse's location set for the base mouse)
        unsafe {
            match op {
                ui_manager::EE_SCREENS_UM_OP_ENGINE_COUNT => sys::ee_screenscpp_um_engine_count(),
                ui_manager::EE_SCREENS_UM_OP_MUSIC => {
                    sys::ee_screenscpp_um_music(self.0);
                    0
                }
                ui_manager::EE_SCREENS_UM_OP_CITIZENS => sys::ee_screenscpp_um_citizens(),
                ui_manager::EE_SCREENS_UM_OP_POST_F9 => {
                    sys::ee_screenscpp_um_post_f9();
                    0
                }
                ui_manager::EE_SCREENS_UM_OP_BASE_KEYBOARD => sys::ee_screenscpp_um_base_keyboard(self.0),
                ui_manager::EE_SCREENS_UM_OP_BASE_MOUSE => sys::ee_screenscpp_um_base_mouse(self.0),
                _ => {
                    let _ = (b, c);
                    0
                }
            }
        }
    }
}

/// `EEUIManager`'s constructor's work after its base, on the host's call record `call`.
#[no_mangle]
pub extern "C" fn ee_screens_ui_manager_construct(call: *mut c_void) {
    ui_manager::construct(&mut ManagerHost(call));
}

/// `EEUIManager`'s constructor body (UI round 16): its own members' record `rec` made (the default state, the
/// manager registered), then [`ee_screens_ui_manager_construct`] on `call`.
///
/// # Safety
/// `rec` is the live manager's record base; `call` its call record.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ui_manager_new(rec: *mut EeScreensUiManagerFields, call: *mut c_void) {
    // SAFETY: the contract
    unsafe { ui_objects::ee_screens_ui_manager_fields(rec) };
    ee_screens_ui_manager_construct(call);
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

// ---- the UI thread's calls of other units' functions whose bodies are Rust (UI round 16: the shim list names these
// instead of a C++ shim calling the C++ forwarder)

/// `EEResetGraphicsValues()` (EERenderObject.cpp): the game's render objects posted, not all.
#[no_mangle]
pub extern "C" fn ee_screens_reset_graphics_values() {
    crate::gamecpp::render_object::ee_gamecpp_ro_post_render_objects(false);
}

/// `EEChangeGameScreen(width, height, flag)` (Empire Earth.cpp).
#[no_mangle]
pub extern "C" fn ee_screens_change_game_screen(width: u32, height: u32, flag: bool) {
    crate::gamecpp::startup::ee_gamecpp_startup_change_screen(
        width as core::ffi::c_ulong,
        height as core::ffi::c_ulong,
        flag,
    );
}

/// `EEUpdateLandContinents()` (EECloakingUtilities.cpp: the units' cloak body).
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_screens_update_land_continents() {
    extern "C" {
        fn ee_units_ck_body(what: i32, a: u64, b: u64, c: i64, d: i64) -> i64;
    }
    // SAFETY: the units' cloak body's op with no arguments
    unsafe { ee_units_ck_body(ee_sim::units::uu_cloak::EE_UNITS_CK_DO_UPDATE_LAND_CONTINENTS, 0, 0, 0, 0) };
}
