//! The citizens' searches (R5b.4, the thirty-fifth round): `EEUnitClass::FindWoodSite`, `FindResourceOfKind` (with
//! `EEIsResourceOfKind`), `FindStorage`, `FindGatherPoint` (with `IsTileHeld`) and `HasGatherPoint`. Each body is the
//! C++ statements in order over one host ([`CzHost`]; `ee_units_cz_host`, EEUnitClassPeople.cpp): the units read
//! whole where the original read them, the noted unit utilities (the square rings, side tiles and ring corners:
//! their UUTL records), the AI's shuffle and every world test or effect one host call each. The members stay and
//! keep their UCBD notes (kinds 40-45, the golden of this move).

/// The tile pairs the unit utilities return (`U2DPoint<long>`).
pub type CzTile = [i32; 2];

const UNIT_TYPE_FARM: u32 = 0x2a;
const UNIT_TYPE_ROBOTIC_FARM: u32 = 0x33a;
const UNIT_TYPE_PATH_POINT: u32 = 0xdf;
const UNIT_TYPE_RES_FORAGE_BUSH: u32 = 0x37;
const UNIT_TYPE_DOCK: u32 = 0x24;
const UNIT_TYPE_NAVY_YARD: u32 = 0x41;
const CATEGORY_AMBIENT: u32 = 0;
const CATEGORY_LAND_CITIZEN: u32 = 6;
const CATEGORY_NAVAL: u32 = 4;
const FAMILY_ANIMAL: u32 = 0x11;
const FAMILY_FISH: u32 = 0x12;
const TERRAIN_CLIFF: i32 = 9;
const TERRAIN_DEEP_WATER: i32 = 1;
const TERRAIN_SHALLOW_WATER: i32 = 10;
const GOAL_GATHER: i32 = 6;
const GOAL_BUILD_REPAIR: i32 = 4;
/// the range from a farm to its drop site within which a citizen counts it the farm's (0x446fdc)
const CITIZEN_FARM_RANGE: f32 = 20.0;

/// A unit as the bodies read it (`ee_units_cz_host`'s `EE_UNITS_CZ_UNIT`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsCzUnit {
    pub obj: u64,
    /// `mUnitClass`, `mComplexUnit`, `mPlayer`, `mGoalState` (a complex unit's)
    pub class_obj: u64,
    pub complex: u64,
    pub player: u64,
    pub goal_state: u64,
    /// the goal's `mKind` (-1: none)
    pub goal_kind: i32,
    pub tile_x: i32,
    pub tile_y: i32,
    pub hit_points: i32,
    pub resource_type: u32,
    /// `mTileNode != NULL`; a complex unit's `mIsDisabled`, `mIsBuilt`, `IsFoundation()`
    pub on_map: u8,
    pub disabled: u8,
    pub built: u8,
    pub foundation: u8,
    // the class's
    pub category: u32,
    pub unit_type: u32,
    pub family: u32,
    pub tile_size: i32,
    /// `IsDropSite()`
    pub drop_site: u8,
}

/// The world the bodies act on (units, goals and tiles are their addresses).
pub trait CzHost {
    fn unit(&mut self, u: u64) -> EeUnitsCzUnit;
    /// `gUnitClassTable[unit_type]->GetTileSize()`
    fn class_tile_size(&mut self, unit_type: i32) -> i32;
    /// `gMapWidth`, `gMapHeight`
    fn map_size(&mut self) -> (u32, u32);
    /// `EEGetSquareRingTiles(x, y, radius, tiles)`
    fn square_ring_tiles(&mut self, x: i32, y: i32, radius: i32) -> Vec<CzTile>;
    /// `EEGetSideTiles(x, y, size, tiles)` into `tiles` as it was
    fn side_tiles(&mut self, x: i32, y: i32, size: i32, tiles: &mut Vec<CzTile>);
    /// `EEGetRingCorners(corners, x, y, width, height, tiles)` into `tiles` as it was
    fn ring_corners(&mut self, corners: i32, x: i32, y: i32, width: i32, height: i32, tiles: &mut Vec<CzTile>);
    /// `EEGetSquareRing(x, y, width, height, tiles)` into `tiles` as it was
    fn square_ring(&mut self, x: i32, y: i32, width: i32, height: i32, tiles: &mut Vec<CzTile>);
    /// `EEAIRandomShuffle(tiles)`
    fn shuffle(&mut self, tiles: &mut Vec<CzTile>);
    /// `gContinentManager.GetContinent(tile, kAIPathLandMass)`; of a unit
    fn tile_continent(&mut self, x: i32, y: i32) -> u16;
    fn unit_continent(&mut self, u: u64) -> u16;
    /// `EEBlockTakes(u, unit type, x, y, size, size, 0)`
    fn block_takes(&mut self, u: u64, unit_type: i32, x: i32, y: i32, size: i32) -> bool;
    /// `gWorld.mMap->GetTile(x, y)`
    fn tile(&mut self, x: i32, y: i32) -> u64;
    /// the tile's units (its `mUnits` chain)
    fn tile_units(&mut self, tile: u64) -> Vec<u64>;
    fn tile_terrain(&mut self, tile: u64) -> i32;
    fn tile_height(&mut self, tile: u64) -> f32;
    /// `u->CanGather(target, x, y, false, false)`
    fn can_gather(&mut self, u: u64, target: u64, x: i32, y: i32) -> bool;
    /// `gContinentManager.CanGetToUnit(a, b)`
    fn can_get_to_unit(&mut self, a: u64, b: u64) -> bool;
    /// `theUnit->mComplexUnit->DistanceSquared2D(GE2DPoint(x, y))`
    fn distance_2d(&mut self, u: u64, x: f32, y: f32) -> f32;
    /// `theUnit->GetTileDistanceSquared(center)`
    fn tile_distance(&mut self, u: u64, center: u64) -> f32;
    /// `gContinentManager.GetPathAreaIDAt(EETileLocation(x, y))`
    fn path_area(&mut self, x: i32, y: i32) -> u16;
    /// the gather goal's `GetGatherStorage()`, `GetGatherResource()`, `IsGathering()`
    fn gather_storage(&mut self, goal: u64) -> u64;
    fn gather_resource(&mut self, goal: u64) -> u64;
    fn is_gathering(&mut self, goal: u64) -> bool;
    /// the build goal's target class's `mIsWall`; its `GetBuildTargetType()`
    fn build_target_is_wall(&mut self, goal: u64) -> bool;
    fn build_target_type(&mut self, goal: u64) -> u32;
    /// `EEGetFarmDropSites(farm, sites)`
    fn farm_drop_sites(&mut self, farm: u64) -> Vec<u64>;
    /// `storage->CanTake(u, resource type)`
    fn can_take(&mut self, storage: u64, u: u64, resource_type: u32) -> bool;
    /// `SetResourceAmount(0)`, then `SetResourceType(t)`
    fn empty_load(&mut self, u: u64, resource_type: u32);
    /// `a->DistanceSquaredToUnit(b)`; `a->DistanceSquaredTo(b)`
    fn distance_to_unit(&mut self, a: u64, b: u64) -> f32;
    fn distance_to(&mut self, a: u64, b: u64) -> f32;
    /// the player's units (its list from `mFirstUnit`)
    fn player_units(&mut self, player: u64) -> Vec<u64>;
}

/// `FindWoodSite`: rings 2..9 around the citizen for a tile on its continent where a unit of the type fits.
pub fn find_wood_site<H: CzHost + ?Sized>(h: &mut H, u: u64, unit_type: i32) -> Option<[f32; 2]> {
    let size = h.class_tile_size(unit_type);
    let s = h.unit(u);
    let (w, hgt) = h.map_size();
    for radius in 2..10 {
        for t in h.square_ring_tiles(s.tile_x, s.tile_y, radius) {
            if (t[0] as u32) >= w || (t[1] as u32) >= hgt {
                continue;
            }
            let continent = h.tile_continent(t[0], t[1]);
            if h.unit_continent(u) != continent || !h.block_takes(u, unit_type, t[0], t[1], size) {
                continue;
            }
            return Some([t[0] as f32, t[1] as f32]);
        }
    }
    None
}

/// `EEIsResourceOfKind`: any for kind 1, else the same sort as `wanted` (a farm, an animal or fish, a forage bush)
/// or the kind's resource type; animals only with `farms`.
pub fn is_resource_of_kind(unit: &EeUnitsCzUnit, wanted: Option<&EeUnitsCzUnit>, kind: u32, farms: bool) -> bool {
    let tree = wanted.is_some_and(|r| r.family == FAMILY_ANIMAL || r.family == FAMILY_FISH);
    let farm = wanted.is_some_and(|r| r.unit_type == UNIT_TYPE_FARM);
    let bush = wanted.is_some_and(|r| r.unit_type == UNIT_TYPE_RES_FORAGE_BUSH);
    (kind == 1
        || kind == 2 && farm && unit.unit_type == UNIT_TYPE_FARM
        || tree && (unit.family == FAMILY_ANIMAL || unit.family == FAMILY_FISH)
        || bush && unit.unit_type == UNIT_TYPE_RES_FORAGE_BUSH
        || kind != 2 && unit.resource_type == kind)
        && (farms || unit.family != FAMILY_ANIMAL)
}

/// `FindResourceOfKind`: the nearest gatherable resource of a kind within `range` rings of `center`, not in
/// `taken` (the tiles shuffled per ring; another path area costs 400; a dead animal wins at once; a boat looks for
/// naval resources). `class` is the searching class's record (its category); `class_obj` the class.
#[allow(clippy::too_many_arguments)] // the member's arguments
pub fn find_resource_of_kind<H: CzHost + ?Sized>(
    h: &mut H,
    class_category: u32,
    u: u64,
    center: u64,
    wanted: u64,
    range: i32,
    kind: u32,
    taken: &[u64],
    farms: bool,
) -> u64 {
    let c = h.unit(center);
    let mut tiles: Vec<CzTile> = Vec::new();
    let mut inner: Vec<CzTile> = Vec::new();
    let mut outer: Vec<CzTile> = Vec::new();
    let (mut x, mut y) = (c.tile_x, c.tile_y);
    let mut size = c.tile_size;
    let mut best = range.wrapping_mul(range).wrapping_mul(2);
    let mut result = 0u64;
    let center_x = size as f32 * 0.5 + x as f32;
    let center_y = h.unit(center).tile_size as f32 * 0.5 + y as f32;
    let w = if wanted != 0 { Some(h.unit(wanted)) } else { None };
    let (map_w, map_h) = h.map_size();
    let mut i = 0;
    if range > 0 {
        loop {
            if best == 0 {
                break;
            }
            h.side_tiles(x, y, size, &mut inner);
            h.ring_corners(1, x, y, size, size, &mut outer);
            size += 2;
            x -= 1;
            y -= 1;
            tiles.clear();
            tiles.extend_from_slice(&inner);
            tiles.extend_from_slice(&outer);
            h.shuffle(&mut tiles);
            for &[tx, ty] in &tiles {
                let tile = h.tile(tx, ty);
                if tile == 0 {
                    continue;
                }
                for other in h.tile_units(tile) {
                    if other == 0 || other == center {
                        continue;
                    }
                    let o = h.unit(other);
                    let distance = if class_category == CATEGORY_LAND_CITIZEN {
                        if !is_resource_of_kind(&o, w.as_ref(), kind, farms)
                            || !h.can_gather(u, other, tx, ty)
                            || taken.contains(&other)
                            || (tx as u32) >= map_w
                            || (ty as u32) >= map_h
                            || !h.can_get_to_unit(u, other)
                        {
                            continue;
                        }
                        let location =
                            find_gather_point(h, u, o.tile_x as u32, o.tile_y as u32, o.tile_size, o.tile_size);
                        if ee_ge::um::floor(location[0]) == o.tile_x && ee_ge::um::floor(location[1]) == o.tile_y {
                            continue;
                        }
                        let mut d = if o.on_map != 0 {
                            ee_ge::um::floor(h.distance_2d(other, center_x, center_y))
                        } else {
                            ee_ge::um::floor(h.tile_distance(other, center))
                        };
                        let s = h.unit(u);
                        let from = h.path_area(s.tile_x, s.tile_y);
                        let to = h.path_area(ee_ge::um::floor(location[0]), ee_ge::um::floor(location[1]));
                        if from != to {
                            d = d.wrapping_add(400);
                        }
                        if o.family == FAMILY_ANIMAL && o.hit_points == 0 {
                            d = 0;
                        }
                        d
                    } else {
                        if kind != 1 && o.resource_type != kind
                            || o.category != CATEGORY_NAVAL
                            || !farms && o.family == FAMILY_ANIMAL
                            || !h.can_gather(u, other, tx, ty)
                            || !h.can_get_to_unit(u, other)
                            || !has_gather_point(h, u, other)
                        {
                            continue;
                        }
                        if o.on_map == 0 {
                            ee_ge::um::floor(h.tile_distance(other, center))
                        } else {
                            ee_ge::um::floor(h.distance_2d(other, center_x, center_y))
                        }
                    };
                    if distance < best {
                        result = other;
                        best = distance;
                    }
                }
            }
            i += 1;
            if i >= range {
                break;
            }
        }
    }
    result
}

/// `FindStorage`: where the citizen drops its load: its goal's storage, else a farm's nearest drop site within
/// 20, else the owner's nearest built drop site that takes the load (docks and navy yards for a boat).
pub fn find_storage<H: CzHost + ?Sized>(h: &mut H, u: u64, any_storage: bool) -> u64 {
    let s = h.unit(u);
    let mut farm = false;
    let mut result = 0u64;
    let mut own = 0u64;
    if s.goal_state != 0 && s.goal_kind == GOAL_GATHER {
        let goal = s.goal_state;
        if !any_storage {
            own = h.gather_storage(goal);
            if own != 0 {
                let o = h.unit(own);
                if o.hit_points > 0 && o.disabled == 0 {
                    result = own;
                }
            }
        }
        let resource = h.gather_resource(goal);
        if resource != 0 && h.unit(resource).unit_type == UNIT_TYPE_FARM {
            let farm_obj = h.unit(resource).complex;
            for site in h.farm_drop_sites(farm_obj) {
                let st = h.unit(site);
                let value = h.unit(u).resource_type;
                if !h.can_take(st.complex, u, value) {
                    h.empty_load(u, st.resource_type);
                }
                let st = h.unit(site);
                let sc = h.unit(st.complex);
                if st.hit_points <= 0 || sc.built == 0 || sc.disabled != 0 {
                    continue;
                }
                if result != 0 && h.distance_to_unit(u, site) >= h.distance_to_unit(u, result) {
                    continue;
                }
                if h.distance_to_unit(st.complex, resource) <= CITIZEN_FARM_RANGE {
                    farm = true;
                    result = st.complex;
                }
            }
        }
    }
    let land = s.category == CATEGORY_LAND_CITIZEN;
    if land && farm {
        return result;
    }
    for unit in h.player_units(s.player) {
        let x = h.unit(unit);
        let storage = x.complex;
        if storage == 0 {
            continue;
        }
        let st = h.unit(storage);
        let kind_ok = if land {
            st.drop_site != 0
        } else {
            st.unit_type == UNIT_TYPE_DOCK || st.unit_type == UNIT_TYPE_NAVY_YARD
        };
        if st.hit_points > 0
            && st.disabled == 0
            && kind_ok
            && storage != own
            && {
                let load = h.unit(u).resource_type;
                h.can_take(storage, u, load)
            }
            && st.built != 0
            && (result == 0 || h.distance_to_unit(u, unit) < h.distance_to(u, result))
        {
            result = storage;
        }
    }
    result
}

/// `IsTileHeld`: a unit on the tile other than a farm, robotic farm, path point, ambient unit or foundation.
fn is_tile_held<H: CzHost + ?Sized>(h: &mut H, tile: u64) -> bool {
    for unit in h.tile_units(tile) {
        if unit == 0 {
            continue;
        }
        let x = h.unit(unit);
        let t = x.unit_type;
        if t != UNIT_TYPE_FARM
            && t != UNIT_TYPE_ROBOTIC_FARM
            && t != UNIT_TYPE_PATH_POINT
            && x.category != CATEGORY_AMBIENT
            && (x.complex == 0 || h.unit(x.complex).foundation == 0)
        {
            return true;
        }
    }
    false
}

fn is_water(terrain: i32) -> bool {
    terrain == TERRAIN_DEEP_WATER || terrain == TERRAIN_SHALLOW_WATER
}

/// `FindGatherPoint`: where the citizen gathers or builds at a footprint: the free tile nearest it (a farm's own
/// tiles when farming, the corners when needed, not on a wall piece of the type being built), else the footprint's
/// corner; z from the map.
pub fn find_gather_point<H: CzHost + ?Sized>(h: &mut H, u: u64, x: u32, y: u32, size_x: i32, size_y: i32) -> [f32; 3] {
    let s = h.unit(u);
    let mut tiles: Vec<CzTile> = Vec::new();
    let gather = if s.goal_kind == GOAL_GATHER { s.goal_state } else { 0 };
    let build = if s.goal_kind == GOAL_BUILD_REPAIR { s.goal_state } else { 0 };
    let farm = if gather != 0 && h.is_gathering(gather) { h.gather_resource(gather) } else { 0 };
    let f = if farm != 0 { Some(h.unit(farm)) } else { None };
    match f {
        Some(f) if f.unit_type == UNIT_TYPE_FARM => {
            let mut fx = f.tile_x;
            while fx < f.tile_size.wrapping_add(f.tile_x) {
                let mut fy = f.tile_y;
                while fy < f.tile_size.wrapping_add(f.tile_y) {
                    tiles.push([fx, fy]);
                    fy += 1;
                }
                fx += 1;
            }
        }
        _ => h.square_ring(x as i32, y as i32, size_x, size_y, &mut tiles),
    }
    let mut keep = Vec::with_capacity(tiles.len());
    for t in tiles {
        let tile = h.tile(t[0], t[1]);
        if tile == 0 || is_tile_held(h, tile) || h.tile_terrain(tile) == TERRAIN_CLIFF {
            continue;
        }
        keep.push(t);
    }
    tiles = keep;
    let center = h.tile(x as i32, y as i32);
    let water = center != 0 && is_water(h.tile_terrain(center));
    if s.goal_state != 0 && s.goal_kind != GOAL_GATHER && (water || tiles.is_empty()) {
        h.ring_corners(0, x as i32, y as i32, size_x, size_y, &mut tiles);
        let mut keep = Vec::with_capacity(tiles.len());
        for t in tiles {
            let tile = h.tile(t[0], t[1]);
            if tile == 0 || is_tile_held(h, tile) {
                continue;
            }
            let terrain = h.tile_terrain(tile);
            if water && is_water(terrain) || terrain == TERRAIN_CLIFF {
                continue;
            }
            keep.push(t);
        }
        tiles = keep;
    }
    if build != 0 && h.build_target_is_wall(build) {
        let wall = h.build_target_type(build);
        let mut keep = Vec::with_capacity(tiles.len());
        for t in tiles {
            let tile = h.tile(t[0], t[1]);
            let mut erased = false;
            if tile != 0 {
                for unit in h.tile_units(tile) {
                    if unit != 0 && h.unit(unit).unit_type == wall {
                        erased = true;
                        break;
                    }
                }
            }
            if !erased {
                keep.push(t);
            }
        }
        tiles = keep;
    }
    if let Some(&first) = tiles.first() {
        let distance = |t: CzTile| {
            let dx = s.tile_x.wrapping_sub(t[0]);
            let dy = s.tile_y.wrapping_sub(t[1]);
            dy.wrapping_mul(dy).wrapping_add(dx.wrapping_mul(dx))
        };
        let mut nearest = first;
        let mut best = distance(first);
        for &t in &tiles[1..] {
            let d = distance(t);
            if d < best {
                nearest = t;
                best = d;
            }
        }
        let tile = h.tile(nearest[0], nearest[1]);
        [nearest[0] as f32 + 0.5, nearest[1] as f32 + 0.5, h.tile_height(tile)]
    } else {
        let tile = h.tile(x as i32, y as i32);
        [x as i32 as f32, y as i32 as f32, h.tile_height(tile)]
    }
}

/// `HasGatherPoint`: a gather point beside the resource (not just its own corner tile).
pub fn has_gather_point<H: CzHost + ?Sized>(h: &mut H, u: u64, resource: u64) -> bool {
    let r = h.unit(resource);
    let location = find_gather_point(h, u, r.tile_x as u32, r.tile_y as u32, r.tile_size, r.tile_size);
    !(ee_ge::um::floor(location[0]) == r.tile_x && ee_ge::um::floor(location[1]) == r.tile_y)
}

#[cfg(test)]
#[path = "citizen_tests.rs"]
mod tests;
