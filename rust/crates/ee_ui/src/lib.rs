//! The Low-Level Engine's UI toolkit (`src/Low-Level Engine/User Interface`), moving to Rust behind the C++ UI*
//! classes (docs/RUST_PLAN.md R6, a faithful port). The C++ classes keep their members, vtables and exported names
//! (the harness hooks some of them); their bodies forward to the algorithms here (`UITrampolines.cpp`).
//!
//! Text stays Windows-1252 bytes (`&[u8]`; decision D6 is open). Each module's behaviour is pinned by golden values
//! that a scratch driver printed from the real C++ unit (the module's tests say how).

pub mod layout;
pub mod rect;
pub mod slider;
pub mod wrap;

mod layout_ffi;
// imports the C++ host functions (UISlider.cpp), absent from a test binary
#[cfg(not(test))]
mod slider_ffi;
// imports the C++ text measuring (UITextLabel.cpp)
#[cfg(not(test))]
mod wrap_ffi;

pub use layout::{Edge, Layout};
pub use rect::{float_to_long, um_floor, um_floor_unsigned, UiPoint, UiRect};
