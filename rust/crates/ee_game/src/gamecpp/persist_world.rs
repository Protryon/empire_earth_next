//! The saved world's chunk (was EEPWorldHelper.cpp, 0x14bd64..0x14c6b1): the helper's constructors, `Load`, `Save`
//! and `Restore`, the teams' load and save, and the host of ee_sim's world_save.rs (the running state from save
//! 0.254 on: the unit chain, the stepped units, the forests, the computer players' schedule, the tiles', regions' and
//! players' units, the ghosts the goals hold, the other systems' parts), with its two parts of this file (the tech
//! nodes not allowed, 0.282; the tiles' terrain types, 0.283) and the loaded ghosts' making. Each C++ call or member
//! is a case of EEPWorldHelper.cpp's `EEPWorldOp` ([`super::EeGameCpp`]'s lane C section).
//!
//! The helper's plain members from `mTimeOfDay` to `mUsingCarbon` are read in place ([`EePWorldHelperFields`]).

#![allow(non_snake_case)]
// the C++'s `inMajorVersion <= 0` on unsigned versions, kept as written
#![allow(clippy::unnecessary_cast, clippy::missing_safety_doc, clippy::absurd_extreme_comparisons)]

use std::ffi::{c_long, c_ulong, c_void};
use std::ptr::addr_of_mut;

use ee_base::math::types::GeTransformation;
use ee_sim::ai::next_game::schedule;
use ee_sim::app::active_ffi::{ee_app_active_list, ee_app_active_set, ee_app_active_set_list};
use ee_sim::astar::cm_file::{ee_astar_cm_load_regions, ee_astar_cm_save_regions};
use ee_sim::astar::pm::ee_path_manager_file;
use ee_sim::combat::regions::{ee_combat_regions_order, ee_combat_regions_restore, EeCombatUnitRegions};
use ee_sim::cp::player_game::ee_cp_pl_state_block;
use ee_sim::save_versions::*;
use ee_sim::units::group_host::{ee_units_groups_load_active, ee_units_groups_save_active};
use ee_sim::units::player_save::EeUnitsPlayerSaveIo;
use ee_sim::units::save_prim::game::ee_units_save_primitive;
use ee_sim::units::world_save::*;
use ee_sim::world::player_fields::EeUnitsRtsPlayerFields;
use ee_sim::world::unit_class_fields::EeUnitsUnitClassFields;
use ee_sim::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};

use super::persistent::{after_load, Longs, Pf, PF_BEYOND_END, PF_OK};
use super::state::EeGameStateTail;
use super::{h, hd, hp, pa, ptr, rec, O, P};
use crate::app::game_options::EeAppOptionsFields;

/// `EEPWorldHelper`'s members from `mTimeOfDay` (+0x1c) to `mUsingCarbon` (+0x3b), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EePWorldHelperFields {
    pub mTimeOfDay: f32,
    pub mZoom: f32,
    pub mGameSpeed: f32,
    pub mPopulationLimit: c_ulong,
    pub mNumberOfWondersToWin: c_ulong,
    pub mTownIDCounter: c_long,
    pub mHerdIDCounter: u16,
    pub mC36: i8,
    pub mVictoryEnabled: bool,
    pub mUnitImprovementsAllowed: bool,
    pub mTeamsLocked: bool,
    pub mCheatsEnabled: bool,
    pub mUsingCarbon: bool,
}

/// The host's context: the file and its version (null at the first tick's restore).
#[repr(C)]
struct PwCtx {
    file: P,
    major: c_ulong,
    minor: c_ulong,
}

fn fields(helper: P) -> *mut EePWorldHelperFields {
    // SAFETY: the helper's plain members (one address case)
    unsafe { rec(O::PwhFields, helper) }
}

fn rts<'a>(u: P) -> &'a mut EeUnitsRtsUnitFields {
    // SAFETY: a live unit's RTSUnit record, used before the next C++ call that changes it
    unsafe { &mut *rec(O::UnRts, u) }
}

fn unf<'a>(u: P) -> &'a mut EeUnitsUnitFields {
    // SAFETY: as rts
    unsafe { &mut *rec(O::UnFields, u) }
}

fn cxf<'a>(cx: P) -> &'a mut EeUnitsComplexUnitFields {
    // SAFETY: a live complex unit's record
    unsafe { &mut *rec(O::CxFields, cx) }
}

fn num_players() -> c_ulong {
    h(O::PwNumPlayers, 0, &[]) as c_ulong
}

fn player(i: u64) -> P {
    hp(O::WorldPlayer, 0, &[i as i64])
}

fn unit_id(u: P) -> c_ulong {
    rts(u).mUnitID
}

fn world_unit(id: c_ulong) -> P {
    hp(O::WorldUnit, 0, &[id as i64])
}

/// `EEPWorldHelper()`: no teams, improvements, team lock and cheats allowed.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pwh_init(helper: *mut c_void) {
    let helper = helper as P;
    h(O::PwhSetScenario, helper, &[0]);
    super::teams::helper_new(helper);
    // SAFETY: the helper's members
    unsafe {
        *fields(helper) = EePWorldHelperFields {
            mUnitImprovementsAllowed: true,
            mTeamsLocked: true,
            mCheatsEnabled: true,
            ..Default::default()
        };
    }
}

/// `EEPWorldHelper(EEWorld&)`: the world's teams, time of day, zoom, speed, limits, victory and cheat settings and id
/// counters.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pwh_from_world(helper: *mut c_void, world: *mut c_void) {
    let (helper, w) = (helper as P, world as P);
    h(O::PwhSetScenario, helper, &[0]);
    let f = fields(helper);
    // SAFETY: the helper's and the world's members, each read before the next C++ call
    unsafe {
        (*f).mTimeOfDay = *(hp(O::WTimeOfDay, w, &[]) as *const f32);
        (*f).mZoom = super::camera_io::zoom();
        (*f).mGameSpeed = hd(O::PwWorldGameSpeed, w, &[], &[]);
        (*f).mPopulationLimit = *(hp(O::PwWorldPopulationLimit, w, &[]) as *const c_long) as c_ulong;
        (*f).mNumberOfWondersToWin = *(hp(O::WWondersToWin, w, &[]) as *const c_ulong);
        (*f).mC36 = 0;
    }
    super::teams::helper_from_world(helper);
    // SAFETY: as above
    unsafe {
        (*f).mVictoryEnabled = *(hp(O::WVictoryEnabled, w, &[]) as *const bool);
        (*f).mUnitImprovementsAllowed = *(hp(O::WUnitImprovementsAllowed, w, &[]) as *const bool);
        (*f).mTeamsLocked = *(hp(O::WTeamsLocked, w, &[]) as *const bool);
        (*f).mCheatsEnabled = *(hp(O::WCheatsEnabled, w, &[]) as *const bool);
        (*f).mTownIDCounter = *(hp(O::WTownIDCounter, w, &[]) as *const c_long);
        (*f).mHerdIDCounter = *(hp(O::WHerdIDCounter, w, &[]) as *const u16);
        (*f).mUsingCarbon = *(hp(O::WUsingCarbon, w, &[]) as *const bool);
    }
}

/// `EEPWorldHelper::Load`: the teams, world settings and id counters (the Carbon flag from 0.239 on), the running
/// state (from 0.254: world_save.rs), then the chat log and the main game form; only the form's error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pwh_load(
    helper: *mut c_void,
    major: c_ulong,
    minor: c_ulong,
    file: *mut c_void,
) -> c_long {
    let (helper, file) = (helper as P, Pf(file as P));
    super::teams::helper_load(helper, major, minor, file);
    let f = fields(helper);
    // SAFETY: the helper's members, each address taken for one read
    unsafe {
        file.read_ulong(addr_of_mut!((*f).mNumberOfWondersToWin));
        file.read_f32(addr_of_mut!((*f).mTimeOfDay));
        file.read_i8(addr_of_mut!((*f).mC36));
        file.read_f32(addr_of_mut!((*f).mZoom));
        file.read_ulong(addr_of_mut!((*f).mPopulationLimit));
        file.read_f32(addr_of_mut!((*f).mGameSpeed));
        file.read_bool(addr_of_mut!((*f).mVictoryEnabled));
        file.read_bool(addr_of_mut!((*f).mUnitImprovementsAllowed));
        file.read_bool(addr_of_mut!((*f).mTeamsLocked));
        file.read_bool(addr_of_mut!((*f).mCheatsEnabled));
        file.read_long(addr_of_mut!((*f).mTownIDCounter));
        file.read_u16(addr_of_mut!((*f).mHerdIDCounter));
        if major <= 0 && minor < 0xef {
            (*f).mUsingCarbon = false;
        } else {
            file.read_bool(addr_of_mut!((*f).mUsingCarbon));
        }
    }
    if EESaveHasLiveState(major, minor) {
        let mut ctx = PwCtx { file: file.0, major, minor };
        // SAFETY: the context lives for the call; the host answers its operations
        unsafe { ee_units_world_load_live(&mut ctx as *mut PwCtx as *mut c_void, ee_gamecpp_pworld_host) };
    }
    // SAFETY: the live file (the chat log's; the tests have no screens)
    #[cfg(not(test))]
    unsafe {
        crate::screens::chat_log_ffi::ee_screens_chat_log_load(file.0 as *mut c_void)
    };
    h(O::PwMainFormLoad, file.0, &[major as i64, minor as i64]) as c_long
}

/// `EEPWorldHelper::Save`: as Load's order (the Carbon flag always), the running state, the chat log and the main game
/// form; only the form's error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pwh_save(
    helper: *mut c_void,
    major: c_ulong,
    minor: c_ulong,
    file: *mut c_void,
) -> c_long {
    let (helper, file) = (helper as P, Pf(file as P));
    super::teams::helper_save(helper, file);
    // SAFETY: the helper's members (no C++ call changes them while they are written)
    let f = unsafe { *fields(helper) };
    file.write_ulong(f.mNumberOfWondersToWin);
    file.write_f32(f.mTimeOfDay);
    file.write_i8(f.mC36);
    file.write_f32(f.mZoom);
    file.write_ulong(f.mPopulationLimit);
    file.write_f32(f.mGameSpeed);
    file.write_bool(f.mVictoryEnabled);
    file.write_bool(f.mUnitImprovementsAllowed);
    file.write_bool(f.mTeamsLocked);
    file.write_bool(f.mCheatsEnabled);
    file.write_long(f.mTownIDCounter);
    file.write_u16(f.mHerdIDCounter);
    file.write_bool(f.mUsingCarbon);
    if EESaveHasLiveState(major, minor) {
        let mut ctx = PwCtx { file: file.0, major, minor };
        // SAFETY: as Load's
        unsafe { ee_units_world_save_live(&mut ctx as *mut PwCtx as *mut c_void, ee_gamecpp_pworld_host) };
    }
    // SAFETY: the live file (the chat log's; the tests have no screens)
    #[cfg(not(test))]
    unsafe {
        crate::screens::chat_log_ffi::ee_screens_chat_log_save(file.0 as *mut c_void)
    };
    h(O::PwMainFormSave, file.0, &[major as i64, minor as i64]) as c_long
}

/// `EEPWorldHelper::Restore`: the loaded teams, world settings and id counters into the game state (and its scenario
/// manager, if any).
#[no_mangle]
pub extern "C" fn ee_gamecpp_pwh_restore(helper: *mut c_void, state: *mut c_void) {
    let (helper, st) = (helper as P, state as P);
    let scenario = h(O::PwhScenario, helper, &[]);
    if scenario != 0 {
        h(O::PwStSetScenario, st, &[scenario]);
    }
    super::teams::helper_to_state(helper);
    // SAFETY: the helper's, the state's and its options' members, no C++ call between
    unsafe {
        let f = *fields(helper);
        let t = &mut *rec::<EeGameStateTail>(O::StTail, st);
        let o = &mut *rec::<EeAppOptionsFields>(O::OptFields, hp(O::StOptionsPtr, st, &[]));
        o.mWondersForVictory = f.mNumberOfWondersToWin;
        t.mTimeOfDay = f.mTimeOfDay;
        t.mC4FA = f.mC36;
        t.mZoom = f.mZoom;
        o.mUnitLimit = f.mPopulationLimit;
        t.mGameSpeed = f.mGameSpeed;
        o.mVictory = f.mVictoryEnabled;
        t.mUnitImprovementsAllowed = f.mUnitImprovementsAllowed;
        o.mCheatCodes = f.mCheatsEnabled;
        t.mTeamsLocked = f.mTeamsLocked;
        t.mTownIDCounter = f.mTownIDCounter;
        t.mHerdIDCounter = f.mHerdIDCounter as i16;
        t.mUsingCarbon = f.mUsingCarbon;
    }
}

fn fill(io: &mut EeUnitsWorldIo, i: usize, v: u64) {
    if i < io.cap {
        // SAFETY: the list holds `cap` entries
        unsafe { *io.list.add(i) = v };
    }
}

fn fill_all(io: &mut EeUnitsWorldIo, items: &[P]) -> i64 {
    for (i, &u) in items.iter().enumerate() {
        fill(io, i, u as u64);
    }
    items.len() as i64
}

/// `EESaveIsPrimitive`: the file's primitives (player_save.rs's numbers).
fn is_primitive(op: i32) -> bool {
    use ee_sim::units::player_save::*;
    (EE_UNITS_PLAYER_SAVE_READ_ULONG..=EE_UNITS_PLAYER_SAVE_WRITE_BOOL).contains(&op)
        || op == EE_UNITS_PLAYER_SAVE_READ_USHORT
        || op == EE_UNITS_PLAYER_SAVE_WRITE_USHORT
}

/// The save's part predicates (`EE_UNITS_WORLD_HAS`'s `a`).
fn has_op(which: i64) -> Option<extern "C" fn(c_ulong, c_ulong) -> bool> {
    Some(match which {
        EE_UNITS_WORLD_HAS_RUNNING_STATE => EESaveHasRunningState,
        EE_UNITS_WORLD_HAS_REGION_ORDER => EESaveHasRegionOrder,
        EE_UNITS_WORLD_HAS_EXPERIMENT_TIMERS => EESaveHasExperimentTimers,
        EE_UNITS_WORLD_HAS_TOWNS => EESaveHasTowns,
        EE_UNITS_WORLD_HAS_TOWN_AREAS => EESaveHasTownAreas,
        EE_UNITS_WORLD_HAS_GHOSTS => EESaveHasGhosts,
        EE_UNITS_WORLD_HAS_GHOST_RESOURCES => EESaveHasGhostResources,
        EE_UNITS_WORLD_HAS_GOAL_GHOSTS => EESaveHasGoalGhosts,
        EE_UNITS_WORLD_HAS_CONTINENT_REGIONS => EESaveHasContinentRegions,
        EE_UNITS_WORLD_HAS_RESOURCE_STATE => EESaveHasResourceState,
        EE_UNITS_WORLD_HAS_PLAYER_UNIT_ORDER => EESaveHasPlayerUnitOrder,
        EE_UNITS_WORLD_HAS_PATH_SEARCHES => EESaveHasPathSearches,
        EE_UNITS_WORLD_HAS_ACTIVE_GROUPS => EESaveHasActiveGroups,
        EE_UNITS_WORLD_HAS_GRAPH_SEARCHES => EESaveHasGraphSearches,
        EE_UNITS_WORLD_HAS_TECH_ALLOWED => EESaveHasTechAllowed,
        EE_UNITS_WORLD_HAS_TILE_TYPES => EESaveHasTileTypes,
        _ => return None,
    })
}

/// The other systems' parts (`PART_*`) as their load and save cases; this file's two parts are Rust.
fn part_ops(part: i64) -> Option<(O, O)> {
    Some(match part {
        EE_UNITS_WORLD_PART_AREA_EFFECTS => (O::PwAreaEffectsLoad, O::PwAreaEffectsSave),
        EE_UNITS_WORLD_PART_TOWNS => (O::PwTownsLoad, O::PwTownsSave),
        _ => return None,
    })
}

/// `EESaveFilePrimitive`: the file primitives on the saved game's `FSFile` (the index unused).
///
/// # Safety
/// `file` a live `FSFile`; `io` a live record.
unsafe extern "C" fn file_primitive(file: *mut c_void, op: i32, _index: u32, io: *mut EeUnitsPlayerSaveIo) -> i32 {
    // SAFETY: the caller's contract
    unsafe { ee_units_save_primitive(file, op, io) }
}

/// The parts whose load and save are Rust's: the path manager's three (`EEPathManager::LoadRunningState` ..
/// `SaveGraphSearches`), the computer players' two state blocks (`EECPLoad/SaveExperimentTimers`,
/// `EECPLoad/SaveResourceState`), the continent regions (`EELoad/SaveContinentRegions`) and the active groups
/// (`EELoad/SaveActiveGroups`); a save's result is 0, as the C++ calls' was dropped. None for the other parts.
fn rust_part(file: *mut c_void, load: bool, which: i64) -> Option<i64> {
    let path = |what: i32| {
        // SAFETY: the game's path manager (gPathManager's) and the saved game's file
        let r = unsafe { ee_path_manager_file(ee_sim::astar::pm::global(), what, load, file) } as i64;
        if load {
            r
        } else {
            0
        }
    };
    let state = |what: u32| {
        // SAFETY: the saved game's file and its primitives
        let r = unsafe { ee_cp_pl_state_block(file as u64, what + load as u32, file_primitive) } as i64;
        if load {
            r
        } else {
            0
        }
    };
    // SAFETY (the calls below): the saved game's file, its primitives and the game's continent record
    Some(match which {
        EE_UNITS_WORLD_PART_PATH_RUNNING => path(0),
        EE_UNITS_WORLD_PART_PATH_SEARCHES => path(1),
        EE_UNITS_WORLD_PART_GRAPH_SEARCHES => path(2),
        EE_UNITS_WORLD_PART_EXPERIMENT_TIMERS => state(0),
        EE_UNITS_WORLD_PART_RESOURCE_STATE => state(2),
        EE_UNITS_WORLD_PART_CONTINENT_REGIONS if load => (unsafe { ee_astar_cm_load_regions(file) }) as i64,
        EE_UNITS_WORLD_PART_CONTINENT_REGIONS => {
            unsafe { ee_astar_cm_save_regions(ee_sim::astar::world_view::cm_ptr(), file) };
            0
        }
        EE_UNITS_WORLD_PART_ACTIVE_GROUPS if load => {
            (unsafe { ee_units_groups_load_active(file, file_primitive) }) as i64
        }
        EE_UNITS_WORLD_PART_ACTIVE_GROUPS => {
            unsafe { ee_units_groups_save_active(file, file_primitive) };
            0
        }
        _ => return None,
    })
}

fn part(ctx: &PwCtx, load: bool, which: i64, town_areas: bool) -> i64 {
    let file = Pf(ctx.file);
    match which {
        EE_UNITS_WORLD_PART_TECH_ALLOWED => {
            if load {
                return load_tech_allowed(file);
            }
            save_tech_allowed(file);
            0
        }
        EE_UNITS_WORLD_PART_TILE_TYPES => {
            if load {
                return load_tile_types(file);
            }
            save_tile_types(file);
            0
        }
        _ => match (rust_part(file.0 as *mut c_void, load, which), part_ops(which)) {
            (Some(r), _) => r,
            (None, Some((l, _))) if load => h(l, file.0, &[town_areas as i64]),
            (None, Some((_, s))) => {
                h(s, file.0, &[]);
                0
            }
            (None, None) => 0,
        },
    }
}

extern "C" fn restore_at_first_tick(pending: usize) {
    // SAFETY: the record world_save.rs handed over; the host takes no context at the first tick
    unsafe { ee_units_world_restore(pending as *mut c_void, std::ptr::null_mut(), ee_gamecpp_pworld_host) };
}

/// The host of world_save.rs (was EEPWorldHost): the file's primitives, the version's predicates, the other systems'
/// parts and the world's units by address.
///
/// # Safety
/// `ctx` is null (the first tick's restore) or a live [`PwCtx`]; `io` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_pworld_host(ctx: *mut c_void, op: i32, io: *mut EeUnitsWorldIo) -> i64 {
    // SAFETY: the caller's record and context
    let io = unsafe { &mut *io };
    let ctx = ctx as *const PwCtx;
    if is_primitive(op) {
        // SAFETY: a primitive comes with the file's context
        let c = unsafe { &*ctx };
        let mut sio = EeUnitsPlayerSaveIo { i: io.a, f: io.f, b: (io.b != 0) as u8 };
        // SAFETY: the saved game's file
        let r = unsafe { ee_units_save_primitive(c.file as *mut c_void, op, &mut sio) };
        io.a = sio.i;
        io.f = sio.f;
        io.b = sio.b as i64;
        return r as i64;
    }
    let u = io.a as P;
    match op {
        EE_UNITS_WORLD_HAS => {
            // SAFETY: as above
            let c = unsafe { &*ctx };
            match has_op(io.a) {
                Some(has) => has(c.major, c.minor) as i64,
                None => 0,
            }
        }
        // SAFETY: as above
        EE_UNITS_WORLD_PART_LOAD => part(unsafe { &*ctx }, true, io.a, io.b != 0),
        EE_UNITS_WORLD_PART_SAVE => part(unsafe { &*ctx }, false, io.a, false),
        EE_UNITS_WORLD_AFTER_LOAD => {
            after_load(restore_at_first_tick, io.a as usize);
            0
        }
        EE_UNITS_WORLD_DEBUG => {
            h(O::PwDebugText, 0, &[io.list as usize as i64]);
            0
        }
        EE_UNITS_WORLD_UNIT_ID => unit_id(u) as i64,
        EE_UNITS_WORLD_CHAIN => {
            let mut n = 0;
            let mut c = hp(O::PwChainFirst, 0, &[]);
            while c != 0 {
                fill(io, n, c as u64);
                n += 1;
                c = hp(O::PwTileNodeNext, c, &[]);
            }
            n as i64
        }
        EE_UNITS_WORLD_UNIT => world_unit(io.a as c_ulong) as i64,
        EE_UNITS_WORLD_IN_CHAIN => {
            let node = hp(O::UnTileNode, u, &[]);
            (node != 0 && (hp(O::PwNodePrev, node, &[]) != 0 || hp(O::PwChainFirst, 0, &[]) == u)) as i64
        }
        EE_UNITS_WORLD_TILE_X => rts(u).mTileX as i64,
        EE_UNITS_WORLD_LINK => {
            let to = if io.b != 0 { io.b as P } else { hp(O::PwChainHead, 0, &[]) };
            h(O::PwLink, u, &[pa(to)]);
            0
        }
        EE_UNITS_WORLD_STEP_LIST | EE_UNITS_WORLD_STEP_SET | EE_UNITS_WORLD_STEP_DROP | EE_UNITS_WORLD_STEP_ADD => {
            step(op, io)
        }
        EE_UNITS_WORLD_COMPLEX => h(O::PwComplexOfId, 0, &[io.a]),
        EE_UNITS_WORLD_FORESTS => {
            let n = h(O::PwForestCount, 0, &[]) as usize;
            let all: Vec<P> = (0..n).map(|i| hp(O::PwForestAt, 0, &[i as i64])).collect();
            fill_all(io, &all)
        }
        EE_UNITS_WORLD_FOREST_RENEWAL => h(O::PwForestRenewal, u, &[]),
        EE_UNITS_WORLD_FOREST_TILES => {
            let points = hp(O::PwPointsNew, 0, &[]);
            h(O::PwForestTiles, u, &[pa(points)]);
            let n = h(O::PwPointsCount, points, &[]) as usize;
            for t in 0..n {
                let x = h(O::PwPointX, points, &[t as i64]) as c_long;
                let y = h(O::PwPointY, points, &[t as i64]) as c_long;
                fill(io, t, (x as u32 as u64) | ((y as u32 as u64) << 32));
            }
            h(O::PwPointsDelete, points, &[]);
            n as i64
        }
        EE_UNITS_WORLD_FOREST_AT => h(O::PwForestAtTile, 0, &[io.a, io.b]),
        EE_UNITS_WORLD_FOREST_SET_RENEWAL => {
            h(O::PwForestSetRenewal, u, &[io.b]);
            0
        }
        EE_UNITS_WORLD_FOREST_SIZE => h(O::PwForestSize, u, &[]),
        // the computer players' schedule is ee_sim's (AI round 8: ee_sim::ai next_schedule.rs)
        EE_UNITS_WORLD_AI_SCHEDULE => {
            // SAFETY: io's two members are writable
            unsafe { schedule::ee_ai_schedule_get(&mut io.a, &mut io.b) }
        }
        EE_UNITS_WORLD_AI_SET_SCHEDULE => schedule::ee_ai_schedule_set(io.a, io.b),
        EE_UNITS_WORLD_TILE_COUNT => {
            let map = hp(O::PwMap, 0, &[]);
            if map != 0 {
                let size = h(O::PwMapSize, map, &[]);
                size * size
            } else {
                0
            }
        }
        EE_UNITS_WORLD_TILE_UNITS | EE_UNITS_WORLD_TILE_SET => {
            let tile = hp(O::PwMapTileAt, hp(O::PwMap, 0, &[]), &[io.a]);
            let mut n: usize = 0;
            let mut node = if tile != 0 { hp(O::TileUnits, tile, &[]) } else { 0 };
            while node != 0 {
                if op == EE_UNITS_WORLD_TILE_UNITS {
                    fill(io, n, hp(O::PwNodeUnit, node, &[]) as u64);
                } else if n as i64 == io.b {
                    // SAFETY: the list holds the unit
                    h(O::PwNodeSetUnit, node, &[unsafe { *io.list } as i64]);
                }
                node = hp(O::PwNodeNext, node, &[]);
                n += 1;
            }
            n as i64
        }
        EE_UNITS_WORLD_REGION_ORDER | EE_UNITS_WORLD_REGION_RESTORE => {
            let map = hp(O::PwMap, 0, &[]);
            let table = if map != 0 { hp(O::PwMapRegions, map, &[]) } else { 0 } as *mut EeCombatUnitRegions;
            if op == EE_UNITS_WORLD_REGION_RESTORE {
                // SAFETY: the list holds `cap` ids
                let ids: Vec<u32> = (0..io.cap).map(|i| unsafe { *io.list.add(i) } as u32).collect();
                // SAFETY: the map's table (or null)
                return unsafe { ee_combat_regions_restore(table, io.a as u32, ids.as_ptr(), ids.len()) } as i64;
            }
            // SAFETY: as above
            let n = unsafe { ee_combat_regions_order(table, std::ptr::null_mut(), 0) };
            let mut order = vec![0u32; n];
            // SAFETY: as above
            unsafe { ee_combat_regions_order(table, order.as_mut_ptr(), order.len()) };
            for (k, &v) in order.iter().enumerate() {
                fill(io, k, v as u64);
            }
            order.len() as i64
        }
        EE_UNITS_WORLD_GHOSTS => {
            let (dead, simple) = gather_ghosts();
            io.a = dead.len() as i64;
            let all: Vec<P> = dead.iter().chain(simple.iter()).copied().collect();
            fill_all(io, &all)
        }
        EE_UNITS_WORLD_GHOST_FIELDS => {
            ghost_fields(u, io);
            0
        }
        EE_UNITS_WORLD_DEREFERENCE => {
            h(O::PwUnitDereference, u, &[]);
            0
        }
        EE_UNITS_WORLD_PLAYER_COUNT => num_players() as i64,
        EE_UNITS_WORLD_PLAYER_UNITS
        | EE_UNITS_WORLD_PLAYER_UNIT
        | EE_UNITS_WORLD_PLAYER_SET_LIST
        | EE_UNITS_WORLD_PLAYER_SET_BUCKET => player_op(op, io),
        _ => 0,
    }
}

fn step(op: i32, io: &mut EeUnitsWorldIo) -> i64 {
    // the active unit lists (ee_sim app active.rs: 0 stepped, 1 activation, 2 deactivation) as units
    let which = io.a as u32;
    let cx = io.b as P;
    if op == EE_UNITS_WORLD_STEP_LIST {
        // SAFETY: the lists' copies out
        let n = unsafe { ee_app_active_list(which, std::ptr::null_mut(), 0) };
        let mut ids = vec![0u32; n];
        // SAFETY: as above
        unsafe { ee_app_active_list(which, ids.as_mut_ptr(), ids.len()) };
        for (i, &id) in ids.iter().enumerate() {
            let unit = h(O::PwComplexOfSavedId, 0, &[id as i64]);
            fill(io, i, unit as u64);
        }
        return ids.len() as i64;
    }
    if op == EE_UNITS_WORLD_STEP_SET {
        // the references UpdateActiveUnits keeps: the ops below
        // SAFETY: the list holds `cap` units
        let ids: Vec<u32> =
            (0..io.cap).map(|i| h(O::PwHandleId, unsafe { *io.list.add(i) } as P, &[]) as u32).collect();
        // SAFETY: the ids' copy in
        unsafe { ee_app_active_set_list(which, ids.as_ptr(), ids.len()) };
    } else if op == EE_UNITS_WORLD_STEP_DROP {
        h(O::PwUnitDereference, cx, &[]);
        if which == 0 {
            h(O::PwUnitDereference, cx, &[]);
            ee_app_active_set(unit_id(cx) as u32, false);
        }
    } else {
        h(O::PwUnitReference, cx, &[]);
        if which == 0 {
            h(O::PwUnitReference, cx, &[]);
            ee_app_active_set(unit_id(cx) as u32, true);
        }
    }
    0
}

fn player_op(op: i32, io: &mut EeUnitsWorldIo) -> i64 {
    let pl = player(io.a as u64);
    if op == EE_UNITS_WORLD_PLAYER_UNIT {
        return if pl != 0 { h(O::PwPlayerGetUnit, pl, &[io.b]) } else { 0 };
    }
    if op == EE_UNITS_WORLD_PLAYER_SET_BUCKET {
        // SAFETY: the list holds the unit
        h(O::PwPlayerSetBucketFirst, pl, &[io.b, unsafe { *io.list } as i64]);
        return 0;
    }
    if op == EE_UNITS_WORLD_PLAYER_SET_LIST {
        // SAFETY: the list holds `cap` units
        let order: Vec<P> = (0..io.cap).map(|i| unsafe { *io.list.add(i) } as P).collect();
        let n = order.len();
        for i in 0..n {
            h(O::PwUnitSetPrevious, order[i], &[if i > 0 { pa(order[i - 1]) } else { 0 }]);
            h(O::PwUnitSetNext, order[i], &[if i + 1 < n { pa(order[i + 1]) } else { 0 }]);
        }
        h(O::PwPlayerSetFirstUnit, pl, &[if n == 0 { 0 } else { pa(order[0]) }]);
        h(O::PwPlayerSetLastUnit, pl, &[if n == 0 { 0 } else { pa(order[n - 1]) }]);
        return 0;
    }
    let mut n = 0;
    let mut owned = if pl != 0 { hp(O::PwPlayerFirstUnit, pl, &[]) } else { 0 };
    while owned != 0 {
        fill(io, n, owned as u64);
        n += 1;
        owned = hp(O::PwUnitNext, owned, &[]);
    }
    n as i64
}

/// 0.272/0.275: a unit a goal holds out of the world, once (simple units too: a gathered tree or mine gone).
fn add_held_ghost(u: P, complex: &mut Vec<P>, simple: &mut Vec<P>) {
    if u == 0 || world_unit(unit_id(u)) == u {
        return;
    }
    let cx = hp(O::PwDynComplex, u, &[]); // (a removed unit's store fields read 0)
    if cx != 0 {
        if !complex.contains(&cx) {
            complex.push(cx);
        }
    } else if !simple.contains(&u) {
        simple.push(u);
    }
}

/// 0.262: the dead attackers the units hold, then (0.272, 0.275) the goals' resources, drop sites and attack targets
/// out of the world: the complex ones first, then the simple ones.
fn gather_ghosts() -> (Vec<P>, Vec<P>) {
    let mut dead: Vec<P> = Vec::new();
    let mut simple: Vec<P> = Vec::new();
    let players = num_players();
    for p in 0..players as u64 {
        let pl = player(p);
        let mut u = if pl != 0 { hp(O::PwPlayerFirstUnit, pl, &[]) } else { 0 };
        while u != 0 {
            let cx = hp(O::UnComplex, u, &[]);
            let attacker = if cx != 0 { hp(O::PwCxLastAttacker, cx, &[]) } else { 0 };
            if !(attacker == 0 || world_unit(unit_id(attacker)) == attacker || dead.contains(&attacker)) {
                dead.push(attacker);
            }
            u = hp(O::PwUnitNext, u, &[]);
        }
    }
    for p in 0..players as u64 {
        let pl = player(p);
        let mut u = if pl != 0 { hp(O::PwPlayerFirstUnit, pl, &[]) } else { 0 };
        while u != 0 {
            let cx = hp(O::UnComplex, u, &[]);
            let queues = if cx != 0 { h(O::PwCxGoalQueueCount, cx, &[]) } else { 0 };
            for q in 0..queues {
                let goals = h(O::PwCxGoalCount, cx, &[q]);
                for g in 0..goals {
                    let goal = hp(O::PwCxGoalAt, cx, &[q, g]);
                    let attack = hp(O::PwAttackUnitCast, goal, &[]);
                    if attack != 0 {
                        add_held_ghost(hp(O::PwAttackTarget, attack, &[]), &mut dead, &mut simple);
                    } else {
                        let gather = hp(O::PwGatherGoalCast, goal, &[]);
                        if gather != 0 {
                            add_held_ghost(hp(O::PwGatherResource, gather, &[]), &mut dead, &mut simple);
                            add_held_ghost(hp(O::PwGatherStorage, gather, &[]), &mut dead, &mut simple);
                        }
                    }
                }
            }
            u = hp(O::PwUnitNext, u, &[]);
        }
    }
    (dead, simple)
}

fn ghost_fields(u: P, io: &mut EeUnitsWorldIo) {
    let r = *rts(u);
    io.v[0] = r.mUnitID as i64;
    let class = hp(O::UnClass, u, &[]);
    // SAFETY: the class's record
    io.v[1] =
        if class != 0 { unsafe { (*rec::<EeUnitsUnitClassFields>(O::UcFields, class)).mUnitType as i64 } } else { -1 };
    let pl = hp(O::UnPlayer, u, &[]);
    // SAFETY: the player's record
    io.v[2] = if pl != 0 { unsafe { (*rec::<EeUnitsRtsPlayerFields>(O::PlRts, pl)).mPlayerID as i64 } } else { -1 };
    let f = *unf(u);
    io.v[3] = f.mHitPoints as i64;
    io.v[4] = r.mTileX as i64;
    io.v[5] = r.mTileY as i64;
    io.v[7] = f.mResourceType as i64;
    io.a = f.mResourceAmount as i64;
    if io.b != 0 {
        // a complex unit: built flag and placement
        io.v[6] = cxf(u).mIsBuilt as i64;
        h(O::PwCxGetPlacement, u, &[io.m.as_mut_ptr() as usize as i64]);
    }
}

extern "C" fn ghost_unit_id(unit: *mut c_void) -> c_ulong {
    unit_id(unit as P)
}

/// `EEFindLoadedGhostUnit` (save 0.262 on): the unit made for the ghost of id `id` (made at the first ask, held by
/// world_save.rs until the first tick), or null.
#[no_mangle]
pub extern "C" fn ee_gamecpp_find_loaded_ghost_unit(id: c_ulong) -> *mut c_void {
    // SAFETY: world_save.rs's made units, asked by their id
    let made = unsafe { ee_units_world_ghost_made_find(id, ghost_unit_id) };
    if !made.is_null() {
        return made;
    }
    let mut g = EeUnitsWorldGhost::default();
    // SAFETY: the record is written whole
    if !unsafe { ee_units_world_ghost_find(id, &mut g) } {
        return std::ptr::null_mut();
    }
    let class = if (g.unit_type as i64) < h(O::PwUnitClassCount, 0, &[]) {
        hp(O::PwUnitClassAt, 0, &[g.unit_type as i64])
    } else {
        0
    };
    let pl = if g.player < num_players() { player(g.player as u64) } else { 0 };
    if class == 0 || pl == 0 {
        return std::ptr::null_mut();
    }
    // one reference, world_save.rs's until the first tick
    let unit = if g.is_complex {
        let cx = hp(O::PwComplexCreate, 0, &[]);
        h(O::PwCxInitUnit, cx, &[pa(class), pa(pl)]);
        cxf(cx).mIsBuilt = g.is_built;
        let mut placement = GeTransformation { m: g.placement };
        h(O::PwCxSetPlacement, cx, &[ptr(&mut placement)]);
        cx
    } else {
        // 0.275: a simple unit, off the map as the load makes one (EEPlayer's Load: tile -10000)
        let u = hp(O::PwUnitCreate, 0, &[]);
        h(O::PwUnitInit, u, &[pa(class), -10000, -10000, pa(pl)]);
        u
    };
    rts(unit).mUnitID = g.id;
    unf(unit).mHitPoints = g.hit_points;
    rts(unit).mTileX = g.tile_x;
    rts(unit).mTileY = g.tile_y;
    // a used-up simple resource keeps its tile pointer, off the tile's lists (the gather goal's ResourceAccessible
    // reads its terrain), as MoveTo sets it without adding the unit to the tile
    if !g.is_complex && hp(O::PwMap, 0, &[]) != 0 {
        let tile = hp(O::MapTile, 0, &[g.tile_x as i64, g.tile_y as i64]);
        h(O::PwUnitSetTile, unit, &[pa(tile)]);
    }
    if g.resource_type != c_ulong::MAX {
        // 0.272
        unf(unit).mResourceType = g.resource_type;
        unf(unit).mResourceAmount = g.resource_amount;
    }
    rts(unit).mIsValid = false; // out of the world, as the dead unit was (FreeNode asserts it)
    h(O::PwRegisterUnit, unit, &[]); // the running game's handles still resolve a dead unit's id until its last reference
    ee_units_world_ghost_made(unit as *mut c_void);
    unit as *mut c_void
}

/// The ids of a player's tech tree nodes (`GetNodeIDs`), with the tree (0 for none).
fn tree_ids(pl: P) -> (P, Vec<c_long>) {
    let tree = if pl != 0 { crate::tech_tree::objects::player_tree(pl) } else { 0 };
    let ids = Longs::new();
    if tree != 0 {
        h(O::PwTreeNodeIds, tree, &[pa(ids.0)]);
    }
    (tree, ids.to_vec())
}

/// 0.282 (EESaveHasTechAllowed): per player, the sorted ids of its tech nodes that are not allowed (EETechTreeNode::
/// mIsAllowed false); they go back at the first tick, after the load's own passes.
fn save_tech_allowed(file: Pf) -> i64 {
    let players = num_players();
    let mut r = file.write_ulong(players);
    let mut p = 0;
    while p < players && r == PF_OK {
        let (tree, ids) = tree_ids(player(p as u64));
        let mut off: Vec<c_long> = Vec::new();
        for &id in &ids {
            let node = crate::tech_tree::objects::get(tree, id as std::ffi::c_long);
            if node != 0 && !crate::tech_tree::objects::allowed(node) {
                off.push(id);
            }
        }
        off.sort();
        r = file.write_ulong(off.len() as c_ulong);
        let mut k = 0;
        while k < off.len() && r == PF_OK {
            r = file.write_long(off[k]);
            k += 1;
        }
        p += 1;
    }
    r
}

extern "C" fn apply_tech_allowed(data: usize) {
    // SAFETY: the box load_tech_allowed handed over, run once
    let lists = unsafe { Box::from_raw(data as *mut Vec<Vec<c_long>>) };
    let players = num_players() as usize;
    for (p, off) in lists.iter().enumerate().take(players) {
        let (tree, ids) = tree_ids(player(p as u64));
        if tree == 0 {
            continue;
        }
        for &id in &ids {
            let node = crate::tech_tree::objects::get(tree, id as std::ffi::c_long);
            if node != 0 {
                crate::tech_tree::objects::set_allowed(node, off.binary_search(&id).is_err());
            }
        }
    }
}

fn load_tech_allowed(file: Pf) -> i64 {
    let mut lists: Vec<Vec<c_long>> = Vec::new();
    let mut players: c_ulong = 0;
    let mut r = file.read_ulong(&mut players);
    let mut p = 0;
    while p < players && r == PF_OK {
        let mut count: c_ulong = 0;
        r = file.read_ulong(&mut count);
        let mut list = Vec::new();
        let mut k = 0;
        while k < count && r == PF_OK {
            let mut id: c_long = 0;
            r = file.read_long(&mut id);
            list.push(id);
            k += 1;
        }
        list.sort();
        lists.push(list);
        p += 1;
    }
    if r != PF_OK {
        return r;
    }
    after_load(apply_tech_allowed, Box::into_raw(Box::new(lists)) as usize);
    PF_OK
}

/// 0.283 (EESaveHasTileTypes): the saved terrain types back on the map's tiles (row by row; -1 leaves a tile).
fn apply_tile_types(types: &[c_long]) {
    let map = hp(O::PwMap, 0, &[]);
    if map == 0 {
        return;
    }
    let size = h(O::PwMapSize, map, &[]) as c_long;
    if types.len() as c_long != size.wrapping_mul(size) {
        return;
    }
    for y in 0..size {
        for x in 0..size {
            let tile = hp(O::MapTile, 0, &[x as i64, y as i64]);
            let t = types[(y * size + x) as usize];
            if tile != 0 && t >= 0 {
                h(O::PwTileSetTerrainType, tile, &[t as i64]);
            }
        }
    }
}

/// 0.283: every map tile's terrain type, row by row (y outer, x inner), as runs of a count and a type (-1: no tile).
fn save_tile_types(file: Pf) -> i64 {
    let map = hp(O::PwMap, 0, &[]);
    let size: c_long = if map != 0 { h(O::PwMapSize, map, &[]) as c_long } else { 0 };
    let mut r = file.write_long(size);
    let mut run: c_ulong = 0;
    let mut run_type: c_long = -1;
    let mut i: c_long = 0;
    while i < size * size && r == PF_OK {
        let tile = hp(O::MapTile, 0, &[(i % size) as i64, (i / size) as i64]);
        let t: c_long = if tile != 0 { h(O::TileTerrainType, tile, &[]) as c_long } else { -1 };
        if run != 0 && t != run_type {
            r = file.write_ulong(run);
            if r == PF_OK {
                r = file.write_long(run_type);
            }
            run = 0;
        }
        run_type = t;
        run += 1;
        i += 1;
    }
    if run != 0 && r == PF_OK {
        r = file.write_ulong(run);
        if r == PF_OK {
            r = file.write_long(run_type);
        }
    }
    r
}

extern "C" fn apply_tile_types_at_first_tick(data: usize) {
    // SAFETY: the box load_tile_types handed over, run once
    let types = unsafe { Box::from_raw(data as *mut Vec<c_long>) };
    apply_tile_types(&types);
}

fn load_tile_types(file: Pf) -> i64 {
    let mut size: c_long = 0;
    let mut r = file.read_long(&mut size);
    if r != PF_OK {
        return r;
    }
    if !(0..=0x4000).contains(&size) {
        return PF_BEYOND_END;
    }
    let total = (size * size) as usize;
    let mut types: Vec<c_long> = Vec::with_capacity(total);
    while types.len() < total && r == PF_OK {
        let mut run: c_ulong = 0;
        let mut t: c_long = -1;
        r = file.read_ulong(&mut run);
        if r == PF_OK {
            r = file.read_long(&mut t);
        }
        if r == PF_OK && (run == 0 || run as usize > total - types.len()) {
            r = PF_BEYOND_END;
        }
        if r == PF_OK {
            types.extend(std::iter::repeat_n(t, run as usize));
        }
    }
    if r != PF_OK {
        return r;
    }
    apply_tile_types(&types);
    after_load(apply_tile_types_at_first_tick, Box::into_raw(Box::new(types)) as usize);
    PF_OK
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_helper_fields_mirror_the_class() {
        // +0x1c..+0x3b on the 32-bit original: 0x20 bytes from mTimeOfDay
        let l = std::mem::size_of::<c_ulong>();
        let f = EePWorldHelperFields::default();
        let base = &f as *const _ as usize;
        assert_eq!(&f.mUsingCarbon as *const _ as usize - base, if l == 4 { 0x1f } else { 0x2f });
        assert_eq!(&f.mHerdIDCounter as *const _ as usize - base, if l == 4 { 0x18 } else { 0x28 });
    }

    #[test]
    fn the_primitives_are_player_saves() {
        assert!(is_primitive(1) && is_primitive(8) && is_primitive(98) && is_primitive(99));
        assert!(!is_primitive(9) && !is_primitive(20) && !is_primitive(96));
    }

    #[test]
    fn every_predicate_and_part_has_a_case() {
        for w in 1..=16 {
            assert!(has_op(w).is_some(), "{w}");
        }
        assert!(has_op(17).is_none());
        for p in 1..=9 {
            let ours = p == EE_UNITS_WORLD_PART_AREA_EFFECTS || p == EE_UNITS_WORLD_PART_TOWNS;
            assert!(part_ops(p).is_some() == ours, "{p}");
        }
        assert!(
            part_ops(EE_UNITS_WORLD_PART_TECH_ALLOWED).is_none() && part_ops(EE_UNITS_WORLD_PART_TILE_TYPES).is_none()
        );
    }
}
