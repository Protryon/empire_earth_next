//! Rust panics abort the process (`panic = "abort"`), and a GUI exe has no stderr: the hook hands the message to the
//! game's log first (PORT_LEARNINGS "wgpu errors are Rust panics").

use std::ffi::{c_char, CString};
use std::panic::{self, Location};

/// Receives one NUL-terminated line, valid during the call. May be called from any thread.
pub type LogFn = unsafe extern "C" fn(line: *const c_char);

/// The log line for a panic. Interior NULs are dropped, because the line crosses a C string boundary.
pub fn format_panic(message: &str, location: Option<&Location<'_>>) -> CString {
    let line = match location {
        Some(at) => format!("EE-RUST panic at {}:{}: {}", at.file(), at.line(), message),
        None => format!("EE-RUST panic: {message}"),
    };
    CString::new(line.replace('\0', "")).expect("NULs were removed")
}

/// The text of a panic payload: `panic!` gives a `&str` or a `String`.
pub fn payload_text(payload: &(dyn std::any::Any + Send)) -> &str {
    if let Some(text) = payload.downcast_ref::<&str>() {
        text
    } else if let Some(text) = payload.downcast_ref::<String>() {
        text
    } else {
        "(a panic payload that is not text)"
    }
}

/// Replaces the panic hook: `log` is called, then the default hook runs (stderr, when there is one).
pub fn install(log: LogFn) {
    let previous = panic::take_hook();
    panic::set_hook(Box::new(move |info| {
        let line = format_panic(payload_text(info.payload()), info.location());
        // SAFETY: `log` is the caller's contract (LogFn) and `line` outlives the call.
        unsafe { log(line.as_ptr()) };
        previous(info);
    }));
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::any::Any;
    use std::ffi::CStr;
    use std::sync::Mutex;

    #[test]
    fn line_names_the_location_when_there_is_one() {
        let at = Location::caller();
        let text = format_panic("boom", Some(at)).into_string().unwrap();
        assert!(text.starts_with("EE-RUST panic at "), "{text}");
        assert!(text.contains(at.file()), "{text}");
        assert!(text.ends_with(": boom"), "{text}");
    }

    #[test]
    fn line_without_a_location() {
        assert_eq!(format_panic("boom", None).to_bytes(), b"EE-RUST panic: boom");
    }

    #[test]
    fn interior_nuls_do_not_lose_the_line() {
        assert_eq!(format_panic("a\0b", None).to_bytes(), b"EE-RUST panic: ab");
    }

    #[test]
    fn payload_kinds() {
        let s: Box<dyn Any + Send> = Box::new("static");
        let owned: Box<dyn Any + Send> = Box::new(String::from("owned"));
        let other: Box<dyn Any + Send> = Box::new(7_u32);
        assert_eq!(payload_text(s.as_ref()), "static");
        assert_eq!(payload_text(owned.as_ref()), "owned");
        assert_eq!(payload_text(other.as_ref()), "(a panic payload that is not text)");
    }

    static LINES: Mutex<Vec<String>> = Mutex::new(Vec::new());

    unsafe extern "C" fn record(line: *const c_char) {
        // SAFETY: LogFn's contract.
        let text = unsafe { CStr::from_ptr(line) }.to_string_lossy().into_owned();
        LINES.lock().unwrap().push(text);
    }

    #[test]
    fn the_hook_logs_a_real_panic() {
        install(record);
        let result = panic::catch_unwind(|| panic!("hook test {}", 42));
        let _ = panic::take_hook();
        assert!(result.is_err());
        let lines = LINES.lock().unwrap();
        assert!(lines.iter().any(|l| l.contains("hook test 42")), "{lines:?}");
    }
}
