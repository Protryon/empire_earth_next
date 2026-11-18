//! EE-SIM's driver (TEST ONLY; docs/progress/app-core.md): the headless simulation's command line, its menus and
//! lobby driven as the harness drives them, the run on the physics thread's after-tick, the samples and the end of
//! the run, the JSON report, and `--games`' several games in one process. Empire Earth/EESim.cpp is the host: the
//! UI's controls, the world lock, the sweeps of the simulation capture, the checksum line's and the state dump's
//! reads, the platform switches and the C library's streams ([`host`]).
//!
//! - [`options`]: the options and their parsing (the C library's `atol`, `atof`, `strtoul`).
//! - [`report`]: the samples and the report's text.
//! - [`lobby`]: the lobby's choices and a combo's line by name.
//! - [`driver`]: the driver: its state, the after-tick, the menus, a game, the batch.
//! - [`ffi`]: the C ABI EESim.cpp calls.

pub mod driver;
pub mod ffi;
pub mod host;
pub mod lobby;
pub mod options;
pub mod report;

#[cfg(test)]
mod tests;
