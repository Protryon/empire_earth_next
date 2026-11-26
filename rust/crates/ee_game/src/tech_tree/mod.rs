//! The tech tree classes' bodies (EEPlayer.h's EETechTree, EETechTreeEpoch and EETechTreeNode call them inline, so
//! their C interface is a header of its own, rust/game/tech_tree.h): the walks' host and the tree's file load, media
//! and cost sums ([`host`], was gamecpp tech.rs), the loads, copies and saved game ([`load`], was gamecpp
//! tech_load.rs). The C++ calls and members they need are one-statement cases of the gamecpp host (`Tb`, `Tk` ops).

// the C++'s own casts (`long` is 32-bit here, 64-bit on LP64), kept as written (as gamecpp's)
#![allow(clippy::unnecessary_cast, clippy::too_many_arguments)]

pub mod host;
pub mod load;
pub mod objects;
