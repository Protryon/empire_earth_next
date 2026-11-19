//! `extern "C"` for the UI thread's game starts and its loop (`ui_game.rs`). The host is `ee_screenscpp_ui_game`
//! (`User Interface Thread/EEUserInterface.cpp`) on the UI object.

use core::ffi::c_void;

use crate::screens::ui_game;
use crate::screens::ui_thread::UiHost;

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
        if let Some(r) = crate::screens::ui_objects_ffi::ui_game(self.0, op, a, b, c) {
            return r;
        }
        if let Some(r) = crate::screens::hud_views::ui_game(op, a, b) {
            return r;
        }
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

/// `EEUserInterface::ShowLagStatus(player, lagging)` (`ui_game::lag_status`) on the toolkit's Rust objects: under the
/// manager's copy lock, the current form compared with the main game form (11) and the lobby (26); the waiting
/// message through the host's string and chat log texts; the lobby's lag button (control 0x111) by index.
#[no_mangle]
pub extern "C" fn ee_screens_ui_show_lag_status(player: u32, lagging: bool) {
    use ee_ui::buttonobj::EeUiButtonCall;
    use ee_ui::direct;
    use ee_ui::managerobj::EeUiManagerCall;
    use ee_ui::obj::EeUiOp;
    use ui_game::LagStatus;
    const FORM_MAIN_GAME: core::ffi::c_ulong = 11;
    const FORM_LOBBY: core::ffi::c_ulong = 26;
    const LAG_BUTTON: core::ffi::c_ulong = 0x111;
    const IDS_WAITING_FOR_PLAYERS: u32 = 30606;
    let m = direct::manager_of(core::ptr::null_mut());
    direct::manager_call(m, EeUiManagerCall::LockForCopy, 0, 0, 0);
    let current = direct::manager_call(m, EeUiManagerCall::CurrentForm, 0, 0, 0) as *mut c_void;
    let main_game = current == direct::form_by_id(m, FORM_MAIN_GAME);
    let lobby = !main_game && current == direct::form_by_id(m, FORM_LOBBY);
    match ui_game::lag_status(main_game, lobby, player, lagging) {
        LagStatus::Nothing => {}
        LagStatus::WaitingMessage => {
            let mut host = crate::screens::menus_ffi::Form(core::ptr::null_mut(), core::ptr::null_mut());
            let text = host.get(crate::screens::menus::EE_SCREENS_TEXT_STRING, IDS_WAITING_FOR_PLAYERS);
            host.put(crate::screens::main_game::EE_SCREENS_TEXT_MG_CHAT_LOG, 0, &text);
        }
        plan @ (LagStatus::HideButton | LagStatus::BlinkButton) => {
            let button = direct::control(current, LAG_BUTTON);
            let shown = plan == LagStatus::BlinkButton;
            direct::control_virtual(button, EeUiOp::SetDisplayMode, shown as isize, 0, &mut [0.0; 4]);
            if shown {
                direct::button_call(button, EeUiButtonCall::BlinkSetIsBlinking, 1, 0);
            }
        }
    }
    direct::manager_call(m, EeUiManagerCall::LockForCopy, 1, 0, 0);
}
