//! The Network Engine's classes whole in Rust (`src/Low-Level Engine/Network Engine`; PORT_LEARNINGS "LLE Network
//! Engine, round 1" and "round 2"). Each object holds its class's state and every method; the C++ keeps a forwarder
//! only while a C++ caller needs the type, and answers the object's calls out through one host function per class.
//!
//! - `rlock`: a lock a thread may take again while it holds it (the engine's critical sections are Win32's);
//! - `msgmgr`: `NEMessageManager`, the server's ring of the last 50 sent messages with the slots that still owe an
//!   answer, the clients' response times and CPU loads, and the per-slot file transfer queues;
//! - `lagloss`: `NELagLossSimulator` (with the `NETimestampedMessage`s it queued), the client's simulated lag and
//!   loss with its lag thread;
//! - `recplay`: `NERecordPlayback`, the client's and the server's message recorder and player with its playback
//!   thread;
//! - `qhost`: the two queues' calls out, and the timestamped record format;
//! - `ffi`: the C ABI (`rust/ee_net.h`).

/// cbindgen:ignore
pub mod lagloss;
/// cbindgen:ignore
pub mod msgmgr;
/// cbindgen:ignore
pub mod qhost;
/// cbindgen:ignore
pub mod recplay;
/// cbindgen:ignore
pub mod rlock;

pub mod ffi;

#[cfg(test)]
mod tests;
#[cfg(test)]
mod tests_queues;
