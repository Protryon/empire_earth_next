//! The Graphics Engine (`src/Low-Level Engine/Graphics Engine`, docs/RUST_PLAN.md R4.3), moving to Rust leaf by leaf
//! behind its C++ classes. So far: the math and geometry (`GE2DPoint`, `GE3DPoint`, `GEVector`, `GE3DLine`,
//! `GE3DPlane`, `GETransformation`, `GEOrientation`, `GEQuaternion`, `GEBoundingBox`, `GERectangle`), the
//! engine's table trigonometry (`UMath.cpp`) and `GETerrainMesh`'s height, normal, closest-tile and line queries, its vertex normals, shorelines, fog of war and
//! file codec, `GEPhysicalModel`'s per-triangle tests, and `GEModel`'s geometry (`model_geom.rs`) and the model it
//! stores (`model_store.rs`, decoded from a glTF by `ee_openformats`; the `.cem` reader and writer were retired under
//! D9).
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
#[cfg(test)]
mod golden_model_geom;
#[cfg(test)]
mod golden_physical;
#[cfg(test)]
mod golden_terrain;
#[cfg(test)]
mod golden_terrain_mesh;
pub mod io;
/// cbindgen:ignore
pub mod line;
pub mod model_ffi;
pub mod model_geom;
pub mod model_store;
/// cbindgen:ignore
pub mod physical;
/// cbindgen:ignore
pub mod point;
pub mod terrain;
pub mod terrain_mesh;
/// cbindgen:ignore
pub mod transform;
pub mod types;
/// cbindgen:ignore
pub mod um;
/// cbindgen:ignore
pub mod vector;
