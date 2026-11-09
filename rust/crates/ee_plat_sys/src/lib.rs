//! The OS part of the platform layer: PlatFile, PlatThread and PlatSocket, exported with `#[no_mangle] extern "C"`
//! exactly as `include/platform/Plat*.h` declares them. One implementation per function replaces
//! `src/Platform/{Win32,Posix}`; the differences between systems are `cfg(windows)` / `cfg(unix)` modules.
//!
//! Windows: the same Win32 imports as the C++ (`CreateFileA`, `WaitForSingleObject`, a `CRITICAL_SECTION` per
//! `PlatMutex`, ...), because the test harness patches the exe's import slots and casts a `PlatMutex*` to a
//! `CRITICAL_SECTION*` (PORT_LEARNINGS "Cross-platform: the platform layer").
//! POSIX: the game's Windows-form paths go through `PlatPathToNative` / `PlatPathFromNative` (`native_path`).

#![allow(non_snake_case, non_camel_case_types)]

pub mod constants;

mod cstr;
mod file;
mod socket;
mod thread;

#[cfg(unix)]
mod last_error;
#[cfg(unix)]
mod native_path;
#[cfg(any(unix, test))]
mod text_mode;
#[cfg(any(unix, test))]
mod wildcard;

#[cfg(test)]
mod test_util;
