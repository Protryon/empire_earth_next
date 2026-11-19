//! The computer player's leaves (docs/RUST_PLAN.md R5a.8): what of the AI host cannot change how an AI plays.
//!
//! - `next_save`: the next engine's state block of a saved game ("EENS");
//! - `units_save`: the per-unit Lua interface's block ("EENU");
//! - `json`, `manifest`: the AI manifest (`Data/ai/ais.json`) and its checks;
//! - `seat`: the lobby's choices, a choice by name, the resolve of random at the game's start;
//! - `names`: the next engine's name tables (result codes and reasons, events, resources, classes and techs);
//! - `lua_value`: the Lua runtime's value tree (`EECPLuaValue`): the key order, equality, text, the memory's bytes,
//!   the seed and SplitMix64;
//! - `units_lua`: the per-unit bindings' Lua helpers (argument readers, result pushers, the units filter);
//! - `lua_host`: the Lua runtime's state (`EECPLuaState`'s body) over the vendored Lua;
//! - `binding_trace` (TEST ONLY): the trace of the bindings' calls, the port's golden;
//! - `units_state`: the per-unit interface's state (`EECPUnitsState`): the held set, the events, the enemy memory,
//!   the danger layers and the route;
//! - `units_bind` (the game only): its hooks and the `ai.*` bindings over the world views and one C++ host;
//! - `next_state`, `next_value`, `next_game` (the game only): the next engine (EECPNext*.cpp before): its per-player
//!   record, the view and its table, the tick, the army's memory, the gather safety, the strategy step, the save;
//! - `ffi`: the C ABI the C++ (`EECPNextLua.cpp`, `EECPNextUnits.cpp`, `EECPEngine.cpp`, `EECPNext.cpp`) calls.
//!
//! The C++ keeps its objects (the states, the manifest's entries) and hands plain values over. `golden` checks every
//! piece against what the unmodified C++ gave (a scratch driver over the original functions).

pub mod binding_trace;
pub mod ffi;
pub mod json;
// Lua's C API, declared by lua.h: not in the headers
/// cbindgen:ignore
pub mod lua_api;
pub mod lua_host;
pub mod lua_value;
pub mod manifest;
pub mod military;
pub mod names;
pub mod next_game;
pub mod next_save;
pub mod next_state;
pub mod next_value;
pub mod seat;
pub mod switches;
pub mod units_bind;
pub mod units_lua;
pub mod units_save;
pub mod units_state;

#[cfg(test)]
mod golden;
#[cfg(test)]
mod golden_lua;
#[cfg(test)]
mod test_ops;
