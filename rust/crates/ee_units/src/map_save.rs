//! The map's plain members as Rust records and its Load and Save in Rust (RUST_PLAN R5b.4, the twenty-eighth round):
//! `RTSMap`'s block `mF1955F4` (+0x1955f4 in the original) .. `mB195620` and `EEMap`'s `mMapCharacteristics`
//! (+0x19564c) .. `mChecksumCursor` are [`EeUnitsRtsMapFields`] and [`EeUnitsMapFields`], which the C++ classes derive
//! from (ee/EEMap.h; names, C++ types and order kept). `RTSMap::Load`/`Save` (the size, the player count, the tiles of
//! the map's diamond row by row, the terrain mesh) and `EEMap::Load`/`Save` (the base terrain, the re-typing of every
//! tile, the characteristics and the cliff terrain; the original's two era bodies as one sequence) are [`load`] and
//! [`save`] over player_save.rs's host (the file's primitives, `EESavePrimitive`) and the map's C++ parts by
//! operation (`EEMapSaveOperation`, EEMap.cpp): the tiles, the terrain tables and the mesh stay C++ objects.

#![allow(non_snake_case)]

use crate::player_save::{EeUnitsPlayerSaveFn, EeUnitsPlayerSaveIo, PlayerSaveHost};
use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

/// `RTSMap`'s plain members (ee/EEMap.h), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsRtsMapFields {
    /// the constructor's tile size (copied to `gMapTileSize`)
    pub mF1955F4: f32,
    pub mF1955F8: f32,
    pub mF1955FC: f32,
    pub mL195600: c_long,
    pub mL195604: c_long,
    pub mL195608: c_long,
    pub mL19560C: c_long,
    /// saved: `Create`'s height
    pub mL195610: c_long,
    /// saved: `Create`'s width
    pub mL195614: c_long,
    /// tiles per side
    pub mSize: c_long,
    /// saved: the player count
    pub mU19561C: c_ulong,
    /// the tiles exist
    pub mB195620: bool,
}

/// `EEMap`'s plain members (ee/EEMap.h), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsMapFields {
    /// saved
    pub mMapCharacteristics: c_ulong,
    /// saved: the base terrain
    pub mU195650: c_ulong,
    /// the next free entry of the tile array
    pub mNextTile: c_ulong,
    pub mTileArrayCount: c_ulong,
    /// saved: the cliff terrain
    pub mU19565C: c_ulong,
    /// `ComputeChecksum` checks 64 x 64 tiles from here
    pub mChecksumCursor: c_ulong,
}

// ---- the operations (`index` a tile's y, the low 32 bits of `io.i` its x, where they take a tile)
/// `Create(mL195614, mL195610, mU19561C, false)`
pub const EE_UNITS_MAP_SAVE_CREATE: i32 = 111;
/// the tile pointer at (x, y): `LoadTile(x, y)` when `io.b`, else NULL
pub const EE_UNITS_MAP_SAVE_LOAD_TILE: i32 = 112;
/// `mTerrainMesh.Load` / `Save`
pub const EE_UNITS_MAP_SAVE_MESH_LOAD: i32 = 113;
pub const EE_UNITS_MAP_SAVE_MESH_SAVE: i32 = 114;
/// the tile at (x, y) saved when there is one (its result; 0 for none)
pub const EE_UNITS_MAP_SAVE_TILE_SAVE: i32 = 115;
/// the default terrain table, chosen once: `io.b` the base terrain was read (then an unknown one becomes
/// `gDefaultTerrain` and the table is its), else `gDefaultTerrain`'s table (none for -1)
pub const EE_UNITS_MAP_SAVE_PICK_DEFAULT: i32 = 116;
/// queries of the tile at (x, y) into `io.i`: the mesh's valid flag; the terrain table's type of its texture set
/// (`io.b`: there is a table); the mesh's space flag
pub const EE_UNITS_MAP_SAVE_VALID: i32 = 117;
pub const EE_UNITS_MAP_SAVE_TERRAIN_TYPE: i32 = 118;
pub const EE_UNITS_MAP_SAVE_SPACE: i32 = 119;
/// the tile's `SetTerrainType` (the type in the upper 32 bits of `io.i`)
pub const EE_UNITS_MAP_SAVE_SET_TYPE: i32 = 120;
/// the tile's terrain table's `Load(mTerrainMesh)`
pub const EE_UNITS_MAP_SAVE_TERRAIN_LOAD: i32 = 121;
/// the default terrain on the tile: `SetTileTerrain(default's texture set)`, then its type
pub const EE_UNITS_MAP_SAVE_SET_DEFAULT: i32 = 122;
/// `io.i` a usable cliff terrain id (below the count, in the table): `io.b` out
pub const EE_UNITS_MAP_SAVE_CLIFF_VALID: i32 = 123;
/// `gFirstCliffTerrain` into `io.i`
pub const EE_UNITS_MAP_SAVE_FIRST_CLIFF: i32 = 124;
/// the region tables made again for `mSize`, then `UpdateCliffs` over the map
pub const EE_UNITS_MAP_SAVE_FINISH: i32 = 125;

/// `kTerrainCliff`, `kTerrainSpace` (ee/EEMap.h)
const TERRAIN_CLIFF: i64 = 9;
const TERRAIN_SPACE: i64 = 12;

const OK: i32 = 0;

/// The map's records behind their raw pointers: one field read or written at a time (no Rust reference to the C++
/// object across a host call; the operations read and write the members too).
pub struct Recs {
    rts: *mut EeUnitsRtsMapFields,
    map: *mut EeUnitsMapFields,
}

macro_rules! get {
    ($p:expr, $f:ident) => {
        // SAFETY: the records are live for the call (the caller's contract)
        unsafe { std::ptr::addr_of!((*$p).$f).read() }
    };
}
macro_rules! set {
    ($p:expr, $f:ident, $v:expr) => {{
        let v = $v;
        // SAFETY: as get!
        unsafe { std::ptr::addr_of_mut!((*$p).$f).write(v) }
    }};
}
macro_rules! read {
    ($h:expr, $p:expr, $f:ident, $read:ident) => {{
        let mut v = get!($p, $f);
        let result = $h.$read(&mut v);
        set!($p, $f, v);
        result
    }};
}

/// The map's diamond, row by row: for each row `y` of a `size`-wide map of height `height`, the first and last
/// column that hold a tile (`left <= x < right + 1`). The left edge shrinks to 0 then grows; the right edge grows to
/// `size - 1`, then shrinks (`Create`'s and `Load`'s walk).
pub fn diamond_rows(size: c_long, height: c_long) -> Vec<(c_long, c_long)> {
    let mut rows = Vec::with_capacity(size.max(0) as usize);
    let mut left_shrinking = true;
    let mut right_growing = true;
    let mut right = height;
    let mut left = right.wrapping_sub(1);
    for _ in 0..size {
        rows.push((left, right));
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
                right_growing = false;
                right = size - 1;
            }
        } else {
            right -= 1;
        }
    }
    rows
}

/// A tile operation's call: `index` the tile's y, `io.i` its x (with a value in the upper 32 bits), `io.b` a flag in;
/// the operation's `io` out (`io.i` the result for the load and save of a tile).
fn tile(h: &mut impl PlayerSaveHost, op: i32, x: c_long, y: c_long, value: i64, flag: bool) -> EeUnitsPlayerSaveIo {
    let mut io = EeUnitsPlayerSaveIo { i: (value << 32) | i64::from(x as u32), f: 0.0, b: u8::from(flag) };
    let r = h.call(op, y as u32, &mut io);
    if op == EE_UNITS_MAP_SAVE_TILE_SAVE || op == EE_UNITS_MAP_SAVE_LOAD_TILE {
        io.i = i64::from(r);
    }
    io
}

/// `RTSMap::Load`: the width, height and player count, the map made again, its diamond's tiles loaded row by row (the
/// rest NULL), the terrain mesh; the first read error.
fn rts_load(h: &mut impl PlayerSaveHost, r: &Recs) -> i32 {
    let mut result = read!(h, r.rts, mL195614, read_long);
    if result != OK {
        return result;
    }
    result = read!(h, r.rts, mL195610, read_long);
    if result != OK {
        return result;
    }
    result = read!(h, r.rts, mU19561C, read_ulong);
    if result != OK {
        return result;
    }
    h.op(EE_UNITS_MAP_SAVE_CREATE);
    let size = get!(r.rts, mSize);
    for (y, (left, right)) in diamond_rows(size, get!(r.rts, mL195610)).into_iter().enumerate() {
        for x in 0..size {
            tile(h, EE_UNITS_MAP_SAVE_LOAD_TILE, x, y as c_long, 0, x >= left && x < right.wrapping_add(1));
        }
    }
    h.op(EE_UNITS_MAP_SAVE_MESH_LOAD)
}

/// `RTSMap::Save`: the width, height and player count, every tile there is (row-major), the mesh; the first error.
fn rts_save(h: &mut impl PlayerSaveHost, r: &Recs) -> i32 {
    let mut result = h.write_long(get!(r.rts, mL195614));
    if result == OK {
        result = h.write_long(get!(r.rts, mL195610));
    }
    if result == OK {
        result = h.write_ulong(get!(r.rts, mU19561C));
    }
    if result != OK {
        return result;
    }
    let size = get!(r.rts, mSize);
    'rows: for y in 0..size {
        for x in 0..size {
            result = tile(h, EE_UNITS_MAP_SAVE_TILE_SAVE, x, y, 0, false).i as i32;
            if result != OK {
                break 'rows;
            }
        }
    }
    if result == OK {
        result = h.op(EE_UNITS_MAP_SAVE_MESH_SAVE);
    }
    result
}

/// `EEMap::Load`: the base's part and the base terrain (an unknown one becomes the default), every valid tile
/// re-typed by its terrain table (a cliff or no table: the default terrain; from 0.204 a space tile is space; a
/// table's media loaded), the characteristics, the cliff terrain (up to 0.203 not saved: the first; later an unusable
/// one becomes the first), the region tables and cliffs. The result: up to 0.203 the characteristics' read, later
/// the cliff terrain's.
fn load(h: &mut impl PlayerSaveHost, r: &Recs, major: u32, minor: u32) -> i32 {
    let old = major == 0 && minor <= 0xcb;
    let loaded = rts_load(h, r) == OK && read!(h, r.map, mU195650, read_ulong) == OK;
    h.call(EE_UNITS_MAP_SAVE_PICK_DEFAULT, 0, &mut EeUnitsPlayerSaveIo { b: u8::from(loaded), ..Default::default() });
    let size = get!(r.rts, mSize);
    for x in 0..size {
        for y in 0..size {
            if tile(h, EE_UNITS_MAP_SAVE_VALID, x, y, 0, false).i == 0 {
                continue;
            }
            let terrain = tile(h, EE_UNITS_MAP_SAVE_TERRAIN_TYPE, x, y, 0, false);
            let kind = terrain.i;
            if terrain.b != 0 && kind != TERRAIN_CLIFF {
                let kind =
                    if !old && tile(h, EE_UNITS_MAP_SAVE_SPACE, x, y, 0, false).i != 0 { TERRAIN_SPACE } else { kind };
                tile(h, EE_UNITS_MAP_SAVE_SET_TYPE, x, y, kind, false);
                tile(h, EE_UNITS_MAP_SAVE_TERRAIN_LOAD, x, y, 0, false);
            } else {
                tile(h, EE_UNITS_MAP_SAVE_SET_DEFAULT, x, y, 0, false);
            }
        }
    }
    let first_cliff = || EeUnitsPlayerSaveIo::default();
    let result;
    if old {
        result = read!(h, r.map, mMapCharacteristics, read_ulong);
        let mut io = first_cliff();
        h.call(EE_UNITS_MAP_SAVE_FIRST_CLIFF, 0, &mut io);
        set!(r.map, mU19565C, io.i as c_ulong);
    } else {
        read!(h, r.map, mMapCharacteristics, read_ulong);
        result = read!(h, r.map, mU19565C, read_ulong);
        let mut io = EeUnitsPlayerSaveIo { i: get!(r.map, mU19565C) as i64, ..Default::default() };
        h.call(EE_UNITS_MAP_SAVE_CLIFF_VALID, 0, &mut io);
        if io.b == 0 {
            let mut io = first_cliff();
            h.call(EE_UNITS_MAP_SAVE_FIRST_CLIFF, 0, &mut io);
            set!(r.map, mU19565C, io.i as c_ulong);
        }
    }
    h.op(EE_UNITS_MAP_SAVE_FINISH);
    result
}

/// `EEMap::Save`: the base's part, the base terrain, the characteristics and the cliff terrain; the first error.
fn save(h: &mut impl PlayerSaveHost, r: &Recs) -> i32 {
    let mut result = rts_save(h, r);
    if result != OK {
        return result;
    }
    result = h.write_ulong(get!(r.map, mU195650));
    if result != OK {
        return result;
    }
    result = h.write_ulong(get!(r.map, mMapCharacteristics));
    if result == OK {
        result = h.write_ulong(get!(r.map, mU19565C));
    }
    result
}

struct FfiHost {
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
}

impl PlayerSaveHost for FfiHost {
    fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        // SAFETY: the C++ callback with its own context; io lives for the call
        unsafe { (self.f)(self.ctx, op, index, io) }
    }
}

/// `EEMap::Load` over the map's records; the file's primitives and the map's parts through `f`.
///
/// # Safety
/// The records are the live map's; `f` is valid with `ctx` for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_map_load(
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    rts: *mut EeUnitsRtsMapFields,
    map: *mut EeUnitsMapFields,
    major: u32,
    minor: u32,
) -> i32 {
    load(&mut FfiHost { ctx, f }, &Recs { rts, map }, major, minor)
}

/// `EEMap::Save`: as [`ee_units_map_load`].
///
/// # Safety
/// As [`ee_units_map_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_map_save(
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    rts: *mut EeUnitsRtsMapFields,
    map: *mut EeUnitsMapFields,
) -> i32 {
    save(&mut FfiHost { ctx, f }, &Recs { rts, map })
}

/// `RTSMap::Load` over its record (the virtual slot; `EEMap::Load` runs it inside [`ee_units_map_load`]).
///
/// # Safety
/// As [`ee_units_map_load`] (`map` is not read).
#[no_mangle]
pub unsafe extern "C" fn ee_units_rts_map_load(
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    rts: *mut EeUnitsRtsMapFields,
) -> i32 {
    rts_load(&mut FfiHost { ctx, f }, &Recs { rts, map: std::ptr::null_mut() })
}

/// `RTSMap::Save` over its record.
///
/// # Safety
/// As [`ee_units_rts_map_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_rts_map_save(
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    rts: *mut EeUnitsRtsMapFields,
) -> i32 {
    rts_save(&mut FfiHost { ctx, f }, &Recs { rts, map: std::ptr::null_mut() })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::player_save::{
        EE_UNITS_PLAYER_SAVE_READ_LONG, EE_UNITS_PLAYER_SAVE_READ_ULONG, EE_UNITS_PLAYER_SAVE_WRITE_BOOL,
        EE_UNITS_PLAYER_SAVE_WRITE_LONG, EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
    };
    use std::collections::BTreeMap;

    /// A map host: a word file, the tiles present by (x, y), the terrain types by tile, and a log of the operations.
    #[derive(Default)]
    struct Map {
        words: Vec<i64>,
        at: usize,
        fail_read: Option<usize>,
        reads: usize,
        tiles: BTreeMap<(i64, u32), bool>,
        terrain: BTreeMap<(i64, u32), i64>,
        space: Vec<(i64, u32)>,
        log: Vec<(i32, i64, u32)>,
        rts: Option<*mut EeUnitsRtsMapFields>,
        cliff_valid: bool,
    }

    impl PlayerSaveHost for Map {
        fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
            match op {
                EE_UNITS_PLAYER_SAVE_READ_ULONG | EE_UNITS_PLAYER_SAVE_READ_LONG => {
                    self.reads += 1;
                    if self.fail_read == Some(self.reads) {
                        return 3;
                    }
                    io.i = self.words[self.at];
                    self.at += 1;
                    0
                }
                EE_UNITS_PLAYER_SAVE_WRITE_ULONG..=EE_UNITS_PLAYER_SAVE_WRITE_BOOL => {
                    assert!(op == EE_UNITS_PLAYER_SAVE_WRITE_ULONG || op == EE_UNITS_PLAYER_SAVE_WRITE_LONG);
                    self.words.push(io.i);
                    0
                }
                EE_UNITS_MAP_SAVE_CREATE => {
                    // Create: the size is width + height
                    let rts = self.rts.unwrap();
                    // SAFETY: the test's record
                    unsafe { (*rts).mSize = (*rts).mL195614 + (*rts).mL195610 };
                    self.log.push((op, 0, 0));
                    0
                }
                _ if op >= EE_UNITS_MAP_SAVE_CREATE && op != EE_UNITS_MAP_SAVE_CLIFF_VALID && io.i >> 32 != 0 => {
                    self.log.push((op, io.i >> 32, index));
                    0
                }
                EE_UNITS_MAP_SAVE_LOAD_TILE => {
                    self.tiles.insert((io.i, index), io.b != 0);
                    0
                }
                EE_UNITS_MAP_SAVE_TILE_SAVE => {
                    if self.tiles.get(&(io.i, index)) == Some(&true) {
                        self.words.push(io.i * 100 + i64::from(index));
                    }
                    0
                }
                EE_UNITS_MAP_SAVE_VALID => {
                    io.i = i64::from(self.tiles.get(&(io.i, index)) == Some(&true));
                    0
                }
                EE_UNITS_MAP_SAVE_TERRAIN_TYPE => {
                    let t = self.terrain.get(&(io.i, index)).copied();
                    io.b = u8::from(t.is_some());
                    io.i = t.unwrap_or(0);
                    0
                }
                EE_UNITS_MAP_SAVE_SPACE => {
                    io.i = i64::from(self.space.contains(&(io.i, index)));
                    0
                }
                EE_UNITS_MAP_SAVE_CLIFF_VALID => {
                    io.b = u8::from(self.cliff_valid);
                    0
                }
                EE_UNITS_MAP_SAVE_FIRST_CLIFF => {
                    io.i = 77;
                    0
                }
                _ => {
                    self.log.push((op, io.i, index));
                    0
                }
            }
        }
    }

    #[test]
    fn the_diamond() {
        // a 3 x 2 map: size 5, rows from the height
        assert_eq!(diamond_rows(5, 2), vec![(1, 2), (0, 3), (0, 4), (1, 4), (2, 3)]);
        assert_eq!(diamond_rows(0, 2), vec![]);
    }

    #[test]
    fn save_then_load() {
        let mut rts = EeUnitsRtsMapFields { mL195614: 3, mL195610: 2, mSize: 5, mU19561C: 4, ..Default::default() };
        let mut map = EeUnitsMapFields { mU195650: 6, mMapCharacteristics: 0x11, mU19565C: 2, ..Default::default() };
        let mut h = Map::default();
        for (y, (l, r)) in diamond_rows(5, 2).into_iter().enumerate() {
            for x in 0..5 {
                h.tiles.insert((x, y as u32), x >= i64::from(l) && x < i64::from(r) + 1);
            }
        }
        let recs = Recs { rts: &mut rts, map: &mut map };
        assert_eq!(save(&mut h, &recs), 0);
        // width, height, players, 17 tiles row-major, (mesh), base terrain, characteristics, cliff
        assert_eq!(&h.words[..4], &[3, 2, 4, 100]);
        assert_eq!(h.words.len(), 3 + 17 + 3);
        assert_eq!(h.log, vec![(EE_UNITS_MAP_SAVE_MESH_SAVE, 0, 0)]);
        let tiles = h.tiles.clone();
        let mut back_rts = EeUnitsRtsMapFields::default();
        let mut back_map = EeUnitsMapFields::default();
        // the file without the tiles' own words (the tile loads read nothing here)
        let words: Vec<i64> = [&h.words[..3], &h.words[20..]].concat();
        let mut l = Map { words, cliff_valid: true, rts: Some(&mut back_rts), ..Default::default() };
        l.terrain.insert((1, 0), 4);
        l.terrain.insert((2, 1), TERRAIN_CLIFF);
        l.space.push((1, 0));
        let recs = Recs { rts: &mut back_rts, map: &mut back_map };
        assert_eq!(load(&mut l, &recs, 0, 0x110), 0);
        assert_eq!(l.tiles, tiles);
        assert_eq!((back_rts.mL195614, back_rts.mL195610, back_rts.mU19561C, back_rts.mSize), (3, 2, 4, 5));
        assert_eq!(
            back_map,
            EeUnitsMapFields { mU195650: 6, mMapCharacteristics: 0x11, mU19565C: 2, ..Default::default() }
        );
        // the valid tiles by column: (1, 0) space; (2, 1) a cliff: the default; the others no table: the default
        let set: Vec<_> = l.log.iter().filter(|e| e.0 == EE_UNITS_MAP_SAVE_SET_TYPE).collect();
        assert_eq!(set, vec![&(EE_UNITS_MAP_SAVE_SET_TYPE, TERRAIN_SPACE, 0)]);
        assert_eq!(l.log.iter().filter(|e| e.0 == EE_UNITS_MAP_SAVE_SET_DEFAULT).count(), 16);
        assert_eq!(l.log.first().unwrap().0, EE_UNITS_MAP_SAVE_CREATE);
        assert_eq!(l.log.last().unwrap().0, EE_UNITS_MAP_SAVE_FINISH);
    }

    #[test]
    fn the_old_saves_and_failures() {
        // up to 0.203: no space tiles, no cliff terrain saved (the first), the characteristics' result
        let mut rts = EeUnitsRtsMapFields::default();
        let mut map = EeUnitsMapFields { mU19565C: 5, ..Default::default() };
        let mut h = Map { words: vec![1, 1, 2, 9, 0x22], rts: Some(&mut rts), ..Default::default() };
        let recs = Recs { rts: &mut rts, map: &mut map };
        // a 1 x 1 map: size 2, the diamond's tiles (0,0),(1,0)? rows (0, 1), (0, 1)
        h.space.push((0, 0));
        h.terrain.insert((0, 0), 4);
        assert_eq!(load(&mut h, &recs, 0, 0xcb), 0);
        assert_eq!((map.mU195650, map.mMapCharacteristics, map.mU19565C), (9, 0x22, 77));
        assert!(h.log.contains(&(EE_UNITS_MAP_SAVE_SET_TYPE, 4, 0)));
        // later: an unusable cliff terrain becomes the first; the cliff read's result is the load's
        let mut rts = EeUnitsRtsMapFields::default();
        let mut map = EeUnitsMapFields::default();
        let mut h = Map { words: vec![1, 1, 2, 9, 0x22, 40], rts: Some(&mut rts), ..Default::default() };
        let recs = Recs { rts: &mut rts, map: &mut map };
        assert_eq!(load(&mut h, &recs, 0, 0xcc), 0);
        assert_eq!(map.mU19565C, 77);
        // a failed base read: the default picked as not loaded, the tiles still re-typed, the reads go on
        let mut rts = EeUnitsRtsMapFields::default();
        let mut map = EeUnitsMapFields::default();
        let mut h = Map {
            words: vec![1, 1, 2, 0x22, 3],
            fail_read: Some(4),
            cliff_valid: true,
            rts: Some(&mut rts),
            ..Default::default()
        };
        let recs = Recs { rts: &mut rts, map: &mut map };
        assert_eq!(load(&mut h, &recs, 0, 0x110), 0);
        assert!(h.log.contains(&(EE_UNITS_MAP_SAVE_PICK_DEFAULT, 0, 0)));
        assert_eq!((map.mMapCharacteristics, map.mU19565C), (0x22, 3));
        // a failed width read: nothing made
        let mut h = Map { words: vec![], fail_read: Some(1), ..Default::default() };
        let mut rts = EeUnitsRtsMapFields::default();
        assert_eq!(rts_load(&mut h, &Recs { rts: &mut rts, map: &mut map }), 3);
        assert!(h.log.is_empty());
    }
}
