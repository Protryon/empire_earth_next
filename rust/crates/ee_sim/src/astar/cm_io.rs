//! The continent manager's whole state as bytes, from the records (`cm`, `area`, `continent`, `area_kinds`,
//! `sector_rec`): the saved game's block (save 0.263, EEAIContinentSave.cpp's format: `WriteRegions` /
//! `ReadRegions`, byte for byte), its digest (`EEContinentRegionsDigest`), and the CNTR capture record with its
//! FNV-1a 64 (`EEAIContinentFill`, `EEAIContinentDigest`, TEST ONLY). The tile lookups they need
//! (`GetContinentID`, `GetPathSubSectorID` with their searches through the sectors once a grid is freed) are here too.
//!
//! The objects stay C++: the record of an object is found from its address by the layout the C++ registers
//! (`EeAStarCmLayout`: each record's offset in its class); what only the C++ can answer goes through the host
//! (`EeAStarCmHostFn`): an area's `Contains` (its hull test writes the HULL capture records), the owner of a town or
//! herd, the region size global, the map tiles' AI values, and making and deleting the objects a load reads.
//!
//! RUST_PLAN 2.1: the C++'s orders (tables by index, lists as stored, the sub sectors numbered as the writer met
//! them), its widths (`U16` written as 4 bytes, a flag as 1), and its bounds checks on a damaged block.

// `c_long`/`c_ulong` are 32 bits on Windows, 64 on LP64: their casts are needed on one or the other
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_long, c_ulong, c_void};
use std::sync::atomic::{AtomicPtr, AtomicUsize, Ordering};

use crate::astar::area::EeAStarAreaRecord;
use crate::astar::area_geom::EeHullRect;
use crate::astar::area_kinds::{
    EeAStarChokeRecord, EeAStarForestRecord, EeAStarHillRecord, EeAStarPathAreaRecord, EeAStarRectRecord,
};
use crate::astar::cm::{EeAStarCmFree, EeAStarCmGrid, EeAStarCmRecord, EeAStarCmShore, EeAStarCmTable};
use crate::astar::continent::{EeAStarContinentList, EeAStarContinentRecord, EE_ASTAR_CONTINENT_LISTS};
use crate::astar::continent_grid::EePoint;
use crate::astar::ffi::EeSparseArray;
use crate::astar::hull::EeHullPoint;
use crate::astar::sector_rec::{EeAStarSectorRecord, EeAStarSubSectorLink, EeAStarSubSectorRecord};

/// Each record's offset in its object (bytes from the object's address; the area kinds' from the derived class's,
/// which is also its `EEAIArea*`), registered once by the C++ (`EEAIContinentManager`'s ctor).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarCmLayout {
    /// `EeAStarAreaRecord` in `EEAIArea`
    pub area_record: usize,
    /// the `EeSparseArray*` of `EEAIArea::mTiles`
    pub area_tiles: usize,
    pub continent: usize,
    pub path_area: usize,
    pub forest: usize,
    pub hill: usize,
    pub choke: usize,
    pub rect: usize,
    /// `EeAStarSectorRecord` in `EEAIPathSector`
    pub sector: usize,
    /// `EeAStarSubSectorRecord` in `EEAIPathSubSector`
    pub sub_sector: usize,
    /// the `EeSparseArray*` of `EEAIPathSubSector::mTiles`
    pub sub_tiles: usize,
}

/// One host call's values.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarCmIo {
    pub h: [u64; 2],
    pub i: [i64; 4],
}

/// The host: `op` with `io`; the answer.
pub type EeAStarCmHostFn = unsafe extern "C" fn(ctx: *mut c_void, op: i32, io: *mut EeAStarCmIo) -> i64;

/// `h0` an `EEAIArea*`, `i0`/`i1` a tile: `Contains` (1/0)
pub const EE_ASTAR_CM_CONTAINS: i32 = 1;
/// `h0` an `EEAIArea*` of no table: a town (`i0` 7, `i1` its town id, `i2` its player) or a herd (8), else `i0` 0
pub const EE_ASTAR_CM_OWNER: i32 = 2;
/// `gEEAIRegionSize`
pub const EE_ASTAR_CM_REGION_SIZE: i32 = 3;
/// `gEEAIRegionSize = i0`
pub const EE_ASTAR_CM_SET_REGION_SIZE: i32 = 4;
/// the map's size, -1 without a map
pub const EE_ASTAR_CM_MAP_SIZE: i32 = 5;
/// tile (`i0`, `i1`)'s `mAIValue`, 0 off the map
pub const EE_ASTAR_CM_AI_VALUE: i32 = 6;
/// tile (`i0`, `i1`)'s `mAIValue = i2` (nothing off the map)
pub const EE_ASTAR_CM_SET_AI_VALUE: i32 = 7;
/// a new object of table `i0`'s class (`EeAStarCmTable`) -> its address
pub const EE_ASTAR_CM_NEW: i32 = 8;
/// `delete` object `h0` of table `i0`'s class
pub const EE_ASTAR_CM_DELETE: i32 = 9;

static LAYOUT: AtomicPtr<EeAStarCmLayout> = AtomicPtr::new(std::ptr::null_mut());
static HOST_FN: AtomicUsize = AtomicUsize::new(0);
static HOST_CTX: AtomicPtr<c_void> = AtomicPtr::new(std::ptr::null_mut());

/// The layout of the records in their objects (once; later calls replace it).
///
/// # Safety
/// `layout` points to a layout.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_set_layout(layout: *const EeAStarCmLayout) {
    let boxed = Box::into_raw(Box::new(*layout));
    let old = LAYOUT.swap(boxed, Ordering::AcqRel);
    if !old.is_null() {
        drop(Box::from_raw(old));
    }
}

/// The host the state's readers and writers call.
#[no_mangle]
pub extern "C" fn ee_astar_cm_set_host(f: EeAStarCmHostFn, ctx: *mut c_void) {
    HOST_CTX.store(ctx, Ordering::Release);
    HOST_FN.store(f as usize, Ordering::Release);
}

fn layout() -> EeAStarCmLayout {
    let p = LAYOUT.load(Ordering::Acquire);
    // SAFETY: a leaked box replaced only by `ee_astar_cm_set_layout`
    if p.is_null() {
        EeAStarCmLayout::default()
    } else {
        unsafe { *p }
    }
}

fn host(op: i32, io: &mut EeAStarCmIo) -> i64 {
    let f = HOST_FN.load(Ordering::Acquire);
    if f == 0 {
        return 0;
    }
    // SAFETY: stored from an `EeAStarCmHostFn` by `ee_astar_cm_set_host`
    let f: EeAStarCmHostFn = unsafe { std::mem::transmute::<usize, EeAStarCmHostFn>(f) };
    unsafe { f(HOST_CTX.load(Ordering::Acquire), op, io) }
}

fn host1(op: i32, h: u64, i: [i64; 4]) -> (i64, EeAStarCmIo) {
    let mut io = EeAStarCmIo { h: [h, 0], i };
    let r = host(op, &mut io);
    (r, io)
}

// ---- the records of an object (unsafe: `h` a live object of that class)

unsafe fn rec<'a, T>(h: u64, offset: usize) -> &'a mut T {
    &mut *((h as usize + offset) as *mut T)
}

pub(crate) unsafe fn area<'a>(h: u64) -> &'a mut EeAStarAreaRecord {
    rec(h, layout().area_record)
}

pub(crate) unsafe fn area_tiles<'a>(h: u64) -> &'a mut EeSparseArray {
    &mut **rec::<*mut EeSparseArray>(h, layout().area_tiles)
}

pub(crate) unsafe fn continent<'a>(h: u64) -> &'a mut EeAStarContinentRecord {
    rec(h, layout().continent)
}

pub(crate) unsafe fn path_area<'a>(h: u64) -> &'a mut EeAStarPathAreaRecord {
    rec(h, layout().path_area)
}

pub(crate) unsafe fn forest<'a>(h: u64) -> &'a mut EeAStarForestRecord {
    rec(h, layout().forest)
}

pub(crate) unsafe fn hill<'a>(h: u64) -> &'a mut EeAStarHillRecord {
    rec(h, layout().hill)
}

pub(crate) unsafe fn choke<'a>(h: u64) -> &'a mut EeAStarChokeRecord {
    rec(h, layout().choke)
}

pub(crate) unsafe fn rect<'a>(h: u64) -> &'a mut EeAStarRectRecord {
    rec(h, layout().rect)
}

pub(crate) unsafe fn sector<'a>(h: u64) -> &'a mut EeAStarSectorRecord {
    rec(h, layout().sector)
}

pub(crate) unsafe fn sub<'a>(h: u64) -> &'a mut EeAStarSubSectorRecord {
    rec(h, layout().sub_sector)
}

pub(crate) unsafe fn sub_tiles<'a>(h: u64) -> &'a mut EeSparseArray {
    &mut **rec::<*mut EeSparseArray>(h, layout().sub_tiles)
}

/// `EEAIArea::Contains` (the C++'s: its hull test is captured).
pub(crate) fn contains(h: u64, x: c_long, y: c_long) -> bool {
    host1(EE_ASTAR_CM_CONTAINS, h, [x as i64, y as i64, 0, 0]).0 != 0
}

fn table(r: &EeAStarCmRecord, t: EeAStarCmTable) -> &[u64] {
    &r.tables().tables[t as usize]
}

// ---- the tile lookups

/// `FindContinent`: the continent among the tile's sector's neighbours (from the last) that contains it, 0 if none.
pub fn find_continent(r: &EeAStarCmRecord, x: c_long, y: c_long) -> u64 {
    let Some(index) = r.sector_index(x, y) else { return 0 };
    let Some(&s) = table(r, EeAStarCmTable::PathSectors).get(index) else { return 0 };
    if s == 0 {
        return 0;
    }
    // SAFETY: a listed sector is a live object
    let neighbors = unsafe { sector(s).lists.as_ref() }.map(|l| l.neighbors.clone()).unwrap_or_default();
    for &c in neighbors.iter().rev() {
        if c != 0 && contains(c, x, y) {
            return c;
        }
    }
    0
}

/// The inline `GetContinentID(x, y)`: the grid's id, or `FindContinentID` without the grid; 0 off the map.
pub fn continent_id_at(r: &EeAStarCmRecord, x: c_long, y: c_long) -> u16 {
    match r.grid_continent_id(x, y) {
        Some(id) => id,
        None => match find_continent(r, x, y) {
            0 => 0,
            // SAFETY: a live continent
            c => unsafe { continent(c).id },
        },
    }
}

/// `FindPathSubSector`: the first of the tile's sector's sub sectors (in id order) whose tiles hold it, 0 if none.
pub fn find_sub_sector(r: &EeAStarCmRecord, x: c_long, y: c_long) -> u64 {
    let Some(index) = r.sector_index(x, y) else { return 0 };
    let Some(&s) = table(r, EeAStarCmTable::PathSectors).get(index) else { return 0 };
    if s == 0 {
        return 0;
    }
    // SAFETY: a listed sector and its sub sectors are live objects
    unsafe {
        let Some(lists) = sector(s).lists.as_ref() else { return 0 };
        for entry in &lists.sub_sectors {
            if sub_tiles(entry.sub).set.contains(EePoint { x: x as i32, y: y as i32 }) {
                return entry.sub;
            }
        }
    }
    0
}

/// The inline `GetPathSubSectorID(x, y)`: the grid's id, or `FindPathSubSectorID` without it; 0 off the map.
pub fn sub_sector_id_at(r: &EeAStarCmRecord, x: c_long, y: c_long) -> u16 {
    if !((x as c_ulong) < r.width && (y as c_ulong) < r.height) {
        return 0;
    }
    if let Some(g) = r.tables().sub_sector_ids.as_ref() {
        let at = r.width.wrapping_mul(x as c_ulong).wrapping_add(y as c_ulong) as usize;
        return g.get(at).copied().unwrap_or(0);
    }
    match find_sub_sector(r, x, y) {
        0 => 0,
        // SAFETY: a live sub sector
        s => unsafe { sub(s).id },
    }
}

// ---- the writers

/// The save block's encoding (`Writer` of EEAIContinentSave.cpp) or the capture's (every value 4 bytes).
struct Out {
    bytes: Vec<u8>,
    capture: bool,
}

impl Out {
    fn u32(&mut self, v: u32) {
        self.bytes.extend_from_slice(&v.to_le_bytes());
    }
    fn i32(&mut self, v: i32) {
        self.u32(v as u32);
    }
    fn long(&mut self, v: c_long) {
        self.i32(v as i32);
    }
    fn u16(&mut self, v: u16) {
        self.u32(v as u32);
    }
    fn flag(&mut self, v: bool) {
        if self.capture {
            self.i32(v as i32);
        } else {
            self.bytes.push(v as u8);
        }
    }
    fn point(&mut self, p: EeHullPoint) {
        self.long(p.x);
        self.long(p.y);
    }
    fn rect(&mut self, r: &EeHullRect) {
        self.long(r.bottom);
        self.long(r.left);
        self.long(r.right);
        self.long(r.top);
    }
    fn count(&mut self, n: usize) {
        if self.capture {
            self.i32(n as i32);
        } else {
            self.u32(n as u32);
        }
    }
    fn points(&mut self, ps: &[EeHullPoint]) {
        self.count(ps.len());
        for &p in ps {
            self.point(p);
        }
    }
    fn shorts(&mut self, vs: &[u16]) {
        self.count(vs.len());
        for &v in vs {
            self.u16(v);
        }
    }
    fn longs(&mut self, vs: &[c_long]) {
        self.count(vs.len());
        for &v in vs {
            self.long(v);
        }
    }
    fn ulongs(&mut self, vs: &[c_ulong]) {
        self.count(vs.len());
        for &v in vs {
            self.u32(v as u32);
        }
    }
}

/// `WriteSparse`: the set as `GetRaw` gives it.
fn write_sparse(o: &mut Out, s: &EeSparseArray) {
    let mut raw = Vec::new();
    s.set.raw(&mut raw);
    let mut at = 0usize;
    for _ in 0..2 {
        if at < raw.len() {
            o.i32(raw[at]);
            at += 1;
        }
    }
    let columns = if at < raw.len() {
        at += 1;
        raw[at - 1] as u32
    } else {
        0
    };
    o.u32(columns);
    let mut c = 0u32;
    while c < columns && at < raw.len() {
        let present = raw[at] != 0;
        at += 1;
        o.flag(present);
        c += 1;
        if !present || at + 1 >= raw.len() {
            continue;
        }
        o.i32(raw[at]);
        at += 1;
        let runs = raw[at] as u32;
        at += 1;
        o.u32(runs);
        let mut k = 0u32;
        while k < runs.wrapping_mul(2) && at < raw.len() {
            o.i32(raw[at]);
            at += 1;
            k += 1;
        }
    }
}

/// `WriteArea`: serial, type, tiles, hull, bounds.
fn write_area(o: &mut Out, h: u64) {
    // SAFETY: a live area
    let (a, tiles) = unsafe { (area(h), area_tiles(h)) };
    o.u32(a.serial as u32);
    o.long(a.area_type);
    write_sparse(o, tiles);
    o.points(a.hull_points());
    o.rect(&a.hull_bounds);
    o.point(a.hull_center);
    o.bytes.extend_from_slice(&a.hull_area.to_ne_bytes());
    o.rect(&a.bounds);
}

/// `ContinentIndex`: the first index of `c` in the continents, -1 for none or not listed.
fn continent_index(r: &EeAStarCmRecord, c: u64) -> i32 {
    if c == 0 {
        return -1;
    }
    table(r, EeAStarCmTable::Continents).iter().position(|&x| x == c).map_or(-1, |i| i as i32)
}

fn write_continents(o: &mut Out, r: &EeAStarCmRecord, cs: &[u64]) {
    o.u32(cs.len() as u32);
    for &c in cs {
        o.i32(continent_index(r, c));
    }
}

fn write_grid(o: &mut Out, bytes: Option<&[u8]>, len: usize) {
    o.flag(bytes.is_some());
    if let Some(b) = bytes {
        o.u32(len as u32);
        for k in 0..len {
            o.bytes.push(b.get(k).copied().unwrap_or(0));
        }
    }
}

fn u16_bytes(g: &[u16]) -> Vec<u8> {
    g.iter().flat_map(|v| v.to_ne_bytes()).collect()
}

/// `AreaOwner`: the table and index of an area of the serial table, or the C++'s town or herd.
fn area_owner(r: &EeAStarCmRecord, h: u64) -> (i32, i32, i32) {
    if h == 0 {
        return (0, -1, -1);
    }
    let order = [
        (EeAStarCmTable::Continents, 1),
        (EeAStarCmTable::Hills, 2),
        (EeAStarCmTable::PathAreas, 3),
        (EeAStarCmTable::Forests, 4),
        (EeAStarCmTable::ChokePoints, 5),
        (EeAStarCmTable::RectangleAreas, 6),
    ];
    for (t, owner) in order {
        if let Some(i) = table(r, t).iter().position(|&x| x == h) {
            return (owner, i as i32, -1);
        }
    }
    let (_, io) = host1(EE_ASTAR_CM_OWNER, h, [0, -1, -1, 0]);
    (io.i[0] as i32, io.i[1] as i32, io.i[2] as i32)
}

const REGIONS_MAGIC: u32 = 0x4752_4d43; // "CMRG"
const REGIONS_FORMAT: u32 = 1;

/// `WriteRegions`: the save block.
pub fn write_regions(r: &EeAStarCmRecord) -> Vec<u8> {
    let mut o = Out { bytes: Vec::new(), capture: false };
    let t = r.tables();
    o.u32(REGIONS_MAGIC);
    o.u32(REGIONS_FORMAT);
    o.u32(r.width as u32);
    o.u32(r.height as u32);
    o.long(r.state);
    o.u32(r.last_serial as u32);
    o.long(r.l118);
    o.u16(r.next_path_area_id);
    o.u16(r.last_sub_sector_id);
    o.long(r.checksum);
    for v in [r.next_land_mass, r.next_water_body, r.next_space_region, r.next_surface_region] {
        o.long(v);
    }
    for v in [r.num_water_tiles, r.num_land_tiles, r.num_space_tiles, r.flags_width] {
        o.long(v);
    }
    o.flag(r.clearing);
    for v in r.next_ids {
        o.u16(v);
    }
    for v in [r.sector_size, r.sector_area, r.sectors_x, r.sectors_y] {
        o.long(v);
    }
    o.i32(host1(EE_ASTAR_CM_REGION_SIZE, 0, [0; 4]).0 as i32);
    // the grids
    let tiles = (r.width as usize).wrapping_mul(r.height as usize);
    o.flag(t.tile_values.is_some());
    if let Some(g) = t.tile_values.as_ref() {
        for k in 0..tiles {
            o.long(g.get(k).copied().unwrap_or(0));
        }
    }
    let ids = t.continent_ids.as_ref().map(|g| u16_bytes(g));
    write_grid(&mut o, ids.as_deref(), tiles * 2);
    let subs = t.sub_sector_ids.as_ref().map(|g| u16_bytes(g));
    write_grid(&mut o, subs.as_deref(), tiles * 2);
    let flags = t.tile_flags.as_ref().map(|g| g.iter().map(|&b| b as u8).collect::<Vec<u8>>());
    let fw = r.flags_width as usize;
    write_grid(&mut o, flags.as_deref(), fw.wrapping_mul(fw));
    // the map tiles' AI values
    for x in 0..r.width as i64 {
        for y in 0..r.height as i64 {
            let v = host1(EE_ASTAR_CM_AI_VALUE, 0, [x, y, 0, 0]).0 as i8;
            o.bytes.push(v as u8);
        }
    }
    // the continents
    let continents = table(r, EeAStarCmTable::Continents);
    o.u32(continents.len() as u32);
    for &c in continents {
        o.flag(c != 0);
        if c == 0 {
            continue;
        }
        write_area(&mut o, c);
        // SAFETY: a live continent
        let rec = unsafe { continent(c) };
        let lists = rec.lists();
        o.points(lists.map_or(&[][..], |l| l.shore()));
        o.longs(lists.map_or(&[][..], |l| l.choke_points()));
        for k in 0..EE_ASTAR_CONTINENT_LISTS {
            o.shorts(lists.map_or(&[][..], |l| l.ids(continent_list(k))));
        }
        write_continents(&mut o, r, lists.map_or(&[][..], |l| l.neighbors()));
        for v in rec.counts {
            o.u16(v);
        }
        o.u32(rec.category as u32);
        o.u16(rec.id);
    }
    // the hills, path areas, forests, choke points, rectangle areas
    for (tbl, kind) in [
        (EeAStarCmTable::Hills, 0),
        (EeAStarCmTable::PathAreas, 1),
        (EeAStarCmTable::Forests, 2),
        (EeAStarCmTable::ChokePoints, 3),
        (EeAStarCmTable::RectangleAreas, 4),
    ] {
        let objects = table(r, tbl);
        o.u32(objects.len() as u32);
        for &h in objects {
            o.flag(h != 0);
            if h == 0 {
                continue;
            }
            write_area(&mut o, h);
            // SAFETY: a live object of the table's class
            unsafe { write_fields(&mut o, h, kind) };
        }
    }
    // the sub sectors: every one listed by id or held by a sector, numbered from 1 in that order
    let mut numbered: Vec<u64> = Vec::new();
    let mut seen = std::collections::BTreeMap::new();
    for &s in table(r, EeAStarCmTable::PathSubSectors) {
        if s != 0 && !seen.contains_key(&s) {
            numbered.push(s);
            seen.insert(s, numbered.len() as u32);
        }
    }
    let sectors = table(r, EeAStarCmTable::PathSectors);
    for &s in sectors {
        if s == 0 {
            continue;
        }
        // SAFETY: a live sector
        if let Some(l) = unsafe { sector(s).lists.as_ref() } {
            for e in &l.sub_sectors {
                if e.sub != 0 && !seen.contains_key(&e.sub) {
                    numbered.push(e.sub);
                    seen.insert(e.sub, numbered.len() as u32);
                }
            }
        }
    }
    let number = |h: u64| -> u32 { seen.get(&h).copied().unwrap_or(0) };
    o.u32(sectors.len() as u32);
    for &s in sectors {
        o.flag(s != 0);
        if s == 0 {
            continue;
        }
        // SAFETY: a live sector
        let rec = unsafe { sector(s) };
        o.u16(rec.id);
        o.rect(&rec.bounds);
        o.point(rec.center);
        // SAFETY: the record's own lists
        let lists = unsafe { rec.lists.as_ref() };
        write_continents(&mut o, r, lists.map_or(&[][..], |l| l.neighbors.as_slice()));
        o.shorts(lists.map_or(&[][..], |l| l.path_area_ids.as_slice()));
        let held = lists.map_or(&[][..], |l| l.sub_sectors.as_slice());
        o.u32(held.len() as u32);
        for e in held {
            o.u16(e.id);
            o.u32(number(e.sub));
        }
    }
    o.u32(numbered.len() as u32);
    for &s in &numbered {
        // SAFETY: a live sub sector
        let (rec, tiles) = unsafe { (sub(s), sub_tiles(s)) };
        let at =
            if rec.sector == 0 { -1 } else { sectors.iter().position(|&x| x == rec.sector).map_or(-1, |i| i as i32) };
        o.i32(at);
        write_sparse(&mut o, tiles);
        // SAFETY: the record's own lists
        let lists = unsafe { rec.lists.as_ref() };
        let neighbors = lists.map_or(&[][..], |l| l.neighbors.as_slice());
        o.u32(neighbors.len() as u32);
        for &n in neighbors {
            o.u32(number(n));
        }
        let links = lists.map_or(&[][..], |l| l.links.as_slice());
        o.u32(links.len() as u32);
        for l in links {
            o.u16(l.id);
            o.u32(number(l.first));
            o.u32(number(l.second));
        }
        o.point(rec.location);
        o.long(rec.num_tiles);
        o.u16(rec.continent_id);
        o.u16(rec.path_area_id);
        o.u16(rec.id);
        o.long(rec.kind);
        o.long(rec.player_id);
        o.flag(rec.is_stale);
    }
    let subs = table(r, EeAStarCmTable::PathSubSectors);
    o.u32(subs.len() as u32);
    for &s in subs {
        o.u32(number(s));
    }
    // the shore tiles, the free ids, the sectors' pending tiles
    o.points(&t.shore[EeAStarCmShore::Shore as usize]);
    o.points(&t.shore[EeAStarCmShore::IceShore as usize]);
    o.shorts(&t.free[EeAStarCmFree::SubSectors as usize]);
    o.shorts(&t.free[EeAStarCmFree::PathAreas as usize]);
    o.u32(t.sector_tiles.len() as u32);
    for s in &t.sector_tiles {
        o.u16(s.id);
        o.points(&s.tiles);
    }
    // the serial table
    o.u32(t.areas.len() as u32);
    for a in &t.areas {
        let (owner, index, player) = area_owner(r, a.area);
        o.u32(a.serial as u32);
        o.i32(owner);
        o.i32(index);
        o.i32(player);
        // SAFETY: a live area
        o.u32(if a.area == 0 { 0 } else { unsafe { area(a.area).serial as u32 } });
    }
    o.bytes
}

fn continent_list(k: usize) -> EeAStarContinentList {
    [
        EeAStarContinentList::Forests,
        EeAStarContinentList::PathAreas,
        EeAStarContinentList::Hills,
        EeAStarContinentList::IdsBc,
        EeAStarContinentList::IdsCc,
        EeAStarContinentList::IdsDc,
        EeAStarContinentList::IdsEc,
    ][k]
}

/// The kinds' `WriteFields` (0 hill, 1 path area, 2 forest, 3 choke point, 4 rectangle area).
unsafe fn write_fields(o: &mut Out, h: u64, kind: i32) {
    match kind {
        0 => {
            let r = hill(h);
            o.u16(r.continent_id);
            o.u16(r.id);
        }
        1 => {
            let r = path_area(h);
            o.shorts(r.sector_ids());
            o.u16(r.continent_id);
            o.u16(r.id);
            o.long(r.kind);
            o.long(r.unit_id);
            for v in [r.surface_region, r.water_body, r.land_mass, r.space_region] {
                o.u16(v);
            }
        }
        2 => {
            let r = forest(h);
            for v in [r.resource_kind, r.num_resources1, r.num_resources2, r.num_resources3, r.l78] {
                o.long(v);
            }
            o.u32(r.next_renewal as u32);
            o.u16(r.continent_id);
            o.u16(r.id);
        }
        3 => {
            let r = choke(h);
            o.ulongs(&(*r.area_serials).0);
            o.long(r.index);
            o.u16(r.continent_id);
        }
        _ => {
            let r = rect(h);
            o.rect(&r.rect);
            for v in [r.scenario_area_id, r.num_land_tiles, r.num_water_tiles, r.num_space_tiles] {
                o.long(v);
            }
            o.u16(r.continent_id);
            o.u16(r.id);
        }
    }
}

/// `EEContinentRegionsDigest`: the block's FNV-1a in an `unsigned long` (32 bits on Windows, 64 on LP64).
pub fn regions_digest(r: &EeAStarCmRecord) -> c_ulong {
    let mut hash: c_ulong = 2_166_136_261;
    for b in write_regions(r) {
        hash = (hash ^ b as c_ulong).wrapping_mul(16_777_619);
    }
    hash
}

// ---- the capture (TEST ONLY: CNTR, CNTQ's digest)

fn cap_tiles(o: &mut Out, s: &EeSparseArray) {
    let n = s.set.size();
    o.i32(n);
    for i in 0..n.max(0) {
        let p = s.set.get(i);
        o.i32(p.x);
        o.i32(p.y);
    }
}

fn cap_area(o: &mut Out, h: u64) {
    // SAFETY: a live area
    let (a, tiles) = unsafe { (area(h), area_tiles(h)) };
    o.u32(a.serial as u32);
    o.long(a.area_type);
    o.rect(&a.bounds);
    o.point(a.hull_center);
    cap_tiles(o, tiles);
}

fn cap_ids(o: &mut Out, objects: &[u64], id: impl Fn(u64) -> u16) {
    o.i32(objects.len() as i32);
    for &h in objects {
        o.i32(if h == 0 { -1 } else { id(h) as i32 });
    }
}

/// `EEAIContinentFill`: the CNTR record's bytes.
pub fn capture_fill(r: &EeAStarCmRecord) -> Vec<u8> {
    let mut o = Out { bytes: Vec::new(), capture: true };
    let t = r.tables();
    o.u32(r.width as u32);
    o.u32(r.height as u32);
    o.long(r.state);
    o.u32(r.last_serial as u32);
    o.u32(r.next_path_area_id as u32);
    o.u32(r.last_sub_sector_id as u32);
    for v in [r.num_water_tiles, r.num_land_tiles, r.num_space_tiles, r.flags_width] {
        o.long(v);
    }
    for v in [r.next_land_mass, r.next_water_body, r.next_space_region, r.next_surface_region, r.checksum] {
        o.long(v);
    }
    o.i32(r.clearing as i32);
    for v in r.next_ids {
        o.u32(v as u32);
    }
    o.i32(t.continent_ids.is_some() as i32);
    o.i32(t.sub_sector_ids.is_some() as i32);
    o.i32(t.tile_values.is_some() as i32);
    o.i32(t.tile_flags.is_some() as i32);
    for x in 0..r.width as c_long {
        for y in 0..r.height as c_long {
            o.u32(continent_id_at(r, x, y) as u32);
            o.u32(sub_sector_id_at(r, x, y) as u32);
            let at = (x as c_ulong).wrapping_mul(r.width).wrapping_add(y as c_ulong) as usize;
            o.long(t.tile_values.as_ref().map_or(0, |g| g.get(at).copied().unwrap_or(0)));
        }
    }
    match t.tile_flags.as_ref() {
        Some(f) => {
            let n = r.flags_width.wrapping_mul(r.flags_width);
            o.i32(n as i32);
            for k in 0..n.max(0) as usize {
                o.i32(f.get(k).copied().unwrap_or(false) as i32);
            }
        }
        None => o.i32(0),
    }
    // SAFETY (the closures): the ids of live objects
    let continent_id = |h: u64| unsafe { continent(h).id };
    let sub_id = |h: u64| unsafe { sub(h).id };
    let continents = table(r, EeAStarCmTable::Continents);
    o.i32(continents.len() as i32);
    for &c in continents {
        o.i32((c != 0) as i32);
        if c == 0 {
            continue;
        }
        cap_area(&mut o, c);
        // SAFETY: a live continent
        let rec = unsafe { continent(c) };
        o.u32(rec.id as u32);
        o.i32(rec.category as i32);
        let lists = rec.lists();
        o.points(lists.map_or(&[][..], |l| l.shore()));
        o.longs(lists.map_or(&[][..], |l| l.choke_points()));
        for k in 0..EE_ASTAR_CONTINENT_LISTS {
            o.shorts(lists.map_or(&[][..], |l| l.ids(continent_list(k))));
        }
        cap_ids(&mut o, lists.map_or(&[][..], |l| l.neighbors()), continent_id);
        for v in rec.counts {
            o.u32(v as u32);
        }
    }
    let areas = table(r, EeAStarCmTable::PathAreas);
    o.i32(areas.len() as i32);
    for &h in areas {
        o.i32((h != 0) as i32);
        if h == 0 {
            continue;
        }
        cap_area(&mut o, h);
        // SAFETY: a live path area
        let p = unsafe { path_area(h) };
        o.u32(p.id as u32);
        o.u32(p.continent_id as u32);
        o.long(p.kind);
        o.long(p.unit_id);
        for v in [p.surface_region, p.water_body, p.land_mass, p.space_region] {
            o.u32(v as u32);
        }
        o.shorts(p.sector_ids());
    }
    let sectors = table(r, EeAStarCmTable::PathSectors);
    o.i32(sectors.len() as i32);
    for &s in sectors {
        o.i32((s != 0) as i32);
        if s == 0 {
            continue;
        }
        // SAFETY: a live sector and its own lists
        let rec = unsafe { sector(s) };
        o.u32(rec.id as u32);
        o.rect(&rec.bounds);
        o.point(rec.center);
        let lists = unsafe { rec.lists.as_ref() };
        cap_ids(&mut o, lists.map_or(&[][..], |l| l.neighbors.as_slice()), continent_id);
        o.shorts(lists.map_or(&[][..], |l| l.path_area_ids.as_slice()));
        let held = lists.map_or(&[][..], |l| l.sub_sectors.as_slice());
        o.i32(held.len() as i32);
        for e in held {
            o.u32(e.id as u32);
            o.i32(if e.sub == 0 { -1 } else { sub_id(e.sub) as i32 });
        }
    }
    let subs = table(r, EeAStarCmTable::PathSubSectors);
    o.i32(subs.len() as i32);
    for &s in subs {
        o.i32((s != 0) as i32);
        if s == 0 {
            continue;
        }
        // SAFETY: a live sub sector and its own lists
        let (rec, tiles) = unsafe { (sub(s), sub_tiles(s)) };
        o.u32(rec.id as u32);
        o.long(rec.kind);
        o.point(rec.location);
        o.u32(rec.continent_id as u32);
        o.u32(rec.path_area_id as u32);
        o.long(rec.player_id);
        o.i32(rec.is_stale as i32);
        o.i32(if rec.sector == 0 { -1 } else { unsafe { sector(rec.sector).id as i32 } });
        cap_tiles(&mut o, tiles);
        let lists = unsafe { rec.lists.as_ref() };
        cap_ids(&mut o, lists.map_or(&[][..], |l| l.neighbors.as_slice()), sub_id);
        let links: &[EeAStarSubSectorLink] = lists.map_or(&[][..], |l| l.links.as_slice());
        o.i32(links.len() as i32);
        for l in links {
            o.u32(l.id as u32);
            o.i32(if l.first == 0 { -1 } else { sub_id(l.first) as i32 });
            o.i32(if l.second == 0 { -1 } else { sub_id(l.second) as i32 });
        }
    }
    let chokes = table(r, EeAStarCmTable::ChokePoints);
    o.i32(chokes.len() as i32);
    for &h in chokes {
        o.i32((h != 0) as i32);
        if h == 0 {
            continue;
        }
        cap_area(&mut o, h);
        // SAFETY: a live choke point
        let c = unsafe { choke(h) };
        o.long(c.index);
        o.u32(c.continent_id as u32);
        o.ulongs(unsafe { &(*c.area_serials).0 });
    }
    let hills = table(r, EeAStarCmTable::Hills);
    o.i32(hills.len() as i32);
    for &h in hills {
        o.i32((h != 0) as i32);
        if h == 0 {
            continue;
        }
        cap_area(&mut o, h);
        // SAFETY: a live hill
        let k = unsafe { hill(h) };
        o.u32(k.continent_id as u32);
        o.u32(k.id as u32);
    }
    let forests = table(r, EeAStarCmTable::Forests);
    o.i32(forests.len() as i32);
    for &h in forests {
        o.i32((h != 0) as i32);
        if h == 0 {
            continue;
        }
        cap_area(&mut o, h);
        // SAFETY: a live forest
        let f = unsafe { forest(h) };
        for v in [f.resource_kind, f.num_resources1, f.num_resources2, f.num_resources3] {
            o.long(v);
        }
        o.u32(f.next_renewal as u32);
        o.u32(f.continent_id as u32);
        o.u32(f.id as u32);
    }
    let rects = table(r, EeAStarCmTable::RectangleAreas);
    o.i32(rects.len() as i32);
    for &h in rects {
        o.i32((h != 0) as i32);
        if h != 0 {
            cap_area(&mut o, h);
        }
    }
    o.points(&t.shore[EeAStarCmShore::Shore as usize]);
    o.points(&t.shore[EeAStarCmShore::IceShore as usize]);
    o.shorts(&t.free[EeAStarCmFree::SubSectors as usize]);
    o.shorts(&t.free[EeAStarCmFree::PathAreas as usize]);
    o.i32(t.sector_tiles.len() as i32);
    for s in &t.sector_tiles {
        o.u32(s.id as u32);
        o.points(&s.tiles);
    }
    o.i32(t.areas.len() as i32);
    for a in &t.areas {
        o.u32(a.serial as u32);
        // SAFETY: a live area
        o.i32(if a.area == 0 { -1 } else { unsafe { area(a.area).area_type as i32 } });
    }
    o.bytes
}

/// `EEAIContinentDigest`: the FNV-1a 64 of the CNTR bytes; 0 before the regions exist (no sectors to divide by).
pub fn capture_digest(r: &EeAStarCmRecord) -> u64 {
    if table(r, EeAStarCmTable::PathSectors).is_empty() || r.sector_size == 0 {
        return 0;
    }
    let mut hash: u64 = 0xcbf2_9ce4_8422_2325;
    for b in capture_fill(r) {
        hash = (hash ^ b as u64).wrapping_mul(0x0000_0100_0000_01b3);
    }
    hash
}

// ---- the reader

struct In<'a> {
    bytes: &'a [u8],
    at: usize,
}

impl In<'_> {
    fn take(&mut self, n: usize) -> Option<&[u8]> {
        if self.bytes.len() - self.at < n {
            return None;
        }
        let s = &self.bytes[self.at..self.at + n];
        self.at += n;
        Some(s)
    }
    fn u32(&mut self) -> Option<u32> {
        self.take(4).map(|b| u32::from_le_bytes([b[0], b[1], b[2], b[3]]))
    }
    fn i32(&mut self) -> Option<i32> {
        self.u32().map(|v| v as i32)
    }
    fn long(&mut self) -> Option<c_long> {
        self.i32().map(|v| v as c_long)
    }
    fn u16(&mut self) -> Option<u16> {
        self.u32().map(|v| v as u16)
    }
    fn flag(&mut self) -> Option<bool> {
        self.take(1).map(|b| b[0] != 0)
    }
    fn point(&mut self) -> Option<EeHullPoint> {
        Some(EeHullPoint { x: self.long()?, y: self.long()? })
    }
    fn rect(&mut self) -> Option<EeHullRect> {
        Some(EeHullRect { bottom: self.long()?, left: self.long()?, right: self.long()?, top: self.long()? })
    }
    /// `Count`: a count no larger than the bytes left (every element takes at least one).
    fn count(&mut self) -> Option<usize> {
        let n = self.u32()? as usize;
        (n <= self.bytes.len() - self.at).then_some(n)
    }
    fn points(&mut self) -> Option<Vec<EeHullPoint>> {
        let n = self.count()?;
        (0..n).map(|_| self.point()).collect()
    }
    fn shorts(&mut self) -> Option<Vec<u16>> {
        let n = self.count()?;
        (0..n).map(|_| self.u16()).collect()
    }
    fn longs(&mut self) -> Option<Vec<c_long>> {
        let n = self.count()?;
        (0..n).map(|_| self.long()).collect()
    }
    fn ulongs(&mut self) -> Option<Vec<c_ulong>> {
        let n = self.count()?;
        (0..n).map(|_| self.u32().map(|v| v as c_ulong)).collect()
    }
}

fn read_sparse(i: &mut In, s: &mut EeSparseArray) -> Option<()> {
    let mut raw = vec![i.i32()?];
    let second = i.i32()?;
    let columns = i.count()?;
    raw.push(second);
    raw.push(columns as i32);
    for _ in 0..columns {
        let present = i.flag()?;
        raw.push(present as i32);
        if !present {
            continue;
        }
        let size = i.i32()?;
        let runs = i.count()?;
        raw.push(size);
        raw.push(runs as i32);
        for _ in 0..runs * 2 {
            raw.push(i.i32()?);
        }
    }
    s.set.set_raw(&raw).then_some(())
}

/// `ReadArea`: serial and type set once the rest was read.
unsafe fn read_area(i: &mut In, h: u64) -> Option<()> {
    let serial = i.u32()?;
    let kind = i.long()?;
    read_sparse(i, area_tiles(h))?;
    let points = i.points()?;
    let bounds = i.rect()?;
    let center = i.point()?;
    let b = i.take(4)?;
    let hull = f32::from_ne_bytes([b[0], b[1], b[2], b[3]]);
    let a = area(h);
    a.set_hull_raw(&points, bounds, center, hull);
    a.bounds = i.rect()?;
    a.serial = serial as c_ulong;
    a.area_type = kind;
    Some(())
}

unsafe fn read_fields(i: &mut In, h: u64, kind: i32) -> Option<()> {
    match kind {
        0 => {
            let r = hill(h);
            r.continent_id = i.u16()?;
            r.id = i.u16()?;
        }
        1 => {
            let ids = i.shorts()?;
            let r = path_area(h);
            (*r.sector_ids).0 = ids;
            r.continent_id = i.u16()?;
            r.id = i.u16()?;
            r.kind = i.long()?;
            r.unit_id = i.long()?;
            r.surface_region = i.u16()?;
            r.water_body = i.u16()?;
            r.land_mass = i.u16()?;
            r.space_region = i.u16()?;
        }
        2 => {
            let r = forest(h);
            r.resource_kind = i.long()?;
            r.num_resources1 = i.long()?;
            r.num_resources2 = i.long()?;
            r.num_resources3 = i.long()?;
            r.l78 = i.long()?;
            r.next_renewal = i.u32()? as c_ulong;
            r.continent_id = i.u16()?;
            r.id = i.u16()?;
        }
        3 => {
            let serials = i.ulongs()?;
            let r = choke(h);
            (*r.area_serials).0 = serials;
            r.index = i.long()?;
            r.continent_id = i.u16()?;
        }
        _ => {
            let r = rect(h);
            r.rect = i.rect()?;
            r.scenario_area_id = i.long()?;
            r.num_land_tiles = i.long()?;
            r.num_water_tiles = i.long()?;
            r.num_space_tiles = i.long()?;
            r.continent_id = i.u16()?;
            r.id = i.u16()?;
        }
    }
    Some(())
}

fn new_object(t: EeAStarCmTable) -> u64 {
    host1(EE_ASTAR_CM_NEW, 0, [t as i64, 0, 0, 0]).0 as u64
}

fn delete_object(t: EeAStarCmTable, h: u64) {
    if h != 0 {
        host1(EE_ASTAR_CM_DELETE, h, [t as i64, 0, 0, 0]);
    }
}

/// Deletes table `t`'s objects and empties it (`DeleteVectorContents`).
pub fn delete_table(r: &mut EeAStarCmRecord, t: EeAStarCmTable) {
    let objects = std::mem::take(&mut r.tables_mut().tables[t as usize]);
    for h in objects {
        delete_object(t, h);
    }
}

/// A table resized to `n`, the objects past it deleted (`KeepTable`).
fn keep_table(r: &mut EeAStarCmRecord, t: EeAStarCmTable, n: usize) {
    let extra: Vec<u64> = r.tables().tables[t as usize].iter().skip(n).copied().collect();
    for h in extra {
        delete_object(t, h);
    }
    r.tables_mut().tables[t as usize].resize(n, 0);
}

/// `ReadTable`: entries present or not; new objects, or with `keep` the table's own where it has one.
unsafe fn read_table(i: &mut In, r: &mut EeAStarCmRecord, t: EeAStarCmTable, kind: i32, keep: bool) -> Option<()> {
    let n = i.count()?;
    if keep {
        keep_table(r, t, n);
    } else {
        r.tables_mut().tables[t as usize] = vec![0; n];
    }
    for k in 0..n {
        let present = i.flag()?;
        if !present {
            let old = std::mem::take(&mut r.tables_mut().tables[t as usize][k]);
            delete_object(t, old);
            continue;
        }
        if r.tables().tables[t as usize][k] == 0 {
            r.tables_mut().tables[t as usize][k] = new_object(t);
        }
        let h = r.tables().tables[t as usize][k];
        read_area(i, h)?;
        read_fields(i, h, kind)?;
    }
    Some(())
}

/// What the C++ finishes a load with: the saved serial counter and clearing flag, the towns and herds the saved
/// serial table named (player, id, serial).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Loaded {
    pub last_serial: c_ulong,
    pub clearing: bool,
    pub towns: Vec<[i64; 3]>,
    pub herds: Vec<[i64; 3]>,
    pub unknown_owners: u32,
    pub unfiled_sub_sectors: u32,
}

/// `ReadRegions`: the manager's contents replaced by the block's; `None` on a bad block (the manager then half
/// made, as the C++'s), or for another map's.
///
/// # Safety
/// The tables' objects live; the layout and host registered.
#[allow(clippy::needless_range_loop, clippy::type_complexity)] // the C++'s index loops over parallel lists
pub unsafe fn read_regions(r: &mut EeAStarCmRecord, bytes: &[u8]) -> Option<Loaded> {
    let mut i = In { bytes, at: 0 };
    if i.u32()? != REGIONS_MAGIC || i.u32()? != REGIONS_FORMAT {
        return None;
    }
    // the regions BuildRegions would make go; the continents and hills Clear made are kept as objects
    r.tables_mut().sector_tiles.clear();
    delete_table(r, EeAStarCmTable::ChokePoints);
    delete_table(r, EeAStarCmTable::Forests);
    delete_table(r, EeAStarCmTable::RectangleAreas);
    delete_table(r, EeAStarCmTable::PathAreas);
    delete_table(r, EeAStarCmTable::PathSectors);
    r.last_sub_sector_id = 0;
    r.tables_mut().free[EeAStarCmFree::SubSectors as usize].clear();
    r.reset_sectors();
    r.tables_mut().tables[EeAStarCmTable::PathSubSectors as usize].clear();
    r.tables_mut().areas.clear();
    let mut loaded = Loaded::default();
    // the counters, read straight in (a short block leaves them half read, as the C++'s)
    let ok = (|| -> Option<()> {
        r.width = i.u32()? as c_ulong;
        r.height = i.u32()? as c_ulong;
        r.state = i.long()?;
        r.last_serial = i.u32()? as c_ulong;
        r.l118 = i.long()?;
        r.next_path_area_id = i.u16()?;
        r.last_sub_sector_id = i.u16()?;
        r.checksum = i.long()?;
        r.next_land_mass = i.long()?;
        r.next_water_body = i.long()?;
        r.next_space_region = i.long()?;
        r.next_surface_region = i.long()?;
        r.num_water_tiles = i.long()?;
        r.num_land_tiles = i.long()?;
        r.num_space_tiles = i.long()?;
        r.flags_width = i.long()?;
        loaded.clearing = i.flag()?;
        for k in 0..7 {
            r.next_ids[k] = i.u16()?;
        }
        r.sector_size = i.long()?;
        r.sector_area = i.long()?;
        r.sectors_x = i.long()?;
        r.sectors_y = i.long()?;
        Some(())
    })();
    let region = i.long();
    let map = host1(EE_ASTAR_CM_MAP_SIZE, 0, [0; 4]).0;
    ok?;
    let region = region?;
    if map < 0 || r.width != map as c_ulong || r.height != map as c_ulong {
        return None;
    }
    host1(EE_ASTAR_CM_SET_REGION_SIZE, 0, [region as i64, 0, 0, 0]);
    loaded.last_serial = r.last_serial;
    let tiles = (r.width as usize).wrapping_mul(r.height as usize);
    r.set_grid(EeAStarCmGrid::TileValues, None);
    if i.flag()? {
        r.set_grid(EeAStarCmGrid::TileValues, Some(tiles));
        for k in 0..tiles {
            let v = i.long()?;
            if let Some(g) = r.tables_mut().tile_values.as_mut() {
                g[k] = v;
            }
        }
    }
    for (grid, bytes) in [(EeAStarCmGrid::ContinentIds, tiles * 2), (EeAStarCmGrid::SubSectorIds, tiles * 2)] {
        r.set_grid(grid, None);
        if i.flag()? {
            if i.u32()? as usize != bytes {
                return None;
            }
            let data = i.take(bytes)?.to_vec();
            r.set_grid(grid, Some(bytes / 2));
            let t = r.tables_mut();
            let g =
                if grid == EeAStarCmGrid::ContinentIds { t.continent_ids.as_mut() } else { t.sub_sector_ids.as_mut() };
            if let Some(g) = g {
                for (k, v) in g.iter_mut().enumerate() {
                    *v = u16::from_ne_bytes([data[2 * k], data[2 * k + 1]]);
                }
            }
        }
    }
    let fw = (r.flags_width as usize).wrapping_mul(r.flags_width as usize);
    r.set_grid(EeAStarCmGrid::TileFlags, None);
    if i.flag()? {
        if i.u32()? as usize != fw {
            return None;
        }
        let data = i.take(fw)?.to_vec();
        r.set_grid(EeAStarCmGrid::TileFlags, Some(fw));
        if let Some(g) = r.tables_mut().tile_flags.as_mut() {
            for (k, v) in g.iter_mut().enumerate() {
                *v = data[k] != 0;
            }
        }
    }
    for x in 0..r.width as i64 {
        for y in 0..r.height as i64 {
            let v = i.take(1)?[0] as i8;
            host1(EE_ASTAR_CM_SET_AI_VALUE, 0, [x, y, v as i64, 0]);
        }
    }
    // the continents first (the other parts name them by index); their neighbours once all are made
    let n = i.count()?;
    keep_table(r, EeAStarCmTable::Continents, n);
    let mut neighbors: Vec<Vec<i32>> = vec![Vec::new(); n];
    for k in 0..n {
        if !i.flag()? {
            let old = std::mem::take(&mut r.tables_mut().tables[EeAStarCmTable::Continents as usize][k]);
            delete_object(EeAStarCmTable::Continents, old);
            continue;
        }
        if r.tables().tables[EeAStarCmTable::Continents as usize][k] == 0 {
            r.tables_mut().tables[EeAStarCmTable::Continents as usize][k] = new_object(EeAStarCmTable::Continents);
        }
        let c = r.tables().tables[EeAStarCmTable::Continents as usize][k];
        continent(c).lists_mut().neighbors_mut().clear();
        read_area(&mut i, c)?;
        let shore = i.points()?;
        let chokes = i.longs()?;
        let mut ids: Vec<Vec<u16>> = Vec::new();
        for _ in 0..EE_ASTAR_CONTINENT_LISTS {
            ids.push(i.shorts()?);
        }
        let lists: [&[u16]; EE_ASTAR_CONTINENT_LISTS] = std::array::from_fn(|q| ids[q].as_slice());
        continent(c).lists_mut().set(&shore, &chokes, lists);
        let count = i.count()?;
        for _ in 0..count {
            neighbors[k].push(i.i32()?);
        }
        for q in 0..7 {
            continent(c).counts[q] = i.u16()?;
        }
        let category = i.u32()?;
        let id = i.u16()?;
        continent(c).category = category as u8;
        continent(c).id = id;
    }
    let continents = r.tables().tables[EeAStarCmTable::Continents as usize].clone();
    for (k, list) in neighbors.iter().enumerate() {
        if continents[k] == 0 {
            continue;
        }
        for &idx in list {
            let h = if idx >= 0 && (idx as usize) < n { continents[idx as usize] } else { 0 };
            continent(continents[k]).lists_mut().neighbors_mut().push(h);
        }
    }
    read_table(&mut i, r, EeAStarCmTable::Hills, 0, true)?;
    read_table(&mut i, r, EeAStarCmTable::PathAreas, 1, false)?;
    read_table(&mut i, r, EeAStarCmTable::Forests, 2, false)?;
    read_table(&mut i, r, EeAStarCmTable::ChokePoints, 3, false)?;
    read_table(&mut i, r, EeAStarCmTable::RectangleAreas, 4, false)?;
    // the sectors: made first, their sub sectors filed once those are made
    let n = i.count()?;
    let mut sectors = vec![0u64; n];
    let mut held: Vec<Vec<(u16, u32)>> = vec![Vec::new(); n];
    for k in 0..n {
        if !i.flag()? {
            continue;
        }
        let s = new_object(EeAStarCmTable::PathSectors);
        sectors[k] = s;
        let rec = sector(s);
        rec.id = i.u16()?;
        rec.bounds = i.rect()?;
        rec.center = i.point()?;
        let count = i.count()?;
        for _ in 0..count {
            let idx = i.i32()?;
            let h = if idx >= 0 && (idx as usize) < continents.len() { continents[idx as usize] } else { 0 };
            (*rec.lists).neighbors.push(h);
        }
        (*rec.lists).path_area_ids = i.shorts()?;
        let count = i.count()?;
        for _ in 0..count {
            let key = i.u16()?;
            let number = i.u32()?;
            held[k].push((key, number));
        }
    }
    r.tables_mut().tables[EeAStarCmTable::PathSectors as usize] = sectors.clone();
    // the sub sectors (numbered from 1), then their neighbours and links
    let m = i.count()?;
    let mut subs = vec![0u64; m + 1];
    let mut links: Vec<(Vec<u32>, Vec<(u16, u32, u32)>)> = vec![(Vec::new(), Vec::new()); m + 1];
    for k in 1..=m {
        let s = new_object(EeAStarCmTable::PathSubSectors);
        subs[k] = s;
        let at = i.i32()?;
        read_sparse(&mut i, sub_tiles(s))?;
        let count = i.count()?;
        sub(s).sector = if at >= 0 && (at as usize) < sectors.len() { sectors[at as usize] } else { 0 };
        for _ in 0..count {
            links[k].0.push(i.u32()?);
        }
        let count = i.count()?;
        for _ in 0..count {
            let key = i.u16()?;
            let first = i.u32()?;
            let second = i.u32()?;
            links[k].1.push((key, first, second));
        }
        let rec = sub(s);
        rec.location = i.point()?;
        rec.num_tiles = i.long()?;
        rec.continent_id = i.u16()?;
        rec.path_area_id = i.u16()?;
        rec.id = i.u16()?;
        rec.kind = i.long()?;
        rec.player_id = i.long()?;
        rec.is_stale = i.flag()?;
    }
    let by_number = |number: u32| -> u64 { subs.get(number as usize).copied().unwrap_or(0) };
    let mut filed = vec![false; m + 1];
    for (s, list) in held.iter().enumerate() {
        for &(key, number) in list {
            crate::astar::sector_rec::EeAStarSectorLists::insert_sub(
                &mut *sector(sectors[s]).lists,
                key,
                by_number(number),
            );
            if (number as usize) <= m {
                filed[number as usize] = true;
            }
        }
    }
    for k in 1..=m {
        let lists = &mut *sub(subs[k]).lists;
        for &number in &links[k].0 {
            lists.neighbors.push(by_number(number));
        }
        for &(key, first, second) in &links[k].1 {
            lists.insert_link(EeAStarSubSectorLink { id: key, first: by_number(first), second: by_number(second) });
        }
    }
    let n = i.count()?;
    let mut table_subs = vec![0u64; n];
    for entry in table_subs.iter_mut() {
        *entry = by_number(i.u32()?);
    }
    r.tables_mut().tables[EeAStarCmTable::PathSubSectors as usize] = table_subs;
    loaded.unfiled_sub_sectors = filed.iter().skip(1).filter(|&&f| !f).count() as u32;
    // the shore tiles, the free ids, the sectors' pending tiles
    let shore = i.points()?;
    let ice = i.points()?;
    let free_subs = i.shorts()?;
    let free_areas = i.shorts()?;
    let n = i.count()?;
    {
        let t = r.tables_mut();
        t.shore[EeAStarCmShore::Shore as usize] = shore;
        t.shore[EeAStarCmShore::IceShore as usize] = ice;
        t.free[EeAStarCmFree::SubSectors as usize] = free_subs;
        t.free[EeAStarCmFree::PathAreas as usize] = free_areas;
    }
    for _ in 0..n {
        let key = i.u16()?;
        let tiles = i.points()?;
        let t = r.tables_mut();
        t.remove_sector_tiles(key);
        for p in tiles {
            t.add_sector_tile(key, p);
        }
        if t.sector_tiles.iter().all(|s| s.id != key) {
            // `mSectorTiles[key] = tiles` with no tiles keeps an empty entry
            let at = t.sector_tiles.partition_point(|s| s.id < key);
            t.sector_tiles.insert(at, crate::astar::cm::SectorTiles { id: key, tiles: Vec::new() });
        }
    }
    // the serial table: the manager's own areas now, the herds and towns returned to the C++
    let n = i.count()?;
    for _ in 0..n {
        let serial = i.u32()? as c_ulong;
        let owner = i.i32()?;
        let index = i.i32()?;
        let player = i.i32()?;
        let _ = i.u32()?;
        let at = index as usize;
        let from = |t: EeAStarCmTable| r.tables().tables[t as usize].get(at).copied().unwrap_or(0);
        let h = match owner {
            1 => from(EeAStarCmTable::Continents),
            2 => from(EeAStarCmTable::Hills),
            3 => from(EeAStarCmTable::PathAreas),
            4 => from(EeAStarCmTable::Forests),
            5 => from(EeAStarCmTable::ChokePoints),
            6 => from(EeAStarCmTable::RectangleAreas),
            7 => {
                loaded.towns.push([player as i64, index as i64, serial as i64]);
                0
            }
            8 => {
                loaded.herds.push([player as i64, index as i64, serial as i64]);
                0
            }
            _ => {
                loaded.unknown_owners += 1;
                0
            }
        };
        if h != 0 {
            r.tables_mut().add_area(serial, h);
        }
    }
    (i.at == bytes.len()).then_some(loaded)
}

// ---- the C interface

/// A byte block (opaque to C++).
pub struct EeAStarCmBytes(Vec<u8>);

/// A load's leftovers (opaque to C++).
pub struct EeAStarCmLoad(Loaded);

/// A town or herd the saved serial table named.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarCmOwned {
    pub player: i64,
    pub index: i64,
    pub serial: i64,
}

/// A load's counters.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarCmLoadInfo {
    pub last_serial: c_ulong,
    pub clearing: bool,
    pub unknown_owners: u32,
    pub unfiled_sub_sectors: u32,
}

fn bytes(v: Vec<u8>) -> *mut EeAStarCmBytes {
    Box::into_raw(Box::new(EeAStarCmBytes(v)))
}

/// A block's bytes; `len` gets their count.
///
/// # Safety
/// `b` from this module; `len` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_bytes_data(b: *const EeAStarCmBytes, len: *mut usize) -> *const u8 {
    *len = (*b).0.len();
    (*b).0.as_ptr()
}

/// # Safety
/// `b` from this module (or NULL), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_bytes_free(b: *mut EeAStarCmBytes) {
    if !b.is_null() {
        drop(Box::from_raw(b));
    }
}

/// `WriteRegions`: the save block.
///
/// # Safety
/// `r` the manager's record; its objects live.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_write_regions(r: *const EeAStarCmRecord) -> *mut EeAStarCmBytes {
    bytes(write_regions(&*r))
}

/// `EEContinentRegionsDigest`.
///
/// # Safety
/// As `ee_astar_cm_write_regions`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_regions_digest(r: *const EeAStarCmRecord) -> c_ulong {
    regions_digest(&*r)
}

/// `EEAIContinentFill` (TEST ONLY): the CNTR record's bytes.
///
/// # Safety
/// As `ee_astar_cm_write_regions`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_capture_fill(r: *const EeAStarCmRecord) -> *mut EeAStarCmBytes {
    bytes(capture_fill(&*r))
}

/// `EEAIContinentDigest` (TEST ONLY).
///
/// # Safety
/// As `ee_astar_cm_write_regions`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_capture_digest(r: *const EeAStarCmRecord) -> u64 {
    capture_digest(&*r)
}

/// `ReadRegions`: NULL on a bad block (or another map's).
///
/// # Safety
/// `r` the manager's record; `data` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_read_regions(
    r: *mut EeAStarCmRecord,
    data: *const u8,
    len: usize,
) -> *mut EeAStarCmLoad {
    let block = if len == 0 { &[][..] } else { std::slice::from_raw_parts(data, len) };
    match read_regions(&mut *r, block) {
        Some(l) => Box::into_raw(Box::new(EeAStarCmLoad(l))),
        None => std::ptr::null_mut(),
    }
}

/// A load's counters.
///
/// # Safety
/// `l` from `ee_astar_cm_read_regions`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_load_info(l: *const EeAStarCmLoad) -> EeAStarCmLoadInfo {
    let l = &(*l).0;
    EeAStarCmLoadInfo {
        last_serial: l.last_serial,
        clearing: l.clearing,
        unknown_owners: l.unknown_owners,
        unfiled_sub_sectors: l.unfiled_sub_sectors,
    }
}

/// The towns (`herds` false) or herds the saved serial table named; `count` gets their number.
///
/// # Safety
/// `l` from `ee_astar_cm_read_regions`; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_load_owned(
    l: *const EeAStarCmLoad,
    herds: bool,
    count: *mut usize,
) -> *const EeAStarCmOwned {
    let list = if herds { &(*l).0.herds } else { &(*l).0.towns };
    *count = list.len();
    list.as_ptr() as *const EeAStarCmOwned
}

/// # Safety
/// `l` from `ee_astar_cm_read_regions` (or NULL), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_load_free(l: *mut EeAStarCmLoad) {
    if !l.is_null() {
        drop(Box::from_raw(l));
    }
}

/// `EEWriteAreaBytes`: one area (`h` an `EEAIArea*`) as the save writes it.
///
/// # Safety
/// `h` a live area.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_area_bytes(h: u64) -> *mut EeAStarCmBytes {
    let mut o = Out { bytes: Vec::new(), capture: false };
    write_area(&mut o, h);
    bytes(o.bytes)
}

/// `EEReadAreaBytes`: an area from those bytes; false unless they were exactly one area.
///
/// # Safety
/// `h` a live area; `data` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_read_area(h: u64, data: *const u8, len: usize) -> bool {
    let block = if len == 0 { &[][..] } else { std::slice::from_raw_parts(data, len) };
    let mut i = In { bytes: block, at: 0 };
    read_area(&mut i, h).is_some() && i.at == block.len()
}

/// `FindContinent`.
///
/// # Safety
/// `r` the manager's record; its objects live.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_find_continent(r: *const EeAStarCmRecord, x: c_long, y: c_long) -> u64 {
    find_continent(&*r, x, y)
}

/// `FindPathSubSector`.
///
/// # Safety
/// As `ee_astar_cm_find_continent`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_find_sub_sector(r: *const EeAStarCmRecord, x: c_long, y: c_long) -> u64 {
    find_sub_sector(&*r, x, y)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn set() -> EeSparseArray {
        // SAFETY: a fresh handle from the C interface
        unsafe { *Box::from_raw(crate::astar::ffi::ee_sparse_new()) }
    }

    #[test]
    fn sparse_round_trip() {
        let mut s = set();
        s.set.create(&[EePoint { x: 2, y: 3 }, EePoint { x: 2, y: 4 }, EePoint { x: 5, y: 1 }]);
        let mut o = Out { bytes: Vec::new(), capture: false };
        write_sparse(&mut o, &s);
        let mut t = set();
        let mut i = In { bytes: &o.bytes, at: 0 };
        assert!(read_sparse(&mut i, &mut t).is_some());
        assert_eq!(i.at, o.bytes.len());
        let (mut a, mut b) = (Vec::new(), Vec::new());
        s.set.raw(&mut a);
        t.set.raw(&mut b);
        assert_eq!(a, b);
    }

    #[test]
    fn encodings() {
        let mut o = Out { bytes: Vec::new(), capture: false };
        o.u16(7);
        o.flag(true);
        o.shorts(&[1, 2]);
        assert_eq!(o.bytes, vec![7, 0, 0, 0, 1, 2, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0]);
        let mut c = Out { bytes: Vec::new(), capture: true };
        c.flag(true);
        assert_eq!(c.bytes, vec![1, 0, 0, 0]);
        let mut i = In { bytes: &o.bytes, at: 0 };
        assert_eq!(i.u16(), Some(7));
        assert_eq!(i.flag(), Some(true));
        assert_eq!(i.shorts(), Some(vec![1, 2]));
        assert_eq!(i.u32(), None);
        // a count past the bytes left is refused
        let big = 100u32.to_le_bytes();
        assert_eq!(In { bytes: &big, at: 0 }.count(), None);
    }

    #[test]
    fn a_damaged_block_is_refused() {
        let mut r: Box<std::mem::MaybeUninit<EeAStarCmRecord>> = Box::new(std::mem::MaybeUninit::uninit());
        // SAFETY: init writes the record whole; a block of the wrong magic touches no object
        unsafe {
            crate::astar::cm::ee_astar_cm_init(r.as_mut_ptr());
            let r = &mut *r.as_mut_ptr();
            assert!(read_regions(r, &[1, 2, 3, 4, 5, 6, 7, 8]).is_none());
            r.free();
        }
    }
}
