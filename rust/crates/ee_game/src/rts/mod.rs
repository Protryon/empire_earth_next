//! The RTS Engine's bodies in Rust (LLE round 8: `src/RTS Engine`): the C++ classes stay the types the game holds
//! and subclasses (`ee/EEPhysics.h`, `ee/RTSGraphics.h`, ...), their members forward here, and the C++ calls the
//! bodies make are one-statement shims in the classes' own units (`ee_base::cpp_shims!`).

// `c_long`/`c_ulong` are the build's `long`: 32 bits on Windows, 64 on LP64, so their casts are needed on one of
// them; the platform objects' trait implementations hand raw pointers to the C ABI
#![allow(clippy::unnecessary_cast, clippy::not_unsafe_ptr_arg_deref)]

pub mod graphics;
pub mod map;
pub mod minimap;
pub mod mouse;
pub mod mouse_pointer;
pub mod physics;
pub mod tile;
pub mod units;
pub mod world;
