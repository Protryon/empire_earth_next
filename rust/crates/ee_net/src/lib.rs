//! The Network Engine's classes whole in Rust (`src/Low-Level Engine/Network Engine`; PORT_LEARNINGS "LLE Network
//! Engine, round 1"). Each object holds its class's state and every method; the C++ keeps a forwarder only while a
//! C++ caller needs the type, and answers the object's calls out through one host function per class.
//!
//! - `rlock`: a lock a thread may take again while it holds it (the engine's critical sections are Win32's);
//! - `msgmgr`: `NEMessageManager`, the server's ring of the last 50 sent messages with the slots that still owe an
//!   answer, the clients' response times and CPU loads, and the per-slot file transfer queues;
//! - `ffi`: the C ABI (`rust/ee_net.h`).

/// cbindgen:ignore
pub mod msgmgr;
/// cbindgen:ignore
pub mod rlock;

pub mod ffi;

#[cfg(test)]
mod tests;
