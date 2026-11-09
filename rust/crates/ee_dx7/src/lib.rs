//! The DX7 display plug-in ('rast', `src/Low-Level Engine/Graphics Engine/DX7 Display Plug-ins`), moving to Rust
//! file by file behind the C++ trampolines (docs/RUST_PLAN.md R4.2, decision D5: a faithful port).
//!
//! Modules that only need their inputs as plain data are tested with golden values captured from the C++
//! (each module's tests say how).

pub mod vertex;

mod ffi;

pub use vertex::{float_to_long, rgb_make, rgba, PolygonArray, StandardVertexArray, StdColorVertexArray};
