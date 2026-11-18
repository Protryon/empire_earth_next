//! The per-unit Lua interface in the game (EECPNextUnits.cpp before; docs/AI_LUA_UNITS.md): the state's C ABI, the
//! game's hooks, the enemy memory and the danger layers' rebuild, and the `ai.*` bindings that read the world
//! (queries, the danger layers, the route, the held set). The world is read through `ee_sim::world`'s views; what
//! the views do not hold (the unit lookup, a goal's facts, the continents, the names, the trace) is one C++ host,
//! `ee_ai_units_host` (EECPNextUnits.cpp). Absent from a test binary (`units_state.rs` holds the tested logic).
//!
//! Every binding makes the Lua calls the C++ made, in its order (the same tables, the same sizes, the same fields
//! in the same order: the trace golden, tools/ai_binding_golden.py, holds them). A Lua error (a charge over the
//! budget, an allocation refused) longjmps out of a binding: nothing here owns a destructor across a Lua call but
//! the read token, whose drop only counts in debug builds; scratch lives in the state.
#![cfg(not(test))]
#![allow(clippy::missing_safety_doc)] // each function's contract is its doc's "Safety:"
#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::ffi::{c_char, c_int, c_long, c_void, CStr, CString};

use crate::ai::lua_api::*;
use crate::ai::lua_host;
use crate::ai::units_lua::{
    ee_ai_units_layer_arg, ee_ai_units_opt_bool, ee_ai_units_opt_int, ee_ai_units_push_sorted, ee_ai_units_read_filter,
    ee_ai_units_read_ids, ee_ai_units_set_bool, ee_ai_units_set_int, ee_ai_units_set_str, EeAiUnitsFilter,
    EeAiUnitsSortEntry, MAX_IDS,
};
use crate::ai::units_state::*;
use crate::world::{ClassView, PlayerView, StatsView, TileView, UnitView, WorldRead, WorldView};
use ee_base::math::um;

// ---------------------------------------------------------------------------------------------------------------
// The host

/// `EECPUnitsGet(player)`: the player's state (made when missing).
pub const EE_AI_UNITS_HOST_STATE: i32 = 1;
/// `gPhysics->mGameTime`.
pub const EE_AI_UNITS_HOST_NOW: i32 = 2;
/// `gWorld.GetUnit(a)` (0: none).
pub const EE_AI_UNITS_HOST_UNIT: i32 = 3;
/// `gMapWidth | gMapHeight << 32` (0 without a map).
pub const EE_AI_UNITS_HOST_MAP_SIZE: i32 = 4;
/// `EECPNextClassByName(a)` (a NUL-terminated name).
pub const EE_AI_UNITS_HOST_CLASS_BY_NAME: i32 = 5;
/// `EECPNextClassName(a)` into `out` (1024 bytes, NUL-terminated) -> its length.
pub const EE_AI_UNITS_HOST_CLASS_NAME: i32 = 6;
/// `player->DetachUnitFromAI(a)`.
pub const EE_AI_UNITS_HOST_DETACH: i32 = 7;
/// `a->mIsDetachedFromAI = false` (when set).
pub const EE_AI_UNITS_HOST_ATTACH: i32 = 8;
/// The complex unit `a`'s `mLastAttacker` (0: none).
pub const EE_AI_UNITS_HOST_LAST_ATTACKER: i32 = 9;
/// The player `a`'s `mUnitClassUpgrades[b]` (0: none or past the vector).
pub const EE_AI_UNITS_HOST_UPGRADE: i32 = 10;
/// `gContinentManager.GetContinent((a, b), c ? kAIPathWaterBody : kAIPathLandMass)`.
pub const EE_AI_UNITS_HOST_CONTINENT: i32 = 11;
/// With the trace on: `EECPTraceOut(player, a, "%s", b)` under the "next" scope.
pub const EE_AI_UNITS_HOST_TRACE: i32 = 12;
/// The facts of the unit `a` (an own or allied complex unit's record, `EeAiUnitsFacts`) into `out`.
pub const EE_AI_UNITS_HOST_FACTS: i32 = 13;
/// `EECPNextAssigned(player, a)`.
pub const EE_AI_UNITS_HOST_ASSIGNED: i32 = 14;
/// `EECPNextHasRoom(player, a)`.
pub const EE_AI_UNITS_HOST_HAS_ROOM: i32 = 15;
/// The player `a`'s `IsHuman()`.
pub const EE_AI_UNITS_HOST_IS_HUMAN: i32 = 16;
/// `gWorld.mTeams.size()`.
pub const EE_AI_UNITS_HOST_TEAMS: i32 = 17;
/// `gWorld.mTeams[a].ContainsPlayer(b)`.
pub const EE_AI_UNITS_HOST_TEAM_HAS: i32 = 18;
/// `gWorld.mTeams[a].GetNumPlayers()`.
pub const EE_AI_UNITS_HOST_TEAM_SIZE: i32 = 19;
/// `gWorld.mTeams[a].GetPlayersAt(b)`.
pub const EE_AI_UNITS_HOST_TEAM_PLAYER: i32 = 20;
/// The complex unit `a`'s gather goal's resource id, when it has one and it is valid (0: none).
pub const EE_AI_UNITS_HOST_GATHER_TARGET: i32 = 21;
/// `gCPTraceOn`.
pub const EE_AI_UNITS_HOST_TRACE_ON: i32 = 22;

/// What a unit's record needs beyond the views (`EE_AI_UNITS_HOST_FACTS`): its goal's place and target, its home
/// base, the terrain it crosses, its stance.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeAiUnitsFacts {
    /// a goal state (then `tx`, `ty`: `GetGoalLocation`, floored)
    pub has_goal: bool,
    pub tx: c_long,
    pub ty: c_long,
    /// a gather goal: the resource kind it carries (0..4; -1 none or not a gather goal)
    pub gathers: c_long,
    /// a gather goal waiting for room (`IsGatherWaiting() && mCurrentPhysicsAction != 0`)
    pub waiting: bool,
    /// a build goal stalled (`kActionWaitForBuildSiteToClear`, the target unbuilt, `IsBuildSiteTaken`)
    pub stalled: bool,
    /// the goal's target unit id (gathered, built or attacked; valid; 0: none)
    pub target: c_long,
    /// `mHomeBase`'s id when valid (0: none)
    pub home_base: c_long,
    /// not a building and its class moves (`GetMaxVelocity() > 0`): then the two terrain flags
    pub moves: bool,
    pub crosses_forest: bool,
    pub crosses_cliff: bool,
    /// the behaviour's index in `EEUnitBehavior::sTable` (-1: none)
    pub stance: c_long,
    /// `mGarrisonCount`
    pub garrisoned: c_long,
}

/// `EE_AI_UNITS_HOST_FACTS`'s record before the C++ fills it: no goal, no gathers (-1), no stance (-1).
///
/// # Safety
/// `out` points to a record.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_facts_reset(out: *mut EeAiUnitsFacts) {
    *out = EeAiUnitsFacts { gathers: -1, stance: -1, ..Default::default() };
}

extern "C" {
    fn ee_ai_units_host(op: i32, player: *mut c_void, a: i64, b: i64, c: i64, out: *mut c_void) -> i64;
}

fn host(op: i32, me: *mut c_void, a: i64, b: i64, c: i64) -> i64 {
    // SAFETY: the C++ switch over the live game (the simulation thread, inside a call from it)
    unsafe { ee_ai_units_host(op, me, a, b, c, std::ptr::null_mut()) }
}

fn host_out(op: i32, me: *mut c_void, a: i64, out: *mut c_void) -> i64 {
    // SAFETY: as above; `out` is the op's record
    unsafe { ee_ai_units_host(op, me, a, 0, 0, out) }
}

fn now() -> c_long {
    host(EE_AI_UNITS_HOST_NOW, std::ptr::null_mut(), 0, 0, 0) as c_long
}

fn map_size() -> (c_long, c_long) {
    let v = host(EE_AI_UNITS_HOST_MAP_SIZE, std::ptr::null_mut(), 0, 0, 0) as u64;
    ((v & 0xffff_ffff) as c_long, (v >> 32) as c_long)
}

fn continent(x: c_long, y: c_long, sea: bool) -> u16 {
    host(EE_AI_UNITS_HOST_CONTINENT, std::ptr::null_mut(), x as i64, y as i64, sea as i64) as u16
}

unsafe extern "C" fn class_by_name(name: *const c_char) -> c_long {
    host(EE_AI_UNITS_HOST_CLASS_BY_NAME, std::ptr::null_mut(), name as i64, 0, 0) as c_long
}

fn unit_handle(id: c_long) -> u64 {
    host(EE_AI_UNITS_HOST_UNIT, std::ptr::null_mut(), id as i64, 0, 0) as u64
}

fn trace(me: *mut c_void, event: &CStr, text: &str) {
    if host(EE_AI_UNITS_HOST_TRACE_ON, me, 0, 0, 0) == 0 {
        return;
    }
    let text = CString::new(text).unwrap_or_default();
    host(EE_AI_UNITS_HOST_TRACE, me, event.as_ptr() as i64, text.as_ptr() as i64, 0);
}

/// The player's state (`EECPUnitsGet`).
fn state_of(me: *mut c_void) -> *mut UnitsState {
    host(EE_AI_UNITS_HOST_STATE, me, 0, 0, 0) as usize as *mut UnitsState
}

// ---------------------------------------------------------------------------------------------------------------
// The game's reads (EECPNextUnits.cpp's unit helpers)

const NO_TILE: i64 = -10000;
const NO_RESEARCH: i64 = 100000;
const CAT_LAND_CITIZEN: u64 = 6;
const CAT_NAVAL_CITIZEN: u64 = 0x16;
const FAMILY_ANIMAL: u64 = 0x11;
const RESOURCE_FOOD: u64 = 2;
const RESOURCE_KINDS: [&CStr; 5] = [c"food", c"wood", c"stone", c"gold", c"iron"];

struct Game<'t> {
    t: &'t WorldRead,
    world: WorldView<'t>,
    me: PlayerView<'t>,
    me_ptr: *mut c_void,
}

impl<'t> Game<'t> {
    fn new(t: &'t WorldRead, me_ptr: *mut c_void) -> Option<Game<'t>> {
        let world = WorldView::global(t)?;
        // SAFETY: the binding's upvalue (the C++'s EEComputerPlayer*, whose EEPlayer and RTSPlayer start at it)
        let me = unsafe { PlayerView::of(t, me_ptr as u64) }?;
        Some(Game { t, world, me, me_ptr })
    }

    fn unit(&self, handle: u64) -> Option<UnitView<'t>> {
        // SAFETY: the handle is a live unit the C++ gave (the lookup, a pointer member) or 0
        unsafe { UnitView::of(self.t, handle) }
    }

    fn gaia(&self) -> u64 {
        self.world.player(0).map_or(0, |p| p.handle())
    }

    fn player_slot(&self, p: c_long) -> Option<PlayerView<'t>> {
        if p < 0 || p as u64 >= self.world.num_players() {
            return None;
        }
        self.world.player(p as usize)
    }

    /// `EEPlayer::IsAlly`: itself, or no war declared on it
    fn is_ally(&self, other: &PlayerView<'_>) -> bool {
        other.handle() == self.me.handle() || self.me.enemy(other.id() as usize) == 0
    }

    fn on_map(u: &UnitView<'_>) -> bool {
        u.tile_xy().0 != NO_TILE && u.tile().is_some()
    }

    fn tile_seen(&self, tile: &TileView<'_>) -> bool {
        tile.visibility(self.me.id() as usize) > 1 || self.me.sees_all()
    }

    fn tile_explored(&self, tile: &TileView<'_>) -> bool {
        tile.visibility(self.me.id() as usize) > 0 || self.me.sees_all()
    }

    /// `EECPUnitsSeen`: its own and its allies' always; others while their tile is seen
    fn seen(&self, u: &UnitView<'_>) -> bool {
        if let Some(p) = u.player() {
            if p.handle() == self.me.handle() || (p.handle() != self.gaia() && self.is_ally(&p)) {
                return true;
            }
        }
        if !Self::on_map(u) {
            return false;
        }
        u.tile().is_some_and(|t| self.tile_seen(&t))
    }

    /// `EECPUnitsExplored`
    fn explored(&self, u: &UnitView<'_>) -> bool {
        Self::on_map(u) && u.tile().is_some_and(|t| self.tile_explored(&t))
    }

    /// `EECPUnitsIsResource`
    fn is_resource(&self, u: &UnitView<'_>) -> bool {
        let r = u.resource_type() as i64 - RESOURCE_FOOD as i64;
        (0..5).contains(&r) && u.resource_amount() > 0 && u.player().is_none_or(|p| p.handle() == self.gaia())
    }

    /// `EECPUnitsAlive`
    fn alive(&self, id: c_long) -> Option<UnitView<'t>> {
        if id <= 0 {
            return None;
        }
        let u = self.unit(unit_handle(id))?;
        (u.valid() && u.hit_points() > 0 && u.class().is_some()).then_some(u)
    }

    /// `EECPUnitsPending`: a foundation in the player's pending list
    fn pending(&self, id: c_long) -> Option<UnitView<'t>> {
        for k in 0..self.me.pending_units_len() {
            if let Some(u) = self.unit(self.me.pending_unit_handle(k)) {
                if u.id() as c_long == id {
                    return Some(u);
                }
            }
        }
        None
    }

    /// `EECPUnitsOwn` -> the complex unit
    fn own(&self, s: &UnitsState, id: c_long) -> Option<UnitView<'t>> {
        let id = s.forward_id(id);
        let u = self.alive(id).or_else(|| self.pending(id))?;
        if u.player().map_or(0, |p| p.handle()) != self.me.handle() || u.class().is_none() {
            return None;
        }
        u.complex_unit()
    }

    /// `EECPUnitsKnown`: a unit the player may know of
    fn known(&self, s: &UnitsState, id: c_long) -> Option<UnitView<'t>> {
        let mut u = self.alive(s.forward_id(id));
        if u.is_none() {
            if let Some(p) = self.pending(s.forward_id(id)) {
                if p.class().is_some() && p.complex_unit_handle() != 0 {
                    return Some(p); // the player's own (its owner field may still be unset)
                }
            }
            // a resource is alive with hit points 0 (a worked mine)
            let r = self.unit(unit_handle(id))?;
            if !r.valid() || !self.is_resource(&r) {
                return None;
            }
            u = Some(r);
        }
        let u = u?;
        u.class()?;
        if self.is_resource(&u) {
            return self.explored(&u).then_some(u);
        }
        if is_projectile(&u) {
            return None;
        }
        if let Some(c) = u.complex_unit() {
            if c.container_handle() != 0 && u.player().map_or(0, |p| p.handle()) != self.me.handle() {
                return None;
            }
        }
        self.seen(&u).then_some(u)
    }
}

fn category(u: &UnitView<'_>) -> u64 {
    u.class().map_or(u64::MAX, |c| c.category())
}

/// `EECPUnitsIsProjectile`
fn is_projectile(u: &UnitView<'_>) -> bool {
    matches!(category(u), 9 | 0xb | 0xc | 0x14)
}

/// `EEUnitClass::IsBuilding`: the class body table's answer for its category
fn is_building(c: &ClassView<'_>) -> bool {
    use crate::units::cls_dispatch::{EE_UNITS_BODY_IS_BUILDING, EE_UNIT_CLASS_BODIES};
    let k = c.category();
    k < 256 && EE_UNIT_CLASS_BODIES[EE_UNITS_BODY_IS_BUILDING][k as usize] == 1
}

/// `EECPUnitsCategoryClass`
fn category_class(c: &ClassView<'_>, has_resource: bool) -> &'static CStr {
    match c.category() {
        9 | 0xb | 0xc | 0x14 => return c"projectile",
        6 => return c"citizen",
        0x16 => return c"boat",
        1 => return c"resource",
        0 => return if has_resource { c"resource" } else { c"ambient" },
        3 | 7 => return c"air",
        4 | 0xa => return c"ship",
        0x10 | 0x11 | 0x12 | 0x21 => return c"transport",
        0xd | 0xe | 0x1a | 0x1e => return c"priest",
        0x17 => return c"hero",
        0x1b | 0x1c => return c"animal",
        _ => {}
    }
    if is_building(c) {
        return c"building";
    }
    if c.family() == FAMILY_ANIMAL {
        return c"animal";
    }
    c"soldier"
}

/// `EECPUnitsCategoryClass` of the class `class` (an `EEUnitClass*`): "citizen", "building", "soldier", ...
///
/// # Safety
/// `class` is a live class.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_category_class(class: u64, has_resource: bool) -> *const c_char {
    let t = WorldRead::begin();
    match ClassView::of(&t, class) {
        Some(c) => category_class(&c, has_resource).as_ptr(),
        None => c"soldier".as_ptr(),
    }
}

fn unit_category(u: &UnitView<'_>) -> &'static CStr {
    match u.class() {
        Some(c) => category_class(&c, u.resource_amount() > 0),
        None => c"soldier",
    }
}

fn attack_of(st: &StatsView<'_>) -> f32 {
    crate::world_field!(st.fields(), mAttack)
}

/// `EECPUnitsArmed`
fn armed(u: &UnitView<'_>) -> bool {
    u.stats().is_some_and(|s| attack_of(&s) != 0.0)
}

/// `EECPUnitsStrength`: hit points x attack
fn strength(u: &UnitView<'_>) -> c_long {
    match u.stats() {
        Some(st) => (u.hit_points() as c_long).wrapping_mul(um::lrintf(attack_of(&st)) as c_long),
        None => 0,
    }
}

/// `EECPUnitsAction` of a complex unit
fn action(c: &UnitView<'_>) -> &'static CStr {
    if c.container_handle() != 0 {
        return c"inside";
    }
    if c.class().is_some_and(|k| is_building(&k)) {
        if !c.is_built() {
            return c"unbuilt";
        }
        return if c.current_research() != NO_RESEARCH { c"produce" } else { c"idle" };
    }
    match c.goal() {
        None => c"idle",
        Some((kind, _)) => match kind {
            1 => c"move",
            2 => c"attack",
            3 => c"attack_ground",
            4 => c"build",
            6 => c"gather",
            7 => c"load",
            8 => c"unload",
            9 => c"patrol",
            10 => c"explore",
            11 => c"cast",
            12 => c"garrison",
            15 => c"heal",
            _ => c"other",
        },
    }
}

fn is_idle(c: &UnitView<'_>) -> bool {
    c.container_handle() == 0 && action(c) == c"idle"
}

fn centi(v: f32) -> c_long {
    um::floor(v * 100.0) as c_long
}

/// `EECPUnitsName`: a class's data id, kept in the state (the pointer lives while the state does)
fn name_of(s: &mut UnitsState, unit_type: c_long) -> *const c_char {
    s.names
        .entry(unit_type)
        .or_insert_with(|| {
            let mut buf = vec![0u8; 1024];
            let n =
                host_out(EE_AI_UNITS_HOST_CLASS_NAME, std::ptr::null_mut(), unit_type as i64, buf.as_mut_ptr().cast());
            buf.truncate(n.clamp(0, 1023) as usize);
            CString::new(buf).unwrap_or_default()
        })
        .as_ptr()
}

// ---------------------------------------------------------------------------------------------------------------
// The state's work for the game

/// `EECPUnitsRemember`: what the player sees of its enemies (every game second)
fn remember(g: &Game<'_>, s: &mut UnitsState) {
    let now = now();
    let n = g.world.num_players().min(64);
    for p in 1..n {
        let Some(other) = g.world.player(p as usize) else { continue };
        if other.handle() == g.me.handle() || g.is_ally(&other) {
            continue;
        }
        for u in other.units() {
            let Some(c) = u.complex_unit() else { continue };
            if !(u.hit_points() > 0) || c.container_handle() != 0 || is_projectile(&u) || !Game::on_map(&u) {
                continue;
            }
            if !u.tile().is_some_and(|t| g.tile_seen(&t)) {
                continue;
            }
            let id = u.id() as c_long;
            if !s.memory.contains_key(&id) && s.memory.len() >= MAX_REMEMBERED {
                continue;
            }
            let class = u.class();
            let (x, y) = u.tile_xy();
            let r = s.memory.entry(id).or_default();
            r.player = p as c_long;
            r.unit_type = class.map_or(0, |c| c.unit_type() as c_long);
            r.x = x as c_long;
            r.y = y as c_long;
            r.hp = u.hit_points() as c_long;
            r.seen = now;
            r.building = class.is_some_and(|c| is_building(&c)) as c_long;
        }
    }
    // forget: the dead whose place is seen; buildings whose tile is seen without them; mobile units after memory_ms
    let map = g.world.map();
    let memory_ms = s.memory_ms;
    let ids: Vec<(c_long, Remembered)> = s.memory.iter().map(|(&k, &v)| (k, v)).collect();
    for (id, r) in ids {
        if r.seen == now {
            continue;
        }
        let tile = if r.x >= 0 && r.y >= 0 { map.and_then(|m| m.tile(r.x as i64, r.y as i64)) } else { None };
        let tile_seen = tile.is_some_and(|t| g.tile_seen(&t));
        let forget =
            if r.building != 0 { tile_seen } else { now - r.seen > memory_ms || (tile_seen && g.alive(id).is_none()) };
        if forget {
            s.memory.remove(&id);
        }
    }
}

/// The player `p`'s upgrade of class `t`'s attack (rounded) and floored range, if it has one
fn upgrade_attack_range(g: &Game<'_>, r: &Remembered) -> Option<(c_long, c_long)> {
    if r.unit_type < 0
        || r.unit_type as usize >= ClassView::count(g.t)
        || ClassView::by_index(g.t, r.unit_type as usize).is_none()
    {
        return None;
    }
    let owner = g.player_slot(r.player).filter(|_| r.player > 0)?;
    let h = host(EE_AI_UNITS_HOST_UPGRADE, std::ptr::null_mut(), owner.handle() as i64, r.unit_type as i64, 0) as u64;
    // SAFETY: the player's live upgrade object
    let st = unsafe { StatsView::of(g.t, h) }?;
    Some((um::lrintf(attack_of(&st)) as c_long, um::floor(st.range()) as c_long))
}

/// `EECPUnitsDangerBuild`: the threat layer from what the player sees and remembers and where its citizens were
/// hurt, the lost layer, the influence layer
fn danger_build(g: &Game<'_>, s: &mut UnitsState) {
    let (w, h) = map_size();
    s.danger_begin(w, h);
    let now = now();
    let memory: Vec<Remembered> = s.memory.values().copied().collect();
    for r in &memory {
        if let Some((attack, range)) = upgrade_attack_range(g, r) {
            s.danger_remembered(r, attack, range, now);
        }
    }
    s.danger_spots_and_lost(now);
    for u in g.me.units() {
        if !(u.hit_points() > 0) || !Game::on_map(&u) || u.stats().is_none() || is_projectile(&u) {
            continue;
        }
        let v = strength(&u);
        if v > 0 {
            let (x, y) = u.tile_xy();
            s.influence_add(x as c_long, y as c_long, v);
        }
    }
    for r in &memory {
        if let Some((attack, _)) = upgrade_attack_range(g, r) {
            s.influence_remembered(r, attack, now);
        }
    }
}

/// `EECPUnitsTake`: the unit becomes the script's (out of the engine's attack and gatherer bookkeeping)
fn take(g: &Game<'_>, s: &mut UnitsState, c: &UnitView<'_>) {
    let id = c.id() as c_long;
    if s.held.contains(&id) {
        return;
    }
    s.held.insert(id);
    if !c.class().is_some_and(|k| is_building(&k)) {
        host(EE_AI_UNITS_HOST_DETACH, g.me_ptr, c.handle() as i64, 0, 0);
    }
}

/// `EECPUnitsRelease`
fn release(g: &Game<'_>, s: &mut UnitsState, c: &UnitView<'_>) {
    if !s.held.remove(&(c.id() as c_long)) || c.class().is_some_and(|k| is_building(&k)) {
        return;
    }
    if crate::world_field!(c.complex(), mIsDetachedFromAI) {
        host(EE_AI_UNITS_HOST_ATTACH, g.me_ptr, c.handle() as i64, 0, 0);
    }
}

/// `EECPUnitsAreaNear`: the land mass (or water body) at a tile, else the nearest within 3 tiles
fn area_near(x: c_long, y: c_long, sea: bool) -> u16 {
    for r in 0..=3 as c_long {
        for dy in -r..=r {
            for dx in -r..=r {
                if dx > -r && dx < r && dy > -r && dy < r {
                    continue;
                }
                if x + dx < 0 || y + dy < 0 {
                    continue;
                }
                let a = continent(x + dx, y + dy, sea);
                if a != 0 {
                    return a;
                }
            }
        }
    }
    0
}

/// `EECPUnitsCellOpen`: one of five tiles of the cell is on the area
fn cell_open(cx: c_long, cy: c_long, sea: bool, area: u16) -> bool {
    const AT: [[c_long; 2]; 5] = [[2, 2], [0, 0], [3, 0], [0, 3], [3, 3]];
    AT.iter().any(|a| continent(cx * DANGER_CELL + a[0], cy * DANGER_CELL + a[1], sea) == area)
}

// ---------------------------------------------------------------------------------------------------------------
// The C ABI of the state (the C++'s hooks and accessors)

/// A new state (`EEComputerPlayer::mUnitsState`).
#[no_mangle]
pub extern "C" fn ee_ai_units_state_new() -> *mut c_void {
    Box::into_raw(Box::<UnitsState>::default()).cast()
}

/// Frees a state (NULL: nothing).
///
/// # Safety
/// `s` is NULL or a live state, not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_state_free(s: *mut c_void) {
    if !s.is_null() {
        drop(Box::from_raw(s as *mut UnitsState));
    }
}

unsafe fn st<'a>(s: *mut c_void) -> &'a mut UnitsState {
    &mut *(s as *mut UnitsState)
}

/// `EECPUnitsHoldsID`.
///
/// # Safety
/// `s` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_holds(s: *mut c_void, id: c_long) -> bool {
    st(s).held.contains(&id)
}

/// The held ids into `out` (room for `cap`) -> their count.
///
/// # Safety
/// `s` is a live state; `out` has room for `cap`.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_held(s: *mut c_void, out: *mut c_long, cap: usize) -> usize {
    let s = st(s);
    for (k, &id) in s.held.iter().enumerate().take(cap) {
        *out.add(k) = id;
    }
    s.held.len()
}

/// `EECPUnitsReflexOn` of a state: "held" (off by default), "naval_flee", "danger", "militia"; others true.
///
/// # Safety
/// `s` is a live state; `name` NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_reflex_on(s: *mut c_void, name: *const c_char) -> bool {
    let s = st(s);
    match CStr::from_ptr(name).to_bytes() {
        b"held" => s.reflex_held,
        b"naval_flee" => s.reflex_naval_flee,
        b"danger" => s.reflex_danger,
        b"militia" => s.reflex_militia,
        _ => true,
    }
}

/// Whether a binding was called (`EECPUnitsUsed`).
///
/// # Safety
/// `s` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_used(s: *mut c_void) -> bool {
    st(s).used
}

/// `EECPUnitsForward`.
///
/// # Safety
/// `s` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_forward(s: *mut c_void, id: c_long) -> c_long {
    st(s).forward_id(id)
}

/// `EECPUnitsWithinLimits` at the game ms `now`.
///
/// # Safety
/// `s` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_within_limits(s: *mut c_void, units: c_long, now: c_long) -> bool {
    st(s).within_limits(units, now)
}

/// The enemy memory's building flag of `id` (-1: not remembered).
///
/// # Safety
/// `s` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_remembered_building(s: *mut c_void, id: c_long) -> c_int {
    st(s).memory.get(&id).map_or(-1, |r| (r.building != 0) as c_int)
}

/// The threat layer at a tile (`EECPUnitsDangerAt`).
///
/// # Safety
/// `s` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_danger_at(s: *mut c_void, x: c_long, y: c_long) -> c_long {
    st(s).layer_at(Layer::Threat, x, y)
}

/// The class's data id (`EECPUnitsName`; valid while the state lives).
///
/// # Safety
/// `s` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_class_name(s: *mut c_void, unit_type: c_long) -> *const c_char {
    name_of(st(s), unit_type)
}

/// A binding was called (`EECPUnitsCharge`'s flag).
///
/// # Safety
/// `s` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_mark_used(s: *mut c_void) {
    st(s).used = true;
}

/// `EECPUnitsTake` of the complex unit `unit` by the player `me`.
///
/// # Safety
/// `s` is a live state, `me` the live player, `unit` a live complex unit.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_take(s: *mut c_void, me: *mut c_void, unit: u64) {
    let t = WorldRead::begin();
    if let Some(g) = Game::new(&t, me) {
        if let Some(c) = g.unit(unit) {
            take(&g, st(s), &c);
        }
    }
}

/// The micro step's timer: -1 not started, 0x7fffffff no micro.
///
/// # Safety
/// `s` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_micro_next(s: *mut c_void) -> c_long {
    st(s).micro_next
}

/// Sets the micro step's timer.
///
/// # Safety
/// `s` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_set_micro_next(s: *mut c_void, v: c_long) {
    st(s).micro_next = v;
}

/// Once a game second (`EECPUnitsMicroTick`): the enemy memory, then the danger layers.
///
/// # Safety
/// `s` is a live state, `me` its live player.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_second(s: *mut c_void, me: *mut c_void, now: c_long) {
    let s = st(s);
    if now < s.memory_next {
        return;
    }
    s.memory_next = now + 1000;
    let t = WorldRead::begin();
    if let Some(g) = Game::new(&t, me) {
        remember(&g, s);
        danger_build(&g, s);
    }
}

/// `EECPUnitsOnDamage`: a hit on the unit `unit` by a unit of `attacker_player`.
///
/// # Safety
/// `s` is a live state, `me` its live player, `unit` a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_on_damage(
    s: *mut c_void,
    me: *mut c_void,
    attacker_player: c_long,
    amount: c_long,
    unit: u64,
) {
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me) else { return };
    let Some(u) = g.unit(unit) else { return };
    let mut lost = amount; // an animal's or an ally's hit is no trade (the event still comes)
    match g.player_slot(attacker_player).filter(|_| attacker_player > 0) {
        Some(p) if !g.is_ally(&p) => {}
        _ => lost = 0,
    }
    let id = u.id() as c_long;
    let mut attacker: c_long = 0;
    let last = match u.complex_unit() {
        Some(c) => host(EE_AI_UNITS_HOST_LAST_ATTACKER, me, c.handle() as i64, 0, 0) as u64,
        None => 0,
    };
    if let Some(a) = g.unit(last) {
        if a.hit_points() > 0 && g.seen(&a) {
            attacker = a.id() as c_long;
        }
    }
    let k = category(&u);
    let (x, y) = u.tile_xy();
    st(s).on_damage(
        id,
        attacker_player,
        amount,
        lost,
        attacker,
        k == CAT_LAND_CITIZEN || k == CAT_NAVAL_CITIZEN,
        Game::on_map(&u),
        x as c_long,
        y as c_long,
        now(),
    );
}

/// `EECPUnitsOnDealt`: a hit one of the player's units dealt to `victim`.
///
/// # Safety
/// As [`ee_ai_units_on_damage`].
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_on_dealt(s: *mut c_void, me: *mut c_void, amount: c_long, victim: u64) {
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me) else { return };
    let Some(v) = g.unit(victim) else { return };
    let Some(p) = v.player() else { return };
    if p.handle() == g.me.handle() || p.handle() == g.gaia() || g.is_ally(&p) {
        return;
    }
    let (x, y) = v.tile_xy();
    st(s).trade(Game::on_map(&v), x as c_long, y as c_long, 0, amount, now());
}

/// `EECPUnitsOnPlaced` (`created`), `EECPUnitsOnCompleted` (`built`, a building only) and `EECPUnitsOnGoalDone`
/// (`idle`, a held unit only) by `kind` (EV_CREATED, EV_BUILT, EV_IDLE).
///
/// # Safety
/// `s` is a live state, `unit` a live complex unit.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_on_unit(s: *mut c_void, kind: c_long, unit: u64) {
    let t = WorldRead::begin();
    let Some(u) = UnitView::of(&t, unit) else { return };
    let s = st(s);
    let id = u.id() as c_long;
    match kind {
        EV_CREATED => {
            if let Some(c) = u.class() {
                s.event_add(EV_CREATED, id, 0, c.unit_type() as c_long, 0, now());
            }
        }
        EV_BUILT => {
            if let Some(c) = u.class().filter(is_building) {
                s.event_add(EV_BUILT, id, 0, c.unit_type() as c_long, 0, now());
            }
        }
        EV_IDLE if s.held.contains(&id) => {
            let t = u.class().map_or(0, |c| c.unit_type() as c_long);
            s.event_add(EV_IDLE, id, 0, t, 0, now());
        }
        _ => {}
    }
}

/// `EECPUnitsOnAttackPlayer` (`attack_player`, `defend_player`) and `EECPUnitsOnFlare` (`flare` at x, y).
///
/// # Safety
/// `s` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_on_player(s: *mut c_void, kind: c_long, player: c_long, x: c_long, y: c_long) {
    let s = st(s);
    if kind == EV_FLARE {
        s.event_add(EV_FLARE, 0, player, x, y, now());
    } else {
        s.event_add(kind, 0, player, -1, 0, now());
    }
}

/// `EECPUnitsOnTaken`: the complex unit `unit` left the player (not a swap): died or converted.
///
/// # Safety
/// As [`ee_ai_units_on_damage`].
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_on_taken(s: *mut c_void, me: *mut c_void, unit: u64) {
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me) else { return };
    let Some(u) = g.unit(unit) else { return };
    let id = u.id() as c_long;
    let mut killer: c_long = 0;
    let last = host(EE_AI_UNITS_HOST_LAST_ATTACKER, me, u.handle() as i64, 0, 0) as u64;
    if let Some(a) = g.unit(last) {
        if g.seen(&a) {
            killer = a.id() as c_long;
        }
    }
    // converted: alive and another player's now (a kill still has its hit points here: EEComplexUnit::Kill calls
    // UnitTaken before it zeroes them; AI_RESEARCH §8.41)
    let converted = u.hit_points() > 0 && u.player().is_some_and(|p| p.handle() != g.me.handle());
    let s = st(s);
    s.event_add(
        if converted { EV_CONVERTED } else { EV_DIED },
        id,
        killer,
        u.class().map_or(-1, |c| c.unit_type() as c_long),
        0,
        now(),
    );
    s.held.remove(&id);
}

/// One swap of `EECPUnitsOnSwapped` (the C++ filters the swaps that are not the player's).
///
/// # Safety
/// `s` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_on_swap(
    s: *mut c_void,
    old: c_long,
    new: c_long,
    new_mine: bool,
    new_type: c_long,
    now: c_long,
) {
    st(s).on_swap(old, new, new_mine, new_type, now);
}

/// The swaps' end: forwarding entries past their life go.
///
/// # Safety
/// `s` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_forward_expire(s: *mut c_void, now: c_long) {
    st(s).forward_expire(now);
}

/// The "EENU" block of a state into `out` when `cap` holds it -> its length (call with `cap` 0 to size it).
///
/// # Safety
/// `s` is a live state; `out` has `cap` writable bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_state_save(s: *mut c_void, out: *mut u8, cap: usize) -> usize {
    let bytes = st(s).save();
    if cap >= bytes.len() && !out.is_null() {
        std::ptr::copy_nonoverlapping(bytes.as_ptr(), out, bytes.len());
    }
    bytes.len()
}

/// The state from a block (false: not one; the state is as it was).
///
/// # Safety
/// `s` is a live state; `data` has `len` readable bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_state_load(s: *mut c_void, data: *const u8, len: usize) -> bool {
    let bytes = if len == 0 { &[][..] } else { std::slice::from_raw_parts(data, len) };
    st(s).load(bytes)
}

/// TEST ONLY (EE_AI_SAVE_CHECK): whether the block decodes into a fresh state that encodes to the same bytes;
/// `counts` gets the state's held, events, memory and trades counts.
///
/// # Safety
/// As [`ee_ai_units_state_load`]; `counts` has room for 4.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_state_save_check(
    s: *mut c_void,
    data: *const u8,
    len: usize,
    counts: *mut usize,
) -> bool {
    let s = st(s);
    *counts = s.held.len();
    *counts.add(1) = s.events.len();
    *counts.add(2) = s.memory.len();
    *counts.add(3) = s.trades.len();
    let bytes = if len == 0 { &[][..] } else { std::slice::from_raw_parts(data, len) };
    let mut copy = UnitsState::default();
    copy.load(bytes) && copy.save() == bytes
}

// ---------------------------------------------------------------------------------------------------------------
// The bindings

// the costs in instruction equivalents (AI_LUA_UNITS §2.1)
const COST_CALL: i64 = 200;
const COST_ID: i64 = 20;
const COST_SCAN: i64 = 5;
const COST_RECORD: i64 = 300;
const COST_FIELD: i64 = 15;
const COST_TILE: i64 = 50;

unsafe fn me_of(l: *mut lua_State) -> *mut c_void {
    lua_touserdata(l, lua_upvalueindex(1))
}

/// `EECPUnitsCharge`: the state's used flag, then the charge (it may raise)
unsafe fn charge(l: *mut lua_State, s: *mut UnitsState, amount: i64) {
    (*s).used = true;
    lua_host::charge(l, amount);
}

unsafe fn set_int(l: *mut lua_State, key: &CStr, v: c_long) {
    ee_ai_units_set_int(l.cast(), key.as_ptr(), v);
}
unsafe fn set_str(l: *mut lua_State, key: &CStr, v: *const c_char) {
    ee_ai_units_set_str(l.cast(), key.as_ptr(), v);
}
unsafe fn set_bool(l: *mut lua_State, key: &CStr, v: bool) {
    ee_ai_units_set_bool(l.cast(), key.as_ptr(), v);
}
unsafe fn opt_bool(l: *mut lua_State, table: c_int, key: &CStr, default: bool) -> bool {
    ee_ai_units_opt_bool(l.cast(), table, key.as_ptr(), default)
}
unsafe fn opt_int(l: *mut lua_State, table: c_int, key: &CStr, out: &mut c_long) -> bool {
    ee_ai_units_opt_int(l.cast(), table, key.as_ptr(), out)
}
unsafe fn is_int(l: *mut lua_State, i: c_int) -> bool {
    lua_isinteger(l, i) != 0
}
unsafe fn int_at(l: *mut lua_State, i: c_int) -> c_long {
    lua_tointeger(l, i) as c_long
}
unsafe fn read_ids(l: *mut lua_State, index: c_int, out: &mut Vec<c_long>) {
    out.resize(MAX_IDS, 0);
    let mut n = 0usize;
    ee_ai_units_read_ids(l.cast(), index, out.as_mut_ptr(), &mut n);
    out.truncate(n);
}
unsafe fn push_sorted(l: *mut lua_State, s: *mut UnitsState, limit: c_long) {
    let v = &mut (*s).sorted;
    ee_ai_units_push_sorted(l.cast(), v.as_mut_ptr(), v.len(), limit);
}

fn new_filter() -> EeAiUnitsFilter {
    EeAiUnitsFilter {
        kind_type: -1,
        types: [0; 16],
        type_count: 0,
        category: std::ptr::null(),
        has_near: false,
        x: 0,
        y: 0,
        r: 0,
        idle: -1,
        built: -1,
        armed: -1,
        held: std::ptr::null(),
        action: std::ptr::null(),
        player: -1,
        enemy: -1,
        kind: -1,
        limit: -1,
    }
}

unsafe fn read_filter(l: *mut lua_State, table: c_int) -> EeAiUnitsFilter {
    let mut f = new_filter();
    ee_ai_units_read_filter(l.cast(), table, &mut f, class_by_name);
    f
}

/// `EECPUnitsTypeMatch`
fn type_match(f: &EeAiUnitsFilter, t: c_long) -> bool {
    if f.kind_type == -1 {
        return true;
    }
    if f.kind_type != -3 {
        return t == f.kind_type;
    }
    f.types[..f.type_count.clamp(0, 16) as usize].contains(&t)
}

unsafe fn cstr_eq(a: *const c_char, b: &CStr) -> bool {
    CStr::from_ptr(a) == b
}

/// `EECPUnitsPasses` -> the sort key (the squared distance with near, else 0)
unsafe fn passes(u: &UnitView<'_>, f: &EeAiUnitsFilter) -> Option<i64> {
    let t = u.class().map_or(-1, |c| c.unit_type() as c_long);
    if !type_match(f, t) {
        return None;
    }
    if !f.category.is_null() && !cstr_eq(f.category, unit_category(u)) {
        return None;
    }
    if f.armed >= 0 && armed(u) as c_int != f.armed {
        return None;
    }
    if f.has_near {
        if !Game::on_map(u) {
            return None;
        }
        let (x, y) = u.tile_xy();
        let d = d2(x as c_long, y as c_long, f.x, f.y);
        if d > f.r.wrapping_mul(f.r) {
            return None;
        }
        return Some(d as i64);
    }
    Some(0)
}

macro_rules! game {
    ($l:expr, $t:ident, $g:ident, $s:ident) => {
        let me_ptr = me_of($l);
        let $s = state_of(me_ptr);
        let $t = WorldRead::begin();
        let Some($g) = Game::new(&$t, me_ptr) else {
            lua_pushnil($l);
            return 1;
        };
    };
}

/// `ai.me()`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_me(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let t = WorldRead::begin();
    let id = PlayerView::of(&t, me_of(l) as u64).map_or(0, |p| p.id());
    lua_pushinteger(l, id as lua_Integer);
    1
}

/// `ai.time()`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_time(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    lua_pushinteger(l, now() as lua_Integer);
    1
}

/// `ai.players()`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_players(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    game!(l, t, g, s);
    charge(l, s, COST_CALL + 16 * COST_RECORD / 10);
    lua_newtable(l);
    let mut n: lua_Integer = 0;
    let teams = host(EE_AI_UNITS_HOST_TEAMS, g.me_ptr, 0, 0, 0);
    let np = g.world.num_players();
    for p in 1..np.min(64) {
        let Some(other) = g.world.player(p as usize) else { continue };
        if other.observer() {
            continue;
        }
        let oid = other.id() as c_long;
        let mine = other.handle() == g.me.handle();
        lua_newtable(l);
        set_int(l, c"id", oid);
        set_bool(l, c"me", mine);
        set_bool(l, c"ally", !mine && g.is_ally(&other));
        set_bool(l, c"enemy", !g.is_ally(&other));
        set_bool(l, c"hostile", !mine && other.enemy(g.me.id() as usize) != 0); // their stance towards me
                                                                                // the teams the player is in (both sides allied): the other players of those teams
        lua_newtable(l);
        let mut na: lua_Integer = 0;
        for team in 0..teams {
            if host(EE_AI_UNITS_HOST_TEAM_HAS, g.me_ptr, team, oid as i64, 0) == 0 {
                continue;
            }
            let size = host(EE_AI_UNITS_HOST_TEAM_SIZE, g.me_ptr, team, 0, 0);
            for k in 0..size {
                let q = host(EE_AI_UNITS_HOST_TEAM_PLAYER, g.me_ptr, team, k, 0) as u64;
                if q != oid as u64 && q > 0 && q < np && g.world.player(q as usize).is_some() {
                    lua_pushinteger(l, q as lua_Integer);
                    na += 1;
                    lua_rawseti(l, -2, na);
                }
            }
        }
        lua_setfield(l, -2, c"allies".as_ptr());
        set_bool(l, c"alive", other.first_unit().is_some());
        set_bool(l, c"human", host(EE_AI_UNITS_HOST_IS_HUMAN, g.me_ptr, other.handle() as i64, 0, 0) != 0);
        n += 1;
        lua_rawseti(l, -2, n);
    }
    1
}

/// `ai.map()`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_map(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let s = state_of(me_of(l));
    charge(l, s, COST_CALL);
    lua_newtable(l);
    let (w, h) = map_size();
    set_int(l, c"width", w);
    set_int(l, c"height", h);
    1
}

/// `ai.units(filter)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_units(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    game!(l, t, g, s);
    let f = read_filter(l, 1);
    (*s).sorted.clear();
    let mut tested: i64 = 0;
    for u in g.me.units() {
        tested += 1;
        let Some(c) = u.complex_unit() else { continue };
        if !(u.hit_points() > 0) || !u.valid() || is_projectile(&u) {
            continue;
        }
        let Some(key) = passes(&u, &f) else { continue };
        if f.idle >= 0 && is_idle(&c) as c_int != f.idle {
            continue;
        }
        if f.built >= 0 && c.is_built() as c_int != f.built {
            continue;
        }
        if !f.held.is_null() && cstr_eq(f.held, c"lua") != (*s).held.contains(&(u.id() as c_long)) {
            continue;
        }
        if !f.action.is_null() && !cstr_eq(f.action, action(&c)) {
            continue;
        }
        (*s).sorted.push(EeAiUnitsSortEntry { key, id: u.id() as c_long });
    }
    charge(l, s, COST_CALL + tested * COST_SCAN + (*s).sorted.len() as i64 * COST_ID);
    push_sorted(l, s, f.limit);
    1
}

/// `ai.seen(filter)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_seen(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    game!(l, t, g, s);
    let f = read_filter(l, 1);
    (*s).sorted.clear();
    let mut tested: i64 = 0;
    for p in 1..g.world.num_players().min(64) {
        let Some(other) = g.world.player(p as usize) else { continue };
        if other.handle() == g.me.handle() || (f.player >= 0 && p as c_long != f.player) {
            continue;
        }
        if f.enemy >= 0 && (!g.is_ally(&other)) as c_int != f.enemy {
            continue;
        }
        for u in other.units() {
            tested += 1;
            let Some(c) = u.complex_unit() else { continue };
            if !(u.hit_points() > 0) || c.container_handle() != 0 || is_projectile(&u) || !g.seen(&u) {
                continue;
            }
            let Some(key) = passes(&u, &f) else { continue };
            (*s).sorted.push(EeAiUnitsSortEntry { key, id: u.id() as c_long });
        }
    }
    charge(l, s, COST_CALL + tested * COST_SCAN + (*s).sorted.len() as i64 * COST_ID);
    push_sorted(l, s, f.limit);
    1
}

/// `ai.resources(filter)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_resources(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    game!(l, t, g, s);
    let f = read_filter(l, 1);
    let room = opt_bool(l, 1, c"has_room", false);
    (*s).sorted.clear();
    let mut tested: i64 = 0;
    if let Some(gaia) = g.world.player(0) {
        for u in gaia.units() {
            tested += 1;
            if !g.is_resource(&u) || !g.explored(&u) {
                continue;
            }
            if f.kind >= 0 && u.resource_type() as c_long - RESOURCE_FOOD as c_long != f.kind {
                continue;
            }
            let Some(key) = passes(&u, &f) else { continue };
            if room && host(EE_AI_UNITS_HOST_HAS_ROOM, g.me_ptr, u.handle() as i64, 0, 0) == 0 {
                continue;
            }
            (*s).sorted.push(EeAiUnitsSortEntry { key, id: u.id() as c_long });
        }
    }
    charge(l, s, COST_CALL + tested * COST_SCAN + (*s).sorted.len() as i64 * COST_ID);
    push_sorted(l, s, f.limit);
    1
}

/// `EECPUnitsPushRecord`: one unit's record (the fields by what the player may know: AI_LUA_UNITS §2.3)
unsafe fn push_record(l: *mut lua_State, g: &Game<'_>, s: *mut UnitsState, u: &UnitView<'_>) {
    let owner = u.player().map_or(0, |p| p.handle());
    let own = owner == g.me.handle() || (owner == 0 && g.pending(u.id() as c_long).is_some());
    let ally = !own && owner != 0 && owner != g.gaia() && u.player().is_some_and(|p| g.is_ally(&p));
    let c = u.complex_unit();
    let class = u.class();
    lua_createtable(l, 0, 24);
    set_int(l, c"id", u.id() as c_long);
    set_str(l, c"type", name_of(&mut *s, class.map_or(0, |k| k.unit_type() as c_long)));
    set_str(l, c"category", unit_category(u).as_ptr());
    set_int(l, c"player", u.player().map_or(0, |p| p.id() as c_long));
    let (x, y) = u.tile_xy();
    set_int(l, c"x", x as c_long);
    set_int(l, c"y", y as c_long);
    if let Some(c) = &c {
        let at = c.location();
        set_int(l, c"px", centi(at[0]));
        set_int(l, c"py", centi(at[1]));
    }
    if g.is_resource(u) {
        let r = (u.resource_type() - RESOURCE_FOOD) as usize;
        set_str(l, c"kind", RESOURCE_KINDS[r].as_ptr());
        set_int(l, c"amount", u.resource_amount() as c_long);
        // a resource's hit points stand at 1 until it is first gathered from
        set_bool(l, c"untapped", u.hit_points() > 0);
        set_int(l, c"workers", host(EE_AI_UNITS_HOST_ASSIGNED, g.me_ptr, u.handle() as i64, 0, 0) as c_long);
        set_bool(l, c"has_room", host(EE_AI_UNITS_HOST_HAS_ROOM, g.me_ptr, u.handle() as i64, 0, 0) != 0);
        return;
    }
    set_int(l, c"hp", u.hit_points() as c_long);
    if let Some(st) = u.stats() {
        set_int(l, c"hp_max", st.max_hit_points() as c_long);
        set_int(l, c"attack", um::lrintf(attack_of(&st)) as c_long);
        set_int(l, c"range", centi(st.range()));
        set_int(l, c"sight", centi(crate::world_field!(st.fields(), mLineOfSight)));
        set_int(l, c"speed", centi(crate::world_field!(st.fields(), mMaxVelocity)));
    }
    set_bool(l, c"armed", armed(u));
    let Some(c) = c else { return };
    set_bool(l, c"built", c.is_built());
    let building = c.class().is_some_and(|k| is_building(&k));
    if !Game::on_map(u) && building && !c.is_built() {
        set_bool(l, c"pending", true); // a foundation the builders have not reached (ai.build's id)
    }
    let mut act = action(&c);
    if !own && !ally {
        // what a human sees of another player's unit: moving, attacking or standing
        if act != c"move" && act != c"attack" && act != c"idle" {
            act = c"busy";
        }
        set_str(l, c"action", act.as_ptr());
        return;
    }
    set_str(l, c"action", act.as_ptr());
    let mut f = EeAiUnitsFacts::default();
    host_out(EE_AI_UNITS_HOST_FACTS, g.me_ptr, c.handle() as i64, (&mut f as *mut EeAiUnitsFacts).cast());
    if f.has_goal {
        set_int(l, c"tx", f.tx);
        set_int(l, c"ty", f.ty);
        if f.gathers >= 0 {
            set_str(l, c"gathers", RESOURCE_KINDS[f.gathers as usize].as_ptr());
        }
        if f.waiting {
            set_bool(l, c"waiting", true);
        }
        if f.stalled {
            set_bool(l, c"stalled", true);
        }
        if f.target != 0 {
            set_int(l, c"target", f.target);
        }
    }
    if c.container_handle() != 0 {
        set_int(l, c"inside", c.container().map_or(0, |k| k.id() as c_long));
    }
    if own && f.home_base != 0 {
        set_int(l, c"base", f.home_base);
    }
    if f.moves {
        if f.crosses_forest {
            set_bool(l, c"crosses_forest", true);
        }
        if f.crosses_cliff {
            set_bool(l, c"crosses_cliff", true);
        }
    }
    set_int(l, c"cargo", c.cargo_len() as c_long);
    set_int(l, c"garrisoned", f.garrisoned);
    if building {
        let producing = c.current_research() != NO_RESEARCH;
        set_int(l, c"queue", c.build_queue_len() as c_long + producing as c_long);
        if producing {
            set_int(l, c"producing", c.current_research() as c_long);
        }
        let n = c.build_queue_len();
        if n > 0 {
            lua_createtable(l, n as c_int, 0);
            for k in 0..n {
                lua_pushinteger(l, c.build_queue_technology(k).unwrap_or(0) as lua_Integer);
                lua_rawseti(l, -2, k as lua_Integer + 1);
            }
            lua_setfield(l, -2, c"queued".as_ptr());
        }
    }
    set_int(l, c"stance", f.stance);
    if own {
        let held = (*s).held.contains(&(u.id() as c_long));
        set_str(l, c"held", if held { c"lua".as_ptr() } else { c"engine".as_ptr() });
        let last = host(EE_AI_UNITS_HOST_LAST_ATTACKER, g.me_ptr, c.handle() as i64, 0, 0) as u64;
        if let Some(a) = g.unit(last) {
            if a.hit_points() > 0 && g.seen(&a) {
                set_int(l, c"attacker", a.id() as c_long);
            }
        }
    }
}

/// `ai.unit(id)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_unit(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_RECORD);
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else {
        lua_pushnil(l);
        return 1;
    };
    if !is_int(l, 1) {
        lua_pushnil(l);
        return 1;
    }
    match g.known(&*s, int_at(l, 1)) {
        None => lua_pushnil(l),
        Some(u) => push_record(l, &g, s, &u),
    }
    1
}

/// `ai.get(ids, fields)` -> {field = {values...}}; a unit not known: nil in each
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_get(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    game!(l, t, g, s);
    read_ids(l, 1, &mut (*s).ids);
    let fields = if lua_type(l, 2) == LUA_TTABLE { lua_rawlen(l, 2) as c_long } else { 0 };
    charge(l, s, COST_CALL + (*s).ids.len() as i64 * (fields as i64 * COST_FIELD + COST_ID));
    lua_newtable(l); // the result
    let result = lua_gettop(l);
    for f in 1..=fields {
        lua_rawgeti(l, 2, f as lua_Integer);
        lua_newtable(l);
        lua_settable(l, result);
    }
    let n = (*s).ids.len();
    for k in 0..n {
        let id = (&(*s).ids)[k];
        let Some(u) = g.known(&*s, id) else { continue };
        push_record(l, &g, s, &u);
        let record = lua_gettop(l);
        for f in 1..=fields {
            lua_rawgeti(l, 2, f as lua_Integer); // the field's name
            lua_pushvalue(l, -1);
            lua_gettable(l, result); // its column
            lua_insert(l, -2);
            lua_gettable(l, record); // the value
            lua_rawseti(l, -2, k as lua_Integer + 1);
            lua_pop(l, 1);
        }
        lua_pop(l, 1);
    }
    1
}

/// `ai.memory(filter)`: the enemy memory
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_memory(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    game!(l, t, g, s);
    let f = read_filter(l, 1);
    let mut since: c_long = 0;
    opt_int(l, 1, c"since", &mut since);
    let mut buildings: c_int = -1;
    if lua_type(l, 1) == LUA_TTABLE {
        lua_getfield(l, 1, c"buildings".as_ptr());
        if !lua_isnil(l, -1) {
            buildings = (lua_toboolean(l, -1) != 0) as c_int;
        }
        lua_pop(l, 1);
    }
    let st = &mut *s;
    st.sorted.clear();
    for (&id, r) in &st.memory {
        if (f.player >= 0 && r.player != f.player)
            || !type_match(&f, r.unit_type)
            || r.seen < since
            || (buildings >= 0 && r.building != buildings as c_long)
        {
            continue;
        }
        if !f.category.is_null() {
            let class = if r.unit_type < 0 { None } else { ClassView::by_index(g.t, r.unit_type as usize) };
            match class {
                Some(c) if cstr_eq(f.category, category_class(&c, false)) => {}
                _ => continue,
            }
        }
        let mut key: i64 = 0;
        if f.has_near {
            let d = d2(r.x, r.y, f.x, f.y);
            if d > f.r.wrapping_mul(f.r) {
                continue;
            }
            key = d as i64;
        }
        st.sorted.push(EeAiUnitsSortEntry { key, id });
    }
    st.sorted.sort_unstable();
    let mut n = st.sorted.len() as c_long;
    if f.limit >= 0 && n > f.limit {
        n = f.limit;
    }
    charge(l, s, COST_CALL + (*s).memory.len() as i64 * COST_SCAN + n as i64 * COST_RECORD / 3);
    lua_createtable(l, n as c_int, 0);
    for k in 0..n as usize {
        let id = (&(*s).sorted)[k].id;
        let r = (*s).memory.get(&id).copied().unwrap_or_default();
        lua_createtable(l, 0, 8);
        set_int(l, c"id", id);
        set_int(l, c"player", r.player);
        set_str(l, c"type", name_of(&mut *s, r.unit_type));
        set_int(l, c"x", r.x);
        set_int(l, c"y", r.y);
        set_int(l, c"hp", r.hp);
        set_int(l, c"seen", r.seen);
        set_bool(l, c"building", r.building != 0);
        lua_rawseti(l, -2, k as lua_Integer + 1);
    }
    1
}

/// `ai.tile(x, y)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_tile(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    game!(l, t, g, s);
    charge(l, s, COST_TILE);
    let map = g.world.map();
    if !is_int(l, 1) || !is_int(l, 2) || map.is_none() {
        lua_pushnil(l);
        return 1;
    }
    let (x, y) = (int_at(l, 1), int_at(l, 2));
    let tile = if x >= 0 && y >= 0 { map.and_then(|m| m.tile(x as i64, y as i64)) } else { None };
    let Some(tile) = tile else {
        lua_pushnil(l);
        return 1;
    };
    let explored = g.tile_explored(&tile);
    lua_createtable(l, 0, 6);
    set_bool(l, c"explored", explored);
    set_bool(l, c"visible", g.tile_seen(&tile));
    if explored {
        set_int(l, c"continent", continent(x, y, false) as c_long);
        set_int(l, c"sea", continent(x, y, true) as c_long);
        set_int(l, c"terrain", tile.terrain_type() as c_long);
    }
    1
}

/// `ai.events()`: the events since the last call
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_events(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    let n = (*s).events.len();
    charge(l, s, COST_CALL + n as i64 * COST_ID * 3);
    lua_createtable(l, n as c_int, 0);
    for k in 0..n {
        let e = (&(*s).events)[k];
        lua_createtable(l, 0, 6);
        set_str(l, c"kind", EVENT_NAMES[e.kind.clamp(0, 10) as usize].as_ptr());
        set_int(l, c"unit", e.unit);
        set_int(l, c"time", e.time);
        match e.kind {
            EV_ATTACKED => {
                if e.other != 0 {
                    set_int(l, c"attacker", e.other);
                }
                set_int(l, c"player", e.unit_type);
                set_int(l, c"damage", e.amount);
            }
            EV_DIED | EV_CONVERTED => {
                if e.other != 0 {
                    set_int(l, c"killer", e.other);
                }
                if e.unit_type >= 0 {
                    set_str(l, c"type", name_of(&mut *s, e.unit_type));
                }
            }
            EV_ATTACK_PLAYER | EV_DEFEND_PLAYER => set_int(l, c"player", e.other),
            EV_FLARE => {
                set_int(l, c"player", e.other);
                set_int(l, c"x", e.unit_type);
                set_int(l, c"y", e.amount);
            }
            EV_SWAPPED => {
                set_int(l, c"new", e.other);
                set_str(l, c"type", name_of(&mut *s, e.unit_type));
            }
            _ => {
                if e.unit_type >= 0 {
                    set_str(l, c"type", name_of(&mut *s, e.unit_type));
                }
            }
        }
        lua_rawseti(l, -2, k as lua_Integer + 1);
    }
    (*s).events.clear();
    (*s).attacked_at.clear();
    if (*s).dropped > 0 {
        trace(me_ptr, c"next.events", &format!("dropped={} (at most {} between deliveries)", (*s).dropped, MAX_EVENTS));
        (*s).dropped = 0;
    }
    1
}

/// `ai.reach(x1, y1, x2, y2)`: both tiles on one land mass (a tile on none takes its nearest within 3)
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_reach(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let s = state_of(me_of(l));
    charge(l, s, COST_TILE * 2);
    for i in 1..=4 {
        if !is_int(l, i) {
            lua_pushboolean(l, 0);
            return 1;
        }
    }
    let ca = area_near(int_at(l, 1), int_at(l, 2), false);
    let ok = ca != 0 && ca == area_near(int_at(l, 3), int_at(l, 4), false);
    lua_pushboolean(l, ok as c_int);
    1
}

/// `ai.strength(ids)`: hit points x attack of the units the player may know of
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_strength(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    game!(l, t, g, s);
    read_ids(l, 1, &mut (*s).ids);
    charge(l, s, COST_CALL + (*s).ids.len() as i64 * COST_FIELD);
    let mut sum: i64 = 0;
    for k in 0..(*s).ids.len() {
        if let Some(u) = g.known(&*s, (&(*s).ids)[k]) {
            sum += strength(&u) as i64;
        }
    }
    lua_pushinteger(l, sum as lua_Integer);
    1
}

unsafe fn hold(l: *mut lua_State, take_it: bool) -> c_int {
    game!(l, t, g, s);
    let work = !take_it && opt_bool(l, 2, c"gather", false);
    let _ = work;
    read_ids(l, 1, &mut (*s).ids);
    charge(l, s, COST_CALL + (*s).ids.len() as i64 * COST_ID);
    let mut n: c_long = 0;
    for k in 0..(*s).ids.len() {
        let Some(c) = g.own(&*s, (&(*s).ids)[k]) else { continue };
        if take_it {
            take(&g, &mut *s, &c);
        } else {
            release(&g, &mut *s, &c);
        }
        n += 1;
    }
    lua_pushinteger(l, n as lua_Integer);
    1
}

/// `ai.take(ids)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_take(l: *mut c_void) -> c_int {
    hold(l as *mut lua_State, true)
}

/// `ai.release(ids)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_release(l: *mut c_void) -> c_int {
    hold(l as *mut lua_State, false)
}

/// `ai.reflex{naval_flee, danger, militia, held, memory_ms}`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_reflex(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let s = state_of(me_of(l));
    charge(l, s, COST_CALL);
    (*s).reflex_naval_flee = opt_bool(l, 1, c"naval_flee", (*s).reflex_naval_flee);
    (*s).reflex_danger = opt_bool(l, 1, c"danger", (*s).reflex_danger);
    (*s).reflex_militia = opt_bool(l, 1, c"militia", (*s).reflex_militia);
    (*s).reflex_held = opt_bool(l, 1, c"held", (*s).reflex_held);
    let mut ms: c_long = 0;
    if opt_int(l, 1, c"memory_ms", &mut ms) && (0..=3_600_000).contains(&ms) {
        (*s).memory_ms = ms;
    }
    0
}

/// `ai.config{memory_ms}` -> memory_ms (30 s to 30 minutes)
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_config(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let s = state_of(me_of(l));
    charge(l, s, COST_CALL);
    let mut v: c_long = 0;
    if opt_int(l, 1, c"memory_ms", &mut v) {
        (*s).memory_ms = v.clamp(30000, 1_800_000);
    }
    lua_pushinteger(l, (*s).memory_ms as lua_Integer);
    1
}

/// The layer an optional string argument names (None: unknown)
unsafe fn layer_arg(l: *mut lua_State, index: c_int) -> Option<Layer> {
    match ee_ai_units_layer_arg(l.cast(), index) {
        0 => Some(Layer::Threat),
        1 => Some(Layer::Lost),
        2 => Some(Layer::Influence),
        _ => None,
    }
}

/// `ai.danger(x, y[, layer])`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_danger(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let s = state_of(me_of(l));
    charge(l, s, COST_TILE);
    let layer = layer_arg(l, 3);
    match layer {
        Some(layer) if is_int(l, 1) && is_int(l, 2) => {
            lua_pushinteger(l, (*s).layer_at(layer, int_at(l, 1), int_at(l, 2)) as lua_Integer)
        }
        _ => lua_pushinteger(l, 0),
    }
    1
}

/// `ai.danger_box(x1, y1, x2, y2[, layer])` -> the highest and the summed value of the cells over the box
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_danger_box(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let s = state_of(me_of(l));
    let layer = layer_arg(l, 5);
    for i in 1..=4 {
        if !is_int(l, i) || (*s).danger_w <= 0 || layer.is_none_or(|y| (*s).layer(y).is_empty()) {
            lua_pushinteger(l, 0);
            lua_pushinteger(l, 0);
            return 2;
        }
    }
    let Some(layer) = layer else { return 0 };
    let (max, sum, cells) = (*s).danger_box(layer, int_at(l, 1), int_at(l, 2), int_at(l, 3), int_at(l, 4));
    charge(l, s, COST_CALL + cells as i64 * COST_SCAN);
    lua_pushinteger(l, max as lua_Integer);
    lua_pushinteger(l, sum as lua_Integer);
    2
}

/// `ai.mark(x, y, r, value, ms)`: the script's own danger (at most 64)
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_mark(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let s = state_of(me_of(l));
    charge(l, s, COST_CALL);
    for i in 1..=5 {
        if !is_int(l, i) {
            lua_pushboolean(l, 0);
            return 1;
        }
    }
    let (r, ms) = (lua_tointeger(l, 3), lua_tointeger(l, 5));
    if (*s).marks.len() >= 64 || !(0..=40).contains(&r) || ms <= 0 || ms > 3_600_000 {
        lua_pushboolean(l, 0);
        return 1;
    }
    let v = int_at(l, 4);
    let m = DangerMark {
        x: int_at(l, 1),
        y: int_at(l, 2),
        r: r as c_long,
        value: v.clamp(0, 30000),
        until: now() + ms as c_long,
    };
    (*s).mark(m);
    lua_pushboolean(l, 1);
    1
}

/// `EECPUnitsRouteArgs`: the tiles at 1-4 and the options at `opts` -> (x1, y1, x2, y2, avoid, avoid_lost, sea)
unsafe fn route_args(l: *mut lua_State, opts: c_int, g: &Game<'_>, s: *mut UnitsState) -> Option<[c_long; 7]> {
    for i in 1..=4 {
        if !is_int(l, i) {
            return None;
        }
    }
    let (x1, y1, x2, y2) = (int_at(l, 1), int_at(l, 2), int_at(l, 3), int_at(l, 4));
    let mut avoid: c_long = 4;
    let mut avoid_lost: c_long = 0;
    opt_int(l, opts, c"avoid", &mut avoid);
    opt_int(l, opts, c"avoid_lost", &mut avoid_lost);
    let avoid = avoid.clamp(0, 100);
    let avoid_lost = avoid_lost.clamp(0, 100);
    let sea = opt_bool(l, opts, c"sea", false);
    let map = g.world.map()?;
    let ok = |x: c_long, y: c_long| x >= 0 && y >= 0 && map.tile(x as i64, y as i64).is_some();
    if !ok(x1, y1) || !ok(x2, y2) {
        return None;
    }
    if (*s).danger_w <= 0 {
        danger_build(g, &mut *s);
    }
    Some([x1, y1, x2, y2, avoid, avoid_lost, sea as c_long])
}

unsafe fn route(s: *mut UnitsState, a: &[c_long; 7], avoid: c_long, avoid_lost: c_long) -> (bool, c_long) {
    let sea = a[6] != 0;
    let area = area_near(a[0], a[1], sea);
    if area == 0 || area_near(a[2], a[3], sea) != area {
        return (false, 0);
    }
    (*s).cell_route(a[0], a[1], a[2], a[3], avoid, avoid_lost, |cx, cy| cell_open(cx, cy, sea, area))
}

/// `ai.route(x1, y1, x2, y2, {avoid, avoid_lost, sea})` -> waypoints and the cost; nil: no way
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_route(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return 0 };
    let Some(a) = route_args(l, 5, &g, s) else { return 0 };
    charge(l, s, 5000);
    let (found, expanded) = route(s, &a, a[4], a[5]);
    charge(l, s, expanded as i64 * 10);
    if !found {
        return 0;
    }
    let w = (*s).danger_w;
    let goal = *(*s).ids.last().unwrap_or(&0);
    (*s).route_corners();
    let n = (*s).ids2.len();
    lua_createtable(l, n as c_int + 1, 0);
    for k in 0..=n {
        lua_createtable(l, 2, 0);
        let x = if k < n { ((&(*s).ids2)[k] % w) * DANGER_CELL + DANGER_CELL / 2 } else { a[2] };
        let y = if k < n { ((&(*s).ids2)[k] / w) * DANGER_CELL + DANGER_CELL / 2 } else { a[3] };
        lua_pushinteger(l, x as lua_Integer);
        lua_rawseti(l, -2, 1);
        lua_pushinteger(l, y as lua_Integer);
        lua_rawseti(l, -2, 2);
        lua_rawseti(l, -2, k as lua_Integer + 1);
    }
    lua_pushinteger(l, (&(*s).g)[goal as usize] as lua_Integer);
    2
}

/// `ai.path_len(x1, y1, x2, y2, {sea})` -> the way's length in tiles over the cells (no danger), nil: none
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_path_len(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return 0 };
    let Some(a) = route_args(l, 5, &g, s) else { return 0 };
    charge(l, s, 2000);
    let (found, expanded) = route(s, &a, 0, 0);
    charge(l, s, expanded as i64 * 10);
    if !found {
        return 0;
    }
    let cost = (&(*s).g)[*(*s).ids.last().unwrap_or(&0) as usize];
    let len = cost * DANGER_CELL / 10;
    let dx = (a[0] - a[2]).abs();
    let dy = (a[1] - a[3]).abs();
    let straight = dx.max(dy);
    lua_pushinteger(l, (if len < straight { straight } else { len }) as lua_Integer);
    1
}

/// `ai.gatherers()` -> {[resource id] = n}: the own living citizens and boats whose gather goal is that resource
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_gatherers(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    game!(l, t, g, s);
    let mut counts: std::collections::BTreeMap<c_long, c_long> = std::collections::BTreeMap::new();
    let mut tested: i64 = 0;
    for u in g.me.units() {
        tested += 1;
        let Some(c) = u.complex_unit() else { continue };
        let k = category(&c);
        if !(u.hit_points() > 0) || !(k == CAT_LAND_CITIZEN || k == CAT_NAVAL_CITIZEN) {
            continue;
        }
        let id = host(EE_AI_UNITS_HOST_GATHER_TARGET, g.me_ptr, c.handle() as i64, 0, 0) as c_long;
        if id != 0 {
            *counts.entry(id).or_insert(0) += 1;
        }
    }
    // (the map is a local: a charge over the budget leaks its nodes once, as no Lua error is expected here)
    let len = counts.len();
    let pairs: Vec<(c_long, c_long)> = counts.into_iter().collect();
    (*s).sorted.clear();
    (*s).sorted.extend(pairs.iter().map(|&(id, n)| EeAiUnitsSortEntry { key: n as i64, id }));
    drop(pairs);
    charge(l, s, COST_CALL + tested * COST_SCAN + len as i64 * COST_ID);
    lua_createtable(l, 0, len as c_int);
    for k in 0..len {
        let e = (&(*s).sorted)[k];
        lua_pushinteger(l, e.key as lua_Integer);
        lua_rawseti(l, -2, e.id as lua_Integer);
    }
    1
}

// ---------------------------------------------------------------------------------------------------------------
// The commands: a human's messages run on the computer player's selection (AI_LUA_UNITS §3)

/// The selection (`mActiveGroup`) cleared (0: the player has none).
pub const EE_AI_UNITS_HOST_SELECT_BEGIN: i32 = 30;
/// The complex unit `a` added to the selection.
pub const EE_AI_UNITS_HOST_SELECT_ADD: i32 = 31;
/// The message `out` (an `EeAiUnitsMessage`) run (its mode says how).
pub const EE_AI_UNITS_HOST_EXEC: i32 = 32;
/// `EECPNextTechByName(a)` (a NUL-terminated name).
pub const EE_AI_UNITS_HOST_TECH_BY_NAME: i32 = 33;
/// `player->EEPlayer::CanProduce(a, 0x20, 1, true, 3, NULL)`.
pub const EE_AI_UNITS_HOST_CAN_PRODUCE: i32 = 34;
/// `EECanPlaceOnTile(player, a, b, c, false, 1)`.
pub const EE_AI_UNITS_HOST_CAN_PLACE: i32 = 35;
/// The class `a`'s `IsTransport()`.
pub const EE_AI_UNITS_HOST_IS_TRANSPORT: i32 = 36;
/// `a` indexes a behaviour of `EEUnitBehavior::sTable` (in range, not NULL).
pub const EE_AI_UNITS_HOST_BEHAVIOR_OK: i32 = 37;
/// `player->mTechnologies->Get(a) != NULL`.
pub const EE_AI_UNITS_HOST_HAS_NODE: i32 = 38;
/// The class `a`'s `IsLandBuilding()`.
pub const EE_AI_UNITS_HOST_LAND_BUILDING: i32 = 39;

/// `EEMRecord`'s members as a command sends them: an integer (`kind` 0), a float (1) or a point (2: `x`, `y`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeAiUnitsMsgValue {
    pub kind: i32,
    pub i: i64,
    pub x: f32,
    pub y: f32,
}

/// A command's message (`EE_AI_UNITS_HOST_EXEC`): `message` one of `EE_AI_UNITS_MSG_*`, `mode` 0 run on the
/// selection under the borrowed slot (`EECPUnitsRun`), 1 run as it is (`Execute`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeAiUnitsMessage {
    pub message: i32,
    pub mode: i32,
    pub count: i32,
    pub values: [EeAiUnitsMsgValue; 8],
}

pub const EE_AI_UNITS_MSG_PATH_TO: i32 = 1;
pub const EE_AI_UNITS_MSG_ATTACK_UNIT: i32 = 2;
pub const EE_AI_UNITS_MSG_ATTACK_GROUND: i32 = 3;
pub const EE_AI_UNITS_MSG_GATHER: i32 = 4;
pub const EE_AI_UNITS_MSG_BUILD: i32 = 5;
pub const EE_AI_UNITS_MSG_REPAIR: i32 = 6;
pub const EE_AI_UNITS_MSG_LOAD_GARRISON: i32 = 7;
pub const EE_AI_UNITS_MSG_LOAD_TRANSPORT: i32 = 8;
pub const EE_AI_UNITS_MSG_UNLOAD: i32 = 9;
pub const EE_AI_UNITS_MSG_RESEARCH: i32 = 10;
pub const EE_AI_UNITS_MSG_BEHAVIOR: i32 = 11;
pub const EE_AI_UNITS_MSG_STOP: i32 = 12;
pub const EE_AI_UNITS_MSG_EXPLORE: i32 = 13;
pub const EE_AI_UNITS_MSG_KILL: i32 = 14;
pub const EE_AI_UNITS_MSG_HEAL: i32 = 15;
pub const EE_AI_UNITS_MSG_RALLY: i32 = 16;
pub const EE_AI_UNITS_MSG_REMOVE_TECH: i32 = 17;
pub const EE_AI_UNITS_MSG_FORMATION: i32 = 18;
pub const EE_AI_UNITS_MSG_PACK: i32 = 19;
pub const EE_AI_UNITS_MSG_DROPOFF: i32 = 20;
pub const EE_AI_UNITS_MSG_REPLANT: i32 = 21;
pub const EE_AI_UNITS_MSG_SMART_FARMERS: i32 = 22;
pub const EE_AI_UNITS_MSG_CAST: i32 = 23;
pub const EE_AI_UNITS_MSG_PATROL_POINT: i32 = 24;
pub const EE_AI_UNITS_MSG_LOCK: i32 = 25;

/// The message record's size (the C++ checks its copy of the layout against it).
///
/// # Safety
/// None: `m` is not read.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_units_message_size(m: *const EeAiUnitsMessage) -> usize {
    let _ = m;
    std::mem::size_of::<EeAiUnitsMessage>()
}

enum V {
    I(i64),
    F(f32),
    P(f32, f32),
}

fn message(id: i32, mode: i32, values: &[V]) -> EeAiUnitsMessage {
    let mut m = EeAiUnitsMessage { message: id, mode, count: values.len() as i32, ..Default::default() };
    for (k, v) in values.iter().enumerate() {
        m.values[k] = match *v {
            V::I(i) => EeAiUnitsMsgValue { kind: 0, i, ..Default::default() },
            V::F(f) => EeAiUnitsMsgValue { kind: 1, x: f, ..Default::default() },
            V::P(x, y) => EeAiUnitsMsgValue { kind: 2, x, y, ..Default::default() },
        };
    }
    m
}

// the refusal reasons (`CmdReason`; units_lua.rs REASON_NAMES)
const OK: c_long = 0;
const BAD_ARGUMENT: c_long = 1;
const NOT_OWN: c_long = 2;
const NOT_SEEN: c_long = 3;
const UNEXPLORED: c_long = 4;
const CANNOT_PLACE: c_long = 5;
const CANNOT_AFFORD: c_long = 6;
const CANNOT_DO: c_long = 7;
const LIMIT: c_long = 8;
const REASON_TEXT: [&str; 11] = [
    "none",
    "bad_argument",
    "not_own",
    "not_seen",
    "unexplored",
    "cannot_place",
    "cannot_afford",
    "cannot_do",
    "limit",
    "full",
    "not_in_epoch",
];
const COST_COMMAND: i64 = 300;
const COST_ORDERED: i64 = 100;
const FAMILY_ICBM: u64 = 0x37;
const TYPE_GRANARY: u64 = 0x34;
const TYPE_GRANARY_IMPROVED: u64 = 0x2ba;

unsafe fn result(l: *mut lua_State, reason: c_long, count: c_long, id: c_long) -> c_int {
    crate::ai::units_lua::ee_ai_units_result(l.cast(), reason, count, id)
}

fn stats_velocity(c: &UnitView<'_>) -> Option<f32> {
    c.stats().map(|s| crate::world_field!(s.fields(), mMaxVelocity))
}

/// `EECPUnitsMobile`
fn mobile(c: &UnitView<'_>) -> bool {
    !c.class().is_some_and(|k| is_building(&k)) && stats_velocity(c).is_some_and(|v| v > 0.0)
}

/// `EECPUnitsCanAttack`: mobile or armed, or a missile base holding its ICBM
fn can_attack(g: &Game<'_>, c: &UnitView<'_>) -> bool {
    if mobile(c) || armed(c) {
        return true;
    }
    (0..c.cargo_len())
        .any(|i| g.unit(c.cargo_handle(i)).and_then(|k| k.class()).is_some_and(|k| k.family() == FAMILY_ICBM))
}

fn citizen(c: &UnitView<'_>) -> bool {
    let k = category(c);
    k == CAT_LAND_CITIZEN || k == CAT_NAVAL_CITIZEN
}

fn built_building(c: &UnitView<'_>) -> bool {
    c.class().is_some_and(|k| is_building(&k)) && c.is_built()
}

fn gate_piece(c: &UnitView<'_>) -> bool {
    use crate::units::kinds::{EE_CLASS_KIND_GATE, EE_UNIT_CLASS_KINDS};
    c.class()
        .is_some_and(|k| k.category() < 256 && EE_UNIT_CLASS_KINDS[k.category() as usize] & EE_CLASS_KIND_GATE != 0)
}

#[derive(Clone, Copy)]
enum Test {
    Any,
    Mobile,
    CanAttack,
    Citizen,
    Building,
    Gate,
}

fn passes_test(g: &Game<'_>, c: &UnitView<'_>, t: Test) -> bool {
    match t {
        Test::Any => true,
        Test::Mobile => mobile(c),
        Test::CanAttack => can_attack(g, c),
        Test::Citizen => citizen(c),
        Test::Building => built_building(c),
        Test::Gate => gate_piece(c),
    }
}

/// `EECPUnitsOwnUnits`: `ids` -> `ids2`, the player's own living units on the map (or inside, with `inside`)
/// that pass the test -> the first reason one failed
fn own_units(g: &Game<'_>, s: &mut UnitsState, inside: bool, test: Test) -> c_long {
    s.ids2.clear();
    let mut reason = OK;
    for k in 0..s.ids.len() {
        let id = s.ids[k];
        let Some(c) = g.own(s, id).filter(|c| !is_projectile(c)) else {
            if reason == OK {
                reason = NOT_OWN;
            }
            continue;
        };
        let disabled: bool = crate::world_field!(c.complex(), mIsDisabled);
        if disabled || (!inside && (c.container_handle() != 0 || c.tile_xy().0 == NO_TILE)) || !passes_test(g, &c, test)
        {
            if reason == OK {
                reason = CANNOT_DO;
            }
            continue;
        }
        let cid = c.id() as c_long;
        if !s.ids2.contains(&cid) {
            s.ids2.push(cid);
        }
    }
    reason
}

/// `EECPUnitsTraceCmd`: the `next.cmd` line (the FNV-1a of the units' ids)
fn trace_cmd(g: &Game<'_>, s: &UnitsState, verb: &str, reason: c_long, count: c_long, a: c_long, b: c_long) {
    if host(EE_AI_UNITS_HOST_TRACE_ON, g.me_ptr, 0, 0, 0) == 0 {
        return;
    }
    let mut hash: std::ffi::c_ulong = 2166136261;
    for &id in &s.ids2 {
        hash = (hash ^ id as std::ffi::c_ulong).wrapping_mul(16777619);
    }
    let text = format!(
        "{verb} n={count} a={a} b={b} ids={:08x} first={} result={} reason={}",
        hash as u64 & 0xffff_ffff,
        s.ids2.first().copied().unwrap_or(0),
        if reason == OK { "ok" } else { "refused" },
        REASON_TEXT[reason as usize]
    );
    trace(g.me_ptr, c"next.cmd", &text);
}

/// `EECPUnitsRun`: the message run for the player on the units of `ids2` (the selection), which it takes
unsafe fn run(g: &Game<'_>, s: *mut UnitsState, m: &EeAiUnitsMessage, take_them: bool) {
    if host(EE_AI_UNITS_HOST_SELECT_BEGIN, g.me_ptr, 0, 0, 0) == 0 {
        return;
    }
    let n = (*s).ids2.len();
    for k in 0..n {
        let id = (&(*s).ids2)[k];
        if let Some(c) = g.own(&*s, id) {
            host(EE_AI_UNITS_HOST_SELECT_ADD, g.me_ptr, c.handle() as i64, 0, 0);
            if take_them {
                take(g, &mut *s, &c);
            }
        }
    }
    let mut m = *m;
    host_out(EE_AI_UNITS_HOST_EXEC, g.me_ptr, 0, (&mut m as *mut EeAiUnitsMessage).cast());
}

fn exec_plain(g: &Game<'_>, m: &EeAiUnitsMessage) {
    let mut m = *m;
    m.mode = 1;
    host_out(EE_AI_UNITS_HOST_EXEC, g.me_ptr, 0, (&mut m as *mut EeAiUnitsMessage).cast());
}

/// `EECPUnitsBegin`: the ids at 1, the charge, the own units -> Some(the result's count) when refused
unsafe fn begin(
    l: *mut lua_State,
    g: &Game<'_>,
    s: *mut UnitsState,
    inside: bool,
    test: Test,
    verb: &str,
) -> Option<c_int> {
    read_ids(l, 1, &mut (*s).ids);
    charge(l, s, COST_COMMAND + (*s).ids.len() as i64 * COST_ORDERED);
    let reason = own_units(g, &mut *s, inside, test);
    if (*s).ids2.is_empty() {
        let r = if reason == OK { BAD_ARGUMENT } else { reason };
        trace_cmd(g, &*s, verb, r, 0, 0, 0);
        return Some(result(l, r, 0, 0));
    }
    if !(*s).within_limits((*s).ids2.len() as c_long, now()) {
        trace_cmd(g, &*s, verb, LIMIT, 0, 0, 0);
        return Some(result(l, LIMIT, 0, 0));
    }
    None
}

fn tile_ok(g: &Game<'_>, x: c_long, y: c_long) -> bool {
    x >= 0 && y >= 0 && g.world.map().is_some_and(|m| m.tile(x as i64, y as i64).is_some())
}

unsafe fn xy_arg(l: *mut lua_State, g: &Game<'_>, i: c_int) -> Option<(c_long, c_long)> {
    if !is_int(l, i) || !is_int(l, i + 1) || !tile_ok(g, int_at(l, i), int_at(l, i + 1)) {
        return None;
    }
    Some((int_at(l, i), int_at(l, i + 1)))
}

unsafe fn ids_len(s: *mut UnitsState) -> c_long {
    (*s).ids2.len() as c_long
}

unsafe fn first_id(s: *mut UnitsState) -> i64 {
    (&(*s).ids2)[0] as i64
}

/// The forwarded id at `i` (0: not an integer)
unsafe fn forwarded_arg(l: *mut lua_State, s: *mut UnitsState, i: c_int) -> c_long {
    if is_int(l, i) {
        (*s).forward_id(int_at(l, i))
    } else {
        0
    }
}

macro_rules! command {
    ($l:ident, $g:ident, $s:ident, $inside:expr, $test:expr, $verb:expr) => {
        let $l = $l as *mut lua_State;
        let me_ptr = me_of($l);
        let $s = state_of(me_ptr);
        let t = WorldRead::begin();
        let Some($g) = Game::new(&t, me_ptr) else {
            return result($l, BAD_ARGUMENT, 0, 0);
        };
        if let Some(r) = begin($l, &$g, $s, $inside, $test, $verb) {
            return r;
        }
    };
}

/// The usual end: the trace line, then ok with the units' count
unsafe fn done(l: *mut lua_State, g: &Game<'_>, s: *mut UnitsState, verb: &str, a: c_long, b: c_long) -> c_int {
    let n = ids_len(s);
    trace_cmd(g, &*s, verb, OK, n, a, b);
    result(l, OK, n, 0)
}

/// `ai.move(ids, x, y, {queue, forced, formation})`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_move(l: *mut c_void) -> c_int {
    command!(l, g, s, false, Test::Mobile, "move");
    let Some((x, y)) = xy_arg(l, &g, 2) else { return result(l, BAD_ARGUMENT, 0, 0) };
    let queue = opt_bool(l, 4, c"queue", false);
    let forced = opt_bool(l, 4, c"forced", false);
    let formation = opt_bool(l, 4, c"formation", false);
    let m = message(
        EE_AI_UNITS_MSG_PATH_TO,
        0,
        &[
            V::F(0.0),
            V::F(x as f32 + 0.5),
            V::F(y as f32 + 0.5),
            V::I(forced as i64),
            V::I(0),
            V::I(queue as i64),
            V::I(formation as i64),
        ],
    );
    run(&g, s, &m, true);
    done(l, &g, s, "move", x, y)
}

/// `ai.attack(ids, target, {queue})`: the target seen now, or a building the player remembers
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_attack(l: *mut c_void) -> c_int {
    command!(l, g, s, false, Test::CanAttack, "attack");
    let id = forwarded_arg(l, s, 2);
    let Some(target) = g.alive(id) else { return result(l, BAD_ARGUMENT, 0, 0) };
    if let Some(p) = target.player() {
        if p.handle() == g.me.handle() || (p.handle() != g.gaia() && g.is_ally(&p)) {
            return result(l, BAD_ARGUMENT, 0, 0);
        }
    }
    let known = g.seen(&target) || (*s).memory.get(&id).is_some_and(|r| r.building != 0);
    if !known {
        trace_cmd(&g, &*s, "attack", NOT_SEEN, 0, id, 0);
        return result(l, NOT_SEEN, 0, 0);
    }
    let queue = opt_bool(l, 3, c"queue", false);
    run(&g, s, &message(EE_AI_UNITS_MSG_ATTACK_UNIT, 0, &[V::I(id as i64), V::I(queue as i64), V::I(0)]), true);
    done(l, &g, s, "attack", id, 0)
}

/// `ai.attack_ground(ids, x, y)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_attack_ground(l: *mut c_void) -> c_int {
    command!(l, g, s, false, Test::CanAttack, "attack_ground");
    let Some((x, y)) = xy_arg(l, &g, 2) else { return result(l, BAD_ARGUMENT, 0, 0) };
    run(&g, s, &message(EE_AI_UNITS_MSG_ATTACK_GROUND, 0, &[V::F(x as f32 + 0.5), V::F(y as f32 + 0.5)]), true);
    done(l, &g, s, "attack_ground", x, y)
}

/// `ai.gather(ids, resource, {queue})`: a resource on an explored tile with some left
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_gather(l: *mut c_void) -> c_int {
    command!(l, g, s, false, Test::Citizen, "gather");
    let id = if is_int(l, 2) { int_at(l, 2) } else { 0 };
    let target = if id > 0 { g.unit(unit_handle(id)) } else { None };
    let Some(target) = target.filter(|t| t.valid() && t.resource_amount() > 0) else {
        return result(l, BAD_ARGUMENT, 0, 0);
    };
    if !g.explored(&target) {
        trace_cmd(&g, &*s, "gather", UNEXPLORED, 0, id, 0);
        return result(l, UNEXPLORED, 0, 0);
    }
    let queue = opt_bool(l, 3, c"queue", false);
    run(&g, s, &message(EE_AI_UNITS_MSG_GATHER, 0, &[V::I(id as i64), V::I(queue as i64)]), true);
    done(l, &g, s, "gather", id, 0)
}

/// `EECPUnitsNewBuilding`: the newest own building of a class whose tile is (x, y), else the first such pending
/// foundation (0: none)
fn new_building(g: &Game<'_>, unit_type: c_long, x: c_long, y: c_long, above: c_long) -> c_long {
    let mut best: c_long = 0;
    for u in g.me.units() {
        let (ux, uy) = u.tile_xy();
        let id = u.id() as c_long;
        if u.class().map_or(-1, |c| c.unit_type() as c_long) == unit_type
            && ux == x as i64
            && uy == y as i64
            && id > above
            && id > best
        {
            best = id;
        }
    }
    let mut k = 0;
    while best == 0 && k < g.me.pending_units_len() {
        // GetPendingUnit: NULL past the end, which ends the scan
        let Some(u) = g.unit(g.me.pending_unit_handle(k)) else { break };
        let (ux, uy) = u.tile_xy();
        if u.class().is_some_and(|c| c.unit_type() as c_long == unit_type) && ux == x as i64 && uy == y as i64 {
            best = u.id() as c_long;
        }
        k += 1;
    }
    best
}

unsafe fn class_arg(l: *mut lua_State, i: c_int) -> c_long {
    crate::ai::units_lua::ee_ai_units_class_arg(l.cast(), i, class_by_name)
}

fn class_at<'t>(g: &Game<'t>, t: c_long) -> Option<ClassView<'t>> {
    if t < 0 || t as usize >= ClassView::count(g.t) {
        return None;
    }
    ClassView::by_index(g.t, t as usize)
}

/// `ai.build(ids, type, x, y, {queue})` -> ..., the foundation's id: the citizens build it (it is the script's)
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_build(l: *mut c_void) -> c_int {
    command!(l, g, s, false, Test::Citizen, "build");
    let unit_type = class_arg(l, 2);
    let class = class_at(&g, unit_type)
        .filter(|c| is_building(c) || host(EE_AI_UNITS_HOST_LAND_BUILDING, g.me_ptr, c.handle() as i64, 0, 0) != 0);
    let Some(class) = class.filter(|_| is_int(l, 3) && is_int(l, 4)) else { return result(l, BAD_ARGUMENT, 0, 0) };
    let (x, y) = (int_at(l, 3), int_at(l, 4));
    let tech = class.technology();
    if host(EE_AI_UNITS_HOST_CAN_PRODUCE, g.me_ptr, tech, 0, 0) == 0 {
        trace_cmd(&g, &*s, "build", CANNOT_AFFORD, 0, unit_type, x);
        return result(l, CANNOT_AFFORD, 0, 0);
    }
    if host(EE_AI_UNITS_HOST_CAN_PLACE, g.me_ptr, unit_type as i64, x as i64, y as i64) == 0 {
        trace_cmd(&g, &*s, "build", CANNOT_PLACE, 0, unit_type, x);
        return result(l, CANNOT_PLACE, 0, 0);
    }
    let before = g.me.units().map(|u| u.id() as c_long).fold(0, |m, id| if id > m { id } else { m });
    let queue = opt_bool(l, 5, c"queue", false);
    run(&g, s, &message(EE_AI_UNITS_MSG_BUILD, 0, &[V::P(x as f32, y as f32), V::I(tech), V::I(queue as i64)]), true);
    let new = new_building(&g, unit_type, x, y, before);
    if new != 0 {
        if let Some(c) = g.own(&*s, new) {
            take(&g, &mut *s, &c);
        }
    }
    let n = ids_len(s);
    let reason = if new != 0 { OK } else { CANNOT_PLACE };
    trace_cmd(&g, &*s, "build", reason, n, unit_type, new);
    result(l, reason, if new != 0 { n } else { 0 }, new)
}

/// `ai.repair(ids, target)` and `ai.heal(ids, target)`: an own or allied unit
unsafe fn repair_heal(l: *mut c_void, heal: bool) -> c_int {
    let verb = if heal { "heal" } else { "repair" };
    command!(l, g, s, false, if heal { Test::Any } else { Test::Citizen }, verb);
    let id = forwarded_arg(l, s, 2);
    if !g.alive(id).and_then(|t| t.player()).is_some_and(|p| g.is_ally(&p)) {
        return result(l, BAD_ARGUMENT, 0, 0);
    }
    let m = if heal {
        message(EE_AI_UNITS_MSG_HEAL, 0, &[V::I(id as i64)])
    } else {
        let queue = opt_bool(l, 3, c"queue", false);
        message(EE_AI_UNITS_MSG_REPAIR, 0, &[V::I(id as i64), V::I(queue as i64)])
    };
    run(&g, s, &m, true);
    done(l, &g, s, verb, id, 0)
}

/// `ai.repair(ids, target)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_repair(l: *mut c_void) -> c_int {
    repair_heal(l, false)
}

/// `ai.heal(ids, target)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_heal(l: *mut c_void) -> c_int {
    repair_heal(l, true)
}

/// `ai.garrison(ids, building)` / `ai.load(ids, transport)`
unsafe fn load_into(l: *mut c_void, garrison: bool) -> c_int {
    let verb = if garrison { "garrison" } else { "load" };
    command!(l, g, s, false, Test::Any, verb);
    let id = forwarded_arg(l, s, 2);
    let into = g.own(&*s, id).filter(|c| match c.class() {
        Some(k) if garrison => {
            let capacity: c_long = crate::world_field!(k.fields(), mGarrisonCapacity);
            capacity > 0
        }
        Some(k) => host(EE_AI_UNITS_HOST_IS_TRANSPORT, g.me_ptr, k.handle() as i64, 0, 0) != 0,
        None => false,
    });
    let Some(into) = into else { return result(l, BAD_ARGUMENT, 0, 0) };
    (*s).ids2.retain(|&k| k != id);
    let before = into.cargo_len() as c_long;
    let kind = if garrison { EE_AI_UNITS_MSG_LOAD_GARRISON } else { EE_AI_UNITS_MSG_LOAD_TRANSPORT };
    run(&g, s, &message(kind, 0, &[V::I(id as i64), V::I(-1), V::I(0)]), true);
    take(&g, &mut *s, &into);
    done(l, &g, s, verb, id, before)
}

/// `ai.garrison(ids, building)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_garrison(l: *mut c_void) -> c_int {
    load_into(l, true)
}

/// `ai.load(ids, transport)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_load(l: *mut c_void) -> c_int {
    load_into(l, false)
}

/// `ai.unload(containers, x, y)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_unload(l: *mut c_void) -> c_int {
    command!(l, g, s, false, Test::Any, "unload");
    let Some((x, y)) = xy_arg(l, &g, 2) else { return result(l, BAD_ARGUMENT, 0, 0) };
    run(&g, s, &message(EE_AI_UNITS_MSG_UNLOAD, 0, &[V::I(first_id(s)), V::I(x as i64), V::I(y as i64)]), true);
    done(l, &g, s, "unload", x, y)
}

fn queued_at(g: &Game<'_>, s: &UnitsState) -> c_long {
    let mut n: c_long = 0;
    for &id in &s.ids2 {
        if let Some(c) = g.own(s, id) {
            n += c.build_queue_len() as c_long + (c.current_research() != NO_RESEARCH) as c_long;
        }
    }
    n
}

unsafe fn tech_by_name(l: *mut lua_State, i: c_int) -> c_long {
    host(EE_AI_UNITS_HOST_TECH_BY_NAME, std::ptr::null_mut(), lua_tostring(l, i) as i64, 0, 0) as c_long
}

/// `ai.train(buildings, type, n)` / `ai.research(buildings, tech)`: the queue of the least busy of the buildings
unsafe fn queue(l: *mut c_void, train: bool) -> c_int {
    let verb = if train { "train" } else { "research" };
    command!(l, g, s, false, Test::Building, verb);
    let mut tech: c_long = -1;
    if train {
        if let Some(c) = class_at(&g, class_arg(l, 2)) {
            tech = c.technology() as c_long;
        }
    } else if lua_type(l, 2) == LUA_TSTRING {
        tech = tech_by_name(l, 2);
    } else if is_int(l, 2) {
        tech = int_at(l, 2);
    }
    let n = if is_int(l, 3) { int_at(l, 3) } else { 1 };
    if tech < 0 || !(1..=5).contains(&n) {
        return result(l, BAD_ARGUMENT, 0, 0);
    }
    // one building of the same type at a time: the message queues at the selected buildings of the clicked one's type
    let first = g.own(&*s, (&(*s).ids2)[0]).map_or(0, |c| c.id() as i64);
    let before = queued_at(&g, &*s);
    for _ in 0..n {
        run(&g, s, &message(EE_AI_UNITS_MSG_RESEARCH, 0, &[V::I(first), V::I(tech as i64), V::I(0)]), false);
    }
    let queued = queued_at(&g, &*s) - before;
    let reason = if queued > 0 { OK } else { CANNOT_AFFORD };
    trace_cmd(&g, &*s, verb, reason, queued, tech, n);
    result(l, reason, queued.max(0), 0)
}

/// `ai.train(buildings, type, n)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_train(l: *mut c_void) -> c_int {
    queue(l, true)
}

/// `ai.research(buildings, tech)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_research(l: *mut c_void) -> c_int {
    queue(l, false)
}

/// `ai.stance(ids, n)`: the behaviour table's index
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_stance(l: *mut c_void) -> c_int {
    command!(l, g, s, false, Test::Any, "stance");
    if !is_int(l, 2)
        || lua_tointeger(l, 2) < 0
        || host(EE_AI_UNITS_HOST_BEHAVIOR_OK, g.me_ptr, lua_tointeger(l, 2), 0, 0) == 0
    {
        return result(l, BAD_ARGUMENT, 0, 0);
    }
    let n = int_at(l, 2);
    run(&g, s, &message(EE_AI_UNITS_MSG_BEHAVIOR, 0, &[V::I(n as i64)]), true);
    done(l, &g, s, "stance", n, 0)
}

/// `ai.stop(ids)`, `ai.explore(ids)`, `ai.delete(ids)`
unsafe fn plain(l: *mut c_void, verb: &'static str) -> c_int {
    let (inside, test, msg, take_them) = match verb {
        "stop" => (false, Test::Any, EE_AI_UNITS_MSG_STOP, true),
        "explore" => (false, Test::Mobile, EE_AI_UNITS_MSG_EXPLORE, true),
        _ => (true, Test::Any, EE_AI_UNITS_MSG_KILL, false),
    };
    command!(l, g, s, inside, test, verb);
    let m = if msg == EE_AI_UNITS_MSG_KILL { message(msg, 0, &[V::I(1)]) } else { message(msg, 0, &[]) };
    run(&g, s, &m, take_them);
    done(l, &g, s, verb, 0, 0)
}

/// `ai.stop(ids)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_stop(l: *mut c_void) -> c_int {
    plain(l, "stop")
}

/// `ai.explore(ids)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_explore(l: *mut c_void) -> c_int {
    plain(l, "explore")
}

/// `ai.delete(ids)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_delete(l: *mut c_void) -> c_int {
    plain(l, "delete")
}

/// A target argument at `i`: (x, y) at i, i + 1, or a unit the player may know of -> (target, x, y); Err: the
/// refusal (bad_argument, not_seen)
unsafe fn place_or_unit(
    l: *mut lua_State,
    g: &Game<'_>,
    s: *mut UnitsState,
    i: c_int,
    check_tile: bool,
) -> Result<(c_long, c_long, c_long), c_long> {
    if is_int(l, i) && is_int(l, i + 1) {
        let (x, y) = (int_at(l, i), int_at(l, i + 1));
        if check_tile && !tile_ok(g, x, y) {
            return Err(BAD_ARGUMENT);
        }
        return Ok((-1, x, y));
    }
    if is_int(l, i) {
        let Some(u) = g.known(&*s, int_at(l, i)) else { return Err(NOT_SEEN) };
        let (x, y) = u.tile_xy();
        return Ok((u.id() as c_long, x as c_long, y as c_long));
    }
    Err(BAD_ARGUMENT)
}

/// `ai.rally(buildings, x, y)` or `ai.rally(buildings, target)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_rally(l: *mut c_void) -> c_int {
    command!(l, g, s, false, Test::Building, "rally");
    let (target, x, y) = match place_or_unit(l, &g, s, 2, true) {
        Ok(v) => v,
        Err(r) => return result(l, r, 0, 0),
    };
    // the message sets the rally of the selected buildings of the named one's type
    let n = ids_len(s);
    let m = message(
        EE_AI_UNITS_MSG_RALLY,
        0,
        &[V::P(x as f32 + 0.5, y as f32 + 0.5), V::I(target as i64), V::I(first_id(s)), V::I(0)],
    );
    run(&g, s, &m, false);
    trace_cmd(&g, &*s, "rally", OK, n, x, y);
    result(l, OK, n, 0)
}

/// `ai.cancel(buildings, type or tech)`: one of it out of their queues
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_cancel(l: *mut c_void) -> c_int {
    command!(l, g, s, false, Test::Building, "cancel");
    let mut tech: c_long = -1;
    if lua_type(l, 2) == LUA_TSTRING {
        tech = match class_at(&g, class_arg(l, 2)) {
            Some(c) => c.technology() as c_long,
            None => tech_by_name(l, 2),
        };
    } else if is_int(l, 2) {
        tech = int_at(l, 2);
    }
    if tech < 0 {
        return result(l, BAD_ARGUMENT, 0, 0);
    }
    let m = message(EE_AI_UNITS_MSG_REMOVE_TECH, 0, &[V::I(first_id(s)), V::I(tech as i64), V::I(0), V::I(0)]);
    run(&g, s, &m, false);
    done(l, &g, s, "cancel", tech, 0)
}

/// `ai.formation(ids, kind)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_formation(l: *mut c_void) -> c_int {
    command!(l, g, s, false, Test::Mobile, "formation");
    if !is_int(l, 2) || !(0..=16).contains(&lua_tointeger(l, 2)) {
        return result(l, BAD_ARGUMENT, 0, 0);
    }
    let k = int_at(l, 2);
    run(&g, s, &message(EE_AI_UNITS_MSG_FORMATION, 0, &[V::I(k as i64), V::I(1)]), true);
    done(l, &g, s, "formation", k, 0)
}

/// `ai.pack(ids, on)` and `ai.lock(ids, on)`
unsafe fn switch(l: *mut c_void, lock: bool) -> c_int {
    let verb = if lock { "lock" } else { "pack" };
    command!(l, g, s, false, if lock { Test::Gate } else { Test::Any }, verb);
    let on = lua_toboolean(l, 2) != 0;
    let msg = if lock { EE_AI_UNITS_MSG_LOCK } else { EE_AI_UNITS_MSG_PACK };
    run(&g, s, &message(msg, 0, &[V::I(on as i64)]), !lock);
    done(l, &g, s, verb, lua_toboolean(l, 2) as c_long, 0)
}

/// `ai.pack(ids, on)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_pack(l: *mut c_void) -> c_int {
    switch(l, false)
}

/// `ai.lock(ids, on)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_lock(l: *mut c_void) -> c_int {
    switch(l, true)
}

/// `ai.dropoff(ids, building)` and `ai.farm(ids, granary or farm, {queue})`
unsafe fn to_building(l: *mut c_void, farm: bool) -> c_int {
    let verb = if farm { "farm" } else { "dropoff" };
    command!(l, g, s, false, Test::Citizen, verb);
    let id = forwarded_arg(l, s, 2);
    let to = g.own(&*s, id);
    let m = if farm {
        if to.is_none() {
            return result(l, BAD_ARGUMENT, 0, 0);
        }
        let queue = opt_bool(l, 3, c"queue", false);
        message(EE_AI_UNITS_MSG_SMART_FARMERS, 0, &[V::I(id as i64), V::I(queue as i64)])
    } else {
        if !to.is_some_and(|c| c.is_built()) {
            return result(l, BAD_ARGUMENT, 0, 0);
        }
        message(EE_AI_UNITS_MSG_DROPOFF, 0, &[V::I(id as i64)])
    };
    run(&g, s, &m, true);
    done(l, &g, s, verb, id, 0)
}

/// `ai.dropoff(ids, building)`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_dropoff(l: *mut c_void) -> c_int {
    to_building(l, false)
}

/// `ai.farm(ids, granary or farm, {queue})`
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_farm(l: *mut c_void) -> c_int {
    to_building(l, true)
}

/// `ai.replant(granary)` -> ok: the granary's missing field foundations laid again
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_replant(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_COMMAND);
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return result(l, BAD_ARGUMENT, 0, 0) };
    let id = if is_int(l, 1) { (*s).forward_id(int_at(l, 1)) } else { 0 };
    let granary = g.own(&*s, id).filter(|c| {
        let k = c.class().map_or(0, |k| k.unit_type());
        c.is_built() && c.has_tile_node() && (k == TYPE_GRANARY || k == TYPE_GRANARY_IMPROVED)
    });
    if granary.is_none() {
        return result(l, BAD_ARGUMENT, 0, 0);
    }
    (*s).ids2.clear();
    (*s).ids2.push(id);
    if !(*s).within_limits(1, now()) {
        return result(l, LIMIT, 0, 0);
    }
    exec_plain(&g, &message(EE_AI_UNITS_MSG_REPLANT, 1, &[V::I(id as i64)]));
    trace_cmd(&g, &*s, "replant", OK, 1, id, 0);
    result(l, OK, 1, 0)
}

/// `ai.cast(ids, tech, target)` / `ai.cast(ids, tech, x, y)`: a power or calamity by the first caster that can
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_cast(l: *mut c_void) -> c_int {
    command!(l, g, s, false, Test::Any, "cast");
    let tech = if lua_type(l, 2) == LUA_TSTRING {
        tech_by_name(l, 2)
    } else if is_int(l, 2) {
        int_at(l, 2)
    } else {
        -1
    };
    if tech < 0 || host(EE_AI_UNITS_HOST_HAS_NODE, g.me_ptr, tech as i64, 0, 0) == 0 {
        return result(l, BAD_ARGUMENT, 0, 0);
    }
    let (target, x, y) = match place_or_unit(l, &g, s, 3, false) {
        Ok(v) => v,
        Err(r) => return result(l, r, 0, 0),
    };
    let m = message(
        EE_AI_UNITS_MSG_CAST,
        0,
        &[V::I(first_id(s)), V::I(target as i64), V::I(x as i64), V::I(y as i64), V::I(tech as i64)],
    );
    run(&g, s, &m, true);
    done(l, &g, s, "cast", tech, target)
}

/// `ai.patrol(ids, {{x, y}, ...})`: the mobile units patrol through 2..8 points
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_patrol(l: *mut c_void) -> c_int {
    command!(l, g, s, false, Test::Mobile, "patrol");
    if lua_type(l, 2) != LUA_TTABLE {
        return result(l, BAD_ARGUMENT, 0, 0);
    }
    let n = lua_rawlen(l, 2) as c_long;
    if !(2..=8).contains(&n) {
        return result(l, BAD_ARGUMENT, 0, 0);
    }
    let mut points = [(0.0f32, 0.0f32); 8];
    for k in 1..=n {
        lua_rawgeti(l, 2, k as lua_Integer);
        if lua_type(l, -1) != LUA_TTABLE {
            lua_pop(l, 1);
            return result(l, BAD_ARGUMENT, 0, 0);
        }
        lua_rawgeti(l, -1, 1);
        lua_rawgeti(l, -2, 2);
        let ok = is_int(l, -2) && is_int(l, -1);
        let (x, y) = if ok { (int_at(l, -2), int_at(l, -1)) } else { (-1, -1) };
        lua_pop(l, 3);
        if !ok || !g.world.map().is_some_and(|m| m.tile(x as i64, y as i64).is_some()) {
            return result(l, BAD_ARGUMENT, 0, 0);
        }
        points[(k - 1) as usize] = (x as f32, y as f32);
    }
    for p in &points[..n as usize] {
        run(&g, s, &message(EE_AI_UNITS_MSG_PATROL_POINT, 0, &[V::P(p.0, p.1)]), true);
    }
    let first = (points[0].0 as c_long) * 10000 + points[0].1 as c_long;
    done(l, &g, s, "patrol", n, first)
}

/// The player has a tech tree (`mTechnologies != NULL`).
pub const EE_AI_UNITS_HOST_HAS_TREE: i32 = 40;
/// `EECPWallClass(player)`: the epoch's wall as the classic wall manager takes it.
pub const EE_AI_UNITS_HOST_WALL_CLASS: i32 = 41;
/// The tech `a`'s node: -1 none, else `CanAfford(node, 0x20, 1)` (0 or 1).
pub const EE_AI_UNITS_HOST_NODE_AFFORD: i32 = 42;
pub const EE_AI_UNITS_MSG_BUILD_WALL: i32 = 26;
const NOT_IN_EPOCH: c_long = 10;

/// `ai.wall(ids, type, x1, y1, x2, y2)`: the citizens lay a straight wall of the class between the two tiles, as a
/// human's drag does (EEPlanWall); n = the citizens sent
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_wall(l: *mut c_void) -> c_int {
    command!(l, g, s, false, Test::Citizen, "wall");
    // the class: "wall" (or nil) is the epoch's wall as the classic wall manager takes it (GetWallClass)
    let named =
        lua_type(l, 2) == LUA_TNIL || (lua_type(l, 2) == LUA_TSTRING && CStr::from_ptr(lua_tostring(l, 2)) == c"wall");
    let unit_type = if named && host(EE_AI_UNITS_HOST_HAS_TREE, g.me_ptr, 0, 0, 0) != 0 {
        host(EE_AI_UNITS_HOST_WALL_CLASS, g.me_ptr, 0, 0, 0) as c_long
    } else {
        class_arg(l, 2)
    };
    let class = class_at(&g, unit_type).filter(|c| c.wall());
    let Some(class) = class.filter(|_| (3..=6).all(|i| is_int(l, i))) else { return result(l, BAD_ARGUMENT, 0, 0) };
    let (x1, y1, x2, y2) = (int_at(l, 3), int_at(l, 4), int_at(l, 5), int_at(l, 6));
    let on_map = |x: c_long, y: c_long| g.world.map().is_some_and(|m| m.tile(x as i64, y as i64).is_some());
    if !on_map(x1, y1) || !on_map(x2, y2) {
        return result(l, BAD_ARGUMENT, 0, 0);
    }
    // (no CanProduce: EEPlanWall and EEBuildWall check the placement and pay the pieces; a wall tech is never
    // "available" the production way for a computer player)
    let tech = class.technology();
    match host(EE_AI_UNITS_HOST_NODE_AFFORD, g.me_ptr, tech, 0, 0) {
        -1 => {
            trace_cmd(&g, &*s, "wall", NOT_IN_EPOCH, 0, unit_type, 0);
            return result(l, NOT_IN_EPOCH, 0, 0);
        }
        0 => {
            trace_cmd(&g, &*s, "wall", CANNOT_AFFORD, 0, unit_type, x1);
            return result(l, CANNOT_AFFORD, 0, 0);
        }
        _ => {}
    }
    let me_id = g.me.id() as i64;
    let m = message(
        EE_AI_UNITS_MSG_BUILD_WALL,
        0,
        &[V::P(x1 as f32, y1 as f32), V::P(x2 as f32, y2 as f32), V::I(tech), V::I(me_id)],
    );
    run(&g, s, &m, true);
    done(l, &g, s, "wall", unit_type, x1 * 10000 + y1)
}

// ---------------------------------------------------------------------------------------------------------------
// The searches over the player's units, the map and the classic's information cheat

/// `EECPEngineSeesAll(player)`: the manifest's "sees_all".
pub const EE_AI_UNITS_HOST_SEES_ALL: i32 = 43;
/// `EEPlayerBlockTakes(player, a, b - 1, c - 1, size + 2, size + 2, 0)` with `out` the size (a `c_long`).
pub const EE_AI_UNITS_HOST_BLOCK_TAKES: i32 = 44;

const TYPE_TOWN_CENTER: u64 = 0x23;
const TYPE_CAPITOL: u64 = 0x29;
const TYPE_SETTLEMENT: u64 = 0x2b;
const CAT_LAND: u64 = 5;
const CAT_HERO: u64 = 0x17;
const FAMILY_BUILDING: u64 = 3;

fn tile_size(c: &ClassView<'_>) -> c_long {
    crate::world_field!(c.fields(), mTileSize)
}

/// `ai.drop_site(x, y[, kind])` -> the id of the player's nearest finished drop site for the resource kind ("food":
/// a granary too), its tile distance and its centre x, y; nil when it has none
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_drop_site(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, 500);
    if !is_int(l, 1) || !is_int(l, 2) {
        return 0;
    }
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return 0 };
    let (x, y) = (int_at(l, 1), int_at(l, 2));
    let food = lua_type(l, 3) == LUA_TSTRING && CStr::from_ptr(lua_tostring(l, 3)) == c"food";
    let mut best: Option<(UnitView<'_>, c_long, c_long)> = None;
    for u in g.me.units() {
        let Some(class) = u.class() else { continue };
        let k = class.unit_type();
        if k != TYPE_TOWN_CENTER && k != TYPE_CAPITOL && k != TYPE_SETTLEMENT && !(food && k == TYPE_GRANARY) {
            continue;
        }
        if !u.complex_unit().is_some_and(|c| c.is_built()) || !(u.hit_points() > 0) || !Game::on_map(&u) {
            continue;
        }
        let half = tile_size(&class) / 2;
        let (ux, uy) = u.tile_xy();
        let d = d2(ux as c_long + half, uy as c_long + half, x, y);
        let better = match &best {
            None => true,
            Some((b, bd, _)) => d < *bd || (d == *bd && u.id() < b.id()),
        };
        if better {
            best = Some((u, d, half));
        }
    }
    let Some((b, d, half)) = best else { return 0 };
    let (bx, by) = b.tile_xy();
    lua_pushinteger(l, b.id() as lua_Integer);
    lua_pushinteger(l, isqrt_up(d) as lua_Integer);
    lua_pushinteger(l, (bx as c_long + half) as lua_Integer);
    lua_pushinteger(l, (by as c_long + half) as lua_Integer);
    4
}

/// `ai.enemy_force(player, x, y, r)` -> n, hp, attack: the player's land soldiers within r tiles, seen or not;
/// only for an AI that "sees_all"
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_enemy_force(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_CALL);
    if host(EE_AI_UNITS_HOST_SEES_ALL, me_ptr, 0, 0, 0) == 0 || !(1..=4).all(|i| is_int(l, i)) {
        return 0;
    }
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return 0 };
    let p = lua_tointeger(l, 1);
    if p < 0 || p as u64 >= g.world.num_players() {
        return 0;
    }
    let Some(other) = g.world.player(p as usize) else { return 0 };
    let (x, y, r) = (int_at(l, 2), int_at(l, 3), int_at(l, 4));
    let (mut n, mut hp, mut attack, mut scanned): (c_long, c_long, c_long, i64) = (0, 0, 0, 0);
    for u in other.units() {
        scanned += 1;
        let Some(st) = u.stats() else { continue };
        let Some(class) = u.class() else { continue };
        if u.complex_unit_handle() == 0 || !(u.hit_points() > 0) || attack_of(&st) == 0.0 {
            continue;
        }
        if crate::units::player_views::land_building(&class) {
            continue;
        }
        let k = class.category();
        if k != CAT_LAND && k != CAT_HERO {
            continue;
        }
        let (ux, uy) = u.tile_xy();
        let (dx, dy) = (ux as c_long - x, uy as c_long - y);
        if dx * dx + dy * dy > r * r {
            continue;
        }
        n += 1;
        hp += u.hit_points() as c_long;
        attack += um::lrintf(attack_of(&st)) as c_long;
    }
    charge(l, s, scanned * COST_SCAN);
    lua_pushinteger(l, n as lua_Integer);
    lua_pushinteger(l, hp as lua_Integer);
    lua_pushinteger(l, attack as lua_Integer);
    3
}

/// `ai.player_file(player)` -> {gatherers, types, military, strength}: the classic's intelligence file of a player
/// (every unit alive, seen or not); only for an AI that "sees_all"
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_player_file(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_CALL);
    if host(EE_AI_UNITS_HOST_SEES_ALL, me_ptr, 0, 0, 0) == 0 || !is_int(l, 1) {
        return 0;
    }
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return 0 };
    let p = lua_tointeger(l, 1);
    if p < 0 || p as u64 >= g.world.num_players() {
        return 0;
    }
    let Some(other) = g.world.player(p as usize) else { return 0 };
    let (mut gatherers, mut scanned): (c_long, i64) = (0, 0);
    let mut count = [0 as c_long; 4];
    let mut strength = [0 as c_long; 4]; // land, sea, air, space
    let mut types: std::collections::BTreeMap<c_long, c_long> = std::collections::BTreeMap::new();
    for u in other.units() {
        scanned += 1;
        let Some(c) = u.class() else { continue };
        if !(u.hit_points() > 0) {
            continue;
        }
        let k = c.category();
        if k == CAT_LAND_CITIZEN || k == CAT_NAVAL_CITIZEN {
            gatherers += 1;
            continue;
        }
        if c.family() == FAMILY_BUILDING || c.family() == FAMILY_ANIMAL {
            continue;
        }
        *types.entry(c.unit_type() as c_long).or_insert(0) += 1;
        let f = c.fields();
        let attack: c_long = crate::world_field!(f, mAttack);
        let calamity: bool = crate::world_field!(f, mHasCalamityPowers);
        let medic: bool = crate::world_field!(f, mIsMedic);
        let conversion: f32 = crate::world_field!(f, mConversionRate);
        if !(attack > 0) && !calamity && !medic && conversion == 0.0 {
            continue;
        }
        let domains: u8 = crate::world_field!(f, mDomains);
        let d = if domains & 1 != 0 {
            2
        } else if domains & 4 != 0 {
            1
        } else if domains & 8 != 0 {
            3
        } else {
            0
        };
        count[d] += 1;
        strength[d] += crate::world_field!(f, mInfluence);
    }
    // (the map moves into the state's scratch before a charge can raise)
    (*s).sorted.clear();
    (*s).sorted.extend(types.into_iter().map(|(k, n)| EeAiUnitsSortEntry { key: n as i64, id: k }));
    let n_types = (*s).sorted.len();
    charge(l, s, scanned * COST_SCAN + n_types as i64 * COST_FIELD);
    const DOMAINS: [&CStr; 4] = [c"land", c"sea", c"air", c"space"];
    lua_createtable(l, 0, 4);
    set_int(l, c"gatherers", gatherers);
    lua_createtable(l, 0, n_types as c_int);
    for k in 0..n_types {
        let e = (&(*s).sorted)[k];
        lua_pushinteger(l, e.key as lua_Integer);
        lua_setfield(l, -2, name_of(&mut *s, e.id));
    }
    lua_setfield(l, -2, c"types".as_ptr());
    lua_createtable(l, 0, 4);
    for d in 0..4 {
        set_int(l, DOMAINS[d], count[d]);
    }
    lua_setfield(l, -2, c"military".as_ptr());
    lua_createtable(l, 0, 4);
    for d in 0..4 {
        set_int(l, DOMAINS[d], strength[d]);
    }
    lua_setfield(l, -2, c"strength".as_ptr());
    1
}

fn test_spawn_on() -> bool {
    static ON: std::sync::OnceLock<bool> = std::sync::OnceLock::new();
    *ON.get_or_init(|| std::env::var("EE_AI_TEST_SPAWN").is_ok_and(|v| v.starts_with('1')))
}

/// TEST ONLY (EE_AI_TEST_SPAWN=1): `ai.test_town(player)` -> x, y of the player's first town centre or capitol
/// (fog ignored); off: nil
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_test_town(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    charge(l, s, COST_CALL);
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return 0 };
    if !test_spawn_on()
        || !is_int(l, 1)
        || lua_tointeger(l, 1) < 0
        || lua_tointeger(l, 1) as u64 >= g.world.num_players()
    {
        return 0;
    }
    let Some(p) = g.world.player(lua_tointeger(l, 1) as usize) else { return 0 };
    for u in p.units() {
        let Some(c) = u.class() else { continue };
        if (c.unit_type() == TYPE_TOWN_CENTER || c.unit_type() == TYPE_CAPITOL) && u.hit_points() > 0 {
            let half = tile_size(&c) / 2;
            let (x, y) = u.tile_xy();
            lua_pushinteger(l, (x as c_long + half) as lua_Integer);
            lua_pushinteger(l, (y as c_long + half) as lua_Integer);
            return 2;
        }
    }
    0
}

/// `ai.find_site(type, x, y, {min, max, safe, margin, far})` -> x, y: the nearest explored tile where the class
/// can be placed, ring by ring; nil: none
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_find_site(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return 0 };
    let unit_type = class_arg(l, 1);
    let Some(class) = class_at(&g, unit_type) else { return 0 };
    let Some(map) = g.world.map() else { return 0 };
    if !is_int(l, 2) || !is_int(l, 3) {
        return 0;
    }
    let (x, y) = (int_at(l, 2), int_at(l, 3));
    let (mut min, mut max): (c_long, c_long) = (0, 12);
    opt_int(l, 4, c"min", &mut min);
    opt_int(l, 4, c"max", &mut max);
    let min = min.max(0);
    let max = max.min(30);
    let safe = opt_bool(l, 4, c"safe", false);
    // margin: the footprint grown by a tile on every side is free as well; far: the rings from max inward
    let margin = opt_bool(l, 4, c"margin", false);
    let far = opt_bool(l, 4, c"far", false);
    let mut size = tile_size(&class);
    charge(l, s, COST_CALL + ((2 * max + 1) * (2 * max + 1)) as i64 * if margin { 6 } else { 4 });
    let mut found: Option<(c_long, c_long)> = None;
    let mut k = min;
    while k <= max && found.is_none() {
        let r = if far { max - (k - min) } else { k };
        let mut dy = -r;
        while dy <= r && found.is_none() {
            for dx in -r..=r {
                if dx > -r && dx < r && dy > -r && dy < r {
                    continue; // the ring only
                }
                let (tx, ty) = (x + dx, y + dy);
                let tile = if tx >= 0 && ty >= 0 { map.tile(tx as i64, ty as i64) } else { None };
                if !tile.is_some_and(|t| g.tile_explored(&t)) {
                    continue;
                }
                if safe && (*s).layer_at(Layer::Threat, tx, ty) > 0 {
                    continue;
                }
                if host(EE_AI_UNITS_HOST_CAN_PLACE, me_ptr, unit_type as i64, tx as i64, ty as i64) != 0
                    && (!margin
                        || ee_ai_units_host(
                            EE_AI_UNITS_HOST_BLOCK_TAKES,
                            me_ptr,
                            unit_type as i64,
                            tx as i64,
                            ty as i64,
                            (&mut size as *mut c_long).cast(),
                        ) != 0)
                {
                    found = Some((tx, ty));
                    break;
                }
            }
            dy += 1;
        }
        k += 1;
    }
    let Some((fx, fy)) = found else { return 0 };
    lua_pushinteger(l, fx as lua_Integer);
    lua_pushinteger(l, fy as lua_Integer);
    2
}

/// `ai.shore(x, y, {sea, land, max})` -> x, y, sea, land: the nearest explored land tile, ring by ring, that
/// touches water (of the water body `sea` when given) and lies on the land mass `land` when given
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_lua_shore(l: *mut c_void) -> c_int {
    let l = l as *mut lua_State;
    let me_ptr = me_of(l);
    let s = state_of(me_ptr);
    let t = WorldRead::begin();
    let Some(g) = Game::new(&t, me_ptr) else { return 0 };
    let Some(map) = g.world.map() else { return 0 };
    if !is_int(l, 1) || !is_int(l, 2) {
        return 0;
    }
    let (x, y) = (int_at(l, 1), int_at(l, 2));
    let (mut sea, mut land, mut max): (c_long, c_long, c_long) = (0, 0, 20);
    opt_int(l, 3, c"sea", &mut sea);
    opt_int(l, 3, c"land", &mut land);
    opt_int(l, 3, c"max", &mut max);
    let max = max.clamp(0, 40);
    charge(l, s, COST_CALL + ((2 * max + 1) * (2 * max + 1)) as i64 * 6);
    let (w, h) = map_size();
    const SIDE: [[c_long; 2]; 4] = [[1, 0], [-1, 0], [0, 1], [0, -1]];
    for r in 0..=max {
        for dy in -r..=r {
            for dx in -r..=r {
                if dx > -r && dx < r && dy > -r && dy < r {
                    continue;
                }
                let (tx, ty) = (x + dx, y + dy);
                if tx < 0 || ty < 0 || tx >= w || ty >= h {
                    continue;
                }
                if !map.tile(tx as i64, ty as i64).is_some_and(|t| g.tile_explored(&t)) {
                    continue;
                }
                let mass = continent(tx, ty, false) as c_long;
                if mass == 0 || (land != 0 && mass != land) {
                    continue;
                }
                let mut water: c_long = 0;
                for side in &SIDE {
                    if water != 0 {
                        break;
                    }
                    let (nx, ny) = (tx + side[0], ty + side[1]);
                    if nx < 0 || ny < 0 || nx >= w || ny >= h {
                        continue;
                    }
                    let b = continent(nx, ny, true) as c_long;
                    if b != 0 && (sea == 0 || b == sea) {
                        water = b;
                    }
                }
                if water == 0 {
                    continue;
                }
                lua_pushinteger(l, tx as lua_Integer);
                lua_pushinteger(l, ty as lua_Integer);
                lua_pushinteger(l, water as lua_Integer);
                lua_pushinteger(l, mass as lua_Integer);
                return 4;
            }
        }
    }
    0
}

// ---------------------------------------------------------------------------------------------------------------
// The registration (EECPUnitsOpen before; lane A, round 3): the `ai` table's functions in the C++ table's order (the
// table's layout in Lua is unchanged), each a closure over the computer player (a light userdata upvalue)

type Binding = unsafe extern "C-unwind" fn(*mut c_void) -> c_int;

const UNITS_FUNCTION_COUNT: usize = 89;

static UNITS_FUNCTIONS: [(&CStr, Binding); UNITS_FUNCTION_COUNT] = [
    (c"me", ee_ai_units_lua_me),
    (c"time", ee_ai_units_lua_time),
    (c"players", ee_ai_units_lua_players),
    (c"map", ee_ai_units_lua_map),
    (c"units", ee_ai_units_lua_units),
    (c"unit", ee_ai_units_lua_unit),
    (c"get", ee_ai_units_lua_get),
    (c"seen", ee_ai_units_lua_seen),
    (c"memory", ee_ai_units_lua_memory),
    (c"resources", ee_ai_units_lua_resources),
    (c"tile", ee_ai_units_lua_tile),
    (c"class", more::ee_ai_units_lua_class),
    (c"can_hurt", more::ee_ai_units_lua_can_hurt),
    (c"wonders", more::ee_ai_units_lua_wonders),
    (c"events", ee_ai_units_lua_events),
    (c"can_place", more::ee_ai_units_lua_can_place),
    (c"reach", ee_ai_units_lua_reach),
    (c"strength", ee_ai_units_lua_strength),
    (c"move", ee_ai_units_lua_move),
    (c"attack", ee_ai_units_lua_attack),
    (c"attack_ground", ee_ai_units_lua_attack_ground),
    (c"gather", ee_ai_units_lua_gather),
    (c"build", ee_ai_units_lua_build),
    (c"repair", ee_ai_units_lua_repair),
    (c"garrison", ee_ai_units_lua_garrison),
    (c"load", ee_ai_units_lua_load),
    (c"unload", ee_ai_units_lua_unload),
    (c"train", ee_ai_units_lua_train),
    (c"research", ee_ai_units_lua_research),
    (c"stance", ee_ai_units_lua_stance),
    (c"stop", ee_ai_units_lua_stop),
    (c"explore", ee_ai_units_lua_explore),
    (c"delete", ee_ai_units_lua_delete),
    (c"take", ee_ai_units_lua_take),
    (c"release", ee_ai_units_lua_release),
    (c"reflex", ee_ai_units_lua_reflex),
    (c"test_spawn", more::ee_ai_units_lua_test_spawn),
    (c"test_town", ee_ai_units_lua_test_town),
    (c"danger", ee_ai_units_lua_danger),
    (c"danger_box", ee_ai_units_lua_danger_box),
    (c"mark", ee_ai_units_lua_mark),
    (c"route", ee_ai_units_lua_route),
    (c"find_site", ee_ai_units_lua_find_site),
    (c"heal", ee_ai_units_lua_heal),
    (c"rally", ee_ai_units_lua_rally),
    (c"cancel", ee_ai_units_lua_cancel),
    (c"improve", more::ee_ai_units_lua_improve),
    (c"improve_cost", more::ee_ai_units_lua_improve_cost),
    (c"formation", ee_ai_units_lua_formation),
    (c"pack", ee_ai_units_lua_pack),
    (c"dropoff", ee_ai_units_lua_dropoff),
    (c"farm", ee_ai_units_lua_farm),
    (c"cast", ee_ai_units_lua_cast),
    (c"replant", ee_ai_units_lua_replant),
    (c"path_len", ee_ai_units_lua_path_len),
    (c"drop_site", ee_ai_units_lua_drop_site),
    (c"shore", ee_ai_units_lua_shore),
    (c"water_sites", more::ee_ai_units_lua_water_sites),
    (c"area", more::ee_ai_units_lua_area),
    (c"garrison_fill", more::ee_ai_units_lua_garrison_fill),
    (c"enemy_force", ee_ai_units_lua_enemy_force),
    (c"player_file", ee_ai_units_lua_player_file),
    (c"area_point", more::ee_ai_units_lua_area_point),
    (c"forest_point", more::ee_ai_units_lua_forest_point),
    (c"hill_point", more::ee_ai_units_lua_hill_point),
    (c"choke_point", more::ee_ai_units_lua_choke_point),
    (c"wall_path", more::ee_ai_units_lua_wall_path),
    (c"wall_route", more::ee_ai_units_lua_wall_route),
    (c"wall_route_result", more::ee_ai_units_lua_wall_route_result),
    (c"wall_line", more::ee_ai_units_lua_wall_line),
    (c"wall_stretches", more::ee_ai_units_lua_wall_stretches),
    (c"enemy_near", more::ee_ai_units_lua_enemy_near),
    (c"unexplored", more::ee_ai_units_lua_unexplored),
    (c"config", ee_ai_units_lua_config),
    (c"used", more::ee_ai_units_lua_used),
    (c"wall", ee_ai_units_lua_wall),
    (c"gate", more::ee_ai_units_lua_gate),
    (c"lock", ee_ai_units_lua_lock),
    (c"tribute", more::ee_ai_units_lua_tribute),
    (c"trade", more::ee_ai_units_lua_trade),
    (c"diplomacy", more::ee_ai_units_lua_diplomacy),
    (c"patrol", ee_ai_units_lua_patrol),
    (c"tuning", more::ee_ai_units_lua_tuning),
    (c"techs", more::ee_ai_units_lua_techs),
    (c"epoch", more::ee_ai_units_lua_epoch),
    (c"gatherers", ee_ai_units_lua_gatherers),
    (c"place", more::ee_ai_units_lua_place),
    (c"site_blocked", more::ee_ai_units_lua_site_blocked),
    (c"build_unsafe", more::ee_ai_units_lua_build_unsafe),
];

/// TEST ONLY (EE_AI_BINDING_TRACE, binding_trace.rs): each binding's arguments and results, the port's golden
unsafe extern "C-unwind" fn traced<const K: usize>(l: *mut lua_State) -> c_int {
    let (name, f) = UNITS_FUNCTIONS[K];
    crate::ai::binding_trace::ee_ai_binding_trace_enter(l.cast(), name.as_ptr());
    let n = f(l.cast());
    crate::ai::binding_trace::ee_ai_binding_trace_leave(l.cast(), name.as_ptr(), n);
    n
}

static UNITS_TRACED: [lua_CFunction; UNITS_FUNCTION_COUNT] = [
    traced::<0>,
    traced::<1>,
    traced::<2>,
    traced::<3>,
    traced::<4>,
    traced::<5>,
    traced::<6>,
    traced::<7>,
    traced::<8>,
    traced::<9>,
    traced::<10>,
    traced::<11>,
    traced::<12>,
    traced::<13>,
    traced::<14>,
    traced::<15>,
    traced::<16>,
    traced::<17>,
    traced::<18>,
    traced::<19>,
    traced::<20>,
    traced::<21>,
    traced::<22>,
    traced::<23>,
    traced::<24>,
    traced::<25>,
    traced::<26>,
    traced::<27>,
    traced::<28>,
    traced::<29>,
    traced::<30>,
    traced::<31>,
    traced::<32>,
    traced::<33>,
    traced::<34>,
    traced::<35>,
    traced::<36>,
    traced::<37>,
    traced::<38>,
    traced::<39>,
    traced::<40>,
    traced::<41>,
    traced::<42>,
    traced::<43>,
    traced::<44>,
    traced::<45>,
    traced::<46>,
    traced::<47>,
    traced::<48>,
    traced::<49>,
    traced::<50>,
    traced::<51>,
    traced::<52>,
    traced::<53>,
    traced::<54>,
    traced::<55>,
    traced::<56>,
    traced::<57>,
    traced::<58>,
    traced::<59>,
    traced::<60>,
    traced::<61>,
    traced::<62>,
    traced::<63>,
    traced::<64>,
    traced::<65>,
    traced::<66>,
    traced::<67>,
    traced::<68>,
    traced::<69>,
    traced::<70>,
    traced::<71>,
    traced::<72>,
    traced::<73>,
    traced::<74>,
    traced::<75>,
    traced::<76>,
    traced::<77>,
    traced::<78>,
    traced::<79>,
    traced::<80>,
    traced::<81>,
    traced::<82>,
    traced::<83>,
    traced::<84>,
    traced::<85>,
    traced::<86>,
    traced::<87>,
    traced::<88>,
];

/// The ai.* functions (`EECPLuaConfig::mOpen`: the `ai` table on top of the stack; the context is the
/// EEComputerPlayer). Makes the player's per-unit state.
///
/// # Safety
/// `state` is a Lua state of the runtime with the `ai` table on top; `player` a live computer player.
pub unsafe extern "C" fn ee_ai_units_open(state: *mut c_void, player: *mut c_void) {
    let l = state as *mut lua_State;
    state_of(player);
    let trace = crate::ai::binding_trace::ee_ai_binding_trace_on();
    if trace {
        crate::ai::binding_trace::ee_ai_binding_trace_set_out(crate::ai::next_game::debug_out);
    }
    for (k, &(name, f)) in UNITS_FUNCTIONS.iter().enumerate() {
        lua_pushlightuserdata(l, player);
        // SAFETY: a binding takes the lua_State as a void pointer: the same ABI
        let f = if trace { UNITS_TRACED[k] } else { std::mem::transmute::<Binding, lua_CFunction>(f) };
        lua_pushcclosure(l, f, 1);
        lua_setfield(l, -2, name.as_ptr());
    }
}

// the rest of the bindings (lane A round 2)
#[path = "units_more.rs"]
mod more;
