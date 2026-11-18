//! `EECPPlacement` whole (EECPPlacement.cpp, the original build manager's searches): the "unit near" tests, the site
//! tiles, `CanBuildAt`, `FindSite` and every finder, `IsNearAirportEnd`, `HasBuilderFor`, `UnitAdded`, the epoch's
//! classes, `EECPCountOwn` and the Lua verbs' `EECPNextPlaceSearch` with its searches.
//!
//! The class's own data (the dock and space site tiles, the classes seen) is Rust's ([`EeCpPlacement`], which the C++
//! object owns); the town centre tile stays the C++ member other files read (`mTownCenterTile`, through the host).
//! Every body is the original's statements in order: the same host calls (their TEST ONLY notes), the same f32
//! operations, `UMFloatToLong` as [`float_to_long`], the shuffles' draws through the C++ generator and the sorts with
//! ties through the C++ `std::sort` ([`sort_by_key`]).

use std::collections::BTreeSet;

use ee_base::random::float_to_long;

use super::place_host::{
    distance_squared, shuffle, sort_by_key, within, PlaceHost, Rect, RectFilter, Tile, CP_NO_TILE,
};
use super::resource_center as rc;

// the unit types, categories and constants the searches name (ee/EEUnitTypes.h and friends; kept out of the C
// header: their bare names would clash)
/// cbindgen:ignore
pub(crate) mod names {
    pub(crate) const CP_TYPE_CITIZEN: i32 = 0x20;
    pub(crate) const CP_TYPE_BARRACKS: i32 = 0x22;
    pub(crate) const CP_TYPE_TOWN_CENTER: i32 = 0x23;
    pub(crate) const CP_TYPE_DOCK: i32 = 0x24;
    pub(crate) const CP_TYPE_HOUSE: i32 = 0x25;
    pub(crate) const CP_TYPE_TEMPLE: i32 = 0x27;
    pub(crate) const CP_TYPE_CAPITOL: i32 = 0x29;
    pub(crate) const CP_TYPE_FARM: i32 = 0x2a;
    pub(crate) const CP_TYPE_SETTLEMENT: i32 = 0x2b;
    pub(crate) const CP_TYPE_GUARD_TOWER_PALEO: i32 = 0x2f;
    pub(crate) const CP_TYPE_GRANARY: i32 = 0x34;
    pub(crate) const CP_TYPE_ARCHERY_RANGE: i32 = 0x36;
    pub(crate) const CP_TYPE_FORAGE_BUSH: i32 = 0x37;
    pub(crate) const CP_TYPE_UNIVERSITY: i32 = 0x3c;
    pub(crate) const CP_TYPE_HOSPITAL: i32 = 0x3e;
    pub(crate) const CP_TYPE_SIEGE_FACTORY: i32 = 0x3f;
    pub(crate) const CP_TYPE_STABLE: i32 = 0x40;
    pub(crate) const CP_TYPE_NAVY_YARD: i32 = 0x41;
    pub(crate) const CP_TYPE_AIRPORT: i32 = 0x42;
    pub(crate) const CP_TYPE_MISSILE_BASE: i32 = 0x43;
    pub(crate) const CP_TYPE_TANK_FACTORY: i32 = 0x44;
    pub(crate) const CP_TYPE_CYBER_FACTORY: i32 = 0x45;
    pub(crate) const CP_TYPE_FISHING_BOAT_STONE: i32 = 0x6c;
    pub(crate) const CP_TYPE_AA_GUN_88MM: i32 = 0x91;
    pub(crate) const CP_TYPE_AA_MISSILE_TOWER: i32 = 0x93;
    pub(crate) const CP_TYPE_PATH_POINT: i32 = 0xdf;
    pub(crate) const CP_TYPE_AIRPORT_RUNWAY: i32 = 0xf0;
    pub(crate) const CP_TYPE_TANK_OBSTACLE: i32 = 0x148;
    pub(crate) const CP_TYPE_GUARD_TOWER_COPPER: i32 = 0x1cd;
    pub(crate) const CP_TYPE_GUARD_TOWER_BRONZE: i32 = 0x1ce;
    pub(crate) const CP_TYPE_GUARD_TOWER_MIDDLE: i32 = 0x1cf;
    pub(crate) const CP_TYPE_GUARD_TOWER_IMPERIAL: i32 = 0x1d0;
    pub(crate) const CP_TYPE_GUARD_TOWER_WW1: i32 = 0x1d1;
    pub(crate) const CP_TYPE_GUARD_TOWER_DIGITAL: i32 = 0x1d2;
    pub(crate) const CP_TYPE_CYBER_LABORATORY: i32 = 0x24a;
    pub(crate) const CP_TYPE_SPACE_DOCK: i32 = 0x2d8;
    pub(crate) const CP_TYPE_CP_MISSILE_BASE: i32 = 0x2ed;
    pub(crate) const CP_TYPE_PRIEST_TOWER: i32 = 0x2f0;
    pub(crate) const CP_TYPE_SPACE_TRANSPORT: i32 = 0x2f4;
    pub(crate) const CP_TYPE_GUARD_TOWER_SPACE: i32 = 0x2fd;
    pub(crate) const CP_TYPE_AA_SPACE: i32 = 0x300;
    pub(crate) const CP_TYPE_SPACE_TURRET: i32 = 0x302;
    pub(crate) const CP_TYPE_ORBITAL_STATION: i32 = 0x31f;
    pub(crate) const CP_TYPE_ROBOTIC_FARM: i32 = 0x33a;

    pub(crate) const CP_CATEGORY_LAND_CITIZEN: i32 = 6;
    pub(crate) const CP_CATEGORY_PROJECTILE: i32 = 9;
    pub(crate) const CP_CATEGORY_BOMB: i32 = 0xb;
    pub(crate) const CP_CATEGORY_MISSILE: i32 = 0xc;
    pub(crate) const CP_FAMILY_FISH: i32 = 0x12;
    pub(crate) const CP_RESOURCE_FOOD: i32 = 2;
    pub(crate) const CP_RESOURCE_GOLD: i32 = 5;
    pub(crate) const CP_RESOURCE_IRON: i32 = 6;
    pub(crate) const CP_TERRAIN_DEEP_WATER: i32 = 1;
    pub(crate) const CP_TERRAIN_SHALLOW_WATER: i32 = 10;
    pub(crate) const CP_TERRAIN_SPACE: i32 = 12;
    pub(crate) const CP_MAP_ISLANDS: u32 = 1;
    pub(crate) const CP_MAP_PLANETS: u32 = 4;
    /// `kTechNodeKindWonder`: a tech node of kind 4 builds a wonder
    pub(crate) const CP_NODE_WONDER: i32 = 4;

    /// `kNextPlaceRing` .. `kNextPlaceDock` (ee/EECPNext.h)
    pub(crate) const CP_PLACE_RING: i32 = 0;
    pub(crate) const CP_PLACE_TOWER: i32 = 1;
    pub(crate) const CP_PLACE_ADJACENT: i32 = 2;
    pub(crate) const CP_PLACE_CENTRE: i32 = 3;
    pub(crate) const CP_PLACE_CLASSIC: i32 = 4;
    pub(crate) const CP_PLACE_DOCK: i32 = 5;
    /// `kNextDockReach`, `kNextDockSpacing` (tiles)
    pub(crate) const CP_NEXT_DOCK_REACH: i64 = 30;
    pub(crate) const CP_NEXT_DOCK_SPACING: i64 = 12;
    /// FindCenterSite's `kHomeSiteRange` (tiles)
    pub(crate) const CP_HOME_SITE_RANGE: i64 = 20;
}
pub(crate) use names::*;

/// The class's own data (the C++ object owns it: `EECPPlacement::mRust`).
#[derive(Debug, Default)]
pub struct EeCpPlacement {
    /// `CollectSiteTiles(2)`'s tiles, `CollectSiteTiles(4)`'s
    pub water_sites: Vec<Tile>,
    pub space_sites: Vec<Tile>,
    /// `mSiteTilesCollected`
    pub site_tiles_collected: bool,
    /// `UnitAdded`'s classes (not saved)
    pub types_seen: BTreeSet<i32>,
}

// ---- the epoch's classes (the original civilization manager's)

/// `EECPAntiAirClass`
pub fn anti_air_class(epoch: i32) -> i32 {
    if epoch >= 9 {
        if epoch > 11 {
            if epoch > 13 {
                if epoch == 14 {
                    return CP_TYPE_AA_SPACE;
                }
                return 0;
            }
            return CP_TYPE_AA_MISSILE_TOWER;
        }
        return CP_TYPE_AA_GUN_88MM;
    }
    0
}

/// `EECPFishingBoatClass`
pub fn fishing_boat_class(epoch: i32) -> i32 {
    if epoch >= 0 {
        if epoch > 2 {
            if epoch > 8 {
                if epoch <= 14 {
                    return 0xd3;
                }
                return 0;
            }
            return 0x6d;
        }
        return 0x6c;
    }
    0
}

/// `EECPGateClass`
pub fn gate_class(epoch: i32) -> i32 {
    match epoch {
        0 | 1 => 0x2e,
        2 => 0x1d9,
        3 | 4 => 0x1c6,
        5 | 6 => 0x1bc,
        7 | 8 => 0x1bb,
        9..=11 => 0x1ba,
        12 | 13 => 0x126,
        14 => 0x2ff,
        _ => 0,
    }
}

/// `EECPTowerClass`
pub fn tower_class(epoch: i32) -> i32 {
    match epoch {
        0 | 1 => 0x2f,
        2 => 0x1cd,
        3 | 4 => 0x1ce,
        5 | 6 => 0x1cf,
        7 | 8 => 0x1d0,
        9..=11 => 0x1d1,
        12 | 13 => 0x1d2,
        14 => 0x2fd,
        _ => 0,
    }
}

/// `EECPWallClass`
pub fn wall_class(epoch: i32) -> i32 {
    match epoch {
        2 => 0x1d3,
        3 | 4 => 0x1d4,
        5 | 6 => 0x1d5,
        7 | 8 => 0x1d6,
        9..=11 => 0x1d7,
        12 | 13 => 0x1d8,
        14 => 0x2fe,
        _ => 0,
    }
}

/// `EECPIsSiteClass`: the dock, navy yard, space dock and the two space classes are built on a shore or space site.
pub fn is_site_class(class: i32) -> bool {
    class == 0x24 || class == 0x41 || class == 0x2d8 || class == 0x302 || class == 0x31f
}

fn is_anti_air(class: i32) -> bool {
    class == CP_TYPE_AA_GUN_88MM || class == CP_TYPE_AA_MISSILE_TOWER || class == CP_TYPE_AA_SPACE
}

/// `EECPCountOwn`: the player's living units of a class.
pub fn count_own<H: PlaceHost + ?Sized>(h: &mut H, unit_type: i32) -> i32 {
    let mut n = 0;
    for u in h.own_units() {
        let i = h.unit(u);
        if i.unit_type == unit_type && i.complex != 0 && i.hit_points > 0 {
            n += 1;
        }
    }
    n
}

// ---- the "unit near" tests

fn any_within<H: PlaceHost + ?Sized>(h: &mut H, units: &[u64], center: Tile, squared: f32) -> bool {
    for &u in units {
        if within(h.unit(u).tile, center, squared) {
            return true;
        }
    }
    false
}

fn of_type<H: PlaceHost + ?Sized>(h: &mut H, unit_type: i32) -> Vec<u64> {
    let mut units = Vec::new();
    let id = h.player_id();
    h.units_of_type(unit_type, id, &mut units);
    units
}

/// `HasUnitNear`: a unit of the type within the squared distance of the tile.
pub fn has_unit_near<H: PlaceHost + ?Sized>(h: &mut H, unit_type: i32, t: Tile, squared: f32) -> bool {
    let units = of_type(h, unit_type);
    any_within(h, &units, t, squared)
}

/// `HasAntiAirNear`: an anti-air building covers the tile (within its range).
pub fn has_anti_air_near<H: PlaceHost + ?Sized>(h: &mut H, t: Tile) -> bool {
    let epoch = h.epoch();
    let class = anti_air_class(epoch);
    let first = h.range(class);
    let second = h.range(class);
    let units = of_type(h, class);
    any_within(h, &units, t, second * first)
}

/// `HasTownCenterNear`: a settlement, town centre or capitol within the distance.
pub fn has_town_center_near<H: PlaceHost + ?Sized>(h: &mut H, t: Tile, distance: f32) -> bool {
    let mut units = of_type(h, CP_TYPE_SETTLEMENT);
    units.extend(of_type(h, CP_TYPE_TOWN_CENTER));
    units.extend(of_type(h, CP_TYPE_CAPITOL));
    any_within(h, &units, t, distance * distance)
}

/// `HasGateNear`: a gate within the radius.
pub fn has_gate_near<H: PlaceHost + ?Sized>(h: &mut H, t: Tile, radius: f32) -> bool {
    let squared = radius * radius;
    let epoch = h.epoch();
    let units = of_type(h, gate_class(epoch));
    any_within(h, &units, t, squared)
}

/// `HasTowerNear`: a tower within the squared distance (the argument is already squared).
pub fn has_tower_near<H: PlaceHost + ?Sized>(h: &mut H, t: Tile, squared: f32) -> bool {
    let epoch = h.epoch();
    let units = of_type(h, tower_class(epoch));
    any_within(h, &units, t, squared)
}

/// `HasWallNear`: a wall within the squared distance.
pub fn has_wall_near<H: PlaceHost + ?Sized>(h: &mut H, t: Tile, squared: f32) -> bool {
    let epoch = h.epoch();
    let units = of_type(h, wall_class(epoch));
    any_within(h, &units, t, squared)
}

/// `HasEnemyNear`: a real threat the player sees or remembers (`EECPNextSiteThreat`).
pub fn has_enemy_near<H: PlaceHost + ?Sized>(h: &mut H, t: Tile) -> bool {
    h.site_threat(t)
}

// ---- the site tiles

/// `CollectSiteTiles`: the dock tiles (kind 2: water) or space dock tiles (4: space) on the largest such area next to
/// the town centre's continent, nearest the town first; then the tiles are marked collected.
pub fn collect_site_tiles<H: PlaceHost + ?Sized>(h: &mut H, s: &mut EeCpPlacement, kind: i32) {
    let center = h.town();
    if !h.has_map() || center[0] == CP_NO_TILE || center[1] == CP_NO_TILE {
        return;
    }
    let kind_id = h.largest_continent(kind) as u16;
    let kind_continent = h.continent_from_id(kind_id);
    let continent = h.continent_at(center);
    if continent == 0 {
        return;
    }
    let id = h.continent_id(continent);
    if h.continent_flag(kind_id, id) {
        let mut tiles = std::mem::take(if kind == 2 { &mut s.water_sites } else { &mut s.space_sites });
        h.shore_next_to(kind_continent, id, &mut tiles);
        let region = tiles.clone();
        for t in &region {
            let ring = h.square_ring_tiles(*t, 1);
            tiles.extend(ring);
        }
        // U2DPoint's operator< (lower y first, then larger x): a total order, so the sort and unique are any sort's
        tiles.sort_by(|a, b| a[1].cmp(&b[1]).then(b[0].cmp(&a[0])));
        tiles.dedup();
        let mut kept = Vec::with_capacity(tiles.len());
        for t in tiles {
            let ok = match h.tile_terrain(t) {
                Some(terrain) if kind == 2 => terrain == CP_TERRAIN_DEEP_WATER || terrain == CP_TERRAIN_SHALLOW_WATER,
                Some(terrain) => terrain == CP_TERRAIN_SPACE,
                None => false,
            };
            if ok {
                kept.push(t);
            }
        }
        sort_by_key(h, &mut kept, |t| distance_squared(center, *t));
        *(if kind == 2 { &mut s.water_sites } else { &mut s.space_sites }) = kept;
    }
    s.site_tiles_collected = true;
}

/// `Restore`: the site tiles collected again after a load.
pub fn restore<H: PlaceHost + ?Sized>(h: &mut H, s: &mut EeCpPlacement) -> i32 {
    s.water_sites.clear();
    collect_site_tiles(h, s, 2);
    s.space_sites.clear();
    collect_site_tiles(h, s, 4);
    0
}

/// `Reset`: no town centre tile, no class seen.
pub fn reset<H: PlaceHost + ?Sized>(h: &mut H, s: &mut EeCpPlacement) {
    h.set_town([CP_NO_TILE, CP_NO_TILE]);
    s.types_seen.clear();
}

/// `UnitAdded`: the first unit of a class that is not a land building sets the town centre tile while there is none.
pub fn unit_added<H: PlaceHost + ?Sized>(h: &mut H, s: &mut EeCpPlacement, unit: u64) {
    if unit == 0 {
        return;
    }
    let i = h.unit(unit);
    if i.has_class == 0 || i.hit_points <= 0 {
        return;
    }
    if i.category == CP_CATEGORY_PROJECTILE
        || i.category == CP_CATEGORY_BOMB
        || i.category == CP_CATEGORY_MISSILE
        || i.unit_type == CP_TYPE_PATH_POINT
    {
        return;
    }
    if !s.types_seen.insert(i.unit_type) {
        return;
    }
    let town = h.town();
    if town[0] == CP_NO_TILE && town[1] == CP_NO_TILE && i.land_building == 0 {
        h.set_town(i.tile);
    }
}

// ---- CanBuildAt

/// `EECPBMCannotStandOn`: off the map or the class cannot stand on the terrain.
fn cannot_stand_on<H: PlaceHost + ?Sized>(h: &mut H, t: Tile, class: i32) -> bool {
    let terrain = if h.has_map() { h.tile_terrain(t) } else { None };
    !matches!(terrain, Some(terrain) if h.class_terrain(class, terrain))
}

/// `CanBuildAt`: free ground on the right (or a reachable) continent, no enemies; strictly also no choke point, hill,
/// bush, runway end, town centre, gate near and walkable ground around it but for one run (at least 3 tiles for the
/// site classes, which need a side or corner tile on the continent).
#[allow(clippy::too_many_arguments)]
pub fn can_build_at<H: PlaceHost + ?Sized>(
    h: &mut H,
    continent: u16,
    class: i32,
    t: Tile,
    size: i32,
    margin: bool,
    strict: bool,
    from: Tile,
) -> bool {
    let (x, y) = (t[0], t[1]);
    let mut result = if margin {
        h.block_takes(class, x - 1, y - 1, size + 2, size + 2)
    } else {
        h.block_takes(class, x, y, size, size)
    };
    if result && !is_site_class(class) && !is_anti_air(class) {
        if h.has_pathfinding() && (from[0] != -1 || from[1] != -1) {
            let id = h.continent_id_at(from);
            let start = if id != 0 { h.continent_by_id(id) } else { 0 };
            if start != 0 {
                let to = h.continent_at(t);
                let flags = h.passability();
                result = h.are_connected(start, to, flags);
            } else {
                result = h.tile_continent(t) == continent;
            }
        } else {
            result = h.tile_continent(t) == continent;
        }
    }
    if result && is_site_class(class) {
        let tiles = h.side_tiles([x, y], size);
        result = tiles.iter().any(|&s| h.tile_continent(s) == continent);
        if !result {
            let tiles = h.corner_tiles([x, y], size);
            result = tiles.iter().any(|&s| h.tile_continent(s) == continent);
        }
    }
    if !result {
        return false;
    }
    result = !has_enemy_near(h, [x, y]);
    if !(result && strict) {
        return result;
    }
    let open = h.has_map() && {
        let flags = h.map_flags();
        flags & CP_MAP_ISLANDS == CP_MAP_ISLANDS || flags & CP_MAP_PLANETS == CP_MAP_PLANETS
    };
    if !open {
        result = !h.choke_at(t);
    }
    if !result {
        return false;
    }
    result = !h.hill_at(t);
    if !result {
        return false;
    }
    if class == CP_TYPE_FARM || class == CP_TYPE_ROBOTIC_FARM {
        result = !h.tile_unit_of_type(t, CP_TYPE_FORAGE_BUSH);
    }
    if !result {
        return false;
    }
    if count_own(h, CP_TYPE_AIRPORT) > 0 {
        result = !is_near_airport_end(h, [x, y], size, 2.0);
    }
    if !result {
        return false;
    }
    if !is_site_class(class) {
        result = !has_town_center_near(h, [x, y], 3.0);
    }
    if !result {
        return false;
    }
    if !open {
        result = !has_gate_near(h, [x, y], 4.0);
    }
    if result
        && class != CP_TYPE_GRANARY
        && class != CP_TYPE_FARM
        && class != CP_TYPE_ROBOTIC_FARM
        && !is_anti_air(class)
    {
        result = border_open(h, class, t, size);
    }
    result
}

/// CanBuildAt's border: the ring of tiles round the footprint must be walkable for the builders (a fishing boat at a
/// dock, a space transport at the space classes, a citizen elsewhere) but for one run, which the site classes need
/// to be at least 3 tiles long.
fn border_open<H: PlaceHost + ?Sized>(h: &mut H, class: i32, t: Tile, size: i32) -> bool {
    let mut tiles: Vec<Tile> = Vec::new();
    let mut corner = [t[0] - 1, t[1] - 1];
    tiles.push(corner);
    for _ in 0..size + 1 {
        corner[1] += 1;
        tiles.push(corner);
    }
    for _ in 0..size + 1 {
        corner[0] += 1;
        tiles.push(corner);
    }
    for _ in 0..size + 1 {
        corner[1] -= 1;
        tiles.push(corner);
    }
    for _ in 0..size {
        corner[0] -= 1;
        tiles.push(corner);
    }
    let stand = if class == CP_TYPE_DOCK || class == CP_TYPE_NAVY_YARD {
        CP_TYPE_FISHING_BOAT_STONE
    } else if class == CP_TYPE_SPACE_DOCK || class == CP_TYPE_SPACE_TURRET || class == CP_TYPE_ORBITAL_STATION {
        CP_TYPE_SPACE_TRANSPORT
    } else {
        CP_TYPE_CITIZEN
    };
    let mut j = 0;
    while cannot_stand_on(h, tiles[0], stand) && j < tiles.len() {
        tiles.rotate_left(1);
        j += 1;
    }
    j = 0;
    while !cannot_stand_on(h, tiles[0], stand) && j < tiles.len() {
        tiles.rotate_left(1);
        j += 1;
    }
    let mut count = 0;
    let at = 0;
    while at < tiles.len() {
        if !cannot_stand_on(h, tiles[at], stand) {
            break;
        }
        tiles.remove(at);
        if is_site_class(class) {
            count += 1;
        }
    }
    let mut result = true;
    if is_site_class(class) {
        result = count >= 3;
    }
    let mut k = 0;
    while k < tiles.len() && result {
        result = !cannot_stand_on(h, tiles[k], stand);
        k += 1;
    }
    result
}

/// `IsNearAirportEnd`: a square of `size` at the tile lies within the distance of the far end of an airport's runway.
pub fn is_near_airport_end<H: PlaceHost + ?Sized>(h: &mut H, t: Tile, size: i32, distance: f32) -> bool {
    let center = [t[0] + size / 2, size / 2 + t[1]];
    let mut gate: Tile = [0, 0];
    let gate_size = h.class_tile_size(CP_TYPE_AIRPORT_RUNWAY);
    let half = float_to_long(gate_size as f32 * 0.5);
    let mut result = false;
    let range = (half as f32 + distance) * (half as f32 + distance);
    let units = of_type(h, CP_TYPE_AIRPORT);
    let mut i = 0;
    while i < units.len() && !result {
        let wall = h.unit(units[i]).complex;
        let wall_info = h.unit(wall);
        let mut last = wall_info.next_part;
        loop {
            let next = h.unit(last).next_part;
            if next == 0 {
                break;
            }
            last = next;
        }
        let end = h.unit(last).tile;
        let (mut x, y) = (end[0], end[1]);
        match wall_info.facing {
            0 => gate = [x + half, y + gate_size],
            1 => gate = [x + half, y],
            2 => {
                x += gate_size;
                gate = [x, y + half];
            }
            3 => gate = [x, y + half],
            _ => {}
        }
        // the original's `> range ? false : true`
        #[allow(clippy::neg_cmp_op_on_partial_ord)]
        let near = !((distance_squared(center, gate) as f32) > range);
        result = near;
        i += 1;
    }
    result
}

/// `HasBuilderFor`: one of the player's living citizens on the map could build the class at the tile (the site's path
/// area, or for the site classes and towers a shore of its continent touching the footprint).
pub fn has_builder_for<H: PlaceHost + ?Sized>(h: &mut H, t: Tile, class: i32) -> bool {
    let area = h.path_area_at(t);
    let edge = h.has_map() && h.is_edge(t);
    for u in h.own_units() {
        let i = h.unit(u);
        if i.complex == 0
            || i.category != CP_CATEGORY_LAND_CITIZEN
            || i.hit_points <= 0
            || i.complex_tile[0] == CP_NO_TILE
        {
            continue;
        }
        if edge || h.unit_path_area(i.complex) == area {
            return true;
        }
        let epoch = h.epoch();
        if is_site_class(class) || class == tower_class(epoch) {
            let size = h.class_tile_size(class);
            let continent = h.continent_at(i.complex_tile);
            if continent == 0 {
                continue;
            }
            let shore = h.shore_tiles(continent);
            for x in t[0]..t[0] + size {
                for y in t[1]..t[1] + size {
                    if shore.contains(&[x, y]) {
                        return true;
                    }
                }
            }
        }
    }
    false
}

// ---- the finders

/// `FindAirportSite`: room for 4 (else 3) runway pieces facing away from the town centre, rings 5..99 round it,
/// reachable and with no enemies near.
pub fn find_airport_site<H: PlaceHost + ?Sized>(h: &mut H, out: &mut Tile) -> bool {
    let mut result = false;
    let center = h.town();
    if center[0] == CP_NO_TILE || center[1] == CP_NO_TILE {
        return false;
    }
    let size = h.class_tile_size(CP_TYPE_AIRPORT);
    let wall = h.class_tile_size(CP_TYPE_AIRPORT_RUNWAY);
    let mut length = size + wall * 4 + 2;
    let offset = -2 - size;
    let mut k = 4;
    while k >= 3 && !result {
        let mut r = 5;
        while r < 100 && !result {
            let tiles = h.square_ring_tiles(center, r);
            for &t in &tiles {
                if result {
                    break;
                }
                let (x, y) = (t[0], t[1]);
                if h.tile_terrain(t).is_none() {
                    continue;
                }
                let from = h.continent_at(center);
                let to = h.continent_at(t);
                let flags = h.passability();
                if !h.are_connected(from, to, flags) {
                    continue;
                }
                match h.facing_quarter(t) {
                    0 => result = h.block_takes(CP_TYPE_AIRPORT, x - 1, y - 1, size + 2, length),
                    1 => result = h.block_takes(CP_TYPE_AIRPORT, x - 1, y - offset - length - 1, size + 2, length),
                    2 => result = h.block_takes(CP_TYPE_AIRPORT, x - 1, y - 1, length, size + 2),
                    3 => result = h.block_takes(CP_TYPE_AIRPORT, x - offset - length - 1, y - 1, length, size + 2),
                    _ => {}
                }
                if !result {
                    continue;
                }
                result = !has_enemy_near(h, t);
                if result {
                    *out = t;
                }
            }
            r += 1;
        }
        length -= wall;
        k -= 1;
    }
    result
}

/// `FindAntiAirSiteNear`: round a building (its tile and size).
pub fn find_anti_air_site_near<H: PlaceHost + ?Sized>(h: &mut H, unit: u64, out: &mut Tile) -> bool {
    let i = h.unit(unit);
    find_anti_air_site_around(h, i.tile, i.tile_size, out)
}

/// `FindAntiAirSiteAround`: square rings growing out from a tile up to the anti-air range, on the tile's continent.
pub fn find_anti_air_site_around<H: PlaceHost + ?Sized>(h: &mut H, tile: Tile, size: i32, out: &mut Tile) -> bool {
    let continent = h.tile_continent(tile);
    let epoch = h.epoch();
    let class = anti_air_class(epoch);
    let mut result = false;
    let range = float_to_long(h.range(class));
    let (mut x, mut y) = (tile[0], tile[1]);
    let class_size = h.class_tile_size(class);
    let mut ring = size;
    let mut tiles: Vec<Tile> = Vec::new();
    let mut i = 0;
    while i < range && !result {
        h.square_block_ring([x, y], ring, &mut tiles);
        ring += 2;
        x -= 1;
        y -= 1;
        for &t in &tiles {
            if result {
                break;
            }
            result = can_build_at(h, continent, class, t, class_size + 1, false, true, tile);
            if result {
                *out = t;
            }
        }
        i += 1;
    }
    result
}

/// `FindAntiAirSite`: by a capitol not yet covered, then by a wonder, then in the biggest clusters of buildings.
pub fn find_anti_air_site<H: PlaceHost + ?Sized>(h: &mut H, out: &mut Tile) -> bool {
    let epoch = h.epoch();
    let class = anti_air_class(epoch);
    let mut result = false;
    let units = of_type(h, CP_TYPE_CAPITOL);
    for &u in &units {
        if result {
            break;
        }
        let t = h.unit(u).tile;
        result = !has_anti_air_near(h, t);
        if result {
            result = find_anti_air_site_near(h, u, out);
        }
    }
    if result {
        return true;
    }
    let mut wonders = Vec::new();
    for u in h.own_units() {
        let tech = h.unit(u).technology;
        if h.tech_kind(tech) == Some(CP_NODE_WONDER) {
            wonders.push(u);
        }
    }
    for &u in &wonders {
        if result {
            break;
        }
        let t = h.unit(u).tile;
        result = !has_anti_air_near(h, t);
        if result {
            result = find_anti_air_site_near(h, u, out);
        }
    }
    if result {
        return true;
    }
    let radius = float_to_long(h.range(class) * 0.5);
    let mut clusters: Vec<(Rect, u32)> = Vec::new();
    for u in h.own_units() {
        let i = h.unit(u);
        if i.land_building == 0
            || i.unit_type == CP_TYPE_AA_GUN_88MM
            || i.unit_type == CP_TYPE_AA_MISSILE_TOWER
            || i.wall != 0
        {
            continue;
        }
        if let Some(c) = clusters.iter_mut().find(|c| c.0.contains(i.tile)) {
            c.1 += 1;
        } else {
            let mut bounds = Rect::point(i.tile);
            bounds.grow(radius, true);
            clusters.push((bounds, 1));
        }
    }
    // EECPBMClusterGreater: the bigger first
    sort_by_key(h, &mut clusters, |c| -(c.1 as i64));
    for c in &clusters {
        if result {
            break;
        }
        let t = [c.0.left + radius, c.0.top + radius];
        result = !has_anti_air_near(h, t);
        if result {
            result = find_anti_air_site_around(h, t, 1, out);
        }
    }
    result
}

/// `EECPBMHomeDistance2`: a centre's squared tile distance from the town centre (1 << 40 with none or no unit).
pub fn home_distance2<H: PlaceHost + ?Sized>(h: &mut H, center: u64) -> i64 {
    let main = h.center_unit(center);
    let mut unit = h.resolve(main);
    if unit == 0 {
        let units = h.center_units(center);
        unit = match units.first() {
            Some(&id) => h.resolve(id),
            None => 0,
        };
    }
    let town = h.town();
    if unit == 0 || town[0] == CP_NO_TILE {
        return 1 << 40;
    }
    let t = h.unit(unit).tile;
    let dx = t[0].wrapping_sub(town[0]) as i64;
    let dy = t[1].wrapping_sub(town[1]) as i64;
    dx * dx + dy * dy
}

/// `EECPBMNearestHomeFirst`: the centres by their distance from the town centre (stable; none: unchanged).
pub fn nearest_home_first<H: PlaceHost + ?Sized>(h: &mut H, centers: &mut Vec<u64>) {
    if h.town()[0] == CP_NO_TILE {
        return;
    }
    let mut order: Vec<(i64, usize)> = Vec::with_capacity(centers.len());
    for (i, &c) in centers.iter().enumerate() {
        order.push((home_distance2(h, c), i));
    }
    order.sort();
    *centers = order.iter().map(|&(_, i)| centers[i]).collect();
}

/// `EECPBMCenterLess`'s count: the centre's units standing in an area effect of the kind for the player.
fn center_effect_count<H: PlaceHost + ?Sized>(h: &mut H, center: u64, kind: i32, player: u32) -> u32 {
    let mut n = 0;
    for id in h.center_units(center) {
        let u = h.resolve(id);
        let t = h.unit(u).tile;
        if h.has_area_effect(kind, t[0], t[1], player) {
            n += 1;
        }
    }
    n
}

/// `FindCenterSite`: a settlement at an unclaimed centre, else a temple, university or hospital at a claimed one
/// (those within `kHomeSiteRange` of the town centre first, then the rest, each by fewer units in matching area
/// effects and nearest first among equals).
pub fn find_center_site<H: PlaceHost + ?Sized>(h: &mut H, class: i32, out: &mut Tile) -> bool {
    let mut tile: Tile = [0, 0];
    let mut result = false;
    let all = h.centers();
    if class == CP_TYPE_SETTLEMENT {
        let mut centers = Vec::new();
        for &c in &all {
            if c != 0 {
                let main = h.center_unit(c);
                if h.resolve(main) == 0 {
                    centers.push(c);
                }
            }
        }
        for &c in &centers {
            if result {
                break;
            }
            result = rc::find_build_site(h, c, CP_TYPE_SETTLEMENT, &mut tile);
            if result {
                *out = tile;
            }
        }
        return result;
    }
    let resource = match class {
        CP_TYPE_TEMPLE => 3,
        CP_TYPE_UNIVERSITY => 4,
        CP_TYPE_HOSPITAL => 1,
        _ => 0,
    };
    let mut centers = Vec::new();
    for &c in &all {
        if c != 0 {
            let main = h.center_unit(c);
            if h.resolve(main) != 0 {
                centers.push(c);
            }
        }
    }
    if centers.is_empty() {
        return false;
    }
    nearest_home_first(h, &mut centers);
    let mut near = Vec::new();
    let mut far = Vec::new();
    for &c in &centers {
        if home_distance2(h, c) <= CP_HOME_SITE_RANGE * CP_HOME_SITE_RANGE {
            near.push(c);
        } else {
            far.push(c);
        }
    }
    let player = h.player_id();
    let keyed = |h: &mut H, list: Vec<u64>| {
        let mut k: Vec<(u32, u64)> =
            list.into_iter().map(|c| (center_effect_count(h, c, resource, player), c)).collect();
        k.sort_by_key(|&(n, _)| n);
        k.into_iter().map(|(_, c)| c).collect::<Vec<u64>>()
    };
    let mut ordered = keyed(h, near);
    ordered.extend(keyed(h, far));
    for &c in &ordered {
        if result {
            break;
        }
        result = rc::find_build_site(h, c, class, &mut tile);
        if result {
            *out = tile;
        }
    }
    result
}

/// `FindGranarySite`: just beyond a centre's drop-off distance with room for farms round it (with the claim range,
/// only round the centres within it, nearest home first, and only sites within it).
pub fn find_granary_site<H: PlaceHost + ?Sized>(h: &mut H, out: &mut Tile) -> bool {
    let mut result = false;
    let mut center: Tile = [0, 0];
    let radius = h.drop_off_distance();
    let size = h.class_tile_size(CP_TYPE_GRANARY);
    let claim = h.claim_range_on() && h.town()[0] != CP_NO_TILE;
    let mut centers = Vec::new();
    for c in h.centers() {
        if !claim || {
            let d = home_distance2(h, c);
            let range = h.claim_range() as i64;
            d <= range * h.claim_range() as i64
        } {
            centers.push(c);
        }
    }
    if claim {
        nearest_home_first(h, &mut centers);
    }
    for &c in &centers {
        if result {
            break;
        }
        rc::get_center(h, c, &mut center);
        let continent = h.tile_continent(center);
        let mut r = radius + 1;
        while r < radius + 0x1e && !result {
            let tiles = h.square_ring_tiles(center, r);
            for &t in &tiles {
                if result {
                    break;
                }
                result = can_build_at(h, continent, CP_TYPE_FARM, t, size * 3, false, true, center);
                if !result {
                    continue;
                }
                let site = [t[0] + size, t[1] + size];
                if claim {
                    let town = h.town();
                    let (dx, dy) = (site[0] - town[0], site[1] - town[1]);
                    let range = h.claim_range();
                    if dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) > range.wrapping_mul(h.claim_range()) {
                        result = false;
                        continue;
                    }
                }
                result = can_build_at(h, continent, CP_TYPE_GRANARY, site, size, false, true, center);
                if result {
                    *out = site;
                }
            }
            r += 1;
        }
    }
    result
}

/// `FindProductionSite`: at the active centres, nearest the town centre first.
pub fn find_production_site<H: PlaceHost + ?Sized>(h: &mut H, class: i32, out: &mut Tile) -> bool {
    let mut centers = Vec::new();
    for c in h.centers() {
        if c != 0 && h.center_active(c) {
            centers.push(c);
        }
    }
    nearest_home_first(h, &mut centers);
    for &c in &centers {
        if rc::find_build_site(h, c, class, out) {
            return true;
        }
    }
    false
}

/// `FindShoreSite`: a dock or navy yard among the water site tiles away from existing ones, with a builder.
pub fn find_shore_site<H: PlaceHost + ?Sized>(
    h: &mut H,
    s: &EeCpPlacement,
    class: i32,
    size: i32,
    out: &mut Tile,
) -> bool {
    let town = h.town();
    let continent = h.tile_continent(town);
    let mut result = false;
    let sites = s.water_sites.clone();
    if sites.is_empty() {
        return false;
    }
    let mut units = of_type(h, CP_TYPE_DOCK);
    units.extend(of_type(h, CP_TYPE_NAVY_YARD));
    let mut i = 0;
    while i < sites.len() && !result {
        let site = sites[i];
        i += 1;
        if any_within(h, &units, site, 75.0) {
            continue;
        }
        result = can_build_at(h, continent, class, site, size, false, true, town);
        if result && has_builder_for(h, site, class) {
            *out = site;
        } else {
            result = false;
        }
    }
    result
}

/// The (continent, town hull centre) pairs of the player's towns on a continent, by the continent's hull centre's
/// distance from `from` (`EETileDistanceLess`'s pair comparator).
fn town_continents<H: PlaceHost + ?Sized>(h: &mut H, from: Tile) -> Vec<(u64, Tile)> {
    let mut list = Vec::new();
    for town in h.towns().into_iter().flatten() {
        if town.0 != 0 {
            let c = h.continent_from_id(town.0);
            list.push((c, town.1));
        }
    }
    let mut keyed: Vec<(u64, Tile, i64)> = Vec::with_capacity(list.len());
    for (c, t) in list {
        let hull = h.hull_center(c, false);
        keyed.push((c, t, distance_squared(from, hull)));
    }
    sort_by_key(h, &mut keyed, |e| e.2);
    keyed.into_iter().map(|(c, t, _)| (c, t)).collect()
}

/// `FindSpaceSite`: a space dock, turret or station site among space tiles away from units of its class, with a
/// builder (the turret: the town continents' shore tiles on planet maps, else the town continent's shore near own
/// units; the dock keeps 75 from its kind and asks `EEIsSpaceDockSiteOpen`, its tile moved by -1, -1).
pub fn find_space_site<H: PlaceHost + ?Sized>(
    h: &mut H,
    s: &EeCpPlacement,
    class: i32,
    size: i32,
    out: &mut Tile,
) -> bool {
    let mut result = false;
    if s.space_sites.is_empty() {
        return false;
    }
    let town = h.town();
    let mut sites: Vec<Tile> = Vec::new();
    if class == CP_TYPE_SPACE_TURRET {
        if h.map_flags() & CP_MAP_PLANETS == CP_MAP_PLANETS {
            for (c, center) in town_continents(h, town) {
                let mut tiles = h.shore_tiles(c);
                sort_by_key(h, &mut tiles, |t| distance_squared(center, *t));
                sites.extend(tiles);
            }
        } else {
            let c = h.continent_at(town);
            let mut tiles = h.shore_tiles(c);
            sort_by_key(h, &mut tiles, |t| distance_squared(town, *t));
            let mut own = Vec::new();
            for u in h.own_units() {
                let i = h.unit(u);
                if i.unit_type != CP_TYPE_SPACE_TURRET {
                    own.push(i.tile);
                }
            }
            for &center in &tiles {
                // (the original sorted the own tiles by their distance and took the first: the smallest distance)
                if let Some(nearest) = own.iter().map(|&t| distance_squared(center, t)).min() {
                    if (nearest as u32) < 25 {
                        sites.push(center);
                    }
                }
            }
        }
    } else {
        sites = s.space_sites.clone();
    }
    if sites.is_empty() {
        return false;
    }
    let continent = h.tile_continent(town);
    let distance = if class == CP_TYPE_SPACE_DOCK { 75.0 } else { h.range(class) * h.range(class) };
    let units = of_type(h, class);
    let mut i = 0;
    while i < sites.len() && !result {
        let site = sites[i];
        i += 1;
        if any_within(h, &units, site, distance) {
            continue;
        }
        result = if class != CP_TYPE_SPACE_DOCK {
            can_build_at(h, continent, class, site, size, false, true, town)
        } else {
            h.space_dock_open(site)
        };
        if result && !has_builder_for(h, site, class) {
            result = false;
        }
        if !result {
            continue;
        }
        *out = site;
        if class == CP_TYPE_SPACE_DOCK {
            out[0] -= 1;
            out[1] -= 1;
        }
    }
    result
}

fn near_tower_or_priest<H: PlaceHost + ?Sized>(h: &mut H, class: i32, t: Tile, range: f32) -> bool {
    if class == CP_TYPE_PRIEST_TOWER {
        has_unit_near(h, CP_TYPE_PRIEST_TOWER, t, range)
    } else {
        has_tower_near(h, t, range)
    }
}

/// `FindTowerSite`: along the town continents' shores on island maps, then at a resource centre (the worked ones
/// first, nearest the town centre first), then near an open gate, then at a choke point near a town not yet guarded
/// by that class.
pub fn find_tower_site<H: PlaceHost + ?Sized>(h: &mut H, class: i32, size: i32, out: &mut Tile) -> bool {
    let has_map = h.has_map();
    let class_exists = h.class_exists(class);
    let epoch = h.epoch();
    let gate_type = gate_class(epoch);
    let player = h.player_id();
    let mut result = false;
    let town = h.town();
    if town[0] == CP_NO_TILE || town[1] == CP_NO_TILE {
        return false;
    }
    let r = h.range(class);
    let r2 = h.range(class);
    let range = (r * 1.5) * (r2 * 1.5);
    if has_map && h.map_flags() & CP_MAP_ISLANDS == CP_MAP_ISLANDS {
        for (c, hull) in town_continents(h, town) {
            if result {
                break;
            }
            let mut tiles = h.shore_tiles(c);
            sort_by_key(h, &mut tiles, |t| distance_squared(hull, *t));
            let mut i = 0;
            while i < tiles.len() && !result {
                let center = tiles[i];
                i += 1;
                if near_tower_or_priest(h, class, center, range) {
                    continue;
                }
                let mut ring_r = 1;
                while ring_r <= 2 && !result {
                    let ring = h.square_ring_tiles(center, ring_r);
                    let mut k = 0;
                    while k < ring.len() && !result {
                        let terrain = h.map_tile_terrain(ring[k]);
                        result = class_exists
                            && matches!(terrain, Some(terrain) if h.class_terrain(class, terrain))
                            && h.block_takes(class, ring[k][0], ring[k][1], size, size);
                        if result {
                            *out = ring[k];
                        }
                        k += 1;
                    }
                    ring_r += 1;
                }
            }
        }
    }
    if !result {
        let mut order: Vec<(i64, usize)> = Vec::new();
        let centers = h.centers();
        for (i, &c) in centers.iter().enumerate() {
            if c == 0 {
                continue;
            }
            let main = h.center_unit(c);
            let unit = h.resolve(main);
            if unit == 0 {
                continue;
            }
            let t = h.unit(unit).tile;
            let dx = t[0].wrapping_sub(town[0]) as i64;
            let dy = t[1].wrapping_sub(town[1]) as i64;
            let active = h.center_active(c);
            order.push(((if active { 0 } else { 1i64 << 40 }) + dx * dx + dy * dy, i));
        }
        order.sort();
        let mut site: Tile = [0, 0];
        let mut j = 0;
        while j < order.len() && !result {
            let c = centers[order[j].1];
            j += 1;
            result = rc::find_build_site(h, c, class, &mut site);
            if result {
                *out = site;
            }
        }
    }
    if !result {
        let mut gates = Vec::new();
        h.units_of_type(gate_type, player, &mut gates);
        let mut open = Vec::with_capacity(gates.len());
        for g in gates {
            let complex = h.unit(g).complex;
            let c = h.unit(complex);
            if c.previous_part == 0 && c.next_part != 0 {
                open.push(g);
            }
        }
        let mut keyed: Vec<(u64, Tile)> = Vec::with_capacity(open.len());
        for g in open {
            let t = h.unit(g).tile;
            keyed.push((g, t));
        }
        sort_by_key(h, &mut keyed, |e| distance_squared(town, e.1));
        let mut radius_tiles: Vec<Tile> = Vec::new();
        for &(_, gt) in &keyed {
            if result {
                break;
            }
            if near_tower_or_priest(h, class, gt, range) {
                continue;
            }
            h.tiles_in_radius(gt, 5, true, &mut radius_tiles);
            let mut kept = Vec::with_capacity(radius_tiles.len());
            for t in std::mem::take(&mut radius_tiles) {
                let drop =
                    has_gate_near(h, t, 2.0) || has_wall_near(h, t, 4.0) || near_tower_or_priest(h, class, t, range);
                if !drop {
                    kept.push(t);
                }
            }
            radius_tiles = kept;
            shuffle(h, &mut radius_tiles);
            let mut k = 0;
            while k < radius_tiles.len() && !result {
                let t = radius_tiles[k];
                k += 1;
                result = h.block_takes(class, t[0], t[1], size, size);
                if result {
                    *out = t;
                }
            }
        }
    }
    if !result {
        let at = h.continent_at(town);
        let id = h.continent_id(at);
        let chokes = h.choke_points(id);
        if !chokes.is_empty() {
            let mut keyed: Vec<(u64, Tile)> = Vec::with_capacity(chokes.len());
            for c in chokes {
                let hull = h.hull_center(c, true);
                keyed.push((c, hull));
            }
            sort_by_key(h, &mut keyed, |e| distance_squared(town, e.1));
            let towns = h.towns();
            let mut units: Vec<u64> = Vec::new();
            let mut ring: Vec<Tile> = Vec::new();
            let mut i = 0;
            while i < keyed.len() && !result {
                let (choke, hull) = keyed[i];
                i += 1;
                let mut near_town = false;
                let mut j = 0;
                while j < towns.len() && !near_town {
                    let center = towns[j].map(|t| t.1).unwrap_or([0, 0]);
                    near_town = distance_squared(center, hull) < 50;
                    j += 1;
                }
                if !near_town {
                    continue;
                }
                let mut area = h.hull_bounds(choke);
                let grow = float_to_long(h.range(class) * 2.0);
                area.grow(grow, true);
                h.units_in_rect(area, RectFilter::Any, &mut units);
                let mut taken = false;
                let mut k = 0;
                while k < units.len() && !taken {
                    let u = h.unit(units[k]);
                    taken = u.hit_points > 0 && u.unit_type == class;
                    k += 1;
                }
                if taken {
                    continue;
                }
                h.choke_point_tiles(choke, &mut ring);
                sort_by_key(h, &mut ring, |t| distance_squared(town, *t));
                let mut m = 0;
                while m < ring.len() && !result {
                    result = h.block_takes(class, ring[m][0], ring[m][1], size, size);
                    m += 1;
                }
                if result {
                    *out = ring[m - 1];
                }
            }
        }
    }
    result
}

/// `FindWonderSite`: a site with a margin of `size` in rings round the town centre (every ring is listed, as the
/// original's loop did after a find).
pub fn find_wonder_site<H: PlaceHost + ?Sized>(h: &mut H, class: i32, size: u32, out: &mut Tile) -> bool {
    let class_size = h.class_tile_size(class);
    let mut result = false;
    let center = h.town();
    if center[0] == CP_NO_TILE || center[1] == CP_NO_TILE {
        return false;
    }
    let continent = h.tile_continent(center);
    for r in 1..200 {
        let tiles = h.square_ring_tiles(center, r);
        if tiles.is_empty() {
            break;
        }
        let mut i = 0;
        while i < tiles.len() && !result {
            let t = tiles[i];
            i += 1;
            result = can_build_at(h, continent, class, t, class_size.wrapping_add(size as i32), false, true, center);
            if result {
                *out = [t[0] + (size >> 1) as i32, (size >> 1) as i32 + t[1]];
            }
        }
    }
    result
}

/// `FindSite`: a building site by the class's kind (wonder, granary, dock, centre, airport, tower, production,
/// anti-air, space) or, for farms, an unbuilt farm foundation.
pub fn find_site<H: PlaceHost + ?Sized>(h: &mut H, s: &EeCpPlacement, class: i32, tile: &mut Tile) -> bool {
    let exists = h.class_exists(class);
    let tech = if exists { h.class_technology(class) } else { 100000 };
    let kind = h.tech_kind(tech);
    let mut result = false;
    if !exists {
        return false;
    }
    if kind == Some(CP_NODE_WONDER) {
        let mut size: u32 = 10;
        while size != 0 && !result {
            result = find_wonder_site(h, class, size, tile);
            size -= 2;
        }
        return result;
    }
    let class_size = h.class_tile_size(class);
    match class {
        CP_TYPE_GRANARY => find_granary_site(h, tile),
        CP_TYPE_DOCK | CP_TYPE_NAVY_YARD => find_shore_site(h, s, class, class_size, tile),
        CP_TYPE_TEMPLE | CP_TYPE_SETTLEMENT | CP_TYPE_UNIVERSITY | CP_TYPE_HOSPITAL => find_center_site(h, class, tile),
        CP_TYPE_AIRPORT => find_airport_site(h, tile),
        CP_TYPE_GUARD_TOWER_PALEO
        | CP_TYPE_TANK_OBSTACLE
        | CP_TYPE_GUARD_TOWER_COPPER
        | CP_TYPE_GUARD_TOWER_BRONZE
        | CP_TYPE_GUARD_TOWER_MIDDLE
        | CP_TYPE_GUARD_TOWER_IMPERIAL
        | CP_TYPE_GUARD_TOWER_WW1
        | CP_TYPE_GUARD_TOWER_DIGITAL
        | CP_TYPE_PRIEST_TOWER
        | CP_TYPE_GUARD_TOWER_SPACE => find_tower_site(h, class, class_size, tile),
        CP_TYPE_BARRACKS
        | CP_TYPE_ARCHERY_RANGE
        | CP_TYPE_SIEGE_FACTORY
        | CP_TYPE_STABLE
        | CP_TYPE_MISSILE_BASE
        | CP_TYPE_TANK_FACTORY
        | CP_TYPE_CYBER_FACTORY
        | CP_TYPE_CYBER_LABORATORY
        | CP_TYPE_CP_MISSILE_BASE => find_production_site(h, class, tile),
        CP_TYPE_AA_GUN_88MM | CP_TYPE_AA_MISSILE_TOWER | CP_TYPE_AA_SPACE => find_anti_air_site(h, tile),
        CP_TYPE_SPACE_DOCK | CP_TYPE_SPACE_TURRET | CP_TYPE_ORBITAL_STATION => {
            find_space_site(h, s, class, class_size, tile)
        }
        CP_TYPE_FARM | CP_TYPE_ROBOTIC_FARM => {
            for u in h.own_units() {
                if result {
                    break;
                }
                let i = h.unit(u);
                if i.unit_type != class || i.complex == 0 || i.complex_built != 0 || i.complex_hit_points != 1 {
                    continue;
                }
                *tile = i.complex_tile;
                result = true;
            }
            result
        }
        _ => false,
    }
}

// ---- the Lua verbs' searches (EECPNextPlaceSearch)

/// `EECPNextRingSite`: the first tile in the rings `first..=last` round a tile where the class can be built.
fn next_ring_site<H: PlaceHost + ?Sized>(
    h: &mut H,
    class: i32,
    at: Tile,
    first: i32,
    last: i32,
    out: &mut Tile,
) -> bool {
    let size = h.class_tile_size(class);
    let continent = h.tile_continent(at);
    for ring in first..=last {
        if ring == 0 {
            if can_build_at(h, continent, class, at, size, true, true, at) {
                *out = at;
                return true;
            }
            continue;
        }
        let tiles = h.square_ring_tiles(at, ring);
        for &t in &tiles {
            if can_build_at(h, continent, class, t, size, true, true, at) {
                *out = t;
                return true;
            }
        }
    }
    false
}

/// `EECPNextTowerRingSite`: a tower's tile in the rings with none of the player's towers within its range.
fn next_tower_ring_site<H: PlaceHost + ?Sized>(
    h: &mut H,
    class: i32,
    at: Tile,
    first: i32,
    last: i32,
    out: &mut Tile,
) -> bool {
    let size = h.class_tile_size(class);
    let continent = h.tile_continent(at);
    let spacing = h.range(class);
    for ring in first..=last {
        let tiles = h.square_ring_tiles(at, ring);
        for &t in &tiles {
            if !has_tower_near(h, t, spacing * spacing) && can_build_at(h, continent, class, t, size, true, true, at) {
                *out = t;
                return true;
            }
        }
    }
    false
}

/// `EECPNextGap`: the tiles between a building of `size` at a tile (its corner) and a unit's footprint (0: touching).
fn next_gap(t: Tile, size: i32, unit: Tile, unit_size: i32) -> i32 {
    let unit_size = if unit_size > 0 { unit_size } else { 1 };
    let dx = (unit[0] - (t[0] + size)).max(t[0] - (unit[0] + unit_size)).max(0);
    let dy = (unit[1] - (t[1] + size)).max(t[1] - (unit[1] + unit_size)).max(0);
    dx.max(dy)
}

/// `EECPNextAdjacentSite`: of the tiles in the rings `1..=last` round a tile where the class can be built, the one
/// nearest the resource units (the tile itself when none), then nearest the town centre, then the first in the
/// rings' order.
fn next_adjacent_site<H: PlaceHost + ?Sized>(
    h: &mut H,
    class: i32,
    at: Tile,
    last: i32,
    units: &[(Tile, i32)],
    out: &mut Tile,
    out_gap: &mut i32,
) -> bool {
    let class_size = h.class_tile_size(class);
    let size = if class_size > 0 { class_size } else { 1 };
    let town = h.town();
    let continent = h.tile_continent(town);
    let (x, y) = (at[0], at[1]);
    let mut found = false;
    let mut best_gap = 0;
    let mut best_home: i64 = 0;
    for ring in 1..=last {
        let tiles = h.square_ring_tiles([x - size / 2, y - size / 2], ring);
        for &t in &tiles {
            let mut gap = 1000;
            if units.is_empty() {
                let dx = (x - (t[0] + size - 1)).max(t[0] - x).max(0);
                let dy = (y - (t[1] + size - 1)).max(t[1] - y).max(0);
                gap = dx.max(dy);
            }
            for &(ut, us) in units {
                gap = gap.min(next_gap(t, size, ut, us));
            }
            if found && gap > best_gap {
                continue;
            }
            let dx = (t[0] - town[0]) as i64;
            let dy = (t[1] - town[1]) as i64;
            let home = dx * dx + dy * dy;
            if found && gap == best_gap && home >= best_home {
                continue;
            }
            if !can_build_at(h, continent, class, t, size, true, true, at) {
                continue;
            }
            found = true;
            best_gap = gap;
            best_home = home;
            *out = t;
            *out_gap = gap;
        }
        if found && best_gap <= 1 && ring >= best_gap + 2 {
            break;
        }
    }
    found
}

/// `EECPNextDockFishSite`: the dock's tile by the nearest fish within `kNextDockReach` of the town centre with none of
/// the player's docks within `kNextDockSpacing` of it; the fish's tile out.
fn next_dock_fish_site<H: PlaceHost + ?Sized>(h: &mut H, out: &mut Tile, fish: &mut Tile) -> bool {
    let town = h.town();
    if town[0] == CP_NO_TILE || !h.class_exists(CP_TYPE_DOCK) {
        return false;
    }
    let Some(nature) = h.nature_units() else {
        return false;
    };
    let mut docks: Vec<Tile> = Vec::new();
    for u in h.own_units() {
        let i = h.unit(u);
        if i.unit_type == CP_TYPE_DOCK && i.hit_points > 0 {
            docks.push(i.tile);
        }
    }
    let class_size = h.class_tile_size(CP_TYPE_DOCK);
    let size = if class_size > 0 { class_size } else { 1 };
    let continent = h.tile_continent(town);
    let mut best: Option<Tile> = None;
    let mut best_d2: i64 = 0;
    for f in nature {
        let i = h.unit(f);
        if i.family != CP_FAMILY_FISH
            || i.resource_type != CP_RESOURCE_FOOD
            || i.resource_amount <= 0
            || i.has_tile == 0
        {
            continue;
        }
        let ft = i.tile;
        let dx = (ft[0] - town[0]) as i64;
        let dy = (ft[1] - town[1]) as i64;
        let d2 = dx * dx + dy * dy;
        if d2 > CP_NEXT_DOCK_REACH * CP_NEXT_DOCK_REACH || (best.is_some() && d2 >= best_d2) {
            continue;
        }
        let served = docks.iter().any(|d| {
            let (x, y) = ((d[0] - ft[0]) as i64, (d[1] - ft[1]) as i64);
            x * x + y * y <= CP_NEXT_DOCK_SPACING * CP_NEXT_DOCK_SPACING
        });
        if served {
            continue;
        }
        let mut shore = false;
        let mut ring = 1;
        while ring <= 6 && !shore {
            let tiles = h.square_ring_tiles(ft, ring);
            let mut k = 0;
            while k < tiles.len() && !shore {
                shore = can_build_at(h, continent, CP_TYPE_DOCK, tiles[k], size, false, true, town);
                k += 1;
            }
            ring += 1;
        }
        if shore {
            best = Some(ft);
            best_d2 = d2;
        }
    }
    let Some(ft) = best else {
        return false;
    };
    let mut found = false;
    let mut best_home: i64 = 0;
    let mut ring = 1;
    while ring <= 6 && !found {
        let tiles = h.square_ring_tiles(ft, ring);
        for &t in &tiles {
            if !can_build_at(h, continent, CP_TYPE_DOCK, t, size, false, true, town) {
                continue;
            }
            let dx = (t[0] - town[0]) as i64;
            let dy = (t[1] - town[1]) as i64;
            if !found || dx * dx + dy * dy < best_home {
                found = true;
                best_home = dx * dx + dy * dy;
                *out = t;
            }
        }
        ring += 1;
    }
    *fish = ft;
    found
}

/// What `EECPNextPlaceSearch` returns: found, x, y, gap.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCpPlaceResult {
    pub found: u8,
    pub x: i32,
    pub y: i32,
    pub gap: i32,
}

/// `EECPNextPlaceSearch`: a building site by one of the verbs' rules (ring, tower, adjacent, centre, classic, dock).
#[allow(clippy::too_many_arguments)]
pub fn next_place_search<H: PlaceHost + ?Sized>(
    h: &mut H,
    s: Option<&EeCpPlacement>,
    class: i32,
    mode: i32,
    at: Tile,
    first: i32,
    last: i32,
    units: &[i64],
    site: i32,
) -> EeCpPlaceResult {
    let mut r = EeCpPlaceResult { found: 0, x: -1, y: -1, gap: -1 };
    if class < 0 || class as u32 >= h.class_count() || !h.class_exists(class) || s.is_none() || !h.has_map() {
        return r;
    }
    let s = s.unwrap();
    let mut tile: Tile = [0, 0];
    let found = match mode {
        CP_PLACE_RING => next_ring_site(h, class, at, first, last, &mut tile),
        CP_PLACE_TOWER => next_tower_ring_site(h, class, at, first, last, &mut tile),
        CP_PLACE_ADJACENT => {
            let mut list: Vec<(Tile, i32)> = Vec::new();
            for &id in units {
                let u = h.world_unit(id as u32);
                if u == 0 {
                    continue;
                }
                let i = h.unit(u);
                if i.valid != 0 && i.has_tile != 0 {
                    list.push((i.tile, i.tile_size));
                }
            }
            next_adjacent_site(h, class, at, last, &list, &mut tile, &mut r.gap)
        }
        CP_PLACE_CENTRE => {
            let centers = h.centers();
            if site < 0 || site as usize >= centers.len() {
                false
            } else {
                let c = centers[site as usize];
                c != 0 && rc::find_build_site(h, c, class, &mut tile)
            }
        }
        CP_PLACE_CLASSIC => find_site(h, s, class, &mut tile),
        CP_PLACE_DOCK => {
            let mut fish: Tile = [-1, -1];
            let found = next_dock_fish_site(h, &mut tile, &mut fish);
            r.gap = if fish[0] >= 0 { fish[0].wrapping_mul(65536).wrapping_add(fish[1]) } else { -1 };
            found
        }
        _ => return r,
    };
    if !found || (tile[0] == 0 && tile[1] == 0) {
        return r;
    }
    r.found = 1;
    r.x = tile[0];
    r.y = tile[1];
    r
}

/// `SetTownCenter`: the unit's tile is the town centre tile.
pub fn set_town_center<H: PlaceHost + ?Sized>(h: &mut H, unit: u64) {
    let t = h.unit(unit).tile;
    h.set_town(t);
}
