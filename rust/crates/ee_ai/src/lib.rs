//! The computer player's leaves (docs/RUST_PLAN.md R5a.8): what of the AI host cannot change how an AI plays.
//!
//! - `next_save`: the next engine's state block of a saved game ("EENS");
//! - `units_save`: the per-unit Lua interface's block ("EENU");
//! - `json`, `manifest`: the AI manifest (`Data/ai/ais.json`) and its checks;
//! - `seat`: the lobby's choices, a choice by name, the resolve of random at the game's start;
//! - `names`: the next engine's name tables (result codes and reasons, events, resources, classes and techs);
//! - `lua_value`: the Lua runtime's value tree (`EECPLuaValue`): the key order, equality, text, the memory's bytes,
//!   the seed and SplitMix64;
//! - `ffi`: the C ABI the C++ (`EECPNextLua.cpp`, `EECPNextUnits.cpp`, `EECPEngine.cpp`, `EECPNext.cpp`) calls.
//!
//! The C++ keeps its objects (the states, the manifest's entries) and hands plain values over. `golden` checks every
//! piece against what the unmodified C++ gave (a scratch driver over the original functions).

pub mod ffi;
pub mod json;
pub mod lua_ffi;
pub mod lua_value;
pub mod manifest;
pub mod names;
pub mod next_save;
pub mod seat;
pub mod units_save;

#[cfg(test)]
mod golden;
#[cfg(test)]
mod golden_lua;
#[cfg(test)]
mod test_ops;
