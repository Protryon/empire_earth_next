//! The platform layer's window logic and fonts (docs/RUST_PLAN.md R1.4, R1.5, decision D4): what `PlatWindow` and
//! `PlatFont` decide, tested without a window, a game or data. The SDL calls that make them real are the crate
//! `ee_plat_sdl`. The C ABI of `platform/PlatFont.h` and `PlatCp1252ToWide` / `PlatWideToCp1252` of
//! `platform/PlatString.h` are exported here (`#[no_mangle]`, reaching the staticlib through `ee_rust`).

pub mod cp1252;
pub mod event;
pub mod font;
pub mod keys;
pub mod ttf;
pub mod window;
