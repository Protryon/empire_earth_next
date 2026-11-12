//! The AI's tile A* searches (`src/Empire Earth/AI Thread/EEAIAStar*`): the node storage and the search machine,
//! ported line by line from the C++ (docs/RUST_PLAN.md R5b.1). The goals stay C++ for now: the machine asks them
//! everything through `machine::Host` (in C, `ffi::EeAStarCallbacks`) in the C++'s order, and the goals ask the world
//! through their view (`ee/EEAIAStarView.h`).
//!
//! The flood goals' tile tests are Rust too (`goals`): the continent, hill and choke point fills, asking their
//! view through `ffi::EeAStarViewCallbacks`. So are the unit path goals' hooks (`path_goals`: the land, sea and
//! space searches and the bounded one), their modifiers' costs and tile tests (`modifiers`), and the path area and
//! path sector fills' tile tests (`area_goals`).
//!
//! The graph searches (`EEAIAStarGraphMachine` over `EEAIAStarGraphStorage`: the sub sector paths and the
//! continental chains) are `graph`: their node storage and machine, the goals and route planners staying C++ behind
//! `ffi::EeAStarGraphCallbacks`.
//!
//! The continent manager's analysis, leaf by leaf (the C++ manager keeps its objects and stores what these
//! compute): `choke` is the choke point pass's classification and smoothing, `continent_grid` the grid, adjacency
//! and shore tiles from the continent fill, `tile_values` the tile values, `sub_sector_links` the sub sector link
//! walks, `sectors` the path sector grid's sizes and rectangles, `tile_kind` a tile's AI kind and wall id,
//! `path_area_kind` the region fills' start tests, `repair` the path area repair's choice and region id lookup, `queries` the
//! manager's chain, adjacency and largest continent queries.
//!
//! The engine's sparse point set (`U2DSparseArrayPointContainer`: the areas' and sub sectors' tiles) is `sparse`,
//! whole (the C++ class owns a handle). `manager` is the manager's last loops over its objects (`GetTilesNextTo`,
//! `PruneChokePoints` with the geometry behind a host, `CreateScenarioAreas`' choice), `path_splice` the path
//! manager's path splicing and step budgets. `hull` is the AI's hulls (`U2DPolygon<long>`/`U2DConvexHull<long>`:
//! every area's outline, on `ee_ge::hull`'s convex hull, which the engine's float hull shares), `area_geom` the
//! areas' geometry on them (`EEAIArea`'s and `EEAIContinent`'s closest points, hull point, tiles and shore walk).
//!
//! Part of the deterministic game (RUST_PLAN 2.1): integer arithmetic, `long` as `i32` (the Windows build's;
//! wrapping where the C++ wraps; the hulls keep `c_long`, `U2DPoint<long>`'s layout, and the exe's `float` steps in
//! its order), the lists' orders and the tie-breaking exactly the C++'s. The gate is
//! `tools/sim_capture.py`'s A* records: every search of the six oracle games, its goal calls and view answers and its
//! storage afterwards, digested and compared with the digests of the C++ machine.

pub mod area_geom;
pub mod area_goals;
pub mod choke;
pub mod continent_grid;
pub mod ffi;
pub mod goals;
pub mod graph;
pub mod graph_save;
pub mod hull;
pub mod machine;
pub mod manager;
pub mod modifiers;
pub mod path_area_kind;
pub mod path_goals;
pub mod path_splice;
pub mod queries;
pub mod repair;
pub mod sectors;
pub mod sparse;
pub mod storage;
pub mod storage_save;
pub mod sub_sector_links;
pub mod tile_kind;
pub mod tile_values;

#[cfg(test)]
mod tests;
#[cfg(test)]
mod tests_areas;
#[cfg(test)]
mod tests_graph;
#[cfg(test)]
mod tests_paths;
