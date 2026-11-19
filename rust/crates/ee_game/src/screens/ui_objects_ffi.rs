//! `extern "C"` for the UI thread object (`ui_objects.rs`): `EEUserInterface`'s constructor, destructor and posts
//! (ee/EEUserInterface.h forwards here with its record, the object's only base); and the direct answers of the UI
//! hosts' ops on the two records, the UI thread's queue and the toolkit's manager (`ui_game_ffi.rs`,
//! `ui_thread_ffi.rs` and `ui_manager_ffi.rs` ask them before the C++ hop).
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use ee_base::ts::{EeTsEvent, EeTsThread};
use ee_ui::direct;
use ee_ui::managerobj::EeUiManagerCall;

use crate::screens::ui_objects::{self, EeScreensUserInterfaceFields};

/// `TSEvent`/`TSThread` pointers are their Rust objects.
fn thread(rec: &EeScreensUserInterfaceFields) -> *const EeTsThread {
    rec.mThread as *const EeTsThread
}

/// `mThread->Post(message, p1, p2, true)`.
fn post(rec: &EeScreensUserInterfaceFields, message: u32, p1: *mut c_void, p2: *mut c_void) -> c_long {
    // SAFETY: the UI object's live thread (made by the scheduler before the object)
    unsafe { ee_base::ts::ee_ts_thread_post(thread(rec), message as c_ulong, p1, p2, true) }
}

/// `EEUserInterface::EEUserInterface` (0x276631): the current thread, the flags, `TMGetTime()` as the seed and the
/// message-done event; the record registered as `gUserInterface`'s.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ui_construct(rec: *mut EeScreensUserInterfaceFields) {
    ui_objects::construct(&mut *rec, ee_base::ts::ee_ts_thread_current().cast(), ee_base::time::ee_time_get());
    let mut event: *mut EeTsEvent = core::ptr::null_mut();
    let result = ee_base::ts::ee_ts_event_new(&mut event, true);
    debug_assert_eq!(result, 0, "EEUserInterface.cpp line 177: CreateEvent");
    (*rec).mMessageQueueDone = event.cast();
    ui_objects::register_ui(rec);
}

/// `EEUserInterface::~EEUserInterface` (0x276740): every queued, undoable and redoable action released, the event
/// destroyed.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ui_destroy(rec: *mut EeScreensUserInterfaceFields) {
    for queue in 0..3 {
        crate::screens::actions_ffi::ee_screens_actions_clear(queue);
    }
    ee_base::ts::ee_ts_event_free((*rec).mMessageQueueDone.cast());
    ui_objects::forget_ui(rec);
}

/// One of `EEUserInterface`'s posts: `mThread->Post(message, p1, p2, true)` -> Post's result.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ui_post(
    rec: *const EeScreensUserInterfaceFields,
    message: u32,
    p1: *mut c_void,
    p2: *mut c_void,
) -> c_long {
    post(&*rec, message, p1, p2)
}

/// `EEUserInterface::PostStartLoading` (0x2792c3): 0x407, 0x413 from the scenario editor.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ui_post_start_loading(rec: *const EeScreensUserInterfaceFields) {
    let message = ui_objects::start_loading_message(ui_objects::in_scenario_editor());
    post(&*rec, message, core::ptr::null_mut(), core::ptr::null_mut());
}

/// `EEUserInterface::PostMessage424Once` (0x279756): the icon bar's update, once until it is handled (nothing on a
/// NULL object, as the original's `this != NULL` test).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ui_post_icon_bar_once(rec: *mut EeScreensUserInterfaceFields) {
    if !rec.is_null() && ui_objects::icon_bar_once(&mut *rec) {
        // (the original posted first, then set the guard: one UI thread object, posted from the physics only)
        post(&*rec, ui_objects::EE_SCREENS_UI_POST_ICON_BAR, core::ptr::null_mut(), core::ptr::null_mut());
    }
}

/// `EEUserInterface::SetLoadProgress(fraction)` (0x27965e): the percentage to the UI thread (0x40a); in a game of
/// several humans also `EEMLoadPercent(mLocalPlayer, percent)` at every new 20 percent step.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ui_set_load_progress(rec: *const EeScreensUserInterfaceFields, fraction: f32) {
    /// cbindgen:ignore
    mod sys {
        extern "C" {
            pub fn ee_screenscpp_ui_humans() -> i64;
        }
    }
    const MSG_LOAD_PERCENT: u8 = 0x94;
    let (percent, step) = ui_objects::load_progress(fraction);
    if step && sys::ee_screenscpp_ui_humans() > 1 {
        use crate::screens::hud_sends::{long, send, Msg};
        send(&Msg { kind: MSG_LOAD_PERCENT, values: vec![long((*rec).mLocalPlayer as i64), long(percent as i64)] });
    }
    post(&*rec, ui_objects::EE_SCREENS_UI_POST_LOAD_PERCENT, percent as usize as *mut c_void, core::ptr::null_mut());
}

/// `EEUIManager`'s constructor's stores (`EE_SCREENS_UM_OP_FORMS`), on the registered record.
pub(crate) fn manager_forms() {
    if let Some(m) = ui_objects::manager() {
        let main_game = direct::form_by_id(direct::manager_of(core::ptr::null_mut()), 11);
        // SAFETY: the live manager's record; no call out while the reference lives
        ui_objects::set_forms(unsafe { &mut *m }, main_game);
    }
}

/// The UI thread loop's ops (`ee_screenscpp_ui_game`, `EE_SCREENS_UG_OP_*`) on the UI object, its thread's queue
/// and the manager's record; None for the C++'s.
pub(crate) fn ui_game(ui: *mut c_void, op: u32, a: i64, b: i64, c: i64) -> Option<i64> {
    use crate::screens::ui_game::*;
    let rec = ui as *mut EeScreensUserInterfaceFields;
    Some(match op {
        // SAFETY (each arm): the live UI object's record, read or written in place
        EE_SCREENS_UG_OP_QUITTING => unsafe { (*rec).mIsQuitting as i64 },
        EE_SCREENS_UG_OP_UPDATING => unsafe { ((*rec).mIsUpdating != 0) as i64 },
        EE_SCREENS_UG_OP_QUEUE_LOCK => {
            // SAFETY: the live thread's message lock
            unsafe {
                let lock = ee_base::ts::ee_ts_thread_message_lock(thread(&*rec));
                if a != 0 {
                    ee_base::ts::ee_ts_section_enter(lock)
                } else {
                    ee_base::ts::ee_ts_section_leave(lock)
                }
            }
            0
        }
        EE_SCREENS_UG_OP_RECEIVE => {
            let (mut message, mut p1, mut p2): (c_ulong, *mut c_void, *mut c_void) = (0, core::ptr::null_mut(), core::ptr::null_mut());
            // SAFETY: the live thread; `c` the loop's three words
            unsafe {
                let result = ee_base::ts::ee_ts_thread_receive(thread(&*rec), &mut message, &mut p1, &mut p2);
                let out = c as *mut i64;
                *out = message as i64;
                *out.add(1) = p1 as i64;
                *out.add(2) = p2 as i64;
                result as i64
            }
        }
        EE_SCREENS_UG_OP_HANDLE => {
            crate::screens::ui_thread_ffi::ee_screens_ui_message(ui, a as u32, b, c);
            0
        }
        EE_SCREENS_UG_OP_TIME => ee_base::time::ee_time_get() as i64,
        EE_SCREENS_UG_OP_SLEEP => {
            ee_base::plat_core::time::PlatSleep(a as u32);
            0
        }
        EE_SCREENS_UG_OP_THREAD_NAME => 0,
        EE_SCREENS_UG_OP_ACTIONS_CLEAR => {
            crate::screens::actions_ffi::ee_screens_actions_clear(a as u32);
            0
        }
        EE_SCREENS_UG_OP_B13D_OFF => {
            if let Some(m) = ui_objects::manager() {
                // SAFETY: the live manager's record
                unsafe { (*m).mB13D = false };
            }
            0
        }
        EE_SCREENS_UG_OP_NOT_MAIN => {
            let m = direct::manager_of(core::ptr::null_mut());
            (direct::manager_call(m, EeUiManagerCall::CurrentForm, 0, 0, 0) as *mut c_void != ui_objects::main_game_form())
                as i64
        }
        EE_SCREENS_UG_OP_SWAP_MAIN | EE_SCREENS_UG_OP_SWAP_EDITOR => {
            let form = if op == EE_SCREENS_UG_OP_SWAP_MAIN { ui_objects::main_game_form() } else { ui_objects::editor_form() };
            direct::manager_call(direct::manager_of(core::ptr::null_mut()), EeUiManagerCall::SwapForm, form as isize, 0, 0);
            0
        }
        _ => return None,
    })
}

/// The UI thread messages' ops (`ee_screenscpp_ui_message`, `EE_SCREENS_UI_OP_*`) on the UI object, the records
/// and the UI thread's own Rust; None for the C++'s.
pub(crate) fn ui_message(ui: *mut c_void, op: u32, a: i64, b: i64, c: i64) -> Option<i64> {
    use crate::screens::ui_game::*;
    use crate::screens::ui_game_ffi::ee_screens_ui_member;
    use crate::screens::ui_thread::*;
    let rec = ui as *mut EeScreensUserInterfaceFields;
    Some(match op {
        EE_SCREENS_UI_OP_MAIN_FORM => ui_objects::main_game_form() as i64,
        EE_SCREENS_UI_OP_EDITOR_FORM => ui_objects::editor_form() as i64,
        // SAFETY (the two): the live UI object's record
        EE_SCREENS_UI_OP_SET_UPDATING => {
            unsafe { (*rec).mIsUpdating = a as u8 };
            0
        }
        EE_SCREENS_UI_OP_SET_QUITTING => {
            unsafe { (*rec).mIsQuitting = true };
            0
        }
        EE_SCREENS_UI_OP_SLEEP => {
            ee_base::plat_core::time::PlatSleep(1);
            0
        }
        EE_SCREENS_UI_OP_QUEUE_DONE => {
            // SAFETY: the object's live event
            unsafe { ee_base::ts::ee_ts_event_set((*rec).mMessageQueueDone as *const EeTsEvent) };
            0
        }
        EE_SCREENS_UI_OP_REFRESH_GROUP => {
            if let Some(r) = crate::screens::main_form_fields::record(ui_objects::main_game_form()) {
                // SAFETY: the main game form's live record
                unsafe { (*r).mRefreshGroup = true };
            }
            0
        }
        EE_SCREENS_UI_OP_LOAD_SAVED_GAME => {
            crate::screens::ui_game_ffi::ee_screens_ui_load_saved_game(ui, a as *mut c_void);
            0
        }
        EE_SCREENS_UI_OP_SAVE_AND_TEST => {
            crate::screens::ui_game_ffi::ee_screens_ui_save_and_test_scenario(ui);
            0
        }
        EE_SCREENS_UI_OP_TEST_SCENARIO => {
            crate::screens::ui_game_ffi::ee_screens_ui_test_scenario(ui);
            0
        }
        EE_SCREENS_UI_OP_CLOSE_EDITOR => {
            ee_screens_ui_member(ui, EE_SCREENS_UG_DO_CLOSE_EDITOR, 0, 0, 0);
            0
        }
        EE_SCREENS_UI_OP_ENTER_EDITOR => {
            ee_screens_ui_member(ui, EE_SCREENS_UG_DO_ENTER_EDITOR, (a != 0) as i64, 0, 0);
            0
        }
        EE_SCREENS_UI_OP_CANCEL_GAME => {
            ee_screens_ui_member(ui, EE_SCREENS_UG_DO_CANCEL_GAME, 0, 0, 0);
            0
        }
        EE_SCREENS_UI_OP_LOAD_FROM_EDITOR => {
            ee_screens_ui_member(ui, EE_SCREENS_UG_DO_LOAD_FROM_EDITOR, 0, 0, 0);
            0
        }
        EE_SCREENS_UI_OP_RESTART => {
            ee_screens_ui_member(ui, EE_SCREENS_UG_DO_RESTART, 0, 0, 0);
            0
        }
        EE_SCREENS_UI_OP_CIV_BUILDER => {
            // (STAND-IN, owner 2026-11-01: the flag is the low byte of the raw dword 0x278b46 pushes)
            let shown = c != 0;
            let (x, y) = if shown { (0, 0) } else { (a, b as u8 as i64) };
            ee_screens_ui_member(ui, EE_SCREENS_UG_DO_CIV_BUILDER, x, y, shown as i64);
            0
        }
        EE_SCREENS_UI_OP_LAG => {
            crate::screens::ui_game_ffi::ee_screens_ui_show_lag_status(a as u32, b != 0);
            0
        }
        EE_SCREENS_UI_OP_LOBBY => {
            use crate::screens::lobby_form_ffi as lobby;
            match a {
                0 => lobby::ee_screens_lobby_update_player(b as _, c as _),
                1 => lobby::ee_screens_lobby_update_game_options(),
                2 => lobby::ee_screens_lobby_show_transfer_progress(b as u8, c as u8),
                _ => lobby::ee_screens_lobby_update_form(),
            }
            0
        }
        _ => return None,
    })
}

/// The window events' ops (`ee_screenscpp_ui_event`, `EE_SCREENS_UI_EV_*`) on the UI object and the manager;
/// None for the C++'s.
pub(crate) fn ui_event(op: u32, a: i64) -> Option<i64> {
    use crate::screens::ui_thread::*;
    let m = direct::manager_of(core::ptr::null_mut());
    Some(match op {
        EE_SCREENS_UI_EV_HAS if a == EE_SCREENS_UI_GLOBAL_USER_INTERFACE => ui_objects::ui().is_some() as i64,
        EE_SCREENS_UI_EV_HAS if a == EE_SCREENS_UI_GLOBAL_MANAGER => (!m.is_null()) as i64,
        // SAFETY (the two): the live UI object's record
        EE_SCREENS_UI_EV_QUITTING => ui_objects::ui().is_some_and(|u| unsafe { (*u).mIsQuitting }) as i64,
        EE_SCREENS_UI_EV_POST_QUIT => {
            // WM_QUIT, not queued
            if let Some(u) = ui_objects::ui() {
                unsafe { ee_base::ts::ee_ts_thread_post(thread(&*u), 0x12, core::ptr::null_mut(), core::ptr::null_mut(), false) };
            }
            0
        }
        EE_SCREENS_UI_EV_SLEEP => {
            ee_base::plat_core::time::PlatSleep(1);
            0
        }
        EE_SCREENS_UI_EV_IME_OPEN | EE_SCREENS_UI_EV_IME_CLOSE => {
            let call = if op == EE_SCREENS_UI_EV_IME_OPEN { EeUiManagerCall::OpenComposition } else { EeUiManagerCall::CloseComposition };
            direct::manager_call(m, call, 0, 0, 0);
            0
        }
        _ => return None,
    })
}

/// `gUserInterface`'s post of `message` with no parameters, when there is the UI object.
pub(crate) fn post_if_ui(message: u32) {
    ui_post(message, core::ptr::null_mut(), core::ptr::null_mut());
}

/// `gUserInterface`'s post of `message` -> Post's result (0 without the UI object).
fn ui_post(message: u32, p1: *mut c_void, p2: *mut c_void) -> c_long {
    // SAFETY: the registered live UI object's record
    ui_objects::ui().map_or(0, |u| post(unsafe { &*u }, message, p1, p2))
}

/// The screens' ops (`EEScreens.cpp`'s `ee_screenscpp_call`) that post to the UI thread or read and write the two
/// records; None for the C++'s.
pub(crate) fn screens(op: u32, a: u32, b: u32) -> Option<u32> {
    use crate::screens::{chat::*, main_game::*, main_game_options::*, menus::*, save_load::*, scenario_screens::*};
    use crate::screens::{settings_form::*, splash::*};
    let null = core::ptr::null_mut();
    let m = direct::manager_of(null);
    let by_id = |id: u32| direct::form_by_id(m, id as c_ulong);
    Some(match op {
        EE_SCREENS_OP_SWITCH => {
            ui_post(0x414, by_id(a), null);
            0
        }
        EE_SCREENS_OP_OPEN_FORM => ui_post(0x40d, by_id(a), null) as u32,
        EE_SCREENS_OP_CLOSE_FORM => ui_post(0x419, null, null) as u32,
        EE_SCREENS_GAME_CH_CLOSE => ui_post(0x419, ui_objects::main_game_form(), null) as u32,
        EE_SCREENS_OP_OPEN_MAIN_GAME_FORM => {
            ui_post(0x40d, ui_objects::main_game_form(), null);
            0
        }
        EE_SCREENS_OP_DISPLAY_CHANGED => {
            ui_post(0x429, null, null);
            0
        }
        EE_SCREENS_GAME_CHANGE_RASTERIZER => {
            ui_post(0x41e, a as c_long as isize as *mut c_void, b as c_long as isize as *mut c_void);
            0
        }
        EE_SCREENS_GAME_QUIT => {
            ui_post(0x401, null, null);
            0
        }
        EE_SCREENS_GAME_GO_RESTART => {
            ui_post(0x428, null, null);
            0
        }
        EE_SCREENS_GAME_MG_CIV_BUILDER => ui_post(0x41f, null, null) as u32,
        // (the main game form's empty FnE123D)
        EE_SCREENS_GAME_MG_REFRESH_FORM => 0,
        EE_SCREENS_OP_SLEEP => {
            ee_base::plat_core::time::PlatSleep(a);
            0
        }
        EE_SCREENS_GAME_EDITOR_MODE_OFF => {
            if let Some(r) = ui_objects::manager() {
                // SAFETY: the live manager's record
                unsafe { (*r).mInScenarioEditor = false };
            }
            0
        }
        EE_SCREENS_GAME_IN_EDITOR => ui_objects::in_scenario_editor() as u32,
        EE_SCREENS_GAME_SS_LOCAL => {
            if let Some(u) = ui_objects::ui() {
                // SAFETY: the live UI object's record
                unsafe { (*u).mLocalPlayer = a as c_long };
            }
            0
        }
        _ => return None,
    })
}
