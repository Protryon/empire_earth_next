//! The next engine in the game (EECPNext.cpp, EECPNextLua.cpp, EECPNextView.cpp, EECPNextArmy.cpp before; lane A,
//! round 3): the tick after the computer player's own bookkeeping, the army's scan and its scouting memory, the
//! view, the gather safety's tests, the strategy step on the script's Lua state, the orders, the save of the state
//! block, the micro step and the citizens' losses trace.
//!
//! The world is read through `ee_sim::world`'s views; what they do not hold (the computer player's own objects: the
//! placement's town tile, the resource centres, the danger zones; a goal's target; the continent manager's areas and
//! forests; the tech nodes' methods with bodies; the trace, the game log, the files) is one C++ host,
//! `ee_ai_next_host` (EECPNext.cpp). Every read and every host call is the C++'s, in its order (the oracle's `ai=`
//! hashes and the bindings' golden hold them).
#![cfg(not(test))]
#![allow(clippy::missing_safety_doc)] // each function's contract is its doc's "Safety:"
#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64
#![allow(clippy::too_many_arguments)]

use std::collections::{BTreeMap, BTreeSet};
use std::ffi::{c_char, c_long, c_ulong, c_void, CStr, CString};
use std::sync::Mutex;

use crate::ai::lua_host::{self, Config, Host, JobValueRef};
use crate::ai::lua_value::{self, EeAiLuaKey, EeAiLuaNode, EeAiLuaTreeOps, OwnedKey, Value};
use crate::ai::names;
use crate::ai::next_save;
use crate::ai::next_state::*;
use crate::ai::next_value::{self, ClassCost, Enemy, Site, Soldiers, Threat, View, ViewEvent};
use crate::units::cls_dispatch::{EE_UNITS_BODY_IS_BUILDING, EE_UNITS_BODY_IS_LAND_BUILDING, EE_UNIT_CLASS_BODIES};
use crate::world::{ClassView, NodeView, PlayerView, TechTreeView, UnitView, WorldRead, WorldView};
use ee_base::math::um;

// ---------------------------------------------------------------------------------------------------------------
// The host (EECPNext.cpp ee_ai_next_host)

/// `gPhysics->mGameTime`
pub const EE_AI_NEXT_HOST_NOW: i32 = 1;
/// `gCPTraceOn`
pub const EE_AI_NEXT_HOST_TRACE_ON: i32 = 2;
/// `EECPTraceOut(player, b, "%s", c)` under the manager `a` (a C string; 0: the current one)
pub const EE_AI_NEXT_HOST_TRACE: i32 = 3;
/// `EEGameLogOn()`
pub const EE_AI_NEXT_HOST_GAME_LOG_ON: i32 = 4;
/// `EEGameLogLine(a)`
pub const EE_AI_NEXT_HOST_GAME_LOG: i32 = 5;
/// `gPlatSimMode`
pub const EE_AI_NEXT_HOST_SIM_MODE: i32 = 6;
/// `fprintf(stderr, "%s", a)`, flushed
pub const EE_AI_NEXT_HOST_STDERR: i32 = 7;
/// `EECPEngineLog("%s", a)`
pub const EE_AI_NEXT_HOST_ENGINE_LOG: i32 = 8;
/// `gUserInterface->mRandomSeed`
pub const EE_AI_NEXT_HOST_GAME_SEED: i32 = 9;
/// the file `a` (a game path, PlatFOpen "rb") appended to `out` (a `Vec<u8>`) -> 1, 0: none
pub const EE_AI_NEXT_HOST_READ: i32 = 10;
/// whether the file `a` opens
pub const EE_AI_NEXT_HOST_EXISTS: i32 = 11;
/// `EECPNextClassName(a)` appended to `out`
pub const EE_AI_NEXT_HOST_CLASS_NAME: i32 = 12;
/// the manifest's classic AI's id appended to `out` -> 1, 0: none
pub const EE_AI_NEXT_HOST_CLASSIC_AI: i32 = 13;
/// the script of the manifest's AI `a` (a C string) appended to `out` -> 1, 0: not in the manifest
pub const EE_AI_NEXT_HOST_AI_SCRIPT: i32 = 14;
/// the player's per-unit state (`mUnitsState->mRust`, 0: none)
pub const EE_AI_NEXT_HOST_UNITS_STATE: i32 = 15;
/// the placement's town centre tile into `out` (two i64) -> 1, 0: no placement
pub const EE_AI_NEXT_HOST_TOWN: i32 = 16;
/// a: 0 `GetTotalPopulation(true, false)`, 1 `GetPopulationLimit(0)`, 2 `GetTotalPopulation(true, true)`,
/// 3 `EEPlayer::GetPopulationLimit(0)`, 4 `mPopulationCap`, 5 `mDropOffDistance`
pub const EE_AI_NEXT_HOST_POPULATION: i32 = 17;
/// `EECPPendingEpoch(player)`
pub const EE_AI_NEXT_HOST_PENDING_EPOCH: i32 = 18;
/// `mCombatManager != NULL`
pub const EE_AI_NEXT_HOST_HAS_COMBAT: i32 = 19;
/// `mCanGather`
pub const EE_AI_NEXT_HOST_CAN_GATHER: i32 = 20;
/// `GetNumResourceCenters()`
pub const EE_AI_NEXT_HOST_RC_COUNT: i32 = 21;
/// the centre `a`'s `mUnit` (handle), `mIsActive`, `mUnits.size()` into `out` (three i64)
pub const EE_AI_NEXT_HOST_RC: i32 = 22;
/// the centre `a`'s `mUnits[b]` (an `EEUnit*`)
pub const EE_AI_NEXT_HOST_RC_UNIT: i32 = 23;
/// the centre `a`'s `GetCenter` tile into `out` (two i64)
pub const EE_AI_NEXT_HOST_RC_CENTER: i32 = 24;
/// `gWorld.GetResourceUsers(a)`
pub const EE_AI_NEXT_HOST_RESOURCE_USERS: i32 = 27;
/// the goal `a`'s target unit (b: 0 `GetGatherResource`, 1 `GetAttackTarget`, 2 `GetBuildTarget`; 0: none)
pub const EE_AI_NEXT_HOST_GOAL_UNIT: i32 = 28;
/// `gContinentManager.GetContinent(a, kAIPathLandMass)` (a unit)
pub const EE_AI_NEXT_HOST_CONTINENT_OF: i32 = 29;
/// the player `a`'s `GetNearestArea((b, c), true)`'s closest hull point to (b, c) into `out` -> 1, 0: no area
pub const EE_AI_NEXT_HOST_AREA_HULL: i32 = 30;
/// `gContinentManager.mForests.size()`
pub const EE_AI_NEXT_HOST_FORESTS: i32 = 31;
/// the forest `a`'s `GetID()` -> -1 when `mForests[a]` is NULL
pub const EE_AI_NEXT_HOST_FOREST: i32 = 32;
/// the forest `a`'s point closest to (b, c) into `out` (out[2]: 0 `GetClosestPoint`, 1 `GetClosestHullPoint`)
pub const EE_AI_NEXT_HOST_FOREST_POINT: i32 = 33;
/// `gWorld.GetUnit(a)` (0: none)
pub const EE_AI_NEXT_HOST_UNIT: i32 = 34;
/// `player->ScaleCost(node a->GetCost(b, c, 1.0f))`
pub const EE_AI_NEXT_HOST_NODE_COST: i32 = 35;
/// the node `a`'s `GetIsAvailable()` (b 0) or `GetIsObsolete()` (b 1)
pub const EE_AI_NEXT_HOST_NODE_TEST: i32 = 36;
/// `EEEpochCount()`
pub const EE_AI_NEXT_HOST_EPOCH_COUNT: i32 = 37;
/// `gWorld.mGameOptions.mEpochEnd`
pub const EE_AI_NEXT_HOST_EPOCH_END: i32 = 38;
/// `EECPExperiment(player, a)`
pub const EE_AI_NEXT_HOST_EXPERIMENT: i32 = 39;
/// the complex unit `a`'s `mLastAttacker` (0: none)
pub const EE_AI_NEXT_HOST_LAST_ATTACKER: i32 = 40;
/// `EECPTraceStats(player, a)`
pub const EE_AI_NEXT_HOST_TRACE_STATS: i32 = 41;
/// `EECPRMFleeRaids(player)`
pub const EE_AI_NEXT_HOST_FLEE_RAIDS: i32 = 42;
/// `EECPNextGetState(player)` (made when missing)
pub const EE_AI_NEXT_HOST_STATE: i32 = 43;
/// the player slot `a`'s computer player when it is active with a per-unit and a next state (0: none)
pub const EE_AI_NEXT_HOST_MICRO_PLAYER: i32 = 44;
/// `player->mNextState` (0: none)
pub const EE_AI_NEXT_HOST_STATE_OF: i32 = 45;
/// `EECPRMResourceInDanger(player, a, b)`
pub const EE_AI_NEXT_HOST_IN_DANGER: i32 = 46;
/// `ESDebugOut("%s", a)`
pub const EE_AI_NEXT_HOST_DEBUG_OUT: i32 = 47;
/// `gCPTraceManager = a` (a static C string) -> the previous one
pub const EE_AI_NEXT_HOST_SCOPE: i32 = 48;
/// the player's `mPlayerID`
pub const EE_AI_NEXT_HOST_PLAYER_ID: i32 = 49;

extern "C" {
    fn ee_ai_next_host(op: i32, player: *mut c_void, a: i64, b: i64, c: i64, out: *mut c_void) -> i64;
    /// appends to a C++ `std::string` (EECPNext.cpp)
    fn ee_ai_next_string_append(string: *mut c_void, data: *const u8, len: usize);
}

fn host(op: i32, me: *mut c_void, a: i64, b: i64, c: i64) -> i64 {
    // SAFETY: the C++ switch over the live game (the simulation thread, inside a call from it)
    unsafe { ee_ai_next_host(op, me, a, b, c, std::ptr::null_mut()) }
}

fn host_out<T>(op: i32, me: *mut c_void, a: i64, b: i64, c: i64, out: &mut T) -> i64 {
    // SAFETY: as above; `out` is the op's record
    unsafe { ee_ai_next_host(op, me, a, b, c, out as *mut T as *mut c_void) }
}

/// Appends bytes to a `Vec<u8>` the host was given (`out` of the byte ops).
///
/// # Safety
/// `out` is a live `Vec<u8>`, `data` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_append(out: *mut c_void, data: *const u8, len: usize) {
    if len > 0 {
        (*(out as *mut Vec<u8>)).extend_from_slice(std::slice::from_raw_parts(data, len));
    }
}

fn host_bytes(op: i32, me: *mut c_void, a: i64) -> (i64, Vec<u8>) {
    let mut v: Vec<u8> = Vec::new();
    let r = host_out(op, me, a, 0, 0, &mut v);
    (r, v)
}

/// A C string for the host (cut at an interior NUL, as `c_str()` reads it).
fn cstr(b: &[u8]) -> CString {
    let end = b.iter().position(|&c| c == 0).unwrap_or(b.len());
    CString::new(&b[..end]).unwrap_or_default()
}

fn now() -> c_ulong {
    host(EE_AI_NEXT_HOST_NOW, std::ptr::null_mut(), 0, 0, 0) as c_ulong
}

fn trace_on() -> bool {
    host(EE_AI_NEXT_HOST_TRACE_ON, std::ptr::null_mut(), 0, 0, 0) != 0
}

/// `EE_CP_TRACE(player, event, "%s", text)` under `manager` (None: the current one); the caller tests the trace.
fn trace_line(me: *mut c_void, manager: Option<&CStr>, event: &str, text: &[u8]) {
    let e = cstr(event.as_bytes());
    let t = cstr(text);
    host(EE_AI_NEXT_HOST_TRACE, me, manager.map_or(0, |m| m.as_ptr() as i64), e.as_ptr() as i64, t.as_ptr() as i64);
}

/// The binding trace's output (`ESDebugOut("%s", text)`).
///
/// # Safety
/// `text` is a C string.
pub(crate) unsafe extern "C" fn debug_out(text: *const c_char) {
    host(EE_AI_NEXT_HOST_DEBUG_OUT, std::ptr::null_mut(), text as i64, 0, 0);
}

fn trace(me: *mut c_void, manager: Option<&CStr>, event: &str, text: &str) {
    if trace_on() {
        trace_line(me, manager, event, text.as_bytes());
    }
}

const NEXT: Option<&CStr> = Some(c"next");

/// `gCPTraceManager = name` -> the previous manager (for `scope_end`)
fn scope(name: &'static CStr) -> i64 {
    host(EE_AI_NEXT_HOST_SCOPE, std::ptr::null_mut(), name.as_ptr() as i64, 0, 0)
}

fn scope_end(saved: i64) {
    host(EE_AI_NEXT_HOST_SCOPE, std::ptr::null_mut(), saved, 0, 0);
}

fn player_id(me: *mut c_void) -> c_ulong {
    host(EE_AI_NEXT_HOST_PLAYER_ID, me, 0, 0, 0) as c_ulong
}

fn class_name(t: c_long) -> Vec<u8> {
    host_bytes(EE_AI_NEXT_HOST_CLASS_NAME, std::ptr::null_mut(), t as i64).1
}

fn experiment(me: *mut c_void, name: &CStr) -> bool {
    host(EE_AI_NEXT_HOST_EXPERIMENT, me, name.as_ptr() as i64, 0, 0) != 0
}

fn in_danger(me: *mut c_void, x: c_long, y: c_long) -> bool {
    host(EE_AI_NEXT_HOST_IN_DANGER, me, x as i64, y as i64, 0) != 0
}

/// The placement's town centre tile (None: no placement; x -10000: no town centre)
fn town(me: *mut c_void) -> Option<(c_long, c_long)> {
    let mut xy = [0i64; 2];
    (host_out(EE_AI_NEXT_HOST_TOWN, me, 0, 0, 0, &mut xy) != 0).then_some((xy[0] as c_long, xy[1] as c_long))
}

/// The town centre tile when there is one
fn town_tile(me: *mut c_void) -> Option<(c_long, c_long)> {
    town(me).filter(|t| t.0 != NO_TILE)
}

// ---------------------------------------------------------------------------------------------------------------
// The game's reads

const NO_TILE: c_long = -10000;
const RESOURCE_FOOD: c_long = 2;
const RESOURCE_WOOD: c_long = 3;
const CAT_LAND: u64 = 5;
const CAT_LAND_CITIZEN: u64 = 6;
const CAT_NAVAL_CITIZEN: u64 = 0x16;
const CAT_HERO: u64 = 0x17;
const FAMILY_ANIMAL: u64 = 0x11;
const FAMILY_FISH: u64 = 0x12;
const MOVEMENT_SEA: u8 = 4;
const TYPE_TOWN_CENTER: u64 = 0x23;
const TYPE_DOCK: u64 = 0x24;
const TYPE_CAPITOL: u64 = 0x29;
const TYPE_FARM: u64 = 0x2a;
const TYPE_SETTLEMENT: u64 = 0x2b;
const TYPE_GRANARY: u64 = 0x34;
const TYPE_FORAGE_BUSH: u64 = 0x37;
const TYPE_NAVY_YARD: u64 = 0x41;
const TYPE_MECH_ZEUS: u64 = 0xc2;
const TYPE_FISH: u64 = 0x13b;
const TYPE_ROBOTIC_FARM: u64 = 0x33a;
const TECH_NONE: i64 = 100000;
const GOAL_LOCATION: u8 = 1;
const GOAL_ATTACK_UNIT: u8 = 2;
const GOAL_BUILD_REPAIR: u8 = 4;
const GOAL_GATHER: u8 = 6;

/// `kNextSiteDangerRange`, `kNextTowerRange`, `kNextHuntMax`, `kNextHomeRange`, `kNextDropReach`,
/// `kNextDropSlack`, `kNextWoodSlack`, `kNextPatchPlaces`, `kNextHuntRange`, `kNextRouteStep`, `kNextRouteMargin`,
/// `kNextBuildHome`
const SITE_DANGER_RANGE: c_long = 12;
const TOWER_RANGE: c_long = 8;
const HUNT_MAX: c_long = 30;
const HOME_RANGE: c_long = 20;
const DROP_REACH: c_long = 3;
const DROP_SLACK: c_long = 1;
const WOOD_SLACK: c_long = 2;
const PATCH_PLACES: c_long = 6;
const HUNT_RANGE: c_long = 24;
const ROUTE_STEP: c_long = 2;
const ROUTE_MARGIN: c_long = 1;
const BUILD_HOME: c_long = 20;

fn body(c: &ClassView<'_>, which: usize) -> u8 {
    let k = c.category();
    if k < 256 {
        EE_UNIT_CLASS_BODIES[which][k as usize]
    } else {
        0
    }
}

/// `EEUnitClass::IsBuilding`
fn is_building(c: &ClassView<'_>) -> bool {
    body(c, EE_UNITS_BODY_IS_BUILDING) == 1
}

/// `EEUnitClass::IsLandBuilding`
fn is_land_building(c: &ClassView<'_>) -> bool {
    matches!(body(c, EE_UNITS_BODY_IS_LAND_BUILDING), 1 | 3)
}

/// `EEUnitClass::IsDropSite`
fn is_drop_site(c: &ClassView<'_>) -> bool {
    let t = c.unit_type();
    let r: c_long = crate::world_field!(c.fields(), mResourceType);
    t == TYPE_MECH_ZEUS
        || (is_land_building(c) && t != TYPE_FARM && t != TYPE_DOCK && t != TYPE_NAVY_YARD && r > 0 && r < 7)
}

fn is_tower(c: &ClassView<'_>) -> bool {
    crate::world_field!(c.fields(), mIsTower)
}

fn domains(c: &ClassView<'_>) -> u8 {
    crate::world_field!(c.fields(), mDomains)
}

fn tile_size(c: &ClassView<'_>) -> c_long {
    crate::world_field!(c.fields(), mTileSize)
}

fn class_resource(c: &ClassView<'_>) -> c_long {
    crate::world_field!(c.fields(), mResourceType)
}

fn attack(u: &UnitView<'_>) -> Option<f32> {
    u.stats().map(|s| crate::world_field!(s.fields(), mAttack))
}

fn armed(u: &UnitView<'_>) -> bool {
    attack(u).is_some_and(|a| a != 0.0)
}

fn hp(u: &UnitView<'_>) -> c_long {
    u.hit_points() as c_long
}

fn xy(u: &UnitView<'_>) -> (c_long, c_long) {
    let (x, y) = u.tile_xy();
    (x as c_long, y as c_long)
}

/// hit points x attack (`EECPNextStrength`, as EECPArmyThreat weighs an army)
fn strength(u: &UnitView<'_>) -> c_long {
    hp(u).wrapping_mul(um::lrintf(attack(u).unwrap_or(0.0)) as c_long)
}

/// `EECPNextIsSoldier` / `EECPNextArmySoldier`: a living land soldier
fn is_soldier(u: &UnitView<'_>) -> bool {
    let Some(c) = u.class() else { return false };
    if u.complex_unit_handle() == 0 || hp(u) <= 0 || !armed(u) || is_building(&c) {
        return false;
    }
    matches!(c.category(), CAT_LAND | CAT_HERO)
}

/// `EECPNextResourceLeft`: some left, on the map
fn resource_left(u: &UnitView<'_>) -> bool {
    u.resource_amount() > 0 && u.tile().is_some() && xy(u).0 != NO_TILE
}

/// `EECPNextShootRange`: the range in tiles of a building that shoots (0: it does not)
fn shoot_range(u: Option<&UnitView<'_>>) -> c_long {
    let Some(u) = u else { return 0 };
    let Some(st) = u.stats() else { return 0 };
    let a: f32 = crate::world_field!(st.fields(), mAttack);
    if hp(u) <= 0 || a == 0.0 {
        return 0;
    }
    let r = um::lrintf(st.range()) as c_long;
    if r > 0 {
        r
    } else {
        1
    }
}

struct G<'t> {
    t: &'t WorldRead,
    world: WorldView<'t>,
    me: PlayerView<'t>,
    me_ptr: *mut c_void,
}

impl<'t> G<'t> {
    fn new(t: &'t WorldRead, me_ptr: *mut c_void) -> Option<G<'t>> {
        let world = WorldView::global(t)?;
        // SAFETY: the C++'s EEComputerPlayer*, whose EEPlayer and RTSPlayer start at it
        let me = unsafe { PlayerView::of(t, me_ptr as u64) }?;
        Some(G { t, world, me, me_ptr })
    }
    fn unit(&self, handle: u64) -> Option<UnitView<'t>> {
        // SAFETY: a live unit the C++ gave (the lookup, a pointer member) or 0
        unsafe { UnitView::of(self.t, handle) }
    }
    fn by_id(&self, id: c_long) -> Option<UnitView<'t>> {
        self.unit(host(EE_AI_NEXT_HOST_UNIT, std::ptr::null_mut(), id as i64, 0, 0) as u64)
    }
    fn id(&self) -> c_ulong {
        self.me.id() as c_ulong
    }
    fn is_me(&self, p: &PlayerView<'_>) -> bool {
        p.handle() == self.me.handle()
    }
    /// `EEPlayer::IsAlly`
    fn is_ally(&self, other: &PlayerView<'_>) -> bool {
        self.is_me(other) || self.me.enemy(other.id() as usize) == 0
    }
    /// `EETile::IsSeenBy`
    fn seen(&self, tile: &crate::world::TileView<'_>) -> bool {
        tile.visibility(self.me.id() as usize) > 1 || self.me.sees_all()
    }
    /// `EETile::IsVisibleTo`
    fn visible(&self, tile: &crate::world::TileView<'_>) -> bool {
        tile.visibility(self.me.id() as usize) > 0 || self.me.sees_all()
    }
    fn unit_seen(&self, u: &UnitView<'_>) -> bool {
        u.tile().is_some_and(|t| self.seen(&t))
    }
    fn unit_visible(&self, u: &UnitView<'_>) -> bool {
        u.tile().is_some_and(|t| self.visible(&t))
    }
    fn players(&self) -> u64 {
        self.world.num_players()
    }
    /// `gWorld.mPlayers[p]` for p below `mNumPlayers` (and 64)
    fn player(&self, p: u64) -> Option<PlayerView<'t>> {
        if p >= self.players() {
            return None;
        }
        self.world.player(p as usize)
    }
    /// the other players the scans read: neither the player, an ally nor an observer (ids 1..63)
    fn enemy(&self, p: u64) -> Option<PlayerView<'t>> {
        let o = self.player(p)?;
        (!self.is_ally(&o) && !o.observer()).then_some(o)
    }
    fn tree(&self) -> Option<TechTreeView<'t>> {
        self.me.tech_tree()
    }
    fn mining_limit(&self) -> c_long {
        let m: c_long = crate::world_field!(self.me.fields(), mMiningLimit);
        if m > 0 {
            m
        } else {
            1
        }
    }
    /// The active goal's (`GetLoadGoal`) kind and its target (`which`: 0 the gathered resource, 1 the attacked unit,
    /// 2 the built one) when the kind is `kind`
    fn goal_target(&self, c: &UnitView<'_>, kind: u8, which: i64) -> Option<UnitView<'t>> {
        let (k, _) = c.active_goal()?;
        if k != kind {
            return None;
        }
        self.unit(host(EE_AI_NEXT_HOST_GOAL_UNIT, std::ptr::null_mut(), c.active_goal_handle() as i64, which, 0) as u64)
    }
    fn tile_seen_or_none(&self, x: c_long, y: c_long) -> bool {
        match self.world.map().and_then(|m| m.tile(x as i64, y as i64)) {
            None => true,
            Some(t) => self.seen(&t),
        }
    }
}

fn rc_count(me: *mut c_void) -> usize {
    host(EE_AI_NEXT_HOST_RC_COUNT, me, 0, 0, 0).max(0) as usize
}

/// The centre `k`: its unit (handle), whether it is active, its resource units (handles)
fn rc(me: *mut c_void, k: usize) -> (u64, bool, Vec<u64>) {
    let mut v = [0i64; 3];
    host_out(EE_AI_NEXT_HOST_RC, me, k as i64, 0, 0, &mut v);
    let units = (0..v[2].max(0)).map(|u| host(EE_AI_NEXT_HOST_RC_UNIT, me, k as i64, u, 0) as u64).collect();
    (v[0] as u64, v[1] != 0, units)
}

// ---------------------------------------------------------------------------------------------------------------
// The state of a player

/// The player's next state (`EECPNextGetState`: made when missing).
fn state_of(me: *mut c_void) -> *mut NextState {
    host(EE_AI_NEXT_HOST_STATE, me, 0, 0, 0) as usize as *mut NextState
}

/// A new state (`new EECPNextState`).
#[no_mangle]
pub extern "C" fn ee_ai_next_state_new() -> *mut c_void {
    Box::into_raw(Box::new(NextState::default())) as *mut c_void
}

/// Frees a state (its Lua states with it).
///
/// # Safety
/// `s` came from `ee_ai_next_state_new` (or is null) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_state_free(s: *mut c_void) {
    if !s.is_null() {
        drop(Box::from_raw(s as *mut NextState));
    }
}

/// The state's AI id (`mAI`): `len` bytes at the result, valid until the state changes.
///
/// # Safety
/// `s` is a live state; `len` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_state_ai(s: *const c_void, len: *mut usize) -> *const u8 {
    let s = &*(s as *const NextState);
    *len = s.ai.len();
    s.ai.as_ptr()
}

/// Sets the state's AI id.
///
/// # Safety
/// `s` is a live state; `data` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_state_set_ai(s: *mut c_void, data: *const u8, len: usize) {
    let s = &mut *(s as *mut NextState);
    s.ai = if len == 0 { Vec::new() } else { std::slice::from_raw_parts(data, len).to_vec() };
}

/// `EECPNextIncome`: a resource index 0..4 got `amount`.
///
/// # Safety
/// `s` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_income(s: *mut c_void, index: c_long, amount: c_long) {
    (*(s as *mut NextState)).income(index, amount);
}

/// `EECPNextEventAdd`.
///
/// # Safety
/// `s` is a live state.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_event_add(s: *mut c_void, kind: c_long, unit_type: c_long, count: c_long) {
    (*(s as *mut NextState)).event_add(kind, unit_type, count);
}

// ---------------------------------------------------------------------------------------------------------------
// The gather safety (EECPNextView.cpp's)

fn tower_look<'a>(g: &'a G<'_>) -> impl FnMut(c_long) -> (c_long, Option<c_long>) + 'a {
    move |id| {
        let u = g.by_id(id);
        let range = shoot_range(u.as_ref());
        let half = u.as_ref().map(|u| u.class().map_or(0, |c| tile_size(&c) / 2));
        (range, half)
    }
}

/// `EECPNextSiteThreat`
fn site_threat(g: &G<'_>, s: &NextState, x: c_long, y: c_long, now_only: bool) -> (c_long, c_long) {
    let mut unit = s.threat_near(x, y, now_only);
    let mut kind = if unit != 0 { BLOCKED_ENEMY_NEAR } else { BLOCKED_NONE };
    if unit == 0 && !now_only {
        unit = s.tower_at(x, y, TOWER_MARGIN, &mut tower_look(g));
        if unit != 0 {
            kind = BLOCKED_TOWER;
        }
    }
    (unit, kind)
}

/// `EECPNextRouteTower`
fn route_tower(g: &G<'_>, s: &NextState, x: c_long, y: c_long) -> c_long {
    if s.towers.is_empty() && s.seen.is_empty() {
        return 0;
    }
    let Some((tx, ty)) = town_tile(g.me_ptr) else { return 0 };
    let length = isqrt(d2(x, y, tx, ty));
    let mut d = ROUTE_STEP;
    while d < length {
        let px = tx.wrapping_add(x.wrapping_sub(tx).wrapping_mul(d) / length);
        let py = ty.wrapping_add(y.wrapping_sub(ty).wrapping_mul(d) / length);
        let tower = s.tower_at(px, py, ROUTE_MARGIN, &mut tower_look(g));
        if tower != 0 {
            return tower;
        }
        d += ROUTE_STEP;
    }
    0
}

/// `EECPNextBuildUnsafe`
fn build_unsafe(g: &G<'_>, s: &NextState, x: c_long, y: c_long) -> c_long {
    if s.tower_at(x, y, TOWER_MARGIN, &mut tower_look(g)) != 0 || route_tower(g, s, x, y) != 0 {
        return BLOCKED_TOWER;
    }
    if let Some((tx, ty)) = town_tile(g.me_ptr) {
        if d2(x, y, tx, ty) <= (BUILD_HOME * BUILD_HOME) as i64 {
            return BLOCKED_NONE;
        }
    }
    let (unit, kind) = site_threat(g, s, x, y, false);
    if unit != 0 {
        return kind;
    }
    if in_danger(g.me_ptr, x, y) {
        return BLOCKED_DANGER;
    }
    BLOCKED_NONE
}

/// `EECPNextSiteBlocked` -> (the reason, the unit id that causes it)
fn site_blocked(g: &G<'_>, s: &NextState, u: &UnitView<'_>) -> (c_long, c_long) {
    if !g.unit_visible(u) {
        return (BLOCKED_UNEXPLORED, 0);
    }
    let (x, y) = xy(u);
    let home = s.is_home(u.id() as c_long);
    let (cause, kind) = site_threat(g, s, x, y, home);
    if cause != 0 {
        return (kind, cause);
    }
    if home {
        return (BLOCKED_NONE, 0);
    }
    let cause = route_tower(g, s, x, y);
    if cause != 0 {
        return (BLOCKED_TOWER, cause);
    }
    if in_danger(g.me_ptr, x, y) {
        return (BLOCKED_DANGER, 0);
    }
    for j in 0..g.players() {
        let Some(o) = g.player(j) else { continue };
        if g.is_me(&o) || o.enemy(g.me.id() as usize) != 0 {
            continue;
        }
        let mut p = [0i64; 2];
        if host_out(EE_AI_NEXT_HOST_AREA_HULL, std::ptr::null_mut(), o.handle() as i64, x as i64, y as i64, &mut p) == 0
        {
            continue;
        }
        let dx = (p[0] as c_long).wrapping_sub(x) as i64;
        let dy = (p[1] as c_long).wrapping_sub(y) as i64;
        if dx * dx + dy * dy <= 100 {
            return (BLOCKED_AREA, 0);
        }
    }
    (BLOCKED_NONE, 0)
}

/// `EECPNextAssigned`: the player's living land citizens whose gather goal is the resource
fn assigned(g: &G<'_>, resource: u64) -> c_long {
    let mut n = 0;
    for u in g.me.units() {
        let Some(c) = u.complex_unit() else { continue };
        if u.class().map(|k| k.category()) != Some(CAT_LAND_CITIZEN) || hp(&u) <= 0 {
            continue;
        }
        if g.goal_target(&c, GOAL_GATHER, 0).is_some_and(|r| r.handle() == resource) {
            n += 1;
        }
    }
    n
}

/// `EECPNextHasRoom`
fn has_room(g: &G<'_>, resource: &UnitView<'_>) -> bool {
    let limit = g.mining_limit();
    let users = host(EE_AI_NEXT_HOST_RESOURCE_USERS, std::ptr::null_mut(), resource.handle() as i64, 0, 0) as c_long;
    if users >= limit || assigned(g, resource.handle()) >= limit {
        return false;
    }
    if resource.class().map(|c| c.unit_type()) != Some(TYPE_FORAGE_BUSH) {
        return true;
    }
    // the resource centre holding it
    let me = g.me_ptr;
    let mut centre = None;
    for k in 0..rc_count(me) {
        let (_, _, units) = rc(me, k);
        if units.contains(&resource.handle()) {
            centre = Some(units);
            break;
        }
    }
    let Some(units) = centre else { return true };
    let mut patch = 0;
    for h in units {
        if g.unit(h).and_then(|u| u.class()).map(|c| c.unit_type()) == Some(TYPE_FORAGE_BUSH) {
            patch += assigned(g, h);
        }
    }
    patch < PATCH_PLACES
}

/// `EECPNextReadyDrops`: the player's finished drop sites (food: granaries too), x, y (the centre tile) and half
/// the size
fn ready_drops(g: &G<'_>, food: bool) -> Vec<c_long> {
    let mut out = Vec::new();
    for u in g.me.units() {
        let Some(c) = u.class() else { continue };
        let t = c.unit_type();
        if t != TYPE_TOWN_CENTER && t != TYPE_CAPITOL && t != TYPE_SETTLEMENT && !(food && t == TYPE_GRANARY) {
            continue;
        }
        if !u.complex_unit().is_some_and(|k| k.is_built()) || hp(&u) <= 0 || u.tile().is_none() {
            continue;
        }
        let half = tile_size(&c) / 2;
        let (x, y) = xy(&u);
        out.extend_from_slice(&[x.wrapping_add(half), y.wrapping_add(half), half]);
    }
    out
}

/// `EECPNextDropIn`
fn drop_in(drops: &[c_long], x: c_long, y: c_long, slack: c_long) -> bool {
    drops.as_chunks::<3>().0.iter().any(|d| {
        let reach = DROP_REACH + slack + d[2];
        d2(x, y, d[0], d[1]) <= (reach as i64) * (reach as i64)
    })
}

/// `EECPNextUnitDropReady`
fn unit_drop_ready(g: &G<'_>, u: &UnitView<'_>) -> bool {
    if u.tile().is_none() {
        return false;
    }
    let (x, y) = xy(u);
    drop_in(&ready_drops(g, u.resource_type() as c_long == RESOURCE_FOOD), x, y, DROP_SLACK)
}

/// `EECPNextUpdateHome`
fn update_home(g: &G<'_>, s: &mut NextState) {
    let Some((tx, ty)) = town_tile(g.me_ptr) else {
        s.home.clear();
        return;
    };
    const WANTED: [bool; 5] = [true, false, true, true, true];
    let mut found: [Vec<(i64, c_long)>; 5] = Default::default();
    for k in 0..rc_count(g.me_ptr) {
        for h in rc(g.me_ptr, k).2 {
            let Some(u) = g.unit(h) else { continue };
            let r = u.resource_type() as c_long - RESOURCE_FOOD;
            if !(0..5).contains(&r) || !WANTED[r as usize] || !resource_left(&u) {
                continue;
            }
            if r == 0 && u.class().map(|c| c.unit_type()) != Some(TYPE_FORAGE_BUSH) {
                continue;
            }
            let (x, y) = xy(&u);
            let d = d2(x, y, tx, ty);
            if d <= (HOME_RANGE * HOME_RANGE) as i64 {
                found[r as usize].push((d, u.id() as c_long));
            }
        }
    }
    s.set_home(found);
}

// ---------------------------------------------------------------------------------------------------------------
// The forests by the town

fn forest_point(i: u16, x: c_long, y: c_long, hull: bool) -> (c_long, c_long) {
    let mut p = [0i64, 0, hull as i64];
    host_out(EE_AI_NEXT_HOST_FOREST_POINT, std::ptr::null_mut(), i as i64, x as i64, y as i64, &mut p);
    (p[0] as c_long, p[1] as c_long)
}

/// `EECPNextSortForests`
fn sort_forests(g: &G<'_>, s: &mut NextState) {
    let Some(town) = town(g.me_ptr) else { return };
    let now = now();
    if !s.forests.is_empty()
        && s.forests_x == town.0
        && s.forests_y == town.1
        && now >= s.forests_time
        && now - s.forests_time < 60000
    {
        return;
    }
    s.forests.clear();
    s.forests_x = town.0;
    s.forests_y = town.1;
    s.forests_time = now;
    let n = host(EE_AI_NEXT_HOST_FORESTS, std::ptr::null_mut(), 0, 0, 0).max(0);
    let mut order: Vec<(i64, u16)> = Vec::new();
    for i in 0..n {
        let id = host(EE_AI_NEXT_HOST_FOREST, std::ptr::null_mut(), (i as u16) as i64, 0, 0);
        if id < 0 {
            continue;
        }
        let mut key = 0i64;
        if town.0 != NO_TILE {
            let (px, py) = forest_point(i as u16, town.0, town.1, true);
            key = d2(px, py, town.0, town.1);
        }
        order.push((key, id as u16));
    }
    order.sort_by_key(|o| o.0); // stable
    s.forests.extend(order.iter().map(|o| o.1));
}

/// `EECPNextTreeAt`
fn tree_at<'t>(g: &G<'t>, x: c_long, y: c_long, with_wood: bool) -> Option<UnitView<'t>> {
    let tile = g.world.map()?.tile(x as i64, y as i64)?;
    tile.units().flatten().find(|u| {
        u.class().is_some_and(|c| class_resource(&c) == RESOURCE_WOOD) && (!with_wood || u.resource_amount() > 0)
    })
}

fn has_map(g: &G<'_>) -> bool {
    g.world.map().is_some()
}

/// `EECPNextForestTarget`
fn forest_target<'t>(g: &G<'t>, s: &mut NextState) -> Option<UnitView<'t>> {
    let center = town_tile(g.me_ptr)?;
    if !has_map(g) {
        return None;
    }
    sort_forests(g, s);
    for &id in &s.forests {
        if host(EE_AI_NEXT_HOST_FOREST, std::ptr::null_mut(), id as i64, 0, 0) < 0 {
            continue;
        }
        let (px, py) = forest_point(id, center.0, center.1, false);
        if let Some(t) = tree_at(g, px, py, false) {
            return Some(t);
        }
    }
    None
}

/// `EECPNextWoodTarget`
fn wood_target<'t>(g: &G<'t>, s: &mut NextState) -> Option<UnitView<'t>> {
    if let Some(t) = forest_target(g, s) {
        let (x, y) = xy(&t);
        if drop_in(&ready_drops(g, false), x, y, WOOD_SLACK) {
            return Some(t);
        }
    }
    let town = town_tile(g.me_ptr)?;
    if !has_map(g) {
        return None;
    }
    let drops = ready_drops(g, false);
    let mut order: Vec<(i64, usize)> =
        (0..drops.len() / 3).map(|k| (d2(drops[3 * k], drops[3 * k + 1], town.0, town.1), 3 * k)).collect();
    order.sort();
    for &(_, d) in &order {
        let (dx, dy) = (drops[d], drops[d + 1]);
        let reach = DROP_REACH + WOOD_SLACK + drops[d + 2];
        let mut best: Option<UnitView<'t>> = None;
        let mut best_d = 0i64;
        for &id in &s.forests {
            if host(EE_AI_NEXT_HOST_FOREST, std::ptr::null_mut(), id as i64, 0, 0) < 0 {
                continue;
            }
            let (px, py) = forest_point(id, dx, dy, false);
            let dist = d2(px, py, dx, dy);
            if dist > (reach as i64) * (reach as i64) || (best.is_some() && dist >= best_d) {
                continue;
            }
            if let Some(u) = tree_at(g, px, py, true) {
                best = Some(u);
                best_d = dist;
            }
        }
        if best.is_some() {
            return best;
        }
    }
    None
}

// ---------------------------------------------------------------------------------------------------------------
// The army's scan (EECPNextArmy.cpp's)

/// `EECPNextScan`: what the player sees now and remembers
fn scan(g: &G<'_>, s: &mut NextState, now: c_long) {
    s.scan_begin();
    let mut army = [(0i64, 0i64, 0 as c_long); 64];
    for p in 1..g.players().min(64) {
        let Some(o) = g.enemy(p) else { continue };
        for u in o.units() {
            let Some(c) = u.complex_unit() else { continue };
            if hp(&u) <= 0 || u.tile().is_none() || u.stats().is_none() || c.container_handle() != 0 || !g.unit_seen(&u)
            {
                continue;
            }
            let Some(k) = u.class() else { continue };
            let kind = if is_building(&k) {
                if is_tower(&k) {
                    SEEN_TOWER
                } else if !k.wall() {
                    SEEN_BUILDING
                } else {
                    -1
                }
            } else if k.category() == CAT_LAND_CITIZEN {
                SEEN_CITIZEN
            } else if k.category() == CAT_NAVAL_CITIZEN {
                SEEN_BOAT
            } else if is_soldier(&u) {
                SEEN_SOLDIER
            } else if domains(&k) & MOVEMENT_SEA != 0 && armed(&u) {
                SEEN_SHIP
            } else {
                -1
            };
            if kind < 0 {
                continue;
            }
            let (x, y) = xy(&u);
            let seen = Seen {
                unit: u.id() as c_long,
                player: p as c_long,
                x,
                y,
                kind,
                hit_points: hp(&u),
                strength: if matches!(kind, SEEN_SOLDIER | SEEN_TOWER | SEEN_SHIP) { strength(&u) } else { 0 },
                continent: if matches!(kind, SEEN_SOLDIER | SEEN_CITIZEN) {
                    host(EE_AI_NEXT_HOST_CONTINENT_OF, std::ptr::null_mut(), u.handle() as i64, 0, 0) as u16
                } else {
                    0
                },
            };
            s.seen.push(seen);
            match kind {
                SEEN_SOLDIER => {
                    let a = &mut army[p as usize];
                    a.0 += x as i64;
                    a.1 += y as i64;
                    a.2 += 1;
                }
                SEEN_CITIZEN | SEEN_BOAT => s.note_gatherer(p as c_long, x, y, now),
                SEEN_TOWER => s.note_tower(p as c_long, x, y, now, seen.unit),
                _ => {}
            }
        }
    }
    s.scan_end(now, &army, &mut |x, y| g.tile_seen_or_none(x, y));
}

/// `EECPNextArmyView`: the free soldiers and the enemies' army fields
fn army_view(g: &G<'_>, s: &NextState, v: &mut View) {
    let held: BTreeSet<c_long> = held_ids(g.me_ptr).into_iter().collect();
    v.free_soldiers = 0;
    v.free_strength = 0;
    for u in g.me.units() {
        if is_soldier(&u) && !held.contains(&(u.id() as c_long)) {
            v.free_soldiers += 1;
            v.free_strength = v.free_strength.wrapping_add(strength(&u));
        }
    }
    for e in v.enemies.iter_mut() {
        let p = e.player;
        e.towers = s.towers_of(p);
        let in_range = p > 0 && p < 64;
        e.citizens_seen = if in_range { s.seen_citizens[p as usize] } else { 0 };
        e.citizen_x = -1;
        e.citizen_y = -1;
        if let Some((x, y)) = s.best_spot(p, None) {
            e.citizen_x = x;
            e.citizen_y = y;
        }
        e.army_x = if in_range { s.army_x[p as usize] } else { -1 };
        e.army_y = if in_range { s.army_y[p as usize] } else { -1 };
    }
}

/// The ids the script holds (`EECPUnitsHeld`)
fn held_ids(me: *mut c_void) -> Vec<c_long> {
    let units = host(EE_AI_NEXT_HOST_UNITS_STATE, me, 0, 0, 0) as usize as *mut c_void;
    if units.is_null() {
        return Vec::new();
    }
    // SAFETY: the player's live per-unit state
    unsafe {
        let n = crate::ai::units_bind::ee_ai_units_held(units, std::ptr::null_mut(), 0);
        let mut ids = vec![0 as c_long; n];
        crate::ai::units_bind::ee_ai_units_held(units, ids.as_mut_ptr(), n);
        ids
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The view (EECPNextBuildView)

struct Place {
    x: c_long,
    y: c_long,
    reach: c_long,
    strength: c_long,
}

fn node_cost(me: *mut c_void, node: &NodeView<'_>, resource: c_long, class: c_long) -> c_long {
    host(EE_AI_NEXT_HOST_NODE_COST, me, node.handle() as i64, resource as i64, class as i64) as c_long
}

fn node_test(node: &NodeView<'_>, obsolete: bool) -> bool {
    host(EE_AI_NEXT_HOST_NODE_TEST, std::ptr::null_mut(), node.handle() as i64, obsolete as i64, 0) != 0
}

fn build_view(g: &G<'_>, s: &mut NextState) -> View {
    let me = g.me_ptr;
    let tree = g.tree();
    let now = now();
    s.update_threats(now as c_long);
    update_home(g, s);
    let mut v = View::default();
    let current = tree.as_ref().map_or(0, |t| t.current_epoch() as c_long);
    let epochs = host(EE_AI_NEXT_HOST_EPOCH_COUNT, me, 0, 0, 0) as c_long;
    let epoch_end = host(EE_AI_NEXT_HOST_EPOCH_END, me, 0, 0, 0) as c_long;
    v.player = g.id() as c_long;
    v.time = now as c_long;
    v.epoch = current;
    v.epoch_start = s.epoch_start as c_long;
    v.last_epoch = if epoch_end < epochs - 1 { epoch_end } else { epochs - 1 };
    v.advancing = (host(EE_AI_NEXT_HOST_PENDING_EPOCH, me, 0, 0, 0) != TECH_NONE) as c_long;
    let caps: [c_long; 7] = crate::world_field!(g.me.saved(), mResourceCaps);
    for i in 0..5 {
        v.stock[i] = caps[RESOURCE_FOOD as usize + i];
        v.income[i] = s.income_last(i);
        v.income_total[i] = s.income_total[i];
    }
    v.population = host(EE_AI_NEXT_HOST_POPULATION, me, 0, 0, 0) as c_long;
    v.population_cap = host(EE_AI_NEXT_HOST_POPULATION, me, 1, 0, 0) as c_long;
    let town = town_tile(me);
    let (town_x, town_y) = town.unwrap_or((-1, -1));
    v.town_x = town_x;
    v.town_y = town_y;
    v.claim_range = next_value::CLAIM_RANGE;

    // the player's own units: citizens, boats, buildings, soldiers, drop sites and towers
    let mut buildings: BTreeMap<c_long, next_value::Buildings> = BTreeMap::new();
    let mut soldiers: BTreeMap<c_long, Soldiers> = BTreeMap::new();
    let mut in_production: BTreeMap<c_long, c_long> = BTreeMap::new();
    let mut built: BTreeMap<c_long, c_long> = BTreeMap::new();
    let (mut drops, mut food_drops, mut towers) = (Vec::new(), Vec::new(), Vec::new());
    for u in g.me.units() {
        let Some(c) = u.complex_unit() else { continue };
        if hp(&u) <= 0 {
            continue;
        }
        let Some(k) = u.class() else { continue };
        let t = k.unit_type() as c_long;
        let category = k.category();
        if category == CAT_LAND_CITIZEN {
            v.citizens += 1;
            let goal = c.active_goal();
            let resource = g.goal_target(&c, GOAL_GATHER, 0);
            let prey = g.goal_target(&c, GOAL_ATTACK_UNIT, 1);
            let hunting = resource.as_ref().and_then(|r| r.class()).is_some_and(|rc| rc.family() == FAMILY_ANIMAL)
                || prey.as_ref().and_then(|p| p.class()).is_some_and(|pc| pc.family() == FAMILY_ANIMAL);
            let r = match &resource {
                Some(r) => r.resource_type() as c_long - RESOURCE_FOOD,
                None if hunting => 0,
                None => -1,
            };
            if (0..5).contains(&r) {
                v.gatherers[r as usize] += 1;
            } else if goal.is_none() {
                v.idle += 1;
            }
            if hunting {
                v.hunters += 1;
            }
        } else if category == CAT_NAVAL_CITIZEN {
            v.boats += 1;
        }
        if is_building(&k) {
            let e = buildings.entry(t).or_default();
            e.unit_type = t;
            if c.is_built() {
                e.finished += 1;
                *built.entry(t).or_default() += 1;
                let queue = c.build_queue_len() as c_long;
                if queue > 0 {
                    e.busy += 1;
                }
                e.queue += queue;
                for q in 0..queue as usize {
                    let id = c.build_queue_technology(q).unwrap_or(0);
                    let made =
                        if id >= 0 && id < epochs as i64 { None } else { tree.as_ref().and_then(|tr| tr.node(id)) };
                    if let Some(d) = made.as_ref().and_then(|m| m.data()) {
                        let makes: bool = crate::world_field!(d, mMakesUnit);
                        if makes {
                            let class: c_long = crate::world_field!(d, mUnitClass);
                            *in_production.entry(class).or_default() += 1;
                        }
                    }
                }
            } else {
                e.unfinished += 1;
            }
            let half = tile_size(&k) / 2;
            let (x, y) = xy(&u);
            let place = || Place { x: x.wrapping_add(half), y: y.wrapping_add(half), reach: half, strength: 0 };
            let ut = t as u64;
            if ut == TYPE_TOWN_CENTER || ut == TYPE_CAPITOL || ut == TYPE_SETTLEMENT {
                drops.push(place());
                food_drops.push(place());
            } else if ut == TYPE_GRANARY {
                food_drops.push(place());
            }
            if is_tower(&k) {
                towers.push(place());
            }
        } else if is_soldier(&u) {
            let e = soldiers.entry(t).or_default();
            e.unit_type = t;
            e.count += 1;
            e.strength = e.strength.wrapping_add(strength(&u));
            v.army_count += 1;
            v.army_strength = v.army_strength.wrapping_add(strength(&u));
        }
    }
    v.buildings = buildings.into_values().collect();
    v.soldiers = soldiers.into_values().collect();

    // other players' units the player sees now: the threats near the town, the scouting
    let mut threats: BTreeMap<c_long, Threat> = BTreeMap::new();
    let mut enemy_soldiers = Vec::new();
    let range = v.claim_range;
    for p in 1..g.players().min(64) {
        let Some(o) = g.enemy(p) else { continue };
        let (mut seen, mut seen_strength) = (0 as c_long, 0 as c_long);
        let mut any = false;
        for u in o.units() {
            if u.complex_unit_handle() == 0 || hp(&u) <= 0 || u.tile().is_none() || !g.unit_seen(&u) {
                continue;
            }
            any = true;
            let t = u.class().map_or(u64::MAX, |c| c.unit_type());
            let (x, y) = xy(&u);
            if t == TYPE_TOWN_CENTER || t == TYPE_CAPITOL {
                s.seen_town_x[p as usize] = x;
                s.seen_town_y[p as usize] = y;
            }
            if !is_soldier(&u) {
                continue;
            }
            let st = strength(&u);
            seen += 1;
            seen_strength = seen_strength.wrapping_add(st);
            enemy_soldiers.push(Place { x, y, reach: 0, strength: st });
            if town.is_none() {
                continue;
            }
            let dd = d2(x, y, town_x, town_y);
            if dd > (range as i64) * (range as i64) {
                continue;
            }
            let th = threats.entry(p as c_long).or_default();
            let distance = isqrt(dd);
            if th.count == 0 || distance < th.distance {
                th.x = x;
                th.y = y;
                th.distance = distance;
            }
            th.player = p as c_long;
            th.count += 1;
            th.strength = th.strength.wrapping_add(st);
        }
        if any {
            s.last_seen[p as usize] = now;
        }
        if seen > 0 {
            s.seen_army[p as usize] = seen;
            s.seen_strength[p as usize] = seen_strength;
        }
        v.enemies.push(Enemy {
            player: p as c_long,
            epoch: o.tech_tree().map_or(0, |t| t.current_epoch() as c_long),
            seen_army: s.seen_army[p as usize],
            seen_strength: s.seen_strength[p as usize],
            last_seen: s.last_seen[p as usize] as c_long,
            town_x: s.seen_town_x[p as usize],
            town_y: s.seen_town_y[p as usize],
            ..Default::default()
        });
    }
    for th in threats.into_values() {
        v.threat_strength = v.threat_strength.wrapping_add(th.strength);
        v.threats.push(th);
    }

    // the resource centres
    let limit = g.mining_limit();
    let ready = ready_drops(g, false);
    let ready_food = ready_drops(g, true);
    for k in 0..rc_count(me) {
        let (unit, active, units) = rc(me, k);
        let units: Vec<UnitView<'_>> = units.iter().filter_map(|&h| g.unit(h)).collect();
        let mut site = Site { index: k as c_long, resource: -1, x: -1, y: -1, ..Default::default() };
        site.id = if let Some(u) = units.first() {
            u.id() as c_long
        } else if let Some(u) = g.unit(unit) {
            -(u.id() as c_long)
        } else {
            -(k as c_long) - 1
        };
        let mut counts = [0 as c_long; 5];
        for u in &units {
            let r = u.resource_type() as c_long - RESOURCE_FOOD;
            if resource_left(u) && (0..5).contains(&r) {
                counts[r as usize] += 1;
                if s.is_home(u.id() as c_long) {
                    site.home = 1;
                }
            }
        }
        site.holds = counts;
        v.site_units.push(units.iter().filter(|u| resource_left(u)).map(|u| u.id() as c_long).collect());
        for r in 0..5 {
            if counts[r] > 0 && (site.resource < 0 || counts[r] > counts[site.resource as usize]) {
                site.resource = r as c_long;
            }
        }
        site.claimed = active as c_long;
        if site.resource >= 0 {
            for u in &units {
                if !resource_left(u) || u.resource_type() as c_long != RESOURCE_FOOD + site.resource {
                    continue;
                }
                if site.units == 0 {
                    (site.x, site.y) = xy(u);
                    site.patch = (u.class().map(|c| c.unit_type()) == Some(TYPE_FORAGE_BUSH)) as c_long;
                    (site.blocked, site.cause) = site_blocked(g, s, u);
                }
                site.units += 1;
                site.amount = site.amount.wrapping_add(u.resource_amount() as c_long);
                site.workers += assigned(g, u.handle());
            }
            site.capacity = site.units.wrapping_mul(limit);
            if site.patch != 0 && site.capacity > PATCH_PLACES {
                site.capacity = PATCH_PLACES;
            }
            site.room = (site.capacity - site.workers).max(0);
        } else {
            let mut t = [0i64; 2];
            host_out(EE_AI_NEXT_HOST_RC_CENTER, me, k as i64, 0, 0, &mut t);
            site.x = t[0] as c_long;
            site.y = t[1] as c_long;
        }
        if site.x >= 0 && site.y >= 0 {
            if let Some(map) = g.world.map() {
                site.explored = map.tile(site.x as i64, site.y as i64).is_some_and(|t| g.visible(&t)) as c_long;
            }
        }
        for u in &units {
            let r = u.resource_type() as c_long - RESOURCE_FOOD;
            if !resource_left(u) || !(0..5).contains(&r) {
                continue;
            }
            let r = r as usize;
            let (ux, uy) = xy(u);
            if site.ready[r] == 0 && drop_in(if r == 0 { &ready_food } else { &ready }, ux, uy, DROP_SLACK) {
                site.ready[r] = 1;
            }
            let list = if r == 0 { &food_drops } else { &drops };
            for d in list {
                if site.drop[r] != 0 {
                    break;
                }
                let reach = DROP_REACH + DROP_SLACK + d.reach;
                if d2(ux, uy, d.x, d.y) <= (reach as i64) * (reach as i64) {
                    site.drop[r] = 1;
                }
            }
        }
        site.distance = if town.is_some() { isqrt(d2(site.x, site.y, town_x, town_y)) } else { 0 };
        let list = if site.resource == 0 { &food_drops } else { &drops };
        for d in list {
            if site.has_drop != 0 {
                break;
            }
            let reach = DROP_REACH + d.reach;
            site.has_drop = (d2(site.x, site.y, d.x, d.y) <= (reach as i64) * (reach as i64)) as c_long;
        }
        for t in &towers {
            if site.has_tower != 0 {
                break;
            }
            site.has_tower = (d2(site.x, site.y, t.x, t.y) <= (TOWER_RANGE * TOWER_RANGE) as i64) as c_long;
        }
        for e in &enemy_soldiers {
            if d2(site.x, site.y, e.x, e.y) <= (SITE_DANGER_RANGE * SITE_DANGER_RANGE) as i64 {
                site.danger = site.danger.wrapping_add(e.strength);
            }
        }
        if in_danger(me, site.x, site.y) {
            site.danger += 1;
        }
        v.sites.push(site);
    }

    // the next epoch
    if let Some(tr) = &tree {
        let next = current + 1;
        if next < epochs && (next as i64) < tr.epochs() && current < epoch_end {
            let epoch = tr.epoch(next as i64);
            if let Some(e) = &epoch {
                if let Some(node) = tr.node(e.id()) {
                    for i in 0..5 {
                        v.next_epoch_cost[i] =
                            node_cost(me, &node, RESOURCE_FOOD + i as c_long, TYPE_TOWN_CENTER as c_long);
                    }
                }
                let missing = (e.required_sol() - e.current_sol()) as c_long;
                v.sol_missing = missing.max(0);
            }
            if next + 1 < epochs && ((next + 1) as i64) < tr.epochs() && next < epoch_end {
                if let Some(node2) = tr.epoch(next as i64 + 1).and_then(|e2| tr.node(e2.id())) {
                    for i in 0..5 {
                        v.epoch2_cost[i] =
                            node_cost(me, &node2, RESOURCE_FOOD + i as c_long, TYPE_TOWN_CENTER as c_long);
                    }
                }
            }
        }

        // what the player can make now
        let has_combat = host(EE_AI_NEXT_HOST_HAS_COMBAT, me, 0, 0, 0) != 0;
        for c in 0..ClassView::count(g.t) {
            let Some(class) = ClassView::by_index(g.t, c) else { continue };
            let Some(node) = tr.node(class.technology()) else { continue };
            let Some(data) = node.data() else { continue };
            let makes: bool = crate::world_field!(data, mMakesUnit);
            let made: c_long = crate::world_field!(data, mUnitClass);
            if !makes || made != c as c_long || !node_test(&node, false) || node_test(&node, true) {
                continue;
            }
            let mut cost = ClassCost { unit_type: c as c_long, ..Default::default() };
            for i in 0..5 {
                cost.cost[i] = node_cost(me, &node, RESOURCE_FOOD + i as c_long, c as c_long);
            }
            cost.building = is_building(&class) as c_long;
            cost.military = (has_combat
                // SAFETY: a live class of the table
                && unsafe { crate::ai::military::ee_ai_military_class(class.handle()) })
                as c_long;
            let n = node.unit_classes_len();
            cost.producer = if n > 0 { node.unit_class(0) as c_long } else { -1 };
            for i in 0..n {
                if let Some(m) = built.get(&(node.unit_class(i) as c_long)) {
                    cost.producers += m;
                }
            }
            cost.in_production = in_production.get(&(c as c_long)).copied().unwrap_or(0);
            let counts: bool = crate::world_field!(data, mCountsTowardEpoch);
            cost.counts_toward_epoch = counts as c_long;
            v.costs.push(cost);
        }
    }

    // §8.35: the game near the town, the hunters, and the forest edge the wood gatherers go to
    v.wood_x = -1;
    v.wood_y = -1;
    if town.is_some() {
        let reach = next_value::CLAIM_RANGE.max(HUNT_RANGE);
        if let Some(gaia) = g.world.player(0) {
            for a in gaia.units() {
                if v.hunt >= HUNT_MAX {
                    break;
                }
                let Some(k) = a.class() else { continue };
                if k.family() != FAMILY_ANIMAL
                    || a.resource_type() as c_long != RESOURCE_FOOD
                    || a.resource_amount() <= 0
                    || hp(&a) <= 0
                    || a.tile().is_none()
                    || attack(&a).unwrap_or(0.0) != 0.0
                {
                    continue;
                }
                let (x, y) = xy(&a);
                if d2(x, y, town_x, town_y) > (reach as i64) * (reach as i64) {
                    continue;
                }
                if !g.unit_visible(&a) || site_blocked(g, s, &a).0 > BLOCKED_UNEXPLORED {
                    continue;
                }
                v.hunt += 1;
            }
        }
        v.wood_ready = wood_target(g, s).is_some() as c_long;
        if let Some(t) = forest_target(g, s) {
            if t.tile().is_some() {
                (v.wood_x, v.wood_y) = xy(&t);
                v.wood_distance = isqrt(d2(v.wood_x, v.wood_y, town_x, town_y));
                for d in &drops {
                    if v.wood_drop != 0 {
                        break;
                    }
                    let reach = DROP_REACH + d.reach;
                    v.wood_drop = (d2(v.wood_x, v.wood_y, d.x, d.y) <= (reach as i64) * (reach as i64)) as c_long;
                }
            }
        }
    }

    v.events = s.events.iter().map(|e| ViewEvent { kind: e.kind, unit_type: e.unit_type, count: e.count }).collect();
    army_view(g, s, &mut v);
    if trace_on() && now % 60000 < 1000 && experiment(me, c"sitetrace") {
        trace_sites(g, &v);
    }
    v
}

/// TEST ONLY (EE_AI_EXP sitetrace): one line a minute for each site the gatherers would pass over but for being out
/// of sight
fn trace_sites(g: &G<'_>, v: &View) {
    const REASONS: [&str; 6] = ["ok", "unexplored", "enemy_near", "danger", "area", "tower"];
    const NAMES: [&str; 5] = ["food", "wood", "stone", "gold", "iron"];
    for site in &v.sites {
        if site.resource < 0 || site.blocked < BLOCKED_ENEMY_NEAR || site.blocked > BLOCKED_TOWER {
            continue;
        }
        let head = format!(
            "site={} res={} d={} claimed={} home={} blocked={}",
            site.index + 1,
            NAMES[site.resource as usize],
            site.distance,
            site.claimed,
            site.home,
            REASONS[site.blocked as usize]
        );
        let cause = if site.cause != 0 { g.by_id(site.cause) } else { None };
        let Some((c, k, owner)) = cause.and_then(|c| Some((c, c.class()?, c.player()?))) else {
            trace(g.me_ptr, None, "next.sitecheck", &format!("{head} cause=-"));
            continue;
        };
        let (cx, cy) = xy(&c);
        trace(
            g.me_ptr,
            None,
            "next.sitecheck",
            &format!(
                "{head} cause={} owner=p{} cat={} cd={} seen={}",
                String::from_utf8_lossy(&class_name(k.unit_type() as c_long)),
                owner.id(),
                k.category() as c_long,
                isqrt(d2(site.x, site.y, cx, cy)),
                g.unit_seen(&c) as i32
            ),
        );
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The citizens' losses (the AI trace only: nothing here is read by the game)

#[derive(Clone, Copy, Default)]
struct LossHit {
    first: c_long,
    last: c_long,
    what: c_long,
    incident: c_long,
}

#[derive(Clone, Copy, Default)]
struct Incident {
    id: c_long,
    x: c_long,
    y: c_long,
    distance: c_long,
    start: c_long,
    last: c_long,
    hits: c_long,
    lost: c_long,
    foes: c_long,
    foe_strength: c_long,
    attacker: c_long,
    guard0: c_long,
    own0: c_long,
    tower0: c_long,
    guard_at: c_long,
}

#[derive(Default)]
struct Losses {
    hits: BTreeMap<c_long, LossHit>,
    incidents: Vec<Incident>,
    next_id: c_long,
    time: c_long,
}

static LOSSES: Mutex<Vec<Losses>> = Mutex::new(Vec::new());

fn with_losses<R>(id: usize, now: c_long, f: impl FnOnce(&mut Losses) -> R) -> R {
    let mut all = LOSSES.lock().unwrap_or_else(|e| e.into_inner());
    if all.len() < 64 {
        all.resize_with(64, Losses::default);
    }
    let l = &mut all[id];
    if now < l.time {
        // a new game in the same process
        l.hits.clear();
        l.incidents.clear();
        l.next_id = 0;
    }
    l.time = now;
    f(l)
}

const DOING: [&str; 14] = [
    "idle", "forage", "hunt", "fish", "farm", "food", "wood", "stone", "gold", "iron", "build", "fight", "move",
    "other",
];

/// `EECPNextDoing`: what a complex unit is doing
fn doing(g: &G<'_>, c: &UnitView<'_>) -> c_long {
    let Some((kind, _)) = c.active_goal() else { return 0 };
    match kind {
        GOAL_GATHER => {
            let Some(r) = g.goal_target(c, GOAL_GATHER, 0) else { return 13 };
            match r.resource_type() as c_long {
                2 => {
                    let k = r.class();
                    let t = k.as_ref().map_or(u64::MAX, |k| k.unit_type());
                    let f = k.as_ref().map_or(u64::MAX, |k| k.family());
                    if t == TYPE_FORAGE_BUSH {
                        1
                    } else if f == FAMILY_ANIMAL {
                        2
                    } else if f == FAMILY_FISH || t == TYPE_FISH {
                        3
                    } else if t == TYPE_FARM || t == TYPE_ROBOTIC_FARM {
                        4
                    } else {
                        5
                    }
                }
                3 => 6,
                4 => 7,
                5 => 8,
                6 => 9,
                _ => 13,
            }
        }
        GOAL_ATTACK_UNIT => {
            let animal = g
                .goal_target(c, GOAL_ATTACK_UNIT, 1)
                .and_then(|t| t.class())
                .is_some_and(|k| k.family() == FAMILY_ANIMAL);
            if animal {
                2
            } else {
                11
            }
        }
        GOAL_BUILD_REPAIR => 10,
        GOAL_LOCATION => 12,
        _ => 13,
    }
}

/// `EECPNextAround`: the foes round a tile, the own soldiers, the nearest own built tower and drop site
fn around(g: &G<'_>, x: c_long, y: c_long, range: c_long) -> (c_long, c_long, c_long, c_long, c_long, c_long) {
    let (mut foes, mut st, mut animals, mut own) = (0 as c_long, 0 as c_long, 0 as c_long, 0 as c_long);
    let (mut tower, mut drop) = (-1 as c_long, -1 as c_long);
    let r2 = (range as i64) * (range as i64);
    for p in 0..g.players().min(64) {
        let Some(o) = g.player(p) else { continue };
        let is_own = g.is_me(&o);
        if !is_own && p > 0 && g.is_ally(&o) {
            continue;
        }
        for u in o.units() {
            let Some(c) = u.complex_unit() else { continue };
            if hp(&u) <= 0 || u.stats().is_none() || c.container_handle() != 0 || xy(&u).0 == NO_TILE {
                continue;
            }
            let (ux, uy) = xy(&u);
            let dd = d2(ux, uy, x, y);
            let Some(k) = u.class() else { continue };
            if is_own {
                if is_building(&k) && c.is_built() {
                    let distance = isqrt(dd);
                    if is_tower(&k) && armed(&u) && (tower < 0 || distance < tower) {
                        tower = distance;
                    }
                    if is_drop_site(&k) && (drop < 0 || distance < drop) {
                        drop = distance;
                    }
                } else if is_soldier(&u) && dd <= r2 {
                    own += 1;
                }
                continue;
            }
            if p == 0 {
                if armed(&u) && k.family() == FAMILY_ANIMAL && dd <= 16 {
                    animals += 1;
                }
                continue;
            }
            if dd > r2 {
                continue;
            }
            if is_soldier(&u)
                || (is_building(&k) && is_tower(&k) && armed(&u))
                || (domains(&k) & MOVEMENT_SEA != 0
                    && armed(&u)
                    && !is_building(&k)
                    && k.category() != CAT_NAVAL_CITIZEN)
            {
                foes += 1;
                st = st.wrapping_add(strength(&u));
            }
        }
    }
    (foes, st, animals, own, tower, drop)
}

fn town_distance(me: *mut c_void, x: c_long, y: c_long) -> c_long {
    match town_tile(me) {
        Some((tx, ty)) => isqrt(d2(x, y, tx, ty)),
        None => -1,
    }
}

fn has_state(me: *mut c_void) -> bool {
    host(EE_AI_NEXT_HOST_STATE_OF, me, 0, 0, 0) != 0
}

/// `EECPNextTraceHit`: a hit on a land citizen (`attacker` the attacker's player id).
///
/// # Safety
/// `me` is a live computer player, `unit` a live unit or 0.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_trace_hit(me: *mut c_void, attacker: c_long, unit: u64) {
    let t = WorldRead::begin();
    let Some(g) = G::new(&t, me) else { return };
    let id = g.id();
    if !trace_on() || !has_state(me) || id >= 64 {
        return;
    }
    let Some(u) = g.unit(unit) else { return };
    let Some(c) = u.complex_unit() else { return };
    if u.class().map(|k| k.category()) != Some(CAT_LAND_CITIZEN) || xy(&u).0 == NO_TILE {
        return;
    }
    let now = now() as c_long;
    let (x, y) = xy(&u);
    let unit_id = u.id() as c_long;
    // (the reset first, then the scan of the world, as the C++)
    with_losses(id as usize, now, |_| ());
    let (foes, st, _, own, tower, _) = around(&g, x, y, 10);
    let what = doing(&g, &c);
    let distance = town_distance(me, x, y);
    with_losses(id as usize, now, |l| {
        let k = match l.incidents.iter().position(|i| now - i.last <= 30000 && near(i.x, i.y, x, y, 20)) {
            Some(k) => k,
            None => {
                l.next_id += 1;
                l.incidents.push(Incident {
                    id: l.next_id,
                    x,
                    y,
                    distance,
                    start: now,
                    attacker,
                    guard0: -1,
                    own0: own,
                    tower0: tower,
                    guard_at: -1,
                    ..Default::default()
                });
                l.incidents.len() - 1
            }
        };
        let inc = &mut l.incidents[k];
        inc.last = now;
        inc.hits += 1;
        inc.foes = inc.foes.max(foes);
        inc.foe_strength = inc.foe_strength.max(st);
        let inc_id = inc.id;
        let hit = l.hits.entry(unit_id).or_default();
        if hit.last == 0 || now - hit.last > 30000 {
            hit.first = now;
            hit.what = what;
        }
        hit.last = now;
        hit.incident = inc_id;
    });
}

/// `EECPNextTraceDeath`: a citizen or fishing boat lost.
///
/// # Safety
/// `me` is a live computer player, `unit` a live unit or 0.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_trace_death(me: *mut c_void, unit: u64) {
    let t = WorldRead::begin();
    let Some(g) = G::new(&t, me) else { return };
    let id = g.id();
    if !trace_on() || !has_state(me) || id >= 64 {
        return;
    }
    let Some(u) = g.unit(unit) else { return };
    let Some(c) = u.complex_unit() else { return };
    let category = u.class().map_or(u64::MAX, |k| k.category());
    if category != CAT_LAND_CITIZEN && category != CAT_NAVAL_CITIZEN {
        return;
    }
    let boat = category == CAT_NAVAL_CITIZEN;
    let now = now() as c_long;
    with_losses(id as usize, now, |_| ());
    let (x, y) = xy(&u);
    let killer = g.unit(host(EE_AI_NEXT_HOST_LAST_ATTACKER, me, c.handle() as i64, 0, 0) as u64);
    let (mut kp, mut kc, mut kd, mut kind) = (-1 as c_long, -1 as c_long, -1 as c_long, '-');
    if let Some((k, kclass)) = killer.as_ref().and_then(|k| Some((k, k.class()?))) {
        kp = k.player().map_or(-1, |p| p.id() as c_long);
        kc = kclass.unit_type() as c_long;
        let (kx, ky) = xy(k);
        if kx != NO_TILE {
            kd = isqrt(d2(kx, ky, x, y));
        }
        kind = if kp == 0 {
            'a'
        } else if is_building(&kclass) {
            if is_tower(&kclass) {
                't'
            } else {
                'b'
            }
        } else if kclass.category() == CAT_LAND_CITIZEN {
            'c'
        } else if is_soldier(k) {
            's'
        } else if domains(&kclass) & MOVEMENT_SEA != 0 {
            'n'
        } else {
            'o'
        };
    }
    let (foes, st, animals, own, tower, drop) = around(&g, x, y, 10);
    let now_doing = doing(&g, &c);
    let distance = town_distance(me, x, y);
    let epoch = g.tree().map_or(-1, |t| t.current_epoch() as c_long);
    let unit_id = u.id() as c_long;
    let (what, hit_ms, inc_id, inc_ms) = with_losses(id as usize, now, |l| {
        let (mut what, mut hit_ms, mut inc_id, mut inc_ms) = (now_doing, -1 as c_long, 0 as c_long, -1 as c_long);
        if let Some(h) = l.hits.remove(&unit_id) {
            what = h.what;
            hit_ms = now - h.first;
            inc_id = h.incident;
        }
        for i in l.incidents.iter_mut() {
            if i.id == inc_id {
                i.lost += 1;
                inc_ms = now - i.start;
            }
        }
        (what, hit_ms, inc_id, inc_ms)
    });
    let kname = if kc >= 0 { String::from_utf8_lossy(&class_name(kc)).into_owned() } else { "-".to_string() };
    trace(
        me,
        NEXT,
        "next.death",
        &format!(
            "unit={} x={x} y={y} d={distance} epoch={epoch} killer={kp}:{kname}:{kind} kd={kd} doing={} now={} \
             hit={hit_ms} inc={inc_id} inc_ms={inc_ms} foes={foes}/{st} animals={animals} own={own} guard=-1:-:0 \
             tower={tower} drop={drop}",
            if boat { "boat" } else { "citizen" },
            DOING.get(what as usize).unwrap_or(&"?"),
            DOING.get(now_doing as usize).unwrap_or(&"?"),
        ),
    );
}

/// `EECPNextTraceIncidents`: each step (trace on), an incident with no hit for 30 s is closed
fn trace_incidents(me: *mut c_void, id: c_ulong, now: c_long) {
    if !trace_on() || id >= 64 {
        return;
    }
    let lines = with_losses(id as usize, now, |l| {
        let mut lines = Vec::new();
        let mut k = l.incidents.len();
        while k > 0 {
            k -= 1;
            let i = l.incidents[k];
            if now - i.last <= 30000 {
                continue;
            }
            lines.push(format!(
                "id={} x={} y={} d={} start={} ms={} hits={} lost={} foes={}/{} attacker={} guard0={} own0={} \
                 tower0={} guard_ms={}",
                i.id,
                i.x,
                i.y,
                i.distance,
                i.start,
                i.last - i.start,
                i.hits,
                i.lost,
                i.foes,
                i.foe_strength,
                i.attacker,
                i.guard0,
                i.own0,
                i.tower0,
                if i.guard_at >= 0 { i.guard_at - i.start } else { -1 }
            ));
            l.incidents.remove(k);
        }
        l.hits.retain(|_, h| now - h.last <= 120000);
        lines
    });
    for line in lines {
        trace(me, None, "next.raid", &line);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The strategy (EECPNextLua.cpp's)

const STATUS_NO_FILE: u32 = 3;
const STATUS_BAD_VALUE: u32 = 8;

/// The script reader's text while it compiles (the Lua host's `owner`)
struct Reader {
    text: Vec<u8>,
}

/// TEST ONLY: EE_AI_SCRIPTS=<dir> is read before Data\ai (dir stands for Data\ai)
fn scripts_override() -> &'static Option<String> {
    static DIR: std::sync::OnceLock<Option<String>> = std::sync::OnceLock::new();
    DIR.get_or_init(|| match std::env::var("EE_AI_SCRIPTS") {
        Ok(mut d) if !d.is_empty() => {
            if !d.ends_with('\\') && !d.ends_with('/') {
                d.push('\\');
            }
            Some(d)
        }
        _ => None,
    })
}

fn read_file(path: &str) -> Option<Vec<u8>> {
    let p = cstr(path.as_bytes());
    let (r, v) = host_bytes(EE_AI_NEXT_HOST_READ, std::ptr::null_mut(), p.as_ptr() as i64);
    (r != 0).then_some(v)
}

/// `EECPNextReadScript`: `path` is "ai\<module path>.lua"
fn read_script(path: &str) -> Option<Vec<u8>> {
    if let (Some(dir), Some(rest)) = (scripts_override(), path.strip_prefix("ai\\")) {
        let mut p = format!("{dir}{rest}");
        if cfg!(not(windows)) {
            p = p.replace('\\', "/");
        }
        let c = cstr(p.as_bytes());
        if host(EE_AI_NEXT_HOST_EXISTS, std::ptr::null_mut(), c.as_ptr() as i64, 0, 0) != 0 {
            return read_file(&p);
        }
    }
    read_file(&format!("Data\\{path}"))
}

unsafe extern "C" fn tree_node(_: *const c_void, out: *mut EeAiLuaNode) {
    // (never called: the views and the memory are Rust values)
    std::ptr::write_bytes(out, 0, 1);
}
unsafe extern "C" fn tree_key(_: *const c_void, out: *mut EeAiLuaKey) {
    *out = EeAiLuaKey::of(lua_value::Key::Int(0));
}
unsafe extern "C" fn tree_set(_: *mut c_void, _: u8, _: u8, _: i64, _: *const u8, _: usize) {}
unsafe extern "C" fn tree_push(_: *mut c_void) -> *mut c_void {
    std::ptr::null_mut()
}
unsafe extern "C" fn tree_field(_: *mut c_void, _: *const EeAiLuaKey) -> *mut c_void {
    std::ptr::null_mut()
}
unsafe extern "C" fn tree_append(_: *mut c_void, _: *const u8, _: usize) {}
unsafe extern "C" fn tree_read(owner: *mut c_void, path: *const c_char, text: *mut *const u8, len: *mut usize) -> bool {
    let r = &mut *(owner as *mut Reader);
    r.text.clear();
    let path = CStr::from_ptr(path).to_string_lossy();
    match read_script(&path) {
        Some(t) => {
            r.text = t;
            *text = r.text.as_ptr();
            *len = r.text.len();
            true
        }
        None => false,
    }
}
unsafe extern "C" fn tree_read_done(owner: *mut c_void) {
    (*(owner as *mut Reader)).text = Vec::new();
}

static OPS: EeAiLuaTreeOps = EeAiLuaTreeOps {
    node: tree_node,
    key: tree_key,
    set: tree_set,
    push: tree_push,
    field: tree_field,
    append: tree_append,
    read: tree_read,
    read_done: tree_read_done,
};

/// The reader shared by the states (one at a time compiles: the simulation thread)
fn reader() -> *mut c_void {
    static READER: std::sync::atomic::AtomicPtr<Reader> = std::sync::atomic::AtomicPtr::new(std::ptr::null_mut());
    let p = READER.load(std::sync::atomic::Ordering::Relaxed);
    if !p.is_null() {
        return p as *mut c_void;
    }
    let p = Box::into_raw(Box::new(Reader { text: Vec::new() }));
    READER.store(p, std::sync::atomic::Ordering::Relaxed);
    p as *mut c_void
}

/// print(...) from a script: a next.print trace line
unsafe extern "C" fn print(context: *mut c_void, text: *const c_char) {
    if !trace_on() {
        return;
    }
    let t = if text.is_null() { &[][..] } else { CStr::from_ptr(text).to_bytes() };
    trace_line(context, None, "next.print", &next_value::one_line(t));
}

fn status_text(status: u32) -> String {
    lua_host::status_name(status).to_string_lossy().replace(' ', "_")
}

/// `EECPNextError`: a failed call: a next.error line in the AI trace and the game log; EE-SIM also writes it to
/// stderr. The seat issues no orders this step; the reflexes go on.
fn error(me: *mut c_void, s: Option<&mut NextState>, what: &str, status: u32, message: &[u8]) {
    let status = status_text(status);
    let message = next_value::one_line(message);
    let message =
        String::from_utf8_lossy(&message[..message.iter().position(|&c| c == 0).unwrap_or(message.len())]).into_owned();
    if let Some(s) = s {
        s.errors += 1;
    }
    let id = player_id(me);
    let t = now();
    if trace_on() {
        trace_line(me, NEXT, "next.error", format!("in={what} status={status} msg={message}").as_bytes());
    } else if host(EE_AI_NEXT_HOST_GAME_LOG_ON, me, 0, 0, 0) != 0 {
        let mut line = format!("EE-AI {t} p{id} next next.error in={what} status={status} msg={message}\n");
        truncate(&mut line, 699);
        let c = cstr(line.as_bytes());
        host(EE_AI_NEXT_HOST_GAME_LOG, me, c.as_ptr() as i64, 0, 0);
    }
    if host(EE_AI_NEXT_HOST_SIM_MODE, me, 0, 0, 0) != 0 {
        let c = cstr(format!("SIM next.error p{id} t={t} in={what} status={status} msg={message}\n").as_bytes());
        host(EE_AI_NEXT_HOST_STDERR, me, c.as_ptr() as i64, 0, 0);
    }
}

/// A `String` cut to at most `n` bytes (as snprintf's buffer cuts it; on a byte boundary, the text is bytes)
fn truncate(s: &mut String, n: usize) {
    if s.len() > n {
        let mut b = std::mem::take(s).into_bytes();
        b.truncate(n);
        *s = String::from_utf8_lossy(&b).into_owned();
    }
}

fn engine_log(text: &str) {
    let c = cstr(text.as_bytes());
    host(EE_AI_NEXT_HOST_ENGINE_LOG, std::ptr::null_mut(), c.as_ptr() as i64, 0, 0);
}

/// TEST ONLY: EE_AI_NEXT_SCRIPT, the script per player (a comma or / list by player id from player 1)
fn script_override(id: c_ulong) -> Vec<u8> {
    static LIST: std::sync::OnceLock<Vec<String>> = std::sync::OnceLock::new();
    let list = LIST.get_or_init(|| {
        let v = std::env::var("EE_AI_NEXT_SCRIPT").unwrap_or_default();
        if v.is_empty() {
            Vec::new()
        } else {
            v.split([',', '/']).map(str::to_string).collect()
        }
    });
    if list.is_empty() {
        return Vec::new();
    }
    let k = (if id > 0 { id - 1 } else { 0 }) as usize % list.len();
    list[k].as_bytes().to_vec()
}

/// `EECPNextChooseScript`
fn choose_script(id: c_ulong, s: &mut NextState) -> Vec<u8> {
    let mut script = script_override(id);
    if script.is_empty() {
        if s.ai.is_empty() {
            let (found, classic) = host_bytes(EE_AI_NEXT_HOST_CLASSIC_AI, std::ptr::null_mut(), 0);
            s.ai = if found != 0 { classic } else { b"none".to_vec() };
        }
        let c = cstr(&s.ai);
        let (found, v) = host_bytes(EE_AI_NEXT_HOST_AI_SCRIPT, std::ptr::null_mut(), c.as_ptr() as i64);
        if found != 0 {
            script = v;
        }
    }
    script
}

unsafe fn new_host(seed: u64, me: *mut c_void, play: bool) -> *mut Host {
    let config = Config {
        seed,
        memory_limit: 32 << 20,
        budget: 2_000_000,
        has_read: true,
        write: if play { Some(print) } else { None },
        write_context: if play { me } else { std::ptr::null_mut() },
        check_memory: true,
        open: if play { Some(crate::ai::units_bind::ee_ai_units_open) } else { None },
        open_context: if play { me } else { std::ptr::null_mut() },
    };
    lua_host::new(config, &OPS, reader())
}

fn last_error(h: *mut Host) -> (u32, Vec<u8>) {
    // SAFETY: a live host
    unsafe { (lua_host::status(h) as u32, lua_host::error(h).to_vec()) }
}

/// `EECPNextOpen`: the Lua state, made and loaded at the first step (a loaded game gets its memory back here)
///
/// The state is a raw pointer here and below: the script's top level and its steps call the bindings, which reach
/// the state again (the gather safety's tests); no reference to it lives across a Lua call.
unsafe fn open(me: *mut c_void, p: *mut NextState, id: c_ulong) -> bool {
    let seed = host(EE_AI_NEXT_HOST_GAME_SEED, me, 0, 0, 0) as u32;
    let lua = new_host(lua_value::seed(seed, id as u32), me, true);
    (*p).lua = lua;
    let module = cstr(&(*p).script);
    if !lua_host::load(lua, module.as_ptr(), false) {
        let (st, e) = last_error(lua);
        error(me, Some(&mut *p), "load", st, &e);
        return false;
    }
    let name = String::from_utf8_lossy(&(*p).script).into_owned();
    if (*p).restore {
        (*p).restore = false;
        let bytes = std::mem::take(&mut (*p).restore_bytes);
        if !lua_host::restore_memory(lua, &bytes) {
            let (st, e) = last_error(lua);
            error(me, Some(&mut *p), "restore", st, &e);
            return false;
        }
        (*p).started = true;
        trace(me, NEXT, "next.script", &format!("name={name} restored=1"));
    } else {
        trace(me, NEXT, "next.script", &format!("name={name}"));
    }
    true
}

/// `EECPNextPrepare`: the next engine's start: resolves the script and loads it -> false when it cannot (logged).
///
/// # Safety
/// `me` is a live computer player.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_prepare(me: *mut c_void) -> bool {
    let saved = scope(c"next");
    let ok = prepare(me, state_of(me));
    scope_end(saved);
    ok
}

unsafe fn prepare(me: *mut c_void, p: *mut NextState) -> bool {
    let id = player_id(me);
    if (*p).script.is_empty() {
        (*p).script = choose_script(id, &mut *p);
        if (*p).script.is_empty() {
            engine_log(&format!(
                "p{id}: the AI '{}' is not in Data/ai/ais.json: the seat stands idle",
                String::from_utf8_lossy(&(*p).ai)
            ));
            (*p).failed = true;
            return false;
        }
    }
    if !(*p).lua.is_null() || (*p).failed {
        return true;
    }
    if !open(me, p, id) {
        engine_log(&format!(
            "p{id}: the AI '{}' (script {}) did not load: the seat stands idle",
            String::from_utf8_lossy(&(*p).ai),
            String::from_utf8_lossy(&(*p).script)
        ));
        (*p).failed = true;
        return false;
    }
    true
}

fn value_text(v: &Value) -> Vec<u8> {
    let mut out = Vec::new();
    lua_value::to_text(v, &mut out);
    out
}

/// TEST ONLY (EE_AI_BINDING_TRACE): the step's view and orders
fn trace_step(id: c_ulong, what: &CStr, view: &Value, orders: &Value) {
    if !crate::ai::binding_trace::ee_ai_binding_trace_on() {
        return;
    }
    let (v, o) = (value_text(view), value_text(orders));
    // SAFETY: the texts live for the call
    unsafe {
        crate::ai::binding_trace::ee_ai_binding_trace_step(
            id as i64,
            what.as_ptr(),
            v.as_ptr(),
            v.len(),
            o.as_ptr(),
            o.len(),
        )
    };
}

fn run_orders(me: *mut c_void, s: &mut NextState, orders: &Value, what: &str) {
    match next_value::run_orders(orders) {
        Err(m) => error(me, Some(s), what, STATUS_BAD_VALUE, m.as_bytes()),
        Ok((lines, results)) => {
            for (event, text) in lines {
                trace(me, None, event, &text);
            }
            s.results.extend(results);
        }
    }
}

/// `EECPNextStrategyStep`
unsafe fn strategy_step(me: *mut c_void, p: *mut NextState, id: c_ulong, view: &View) {
    if (*p).script.is_empty() {
        (*p).script = choose_script(id, &mut *p);
        if (*p).script.is_empty() {
            (*p).failed = true;
            let m = format!("the AI '{}' is not in Data/ai/ais.json", String::from_utf8_lossy(&(*p).ai));
            error(me, Some(&mut *p), "load", STATUS_NO_FILE, m.as_bytes());
            return;
        }
    }
    if (*p).failed {
        return;
    }
    if (*p).lua.is_null() && !open(me, p, id) {
        (*p).failed = true;
        return;
    }
    let value = next_value::to_value(view, &(*p).results, &mut class_name);
    (*p).results.clear();
    let lua = (*p).lua;
    let mut orders = Value::Nil;
    let mut found = false;
    if !(*p).started {
        (*p).started = true;
        if !lua_host::entry(lua, c"init".as_ptr(), true, JobValueRef::rust(&value), 0, &mut orders, &mut found) {
            let (st, e) = last_error(lua);
            error(me, Some(&mut *p), "init", st, &e);
        } else {
            trace_step(id, c"init", &value, &orders);
            run_orders(me, &mut *p, &orders, "init");
        }
    }
    if !lua_host::entry(lua, c"step".as_ptr(), false, JobValueRef::rust(&value), 0, &mut orders, &mut found) {
        let (st, e) = last_error(lua);
        error(me, Some(&mut *p), "step", st, &e);
        return;
    }
    trace_step(id, c"step", &value, &orders);
    if !(*p).shadow.is_null() {
        shadow_step(me, p, &value, &orders);
    }
    run_orders(me, &mut *p, &orders, "step");
}

/// The per-unit interface was used (`EECPUnitsUsed`)
fn units_used(me: *mut c_void) -> bool {
    let u = host(EE_AI_NEXT_HOST_UNITS_STATE, me, 0, 0, 0) as usize as *mut c_void;
    // SAFETY: the player's live per-unit state
    !u.is_null() && unsafe { crate::ai::units_bind::ee_ai_units_used(u) }
}

/// TEST ONLY: the shadow's step on the live step's view: the orders and the memory after it must equal the live ones
unsafe fn shadow_step(me: *mut c_void, p: *mut NextState, view: &Value, orders: &Value) {
    let shadow = std::mem::replace(&mut (*p).shadow, std::ptr::null_mut());
    if units_used(me) {
        trace(me, None, "next.savecheck", "step skipped=units_api");
        lua_host::free(shadow);
        return;
    }
    let mut theirs = Value::Nil;
    let mut found = false;
    let (mut live, mut restored) = (Vec::new(), Vec::new());
    let ok = lua_host::entry(shadow, c"step".as_ptr(), false, JobValueRef::rust(view), 0, &mut theirs, &mut found)
        && lua_host::save_memory(shadow, &mut restored)
        && lua_host::save_memory((*p).lua, &mut live);
    let same = |b: bool| if b { "same" } else { "DIFFERENT" };
    trace(
        me,
        None,
        "next.savecheck",
        &format!(
            "step orders={} memory={} hash={:08x}/{:08x}",
            same(ok && lua_value::equal(&theirs, orders)),
            same(ok && live == restored),
            fnv32(&live),
            fnv32(&restored)
        ),
    );
    lua_host::free(shadow);
}

// ---------------------------------------------------------------------------------------------------------------
// The tick (EECPNextUpdate after the computer player's bookkeeping)

/// `EECPNextUpdate`'s body after the classic tick's bookkeeping (EEComputerPlayer's, in EECPNext.cpp): the epoch,
/// the reflexes' danger zones, the army's scan, one strategy step on a fresh view, the stats line.
///
/// # Safety
/// `me` is a live computer player with a placement and a tech tree; the simulation thread.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_update(me: *mut c_void, time: c_ulong) {
    let p = state_of(me);
    let t = WorldRead::begin();
    let Some(g) = G::new(&t, me) else { return };
    let id = g.id();
    (*p).note_epoch(g.tree().map_or(0, |t| t.current_epoch() as c_long), time);
    if host(EE_AI_NEXT_HOST_CAN_GATHER, me, 0, 0, 0) != 0 {
        host(EE_AI_NEXT_HOST_FLEE_RAIDS, me, 0, 0, 0);
    }
    let saved = scope(c"next");
    let s = &mut *p;
    // the army's step: the scouting memory, the losses' incidents (under "next")
    let now = now() as c_long;
    scan(&g, s, now);
    trace_incidents(me, id, now);
    // one strategy step on a fresh view
    let view = build_view(&g, s);
    if trace_on() {
        for e in &view.events {
            trace_line(
                me,
                None,
                "next.event",
                format!(
                    "kind={} cls={} n={}",
                    names::event_name(e.kind as i64, c"?").to_string_lossy(),
                    e.unit_type,
                    e.count
                )
                .as_bytes(),
            );
        }
    }
    s.events.clear();
    s.ring_step();
    if time % 30000 < 1000 && trace_on() {
        let v = &view;
        let r = |a: &[c_long; 5]| format!("{}/{}/{}/{}/{}", a[0], a[1], a[2], a[3], a[4]);
        trace_line(
            me,
            None,
            "next.view",
            format!(
                "epoch={} sol={} adv={} cit={} idle={} boats={} army={}/{} threat={} sites={} claim={} gat={} want={} \
                 inc={} res={}",
                v.epoch,
                v.sol_missing,
                v.advancing,
                v.citizens,
                v.idle,
                v.boats,
                v.army_count,
                v.army_strength,
                v.threat_strength,
                v.sites.len(),
                v.claim_range,
                r(&v.gatherers),
                r(&v.wanted),
                r(&v.income),
                r(&v.reserve)
            )
            .as_bytes(),
        );
    }
    if trace_on() && time % 60000 < 1000 {
        // the claimed sites: index:resource:units:room:workers:distance:drop:tower:danger
        let mut sites = String::new();
        let mut count = 0;
        for site in &view.sites {
            if sites.len() >= 160 {
                break;
            }
            if site.claimed == 0 || site.units == 0 {
                continue;
            }
            sites.push_str(&format!(
                " {}:{}:{}:{}:{}:{}:{}:{}:{}",
                site.index,
                site.resource,
                site.units,
                site.room,
                site.workers,
                site.distance,
                site.has_drop,
                site.has_tower,
                site.danger
            ));
            count += 1;
        }
        truncate(&mut sites, 199);
        trace_line(me, None, "next.sites", format!("claimed={count}{sites}").as_bytes());
    }
    drop(t);
    strategy_step(me, p, id, &view);
    host(EE_AI_NEXT_HOST_TRACE_STATS, me, time as i64, 0, 0);
    scope_end(saved);
}

// ---------------------------------------------------------------------------------------------------------------
// The save (EEPPlayerHelper, from version 0.252; the block's format is next_save.rs's)

fn save_check_on() -> bool {
    static ON: std::sync::OnceLock<bool> = std::sync::OnceLock::new();
    *ON.get_or_init(|| std::env::var("EE_AI_SAVE_CHECK").is_ok_and(|v| v.starts_with('1')))
}

/// TEST ONLY (EE_AI_SAVE_CHECK=1): what a load of these bytes would resume with; the shadow the next step runs
unsafe fn save_check(me: *mut c_void, p: *mut NextState, bytes: &[u8]) {
    if !save_check_on() {
        return;
    }
    let decoded = next_save::decode(bytes, next_value::MAX_ORDERS);
    let mut copy = NextState::default();
    let mut again = Vec::new();
    let ok = decoded.is_some();
    if let Some(d) = decoded {
        copy.from_save(d);
        again = next_save::encode(&copy.to_save(&copy.restore_bytes));
    }
    let mut restored = false;
    if !(*p).shadow.is_null() {
        lua_host::free((*p).shadow);
        (*p).shadow = std::ptr::null_mut();
    }
    if ok && copy.restore {
        let shadow = new_host(0, me, false);
        (*p).shadow = shadow;
        let module = cstr(&copy.script);
        restored =
            lua_host::load(shadow, module.as_ptr(), false) && lua_host::restore_memory(shadow, &copy.restore_bytes);
        if !restored {
            let (st, e) = last_error(shadow);
            error(me, Some(&mut *p), "savecheck", st, &e);
            lua_host::free(shadow);
            (*p).shadow = std::ptr::null_mut();
        }
    }
    trace(
        me,
        None,
        "next.savecheck",
        &format!("state={} restored={}", if ok && again == bytes { "same" } else { "DIFFERENT" }, restored as i32),
    );
}

/// `EECPNextSaveState`: the state's bytes appended to `out` (a C++ `std::string`; nothing without a state) ->
/// false when the script's memory could not be saved (logged; the rest of the block is written).
///
/// # Safety
/// `me` is a live computer player, `out` a live `std::string`.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_save(me: *mut c_void, out: *mut c_void) -> bool {
    let p = host(EE_AI_NEXT_HOST_STATE_OF, me, 0, 0, 0) as usize as *mut NextState;
    if p.is_null() {
        return true;
    }
    let mut ok = true;
    let memory = if (*p).restore {
        (*p).restore_bytes.clone()
    } else {
        let mut m = Vec::new();
        let lua = (*p).lua;
        if !lua.is_null() && (*p).started && !(*p).failed && !lua_host::save_memory(lua, &mut m) {
            let (st, e) = last_error(lua);
            error(me, Some(&mut *p), "save", st, &e);
            m.clear();
            ok = false;
        }
        m
    };
    let bytes = next_save::encode(&(*p).to_save(&memory));
    ee_ai_next_string_append(out, bytes.as_ptr(), bytes.len());
    trace(me, NEXT, "next.save", &format!("bytes={} memory={} hash={:08x}", bytes.len(), memory.len(), fnv32(&memory)));
    save_check(me, p, &bytes);
    ok
}

/// `EECPNextLoadState`: the state from its bytes (made when missing) -> false when they are not a block.
///
/// # Safety
/// `me` is a live computer player; `data` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_load(me: *mut c_void, data: *const u8, len: usize) -> bool {
    if len == 0 {
        return true;
    }
    let bytes = std::slice::from_raw_parts(data, len);
    let s = &mut *state_of(me);
    let Some(d) = next_save::decode(bytes, next_value::MAX_ORDERS) else { return false };
    if !s.lua.is_null() {
        lua_host::free(s.lua);
        s.lua = std::ptr::null_mut();
    }
    s.from_save(d);
    trace(
        me,
        NEXT,
        "next.load",
        &format!(
            "bytes={len} memory={} hash={:08x} script={}",
            s.restore_bytes.len(),
            fnv32(&s.restore_bytes),
            String::from_utf8_lossy(&s.script)
        ),
    );
    true
}

// ---------------------------------------------------------------------------------------------------------------
// The micro step (EECPUnitsMicroTick)

const MICRO_MS: c_long = 250;
const MICRO_BUDGET: i64 = 300000;
const MICRO_NONE: c_long = 0x7fffffff;

/// Every physics tick (EEArtificialIntelligence::Update): the enemy memory once a game second and the scripts'
/// `micro` every 250 game ms.
///
/// # Safety
/// The simulation thread, with the world.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_micro_tick(time: c_ulong) {
    let players = {
        let t = WorldRead::begin();
        WorldView::global(&t).map_or(0, |w| w.num_players())
    };
    for p in 1..players.min(64) {
        let cp = host(EE_AI_NEXT_HOST_MICRO_PLAYER, std::ptr::null_mut(), p as i64, 0, 0) as usize as *mut c_void;
        if cp.is_null() {
            continue;
        }
        let units = host(EE_AI_NEXT_HOST_UNITS_STATE, cp, 0, 0, 0) as usize as *mut c_void;
        crate::ai::units_bind::ee_ai_units_second(units, cp, time as c_long);
        let p = host(EE_AI_NEXT_HOST_STATE_OF, cp, 0, 0, 0) as usize as *mut NextState;
        let mut micro = crate::ai::units_bind::ee_ai_units_micro_next(units);
        let lua = (*p).lua;
        if lua.is_null() || !(*p).started || (*p).failed || micro == MICRO_NONE {
            continue;
        }
        if micro < 0 {
            micro = (time as c_long).wrapping_add(MICRO_MS);
        }
        if (time as c_long) < micro {
            crate::ai::units_bind::ee_ai_units_set_micro_next(units, micro);
            continue;
        }
        while micro <= time as c_long {
            micro += MICRO_MS;
        }
        crate::ai::units_bind::ee_ai_units_set_micro_next(units, micro);
        let arg = Value::Map(vec![(OwnedKey::Str(b"time".to_vec()), Value::Int(time as i64))]);
        let mut result = Value::Nil;
        let mut found = false;
        if !lua_host::entry(
            lua,
            c"micro".as_ptr(),
            true,
            JobValueRef::rust(&arg),
            MICRO_BUDGET,
            &mut result,
            &mut found,
        ) {
            let (st, e) = last_error(lua);
            let status = status_text(st);
            let msg =
                String::from_utf8_lossy(&e[..e.iter().position(|&c| c == 0).unwrap_or(e.len())]).replace('\n', "|");
            if trace_on() {
                trace_line(cp, NEXT, "next.error", format!("in=micro status={status} msg={msg}").as_bytes());
            }
            if host(EE_AI_NEXT_HOST_SIM_MODE, cp, 0, 0, 0) != 0 {
                let id = player_id(cp);
                let line = format!("SIM next.error p{id} t={} in=micro status={status} msg={msg}\n", now());
                let c = cstr(line.as_bytes());
                host(EE_AI_NEXT_HOST_STDERR, cp, c.as_ptr() as i64, 0, 0);
            }
        } else if !found {
            crate::ai::units_bind::ee_ai_units_set_micro_next(units, MICRO_NONE);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The gather safety's C ABI (the placement and the bindings' host call these)

/// `EECPNextSiteThreat`: the unit id of the threat at a tile (0: none) and its kind (`kind`).
///
/// # Safety
/// `me` is a live computer player; `kind` null or valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_site_threat(
    me: *mut c_void,
    x: c_long,
    y: c_long,
    now_only: bool,
    kind: *mut c_long,
) -> c_long {
    let s = &*state_of(me);
    let t = WorldRead::begin();
    let Some(g) = G::new(&t, me) else { return 0 };
    let (unit, k) = site_threat(&g, s, x, y, now_only);
    if !kind.is_null() {
        *kind = k;
    }
    unit
}

/// `EECPNextSiteBlocked`: why the gatherer finders pass a resource unit over (`cause`: the unit id that blocks it).
///
/// # Safety
/// `me` is a live computer player, `unit` a live unit; `cause` null or valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_site_blocked(me: *mut c_void, unit: u64, cause: *mut c_long) -> c_long {
    let s = &*state_of(me);
    let t = WorldRead::begin();
    let Some(g) = G::new(&t, me) else { return 0 };
    let Some(u) = g.unit(unit) else { return 0 };
    let (b, c) = site_blocked(&g, s, &u);
    if !cause.is_null() {
        *cause = c;
    }
    b
}

/// `EECPNextBuildUnsafe`.
///
/// # Safety
/// `me` is a live computer player.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_build_unsafe(me: *mut c_void, x: c_long, y: c_long) -> c_long {
    let s = &*state_of(me);
    let t = WorldRead::begin();
    let Some(g) = G::new(&t, me) else { return 0 };
    build_unsafe(&g, s, x, y)
}

/// `EECPNextAssigned`.
///
/// # Safety
/// `me` is a live computer player, `unit` a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_assigned(me: *mut c_void, unit: u64) -> c_long {
    let t = WorldRead::begin();
    let Some(g) = G::new(&t, me) else { return 0 };
    assigned(&g, unit)
}

/// `EECPNextHasRoom`.
///
/// # Safety
/// `me` is a live computer player, `unit` a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_has_room(me: *mut c_void, unit: u64) -> bool {
    let t = WorldRead::begin();
    let Some(g) = G::new(&t, me) else { return false };
    let Some(u) = g.unit(unit) else { return false };
    has_room(&g, &u)
}

/// `EECPNextDropReady` (`unit` 0: at the tile (x, y), for food when `food`, `slack` tiles further) or
/// `EECPNextUnitDropReady` (the unit's own tile and resource, `kNextDropSlack` further).
///
/// # Safety
/// `me` is a live computer player, `unit` 0 or a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_next_drop_ready(
    me: *mut c_void,
    unit: u64,
    x: c_long,
    y: c_long,
    food: bool,
    slack: c_long,
) -> bool {
    let t = WorldRead::begin();
    let Some(g) = G::new(&t, me) else { return false };
    if unit != 0 {
        return g.unit(unit).is_some_and(|u| unit_drop_ready(&g, &u));
    }
    drop_in(&ready_drops(&g, food), x, y, slack)
}
