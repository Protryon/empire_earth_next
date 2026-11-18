//! The computer player and its economy (the AI Thread's port, lane B): what of `EEComputerPlayer`, its placement,
//! resource centres, towns, herds and the influence table moved into Rust whole.
//!
//! - `fields`: `EEComputerPlayer`'s plain members as the record the class derives from ([`fields::EeCpFields`]);
//! - `init`: `Init`'s tuning values from the behaviour table;
//! - `save`: the class's `Load` and `Save` (one field order for both);
//! - `player`: the per-difficulty scales, their setters, `ScaleCost`, `SetNextAttackTime`, `RestoreManagers`' fields;
//! - `file`: the saved game's primitives as these loads and saves use them;
//! - `legacy_load`: the readers that step over a retail file's deleted C++ manager blocks (`EECPLegacyLoad.cpp`);
//! - `ffi`: the C ABI the C++ calls (`rust/sim/cp.h`).

pub mod ffi;
pub mod fields;
pub mod file;
pub mod init;
pub mod legacy_load;
pub mod player;
pub mod save;
