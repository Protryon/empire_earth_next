//! The path manager's graph searches whole (`EEAIAStarGraphMachine<EEAIAStarGraphStorage, EEAIAStarPathGraphGoal,
//! EEAIPathSubSector>`, its `EEGraphPathMachine`): the machine's members, the node storage and the goal (with
//! `EEAIAStarGraphGoalBase`) as one Rust object over the sub sectors (the C++ objects, by their graph handle:
//! `EEAIPathSubSector*` is its own `AStarGraphableObjectBase*`). The goal's members are one C record,
//! `EeAStarGraphGoalRec`, at a fixed address: the C++ route planners keep it as their goal
//! (`EEAIGraphPathGoalModifierBase::mGoal`).
//!
//! The path manager's graph operations (`EE_PATH_GM_*`, `EE_PATH_GRAPH_REVS`, `EE_PATH_REQUEST_UNIT_PATHS`, the saved
//! graph searches) are answered here (`intercept`, after path_search.rs's); what only the C++ knows (a sub sector's
//! id, kind, neighbours, links and gate, the route planners, the unit's terrain flags, the goals a found path gives
//! a unit) is the host's (`EE_PATH_SS_INFO` and below). The hooks note the capture's events as `EEAIAStarGraphHooks`
//! did, and `run` writes the ASGR record.

use crate::astar::continent_grid::EePoint;
use crate::astar::flood::capture_graph_storage;
use crate::astar::graph::{self, GraphHost, GraphKind, GraphMachineState, GraphNodeView, GraphStorage, Obj};
use crate::astar::path_search::Raw;
use crate::astar::pm::*;
use crate::astar::search_note::*;
use std::ffi::c_void;
use std::ptr;

// ---- the C++ host's operations the graph searches ask
/// sub sector `h0` -> `i0` its id, `i1` kind, `i2` neighbour count, (`i3`, `i4`) its sector's center (the graph
/// location), `i5` its link count (`GetAllLinks().size()`), `i6` its sector (a handle)
pub const EE_PATH_SS_INFO: i32 = 141;
/// sub sector `h0`'s neighbour `i0` -> it
pub const EE_PATH_SS_NEIGHBOR: i32 = 142;
/// sub sector `h0` `IsNeighbor(h1)`
pub const EE_PATH_SS_IS_NEIGHBOR: i32 = 143;
/// sub sector `h0` `IsPassable(i0)` (a gate open to the player)
pub const EE_PATH_SS_GATE_OPEN: i32 = 144;
/// `HasPassableLink(h0, h1)` over the goal record `h2`'s flags
pub const EE_PATH_SS_PASSABLE_LINK: i32 = 145;
/// route planner `h0` `CanEnter(h1, h2)`
pub const EE_PATH_GRAPH_MODIFIER_ENTER: i32 = 146;
/// route planner `h0` `GetCost(the parent node: object h2, index i0, tile (i1, i2), costs i3 .. i5, h1)`
pub const EE_PATH_GRAPH_MODIFIER_COST: i32 = 147;
/// route planner `h0` `Reference()`
pub const EE_PATH_GRAPH_MODIFIER_REFERENCE: i32 = 148;
/// route planner `h0` takes the goal record `h1` (`mGoal`)
pub const EE_PATH_GRAPH_MODIFIER_ATTACH: i32 = 149;
/// unit `h0`'s terrain flags (`GetTerrainFlags`) into the goal record `h1`
pub const EE_PATH_GRAPH_TERRAIN_FLAGS: i32 = 150;
/// `RequestUnitPaths`' world: the found path's sub sector ids `h0[..i0]` given to unit `h1` (its goals) or request
/// `h2` (its path), the goal's start sub sector `h3`
pub const EE_PATH_APPLY_GRAPH_PATH: i32 = 151;
/// `StartGraphPath`'s world: tiles (`i0`, `i1`), (`i2`, `i3`) moved onto the map (written back), their sub sectors
/// `h0`, `h1`, those sub sectors' sector centers (`i4`, `i5`), (`i6`, `i7`) and ids `i8`, `i9`
pub const EE_PATH_GRAPH_ENDS: i32 = 152;
/// request `h0`'s flags into the goal record `h1` (`EECPASGFlags::operator=`)
pub const EE_PATH_REQUEST_FLAGS: i32 = 153;
/// `SetGraphRevs`' world: from (`i0`, `i1`) to (`i2`, `i3`), the goal's unit `h0` -> `i0` the distance, `i1` the kind of
/// the sub sector at the end, `i2` the unit's id when a computer player's citizen (else 0xffffffff)
pub const EE_PATH_GRAPH_REVS_WORLD: i32 = 154;
/// sub sector `i0` as a graph object (`EEGraphObjectOf`: null when none)
pub const EE_PATH_GRAPH_OBJECT: i32 = 155;
/// route planner `h0` into the saved graph search `h1` -> its kind (`-1`: none that a load can make)
pub const EE_PATH_GRAPH_MODIFIER_SAVE: i32 = 156;

const KIND_GATE: i64 = 0xe;
/// The goal's flags (`mSearchFailed`, `mB31`, `mAcceptNearest`, `mB33`, `mB34`, `mTookNearest`).
const FAILED: usize = 0;
const ACCEPT_NEAREST: usize = 2;
const TOOK_NEAREST: usize = 5;

/// `EEAIAStarPathGraphGoal`'s members with its base's.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAStarGraphGoalRec {
    /// `mStartTile`, `mGoalTile`
    pub start: EePoint,
    pub goal: EePoint,
    pub player_id: i32,
    /// `mRevLimit`, `mMaxRevs` (-1: none)
    pub rev_limit: i32,
    pub max_revs: i32,
    pub goal_sector: i32,
    pub start_sector: i32,
    /// `mSearchFailed`, `mB31`, `mAcceptNearest`, `mB33`, `mB34`, `mTookNearest`
    pub flags: [bool; 6],
    pub point38: EePoint,
    /// the goal sub sector's sector center (`Prepare`)
    pub point40: EePoint,
    /// `mUnit` (referenced), `mModifier` (referenced: an `EEAIGraphPathGoalModifierBase*`)
    pub unit: *mut c_void,
    pub modifier: *mut c_void,
    /// `mFrom`, `mTo` (`EEAIPathSubSector*`)
    pub from: *mut c_void,
    pub to: *mut c_void,
    /// `mPathFlags` (`EECPASGFlags`: the player, the passable sub sector kinds)
    pub flags_player: i32,
    pub path_flags: [bool; 17],
    pub l70: i32,
}

impl EeAStarGraphGoalRec {
    fn new() -> EeAStarGraphGoalRec {
        EeAStarGraphGoalRec {
            start: EePoint::default(),
            goal: EePoint::default(),
            player_id: 0,
            rev_limit: -1,
            max_revs: -1,
            goal_sector: 0,
            start_sector: 0,
            flags: [false; 6],
            point38: EePoint::default(),
            point40: EePoint::default(),
            unit: ptr::null_mut(),
            modifier: ptr::null_mut(),
            from: ptr::null_mut(),
            to: ptr::null_mut(),
            flags_player: 0,
            path_flags: [false; 17],
            l70: 0,
        }
    }
}

fn ask(raw: &mut Raw, op: i32, h: &[u64], i: &[i64]) -> (i64, EePathIo) {
    let mut io = EePathIo::default();
    io.h[..h.len()].copy_from_slice(h);
    io.i[..i.len()].copy_from_slice(i);
    let r = raw(op, &mut io);
    (r, io)
}

fn call(raw: &mut Raw, op: i32, h: &[u64], i: &[i64]) -> i64 {
    ask(raw, op, h, i).0
}

/// A sub sector as the hooks read it.
struct SubSector {
    id: i32,
    kind: i64,
    neighbors: u32,
    center: (i32, i32),
    links: i64,
    sector: i64,
}

fn sub_sector(raw: &mut Raw, s: Obj) -> SubSector {
    let (_, io) = ask(raw, EE_PATH_SS_INFO, &[s as u64], &[]);
    SubSector {
        id: io.i[0] as i32,
        kind: io.i[1],
        neighbors: io.i[2] as u32,
        center: (io.i[3] as i32, io.i[4] as i32),
        links: io.i[5],
        sector: io.i[6],
    }
}

/// One graph search.
pub struct GraphSearch {
    pub machine_type: i32,
    /// `mNodeCount`
    node_count: i32,
    st: GraphStorage,
    pub m: GraphMachineState,
    goal: *mut EeAStarGraphGoalRec,
}

impl Drop for GraphSearch {
    fn drop(&mut self) {
        // SAFETY: made by `new`, freed only here
        drop(unsafe { Box::from_raw(self.goal) });
    }
}

struct Hooks<'a, 'r> {
    m: &'a mut GraphMachineState,
    goal: *mut EeAStarGraphGoalRec,
    raw: &'a mut Raw<'r>,
}

impl Hooks<'_, '_> {
    fn g(&self) -> EeAStarGraphGoalRec {
        // SAFETY: the record lives as long as the search; the C++ writes it only during the calls below
        unsafe { *self.goal }
    }
    fn id(&mut self, o: Obj) -> i32 {
        sub_sector(self.raw, o).id
    }
}

impl GraphHost for Hooks<'_, '_> {
    fn start_object(&mut self) -> Obj {
        let from = self.g().from as Obj;
        let id = if from != 0 { self.id(from) } else { -1 };
        note(EV_GRAPH_START, 0, 0, 0, 0, 0, id);
        from
    }
    fn goal_id(&mut self) -> i32 {
        let to = self.g().to as Obj;
        // `mTo->GetID()`: an unsigned short
        i32::from(self.id(to) as u16)
    }
    fn estimate(&mut self, from_x: i32, from_y: i32, to_x: i32, to_y: i32) -> i32 {
        let e = from_x
            .wrapping_sub(to_x)
            .wrapping_abs()
            .wrapping_add(from_y.wrapping_sub(to_y).wrapping_abs())
            .wrapping_mul(10);
        note(EV_GRAPH_ESTIMATE, from_x, from_y, to_x, to_y, 0, e)
    }
    fn is_goal_node(&mut self, node: Option<&GraphNodeView>) -> bool {
        let g = self.g();
        let result = match node {
            None => true,
            Some(n) => {
                let to = g.to as Obj;
                if n.object == to {
                    true
                } else {
                    let s = sub_sector(self.raw, n.object);
                    if s.center.0.wrapping_sub(g.point40.x).wrapping_abs() <= 1
                        && s.center.1.wrapping_sub(g.point40.y).wrapping_abs() <= 1
                        && call(self.raw, EE_PATH_SS_IS_NEIGHBOR, &[n.object as u64, to as u64], &[]) != 0
                    {
                        let kind = sub_sector(self.raw, to).kind;
                        !g.path_flags[kind as usize]
                    } else {
                        false
                    }
                }
            }
        };
        note_bool(EV_GRAPH_GOAL_NODE, node.map_or(-1, |n| n.index), 0, 0, 0, 0, result)
    }
    fn object_id(&mut self, object: Obj) -> i32 {
        self.id(object)
    }
    fn num_neighbors(&mut self, object: Obj) -> u32 {
        sub_sector(self.raw, object).neighbors
    }
    fn neighbor(&mut self, object: Obj, i: u32) -> Obj {
        call(self.raw, EE_PATH_SS_NEIGHBOR, &[object as u64], &[i64::from(i)]) as Obj
    }
    fn graph_location(&mut self, object: Obj) -> (i32, i32) {
        sub_sector(self.raw, object).center
    }
    fn can_enter(&mut self, neighbor: Obj, from: Obj) -> bool {
        let g = self.g();
        let result = if !g.modifier.is_null() {
            call(self.raw, EE_PATH_GRAPH_MODIFIER_ENTER, &[g.modifier as u64, neighbor as u64, from as u64], &[]) != 0
        } else {
            // AcceptNeighbor: a passable kind (or a gate open to the player), then a passable link from `from`
            let s = sub_sector(self.raw, neighbor);
            let mut r = g.path_flags[s.kind as usize];
            if !r && s.kind == KIND_GATE {
                r = call(self.raw, EE_PATH_SS_GATE_OPEN, &[neighbor as u64], &[g.player_id.into()]) != 0;
            }
            if r && from != 0 && s.links > 0 {
                r = call(self.raw, EE_PATH_SS_PASSABLE_LINK, &[neighbor as u64, from as u64, self.goal as u64], &[]) != 0;
            }
            r
        };
        let (n, f) = (self.id(neighbor), self.id(from));
        note_bool(EV_GRAPH_ENTER, n, f, 0, 0, 0, result)
    }
    fn step_cost(&mut self, parent: &GraphNodeView, object: Obj) -> i32 {
        let g = self.g();
        let cost = if !g.modifier.is_null() {
            call(
                self.raw,
                EE_PATH_GRAPH_MODIFIER_COST,
                &[g.modifier as u64, object as u64, parent.object as u64],
                &[
                    parent.index.into(),
                    parent.x.into(),
                    parent.y.into(),
                    i64::from(parent.cost),
                    parent.path_cost.into(),
                    parent.estimate.into(),
                ],
            ) as i32
        } else {
            // the default: 1 inside one sector, else 10 straight or 14 diagonal between the sectors' centers
            let (a, b) = (sub_sector(self.raw, parent.object), sub_sector(self.raw, object));
            if a.sector != b.sector {
                if a.center.0 == b.center.0 || a.center.1 == b.center.1 {
                    10
                } else {
                    14
                }
            } else {
                1
            }
        };
        let id = self.id(object);
        note(EV_GRAPH_COST, parent.index, id, 0, 0, 0, cost)
    }
    fn search_done(&mut self, current: Option<&GraphNodeView>, has_open: bool) {
        let mut g = self.g();
        let mut found = false;
        match current {
            None => {
                if !has_open {
                    g.flags[FAILED] = true;
                    g.flags[TOOK_NEAREST] = g.flags[ACCEPT_NEAREST];
                }
            }
            Some(n) => {
                if n.x.wrapping_sub(g.goal.x).wrapping_abs() <= 1 && n.y.wrapping_sub(g.goal.y).wrapping_abs() <= 1 {
                    g.flags[FAILED] = false;
                    g.flags[TOOK_NEAREST] = g.flags[ACCEPT_NEAREST];
                    found = n.index == g.start_sector;
                }
            }
        }
        if !g.flags[FAILED] {
            g.flags[FAILED] = !found && self.m.revs >= g.max_revs;
        }
        // SAFETY: as `g`
        unsafe {
            (*self.goal).flags[FAILED] = g.flags[FAILED];
            (*self.goal).flags[TOOK_NEAREST] = g.flags[TOOK_NEAREST];
        }
        note(EV_GRAPH_SEARCH_DONE, i32::from(g.flags[FAILED]), i32::from(g.flags[TOOK_NEAREST]), 0, 0, 0, 0);
    }
    fn search_failed(&mut self) -> bool {
        self.g().flags[FAILED]
    }
    fn rev_limit(&mut self) -> i32 {
        self.g().rev_limit
    }
    fn max_revs(&mut self) -> i32 {
        self.g().max_revs
    }
    fn start_tile(&mut self) -> (i32, i32) {
        let g = self.g();
        (g.start.x, g.start.y)
    }
    fn goal_tile(&mut self) -> (i32, i32) {
        let g = self.g();
        (g.goal.x, g.goal.y)
    }
    fn state(&mut self) -> GraphMachineState {
        *self.m
    }
    fn set_state(&mut self, state: &GraphMachineState) {
        *self.m = *state;
    }
}

impl GraphSearch {
    /// `new EEGraphPathMachine(type, node count)`
    pub fn new(machine_type: i32, node_count: i32) -> GraphSearch {
        GraphSearch {
            machine_type,
            node_count,
            st: GraphStorage::new(GraphKind::Hashed),
            m: GraphMachineState::default(),
            goal: Box::into_raw(Box::new(EeAStarGraphGoalRec::new())),
        }
    }

    pub fn goal(&self) -> EeAStarGraphGoalRec {
        // SAFETY: as `Hooks::g`
        unsafe { *self.goal }
    }
    fn goal_mut(&mut self) -> &mut EeAStarGraphGoalRec {
        // SAFETY: as `Hooks::g`; no host call is made while the reference lives
        unsafe { &mut *self.goal }
    }

    /// The machine's dtor: the goal's (its unit and its route planner released).
    pub fn destroy(mut self, raw: &mut Raw) {
        self.set_unit(raw, 0);
        self.release_modifier(raw);
    }

    pub fn set_unit(&mut self, raw: &mut Raw, unit: u64) {
        let old = self.goal().unit as u64;
        if unit == old {
            return;
        }
        if old != 0 {
            call(raw, EE_PATH_UNIT_DEREFERENCE, &[old], &[]);
        }
        self.goal_mut().unit = unit as *mut c_void;
        if unit != 0 {
            call(raw, EE_PATH_UNIT_REFERENCE, &[unit], &[]);
        }
    }

    pub fn release_modifier(&mut self, raw: &mut Raw) {
        let m = self.goal().modifier as u64;
        if m != 0 {
            call(raw, EE_PATH_MODIFIER_DEREFERENCE, &[m], &[]);
        }
        self.goal_mut().modifier = ptr::null_mut();
    }

    pub fn set_modifier(&mut self, raw: &mut Raw, modifier: u64) {
        self.release_modifier(raw);
        self.goal_mut().modifier = modifier as *mut c_void;
        call(raw, EE_PATH_GRAPH_MODIFIER_REFERENCE, &[modifier], &[]);
        call(raw, EE_PATH_GRAPH_MODIFIER_ATTACH, &[modifier, self.goal as u64], &[]);
    }

    /// The goal's `Reset` (its base's, the unit and planner released, the ends and flags cleared).
    fn goal_reset(&mut self, raw: &mut Raw) {
        {
            let g = self.goal_mut();
            g.start = EePoint::default();
            g.goal = EePoint::default();
            g.max_revs = -1;
            g.rev_limit = -1;
            g.goal_sector = 0;
            g.start_sector = 0;
            g.player_id = 0;
            g.flags = [false; 6];
        }
        self.set_unit(raw, 0);
        self.release_modifier(raw);
        let g = self.goal_mut();
        g.l70 = 0;
        g.from = ptr::null_mut();
        g.to = ptr::null_mut();
        g.path_flags = [false; 17];
        g.flags_player = 0;
    }

    /// The machine's `Reset` (the nodes; the goal's; `mFound` stays).
    pub fn reset(&mut self, raw: &mut Raw) {
        self.st.reset_nodes();
        self.goal_reset(raw);
    }

    /// `Run()` with the capture's record (ASGR); returns `mFound`.
    pub fn run(&mut self, raw: &mut Raw) -> bool {
        let mut cap = CaptureRun::begin();
        let g = self.goal();
        if cap.on() {
            cap.i32(self.machine_type);
            cap.bool(self.m.found);
            cap.bool(g.flags[ACCEPT_NEAREST]);
            cap.i32(g.start.x);
            cap.i32(g.start.y);
            cap.i32(g.goal.x);
            cap.i32(g.goal.y);
            cap.i32(g.start_sector);
            cap.i32(g.goal_sector);
            cap.i32(g.player_id);
            cap.i32(g.rev_limit);
            cap.i32(g.max_revs);
            cap.i32(self.node_count);
        }
        let fresh = !self.m.found;
        if fresh {
            self.m.num_runs = 0;
            self.m.revs = 0;
            self.st.init(self.node_count);
            // Prepare: the unit's terrain flags, the goal sub sector's sector center
            let g = self.goal();
            if !g.unit.is_null() {
                call(raw, EE_PATH_GRAPH_TERRAIN_FLAGS, &[g.unit as u64, self.goal as u64], &[]);
            }
            let center = sub_sector(raw, g.to as Obj).center;
            self.goal_mut().point40 = EePoint { x: center.0, y: center.1 };
        }
        {
            let mut hooks = Hooks { m: &mut self.m, goal: self.goal, raw: &mut *raw };
            if fresh {
                graph::start(&mut self.st, &mut hooks);
            }
            graph::expand(&mut self.st, &mut hooks);
        }
        if cap.on() {
            let g = self.goal();
            cap.bool(self.m.found);
            cap.i32(self.m.revs);
            cap.i32(self.m.steps);
            cap.i32(self.m.num_runs);
            cap.i32(self.m.tile_x);
            cap.i32(self.m.tile_y);
            cap.i32(self.m.sector);
            cap.bool(g.flags[FAILED]);
            cap.bool(g.flags[TOOK_NEAREST]);
            capture_graph_storage(&mut cap, &self.st);
        }
        cap.commit(TAG_GRAPH_RUN);
        self.m.found
    }

    /// The found path's node indices (`GetPath`).
    pub fn path(&mut self) -> Vec<i32> {
        let mut out = Vec::new();
        if let Some((_, _, index)) = self.st.first_path_node() {
            out.push(index);
            while let Some((_, _, index)) = self.st.next_path_node() {
                out.push(index);
            }
        }
        out
    }

    /// `StartGraphPath`'s setup (`EE_PATH_GM_START`); the clamped tiles go back into `io`.
    fn start(&mut self, raw: &mut Raw, io: &mut EePathIo) {
        let (h, i) = (io.h, io.i);
        self.reset(raw);
        self.goal_reset(raw);
        if i[5] == 0 {
            self.goal_mut().rev_limit = 0xf;
        }
        if h[3] != 0 {
            self.set_modifier(raw, h[3]);
        }
        let (_, ends) = ask(raw, EE_PATH_GRAPH_ENDS, &[], &i[..4]);
        io.i[..4].copy_from_slice(&ends.i[..4]);
        let (from, to) = (ends.h[0], ends.h[1]);
        let from_center = EePoint { x: ends.i[4] as i32, y: ends.i[5] as i32 };
        let to_center = EePoint { x: ends.i[6] as i32, y: ends.i[7] as i32 };
        let (from_id, to_id) = (ends.i[8] as i32, ends.i[9] as i32);
        self.set_unit(raw, h[1]);
        if h[2] != 0 {
            call(raw, EE_PATH_REQUEST_FLAGS, &[h[2], self.goal as u64], &[]);
        }
        let g = self.goal_mut();
        g.start = from_center;
        g.goal_sector = from_id;
        g.goal = to_center;
        g.start_sector = to_id;
        g.point38 = to_center;
        g.from = from as *mut c_void;
        g.to = to as *mut c_void;
        g.player_id = i[4] as i32;
        self.m.tile_x = to_center.x;
        self.m.tile_y = to_center.y;
        self.m.sector = to_id;
    }

    /// The saved game's record (`EESaveGraphMachine`) when a load can make every part of it.
    fn save(&self, raw: &mut Raw, out: &mut EePathSavedGraph, storage: &mut Vec<u8>) -> bool {
        let g = self.goal();
        let kind = if g.modifier.is_null() {
            0
        } else {
            call(raw, EE_PATH_GRAPH_MODIFIER_SAVE, &[g.modifier as u64, out as *mut EePathSavedGraph as u64], &[])
        };
        let sectors = call(raw, EE_PATH_SUB_SECTORS, &[], &[]);
        if kind < 0
            || i64::from(self.node_count) != sectors + 1
            || !self.st.objects_match(&mut |k| call(raw, EE_PATH_GRAPH_OBJECT, &[], &[k.into()]) as Obj)
        {
            return false;
        }
        out.machine = [
            self.m.tile_x,
            self.m.tile_y,
            self.machine_type,
            self.m.revs,
            self.m.num_runs,
            self.m.steps,
            self.m.sector,
            self.node_count,
            self.st.count,
        ];
        out.found = self.m.found;
        let unit = if g.unit.is_null() { -1 } else { call(raw, EE_PATH_UNIT_ID, &[g.unit as u64], &[]) as i32 };
        let id = |raw: &mut Raw, s: *mut c_void| if s.is_null() { -1 } else { sub_sector(raw, s as Obj).id };
        let (from, to) = (id(raw, g.from), id(raw, g.to));
        out.goal = [
            g.start.x,
            g.start.y,
            g.goal.x,
            g.goal.y,
            g.point38.x,
            g.point38.y,
            g.point40.x,
            g.point40.y,
            g.player_id,
            g.rev_limit,
            g.max_revs,
            g.goal_sector,
            g.start_sector,
            g.l70,
            unit,
            from,
            to,
        ];
        out.flags = g.flags;
        out.path_flags_player = g.flags_player;
        out.path_flags = g.path_flags;
        out.modifier = kind as i32;
        *storage = self.st.to_bytes();
        true
    }

    /// A saved graph search back (`EELoadGraphMachine`), with its route planner and goal unit.
    fn load(&mut self, raw: &mut Raw, s: &EePathSavedGraph, modifier: u64, unit: u64, bytes: &[u8]) -> bool {
        self.reset(raw);
        let object_of = &mut |k: i32| call(raw, EE_PATH_GRAPH_OBJECT, &[], &[k.into()]) as Obj;
        let Some(loaded) = GraphStorage::from_bytes(self.st.kind, bytes, object_of) else {
            return false;
        };
        self.st = loaded;
        self.st.count = s.machine[8];
        self.m.tile_x = s.machine[0];
        self.m.tile_y = s.machine[1];
        self.m.revs = s.machine[3];
        self.m.num_runs = s.machine[4];
        self.m.steps = s.machine[5];
        self.m.sector = s.machine[6];
        self.m.found = s.found;
        {
            let g = self.goal_mut();
            g.start = EePoint { x: s.goal[0], y: s.goal[1] };
            g.goal = EePoint { x: s.goal[2], y: s.goal[3] };
            g.player_id = s.goal[8];
            g.rev_limit = s.goal[9];
            g.max_revs = s.goal[10];
            g.goal_sector = s.goal[11];
            g.start_sector = s.goal[12];
            g.flags = s.flags;
            g.point38 = EePoint { x: s.goal[4], y: s.goal[5] };
            g.point40 = EePoint { x: s.goal[6], y: s.goal[7] };
        }
        self.set_unit(raw, unit);
        if modifier != 0 {
            self.set_modifier(raw, modifier);
        }
        let from = call(raw, EE_PATH_GRAPH_OBJECT, &[], &[s.goal[15].into()]);
        let to = call(raw, EE_PATH_GRAPH_OBJECT, &[], &[s.goal[16].into()]);
        let g = self.goal_mut();
        g.from = from as *mut c_void;
        g.to = to as *mut c_void;
        g.flags_player = s.path_flags_player;
        g.path_flags = s.path_flags;
        g.l70 = s.goal[13];
        true
    }
}

/// A graph search's members as the path manager's dumps and its PMRQ capture read them.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarGraphSearchInfo {
    pub machine_type: i32,
    pub revs: i32,
    pub steps: i32,
    pub found: bool,
    pub max_revs: i32,
    pub rev_limit: i32,
}

/// # Safety
/// `search` a live graph search (a graph machine's handle); `out` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_graph_search_info(search: *const c_void, out: *mut EeAStarGraphSearchInfo) {
    let s = unsafe { &*(search as *const GraphSearch) };
    let g = s.goal();
    unsafe {
        *out = EeAStarGraphSearchInfo {
            machine_type: s.machine_type,
            revs: s.m.revs,
            steps: s.m.steps,
            found: s.m.found,
            max_revs: g.max_revs,
            rev_limit: g.rev_limit,
        }
    };
}

/// # Safety
/// `h` a graph machine's handle, made by `intercept`.
unsafe fn search<'a>(h: u64) -> &'a mut GraphSearch {
    unsafe { &mut *(h as *mut GraphSearch) }
}

/// The graph machine operations of the path manager's host over the Rust searches (`None`: not one of them).
pub fn intercept(op: i32, io: &mut EePathIo, raw: &mut Raw) -> Option<i64> {
    let (h, i) = (io.h, io.i);
    // SAFETY (every `search` below): the handles are the manager's graph machines, made here
    let r = match op {
        EE_PATH_GM_NEW => Box::into_raw(Box::new(GraphSearch::new(i[0] as i32, i[1] as i32))) as i64,
        EE_PATH_GM_DELETE => {
            let s = unsafe { Box::from_raw(h[0] as *mut GraphSearch) };
            s.destroy(raw);
            0
        }
        EE_PATH_GM_COUNT => i64::from(unsafe { search(h[0]) }.st.count),
        EE_PATH_GM_TYPE => {
            unsafe { search(h[0]) }.machine_type = i[0] as i32;
            0
        }
        EE_PATH_GM_RUN => i64::from(unsafe { search(h[0]) }.run(raw)),
        EE_PATH_GM_FOUND => i64::from(unsafe { search(h[0]) }.m.found),
        EE_PATH_GM_SET_REV_LIMIT => {
            unsafe { search(h[0]) }.goal_mut().rev_limit = i[0] as i32;
            0
        }
        EE_PATH_GM_REV_LIMIT => i64::from(unsafe { search(h[0]) }.goal().rev_limit),
        EE_PATH_GM_STEPS => i64::from(unsafe { search(h[0]) }.m.steps),
        EE_PATH_GM_RESET => {
            unsafe { search(h[0]) }.reset(raw);
            0
        }
        EE_PATH_GM_RELEASE => {
            let s = unsafe { search(h[0]) };
            s.reset(raw);
            s.m.found = false;
            if i[0] != 0 {
                s.release_modifier(raw);
                s.set_unit(raw, 0);
            }
            0
        }
        EE_PATH_GM_START => {
            unsafe { search(h[0]) }.start(raw, io);
            0
        }
        EE_PATH_GRAPH_REVS => {
            let s = unsafe { search(h[0]) };
            let g = s.goal();
            let (_, w) = ask(raw, EE_PATH_GRAPH_REVS_WORLD, &[g.unit as u64], &i[..4]);
            io.i[0] = w.i[0];
            io.i[1] = i64::from(g.path_flags[w.i[1] as usize]);
            io.i[2] = w.i[2];
            0
        }
        EE_PATH_SET_MAX_REVS if i[0] != 0 => {
            unsafe { search(h[0]) }.goal_mut().max_revs = i[1] as i32;
            0
        }
        EE_PATH_REQUEST_UNIT_PATHS => {
            // RequestUnitPaths: the path's sub sectors to the unit or the request; whether it reached the goal's
            let s = unsafe { search(h[0]) };
            let path = s.path();
            let g = s.goal();
            call(
                raw,
                EE_PATH_APPLY_GRAPH_PATH,
                &[path.as_ptr() as u64, h[1], h[2], g.from as u64],
                &[path.len() as i64],
            );
            let to = i64::from(sub_sector(raw, g.to as Obj).id as u16);
            i64::from(to == i64::from(s.m.sector))
        }
        EE_PATH_GM_SAVE => {
            let s = unsafe { search(h[0]) };
            // SAFETY: the record and the bytes are the caller's
            let (out, bytes) = unsafe { (&mut *(h[1] as *mut EePathSavedGraph), &mut *(h[2] as *mut Vec<u8>)) };
            i64::from(s.save(raw, out, bytes))
        }
        EE_PATH_GM_LOAD => {
            let s = unsafe { search(h[0]) };
            // SAFETY: the record and the bytes are the caller's
            let rec = unsafe { &*(h[1] as *const EePathSavedGraph) };
            let bytes: &[u8] = if i[1] == 0 || i[0] == 0 {
                &[]
            } else {
                unsafe { std::slice::from_raw_parts(i[0] as *const u8, i[1] as usize) }
            };
            i64::from(s.load(raw, rec, h[2], h[3], bytes))
        }
        _ => return None,
    };
    Some(r)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A row of sub sectors 1..=n (sector = id, center (10 id, 0)), each a neighbour of the next; kinds land (1).
    struct Row {
        n: i64,
        refs: Vec<(i32, u64)>,
    }

    fn host(row: &mut Row) -> impl FnMut(i32, &mut EePathIo) -> i64 + '_ {
        move |op, io| match op {
            EE_PATH_SS_INFO => {
                let id = io.h[0] as i64;
                io.i[0] = id;
                io.i[1] = 1;
                io.i[2] = if id == 1 || id == row.n { 1 } else { 2 };
                io.i[3] = 10 * id;
                io.i[4] = 0;
                io.i[5] = 0;
                io.i[6] = id;
                0
            }
            EE_PATH_SS_NEIGHBOR => {
                let id = io.h[0] as i64;
                if id == 1 {
                    2
                } else if id == row.n || io.i[0] == 0 {
                    id - 1
                } else {
                    id + 1
                }
            }
            EE_PATH_SS_IS_NEIGHBOR => i64::from((io.h[0] as i64 - io.h[1] as i64).abs() == 1),
            EE_PATH_UNIT_REFERENCE | EE_PATH_UNIT_DEREFERENCE => {
                row.refs.push((op, io.h[0]));
                0
            }
            EE_PATH_GRAPH_TERRAIN_FLAGS => {
                unsafe { (*(io.h[1] as *mut EeAStarGraphGoalRec)).path_flags[1] = true };
                0
            }
            _ => panic!("unexpected op {op}"),
        }
    }

    #[test]
    fn a_graph_search_walks_the_row() {
        let mut row = Row { n: 5, refs: Vec::new() };
        let mut raw = host(&mut row);
        let mut s = GraphSearch::new(1, 6);
        s.set_unit(&mut raw, 0x900);
        {
            let g = s.goal_mut();
            g.from = 1 as *mut c_void;
            g.to = 5 as *mut c_void;
            g.start = EePoint { x: 10, y: 0 };
            g.goal = EePoint { x: 50, y: 0 };
            g.start_sector = 5;
            g.goal_sector = 1;
        }
        s.m.tile_x = 50;
        s.m.sector = 5;
        TEST_NOTES.with(|n| n.borrow_mut().clear());
        s.run(&mut raw);
        assert_eq!(s.goal().point40, EePoint { x: 50, y: 0 });
        assert!(s.goal().path_flags[1]);
        let path = s.path();
        assert_eq!(path, vec![5, 4, 3, 2, 1]);
        let notes = TEST_NOTES.with(|n| n.borrow().clone());
        assert_eq!(notes[0], (EV_GRAPH_START, [0, 0, 0, 0, 0], 1));
        assert!(notes.iter().any(|n| n.0 == EV_GRAPH_COST && n.2 == 10));
        s.destroy(&mut raw);
        drop(raw);
        assert_eq!(row.refs, vec![(EE_PATH_UNIT_REFERENCE, 0x900), (EE_PATH_UNIT_DEREFERENCE, 0x900)]);
    }
}
