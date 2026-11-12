//! The game's Graphics Thread and camera (docs/RUST_PLAN.md R4.4), moving to Rust leaf by leaf behind their C++
//! classes as plans and values the C++ executes: it keeps every GE/GX call, the render objects, the locks and the
//! world. So far: the camera's math (`camera.rs`, `EECameraControl.cpp`), the pointer's (`mouse.rs`,
//! `RTSMouse.cpp`), the units' screen point, bars and selection rings (`units.rs`, `EEROUnits.cpp`), the picker's
//! ray and ranking (`pick.rs`, `EEUnitPicker.cpp`), the mini map's transforms and view trapezoid (`minimap.rs`,
//! `EEMiniMap.cpp`) and the effects' frustum test and the buildings' near fade (`view.rs`).
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
mod golden_minimap;
#[cfg(test)]
mod golden_mouse;
#[cfg(test)]
mod golden_picker;
#[cfg(test)]
mod golden_rings;
#[cfg(test)]
mod golden_rounits;
#[cfg(test)]
mod golden_view;
pub mod minimap;
pub mod mouse;
pub mod pick;
pub mod units;
pub mod view;
