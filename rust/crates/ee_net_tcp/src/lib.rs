//! The TCP/IP network plug-in (33 `NPIM*` entry points, `src/Low-Level Engine/Network Engine/TCP Protocol`): stream
//! and datagram sockets over the platform layer, the datagram cipher, the LAN game search and the file-transfer stream.
//! A C++ shim keeps the `NPIM*` names and converts `UString`s; `ffi` is the C ABI it calls.

// `c_long`/`c_ulong` are `i32`/`u32` on Windows and `i64`/`u64` on Linux and macOS: a cast or `from` that one data
// model needs is to the same type on the other. The Windows gate keeps both lints; off Windows they would flag
// only those.
#![cfg_attr(not(windows), allow(clippy::unnecessary_cast, clippy::useless_conversion))]

/// cbindgen:ignore
pub mod cipher;
/// cbindgen:ignore
pub mod host;
/// cbindgen:ignore
pub mod plugin;

#[cfg(not(test))]
pub mod ffi;

#[cfg(test)]
mod tests;

pub use host::{Adapter, Host, Sock, SockAddr, SOCK_INVALID};
pub use plugin::{Connection, Server, Tcp};
