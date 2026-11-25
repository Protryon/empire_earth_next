//! The game world's own bodies (was EEWorld.cpp, 0x137ba3..0x13e967): the game's setup from a game state
//! (`StartGame`, `CreatePlayers`, `AddComputerPlayers`), its tear-down (`Clear`, `RemoveAllUnits`, `Reset`), the
//! replacement of a player, the queued placements and the x-sorted unit list (`PlaceQueuedUnits`, `ResortUnit`), the
//! active lists' queues, the resource users, the diplomacy and slot moves, the shared media, the time of day. Each
//! C++ call or member is a case of EEGameOps.cpp ([`super::EeGameCpp`]); EEWorld's methods forward here.
//!
//! The order of every call is the C++'s: the sight scope of the revealed map (TSGT), the load progress, the locks.

// the C++'s own casts (`long` and `unsigned long` are 32-bit here, 64-bit on LP64), literals, products and
// loops, kept as written
#![allow(
    clippy::unnecessary_cast,
    clippy::excessive_precision,
    clippy::approx_constant,
    clippy::neg_multiply,
    clippy::assign_op_pattern,
    clippy::needless_range_loop,
    clippy::too_many_arguments
)]
// the C++'s own tests, kept as written
#![allow(clippy::neg_cmp_op_on_partial_ord, clippy::bool_comparison, clippy::nonminimal_bool)]

use std::ffi::{c_long, c_ulong, c_void};

use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{Ge3DPoint, GeTransformation, GeVector};
use ee_sim::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved, EeUnitsRtsPlayerFields};
use ee_sim::world::unit_class_fields::EeUnitsUnitClassFields;
use ee_sim::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};

use crate::app::game_options::EeAppOptionsFields;

use ee_sim::units::{player_body as pb, player_rest as pr};

use super::{h, hd, hf, hp, pa, rec, O, P};

/// The per-player media the world loads and unloads, in the C++'s order: the foundations 1x1, 2x2, 3x3, 6x6 and
/// the airport's, the citizen and the female citizen.
const FOUNDATION_MEDIA: [i64; 7] = [0x136, 0x132, 0x133, 0x168, 0x2cd, 0x20, 0x1e0];
/// The shared effects LoadMedia loads, in its order (UnloadMedia's order is [`UNLOAD_EFFECTS`]).
const LOAD_EFFECTS: [i64; 8] = [0xb8, 0xb9, 0xba, 0xbc, 0x56, 0x60, 0x62, 0xd2];
const UNLOAD_EFFECTS: [i64; 8] = [0xb8, 0xb9, 0xba, 0xbc, 0x60, 0x62, 0xd2, 0x56];
/// `kSoundButton1`, `kSoundButton2`.
const BUTTON_SOUNDS: [i64; 2] = [0x207, 0x208];
/// `kUnitTypePathPoint`, `kUnitTypeTownCenter`.
const UNIT_TYPE_PATH_POINT: c_ulong = 0xdf;
const UNIT_TYPE_TOWN_CENTER: c_ulong = 0x23;
/// `kSlotNone`.
const SLOT_NONE: u8 = 0xff;
/// The load progress steps (a unit-local .rdata block of the original).
const PROGRESS: [f32; 14] = [
    0.53333336, 0.56666666, 0.60000002, 0.63333333, 0.66666669, 0.69999999, 0.73333335, 0.76666665, 0.80000001,
    0.83333331, 0.86666667, 0.89999998, 0.93333334, 0.96666664,
];

// ---- the records

fn rts<'a>(u: P) -> &'a mut EeUnitsRtsUnitFields {
    // SAFETY: a live unit's record, used before the next C++ call
    unsafe { &mut *rec(O::UnRts, u) }
}

fn uf<'a>(u: P) -> &'a mut EeUnitsUnitFields {
    // SAFETY: as rts
    unsafe { &mut *rec(O::UnFields, u) }
}

fn cf<'a>(cx: P) -> &'a mut EeUnitsComplexUnitFields {
    // SAFETY: as rts
    unsafe { &mut *rec(O::CxFields, cx) }
}

fn ucf<'a>(uc: P) -> &'a EeUnitsUnitClassFields {
    // SAFETY: as rts
    unsafe { &*rec(O::UcFields, uc) }
}

fn prts<'a>(pl: P) -> &'a mut EeUnitsRtsPlayerFields {
    // SAFETY: a live player's record, as rts
    unsafe { &mut *rec(O::PlRts, pl) }
}

fn psaved<'a>(pl: P) -> &'a mut EeUnitsPlayerSaved {
    // SAFETY: as prts
    unsafe { &mut *rec(O::PlSaved, pl) }
}

fn pfields<'a>(pl: P) -> &'a mut EeUnitsPlayerFields {
    // SAFETY: as prts
    unsafe { &mut *rec(O::PlFields, pl) }
}

/// The world's game options (`mGameOptions`), or the lobby's (`gGameOptions`).
fn options<'a>(w: P) -> &'a mut EeAppOptionsFields {
    // SAFETY: the world's member
    unsafe { &mut *rec(O::WOptions, w) }
}

fn lobby_options<'a>() -> &'a mut EeAppOptionsFields {
    // SAFETY: the global
    unsafe { &mut *rec(O::LobbyOptions, 0) }
}

fn state_options<'a>(st: P) -> &'a mut EeAppOptionsFields {
    // SAFETY: the game state's member
    unsafe { &mut *rec(O::StOptions, st) }
}

/// A plain member of the world (`op` answers its address).
fn wm<'a, T>(op: O, w: P) -> &'a mut T {
    // SAFETY: the op answers the address of the world's member of type T
    unsafe { &mut *rec(op, w) }
}

/// A plain member of the game state.
fn sm<'a, T>(op: O, st: P) -> &'a mut T {
    // SAFETY: as wm
    unsafe { &mut *rec(op, st) }
}

fn num_players(w: P) -> c_ulong {
    *wm::<c_ulong>(O::WNumPlayers, w)
}

fn player(w: P, i: c_ulong) -> P {
    hp(O::WPlayer, w, &[i as i64])
}

fn current(w: P) -> c_long {
    *wm::<c_long>(O::WCurrentPlayer, w)
}

fn media_loaded(w: P) -> bool {
    *wm::<bool>(O::WMediaLoaded, w)
}

fn map(w: P) -> P {
    hp(O::WMap, w, &[])
}

fn complex(u: P) -> P {
    hp(O::UnComplex, u, &[])
}

fn unit_class(u: P) -> P {
    hp(O::UnClass, u, &[])
}

fn next_unit(u: P) -> P {
    hp(O::UnNext, u, &[])
}

fn tile_next(u: P) -> P {
    hp(O::UnTileNext, u, &[])
}

fn tile_prev(u: P) -> P {
    hp(O::UnTilePrev, u, &[])
}

/// EEPlayer's member `what` of player_rest.rs (the C++ member forwarded there).
fn player_rest(pl: P, what: u32, a: u64, b: i64, c: i64) -> i64 {
    // SAFETY: a live player of the world, on the thread that owns it
    unsafe { ee_sim::units::player_rest::game::ee_units_player_rest(pl as *mut c_void, what, a, b, c) }
}

/// EEPlayer's argument-less member `what` of player_body.rs.
fn player_body(pl: P, what: u32) {
    // SAFETY: as player_rest
    unsafe { ee_sim::units::player_body::game::ee_units_player_body(pl as *mut c_void, what, 0, 0, 0, 0.0) };
}

/// The RTS world's and player's members (rts world.rs, units.rs; the tests run no host).
#[cfg(not(test))]
mod rts {
    use super::{c_void, P};
    use crate::rts::{units::ffi as units, world::ffi as world};

    /// `RTSWorld::FlushQueuedPlacements`.
    pub fn flush_queued(w: P) {
        // SAFETY: the live world, on the thread that owns it
        unsafe { world::ee_rts_world_flush_queued_placements(w as *mut c_void) };
    }

    /// `RTSWorld::AddUnitEntry(unit, NULL)`.
    pub fn add_unit_entry(w: P, u: P) {
        // SAFETY: the live world and unit
        unsafe { world::ee_rts_world_add_unit_entry(w as *mut c_void, u as *mut c_void, std::ptr::null_mut()) };
    }

    /// `RTSPlayer::KillUnit(unit, false)`.
    pub fn kill_unit(pl: P, u: P) {
        units::ee_rts_player_kill_unit(pl as *mut c_void, u as *mut c_void, false);
    }

    /// `RTSPlayer::RemoveKilledUnits`.
    pub fn remove_killed(pl: P) {
        units::ee_rts_player_remove_killed_units(pl as *mut c_void);
    }
}

#[cfg(test)]
mod rts {
    use super::P;
    pub fn flush_queued(_: P) {
        panic!("no host in the tests")
    }
    pub fn add_unit_entry(_: P, _: P) {
        panic!("no host in the tests")
    }
    pub fn kill_unit(_: P, _: P) {
        panic!("no host in the tests")
    }
    pub fn remove_killed(_: P) {
        panic!("no host in the tests")
    }
}

use rts::{add_unit_entry as rts_add_unit_entry, kill_unit as rts_kill_unit};
pub(crate) use rts::{flush_queued as rts_flush_queued, remove_killed as rts_remove_killed};

/// EEWorld::RemovePlayerFromTeams.
fn remove_from_teams(w: P, id: c_ulong) {
    // SAFETY: the live world
    unsafe { super::teams::ee_gamecpp_world_teams_remove(w as *mut c_void, id as u32) };
}

fn progress(step: usize) {
    crate::screens::ui_objects::ui_set_load_progress(PROGRESS[step - 16]);
}

fn load_foundations(pl: P) {
    for t in FOUNDATION_MEDIA {
        h(O::PlUpgradeLoadMedia, pl, &[t, 0x7f]);
    }
}

fn unload_foundations(pl: P) {
    for t in FOUNDATION_MEDIA {
        h(O::PlUpgradeUnloadMedia, pl, &[t]);
    }
}

/// A player's units killed and removed (the old player of ReplacePlayer, RemoveAllUnits').
fn kill_units(pl: P) {
    let mut u = hp(O::PlFirstUnit, pl, &[]);
    while u != 0 {
        let next = next_unit(u);
        let cx = complex(u);
        if cx != 0 {
            h(O::CxClearCollisionState, cx, &[]);
        }
        h(O::UnKill, u, &[0, 0, 0]);
        rts_kill_unit(pl, u);
        u = next;
    }
    rts_remove_killed(pl);
    h(O::PlReleasePending, pl, &[]);
}

/// The queued placements' units dereferenced and the queue emptied.
fn drop_queued() {
    for (u, _) in crate::rts::world::take_queued() {
        h(O::RefUnitDereference, u as P, &[]);
    }
}

/// The player's units' media and, for a built land building, its class's buttons.
fn load_unit_media(pl: P, button_player_of_unit: bool) {
    let mut u = hp(O::PlFirstUnit, pl, &[]);
    while u != 0 {
        h(O::UnLoadMedia, u, &[h(O::UnIsFoundation, u, &[])]);
        let class = unit_class(u);
        if h(O::UcIsLandBuilding, class, &[]) != 0 && cf(complex(u)).mIsBuilt == true {
            let owner = if button_player_of_unit { hp(O::UnPlayer, u, &[]) } else { pl };
            h(O::PlLoadButtons, owner, &[ucf(unit_class(u)).mUnitType as i64]);
        }
        u = next_unit(u);
    }
}

// ---- EEWorld

/// The ctor's members (the world layout registered first; the ghost lock made; the statistics reset).
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_init(w: *mut c_void) {
    let w = w as P;
    h(O::WorldLayoutRegister, 0, &[]);
    *wm::<f32>(O::WWindYaw, w) = 0.0;
    h(O::WSetEffectManagerRaw, w, &[0]);
    *wm::<c_long>(O::WUB80, w) = 0;
    h(O::WSetEnvironmentRaw, w, &[0]);
    h(O::WSetCalamityHelperRaw, w, &[0]);
    ee_gamecpp_world_set_time_of_day(w as *mut std::ffi::c_void, 0.35);
    h(O::WCreateGhostLock, w, &[]);
    for i in 0..10 {
        wm::<[c_long; 10]>(O::WUB30, w)[i] = 1;
    }
    *wm::<f32>(O::WSavedGameSpeed, w) = 0.0;
    *wm::<c_long>(O::WUB58, w) = 2;
    *wm::<c_long>(O::WCurrentPlayer, w) = 0;
    *wm::<c_ulong>(O::WWondersToWin, w) = 1;
    h(O::WSetInfluenceRaw, w, &[0]);
    h(O::WSetScenarioRaw, w, &[0]);
    *wm::<bool>(O::WGameScreenActive, w) = false;
    *wm::<c_long>(O::WMaxPlayers, w) = 3;
    *wm::<bool>(O::WLoadingSavedGame, w) = false;
    *wm::<bool>(O::WUnitImprovementsAllowed, w) = true;
    *wm::<bool>(O::WCheatsEnabled, w) = true;
    *wm::<bool>(O::WTeamsLocked, w) = false;
    *wm::<c_long>(O::WTownIDCounter, w) = 0;
    *wm::<u16>(O::WHerdIDCounter, w) = 0;
    *wm::<bool>(O::WClearing, w) = false;
    *wm::<c_long>(O::WStartTime, w) = 0;
    *wm::<u16>(O::WLagStatus, w) = 0;
    *wm::<bool>(O::WLoadingPlayerMedia, w) = false;
    *wm::<bool>(O::WUserPaused, w) = false;
    *wm::<bool>(O::WUsingCarbon, w) = false;
    *wm::<bool>(O::WInCinematic, w) = false;
    *wm::<bool>(O::WShowCollisionSpheres, w) = false;
    *wm::<bool>(O::WBB9D, w) = true;
    *wm::<bool>(O::WMediaLoaded, w) = false;
    *wm::<c_long>(O::WCollisionFlags, w) = 15;
    *wm::<bool>(O::WExecutingLocalSelection, w) = false;
    super::stats_history::with(|s| s.reset());
}

/// EEWorld::~EEWorld: the ghost lock freed; the influence table, effect manager, ghost units, scenario, environment
/// and calamity managers deleted, in the C++'s order (the members' own dtors and RTSWorld's follow in C++).
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_destroy(w: *mut c_void) {
    let w = w as P;
    h(O::C13bGhostLockFree, w, &[]);
    h(O::WDeleteInfluence, w, &[]);
    h(O::WSetEffectManager, w, &[0]);
    for i in 0..ee_sim::app::ghosts::count() {
        h(O::GhostDelete, ee_sim::app::ghosts::at(i), &[]);
    }
    while !ee_sim::app::ghosts::ee_app_ghost_pop().is_null() {}
    h(O::WDeleteScenario, w, &[]);
    h(O::WDeleteEnvironment, w, &[]);
    h(O::WDeleteCalamityHelper, w, &[]);
}

/// EEWorld::Clear: tears down the current game (the end statistics, the media, every unit, ghost, effect, calamity
/// and manager, the minimap and active lists; campaign games back to the campaign screen).
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_clear(w: *mut c_void) {
    let w = w as P;
    *wm::<bool>(O::WClearing, w) = true;
    if !ee_base::util::system::ee_util_shutdown_in_progress() && map(w) != 0 {
        super::world_stats::ee_gamecpp_world_record_statistics(w);
        h(O::PickerLock, 0, &[]);
        h(O::PickerReset, 0, &[]);
        h(O::PickerUnlock, 0, &[]);
    }
    if h(O::UiManagerPresent, 0, &[]) != 0 && hp(O::UiEditorForm, 0, &[]) != 0 {
        h(O::UiEditorClearSelection, 0, &[]);
    }
    if h(O::WCampaignNameLength, w, &[]) > 0
        && options(w).mCampaignChecksum > 0
        && !*wm::<bool>(O::WLoadingSavedGame, w)
    {
        super::startup::ee_gamecpp_startup_data_paths();
    }
    ee_gamecpp_world_unload_media(w as *mut c_void);
    h(O::WGhostLock, w, &[]);
    ee_gamecpp_world_clear_ghosts(w as *mut c_void);
    h(O::WGhostUnlock, w, &[]);
    let env = hp(O::WEnvironment, w, &[]);
    if env != 0 {
        h(O::EnvSetEnabled, env, &[0]);
        h(O::WDeleteEnvironment, w, &[]);
        h(O::WSetEnvironmentRaw, w, &[0]);
    }
    h(O::PathFreePaths, 0, &[]);
    let effects = hp(O::WEffectManager, w, &[]);
    if effects != 0 {
        h(O::EffClearEffects, effects, &[]);
    }
    ee_gamecpp_world_remove_all_units(w as *mut c_void);
    h(O::AeClearEffects, 0, &[]);
    h(O::WDeleteScenario, w, &[]);
    h(O::WSetScenarioRaw, w, &[0]);
    ee_sim::app::teams_ffi::ee_app_teams_clear();
    h(O::CalamitiesDestroy, 0, &[]);
    let n = num_players(w);
    for i in 1..n {
        let pi = player(w, i);
        if pi == 0 || !((pfields(pi).mHackUnitClass as u64) < h(O::UnitClassCount, 0, &[]) as u64) {
            continue;
        }
        for j in 1..n {
            let pj = player(w, j);
            if pj != 0 {
                let hack = pfields(player(w, i)).mHackUnitClass as i64;
                h(O::PlUnloadClassMedia, pj, &[hack]);
                let hack = pfields(player(w, i)).mHackUnitClass as i64;
                h(O::PlUnloadClassButtons, pj, &[hack]);
            }
        }
        h(O::PlSetHackClassNone, player(w, i), &[]);
    }
    h(O::RtsClear, w, &[]);
    if h(O::UserInterfacePresent, 0, &[]) != 0 {
        h(O::UiPostUpdateBackground, 0, &[]);
    }
    super::startup::ee_gamecpp_startup_stop_sounds();
    h(O::MiniMapDelete, 0, &[]);
    super::globals::set!(gMiniMap, core::ptr::null_mut());
    h(O::PathReset, 0, &[]);
    h(O::SetLocalPlayerOut, 0, &[0]);
    active_clear();
    ee_sim::units::patrol_group::ee_units_pg_reset_ids();
    super::globals::set!(gCollisionDetectionUnit, 2000);
    h(O::WDeleteCalamityHelper, w, &[]);
    h(O::WSetCalamityHelperRaw, w, &[0]);
    h(O::WDeleteInfluence, w, &[]);
    h(O::WSetInfluenceRaw, w, &[0]);
    *wm::<u16>(O::WLagStatus, w) = 0;
    if h(O::SoundManagerPresent, 0, &[]) != 0 {
        h(O::SoundSetMuted, 0, &[0]);
    }
    if h(O::WonNetPresent, 0, &[]) != 0 {
        h(O::WonNetClearGameList, 0, &[]);
    }
    h(O::CivBuilderFree, 0, &[]);
    *wm::<bool>(O::WClearing, w) = false;
}

/// EEWorld::RestartUnits (`on`) and RefreshUnits: every unit's sight on (off), the local player's terrain refreshed.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_sight_all(w: *mut c_void, on: bool) {
    let w = w as P;
    let mut u = hp(O::WFirstUnit, w, &[]);
    while u != 0 {
        h(if on { O::UnSightOnHere } else { O::UnSightOffHere }, u, &[]);
        u = tile_next(u);
    }
    player_body(hp(O::WCurrentPlayerPtr, w, &[]), pb::EE_UNITS_PLAYER_DO_REFRESH_TERRAIN);
}

/// EEWorld::ReplacePlayer: a human player copied from the player by a save and load replaces it; the old player's
/// units killed, its alliances, pending units and media moved to the copy.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_replace_player(
    w: *mut c_void,
    index: c_ulong,
    major: c_ulong,
    minor: c_ulong,
    m: *mut c_void,
) {
    let w = w as P;
    let file = hp(O::MemFileNew, 0, &[1000, 1, 1]);
    let old = player(w, index);
    rts_flush_queued(w);
    let new = hp(O::NewHumanPlayer, 0, &[]);
    h(O::PlSaveTo, old, &[major as i64, minor as i64, pa(file)]);
    h(O::MemFileRewind, file, &[]);
    h(O::PlLoadFrom, new, &[major as i64, minor as i64, pa(file)]);
    kill_units(old);
    drop_queued();
    for i in 0..num_players(w) {
        if old != player(w, i) {
            let mut j: i64 = 0;
            while j < h(O::PlAlliesCount, player(w, i), &[]) {
                if hp(O::PlAllyAt, player(w, i), &[j]) != old {
                    j += 1;
                    continue;
                }
                player_rest(player(w, i), pr::EE_UNITS_PLAYER_REST_REMOVE_ALLY, old as u64, 0, 0);
                h(O::PlAlliesPush, player(w, i), &[pa(new)]);
                j += 1;
            }
        } else {
            let mut j: i64 = 0;
            while j < h(O::PlAlliesCount, player(w, i), &[]) {
                let ally = hp(O::PlAllyAt, old, &[j]);
                h(O::PlAlliesPush, new, &[pa(ally)]);
                j += 1;
            }
        }
    }
    if !media_loaded(w) && prts(old).mPlayerID > 0 {
        unload_foundations(old);
    }
    h(O::WSetPlayer, w, &[index as i64, pa(new)]);
    let tech = hp(O::PlTechnologies, new, &[]);
    h(O::TechSetPlayer, tech, &[pa(new)]);
    let mut k: i64 = 0;
    while k < h(O::PlNumPending, new, &[]) {
        let u = hp(O::PlPendingAt, new, &[k]);
        h(O::RtsUpdateTile, u, &[m as i64]);
        rts_add_unit_entry(w, u);
        k += 1;
    }
    h(O::PlReleasePending, new, &[]);
    h(O::TechApplySaved, tech, &[]);
    if !media_loaded(w) && prts(new).mPlayerID > 0 {
        load_foundations(new);
    }
    load_unit_media(new, true);
    h(O::PlRepairPointers, new, &[major as i64, minor as i64]);
    h(O::MemFileDelete, file, &[]);
}

/// EEWorld::RemoveAllUnits: every player's units (pending ones included) killed and removed, the queued placements
/// dropped, the unit list emptied.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_remove_all_units(w: *mut c_void) {
    let w = w as P;
    let mut was_clear = false;
    if !*wm::<bool>(O::WClearing, w) {
        *wm::<bool>(O::WClearing, w) = true;
        was_clear = true;
    }
    let count = num_players(w);
    for n in 0..count {
        let pl = player(w, n);
        if pl == 0 {
            continue;
        }
        h(O::PlAddPendingUnits, pl, &[0, 1]);
        kill_units(pl);
    }
    drop_queued();
    h(O::WSetFirstUnitRaw, w, &[0]);
    if was_clear {
        *wm::<bool>(O::WClearing, w) = false;
    }
}

/// EEWorld::ClearGhostUnits: every ghost unit deleted, under the ghost lock.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_clear_ghosts(w: *mut c_void) {
    let w = w as P;
    h(O::WGhostLock, w, &[]);
    loop {
        let ghost = ee_sim::app::ghosts::ee_app_ghost_pop() as P;
        if ghost == 0 {
            break;
        }
        h(O::GhostDelete, ghost, &[]);
    }
    h(O::WGhostUnlock, w, &[]);
}

/// EEWorld::SetDiplomaticStance: the stance; the teams joined when the alliance becomes mutual, split when a mutual
/// alliance ends.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_set_stance(w: *mut c_void, id: c_ulong, other: c_long, stance: c_long) {
    let w = w as P;
    let pl = player(w, id);
    let ot = player(w, other as c_ulong);
    let was_allied = psaved(pl).mEnemies[other as usize] == 0;
    h(O::PlSetStance, pl, &[pa(ot), stance as i64, pa(map(w))]);
    if stance == 0 {
        if !was_allied && psaved(ot).mEnemies[id as usize] == 0 {
            unsafe { super::teams::ee_gamecpp_world_teams_ally(w as *mut c_void, id as u32, other as u32) };
        }
    } else if was_allied && psaved(ot).mEnemies[id as usize] == 0 {
        unsafe { super::teams::ee_gamecpp_world_teams_break(w as *mut c_void, id as u32, other as u32) };
    }
}

/// EEWorld::RebuildTeams' stances after the teams: Gaia and every player at war with each other.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_gaia_stances(w: *mut c_void) {
    let w = w as P;
    let gaia = player(w, 0);
    for k in 1..num_players(w) {
        h(O::PlSetStance, gaia, &[pa(player(w, k)), 1, pa(map(w))]);
    }
    for m in 1..num_players(w) {
        if player(w, m) != 0 {
            h(O::PlSetStance, player(w, m), &[pa(player(w, 0)), 1, pa(map(w))]);
        }
    }
}

/// EEWorld::DumpMedia: every loaded medium to the file (the leaks too when asked).
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_dump_media(w: *mut c_void, file: *mut c_void, leaks: bool) {
    let (w, file) = (w as P, file as P);
    for i in 0..num_players(w) {
        if player(w, i) != 0 {
            player_rest(player(w, i), pr::EE_UNITS_PLAYER_REST_DUMP_MEDIA, file as u64, 0, 0);
        }
        h(O::DumpBlankLines, file, &[]);
    }
    h(O::DumpEffectIds, file, &[]);
    h(O::DumpGraphicObjects, file, &[]);
    h(O::DumpSounds, file, &[]);
    h(O::DumpButtons, file, &[]);
    h(O::DumpTerrain, file, &[]);
    h(O::DumpTextures, file, &[]);
    h(O::DumpModels, file, &[]);
    if leaks == true {
        h(O::DumpLeakedUnits, file, &[]);
        h(O::DumpLeakTitle, file, &[]);
    }
}

/// EEWorld::RemovePlayerFromGame: the player out of the game (out, off its teams, the map revealed but for a lost
/// scenario, its end time, any cinematic ended; the local player's minimap redrawn).
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_remove_player(w: *mut c_void, won: bool, id: c_ulong) {
    let w = w as P;
    // TEST ONLY: the headless simulation keeps the idle local human in (platform/PlatSim.h; --no-human-defeat)
    if h(O::SimNoHumanDefeat, 0, &[]) != 0 && !won && id as c_long == current(w) {
        h(O::SimNoHumanDefeatHit, 0, &[]);
        return;
    }
    let pl = player(w, id);
    ee_gamecpp_world_mark_out(w as *mut c_void, id);
    remove_from_teams(w, id);
    if !options(w).mIsScenarioGame || won == true {
        prts(pl).mSeesAll = true;
    }
    h(O::PlSetEndTimeNow, pl, &[]);
    h(O::ScenarioEndCinematic, 0, &[]);
    if pl == player(w, current(w) as c_ulong) {
        super::minimap::ee_gamecpp_minimap_redraw(super::globals::get!(gMiniMap));
        h(O::SetLocalPlayerOut, 0, &[1]);
    }
}

/// EEWorld::AddComputerPlayers: the empty slots after the state's players filled with computer players in unused
/// colours and 999,999 of each resource (the scenario editor), their foundation media loaded.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_add_computers(w: *mut c_void, _st: *mut c_void) {
    let w = w as P;
    let name = hp(O::StrNew, 0, &[]);
    let mut used = [false; 17];
    let mut i: c_ulong = 0;
    while (i as usize) < super::state::players_len() {
        for c in 0..=16usize {
            if h(O::PlColorIsPlayerColor, player(w, i), &[c as i64]) != 0 {
                used[c] = true;
                break;
            }
        }
        i += 1;
    }
    let mut i = super::state::players_len() as c_ulong;
    while i <= 16 {
        if player(w, i) != 0 {
            i += 1;
            continue;
        }
        h(O::StrComputerName, name, &[]);
        let pl = hp(O::NewComputerPlayer, name, &[i as i64]);
        for c in 0..=16usize {
            if used[c] {
                continue;
            }
            h(O::PlSetPlayerColor, pl, &[c as i64]);
            used[c] = true;
            break;
        }
        player_rest(pl, pr::EE_UNITS_PLAYER_REST_SET_ALL_RESOURCES, 0, 999999, 0);
        h(O::WSetPlayer, w, &[i as i64, pa(pl)]);
        if media_loaded(w) || i == 0 {
            i += 1;
            continue;
        }
        load_foundations(pl);
        i += 1;
    }
    h(O::StrFree, name, &[]);
}

/// The teams of the state (StartGame's), or the teams rebuilt.
fn start_teams(w: P) {
    if super::teams::state_to_world() > 0 {
        let mut i = num_players(w);
        while i <= 16 {
            if player(w, i) == 0 {
                remove_from_teams(w, i);
            }
            i += 1;
        }
    } else {
        unsafe { super::teams::ee_gamecpp_world_teams_rebuild(w as *mut c_void, num_players(w) as u32) };
    }
}

/// EEWorld::StartGame: the world set up from a game state (map, players, teams, local player, camera, sight,
/// minimap, calamities, music, media), the load progress reported; in multiplayer the local player found by name.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_start_game(w: *mut c_void, st: *mut c_void, locked: bool) {
    let (w, st) = (w as P, st as P);
    let progress_obj = hp(O::CampaignProgressNew, 0, &[]);
    let mut local: c_ulong = 0;
    let loaded = *sm::<bool>(O::StIsLoadedGame, st);
    let mut moved = false;
    if !locked {
        h(O::WWriteLock, w, &[]);
    }
    super::stats_history::with(|s| s.reset());
    h(O::StReadLock, st, &[]);
    crate::screens::ui_objects::ui_set_load_progress(0.5);
    h(O::WCopyGameOptions, w, &[]);
    h(O::SetSightUpdates, 0, &[0]);
    h(O::UiSetRandomSeed, 0, &[*sm::<c_long>(O::StRandomSeed, st) as i64]);
    h(O::WSetMap, w, &[pa(hp(O::StMap, st, &[]))]);
    h(O::GraphicsReleaseScene, 0, &[]);
    h(O::WSetEffectManager, w, &[pa(hp(O::StEffectManager, st, &[]))]);
    if *sm::<bool>(O::StInScenarioEditor, st) == true {
        ee_gamecpp_world_set_count(w as *mut std::ffi::c_void, false, 17);
    } else {
        ee_gamecpp_world_set_count(w as *mut std::ffi::c_void, false, super::state::players_len() as c_ulong);
    }
    progress(16);
    *wm::<bool>(O::WInCinematic, w) = false;
    *wm::<f32>(O::WSavedGameSpeed, w) = 0.0;
    *wm::<c_long>(O::WMaxPlayers, w) = *sm::<c_long>(O::StNumPlayersMade, st);
    *wm::<c_long>(O::WTownIDCounter, w) = *sm::<c_long>(O::StTownIDCounter, st);
    *wm::<u16>(O::WHerdIDCounter, w) = *sm::<i16>(O::StHerdIDCounter, st) as u16;
    h(O::SetHerdUnreadValue, 0, &[0]);
    h(O::WCreateInfluenceTable, w, &[pa(map(w))]);
    progress(17);
    if h(O::CommNumHumanPlayers, 0, &[]) > 1 {
        let won = h(O::WonNetPresent, 0, &[]) != 0;
        let mut i: c_ulong = 0;
        while (i as usize) < super::state::players_len() {
            if h(O::StPlayerIsLocalName, super::state::player(i as usize), &[won as i64]) == 0 {
                i += 1;
                continue;
            }
            local = i;
            if *sm::<c_long>(O::StLocalPlayer, st) != i as c_long {
                moved = true;
            }
            break;
        }
    } else {
        local = *sm::<c_long>(O::StLocalPlayer, st) as c_ulong;
    }
    *wm::<c_long>(O::WCurrentPlayer, w) = local as c_long;
    h(O::CommSetPlayerNumber, 0, &[local as i64]);
    ee_gamecpp_world_set_count(w as *mut std::ffi::c_void, true, state_options(st).mUnitLimit as c_ulong);
    progress(18);
    h(O::WClearContinents, w, &[]);
    // port (save version 0.263): a loaded game's continent manager is the saved one (made in place of BuildRegions,
    // below)
    let saved_regions = loaded && ee_sim::astar::cm_file::ee_astar_cm_regions_pending();
    progress(19);
    h(O::WCreateMiniMap, w, &[pa(st)]);
    progress(20);
    *wm::<bool>(O::WUsingCarbon, w) = *sm::<bool>(O::StUsingCarbon, st);
    ee_sim::ai::next_game::trace::ee_ai_new_game(); // the computer players' per-game statics cleared
    ee_gamecpp_world_create_players(w as *mut c_void, st as *mut c_void);
    progress(21);
    if *sm::<bool>(O::StInScenarioEditor, st) == true {
        ee_gamecpp_world_set_count(w as *mut std::ffi::c_void, false, 17);
    } else {
        ee_gamecpp_world_set_count(w as *mut std::ffi::c_void, false, super::state::players_len() as c_ulong);
    }
    for i in 0..num_players(w) {
        h(O::PlCivApplyDeferred, player(w, i), &[pa(st)]);
    }
    h(O::AiCollectComputerPlayers, 0, &[]);
    progress(22);
    start_teams(w);
    *wm::<c_ulong>(O::WWondersToWin, w) = state_options(st).mWondersForVictory;
    *wm::<bool>(O::WVictoryEnabled, w) = state_options(st).mVictory;
    ee_gamecpp_world_set_time_of_day(w as *mut std::ffi::c_void, *sm::<f32>(O::StTimeOfDay, st));
    hf(O::WSetGameSpeed, w, &[], &[*sm::<f32>(O::StGameSpeed, st)]);
    let observer = prts(hp(O::WCurrentPlayerPtr, w, &[])).mObserverMode;
    h(O::SetLocalPlayerOut, 0, &[observer as i64]);
    // the owning setters (SetScenarioManager 0x13a237, SetEnvironmentManager 0x13c1e3): the old one deleted
    h(O::WDeleteScenario, w, &[]);
    h(O::WSetScenarioRaw, w, &[pa(hp(O::StScenarioManager, st, &[]))]);
    h(O::WDeleteEnvironment, w, &[]);
    h(O::WSetEnvironmentRaw, w, &[pa(hp(O::StEnvironmentManager, st, &[]))]);
    *wm::<bool>(O::WTeamsLocked, w) = state_options(st).mLockTeams;
    *wm::<bool>(O::WUnitImprovementsAllowed, w) = *sm::<bool>(O::StUnitImprovementsAllowed, st);
    *wm::<bool>(O::WCheatsEnabled, w) = state_options(st).mCheatCodes;
    super::camera_io::clear_unit_b();
    super::camera_io::clear_unit_a();
    super::camera_io::reset_path();
    super::camera_io::set_zoom(1.0);
    if moved == true {
        let width = h(O::MapSize, map(w), &[]) as c_long;
        let height = h(O::MapSize, map(w), &[]) as c_long;
        let center = Ge3DPoint::new(width as f32 * 0.5, height as f32 * 0.5, 0.0);
        super::camera_io::set_location(&center, true, true);
    } else {
        super::camera_io::set_location(hp(O::StMapCenter, st, &[]) as *const Ge3DPoint, false, true);
    }
    super::camera_io::set_changed();
    super::state::ee_gamecpp_state_repair_restorables(st as *mut std::ffi::c_void);
    progress(23);
    h(O::SetSightUpdates, 0, &[1]);
    if loaded {
        h(O::ReportGameLoaded, st, &[]);
    } else {
        super::utilities::ee_gamecpp_report_load_failure(c"Game initialized.".as_ptr(), false, std::ptr::null_mut());
    }
    if num_players(w) as i64 != h(O::MapPlayerCount, map(w), &[]) {
        super::map::ee_gamecpp_map_set_player_count(map(w) as *mut std::ffi::c_void, num_players(w) as c_ulong);
    }
    h(O::MiniMapSetViewer, 0, &[pa(player(w, current(w) as c_ulong))]);
    if *sm::<bool>(O::StRevealMap, st) == true {
        let scope = hp(O::SightRevealScopeNew, 0, &[]); // TSGT (TEST ONLY)
        let mut y: c_long = 0;
        while (y as i64) < h(O::MapSize, map(w), &[]) {
            let mut x: c_long = 0;
            while (x as i64) < h(O::MapSize, map(w), &[]) {
                let tile = hp(O::MapTile, 0, &[x as i64, y as i64]);
                if tile == 0 {
                    x += 1;
                    continue;
                }
                h(O::MeshReferenceTile, 0, &[x as i64, y as i64]);
                h(O::MeshDereferenceTile, 0, &[x as i64, y as i64]);
                for p in 1..num_players(w) {
                    h(O::PlAddOwnSight, player(w, p), &[pa(tile), x as i64, y as i64]);
                    h(O::PlRemoveOwnSight, player(w, p), &[pa(tile)]);
                }
                x += 1;
            }
            y += 1;
        }
        h(O::SightScopeDelete, scope, &[]);
    }
    super::minimap::ee_gamecpp_minimap_redraw_all(super::globals::get!(gMiniMap));
    super::minimap::ee_gamecpp_minimap_redraw(super::globals::get!(gMiniMap));
    super::minimap::ee_gamecpp_minimap_refresh(super::globals::get!(gMiniMap));
    ee_gamecpp_world_sight_all(w as *mut c_void, true);
    super::minimap::ee_gamecpp_minimap_redraw_all(super::globals::get!(gMiniMap));
    super::minimap::ee_gamecpp_minimap_redraw(super::globals::get!(gMiniMap));
    super::minimap::ee_gamecpp_minimap_refresh(super::globals::get!(gMiniMap));
    progress(24);
    if saved_regions {
        h(O::ContinentRestoreRegions, 0, &[]); // port (0.263): the saved regions (it builds them itself if it cannot read them)
    } else {
        h(O::ContinentBuildRegions, 0, &[pa(map(w))]);
    }
    h(O::PathReset, 0, &[]);
    progress(25);
    if hp(O::WEnvironment, w, &[]) != 0 {
        let enabled = h(O::DisplayWeatherEffects, 0, &[]) != 0 && h(O::CommIsMultiplayer, 0, &[]) == 0;
        h(O::EnvSetEnabled, hp(O::WEnvironment, w, &[]), &[enabled as i64]);
    }
    if hp(O::WEnvironment, w, &[]) != 0 {
        h(O::EnvFnE123D, hp(O::WEnvironment, w, &[]), &[]);
    }
    h(O::WSetCalamityHelper, w, &[pa(hp(O::StCalamityHelper, st, &[]))]);
    let calamities = hp(O::WCalamityList, w, &[]);
    if calamities != 0 {
        h(O::CalamitiesRestore, calamities, &[pa(st)]);
    }
    progress(26);
    h(O::StReadUnlock, st, &[]);
    h(O::MusicSetIntensity, 0, &[1]);
    h(O::MusicStart, 0, &[]);
    h(O::AmbientEnable, 0, &[]);
    h(O::AmbientRestart, 0, &[]);
    h(O::GraphicsReleaseScene, 0, &[]);
    h(O::GraphicsInitialiseScene, 0, &[]);
    h(O::PickerLock, 0, &[]);
    h(O::PickerReset, 0, &[]);
    h(O::PickerUnlock, 0, &[]);
    super::minimap::ee_gamecpp_minimap_refresh(super::globals::get!(gMiniMap));
    h(O::PathRequestPending, 0, &[pa(map(w))]);
    progress(27);
    for i in 1..num_players(w) {
        let pl = hp(O::WPlayer, hp(O::WorldAddr, 0, &[]), &[i as i64]);
        if pl == 0 {
            continue;
        }
        let cp = hp(O::PlComputerPlayer, pl, &[]);
        if cp != 0 {
            h(O::CpRefreshResourceCenters, cp, &[]);
        } else {
            h(O::PlRefreshTownGroups, pl, &[]);
        }
    }
    h(O::PhysSetGameStarted, 0, &[]);
    h(O::PhysClearAutoSave, 0, &[]);
    if *sm::<c_long>(O::StMajorVersion, st) == 0 && *sm::<c_ulong>(O::StMinorVersion, st) <= 0x98 {
        for i in 1..num_players(w) {
            h(O::WOptionsPlayerColorFix, w, &[i as i64]);
        }
    }
    if h(O::LobbyCampaignNameLength, 0, &[]) > 0 && lobby_options().mCampaignChecksum > 0 {
        h(O::CampaignRestoreCivilization, progress_obj, &[pa(st)]);
    }
    let civ_name = hp(O::StrNew, 0, &[]);
    if h(O::LobbyCampaignMapName, civ_name, &[]) != 0 {
        h(O::PlCivSetName, hp(O::WCurrentPlayerPtr, w, &[]), &[pa(civ_name)]);
    }
    let load_type = state_options(st).mLoadType;
    if load_type == 0 || load_type == 3 {
        h(O::ClearFormManager, 0, &[]);
    }
    ee_gamecpp_world_load_media(w as *mut c_void);
    progress(28);
    if h(O::UiInScenarioEditor, 0, &[]) == 0 {
        h(O::ScenarioPrepare, 0, &[]);
    }
    h(O::ScenarioApplyPendingEffects, 0, &[]);
    progress(29);
    ee_gamecpp_world_rebuild_active(w as *mut c_void);
    if h(O::UiInScenarioEditor, 0, &[]) == 1 && *wm::<bool>(O::WGameScreenActive, w) == true {
        ee_gamecpp_world_set_count(
            w as *mut std::ffi::c_void,
            false,
            *sm::<c_long>(O::StNumPlayersMade, st) as c_ulong,
        );
    }
    if *sm::<bool>(O::StIsLoadedGame, st) == true {
        ee_gamecpp_world_notify_loaded(
            w as *mut c_void,
            *sm::<c_long>(O::StMajorVersion, st) as c_ulong,
            *sm::<c_ulong>(O::StMinorVersion, st),
        );
    }
    for i in 0..num_players(w) {
        if player(w, i) != 0 {
            h(O::PlRestartTowns, player(w, i), &[]);
            player_body(player(w, i), pb::EE_UNITS_PLAYER_DO_RESTORE_PATROLS);
        }
    }
    *wm::<bool>(O::WLoadingPlayerMedia, w) = true;
    for i in 0..num_players(w) {
        if player(w, i) != 0 {
            player_body(player(w, i), pb::EE_UNITS_PLAYER_DO_KILL_STRANDED);
        }
    }
    *wm::<bool>(O::WLoadingPlayerMedia, w) = false;
    h(O::UpdateLandContinents, 0, &[]);
    *wm::<bool>(O::WUserPaused, w) = false;
    *wm::<u16>(O::WLagStatus, w) = 0;
    if !locked {
        h(O::WWriteUnlock, w, &[]);
    }
    h(O::StrFree, civ_name, &[]);
    h(O::CampaignProgressDelete, progress_obj, &[]);
}

/// EEWorld::CreatePlayers: the game state's players installed (their pending units placed, tech trees, map
/// restrictions and variant hit points applied, media loaded); a loaded game's pointers repaired and flagged computer
/// players replaced.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_create_players(w: *mut c_void, st: *mut c_void) {
    let (w, st) = (w as P, st as P);
    let loaded = *sm::<bool>(O::StIsLoadedGame, st);
    let major = *sm::<c_long>(O::StMajorVersion, st) as i64;
    let minor = *sm::<c_ulong>(O::StMinorVersion, st) as i64;
    let mut i: i64 = 0;
    while (i as usize) < super::state::players_len() {
        let pl = super::state::player(i as usize);
        h(O::WSetPlayer, w, &[i, pa(pl)]);
        let tree = hp(O::PlTechTree, pl, &[]);
        h(O::TechSetPlayer, tree, &[pa(pl)]);
        if h(O::TechIsLoaded, tree, &[]) == 0 {
            let o = state_options(st);
            let (start, end) = (o.mEpochStart as i64, o.mEpochEnd as i64);
            h(O::TechResetEpochRange, tree, &[start, end]);
        }
        let mut j: i64 = 0;
        while j < h(O::PlNumPending, pl, &[]) {
            let u = hp(O::PlPendingAt, pl, &[j]);
            h(O::RtsUpdateTile, u, &[pa(hp(O::StMap, st, &[]))]);
            if !*sm::<bool>(O::StIsLoadedGame, st) && hp(O::UnTileNode, u, &[]) != 0 {
                h(O::CxMakeAreaEffects, u, &[]);
            }
            rts_add_unit_entry(w, u);
            j += 1;
        }
        if *sm::<bool>(O::StIsLoadedGame, st) {
            h(O::PlReleasePending, pl, &[]);
        } else {
            h(O::PlAddPendingUnits, pl, &[0, 1]);
        }
        player_body(pl, pb::EE_UNITS_PLAYER_DO_MAP_RESTRICTIONS);
        if *sm::<bool>(O::StIsLoadedGame, st)
            && (prts(pl).mPlayerID as i64) < h(O::MapPlayerCount, hp(O::StMap, st, &[]), &[])
        {
            h(O::MapFogSeen, hp(O::StMap, st, &[]), &[pa(pl)]);
        }
        h(O::TechApplySaved, tree, &[]);
        let mut k: i64 = 0;
        while k < h(O::UnitClassCount, 0, &[]) {
            let upgrade = hp(O::PlUpgradeAt, pl, &[k]);
            if upgrade == 0 {
                k += 1;
                continue;
            }
            let unit_type = h(O::UpgradeUnitType, upgrade, &[]);
            let factor = hd(O::VariantFactor, w, &[unit_type], &[]);
            hf(O::UpgradeHitPoints, upgrade, &[unit_type, pa(pl)], &[factor]);
            k += 1;
        }
        if !media_loaded(w) && i != 0 {
            load_foundations(pl);
        }
        load_unit_media(pl, false);
        i += 1;
    }
    h(O::PlAddPendingUnits, player(w, 0), &[0, 0]);
    if loaded {
        // port (saves from 0.260 on): the computer players' managers are restored once every player is repaired
        // (their tasks name other players' towns, rebuilt by those players' RepairPointers;
        // EEComputerPlayer::RestoreManagers), unless a computer player is replaced on load (the original order then)
        let mut defer = h(O::SaveHasTowns, 0, &[major, minor]) != 0;
        let mut i: i64 = 0;
        while (i as usize) < super::state::players_len() && defer {
            let cp = hp(O::PlComputerPlayer, super::state::player(i as usize), &[]);
            defer = cp == 0 || h(O::CpReplaceOnLoad, cp, &[]) == 0;
            i += 1;
        }
        h(O::SetDeferComputerRestore, 0, &[defer as i64]);
        let mut i: i64 = 0;
        while (i as usize) < super::state::players_len() {
            let pl = super::state::player(i as usize);
            h(O::PlRepairPointers, pl, &[major, minor]);
            let cp = hp(O::PlComputerPlayer, pl, &[]);
            if cp != 0 && h(O::CpReplaceOnLoad, cp, &[]) == 1 {
                ee_gamecpp_world_replace_player(
                    w as *mut c_void,
                    i as c_ulong,
                    major as c_ulong,
                    minor as c_ulong,
                    hp(O::StMap, st, &[]) as *mut c_void,
                );
            }
            i += 1;
        }
        h(O::SetDeferComputerRestore, 0, &[0]);
        // SAFETY: the C++'s restore of the computer players' deferred managers, on the loading thread
        unsafe { ee_sim::cp::player_game::ee_cp_pl_restore_deferred(major as c_ulong, minor as c_ulong) };
    }
    rts_flush_queued(w);
    if *sm::<bool>(O::StInScenarioEditor, st) == true {
        ee_gamecpp_world_add_computers(w as *mut c_void, st as *mut c_void);
    }
    let mut i: i64 = 0;
    while (i as usize) < super::state::loaded_len() {
        let pl = super::state::loaded(i as usize);
        h(O::PlRepairPointers, pl, &[major, minor]);
        h(O::PlDelete, super::state::loaded(i as usize), &[]);
        i += 1;
    }
}

/// EEWorld::LoadMedia: the shared command confirmation effects and button sounds, once per game.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_load_media(w: *mut c_void) {
    let w = w as P;
    if media_loaded(w) {
        return;
    }
    for e in LOAD_EFFECTS {
        h(O::GfxEffectLoad, 0, &[e, 0]);
    }
    for s in BUTTON_SOUNDS {
        h(O::SoundObjectLoad, 0, &[s]);
    }
    *wm::<bool>(O::WMediaLoaded, w) = true;
}

/// EEWorld::UnloadMedia: the per-player foundation media, the shared effects and the button sounds.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_unload_media(w: *mut c_void) {
    let w = w as P;
    if media_loaded(w) != true {
        return;
    }
    for i in 1..=16 {
        let pl = player(w, i);
        if pl != 0 {
            unload_foundations(pl);
        }
    }
    for e in UNLOAD_EFFECTS {
        h(O::GfxEffectUnload, 0, &[e]);
    }
    for s in BUTTON_SOUNDS {
        h(O::SoundObjectUnload, 0, &[s]);
    }
    *wm::<bool>(O::WMediaLoaded, w) = false;
}

/// EEWorld::PauseGame (`pause`) and UnpauseGame: a running single-player game on the server; PauseGame answers 2
/// when it is already paused.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_pause(pause: bool) -> c_long {
    let running =
        h(O::CommIsServer, 0, &[]) != 0 && h(O::CommCTsProcessed, 0, &[]) > 0 && h(O::CommIsMultiplayer, 0, &[]) == 0;
    if !running {
        return 0;
    }
    let server = hp(O::CommServer, 0, &[]);
    if !pause {
        h(O::ServerUnpause, server, &[]);
        h(O::UiPostPauseInformation, 0, &[0]);
        return 0;
    }
    if h(O::ServerIsPaused, server, &[]) == 0 {
        h(O::ServerPause, server, &[]);
        h(O::UiPostPauseInformation, 0, &[1]);
        0
    } else {
        2
    }
}

/// EEWorld::ActivateUnit: queued for activation unless NULL, without an id, a path point or already queued.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_activate(cx: *mut c_void) {
    let cx = cx as P;
    if cx == 0 || rts(cx).mUnitID as u32 == u32::MAX || ucf(unit_class(cx)).mUnitType == UNIT_TYPE_PATH_POINT {
        return;
    }
    if ee_sim::app::active_ffi::ee_app_active_activate(rts(cx).mUnitID as u32) {
        h(O::RefUnitReference, cx, &[]);
    }
}

/// The deactivations queued before the unit had an id (`EEWorld::DeactivateUnit`), each holding a reference, in
/// call order (was EEWorld.cpp's `sUnbornDeactivations`).
static UNBORN: std::sync::Mutex<Vec<P>> = std::sync::Mutex::new(Vec::new());

/// The active lists' reference changes (ee_sim app active.rs), applied to the unit of the id as each is made.
fn active_refs(id: u32, delta: i32) {
    let cx = hp(O::PwComplexOfSavedId, 0, &[i64::from(id)]);
    for _ in 0..delta.max(0) {
        h(O::RefUnitReference, cx, &[]);
    }
    for _ in delta.min(0)..0 {
        h(O::RefUnitDereference, cx, &[]);
    }
}

/// The active lists emptied with their references; the waiting deactivations' references go after the queues'.
fn active_clear() {
    ee_sim::app::active::Lists::world().clear(&mut active_refs, &mut || {
        for cx in std::mem::take(&mut *UNBORN.lock().unwrap_or_else(|e| e.into_inner())) {
            h(O::RefUnitDereference, cx, &[]);
        }
    });
}

/// EEWorld::UpdateActiveUnits: the queued unit activations and deactivations applied to the list of units stepped
/// every tick.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_update_active() {
    ee_gamecpp_world_queue_born();
    ee_sim::app::active::Lists::world().update(&mut active_refs);
}

/// EEWorld::DeactivateUnit: queued for deactivation unless a path point or already queued; a unit without an id
/// waits by pointer (as the original's queue held it).
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_deactivate(cx: *mut c_void) {
    let cx = cx as P;
    if ucf(unit_class(cx)).mUnitType == UNIT_TYPE_PATH_POINT {
        return;
    }
    let mut unborn = UNBORN.lock().unwrap_or_else(|e| e.into_inner());
    if unborn.contains(&cx) {
        return;
    }
    if rts(cx).mUnitID as u32 == u32::MAX {
        unborn.push(cx);
        h(O::RefUnitReference, cx, &[]);
        return;
    }
    if ee_sim::app::active_ffi::ee_app_active_deactivate(rts(cx).mUnitID as u32) {
        h(O::RefUnitReference, cx, &[]);
    }
}

/// EEWorld::QueueBornDeactivations: the waiting deactivations whose units have an id now join the queue; a unit
/// still without one is dropped.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_queue_born() {
    let waiting = std::mem::take(&mut *UNBORN.lock().unwrap_or_else(|e| e.into_inner()));
    for cx in waiting {
        if rts(cx).mUnitID as u32 == u32::MAX
            || !ee_sim::app::active_ffi::ee_app_active_deactivate(rts(cx).mUnitID as u32)
        {
            h(O::RefUnitDereference, cx, &[]); // (queued: the reference DeactivateUnit took goes with it)
        }
    }
}

/// EEWorld::RebuildActiveUnits: the active list from the placed units (first parts; units of inactive classes only
/// when dead), the pending queues dropped.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_rebuild_active(w: *mut c_void) {
    let _ = w;
    active_clear();
    let mut u = hp(O::WFirstUnit, hp(O::WorldAddr, 0, &[]), &[]);
    while u != 0 {
        let cx = complex(u);
        let skip = cx == 0
            || hp(O::CxPreviousPart, cx, &[]) != 0
            || rts(cx).mTileX == -10000
            || ucf(unit_class(cx)).mUnitType == UNIT_TYPE_PATH_POINT
            || (ucf(unit_class(u)).mIsInactive && !(uf(cx).mHitPoints <= 0));
        if !skip {
            ee_sim::app::active_ffi::ee_app_active_start(rts(cx).mUnitID as u32);
            h(O::RefUnitReference, cx, &[]);
            h(O::RefUnitReference, cx, &[]);
        }
        u = tile_next(u);
    }
}

/// EEWorld::Reset: effects and calamities cleared, a fresh calamity helper, every unit removed, the scenario and
/// environment managers dropped.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_reset(w: *mut c_void) {
    let w = w as P;
    h(O::EffClearEffects, hp(O::WEffectManager, w, &[]), &[]);
    h(O::CalamitiesDestroy, 0, &[]);
    h(O::WSetCalamityHelper, w, &[pa(hp(O::NewCalamityHelper, 0, &[]))]);
    ee_gamecpp_world_remove_all_units(w as *mut c_void);
    h(O::WDeleteScenario, w, &[]);
    h(O::WSetScenarioRaw, w, &[0]);
    h(O::WDeleteEnvironment, w, &[]);
    h(O::WSetEnvironmentRaw, w, &[0]);
    h(O::RtsReset, w, &[]);
}

/// EEWorld::RemoveDisconnectedPlayer: flagged disconnected (unless observing), off its teams, out, its slot freed.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_remove_disconnected(w: *mut c_void, id: c_ulong) {
    let w = w as P;
    let pl = player(w, id);
    if !prts(pl).mObserverMode {
        psaved(pl).mDisconnected = true;
    }
    remove_from_teams(w, id);
    ee_gamecpp_world_mark_out(hp(O::WorldAddr, 0, &[]) as *mut c_void, id);
    prts(pl).mSlot = SLOT_NONE;
}

/// EEWorld::NotifyPlayersLoaded: every player finishes loading its units.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_notify_loaded(w: *mut c_void, major: c_ulong, minor: c_ulong) {
    let w = w as P;
    for i in 0..num_players(w) {
        if player(w, i) != 0 {
            player_rest(
                player(w, i),
                pr::EE_UNITS_PLAYER_REST_FINISH_UNITS_LOAD,
                0,
                major as c_long as i64,
                minor as c_long as i64,
            );
        }
    }
}

/// EEWorld::ResortUnit: the unit moved to its place in the world's list, sorted by tile x.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_resort(u: *mut c_void) {
    let cx = complex(u as P);
    if cx == 0 {
        return;
    }
    let prev = tile_prev(cx);
    let dummy = hp(O::DummyHead, prev, &[]);
    let mut next = tile_next(cx);
    if dummy != 0 && next == 0 {
        return;
    }
    h(O::UnUnlink, cx, &[]);
    let mut at;
    if dummy != 0 {
        at = next;
    } else {
        at = prev;
        if next == 0 {
            next = prev;
        }
    }
    let x = rts(cx).mTileX;
    let mut stop: P = 0;
    while next != 0 {
        // one bracket test (the unit's x between at's and next's), then a step back or forward
        if rts(at).mTileX <= x && rts(next).mTileX >= x {
            break;
        }
        if rts(at).mTileX > x {
            next = at;
            at = tile_prev(at);
            stop = hp(O::DummyHead, at, &[]);
        } else {
            at = next;
            next = tile_next(next);
        }
        if stop != 0 {
            break;
        }
    }
    h(O::UnLink, cx, &[pa(at)]);
}

/// EEWorld::SetPopulationLimit (`limit`) and SetNumPlayers: the RTSWorld's, then every player's share bonus and
/// civilization population bonus again.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_set_count(w: *mut c_void, population: bool, value: c_ulong) {
    let w = w as P;
    h(if population { O::RtsSetPopulationLimit } else { O::RtsSetNumPlayers }, w, &[value as i64]);
    for i in 1..17 {
        let pl = player(w, i);
        if pl != 0 {
            pfields(pl).mShareBonus = 0;
            h(O::PlCivApplyLevel21, player(w, i), &[]);
        }
    }
    h(O::PlayerShareBonus, 0, &[0]);
}

/// EEWorld::MarkPlayerOut: observer mode, its game options entry marked out (type 2).
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_mark_out(w: *mut c_void, id: c_ulong) {
    let w = w as P;
    let entry = hp(O::OptionsPlayerNew, 0, &[]);
    prts(player(w, id)).mObserverMode = true;
    let world = hp(O::WorldAddr, 0, &[]);
    if id > 0 {
        h(O::WOptionsPlayerGet, world, &[id as i64 - 1, pa(entry)]);
    }
    h(O::OptionsPlayerSetType, entry, &[2]);
    h(O::WOptionsPlayerSet, world, &[(id as c_long).wrapping_sub(1) as i64, pa(entry)]);
    h(O::OptionsPlayerDelete, entry, &[]);
}

/// EEWorld::SetTimeOfDay: the time of day, the sun's and moon's light directions.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_set_time_of_day(w: *mut c_void, time_of_day: f32) {
    let w = w as P;
    let mut t = GeTransformation::IDENTITY;
    let down = GeVector::new(-1.0, 0.0, 0.0);
    *wm::<f32>(O::WTimeOfDay, w) = time_of_day;
    t.set_orientation(RotationOrder::Ypr, 0.0, 0.0, time_of_day * 6.2831855);
    let sun = t.apply_vector(down);
    let moon = GeVector::new(sun.x * -1.0, sun.y * -1.0, sun.z * -1.0);
    t.set_orientation(RotationOrder::Ypr, 0.0, 0.62831855, 0.0);
    let sun2 = t.apply_vector(sun);
    let moon2 = t.apply_vector(moon);
    t.set_orientation(RotationOrder::Ypr, 0.78539819, 0.0, 0.0);
    *wm::<GeVector>(O::WSunDirection, w) = t.apply_vector(sun2);
    *wm::<GeVector>(O::WMoonDirection, w) = t.apply_vector(moon2);
}

/// EEWorld::PlaceQueuedUnits: the queued units on their tiles, the minimap, regions and influence table, linked into
/// the x-sorted list, their contacts found.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_place_queued(w: *mut c_void) {
    let w = w as P;
    for (u, at) in crate::rts::world::queued() {
        let (u, mut at) = (u as P, at as P);
        // port: a unit removed between its creation and this placement (Invalidate keeps its node until the node is
        // freed) was linked into the x-sorted list as a dead unit, and the list broke later. It is not placed.
        if !rts(u).mIsValid {
            h(O::LogSkipRemoved, u, &[]);
            continue;
        }
        // likewise the unit to place it after (its creator) may have left the list: search from the head instead
        if at != 0
            && hp(O::DummyHead, at, &[]) == 0
            && (!rts(at).mIsValid || hp(O::UnTileNode, at, &[]) == 0 || tile_prev(at) == 0)
        {
            h(O::LogAfterGone, at, &[]);
            at = 0;
        }
        let x = rts(u).mTileX;
        h(O::UcAddToTile, unit_class(u), &[pa(u)]);
        let (tx, ty) = (rts(u).mTileX, rts(u).mTileY);
        super::minimap::ee_gamecpp_minimap_move_unit(
            super::globals::get!(gMiniMap),
            u as *mut c_void,
            -10000,
            -10000,
            tx as c_long,
            ty as c_long,
        );
        if hp(O::UnTileNode, u, &[]) == 0 {
            continue;
        }
        if hp(O::UnTile, u, &[]) != 0 {
            h(O::UcAddToRegion, unit_class(u), &[pa(u)]);
            if ucf(unit_class(u)).mInfluence != 0 {
                let (tx, ty) = (rts(u).mTileX, rts(u).mTileY);
                h(O::InfluenceAddUnit, hp(O::WInfluence, w, &[]), &[pa(u), tx as i64, ty as i64]);
            }
        }
        if at == 0 {
            let mut head = hp(O::WUnitsHead, w, &[]);
            let mut node = hp(O::WFirstUnit, w, &[]);
            while node != 0 && x > rts(node).mTileX {
                head = node;
                node = tile_next(node);
            }
            at = head;
        } else if x != rts(at).mTileX {
            if x < rts(at).mTileX {
                at = tile_prev(at);
                let mut done = false;
                while !done {
                    if hp(O::DummyHead, at, &[]) == 0 && x < rts(at).mTileX {
                        at = tile_prev(at);
                    } else {
                        done = true;
                    }
                }
            } else {
                while tile_next(at) != 0 && x > rts(tile_next(at)).mTileX {
                    at = tile_next(at);
                }
            }
        }
        h(O::UnLink, u, &[pa(at)]);
        if uf(u).mHitPoints > 0 && cf(u).mIsCollidable != 0 {
            h(O::CxClearContacts, u, &[]);
            h(O::CxFindContacts, u, &[]);
        }
    }
}

/// EEWorld::MovePlayerToSlot: the player to a new lobby slot and name, its old slot and name to the slot's previous
/// holder; the local view switched to it when it takes the local slot.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_move_to_slot(slot: u8, new_slot: u8, player_index: u8, name: *mut c_void) {
    let world = hp(O::WorldAddr, 0, &[]);
    let mut selected: P = 0;
    let old = ee_gamecpp_world_player_by_slot(world as *mut c_void, slot) as P;
    let old_name = hp(O::StrNew, 0, &[]);
    let index = player_index as c_ulong;
    if !(index < num_players(world)) {
        h(O::StrFree, old_name, &[]);
        return;
    }
    let pl = player(world, index);
    let own_slot = prts(pl).mSlot;
    if h(O::PlIsHuman, pl, &[]) != 0 {
        h(O::HumanGetName, pl, &[pa(old_name)]);
    }
    prts(pl).mSlot = new_slot;
    if h(O::PlIsHuman, pl, &[]) != 0 {
        h(O::HumanSetName, pl, &[name as i64]);
    }
    if prts(pl).mSlot == prts(hp(O::WCurrentPlayerPtr, world, &[])).mSlot {
        let mut u = hp(O::PlFirstUnit, player(world, index), &[]);
        while u != 0 && ucf(unit_class(u)).mUnitType != UNIT_TYPE_TOWN_CENTER {
            let cx = complex(u);
            if hp(O::UnTile, u, &[]) != 0 || (cx != 0 && hp(O::CxContainer, cx, &[]) != 0) {
                selected = u;
            }
            u = next_unit(u);
        }
        let _ = selected;
        *wm::<c_long>(O::WCurrentPlayer, world) = index as c_long;
        h(O::MiniMapSetViewer, 0, &[pa(player(world, index))]);
        super::minimap::ee_gamecpp_minimap_redraw(super::globals::get!(gMiniMap));
        super::minimap::ee_gamecpp_minimap_refresh(super::globals::get!(gMiniMap));
        player_body(pl, pb::EE_UNITS_PLAYER_DO_REFRESH_TERRAIN);
        h(O::TechUpdateInterface, hp(O::PlTechTree, pl, &[]), &[1]);
        h(O::UiPostUpdateBackground, 0, &[]);
        h(O::SetCameraFollowing, 0, &[0]);
    }
    prts(old).mSlot = own_slot;
    let id = prts(pl).mPlayerID as u8 as i64;
    if h(O::CommIsServer, 0, &[]) != 0 {
        h(O::ServerFastSelect, 0, &[id]);
    } else {
        h(O::ClientFastSelect, 0, &[id]);
    }
    if h(O::PlIsHuman, old, &[]) != 0 {
        h(O::HumanSetName, old, &[pa(old_name)]);
    }
    h(O::StrFree, old_name, &[]);
}

/// EEWorld::GetPlayerIndex: the player's index in the world, -1 if absent (the last match).
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_player_index(w: *mut c_void, pl: *mut c_void) -> c_long {
    let w = w as P;
    let mut result: c_long = -1;
    for i in 0..num_players(w) {
        if player(w, i) == pl as P {
            result = i as c_long;
        }
    }
    result
}

/// EEWorld::GetPlayerBySlot: the player (Gaia excluded) in a lobby slot, or NULL.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_player_by_slot(w: *mut c_void, slot: u8) -> *mut c_void {
    let w = w as P;
    for i in 1..num_players(w) {
        if prts(player(w, i)).mSlot == slot {
            return player(w, i) as *mut c_void;
        }
    }
    std::ptr::null_mut()
}

/// EEWorld::ScreenToWorld: a screen point on the orthographic view plane (y 0) around the viewport's centre.
///
/// # Safety
/// `point` is two floats, `out` three.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_world_screen_to_world(point: *const [f32; 2], out: *mut [f32; 3]) {
    let scale = 1.0f32 / hd(O::ViewportHalfWidth, 0, &[], &[]);
    let size = hd(O::ViewportOrthoWidth, 0, &[], &[]) * 0.5;
    // SAFETY: the caller's contract
    let (px, py) = unsafe { ((*point)[0], (*point)[1]) };
    let x = (px - hd(O::ViewportXCenter, 0, &[], &[])) * scale;
    let y = (py - hd(O::ViewportYCenter, 0, &[], &[])) * scale;
    // SAFETY: as above
    unsafe { *out = [x * size, 0.0, -(y * size)] };
}

/// EEWorld's resource users (the C++'s `mResourceUsers`, +0x9a4): a resource unit's gatherer count by the unit's
/// handle id. Looked up only, never iterated, never cleared (the world's lifetime: the C++ map was gWorld's member).
static RESOURCE_USERS: std::sync::Mutex<std::collections::BTreeMap<u32, c_ulong>> =
    std::sync::Mutex::new(std::collections::BTreeMap::new());

/// EEWorld::ReleaseResourceUser (`delta` -1), AddResourceUser (1), GetResourceUsers (0): a resource unit's gatherer
/// count (an untracked unit starts at 0 when read, 1 when added, and is left alone when released).
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_resource_users(_w: *mut c_void, u: *mut c_void, delta: i32) -> c_ulong {
    // EEUnitHandle's key: the RTSUnit's id
    let id = rts(u as P).mUnitID as u32;
    resource_users(&mut RESOURCE_USERS.lock().unwrap_or_else(|e| e.into_inner()), id, delta)
}

fn resource_users(map: &mut std::collections::BTreeMap<u32, c_ulong>, id: u32, delta: i32) -> c_ulong {
    match delta {
        -1 => {
            if let Some(n) = map.get_mut(&id).filter(|n| **n > 0) {
                *n -= 1;
            }
            0
        }
        1 => {
            let n = map.entry(id).or_insert(0);
            *n = n.wrapping_add(1);
            0
        }
        _ => *map.entry(id).or_insert(0),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn resource_users_count_as_the_cpp_map_did() {
        let mut m = std::collections::BTreeMap::new();
        assert_eq!(resource_users(&mut m, 7, -1), 0);
        assert!(m.is_empty(), "a release of an untracked unit adds nothing");
        assert_eq!(resource_users(&mut m, 7, 0), 0);
        assert_eq!(m.get(&7), Some(&0), "a read adds the unit at 0");
        resource_users(&mut m, 7, -1);
        assert_eq!(m.get(&7), Some(&0), "not below 0");
        resource_users(&mut m, 7, 1);
        resource_users(&mut m, 7, 1);
        resource_users(&mut m, 8, 1);
        resource_users(&mut m, 7, -1);
        assert_eq!(resource_users(&mut m, 7, 0), 1);
        assert_eq!(resource_users(&mut m, 8, 0), 1);
    }

    #[test]
    fn the_media_lists_hold_the_cpp_constants_in_order() {
        assert_eq!(FOUNDATION_MEDIA, [0x136, 0x132, 0x133, 0x168, 0x2cd, 0x20, 0x1e0]);
        let mut a = LOAD_EFFECTS.to_vec();
        let mut b = UNLOAD_EFFECTS.to_vec();
        a.sort();
        b.sort();
        assert_eq!(a, b, "the same eight effects, in two orders");
        assert_eq!(PROGRESS.len(), 29 - 16 + 1);
        assert_eq!(PROGRESS[0], 0.53333336);
        assert_eq!(PROGRESS[13], 0.96666664);
        assert!(PROGRESS.windows(2).all(|p| p[0] < p[1]));
    }

    #[test]
    fn the_time_of_day_math_matches_the_noon_case() {
        // noon (0.5): the sun's direction before the tilt is the down vector turned half a circle
        let mut t = GeTransformation::IDENTITY;
        t.set_orientation(RotationOrder::Ypr, 0.0, 0.0, 0.5 * 6.2831855);
        let sun = t.apply_vector(GeVector::new(-1.0, 0.0, 0.0));
        assert!((sun.x - 1.0).abs() < 1e-3, "{sun:?}");
    }
}
