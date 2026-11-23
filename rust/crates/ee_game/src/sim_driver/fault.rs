//! TEST ONLY: the fault report of EE-SIM and of the game under the UI fixture (was EESim.cpp's EESimFault and
//! EETestFixture.cpp's FixtureFault): a fault prints its code, place and return addresses (exe addresses: the
//! exe's .map) on stderr, once, and goes on to the next handler. Windows only (a vectored exception handler); the
//! POSIX signal handler stays EESim.cpp's.

use std::fmt::Write as _;

/// A text in a fixed buffer (the handler allocates nothing: the heap may be what faulted).
struct Line {
    bytes: [u8; 2048],
    len: usize,
}

impl std::fmt::Write for Line {
    fn write_str(&mut self, s: &str) -> std::fmt::Result {
        let n = s.len().min(self.bytes.len() - self.len);
        self.bytes[self.len..self.len + n].copy_from_slice(&s.as_bytes()[..n]);
        self.len += n;
        Ok(())
    }
}

/// Whether the exception code is a fault worth a report (not a thread name, debug string or C++ exception).
pub fn is_fault(code: u32) -> bool {
    code >= 0xC000_0000 && code != 0xE06D_7363
}

/// The report's line, as the C++ wrote it with `%08lx`, `%p` (16 upper-case hex digits) and `%lu`, the frames while
/// the line has room for one more (the C++'s 2048-byte buffer less 24), then a new line.
fn report(line: &mut Line, prefix: &str, code: u32, at: usize, thread: u32, frames: &[usize]) {
    let _ = write!(line, "{prefix} FAULT {code:08x} at {at:016X} thread {thread}; frames:");
    for f in frames {
        if line.len >= 2048 - 24 {
            break;
        }
        let _ = write!(line, " {f:016X}");
    }
    let _ = line.write_str("\n");
}

#[cfg(windows)]
mod win {
    use std::sync::atomic::{AtomicBool, AtomicPtr, Ordering};

    use windows_sys_059::Win32::System::Diagnostics::Debug::{
        AddVectoredExceptionHandler, RtlCaptureStackBackTrace, EXCEPTION_POINTERS,
    };
    use windows_sys_059::Win32::System::Threading::GetCurrentThreadId;

    use super::Line;

    static ONCE: AtomicBool = AtomicBool::new(false);
    static PREFIX: AtomicPtr<u8> = AtomicPtr::new(std::ptr::null_mut());
    static PREFIX_LEN: std::sync::atomic::AtomicUsize = std::sync::atomic::AtomicUsize::new(0);

    extern "C" {
        fn _write(fd: i32, buffer: *const std::ffi::c_void, count: u32) -> i32;
    }

    const EXCEPTION_CONTINUE_SEARCH: i32 = 0;

    unsafe extern "system" fn handler(info: *mut EXCEPTION_POINTERS) -> i32 {
        // SAFETY: the system passes a live record
        let record = unsafe { &*(*info).ExceptionRecord };
        let code = record.ExceptionCode as u32;
        if !super::is_fault(code) || ONCE.swap(true, Ordering::SeqCst) {
            return EXCEPTION_CONTINUE_SEARCH;
        }
        let mut frames = [std::ptr::null_mut(); 48];
        // SAFETY: a 48-entry buffer
        let n = unsafe { RtlCaptureStackBackTrace(0, 48, frames.as_mut_ptr(), std::ptr::null_mut()) } as usize;
        let mut addresses = [0usize; 48];
        for (a, f) in addresses.iter_mut().zip(&frames[..n]) {
            *a = *f as usize;
        }
        // SAFETY: the prefix was leaked by install and is never freed
        let prefix = unsafe {
            std::str::from_utf8_unchecked(std::slice::from_raw_parts(
                PREFIX.load(Ordering::SeqCst),
                PREFIX_LEN.load(Ordering::SeqCst),
            ))
        };
        let mut line = Line { bytes: [0; 2048], len: 0 };
        // SAFETY: plain reads of the thread id
        let thread = unsafe { GetCurrentThreadId() };
        super::report(&mut line, prefix, code, record.ExceptionAddress as usize, thread, &addresses[..n]);
        // SAFETY: the C runtime's descriptor 2 (where the drivers point stderr), the line's bytes
        unsafe { _write(2, line.bytes.as_ptr().cast(), line.len as u32) };
        EXCEPTION_CONTINUE_SEARCH
    }

    pub fn install(prefix: &str) {
        let leaked: &'static str = Box::leak(prefix.to_string().into_boxed_str());
        PREFIX_LEN.store(leaked.len(), Ordering::SeqCst);
        PREFIX.store(leaked.as_ptr() as *mut u8, Ordering::SeqCst);
        // SAFETY: a handler that lives as long as the process
        unsafe { AddVectoredExceptionHandler(1, Some(handler)) };
    }
}

/// Installs the fault report (first in the handler chain) with the exe's name as its prefix (`EE-SIM`, `EE-AOC`).
///
/// # Safety
/// `prefix` is a NUL-ended text.
#[no_mangle]
pub unsafe extern "C" fn ee_sim_driver_fault_install(prefix: *const std::ffi::c_char) {
    // SAFETY: as the caller promises
    let _prefix = unsafe { std::ffi::CStr::from_ptr(prefix) }.to_string_lossy().into_owned();
    #[cfg(windows)]
    win::install(&_prefix);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn faults_are_the_high_codes_but_cpp_exceptions() {
        assert!(is_fault(0xC000_0005));
        assert!(!is_fault(0x406D_1388)); // a thread name
        assert!(!is_fault(0xE06D_7363)); // a C++ exception
        assert!(is_fault(0xC000_0409));
    }

    #[test]
    fn the_line_is_the_cpp_format() {
        let mut line = Line { bytes: [0; 2048], len: 0 };
        report(&mut line, "EE-SIM", 0xC000_0005, 0x7ff6_1234_abcd, 42, &[0x10, 0x7ff6_0000_0001]);
        assert_eq!(
            std::str::from_utf8(&line.bytes[..line.len]).unwrap(),
            "EE-SIM FAULT c0000005 at 00007FF61234ABCD thread 42; frames: 0000000000000010 00007FF600000001\n"
        );
    }

    #[test]
    fn the_frames_stop_short_of_the_buffer_end() {
        let mut line = Line { bytes: [0; 2048], len: 0 };
        report(&mut line, "EE-AOC", 0xC000_0005, 0, 1, &[1; 200]);
        assert!(line.len <= 2048);
        assert_eq!(line.bytes[line.len - 1], b'\n');
    }
}
