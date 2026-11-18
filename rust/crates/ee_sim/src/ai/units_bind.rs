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
