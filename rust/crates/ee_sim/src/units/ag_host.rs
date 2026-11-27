//! The active group bodies' host in the game (R5b.4, the thirty-fifth round): [`AgHost`] over one C++ switch
//! (`ee_units_ag_host`, EEActiveGroup.cpp), and the entry points the `EEActiveGroup` members call. Groups, units,
//! goals and the other objects cross as their addresses. Absent from a test binary.

#![cfg(not(test))]

use std::ffi::c_void;

use crate::formations::units::{formation_arrange, formation_fields};
use crate::units::ag::{self, AgHost, EeUnitsAgGroup, EeUnitsAgUnit, LocationFields, NewLocation, Rect, Slot};
use crate::units::group_host::GameGroups;
use crate::units::group_obj as go;
use ee_base::math::transform::RotationOrder;
use ee_base::math::types::GeTransformation;

/// One host operation's values.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeUnitsAgIo {
    pub a: i64,
    pub b: i64,
    pub c: i64,
    pub d: i64,
    pub f: f32,
    /// a point's x, y, z
    pub p: [f32; 3],
    /// a second point
    pub q: [f32; 3],
    pub unit: EeUnitsAgUnit,
    pub group: EeUnitsAgGroup,
    /// the values the operation reads
    pub list: *mut c_void,
}

impl Default for EeUnitsAgIo {
    fn default() -> Self {
        EeUnitsAgIo {
            a: 0,
            b: 0,
            c: 0,
            d: 0,
            f: 0.0,
            p: [0.0; 3],
            q: [0.0; 3],
            unit: EeUnitsAgUnit::default(),
            group: EeUnitsAgGroup::default(),
            list: std::ptr::null_mut(),
        }
    }
}

// ---- the host operations (`ee_units_ag_host`'s `op`; `a`, `b` objects, `c`, `d` values)
/// group `a`'s state into `io.group`
pub const EE_UNITS_AG_GROUP: i32 = 1;
/// group `a`'s `GetUnit(c)`
pub const EE_UNITS_AG_GROUP_UNIT: i32 = 2;
/// group `a`'s `mMovesTogether = true`
pub const EE_UNITS_AG_MOVES_TOGETHER: i32 = 5;
/// `EESendGroupCommand(a, b, a's player id, c, 0)`
pub const EE_UNITS_AG_SEND: i32 = 8;
/// `gWorld.GetUnit(c)`'s inner collision sphere into `io.f` (0: no unit)
pub const EE_UNITS_AG_WORLD_SPHERE: i32 = 15;
/// `ee_combat_regions_size(gWorld.mMap->mUnitRegions)`
pub const EE_UNITS_AG_REGIONS_SIZE: i32 = 16;
/// the world table's group speed cap factor into `io.f`
pub const EE_UNITS_AG_CAP_FACTOR: i32 = 17;
/// group `a`'s 17 values `mSpeed .. mCapFactor` from `io.list`
pub const EE_UNITS_AG_SET_VALUES: i32 = 18;
/// unit `a` into `io.unit`
pub const EE_UNITS_AG_UNIT: i32 = 20;
/// unit `a`'s `GetQueuedGoal(c)`
pub const EE_UNITS_AG_QUEUED_GOAL: i32 = 21;
/// unit `a`'s `ClearGoalQueue(c)`
pub const EE_UNITS_AG_CLEAR_QUEUE: i32 = 22;
/// unit `a`'s `ClearGoals(c)`
pub const EE_UNITS_AG_CLEAR_GOALS: i32 = 23;
/// unit `a`'s `QueueGoal(b, c)`
pub const EE_UNITS_AG_QUEUE_GOAL: i32 = 24;
/// unit `a`'s `SetGoal(b, 0, c)`
pub const EE_UNITS_AG_SET_GOAL: i32 = 25;
/// unit `a`'s `mGoalQueues[1].AddTime(1)`
pub const EE_UNITS_AG_ADD_TIME: i32 = 27;
/// unit `a`'s `SetGatheredResource(NULL)`
pub const EE_UNITS_AG_CLEAR_GATHERED: i32 = 30;
/// unit `a`'s `mIsSpecialAttack = c`
pub const EE_UNITS_AG_SET_SPECIAL: i32 = 31;
/// unit `a`'s `mFormationSlot = c` (unless -1), `mFormationRank = d`
pub const EE_UNITS_AG_SET_PLACE: i32 = 32;
/// unit `a`'s `CanTake(b, c)`
pub const EE_UNITS_AG_CAN_TAKE: i32 = 33;
/// `gWorld.mMap->GetTile(c, d) != NULL`
pub const EE_UNITS_AG_HAS_TILE: i32 = 35;
/// `gContinentManager.CanGetTo(a, c, d)`
pub const EE_UNITS_AG_CAN_GET_TO: i32 = 37;
/// `EECanConvert(a, b, flag)`, the flag in and out in `io.a`
pub const EE_UNITS_AG_CAN_CONVERT: i32 = 38;
/// unit `a`'s `CanAttackClass(b)`
pub const EE_UNITS_AG_CAN_ATTACK_CLASS: i32 = 39;
/// class `a`'s `FindAction(c) != NULL`
pub const EE_UNITS_AG_HAS_ACTION: i32 = 40;
/// unit `a`'s `GetCommandType()`
pub const EE_UNITS_AG_COMMAND_TYPE: i32 = 41;
/// unit `a`'s `GetComplexUnit()->GetNumCargoUnits()`
pub const EE_UNITS_AG_NUM_CARGO: i32 = 42;
/// the units of tile (c, d) (pushed)
pub const EE_UNITS_AG_TILE_UNITS: i32 = 45;
/// unit `a`'s `IsPassableBy(b)`
pub const EE_UNITS_AG_PASSABLE: i32 = 46;
/// `EEIsSeenBy(a, player b, true)`
pub const EE_UNITS_AG_SEEN_BY: i32 = 47;
/// `EECanPriestConvert(a, b)`
pub const EE_UNITS_AG_PRIEST_CONVERT: i32 = 48;
/// goal `a`'s `mKind` (-1: none)
pub const EE_UNITS_AG_GOAL_KIND: i32 = 60;
/// gather goal `a`'s `GetGatherStorage()`
pub const EE_UNITS_AG_GATHER_STORAGE: i32 = 72;
/// gather goal `a`'s `GetGatherResource()`
pub const EE_UNITS_AG_GATHER_RESOURCE: i32 = 73;
/// patrol goal `a`'s `GetPatrolTarget()`, else `GetPatrolGroup()->GetLast()`
pub const EE_UNITS_AG_PATROL_POINT: i32 = 81;
// the compound statements as one-statement operations (the groups lane, round 2)
/// unit `a`'s `mGoalQueues[c].Add(goal b)`
pub const EE_UNITS_AG_QUEUE_ADD_ONLY: i32 = 90;
/// unit `a`'s `RefreshGoals()`
pub const EE_UNITS_AG_REFRESH_GOALS: i32 = 91;
/// unit `a`'s `RefreshActiveGoal()`
pub const EE_UNITS_AG_REFRESH_ACTIVE_GOAL: i32 = 92;
/// unit `a`'s `StartAction(c)`
pub const EE_UNITS_AG_START_ACTION: i32 = 93;
/// unit `a`'s class's `ActionChanged(unit)`
pub const EE_UNITS_AG_ACTION_CHANGED: i32 = 94;
/// unit `a`'s `QueueSquadronGoals(goal b, c)`
pub const EE_UNITS_AG_QUEUE_SQUADRON: i32 = 95;
/// unit `a`'s `SetSquadronGoals(goal b, c)`
pub const EE_UNITS_AG_SET_SQUADRON: i32 = 96;
/// unit `a`'s `GetPower()`
pub const EE_UNITS_AG_POWER: i32 = 97;
/// calamity class `a`'s `GetCost()`
pub const EE_UNITS_AG_CALAMITY_COST: i32 = 98;
/// tech node `a`'s `Contains(unit b's class's mUnitType)`
pub const EE_UNITS_AG_NODE_HAS_TYPE: i32 = 99;
/// `gPhysics->GetRandom(c, d)`
pub const EE_UNITS_AG_RANDOM: i32 = 100;
/// the map tile at `UMFloorUnsigned` of `p` is there
pub const EE_UNITS_AG_TILE_PRESENT: i32 = 101;
/// that tile's `GetHeight()` into `f`
pub const EE_UNITS_AG_TILE_HEIGHT: i32 = 102;
/// that tile's `mTerrainType`
pub const EE_UNITS_AG_TILE_TERRAIN: i32 = 103;

extern "C" {
    fn ee_units_ag_host(op: i32, a: u64, b: u64, c: i64, d: i64, io: *mut EeUnitsAgIo) -> i64;
}
use crate::combat::cg_query::rust_calls::ee_units_eu_nearest_footprint_tile;

/// The game's world for the active group bodies.
pub struct AgGame;

impl AgGame {
    fn io(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, io: &mut EeUnitsAgIo) -> i64 {
        if let Some(v) = crate::units::dm_group_view::ag_plain(op, a, b, c) {
            return v;
        }
        // SAFETY: the C++ switch reads live objects the bodies hand it
        unsafe { ee_units_ag_host(op, a, b, c, d, io) }
    }
    fn op(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64) -> i64 {
        let mut io = EeUnitsAgIo::default();
        self.io(op, a, b, c, d, &mut io)
    }
}

impl AgHost for AgGame {
    // (`c_long` is 32 bits on Windows and 64 elsewhere: the cast is needed there)
    #[allow(clippy::unnecessary_cast)]
    fn group(&mut self, g: u64) -> EeUnitsAgGroup {
        // the group's record and members in place (the units' state lane: once the C++ host's EE_UNITS_AG_GROUP)
        let f = group_fields(g);
        // SAFETY: a live group's `mGoal` member (the layout's offset)
        let goal = unsafe { read_at::<usize>(g, layout().active_group_goal) } as u64;
        // SAFETY: a group body runs inside the C++ call that holds the world
        let t = unsafe { WorldRead::begin() };
        // SAFETY: a live group
        let count = unsafe { GroupView::of(&t, g) }.map_or(0, |v| v.len()) as u32;
        EeUnitsAgGroup {
            goal,
            goal_kind: goal_kind_of(goal),
            action: f.mAction as i32,
            count,
            speed: f.mSpeed,
            queued: f.mIsQueued as u8,
            moves_together: f.mMovesTogether as u8,
            special_attack: f.mIsSpecialAttack as u8,
        }
    }
    fn group_unit(&mut self, g: u64, i: u32) -> u64 {
        // SAFETY: as above
        let t = unsafe { WorldRead::begin() };
        // SAFETY: a live group (`GetUnit(i)`: the vector's element; the C++ read past the end, the view gives 0)
        unsafe { GroupView::of(&t, g) }.map_or(0, |v| v.unit_handle(i as usize))
    }
    fn remove(&mut self, g: u64, u: u64) {
        // SAFETY: a live group and unit (the group object, group_obj.rs)
        unsafe { go::remove_unit(&mut GameGroups, g as go::G, u) }
    }
    fn finish(&mut self, g: u64) {
        // SAFETY: as above
        unsafe { go::finish(&mut GameGroups, g as go::G) }
    }
    fn set_moves_together(&mut self, g: u64) {
        with_group_fields(g, |f| f.mMovesTogether = true);
    }
    fn request_all_paths(&mut self, g: u64) {
        // SAFETY: as above
        unsafe { go::request_all_paths(&mut GameGroups, g as go::G) }
    }
    fn request_paths(&mut self, g: u64, units: &[u64]) {
        // SAFETY: as above
        unsafe { go::request_paths(&mut GameGroups, g as go::G, units) };
    }
    fn send_command(&mut self, g: u64, target: u64, command: u32) {
        self.op(EE_UNITS_AG_SEND, g, target, command as i64, 0);
    }
    fn arrange(&mut self, g: u64, center: [f32; 3], yaw: Option<f32>) -> Vec<Slot> {
        // the original's statements in order: the centre, with a yaw the heading and the orientation, then Arrange
        // (the formation's members: its Rust record, formations/units.rs; units round 14)
        // SAFETY: a live group
        let formation = unsafe { (*(g as go::G)).mFormation } as u64;
        // SAFETY: the group's live formation
        let f = unsafe { formation_fields(formation) };
        f.mCenter = center;
        if let Some(yaw) = yaw {
            f.mHeading = yaw;
            let mut t = GeTransformation { m: f.mOrientation };
            t.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
            f.mOrientation = t.m;
        }
        // SAFETY: the group's live formation and units
        let slots = unsafe { formation_arrange(formation, g) };
        slots.into_iter().map(|s| Slot { position: s.position, unit: s.unit as u64, heading: s.heading }).collect()
    }
    fn new_group(&mut self, g: u64) -> u64 {
        // SAFETY: as above
        let player = unsafe { (*(g as go::G)).mPlayer } as u64;
        go::new(&mut GameGroups, go::EE_UNITS_GROUP_KIND_ACTIVE, player, 0) as u64
    }
    fn add(&mut self, g: u64, u: u64) {
        // SAFETY: as above
        unsafe { go::add(&mut GameGroups, g as go::G, u) }
    }
    fn set_formation(&mut self, ng: u64, g: u64) {
        // SAFETY: as above
        unsafe {
            let formation = (*(g as go::G)).mFormation as u64;
            go::set_formation(&mut GameGroups, ng as go::G, formation)
        }
    }
    #[allow(clippy::too_many_arguments)] // as the trait's
    fn launch(&mut self, g: u64, ng: u64, goal: u64, queued: bool, action: i32, target: u64, command: u32) {
        // SAFETY (all): the live groups the body holds
        let player = unsafe { (*(g as go::G)).mPlayer } as u64;
        unsafe { go::set_goal(&mut GameGroups, ng as go::G, goal, queued) };
        with_group_fields(ng, |f| f.mAction = action as std::os::raw::c_long);
        GameGroups.player_groups_push(player, go::EE_UNITS_PLAYER_GROUPS_PENDING, ng);
        crate::units::ag::plan(self, ng);
        self.send_command(ng, target, command);
        let n = unsafe { go::len(ng as go::G) } as i64;
        if n > 0 {
            let i = self.op(EE_UNITS_AG_RANDOM, 0, 0, 0, n - 1);
            let unit = unsafe { go::unit_at(ng as go::G, i as std::os::raw::c_long as usize) };
            crate::units::selection::play_acknowledgement(&mut GameGroups, player, unit);
        }
    }
    fn destinations(&mut self, g: u64) -> Vec<(u32, [f32; 3])> {
        // SAFETY: as above
        unsafe { go::destinations(g as go::G) }
    }
    fn world_unit_sphere(&mut self, id: u32) -> Option<f32> {
        // `gWorld.GetUnit(id)`'s class's inner collision sphere
        // SAFETY: the group's call runs inside the C++ caller's hold of the world
        let t = unsafe { WorldRead::begin() };
        let u = crate::world::view::UnitView::by_id(&t, id)?;
        Some(crate::world_field!(u.class()?.fields(), mInnerCollisionSphere))
    }
    fn regions_size(&mut self) -> i32 {
        // SAFETY: as above
        let t = unsafe { WorldRead::begin() };
        let regions = WorldView::global(&t).and_then(|w| w.map()).map_or(std::ptr::null(), |m| m.unit_regions());
        // SAFETY: the map's unit regions (`ee_combat_regions_size(gWorld.mMap->mUnitRegions)`)
        unsafe { crate::combat::regions::ee_combat_regions_size(regions) }
    }
    fn cap_factor(&mut self) -> f32 {
        let mut io = EeUnitsAgIo::default();
        self.io(EE_UNITS_AG_CAP_FACTOR, 0, 0, 0, 0, &mut io);
        io.f
    }
    fn set_values(&mut self, g: u64, values: &[f32; 17]) {
        with_group_fields(g, |f| f.set_values(values));
    }
    fn unit(&mut self, u: u64) -> EeUnitsAgUnit {
        // the unit's fields over the world views (the units' round 1: once the C++ host's fill, EE_UNITS_AG_UNIT)
        // SAFETY: a group body runs inside the C++ call that holds the world; `u` is a live unit
        let t = unsafe { crate::world::WorldRead::begin() };
        match unsafe { crate::world::UnitView::of(&t, u) } {
            Some(v) => ag_unit_fields(&v),
            None => EeUnitsAgUnit::default(),
        }
    }
    fn queued_goal(&mut self, u: u64, q: u32) -> u64 {
        self.op(EE_UNITS_AG_QUEUED_GOAL, u, 0, q as i64, 0) as u64
    }
    fn clear_goal_queue(&mut self, u: u64, q: u32) {
        self.op(EE_UNITS_AG_CLEAR_QUEUE, u, 0, q as i64, 0);
    }
    fn clear_goals(&mut self, u: u64, q: u32) {
        self.op(EE_UNITS_AG_CLEAR_GOALS, u, 0, q as i64, 0);
    }
    fn queue_goal(&mut self, u: u64, goal: u64, q: u32) {
        self.op(EE_UNITS_AG_QUEUE_GOAL, u, goal, q as i64, 0);
    }
    fn set_goal(&mut self, u: u64, goal: u64, q: u32) {
        self.op(EE_UNITS_AG_SET_GOAL, u, goal, q as i64, 0);
    }
    fn queue_add(&mut self, u: u64, goal: u64, q: u32) {
        self.op(EE_UNITS_AG_QUEUE_ADD_ONLY, u, goal, q as i64, 0);
        self.op(EE_UNITS_AG_REFRESH_GOALS, u, 0, 0, 0);
        self.op(EE_UNITS_AG_REFRESH_ACTIVE_GOAL, u, 0, 0, 0);
    }
    fn add_time(&mut self, u: u64) {
        self.op(EE_UNITS_AG_ADD_TIME, u, 0, 0, 0);
    }
    fn act(&mut self, u: u64, action: i32) {
        self.op(EE_UNITS_AG_START_ACTION, u, 0, action as i64, 0);
        self.op(EE_UNITS_AG_ACTION_CHANGED, u, 0, 0, 0);
    }
    fn squadron(&mut self, u: u64, goal: u64, flag: bool, queued: bool) {
        let op = if queued { EE_UNITS_AG_QUEUE_SQUADRON } else { EE_UNITS_AG_SET_SQUADRON };
        self.op(op, u, goal, flag as i64, 0);
    }
    fn clear_gathered(&mut self, u: u64) {
        self.op(EE_UNITS_AG_CLEAR_GATHERED, u, 0, 0, 0);
    }
    fn set_special_attack(&mut self, u: u64, v: bool) {
        with_complex_fields(u, |c| c.mIsSpecialAttack = v);
    }
    fn set_formation_place(&mut self, u: u64, slot: Option<u32>, rank: i32) {
        with_complex_fields(u, |c| {
            if let Some(s) = slot {
                c.mFormationSlot = s as std::os::raw::c_ulong;
            }
            c.mFormationRank = rank as std::os::raw::c_long;
        });
    }
    fn can_take(&mut self, storage: u64, u: u64, resource_type: u32) -> bool {
        self.op(EE_UNITS_AG_CAN_TAKE, storage, u, resource_type as i64, 0) != 0
    }
    // (`c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there)
    #[allow(clippy::unnecessary_cast)]
    fn nearest_footprint(&mut self, u: u64, target: u64, x: i32, y: i32) -> (i32, i32) {
        // `u->GetNearestFootprintTile(target, &x, &y)` (eu_body.rs; the outs start as the caller's values)
        let (mut x, mut y) = (x as std::os::raw::c_long, y as std::os::raw::c_long);
        // SAFETY: two live units of the body's world; the outs are locals
        unsafe {
            ee_units_eu_nearest_footprint_tile(
                u as usize as *mut c_void,
                target as usize as *mut c_void,
                &mut x,
                &mut y,
            )
        };
        (x as i32, y as i32)
    }
    fn has_tile(&mut self, x: i32, y: i32) -> bool {
        // SAFETY: as above
        let t = unsafe { WorldRead::begin() };
        WorldView::global(&t).and_then(|w| w.map()).is_some_and(|m| m.tile(i64::from(x), i64::from(y)).is_some())
    }
    fn tile_at(&mut self, p: [f32; 3]) -> Option<(f32, i32)> {
        let mut io = EeUnitsAgIo { p, ..Default::default() };
        if self.io(EE_UNITS_AG_TILE_PRESENT, 0, 0, 0, 0, &mut io) == 0 {
            return None;
        }
        self.io(EE_UNITS_AG_TILE_HEIGHT, 0, 0, 0, 0, &mut io);
        let terrain = self.io(EE_UNITS_AG_TILE_TERRAIN, 0, 0, 0, 0, &mut io);
        Some((io.f, terrain as i32))
    }
    fn can_get_to(&mut self, u: u64, x: i32, y: i32) -> bool {
        self.op(EE_UNITS_AG_CAN_GET_TO, u, 0, x as i64, y as i64) != 0
    }
    fn can_convert(&mut self, u: u64, target: u64, flag: &mut bool) -> bool {
        // (the flag in place: `io.list` names the caller's bool)
        let mut io = EeUnitsAgIo { list: (flag as *mut bool).cast(), ..Default::default() };
        self.io(EE_UNITS_AG_CAN_CONVERT, u, target, 0, 0, &mut io) != 0
    }
    fn can_attack_class(&mut self, u: u64, class: u64) -> bool {
        self.op(EE_UNITS_AG_CAN_ATTACK_CLASS, u, class, 0, 0) != 0
    }
    fn has_action(&mut self, class: u64, action: i32) -> bool {
        self.op(EE_UNITS_AG_HAS_ACTION, class, 0, action as i64, 0) != 0
    }
    fn command_type(&mut self, u: u64) -> i32 {
        // SAFETY: as above; `u` is a live complex unit
        let t = unsafe { WorldRead::begin() };
        unsafe { UnitView::of(&t, u) }.map_or(0, |v| v.command_type() as i32)
    }
    fn num_cargo(&mut self, u: u64) -> u32 {
        // `GetComplexUnit()->GetNumCargoUnits()`
        // SAFETY: as above; `u` is a live unit
        let t = unsafe { WorldRead::begin() };
        unsafe { UnitView::of(&t, u) }.and_then(|v| v.complex_unit()).map_or(0, |c| c.cargo_len() as u32)
    }
    fn position(&mut self, u: u64) -> [f32; 3] {
        crate::units::unit_geom::position_of(u)
    }
    fn angle_from_north(&mut self, from: [f32; 3], to: [f32; 3]) -> f32 {
        // `EEGetAngleFromNorth(from, to)` (uu_ffi.rs)
        // SAFETY: two points for the call
        unsafe { crate::units::uu_ffi::ee_units_uu_angle(1, 0, 0, &from, &to) }
    }
    fn tile_units(&mut self, x: i32, y: i32) -> Vec<u64> {
        // the tile's nodes' units in order (a NULL unit as 0)
        // SAFETY: as above
        let t = unsafe { WorldRead::begin() };
        let tile = WorldView::global(&t).and_then(|w| w.map()).and_then(|m| m.tile(i64::from(x), i64::from(y)));
        tile.map_or_else(Vec::new, |tl| tl.units().map(|u| u.map_or(0, |v| v.handle())).collect())
    }
    fn passable_by(&mut self, obj: u64, by: u64) -> bool {
        self.op(EE_UNITS_AG_PASSABLE, obj, by, 0, 0) != 0
    }
    fn seen_by(&mut self, target: u64, player: u64) -> bool {
        self.op(EE_UNITS_AG_SEEN_BY, target, player, 0, 0) != 0
    }
    fn can_priest_convert(&mut self, u: u64, target: u64) -> bool {
        self.op(EE_UNITS_AG_PRIEST_CONVERT, u, target, 0, 0) != 0
    }
    fn can_cast(&mut self, u: u64, calamity: u64, node: u64) -> bool {
        self.op(EE_UNITS_AG_POWER, u, 0, 0, 0) >= self.op(EE_UNITS_AG_CALAMITY_COST, calamity, 0, 0, 0)
            && self.op(EE_UNITS_AG_NODE_HAS_TYPE, node, u, 0, 0) != 0
    }
    fn reference(&mut self, u: u64) {
        GameGroups.reference_unit(u)
    }
    fn dereference(&mut self, u: u64) {
        GameGroups.dereference_unit(u)
    }
    fn rect_closest(&mut self, r: Rect, p: [f32; 2]) -> [f32; 2] {
        // `U2DRectangle<float>::GetClosestPoint` (ee_base math u2d.rs)
        ee_base::math::u2d::rect_closest_point([r.left, r.top, r.right, r.bottom], p)
    }
    fn goal_kind(&mut self, goal: u64) -> i32 {
        goal_kind_of(goal)
    }
    // the goals are Rust objects (goal_obj.rs): the goal calls are the C++ accessors' (ee/EEUnitGoal.h)
    fn clone_goal(&mut self, goal: u64) -> u64 {
        // `Clone()`: the kind's clone with the goal's mB1C
        let b1c = goal_b1c(goal);
        goal_call(EE_UNITS_GOAL_CLONE, goal, 0, b1c as u64, &mut EeUnitsGoalIo::default()) as u64
    }
    fn delete_goal(&mut self, goal: u64) {
        // SAFETY: a goal the body owns, used by no one after
        unsafe { crate::units::goal_obj::goal_delete(goal) }
    }
    fn location_fields(&mut self, goal: u64) -> LocationFields {
        let l = crate::units::location::get(goal);
        LocationFields { b1c: goal_b1c(goal), forced: l.forced_move, should_face: l.should_face, yaw: l.yaw }
    }
    fn goal_b1c(&mut self, goal: u64) -> bool {
        goal_b1c(goal)
    }
    fn goal_location(&mut self, goal: u64) -> [f32; 3] {
        // `goal->GetGoalLocation(point)` (ee/EEUnitGoal.h: the goal call seeded with the point)
        let mut point = [0.0f32; 3];
        // SAFETY: a live goal of the body's
        unsafe {
            crate::units::goal_calls::ee_units_goal_point(
                crate::units::rust_goal::EE_UNITS_GOAL_LOCATION,
                goal as usize as *const _,
                0,
                &mut point,
                true,
            )
        };
        point
    }
    fn new_location(&mut self, l: NewLocation) -> u64 {
        // `EEUGRust::NewLocation(&point, b1c, forced, face, yaw, auto_waypoint, group_state)` (goal_calls.rs)
        let [x, y, z] = l.point;
        crate::units::goal_calls::ee_units_goal_new_location(
            l.b1c,
            x,
            y,
            z,
            l.forced,
            l.face,
            l.yaw,
            l.auto_waypoint,
            l.group_state,
        ) as usize as u64
    }
    fn new_unload(&mut self, u: u64, x: i32, y: i32) -> u64 {
        // `NewUnloadTransport(u, x, y, true)`
        let goal = crate::units::goal_obj::goal_new(GOAL_UNLOAD_TRANSPORT, true);
        let mut io = EeUnitsGoalIo::default();
        io.i[0] = x as i64;
        io.i[1] = y as i64;
        goal_call(EE_UNITS_GOAL_NEW_UNLOAD, goal, u, 0, &mut io);
        goal
    }
    fn can_unload_at_destination(&mut self, goal: u64) -> bool {
        goal_call(EE_UNITS_GOAL_UNLOAD_CAN_UNLOAD, goal, 0, 0, &mut EeUnitsGoalIo::default()) != 0
    }
    fn unload_destination(&mut self, goal: u64) -> (i32, i32) {
        // `GetUnloadTransport(&x, &y)`'s tile (as `long`)
        let mut io = EeUnitsGoalIo::default();
        goal_call(EE_UNITS_GOAL_UNLOAD_FIELDS, goal, 0, 0, &mut io);
        (io.i[0] as i32, io.i[1] as i32)
    }
    fn set_b1d(&mut self, goal: u64) {
        // SAFETY: a live goal's base record
        unsafe { crate::units::goal_obj::base(goal) }.b1d = true;
    }
    fn clear_group_path_state(&mut self, goal: u64) {
        // `SetGroupPathState(0)`
        let l = crate::units::location::get(goal);
        crate::units::location::put(goal, crate::units::location::EeUnitsLocationGoal { group_path_state: 0, ..l });
    }
    fn gather_storage(&mut self, goal: u64) -> u64 {
        // `GetGatherStorage()`: the gather goal's record (another kind's answer: none)
        if goal_kind_of(goal) == GOAL_GATHER {
            crate::units::gather::storage_of(goal)
        } else {
            0
        }
    }
    fn gather_resource(&mut self, goal: u64) -> u64 {
        if goal_kind_of(goal) == GOAL_GATHER {
            crate::units::gather::resource_of(goal)
        } else {
            0
        }
    }
    fn set_gather_storage(&mut self, goal: u64, storage: u64) {
        goal_call(EE_UNITS_GOAL_GATHER_SET_STORAGE, goal, storage, 0, &mut EeUnitsGoalIo::default());
    }
    fn attack_target(&mut self, goal: u64) -> u64 {
        crate::combat::goal_attack::attack_of(goal).target
    }
    fn is_convert_attack(&mut self, goal: u64) -> bool {
        crate::combat::goal_attack::attack_of(goal).convert != 0
    }
    fn clear_attacker(&mut self, goal: u64) {
        // `SetAttacker(NULL)`
        let a = crate::combat::goal_attack::attack_of(goal);
        let cleared = crate::combat::goal_attack::EeCombatAttackGoal { attacker: 0, ..a };
        // SAFETY: a live attack goal's record
        unsafe { crate::combat::goal_attack::ee_combat_attack_set(goal, &cleared) }
    }
    fn heal_target(&mut self, goal: u64) -> u64 {
        crate::combat::goal_heal::heal_of(goal).target
    }
    fn suits(&mut self, goal: u64, u: u64) -> bool {
        goal_call(EE_UNITS_GOAL_SUITS, goal, u, 0, &mut EeUnitsGoalIo::default()) != 0
    }
    fn set_explore_tile(&mut self, goal: u64, x: i32, y: i32) {
        let mut io = EeUnitsGoalIo::default();
        io.i[0] = x as i64;
        io.i[1] = y as i64;
        goal_call(EE_UNITS_GOAL_EXPLORE_SET_TILE, goal, 0, 0, &mut io);
    }
    fn patrol_point(&mut self, goal: u64) -> u64 {
        // `GetPatrolTarget()`, else `ee_units_pg_last(GetPatrolGroup())` (a patrol goal's)
        match crate::units::patrol::target(goal) {
            0 => {
                let group =
                    crate::units::patrol::group(goal) as usize as *mut crate::units::patrol_group::EeUnitsPatrolGroup;
                // SAFETY: the goal's live patrol group
                unsafe { crate::units::patrol_group::ee_units_pg_last(group) as usize as u64 }
            }
            t => t,
        }
    }
    fn new_attack_unit(&mut self, first: u64, target: u64) -> u64 {
        // `NewAttackUnit(first, target, true, false)`
        let goal = crate::units::goal_obj::goal_new(GOAL_ATTACK_UNIT, true);
        goal_call(
            crate::combat::goal_dispatch::EE_COMBAT_GOAL_NEW_ATTACK,
            goal,
            first,
            target,
            &mut EeUnitsGoalIo::default(),
        );
        goal
    }
}

use crate::units::group_obj::GroupHost;
use crate::units::rust_goal::{
    EeUnitsGoalIo, EE_UNITS_GOAL_CLONE, EE_UNITS_GOAL_EXPLORE_SET_TILE, EE_UNITS_GOAL_GATHER_SET_STORAGE,
    EE_UNITS_GOAL_NEW_UNLOAD, EE_UNITS_GOAL_SUITS, EE_UNITS_GOAL_UNLOAD_CAN_UNLOAD, EE_UNITS_GOAL_UNLOAD_FIELDS,
};

/// `kGoalUnloadTransport`, `kGoalAttackUnit`
const GOAL_UNLOAD_TRANSPORT: u8 = 8;
const GOAL_ATTACK_UNIT: u8 = 2;

/// One body of a goal (`EEUnitGoal::Call`).
fn goal_call(op: i32, goal: u64, a: u64, b: u64, io: &mut EeUnitsGoalIo) -> i64 {
    // SAFETY: a live goal; io lives for the call
    unsafe { crate::units::goal_obj::call(op, goal, a, b, io) }
}

/// A goal's `mB1C`.
fn goal_b1c(goal: u64) -> bool {
    // SAFETY: a live goal's base record
    unsafe { crate::units::goal_obj::base(goal) }.b1c
}

// ---- the entry points (the `EEActiveGroup` members; `group` is the group's address)

/// `EEActiveGroup::Plan`.
#[no_mangle]
pub extern "C" fn ee_units_ag_plan(group: u64) {
    ag::plan(&mut AgGame, group)
}

/// `EEActiveGroup::LayOutFormation`.
#[no_mangle]
pub extern "C" fn ee_units_ag_lay_out_formation(group: u64) {
    ag::lay_out_formation(&mut AgGame, group)
}

/// `EEActiveGroup::ComputeValues`.
#[no_mangle]
pub extern "C" fn ee_units_ag_compute_values(group: u64) {
    ag::compute_values(&mut AgGame, group)
}

/// `EEActiveGroup::KeepAttackers`.
#[no_mangle]
pub extern "C" fn ee_units_ag_keep_attackers(group: u64) {
    ag::keep_attackers(&mut AgGame, group)
}

/// `EEActiveGroup::KeepMobileUnits`.
#[no_mangle]
pub extern "C" fn ee_units_ag_keep_mobile(group: u64) {
    ag::keep_mobile(&mut AgGame, group)
}

/// `EEActiveGroup::SpreadAroundUnit(target, x, y)`.
#[no_mangle]
pub extern "C" fn ee_units_ag_spread_around_unit(group: u64, target: u64, x: i32, y: i32) {
    ag::spread_around_unit(&mut AgGame, group, target, x, y)
}

/// `EEActiveGroup::SpreadAroundResource(target)`.
#[no_mangle]
pub extern "C" fn ee_units_ag_spread_around_resource(group: u64, target: u64) {
    ag::spread_around_resource(&mut AgGame, group, target)
}

/// `EEActiveGroup::SpreadAroundTile(x, y)` (`for_unload` 0) and `SpreadForUnload(x, y)` (1).
#[no_mangle]
pub extern "C" fn ee_units_ag_spread_around_tile(group: u64, x: i32, y: i32, for_unload: u8) {
    ag::spread_around_tile(&mut AgGame, group, x, y, for_unload != 0)
}

/// `EEActiveGroup::SplitCalamityCasters(calamity, node, target)`.
#[no_mangle]
pub extern "C" fn ee_units_ag_split_calamity_casters(group: u64, calamity: u64, node: u64, target: u64) {
    ag::split_calamity_casters(&mut AgGame, group, calamity, node, target)
}

/// `EEActiveGroup::IsReserved(point)` (`which` 0) and `IsCellTaken(point)` (1).
///
/// # Safety
/// `point` is three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_ag_reserved(group: u64, point: *const [f32; 3], which: u8) -> u8 {
    // SAFETY: the caller's contract
    let p = unsafe { *point };
    (if which == 0 { ag::is_reserved(&mut AgGame, group, p) } else { ag::is_cell_taken(&mut AgGame, group, p) }) as u8
}

/// `kUnitCategory*` of `IsPriest()` and `IsTransport()`; the crusaders' unit types (`IsCrusader()`)
const PRIEST_CATEGORIES: [u64; 4] = [0xd, 0xe, 0x1a, 0x1e];
const TRANSPORT_CATEGORIES: [u64; 5] = [0x10, 0x11, 0x12, 0x21, 0x1d];
const CRUSADER_TYPES: [u64; 4] = [0x2e6, 0x2e7, 0x2e8, 0x2e9];

/// The fields of a unit the group bodies read (`EE_UNITS_AG_UNIT`, once a C++ fill) over the views: the unit's, its
/// complex unit's (`mComplexUnit`), its class's and its stats'.
// ---- the records in place (the units' state lane, round 1)
use crate::units::ag_fields::EeUnitsActiveGroupFields;
use crate::units::unit_fields::EeUnitsComplexUnitFields;
use crate::world::layout::layout;
use crate::world::{GroupView, UnitView, WorldRead, WorldView};

/// The value of type `T` at `object + offset`.
///
/// # Safety
/// The bytes there are a live member of type `T`.
unsafe fn read_at<T: Copy>(object: u64, offset: u64) -> T {
    // SAFETY: the caller's contract
    unsafe { std::ptr::read((object as usize + offset as usize) as *const T) }
}

/// An active group's record (`EeUnitsActiveGroupFields`, its second base), for one access.
fn with_group_fields<R>(g: u64, f: impl FnOnce(&mut EeUnitsActiveGroupFields) -> R) -> R {
    // SAFETY: a live EEActiveGroup's record base (the layout's offset); the borrow ends inside `f`, before any host
    // call
    f(unsafe { &mut *((g as usize + layout().active_group_fields as usize) as *mut EeUnitsActiveGroupFields) })
}

fn group_fields(g: u64) -> EeUnitsActiveGroupFields {
    with_group_fields(g, |f| *f)
}

/// A complex unit's record (`EeUnitsComplexUnitFields`), for one store.
fn with_complex_fields(u: u64, f: impl FnOnce(&mut EeUnitsComplexUnitFields)) {
    // SAFETY: a live EEComplexUnit's record base; the borrow ends inside `f`
    f(unsafe { &mut *((u as usize + layout().complex_unit_fields as usize) as *mut EeUnitsComplexUnitFields) })
}

/// `kGoalGather`
const GOAL_GATHER: i32 = 6;

/// A goal's `mKind` (-1: no goal)
fn goal_kind_of(goal: u64) -> i32 {
    if goal == 0 {
        return -1;
    }
    // SAFETY: a live group's goal (the C++ deletes it only through ClearGoal, which clears the member)
    i32::from(unsafe { read_at::<u8>(goal, layout().goal_kind) })
}

// (`c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there)
#[allow(clippy::unnecessary_cast)]
pub(crate) fn ag_unit_fields(u: &crate::world::UnitView) -> EeUnitsAgUnit {
    use crate::world_field;
    let class = u.class();
    let (x, y) = u.tile_xy();
    let mut o = EeUnitsAgUnit {
        obj: u.handle(),
        class_obj: class.map_or(0, |c| c.handle()),
        complex: u.complex_unit_handle(),
        player: u.player().map_or(0, |p| p.handle()),
        tile_x: x as i32,
        tile_y: y as i32,
        hit_points: u.hit_points() as i32,
        resource_amount: u.resource_amount() as i32,
        resource_type: u.resource_type() as u32,
        is_valid: u8::from(u.valid()),
        tile_height: u.tile().map_or(0.0, |t| t.height()),
        ..EeUnitsAgUnit::default()
    };
    if let Some(x) = u.complex_unit() {
        let c = x.complex();
        o.goal_state = x.goal_handle();
        o.container = x.container_handle();
        o.power = x.power() as i32;
        o.formation_slot = x.formation_slot() as u32;
        o.velocity = world_field!(c, mVelocity);
        o.location = x.location();
        o.commandable = u8::from(x.commandable());
        o.is_built = u8::from(x.is_built());
        o.detached = u8::from(world_field!(c, mIsDetachedFromAI));
    }
    if let Some(c) = class {
        let f = c.fields();
        o.category = c.category() as u32;
        o.unit_type = c.unit_type() as u32;
        o.ai_kind = c.ai_kind() as u32;
        o.family = c.family() as u32;
        o.tile_size = c.tile_size() as i32;
        o.air_traffic_type = world_field!(f, mAirTrafficType) as i32;
        o.class_max_velocity = world_field!(f, mMaxVelocity);
        o.inner_sphere = world_field!(f, mInnerCollisionSphere);
        o.can_attack_ground = u8::from(world_field!(f, mCanAttackGround));
        o.paratrooper_bomber = u8::from(world_field!(f, mIsParatrooperBomber));
        o.can_patrol = u8::from(world_field!(f, mCanPatrol));
        o.has_special_attack = u8::from(world_field!(f, mHasSpecialAttack));
        o.is_priest = u8::from(PRIEST_CATEGORIES.contains(&c.category()));
        o.is_crusader = u8::from(CRUSADER_TYPES.contains(&c.unit_type()));
        o.is_transport = u8::from(TRANSPORT_CATEGORIES.contains(&c.category()));
    }
    if let Some(s) = u.stats() {
        let f = s.fields();
        o.max_velocity = world_field!(f, mMaxVelocity);
        o.acceleration = world_field!(f, mAcceleration);
        o.max_angular_velocity = world_field!(f, mMaxAngularVelocity);
        o.angular_acceleration = world_field!(f, mAngularAcceleration);
        o.range = world_field!(f, mRange);
        o.line_of_sight = world_field!(f, mLineOfSight);
        o.attack = world_field!(f, mAttack);
    }
    o
}
