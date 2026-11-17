//! The platform layer's window logic and fonts (docs/RUST_PLAN.md R1.4, R1.5, decision D4): what `PlatWindow` and
//! `PlatFont` decide, tested without a window, a game or data. The SDL calls that make them real are the crate
//! `ee_plat_sdl`. The C ABI of `platform/PlatFont.h` is exported here (`#[no_mangle]`, reaching the staticlib through
//! `ee_rust`). Text is UTF-8 throughout (decision D6; the conversions are `ee_plat_core::utf8`).

pub mod event;
pub mod font;
pub mod keys;
pub mod ttf;
pub mod window;
