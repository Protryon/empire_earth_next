//! `RTSMap`'s own bodies (LLE round 8, from `src/RTS Engine/Map/RTSMap.cpp`): the construction and teardown,
//! `Create` (the tile table's diamond of tiles and the flat mesh) and `ApplyVertexColors`. Simulation code (the
//! oracle and the editor). The terrain edits were Rust already (ee_render map_raise.rs); the `EEMap::` members of the
//! unit are game core's.
//!
//! The class stays C++ (`ee/EEMap.h`); its plain members are the record `EeUnitsRtsMapFields` it derives from, read
//! and written in place; the tile table, the mesh, the lock and the queued colours through one-statement shims of
//! `RTSMap.cpp` ([`MapCpp`]), the virtuals by call.

use core::ffi::{c_long, c_ulong, c_void};

use ee_sim::world::map_fields::EeUnitsRtsMapFields;

ee_base::cpp_shims! {
    trait MapCpp;
    fn fields(map: usize) -> *mut EeUnitsRtsMapFields = ee_rtsmapcpp_fields;
    fn lock_slot(map: usize) -> *mut *mut c_void = ee_rtsmapcpp_lock_slot;
    fn l1955ec_slot(map: usize) -> *mut c_long = ee_rtsmapcpp_l1955ec_slot;
    fn set_tile_size(size: f32) = ee_rtsmapcpp_set_tile_size;
    fn tiles_slot(map: usize) -> *mut *mut usize = ee_rtsmapcpp_tiles_slot;
    fn tiles_new(count: c_long) -> *mut usize = ee_rtsmapcpp_tiles_new;
    fn delete_tiles(map: usize) = ee_rtsmapcpp_delete_tiles;
    fn reserve_tiles(map: usize, count: c_ulong) = ee_rtsmapcpp_reserve_tiles;
    fn create_tile(map: usize, height: f32) -> usize = ee_rtsmapcpp_create_tile;
    fn create_flat_mesh(map: usize, width: c_long, height: c_long) = ee_rtsmapcpp_create_flat_mesh;
    fn colors_size(map: usize) -> usize = ee_rtsmapcpp_colors_size;
    fn colors_at(map: usize, index: usize) -> *const EeRtsMapVertexColors = ee_rtsmapcpp_colors_at;
    fn colors_clear(map: usize) = ee_rtsmapcpp_colors_clear;
    fn mesh_valid_point(map: usize, x: c_long, y: c_long) -> bool = ee_rtsmapcpp_mesh_valid_point;
    fn mesh_set_color(map: usize, x: c_long, y: c_long, index: u8) = ee_rtsmapcpp_mesh_set_color;
}

/// `RTSMapVertexColors`: a rectangle of colour-table indices (x in [left, right), y in [bottom, top), row-major).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeRtsMapVertexColors {
    pub bottom: c_long,
    pub left: c_long,
    pub right: c_long,
    pub top: c_long,
    pub indices: *const u8,
}

/// `RTSMap::RTSMap`: the members' starting values (the record has no initializers), the coloured rectangles' lock,
/// the tile size copied to `gMapTileSize`.
///
/// # Safety
/// `map` is the C++ object under construction.
pub unsafe fn construct<C: MapCpp>(c: &mut C, map: usize, scale: f32, new_lock: &mut dyn FnMut() -> *mut c_void) {
    let f = c.fields(map);
    unsafe {
        (*f).mF1955F4 = scale;
        (*f).mF1955F8 = 0.0;
        (*f).mF1955FC = 0.0;
        (*f).mB195620 = false;
        *c.l1955ec_slot(map) = 0;
        *c.tiles_slot(map) = core::ptr::null_mut();
        (*f).mL195600 = 0;
        (*f).mL195604 = 0;
        (*f).mL195610 = 0;
        (*f).mL195614 = 0;
        (*f).mSize = 0;
        (*f).mL195608 = 0;
        (*f).mL19560C = 0;
        *c.lock_slot(map) = new_lock();
    }
    c.set_tile_size(unsafe { (*f).mF1955F4 });
}

/// `RTSMap::Create`: the old tiles deleted, the sizes, the tile table (`size * size`) with the tiles of the
/// diamond the map's rows cover (a row's span shrinks to the left edge, then grows back; its right end grows to the
/// edge, then shrinks), then the flat mesh.
///
/// # Safety
/// `map` is the live object.
pub unsafe fn create<C: MapCpp>(
    c: &mut C,
    map: usize,
    width: c_long,
    height: c_long,
    players: c_ulong,
    create_tiles: bool,
) {
    let f = c.fields(map);
    let mut right = height;
    let mut left = height - 1;
    let mut left_shrinking = true;
    let mut right_growing = true;
    unsafe {
        if (*f).mB195620 {
            c.delete_tiles(map);
        }
        (*f).mL195608 = height - 1;
        (*f).mL195614 = width;
        (*f).mSize = width + height;
        (*f).mL195610 = height;
        (*f).mU19561C = players;
        (*f).mL19560C = (*f).mSize + width - 1;
        (*f).mB195620 = true;
        (*f).mF1955F8 = 84.0;
        (*f).mF1955FC = 42.0;
    }
    let size = unsafe { (*f).mSize };
    let count = size.wrapping_mul(size);
    c.reserve_tiles(map, count as c_ulong);
    let tiles = c.tiles_new(count);
    unsafe { *c.tiles_slot(map) = tiles };
    for y in 0..size {
        if create_tiles {
            for x in 0..size {
                let tile = if x >= left && x < right + 1 { c.create_tile(map, 0.0) } else { 0 };
                // SAFETY: the new table of size * size
                unsafe { *tiles.add((size * y + x) as usize) = tile };
            }
        }
        if left_shrinking {
            if left != 0 {
                left -= 1;
            } else {
                left_shrinking = false;
            }
        } else {
            left += 1;
        }
        if right_growing {
            right += 1;
            if right == size {
                right = size - 1;
                right_growing = false;
            }
        } else {
            right -= 1;
        }
    }
    c.create_flat_mesh(map, width, height);
}

/// `RTSMap::ApplyVertexColors`: the queued rectangles painted on the mesh (the newest first; valid points only),
/// then the queue emptied.
pub fn apply_vertex_colors<C: MapCpp>(c: &mut C, map: usize) {
    let mut i = c.colors_size(map) as c_long - 1;
    while i >= 0 {
        // SAFETY: the queue's element
        let r = unsafe { *c.colors_at(map, i as usize) };
        let width = r.right - r.left;
        let mut offset: c_long = 0;
        for y in r.bottom..r.top {
            for (k, x) in (r.left..r.right).enumerate() {
                if c.mesh_valid_point(map, x, y) {
                    // SAFETY: the rectangle's indices
                    let index = unsafe { *r.indices.add(offset as usize + k) };
                    c.mesh_set_color(map, x, y, index);
                }
            }
            offset += width;
        }
        i -= 1;
    }
    c.colors_clear(map);
}

#[cfg(not(test))]
mod ffi {
    use super::*;
    use ee_base::shim::ShimFfi as Ffi;

    /// `RTSMap::RTSMap`.
    ///
    /// # Safety
    /// `map` is the C++ object under construction.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_map_construct(map: *mut c_void, scale: f32) {
        let mut lock = || ee_base::ts::ee_ts_section_new().cast::<c_void>();
        unsafe { construct(&mut Ffi, map as usize, scale, &mut lock) }
    }

    /// `RTSMap::~RTSMap`: the tiles, then the lock.
    ///
    /// # Safety
    /// `map` is the C++ object being destroyed.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_map_destruct(map: *mut c_void) {
        Ffi.delete_tiles(map as usize);
        unsafe { ee_base::ts::ee_ts_section_free((*Ffi.lock_slot(map as usize)).cast()) };
    }

    /// `RTSMap::Create`.
    ///
    /// # Safety
    /// `map` is the live object.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_map_create(
        map: *mut c_void,
        width: c_long,
        height: c_long,
        players: c_ulong,
        create_tiles: bool,
    ) {
        unsafe { create(&mut Ffi, map as usize, width, height, players, create_tiles) }
    }

    /// `RTSMap::ApplyVertexColors`.
    #[no_mangle]
    pub extern "C" fn ee_rts_map_apply_vertex_colors(map: *mut c_void) {
        apply_vertex_colors(&mut Ffi, map as usize)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_base::shim::ShimFake;

    #[test]
    fn create_makes_the_diamond_of_tiles() {
        let mut f = EeUnitsRtsMapFields::default();
        let mut tiles_slot: *mut usize = core::ptr::null_mut();
        let mut table = vec![0usize; 25];
        let mut c = ShimFake::default();
        c.answer("fields", &[&mut f as *mut _ as i64]);
        c.answer("tiles_slot", &[&mut tiles_slot as *mut _ as i64]);
        c.answer("tiles_new", &[table.as_mut_ptr() as i64]);
        c.answer("create_tile", &[1; 25]);
        // width 2, height 3: size 5
        unsafe { create(&mut c, 1, 2, 3, 4, true) };
        assert_eq!(f.mSize, 5);
        assert_eq!((f.mL195608, f.mL195614, f.mL195610, f.mU19561C, f.mL19560C), (2, 2, 3, 4, 6));
        assert!(f.mB195620);
        let rows: Vec<String> =
            table.chunks(5).map(|r| r.iter().map(|&t| if t != 0 { '#' } else { '.' }).collect()).collect();
        assert_eq!(rows, ["..##.", ".####", "#####", "####.", ".##.."]);
        assert!(c.calls().contains("reserve_tiles(1, 25)"));
        assert!(c.calls().ends_with("create_flat_mesh(1, 2, 3)"));
    }

    #[test]
    fn vertex_colors_paint_newest_first_and_clear() {
        let a = [10u8, 11, 12, 13];
        let b = [20u8];
        let rects = [
            EeRtsMapVertexColors { top: 2, left: 0, bottom: 0, right: 2, indices: a.as_ptr() },
            EeRtsMapVertexColors { top: 1, left: 5, bottom: 0, right: 6, indices: b.as_ptr() },
        ];
        let mut c = ShimFake::default();
        c.answer("colors_size", &[2]);
        c.answer("colors_at", &[&rects[1] as *const _ as i64, &rects[0] as *const _ as i64]);
        c.answer("mesh_valid_point", &[1, 1, 0, 1, 1]);
        apply_vertex_colors(&mut c, 1);
        let sets: Vec<_> = c.log.iter().filter(|l| l.starts_with("mesh_set_color")).cloned().collect();
        assert_eq!(
            sets,
            [
                "mesh_set_color(1, 5, 0, 20)",
                "mesh_set_color(1, 0, 0, 10)",
                "mesh_set_color(1, 0, 1, 12)",
                "mesh_set_color(1, 1, 1, 13)"
            ]
        );
        assert!(c.calls().ends_with("colors_clear(1)"));
    }
}
