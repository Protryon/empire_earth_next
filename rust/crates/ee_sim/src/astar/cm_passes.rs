//! The continent manager's build passes whole (was EEAIContinentManager.cpp and EEAIChokePoint.cpp): the
//! continents and their grid (`CreateContinents`), the hills, the path areas, forests, path sectors and sub sectors
//! with their links (`CreatePathAreas`, `CreatePathSectors`, `LinkSubSectorGrid`), the region ids
//! (`CreatePathAreaKind`), the choke points (`EEAIBuildChokePoints`, `PruneChokePoints`), the scenario areas, the
//! continental chains, `Clear`, `BuildRegions`, `Reset` and the sector rebuild (`Rebuild`, `RelinkSubSectors`).
//!
//! Each pass drives the fills (flood.rs) over the world view (world_view.rs) as the C++ did, makes the objects
//! (objects.rs) and files them in the record. The queries the passes made through the manager's public members
//! (`GetPathAreaAt`, `GetForestAt`, `RepairPathArea`) are the queries with their CNTQ records (cm_queries.rs). No
//! reference to the record lives across a fill's run: the view reads the registered record while it runs.
//!
//! RUST_PLAN 2.1: the C++'s loops (from the last tile, x outer), its maps by id (`BTreeMap`), its set of path areas
//! by address (`BTreeSet` of the handles), its f32 sums.

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::collections::{BTreeMap, BTreeSet};
use std::ffi::{c_long, c_ulong, c_void};

use crate::astar::area_kinds::{
    ee_astar_choke_add_area_serials, ee_astar_forest_update_resource_kind, ee_astar_path_area_add_sector_id,
};
use crate::astar::area_ops;
use crate::astar::cm::{EeAStarCmGrid, EeAStarCmRecord, EeAStarCmTable};
use crate::astar::cm_io::{self, EeAStarCmIo};
use crate::astar::cm_queries;
use crate::astar::continent::{ee_astar_continent_add_choke_point, ee_astar_continent_add_id, EeAStarContinentList};
use crate::astar::continent_grid::{self, EePoint};
use crate::astar::ffi;
use crate::astar::ffi::EeAStarViewCallbacks;
use crate::astar::flood::{self, EeAStarChainCallbacks, EeAStarFlood, EeAStarFloodKind, EeAStarFloodState};
use crate::astar::hull::EeHullPoint;
use crate::astar::manager::{self, EeScenarioArea, PruneHost};
use crate::astar::objects::new_object;
use crate::astar::path_area_kind::{EeMapEdges, EePathAreaIDs};
use crate::astar::sector_rec::{
    ee_astar_sector_add_neighbor, ee_astar_sector_add_path_area_id, ee_astar_sector_insert_sub,
};
use crate::astar::storage::AStarRect;
use crate::astar::tile_kind::EeTileUnit;
use crate::astar::world_view::{ee_astar_world_view_callbacks, EeAStarWorldView};
use crate::world::{MapView, TileView, UnitView, WorldRead};

// the kinds (ee/EEAIKindIDs.h)
const AREA_CONTINENT: c_long = 1;
const AREA_CHOKE_POINT: c_long = 2;
const AREA_FOREST: c_long = 3;
const AREA_HILL: c_long = 7;
const AREA_RECTANGLE: c_long = 8;
const AREA_PATH_AREA: c_long = 10;
const CATEGORY_LAND: u8 = 1;
const CATEGORY_WATER: u8 = 2;
const CATEGORY_CLIFF: u8 = 3;
const CATEGORY_SPACE: u8 = 4;
const PATH_NONE: i32 = 0;
const PATH_WATER_BODY: i32 = 1;
const PATH_LAND_MASS: i32 = 2;
const PATH_SPACE_REGION: i32 = 3;
const PATH_NON_SPACE: i32 = 4;
const TILE_FOREST: i32 = 4;
const TILE_UNIT_OBSTACLE: i32 = 0xc;
const TERRAIN_CLIFF: i32 = 9;
const RESOURCE_WOOD: i64 = 3;
const CATEGORY_GATE: u64 = 0x19;
const TYPE_DOCK: u64 = 0x24;
const TYPE_NAVY_YARD: u64 = 0x41;
const TYPE_SPACE_DOCK: u64 = 0x2d8;
const TYPE_SPACE_TURRET: u64 = 0x302;
const TYPE_ORBITAL_STATION: u64 = 0x31f;
/// "CNTB", "CNTR"
const CNTB: u32 = 0x4254_4e43;
const CNTR: u32 = 0x5254_4e43;

fn token() -> WorldRead {
    // SAFETY: the simulation thread's reads of the live world
    unsafe { WorldRead::begin() }
}

/// One pass's world: the map and the influence table's region size (the areas' bounds).
struct Pass<'t> {
    map: MapView<'t>,
    handle: u64,
    size: i32,
    region: c_long,
}

impl<'t> Pass<'t> {
    fn edges(&self) -> EeMapEdges {
        let r = self.map.rts();
        EeMapEdges {
            sum_low: crate::world_field!(r, mL195608) as i32,
            half: crate::world_field!(r, mL195610) as i32,
            sum_high: crate::world_field!(r, mL19560C) as i32,
        }
    }
    fn size_class(&self) -> i32 {
        crate::world_field!(self.map.rts(), mL195610) as i32
    }
    fn tile(&self, x: i32, y: i32) -> Option<TileView<'t>> {
        self.map.tile(x as i64, y as i64)
    }
}

/// A fill and its view (`EEAIAStarView` of the pass's map).
struct Fill {
    f: *mut EeAStarFlood,
    view: Box<EeAStarWorldView>,
    cb: EeAStarViewCallbacks,
}

impl Fill {
    fn new(kind: EeAStarFloodKind, machine: i32, p: &Pass) -> Fill {
        let f = flood::ee_astar_flood_new(kind, machine, p.size);
        let mut view = Box::new(EeAStarWorldView { map: p.handle as usize as *mut c_void, quiet: false });
        // SAFETY: the boxed view outlives the callbacks (both are the fill's)
        let cb = unsafe { ee_astar_world_view_callbacks(&mut *view) };
        Fill { f, view, cb }
    }
    #[allow(clippy::mut_from_ref)] // the fill's own state, never borrowed across a run
    fn state(&self) -> &mut EeAStarFloodState {
        // SAFETY: the fill's own state, used between runs only
        unsafe { &mut *flood::ee_astar_flood_state(self.f) }
    }
    fn init(&self, w: i32, h: i32) {
        // SAFETY: the fill
        unsafe { flood::ee_astar_flood_init(self.f, w, h) }
    }
    fn id(&self, x: i32, y: i32) -> u32 {
        // SAFETY: an initialised fill
        unsafe { flood::ee_astar_flood_id(self.f, x, y) }
    }
    fn corners(&self, on: bool) {
        // SAFETY: the fill
        unsafe { flood::ee_astar_flood_set_check_corners(self.f, on) }
    }
    fn run(&self, x: i32, y: i32, gx: i32, gy: i32) {
        let _ = &self.view;
        // SAFETY: the fill and its callbacks (the view is alive)
        unsafe { flood::ee_astar_flood_run(self.f, x, y, gx, gy, &self.cb) }
    }
    fn collect(&self, id: u32) -> Vec<EePoint> {
        let mut n = 0;
        // SAFETY: the fill; the tiles copied before the next call
        unsafe {
            let p = flood::ee_astar_flood_collect(self.f, id, &mut n);
            if n == 0 {
                Vec::new()
            } else {
                std::slice::from_raw_parts(p, n).to_vec()
            }
        }
    }
    fn finish(&self, bounds: Option<AStarRect>) {
        // SAFETY: the fill
        unsafe {
            match bounds {
                Some(b) => flood::ee_astar_flood_finish(self.f, &b),
                None => flood::ee_astar_flood_finish(self.f, std::ptr::null()),
            }
        }
    }
}

impl Drop for Fill {
    fn drop(&mut self) {
        // SAFETY: the fill, freed once
        unsafe { flood::ee_astar_flood_free(self.f) };
    }
}

// ---- the record, briefly (never across a fill's run)

unsafe fn rm<'a>(r: *mut EeAStarCmRecord) -> &'a mut EeAStarCmRecord {
    &mut *r
}

unsafe fn resize(r: *mut EeAStarCmRecord, t: EeAStarCmTable, n: usize) {
    crate::astar::cm::ee_astar_cm_table_resize(r, t, n);
}

unsafe fn set(r: *mut EeAStarCmRecord, t: EeAStarCmTable, id: usize, h: u64) {
    crate::astar::cm::ee_astar_cm_table_set(r, t, id, h);
}

fn at(r: &EeAStarCmRecord, t: EeAStarCmTable, id: usize) -> u64 {
    r.tables().tables[t as usize].get(id).copied().unwrap_or(0)
}

unsafe fn new_serial(r: *mut EeAStarCmRecord) -> c_ulong {
    let r = rm(r);
    r.last_serial = r.last_serial.wrapping_add(1);
    r.last_serial
}

unsafe fn add_area(r: *mut EeAStarCmRecord, h: u64) {
    cm_queries::ee_astar_cm_add_object_area(r, h);
}

/// `Create(tiles)` of an area of the manager's kinds, then `UpdateBounds(gWorld.mInfluenceTable)`.
unsafe fn create_area(h: u64, tiles: &[EePoint], region: c_long) {
    let set = cm_io::area_tiles(h) as *mut _;
    ee_base::sparse::ee_sparse_insert_list(set, tiles.as_ptr(), tiles.len(), true);
    area_ops::ee_astar_area_update_hull_captured(cm_io::area(h), set, region);
    crate::astar::area::ee_astar_area_update_bounds(cm_io::area(h), region);
}

/// `EEAIAStarRectOf(GetHullBounds())`.
unsafe fn hull_bounds(h: u64) -> AStarRect {
    let b = cm_io::area(h).hull_bounds;
    AStarRect { bottom: b.bottom as i32, left: b.left as i32, right: b.right as i32, top: b.top as i32 }
}

/// `EEAITileUnits`: the units of a map tile, in its list's order.
pub(crate) fn tile_units(tl: &TileView<'_>) -> Vec<EeTileUnit> {
    tl.units().flatten().map(|u| tile_unit(&u)).collect()
}

fn tile_unit(u: &UnitView<'_>) -> EeTileUnit {
    let c = u.class();
    let unit_type = c.map_or(0, |c| c.unit_type());
    EeTileUnit {
        wood: c.is_some_and(|c| {
            let v: c_long = crate::world_field!(c.fields(), mResourceType);
            v as i64 == RESOURCE_WOOD
        }),
        has_amount: u.resource_amount() > 0,
        blocks: c.is_some_and(|c| c.blocks_tile()),
        gate: c.is_some_and(|c| c.category() == CATEGORY_GATE),
        wall: c.is_some_and(|c| c.wall()),
        foundation: u.is_complex() && crate::world_field!(u.complex(), mIsFoundation),
        alive: u.hit_points() > 0,
        dock: unit_type == TYPE_DOCK || unit_type == TYPE_NAVY_YARD,
        space_dock: unit_type == TYPE_SPACE_DOCK || unit_type == TYPE_SPACE_TURRET || unit_type == TYPE_ORBITAL_STATION,
        id: u.id() as i32,
    }
}

// ---- the continents

/// `CreateContinents(map)`.
unsafe fn create_continents(r: *mut EeAStarCmRecord, p: &Pass) {
    let fill = Fill::new(EeAStarFloodKind::Continent, 6, p);
    let mut continents: BTreeMap<u16, u64> = BTreeMap::new();
    fill.init(p.size, p.size);
    let edges = p.edges();
    for x in (0..p.size).rev() {
        for y in (0..p.size).rev() {
            if fill.id(x, y) != 0 {
                continue;
            }
            let Some(tl) = p.tile(x, y) else { continue };
            fill.corners(ffi::ee_astar_check_corners(tl.terrain_type() as i32, TERRAIN_CLIFF, x, y, edges));
            fill.run(x, y, p.size, p.size);
            let id = fill.state().next_id as u16;
            let tiles = fill.collect(id as u32);
            let h = new_object(EeAStarCmTable::Continents);
            let c = cm_io::continent(h);
            c.id = id;
            c.category = fill.state().continent.kind;
            cm_io::area(h).area_type = AREA_CONTINENT;
            create_area(h, &tiles, p.region);
            fill.finish(Some(hull_bounds(h)));
            continents.insert(id, h);
            fill.state().next_id += 1;
        }
    }
    delete_continents(r);
    resize(r, EeAStarCmTable::Continents, fill.state().next_id as usize);
    for (&id, &h) in &continents {
        let n = ee_base::sparse::ee_sparse_size(cm_io::area_tiles(h)) as c_long;
        let rec = rm(r);
        match cm_io::continent(h).category {
            CATEGORY_WATER => rec.num_water_tiles += n,
            CATEGORY_SPACE => rec.num_space_tiles += n,
            _ => rec.num_land_tiles += n,
        }
        cm_io::area(h).serial = new_serial(r);
        add_area(r, h);
        set(r, EeAStarCmTable::Continents, id as usize, h);
    }
    create_continent_grid(r, &fill, p);
}

/// `CreateContinentGrid(fill, map)`.
unsafe fn create_continent_grid(r: *mut EeAStarCmRecord, fill: &Fill, p: &Pass) {
    let size = p.size as usize;
    {
        let rec = rm(r);
        rec.width = p.size as c_ulong;
        rec.height = p.size as c_ulong;
        rec.set_grid(EeAStarCmGrid::ContinentIds, Some(size * size));
        rec.flags_width = fill.state().next_id as c_long;
        let w = rec.flags_width as usize;
        rec.set_grid(EeAStarCmGrid::TileFlags, Some(w * w));
    }
    let mut ids = vec![0u16; size * size];
    let mut kinds = vec![0u8; size * size];
    let mut terrain = vec![0i32; size * size];
    for x in 0..p.size {
        for y in 0..p.size {
            let i = x as usize * size + y as usize;
            ids[i] = fill.id(x, y) as u16;
            kinds[i] = flood::ee_astar_flood_extra(fill.f, x, y) as u8;
            terrain[i] = p.tile(x, y).map_or(-1, |t| t.terrain_type() as i32);
        }
    }
    let w = rm(r).flags_width as usize;
    let mut categories = vec![0u8; w];
    for (i, c) in categories.iter_mut().enumerate() {
        let h = at(rm(r), EeAStarCmTable::Continents, i);
        *c = if h != 0 { cm_io::continent(h).category } else { 0 };
    }
    let grid = ffi::ee_astar_continent_grid_build(
        p.size,
        ids.as_ptr(),
        kinds.as_ptr(),
        terrain.as_ptr(),
        categories.as_ptr(),
        w as i32,
    );
    crate::astar::cm_build::ee_astar_cm_apply_continent_grid(r, grid);
}

/// `CreateHills(map)`.
unsafe fn create_hills(r: *mut EeAStarCmRecord, p: &Pass) {
    let fill = Fill::new(EeAStarFloodKind::Hill, 10, p);
    let mut hills: BTreeMap<u16, u64> = BTreeMap::new();
    fill.init(p.size, p.size);
    fill.corners(false);
    let step: f32 = crate::world_field!(p.map.rts(), mF1955F4);
    for x in (0..p.size).rev() {
        for y in (0..p.size).rev() {
            let c = crate::astar::cm_query::continent_at(rm(r), x as c_long, y as c_long);
            if c == 0 {
                continue;
            }
            let category = cm_io::continent(c).category;
            if category == CATEGORY_WATER || category == CATEGORY_SPACE || fill.id(x, y) != 0 {
                continue;
            }
            let Some(tl) = p.tile(x, y) else { continue };
            let level = crate::astar::world_view::height_level(tl.height(), step);
            if tl.terrain_type() as i32 == TERRAIN_CLIFF || level < 4 {
                continue;
            }
            fill.run(x, y, p.size, p.size);
            let id = fill.state().next_id as u16;
            let tiles = fill.collect(id as u32);
            let h = new_object(EeAStarCmTable::Hills);
            let hr = cm_io::hill(h);
            hr.id = id;
            hr.continent_id = fill.state().hill.continent_id;
            cm_io::area(h).area_type = AREA_HILL;
            create_area(h, &tiles, p.region);
            fill.finish(Some(hull_bounds(h)));
            hills.insert(id, h);
            fill.state().next_id += 1;
        }
    }
    resize(r, EeAStarCmTable::Hills, fill.state().next_id as usize);
    for &h in hills.values() {
        cm_io::area(h).serial = new_serial(r);
        let (id, continent) = (cm_io::hill(h).id, cm_io::hill(h).continent_id);
        set(r, EeAStarCmTable::Hills, id as usize, h);
        let c = at(rm(r), EeAStarCmTable::Continents, continent as usize);
        if c != 0 {
            ee_astar_continent_add_id(cm_io::continent(c), EeAStarContinentList::Hills, id);
        }
        add_area(r, h);
    }
}

// ---- the path areas, forests and region ids

/// A path area fill's areas (`EEAIAStarPathAreaGoal`'s maps) and, in a kind pass, the map's path areas by tile.
struct PathAreaFill {
    fill: Fill,
    path_areas: BTreeMap<u16, u64>,
    forests: BTreeMap<u16, u64>,
    /// `x * size + y`
    areas: Vec<u64>,
    size: usize,
}

/// `EEAIAssignKindIDs`: the next kind id to every path area under the tiles and to their continents' lists.
unsafe fn assign_kind_ids(r: *mut EeAStarCmRecord, pf: &PathAreaFill, tiles: &[EePoint]) {
    let mode = pf.fill.state().path_area.mode;
    let kind_id = pf.fill.state().next_kind_id;
    let mut areas: BTreeSet<u64> = BTreeSet::new(); // the VC6 set of addresses
    for t in tiles.iter().rev() {
        let (x, y) = (t.x as u32 as usize, t.y as u32 as usize);
        let h = if x < pf.size && y < pf.size { pf.areas[pf.size * x + y] } else { 0 };
        areas.insert(h);
        let a = cm_io::path_area(h);
        match mode {
            PATH_WATER_BODY => a.water_body = kind_id,
            PATH_SPACE_REGION => a.space_region = kind_id,
            PATH_LAND_MASS => a.land_mass = kind_id,
            _ => a.surface_region = kind_id,
        }
    }
    for &h in &areas {
        let c = at(rm(r), EeAStarCmTable::Continents, cm_io::path_area(h).continent_id as usize);
        if c == 0 {
            continue;
        }
        let list = match mode {
            PATH_WATER_BODY => EeAStarContinentList::IdsBc,
            PATH_SPACE_REGION => EeAStarContinentList::IdsDc,
            PATH_LAND_MASS => EeAStarContinentList::IdsCc,
            _ => EeAStarContinentList::IdsEc,
        };
        ee_astar_continent_add_id(cm_io::continent(c), list, kind_id);
    }
}

/// `CreateAreas`: a new path area (and a forest for forest tiles) of the filled tiles, or in a kind pass the next
/// kind id; then the fill finished.
unsafe fn path_area_create(r: *mut EeAStarCmRecord, pf: &mut PathAreaFill, region: c_long) {
    let st = pf.fill.state();
    let tiles = pf.fill.collect(st.next_id as u32);
    let st = pf.fill.state();
    if st.path_area.mode == PATH_NONE {
        let id = st.next_id as u16;
        let h = new_object(EeAStarCmTable::PathAreas);
        let a = cm_io::path_area(h);
        a.id = id;
        a.continent_id = st.path_area.continent_id;
        cm_io::area(h).area_type = AREA_PATH_AREA;
        a.kind = st.path_area.kind as c_long;
        create_area(h, &tiles, region);
        pf.path_areas.insert(id, h);
        st.next_id += 1;
        if st.path_area.kind == TILE_FOREST {
            let f = new_object(EeAStarCmTable::Forests);
            let fr = cm_io::forest(f);
            fr.id = st.path_area.next_forest_id;
            fr.continent_id = st.path_area.continent_id;
            cm_io::area(f).area_type = AREA_FOREST;
            create_area(f, &tiles, region);
            let fr = cm_io::forest(f);
            fr.num_resources1 = st.path_area.resources[0] as c_long;
            fr.num_resources3 = st.path_area.resources[2] as c_long;
            fr.num_resources2 = st.path_area.resources[1] as c_long;
            ee_astar_forest_update_resource_kind(fr);
            pf.forests.insert(st.path_area.next_forest_id, f);
            st.path_area.next_forest_id = st.path_area.next_forest_id.wrapping_add(1);
        } else if st.path_area.kind == TILE_UNIT_OBSTACLE {
            cm_io::path_area(h).unit_id = st.path_area.owner_id as c_long;
        }
    } else {
        st.next_kind_id = st.next_kind_id.wrapping_add(1);
        assign_kind_ids(r, pf, &tiles);
    }
    pf.fill.finish(None);
}

/// `GetPathAreas(map, areas)`: the path area of every tile (the manager's `GetPathAreaAt`, its CNTQ records).
unsafe fn path_areas_by_tile(r: *mut EeAStarCmRecord, p: &Pass) -> Vec<u64> {
    let size = p.size as usize;
    let mut out = vec![0u64; size * size];
    for x in (0..p.size).rev() {
        for y in (0..p.size).rev() {
            let rec = rm(r);
            let i = x as usize * size + y as usize;
            out[i] = if (x as c_ulong) < rec.width && (y as c_ulong) < rec.height && p.tile(x, y).is_some() {
                cm_queries::ee_astar_cmq_area_at(r, 3, x as c_long, y as c_long)
            } else {
                0
            };
        }
    }
    out
}

/// `CreatePathAreaKind(map, kind)`.
unsafe fn create_path_area_kind(r: *mut EeAStarCmRecord, p: &Pass, kind: i32) {
    let mut pf = PathAreaFill {
        fill: Fill::new(EeAStarFloodKind::PathArea, 0xe, p),
        path_areas: BTreeMap::new(),
        forests: BTreeMap::new(),
        areas: Vec::new(),
        size: p.size as usize,
    };
    let mut flag = false;
    pf.fill.init(p.size, p.size);
    pf.fill.corners(true);
    pf.areas = path_areas_by_tile(r, p);
    let edges = p.edges();
    for x in (0..p.size).rev() {
        for y in (0..p.size).rev() {
            let Some(tl) = p.tile(x, y) else { continue };
            let h = pf.areas[x as usize * pf.size + y as usize];
            let ids = if h == 0 {
                EePathAreaIDs::default()
            } else {
                let a = cm_io::path_area(h);
                EePathAreaIDs {
                    present: true,
                    kind: a.kind as i32,
                    land_mass: a.land_mass,
                    water_body: a.water_body,
                    space_region: a.space_region,
                    surface_region: a.surface_region,
                }
            };
            let units = tile_units(&tl);
            let terrain = tl.terrain_type() as i32;
            if !ffi::ee_astar_path_area_kind_starts(
                kind,
                terrain,
                x,
                y,
                edges,
                &ids,
                units.as_ptr(),
                units.len(),
                &mut flag,
            ) {
                continue;
            }
            pf.fill.state().path_area.mode = kind;
            // terrain 3 here, as the original
            pf.fill.corners(ffi::ee_astar_check_corners(terrain, 3, x, y, edges));
            pf.fill.run(x, y, p.size, p.size);
            path_area_create(r, &mut pf, p.region);
        }
    }
    let next = pf.fill.state().next_kind_id as c_long;
    let rec = rm(r);
    match kind {
        PATH_LAND_MASS => rec.next_land_mass = next,
        PATH_WATER_BODY => rec.next_water_body = next,
        PATH_SPACE_REGION => rec.next_space_region = next,
        PATH_NON_SPACE => rec.next_surface_region = next,
        _ => {}
    }
}

/// `CreatePathAreas(map)`.
unsafe fn create_path_areas(r: *mut EeAStarCmRecord, p: &Pass) {
    let mut pf = PathAreaFill {
        fill: Fill::new(EeAStarFloodKind::PathArea, 0xe, p),
        path_areas: BTreeMap::new(),
        forests: BTreeMap::new(),
        areas: Vec::new(),
        size: 0,
    };
    pf.fill.init(p.size, p.size);
    pf.fill.corners(true);
    for x in (0..p.size).rev() {
        for y in (0..p.size).rev() {
            if crate::astar::cm_query::continent_at(rm(r), x as c_long, y as c_long) == 0 || pf.fill.id(x, y) != 0 {
                continue;
            }
            pf.fill.run(x, y, p.size, p.size);
            path_area_create(r, &mut pf, p.region);
        }
    }
    let count = pf.fill.state().next_id as u16;
    resize(r, EeAStarCmTable::PathAreas, count as usize);
    rm(r).next_path_area_id = count;
    for &h in pf.path_areas.values() {
        cm_io::area(h).serial = new_serial(r);
        let (id, continent) = (cm_io::path_area(h).id, cm_io::path_area(h).continent_id);
        set(r, EeAStarCmTable::PathAreas, id as usize, h);
        let c = at(rm(r), EeAStarCmTable::Continents, continent as usize);
        if c != 0 {
            ee_astar_continent_add_id(cm_io::continent(c), EeAStarContinentList::PathAreas, id);
        }
        add_area(r, h);
    }
    resize(r, EeAStarCmTable::Forests, pf.fill.state().path_area.next_forest_id as usize);
    for &f in pf.forests.values() {
        cm_io::area(f).serial = new_serial(r);
        let (id, continent) = (cm_io::forest(f).id, cm_io::forest(f).continent_id);
        set(r, EeAStarCmTable::Forests, id as usize, f);
        let c = at(rm(r), EeAStarCmTable::Continents, continent as usize);
        if c != 0 {
            ee_astar_continent_add_id(cm_io::continent(c), EeAStarContinentList::Forests, id);
        }
        add_area(r, f);
    }
    create_path_sectors(r, p, pf.fill.f);
    drop(pf);
    let subs = rm(r).tables().tables[EeAStarCmTable::PathSubSectors as usize].clone();
    for &s in subs.iter().rev() {
        let c = if s != 0 { at(rm(r), EeAStarCmTable::Continents, cm_io::sub(s).continent_id as usize) } else { 0 };
        if c != 0 {
            ee_astar_sector_add_neighbor(cm_io::sector(cm_io::sub(s).sector), c, true);
        }
    }
}

// ---- the path sectors and sub sectors

/// `ComputeCenterPoint(true)` of a sub sector's tiles (`U2DComputeCenterPoint`).
unsafe fn center_point(s: u64) -> EeHullPoint {
    let mut points: Vec<EePoint> = Vec::new();
    cm_io::sub_tiles(s).set.points(crate::astar::sparse::PointList::All(1), &mut points);
    let n = points.len() as c_long;
    let mut center = EeHullPoint::default();
    if points.is_empty() {
        return center;
    }
    let (mut fx, mut fy) = (0.0f32, 0.0f32);
    for p in points.iter().rev() {
        fx += p.x as c_long as f32 / n as f32;
        fy += p.y as c_long as f32 / n as f32;
    }
    center = EeHullPoint::new(fx as c_long, fy as c_long);
    let hp = |p: &EePoint| EeHullPoint::new(p.x as c_long, p.y as c_long);
    let d2 = |a: EeHullPoint, b: EeHullPoint| {
        let (dx, dy) = (a.x.wrapping_sub(b.x), a.y.wrapping_sub(b.y));
        dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy))
    };
    if !points.iter().any(|p| hp(p) == center) {
        let mut closest = hp(points.last().unwrap());
        let mut best = d2(center, closest);
        for p in points[..points.len() - 1].iter().rev() {
            let d = d2(center, hp(p));
            if d <= best {
                closest = hp(p);
                best = d;
            }
        }
        center = closest;
    }
    center
}

/// `EEAIPathSectorCreate`: a sub sector of the sorted filled tiles under a new id, in its sector and `out`.
unsafe fn sub_sector_create(r: *mut EeAStarCmRecord, fill: &Fill, sector: u64, out: &mut BTreeMap<u16, u64>) {
    let id = rm(r).new_sub_sector_id();
    let mut tiles = fill.collect(id as u32);
    let st = fill.state().sector;
    let s = new_object(EeAStarCmTable::PathSubSectors);
    tiles.sort_by(|a, b| continent_grid::point_cmp(*a, *b));
    let sr = cm_io::sub(s);
    sr.sector = sector;
    sr.id = id;
    sr.continent_id = st.continent_id;
    sr.path_area_id = st.path_area_id;
    sr.num_tiles = st.tile_count as c_long;
    ee_base::sparse::ee_sparse_insert_list(cm_io::sub_tiles(s), tiles.as_ptr(), tiles.len(), true);
    cm_io::sub(s).kind = (if !st.classify_tiles { st.path_area_kind } else { st.kind }) as c_long;
    ee_astar_sector_insert_sub(cm_io::sector(sector), id, s);
    out.insert(id, s);
    fill.finish(None);
}

/// `GetPathSector(tile)`.
fn sector_at(r: &EeAStarCmRecord, x: c_long, y: c_long) -> u64 {
    r.sector_index(x, y).map_or(0, |i| at(r, EeAStarCmTable::PathSectors, i))
}

unsafe fn sector_rect(s: u64) -> AStarRect {
    let b = cm_io::sector(s).bounds;
    AStarRect { bottom: b.bottom as i32, left: b.left as i32, right: b.right as i32, top: b.top as i32 }
}

/// The region size global (`gEEAIRegionSize`, the C++'s).
fn set_region_size(v: c_long) {
    let mut io = EeAStarCmIo { h: [0, 0], i: [v as i64, 0, 0, 0] };
    cm_io::call_host(cm_io::EE_ASTAR_CM_SET_REGION_SIZE, &mut io);
}

/// `CreatePathSectors(map, areas)`: `areas` the path area fill (its id grid).
unsafe fn create_path_sectors(r: *mut EeAStarCmRecord, p: &Pass, areas: *mut EeAStarFlood) {
    let fill = Fill::new(EeAStarFloodKind::PathSector, 0xf, p);
    let mut subs: BTreeMap<u16, u64> = BTreeMap::new();
    let mut id: u16 = 1;
    delete_path_sectors(r);
    let sizes = ffi::ee_astar_sector_sizes(p.size_class());
    set_region_size(sizes.region_size as c_long);
    fill.init(p.size, p.size);
    rm(r).init_sectors(p.size as c_long, sizes.sector_size as c_long);
    let (sx, sy) = (rm(r).sectors_x, rm(r).sectors_y);
    resize(r, EeAStarCmTable::PathSectors, (sx * sy + 1) as usize);
    for x in 0..sx {
        for y in 0..sy {
            let s = new_object(EeAStarCmTable::PathSectors);
            let sr = cm_io::sector(s);
            sr.id = id;
            id = id.wrapping_add(1);
            let rect = ffi::ee_astar_sector_rect(x as i32, y as i32, rm(r).sector_size as i32, p.size);
            sr.center = EeHullPoint::new(rect.center_x as c_long, rect.center_y as c_long);
            sr.bounds.top = rect.top as c_long;
            sr.bounds.left = rect.left as c_long;
            sr.bounds.bottom = rect.bottom as c_long;
            sr.bounds.right = rect.right as c_long;
            set(r, EeAStarCmTable::PathSectors, sr.id as usize, s);
        }
    }
    fill.corners(false);
    fill.state().sector.path_area_storage = areas as *const c_void;
    for i in (0..p.size).rev() {
        for j in (0..p.size).rev() {
            if fill.id(i, j) != 0 || p.tile(i, j).is_none() {
                continue;
            }
            let s = sector_at(rm(r), i as c_long, j as c_long);
            fill.state().sector.sector_bounds = sector_rect(s);
            fill.run(i, j, p.size, p.size);
            sub_sector_create(r, &fill, s, &mut subs);
        }
    }
    let last = rm(r).last_sub_sector_id as usize;
    resize(r, EeAStarCmTable::PathSubSectors, last + 1);
    for (&sid, &s) in &subs {
        cm_io::sub(s).location = center_point(s);
        set(r, EeAStarCmTable::PathSubSectors, sid as usize, s);
        let a = at(rm(r), EeAStarCmTable::PathAreas, cm_io::sub(s).path_area_id as usize);
        if a != 0 {
            let loc = cm_io::sub(s).location;
            let sector = sector_at(rm(r), loc.x, loc.y);
            if sector != 0 {
                ee_astar_path_area_add_sector_id(cm_io::path_area(a), cm_io::sector(sector).id);
                ee_astar_sector_add_path_area_id(cm_io::sector(sector), cm_io::path_area(a).id);
            }
        }
    }
    link_sub_sector_grid(r, &fill);
}

/// The link walks' view of the sub sectors: the id at a tile from the fill's grid (null: the manager's lookup).
struct GridView {
    r: *mut EeAStarCmRecord,
    fill: *mut EeAStarFlood,
}

extern "C" fn grid_id_at(ctx: *mut c_void, x: i32, y: i32) -> u16 {
    // SAFETY: the walk's view
    let v = unsafe { &*(ctx as *const GridView) };
    if !v.fill.is_null() {
        // SAFETY: the pass's fill
        return unsafe { flood::ee_astar_flood_id(v.fill, x, y) } as u16;
    }
    // SAFETY: the live record (read only during the walk)
    let r = unsafe { &*v.r };
    if (x as c_long as c_ulong) >= r.width || (y as c_long as c_ulong) >= r.height {
        return 0;
    }
    cm_io::sub_sector_id_at(r, x as c_long, y as c_long)
}

extern "C" fn grid_present(ctx: *mut c_void, id: u16) -> bool {
    // SAFETY: as above
    let v = unsafe { &*(ctx as *const GridView) };
    at(unsafe { &*v.r }, EeAStarCmTable::PathSubSectors, id as usize) != 0
}

unsafe fn walk(r: *mut EeAStarCmRecord, fill: *mut EeAStarFlood, x0: i32, x1: i32, y0: i32, y1: i32) {
    let view = GridView { r, fill };
    let cb = ffi::EeSubSectorGridCallbacks {
        ctx: &view as *const GridView as *mut c_void,
        id_at: grid_id_at,
        present: grid_present,
    };
    let links = ffi::ee_astar_sub_sector_links(x0, x1, y0, y1, &cb);
    crate::astar::cm_links::ee_astar_cm_apply_links(r, links);
}

/// `LinkSubSectorGrid(fill)`: the fill's ids into the grid, then every neighbouring pair linked.
unsafe fn link_sub_sector_grid(r: *mut EeAStarCmRecord, fill: &Fill) {
    let (w, h) = (rm(r).width, rm(r).height);
    rm(r).set_grid(EeAStarCmGrid::SubSectorIds, Some((h * w) as usize));
    let mut size = [0i32; 2];
    flood::ee_astar_flood_size(fill.f, size.as_mut_ptr());
    let ids = rm(r).sub_sector_ids;
    for x in 0..size[0] {
        for y in 0..size[1] {
            *ids.add((w as usize) * x as usize + y as usize) = fill.id(x, y) as u16;
        }
    }
    walk(r, fill.f, 0, size[0], 0, size[1]);
}

// ---- the scenario areas

/// `CreateScenarioAreas`: one rectangle area per scenario area with a rectangle (`ids` the scenario areas' ids).
unsafe fn create_scenario_areas(r: *mut EeAStarCmRecord, inputs: &[EeScenarioArea], ids: &[c_long], region: c_long) {
    let plan = manager::scenario_area_plan(inputs);
    if plan.is_empty() {
        return;
    }
    resize(r, EeAStarCmTable::RectangleAreas, 0);
    resize(r, EeAStarCmTable::RectangleAreas, plan.len() + 1);
    for e in &plan {
        let h = new_object(EeAStarCmTable::RectangleAreas);
        let a = cm_io::rect(h);
        a.num_land_tiles = 0;
        a.num_water_tiles = 0;
        a.num_space_tiles = 0;
        a.id = e.id;
        cm_io::area(h).serial = new_serial(r);
        cm_io::area(h).area_type = AREA_RECTANGLE;
        let a = cm_io::rect(h);
        a.scenario_area_id = ids.get(e.index as usize).copied().unwrap_or(0);
        a.rect.top = e.top as c_long;
        a.rect.bottom = e.bottom as c_long;
        a.rect.left = e.left as c_long;
        a.rect.right = e.right as c_long;
        let mut tiles = Vec::new();
        for x in e.left..=e.right {
            for y in e.top..=e.bottom {
                tiles.push(EePoint { x, y });
            }
        }
        // `Create`: the tiles and the hull (no bounds: the C++ did not update them here)
        let tile_set = cm_io::area_tiles(h) as *mut _;
        ee_base::sparse::ee_sparse_insert_list(tile_set, tiles.as_ptr(), tiles.len(), true);
        area_ops::ee_astar_area_update_hull_captured(cm_io::area(h), tile_set, region);
        // `AddRectangleArea`
        set(r, EeAStarCmTable::RectangleAreas, e.id as usize, h);
        crate::astar::cm_build::ee_astar_cm_rect_update_continents(r, h);
        add_area(r, h);
    }
}

// ---- the continental chains

struct Chains {
    r: *mut EeAStarCmRecord,
}

fn chains<'a>(ctx: *mut c_void) -> &'a mut EeAStarCmRecord {
    // SAFETY: the pass's record, borrowed by one callback at a time
    unsafe { &mut *(*(ctx as *mut Chains)).r }
}

extern "C" fn ch_count(ctx: *mut c_void) -> i32 {
    chains(ctx).tables().tables[EeAStarCmTable::Continents as usize].len() as i32
}
extern "C" fn ch_continent(ctx: *mut c_void, i: i32) -> *mut c_void {
    at(chains(ctx), EeAStarCmTable::Continents, i as usize) as usize as *mut c_void
}
extern "C" fn ch_category(_: *mut c_void, o: *mut c_void) -> u8 {
    // SAFETY: a live continent
    unsafe { cm_io::continent(o as u64).category }
}
extern "C" fn ch_id(_: *mut c_void, o: *mut c_void) -> i32 {
    // SAFETY: as above
    unsafe { cm_io::continent(o as u64).id as i32 }
}
extern "C" fn ch_get(_: *mut c_void, o: *mut c_void, kind: u32) -> u16 {
    // SAFETY: as above
    unsafe { cm_io::continent(o as u64).counts[kind as usize] }
}
extern "C" fn ch_set(_: *mut c_void, o: *mut c_void, kind: u32, id: u16) {
    // SAFETY: as above
    unsafe { cm_io::continent(o as u64).counts[kind as usize] = id };
}
extern "C" fn ch_center(_: *mut c_void, o: *mut c_void, out: *mut EePoint) {
    // SAFETY: as above; `out` writable
    unsafe {
        let c = cm_io::area(o as u64).hull_center;
        *out = EePoint { x: c.x as i32, y: c.y as i32 };
    }
}
extern "C" fn ch_num_neighbors(_: *mut c_void, o: *mut c_void) -> u32 {
    // SAFETY: as above
    unsafe { cm_io::continent(o as u64).lists() }.map_or(0, |l| l.neighbors().len() as u32)
}
extern "C" fn ch_neighbor(_: *mut c_void, o: *mut c_void, i: u32) -> *mut c_void {
    // SAFETY: as above
    unsafe { cm_io::continent(o as u64).lists() }.map_or(0, |l| l.neighbors()[i as usize]) as usize as *mut c_void
}
extern "C" fn ch_next(ctx: *mut c_void, kind: u32) -> u16 {
    let r = chains(ctx);
    r.next_ids[kind as usize] = r.next_ids[kind as usize].wrapping_add(1);
    r.next_ids[kind as usize]
}

/// `NumberContinentChains(kind)`.
unsafe fn number_chains(r: *mut EeAStarCmRecord, kind: u32) {
    let mut ctx = Chains { r };
    let cb = EeAStarChainCallbacks {
        ctx: &mut ctx as *mut Chains as *mut c_void,
        count: ch_count,
        continent: ch_continent,
        category: ch_category,
        id: ch_id,
        chain_id: ch_get,
        set_chain_id: ch_set,
        center: ch_center,
        num_neighbors: ch_num_neighbors,
        neighbor: ch_neighbor,
        next_chain_id: ch_next,
    };
    flood::number_chains(kind, &cb);
}

// ---- the choke points

/// `EEAIChokeGetTileInfo`: what the choke pass reads of a tile.
unsafe fn choke_tile_info(
    r: *mut EeAStarCmRecord,
    tl: Option<TileView<'_>>,
    x: i32,
    y: i32,
) -> crate::astar::choke::EeChokeTileInfo {
    let mut info = crate::astar::choke::EeChokeTileInfo::default();
    let Some(tl) = tl else { return info };
    info.tile = true;
    let c = at(rm(r), EeAStarCmTable::Continents, cm_io::continent_id_at(rm(r), x as c_long, y as c_long) as usize);
    info.category = cm_io::continent(c).category;
    info.continent_serial = cm_io::area(c).serial as u32;
    if info.category != CATEGORY_LAND && info.category != CATEGORY_CLIFF {
        return info;
    }
    info.resource = crate::astar::world_view::ee_astar_tile_resource_kind(tl.handle()) != 0;
    if !info.resource {
        return info;
    }
    let f = cm_queries::ee_astar_cmq_area_at(r, 1, x as c_long, y as c_long);
    let id = if f != 0 { cm_io::forest(f).id } else { 0 };
    let f = at(rm(r), EeAStarCmTable::Forests, id as usize);
    if f != 0 {
        info.forest = true;
        info.forest_serial = cm_io::area(f).serial as u32;
        info.forest_tiles = ee_base::sparse::ee_sparse_size(cm_io::area_tiles(f));
    }
    info
}

/// `EEAIBuildChokePoints(map, passes)`: the tiles classified, smoothed, flood-filled into choke points.
unsafe fn build_choke_points(r: *mut EeAStarCmRecord, p: &Pass, passes: i32) {
    let size = p.size;
    let mut infos =
        vec![crate::astar::choke::EeChokeTileInfo::default(); (size.max(0) as usize) * (size.max(0) as usize)];
    for x in (0..size).rev() {
        for y in (0..size).rev() {
            infos[(x * size + y) as usize] = choke_tile_info(r, p.tile(x, y), x, y);
        }
    }
    crate::astar::choke::ee_astar_choke_grids_build(size, passes, infos.as_ptr());
    choke_create(r, p);
    crate::astar::choke::ee_astar_choke_grids_free();
}

/// `EEAIChokeCreate`: every flagged tile not yet in a choke point flood-filled into one.
unsafe fn choke_create(r: *mut EeAStarCmRecord, p: &Pass) {
    let size = p.size;
    let fill = Fill::new(EeAStarFloodKind::Choke, 9, p);
    let mut chokes: BTreeMap<i32, u64> = BTreeMap::new();
    fill.init(size, size);
    fill.corners(false);
    for x in (0..size).rev() {
        for y in (0..size).rev() {
            let mut serial = 0u32;
            if !crate::astar::choke::ee_astar_choke_grids_tile(x * size + y, &mut serial) || fill.id(x, y) != 0 {
                continue;
            }
            fill.run(x, y, size, size);
            let number = fill.state().next_id;
            let tiles = fill.collect(number as u32);
            let h = new_object(EeAStarCmTable::ChokePoints);
            cm_io::choke(h).index = number as c_long;
            cm_io::area(h).area_type = AREA_CHOKE_POINT;
            create_area(h, &tiles, p.region);
            let mut n = 0;
            let s = flood::ee_astar_flood_serials(fill.f, &mut n);
            let serials: Vec<c_ulong> = if n == 0 {
                Vec::new()
            } else {
                std::slice::from_raw_parts(s, n).iter().map(|&v| v as c_ulong).collect()
            };
            ee_astar_choke_add_area_serials(cm_io::choke(h), serials.as_ptr(), serials.len(), false);
            if let Some(t) = tiles.first() {
                cm_io::choke(h).continent_id = cm_io::continent_id_at(rm(r), t.x as c_long, t.y as c_long);
            }
            fill.finish(Some(hull_bounds(h)));
            chokes.insert(number, h);
            fill.state().next_id += 1;
        }
    }
    resize(r, EeAStarCmTable::ChokePoints, fill.state().next_id as usize);
    for &h in chokes.values() {
        cm_io::area(h).serial = new_serial(r);
        // `AddChokePoint`
        let (index, continent) = (cm_io::choke(h).index, cm_io::choke(h).continent_id);
        set(r, EeAStarCmTable::ChokePoints, index as usize, h);
        let c = at(rm(r), EeAStarCmTable::Continents, continent as usize);
        if c != 0 {
            ee_astar_continent_add_choke_point(cm_io::continent(c), index);
        }
        add_area(r, h);
    }
}

struct Prune {
    r: *mut EeAStarCmRecord,
}

impl Prune {
    fn rec(&self) -> &EeAStarCmRecord {
        // SAFETY: the pass's record
        unsafe { &*self.r }
    }
    fn choke(&self, i: usize) -> u64 {
        at(self.rec(), EeAStarCmTable::ChokePoints, i)
    }
    fn serial(&self, choke: usize, k: usize) -> c_ulong {
        // SAFETY: a live choke point
        unsafe { (&(*cm_io::choke(self.choke(choke)).area_serials).0)[k] }
    }
}

impl PruneHost for Prune {
    fn choke_exists(&mut self, choke: usize) -> bool {
        self.choke(choke) != 0
    }
    fn area_count(&mut self, choke: usize) -> usize {
        // SAFETY: a live choke point
        unsafe { (*cm_io::choke(self.choke(choke)).area_serials).0.len() }
    }
    fn area(&mut self, choke: usize, k: usize) -> i32 {
        let a = self.rec().tables().area(self.serial(choke, k));
        if a == 0 {
            return -2;
        }
        let continents = &self.rec().tables().tables[EeAStarCmTable::Continents as usize];
        continents.iter().rposition(|&c| c == a).map_or(-1, |i| i as i32)
    }
    fn near(&mut self, edge: usize, choke: usize, k: Option<usize>) -> bool {
        let target = match k {
            None => self.choke(choke),
            Some(k) => self.rec().tables().area(self.serial(choke, k)),
        };
        let c = at(self.rec(), EeAStarCmTable::Continents, edge);
        // SAFETY: live areas
        let (a, b) = unsafe { crate::astar::area_continent::closest_points_of(c, target) }.1;
        let (dx, dy) = (a.x.wrapping_sub(b.x), a.y.wrapping_sub(b.y));
        dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) <= 1
    }
    fn remove(&mut self, choke: usize) {
        let h = self.choke(choke);
        // SAFETY: the pass's record and a live choke point
        unsafe {
            cm_queries::ee_astar_cm_remove_object_area(self.r, h);
            set(self.r, EeAStarCmTable::ChokePoints, choke, 0);
            crate::astar::objects::delete_object(EeAStarCmTable::ChokePoints, h);
        }
    }
}

/// `PruneChokePoints()`.
unsafe fn prune_choke_points(r: *mut EeAStarCmRecord) {
    let continents = rm(r).tables().tables[EeAStarCmTable::Continents as usize].clone();
    let categories: Vec<i32> =
        continents.iter().map(|&c| if c != 0 { cm_io::continent(c).category as i32 } else { -1 }).collect();
    let chokes = rm(r).tables().tables[EeAStarCmTable::ChokePoints as usize].len();
    manager::prune_choke_points(&categories, chokes, &mut Prune { r });
}

// ---- the deletes and the whole passes

unsafe fn delete_continents(r: *mut EeAStarCmRecord) {
    cm_io::delete_table(rm(r), EeAStarCmTable::Continents);
    let rec = rm(r);
    rec.num_land_tiles = 0;
    rec.num_water_tiles = 0;
    rec.num_space_tiles = 0;
}

unsafe fn delete_path_sectors(r: *mut EeAStarCmRecord) {
    cm_io::delete_table(rm(r), EeAStarCmTable::PathSectors);
    crate::astar::cm::ee_astar_cm_sectors_deleted(r);
}

/// `Reset()`: every object deleted, the tables, lists and counters emptied (not the grids).
unsafe fn reset(r: *mut EeAStarCmRecord) {
    crate::astar::cm::ee_astar_cm_clear_sector_tiles(r);
    for t in [EeAStarCmTable::ChokePoints, EeAStarCmTable::Forests, EeAStarCmTable::Hills] {
        cm_io::delete_table(rm(r), t);
    }
    delete_continents(r);
    cm_io::delete_table(rm(r), EeAStarCmTable::RectangleAreas);
    cm_io::delete_table(rm(r), EeAStarCmTable::PathAreas);
    delete_path_sectors(r);
    resize(r, EeAStarCmTable::PathSubSectors, 0);
    crate::astar::cm::ee_astar_cm_reset(r);
}

unsafe fn free_grids(r: *mut EeAStarCmRecord) {
    let rec = rm(r);
    for g in
        [EeAStarCmGrid::TileFlags, EeAStarCmGrid::ContinentIds, EeAStarCmGrid::TileValues, EeAStarCmGrid::SubSectorIds]
    {
        rec.set_grid(g, None);
    }
}

fn pass<'t>(t: &'t WorldRead, map: u64, region: c_long) -> Option<Pass<'t>> {
    // SAFETY: the caller's live map
    let m = unsafe { MapView::of(t, map) }?;
    Some(Pass { map: m, handle: map, size: m.size() as i32, region })
}

/// `Reset()`.
///
/// # Safety
/// `r` the live record, not in use elsewhere.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_reset_all(r: *mut EeAStarCmRecord) {
    reset(r);
}

/// `Shutdown()` of the manager: `Reset`, then the four grids freed.
///
/// # Safety
/// As `ee_astar_cm_reset_all`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_shutdown(r: *mut EeAStarCmRecord) {
    reset(r);
    free_grids(r);
}

/// `~EEAIContinentManager`: `Shutdown` of the game's manager `global`, then this one's tables freed.
///
/// # Safety
/// `global` the game's live record; `r` this manager's record, not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_destroy(global: *mut EeAStarCmRecord, r: *mut EeAStarCmRecord) {
    ee_astar_cm_shutdown(global);
    crate::astar::cm::ee_astar_cm_free(r);
}

/// `Clear(map)`: clearing, reset, the grids freed, the continents and hills of the map made.
///
/// # Safety
/// `r` the live record, not in use elsewhere; `map` a live `EEMap*`; the world readable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_clear(r: *mut EeAStarCmRecord, map: u64, region: c_long) {
    rm(r).clearing = true;
    reset(r);
    free_grids(r);
    let t = token();
    let Some(p) = pass(&t, map, region) else { return };
    create_continents(r, &p);
    create_hills(r, &p);
}

/// `BuildRegions(map)` after `Clear`: the tile values, path areas and sectors, choke points, region ids, scenario
/// areas (`scenario`, `scenario_ids`: the scenario manager's areas) and the six chain kinds; the work grids freed
/// (the continent ids kept when `keep_continent_ids`).
///
/// # Safety
/// As `ee_astar_cm_clear`; `scenario` and `scenario_ids` hold `count` entries.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_build_regions(
    r: *mut EeAStarCmRecord,
    map: u64,
    region: c_long,
    scenario: *const EeScenarioArea,
    scenario_ids: *const c_long,
    count: usize,
    keep_continent_ids: bool,
) {
    let t = token();
    let Some(p) = pass(&t, map, region) else { return };
    {
        let rec = rm(r);
        let n = (rec.height * rec.width) as usize;
        rec.set_grid(EeAStarCmGrid::TileValues, Some(n));
    }
    crate::astar::cm_build::ee_astar_cm_update_tile_values(r, true);
    create_path_areas(r, &p);
    // `Build(map)`: the choke points, then the pruning
    build_choke_points(r, &p, ffi::ee_astar_choke_passes(p.size_class()));
    prune_choke_points(r);
    crate::astar::cm_build::ee_astar_cm_update_tile_values(r, false);
    for kind in [PATH_WATER_BODY, PATH_LAND_MASS, PATH_SPACE_REGION, PATH_NON_SPACE] {
        create_path_area_kind(r, &p, kind);
    }
    if !scenario.is_null() && count != 0 {
        create_scenario_areas(
            r,
            std::slice::from_raw_parts(scenario, count),
            std::slice::from_raw_parts(scenario_ids, count),
            region,
        );
    }
    for kind in 1..=6 {
        number_chains(r, kind);
    }
    cm_io::ee_astar_cm_capture(r, CNTB);
    let rec = rm(r);
    rec.set_grid(EeAStarCmGrid::TileValues, None);
    if !keep_continent_ids {
        rec.set_grid(EeAStarCmGrid::ContinentIds, None);
    }
    rec.set_grid(EeAStarCmGrid::SubSectorIds, None);
    rec.checksum = 0;
    rec.clearing = false;
}

/// `Rebuild()`: the sub sectors of the first pending sector made again, linked, the path areas of its changed
/// tiles repaired, the new sub sectors filed; whether no sector is left pending.
///
/// # Safety
/// As `ee_astar_cm_clear`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_rebuild(r: *mut EeAStarCmRecord, map: u64, region: c_long) -> bool {
    let t = token();
    let Some(p) = pass(&t, map, region) else { return crate::astar::cm::ee_astar_cm_sector_tiles_count(r) == 0 };
    let fill = Fill::new(EeAStarFloodKind::PathSector, 0xf, &p);
    let mut new_subs: BTreeMap<u16, u64> = BTreeMap::new();
    let mut tiles: BTreeMap<u16, Vec<EeHullPoint>> = BTreeMap::new();
    let mut subs: Vec<u64> = Vec::new();
    fill.corners(true);
    fill.state().sector.classify_tiles = true;
    if let Some(first) = rm(r).tables().sector_tiles.first() {
        tiles.entry(first.id).or_default().extend_from_slice(&first.tiles);
    }
    // the bounds (U2DRectangle's Extend from (0, 0, 0, 0))
    let (mut top, mut left, mut bottom, mut right): (c_long, c_long, c_long, c_long) = (0, 0, 0, 0);
    if tiles.is_empty() {
        bottom = p.size as c_long;
        right = p.size as c_long;
    } else {
        for &id in tiles.keys() {
            let s = at(rm(r), EeAStarCmTable::PathSectors, id as usize);
            crate::astar::cm_links::ee_astar_cm_remove_sub_sectors(r, s);
            cm_queries::ee_astar_cm_remove_sector_tiles_of(r, s);
            let b = cm_io::sector(s).bounds;
            for (px, py) in [(b.left, b.top), (b.right, b.bottom)] {
                left = left.min(px);
                right = right.max(px);
                top = top.min(py);
                bottom = bottom.max(py);
            }
        }
    }
    fill.state().sector.tile_bounds =
        AStarRect { bottom: bottom as i32, left: left as i32, right: right as i32, top: top as i32 };
    fill.init(p.size, p.size);
    for &id in tiles.keys() {
        // the C++'s `theSector`: the loops' bounds are read from it at every test, and a fill makes it the filled
        // tile's sector
        let mut s = at(rm(r), EeAStarCmTable::PathSectors, id as usize);
        let mut x = cm_io::sector(s).bounds.right;
        while x >= cm_io::sector(s).bounds.left {
            let mut y = cm_io::sector(s).bounds.bottom;
            while y >= cm_io::sector(s).bounds.top {
                if fill.id(x as i32, y as i32) == 0 && p.tile(x as i32, y as i32).is_some() {
                    s = sector_at(rm(r), x, y);
                    fill.state().sector.sector_bounds = sector_rect(s);
                    fill.run(x as i32, y as i32, p.size, p.size);
                    sub_sector_create(r, &fill, s, &mut new_subs);
                }
                y -= 1;
            }
            x -= 1;
        }
    }
    let last = rm(r).last_sub_sector_id as usize;
    if rm(r).tables().tables[EeAStarCmTable::PathSubSectors as usize].len() <= last {
        resize(r, EeAStarCmTable::PathSubSectors, last + 1);
    }
    for (&sid, &s) in &new_subs {
        cm_io::sub(s).location = center_point(s);
        set(r, EeAStarCmTable::PathSubSectors, sid as usize, s);
        cm_io::sub(s).is_stale = true;
        subs.push(s);
    }
    relink_sub_sectors(r, &p, &tiles, &subs);
    rm(r).state = 2;
    let mut n = 0;
    let stale = flood::ee_astar_flood_stale(fill.f, &mut n);
    let stale: Vec<EePoint> = if n == 0 { Vec::new() } else { std::slice::from_raw_parts(stale, n).to_vec() };
    for t in stale.iter().rev() {
        cm_queries::ee_astar_cmq_repair_path_area(r, t.x as c_long, t.y as c_long);
    }
    drop(fill);
    for &s in subs.iter().rev() {
        cm_io::sub(s).is_stale = true;
        let loc = cm_io::sub(s).location;
        let a = cm_queries::ee_astar_cmq_area_at(r, 3, loc.x, loc.y);
        if a != 0 {
            let sector = sector_at(rm(r), loc.x, loc.y);
            if sector != 0 {
                ee_astar_sector_add_path_area_id(cm_io::sector(sector), cm_io::path_area(a).id);
                ee_astar_path_area_add_sector_id(cm_io::path_area(a), cm_io::sector(sector).id);
            }
            cm_io::sub(s).path_area_id = cm_io::path_area(a).id;
        }
        cm_io::sub(s).is_stale = false;
    }
    let rec = rm(r);
    rec.state = 0;
    rec.checksum = 0;
    cm_io::ee_astar_cm_capture(r, CNTR);
    crate::astar::cm::ee_astar_cm_sector_tiles_count(r) == 0
}

/// `RelinkSubSectors(sectors, subs)`: the new sub sectors into the id grid, then every pending sector's tiles (grown
/// by one) linked again through the manager's lookup.
unsafe fn relink_sub_sectors(
    r: *mut EeAStarCmRecord,
    p: &Pass,
    sectors: &BTreeMap<u16, Vec<EeHullPoint>>,
    subs: &[u64],
) {
    for &s in subs.iter().rev() {
        let mut points: Vec<EePoint> = Vec::new();
        cm_io::sub_tiles(s).set.points(crate::astar::sparse::PointList::All(1), &mut points);
        let ids = rm(r).sub_sector_ids;
        if ids.is_null() {
            continue;
        }
        let (w, id) = (rm(r).width as usize, cm_io::sub(s).id);
        for t in points.iter().rev() {
            *ids.add(t.x as c_long as usize * w + t.y as c_long as usize) = id;
        }
    }
    for &id in sectors.keys() {
        let s = at(rm(r), EeAStarCmTable::PathSectors, id as usize);
        let b = cm_io::sector(s).bounds;
        let size = p.size as c_long;
        let end_x = (b.right + 2).min(size);
        let end_y = (b.bottom + 2).min(size);
        let x = if b.left - 1 < 1 { 0 } else { b.left - 1 };
        let y = if b.top - 1 < 1 { 0 } else { b.top - 1 };
        walk(r, std::ptr::null_mut(), x as i32, end_x as i32, y as i32, end_y as i32);
    }
}

/// `AddSectorTiles(tiles)`: sorted and unique, then each (from the last) queued under its path sector's id.
///
/// # Safety
/// `r` the live record; `tiles` holds `n` points.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_add_sector_tiles(r: *mut EeAStarCmRecord, tiles: *const EeHullPoint, n: usize) {
    let mut list: Vec<EeHullPoint> = if n == 0 { Vec::new() } else { std::slice::from_raw_parts(tiles, n).to_vec() };
    list.sort_by(crate::astar::cm::point_cmp);
    list.dedup();
    for &t in list.iter().rev() {
        let s = sector_at(rm(r), t.x, t.y);
        if s != 0 {
            let id = cm_io::sector(s).id;
            rm(r).tables_mut().add_sector_tile(id, t);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_sector_tiles_are_sorted_and_unique_before_they_are_queued() {
        let mut v =
            vec![EeHullPoint::new(2, 1), EeHullPoint::new(1, 1), EeHullPoint::new(2, 1), EeHullPoint::new(0, 0)];
        v.sort_by(crate::astar::cm::point_cmp);
        v.dedup();
        // lower y first, then the larger x (U2DPoint's operator<)
        assert_eq!(v, vec![EeHullPoint::new(0, 0), EeHullPoint::new(2, 1), EeHullPoint::new(1, 1)]);
    }

    #[test]
    fn a_map_edge_test_reads_the_three_lines() {
        assert_eq!(AREA_CONTINENT, 1);
        assert_eq!(CNTB.to_le_bytes(), *b"CNTB");
        assert_eq!(CNTR.to_le_bytes(), *b"CNTR");
    }
}
