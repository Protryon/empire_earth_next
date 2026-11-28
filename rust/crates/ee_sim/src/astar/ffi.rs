//! The C ABI of the tile searches' storage and machine. The C++ storage classes (`EEAIAStarStorage` and the
//! classes over it) own one `EeAStarStorage` each and forward to it; the C++ machine's `Run` calls `ee_astar_start`
//! and `ee_astar_expand` with its goal behind `EeAStarCallbacks`.
//!
//! While `ee_astar_start` / `ee_astar_expand` run, the callbacks must not call any `ee_astar_*` function on the same
//! storage: what the goals read of the storage during a search comes in the callbacks' arguments.

use crate::astar::area_goals::{AreaView, EeAStarBlocker, EeAStarContinentInfo, EeAStarPathAreaInfo};
use crate::astar::goals::View;
use crate::astar::graph::{self, GraphStorage};
use crate::astar::modifiers::{ModView, Skip, Unit};
use crate::astar::path_goals::PathView;
use crate::astar::storage::{Storage, NIL};
pub use ee_base::sparse::EeSparseArray;
use std::ffi::c_void;

/// A node as the goals and the capture see it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarNodeView {
    pub x: i32,
    pub y: i32,
    pub cost: u32,
    pub path_cost: i32,
    pub estimate: i32,
    pub has_parent: bool,
    pub parent_x: i32,
    pub parent_y: i32,
}

pub(crate) fn view(s: &Storage, node: i32) -> EeAStarNodeView {
    let n = s.node(node);
    let (has_parent, parent_x, parent_y) =
        if n.parent != NIL { (true, s.node(n.parent).x, s.node(n.parent).y) } else { (false, 0, 0) };
    EeAStarNodeView {
        x: n.x,
        y: n.y,
        cost: n.cost,
        path_cost: n.path_cost,
        estimate: n.estimate,
        has_parent,
        parent_x,
        parent_y,
    }
}

/// A goal's view (`EEAIAStarView`), as C function pointers over `ctx`: the queries the flood goals make.
#[repr(C)]
pub struct EeAStarViewCallbacks {
    pub ctx: *mut c_void,
    pub has_tile: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> bool,
    pub terrain: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub is_edge: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> bool,
    pub height_level: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub continent_id: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> u16,
    /// a choke tile at `index` (`*serial` its area serial)
    pub choke_tile: extern "C" fn(ctx: *mut c_void, index: i32, serial: *mut u32) -> bool,
    // the modifiers' queries (`modifiers::ModView`)
    pub ai_value: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub tile_flag: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub can_move_on: extern "C" fn(ctx: *mut c_void, walker: Unit, terrain: i32) -> bool,
    pub are_passable: extern "C" fn(ctx: *mut c_void, walker: Unit, x: i32, y: i32, skip: Skip) -> bool,
    pub formation_rank: extern "C" fn(ctx: *mut c_void, walker: Unit) -> i32,
    pub step_blocker: extern "C" fn(ctx: *mut c_void, walker: Unit, sea: bool, x: i32, y: i32) -> bool,
    pub unit_region_state: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub epoch: extern "C" fn(ctx: *mut c_void, walker: Unit) -> i32,
    pub is_alone: extern "C" fn(ctx: *mut c_void, walker: Unit) -> bool,
    pub water_state: extern "C" fn(ctx: *mut c_void, walker: Unit) -> i32,
    pub is_sea_going: extern "C" fn(ctx: *mut c_void, walker: Unit) -> bool,
    pub has_hill: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> bool,
    pub random_at_most: extern "C" fn(ctx: *mut c_void, chance: f32) -> bool,
    // the path goals' queries (`path_goals::PathView`)
    pub crowd_cost: extern "C" fn(ctx: *mut c_void, walker: Unit, x: i32, y: i32) -> i32,
    pub is_wide: extern "C" fn(ctx: *mut c_void, walker: Unit) -> bool,
    pub ignores_formation: extern "C" fn(ctx: *mut c_void, walker: Unit) -> bool,
    pub rank_probe_blocked: extern "C" fn(ctx: *mut c_void, walker: Unit, continent_id: u16, x: i32, y: i32) -> bool,
    pub can_enter: extern "C" fn(ctx: *mut c_void, modifier: *mut c_void, x: i32, y: i32, at_goal: bool) -> bool,
    #[allow(clippy::type_complexity)]
    pub modifier_cost: extern "C" fn(
        ctx: *mut c_void,
        modifier: *mut c_void,
        from_x: i32,
        from_y: i32,
        x: i32,
        y: i32,
        parent_x: i32,
        parent_y: i32,
    ) -> i32,
    // the area fills' queries (`area_goals::AreaView`)
    pub has_units: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> bool,
    pub continent_at: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, out: *mut EeAStarContinentInfo),
    pub tile_value: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub path_area_at: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, out: *mut EeAStarPathAreaInfo),
    pub path_area: extern "C" fn(ctx: *mut c_void, id: u16, out: *mut EeAStarPathAreaInfo),
    pub stored_path_area_id: extern "C" fn(ctx: *mut c_void, storage: *const c_void, x: i32, y: i32) -> u16,
    pub resource_kind: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub has_blocker: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> bool,
    pub first_blocker_id: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub first_blocker: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, out: *mut EeAStarBlocker),
    pub first_obstacle_id: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> i32,
    pub own_wall_first: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, player_id: i32) -> bool,
    pub area_gate_owner: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, player_id: i32) -> i32,
    pub sector_gate_owner: extern "C" fn(ctx: *mut c_void, x: i32, y: i32, player_id: i32) -> i32,
    pub live_dock: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> bool,
    pub live_space_dock: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> bool,
}

pub(crate) struct CView<'a>(pub(crate) &'a EeAStarViewCallbacks);

impl View for CView<'_> {
    fn has_tile(&mut self, x: i32, y: i32) -> bool {
        (self.0.has_tile)(self.0.ctx, x, y)
    }
    fn terrain(&mut self, x: i32, y: i32) -> i32 {
        (self.0.terrain)(self.0.ctx, x, y)
    }
    fn is_edge(&mut self, x: i32, y: i32) -> bool {
        (self.0.is_edge)(self.0.ctx, x, y)
    }
    fn height_level(&mut self, x: i32, y: i32) -> i32 {
        (self.0.height_level)(self.0.ctx, x, y)
    }
    fn continent_id(&mut self, x: i32, y: i32) -> u16 {
        (self.0.continent_id)(self.0.ctx, x, y)
    }
    fn choke_tile(&mut self, index: i32) -> Option<u32> {
        let mut serial = 0u32;
        (self.0.choke_tile)(self.0.ctx, index, &mut serial).then_some(serial)
    }
}

impl ModView for CView<'_> {
    fn ai_value(&mut self, x: i32, y: i32) -> i32 {
        (self.0.ai_value)(self.0.ctx, x, y)
    }
    fn tile_flag(&mut self, x: i32, y: i32) -> i32 {
        (self.0.tile_flag)(self.0.ctx, x, y)
    }
    fn can_move_on(&mut self, walker: Unit, terrain: i32) -> bool {
        (self.0.can_move_on)(self.0.ctx, walker, terrain)
    }
    fn are_passable(&mut self, walker: Unit, x: i32, y: i32, skip: Skip) -> bool {
        (self.0.are_passable)(self.0.ctx, walker, x, y, skip)
    }
    fn formation_rank(&mut self, walker: Unit) -> i32 {
        (self.0.formation_rank)(self.0.ctx, walker)
    }
    fn step_blocker(&mut self, walker: Unit, sea: bool, x: i32, y: i32) -> bool {
        (self.0.step_blocker)(self.0.ctx, walker, sea, x, y)
    }
    fn unit_region_state(&mut self, x: i32, y: i32) -> i32 {
        (self.0.unit_region_state)(self.0.ctx, x, y)
    }
    fn epoch(&mut self, walker: Unit) -> i32 {
        (self.0.epoch)(self.0.ctx, walker)
    }
    fn is_alone(&mut self, walker: Unit) -> bool {
        (self.0.is_alone)(self.0.ctx, walker)
    }
    fn water_state(&mut self, walker: Unit) -> i32 {
        (self.0.water_state)(self.0.ctx, walker)
    }
    fn is_sea_going(&mut self, walker: Unit) -> bool {
        (self.0.is_sea_going)(self.0.ctx, walker)
    }
    fn has_hill(&mut self, x: i32, y: i32) -> bool {
        (self.0.has_hill)(self.0.ctx, x, y)
    }
    fn random_at_most(&mut self, chance: f32) -> bool {
        (self.0.random_at_most)(self.0.ctx, chance)
    }
}

impl AreaView for CView<'_> {
    fn has_units(&mut self, x: i32, y: i32) -> bool {
        (self.0.has_units)(self.0.ctx, x, y)
    }
    fn continent_at(&mut self, x: i32, y: i32) -> EeAStarContinentInfo {
        let mut out = EeAStarContinentInfo::default();
        (self.0.continent_at)(self.0.ctx, x, y, &mut out);
        out
    }
    fn tile_value(&mut self, x: i32, y: i32) -> i32 {
        (self.0.tile_value)(self.0.ctx, x, y)
    }
    fn path_area_at(&mut self, x: i32, y: i32) -> EeAStarPathAreaInfo {
        let mut out = EeAStarPathAreaInfo::default();
        (self.0.path_area_at)(self.0.ctx, x, y, &mut out);
        out
    }
    fn path_area(&mut self, id: u16) -> EeAStarPathAreaInfo {
        let mut out = EeAStarPathAreaInfo::default();
        (self.0.path_area)(self.0.ctx, id, &mut out);
        out
    }
    fn stored_path_area_id(&mut self, storage: *const c_void, x: i32, y: i32) -> u16 {
        (self.0.stored_path_area_id)(self.0.ctx, storage, x, y)
    }
    fn resource_kind(&mut self, x: i32, y: i32) -> i32 {
        (self.0.resource_kind)(self.0.ctx, x, y)
    }
    fn has_blocker(&mut self, x: i32, y: i32) -> bool {
        (self.0.has_blocker)(self.0.ctx, x, y)
    }
    fn first_blocker_id(&mut self, x: i32, y: i32) -> i32 {
        (self.0.first_blocker_id)(self.0.ctx, x, y)
    }
    fn first_blocker(&mut self, x: i32, y: i32) -> EeAStarBlocker {
        let mut out = EeAStarBlocker::default();
        (self.0.first_blocker)(self.0.ctx, x, y, &mut out);
        out
    }
    fn first_obstacle_id(&mut self, x: i32, y: i32) -> i32 {
        (self.0.first_obstacle_id)(self.0.ctx, x, y)
    }
    fn own_wall_first(&mut self, x: i32, y: i32, player_id: i32) -> bool {
        (self.0.own_wall_first)(self.0.ctx, x, y, player_id)
    }
    fn area_gate_owner(&mut self, x: i32, y: i32, player_id: i32) -> i32 {
        (self.0.area_gate_owner)(self.0.ctx, x, y, player_id)
    }
    fn sector_gate_owner(&mut self, x: i32, y: i32, player_id: i32) -> i32 {
        (self.0.sector_gate_owner)(self.0.ctx, x, y, player_id)
    }
    fn live_dock(&mut self, x: i32, y: i32) -> bool {
        (self.0.live_dock)(self.0.ctx, x, y)
    }
    fn live_space_dock(&mut self, x: i32, y: i32) -> bool {
        (self.0.live_space_dock)(self.0.ctx, x, y)
    }
}

impl PathView for CView<'_> {
    fn crowd_cost(&mut self, walker: Unit, x: i32, y: i32) -> i32 {
        (self.0.crowd_cost)(self.0.ctx, walker, x, y)
    }
    fn is_wide(&mut self, walker: Unit) -> bool {
        (self.0.is_wide)(self.0.ctx, walker)
    }
    fn ignores_formation(&mut self, walker: Unit) -> bool {
        (self.0.ignores_formation)(self.0.ctx, walker)
    }
    fn rank_probe_blocked(&mut self, walker: Unit, continent_id: u16, x: i32, y: i32) -> bool {
        (self.0.rank_probe_blocked)(self.0.ctx, walker, continent_id, x, y)
    }
    fn can_enter(&mut self, modifier: *mut c_void, x: i32, y: i32, at_goal: bool) -> bool {
        (self.0.can_enter)(self.0.ctx, modifier, x, y, at_goal)
    }
    fn modifier_cost(
        &mut self,
        modifier: *mut c_void,
        from_x: i32,
        from_y: i32,
        x: i32,
        y: i32,
        px: i32,
        py: i32,
    ) -> i32 {
        (self.0.modifier_cost)(self.0.ctx, modifier, from_x, from_y, x, y, px, py)
    }
}

// (the modifiers' slots: modifier_obj.rs, over modifiers.rs)

/// A graph node as the capture sees it (`EEAIAStarGraphStorage::CaptureNodes`): the parent's index in `parent_x`
/// and the node's own index in `parent_y`.
pub(crate) fn graph_view(s: &GraphStorage, node: i32) -> EeAStarNodeView {
    let n = s.node(node);
    EeAStarNodeView {
        x: n.x,
        y: n.y,
        cost: n.cost,
        path_cost: n.path_cost,
        estimate: n.estimate,
        has_parent: n.parent != graph::NIL,
        parent_x: if n.parent != graph::NIL { s.node(n.parent).index } else { 0 },
        parent_y: n.index,
    }
}

use crate::astar::continent_grid::{self, ContinentGrid, EeContinentNeighbor, EeContinentShore, EePoint, FillTile};
use crate::astar::sectors::{self, EeSectorRect, EeSectorSizes};
use crate::astar::sub_sector_links::{self, EeSubSectorLink};
use crate::astar::tile_values;

/// What `CreateContinentGrid` computed (opaque to C++; `ee_astar_continent_grid_free` releases it).
pub struct EeContinentGrid(ContinentGrid);

/// `EEAIContinentManager::CreateContinentGrid` over a `size` x `size` map: `ids` and `kinds` the continent fill's
/// tiles (`x * size + y`), `terrain` every tile's terrain type (-1 where the map has none), `categories` the
/// continents' categories by id (`next_id` of them).
///
/// # Safety
/// `ids`, `kinds` and `terrain` valid for `size * size` cells, `categories` for `next_id`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_grid_build(
    size: i32,
    ids: *const u16,
    kinds: *const u8,
    terrain: *const i32,
    categories: *const u8,
    next_id: i32,
) -> *mut EeContinentGrid {
    let n = (size.max(0) as usize) * (size.max(0) as usize);
    // SAFETY: the caller's contract
    let (ids, kinds, terrain, categories) = unsafe {
        (
            std::slice::from_raw_parts(ids, n),
            std::slice::from_raw_parts(kinds, n),
            std::slice::from_raw_parts(terrain, n),
            std::slice::from_raw_parts(categories, next_id.max(0) as usize),
        )
    };
    let fill: Vec<FillTile> = ids.iter().zip(kinds).map(|(&id, &kind)| FillTile { id, kind }).collect();
    let grid = continent_grid::create_continent_grid(size, &fill, terrain, categories, next_id);
    Box::into_raw(Box::new(EeContinentGrid(grid)))
}

/// Releases a grid.
///
/// # Safety
/// `g` from `ee_astar_continent_grid_build`, not freed yet.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_grid_free(g: *mut EeContinentGrid) {
    if !g.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(g) });
    }
}

/// The tile-to-continent id grid (`size * size`, `x * size + y`).
///
/// # Safety
/// `g` from `ee_astar_continent_grid_build`; the pointer lives while `g` does.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_grid_ids(g: *const EeContinentGrid, count: *mut usize) -> *const u16 {
    let v = &unsafe { &*g }.0.ids;
    unsafe { *count = v.len() };
    v.as_ptr()
}

/// The adjacency matrix (`next_id * next_id`, `[a * next_id + b]`, 0 or 1).
///
/// # Safety
/// As `ee_astar_continent_grid_ids`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_grid_flags(g: *const EeContinentGrid, count: *mut usize) -> *const u8 {
    let v = &unsafe { &*g }.0.flags;
    unsafe { *count = v.len() };
    v.as_ptr()
}

/// The continents' neighbour pushes, in order.
///
/// # Safety
/// As `ee_astar_continent_grid_ids`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_grid_neighbors(
    g: *const EeContinentGrid,
    count: *mut usize,
) -> *const EeContinentNeighbor {
    let v = &unsafe { &*g }.0.neighbors;
    unsafe { *count = v.len() };
    v.as_ptr()
}

/// The manager's shore tiles, sorted and unique.
///
/// # Safety
/// As `ee_astar_continent_grid_ids`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_grid_shore(g: *const EeContinentGrid, count: *mut usize) -> *const EePoint {
    let v = &unsafe { &*g }.0.shore;
    unsafe { *count = v.len() };
    v.as_ptr()
}

/// The manager's ice shore tiles, sorted.
///
/// # Safety
/// As `ee_astar_continent_grid_ids`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_grid_ice_shore(
    g: *const EeContinentGrid,
    count: *mut usize,
) -> *const EePoint {
    let v = &unsafe { &*g }.0.ice_shore;
    unsafe { *count = v.len() };
    v.as_ptr()
}

/// Every continent's shore tiles (sorted and unique per continent), grouped by continent id.
///
/// # Safety
/// As `ee_astar_continent_grid_ids`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_grid_continent_shore(
    g: *const EeContinentGrid,
    count: *mut usize,
) -> *const EeContinentShore {
    let v = &unsafe { &*g }.0.continent_shore;
    unsafe { *count = v.len() };
    v.as_ptr()
}

// ---- the continent manager's tile values (tile_values.rs) ----

/// `EEAIContinentManager::UpdateTileValues` over a `size` x `size` map: `terrain` every tile's terrain type (-1
/// where the map has none), `shore` and `ice_shore` the manager's sorted lists, `weights` every tile's
/// `GetResourceWeight` (read only when `water` is false). `values` gets every tile's new value (0: unchanged),
/// `bytes` the AI value to store with a changed one.
///
/// # Safety
/// `terrain`, `values` and `bytes` valid for `size * size` cells, `weights` too unless `water`; `shore` and
/// `ice_shore` for their counts.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_update_tile_values(
    size: i32,
    terrain: *const i32,
    water: bool,
    shore: *const EePoint,
    shore_count: usize,
    ice_shore: *const EePoint,
    ice_count: usize,
    weights: *const i32,
    values: *mut i32,
    bytes: *mut i8,
) {
    let n = (size.max(0) as usize) * (size.max(0) as usize);
    // SAFETY: the caller's contract
    let (terrain, shore, ice_shore, weights, values, bytes) = unsafe {
        (
            std::slice::from_raw_parts(terrain, n),
            std::slice::from_raw_parts(shore, shore_count),
            std::slice::from_raw_parts(ice_shore, ice_count),
            std::slice::from_raw_parts(weights, if water { 0 } else { n }),
            std::slice::from_raw_parts_mut(values, n),
            std::slice::from_raw_parts_mut(bytes, n),
        )
    };
    let world = tile_values::TileWorld { size, terrain, water, shore, ice_shore, weights };
    tile_values::update_tile_values(&world, values, bytes);
}

// ---- the choke point pass's grids (choke.rs) ----

// (the pass's grids are Rust's: choke.rs `ee_astar_choke_grids_*`)

// ---- the sub sector links (sub_sector_links.rs) ----

/// The sub sectors as the link walk sees them: C function pointers over `ctx`.
#[repr(C)]
pub struct EeSubSectorGridCallbacks {
    pub ctx: *mut c_void,
    /// the sub sector id at a tile (0 none)
    pub id_at: extern "C" fn(ctx: *mut c_void, x: i32, y: i32) -> u16,
    /// whether the manager lists a sub sector under `id`
    pub present: extern "C" fn(ctx: *mut c_void, id: u16) -> bool,
}

struct CSubSectorGrid<'a>(&'a EeSubSectorGridCallbacks);

impl sub_sector_links::Grid for CSubSectorGrid<'_> {
    fn id_at(&mut self, x: i32, y: i32) -> u16 {
        (self.0.id_at)(self.0.ctx, x, y)
    }
    fn present(&mut self, id: u16) -> bool {
        (self.0.present)(self.0.ctx, id)
    }
}

/// The `LinkSubSectors` calls of a walk (opaque to C++; `ee_astar_sub_sector_links_free` releases it).
pub struct EeSubSectorLinks(Vec<EeSubSectorLink>);

/// The link walk over `x0..x_end` x `y0..y_end` (`LinkSubSectorGrid` over the whole map, `RelinkSubSectors` over a
/// grown sector): the `LinkSubSectors` calls to make, in order.
///
/// # Safety
/// `cb` valid, its callbacks safe to call for the tiles of the walk and any id.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sub_sector_links(
    x0: i32,
    x_end: i32,
    y0: i32,
    y_end: i32,
    cb: *const EeSubSectorGridCallbacks,
) -> *mut EeSubSectorLinks {
    let mut out = Vec::new();
    sub_sector_links::link_walk(&mut CSubSectorGrid(unsafe { &*cb }), x0, x_end, y0, y_end, &mut out);
    Box::into_raw(Box::new(EeSubSectorLinks(out)))
}

/// The calls of a walk.
///
/// # Safety
/// `links` from `ee_astar_sub_sector_links`; the pointer lives while `links` does.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sub_sector_links_get(
    links: *const EeSubSectorLinks,
    count: *mut usize,
) -> *const EeSubSectorLink {
    let v = &unsafe { &*links }.0;
    unsafe { *count = v.len() };
    v.as_ptr()
}

/// Releases a walk's calls.
///
/// # Safety
/// `links` from `ee_astar_sub_sector_links`, not freed yet.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sub_sector_links_free(links: *mut EeSubSectorLinks) {
    if !links.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(links) });
    }
}

// ---- the path sector grid (sectors.rs) ----

/// `CreatePathSectors`' sector and region sizes by the map's size class.
#[no_mangle]
pub extern "C" fn ee_astar_sector_sizes(size_class: i32) -> EeSectorSizes {
    sectors::sector_sizes(size_class)
}

/// `Build`'s number of choke point passes by the map's size class.
#[no_mangle]
pub extern "C" fn ee_astar_choke_passes(size_class: i32) -> i32 {
    sectors::choke_passes(size_class)
}

/// `CreatePathSectors`' rectangle of sector `(x, y)`.
#[no_mangle]
pub extern "C" fn ee_astar_sector_rect(x: i32, y: i32, sector_size: i32, map_size: i32) -> EeSectorRect {
    sectors::sector_rect(x, y, sector_size, map_size)
}

// ---- a tile's AI kind and wall id (tile_kind.rs) ----

use crate::astar::tile_kind::EeTileUnit;

/// # Safety
/// `units` valid for `count` records unless `count` is 0.
unsafe fn unit_slice<'a>(units: *const EeTileUnit, count: usize) -> &'a [EeTileUnit] {
    if count == 0 || units.is_null() {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(units, count) }
    }
}

// ---- the region fills' start tests (path_area_kind.rs) ----

use crate::astar::path_area_kind::{self, EeMapEdges, EePathAreaIDs};

/// A fill's `mCheckCorners` at `(x, y)`: not of terrain `no_corners` and off the map's diagonal edges.
#[no_mangle]
pub extern "C" fn ee_astar_check_corners(terrain: i32, no_corners: i32, x: i32, y: i32, edges: EeMapEdges) -> bool {
    path_area_kind::check_corners(terrain, no_corners, x, y, &edges)
}

/// `CreatePathAreaKind`'s test of tile `(x, y)`: whether a fill of `kind` starts there; `flag` is the loop's
/// `theFlag`, read and updated.
///
/// # Safety
/// `area` and `flag` valid; `units` valid for `count` records (any pointer when `count` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_path_area_kind_starts(
    kind: i32,
    terrain: i32,
    x: i32,
    y: i32,
    edges: EeMapEdges,
    area: *const EePathAreaIDs,
    units: *const EeTileUnit,
    count: usize,
    flag: *mut bool,
) -> bool {
    // SAFETY: the caller's contract
    let (area, units, flag) = unsafe { (&*area, unit_slice(units, count), &mut *flag) };
    path_area_kind::starts_fill(kind, terrain, x, y, &edges, area, units, flag)
}

// ---- the path area repair (repair.rs) ----

// ---- the manager's queries (queries.rs) ----

// ---- the manager's last loops (manager.rs) ----

/// What `PruneChokePoints` asks the C++ manager (`manager::PruneHost`), as C function pointers over `ctx`.
#[repr(C)]
pub struct EePruneCallbacks {
    pub ctx: *mut c_void,
    pub choke_exists: extern "C" fn(*mut c_void, usize) -> bool,
    pub area_count: extern "C" fn(*mut c_void, usize) -> usize,
    pub area: extern "C" fn(*mut c_void, usize, usize) -> i32,
    /// `k` -1 for the choke point itself
    pub is_near: extern "C" fn(*mut c_void, usize, usize, isize) -> bool,
    pub remove: extern "C" fn(*mut c_void, usize),
}

// ---- the path manager's splicing (path_splice.rs) ----

// ---- the AI's hulls (hull.rs): U2DPolygon<long> / U2DConvexHull<long>'s algorithms over the C++ class's points

use crate::astar::hull::EeHullPoint;
use std::ffi::c_long;

// ---- the areas' geometry (area_geom.rs): EEAIArea's and EEAIContinent's, over the C++ objects' points

use crate::astar::area_geom::ShoreHost;

/// What `EEAIContinent::GetShoreInRegions` asks the continent manager (`area_geom::ShoreHost`), over `ctx`.
#[repr(C)]
pub struct EeShoreCallbacks {
    pub ctx: *mut c_void,
    /// the path area at the tile: false without one, else true and its region id of the kind in `id`
    pub path_area_region: extern "C" fn(*mut c_void, c_long, c_long, bool, *mut u16) -> bool,
    /// the continent at the tile exists and is land
    pub is_land: extern "C" fn(*mut c_void, c_long, c_long) -> bool,
    /// `GetContinent(tile, kind)`
    pub region: extern "C" fn(*mut c_void, c_long, c_long, bool) -> u16,
    pub map_width: u32,
    pub map_height: u32,
}

impl ShoreHost for &EeShoreCallbacks {
    fn path_area_region(&mut self, tile: EeHullPoint, space: bool) -> Option<u16> {
        let mut id = 0u16;
        (self.path_area_region)(self.ctx, tile.x, tile.y, space, &mut id).then_some(id)
    }
    fn is_land(&mut self, tile: EeHullPoint) -> bool {
        (self.is_land)(self.ctx, tile.x, tile.y)
    }
    fn region(&mut self, tile: EeHullPoint, space: bool) -> u16 {
        (self.region)(self.ctx, tile.x, tile.y, space)
    }
    fn map_size(&self) -> (u32, u32) {
        (self.map_width, self.map_height)
    }
}
