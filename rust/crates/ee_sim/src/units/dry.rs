//! TEST ONLY: the dry host (the units' lane, round 2; it was C++, `EEUnitHost.cpp`'s `sDry` branches). Between
//! `ee_units_dry_begin` and `ee_units_dry_end` every group command body (`ee_units_explore` ... `ee_units_cast_calamity`)
//! runs over `DryHost` instead of the game's host: every call logs what it would do and answers without changing the
//! world (no group, goal, draw, order or effect: groups are plain unit lists, a goal a record of its arguments, a draw a
//! counter), so a body's decisions over the live world can be recorded at any point of a game without changing it. The
//! reads (the map's height, the nearest town's patrol point, a route, a unit's type) are the game host's; Fill takes the
//! selection's units by the store's test (`group::fill_members`), the keeps test the objects as the originals did (the
//! store's keep tests are compared with the objects': a disagreement counts as a mismatch); the spreads keep the group
//! as it is (logged). The group command sweep (group_sweep.rs) writes the log as its USGC record, the gate
//! of the bodies (tools/sim_capture.py), so every word below is the C++ dry host's, in its order.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::units::host::{EeGoalSpec, GoalHandle, Host, HostRef};
use crate::units::host::{
    EE_GOAL_CTOR_ATTACK_UNIT, EE_GOAL_CTOR_BUILD_REPAIR, EE_GOAL_CTOR_CAST_CALAMITY, EE_GOAL_CTOR_HEAL_UNIT,
    EE_GOAL_CTOR_LOCATION, EE_GOAL_CTOR_UNLOAD_TRANSPORT, EE_UNIT_ID_NONE,
};

// the log's events (the words after each code are its values)
const NEW_GROUP: u32 = 1; // tag
const FILL: u32 = 2; // tag, selection tag, sort, slot, the count, the ids
const RELEASE: u32 = 3; // tag
const PENDING: u32 = 4; // player id, tag
const KEEP_MOBILE: u32 = 5; // tag, the count, the ids kept
const KEEP_GROUND: u32 = 6; // tag, the count, the ids kept
const CLEAR_GOAL: u32 = 7; // tag
const LOG_GOAL_NEW: u32 = 8; // ctor, unit a, unit b, the point's three bits, flag a, flag b, the route (2 words), made
const LOG_GOAL_RELEASE: u32 = 9; // ctor
const SET_GOAL: u32 = 10; // tag, ctor, queued
const SET_ACTION: u32 = 11; // tag, action
const PLAN: u32 = 12; // tag
const RANDOM: u32 = 13; // low, high, the answer
const ACKNOWLEDGE: u32 = 14; // player id, tag, index, the unit at it
const MAP_HEIGHT: u32 = 15; // x, y, the height (bits)
const SEND_COMMAND: u32 = 16; // tag, target, player id, command, queue
const ACKNOWLEDGE_UNIT: u32 = 17; // player id, unit
const TOWN_POINT: u32 = 18; // unit, found, the point
const ADD_UNIT_TILE: u32 = 19; // unit, target, player id, the target's tile x, y
const ROUTE: u32 = 20; // unit, the route (2 words)
                       // 21: a unit's type (never logged)
const QUEUED: u32 = 22; // tag, queued
const SPREAD_RESOURCE: u32 = 23; // tag, target
const SPREAD_TILE: u32 = 24; // tag, x, y
const ADD_BUILD_TARGET: u32 = 25; // unit, target
const ORDER_REPAIR: u32 = 26; // unit, target
const ADD_MEMBER: u32 = 27; // tag, unit
const KEEP_NON_CITIZENS: u32 = 28; // tag, the count, the ids kept
const LONE_PATROL: u32 = 29; // player id, x, y
const INSERT_POINT: u32 = 30; // unit, player id, x, y
const LOG_GOAL_ROUTE: u32 = 31; // unit, the route (2 words)
const ADD_POINT: u32 = 32; // the route (2 words), player id, x, y
const LOG_GOAL_ADD_POINT: u32 = 33; // ctor, player id, x, y
const SPREAD_UNIT: u32 = 34; // tag, target, x, y
const COMPLEX_OF: u32 = 35; // unit, its complex unit
const PATH_POINT: u32 = 36; // unit, x, y in, x, y out, the height (bits)
const RESET_PATHS: u32 = 37; // tag
const KEEP_CITIZENS: u32 = 38; // tag, the count, the ids kept
const WALL_PLAN: u32 = 39; // player id, technology, the start's and end's bits (the dry plan makes 2 pieces)
const WALL_BUILD: u32 = 40; // tag
const WALL_RELEASE: u32 = 41; // -
const SPREAD_UNLOAD: u32 = 42; // tag, x, y
const CAN_UNLOAD: u32 = 43; // unit, the answer
const SET_TRANSPORT: u32 = 44; // ctor, unit
const CAN_UNLOAD_THERE: u32 = 45; // ctor (the dry answer: yes)
const UNLOAD_WARNING: u32 = 46; // player id, x, y
const TECH_CLASS: u32 = 47; // player id, technology, found, the class
const CAPITOLS: u32 = 48; // player id, the count
const CAN_PRODUCE: u32 = 49; // player id, technology (the dry answer: yes, not asked)
const CAN_PLACE: u32 = 50; // player id, unit type, x, y, the real answer (the dry answer: yes)
const MAKE_BUILDING: u32 = 51; // player id, unit type, x, y (the dry building: DRY_BUILDING)
const FREE_FARMS: u32 = 52; // unit, the count, the ids (a unit the dry host does not know: two dry farms)
const SORT_FARMS: u32 = 53; // x, y, greater, the count, the ids before, the ids after
const ADD_FARM: u32 = 54; // unit, target
const ORDER_BUILD: u32 = 55; // unit, target
const START_ACTION: u32 = 56; // unit, action
const LOAD_FARM: u32 = 57; // unit, the farm
const DROP_SITES: u32 = 58; // farm, found, the count, the ids
const CAN_TAKE: u32 = 59; // target, unit, the answer
const DROP_GATHERED: u32 = 60; // unit
const WORK_FARM: u32 = 61; // farm, unit
const UNIT_SET_GOAL: u32 = 62; // unit, ctor
const FINISH: u32 = 63; // tag
const CALAMITY_OF: u32 = 64; // player id, technology, found, class enum, cast by all
const CAN_CAST: u32 = 65; // unit, x, y, target (the dry answer: 0, it can; not asked)
const CONTAINS: u32 = 66; // player id, technology, unit type, the answer
const POWER: u32 = 67; // unit, its power
const COST: u32 = 68; // the cost
const UNIT_PLAYER: u32 = 69; // unit, its player's id
const SPLIT: u32 = 70; // tag, technology, target, the count, the ids kept
const CLONE: u32 = 71; // ctor
const REMOVE_UNIT: u32 = 72; // tag, unit
const CLEAR_GOALS: u32 = 73; // unit
const PLAYER_SELECTION: u32 = 74; // player id
const CLEAR_QUEUE: u32 = 75; // unit, queue
const CLEAR_ALL_QUEUES: u32 = 76; // unit
const QUEUE_GOAL: u32 = 77; // unit, ctor, queue
const QUEUE_TIMER: u32 = 78; // unit, queue, ms
const LOG_GOAL_STATE_CLONE: u32 = 79; // unit, its goal's kind
const GARRISON_LOAD: u32 = 80; // garrison (the dry answer: valid)
const QUEUE_CLEAR: u32 = 81; // unit, queue
const CLEAR_AIR_TRAFFIC: u32 = 82; // unit, its air traffic type
const REFRESH_GOALS: u32 = 83; // unit
const CLEAR_BUILD_TARGETS: u32 = 84; // unit
const LOG_ACTION_CHANGED: u32 = 85; // unit
const HALT: u32 = 86; // unit
const LOAD_GOAL: u32 = 87; // unit, kind, found
const ADD_LOAD_UNITS: u32 = 88; // ctor, the count, the ids
const NEW_LOAD: u32 = 89; // kind, carrier, flag, the count, the ids
const LOAD_FIELDS: u32 = 90; // ctor, valid, the tile x, y
const EVENT: u32 = 91; // player id, x, y, code
const ADD_UNIT: u32 = 92; // tag, unit
const FILTER: u32 = 93; // transport, the count kept, the ids, the count rejected, the ids
const QUEUE_RESEARCH: u32 = 94; // unit, technology, front
const HACK_UNIT: u32 = 95; // unit type, x, y (bits), player id
const HACK_BUILDING: u32 = 96; // unit type, x, y, player id

/// The dry goals made without a goal spec: a unit's active goal's clone, the garrison's load goal; a carrier's load
/// goal (the real one, read through) and a load goal the dry host made.
const CTOR_GOAL_STATE: u32 = 0x101;
const CTOR_GARRISON_LOAD: u32 = 0x102;
const CTOR_OLD_LOAD: u32 = 0x103;
const CTOR_NEW_LOAD: u32 = 0x104;
/// `kGoalLoadGarrison`
const GOAL_LOAD_GARRISON: u8 = 12;
/// The dry building (`EEMakeBuilding` makes none) and the two dry farms (ids no unit has).
const DRY_BUILDING: u32 = 0x7e00_0002;
const DRY_FARMS: [u32; 2] = [0x7e00_0004, 0x7e00_0003];
/// `kUnitTypeAirport`, `kUnitCategoryLandCitizen`, `kUnitCategoryNavalCitizen`
const UNIT_TYPE_AIRPORT: i64 = 0x42;
const CATEGORY_LAND_CITIZEN: i64 = 6;
const CATEGORY_NAVAL_CITIZEN: i64 = 0x16;

#[derive(Clone, Copy)]
struct DryGoal {
    ctor: u32,
    kind: u8,
    /// a load goal's carrier
    unit: u32,
    /// a carrier's real load goal (its reads are made on it; null: none)
    real: *mut c_void,
}

#[derive(Default)]
struct DryState {
    /// the dry groups' units (ids; EE_UNIT_ID_NONE for a unit no id resolves), by tag (0: the sweep's selection)
    groups: Vec<Vec<u32>>,
    goals: Vec<DryGoal>,
    log: Vec<u32>,
    mismatches: u32,
    draws: u32,
}

thread_local! {
    /// The dry state of this thread (the sweep's); None: the game's host.
    static DRY: RefCell<Option<DryState>> = const { RefCell::new(None) };
}

/// Whether the bodies of this thread run over the dry host.
pub fn active() -> bool {
    DRY.with(|d| d.borrow().is_some())
}

fn with<R>(f: impl FnOnce(&mut DryState) -> R) -> R {
    DRY.with(|d| f(d.borrow_mut().as_mut().expect("the dry host is on")))
}

fn log(words: &[u32]) {
    with(|s| s.log.extend_from_slice(words));
}

fn log_ids(ids: &[u32]) {
    with(|s| {
        s.log.push(ids.len() as u32);
        s.log.extend_from_slice(ids);
    });
}

fn bits(v: f32) -> u32 {
    v.to_bits()
}

/// A group as the bodies hold it: its tag + 1 (never dereferenced: every group call is the dry host's).
fn group_ref(tag: usize) -> HostRef {
    HostRef((tag + 1) as *mut c_void)
}

fn tag(group: HostRef) -> usize {
    group.0 as usize - 1
}

fn group_units(group: HostRef) -> Vec<u32> {
    with(|s| s.groups[tag(group)].clone())
}

fn set_group_units(group: HostRef, units: Vec<u32>) {
    with(|s| s.groups[tag(group)] = units);
}

fn log_group(code: u32, group: HostRef) {
    let units = group_units(group);
    log(&[code, tag(group) as u32]);
    log_ids(&units);
}

/// `EEGroup::RemoveUnit` over a dry group: its first place.
fn remove_first(units: &mut Vec<u32>, unit: u32) {
    if let Some(i) = units.iter().position(|&u| u == unit) {
        units.remove(i);
    }
}

fn new_goal(goal: DryGoal) -> *mut c_void {
    with(|s| {
        s.goals.push(goal);
        s.goals.len() as *mut c_void
    })
}

fn goal_of(goal: *mut c_void) -> DryGoal {
    with(|s| s.goals[goal as usize - 1])
}

fn handle(goal: DryGoal) -> GoalHandle {
    GoalHandle::from_ptr(new_goal(goal)).expect("a dry goal's handle")
}

/// The unit an id names (`EEUnitHandleResolve`), its id kept (EE_UNIT_ID_NONE when none resolves).
fn resolved(unit: u32) -> u32 {
    if unit != EE_UNIT_ID_NONE && !crate::units::ffi::ee_units_resolve(unit).is_null() {
        unit
    } else {
        EE_UNIT_ID_NONE
    }
}

#[cfg(not(test))]
mod reads {
    //! The objects' reads the dry host makes itself (the C++ dry branches' member reads), over the world views.
    use crate::world::{PlayerView, UnitView, WorldRead};

    pub(super) fn unit<R>(id: u32, f: impl FnOnce(&UnitView) -> R) -> Option<R> {
        let object = crate::units::ffi::ee_units_resolve(id) as u64;
        // SAFETY: a body runs inside the C++ call that holds the world; a registered unit is live
        let t = unsafe { WorldRead::begin() };
        // SAFETY: as above
        let u = unsafe { UnitView::of(&t, object) }?;
        Some(f(&u))
    }

    /// `PlayerID(player)`: `mPlayerID`
    pub(super) fn player_id(player: *mut std::ffi::c_void) -> u32 {
        // SAFETY: as above; the bodies' players are live
        let t = unsafe { WorldRead::begin() };
        // SAFETY: as above
        unsafe { PlayerView::of(&t, player as u64) }.map_or(0, |p| p.id())
    }

    /// `mIsCommandable && mHitPoints > 0`
    pub(super) fn live_commandable(id: u32) -> bool {
        unit(id, |u| u.commandable() && u.hit_points() > 0).unwrap_or(false)
    }

    /// The unit's class's category, type, `mCanAttackGround`, `mHasSpecialAttack` (-1, -1, false, false: none).
    pub(super) fn class(id: u32) -> (i64, i64, bool, bool) {
        unit(id, |u| {
            u.class().map_or((-1, -1, false, false), |c| {
                (
                    c.category() as i64,
                    c.unit_type() as i64,
                    crate::world_field!(c.fields(), mCanAttackGround),
                    crate::world_field!(c.fields(), mHasSpecialAttack),
                )
            })
        })
        .unwrap_or((-1, -1, false, false))
    }

    /// `mStats->GetMaxVelocity()`
    pub(super) fn max_velocity(id: u32) -> f32 {
        unit(id, |u| u.stats().map_or(0.0, |s| crate::world_field!(s.fields(), mMaxVelocity))).unwrap_or(0.0)
    }

    /// `mGoalState`'s `mKind` (0: none)
    pub(super) fn goal_state_kind(id: u32) -> u8 {
        unit(id, |u| u.goal().map_or(0, |(kind, _)| kind)).unwrap_or(0)
    }

    /// `GetTileX()`, `GetTileY()`
    pub(super) fn tile(id: u32) -> (i64, i64) {
        unit(id, |u| u.tile_xy()).unwrap_or((0, 0))
    }

    /// A route's two words: its points' count + 1 (0: none) and its first point's id (heap addresses differ between
    /// runs).
    pub(super) fn route_words(route: *mut std::ffi::c_void) -> [u32; 2] {
        if route.is_null() {
            return [0, crate::units::host::EE_UNIT_ID_NONE];
        }
        let points = crate::units::patrol_group::host_points(route as u64);
        let first = points.first().copied().filter(|&p| p != 0).map_or(crate::units::host::EE_UNIT_ID_NONE, |p| {
            // SAFETY: as above; a route's points are live units
            let t = unsafe { WorldRead::begin() };
            // SAFETY: as above
            unsafe { UnitView::of(&t, p) }.map_or(crate::units::host::EE_UNIT_ID_NONE, |u| u.id())
        });
        [points.len() as u32 + 1, first]
    }
}

#[cfg(test)]
mod reads {
    //! The unit tests' objects: none (the dry host's logic is tested over the store's records alone).
    pub(super) fn player_id(player: *mut std::ffi::c_void) -> u32 {
        player as usize as u32
    }
    pub(super) fn live_commandable(_id: u32) -> bool {
        true
    }
    pub(super) fn class(_id: u32) -> (i64, i64, bool, bool) {
        (1, 1, true, true)
    }
    pub(super) fn max_velocity(_id: u32) -> f32 {
        1.0
    }
    pub(super) fn goal_state_kind(_id: u32) -> u8 {
        0
    }
    pub(super) fn tile(id: u32) -> (i64, i64) {
        (i64::from(id), i64::from(id) + 1)
    }
    pub(super) fn route_words(route: *mut std::ffi::c_void) -> [u32; 2] {
        if route.is_null() {
            [0, crate::units::host::EE_UNIT_ID_NONE]
        } else {
            [1, crate::units::host::EE_UNIT_ID_NONE]
        }
    }
}

fn pid(player: HostRef) -> u32 {
    reads::player_id(player.0)
}

fn log_route(route: HostRef) {
    log(&reads::route_words(route.0));
}

/// Begins a dry run: the selection (group 0) holds `units` (ids); its reference is what the bodies take.
pub fn begin(units: &[u32]) -> HostRef {
    DRY.with(|d| *d.borrow_mut() = Some(DryState { groups: vec![units.to_vec()], ..DryState::default() }));
    group_ref(0)
}

/// Ends the dry run: its log and the keeps' disagreements (the store's test against the object's).
pub fn end() -> (Vec<u32>, u32) {
    DRY.with(|d| d.borrow_mut().take()).map_or((Vec::new(), 0), |s| (s.log, s.mismatches))
}

/// A keep over the group: `keeps` is the store's test, `object` the object's (a disagreement counts).
fn keep(group: HostRef, code: u32, keeps: impl Fn(u32) -> bool, object: impl Fn(u32) -> bool) {
    let mut units = group_units(group);
    let mut i = 0;
    while i < units.len() {
        let u = units[i];
        let k = keeps(u);
        if k != object(u) {
            with(|s| s.mismatches += 1);
        }
        if k {
            i += 1;
        } else {
            remove_first(&mut units, u);
        }
    }
    set_group_units(group, units);
    log_group(code, group);
}

/// The dry host over the game's (its reads); a zero-sized handle of this thread's dry state.
pub struct DryHost<G: Host> {
    pub game: G,
}

impl<G: Host> Host for DryHost<G> {
    fn unit(&mut self, id: u32) -> Option<crate::units::store::EeUnitRecord> {
        self.game.unit(id)
    }
    fn group_ids(&mut self, group: HostRef, out: &mut Vec<u32>) {
        *out = group_units(group);
    }
    fn goal_new(&mut self, spec: &EeGoalSpec) -> Option<GoalHandle> {
        let kind = spec.kind();
        let made = kind.is_some()
            && match spec.ctor {
                EE_GOAL_CTOR_ATTACK_UNIT => resolved(spec.unit_b) != EE_UNIT_ID_NONE,
                EE_GOAL_CTOR_HEAL_UNIT
                | EE_GOAL_CTOR_CAST_CALAMITY
                | EE_GOAL_CTOR_UNLOAD_TRANSPORT
                | EE_GOAL_CTOR_BUILD_REPAIR => resolved(spec.unit_a) != EE_UNIT_ID_NONE,
                _ => true,
            };
        log(&[
            LOG_GOAL_NEW,
            spec.ctor,
            spec.unit_a,
            spec.unit_b,
            bits(spec.point[0]),
            bits(spec.point[1]),
            bits(spec.point[2]),
            u32::from(spec.flag_a),
            u32::from(spec.flag_b),
        ]);
        if spec.ctor == EE_GOAL_CTOR_LOCATION {
            log(&[bits(spec.value), u32::from(spec.flag_c)]);
        }
        if spec.ctor == EE_GOAL_CTOR_CAST_CALAMITY {
            log(&[spec.ints[0] as u32, spec.ints[1] as u32, spec.ints[2] as u32]);
        }
        log_route(HostRef(spec.object));
        log(&[u32::from(made)]);
        made.then(|| handle(DryGoal { ctor: spec.ctor, kind: kind.unwrap_or(0), unit: 0, real: std::ptr::null_mut() }))
    }
    fn goal_kind(&mut self, goal: &GoalHandle) -> u8 {
        goal_of(goal.as_ptr()).kind
    }
    fn goal_release(&mut self, goal: GoalHandle) {
        log(&[LOG_GOAL_RELEASE, goal_of(goal.as_ptr()).ctor]);
    }
    fn active_group_new(&mut self, _player: HostRef) -> HostRef {
        let t = with(|s| {
            s.groups.push(Vec::new());
            s.groups.len() - 1
        });
        log(&[NEW_GROUP, t as u32]);
        group_ref(t)
    }
    fn group_fill(&mut self, group: HostRef, selection: HostRef, sort: i32, slot: i32) {
        // EEActiveGroup::Fill: the units the store's test takes, in the selection's order (the dry host sorts neither
        // the selection nor the group: the sort is logged)
        let from = group_units(selection);
        let mut taken = vec![0u32; from.len() + 1];
        // SAFETY: `from` holds its ids and `taken` room for them
        let n = unsafe {
            crate::units::ffi::ee_units_fill_members(from.as_ptr(), from.len(), i64::from(slot), taken.as_mut_ptr())
        };
        set_group_units(group, taken[..n].iter().map(|&k| from[k as usize]).collect());
        log(&[FILL, tag(group) as u32, tag(selection) as u32, sort as u32, slot as u32]);
        log_ids(&group_units(group));
    }
    fn group_release(&mut self, group: HostRef) {
        log(&[RELEASE, tag(group) as u32]);
    }
    fn pending_push(&mut self, player: HostRef, group: HostRef) {
        log(&[PENDING, pid(player), tag(group) as u32]);
    }
    fn group_keep_mobile(&mut self, group: HostRef) {
        // EEActiveGroup::KeepMobileUnits' loop and test over the objects
        let mut units = group_units(group);
        let mut i = 0;
        while i < units.len() {
            let u = units[i];
            if reads::live_commandable(u) && (reads::max_velocity(u) != 0.0 || reads::class(u).1 == UNIT_TYPE_AIRPORT) {
                i += 1;
            } else {
                remove_first(&mut units, u);
            }
        }
        set_group_units(group, units);
        log_group(KEEP_MOBILE, group);
    }
    fn group_clear_goal(&mut self, group: HostRef) {
        log(&[CLEAR_GOAL, tag(group) as u32]);
    }
    fn group_set_goal(&mut self, group: HostRef, goal: GoalHandle, queued: bool) {
        log(&[SET_GOAL, tag(group) as u32, goal_of(goal.as_ptr()).ctor, u32::from(queued)]);
    }
    fn group_set_action(&mut self, group: HostRef, action: i32) {
        log(&[SET_ACTION, tag(group) as u32, action as u32]);
    }
    fn group_plan(&mut self, group: HostRef) {
        log(&[PLAN, tag(group) as u32]);
    }
    fn random(&mut self, low: i32, high: i32) -> i32 {
        // no draw: the answers walk the range
        let answer = if high >= low {
            let draw = with(|s| {
                let d = s.draws;
                s.draws = s.draws.wrapping_add(1);
                d
            });
            low.wrapping_add((draw % (high.wrapping_sub(low).wrapping_add(1) as u32)) as i32)
        } else {
            low
        };
        log(&[RANDOM, low as u32, high as u32, answer as u32]);
        answer
    }
    fn play_acknowledgement(&mut self, player: HostRef, group: HostRef, index: i32) {
        let units = group_units(group);
        let unit = usize::try_from(index).ok().and_then(|i| units.get(i).copied()).unwrap_or(EE_UNIT_ID_NONE);
        log(&[ACKNOWLEDGE, pid(player), tag(group) as u32, index as u32, unit]);
    }
    fn map_height(&mut self, x: f32, y: f32) -> f32 {
        let h = self.game.map_height(x, y);
        log(&[MAP_HEIGHT, bits(x), bits(y), bits(h)]);
        h
    }
    fn group_keep_ground_attackers(&mut self, group: HostRef) {
        keep(group, KEEP_GROUND, |u| crate::units::ffi::ee_units_keeps_ground_attacker(u), |u| reads::class(u).2);
    }
    fn send_group_command(&mut self, group: HostRef, target: u32, player: HostRef, command: i32, queue: i32) {
        log(&[SEND_COMMAND, tag(group) as u32, target, pid(player), command as u32, queue as u32]);
    }
    fn acknowledge_unit(&mut self, player: HostRef, unit: u32) {
        log(&[ACKNOWLEDGE_UNIT, pid(player), unit]);
    }
    fn town_patrol_point(&mut self, unit: u32) -> Option<u32> {
        let point = self.game.town_patrol_point(unit);
        log(&[TOWN_POINT, unit, u32::from(point.is_some()), point.unwrap_or(EE_UNIT_ID_NONE)]);
        point
    }
    fn patrol_add_unit_tile(&mut self, unit: u32, target: u32, player: HostRef) {
        let (x, y) = reads::tile(target);
        log(&[ADD_UNIT_TILE, unit, target, pid(player), bits(x as f32), bits(y as f32)]);
    }
    fn unit_patrol_group(&mut self, unit: u32) -> HostRef {
        let route = self.game.unit_patrol_group(unit);
        log(&[ROUTE, unit]);
        log_route(route);
        route
    }
    fn object_unit_type(&mut self, unit: u32) -> i64 {
        self.game.object_unit_type(unit)
    }
    fn group_set_queued(&mut self, group: HostRef, queued: bool) {
        log(&[QUEUED, tag(group) as u32, u32::from(queued)]);
    }
    fn group_spread_around_resource(&mut self, group: HostRef, target: u32) {
        log(&[SPREAD_RESOURCE, tag(group) as u32, target]);
    }
    fn group_spread_around_tile(&mut self, group: HostRef, x: i64, y: i64) {
        log(&[SPREAD_TILE, tag(group) as u32, x as u32, y as u32]);
    }
    fn unit_add_build_target(&mut self, unit: u32, target: u32) {
        log(&[ADD_BUILD_TARGET, unit, target]);
    }
    fn unit_order_repair(&mut self, unit: u32, target: u32) {
        log(&[ORDER_REPAIR, unit, target]);
    }
    fn group_add_member(&mut self, group: HostRef, unit: u32) {
        log(&[ADD_MEMBER, tag(group) as u32, unit]);
    }
    fn group_keep_non_citizens(&mut self, group: HostRef) {
        keep(
            group,
            KEEP_NON_CITIZENS,
            |u| crate::units::ffi::ee_units_keeps_non_citizen(u),
            |u| {
                let c = reads::class(u).0;
                reads::live_commandable(u) && c != CATEGORY_LAND_CITIZEN && c != CATEGORY_NAVAL_CITIZEN
            },
        );
    }
    fn patrol_new_lone(&mut self, player: HostRef, x: f32, y: f32) {
        log(&[LONE_PATROL, pid(player), bits(x), bits(y)]);
    }
    fn patrol_insert_point_at(&mut self, unit: u32, x: f32, y: f32, player: HostRef) {
        log(&[INSERT_POINT, unit, pid(player), bits(x), bits(y)]);
    }
    fn unit_goal_patrol_route(&mut self, unit: u32) -> HostRef {
        let route = self.game.unit_goal_patrol_route(unit);
        log(&[LOG_GOAL_ROUTE, unit]);
        log_route(route);
        route
    }
    fn patrol_add_point(&mut self, route: HostRef, x: f32, y: f32, player: HostRef) {
        log(&[ADD_POINT]);
        log_route(route);
        log(&[pid(player), bits(x), bits(y)]);
    }
    fn goal_patrol_add_point(&mut self, goal: &GoalHandle, x: f32, y: f32, player: HostRef) {
        log(&[LOG_GOAL_ADD_POINT, goal_of(goal.as_ptr()).ctor, pid(player), bits(x), bits(y)]);
    }
    fn group_spread_around_unit(&mut self, group: HostRef, target: u32, x: i64, y: i64) {
        log(&[SPREAD_UNIT, tag(group) as u32, target, x as u32, y as u32]);
    }
    fn unit_complex_id(&mut self, unit: u32) -> u32 {
        let id = self.game.unit_complex_id(unit);
        log(&[COMPLEX_OF, unit, id]);
        id
    }
    fn path_to_point(&mut self, first: u32, x: f32, y: f32) -> (f32, f32, f32) {
        let (nx, ny, z) = self.game.path_to_point(first, x, y);
        log(&[PATH_POINT, first, bits(x), bits(y), bits(nx), bits(ny), bits(z)]);
        (nx, ny, z)
    }
    fn group_reset_paths(&mut self, group: HostRef) {
        log(&[RESET_PATHS, tag(group) as u32]);
    }
    fn group_keep_citizens(&mut self, group: HostRef) {
        keep(
            group,
            KEEP_CITIZENS,
            |u| crate::units::ffi::ee_units_keeps_citizen(u),
            |u| {
                let c = reads::class(u).0;
                reads::live_commandable(u) && (c == CATEGORY_LAND_CITIZEN || c == CATEGORY_NAVAL_CITIZEN)
            },
        );
    }
    fn wall_plan(&mut self, player: HostRef, technology: i32, start: (f32, f32), end: (f32, f32)) -> (HostRef, usize) {
        log(&[WALL_PLAN, pid(player), technology as u32, bits(start.0), bits(start.1), bits(end.0), bits(end.1)]);
        // the dry plan's pieces (never read: the dry build only logs)
        (HostRef(std::ptr::dangling_mut::<c_void>()), 2)
    }
    fn wall_build(&mut self, group: HostRef, _pieces: HostRef) {
        log(&[WALL_BUILD, tag(group) as u32]);
    }
    fn wall_release(&mut self, _pieces: HostRef) {
        log(&[WALL_RELEASE]);
    }
    fn group_spread_for_unload(&mut self, group: HostRef, x: i64, y: i64) {
        log(&[SPREAD_UNLOAD, tag(group) as u32, x as u32, y as u32]);
    }
    fn unit_can_unload(&mut self, unit: u32) -> bool {
        let r = self.game.unit_can_unload(unit);
        log(&[CAN_UNLOAD, unit, u32::from(r)]);
        r
    }
    fn goal_set_transport(&mut self, goal: &GoalHandle, unit: u32) {
        log(&[SET_TRANSPORT, goal_of(goal.as_ptr()).ctor, unit]);
    }
    fn goal_can_unload(&mut self, goal: &GoalHandle) -> bool {
        log(&[CAN_UNLOAD_THERE, goal_of(goal.as_ptr()).ctor]);
        true
    }
    fn unload_warning(&mut self, player: HostRef, x: i64, y: i64) {
        log(&[UNLOAD_WARNING, pid(player), x as u32, y as u32]);
    }
    fn technology_unit_class(&mut self, player: HostRef, technology: i32) -> Option<i64> {
        let class = self.game.technology_unit_class(player, technology);
        log(&[TECH_CLASS, pid(player), technology as u32, u32::from(class.is_some()), class.unwrap_or(-1) as u32]);
        class
    }
    fn player_num_capitols(&mut self, player: HostRef) -> i64 {
        let n = self.game.player_num_capitols(player);
        log(&[CAPITOLS, pid(player), n as u32]);
        n
    }
    fn can_produce(&mut self, player: HostRef, technology: i32) -> bool {
        // yes without asking (so the sweep reaches the rest of the body; CanProduce's cost reads are the tech
        // capture's TCST records, which the sweep must not add)
        log(&[CAN_PRODUCE, pid(player), technology as u32]);
        true
    }
    fn can_place_on_tile(&mut self, player: HostRef, unit_type: i64, x: i64, y: i64) -> bool {
        let r = self.game.can_place_on_tile(player, unit_type, x, y);
        log(&[CAN_PLACE, pid(player), unit_type as u32, x as u32, y as u32, u32::from(r)]);
        true
    }
    fn make_building(&mut self, player: HostRef, unit_type: i64, x: i64, y: i64) -> u32 {
        log(&[MAKE_BUILDING, pid(player), unit_type as u32, x as u32, y as u32]);
        DRY_BUILDING
    }
    fn free_farms(&mut self, unit: u32, out: &mut Vec<u32>) {
        // a unit the dry host does not know (the dry building's complex unit): its two dry farms
        if resolved(unit) != EE_UNIT_ID_NONE {
            self.game.free_farms(unit, out);
        } else {
            *out = DRY_FARMS.to_vec();
        }
        log(&[FREE_FARMS, unit]);
        log_ids(out);
    }
    fn sort_farms(&mut self, farms: &mut [u32], x: i64, y: i64, greater: bool) {
        let before = farms.to_vec();
        self.game.sort_farms(farms, x, y, greater);
        log(&[SORT_FARMS, x as u32, y as u32, u32::from(greater)]);
        log_ids(&before);
        log(farms);
    }
    fn unit_add_farm_target(&mut self, unit: u32, target: u32) {
        log(&[ADD_FARM, unit, target]);
    }
    fn unit_order_build(&mut self, unit: u32, target: u32) {
        log(&[ORDER_BUILD, unit, target]);
    }
    fn unit_start_action(&mut self, unit: u32, action: i32) {
        log(&[START_ACTION, unit, action as u32]);
    }
    fn unit_load_farm(&mut self, unit: u32) -> u32 {
        let farm = self.game.unit_load_farm(unit);
        log(&[LOAD_FARM, unit, farm]);
        farm
    }
    fn farm_drop_sites(&mut self, farm: u32, out: &mut Vec<u32>) -> bool {
        let found = self.game.farm_drop_sites(farm, out);
        log(&[DROP_SITES, farm, u32::from(found)]);
        log_ids(out);
        found
    }
    fn can_take(&mut self, target: u32, unit: u32) -> bool {
        let r = self.game.can_take(target, unit);
        log(&[CAN_TAKE, target, unit, u32::from(r)]);
        r
    }
    fn unit_drop_gathered(&mut self, unit: u32) {
        log(&[DROP_GATHERED, unit]);
    }
    fn work_farm(&mut self, farm: u32, unit: u32) {
        log(&[WORK_FARM, farm, unit]);
    }
    fn unit_set_goal(&mut self, unit: u32, goal: GoalHandle) {
        log(&[UNIT_SET_GOAL, unit, goal_of(goal.as_ptr()).ctor]);
    }
    fn group_finish(&mut self, group: HostRef) {
        log(&[FINISH, tag(group) as u32]);
    }
    fn calamity_of(&mut self, player: HostRef, technology: i32) -> Option<(HostRef, i32, bool)> {
        let c = self.game.calamity_of(player, technology);
        let (class_enum, all) = c.map_or((0, false), |(_, e, a)| (e, a));
        log(&[CALAMITY_OF, pid(player), technology as u32, u32::from(c.is_some()), class_enum as u32, u32::from(all)]);
        c
    }
    fn calamity_can_cast_at(&mut self, _class: HostRef, unit: u32, x: i32, y: i32, target: u32) -> i64 {
        // 0 ("can") without asking: CanCastAt is the tech capture's CCAS record, which the sweep must not add
        log(&[CAN_CAST, unit, x as u32, y as u32, target]);
        0
    }
    fn technology_contains(&mut self, player: HostRef, technology: i32, unit_type: i64) -> bool {
        let r = self.game.technology_contains(player, technology, unit_type);
        log(&[CONTAINS, pid(player), technology as u32, unit_type as u32, u32::from(r)]);
        r
    }
    fn unit_power(&mut self, unit: u32) -> i64 {
        let p = self.game.unit_power(unit);
        log(&[POWER, unit, p as u32]);
        p
    }
    fn calamity_cost(&mut self, class: HostRef) -> i64 {
        let c = self.game.calamity_cost(class);
        log(&[COST, c as u32]);
        c
    }
    fn unit_player(&mut self, unit: u32) -> HostRef {
        let p = self.game.unit_player(unit);
        log(&[UNIT_PLAYER, unit, pid(p)]);
        p
    }
    fn group_split_calamity_casters(
        &mut self,
        group: HostRef,
        class: HostRef,
        player: HostRef,
        technology: i32,
        target: u32,
    ) {
        // the keep test on the objects (the units that cannot cast leave; the attack group they would form is not
        // made: logged)
        let mut units = group_units(group);
        let mut i = 0;
        while i < units.len() {
            let u = units[i];
            let (_, unit_type, _, special) = reads::class(u);
            if reads::live_commandable(u)
                && special
                && self.game.unit_power(u) >= self.game.calamity_cost(class)
                && self.game.technology_contains(player, technology, unit_type)
            {
                i += 1;
            } else {
                remove_first(&mut units, u);
            }
        }
        set_group_units(group, units);
        let units = group_units(group);
        log(&[SPLIT, tag(group) as u32, technology as u32, target]);
        log_ids(&units);
    }
    fn goal_clone(&mut self, goal: &GoalHandle) -> Option<GoalHandle> {
        let g = goal_of(goal.as_ptr());
        let h = handle(g);
        log(&[CLONE, g.ctor]);
        Some(h)
    }
    fn group_remove_unit(&mut self, group: HostRef, unit: u32) {
        log(&[REMOVE_UNIT, tag(group) as u32, unit]);
        let mut units = group_units(group);
        remove_first(&mut units, resolved(unit));
        set_group_units(group, units);
    }
    fn unit_clear_goals(&mut self, unit: u32) {
        log(&[CLEAR_GOALS, unit]);
    }
    fn object_hit_points(&mut self, unit: u32) -> i64 {
        self.game.object_hit_points(unit)
    }
    fn player_record(&mut self, player: u64) -> Option<crate::units::players::PlayerRecord> {
        self.game.player_record(player)
    }
    fn player_selection(&mut self, player: HostRef) -> HostRef {
        log(&[PLAYER_SELECTION, pid(player)]);
        group_ref(0) // the sweep's selection
    }
    fn unit_seen_by(&mut self, unit: u32, player: HostRef) -> bool {
        self.game.unit_seen_by(unit, player)
    }
    fn unit_ghost_seen(&mut self, unit: u32) -> bool {
        self.game.unit_ghost_seen(unit)
    }
    fn unit_attackable(&mut self, unit: u32, damage_index: i64) -> bool {
        self.game.unit_attackable(unit, damage_index)
    }
    fn unit_class_targetable(&mut self, unit: u32) -> bool {
        self.game.unit_class_targetable(unit)
    }
    fn unit_can_attack_class(&mut self, unit: u32, target: u32) -> bool {
        self.game.unit_can_attack_class(unit, target)
    }
    fn unit_max_hit_points(&mut self, unit: u32) -> i64 {
        self.game.unit_max_hit_points(unit)
    }
    fn unit_garrison_capacity(&mut self, unit: u32) -> i64 {
        self.game.unit_garrison_capacity(unit)
    }
    fn unit_has_tile_node(&mut self, unit: u32) -> bool {
        self.game.unit_has_tile_node(unit)
    }
    fn unit_shares_rally_point(&mut self, unit: u32, unit_type: i64) -> bool {
        self.game.unit_shares_rally_point(unit, unit_type)
    }
    fn unit_num_goal_queues(&mut self, unit: u32) -> i64 {
        self.game.unit_num_goal_queues(unit)
    }
    fn unit_clear_goal_queue(&mut self, unit: u32, queue: i64) {
        log(&[CLEAR_QUEUE, unit, queue as u32]);
    }
    fn unit_clear_all_goal_queues(&mut self, unit: u32) {
        log(&[CLEAR_ALL_QUEUES, unit]);
    }
    fn unit_queue_goal(&mut self, unit: u32, goal: GoalHandle, queue: i64) {
        log(&[QUEUE_GOAL, unit, goal_of(goal.as_ptr()).ctor, queue as u32]);
    }
    fn unit_queue_timer(&mut self, unit: u32, queue: i64, ms: i64) {
        log(&[QUEUE_TIMER, unit, queue as u32, ms as u32]);
    }
    fn unit_goal_clone(&mut self, unit: u32) -> Option<GoalHandle> {
        let kind = reads::goal_state_kind(unit);
        let h = handle(DryGoal { ctor: CTOR_GOAL_STATE, kind, unit: 0, real: std::ptr::null_mut() });
        log(&[LOG_GOAL_STATE_CLONE, unit, u32::from(kind)]);
        Some(h)
    }
    fn garrison_rally_load(&mut self, garrison: u32) -> Option<GoalHandle> {
        // valid (the load goal is not made: a goal is a world change)
        let h =
            handle(DryGoal { ctor: CTOR_GARRISON_LOAD, kind: GOAL_LOAD_GARRISON, unit: 0, real: std::ptr::null_mut() });
        log(&[GARRISON_LOAD, garrison]);
        Some(h)
    }
    fn object_record(&mut self, unit: u32) -> Option<crate::units::store::EeUnitRecord> {
        self.game.object_record(unit)
    }
    fn unit_air_traffic_type(&mut self, unit: u32) -> i64 {
        self.game.unit_air_traffic_type(unit)
    }
    fn unit_queue_clear(&mut self, unit: u32, queue: i64) {
        log(&[QUEUE_CLEAR, unit, queue as u32]);
    }
    fn unit_clear_air_traffic(&mut self, unit: u32) {
        let t = self.game.unit_air_traffic_type(unit);
        log(&[CLEAR_AIR_TRAFFIC, unit, t as u32]);
    }
    fn unit_refresh_goals(&mut self, unit: u32) {
        log(&[REFRESH_GOALS, unit]);
    }
    fn unit_clear_build_targets(&mut self, unit: u32) {
        log(&[CLEAR_BUILD_TARGETS, unit]);
    }
    fn unit_action_changed(&mut self, unit: u32) {
        log(&[LOG_ACTION_CHANGED, unit]);
    }
    fn unit_halt(&mut self, unit: u32) {
        log(&[HALT, unit]);
    }
    fn unit_load_traits(&mut self, unit: u32) -> u8 {
        self.game.unit_load_traits(unit)
    }
    fn unit_load_goal(&mut self, unit: u32, kind: u8) -> HostRef {
        // the real goal, wrapped
        let real = self.game.unit_load_goal(unit, kind);
        log(&[LOAD_GOAL, unit, u32::from(kind), u32::from(!real.0.is_null())]);
        if real.0.is_null() {
            return HostRef(std::ptr::null_mut());
        }
        HostRef(new_goal(DryGoal { ctor: CTOR_OLD_LOAD, kind, unit, real: real.0 }))
    }
    fn load_goal_add_units(&mut self, goal: HostRef, units: &[u32]) {
        log(&[ADD_LOAD_UNITS, goal_of(goal.0).ctor]);
        log_ids(units);
    }
    fn load_goal_new(&mut self, kind: u8, carrier: u32, units: &[u32], flag: bool) -> HostRef {
        log(&[NEW_LOAD, u32::from(kind), carrier, u32::from(flag)]);
        log_ids(units);
        HostRef(new_goal(DryGoal { ctor: CTOR_NEW_LOAD, kind, unit: carrier, real: std::ptr::null_mut() }))
    }
    fn load_goal_fields(&mut self, goal: HostRef) -> (bool, i64, i64) {
        // a load goal the dry host made is valid for an even carrier id and its tile is the carrier's
        let g = goal_of(goal.0);
        let (valid, x, y) = if g.real.is_null() {
            let (x, y) = reads::tile(g.unit);
            (g.unit & 1 == 0, x, y)
        } else {
            self.game.load_goal_fields(HostRef(g.real))
        };
        log(&[LOAD_FIELDS, g.ctor, u32::from(valid), x as u32, y as u32]);
        (valid, x, y)
    }
    fn load_goal_clone(&mut self, goal: HostRef) -> Option<GoalHandle> {
        let g = goal_of(goal.0);
        let h = handle(g);
        log(&[CLONE, g.ctor]);
        Some(h)
    }
    fn unit_take_load_goal(&mut self, unit: u32, goal: HostRef) {
        log(&[UNIT_SET_GOAL, unit, goal_of(goal.0).ctor]);
    }
    fn player_event(&mut self, player: HostRef, x: i64, y: i64, code: i32) {
        log(&[EVENT, pid(player), x as u32, y as u32, code as u32]);
    }
    fn group_add_unit(&mut self, group: HostRef, unit: u32) {
        log(&[ADD_UNIT, tag(group) as u32, unit]);
        let mut units = group_units(group);
        units.push(resolved(unit));
        set_group_units(group, units);
    }
    fn transport_filter(&mut self, transport: u32, units: &mut Vec<u32>, rejected: &mut Vec<u32>) {
        let first = rejected.len();
        self.game.transport_filter(transport, units, rejected);
        log(&[FILTER, transport]);
        log_ids(units);
        log_ids(&rejected[first..]);
    }
    fn research_node(&mut self, player: HostRef, technology: i32) -> Option<crate::units::research::ResearchNode> {
        self.game.research_node(player, technology)
    }
    fn scenario_game(&mut self) -> bool {
        self.game.scenario_game()
    }
    fn unit_shares_research(&mut self, unit: u32, unit_type: i64) -> bool {
        self.game.unit_shares_research(unit, unit_type)
    }
    fn unit_research_state(&mut self, unit: u32) -> (i64, i64, bool) {
        self.game.unit_research_state(unit)
    }
    fn unit_queue_research(&mut self, unit: u32, technology: i32, front: bool) {
        log(&[QUEUE_RESEARCH, unit, technology as u32, u32::from(front)]);
    }
    fn player_hack_class(&mut self, player: HostRef) -> i32 {
        self.game.player_hack_class(player)
    }
    fn unit_class_count(&mut self) -> i64 {
        self.game.unit_class_count()
    }
    fn unit_class_info(&mut self, unit_type: i32) -> Option<(i64, bool)> {
        self.game.unit_class_info(unit_type)
    }
    fn player_class_scale(&mut self, player_id: i64, unit_type: i32) -> f32 {
        self.game.player_class_scale(player_id, unit_type)
    }
    fn map_has_tile(&mut self, x: i64, y: i64) -> bool {
        self.game.map_has_tile(x, y)
    }
    fn hack_create_unit(&mut self, unit_type: i32, x: f32, y: f32, player_id: i64) {
        log(&[HACK_UNIT, unit_type as u32, bits(x), bits(y), player_id as u32]);
    }
    fn hack_create_building(&mut self, unit_type: i32, x: i64, y: i64, player_id: i64) {
        log(&[HACK_BUILDING, unit_type as u32, x as u32, y as u32, player_id as u32]);
    }
}

/// TEST ONLY: begins a dry run of this thread over the selection's units (`count` ids); its reference, which the
/// bodies take as the sender's selection.
///
/// # Safety
/// `ids` points to `count` ids (or is null for 0).
#[no_mangle]
pub unsafe extern "C" fn ee_units_dry_begin(ids: *const u32, count: usize) -> *mut c_void {
    let ids = if count == 0 {
        &[][..]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(ids, count) }
    };
    begin(ids).0
}

/// TEST ONLY: the length of this thread's dry log so far.
#[no_mangle]
pub extern "C" fn ee_units_dry_log_len() -> usize {
    DRY.with(|d| d.borrow().as_ref().map_or(0, |s| s.log.len()))
}

/// TEST ONLY: ends the dry run: its log into `out` (room for `ee_units_dry_log_len()` words), the keeps'
/// disagreements into `mismatches`; the number of words.
///
/// # Safety
/// `out` has room for the log; `mismatches` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_units_dry_end(out: *mut u32, mismatches: *mut u32) -> usize {
    let (log, m) = end();
    // SAFETY: the caller's contract
    unsafe {
        if !log.is_empty() {
            std::ptr::copy_nonoverlapping(log.as_ptr(), out, log.len());
        }
        *mismatches = m;
    }
    log.len()
}

#[cfg(test)]
#[path = "dry_tests.rs"]
mod tests;
