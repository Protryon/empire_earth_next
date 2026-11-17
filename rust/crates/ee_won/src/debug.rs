//! WONDebug (net round 27): the stack dump file's name and the crash dump the WON threads' exception filter writes
//! (WONCommon/WONDebug.cpp, deleted). The original appended a timestamped trace of the faulting thread, through
//! DbgHelp's `StackWalk64` over the exception's context, to the file `SetStackFileName` named, and returned
//! `EXCEPTION_CONTINUE_SEARCH`; with no name (the game never set one) it wrote nothing. Here the trace is Rust's
//! backtrace of the calling thread: the filter runs on the faulting thread with the faulting frames still on its
//! stack, so they are in it, printed as Rust prints a backtrace rather than DbgHelp's `address: module!symbol` lines.
//! The begin and end lines and the time are the original's (`%c` as the Windows CRT's C locale prints it).

use std::ffi::c_void;
use std::io::Write;
use std::sync::Mutex;

use crate::ffi::{bytes, give, EeWonByteSink};

/// `gStackFileName`.
static STACK_FILE: Mutex<Vec<u8>> = Mutex::new(Vec::new());

/// `EXCEPTION_CONTINUE_SEARCH`
pub const EE_WON_DEBUG_CONTINUE_SEARCH: i32 = 0;

fn stack_file() -> std::sync::MutexGuard<'static, Vec<u8>> {
    STACK_FILE.lock().unwrap_or_else(|e| e.into_inner())
}

/// `strftime("%c")` in the Windows CRT's C locale: `11/15/26 14:03:05`.
pub fn time_text(t: &ee_base::plat_core::time::PlatDateTime) -> String {
    format!("{:02}/{:02}/{:02} {:02}:{:02}:{:02}", t.month, t.day, t.year % 100, t.hour, t.minute, t.second)
}

/// The text `MyDumpStack` appends for a trace taken at `time`.
pub fn dump_text(time: &str, trace: &str) -> String {
    let mut s = format!("\n\n*** Begin WONDebug StackTrace {time} ***\n");
    s.push_str(trace);
    if !trace.ends_with('\n') {
        s.push('\n');
    }
    s.push_str(&format!("*** End WONDebug StackTrace {time} ***\n"));
    s
}

/// `DumpStackToFile`: the trace appended to the stack file (nothing without a name or when it does not open), then
/// `EXCEPTION_CONTINUE_SEARCH`.
pub fn dump_stack() -> i32 {
    let name = stack_file().clone();
    if name.is_empty() {
        return EE_WON_DEBUG_CONTINUE_SEARCH;
    }
    let Ok(path) = std::str::from_utf8(&name) else {
        return EE_WON_DEBUG_CONTINUE_SEARCH;
    };
    let Ok(mut file) = std::fs::OpenOptions::new().append(true).create(true).open(path) else {
        return EE_WON_DEBUG_CONTINUE_SEARCH;
    };
    let mut now = ee_base::plat_core::time::PlatDateTime::default();
    // SAFETY: a writable value
    unsafe { ee_base::plat_core::time::PlatGetLocalTime(&mut now) };
    let trace = std::backtrace::Backtrace::force_capture().to_string();
    let _ = file.write_all(dump_text(&time_text(&now), &trace).as_bytes());
    EE_WON_DEBUG_CONTINUE_SEARCH
}

// ---- the C ABI (WONDebug's static members in WONRouting.h; AsyncOp.cpp's thread filter)

/// `WONDebug::SetStackFileName`.
///
/// # Safety
/// `name` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_debug_set_stack_file(name: *const u8, length: usize) {
    // SAFETY: the caller's bytes
    *stack_file() = unsafe { bytes(name, length) }.to_vec();
}

/// `WONDebug::GetStackFileName`: the name to the sink.
///
/// # Safety
/// `sink` receives the name during the call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_debug_stack_file(sink: EeWonByteSink, context: *mut c_void) {
    let name = stack_file().clone();
    // SAFETY: the sink's contract
    unsafe { give(sink, context, &name) }
}

/// `WONDebug::DumpStackToFile`: an exception filter's value.
#[no_mangle]
pub extern "C" fn ee_won_debug_dump_stack() -> i32 {
    dump_stack()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn time_is_the_c_locale_form() {
        let t = ee_base::plat_core::time::PlatDateTime {
            year: 2026,
            month: 11,
            day_of_week: 0,
            day: 5,
            hour: 9,
            minute: 3,
            second: 7,
            milliseconds: 0,
        };
        assert_eq!(time_text(&t), "11/05/26 09:03:07");
    }

    #[test]
    fn dump_text_brackets_the_trace() {
        assert_eq!(
            dump_text("T", "a\nb"),
            "\n\n*** Begin WONDebug StackTrace T ***\na\nb\n*** End WONDebug StackTrace T ***\n"
        );
        assert_eq!(
            dump_text("T", "x\n"),
            "\n\n*** Begin WONDebug StackTrace T ***\nx\n*** End WONDebug StackTrace T ***\n"
        );
    }

    #[test]
    fn no_name_writes_nothing_then_a_name_appends() {
        // one test owns the global name
        assert!(stack_file().is_empty());
        assert_eq!(dump_stack(), EE_WON_DEBUG_CONTINUE_SEARCH);
        let d = std::env::temp_dir().join(format!("ee_won_debug_{}.txt", std::process::id()));
        let _ = std::fs::remove_file(&d);
        let p = d.to_str().unwrap().as_bytes().to_vec();
        // SAFETY: a valid slice
        unsafe { ee_won_debug_set_stack_file(p.as_ptr(), p.len()) };
        let mut got = Vec::new();
        unsafe extern "C" fn sink(c: *mut c_void, b: *const u8, n: usize) {
            // SAFETY: the test's vector and the callee's bytes
            unsafe { (*(c as *mut Vec<u8>)).extend_from_slice(std::slice::from_raw_parts(b, n)) };
        }
        // SAFETY: the sink takes the vector
        unsafe { ee_won_debug_stack_file(sink, &mut got as *mut Vec<u8> as *mut c_void) };
        assert_eq!(got, p);
        assert_eq!(ee_won_debug_dump_stack(), 0);
        assert_eq!(ee_won_debug_dump_stack(), 0);
        let text = std::fs::read_to_string(&d).unwrap();
        assert_eq!(text.matches("*** Begin WONDebug StackTrace ").count(), 2);
        assert_eq!(text.matches("*** End WONDebug StackTrace ").count(), 2);
        assert!(text.starts_with("\n\n*** Begin"));
        // SAFETY: an empty name
        unsafe { ee_won_debug_set_stack_file(p.as_ptr(), 0) };
        let _ = std::fs::remove_file(&d);
    }
}
