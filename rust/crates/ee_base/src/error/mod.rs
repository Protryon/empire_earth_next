//! The Low-Level Engine's Error System (`src/Low-Level Engine/Error System/Error System.cpp` before; LLE lane round
//! 2): the debug-output filter read from `Filter.txt`, the verification failure (`ESVerify`), the last native
//! error and the checksum sets (`checksum.rs`). C++ keeps `ESDebugOut`'s `printf` formatting and the platform's
//! debug output, `ESVerify`'s and the native error's exported names as one-line forwarders, and reading
//! `Filter.txt` through the File System (text mode).
//!
//! `Filter.txt` (in the startup directory, read on the first debug message): one filter per line, read as `fgets`
//! with a 1024-byte buffer read it (a longer line is several filters; a line ends at its first NUL). The first
//! line may set the mode instead: `-` (the default) suppresses the messages that contain a filter, `+` shows only
//! them; `OutputToDebugger` sets a flag nothing reads. After the first filter, empty lines are skipped (the
//! original compared each line with `';'`, which converted to an empty string). A message starting with `!` always
//! passes; with no filter, every message passes in `-` mode and none in `+` mode.
//!
//! `golden_filter.txt` is the trace of the original function (`tests/error/es_filter_golden.cpp`); `golden.rs`
//! replays it.

pub mod checksum;
#[cfg(test)]
mod golden;
#[cfg(test)]
mod golden_checksum;

use std::ffi::{c_char, c_ulong, CStr};
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::Mutex;

/// The filters of `Filter.txt` and its mode.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Filters {
    /// Loaded (the first message with a startup path read the file, found or not).
    pub loaded: bool,
    /// `true`: suppress what matches (`-`); `false`: show only what matches (`+`).
    pub suppress: bool,
    /// `OutputToDebugger` was read (nothing reads it).
    pub output_to_debugger: bool,
    pub filters: Vec<Vec<u8>>,
}

impl Default for Filters {
    fn default() -> Self {
        Filters { loaded: false, suppress: true, output_to_debugger: false, filters: Vec::new() }
    }
}

/// `fgets(line, 1024, file)` over `text`: the next line (with its '\n') or 1023 bytes, and the rest.
fn fgets(text: &[u8]) -> (&[u8], &[u8]) {
    let limit = text.len().min(1023);
    let end = text[..limit].iter().position(|&b| b == b'\n').map_or(limit, |i| i + 1);
    text.split_at(end)
}

impl Filters {
    /// Reads `Filter.txt`'s text (`None`: not found): the filters are replaced, the mode kept unless the file sets
    /// it; the filters count as loaded either way.
    pub fn load(&mut self, text: Option<&[u8]>) {
        self.filters.clear();
        let mut rest = text.unwrap_or_default();
        while !rest.is_empty() {
            let (chunk, next) = fgets(rest);
            rest = next;
            let mut line = &chunk[..chunk.iter().position(|&b| b == 0).unwrap_or(chunk.len())];
            if line.is_empty() {
                continue;
            }
            if line.last() == Some(&b'\n') {
                line = &line[..line.len() - 1];
            }
            if self.filters.is_empty() {
                match line {
                    b"-" => self.suppress = true,
                    b"+" => self.suppress = false,
                    b"OutputToDebugger" => self.output_to_debugger = true,
                    _ => self.filters.push(line.to_vec()),
                }
            } else if !line.is_empty() {
                self.filters.push(line.to_vec());
            }
        }
        self.loaded = true;
    }

    /// Whether a debug message passes (`strstr` matching: an empty filter matches every message).
    pub fn passes(&self, text: &[u8]) -> bool {
        if text.first() == Some(&b'!') {
            return true;
        }
        let matches = |f: &Vec<u8>| f.is_empty() || text.windows(f.len()).any(|w| w == f.as_slice());
        if self.filters.is_empty() {
            self.suppress
        } else if self.suppress {
            !self.filters.iter().any(matches)
        } else {
            self.filters.iter().any(matches)
        }
    }

    /// The shutdown: the filters freed (still counted as loaded, as in the original).
    pub fn clear(&mut self) {
        self.filters.clear();
    }
}

static FILTERS: Mutex<Filters> =
    Mutex::new(Filters { loaded: false, suppress: true, output_to_debugger: false, filters: Vec::new() });
static NATIVE_ERROR: AtomicU64 = AtomicU64::new(0);
static IN_VERIFY: AtomicBool = AtomicBool::new(false);

fn filters() -> std::sync::MutexGuard<'static, Filters> {
    FILTERS.lock().unwrap_or_else(|e| e.into_inner())
}

/// Whether `Filter.txt` was read (C++ reads it on the first debug message that has a startup path).
#[no_mangle]
pub extern "C" fn ee_error_filters_loaded() -> bool {
    filters().loaded
}

/// Loads `Filter.txt`'s text as text mode read it (`found` false: the file was not found; `text` may then be NULL).
///
/// # Safety
/// `text` holds `len` bytes, or is NULL with `len` 0.
#[no_mangle]
pub unsafe extern "C" fn ee_error_filters_load(found: bool, text: *const u8, len: usize) {
    let bytes = if text.is_null() { &[][..] } else { std::slice::from_raw_parts(text, len) };
    filters().load(found.then_some(bytes));
}

/// Whether a debug message passes the filters (`ESFilterDebugOutput`).
///
/// # Safety
/// `text` is a NUL-terminated string.
#[no_mangle]
pub unsafe extern "C" fn ee_error_filter_passes(text: *const c_char) -> bool {
    filters().passes(CStr::from_ptr(text).to_bytes())
}

/// `ESInitialize`: [`ee_error_shutdown_hook`] registered with the engine's shutdown list (ee_util).
#[no_mangle]
pub extern "C" fn ee_error_initialize() {
    crate::util::ee_util_register_shutdown(ee_error_shutdown_hook);
}

/// `ESShutdown` (a shutdown-list function): [`ee_error_shutdown`].
#[no_mangle]
pub extern "C" fn ee_error_shutdown_hook(_forced: bool) {
    ee_error_shutdown();
}

/// The shutdown hook's part (`ESShutdown`): the filters freed.
#[no_mangle]
pub extern "C" fn ee_error_shutdown() {
    filters().clear();
}

/// `ESRecordNativeError`: a nonzero native error is remembered; returns it unchanged.
#[no_mangle]
pub extern "C" fn ee_error_record_native(error: c_ulong) -> c_ulong {
    if error != 0 {
        NATIVE_ERROR.store(error as u64, Ordering::Relaxed);
    }
    error
}

/// `ESGetNativeError`: the last nonzero native error recorded.
#[no_mangle]
pub extern "C" fn ee_error_native() -> c_ulong {
    NATIVE_ERROR.load(Ordering::Relaxed) as c_ulong
}

/// A verification failure's report ("<file> (<line>) : Verification failure (<expression>)"); `None` for a failure
/// inside another (it is ignored).
pub fn verify_report(file: &[u8], line: c_ulong, expression: &[u8]) -> Option<Vec<u8>> {
    if IN_VERIFY.swap(true, Ordering::AcqRel) {
        return None;
    }
    let mut text = file.to_vec();
    text.extend_from_slice(format!(" ({line}) : Verification failure (").as_bytes());
    text.extend_from_slice(expression);
    text.push(b')');
    // the original's sprintf into 1024 bytes: longer reports overran it; they are cut here
    text.truncate(1023);
    Some(text)
}

/// `ESVerify`: reports a failed verification on stderr, breaks into the debugger and aborts the process; a failure
/// while one is being reported is ignored (returns).
///
/// # Safety
/// `file` and `expression` are NULL or NUL-terminated strings.
#[no_mangle]
pub unsafe extern "C" fn ee_error_verify(file: *const c_char, line: c_ulong, expression: *const c_char) {
    let text = |p: *const c_char| if p.is_null() { &b""[..] } else { CStr::from_ptr(p).to_bytes() };
    let Some(report) = verify_report(text(file), line, text(expression)) else { return };
    use std::io::Write;
    let mut err = std::io::stderr().lock();
    let _ = err.write_all(&report);
    let _ = err.write_all(b"\n");
    let _ = err.flush();
    breakpoint();
    std::process::abort();
}

/// The original's `__debugbreak` (`int3`): a debugger stops here; without one the process ends.
fn breakpoint() {
    #[cfg(target_arch = "x86_64")]
    // SAFETY: a breakpoint trap; no memory or register is touched.
    unsafe {
        std::arch::asm!("int3");
    }
    #[cfg(target_arch = "aarch64")]
    // SAFETY: as above.
    unsafe {
        std::arch::asm!("brk #0xf000");
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn loaded(text: &str) -> Filters {
        let mut f = Filters::default();
        f.load(Some(text.as_bytes()));
        f
    }

    #[test]
    fn suppress_and_show_modes() {
        let f = loaded("-\nnet\nai\n");
        assert!(!f.passes(b"the net is down") && f.passes(b"units") && f.passes(b"!net"));
        let f = loaded("+\nnet\n");
        assert!(f.passes(b"net") && !f.passes(b"units"));
        let f = loaded("+\n");
        assert!(!f.passes(b"anything") && f.passes(b"!anything"));
        assert!(Filters::default().passes(b"x"));
    }

    #[test]
    fn line_rules() {
        // the mode is read only before the first filter; later empty lines are skipped, a first empty line is a
        // filter that matches everything
        let f = loaded("net\n-\n\n+\n");
        assert_eq!(f.filters, vec![b"net".to_vec(), b"-".to_vec(), b"+".to_vec()]);
        let f = loaded("\nnet\n");
        assert_eq!(f.filters, vec![b"".to_vec(), b"net".to_vec()]);
        assert!(!f.passes(b"x"));
        // a line ends at a NUL; a long line is several filters
        let f = loaded("ai\0net\nx");
        assert_eq!(f.filters, vec![b"ai".to_vec(), b"x".to_vec()]);
        let long = "a".repeat(1030);
        assert_eq!(loaded(&long).filters.iter().map(Vec::len).collect::<Vec<_>>(), vec![1023, 7]);
        let mut f = loaded("OutputToDebugger\n+\n");
        assert!(f.output_to_debugger && !f.suppress && f.filters.is_empty());
        f.load(None);
        assert!(f.loaded && !f.suppress);
    }

    #[test]
    fn native_error_keeps_the_last_nonzero() {
        assert_eq!(ee_error_record_native(5), 5);
        assert_eq!(ee_error_record_native(0), 0);
        assert_eq!(ee_error_native(), 5);
    }

    #[test]
    fn verify_reports_once() {
        let r = verify_report(b"a.cpp", 65, b"x == 1").unwrap();
        assert_eq!(r, b"a.cpp (65) : Verification failure (x == 1)");
        assert!(verify_report(b"b.cpp", 1, b"y").is_none());
    }
}
