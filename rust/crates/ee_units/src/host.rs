//! The world calls a Rust decision makes (RUST_PLAN R5b.4 leaf 5, stage one): the host is the C++ game
//! (`src/Empire Earth/Units/EEUnitHost.cpp`, `include/ee/EEUnitStore.h`). A decision reads units' plain state from
//! the store (`store`) and acts through these calls; it gets the objects it acts on as opaque references and never
//! dereferences them.
//!
//! - A group (`EEGroup*`: the sender's selected group, an active group a body builds) is a `HostRef`; its units are
//!   read as ids (`Host::group_ids`: the group's vector in its order, the game's order).
//! - A goal is created by the host from a `EeGoalSpec` (the constructor and its arguments, units by id) and comes
//!   back as a `GoalHandle` that owns it until a host call consumes it or `Host::goal_release` deletes it.
//!
//! No Rust reference lives across a host call: the calls take and return plain values.

use std::ffi::c_void;
use std::ptr::NonNull;

use crate::store::EeUnitRecord;

/// The goal constructors the host builds (`EeGoalSpec::ctor`; ee/EEUnitStore.h `EEGoalConstructor`).
/// `new EEUGExplore(flag_a)`
pub const EE_GOAL_CTOR_EXPLORE: u32 = 1;
/// `new EEUGAttackGround(point, flag_a)`
pub const EE_GOAL_CTOR_ATTACK_GROUND: u32 = 2;
/// `new EEUGAttackUnit(unit_a, unit_b, flag_a, flag_b)` (the attacker's complex unit, the target, the third
/// argument, convert)
pub const EE_GOAL_CTOR_ATTACK_UNIT: u32 = 3;
/// `new EEUGGather(unit_a, unit_b, flag_a)` (the resource, the drop-off; either may be none)
pub const EE_GOAL_CTOR_GATHER: u32 = 4;
/// `new EEUGHealUnit(unit_a, flag_a)` (the healed complex unit)
pub const EE_GOAL_CTOR_HEAL_UNIT: u32 = 5;
/// `new EEUGPatrol(flag_a)`
pub const EE_GOAL_CTOR_PATROL: u32 = 6;
/// `new EEUGPatrol(object, unit_a, flag_a)` (the route, a patrol group from `Host::unit_patrol_group`; the target)
pub const EE_GOAL_CTOR_PATROL_ROUTE: u32 = 7;

/// "no unit" in a spec (`EEUnitHandle::kNone`)
pub const EE_UNIT_ID_NONE: u32 = 0xffff_ffff;

/// A goal to create: the constructor and its arguments, units by id (resolved by the host through the registry).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGoalSpec {
    pub ctor: u32,
    pub unit_a: u32,
    pub unit_b: u32,
    /// a location (`GE3DPoint`)
    pub point: [f32; 3],
    pub flag_a: u8,
    pub flag_b: u8,
    /// an object the host handed out for the goal (a route)
    pub object: *mut c_void,
}

impl EeGoalSpec {
    pub fn new(ctor: u32) -> Self {
        EeGoalSpec {
            ctor,
            unit_a: EE_UNIT_ID_NONE,
            unit_b: EE_UNIT_ID_NONE,
            point: [0.0; 3],
            flag_a: 0,
            flag_b: 0,
            object: std::ptr::null_mut(),
        }
    }

    /// The goal kind (`EEUnitGoal::mKind`) the constructor gives, or None for one the host does not build.
    pub fn kind(&self) -> Option<u8> {
        Some(match self.ctor {
            EE_GOAL_CTOR_EXPLORE => 10,
            EE_GOAL_CTOR_ATTACK_GROUND => 3,
            EE_GOAL_CTOR_ATTACK_UNIT => 2,
            EE_GOAL_CTOR_GATHER => 6,
            EE_GOAL_CTOR_HEAL_UNIT => 15,
            EE_GOAL_CTOR_PATROL | EE_GOAL_CTOR_PATROL_ROUTE => 9,
            _ => return None,
        })
    }
}

/// An object the host owns (a group), as Rust holds it: an address handed back to the host, never read.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct HostRef(pub *mut c_void);

/// A goal the host created; it owns the goal until a host call consumes it or `Host::goal_release` deletes it.
#[derive(Debug, PartialEq, Eq)]
pub struct GoalHandle(NonNull<c_void>);

impl GoalHandle {
    pub fn as_ptr(&self) -> *mut c_void {
        self.0.as_ptr()
    }
    /// The handle of a goal the host just created (null: none)
    pub fn from_ptr(goal: *mut c_void) -> Option<Self> {
        NonNull::new(goal).map(GoalHandle)
    }
}

/// The host's calls. A group made by `active_group_new` is the body's until `pending_push` hands it to the player
/// or `group_release` drops it.
pub trait Host {
    /// A unit's record in the store (a copy: no store lock is held across a host call), None when the unit is in no
    /// player's list.
    fn unit(&mut self, id: u32) -> Option<EeUnitRecord>;
    /// The group's units as ids, in the group's order.
    fn group_ids(&mut self, group: HostRef, out: &mut Vec<u32>);
    /// A new goal, or None when the host builds no goal of that constructor (or a unit it names is gone).
    fn goal_new(&mut self, spec: &EeGoalSpec) -> Option<GoalHandle>;
    /// The kind of a goal (`EEUnitGoal::mKind`).
    fn goal_kind(&mut self, goal: &GoalHandle) -> u8;
    /// Deletes a goal no call consumed.
    fn goal_release(&mut self, goal: GoalHandle);
    /// `new EEActiveGroup(player)`
    fn active_group_new(&mut self, player: HostRef) -> HostRef;
    /// `EEActiveGroup::Fill(selection, sort, slot)` (its test is `group::fill_members`)
    fn group_fill(&mut self, group: HostRef, selection: HostRef, sort: i32, slot: i32);
    /// `group->Dereference()`: a group nothing took
    fn group_release(&mut self, group: HostRef);
    /// `player->mPendingGroups.push_back(group)` (the player takes the body's reference)
    fn pending_push(&mut self, player: HostRef, group: HostRef);
    /// `EEActiveGroup::KeepMobileUnits`
    fn group_keep_mobile(&mut self, group: HostRef);
    /// `EEActiveGroup::ClearGoal`
    fn group_clear_goal(&mut self, group: HostRef);
    /// `EEActiveGroup::SetGoal(goal, queued)`: the group takes the goal
    fn group_set_goal(&mut self, group: HostRef, goal: GoalHandle, queued: bool);
    /// `group->mAction = action`
    fn group_set_action(&mut self, group: HostRef, action: i32);
    /// `EEActiveGroup::Plan`
    fn group_plan(&mut self, group: HostRef);
    /// `gPhysics->GetRandom(low, high)` (the simulation's draw)
    fn random(&mut self, low: i32, high: i32) -> i32;
    /// `player->PlayAcknowledgement(group->GetUnit(index))`
    fn play_acknowledgement(&mut self, player: HostRef, group: HostRef, index: i32);
    /// `gWorld.GetMap()->GetHeight(point)` for a point at (x, y)
    fn map_height(&mut self, x: f32, y: f32) -> f32;
    /// `EEActiveGroup::KeepGroundAttackers` (its test is `group::keeps_ground_attacker` over the store)
    fn group_keep_ground_attackers(&mut self, group: HostRef);
    /// `EESendGroupCommand(group, target (EE_UNIT_ID_NONE: NULL), player->mPlayerID, command, queue)`: the local
    /// command markers
    fn send_group_command(&mut self, group: HostRef, target: u32, player: HostRef, command: i32, queue: i32);
    /// `player->PlayAcknowledgement(unit)`
    fn acknowledge_unit(&mut self, player: HostRef, unit: u32);
    /// `unit->FindNearestTown()`: None when there is none or it has no patrol group, else its patrol group's point
    /// nearest to the unit's tile (`GetNearest`; EE_UNIT_ID_NONE: the route has no point)
    fn town_patrol_point(&mut self, unit: u32) -> Option<u32>;
    /// `unit->mPatrolGroup->AddPoint(GE2DPoint(target's tile), player->mPlayerID)`
    fn patrol_add_unit_tile(&mut self, unit: u32, target: u32, player: HostRef);
    /// `unit->mPatrolGroup` (a route, for a goal spec's `object`)
    fn unit_patrol_group(&mut self, unit: u32) -> HostRef;
    /// `unit->mUnitClass->mUnitType` read on the object (-1: no class), for a unit the store does not hold
    fn object_unit_type(&mut self, unit: u32) -> i64;
    /// `group->mIsQueued = queued`
    fn group_set_queued(&mut self, group: HostRef, queued: bool);
    /// `EEActiveGroup::SpreadAroundResource(target)`
    fn group_spread_around_resource(&mut self, group: HostRef, target: u32);
    /// `EEActiveGroup::SpreadAroundTile(x, y)`
    fn group_spread_around_tile(&mut self, group: HostRef, x: i64, y: i64);
    /// `unit->mComplexUnit->AddBuildTarget(target)`, then its `mHasShiftQueuedBuild = true`
    fn unit_add_build_target(&mut self, unit: u32, target: u32);
    /// `unit->mComplexUnit`'s `OrderRepair(target)`, `StartAction(kActionIdle)`, `mUnitClass->ActionChanged(it)`
    fn unit_order_repair(&mut self, unit: u32, target: u32);
    /// `group->AddMember(unit->mComplexUnit->mComplexUnit, true)`
    fn group_add_member(&mut self, group: HostRef, unit: u32);
}

/// The game's host (C++). Absent from a test binary.
#[cfg(not(test))]
pub mod game {
    use super::*;

    extern "C" {
        fn ee_units_host_group_ids(group: *mut c_void, out: *mut u32, capacity: usize) -> usize;
        fn ee_units_host_goal_new(spec: *const EeGoalSpec) -> *mut c_void;
        fn ee_units_host_goal_kind(goal: *mut c_void) -> u8;
        fn ee_units_host_goal_release(goal: *mut c_void);
        fn ee_units_host_active_group_new(player: *mut c_void) -> *mut c_void;
        fn ee_units_host_group_fill(group: *mut c_void, selection: *mut c_void, sort: i32, slot: i32);
        fn ee_units_host_group_release(group: *mut c_void);
        fn ee_units_host_pending_push(player: *mut c_void, group: *mut c_void);
        fn ee_units_host_group_keep_mobile(group: *mut c_void);
        fn ee_units_host_group_clear_goal(group: *mut c_void);
        fn ee_units_host_group_set_goal(group: *mut c_void, goal: *mut c_void, queued: u8);
        fn ee_units_host_group_set_action(group: *mut c_void, action: i32);
        fn ee_units_host_group_plan(group: *mut c_void);
        fn ee_units_host_random(low: i32, high: i32) -> i32;
        fn ee_units_host_play_acknowledgement(player: *mut c_void, group: *mut c_void, index: i32);
        fn ee_units_host_map_height(x: f32, y: f32) -> f32;
        fn ee_units_host_group_keep_ground_attackers(group: *mut c_void);
        fn ee_units_host_send_group_command(
            group: *mut c_void,
            target: u32,
            player: *mut c_void,
            command: i32,
            queue: i32,
        );
        fn ee_units_host_acknowledge_unit(player: *mut c_void, unit: u32);
        fn ee_units_host_town_patrol_point(unit: u32, found: *mut u8) -> u32;
        fn ee_units_host_patrol_add_unit_tile(unit: u32, target: u32, player: *mut c_void);
        fn ee_units_host_unit_patrol_group(unit: u32) -> *mut c_void;
        fn ee_units_host_object_unit_type(unit: u32) -> i64;
        fn ee_units_host_group_set_queued(group: *mut c_void, queued: u8);
        fn ee_units_host_group_spread_around_resource(group: *mut c_void, target: u32);
        fn ee_units_host_group_spread_around_tile(group: *mut c_void, x: i64, y: i64);
        fn ee_units_host_unit_add_build_target(unit: u32, target: u32);
        fn ee_units_host_unit_order_repair(unit: u32, target: u32);
        fn ee_units_host_group_add_member(group: *mut c_void, unit: u32);
    }

    pub struct GameHost;

    impl Host for GameHost {
        fn unit(&mut self, id: u32) -> Option<EeUnitRecord> {
            crate::store::with_store(|s| s.get(id).copied())
        }
        fn group_ids(&mut self, group: HostRef, out: &mut Vec<u32>) {
            out.clear();
            // SAFETY: the host fills at most `capacity` ids and returns the group's count
            let n = unsafe { ee_units_host_group_ids(group.0, std::ptr::null_mut(), 0) };
            out.resize(n, 0);
            if n > 0 {
                // SAFETY: as above, `out` holds n ids
                let m = unsafe { ee_units_host_group_ids(group.0, out.as_mut_ptr(), n) };
                out.truncate(m.min(n));
            }
        }
        fn goal_new(&mut self, spec: &EeGoalSpec) -> Option<GoalHandle> {
            // SAFETY: spec is a valid record for the call
            GoalHandle::from_ptr(unsafe { ee_units_host_goal_new(spec) })
        }
        fn goal_kind(&mut self, goal: &GoalHandle) -> u8 {
            // SAFETY: a goal the host created and nothing consumed
            unsafe { ee_units_host_goal_kind(goal.as_ptr()) }
        }
        fn goal_release(&mut self, goal: GoalHandle) {
            // SAFETY: as above; the handle is consumed
            unsafe { ee_units_host_goal_release(goal.as_ptr()) }
        }
        // SAFETY (each call below): the references are objects the host handed out (the sender, its selection, a
        // group from active_group_new) and live for the body's call
        fn active_group_new(&mut self, player: HostRef) -> HostRef {
            HostRef(unsafe { ee_units_host_active_group_new(player.0) })
        }
        fn group_fill(&mut self, group: HostRef, selection: HostRef, sort: i32, slot: i32) {
            unsafe { ee_units_host_group_fill(group.0, selection.0, sort, slot) }
        }
        fn group_release(&mut self, group: HostRef) {
            unsafe { ee_units_host_group_release(group.0) }
        }
        fn pending_push(&mut self, player: HostRef, group: HostRef) {
            unsafe { ee_units_host_pending_push(player.0, group.0) }
        }
        fn group_keep_mobile(&mut self, group: HostRef) {
            unsafe { ee_units_host_group_keep_mobile(group.0) }
        }
        fn group_clear_goal(&mut self, group: HostRef) {
            unsafe { ee_units_host_group_clear_goal(group.0) }
        }
        fn group_set_goal(&mut self, group: HostRef, goal: GoalHandle, queued: bool) {
            unsafe { ee_units_host_group_set_goal(group.0, goal.as_ptr(), queued as u8) }
        }
        fn group_set_action(&mut self, group: HostRef, action: i32) {
            unsafe { ee_units_host_group_set_action(group.0, action) }
        }
        fn group_plan(&mut self, group: HostRef) {
            unsafe { ee_units_host_group_plan(group.0) }
        }
        fn random(&mut self, low: i32, high: i32) -> i32 {
            unsafe { ee_units_host_random(low, high) }
        }
        fn play_acknowledgement(&mut self, player: HostRef, group: HostRef, index: i32) {
            unsafe { ee_units_host_play_acknowledgement(player.0, group.0, index) }
        }
        // SAFETY (each call below): plain values, and objects the host handed out as above; a unit id the host
        // resolves through the registry (the units a body names are alive for its call)
        fn map_height(&mut self, x: f32, y: f32) -> f32 {
            unsafe { ee_units_host_map_height(x, y) }
        }
        fn group_keep_ground_attackers(&mut self, group: HostRef) {
            unsafe { ee_units_host_group_keep_ground_attackers(group.0) }
        }
        fn send_group_command(&mut self, group: HostRef, target: u32, player: HostRef, command: i32, queue: i32) {
            unsafe { ee_units_host_send_group_command(group.0, target, player.0, command, queue) }
        }
        fn acknowledge_unit(&mut self, player: HostRef, unit: u32) {
            unsafe { ee_units_host_acknowledge_unit(player.0, unit) }
        }
        fn town_patrol_point(&mut self, unit: u32) -> Option<u32> {
            let mut found = 0u8;
            let point = unsafe { ee_units_host_town_patrol_point(unit, &mut found) };
            (found != 0).then_some(point)
        }
        fn patrol_add_unit_tile(&mut self, unit: u32, target: u32, player: HostRef) {
            unsafe { ee_units_host_patrol_add_unit_tile(unit, target, player.0) }
        }
        fn unit_patrol_group(&mut self, unit: u32) -> HostRef {
            HostRef(unsafe { ee_units_host_unit_patrol_group(unit) })
        }
        fn object_unit_type(&mut self, unit: u32) -> i64 {
            unsafe { ee_units_host_object_unit_type(unit) }
        }
        fn group_set_queued(&mut self, group: HostRef, queued: bool) {
            unsafe { ee_units_host_group_set_queued(group.0, queued as u8) }
        }
        fn group_spread_around_resource(&mut self, group: HostRef, target: u32) {
            unsafe { ee_units_host_group_spread_around_resource(group.0, target) }
        }
        fn group_spread_around_tile(&mut self, group: HostRef, x: i64, y: i64) {
            unsafe { ee_units_host_group_spread_around_tile(group.0, x, y) }
        }
        fn unit_add_build_target(&mut self, unit: u32, target: u32) {
            unsafe { ee_units_host_unit_add_build_target(unit, target) }
        }
        fn unit_order_repair(&mut self, unit: u32, target: u32) {
            unsafe { ee_units_host_unit_order_repair(unit, target) }
        }
        fn group_add_member(&mut self, group: HostRef, unit: u32) {
            unsafe { ee_units_host_group_add_member(group.0, unit) }
        }
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use std::collections::BTreeMap;

    /// A host call as the mock saw it (goals by kind)
    #[derive(Debug, PartialEq)]
    pub(crate) enum Call {
        NewGroup(HostRef, HostRef),
        Fill(HostRef, HostRef, i32, i32),
        Release(HostRef),
        Pending(HostRef, HostRef),
        KeepMobile(HostRef),
        ClearGoal(HostRef),
        GoalNew(u32, u8),
        SetGoal(HostRef, u8, bool),
        SetAction(HostRef, i32),
        Plan(HostRef),
        Random(i32, i32),
        Acknowledge(HostRef, HostRef, i32),
        MapHeight(u32, u32),
        KeepGround(HostRef),
        Send(HostRef, u32, HostRef, i32, i32),
        AcknowledgeUnit(HostRef, u32),
        TownPoint(u32),
        AddUnitTile(u32, u32, HostRef),
        Route(u32),
        ObjectType(u32),
        Queued(HostRef, bool),
        SpreadResource(HostRef, u32),
        SpreadTile(HostRef, i64, i64),
        AddBuildTarget(u32, u32),
        OrderRepair(u32, u32),
        AddMember(HostRef, u32),
        /// a goal's arguments: (ctor, unit a, unit b, the point's bits, flag a, object)
        Spec(u32, u32, u32, [u32; 3], u8, usize),
    }

    /// A host over plain values: groups as id lists (new ones at 0x1001, 0x1002, ...), goals as boxed kinds; Fill
    /// gives a group `fills[slot]`, KeepMobileUnits keeps the ids in `mobile`, the draws come from `draws` (else the
    /// low bound). Every call is logged.
    #[derive(Default)]
    pub(crate) struct MockHost {
        pub groups: BTreeMap<usize, Vec<u32>>,
        pub live: usize,
        pub calls: Vec<Call>,
        pub fills: BTreeMap<i32, Vec<u32>>,
        pub mobile: Vec<u32>,
        pub draws: Vec<i32>,
        /// the store as the mock's body reads it
        pub units: BTreeMap<u32, EeUnitRecord>,
        /// KeepGroundAttackers keeps these; the map's height; the town's patrol point; the units' routes
        pub ground: Vec<u32>,
        pub height: f32,
        pub town_point: Option<u32>,
        pub routes: BTreeMap<u32, usize>,
        pub object_types: BTreeMap<u32, i64>,
        /// log the goals' arguments too (Call::Spec)
        pub specs: bool,
        pub made: usize,
    }

    impl Host for MockHost {
        fn unit(&mut self, id: u32) -> Option<EeUnitRecord> {
            self.units.get(&id).copied()
        }
        fn group_ids(&mut self, group: HostRef, out: &mut Vec<u32>) {
            out.clear();
            out.extend(self.groups.get(&(group.0 as usize)).into_iter().flatten());
        }
        fn goal_new(&mut self, spec: &EeGoalSpec) -> Option<GoalHandle> {
            self.calls.push(Call::GoalNew(spec.ctor, spec.flag_a));
            if self.specs {
                let bits = spec.point.map(f32::to_bits);
                self.calls.push(Call::Spec(
                    spec.ctor,
                    spec.unit_a,
                    spec.unit_b,
                    bits,
                    spec.flag_a,
                    spec.object as usize,
                ));
            }
            let kind = spec.kind()?;
            self.live += 1;
            GoalHandle::from_ptr(Box::into_raw(Box::new(kind)) as *mut c_void)
        }
        fn goal_kind(&mut self, goal: &GoalHandle) -> u8 {
            // SAFETY: a goal this host boxed
            unsafe { *(goal.as_ptr() as *const u8) }
        }
        fn goal_release(&mut self, goal: GoalHandle) {
            self.live -= 1;
            // SAFETY: as above, released once
            drop(unsafe { Box::from_raw(goal.as_ptr() as *mut u8) });
        }
        fn active_group_new(&mut self, player: HostRef) -> HostRef {
            self.made += 1;
            let group = HostRef((0x1000 + self.made) as *mut c_void);
            self.groups.insert(group.0 as usize, Vec::new());
            self.calls.push(Call::NewGroup(player, group));
            group
        }
        fn group_fill(&mut self, group: HostRef, selection: HostRef, sort: i32, slot: i32) {
            let ids = self.fills.get(&slot).cloned().unwrap_or_default();
            self.groups.insert(group.0 as usize, ids);
            self.calls.push(Call::Fill(group, selection, sort, slot));
        }
        fn group_release(&mut self, group: HostRef) {
            self.groups.remove(&(group.0 as usize));
            self.calls.push(Call::Release(group));
        }
        fn pending_push(&mut self, player: HostRef, group: HostRef) {
            self.calls.push(Call::Pending(player, group));
        }
        fn group_keep_mobile(&mut self, group: HostRef) {
            let mobile = self.mobile.clone();
            if let Some(ids) = self.groups.get_mut(&(group.0 as usize)) {
                ids.retain(|id| mobile.contains(id));
            }
            self.calls.push(Call::KeepMobile(group));
        }
        fn group_clear_goal(&mut self, group: HostRef) {
            self.calls.push(Call::ClearGoal(group));
        }
        fn group_set_goal(&mut self, group: HostRef, goal: GoalHandle, queued: bool) {
            let kind = self.goal_kind(&goal);
            self.goal_release(goal);
            self.calls.push(Call::SetGoal(group, kind, queued));
        }
        fn group_set_action(&mut self, group: HostRef, action: i32) {
            self.calls.push(Call::SetAction(group, action));
        }
        fn group_plan(&mut self, group: HostRef) {
            self.calls.push(Call::Plan(group));
        }
        fn random(&mut self, low: i32, high: i32) -> i32 {
            self.calls.push(Call::Random(low, high));
            if self.draws.is_empty() {
                low
            } else {
                self.draws.remove(0)
            }
        }
        fn play_acknowledgement(&mut self, player: HostRef, group: HostRef, index: i32) {
            self.calls.push(Call::Acknowledge(player, group, index));
        }
        fn map_height(&mut self, x: f32, y: f32) -> f32 {
            self.calls.push(Call::MapHeight(x.to_bits(), y.to_bits()));
            self.height
        }
        fn group_keep_ground_attackers(&mut self, group: HostRef) {
            let ground = self.ground.clone();
            if let Some(ids) = self.groups.get_mut(&(group.0 as usize)) {
                ids.retain(|id| ground.contains(id));
            }
            self.calls.push(Call::KeepGround(group));
        }
        fn send_group_command(&mut self, group: HostRef, target: u32, player: HostRef, command: i32, queue: i32) {
            self.calls.push(Call::Send(group, target, player, command, queue));
        }
        fn acknowledge_unit(&mut self, player: HostRef, unit: u32) {
            self.calls.push(Call::AcknowledgeUnit(player, unit));
        }
        fn town_patrol_point(&mut self, unit: u32) -> Option<u32> {
            self.calls.push(Call::TownPoint(unit));
            self.town_point
        }
        fn patrol_add_unit_tile(&mut self, unit: u32, target: u32, player: HostRef) {
            self.calls.push(Call::AddUnitTile(unit, target, player));
        }
        fn unit_patrol_group(&mut self, unit: u32) -> HostRef {
            self.calls.push(Call::Route(unit));
            HostRef(self.routes.get(&unit).copied().unwrap_or(0) as *mut c_void)
        }
        fn object_unit_type(&mut self, unit: u32) -> i64 {
            self.calls.push(Call::ObjectType(unit));
            self.object_types.get(&unit).copied().unwrap_or(-1)
        }
        fn group_set_queued(&mut self, group: HostRef, queued: bool) {
            self.calls.push(Call::Queued(group, queued));
        }
        fn group_spread_around_resource(&mut self, group: HostRef, target: u32) {
            // the spread keeps the ids in `mobile` when it is set (a unit that cannot reach the resource leaves)
            if !self.mobile.is_empty() {
                let mobile = self.mobile.clone();
                if let Some(ids) = self.groups.get_mut(&(group.0 as usize)) {
                    ids.retain(|id| mobile.contains(id));
                }
            }
            self.calls.push(Call::SpreadResource(group, target));
        }
        fn group_spread_around_tile(&mut self, group: HostRef, x: i64, y: i64) {
            self.calls.push(Call::SpreadTile(group, x, y));
        }
        fn unit_add_build_target(&mut self, unit: u32, target: u32) {
            self.calls.push(Call::AddBuildTarget(unit, target));
        }
        fn unit_order_repair(&mut self, unit: u32, target: u32) {
            self.calls.push(Call::OrderRepair(unit, target));
        }
        fn group_add_member(&mut self, group: HostRef, unit: u32) {
            self.calls.push(Call::AddMember(group, unit));
        }
    }

    #[test]
    fn the_constructors_kinds() {
        let kinds: Vec<_> = (0..9).map(|c| EeGoalSpec::new(c).kind()).collect();
        assert_eq!(kinds, vec![None, Some(10), Some(3), Some(2), Some(6), Some(15), Some(9), Some(9), None]);
        let s = EeGoalSpec::new(EE_GOAL_CTOR_ATTACK_UNIT);
        assert_eq!((s.unit_a, s.unit_b, s.flag_a, s.flag_b), (EE_UNIT_ID_NONE, EE_UNIT_ID_NONE, 0, 0));
        assert!(s.object.is_null());
    }

    #[test]
    fn goals_and_groups_through_a_host() {
        let mut host = MockHost::default();
        host.groups.insert(0x10, vec![0x0100_0002, 0x0100_0001]);
        let mut ids = vec![7];
        host.group_ids(HostRef(0x10 as *mut c_void), &mut ids);
        assert_eq!(ids, vec![0x0100_0002, 0x0100_0001]);
        host.group_ids(HostRef(0x20 as *mut c_void), &mut ids);
        assert!(ids.is_empty());
        let goal = host.goal_new(&EeGoalSpec::new(EE_GOAL_CTOR_EXPLORE)).unwrap();
        assert_eq!(host.goal_kind(&goal), 10);
        assert!(host.goal_new(&EeGoalSpec::new(99)).is_none());
        host.goal_release(goal);
        assert_eq!(host.live, 0);
        assert!(GoalHandle::from_ptr(std::ptr::null_mut()).is_none());
    }
}
