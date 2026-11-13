//! The game's Graphics Thread and camera (docs/RUST_PLAN.md R4.4), moving to Rust leaf by leaf behind their C++
//! classes as plans and values the C++ executes: it keeps every GE/GX call, the render objects, the locks and the
//! world. So far: the camera's math (`camera.rs`, `EECameraControl.cpp`), the pointer's (`mouse.rs`,
//! `RTSMouse.cpp`), the units' screen point, bars and selection rings (`units.rs`, `EEROUnits.cpp`), the picker's
//! ray and ranking (`pick.rs`, `EEUnitPicker.cpp`), the mini map's transforms and view trapezoid (`minimap.rs`,
//! `EEMiniMap.cpp`), the effects' frustum test and the buildings' near fade (`view.rs`) and the units' ring
//! models (`models.rs`, `EEROUnits::LoadMedia`) and the sky's colours, the sky, star and cloud models' points
//! and the grid mode (`terrain.rs`, `EEROTerrain.cpp`), the earthquake's crack (`quake.rs`), the tracer beam's
//! aim and stretch (`beam.rs`) and the farm's and the volcano's meshes (`farm.rs`, `volcano.rs`, over `mesh.rs`) and the
//! effect manager's containers (`effect_store.rs`) and algorithms (`effect_manager.rs`), the capture's fade, letterbox,
//! clocks and time-of-day light blend (`scene.rs`) and its per-object arithmetic (`capture.rs`), the effects' records, their base classes' Load and Save, their random
//! generator and the smoke generators (`effect_state.rs`).
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

pub mod beam;
pub mod camera;
pub mod capture;
pub mod effect_icon;
pub mod effect_manager;
pub mod effect_rising;
pub mod effect_state;
pub mod effect_store;
pub mod effect_storm;
pub mod effect_tracer;
pub mod effect_wall;
pub mod effects;
pub mod farm;
pub mod ffi;
pub mod ffi_effect;
pub mod ffi_manager;
pub mod ffi_mesh;
pub mod ffi_scene;
#[cfg(test)]
mod golden_beam;
#[cfg(test)]
mod golden_camera;
#[cfg(test)]
mod golden_center;
#[cfg(test)]
mod golden_farm;
#[cfg(test)]
mod golden_lighting;
#[cfg(test)]
mod golden_media;
#[cfg(test)]
mod golden_minimap;
#[cfg(test)]
mod golden_mouse;
#[cfg(test)]
mod golden_picker;
#[cfg(test)]
mod golden_placement;
#[cfg(test)]
mod golden_press;
#[cfg(test)]
mod golden_quads;
#[cfg(test)]
mod golden_quake;
#[cfg(test)]
mod golden_rings;
#[cfg(test)]
mod golden_rounits;
#[cfg(test)]
mod golden_scene;
#[cfg(test)]
mod golden_select;
#[cfg(test)]
mod golden_sky;
#[cfg(test)]
mod golden_view;
#[cfg(test)]
mod golden_volcano;
#[cfg(test)]
mod golden_walk;
pub mod lighting;
pub mod mesh;
pub mod minimap;
pub mod models;
pub mod mouse;
pub mod pick;
pub mod quake;
pub mod raster;
pub mod scene;
pub mod select;
pub mod terrain;
pub mod units;
pub mod view;
pub mod volcano;
pub mod walk;
