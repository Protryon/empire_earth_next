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
/// `new EEUGLocation(&point, true, flag_a, flag_b, value, false, 0)`, then `mUseGroupMovement = flag_c` (EEMPathTo's:
/// forced, rotating, the facing, group movement)
pub const EE_GOAL_CTOR_LOCATION: u32 = 8;
/// `new EEUGUnloadTransport(unit_a's complex unit, (long)point[0], (long)point[1], flag_a)`
pub const EE_GOAL_CTOR_UNLOAD_TRANSPORT: u32 = 9;
/// `new EEUGLocation(point[0], point[1], 0.0f, false, true, false, 0.0f, false, 0)` (EEMUnloadTransport's move)
pub const EE_GOAL_CTOR_LOCATION_XY: u32 = 10;
/// `new EEUGLocation(GE2DPoint(point[0], point[1]), true, true, false, 0.0f, false, 0)` (EEMSmartFarmers' move)
pub const EE_GOAL_CTOR_LOCATION_2D: u32 = 11;
/// `new EEUGCastCalamity(unit_a, unit_b, ints[0], ints[1], ints[2], flag_a)` (the caster's complex unit, the target
/// or none, the tile x and y, the technology)
pub const EE_GOAL_CTOR_CAST_CALAMITY: u32 = 12;

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
    /// a value (the location's facing)
    pub value: f32,
    pub flag_c: u8,
    /// integer arguments (the cast's tile and technology)
    pub ints: [i64; 3],
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
            value: 0.0,
            flag_c: 0,
            ints: [0; 3],
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
            EE_GOAL_CTOR_LOCATION | EE_GOAL_CTOR_LOCATION_XY | EE_GOAL_CTOR_LOCATION_2D => 1,
            EE_GOAL_CTOR_UNLOAD_TRANSPORT => 8,
            EE_GOAL_CTOR_CAST_CALAMITY => 11,
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
    /// `EEActiveGroup::KeepNonCitizens` (its test is `group::keeps_non_citizen` over the store)
    fn group_keep_non_citizens(&mut self, group: HostRef);
    /// `new EEPPatrolGroup`, `AddPoint(GE2DPoint(x, y), player->mPlayerID)`, `player->AddPatrolGroup(it)`, the
    /// body's reference released
    fn patrol_new_lone(&mut self, player: HostRef, x: f32, y: f32);
    /// `unit->mPatrolGroup->InsertPatrolPointAt(GE2DPoint(x, y), player->mPlayerID, unit)`
    fn patrol_insert_point_at(&mut self, unit: u32, x: f32, y: f32, player: HostRef);
    /// the route of the unit's current goal when it is a patrol (`EEGoalCast<EEUGPatrol>(unit->GetGoal())`), else null
    fn unit_goal_patrol_route(&mut self, unit: u32) -> HostRef;
    /// `route->AddPoint(GE2DPoint(x, y), player->mPlayerID)`
    fn patrol_add_point(&mut self, route: HostRef, x: f32, y: f32, player: HostRef);
    /// a new patrol goal's route: `goal->mPatrolGroup->AddPoint(GE2DPoint(x, y), player->mPlayerID)`, then
    /// `player->AddPatrolGroup(goal->mPatrolGroup)`
    fn goal_patrol_add_point(&mut self, goal: &GoalHandle, x: f32, y: f32, player: HostRef);
    /// `EEActiveGroup::SpreadAroundUnit(target, x, y)`
    fn group_spread_around_unit(&mut self, group: HostRef, target: u32, x: i64, y: i64);
    /// `unit->GetComplexUnit()` as an id (EE_UNIT_ID_NONE: none), read on the object
    fn unit_complex_id(&mut self, unit: u32) -> u32;
    /// EEMPathTo's point before its loop (the map's closest tile, a building the unit cannot pass): (x, y) in, the
    /// point the body uses and its height out
    fn path_to_point(&mut self, first: u32, x: f32, y: f32) -> (f32, f32, f32);
    /// `EEActiveGroup::ResetUnitsPaths`
    fn group_reset_paths(&mut self, group: HostRef);
    /// `EEActiveGroup::KeepCitizens` (its test is `group::keeps_citizen` over the store)
    fn group_keep_citizens(&mut self, group: HostRef);
    /// `EEPlanWall(start, end, gUnitClassTable[the technology's unit class], player, true, false, pieces)`: the new
    /// pieces (a list the host owns until `wall_release`) and their count
    fn wall_plan(&mut self, player: HostRef, technology: i32, start: (f32, f32), end: (f32, f32)) -> (HostRef, usize);
    /// `EEBuildWall(group, pieces)`
    fn wall_build(&mut self, group: HostRef, pieces: HostRef);
    /// the pieces' list goes
    fn wall_release(&mut self, pieces: HostRef);
    /// `EEActiveGroup::SpreadForUnload(x, y)`
    fn group_spread_for_unload(&mut self, group: HostRef, x: i64, y: i64);
    /// the unit carries cargo it can unload: `(mUnitClass->IsTransport() || mUnitClass->mIsParatrooperBomber) &&
    /// mComplexUnit->GetNumCargoUnits() > 0`, read on the object
    fn unit_can_unload(&mut self, unit: u32) -> bool;
    /// `goal->SetTransport(unit->mComplexUnit)` on an unload goal not yet given
    fn goal_set_transport(&mut self, goal: &GoalHandle, unit: u32);
    /// `goal->CanUnloadAtDestination()` on an unload goal not yet given
    fn goal_can_unload(&mut self, goal: &GoalHandle) -> bool;
    /// `player->mEvents.AddEvent(U2DPoint<long>(x, y), 2, player, 0x7c3b, false)`: "cannot unload there"
    fn unload_warning(&mut self, player: HostRef, x: i64, y: i64);
    /// `player->mTechnologies->Get(technology)->mData->mUnitClass` (None: the player has no such node)
    fn technology_unit_class(&mut self, player: HostRef, technology: i32) -> Option<i64>;
    /// `player->mNumCapitols`
    fn player_num_capitols(&mut self, player: HostRef) -> i64;
    /// `player->CanProduce(technology, 0x20, 1, true, 3, NULL)`
    fn can_produce(&mut self, player: HostRef, technology: i32) -> bool;
    /// `EECanPlaceOnTile(player, unit_type, x, y, false, 1)`
    fn can_place_on_tile(&mut self, player: HostRef, unit_type: i64, x: i64, y: i64) -> bool;
    /// `EEMakeBuilding(player, unit_type, x, y)` as an id (EE_UNIT_ID_NONE: none)
    fn make_building(&mut self, player: HostRef, unit_type: i64, x: i64, y: i64) -> u32;
    /// the farms `EEFindFreeFarm(unit, true, &ring)` finds from ring 1 on until it finds none, in order (unit: a
    /// complex unit's id; EE_UNIT_ID_NONE: none)
    fn free_farms(&mut self, unit: u32, out: &mut Vec<u32>);
    /// `std::sort` of the farms (complex units' ids) around the tile (x, y): `EETileDistanceLess` (built first, then
    /// paid for, then the nearer; EEMBuildUnit) or, `greater`, `EEFarmDistanceGreater` (the reverse; EEMSmartFarmers)
    fn sort_farms(&mut self, farms: &mut [u32], x: i64, y: i64, greater: bool);
    /// `unit->AddBuildTarget(target)` (a granary's farm: the shift flag is not touched)
    fn unit_add_farm_target(&mut self, unit: u32, target: u32);
    /// `unit->OrderRepair(target)`
    fn unit_order_build(&mut self, unit: u32, target: u32);
    /// `unit->StartAction(action)`, then `unit->mUnitClass->ActionChanged(unit)`
    fn unit_start_action(&mut self, unit: u32, action: i32);
    /// the farm of the unit's load goal: `EEGoalCast<EEUGGather>(unit->GetLoadGoal())`'s resource when it is a farm,
    /// its complex unit as an id (EE_UNIT_ID_NONE: no such goal, no resource or not a farm)
    fn unit_load_farm(&mut self, unit: u32) -> u32;
    /// `EEGetFarmDropSites(farm, sites)`: the player's granaries at the farm's corners and sides, as ids
    fn farm_drop_sites(&mut self, farm: u32, out: &mut Vec<u32>) -> bool;
    /// `target->CanTake(unit, unit->mResourceType)`
    fn can_take(&mut self, target: u32, unit: u32) -> bool;
    /// `unit->SetGatheredResource(NULL)`
    fn unit_drop_gathered(&mut self, unit: u32);
    /// `EEWorkFarm(farm, unit)`
    fn work_farm(&mut self, farm: u32, unit: u32);
    /// `unit->SetGoal(goal, false, 0)`: the unit takes the goal
    fn unit_set_goal(&mut self, unit: u32, goal: GoalHandle);
    /// `EEActiveGroup::Finish`
    fn group_finish(&mut self, group: HostRef);
    /// the calamity class of the technology's node (`EECalamityManager::GetCalamityClass(node->mData->mCalamity)`):
    /// the class, its `mClassEnum`, its `mIsCastByAllSelected` (None: no node, or no class)
    fn calamity_of(&mut self, player: HostRef, technology: i32) -> Option<(HostRef, i32, bool)>;
    /// `class->CanCastAt(unit, location, x, y, true, target (EE_UNIT_ID_NONE: NULL))`: 0 when it can
    fn calamity_can_cast_at(&mut self, class: HostRef, unit: u32, x: i32, y: i32, target: u32) -> i64;
    /// `player->mTechnologies->Get(technology)->Contains(unit_type)`
    fn technology_contains(&mut self, player: HostRef, technology: i32, unit_type: i64) -> bool;
    /// `unit->mPower`, read on the object
    fn unit_power(&mut self, unit: u32) -> i64;
    /// `class->GetCost()`
    fn calamity_cost(&mut self, class: HostRef) -> i64;
    /// `unit->mPlayer` (the player object, as the bodies' player references are)
    fn unit_player(&mut self, unit: u32) -> HostRef;
    /// `EEActiveGroup::SplitCalamityCasters(class, player's node of the technology, target)`
    fn group_split_calamity_casters(
        &mut self,
        group: HostRef,
        class: HostRef,
        player: HostRef,
        technology: i32,
        target: u32,
    );
    /// `goal->Clone()`
    fn goal_clone(&mut self, goal: &GoalHandle) -> Option<GoalHandle>;
    /// `EEActiveGroup::RemoveUnit(unit)`
    fn group_remove_unit(&mut self, group: HostRef, unit: u32);
    /// `unit->ClearGoals(0)`
    fn unit_clear_goals(&mut self, unit: u32);
    /// `unit->mHitPoints` read on the object, for a unit the store does not hold
    fn object_hit_points(&mut self, unit: u32) -> i64;
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
        fn ee_units_host_group_keep_non_citizens(group: *mut c_void);
        fn ee_units_host_patrol_new_lone(player: *mut c_void, x: f32, y: f32);
        fn ee_units_host_patrol_insert_point_at(unit: u32, x: f32, y: f32, player: *mut c_void);
        fn ee_units_host_unit_goal_patrol_route(unit: u32) -> *mut c_void;
        fn ee_units_host_patrol_add_point(route: *mut c_void, x: f32, y: f32, player: *mut c_void);
        fn ee_units_host_goal_patrol_add_point(goal: *mut c_void, x: f32, y: f32, player: *mut c_void);
        fn ee_units_host_group_spread_around_unit(group: *mut c_void, target: u32, x: i64, y: i64);
        fn ee_units_host_unit_complex_id(unit: u32) -> u32;
        fn ee_units_host_path_to_point(first: u32, x: *mut f32, y: *mut f32) -> f32;
        fn ee_units_host_group_reset_paths(group: *mut c_void);
        fn ee_units_host_group_keep_citizens(group: *mut c_void);
        fn ee_units_host_wall_plan(
            player: *mut c_void,
            technology: i32,
            start_x: f32,
            start_y: f32,
            end_x: f32,
            end_y: f32,
            count: *mut usize,
        ) -> *mut c_void;
        fn ee_units_host_wall_build(group: *mut c_void, pieces: *mut c_void);
        fn ee_units_host_wall_release(pieces: *mut c_void);
        fn ee_units_host_group_spread_for_unload(group: *mut c_void, x: i64, y: i64);
        fn ee_units_host_unit_can_unload(unit: u32) -> u8;
        fn ee_units_host_goal_set_transport(goal: *mut c_void, unit: u32);
        fn ee_units_host_goal_can_unload(goal: *mut c_void) -> u8;
        fn ee_units_host_unload_warning(player: *mut c_void, x: i64, y: i64);
        fn ee_units_host_technology_unit_class(player: *mut c_void, technology: i32, found: *mut u8) -> i64;
        fn ee_units_host_player_num_capitols(player: *mut c_void) -> i64;
        fn ee_units_host_can_produce(player: *mut c_void, technology: i32) -> u8;
        fn ee_units_host_can_place_on_tile(player: *mut c_void, unit_type: i64, x: i64, y: i64) -> u8;
        fn ee_units_host_make_building(player: *mut c_void, unit_type: i64, x: i64, y: i64) -> u32;
        fn ee_units_host_free_farms(unit: u32, out: *mut u32, capacity: usize) -> usize;
        fn ee_units_host_sort_farms(farms: *mut u32, count: usize, x: i64, y: i64, greater: u8);
        fn ee_units_host_unit_add_farm_target(unit: u32, target: u32);
        fn ee_units_host_unit_order_build(unit: u32, target: u32);
        fn ee_units_host_unit_start_action(unit: u32, action: i32);
        fn ee_units_host_unit_load_farm(unit: u32) -> u32;
        fn ee_units_host_farm_drop_sites(farm: u32, out: *mut u32, capacity: usize, found: *mut u8) -> usize;
        fn ee_units_host_can_take(target: u32, unit: u32) -> u8;
        fn ee_units_host_unit_drop_gathered(unit: u32);
        fn ee_units_host_work_farm(farm: u32, unit: u32);
        fn ee_units_host_unit_set_goal(unit: u32, goal: *mut c_void);
        fn ee_units_host_group_finish(group: *mut c_void);
        fn ee_units_host_calamity_of(
            player: *mut c_void,
            technology: i32,
            class_enum: *mut i32,
            cast_by_all: *mut u8,
        ) -> *mut c_void;
        fn ee_units_host_calamity_can_cast_at(class: *mut c_void, unit: u32, x: i32, y: i32, target: u32) -> i64;
        fn ee_units_host_technology_contains(player: *mut c_void, technology: i32, unit_type: i64) -> u8;
        fn ee_units_host_unit_power(unit: u32) -> i64;
        fn ee_units_host_calamity_cost(class: *mut c_void) -> i64;
        fn ee_units_host_unit_player(unit: u32) -> *mut c_void;
        fn ee_units_host_group_split_calamity_casters(
            group: *mut c_void,
            class: *mut c_void,
            player: *mut c_void,
            technology: i32,
            target: u32,
        );
        fn ee_units_host_goal_clone(goal: *mut c_void) -> *mut c_void;
        fn ee_units_host_group_remove_unit(group: *mut c_void, unit: u32);
        fn ee_units_host_unit_clear_goals(unit: u32);
        fn ee_units_host_object_hit_points(unit: u32) -> i64;
    }

    /// A list the host fills: one call (`capacity` 0) for the count, a second for the ids. The host answers the
    /// second call from the list the first computed (it keeps the last one), so the search runs once.
    fn read_ids(out: &mut Vec<u32>, mut read: impl FnMut(*mut u32, usize) -> usize) {
        out.clear();
        let n = read(std::ptr::null_mut(), 0);
        out.resize(n, 0);
        if n > 0 {
            let m = read(out.as_mut_ptr(), n);
            out.truncate(m.min(n));
        }
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
        fn group_keep_non_citizens(&mut self, group: HostRef) {
            unsafe { ee_units_host_group_keep_non_citizens(group.0) }
        }
        fn patrol_new_lone(&mut self, player: HostRef, x: f32, y: f32) {
            unsafe { ee_units_host_patrol_new_lone(player.0, x, y) }
        }
        fn patrol_insert_point_at(&mut self, unit: u32, x: f32, y: f32, player: HostRef) {
            unsafe { ee_units_host_patrol_insert_point_at(unit, x, y, player.0) }
        }
        fn unit_goal_patrol_route(&mut self, unit: u32) -> HostRef {
            HostRef(unsafe { ee_units_host_unit_goal_patrol_route(unit) })
        }
        fn patrol_add_point(&mut self, route: HostRef, x: f32, y: f32, player: HostRef) {
            unsafe { ee_units_host_patrol_add_point(route.0, x, y, player.0) }
        }
        fn goal_patrol_add_point(&mut self, goal: &GoalHandle, x: f32, y: f32, player: HostRef) {
            // SAFETY: a goal the host created and nothing consumed yet
            unsafe { ee_units_host_goal_patrol_add_point(goal.as_ptr(), x, y, player.0) }
        }
        fn group_spread_around_unit(&mut self, group: HostRef, target: u32, x: i64, y: i64) {
            unsafe { ee_units_host_group_spread_around_unit(group.0, target, x, y) }
        }
        fn unit_complex_id(&mut self, unit: u32) -> u32 {
            unsafe { ee_units_host_unit_complex_id(unit) }
        }
        fn path_to_point(&mut self, first: u32, x: f32, y: f32) -> (f32, f32, f32) {
            let (mut x, mut y) = (x, y);
            // SAFETY: two floats the host reads and writes for the call
            let z = unsafe { ee_units_host_path_to_point(first, &mut x, &mut y) };
            (x, y, z)
        }
        fn group_reset_paths(&mut self, group: HostRef) {
            unsafe { ee_units_host_group_reset_paths(group.0) }
        }
        fn group_keep_citizens(&mut self, group: HostRef) {
            unsafe { ee_units_host_group_keep_citizens(group.0) }
        }
        fn wall_plan(
            &mut self,
            player: HostRef,
            technology: i32,
            start: (f32, f32),
            end: (f32, f32),
        ) -> (HostRef, usize) {
            let mut count = 0usize;
            // SAFETY: the count is written for the call
            let pieces =
                unsafe { ee_units_host_wall_plan(player.0, technology, start.0, start.1, end.0, end.1, &mut count) };
            (HostRef(pieces), count)
        }
        fn wall_build(&mut self, group: HostRef, pieces: HostRef) {
            unsafe { ee_units_host_wall_build(group.0, pieces.0) }
        }
        fn wall_release(&mut self, pieces: HostRef) {
            unsafe { ee_units_host_wall_release(pieces.0) }
        }
        fn group_spread_for_unload(&mut self, group: HostRef, x: i64, y: i64) {
            unsafe { ee_units_host_group_spread_for_unload(group.0, x, y) }
        }
        fn unit_can_unload(&mut self, unit: u32) -> bool {
            unsafe { ee_units_host_unit_can_unload(unit) != 0 }
        }
        fn goal_set_transport(&mut self, goal: &GoalHandle, unit: u32) {
            // SAFETY: a goal the host created and nothing consumed yet
            unsafe { ee_units_host_goal_set_transport(goal.as_ptr(), unit) }
        }
        fn goal_can_unload(&mut self, goal: &GoalHandle) -> bool {
            // SAFETY: as above
            unsafe { ee_units_host_goal_can_unload(goal.as_ptr()) != 0 }
        }
        fn unload_warning(&mut self, player: HostRef, x: i64, y: i64) {
            unsafe { ee_units_host_unload_warning(player.0, x, y) }
        }
        fn technology_unit_class(&mut self, player: HostRef, technology: i32) -> Option<i64> {
            let mut found = 0u8;
            // SAFETY: the flag is written for the call
            let class = unsafe { ee_units_host_technology_unit_class(player.0, technology, &mut found) };
            (found != 0).then_some(class)
        }
        fn player_num_capitols(&mut self, player: HostRef) -> i64 {
            unsafe { ee_units_host_player_num_capitols(player.0) }
        }
        fn can_produce(&mut self, player: HostRef, technology: i32) -> bool {
            unsafe { ee_units_host_can_produce(player.0, technology) != 0 }
        }
        fn can_place_on_tile(&mut self, player: HostRef, unit_type: i64, x: i64, y: i64) -> bool {
            unsafe { ee_units_host_can_place_on_tile(player.0, unit_type, x, y) != 0 }
        }
        fn make_building(&mut self, player: HostRef, unit_type: i64, x: i64, y: i64) -> u32 {
            unsafe { ee_units_host_make_building(player.0, unit_type, x, y) }
        }
        fn free_farms(&mut self, unit: u32, out: &mut Vec<u32>) {
            // SAFETY: the host fills at most `capacity` ids
            read_ids(out, |ids, capacity| unsafe { ee_units_host_free_farms(unit, ids, capacity) });
        }
        fn sort_farms(&mut self, farms: &mut [u32], x: i64, y: i64, greater: bool) {
            // SAFETY: the host permutes the `count` ids in place
            unsafe { ee_units_host_sort_farms(farms.as_mut_ptr(), farms.len(), x, y, greater as u8) }
        }
        fn unit_add_farm_target(&mut self, unit: u32, target: u32) {
            unsafe { ee_units_host_unit_add_farm_target(unit, target) }
        }
        fn unit_order_build(&mut self, unit: u32, target: u32) {
            unsafe { ee_units_host_unit_order_build(unit, target) }
        }
        fn unit_start_action(&mut self, unit: u32, action: i32) {
            unsafe { ee_units_host_unit_start_action(unit, action) }
        }
        fn unit_load_farm(&mut self, unit: u32) -> u32 {
            unsafe { ee_units_host_unit_load_farm(unit) }
        }
        fn farm_drop_sites(&mut self, farm: u32, out: &mut Vec<u32>) -> bool {
            let mut found = 0u8;
            // SAFETY: as free_farms; the flag is written for the call
            read_ids(out, |ids, capacity| unsafe { ee_units_host_farm_drop_sites(farm, ids, capacity, &mut found) });
            found != 0
        }
        fn can_take(&mut self, target: u32, unit: u32) -> bool {
            unsafe { ee_units_host_can_take(target, unit) != 0 }
        }
        fn unit_drop_gathered(&mut self, unit: u32) {
            unsafe { ee_units_host_unit_drop_gathered(unit) }
        }
        fn work_farm(&mut self, farm: u32, unit: u32) {
            unsafe { ee_units_host_work_farm(farm, unit) }
        }
        fn unit_set_goal(&mut self, unit: u32, goal: GoalHandle) {
            // SAFETY: a goal the host created; the unit takes it
            unsafe { ee_units_host_unit_set_goal(unit, goal.as_ptr()) }
        }
        fn group_finish(&mut self, group: HostRef) {
            unsafe { ee_units_host_group_finish(group.0) }
        }
        fn calamity_of(&mut self, player: HostRef, technology: i32) -> Option<(HostRef, i32, bool)> {
            let (mut class_enum, mut all) = (0i32, 0u8);
            // SAFETY: the two values are written for the call
            let class = unsafe { ee_units_host_calamity_of(player.0, technology, &mut class_enum, &mut all) };
            (!class.is_null()).then_some((HostRef(class), class_enum, all != 0))
        }
        fn calamity_can_cast_at(&mut self, class: HostRef, unit: u32, x: i32, y: i32, target: u32) -> i64 {
            unsafe { ee_units_host_calamity_can_cast_at(class.0, unit, x, y, target) }
        }
        fn technology_contains(&mut self, player: HostRef, technology: i32, unit_type: i64) -> bool {
            unsafe { ee_units_host_technology_contains(player.0, technology, unit_type) != 0 }
        }
        fn unit_power(&mut self, unit: u32) -> i64 {
            unsafe { ee_units_host_unit_power(unit) }
        }
        fn calamity_cost(&mut self, class: HostRef) -> i64 {
            unsafe { ee_units_host_calamity_cost(class.0) }
        }
        fn unit_player(&mut self, unit: u32) -> HostRef {
            HostRef(unsafe { ee_units_host_unit_player(unit) })
        }
        fn group_split_calamity_casters(
            &mut self,
            group: HostRef,
            class: HostRef,
            player: HostRef,
            technology: i32,
            target: u32,
        ) {
            unsafe { ee_units_host_group_split_calamity_casters(group.0, class.0, player.0, technology, target) }
        }
        fn goal_clone(&mut self, goal: &GoalHandle) -> Option<GoalHandle> {
            // SAFETY: a goal the host created and nothing consumed
            GoalHandle::from_ptr(unsafe { ee_units_host_goal_clone(goal.as_ptr()) })
        }
        fn group_remove_unit(&mut self, group: HostRef, unit: u32) {
            unsafe { ee_units_host_group_remove_unit(group.0, unit) }
        }
        fn unit_clear_goals(&mut self, unit: u32) {
            unsafe { ee_units_host_unit_clear_goals(unit) }
        }
        fn object_hit_points(&mut self, unit: u32) -> i64 {
            unsafe { ee_units_host_object_hit_points(unit) }
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
        KeepNonCitizens(HostRef),
        LonePatrol(HostRef, u32, u32),
        InsertPoint(u32, u32, u32, HostRef),
        GoalRoute(u32),
        AddPoint(HostRef, u32, u32, HostRef),
        GoalAddPoint(u8, u32, u32, HostRef),
        SpreadUnit(HostRef, u32, i64, i64),
        ComplexOf(u32),
        PathPoint(u32, u32, u32),
        ResetPaths(HostRef),
        KeepCitizens(HostRef),
        /// player, technology, the points' bits
        WallPlan(HostRef, i32, [u32; 4]),
        WallBuild(HostRef, HostRef),
        WallRelease(HostRef),
        SpreadUnload(HostRef, i64, i64),
        CanUnload(u32),
        SetTransport(u8, u32),
        CanUnloadThere(u8),
        UnloadWarning(HostRef, i64, i64),
        /// a location goal's facing and group movement flag
        Location(u32, u8, u8),
        TechClass(i32),
        Capitols,
        CanProduce(i32),
        CanPlace(i64, i64, i64),
        MakeBuilding(i64, i64, i64),
        FreeFarms(u32),
        SortFarms(Vec<u32>, i64, i64, bool),
        AddFarm(u32, u32),
        OrderBuild(u32, u32),
        StartAction(u32, i32),
        LoadFarm(u32),
        DropSites(u32),
        CanTake(u32, u32),
        DropGathered(u32),
        WorkFarm(u32, u32),
        UnitSetGoal(u32, u8),
        Finish(HostRef),
        CalamityOf(i32),
        CanCast(u32, i32, i32, u32),
        Contains(i64),
        Power(u32),
        Cost,
        UnitPlayer(u32),
        Split(HostRef, i32, u32),
        Clone(u8),
        RemoveUnit(HostRef, u32),
        ClearGoals(u32),
        ObjectHitPoints(u32),
        /// a cast goal's integers
        Ints([i64; 3]),
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
        /// KeepNonCitizens keeps these; the units' patrol goals' routes
        pub non_citizens: Vec<u32>,
        pub goal_routes: BTreeMap<u32, usize>,
        /// the units' complex units (absent: the unit itself)
        pub complex_of: BTreeMap<u32, u32>,
        /// Path To's point as the host adjusts it (x, y, z; None: unchanged, height 0)
        pub path_point: Option<(f32, f32, f32)>,
        /// KeepCitizens keeps these; the wall's pieces (EEPlanWall's count)
        pub citizens: Vec<u32>,
        pub wall_pieces: usize,
        /// the units that carry cargo; whether an unload goal can unload at its destination
        pub cargo: Vec<u32>,
        pub unload_there: bool,
        /// log the goals' arguments too (Call::Spec)
        pub specs: bool,
        pub made: usize,
        /// the technologies' unit classes; the capitols; CanProduce's and EECanPlaceOnTile's answers; the new building
        pub tech_classes: BTreeMap<i32, i64>,
        pub capitols: i64,
        pub produce: bool,
        pub place: bool,
        pub building: u32,
        /// the free farms; the units that leave their group when given a build target or a build order
        pub free: Vec<u32>,
        pub leave_on_order: Vec<u32>,
        /// the units' load goals' farms; the farms' drop sites (absent: none); the units the target can take from
        pub load_farms: BTreeMap<u32, u32>,
        pub drop_sites: BTreeMap<u32, Vec<u32>>,
        pub takes: Vec<u32>,
        /// the calamity (class, enum, cast by all); CanCastAt's answers by unit (absent: 1); the unit types the node
        /// contains; the units' power; the class's cost; hit points read on the objects
        pub calamity: Option<(usize, i32, bool)>,
        pub cast_at: BTreeMap<u32, i64>,
        pub contains: Vec<i64>,
        pub power: BTreeMap<u32, i64>,
        pub cost: i64,
        pub object_hp: BTreeMap<u32, i64>,
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
            if self.specs && spec.ctor == EE_GOAL_CTOR_LOCATION {
                self.calls.push(Call::Location(spec.value.to_bits(), spec.flag_b, spec.flag_c));
            }
            if self.specs && spec.ctor == EE_GOAL_CTOR_CAST_CALAMITY {
                self.calls.push(Call::Ints(spec.ints));
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
            self.leave(unit);
        }
        fn unit_order_repair(&mut self, unit: u32, target: u32) {
            self.calls.push(Call::OrderRepair(unit, target));
        }
        fn group_add_member(&mut self, group: HostRef, unit: u32) {
            self.calls.push(Call::AddMember(group, unit));
        }
        fn group_keep_non_citizens(&mut self, group: HostRef) {
            let keep = self.non_citizens.clone();
            if let Some(ids) = self.groups.get_mut(&(group.0 as usize)) {
                ids.retain(|id| keep.contains(id));
            }
            self.calls.push(Call::KeepNonCitizens(group));
        }
        fn patrol_new_lone(&mut self, player: HostRef, x: f32, y: f32) {
            self.calls.push(Call::LonePatrol(player, x.to_bits(), y.to_bits()));
        }
        fn patrol_insert_point_at(&mut self, unit: u32, x: f32, y: f32, player: HostRef) {
            self.calls.push(Call::InsertPoint(unit, x.to_bits(), y.to_bits(), player));
        }
        fn unit_goal_patrol_route(&mut self, unit: u32) -> HostRef {
            self.calls.push(Call::GoalRoute(unit));
            HostRef(self.goal_routes.get(&unit).copied().unwrap_or(0) as *mut c_void)
        }
        fn patrol_add_point(&mut self, route: HostRef, x: f32, y: f32, player: HostRef) {
            self.calls.push(Call::AddPoint(route, x.to_bits(), y.to_bits(), player));
        }
        fn goal_patrol_add_point(&mut self, goal: &GoalHandle, x: f32, y: f32, player: HostRef) {
            let kind = self.goal_kind(goal);
            self.calls.push(Call::GoalAddPoint(kind, x.to_bits(), y.to_bits(), player));
        }
        fn group_spread_around_unit(&mut self, group: HostRef, target: u32, x: i64, y: i64) {
            // the spread keeps the ids in `mobile` when it is set
            if !self.mobile.is_empty() {
                let mobile = self.mobile.clone();
                if let Some(ids) = self.groups.get_mut(&(group.0 as usize)) {
                    ids.retain(|id| mobile.contains(id));
                }
            }
            self.calls.push(Call::SpreadUnit(group, target, x, y));
        }
        fn unit_complex_id(&mut self, unit: u32) -> u32 {
            self.calls.push(Call::ComplexOf(unit));
            self.complex_of.get(&unit).copied().unwrap_or(unit)
        }
        fn path_to_point(&mut self, first: u32, x: f32, y: f32) -> (f32, f32, f32) {
            self.calls.push(Call::PathPoint(first, x.to_bits(), y.to_bits()));
            self.path_point.unwrap_or((x, y, 0.0))
        }
        fn group_reset_paths(&mut self, group: HostRef) {
            self.calls.push(Call::ResetPaths(group));
        }
        fn group_keep_citizens(&mut self, group: HostRef) {
            let keep = self.citizens.clone();
            if let Some(ids) = self.groups.get_mut(&(group.0 as usize)) {
                ids.retain(|id| keep.contains(id));
            }
            self.calls.push(Call::KeepCitizens(group));
        }
        fn wall_plan(
            &mut self,
            player: HostRef,
            technology: i32,
            start: (f32, f32),
            end: (f32, f32),
        ) -> (HostRef, usize) {
            let bits = [start.0.to_bits(), start.1.to_bits(), end.0.to_bits(), end.1.to_bits()];
            self.calls.push(Call::WallPlan(player, technology, bits));
            (HostRef(0x7700 as *mut c_void), self.wall_pieces)
        }
        fn wall_build(&mut self, group: HostRef, pieces: HostRef) {
            self.calls.push(Call::WallBuild(group, pieces));
        }
        fn wall_release(&mut self, pieces: HostRef) {
            self.calls.push(Call::WallRelease(pieces));
        }
        fn group_spread_for_unload(&mut self, group: HostRef, x: i64, y: i64) {
            self.calls.push(Call::SpreadUnload(group, x, y));
        }
        fn unit_can_unload(&mut self, unit: u32) -> bool {
            self.calls.push(Call::CanUnload(unit));
            self.cargo.contains(&unit)
        }
        fn goal_set_transport(&mut self, goal: &GoalHandle, unit: u32) {
            let kind = self.goal_kind(goal);
            self.calls.push(Call::SetTransport(kind, unit));
        }
        fn goal_can_unload(&mut self, goal: &GoalHandle) -> bool {
            let kind = self.goal_kind(goal);
            self.calls.push(Call::CanUnloadThere(kind));
            self.unload_there
        }
        fn unload_warning(&mut self, player: HostRef, x: i64, y: i64) {
            self.calls.push(Call::UnloadWarning(player, x, y));
        }
        fn technology_unit_class(&mut self, _player: HostRef, technology: i32) -> Option<i64> {
            self.calls.push(Call::TechClass(technology));
            self.tech_classes.get(&technology).copied()
        }
        fn player_num_capitols(&mut self, _player: HostRef) -> i64 {
            self.calls.push(Call::Capitols);
            self.capitols
        }
        fn can_produce(&mut self, _player: HostRef, technology: i32) -> bool {
            self.calls.push(Call::CanProduce(technology));
            self.produce
        }
        fn can_place_on_tile(&mut self, _player: HostRef, unit_type: i64, x: i64, y: i64) -> bool {
            self.calls.push(Call::CanPlace(unit_type, x, y));
            self.place
        }
        fn make_building(&mut self, _player: HostRef, unit_type: i64, x: i64, y: i64) -> u32 {
            self.calls.push(Call::MakeBuilding(unit_type, x, y));
            self.building
        }
        fn free_farms(&mut self, unit: u32, out: &mut Vec<u32>) {
            self.calls.push(Call::FreeFarms(unit));
            out.clear();
            out.extend(&self.free);
        }
        fn sort_farms(&mut self, farms: &mut [u32], x: i64, y: i64, greater: bool) {
            self.calls.push(Call::SortFarms(farms.to_vec(), x, y, greater));
            if greater {
                farms.sort_by(|a, b| b.cmp(a));
            } else {
                farms.sort();
            }
        }
        fn unit_add_farm_target(&mut self, unit: u32, target: u32) {
            self.calls.push(Call::AddFarm(unit, target));
        }
        fn unit_order_build(&mut self, unit: u32, target: u32) {
            self.calls.push(Call::OrderBuild(unit, target));
            self.leave(unit);
        }
        fn unit_start_action(&mut self, unit: u32, action: i32) {
            self.calls.push(Call::StartAction(unit, action));
        }
        fn unit_load_farm(&mut self, unit: u32) -> u32 {
            self.calls.push(Call::LoadFarm(unit));
            self.load_farms.get(&unit).copied().unwrap_or(EE_UNIT_ID_NONE)
        }
        fn farm_drop_sites(&mut self, farm: u32, out: &mut Vec<u32>) -> bool {
            self.calls.push(Call::DropSites(farm));
            out.clear();
            out.extend(self.drop_sites.get(&farm).into_iter().flatten());
            !out.is_empty()
        }
        fn can_take(&mut self, target: u32, unit: u32) -> bool {
            self.calls.push(Call::CanTake(target, unit));
            self.takes.contains(&unit)
        }
        fn unit_drop_gathered(&mut self, unit: u32) {
            self.calls.push(Call::DropGathered(unit));
        }
        fn work_farm(&mut self, farm: u32, unit: u32) {
            self.calls.push(Call::WorkFarm(farm, unit));
        }
        fn unit_set_goal(&mut self, unit: u32, goal: GoalHandle) {
            let kind = self.goal_kind(&goal);
            self.goal_release(goal);
            self.calls.push(Call::UnitSetGoal(unit, kind));
        }
        fn group_finish(&mut self, group: HostRef) {
            self.calls.push(Call::Finish(group));
        }
        fn calamity_of(&mut self, _player: HostRef, technology: i32) -> Option<(HostRef, i32, bool)> {
            self.calls.push(Call::CalamityOf(technology));
            self.calamity.map(|(class, class_enum, all)| (HostRef(class as *mut c_void), class_enum, all))
        }
        fn calamity_can_cast_at(&mut self, _class: HostRef, unit: u32, x: i32, y: i32, target: u32) -> i64 {
            self.calls.push(Call::CanCast(unit, x, y, target));
            self.cast_at.get(&unit).copied().unwrap_or(1)
        }
        fn technology_contains(&mut self, _player: HostRef, _technology: i32, unit_type: i64) -> bool {
            self.calls.push(Call::Contains(unit_type));
            self.contains.contains(&unit_type)
        }
        fn unit_power(&mut self, unit: u32) -> i64 {
            self.calls.push(Call::Power(unit));
            self.power.get(&unit).copied().unwrap_or(0)
        }
        fn calamity_cost(&mut self, _class: HostRef) -> i64 {
            self.calls.push(Call::Cost);
            self.cost
        }
        fn unit_player(&mut self, unit: u32) -> HostRef {
            self.calls.push(Call::UnitPlayer(unit));
            HostRef(0x9000 as *mut c_void)
        }
        fn group_split_calamity_casters(
            &mut self,
            group: HostRef,
            _class: HostRef,
            _player: HostRef,
            technology: i32,
            target: u32,
        ) {
            // the split keeps the ids in `mobile` when it is set
            if !self.mobile.is_empty() {
                let mobile = self.mobile.clone();
                if let Some(ids) = self.groups.get_mut(&(group.0 as usize)) {
                    ids.retain(|id| mobile.contains(id));
                }
            }
            self.calls.push(Call::Split(group, technology, target));
        }
        fn goal_clone(&mut self, goal: &GoalHandle) -> Option<GoalHandle> {
            let kind = self.goal_kind(goal);
            self.calls.push(Call::Clone(kind));
            self.live += 1;
            GoalHandle::from_ptr(Box::into_raw(Box::new(kind)) as *mut c_void)
        }
        fn group_remove_unit(&mut self, group: HostRef, unit: u32) {
            if let Some(ids) = self.groups.get_mut(&(group.0 as usize)) {
                if let Some(at) = ids.iter().position(|&id| id == unit) {
                    ids.remove(at);
                }
            }
            self.calls.push(Call::RemoveUnit(group, unit));
        }
        fn unit_clear_goals(&mut self, unit: u32) {
            self.calls.push(Call::ClearGoals(unit));
        }
        fn object_hit_points(&mut self, unit: u32) -> i64 {
            self.calls.push(Call::ObjectHitPoints(unit));
            self.object_hp.get(&unit).copied().unwrap_or(0)
        }
    }

    impl MockHost {
        /// a unit of `leave_on_order` leaves every group (once)
        fn leave(&mut self, unit: u32) {
            if let Some(at) = self.leave_on_order.iter().position(|&id| id == unit) {
                self.leave_on_order.remove(at);
                for ids in self.groups.values_mut() {
                    ids.retain(|&id| id != unit);
                }
            }
        }
    }

    #[test]
    fn the_constructors_kinds() {
        let kinds: Vec<_> = (0..14).map(|c| EeGoalSpec::new(c).kind()).collect();
        assert_eq!(
            kinds,
            vec![
                None,
                Some(10),
                Some(3),
                Some(2),
                Some(6),
                Some(15),
                Some(9),
                Some(9),
                Some(1),
                Some(8),
                Some(1),
                Some(1),
                Some(11),
                None
            ]
        );
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
