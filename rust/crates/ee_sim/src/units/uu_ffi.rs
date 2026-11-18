//! The unit utilities' entry points (the C++ functions of EEFOFindNewTargetNearOldTarget.cpp forward here; their
//! names and signatures stay the C++ callers'). Vectors cross as their C++ objects (`std::vector<U2DPoint<long> >*`,
//! `std::vector<EEUnit*>*`), read and refilled through the host; units, classes, players, tiles and maps as their
//! objects' addresses. Each entry point a C++ utility forwards to notes its call (UUTL, uu_note.rs: the kind, the
//! inputs, the result, the outputs, in the C++ notes' order). Absent from a test binary.

#![cfg(not(test))]

use std::ffi::c_void;

use ee_base::math::types::GeTransformation;

use crate::units::uu;
use crate::units::uu_host::{set_tiles, set_units, tiles_of, units_of, Game};
use crate::units::uu_note::Note;
use crate::units::uu_place;
use crate::units::uu_spots;
use crate::units::uu_tiles::{self, EeUnitsUuSteps, EeUnitsUuWallPiece};
use crate::units::uu_world::{self, EeUnitsUuSoundView, UuWorld};
use crate::world::{UnitView, WorldRead};

/// A unit's view for a note's reads (the unit is the caller's live unit).
fn with_unit<R: Default>(unit: u64, f: impl FnOnce(UnitView<'_>) -> R) -> R {
    // SAFETY: the utility's caller holds the world and passes a live unit (or 0)
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    unsafe { UnitView::of(&t, unit) }.map(f).unwrap_or_default()
}

/// A unit's tile and its class's size (`GetTileX()`, `GetTileY()`, `mUnitClass->mTileSize`).
fn foot_of(unit: u64) -> (i64, i64, i64) {
    with_unit(unit, |u| {
        let (x, y) = u.tile_xy();
        let size = u.class().map_or(0, |c| {
            let s: std::os::raw::c_long = crate::world_field!(c.fields(), mTileSize);
            s as i64
        });
        (x, y, size)
    })
}

/// `EEGetSubTileSteps`.
#[no_mangle]
pub extern "C" fn ee_units_uu_sub_tile_steps(size: f32, length: i32, dx: i32, dy: i32, square: u8) -> EeUnitsUuSteps {
    uu_tiles::sub_tile_steps(size, length, dx, dy, square != 0)
}

/// `EEGetSquareRing` (19; `square` 1: `EEGetSideTiles`, 20) (`out` replaced).
///
/// # Safety
/// `out` is a live `std::vector<U2DPoint<long> >`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_square_ring(
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    square: u8,
    out: *mut c_void,
) {
    let n = Note::begin(if square != 0 { 20 } else { 19 });
    n.i(x.into()).i(y.into()).i(width.into());
    if square == 0 {
        n.i(height.into());
    }
    let tiles = uu_tiles::square_ring(x, y, width, height);
    // SAFETY: the caller's contract
    unsafe { set_tiles(out, &tiles) };
    n.t(&tiles);
}

/// `EEGetSquareRingTiles` (33) (`out` replaced).
///
/// # Safety
/// As [`ee_units_uu_square_ring`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_square_ring_tiles(x: i32, y: i32, radius: i32, out: *mut c_void) {
    let n = Note::begin(33);
    n.i(x.into()).i(y.into()).i(radius.into());
    let tiles = uu_tiles::square_ring_tiles(x, y, radius);
    // SAFETY: the caller's contract
    unsafe { set_tiles(out, &tiles) };
    n.t(&tiles);
}

/// `EEGetTilesInRadius` (35) (appended to `io`).
///
/// # Safety
/// As [`ee_units_uu_square_ring`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_tiles_in_radius(x: i32, y: i32, radius: u32, inclusive: u8, io: *mut c_void) {
    let n = Note::begin(35);
    // SAFETY: the caller's contract
    unsafe {
        let mut tiles = tiles_of(io);
        n.i(x.into()).i(y.into()).i(radius.into()).i(inclusive.into()).t(&tiles);
        uu_tiles::tiles_in_radius(x, y, radius, inclusive != 0, &mut tiles);
        set_tiles(io, &tiles);
        n.t(&tiles);
    }
}

/// `EEGetTilesNearPoint`'s tiles (appended to `io`), not noted (the app capture's own reads).
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

/// `EEGetTilesNearPoint` (34) (appended to `io`).
///
/// # Safety
/// As [`ee_units_uu_square_ring`]; `point` holds three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_get_tiles_near_point(point: *const [f32; 3], distance: f32, io: *mut c_void) {
    let n = Note::begin(34);
    // SAFETY: the caller's contract
    unsafe {
        let p = *point;
        let mut tiles = tiles_of(io);
        n.p(p).f(distance).t(&tiles);
        uu_tiles::tiles_near_point(p[0], p[1], distance, &mut tiles);
        set_tiles(io, &tiles);
        n.t(&tiles);
    }
}

/// `EEDropDiagonalCorners` (`which` 0, 15) and `EEDropRepeatedTiles` (1, 46) on `io`.
///
/// # Safety
/// As [`ee_units_uu_square_ring`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_drop_tiles(which: i32, io: *mut c_void) {
    let n = Note::begin(if which == 0 { 15 } else { 46 });
    // SAFETY: the caller's contract
    unsafe {
        let mut tiles = tiles_of(io);
        n.t(&tiles);
        if which == 0 {
            uu_tiles::drop_diagonal_corners(&mut tiles);
        } else {
            uu_tiles::drop_repeated_tiles(&mut tiles);
        }
        set_tiles(io, &tiles);
        n.t(&tiles);
    }
}

/// `EETurnDirection`.
#[no_mangle]
pub extern "C" fn ee_units_uu_turn_direction(direction: i32, steps: i32, clockwise: u8) -> i32 {
    uu_tiles::turn_direction(direction, steps, clockwise != 0)
}

/// `EEGetNextTile` (30).
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
    let n = Note::begin(30);
    n.i(x.into()).i(y.into()).i(direction.into()).i(distance.into());
    let t = uu_tiles::tile_in_direction(x, y, direction, distance);
    // SAFETY: the caller's contract
    unsafe { *out = t };
    n.i(t[0].into()).i(t[1].into());
}

/// `EEMoveInDirection` (31).
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
    let n = Note::begin(31);
    n.f(x).f(y).i(direction.into()).i(distance.into());
    let p = uu_tiles::move_in_direction(x, y, direction, distance);
    // SAFETY: the caller's contract
    unsafe { *out = p };
    n.f(p[0]).f(p[1]);
}

/// `EEGetTileDirection` (14).
#[no_mangle]
pub extern "C" fn ee_units_uu_tile_direction(x1: i32, y1: i32, x2: i32, y2: i32) -> i32 {
    let n = Note::begin(14);
    n.i(x1.into()).i(y1.into()).i(x2.into()).i(y2.into());
    let r = uu_tiles::tile_direction(x1, y1, x2, y2);
    n.i(r.into());
    r
}

/// `EEGetSlotTile` (45).
///
/// # Safety
/// `out` holds two `int32_t`s.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_slot_tile(x: i32, y: i32, direction: i32, out: *mut [i32; 2]) {
    let n = Note::begin(45);
    n.i(x.into()).i(y.into()).i(direction.into());
    let t = uu_tiles::slot_tile([x, y], direction);
    // SAFETY: the caller's contract
    unsafe { *out = t };
    n.i(t[0].into()).i(t[1].into());
}

/// `EEOrientWallPiece`'s frame set and yaw.
#[no_mangle]
pub extern "C" fn ee_units_uu_wall_piece(corner: i32) -> EeUnitsUuWallPiece {
    uu_tiles::wall_piece(corner)
}

/// `EEIsCornerBlocked` (23).
#[no_mangle]
pub extern "C" fn ee_units_uu_is_corner_blocked(x1: i32, y1: i32, x2: i32, y2: i32) -> u8 {
    let n = Note::begin(23);
    n.i(x1.into()).i(y1.into()).i(x2.into()).i(y2.into());
    let r = uu::is_corner_blocked(&mut Game, x1, y1, x2, y2);
    n.b(r);
    u8::from(r)
}

/// `EEGetBlockRing` (21; `square` 1: `EEGetSquareBlockRing`, 22) (`io` cleared unless `append`).
///
/// # Safety
/// As [`ee_units_uu_square_ring`].
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_uu_block_ring(
    corners: i32,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    io: *mut c_void,
    append: u8,
    square: u8,
) {
    let n = Note::begin(if square != 0 { 22 } else { 21 });
    n.i(corners.into()).i(x.into()).i(y.into()).i(width.into());
    if square == 0 {
        n.i(height.into());
    }
    n.i(append.into());
    // SAFETY: the caller's contract
    unsafe {
        let before = if n.on() || append != 0 { tiles_of(io) } else { Vec::new() };
        n.t(&before);
        let mut tiles = if append != 0 { before } else { Vec::new() };
        uu::block_ring(&mut Game, corners, x, y, width, height, &mut tiles, append != 0);
        set_tiles(io, &tiles);
        n.t(&tiles);
    }
}

/// `EEGetRingCorners` (24; `square` 1: `EEGetCornerTiles`, 25) (`out` replaced).
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
    square: u8,
    out: *mut c_void,
) {
    let n = Note::begin(if square != 0 { 25 } else { 24 });
    n.i(corners.into()).i(x.into()).i(y.into()).i(width.into());
    if square == 0 {
        n.i(height.into());
    }
    let tiles = uu::ring_corners(&mut Game, corners, x, y, width, height);
    // SAFETY: the caller's contract
    unsafe { set_tiles(out, &tiles) };
    n.t(&tiles);
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

/// `EEGetSortedRingTiles` (6) (`out` replaced).
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
    let n = Note::begin(6);
    // SAFETY: the caller's contract
    unsafe {
        n.p(*from).i(x.into()).i(y.into()).i(size.into()).i(range.into()).i(corners.into());
        let tiles = uu::sorted_ring_tiles(&mut Game, *from, x, y, size, range, corners);
        set_tiles(out, &tiles);
        n.t(&tiles);
    }
}

/// `EEGetUnitsOn` (`which` 0, 42: appended to `io`), `EEGetUnitsOnTiles` (1, not noted); the number found (all of
/// `io` for 1).
///
/// # Safety
/// `tiles` a live tile vector, `io` a live `std::vector<EEUnit*>`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_units_on(which: i32, tiles: *const c_void, io: *mut c_void) -> i32 {
    if which != 0 {
        // SAFETY: the caller's contract
        return unsafe {
            let mut units = units_of(io);
            let count = uu::units_on_tiles(&mut Game, &tiles_of(tiles), &mut units);
            set_units(io, &units);
            count
        };
    }
    let n = Note::begin(42);
    // SAFETY: the caller's contract
    unsafe {
        let at = tiles_of(tiles);
        let mut units = units_of(io);
        n.t(&at).us(&units);
        let count = uu::units_on(&mut Game, &at, &mut units);
        set_units(io, &units);
        n.i(count.into()).us(&units);
        count
    }
}

/// `EEGetUnitsOnTile` (41): `io` replaced by the units of tile (x, y); their number.
///
/// # Safety
/// `io` a live `std::vector<EEUnit*>`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_units_on_tile(x: i32, y: i32, io: *mut c_void) -> i32 {
    let n = Note::begin(41);
    n.i(x.into()).i(y.into());
    let mut units = Vec::new();
    let count = uu::units_on(&mut Game, &[[x, y]], &mut units);
    // SAFETY: the caller's contract
    unsafe { set_units(io, &units) };
    n.i(count.into()).us(&units);
    count
}

/// `EEGetUnitsInRectangle` (38) over a screen rectangle: the units of its tiles (`UMFloor` of each side, every
/// column from the left, each from the top) appended to `io` as `EEGetUnitsOnTiles` does; their number.
///
/// # Safety
/// `io` a live `std::vector<EEUnit*>`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_units_in_rectangle(
    left: f32,
    top: f32,
    right: f32,
    bottom: f32,
    io: *mut c_void,
) -> i32 {
    let n = Note::begin(38);
    // SAFETY: the caller's contract
    unsafe {
        let mut units = units_of(io);
        n.f(left).f(top).f(right).f(bottom).us(&units);
        let floor = ee_base::math::um::floor;
        let (r, b, l, t) = (floor(right), floor(bottom), floor(left), floor(top));
        let mut tiles = Vec::new();
        for x in l..=r {
            for y in t..=b {
                tiles.push([x, y]);
            }
        }
        let count = uu::units_on_tiles(&mut Game, &tiles, &mut units);
        set_units(io, &units);
        n.i(count.into()).us(&units);
        count
    }
}

/// `EEGetWallUnitsOfTypeOnTiles` (43): appended to `io`; their number.
///
/// # Safety
/// As [`ee_units_uu_units_on`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_units_of_type_on_tiles(
    tiles: *const c_void,
    unit_type: i32,
    io: *mut c_void,
) -> i32 {
    let n = Note::begin(43);
    // SAFETY: the caller's contract
    unsafe {
        let at = tiles_of(tiles);
        let mut units = units_of(io);
        n.t(&at).i(unit_type.into()).us(&units);
        let count = uu::units_of_type_on_tiles(&mut Game, &at, None, unit_type, &mut units);
        set_units(io, &units);
        n.i(count.into()).us(&units);
        count
    }
}

/// `EEGetTileUnitOfType` (39).
#[no_mangle]
pub extern "C" fn ee_units_uu_tile_unit_of_type(x: i32, y: i32, unit_type: u32) -> u64 {
    let n = Note::begin(39);
    n.i(x.into()).i(y.into()).i(unit_type.into());
    let r = uu::tile_unit_of_type(&mut Game, [x, y], unit_type, None);
    n.u(r);
    r
}

/// `EEGetPlayerUnitsOfType` (40) (appended to `io`); their number.
///
/// # Safety
/// `io` a live `std::vector<EEUnit*>`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_player_units_of_type(unit_type: i32, index: i32, io: *mut c_void) -> i32 {
    let n = Note::begin(40);
    // SAFETY: the caller's contract
    unsafe {
        let mut units = units_of(io);
        n.i(unit_type.into()).i(index.into()).us(&units);
        let count = uu::player_units_of_type(&mut Game, unit_type, index, &mut units);
        set_units(io, &units);
        n.i(count.into()).us(&units);
        count
    }
}

/// `EEGetFootprintTiles` (`which` 0, 36), `EEGetResourceSpots` (1, 44): into `io` as the C++ did.
///
/// # Safety
/// `unit` a live unit, `io` a live tile vector.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_unit_tiles(which: i32, unit: u64, io: *mut c_void) {
    let n = Note::begin(if which == 0 { 36 } else { 44 });
    // SAFETY: the caller's contract
    unsafe {
        let mut tiles = tiles_of(io);
        n.u(unit).t(&tiles);
        if which == 0 {
            uu::footprint_tiles(&mut Game, unit, &mut tiles);
        } else {
            uu::resource_spots(&mut Game, unit, &mut tiles);
        }
        set_tiles(io, &tiles);
        n.t(&tiles);
    }
}

/// `EEGetFarmDropSites` (16) (appended to `io`).
///
/// # Safety
/// `unit` a live unit, `io` a live `std::vector<EEUnit*>`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_farm_drop_sites(unit: u64, io: *mut c_void) -> u8 {
    let n = Note::begin(16);
    // SAFETY: the caller's contract
    unsafe {
        let mut units = units_of(io);
        n.u(unit).us(&units);
        let found = uu::farm_drop_sites(&mut Game, unit, &mut units);
        set_units(io, &units);
        n.b(found).us(&units);
        u8::from(found)
    }
}

/// `EEFindFreeFarm` (17): the farm's complex unit (0: none).
///
/// # Safety
/// `unit` a live unit, `ring` a live `int32_t`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_find_free_farm(unit: u64, free_only: u8, ring: *mut i32) -> u64 {
    let n = Note::begin(17);
    // SAFETY: the caller's contract
    unsafe {
        n.u(unit).i(free_only.into()).i((*ring).into());
        let farm = uu::find_free_farm(&mut Game, unit, free_only != 0, &mut *ring);
        let complex = with_unit(farm, |f| f.complex_unit_handle());
        n.u(complex).i((*ring).into());
        complex
    }
}

/// `EEGetWallLine` (37) (`out` replaced).
///
/// # Safety
/// As [`ee_units_uu_square_ring`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_wall_line(x1: i32, y1: i32, x2: i32, y2: i32, out: *mut c_void) {
    let n = Note::begin(37);
    n.i(x1.into()).i(y1.into()).i(x2.into()).i(y2.into());
    let tiles = uu::wall_line(&mut Game, [x1, y1], [x2, y2]);
    // SAFETY: the caller's contract
    unsafe { set_tiles(out, &tiles) };
    n.t(&tiles);
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

/// `EEGetTileUnitStanding` (11).
#[no_mangle]
pub extern "C" fn ee_units_uu_tile_unit_standing(unit: u64, player: u64, unit_type: i32, ignore_owner: u8) -> i32 {
    let n = Note::begin(11);
    n.u(unit).pl(player).i(unit_type.into()).i(ignore_owner.into());
    let r = uu::tile_unit_standing(&mut Game, unit, player, unit_type, ignore_owner != 0);
    // (the C++ returned it as a char)
    n.i(i64::from(r as i8));
    r
}

/// `EEGetBlockingUnits` (`standing` 2, 2) and `EEGetMovableTileUnits` (1, 3): `out` replaced.
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
    let n = Note::begin(if standing == 2 { 2 } else { 3 });
    n.pl(player).i(unit_type.into()).i(ignore_owner.into());
    let mut units = Vec::new();
    let any = uu::tile_units_standing(&mut Game, player, unit_type, tile, ignore_owner != 0, standing, &mut units);
    // SAFETY: the caller's contract
    unsafe { set_units(out, &units) };
    n.b(any).us(&units);
    u8::from(any)
}

/// `EEIsTileUnblocked` (50).
#[no_mangle]
pub extern "C" fn ee_units_uu_is_tile_unblocked(player: u64, unit_class: i32, tile: u64) -> u8 {
    let n = Note::begin(50);
    n.pl(player).i(unit_class.into());
    let r = uu::is_tile_unblocked(&mut Game, player, unit_class, tile);
    n.b(r);
    u8::from(r)
}

/// `EECanStandOn` (8).
#[no_mangle]
pub extern "C" fn ee_units_uu_can_stand_on(x: i32, y: i32, unit: u64) -> u8 {
    let n = Note::begin(8);
    n.i(x.into()).i(y.into()).u(unit);
    let r = uu::can_stand_on(&mut Game, [x, y], unit);
    n.b(r);
    u8::from(r)
}

/// `EEFindPlaceOnTile` (4): `io` (the location) gets the spot's x and y (its z stays).
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
    let n = Note::begin(4);
    n.u(unit).c(unit_class).i(x.into()).i(y.into()).i(size.into()).i(to_x.into()).i(to_y.into()).i(square.into());
    // SAFETY: the caller's contract
    unsafe {
        let mut p = *io;
        let found = uu::find_place_on_tile(&mut Game, unit, unit_class, x, y, size, to_x, to_y, square, &mut p);
        (*io)[0] = p[0];
        (*io)[1] = p[1];
        n.b(found).p(*io);
        u8::from(found)
    }
}

/// `EEGetTilesAroundUnit` (5): the tiles around `target` (its tile and size) for `unit` (`out` replaced).
///
/// # Safety
/// As [`ee_units_uu_square_ring`]; `target` a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_tiles_around(
    unit: u64,
    target: u64,
    range: i32,
    corners: i32,
    virtual_call: u8,
    out: *mut c_void,
) {
    let n = Note::begin(5);
    n.u(unit).u(target).i(range.into()).i(corners.into()).i(virtual_call.into());
    let (x, y, size) = foot_of(target);
    let tiles =
        uu::tiles_around(&mut Game, unit, target, x as i32, y as i32, size as i32, range, corners, virtual_call != 0);
    // SAFETY: the caller's contract
    unsafe { set_tiles(out, &tiles) };
    n.t(&tiles);
}

/// `EEPickTileNear` (7): `io` as [`ee_units_uu_find_place_on_tile`]'s.
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
    let n = Note::begin(7);
    // SAFETY: the caller's contract
    unsafe {
        let at = tiles_of(tiles);
        n.t(&at).u(unit).u(container).i(ignore_blocked_line.into());
        let mut p = *io;
        let found = uu::pick_tile_near(&mut Game, &at, unit, container, ignore_blocked_line != 0, &mut p);
        (*io)[0] = p[0];
        (*io)[1] = p[1];
        n.b(found).p(*io);
        u8::from(found)
    }
}

/// `EEFindPlacement` (10): `m` (a `GETransformation`'s matrix) is turned and moved to the place found.
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
    let n = Note::begin(10);
    // SAFETY: the caller's contract
    unsafe {
        n.p(*point).u(unit).c(unit_class).i(x.into()).i(y.into()).i(size.into()).i(rings.into());
        n.i(corners.into()).i(include_center.into()).i(face_point.into()).i(linked.into()).u(target);
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
        n.b(found).m(&t.m);
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

/// `EETurnAway` (32).
#[no_mangle]
pub extern "C" fn ee_units_uu_turn_away(direction: i32, spread: i32) -> i32 {
    let n = Note::begin(32);
    n.i(direction.into()).i(spread.into());
    let r = uu::turn_away(&mut Game, direction, spread);
    n.i(r.into());
    r
}

/// `EEJitterHeading` (48).
#[no_mangle]
pub extern "C" fn ee_units_uu_jitter_heading(angle: f32, min: f32, max: f32) -> f32 {
    let n = Note::begin(48);
    n.f(angle).f(min).f(max);
    let r = uu::jitter(&mut Game, angle, min, max);
    n.f(r);
    r
}

/// `EECanPlaceUnitOnTile` (49): `flag` written as the C++ wrote it (only when the tile's blockers were asked).
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
    ignore_owner: i32,
    flag: *mut u8,
) -> u8 {
    let n = Note::begin(49);
    n.pl(player).u(unit).i(unit_type.into()).i(x.into()).i(y.into()).i(skip_player_checks.into());
    n.i(ignore_owner.into());
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
        n.b(ok);
        u8::from(ok)
    }
}

/// `EEPlayerBlockTakes` (13).
#[no_mangle]
pub extern "C" fn ee_units_uu_block_takes(
    player: u64,
    unit_type: i32,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    ignore_owner: i32,
) -> u8 {
    let n = Note::begin(13);
    n.pl(player).i(unit_type.into()).i(x.into()).i(y.into()).i(width.into()).i(height.into()).i(ignore_owner.into());
    let r = uu_place::block_takes(&mut Game, player, unit_type, x, y, width, height, ignore_owner != 0);
    n.b(r);
    u8::from(r)
}

/// `EEBlockTakes` (12): [`ee_units_uu_block_takes`] for the unit's player (none for no unit).
#[no_mangle]
pub extern "C" fn ee_units_uu_unit_block_takes(
    unit: u64,
    unit_type: i32,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    ignore_owner: i32,
) -> u8 {
    let n = Note::begin(12);
    n.u(unit).i(unit_type.into()).i(x.into()).i(y.into()).i(width.into()).i(height.into()).i(ignore_owner.into());
    let player = with_unit(unit, |u| u.player().map_or(0, |p| p.handle()));
    let r = uu_place::block_takes(&mut Game, player, unit_type, x, y, width, height, ignore_owner != 0);
    n.b(r);
    u8::from(r)
}

/// `EEAdjustNavalPath` (1): with `smooth`, the kept points' indices into `kept` (room for `count`); their number
/// (every point without `smooth`).
///
/// # Safety
/// `points` holds `count` points, `kept` `count` indices.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_naval_path(
    unit: u64,
    points: *const [f32; 3],
    count: u32,
    smooth: u8,
    kept: *mut u32,
) -> u32 {
    let n = Note::begin(1);
    // SAFETY: the caller's contract
    unsafe {
        let path = if count == 0 { &[][..] } else { std::slice::from_raw_parts(points, count as usize) };
        n.u(unit).ps(path).i(smooth.into());
        let out: Vec<u32> = if smooth != 0 {
            let xy: Vec<[f32; 2]> = path.iter().map(|p| [p[0], p[1]]).collect();
            uu_place::naval_path(&mut Game, unit, &xy)
        } else {
            (0..count).collect()
        };
        std::ptr::copy_nonoverlapping(out.as_ptr(), kept, out.len().min(count as usize));
        if n.on() {
            let new_path: Vec<[f32; 3]> = out.iter().map(|&i| path[i as usize]).collect();
            n.ps(&new_path);
        }
        out.len() as u32
    }
}

/// `EEGetDockShipPlacement` (9): `m` (a `GETransformation`'s matrix) gets the place and yaw.
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
    let n = Note::begin(9);
    // SAFETY: the caller's contract
    let (found, m) = unsafe {
        n.p(*point).c(unit_class).u(unit).i(face_point.into()).i(facing_only.into());
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
        (found, t.m)
    };
    if n.on() {
        let yaw: f32 =
            with_unit(unit, |u| u.complex_unit().map_or(0.0, |c| crate::world_field!(c.complex(), mLaunchYaw)));
        n.b(found).m(&m).f(yaw);
    }
    u8::from(found)
}

/// `EEFindOpenSpot` (0), `EEFindResourceSpot` (1, 27), `EEFindNextFreeTile` (2, 28), `EEGetWallSpot` (3, 29): `io`
/// (the location) gets the spot.
///
/// # Safety
/// `unit` and `target` live units; `io` holds three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_spot(which: i32, unit: u64, target: u64, io: *mut [f32; 3]) -> u8 {
    // SAFETY: the caller's contract
    let out = unsafe { &mut *io };
    if which == 0 {
        return u8::from(uu_spots::find_open_spot(&mut Game, unit, target, out));
    }
    let n = Note::begin(26 + which as u32);
    n.u(unit).u(target);
    let found = match which {
        1 => uu_spots::find_resource_spot(&mut Game, unit, target, out),
        2 => uu_spots::find_next_free_tile(&mut Game, unit, target, out),
        _ => uu_spots::wall_spot(&mut Game, unit, target, out),
    };
    n.b(found).p(*out);
    u8::from(found)
}

/// `EEGetAttackPart`.
#[no_mangle]
pub extern "C" fn ee_units_uu_attack_part(unit: u64, target: u64) -> u64 {
    uu_spots::attack_part(&mut Game, unit, target)
}

// ---- round 2 (uu_world.rs) ----

/// `EEGetSeabedDepth` (`which` 0, 51) and `EEAdjustDiveDepth` (1, 52): the point's new height.
///
/// # Safety
/// `unit` is a live unit; `at` holds three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_seabed(which: i32, unit: u64, at: *const [f32; 3]) -> f32 {
    let n = Note::begin(if which == 0 { 51 } else { 52 });
    // SAFETY: the caller's contract
    let at = unsafe { *at };
    n.u(unit).p(at);
    let z = if which == 0 {
        uu_world::seabed_depth(&mut Game, unit, at)
    } else {
        uu_world::adjust_dive_depth(&mut Game, unit, at)
    };
    n.p([at[0], at[1], z]);
    z
}

/// `EEGetUnitTiles` (53) (appended to `io`).
///
/// # Safety
/// `unit` is a live unit; `io` a live tile vector.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_unit_tiles_margin(unit: u64, margin: f32, io: *mut c_void) {
    let n = Note::begin(53);
    // SAFETY: the caller's contract
    unsafe {
        let mut tiles = tiles_of(io);
        n.u(unit).f(margin).t(&tiles);
        uu_world::unit_tiles(&mut Game, unit, &mut tiles, margin);
        set_tiles(io, &tiles);
        n.t(&tiles);
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

/// `EEGetUnitRadius` (`which` 0, 55: two complex units) and `EEGetAngleFromNorth` (1, 56: `from` and `to`).
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
    let n = Note::begin(if which == 0 { 55 } else { 56 });
    let r = if which == 0 {
        n.u(unit).u(other);
        uu_world::unit_radius(&mut Game, unit, other)
    } else {
        // SAFETY: the caller's contract
        unsafe {
            n.p(*from).p(*to);
            uu_world::angle_from_north(*from, *to)
        }
    };
    n.f(r);
    r
}

/// `EEGetHeadingBetween` (57): the ground heading from `from` to `to` (`UMArcTan2(-dx, dy)`).
///
/// # Safety
/// The points hold three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_heading_between(from: *const [f32; 3], to: *const [f32; 3]) -> f32 {
    let n = Note::begin(57);
    // SAFETY: the caller's contract
    let (from, to) = unsafe { (*from, *to) };
    n.p(from).p(to);
    let r = uu_world::heading_between(from, to);
    n.f(r);
    r
}

/// `EEIsWithinReach` (`at` null, 60) and `EEIsWithinReachOf` (61).
///
/// # Safety
/// The units are live; `at` is null or holds three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_within_reach(unit: u64, target: u64, at: *const [f32; 3]) -> u8 {
    // SAFETY: the caller's contract
    let at = unsafe { at.as_ref().copied() };
    let n = Note::begin(if at.is_none() { 60 } else { 61 });
    if let Some(p) = at {
        n.p(p);
    }
    n.u(unit).u(target);
    let r = uu_world::within_reach(&mut Game, unit, target, at);
    n.b(r);
    u8::from(r)
}

/// `EEIsFootprintVisibleTo` (62).
#[no_mangle]
pub extern "C" fn ee_units_uu_footprint_visible(unit: u64, player: u64) -> u8 {
    let n = Note::begin(62);
    n.u(unit).pl(player);
    let r = uu_world::footprint_visible(&mut Game, unit, player);
    n.b(r);
    u8::from(r)
}

/// `EEGetEscapePoint` (18) (`io` left alone without attackers).
///
/// # Safety
/// `unit` is a live complex unit; `io` holds two floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_escape_point(unit: u64, io: *mut [f32; 2]) -> u8 {
    let n = Note::begin(18);
    n.u(unit);
    // SAFETY: the caller's contract
    let io = unsafe { &mut *io };
    let r = uu_world::escape_point(&mut Game, unit, io);
    n.b(r).f(io[0]).f(io[1]);
    u8::from(r)
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

/// `EEAddToFootprintTiles` (`which` 0, 66), `EERemoveFromFootprintTiles` (1, 67), `EEAddBuildingRegion` (2, 69),
/// `EERemoveBuildingRegion` (3, 70).
#[no_mangle]
pub extern "C" fn ee_units_uu_footprint(which: i32, unit: u64) {
    let n = Note::begin([66, 67, 69, 70][which.clamp(0, 3) as usize]);
    n.u(unit);
    let foot = if which != 3 && n.on() { Some(foot_of(unit)) } else { None };
    match which {
        0 | 1 => uu_world::footprint_link(&mut Game, unit, which == 0),
        _ => uu_world::building_region(&mut Game, unit, which == 2),
    }
    if let Some((x, y, size)) = foot {
        n.foot(x, y, size);
    }
}

/// `EEStepAsideFromSite` (71).
#[no_mangle]
pub extern "C" fn ee_units_uu_step_aside(unit: u64, class: u64, player: u64, x: i32, y: i32, force: u8) {
    let n = Note::begin(71);
    n.u(unit).c(class).pl(player).i(x.into()).i(y.into()).i(force.into());
    uu_world::step_aside_from_site(&mut Game, unit, class, player, [x, y], force != 0);
}

/// `EEBuildWall` (73).
///
/// # Safety
/// `group` is a live active group; `pieces` a live unit vector.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_build_wall(group: u64, pieces: *const c_void) {
    let n = Note::begin(73);
    // SAFETY: the caller's contract
    let pieces = unsafe { units_of(pieces) };
    if n.on() {
        n.i(Game.group_len(group).into()).us(&pieces);
    }
    uu_world::build_wall(&mut Game, group, &pieces);
}

/// `EEPlanWall` (74) (the new pieces appended to `out`).
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
    let n = Note::begin(74);
    // SAFETY: the caller's contract
    unsafe {
        let mut pieces = units_of(out);
        n.f(start_x).f(start_y).f(end_x).f(end_y).c(class).pl(player).i(foundation.into());
        n.i(skip_player_checks.into()).us(&pieces);
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
        n.us(&pieces);
    }
}

/// `EEShapeWall` (76) (its raised corners unused but noted).
#[no_mangle]
pub extern "C" fn ee_units_uu_shape_wall(complex: u64, corner: i32, raised_corners: i32) {
    let n = Note::begin(76);
    n.u(complex).i(corner.into()).i(raised_corners.into());
    uu_world::shape_wall(&mut Game, complex, corner);
    if n.on() {
        n.m(&with_unit(complex, |u| u.transform()));
    }
}

/// `EEIsTileTopAbove` (77) (`wall`: the wall found, 0 for none).
///
/// # Safety
/// `wall` points to a u64.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_tile_top_above(x: i32, y: i32, height: f32, bare: u8, wall: *mut u64) -> u8 {
    let n = Note::begin(77);
    n.i(x.into()).i(y.into()).f(height).i(bare.into());
    let (above, found) = uu_world::tile_top_above(&mut Game, x, y, height, bare != 0);
    // SAFETY: the caller's contract
    unsafe { *wall = found };
    n.u(found).b(above);
    u8::from(above)
}

/// `EEIsLineBlocked` (78) (`wall` written only when a tile was asked, `last`: the last free tile).
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
    let n = Note::begin(78);
    n.i(x1.into()).i(y1.into()).i(x2.into()).i(y2.into());
    let (blocked, found, last_x, last_y) = uu_world::line_blocked(&mut Game, x1, y1, x2, y2);
    // SAFETY: the caller's contract
    unsafe {
        *written = u8::from(found.is_some());
        *wall = found.unwrap_or(0);
        *last = [last_x, last_y];
    }
    n.b(blocked).i(last_x.into()).i(last_y.into());
    u8::from(blocked)
}

/// `EEGetPathLengthTo` (80) (`tile` in and out).
///
/// # Safety
/// `unit` is a live complex unit; `tile` holds two ints.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_path_length_to(unit: u64, tile: *mut [i32; 2], limit: u32, walls: u8) -> f32 {
    let n = Note::begin(80);
    // SAFETY: the caller's contract
    let tile = unsafe { &mut *tile };
    n.u(unit).i(tile[0].into()).i(tile[1].into()).i(limit.into()).i(walls.into());
    let r = uu_world::path_length_to(&mut Game, unit, tile, limit, walls != 0);
    n.f(r).i(tile[0].into()).i(tile[1].into());
    r
}

/// `EEComputeSoundParameters` (`out`: volume, pan, pitch) over the camera's view the caller gathered (kind 82:
/// counted in the depth, never recorded).
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
    let _n = Note::begin(82);
    // SAFETY: the caller's contract
    unsafe { *out = uu_world::sound_parameters(&*view, volume, *location) }
}

/// `EEGetDopplerPitch` (83)'s factor (`pitch` written unless silent or still).
///
/// # Safety
/// `location` holds a matrix, `camera` three floats, `pitch` one.
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_doppler_pitch(
    unit: u64,
    volume: f32,
    velocity: f32,
    location: *const [[f32; 4]; 4],
    camera: *const [f32; 3],
    pitch: *mut f32,
) {
    let n = Note::begin(83);
    // SAFETY: the caller's contract
    unsafe {
        n.u(unit).f(volume).f(*pitch);
        if let Some(p) = uu_world::doppler_pitch(volume, velocity, &GeTransformation { m: *location }, *camera) {
            *pitch = p;
        }
        n.f(*pitch);
    }
}

/// `EEEditorCanPlace` (72).
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
    let n = Note::begin(72);
    // SAFETY: the caller's contract
    let at = unsafe { *at };
    n.pl(player).i(unit_type.into()).p(at).i(x.into()).i(y.into());
    let r = uu_world::editor_can_place(&mut Game, player, unit_type, at, x, y);
    n.b(r);
    u8::from(r)
}

/// `EEDepositLoad` (84).
#[no_mangle]
pub extern "C" fn ee_units_uu_deposit_load(unit: u64, container: u64) {
    let n = Note::begin(84);
    if n.on() {
        let (amount, kind) = with_unit(unit, |u| (u.resource_amount(), u.resource_type() as i64));
        n.u(unit).u(container).i(amount).i(kind);
    }
    uu_world::deposit_load(&mut Game, unit, container);
}
