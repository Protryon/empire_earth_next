//! The real [`Platform`]: the platform layer (`PlatWindow.h`, C++ on SDL3), the process environment, and the wgpu log.

use std::ffi::{c_void, CString};
use std::sync::atomic::{AtomicBool, Ordering};

use crate::abi::NativeWindow;
use crate::gx::Platform;
use crate::modes::DisplayInfo;

extern "C" {
    fn PlatWindowGetClientSize(window: *mut c_void, width: *mut i32, height: *mut i32);
    fn PlatWindowGetDisplaySize(window: *mut c_void, width: *mut i32, height: *mut i32) -> i32;
    fn PlatWindowGetDisplayLogicalSize(window: *mut c_void, width: *mut i32, height: *mut i32) -> i32;
    fn PlatWindowGetNative(window: *mut c_void, native: *mut NativeWindow) -> i32;
}

/// `GXLog`: the game's debug output (the harness and the debugger read it) and, with `EE_GX_LOG`, stderr.
pub fn gx_log(line: &str) {
    let mut text = line.replace('\0', " ");
    text.truncate(1021); // the C++'s 1024-byte buffer
    text.push('\n');
    if let Ok(text) = CString::new(text) {
        // SAFETY: a valid NUL-terminated string
        unsafe { ee_plat_core::message::PlatDebugOutput(text.as_ptr()) };
        if std::env::var_os("EE_GX_LOG").is_some() {
            eprint!("{}", text.to_string_lossy());
        }
    }
}

#[derive(Clone, Copy, Debug, Default)]
pub struct RealPlatform;

// `window` values reach here from the renderer's objects: never null, always a live PlatWindow

fn size_of_call(f: unsafe extern "C" fn(*mut c_void, *mut i32, *mut i32) -> i32) -> Option<(i32, i32)> {
    let (mut width, mut height) = (0, 0);
    // SAFETY: the window system is started by the time GX is created; the pointers are to local variables
    (unsafe { f(std::ptr::null_mut(), &mut width, &mut height) } != 0).then_some((width, height))
}

#[allow(clippy::not_unsafe_ptr_arg_deref)]
impl Platform for RealPlatform {
    fn env(&self, name: &str) -> Option<String> {
        std::env::var(name).ok()
    }

    fn log(&self, line: &str) {
        gx_log(line);
    }

    fn client_size(&self, window: *mut c_void) -> (i32, i32) {
        let (mut width, mut height) = (0, 0);
        // SAFETY: `window` is the live window the primary surface shows in
        unsafe { PlatWindowGetClientSize(window, &mut width, &mut height) };
        (width, height)
    }

    fn display_info(&self) -> DisplayInfo {
        DisplayInfo {
            logical: size_of_call(PlatWindowGetDisplayLogicalSize),
            pixels: size_of_call(PlatWindowGetDisplaySize),
        }
    }

    fn native_window(&self, window: *mut c_void) -> NativeWindow {
        let mut native = NativeWindow::default();
        // SAFETY: `window` is a live PlatWindow; the out pointer is local
        unsafe { PlatWindowGetNative(window, &mut native) };
        native
    }

    fn now_ms(&self) -> u64 {
        ee_plat_core::time::PlatTimeMilliseconds64()
    }

    fn now_us(&self) -> u64 {
        static START: std::sync::OnceLock<std::time::Instant> = std::sync::OnceLock::new();
        START.get_or_init(std::time::Instant::now).elapsed().as_micros() as u64
    }

    fn sleep_us(&self, microseconds: u64) {
        std::thread::sleep(std::time::Duration::from_micros(microseconds));
    }

    fn redirect_stderr(&self, path: &str) {
        redirect_stderr(path);
    }

    fn install_wgpu_log(&self, level: &str) {
        install_wgpu_log(level);
    }
}

#[cfg(windows)]
fn redirect_stderr(path: &str) {
    const GENERIC_WRITE: u32 = 0x4000_0000;
    const FILE_SHARE_READ: u32 = 1;
    const CREATE_ALWAYS: u32 = 2;
    const STD_ERROR_HANDLE: u32 = (-12i32) as u32;
    extern "system" {
        fn CreateFileA(
            name: *const std::ffi::c_char,
            access: u32,
            share: u32,
            security: *mut c_void,
            disposition: u32,
            flags: u32,
            template: *mut c_void,
        ) -> *mut c_void;
        fn SetStdHandle(which: u32, handle: *mut c_void) -> i32;
    }
    let Ok(path) = CString::new(path) else { return };
    // SAFETY: Win32 calls with a valid NUL-terminated path
    unsafe {
        let handle = CreateFileA(
            path.as_ptr(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            std::ptr::null_mut(),
            CREATE_ALWAYS,
            0,
            std::ptr::null_mut(),
        );
        if handle as isize != -1 {
            SetStdHandle(STD_ERROR_HANDLE, handle);
        }
    }
}

#[cfg(unix)]
fn redirect_stderr(path: &str) {
    extern "C" {
        fn open(name: *const std::ffi::c_char, flags: i32, ...) -> i32;
        fn dup2(from: i32, to: i32) -> i32;
        fn close(fd: i32) -> i32;
    }
    const O_WRONLY: i32 = 1;
    #[cfg(target_os = "linux")]
    const O_CREAT_TRUNC: i32 = 0o100 | 0o1000;
    #[cfg(target_os = "macos")]
    const O_CREAT_TRUNC: i32 = 0x200 | 0x400;
    let Ok(path) = CString::new(path) else { return };
    // SAFETY: POSIX calls with a valid NUL-terminated path; file descriptor 2 is stderr
    unsafe {
        let fd = open(path.as_ptr(), O_WRONLY | O_CREAT_TRUNC, 0o644);
        if fd >= 0 {
            dup2(fd, 2);
            close(fd);
        }
    }
}

struct WgpuLogger;

impl log::Log for WgpuLogger {
    fn enabled(&self, _: &log::Metadata) -> bool {
        true
    }

    fn log(&self, record: &log::Record) {
        // WGPULogLevel's numbering
        let level = match record.level() {
            log::Level::Error => 1,
            log::Level::Warn => 2,
            log::Level::Info => 3,
            log::Level::Debug => 4,
            log::Level::Trace => 5,
        };
        gx_log(&format!("GX: wgpu log {level}: {}", record.args()));
    }

    fn flush(&self) {}
}

static LOGGER: WgpuLogger = WgpuLogger;
static LOGGER_SET: AtomicBool = AtomicBool::new(false);

/// `EE_GX_LOG`: wgpu's log into the game's (backend details of validation errors).
fn install_wgpu_log(level: &str) {
    if !LOGGER_SET.swap(true, Ordering::SeqCst) {
        let _ = log::set_logger(&LOGGER);
    }
    log::set_max_level(match level {
        "debug" => log::LevelFilter::Debug,
        "info" => log::LevelFilter::Info,
        _ => log::LevelFilter::Warn,
    });
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn logging_handles_long_and_odd_text() {
        gx_log("GX: a short line");
        gx_log(&"x".repeat(5000));
        gx_log("contains \0 a nul");
    }

    #[test]
    fn the_stderr_redirect_tolerates_a_bad_path() {
        redirect_stderr("/no/such/directory/at/all/here\0"); // a NUL: refused before the OS
    }

    #[test]
    fn the_logger_installs_once() {
        install_wgpu_log("warn");
        install_wgpu_log("debug");
        assert_eq!(log::max_level(), log::LevelFilter::Debug);
        install_wgpu_log("off-ish");
        assert_eq!(log::max_level(), log::LevelFilter::Warn);
    }
}
