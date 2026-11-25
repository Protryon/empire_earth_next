//! The game state's own bodies (was EEGameState.cpp, 0x1304e4..0x131b7c): `Create` (a saved game, a scenario or a
//! campaign scenario loaded, or the players, teams, managers and random map of a new game made from the options),
//! `Init`, `Clear`, `Reset`, `RepairRestorables` and the two manager setters. Each C++ call or member is a case of
//! EEGameState.cpp's `EEGameStateOp` ([`super::EeGameCpp`]); EEGameState's methods forward here.
//!
//! The state's pointers and scalars from `mMap` to `mRevealMap` are read and written in place ([`EeGameStateTail`]).

#![allow(non_snake_case)]
// the C++'s own casts and loops, kept as written
#![allow(clippy::unnecessary_cast, clippy::needless_range_loop, clippy::bool_comparison)]

use std::ffi::{c_long, c_ulong, c_void};

use ee_base::math::types::Ge3DPoint;
use ee_sim::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved, EeUnitsRtsPlayerFields};

use crate::app::game_options::{EeAppOptionsFields, EeAppOptionsPlayerFields};

use super::{h, hd, hp, pa, ptr, rec, Owned, O, P};

/// `EEGameState`'s members from `mMap` (+0x4c0) to `mRevealMap` (+0x502), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGameStateTail {
    pub mMap: usize,
    pub mScenarioManager: usize,
    pub mEnvironmentManager: usize,
    pub mEffectManager: usize,
    pub mCalamityHelper: usize,
    pub mZoom: f32,
    pub mTimeOfDay: f32,
    pub mGameSpeed: f32,
    pub mLocalPlayer: c_long,
    pub mRandomSeed: c_long,
    pub mMajorVersion: c_long,
    pub mMinorVersion: c_ulong,
    pub mNumPlayersMade: c_long,
    pub mTownIDCounter: c_long,
    pub mHerdIDCounter: i16,
    pub mC4FA: i8,
    pub mB4FB: bool,
    pub mIsLoadedGame: bool,
    pub mInScenarioEditor: bool,
    pub mTeamsLocked: bool,
    pub mUnitImprovementsAllowed: bool,
    pub mUsingCarbon: bool,
    pub mB501: bool,
    pub mRevealMap: bool,
}

/// `kSlotNone`, `kSlotServer`.
const ST_SLOT_NONE: u8 = 0xff;
const ST_SLOT_SERVER: u8 = 0xfe;
/// `IDS_LOADING_GAME`, `IDS_CREATING_WORLD`, `IDS_COMPUTER`, `keeCouldntOpenFile`, `keeslSaveGameInvalid`.
const IDS_LOADING_GAME: i64 = 30292;
const IDS_CREATING_WORLD: i64 = 30293;
const IDS_COMPUTER: i64 = 30241;
const KEE_COULDNT_OPEN_FILE: i64 = 0x1bf0;
const KEESL_SAVE_GAME_INVALID: i64 = 0x1bf1;
/// `kWorldCivilizationBuilderPoints`.
const WORLD_CIVILIZATION_BUILDER_POINTS: i64 = 76;

fn tail<'a>(st: P) -> &'a mut EeGameStateTail {
    // SAFETY: the state's members from mMap on, used before the next C++ call that changes them
    unsafe { &mut *rec(O::StTail, st) }
}

fn opts<'a>(opt: P) -> &'a mut EeAppOptionsFields {
    // SAFETY: an EEGameOptions' plain members
    unsafe { &mut *rec(O::OptFields, opt) }
}

fn slot_fields<'a>(slot: P) -> &'a mut EeAppOptionsPlayerFields {
    // SAFETY: an EEGameOptionsPlayer's plain members
    unsafe { &mut *rec(O::OptPlayerFields, slot) }
}

fn prts<'a>(pl: P) -> &'a mut EeUnitsRtsPlayerFields {
    // SAFETY: a live player's record
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

/// The options' slot `i` (an `EEGameOptionsPlayer*`).
fn opt_slot(opt: P, i: c_ulong) -> P {
    hp(O::OptPlayer, opt, &[i as i64])
}

/// `theSlot = inOptions.mPlayers[i]` (the whole slot, its name too).
fn copy_slot(dst: P, src: P) {
    h(O::OptPlayerAssign, dst, &[pa(src)]);
}

fn player_at(st: P, i: c_ulong) -> P {
    h(O::StPlayerAt, st, &[i as i64]) as P
}

fn player_count(st: P) -> c_ulong {
    h(O::StPlayerCount, st, &[]) as c_ulong
}

/// The state's `mMapCenter`.
fn center<'a>(st: P) -> &'a mut Ge3DPoint {
    // SAFETY: the state's GE3DPoint member, used before the next C++ call
    unsafe { &mut *(hp(O::StMapCenter, st, &[]) as *mut Ge3DPoint) }
}

/// `gResource->GetString(keeCouldntOpenFile, outError)` then `"%s\n\"%s\""` of it and the file.
fn couldnt_open(err: P, file: P) {
    h(O::ResString, err, &[KEE_COULDNT_OPEN_FILE]);
    h(O::StrFormatFileError, err, &[pa(file)]);
}

/// The campaign's archive added to the data path (`theArchive.SetArchive(file)`, `AddArchive`, `EESetDataPaths`).
fn add_archive(archive: P, file: P) {
    h(O::ArchiveSet, archive, &[pa(file)]);
    h(O::DataPathAddArchive, archive, &[]);
    super::startup::ee_gamecpp_startup_set_data_paths();
}

/// EEGameState::Init: the settings' defaults (normal speed and zoom, no map, managers, teams or loaded players, a
/// fresh checksum).
#[no_mangle]
pub extern "C" fn ee_gamecpp_state_init(st: *mut c_void) {
    let st = st as P;
    let t = tail(st);
    t.mGameSpeed = 1.0;
    t.mMap = 0;
    t.mZoom = 1.0;
    t.mScenarioManager = 0;
    t.mEnvironmentManager = 0;
    t.mEffectManager = 0;
    t.mCalamityHelper = 0;
    t.mLocalPlayer = 0;
    t.mRandomSeed = 0;
    t.mMajorVersion = 0;
    t.mMinorVersion = 0;
    t.mTimeOfDay = 0.35;
    t.mNumPlayersMade = 3;
    t.mC4FA = 0;
    t.mB4FB = false;
    t.mIsLoadedGame = false;
    t.mInScenarioEditor = false;
    t.mTeamsLocked = true;
    t.mUnitImprovementsAllowed = true;
    t.mUsingCarbon = false;
    t.mRevealMap = false;
    t.mB501 = false;
    t.mTownIDCounter = 0;
    t.mHerdIDCounter = 0;
    h(O::StTeamsClear, st, &[]);
    h(O::StLoadedPlayersClear, st, &[]);
    h(O::StChecksumReset, st, &[]);
}

/// EEGameState::Reset: the map, the managers and the player list forgotten (not deleted).
#[no_mangle]
pub extern "C" fn ee_gamecpp_state_reset(st: *mut c_void) {
    let st = st as P;
    let t = tail(st);
    t.mMap = 0;
    t.mScenarioManager = 0;
    t.mEnvironmentManager = 0;
    t.mEffectManager = 0;
    h(O::StPlayersClear, st, &[]);
    tail(st).mCalamityHelper = 0;
}

/// EEGameState::Clear: the map, the scenario, effect, calamity and weather managers and every player deleted, then
/// the state's defaults (Init) and Reset.
#[no_mangle]
pub extern "C" fn ee_gamecpp_state_clear(st: *mut c_void) {
    let st = st as P;
    h(O::StDeleteMap, st, &[]);
    tail(st).mMap = 0;
    h(O::StDeleteScenario, st, &[]);
    tail(st).mScenarioManager = 0;
    h(O::StDeleteEffects, st, &[]);
    tail(st).mEffectManager = 0;
    h(O::StDeleteCalamity, st, &[]);
    tail(st).mCalamityHelper = 0;
    h(O::StDeleteEnvironment, st, &[]);
    tail(st).mEnvironmentManager = 0;
    h(O::StRestorablesClear, st, &[]);
    let mut i: c_ulong = 0;
    while i < player_count(st) {
        h(O::PlDelete, player_at(st, i), &[]);
        i += 1;
    }
    h(O::StPlayersClear, st, &[]);
    ee_gamecpp_state_init(st as *mut c_void);
    ee_gamecpp_state_reset(st as *mut c_void);
}

/// EEGameState::RepairRestorables: every restorable repairs its pointers with the loaded save's versions.
#[no_mangle]
pub extern "C" fn ee_gamecpp_state_repair_restorables(st: *mut c_void) {
    let st = st as P;
    let mut i: c_ulong = 0;
    while (i as i64) < h(O::StRestorableCount, st, &[]) {
        let t = tail(st);
        let (major, minor) = (t.mMajorVersion, t.mMinorVersion);
        h(O::StRestorableRepair, st, &[i as i64, major as i64, minor as i64]);
        i += 1;
    }
}

/// EEGameState::SetCalamityHelper: the old helper reset and deleted (unless it is the new one).
#[no_mangle]
pub extern "C" fn ee_gamecpp_state_set_calamity_helper(st: *mut c_void, helper: *mut c_void) {
    let st = st as P;
    let old = tail(st).mCalamityHelper;
    if helper as usize == old {
        return;
    }
    if old != 0 {
        h(O::CalamityHelperReset, old, &[]);
        h(O::StDeleteCalamity, st, &[]);
    }
    tail(st).mCalamityHelper = helper as usize;
}

/// EEGameState::SetEnvironmentManager: the old manager deleted.
#[no_mangle]
pub extern "C" fn ee_gamecpp_state_set_environment(st: *mut c_void, manager: *mut c_void) {
    let st = st as P;
    h(O::StDeleteEnvironment, st, &[]);
    tail(st).mEnvironmentManager = manager as usize;
}

/// The saved game, scenario or campaign scenario of the options loaded (`inOptions.mLoadType` 1, 2, 4).
fn create_loaded(st: P, opt: P, err: P, file: &mut P, l: &Locals) -> bool {
    crate::screens::ui_objects::ui_set_load_progress(0.1);
    h(O::UiSetLoadingText, 0, &[IDS_LOADING_GAME]);
    h(O::FileSpecSetVolumeStartup, *file, &[]);
    if opts(opt).mLoadType == 2 {
        h(O::FileSpecSetStartupData, *file, &[1]);
        h(O::FileSpecSetFileW, *file, &[pa(hp(O::OptMapType, opt, &[]))]);
    } else if opts(opt).mLoadType == 4 {
        h(O::FileSpecDelete, *file, &[]);
        *file = 0;
        let error = h(O::DataPathFindFile, 0, &[pa(hp(O::OptMapType, opt, &[])), ptr(file)]) as c_long;
        if error != 0 {
            h(O::CampaignFileName, l.campaign_file.0, &[pa(opt)]);
            if h(O::CampaignLoad, l.campaign.0, &[pa(l.campaign_file.0), pa(err)]) != 0 {
                let mut archive_file: P = 0;
                let error = h(O::DataPathFindFile, 0, &[pa(l.campaign_file.0), ptr(&mut archive_file)]) as c_long;
                if error == 0 {
                    add_archive(l.archive.0, archive_file);
                    h(O::FileSpecDelete, archive_file, &[]);
                    h(O::FileSpecDelete, *file, &[]);
                    *file = 0;
                    h(O::DataPathFindFile, 0, &[pa(hp(O::OptMapType, opt, &[])), ptr(file)]);
                } else {
                    couldnt_open(err, l.campaign_file.0);
                }
            } else {
                couldnt_open(err, l.campaign_file.0);
            }
        }
    } else {
        h(O::FileSpecSetStartupData, *file, &[0]);
        h(O::FileSpecSetFileW, *file, &[pa(hp(O::OptMapType, opt, &[]))]);
    }
    let mut result = false;
    let error = if *file != 0 { h(O::FileOpenReadSpec, *file, &[]) as c_long } else { 9 };
    if error == 0 {
        if h(O::ValidateSaveGameFile, *file, &[pa(err)]) != 0 {
            if h(O::LoaderLoad, l.loader.0, &[pa(*file)]) != 0 {
                result = true;
            } else {
                h(O::ResStringVerify, err, &[KEESL_SAVE_GAME_INVALID, 759]);
            }
        }
    } else {
        h(O::ResStringVerify, err, &[KEE_COULDNT_OPEN_FILE, 765]);
    }
    if !result {
        return result;
    }
    let mine = hp(O::StOptionsPtr, st, &[]);
    if opts(opt).mLoadType == 2 {
        if h(O::OptCampaignNameLength, mine, &[]) > 0 && opts(mine).mCampaignChecksum > 0 {
            h(O::CampaignFileName, l.campaign_file.0, &[pa(mine)]);
            if h(O::CampaignLoad, l.campaign.0, &[pa(l.campaign_file.0), pa(err)]) != 0 {
                h(O::FileSpecDelete, *file, &[]);
                *file = 0;
                if h(O::DataPathFindFile, 0, &[pa(l.campaign_file.0), ptr(file)]) == 0 {
                    add_archive(l.archive.0, *file);
                } else {
                    result = false;
                    couldnt_open(err, l.campaign_file.0);
                }
            } else {
                couldnt_open(err, l.campaign_file.0);
                result = false;
            }
        }
    } else if opts(opt).mLoadType == 4 {
        if h(O::OptGetCampaignMapName, opt, &[pa(l.map_name.0)]) == 0 {
            h(O::StrAssignEmpty, l.map_name.0, &[]);
        }
        opts(mine).mCampaignChecksum = opts(opt).mCampaignChecksum;
        h(O::OptCopyCampaignName, mine, &[pa(opt)]);
        h(O::OptCopyScenarioName, mine, &[pa(opt)]);
        h(O::OptSetMapName, mine, &[pa(l.map_name.0)]);
        opts(mine).mIsScenarioGame = true;
        opts(mine).mDifficulty = opts(opt).mDifficulty;
    } else {
        opts(mine).mIsScenarioGame = true;
        let mut player_index: c_ulong = 1;
        for i in 0..16 as c_ulong {
            copy_slot(l.slot.0, opt_slot(opt, i));
            let kind = slot_fields(l.slot.0).mType;
            if kind != 1 && kind != 2 {
                continue;
            }
            let mut k = player_index - 1;
            while k < 16 && k != 0 && slot_fields(opt_slot(mine, k)).mB36 {
                k += 1;
            }
            player_index = k + 1;
            let pl = player_at(st, player_index);
            if h(O::PlIsHuman, pl, &[]) != 0 {
                h(O::PlSetNameFromSlot, pl, &[pa(l.slot.0)]);
            } else {
                h(O::PlSetNameFromSlot, pl, &[pa(l.slot.0)]);
                h(O::CpSetReplaceOnLoad, hp(O::PlComputerPlayer, pl, &[]), &[1]);
            }
            copy_slot(l.other.0, opt_slot(mine, i));
            slot_fields(l.other.0).mType = slot_fields(l.slot.0).mType;
            h(O::OptPlayerCopyName, l.other.0, &[pa(l.slot.0)]);
            copy_slot(opt_slot(mine, i), l.other.0);
            player_index += 1;
        }
    }
    result
}

/// A new game's players, teams, managers and (for a random map) map made from the options (`mLoadType` 0, 3).
fn create_new(st: P, opt: P, l: &Locals) {
    crate::screens::ui_objects::ui_set_load_progress(0.1);
    h(O::UiSetLoadingText, 0, &[IDS_CREATING_WORLD]);
    let mut used = [false; 16];
    for i in 0..16 as c_ulong {
        copy_slot(l.slot.0, opt_slot(opt, i));
        if slot_fields(l.slot.0).mType == 1 {
            if let Some(u) = used.get_mut(slot_fields(l.slot.0).mColor as usize) {
                *u = true;
            }
        }
    }
    h(O::StPlayersResize, st, &[1]);
    tail(st).mNumPlayersMade = 1;
    h(O::StPlayerSet, st, &[0, pa(hp(O::NewWorldPlayer, 0, &[]))]);
    let mut player_id: c_ulong = 1;
    let mut color: c_ulong = 0;
    let mut engines = [-1 as c_long; 16];
    for i in 0..16 as c_ulong {
        copy_slot(l.slot.0, opt_slot(opt, i));
        let kind = slot_fields(l.slot.0).mType;
        if kind != 1 && kind != 0 && kind != 2 {
            continue;
        }
        let pl;
        if kind == 0 {
            while used.get(color as usize).copied().unwrap_or(false) {
                color += 1;
            }
            slot_fields(l.slot.0).mColor = color;
            color += 1;
            h(O::ResString, l.computer_name.0, &[IDS_COMPUTER]);
            let computer = hp(O::NewComputerPlayer, l.computer_name.0, &[player_id as i64]);
            let seed = h(O::UiRandomSeed, 0, &[]);
            let choice =
                h(O::CpEngineResolve, 0, &[slot_fields(l.slot.0).mEngine as i64, seed, i as i64, player_id as i64]);
            h(O::CpEngineSet, computer, &[choice]);
            engines[i as usize] = choice as c_ulong as c_long;
            pl = computer;
        } else {
            if kind == 2 {
                while used.get(color as usize).copied().unwrap_or(false) {
                    color += 1;
                }
                slot_fields(l.slot.0).mColor = color;
                color += 1;
            }
            pl = hp(O::NewHumanPlayerNamed, l.slot.0, &[player_id as i64]);
            if slot_fields(l.slot.0).mIsLocal {
                tail(st).mLocalPlayer = player_id as c_long;
            }
            if kind == 2 {
                prts(pl).mObserverMode = true;
                prts(pl).mSeesAll = true;
                slot_fields(l.slot.0).mTeam = 0;
            }
        }
        h(O::PlSetPlayerColor, pl, &[slot_fields(l.slot.0).mColor as i64]);
        if opts(opt).mLoadType == 0 && !prts(pl).mObserverMode {
            let points = h(O::WorldTableLong, 0, &[WORLD_CIVILIZATION_BUILDER_POINTS]);
            h(O::PlSetCivilizationPoints, pl, &[points]);
        }
        pfields(pl).mMaxStartingCitizens = slot_fields(l.slot.0).mStartingCitizens as c_long;
        for j in 1..6i64 {
            let amount = h(O::ResourceLevelAmount, 0, &[opts(opt).mResources as i64, j - 1]);
            psaved(pl).mResourceCaps[(j + 1) as usize] = amount as c_long;
        }
        h(O::StPlayersResize, st, &[player_id as i64 + 1]);
        tail(st).mNumPlayersMade = (player_id + 1) as c_long;
        h(O::StPlayerSet, st, &[player_id as i64, pa(pl)]);
        player_id += 1;
    }
    let mut player_id: c_ulong = 1;
    for i in 0..16 as c_ulong {
        copy_slot(l.slot.0, opt_slot(opt, i));
        let kind = slot_fields(l.slot.0).mType;
        if kind != 1 && kind != 0 && kind != 2 {
            continue;
        }
        if slot_fields(l.slot.0).mTeam > 0 && kind != 2 {
            let mut other_id: c_ulong = 1;
            for j in 0..16 as c_ulong {
                copy_slot(l.other.0, opt_slot(opt, j));
                let other_kind = slot_fields(l.other.0).mType;
                if other_kind != 1 && other_kind != 0 && other_kind != 2 {
                    continue;
                }
                if other_kind != 2
                    && j != i
                    && slot_fields(l.other.0).mTeam == slot_fields(l.slot.0).mTeam
                    && player_at(st, player_id) != player_at(st, other_id)
                {
                    h(O::PlSetStance, player_at(st, player_id), &[pa(player_at(st, other_id)), 0, 0]);
                }
                other_id += 1;
            }
        }
        player_id += 1;
    }
    let mut i: c_ulong = 0;
    while i < player_count(st) {
        let pl = player_at(st, i);
        psaved(pl).mChatType = 0;
        let allies = h(O::PlAlliesCount, pl, &[]);
        if allies > 0 {
            for j in 0..allies {
                if h(O::PlIsHuman, hp(O::PlAllyAt, pl, &[j]), &[]) != 0 {
                    psaved(pl).mChatType = 1;
                    break;
                }
            }
        }
        i += 1;
    }
    // the speed option's factor (gameoptions.yaml speeds; an unknown option: the default speed's)
    let speeds = h(O::RulesSpeedCount, 0, &[]) as c_ulong;
    let speed =
        if opts(opt).mGameSpeed < speeds { opts(opt).mGameSpeed } else { h(O::RulesSpeedDefault, 0, &[]) as c_ulong };
    tail(st).mGameSpeed = hd(O::RulesSpeedFactor, 0, &[speed as i64], &[]);
    let (mut width, mut height): (c_long, c_long) = (0, 0);
    h(O::OptGetMapDimensions, opt, &[ptr(&mut width), ptr(&mut height)]);
    h(O::StNewMap, st, &[width as i64, height as i64, player_count(st) as i64]);
    let middle = 0.5f32 + ((width + height) as c_ulong) as f32 * 0.5f32;
    *center(st) = Ge3DPoint::new(middle, middle, 0.0);
    h(O::StNewScenarioManager, st, &[]);
    h(O::StNewEffectManager, st, &[]);
    h(O::StNewCalamityHelper, st, &[]);
    h(O::StNewEnvironmentManager, st, &[]);
    let mut i: c_ulong = 0;
    while i < player_count(st) {
        let pl = player_at(st, i);
        if i == 0 {
            prts(pl).mSlot = ST_SLOT_NONE;
        }
        if i == 1 {
            prts(pl).mSlot = ST_SLOT_SERVER;
        }
        i += 1;
    }
    if opts(opt).mLoadType == 0 {
        let mut local_index: c_ulong = 0;
        let mut i: c_ulong = 0;
        while i < player_count(st) {
            let pl = player_at(st, i);
            if !prts(pl).mObserverMode {
                h(O::VecPlayersPush, l.players.0, &[pa(pl)]);
                if i as c_long == tail(st).mLocalPlayer {
                    local_index = (h(O::VecPlayersSize, l.players.0, &[]) - 1) as c_ulong;
                }
            }
            i += 1;
        }
        tail(st).mRandomSeed = h(O::UiRandomSeed, 0, &[]) as c_long;
        let mut place = Ge3DPoint::default();
        let (seed, map) = (tail(st).mRandomSeed, tail(st).mMap);
        h(O::CreateRandomMap, opt, &[pa(l.players.0), local_index as i64, seed as i64, pa(map), ptr(&mut place)]);
        *center(st) = place;
    }
    if opts(opt).mLoadType == 3 {
        tail(st).mNumPlayersMade = 3;
    }
    let mine = hp(O::StOptionsPtr, st, &[]);
    h(O::OptAssign, mine, &[pa(opt)]);
    for i in 0..16 {
        // the game's options hold the engines played (a saved game shows them)
        if engines[i] >= 0 {
            slot_fields(opt_slot(mine, i as c_ulong)).mEngine = engines[i] as c_ulong;
        }
    }
    if opts(opt).mEpochStart == ee_sim::tech::tech_ids::ee_tech_epoch_count() as c_long {
        // random
        let random = Owned::new(O::RandomNew, O::RandomDelete);
        h(O::RandomSetSeed, random.0, &[h(O::UiRandomSeed, 0, &[])]);
        opts(mine).mEpochStart = h(O::RandomGenerate, random.0, &[0, opts(opt).mEpochEnd as i64]) as c_long;
    }
    if opts(opt).mRevealMap {
        tail(st).mRevealMap = true;
    }
    h(O::LobbyWriteLock, 0, &[]);
    if opts(opt).mLoadType == 3 {
        // SAFETY: the lobby's options
        let lobby = unsafe { &mut *rec::<EeAppOptionsFields>(O::LobbyOptions, 0) };
        lobby.mLoadType = 1;
        h(O::LobbyMapTypeScenDefault, 0, &[]);
    }
    h(O::LobbyWriteUnlock, 0, &[]);
}

/// Create's C++ locals (made before the first call, as the C++'s were).
struct Locals {
    players: Owned,
    loader: Owned,
    slot: Owned,
    other: Owned,
    archive: Owned,
    campaign_file: Owned,
    map_name: Owned,
    computer_name: Owned,
    campaign: Owned,
}

/// EEGameState::Create: a saved game, scenario or campaign scenario loaded (the error message set on failure), or
/// the players, teams, managers and random map of a new game made; whether it succeeded.
#[no_mangle]
pub extern "C" fn ee_gamecpp_state_create(st: *mut c_void, opt: *mut c_void, err: *mut c_void) -> bool {
    let (st, opt, err) = (st as P, opt as P, err as P);
    let l = Locals {
        players: Owned::new(O::VecPlayersNew, O::VecPlayersDelete),
        loader: Owned::new(O::LoaderNew, O::LoaderDelete),
        slot: Owned::new(O::OptionsPlayerNew, O::OptionsPlayerDelete),
        other: Owned::new(O::OptionsPlayerNew, O::OptionsPlayerDelete),
        archive: Owned::new(O::ArchiveNew, O::ArchiveDelete),
        campaign_file: Owned::new(O::StrNew, O::StrFree),
        map_name: Owned::new(O::StrNew, O::StrFree),
        computer_name: Owned::new(O::StrNew, O::StrFree),
        campaign: Owned::new(O::CampaignNew, O::CampaignDelete),
    };
    let mut file = hp(O::FileSpecNew, 0, &[]);
    let mut result = true;
    ee_gamecpp_state_init(st as *mut c_void);
    if crate::gamecpp::globals::have!(gPhysics) {
        h(O::CxSetPoolSize, 0, &[500, 40]);
    }
    if opts(opt).mLoadType == 5 {
        opts(opt).mLoadType = 3;
    }
    let load_type = opts(opt).mLoadType;
    if load_type != 0 && load_type != 3 && load_type != 5 {
        result = create_loaded(st, opt, err, &mut file, &l);
    } else {
        create_new(st, opt, &l);
    }
    h(O::FileSpecDelete, file, &[]);
    drop(l);
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem::offset_of;

    #[test]
    fn the_tail_is_the_classes_order() {
        // +0x4c0..+0x502 of the 32-bit original; the pointers' and longs' widths are the platform's
        let p = std::mem::size_of::<usize>();
        let l = std::mem::size_of::<c_long>();
        assert_eq!(offset_of!(EeGameStateTail, mZoom), 5 * p);
        assert_eq!(offset_of!(EeGameStateTail, mLocalPlayer), 5 * p + 12);
        assert_eq!(offset_of!(EeGameStateTail, mHerdIDCounter), 5 * p + 12 + 6 * l);
        assert_eq!(offset_of!(EeGameStateTail, mRevealMap), 5 * p + 12 + 6 * l + 10);
    }
}
