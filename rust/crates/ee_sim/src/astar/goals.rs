//! The flood goals' tile tests (R5b.1 step 4): `EEAIAStarContinentGoal::AcceptTile`, `EEAIAStarHillGoal::AcceptTile`
//! and `EEAIAStarChokePointGoal::AcceptTile`, line by line. Each asks the world only through `View` (in C,
//! `ffi::EeAStarViewCallbacks` over the goal's `EEAIAStarView`), in the C++'s order, and updates the goal's state the
//! C++ kept in its members (the caller copies them in and out). Their machine hooks are constants the C++ headers
//! keep (no goal node but the empty list, unit costs and estimates, no limits, no reopening).

/// `EEMap.h`'s terrain types the fills test.
pub(crate) const TERRAIN_DEEP_WATER: i32 = 1;
pub(crate) const TERRAIN_CLIFF: i32 = 9;
pub(crate) const TERRAIN_SPACE: i32 = 0xc;

/// `EEAIContinentCategory`.
pub(crate) const CATEGORY_NONE: u8 = 0;
pub(crate) const CATEGORY_LAND: u8 = 1;
pub(crate) const CATEGORY_WATER: u8 = 2;
pub(crate) const CATEGORY_CLIFF: u8 = 3;
pub(crate) const CATEGORY_SPACE: u8 = 4;
pub(crate) const CATEGORY_MAP_EDGE: u8 = 5;

/// The view queries the flood goals make (`ee/EEAIAStarView.h`).
pub trait View {
    fn has_tile(&mut self, x: i32, y: i32) -> bool;
    fn terrain(&mut self, x: i32, y: i32) -> i32;
    fn is_edge(&mut self, x: i32, y: i32) -> bool;
    fn height_level(&mut self, x: i32, y: i32) -> i32;
    fn continent_id(&mut self, x: i32, y: i32) -> u16;
    /// `IsChokeTile(index)`: a flagged tile that is not an area tile, and its area serial
    fn choke_tile(&mut self, index: i32) -> Option<u32>;
}

/// `EEAIAStarContinentGoal`'s fill state.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarContinentFill {
    /// `mKind` (`EEAIContinentCategory`; 0 until the first tile picks it)
    pub kind: u8,
    /// `mTileCount`
    pub tile_count: i32,
}

/// 0x41575c: whether the fill takes the tile (land, deep water, space, cliff or map edge; the first tile picks the
/// kind), counting it; map edge tiles join only map edge continents.
pub fn continent_accept<V: View + ?Sized>(fill: &mut EeAStarContinentFill, view: &mut V, x: i32, y: i32) -> bool {
    if !view.has_tile(x, y) {
        return false;
    }
    let terrain = view.terrain(x, y);
    let matched = match fill.kind {
        CATEGORY_LAND => terrain != TERRAIN_DEEP_WATER && terrain != TERRAIN_CLIFF && terrain != TERRAIN_SPACE,
        CATEGORY_WATER => terrain == TERRAIN_DEEP_WATER,
        CATEGORY_SPACE => terrain == TERRAIN_SPACE,
        CATEGORY_CLIFF => terrain == TERRAIN_CLIFF,
        CATEGORY_MAP_EDGE => view.is_edge(x, y),
        CATEGORY_NONE => {
            fill.kind = match terrain {
                TERRAIN_DEEP_WATER => CATEGORY_WATER,
                TERRAIN_CLIFF => CATEGORY_CLIFF,
                TERRAIN_SPACE => CATEGORY_SPACE,
                _ => CATEGORY_LAND,
            };
            if view.is_edge(x, y) {
                fill.kind = CATEGORY_MAP_EDGE;
            }
            true
        }
        _ => false,
    };
    if matched && view.is_edge(x, y) == (fill.kind == CATEGORY_MAP_EDGE) {
        fill.tile_count = fill.tile_count.wrapping_add(1);
        return true;
    }
    false
}

/// `EEAIAStarHillGoal`'s fill state.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarHillFill {
    /// `mContinentID` (the first tile's; 0 between hills)
    pub continent_id: u16,
    /// `mTileCount`
    pub tile_count: i32,
    /// `mLevelSum`
    pub level_sum: i32,
}

/// 0x416db8: whether the fill takes the tile: not cliff, height level 4 or more, on the hill's continent (the first
/// tile picks it); counts the tile and adds its level to the sum.
pub fn hill_accept<V: View + ?Sized>(fill: &mut EeAStarHillFill, view: &mut V, x: i32, y: i32) -> bool {
    if !view.has_tile(x, y) {
        return false;
    }
    let level = view.height_level(x, y);
    let mut result = false;
    if view.terrain(x, y) != TERRAIN_CLIFF && level >= 4 {
        if fill.continent_id != 0 {
            result = view.continent_id(x, y) == fill.continent_id;
        } else {
            fill.continent_id = view.continent_id(x, y);
            result = true;
        }
        if result {
            fill.tile_count = fill.tile_count.wrapping_add(1);
            fill.level_sum = fill.level_sum.wrapping_add(level);
        }
    }
    result
}

/// `EEAIAStarChokePointGoal`'s fill state.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarChokeFill {
    /// `mWidth`, `mHeight` (the map's size: `Attach`)
    pub width: i32,
    pub height: i32,
    /// `mTileCount`
    pub tile_count: i32,
}

/// 0x410fcc: whether the fill takes the tile (marked by the terrain pass and not an area tile), counting it; the
/// tile's area serial, which the caller appends to `mAreaSerials`.
pub fn choke_accept<V: View + ?Sized>(fill: &mut EeAStarChokeFill, view: &mut V, x: i32, y: i32) -> Option<u32> {
    if x >= 0 && x < fill.width && y >= 0 && y < fill.height {
        if let Some(serial) = view.choke_tile(fill.width.wrapping_mul(x).wrapping_add(y)) {
            fill.tile_count = fill.tile_count.wrapping_add(1);
            return Some(serial);
        }
    }
    None
}
