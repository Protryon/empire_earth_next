//! `extern "C"` for the UI thread's members that are Rust's (`ui_calls.rs`): `EEUserInterface::ChangeRasterizer`,
//! `ResizeGameScreen`, `ShowMessageOfTheDay`, `OpenSelectServer`, `PauseGraphics` and `UpdateInterface` forward
//! here (`User Interface Thread/EEUserInterface.cpp`).

// (the casts of the C `long`s are no-ops on Windows and conversions on LP64)
#![allow(clippy::unnecessary_cast)]

use core::ffi::{c_long, c_void};

use crate::screens::game_cpp::Ffi;
use crate::screens::ui_calls;

/// `EEUserInterface::ChangeRasterizer`.
#[no_mangle]
pub extern "C" fn ee_screens_ui_change_rasterizer(rasterizer: c_long, screen: c_long) {
    ui_calls::change_rasterizer(&mut Ffi, rasterizer as i32, screen as i32);
}

/// `EEUserInterface::ResizeGameScreen`.
#[no_mangle]
pub extern "C" fn ee_screens_ui_resize_game_screen(width: u32, height: u32, force: bool, flag: bool) {
    ui_calls::resize_game_screen(&mut Ffi, width, height, force, flag);
}

/// `EEUserInterface::ShowMessageOfTheDay` (the two `const UWideString*`).
#[no_mangle]
pub extern "C" fn ee_screens_ui_show_motd(game: *const c_void, system: *const c_void) {
    ui_calls::show_message_of_the_day(&mut Ffi, game as usize, system as usize);
}

/// `EEUserInterface::OpenSelectServer`.
#[no_mangle]
pub extern "C" fn ee_screens_ui_open_select_server() {
    ui_calls::open_select_server(&mut Ffi);
}

/// `EEUserInterface::PauseGraphics`.
#[no_mangle]
pub extern "C" fn ee_screens_ui_pause_graphics() {
    ui_calls::pause_graphics(&mut Ffi);
}

/// `EEUserInterface::UpdateInterface`.
#[no_mangle]
pub extern "C" fn ee_screens_ui_update_interface() {
    ui_calls::update_interface(&mut Ffi);
}
