//! The path manager (`EEPathManager`, EEPathManager.cpp), whole but for the bounded searches: the searches in
//! progress (the nodes, `mPaths` and `mAutoPaths`), the spare machines, the units waiting for the map (`mPending`),
//! the counters, the request record (`pm_record.rs`), and every decision over them as the C++ bodies in their order:
//!
//! - the queues: `Update`, `UpdateAutoPaths`, `UpdatePaths` (with the prunes of the record), `CancelPaths`,
//!   `CancelAutoPaths`, `CancelAutoPathRequest`, `CancelAutoPath`, `CompletePath`, `CompleteAutoPath`,
//!   `HasAutoPath`, `IsPathDone`, `GetPathGoal`, `FindPath(unit)`, `FreePaths`, `FreeMachines`, `Reset`, `Clear`;
//! - the machines' pools: `NewPathMachine`, `NewGraphMachine`, `ReleasePathMachine`, `ReleaseGraphMachine`;
//! - the requests: `RequestPath`, `RequestGroupPath`, `StartGraphPath`, `RequestUnitPath`, `FinishGraphPath`,
//!   `SetGraphRevs`, `SetPathRevs`, `RequestPendingPaths`, `SetRegionUpdates`, `UpdateRegions`;
//! - the saved game's parts (save 0.256, 0.276, 0.277, 0.278; their layouts unchanged): `SaveRunningState`,
//!   `LoadRunningState`, `SaveSearches`, `LoadSearches`, `SaveGraphSearches`, `LoadGraphSearches` and the loaded
//!   game's first tick (the searches back in their queues), a request's record;
//! - the dump of EE-SIM (`DumpSim`'s lines but the bounded machine's).
//!
//! The C++ keeps the objects: the machines (`EEAIAStarMachine`, `EEAIAStarGraphMachine` with their C++ goals and
//! modifiers), the units, the requests; and the bounded machine's searches (`FindPath`, `FindBoundedPath`,
//! `TryShortPath`, the finished tile search's `FinishPath` with its splicing), `RequestUnitPaths`' goals, the
//! modifiers a loaded graph search gets back. Rust asks it everything through one host ([`PathHost`], the C++
//! `EEPathManagerHost`'s operations): handles are the C++ pointers as `u64`, a node's unit its unit id (the
//! `EEUnitHandle` it was).
//!
//! The nodes are a slab with the queues of their indices, so a call back into the manager while a node is out of
//! its queue's front (the original's hazard) sees what the C++ saw. `long` is the Windows build's `i32`, the game
//! time `unsigned long` (`u32`). No random draw, no float but the host's.

use std::collections::VecDeque;
use std::ffi::c_void;

use crate::astar::pm_record::PathRecord;

/// `EEUnitHandle::kNone`: no unit.
pub const EE_PATH_NO_UNIT: u32 = 0xffff_ffff;
/// The spare machines kept (`ReleasePathMachine`, `ReleaseGraphMachine`).
const SPARE_PATH_MACHINES: usize = 0x23;
const SPARE_GRAPH_MACHINES: usize = 0x19;
/// A search's first step budget per pass, and the one it grows to (`UpdatePaths`).
const STEP_LIMIT: i64 = 0xf;
const STEP_BUDGET: i64 = 0x20;

/// One host call's values: handles in `h`, the arguments and answers in `i`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EePathIo {
    pub h: [u64; 4],
    pub i: [i64; 12],
}

// ---- the host's operations (h0 the object, `i` the values)
/// `h0` unit -> its id (`EEUnitHandleID`; none for 0)
pub const EE_PATH_UNIT_ID: i32 = 1;
/// `i0` id -> the complex unit (`EEUnitHandleResolve`, 0 none)
pub const EE_PATH_UNIT_RESOLVE: i32 = 2;
/// `h0` unit `Reference()`
pub const EE_PATH_UNIT_REFERENCE: i32 = 3;
/// `h0` unit `Dereference()`
pub const EE_PATH_UNIT_DEREFERENCE: i32 = 4;
/// `h0` request `Reference()`
pub const EE_PATH_REQUEST_REFERENCE: i32 = 5;
/// `h0` request `Dereference()`
pub const EE_PATH_REQUEST_DEREFERENCE: i32 = 6;
/// the game time (`gPhysics ? GetGameTime() : 0`)
pub const EE_PATH_GAME_TIME: i32 = 7;
/// `gContinentManager.mPathSubSectors.size()`
pub const EE_PATH_SUB_SECTORS: i32 = 8;
/// `gWorld.mMap`
pub const EE_PATH_MAP: i32 = 9;
/// `ClampTile(gWorld.mMap, (i0, i1))` -> `i0`, `i1`
pub const EE_PATH_CLAMP: i32 = 10;
/// a new `EEPathMachine(1, h1 map)`
pub const EE_PATH_PM_NEW: i32 = 11;
/// `delete` path machine `h0`
pub const EE_PATH_PM_DELETE: i32 = 12;
/// path machine `h0`'s goal's map
pub const EE_PATH_PM_MAP: i32 = 13;
/// path machine `h0`: the modifier of type `i0` (sea 2, space 0x10, else none), then `mType = i0`
pub const EE_PATH_PM_TYPE: i32 = 14;
/// tile machine `h0` (path or bounded) `Run()` -> `mFound`
pub const EE_PATH_TM_RUN: i32 = 15;
/// tile machine `h0` `SetRevLimit(i0)`
pub const EE_PATH_TM_SET_REV_LIMIT: i32 = 16;
/// tile machine `h0` `GetRevLimit()`
pub const EE_PATH_TM_REV_LIMIT: i32 = 17;
/// tile machine `h0` `mSteps`
pub const EE_PATH_TM_STEPS: i32 = 18;
/// path machine `h0` `Reset()`
pub const EE_PATH_PM_RESET: i32 = 19;
/// path machine `h0`: `Reset()`, `mFound = false`, then when `i0` the goal's `ReleaseModifier()` and `SetUnit(0)`
pub const EE_PATH_PM_RELEASE: i32 = 20;
/// path machine `h0` ready for unit `h1`'s search from (`i0`, `i1`) to (`i2`, `i3`): `CancelSearch`, the step
/// limit `i4`, the rev limit `i5`, start, goal, `mTile`, the goal's destination, `SetUnit`, `mCheckCorners`
pub const EE_PATH_PM_START: i32 = 21;
/// unit `h0`'s `SetPathRevs` inputs from (`i0`, `i1`) to (`i2`, `i3`) -> `i0` distance, `i1` amphibious, `i2` the
/// same continent
pub const EE_PATH_PATH_REVS: i32 = 22;
/// a new `EEGraphPathMachine(i0, i1)`
pub const EE_PATH_GM_NEW: i32 = 23;
/// `delete` graph machine `h0`
pub const EE_PATH_GM_DELETE: i32 = 24;
/// graph machine `h0` `GetCount()`
pub const EE_PATH_GM_COUNT: i32 = 25;
/// graph machine `h0` `mType = i0`
pub const EE_PATH_GM_TYPE: i32 = 26;
/// graph machine `h0` `Run()` -> `mFound`
pub const EE_PATH_GM_RUN: i32 = 27;
/// graph machine `h0` `SetRevLimit(i0)`
pub const EE_PATH_GM_SET_REV_LIMIT: i32 = 28;
/// graph machine `h0` `GetRevLimit()`
pub const EE_PATH_GM_REV_LIMIT: i32 = 29;
/// graph machine `h0` `mSteps`
pub const EE_PATH_GM_STEPS: i32 = 30;
/// graph machine `h0`: `Reset()`, `mFound = false`, then when `i0` the goal's `ReleaseModifier()` and `SetUnit(0)`
pub const EE_PATH_GM_RELEASE: i32 = 31;
/// graph machine `h0` `Reset()`
pub const EE_PATH_GM_RESET: i32 = 32;
/// graph machine `h0` ready for unit `h1` / request `h2` from (`i0`, `i1`) to (`i2`, `i3`) for player `i4`, the
/// step limit unless `i5` (unlimited), modifier `h3`: `StartGraphPath`'s setup -> the tiles on the map in `i0..i3`
pub const EE_PATH_GM_START: i32 = 33;
/// graph machine `h0`'s `SetGraphRevs` inputs from (`i0`, `i1`) to (`i2`, `i3`) -> `i0` distance, `i1` the goal
/// sector's kind allowed, `i2` the goal unit's id when a computer player's citizen (else none)
pub const EE_PATH_GRAPH_REVS: i32 = 34;
/// machine `h0` (`i0` 0 tile, 1 graph) `SetMaxRevs(i1)`
pub const EE_PATH_SET_MAX_REVS: i32 = 35;
/// `RequestUnitPaths(h1 unit, h2 request, h0 graph machine)` -> its result
pub const EE_PATH_REQUEST_UNIT_PATHS: i32 = 37;
/// `FinishGraphPath`'s unit part: unit `h0`'s auto waypoint goal -> 1 and (`i0`, `i1`) its tile, (`i2`, `i3`) the
/// goal's (the unit's path dropped), else 0
pub const EE_PATH_AUTO_WAYPOINT: i32 = 38;
/// unit `h0`'s player id
pub const EE_PATH_UNIT_PLAYER: i32 = 40;
/// unit `h0`'s waypoint count
pub const EE_PATH_UNIT_WAYPOINTS: i32 = 41;
/// unit `h0`'s location goal when an auto waypoint -> 1 and its location truncated in `i0`, `i1`
pub const EE_PATH_UNIT_GOAL_TILE: i32 = 42;
/// unit `h0`'s search type (land 1, sea 2, space 0x10)
pub const EE_PATH_UNIT_PATH_TYPE: i32 = 43;
/// unit `h0`: `mNumPathRequests` counted; whether its request goes into the history
pub const EE_PATH_UNIT_COUNT_REQUEST: i32 = 44;
/// `RequestPendingPath`'s unit part: unit `h0`'s path dropped -> its tile in `i0`, `i1`, whether a graph search
pub const EE_PATH_PENDING_UNIT: i32 = 45;
/// unit `h0` stranded (`mPlayer->UnitStranded`)
pub const EE_PATH_UNIT_STRANDED: i32 = 46;
/// `gContinentManager.Rebuild()`
pub const EE_PATH_REBUILD: i32 = 47;
/// `UMFloatToLong(gWorld.mGameSpeed * 10000.0f)`
pub const EE_PATH_REGION_DELAY: i32 = 48;
/// the bounded machine deleted (`Reset` and `Clear`)
pub const EE_PATH_BOUNDED_FREE: i32 = 49;
/// the storages' node pools freed (`Clear`)
pub const EE_PATH_FREE_POOLS: i32 = 50;
/// a new request (`i0` id, `i1` player, from (`i2`, `i3`), to (`i4`, `i5`), the flags `h1`) -> it
pub const EE_PATH_REQUEST_NEW: i32 = 51;
/// request `h0` into the request record `h1` (`ee_path_request_record_set`)
pub const EE_PATH_REQUEST_SAVE: i32 = 52;
/// tile machine `h0` (in `mPaths`) as the saved game holds it into the record `h1` -> whether it can be saved
pub const EE_PATH_PM_SAVE: i32 = 53;
/// path machine `h0` from the record `h1`, for unit `h2` -> whether its storage loaded
pub const EE_PATH_PM_LOAD: i32 = 54;
/// graph machine `h0` as the saved game holds it into the record `h1` -> whether it can be saved
pub const EE_PATH_GM_SAVE: i32 = 55;
/// graph machine `h0` from the record `h1` (its modifier `h2`, goal unit `h3`) -> whether its storage loaded
pub const EE_PATH_GM_LOAD: i32 = 56;
/// a loaded graph search's modifier from the record `h1` -> it (0: none, or one that cannot be rebuilt: `i0` 0)
pub const EE_PATH_GM_MODIFIER: i32 = 57;
/// modifier `h0` `Dereference()`
pub const EE_PATH_MODIFIER_DEREFERENCE: i32 = 58;
/// sub sector `i0` exists
pub const EE_PATH_SUB_SECTOR: i32 = 59;
/// the owners waiting for their requests: 0 take them (the first tick), 1 whether one waits for request id `i0`,
/// 2 give it request `h0`, 3 give the others none
pub const EE_PATH_AWAIT: i32 = 60;
/// the graph searches' resolve queued for the loaded game's first tick
pub const EE_PATH_QUEUE_RESOLVE: i32 = 61;
/// the map is there (`gWorld.mMap != NULL`)
pub const EE_PATH_HAS_MAP: i32 = 62;
/// `gLoadingSavedGame`
pub const EE_PATH_LOADING: i32 = 63;
/// `i0` id -> `dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(id))` (0 none)
pub const EE_PATH_WORLD_UNIT: i32 = 64;
/// tile machine `h0` `mFound`
pub const EE_PATH_TM_FOUND: i32 = 65;
/// graph machine `h0` `mFound`
pub const EE_PATH_GM_FOUND: i32 = 66;
/// a loaded request from the request record `h0` (a `RequestRecord`) -> it (one reference, the caller's)
pub const EE_PATH_REQUEST_LOAD: i32 = 67;

/// The manager's C++ side: one call per operation.
pub trait PathHost {
    fn call(&mut self, op: i32, io: &mut EePathIo) -> i64;
}

/// A host call with handles and values.
fn ask(host: &mut dyn PathHost, op: i32, h: &[u64], i: &[i64]) -> (i64, EePathIo) {
    let mut io = EePathIo::default();
    io.h[..h.len()].copy_from_slice(h);
    io.i[..i.len()].copy_from_slice(i);
    let r = host.call(op, &mut io);
    (r, io)
}

fn op(host: &mut dyn PathHost, op: i32, h: &[u64], i: &[i64]) -> i64 {
    ask(host, op, h, i).0
}

/// `EEPathManagerNode`: a search in progress.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Node {
    pub l04: i32,
    pub tile: (i32, i32),
    /// an `EEPathMachine` (`EEAIAStarMachineBase*`)
    pub path_machine: u64,
    pub graph_machine: u64,
    /// referenced
    pub request: u64,
    /// referenced; [`EE_PATH_NO_UNIT`] for none
    pub unit: u32,
    pub l20: i32,
    pub l24: i32,
    pub l28: i32,
    pub l2c: i32,
    pub passes: i32,
    pub l34: i32,
}

/// `EEPathManagerPending`: a unit asking its path again when the map is there.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Pending {
    pub unit: u32,
    pub x: i32,
    pub y: i32,
}

/// A queued search as the capture and the dumps see it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EePathQueued {
    /// 0 `mPaths`, 1 `mAutoPaths`
    pub queue: u32,
    pub unit: u32,
    pub x: i32,
    pub y: i32,
    pub passes: i32,
    pub machine: u64,
    pub request: u64,
}

/// The manager.
#[derive(Debug, Default)]
pub struct PathManager {
    nodes: Vec<Node>,
    free: Vec<u32>,
    pub paths: VecDeque<u32>,
    pub auto_paths: VecDeque<u32>,
    spare_paths: VecDeque<u64>,
    spare_graphs: VecDeque<u64>,
    pub pending: Vec<Pending>,
    pub record: PathRecord,
    pub lcc: i32,
    pub next_request_id: i32,
    pub next_region_time: u32,
    pub updating_regions: bool,
    loaded_running: Option<RunningState>,
    loaded_searches: Vec<SavedSearch>,
    loaded_graphs: Vec<SavedGraph>,
}

impl PathManager {
    pub fn node(&self, n: u32) -> &Node {
        &self.nodes[n as usize]
    }

    fn node_mut(&mut self, n: u32) -> &mut Node {
        &mut self.nodes[n as usize]
    }

    fn unit_id(host: &mut dyn PathHost, unit: u64) -> u32 {
        if unit == 0 {
            EE_PATH_NO_UNIT
        } else {
            op(host, EE_PATH_UNIT_ID, &[unit], &[]) as u32
        }
    }

    /// Whether node `n` holds `unit` (whose id is `id`): `(*it)->mUnit == inUnit`.
    fn holds(&self, n: u32, unit: u64, id: u32) -> bool {
        let u = self.node(n).unit;
        u == id && (u != EE_PATH_NO_UNIT || unit == 0)
    }

    fn release_unit(&mut self, host: &mut dyn PathHost, n: u32) {
        let id = self.node(n).unit;
        if id != EE_PATH_NO_UNIT {
            let unit = op(host, EE_PATH_UNIT_RESOLVE, &[], &[id as i64]) as u64;
            if unit != 0 {
                op(host, EE_PATH_UNIT_DEREFERENCE, &[unit], &[]);
            }
        }
        self.node_mut(n).unit = EE_PATH_NO_UNIT;
    }

    fn set_unit(&mut self, host: &mut dyn PathHost, n: u32, unit: u64) {
        self.release_unit(host, n);
        self.node_mut(n).unit = Self::unit_id(host, unit);
        if unit != 0 {
            op(host, EE_PATH_UNIT_REFERENCE, &[unit], &[]);
        }
    }

    fn release_request(&mut self, host: &mut dyn PathHost, n: u32) {
        let r = self.node(n).request;
        if r != 0 {
            op(host, EE_PATH_REQUEST_DEREFERENCE, &[r], &[]);
            self.node_mut(n).request = 0;
        }
    }

    fn set_request(&mut self, host: &mut dyn PathHost, n: u32, request: u64) {
        self.release_request(host, n);
        if request != 0 {
            self.node_mut(n).request = request;
            op(host, EE_PATH_REQUEST_REFERENCE, &[request], &[]);
        }
    }

    /// The node's unit (resolved), 0 for none.
    pub fn unit_of(&self, host: &mut dyn PathHost, n: u32) -> u64 {
        let id = self.node(n).unit;
        if id == EE_PATH_NO_UNIT {
            0
        } else {
            op(host, EE_PATH_UNIT_RESOLVE, &[], &[id as i64]) as u64
        }
    }

    /// `NewNode`: an empty node from the pool (`Clear`: its unit and request released, `mL04`/`mL34` kept).
    fn new_node(&mut self, host: &mut dyn PathHost) -> u32 {
        let n = match self.free.pop() {
            Some(n) => n,
            None => {
                self.nodes.push(Node { unit: EE_PATH_NO_UNIT, ..Node::default() });
                (self.nodes.len() - 1) as u32
            }
        };
        self.release_unit(host, n);
        self.release_request(host, n);
        let node = self.node_mut(n);
        node.l20 = 0;
        node.l28 = 0;
        node.path_machine = 0;
        node.graph_machine = 0;
        node.l24 = 0;
        node.l2c = 0;
        node.passes = 0;
        node.tile = (0, 0);
        n
    }

    /// `ReleaseNode`: its unit released, back to the pool (its request stays until the node's reuse).
    fn release_node(&mut self, host: &mut dyn PathHost, n: u32) {
        self.release_unit(host, n);
        self.free.push(n);
    }

    /// The pool freed (`mNodePool.Release()`): every node's unit and request released.
    fn free_nodes(&mut self, host: &mut dyn PathHost) {
        for n in (0..self.nodes.len() as u32).rev() {
            self.release_unit(host, n);
            self.release_request(host, n);
        }
        self.nodes.clear();
        self.free.clear();
    }

    // ---- the machines' pools

    /// `NewGraphMachine`
    fn new_graph_machine(&mut self, host: &mut dyn PathHost, kind: i64) -> u64 {
        let count = op(host, EE_PATH_SUB_SECTORS, &[], &[]) + 1;
        let mut machine = 0;
        while let Some(&back) = self.spare_graphs.back() {
            if op(host, EE_PATH_GM_COUNT, &[back], &[]) == count {
                machine = back;
            } else {
                op(host, EE_PATH_GM_DELETE, &[back], &[]);
            }
            self.spare_graphs.pop_back();
            if machine != 0 {
                break;
            }
        }
        if machine == 0 {
            machine = op(host, EE_PATH_GM_NEW, &[], &[kind, count]) as u64;
        }
        op(host, EE_PATH_GM_TYPE, &[machine], &[kind]);
        machine
    }

    /// `NewPathMachine`: a spare one built for the map, else a new one; the modifier and type of `kind`.
    fn new_path_machine(&mut self, host: &mut dyn PathHost, kind: i64, map: u64) -> u64 {
        let machine = match self.spare_paths.pop_back() {
            Some(m) => {
                if map != op(host, EE_PATH_PM_MAP, &[m], &[]) as u64 {
                    op(host, EE_PATH_PM_DELETE, &[m], &[]);
                    op(host, EE_PATH_PM_NEW, &[0, map], &[]) as u64
                } else {
                    m
                }
            }
            None => op(host, EE_PATH_PM_NEW, &[0, map], &[]) as u64,
        };
        op(host, EE_PATH_PM_TYPE, &[machine], &[kind]);
        machine
    }

    /// `ReleaseGraphMachine`
    fn release_graph_machine(&mut self, host: &mut dyn PathHost, machine: u64) {
        let keep = self.spare_graphs.len() < SPARE_GRAPH_MACHINES;
        op(host, EE_PATH_GM_RELEASE, &[machine], &[keep as i64]);
        if keep {
            self.spare_graphs.push_back(machine);
        } else {
            op(host, EE_PATH_GM_DELETE, &[machine], &[]);
        }
    }

    /// `ReleasePathMachine`
    fn release_path_machine(&mut self, host: &mut dyn PathHost, machine: u64) {
        let keep = self.spare_paths.len() < SPARE_PATH_MACHINES;
        op(host, EE_PATH_PM_RELEASE, &[machine], &[keep as i64]);
        if keep {
            self.spare_paths.push_back(machine);
        } else {
            op(host, EE_PATH_PM_DELETE, &[machine], &[]);
        }
    }

    /// `FreeMachines`
    pub fn free_machines(&mut self, host: &mut dyn PathHost) {
        while let Some(&m) = self.spare_paths.back() {
            op(host, EE_PATH_PM_RESET, &[m], &[]);
            self.spare_paths.pop_back();
            op(host, EE_PATH_PM_DELETE, &[m], &[]);
        }
        while let Some(&m) = self.spare_graphs.back() {
            op(host, EE_PATH_GM_RESET, &[m], &[]);
            self.spare_graphs.pop_back();
            op(host, EE_PATH_GM_DELETE, &[m], &[]);
        }
    }

    /// `FreePaths`
    pub fn free_paths(&mut self, host: &mut dyn PathHost) {
        while let Some(&n) = self.paths.front() {
            let m = self.node(n).path_machine;
            self.release_path_machine(host, m);
            self.release_node(host, n);
            self.paths.pop_front();
        }
        while let Some(&n) = self.auto_paths.front() {
            let m = self.node(n).graph_machine;
            self.release_graph_machine(host, m);
            self.release_node(host, n);
            self.auto_paths.pop_front();
        }
    }

    // ---- the queries

    fn find_in(&self, queue: &VecDeque<u32>, unit: u64, id: u32) -> Option<usize> {
        queue.iter().position(|&n| self.holds(n, unit, id))
    }

    /// `HasAutoPath`
    pub fn has_auto_path(&self, host: &mut dyn PathHost, unit: u64) -> bool {
        if self.auto_paths.is_empty() {
            return false;
        }
        let id = Self::unit_id(host, unit);
        self.find_in(&self.auto_paths, unit, id).is_some()
    }

    /// `IsPathDone`
    pub fn is_path_done(&self, host: &mut dyn PathHost, unit: u64) -> bool {
        if self.paths.is_empty() || unit == 0 {
            return true;
        }
        let id = Self::unit_id(host, unit);
        self.find_in(&self.paths, unit, id).is_none()
    }

    /// `GetPathGoal`: false and the goal tile when a tile search is queued for the unit, else true and (0, 0).
    pub fn path_goal(&self, host: &mut dyn PathHost, unit: u64) -> (bool, (i32, i32)) {
        if self.paths.is_empty() || unit == 0 {
            return (true, (0, 0));
        }
        let id = Self::unit_id(host, unit);
        match self.find_in(&self.paths, unit, id) {
            Some(i) => (false, self.node(self.paths[i]).tile),
            None => (true, (0, 0)),
        }
    }

    /// `FindPath(EEComplexUnit*)`: the unit's queued tile search.
    fn find_path(&self, host: &mut dyn PathHost, unit: u64) -> Option<u32> {
        if self.paths.is_empty() || unit == 0 {
            return None;
        }
        let id = Self::unit_id(host, unit);
        self.find_in(&self.paths, unit, id).map(|i| self.paths[i])
    }

    /// The queued searches, `mPaths` then `mAutoPaths`.
    pub fn queued(&self) -> Vec<EePathQueued> {
        let mut out = Vec::new();
        for (queue, list) in [(0u32, &self.paths), (1, &self.auto_paths)] {
            for &n in list {
                let node = self.node(n);
                out.push(EePathQueued {
                    queue,
                    unit: node.unit,
                    x: node.tile.0,
                    y: node.tile.1,
                    passes: node.passes,
                    machine: if queue == 0 { node.path_machine } else { node.graph_machine },
                    request: node.request,
                });
            }
        }
        out
    }

    // ---- the cancels

    /// `CancelAutoPaths`
    pub fn cancel_auto_paths(&mut self, host: &mut dyn PathHost, unit: u64) {
        if self.auto_paths.is_empty() {
            return;
        }
        let id = Self::unit_id(host, unit);
        if let Some(i) = self.find_in(&self.auto_paths, unit, id) {
            let n = self.auto_paths[i];
            let m = self.node(n).graph_machine;
            self.release_graph_machine(host, m);
            self.release_node(host, n);
            self.auto_paths.remove(i);
        }
    }

    /// `CancelAutoPathRequest`
    pub fn cancel_auto_path_request(&mut self, host: &mut dyn PathHost, request: u64) {
        if self.auto_paths.is_empty() {
            return;
        }
        if let Some(i) = self.auto_paths.iter().position(|&n| self.node(n).request == request) {
            let n = self.auto_paths[i];
            let m = self.node(n).graph_machine;
            self.release_graph_machine(host, m);
            self.release_node(host, n);
            self.auto_paths.remove(i);
        }
    }

    /// `CancelPaths`
    pub fn cancel_paths(&mut self, host: &mut dyn PathHost, unit: u64, auto_paths: bool) {
        if auto_paths {
            self.cancel_auto_paths(host, unit);
        }
        if self.paths.is_empty() {
            return;
        }
        let id = Self::unit_id(host, unit);
        if let Some(i) = self.find_in(&self.paths, unit, id) {
            let n = self.paths[i];
            let m = self.node(n).path_machine;
            self.release_path_machine(host, m);
            self.release_node(host, n);
            self.paths.remove(i);
        }
    }

    /// `CancelAutoPath`: the unit's graph search finished at once, applied and dropped.
    fn cancel_auto_path(&mut self, host: &mut dyn PathHost, unit: u64) {
        if self.auto_paths.is_empty() {
            return;
        }
        let id = Self::unit_id(host, unit);
        if let Some(i) = self.find_in(&self.auto_paths, unit, id) {
            let n = self.auto_paths[i];
            let m = self.node(n).graph_machine;
            op(host, EE_PATH_GM_SET_REV_LIMIT, &[m], &[-1]);
            op(host, EE_PATH_GM_RUN, &[m], &[]);
            self.finish_graph_path(host, n);
            self.cancel_auto_paths(host, unit);
        }
    }

    /// `CompleteAutoPath`
    pub fn complete_auto_path(&mut self, host: &mut dyn PathHost, request: u64) {
        if self.auto_paths.is_empty() {
            return;
        }
        if let Some(i) = self.auto_paths.iter().position(|&n| self.node(n).request == request) {
            let n = self.auto_paths[i];
            let m = self.node(n).graph_machine;
            op(host, EE_PATH_GM_SET_REV_LIMIT, &[m], &[-1]);
            op(host, EE_PATH_GM_RUN, &[m], &[]);
            self.finish_graph_path(host, n);
            self.cancel_auto_path_request(host, request);
        }
    }

    /// `CompletePath`: the unit's graph search, then its tile search, finished at once.
    pub fn complete_path(&mut self, host: &mut dyn PathHost, unit: u64) {
        if !self.auto_paths.is_empty() {
            self.cancel_auto_path(host, unit);
        }
        let Some(n) = self.find_path(host, unit) else {
            return;
        };
        let m = self.node(n).path_machine;
        op(host, EE_PATH_TM_SET_REV_LIMIT, &[m], &[-1]);
        op(host, EE_PATH_TM_RUN, &[m], &[]);
        self.finish_path(host, n);
        self.cancel_paths(host, unit, true);
    }

    // ---- the requests

    /// `FinishPath` on the node's machine and unit (pm_bounded.rs).
    fn finish_path(&mut self, host: &mut dyn PathHost, n: u32) {
        let m = self.node(n).path_machine;
        let unit = self.unit_of(host, n);
        crate::astar::pm_bounded::finish_path(host, m, unit);
    }

    /// `SetGraphRevs`
    fn set_graph_revs(&mut self, host: &mut dyn PathHost, machine: u64, from: (i32, i32), to: (i32, i32)) {
        let (_, io) =
            ask(host, EE_PATH_GRAPH_REVS, &[machine], &[from.0 as i64, from.1 as i64, to.0 as i64, to.1 as i64]);
        let mut time = op(host, EE_PATH_GAME_TIME, &[], &[]) as u32;
        if time > 4000 {
            time -= 4000;
        }
        let citizen = io.i[2] as u32;
        let repeated = citizen != EE_PATH_NO_UNIT && self.record.repeated(citizen as i32, to.0, to.1, time);
        let revs = crate::astar::path_splice::graph_revs(io.i[0] as i32, io.i[1] != 0, repeated);
        op(host, EE_PATH_SET_MAX_REVS, &[machine], &[1, revs as i64]);
    }

    /// `RequestPath`: a new request with the next id, its graph search started; the request (one reference, the
    /// caller's).
    #[allow(clippy::too_many_arguments)]
    pub fn request_path(
        &mut self,
        host: &mut dyn PathHost,
        player: i64,
        from: (i32, i32),
        to: (i32, i32),
        flags: u64,
        kind: i64,
        revs: i64,
        modifier: u64,
        unlimited: bool,
    ) -> u64 {
        let id = self.next_request_id;
        let request = op(
            host,
            EE_PATH_REQUEST_NEW,
            &[0, flags],
            &[id as i64, player, from.0 as i64, from.1 as i64, to.0 as i64, to.1 as i64],
        ) as u64;
        self.next_request_id = self.next_request_id.wrapping_add(1);
        self.start_graph_path(host, 0, from, to, player, request, kind, revs, modifier, unlimited);
        request
    }

    /// `StartGraphPath`: queued while it runs (true), else applied and freed at once (false).
    #[allow(clippy::too_many_arguments)]
    pub fn start_graph_path(
        &mut self,
        host: &mut dyn PathHost,
        unit: u64,
        from: (i32, i32),
        to: (i32, i32),
        player: i64,
        request: u64,
        kind: i64,
        revs: i64,
        modifier: u64,
        unlimited: bool,
    ) -> bool {
        let machine = self.new_graph_machine(host, kind);
        if unit != 0 {
            self.cancel_auto_paths(host, unit);
        }
        let (_, io) = ask(
            host,
            EE_PATH_GM_START,
            &[machine, unit, request, modifier],
            &[from.0 as i64, from.1 as i64, to.0 as i64, to.1 as i64, player, unlimited as i64],
        );
        let (from, to) = ((io.i[0] as i32, io.i[1] as i32), (io.i[2] as i32, io.i[3] as i32));
        if revs <= 0 {
            self.set_graph_revs(host, machine, from, to);
        } else {
            op(host, EE_PATH_SET_MAX_REVS, &[machine], &[1, revs]);
        }
        let started = op(host, EE_PATH_GM_RUN, &[machine], &[]) != 0;
        if started {
            let n = self.new_node(host);
            self.set_request(host, n, request);
            self.set_unit(host, n, unit);
            let node = self.node_mut(n);
            node.graph_machine = machine;
            node.tile = to;
            node.passes = 1;
            self.auto_paths.push_back(n);
        } else {
            op(host, EE_PATH_REQUEST_UNIT_PATHS, &[machine, unit, request], &[]);
            self.release_graph_machine(host, machine);
        }
        started
    }

    /// `RequestUnitPath`
    #[allow(clippy::too_many_arguments)]
    pub fn request_unit_path(
        &mut self,
        host: &mut dyn PathHost,
        unit: u64,
        from: (i32, i32),
        to: (i32, i32),
        unlimited: bool,
        revs: i64,
        use_graph: bool,
        modifier: u64,
        arg8: bool,
    ) {
        let (raw_from, raw_to) = (from, to);
        if from.0 == -10000 || from.1 == -10000 {
            return;
        }
        let map = op(host, EE_PATH_MAP, &[], &[]) as u64;
        let mut from = clamp(host, from);
        let mut to = clamp(host, to);
        let mut started = false;
        let mut no_path = true;
        if use_graph {
            let player = op(host, EE_PATH_UNIT_PLAYER, &[unit], &[]);
            started = self.start_graph_path(host, unit, raw_from, to, player, 0, 1, -1, modifier, unlimited);
            if !started {
                let (has, io) = ask(host, EE_PATH_UNIT_GOAL_TILE, &[unit], &[]);
                if has != 0 {
                    to = (io.i[0] as i32, io.i[1] as i32);
                }
            }
            no_path = op(host, EE_PATH_UNIT_WAYPOINTS, &[unit], &[]) == 0;
        }
        if unit != 0 && !no_path {
            return;
        }
        let kind = op(host, EE_PATH_UNIT_PATH_TYPE, &[unit], &[]);
        if op(host, EE_PATH_UNIT_COUNT_REQUEST, &[unit], &[]) != 0 {
            let time = op(host, EE_PATH_GAME_TIME, &[], &[]) as u32;
            let id = Self::unit_id(host, unit);
            self.record.add_history(id as i32, raw_to.0, raw_to.1, time);
        }
        self.cancel_paths(host, unit, false);
        if !unlimited && (!arg8 || unit == 0 || op(host, EE_PATH_UNIT_WAYPOINTS, &[unit], &[]) < 7) {
            let queued = self.paths.len() as i32;
            let mut short = to;
            let done = crate::astar::pm_bounded::try_short_path(host, unit, map, kind, from, &mut short, queued);
            from = short;
            if done {
                return;
            }
        }
        if !started || !use_graph {
            let n = self.new_node(host);
            let machine = self.new_path_machine(host, kind, map);
            let limit = if unlimited { -1 } else { STEP_LIMIT };
            // (SetPathRevs between the rev limit and the node: pure reads of the unit and the continents)
            let max_revs = if revs <= 0 {
                let (_, io) =
                    ask(host, EE_PATH_PATH_REVS, &[unit], &[from.0 as i64, from.1 as i64, to.0 as i64, to.1 as i64]);
                crate::astar::path_splice::path_revs(io.i[0] as i32, io.i[1] != 0, io.i[2] != 0) as i64
            } else {
                revs
            };
            self.set_unit(host, n, unit);
            let node = self.node_mut(n);
            node.path_machine = machine;
            node.tile = to;
            op(
                host,
                EE_PATH_PM_START,
                &[machine, unit],
                &[from.0 as i64, from.1 as i64, to.0 as i64, to.1 as i64, limit, max_revs],
            );
            if op(host, EE_PATH_TM_RUN, &[machine], &[]) != 0 {
                self.node_mut(n).passes = 1;
                self.paths.push_back(n);
            } else {
                self.finish_path(host, n);
                op(host, EE_PATH_PM_RESET, &[machine], &[]);
                self.release_path_machine(host, machine);
                self.release_node(host, n);
            }
        }
    }

    /// `FinishGraphPath`: the graph path applied; a unit that now has an auto waypoint goal asks its tile path.
    fn finish_graph_path(&mut self, host: &mut dyn PathHost, n: u32) -> bool {
        let node = *self.node(n);
        let unit = self.unit_of(host, n);
        let result = op(host, EE_PATH_REQUEST_UNIT_PATHS, &[node.graph_machine, unit, node.request], &[]) != 0;
        if unit != 0 {
            let (has, io) = ask(host, EE_PATH_AUTO_WAYPOINT, &[unit], &[]);
            if has != 0 {
                let (from, to) = ((io.i[0] as i32, io.i[1] as i32), (io.i[2] as i32, io.i[3] as i32));
                self.request_unit_path(host, unit, from, to, false, -1, false, 0, false);
            }
        }
        result
    }

    /// `RequestPendingPath`: one unit's path asked again.
    pub fn request_pending_path(&mut self, host: &mut dyn PathHost, unit: u64, tile: (i32, i32)) {
        let (graph, io) = ask(host, EE_PATH_PENDING_UNIT, &[unit], &[]);
        let from = (io.i[0] as i32, io.i[1] as i32);
        self.request_unit_path(host, unit, from, tile, false, -1, graph != 0, 0, false);
    }

    /// `RequestPendingPaths`: last first, then none is pending.
    pub fn request_pending_paths(&mut self, host: &mut dyn PathHost) {
        for i in (0..self.pending.len()).rev() {
            let p = self.pending[i];
            let unit = op(host, EE_PATH_WORLD_UNIT, &[], &[p.unit as i64]) as u64;
            if unit != 0 {
                self.request_pending_path(host, unit, (p.x, p.y));
            }
        }
        self.pending.clear();
    }

    // ---- the updates

    /// `Update`
    pub fn update(&mut self, host: &mut dyn PathHost) {
        if !self.auto_paths.is_empty() {
            self.update_auto_paths(host);
        }
        if !self.paths.is_empty() {
            self.update_paths(host);
        }
        if self.updating_regions {
            self.update_regions(host);
        }
    }

    /// `UpdateAutoPaths`: the graph searches in turn until 32 steps are spent.
    fn update_auto_paths(&mut self, host: &mut dyn PathHost) {
        let mut revs: i64 = 0;
        while let Some(&n) = self.auto_paths.front() {
            if revs >= STEP_BUDGET {
                break;
            }
            let m = self.node(n).graph_machine;
            let running = op(host, EE_PATH_GM_RUN, &[m], &[]) != 0;
            let node = self.node_mut(n);
            node.passes = node.passes.wrapping_add(1);
            revs = revs.wrapping_add(op(host, EE_PATH_GM_STEPS, &[m], &[]));
            if !running {
                self.finish_graph_path(host, n);
                self.release_graph_machine(host, m);
                self.release_node(host, n);
            } else {
                let limit = op(host, EE_PATH_GM_REV_LIMIT, &[m], &[]);
                if (limit as u32) < STEP_BUDGET as u32 {
                    op(host, EE_PATH_GM_SET_REV_LIMIT, &[m], &[limit + 1]);
                }
                self.auto_paths.push_back(n);
            }
            self.auto_paths.pop_front();
        }
    }

    /// `UpdatePaths`: the tile searches in turn until 32 steps are spent, then the record's prunes.
    fn update_paths(&mut self, host: &mut dyn PathHost) {
        let time = op(host, EE_PATH_GAME_TIME, &[], &[]) as u32;
        let mut revs: i64 = 0;
        while let Some(&n) = self.paths.front() {
            if revs >= STEP_BUDGET {
                break;
            }
            let m = self.node(n).path_machine;
            let running = op(host, EE_PATH_TM_RUN, &[m], &[]) != 0;
            let node = self.node_mut(n);
            node.passes = node.passes.wrapping_add(1);
            revs = revs.wrapping_add(op(host, EE_PATH_TM_STEPS, &[m], &[]));
            if !running {
                self.finish_path(host, n);
                self.release_path_machine(host, m);
                self.release_node(host, n);
            } else {
                let limit = op(host, EE_PATH_TM_REV_LIMIT, &[m], &[]);
                if (limit as u32) < STEP_BUDGET as u32 {
                    op(host, EE_PATH_TM_SET_REV_LIMIT, &[m], &[limit + 1]);
                }
                self.paths.push_back(n);
            }
            self.paths.pop_front();
        }
        let (history, regions) = self.record.due(time);
        if history {
            let now = op(host, EE_PATH_GAME_TIME, &[], &[]) as u32;
            let loading = op(host, EE_PATH_LOADING, &[], &[]) != 0;
            self.record.prune_history(now, loading);
        }
        if regions {
            let loading = op(host, EE_PATH_LOADING, &[], &[]) != 0;
            for id in self.record.prune_regions(loading) {
                let unit = op(host, EE_PATH_WORLD_UNIT, &[], &[id as i64]) as u64;
                if unit != 0 {
                    self.cancel_paths(host, unit, true);
                    op(host, EE_PATH_UNIT_STRANDED, &[unit], &[]);
                }
            }
        }
    }

    /// `UpdateRegions`: when due and no graph search is queued, the regions rebuilt (again in 0.1 s until done).
    fn update_regions(&mut self, host: &mut dyn PathHost) {
        let time = op(host, EE_PATH_GAME_TIME, &[], &[]) as u32;
        if self.next_region_time > time || !self.auto_paths.is_empty() {
            return;
        }
        if op(host, EE_PATH_REBUILD, &[], &[]) == 0 {
            self.next_region_time = time.wrapping_add(100);
            return;
        }
        self.updating_regions = false;
    }

    /// `SetRegionUpdates`
    pub fn set_region_updates(&mut self, host: &mut dyn PathHost, on: bool, now: bool) {
        if self.updating_regions == on {
            return;
        }
        if on {
            let time = op(host, EE_PATH_GAME_TIME, &[], &[]) as u32;
            self.next_region_time = if now {
                time.wrapping_add(1)
            } else {
                (op(host, EE_PATH_REGION_DELAY, &[], &[]) as u32).wrapping_add(time)
            };
        }
        self.updating_regions = on;
    }

    /// `Reset`
    pub fn reset(&mut self, host: &mut dyn PathHost) {
        op(host, EE_PATH_BOUNDED_FREE, &[], &[]);
        self.free_paths(host);
        self.free_machines(host);
        self.record = PathRecord::default();
        self.free_nodes(host);
        self.lcc = 0;
        self.next_request_id = 0;
        self.updating_regions = false;
        self.next_region_time = 0;
    }

    /// `Clear` (keeps the prune timers)
    pub fn clear(&mut self, host: &mut dyn PathHost) {
        op(host, EE_PATH_BOUNDED_FREE, &[], &[]);
        self.free_paths(host);
        self.free_machines(host);
        let (h, r) = (self.record.next_history_prune, self.record.next_region_prune);
        self.record = PathRecord { next_history_prune: h, next_region_prune: r, ..PathRecord::default() };
        self.free_nodes(host);
        op(host, EE_PATH_FREE_POOLS, &[], &[]);
    }

    /// `DumpSim`'s lines but the bounded machine's (EE-SIM, TEST ONLY).
    pub fn dump(&self, host: &mut dyn PathHost) -> String {
        use std::fmt::Write;
        let mut s = String::new();
        for (name, list, graph) in [("PATHS", &self.paths, false), ("\nAUTOPATHS", &self.auto_paths, true)] {
            let _ = write!(s, "{} {}:", name, list.len());
            for &n in list {
                let node = *self.node(n);
                let unit = if self.unit_of(host, n) != 0 { node.unit } else { 0 };
                let m = if graph { node.graph_machine } else { node.path_machine };
                let (steps, found) = if m == 0 {
                    (-1, 0)
                } else if graph {
                    (op(host, EE_PATH_GM_STEPS, &[m], &[]), op(host, EE_PATH_GM_FOUND, &[m], &[]))
                } else {
                    (op(host, EE_PATH_TM_STEPS, &[m], &[]), op(host, EE_PATH_TM_FOUND, &[m], &[]))
                };
                let _ = write!(s, " u{}@{},{}/p{}/s{}/f{}", unit, node.tile.0, node.tile.1, node.passes, steps, found);
            }
        }
        let _ = write!(s, "\nPENDING {}:", self.pending.len());
        for p in &self.pending {
            let _ = write!(s, " u{}@{},{}", p.unit, p.x, p.y);
        }
        let _ = writeln!(
            s,
            "\nPATHMANAGER history={} regions={} prunehistory={} pruneregions={} nextrequest={} nextregion={} updating={}",
            self.record.history.len(),
            self.record.regions.len(),
            self.record.next_history_prune,
            self.record.next_region_prune,
            self.next_request_id,
            self.next_region_time,
            self.updating_regions as i32
        );
        s
    }
}

/// `ClampTile(gWorld.mMap, tile)`
fn clamp(host: &mut dyn PathHost, tile: (i32, i32)) -> (i32, i32) {
    let (_, io) = ask(host, EE_PATH_CLAMP, &[], &[tile.0 as i64, tile.1 as i64]);
    (io.i[0] as i32, io.i[1] as i32)
}

// ---- the saved game

/// The save file through the C++ `FSFile`'s typed calls: `kind` 1 `uint32_t`, 2 `int32_t`, 3 `unsigned short`,
/// 4 `bool`, 5 `float` (the value's bits), 6 the bytes `data[..len]`. A read returns 0 and the value (or fills the
/// bytes), else the file's error.
pub type EePathFileFn =
    unsafe extern "C" fn(ctx: *mut c_void, kind: u32, value: *mut u32, data: *mut u8, len: u32) -> i32;

pub const EE_PATH_FILE_U32: u32 = 1;
pub const EE_PATH_FILE_I32: u32 = 2;
pub const EE_PATH_FILE_U16: u32 = 3;
pub const EE_PATH_FILE_BOOL: u32 = 4;
pub const EE_PATH_FILE_F32: u32 = 5;
pub const EE_PATH_FILE_BYTES: u32 = 6;

/// One typed value of the file (the layouts below are these in order).
#[derive(Clone, Debug, PartialEq)]
pub enum Item {
    U32(u32),
    I32(i32),
    U16(u16),
    Bool(bool),
    F32(f32),
    Bytes(Vec<u8>),
}

/// A typed writer (the C++ file, or a test's list).
pub trait FileOut {
    fn put(&mut self, item: Item);
}

impl FileOut for Vec<Item> {
    fn put(&mut self, item: Item) {
        self.push(item);
    }
}

/// A typed reader: `Err(code)` for the file's error.
pub trait FileIn {
    fn u32(&mut self) -> Result<u32, i32>;
    fn i32(&mut self) -> Result<i32, i32>;
    fn u16(&mut self) -> Result<u16, i32>;
    fn bool(&mut self) -> Result<bool, i32>;
    fn f32(&mut self) -> Result<f32, i32>;
    fn bytes(&mut self, len: usize) -> Result<Vec<u8>, i32>;
}

/// The C++ file.
pub struct CFile {
    pub f: EePathFileFn,
    pub ctx: *mut c_void,
}

impl CFile {
    fn get(&mut self, kind: u32) -> Result<u32, i32> {
        let mut v = 0u32;
        // SAFETY: the C++ file callback's contract
        match unsafe { (self.f)(self.ctx, kind, &mut v, std::ptr::null_mut(), 0) } {
            0 => Ok(v),
            e => Err(e),
        }
    }
}

impl FileOut for CFile {
    fn put(&mut self, item: Item) {
        let (kind, mut v, mut bytes) = match item {
            Item::U32(v) => (EE_PATH_FILE_U32, v, Vec::new()),
            Item::I32(v) => (EE_PATH_FILE_I32, v as u32, Vec::new()),
            Item::U16(v) => (EE_PATH_FILE_U16, v as u32, Vec::new()),
            Item::Bool(v) => (EE_PATH_FILE_BOOL, v as u32, Vec::new()),
            Item::F32(v) => (EE_PATH_FILE_F32, v.to_bits(), Vec::new()),
            Item::Bytes(b) => (EE_PATH_FILE_BYTES, 0, b),
        };
        let (ptr, len) =
            if bytes.is_empty() { (std::ptr::null_mut(), 0) } else { (bytes.as_mut_ptr(), bytes.len() as u32) };
        // SAFETY: the C++ file callback's contract
        unsafe { (self.f)(self.ctx, kind, &mut v, ptr, len) };
    }
}

impl FileIn for CFile {
    fn u32(&mut self) -> Result<u32, i32> {
        self.get(EE_PATH_FILE_U32)
    }
    fn i32(&mut self) -> Result<i32, i32> {
        self.get(EE_PATH_FILE_I32).map(|v| v as i32)
    }
    fn u16(&mut self) -> Result<u16, i32> {
        self.get(EE_PATH_FILE_U16).map(|v| v as u16)
    }
    fn bool(&mut self) -> Result<bool, i32> {
        self.get(EE_PATH_FILE_BOOL).map(|v| v != 0)
    }
    fn f32(&mut self) -> Result<f32, i32> {
        self.get(EE_PATH_FILE_F32).map(f32::from_bits)
    }
    fn bytes(&mut self, len: usize) -> Result<Vec<u8>, i32> {
        let mut b = vec![0u8; len];
        let mut v = 0u32;
        // SAFETY: the C++ file callback's contract (fills `len` bytes; `v` the count read)
        match unsafe { (self.f)(self.ctx, EE_PATH_FILE_BYTES, &mut v, b.as_mut_ptr(), len as u32) } {
            0 if v as usize == len => Ok(b),
            0 => Err(1),
            e => Err(e),
        }
    }
}

/// A list of items read back (the tests).
impl FileIn for VecDeque<Item> {
    fn u32(&mut self) -> Result<u32, i32> {
        match self.pop_front() {
            Some(Item::U32(v)) => Ok(v),
            _ => Err(-1),
        }
    }
    fn i32(&mut self) -> Result<i32, i32> {
        match self.pop_front() {
            Some(Item::I32(v)) => Ok(v),
            _ => Err(-1),
        }
    }
    fn u16(&mut self) -> Result<u16, i32> {
        match self.pop_front() {
            Some(Item::U16(v)) => Ok(v),
            _ => Err(-1),
        }
    }
    fn bool(&mut self) -> Result<bool, i32> {
        match self.pop_front() {
            Some(Item::Bool(v)) => Ok(v),
            _ => Err(-1),
        }
    }
    fn f32(&mut self) -> Result<f32, i32> {
        match self.pop_front() {
            Some(Item::F32(v)) => Ok(v),
            _ => Err(-1),
        }
    }
    fn bytes(&mut self, len: usize) -> Result<Vec<u8>, i32> {
        match self.pop_front() {
            Some(Item::Bytes(v)) if v.len() == len => Ok(v),
            _ => Err(-1),
        }
    }
}

fn put_points(w: &mut dyn FileOut, points: &[[i32; 2]]) {
    w.put(Item::U32(points.len() as u32));
    for p in points {
        w.put(Item::I32(p[0]));
        w.put(Item::I32(p[1]));
    }
}

fn get_points(r: &mut dyn FileIn) -> Result<Vec<[i32; 2]>, i32> {
    let count = r.u32()?;
    let mut out = Vec::new();
    for _ in 0..count {
        out.push([r.i32()?, r.i32()?]);
    }
    Ok(out)
}

fn put_bytes(w: &mut dyn FileOut, bytes: &[u8]) {
    w.put(Item::U32(bytes.len() as u32));
    if !bytes.is_empty() {
        w.put(Item::Bytes(bytes.to_vec()));
    }
}

fn get_bytes(r: &mut dyn FileIn) -> Result<Vec<u8>, i32> {
    let len = r.u32()? as usize;
    if len == 0 {
        Ok(Vec::new())
    } else {
        r.bytes(len)
    }
}

/// The running state of `SaveRunningState`: the record, then `mLCC`, `mNextRequestID`, `mNextRegionTime`,
/// `mIsUpdatingRegions`.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct RunningState {
    pub record: PathRecord,
    pub lcc: i32,
    pub next_request_id: i32,
    pub next_region_time: u32,
    pub updating_regions: bool,
}

impl RunningState {
    pub fn write(&self, w: &mut dyn FileOut) {
        for v in self.record.save_words() {
            w.put(Item::U32(v));
        }
        w.put(Item::I32(self.lcc));
        w.put(Item::I32(self.next_request_id));
        w.put(Item::U32(self.next_region_time));
        w.put(Item::Bool(self.updating_regions));
    }

    pub fn read(r: &mut dyn FileIn) -> Result<RunningState, i32> {
        // (a failed read inside the record is the original's kErrorBeyondEnd, whatever the file said)
        let record = PathRecord::load_words(|| r.u32().ok()).ok_or(EE_PATH_ERROR_BEYOND_END)?;
        Ok(RunningState {
            record,
            lcc: r.i32()?,
            next_request_id: r.i32()?,
            next_region_time: r.u32()?,
            updating_regions: r.bool()?,
        })
    }
}

/// `kErrorBeyondEnd`, `LoadRunningState`'s answer for a short record.
pub const EE_PATH_ERROR_BEYOND_END: i32 = 0xa;

/// A path request as the saved game holds it (save 0.277, `EESaveAutoPathRequest`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EePathRequestRecord {
    pub from: [i32; 2],
    pub to: [i32; 2],
    pub player: i32,
    pub id: i32,
    pub length: i32,
    pub done: bool,
    pub flags_player: i32,
    pub flags: [bool; 17],
}

/// A request's record and its path.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct RequestRecord {
    pub r: EePathRequestRecord,
    pub path: Vec<[i32; 2]>,
}

impl RequestRecord {
    pub fn write(&self, w: &mut dyn FileOut) {
        let r = &self.r;
        for v in [r.from[0], r.from[1], r.to[0], r.to[1], r.player, r.id, r.length] {
            w.put(Item::I32(v));
        }
        w.put(Item::Bool(r.done));
        w.put(Item::I32(r.flags_player));
        for f in r.flags {
            w.put(Item::Bool(f));
        }
        put_points(w, &self.path);
    }

    pub fn read(rd: &mut dyn FileIn) -> Result<RequestRecord, i32> {
        let mut r = EePathRequestRecord {
            from: [rd.i32()?, rd.i32()?],
            to: [rd.i32()?, rd.i32()?],
            player: rd.i32()?,
            id: rd.i32()?,
            length: rd.i32()?,
            done: rd.bool()?,
            flags_player: rd.i32()?,
            ..EePathRequestRecord::default()
        };
        for f in r.flags.iter_mut() {
            *f = rd.bool()?;
        }
        Ok(RequestRecord { r, path: get_points(rd)? })
    }
}

/// A tile search's machine and goal as the saved game holds them (save 0.276; the C++ fills and applies it).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EePathSavedMachine {
    pub tile: [i32; 2],
    /// `mType`, `mRevs`, `mNumRuns`, `mSteps`, `mMapWidth`, `mMapHeight`, the storage's `mWidth`, `mHeight`
    pub fields: [i32; 8],
    pub check_corners: bool,
    pub found: bool,
    pub start: [i32; 2],
    pub goal: [i32; 2],
    pub rev_limit: i32,
    pub max_revs: i32,
    /// `mSearchFailed`, `mB29`, `mAcceptNearest`, `mB2B`, `mB2C`, `mTookNearest`
    pub flags: [bool; 6],
    /// `mBounds24`, `mGoalBounds`, `mStartBounds`: bottom, left, right, top
    pub bounds: [[i32; 4]; 3],
    pub destination: [i32; 2],
    pub l64: i32,
    pub offset: [f32; 2],
    pub continent: u16,
    pub bounded: bool,
    pub avoid_units: bool,
}

/// The lists a saved search carries beside its record (the C++ fills them through `ee_path_saved_*`).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct EePathSavedLists {
    pub blocked: Vec<[i32; 2]>,
    pub unit_tiles: Vec<[i32; 2]>,
    pub storage: Vec<u8>,
}

/// A unit's tile search as saved (`SaveSearches`).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct SavedSearch {
    pub unit: u32,
    pub tile: [i32; 2],
    /// `mPasses`, `mL04`, `mL20`, `mL24`, `mL28`, `mL2C`
    pub node: [i32; 6],
    pub m: EePathSavedMachine,
    pub lists: EePathSavedLists,
}

impl SavedSearch {
    pub fn write(&self, w: &mut dyn FileOut) {
        let m = &self.m;
        w.put(Item::U32(self.unit));
        w.put(Item::I32(self.tile[0]));
        w.put(Item::I32(self.tile[1]));
        for v in self.node {
            w.put(Item::I32(v));
        }
        w.put(Item::I32(m.tile[0]));
        w.put(Item::I32(m.tile[1]));
        for v in m.fields {
            w.put(Item::I32(v));
        }
        w.put(Item::Bool(m.check_corners));
        w.put(Item::Bool(m.found));
        for v in [m.start[0], m.start[1], m.goal[0], m.goal[1], m.rev_limit, m.max_revs] {
            w.put(Item::I32(v));
        }
        for f in m.flags {
            w.put(Item::Bool(f));
        }
        put_points(w, &self.lists.blocked);
        put_points(w, &self.lists.unit_tiles);
        for b in m.bounds {
            for v in b {
                w.put(Item::I32(v));
            }
        }
        w.put(Item::I32(m.destination[0]));
        w.put(Item::I32(m.destination[1]));
        w.put(Item::I32(m.l64));
        w.put(Item::F32(m.offset[0]));
        w.put(Item::F32(m.offset[1]));
        w.put(Item::U16(m.continent));
        w.put(Item::Bool(m.bounded));
        w.put(Item::Bool(m.avoid_units));
        put_bytes(w, &self.lists.storage);
    }

    pub fn read(r: &mut dyn FileIn) -> Result<SavedSearch, i32> {
        let mut s = SavedSearch { unit: r.u32()?, tile: [r.i32()?, r.i32()?], ..SavedSearch::default() };
        for v in s.node.iter_mut() {
            *v = r.i32()?;
        }
        let m = &mut s.m;
        m.tile = [r.i32()?, r.i32()?];
        for v in m.fields.iter_mut() {
            *v = r.i32()?;
        }
        m.check_corners = r.bool()?;
        m.found = r.bool()?;
        m.start = [r.i32()?, r.i32()?];
        m.goal = [r.i32()?, r.i32()?];
        m.rev_limit = r.i32()?;
        m.max_revs = r.i32()?;
        for f in m.flags.iter_mut() {
            *f = r.bool()?;
        }
        s.lists.blocked = get_points(r)?;
        s.lists.unit_tiles = get_points(r)?;
        let m = &mut s.m;
        for b in m.bounds.iter_mut() {
            for v in b.iter_mut() {
                *v = r.i32()?;
            }
        }
        m.destination = [r.i32()?, r.i32()?];
        m.l64 = r.i32()?;
        m.offset = [r.f32()?, r.f32()?];
        m.continent = r.u16()?;
        m.bounded = r.bool()?;
        m.avoid_units = r.bool()?;
        s.lists.storage = get_bytes(r)?;
        Ok(s)
    }
}

/// The graph search's modifier kinds (`EEGraphModifierKind`; 3..8, the deleted sub groups' route planners, unused).
pub const EE_PATH_MODIFIER_NONE: i32 = 0;
pub const EE_PATH_MODIFIER_STORM_RAIN: i32 = 9;
pub const EE_PATH_MODIFIER_STORM_SNOW: i32 = 10;

/// A graph search's machine, goal and modifier as the saved game holds them (save 0.278).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EePathSavedGraph {
    /// `mTile`'s x, y, `mType`, `mRevs`, `mNumRuns`, `mSteps`, `mSector`, `mNodeCount`, `mCount`
    pub machine: [i32; 9],
    pub found: bool,
    /// start x, y, goal x, y, `mPoint38`, `mPoint40`, `mPlayerID`, `mRevLimit`, `mMaxRevs`, `mGoalSector`,
    /// `mStartSector`, `mL70`, the goal unit's id (-1 none), the from and to sub sectors (-1 none)
    pub goal: [i32; 17],
    /// `mSearchFailed`, `mB31`, `mAcceptNearest`, `mB33`, `mB34`, `mTookNearest`
    pub flags: [bool; 6],
    pub path_flags_player: i32,
    pub path_flags: [bool; 17],
    pub modifier: i32,
    pub weights: [f32; 15],
    pub modifier_player: u32,
    pub modifier_enemy: u32,
    pub continents: [u16; 2],
    pub any_continent: bool,
    pub avoid_friendly_blockers: bool,
    pub sub_group: bool,
}

impl EePathSavedGraph {
    fn has_influence(&self) -> bool {
        self.modifier != EE_PATH_MODIFIER_NONE
            && self.modifier != EE_PATH_MODIFIER_STORM_RAIN
            && self.modifier != EE_PATH_MODIFIER_STORM_SNOW
    }
}

/// A graph search as saved (`SaveGraphSearches`).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct SavedGraph {
    /// -1 none
    pub unit: i32,
    pub request: Option<RequestRecord>,
    /// the loaded request (one reference, the load's), 0 none
    pub request_object: u64,
    /// `mL04`, tile x, y, `mL20`, `mL24`, `mL28`, `mL2C`, `mPasses`, `mL34`
    pub node: [i32; 9],
    pub g: EePathSavedGraph,
    pub storage: Vec<u8>,
}

impl SavedGraph {
    pub fn write(&self, w: &mut dyn FileOut) {
        let g = &self.g;
        w.put(Item::I32(self.unit));
        w.put(Item::Bool(self.request.is_some()));
        if let Some(r) = &self.request {
            r.write(w);
        }
        for v in self.node {
            w.put(Item::I32(v));
        }
        for v in g.machine {
            w.put(Item::I32(v));
        }
        w.put(Item::Bool(g.found));
        for v in g.goal {
            w.put(Item::I32(v));
        }
        for f in g.flags {
            w.put(Item::Bool(f));
        }
        w.put(Item::I32(g.path_flags_player));
        for f in g.path_flags {
            w.put(Item::Bool(f));
        }
        w.put(Item::I32(g.modifier));
        if g.has_influence() {
            for v in g.weights {
                w.put(Item::F32(v));
            }
            w.put(Item::U32(g.modifier_player));
            w.put(Item::U32(g.modifier_enemy));
            w.put(Item::U16(g.continents[0]));
            w.put(Item::U16(g.continents[1]));
            w.put(Item::Bool(g.any_continent));
            w.put(Item::Bool(g.avoid_friendly_blockers));
            w.put(Item::Bool(g.sub_group));
        }
        put_bytes(w, &self.storage);
    }

    /// The search read (the caller makes its request's object).
    pub fn read(r: &mut dyn FileIn) -> Result<SavedGraph, i32> {
        let unit = r.i32()?;
        let request = if r.bool()? { Some(RequestRecord::read(r)?) } else { None };
        let mut s = SavedGraph { unit, request, ..SavedGraph::default() };
        {
            for v in s.node.iter_mut() {
                *v = r.i32()?;
            }
            let g = &mut s.g;
            for v in g.machine.iter_mut() {
                *v = r.i32()?;
            }
            g.found = r.bool()?;
            for v in g.goal.iter_mut() {
                *v = r.i32()?;
            }
            for f in g.flags.iter_mut() {
                *f = r.bool()?;
            }
            g.path_flags_player = r.i32()?;
            for f in g.path_flags.iter_mut() {
                *f = r.bool()?;
            }
            g.modifier = r.i32()?;
            if g.has_influence() {
                for v in g.weights.iter_mut() {
                    *v = r.f32()?;
                }
                g.modifier_player = r.u32()?;
                g.modifier_enemy = r.u32()?;
                g.continents = [r.u16()?, r.u16()?];
                g.any_continent = r.bool()?;
                g.avoid_friendly_blockers = r.bool()?;
                g.sub_group = r.bool()?;
            }
            s.storage = get_bytes(r)?;
        }
        Ok(s)
    }
}

impl PathManager {
    /// `SaveRunningState`
    pub fn save_running(&self, w: &mut dyn FileOut) {
        RunningState {
            record: self.record.clone(),
            lcc: self.lcc,
            next_request_id: self.next_request_id,
            next_region_time: self.next_region_time,
            updating_regions: self.updating_regions,
        }
        .write(w);
    }

    /// `LoadRunningState`: read now, put back at the loaded game's first tick ([`Self::apply_running`]).
    pub fn load_running(&mut self, r: &mut dyn FileIn) -> i32 {
        match RunningState::read(r) {
            Ok(s) => {
                self.loaded_running = Some(s);
                0
            }
            Err(e) => e,
        }
    }

    /// The loaded running state put back (the first tick).
    pub fn apply_running(&mut self) {
        if let Some(s) = self.loaded_running.take() {
            self.record = s.record;
            self.lcc = s.lcc;
            self.next_request_id = s.next_request_id;
            self.next_region_time = s.next_region_time;
            self.updating_regions = s.updating_regions;
        }
    }

    /// `SaveSearches`: the units' own tile searches (no request, not the bounded machine) that the host can save.
    pub fn save_searches(&self, host: &mut dyn PathHost, w: &mut dyn FileOut) {
        let mut out = Vec::new();
        for &n in &self.paths {
            let node = *self.node(n);
            let unit = self.unit_of(host, n);
            if unit == 0 || node.request != 0 || node.path_machine == 0 {
                continue;
            }
            let mut s = SavedSearch { unit: node.unit, tile: [node.tile.0, node.tile.1], ..SavedSearch::default() };
            s.node = [node.passes, node.l04, node.l20, node.l24, node.l28, node.l2c];
            let saved = op(
                host,
                EE_PATH_PM_SAVE,
                &[
                    node.path_machine,
                    &mut s.m as *mut EePathSavedMachine as u64,
                    &mut s.lists as *mut EePathSavedLists as u64,
                ],
                &[],
            );
            if saved != 0 {
                out.push(s);
            }
        }
        w.put(Item::U32(out.len() as u32));
        for s in &out {
            s.write(w);
        }
    }

    /// The units the loaded searches hold ask no path again (their unit's load put them in `mPending`).
    fn drop_pending(&mut self, units: impl Iterator<Item = u32> + Clone) {
        for id in units {
            for p in (0..self.pending.len()).rev() {
                if self.pending[p].unit == id {
                    self.pending.remove(p);
                }
            }
        }
    }

    /// `LoadSearches`: read now, back in `mPaths` at the first tick ([`Self::restore_searches`]).
    pub fn load_searches(&mut self, r: &mut dyn FileIn) -> i32 {
        let count = match r.u32() {
            Ok(c) => c,
            Err(e) => return e,
        };
        let mut searches = Vec::new();
        for _ in 0..count {
            match SavedSearch::read(r) {
                Ok(s) => searches.push(s),
                Err(e) => return e,
            }
        }
        self.drop_pending(searches.iter().map(|s| s.unit).collect::<Vec<_>>().into_iter());
        self.loaded_searches.extend(searches);
        0
    }

    /// The loaded tile searches back at the queue's front (the loaded game's first tick).
    pub fn restore_searches(&mut self, host: &mut dyn PathHost) {
        let searches = std::mem::take(&mut self.loaded_searches);
        let mut restored = Vec::new();
        for s in &searches {
            let unit = op(host, EE_PATH_WORLD_UNIT, &[], &[s.unit as i64]) as u64;
            if unit == 0 || op(host, EE_PATH_HAS_MAP, &[], &[]) == 0 {
                continue;
            }
            let map = op(host, EE_PATH_MAP, &[], &[]) as u64;
            let machine = self.new_path_machine(host, s.m.fields[0] as i64, map);
            let ok = op(
                host,
                EE_PATH_PM_LOAD,
                &[machine, &s.m as *const EePathSavedMachine as u64, &s.lists as *const EePathSavedLists as u64, unit],
                &[],
            );
            if ok == 0 {
                self.release_path_machine(host, machine);
                continue;
            }
            let n = self.new_node(host);
            self.set_unit(host, n, unit);
            let node = self.node_mut(n);
            node.path_machine = machine;
            node.tile = (s.tile[0], s.tile[1]);
            [node.passes, node.l04, node.l20, node.l24, node.l28, node.l2c] = s.node;
            restored.push(n);
        }
        for n in restored.into_iter().rev() {
            self.paths.push_front(n);
        }
    }

    /// `SaveGraphSearches`: the graph searches the host can rebuild.
    pub fn save_graph_searches(&self, host: &mut dyn PathHost, w: &mut dyn FileOut) {
        let mut out = Vec::new();
        for &n in &self.auto_paths {
            let node = *self.node(n);
            if node.graph_machine == 0 {
                continue;
            }
            let mut s = SavedGraph::default();
            let saved = op(
                host,
                EE_PATH_GM_SAVE,
                &[node.graph_machine, &mut s.g as *mut EePathSavedGraph as u64, &mut s.storage as *mut Vec<u8> as u64],
                &[],
            );
            if saved == 0 {
                continue;
            }
            let unit = self.unit_of(host, n);
            s.unit = if unit != 0 { node.unit as i32 } else { -1 };
            if node.request != 0 {
                let mut r = RequestRecord::default();
                op(host, EE_PATH_REQUEST_SAVE, &[node.request, &mut r as *mut RequestRecord as u64], &[]);
                s.request = Some(r);
            }
            s.node =
                [node.l04, node.tile.0, node.tile.1, node.l20, node.l24, node.l28, node.l2c, node.passes, node.l34];
            out.push(s);
        }
        w.put(Item::U32(out.len() as u32));
        for s in &out {
            s.write(w);
        }
    }

    /// `LoadGraphSearches`: read now (each request made), resolved at the first tick ([`Self::resolve_graphs`]).
    pub fn load_graph_searches(&mut self, host: &mut dyn PathHost, r: &mut dyn FileIn) -> i32 {
        let mut searches: Vec<SavedGraph> = Vec::new();
        let mut result = 0;
        match r.u32() {
            Ok(count) => {
                for _ in 0..count {
                    match SavedGraph::read(r) {
                        Ok(mut s) => {
                            if let Some(rr) = &s.request {
                                s.request_object = new_request(host, rr);
                            }
                            searches.push(s);
                        }
                        Err(e) => {
                            result = e;
                            break;
                        }
                    }
                }
            }
            Err(e) => result = e,
        }
        if result != 0 {
            for s in &searches {
                if s.request_object != 0 {
                    op(host, EE_PATH_REQUEST_DEREFERENCE, &[s.request_object], &[]);
                }
            }
            return result;
        }
        self.drop_pending(
            searches.iter().filter(|s| s.unit != -1).map(|s| s.unit as u32).collect::<Vec<_>>().into_iter(),
        );
        for s in &self.loaded_graphs {
            // (a load that never reached its first tick)
            if s.request_object != 0 {
                op(host, EE_PATH_REQUEST_DEREFERENCE, &[s.request_object], &[]);
            }
        }
        self.loaded_graphs = searches;
        op(host, EE_PATH_QUEUE_RESOLVE, &[], &[]);
        0
    }

    /// The loaded game's first tick (after the owners' own fix-ups): each loaded graph search back at the queue's
    /// front, its request given to the owner that waited for it; the owners whose search could not be rebuilt ask
    /// again.
    pub fn resolve_graphs(&mut self, host: &mut dyn PathHost) {
        let searches = std::mem::take(&mut self.loaded_graphs);
        op(host, EE_PATH_AWAIT, &[], &[0]);
        let mut restored = Vec::new();
        for s in &searches {
            let subsectors = op(host, EE_PATH_SUB_SECTORS, &[], &[]);
            let request = s.request_object;
            let request_id = if request != 0 { s.request.as_ref().map_or(0, |r| r.r.id) } else { 0 };
            let owner = request != 0 && op(host, EE_PATH_AWAIT, &[], &[1, request_id as i64]) != 0;
            let resolve = |host: &mut dyn PathHost, id: i32| -> u64 {
                if id == -1 {
                    0
                } else {
                    op(host, EE_PATH_WORLD_UNIT, &[], &[id as u32 as i64]) as u64
                }
            };
            let unit = resolve(host, s.unit);
            let goal_unit = resolve(host, s.g.goal[14]);
            let sector_ok =
                |host: &mut dyn PathHost, id: i32| id == -1 || op(host, EE_PATH_SUB_SECTOR, &[], &[id as i64]) != 0;
            let mut ok = (s.unit == -1 || unit != 0)
                && (s.g.goal[14] == -1 || goal_unit != 0)
                && s.g.machine[7] as i64 == subsectors + 1
                && sector_ok(host, s.g.goal[15])
                && sector_ok(host, s.g.goal[16]);
            let mut modifier = 0;
            if ok && s.g.modifier != EE_PATH_MODIFIER_NONE {
                modifier = op(host, EE_PATH_GM_MODIFIER, &[0, &s.g as *const EePathSavedGraph as u64], &[]) as u64;
                ok = modifier != 0;
            }
            let mut machine = 0;
            if ok {
                machine = self.new_graph_machine(host, s.g.machine[2] as i64);
                ok = op(
                    host,
                    EE_PATH_GM_LOAD,
                    &[machine, &s.g as *const EePathSavedGraph as u64, modifier, goal_unit],
                    &[s.storage.as_ptr() as i64, s.storage.len() as i64],
                ) != 0;
            }
            if ok {
                let n = self.new_node(host);
                self.set_request(host, n, request);
                self.set_unit(host, n, unit);
                let node = self.node_mut(n);
                node.graph_machine = machine;
                let v = s.node;
                node.l04 = v[0];
                node.tile = (v[1], v[2]);
                node.l20 = v[3];
                node.l24 = v[4];
                node.l28 = v[5];
                node.l2c = v[6];
                node.passes = v[7];
                node.l34 = v[8];
                restored.push(n);
            } else if machine != 0 {
                self.release_graph_machine(host, machine);
            }
            if modifier != 0 {
                op(host, EE_PATH_MODIFIER_DEREFERENCE, &[modifier], &[]);
            }
            if unit != 0 && !ok {
                // (as the load did before 0.278)
                self.request_pending_path(host, unit, (s.node[1], s.node[2]));
            }
            if request != 0 {
                if ok && owner {
                    // the owner takes the load's reference
                    op(host, EE_PATH_AWAIT, &[request], &[2, request_id as i64]);
                } else {
                    op(host, EE_PATH_REQUEST_DEREFERENCE, &[request], &[]);
                }
            }
        }
        for n in restored.into_iter().rev() {
            self.auto_paths.push_front(n);
        }
        op(host, EE_PATH_AWAIT, &[], &[3]);
    }
}

/// A loaded request (`EELoadAutoPathRequest`'s object).
fn new_request(host: &mut dyn PathHost, r: &RequestRecord) -> u64 {
    op(host, EE_PATH_REQUEST_LOAD, &[r as *const RequestRecord as u64], &[]) as u64
}

// ---- the C ABI

/// The C++ host: `(context, op, io) -> the answer`.
pub type EePathHostFn = unsafe extern "C" fn(*mut c_void, i32, *mut EePathIo) -> i64;

struct FnHost {
    ctx: *mut c_void,
    f: EePathHostFn,
}

impl PathHost for FnHost {
    fn call(&mut self, op: i32, io: &mut EePathIo) -> i64 {
        // SAFETY: the C++ callback's contract
        unsafe { (self.f)(self.ctx, op, io) }
    }
}

/// The C++ manager's handle: the manager and its host.
pub struct EePathManager {
    m: PathManager,
    host: FnHost,
}

/// A new manager over the host (`f` called with `ctx`).
#[no_mangle]
pub extern "C" fn ee_path_manager_new(f: EePathHostFn, ctx: *mut c_void) -> *mut EePathManager {
    Box::into_raw(Box::new(EePathManager { m: PathManager::default(), host: FnHost { ctx, f } }))
}

/// # Safety
/// `p` from `ee_path_manager_new` (or NULL), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_path_manager_free(p: *mut EePathManager) {
    if !p.is_null() {
        drop(Box::from_raw(p));
    }
}

/// Runs `body` over the manager. The manager is reentered only by the host's calls that the C++ never makes
/// during one of these (the original's call graph: a unit asking its path again while its search finishes goes
/// through the queues, which `body` reads again after each host call).
unsafe fn with<R>(p: *mut EePathManager, body: impl FnOnce(&mut PathManager, &mut dyn PathHost) -> R) -> R {
    let p = &mut *p;
    let mut host = FnHost { ctx: p.host.ctx, f: p.host.f };
    body(&mut p.m, &mut host)
}

/// The manager's calls (`what`; tiles as `i`, handles as `h`; the answer, `io` for the out values):
/// 1 `Update`, 2 `Reset`, 3 `Clear`, 4 `FreePaths`, 5 `RequestPendingPaths`, 6 `SetRegionUpdates(i0, i1)`,
/// 7 `CancelPaths(h0, i0)`, 8 `CancelAutoPathRequest(h0)`, 9 `CompletePath(h0)`, 10 `CompleteAutoPath(h0)`,
/// 11 `HasAutoPath(h0)`, 12 `IsPathDone(h0)`, 13 `GetPathGoal(h0)` (the tile in `i0`, `i1`),
/// 14 `RequestPath(player i0, from (i1, i2), to (i3, i4), flags h0, type i5, revs i6, modifier h1, unlimited i7)`,
/// 15 `RequestUnitPath(h0, from (i0, i1), to (i2, i3), unlimited i4, revs i5, graph i6, modifier h1, i7)`,
/// 16 `RequestPendingPath(h0, (i0, i1))`, 17 `AddPending(unit i0, (i1, i2))`, 18 the loaded running state
/// put back, 19 the graph searches' resolve, 20 the pending count, 21 the queued count, 22 `mLCC`,
/// 23 `mNextRequestID`, 24 `mNextRegionTime`, 25 `mIsUpdatingRegions`, 26 the loaded tile searches put back,
/// 27 `FindPath` (pm_bounded.rs), 28 `FindBoundedPath` (its points appended to the C++ vector `i6`).
///
/// # Safety
/// `p` a live manager, `io` live; the handles live objects of their kinds.
#[no_mangle]
pub unsafe extern "C" fn ee_path_manager_call(p: *mut EePathManager, what: i32, io: *mut EePathIo) -> i64 {
    let io = &mut *io;
    let (h, i) = (io.h, io.i);
    let t = |a: usize| (i[a] as i32, i[a + 1] as i32);
    with(p, |m, host| match what {
        1 => {
            m.update(host);
            0
        }
        2 => {
            m.reset(host);
            0
        }
        3 => {
            m.clear(host);
            0
        }
        4 => {
            m.free_paths(host);
            0
        }
        5 => {
            m.request_pending_paths(host);
            0
        }
        6 => {
            m.set_region_updates(host, i[0] != 0, i[1] != 0);
            0
        }
        7 => {
            m.cancel_paths(host, h[0], i[0] != 0);
            0
        }
        8 => {
            m.cancel_auto_path_request(host, h[0]);
            0
        }
        9 => {
            m.complete_path(host, h[0]);
            0
        }
        10 => {
            m.complete_auto_path(host, h[0]);
            0
        }
        11 => m.has_auto_path(host, h[0]) as i64,
        12 => m.is_path_done(host, h[0]) as i64,
        13 => {
            let (r, tile) = m.path_goal(host, h[0]);
            io.i[0] = tile.0 as i64;
            io.i[1] = tile.1 as i64;
            r as i64
        }
        14 => m.request_path(host, i[0], t(1), t(3), h[0], i[5], i[6], h[1], i[7] != 0) as i64,
        15 => {
            m.request_unit_path(host, h[0], t(0), t(2), i[4] != 0, i[5], i[6] != 0, h[1], i[7] != 0);
            0
        }
        16 => {
            m.request_pending_path(host, h[0], t(0));
            0
        }
        17 => {
            m.pending.push(Pending { unit: i[0] as u32, x: i[1] as i32, y: i[2] as i32 });
            0
        }
        18 => {
            m.apply_running();
            0
        }
        19 => {
            m.resolve_graphs(host);
            0
        }
        20 => m.pending.len() as i64,
        21 => (m.paths.len() + m.auto_paths.len()) as i64,
        22 => m.lcc as i64,
        23 => m.next_request_id as i64,
        24 => m.next_region_time as i64,
        25 => m.updating_regions as i64,
        26 => {
            m.restore_searches(host);
            0
        }
        27 => {
            let b = [i[4] as i32, i[5] as i32, i[6] as i32, i[7] as i32];
            crate::astar::pm_bounded::find_path(host, h[0], h[1], t(0), t(2), b, h[2], i[8], i[9], i[10] as u64, h[3]);
            0
        }
        28 => {
            let (reached, path) =
                crate::astar::pm_bounded::find_bounded_path(host, h[0], h[1], i[4], t(0), t(2), i[5] as i32);
            crate::astar::pm_bounded::points_out(host, i[6] as u64, &path);
            reached as i64
        }
        _ => -1,
    })
}

/// The queued searches into `out` (up to `cap`), `mPaths` then `mAutoPaths`; their count.
///
/// # Safety
/// `p` a live manager; `out` writable for `cap` entries (or NULL with `cap` 0).
#[no_mangle]
pub unsafe extern "C" fn ee_path_manager_queued(p: *mut EePathManager, out: *mut EePathQueued, cap: u32) -> u32 {
    let q = (*p).m.queued();
    for (k, e) in q.iter().take(cap as usize).enumerate() {
        *out.add(k) = *e;
    }
    q.len() as u32
}

/// The pending unit `k` (its id and tile into `out`); false past the end.
///
/// # Safety
/// `p` a live manager, `out` writable for three values.
#[no_mangle]
pub unsafe extern "C" fn ee_path_manager_pending(p: *mut EePathManager, k: u32, out: *mut i32) -> bool {
    let m = &(*p).m;
    match m.pending.get(k as usize) {
        Some(e) => {
            *out = e.unit as i32;
            *out.add(1) = e.x;
            *out.add(2) = e.y;
            true
        }
        None => false,
    }
}

/// The saved game's parts: `what` 0 the running state, 1 the tile searches, 2 the graph searches; `load` reads
/// (the answer the file's error, 0 done), else writes.
///
/// # Safety
/// `p` a live manager; `f` called with `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_path_manager_file(
    p: *mut EePathManager,
    what: i32,
    load: bool,
    f: EePathFileFn,
    ctx: *mut c_void,
) -> i32 {
    let mut file = CFile { f, ctx };
    with(p, |m, host| match (what, load) {
        (0, false) => {
            m.save_running(&mut file);
            0
        }
        (0, true) => m.load_running(&mut file),
        (1, false) => {
            m.save_searches(host, &mut file);
            0
        }
        (1, true) => m.load_searches(&mut file),
        (2, false) => {
            m.save_graph_searches(host, &mut file);
            0
        }
        (2, true) => m.load_graph_searches(host, &mut file),
        _ => -1,
    })
}

/// A request's record written (`EESaveAutoPathRequest`); `path` its `len` tiles.
///
/// # Safety
/// `r` live, `path` readable for `len` tiles; `f` called with `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_path_request_write(
    r: *const EePathRequestRecord,
    path: *const i32,
    len: u32,
    f: EePathFileFn,
    ctx: *mut c_void,
) {
    let mut file = CFile { f, ctx };
    let mut rr = RequestRecord { r: *r, path: Vec::new() };
    for k in 0..len as usize {
        rr.path.push([*path.add(2 * k), *path.add(2 * k + 1)]);
    }
    rr.write(&mut file);
}

/// A request's record read (`EELoadAutoPathRequest`): the file's error, or 0 with the record in `out` and its path
/// in the list `path` (an [`ee_path_tiles_push`] list).
///
/// # Safety
/// `out` writable, `path` a live list; `f` called with `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_path_request_read(
    out: *mut EePathRequestRecord,
    path: *mut c_void,
    f: EePathFileFn,
    ctx: *mut c_void,
) -> i32 {
    let mut file = CFile { f, ctx };
    match RequestRecord::read(&mut file) {
        Ok(rr) => {
            *out = rr.r;
            let list = &mut *(path as *mut Vec<[i32; 2]>);
            list.clear();
            list.extend(rr.path);
            0
        }
        Err(e) => e,
    }
}

/// A request record's (`RequestRecord`, as the host gets it) fields into `out` and its path's length; `path`
/// receives up to `cap` tiles.
///
/// # Safety
/// `r` a live `RequestRecord` (from a host operation), `out` writable, `path` writable for `cap` tiles.
#[no_mangle]
pub unsafe extern "C" fn ee_path_request_record_get(
    r: *const c_void,
    out: *mut EePathRequestRecord,
    path: *mut i32,
    cap: u32,
) -> u32 {
    let r = &*(r as *const RequestRecord);
    *out = r.r;
    for (k, p) in r.path.iter().take(cap as usize).enumerate() {
        *path.add(2 * k) = p[0];
        *path.add(2 * k + 1) = p[1];
    }
    r.path.len() as u32
}

/// A request record's fields and path set from the C++ request (`SaveGraphSearches`).
///
/// # Safety
/// `r` a live `RequestRecord`, `rec` readable, `path` readable for `len` tiles.
#[no_mangle]
pub unsafe extern "C" fn ee_path_request_record_set(
    r: *mut c_void,
    rec: *const EePathRequestRecord,
    path: *const i32,
    len: u32,
) {
    let r = &mut *(r as *mut RequestRecord);
    r.r = *rec;
    r.path = (0..len as usize).map(|k| [*path.add(2 * k), *path.add(2 * k + 1)]).collect();
}

/// A saved search's tile list (`which` 0 the blocked tiles, 1 the unit tiles): `k`'s tile into `out`, false past
/// the end; `push` appends (x, y).
///
/// # Safety
/// `lists` a live `EePathSavedLists`, `out` writable for two values.
#[no_mangle]
pub unsafe extern "C" fn ee_path_saved_tile(lists: *mut EePathSavedLists, which: u32, k: u32, out: *mut i32) -> bool {
    let l = &*lists;
    let v = if which == 0 { &l.blocked } else { &l.unit_tiles };
    match v.get(k as usize) {
        Some(p) => {
            *out = p[0];
            *out.add(1) = p[1];
            true
        }
        None => false,
    }
}

/// Appends (x, y) to a saved search's tile list `which`.
///
/// # Safety
/// `lists` a live `EePathSavedLists`.
#[no_mangle]
pub unsafe extern "C" fn ee_path_saved_tile_push(lists: *mut EePathSavedLists, which: u32, x: i32, y: i32) {
    let l = &mut *lists;
    if which == 0 { &mut l.blocked } else { &mut l.unit_tiles }.push([x, y]);
}

/// A saved search's storage bytes: set from `data[..len]` (`data` non-NULL), and their pointer and length.
///
/// # Safety
/// `bytes` a live `Vec<u8>` (an `EePathSavedLists`'s `storage` or a saved graph's), `data` readable for `len`.
#[no_mangle]
pub unsafe extern "C" fn ee_path_saved_bytes(
    bytes: *mut c_void,
    data: *const u8,
    len: u32,
    out_len: *mut u32,
) -> *const u8 {
    let b = &mut *(bytes as *mut Vec<u8>);
    if !data.is_null() {
        b.clear();
        b.extend_from_slice(std::slice::from_raw_parts(data, len as usize));
    }
    *out_len = b.len() as u32;
    b.as_ptr()
}

/// The storage bytes of a saved search's lists.
///
/// # Safety
/// `lists` a live `EePathSavedLists`.
#[no_mangle]
pub unsafe extern "C" fn ee_path_saved_storage(lists: *mut EePathSavedLists) -> *mut c_void {
    &mut (*lists).storage as *mut Vec<u8> as *mut c_void
}

/// A tile list for [`ee_path_request_read`]: new, its tiles, freed.
#[no_mangle]
pub extern "C" fn ee_path_tiles_new() -> *mut c_void {
    Box::into_raw(Box::new(Vec::<[i32; 2]>::new())) as *mut c_void
}

/// Tile `k` of the list into `out`; false past the end.
///
/// # Safety
/// `list` from `ee_path_tiles_new`, `out` writable for two values.
#[no_mangle]
pub unsafe extern "C" fn ee_path_tiles_get(list: *mut c_void, k: u32, out: *mut i32) -> bool {
    let list = &*(list as *mut Vec<[i32; 2]>);
    match list.get(k as usize) {
        Some(p) => {
            *out = p[0];
            *out.add(1) = p[1];
            true
        }
        None => false,
    }
}

/// # Safety
/// `list` from `ee_path_tiles_new`, not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_path_tiles_free(list: *mut c_void) {
    if !list.is_null() {
        drop(Box::from_raw(list as *mut Vec<[i32; 2]>));
    }
}

/// The dump's lines (`DumpSim` but the bounded machine's) into `out` (up to `cap` bytes, no terminator); their
/// length.
///
/// # Safety
/// `p` a live manager, `out` writable for `cap` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_path_manager_dump(p: *mut EePathManager, out: *mut u8, cap: u32) -> u32 {
    let s = with(p, |m, host| m.dump(host));
    let n = s.len().min(cap as usize);
    std::ptr::copy_nonoverlapping(s.as_ptr(), out, n);
    s.len() as u32
}

#[cfg(test)]
#[path = "pm_tests.rs"]
mod tests;
