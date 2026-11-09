//! The AI's tile A* searches (`src/Empire Earth/AI Thread/EEAIAStar*`): the node storage and the search machine,
//! ported line by line from the C++ (docs/RUST_PLAN.md R5b.1). The goals stay C++ for now: the machine asks them
//! everything through `machine::Host` (in C, `ffi::EeAStarCallbacks`) in the C++'s order, and the goals ask the world
//! through their view (`ee/EEAIAStarView.h`).
//!
//! Part of the deterministic game (RUST_PLAN 2.1): integer arithmetic only, `long` as `i32` (the Windows build's;
//! wrapping where the C++ wraps), the lists' orders and the tie-breaking exactly the C++'s. The gate is
//! `tools/sim_capture.py`'s A* records: every search of the six oracle games, its goal calls and view answers and its
//! storage afterwards, digested and compared with the digests of the C++ machine.

pub mod ffi;
pub mod machine;
pub mod storage;

#[cfg(test)]
mod tests;
