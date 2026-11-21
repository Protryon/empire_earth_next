//! The RTS Engine's bodies in Rust (LLE round 8: `src/RTS Engine`): the C++ classes stay the types the game holds
//! and subclasses (`ee/EEPhysics.h`, `ee/RTSGraphics.h`, ...), their members forward here, and the C++ calls the
//! bodies make are one-statement shims in the classes' own units (`ee_base::cpp_shims!`).

pub mod physics;
