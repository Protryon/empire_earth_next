//! The Graphics Engine (`src/Low-Level Engine/Graphics Engine`, docs/RUST_PLAN.md R4.3), moving to Rust leaf by leaf
//! behind its C++ classes. So far: the math and geometry (`GE2DPoint`, `GE3DPoint`, `GEVector`, `GE3DLine`,
//! `GE3DPlane`, `GETransformation`, `GEOrientation`, `GEQuaternion`, `GEBoundingBox`, `GERectangle`) and the
//! engine's table trigonometry (`UMath.cpp`).
//!
//! The simulation calls this math everywhere (unit positions, distances, orientations, bounding boxes), so it is
//! deterministic code under RUST_PLAN 2.1: `f32` operations in the C++'s order, no fused multiply-add, `lrintf`
//! (`um::lrintf`) for `UMFloatToLong`, std's `sqrt` (exact, like the C++'s) and no `libm` crate. The tests compare
//! every function with golden values from the real C++ (`golden.rs`).
// the C++'s shapes are kept on purpose: its comparisons, its operand order, its index loops
#![allow(
    clippy::approx_constant,
    clippy::excessive_precision,
    clippy::neg_cmp_op_on_partial_ord,
    clippy::manual_range_contains,
    clippy::assign_op_pattern,
    clippy::needless_range_loop
)]

/// cbindgen:ignore
pub mod bbox;
pub mod ffi;
#[cfg(test)]
mod golden;
/// cbindgen:ignore
pub mod line;
/// cbindgen:ignore
pub mod point;
/// cbindgen:ignore
pub mod transform;
pub mod types;
/// cbindgen:ignore
pub mod um;
/// cbindgen:ignore
pub mod vector;
