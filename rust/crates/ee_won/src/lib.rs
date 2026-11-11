//! The WON library in Rust (docs/RUST_PLAN.md R6, decision D8), leaves first: the message buffers, the text
//! conversions, CRC-16, MD5, the random generator, Blowfish, big integers, ElGamal, the auth blocks and the CD key
//! logic; the plain text the lobby shows where it showed HTML (`plaintext`); and the network layer on the platform
//! layer's sockets: the status names, NetStats, IPAddr, AsyncSocket's core and the socket ops' algorithms. The C++
//! classes of `src/Empire Earth/WONAPI` that remain call these through `ffi` and `ffi_net`.

pub mod asyncop;
pub mod auth;
pub mod bigint;
pub mod blowfish;
mod blowfish_tables;
pub mod cdkey;
pub mod config;
pub mod crc16;
pub mod der;
pub mod elgamal;
pub mod ffi;
pub mod ffi_lan;
pub mod ffi_net;
pub mod ffi_ops;
pub mod ffi_parse;
pub mod file;
/// cbindgen:ignore
pub mod ipaddr;
pub mod lan;
pub mod md5;
/// cbindgen:ignore
pub mod netstats;
pub mod ops;
pub mod parser;
pub mod plaintext;
pub mod random;
pub mod socket;
/// cbindgen:ignore
pub mod status;
pub mod text;
pub mod wire;

#[cfg(test)]
mod golden;
#[cfg(test)]
mod parser_golden;
