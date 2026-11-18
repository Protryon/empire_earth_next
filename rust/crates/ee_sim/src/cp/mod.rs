//! The computer player and its economy (the AI Thread's port, lane B): what of `EEComputerPlayer`, its placement,
//! resource centres, towns, herds and the influence table moved into Rust whole.
//!
//! - `file`: the saved game's primitives as these loads and saves use them;
//! - `legacy_load`: the readers that step over a retail file's deleted C++ manager blocks (`EECPLegacyLoad.cpp`);
//! - `ffi`: the C ABI the C++ calls (`rust/sim/cp.h`).

pub mod ffi;
pub mod file;
pub mod legacy_load;
