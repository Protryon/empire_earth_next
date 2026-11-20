//! `EEComputerPlayer`'s members in the game (AI Thread cluster round 7; were EEComputerPlayer.cpp's bodies): the
//! events the player gets (units placed, completed, taken, swapped, stranded, done with a goal, damage, flares,
//! epochs, payments), the tick (`Update` with `EECPNextUpdate`'s bookkeeping), the start, `Clear`, the load's and
//! save's C++ parts with their after-load fixups, `RepairPointers` and the deferred `RestoreManagers`, the
//! experiment timers' and resource state's save blocks, the favourite screener and the hero count. The class keeps
//! its vtable: each member is one call here, and what only C++ holds (EEPlayer's own members, the goals, the
//! placement's file part, the tech tree's epochs) is one `ee_cp_player_host` case each (EE_CP_PLAYER_*). The CPEV
//! and PLCE scopes (TEST ONLY) open here in the original's places. The rules are [`super::player_ops`]. Absent from
//! a test binary.

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_char, c_void, CStr};
use std::os::raw::{c_long, c_ulong};
use std::sync::Mutex;

use super::fields::EeCpFields;
use super::lists::{self, EeCpDangerZone, EeCpHistoryEntry};
use super::place_capture::{self as pc, EeCpPlaceScope};
use super::place_game::{self as pg, EeCpPlaceCtx, EeCpPlaceIo};
use super::place_host::EeCpUnitInfo;
use super::player_ops::{self as ops, cpev, plce, FavoriteClass};
use super::save::{self, EeCpLoaded};
use super::timers::{EE_CP_STATE_PUSH_PLAYER, EE_CP_STATE_PUSH_ZONE, EE_CP_STATE_ZONE, EE_CP_STATE_ZONES};
use crate::app::capture_record;
use crate::units::player_save::{EeUnitsPlayerSaveFn, EeUnitsPlayerSaveIo};
use crate::world::layout::layout;
use crate::world::view::{ClassView, PlayerView, UnitView, WorldRead, WorldView};
use crate::world::player_fields::EeUnitsPlayerSaved;
use crate::world::unit_fields::EeUnitsComplexUnitFields;

// ---- the player host's operations (EEComputerPlayer.cpp ee_cp_player_host; `a`, `b` units, players or C++ objects)
/// `EEPlayer::AddResource(v0, v1)`
pub const EE_CP_PLAYER_BASE_ADD_RESOURCE: i32 = 340;
/// the player's `mNextState` (0: none); `EECPNextGetState(player)` (made when missing)
pub const EE_CP_PLAYER_NEXT_STATE: i32 = 341;
pub const EE_CP_PLAYER_NEXT_STATE_GET: i32 = 342;
/// `EECPUnitsRust(player)` (0: no units state)
pub const EE_CP_PLAYER_UNITS_STATE: i32 = 343;
/// `EEPlayer::CanProduce(v0, v1, v2, v3, v4, (UWideString*)a)`
pub const EE_CP_PLAYER_BASE_CAN_PRODUCE: i32 = 344;
/// `EEPlayer::UnitCompleted(a)`, `UnitPlaced(a)`, `UnitTaken(a, v0)`
pub const EE_CP_PLAYER_BASE_UNIT_COMPLETED: i32 = 345;
pub const EE_CP_PLAYER_BASE_UNIT_PLACED: i32 = 346;
pub const EE_CP_PLAYER_BASE_UNIT_TAKEN: i32 = 347;
/// `EEPlayer::DamageTaken(v0, v1, a)`
pub const EE_CP_PLAYER_BASE_DAMAGE_TAKEN: i32 = 348;
/// `EECPNavalFleeOn`, `EECPMilitiaOn`, `gCPTraceOn`, `EECPExperiment(player, (const char*)a)`
pub const EE_CP_PLAYER_NAVAL_FLEE_ON: i32 = 349;
pub const EE_CP_PLAYER_MILITIA_ON: i32 = 350;
pub const EE_CP_PLAYER_TRACE_ON: i32 = 351;
pub const EE_CP_PLAYER_EXPERIMENT: i32 = 352;
/// `IsAlly((EEPlayer*)a)`
pub const EE_CP_PLAYER_IS_ALLY: i32 = 353;
/// the epoch `v0`'s kind-7 media loaded (`v1` 1) or unloaded; `EEPlayer::EpochMediaLoaded(v0)`, `..Unloaded(v0)`
pub const EE_CP_PLAYER_EPOCH_MEDIA: i32 = 354;
pub const EE_CP_PLAYER_BASE_EPOCH_MEDIA_LOADED: i32 = 355;
pub const EE_CP_PLAYER_BASE_EPOCH_MEDIA_UNLOADED: i32 = 356;
/// the epoch `v0`'s `ResearchKind(7, true)` (an epoch node)
pub const EE_CP_PLAYER_EPOCH_RESEARCH: i32 = 357;
/// `mCivilizationFile`'s text; the scratch name set to `(const char*)a`, applied (`EEApplyCivilizationFile`), stored
pub const EE_CP_PLAYER_CIV_NAME: i32 = 358;
pub const EE_CP_PLAYER_CIV_SET_NAME: i32 = 359;
pub const EE_CP_PLAYER_CIV_APPLY: i32 = 360;
pub const EE_CP_PLAYER_CIV_STORE: i32 = 361;
/// `gWorld.mMap->mTerrainMesh.GetIsSpaceMap()`
pub const EE_CP_PLAYER_SPACE_MAP: i32 = 362;
/// `EEPlayer::UnitsSwapped(*(std::vector<EEPUnitSwap>*)a)`; `EEPlayer::Clear()`
pub const EE_CP_PLAYER_BASE_UNITS_SWAPPED: i32 = 363;
pub const EE_CP_PLAYER_BASE_CLEAR: i32 = 364;
/// `EEPlayer::RepairPointers(v0, v1)`; `gDeferComputerPlayerRestore`
pub const EE_CP_PLAYER_BASE_REPAIR: i32 = 365;
pub const EE_CP_PLAYER_DEFER_RESTORE: i32 = 366;
/// unit `a`'s class `CanMoveOnTerrain(a, v0)`; `AddStrandedUnit(a)`
pub const EE_CP_PLAYER_CAN_MOVE_ON: i32 = 367;
pub const EE_CP_PLAYER_ADD_STRANDED: i32 = 368;
/// unit `a`'s `QueueGoal(NewLocation(f0, f1, f2, false, true, false, 0, false, false), 0)`
pub const EE_CP_PLAYER_QUEUE_LOCATION: i32 = 369;
/// `DetachUnitFromAI(a)` (the virtual)
pub const EE_CP_PLAYER_DETACH: i32 = 370;
/// the tech tree's current epoch's `mCurrentSOL = 0`; `EEPlayer::EpochReached(v0)`
pub const EE_CP_PLAYER_RESET_SOL: i32 = 371;
pub const EE_CP_PLAYER_BASE_EPOCH_REACHED: i32 = 372;
/// `mEpochEnd - mEpochStart + 1`; `gPhysics->GetRandom(v0, v1)`
pub const EE_CP_PLAYER_EPOCH_SPAN: i32 = 373;
pub const EE_CP_PLAYER_RANDOM_RANGE: i32 = 374;
/// `EEPlayer::Load(v0, v1, *(FSFile*)a)`, `Save(..)`; the placement's `Load(*(FSFile*)a)`, `Save(..)`
pub const EE_CP_PLAYER_BASE_LOAD: i32 = 375;
pub const EE_CP_PLAYER_BASE_SAVE: i32 = 376;
pub const EE_CP_PLAYER_PLACEMENT_LOAD: i32 = 377;
pub const EE_CP_PLAYER_PLACEMENT_SAVE: i32 = 378;
/// `&EESaveFilePrimitive`
pub const EE_CP_PLAYER_SAVE_PRIMITIVE: i32 = 379;
/// `EEEpochCount()`
pub const EE_CP_PLAYER_EPOCH_COUNT: i32 = 380;
/// `EEPlayer::TakeResource(v0, v1)`; `Pay((EETechnology*)a, v0, f0)`; `PayImprovement((EEUnitClass*)a, v0)`
pub const EE_CP_PLAYER_BASE_TAKE_RESOURCE: i32 = 382;
pub const EE_CP_PLAYER_BASE_PAY: i32 = 383;
pub const EE_CP_PLAYER_BASE_PAY_IMPROVEMENT: i32 = 384;
/// node `a`'s `mData->mID` (-1: no data); `mData->mUnitClass`
pub const EE_CP_PLAYER_TECH_ID: i32 = 385;
pub const EE_CP_PLAYER_TECH_UNIT_CLASS: i32 = 386;
/// class `a`'s `IsBuilding()`, `IsTransport()`, `IsDropSite()`, `IsPriest()`, `FliesOver()`
pub const EE_CP_PLAYER_CLASS_IS_BUILDING: i32 = 387;
pub const EE_CP_PLAYER_CLASS_IS_TRANSPORT: i32 = 388;
pub const EE_CP_PLAYER_CLASS_IS_DROP_SITE: i32 = 389;
pub const EE_CP_PLAYER_CLASS_IS_PRIEST: i32 = 390;
pub const EE_CP_PLAYER_CLASS_FLIES_OVER: i32 = 391;
/// `EEPlayer::GetPopulationLimit(v0)`, `EEPlayer::CanAfford((EETechTreeNode*)a, v0, v1)`
pub const EE_CP_PLAYER_BASE_POP_LIMIT_OF: i32 = 392;
pub const EE_CP_PLAYER_BASE_CAN_AFFORD: i32 = 393;
/// `gWorld.mGameOptions.mResources`, `mIsScenarioGame`, `gGameOptions.mLoadType`
pub const EE_CP_PLAYER_GAME_RESOURCES: i32 = 394;
pub const EE_CP_PLAYER_SCENARIO_GAME: i32 = 395;
pub const EE_CP_PLAYER_LOAD_TYPE: i32 = 396;
/// `gWorld.mMap->ExploreUnseen(player)`; the start's trace line
pub const EE_CP_PLAYER_EXPLORE_UNSEEN: i32 = 397;
pub const EE_CP_PLAYER_TRACE_START: i32 = 398;
/// `gCPTraceManager` set to `(const char*)a` -> the one before
pub const EE_CP_PLAYER_TRACE_MANAGER: i32 = 399;
/// `new EECPPlacement(player)` into `mPlacement`; `delete mPlacement`; `mComputerPlayer = player`
pub const EE_CP_PLAYER_NEW_PLACEMENT: i32 = 400;
pub const EE_CP_PLAYER_DELETE_PLACEMENT: i32 = 401;
pub const EE_CP_PLAYER_SET_SELF: i32 = 402;
/// `gCPBehaviors`; `EEDifficultyAILevel(gWorld.mLobbyOptions.mDifficulty)`
pub const EE_CP_PLAYER_BEHAVIORS: i32 = 403;
pub const EE_CP_PLAYER_AI_LEVEL: i32 = 404;
/// `EECPNextDeleteState(player)`, `EECPUnitsDelete(player)`
pub const EE_CP_PLAYER_NEXT_DELETE: i32 = 405;
pub const EE_CP_PLAYER_UNITS_DELETE: i32 = 406;
/// unit `a`'s goal queue 0's size; `GetGoalState()`'s kind (-1: none); `IsRepairable()`
pub const EE_CP_PLAYER_GOAL_COUNT: i32 = 407;
pub const EE_CP_PLAYER_GOAL_STATE_KIND: i32 = 408;
pub const EE_CP_PLAYER_REPAIRABLE: i32 = 409;
/// unit `a`'s `ClearGoalQueue(0)`; `StartAction(v0)`; `mUnitClass->ActionChanged(a)`; `GetActiveGroup()->RemoveUnit(a)`
pub const EE_CP_PLAYER_CLEAR_GOALS: i32 = 410;
pub const EE_CP_PLAYER_START_ACTION: i32 = 411;
pub const EE_CP_PLAYER_ACTION_CHANGED: i32 = 412;
pub const EE_CP_PLAYER_GROUP_REMOVE: i32 = 413;
/// the load goal `b`'s `GetNumLoadUnits()`, `HasLoadUnit(a)`, `EvaluateCondition(a, v0)`, its container tile's x, y
pub const EE_CP_PLAYER_LOAD_UNITS: i32 = 414;
pub const EE_CP_PLAYER_HAS_LOAD_UNIT: i32 = 415;
pub const EE_CP_PLAYER_GOAL_CONDITION: i32 = 416;
pub const EE_CP_PLAYER_LOAD_TILE_X: i32 = 417;
pub const EE_CP_PLAYER_LOAD_TILE_Y: i32 = 418;
/// unit `a`'s `UnloadCargoAtTile(v0, v1, true)`
pub const EE_CP_PLAYER_UNLOAD_AT: i32 = 419;
/// a new unload goal of `a` to (v0, v1); the goal `b`'s `CanUnloadAtDestination()`; unit `a`'s `QueueGoal(b, 0)`
pub const EE_CP_PLAYER_NEW_UNLOAD: i32 = 422;
pub const EE_CP_PLAYER_CAN_UNLOAD: i32 = 423;
pub const EE_CP_PLAYER_QUEUE_GOAL: i32 = 424;
/// `gUnitSetManager.GetSet(v0)->IsEmpty()`, `->Contains(v1)`
pub const EE_CP_PLAYER_SET_EMPTY: i32 = 425;
pub const EE_CP_PLAYER_SET_HAS: i32 = 426;
/// node `v0`'s `GetIsObsolete()` (the virtual)
pub const EE_CP_PLAYER_NODE_OBSOLETE: i32 = 427;
/// `GetTotalPopulation(v0, v1)`
pub const EE_CP_PLAYER_TOTAL_POPULATION: i32 = 429;
/// `gPhysics->mGameTime`
pub const EE_CP_PLAYER_GAME_TIME: i32 = 428;

/// One `EEPUnitSwap` (`mOld`, `mNew`).
#[repr(C)]
pub struct EeCpUnitSwap {
    pub old: u64,
    pub new: u64,
}

/// The kind-of-host dispatch ([`pg`]'s): the player's operations from 300, the centres' from 100, the placement's.
fn call(ctx: &EeCpPlaceCtx, op: i32, a: u64, b: u64, io: &mut EeCpPlaceIo) -> i64 {
    // SAFETY: the C++ switches read the live objects the bodies hand them
    unsafe { pg::host_call(ctx, op, a, b, io) }
}

fn io() -> EeCpPlaceIo {
    EeCpPlaceIo {
        v: [0; 6],
        f: [0.0; 3],
        unit: EeCpUnitInfo::default(),
        input: std::ptr::null(),
        input_len: 0,
        list: std::ptr::null_mut(),
        order: std::ptr::null_mut(),
    }
}

/// The game of one call: the player's context and its operations.
struct Cp {
    ctx: EeCpPlaceCtx,
}

impl Cp {
    fn of(player: u64) -> Cp {
        // SAFETY: the C++ callers hand a live computer player
        Cp { ctx: unsafe { ctx_of(player) } }
    }
    fn q(&self, op: i32, a: u64, b: u64, v: &[i64]) -> i64 {
        let mut io = io();
        io.v[..v.len()].copy_from_slice(v);
        call(&self.ctx, op, a, b, &mut io)
    }
    fn qf(&self, op: i32, a: u64, v: &[i64], f: &[f32]) -> i64 {
        let mut io = io();
        io.v[..v.len()].copy_from_slice(v);
        io.f[..f.len()].copy_from_slice(f);
        call(&self.ctx, op, a, 0, &mut io)
    }
    fn on(&self, op: i32) -> bool {
        self.q(op, 0, 0, &[]) != 0
    }
    fn me(&self) -> u64 {
        self.ctx.player
    }
    // the record is the C++ object's, not this context's: a short-lived `&mut` per access
    #[allow(clippy::mut_from_ref)]
    fn fields(&self) -> &mut EeCpFields {
        // SAFETY: the player's record; never held across a call that could reach it again
        unsafe { &mut *self.ctx.fields }
    }
    fn saved(&self) -> *mut EeUnitsPlayerSaved {
        let t = unsafe { WorldRead::begin() };
        // SAFETY: a live player
        let p = unsafe { PlayerView::of(&t, self.me()) }.expect("ee_sim::cp: a computer player");
        p.saved().ptr() as *mut EeUnitsPlayerSaved
    }
    fn observer(&self) -> bool {
        let t = unsafe { WorldRead::begin() };
        // SAFETY: as above
        unsafe { PlayerView::of(&t, self.me()) }.is_some_and(|p| p.observer())
    }
    fn first_unit(&self) -> u64 {
        let t = unsafe { WorldRead::begin() };
        // SAFETY: as above
        unsafe { PlayerView::of(&t, self.me()) }.and_then(|p| p.first_unit()).map_or(0, |u| u.handle())
    }
    fn own_units(&self) -> Vec<u64> {
        let t = unsafe { WorldRead::begin() };
        // SAFETY: as above
        unsafe { PlayerView::of(&t, self.me()) }.map_or(Vec::new(), |p| p.units().map(|u| u.handle()).collect())
    }
    fn has_tree(&self) -> bool {
        let t = unsafe { WorldRead::begin() };
        // SAFETY: as above
        unsafe { PlayerView::of(&t, self.me()) }.is_some_and(|p| p.tech_tree().is_some())
    }
    fn current_epoch(&self) -> Option<i64> {
        let t = unsafe { WorldRead::begin() };
        // SAFETY: as above
        unsafe { PlayerView::of(&t, self.me()) }.and_then(|p| p.tech_tree()).map(|tree| tree.current_epoch())
    }
    fn now(&self) -> c_ulong {
        self.q(EE_CP_PLAYER_GAME_TIME, 0, 0, &[]) as c_ulong
    }
    fn loading(&self) -> bool {
        let t = unsafe { WorldRead::begin() };
        WorldView::global(&t).is_some_and(|w| w.loading_saved_game())
    }
    fn is_epoch(&self, id: i64) -> bool {
        self.q(pg::EE_CP_PLAYER_IS_EPOCH, 0, 0, &[id]) != 0
    }
    fn cheat_on(&self, cheat: i64) -> bool {
        self.q(pg::EE_CP_PLAYER_CHEAT_ON, 0, 0, &[cheat]) != 0
    }
    fn next_state(&self) -> *mut c_void {
        self.q(EE_CP_PLAYER_NEXT_STATE, 0, 0, &[]) as usize as *mut c_void
    }
    fn units_state(&self) -> *mut c_void {
        self.q(EE_CP_PLAYER_UNITS_STATE, 0, 0, &[]) as usize as *mut c_void
    }
    fn trace_on(&self) -> bool {
        self.on(EE_CP_PLAYER_TRACE_ON)
    }
    /// `EECPUnitsReflexOn(player, name)`
    fn reflex_on(&self, name: &CStr) -> bool {
        let s = self.units_state();
        if s.is_null() {
            return name != c"held";
        }
        // SAFETY: the player's live units state
        unsafe { crate::ai::units_bind::ee_ai_units_reflex_on(s, name.as_ptr()) }
    }
    /// `EECPUnitsHolds(player, unit)` (`unit` the complex unit; 0: none)
    fn holds(&self, unit: u64) -> bool {
        let s = self.units_state();
        if unit == 0 || s.is_null() {
            return false;
        }
        let id = unit_view(unit, |u| u.id()).unwrap_or(0);
        // SAFETY: as above
        unsafe { crate::ai::units_bind::ee_ai_units_holds(s, id as c_long) }
    }
    fn scope(&self, method: u32) -> Scope {
        Scope::open(self.me(), method, true)
    }
    fn place_scope(&self, method: u32) -> Scope {
        Scope::open(self.me(), method, false)
    }
    fn placement(&self) -> u64 {
        // SAFETY: a live computer player
        unsafe { std::ptr::read((self.me() as usize + layout().cp_placement as usize) as *const usize) as u64 }
    }
}

/// The context `EECPPlaceContext` builds, from the player's layout.
///
/// # Safety
/// `player` is a live `EEComputerPlayer`'s `EEPlayer*`.
pub unsafe fn ctx_of(player: u64) -> EeCpPlaceCtx {
    let l = layout();
    // SAFETY: the caller's contract; the layout's offsets are the build's
    unsafe {
        let placement = std::ptr::read((player as usize + l.cp_placement as usize) as *const usize);
        let (town, rust) = if placement == 0 {
            (std::ptr::null_mut(), std::ptr::null_mut())
        } else {
            (
                (placement + l.placement_town as usize) as *mut c_long,
                std::ptr::read((placement + l.placement_rust as usize) as *const *mut super::placement::EeCpPlacement),
            )
        };
        let t = WorldRead::begin();
        let player_id = PlayerView::of(&t, player).map_or(0, |p| p.id());
        EeCpPlaceCtx {
            player,
            player_id,
            fields: (player as usize + l.cp_fields as usize) as *mut EeCpFields,
            town,
            placement: rust,
            capture: u8::from(capture_record::active()),
        }
    }
}

/// A CPEV or PLCE scope (TEST ONLY), closed when dropped.
struct Scope {
    s: *mut EeCpPlaceScope,
    events: bool,
}

impl Scope {
    fn open(player: u64, method: u32, events: bool) -> Scope {
        // SAFETY: a live computer player
        Scope { s: unsafe { pc::ee_cp_scope_begin(player, method, events) }, events }
    }
    fn i(&self, w: i64) -> &Self {
        // SAFETY: this open scope
        unsafe { pc::ee_cp_scope_word(self.s, w as u32) };
        self
    }
    fn u(&self, unit: u64) -> &Self {
        // SAFETY: as above; a live unit
        unsafe { pc::ee_cp_scope_unit(self.s, unit) };
        self
    }
    fn state(&self, placement: u64) -> &Self {
        // SAFETY: as above; the player's placement
        unsafe { pc::ee_cp_scope_state(self.s, placement) };
        self
    }
}

impl Drop for Scope {
    fn drop(&mut self) {
        // SAFETY: the scope this opened
        unsafe { pc::ee_cp_scope_end(self.s, self.events) }
    }
}

fn unit_view<R>(unit: u64, f: impl FnOnce(UnitView<'_>) -> R) -> Option<R> {
    let t = unsafe { WorldRead::begin() };
    // SAFETY: the callers hand live units
    unsafe { UnitView::of(&t, unit) }.map(f)
}

fn class_of<R>(unit: u64, f: impl FnOnce(ClassView<'_>) -> R) -> Option<R> {
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    unsafe { UnitView::of(&t, unit) }.and_then(|u| u.class()).map(f)
}

fn complex_mut(unit: u64) -> Option<*mut EeUnitsComplexUnitFields> {
    unit_view(unit, |u| u.is_complex().then(|| u.complex().ptr() as *mut EeUnitsComplexUnitFields)).flatten()
}

fn land_building(unit: u64) -> bool {
    class_of(unit, |c| c.land_building()).unwrap_or(false)
}

// ---------------------------------------------------------------------------------------------------------------
// The construction and destruction

/// `Init` (both constructors): the record's tuning values, the lists, the placement, `mComputerPlayer`.
///
/// # Safety
/// `player` is the `EEComputerPlayer` being constructed (its `EEPlayer` base made, `mPlacement` null).
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_init(player: u64) {
    let cp = Cp::of(player);
    let behaviors = cp.q(EE_CP_PLAYER_BEHAVIORS, 0, 0, &[]) as usize as *const super::init::EeCpBehavior;
    let level = cp.q(EE_CP_PLAYER_AI_LEVEL, 0, 0, &[]) as c_long;
    let cheat = cp.cheat_on(ops::CHEAT_COST);
    // SAFETY: the player's record; the behaviour table
    unsafe {
        super::ffi::ee_cp_init(cp.ctx.fields, behaviors, level, cp.ctx.player_id as c_ulong, cheat);
        pg::ee_cp_lists_new(cp.ctx.fields);
    }
    cp.q(EE_CP_PLAYER_NEW_PLACEMENT, 0, 0, &[]);
    cp.q(EE_CP_PLAYER_SET_SELF, 0, 0, &[]);
}

/// The destructor: the units to kill dereferenced, the centres deleted, the lists dropped; the placement and the
/// engines' states deleted.
///
/// # Safety
/// `player` is the `EEComputerPlayer` being destroyed.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_destroy(player: u64) {
    let cp = Cp::of(player);
    // SAFETY: the player's lists, not used again
    unsafe { pg::ee_cp_lists_free(&cp.ctx) };
    cp.q(EE_CP_PLAYER_DELETE_PLACEMENT, 0, 0, &[]);
    cp.q(EE_CP_PLAYER_NEXT_DELETE, 0, 0, &[]);
    cp.q(EE_CP_PLAYER_UNITS_DELETE, 0, 0, &[]);
}

// ---------------------------------------------------------------------------------------------------------------
// The small members

/// `SetCombatState`: a new state clears the next enable time.
///
/// # Safety
/// `player` is a live computer player.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_set_combat_state(player: u64, state: c_ulong) {
    let cp = Cp::of(player);
    set_combat_state(&cp, state);
}

fn set_combat_state(cp: &Cp, state: c_ulong) {
    let saved = cp.saved();
    // SAFETY: the player's records
    unsafe {
        if state != (*saved).mCombatState {
            cp.fields().mNextEnableTime = 0;
        }
        (*saved).mCombatState = state;
    }
}

/// `SetCitizenPercentage`.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_set_citizen_percentage(player: u64, value: f32) {
    let cp = Cp::of(player);
    cp.fields().mCitizenPercentage = value;
    cp.fields().mIsCitizenPercentageSet = true;
}

/// `SetNextIslandExpansionTime`: the difficulty's island timer (minutes) from now.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_set_next_island_expansion_time(player: u64) {
    set_next_island_expansion_time(&Cp::of(player));
}

fn set_next_island_expansion_time(cp: &Cp) {
    let now = cp.now();
    // SAFETY: the player's record
    let timer = unsafe { super::ffi::ee_cp_island_expansion_timer(cp.ctx.fields) };
    cp.fields().mNextIslandExpansionTime = timer.wrapping_mul(60000).wrapping_add(now) as _;
}

unsafe extern "C" fn attack_random(ctx: *mut c_void, min: c_long, max: c_long) -> c_long {
    // SAFETY: the context SetNextAttackTime hands
    let cp = unsafe { &*(ctx as *const Cp) };
    cp.q(EE_CP_PLAYER_RANDOM_RANGE, 0, 0, &[min as i64, max as i64]) as c_long
}

/// `SetNextAttackTime`.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_set_next_attack_time(player: u64, time: c_ulong) {
    let cp = Cp::of(player);
    let epochs = cp.q(EE_CP_PLAYER_EPOCH_SPAN, 0, 0, &[]) as c_long;
    let deathmatch = is_deathmatch(&cp);
    // SAFETY: the player's record; the context lives for the call
    unsafe {
        super::ffi::ee_cp_set_next_attack_time(
            cp.ctx.fields,
            time,
            epochs,
            deathmatch,
            attack_random,
            &cp as *const Cp as *mut c_void,
        )
    }
}

fn is_deathmatch(cp: &Cp) -> bool {
    cp.q(EE_CP_PLAYER_GAME_RESOURCES, 0, 0, &[]) == 4 && !cp.on(EE_CP_PLAYER_SCENARIO_GAME)
}

/// `IsDeathmatch`: a deathmatch game (resources 4, not a scenario).
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_is_deathmatch(player: u64) -> bool {
    is_deathmatch(&Cp::of(player))
}

/// `DetachUnitFromAI`: the unit marked detached, its AI state 290008.
///
/// # Safety
/// `unit` is a live complex unit.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_detach_unit(unit: u64) {
    if let Some(c) = complex_mut(unit) {
        // SAFETY: the live unit's record
        unsafe {
            if !(*c).mIsDetachedFromAI {
                (*c).mIsDetachedFromAI = true;
            }
            if (*c).mAIState != 290008 {
                (*c).mAIState = 290008;
            }
        }
    }
}

/// `AddResource`: the player's, the trace's income line, the next engine's view.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_add_resource(player: u64, index: c_long, amount: c_long) {
    let cp = Cp::of(player);
    cp.q(EE_CP_PLAYER_BASE_ADD_RESOURCE, 0, 0, &[index as i64, amount as i64]);
    if cp.trace_on() {
        crate::ai::next_game::trace::ee_ai_trace_income(cp.ctx.player_id as c_ulong, index, amount);
    }
    let s = cp.next_state();
    if !s.is_null() {
        // SAFETY: the player's live next state
        unsafe { crate::ai::next_game::ee_ai_next_income(s, index - ops::RESOURCE_FOOD, amount) };
    }
}

/// `TakeResource`: positive amounts only.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_take_resource(player: u64, index: c_long, amount: c_long) {
    if amount > 0 {
        Cp::of(player).q(EE_CP_PLAYER_BASE_TAKE_RESOURCE, 0, 0, &[index as i64, amount as i64]);
    }
}

/// `WorkOutPopulation` with its CPEV scope.
fn work_out_population(cp: &Cp) {
    let _s = cp.scope(cpev::WORK_OUT_POPULATION);
    // SAFETY: the player's context
    unsafe { pg::ee_cp_work_out_population(&cp.ctx) };
}

/// `WorkOutPopulation`.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_work_out_population(player: u64) {
    work_out_population(&Cp::of(player));
}

fn regroup(cp: &Cp) {
    let _s = cp.scope(cpev::REGROUP);
    // SAFETY: as above
    unsafe { pg::ee_cp_regroup(&cp.ctx) };
}

fn update_groups(cp: &Cp) {
    let _s = cp.scope(cpev::UPDATE_GROUPS);
    // SAFETY: as above
    unsafe { pg::ee_cp_update_groups(&cp.ctx) };
}

/// `Regroup`, `UpdateGroups` (`which` 0, 1).
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_regroup(player: u64, which: u32) {
    let cp = Cp::of(player);
    if which == 0 {
        regroup(&cp)
    } else {
        update_groups(&cp)
    }
}

fn center_count(cp: &Cp) -> usize {
    // SAFETY: the player's record
    unsafe { pg::ee_cp_center_count(cp.ctx.fields) }
}

/// `RefreshResourceCenters`: a regroup when there is no centre, else the groups updated; a regroup again when none
/// is left (RefreshTownGroups is empty since stage 5).
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_refresh_resource_centers(player: u64) {
    let cp = Cp::of(player);
    if center_count(&cp) == 0 {
        regroup(&cp);
    } else {
        update_groups(&cp);
    }
    if center_count(&cp) == 0 {
        regroup(&cp);
    }
}

/// `GetPopulationLimit`: EEPlayer's when ignoring the others, else the cap (worked out first when unset and
/// inactive).
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_population_limit(player: u64, arg: c_long) -> c_long {
    let cp = Cp::of(player);
    if cp.fields().mIgnoreOthers {
        return cp.q(EE_CP_PLAYER_BASE_POP_LIMIT_OF, 0, 0, &[arg as i64]) as c_long;
    }
    if cp.fields().mPopulationCap == 0 && !cp.fields().mActive {
        work_out_population(&cp);
    }
    cp.fields().mPopulationCap as c_long
}

/// `EECPPendingEpoch` with its CPEV scope.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_pending_epoch(player: u64) -> c_long {
    let cp = Cp::of(player);
    let s = cp.scope(cpev::PENDING_EPOCH);
    // SAFETY: the player's context
    let r = unsafe { pg::ee_cp_pending_epoch(&cp.ctx) } as c_long;
    s.i(r as i64);
    r
}

/// `AvoidsEnemies` with its CPEV scope.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`]; `unit` is a live complex unit.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_avoids_enemies(player: u64, unit: u64) -> bool {
    let cp = Cp::of(player);
    let s = cp.scope(cpev::AVOIDS_ENEMIES);
    s.u(unit);
    // SAFETY: as above
    let r = unsafe { pg::ee_cp_avoids_enemies(&cp.ctx, unit) };
    s.i(r as i64);
    r
}

/// `CanAfford` (`EECPNextCanAfford`): an epoch is free with the epochs cheat, else EEPlayer's check.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`]; `node` is 0 or a live tech node.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_can_afford(player: u64, node: u64, unit_type: c_long, count: c_long) -> bool {
    let cp = Cp::of(player);
    let epoch = node != 0 && cp.is_epoch(cp.q(EE_CP_PLAYER_TECH_ID, node, 0, &[]));
    if epoch && cp.has_tree() && cp.cheat_on(ops::CHEAT_EPOCHS) {
        return true;
    }
    cp.q(EE_CP_PLAYER_BASE_CAN_AFFORD, node, 0, &[unit_type as i64, count as i64]) != 0
}

/// `CanProduce`: an epoch technology always; else EEPlayer's answer, refusing a missing node or one of kind 5.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`]; `reason` is null or a live `UWideString`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_can_produce(
    player: u64,
    tech: c_long,
    class: c_long,
    count: c_long,
    check_cost: bool,
    wonder_mode: c_long,
    reason: u64,
) -> bool {
    let cp = Cp::of(player);
    let s = cp.scope(cpev::CAN_PRODUCE);
    s.i(tech as i64).i(class as i64).i(count as i64).i(check_cost as i64).i(wonder_mode as i64);
    let mut r = true;
    if !(0..15).contains(&tech) {
        let v = [tech as i64, class as i64, count as i64, check_cost as i64, wonder_mode as i64];
        r = cp.q(EE_CP_PLAYER_BASE_CAN_PRODUCE, reason, 0, &v) != 0;
        if r {
            let mut io = io();
            io.v[0] = tech as i64;
            let found = call(&cp.ctx, pg::EE_CP_PLACE_TECH_KIND, 0, 0, &mut io) != 0;
            r = found && io.v[0] != 5;
        }
    }
    s.i(r as i64);
    r
}

// ---------------------------------------------------------------------------------------------------------------
// The units' events

fn on_unit(cp: &Cp, kind: c_long, unit: u64) {
    let s = cp.units_state();
    if !s.is_null() && unit != 0 {
        // SAFETY: the player's live units state; a live unit
        unsafe { crate::ai::units_bind::ee_ai_units_on_unit(s, kind, unit) };
    }
}

fn unit_added(cp: &Cp, unit: u64) {
    let s = cp.place_scope(plce::UNIT_ADDED);
    s.u(unit);
    // SAFETY: the player's context; a live unit
    unsafe { pg::ee_cp_unit_added(&cp.ctx, unit) };
    s.state(cp.placement());
}

/// `UnitPlaced`: the player's, the script's event, the placement while active.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`]; `unit` is a live complex unit.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_unit_placed(player: u64, unit: u64) {
    let cp = Cp::of(player);
    let s = cp.scope(cpev::UNIT_PLACED);
    s.u(unit);
    cp.q(EE_CP_PLAYER_BASE_UNIT_PLACED, unit, 0, &[]);
    on_unit(&cp, 3, unit); // created
    if cp.fields().mActive {
        unit_added(&cp, unit);
    }
}

/// `UnitCompleted`: the player's, the script's event, a land building offered to the centres.
///
/// # Safety
/// As [`ee_cp_pl_unit_placed`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_unit_completed(player: u64, unit: u64) {
    let cp = Cp::of(player);
    let s = cp.scope(cpev::UNIT_COMPLETED);
    s.u(unit);
    cp.q(EE_CP_PLAYER_BASE_UNIT_COMPLETED, unit, 0, &[]);
    on_unit(&cp, 4, unit); // built
    if land_building(unit) {
        // SAFETY: the player's context
        unsafe { pg::ee_cp_centers_adopt(&cp.ctx, unit) };
    }
}

/// `UnitTaken`: the player's, the script's, the traces, the next engine's losses, the centres' release.
///
/// # Safety
/// As [`ee_cp_pl_unit_placed`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_unit_taken(player: u64, unit: u64, swapped: bool) {
    let cp = Cp::of(player);
    let s = cp.scope(cpev::UNIT_TAKEN);
    s.u(unit).i(swapped as i64);
    cp.q(EE_CP_PLAYER_BASE_UNIT_TAKEN, unit, 0, &[swapped as i64]);
    let units = cp.units_state();
    if !units.is_null() && unit != 0 && !swapped {
        // SAFETY: the player's live units state; a live unit
        unsafe { crate::ai::units_bind::ee_ai_units_on_taken(units, player as usize as *mut c_void, unit) };
    }
    let trace = cp.trace_on();
    if trace && !swapped {
        // SAFETY: as above
        unsafe { crate::ai::next_game::trace::ee_ai_trace_unit_lost(player as usize as *mut c_void, unit) };
    }
    let next = cp.next_state();
    if trace && !swapped && !next.is_null() {
        // SAFETY: as above
        unsafe { crate::ai::next_game::ee_ai_next_trace_death(player as usize as *mut c_void, unit) };
    }
    if !next.is_null() && !swapped {
        if let Some(unit_type) = class_of(unit, |c| c.unit_type()) {
            // SAFETY: the player's live next state
            unsafe { crate::ai::next_game::ee_ai_next_event_add(next, ops::NEXT_EVENT_UNIT_LOST, unit_type as c_long, 1) };
        }
    }
    if land_building(unit) {
        // SAFETY: the player's context
        unsafe { pg::ee_cp_centers_release(&cp.ctx, unit) };
    }
}

/// `UnitsSwapped`: the script's held set and ids follow the new units; a replacing drop site is offered to the
/// centres; then the player's.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`]; `swaps` holds `count` swaps of the vector `vector`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_units_swapped(player: u64, swaps: *const EeCpUnitSwap, count: usize, vector: u64) {
    let cp = Cp::of(player);
    let s = cp.scope(cpev::UNITS_SWAPPED);
    s.i(count as i64);
    // SAFETY: the caller's contract
    let swaps = if count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(swaps, count) } };
    let units = cp.units_state();
    if !units.is_null() {
        let now = cp.now() as c_long;
        let t = unsafe { WorldRead::begin() };
        for w in swaps {
            if w.old == 0 || w.new == 0 {
                continue;
            }
            // SAFETY: the swaps' live units
            let (Some(old), Some(new)) = (unsafe { UnitView::of(&t, w.old) }, unsafe { UnitView::of(&t, w.new) }) else {
                continue;
            };
            let (old_owner, new_owner) =
                (old.player().map_or(0, |p| p.handle()), new.player().map_or(0, |p| p.handle()));
            if old_owner != player && new_owner != player {
                continue;
            }
            let unit_type = new.class().map_or(0, |c| c.unit_type());
            // SAFETY: the player's live units state
            unsafe {
                crate::ai::units_bind::ee_ai_units_on_swap(
                    units,
                    old.id() as c_long,
                    new.id() as c_long,
                    new_owner == player,
                    unit_type as c_long,
                    now,
                )
            };
        }
        // SAFETY: as above
        unsafe { crate::ai::units_bind::ee_ai_units_forward_expire(units, now) };
    }
    for w in swaps {
        let class = class_of(w.new, |c| c.handle()).unwrap_or(0);
        if cp.q(EE_CP_PLAYER_CLASS_IS_DROP_SITE, class, 0, &[]) != 0 {
            // SAFETY: the player's context
            unsafe { pg::ee_cp_centers_adopt(&cp.ctx, w.new) };
        }
    }
    cp.q(EE_CP_PLAYER_BASE_UNITS_SWAPPED, vector, 0, &[]);
}

/// `UnitStranded`: the unit referenced and queued to be killed at the next update.
///
/// # Safety
/// As [`ee_cp_pl_unit_placed`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_unit_stranded(player: u64, unit: u64) {
    let cp = Cp::of(player);
    let s = cp.scope(cpev::UNIT_STRANDED);
    s.u(unit);
    cp.q(pg::EE_CP_CENTER_UNIT_REFERENCE, unit, 0, &[]);
    let id = unit_view(unit, |u| u.id()).unwrap_or(0xffff_ffff);
    // SAFETY: the player's record
    unsafe { lists::ee_cp_kill_push(cp.ctx.fields, id) };
}

/// `FlareReceived`: the seat's Lua AI gets the flare (not in observer mode).
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`]; `from` is 0 or a live player.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_flare(player: u64, from: u64, x: c_long, y: c_long) {
    let cp = Cp::of(player);
    if cp.observer() {
        return;
    }
    let s = cp.units_state();
    if !s.is_null() {
        let t = unsafe { WorldRead::begin() };
        // SAFETY: as above
        let id = unsafe { PlayerView::of(&t, from) }.map_or(0, |p| p.id()) as c_long;
        // SAFETY: the player's live units state
        unsafe { crate::ai::units_bind::ee_ai_units_on_player(s, 10, id, x, y) };
    }
}

/// `DamageTaken`: the player's; the time; the script's event; the building and citizen flags; the naval flee and
/// the danger zones' reflexes.
///
/// # Safety
/// As [`ee_cp_pl_unit_placed`] (`unit` a live unit).
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_damage_taken(player: u64, attacker: c_long, amount: c_long, unit: u64) {
    let cp = Cp::of(player);
    let s = cp.scope(cpev::DAMAGE_TAKEN);
    s.i(attacker as i64).i(amount as i64).u(unit);
    let category = class_of(unit, |c| c.category()).unwrap_or(0) as c_ulong;
    cp.q(EE_CP_PLAYER_BASE_DAMAGE_TAKEN, unit, 0, &[attacker as i64, amount as i64]);
    cp.fields().mLastDamagedTime = cp.now() as _;
    let units = cp.units_state();
    if !units.is_null() && unit != 0 {
        // SAFETY: the player's live units state; a live unit
        unsafe { crate::ai::units_bind::ee_ai_units_on_damage(units, player as usize as *mut c_void, attacker, amount, unit) };
    }
    let building = land_building(unit);
    if building {
        cp.fields().mBuildingDamaged = true;
    }
    // the attacker's player when it is another, not allied one
    let enemy = || {
        let t = unsafe { WorldRead::begin() };
        let w = WorldView::global(&t)?;
        if attacker <= 0 || attacker as u64 >= w.num_players() {
            return None;
        }
        let p = w.player(attacker as usize)?.handle();
        (cp.q(EE_CP_PLAYER_IS_ALLY, p, 0, &[]) == 0).then_some(p)
    };
    let complex = unit_view(unit, |u| u.complex_unit_handle()).unwrap_or(0);
    if attacker > 0
        && cp.on(EE_CP_PLAYER_NAVAL_FLEE_ON)
        && !building
        && cp.reflex_on(c"naval_flee")
        && complex != 0
        && enemy().is_some()
    {
        let last = unit_view(complex, |u| u.last_attacker_handle()).unwrap_or(0);
        // SAFETY: the live units
        unsafe { crate::ai::next_game::reflex::ee_ai_next_naval_flee(player as usize as *mut c_void, unit, last) };
    }
    if category == ops::CATEGORY_NAVAL_CITIZEN || category == ops::CATEGORY_LAND_CITIZEN {
        cp.fields().mCitizenDamaged = true;
        if !cp.next_state().is_null() && cp.trace_on() {
            // SAFETY: as above
            unsafe { crate::ai::next_game::ee_ai_next_trace_hit(player as usize as *mut c_void, attacker, unit) };
        }
        if attacker > 0
            && category == ops::CATEGORY_LAND_CITIZEN
            && enemy().is_some()
            && (cp.q(pg::EE_CP_PLAYER_HUMAN, 0, 0, &[attacker as i64]) == 1
                || cp.q(EE_CP_PLAYER_EXPERIMENT, c"flee".as_ptr() as u64, 0, &[]) != 0
                || cp.on(EE_CP_PLAYER_MILITIA_ON))
            && cp.reflex_on(c"danger")
            && (!cp.holds(complex) || cp.reflex_on(c"held"))
        {
            // SAFETY: as above
            unsafe { crate::ai::next_game::reflex::ee_ai_next_note_danger(player as usize as *mut c_void, unit, attacker as c_ulong) };
        }
    }
}

/// `SetActive`: turning on hands the player's units to the placement.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_set_active(player: u64, active: bool) {
    let cp = Cp::of(player);
    let s = cp.scope(cpev::SET_ACTIVE);
    s.i(active as i64);
    if active && !cp.fields().mActive {
        for unit in cp.own_units() {
            let complex = unit_view(unit, |u| u.complex_unit_handle()).unwrap_or(0);
            if complex != 0 {
                unit_added(&cp, complex);
            }
        }
    }
    cp.fields().mActive = active;
}

/// `PayImprovement`: EEPlayer's (the classic's ledger is gone since stage 6).
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`]; `class` is 0 or a live class.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_pay_improvement(player: u64, class: u64, improvement: c_long) {
    let cp = Cp::of(player);
    let s = cp.scope(cpev::PAY_IMPROVEMENT);
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let unit_type = unsafe { ClassView::of(&t, class) }.map_or(-1, |c| c.unit_type() as c_long);
    s.i(unit_type as i64).i(improvement as i64);
    cp.q(EE_CP_PLAYER_BASE_PAY_IMPROVEMENT, class, 0, &[improvement as i64]);
}

/// `Pay`: epoch advances are free with the epochs cheat; with the trace, what the payment bought (TEST ONLY).
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`]; `tech` is a live technology.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_pay(player: u64, tech: u64, unit_type: c_long, fraction: f32) {
    let cp = Cp::of(player);
    let epoch = cp.is_epoch(cp.q(EE_CP_PLAYER_TECH_ID, tech, 0, &[]));
    if epoch && cp.cheat_on(ops::CHEAT_EPOCHS) {
        return;
    }
    if !cp.trace_on() {
        cp.qf(EE_CP_PLAYER_BASE_PAY, tech, &[unit_type as i64], &[fraction]);
        return;
    }
    let caps = || {
        // SAFETY: the player's record
        let c = unsafe { (*cp.saved()).mResourceCaps };
        [c[2], c[3], c[4], c[5], c[6]]
    };
    let before = caps();
    cp.qf(EE_CP_PLAYER_BASE_PAY, tech, &[unit_type as i64], &[fraction]);
    let after = caps();
    let cost: [c_long; 5] = std::array::from_fn(|r| before[r].wrapping_sub(after[r]));
    let made = cp.q(EE_CP_PLAYER_TECH_UNIT_CLASS, tech, 0, &[]);
    let t = unsafe { WorldRead::begin() };
    let class = if made > 0 { ClassView::by_index(&t, made as usize) } else { None };
    let facts = class.map(|c| {
        let military = crate::world_field!(c.fields(), mIsAttackUnit) || crate::world_field!(c.fields(), mIsDefenseUnit);
        (c.category() as c_ulong, cp.q(EE_CP_PLAYER_CLASS_IS_BUILDING, c.handle(), 0, &[]) != 0, military)
    });
    let kind = ops::pay_kind(epoch, facts);
    let id = cp.ctx.player_id as c_ulong;
    if id < 64
        && (0..=7).contains(&kind)
        && cp.q(EE_CP_PLAYER_EXPERIMENT, c"spending".as_ptr() as u64, 0, &[]) != 0
    {
        // SAFETY: five costs
        unsafe { crate::ai::next_game::trace::ee_ai_trace_spend(id, kind, cost.as_ptr()) };
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The goals

/// An idle transport not a script's (the resource manager's TransportIdle): the citizens it was loading stop, and it
/// unloads at its rendezvous; an unloading one that was detached tries its destination again.
fn transport_idle(cp: &Cp, unit: u64) {
    let goal = unit_view(unit, |u| (u.active_goal_handle(), u.active_goal().map(|g| g.0))).unwrap_or((0, None));
    let cargo = || unit_view(unit, |u| u.cargo_len()).unwrap_or(0);
    match goal {
        (g, Some(ops::GOAL_LOAD_TRANSPORT)) => {
            if cp.q(EE_CP_PLAYER_LOAD_UNITS, 0, g, &[]) <= 0 {
                return;
            }
            let mut changed = false;
            for citizen in cp.own_units() {
                let Some((complex, unit_type)) =
                    unit_view(citizen, |u| (u.complex_unit_handle(), u.class().map_or(0, |c| c.unit_type())))
                else {
                    continue;
                };
                if complex == 0 || (unit_type != 0x20 && unit_type != 0x1e0) {
                    continue;
                }
                if cp.q(EE_CP_PLAYER_HAS_LOAD_UNIT, complex, g, &[]) == 0 {
                    continue;
                }
                cp.q(EE_CP_PLAYER_CLEAR_GOALS, complex, 0, &[]);
                cp.q(EE_CP_PLAYER_START_ACTION, complex, 0, &[ops::ACTION_IDLE]);
                cp.q(EE_CP_PLAYER_ACTION_CHANGED, complex, 0, &[]);
                changed = true;
            }
            if changed {
                if cargo() > 0 && cp.q(EE_CP_PLAYER_GOAL_CONDITION, unit, g, &[ops::TRIGGER_AT_RENDEZVOUS]) != 0 {
                    let x = cp.q(EE_CP_PLAYER_LOAD_TILE_X, 0, g, &[]);
                    let y = cp.q(EE_CP_PLAYER_LOAD_TILE_Y, 0, g, &[]);
                    cp.q(EE_CP_PLAYER_UNLOAD_AT, unit, 0, &[x, y]);
                }
                cp.q(EE_CP_PLAYER_CLEAR_GOALS, unit, 0, &[]);
            }
        }
        (g, Some(ops::GOAL_UNLOAD_TRANSPORT)) => {
            let detached = unit_view(unit, |u| crate::world_field!(u.complex(), mIsDetachedFromAI)).unwrap_or(false);
            if cargo() > 0 && detached {
                // GetUnloadTransport(&x, &y): the goal's own fields
                // SAFETY: a plain record the call fills
                let mut gio: crate::units::rust_goal::EeUnitsGoalIo = unsafe { std::mem::zeroed() };
                // SAFETY: the unit's live unload goal
                unsafe {
                    crate::units::goal_obj::ee_units_goal_call(
                        crate::units::rust_goal::EE_UNITS_GOAL_UNLOAD_FIELDS,
                        g as usize as *const crate::units::goal_obj::EeUnitsGoalBase,
                        0,
                        0,
                        &mut gio,
                    )
                };
                let (x, y) = (gio.i[0] as c_long as i64, gio.i[1] as c_long as i64);
                let new = cp.q(EE_CP_PLAYER_NEW_UNLOAD, unit, 0, &[x, y]) as u64;
                if cp.q(EE_CP_PLAYER_CAN_UNLOAD, 0, new, &[]) != 0 {
                    cp.q(EE_CP_PLAYER_QUEUE_GOAL, unit, new, &[]);
                }
            }
        }
        _ => {}
    }
}

/// `SendToNearestBase`: an empty naval, helicopter or space transport to the berth of its nearest dock, airport or
/// space dock, detached from the AI.
fn send_to_nearest_base(cp: &Cp, unit: u64) {
    let (category, class, tile) = unit_view(unit, |u| {
        let (x, y) = u.tile_xy();
        (u.class().map_or(0, |c| c.category()) as c_ulong, u.class().map_or(0, |c| c.handle()), (x as c_long, y as c_long))
    })
    .unwrap_or((0, 0, (0, 0)));
    let base = match category {
        ops::CATEGORY_NAVAL_TRANSPORT => ops::TYPE_DOCK,
        ops::CATEGORY_HELICOPTER_TRANSPORT => ops::TYPE_AIRPORT,
        ops::CATEGORY_SPACE_TRANSPORT => ops::TYPE_SPACE_DOCK,
        _ => return,
    };
    let mut units = Vec::new();
    crate::units::uu_ffi::player_units_of_type(base, cp.ctx.player_id as i32, &mut units);
    if units.is_empty() {
        return;
    }
    let tiles: Vec<(u64, (c_long, c_long))> = units
        .iter()
        .map(|&u| (u, unit_view(u, |v| v.tile_xy()).map_or((0, 0), |(x, y)| (x as c_long, y as c_long))))
        .collect();
    let mut sorted = tiles;
    crate::units::msvc_sort::sort_by(&mut sorted, |a, b| ops::tile_distance(tile, a.1) < ops::tile_distance(tile, b.1));
    let point = [0.0f32; 3];
    let mut m = [[0.0f32; 4]; 4];
    for (i, row) in m.iter_mut().enumerate() {
        row[i] = 1.0;
    }
    // SAFETY: the live class and dock
    let found = unsafe { crate::units::uu_ffi::ee_units_uu_dock_ship_placement(&point, class, sorted[0].0, 0, 1, &mut m) };
    if found != 0 {
        cp.qf(EE_CP_PLAYER_QUEUE_LOCATION, unit, &[], &[m[0][3], m[1][3], m[2][3]]);
        cp.q(EE_CP_PLAYER_DETACH, unit, 0, &[]);
    }
}

/// `UnitGoalDone`: transports idled (empty damaged ones sent home), the script's event, a detached unit given back,
/// nearly idle units dropped from their group.
///
/// # Safety
/// As [`ee_cp_pl_unit_placed`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_unit_goal_done(player: u64, unit: u64) {
    let cp = Cp::of(player);
    let s = cp.scope(cpev::UNIT_GOAL_DONE);
    s.u(unit);
    let goals = || cp.q(EE_CP_PLAYER_GOAL_COUNT, unit, 0, &[]);
    let cargo = || unit_view(unit, |u| u.cargo_len()).unwrap_or(0);
    let mut count = goals();
    let class = class_of(unit, |c| c.handle()).unwrap_or(0);
    if cp.q(EE_CP_PLAYER_CLASS_IS_TRANSPORT, class, 0, &[]) != 0 && !cp.holds(unit) {
        transport_idle(&cp, unit);
        if count == 1 && cp.q(EE_CP_PLAYER_REPAIRABLE, unit, 0, &[]) != 0 && cargo() == 0 {
            send_to_nearest_base(&cp, unit);
        }
        if cp.q(EE_CP_PLAYER_GOAL_STATE_KIND, unit, 0, &[]) == ops::GOAL_LOAD_TRANSPORT as i64 && cargo() == 0 {
            cp.q(EE_CP_PLAYER_CLEAR_GOALS, unit, 0, &[]);
        }
    }
    count = goals();
    if count <= 1 {
        on_unit(&cp, 5, unit); // idle
    }
    let in_group = || unit_view(unit, |u| u.active_group_handle()).unwrap_or(0) != 0;
    if cp.holds(unit) {
        if count <= 1 && in_group() {
            cp.q(EE_CP_PLAYER_GROUP_REMOVE, unit, 0, &[]);
        }
        return;
    }
    if count <= 1 {
        if let Some(c) = complex_mut(unit) {
            // SAFETY: the live unit's record
            unsafe {
                if (*c).mIsDetachedFromAI {
                    (*c).mIsDetachedFromAI = false;
                }
            }
        }
    }
    if count <= 1 && in_group() {
        cp.q(EE_CP_PLAYER_GROUP_REMOVE, unit, 0, &[]);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The epochs and the civilization

fn enter_epoch(cp: &Cp, epoch: c_long) {
    let s = cp.scope(cpev::ENTER_EPOCH);
    s.i(epoch as i64);
    if cp.has_tree() {
        cp.q(EE_CP_PLAYER_EPOCH_RESEARCH, 0, 0, &[epoch as i64]);
    }
    for kind in [super::events::EE_CP_FAVORITE_LAND, super::events::EE_CP_FAVORITE_SEA, super::events::EE_CP_FAVORITE_AIR, super::events::EE_CP_FAVORITE_SPACE] {
        // SAFETY: the player's context
        unsafe { pg::ee_cp_choose_favorite(&cp.ctx, kind) };
    }
    if !cp.loading() {
        // SAFETY: as above
        unsafe { pg::ee_cp_retire_obsolete_units(&cp.ctx) };
    }
}

/// `EnterEpoch`: the epoch's kind-7 technologies, the four favourites again and, outside a load, the retired units.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_enter_epoch(player: u64, epoch: c_long) {
    enter_epoch(&Cp::of(player), epoch);
}

/// `EpochReached`: the reached epoch's SOL count starts over, a real epoch entered, then the player's.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_epoch_reached(player: u64, epoch: c_long) {
    let cp = Cp::of(player);
    let s = cp.scope(cpev::EPOCH_REACHED);
    s.i(epoch as i64);
    let real = cp.is_epoch(epoch as i64);
    if real && !cp.loading() && cp.has_tree() {
        cp.q(EE_CP_PLAYER_RESET_SOL, 0, 0, &[]);
    }
    if real {
        enter_epoch(&cp, epoch);
    }
    cp.q(EE_CP_PLAYER_BASE_EPOCH_REACHED, 0, 0, &[epoch as i64]);
}

/// `EpochMediaLoaded` (`loaded`) and `EpochMediaUnloaded`: a real epoch's kind-7 media, then the player's.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_epoch_media(player: u64, epoch: c_long, loaded: bool) {
    let cp = Cp::of(player);
    if cp.is_epoch(epoch as i64) {
        cp.q(EE_CP_PLAYER_EPOCH_MEDIA, 0, 0, &[epoch as i64, loaded as i64]);
    }
    let base = if loaded { EE_CP_PLAYER_BASE_EPOCH_MEDIA_LOADED } else { EE_CP_PLAYER_BASE_EPOCH_MEDIA_UNLOADED };
    cp.q(base, 0, 0, &[epoch as i64]);
}

fn civ_name(cp: &Cp) -> Vec<u8> {
    let p = cp.q(EE_CP_PLAYER_CIV_NAME, 0, 0, &[]) as usize as *const c_char;
    if p.is_null() {
        return Vec::new();
    }
    // SAFETY: the player's UString's text, copied at once
    unsafe { CStr::from_ptr(p) }.to_bytes().to_vec()
}

fn choose_civilization(cp: &Cp) {
    let s = cp.scope(cpev::CHOOSE_CIVILIZATION);
    let epoch = cp.current_epoch().unwrap_or(100000) as i32;
    let map = cp.q(pg::EE_CP_PLACE_HAS_MAP, 0, 0, &[]) != 0;
    let waterless = map && cp.q(pg::EE_CP_PLACE_MAP_FLAGS, 0, 0, &[]) & ops::MAP_WATERLESS == ops::MAP_WATERLESS;
    let space = map && cp.on(EE_CP_PLAYER_SPACE_MAP);
    // SAFETY: the player's context
    let name = unsafe { pg::ee_cp_choose_civilization(&cp.ctx, epoch, waterless, space) };
    if !name.is_null() {
        cp.q(EE_CP_PLAYER_CIV_SET_NAME, name as u64, 0, &[]);
        cp.q(EE_CP_PLAYER_CIV_APPLY, 0, 0, &[]);
        cp.q(EE_CP_PLAYER_CIV_STORE, 0, 0, &[]);
    }
    s.i(ops::civ_hash(&civ_name(cp)) as c_long as i64);
}

// ---------------------------------------------------------------------------------------------------------------
// The tick

fn trace_manager(cp: &Cp, name: *const c_char) -> u64 {
    cp.q(EE_CP_PLAYER_TRACE_MANAGER, name as u64, 0, &[]) as u64
}

/// `EECPNextUpdate`: the bookkeeping the classic tick did first (a pending regroup, the combat state's decay, the
/// population cap), then the engine's tick.
fn next_update(cp: &Cp, time: c_ulong) {
    cp.q(EE_CP_PLAYER_NEXT_STATE_GET, 0, 0, &[]);
    if cp.placement() == 0 || !cp.has_tree() {
        return;
    }
    if cp.fields().mRegroupPending {
        update_groups(cp);
        cp.fields().mRegroupPending = false;
    }
    // SAFETY: the player's records
    let (combat, attacks) = {
        let t = unsafe { WorldRead::begin() };
        // SAFETY: a live player
        let p = unsafe { PlayerView::of(&t, cp.me()) }.expect("ee_sim::cp: a computer player");
        let attacks: c_long = crate::world_field!(p.fields(), mNumAttacks);
        // SAFETY: the player's record
        (unsafe { (*cp.saved()).mCombatState }, attacks)
    };
    if combat == 2 && attacks == 0 && time.wrapping_sub(cp.fields().mLastDamagedTime as c_ulong) > 60000 {
        set_combat_state(cp, 1);
        cp.fields().mBuildingDamaged = false;
        cp.fields().mCitizenDamaged = false;
    }
    if time >= cp.fields().mNextPopulationTime as c_ulong {
        work_out_population(cp);
        cp.fields().mNextPopulationTime = time.wrapping_add(60000) as _;
    }
    // SAFETY: the live computer player
    unsafe { crate::ai::next_game::ee_ai_next_update(cp.me() as usize as *mut c_void, time) };
}

/// `Update`: the start at the first call (the script, the reveal cheat, the trace line, the island timer, the
/// centres, the civilization, the extra resources), the next engine's tick, then the stranded units killed.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_update(player: u64, time: c_ulong) {
    let cp = Cp::of(player);
    let saved = trace_manager(&cp, c"cp".as_ptr());
    pc::ee_cp_capture_thread();
    if !cp.fields().mIsStarted {
        if !cp.observer() {
            // SAFETY: the live computer player
            unsafe { crate::ai::next_game::ee_ai_next_prepare(player as usize as *mut c_void) };
        }
        if cp.cheat_on(ops::CHEAT_REVEAL) {
            cp.q(EE_CP_PLAYER_EXPLORE_UNSEEN, 0, 0, &[]);
        }
        cp.q(EE_CP_PLAYER_TRACE_START, 0, 0, &[]);
        set_next_island_expansion_time(&cp);
        if center_count(&cp) == 0 {
            regroup(&cp);
        }
        if cp.q(EE_CP_PLAYER_LOAD_TYPE, 0, 0, &[]) == 0 {
            choose_civilization(&cp);
        }
        let bonus = if cp.cheat_on(ops::CHEAT_EXTRA) { cp.fields().mExtraResources as c_long } else { 0 };
        // SAFETY: the player's record
        unsafe {
            let caps = &mut (*cp.saved()).mResourceCaps;
            caps[ops::RESOURCE_GOLD] = caps[ops::RESOURCE_GOLD].wrapping_add(bonus);
            caps[ops::RESOURCE_IRON] = caps[ops::RESOURCE_IRON].wrapping_add(bonus);
        }
        cp.fields().mIsStarted = true;
    }
    if !cp.observer() && cp.first_unit() != 0 {
        let outer = trace_manager(&cp, c"next".as_ptr());
        next_update(&cp, time);
        trace_manager(&cp, outer as usize as *const c_char);
    }
    // SAFETY: the player's record
    let kills = unsafe { lists::ee_cp_kill_count(cp.ctx.fields) };
    for i in (0..kills).rev() {
        // SAFETY: as above
        let unit = resolve(unsafe { lists::ee_cp_kill_at(cp.ctx.fields, i) });
        cp.q(pg::EE_CP_PLAYER_KILL, unit, 0, &[]);
        // SAFETY: as above
        let unit = resolve(unsafe { lists::ee_cp_kill_at(cp.ctx.fields, i) });
        cp.q(pg::EE_CP_CENTER_UNIT_DEREFERENCE, unit, 0, &[]);
    }
    // SAFETY: as above
    unsafe { lists::ee_cp_kill_clear(cp.ctx.fields) };
    trace_manager(&cp, saved as usize as *const c_char);
}

/// `RTSResolveUnitID`: 0 for none (0xffffffff) or a freed unit.
fn resolve(id: u32) -> u64 {
    if id == 0xffff_ffff {
        0
    } else {
        crate::units::ffi::ee_units_resolve(id) as u64
    }
}

/// `Clear`: the player's, the centres deleted, not started, the placement reset, no danger zones.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_clear(player: u64) {
    let cp = Cp::of(player);
    cp.q(EE_CP_PLAYER_BASE_CLEAR, 0, 0, &[]);
    // SAFETY: the player's context
    unsafe { pg::ee_cp_centers_delete(&cp.ctx) };
    cp.fields().mIsStarted = false;
    {
        let s = cp.place_scope(plce::RESET);
        // SAFETY: as above
        unsafe { pg::ee_cp_placement_reset(&cp.ctx) };
        s.state(cp.placement());
    }
    // SAFETY: the player's record
    unsafe { lists::ee_cp_zones_set(cp.ctx.fields, std::ptr::null(), 0) };
}

// ---------------------------------------------------------------------------------------------------------------
// RepairPointers and the deferred restores

/// The computer players repaired while `gDeferComputerPlayerRestore` was set, with EEPlayer::RepairPointers' result.
static DEFERRED: Mutex<Vec<(u64, c_long)>> = Mutex::new(Vec::new());

/// `RepairPointers`: the player's, then the managers' restore (deferred while EEWorld::CreatePlayers repairs every
/// player of a 0.260-or-later save).
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_repair_pointers(player: u64, major: c_ulong, minor: c_ulong) -> c_long {
    let cp = Cp::of(player);
    let r = cp.q(EE_CP_PLAYER_BASE_REPAIR, 0, 0, &[major as i64, minor as i64]) as c_long;
    if cp.on(EE_CP_PLAYER_DEFER_RESTORE) {
        DEFERRED.lock().unwrap_or_else(|e| e.into_inner()).push((player, r));
        return r;
    }
    restore_managers(&cp, major, minor, r)
}

/// `EECPRestoreDeferredManagers`: the deferred players' managers restored in their repair order.
///
/// # Safety
/// The deferred players are live.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_restore_deferred(major: c_ulong, minor: c_ulong) {
    let list = std::mem::take(&mut *DEFERRED.lock().unwrap_or_else(|e| e.into_inner()));
    for (player, r) in list {
        restore_managers(&Cp::of(player), major, minor, r);
    }
}

fn placement_restore(cp: &Cp) -> c_long {
    let s = cp.place_scope(plce::RESTORE);
    // SAFETY: the player's context
    let r = unsafe { pg::ee_cp_placement_restore(&cp.ctx) } as c_long;
    s.i(r as i64).state(cp.placement());
    r
}

/// The rest of RepairPointers: the placement, the centres, the timers, the favourites, the stranded units.
fn restore_managers(cp: &Cp, major: c_ulong, minor: c_ulong, result: c_long) -> c_long {
    let s = cp.scope(cpev::RESTORE_MANAGERS);
    s.i(major as i64).i(minor as i64).i(result as i64);
    let fields = cp.ctx.fields;
    let mut r = result;
    if r == 0 {
        r = placement_restore(cp);
    }
    if r == 0 {
        // SAFETY: the player's record and context
        unsafe {
            super::ffi::ee_cp_restore_fields(fields, 0, cp.now(), true);
            r = pg::ee_cp_centers_restore(&cp.ctx, major as u32, minor as u32) as c_long;
            if r == 0 {
                super::ffi::ee_cp_restore_fields(fields, 1, cp.now(), true);
            }
        }
    }
    // SAFETY: as above
    unsafe {
        super::ffi::ee_cp_restore_fields(fields, 2, 0, r == 0);
        pg::ee_cp_restore_favorites(&cp.ctx);
    }
    cp.fields().mSeed = (cp.ctx.player_id % 10) as _;
    for unit in cp.own_units() {
        let facts = unit_view(unit, |u| {
            let c = u.class();
            (
                u.tile_xy().0 as c_long,
                u.complex_unit_handle() != 0,
                u.tile().map(|t| t.terrain_type()),
                c.map_or(0, |c| c.unit_type()) as c_ulong,
                c.map_or(0, |c| crate::world_field!(c.fields(), mDomains)),
                c.is_some_and(|c| crate::world_field!(c.fields(), mIsProjectile)),
            )
        });
        let Some((x, complex, terrain, unit_type, domains, projectile)) = facts else { continue };
        if ops::stranded(x, complex, terrain, unit_type, domains, projectile, || {
            cp.q(EE_CP_PLAYER_CAN_MOVE_ON, unit, 0, &[ops::TERRAIN_CLIFF]) != 0
        }) {
            cp.q(EE_CP_PLAYER_ADD_STRANDED, unit, 0, &[]);
        }
    }
    // SAFETY: as above
    unsafe { super::ffi::ee_cp_restore_fields(fields, 3, minor, major > 0) };
    s.i(r as i64);
    r
}

// ---------------------------------------------------------------------------------------------------------------
// The load and the save

/// The world's computer player of `id` is still `player` (an after-load fixup's test).
fn still_computer(id: u32, player: u64) -> bool {
    // SAFETY: the world's players
    unsafe { ee_cp_computer_at(id) == player }
}

/// The save's callback context: the player and its file.
struct SaveCtx {
    cp: Cp,
    file: u64,
    major: u32,
    minor: u32,
    primitive: EeUnitsPlayerSaveFn,
}

fn primitive(cp: &Cp) -> EeUnitsPlayerSaveFn {
    let f = cp.q(EE_CP_PLAYER_SAVE_PRIMITIVE, 0, 0, &[]) as usize;
    // SAFETY: the C++'s EESaveFilePrimitive
    unsafe { std::mem::transmute::<usize, EeUnitsPlayerSaveFn>(f) }
}

unsafe extern "C" fn save_op(ctx: *mut c_void, op: i32, index: u32, io: *mut EeUnitsPlayerSaveIo) -> i32 {
    // SAFETY: the context Load and Save hand
    let c = unsafe { &*(ctx as *const SaveCtx) };
    let cp = &c.cp;
    let file = c.file as usize as *mut c_void;
    match op {
        save::EE_CP_SAVE_PLACEMENT_LOAD => cp.q(EE_CP_PLAYER_PLACEMENT_LOAD, c.file, 0, &[]) as i32,
        save::EE_CP_SAVE_PLACEMENT_SAVE => cp.q(EE_CP_PLAYER_PLACEMENT_SAVE, c.file, 0, &[]) as i32,
        save::EE_CP_SAVE_TOWN_TILE => {
            // SAFETY: the placement's town tile (the save has a placement)
            unsafe { (*io).i = *cp.ctx.town.add(index as usize) as i64 };
            0
        }
        save::EE_CP_SAVE_SET_TOWN_TILE => {
            // SAFETY: as above
            unsafe { *cp.ctx.town.add(index as usize) = (*io).i as c_long };
            0
        }
        // SAFETY: the player's context and file
        save::EE_CP_SAVE_CENTER_LOAD => unsafe { pg::ee_cp_center_load(&cp.ctx, file, c.primitive, c.major, c.minor) },
        // SAFETY: as above
        save::EE_CP_SAVE_CENTER_SAVE => unsafe {
            pg::ee_cp_center_save_at(&cp.ctx, index as usize, file, c.primitive, c.major, c.minor)
        },
        // SAFETY: the file's primitive
        _ => unsafe { (c.primitive)(file, op, index, io) },
    }
}

/// `Save`: the player's, then ee_sim::cp::save's block.
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`]; `file` is a live `FSFile`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_save(player: u64, major: c_ulong, minor: c_ulong, file: u64) -> c_long {
    let cp = Cp::of(player);
    let r = cp.q(EE_CP_PLAYER_BASE_SAVE, file, 0, &[major as i64, minor as i64]) as i32;
    let now = cp.now() as u32;
    let fields = cp.ctx.fields;
    let primitive = primitive(&cp);
    let c = SaveCtx { cp, file, major: major as u32, minor: minor as u32, primitive };
    // SAFETY: the record; the context lives for the call
    unsafe { super::ffi::ee_cp_save(&c as *const SaveCtx as *mut c_void, save_op, fields, major as u32, minor as u32, now, r) as c_long }
}

/// `Load`: the player's, then ee_sim::cp::save's block; the favourites and their history and the running values
/// put back at the first tick (the load's EnterEpoch picks new favourites after this).
///
/// # Safety
/// As [`ee_cp_pl_save`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_load(player: u64, major: c_ulong, minor: c_ulong, file: u64) -> c_long {
    let cp = Cp::of(player);
    let r = cp.q(EE_CP_PLAYER_BASE_LOAD, file, 0, &[major as i64, minor as i64]) as i32;
    let fields = cp.ctx.fields;
    let primitive = primitive(&cp);
    let c = SaveCtx { cp, file, major: major as u32, minor: minor as u32, primitive };
    let mut loaded = EeCpLoaded::default();
    // SAFETY: as above
    let r = unsafe {
        super::ffi::ee_cp_load(&c as *const SaveCtx as *mut c_void, save_op, fields, major as u32, minor as u32, r, &mut loaded)
    } as c_long;
    let cp = c.cp;
    let id = cp.ctx.player_id;
    if loaded.favorites_ok && save::has_live_state(major as u32, minor as u32) {
        let f = cp.fields();
        let favorites = (f.mFavoriteLandType, f.mFavoriteSeaType, f.mFavoriteAirType, f.mFavoriteSpaceType);
        // SAFETY: the player's record
        let history: Vec<EeCpHistoryEntry> = unsafe {
            (0..lists::ee_cp_history_count(fields)).map(|i| lists::ee_cp_history_at(fields, i)).collect()
        };
        world_after_load(Box::new(move || {
                if !still_computer(id, player) {
                    return;
                }
                // SAFETY: the live player's record
                let f = unsafe { &mut *fields };
                (f.mFavoriteLandType, f.mFavoriteSeaType, f.mFavoriteAirType, f.mFavoriteSpaceType) = favorites;
                // SAFETY: the live player's record
                unsafe { lists::ee_cp_history_set(fields, history.as_ptr(), history.len()) };
        }));
    }
    if loaded.running {
        world_after_load(Box::new(move || {
                if !still_computer(id, player) {
                    return;
                }
                // SAFETY: as above
                let f = unsafe { &mut *fields };
                f.mPopulationCap = loaded.population_cap as _;
                f.mIsEnableTimeSet = loaded.enable_time_set;
                f.mRegroupPending = loaded.regroup_pending;
                f.mNextPopulationTime = loaded.next_population_time as _;
                f.mLastDamagedTime = loaded.last_damaged_time as _;
        }));
    }
    r
}

/// The experiments' timers' and the resource state's blocks (EECPSaveExperimentTimers .. EECPLoadResourceState):
/// `what` 0 saves the timers, 1 loads them, 2 saves the resource state, 3 loads it; `player` is any computer
/// player's handle for the host (the after-load fixups), 0 when there is none.
///
/// # Safety
/// `file` is a live `FSFile`; `player` is 0 or a live computer player.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_state_block(file: u64, what: u32, primitive: EeUnitsPlayerSaveFn) -> c_long {
    let f = file as usize as *mut c_void;
    match what {
        // SAFETY: the caller's contract
        0 => unsafe { super::ffi::ee_cp_timers_save(f, primitive) as c_long },
        1 => {
            // SAFETY: as above
            let r = unsafe { super::ffi::ee_cp_timers_load(f, primitive) };
            if r == 0 {
                world_after_load(Box::new(|| super::ffi::ee_cp_timers_apply(0)));
            }
            r as c_long
        }
        2 => {
            let mut c = StateCtx { file, primitive, zones: Vec::new(), x: 0, y: 0 };
            let t = unsafe { WorldRead::begin() };
            let n = WorldView::global(&t).map_or(0, |w| w.num_players()) as u32;
            // SAFETY: the context lives for the call
            unsafe { super::ffi::ee_cp_resource_state_save(&mut c as *mut StateCtx as *mut c_void, state_op, n) as c_long }
        }
        _ => {
            let mut c = StateCtx { file, primitive, zones: Vec::new(), x: 0, y: 0 };
            // SAFETY: as above
            let r = unsafe { super::ffi::ee_cp_resource_state_load(&mut c as *mut StateCtx as *mut c_void, state_op) };
            if r == 0 {
                let zones = std::mem::take(&mut c.zones);
                // after the experiment timers' fixup, which zeroes the claim ranges
                world_after_load(Box::new(move || {
                    super::ffi::ee_cp_timers_apply(1);
                    for (p, z) in zones.iter().enumerate() {
                        let fields = computer_fields(p as u32);
                        if !fields.is_null() {
                            // SAFETY: the live computer player's record
                            unsafe { lists::ee_cp_zones_set(fields, z.as_ptr(), z.len()) };
                        }
                    }
                }));
            }
            r as c_long
        }
    }
}

extern "C" {
    /// `EEAfterLoad([f, b]() { f(b); })` (EEComputerPlayer.cpp)
    fn ee_cp_after_load(f: unsafe extern "C" fn(u64), b: u64);
    /// `gWorld.mPlayers[id]`'s `mComputerPlayer` (0: none or past the players)
    fn ee_cp_computer_at(id: u32) -> u64;
}

fn world_after_load(f: Box<dyn FnOnce()>) {
    unsafe extern "C" fn run(f: u64) {
        // SAFETY: the box world_after_load made, run once
        let f = unsafe { Box::from_raw(f as usize as *mut Box<dyn FnOnce()>) };
        f()
    }
    // SAFETY: the C++ queue keeps the box until the first tick
    unsafe { ee_cp_after_load(run, Box::into_raw(Box::new(f)) as usize as u64) }
}

/// The world's computer player `id` (its `EEPlayer*`; 0: none or past the players).
///
/// # Safety
/// On the simulation thread.
pub unsafe fn computer_at(id: u32) -> u64 {
    // SAFETY: the world's players
    unsafe { ee_cp_computer_at(id) }
}

/// The record of the world's computer player `id` (null: none).
fn computer_fields(id: u32) -> *mut EeCpFields {
    // SAFETY: the world's players
    let p = unsafe { ee_cp_computer_at(id) };
    if p == 0 {
        std::ptr::null_mut()
    } else {
        (p as usize + layout().cp_fields as usize) as *mut EeCpFields
    }
}

struct StateCtx {
    file: u64,
    primitive: EeUnitsPlayerSaveFn,
    /// a load's zones, by player id
    zones: Vec<Vec<EeCpDangerZone>>,
    x: c_long,
    y: c_long,
}

unsafe extern "C" fn state_op(ctx: *mut c_void, op: i32, index: u32, io: *mut EeUnitsPlayerSaveIo) -> i32 {
    // SAFETY: the context the blocks hand
    let c = unsafe { &mut *(ctx as *mut StateCtx) };
    // SAFETY: the operation's values
    let io = unsafe { &mut *io };
    match op {
        EE_CP_STATE_ZONES => {
            let f = computer_fields(index);
            // SAFETY: a live computer player's record
            io.i = if f.is_null() { 0 } else { unsafe { lists::ee_cp_zone_count(f) as i64 } };
            0
        }
        EE_CP_STATE_ZONE => {
            // SAFETY: as above (the save asks a listed zone of a computer player)
            let z = unsafe { lists::ee_cp_zone_at(computer_fields(index), io.i as usize) };
            io.i = match io.b {
                0 => z.x as i64,
                1 => z.y as i64,
                _ => z.time as i64,
            };
            0
        }
        EE_CP_STATE_PUSH_PLAYER => {
            c.zones.push(Vec::new());
            0
        }
        EE_CP_STATE_PUSH_ZONE => {
            match io.b {
                0 => c.x = io.i as c_long,
                1 => c.y = io.i as c_long,
                _ => {
                    let z = EeCpDangerZone { x: c.x, y: c.y, time: io.i as c_ulong };
                    if let Some(last) = c.zones.last_mut() {
                        last.push(z);
                    }
                }
            }
            0
        }
        // SAFETY: the file's primitive
        _ => unsafe { (c.primitive)(c.file as usize as *mut c_void, op, index, io) },
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The favourite screener and the hero count

/// `EEAIFOFavoriteUnitScreener(player, kind, epoch, 0).Screen(class)` for an enabled class `class`.
pub(crate) fn favorite_screen(ctx: &EeCpPlaceCtx, class: u64, domain_kind: i32, epoch: i64) -> bool {
    let cp = Cp { ctx: EeCpPlaceCtx { ..*ctx } };
    let t = unsafe { WorldRead::begin() };
    // SAFETY: a listed class
    let Some(c) = (unsafe { ClassView::of(&t, class) }) else { return false };
    let f = c.fields();
    let facts = FavoriteClass {
        epoch: crate::world_field!(f, mEpoch),
        max_velocity: crate::world_field!(f, mMaxVelocity),
        attack: crate::world_field!(f, mAttack),
        category: crate::world_field!(f, mCategory),
        domains: crate::world_field!(f, mDomains),
        ai_kind: crate::world_field!(f, mAIKind),
        calamity_powers: crate::world_field!(f, mHasCalamityPowers),
        conversion_rate: crate::world_field!(f, mConversionRate),
        priest: false,
        ammo_type: crate::world_field!(f, mAmmoType),
        medic: crate::world_field!(f, mIsMedic),
        transport: false,
        family: crate::world_field!(f, mFamily),
        flies_over: false,
        unit_type: crate::world_field!(f, mUnitType),
    };
    let technology = c.technology();
    drop(t);
    let epoch = if (0..cp.q(EE_CP_PLAYER_EPOCH_COUNT, 0, 0, &[])).contains(&epoch) { epoch as c_long } else { 0 };
    if !ops::screen_class(&facts, epoch, 0) {
        return false;
    }
    let d = ops::domain(&facts);
    let allowed = match d {
        ops::Domain::Air => domain_kind == super::events::EE_CP_FAVORITE_AIR,
        ops::Domain::Sea => domain_kind == super::events::EE_CP_FAVORITE_SEA,
        ops::Domain::Space => domain_kind == super::events::EE_CP_FAVORITE_SPACE,
        ops::Domain::Land => domain_kind == super::events::EE_CP_FAVORITE_LAND,
    };
    if !allowed {
        return false;
    }
    // the class's node live, its builder's node live or the builder present
    let node_live = |id: i64| -> Option<(bool, i64)> {
        let t = unsafe { WorldRead::begin() };
        let tree = unsafe { PlayerView::of(&t, cp.me()) }?.tech_tree()?;
        let n = tree.node(id)?;
        let scen = crate::world_field!(n.fields(), mScenEditorObsoleted);
        let first = n.unit_class(0);
        drop(t);
        let obsolete = cp.q(EE_CP_PLAYER_NODE_OBSOLETE, 0, 0, &[id]) != 0;
        Some((!obsolete && !scen, first))
    };
    let Some((true, first)) = node_live(technology) else { return false };
    let t = unsafe { WorldRead::begin() };
    let Some(builder) = (first >= 0).then(|| ClassView::by_index(&t, first as usize)).flatten() else { return false };
    let builder_tech = builder.technology();
    drop(t);
    let Some((builder_live, _)) = node_live(builder_tech) else { return false };
    if !builder_live && count_own(&cp, first as i32) <= 0 {
        return false;
    }
    let class_test = |op: i32| cp.q(op, class, 0, &[]) != 0;
    let facts = FavoriteClass {
        priest: d != ops::Domain::Land && class_test(EE_CP_PLAYER_CLASS_IS_PRIEST),
        transport: class_test(EE_CP_PLAYER_CLASS_IS_TRANSPORT),
        flies_over: d == ops::Domain::Land && class_test(EE_CP_PLAYER_CLASS_FLIES_OVER),
        ..facts
    };
    ops::is_favorite(&facts, d, |set| {
        if cp.q(EE_CP_PLAYER_SET_EMPTY, 0, 0, &[set]) != 0 {
            None
        } else {
            Some(cp.q(EE_CP_PLAYER_SET_HAS, 0, 0, &[set, facts.unit_type as i64]) != 0)
        }
    })
}

/// `EECPCountOwn` with its PLCE scope.
fn count_own(cp: &Cp, unit_type: i32) -> i32 {
    let s = cp.place_scope(plce::COUNT_OWN);
    s.i(unit_type as i64);
    // SAFETY: the player's context
    let r = unsafe { pg::ee_cp_count_own(&cp.ctx, unit_type) };
    s.i(r as i64);
    r
}

/// `GetHeroCount`: the player's heroes by class (`by_class`) plus what its land buildings make (`requests`).
///
/// # Safety
/// As [`ee_cp_pl_set_combat_state`].
#[no_mangle]
pub unsafe extern "C" fn ee_cp_pl_hero_count(player: u64, by_class: bool, requests: bool) -> c_long {
    let cp = Cp::of(player);
    let t = unsafe { WorldRead::begin() };
    let hero = |class: i64| {
        class > 0 && ClassView::by_index(&t, class as usize).is_some_and(|c| c.ai_kind() as c_ulong == ops::AI_KIND_HERO)
    };
    let units: Vec<UnitView<'_>> = cp.own_units().into_iter().filter_map(|u| unsafe { UnitView::of(&t, u) }).collect();
    let mut count: c_long = 0;
    if by_class {
        let mut types: Vec<c_ulong> = units
            .iter()
            .filter_map(|u| {
                let c = u.class()?;
                let hp = u.hit_points() as c_long;
                ops::hero_walk_counts(u.complex_unit_handle() != 0, hp, c.category() as c_ulong, c.unit_type() as c_ulong)
                    .then_some(c.unit_type() as c_ulong)
            })
            .collect();
        types.sort_unstable();
        // each class's units counted as one list (CountUnits)
        let mut i = 0;
        while i < types.len() {
            let n = types[i..].iter().take_while(|&&k| k == types[i]).count();
            if hero(types[i] as i64) {
                count = count.wrapping_add(n as c_long);
            }
            i += n;
        }
    }
    let mut requested: c_long = 0;
    if requests {
        let tree = unsafe { PlayerView::of(&t, player) }.and_then(|p| p.tech_tree());
        let made = |tech: i64| -> i64 {
            tree.and_then(|tr| tr.node(tech)).and_then(|n| n.data()).map_or(0, |d| {
                let c: c_long = crate::world_field!(d, mUnitClass);
                c as i64
            })
        };
        for u in &units {
            let Some(c) = u.complex_unit() else { continue };
            if !c.class().is_some_and(|k| k.land_building()) {
                continue;
            }
            let research = c.current_research();
            if research != ops::TECH_NONE as i64 && hero(made(research)) {
                requested += 1;
            }
            for k in (0..c.build_queue_len()).rev() {
                if c.build_queue_technology(k).is_some_and(|tech| hero(made(tech))) {
                    requested += 1;
                }
            }
        }
    }
    count.wrapping_add(requested)
}

/// `EETechTreeNode::GetUnitClass(i)`: the node's unit class `i`, or its default class past the end.
///
/// # Safety
/// `node` is a live tech node.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_node_unit_class(node: u64, i: c_ulong) -> c_long {
    let t = unsafe { WorldRead::begin() };
    // SAFETY: the caller's contract
    unsafe { crate::world::NodeView::of(&t, node) }.map_or(0, |n| n.unit_class(i as usize) as c_long)
}
