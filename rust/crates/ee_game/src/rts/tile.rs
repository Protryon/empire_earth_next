//! `RTSTile`'s bodies (LLE round 8, from `src/RTS Engine/Map/RTSTile.cpp`): a tile's checksum, its saved height and
//! visibility words, its dump and its unit list's removals and search. Simulation code (the oracle and the saves).
//!
//! The class stays C++ (`ee/EEMap.h`: EETile derives from it); its members and its unit list's nodes
//! (`RTSTUnitList`) are reached through one-statement shims of `RTSTile.cpp` ([`TileCpp`]); `FreeUnitNode` is a
//! virtual (EETile returns the node to its pool).

use core::ffi::{c_char, c_long, c_ulong};

ee_base::cpp_shims! {
    trait TileCpp;
    fn terrain_type(tile: usize) -> c_long = ee_rtstcpp_terrain_type;
    fn visibility(tile: usize) -> *const u32 = ee_rtstcpp_visibility;
    fn height_slot(tile: usize) -> *mut f32 = ee_rtstcpp_height_slot;
    fn units_slot(tile: usize) -> *mut usize = ee_rtstcpp_units_slot;
    fn node_unit(node: usize) -> usize = ee_rtstcpp_node_unit;
    fn node_next_slot(node: usize) -> *mut usize = ee_rtstcpp_node_next_slot;
    fn free_node(tile: usize, node: usize) = ee_rtstcpp_free_node;
    fn read_float(file: usize, out: ee_base::shim::ShimOut<f32>) -> c_long = ee_rtscpp_read_float;
    fn write_float(file: usize, value: f32) -> c_long = ee_rtscpp_write_float;
    fn read_bytes(file: usize, data: *mut u8, length: c_ulong, done: *mut c_ulong) -> c_long = ee_rtstcpp_read_bytes;
    fn write_bytes(file: usize, data: *const u8, length: c_ulong, done: *mut c_ulong) -> c_long = ee_rtstcpp_write_bytes;
    fn puts(file: usize, text: *const c_char) = ee_rtstcpp_puts;
}

const OK: c_long = 0;

/// `GetVisibility`: player `id`'s count, two 16-bit entries a word, its low byte.
fn visibility_of(words: *const u32, id: c_ulong) -> c_ulong {
    // SAFETY: the tile's words cover every player
    let w = unsafe { *words.add((id >> 1) as usize) };
    ((w >> ((id & 1) << 4)) & 0xff) as c_ulong
}

/// The visibility words' bytes for `players` (two players a word).
fn visibility_bytes(players: c_ulong) -> c_ulong {
    ((players >> 1) + (players & 1)) * 4
}

/// `RTSTile::GetChecksum`: the terrain type plus every player's visibility count.
pub fn checksum<C: TileCpp>(c: &mut C, tile: usize, players: c_ulong) -> c_long {
    let words = c.visibility(tile);
    let mut count: c_long = 0;
    for i in 0..players {
        count = count.wrapping_add(visibility_of(words, i) as c_long);
    }
    c.terrain_type(tile).wrapping_add(count)
}

/// `RTSTile::Load`: the height, then the visibility words.
pub fn load<C: TileCpp>(c: &mut C, tile: usize, file: usize, players: c_ulong) -> c_long {
    let height = c.height_slot(tile);
    let r = c.read_float(file, ee_base::shim::ShimOut(height));
    if r != OK {
        return r;
    }
    let words = c.visibility(tile) as *mut u8;
    let mut done: c_ulong = 0;
    c.read_bytes(file, words, visibility_bytes(players), &mut done)
}

/// `RTSTile::Save`: the height, then the visibility words.
pub fn save<C: TileCpp>(c: &mut C, tile: usize, file: usize, players: c_ulong) -> c_long {
    // SAFETY: the tile's member
    let height = unsafe { *c.height_slot(tile) };
    let r = c.write_float(file, height);
    if r != OK {
        return r;
    }
    let words = c.visibility(tile) as *const u8;
    let mut done: c_ulong = 0;
    c.write_bytes(file, words, visibility_bytes(players), &mut done)
}

/// `RTSTile::Dump`: every player's visibility count, then the terrain type.
pub fn dump<C: TileCpp>(c: &mut C, tile: usize, file: usize, players: c_ulong) {
    let words = c.visibility(tile);
    for i in 0..players {
        let line = format!("Tile Vis Count For Player {} has a value of {}\n\0", i, visibility_of(words, i) as c_long);
        c.puts(file, line.as_ptr().cast());
    }
    let line = format!("Tile Terrain Type has value {}\n\0", c.terrain_type(tile));
    c.puts(file, line.as_ptr().cast());
}

/// `RTSTile::RemoveAllUnits`: every node freed (a virtual), the list emptied.
///
/// # Safety
/// `tile` is a live tile.
pub unsafe fn remove_all_units<C: TileCpp>(c: &mut C, tile: usize) {
    let mut node = unsafe { *c.units_slot(tile) };
    while node != 0 {
        let next = unsafe { *c.node_next_slot(node) };
        c.free_node(tile, node);
        node = next;
    }
    unsafe { *c.units_slot(tile) = 0 };
}

/// `RTSTile::RemoveUnit`: the unit's first node unlinked and freed.
///
/// # Safety
/// `tile` is a live tile.
pub unsafe fn remove_unit<C: TileCpp>(c: &mut C, tile: usize, unit: usize) {
    let mut previous = 0usize;
    let mut node = unsafe { *c.units_slot(tile) };
    while node != 0 {
        if c.node_unit(node) == unit {
            let next = unsafe { *c.node_next_slot(node) };
            unsafe {
                if previous != 0 {
                    *c.node_next_slot(previous) = next;
                } else {
                    *c.units_slot(tile) = next;
                }
            }
            c.free_node(tile, node);
            return;
        }
        previous = node;
        node = unsafe { *c.node_next_slot(node) };
    }
}

/// `RTSTile::HasUnit`.
///
/// # Safety
/// `tile` is a live tile.
pub unsafe fn has_unit<C: TileCpp>(c: &mut C, tile: usize, unit: usize) -> bool {
    let mut node = unsafe { *c.units_slot(tile) };
    while node != 0 {
        if c.node_unit(node) == unit {
            return true;
        }
        node = unsafe { *c.node_next_slot(node) };
    }
    false
}

#[cfg(not(test))]
mod ffi {
    use super::*;
    use core::ffi::c_void;
    use ee_base::shim::ShimFfi as Ffi;

    /// `RTSTile::GetChecksum`.
    #[no_mangle]
    pub extern "C" fn ee_rts_tile_checksum(tile: *const c_void, players: c_ulong) -> c_long {
        checksum(&mut Ffi, tile as usize, players)
    }

    /// `RTSTile::Load`.
    #[no_mangle]
    pub extern "C" fn ee_rts_tile_load(tile: *mut c_void, file: *mut c_void, players: c_ulong) -> c_long {
        load(&mut Ffi, tile as usize, file as usize, players)
    }

    /// `RTSTile::Save`.
    #[no_mangle]
    pub extern "C" fn ee_rts_tile_save(tile: *mut c_void, file: *mut c_void, players: c_ulong) -> c_long {
        save(&mut Ffi, tile as usize, file as usize, players)
    }

    /// `RTSTile::Dump`.
    #[no_mangle]
    pub extern "C" fn ee_rts_tile_dump(tile: *const c_void, file: *mut c_void, players: c_ulong) {
        dump(&mut Ffi, tile as usize, file as usize, players)
    }

    /// `RTSTile::RemoveAllUnits`.
    ///
    /// # Safety
    /// `tile` is a live tile.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_tile_remove_all_units(tile: *mut c_void) {
        unsafe { remove_all_units(&mut Ffi, tile as usize) }
    }

    /// `RTSTile::RemoveUnit`.
    ///
    /// # Safety
    /// `tile` is a live tile.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_tile_remove_unit(tile: *mut c_void, unit: *mut c_void) {
        unsafe { remove_unit(&mut Ffi, tile as usize, unit as usize) }
    }

    /// `RTSTile::HasUnit`.
    ///
    /// # Safety
    /// `tile` is a live tile.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_tile_has_unit(tile: *mut c_void, unit: *mut c_void) -> bool {
        unsafe { has_unit(&mut Ffi, tile as usize, unit as usize) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_base::shim::ShimFake;

    #[test]
    fn the_checksum_adds_each_players_low_byte() {
        let words = [0x0203_0105u32, 0x0000_0007];
        let mut c = ShimFake::default();
        c.answer("visibility", &[words.as_ptr() as i64]);
        c.answer("terrain_type", &[10]);
        // players 0..3: 5, 3, 7
        assert_eq!(checksum(&mut c, 1, 3), 10 + 5 + 3 + 7);
        assert_eq!(visibility_bytes(3), 8);
        assert_eq!(visibility_bytes(4), 8);
        assert_eq!(visibility_bytes(1), 4);
    }

    /// Nodes as (unit, next) pairs in a vector; the fake's node shims point into it.
    #[test]
    fn removing_a_unit_relinks_and_frees_its_node() {
        // nodes: [unit 7 -> 1], [unit 8 -> 2], [unit 9 -> end]
        let mut next = [0usize; 3];
        let base = next.as_mut_ptr() as usize;
        let node = |i: usize| 0x1000 + i;
        next[0] = node(1);
        next[1] = node(2);
        next[2] = 0;
        let mut head = node(0);
        let mut c = ShimFake::default();
        c.answer("units_slot", &[&mut head as *mut _ as i64; 8]);
        c.answer("node_unit", &[7, 8]);
        // node_next_slot(node(i)) -> &next[i]: answered in call order (the walk asks node 0, then node 1 twice)
        c.answer("node_next_slot", &[base as i64, (base + 8) as i64, base as i64]);
        unsafe { remove_unit(&mut c, 1, 8) };
        assert_eq!(next[0], node(2), "node 0 now links past node 1");
        assert!(c.calls().ends_with(&format!("free_node(1, {})", node(1))));
        let mut c = ShimFake::default();
        c.answer("units_slot", &[&mut head as *mut _ as i64; 8]);
        c.answer("node_unit", &[7]);
        assert!(unsafe { has_unit(&mut c, 1, 7) });
    }
}
