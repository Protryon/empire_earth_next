//! The unit utilities' entry points (the C++ functions of EEFOFindNewTargetNearOldTarget.cpp forward here; their
//! names and signatures stay the C++ callers'). Vectors cross as their C++ objects (`std::vector<U2DPoint<long> >*`,
//! `std::vector<EEUnit*>*`), read and refilled through the host; units, classes, players, tiles and maps as their
//! objects' addresses. Absent from a test binary.

#![cfg(not(test))]

use std::ffi::c_void;

use ee_render::ge::types::GeTransformation;

use crate::uu;
use crate::uu_host::{set_tiles, set_units, tiles_of, units_of, Game};
use crate::uu_place;
use crate::uu_spots;
use crate::uu_tiles::{self, EeUnitsUuSteps, EeUnitsUuWallPiece};
use crate::uu_world::{self, EeUnitsUuSoundView};

/// `EEGetSubTileSteps`.
#[no_mangle]
pub extern "C" fn ee_units_uu_sub_tile_steps(size: f32, length: i32, dx: i32, dy: i32, square: u8) -> EeUnitsUuSteps {
    uu_tiles::sub_tile_steps(size, length, dx, dy, square != 0)
}

/// `EEGetSquareRing` (`out` replaced).
///
/// # Safety
/// `out` is a live `std::vector<U2DPoint<long> >`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_square_ring(x: i32, y: i32, width: i32, height: i32, out: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { set_tiles(out, &uu_tiles::square_ring(x, y, width, height)) }
}

/// `EEGetSquareRingTiles` (`out` replaced).
///
/// # Safety
/// As [`ee_units_uu_square_ring`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_square_ring_tiles(x: i32, y: i32, radius: i32, out: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { set_tiles(out, &uu_tiles::square_ring_tiles(x, y, radius)) }
}

/// `EEGetTilesInRadius` (appended to `io`).
///
/// # Safety
/// As [`ee_units_uu_square_ring`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_tiles_in_radius(x: i32, y: i32, radius: u32, inclusive: u8, io: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe {
        let mut tiles = tiles_of(io);
        uu_tiles::tiles_in_radius(x, y, radius, inclusive != 0, &mut tiles);
        set_tiles(io, &tiles);
    }
}

/// `EEGetTilesNearPoint` (appended to `io`).
///
/// # Safety
/// As [`ee_units_uu_square_ring`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_tiles_near_point(x: f32, y: f32, distance: f32, io: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe {
        let mut tiles = tiles_of(io);
        uu_tiles::tiles_near_point(x, y, distance, &mut tiles);
        set_tiles(io, &tiles);
    }
}

/// `EEDropDiagonalCorners` (`which` 0) and `EEDropRepeatedTiles` (1) on `io`.
///
/// # Safety
/// As [`ee_units_uu_square_ring`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_drop_tiles(which: i32, io: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe {
        let mut tiles = tiles_of(io);
        if which == 0 {
            uu_tiles::drop_diagonal_corners(&mut tiles);
        } else {
            uu_tiles::drop_repeated_tiles(&mut tiles);
        }
        set_tiles(io, &tiles);
    }
}

/// `EETurnDirection`.
#[no_mangle]
pub extern "C" fn ee_units_uu_turn_direction(direction: i32, steps: i32, clockwise: u8) -> i32 {
    uu_tiles::turn_direction(direction, steps, clockwise != 0)
}

/// `EEGetTileInDirection`.
///
/// # Safety
/// `out` holds two `int32_t`s.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_tile_in_direction(
    x: i32,
    y: i32,
    direction: i32,
    distance: i32,
    out: *mut [i32; 2],
) {
    // SAFETY: the caller's contract
    unsafe { *out = uu_tiles::tile_in_direction(x, y, direction, distance) }
}

/// `EEMoveInDirection`.
///
/// # Safety
/// `out` holds two floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_move_in_direction(
    x: f32,
    y: f32,
    direction: i32,
    distance: i32,
    out: *mut [f32; 2],
) {
    // SAFETY: the caller's contract
    unsafe { *out = uu_tiles::move_in_direction(x, y, direction, distance) }
}

/// `EEGetTileDirection`.
#[no_mangle]
pub extern "C" fn ee_units_uu_tile_direction(x1: i32, y1: i32, x2: i32, y2: i32) -> i32 {
    uu_tiles::tile_direction(x1, y1, x2, y2)
}

/// `EEGetSlotTile`.
///
/// # Safety
/// `out` holds two `int32_t`s.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_slot_tile(x: i32, y: i32, direction: i32, out: *mut [i32; 2]) {
    // SAFETY: the caller's contract
    unsafe { *out = uu_tiles::slot_tile([x, y], direction) }
}

/// `EEOrientWallPiece`'s frame set and yaw.
#[no_mangle]
pub extern "C" fn ee_units_uu_wall_piece(corner: i32) -> EeUnitsUuWallPiece {
    uu_tiles::wall_piece(corner)
}

/// `EEIsCornerBlocked`.
#[no_mangle]
pub extern "C" fn ee_units_uu_is_corner_blocked(x1: i32, y1: i32, x2: i32, y2: i32) -> u8 {
    u8::from(uu::is_corner_blocked(&mut Game, x1, y1, x2, y2))
}

/// `EEGetBlockRing` (`io` cleared unless `append`).
///
/// # Safety
/// As [`ee_units_uu_square_ring`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_block_ring(
    corners: i32,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    io: *mut c_void,
    append: u8,
) {
    // SAFETY: the caller's contract
    unsafe {
        let mut tiles = if append != 0 { tiles_of(io) } else { Vec::new() };
        uu::block_ring(&mut Game, corners, x, y, width, height, &mut tiles, append != 0);
        set_tiles(io, &tiles);
    }
}

/// `EEGetRingCorners` (`out` replaced).
///
/// # Safety
/// As [`ee_units_uu_square_ring`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_ring_corners(
    corners: i32,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    out: *mut c_void,
) {
    // SAFETY: the caller's contract
    unsafe { set_tiles(out, &uu::ring_corners(&mut Game, corners, x, y, width, height)) }
}

/// `EERemoveEdgeTiles`.
///
/// # Safety
/// As [`ee_units_uu_square_ring`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_remove_edge_tiles(io: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe {
        let mut tiles = tiles_of(io);
        uu::remove_edge_tiles(&mut Game, &mut tiles);
        set_tiles(io, &tiles);
    }
}

/// `EEGetRingTiles` (`out` replaced).
///
/// # Safety
/// As [`ee_units_uu_square_ring`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_ring_tiles(x: i32, y: i32, size: i32, ring: i32, corners: i32, out: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { set_tiles(out, &uu::ring_tiles(&mut Game, x, y, size, ring, corners)) }
}

/// `EEGetSortedRingTiles` (`out` replaced).
///
/// # Safety
/// As [`ee_units_uu_square_ring`]; `from` holds three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_sorted_ring_tiles(
    from: *const [f32; 3],
    x: i32,
    y: i32,
    size: i32,
    range: i32,
    corners: i32,
    out: *mut c_void,
) {
    // SAFETY: the caller's contract
    unsafe { set_tiles(out, &uu::sorted_ring_tiles(&mut Game, *from, x, y, size, range, corners)) }
}

/// `EEGetUnitsOn` (`which` 0: appended to `io`), `EEGetUnitsOnTiles` (1); the number found (all of `io` for 1).
///
/// # Safety
/// `tiles` a live tile vector, `io` a live `std::vector<EEUnit*>`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_units_on(which: i32, tiles: *const c_void, io: *mut c_void) -> i32 {
    // SAFETY: the caller's contract
    unsafe {
        let at = tiles_of(tiles);
        let mut units = units_of(io);
        let n = if which == 0 {
            uu::units_on(&mut Game, &at, &mut units)
        } else {
            uu::units_on_tiles(&mut Game, &at, &mut units)
        };
        set_units(io, &units);
        n
    }
}

/// `EEGetUnitsOfTypeOnTiles` (`player` given) and `EEGetWallUnitsOfTypeOnTiles` (`has_player` 0): appended to
/// `io`; their number.
///
/// # Safety
/// As [`ee_units_uu_units_on`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_units_of_type_on_tiles(
    tiles: *const c_void,
    player: u64,
    has_player: u8,
    unit_type: i32,
    io: *mut c_void,
) -> i32 {
    // SAFETY: the caller's contract
    unsafe {
        let at = tiles_of(tiles);
        let mut units = units_of(io);
        let n = uu::units_of_type_on_tiles(&mut Game, &at, (has_player != 0).then_some(player), unit_type, &mut units);
        set_units(io, &units);
        n
    }
}

/// `EEGetTileUnitOfType` (`has_player` 0) and `EEGetWallAt`.
#[no_mangle]
pub extern "C" fn ee_units_uu_tile_unit_of_type(x: i32, y: i32, unit_type: u32, player: u64, has_player: u8) -> u64 {
    uu::tile_unit_of_type(&mut Game, [x, y], unit_type, (has_player != 0).then_some(player))
}

/// `EEGetPlayerUnitsOfType` (appended to `io`); their number.
///
/// # Safety
/// `io` a live `std::vector<EEUnit*>`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_player_units_of_type(unit_type: i32, index: i32, io: *mut c_void) -> i32 {
    // SAFETY: the caller's contract
    unsafe {
        let mut units = units_of(io);
        let n = uu::player_units_of_type(&mut Game, unit_type, index, &mut units);
        set_units(io, &units);
        n
    }
}

/// `EEGetFootprintTiles` (`which` 0), `EEGetResourceSpots` (1): into `io` as the C++ did.
///
/// # Safety
/// `unit` a live unit, `io` a live tile vector.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_unit_tiles(which: i32, unit: u64, io: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe {
        let mut tiles = tiles_of(io);
        if which == 0 {
            uu::footprint_tiles(&mut Game, unit, &mut tiles);
        } else {
            uu::resource_spots(&mut Game, unit, &mut tiles);
        }
        set_tiles(io, &tiles);
    }
}

/// `EEGetFarmDropSites` (appended to `io`).
///
/// # Safety
/// `unit` a live unit, `io` a live `std::vector<EEUnit*>`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_farm_drop_sites(unit: u64, io: *mut c_void) -> u8 {
    // SAFETY: the caller's contract
    unsafe {
        let mut units = units_of(io);
        let found = uu::farm_drop_sites(&mut Game, unit, &mut units);
        set_units(io, &units);
        u8::from(found)
    }
}

/// `EEFindFreeFarm`.
///
/// # Safety
/// `unit` a live unit, `ring` a live `int32_t`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_find_free_farm(unit: u64, free_only: u8, ring: *mut i32) -> u64 {
    // SAFETY: the caller's contract
    unsafe { uu::find_free_farm(&mut Game, unit, free_only != 0, &mut *ring) }
}

/// `EEGetWallLine` (`out` replaced).
///
/// # Safety
/// As [`ee_units_uu_square_ring`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_wall_line(x1: i32, y1: i32, x2: i32, y2: i32, out: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { set_tiles(out, &uu::wall_line(&mut Game, [x1, y1], [x2, y2])) }
}

/// `EEExtendWallLine` (into `io`).
///
/// # Safety
/// `tiles` and `io` live tile vectors, `player` a live player.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_extend_wall_line(
    tiles: *const c_void,
    player: u64,
    unit_type: u32,
    io: *mut c_void,
) -> i32 {
    // SAFETY: the caller's contract
    unsafe {
        let line = tiles_of(tiles);
        let mut out = tiles_of(io);
        let n = uu::extend_wall_line(&mut Game, &line, player, unit_type, &mut out);
        set_tiles(io, &out);
        n
    }
}

/// `EEPickWallPieces`: one piece per tile into `out`.
///
/// # Safety
/// `tiles` a live tile vector; `out` holds as many `int32_t`s.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_pick_wall_pieces(map: u64, tiles: *const c_void, out: *mut i32) {
    // SAFETY: the caller's contract
    unsafe {
        let pieces = uu::pick_wall_pieces(&mut Game, map, &tiles_of(tiles));
        std::ptr::copy_nonoverlapping(pieces.as_ptr(), out, pieces.len());
    }
}

/// `EEMakeWallJunctions`.
///
/// # Safety
/// `units` holds `count` live complex units; `placed` a live tile vector.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_make_wall_junctions(
    units: *const u64,
    count: u32,
    placed: *const c_void,
    player: u64,
    map: u64,
) {
    // SAFETY: the caller's contract
    unsafe {
        let list = if count == 0 { &[][..] } else { std::slice::from_raw_parts(units, count as usize) };
        uu::make_wall_junctions(&mut Game, list, &tiles_of(placed), player, map);
    }
}

/// `EEGetTileUnitStanding`.
#[no_mangle]
pub extern "C" fn ee_units_uu_tile_unit_standing(unit: u64, player: u64, unit_type: i32, ignore_owner: u8) -> i32 {
    uu::tile_unit_standing(&mut Game, unit, player, unit_type, ignore_owner != 0)
}

/// `EEGetBlockingUnits` (`standing` 2) and `EEGetMovableTileUnits` (1): `out` replaced.
///
/// # Safety
/// `out` a live `std::vector<EEUnit*>`; `tile` a live tile or 0.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_tile_units_standing(
    player: u64,
    unit_type: i32,
    tile: u64,
    ignore_owner: u8,
    standing: i32,
    out: *mut c_void,
) -> u8 {
    let mut units = Vec::new();
    let any = uu::tile_units_standing(&mut Game, player, unit_type, tile, ignore_owner != 0, standing, &mut units);
    // SAFETY: the caller's contract
    unsafe { set_units(out, &units) };
    u8::from(any)
}

/// `EEIsTileUnblocked`.
#[no_mangle]
pub extern "C" fn ee_units_uu_is_tile_unblocked(player: u64, unit_class: i32, tile: u64) -> u8 {
    u8::from(uu::is_tile_unblocked(&mut Game, player, unit_class, tile))
}

/// `EECanStandOn`.
#[no_mangle]
pub extern "C" fn ee_units_uu_can_stand_on(x: i32, y: i32, unit: u64) -> u8 {
    u8::from(uu::can_stand_on(&mut Game, [x, y], unit))
}

/// `EEFindPlaceOnTile`: `io` (the location, its z the caller's) gets the spot's x and y.
///
/// # Safety
/// `io` holds three floats.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_uu_find_place_on_tile(
    unit: u64,
    unit_class: u64,
    x: i32,
    y: i32,
    size: i32,
    to_x: i32,
    to_y: i32,
    square: i32,
    io: *mut [f32; 3],
) -> u8 {
    // SAFETY: the caller's contract
    unsafe { u8::from(uu::find_place_on_tile(&mut Game, unit, unit_class, x, y, size, to_x, to_y, square, &mut *io)) }
}

/// `EEGetTilesAround` (`out` replaced).
///
/// # Safety
/// As [`ee_units_uu_square_ring`].
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_uu_tiles_around(
    unit: u64,
    target: u64,
    x: i32,
    y: i32,
    size: i32,
    range: i32,
    corners: i32,
    virtual_call: u8,
    out: *mut c_void,
) {
    let tiles = uu::tiles_around(&mut Game, unit, target, x, y, size, range, corners, virtual_call != 0);
    // SAFETY: the caller's contract
    unsafe { set_tiles(out, &tiles) }
}

/// `EEPickTileNear`: `io` as [`ee_units_uu_find_place_on_tile`]'s.
///
/// # Safety
/// `tiles` a live tile vector; `io` holds three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_pick_tile_near(
    tiles: *const c_void,
    unit: u64,
    container: u64,
    ignore_blocked_line: u8,
    io: *mut [f32; 3],
) -> u8 {
    // SAFETY: the caller's contract
    unsafe {
        let at = tiles_of(tiles);
        u8::from(uu::pick_tile_near(&mut Game, &at, unit, container, ignore_blocked_line != 0, &mut *io))
    }
}

/// `EEFindPlacement`: `m` (a `GETransformation`'s matrix) is turned and moved to the place found.
///
/// # Safety
/// `point` holds three floats, `m` sixteen.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_uu_find_placement(
    point: *const [f32; 3],
    unit: u64,
    unit_class: u64,
    x: u32,
    y: u32,
    size: i32,
    rings: i32,
    corners: u8,
    include_center: u8,
    face_point: u8,
    linked: u8,
    m: *mut [[f32; 4]; 4],
    target: u64,
) -> u8 {
    // SAFETY: the caller's contract
    unsafe {
        let mut t = GeTransformation { m: *m };
        let found = uu::find_placement(
            &mut Game,
            *point,
            unit,
            unit_class,
            x,
            y,
            size,
            rings,
            corners != 0,
            include_center != 0,
            face_point != 0,
            linked != 0,
            &mut t,
            target,
        );
        *m = t.m;
        u8::from(found)
    }
}

/// `EEGetNearestFootprintTile`: the squared distance; the tile into `out`.
///
/// # Safety
/// `point` holds three floats, `out` two `int32_t`s.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_nearest_footprint_tile(
    unit: u64,
    point: *const [f32; 3],
    out: *mut [i32; 2],
) -> f32 {
    // SAFETY: the caller's contract
    unsafe {
        let (d, at) = uu::nearest_footprint_tile(&mut Game, unit, *point);
        *out = at;
        d
    }
}

/// `EETurnAway`.
#[no_mangle]
pub extern "C" fn ee_units_uu_turn_away(direction: i32, spread: i32) -> i32 {
    uu::turn_away(&mut Game, direction, spread)
}

/// `EEJitterHeading`.
#[no_mangle]
pub extern "C" fn ee_units_uu_jitter_heading(angle: f32, min: f32, max: f32) -> f32 {
    uu::jitter(&mut Game, angle, min, max)
}

/// `EECanPlaceUnitOnTile`: `flag` written as the C++ wrote it (only when the tile's blockers were asked).
///
/// # Safety
/// `flag` a live byte.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_uu_can_place_unit_on_tile(
    player: u64,
    unit: u64,
    unit_type: i32,
    x: i32,
    y: i32,
    skip_player_checks: u8,
    ignore_owner: u8,
    flag: *mut u8,
) -> u8 {
    // SAFETY: the caller's contract
    unsafe {
        let mut f = None;
        let ok = uu_place::can_place_unit_on_tile(
            &mut Game,
            player,
            unit,
            unit_type,
            x,
            y,
            skip_player_checks != 0,
            ignore_owner != 0,
            &mut f,
        );
        if let Some(f) = f {
            *flag = u8::from(f);
        }
        u8::from(ok)
    }
}

/// `EEBlockTakes` / `EEPlayerBlockTakes`.
#[no_mangle]
pub extern "C" fn ee_units_uu_block_takes(
    player: u64,
    unit_type: i32,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    ignore_owner: u8,
) -> u8 {
    u8::from(uu_place::block_takes(&mut Game, player, unit_type, x, y, width, height, ignore_owner != 0))
}

/// `EEAdjustNavalPath`'s smoothing: the kept points' indices into `kept` (room for `count`); their number.
///
/// # Safety
/// `points` holds `count` (x, y) pairs, `kept` `count` indices.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_naval_path(unit: u64, points: *const [f32; 2], count: u32, kept: *mut u32) -> u32 {
    // SAFETY: the caller's contract
    unsafe {
        let path = if count == 0 { &[][..] } else { std::slice::from_raw_parts(points, count as usize) };
        let out = uu_place::naval_path(&mut Game, unit, path);
        std::ptr::copy_nonoverlapping(out.as_ptr(), kept, out.len().min(count as usize));
        out.len() as u32
    }
}

/// `EEGetDockShipPlacement`: `m` (a `GETransformation`'s matrix) gets the place and yaw.
///
/// # Safety
/// `point` holds three floats, `m` sixteen; `unit` a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_dock_ship_placement(
    point: *const [f32; 3],
    unit_class: u64,
    unit: u64,
    face_point: u8,
    facing_only: u8,
    m: *mut [[f32; 4]; 4],
) -> u8 {
    // SAFETY: the caller's contract
    unsafe {
        let mut t = GeTransformation { m: *m };
        let found = uu_place::dock_ship_placement(
            &mut Game,
            *point,
            unit_class,
            unit,
            face_point != 0,
            facing_only != 0,
            &mut t,
        );
        *m = t.m;
        u8::from(found)
    }
}

/// `EEFindOpenSpot` (0), `EEFindResourceSpot` (1), `EEFindNextFreeTile` (2), `EEGetWallSpot` (3): `io` (the
/// location, its z the caller's) gets the spot.
///
/// # Safety
/// `unit` and `target` live units; `io` holds three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_spot(which: i32, unit: u64, target: u64, io: *mut [f32; 3]) -> u8 {
    // SAFETY: the caller's contract
    let out = unsafe { &mut *io };
    let found = match which {
        0 => uu_spots::find_open_spot(&mut Game, unit, target, out),
        1 => uu_spots::find_resource_spot(&mut Game, unit, target, out),
        2 => uu_spots::find_next_free_tile(&mut Game, unit, target, out),
        _ => uu_spots::wall_spot(&mut Game, unit, target, out),
    };
    u8::from(found)
}

/// `EEGetAttackPart`.
#[no_mangle]
pub extern "C" fn ee_units_uu_attack_part(unit: u64, target: u64) -> u64 {
    uu_spots::attack_part(&mut Game, unit, target)
}

// ---- round 2 (uu_world.rs) ----

/// `EEGetSeabedDepth` (`which` 0) and `EEAdjustDiveDepth` (1): the point's new height.
///
/// # Safety
/// `unit` is a live unit; `at` holds three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_seabed(which: i32, unit: u64, at: *const [f32; 3]) -> f32 {
    // SAFETY: the caller's contract
    let at = unsafe { *at };
    if which == 0 {
        uu_world::seabed_depth(&mut Game, unit, at)
    } else {
        uu_world::adjust_dive_depth(&mut Game, unit, at)
    }
}

/// `EEGetUnitTiles` (appended to `io`).
///
/// # Safety
/// `unit` is a live unit; `io` a live tile vector.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_unit_tiles_margin(unit: u64, margin: f32, io: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe {
        let mut tiles = tiles_of(io);
        uu_world::unit_tiles(&mut Game, unit, &mut tiles, margin);
        set_tiles(io, &tiles);
    }
}

/// `EEGetSelectionBounds` (`bounds`: left, top, right, bottom).
///
/// # Safety
/// `units` is a live unit vector; `bounds` holds four floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_selection_bounds(units: *const c_void, bounds: *mut [f32; 4], filter: i32) {
    // SAFETY: the caller's contract
    unsafe { uu_world::selection_bounds(&mut Game, &units_of(units), &mut *bounds, filter) }
}

/// `EEGetUnitRadius` (`which` 0, two complex units) and `EEGetAngleFromNorth` (1, `from` and `to`).
///
/// # Safety
/// The units are live; the points hold three floats when `which` is 1.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_angle(
    which: i32,
    unit: u64,
    other: u64,
    from: *const [f32; 3],
    to: *const [f32; 3],
) -> f32 {
    if which == 0 {
        uu_world::unit_radius(&mut Game, unit, other)
    } else {
        // SAFETY: the caller's contract
        unsafe { uu_world::angle_from_north(*from, *to) }
    }
}

/// `EEIsWithinReach` (`at` null) and `EEIsWithinReachOf`.
///
/// # Safety
/// The units are live; `at` is null or holds three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_within_reach(unit: u64, target: u64, at: *const [f32; 3]) -> u8 {
    // SAFETY: the caller's contract
    let at = unsafe { at.as_ref().copied() };
    u8::from(uu_world::within_reach(&mut Game, unit, target, at))
}

/// `EEIsFootprintVisibleTo`.
#[no_mangle]
pub extern "C" fn ee_units_uu_footprint_visible(unit: u64, player: u64) -> u8 {
    u8::from(uu_world::footprint_visible(&mut Game, unit, player))
}

/// `EEGetEscapePoint` (`io` left alone without attackers).
///
/// # Safety
/// `unit` is a live complex unit; `io` holds two floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_escape_point(unit: u64, io: *mut [f32; 2]) -> u8 {
    // SAFETY: the caller's contract
    u8::from(uu_world::escape_point(&mut Game, unit, unsafe { &mut *io }))
}

/// `EEGetEnemiesAround` (`out` replaced).
///
/// # Safety
/// `unit` is a live complex unit; `out` a live unit vector.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_enemies_around(unit: u64, out: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { set_units(out, &uu_world::enemies_around(&mut Game, unit)) }
}

/// `EEAddToFootprintTiles` (`which` 0), `EERemoveFromFootprintTiles` (1), `EEAddBuildingRegion` (2),
/// `EERemoveBuildingRegion` (3).
#[no_mangle]
pub extern "C" fn ee_units_uu_footprint(which: i32, unit: u64) {
    match which {
        0 | 1 => uu_world::footprint_link(&mut Game, unit, which == 0),
        _ => uu_world::building_region(&mut Game, unit, which == 2),
    }
}

/// `EEStepAsideFromSite`.
#[no_mangle]
pub extern "C" fn ee_units_uu_step_aside(unit: u64, class: u64, player: u64, x: i32, y: i32, force: u8) {
    uu_world::step_aside_from_site(&mut Game, unit, class, player, [x, y], force != 0);
}

/// `EEBuildWall`.
///
/// # Safety
/// `group` is a live active group; `pieces` a live unit vector.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_build_wall(group: u64, pieces: *const c_void) {
    // SAFETY: the caller's contract
    let pieces = unsafe { units_of(pieces) };
    uu_world::build_wall(&mut Game, group, &pieces);
}

/// `EEPlanWall` (the new pieces appended to `out`).
///
/// # Safety
/// `class` and `player` are live; `out` a live unit vector.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_uu_plan_wall(
    start_x: f32,
    start_y: f32,
    end_x: f32,
    end_y: f32,
    class: u64,
    player: u64,
    foundation: u8,
    skip_player_checks: u8,
    out: *mut c_void,
) {
    // SAFETY: the caller's contract
    unsafe {
        let mut pieces = units_of(out);
        uu_world::plan_wall(
            &mut Game,
            [start_x, start_y],
            [end_x, end_y],
            class,
            player,
            foundation != 0,
            skip_player_checks != 0,
            &mut pieces,
        );
        set_units(out, &pieces);
    }
}

/// `EEShapeWall` (its raised corners unused).
#[no_mangle]
pub extern "C" fn ee_units_uu_shape_wall(complex: u64, corner: i32) {
    uu_world::shape_wall(&mut Game, complex, corner);
}

/// `EEIsTileTopAbove` (`wall`: the wall found, 0 for none).
///
/// # Safety
/// `wall` points to a u64.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_tile_top_above(x: i32, y: i32, height: f32, bare: u8, wall: *mut u64) -> u8 {
    let (above, found) = uu_world::tile_top_above(&mut Game, x, y, height, bare != 0);
    // SAFETY: the caller's contract
    unsafe { *wall = found };
    u8::from(above)
}

/// `EEIsLineBlocked` (`wall` written only when a tile was asked, `last`: the last free tile).
///
/// # Safety
/// `wall` points to a u64, `written` to a byte, `last` to two ints.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_line_blocked(
    x1: i32,
    y1: i32,
    x2: i32,
    y2: i32,
    wall: *mut u64,
    written: *mut u8,
    last: *mut [i32; 2],
) -> u8 {
    let (blocked, found, last_x, last_y) = uu_world::line_blocked(&mut Game, x1, y1, x2, y2);
    // SAFETY: the caller's contract
    unsafe {
        *written = u8::from(found.is_some());
        *wall = found.unwrap_or(0);
        *last = [last_x, last_y];
    }
    u8::from(blocked)
}

/// `EEGetPathLengthTo` (`tile` in and out).
///
/// # Safety
/// `unit` is a live complex unit; `tile` holds two ints.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_path_length_to(unit: u64, tile: *mut [i32; 2], limit: u32, walls: u8) -> f32 {
    // SAFETY: the caller's contract
    uu_world::path_length_to(&mut Game, unit, unsafe { &mut *tile }, limit, walls != 0)
}

/// `EEComputeSoundParameters` (`out`: volume, pan, pitch) over the camera's view the caller gathered.
///
/// # Safety
/// `view` points to a view; `location` holds three floats, `out` three.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_sound_parameters(
    view: *const EeUnitsUuSoundView,
    volume: f32,
    location: *const [f32; 3],
    out: *mut [f32; 3],
) {
    // SAFETY: the caller's contract
    unsafe { *out = uu_world::sound_parameters(&*view, volume, *location) }
}

/// `EEGetDopplerPitch`'s factor (`pitch` written unless silent or still).
///
/// # Safety
/// `location` holds a matrix, `camera` three floats, `pitch` one.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_doppler_pitch(
    volume: f32,
    velocity: f32,
    location: *const [[f32; 4]; 4],
    camera: *const [f32; 3],
    pitch: *mut f32,
) {
    // SAFETY: the caller's contract
    unsafe {
        if let Some(p) = uu_world::doppler_pitch(volume, velocity, &GeTransformation { m: *location }, *camera) {
            *pitch = p;
        }
    }
}

/// `EEEditorCanPlace`.
///
/// # Safety
/// `at` holds three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_editor_can_place(
    player: u64,
    unit_type: i32,
    at: *const [f32; 3],
    x: i32,
    y: i32,
) -> u8 {
    // SAFETY: the caller's contract
    u8::from(uu_world::editor_can_place(&mut Game, player, unit_type, unsafe { *at }, x, y))
}

/// `EEDepositLoad`.
#[no_mangle]
pub extern "C" fn ee_units_uu_deposit_load(unit: u64, container: u64) {
    uu_world::deposit_load(&mut Game, unit, container);
}
