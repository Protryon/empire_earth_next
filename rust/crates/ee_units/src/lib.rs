//! The units' side of the game that is Rust so far: the unit registry behind `EEUnitHandle<T>`
//! (`include/ee/EEUnitHandle.h`, RUST_PLAN R5a.4).
//!
//! A unit handle is the unit's id; the registry maps the id to the object from `RTSPlayer::InsertUnit` to
//! `RTSUnit::FreeNode`. It is read by every thread that resolves a handle (the physics thread under the world's
//! write lock, the UI and graphics threads under its read lock) and written by whichever thread inserts a unit or
//! runs its last `Dereference`, so it is lock-free: see `registry`.
//!
//! The unit store (`store`, RUST_PLAN R5b.4 leaf 5): each unit's plain state as a record by id while the unit is in
//! a player's list, kept current by the C++ members' write-through holders (`include/ee/EEUnitStore.h`). The world
//! calls a decision makes (a group's units as ids, goals as handles) are `host`'s; the box selection's pick over the
//! store is `select`, the active groups' Fill test and the group commands `group` and `commands`. The units stepped
//! every tick, in their order, are `order`; the smallest leaves of a unit's step (the
//! countdowns, the power's regeneration, the camouflage) over the store are `step`, its AI step and the AI script's
//! decision `ai`, the physics models' arithmetic over the store's velocities `motion` and their per-action step
//! arithmetic (the turn, the speed, the pitch) `physics`; the physics tick's unit loop
//! over the stepped units' list `tick`; the classes' `Update`s as sequences of those leaves and host calls `update`,
//! their `ActionChanged`s as decisions over the store and host operations `actions`.
//! The players' diplomacy tables (by player object) are `players`. The goal classes moved whole (one C++ class,
//! `EEUGRust`, every virtual one call) are `rust_goal` (the dispatch and the host) with `patrol`, `calamity`,
//! `explore` and `unload`.

pub mod actions;
pub mod ai;
pub mod build;
pub mod calamity;
pub mod commands;
pub mod explore;
pub mod ffi;
pub mod goals;
pub mod group;
mod header_consts;
pub mod host;
pub mod kinds;
pub mod load;
pub mod motion;
pub mod order;
pub mod patrol;
pub mod physics;
pub mod players;
pub mod registry;
pub mod rust_goal;
pub mod select;
pub mod stats;
pub mod step;
pub mod store;
pub mod tick;
pub mod unload;
pub mod update;

pub use registry::Registry;
