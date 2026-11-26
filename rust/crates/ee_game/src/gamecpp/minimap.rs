//! The game's mini map object's own bodies in Rust (game core lane, round 9; was EEMiniMap.cpp): the dots of a
//! building image and of a unit, the Rust half's host (the local player's sight of the tiles, a tile's items, the
//! terrain colours: `EEMiniMapHostOp` whole), the constructor, the map's transformations, the unit layer, the redraws,
//! the recolour and the view trapezoid. The cells, their dots and pixels are ee_app minimap.rs (`crate::app`). Each
//! C++ call or member is a case of EEEffectHost.cpp's `EECore9OpB` chain (EEMiniMap.cpp's `EECore9OpB3`); the methods
//! forward here (`ee_gamecpp_minimap_*`).

#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};
use std::sync::Mutex;

use ee_base::math::types::{Ge2DPoint, GeRectangle, GeTransformation, GeVector};
use ee_render::ge::viewport::GeViewport;
use ee_render::gfx::effects::EeGfxColor;
use ee_render::gfx::ffi::{ee_gfx_minimap_transforms, ee_gfx_minimap_view};
use ee_sim::units::uu_ffi::ee_units_uu_cloaked_enemy;
use ee_sim::world::map_fields::EeUnitsRtsMapFields;
use ee_sim::world::player_fields::EeUnitsRtsPlayerFields;
use ee_sim::world::unit_class_fields::EeUnitsUnitClassFields;
use ee_sim::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};

use crate::app::minimap::{EeAppMiniMap, EeAppMiniMapItem};
use crate::app::minimap_ffi::{
    ee_app_minimap_call, ee_app_minimap_new, ee_app_minimap_pixel_rect, ee_app_minimap_recolor,
    ee_app_minimap_redraw_pixels, ee_app_minimap_set_map, ee_app_minimap_set_pixels, EeAppMiniMapHost, EeAppMiniMapIo,
    EE_APP_MINIMAP_OP_TERRAIN_COLOR, EE_APP_MINIMAP_OP_TILE_ITEMS, EE_APP_MINIMAP_OP_TILE_SIGHT,
    EE_APP_MINIMAP_OP_UNIT_SHOWN,
};
use crate::app::world_capture::ee_app_capture_minimap_pixels;

use super::{h, hp, pa, ptr, rec, O, P};

// the constants (static_assert'd in EEMiniMap.cpp)
/// A class without a mini map colour.
const MM_NO_COLOR: c_long = 0x44;
/// `kUnitCategoryResource`, `kUnitTypeResForageBush`, `kUnitTypeAirport`.
const MM_CATEGORY_RESOURCE: c_ulong = 1;
const MM_TYPE_FORAGE_BUSH: c_ulong = 0x37;
const MM_TYPE_AIRPORT: c_ulong = 0x42;
/// `kResourceStone`, `kResourceGold`, `kResourceIron`.
const MM_RESOURCE_STONE: c_long = 4;
const MM_RESOURCE_GOLD: c_long = 5;
const MM_RESOURCE_IRON: c_long = 6;

/// `EEMiniMap`'s members from `mL12174` to `mB12185` (static_assert'd in EEMiniMap.cpp).
#[repr(C)]
struct MiniMapTail {
    map: usize,
    l12178: c_long,
    l1217c: c_long,
    l12180: c_long,
    b12184: bool,
    b12185: bool,
}

fn tail<'a>(mm: P) -> &'a mut MiniMapTail {
    // SAFETY: the mini map's members
    unsafe { &mut *rec::<MiniMapTail>(O::MmTail, mm) }
}

fn rust(mm: P) -> *mut EeAppMiniMap {
    hp(O::MmRust, mm, &[]) as *mut EeAppMiniMap
}

fn class_fields<'a>(uc: P) -> &'a EeUnitsUnitClassFields {
    // SAFETY: a live unit class's record
    unsafe { &*rec::<EeUnitsUnitClassFields>(O::UcFields, uc) }
}

fn player_id(pl: P) -> c_ulong {
    // SAFETY: a live player's record
    unsafe { (*rec::<EeUnitsRtsPlayerFields>(O::PlRts, pl)).mPlayerID }
}

/// A resource whose dot hides the units' dots after it: a forage bush, iron, gold or stone.
fn hides_units(category: c_ulong, unit_type: c_ulong, resource: c_long) -> bool {
    category == MM_CATEGORY_RESOURCE
        && (unit_type == MM_TYPE_FORAGE_BUSH
            || resource == MM_RESOURCE_IRON
            || resource == MM_RESOURCE_GOLD
            || resource == MM_RESOURCE_STONE)
}

/// A building image's dot: gaia's in its class colour, others' in the owner's shade; a resource heading the cell keeps
/// the units' dots after it (a wall's facing for its next three segments).
fn building_item(d: P, wall: bool) -> EeAppMiniMapItem {
    let unit = hp(O::MmDoppUnit, d, &[]);
    let class = hp(O::UnClass, unit, &[]);
    let c = *class_fields(class);
    let mut item = EeAppMiniMapItem { shown: c.mMiniMapColor != MM_NO_COLOR, ..Default::default() };
    if item.shown {
        let pl = hp(O::MmDoppPlayer, d, &[]);
        let id = player_id(pl);
        let color = if id == 0 { h(O::MmPackDark, 0, &[c.mMiniMapColor as i64]) } else { h(O::MmPackShade, pl, &[]) };
        item.player = id as u16;
        item.color = color as u16;
        item.flag = hides_units(c.mCategory, c.mUnitType, c.mResourceType) as c_long;
    }
    item.tile_size = c.mTileSize;
    item.wall = wall;
    if wall {
        // SAFETY: the complex unit's record
        item.facing =
            unsafe { (*rec::<EeUnitsComplexUnitFields>(O::CxFields, hp(O::UnComplex, unit, &[]))).mFacing } as c_long;
    }
    item
}

/// A unit's dot (none for a class without a mini map colour or another player's foundation): gaia's in its class
/// colour, others' in the owner's.
fn unit_item(u: P) -> EeAppMiniMapItem {
    let c = *class_fields(hp(O::UnClass, u, &[]));
    // SAFETY: the unit's record
    let resource = unsafe { (*rec::<EeUnitsUnitFields>(O::UnFields, u)).mResourceType } as c_long;
    let cx = hp(O::UnComplex, u, &[]);
    let pl = hp(O::UnPlayer, u, &[]);
    let mut item = EeAppMiniMapItem {
        shown: c.mMiniMapColor != MM_NO_COLOR
            && !(cx != 0 && h(O::FxIsFoundation, cx, &[]) != 0 && pl != hp(O::WorldCurrentPlayer, 0, &[])),
        ..Default::default()
    };
    if item.shown {
        let id = player_id(pl);
        let color =
            if id == 0 { h(O::MmPackColor2, 0, &[c.mMiniMapColor as i64]) } else { h(O::MmPackPlayerColor, pl, &[]) };
        item.player = id as u16;
        item.color = color as u16;
        item.flag = hides_units(c.mCategory, c.mUnitType, resource) as c_long;
    }
    item.tile_size = c.mTileSize;
    item.unit = true;
    // SAFETY: the unit's record
    let r = unsafe { *rec::<EeUnitsRtsUnitFields>(O::UnRts, u) };
    item.x = r.mTileX;
    item.y = r.mTileY;
    item
}

// ---- the Rust half's host (was EEMiniMapHostOp)

/// A call's context: TILE_ITEMS' answer.
#[derive(Default)]
struct HostContext {
    items: Vec<EeAppMiniMapItem>,
}

unsafe extern "C" fn host_op(context: *mut c_void, op: i32, io: *mut EeAppMiniMapIo) {
    // SAFETY: the context of the call (host() below) and the caller's io
    let (ctx, io) = unsafe { (&mut *(context as *mut HostContext), &mut *io) };
    let player = hp(O::WorldCurrentPlayer, 0, &[]);
    let tile = if op == EE_APP_MINIMAP_OP_TERRAIN_COLOR { 0 } else { hp(O::MapTile, 0, &[io.x as i64, io.y as i64]) };
    match op {
        EE_APP_MINIMAP_OP_UNIT_SHOWN => {
            io.flag = tile == 0 || h(O::FxTileSeenBy, tile, &[pa(player)]) != 0 || h(O::FxFreeCamera, 0, &[]) != 0;
        }
        EE_APP_MINIMAP_OP_TILE_SIGHT => {
            io.flag = tile != 0;
            if tile == 0 {
                return;
            }
            io.seen = h(O::FxTileSeenBy, tile, &[pa(player)]) != 0 || h(O::FxFreeCamera, 0, &[]) != 0;
            io.visible = !io.seen && h(O::FxTileVisibleTo, tile, &[pa(player)]) != 0;
            // SAFETY: the player's record
            let sees_all = unsafe { (*rec::<EeUnitsRtsPlayerFields>(O::PlRts, player)).mSeesAll };
            io.all = sees_all || h(O::FxFreeCamera, 0, &[]) != 0;
            io.visibility = h(O::MmTileVisibility, tile, &[player_id(player) as i64]) as c_ulong;
        }
        EE_APP_MINIMAP_OP_TILE_ITEMS => {
            // the tile's building images of the local player (a complex one on its own tile), then its living units
            // the local player may see
            ctx.items.clear();
            if tile != 0 {
                let count = h(O::MmTileDoppCount, tile, &[]) as c_ulong;
                for i in 0..count {
                    let d = hp(O::MmTileDopp, tile, &[i as i64]);
                    if player == hp(O::MmDoppPlayerField, d, &[])
                        && (h(O::MmDoppIsComplex, d, &[]) == 0 || h(O::MmDoppOnTile, d, &[pa(tile)]) != 0)
                    {
                        let unit = hp(O::MmDoppUnit, d, &[]);
                        let airport = class_fields(hp(O::UnClass, unit, &[])).mUnitType == MM_TYPE_AIRPORT;
                        ctx.items.push(building_item(d, airport));
                    }
                }
                let mut node = hp(O::TileUnits, tile, &[]);
                while node != 0 {
                    let unit = hp(O::MmNodeUnit, node, &[]);
                    // SAFETY: the unit's records
                    let (hit_points, r) = unsafe {
                        (
                            (*rec::<EeUnitsUnitFields>(O::UnFields, unit)).mHitPoints,
                            *rec::<EeUnitsRtsUnitFields>(O::UnRts, unit),
                        )
                    };
                    if hit_points != 0
                        && r.mTileX == io.x
                        && r.mTileY == io.y
                        && ee_units_uu_cloaked_enemy(unit as u64, player as u64) == 0
                    {
                        ctx.items.push(unit_item(unit));
                    }
                    node = hp(O::MmNodeNext, node, &[]);
                }
            }
            io.items = if ctx.items.is_empty() { std::ptr::null() } else { ctx.items.as_ptr() };
            io.count = ctx.items.len() as c_long;
        }
        EE_APP_MINIMAP_OP_TERRAIN_COLOR => {
            let mut c = EeGfxColor::default();
            h(O::MmTerrainColor, 0, &[io.id as c_long as i64, ptr(&mut c)]);
            io.color = [c.red, c.green, c.blue];
        }
        _ => {}
    }
}

/// `gMiniMapLight`: the terrain light (each mini map's constructor sets it to (0, 1.5, 1) normalized).
static LIGHT: Mutex<[f32; 3]> = Mutex::new([0.0; 3]);

/// The host of a call over its context (the world map's mesh, the terrain light).
fn host(ctx: &mut HostContext) -> EeAppMiniMapHost {
    let map = hp(O::MpWorldMap, 0, &[]);
    let light = *LIGHT.lock().unwrap();
    EeAppMiniMapHost {
        call: host_op,
        ctx: ctx as *mut HostContext as *mut c_void,
        mesh: if map != 0 { hp(O::MpMeshRust, map, &[]) as *mut c_void } else { std::ptr::null_mut() },
        light,
    }
}

/// One of the Rust half's bodies (ee_app_minimap_call's entries).
fn call(mm: P, what: u32, item: Option<&EeAppMiniMapItem>, x: c_long, y: c_long, x2: c_long, y2: c_long) {
    let mut ctx = HostContext::default();
    let host = host(&mut ctx);
    let item = item.map_or(std::ptr::null(), |i| i as *const EeAppMiniMapItem);
    // SAFETY: the mini map's Rust half and the host over the live context
    unsafe { ee_app_minimap_call(rust(mm), &host, what, item, x, y, x2, y2) };
}

// ---- the methods

/// EEMiniMap's constructor's body: the Rust half, the map attached if given, the terrain light.
#[no_mangle]
pub extern "C" fn ee_gamecpp_minimap_construct(mm: *mut c_void, map: *mut c_void) {
    let mm = mm as P;
    h(O::MmSetRust, mm, &[ee_app_minimap_new() as i64]);
    let t = tail(mm);
    t.b12184 = false;
    t.b12185 = false;
    if map.is_null() {
        let t = tail(mm);
        t.map = 0;
        t.l12178 = 0;
        t.l1217c = 0;
        t.l12180 = 0;
    } else {
        ee_gamecpp_minimap_set_map(mm as *mut c_void, map);
    }
    let mut light = GeVector { x: 0.0, y: 1.5, z: 1.0 };
    // SAFETY: a local vector
    unsafe { ee_base::math::ffi::ee_ge_vector_normalize(&mut light) };
    *LIGHT.lock().unwrap() = [light.x, light.y, light.z];
}

/// AddDoppelganger (4) and RemoveDoppelganger (5): a building image's dot (a wall's next three segments too).
#[no_mangle]
pub extern "C" fn ee_gamecpp_minimap_doppelganger(
    mm: *mut c_void,
    d: *mut c_void,
    x: c_long,
    y: c_long,
    wall: bool,
    add: bool,
) {
    let item = building_item(d as P, wall);
    call(mm as P, if add { 4 } else { 5 }, Some(&item), x, y, 0, 0);
}

/// MoveUnit: a unit's dot between cells (-10000: off the map).
#[no_mangle]
pub extern "C" fn ee_gamecpp_minimap_move_unit(
    mm: *mut c_void,
    unit: *mut c_void,
    ox: c_long,
    oy: c_long,
    x: c_long,
    y: c_long,
) {
    let item = unit_item(unit as P);
    call(mm as P, 6, Some(&item), ox, oy, x, y);
}

/// CountCellOff (0) and CountCellOn (1).
#[no_mangle]
pub extern "C" fn ee_gamecpp_minimap_count_cell(mm: *mut c_void, x: c_long, y: c_long, on: bool) {
    call(mm as P, on as u32, None, x, y, 0, 0);
}

/// UpdateWorldTexture: the 64 x 64 unit layer copied into the world texture and locked again.
#[no_mangle]
pub extern "C" fn ee_gamecpp_minimap_update_world_texture(mm: *mut c_void) {
    let m = mm as P;
    let mut r = GeRectangle { left: 0.0, right: 64.0, top: 0.0, bottom: 64.0 };
    h(O::MmUnitUnlock, m, &[]);
    h(O::MmUnitCopyToWorld, m, &[ptr(&mut r)]);
    ee_gamecpp_minimap_lock_unit_layer(mm);
}

/// LockUnitLayer: the unit layer locked, its pixels to the Rust half.
#[no_mangle]
pub extern "C" fn ee_gamecpp_minimap_lock_unit_layer(mm: *mut c_void) {
    let m = mm as P;
    h(O::MmUnitLock, m, &[]);
    let pixels = hp(O::MmUnitMemory, m, &[]);
    let per_row = (h(O::MmUnitBytesPerRow, m, &[]) as c_long) >> 1;
    h(O::MmSetPixels, m, &[pixels as i64]);
    h(O::MmSetPixelsPerRow, m, &[per_row as i64]);
    // SAFETY: the mini map's Rust half and its locked pixels
    unsafe { ee_app_minimap_set_pixels(rust(m), pixels as *mut u16, per_row) };
}

/// SetMap: the map/pixel and map/cell transformations (rotated 45 degrees, centred), the map size of one pixel, the
/// mini map redrawn (ee_gfx minimap.rs).
#[no_mangle]
pub extern "C" fn ee_gamecpp_minimap_set_map(mm: *mut c_void, map: *mut c_void) {
    let m = mm as P;
    tail(m).map = map as P;
    // SAFETY: the map's record
    let r = unsafe { *rec::<EeUnitsRtsMapFields>(O::MpRts, map as P) };
    let t = ee_gfx_minimap_transforms(r.mL195610 as i32, r.mL195614 as i32, r.mSize as i32);
    let at = hp(O::MmTransforms, m, &[]) as *mut [GeTransformation; 4];
    // SAFETY: the mini map's four transformations (mMapToPixel, mPixelToMap, mMapToCell, mCellToMap in order)
    unsafe { *at = [t.map_to_pixel, t.pixel_to_map, t.map_to_cell, t.cell_to_map] };
    let tl = tail(m);
    tl.l12178 = t.pixel_width as c_long;
    tl.l1217c = t.pixel_height as c_long;
    let (w, ht) = (tl.l12178, tl.l1217c);
    // SAFETY: as above
    unsafe {
        let a = &*at;
        ee_app_minimap_set_map(rust(m), &a[0].m[0][0], &a[1].m[0][0], &a[2].m[0][0], w, ht);
    }
    h(O::MmUiUpdate, m, &[]);
    ee_gamecpp_minimap_lock_unit_layer(mm);
}

/// Redraw: the unit layer and every dot cleared, every tile's sight and contents read again (ee_app minimap.rs).
#[no_mangle]
pub extern "C" fn ee_gamecpp_minimap_redraw(mm: *mut c_void) {
    let m = mm as P;
    let map = hp(O::MpWorldMap, 0, &[]);
    if map == 0 {
        return;
    }
    h(O::MmUnitUnlock, m, &[]);
    let mut black = EeGfxColor::default();
    h(O::MmUnitClear, m, &[ptr(&mut black)]);
    ee_gamecpp_minimap_lock_unit_layer(mm);
    // SAFETY: the map's record
    let r = unsafe { *rec::<EeUnitsRtsMapFields>(O::MpRts, map) };
    call(m, 3, None, r.mL195614, r.mL195610, 0, 0);
}

/// Recolor: every player's dots in their current colours (units in the player colour, buildings in the shade).
#[no_mangle]
pub extern "C" fn ee_gamecpp_minimap_recolor(mm: *mut c_void) {
    let mut units = [0u16; 17];
    let mut colors = [0u16; 17];
    let players = h(O::MmNumPlayers, 0, &[]) as c_ulong;
    for i in 0..players as usize {
        units[i] = h(O::MmPackPlayerColor, hp(O::WorldPlayer, 0, &[i as i64]), &[]) as u16;
        colors[i] = h(O::MmPackShade, hp(O::WorldPlayer, 0, &[i as i64]), &[]) as u16;
    }
    // SAFETY: the mini map's Rust half
    unsafe { ee_app_minimap_recolor(rust(mm as P), units.as_ptr(), colors.as_ptr()) };
}

/// Refresh: the camera's view trapezoid on the mini map (ee_gfx minimap.rs).
#[no_mangle]
pub extern "C" fn ee_gamecpp_minimap_refresh(mm: *mut c_void) {
    let m = mm as P;
    let viewport = super::rast::game_viewport() as *const GeViewport;
    let orthographic = h(O::MpOrthographic, 0, &[]) != 0;
    let at = hp(O::MmTransforms, m, &[]) as *const GeTransformation;
    // SAFETY: the viewport and the mini map's map/pixel transformation
    let view = unsafe { ee_gfx_minimap_view(viewport, orthographic, at) };
    let corners = hp(O::MmCorners, m, &[]) as *mut [Ge2DPoint; 4];
    // SAFETY: the mini map's four corners in order
    unsafe { *corners = view.corners };
}

/// `RefreshView`: Refresh when a map is attached (were EEMiniMap.cpp's body and the camera host's case).
pub fn refresh_view(mm: *mut c_void) {
    if tail(mm as P).map != 0 {
        ee_gamecpp_minimap_refresh(mm);
    }
}

/// `gMiniMap`.
fn world() -> *mut c_void {
    super::globals::get!(gMiniMap) as *mut c_void
}

// ---- the screens' mini map calls (were EEScreens.cpp's and EEUserInterface.cpp's shims)

/// `gMiniMap`.
#[no_mangle]
pub extern "C" fn ee_screenscpp_mini_map() -> usize {
    world() as usize
}

/// `gMiniMap->Refresh()`.
#[no_mangle]
pub extern "C" fn ee_screenscpp_mini_map_refresh() {
    ee_gamecpp_minimap_refresh(world());
}

/// `gMiniMap->RedrawAll()`.
#[no_mangle]
pub extern "C" fn ee_screenscpp_mini_map_redraw_all() {
    ee_gamecpp_minimap_redraw_all(world());
}

/// `gMiniMap->Redraw()`.
#[no_mangle]
pub extern "C" fn ee_screenscpp_mini_map_redraw() {
    ee_gamecpp_minimap_redraw(world());
}

/// `gMiniMap->CountCellOn` / `CountCellOff` (the map host's COUNT_CELL).
pub fn count_cell(x: c_long, y: c_long, on: bool) {
    ee_gamecpp_minimap_count_cell(world(), x, y, on);
}

/// RedrawAll: every terrain pixel recomputed, the terrain texture flagged changed.
#[no_mangle]
pub extern "C" fn ee_gamecpp_minimap_redraw_all(mm: *mut c_void) {
    let m = mm as P;
    h(O::MmTerrainEnter, m, &[]);
    ee_gamecpp_minimap_redraw_pixels(mm, 0, 0x80, 0, 0x80);
    tail(m).b12184 = true;
    h(O::MmTerrainLeave, m, &[]);
}

/// RedrawRect: the terrain pixels covering a map rectangle recomputed, the texture flagged changed.
#[no_mangle]
pub extern "C" fn ee_gamecpp_minimap_redraw_rect(
    mm: *mut c_void,
    left: c_long,
    right: c_long,
    bottom: c_long,
    top: c_long,
) {
    let m = mm as P;
    let mut r: [c_long; 4] = [0; 4];
    h(O::MmTerrainEnter, m, &[]);
    // SAFETY: the mini map's Rust half
    unsafe { ee_app_minimap_pixel_rect(rust(m), left, right, bottom, top, r.as_mut_ptr()) };
    ee_gamecpp_minimap_redraw_pixels(mm, r[0], r[1], r[2], r[3]);
    tail(m).b12184 = true;
    h(O::MmTerrainLeave, m, &[]);
}

/// RedrawPixels: each pixel's terrain colour in a texture rectangle (ee_app minimap.rs), MMPX's record.
#[no_mangle]
pub extern "C" fn ee_gamecpp_minimap_redraw_pixels(
    mm: *mut c_void,
    left: c_long,
    right: c_long,
    bottom: c_long,
    top: c_long,
) {
    let m = mm as P;
    h(O::MmTerrainLock, m, &[]);
    let bits = hp(O::MmTerrainMemory, m, &[]) as *mut u16;
    let pitch = (h(O::MmTerrainBytesPerRow, m, &[]) as c_ulong) >> 1;
    let mut ctx = HostContext::default();
    let host = host(&mut ctx);
    // SAFETY: the locked terrain texture's pixels and the host over the live context
    unsafe {
        ee_app_minimap_redraw_pixels(rust(m), &host, bits, pitch, left, right, bottom, top);
        ee_app_capture_minimap_pixels(left, right, bottom, top, bits, pitch); // MMPX (TEST ONLY)
    }
    h(O::MmTerrainUnlock, m, &[]);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_resources_that_hide_the_units() {
        assert!(hides_units(1, 0x37, 0));
        assert!(hides_units(1, 0, 6));
        assert!(hides_units(1, 0, 4));
        assert!(!hides_units(1, 0, 2));
        assert!(!hides_units(2, 0x37, 6));
    }

    #[test]
    fn the_tail_is_a_pointer_three_longs_and_two_flags() {
        assert_eq!(std::mem::offset_of!(MiniMapTail, l12178), std::mem::size_of::<usize>());
        assert_eq!(std::mem::offset_of!(MiniMapTail, b12185), std::mem::offset_of!(MiniMapTail, b12184) + 1);
    }
}
