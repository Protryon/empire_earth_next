//! The WON library in Rust (docs/RUST_PLAN.md R6, decision D8), leaves first: the message buffers, the text
//! conversions, CRC-16, MD5, the random generator, Blowfish, big integers, ElGamal, the auth blocks and the CD key
//! logic; and the plain text the lobby shows where it showed HTML (`plaintext`). The C++ classes of
//! `src/Empire Earth/WONAPI` that remain call these through `ffi`.

pub mod auth;
pub mod bigint;
pub mod blowfish;
mod blowfish_tables;
pub mod cdkey;
pub mod crc16;
pub mod der;
pub mod elgamal;
pub mod ffi;
pub mod md5;
pub mod plaintext;
pub mod random;
pub mod text;
pub mod wire;

#[cfg(test)]
mod golden;
