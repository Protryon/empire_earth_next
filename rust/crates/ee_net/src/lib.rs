//! The Network Engine (its TCP plug-in is the crate ee_net_tcp, reached through its C ABI).

// `c_long`/`c_ulong` are `i32`/`u32` on Windows and `i64`/`u64` on Linux and macOS: a cast or `from` that one data
// model needs is to the same type on the other. The Windows gate keeps both lints; off Windows they would flag
// only those.
#![cfg_attr(not(windows), allow(clippy::unnecessary_cast, clippy::useless_conversion))]

/// The crate ee_net before the restructure.
pub mod net;
