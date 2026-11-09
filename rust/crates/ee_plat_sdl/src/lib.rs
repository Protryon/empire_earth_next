//! The platform layer's window (include/platform/PlatWindow.h) on SDL3, and the C exports: the thin layer of SDL calls
//! over `ee_plat_ui`, which holds the logic and the tests. SDL is the static library tools/xbuild.py builds (one SDL,
//! no `sdl3-sys`): the imports are hand-written, checked against SDL's own headers (their layouts by the golden table
//! of `ee_plat_ui::event`).
//!
//! The `extern "C"` blocks and the exports exist only outside `cargo test`: a test binary of this crate must not link
//! SDL. The crate has no tests of its own; there is nothing left in it to test without a window.

#![allow(non_snake_case, clippy::missing_safety_doc)]
#![cfg_attr(test, allow(dead_code, unused_imports))]

#[cfg(not(test))]
mod exports;
#[cfg(not(test))]
mod ffi;
