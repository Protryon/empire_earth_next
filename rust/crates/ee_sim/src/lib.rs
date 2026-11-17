//! The simulation: the units, combat, path finding, formations, the random maps, the tech tree, the scenarios, the
//! computer players, the game messages and the simulation parts of the app.

// `c_long`/`c_ulong` are `i32`/`u32` on Windows and `i64`/`u64` on Linux and macOS: a cast or `from` that one data
// model needs is to the same type on the other. The Windows gate keeps both lints; off Windows they would flag
// only those.
#![cfg_attr(not(windows), allow(clippy::unnecessary_cast, clippy::useless_conversion))]

/// The crate ee_ai before the restructure.
pub mod ai;
pub mod app;
/// The crate ee_astar before the restructure.
pub mod astar;
/// The crate ee_combat before the restructure.
pub mod combat;
/// The crate ee_formations before the restructure.
pub mod formations;
/// The crate ee_netmsg before the restructure.
pub mod netmsg;
/// The crate ee_rmap before the restructure.
pub mod rmap;
/// The crate ee_scenario before the restructure.
pub mod scenario;
/// The crate ee_tech before the restructure.
pub mod tech;
/// The crate ee_units before the restructure.
pub mod units;
