//! The TCP/IP network plug-in (33 `NPIM*` entry points, `src/Low-Level Engine/Network Engine/TCP Protocol`): stream
//! and datagram sockets over the platform layer, the datagram cipher, the LAN game search and the file-transfer stream.
//! A C++ shim keeps the `NPIM*` names and converts `UString`s; `ffi` is the C ABI it calls.

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
