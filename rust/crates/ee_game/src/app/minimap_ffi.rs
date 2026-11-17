//! `EEMiniMap`'s C ABI (include/rust/ee_app.h): the Rust half as an object the C++ class owns, its bodies over one
//! host record (the C++ callback, its context, the terrain mesh). No host operation calls back into the mini map.
#![allow(clippy::unnecessary_cast)]

use crate::app::minimap::{
    EeAppMiniMap, EeAppMiniMapDot, EeAppMiniMapItem, EeAppMiniMapUnitDot, MiniMapHost, TileSight,
};
use ee_render::ge::tmobj::{ee_ge_tm_is_valid, ee_ge_tm_point, ee_ge_tm_tile, ee_ge_tm_vertex_color, EeGeTerrainMesh};
use ee_render::ge::types::GeTransformation;
use ee_render::ge::types::GeVector;
use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

/// One host operation's arguments and answers.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAppMiniMapIo {
    pub x: c_long,
    pub y: c_long,
    /// an answer; `TILE_SIGHT`: the tile exists
    pub flag: bool,
    pub seen: bool,
    pub visible: bool,
    pub all: bool,
    pub visibility: c_ulong,
    /// `TERRAIN_COLOR`: the terrain id in, its colour out
    pub id: c_ulong,
    pub color: [f32; 3],
    /// `TILE_ITEMS`' answer: `count` items (the C++ keeps them until its next call)
    pub items: *const EeAppMiniMapItem,
    pub count: c_long,
}

impl Default for EeAppMiniMapIo {
    fn default() -> Self {
        EeAppMiniMapIo {
            x: 0,
            y: 0,
            flag: false,
            seen: false,
            visible: false,
            all: false,
            visibility: 0,
            id: 0,
            color: [0.0; 3],
            items: std::ptr::null(),
            count: 0,
        }
    }
}

/// The host: `(ctx, op, io)`.
pub type EeAppMiniMapFn = unsafe extern "C" fn(*mut c_void, i32, *mut EeAppMiniMapIo);

// ---- the operations (io's fields: in -> out)
/// x, y -> flag: a unit dot on the tile is drawn (no tile, the local player sees it, or the free camera)
pub const EE_APP_MINIMAP_OP_UNIT_SHOWN: i32 = 1;
/// x, y -> flag (a tile), seen, visible, all, visibility: `TellTileVisibility`'s reads
pub const EE_APP_MINIMAP_OP_TILE_SIGHT: i32 = 2;
/// x, y -> items, count: `TellTile`'s doppelgangers and units
pub const EE_APP_MINIMAP_OP_TILE_ITEMS: i32 = 3;
/// id -> color: `EETerrainTable::Get(id)->mColor`
pub const EE_APP_MINIMAP_OP_TERRAIN_COLOR: i32 = 4;

/// A call's host: the callback, its context, the world map's mesh (`EeGeTerrainMesh`) and the terrain light.
#[repr(C)]
pub struct EeAppMiniMapHost {
    pub call: EeAppMiniMapFn,
    pub ctx: *mut c_void,
    pub mesh: *mut c_void,
    pub light: [f32; 3],
}

struct Host<'a> {
    h: &'a EeAppMiniMapHost,
}

impl Host<'_> {
    fn op(&mut self, op: i32, io: EeAppMiniMapIo) -> EeAppMiniMapIo {
        let mut io = io;
        // SAFETY: the C++ passed a live context and its callback for the entry's duration
        unsafe { (self.h.call)(self.h.ctx, op, &mut io) };
        io
    }
    fn mesh(&self) -> *mut EeGeTerrainMesh {
        self.h.mesh.cast()
    }
}

// SAFETY (every mesh call below): the C++ passed the world map's live mesh; the coordinates are those the original
// read (a valid point's, a tile's around it)
impl MiniMapHost for Host<'_> {
    fn unit_shown(&mut self, x: c_long, y: c_long) -> bool {
        self.op(EE_APP_MINIMAP_OP_UNIT_SHOWN, EeAppMiniMapIo { x, y, ..Default::default() }).flag
    }
    fn tile_sight(&mut self, x: c_long, y: c_long) -> Option<TileSight> {
        let io = self.op(EE_APP_MINIMAP_OP_TILE_SIGHT, EeAppMiniMapIo { x, y, ..Default::default() });
        io.flag.then_some(TileSight { seen: io.seen, visible: io.visible, all: io.all, visibility: io.visibility })
    }
    fn tile_items(&mut self, x: c_long, y: c_long, out: &mut Vec<EeAppMiniMapItem>) {
        let io = self.op(EE_APP_MINIMAP_OP_TILE_ITEMS, EeAppMiniMapIo { x, y, ..Default::default() });
        for i in 0..io.count.max(0) as usize {
            // SAFETY: the C++ answers `count` items that live until its next call
            out.push(unsafe { *io.items.add(i) });
        }
    }
    fn terrain_color(&mut self, id: c_ulong) -> [f32; 3] {
        self.op(EE_APP_MINIMAP_OP_TERRAIN_COLOR, EeAppMiniMapIo { id, ..Default::default() }).color
    }
    fn valid_point(&mut self, x: c_ulong, y: c_ulong) -> bool {
        unsafe { ee_ge_tm_is_valid(self.mesh(), x, y, false) }
    }
    fn valid_tile(&mut self, x: c_ulong, y: c_ulong) -> bool {
        unsafe { ee_ge_tm_is_valid(self.mesh(), x, y, true) }
    }
    fn texture_set(&mut self, x: c_ulong, y: c_ulong) -> c_ulong {
        unsafe { (*ee_ge_tm_tile(self.mesh(), x, y)).texture_set_id as c_ulong }
    }
    fn vertex_color(&mut self, x: c_ulong, y: c_ulong) -> [f32; 3] {
        unsafe { *ee_ge_tm_vertex_color(self.mesh(), x, y).cast::<[f32; 3]>() }
    }
    fn normal(&mut self, x: c_ulong, y: c_ulong) -> GeVector {
        unsafe { *(*ee_ge_tm_point(self.mesh(), x, y)).normal }
    }
}

/// A new mini map's Rust half (no map: every count zero, every list empty).
#[no_mangle]
pub extern "C" fn ee_app_minimap_new() -> *mut EeAppMiniMap {
    Box::into_raw(Box::default())
}

/// # Safety
/// `p` came from [`ee_app_minimap_new`] (or is null) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_app_minimap_free(p: *mut EeAppMiniMap) {
    if !p.is_null() {
        // SAFETY: the function's contract
        drop(unsafe { Box::from_raw(p) });
    }
}

/// `SetMap`'s numbers: the three transformations (16 floats each, `GETransformation`) and the map points a pixel
/// spans.
///
/// # Safety
/// `p` is live; the transformations point at 16 floats each.
#[no_mangle]
pub unsafe extern "C" fn ee_app_minimap_set_map(
    p: *mut EeAppMiniMap,
    map_to_pixel: *const f32,
    pixel_to_map: *const f32,
    map_to_cell: *const f32,
    pixel_width: c_long,
    pixel_height: c_long,
) {
    // SAFETY: the function's contract (GETransformation is 4 x 4 floats, row-major, as GeTransformation)
    unsafe {
        let m = &mut *p;
        m.map_to_pixel = *map_to_pixel.cast::<GeTransformation>();
        m.pixel_to_map = *pixel_to_map.cast::<GeTransformation>();
        m.map_to_cell = *map_to_cell.cast::<GeTransformation>();
        m.pixel_width = pixel_width;
        m.pixel_height = pixel_height;
    }
}

/// The unit layer's locked pixels (`mPixels`, `mPixelsPerRow`) after each lock.
///
/// # Safety
/// `p` is live; `pixels` holds 64 rows of `pitch` pixels until the next call.
#[no_mangle]
pub unsafe extern "C" fn ee_app_minimap_set_pixels(p: *mut EeAppMiniMap, pixels: *mut u16, pitch: c_long) {
    // SAFETY: the function's contract
    unsafe {
        (*p).pixels = pixels;
        (*p).pitch = pitch;
    }
}

/// The mini map's bodies by entry (`what`): 0 `CountCellOff(x, y)`, 1 `CountCellOn`, 2 `RedrawCell`, 3 `Redraw`'s
/// walk on a map of the side lengths x (`mL195614`) and y (`mL195610`), 4 `AddDoppelganger(item, x, y)`,
/// 5 `RemoveDoppelganger`, 6 `MoveUnit(item, x, y, x2, y2)`.
///
/// # Safety
/// `p` and `host` are live (and its callback and context for the call); `item` is live for 4 to 6.
#[no_mangle]
pub unsafe extern "C" fn ee_app_minimap_call(
    p: *mut EeAppMiniMap,
    host: *const EeAppMiniMapHost,
    what: u32,
    item: *const EeAppMiniMapItem,
    x: c_long,
    y: c_long,
    x2: c_long,
    y2: c_long,
) {
    // SAFETY: the function's contract; no host op reaches the mini map
    let (m, mut h) = unsafe { (&mut *p, Host { h: &*host }) };
    match what {
        0 | 1 => m.count_cell(&mut h, x, y, what == 1),
        2 => m.redraw_cell(&mut h, x, y),
        3 => {
            m.width = x;
            m.height = y;
            m.redraw(&mut h);
        }
        4 | 5 => m.building(&mut h, unsafe { &*item }, x, y, what == 4),
        _ => m.move_unit(&mut h, unsafe { &*item }, x, y, x2, y2),
    }
}

/// `Recolor`: the players' unit and building colours, 17 each.
///
/// # Safety
/// `p` is live; both arrays hold 17 colours.
#[no_mangle]
pub unsafe extern "C" fn ee_app_minimap_recolor(p: *mut EeAppMiniMap, unit_colors: *const u16, colors: *const u16) {
    // SAFETY: the function's contract
    unsafe { (*p).recolor(&*unit_colors.cast::<[u16; 17]>(), &*colors.cast::<[u16; 17]>()) };
}

/// `RedrawPixels(left, right, bottom, top)` into the locked terrain texture (`bits`, rows of `pitch` pixels).
///
/// # Safety
/// `p` and `host` are live; `bits` holds `top` rows of `pitch` pixels and `right <= pitch`.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_app_minimap_redraw_pixels(
    p: *const EeAppMiniMap,
    host: *const EeAppMiniMapHost,
    bits: *mut u16,
    pitch: c_ulong,
    left: c_long,
    right: c_long,
    bottom: c_long,
    top: c_long,
) {
    if top <= bottom || right <= left {
        return;
    }
    // SAFETY: the function's contract
    unsafe {
        let h = &*host;
        let light = GeVector { x: h.light[0], y: h.light[1], z: h.light[2] };
        let bits = std::slice::from_raw_parts_mut(bits, pitch as usize * top as usize);
        (*p).redraw_pixels(&mut Host { h }, light, bits, pitch as usize, left, right, bottom, top);
    }
}

/// `RedrawRect`'s pixel rectangle: `out` = left, right, bottom, top.
///
/// # Safety
/// `p` is live; `out` holds four values.
#[no_mangle]
pub unsafe extern "C" fn ee_app_minimap_pixel_rect(
    p: *const EeAppMiniMap,
    left: c_long,
    right: c_long,
    bottom: c_long,
    top: c_long,
    out: *mut c_long,
) {
    // SAFETY: the function's contract
    unsafe { *out.cast::<[c_long; 4]>() = (*p).pixel_rect(left, right, bottom, top) };
}

/// A cell's count and its dots (the capture's read): the pointers live until the mini map changes.
///
/// # Safety
/// `p` is live, `index` below 4096, the outputs live.
#[no_mangle]
pub unsafe extern "C" fn ee_app_minimap_cell(
    p: *const EeAppMiniMap,
    index: c_long,
    count: *mut u16,
    dots: *mut *const EeAppMiniMapDot,
    dot_count: *mut c_long,
    units: *mut *const EeAppMiniMapUnitDot,
    unit_count: *mut c_long,
) {
    // SAFETY: the function's contract
    unsafe {
        let m = &*p;
        let i = index as usize;
        *count = m.counts[i];
        *dots = m.dots[i].as_ptr();
        *dot_count = m.dots[i].len() as c_long;
        *units = m.units[i].as_ptr();
        *unit_count = m.units[i].len() as c_long;
    }
}
