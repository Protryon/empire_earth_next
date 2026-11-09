//! The data layer in Rust (docs/RUST_PLAN.md R2): the db tables' text compiler, the content dump, the rules
//! documents. C++ reaches it through the C API in `ffi` (include/rust/ee_rust.h).

#[allow(clippy::needless_range_loop)] // the loops mirror the C++ index by index
pub mod content;
pub mod dump;
pub mod ffi;
pub mod gate;
pub mod msg;
pub mod node;
pub mod rules;
pub mod spec;
#[allow(clippy::needless_range_loop)]
pub mod text;
pub mod textrow;

/// Generated code (tools/eedata/rustgen.py, rulesgen.py): never hand-edited; `eedata.py check` keeps it current.
#[rustfmt::skip]
pub mod generated {
    pub mod specs_gen;
    pub mod rules_gen;
}
