//! The unit path searches whole (`EEAIAStarMachine<EEAIAStarStorage, EEAIAStarPathfindingGoal, EEMap>`, the path
//! manager's `EEPathMachine`, and `EEAIAStarMachine<EEAIAStarBoundedStorage, EEAIAStarBoundedPathGoal, EEMap>`, its
//! bounded machine): the machine's members, the node storage and the goal (with `EEAIAStarGoalBase` and
//! `EEAIAStarPathfindingGoalBase`) as one Rust object. The goal's members are one C record, `EeAStarPathGoalRec`, at a
//! fixed address: the C++ modifiers keep it as their goal (`EEAIPathGoalModifierBase::mGoal`) and read and write it
//! during a search, as they did the C++ goal.
//!
//! The path manager's machine operations (`EE_PATH_PM_*`, `EE_PATH_TM_*`, `EE_PATH_BM_*`, the saved searches) are
//! answered here, before its C++ host (`intercept`); what only the C++ knows (the view of a map, a unit's continent
//! and blocked tiles, the C++ modifiers made, referenced and asked) is the host's (`EE_PATH_VIEW_NEW` and below).
//! The hooks note the capture's events as `EEAIAStarHooks` did, and `run` writes the ASRN record.

use crate::astar::continent_grid::EePoint;
use crate::astar::ffi::{view, CView, EeAStarNodeView, EeAStarViewCallbacks};
use crate::astar::machine::{self, Host, MachineState};
use crate::astar::path_goals::{self, EeAStarPathGoal};
use crate::astar::path_splice::EePathPoint;
use crate::astar::pm::*;
use crate::astar::pm_bounded::*;
use crate::astar::search_note::*;
use crate::astar::storage::{AStarRect, GridKind, List, Storage, StorageKind, NIL};
use std::ffi::c_void;
use std::ptr;

// ---- the C++ host's operations the searches ask (pm.rs's numbering continued)
/// a view of the map `h1` (`new EEAIAStarView(map)`), its callbacks into `*h2` -> the view
pub const EE_PATH_VIEW_NEW: i32 = 130;
/// view `h0` deleted
pub const EE_PATH_VIEW_FREE: i32 = 131;
/// unit `h0`'s goal facts (`SetUnit`) -> `i0` its continent id, `i1` whether the search avoids units
pub const EE_PATH_GOAL_UNIT: i32 = 132;
/// `CollectBlockedTiles`: unit `h0`'s bounded path tiles and (a sea unit) the stopped contacts in the way of goal
/// (`i0`, `i1`) (`i2` the goal bounded), into the tile list `h1` (`ee_astar_tiles_push`)
pub const EE_PATH_BLOCKED_TILES: i32 = 133;
/// `CollectUnitTiles`: a sea unit `h0`'s tiles appended to the tile list `h1`
pub const EE_PATH_UNIT_TILES: i32 = 134;
/// a new modifier of machine type `i0` (sea 2, space 0x10; `i1` the bounded goal's) over the goal record `h1` (one
/// reference, the caller's) -> it, 0 for none
pub const EE_PATH_TILE_MODIFIER_NEW: i32 = 135;
/// tile modifier `h0` `Reference()`, `Dereference()`
pub const EE_PATH_TILE_MODIFIER_REFERENCE: i32 = 136;
pub const EE_PATH_TILE_MODIFIER_DEREFERENCE: i32 = 137;
/// tile modifier `h0` takes the goal record `h1` (`mGoal`); `i0`: as the bounded goal's (`SetGoal`) -> its bounded
/// modifier (`dynamic_cast`)
pub const EE_PATH_TILE_MODIFIER_ATTACH: i32 = 138;
/// bounded modifier `h0` `CheckNode(the node at (i1, i2), none unless i0)`
pub const EE_PATH_CHECK_NODE: i32 = 139;
/// the tiles `h2[..i0]` appended to the C++ vector `h1` (`std::vector<U2DPoint<long> >`)
pub const EE_PATH_TILES_OUT: i32 = 140;

/// The machine types (`EEAIMachineType`).
const TYPE_BOUNDED: i32 = 4;
/// The goal's flags (`mSearchFailed`, `mB29`, `mAcceptNearest`, `mB2B`, `mB2C`, `mTookNearest`).
const FAILED: usize = 0;
const ACCEPT_NEAREST: usize = 2;
const TOOK_NEAREST: usize = 5;

/// The goal's members: `EEAIAStarGoalBase`'s, `EEAIAStarPathfindingGoalBase`'s and `EEAIAStarBoundedPathGoal`'s.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAStarPathGoalRec {
    /// `mStartTile`, `mGoalTile`
    pub start: EePoint,
    pub goal: EePoint,
    /// `mRevLimit`, `mMaxRevs` (-1: none)
    pub rev_limit: i32,
    pub max_revs: i32,
    /// `mSearchFailed`, `mB29`, `mAcceptNearest`, `mB2B`, `mB2C`, `mTookNearest`
    pub flags: [bool; 6],
    /// `mBounds24`, `mGoalBounds`, `mStartBounds`
    pub bounds24: AStarRect,
    pub goal_bounds: AStarRect,
    pub start_bounds: AStarRect,
    /// `mDestination`
    pub destination: EePoint,
    /// `mUnit` (referenced), `mModifier` (referenced: an `EEAIPathGoalModifierBase*`)
    pub unit: *mut c_void,
    pub modifier: *mut c_void,
    pub l64: i32,
    /// `mOffsetX`, `mOffsetY`
    pub offset: [f32; 2],
    /// `mContinentID` (the unit's), `mIsBounded`, `mAvoidUnits`
    pub continent: u16,
    pub is_bounded: bool,
    pub avoid_units: bool,
    /// the bounded goal's `mBounds` (the search window), `mBoundedModifier` (referenced), `mB8`
    pub bounds: AStarRect,
    pub bounded_modifier: *mut c_void,
    pub b8: bool,
}

impl EeAStarPathGoalRec {
    fn new() -> EeAStarPathGoalRec {
        EeAStarPathGoalRec {
            start: EePoint::default(),
            goal: EePoint::default(),
            rev_limit: -1,
            max_revs: -1,
            flags: [false; 6],
            bounds24: AStarRect::default(),
            goal_bounds: AStarRect::default(),
            start_bounds: AStarRect::default(),
            destination: EePoint::default(),
            unit: ptr::null_mut(),
            modifier: ptr::null_mut(),
            l64: 0,
            offset: [0.5, 0.5],
            continent: 0,
            is_bounded: false,
            avoid_units: false,
            bounds: AStarRect::default(),
            bounded_modifier: ptr::null_mut(),
            b8: true,
        }
    }
}

/// The C++ host, as the searches ask it (the path manager's host function).
pub type Raw<'a> = dyn FnMut(i32, &mut EePathIo) -> i64 + 'a;

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

/// `U2DRectangle<long>::Set(tile)` then `Grow(margin, true)`.
fn around(tile: EePoint, margin: i32) -> AStarRect {
    AStarRect {
        bottom: tile.y.wrapping_add(margin),
        left: tile.x.wrapping_sub(margin).max(0),
        right: tile.x.wrapping_add(margin),
        top: tile.y.wrapping_sub(margin).max(0),
    }
}

/// One search: the machine, its storage, its goal.
pub struct PathSearch {
    /// the bounded machine (`EEBoundedPathMachine`)
    bounded: bool,
    /// `EEAIAStarMachineBase::mType`
    pub machine_type: i32,
    /// the machine's map (`mMap`), the goal's (`Attach`'s; 0 before a search)
    map: u64,
    goal_map: u64,
    st: Storage,
    pub m: MachineState,
    /// the goal (C++ modifiers hold its address)
    goal: *mut EeAStarPathGoalRec,
    /// `mBlockedTiles`, `mUnitTiles`
    blocked: Vec<EePoint>,
    unit_tiles: Vec<EePoint>,
    /// the goal's view (`mView`, on the goal's map): the C++ view and its callbacks
    view: u64,
    view_cb: Option<EeAStarViewCallbacks>,
}

/// The goal and the machine's members as the machine sees them (`EEAIAStarHooks`), with the C++'s notes.
struct PathHost<'a, 'r> {
    bounded: bool,
    m: &'a mut MachineState,
    goal: *mut EeAStarPathGoalRec,
    view: CView<'a>,
    raw: &'a mut Raw<'r>,
}

impl PathHost<'_, '_> {
    fn g(&self) -> EeAStarPathGoalRec {
        // SAFETY: the record lives as long as the search; C++ writes it only during the calls below
        unsafe { *self.goal }
    }
    /// `PathState`: the goal's state for `path_goals`, read afresh (a modifier may have moved the goal tile)
    fn goal_state(&self) -> EeAStarPathGoal {
        let g = self.g();
        EeAStarPathGoal {
            bounded: self.bounded,
            bounds: if self.bounded { g.bounds } else { AStarRect::default() },
            goal_x: g.goal.x,
            goal_y: g.goal.y,
            walker: g.unit,
            modifier: g.modifier,
            continent_id: g.continent,
            check_corners: self.m.check_corners,
            accept_nearest: g.flags[ACCEPT_NEAREST],
            search_failed: g.flags[FAILED],
            took_nearest: g.flags[TOOK_NEAREST],
            revs: self.m.revs,
            max_revs: g.max_revs,
        }
    }
}

impl Host for PathHost<'_, '_> {
    fn accept_tile(&mut self, x: i32, y: i32, flag: bool) -> bool {
        let s = self.goal_state();
        let result = path_goals::accept_tile(&s, &mut self.view, x, y, flag);
        note_bool(EV_ACCEPT, x, y, i32::from(flag), 0, 0, result)
    }
    fn get_cost(&mut self, from_x: i32, from_y: i32, x: i32, y: i32, parent: Option<(i32, i32)>) -> i32 {
        let s = self.goal_state();
        let cost = path_goals::get_cost(&s, &mut self.view, from_x, from_y, x, y, parent);
        note(EV_COST, from_x, from_y, x, y, 0, cost)
    }
    fn estimate(&mut self, from_x: i32, from_y: i32, to_x: i32, to_y: i32) -> i32 {
        // EEAIAStarPathfindingGoalBase::Estimate: the Manhattan distance times 10
        let e = from_x
            .wrapping_sub(to_x)
            .wrapping_abs()
            .wrapping_add(from_y.wrapping_sub(to_y).wrapping_abs())
            .wrapping_mul(10);
        note(EV_ESTIMATE, from_x, from_y, to_x, to_y, 0, e)
    }
    fn is_goal_node(&mut self, node: Option<(i32, i32)>) -> bool {
        let s = self.goal_state();
        let result = path_goals::is_goal_node(&s, &mut self.view, node);
        let g = self.g();
        if self.bounded && !g.modifier.is_null() {
            // the bounded modifier inspects the node (it may move the goal: the enemy-in-range modifier)
            let (x, y) = node.unwrap_or((0, 0));
            call(self.raw, EE_PATH_CHECK_NODE, &[g.bounded_modifier as u64], &[i64::from(node.is_some()), x.into(), y.into()]);
        }
        let (x, y) = node.unwrap_or((0, 0));
        note_bool(EV_GOAL_NODE, i32::from(node.is_some()), x, y, 0, 0, result)
    }
    fn search_done(&mut self, current: Option<(i32, i32)>, has_open: bool) {
        let mut s = self.goal_state();
        path_goals::search_done(&mut s, &mut self.view, current, has_open);
        // SAFETY: as `g`
        unsafe {
            (*self.goal).flags[FAILED] = s.search_failed;
            (*self.goal).flags[TOOK_NEAREST] = s.took_nearest;
        }
        note(EV_SEARCH_DONE, i32::from(s.search_failed), i32::from(s.took_nearest), 0, 0, 0, 0);
    }
    fn search_failed(&mut self) -> bool {
        self.g().flags[FAILED]
    }
    fn accept_nearest(&mut self) -> bool {
        self.g().flags[ACCEPT_NEAREST]
    }
    fn is_step_limit(&mut self, steps: i32) -> bool {
        // the bounded goal's is false (its machine tests only the revs)
        let g = self.g();
        !self.bounded && g.rev_limit != -1 && steps >= g.rev_limit
    }
    fn is_rev_limit(&mut self, revs: i32) -> bool {
        let g = self.g();
        g.max_revs != -1 && revs >= g.max_revs
    }
    fn is_cheaper(&mut self, cost: u32, old: u32) -> bool {
        old > cost
    }
    fn start_tile(&mut self) -> (i32, i32) {
        let g = self.g();
        (g.start.x, g.start.y)
    }
    fn goal_tile(&mut self) -> (i32, i32) {
        let g = self.g();
        (g.goal.x, g.goal.y)
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

impl Drop for PathSearch {
    fn drop(&mut self) {
        // SAFETY: the record was made by `new` and is freed only here
        drop(unsafe { Box::from_raw(self.goal) });
    }
}

impl PathSearch {
    /// `new EEPathMachine(type, map)` / `new EEBoundedPathMachine(4, map)`.
    pub fn new(bounded: bool, machine_type: i32, map: u64) -> PathSearch {
        PathSearch {
            bounded,
            machine_type,
            map,
            goal_map: 0,
            st: Storage::new(if bounded { StorageKind::Bounded } else { StorageKind::Hashed }),
            m: MachineState::default(),
            goal: Box::into_raw(Box::new(EeAStarPathGoalRec::new())),
            blocked: Vec::new(),
            unit_tiles: Vec::new(),
            view: 0,
            view_cb: None,
        }
    }

    /// The goal record (its address is the C++ modifiers' goal).
    pub fn goal(&self) -> EeAStarPathGoalRec {
        // SAFETY: as `PathHost::g`
        unsafe { *self.goal }
    }
    fn goal_mut(&mut self) -> &mut EeAStarPathGoalRec {
        // SAFETY: as `PathHost::g`; no host call is made while the reference lives
        unsafe { &mut *self.goal }
    }

    /// The machine and the goal released: the C++ machine's dtor (the goal base's dtor drops the unit and its
    /// `mModifier`; the bounded modifier is not dropped there, as in the original) and the view.
    pub fn destroy(mut self, raw: &mut Raw) {
        self.set_unit(raw, 0);
        self.release_base_modifier(raw);
        if self.view != 0 {
            call(raw, EE_PATH_VIEW_FREE, &[self.view], &[]);
        }
    }

    // ---- the goal's members over the C++ (`EEAIAStarPathfindingGoalBase` and the derived goals)

    /// `SetUnit`: the reference moves; a new unit's continent and whether to avoid units.
    pub fn set_unit(&mut self, raw: &mut Raw, unit: u64) {
        let old = self.goal().unit as u64;
        if unit == old {
            return;
        }
        if old != 0 {
            call(raw, EE_PATH_UNIT_DEREFERENCE, &[old], &[]);
        }
        self.goal_mut().unit = unit as *mut c_void;
        if unit == 0 {
            return;
        }
        call(raw, EE_PATH_UNIT_REFERENCE, &[unit], &[]);
        self.goal_mut().offset = [0.5, 0.5];
        let (_, io) = ask(raw, EE_PATH_GOAL_UNIT, &[unit], &[]);
        let g = self.goal_mut();
        g.continent = io.i[0] as u16;
        g.avoid_units = io.i[1] != 0;
    }

    fn release_base_modifier(&mut self, raw: &mut Raw) {
        let m = self.goal().modifier as u64;
        if m != 0 {
            call(raw, EE_PATH_TILE_MODIFIER_DEREFERENCE, &[m], &[]);
        }
        self.goal_mut().modifier = ptr::null_mut();
    }

    /// `ReleaseModifier` (the bounded goal's drops its bounded modifier first).
    pub fn release_modifier(&mut self, raw: &mut Raw) {
        if self.bounded {
            let b = self.goal().bounded_modifier as u64;
            if b != 0 {
                call(raw, EE_PATH_TILE_MODIFIER_DEREFERENCE, &[b], &[]);
            }
            self.goal_mut().bounded_modifier = ptr::null_mut();
        }
        self.release_base_modifier(raw);
    }

    /// `SetModifier`: referenced, pointed at the goal (the bounded goal's also as its bounded modifier).
    pub fn set_modifier(&mut self, raw: &mut Raw, modifier: u64) {
        if self.bounded {
            self.release_modifier(raw);
        }
        self.release_modifier(raw);
        self.goal_mut().modifier = modifier as *mut c_void;
        call(raw, EE_PATH_TILE_MODIFIER_REFERENCE, &[modifier], &[]);
        let rec = self.goal as u64;
        call(raw, EE_PATH_TILE_MODIFIER_ATTACH, &[modifier, rec], &[0]);
        if self.bounded {
            let b = call(raw, EE_PATH_TILE_MODIFIER_ATTACH, &[modifier, rec], &[1]) as u64;
            self.goal_mut().bounded_modifier = b as *mut c_void;
            call(raw, EE_PATH_TILE_MODIFIER_REFERENCE, &[b], &[]);
        }
    }

    /// A new C++ modifier of the machine type for this goal (sea, space), set and its own reference dropped.
    fn type_modifier(&mut self, raw: &mut Raw, machine_type: i64) {
        let rec = self.goal as u64;
        let m = call(raw, EE_PATH_TILE_MODIFIER_NEW, &[0, rec], &[machine_type, i64::from(self.bounded)]) as u64;
        if m != 0 {
            self.set_modifier(raw, m);
            call(raw, EE_PATH_TILE_MODIFIER_DEREFERENCE, &[m], &[]);
        }
    }

    /// `EEAIAStarPathfindingGoalBase::Reset`
    fn base_reset(&mut self, raw: &mut Raw) {
        self.set_unit(raw, 0);
        self.blocked.clear();
        self.unit_tiles.clear();
        let g = self.goal_mut();
        g.l64 = 0;
        g.goal_bounds.clear();
        g.bounds24.clear();
        g.destination = EePoint::default();
        g.offset = [0.5, 0.5];
        g.continent = 0;
        g.is_bounded = false;
        g.avoid_units = false;
    }

    /// The machine's `Reset`: no nodes, not found, the goal's `Reset` (`ClearSearch`, the goal base's reset; the
    /// bounded goal's window cleared).
    pub fn reset(&mut self, raw: &mut Raw) {
        self.st.reset_nodes();
        self.m.found = false;
        // ClearSearch
        self.base_reset(raw);
        self.release_modifier(raw);
        self.blocked.clear();
        let bounded = self.bounded;
        let g = self.goal_mut();
        g.start_bounds.clear();
        g.goal_bounds.clear();
        // EEAIAStarGoalBase::Reset
        g.start = EePoint::default();
        g.goal = EePoint::default();
        g.max_revs = -1;
        g.rev_limit = -1;
        g.flags = [false; 6];
        if bounded {
            g.bounds.clear();
            g.b8 = true;
        }
    }

    /// `CancelSearch`
    pub fn cancel(&mut self) {
        self.m.found = false;
    }

    /// `Attach`: the goal on the map (its view).
    fn attach(&mut self, raw: &mut Raw, map: u64) {
        if self.view == 0 || self.goal_map != map {
            if self.view != 0 {
                call(raw, EE_PATH_VIEW_FREE, &[self.view], &[]);
            }
            let mut cb = std::mem::MaybeUninit::<EeAStarViewCallbacks>::uninit();
            self.view = call(raw, EE_PATH_VIEW_NEW, &[0, map, cb.as_mut_ptr() as u64], &[]) as u64;
            // SAFETY: the host wrote the callbacks
            self.view_cb = Some(unsafe { cb.assume_init() });
        }
        self.goal_map = map;
    }

    /// `Prepare` (both goals): the unit's own tiles, the blocked tiles marked, the start and goal bounds; the bounded
    /// goal also takes the nearest tile and its window's size flag.
    fn prepare(&mut self, raw: &mut Raw) {
        let g = self.goal();
        if !g.unit.is_null() {
            call(raw, EE_PATH_UNIT_TILES, &[g.unit as u64, &mut self.unit_tiles as *mut Vec<EePoint> as u64], &[]);
            self.blocked.clear();
            call(
                raw,
                EE_PATH_BLOCKED_TILES,
                &[g.unit as u64, &mut self.blocked as *mut Vec<EePoint> as u64],
                &[g.goal.x.into(), g.goal.y.into(), i64::from(g.is_bounded)],
            );
        }
        for p in self.blocked.iter().rev() {
            note(EV_MARK_TILE, p.x, p.y, 0, 0, 0, 0);
            self.st.mark_tile(p.x as u32, p.y as u32, false);
        }
        let bounded = self.bounded;
        let g = self.goal_mut();
        g.start_bounds = around(g.start, 3);
        g.goal_bounds = around(g.goal, 3);
        if bounded {
            g.flags[ACCEPT_NEAREST] = true;
            g.b8 = g.bounds.right.wrapping_sub(g.bounds.left) > 20 && g.bounds.bottom.wrapping_sub(g.bounds.top) > 20;
        }
    }

    /// `Run()` with the capture's record (ASRN); returns `mFound`.
    pub fn run(&mut self, raw: &mut Raw) -> bool {
        let size = call(raw, EE_PATH_MAP_SIZE, &[0, self.map], &[]) as i32;
        let mut cap = CaptureRun::begin();
        let g = self.goal();
        if cap.on() {
            cap.i32(self.machine_type);
            cap.bool(self.m.found);
            cap.bool(self.m.check_corners);
            cap.bool(g.flags[ACCEPT_NEAREST]);
            cap.i32(g.start.x);
            cap.i32(g.start.y);
            cap.i32(g.goal.x);
            cap.i32(g.goal.y);
            cap.i32(g.rev_limit);
            cap.i32(g.max_revs);
            cap.i32(size);
        }
        let fresh = !self.m.found;
        if fresh {
            self.m.num_runs = 0;
            self.m.map_width = size;
            self.m.map_height = size;
            self.m.revs = 0;
            self.st.init_begin(size, size);
            self.st.prepare_grid(size, size, if self.bounded { GridKind::Bounded } else { GridKind::Hashed }, false);
            self.st.alloc_tables();
            self.attach(raw, self.map);
            self.prepare(raw);
        }
        let cb = self.view_cb.as_ref().expect("the goal's view");
        {
            let mut host =
                PathHost { bounded: self.bounded, m: &mut self.m, goal: self.goal, view: CView(cb), raw: &mut *raw };
            if fresh {
                machine::start(&mut self.st, &mut host);
            }
            machine::expand(&mut self.st, &mut host);
        }
        if cap.on() {
            let g = self.goal();
            cap.bool(self.m.found);
            cap.bool(self.m.check_corners);
            cap.i32(self.m.revs);
            cap.i32(self.m.steps);
            cap.i32(self.m.num_runs);
            cap.i32(self.m.tile_x);
            cap.i32(self.m.tile_y);
            cap.bool(g.flags[FAILED]);
            cap.bool(g.flags[TOOK_NEAREST]);
            cap.i32(g.start.x);
            cap.i32(g.start.y);
            cap.i32(g.goal.x);
            cap.i32(g.goal.y);
            capture_storage(&mut cap, &self.st);
        }
        cap.commit(TAG_TILE_RUN);
        self.m.found
    }

    /// The found path's tiles in order (`GetPath`).
    pub fn path(&mut self) -> Vec<EePoint> {
        let mut out = Vec::new();
        if let Some((x, y)) = self.st.first_path_tile() {
            out.push(EePoint { x, y });
            while let Some((x, y)) = self.st.next_path_tile() {
                out.push(EePoint { x, y });
            }
        }
        out
    }

    /// The found path as points (`GetPoints`): the tile plus the goal's offsets (the bounded goal's 0.5), height 0.
    pub fn points(&mut self) -> Vec<EePathPoint> {
        let off = if self.bounded { [0.5f32, 0.5] } else { self.goal().offset };
        self.path().into_iter().map(|p| EePathPoint { x: p.x as f32 + off[0], y: p.y as f32 + off[1], z: 0.0 }).collect()
    }

    /// The closed, then the open nodes' tiles (`GetPath`'s visited tiles).
    pub fn visited(&self) -> Vec<EePoint> {
        let mut out = Vec::new();
        for list in [List::FastClosed, List::FastOpen] {
            for n in self.st.list(list) {
                out.push(EePoint { x: self.st.node(n).x, y: self.st.node(n).y });
            }
        }
        out
    }

    /// The saved game's record (`EESavePathMachine`; the path machines only).
    pub fn save(&self, out: &mut EePathSavedMachine, lists: &mut EePathSavedLists) -> bool {
        if self.machine_type == TYPE_BOUNDED || self.bounded {
            return false;
        }
        let g = self.goal();
        out.tile = [self.m.tile_x, self.m.tile_y];
        out.fields = [
            self.machine_type,
            self.m.revs,
            self.m.num_runs,
            self.m.steps,
            self.m.map_width,
            self.m.map_height,
            self.st.width,
            self.st.height,
        ];
        out.check_corners = self.m.check_corners;
        out.found = self.m.found;
        out.start = [g.start.x, g.start.y];
        out.goal = [g.goal.x, g.goal.y];
        out.rev_limit = g.rev_limit;
        out.max_revs = g.max_revs;
        out.flags = g.flags;
        lists.blocked.extend(self.blocked.iter().map(|p| [p.x, p.y]));
        lists.unit_tiles.extend(self.unit_tiles.iter().map(|p| [p.x, p.y]));
        let rect = |r: AStarRect| [r.bottom, r.left, r.right, r.top];
        out.bounds = [rect(g.bounds24), rect(g.goal_bounds), rect(g.start_bounds)];
        out.destination = [g.destination.x, g.destination.y];
        out.l64 = g.l64;
        out.offset = g.offset;
        out.continent = g.continent;
        out.bounded = g.is_bounded;
        out.avoid_units = g.avoid_units;
        lists.storage = self.st.to_bytes();
        true
    }

    /// A saved search back (`EELoadPathMachine`) for `unit`; false when its storage does not load.
    pub fn load(&mut self, raw: &mut Raw, s: &EePathSavedMachine, lists: &EePathSavedLists, unit: u64) -> bool {
        let Some(loaded) = Storage::from_bytes(self.st.kind, &lists.storage) else {
            return false;
        };
        self.st = loaded;
        self.st.width = s.fields[6];
        self.st.height = s.fields[7];
        self.m.tile_x = s.tile[0];
        self.m.tile_y = s.tile[1];
        self.m.revs = s.fields[1];
        self.m.num_runs = s.fields[2];
        self.m.steps = s.fields[3];
        self.m.map_width = s.fields[4];
        self.m.map_height = s.fields[5];
        self.m.check_corners = s.check_corners;
        self.m.found = s.found;
        // as Run does at a search's start
        let world_map = call(raw, EE_PATH_MAP, &[], &[]) as u64;
        self.attach(raw, world_map);
        self.set_unit(raw, unit);
        self.blocked = lists.blocked.iter().map(|p| EePoint { x: p[0], y: p[1] }).collect();
        self.unit_tiles = lists.unit_tiles.iter().map(|p| EePoint { x: p[0], y: p[1] }).collect();
        let rect = |r: [i32; 4]| AStarRect { bottom: r[0], left: r[1], right: r[2], top: r[3] };
        let g = self.goal_mut();
        g.start = EePoint { x: s.start[0], y: s.start[1] };
        g.goal = EePoint { x: s.goal[0], y: s.goal[1] };
        g.rev_limit = s.rev_limit;
        g.max_revs = s.max_revs;
        g.flags = s.flags;
        g.bounds24 = rect(s.bounds[0]);
        g.goal_bounds = rect(s.bounds[1]);
        g.start_bounds = rect(s.bounds[2]);
        g.destination = EePoint { x: s.destination[0], y: s.destination[1] };
        g.l64 = s.l64;
        g.offset = s.offset;
        g.continent = s.continent;
        g.is_bounded = s.bounded;
        g.avoid_units = s.avoid_units;
        true
    }
}

/// The storage as `CaptureOutputs` writes it: the current and the path's node, the bounds, the lists' digests.
pub(crate) fn capture_storage(cap: &mut CaptureRun, st: &Storage) {
    for which in 0..2 {
        let node = if which == 0 { st.current } else { st.path };
        cap.bool(node != NIL);
        if node != NIL {
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

/// A search by its handle.
///
/// # Safety
/// `h` from `PathSearch` boxed by `intercept` (a path machine handle or the bounded machine), alive.
unsafe fn search<'a>(h: u64) -> &'a mut PathSearch {
    unsafe { &mut *(h as *mut PathSearch) }
}

fn tile(i: &[i64], at: usize) -> EePoint {
    EePoint { x: i[at] as i32, y: i[at + 1] as i32 }
}

/// Appends points to a point list of pm_bounded.rs (`Vec<EePathPoint>`).
fn points_out(list: u64, points: &[EePathPoint]) {
    // SAFETY: the list is the caller's live `Vec<EePathPoint>`
    unsafe { (*(list as *mut Vec<EePathPoint>)).extend_from_slice(points) };
}

/// Appends tiles to a C++ `std::vector<U2DPoint<long> >`.
fn tiles_out(raw: &mut Raw, vector: u64, tiles: &[EePoint]) {
    if !tiles.is_empty() {
        call(raw, EE_PATH_TILES_OUT, &[0, vector, tiles.as_ptr() as u64], &[tiles.len() as i64]);
    }
}

/// The machine operations of the path manager's host, answered over the Rust searches (`None`: not one of them).
/// `bounded` is the manager's bounded machine (`mBoundedMachine`, null for none).
pub fn intercept(bounded: &mut *mut PathSearch, op: i32, io: &mut EePathIo, raw: &mut Raw) -> Option<i64> {
    let h = io.h;
    let i = io.i;
    // SAFETY (every `search` below): the handles are the manager's machines, made here
    let r = match op {
        EE_PATH_PM_NEW => Box::into_raw(Box::new(PathSearch::new(false, 1, h[1]))) as i64,
        EE_PATH_PM_DELETE => {
            let s = unsafe { Box::from_raw(h[0] as *mut PathSearch) };
            s.destroy(raw);
            0
        }
        EE_PATH_PM_MAP => unsafe { search(h[0]) }.goal_map as i64,
        EE_PATH_PM_TYPE => {
            let s = unsafe { search(h[0]) };
            s.type_modifier(raw, i[0]);
            s.machine_type = i[0] as i32;
            0
        }
        EE_PATH_TM_RUN => i64::from(unsafe { search(h[0]) }.run(raw)),
        EE_PATH_TM_FOUND => i64::from(unsafe { search(h[0]) }.m.found),
        EE_PATH_TM_SET_REV_LIMIT => {
            unsafe { search(h[0]) }.goal_mut().rev_limit = i[0] as i32;
            0
        }
        EE_PATH_TM_REV_LIMIT => i64::from(unsafe { search(h[0]) }.goal().rev_limit),
        EE_PATH_TM_STEPS => i64::from(unsafe { search(h[0]) }.m.steps),
        EE_PATH_TM_TYPE => i64::from(unsafe { search(h[0]) }.machine_type),
        EE_PATH_TM_FLAGS => {
            let g = unsafe { search(h[0]) }.goal();
            i64::from(g.flags[ACCEPT_NEAREST]) | (i64::from(g.flags[TOOK_NEAREST]) << 1)
        }
        EE_PATH_TM_POINTS => {
            let s = unsafe { search(h[0]) };
            let points = s.points();
            points_out(h[1], &points);
            0
        }
        EE_PATH_PM_RESET => {
            unsafe { search(h[0]) }.reset(raw);
            0
        }
        EE_PATH_PM_RELEASE => {
            let s = unsafe { search(h[0]) };
            s.reset(raw);
            s.m.found = false;
            if i[0] != 0 {
                s.release_modifier(raw);
                s.set_unit(raw, 0);
            }
            0
        }
        EE_PATH_PM_START => {
            let s = unsafe { search(h[0]) };
            let (from, to) = (tile(&i, 0), tile(&i, 2));
            s.cancel();
            {
                let g = s.goal_mut();
                g.rev_limit = i[4] as i32;
                g.max_revs = i[5] as i32;
                g.start = from;
                g.goal = to;
                g.destination = to;
            }
            s.m.tile_x = to.x;
            s.m.tile_y = to.y;
            s.set_unit(raw, h[1]);
            s.m.check_corners = true;
            0
        }
        EE_PATH_SET_MAX_REVS if i[0] == 0 => {
            unsafe { search(h[0]) }.goal_mut().max_revs = i[1] as i32;
            0
        }
        EE_PATH_PM_SAVE => {
            let s = unsafe { search(h[0]) };
            // SAFETY: the record and the lists are the caller's
            let (out, lists) = unsafe { (&mut *(h[1] as *mut EePathSavedMachine), &mut *(h[2] as *mut EePathSavedLists)) };
            i64::from(s.save(out, lists))
        }
        EE_PATH_PM_LOAD => {
            let s = unsafe { search(h[0]) };
            // SAFETY: the record and the lists are the caller's
            let (rec, lists) = unsafe { (&*(h[1] as *const EePathSavedMachine), &*(h[2] as *const EePathSavedLists)) };
            i64::from(s.load(raw, rec, lists, h[3]))
        }
        EE_PATH_BOUNDED_FREE => {
            if !bounded.is_null() {
                let mut s = unsafe { Box::from_raw(*bounded) };
                s.reset(raw);
                s.destroy(raw);
                *bounded = ptr::null_mut();
            }
            0
        }
        // the storages' node pools: each search frees its nodes with itself
        EE_PATH_FREE_POOLS => 0,
        EE_PATH_BM_PREPARE => {
            // PrepareBoundedMachine: the bounded machine of the map h1 (made again, corner checks on, for another)
            let map = h[1];
            let same = !bounded.is_null() && unsafe { search(*bounded as u64) }.map == map;
            if !same {
                if !bounded.is_null() {
                    let mut s = unsafe { Box::from_raw(*bounded) };
                    s.reset(raw);
                    s.destroy(raw);
                }
                let mut s = Box::new(PathSearch::new(true, TYPE_BOUNDED, map));
                s.m.check_corners = true;
                *bounded = Box::into_raw(s);
            }
            *bounded as i64
        }
        EE_PATH_BM_MODIFIER..=EE_PATH_BM_GOAL_POINTS => {
            let s = unsafe { search(*bounded as u64) };
            bounded_op(s, op, &h, &i, io, raw)
        }
        _ => return None,
    };
    Some(r)
}

/// The bounded machine's operations (pm_bounded.rs `EE_PATH_BM_*`).
fn bounded_op(s: &mut PathSearch, op: i32, h: &[u64; 4], i: &[i64; 12], io: &mut EePathIo, raw: &mut Raw) -> i64 {
    match op {
        EE_PATH_BM_MODIFIER => s.type_modifier(raw, i[0]),
        EE_PATH_BM_SET_MODIFIER => s.set_modifier(raw, h[1]),
        EE_PATH_BM_SET_UNIT => s.set_unit(raw, h[1]),
        EE_PATH_BM_ENDS => {
            let g = s.goal_mut();
            g.start = tile(i, 0);
            g.goal = tile(i, 2);
            if i[4] != 0 {
                s.m.tile_x = i[2] as i32;
                s.m.tile_y = i[3] as i32;
            }
        }
        EE_PATH_BM_SET_MAX_REVS => s.goal_mut().max_revs = i[0] as i32,
        EE_PATH_BM_MAX_REVS => return i64::from(s.goal().max_revs),
        EE_PATH_BM_BOUNDS => {
            // the goal's window (the machine's mSearchBounds, read by nothing, is not kept)
            s.goal_mut().bounds =
                AStarRect { top: i[0] as i32, left: i[1] as i32, bottom: i[2] as i32, right: i[3] as i32 };
        }
        EE_PATH_BM_RUN => return i64::from(s.run(raw)),
        EE_PATH_BM_TILE => {
            io.i[0] = s.m.tile_x.into();
            io.i[1] = s.m.tile_y.into();
        }
        EE_PATH_BM_RESET => s.reset(raw),
        EE_PATH_BM_PATH => {
            let path = s.path();
            tiles_out(raw, h[1], &path);
            if h[2] != 0 {
                let visited = s.visited();
                tiles_out(raw, h[2], &visited);
            }
        }
        EE_PATH_BM_BOUNDED_START => {
            s.cancel();
            {
                let g = s.goal_mut();
                g.max_revs = i[0] as i32;
                g.start = tile(i, 1);
                g.goal = tile(i, 3);
                g.destination = tile(i, 3);
                g.rev_limit = -1;
            }
            s.set_unit(raw, h[1]);
            s.goal_mut().is_bounded = true;
        }
        EE_PATH_BM_RELEASE_MODIFIER => s.release_modifier(raw),
        EE_PATH_BM_RUN_BOUNDED => {
            s.goal_mut().max_revs = i[0] as i32;
            s.set_unit(raw, h[1]);
            {
                let g = s.goal_mut();
                g.flags[ACCEPT_NEAREST] = true;
                g.start = tile(i, 1);
                g.goal = tile(i, 3);
            }
            s.run(raw);
            return i64::from(!s.goal().flags[FAILED]);
        }
        EE_PATH_BM_GOAL_POINTS => {
            let points = s.points();
            points_out(h[1], &points);
        }
        _ => {}
    }
    0
}

/// Appends a tile to a search's tile list (the host's `EE_PATH_BLOCKED_TILES`, `EE_PATH_UNIT_TILES`).
///
/// # Safety
/// `list` the `Vec<EePoint>` the operation was given.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_tiles_push(list: *mut c_void, x: i32, y: i32) {
    unsafe { (*(list as *mut Vec<EePoint>)).push(EePoint { x, y }) };
}

/// A search's members as the path manager's dumps and its PMRQ capture read them.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarPathSearchInfo {
    pub machine_type: i32,
    pub revs: i32,
    pub steps: i32,
    pub found: bool,
    pub max_revs: i32,
    pub rev_limit: i32,
    /// the storage's `mWidth`, `mHeight`; the machine's `mMapWidth`, `mMapHeight`
    pub width: i32,
    pub height: i32,
    pub map_width: i32,
    pub map_height: i32,
    /// with `storage`: the storage's bytes (`storage_save.rs`), their length and FNV-1a 32
    pub storage_len: u32,
    pub storage_hash: u32,
}

impl PathSearch {
    pub fn info(&self, storage: bool) -> EeAStarPathSearchInfo {
        let g = self.goal();
        let mut info = EeAStarPathSearchInfo {
            machine_type: self.machine_type,
            revs: self.m.revs,
            steps: self.m.steps,
            found: self.m.found,
            max_revs: g.max_revs,
            rev_limit: g.rev_limit,
            width: self.st.width,
            height: self.st.height,
            map_width: self.m.map_width,
            map_height: self.m.map_height,
            storage_len: 0,
            storage_hash: 0x811c_9dc5,
        };
        if storage {
            let bytes = self.st.to_bytes();
            info.storage_len = bytes.len() as u32;
            for b in bytes {
                info.storage_hash = (info.storage_hash ^ u32::from(b)).wrapping_mul(16_777_619);
            }
        }
        info
    }
}

/// A search's members (`search` a path machine's handle or the bounded machine); with `storage` its storage digest.
///
/// # Safety
/// `search` a live search; `out` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_path_search_info(search: *const c_void, storage: bool, out: *mut EeAStarPathSearchInfo) {
    unsafe { *out = (*(search as *const PathSearch)).info(storage) };
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::astar::flood::test_view;
    use std::cell::RefCell;

    /// The fake host: the unit's facts, the references and modifier calls it saw.
    #[derive(Default)]
    struct Fake {
        refs: Vec<(i32, u64)>,
        blocked: Vec<EePoint>,
        map_size: i64,
        views: i64,
    }

    thread_local! {
        static WALLS: RefCell<Vec<(i32, i32)>> = const { RefCell::new(Vec::new()) };
    }

    extern "C" fn has_tile(_: *mut c_void, x: i32, y: i32) -> bool {
        (0..8).contains(&x) && (0..8).contains(&y)
    }
    extern "C" fn terrain(_: *mut c_void, x: i32, y: i32) -> i32 {
        WALLS.with(|w| i32::from(w.borrow().contains(&(x, y))))
    }
    extern "C" fn is_edge(_: *mut c_void, _x: i32, _y: i32) -> bool {
        false
    }
    extern "C" fn can_move_on(_: *mut c_void, _: *mut c_void, terrain: i32) -> bool {
        terrain == 0
    }
    extern "C" fn are_passable(_: *mut c_void, _: *mut c_void, _: i32, _: i32, _: crate::astar::modifiers::Skip) -> bool {
        true
    }
    extern "C" fn crowd_cost(_: *mut c_void, _: *mut c_void, _: i32, _: i32) -> i32 {
        0
    }
    extern "C" fn is_wide(_: *mut c_void, _: *mut c_void) -> bool {
        false
    }

    fn host(fake: &mut Fake) -> impl FnMut(i32, &mut EePathIo) -> i64 + '_ {
        move |op, io| match op {
            EE_PATH_MAP_SIZE => fake.map_size,
            EE_PATH_VIEW_NEW => {
                let mut cb = test_view::callbacks();
                cb.has_tile = has_tile;
                cb.terrain = terrain;
                cb.is_edge = is_edge;
                cb.can_move_on = can_move_on;
                cb.are_passable = are_passable;
                cb.crowd_cost = crowd_cost;
                cb.is_wide = is_wide;
                unsafe { *(io.h[2] as *mut EeAStarViewCallbacks) = cb };
                fake.views += 1;
                0x7000
            }
            EE_PATH_VIEW_FREE => {
                fake.views -= 1;
                0
            }
            EE_PATH_UNIT_REFERENCE | EE_PATH_UNIT_DEREFERENCE => {
                fake.refs.push((op, io.h[0]));
                0
            }
            EE_PATH_GOAL_UNIT => {
                io.i[0] = 3;
                io.i[1] = 1;
                0
            }
            EE_PATH_BLOCKED_TILES => {
                for p in &fake.blocked {
                    unsafe { ee_astar_tiles_push(io.h[1] as *mut c_void, p.x, p.y) };
                }
                0
            }
            EE_PATH_UNIT_TILES => 0,
            _ => panic!("unexpected op {op}"),
        }
    }

    #[test]
    fn a_path_search_goes_around_a_wall() {
        WALLS.with(|w| *w.borrow_mut() = vec![(3, 1), (3, 2), (3, 3), (3, 4), (3, 5)]);
        let mut fake = Fake { map_size: 8, ..Default::default() };
        let mut raw = host(&mut fake);
        let mut io = EePathIo::default();
        let mut bounded = ptr::null_mut();
        io.h[1] = 0x5000;
        let h = intercept(&mut bounded, EE_PATH_PM_NEW, &mut io, &mut raw).unwrap() as u64;
        let mut io = EePathIo::default();
        io.h[0] = h;
        io.h[1] = 0x900;
        io.i[..6].copy_from_slice(&[1, 3, 6, 3, -1, -1]);
        intercept(&mut bounded, EE_PATH_PM_START, &mut io, &mut raw).unwrap();
        let s = unsafe { search(h) };
        assert_eq!(s.goal().continent, 3);
        assert!(s.goal().avoid_units);
        assert!(!s.run(&mut raw));
        let path = s.path();
        assert_eq!(path.first(), Some(&EePoint { x: 6, y: 3 }));
        assert_eq!(path.last(), Some(&EePoint { x: 1, y: 3 }));
        assert!(path.iter().all(|p| !(p.x == 3 && (1..=5).contains(&p.y))), "{path:?}");
        assert_eq!(s.goal().start_bounds, AStarRect { bottom: 6, left: 0, right: 4, top: 0 });
        let mut io = EePathIo::default();
        io.h[0] = h;
        intercept(&mut bounded, EE_PATH_PM_DELETE, &mut io, &mut raw).unwrap();
        drop(raw);
        assert_eq!(fake.refs, vec![(EE_PATH_UNIT_REFERENCE, 0x900), (EE_PATH_UNIT_DEREFERENCE, 0x900)]);
        assert_eq!(fake.views, 0);
    }

    #[test]
    fn blocked_tiles_are_marked_before_the_search() {
        WALLS.with(|w| w.borrow_mut().clear());
        let mut fake = Fake { map_size: 8, blocked: vec![EePoint { x: 2, y: 2 }], ..Default::default() };
        let mut raw = host(&mut fake);
        let mut s = PathSearch::new(false, 1, 0x5000);
        s.set_unit(&mut raw, 0x900);
        s.goal_mut().start = EePoint { x: 0, y: 2 };
        s.goal_mut().goal = EePoint { x: 4, y: 2 };
        TEST_NOTES.with(|n| n.borrow_mut().clear());
        s.run(&mut raw);
        let notes = TEST_NOTES.with(|n| n.borrow().clone());
        assert_eq!(notes[0], (EV_MARK_TILE, [2, 2, 0, 0, 0], 0));
        assert!(!s.path().contains(&EePoint { x: 2, y: 2 }));
        // the goal's offsets on the points
        assert_eq!(s.points()[0], EePathPoint { x: 4.5, y: 2.5, z: 0.0 });
        s.destroy(&mut raw);
    }

    #[test]
    fn the_saved_search_comes_back() {
        WALLS.with(|w| w.borrow_mut().clear());
        let mut fake = Fake { map_size: 8, ..Default::default() };
        let mut raw = host(&mut fake);
        let mut s = PathSearch::new(false, 1, 0x5000);
        s.set_unit(&mut raw, 0x900);
        s.goal_mut().start = EePoint { x: 0, y: 0 };
        s.goal_mut().goal = EePoint { x: 7, y: 7 };
        s.goal_mut().max_revs = 3;
        s.run(&mut raw);
        let mut rec = EePathSavedMachine::default();
        let mut lists = EePathSavedLists::default();
        assert!(s.save(&mut rec, &mut lists));
        let mut t = PathSearch::new(false, 1, 0x5000);
        let mut raw2 = |op: i32, io: &mut EePathIo| if op == EE_PATH_MAP { 0x5000 } else { raw(op, io) };
        assert!(t.load(&mut raw2, &rec, &lists, 0x901));
        let mut rec2 = EePathSavedMachine::default();
        let mut lists2 = EePathSavedLists::default();
        assert!(t.save(&mut rec2, &mut lists2));
        assert_eq!(rec, rec2);
        assert_eq!(lists, lists2);
        assert_eq!(t.goal().unit as u64, 0x901);
        t.destroy(&mut raw2);
        s.destroy(&mut raw);
    }
}
