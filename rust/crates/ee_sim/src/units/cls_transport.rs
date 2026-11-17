//! The transports' bodies of `EEUnitClass` (R5b.4, the thirty-third round): the tile searches of the transport
//! kinds (EEUnitClassTransports.cpp until this round: `EEUCTransport`, `EEUCLandTransport`,
//! `EEUCHelicopterTransport`, `EEUCNavalTransport`, `EEUCSpaceTransport`, `EEUCFortress` before round 32 made them
//! kinds of one class), and the six dispatchers that reach them (`CanCarry`, `CanUnloadAt`, `FindGatheringTile`,
//! `FindRendezvousTiles`, `FindUnloadTiles`, `FilterLoadable`) as kind tests deepest first, as
//! EEUnitClassKinds.cpp's did.
//!
//! The world is the [`TrHost`]: the map's tiles, the continent manager's ids, categories, links and shore lists,
//! the unit utilities' C++ leaves (`EEGetBlockingUnits`, `EEGetTilesInRadius`, `EEGetTileLine`, `EEIsLineBlocked`:
//! called through the C++ so their UUTL and CBTL records stay), the path lengths, a cargo's trial location. A unit's
//! plain members are read in place from the records its object derives from ([`EeUnitsUuUnit`]), never across a
//! host call.
//!
//! Determinism (RUST_PLAN 2.1): the C++'s statements in order, the host's questions in the C++'s order and number
//! where a question writes a record (the shore lists' AREA, the leaves' UUTL/CBTL), its float operations in order,
//! `long` as `i32`, the sort of the meeting tiles as MSVC's `std::sort`.

// `c_long` is 32 bits on Windows and 64 on LP64: the casts are needed there. The C++'s negated float comparisons
// (`!(a < b)`) are kept as written: they differ from the flipped comparison for a NaN.
#![allow(clippy::unnecessary_cast, clippy::neg_cmp_op_on_partial_ord)]

use crate::units::kinds::*;
use crate::units::unit_class_fields::EeUnitsUnitClassFields;
use crate::units::unit_fields::bool_byte;
use crate::units::uu::EeUnitsUuUnit;
use crate::units::uu_tiles::Tile;
use ee_base::math::types::Ge3DPoint;
use ee_base::math::um::{floor, floor_unsigned};

/// The categories, unit types, terrains, movement bits and continent categories the bodies test (the C++ enums).
/// cbindgen:ignore
pub mod k {
    pub const TR_CATEGORY_LAND_TRANSPORT: u32 = 0x10;
    pub const TR_CATEGORY_HELICOPTER_TRANSPORT: u32 = 0x11;
    pub const TR_CATEGORY_NAVAL_TRANSPORT: u32 = 0x12;
    pub const TR_CATEGORY_FORTRESS: u32 = 0x1d;
    pub const TR_CATEGORY_SPACE_TRANSPORT: u32 = 0x21;
    pub const TR_TYPE_SIEGE_TOWER: u32 = 0x67;
    pub const TR_TYPE_HEAVY_SIEGE_TOWER: u32 = 0x134;
    pub const TR_TYPE_TELEPORTER: u32 = 0x313;
    pub const TR_TERRAIN_DEEP_WATER: i32 = 1;
    pub const TR_TERRAIN_CLIFF: i32 = 9;
    pub const TR_TERRAIN_SHALLOW_WATER: i32 = 10;
    pub const TR_TERRAIN_SPACE: i32 = 12;
    pub const TR_MOVEMENT_LAND: u8 = 2;
    pub const TR_MOVEMENT_SPACE: u8 = 8;
    pub const TR_CONTINENT_LAND: u8 = 1;
    pub const TR_CONTINENT_WATER: u8 = 2;
    pub const TR_CONTINENT_CLIFF: u8 = 3;
    pub const TR_CONTINENT_SPACE: u8 = 4;
    pub const TR_GOAL_LOCATION: i32 = 1;
    /// `EEGetPathLengthTo`'s "no path"
    pub const TR_NO_PATH: f32 = 10000.0;
    /// the off-map tile
    pub const TR_NONE: i32 = -10000;
}
use k::*;

/// The world the transports' bodies ask (the game's: cls_host.rs).
pub trait TrHost {
    /// a unit's records and pointer members (`obj` an `EEUnit*`; the unit utilities' read)
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit;
    /// a class object's record
    fn class(&mut self, class: u64) -> *const EeUnitsUnitClassFields;
    /// `gWorld.mMap->GetTile(x, y)` (0: off the map), its `mTerrainType`, `IsEdgeTile`, `IsVisibleTo(player)`
    fn map_tile(&mut self, x: i32, y: i32) -> u64;
    fn tile_terrain(&mut self, tile: u64) -> i32;
    fn is_edge_tile(&mut self, x: i32, y: i32) -> bool;
    fn tile_visible_to(&mut self, tile: u64, player: u64) -> bool;
    /// `gWorld.mMap->GetTileLocation(x, y, out)`
    fn tile_location(&mut self, x: i32, y: i32) -> Ge3DPoint;
    /// `gContinentManager`'s `GetContinentID(x, y)`, `GetContinentAt(x, y)` (0: none), the continent's `GetID()`
    /// and `GetCategory()`, `mContinents[id]`, `GetContinent(U2DPoint(x, y), kind)`, `mTileFlags[row * mFlagsWidth
    /// + column]`, `IsNextToArea(x, y, id)`, `GetAdjacentIDs(id)`
    fn continent_id(&mut self, x: i32, y: i32) -> u16;
    fn continent_at(&mut self, x: i32, y: i32) -> u64;
    fn continent_number(&mut self, continent: u64) -> u16;
    fn continent_category(&mut self, continent: u64) -> u8;
    fn continent_by_id(&mut self, id: u16) -> u64;
    fn continent_of(&mut self, x: i32, y: i32, kind: i32) -> u16;
    fn continent_link(&mut self, row: u16, column: u16) -> bool;
    fn next_to_area(&mut self, x: i32, y: i32, id: u16) -> bool;
    fn adjacent_ids(&mut self, id: u16) -> Vec<u16>;
    /// the continent's `GetShoreInRegion(id, out, kind)`, `GetShoreNextTo(id, out)` (the AREA record's calls)
    fn shore_in_region(&mut self, continent: u64, id: u16, kind: i32) -> Vec<Tile>;
    fn shore_next_to(&mut self, continent: u64, id: u16) -> Vec<Tile>;
    /// `EEGetBlockingUnits(player, unit_type, tile, 0, out)`'s answer
    fn blocked(&mut self, player: u64, unit_type: u32, tile: u64) -> bool;
    /// `EEIsLineBlocked(x1, y1, x2, y2, ...)`; `EEGetTileLine(x1, y1, x2, y2, &out, 1, 0)`; `EEGetTilesInRadius(x,
    /// y, 2, true, out)`; `U2DPoint(from).GetLineTiles(to, 1.0f, out)`
    fn line_blocked(&mut self, x1: i32, y1: i32, x2: i32, y2: i32) -> bool;
    fn tile_line(&mut self, from: Tile, to: Tile) -> Vec<Tile>;
    fn tiles_in_radius(&mut self, at: Tile) -> Vec<Tile>;
    fn line_tiles(&mut self, from: Tile, to: Tile) -> Vec<Tile>;
    /// `EEGetPathLengthTo(unit, tile, 0x10, walls)` (it may move the tile: written back)
    fn path_length(&mut self, unit: u64, tile: &mut Tile, walls: bool) -> f32;
    /// `unit->SetLocation(point, false)`
    fn set_location(&mut self, unit: u64, point: Ge3DPoint);
    /// `unit->mUnitClass->GetSurroundingTiles(unit, out, 0, false)` (the dispatcher)
    fn surrounding_tiles(&mut self, unit: u64) -> Vec<Tile>;
    /// the unit's load transport goal's `GetLoadUnitTile()` (None: no such goal)
    fn load_goal_tile(&mut self, unit: u64) -> Option<Tile>;
    /// `mCargo[index]`, `mContainer`, `mWaypoints.size()`, `GetNextWaypointPoint(out)`, `GetQueueGoals(index).size()`,
    /// the goal's `mKind` (-1: none)
    fn cargo(&mut self, unit: u64, index: i32) -> u64;
    fn container(&mut self, unit: u64) -> u64;
    fn waypoints(&mut self, unit: u64) -> i32;
    fn next_waypoint(&mut self, unit: u64) -> Ge3DPoint;
    fn queue_goals(&mut self, unit: u64, index: i32) -> i32;
    fn goal_kind(&mut self, unit: u64) -> i32;
}

/// A class record's members, read in place.
#[derive(Clone, Copy)]
pub struct Cls(pub *const EeUnitsUnitClassFields);

impl Cls {
    fn f(&self) -> &EeUnitsUnitClassFields {
        // SAFETY: a live class's record (the class objects live as long as the game)
        unsafe { &*self.0 }
    }
    pub fn category(&self) -> u32 {
        self.f().mCategory as u32
    }
    pub fn unit_type(&self) -> u32 {
        self.f().mUnitType as u32
    }
    pub fn kinds(&self) -> u64 {
        let c = self.category();
        if c < 256 {
            EE_UNIT_CLASS_KINDS[c as usize]
        } else {
            0
        }
    }
    fn flag(&self, field: &bool) -> bool {
        // SAFETY: a field of the record
        unsafe { bool_byte(field) != 0 }
    }
    /// the siege towers path as walls (`EEGetPathLengthTo`'s last argument)
    fn is_tower(&self) -> bool {
        self.unit_type() == TR_TYPE_SIEGE_TOWER || self.unit_type() == TR_TYPE_HEAVY_SIEGE_TOWER
    }
}

/// A unit's reads: its view (the records' addresses), in place.
#[derive(Clone, Copy)]
pub struct Un(pub EeUnitsUuUnit);

impl Un {
    pub fn obj(&self) -> u64 {
        self.0.obj
    }
    pub fn tile(&self) -> Tile {
        // SAFETY: a live unit's record
        unsafe { [(*self.0.rts).mTileX as i32, (*self.0.rts).mTileY as i32] }
    }
    pub fn class(&self) -> Cls {
        Cls(self.0.class_fields)
    }
    pub fn player(&self) -> u64 {
        self.0.player
    }
    fn loc(&self) -> [f32; 3] {
        self.0.location
    }
    /// `DistanceSquaredToTile(tile)`
    fn distance_squared_to_tile(&self, t: Tile) -> f32 {
        let l = self.loc();
        (l[0] - t[0] as f32) * (l[0] - t[0] as f32) + (l[1] - t[1] as f32) * (l[1] - t[1] as f32)
    }
    /// `DistanceSquaredTo(other)`: the y difference first
    fn distance_squared_to(&self, other: &Un) -> f32 {
        let (a, b) = (self.loc(), other.loc());
        let dy = a[1] - b[1];
        let dx = a[0] - b[0];
        dx * dx + dy * dy
    }
    /// `DistanceSquared3D(point)`: z, y, x
    fn distance_squared_3d(&self, p: Ge3DPoint) -> f32 {
        let l = self.loc();
        let dz = l[2] - p.z;
        let dy = l[1] - p.y;
        let dx = l[0] - p.x;
        dx * dx + dy * dy + dz * dz
    }
    /// `DistanceSquared2D(point)`
    fn distance_squared_2d(&self, x: f32, y: f32) -> f32 {
        let l = self.loc();
        (l[0] - x) * (l[0] - x) + (l[1] - y) * (l[1] - y)
    }
    fn cx(&self) -> &crate::units::unit_fields::EeUnitsComplexUnitFields {
        // SAFETY: a live complex unit's record
        unsafe { &*self.0.cx }
    }
}

/// `U2DPoint<long>::DistanceToSquaredF`: the squares in `long`, summed as floats.
pub fn tile_distance_squared(a: Tile, b: Tile) -> f32 {
    let dx = a[0].wrapping_sub(b[0]);
    let dy = a[1].wrapping_sub(b[1]);
    dx.wrapping_mul(dx) as f32 + dy.wrapping_mul(dy) as f32
}

/// `EETileDistanceLess::DistanceSquared` (`long`).
fn tile_distance_long(center: Tile, t: Tile) -> i32 {
    let dy = center[1].wrapping_sub(t[1]);
    let dx = center[0].wrapping_sub(t[0]);
    dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy))
}

/// The continent kind a transport's shore searches use: 1 (water) or 3 (space).
fn shore_kind(t: &Un) -> i32 {
    ((t.class().f().mDomains & TR_MOVEMENT_SPACE) | 4) as i32 >> 2
}

/// `GetRingTiles(x, y)`: the four sides first, then the corners.
pub fn ring_tiles(x: i32, y: i32) -> [Tile; 8] {
    [[x, y + 1], [x, y - 1], [x + 1, y], [x - 1, y], [x - 1, y + 1], [x + 1, y + 1], [x + 1, y - 1], [x - 1, y - 1]]
}

/// `TransportFindGatheringTile`: the index of the unit nearest `from` and its tile (0 and nothing for no units).
pub fn gathering_tile<H: TrHost + ?Sized>(h: &mut H, units: &[u64], from: Tile, out: &mut Tile) -> i32 {
    let mut result = 0;
    if let Some(&first) = units.first() {
        let tile = Un(h.unit(first)).tile();
        let mut best = tile_distance_squared(from, tile);
        *out = tile;
        for (i, &u) in units.iter().enumerate().skip(1) {
            let tile = Un(h.unit(u)).tile();
            let d = tile_distance_squared(from, tile);
            if !(d < best) {
                continue;
            }
            best = d;
            *out = tile;
            result = i as i32;
        }
    }
    result
}

/// `FindGatheringTile`'s dispatch.
pub fn find_gathering_tile<H: TrHost + ?Sized>(h: &mut H, cls: Cls, units: &[u64], from: Tile, out: &mut Tile) -> i32 {
    if cls.kinds() & EE_CLASS_KIND_TRANSPORT != 0 {
        return gathering_tile(h, units, from, out);
    }
    0
}

/// `FindMeetingTiles`: the unit's boarding tile.
pub fn meeting_tiles<H: TrHost + ?Sized>(
    h: &mut H,
    cls: Cls,
    transport: u64,
    unit: u64,
    io_transport: &mut Tile,
    io_unit: &mut Tile,
) {
    if can_unload_at(h, cls, transport, io_unit[0], io_unit[1]) {
        return;
    }
    let mut tiles = h.tiles_in_radius(*io_unit);
    let center = *io_unit;
    crate::units::msvc_sort::sort_by(&mut tiles, |a, b| {
        tile_distance_long(center, *a) < tile_distance_long(center, *b)
    });
    let player = Un(h.unit(transport)).player();
    let mut found = false;
    for t in tiles {
        let map_tile = h.map_tile(t[0], t[1]);
        if map_tile != 0
            && can_unload_at(h, cls, transport, t[0], t[1])
            && !h.blocked(player, cls.unit_type(), map_tile)
        {
            *io_unit = t;
            found = true;
            break;
        }
    }
    if found {
        return;
    }
    let kind = Un(h.unit(transport)).class().category();
    if kind == TR_CATEGORY_NAVAL_TRANSPORT || kind == TR_CATEGORY_SPACE_TRANSPORT {
        *io_unit = *io_transport;
    } else {
        *io_unit = Un(h.unit(unit)).tile();
    }
}

/// `TransportCanCarry`: whether the unit may enter this kind of transport.
fn transport_can_carry<H: TrHost + ?Sized>(h: &mut H, cls: Cls, cargo: u64) -> bool {
    let c = Un(h.unit(cargo)).class();
    let f = c.f();
    match cls.category() {
        TR_CATEGORY_LAND_TRANSPORT => c.flag(&f.mCanEnterLandTransports),
        TR_CATEGORY_HELICOPTER_TRANSPORT => c.flag(&f.mCanEnterAirTransports),
        TR_CATEGORY_NAVAL_TRANSPORT => c.flag(&f.mCanEnterSeaTransports),
        TR_CATEGORY_SPACE_TRANSPORT => c.flag(&f.mCanEnterSpaceTransports),
        TR_CATEGORY_FORTRESS => c.flag(&f.mCanEnterFortresses),
        _ => false,
    }
}

/// `CanCarry`'s dispatch: the land transport's (one continent), the naval and space transports' (connected
/// continents), the common test.
pub fn can_carry<H: TrHost + ?Sized>(h: &mut H, cls: Cls, unit1: u64, unit2: u64) -> bool {
    let kinds = cls.kinds();
    if kinds & EE_CLASS_KIND_LAND_TRANSPORT != 0 {
        let (a, b) = (Un(h.unit(unit1)).tile(), Un(h.unit(unit2)).tile());
        if a[0] == TR_NONE || b[0] == TR_NONE {
            return false;
        }
        return h.continent_id(a[0], a[1]) == h.continent_id(b[0], b[1]) && transport_can_carry(h, cls, unit2);
    }
    if kinds & (EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_SPACE_TRANSPORT) != 0 {
        let (a, b) = (Un(h.unit(unit1)).tile(), Un(h.unit(unit2)).tile());
        if a[0] == TR_NONE || b[0] == TR_NONE {
            return false;
        }
        let first = h.continent_id(a[0], a[1]);
        let second = h.continent_id(b[0], b[1]);
        return transport_can_carry(h, cls, unit2) && continents_connect(h, first, second);
    }
    if kinds & EE_CLASS_KIND_TRANSPORT != 0 {
        return transport_can_carry(h, cls, unit2);
    }
    false
}

/// `TransportCanUnloadAt`: on the map, not a cliff, space or a map edge tile.
fn transport_can_unload_at<H: TrHost + ?Sized>(h: &mut H, x: i32, y: i32) -> bool {
    let tile = h.map_tile(x, y);
    if tile == 0 {
        return false;
    }
    let terrain = h.tile_terrain(tile);
    terrain != TR_TERRAIN_CLIFF && terrain != TR_TERRAIN_SPACE && !h.is_edge_tile(x, y)
}

/// `LandTransportCanUnloadAt`: on the transport's own continent.
fn land_can_unload_at<H: TrHost + ?Sized>(h: &mut H, transport: u64, x: i32, y: i32) -> bool {
    let t = Un(h.unit(transport)).tile();
    h.continent_id(t[0], t[1]) == h.continent_id(x, y) && transport_can_unload_at(h, x, y)
}

/// `NavalTransportCanUnloadAt` and `SpaceTransportCanUnloadAt` (`region` the continent category whose links
/// decide: water for the naval transport, space for the space one).
fn region_can_unload_at<H: TrHost + ?Sized>(h: &mut H, transport: u64, x: i32, y: i32, region: u8) -> bool {
    let tile = h.map_tile(x, y);
    if tile == 0 {
        return false;
    }
    let terrain = h.tile_terrain(tile);
    if terrain == TR_TERRAIN_DEEP_WATER || terrain == TR_TERRAIN_CLIFF || terrain == TR_TERRAIN_SPACE {
        return false;
    }
    let t = Un(h.unit(transport)).tile();
    let from = h.continent_at(t[0], t[1]);
    let to = h.continent_at(x, y);
    if from == 0 || to == 0 {
        return false;
    }
    // the base class decides; a continent of another kind keeps its answer
    let mut result = transport_can_unload_at(h, x, y);
    if result {
        let category = h.continent_category(from);
        if category == TR_CONTINENT_LAND {
            let to_id = h.continent_number(to);
            let from_id = h.continent_number(from);
            result = continents_connect(h, from_id, to_id);
        } else if category == region {
            let from_id = h.continent_number(from);
            let to_id = h.continent_number(to);
            result = h.continent_link(from_id, to_id);
        }
    }
    result
}

/// `CanUnloadAt`'s dispatch (the fortress's range or teleporter's sight, the helicopter's ground, the land
/// transport's continent, the naval and space transports' links, the common test).
pub fn can_unload_at<H: TrHost + ?Sized>(h: &mut H, cls: Cls, transport: u64, x: i32, y: i32) -> bool {
    let kinds = cls.kinds();
    if kinds & EE_CLASS_KIND_FORTRESS != 0 {
        if cls.unit_type() != TR_TYPE_TELEPORTER {
            let t = Un(h.unit(transport));
            let (cx, cy) = (x as f32 + 0.5, y as f32 + 0.5);
            let range = t.class().f().mUnloadRange;
            return t.distance_squared_2d(cx, cy) <= range * range && land_can_unload_at(h, transport, x, y);
        }
        let tile = h.map_tile(x, y);
        let player = Un(h.unit(transport)).player();
        return tile != 0 && h.tile_visible_to(tile, player) && transport_can_unload_at(h, x, y);
    }
    if kinds & EE_CLASS_KIND_HELICOPTER_TRANSPORT != 0 {
        let tile = h.map_tile(x, y);
        if tile == 0 {
            return false;
        }
        let terrain = h.tile_terrain(tile);
        return terrain != TR_TERRAIN_DEEP_WATER && terrain != TR_TERRAIN_CLIFF && transport_can_unload_at(h, x, y);
    }
    if kinds & EE_CLASS_KIND_LAND_TRANSPORT != 0 {
        return land_can_unload_at(h, transport, x, y);
    }
    if kinds & EE_CLASS_KIND_NAVAL_TRANSPORT != 0 {
        return region_can_unload_at(h, transport, x, y, TR_CONTINENT_WATER);
    }
    if kinds & EE_CLASS_KIND_SPACE_TRANSPORT != 0 {
        return region_can_unload_at(h, transport, x, y, TR_CONTINENT_SPACE);
    }
    if kinds & EE_CLASS_KIND_TRANSPORT != 0 {
        return transport_can_unload_at(h, x, y);
    }
    false
}

/// `ContinentsConnect`: the same continent, or a neighbour of the first that is the second or (not a cliff) is
/// linked to it (the last neighbour first).
pub fn continents_connect<H: TrHost + ?Sized>(h: &mut H, from: u16, to: u16) -> bool {
    let mut result = from == to;
    if !result && from != 0 && to != 0 {
        let ids = h.adjacent_ids(from);
        for &id in ids.iter().rev() {
            if result {
                break;
            }
            let continent = h.continent_by_id(id);
            result = continent != 0
                && (id == to || (h.continent_category(continent) != TR_CONTINENT_CLIFF && h.continent_link(to, id)));
        }
    }
    result
}

/// `CanLoad`: whether the transport can pick the unit up now.
pub fn can_load<H: TrHost + ?Sized>(h: &mut H, cls: Cls, transport: u64, unit: u64) -> bool {
    let goal_tile = h.load_goal_tile(transport);
    let mut range = cls.f().mLoadRange;
    let t = Un(h.unit(transport));
    let u = Un(h.unit(unit));
    let (tt, ut) = (t.tile(), u.tile());
    let from = h.continent_id(tt[0], tt[1]);
    let to = h.continent_id(ut[0], ut[1]);
    range *= range;
    let mut result = false;
    let mut transport_tile = tt;
    if from == to {
        let d1 = h.path_length(unit, &mut transport_tile, false);
        let d2 = h.path_length(unit, &mut transport_tile, false);
        if d2 * d1 <= range {
            result = true;
        } else {
            let tiles = h.surrounding_tiles(transport);
            let cargo_tile = Un(h.unit(unit)).tile();
            for it in tiles {
                if result {
                    break;
                }
                result = it == cargo_tile;
            }
        }
    } else {
        let cargo_tile = ut;
        let mut unit_tile: Tile = [0, 0];
        match goal_tile {
            None => {
                best_shore_tile(h, transport, transport_tile, cargo_tile, transport_tile, false, &mut unit_tile);
            }
            Some(g) => unit_tile = g,
        }
        if unit_tile[0] != TR_NONE {
            let u = Un(h.unit(unit));
            let d = u.distance_squared_to_tile(unit_tile);
            if d < range {
                let t = Un(h.unit(transport));
                if !(t.distance_squared_to_tile(unit_tile) + d < range) {
                    result = false;
                } else if h.path_length(unit, &mut unit_tile, false) != TR_NO_PATH {
                    result = true;
                } else {
                    let (t, u) = (Un(h.unit(transport)), Un(h.unit(unit)));
                    if t.distance_squared_to(&u) < range {
                        let (a, b) = (u.tile(), t.tile());
                        result = !h.line_blocked(a[0], a[1], b[0], b[1]);
                    } else {
                        result = false;
                    }
                }
            }
        }
    }
    result
}

/// `CanUnloadNear`: whether cargo unloaded here reaches the destination within unload range (the cargo set on the
/// trial tile for its path lengths, then off the map). `transport` 0: the naval transports' search, which has none.
pub fn can_unload_near<H: TrHost + ?Sized>(
    h: &mut H,
    cls: Cls,
    transport: u64,
    cargo: u64,
    unload: Tile,
    first: Tile,
    destination: Tile,
) -> bool {
    let mut dest = destination;
    let range = cls.f().mUnloadRange * cls.f().mUnloadRange;
    let from = h.continent_id(unload[0], unload[1]);
    let to = h.continent_id(destination[0], destination[1]);
    let invalid = Ge3DPoint { x: -10000.0, y: -10000.0, z: 0.0 };
    let tower = cls.is_tower();
    if from == to {
        let at = if (cls.category() == TR_CATEGORY_LAND_TRANSPORT || cls.category() == TR_CATEGORY_FORTRESS)
            && transport != 0
        {
            Un(h.unit(transport)).tile()
        } else {
            // (a land transport's search always has its transport; the naval search's first cargo tile otherwise)
            first
        };
        let position = h.tile_location(at[0], at[1]);
        h.set_location(cargo, position);
        let d1 = h.path_length(cargo, &mut dest, tower);
        let d2 = h.path_length(cargo, &mut dest, tower);
        let distance = d2 * d1;
        h.set_location(cargo, invalid);
        distance < range
    } else {
        let position = h.tile_location(first[0], first[1]);
        let distance = if transport != 0 {
            Un(h.unit(transport)).distance_squared_3d(position)
        } else {
            h.tile_location(unload[0], unload[1]).distance_to_squared(position)
        };
        if distance < range {
            h.set_location(cargo, position);
            let d1 = h.path_length(cargo, &mut dest, tower);
            let d2 = h.path_length(cargo, &mut dest, tower);
            let cargo_distance = d2 * d1;
            h.set_location(cargo, invalid);
            distance + cargo_distance < range
        } else {
            false
        }
    }
}

/// `FindLandingTile`: where cargo lands (None: the tile is off the map and the out tile stays).
pub fn landing_tile<H: TrHost + ?Sized>(
    h: &mut H,
    cls: Cls,
    transport: u64,
    unload: Tile,
    target: Tile,
) -> Option<Tile> {
    let tile = h.map_tile(unload[0], unload[1]);
    if tile == 0 {
        return None;
    }
    let terrain = h.tile_terrain(tile);
    if terrain != TR_TERRAIN_SHALLOW_WATER {
        if terrain == TR_TERRAIN_DEEP_WATER || terrain == TR_TERRAIN_SPACE {
            let continent = h.continent_at(unload[0], unload[1]);
            let player = Un(h.unit(transport)).player();
            let mut best = 0.0f32;
            let mut out = None;
            for it in ring_tiles(unload[0], unload[1]) {
                let tile = h.map_tile(it[0], it[1]);
                let id = h.continent_number(continent);
                if tile == 0 {
                    continue;
                }
                let terrain = h.tile_terrain(tile);
                if terrain == TR_TERRAIN_DEEP_WATER
                    || terrain == TR_TERRAIN_CLIFF
                    || terrain == TR_TERRAIN_SPACE
                    || !h.next_to_area(it[0], it[1], id)
                    || h.blocked(player, cls.unit_type(), tile)
                    || h.line_blocked(unload[0], unload[1], it[0], it[1])
                {
                    continue;
                }
                let d = tile_distance_squared(it, target);
                if !(d <= best) && best != 0.0 {
                    continue;
                }
                best = d;
                out = Some(it);
            }
            if out.is_some() {
                return out;
            }
        } else if terrain == TR_TERRAIN_CLIFF {
            return Some(Un(h.unit(transport)).tile());
        }
    }
    Some(unload)
}

/// Whether a tile passes the unload tile tests (`EETestUnloadTile`'s and `EEUpdateBestUnloadTile`'s common part:
/// on the map, of the transport's terrain when restricted, not blocked).
fn unload_tile_open<H: TrHost + ?Sized>(h: &mut H, t: &Un, tile_at: Tile, restrict: bool) -> bool {
    let tile = h.map_tile(tile_at[0], tile_at[1]);
    if tile == 0 {
        return false;
    }
    let category = t.class().category();
    if restrict {
        let terrain = h.tile_terrain(tile);
        if (category != TR_CATEGORY_NAVAL_TRANSPORT
            || (terrain != TR_TERRAIN_DEEP_WATER && terrain != TR_TERRAIN_SHALLOW_WATER))
            && (category != TR_CATEGORY_SPACE_TRANSPORT || terrain != TR_TERRAIN_SPACE)
        {
            return false;
        }
    }
    !h.blocked(t.player(), t.class().unit_type(), tile)
}

/// `EETestUnloadTile`: the tile when it passes and lies within 5 tiles of the target.
fn test_unload_tile<H: TrHost + ?Sized>(h: &mut H, t: &Un, tile: Tile, target: Tile, restrict: bool, out: &mut Tile) {
    if unload_tile_open(h, t, tile, restrict) && tile_distance_squared(tile, target) < 25.0 {
        *out = tile;
    }
}

/// `EEUpdateBestUnloadTile`: the tile when it passes and is no further from the target than the best (0: none).
fn update_best_unload_tile<H: TrHost + ?Sized>(
    h: &mut H,
    t: &Un,
    tile: Tile,
    target: Tile,
    restrict: bool,
    best: &mut f32,
    out: &mut Tile,
) {
    if !unload_tile_open(h, t, tile, restrict) {
        return;
    }
    let d = tile_distance_squared(tile, target);
    if d <= *best || *best == 0.0 {
        *out = tile;
        *best = d;
    }
}

/// The shore scan the searches repeat: from the last tile down, the tiles next to area `id` as unload tiles, then
/// `from` itself when it was one of them.
#[allow(clippy::too_many_arguments)] // the C++'s inputs
fn scan_shore<H: TrHost + ?Sized>(
    h: &mut H,
    t: &Un,
    tiles: &[Tile],
    id: u16,
    from: Tile,
    target: Tile,
    restrict: bool,
    out: &mut Tile,
) {
    let mut contains_from = false;
    let mut best = 0.0f32;
    for &tile in tiles.iter().rev() {
        if !h.next_to_area(tile[0], tile[1], id) {
            continue;
        }
        update_best_unload_tile(h, t, tile, target, restrict, &mut best, out);
        if tile == from {
            contains_from = true;
        }
    }
    if contains_from {
        test_unload_tile(h, t, from, target, restrict, out);
    }
}

/// `FindShoreTile`: a shore tile of the transport's region next to the destination's continent, nearest the
/// target; the destination when there is no region, (0, 0) when no tile fits.
#[allow(clippy::too_many_arguments)] // the C++'s inputs
pub fn shore_tile<H: TrHost + ?Sized>(
    h: &mut H,
    transport: u64,
    from: Tile,
    destination: Tile,
    target: Tile,
    restrict: bool,
    out: &mut Tile,
) {
    let t = Un(h.unit(transport));
    let to = h.continent_at(destination[0], destination[1]);
    let kind = shore_kind(&t);
    let from_id = h.continent_of(from[0], from[1], kind);
    *out = [0, 0];
    let from_c = h.continent_at(from[0], from[1]);
    if from_c == to {
        if from_id != 0 {
            let tiles = h.shore_in_region(from_c, from_id, kind);
            if !tiles.is_empty() {
                let id = h.continent_number(to);
                scan_shore(h, &t, &tiles, id, from, target, restrict, out);
            }
        } else {
            let id = h.continent_number(to);
            if h.next_to_area(destination[0], destination[1], id) {
                *out = destination;
            }
        }
    } else if from_id != 0 {
        let tiles = h.shore_in_region(to, from_id, kind);
        if !tiles.is_empty() {
            best_shore_tile(h, transport, from, destination, target, restrict, out);
        } else {
            let (to_id, from_number) = (h.continent_number(to), h.continent_number(from_c));
            let id = if h.continent_link(from_number, to_id) { to_id } else { from_number };
            let tiles = h.shore_in_region(from_c, from_id, kind);
            if !tiles.is_empty() {
                scan_shore(h, &t, &tiles, id, from, target, restrict, out);
            }
        }
    } else {
        *out = destination;
    }
}

/// `FindBestShoreTile`: the shore tile between the start's and destination's continents nearest the target (else
/// the first shore tile); its distance from the start, or 0 with the out tile -10000 when there is none.
#[allow(clippy::too_many_arguments)] // the C++'s inputs
pub fn best_shore_tile<H: TrHost + ?Sized>(
    h: &mut H,
    transport: u64,
    from: Tile,
    destination: Tile,
    target: Tile,
    restrict: bool,
    out: &mut Tile,
) -> f32 {
    let t = Un(h.unit(transport));
    let from_c = h.continent_at(from[0], from[1]);
    let to_c = h.continent_at(destination[0], destination[1]);
    let kind = shore_kind(&t);
    let from_id = h.continent_of(from[0], from[1], kind);
    let to_id = h.continent_of(destination[0], destination[1], kind);
    let mut id = 0u16;
    let mut tiles: Vec<Tile> = Vec::new();
    *out = [TR_NONE, TR_NONE];
    if from_c != 0 {
        if from_id != 0 && h.continent_category(to_c) == TR_CONTINENT_LAND {
            tiles = h.shore_in_region(to_c, from_id, kind);
            id = h.continent_number(to_c);
            let from_number = h.continent_number(from_c);
            if tiles.is_empty() && h.continent_link(from_number, id) {
                tiles = h.shore_in_region(from_c, from_id, kind);
                if !tiles.is_empty() {
                    let to_area = h.continent_number(to_c);
                    scan_shore(h, &t, &tiles, to_area, from, target, restrict, out);
                }
            }
        } else if to_id != 0 && h.continent_category(from_c) == TR_CONTINENT_LAND {
            tiles = h.shore_in_region(to_c, to_id, kind);
            id = h.continent_number(to_c);
        } else {
            let mut source = 0u64;
            if to_id == 0 && h.continent_category(from_c) == TR_CONTINENT_LAND {
                source = from_c;
                id = if to_c == 0 { 0 } else { h.continent_number(to_c) };
            } else if to_c != 0 {
                id = h.continent_number(from_c);
                source = to_c;
            }
            if source != 0 {
                tiles = h.shore_next_to(source, id);
            }
        }
        if !tiles.is_empty() {
            scan_shore(h, &t, &tiles, id, from, target, restrict, out);
            if out[0] == TR_NONE {
                *out = tiles[0];
            }
        }
    }
    if out[0] != TR_NONE && out[1] != TR_NONE {
        // `U2DPoint<long>::DistanceToF`: the float sqrt of the float sum
        ee_base::math::um::sqrt(tile_distance_squared(*out, from))
    } else {
        0.0
    }
}

/// `FilterLoadable` (the transport kinds): moves the units that cannot board (not commandable, calamity flags 0x10
/// or 1, inside something, off the map or on deep water, or not carried by this class) to `rejected`, in order.
pub fn filter_loadable<H: TrHost + ?Sized>(
    h: &mut H,
    cls: Cls,
    units: &mut Vec<u64>,
    transport: u64,
    rejected: &mut Vec<u64>,
) {
    if cls.kinds() & EE_CLASS_KIND_TRANSPORT == 0 {
        return;
    }
    let mut i = 0;
    while i < units.len() {
        let unit = units[i];
        let u = Un(h.unit(unit));
        let t = u.tile();
        let tile = h.map_tile(t[0], t[1]);
        let cx = u.cx();
        // SAFETY: a field of the record
        let commandable = unsafe { bool_byte(&cx.mIsCommandable) } != 0;
        let flags = cx.mCalamityFlags as u32;
        let keep = commandable
            && flags & 0x10 == 0
            && flags & 1 == 0
            && h.container(unit) == 0
            && tile != 0
            && h.tile_terrain(tile) != TR_TERRAIN_DEEP_WATER
            && can_carry(h, cls, transport, unit);
        if keep {
            i += 1;
            continue;
        }
        units.remove(i);
        rejected.push(unit);
    }
}

/// `FindRendezvousTiles`' dispatch: where the transport and its passengers meet.
pub fn rendezvous_tiles<H: TrHost + ?Sized>(
    h: &mut H,
    cls: Cls,
    units: &[u64],
    transport: u64,
    out_transport: &mut Tile,
    out_unit: &mut Tile,
) {
    let kinds = cls.kinds();
    if kinds & EE_CLASS_KIND_FORTRESS != 0 {
        own_rendezvous(h, transport, out_transport, out_unit);
    } else if kinds & EE_CLASS_KIND_HELICOPTER_TRANSPORT != 0 {
        let from = Un(h.unit(transport)).tile();
        find_gathering_tile(h, cls, units, from, out_transport);
        *out_unit = *out_transport;
    } else if kinds & EE_CLASS_KIND_LAND_TRANSPORT != 0 {
        let from = Un(h.unit(transport)).tile();
        let mut nearest: Tile = [0, 0];
        let index = find_gathering_tile(h, cls, units, from, &mut nearest);
        let unit = units[index as usize];
        let line = h.line_tiles(from, nearest);
        let share = Un(h.unit(transport)).class().f().mMaxVelocity;
        let other = Un(h.unit(unit)).class().f().mMaxVelocity;
        *out_transport = line[floor_unsigned(line.len() as f32 * share / (share + other)) as usize];
        let mut tile = *out_transport;
        meeting_tiles(h, cls, transport, unit, &mut tile, out_transport);
        *out_unit = *out_transport;
    } else if kinds & (EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_SPACE_TRANSPORT) != 0 {
        let first = units[0];
        let from = Un(h.unit(transport)).tile();
        let unit_tile = Un(h.unit(first)).tile();
        let unit_continent = h.continent_id(unit_tile[0], unit_tile[1]);
        let transport_continent = h.continent_id(from[0], from[1]);
        let mut gather: Tile = [0, 0];
        find_gathering_tile(h, cls, units, from, &mut gather);
        if unit_continent == transport_continent {
            own_rendezvous(h, transport, out_transport, out_unit);
        } else {
            shore_tile(h, transport, from, unit_tile, from, true, out_transport);
            if let Some(t) = landing_tile(h, cls, transport, *out_transport, unit_tile) {
                *out_unit = t;
            }
        }
        meeting_tiles(h, cls, transport, first, out_transport, out_unit);
    } else if kinds & EE_CLASS_KIND_TRANSPORT != 0 {
        own_rendezvous(h, transport, out_transport, out_unit);
    }
}

/// `TransportFindRendezvousTiles`: the transport's own tile for both.
fn own_rendezvous<H: TrHost + ?Sized>(h: &mut H, transport: u64, out_transport: &mut Tile, out_unit: &mut Tile) {
    *out_transport = Un(h.unit(transport)).tile();
    *out_unit = *out_transport;
}

/// `FindUnloadTiles`' dispatch: the fortress's (its own tile, a teleporter the destination), the naval and space
/// transports' search, the destination for the others (and the airplanes).
pub fn unload_tiles<H: TrHost + ?Sized>(
    h: &mut H,
    cls: Cls,
    transport: u64,
    destination: Tile,
    out: &mut [Tile; 3],
) -> bool {
    let kinds = cls.kinds();
    if kinds & EE_CLASS_KIND_FORTRESS != 0 {
        let at = if cls.unit_type() != TR_TYPE_TELEPORTER { Un(h.unit(transport)).tile() } else { destination };
        *out = [at, at, at];
    } else if kinds & EE_CLASS_KIND_AIRPLANE != 0 {
        *out = [destination; 3];
    } else if kinds & (EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_SPACE_TRANSPORT) != 0 {
        region_unload_tiles(h, cls, transport, destination, out);
    } else if kinds & EE_CLASS_KIND_TRANSPORT != 0 {
        *out = [destination; 3];
    } else {
        return false;
    }
    true
}

/// `NavalTransportFindUnloadTiles` (and the space transport's identical copy): the shore and landing tiles, then
/// the destination when in range and reachable, else the last good tile of the line from the landing tile.
/// `out`: the unload tile, the first cargo tile, the tile (each kept when the search does not set it).
fn region_unload_tiles<H: TrHost + ?Sized>(
    h: &mut H,
    cls: Cls,
    transport: u64,
    destination: Tile,
    out: &mut [Tile; 3],
) {
    let range = cls.f().mUnloadRange * cls.f().mUnloadRange;
    let from = Un(h.unit(transport)).tile();
    shore_tile(h, transport, from, destination, destination, true, &mut out[0]);
    if let Some(t) = landing_tile(h, cls, transport, out[0], destination) {
        out[1] = t;
    }
    let (unload, first) = (out[0], out[1]);
    if tile_distance_squared(unload, destination) < range && {
        let cargo = h.cargo(transport, 0);
        can_unload_near(h, cls, 0, cargo, unload, first, destination)
    } {
        let cargo = h.cargo(transport, 0);
        out[2] = if can_unload_near(h, cls, 0, cargo, unload, first, destination) { destination } else { first };
    } else {
        let tiles = h.tile_line(first, destination);
        let player = Un(h.unit(transport)).player();
        let mut index = 0;
        while index < tiles.len() {
            let point = tiles[index];
            let d = tile_distance_squared(unload, point);
            let tile = h.map_tile(point[0], point[1]);
            // port: a tile off the map ends the line (AI_RESEARCH.md §8.27)
            if tile == 0 || d > range || h.blocked(player, cls.unit_type(), tile) || {
                let cargo = h.cargo(transport, 0);
                !can_unload_near(h, cls, 0, cargo, unload, first, point)
            } {
                break;
            }
            index += 1;
        }
        if index != 0 && !tiles.is_empty() {
            index -= 1;
        }
        out[2] = if index != tiles.len() { tiles[index] } else { first };
    }
}

/// `NavalTransportHasArrived`: sinking, at its path's end tile with its speed reached, or slow.
pub fn naval_has_arrived<H: TrHost + ?Sized>(h: &mut H, unit: u64, value: f32) -> bool {
    let u = Un(h.unit(unit));
    if u.cx().mCollisionState == 8 {
        return true;
    }
    let count = h.waypoints(unit);
    let point = h.next_waypoint(unit);
    let t = Un(h.unit(unit)).tile();
    if t[0] == floor(point.x)
        && t[1] == floor(point.y)
        && (count > 1 || (h.queue_goals(unit, 0) > 1 && h.goal_kind(unit) == TR_GOAL_LOCATION))
    {
        let radius = Un(h.unit(unit)).cx().mCollisionRadius;
        !(radius < value)
    } else {
        value <= 0.05
    }
}

#[cfg(test)]
#[path = "cls_transport_tests.rs"]
mod tests;
