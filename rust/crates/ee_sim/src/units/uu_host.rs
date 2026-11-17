//! The unit utilities' host in the game: every [`UuHost`] call one C++ function (User Interface
//! Thread/EEFOFindNewTargetNearOldTarget.cpp, `ee_units_uu_host_*`), and the C++ vectors the entry points read and
//! fill. Absent from a test binary.

#![cfg(not(test))]

use std::ffi::c_void;

use ee_base::math::types::GeTransformation;

use crate::units::uu::{EeUnitsUuClass, EeUnitsUuUnit, UuHost};
use crate::units::uu_tiles::Tile;
use crate::units::uu_world::UuWorld;

extern "C" {
    fn ee_units_uu_host_map() -> u64;
    fn ee_units_uu_host_map_tile(map: u64, x: i32, y: i32) -> u64;
    fn ee_units_uu_host_tile_terrain(tile: u64) -> i32;
    fn ee_units_uu_host_tile_height(tile: u64) -> f32;
    fn ee_units_uu_host_tile_units(tile: u64, out: *mut u64, capacity: u32) -> u32;
    fn ee_units_uu_host_is_edge_tile(x: i32, y: i32) -> u8;
    fn ee_units_uu_host_raised_corners(map: u64, x: i32, y: i32) -> i32;
    fn ee_units_uu_host_mesh_z(map: u64, x: i32, y: i32) -> f32;
    fn ee_units_uu_host_mesh_ground(x: f32, y: f32) -> f32;
    fn ee_units_uu_host_unit(obj: u64, out: *mut EeUnitsUuUnit);
    fn ee_units_uu_host_class(class_obj: u64, unit_type: i32, out: *mut EeUnitsUuClass);
    fn ee_units_uu_host_position(obj: u64, out: *mut f32);
    fn ee_units_uu_host_passable_by(obj: u64, by: u64) -> u8;
    fn ee_units_uu_host_is_foundation(obj: u64) -> u8;
    fn ee_units_uu_host_can_move_on_terrain(class_obj: u64, unit: u64, terrain: i32) -> u8;
    fn ee_units_uu_host_passable_flags(class_obj: u64, unit: u64) -> i32;
    fn ee_units_uu_host_surrounding_tiles(target: u64, virtual_call: u8, out: *mut [i32; 2], capacity: u32) -> u32;
    fn ee_units_uu_host_container_continent(container: u64, unit: u64) -> u64;
    fn ee_units_uu_host_continent(which: i32, a: u64, b: u64, c: u64) -> u64;
    fn ee_units_uu_host_tile_line(
        x1: i32,
        y1: i32,
        x2: i32,
        y2: i32,
        ends: u8,
        thin: u8,
        out: *mut [i32; 2],
        capacity: u32,
    ) -> u32;
    fn ee_units_uu_host_reserved(group: u64, point: *const f32) -> u8;
    fn ee_units_uu_host_ground_point(point: *const f32, out: *mut f32);
    fn ee_units_uu_host_terrain_tilt(m: *const f32, roll: u8) -> f32;
    fn ee_units_uu_host_player(which: i32, player: u64, value: i32) -> i32;
    fn ee_units_uu_host_player_units(index: i32, out: *mut u64, capacity: u32) -> u32;
    fn ee_units_uu_host_part(complex: u64, next: u8) -> u64;
    fn ee_units_uu_host_facing_quarter(x: i32, y: i32) -> i32;
    fn ee_units_uu_host_resource_users(unit: u64) -> i32;
    fn ee_units_uu_host_random_long(min: i32, max: i32) -> i32;
    fn ee_units_uu_host_random_float(min: f32, max: f32) -> f32;
    fn ee_units_uu_host_location(complex: u64, out: *mut f32);
    fn ee_units_uu_host_set_animation(complex: u64, animation: i32);
    fn ee_units_uu_host_set_transformation(complex: u64, m: *const f32);
    fn ee_units_uu_host_tiles_len(v: *const c_void) -> u32;
    fn ee_units_uu_host_tiles_get(v: *const c_void, out: *mut [i32; 2]);
    fn ee_units_uu_host_tiles_set(v: *mut c_void, tiles: *const [i32; 2], count: u32);
    fn ee_units_uu_host_units_len(v: *const c_void) -> u32;
    fn ee_units_uu_host_units_get(v: *const c_void, out: *mut u64);
    fn ee_units_uu_host_units_set(v: *mut c_void, units: *const u64, count: u32);
    fn ee_units_uu_host_tile_test(which: i32, tile: u64, player: u64, x: i32, y: i32) -> u8;
    fn ee_units_uu_host_regions(which: i32, x: i32, y: i32) -> i32;
    fn ee_units_uu_host_goal_kind(unit: u64) -> i32;
    fn ee_units_uu_host_range(unit: u64) -> f32;
    fn ee_units_uu_host_center_offset(complex: u64, io: *mut f32);
    fn ee_units_uu_host_tiles_test(which: i32, tiles: *const [i32; 2], count: u32, kind: i32) -> u8;
    fn ee_units_uu_host_spot(which: i32, unit: u64, x: i32, y: i32, out: *mut f32) -> i32;
    fn ee_units_uu_host_world(which: i32, a: u64, b: u64, c: u64, x: i32, y: i32, f: f32) -> u64;
    fn ee_units_uu_host_world_f(which: i32, a: u64, b: u64, f: f32) -> f32;
    fn ee_units_uu_host_units_near(center: *const f32, radius: f32, out: *mut u64, capacity: u32) -> u32;
    fn ee_units_uu_host_region_units(x: i32, y: i32, out: *mut u64, capacity: u32) -> u32;
    fn ee_units_uu_host_find_path(
        unit: u64,
        from: *const i32,
        to: *const i32,
        bounds: *const i32,
        limit: u32,
        walls: u8,
        first: *mut i32,
    ) -> u8;
}

/// `ee_units_uu_host_world`'s questions and writes (one C++ switch).
const WORLD_GROUP_LEN: i32 = 0;
const WORLD_GROUP_UNIT: i32 = 1;
const WORLD_ADD_BUILD_TARGET: i32 = 2;
const WORLD_RESTART_IDLE: i32 = 3;
const WORLD_CAN_PRODUCE: i32 = 4;
const WORLD_MAKE_WALL_PIECE: i32 = 5;
const WORLD_TILE_UNIT: i32 = 6;
const WORLD_CLEAR_AMBIENT: i32 = 7;
const WORLD_REGIONS_FOOTPRINT: i32 = 8;
const WORLD_LOADING: i32 = 9;
const WORLD_STEP_ASIDE: i32 = 10;
const WORLD_CAN_TAKE: i32 = 11;
const WORLD_LAST_RESOURCE: i32 = 12;
const WORLD_DEPOSIT_EFFECT: i32 = 13;
const WORLD_ADD_RESOURCE: i32 = 14;
const WORLD_STATISTIC: i32 = 15;
const WORLD_MAP_SIZE: i32 = 16;
const WORLD_EDITOR_TRANSPORT: i32 = 17;
/// `ee_units_uu_host_world_f`'s.
const WORLD_F_LINE_OF_SIGHT: i32 = 0;
const WORLD_F_GATHER_REACH: i32 = 1;
const WORLD_F_WALL_RISE: i32 = 2;
const WORLD_F_DISTANCE_TO_UNIT: i32 = 3;
const WORLD_F_DEPOSIT_FACTOR: i32 = 4;

/// `ee_units_uu_host_continent`'s questions (one C++ switch).
const CONTINENT_ID: i32 = 0;
const CONTINENT_BY_ID: i32 = 1;
const CONTINENT_AT: i32 = 2;
const CONTINENT_NUMBER: i32 = 3;
const CONTINENT_CONNECTED: i32 = 4;
const CONTINENT_CONNECTED_FOR_UNIT: i32 = 5;
const CONTINENT_TILE_FLAG: i32 = 6;
const CONTINENT_LINK: i32 = 7;
const CONTINENT_CATEGORY: i32 = 8;
const CONTINENT_CONNECTED_BY_ID: i32 = 9;
const CONTINENT_OF: i32 = 10;
const CONTINENT_SHORE: i32 = 11;
const CONTINENT_FOOTPRINT: i32 = 12;
/// `ee_units_uu_host_player`'s questions.
const PLAYER_ENEMY: i32 = 0;
const PLAYER_HUMAN: i32 = 1;
const PLAYER_UNIT_SET: i32 = 2;
const PLAYER_GROUP_UNITS: i32 = 3;
const PLAYER_GROUP_CONTINENT: i32 = 4;
const PLAYER_PASSABILITY: i32 = 5;
const PLAYER_SAVES_VISIBILITY: i32 = 6;
/// `ee_units_uu_host_tile_test`'s questions.
const TILE_FLAT: i32 = 0;
const TILE_VISIBLE: i32 = 1;
const TILE_MESH_VALID: i32 = 2;

/// A list the C++ writes into a buffer (its count first; again with the room it asked for when that was more).
fn read_list<T: Copy + Default>(mut f: impl FnMut(*mut T, u32) -> u32) -> Vec<T> {
    let mut out = vec![T::default(); 32];
    let n = f(out.as_mut_ptr(), out.len() as u32) as usize;
    if n > out.len() {
        out.resize(n, T::default());
        f(out.as_mut_ptr(), n as u32);
    }
    out.truncate(n);
    out
}

/// The game's world.
pub struct Game;

impl UuHost for Game {
    fn map(&mut self) -> u64 {
        // SAFETY: a C++ read of gWorld
        unsafe { ee_units_uu_host_map() }
    }
    fn map_tile(&mut self, map: u64, x: i32, y: i32) -> u64 {
        // SAFETY: the map is the C++'s (0 answers 0)
        unsafe { ee_units_uu_host_map_tile(map, x, y) }
    }
    fn tile_terrain(&mut self, tile: u64) -> i32 {
        // SAFETY: a tile the C++ gave
        unsafe { ee_units_uu_host_tile_terrain(tile) }
    }
    fn tile_height(&mut self, tile: u64) -> f32 {
        // SAFETY: as tile_terrain
        unsafe { ee_units_uu_host_tile_height(tile) }
    }
    fn tile_units(&mut self, tile: u64, out: &mut Vec<u64>) {
        // SAFETY: as tile_terrain; the buffer holds `capacity` units
        *out = read_list(|p, n| unsafe { ee_units_uu_host_tile_units(tile, p, n) });
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
        let mut out = EeUnitsUuUnit::default();
        // SAFETY: a live unit the C++ gave (0 leaves the default)
        unsafe { ee_units_uu_host_unit(obj, &mut out) };
        out
    }
    fn class_of_type(&mut self, unit_type: i32) -> EeUnitsUuClass {
        let mut out = EeUnitsUuClass::default();
        // SAFETY: the class table's entry
        unsafe { ee_units_uu_host_class(0, unit_type, &mut out) };
        out
    }
    fn class(&mut self, class_obj: u64) -> EeUnitsUuClass {
        let mut out = EeUnitsUuClass::default();
        // SAFETY: a class the C++ gave
        unsafe { ee_units_uu_host_class(class_obj, -1, &mut out) };
        out
    }
    fn position(&mut self, obj: u64) -> [f32; 3] {
        let mut out = [0.0; 3];
        // SAFETY: a live unit; three floats
        unsafe { ee_units_uu_host_position(obj, out.as_mut_ptr()) };
        out
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
        // SAFETY: a live unit; the buffer holds `capacity` tiles
        read_list(|p, n| unsafe { ee_units_uu_host_surrounding_tiles(target, u8::from(virtual_call), p, n) })
    }
    fn container_continent(&mut self, container: u64, unit: u64) -> u64 {
        // SAFETY: live units
        unsafe { ee_units_uu_host_container_continent(container, unit) }
    }
    fn continent_id(&mut self, x: i32, y: i32) -> u16 {
        // SAFETY: a C++ read of the continent manager
        unsafe { ee_units_uu_host_continent(CONTINENT_ID, x as u32 as u64, y as u32 as u64, 0) as u16 }
    }
    fn continent_by_id(&mut self, id: u16) -> u64 {
        // SAFETY: as continent_id
        unsafe { ee_units_uu_host_continent(CONTINENT_BY_ID, u64::from(id), 0, 0) }
    }
    fn continent_at(&mut self, x: i32, y: i32) -> u64 {
        // SAFETY: as continent_id
        unsafe { ee_units_uu_host_continent(CONTINENT_AT, x as u32 as u64, y as u32 as u64, 0) }
    }
    fn continent_number(&mut self, continent: u64) -> u16 {
        // SAFETY: a continent the C++ gave
        unsafe { ee_units_uu_host_continent(CONTINENT_NUMBER, continent, 0, 0) as u16 }
    }
    fn are_connected(&mut self, a: u64, b: u64, flags: i32) -> bool {
        // SAFETY: continents the C++ gave
        unsafe { ee_units_uu_host_continent(CONTINENT_CONNECTED, a, b, flags as u32 as u64) != 0 }
    }
    fn are_connected_for_unit(&mut self, unit: u64, a: u64, b: u64) -> bool {
        // SAFETY: as are_connected; a live unit
        unsafe { ee_units_uu_host_continent(CONTINENT_CONNECTED_FOR_UNIT, a, b, unit) != 0 }
    }
    fn continent_tile_flag(&mut self, x: i32, y: i32) -> i32 {
        // SAFETY: as continent_id
        unsafe { ee_units_uu_host_continent(CONTINENT_TILE_FLAG, x as u32 as u64, y as u32 as u64, 0) as i32 }
    }
    fn continent_link(&mut self, row: u16, column: u16) -> bool {
        // SAFETY: as continent_id
        unsafe { ee_units_uu_host_continent(CONTINENT_LINK, u64::from(row), u64::from(column), 0) != 0 }
    }
    fn line_blocked(&mut self, x1: i32, y1: i32, x2: i32, y2: i32) -> bool {
        crate::units::uu_world::line_blocked(self, x1, y1, x2, y2).0
    }
    fn tile_line(&mut self, x1: i32, y1: i32, x2: i32, y2: i32, ends: bool, thin: bool) -> Vec<Tile> {
        // one call (the C++ leaf writes its CBTL record each time): the room of the line's most tiles, two a step
        // and its two ends (EEGetTileLine's own bound)
        let room =
            ((x1.wrapping_sub(x2)).unsigned_abs() as usize + (y1.wrapping_sub(y2)).unsigned_abs() as usize) * 4 + 8;
        let mut out = vec![[0; 2]; room / 2];
        // SAFETY: the buffer holds `room / 2` tiles
        let n = unsafe {
            ee_units_uu_host_tile_line(
                x1,
                y1,
                x2,
                y2,
                u8::from(ends),
                u8::from(thin),
                out.as_mut_ptr(),
                out.len() as u32,
            )
        };
        out.truncate((n as usize).min(out.len()));
        out
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
        unsafe { ee_units_uu_host_player(PLAYER_ENEMY, player, other_id) != 0 }
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
        // SAFETY: the buffer holds `capacity` units
        read_list(|p, n| unsafe { ee_units_uu_host_player_units(index, p, n) })
    }
    fn part(&mut self, complex: u64, next: bool) -> u64 {
        // SAFETY: a live complex unit
        unsafe { ee_units_uu_host_part(complex, u8::from(next)) }
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
        let mut m = GeTransformation::IDENTITY;
        // SAFETY: a live complex unit; sixteen floats
        unsafe { ee_units_uu_host_location(complex, m.m.as_mut_ptr().cast()) };
        m
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
        unsafe { ee_units_uu_host_player(PLAYER_SAVES_VISIBILITY, player, 0) != 0 }
    }
    fn continent_category(&mut self, continent: u64) -> u8 {
        // SAFETY: a continent the C++ gave
        unsafe { ee_units_uu_host_continent(CONTINENT_CATEGORY, continent, 0, 0) as u8 }
    }
    fn are_connected_by_id(&mut self, a: u16, b: u16, flags: i32) -> bool {
        // SAFETY: a C++ read of the continent manager
        unsafe {
            ee_units_uu_host_continent(CONTINENT_CONNECTED_BY_ID, u64::from(a), u64::from(b), flags as u32 as u64) != 0
        }
    }
    fn regions_size(&mut self) -> i32 {
        // SAFETY: a C++ read of the map's regions
        unsafe { ee_units_uu_host_regions(0, 0, 0) }
    }
    fn regions_len(&mut self, x: i32, y: i32) -> i32 {
        // SAFETY: as regions_size
        unsafe { ee_units_uu_host_regions(1, x, y) }
    }
    fn goal_kind(&mut self, unit: u64) -> i32 {
        // SAFETY: a live complex unit
        unsafe { ee_units_uu_host_goal_kind(unit) }
    }
    fn stats_range(&mut self, unit: u64) -> f32 {
        // SAFETY: a live complex unit
        unsafe { ee_units_uu_host_range(unit) }
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
        // SAFETY: as continent_id
        unsafe { ee_units_uu_host_continent(CONTINENT_SHORE, x as u32 as u64, y as u32 as u64, 0) != 0 }
    }
    fn continent_of(&mut self, x: i32, y: i32, kind: i32) -> u16 {
        // SAFETY: as continent_id
        unsafe { ee_units_uu_host_continent(CONTINENT_OF, x as u32 as u64, y as u32 as u64, kind as u32 as u64) as u16 }
    }
    fn footprint_continent(&mut self, complex: u64, kind: i32) -> u16 {
        // SAFETY: a live complex unit
        unsafe { ee_units_uu_host_continent(CONTINENT_FOOTPRINT, complex, 0, kind as u32 as u64) as u16 }
    }
    fn in_same_area(&mut self, tiles: &[Tile], kind: i32) -> bool {
        // SAFETY: the tiles' buffer
        unsafe { ee_units_uu_host_tiles_test(0, tiles.as_ptr(), tiles.len() as u32, kind) != 0 }
    }
    fn has_shore_tile(&mut self, tiles: &[Tile]) -> bool {
        // SAFETY: the tiles' buffer
        unsafe { ee_units_uu_host_tiles_test(1, tiles.as_ptr(), tiles.len() as u32, 0) != 0 }
    }
    fn drop_point(&mut self, target: u64) -> [f32; 3] {
        let mut out = [0.0; 3];
        // SAFETY: a live unit; three floats
        unsafe { ee_units_uu_host_spot(0, target, 0, 0, out.as_mut_ptr()) };
        out
    }
    fn part_left_out(&mut self, piece: u64) -> bool {
        // SAFETY: a live complex unit
        unsafe { ee_units_uu_host_spot(1, piece, 0, 0, std::ptr::null_mut()) != 0 }
    }
    fn path_area_id(&mut self, x: i32, y: i32) -> u16 {
        // SAFETY: a C++ read of the continent manager
        unsafe { ee_units_uu_host_spot(2, 0, x, y, std::ptr::null_mut()) as u16 }
    }
    fn gate_ends(&mut self, gate: u64) -> [[f32; 3]; 2] {
        let mut out = [[0.0; 3]; 2];
        // SAFETY: a live gate piece; six floats
        unsafe { ee_units_uu_host_spot(3, gate, 0, 0, out.as_mut_ptr().cast()) };
        out
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
        world_f(WORLD_F_LINE_OF_SIGHT, complex, 0, 0.0)
    }
    fn units_near(&mut self, center: [f32; 3], radius: f32) -> Vec<u64> {
        // SAFETY: three floats; the buffer holds `capacity` units
        read_list(|p, n| unsafe { ee_units_uu_host_units_near(center.as_ptr(), radius, p, n) })
    }
    fn gather_reach(&mut self, distance: f32, unit: u64, target: u64) -> bool {
        world_f(WORLD_F_GATHER_REACH, unit, target, distance) != 0.0
    }
    fn tile_unit(&mut self, tile: u64, unit: u64, add: bool) {
        world(WORLD_TILE_UNIT, tile, unit, 0, i32::from(add), 0, 0.0);
    }
    fn clear_ambient(&mut self, unit: u64) {
        world(WORLD_CLEAR_AMBIENT, unit, 0, 0, 0, 0, 0.0);
    }
    fn regions_footprint(&mut self, x: i32, y: i32, size: i32, complex: u64, add: bool) {
        world(WORLD_REGIONS_FOOTPRINT, complex, u64::from(add), size as u32 as u64, x, y, 0.0);
    }
    fn loading_saved_game(&mut self) -> bool {
        world(WORLD_LOADING, 0, 0, 0, 0, 0, 0.0) != 0
    }
    fn step_aside(&mut self, complex: u64, player: u64, class: u64, x: i32, y: i32, force: bool) {
        world(WORLD_STEP_ASIDE, complex, player, class, x, y, if force { 1.0 } else { 0.0 });
    }
    fn group_len(&mut self, group: u64) -> i32 {
        world(WORLD_GROUP_LEN, group, 0, 0, 0, 0, 0.0) as i32
    }
    fn group_unit(&mut self, group: u64, i: i32) -> u64 {
        world(WORLD_GROUP_UNIT, group, 0, 0, i, 0, 0.0)
    }
    fn add_build_target(&mut self, unit: u64, piece: u64) {
        world(WORLD_ADD_BUILD_TARGET, unit, piece, 0, 0, 0, 0.0);
    }
    fn restart_idle(&mut self, unit: u64) {
        world(WORLD_RESTART_IDLE, unit, 0, 0, 0, 0, 0.0);
    }
    fn can_produce(&mut self, player: u64, class: u64) -> bool {
        world(WORLD_CAN_PRODUCE, player, class, 0, 0, 0, 0.0) != 0
    }
    fn make_wall_piece(&mut self, class: u64, player: u64, x: i32, y: i32, corner: i32, foundation: bool) -> u64 {
        world(WORLD_MAKE_WALL_PIECE, class, player, corner as u32 as u64, x, y, if foundation { 1.0 } else { 0.0 })
    }
    fn wall_rise(&mut self, wall: u64) -> f32 {
        world_f(WORLD_F_WALL_RISE, wall, 0, 0.0)
    }
    fn map_size(&mut self) -> i32 {
        world(WORLD_MAP_SIZE, 0, 0, 0, 0, 0, 0.0) as i32
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
        let mut first = [0; 2];
        // SAFETY: a live unit; two, two and four ints; two out
        let found = unsafe {
            ee_units_uu_host_find_path(
                unit,
                from.as_ptr(),
                to.as_ptr(),
                bounds.as_ptr(),
                limit,
                u8::from(walls),
                first.as_mut_ptr(),
            )
        };
        (found != 0).then_some(first)
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
        world_f(WORLD_F_DISTANCE_TO_UNIT, complex, other, 0.0)
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
        world(WORLD_EDITOR_TRANSPORT, player, 0, 0, unit_type, 0, 0.0) != 0
    }
    fn region_units(&mut self, x: i32, y: i32) -> Vec<u64> {
        // SAFETY: the buffer holds `capacity` units
        read_list(|p, n| unsafe { ee_units_uu_host_region_units(x, y, p, n) })
    }
}

/// A `std::vector<U2DPoint<long> >`'s tiles.
///
/// # Safety
/// `v` points to a live vector of tiles.
pub unsafe fn tiles_of(v: *const c_void) -> Vec<Tile> {
    // SAFETY: the caller's contract; the buffer holds the vector's length
    unsafe {
        let n = ee_units_uu_host_tiles_len(v) as usize;
        let mut out = vec![[0; 2]; n];
        if n > 0 {
            ee_units_uu_host_tiles_get(v, out.as_mut_ptr());
        }
        out
    }
}

/// Replaces a `std::vector<U2DPoint<long> >`'s tiles.
///
/// # Safety
/// `v` points to a live vector of tiles.
pub unsafe fn set_tiles(v: *mut c_void, tiles: &[Tile]) {
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
