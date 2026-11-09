//! The random map generator (`Random Map Generators/Default`): rust port of the C++ plug-in behind the `rmap` PIM.
//! The generator takes flat arrays and returns flat arrays (`ffi`); a C++ adapter (`Default.cpp`) copies them to and
//! from `EERandomMap`. The `.rmv` script reader stays in C++ (`Unit10.cpp`).
//!
//! The generator is part of the deterministic game: every float operation is `f32`, in the C++'s order, with no
//! `libm`, no HashMap iteration and wrapping integers (RUST_PLAN 2.1). `tools/rmap_gate` is the byte-for-byte gate
//! against digests recorded from the C++ generator.
#![allow(clippy::approx_constant, clippy::excessive_precision, clippy::neg_cmp_op_on_partial_ord)]

pub mod clumps;
pub mod ffi;
pub mod fractal;
pub mod generator;
#[cfg(test)]
mod golden;
pub mod grid;
pub mod map;
pub mod math;
pub mod points;
pub mod resources;
