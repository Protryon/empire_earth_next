//! The data layer in Rust (docs/RUST_PLAN.md R2): the db tables' text compiler, the content dump, the rules
//! documents. C++ reaches it through the C API in `ffi` (include/rust/ee_rust.h).

// `c_long`/`c_ulong` are `i32`/`u32` on Windows and `i64`/`u64` on Linux and macOS: a cast or `from` that one data
// model needs is to the same type on the other. The Windows gate keeps both lints; off Windows they would flag
// only those.
#![cfg_attr(not(windows), allow(clippy::unnecessary_cast, clippy::useless_conversion))]

#[allow(clippy::needless_range_loop)] // the loops mirror the C++ index by index
pub mod content;
pub mod dump;
pub mod ffi;
pub mod gate;
pub mod msg;
pub mod node;
pub mod rules;
pub mod spec;
pub mod tables;
#[allow(clippy::needless_range_loop)]
pub mod text;
pub mod textrow;

/// Generated code (tools/eedata/rustgen.py, rulesgen.py): never hand-edited; `eedata.py check` keeps it current.
#[rustfmt::skip]
pub mod generated {
    pub mod specs_gen;
    pub mod rules_gen;
}
