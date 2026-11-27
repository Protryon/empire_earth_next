//! The game map's remaining bodies in Rust (game core lane, round 11; was EEMap.cpp): the two constructors' bodies,
//! the terrain edits with their MAPC scopes (TEST ONLY), the sights with their TSGT scopes and MAPS records (TEST
//! ONLY), the cliff terrain's switch, the picks through a screen point, Load and Save over ee_units map_save.rs and
//! the save's C++ operations (`EEMapSaveOperation`, which RTSMap's Load and Save use too). Each C++ call or member is
//! a case of EEMap.cpp's `EECore11OpA3` ([`super::EeGameCpp`]'s round 11 lane A section); the methods forward here.
//!
//! Simulation code: the map's edits, sights and saves run in EE-SIM (the oracle, the captures' MAPC, MAPS, TSGT); the
//! picks only in a game with a window.

#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use ee_base::math::types::{Ge2DPoint, Ge3DLine, Ge3DPoint};
use ee_sim::units::map_save::{
    ee_units_map_load, ee_units_map_save, EE_UNITS_MAP_SAVE_CLIFF_VALID, EE_UNITS_MAP_SAVE_CREATE,
    EE_UNITS_MAP_SAVE_FINISH, EE_UNITS_MAP_SAVE_FIRST_CLIFF, EE_UNITS_MAP_SAVE_LOAD_TILE, EE_UNITS_MAP_SAVE_MESH_LOAD,
    EE_UNITS_MAP_SAVE_MESH_SAVE, EE_UNITS_MAP_SAVE_PICK_DEFAULT, EE_UNITS_MAP_SAVE_SET_DEFAULT,
    EE_UNITS_MAP_SAVE_SET_TYPE, EE_UNITS_MAP_SAVE_SPACE, EE_UNITS_MAP_SAVE_TERRAIN_LOAD,
    EE_UNITS_MAP_SAVE_TERRAIN_TYPE, EE_UNITS_MAP_SAVE_TILE_SAVE, EE_UNITS_MAP_SAVE_VALID,
};
use ee_sim::units::player_save::EeUnitsPlayerSaveIo;
use ee_sim::world::map_fields::{EeUnitsMapFields, EeUnitsRtsMapFields};

use crate::sight_capture::{ee_app_sight_enter, ee_app_sight_leave, EE_APP_SIGHT_CIRCLE};

use super::map::{
    ee_gamecpp_map_create_map, ee_gamecpp_map_edit, ee_gamecpp_map_initialize, ee_gamecpp_map_pick_default,
    ee_gamecpp_map_save_finish, ee_gamecpp_map_save_set_default, ee_gamecpp_map_save_terrain_type,
    ee_gamecpp_map_save_tile, ee_gamecpp_map_screen_line, ee_gamecpp_map_set_terrain_type, ee_gamecpp_map_sight,
    ee_gamecpp_map_sight_record,
};
use super::{cptr, h, hf, hp, pa, ptr, O, P};

/// ee/EEAppCapture.h's MAPC kinds (static_assert'd in EEMap.cpp).
const MX_MAPC_UPDATE_CLIFFS: u32 = 1;
const MX_MAPC_REMOVE_FLAT_CLIFFS: u32 = 2;
const MX_MAPC_PAINT_TERRAIN: u32 = 3;
const MX_MAPC_PAINT_TERRAIN_RECT: u32 = 4;
const MX_MAPC_UPDATE_WATER: u32 = 6;
const MX_MAPC_CREATE_MAP: u32 = 7;
const MX_MAPC_SET_CLIFF_TEXTURE: u32 = 8;
/// ee/EESightCapture.h's map kinds (static_assert'd in EEMap.cpp).
const MX_SIGHT_MAP_ADD: u32 = 8;
const MX_SIGHT_MAP_REMOVE: u32 = 9;
/// ee_app_map_edit's entries.
const MX_EDIT_WATER: u32 = 0;
const MX_EDIT_CLIFFS: u32 = 2;
const MX_EDIT_FLAT_CLIFFS: u32 = 3;
const MX_EDIT_PAINT: u32 = 4;
const MX_EDIT_PAINT_RECT: u32 = 5;

fn rts<'a>(m: P) -> &'a mut EeUnitsRtsMapFields {
    // SAFETY: a live map's record
    unsafe { &mut *super::map::map_rts(m) }
}

fn fields<'a>(m: P) -> &'a mut EeUnitsMapFields {
    // SAFETY: a live map's record
    unsafe { &mut *super::map::map_fields(m) }
}

// ---- the MAPC scope (ee/EEAppCapture.h EEAppMapScope: Enter at its start, Leave at its end)

/// An edit between its scope's hooks: the kind, its six arguments and the tile rectangle the hash reads.
fn scoped(m: P, kind: u32, args: [c_long; 6], rect: [c_long; 4], edit: impl FnOnce()) {
    let outer = h(O::MxScopeEnter, 0, &[]) != 0;
    edit();
    h(O::MxScopeLeave, m, &[outer as i64, kind as i64, cptr(&args), cptr(&rect)]);
}

fn edit(m: P, what: u32, a: [c_long; 6]) {
    ee_gamecpp_map_edit(m as *mut c_void, what, a[0], a[1], a[2], a[3], a[4], a[5]);
}

/// EEMap::UpdateWater: the four tiles around a vertex re-typed, the vertex's colour set.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mx_update_water(map: *mut c_void, x: c_long, y: c_long) {
    let m = map as P;
    let a = [x, y, 0, 0, 0, 0];
    scoped(m, MX_MAPC_UPDATE_WATER, a, [x - 1, x, y - 1, y], || edit(m, MX_EDIT_WATER, a));
}

/// EEMap::CreateMap: a new map of the size on the base terrain.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mx_create_map(map: *mut c_void, width: c_long, height: c_long, players: c_ulong) {
    let m = map as P;
    scoped(m, MX_MAPC_CREATE_MAP, [width, height, players as c_long, 0, 0, 0], [1, 0, 0, 0], || {
        ee_gamecpp_map_create_map(map, width, height, players)
    });
}

/// EEMap::PaintTerrainRect: a rectangle painted, its vertices recoloured.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mx_paint_rect(
    map: *mut c_void,
    left: c_long,
    right: c_long,
    top: c_long,
    bottom: c_long,
    terrain: c_long,
    cliff: c_long,
) {
    let m = map as P;
    let a = [left, right, top, bottom, terrain, cliff];
    scoped(m, MX_MAPC_PAINT_TERRAIN_RECT, a, [left, right, bottom, top], || edit(m, MX_EDIT_PAINT_RECT, a));
}

/// EEMap::PaintTerrain: a tile's terrain and the textures of it and its lower-left neighbours.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mx_paint(map: *mut c_void, x: c_long, y: c_long, terrain: c_long) {
    let m = map as P;
    let a = [x, y, terrain, 0, 0, 0];
    scoped(m, MX_MAPC_PAINT_TERRAIN, a, [x - 1, x, y - 1, y], || edit(m, MX_EDIT_PAINT, a));
}

/// EEMap::UpdateCliffs: the steep tiles of a rectangle made cliffs.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mx_update_cliffs(
    map: *mut c_void,
    left: c_long,
    right: c_long,
    bottom: c_long,
    top: c_long,
) {
    let m = map as P;
    let a = [left, right, bottom, top, 0, 0];
    scoped(m, MX_MAPC_UPDATE_CLIFFS, a, [left - 1, right, bottom - 1, top], || edit(m, MX_EDIT_CLIFFS, a));
}

/// EEMap::RemoveFlatCliffs: the cliffs of a rectangle no longer steep back on the base terrain.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mx_remove_flat_cliffs(
    map: *mut c_void,
    left: c_ulong,
    right: c_ulong,
    bottom: c_ulong,
    top: c_ulong,
) {
    let m = map as P;
    let (l, r, b, t) = (left as c_long, right as c_long, bottom as c_long, top as c_long);
    let a = [l, r, b, t, 0, 0];
    scoped(m, MX_MAPC_REMOVE_FLAT_CLIFFS, a, [l - 1, r, b - 1, t], || edit(m, MX_EDIT_FLAT_CLIFFS, a));
}

/// Whether a cliff terrain index names a cliff table (`GetNumCliffTerrains`, `sCliffTable`).
fn cliff_valid(index: c_ulong) -> bool {
    index < h(O::MxCliffCount, 0, &[]) as c_ulong && h(O::MxCliffExists, 0, &[index as i64]) != 0
}

/// A cliff terrain to switch to: a bad one (a random map script without CliffTexture passes -1) is the first.
fn cliff_or_first(cliff: c_long, valid: impl FnOnce(c_ulong) -> bool, first: impl FnOnce() -> c_long) -> c_long {
    if cliff < 0 || !valid(cliff as c_ulong) {
        first()
    } else {
        cliff
    }
}

/// EEMap::SetCliffTexture: another cliff terrain, the cliffs re-detected over the whole map. Fixed: the original
/// indexed the cliff table at [-1] for a script without CliffTexture; as Load does with a bad saved index, it takes
/// the first cliff terrain.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mx_set_cliff_texture(map: *mut c_void, cliff: c_long) {
    let m = map as P;
    let cliff = cliff_or_first(cliff, cliff_valid, || h(O::MxFirstCliff, 0, &[]) as c_long);
    scoped(m, MX_MAPC_SET_CLIFF_TEXTURE, [cliff, 0, 0, 0, 0, 0], [1, 0, 0, 0], || {
        fields(m).mU19565C = cliff as c_ulong;
        let size = rts(m).mSize;
        ee_gamecpp_mx_update_cliffs(map, 0, size, 0, size);
    });
}

/// EEMap::EEMap(): an empty map on the default terrain.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mx_construct_empty(map: *mut c_void) {
    fields(map as P).mU195650 = h(O::MpDefaultTerrain, 0, &[]) as c_long as c_ulong;
    ee_gamecpp_map_initialize(map);
}

/// EEMap::EEMap(width, height, players, terrain): a new map filled with the terrain, on the first cliff terrain.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mx_construct(
    map: *mut c_void,
    width: c_long,
    height: c_long,
    players: c_ulong,
    terrain: c_long,
) {
    let m = map as P;
    ee_gamecpp_map_initialize(map);
    fields(m).mU195650 = terrain as c_ulong;
    ee_gamecpp_mx_create_map(map, width, height, players);
    fields(m).mU19565C = h(O::MxFirstCliff, 0, &[]) as c_long as c_ulong;
}

// ---- the sights (TSGT's EESightScope around the call, MAPS after it)

/// EEMap::AddSight (`add`) / RemoveSight: a player's own or shared sight of the tiles within a radius.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mx_sight(
    map: *mut c_void,
    player: *mut c_void,
    x: f32,
    y: f32,
    radius: f32,
    shared: bool,
    add: bool,
) {
    let outer = ee_app_sight_enter(); // TSGT (TEST ONLY)
    ee_gamecpp_map_sight(map, player, x, y, radius, shared, add);
    hf(O::MxCaptureSight, player as P, &[(!add) as i64, shared as i64], &[x, y, radius]); // MAPS (TEST ONLY)
    if ee_app_sight_leave(outer) {
        let kind = if add { MX_SIGHT_MAP_ADD } else { MX_SIGHT_MAP_REMOVE };
        let args = [x, y, 0.0, 0.0, radius];
        ee_gamecpp_map_sight_record(
            EE_APP_SIGHT_CIRCLE,
            kind,
            player as u64,
            0,
            -1,
            -1,
            shared as c_ulong,
            args.as_ptr(),
        );
    }
}

// ---- the picks through a screen point

fn screen_line(at: *const Ge2DPoint) -> Ge3DLine {
    let mut line = Ge3DLine::default();
    ee_gamecpp_map_screen_line(at, &mut line);
    line
}

/// EEMap::PickScreenTile: the tile the ray through a screen point hits (optionally at the water surface).
#[no_mangle]
pub extern "C" fn ee_gamecpp_mx_pick_tile(
    map: *mut c_void,
    at: *const Ge2DPoint,
    x: *mut c_long,
    y: *mut c_long,
    water: bool,
) -> *mut c_void {
    let line = screen_line(at);
    hp(O::MxTileOnLine, map as P, &[cptr(&line), x as usize as i64, y as usize as i64, water as i64]) as *mut c_void
}

/// EEMap::GetLocationAt: the tile and the point the ray through a screen point hits.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mx_location_at(
    map: *mut c_void,
    at: *const Ge2DPoint,
    out: *mut Ge3DPoint,
    water: bool,
) -> *mut c_void {
    let line = screen_line(at);
    hp(O::MxLocationOnLine, map as P, &[cptr(&line), out as usize as i64, water as i64]) as *mut c_void
}

// ---- Load and Save (ee_units map_save.rs over the records and the operations below)

/// ee/EEMap.h's `EEMapSaveContext` (the map is its `RTSMap*`; the default and the last terrain tables).
#[repr(C)]
pub struct EeGamecppMxSaveContext {
    map: P,
    file: P,
    major: c_ulong,
    minor: c_ulong,
    default: P,
    terrain: P,
}

fn context(map: P, file: P, major: c_ulong, minor: c_ulong) -> EeGamecppMxSaveContext {
    EeGamecppMxSaveContext { map: hp(O::MxRtsOf, map, &[]), file, major, minor, default: 0, terrain: 0 }
}

/// EEMap::Load: the map, its base terrain, characteristics and cliff terrain.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mx_load(map: *mut c_void, file: *mut c_void, major: c_ulong, minor: c_ulong) -> c_long {
    let m = map as P;
    let mut c = context(m, file as P, major, minor);
    // SAFETY: the context and the map's records for the call
    unsafe {
        ee_units_map_load(
            ptr(&mut c) as usize as *mut c_void,
            ee_gamecpp_mx_save_op,
            rts(m),
            fields(m),
            major as u32,
            minor as u32,
        ) as c_long
    }
}

/// EEMap::Save: the map, its base terrain, characteristics and cliff terrain; the first error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_mx_save(map: *mut c_void, file: *mut c_void, major: c_ulong, minor: c_ulong) -> c_long {
    let m = map as P;
    let mut c = context(m, file as P, major, minor);
    // SAFETY: as ee_gamecpp_mx_load
    unsafe {
        ee_units_map_save(ptr(&mut c) as usize as *mut c_void, ee_gamecpp_mx_save_op, rts(m), fields(m)) as c_long
    }
}

/// `EEMapSaveOperation`: the C++ parts of the maps' Load and Save (the file's primitives, the tiles, the mesh and the
/// terrain tables); a tile's y in `index`, its x in `io.i`'s low 32 bits.
///
/// # Safety
/// `ctx` is a live `EEMapSaveContext`, `io` the call's.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_mx_save_op(
    ctx: *mut c_void,
    op: i32,
    index: u32,
    io: *mut EeUnitsPlayerSaveIo,
) -> i32 {
    // SAFETY: the function's contract
    let (c, io) = unsafe { (&mut *(ctx as *mut EeGamecppMxSaveContext), &mut *io) };
    let map = c.map;
    let ee = hp(O::MxEEMapOf, map, &[]);
    let e = ee as *mut c_void;
    let file = c.file as *mut c_void;
    let x = io.i as u32 as c_long;
    let y = index as c_long;
    match op {
        EE_UNITS_MAP_SAVE_CREATE => {
            let r = rts(ee);
            let (w, hgt, n) = (r.mL195614, r.mL195610, r.mU19561C);
            h(O::MxCreate, map, &[w as i64, hgt as i64, n as i64]);
            0
        }
        EE_UNITS_MAP_SAVE_LOAD_TILE => {
            let tile = if io.b != 0 {
                hp(O::MxLoadTile, map, &[c.major as i64, c.minor as i64, pa(c.file), x as i64, y as i64])
            } else {
                0
            };
            let at = (rts(ee).mSize as i64).wrapping_mul(y as i64).wrapping_add(x as i64);
            h(O::MxSetRtsTile, map, &[at, pa(tile)]);
            0
        }
        EE_UNITS_MAP_SAVE_MESH_LOAD => h(O::MxMeshLoad, map, &[c.major as i64, c.minor as i64, pa(c.file)]) as i32,
        EE_UNITS_MAP_SAVE_MESH_SAVE => h(O::MxMeshSave, map, &[pa(c.file)]) as i32,
        EE_UNITS_MAP_SAVE_TILE_SAVE => ee_gamecpp_map_save_tile(e, file, x, y),
        EE_UNITS_MAP_SAVE_PICK_DEFAULT => ee_gamecpp_map_pick_default(e, &mut c.default, io.b),
        EE_UNITS_MAP_SAVE_VALID => {
            io.i = (h(O::MpValidTile, ee, &[x as i64, y as i64]) != 0) as i64;
            0
        }
        EE_UNITS_MAP_SAVE_TERRAIN_TYPE => ee_gamecpp_map_save_terrain_type(e, &mut c.terrain, x, y, io),
        EE_UNITS_MAP_SAVE_SPACE => {
            io.i = (h(O::MxSpaceTile, ee, &[x as i64, y as i64]) != 0) as i64;
            0
        }
        EE_UNITS_MAP_SAVE_SET_TYPE => ee_gamecpp_map_set_terrain_type(e, x, y, (io.i >> 32) as c_long),
        EE_UNITS_MAP_SAVE_TERRAIN_LOAD => {
            h(O::MxTerrainLoadInto, c.terrain, &[pa(ee)]);
            0
        }
        EE_UNITS_MAP_SAVE_SET_DEFAULT => ee_gamecpp_map_save_set_default(e, c.default, x, y),
        EE_UNITS_MAP_SAVE_CLIFF_VALID => {
            io.b = cliff_valid(io.i as c_ulong) as u8;
            0
        }
        EE_UNITS_MAP_SAVE_FIRST_CLIFF => {
            io.i = h(O::MxFirstCliff, 0, &[]);
            0
        }
        EE_UNITS_MAP_SAVE_FINISH => ee_gamecpp_map_save_finish(e),
        _ => h(O::MxPrimitive, c.file, &[op as i64, io as *mut EeUnitsPlayerSaveIo as usize as i64]) as i32,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_bad_cliff_terrain_is_the_first() {
        assert_eq!(cliff_or_first(-1, |_| panic!("not asked"), || 3), 3);
        assert_eq!(cliff_or_first(5, |_| false, || 3), 3);
        assert_eq!(cliff_or_first(5, |i| i == 5, || 3), 5);
    }

    #[test]
    fn the_save_context_is_the_cpp_one() {
        use std::mem::{offset_of, size_of};
        assert_eq!(offset_of!(EeGamecppMxSaveContext, major), 2 * size_of::<usize>());
        assert_eq!(offset_of!(EeGamecppMxSaveContext, default), 2 * size_of::<usize>() + 2 * size_of::<c_ulong>());
        assert_eq!(size_of::<EeGamecppMxSaveContext>(), 4 * size_of::<usize>() + 2 * size_of::<c_ulong>());
    }
}
