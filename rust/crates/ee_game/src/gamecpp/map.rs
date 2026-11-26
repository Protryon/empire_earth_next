//! The game map's own bodies in Rust (game core lane, round 9; was EEMap.cpp): the destructor's frees and terrain
//! unloads, the tile array and its visibility words, the region tables, the screen line and the tile under a screen
//! point, the ground's pitch and roll, the terrain sets' registration, the tile terrain, the saved tiles, the
//! out-of-sync dump, the save's and the map host's compound operations, the sight capture's tile index. Each C++
//! call or member is a case of EEEffectHost.cpp's `EECore9OpB` ([`super::EeGameCpp`]'s lane B section); the C++
//! methods forward here (`ee_gamecpp_map_*`).
//!
//! Simulation code (RUST_PLAN 2.1): the pitch and roll are the C++'s float operations in its order (the inline
//! `GETransformation::Apply`, `UMArcCos`).

#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use ee_base::math::ffi::ee_ge_um_arccos;
use ee_base::math::types::{Ge2DPoint, Ge3DLine, Ge3DPoint, GeTransformation, GeVector};
use ee_base::math::um;
use ee_render::ge::tmobj::{ee_ge_tm_is_valid, ee_ge_tm_mesh_z, EeGeTerrainMesh};
use ee_render::gfx::effect_obj::ee_gfx_effect_regions_new;
use ee_sim::app::map::{tile_count, EeAppMapDims};
use ee_sim::combat::regions::ee_combat_regions_new;
use ee_sim::units::player_save::EeUnitsPlayerSaveIo;
use ee_sim::units::tile_sight_host::{ee_units_map_words_free, ee_units_map_words_new, ee_units_map_words_resize};
use ee_sim::units::uu_ffi::ee_units_uu_get_tiles_near_point;
use ee_sim::world::map_fields::{EeUnitsMapFields, EeUnitsRtsMapFields};
use ee_sim::world::view::{MapView, PlayerView, UnitView, WorldRead, WorldView};

use crate::app::map_ffi::{
    ee_app_map_checksum, ee_app_map_edit, ee_app_map_sight, EeAppMapHost, EeAppMapIo, EE_APP_MAP_OP_CLIFF,
    EE_APP_MAP_OP_COLOR_SET, EE_APP_MAP_OP_COUNT_CELL, EE_APP_MAP_OP_LOCAL_SEES, EE_APP_MAP_OP_SET_TERRAIN_TYPE,
    EE_APP_MAP_OP_SET_TILE_TERRAIN, EE_APP_MAP_OP_SIGHT_AT, EE_APP_MAP_OP_SIGHT_ENABLED, EE_APP_MAP_OP_TERRAIN,
    EE_APP_MAP_OP_TERRAIN_TYPE, EE_APP_MAP_OP_TILE, EE_APP_MAP_OP_TILES_NEAR, EE_APP_MAP_OP_TILE_CHECKSUM,
};
use crate::app::world_capture::{ee_app_capture_map_checksum, ee_app_capture_slope};
use crate::sight_capture::ee_app_sight_record;

use super::{cptr, h, hd, hf, hp, pa, position, ptr, Owned, O, P};

/// `kUnitTypeCameraObject` (static_assert'd in EEEffectHost.cpp).
const MP_TYPE_CAMERA_OBJECT: c_ulong = 0x167;
/// `kSlopeScale` (0x441cf8: GetTerrainPitch / GetTerrainRoll).
const MP_SLOPE_SCALE: f32 = -0.769_230_8;
/// The pitch's and roll's quarter turn.
const MP_QUARTER_TURN: f32 = 1.570_796_4;

fn rts<'a>(m: P) -> &'a mut EeUnitsRtsMapFields {
    // SAFETY: a live map's record
    unsafe { &mut *super::map::map_rts(m) }
}

fn fields<'a>(m: P) -> &'a mut EeUnitsMapFields {
    // SAFETY: a live map's record
    unsafe { &mut *super::map::map_fields(m) }
}

/// `gWorld.mMap` (0 before the world has one), from the world views.
pub(crate) fn world_map() -> P {
    // SAFETY: a read of the world's member (the C++'s read of it, from whichever thread read it)
    let t = unsafe { WorldRead::begin() };
    WorldView::global(&t).and_then(|w| w.map()).map_or(0, |m| m.handle() as P)
}

/// `gWorld.mNumPlayers`.
pub(crate) fn num_players() -> i64 {
    // SAFETY: as above
    let t = unsafe { WorldRead::begin() };
    WorldView::global(&t).map_or(0, |w| w.num_players() as i64)
}

/// `gWorld.mPlayers[gWorld.mCurrentPlayer]` (0: none).
pub(crate) fn local_player() -> P {
    // SAFETY: as above
    let t = unsafe { WorldRead::begin() };
    WorldView::global(&t).and_then(|w| w.current_player()).map_or(0, |p| p.handle() as P)
}

/// The player `player`'s `mSharedLOS`.
pub(crate) fn shared_los(player: P) -> bool {
    // SAFETY: a live player
    let t = unsafe { WorldRead::begin() };
    unsafe { PlayerView::of(&t, player as u64) }.is_some_and(|p| p.shared_los())
}

/// The map `m`'s records (`static_cast<EeUnitsRtsMapFields*>`, `<EeUnitsMapFields*>`).
pub(crate) fn map_rts(m: P) -> *mut EeUnitsRtsMapFields {
    // SAFETY: a live map
    let t = unsafe { WorldRead::begin() };
    unsafe { MapView::of(&t, m as u64) }.map_or(std::ptr::null_mut(), |v| v.rts().ptr() as *mut EeUnitsRtsMapFields)
}

pub(crate) fn map_fields(m: P) -> *mut EeUnitsMapFields {
    // SAFETY: as above
    let t = unsafe { WorldRead::begin() };
    unsafe { MapView::of(&t, m as u64) }.map_or(std::ptr::null_mut(), |v| v.fields().ptr() as *mut EeUnitsMapFields)
}

/// The map `m`'s `GetTile((unsigned long)x, (unsigned long)y)` (0: off the map).
pub(crate) fn map_tile(m: P, x: i64, y: i64) -> P {
    // SAFETY: as above
    let t = unsafe { WorldRead::begin() };
    unsafe { MapView::of(&t, m as u64) }
        .and_then(|v| v.tile(x as c_ulong as i64, y as c_ulong as i64))
        .map_or(0, |k| k.handle() as P)
}

/// The unit `unit`'s `GetUnitClass()` (0: none).
pub(crate) fn unit_class(unit: P) -> P {
    // SAFETY: a live unit
    let t = unsafe { WorldRead::begin() };
    unsafe { UnitView::of(&t, unit as u64) }.and_then(|u| u.class()).map_or(0, |k| k.handle() as P)
}

/// A tile's unit node's `mUnit` and `mNext` (an `RTSTileUnitNode`, at the layout's offsets).
pub(crate) fn node_unit(node: P) -> P {
    // SAFETY: a live node's pointer member
    unsafe { *((node + ee_sim::world::layout::layout().unit_node_unit as usize) as *const P) }
}

pub(crate) fn node_next(node: P) -> P {
    // SAFETY: as above
    unsafe { *((node + ee_sim::world::layout::layout().unit_node_next as usize) as *const P) }
}

/// `gWorld.GetUnit(id)` (0: none).
pub(crate) fn world_unit(id: i64) -> P {
    #[cfg(not(test))]
    {
        let world = ee_sim::world::layout::layout().world_address as usize as *mut c_void;
        // SAFETY: the registered world, live
        unsafe { crate::rts::world::ffi::ee_rts_world_unit(world, id as c_ulong) as P }
    }
    #[cfg(test)]
    {
        let _ = id;
        0
    }
}

/// The visibility words of a tile: two players a word.
fn words_per_tile(players: c_ulong) -> c_ulong {
    (players >> 1).wrapping_add(players & 1)
}

/// `UMCeilUnsigned`: rounded to nearest, then up when below the value.
fn ceil_unsigned(value: f32) -> c_ulong {
    let mut r = um::lrintf(value) as c_ulong;
    if (r as f32) < value {
        r = r.wrapping_add(1);
    }
    r
}

/// The inline `GETransformation::Apply(const GEVector&, GEVector&)`, in its order of terms.
fn apply_vector(t: &GeTransformation, v: GeVector) -> GeVector {
    let m = &t.m;
    GeVector {
        x: v.z * m[0][2] + v.y * m[0][1] + v.x * m[0][0],
        y: v.x * m[1][0] + v.z * m[1][2] + v.y * m[1][1],
        z: v.x * m[2][0] + v.z * m[2][2] + v.y * m[2][1],
    }
}

/// `EEMap::GetGroundPoint` over the map's mesh: x and y kept, z the mesh's height there on a valid tile (the tile
/// under the point's floor), the point's own z off the map.
pub(crate) fn mesh_ground_point(mesh: *const EeGeTerrainMesh, at: &Ge3DPoint) -> Ge3DPoint {
    let (x, y) = (um::floor_unsigned(at.x) as c_ulong, um::floor_unsigned(at.y) as c_ulong);
    // SAFETY: a live map's mesh
    let z = unsafe {
        if ee_ge_tm_is_valid(mesh, x, y, true) {
            ee_ge_tm_mesh_z(mesh, at.x, at.y)
        } else {
            at.z
        }
    };
    Ge3DPoint { x: at.x, y: at.y, z }
}

/// `EEMap::GetGroundPoint(point, out)` for the C++ (`mesh` the map's `mTerrainMesh.GetRust()`; `out` may be `point`).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_map_ground_point(
    mesh: *const c_void,
    point: *const Ge3DPoint,
    out: *mut Ge3DPoint,
) {
    // SAFETY: the caller's mesh and points
    unsafe { *out = mesh_ground_point(mesh.cast(), &*point) }
}

/// The map's ground point under `at`.
pub(crate) fn ground_point(m: P, at: &Ge3DPoint) -> Ge3DPoint {
    mesh_ground_point(super::rast::mesh_of(m), at)
}

/// The world's map's ground point under `at` (`gWorld.mMap->GetGroundPoint`).
pub(crate) fn world_ground_point(at: &Ge3DPoint) -> Ge3DPoint {
    ground_point(super::map::world_map(), at)
}

/// The unit and effect region tables made again for the map's size.
fn make_regions(m: P) {
    let size = rts(m).mSize;
    h(O::MpUnitRegionsFree, m, &[]);
    h(O::MpEffectRegionsFree, m, &[]);
    let units = ee_combat_regions_new(ceil_unsigned(size as f32 / 3.0) as u32, 3);
    h(O::MpSetUnitRegions, m, &[units as i64]);
    let effects = ee_gfx_effect_regions_new(ceil_unsigned(size as f32 / 10.0) as u32, 10);
    h(O::MpSetEffectRegions, m, &[effects as i64]);
}

/// The map's dimensions for ee_app map.rs.
fn dims(m: P) -> EeAppMapDims {
    let (r, f) = (rts(m), fields(m));
    EeAppMapDims {
        size: r.mSize,
        l195610: r.mL195610,
        l195614: r.mL195614,
        f1955f4: r.mF1955F4,
        base_terrain: f.mU195650 as c_long,
        cliff_terrain: f.mU19565C as c_long,
        players: r.mU19561C,
    }
}

/// EEMap::~EEMap: the region tables, tiles and visibility words freed, the terrain textures unloaded.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_destroy(map: *mut c_void) {
    let m = map as P;
    h(O::MpUnitRegionsFree, m, &[]);
    h(O::MpEffectRegionsFree, m, &[]);
    h(O::MpDeleteTiles, m, &[]);
    // SAFETY: the map's words (Rust's)
    unsafe { ee_units_map_words_free(hp(O::MpWords, m, &[]) as *mut u32) };
    let water = h(O::MpGlobalWaterSet, m, &[]);
    h(O::MpTerrainUnload, m, &[water]);
    let n = (rts(m).mSize.wrapping_add(1)) as c_ulong;
    for x in 0..n {
        for y in 0..n {
            if h(O::MpValidTile, m, &[x as i64, y as i64]) != 0
                && h(O::MpTerrainExists, 0, &[h(O::MpTextureSetId, m, &[x as i64, y as i64])]) != 0
            {
                h(O::MpTerrainUnload, m, &[h(O::MpTextureSetId, m, &[x as i64, y as i64])]);
            }
        }
    }
}

/// EEMap::CreateTile: the next reserved tile at the height, with its slice of the visibility words.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_create_tile(map: *mut c_void, height: f32) -> *mut c_void {
    let m = map as P;
    let players = rts(m).mU19561C;
    let next = fields(m).mNextTile;
    let tile = hp(O::MpTileAt, m, &[next as i64]);
    hf(O::MpTileSetHeight, tile, &[], &[height]);
    h(O::MpTileSetVisibility, m, &[pa(tile), words_per_tile(players).wrapping_mul(next) as i64]);
    fields(m).mNextTile = next.wrapping_add(1);
    tile as *mut c_void
}

/// EEMap::ReserveTiles: the tiles and cleared visibility words (two players a word), the old ones dropped.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_reserve_tiles(map: *mut c_void, count: c_ulong) {
    let m = map as P;
    let words = words_per_tile(rts(m).mU19561C);
    if hp(O::MpTileArray, m, &[]) != 0 {
        h(O::MpDeleteTiles, m, &[]);
        // SAFETY: the map's words (Rust's)
        unsafe { ee_units_map_words_free(hp(O::MpWords, m, &[]) as *mut u32) };
        h(O::MpSetTiles, m, &[0]);
        h(O::MpSetWords, m, &[0]);
    }
    h(O::MpNewTiles, m, &[count as i64]);
    let w = ee_units_map_words_new(words.wrapping_mul(count) as u64);
    h(O::MpSetWords, m, &[w as i64]);
    fields(m).mNextTile = 0;
    fields(m).mTileArrayCount = count;
}

/// EEMap::SetPlayerCount: every tile's visibility words resized, the visibility kept.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_set_player_count(map: *mut c_void, players: c_ulong) {
    let m = map as P;
    let old = words_per_tile(rts(m).mU19561C);
    let new = words_per_tile(players);
    let words = hp(O::MpWords, m, &[]) as *mut u32;
    let tiles = tile_count(&dims(m));
    // SAFETY: the map's own words
    let w = unsafe { ee_units_map_words_resize(m as u64, words, tiles as u64, old as u64, new as u64) };
    h(O::MpSetWords, m, &[w as i64]);
    rts(m).mU19561C = players;
}

/// EEMap::GetTileCount (ee_app map.rs).
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_tile_count(map: *mut c_void) -> c_ulong {
    tile_count(&dims(map as P))
}

/// EEMap::PlaceUnitOnGround: a placed unit dropped onto the ground at its position (camera objects excluded).
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_place_on_ground(unit: *mut c_void) {
    let cx = unit as P;
    if hp(O::UnTileNode, cx, &[]) == 0 {
        return;
    }
    // SAFETY: the class's record
    let kind = unsafe { (*crate::gamecpp::records::uc_fields(super::map::unit_class(cx))).mUnitType };
    if kind != MP_TYPE_CAMERA_OBJECT {
        let mut t = GeTransformation::IDENTITY;
        h(O::CxLocation, cx, &[ptr(&mut t)]);
        let mut at = position(&t);
        h(O::MpPlaceOnGround, cx, &[ptr(&mut at)]);
    }
}

/// EEMap::CreateMap's body (after its capture scope): the base map, fresh region tables, every tile on the base
/// terrain (ee_app map.rs).
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_create_map(map: *mut c_void, width: c_long, height: c_long, players: c_ulong) {
    let m = map as P;
    h(O::MpRtsCreate, m, &[width as i64, height as i64, players as i64]);
    make_regions(m);
    ee_gamecpp_map_edit(map, 1, 0, 0, 0, 0, 0, 0);
}

/// The 3D line through a screen point from the near to the far clipping plane (EEMap::GetScreenLine).
fn screen_line(at: Ge2DPoint) -> Ge3DLine {
    let scale = 1.0 / hd(O::MpViewHalfWidth, 0, &[], &[]);
    let xc = hd(O::MpViewXCenter, 0, &[], &[]);
    let yc = hd(O::MpViewYCenter, 0, &[], &[]);
    let point = Ge3DPoint::new((at.x - xc) * scale, -((at.y - yc) * scale), 0.0);
    let far_point = Ge3DPoint::new(point.x, point.y, 1.0);
    let mut near = Ge3DPoint::default();
    let mut far = Ge3DPoint::default();
    if h(O::MpOrthographic, 0, &[]) == 0 {
        let front = hd(O::MpViewFrontClip, 0, &[], &[]);
        hf(O::MpDenormalizePerspective, 0, &[cptr(&point), ptr(&mut near)], &[front]);
        let rear = hd(O::MpViewRearClip, 0, &[], &[]);
        hf(O::MpDenormalizePerspective, 0, &[cptr(&far_point), ptr(&mut far)], &[rear]);
    } else {
        h(O::MpDenormalizeOrthographic, 0, &[cptr(&point), ptr(&mut near)]);
        h(O::MpDenormalizeOrthographic, 0, &[cptr(&far_point), ptr(&mut far)]);
    }
    let mut line = Ge3DLine::default();
    h(O::MpLineDefine, 0, &[ptr(&mut line), cptr(&near), cptr(&far)]);
    line
}

/// EEMap::GetScreenLine.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_screen_line(at: *const Ge2DPoint, out: *mut Ge3DLine) {
    // SAFETY: the caller's point and line
    unsafe { *out = screen_line(*at) };
}

/// EEMap::GetTileAt: the tile under a screen point and its coordinates (in orthographic view, where the ray meets z 0).
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_tile_at(
    map: *mut c_void,
    at: *const Ge2DPoint,
    x: *mut c_long,
    y: *mut c_long,
) -> *mut c_void {
    let m = map as P;
    // SAFETY: the caller's point
    let line = screen_line(unsafe { *at });
    if h(O::MpOrthographic, 0, &[]) != 0 {
        let mut distance = 0f32;
        let mut point = Ge3DPoint::default();
        if hf(O::MpLineSolveForZ, 0, &[cptr(&line), ptr(&mut distance), ptr(&mut point)], &[0.0]) == 0 {
            point = line.start;
        }
        h(O::MpClosestTileToPoint, m, &[cptr(&point), x as usize as i64, y as usize as i64]);
    } else {
        h(O::MpClosestTileToLine, m, &[cptr(&line), x as usize as i64, y as usize as i64]);
    }
    // SAFETY: the caller's coordinates
    let (tx, ty) = unsafe { (*x, *y) };
    super::map::map_tile(m, tx as i64, ty as i64) as *mut c_void
}

/// EEMap::GetTileLocation: the ground point at a tile's centre; false without the tile.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_tile_location(map: *mut c_void, x: c_long, y: c_long, out: *mut Ge3DPoint) -> bool {
    let m = map as P;
    if super::map::map_tile(m, x as i64, y as i64) == 0 {
        return false;
    }
    let px = x as f32 + 0.5;
    let py = y as f32 + 0.5;
    let z = super::rast::mesh_z(super::rast::mesh_of(m), px, py);
    // SAFETY: the caller's point
    unsafe { *out = Ge3DPoint::new(px, py, z) };
    true
}

/// The ground's slope under a location along `offset` (0.65 ahead and behind, or to either side).
fn slope(m: P, t: &GeTransformation, offset: GeVector, back: GeVector, kind: u32) -> f32 {
    let mut front = position(t);
    let mut behind = front;
    let d = apply_vector(t, offset);
    front = Ge3DPoint::new(front.x + d.x, front.y + d.y, front.z + d.z);
    let front_ground = ground_point(m, &front);
    let d = apply_vector(t, back);
    behind = Ge3DPoint::new(behind.x + d.x, behind.y + d.y, behind.z + d.z);
    let back_ground = ground_point(m, &behind);
    let r = ee_ge_um_arccos((front_ground.z - back_ground.z) * MP_SLOPE_SCALE) - MP_QUARTER_TURN;
    // SAFETY: the location is the caller's
    unsafe { ee_app_capture_slope(kind, t, r) }; // MAPP (TEST ONLY)
    r
}

/// EEMap::GetTerrainPitch: from the ground heights 0.65 ahead of and behind the location.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_terrain_pitch(map: *mut c_void, t: *const GeTransformation) -> f32 {
    // SAFETY: the caller's transformation
    let t = unsafe { &*t };
    slope(map as P, t, GeVector::new(0.0, 0.65, 0.0), GeVector::new(0.0, -0.65, 0.0), 0)
}

/// EEMap::GetTerrainRoll: from the ground heights 0.65 to either side of the location.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_terrain_roll(map: *mut c_void, t: *const GeTransformation) -> f32 {
    // SAFETY: the caller's transformation
    let t = unsafe { &*t };
    slope(map as P, t, GeVector::new(0.65, 0.0, 0.0), GeVector::new(-0.65, 0.0, 0.0), 1)
}

/// EEMap::Initialize: empty (no tiles or region tables), its random generator seeded, the terrain sets registered.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_initialize(map: *mut c_void) {
    let m = map as P;
    fields(m).mU195650 = 0;
    h(O::MpSeedRandom, m, &[]);
    h(O::MpSetUnitRegions, m, &[0]);
    h(O::MpSetEffectRegions, m, &[0]);
    h(O::MpSetTiles, m, &[0]);
    h(O::MpSetWords, m, &[0]);
    let f = fields(m);
    f.mNextTile = 0;
    f.mTileArrayCount = 0;
    f.mChecksumCursor = 0;
    ee_gamecpp_map_register_terrain_sets(map);
    fields(m).mMapCharacteristics = 0;
    let water = h(O::MpGlobalWaterSet, m, &[]);
    h(O::MpTerrainLoad, m, &[water]);
}

/// EEMap::SetTileTerrain: a tile's texture set, the new terrain's textures loaded (the old one's unloaded first).
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_set_tile_terrain(map: *mut c_void, x: c_long, y: c_long, set: c_long, unload: bool) {
    let m = map as P;
    if unload {
        h(O::MpTerrainUnload, m, &[h(O::MpTextureSetId, m, &[x as i64, y as i64])]);
    }
    h(O::MpTerrainLoad, m, &[set as i64]);
    h(O::MpSetTextureSet, m, &[x as i64, y as i64, h(O::MpTerrainTextureSet, 0, &[set as i64])]);
}

/// EEMap::LoadTile: the next tile from the saved game (saves up to 0.46 also carry its terrain).
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_load_tile(
    map: *mut c_void,
    major: c_ulong,
    minor: c_ulong,
    file: *mut c_void,
    x: c_long,
    y: c_long,
) -> *mut c_void {
    let m = map as P;
    let next = fields(m).mNextTile;
    let players = rts(m).mU19561C;
    let tile = hp(O::MpTileAt, m, &[next as i64]);
    h(O::MpTileSetVisibility, m, &[pa(tile), words_per_tile(players).wrapping_mul(next) as i64]);
    fields(m).mNextTile = next.wrapping_add(1);
    if major == 0 && minor <= 0x2e {
        let mut terrain: c_ulong = 0;
        let mut model: c_ulong = 0;
        h(O::MpFileReadUlong, file as P, &[ptr(&mut terrain)]);
        h(O::MpFileReadUlong, file as P, &[ptr(&mut model)]);
        let terrain = terrain as c_long;
        if h(O::MpTerrainExists, 0, &[terrain as i64]) != 0 {
            let set = h(O::MpTerrainTextureSet, 0, &[terrain as i64]) as c_long;
            ee_gamecpp_map_set_tile_terrain(map, x, y, set, true);
        } else {
            ee_gamecpp_map_set_tile_terrain(map, x, y, fields(m).mU195650 as c_long, true);
        }
    }
    h(O::MpTileLoad, tile, &[major as i64, minor as i64, file as usize as i64, rts(m).mU19561C as i64]);
    tile as *mut c_void
}

/// EEMap::DumpOutOfSynch: every tile's state to the out-of-sync dump file.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_dump(map: *mut c_void, file: *mut c_void) {
    let m = map as P;
    h(O::MpDumpTitle, file as P, &[]);
    let size = rts(m).mSize;
    for y in 0..size {
        for x in 0..size {
            let tile = super::map::map_tile(m, x as i64, y as i64);
            if tile == 0 {
                continue;
            }
            h(O::MpDumpTileTitle, file as P, &[x as i64, y as i64]);
            super::tile::ee_gamecpp_tile_dump(
                tile as *const std::ffi::c_void,
                file as *mut std::ffi::c_void,
                rts(m).mU19561C as c_ulong,
            );
            h(O::MpDumpNewLine, file as P, &[]);
        }
    }
}

/// EEMap::RegisterTerrainSets: every terrain's texture set and every terrain type's colour set registered with the
/// terrain mesh, then the water state and the colour table.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_register_terrain_sets(map: *mut c_void) {
    let m = map as P;
    let colors = Owned::new(O::MpColorsNew, O::MpColorsDelete);
    let set = Owned::new(O::MpColorSetNew, O::MpColorSetDelete);
    let mut water_set: c_ulong = 0;
    let mut id: c_ulong = 0;
    let terrains = h(O::MpTerrainCount, 0, &[]) as c_ulong;
    for i in 0..terrains {
        let terrain = hp(O::MpTerrainGet, 0, &[i as i64]);
        if terrain != 0 {
            let textures = hp(O::MpNewTextureSet, terrain, &[]);
            h(O::MpRegisterTextureSet, m, &[pa(textures), ptr(&mut id)]);
            if h(O::MpTerrainIsWater, terrain, &[]) != 0 {
                water_set = id;
            }
            h(O::MpTerrainAttach, terrain, &[pa(textures)]);
            h(O::MpTerrainSetTextureSet, terrain, &[id as i64]);
        } else {
            h(O::MpRegisterTextureSet, m, &[0, ptr(&mut id)]);
        }
    }
    let types = h(O::MpTypeCount, 0, &[]) as c_ulong;
    for j in 0..types {
        let kind = hp(O::MpTypeGet, 0, &[j as i64]);
        if kind == 0 {
            continue;
        }
        h(O::MpColorSetClear, set.0, &[]);
        let (first, last) = (h(O::MpTypeFirstColor, kind, &[]) as c_ulong, h(O::MpTypeLastColor, kind, &[]) as c_long);
        let mut k = first;
        while (k as c_long) <= last {
            h(O::MpColorSetPush, set.0, &[k as i64]);
            k = k.wrapping_add(1);
        }
        h(O::MpRegisterColorSet, m, &[pa(set.0), ptr(&mut id)]);
        h(O::MpTypeSetColorSet, kind, &[id as i64]);
    }
    let count = h(O::MpColorCount, 0, &[]) as c_ulong;
    for c in 0..count {
        if h(O::MpColorExists, 0, &[c as i64]) != 0 {
            h(O::MpColorsPush, colors.0, &[c as i64]);
        }
    }
    let water = hp(O::MpTerrainGet, 0, &[water_set as i64]);
    let color_set = h(O::MpTypeColorSet, 0, &[h(O::MpTerrainColorType, water, &[])]);
    let period = h(O::MpTerrainWaterPeriod, water, &[]);
    let level = -(rts(m).mF1955F4 * hd(O::MpTerrainWaterDepth, water, &[], &[]));
    hf(O::MpSetWaterState, m, &[water_set as i64, color_set, period], &[level]);
    h(O::MpSetColorTable, m, &[pa(colors.0)]);
}

// ---- the save's compound operations (EEMapSaveOperation)

/// TILE_SAVE: the tile at (x, y), if any, saved.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_save_tile(map: *mut c_void, file: *mut c_void, x: c_long, y: c_long) -> i32 {
    let m = map as P;
    let size = rts(m).mSize;
    let tile = hp(O::MpRtsTile, m, &[size.wrapping_mul(y).wrapping_add(x) as i64]);
    if tile == 0 {
        return 0;
    }
    h(O::MpTileSave, tile, &[file as usize as i64, rts(m).mU19561C as i64]) as i32
}

/// PICK_DEFAULT: the load's default terrain (the map's base terrain when saved, made valid; else the default).
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_pick_default(map: *mut c_void, default: *mut usize, saved: u8) -> i32 {
    let m = map as P;
    let d = if saved != 0 {
        if h(O::MpTerrainExists, 0, &[fields(m).mU195650 as c_long as i64]) == 0 {
            fields(m).mU195650 = h(O::MpDefaultTerrain, 0, &[]) as c_long as c_ulong;
        }
        hp(O::MpTerrainGet, 0, &[fields(m).mU195650 as c_long as i64])
    } else {
        let t = h(O::MpDefaultTerrain, 0, &[]) as c_long;
        if t != -1 {
            hp(O::MpTerrainGet, 0, &[t as i64])
        } else {
            0
        }
    };
    // SAFETY: the save context's member
    unsafe { *default = d };
    0
}

/// TERRAIN_TYPE: the tile's terrain table (kept for TERRAIN_LOAD), whether there is one and its type.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_save_terrain_type(
    map: *mut c_void,
    terrain: *mut usize,
    x: c_long,
    y: c_long,
    io: *mut EeUnitsPlayerSaveIo,
) -> i32 {
    let m = map as P;
    let t = hp(O::MpTerrainGet, 0, &[h(O::MpTextureSetId, m, &[x as i64, y as i64])]);
    let kind = if t != 0 { h(O::MpTerrainTypeOf, t, &[]) } else { 0 };
    // SAFETY: the save context's member and the caller's io
    unsafe {
        *terrain = t;
        (*io).b = (t != 0) as u8;
        (*io).i = kind;
    }
    0
}

/// SET_TYPE (and the map host's SET_TERRAIN_TYPE): the tile's terrain type.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_set_terrain_type(map: *mut c_void, x: c_long, y: c_long, kind: c_long) -> i32 {
    h(O::MpTileSetTerrainType, map as P, &[x as i64, y as i64, kind as i64]);
    0
}

/// SET_DEFAULT: the tile on the default terrain (its texture set, then its type).
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_save_set_default(map: *mut c_void, default: usize, x: c_long, y: c_long) -> i32 {
    let set = h(O::MpTerrainTextureSetOf, default, &[]) as c_long;
    ee_gamecpp_map_set_tile_terrain(map, x, y, set, false);
    ee_gamecpp_map_set_terrain_type(map, x, y, h(O::MpTerrainTypeOf, default, &[]) as c_long)
}

/// FINISH: fresh region tables, the cliffs over the whole map.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_save_finish(map: *mut c_void) -> i32 {
    let m = map as P;
    make_regions(m);
    let size = rts(m).mSize as i64;
    super::map_more::ee_gamecpp_mx_update_cliffs(m as *mut c_void, 0, size as c_long, 0, size as c_long);
    0
}

// ---- the map host's compound operations (EEMapHostOp)

/// TERRAIN: a terrain's texture set, type and colour type.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_host_terrain(io: *mut EeAppMapIo) {
    // SAFETY: the host's io
    let io = unsafe { &mut *io };
    let t = hp(O::MpTerrainGet, 0, &[io.a as i64]);
    io.a = h(O::MpTerrainTextureSetOf, t, &[]) as c_long;
    io.b = h(O::MpTerrainTypeOf, t, &[]) as c_long;
    io.c = h(O::MpTerrainColorType, t, &[]) as c_long;
}

/// CLIFF: a cliff terrain's first and last terrain.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_host_cliff(io: *mut EeAppMapIo) {
    // SAFETY: the host's io
    let io = unsafe { &mut *io };
    io.b = h(O::MpCliffLast, 0, &[io.a as i64]) as c_long;
    io.a = h(O::MpCliffFirst, 0, &[io.a as i64]) as c_long;
}

/// LOCAL_SEES: the local player sees through the player (itself, or with shared sight between allies).
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_host_local_sees(player: *mut c_void, io: *mut EeAppMapIo) {
    let pl = player as P;
    let local = super::map::local_player();
    // SAFETY: the host's io
    let flag = unsafe { (*io).flag };
    let sees = local == pl
        || (flag
            && h(O::MpPlAlliedWith, local, &[pa(pl)]) != 0
            && (super::map::shared_los(local) as i64) != 0
            && (super::map::shared_los(pl) as i64) != 0);
    // SAFETY: as above
    unsafe { (*io).flag = sees };
}

/// TILES_NEAR: the tiles near a point (the context's vector), their coordinates and number.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_host_tiles_near(tiles: *mut c_void, io: *mut EeAppMapIo) {
    // SAFETY: the host's io
    let io = unsafe { &mut *io };
    let point: [f32; 3] = [io.f[0], io.f[1], 0.0];
    h(O::MpTileVecClear, tiles as P, &[]);
    // SAFETY: a live std::vector<EETileCoordinate>
    unsafe { ee_units_uu_get_tiles_near_point(&point, io.f[2], tiles) };
    io.tiles = hp(O::MpTileVecData, tiles as P, &[]) as *const c_long;
    io.a = h(O::MpTileVecSize, tiles as P, &[]) as c_long;
}

/// SIGHT_AT: the player's own or shared sight added at or removed from the tile.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_host_sight_at(player: *mut c_void, io: *const EeAppMapIo) {
    // SAFETY: the host's io
    let io = unsafe { &*io };
    let op = if io.a != 0 && io.flag {
        O::MpAddSharedSight
    } else if io.a != 0 {
        O::MpRemoveSharedSight
    } else if io.flag {
        O::MpAddOwnSight
    } else {
        O::MpRemoveOwnSight
    };
    h(op, player as P, &[io.x as i64, io.y as i64]);
}

/// COUNT_CELL: the mini map's cell counted on or off.
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_host_count_cell(io: *const EeAppMapIo) {
    // SAFETY: the host's io
    let io = unsafe { &*io };
    super::minimap::count_cell(io.x as c_long, io.y as c_long, io.flag);
}

// ---- TSGT (TEST ONLY)

/// The sight capture's record for the world's map, with the tile's index in the map's tile array (-1: not one of
/// them).
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_sight_record(
    shape: u32,
    kind: u32,
    player: u64,
    tile: u64,
    tile_x: c_long,
    tile_y: c_long,
    value: c_ulong,
    args: *const f32,
) {
    let m = super::map::world_map();
    if m == 0 {
        return;
    }
    let array = hp(O::MpTileArray, m, &[]) as u64;
    let count = fields(m).mTileArrayCount as u64;
    let size = h(O::MpTileSize, 0, &[]) as u64;
    let index = tile_index(tile, array, count, size);
    let mesh = hp(O::MpMeshRust, m, &[]);
    // SAFETY: the caller's arguments, the world's map
    unsafe {
        ee_app_sight_record(shape, kind, player, tile, index, tile_x, tile_y, value, args, m as u64, mesh as *const _)
    };
}

/// A tile's index in a tile array of `count` tiles `size` bytes each (-1: not one of them).
fn tile_index(tile: u64, array: u64, count: u64, size: u64) -> c_long {
    if tile != 0 && array != 0 && tile >= array && tile < array + count * size {
        ((tile - array) / size) as c_long
    } else {
        -1
    }
}

// ---- the map's host for ee_app map.rs (was EEMapHostOp, EEMapHost, EEMapEdit)

/// A call's context: the map, the sight's player and TILES_NEAR's answer (a C++ vector made on first use).
struct MapContext {
    map: P,
    player: P,
    tiles: Option<Owned>,
}

unsafe extern "C" fn map_host_op(context: *mut c_void, op: i32, io: *mut EeAppMapIo) {
    // SAFETY: the context of the call (map_host() below)
    let ctx = unsafe { &mut *(context as *mut MapContext) };
    let (m, pl) = (ctx.map, ctx.player);
    // SAFETY: the caller's io
    let (x, y) = unsafe { ((*io).x, (*io).y) };
    match op {
        EE_APP_MAP_OP_TILE => {
            let tile = super::map::map_tile(m, x as i64, y as i64);
            // SAFETY: as above
            unsafe { (*io).flag = tile != 0 };
        }
        EE_APP_MAP_OP_TERRAIN_TYPE => {
            let kind = h(O::FxTileTerrainType, super::map::map_tile(m, x as i64, y as i64), &[]);
            // SAFETY: as above
            unsafe { (*io).a = kind as c_long };
        }
        // SAFETY: as above
        EE_APP_MAP_OP_SET_TERRAIN_TYPE => {
            ee_gamecpp_map_set_terrain_type(m as *mut c_void, x, y, unsafe { (*io).a });
        }
        EE_APP_MAP_OP_SET_TILE_TERRAIN => {
            // SAFETY: as above
            let (set, unload) = unsafe { ((*io).a, (*io).flag) };
            ee_gamecpp_map_set_tile_terrain(m as *mut c_void, x, y, set, unload);
        }
        EE_APP_MAP_OP_TERRAIN => ee_gamecpp_map_host_terrain(io),
        EE_APP_MAP_OP_COLOR_SET => {
            // SAFETY: as above
            unsafe { (*io).a = h(O::MpTypeColorSet, 0, &[(*io).a as i64]) as c_long };
        }
        EE_APP_MAP_OP_CLIFF => ee_gamecpp_map_host_cliff(io),
        EE_APP_MAP_OP_TILE_CHECKSUM => {
            let tile = super::map::map_tile(m, x as i64, y as i64);
            let sum =
                super::tile::ee_gamecpp_tile_checksum(tile as *const std::ffi::c_void, rts(m).mU19561C as c_ulong);
            // SAFETY: as above
            unsafe { (*io).a = sum };
        }
        EE_APP_MAP_OP_SIGHT_ENABLED => {
            let on = h(O::MpSightEnabled, 0, &[]) != 0;
            // SAFETY: as above
            unsafe { (*io).flag = on };
        }
        EE_APP_MAP_OP_LOCAL_SEES => ee_gamecpp_map_host_local_sees(pl as *mut c_void, io),
        EE_APP_MAP_OP_TILES_NEAR => {
            let tiles = ctx.tiles.get_or_insert_with(|| Owned::new(O::MpTileVecNew, O::MpTileVecDelete)).0;
            ee_gamecpp_map_host_tiles_near(tiles as *mut c_void, io);
        }
        EE_APP_MAP_OP_SIGHT_AT => ee_gamecpp_map_host_sight_at(pl as *mut c_void, io),
        EE_APP_MAP_OP_COUNT_CELL => ee_gamecpp_map_host_count_cell(io),
        _ => {}
    }
}

/// The map's host over a call's context.
fn map_host(ctx: &mut MapContext) -> EeAppMapHost {
    let m = ctx.map;
    EeAppMapHost {
        call: map_host_op,
        ctx: ctx as *mut MapContext as *mut c_void,
        mesh: hp(O::MpMeshRust, m, &[]) as *mut c_void,
        dims: dims(m),
    }
}

/// One of the map's edits (ee_app_map_edit's entries: 0 the water, 1 the base terrain's fill, 2 the cliffs, 3 the
/// flat cliffs, 4 a tile's paint, 5 a rectangle's).
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub extern "C" fn ee_gamecpp_map_edit(
    map: *mut c_void,
    what: u32,
    a: c_long,
    b: c_long,
    c: c_long,
    d: c_long,
    e: c_long,
    f: c_long,
) {
    let args: [c_long; 6] = [a, b, c, d, e, f];
    let mut ctx = MapContext { map: map as P, player: 0, tiles: None };
    let host = map_host(&mut ctx);
    // SAFETY: the host over the live context, the six arguments
    unsafe { ee_app_map_edit(&host, what, args.as_ptr()) };
}

/// EEMap::AddSight / RemoveSight's body (between their capture scopes): a player's own or shared sight of the
/// mesh-flagged tiles within a radius (ee_app map.rs).
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_sight(
    map: *mut c_void,
    player: *mut c_void,
    x: f32,
    y: f32,
    radius: f32,
    shared: bool,
    add: bool,
) {
    let mut ctx = MapContext { map: map as P, player: player as P, tiles: None };
    let host = map_host(&mut ctx);
    // SAFETY: the host over the live context
    unsafe { ee_app_map_sight(&host, x, y, radius, shared, add) };
}

/// EEMap::ComputeChecksum: the next 64 x 64 window's tile and corner-height checksums, the window advanced (MAPK).
#[no_mangle]
pub extern "C" fn ee_gamecpp_map_checksum(map: *mut c_void) -> c_long {
    let m = map as P;
    let before = fields(m).mChecksumCursor;
    // SAFETY: the map's record (read and written by the checksum in place, as the C++ passed it)
    let cursor = unsafe { &mut (*super::map::map_fields(m)).mChecksumCursor as *mut c_ulong };
    let mut ctx = MapContext { map: m, player: 0, tiles: None };
    let host = map_host(&mut ctx);
    // SAFETY: the host over the live context, the record's cursor
    let sum = unsafe { ee_app_map_checksum(&host, cursor) };
    ee_app_capture_map_checksum(before, sum, fields(m).mChecksumCursor); // MAPK (TEST ONLY)
    sum
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn two_players_share_a_visibility_word() {
        assert_eq!(words_per_tile(1), 1);
        assert_eq!(words_per_tile(2), 1);
        assert_eq!(words_per_tile(3), 2);
        assert_eq!(words_per_tile(8), 4);
    }

    #[test]
    fn the_ceiling_rounds_up_below_the_value() {
        assert_eq!(ceil_unsigned(64.0 / 3.0), 22);
        assert_eq!(ceil_unsigned(30.0 / 10.0), 3);
        assert_eq!(ceil_unsigned(0.0), 0);
    }

    #[test]
    fn the_inline_apply_rotates_a_vector() {
        let t = GeTransformation::IDENTITY;
        assert_eq!(apply_vector(&t, GeVector::new(1.0, 2.0, 3.0)), GeVector::new(1.0, 2.0, 3.0));
    }

    #[test]
    fn a_tile_index_is_inside_the_array_only() {
        assert_eq!(tile_index(0x1000 + 3 * 16, 0x1000, 10, 16), 3);
        assert_eq!(tile_index(0x1000 + 10 * 16, 0x1000, 10, 16), -1);
        assert_eq!(tile_index(0, 0x1000, 10, 16), -1);
        assert_eq!(tile_index(0x800, 0x1000, 10, 16), -1);
    }
}
