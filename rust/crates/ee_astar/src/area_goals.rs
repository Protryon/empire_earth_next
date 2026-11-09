//! The path area and path sector flood goals' tile tests (R5b.1 step 4): `EEAIAStarPathAreaGoal::AcceptTile` with
//! its `ClassifyTile` (0x417bcb, 0x4176c7) and `EEAIAStarPathSectorGoal::AcceptTile` with its `ClassifyTile`
//! (0x41acbf, 0x41ae1d), line by line. They ask the world only through `AreaView` (in C, `ffi::EeAStarViewCallbacks`
//! over the goal's `EEAIAStarView`), in the C++'s order, and update the fill state the C++ keeps in its members (the
//! caller copies it in and out). What they did to others comes back as out fields: the machine's `mCheckCorners`
//! (written through `dynamic_cast<EEAIAStarMachineBase*>(mStorage)`), and the sector fill's stale tile.

// the C++'s shape (its nested tests and late results) is kept line by line
#![allow(clippy::collapsible_if, clippy::collapsible_match, clippy::needless_late_init)]

use crate::goals::{TERRAIN_CLIFF, TERRAIN_DEEP_WATER, TERRAIN_SPACE};
use crate::modifiers::{contains, ModView, TERRAIN_SHALLOW_WATER};
use crate::storage::AStarRect;
use std::ffi::c_void;

/// `EEAIContinentCategory`.
const CATEGORY_LAND: u8 = 1;
const CATEGORY_WATER: u8 = 2;
const CATEGORY_CLIFF: u8 = 3;
const CATEGORY_SPACE: u8 = 4;
const CATEGORY_MAP_EDGE: u8 = 5;

/// `EEAITileKind` (`ee/EEAIKindIDs.h`).
pub(crate) const KIND_LAND: i32 = 1;
pub(crate) const KIND_WATER: i32 = 2;
pub(crate) const KIND_CLIFF: i32 = 3;
pub(crate) const KIND_FOREST: i32 = 4;
pub(crate) const KIND_GAIA_OBSTACLE: i32 = 5;
pub(crate) const KIND_MAP_EDGE: i32 = 6;
pub(crate) const KIND_SHALLOWS: i32 = 7;
pub(crate) const KIND_SHALLOWS_SHORE: i32 = 8;
pub(crate) const KIND_SHALLOWS_NEAR_SHORE: i32 = 9;
pub(crate) const KIND_SHORE: i32 = 10;
pub(crate) const KIND_NEAR_SHORE: i32 = 11;
pub(crate) const KIND_UNIT_OBSTACLE: i32 = 0xc;
pub(crate) const KIND_WALL: i32 = 0xd;
pub(crate) const KIND_GATE: i32 = 0xe;
pub(crate) const KIND_SPACE: i32 = 0xf;

/// `EEAIPathKind`: the path area fill's passes.
pub(crate) const PATH_NONE: i32 = 0;
pub(crate) const PATH_NON_SPACE: i32 = 4;

/// A continent at a tile (`EEAIAStarContinentInfo`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarContinentInfo {
    pub id: u16,
    pub category: u8,
}

/// A path area at a tile or by id (`EEAIAStarPathAreaInfo`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarPathAreaInfo {
    pub found: bool,
    pub id: u16,
    pub continent_id: u16,
    pub kind: i32,
}

/// The first blocking unit of a tile (`EEAIAStarBlocker`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarBlocker {
    pub found: bool,
    pub unit_id: i32,
    pub player_id: i32,
    pub is_gate: bool,
    pub is_wall: bool,
}

/// The area fills' view queries (`ee/EEAIAStarView.h`).
pub trait AreaView: ModView {
    fn has_units(&mut self, x: i32, y: i32) -> bool;
    fn continent_at(&mut self, x: i32, y: i32) -> EeAStarContinentInfo;
    fn tile_value(&mut self, x: i32, y: i32) -> i32;
    fn path_area_at(&mut self, x: i32, y: i32) -> EeAStarPathAreaInfo;
    fn path_area(&mut self, id: u16) -> EeAStarPathAreaInfo;
    /// the path area fill's id grid (`storage`: that fill's `EEAIAStarPathAreaStorage`)
    fn stored_path_area_id(&mut self, storage: *const c_void, x: i32, y: i32) -> u16;
    /// `EEAIHasResourceUnit`: 0, or the resource kind 1..3
    fn resource_kind(&mut self, x: i32, y: i32) -> i32;
    fn has_blocker(&mut self, x: i32, y: i32) -> bool;
    fn first_blocker_id(&mut self, x: i32, y: i32) -> i32;
    fn first_blocker(&mut self, x: i32, y: i32) -> EeAStarBlocker;
    fn first_obstacle_id(&mut self, x: i32, y: i32) -> i32;
    fn own_wall_first(&mut self, x: i32, y: i32, player_id: i32) -> bool;
    fn area_gate_owner(&mut self, x: i32, y: i32, player_id: i32) -> i32;
    fn sector_gate_owner(&mut self, x: i32, y: i32, player_id: i32) -> i32;
    fn live_dock(&mut self, x: i32, y: i32) -> bool;
    fn live_space_dock(&mut self, x: i32, y: i32) -> bool;
}

fn point(x: i32, y: i32) -> AStarRect {
    AStarRect { bottom: y, left: x, right: x, top: y }
}

/// Counts a resource tile of kind 1..3.
fn count_resource(resources: &mut [i32; 3], kind: i32) {
    if (1..=3).contains(&kind) {
        let r = &mut resources[(kind - 1) as usize];
        *r = r.wrapping_add(1);
    }
}

/// The shore kinds of a tile value: below 0 `shore`, above 0 `near`, else `flat`.
fn by_value(value: i32, shore: i32, near: i32, flat: i32) -> i32 {
    if value < 0 {
        shore
    } else if value > 0 {
        near
    } else {
        flat
    }
}

/// `EEAIAStarPathAreaGoal`'s fill state.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarPathAreaFill {
    /// `mMode` (`EEAIPathKind`: 0 the path area pass, 1..4 a kind pass)
    pub mode: i32,
    /// `mContinentID`, `mContinentKind` (the first tile's continent; 0 between areas)
    pub continent_id: u16,
    pub continent_kind: u8,
    /// `mKind` (`EEAITileKind`)
    pub kind: i32,
    /// `mOwnerID`, `mPlayerID` (the first tile's blocking unit)
    pub owner_id: i32,
    pub player_id: i32,
    /// `mTileCount`
    pub tile_count: i32,
    /// `mResources1..3`
    pub resources: [i32; 3],
    /// `mForestID`, `mNextForestID`
    pub forest_id: u16,
    pub next_forest_id: u16,
    /// `mTileBounds`
    pub tile_bounds: AStarRect,
    /// out: the machine's `mCheckCorners` as the fill left it (-1: not written)
    pub check_corners: i32,
}

/// 0x4176c7 (`ClassifyTile`): whether a kind pass takes the tile; kind 0 (`*kind`) picks the kind from the first tile.
/// Water and space tiles under units pass only for working docks and space docks.
pub fn path_area_classify<V: AreaView + ?Sized>(
    fill: &EeAStarPathAreaFill,
    view: &mut V,
    x: i32,
    y: i32,
    kind: &mut u8,
    shallows_are_water: bool,
) -> bool {
    let terrain = view.terrain(x, y);
    match *kind {
        0 => {
            *kind = match terrain {
                TERRAIN_DEEP_WATER => {
                    if fill.mode != PATH_NON_SPACE {
                        2
                    } else {
                        1
                    }
                }
                TERRAIN_CLIFF => 3,
                TERRAIN_SHALLOW_WATER => {
                    if fill.mode != PATH_NON_SPACE && shallows_are_water {
                        2
                    } else {
                        1
                    }
                }
                TERRAIN_SPACE => 4,
                _ => 1,
            };
            if view.is_edge(x, y) {
                *kind = 5;
            }
            true
        }
        1 => {
            if fill.mode == PATH_NON_SPACE {
                terrain != TERRAIN_SPACE && !view.is_edge(x, y)
            } else if terrain != TERRAIN_DEEP_WATER
                && terrain != TERRAIN_CLIFF
                && terrain != TERRAIN_SPACE
                && !view.is_edge(x, y)
            {
                view.resource_kind(x, y) == 0
            } else {
                false
            }
        }
        2 => {
            if (terrain == TERRAIN_DEEP_WATER || terrain == TERRAIN_SHALLOW_WATER) && !view.is_edge(x, y) {
                let mut result = true;
                if terrain == TERRAIN_SHALLOW_WATER && view.has_units(x, y) {
                    let area = view.path_area_at(x, y);
                    if area.found && area.kind != KIND_WATER {
                        if area.kind <= KIND_MAP_EDGE {
                            result = false;
                        } else if area.kind > KIND_NEAR_SHORE {
                            result = area.kind == KIND_UNIT_OBSTACLE && view.live_dock(x, y);
                        }
                    }
                }
                result
            } else {
                false
            }
        }
        3 => terrain == TERRAIN_CLIFF && !view.is_edge(x, y),
        4 => {
            if terrain == TERRAIN_SPACE && !view.is_edge(x, y) {
                let mut result = true;
                if view.has_units(x, y) {
                    let area = view.path_area_at(x, y);
                    if area.found {
                        if area.kind != KIND_UNIT_OBSTACLE {
                            if area.kind != KIND_SPACE {
                                result = false;
                            }
                        } else {
                            result = view.live_space_dock(x, y);
                        }
                    }
                }
                result
            } else {
                false
            }
        }
        5 => view.is_edge(x, y),
        _ => false,
    }
}

/// 0x417bcb (`EEAIAStarPathAreaGoal::AcceptTile`): whether the fill takes the tile into the current area, counting it
/// and growing the bounds. In the path area pass the first tile fixes the continent and the kind and later tiles must
/// match them; the area's tiles are all on or all off the map edge as its continent is a map edge one. In a kind pass
/// `path_area_classify` decides.
pub fn path_area_accept<V: AreaView + ?Sized>(fill: &mut EeAStarPathAreaFill, view: &mut V, x: i32, y: i32) -> bool {
    fill.check_corners = -1;
    let mut result = false;
    if !view.has_tile(x, y) {
        return false;
    }
    if fill.mode == PATH_NONE {
        let continent = view.continent_at(x, y);
        let terrain = view.terrain(x, y);
        if fill.continent_id != 0 {
            if fill.continent_kind == continent.category && fill.continent_id == continent.id {
                // land and the shallows: no resource, then no blocker
                let open = |view: &mut V| view.resource_kind(x, y) == 0 && !view.has_blocker(x, y);
                match fill.kind {
                    KIND_LAND => {
                        if terrain != TERRAIN_SHALLOW_WATER && terrain != TERRAIN_SPACE {
                            result = open(view);
                        }
                    }
                    KIND_WATER => result = terrain == TERRAIN_DEEP_WATER && view.tile_value(x, y) == 0,
                    KIND_SPACE => result = terrain == TERRAIN_SPACE && view.tile_value(x, y) == 0,
                    KIND_CLIFF => result = terrain == TERRAIN_CLIFF,
                    KIND_FOREST => {
                        let resource = view.resource_kind(x, y);
                        result = resource != 0;
                        if result {
                            count_resource(&mut fill.resources, resource);
                        }
                    }
                    KIND_GAIA_OBSTACLE => {
                        if view.resource_kind(x, y) == 0 {
                            result = view.first_blocker_id(x, y) == fill.owner_id;
                        }
                    }
                    KIND_MAP_EDGE => result = true,
                    KIND_SHALLOWS => {
                        if terrain == TERRAIN_SHALLOW_WATER && view.tile_value(x, y) == 0 {
                            result = open(view);
                        }
                    }
                    KIND_SHALLOWS_SHORE => {
                        if terrain == TERRAIN_SHALLOW_WATER && view.tile_value(x, y) < 0 {
                            result = open(view);
                        }
                    }
                    KIND_SHALLOWS_NEAR_SHORE => {
                        if terrain == TERRAIN_SHALLOW_WATER && view.tile_value(x, y) > 0 {
                            result = open(view);
                        }
                    }
                    KIND_SHORE => {
                        result =
                            (terrain == TERRAIN_DEEP_WATER || terrain == TERRAIN_SPACE) && view.tile_value(x, y) < 0
                    }
                    KIND_NEAR_SHORE => {
                        result =
                            (terrain == TERRAIN_DEEP_WATER || terrain == TERRAIN_SPACE) && view.tile_value(x, y) > 0
                    }
                    KIND_UNIT_OBSTACLE => {
                        if view.resource_kind(x, y) == 0 {
                            result = view.first_obstacle_id(x, y) == fill.owner_id;
                        }
                    }
                    KIND_WALL => {
                        if view.resource_kind(x, y) == 0 {
                            result = view.own_wall_first(x, y, fill.player_id);
                        }
                    }
                    KIND_GATE => {
                        if view.resource_kind(x, y) == 0 {
                            result = view.area_gate_owner(x, y, fill.player_id) == fill.owner_id;
                        }
                    }
                    _ => {}
                }
            }
        } else {
            fill.check_corners = 1;
            fill.tile_bounds = point(x, y);
            fill.continent_id = continent.id;
            fill.continent_kind = continent.category;
            match fill.continent_kind {
                CATEGORY_LAND => {
                    fill.kind = if terrain == TERRAIN_SHALLOW_WATER {
                        by_value(view.tile_value(x, y), KIND_SHALLOWS_SHORE, KIND_SHALLOWS_NEAR_SHORE, KIND_SHALLOWS)
                    } else {
                        KIND_LAND
                    };
                    let resource = view.resource_kind(x, y);
                    if resource != 0 {
                        fill.forest_id = fill.next_forest_id;
                        fill.kind = KIND_FOREST;
                        fill.check_corners = 0;
                        count_resource(&mut fill.resources, resource);
                    } else {
                        let blocker = view.first_blocker(x, y);
                        if blocker.found {
                            fill.owner_id = blocker.unit_id;
                            fill.kind = KIND_UNIT_OBSTACLE;
                            fill.player_id = blocker.player_id;
                            fill.check_corners = 0;
                            if blocker.is_gate {
                                fill.kind = KIND_GATE;
                            } else if blocker.is_wall {
                                fill.kind = KIND_WALL;
                            }
                        }
                    }
                }
                CATEGORY_WATER => fill.kind = by_value(view.tile_value(x, y), KIND_SHORE, KIND_NEAR_SHORE, KIND_WATER),
                CATEGORY_SPACE => fill.kind = by_value(view.tile_value(x, y), KIND_SHORE, KIND_NEAR_SHORE, KIND_SPACE),
                CATEGORY_MAP_EDGE => {
                    fill.kind = KIND_MAP_EDGE;
                    fill.check_corners = 0;
                }
                CATEGORY_CLIFF => {
                    fill.kind = KIND_CLIFF;
                    fill.check_corners = 0;
                }
                _ => {}
            }
            result = true;
        }
        if result {
            result = view.is_edge(x, y) == (fill.continent_kind == CATEGORY_MAP_EDGE);
        }
    } else {
        if fill.tile_count == 0 {
            fill.tile_bounds = point(x, y);
        }
        let mut kind = fill.continent_kind;
        result = path_area_classify(fill, view, x, y, &mut kind, true);
        fill.continent_kind = kind;
    }
    if result {
        fill.tile_count = fill.tile_count.wrapping_add(1);
        fill.tile_bounds.extend(x, y);
    }
    result
}

/// `EEAIAStarPathSectorGoal`'s fill state.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAStarPathSectorFill {
    /// `mPathAreaStorage` (the path area fill's storage, whose id grid the fill reads)
    pub path_area_storage: *const c_void,
    /// `mClassifyTiles`
    pub classify_tiles: bool,
    /// `mTileCount`, `mTileBounds`, `mSectorBounds`
    pub tile_count: i32,
    pub tile_bounds: AStarRect,
    pub sector_bounds: AStarRect,
    /// `mPathAreaID`, `mPathAreaKind`, `mContinentID`, `mCategory`, `mKind`
    pub path_area_id: u16,
    pub path_area_kind: i32,
    pub continent_id: u16,
    pub category: u8,
    pub kind: i32,
    /// `mUnitID`, `mPlayerID`, `mWA0`, `mWA2`
    pub unit_id: i32,
    pub player_id: i32,
    pub wa0: u16,
    pub wa2: u16,
    /// out: the machine's `mCheckCorners` as the fill left it (-1: not written)
    pub check_corners: i32,
    /// out: the tile goes to `mStaleTiles` (classified, but not in a path area of the sub-sector's kind)
    pub stale: bool,
}

/// 0x41ae1d (`EEAIAStarPathSectorGoal::ClassifyTile`): the first tile sets the sub-sector's continent and kind from
/// terrain, shore distance and blocking units; later ones must match it inside the sector bounds.
pub fn path_sector_classify<V: AreaView + ?Sized>(
    fill: &mut EeAStarPathSectorFill,
    view: &mut V,
    x: i32,
    y: i32,
) -> bool {
    let terrain = view.terrain(x, y);
    let continent = view.continent_at(x, y);
    if fill.continent_id != 0 {
        if fill.category != continent.category || continent.id != fill.continent_id {
            return false;
        }
        let matched;
        match fill.kind {
            KIND_LAND => {
                if terrain == TERRAIN_SHALLOW_WATER || view.resource_kind(x, y) != 0 {
                    return false;
                }
                matched = !view.has_blocker(x, y);
            }
            KIND_WATER => {
                if terrain != TERRAIN_DEEP_WATER || view.tile_flag(x, y) != 0 {
                    return false;
                }
                matched = true;
            }
            KIND_CLIFF => {
                if terrain != TERRAIN_CLIFF {
                    return false;
                }
                matched = true;
            }
            KIND_FOREST => matched = view.resource_kind(x, y) != 0,
            KIND_GAIA_OBSTACLE => {
                if view.resource_kind(x, y) != 0 {
                    return false;
                }
                matched = view.first_blocker_id(x, y) == fill.unit_id;
            }
            KIND_MAP_EDGE => matched = true,
            KIND_SHALLOWS | KIND_SHALLOWS_SHORE | KIND_SHALLOWS_NEAR_SHORE => {
                if terrain != TERRAIN_SHALLOW_WATER {
                    return false;
                }
                let flag = view.tile_flag(x, y);
                let wrong = match fill.kind {
                    KIND_SHALLOWS => flag != 0,
                    KIND_SHALLOWS_SHORE => flag >= 0,
                    _ => flag <= 0,
                };
                if wrong || view.resource_kind(x, y) != 0 {
                    return false;
                }
                matched = !view.has_blocker(x, y);
            }
            KIND_SHORE | KIND_NEAR_SHORE => {
                if terrain != TERRAIN_DEEP_WATER && terrain != TERRAIN_SPACE {
                    return false;
                }
                let flag = view.tile_flag(x, y);
                if (fill.kind == KIND_SHORE && flag >= 0) || (fill.kind == KIND_NEAR_SHORE && flag <= 0) {
                    return false;
                }
                matched = true;
            }
            KIND_UNIT_OBSTACLE => {
                if view.resource_kind(x, y) != 0 {
                    return false;
                }
                matched = view.first_obstacle_id(x, y) == fill.unit_id;
            }
            KIND_WALL => {
                if view.resource_kind(x, y) != 0 || !view.own_wall_first(x, y, fill.player_id) {
                    return false;
                }
                matched = true;
            }
            KIND_GATE => {
                if view.resource_kind(x, y) != 0 {
                    return false;
                }
                matched = view.sector_gate_owner(x, y, fill.player_id) == fill.unit_id;
            }
            KIND_SPACE => {
                if terrain != TERRAIN_SPACE || view.tile_flag(x, y) != 0 {
                    return false;
                }
                matched = true;
            }
            _ => return false,
        }
        if !matched || !contains(&fill.sector_bounds, x, y) {
            return false;
        }
    } else {
        fill.check_corners = 1;
        fill.continent_id = continent.id;
        fill.category = continent.category;
        let area = view.path_area_at(x, y);
        fill.path_area_id = if area.found { area.id } else { 0 };
        match fill.category {
            CATEGORY_LAND => {
                fill.kind = if terrain == TERRAIN_SHALLOW_WATER {
                    by_value(view.tile_flag(x, y), KIND_SHALLOWS_SHORE, KIND_SHALLOWS_NEAR_SHORE, KIND_SHALLOWS)
                } else {
                    KIND_LAND
                };
                if view.resource_kind(x, y) != 0 {
                    fill.wa0 = fill.wa2;
                    fill.kind = KIND_FOREST;
                } else {
                    let blocker = view.first_blocker(x, y);
                    if blocker.found {
                        fill.unit_id = blocker.unit_id;
                        fill.kind = KIND_UNIT_OBSTACLE;
                        fill.player_id = blocker.player_id;
                        fill.check_corners = 0;
                        if blocker.is_gate {
                            fill.kind = KIND_GATE;
                        } else if blocker.is_wall {
                            fill.kind = KIND_WALL;
                        }
                    }
                }
            }
            CATEGORY_WATER => fill.kind = by_value(view.tile_flag(x, y), KIND_SHORE, KIND_NEAR_SHORE, KIND_WATER),
            CATEGORY_SPACE => fill.kind = by_value(view.tile_flag(x, y), KIND_SHORE, KIND_NEAR_SHORE, KIND_SPACE),
            CATEGORY_MAP_EDGE => {
                fill.kind = KIND_MAP_EDGE;
                fill.check_corners = 0;
            }
            CATEGORY_CLIFF => {
                fill.kind = KIND_CLIFF;
                fill.check_corners = 0;
            }
            _ => {}
        }
    }
    if view.is_edge(x, y) != (fill.category == CATEGORY_MAP_EDGE) {
        return false;
    }
    let area = view.path_area_at(x, y);
    if !area.found || area.kind != fill.kind {
        fill.stale = true;
    }
    true
}

/// 0x41acbf (`EEAIAStarPathSectorGoal::AcceptTile`): by default the tile must lie in the first tile's path area and
/// the sector's bounds; in classify mode `path_sector_classify` decides. Accepted tiles are counted and grow the bounds.
pub fn path_sector_accept<V: AreaView + ?Sized>(
    fill: &mut EeAStarPathSectorFill,
    view: &mut V,
    x: i32,
    y: i32,
) -> bool {
    fill.check_corners = -1;
    fill.stale = false;
    if !view.has_tile(x, y) {
        return false;
    }
    if fill.tile_count == 0 {
        fill.tile_bounds = point(x, y);
    }
    if !fill.classify_tiles {
        if fill.path_area_id != 0 {
            if fill.path_area_id != view.stored_path_area_id(fill.path_area_storage, x, y) {
                return false;
            }
            if !contains(&fill.sector_bounds, x, y) {
                return false;
            }
        } else {
            fill.path_area_id = view.stored_path_area_id(fill.path_area_storage, x, y);
            let area = view.path_area(fill.path_area_id);
            fill.continent_id = area.continent_id;
            fill.path_area_kind = area.kind;
            let k = fill.path_area_kind;
            let no_corners =
                k == KIND_CLIFF || (k > KIND_FOREST && k <= KIND_MAP_EDGE) || (k > KIND_NEAR_SHORE && k <= KIND_GATE);
            fill.check_corners = if no_corners { 0 } else { 1 };
        }
    } else if !path_sector_classify(fill, view, x, y) {
        return false;
    }
    fill.tile_count = fill.tile_count.wrapping_add(1);
    fill.tile_bounds.extend(x, y);
    true
}
