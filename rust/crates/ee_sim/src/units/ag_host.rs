//! The active group bodies' host in the game (R5b.4, the thirty-fifth round): [`AgHost`] over one C++ switch
//! (`ee_units_ag_host`, EEActiveGroup.cpp), and the entry points the `EEActiveGroup` members call. Groups, units,
//! goals and the other objects cross as their addresses. Absent from a test binary.

#![cfg(not(test))]

use std::ffi::c_void;

use crate::units::ag::{self, AgHost, EeUnitsAgGroup, EeUnitsAgUnit, LocationFields, NewLocation, Rect, Slot};

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
    /// the list the operation pushes onto (`ee_units_ag_push`), or the values it reads
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

/// The list a host operation fills.
#[derive(Default)]
pub struct EeUnitsAgListHandle {
    items: Vec<(u64, [f32; 3], f32)>,
}

// ---- the host operations (`ee_units_ag_host`'s `op`; `a`, `b` objects, `c`, `d` values)
/// group `a`'s state into `io.group`
pub const EE_UNITS_AG_GROUP: i32 = 1;
/// group `a`'s `GetUnit(c)`
pub const EE_UNITS_AG_GROUP_UNIT: i32 = 2;
/// group `a`'s `RemoveUnit(b)`
pub const EE_UNITS_AG_REMOVE: i32 = 3;
/// group `a`'s `Finish()`
pub const EE_UNITS_AG_FINISH: i32 = 4;
/// group `a`'s `mMovesTogether = true`
pub const EE_UNITS_AG_MOVES_TOGETHER: i32 = 5;
/// group `a`'s `RequestAllPaths()`
pub const EE_UNITS_AG_REQUEST_ALL: i32 = 6;
/// group `a`'s `RequestPaths(units)`: the `c` units at `io.list`
pub const EE_UNITS_AG_REQUEST_PATHS: i32 = 7;
/// `EESendGroupCommand(a, b, a's player id, c, 0)`
pub const EE_UNITS_AG_SEND: i32 = 8;
/// group `a`'s formation: centre `io.p`, heading `io.f` when `c`, then `Arrange` (slots pushed: unit, position,
/// heading)
pub const EE_UNITS_AG_ARRANGE: i32 = 9;
/// `new EEActiveGroup(a's mPlayer)`
pub const EE_UNITS_AG_NEW_GROUP: i32 = 10;
/// group `a`'s `Add(b)`
pub const EE_UNITS_AG_ADD: i32 = 11;
/// group `a`'s `SetFormation(b's mFormation)`
pub const EE_UNITS_AG_SET_FORMATION: i32 = 12;
/// group `b` launched from `a` (ag.rs `AgHost::launch`): goal `io.a`, target `io.b`, action `io.c`, command
/// `io.d`, queued `c`
pub const EE_UNITS_AG_LAUNCH: i32 = 13;
/// group `a`'s `mDestinations` (pushed: id, point)
pub const EE_UNITS_AG_DESTINATIONS: i32 = 14;
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
/// unit `a`'s `mGoalQueues[c].Add(b)`, `RefreshGoals()`, `RefreshActiveGoal()`
pub const EE_UNITS_AG_QUEUE_ADD: i32 = 26;
/// unit `a`'s `mGoalQueues[1].AddTime(1)`
pub const EE_UNITS_AG_ADD_TIME: i32 = 27;
/// unit `a`'s `StartAction(c)`, then its class's `ActionChanged(a)`
pub const EE_UNITS_AG_ACT: i32 = 28;
/// unit `a`'s `QueueSquadronGoals(b, c & 1)` (`c & 2`) or `SetSquadronGoals(b, c & 1)`
pub const EE_UNITS_AG_SQUADRON: i32 = 29;
/// unit `a`'s `SetGatheredResource(NULL)`
pub const EE_UNITS_AG_CLEAR_GATHERED: i32 = 30;
/// unit `a`'s `mIsSpecialAttack = c`
pub const EE_UNITS_AG_SET_SPECIAL: i32 = 31;
/// unit `a`'s `mFormationSlot = c` (unless -1), `mFormationRank = d`
pub const EE_UNITS_AG_SET_PLACE: i32 = 32;
/// unit `a`'s `CanTake(b, c)`
pub const EE_UNITS_AG_CAN_TAKE: i32 = 33;
/// unit `a`'s `GetNearestFootprintTile(b, &io.a, &io.b)`
pub const EE_UNITS_AG_FOOTPRINT: i32 = 34;
/// `gWorld.mMap->GetTile(c, d) != NULL`
pub const EE_UNITS_AG_HAS_TILE: i32 = 35;
/// the tile at `io.p` (`UMFloorUnsigned`): 1 and its height in `io.f`, terrain in `io.a`; 0 for none
pub const EE_UNITS_AG_TILE_AT: i32 = 36;
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
/// unit `a`'s `GetPosition(io.p)`
pub const EE_UNITS_AG_POSITION: i32 = 43;
/// `EEGetAngleFromNorth(&io.p, &io.q)` into `io.f`
pub const EE_UNITS_AG_ANGLE: i32 = 44;
/// the units of tile (c, d) (pushed)
pub const EE_UNITS_AG_TILE_UNITS: i32 = 45;
/// unit `a`'s `IsPassableBy(b)`
pub const EE_UNITS_AG_PASSABLE: i32 = 46;
/// `EEIsSeenBy(a, player b, true)`
pub const EE_UNITS_AG_SEEN_BY: i32 = 47;
/// `EECanPriestConvert(a, b)`
pub const EE_UNITS_AG_PRIEST_CONVERT: i32 = 48;
/// unit `a`'s `GetPower() >= calamity b's GetCost() && node c's Contains(its type)`
pub const EE_UNITS_AG_CAN_CAST: i32 = 49;
/// unit `a`'s `Reference()`
pub const EE_UNITS_AG_REFERENCE: i32 = 50;
/// unit `a`'s `Dereference()`
pub const EE_UNITS_AG_DEREFERENCE: i32 = 51;
/// `U2DRectangle<float>` (top `io.p[0]`, left `io.p[1]`, bottom `io.p[2]`, right `io.q[0]`)'s
/// `GetClosestPoint((io.q[1], io.q[2]))` into `io.p[0]`, `io.p[1]`
pub const EE_UNITS_AG_RECT_CLOSEST: i32 = 52;
/// goal `a`'s `mKind` (-1: none)
pub const EE_UNITS_AG_GOAL_KIND: i32 = 60;
/// goal `a`'s `Clone()`
pub const EE_UNITS_AG_CLONE: i32 = 61;
/// `delete` goal `a`
pub const EE_UNITS_AG_DELETE: i32 = 62;
/// location goal `a`'s `GetB1C()` (`io.a`), `GetIsForcedMove()` (`io.b`), `GetShouldFace()` (`io.c`), `GetYaw()`
/// (`io.f`)
pub const EE_UNITS_AG_LOCATION_FIELDS: i32 = 63;
/// goal `a`'s `GetB1C()`
pub const EE_UNITS_AG_GOAL_B1C: i32 = 64;
/// goal `a`'s `GetGoalLocation(io.p)`
pub const EE_UNITS_AG_GOAL_LOCATION: i32 = 65;
/// `EEUGRust::NewLocation(&io.p, a & 1, a & 2, a & 4, io.f, a & 8, io.b)` (`a` is `io.a`)
pub const EE_UNITS_AG_NEW_LOCATION: i32 = 66;
/// `EEUGRust::NewUnloadTransport(a, c, d, true)`
pub const EE_UNITS_AG_NEW_UNLOAD: i32 = 67;
/// unload goal `a`'s `CanUnloadAtDestination()`
pub const EE_UNITS_AG_CAN_UNLOAD_DEST: i32 = 68;
/// unload goal `a`'s `GetUnloadTransport(&io.a, &io.b)`
pub const EE_UNITS_AG_UNLOAD_DEST: i32 = 69;
/// goal `a`'s `mB1D = true`
pub const EE_UNITS_AG_SET_B1D: i32 = 70;
/// location goal `a`'s `SetGroupPathState(0)`
pub const EE_UNITS_AG_CLEAR_PATH_STATE: i32 = 71;
/// gather goal `a`'s `GetGatherStorage()`
pub const EE_UNITS_AG_GATHER_STORAGE: i32 = 72;
/// gather goal `a`'s `GetGatherResource()`
pub const EE_UNITS_AG_GATHER_RESOURCE: i32 = 73;
/// gather goal `a`'s `SetGatherStorage(b)`
pub const EE_UNITS_AG_SET_GATHER_STORAGE: i32 = 74;
/// attack goal `a`'s `GetAttackTarget()`
pub const EE_UNITS_AG_ATTACK_TARGET: i32 = 75;
/// attack goal `a`'s `IsConvertAttack()`
pub const EE_UNITS_AG_IS_CONVERT_ATTACK: i32 = 76;
/// attack goal `a`'s `SetAttacker(NULL)`
pub const EE_UNITS_AG_CLEAR_ATTACKER: i32 = 77;
/// heal goal `a`'s `GetHealTarget()`
pub const EE_UNITS_AG_HEAL_TARGET: i32 = 78;
/// heal goal `a`'s `Suits(b)`
pub const EE_UNITS_AG_SUITS: i32 = 79;
/// explore goal `a`'s `SetExploreTile(c, d)`
pub const EE_UNITS_AG_SET_EXPLORE_TILE: i32 = 80;
/// patrol goal `a`'s `GetPatrolTarget()`, else `GetPatrolGroup()->GetLast()`
pub const EE_UNITS_AG_PATROL_POINT: i32 = 81;
/// `EEUGRust::NewAttackUnit(a, b, true, false)`
pub const EE_UNITS_AG_NEW_ATTACK_UNIT: i32 = 82;

extern "C" {
    fn ee_units_ag_host(op: i32, a: u64, b: u64, c: i64, d: i64, io: *mut EeUnitsAgIo) -> i64;
}

/// Pushes an item onto a host operation's list.
///
/// # Safety
/// `list` is the `EeUnitsAgIo::list` of a running operation that pushes; `p` is three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_units_ag_push(list: *mut EeUnitsAgListHandle, obj: u64, p: *const [f32; 3], w: f32) {
    // SAFETY: the caller's contract
    unsafe { (*list).items.push((obj, *p, w)) }
}

/// The game's world for the active group bodies.
pub struct AgGame;

impl AgGame {
    fn io(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, io: &mut EeUnitsAgIo) -> i64 {
        // SAFETY: the C++ switch reads live objects the bodies hand it
        unsafe { ee_units_ag_host(op, a, b, c, d, io) }
    }
    fn op(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64) -> i64 {
        let mut io = EeUnitsAgIo::default();
        self.io(op, a, b, c, d, &mut io)
    }
    fn list(&mut self, op: i32, a: u64, c: i64, d: i64, io: &mut EeUnitsAgIo) -> Vec<(u64, [f32; 3], f32)> {
        let mut handle = EeUnitsAgListHandle::default();
        io.list = std::ptr::addr_of_mut!(handle).cast();
        self.io(op, a, 0, c, d, io);
        handle.items
    }
}

impl AgHost for AgGame {
    fn group(&mut self, g: u64) -> EeUnitsAgGroup {
        let mut io = EeUnitsAgIo::default();
        self.io(EE_UNITS_AG_GROUP, g, 0, 0, 0, &mut io);
        io.group
    }
    fn group_unit(&mut self, g: u64, i: u32) -> u64 {
        self.op(EE_UNITS_AG_GROUP_UNIT, g, 0, i as i64, 0) as u64
    }
    fn remove(&mut self, g: u64, u: u64) {
        self.op(EE_UNITS_AG_REMOVE, g, u, 0, 0);
    }
    fn finish(&mut self, g: u64) {
        self.op(EE_UNITS_AG_FINISH, g, 0, 0, 0);
    }
    fn set_moves_together(&mut self, g: u64) {
        self.op(EE_UNITS_AG_MOVES_TOGETHER, g, 0, 0, 0);
    }
    fn request_all_paths(&mut self, g: u64) {
        self.op(EE_UNITS_AG_REQUEST_ALL, g, 0, 0, 0);
    }
    fn request_paths(&mut self, g: u64, units: &[u64]) {
        let mut io = EeUnitsAgIo { list: units.as_ptr() as *mut c_void, ..Default::default() };
        self.io(EE_UNITS_AG_REQUEST_PATHS, g, 0, units.len() as i64, 0, &mut io);
    }
    fn send_command(&mut self, g: u64, target: u64, command: u32) {
        self.op(EE_UNITS_AG_SEND, g, target, command as i64, 0);
    }
    fn arrange(&mut self, g: u64, center: [f32; 3], yaw: Option<f32>) -> Vec<Slot> {
        let mut io = EeUnitsAgIo { p: center, f: yaw.unwrap_or(0.0), ..Default::default() };
        let items = self.list(EE_UNITS_AG_ARRANGE, g, yaw.is_some() as i64, 0, &mut io);
        items.into_iter().map(|(unit, position, heading)| Slot { position, unit, heading }).collect()
    }
    fn new_group(&mut self, g: u64) -> u64 {
        self.op(EE_UNITS_AG_NEW_GROUP, g, 0, 0, 0) as u64
    }
    fn add(&mut self, g: u64, u: u64) {
        self.op(EE_UNITS_AG_ADD, g, u, 0, 0);
    }
    fn set_formation(&mut self, ng: u64, g: u64) {
        self.op(EE_UNITS_AG_SET_FORMATION, ng, g, 0, 0);
    }
    #[allow(clippy::too_many_arguments)] // as the trait's
    fn launch(&mut self, g: u64, ng: u64, goal: u64, queued: bool, action: i32, target: u64, command: u32) {
        let mut io =
            EeUnitsAgIo { a: goal as i64, b: target as i64, c: action as i64, d: command as i64, ..Default::default() };
        self.io(EE_UNITS_AG_LAUNCH, g, ng, queued as i64, 0, &mut io);
    }
    fn destinations(&mut self, g: u64) -> Vec<(u32, [f32; 3])> {
        let mut io = EeUnitsAgIo::default();
        self.list(EE_UNITS_AG_DESTINATIONS, g, 0, 0, &mut io).into_iter().map(|(id, p, _)| (id as u32, p)).collect()
    }
    fn world_unit_sphere(&mut self, id: u32) -> Option<f32> {
        let mut io = EeUnitsAgIo::default();
        (self.io(EE_UNITS_AG_WORLD_SPHERE, 0, 0, id as i64, 0, &mut io) != 0).then_some(io.f)
    }
    fn regions_size(&mut self) -> i32 {
        self.op(EE_UNITS_AG_REGIONS_SIZE, 0, 0, 0, 0) as i32
    }
    fn cap_factor(&mut self) -> f32 {
        let mut io = EeUnitsAgIo::default();
        self.io(EE_UNITS_AG_CAP_FACTOR, 0, 0, 0, 0, &mut io);
        io.f
    }
    fn set_values(&mut self, g: u64, values: &[f32; 17]) {
        let mut io = EeUnitsAgIo { list: values.as_ptr() as *mut c_void, ..Default::default() };
        self.io(EE_UNITS_AG_SET_VALUES, g, 0, 0, 0, &mut io);
    }
    fn unit(&mut self, u: u64) -> EeUnitsAgUnit {
        let mut io = EeUnitsAgIo::default();
        self.io(EE_UNITS_AG_UNIT, u, 0, 0, 0, &mut io);
        io.unit
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
        self.op(EE_UNITS_AG_QUEUE_ADD, u, goal, q as i64, 0);
    }
    fn add_time(&mut self, u: u64) {
        self.op(EE_UNITS_AG_ADD_TIME, u, 0, 0, 0);
    }
    fn act(&mut self, u: u64, action: i32) {
        self.op(EE_UNITS_AG_ACT, u, 0, action as i64, 0);
    }
    fn squadron(&mut self, u: u64, goal: u64, flag: bool, queued: bool) {
        self.op(EE_UNITS_AG_SQUADRON, u, goal, flag as i64 | (queued as i64) << 1, 0);
    }
    fn clear_gathered(&mut self, u: u64) {
        self.op(EE_UNITS_AG_CLEAR_GATHERED, u, 0, 0, 0);
    }
    fn set_special_attack(&mut self, u: u64, v: bool) {
        self.op(EE_UNITS_AG_SET_SPECIAL, u, 0, v as i64, 0);
    }
    fn set_formation_place(&mut self, u: u64, slot: Option<u32>, rank: i32) {
        self.op(EE_UNITS_AG_SET_PLACE, u, 0, slot.map_or(-1, |s| s as i64), rank as i64);
    }
    fn can_take(&mut self, storage: u64, u: u64, resource_type: u32) -> bool {
        self.op(EE_UNITS_AG_CAN_TAKE, storage, u, resource_type as i64, 0) != 0
    }
    fn nearest_footprint(&mut self, u: u64, target: u64, x: i32, y: i32) -> (i32, i32) {
        let mut io = EeUnitsAgIo { a: x as i64, b: y as i64, ..Default::default() };
        self.io(EE_UNITS_AG_FOOTPRINT, u, target, 0, 0, &mut io);
        (io.a as i32, io.b as i32)
    }
    fn has_tile(&mut self, x: i32, y: i32) -> bool {
        self.op(EE_UNITS_AG_HAS_TILE, 0, 0, x as i64, y as i64) != 0
    }
    fn tile_at(&mut self, p: [f32; 3]) -> Option<(f32, i32)> {
        let mut io = EeUnitsAgIo { p, ..Default::default() };
        (self.io(EE_UNITS_AG_TILE_AT, 0, 0, 0, 0, &mut io) != 0).then_some((io.f, io.a as i32))
    }
    fn can_get_to(&mut self, u: u64, x: i32, y: i32) -> bool {
        self.op(EE_UNITS_AG_CAN_GET_TO, u, 0, x as i64, y as i64) != 0
    }
    fn can_convert(&mut self, u: u64, target: u64, flag: &mut bool) -> bool {
        let mut io = EeUnitsAgIo { a: *flag as i64, ..Default::default() };
        let result = self.io(EE_UNITS_AG_CAN_CONVERT, u, target, 0, 0, &mut io) != 0;
        *flag = io.a != 0;
        result
    }
    fn can_attack_class(&mut self, u: u64, class: u64) -> bool {
        self.op(EE_UNITS_AG_CAN_ATTACK_CLASS, u, class, 0, 0) != 0
    }
    fn has_action(&mut self, class: u64, action: i32) -> bool {
        self.op(EE_UNITS_AG_HAS_ACTION, class, 0, action as i64, 0) != 0
    }
    fn command_type(&mut self, u: u64) -> i32 {
        self.op(EE_UNITS_AG_COMMAND_TYPE, u, 0, 0, 0) as i32
    }
    fn num_cargo(&mut self, u: u64) -> u32 {
        self.op(EE_UNITS_AG_NUM_CARGO, u, 0, 0, 0) as u32
    }
    fn position(&mut self, u: u64) -> [f32; 3] {
        let mut io = EeUnitsAgIo::default();
        self.io(EE_UNITS_AG_POSITION, u, 0, 0, 0, &mut io);
        io.p
    }
    fn angle_from_north(&mut self, from: [f32; 3], to: [f32; 3]) -> f32 {
        let mut io = EeUnitsAgIo { p: from, q: to, ..Default::default() };
        self.io(EE_UNITS_AG_ANGLE, 0, 0, 0, 0, &mut io);
        io.f
    }
    fn tile_units(&mut self, x: i32, y: i32) -> Vec<u64> {
        let mut io = EeUnitsAgIo::default();
        self.list(EE_UNITS_AG_TILE_UNITS, 0, x as i64, y as i64, &mut io).into_iter().map(|(u, _, _)| u).collect()
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
        self.op(EE_UNITS_AG_CAN_CAST, u, calamity, node as i64, 0) != 0
    }
    fn reference(&mut self, u: u64) {
        self.op(EE_UNITS_AG_REFERENCE, u, 0, 0, 0);
    }
    fn dereference(&mut self, u: u64) {
        self.op(EE_UNITS_AG_DEREFERENCE, u, 0, 0, 0);
    }
    fn rect_closest(&mut self, r: Rect, p: [f32; 2]) -> [f32; 2] {
        let mut io = EeUnitsAgIo { p: [r.top, r.left, r.bottom], q: [r.right, p[0], p[1]], ..Default::default() };
        self.io(EE_UNITS_AG_RECT_CLOSEST, 0, 0, 0, 0, &mut io);
        [io.p[0], io.p[1]]
    }
    fn goal_kind(&mut self, goal: u64) -> i32 {
        self.op(EE_UNITS_AG_GOAL_KIND, goal, 0, 0, 0) as i32
    }
    fn clone_goal(&mut self, goal: u64) -> u64 {
        self.op(EE_UNITS_AG_CLONE, goal, 0, 0, 0) as u64
    }
    fn delete_goal(&mut self, goal: u64) {
        self.op(EE_UNITS_AG_DELETE, goal, 0, 0, 0);
    }
    fn location_fields(&mut self, goal: u64) -> LocationFields {
        let mut io = EeUnitsAgIo::default();
        self.io(EE_UNITS_AG_LOCATION_FIELDS, goal, 0, 0, 0, &mut io);
        LocationFields { b1c: io.a != 0, forced: io.b != 0, should_face: io.c != 0, yaw: io.f }
    }
    fn goal_b1c(&mut self, goal: u64) -> bool {
        self.op(EE_UNITS_AG_GOAL_B1C, goal, 0, 0, 0) != 0
    }
    fn goal_location(&mut self, goal: u64) -> [f32; 3] {
        let mut io = EeUnitsAgIo::default();
        self.io(EE_UNITS_AG_GOAL_LOCATION, goal, 0, 0, 0, &mut io);
        io.p
    }
    fn new_location(&mut self, l: NewLocation) -> u64 {
        let flags = l.b1c as i64 | (l.forced as i64) << 1 | (l.face as i64) << 2 | (l.auto_waypoint as i64) << 3;
        let mut io = EeUnitsAgIo { a: flags, b: l.group_state as i64, p: l.point, f: l.yaw, ..Default::default() };
        self.io(EE_UNITS_AG_NEW_LOCATION, 0, 0, 0, 0, &mut io) as u64
    }
    fn new_unload(&mut self, u: u64, x: i32, y: i32) -> u64 {
        self.op(EE_UNITS_AG_NEW_UNLOAD, u, 0, x as i64, y as i64) as u64
    }
    fn can_unload_at_destination(&mut self, goal: u64) -> bool {
        self.op(EE_UNITS_AG_CAN_UNLOAD_DEST, goal, 0, 0, 0) != 0
    }
    fn unload_destination(&mut self, goal: u64) -> (i32, i32) {
        let mut io = EeUnitsAgIo::default();
        self.io(EE_UNITS_AG_UNLOAD_DEST, goal, 0, 0, 0, &mut io);
        (io.a as i32, io.b as i32)
    }
    fn set_b1d(&mut self, goal: u64) {
        self.op(EE_UNITS_AG_SET_B1D, goal, 0, 0, 0);
    }
    fn clear_group_path_state(&mut self, goal: u64) {
        self.op(EE_UNITS_AG_CLEAR_PATH_STATE, goal, 0, 0, 0);
    }
    fn gather_storage(&mut self, goal: u64) -> u64 {
        self.op(EE_UNITS_AG_GATHER_STORAGE, goal, 0, 0, 0) as u64
    }
    fn gather_resource(&mut self, goal: u64) -> u64 {
        self.op(EE_UNITS_AG_GATHER_RESOURCE, goal, 0, 0, 0) as u64
    }
    fn set_gather_storage(&mut self, goal: u64, storage: u64) {
        self.op(EE_UNITS_AG_SET_GATHER_STORAGE, goal, storage, 0, 0);
    }
    fn attack_target(&mut self, goal: u64) -> u64 {
        self.op(EE_UNITS_AG_ATTACK_TARGET, goal, 0, 0, 0) as u64
    }
    fn is_convert_attack(&mut self, goal: u64) -> bool {
        self.op(EE_UNITS_AG_IS_CONVERT_ATTACK, goal, 0, 0, 0) != 0
    }
    fn clear_attacker(&mut self, goal: u64) {
        self.op(EE_UNITS_AG_CLEAR_ATTACKER, goal, 0, 0, 0);
    }
    fn heal_target(&mut self, goal: u64) -> u64 {
        self.op(EE_UNITS_AG_HEAL_TARGET, goal, 0, 0, 0) as u64
    }
    fn suits(&mut self, goal: u64, u: u64) -> bool {
        self.op(EE_UNITS_AG_SUITS, goal, u, 0, 0) != 0
    }
    fn set_explore_tile(&mut self, goal: u64, x: i32, y: i32) {
        self.op(EE_UNITS_AG_SET_EXPLORE_TILE, goal, 0, x as i64, y as i64);
    }
    fn patrol_point(&mut self, goal: u64) -> u64 {
        self.op(EE_UNITS_AG_PATROL_POINT, goal, 0, 0, 0) as u64
    }
    fn new_attack_unit(&mut self, first: u64, target: u64) -> u64 {
        self.op(EE_UNITS_AG_NEW_ATTACK_UNIT, first, target, 0, 0) as u64
    }
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
