//! The game window's event pump and the Win32 messages that are not window events (were Empire Earth.cpp's
//! EEPumpWindowEvent and EENativeMessageHook; game core lane, round 12, fork D).
//!
//! The pump (`TSThread::SetEventPump`, set by EERun: [`super::entry`]) takes one window event: the WON lobby's
//! wake-up (PlatWindowWake while the lobby does not run) only wakes; every other event goes to the Input System's
//! keyboard and mouse (`ISHandleEvent`: ee_base input) and to the user interface's window handler
//! (`EEUserInterface::HandleEvent`: ee_screens_ui_event). `EE_WINDOW_EVENT_LOG` (opt-in) writes each event as a debug
//! string. The hook (Windows) handles the WON lobby's two messages as the original's window procedure did.

#[cfg(not(test))]
use std::ffi::{c_int, c_void};
#[cfg(not(test))]
use std::sync::OnceLock;

#[cfg(not(test))]
use ee_base::plat_ui::event::PLAT_EVENT_WAKE;
use ee_base::plat_ui::event::{PlatInputEvent, PLAT_EVENT_TEXT};

#[cfg(not(test))]
use super::{h, pa, O, P};

/// cbindgen:ignore
#[cfg(not(test))]
mod sys {
    use std::ffi::{c_char, c_int, c_void};

    // ee_base (platform/PlatWindow.h, ISHandleEvent's body) and ee_game screens (ui_thread_ffi.rs)
    extern "C" {
        pub fn PlatWindowPollEvent(out: *mut ee_base::plat_ui::event::PlatInputEvent) -> c_int;
        pub fn PlatWindowSetNativeMessageHook(hook: Option<unsafe extern "C" fn(*mut c_void, u32, u64, i64) -> c_int>);
        pub fn ee_input_handle_event(kind: u32, scan_code: u32, dx: i32, dy: i32, button: u32, wheel: f32);
        pub fn ee_screens_ui_event(kind: u32, virtual_key: u32, text: *const c_char, cursor: i32, length: i32);
    }
}

/// The text of a text event (its NUL-ended bytes), empty for the others: what the log line shows.
fn event_text(event: &PlatInputEvent) -> String {
    if event.event_type != PLAT_EVENT_TEXT {
        return String::new();
    }
    // SAFETY: a text event's data is the text
    let bytes = unsafe { &event.data.text.text };
    let n = bytes.iter().position(|b| *b == 0).unwrap_or(bytes.len());
    String::from_utf8_lossy(&bytes[..n]).into_owned()
}

/// The log line of an event (the C++'s `"EE-WINDOW-EVENT %u (key %u/%u, text '%s') ui %d window %lux%lu\n"`).
pub fn log_line(event: &PlatInputEvent, ui: bool, width: u64, height: u64) -> String {
    // SAFETY: the key fields are read whatever the event (as the C++ read the union)
    let key = unsafe { event.data.key };
    format!(
        "EE-WINDOW-EVENT {} (key {}/{}, text '{}') ui {} window {}x{}\n",
        event.event_type,
        key.scan_code,
        key.virtual_key,
        event_text(event),
        i32::from(ui),
        width,
        height
    )
}

/// The pump: one window event handled (true), or none waiting (false).
///
/// # Safety
/// Called by the thread scheduler on the window's thread.
#[cfg(not(test))]
pub unsafe extern "C" fn pump() -> bool {
    let mut event = PlatInputEvent::zeroed();
    // SAFETY: a zeroed event to fill
    if unsafe { sys::PlatWindowPollEvent(&mut event) } == 0 {
        return false;
    }
    if event.event_type == PLAT_EVENT_WAKE {
        return true;
    }
    static LOG: OnceLock<bool> = OnceLock::new();
    if *LOG.get_or_init(|| std::env::var_os("EE_WINDOW_EVENT_LOG").is_some()) {
        // SAFETY: the user interface's pointer (gamecpp globals.rs)
        let ui = unsafe { !(*std::ptr::addr_of!(super::globals::gUserInterface)).is_null() };
        let width = h(O::C12GameWindowSize, 0, &[0]) as u64;
        let height = h(O::C12GameWindowSize, 0, &[1]) as u64;
        let line = std::ffi::CString::new(log_line(&event, ui, width, height).replace('\0', "")).unwrap_or_default();
        h(O::C12DebugText, 0, &[pa(line.as_ptr() as P)]);
    }
    // SAFETY: the event's fields as ISHandleEvent and EEUserInterface::HandleEvent read them
    unsafe {
        let (key, mouse, text) = (event.data.key, event.data.mouse, event.data.text);
        sys::ee_input_handle_event(
            event.event_type,
            key.scan_code,
            mouse.delta_x,
            mouse.delta_y,
            mouse.button,
            mouse.wheel,
        );
        sys::ee_screens_ui_event(
            event.event_type,
            key.virtual_key,
            event.data.text.text.as_ptr().cast(),
            text.cursor,
            text.length,
        );
    }
    true
}

/// The WON lobby's messages: 0x433 posts 0x434 back to the window and runs the lobby's FnCFFDD(0); 0x434 runs its
/// FnCFFF2; nonzero drops the message.
#[cfg(all(windows, not(test)))]
unsafe extern "C" fn native_hook(window: *mut c_void, message: u32, _: u64, _: i64) -> c_int {
    // SAFETY: the WON front end's pointer (gamecpp globals.rs)
    let won = unsafe { *std::ptr::addr_of!(super::globals::gWonNet) } as P;
    match message {
        0x433 => {
            // SAFETY: the game window's handle
            unsafe { windows_sys_059::Win32::UI::WindowsAndMessaging::PostMessageA(window, 0x434, 0, 0) };
            if won != 0 {
                h(O::C12WonNetCffdd, won, &[]);
            }
            1
        }
        0x434 => {
            if won != 0 {
                h(O::C12WonNetCfff2, won, &[]);
            }
            1
        }
        _ => 0,
    }
}

/// EERun's set-up: the pump on this (the window's) thread, and on Windows the native message hook.
#[cfg(not(test))]
pub fn install() {
    // SAFETY: the platform's id of this thread
    ee_base::ts::ee_ts_set_event_pump(Some(pump), unsafe { ee_base::plat_sys::thread::imp::PlatCurrentThreadId() });
    #[cfg(windows)]
    // SAFETY: the platform's hook slot
    unsafe {
        sys::PlatWindowSetNativeMessageHook(Some(native_hook))
    };
}

/// The tests' set-up: no window.
#[cfg(test)]
pub fn install() {}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_log_line_is_the_cs_format() {
        let mut e = PlatInputEvent::zeroed();
        e.event_type = 3;
        e.data.key = ee_base::plat_ui::event::PlatKeyData { scan_code: 30, virtual_key: 65, modifiers: 0, repeat: 0 };
        assert_eq!(log_line(&e, true, 1024, 768), "EE-WINDOW-EVENT 3 (key 30/65, text '') ui 1 window 1024x768\n");
        let mut t = PlatInputEvent::zeroed();
        t.event_type = PLAT_EVENT_TEXT;
        let mut text = [0u8; 64];
        text[..2].copy_from_slice(b"hi");
        t.data.text = ee_base::plat_ui::event::PlatTextData { text, cursor: 0, length: 2 };
        assert!(log_line(&t, false, 0, 0).contains("text 'hi') ui 0 window 0x0"));
    }
}
