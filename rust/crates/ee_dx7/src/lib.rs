//! The DX7 display plug-in ('rast', `src/Low-Level Engine/Graphics Engine/DX7 Display Plug-ins`), moving to Rust
//! file by file behind the C++ trampolines (docs/RUST_PLAN.md R4.2, decision D5: a faithful port).
//!
//! Modules that only need their inputs as plain data are tested with golden values captured from the C++
//! (each module's tests say how).

pub mod api;
pub mod draw;
pub mod draw2d;
pub mod draw_model;
pub mod draw_terrain;
pub mod filter;
pub mod movie;
pub mod movobj;
pub mod raster;
pub mod screen;
pub mod scrobj;
pub mod state;
pub mod surface;
pub mod surfobj;
/// the vertex layouts and arrays (ee_ge vertarr.rs since R4.3 part 17)
pub use ee_ge::vertarr as vertex;

#[cfg(not(test))]
mod draw_ffi;
#[cfg(not(test))]
pub mod gx;
#[cfg(not(test))]
pub mod rastobj;

pub use vertex::{float_to_long, rgb_make, rgba, PolygonArray, StandardVertexArray, StdColorVertexArray};
