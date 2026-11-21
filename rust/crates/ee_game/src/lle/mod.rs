//! The Low-Level Engine's bodies in Rust over one-statement C++ shims (LLE round 8): the network messages' record
//! class (netrecord.rs).

// `c_long`/`c_ulong` are the build's `long`: 32 bits on Windows, 64 on LP64, so their casts are needed on one of
// them; the platform objects' trait implementations hand raw pointers to the C ABI
#![allow(clippy::unnecessary_cast, clippy::not_unsafe_ptr_arg_deref)]

pub mod netrecord;
