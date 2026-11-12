//! The game's Graphics Thread and camera (docs/RUST_PLAN.md R4.4), moving to Rust leaf by leaf behind their C++
//! classes as plans and values the C++ executes: it keeps every GE/GX call, the render objects, the locks and the
//! world. So far: the camera's math (`camera.rs`, `EECameraControl.cpp`), the pointer's (`mouse.rs`,
//! `RTSMouse.cpp`) and the units' screen point and bars (`units.rs`, `EEROUnits.cpp`).
//!
//! The camera's `Update` also runs in the simulation tick, so its math is deterministic code under RUST_PLAN 2.1;
//! the tests replay golden values from the unmodified C++ (`golden_camera.txt`).
// the C++'s shapes are kept on purpose: its constants as written, its comparisons, its nested tests
#![allow(
    clippy::approx_constant,
    clippy::neg_cmp_op_on_partial_ord,
    clippy::excessive_precision,
    clippy::assign_op_pattern,
    clippy::double_comparisons,
    clippy::collapsible_if
)]

pub mod camera;
pub mod ffi;
#[cfg(test)]
mod golden_camera;
#[cfg(test)]
mod golden_mouse;
#[cfg(test)]
mod golden_picker;
#[cfg(test)]
mod golden_rounits;
pub mod mouse;
pub mod pick;
pub mod units;
