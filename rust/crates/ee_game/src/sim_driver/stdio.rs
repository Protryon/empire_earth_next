//! TEST ONLY: EE-SIM's C library calls (were EESim.cpp's host functions HostSetEnv, HostDupStderr, HostSetStderr,
//! HostWrite, HostQuit, and HostRngTrace with its RngTraceHook): the C runtime's environment, descriptors and streams,
//! so the game's C++ (which reads `getenv` and writes `stdout`/`stderr` through the same C library) sees what the
//! driver did. The functions are the C library's own (UCRT's `_putenv_s`, `_dup`, `_dup2`, `_fileno`, `_write`;
//! POSIX `setenv`, `dup`, `dup2`, `fileno`, `write`).

use std::ffi::{c_long, CString};
#[cfg(not(test))]
use std::io::Write as _;
use std::sync::Mutex;

#[cfg(not(test))]
mod crt {
    use std::ffi::{c_char, c_int, c_uint, c_void};

    /// The C library's `FILE` (opaque: declared as cp_engine.rs declares it).
    pub type CFile = c_void;

    extern "C" {
        pub fn fopen(name: *const c_char, mode: *const c_char) -> *mut CFile;
        pub fn fclose(file: *mut CFile) -> c_int;
        pub fn fflush(file: *mut CFile) -> c_int;
        pub fn fwrite(bytes: *const c_void, size: usize, count: usize, file: *mut CFile) -> usize;
    }
    #[cfg(windows)]
    extern "C" {
        fn __acrt_iob_func(index: c_uint) -> *mut CFile;
        pub fn _putenv_s(name: *const c_char, value: *const c_char) -> c_int;
        pub fn _dup(fd: c_int) -> c_int;
        pub fn _dup2(from: c_int, to: c_int) -> c_int;
        pub fn _fileno(file: *mut CFile) -> c_int;
        pub fn _write(fd: c_int, bytes: *const c_void, count: c_uint) -> c_int;
        pub fn _exit(code: c_int) -> !;
    }
    #[cfg(not(windows))]
    extern "C" {
        pub fn setenv(name: *const c_char, value: *const c_char, overwrite: c_int) -> c_int;
        pub fn dup(fd: c_int) -> c_int;
        pub fn dup2(from: c_int, to: c_int) -> c_int;
        pub fn fileno(file: *mut CFile) -> c_int;
        pub fn write(fd: c_int, bytes: *const c_void, count: usize) -> isize;
        pub fn _exit(code: c_int) -> !;
    }
    #[cfg(target_os = "macos")]
    extern "C" {
        static __stdoutp: *mut CFile;
        static __stderrp: *mut CFile;
    }
    #[cfg(all(not(windows), not(target_os = "macos")))]
    extern "C" {
        static stdout: *mut CFile;
        static stderr: *mut CFile;
    }

    /// The C library's `stdout` (1) or `stderr` (2).
    ///
    /// # Safety
    /// The C library is initialised (always, once the game runs).
    pub unsafe fn stream(which: c_int) -> *mut CFile {
        #[cfg(windows)]
        // SAFETY: UCRT's stream table (0 stdin, 1 stdout, 2 stderr)
        unsafe {
            __acrt_iob_func(which as c_uint)
        }
        #[cfg(target_os = "macos")]
        // SAFETY: the C library's streams
        unsafe {
            if which == 1 {
                __stdoutp
            } else {
                __stderrp
            }
        }
        #[cfg(all(not(windows), not(target_os = "macos")))]
        // SAFETY: the C library's streams
        unsafe {
            if which == 1 {
                stdout
            } else {
                stderr
            }
        }
    }
}

/// A NUL-ended copy of the text (any NUL in it dropped).
fn c(text: &str) -> CString {
    CString::new(text.replace('\0', "")).unwrap_or_default()
}

/// HostSetEnv: the C library's environment (`_putenv_s` / `setenv`), which the game's `getenv` reads.
pub fn set_env(name: &str, value: &str) {
    let (n, v) = (c(name), c(value));
    #[cfg(not(test))]
    // SAFETY: two NUL-ended texts
    unsafe {
        #[cfg(windows)]
        crt::_putenv_s(n.as_ptr(), v.as_ptr());
        #[cfg(not(windows))]
        crt::setenv(n.as_ptr(), v.as_ptr(), 1);
    }
    #[cfg(test)]
    let _ = (n, v);
}

/// HostDupStderr: a duplicate of the process's stderr descriptor.
pub fn dup_stderr() -> i32 {
    #[cfg(all(windows, not(test)))]
    // SAFETY: descriptor 2
    return unsafe { crt::_dup(2) };
    #[cfg(all(not(windows), not(test)))]
    // SAFETY: descriptor 2
    return unsafe { crt::dup(2) };
    #[cfg(test)]
    -1
}

/// stderr (descriptor 2) made a copy of the descriptor, the streams flushed first (the C++ flushed stderr only).
///
/// # Safety
/// `fd` is an open descriptor.
#[cfg(not(test))]
unsafe fn redirect_stderr(fd: std::ffi::c_int) {
    // SAFETY: the caller's descriptor
    unsafe {
        crt::fflush(std::ptr::null_mut());
        #[cfg(windows)]
        crt::_dup2(fd, 2);
        #[cfg(not(windows))]
        crt::dup2(fd, 2);
    }
}

/// HostSetStderr: stderr (descriptor 2) to the file, opened for writing, or (no file) back to the descriptor.
pub fn set_stderr(file: Option<&str>, fd: i32) {
    #[cfg(not(test))]
    // SAFETY: the C library's file and descriptor calls on a file this function opens and closes
    unsafe {
        let Some(file) = file else {
            redirect_stderr(fd);
            return;
        };
        let name = c(file);
        let err = crt::fopen(name.as_ptr(), c"w".as_ptr());
        if !err.is_null() {
            #[cfg(windows)]
            redirect_stderr(crt::_fileno(err));
            #[cfg(not(windows))]
            redirect_stderr(crt::fileno(err));
            crt::fclose(err);
        }
    }
    #[cfg(test)]
    let _ = (file, fd);
}

/// HostWrite: bytes to the C library's stdout (1) or stderr (2), flushed when asked (stderr always), or to a
/// descriptor (3 + fd) after the streams are flushed.
pub fn write(stream: i32, bytes: &[u8], flush: bool) {
    #[cfg(not(test))]
    // SAFETY: the C library's streams and descriptors; `bytes` is live
    unsafe {
        if stream >= 3 {
            crt::fflush(std::ptr::null_mut());
            #[cfg(windows)]
            crt::_write(stream - 3, bytes.as_ptr().cast(), bytes.len() as u32);
            #[cfg(not(windows))]
            let _ = crt::write(stream - 3, bytes.as_ptr().cast(), bytes.len());
            return;
        }
        let out = crt::stream(if stream == 1 { 1 } else { 2 });
        if !bytes.is_empty() {
            crt::fwrite(bytes.as_ptr().cast(), 1, bytes.len(), out);
        }
        if flush || stream == 2 {
            crt::fflush(out);
        }
    }
    #[cfg(test)]
    let _ = (stream, bytes, flush);
}

/// HostQuit: the capture and the streams flushed, then the process ended with the code (Windows: TerminateProcess,
/// so no other thread runs on into the static destructors).
pub fn quit(code: i32) -> ! {
    #[cfg(not(test))]
    // SAFETY: the C library's flush and exit; TerminateProcess on this process
    unsafe {
        super::sim_ops::h(super::sim_ops::EeSimDriverSim::CaptureFlush, 0, &[]);
        crt::fflush(std::ptr::null_mut());
        #[cfg(windows)]
        windows_sys_059::Win32::System::Threading::TerminateProcess(
            windows_sys_059::Win32::System::Threading::GetCurrentProcess(),
            code as u32,
        );
        crt::_exit(code)
    }
    #[cfg(test)]
    std::process::exit(code)
}

// ------------------------------------------------------------------------------------------------ --rng-trace

/// --rng-trace MS=FILE (the save/load fidelity hunt): every draw of the physics random stream after game ms MS
/// (gURandomTraceHook, lle/URandom.h), one line each: the draw's number, the new seed, the game ms, the unit being
/// stepped (0 outside UpdateUnits) and the callers as exe-relative addresses (tools/sim.py resolve adds the map's
/// preferred base). The same MS in a continued game and in the game loaded from its save at MS gives two traces
/// whose first differing caller names the state that differs. At most this many draws; the hook reads only.
pub const RNG_TRACE_MAX: u64 = 50_000;

/// The trace's settings and file (the file opened at the first traced draw, each line written unbuffered).
struct RngTrace {
    path: String,
    after_ms: c_long,
    #[cfg_attr(test, allow(dead_code))]
    file: Option<std::fs::File>,
    lines: u64,
}

static RNG: Mutex<Option<RngTrace>> = Mutex::new(None);

/// One trace line as the C++ printed it (`"%lu %08lx %ld u=%lu"`, then `" %llx"` per frame, then a new line).
pub fn rng_line(number: u64, seed: u32, ms: c_long, unit: u64, frames: &[u64]) -> String {
    let mut line = format!("{number} {seed:08x} {ms} u={unit}");
    for f in frames {
        line.push_str(&format!(" {f:x}"));
    }
    line.push('\n');
    line
}

/// gURandomTraceHook (the LLE's pointer, GEBitmap.cpp).
#[cfg(not(test))]
mod rng_hook {
    use ee_base::random::{EeURandom, EeURandomTraceHook};

    extern "C" {
        static mut gURandomTraceHook: EeURandomTraceHook;
    }

    pub fn set(on: bool) {
        let hook: EeURandomTraceHook = if on { Some(super::rng_draw) } else { None };
        // SAFETY: the hook pointer, set before the game runs or from the drawing thread
        unsafe { *core::ptr::addr_of_mut!(gURandomTraceHook) = hook };
    }

    pub fn seed(r: *mut EeURandom) -> u32 {
        // SAFETY: the drawing generator
        unsafe { (*r).seed }
    }
}

/// The exe-relative return addresses of the hook's callers (Windows; none elsewhere, as the C++'s shim of
/// RtlCaptureStackBackTrace gave), skipping this function and the hook.
#[cfg(not(test))]
#[inline(never)]
fn callers() -> Vec<u64> {
    #[cfg(windows)]
    {
        use std::ffi::c_void;
        #[link(name = "kernel32")]
        extern "system" {
            fn GetModuleHandleA(name: *const u8) -> *mut c_void;
        }
        let mut frames = [std::ptr::null_mut::<c_void>(); 16];
        // SAFETY: a 16-entry buffer; this process's module
        let (count, base) = unsafe {
            (
                windows_sys_059::Win32::System::Diagnostics::Debug::RtlCaptureStackBackTrace(
                    2,
                    16,
                    frames.as_mut_ptr(),
                    std::ptr::null_mut(),
                ),
                GetModuleHandleA(std::ptr::null()) as u64,
            )
        };
        frames[..count as usize].iter().map(|f| *f as u64).filter(|f| *f >= base).map(|f| f - base).collect()
    }
    #[cfg(not(windows))]
    Vec::new()
}

/// gURandomTraceHook: a draw of any generator; only the physics' stream after the trace's game ms is written.
///
/// # Safety
/// `r` is the generator that drew.
#[cfg(not(test))]
#[inline(never)]
unsafe extern "C" fn rng_draw(r: *mut ee_base::random::EeURandom) {
    use super::sim_ops::{h, EeSimDriverSim as O};
    let physics_random = h(O::PhysicsRandom, 0, &[]) as usize;
    if physics_random == 0 || physics_random != r as usize {
        return;
    }
    let ms = h(O::PhysicsGameTime, 0, &[]) as c_long;
    let mut guard = RNG.lock().unwrap_or_else(|e| e.into_inner());
    let Some(trace) = guard.as_mut() else { return };
    if ms <= trace.after_ms {
        return;
    }
    if trace.file.is_none() {
        match std::fs::File::create(&trace.path) {
            Ok(f) => trace.file = Some(f),
            Err(_) => {
                rng_hook::set(false);
                return;
            }
        }
    }
    trace.lines += 1;
    if trace.lines > RNG_TRACE_MAX {
        trace.file = None;
        rng_hook::set(false);
        return;
    }
    // SAFETY: the physics' stepping unit pointer (gamecpp globals.rs), written by this thread
    let stepping = unsafe { *core::ptr::addr_of!(crate::gamecpp::globals::gPhysicsSteppingUnit) } as usize;
    let unit = if stepping != 0 { h(O::DmCxUnitId, stepping, &[]) as u64 } else { 0 };
    let mut line = rng_line(trace.lines, rng_hook::seed(r), ms, unit, &callers());
    if cfg!(windows) {
        line.insert(line.len() - 1, char::from(13u8)); // the C++'s text-mode FILE wrote CR LF
    }
    if let Some(f) = trace.file.as_mut() {
        let _ = f.write_all(line.as_bytes());
    }
}

/// HostRngTrace: the trace from game ms `ms` into the file.
pub fn rng_trace(ms: c_long, path: &str) {
    *RNG.lock().unwrap_or_else(|e| e.into_inner()) =
        Some(RngTrace { path: path.to_string(), after_ms: ms, file: None, lines: 0 });
    #[cfg(not(test))]
    rng_hook::set(true);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rng_line_is_the_cs_format() {
        assert_eq!(rng_line(1, 0xab, 1500, 0, &[]), "1 000000ab 1500 u=0\n");
        assert_eq!(rng_line(50000, 0x1234_5678, -1, 77, &[0x1a2b, 0x10]), "50000 12345678 -1 u=77 1a2b 10\n");
    }

    #[test]
    fn rng_trace_records_the_settings() {
        rng_trace(600, "x.txt");
        let g = RNG.lock().unwrap();
        let t = g.as_ref().unwrap();
        assert_eq!((t.after_ms, t.path.as_str(), t.lines), (600, "x.txt", 0));
    }

    #[test]
    fn c_strips_nuls() {
        assert_eq!(c("a\0b").as_bytes(), b"ab");
        assert_eq!(dup_stderr(), -1);
    }
}
