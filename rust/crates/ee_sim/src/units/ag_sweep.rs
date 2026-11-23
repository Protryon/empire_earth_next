//! TEST ONLY: the active groups' sweep (AGPL records; R5b.4 round 35, the golden of the active group's bodies),
//! moved whole from EEActiveGroupSweep.cpp (the units' lane, round 5). No capture game makes an active group plan
//! (EE-SIM's human idles, the computer players send only walls and the unit sweep sets goals directly), so with
//! EE_SIM_CAPTURE and --group-sweep (the capture's groups profile), when the game ends, every player's units get real
//! group commands: an `EEActiveGroup` of its first mobile units (and its airports and navy yards) for each case
//! (moves far and near, alone and together, queued; attacks on a unit and on the ground; gather and drop off; heal;
//! explore; patrol; unload; disband; the four spreads; the reserved destination tests), each planned as the
//! commands plan it (`SetGoal`, `mAction`, the player's pending groups, `Plan`). After each case one record (the
//! host's: the group as left, every selected unit's goal, queues, command, place in the formation and active group,
//! the player's pending groups, and one random draw). The game is over: nothing simulates after it. Seed 6 faults in
//! its sweep (a group path found at once hands a unit `SetGoal(goal, true)`, whose keep-current branch reads
//! `mCommand->mType`); its golden is not recorded.
//!
//! The units are chosen and the records written over the views; the C++ host (EEPhysicsSweep.cpp
//! `ee_units_sweep_host`) makes the groups and the goals and answers the members' calls, one call each; the spreads,
//! keeps and reserved tests are the group's Rust bodies (ag_host.rs).

use crate::app::capture_record;
#[cfg(not(test))]
use crate::units::ag_host;

/// The group bodies the sweep calls (ag_host.rs; a unit test reaches none of them).
#[cfg(test)]
mod ag_host {
    pub fn ee_units_ag_spread_around_unit(_g: u64, _t: u64, _x: i32, _y: i32) {}
    pub fn ee_units_ag_spread_around_resource(_g: u64, _t: u64) {}
    pub fn ee_units_ag_spread_around_tile(_g: u64, _x: i32, _y: i32, _u: u8) {}
    pub fn ee_units_ag_keep_attackers(_g: u64) {}
    pub fn ee_units_ag_keep_mobile(_g: u64) {}
    pub unsafe fn ee_units_ag_reserved(_g: u64, _p: *const [f32; 3], _w: u8) -> u8 {
        0
    }
}
use crate::units::ag_fields::EeUnitsActiveGroupFields;
use crate::units::unit_geom::position;
use crate::units::unit_sweep as op;
use crate::units::unit_sweep::{set_point, Game, SweepHost};
use crate::world::{UnitView, WorldRead, WorldView};

/// The goals the sweep makes (`EE_UNITS_SWEEP_OP_GOAL_MOVE` + the kind).
pub const EE_UNITS_SWEEP_GOAL_MOVE: u64 = 0;
pub const EE_UNITS_SWEEP_GOAL_ATTACK_UNIT: u64 = 1;
pub const EE_UNITS_SWEEP_GOAL_ATTACK_GROUND: u64 = 2;
pub const EE_UNITS_SWEEP_GOAL_GATHER: u64 = 3;
pub const EE_UNITS_SWEEP_GOAL_HEAL: u64 = 4;
pub const EE_UNITS_SWEEP_GOAL_EXPLORE: u64 = 5;
pub const EE_UNITS_SWEEP_GOAL_UNLOAD: u64 = 6;
pub const EE_UNITS_SWEEP_GOAL_PATROL: u64 = 7;
/// not a goal: player `unit`'s first town patrol group with a last point (0: none)
pub const EE_UNITS_SWEEP_GOAL_FIND_PATROL: u64 = 8;

// ee/EEUnitAction.h
const ACTION_PREPARE_TO_MOVE: i64 = 8000;
const ACTION_ATTACK_ENEMY_UNIT: i64 = 2001;
const ACTION_ATTACK_LOCATION: i64 = 2002;
const ACTION_GATHER_RESOURCE: i64 = 4004;
const ACTION_DROPOFF_RESOURCE: i64 = 4011;
const ACTION_HEAL_UNIT: i64 = 16001;
const ACTION_FIND_UNEXPLORED_AREA: i64 = 11002;
const ACTION_UNLOAD_TRANSPORT: i64 = 12001;
const ACTION_DISBAND: i64 = 7001;
const ACTION_MOVE_TO_NEXT_PATROL_POINT: i64 = 8013;
// ee/EEUnitClassIDs.h
const TYPE_AIRPORT: u64 = 0x42;
const TYPE_NAVY_YARD: u64 = 0x41;

const TAG: u32 = u32::from_le_bytes(*b"AGPL");

struct Sweep<'h> {
    h: &'h mut dyn SweepHost,
    player: u64,
    case: i64,
}

impl Sweep<'_> {
    /// A new active group of the player: the units that live and are valid added, formation kind `formation` set
    fn group(&mut self, units: &[u64], formation: i64) -> u64 {
        let g = self.h.op(op::EE_UNITS_SWEEP_OP_GROUP_NEW, self.player, 0, 0, 0) as u64;
        for &u in units {
            // SAFETY: the sweep runs on the physics thread inside its hold of the world
            let t = unsafe { WorldRead::begin() };
            if view(&t, u).is_some_and(|v| v.hit_points() > 0 && v.valid()) {
                self.h.op(op::EE_UNITS_SWEEP_OP_ADD_MEMBER, g, u, 0, 0);
            }
        }
        let f = self.h.op(op::EE_UNITS_SWEEP_OP_FORMATION_CREATE, formation as u64, 0, 0, 0) as u64;
        if f != 0 {
            self.h.op(op::EE_UNITS_SWEEP_OP_SET_FORMATION, g, f, 0, 0);
            self.h.op(op::EE_UNITS_SWEEP_OP_FORMATION_DEREFERENCE, f, 0, 0, 0);
        }
        g
    }
    /// A goal of `kind` for `unit` at `target` and the point (a move's yaw its fourth float, its `flags` bit 0: the
    /// group moves together); [`EE_UNITS_SWEEP_GOAL_FIND_PATROL`]: player `unit`'s first town patrol group with a
    /// last point (0: none)
    fn goal(&mut self, kind: u64, unit: u64, target: u64, point: [f32; 4], flags: i64) -> u64 {
        if kind == EE_UNITS_SWEEP_GOAL_FIND_PATROL {
            return find_patrol(self.h, unit);
        }
        set_point(point);
        let g = self.h.op(op::EE_UNITS_SWEEP_OP_GOAL_MOVE + kind as i32, unit, target, 0, 0) as u64;
        if kind == EE_UNITS_SWEEP_GOAL_MOVE {
            self.h.op(op::EE_UNITS_SWEEP_OP_GROUP_MOVEMENT, g, (flags & 1) as u64, 0, 0);
        }
        g
    }
    /// `Move(point, together, yaw)`
    fn moving(&mut self, p: [f32; 3], together: bool, yaw: f32) -> u64 {
        self.goal(EE_UNITS_SWEEP_GOAL_MOVE, 0, 0, [p[0], p[1], p[2], yaw], i64::from(together))
    }
    /// `Run(group, goal, action, queued)`; `action` < 0: the goal set alone (the spreads'); `special` the group's
    /// `mIsSpecialAttack` first
    fn run(&mut self, g: u64, goal: u64, action: i64, queued: bool, special: bool) {
        if special {
            group_fields(g, |f| f.mIsSpecialAttack = true);
        }
        self.h.op(op::EE_UNITS_SWEEP_OP_GROUP_SET_GOAL, g, goal, i64::from(queued), 0);
        if action >= 0 {
            // the commands' way: the action, into the pending groups, Plan
            group_fields(g, |f| f.mAction = action as std::os::raw::c_long);
            self.h.op(op::EE_UNITS_SWEEP_OP_ADD_PENDING_GROUP, g, 0, 0, 0);
            self.h.op(op::EE_UNITS_SWEEP_OP_PLAN, g, 0, 0, 0);
        }
    }
    fn record(&mut self, g: u64, units: &[u64], answers: u32) {
        let case = self.case;
        self.case += 1;
        record_call(self.h, case, self.player, g, units, answers);
    }
}

fn usable(u: &UnitView<'_>) -> bool {
    let Some(c) = u.complex_unit() else { return false };
    u.hit_points() > 0
        && c.valid()
        && c.has_tile_node()
        && c.tile_xy().0 != -10000
        && c.stats().is_some()
        && c.class().is_some()
}

/// `EEActiveGroupCaptureSweep()` over a host.
pub fn sweep(h: &mut dyn SweepHost) {
    // SAFETY: the physics thread's sweep, at the game's end, reads the world it steps
    let t = unsafe { WorldRead::begin() };
    let Some(world) = WorldView::global(&t) else { return };
    let n = world.num_players() as usize;
    // the resources (gaia's first units with an amount) and each player's newest usable unit (the targets)
    let mut resources = Vec::new();
    let mut newest = vec![0u64; n];
    for (p, last) in newest.iter_mut().enumerate() {
        let Some(player) = world.player(p) else { continue };
        for u in player.units() {
            if p == 0 && u.resource_amount() > 0 && u.has_tile_node() && resources.len() < 4 {
                resources.push(u.handle());
            }
            if p != 0 && usable(&u) {
                *last = u.complex_unit_handle();
            }
        }
    }
    for p in 1..n {
        let Some(player) = world.player(p) else { continue };
        let (mut mobile, mut buildings) = (Vec::new(), Vec::new());
        for u in player.units() {
            if !usable(&u) {
                continue;
            }
            let Some(c) = u.complex_unit() else { continue };
            let (Some(stats), Some(class)) = (c.stats(), c.class()) else { continue };
            let velocity: f32 = crate::world_field!(stats.fields(), mMaxVelocity);
            let ty = class.unit_type();
            if velocity > 0.0 && mobile.len() < 10 {
                mobile.push(c.handle());
            } else if (ty == TYPE_AIRPORT || ty == TYPE_NAVY_YARD || class.tile_size() > 1) && buildings.len() < 3 {
                buildings.push(c.handle());
            }
        }
        if mobile.is_empty() {
            continue;
        }
        let mut all = mobile.clone();
        all.extend_from_slice(&buildings);
        let mut target = 0;
        for k in 1..n {
            if target != 0 {
                break;
            }
            target = newest[(p + k) % n];
        }
        // SAFETY: live units of the world (the views' handles)
        let view = |u: u64| unsafe { UnitView::of(&t, u) }.expect("a live unit");
        if target == 0 || view(target).player().map(|q| q.handle()) == Some(player.handle()) {
            target = *mobile.last().expect("a mobile unit");
        }
        let far = position(&view(target));
        let mut near = position(&view(mobile[0]));
        near[0] += 2.0;
        near[1] += 2.0;
        let resource = if resources.is_empty() { 0 } else { resources[p % resources.len()] };
        let storage = buildings.first().copied().unwrap_or(mobile[0]);
        let (tx, ty) = view(target).tile_xy();
        let mut s = Sweep { h: &mut *h, player: player.handle(), case: 0 };
        // moves: far and alone, far and together, near and queued, near and together, every formation kind
        for f in 0..8i64 {
            let g = s.group(if f < 4 { &all } else { &mobile }, f);
            let goal = s.moving(if f & 2 != 0 { near } else { far }, f & 1 != 0, 0.25 * f as f32);
            s.run(g, goal, ACTION_PREPARE_TO_MOVE, f >= 4, false);
            s.record(g, &all, 0);
        }
        // attacks: a unit (special), a unit queued, the ground
        for k in 0..3 {
            let g = s.group(&all, 1);
            let goal = if k < 2 {
                s.goal(EE_UNITS_SWEEP_GOAL_ATTACK_UNIT, mobile[0], target, [0.0; 4], 0)
            } else {
                s.goal(EE_UNITS_SWEEP_GOAL_ATTACK_GROUND, 0, 0, [far[0], far[1], far[2], 0.0], 0)
            };
            let action = if k < 2 { ACTION_ATTACK_ENEMY_UNIT } else { ACTION_ATTACK_LOCATION };
            s.run(g, goal, action, k == 1, k == 0);
            s.record(g, &all, 0);
        }
        // gather, then drop off, plain and queued
        if resource != 0 {
            for k in 0..4 {
                let g = s.group(&mobile, 0);
                let goal = s.goal(EE_UNITS_SWEEP_GOAL_GATHER, resource, storage, [0.0; 4], 0);
                let action = if k < 2 { ACTION_GATHER_RESOURCE } else { ACTION_DROPOFF_RESOURCE };
                s.run(g, goal, action, k & 1 != 0, false);
                s.record(g, &mobile, 0);
            }
        }
        // heal, explore, unload, disband
        let g = s.group(&all, 2);
        let goal = s.goal(EE_UNITS_SWEEP_GOAL_HEAL, *mobile.last().expect("a unit"), 0, [0.0; 4], 0);
        s.run(g, goal, ACTION_HEAL_UNIT, false, false);
        s.record(g, &all, 0);
        let g = s.group(&all, 3);
        let goal = s.goal(EE_UNITS_SWEEP_GOAL_EXPLORE, 0, 0, [0.0; 4], 0);
        s.run(g, goal, ACTION_FIND_UNEXPLORED_AREA, false, false);
        s.record(g, &all, 0);
        let g = s.group(&all, 4);
        let goal = s.goal(EE_UNITS_SWEEP_GOAL_UNLOAD, mobile[0], 0, [tx as f32, ty as f32, 0.0, 0.0], 0);
        s.run(g, goal, ACTION_UNLOAD_TRANSPORT, false, false);
        s.record(g, &all, 0);
        let g = s.group(&mobile, 0);
        let goal = s.moving(far, false, 0.0);
        s.run(g, goal, ACTION_DISBAND, false, false);
        s.record(g, &mobile, 0);
        // patrol along the player's first town patrol with a last point (the host finds it)
        let patrol = s.goal(EE_UNITS_SWEEP_GOAL_FIND_PATROL, player.handle(), 0, [0.0; 4], 0);
        if patrol != 0 {
            let g = s.group(&all, 5);
            let goal = s.goal(EE_UNITS_SWEEP_GOAL_PATROL, 0, patrol, [0.0; 4], 0);
            s.run(g, goal, ACTION_MOVE_TO_NEXT_PATROL_POINT, false, false);
            s.record(g, &all, 0);
        }
        // the spreads (a fresh group of all the units each)
        let g = s.group(&all, 1);
        let goal = s.moving(far, false, 0.0);
        s.run(g, goal, -1, false, false);
        ag_host::ee_units_ag_spread_around_unit(g, target, tx as i32, ty as i32);
        s.record(g, &all, 0);
        if resource != 0 {
            let g = s.group(&all, 1);
            let goal = s.moving(far, false, 0.0);
            s.run(g, goal, -1, true, false);
            ag_host::ee_units_ag_spread_around_resource(g, resource);
            s.record(g, &all, 0);
        }
        let (sx, sy) = view(storage).tile_xy();
        let g = s.group(&all, 2);
        let goal = s.moving(far, false, 0.0);
        s.run(g, goal, -1, false, false);
        ag_host::ee_units_ag_spread_around_tile(g, sx as i32, sy as i32, 0);
        s.record(g, &all, 0);
        let g = s.group(&all, 2);
        let goal = s.moving(far, false, 0.0);
        s.run(g, goal, -1, false, false);
        ag_host::ee_units_ag_spread_around_tile(g, tx as i32, ty as i32, 1);
        s.record(g, &all, 0);
        let g = s.group(&all, 0);
        ag_host::ee_units_ag_keep_attackers(g);
        s.record(g, &all, 0);
        let g = s.group(&all, 0);
        ag_host::ee_units_ag_keep_mobile(g);
        s.record(g, &all, 0);
        // the reserved destinations: every unit's position told, then points at, near and away from them
        let g = s.group(&all, 0);
        for &u in &all {
            let at = op::unit_position(s.h, u);
            set_point([at[0], at[1], at[2], 0.0]);
            s.h.op(op::EE_UNITS_SWEEP_OP_TELL, g, u, 0, 0);
        }
        let mut answers = 0u32;
        for &u in all.iter().take(5) {
            let at = position(&view(u));
            for o in [0.0f32, 0.2, 7.0] {
                let probe = [at[0] + o, at[1] + o, at[2]];
                // SAFETY: a live point
                let reserved = unsafe { ag_host::ee_units_ag_reserved(g, &probe, 0) } != 0;
                // SAFETY: as above
                let taken = unsafe { ag_host::ee_units_ag_reserved(g, &probe, 1) } != 0;
                answers = answers << 2 | u32::from(reserved) | u32::from(taken) << 1;
            }
        }
        s.record(g, &all, answers);
    }
}

/// Unit `u`'s view.
fn view<'t>(t: &'t WorldRead, u: u64) -> Option<UnitView<'t>> {
    // SAFETY: a live unit the sweep handed out
    unsafe { UnitView::of(t, u) }
}

/// A store on group `g`'s record (`EeUnitsActiveGroupFields`; the group's handle is its `EEActiveGroup*`).
fn group_fields(g: u64, f: impl FnOnce(&mut EeUnitsActiveGroupFields)) {
    #[cfg(not(test))]
    {
        let at = g as usize + crate::world::layout::layout().active_group_fields as usize;
        // SAFETY: a live active group's record; the borrow ends inside `f`
        f(unsafe { &mut *(at as *mut EeUnitsActiveGroupFields) })
    }
    #[cfg(test)]
    let _ = (g, f);
}

/// Player `player`'s first town patrol group with a last point (0: none).
fn find_patrol(h: &mut dyn SweepHost, player: u64) -> u64 {
    // SAFETY: as [`view`]
    let t = unsafe { WorldRead::begin() };
    // SAFETY: the live player the sweep handed out
    let Some(p) = (unsafe { crate::world::PlayerView::of(&t, player) }) else { return 0 };
    for i in 0..p.towns_len() {
        let town = p.town_handle(i);
        if town == 0 {
            continue;
        }
        let patrol = h.op(op::EE_UNITS_SWEEP_OP_TOWN_PATROL, town, 0, 0, 0) as u64;
        if patrol != 0 && last_point(patrol) != 0 {
            return patrol;
        }
    }
    0
}

#[cfg(not(test))]
fn last_point(patrol: u64) -> u64 {
    // SAFETY: a live patrol group
    unsafe { crate::units::patrol_group::ee_units_pg_last(patrol as usize as *mut _) as u64 }
}
#[cfg(test)]
fn last_point(_patrol: u64) -> u64 {
    0
}

/// The record's goal: its kind (-1: none), id and location.
fn record_goal(h: &mut dyn SweepHost, r: &mut capture_record::Record, goal: u64) {
    if goal == 0 {
        r.i32(-1);
        return;
    }
    let (kind, id) = goal_kind_id(goal);
    r.i32(i32::from(kind));
    r.u32(id);
    set_point([0.0; 4]);
    h.op(op::EE_UNITS_SWEEP_OP_GOAL_LOCATION, goal, 0, 0, 0);
    let p = op::point();
    r.f32(p[0]);
    r.f32(p[1]);
    r.f32(p[2]);
}

#[cfg(not(test))]
fn goal_kind_id(goal: u64) -> (u8, u32) {
    let l = crate::world::layout::layout();
    // SAFETY: a live unit's goal
    unsafe {
        let kind = std::ptr::read_unaligned((goal as usize + l.goal_kind as usize) as *const u8);
        let id = std::ptr::read_unaligned((goal as usize + l.goal_id as usize) as *const std::os::raw::c_ulong);
        (kind, id as u32)
    }
}
#[cfg(test)]
fn goal_kind_id(goal: u64) -> (u8, u32) {
    (goal as u8, goal as u32)
}

/// The group's units (`GetNumUnits`, `GetUnit`) and its record (`mFields`): its 17 speeds from `mSpeed` on, whether
/// it moves together.
#[cfg(not(test))]
fn group_state(g: u64) -> (Vec<u64>, [f32; 17], bool) {
    let group = g as usize as *mut crate::units::group_obj::EeUnitsGroup;
    // SAFETY: a live group
    let n = unsafe { crate::units::group_host::ee_units_group_len(group) };
    let units = (0..n)
        .map(|i| unsafe { crate::units::group_host::ee_units_group_unit(group, i as std::os::raw::c_long) } as u64)
        .collect();
    let at = g as usize + crate::world::layout::layout().active_group_fields as usize;
    // SAFETY: the live group's record: 17 floats from mSpeed on, as the C++ record wrote them
    unsafe {
        let f = &*(at as *const EeUnitsActiveGroupFields);
        (units, std::ptr::read_unaligned(std::ptr::addr_of!(f.mSpeed) as *const [f32; 17]), f.mMovesTogether)
    }
}
#[cfg(test)]
fn group_state(_g: u64) -> (Vec<u64>, [f32; 17], bool) {
    (Vec::new(), [0.0; 17], false)
}

/// The AGPL record: the group as left, every unit's goal, queues, command, formation place and group, one draw.
// `c_ulong`/`c_long` are 32 bits on Windows and 64 elsewhere: the casts are needed there
#[allow(clippy::unnecessary_cast)]
fn record_call(h: &mut dyn SweepHost, case: i64, player: u64, g: u64, units: &[u64], answers: u32) {
    let mut r = capture_record::Record::new(TAG);
    r.i32(case as i32);
    // SAFETY: as [`view`]
    let t = unsafe { WorldRead::begin() };
    // SAFETY: the live player
    r.u32(unsafe { crate::world::PlayerView::of(&t, player) }.map_or(0, |p| p.id()));
    r.u32(answers);
    let (members, speeds, together) = group_state(g);
    r.u32(members.len() as u32);
    for &m in &members {
        r.u32(view(&t, m).map_or(0, |v| v.id()));
    }
    for v in speeds {
        r.f32(v);
    }
    r.u32(u32::from(together));
    r.u32(h.op(op::EE_UNITS_SWEEP_OP_PENDING_GROUPS, player, 0, 0, 0) as u32);
    for &u in units {
        let Some(v) = view(&t, u) else { continue };
        r.u32(v.id());
        r.i32(v.hit_points() as i32);
        record_goal(h, &mut r, v.goal_handle());
        let queues = v.goal_queues();
        r.u32(queues.len() as u32);
        for q in queues {
            let goals = queue_goals(q);
            r.u32(goals.len() as u32);
            for goal in goals {
                record_goal(h, &mut r, goal);
            }
        }
        let c = v.complex();
        r.u32(v.command_type());
        r.u32(crate::world_field!(c, mFormationSlot) as u32);
        r.i32(crate::world_field!(c, mFormationRank) as i32);
        r.u32(u32::from(crate::world_field!(c, mIsSpecialAttack)));
        r.u32(u32::from(v.active_group_handle() == g));
        r.u32(u32::from(v.active_group_handle() != 0));
    }
    r.i32(h.op(op::EE_UNITS_SWEEP_OP_RANDOM, 0, 0, 0, 0) as i32);
    r.commit();
}

/// A goal queue's goals in order.
#[cfg(not(test))]
fn queue_goals(q: u64) -> Vec<u64> {
    let q = q as usize as *const crate::units::goals::EeUnitsGoalQueue;
    // SAFETY: a live unit's queue
    let n = unsafe { crate::units::goals::ee_units_gq_len(q) };
    (0..n).map(|k| unsafe { crate::units::goals::ee_units_gq_get(q, k) }).collect()
}
#[cfg(test)]
fn queue_goals(_q: u64) -> Vec<u64> {
    Vec::new()
}

/// `EEActiveGroupCaptureSweep()` (EE-SIM's game end, with the capture's groups profile): the capture only takes AGPL
/// records while it runs, and alone.
#[no_mangle]
pub extern "C" fn ee_units_ag_sweep() {
    if !capture_record::active() {
        return;
    }
    let tag = TAG;
    // SAFETY: one tag
    unsafe { crate::app::capture::ee_app_capture_only(&tag, 1) };
    crate::app::capture::ee_app_capture_exclusive(true);
    sweep(&mut Game);
    crate::app::capture::ee_app_capture_exclusive(false);
    // SAFETY: no tag
    unsafe { crate::app::capture::ee_app_capture_only(std::ptr::null(), 0) };
}

#[cfg(test)]
#[path = "ag_sweep_tests.rs"]
mod tests;
