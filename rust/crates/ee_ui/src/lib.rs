//! The Low-Level Engine's UI toolkit (`src/Low-Level Engine/User Interface`), moving to Rust behind the C++ UI*
//! classes (docs/RUST_PLAN.md R6, a faithful port). The C++ classes keep their members, vtables and exported names
//! (the harness hooks some of them); their bodies forward to the algorithms here (`UITrampolines.cpp`).
//!
//! Text is UTF-8 bytes (`&[u8]`; decision D6, `text.rs`). Each module's behaviour is pinned by golden values
//! that a scratch driver printed from the real C++ unit (the module's tests say how).

pub mod background;
pub mod button;
pub mod button_variants;
pub mod combo;
pub mod controls_table;
pub mod draw;
pub mod edit;
pub mod edit_draw;
pub mod font;
pub mod form;
pub mod form_state;
pub mod hotkey;
pub mod layout;
pub mod list;
pub mod manager;
pub mod manager_media;
pub mod rect;
pub mod scrollbar;
pub mod slider;
pub mod text;
pub mod wrap;

// imports the C++ backgrounds' draws and moves (UI*Background.cpp, UIForm.cpp), absent from a test binary
#[cfg(not(test))]
mod background_ffi;
// imports the C++ buttons' host calls (UIButton.cpp and its subclasses' units), absent from a test binary
#[cfg(not(test))]
mod button_ffi;
// imports the C++ tri-state check box's host calls (UITriStateCheckBox.cpp), absent from a test binary
#[cfg(not(test))]
mod button_variants_ffi;
// imports the C++ combo box's host calls (UIComboBox.cpp), absent from a test binary
#[cfg(not(test))]
mod combo_ffi;
// imports the C++ controls table's static array (UIControlsTable.cpp), absent from a test binary
#[cfg(not(test))]
mod controls_table_ffi;
// imports the C++ edit box's lines, measuring and scroll bar (UIEditBox.cpp), absent from a test binary
#[cfg(not(test))]
mod edit_ffi;
// imports the C++ form questions (UIForm.cpp), absent from a test binary
#[cfg(not(test))]
mod form_ffi;
// imports the C++ form's lists, members and calls (UIForm.cpp), absent from a test binary
#[cfg(not(test))]
mod form_state_ffi;
// imports the C++ shared font list (UIFontTable.cpp), absent from a test binary
#[cfg(not(test))]
mod font_ffi;
// imports the C++ hot keys' keys and events and the form's lists (UIHotKey.cpp, UIForm.cpp), absent from a test binary
#[cfg(not(test))]
mod hotkey_ffi;
// imports nothing: the clipped text draw takes the C++'s draw as a function
mod draw_ffi;
// imports nothing: the edit box's measures and draws come as a function table
mod edit_draw_ffi;
mod layout_ffi;
// imports nothing: the list boxes' host calls come as a table from the C++ template; absent from a test binary
#[cfg(not(test))]
mod list_ffi;
// imports the C++ manager's lists and the forms' members (UIManager.cpp), absent from a test binary
#[cfg(not(test))]
mod manager_ffi;
// imports the C++ manager's music and IME members, the sound manager's calls (UIManager.cpp), absent from a test binary
#[cfg(not(test))]
mod manager_media_ffi;
// imports the C++ scroll bar's host calls (UIScrollBar.cpp), absent from a test binary
#[cfg(not(test))]
mod scrollbar_ffi;
// imports the C++ host functions (UISlider.cpp), absent from a test binary
#[cfg(not(test))]
mod slider_ffi;
// imports the C++ text measuring (UITextLabel.cpp)
#[cfg(not(test))]
mod wrap_ffi;

pub use layout::{Edge, Layout};
pub use rect::{float_to_long, um_floor, um_floor_unsigned, UiPoint, UiRect};
