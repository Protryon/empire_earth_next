//! The simulation parts of the app and world lane (from ee_app): the map, the storms, the cheats, the end-of-game
//! statistics (`EEWorld::RecordStatistics`).

pub mod cheats;
pub mod cheats_ffi;
#[cfg(test)]
mod cheats_tests;
pub mod map;
#[cfg(test)]
mod map_tests;
pub mod statistics;
pub mod statistics_ffi;
#[cfg(test)]
mod statistics_tests;
pub mod storm;
pub mod storm_ffi;
#[cfg(test)]
mod storm_tests;
