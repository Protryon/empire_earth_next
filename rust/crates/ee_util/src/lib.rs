//! The Low-Level Engine's Utilities whole in Rust (LLE lane round 7): `UGraphSeries`/`UGraph` (graph.rs, a Rust
//! store under C++ views), `U2DFractal` (fractal.rs, simulation code) and `Utilities.cpp`'s shutdown list and
//! system questions (system.rs), and `UMath.cpp`'s floating-point control (fp.rs). The C++ classes keep their names and hold one pointer each; every member forwards.
//!
//! Golden: `tests/util/util_golden.cpp` traces the original units (`golden_util.txt`, replayed by `golden.rs`).

// `c_long`/`c_ulong` are the build's `long`: 32 bits on Windows, 64 on LP64, so their casts are needed on one of them
#![allow(clippy::unnecessary_cast)]

pub mod fp;
pub mod fractal;
#[cfg(test)]
mod golden;
pub mod graph;
pub mod system;

pub use fp::*;
pub use fractal::*;
pub use graph::*;
pub use system::*;
