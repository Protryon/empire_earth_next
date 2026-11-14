//! `extern "C"` for the UI thread's game starts and its loop (`ui_game.rs`). The host is `ee_screenscpp_ui_game`
//! (`User Interface Thread/EEUserInterface.cpp`) on the UI object.

use core::ffi::c_void;

use crate::ui_game;
use crate::ui_thread::UiHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_ui_game(ui: *mut c_void, op: u32, a: i64, b: i64, c: i64) -> i64;
    }
}

struct GameHost(*mut c_void);

impl UiHost for GameHost {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        unsafe { sys::ee_screenscpp_ui_game(self.0, op, a, b, c) }
    }
}

/// `EEUserInterface::TestScenario` on the UI object `ui`.
#[no_mangle]
pub extern "C" fn ee_screens_ui_test_scenario(ui: *mut c_void) {
    ui_game::test_scenario(&mut GameHost(ui));
}

/// `EEUserInterface::SaveAndTestScenario` on the UI object `ui`.
#[no_mangle]
pub extern "C" fn ee_screens_ui_save_and_test_scenario(ui: *mut c_void) {
    ui_game::save_and_test_scenario(&mut GameHost(ui));
}

/// `EEUserInterface::LoadSavedGame(file)` on the UI object `ui` (`file` an `FSFile*`, deleted).
#[no_mangle]
pub extern "C" fn ee_screens_ui_load_saved_game(ui: *mut c_void, file: *mut c_void) {
    ui_game::load_saved_game(&mut GameHost(ui), file as i64);
}

/// `EEUserInterface::Run` on the UI object `ui`: the UI thread's loop until it quits.
#[no_mangle]
pub extern "C" fn ee_screens_ui_run(ui: *mut c_void) -> i64 {
    ui_game::run(&mut GameHost(ui))
}

/// `EEUserInterface`'s member `which` (`EE_SCREENS_UG_DO_*`: `LeaveMap`, `CancelGame`, `CloseScenarioEditor`,
/// `EnterScenarioEditor`, `StartLoadingFromEditor`, `RestartGame`, `ShowCivilizationBuilder`) with its arguments on
/// the UI object `ui`.
#[no_mangle]
pub extern "C" fn ee_screens_ui_member(ui: *mut c_void, which: u32, a: i64, b: i64, c: i64) {
    ui_game::member(&mut GameHost(ui), which, a, b, c);
}
