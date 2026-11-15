//! The Network Engine's classes whole in Rust (`src/Low-Level Engine/Network Engine`; PORT_LEARNINGS "LLE Network
//! Engine, round 1" and "round 2"). Each object holds its class's state and every method; the C++ keeps a forwarder
//! only while a C++ caller needs the type, and answers the object's calls out through one host function per class.
//!
//! - `comms`: `NESynchronousCommunications`' state, the base class's fields the C++ base, the server and the
//!   client share;
//! - `rlock`: a lock a thread may take again while it holds it (the engine's critical sections are Win32's);
//! - `msgmgr`: `NEMessageManager`, the server's ring of the last 50 sent messages with the slots that still owe an
//!   answer, the clients' response times and CPU loads, and the per-slot file transfer queues;
//! - `lagloss`: `NELagLossSimulator` (with the `NETimestampedMessage`s it queued), the client's simulated lag and
//!   loss with its lag thread;
//! - `recplay`: `NERecordPlayback`, the client's and the server's message recorder and player with its playback
//!   thread;
//! - `client`: `NESynchronousClient`, the lock-step client with its main and receive threads, its lag simulator and
//!   recorder;
//! - `tune`: the lock-step server's network and CPU tuning and its booting of laggers (NESynchronousServer's
//!   TuneServer and the members it calls);
//! - `server`: `NESynchronousServer`, the lock-step server with its main, send and transfer threads, its message
//!   manager, tuning and recorder;
//! - `qhost`: the two queues' calls out, and the timestamped record format;
//! - `native`: the engine's clock, events and threads the game's server and client call directly;
//! - `ffi_protocol`: `NEProtocol` whole, over the TCP plug-in (`ee_net_tcp`'s C ABI), with its C ABI;
//! - `ffi`, `ffi_comms`, `ffi_client`, `ffi_tune`, `ffi_server`: the C ABI (`rust/ee_net.h`).

/// cbindgen:ignore
pub mod client;
/// cbindgen:ignore
pub mod comms;
/// cbindgen:ignore
pub mod lagloss;
/// cbindgen:ignore
pub mod msgmgr;
/// cbindgen:ignore
pub mod native;
/// cbindgen:ignore
pub mod qhost;
/// cbindgen:ignore
pub mod recplay;
/// cbindgen:ignore
pub mod rlock;
/// cbindgen:ignore
pub mod server;
/// cbindgen:ignore
pub mod tune;

pub mod ffi;
pub mod ffi_client;
pub mod ffi_comms;
pub mod ffi_protocol;
pub mod ffi_server;
pub mod ffi_tune;

#[cfg(test)]
mod tests;
#[cfg(test)]
mod tests_client;
#[cfg(test)]
mod tests_protocol;
#[cfg(test)]
mod tests_queues;
#[cfg(test)]
mod tests_server;
#[cfg(test)]
mod tests_tune;
