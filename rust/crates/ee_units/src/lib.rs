//! The units' side of the game that is Rust so far: the unit registry behind `EEUnitHandle<T>`
//! (`include/ee/EEUnitHandle.h`, RUST_PLAN R5a.4).
//!
//! A unit handle is the unit's id; the registry maps the id to the object from `RTSPlayer::InsertUnit` to
//! `RTSUnit::FreeNode`. It is read by every thread that resolves a handle (the physics thread under the world's
//! write lock, the UI and graphics threads under its read lock) and written by whichever thread inserts a unit or
//! runs its last `Dereference`, so it is lock-free: see `registry`.

pub mod ffi;
pub mod registry;

pub use registry::Registry;
