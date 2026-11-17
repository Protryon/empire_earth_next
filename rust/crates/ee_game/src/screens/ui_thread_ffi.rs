//! `extern "C"` for the UI thread's message switch and the game window's events (`ui_thread.rs`). The hosts are
//! `ee_screenscpp_ui_message` and `ee_screenscpp_ui_event` (`User Interface Thread/EEUserInterface.cpp`).

use core::ffi::{c_char, c_void, CStr};

use crate::screens::ui_thread::{handle_event, handle_message, plat_utf8_length, EventState, UiHost, WindowEvent};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_ui_message(ui: *mut c_void, op: u32, a: i64, b: i64, c: i64) -> i64;
        pub fn ee_screenscpp_ui_event(op: u32, a: i64, b: i64, c: i64) -> i64;
    }
}

struct MessageHost(*mut c_void);

impl UiHost for MessageHost {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        unsafe { sys::ee_screenscpp_ui_message(self.0, op, a, b, c) }
    }
}

struct EventHost;

impl UiHost for EventHost {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        unsafe { sys::ee_screenscpp_ui_event(op, a, b, c) }
    }
}

/// `EEUserInterface::HandleMessage`: the thread message `message` with its two parameters, on the UI object `ui`.
#[no_mangle]
pub extern "C" fn ee_screens_ui_message(ui: *mut c_void, message: u32, p1: i64, p2: i64) {
    handle_message(&mut MessageHost(ui), message, p1, p2);
}

static SCREENS_UI_EVENT_STATE: EventState = EventState::new();

/// `EEUserInterface::HandleEvent`: the window event of type `kind` (its key's `virtual_key`, its zero-terminated
/// `text`, the composition's `cursor` and `length`).
///
/// # Safety
/// `text` is NULL or zero-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ui_event(
    kind: u32,
    virtual_key: u32,
    text: *const c_char,
    cursor: i32,
    length: i32,
) {
    let bytes = if text.is_null() { &[][..] } else { unsafe { CStr::from_ptr(text) }.to_bytes() };
    let ev = WindowEvent { kind, virtual_key, text_address: text as i64, text: bytes, cursor, length };
    handle_event(&mut EventHost, &SCREENS_UI_EVENT_STATE, &ev, plat_utf8_length);
}
