//! The computer player and its economy (the AI Thread's port, lane B): what of `EEComputerPlayer`, its placement,
//! resource centres, towns, herds and the influence table moved into Rust whole.
//!
//! - `fields`: `EEComputerPlayer`'s plain members as the record the class derives from ([`fields::EeCpFields`]);
//! - `influence`: `EEInfluenceTable` whole (the regions' influence by player, the enemies' sums);
//! - `init`: `Init`'s tuning values from the behaviour table;
//! - `save`: the class's `Load` and `Save` (one field order for both);
//! - `player`: the per-difficulty scales, their setters, `ScaleCost`, `SetNextAttackTime`, `RestoreManagers`' fields;
//! - `timers`: the experiments' timers (Rust's) and the resource state's save blocks;
//! - `file`: the saved game's primitives as these loads and saves use them;
//! - `placement`: `EECPPlacement` whole (the site searches, `CanBuildAt`, the "unit near" tests, the epoch's classes,
//!   the verbs' `EECPNextPlaceSearch`); `resource_center`: `EECPResourceCenter`'s bodies; `place_host`: the world
//!   both ask ([`place_host::PlaceHost`]); `place_game`: its game host and their C ABI;
//! - `events`: `EEComputerPlayer`'s own bodies (the population cap, the centres' grouping, the favourite types,
//!   the civilization's pick, the retired units, the pending epoch, `AvoidsEnemies`) over [`events::PlayerHost`];
//! - `legacy_load`: the readers that step over a retail file's deleted C++ manager blocks (`EECPLegacyLoad.cpp`);
//! - `ffi`: the C ABI the C++ calls (`rust/sim/cp.h`).

pub mod events;
#[cfg(test)]
mod events_tests;
pub mod ffi;
pub mod fields;
pub mod file;
pub mod influence;
pub mod init;
pub mod legacy_load;
pub mod place_game;
pub mod place_host;
pub mod placement;
#[cfg(test)]
mod placement_tests;
pub mod player;
pub mod resource_center;
#[cfg(test)]
mod resource_center_tests;
pub mod save;
pub mod timers;
