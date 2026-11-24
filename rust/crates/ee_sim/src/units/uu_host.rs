//! The unit utilities' host in the game: every [`UuHost`] call one C++ function (User Interface
//! Thread/EEFOFindNewTargetNearOldTarget.cpp, `ee_units_uu_host_*`), and the C++ vectors the entry points read and
//! fill. Absent from a test binary.

#![cfg(not(test))]

use crate::units::uu_world_ops;
use std::ffi::{c_long, c_void};

use ee_base::math::types::GeTransformation;

use crate::units::uu::{EeUnitsUuClass, EeUnitsUuUnit, UuHost};
use crate::units::uu_tiles::Tile;
use crate::units::uu_world::UuWorld;
use crate::world::{ClassView, GroupView, MapView, PlayerView, TileView, UnitView, WorldRead, WorldView};

extern "C" {
    fn ee_units_uu_host_is_edge_tile(x: i32, y: i32) -> u8;
    fn ee_units_uu_host_raised_corners(map: u64, x: i32, y: i32) -> i32;
    fn ee_units_uu_host_mesh_z(map: u64, x: i32, y: i32) -> f32;
    fn ee_units_uu_host_mesh_ground(x: f32, y: f32) -> f32;
    fn ee_units_uu_host_passable_by(obj: u64, by: u64) -> u8;
    fn ee_units_uu_host_is_foundation(obj: u64) -> u8;
    fn ee_units_uu_host_can_move_on_terrain(class_obj: u64, unit: u64, terrain: i32) -> u8;
    fn ee_units_uu_host_passable_flags(class_obj: u64, unit: u64) -> i32;
    /// a fresh `std::vector<U2DPoint<long> >` (a body's local list), and its end
    fn ee_units_uu_host_tiles_new() -> *mut c_void;
    fn ee_units_uu_host_tiles_delete(v: *mut c_void);
    fn ee_units_uu_host_container_continent(container: u64, unit: u64) -> u64;
    fn ee_units_uu_host_reserved(group: u64, point: *const f32) -> u8;
    fn ee_units_uu_host_ground_point(point: *const f32, out: *mut f32);
    fn ee_units_uu_host_terrain_tilt(m: *const f32, roll: u8) -> f32;
    fn ee_units_uu_host_player(which: i32, player: u64, value: i32) -> i32;
    fn ee_units_uu_host_facing_quarter(x: i32, y: i32) -> i32;
    fn ee_units_uu_host_resource_users(unit: u64) -> i32;
    fn ee_units_uu_host_random_long(min: i32, max: i32) -> i32;
    fn ee_units_uu_host_random_float(min: f32, max: f32) -> f32;
    fn ee_units_uu_host_set_animation(complex: u64, animation: i32);
    fn ee_units_uu_host_set_transformation(complex: u64, m: *const f32);
    fn ee_units_uu_host_tiles_set(v: *mut c_void, tiles: *const [c_long; 2], count: u32);
    fn ee_units_uu_host_units_len(v: *const c_void) -> u32;
    fn ee_units_uu_host_units_get(v: *const c_void, out: *mut u64);
    fn ee_units_uu_host_units_set(v: *mut c_void, units: *const u64, count: u32);
    fn ee_units_uu_host_tile_test(which: i32, tile: u64, player: u64, x: i32, y: i32) -> u8;
    fn ee_units_uu_host_center_offset(complex: u64, io: *mut f32);
    fn ee_units_uu_host_tiles_test(which: i32, tiles: *const [c_long; 2], count: u32, kind: i32) -> u8;
    fn ee_units_uu_host_spot(which: i32, unit: u64, x: i32, y: i32, out: *mut f32) -> i32;
    fn ee_units_uu_host_world(which: i32, a: u64, b: u64, c: u64, x: i32, y: i32, f: f32) -> u64;
    fn ee_units_uu_host_world_f(which: i32, a: u64, b: u64, f: f32) -> f32;
}

/// `ee_units_uu_host_world`'s questions and writes (one C++ switch).
const WORLD_CAN_PRODUCE: i32 = 4;
const WORLD_REGIONS_FOOTPRINT: i32 = 8;
const WORLD_LOADING: i32 = 9;
const WORLD_CAN_TAKE: i32 = 11;
const WORLD_LAST_RESOURCE: i32 = 12;
const WORLD_DEPOSIT_EFFECT: i32 = 13;
const WORLD_ADD_RESOURCE: i32 = 14;
const WORLD_STATISTIC: i32 = 15;
const WORLD_HAS_GATHER_POINT: i32 = 18;
/// `ee_units_uu_host_world_f`'s.
const WORLD_F_GATHER_REACH: i32 = 1;
const WORLD_F_DEPOSIT_FACTOR: i32 = 4;

/// `ee_units_uu_host_player`'s questions.
const PLAYER_HUMAN: i32 = 1;
const PLAYER_UNIT_SET: i32 = 2;
const PLAYER_GROUP_UNITS: i32 = 3;
const PLAYER_GROUP_CONTINENT: i32 = 4;
const PLAYER_PASSABILITY: i32 = 5;
/// `ee_units_uu_host_tile_test`'s questions.
const TILE_FLAT: i32 = 0;
const TILE_VISIBLE: i32 = 1;
const TILE_MESH_VALID: i32 = 2;

/// `f` of the world map's unit regions (None before there are any).
fn with_regions<R>(f: impl FnOnce(&crate::combat::regions::EeCombatUnitRegions) -> R) -> Option<R> {
    // SAFETY: the utilities' callers hold the world
    let t = unsafe { crate::world::WorldRead::begin() };
    let map = crate::world::WorldView::global(&t)?.map()?;
    // SAFETY: the map's live table (the map owns it)
    unsafe { map.unit_regions().as_ref() }.map(f)
}

/// `f` of a read token (the utilities' callers hold the world).
fn read<R>(f: impl FnOnce(&WorldRead) -> R) -> R {
    // SAFETY: the utilities run on a thread that holds the world
    let t = unsafe { WorldRead::begin() };
    f(&t)
}

/// A class's `IsLandBuilding()` and `FliesOver()` (the dispatch table's bodies, EEUnitClassKinds.cpp).
fn class_flags(c: &ClassView<'_>) -> (u8, u8) {
    use crate::units::cls_dispatch::{EE_UNITS_BODY_FLIES_OVER, EE_UNITS_BODY_IS_LAND_BUILDING, EE_UNIT_CLASS_BODIES};
    let category = c.category();
    if category >= 256 {
        return (0, 0);
    }
    let land = matches!(EE_UNIT_CLASS_BODIES[EE_UNITS_BODY_IS_LAND_BUILDING][category as usize], 1 | 3);
    let domains: u8 = crate::world_field!(c.fields(), mDomains);
    let flies = match EE_UNIT_CLASS_BODIES[EE_UNITS_BODY_FLIES_OVER][category as usize] {
        1 => true,
        2 => domains & MOVEMENT_AIR != 0,
        _ => false,
    };
    (u8::from(land), u8::from(flies))
}

/// `kMovementAir`
const MOVEMENT_AIR: u8 = 1;

/// A class's record (`ee_units_uu_host_class` was).
fn class_record(c: &ClassView<'_>) -> EeUnitsUuClass {
    let (land_building, flies_over) = class_flags(c);
    EeUnitsUuClass { obj: c.handle(), fields: c.fields().ptr(), land_building, flies_over }
}

/// A unit's record (`ee_units_uu_host_unit` was): its records, pointers and the class's two virtuals' answers.
fn unit_record(u: &UnitView<'_>) -> EeUnitsUuUnit {
    let complex = u.complex_unit();
    let class = u.class();
    let player = u.player();
    let (land_building, flies_over) = class.as_ref().map_or((0, 0), class_flags);
    let mut out = EeUnitsUuUnit {
        obj: u.handle(),
        rts: u.rts().ptr(),
        unit: u.fields().ptr(),
        cx: complex.as_ref().map_or(std::ptr::null(), |c| c.complex().ptr()),
        class_fields: class.as_ref().map_or(std::ptr::null(), |c| c.fields().ptr()),
        class_obj: class.as_ref().map_or(0, |c| c.handle()),
        complex: u.complex_unit_handle(),
        player: player.as_ref().map_or(0, |p| p.handle()),
        tile: u.tile().map_or(0, |t| t.handle()),
        player_id: player.as_ref().map_or(-1, |p| p.id() as i32),
        on_map: u8::from(u.has_tile_node()),
        land_building,
        flies_over,
        ..EeUnitsUuUnit::default()
    };
    let Some(c) = complex else { return out };
    out.active_group = c.active_group_handle();
    out.location = c.location();
    if let Some(s) = c.stats() {
        out.stats_max_velocity = crate::world_field!(s.fields(), mMaxVelocity);
        out.stats_attack = crate::world_field!(s.fields(), mAttack);
        out.has_stats = 1;
    }
    out.command = c.command_type() as i32;
    out
}

/// The game's world.
pub struct Game;

impl UuHost for Game {
    fn map(&mut self) -> u64 {
        read(|t| WorldView::global(t).and_then(|w| w.map()).map_or(0, |m| m.handle()))
    }
    fn map_tile(&mut self, map: u64, x: i32, y: i32) -> u64 {
        // SAFETY: a live map (or 0)
        read(|t| unsafe { MapView::of(t, map) }.and_then(|m| m.tile(x.into(), y.into())).map_or(0, |t| t.handle()))
    }
    fn tile_terrain(&mut self, tile: u64) -> i32 {
        // SAFETY: a live tile the utilities were given
        read(|t| unsafe { TileView::of(t, tile) }.map_or(0, |t| t.terrain_type() as i32))
    }
    fn tile_height(&mut self, tile: u64) -> f32 {
        // SAFETY: as tile_terrain
        read(|t| unsafe { TileView::of(t, tile) }.map_or(0.0, |t| t.height()))
    }
    fn tile_units(&mut self, tile: u64, out: &mut Vec<u64>) {
        // SAFETY: as tile_terrain
        *out = read(|t| {
            unsafe { TileView::of(t, tile) }.map(|t| t.units().map(|u| u.map_or(0, |u| u.handle())).collect())
        })
        .unwrap_or_default();
    }
    fn is_edge_tile(&mut self, x: i32, y: i32) -> bool {
        // SAFETY: a C++ read of the map
        unsafe { ee_units_uu_host_is_edge_tile(x, y) != 0 }
    }
    fn raised_corners(&mut self, map: u64, x: i32, y: i32) -> i32 {
        // SAFETY: as map_tile
        unsafe { ee_units_uu_host_raised_corners(map, x, y) }
    }
    fn mesh_z(&mut self, map: u64, x: i32, y: i32) -> f32 {
        // SAFETY: as map_tile
        unsafe { ee_units_uu_host_mesh_z(map, x, y) }
    }
    fn mesh_ground(&mut self, x: f32, y: f32) -> f32 {
        // SAFETY: a C++ read of the map
        unsafe { ee_units_uu_host_mesh_ground(x, y) }
    }
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit {
        // SAFETY: a live unit the utilities were given (0 gives the default)
        read(|t| unsafe { UnitView::of(t, obj) }.map(|u| unit_record(&u)).unwrap_or_default())
    }
    fn class_of_type(&mut self, unit_type: i32) -> EeUnitsUuClass {
        read(|t| ClassView::by_index(t, unit_type as usize).map(|c| class_record(&c)).unwrap_or_default())
    }
    fn class(&mut self, class_obj: u64) -> EeUnitsUuClass {
        // SAFETY: a live class (or 0)
        read(|t| unsafe { ClassView::of(t, class_obj) }.map(|c| class_record(&c)).unwrap_or_default())
    }
    fn position(&mut self, obj: u64) -> [f32; 3] {
        crate::units::unit_geom::position_of(obj)
    }
    fn passable_by(&mut self, obj: u64, by: u64) -> bool {
        // SAFETY: live units the C++ gave
        unsafe { ee_units_uu_host_passable_by(obj, by) != 0 }
    }
    fn is_foundation(&mut self, obj: u64) -> bool {
        // SAFETY: as passable_by
        unsafe { ee_units_uu_host_is_foundation(obj) != 0 }
    }
    fn can_move_on_terrain(&mut self, class_obj: u64, unit: u64, terrain: i32) -> bool {
        // SAFETY: as passable_by
        unsafe { ee_units_uu_host_can_move_on_terrain(class_obj, unit, terrain) != 0 }
    }
    fn passable_flags(&mut self, class_obj: u64, unit: u64) -> i32 {
        // SAFETY: as passable_by
        unsafe { ee_units_uu_host_passable_flags(class_obj, unit) }
    }
    fn surrounding_tiles(&mut self, target: u64, virtual_call: bool) -> Vec<Tile> {
        // `target->mUnitClass->GetSurroundingTiles(target, tiles, 1, false)` (ucn_noted.rs) or its
        // `BaseGetSurroundingTiles` (uc_rest.rs), into a fresh C++ list as the original's local
        let class = read(|t| {
            // SAFETY: the body's live target
            unsafe { UnitView::of(t, target) }.and_then(|u| u.class()).map_or(0, |c| c.handle())
        });
        // SAFETY: a live unit and its class; the list lives until its delete
        unsafe {
            let list = ee_units_uu_host_tiles_new();
            if virtual_call {
                crate::units::ucn_noted::ee_units_ucn_surrounding_tiles(class, target, list as usize as u64, 1, false);
            } else {
                crate::combat::cg_query::rust_calls::ee_units_uc_rest(
                    crate::units::uc_rest::EE_UNITS_UC_DO_R_BASE_SURROUNDING,
                    class as usize as *mut c_void,
                    target as usize as *mut c_void,
                    list as usize as u64,
                    1,
                    0,
                    std::ptr::null_mut(),
                );
            }
            let tiles = tiles_of(list);
            ee_units_uu_host_tiles_delete(list);
            tiles
        }
    }
    fn container_continent(&mut self, container: u64, unit: u64) -> u64 {
        // SAFETY: live units
        unsafe { ee_units_uu_host_container_continent(container, unit) }
    }
    // the continent manager's lookups: its Rust record (ee_sim::astar), its entry points where the C++ called them
    fn continent_id(&mut self, x: i32, y: i32) -> u16 {
        cm_lookup::continent_id(x, y)
    }
    fn continent_by_id(&mut self, id: u16) -> u64 {
        cm_lookup::continent_by_id(id)
    }
    fn continent_at(&mut self, x: i32, y: i32) -> u64 {
        match cm_lookup::continent_id(x, y) {
            0 => 0,
            id => cm_lookup::continent_by_id(id),
        }
    }
    fn continent_number(&mut self, continent: u64) -> u16 {
        // SAFETY: a continent the manager gave
        unsafe { crate::astar::cm_io::continent(continent).id }
    }
    fn are_connected(&mut self, a: u64, b: u64, flags: i32) -> bool {
        // SAFETY: continents the manager gave (its own CNTQ scope)
        unsafe { crate::astar::cm_queries::ee_astar_cmq_are_connected(a, b, flags as c_long) }
    }
    fn are_connected_for_unit(&mut self, unit: u64, a: u64, b: u64) -> bool {
        // SAFETY: as are_connected; a live unit
        unsafe { crate::astar::cm_unit::connected_for_unit(unit, a, b) }
    }
    fn continent_tile_flag(&mut self, x: i32, y: i32) -> i32 {
        crate::astar::world_view::ee_astar_tile_flag(x as c_long, y as c_long) as i32
    }
    fn continent_link(&mut self, row: u16, column: u16) -> bool {
        cm_lookup::adjacent(row, column)
    }
    fn line_blocked(&mut self, x1: i32, y1: i32, x2: i32, y2: i32) -> bool {
        crate::units::uu_world::line_blocked(self, x1, y1, x2, y2).0
    }
    fn tile_line(&mut self, x1: i32, y1: i32, x2: i32, y2: i32, ends: bool, thin: bool) -> Vec<Tile> {
        // `EEGetTileLine` (uu_line.rs: one call, its CBTL record written once)
        let tiles = crate::units::uu_line::tile_line_recorded(
            &mut crate::units::uu_line::game_host(),
            x1,
            y1,
            x2,
            y2,
            ends,
            thin,
        );
        tiles.iter().map(|&(x, y)| [x, y]).collect()
    }
    fn reserved(&mut self, group: u64, point: [f32; 3]) -> bool {
        // SAFETY: a live group; three floats
        unsafe { ee_units_uu_host_reserved(group, point.as_ptr()) != 0 }
    }
    fn ground_point(&mut self, point: [f32; 3]) -> [f32; 3] {
        let mut out = [0.0; 3];
        // SAFETY: three floats each way
        unsafe { ee_units_uu_host_ground_point(point.as_ptr(), out.as_mut_ptr()) };
        out
    }
    fn terrain_tilt(&mut self, m: &GeTransformation, roll: bool) -> f32 {
        // SAFETY: sixteen floats
        unsafe { ee_units_uu_host_terrain_tilt(m.m.as_ptr().cast(), u8::from(roll)) }
    }
    fn enemy(&mut self, player: u64, other_id: i32) -> bool {
        // SAFETY: a live player
        read(|t| unsafe { PlayerView::of(t, player) }.is_some_and(|p| p.enemy(other_id as usize) != 0))
    }
    fn is_human(&mut self, player: u64) -> bool {
        // SAFETY: a live player
        unsafe { ee_units_uu_host_player(PLAYER_HUMAN, player, 0) != 0 }
    }
    fn unit_set_contains(&mut self, unit_type: u32, set: i32) -> bool {
        // SAFETY: a C++ read of the unit set manager (the type in the player's place)
        unsafe { ee_units_uu_host_player(PLAYER_UNIT_SET, u64::from(unit_type), set) != 0 }
    }
    fn player_units(&mut self, index: i32) -> Vec<u64> {
        read(|t| {
            let w = WorldView::global(t);
            w.and_then(|w| w.player(index as usize))
                .map(|p| p.units().map(|u| u.handle()).collect())
                .unwrap_or_default()
        })
    }
    fn part(&mut self, complex: u64, next: bool) -> u64 {
        // SAFETY: a live complex unit
        read(|t| {
            unsafe { UnitView::of(t, complex) }.map_or(0, |u| {
                if next {
                    u.next_part_handle()
                } else {
                    u.previous_part_handle()
                }
            })
        })
    }
    fn facing_quarter(&mut self, x: i32, y: i32) -> i32 {
        // SAFETY: a C++ read of the map
        unsafe { ee_units_uu_host_facing_quarter(x, y) }
    }
    fn resource_users(&mut self, unit: u64) -> i32 {
        // SAFETY: a live unit
        unsafe { ee_units_uu_host_resource_users(unit) }
    }
    fn random_long(&mut self, min: i32, max: i32) -> i32 {
        // SAFETY: the physics' generator
        unsafe { ee_units_uu_host_random_long(min, max) }
    }
    fn random_float(&mut self, min: f32, max: f32) -> f32 {
        // SAFETY: the physics' generator
        unsafe { ee_units_uu_host_random_float(min, max) }
    }
    fn location(&mut self, complex: u64) -> GeTransformation {
        // SAFETY: a live complex unit
        GeTransformation { m: read(|t| unsafe { UnitView::of(t, complex) }.map(|u| u.transform())).unwrap_or_default() }
    }
    fn set_animation(&mut self, complex: u64, animation: i32) {
        // SAFETY: a live complex unit
        unsafe { ee_units_uu_host_set_animation(complex, animation) }
    }
    fn set_transformation(&mut self, complex: u64, m: &GeTransformation) {
        // SAFETY: a live complex unit; sixteen floats
        unsafe { ee_units_uu_host_set_transformation(complex, m.m.as_ptr().cast()) }
    }
    fn is_tile_flat(&mut self, x: i32, y: i32) -> bool {
        // SAFETY: a C++ read of the map
        unsafe { ee_units_uu_host_tile_test(TILE_FLAT, 0, 0, x, y) != 0 }
    }
    fn tile_visible_to(&mut self, tile: u64, player: u64) -> bool {
        // SAFETY: a live tile and player
        unsafe { ee_units_uu_host_tile_test(TILE_VISIBLE, tile, player, 0, 0) != 0 }
    }
    fn group_units(&mut self, player: u64) -> i32 {
        // SAFETY: a live player
        unsafe { ee_units_uu_host_player(PLAYER_GROUP_UNITS, player, 0) }
    }
    fn group_continent(&mut self, player: u64) -> u16 {
        // SAFETY: a live player
        unsafe { ee_units_uu_host_player(PLAYER_GROUP_CONTINENT, player, 0) as u16 }
    }
    fn citizen_passability(&mut self, player: u64) -> i32 {
        // SAFETY: a live player
        unsafe { ee_units_uu_host_player(PLAYER_PASSABILITY, player, 0) }
    }
    fn saves_visibility(&mut self, player: u64) -> bool {
        // SAFETY: a live player
        read(|t| unsafe { PlayerView::of(t, player) }.is_some_and(|p| p.save_vis_map()))
    }
    fn continent_category(&mut self, continent: u64) -> u8 {
        // SAFETY: a continent the manager gave
        unsafe { crate::astar::cm_io::continent(continent).category }
    }
    fn are_connected_by_id(&mut self, a: u16, b: u16, flags: i32) -> bool {
        let (second, first) = (cm_lookup::continent_by_id(b), cm_lookup::continent_by_id(a));
        // SAFETY: as are_connected
        unsafe { crate::astar::cm_queries::ee_astar_cmq_are_connected(first, second, flags as c_long) }
    }
    fn regions_size(&mut self) -> i32 {
        with_regions(|r| r.size()).unwrap_or(0)
    }
    fn regions_len(&mut self, x: i32, y: i32) -> i32 {
        with_regions(|r| r.region(x as u32, y as u32).map_or(-1, |v| v.len() as i32)).unwrap_or(-1)
    }
    fn goal_kind(&mut self, unit: u64) -> i32 {
        // SAFETY: a live complex unit
        read(|t| unsafe { UnitView::of(t, unit) }.and_then(|u| u.goal()).map_or(-1, |g| i32::from(g.0)))
    }
    fn stats_range(&mut self, unit: u64) -> f32 {
        // SAFETY: a live complex unit
        read(|t| unsafe { UnitView::of(t, unit) }.and_then(|u| u.stats()).map_or(0.0, |s| s.range()))
    }
    fn center_offset(&mut self, complex: u64, x: f32, y: f32) -> (f32, f32) {
        let mut io = [x, y];
        // SAFETY: a live complex unit; two floats
        unsafe { ee_units_uu_host_center_offset(complex, io.as_mut_ptr()) };
        (io[0], io[1])
    }
    fn mesh_tile_is(&mut self, which: i32, x: u32, y: u32) -> bool {
        // SAFETY: a C++ read of the terrain mesh
        unsafe { ee_units_uu_host_tile_test(TILE_MESH_VALID + which, 0, 0, x as i32, y as i32) != 0 }
    }
    fn shore_tile(&mut self, x: i32, y: i32) -> bool {
        // SAFETY: the registered record
        crate::astar::world_view::cm()
            .is_some_and(|r| unsafe { crate::astar::cm::ee_astar_cm_is_shore(r, x as c_long, y as c_long) })
    }
    fn continent_of(&mut self, x: i32, y: i32, kind: i32) -> u16 {
        // SAFETY: the registered record (its own CNTQ scope)
        crate::astar::world_view::cm().map_or(0, |r| unsafe {
            crate::astar::cm_queries::ee_astar_cmq_region_id(r, x as c_long, y as c_long, kind as c_long)
        })
    }
    fn footprint_continent(&mut self, complex: u64, kind: i32) -> u16 {
        crate::astar::cm_unit::footprint_region(complex, kind as c_long)
    }
    fn in_same_area(&mut self, tiles: &[Tile], kind: i32) -> bool {
        let tiles = long_pairs(tiles);
        // SAFETY: the tiles' buffer
        unsafe { ee_units_uu_host_tiles_test(0, tiles.as_ptr(), tiles.len() as u32, kind) != 0 }
    }
    fn has_shore_tile(&mut self, tiles: &[Tile]) -> bool {
        let tiles = long_pairs(tiles);
        // SAFETY: the tiles' buffer
        unsafe { ee_units_uu_host_tiles_test(1, tiles.as_ptr(), tiles.len() as u32, 0) != 0 }
    }
    fn drop_point(&mut self, target: u64) -> [f32; 3] {
        uu_world_ops::drop_point(&mut Ops, target)
    }
    fn part_left_out(&mut self, piece: u64) -> bool {
        // SAFETY: a live complex unit
        unsafe { ee_units_uu_host_spot(1, piece, 0, 0, std::ptr::null_mut()) != 0 }
    }
    fn path_area_id(&mut self, x: i32, y: i32) -> u16 {
        uu_world_ops::path_area_id(&mut Ops, x, y)
    }
    fn gate_ends(&mut self, gate: u64) -> [[f32; 3]; 2] {
        uu_world_ops::gate_ends(&mut Ops, gate)
    }
}

/// `gContinentManager.CanGetTo(unit, x, y)` (the AI lane's C++: landing.rs).
pub(crate) fn can_get_to(unit: u64, x: i32, y: i32) -> bool {
    // SAFETY: a live unit
    unsafe { crate::astar::cm_unit::can_get_to_unit(unit, x as c_long, y as c_long) }
}

/// The continent manager's inline lookups (ee/EEAIContinentManager.h) over its record.
mod cm_lookup {
    use crate::astar::cm::EeAStarCmTable;
    use crate::astar::world_view::cm;
    use std::ffi::c_long;

    /// `GetContinentID(x, y)`: 0 off the map
    pub fn continent_id(x: i32, y: i32) -> u16 {
        cm().map_or(0, |r| crate::astar::cm_io::continent_id_at(r, x as c_long, y as c_long))
    }
    /// `GetContinentByID(id)` (0: none)
    pub fn continent_by_id(id: u16) -> u64 {
        cm().map_or(0, |r| {
            r.tables().tables[EeAStarCmTable::Continents as usize].get(id as usize).copied().unwrap_or(0)
        })
    }
    /// `AreAdjacent(row, column)`: `mTileFlags[row * mFlagsWidth + column]`
    pub fn adjacent(row: u16, column: u16) -> bool {
        let Some(r) = cm() else { return false };
        // SAFETY: the flags grid is mFlagsWidth squared (the C++ read it unchecked)
        unsafe { *r.tile_flags.add(row as usize * r.flags_width as usize + column as usize) }
    }
}

/// The citizen class's `HasGatherPoint(unit, resource)` (its UCBD note: gather_spot.rs's accessibility test).
pub(crate) fn has_gather_point(unit: u64, resource: u64) -> bool {
    world(WORLD_HAS_GATHER_POINT, unit, resource, 0, 0, 0, 0.0) != 0
}

/// The world host's statements as they are (uu_world_ops.rs's compound questions over them).
struct Ops;

impl uu_world_ops::UuOps for Ops {
    fn world(&mut self, which: i32, a: u64, b: u64, c: u64, x: i32, y: i32, f: f32) -> u64 {
        world(which, a, b, c, x, y, f)
    }
    fn world_f(&mut self, which: i32, a: u64, b: u64, f: f32) -> f32 {
        world_f(which, a, b, f)
    }
    fn spot(&mut self, which: i32, unit: u64, x: i32, y: i32, out: &mut [f32; 6]) -> i32 {
        // SAFETY: the objects are live ones the C++ gave; six floats
        unsafe { ee_units_uu_host_spot(which, unit, x, y, out.as_mut_ptr()) }
    }
    fn note_event(&mut self, a: i32, b: i32, c: i32) {
        crate::units::uu_note::ee_units_uu_note_event(a, b, c);
    }
    fn shape_wall(&mut self, complex: u64, corner: i32, raised: i32) {
        crate::units::uu_ffi::ee_units_uu_shape_wall(complex, corner, raised);
    }
}

fn world(which: i32, a: u64, b: u64, c: u64, x: i32, y: i32, f: f32) -> u64 {
    // SAFETY: the objects are live ones the C++ gave (each question's own contract in the C++ switch)
    unsafe { ee_units_uu_host_world(which, a, b, c, x, y, f) }
}

fn world_f(which: i32, a: u64, b: u64, f: f32) -> f32 {
    // SAFETY: as `world`
    unsafe { ee_units_uu_host_world_f(which, a, b, f) }
}

impl UuWorld for Game {
    fn line_of_sight(&mut self, complex: u64) -> f32 {
        // `GetLineOfSight()`: the stats' sight and the unit's own
        // SAFETY: a live complex unit
        read(|t| {
            let u = unsafe { UnitView::of(t, complex) }.expect("a live unit");
            let base: f32 = u.stats().map_or(0.0, |s| crate::world_field!(s.fields(), mLineOfSight));
            base + u.sight()
        })
    }
    fn units_near(&mut self, center: [f32; 3], radius: f32) -> Vec<u64> {
        crate::units::uu_ffi::units_near(center, radius)
    }
    fn gather_reach(&mut self, distance: f32, unit: u64, target: u64) -> bool {
        world_f(WORLD_F_GATHER_REACH, unit, target, distance) != 0.0
    }
    fn tile_unit(&mut self, tile: u64, unit: u64, add: bool) {
        uu_world_ops::tile_unit(&mut Ops, tile, unit, add);
    }
    fn clear_ambient(&mut self, unit: u64) {
        uu_world_ops::clear_ambient(&mut Ops, unit);
    }
    fn regions_footprint(&mut self, x: i32, y: i32, size: i32, complex: u64, add: bool) {
        world(WORLD_REGIONS_FOOTPRINT, complex, u64::from(add), size as u32 as u64, x, y, 0.0);
    }
    fn loading_saved_game(&mut self) -> bool {
        world(WORLD_LOADING, 0, 0, 0, 0, 0, 0.0) != 0
    }
    fn step_aside(&mut self, complex: u64, player: u64, class: u64, x: i32, y: i32, force: bool) {
        uu_world_ops::step_aside(&mut Ops, complex, player, class, x, y, force);
    }
    fn group_len(&mut self, group: u64) -> i32 {
        // SAFETY: a live active group (an EEGroup)
        read(|t| unsafe { GroupView::of(t, group) }.map_or(0, |g| g.len() as i32))
    }
    fn group_unit(&mut self, group: u64, i: i32) -> u64 {
        // SAFETY: as group_len
        read(|t| unsafe { GroupView::of(t, group) }.map_or(0, |g| g.unit_handle(i as usize)))
    }
    fn add_build_target(&mut self, unit: u64, piece: u64) {
        uu_world_ops::add_build_target(&mut Ops, unit, piece);
    }
    fn restart_idle(&mut self, unit: u64) {
        uu_world_ops::restart_idle(&mut Ops, unit);
    }
    fn can_produce(&mut self, player: u64, class: u64) -> bool {
        world(WORLD_CAN_PRODUCE, player, class, 0, 0, 0, 0.0) != 0
    }
    fn make_wall_piece(&mut self, class: u64, player: u64, x: i32, y: i32, corner: i32, foundation: bool) -> u64 {
        uu_world_ops::make_wall_piece(&mut Ops, class, player, x, y, corner, foundation)
    }
    fn wall_rise(&mut self, wall: u64) -> f32 {
        uu_world_ops::wall_rise(&mut Ops, wall)
    }
    fn map_size(&mut self) -> i32 {
        read(|t| WorldView::global(t).and_then(|w| w.map()).map_or(0, |m| m.size() as i32))
    }
    fn find_path(
        &mut self,
        unit: u64,
        from: Tile,
        to: Tile,
        bounds: [i32; 4],
        limit: u32,
        walls: bool,
    ) -> Option<Tile> {
        // FindPath(unit, the map, from, to, (left, top, right, bottom), kind 0xb, the limit, the walls' modifier)
        let map = read(|t| WorldView::global(t).and_then(|w| w.map()).map_or(0, |m| m.handle()));
        let modifier =
            if walls { crate::astar::modifier_obj::ee_astar_modifier_new_walls() } else { std::ptr::null_mut() };
        let window = crate::astar::pm_entry::EePathWindow {
            top: bounds[1],
            left: bounds[0],
            bottom: bounds[3],
            right: bounds[2],
        };
        // SAFETY: a live unit, the live map, the modifier made here
        let path = unsafe {
            crate::astar::pm_entry::find_path(
                unit,
                map,
                (from[0], from[1]),
                (to[0], to[1]),
                window,
                0xb,
                limit as i32,
                modifier as u64,
            )
        };
        // SAFETY: made above (or null)
        unsafe { crate::astar::modifier_obj::ee_astar_modifier_release(modifier) };
        path.first().map(|p| [p.x, p.y])
    }
    fn can_take(&mut self, container: u64, unit: u64, kind: u32) {
        world(WORLD_CAN_TAKE, container, unit, 0, kind as i32, 0, 0.0);
    }
    fn deposit_factor(&mut self) -> f32 {
        world_f(WORLD_F_DEPOSIT_FACTOR, 0, 0, 0.0)
    }
    fn last_resource(&mut self, unit: u64) -> u64 {
        world(WORLD_LAST_RESOURCE, unit, 0, 0, 0, 0, 0.0)
    }
    fn distance_squared_to_unit(&mut self, complex: u64, other: u64) -> f32 {
        crate::units::unit_geom::distance_squared_to_unit_of(complex, other)
    }
    fn deposit_effect(&mut self, unit: u64) {
        world(WORLD_DEPOSIT_EFFECT, unit, 0, 0, 0, 0, 0.0);
    }
    fn add_resource(&mut self, container: u64, unit: u64, amount: i32, carried: i32, near: bool, bonus: f32) {
        world(WORLD_ADD_RESOURCE, container, unit, u64::from(near), amount, carried, bonus);
    }
    fn statistic(&mut self, container: u64, series: i32, amount: i32) {
        world(WORLD_STATISTIC, container, 0, 0, series, amount, 0.0);
    }
    fn editor_transport_takes(&mut self, player: u64, unit_type: i32) -> bool {
        uu_world_ops::editor_transport_takes(&mut Ops, player, unit_type)
    }
    fn region_units(&mut self, x: i32, y: i32) -> Vec<u64> {
        with_regions(|r| r.region(x as u32, y as u32).cloned().unwrap_or_default()).unwrap_or_default()
    }
}

/// Tiles as `U2DPoint<long>` holds them (`long` pairs: 32 bits each on Windows, 64 elsewhere).
#[allow(clippy::unnecessary_cast)]
fn long_pairs(tiles: &[Tile]) -> Vec<[c_long; 2]> {
    tiles.iter().map(|t| [t[0] as c_long, t[1] as c_long]).collect()
}

/// A `std::vector<U2DPoint<long> >`'s tiles, read in place: a vector's first two words are its begin and end
/// (EEWorldLayout.cpp checks that), a tile two longs.
///
/// # Safety
/// `v` points to a live vector of tiles.
#[allow(clippy::unnecessary_cast)]
pub unsafe fn tiles_of(v: *const c_void) -> Vec<Tile> {
    // SAFETY: the caller's contract; [begin, end) holds the vector's tiles
    unsafe {
        let words = v as *const *const [c_long; 2];
        let (begin, end) = (*words, *words.add(1));
        if begin.is_null() {
            return Vec::new();
        }
        std::slice::from_raw_parts(begin, end.offset_from(begin) as usize)
            .iter()
            .map(|t| [t[0] as i32, t[1] as i32])
            .collect()
    }
}

/// Replaces a `std::vector<U2DPoint<long> >`'s tiles.
///
/// # Safety
/// `v` points to a live vector of tiles.
pub unsafe fn set_tiles(v: *mut c_void, tiles: &[Tile]) {
    let tiles = long_pairs(tiles);
    // SAFETY: the caller's contract
    unsafe { ee_units_uu_host_tiles_set(v, tiles.as_ptr(), tiles.len() as u32) }
}

/// A `std::vector<EEUnit*>`'s units.
///
/// # Safety
/// `v` points to a live vector of units.
pub unsafe fn units_of(v: *const c_void) -> Vec<u64> {
    // SAFETY: the caller's contract; the buffer holds the vector's length
    unsafe {
        let n = ee_units_uu_host_units_len(v) as usize;
        let mut out = vec![0; n];
        if n > 0 {
            ee_units_uu_host_units_get(v, out.as_mut_ptr());
        }
        out
    }
}

/// Replaces a `std::vector<EEUnit*>`'s units.
///
/// # Safety
/// `v` points to a live vector of units.
pub unsafe fn set_units(v: *mut c_void, units: &[u64]) {
    // SAFETY: the caller's contract
    unsafe { ee_units_uu_host_units_set(v, units.as_ptr(), units.len() as u32) }
}
