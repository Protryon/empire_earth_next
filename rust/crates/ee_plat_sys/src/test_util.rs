//! Helpers for the unit tests: scratch directories in the game's path form.

use crate::constants::PLAT_MAX_PATH;
use crate::file::imp as file;
use std::ffi::CString;
use std::sync::atomic::{AtomicU32, Ordering};

pub fn c(text: &str) -> CString {
    CString::new(text).expect("no NUL in test strings")
}

pub fn c_text(buffer: &[u8]) -> String {
    let n = buffer.iter().position(|&b| b == 0).unwrap_or(buffer.len());
    String::from_utf8_lossy(&buffer[..n]).into_owned()
}

/// A new, empty directory for one test, in the game's form with a trailing backslash.
pub fn scratch(name: &str) -> String {
    static NEXT: AtomicU32 = AtomicU32::new(0);
    let mut temp = [0u8; PLAT_MAX_PATH];
    let n = unsafe { file::PlatGetTempDirectory(temp.as_mut_ptr().cast(), PLAT_MAX_PATH as u32) };
    assert!(n > 0, "PlatGetTempDirectory failed");
    let unique = NEXT.fetch_add(1, Ordering::Relaxed);
    let dir = format!("{}EeSys{}_{}_{}\\", c_text(&temp), std::process::id(), unique, name);
    let result = unsafe { file::PlatCreateDirectory(c(&dir).as_ptr()) };
    assert_eq!(result, crate::constants::PlatFileResult::Ok, "cannot create {dir}");
    dir
}
