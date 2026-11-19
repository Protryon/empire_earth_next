//! The world the placement searches and resource centres ask ([`PlaceHost`]): one trait for both modules
//! (`placement`, `resource_center`), served in the game by the C++ host functions `ee_cp_place_host`
//! (EECPPlacement.cpp) and `ee_cp_center_host` (EECPResourceCenter.cpp) through `place_game`, and by a fake world in
//! the unit tests.
//!
//! What the original asked of C++ calls with a TEST ONLY note (the UUTL record of `EEGetSquareRingTiles` and its
//! family) is the trait's tile lists: their default bodies are the pure tile math (`units::uu_tiles`); the game host
//! takes the noted entry points (`units::uu_ffi`, the same math) only while the simulation capture records, so the
//! notes stay. The sorts whose comparator has ties (`std::sort` is not stable) go back to the C++ `std::sort` through
//! [`PlaceHost::sort_order`], with the comparator's key: the permutation a comparison sort makes depends only on the
//! comparisons' outcomes.

use std::os::raw::c_long;

use crate::units::uu_tiles;

/// A tile (x, y): the C++'s `U2DPoint<long>` / `EETileCoordinate`.
pub type Tile = [i32; 2];

/// "No tile" (`mTownCenterTile`'s none, a unit off the map).
/// cbindgen:ignore
pub(crate) const CP_NO_TILE: i32 = -10000;

/// One unit's members the searches read (`UNIT_INFO`; a null unit reads as `Default`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCpUnitInfo {
    /// `mTileX`, `mTileY`
    pub tile: [i32; 2],
    /// `mUnitID`
    pub id: u32,
    /// `mUnitClass->mUnitType`, `GetTileSize()`, `mCategory`, `mFamily`, `mTechnology`
    pub unit_type: i32,
    pub tile_size: i32,
    pub category: i32,
    pub family: i32,
    pub technology: i32,
    /// `mHitPoints`, `mResourceAmount`, `mResourceType`
    pub hit_points: i32,
    pub resource_amount: i32,
    pub resource_type: i32,
    /// `mComplexUnit` (0: none) and, when there is one, its `mTileX`, `mTileY`, `mHitPoints`, `mIsBuilt`, `mFacing`,
    /// `mPreviousPart`, `mNextPart`
    pub complex: u64,
    pub complex_tile: [i32; 2],
    pub complex_hit_points: i32,
    pub complex_built: u8,
    /// `mIsValid`, `mTile != NULL`, `GetTileNode() != NULL`, `mUnitClass->IsLandBuilding()`, `mIsWall`, `IsDropSite()`
    pub valid: u8,
    pub has_tile: u8,
    pub has_tile_node: u8,
    pub land_building: u8,
    pub wall: u8,
    pub drop_site: u8,
    pub has_class: u8,
    pub facing: u32,
    pub previous_part: u64,
    pub next_part: u64,
    /// `mPlayer` (the owner's `EEPlayer*`)
    pub player: u64,
}

/// A unit-rectangle filter of `EEGetUnitsInRectangle` (with the computer player and the rectangle).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RectFilter {
    /// `EEAnyUnit`
    Any = 0,
    /// `EECPRCLiveBuildingInRectangle`: the player's living houses
    LiveHouse = 1,
    /// `EECPRCBuildingInRectangle`: the player's living, unfinished houses
    UnfinishedHouse = 2,
}

/// A rectangle (`U2DRectangle<long>`): left, top, right, bottom, inclusive.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Rect {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}

impl Rect {
    /// `Set(point)`
    pub fn point(t: Tile) -> Rect {
        Rect { left: t[0], top: t[1], right: t[0], bottom: t[1] }
    }
    /// `Contains(point)`: edges included
    pub fn contains(&self, t: Tile) -> bool {
        t[0] >= self.left && t[0] <= self.right && t[1] >= self.top && t[1] <= self.bottom
    }
    /// `Extend(point)`
    pub fn extend(&mut self, t: Tile) {
        self.left = self.left.min(t[0]);
        self.right = self.right.max(t[0]);
        self.top = self.top.min(t[1]);
        self.bottom = self.bottom.max(t[1]);
    }
    /// `Grow(margin, clamp)`: the edges move out, the left and top clamped at 0
    pub fn grow(&mut self, margin: i32, clamp: bool) {
        self.left = self.left.wrapping_sub(margin);
        self.top = self.top.wrapping_sub(margin);
        self.right = self.right.wrapping_add(margin);
        self.bottom = self.bottom.wrapping_add(margin);
        if clamp {
            self.left = self.left.max(0);
            self.top = self.top.max(0);
        }
    }
    /// `GetCenter`
    pub fn center(&self) -> Tile {
        [self.right.wrapping_sub(self.left) / 2 + self.left, self.bottom.wrapping_sub(self.top) / 2 + self.top]
    }
}

/// `U2DPoint<long>::DistanceToSquared` (and `EETileDistanceLess::DistanceSquared`): the C++ `long` sum of squares.
pub fn distance_squared(a: Tile, b: Tile) -> i64 {
    let dx = (a[0] as c_long).wrapping_sub(b[0] as c_long);
    let dy = (a[1] as c_long).wrapping_sub(b[1] as c_long);
    dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) as i64
}

/// `EECPUnitWithinDistance`: `(float)tile.DistanceToSquared(center) <= squared`.
pub fn within(tile: Tile, center: Tile, squared: f32) -> bool {
    (distance_squared(tile, center) as f32) <= squared
}

/// `U2DPoint<long>::DistanceTo` (the exe's: the squares summed as float, `sqrt`, truncated).
pub fn distance_to(a: Tile, b: Tile) -> i32 {
    let dy = (a[1] as c_long).wrapping_sub(b[1] as c_long);
    let dx = (a[0] as c_long).wrapping_sub(b[0] as c_long);
    let (dx, dy) = (dx.wrapping_mul(dx), dy.wrapping_mul(dy));
    ((dx as f32) + (dy as f32)).sqrt() as i32
}

/// `EEGetSquareBlockRing(1, x, y, size, tiles, false)`: the square's sides (per row its left and right tile, per
/// column its top and bottom one), then its four corners (`EEGetBlockRing` with every corner kept).
pub fn square_block_ring_all(x: i32, y: i32, size: i32) -> Vec<Tile> {
    let mut out = Vec::new();
    let bottom = size.wrapping_add(y);
    let mut ty = y;
    while ty < bottom {
        out.push([x - 1, ty]);
        out.push([size + x, ty]);
        ty += 1;
    }
    let right = size.wrapping_add(x);
    let mut tx = x;
    while tx < right {
        out.push([tx, y - 1]);
        out.push([tx, bottom]);
        tx += 1;
    }
    corners_all(x, y, size, &mut out);
    out
}

/// `EEGetCornerTiles(1, x, y, size, tiles)`: the square's four corners.
pub fn corner_tiles_all(x: i32, y: i32, size: i32) -> Vec<Tile> {
    let mut out = Vec::with_capacity(4);
    corners_all(x, y, size, &mut out);
    out
}

/// The four corners of a square (`EEGetRingCorners`' order with every corner kept).
fn corners_all(x: i32, y: i32, size: i32, out: &mut Vec<Tile>) {
    let bottom = y.wrapping_add(size);
    let right = x.wrapping_add(size);
    out.push([x - 1, y - 1]);
    out.push([x - 1, bottom]);
    out.push([right, y - 1]);
    out.push([right, bottom]);
}

/// The placement's and centres' world. Units, players, towns, continents, choke points and resource centres are
/// handles (the C++ objects' addresses; 0: none).
pub trait PlaceHost {
    // ---- the computer player
    /// `mPlayerID`
    fn player_id(&mut self) -> u32;
    /// the player's `EEPlayer*`
    fn player_handle(&mut self) -> u64;
    /// `mTechnologies->mCurrentEpoch`
    fn epoch(&mut self) -> i32;
    /// `mDropOffDistance`, `mTowerSpacing`
    fn drop_off_distance(&mut self) -> i32;
    fn tower_spacing(&mut self) -> f32;
    /// the placement's `mTownCenterTile`
    fn town(&mut self) -> Tile;
    fn set_town(&mut self, tile: Tile);
    /// `GetCitizenPassability()`
    fn passability(&mut self) -> i32;
    /// `mUnitClassUpgrades[class]->mRange`
    fn range(&mut self, class: i32) -> f32;
    /// `GetTechTree()->Get(id)`'s `mData->mKind` (None: no node)
    fn tech_kind(&mut self, id: i32) -> Option<i32>;
    /// `gCivPowersManager.HasPower(player, kCivPowerPathfinding)`
    fn has_pathfinding(&mut self) -> bool;
    /// `EECPClaimRangeOn(player)`, `EECPClaimRange(player)`
    fn claim_range_on(&mut self) -> bool;
    fn claim_range(&mut self) -> i32;
    /// the units from `mFirstUnit` (their list's order)
    fn own_units(&mut self) -> Vec<u64>;
    /// `EEGetPlayerUnitsOfType(type, player id, units)` on `units` as it is
    fn units_of_type(&mut self, unit_type: i32, player_id: u32, units: &mut Vec<u64>);
    /// `gWorld.mPlayers[0]`'s units from its `mFirstUnit` (None: no player 0)
    fn nature_units(&mut self) -> Option<Vec<u64>>;
    /// `mTowns`: per town (None for a null entry) its `mContinent` and `GetHullCenter()`
    fn towns(&mut self) -> Vec<Option<(u16, Tile)>>;
    /// `mResourceCenters` (0 for a null entry)
    fn centers(&mut self) -> Vec<u64>;

    // ---- a resource centre (`EECPResourceCenter*`)
    /// `mUnits`' saved ids, `mUnit`'s, `mIsActive`
    fn center_units(&mut self, center: u64) -> Vec<u32>;
    fn center_unit(&mut self, center: u64) -> u32;
    fn center_active(&mut self, center: u64) -> bool;
    /// `mUnits` reordered (the same ids)
    fn center_set_order(&mut self, center: u64, ids: &[u32]);
    /// `mUnits.clear()` (the references stay: the original's Restore dropped them unreleased)
    fn center_clear_units(&mut self, center: u64);
    /// `AddUnit(unit)`: referenced, pushed
    fn center_add_unit(&mut self, center: u64, unit: u64);
    /// Restore's main unit: `mUnit` from its saved id `id` (`dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(id))`,
    /// referenced) unless it is -1 (then none); 3 when the unit is gone, else 0
    fn center_restore_main(&mut self, center: u64, id: u32) -> i32;

    // ---- units and classes
    /// the unit registered under a handle id (`EEUnitHandle::Get`; 0: none)
    fn resolve(&mut self, id: u32) -> u64;
    /// `gWorld.mPlayers[0] != NULL`
    fn has_player0(&mut self) -> bool;
    /// `gWorld.GetUnit(id)`
    fn world_unit(&mut self, id: u32) -> u64;
    fn unit(&mut self, unit: u64) -> EeCpUnitInfo;
    /// `a->DistanceSquaredToUnit(b)` (complex unit `a`)
    fn distance_to_unit(&mut self, a: u64, b: u64) -> f32;
    /// `EEUnitClassCount()`, `gUnitClassTable[class] != NULL`, its `GetTileSize()`, `mTerrainTypes[terrain]`
    fn class_count(&mut self) -> u32;
    fn class_exists(&mut self, class: i32) -> bool;
    fn class_tile_size(&mut self, class: i32) -> i32;
    fn class_terrain(&mut self, class: i32, terrain: i32) -> bool;
    /// `gUnitClassTable[class]->GetTechnology()`
    fn class_technology(&mut self, class: i32) -> i32;

    // ---- the map
    /// `gWorld.mMap != NULL`, its `mMapCharacteristics`
    fn has_map(&mut self) -> bool;
    fn map_flags(&mut self) -> u32;
    /// `GetTileAtLocation(tile)`'s `mTerrainType` (None: no tile)
    fn tile_terrain(&mut self, t: Tile) -> Option<i32>;
    /// `GetTile(x, y)`'s `mTerrainType` (None: no tile)
    fn map_tile_terrain(&mut self, t: Tile) -> Option<i32>;
    /// `GetTileAtLocation(tile)`: Some(whether its `mUnits` is NULL), None: no tile
    fn tile_free(&mut self, t: Tile) -> Option<bool>;
    fn is_edge(&mut self, t: Tile) -> bool;

    // ---- the continent manager
    fn largest_continent(&mut self, kind: i32) -> i32;
    /// `GetContinentFromID(id)`, `GetContinentByID(id)`, `GetContinentAt(x, y)`, the continent's `GetID()`
    fn continent_from_id(&mut self, id: u16) -> u64;
    fn continent_by_id(&mut self, id: u16) -> u64;
    fn continent_at(&mut self, t: Tile) -> u64;
    fn continent_id(&mut self, continent: u64) -> u16;
    /// `mTileFlags[a * mFlagsWidth + b]`
    fn continent_flag(&mut self, a: u16, b: u16) -> bool;
    /// continent `c`'s `GetShoreNextTo(id, tiles)` on `tiles` as it is
    fn shore_next_to(&mut self, continent: u64, id: u16, tiles: &mut Vec<Tile>);
    /// continent `c`'s `GetShoreTiles()`, `GetHullCenter()`
    fn shore_tiles(&mut self, continent: u64) -> Vec<Tile>;
    /// a continent's (or, `choke`, a choke point's) `GetHullCenter()`
    fn hull_center(&mut self, area: u64, choke: bool) -> Tile;
    fn are_connected(&mut self, a: u64, b: u64, flags: i32) -> bool;
    /// `GetContinent(tile, kAIPathLandMass)`, `GetContinent(unit, kAIPathLandMass)`, `GetContinentID(x, y)`
    fn tile_continent(&mut self, t: Tile) -> u16;
    fn unit_continent(&mut self, unit: u64) -> u16;
    fn continent_id_at(&mut self, t: Tile) -> u16;
    /// `GetChokePointAt(tile) != NULL`, `GetHillAt(tile) != NULL`
    fn choke_at(&mut self, t: Tile) -> bool;
    fn hill_at(&mut self, t: Tile) -> bool;
    /// `GetPathAreaAt(tile)`, `GetPathAreaAt(unit)`
    fn path_area_at(&mut self, t: Tile) -> u64;
    fn unit_path_area(&mut self, unit: u64) -> u64;
    /// `GetChokePoints(list, id, true)`; a choke point's `GetHullBounds()`, `GetTileSet().BuildPointList(tiles, 1)` on
    /// `tiles` as it is
    fn choke_points(&mut self, id: u16) -> Vec<u64>;
    fn hull_bounds(&mut self, area: u64) -> Rect;
    fn choke_point_tiles(&mut self, choke: u64, tiles: &mut Vec<Tile>);

    // ---- the rest of the world
    /// `EEPlayerBlockTakes(player, type, x, y, w, h, 0)`
    fn block_takes(&mut self, unit_type: i32, x: i32, y: i32, w: i32, h: i32) -> bool;
    /// `EEGetFacingQuarter(x, y)`
    fn facing_quarter(&mut self, t: Tile) -> i32;
    /// `EECPNextSiteThreat(player, x, y, false, NULL) != 0`
    fn site_threat(&mut self, t: Tile) -> bool;
    /// `EEGetTileUnitOfType(tile, type) != NULL`
    fn tile_unit_of_type(&mut self, t: Tile, unit_type: i32) -> bool;
    /// `EEGetUnitsInRectangle(rect, units, filter, true)` on `units` as it is
    fn units_in_rect(&mut self, rect: Rect, filter: RectFilter, units: &mut Vec<u64>);
    /// `gPhysics->GetRandom(0, max)`
    fn random(&mut self, max: i32) -> i32;
    /// `EEIsSpaceDockSiteOpen(x, y, player)`
    fn space_dock_open(&mut self, t: Tile) -> bool;
    /// the order `std::sort` puts elements with these keys in (comparator: key less)
    fn sort_order(&mut self, keys: &[i64]) -> Vec<usize>;

    // ---- the area effects
    /// `gAEManager.HasAreaEffect(kind, x, y, player)`
    fn has_area_effect(&mut self, kind: i32, x: i32, y: i32, player: u32) -> bool;
    /// the unit's first morale-from-buildings effect's `GetRadius()` (None: none)
    fn morale_radius(&mut self, unit: u64) -> Option<f32>;
    /// the same effect after `MarkDirty()`, `Recount(0)`: its `GetLevel()`, `mTemplate->mMaxLevel`, `GetRadius()`
    fn morale_recount(&mut self, unit: u64) -> Option<(i32, i32, f32)>;
    /// the class upgrade's `mAreaEffectTemplates[0..4]`, a template's `mRadius`
    fn effect_templates(&mut self, class: i32) -> [u32; 4];
    fn template_radius(&mut self, template: u32) -> f32;

    // ---- the tile lists (`units::uu_tiles`)
    /// `EEGetSquareRingTiles(x, y, radius, tiles)`
    fn square_ring_tiles(&mut self, t: Tile, radius: i32) -> Vec<Tile> {
        uu_tiles::square_ring_tiles(t[0], t[1], radius)
    }
    /// `EEGetSquareBlockRing(1, x, y, size, tiles, false)`: `tiles` replaced (its TEST ONLY note records what it held)
    fn square_block_ring(&mut self, t: Tile, size: i32, tiles: &mut Vec<Tile>) {
        *tiles = square_block_ring_all(t[0], t[1], size);
    }
    /// `EEGetSquareRing(x, y, w, h, tiles)`
    fn square_ring(&mut self, t: Tile, w: i32, h: i32) -> Vec<Tile> {
        uu_tiles::square_ring(t[0], t[1], w, h)
    }
    /// `EEGetSideTiles(x, y, size, tiles)`
    fn side_tiles(&mut self, t: Tile, size: i32) -> Vec<Tile> {
        uu_tiles::square_ring(t[0], t[1], size, size)
    }
    /// `EEGetCornerTiles(1, x, y, size, tiles)`
    fn corner_tiles(&mut self, t: Tile, size: i32) -> Vec<Tile> {
        corner_tiles_all(t[0], t[1], size)
    }
    /// `EEGetTilesInRadius(x, y, radius, inclusive, tiles)`: appended
    fn tiles_in_radius(&mut self, t: Tile, radius: u32, inclusive: bool, tiles: &mut Vec<Tile>) {
        uu_tiles::tiles_in_radius(t[0], t[1], radius, inclusive, tiles)
    }
}

/// `EEAIRandomShuffle` (Dinkum 2.33's `random_shuffle`): the same draws and swaps.
pub fn shuffle<H: PlaceHost + ?Sized, T>(h: &mut H, items: &mut [T]) {
    if items.is_empty() {
        return;
    }
    for (count, at) in (2i32..).zip(1..items.len()) {
        let max = (count - 1).max(0);
        let j = h.random(max) as usize;
        items.swap(at, j);
    }
}

/// `std::sort` of `items` by a key (comparator: key less), through the host's C++ sort.
pub fn sort_by_key<H: PlaceHost + ?Sized, T: Clone>(h: &mut H, items: &mut Vec<T>, key: impl Fn(&T) -> i64) {
    if items.len() < 2 {
        return;
    }
    let keys: Vec<i64> = items.iter().map(&key).collect();
    let order = h.sort_order(&keys);
    let sorted: Vec<T> = order.iter().map(|&i| items[i].clone()).collect();
    *items = sorted;
}
