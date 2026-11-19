//! The continent manager's flood fills whole (`EEAIAStarMachine<S, G, EEMap>` over the continent, hill, path area,
//! path sector and choke point goals and storages, and the continental chains' graph machine): one Rust object per
//! fill with the machine's members, the node storage, the goal's fill state and the per-tile id grid the storage
//! carried. The C++ pass drives it (`EEAIContinentManager::CreateContinents` and the others, `EEAIBuildChokePoints`):
//! it asks a tile's id, runs a fill from a start tile, collects the reached tiles under an id, makes the area of them
//! (the C++ objects and their geometry stay C++) and finishes the area. The goal's answers are the Rust leaves
//! (`goals`, `area_goals`) over the C++ view (`EEAIAStarView`), noted for the capture as the C++ machine's hooks did.

use crate::astar::area_goals::{self, EeAStarPathAreaFill, EeAStarPathSectorFill};
use crate::astar::continent_grid::EePoint;
use crate::astar::ffi::{graph_view, view, CView, EeAStarNodeView, EeAStarViewCallbacks};
use crate::astar::goals::{self, EeAStarChokeFill, EeAStarContinentFill, EeAStarHillFill};
use crate::astar::graph::{self, GraphHost, GraphKind, GraphList, GraphMachineState, GraphNodeView, GraphStorage, Obj};
use crate::astar::machine::{self, Host, MachineState};
use crate::astar::search_note::*;
use crate::astar::storage::{AStarRect, GridKind, List, Storage, StorageKind};
use std::ffi::c_void;
use std::ptr;

/// Which goal a fill has.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeAStarFloodKind {
    /// `EEAIAStarContinentGoal` (machine type 6)
    Continent = 0,
    /// `EEAIAStarHillGoal` (10)
    Hill = 1,
    /// `EEAIAStarPathAreaGoal` (0xe)
    PathArea = 2,
    /// `EEAIAStarPathSectorGoal` (0xf)
    PathSector = 3,
    /// `EEAIAStarChokePointGoal` (9)
    Choke = 4,
}

/// The goal's members the pass reads and writes between fills: the next ids, and each kind's fill state (the one of
/// the fill's kind is used).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAStarFloodState {
    /// `mNextID` (continent, hill, sub sector), `mNextPathAreaID`, `mNextIndex` (choke point): 1 at the start
    pub next_id: i32,
    /// `EEAIAStarPathAreaGoal::mNextKindID`
    pub next_kind_id: u16,
    pub continent: EeAStarContinentFill,
    pub hill: EeAStarHillFill,
    pub choke: EeAStarChokeFill,
    pub path_area: EeAStarPathAreaFill,
    pub sector: EeAStarPathSectorFill,
}

impl EeAStarFloodState {
    fn new() -> EeAStarFloodState {
        EeAStarFloodState {
            next_id: 1,
            next_kind_id: 0,
            continent: EeAStarContinentFill::default(),
            hill: EeAStarHillFill::default(),
            choke: EeAStarChokeFill::default(),
            path_area: EeAStarPathAreaFill { owner_id: -1, next_forest_id: 1, check_corners: -1, ..Default::default() },
            sector: EeAStarPathSectorFill {
                path_area_storage: ptr::null(),
                classify_tiles: false,
                tile_count: 0,
                tile_bounds: AStarRect::default(),
                sector_bounds: AStarRect::default(),
                path_area_id: 0,
                path_area_kind: 0,
                continent_id: 0,
                category: 0,
                kind: 0,
                unit_id: -1,
                player_id: 0,
                wa0: 0,
                wa2: 0,
                check_corners: -1,
                stale: false,
            },
        }
    }
}

/// One fill: the machine, its storage, the goal.
pub struct EeAStarFlood {
    kind: EeAStarFloodKind,
    /// `EEAIAStarMachineBase::mType`
    machine_type: i32,
    /// the machine's map's `mSize`
    map_size: i32,
    st: Storage,
    m: MachineState,
    start: (i32, i32),
    goal: (i32, i32),
    state: EeAStarFloodState,
    /// the storage's id grid (`mTiles60[].mID`, `mPathAreaIDs`, `mSubSectorIDs`, `mChokePointIDs`), `height * x + y`
    ids: Vec<u32>,
    /// the continent tiles' kinds, the hill tiles' continents
    extra: Vec<u16>,
    /// `mTiles`: the last collected fill
    tiles: Vec<EePoint>,
    /// `EEAIAStarChokePointGoal::mAreaSerials`
    serials: Vec<u32>,
    /// `EEAIAStarPathSectorGoal::mStaleTiles`
    stale: Vec<EePoint>,
}

/// The goal and the machine's members as the machine sees them (`EEAIAStarHooks<S, G, M>`), with the C++'s notes.
struct FloodHost<'a> {
    kind: EeAStarFloodKind,
    m: &'a mut MachineState,
    start: (i32, i32),
    goal: (i32, i32),
    state: &'a mut EeAStarFloodState,
    serials: &'a mut Vec<u32>,
    stale: &'a mut Vec<EePoint>,
    view: CView<'a>,
}

impl FloodHost<'_> {
    fn accept(&mut self, x: i32, y: i32) -> bool {
        match self.kind {
            EeAStarFloodKind::Continent => goals::continent_accept(&mut self.state.continent, &mut self.view, x, y),
            EeAStarFloodKind::Hill => goals::hill_accept(&mut self.state.hill, &mut self.view, x, y),
            EeAStarFloodKind::Choke => match goals::choke_accept(&mut self.state.choke, &mut self.view, x, y) {
                Some(serial) => {
                    self.serials.push(serial);
                    true
                }
                None => false,
            },
            EeAStarFloodKind::PathArea => {
                let result = area_goals::path_area_accept(&mut self.state.path_area, &mut self.view, x, y);
                if self.state.path_area.check_corners >= 0 {
                    self.m.check_corners = self.state.path_area.check_corners != 0;
                }
                result
            }
            EeAStarFloodKind::PathSector => {
                let result = area_goals::path_sector_accept(&mut self.state.sector, &mut self.view, x, y);
                if self.state.sector.check_corners >= 0 {
                    self.m.check_corners = self.state.sector.check_corners != 0;
                }
                if self.state.sector.stale {
                    self.stale.push(EePoint { x, y });
                }
                result
            }
        }
    }
}

impl Host for FloodHost<'_> {
    fn accept_tile(&mut self, x: i32, y: i32, flag: bool) -> bool {
        let result = self.accept(x, y);
        note_bool(EV_ACCEPT, x, y, i32::from(flag), 0, 0, result)
    }
    fn get_cost(&mut self, from_x: i32, from_y: i32, x: i32, y: i32, _parent: Option<(i32, i32)>) -> i32 {
        note(EV_COST, from_x, from_y, x, y, 0, 1)
    }
    fn estimate(&mut self, from_x: i32, from_y: i32, to_x: i32, to_y: i32) -> i32 {
        note(EV_ESTIMATE, from_x, from_y, to_x, to_y, 0, 1)
    }
    fn is_goal_node(&mut self, node: Option<(i32, i32)>) -> bool {
        let (x, y) = node.unwrap_or((0, 0));
        note_bool(EV_GOAL_NODE, i32::from(node.is_some()), x, y, 0, 0, node.is_none())
    }
    fn search_done(&mut self, _current: Option<(i32, i32)>, _has_open: bool) {
        // the flood goals' SearchDone is the base's empty one; their search never fails, never takes the nearest
        note(EV_SEARCH_DONE, 0, 0, 0, 0, 0, 0);
    }
    fn search_failed(&mut self) -> bool {
        false
    }
    fn accept_nearest(&mut self) -> bool {
        false
    }
    fn is_step_limit(&mut self, _steps: i32) -> bool {
        false
    }
    fn is_rev_limit(&mut self, _revs: i32) -> bool {
        false
    }
    fn is_cheaper(&mut self, _cost: u32, _old: u32) -> bool {
        false
    }
    fn start_tile(&mut self) -> (i32, i32) {
        self.start
    }
    fn goal_tile(&mut self) -> (i32, i32) {
        self.goal
    }
    fn state(&mut self) -> MachineState {
        *self.m
    }
    fn set_state(&mut self, state: &MachineState) {
        let (w, h) = (self.m.map_width, self.m.map_height);
        *self.m = *state;
        self.m.map_width = w;
        self.m.map_height = h;
    }
}

impl EeAStarFlood {
    pub fn new(kind: EeAStarFloodKind, machine_type: i32, map_size: i32) -> EeAStarFlood {
        EeAStarFlood {
            kind,
            machine_type,
            map_size,
            st: Storage::new(StorageKind::Terrain),
            m: MachineState::default(),
            start: (0, 0),
            goal: (0, 0),
            state: EeAStarFloodState::new(),
            ids: Vec::new(),
            extra: Vec::new(),
            tiles: Vec::new(),
            serials: Vec::new(),
            stale: Vec::new(),
        }
    }

    /// `Init(width, height)`: the size, `Clear`, the storage's `PrepareGrid` (the id grid made once: `Init` stores the
    /// size before the slot compares it), empty hash tables.
    pub fn init(&mut self, width: i32, height: i32) {
        self.st.init_begin(width, height);
        let resized = self.ids.is_empty();
        if resized {
            let n = (height.wrapping_mul(width)).max(0) as usize;
            self.ids = vec![0; n];
            self.extra = vec![0; n];
        }
        self.st.prepare_grid(width, height, GridKind::Dirty, resized);
        self.st.alloc_tables();
    }

    fn index(&self, x: i32, y: i32) -> usize {
        self.st.height.wrapping_mul(x).wrapping_add(y) as usize
    }

    /// The id the fills gave the tile (0: none yet).
    pub fn id_at(&self, x: i32, y: i32) -> u32 {
        self.ids[self.index(x, y)]
    }

    /// The continent tile's kind, the hill tile's continent.
    pub fn extra_at(&self, x: i32, y: i32) -> u16 {
        self.extra[self.index(x, y)]
    }

    /// `Run()` from `start` towards `goal` (the flood goals' goal tile: the map's size), with the capture's record.
    pub fn run(&mut self, start: (i32, i32), goal: (i32, i32), cb: &EeAStarViewCallbacks) {
        self.start = start;
        self.goal = goal;
        let mut cap = CaptureRun::begin();
        if cap.on() {
            cap.i32(self.machine_type);
            cap.bool(self.m.found);
            cap.bool(self.m.check_corners);
            cap.bool(false);
            cap.i32(start.0);
            cap.i32(start.1);
            cap.i32(goal.0);
            cap.i32(goal.1);
            cap.i32(-1);
            cap.i32(-1);
            cap.i32(self.map_size);
        }
        let fresh = !self.m.found;
        if fresh {
            self.m.num_runs = 0;
            self.m.map_width = self.map_size;
            self.m.map_height = self.map_size;
            self.m.revs = 0;
            self.init(self.map_size, self.map_size);
            if self.kind == EeAStarFloodKind::Choke {
                // EEAIAStarChokePointGoal::Attach: the map's size as the grid's
                self.state.choke.width = self.map_size;
                self.state.choke.height = self.map_size;
            }
        }
        {
            let mut host = FloodHost {
                kind: self.kind,
                m: &mut self.m,
                start,
                goal,
                state: &mut self.state,
                serials: &mut self.serials,
                stale: &mut self.stale,
                view: CView(cb),
            };
            if fresh {
                machine::start(&mut self.st, &mut host);
            }
            machine::expand(&mut self.st, &mut host);
        }
        if cap.on() {
            cap.bool(self.m.found);
            cap.bool(self.m.check_corners);
            cap.i32(self.m.revs);
            cap.i32(self.m.steps);
            cap.i32(self.m.num_runs);
            cap.i32(self.m.tile_x);
            cap.i32(self.m.tile_y);
            cap.bool(false);
            cap.bool(false);
            cap.i32(start.0);
            cap.i32(start.1);
            cap.i32(goal.0);
            cap.i32(goal.1);
            capture_storage(&mut cap, &self.st);
        }
        cap.commit(TAG_TILE_RUN);
    }

    /// `CollectTiles()`: every closed, then open, node's tile gets `id` (and the continent's kind or the hill's
    /// continent) and joins the collected tiles; returns their count.
    pub fn collect(&mut self, id: u32) -> usize {
        let extra = match self.kind {
            EeAStarFloodKind::Continent => u16::from(self.state.continent.kind),
            EeAStarFloodKind::Hill => self.state.hill.continent_id,
            _ => 0,
        };
        let mut nodes = self.st.list(List::FastClosed);
        nodes.extend(self.st.list(List::FastOpen));
        for node in nodes {
            let (x, y) = (self.st.node(node).x, self.st.node(node).y);
            let i = self.index(x, y);
            self.ids[i] = id;
            if matches!(self.kind, EeAStarFloodKind::Continent | EeAStarFloodKind::Hill) {
                self.extra[i] = extra;
            }
            self.tiles.push(EePoint { x, y });
        }
        self.tiles.len()
    }

    /// The end of the goal's `Create*` after the caller made the area: the storage's dirty bounds (the area's hull
    /// bounds, or the fill's tile bounds for the path areas and sub sectors), then the fill state for the next area.
    pub fn finish(&mut self, hull_bounds: Option<AStarRect>) {
        let s = &mut self.state;
        let dirty = match self.kind {
            EeAStarFloodKind::PathArea => s.path_area.tile_bounds,
            EeAStarFloodKind::PathSector => s.sector.tile_bounds,
            _ => hull_bounds.unwrap_or_default(),
        };
        self.st.dirty = dirty;
        self.tiles.clear();
        match self.kind {
            EeAStarFloodKind::Continent => {
                s.continent.tile_count = 0;
                s.continent.kind = goals::CATEGORY_NONE;
            }
            EeAStarFloodKind::Hill => {
                s.hill.tile_count = 0;
                s.hill.continent_id = 0;
            }
            EeAStarFloodKind::Choke => {
                s.choke.tile_count = 0;
                self.serials.clear();
            }
            EeAStarFloodKind::PathArea => {
                let a = &mut s.path_area;
                a.tile_count = 0;
                a.continent_kind = 0;
                a.owner_id = -1;
                a.continent_id = 0;
                a.forest_id = 0;
                a.kind = 0;
                a.resources = [0; 3];
            }
            EeAStarFloodKind::PathSector => {
                let c = &mut s.sector;
                c.tile_count = 0;
                c.unit_id = -1;
                c.continent_id = 0;
                c.category = 0;
                c.path_area_id = 0;
                c.path_area_kind = 0;
                c.wa0 = 0;
                c.player_id = 0;
                c.kind = 0;
            }
        }
    }

    /// The choke point's area serials sorted, without repeats (`CreateChokePoint`'s `std::sort` and `std::unique`).
    pub fn unique_serials(&mut self) -> &[u32] {
        self.serials.sort_unstable();
        self.serials.dedup();
        &self.serials
    }
}

/// The storage as `CaptureOutputs` writes it: the current and the path's node, the bounds, the lists' digests.
fn capture_storage(cap: &mut CaptureRun, st: &Storage) {
    for which in 0..2 {
        let node = if which == 0 { st.current } else { st.path };
        cap.bool(node != crate::astar::storage::NIL);
        if node != crate::astar::storage::NIL {
            cap.node(&view(st, node));
        }
    }
    let b = st.bounds;
    cap.i32(b.bottom);
    cap.i32(b.left);
    cap.i32(b.right);
    cap.i32(b.top);
    cap.bool(st.has_bounds);
    for list in [List::SortedOpen, List::FastOpen, List::FastClosed, List::OpenTable, List::ClosedTable] {
        let nodes: Vec<EeAStarNodeView> = st.list(list).into_iter().map(|n| view(st, n)).collect();
        cap.nodes(&nodes);
    }
}

// ---- the continental chains (`EEAIContinentManager::NumberContinentChains` with
// `EEAIAStarGraphMachine<EEAIAStarGraphTerrainStorage, EEAIAStarGraphContinentalChainGoal, EEAIContinent>`)

/// The continents as the chain search reads and numbers them (the C++ objects, by their graph handle:
/// `AStarGraphableObjectBase*`).
#[repr(C)]
pub struct EeAStarChainCallbacks {
    pub ctx: *mut c_void,
    /// `GetContinents().size()`
    pub count: extern "C" fn(ctx: *mut c_void) -> i32,
    /// `GetContinents()[i]`'s handle, null for none
    pub continent: extern "C" fn(ctx: *mut c_void, i: i32) -> *mut c_void,
    /// `GetCategory()`, `GetID()`, `GetCount(kind)`, `SetCount(kind, id)`
    pub category: extern "C" fn(ctx: *mut c_void, object: *mut c_void) -> u8,
    pub id: extern "C" fn(ctx: *mut c_void, object: *mut c_void) -> i32,
    pub chain_id: extern "C" fn(ctx: *mut c_void, object: *mut c_void, kind: u32) -> u16,
    pub set_chain_id: extern "C" fn(ctx: *mut c_void, object: *mut c_void, kind: u32, id: u16),
    /// `GetHullCenter()` (the graph location)
    pub center: extern "C" fn(ctx: *mut c_void, object: *mut c_void, out: *mut EePoint),
    /// `GetNumNeighbors()`, `GetNeighbors()[i]`
    pub num_neighbors: extern "C" fn(ctx: *mut c_void, object: *mut c_void) -> u32,
    pub neighbor: extern "C" fn(ctx: *mut c_void, object: *mut c_void, i: u32) -> *mut c_void,
    /// the manager's `NextChainID(kind)`
    pub next_chain_id: extern "C" fn(ctx: *mut c_void, kind: u32) -> u16,
}

const CHAIN_MACHINE_TYPE: i32 = 7;
const CHAIN_KINDS: u32 = 7;
const CATEGORY_LAND: u8 = 1;
// the chain kinds (`EEAIChainKind`)
const CHAIN_LAND_CLIFF: u32 = 1;
const CHAIN_LAND_WATER: u32 = 2;
const CHAIN_LAND_WATER_CLIFF: u32 = 3;
const CHAIN_LAND_SPACE: u32 = 4;
const CHAIN_LAND_WATER_SPACE: u32 = 5;
const CHAIN_LAND_CLIFF_SPACE: u32 = 6;

/// The chain goal's members (`EEAIAStarGraphContinentalChainGoal` over its base).
struct ChainGoal {
    start_tile: (i32, i32),
    start: Obj,
    /// `mKinds`: may the chain cross a continent of the category
    kinds: [bool; 5],
    /// `mCount`
    count: i32,
}

struct ChainHost<'a> {
    goal: &'a mut ChainGoal,
    m: &'a mut GraphMachineState,
    cb: &'a EeAStarChainCallbacks,
}

impl ChainHost<'_> {
    fn obj(o: Obj) -> *mut c_void {
        o as *mut c_void
    }
    fn id(&self, o: Obj) -> i32 {
        (self.cb.id)(self.cb.ctx, Self::obj(o))
    }
}

impl GraphHost for ChainHost<'_> {
    fn start_object(&mut self) -> Obj {
        let start = self.goal.start;
        let id = if start != 0 { self.id(start) } else { -1 };
        note(EV_GRAPH_START, 0, 0, 0, 0, 0, id);
        start
    }
    fn goal_id(&mut self) -> i32 {
        0
    }
    fn estimate(&mut self, from_x: i32, from_y: i32, to_x: i32, to_y: i32) -> i32 {
        note(EV_GRAPH_ESTIMATE, from_x, from_y, to_x, to_y, 0, 1)
    }
    fn is_goal_node(&mut self, node: Option<&GraphNodeView>) -> bool {
        note_bool(EV_GRAPH_GOAL_NODE, node.map_or(-1, |n| n.index), 0, 0, 0, 0, node.is_none())
    }
    fn object_id(&mut self, object: Obj) -> i32 {
        self.id(object)
    }
    fn num_neighbors(&mut self, object: Obj) -> u32 {
        (self.cb.num_neighbors)(self.cb.ctx, Self::obj(object))
    }
    fn neighbor(&mut self, object: Obj, i: u32) -> Obj {
        (self.cb.neighbor)(self.cb.ctx, Self::obj(object), i) as Obj
    }
    fn graph_location(&mut self, object: Obj) -> (i32, i32) {
        let mut p = EePoint::default();
        (self.cb.center)(self.cb.ctx, Self::obj(object), &mut p);
        (p.x, p.y)
    }
    fn can_enter(&mut self, neighbor: Obj, from: Obj) -> bool {
        // AcceptNeighbor: the same continent, or one of an allowed category; each accepted step counted
        let mut result = neighbor == from;
        if !result {
            let category = (self.cb.category)(self.cb.ctx, Self::obj(neighbor));
            result = self.goal.kinds[category as usize];
        }
        if result {
            self.goal.count = self.goal.count.wrapping_add(1);
        }
        let (n, f) = (self.id(neighbor), self.id(from));
        note_bool(EV_GRAPH_ENTER, n, f, 0, 0, 0, result)
    }
    fn step_cost(&mut self, parent: &GraphNodeView, object: Obj) -> i32 {
        let id = self.id(object);
        note(EV_GRAPH_COST, parent.index, id, 0, 0, 0, 1)
    }
    fn search_done(&mut self, _current: Option<&GraphNodeView>, _has_open: bool) {
        note(EV_GRAPH_SEARCH_DONE, 0, 0, 0, 0, 0, 0);
    }
    fn search_failed(&mut self) -> bool {
        false
    }
    fn rev_limit(&mut self) -> i32 {
        -1
    }
    fn max_revs(&mut self) -> i32 {
        -1
    }
    fn start_tile(&mut self) -> (i32, i32) {
        self.goal.start_tile
    }
    fn goal_tile(&mut self) -> (i32, i32) {
        (0, 0)
    }
    fn state(&mut self) -> GraphMachineState {
        *self.m
    }
    fn set_state(&mut self, state: &GraphMachineState) {
        *self.m = *state;
    }
}

/// `NumberContinentChains(kind)`: one search from every land continent (the last first) without a chain id of that
/// kind yet; the continents it reaches (closed, then open) get the kind's next id.
pub fn number_chains(kind: u32, cb: &EeAStarChainCallbacks) {
    let node_count = (cb.count)(cb.ctx);
    let mut st = GraphStorage::new(GraphKind::Terrain);
    st.init(node_count);
    let mut m = GraphMachineState::default();
    let mut goal = ChainGoal { start_tile: (0, 0), start: 0, kinds: [false; 5], count: 0 };
    if matches!(kind, CHAIN_LAND_CLIFF | CHAIN_LAND_WATER_CLIFF | CHAIN_LAND_CLIFF_SPACE) {
        goal.kinds[3] = true;
    }
    if matches!(kind, CHAIN_LAND_WATER | CHAIN_LAND_WATER_CLIFF | CHAIN_LAND_WATER_SPACE) {
        goal.kinds[2] = true;
    }
    if matches!(kind, CHAIN_LAND_SPACE | CHAIN_LAND_WATER_SPACE | CHAIN_LAND_CLIFF_SPACE) {
        goal.kinds[4] = true;
    }
    goal.kinds[1] = true;
    for i in (0..node_count).rev() {
        let continent = (cb.continent)(cb.ctx, i);
        if continent.is_null() || (cb.category)(cb.ctx, continent) != CATEGORY_LAND {
            continue;
        }
        let chained = if kind < CHAIN_KINDS { (cb.chain_id)(cb.ctx, continent, kind) } else { 0 };
        if chained != 0 {
            continue;
        }
        let id = (cb.id)(cb.ctx, continent);
        let mut center = EePoint::default();
        (cb.center)(cb.ctx, continent, &mut center);
        goal.start_tile = (center.x, center.y);
        goal.start = continent as Obj;
        chain_run(&mut st, &mut m, &mut goal, id, node_count, cb);
        // NumberChain
        let chain = if kind < CHAIN_KINDS { (cb.next_chain_id)(cb.ctx, kind) } else { 0 };
        for list in [GraphList::Closed, GraphList::FastOpen] {
            for node in st.list(list) {
                if kind < CHAIN_KINDS {
                    (cb.set_chain_id)(cb.ctx, st.node(node).object as *mut c_void, kind, chain);
                }
            }
        }
    }
}

/// The graph machine's `Run()` with the capture's record (ASGR).
fn chain_run(
    st: &mut GraphStorage,
    m: &mut GraphMachineState,
    goal: &mut ChainGoal,
    goal_sector: i32,
    node_count: i32,
    cb: &EeAStarChainCallbacks,
) {
    let mut cap = CaptureRun::begin();
    if cap.on() {
        cap.i32(CHAIN_MACHINE_TYPE);
        cap.bool(m.found);
        cap.bool(false);
        cap.i32(goal.start_tile.0);
        cap.i32(goal.start_tile.1);
        cap.i32(0);
        cap.i32(0);
        cap.i32(0);
        cap.i32(goal_sector);
        cap.i32(0);
        cap.i32(-1);
        cap.i32(-1);
        cap.i32(node_count);
    }
    let fresh = !m.found;
    if fresh {
        m.num_runs = 0;
        m.revs = 0;
        st.init(node_count);
    }
    {
        let mut host = ChainHost { goal, m, cb };
        if fresh {
            graph::start(st, &mut host);
        }
        graph::expand(st, &mut host);
    }
    if cap.on() {
        cap.bool(m.found);
        cap.i32(m.revs);
        cap.i32(m.steps);
        cap.i32(m.num_runs);
        cap.i32(m.tile_x);
        cap.i32(m.tile_y);
        cap.i32(m.sector);
        cap.bool(false);
        cap.bool(false);
        capture_graph_storage(&mut cap, st);
    }
    cap.commit(TAG_GRAPH_RUN);
}

/// `EEAIAStarGraphStorage::CaptureNodes`
pub(crate) fn capture_graph_storage(cap: &mut CaptureRun, st: &GraphStorage) {
    for which in 0..2 {
        let node = if which == 0 { st.current } else { st.path };
        cap.bool(node != graph::NIL);
        if node != graph::NIL {
            cap.node(&graph_view(st, node));
        }
    }
    for list in [GraphList::Closed, GraphList::ClosedTable, GraphList::SortedOpen, GraphList::OpenTable, GraphList::FastOpen]
    {
        let nodes: Vec<EeAStarNodeView> = st.list(list).into_iter().map(|n| graph_view(st, n)).collect();
        cap.nodes(&nodes);
    }
}

// ---- the C interface

/// A new fill of `kind` (its machine's `mType`) on a map of `map_size`.
#[no_mangle]
pub extern "C" fn ee_astar_flood_new(kind: EeAStarFloodKind, machine_type: i32, map_size: i32) -> *mut EeAStarFlood {
    Box::into_raw(Box::new(EeAStarFlood::new(kind, machine_type, map_size)))
}

/// # Safety
/// `f` from `ee_astar_flood_new` (or null), not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_flood_free(f: *mut EeAStarFlood) {
    if !f.is_null() {
        drop(unsafe { Box::from_raw(f) });
    }
}

/// # Safety
/// `f` from `ee_astar_flood_new`, not in use elsewhere.
unsafe fn fl<'a>(f: *mut EeAStarFlood) -> &'a mut EeAStarFlood {
    // SAFETY: the caller's contract
    unsafe { &mut *f }
}

/// `Init(width, height)`.
///
/// # Safety
/// As `fl`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_flood_init(f: *mut EeAStarFlood, width: i32, height: i32) {
    unsafe { fl(f) }.init(width, height);
}

/// The id the fills gave tile (x, y) (on the grid).
///
/// # Safety
/// `f` from `ee_astar_flood_new`, initialised (a fill's view may ask it while another fill runs).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_flood_id(f: *const EeAStarFlood, x: i32, y: i32) -> u32 {
    unsafe { &*f }.id_at(x, y)
}

/// The continent tile's kind, the hill tile's continent.
///
/// # Safety
/// As `ee_astar_flood_id`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_flood_extra(f: *const EeAStarFlood, x: i32, y: i32) -> u16 {
    unsafe { &*f }.extra_at(x, y)
}

/// The grid's width and height (`GetWidth`, `GetHeight`).
///
/// # Safety
/// As `fl`; `out` holds two writable values.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_flood_size(f: *const EeAStarFlood, out: *mut i32) {
    let f = unsafe { &*f };
    unsafe {
        *out = f.st.width;
        *out.add(1) = f.st.height;
    }
}

/// The goal's members (the next ids, the fill states), read and written by the pass between fills.
///
/// # Safety
/// As `fl`; the pointer lives as long as the fill.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_flood_state(f: *mut EeAStarFlood) -> *mut EeAStarFloodState {
    &mut unsafe { fl(f) }.state
}

/// The machine's `mCheckCorners`.
///
/// # Safety
/// As `fl`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_flood_set_check_corners(f: *mut EeAStarFlood, on: bool) {
    unsafe { fl(f) }.m.check_corners = on;
}

/// `Run()` from (x, y) towards (goal_x, goal_y) over the view.
///
/// # Safety
/// As `fl`; `view` valid, its functions callable (they may read other fills, never this one).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_flood_run(
    f: *mut EeAStarFlood,
    x: i32,
    y: i32,
    goal_x: i32,
    goal_y: i32,
    cb: *const EeAStarViewCallbacks,
) {
    unsafe { fl(f) }.run((x, y), (goal_x, goal_y), unsafe { &*cb });
}

/// `CollectTiles()` under `id`; the tiles stay readable until `ee_astar_flood_finish`.
///
/// # Safety
/// As `fl`; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_flood_collect(f: *mut EeAStarFlood, id: u32, count: *mut usize) -> *const EePoint {
    let f = unsafe { fl(f) };
    let n = f.collect(id);
    unsafe { *count = n };
    f.tiles.as_ptr()
}

/// The end of the goal's `Create*`: `hull_bounds` the area's hull bounds (null: the fill's tile bounds).
///
/// # Safety
/// As `fl`; `hull_bounds` null or readable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_flood_finish(f: *mut EeAStarFlood, hull_bounds: *const AStarRect) {
    let bounds = if hull_bounds.is_null() { None } else { Some(unsafe { *hull_bounds }) };
    unsafe { fl(f) }.finish(bounds);
}

/// The choke point's area serials, sorted and without repeats.
///
/// # Safety
/// As `fl`; `count` writable; the serials stay readable until `ee_astar_flood_finish`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_flood_serials(f: *mut EeAStarFlood, count: *mut usize) -> *const u32 {
    let s = unsafe { fl(f) }.unique_serials();
    unsafe { *count = s.len() };
    s.as_ptr()
}

/// `EEAIAStarPathSectorGoal::mStaleTiles`.
///
/// # Safety
/// As `fl`; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_flood_stale(f: *mut EeAStarFlood, count: *mut usize) -> *const EePoint {
    let f = unsafe { fl(f) };
    unsafe { *count = f.stale.len() };
    f.stale.as_ptr()
}

/// `EEAIContinentManager::NumberContinentChains(kind)`.
///
/// # Safety
/// `cb` valid, its functions callable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_number_chains(kind: u32, cb: *const EeAStarChainCallbacks) {
    number_chains(kind, unsafe { &*cb });
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::astar::goals::View;
    use crate::astar::area_goals::{EeAStarBlocker, EeAStarContinentInfo, EeAStarPathAreaInfo};
    use crate::astar::modifiers::{Skip, Unit};
    use std::cell::RefCell;

    /// A square map of rows of terrain digits ('0' land, '1' deep water, '9' cliff).
    struct Map {
        rows: Vec<&'static [u8]>,
    }

    impl Map {
        fn n(&self) -> i32 {
            self.rows.len() as i32
        }
        fn terrain_at(&self, x: i32, y: i32) -> i32 {
            i32::from(self.rows[y as usize][x as usize] - b'0')
        }
    }

    impl View for Map {
        fn has_tile(&mut self, x: i32, y: i32) -> bool {
            x >= 0 && y >= 0 && x < self.n() && y < self.n()
        }
        fn terrain(&mut self, x: i32, y: i32) -> i32 {
            self.terrain_at(x, y)
        }
        fn is_edge(&mut self, _x: i32, _y: i32) -> bool {
            false
        }
        fn height_level(&mut self, _x: i32, _y: i32) -> i32 {
            0
        }
        fn continent_id(&mut self, _x: i32, _y: i32) -> u16 {
            1
        }
        fn choke_tile(&mut self, _index: i32) -> Option<u32> {
            None
        }
    }

    thread_local! {
        static MAP: RefCell<Option<Map>> = const { RefCell::new(None) };
    }

    extern "C" fn has_tile(_: *mut c_void, x: i32, y: i32) -> bool {
        MAP.with(|m| m.borrow_mut().as_mut().unwrap().has_tile(x, y))
    }
    extern "C" fn terrain(_: *mut c_void, x: i32, y: i32) -> i32 {
        MAP.with(|m| m.borrow().as_ref().unwrap().terrain_at(x, y))
    }
    extern "C" fn is_edge(_: *mut c_void, _x: i32, _y: i32) -> bool {
        false
    }

    extern "C" fn no_i(_: *mut c_void, _: i32, _: i32) -> i32 {
        unreachable!()
    }
    extern "C" fn no_b(_: *mut c_void, _: i32, _: i32) -> bool {
        unreachable!()
    }
    extern "C" fn no_u16(_: *mut c_void, _: i32, _: i32) -> u16 {
        unreachable!()
    }
    extern "C" fn no_choke(_: *mut c_void, _: i32, _: *mut u32) -> bool {
        unreachable!()
    }
    extern "C" fn no_ui(_: *mut c_void, _: Unit) -> i32 {
        unreachable!()
    }
    extern "C" fn no_ub(_: *mut c_void, _: Unit) -> bool {
        unreachable!()
    }
    extern "C" fn no_ut(_: *mut c_void, _: Unit, _: i32) -> bool {
        unreachable!()
    }
    extern "C" fn no_uxy(_: *mut c_void, _: Unit, _: i32, _: i32) -> i32 {
        unreachable!()
    }
    extern "C" fn no_pass(_: *mut c_void, _: Unit, _: i32, _: i32, _: Skip) -> bool {
        unreachable!()
    }
    extern "C" fn no_step(_: *mut c_void, _: Unit, _: bool, _: i32, _: i32) -> bool {
        unreachable!()
    }
    extern "C" fn no_rand(_: *mut c_void, _: f32) -> bool {
        unreachable!()
    }
    extern "C" fn no_probe(_: *mut c_void, _: Unit, _: u16, _: i32, _: i32) -> bool {
        unreachable!()
    }
    extern "C" fn no_enter(_: *mut c_void, _: *mut c_void, _: i32, _: i32, _: bool) -> bool {
        unreachable!()
    }
    #[allow(clippy::too_many_arguments)]
    extern "C" fn no_cost(_: *mut c_void, _: *mut c_void, _: i32, _: i32, _: i32, _: i32, _: i32, _: i32) -> i32 {
        unreachable!()
    }
    extern "C" fn no_cont(_: *mut c_void, _: i32, _: i32, _: *mut EeAStarContinentInfo) {
        unreachable!()
    }
    extern "C" fn no_area_at(_: *mut c_void, _: i32, _: i32, _: *mut EeAStarPathAreaInfo) {
        unreachable!()
    }
    extern "C" fn no_area(_: *mut c_void, _: u16, _: *mut EeAStarPathAreaInfo) {
        unreachable!()
    }
    extern "C" fn no_stored(_: *mut c_void, _: *const c_void, _: i32, _: i32) -> u16 {
        unreachable!()
    }
    extern "C" fn no_blocker(_: *mut c_void, _: i32, _: i32, _: *mut EeAStarBlocker) {
        unreachable!()
    }
    extern "C" fn no_owner(_: *mut c_void, _: i32, _: i32, _: i32) -> i32 {
        unreachable!()
    }
    extern "C" fn no_wall(_: *mut c_void, _: i32, _: i32, _: i32) -> bool {
        unreachable!()
    }

    /// The view of the test map: the continent fill's queries; every other one is never asked.
    fn callbacks() -> EeAStarViewCallbacks {
        EeAStarViewCallbacks {
            ctx: ptr::null_mut(),
            has_tile,
            terrain,
            is_edge,
            height_level: no_i,
            continent_id: no_u16,
            choke_tile: no_choke,
            ai_value: no_i,
            tile_flag: no_i,
            can_move_on: no_ut,
            are_passable: no_pass,
            formation_rank: no_ui,
            step_blocker: no_step,
            unit_region_state: no_i,
            epoch: no_ui,
            is_alone: no_ub,
            water_state: no_ui,
            is_sea_going: no_ub,
            has_hill: no_b,
            random_at_most: no_rand,
            crowd_cost: no_uxy,
            is_wide: no_ub,
            ignores_formation: no_ub,
            rank_probe_blocked: no_probe,
            can_enter: no_enter,
            modifier_cost: no_cost,
            has_units: no_b,
            continent_at: no_cont,
            tile_value: no_i,
            path_area_at: no_area_at,
            path_area: no_area,
            stored_path_area_id: no_stored,
            resource_kind: no_i,
            has_blocker: no_b,
            first_blocker_id: no_i,
            first_blocker: no_blocker,
            first_obstacle_id: no_i,
            own_wall_first: no_wall,
            area_gate_owner: no_owner,
            sector_gate_owner: no_owner,
            live_dock: no_b,
            live_space_dock: no_b,
        }
    }

    fn continents(rows: Vec<&'static [u8]>) -> (EeAStarFlood, Vec<(u32, u8, Vec<EePoint>)>) {
        let n = rows.len() as i32;
        MAP.with(|m| *m.borrow_mut() = Some(Map { rows }));
        let cb = callbacks();
        let mut f = EeAStarFlood::new(EeAStarFloodKind::Continent, 6, n);
        f.init(n, n);
        let mut made = Vec::new();
        for x in (0..n).rev() {
            for y in (0..n).rev() {
                if f.id_at(x, y) != 0 {
                    continue;
                }
                f.run((x, y), (n, n), &cb);
                let id = f.state.next_id as u32;
                f.collect(id);
                let mut tiles = f.tiles.clone();
                tiles.sort_by_key(|p| (p.x, p.y));
                // the hull's bounds: the C++ clears the flags of the next fill there, grown by one
                let mut bounds = AStarRect { bottom: tiles[0].y, left: tiles[0].x, right: tiles[0].x, top: tiles[0].y };
                for p in &tiles {
                    bounds.extend(p.x, p.y);
                }
                made.push((id, f.state.continent.kind, tiles));
                f.state.next_id += 1;
                f.finish(Some(bounds));
            }
        }
        (f, made)
    }

    #[test]
    fn a_continent_fill_takes_each_land_and_water_body_once() {
        let (f, made) = continents(vec![b"0011", b"0011", b"1100", b"1100"]);
        // the last tile first: (3, 3) is land; diagonal steps join the two land squares and the two water ones
        assert_eq!(made.len(), 2);
        assert_eq!(made[0].1, goals::CATEGORY_LAND);
        assert_eq!(made[0].2.len(), 8);
        assert_eq!(made[1].1, goals::CATEGORY_WATER);
        assert_eq!(made[1].2.len(), 8);
        for x in 0..4 {
            for y in 0..4 {
                let land = MAP.with(|m| m.borrow().as_ref().unwrap().terrain_at(x, y)) == 0;
                assert_eq!(f.id_at(x, y), if land { 1 } else { 2 }, "tile ({x}, {y})");
                assert_eq!(f.extra_at(x, y), u16::from(if land { goals::CATEGORY_LAND } else { goals::CATEGORY_WATER }));
            }
        }
    }

    #[test]
    fn the_notes_follow_the_cpp_hooks() {
        TEST_NOTES.with(|n| n.borrow_mut().clear());
        let _ = continents(vec![b"0"]);
        let notes = TEST_NOTES.with(|n| n.borrow().clone());
        let kinds: Vec<u8> = notes.iter().map(|n| n.0).collect();
        // Start: the start tile's accept, its estimate; Expand: the goal test of the start node (not the goal), the
        // eight neighbours (four sides, then the diagonals: corner checks off), the next pop (none: the goal)
        assert_eq!(kinds[0], EV_ACCEPT);
        assert_eq!(notes[0].1, [0, 0, 0, 0, 0]);
        assert_eq!(notes[0].2, 1);
        assert_eq!(kinds[1], EV_ESTIMATE);
        assert_eq!(kinds[2], EV_GOAL_NODE);
        assert_eq!(notes[2].2, 0);
        // the empty open list is the goal; the search is done (not failed, not the nearest)
        let n = notes.len();
        assert_eq!(kinds[n - 2], EV_GOAL_NODE);
        assert_eq!(notes[n - 2], (EV_GOAL_NODE, [0, 0, 0, 0, 0], 1));
        assert_eq!(notes[n - 1], (EV_SEARCH_DONE, [0, 0, 0, 0, 0], 0));
        // the off-map neighbours are never asked
        assert!(notes.iter().all(|n| n.0 != EV_ACCEPT || n.1[0] == 0 && n.1[1] == 0));
    }

    #[test]
    fn finish_resets_the_fill_for_the_next_area() {
        let mut f = EeAStarFlood::new(EeAStarFloodKind::PathSector, 0xf, 4);
        f.state.sector.unit_id = 7;
        f.state.sector.tile_count = 3;
        f.state.sector.tile_bounds = AStarRect { bottom: 2, left: 1, right: 3, top: 0 };
        f.tiles.push(EePoint { x: 1, y: 1 });
        f.finish(None);
        assert_eq!(f.state.sector.unit_id, -1);
        assert_eq!(f.state.sector.tile_count, 0);
        assert!(f.tiles.is_empty());
        assert_eq!(f.st.dirty, AStarRect { bottom: 2, left: 1, right: 3, top: 0 });
        let mut c = EeAStarFlood::new(EeAStarFloodKind::Choke, 9, 4);
        c.serials = vec![5, 3, 5, 1];
        assert_eq!(c.unique_serials(), &[1, 3, 5]);
        c.finish(Some(AStarRect { bottom: 9, left: 0, right: 0, top: 0 }));
        assert!(c.serials.is_empty());
        assert_eq!(c.st.dirty.bottom, 9);
    }

    struct Chains {
        // per continent: category, neighbours, chain id
        cats: Vec<u8>,
        neighbors: Vec<Vec<usize>>,
        ids: RefCell<Vec<u16>>,
        next: RefCell<u16>,
    }

    fn ch(ctx: *mut c_void) -> &'static Chains {
        unsafe { &*(ctx as *const Chains) }
    }
    /// a continent's handle is its id (1..): the table's entry 0 is null, as the manager's
    fn ix(o: *mut c_void) -> usize {
        o as usize - 1
    }
    extern "C" fn c_count(ctx: *mut c_void) -> i32 {
        ch(ctx).cats.len() as i32 + 1
    }
    extern "C" fn c_continent(_: *mut c_void, i: i32) -> *mut c_void {
        i as usize as *mut c_void
    }
    extern "C" fn c_category(ctx: *mut c_void, o: *mut c_void) -> u8 {
        ch(ctx).cats[ix(o)]
    }
    extern "C" fn c_id(_: *mut c_void, o: *mut c_void) -> i32 {
        o as usize as i32
    }
    extern "C" fn c_chain_id(ctx: *mut c_void, o: *mut c_void, _kind: u32) -> u16 {
        ch(ctx).ids.borrow()[ix(o)]
    }
    extern "C" fn c_set_chain_id(ctx: *mut c_void, o: *mut c_void, _kind: u32, id: u16) {
        ch(ctx).ids.borrow_mut()[ix(o)] = id;
    }
    extern "C" fn c_center(_: *mut c_void, o: *mut c_void, out: *mut EePoint) {
        unsafe { *out = EePoint { x: ix(o) as i32 * 10, y: 0 } };
    }
    extern "C" fn c_num(ctx: *mut c_void, o: *mut c_void) -> u32 {
        ch(ctx).neighbors[ix(o)].len() as u32
    }
    extern "C" fn c_neighbor(ctx: *mut c_void, o: *mut c_void, i: u32) -> *mut c_void {
        (ch(ctx).neighbors[ix(o)][i as usize] + 1) as *mut c_void
    }
    extern "C" fn c_next(ctx: *mut c_void, _kind: u32) -> u16 {
        let mut n = ch(ctx).next.borrow_mut();
        *n += 1;
        *n
    }

    #[test]
    fn chains_cross_only_the_kinds_allowed() {
        // 1 land - 2 water - 3 land, 4 land alone (no neighbour)
        let chains = Chains {
            cats: vec![1, 2, 1, 1],
            neighbors: vec![vec![1], vec![0, 2], vec![1], vec![]],
            ids: RefCell::new(vec![0; 4]),
            next: RefCell::new(0),
        };
        let cb = EeAStarChainCallbacks {
            ctx: &chains as *const Chains as *mut c_void,
            count: c_count,
            continent: c_continent,
            category: c_category,
            id: c_id,
            chain_id: c_chain_id,
            set_chain_id: c_set_chain_id,
            center: c_center,
            num_neighbors: c_num,
            neighbor: c_neighbor,
            next_chain_id: c_next,
        };
        number_chains(CHAIN_LAND_CLIFF, &cb);
        // the last land continent first: 4 alone gets 1, then 3 (water not allowed) 2, then 1 gets 3
        assert_eq!(*chains.ids.borrow(), vec![3, 0, 2, 1]);
        chains.ids.borrow_mut().fill(0);
        *chains.next.borrow_mut() = 0;
        number_chains(CHAIN_LAND_WATER, &cb);
        // across the water: 3, 2 and 1 share a chain
        assert_eq!(*chains.ids.borrow(), vec![2, 2, 2, 1]);
    }
}
