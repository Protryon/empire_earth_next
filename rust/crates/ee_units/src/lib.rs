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
//! The players' diplomacy tables (by player object) are `players`, the player's saved state and its Load/Save
//! `player_save`, its tick and reset over that record `player_tick`, its
//! statistics `player_stats` and events `player_events`; the turrets' turn `turret`. The goal classes moved whole (one C++ class,
//! `EEUGRust`, every virtual one call) are `rust_goal` (the dispatch and the host) with `patrol`, `calamity`,
//! `explore` and `unload`. The unit utilities (EEFOFindNewTargetNearOldTarget.cpp: the tile rings and directions,
//! the units on tiles, the wall lines, the placement tests) are `uu_tiles` (the pure geometry) and `uu` over its host.

pub mod actions;
pub mod ai;
pub mod animate;
pub mod atc;
pub mod atc_save;
pub mod boarding;
pub mod build;
pub mod calamity;
pub mod commands;
pub mod explore;
pub mod ffi;
pub mod gather;
pub mod goals;
pub mod group;
pub mod group_save;
pub mod hack;
mod header_consts;
pub mod host;
pub mod improve;
pub mod kinds;
pub mod load;
pub mod location;
pub mod map_save;
pub mod motion;
pub mod msvc_sort;
pub mod order;
pub mod patrol;
pub mod patrol_group;
pub mod physics;
pub mod physics_save;
pub mod player_events;
pub mod player_save;
pub mod player_stats;
pub mod player_tick;
pub mod players;
pub mod pm;
pub mod pm_air;
pub mod pm_ground;
pub mod pm_host;
#[cfg(test)]
mod pm_tests;
pub mod rally;
pub mod registry;
pub mod research;
pub mod rts_player_save;
pub mod rust_goal;
pub mod script;
pub mod select;
pub mod stats;
pub mod step;
pub mod store;
pub mod tick;
pub mod town_save;
pub mod turret;
pub mod unit_bulk;
pub mod unit_class_fields;
pub mod unit_fields;
pub mod unit_load;
pub mod unit_save;
pub mod unload;
pub mod update;
pub mod uu;
pub mod uu_ffi;
pub mod uu_host;
pub mod uu_place;
pub mod uu_spots;
#[cfg(test)]
mod uu_tests;
pub mod uu_tiles;
pub mod uu_world;
#[cfg(test)]
mod uu_world_tests;
pub mod world;
pub mod world_save;

pub use registry::Registry;
