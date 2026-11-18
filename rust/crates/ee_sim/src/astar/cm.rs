//! The continent manager's state (`EEAIContinentManager`, `ee/EEAIContinentManager.h`): its counters, the sector
//! grid's sizes and the four tile grids are `EeAStarCmRecord`, the C++ class's member (its scalars keep a fixed
//! address: `gMapWidth`, `gContinentManagerClearing` and the others alias them); the tables are Rust behind the
//! record's `tables`: the serial table of every area (`mAreas`), the objects by id of the eight kinds (continents,
//! path sectors, sub sectors, forests, path areas, hills, choke points, rectangle areas: the C++ objects' addresses,
//! 0 for none), the pending sector tiles (`mSectorTiles`), the shore lists and the free ids.
//!
//! The objects stay C++ (their classes are the types the AI uses); the tables hold their addresses as handles.
//!
//! RUST_PLAN 2.1: every container keeps the C++'s order: the serial table is `std::map<unsigned long, EEAIArea*>`'s
//! (sorted by serial; an insert of a present serial keeps the old entry), the pending tiles `std::map<unsigned
//! short, std::vector<EETileCoordinate>>`'s, the free ids are stacks (`push_back`, `back`/`pop_back`), the shore list
//! is sorted with `U2DPoint<long>::operator<` (`binary_search`).

use std::ffi::{c_long, c_ulong};

use crate::astar::hull::EeHullPoint;

/// The tables by id (`EeAStarCmTable`'s numbers).
pub const EE_ASTAR_CM_TABLES: usize = 8;

/// Which table of objects by id (the C++ manager's vectors).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeAStarCmTable {
    /// `mContinents` (`EEAIContinent*` by its id)
    Continents = 0,
    /// `mPathSectors` (`EEAIPathSector*` by its id)
    PathSectors = 1,
    /// `mPathSubSectors` (`EEAIPathSubSector*` by its id; not owned: the sectors own them)
    PathSubSectors = 2,
    /// `mForests` (`EEAIForest*`)
    Forests = 3,
    /// `mPathAreas` (`EEAIPathArea*`)
    PathAreas = 4,
    /// `mHills` (`EEAIHill*`)
    Hills = 5,
    /// `mChokePoints` (`EEAIChokePoint*` by its index)
    ChokePoints = 6,
    /// `mRectangleAreas` (`EEAIRectangleArea*`)
    RectangleAreas = 7,
}

/// Which free id list.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeAStarCmFree {
    /// `mFreeSubSectorIDs`
    SubSectors = 0,
    /// `mFreePathAreaIDs`
    PathAreas = 1,
}

/// Which shore list.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeAStarCmShore {
    /// `mShoreTiles` (sorted after the grid is built)
    Shore = 0,
    /// `mIceShoreTiles`
    IceShore = 1,
}

/// Which tile grid.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeAStarCmGrid {
    /// `mTileValues` (`long` per tile)
    TileValues = 0,
    /// `mContinentIDs` (`unsigned short` per tile)
    ContinentIds = 1,
    /// `mPathSubSectorIDs` (`unsigned short` per tile)
    SubSectorIds = 2,
    /// `mTileFlags` (`bool`, `mFlagsWidth` squared: the continents' adjacency)
    TileFlags = 3,
}

/// One entry of the serial table.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarCmArea {
    /// the area's serial (`EEAIArea::GetSerial()` when added)
    pub serial: c_ulong,
    /// the `EEAIArea*`
    pub area: u64,
}

/// One sector's pending tiles (`mSectorTiles`' entry).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct SectorTiles {
    pub id: u16,
    pub tiles: Vec<EeHullPoint>,
}

/// The tables (opaque to C++).
#[derive(Debug, Default)]
pub struct EeAStarCmTables {
    pub areas: Vec<EeAStarCmArea>,
    pub tables: [Vec<u64>; EE_ASTAR_CM_TABLES],
    pub sector_tiles: Vec<SectorTiles>,
    pub shore: [Vec<EeHullPoint>; 2],
    pub free: [Vec<u16>; 2],
    pub tile_values: Option<Box<[c_long]>>,
    pub continent_ids: Option<Box<[u16]>>,
    pub sub_sector_ids: Option<Box<[u16]>>,
    pub tile_flags: Option<Box<[bool]>>,
}

/// `EEAIContinentManager`'s members (`ee_astar_cm_init` / `ee_astar_cm_free` in its ctor and dtor). The grid
/// pointers are the tables' grids (NULL: none), kept here for the C++'s inline lookups.
#[repr(C)]
#[derive(Debug)]
pub struct EeAStarCmRecord {
    /// the tables (`ee_astar_cm_*`)
    pub tables: *mut EeAStarCmTables,
    /// `mTileValues` (width x height)
    pub tile_values: *mut c_long,
    /// `mContinentIDs` (width x height)
    pub continent_ids: *mut u16,
    /// `mPathSubSectorIDs` (width x height)
    pub sub_sector_ids: *mut u16,
    /// `mTileFlags` (flags_width squared)
    pub tile_flags: *mut bool,
    /// `mNextIDs`: the continental chains' next numbers by kind
    pub next_ids: [u16; 7],
    /// `mState` (2 while `Rebuild` repairs the path areas)
    pub state: c_long,
    /// `mLastSerial`: the last area serial handed out
    pub last_serial: c_ulong,
    /// `mL118` (only zeroed)
    pub l118: c_long,
    /// `mNextPathAreaID`
    pub next_path_area_id: u16,
    /// `mLastSubSectorID`
    pub last_sub_sector_id: u16,
    /// `mChecksum`
    pub checksum: c_long,
    /// `mNextID124`..`mNextID130`: the next land mass, water body, space region and surface region ids
    pub next_land_mass: c_long,
    pub next_water_body: c_long,
    pub next_space_region: c_long,
    pub next_surface_region: c_long,
    /// `mNumWaterTiles`, `mNumLandTiles`, `mNumSpaceTiles`
    pub num_water_tiles: c_long,
    pub num_land_tiles: c_long,
    pub num_space_tiles: c_long,
    /// `mFlagsWidth`: the adjacency matrix's side (the continent count)
    pub flags_width: c_long,
    /// `mWidth`, `mHeight`: the map's tiles
    pub width: c_ulong,
    pub height: c_ulong,
    /// `mClearing` (between `Clear` and the end of `BuildRegions`)
    pub clearing: bool,
    /// the sector grid (was `EEAIPathSectorRegionInformation`): tiles per sector side, its square, sectors per side
    pub sector_size: c_long,
    pub sector_area: c_long,
    pub sectors_x: c_long,
    pub sectors_y: c_long,
}

impl EeAStarCmTables {
    /// The serial table's index of `serial` (`Ok`) or where it goes (`Err`).
    fn find_area(&self, serial: c_ulong) -> Result<usize, usize> {
        self.areas.binary_search_by(|a| a.serial.cmp(&serial))
    }

    /// `mAreas.insert((serial, area))`: a present serial keeps its area.
    pub fn add_area(&mut self, serial: c_ulong, area: u64) {
        if let Err(at) = self.find_area(serial) {
            self.areas.insert(at, EeAStarCmArea { serial, area });
        }
    }

    /// `RemoveArea`: erases `serial`; whether it was there.
    pub fn remove_area(&mut self, serial: c_ulong) -> bool {
        match self.find_area(serial) {
            Ok(at) => {
                self.areas.remove(at);
                true
            }
            Err(_) => false,
        }
    }

    /// `GetArea`: the area of `serial`, 0 if none.
    pub fn area(&self, serial: c_ulong) -> u64 {
        self.find_area(serial).map(|at| self.areas[at].area).unwrap_or(0)
    }

    /// `AddSectorTiles`' insert: `tile` appended to sector `id`'s pending tiles (a new entry in id order).
    pub fn add_sector_tile(&mut self, id: u16, tile: EeHullPoint) {
        match self.sector_tiles.binary_search_by(|s| s.id.cmp(&id)) {
            Ok(at) => self.sector_tiles[at].tiles.push(tile),
            Err(at) => self.sector_tiles.insert(at, SectorTiles { id, tiles: vec![tile] }),
        }
    }

    /// `RemoveSectorTiles`: drops sector `id`'s pending tiles.
    pub fn remove_sector_tiles(&mut self, id: u16) {
        if let Ok(at) = self.sector_tiles.binary_search_by(|s| s.id.cmp(&id)) {
            self.sector_tiles.remove(at);
        }
    }

    /// `NewSubSectorID` / `NewPathAreaID`'s free list: the last freed id.
    pub fn pop_free(&mut self, list: EeAStarCmFree) -> Option<u16> {
        self.free[list as usize].pop()
    }

    /// `std::binary_search` of `tile` in the sorted shore list.
    pub fn is_shore(&self, tile: EeHullPoint) -> bool {
        self.shore[EeAStarCmShore::Shore as usize].binary_search_by(|p| point_cmp(p, &tile)).is_ok()
    }
}

/// `U2DPoint<long>::operator<`: the lower y first, then the larger x.
pub fn point_cmp(a: &EeHullPoint, b: &EeHullPoint) -> std::cmp::Ordering {
    a.y.cmp(&b.y).then(b.x.cmp(&a.x))
}

impl EeAStarCmRecord {
    /// The tables (made by `ee_astar_cm_init`).
    pub fn tables(&self) -> &EeAStarCmTables {
        // SAFETY: made by `init`, freed only by `free`
        unsafe { &*self.tables }
    }

    pub fn tables_mut(&mut self) -> &mut EeAStarCmTables {
        // SAFETY: as `tables`
        unsafe { &mut *self.tables }
    }

    /// The record of a new manager: no grids, every id and counter 0.
    pub fn init(&mut self) {
        *self = EeAStarCmRecord {
            tables: Box::into_raw(Box::default()),
            tile_values: std::ptr::null_mut(),
            continent_ids: std::ptr::null_mut(),
            sub_sector_ids: std::ptr::null_mut(),
            tile_flags: std::ptr::null_mut(),
            next_ids: [0; 7],
            state: 0,
            last_serial: 0,
            l118: 0,
            next_path_area_id: 0,
            last_sub_sector_id: 0,
            checksum: 0,
            next_land_mass: 0,
            next_water_body: 0,
            next_space_region: 0,
            next_surface_region: 0,
            num_water_tiles: 0,
            num_land_tiles: 0,
            num_space_tiles: 0,
            flags_width: 0,
            width: 0,
            height: 0,
            clearing: false,
            sector_size: 0,
            sector_area: 0,
            sectors_x: 0,
            sectors_y: 0,
        };
    }

    /// Frees the tables (the record is then unusable until `init`).
    pub fn free(&mut self) {
        if !self.tables.is_null() {
            // SAFETY: made by `init`
            drop(unsafe { Box::from_raw(self.tables) });
            self.tables = std::ptr::null_mut();
        }
        self.sync_grids();
    }

    /// The grid pointers again after a grid changed.
    fn sync_grids(&mut self) {
        if self.tables.is_null() {
            self.tile_values = std::ptr::null_mut();
            self.continent_ids = std::ptr::null_mut();
            self.sub_sector_ids = std::ptr::null_mut();
            self.tile_flags = std::ptr::null_mut();
            return;
        }
        // SAFETY: as `tables`; the boxes live in the tables until replaced
        let t = unsafe { &mut *self.tables };
        self.tile_values = t.tile_values.as_mut().map_or(std::ptr::null_mut(), |g| g.as_mut_ptr());
        self.continent_ids = t.continent_ids.as_mut().map_or(std::ptr::null_mut(), |g| g.as_mut_ptr());
        self.sub_sector_ids = t.sub_sector_ids.as_mut().map_or(std::ptr::null_mut(), |g| g.as_mut_ptr());
        self.tile_flags = t.tile_flags.as_mut().map_or(std::ptr::null_mut(), |g| g.as_mut_ptr());
    }

    /// A grid of `count` zeroed entries in place of the old one (`new T[count]` + `memset`); `count` none frees it.
    pub fn set_grid(&mut self, grid: EeAStarCmGrid, count: Option<usize>) {
        let t = self.tables_mut();
        match grid {
            EeAStarCmGrid::TileValues => t.tile_values = count.map(|n| vec![0; n].into_boxed_slice()),
            EeAStarCmGrid::ContinentIds => t.continent_ids = count.map(|n| vec![0; n].into_boxed_slice()),
            EeAStarCmGrid::SubSectorIds => t.sub_sector_ids = count.map(|n| vec![0; n].into_boxed_slice()),
            EeAStarCmGrid::TileFlags => t.tile_flags = count.map(|n| vec![false; n].into_boxed_slice()),
        }
        self.sync_grids();
    }

    /// A grid's entry count (0: none).
    pub fn grid_len(&self, grid: EeAStarCmGrid) -> usize {
        let t = self.tables();
        match grid {
            EeAStarCmGrid::TileValues => t.tile_values.as_ref().map_or(0, |g| g.len()),
            EeAStarCmGrid::ContinentIds => t.continent_ids.as_ref().map_or(0, |g| g.len()),
            EeAStarCmGrid::SubSectorIds => t.sub_sector_ids.as_ref().map_or(0, |g| g.len()),
            EeAStarCmGrid::TileFlags => t.tile_flags.as_ref().map_or(0, |g| g.len()),
        }
    }

    /// `Reset`'s counters (the grids and the sector grid stay; the tables' objects are the C++'s to delete).
    pub fn reset_counters(&mut self) {
        self.last_serial = 0;
        self.l118 = 0;
        self.next_path_area_id = 0;
        self.last_sub_sector_id = 0;
        self.checksum = 0;
        self.next_land_mass = 0;
        self.next_water_body = 0;
        self.next_space_region = 0;
        self.next_surface_region = 0;
        self.flags_width = 0;
        self.width = 0;
        self.height = 0;
        self.state = 0;
        self.next_ids = [0; 7];
    }

    /// `EEAIPathSectorRegionInformation::Init`: the sector size, its square, and the sectors per side (the ceiling
    /// of the map's size over it).
    #[allow(clippy::unnecessary_cast)] // `c_long` is `i64` on LP64
    pub fn init_sectors(&mut self, map_size: c_long, sector_size: c_long) {
        self.sector_size = sector_size;
        self.sector_area = sector_size.wrapping_mul(sector_size);
        let per_side = crate::astar::sectors::sectors_per_side(map_size as i32, sector_size as i32) as c_long;
        self.sectors_x = per_side;
        self.sectors_y = per_side;
    }

    /// `EEAIPathSectorRegionInformation::Reset`.
    pub fn reset_sectors(&mut self) {
        self.sector_size = 0;
        self.sector_area = 0;
        self.sectors_x = 0;
        self.sectors_y = 0;
    }

    /// `NewSubSectorID`: the last freed id, else `++mLastSubSectorID`.
    pub fn new_sub_sector_id(&mut self) -> u16 {
        match self.tables_mut().pop_free(EeAStarCmFree::SubSectors) {
            Some(id) => id,
            None => {
                self.last_sub_sector_id = self.last_sub_sector_id.wrapping_add(1);
                self.last_sub_sector_id
            }
        }
    }

    /// `NewPathAreaID`: the last freed id, else `mNextPathAreaID++`.
    pub fn new_path_area_id(&mut self) -> u16 {
        match self.tables_mut().pop_free(EeAStarCmFree::PathAreas) {
            Some(id) => id,
            None => {
                let id = self.next_path_area_id;
                self.next_path_area_id = id.wrapping_add(1);
                id
            }
        }
    }

    /// The inline `GetContinentID(x, y)`'s grid part: `Some(id)` on the map with the grid, `Some(0)` off it,
    /// `None` on the map without the grid (the C++ then searches: `FindContinentID`).
    pub fn grid_continent_id(&self, x: c_long, y: c_long) -> Option<u16> {
        // `inX < mWidth`: a long against an unsigned long (unsigned on both builds)
        if !((x as c_ulong) < self.width && (y as c_ulong) < self.height) {
            return Some(0);
        }
        let g = self.tables().continent_ids.as_ref()?;
        let at = (x as c_ulong).wrapping_mul(self.width).wrapping_add(y as c_ulong) as usize;
        Some(g.get(at).copied().unwrap_or(0))
    }

    /// The path sector table's index of tile (x, y) (`GetPathSector`: `x / size * sectorsX + y / size + 1`), none
    /// off the map.
    pub fn sector_index(&self, x: c_long, y: c_long) -> Option<usize> {
        if !((x as c_ulong) < self.width && (y as c_ulong) < self.height) || self.sector_size == 0 {
            return None;
        }
        let sx = x / self.sector_size;
        let sy = y / self.sector_size;
        Some(sx.wrapping_mul(self.sectors_x).wrapping_add(sy).wrapping_add(1) as usize)
    }

    /// The continents adjacent to `id` (`GetConnectedIDs` with `with_self`, `GetAdjacentIDs` without): empty for
    /// an id past the matrix.
    pub fn adjacent_ids(&self, id: u16, with_self: bool) -> Vec<u16> {
        if !((id as c_long) < self.flags_width) {
            return Vec::new();
        }
        match self.tables().tile_flags.as_ref() {
            Some(f) => crate::astar::queries::adjacent_ids(f, self.flags_width as usize, id, with_self),
            None => Vec::new(),
        }
    }
}

// ---- the C interface

/// # Safety
/// `r` points to a record (uninitialised is fine).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_init(r: *mut EeAStarCmRecord) {
    // the record may be uninitialised memory: write it whole without reading it
    std::ptr::write(
        r,
        EeAStarCmRecord {
            tables: std::ptr::null_mut(),
            tile_values: std::ptr::null_mut(),
            continent_ids: std::ptr::null_mut(),
            sub_sector_ids: std::ptr::null_mut(),
            tile_flags: std::ptr::null_mut(),
            next_ids: [0; 7],
            state: 0,
            last_serial: 0,
            l118: 0,
            next_path_area_id: 0,
            last_sub_sector_id: 0,
            checksum: 0,
            next_land_mass: 0,
            next_water_body: 0,
            next_space_region: 0,
            next_surface_region: 0,
            num_water_tiles: 0,
            num_land_tiles: 0,
            num_space_tiles: 0,
            flags_width: 0,
            width: 0,
            height: 0,
            clearing: false,
            sector_size: 0,
            sector_area: 0,
            sectors_x: 0,
            sectors_y: 0,
        },
    );
    (*r).init();
}

/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_free(r: *mut EeAStarCmRecord) {
    (*r).free();
}

/// Table `table`'s entries (the objects' addresses by id, 0 none); `count` gets its size. Valid until it changes.
///
/// # Safety
/// `r` from `ee_astar_cm_init`; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_table(
    r: *const EeAStarCmRecord,
    table: EeAStarCmTable,
    count: *mut usize,
) -> *const u64 {
    let t = &(*r).tables().tables[table as usize];
    *count = t.len();
    if t.is_empty() {
        std::ptr::null()
    } else {
        t.as_ptr()
    }
}

/// `table.resize(count, NULL)` (entries below `count` kept).
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_table_resize(r: *mut EeAStarCmRecord, table: EeAStarCmTable, count: usize) {
    (*r).tables_mut().tables[table as usize].resize(count, 0);
}

/// `table[id] = object` (the table grows to hold `id` when `grow`: `push_back` past its end; else an id past the
/// end is ignored).
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_table_set(r: *mut EeAStarCmRecord, table: EeAStarCmTable, id: usize, object: u64) {
    let t = &mut (*r).tables_mut().tables[table as usize];
    if id < t.len() {
        t[id] = object;
    }
}

/// `table.push_back(object)`.
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_table_push(r: *mut EeAStarCmRecord, table: EeAStarCmTable, object: u64) {
    (*r).tables_mut().tables[table as usize].push(object);
}

/// The serial table (sorted by serial); `count` gets its size. Valid until it changes.
///
/// # Safety
/// `r` from `ee_astar_cm_init`; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_areas(r: *const EeAStarCmRecord, count: *mut usize) -> *const EeAStarCmArea {
    let a = &(*r).tables().areas;
    *count = a.len();
    if a.is_empty() {
        std::ptr::null()
    } else {
        a.as_ptr()
    }
}

/// `AddArea`.
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_add_area(r: *mut EeAStarCmRecord, serial: c_ulong, area: u64) {
    (*r).tables_mut().add_area(serial, area);
}

/// `RemoveArea`.
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_remove_area(r: *mut EeAStarCmRecord, serial: c_ulong) -> bool {
    (*r).tables_mut().remove_area(serial)
}

/// `GetArea`.
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_area(r: *const EeAStarCmRecord, serial: c_ulong) -> u64 {
    (*r).tables().area(serial)
}

/// Empties the serial table.
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_clear_areas(r: *mut EeAStarCmRecord) {
    (*r).tables_mut().areas.clear();
}

/// `AddSectorTiles`' insert of one tile under sector `id`.
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_add_sector_tile(r: *mut EeAStarCmRecord, id: u16, tile: EeHullPoint) {
    (*r).tables_mut().add_sector_tile(id, tile);
}

/// `RemoveSectorTiles`.
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_remove_sector_tiles(r: *mut EeAStarCmRecord, id: u16) {
    (*r).tables_mut().remove_sector_tiles(id);
}

/// The pending sectors' count (`mSectorTiles.size()`).
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_sector_tiles_count(r: *const EeAStarCmRecord) -> usize {
    (*r).tables().sector_tiles.len()
}

/// The `index`-th pending sector in id order: its id, and its tiles (`count` gets their number). NULL past the end.
///
/// # Safety
/// `r` from `ee_astar_cm_init`; `id` and `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_sector_tiles(
    r: *const EeAStarCmRecord,
    index: usize,
    id: *mut u16,
    count: *mut usize,
) -> *const EeHullPoint {
    match (*r).tables().sector_tiles.get(index) {
        Some(s) => {
            *id = s.id;
            *count = s.tiles.len();
            s.tiles.as_ptr()
        }
        None => {
            *id = 0;
            *count = 0;
            std::ptr::null()
        }
    }
}

/// Empties the pending sector tiles.
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_clear_sector_tiles(r: *mut EeAStarCmRecord) {
    (*r).tables_mut().sector_tiles.clear();
}

/// A shore list; `count` gets its size. Valid until it changes.
///
/// # Safety
/// `r` from `ee_astar_cm_init`; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_shore(
    r: *const EeAStarCmRecord,
    list: EeAStarCmShore,
    count: *mut usize,
) -> *const EeHullPoint {
    let s = &(*r).tables().shore[list as usize];
    *count = s.len();
    if s.is_empty() {
        std::ptr::null()
    } else {
        s.as_ptr()
    }
}

/// Replaces a shore list with `count` tiles.
///
/// # Safety
/// `r` from `ee_astar_cm_init`; `tiles` holds `count` points (NULL for 0).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_set_shore(
    r: *mut EeAStarCmRecord,
    list: EeAStarCmShore,
    tiles: *const EeHullPoint,
    count: usize,
) {
    let s = &mut (*r).tables_mut().shore[list as usize];
    s.clear();
    if count > 0 {
        s.extend_from_slice(std::slice::from_raw_parts(tiles, count));
    }
}

/// `IsShoreTile`: `std::binary_search` of (x, y) in the shore list.
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_is_shore(r: *const EeAStarCmRecord, x: c_long, y: c_long) -> bool {
    (*r).tables().is_shore(EeHullPoint { x, y })
}

/// A free id list; `count` gets its size.
///
/// # Safety
/// `r` from `ee_astar_cm_init`; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_free_ids(
    r: *const EeAStarCmRecord,
    list: EeAStarCmFree,
    count: *mut usize,
) -> *const u16 {
    let f = &(*r).tables().free[list as usize];
    *count = f.len();
    if f.is_empty() {
        std::ptr::null()
    } else {
        f.as_ptr()
    }
}

/// Appends `count` ids to a free id list (`insert(end, ...)` / `push_back`); `clear` empties it first.
///
/// # Safety
/// `r` from `ee_astar_cm_init`; `ids` holds `count` ids (NULL for 0).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_add_free_ids(
    r: *mut EeAStarCmRecord,
    list: EeAStarCmFree,
    ids: *const u16,
    count: usize,
    clear: bool,
) {
    let f = &mut (*r).tables_mut().free[list as usize];
    if clear {
        f.clear();
    }
    if count > 0 {
        f.extend_from_slice(std::slice::from_raw_parts(ids, count));
    }
}

/// `NewSubSectorID`.
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_new_sub_sector_id(r: *mut EeAStarCmRecord) -> u16 {
    (*r).new_sub_sector_id()
}

/// `NewPathAreaID`.
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_new_path_area_id(r: *mut EeAStarCmRecord) -> u16 {
    (*r).new_path_area_id()
}

/// A grid of `count` zeroed entries (`allocate`), or none (`!allocate`); the record's pointer follows.
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_set_grid(
    r: *mut EeAStarCmRecord,
    grid: EeAStarCmGrid,
    allocate: bool,
    count: usize,
) {
    (*r).set_grid(grid, if allocate { Some(count) } else { None });
}

/// A grid's entry count (0: none).
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_grid_len(r: *const EeAStarCmRecord, grid: EeAStarCmGrid) -> usize {
    (*r).grid_len(grid)
}

/// `Reset`'s part on the record and tables: the counters, the serial table, the pending tiles, the shore lists
/// and the free path area ids (the C++ deletes the objects first and empties the object tables).
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_reset(r: *mut EeAStarCmRecord) {
    let r = &mut *r;
    r.reset_counters();
    let t = r.tables_mut();
    t.areas.clear();
    t.sector_tiles.clear();
    for s in t.shore.iter_mut() {
        s.clear();
    }
    t.free[EeAStarCmFree::PathAreas as usize].clear();
}

/// `EEAIPathSectorRegionInformation::Init` (`map_size` the map's tiles per side).
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_init_sectors(r: *mut EeAStarCmRecord, map_size: c_long, sector_size: c_long) {
    (*r).init_sectors(map_size, sector_size);
}

/// `DeletePathSectors`' part after the sectors are deleted: the last sub sector id, its free ids, the sector grid.
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_sectors_deleted(r: *mut EeAStarCmRecord) {
    let r = &mut *r;
    r.tables_mut().tables[EeAStarCmTable::PathSectors as usize].clear();
    r.last_sub_sector_id = 0;
    r.tables_mut().free[EeAStarCmFree::SubSectors as usize].clear();
    r.reset_sectors();
}

/// The path sector table's index of (x, y) (`GetPathSector`), -1 off the map.
///
/// # Safety
/// `r` from `ee_astar_cm_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_sector_index(r: *const EeAStarCmRecord, x: c_long, y: c_long) -> isize {
    (*r).sector_index(x, y).map_or(-1, |i| i as isize)
}

/// `GetConnectedIDs` (`with_self`) / `GetAdjacentIDs`: the ids into `out` (room for `flags_width`); their count.
///
/// # Safety
/// `r` from `ee_astar_cm_init`; `out` holds `flags_width` ids.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_adjacent_ids(
    r: *const EeAStarCmRecord,
    id: u16,
    with_self: bool,
    out: *mut u16,
) -> usize {
    let ids = (*r).adjacent_ids(id, with_self);
    if !ids.is_empty() {
        std::ptr::copy_nonoverlapping(ids.as_ptr(), out, ids.len());
    }
    ids.len()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn record() -> Box<EeAStarCmRecord> {
        let mut r: Box<std::mem::MaybeUninit<EeAStarCmRecord>> = Box::new(std::mem::MaybeUninit::uninit());
        // SAFETY: init writes the whole record
        unsafe {
            ee_astar_cm_init(r.as_mut_ptr());
            Box::from_raw(Box::into_raw(r) as *mut EeAStarCmRecord)
        }
    }

    #[test]
    fn serial_table_is_a_map() {
        let mut r = record();
        let t = r.tables_mut();
        t.add_area(5, 0x50);
        t.add_area(2, 0x20);
        t.add_area(9, 0x90);
        t.add_area(5, 0x55); // present: kept
        assert_eq!(
            t.areas.iter().map(|a| (a.serial, a.area)).collect::<Vec<_>>(),
            vec![(2, 0x20), (5, 0x50), (9, 0x90)]
        );
        assert_eq!(t.area(5), 0x50);
        assert_eq!(t.area(6), 0);
        assert!(t.remove_area(5));
        assert!(!t.remove_area(5));
        assert_eq!(t.areas.len(), 2);
        r.free();
    }

    #[test]
    fn sector_tiles_by_id() {
        let mut r = record();
        let t = r.tables_mut();
        t.add_sector_tile(7, EeHullPoint { x: 1, y: 2 });
        t.add_sector_tile(3, EeHullPoint { x: 4, y: 5 });
        t.add_sector_tile(7, EeHullPoint { x: 6, y: 7 });
        assert_eq!(t.sector_tiles.iter().map(|s| s.id).collect::<Vec<_>>(), vec![3, 7]);
        assert_eq!(t.sector_tiles[1].tiles.len(), 2);
        t.remove_sector_tiles(3);
        t.remove_sector_tiles(4);
        assert_eq!(t.sector_tiles.len(), 1);
        r.free();
    }

    #[test]
    fn new_ids_reuse_the_last_freed() {
        let mut r = record();
        assert_eq!(r.new_sub_sector_id(), 1);
        assert_eq!(r.new_sub_sector_id(), 2);
        // SAFETY: the record is initialised
        unsafe { ee_astar_cm_add_free_ids(&mut *r, EeAStarCmFree::SubSectors, [9u16, 4].as_ptr(), 2, false) };
        assert_eq!(r.new_sub_sector_id(), 4);
        assert_eq!(r.new_sub_sector_id(), 9);
        assert_eq!(r.new_sub_sector_id(), 3);
        assert_eq!(r.new_path_area_id(), 0);
        assert_eq!(r.new_path_area_id(), 1);
        r.free();
    }

    #[test]
    fn grids_and_lookups() {
        let mut r = record();
        r.width = 4;
        r.height = 4;
        assert_eq!(r.grid_continent_id(1, 1), None);
        assert_eq!(r.grid_continent_id(-1, 1), Some(0));
        r.set_grid(EeAStarCmGrid::ContinentIds, Some(16));
        assert!(!r.continent_ids.is_null());
        // SAFETY: 16 entries
        unsafe { *r.continent_ids.add(4 + 2) = 3 };
        assert_eq!(r.grid_continent_id(1, 2), Some(3));
        assert_eq!(r.grid_continent_id(4, 0), Some(0));
        r.set_grid(EeAStarCmGrid::ContinentIds, None);
        assert!(r.continent_ids.is_null());
        r.init_sectors(4, 3);
        assert_eq!((r.sector_area, r.sectors_x, r.sectors_y), (9, 2, 2));
        assert_eq!(r.sector_index(0, 0), Some(1));
        assert_eq!(r.sector_index(3, 1), Some(3));
        assert_eq!(r.sector_index(3, 3), Some(4));
        assert_eq!(r.sector_index(4, 3), None);
        r.free();
    }

    #[test]
    fn shore_and_adjacency() {
        let mut r = record();
        // sorted with `U2DPoint<long>::operator<`: by y, then the larger x first
        let tiles = [
            EeHullPoint { x: 2, y: 0 },
            EeHullPoint { x: 2, y: 3 },
            EeHullPoint { x: 1, y: 3 },
            EeHullPoint { x: 1, y: 5 },
        ];
        // SAFETY: three tiles
        unsafe { ee_astar_cm_set_shore(&mut *r, EeAStarCmShore::Shore, tiles.as_ptr(), 4) };
        assert!(r.tables().is_shore(EeHullPoint { x: 2, y: 3 }));
        assert!(r.tables().is_shore(EeHullPoint { x: 1, y: 3 }));
        assert!(r.tables().is_shore(EeHullPoint { x: 1, y: 5 }));
        assert!(!r.tables().is_shore(EeHullPoint { x: 2, y: 4 }));
        r.flags_width = 3;
        r.set_grid(EeAStarCmGrid::TileFlags, Some(9));
        // SAFETY: nine entries
        unsafe {
            *r.tile_flags.add(3 + 2) = true;
            *r.tile_flags.add(3 + 1) = true;
        }
        assert_eq!(r.adjacent_ids(1, true), vec![2, 1]);
        assert_eq!(r.adjacent_ids(1, false), vec![2]);
        assert!(r.adjacent_ids(3, true).is_empty());
        r.free();
    }
}
