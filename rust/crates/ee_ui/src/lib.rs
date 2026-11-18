//! The Low-Level Engine's UI toolkit (`src/Low-Level Engine/User Interface`), moving to Rust behind the C++ UI*
//! classes (docs/RUST_PLAN.md R6, a faithful port). The C++ classes keep their members, vtables and exported names
//! (the harness hooks some of them); their bodies forward to the algorithms here (`UITrampolines.cpp`).
//!
//! Text is UTF-8 bytes (`&[u8]`; decision D6, `text.rs`). Each module's behaviour is pinned by golden values
//! that a scratch driver printed from the real C++ unit (the module's tests say how).

// `c_long`/`c_ulong` are `i32`/`u32` on Windows and `i64`/`u64` on Linux and macOS: a cast or `from` that one data
// model needs is to the same type on the other. The Windows gate keeps both lints; off Windows they would flag
// only those.
#![cfg_attr(not(windows), allow(clippy::unnecessary_cast, clippy::useless_conversion))]

pub mod background;
pub mod background_table;
pub mod bgobj;
pub mod bgtableobj;
pub mod button;
pub mod button_variants;
pub mod buttonobj;
pub mod checkobj;
pub mod combo;
pub mod comboobj;
pub mod controlobj;
pub mod controls_table;
pub mod direct;
pub mod draw;
pub mod edit;
pub mod edit_draw;
pub mod edit_keys;
pub mod editobj;
pub mod entity_group;
pub mod event;
pub mod eventobj;
pub mod filelistobj;
pub mod find_key;
pub mod font;
pub mod fonttableobj;
pub mod form;
pub mod form_state;
pub mod formobj;
pub mod graph;
pub mod graphobj;
pub mod groupobj;
pub mod hotkey;
pub mod hotkeyobj;
pub mod label_fit;
pub mod labelobj;
pub mod layout;
pub mod list;
pub mod listobj;
pub mod manager;
pub mod manager_media;
pub mod managerobj;
pub mod mediaobj;
pub mod mouse;
pub mod obj;
pub mod rect;
pub mod scrollbar;
pub mod scrollbarobj;
pub mod scrolling;
pub mod slider;
pub mod sliderobj;
pub mod status;
pub mod statusobj;
pub mod tableobj;
pub mod text;
pub mod variantobj;
pub mod wrap;

// the hot keys' name and order (UIHotKey.cpp, UIForm.cpp)
mod hotkey_ffi;
// imports nothing: the clipped text draw takes the C++'s draw as a function
mod draw_ffi;
mod layout_ffi;
// imports the C++ controls' and form's mouse calls (UIControl.cpp, UIForm.cpp), absent from a test binary
#[cfg(not(any(test, feature = "no_cpp")))]
mod mouse_ffi;
// imports the C++ string list's lines (UILBString.cpp), absent from a test binary
#[cfg(not(any(test, feature = "no_cpp")))]
mod string_list_ffi;
// imports the C++ scrolling label's members, timers and lines (UITLScrolling.cpp), absent from a test binary
// imports the C++ text measuring (UITextLabel.cpp)

pub use layout::{Edge, Layout};
pub use rect::{float_to_long, um_floor, um_floor_unsigned, UiPoint, UiRect};
