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
//! economy, improvements and other members `player`, its statistics `player_stats` and events `player_events`; the turrets' turn `turret`. The goal classes moved whole (one C++ class,
//! `EEUGRust`, every virtual one call) are `rust_goal` (the dispatch and the host) with `patrol`, `calamity`,
//! `explore` and `unload`. The unit utilities (EEFOFindNewTargetNearOldTarget.cpp: the tile rings and directions,
//! the units on tiles, the wall lines, the placement tests) are `uu_tiles` (the pure geometry) and `uu` over its host.

pub mod actions;
pub mod ag;
pub mod ag_fields;
pub mod ag_host;
pub mod ag_sweep;
pub mod ai;
pub mod animate;
pub mod atc;
pub mod atc_save;
pub mod atc_views;
pub mod boarding;
pub mod build;
pub mod calamity;
pub mod citizen;
pub mod citizen_host;
pub mod cls_air;
pub mod cls_answers;
pub mod cls_building;
pub mod cls_carrier;
pub mod cls_carrier_host;
pub mod cls_condition;
pub mod cls_dispatch;
pub mod cls_host;
pub mod cls_level;
pub mod cls_level_host;
pub mod cls_move;
pub mod cls_move_host;
pub mod cls_status;
pub mod cls_status_host;
pub mod cls_transport;
pub mod commands;
pub mod cu_body;
pub mod cu_cargo;
pub mod cu_dump;
pub mod cu_life;
pub mod cu_misc;
pub mod cu_more;
pub mod cu_small;
pub mod cu_wheel;
pub mod cu_world;
pub mod dm_body;
pub mod dm_dop;
pub mod dop_file;
pub mod dop_load;
pub mod dop_more;
pub mod dop_repair;
pub mod dop_save;
pub mod dry;
pub mod eu_body;
pub mod eu_describe;
pub mod eu_hit;
pub mod eu_more;
pub mod explore;
pub mod ffi;
pub mod game_over;
pub mod game_over_host;
pub mod gather;
pub mod gather_spot;
pub mod gather_world;
pub mod ghost_body;
pub mod ghost_make;
pub mod ghost_more;
pub mod goal_calls;
pub mod goal_more;
pub mod goal_obj;
pub mod goal_spec;
pub mod goal_split;
pub mod goals;
pub mod group;
pub mod group_host;
pub mod group_obj;
#[cfg(test)]
mod group_obj_tests;
/// cbindgen:ignore
pub mod group_sweep;
pub mod hack;
mod header_consts;
pub mod hidden;
pub mod host;
pub mod improve;
pub mod improvement;
pub mod kinds;
pub mod landing;
pub mod load;
pub mod location;
pub mod map_save;
pub mod map_words;
pub mod motion;
pub mod msvc_sort;
pub mod notes;
pub mod obj_fields;
pub mod order;
pub mod order_log;
pub mod patrol;
pub mod patrol_group;
pub mod physics;
pub mod physics_save;
pub mod pick_sweep;
pub mod player;
pub mod player_areas;
pub mod player_body;
pub mod player_diplomacy;
pub mod player_econ;
pub mod player_events;
pub mod player_events_split;
pub mod player_lists;
pub mod player_rest;
pub mod player_save;
pub mod player_save_split;
pub mod player_split;
pub mod player_split_more;
pub mod player_stats;
pub mod player_tick;
pub mod player_units;
pub mod player_views;
pub mod players;
pub mod pm;
pub mod pm_air;
pub mod pm_flight;
pub mod pm_ground;
pub mod pm_host;
pub mod pm_note;
#[cfg(test)]
mod pm_tests;
pub mod rally;
pub mod registry;
pub mod research;
pub mod rts_player_save;
pub mod rust_goal;
pub mod save_prim;
pub mod script;
pub mod select;
pub mod selection;
pub mod sight;
pub mod sight_host;
pub mod stats;
pub mod step;
pub mod store;
pub mod tick;
pub mod tile_sight;
pub mod tile_sight_host;
pub mod town_save;
pub mod town_split;
pub mod turret;
pub mod uai_cond;
pub mod uc_anim;
pub mod uc_body;
pub mod uc_class;
pub mod uc_create;
pub mod uc_media;
#[cfg(test)]
mod uc_mock;
pub mod uc_produce;
pub mod uc_rest;
pub mod uca_air;
pub mod ucb_gate;
pub mod ucb_release;
pub mod ucbd_note;
pub mod ucg_ground;
pub mod ucg_sea;
pub mod uck_dispatch;
pub mod uck_kinds;
pub mod ucp_people;
pub mod ucs_impact;
pub mod ucs_shots;
pub mod ucx_transports;
pub mod uh_body;
pub mod unit_bulk;
pub mod unit_class_fields;
pub mod unit_fields;
pub mod unit_geom;
pub mod unit_load;
pub mod unit_save;
pub mod unit_save_views;
pub mod unit_sweep;
pub mod unload;
pub mod upd_entry;
pub mod upd_split;
pub mod update;
pub mod update_views;
pub mod upgrade_media;
pub mod us_check;
pub mod us_fill;
pub mod uu;
pub mod uu_adapt;
pub mod uu_body;
pub mod uu_cloak;
pub mod uu_direct;
pub mod uu_ffi;
pub mod uu_fill;
pub mod uu_gather_tree;
pub mod uu_host;
pub mod uu_line;
pub mod uu_note;
pub mod uu_place;
pub mod uu_reads;
pub mod uu_search;
pub mod uu_spots;
#[cfg(test)]
mod uu_tests;
pub mod uu_tiles;
pub mod uu_world;
pub mod uu_world_ops;
#[cfg(test)]
mod uu_world_tests;
pub mod waypoints;
pub mod world;
pub mod world_save;

pub use registry::Registry;
