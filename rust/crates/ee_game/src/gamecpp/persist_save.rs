//! Saving and loading a game (was EESaveLoad.cpp, 0x14c6b1..0x14e839, and EEGameStateLoader.cpp, 0x14a009..0x14ac35):
//! the save's chunk list and header, the chunks' write, the auto-saves' trim, the sound muted around a save or a
//! load, the save file's open, the file's validation and header check, the lobby's check of a save's players, and
//! the loader (the chunks restored into the game state in tag order, the players moved into it, one chunk read out of
//! a file). Each C++ call or member is a case of EESaveLoad.cpp's `EECore9OpC` or EEGameStateLoader.cpp's
//! `EEPLoaderOp` ([`super::EeGameCpp`]'s lane C section); the C++ functions forward here.

#![allow(non_snake_case)]
#![allow(clippy::unnecessary_cast, clippy::bool_comparison)]

use std::ffi::{c_long, c_ulong, c_void};
use std::sync::atomic::{AtomicU32, Ordering};

use ee_base::math::types::Ge3DPoint;
use ee_sim::world::player_fields::EeUnitsRtsPlayerFields;

use super::persistent::{PF_FAILED, PF_OK};
use super::state::EeGameStateTail;
use super::{h, hd, hf, hp, pa, ptr, rec, Owned, O, P};
use crate::app::game_options::EeAppOptionsPlayerFields;

/// The string ids (ee/EEStringIDs.h, ee/EESaveLoad.h; EESaveLoad.cpp's static_assert).
const PS_IDS_FILE_COULDNT_BE_OPENED: i64 = 7152;
const PS_IDS_SAVE_GAME_INCOMPATIBLE: i64 = 7154;
const PS_IDS_SAVE_GAME_INVALID_DATA: i64 = 7156;
const PS_IDS_SAVE_GAME_MISSING_DATA: i64 = 7157;
const PS_IDS_TOO_MANY_PLAYERS_IN_LOBBY: i64 = 10034;
const PS_IDS_PLAYER_NOT_IN_SAVED: i64 = 10037;
const PS_IDS_SAVED_GAME_REQUIRES_PLAYERS: i64 = 10038;
const PS_IDS_TOO_MANY_PLAYERS_TO_PLAY: i64 = 10039;
const PS_IDS_NOT_VALID_MULTIPLAYER_SCENARIO: i64 = 10040;
const PS_IDS_ERROR_CREATING_FILE: i64 = 10049;
const PS_IDS_ERROR_SAVING_FILE: i64 = 10050;
const PS_IDS_INVALID_FILE_NAME: i64 = 10068;
const PS_KEESL_SAVE_GAME_INVALID: i64 = 0x1bf1;

/// The chunk tags (ee/EEPersistentTag.h): `kPersistentChecksum` .. `kPersistentCalamities`.
const PS_TAG_CHECKSUM: i64 = 1;
const PS_TAG_SEED: i64 = 2;
const PS_TAG_CAMERA: i64 = 3;
const PS_TAG_MAP: i64 = 4;
const PS_TAG_PLAYERS: i64 = 5;
const PS_TAG_SCENARIO: i64 = 6;
const PS_TAG_WORLD: i64 = 7;
const PS_TAG_EFFECTS: i64 = 10;
const PS_TAG_MULTIPLAYER: i64 = 8;
const PS_TAG_ENVIRONMENT: i64 = 12;
const PS_TAG_CALAMITIES: i64 = 11;

/// `kFileAccessRead`, `kFileAccessWrite`.
const PS_READ: i64 = 1;
const PS_WRITE: i64 = 2;
/// `kSlotNone`, `kSlotServer`.
const PS_SLOT_NONE: u8 = 0xff;
const PS_SLOT_SERVER: u8 = 0xfe;
/// The load result for a file that is not a valid save, or a load that failed.
const PS_NOT_LOADED: i64 = 9;

/// The volumes MuteForSaveLoad mutes and UnmuteAfterSaveLoad restores (0x531cac, 0x531ca8).
static MASTER_VOLUME: AtomicU32 = AtomicU32::new(0);
static MUSIC_VOLUME: AtomicU32 = AtomicU32::new(0);

fn tail<'a>(st: P) -> &'a mut EeGameStateTail {
    // SAFETY: the state's members from mMap on, used before the next C++ call that changes them
    unsafe { &mut *rec(O::StTail, st) }
}

fn major() -> i64 {
    h(O::PsMajorVersion, 0, &[])
}

fn minor() -> i64 {
    h(O::SaveMinorVersion, 0, &[])
}

/// A C++ object the Rust made for the call (`new`, then `delete` when it goes).
fn owned(make: O, delete: O) -> Owned {
    Owned::new(make, delete)
}

/// BuildSaveGameChunks: the chunks a save writes, in chunk order (each new; the game objects only borrowed).
fn build_chunks() -> Vec<P> {
    let mut list = Vec::new();
    let checksum = hp(O::PsNewChecksumChunk, 0, &[]);
    h(O::PsWorldChecksum, checksum, &[]);
    list.push(checksum);
    list.push(hp(O::PsNewSeedChunk, 0, &[]));
    let map = hp(O::PsNewMapChunk, 0, &[]);
    h(O::PsMapChunkUnown, map, &[]);
    list.push(map);
    let mut location = Ge3DPoint::default();
    h(O::CameraGetLocation, 0, &[ptr(&mut location)]);
    list.push(hp(O::PsNewCameraChunk, 0, &[ptr(&mut location)]));
    let scenario = hp(O::PsNewScenarioChunk, 0, &[]);
    h(O::PsScenarioChunkUnown, scenario, &[]);
    list.push(scenario);
    let effects = hp(O::PsNewEffectsChunk, 0, &[]);
    h(O::PsEffectsChunkUnown, effects, &[]);
    list.push(effects);
    let environment = hp(O::PsNewEnvironmentChunk, 0, &[]);
    h(O::PsEnvironmentChunkUnown, environment, &[]);
    list.push(environment);
    list.push(hp(O::PsNewPlayerHelper, 0, &[]));
    list.push(hp(O::PsNewWorldHelper, 0, &[]));
    list.push(hp(O::PsNewMultiplayerHelper, 0, &[]));
    list.push(hp(O::PsNewCalamityHelper, 0, &[]));
    list
}

/// `BuildSaveGameChunks`'s C++ entry (the EE-SIM editor's chunk digest calls it): the chunks into its std::vector.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ps_build_chunks(list: *mut c_void) {
    for item in build_chunks() {
        h(O::PsListPush, list as P, &[pa(item)]);
    }
}

/// FillSaveGameHeader: name, versions, player names, date, chunk tags, scenario media strings and game options.
fn fill_header(name: P, header: P, list: &[P]) {
    h(O::PsHdrSetName, header, &[pa(name)]);
    h(O::PsHdrDefaultDescription, header, &[]);
    // SAFETY: the header's mMajorVersion and mMinorVersion
    unsafe {
        let v = hp(O::PsHdrVersions, header, &[]) as *mut c_ulong;
        *v = major() as c_ulong;
        *v.add(1) = minor() as c_ulong;
    }
    h(O::PsHdrNamesClear, header, &[]);
    let players = h(O::PwNumPlayers, 0, &[]) as c_ulong;
    for i in 1..players {
        h(O::PsHdrNamesAppendPlayer, header, &[i as i64]);
        if i + 1 < players {
            h(O::PsHdrNamesAppendSpace, header, &[]);
        }
    }
    h(O::PsHdrSetDate, header, &[]);
    h(O::PsHdrTagsClear, header, &[]);
    for &item in list {
        let tag = h(O::PsPersistentTag, item, &[]);
        h(O::PsHdrTagsPush, header, &[tag]);
    }
    if h(O::PsWorldScenario, 0, &[]) != 0 {
        h(O::PsHdrMediaFromScenario, header, &[]);
    }
    h(O::PsHdrOptionsFromWorld, header, &[]);
    // SAFETY: the header's mL4E4, mNumPlayers, mChecksum
    unsafe { *(hp(O::PsHdrNumbers, header, &[]) as *mut c_ulong).add(1) = h(O::PsMaxPlayers, 0, &[]) as c_ulong };
}

/// `TrimAutoSaves`: the files matching a pattern in a directory deleted, lowest (FSFileSpec order) first, until at
/// most `max` remain.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ps_trim_auto_saves(
    pattern: *const c_void,
    directory: *const c_void,
    max: c_ulong,
) -> c_long {
    let files = owned(O::PsSpecsNew, O::PsSpecsDelete);
    let file = owned(O::PsSpecNew, O::PsSpecDelete);
    let dir = owned(O::PsSpecNew, O::PsSpecDelete);
    h(O::PsSpecSetDirectory, dir.0, &[directory as i64]);
    let mut r = h(O::PsSpecFirstFile, dir.0, &[pattern as i64, pa(file.0)]);
    while r == PF_OK {
        let n = h(O::PsSpecsCount, files.0, &[]);
        let mut i = 0;
        while i < n {
            if h(O::PsSpecLess, file.0, &[pa(files.0), i]) != 0 {
                break;
            }
            i += 1;
        }
        h(O::PsSpecsInsert, files.0, &[i, pa(file.0)]);
        if h(O::PsSpecsCount, files.0, &[]) as c_ulong > max {
            h(O::PsSpecsDeleteFirst, files.0, &[]);
            h(O::PsSpecsEraseFirst, files.0, &[]);
        }
        r = h(O::PsSpecNextFile, dir.0, &[pa(file.0)]);
    }
    0
}

/// WriteSaveGameChunks: each listed chunk written to a file; on success the chunks deleted and the list emptied; the
/// first error.
fn write_chunks(list: &mut Vec<P>, file: P) -> i64 {
    let chunks = owned(O::PsChunksNew, O::PsChunksDelete);
    let n = list.len() as i64;
    h(O::PsChunksResize, chunks.0, &[n]);
    let mut r = PF_OK;
    let mut i = 0;
    while i < n && r == PF_OK {
        r = h(O::PsChunkSavePersistent, chunks.0, &[i, major(), minor(), pa(list[i as usize])]);
        if r != PF_OK {
            break;
        }
        r = h(O::PsChunkSaveToFile, chunks.0, &[i, pa(file)]);
        i += 1;
    }
    if r == PF_OK {
        for item in list.drain(..) {
            h(O::PsPersistentDelete, item, &[]);
        }
    }
    h(O::PsChunksClear, chunks.0, &[]);
    r
}

/// WriteSaveGame: a whole saved game (the header, then every chunk); the first error.
fn write_save(name: P, time: c_ulong, file: P, other: P) -> i64 {
    let header = owned(O::PsHeaderNew, O::PsHeaderDelete);
    let mut list = build_chunks();
    fill_header(name, header.0, &list);
    // SAFETY: the header's mChecksum
    unsafe { *(hp(O::PsHdrNumbers, header.0, &[]) as *mut c_ulong).add(2) = time };
    let mut r = h(O::PsHeaderSave, header.0, &[major(), minor(), pa(file)]);
    if r == PF_OK {
        r = write_chunks(&mut list, other);
    }
    r
}

/// `LoadSavedGameFile`: a saved game validated and loaded with the sound muted (a multiplayer reload or into the game
/// state), the graphics resumed after when asked; 9 when the file is not a valid save.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ps_load_saved_game_file(
    file: *mut c_void,
    reload: bool,
    quiet: bool,
    resume: bool,
) -> c_long {
    let file = file as P;
    let r = if mute(true, quiet) == 0 && h(O::PsValidateIntoMessage, file, &[]) != 0 {
        let r = load_into_game(file, reload, h(O::PsGameState, 0, &[]) as P);
        if r == 0 {
            unmute(true, quiet)
        } else {
            r
        }
    } else {
        PS_NOT_LOADED
    };
    if resume {
        super::globals::graphics_post_resume();
    }
    r as c_long
}

/// `EELoadSavedGameFile`: a saved game opened from a directory, validated and loaded into the game state with the sound
/// muted, the graphics resumed after when asked; the first error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ps_load_from_directory(
    name: *const c_void,
    reload: bool,
    quiet: bool,
    directory: *const c_void,
    resume: bool,
) -> c_long {
    let file = owned(O::PsSpecNew, O::PsSpecDelete);
    let mut r = open_save_file(file.0, true, name as P, directory as P);
    if r != PF_OK {
        return r as c_long;
    }
    if mute(false, quiet) == 0 && h(O::PsValidateIntoMessage, file.0, &[]) != 0 {
        r = load_into_game(file.0, reload, h(O::PsGameState, 0, &[]) as P);
        if r == PF_OK {
            r = unmute(true, quiet);
        }
    } else {
        r = PF_FAILED;
    }
    if resume {
        crate::screens::ui_objects::ui_resume_graphics();
    }
    r as c_long
}

/// `SaveGame`: the game saved to a file in a directory with the sound muted; the first error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ps_save_game(
    name: *const c_void,
    time: c_ulong,
    quiet: bool,
    directory: *const c_void,
) -> c_long {
    let file = owned(O::PsSpecNew, O::PsSpecDelete);
    // the original reads the physics CT count into a local it never uses (0x14d080)
    h(O::CommCTsProcessed, 0, &[]);
    let mut r = open_save_file(file.0, false, name as P, directory as P);
    if r != PF_OK {
        return r as c_long;
    }
    mute(false, quiet);
    r = save_to_file(file.0, time);
    unmute(false, quiet);
    r as c_long
}

/// OpenSaveFile: a file spec pointed at the startup directory plus a subdirectory and name and opened (recreated
/// first for a save); the load/save error message set on failure.
fn open_save_file(file: P, loading: bool, name: P, directory: P) -> i64 {
    let mut r = PF_OK;
    h(O::PsSpecSetStartupVolume, file, &[]);
    h(O::PsSpecSetStartupDirectory, file, &[pa(directory)]);
    h(O::PsSpecSetFile, file, &[pa(name)]);
    let access = if loading { PS_READ } else { PS_WRITE };
    if access == PS_WRITE {
        h(O::PsSpecDeleteFile, file, &[]);
        r = h(O::PsSpecCreate, file, &[]);
        if r != PF_OK {
            if r == PF_FAILED {
                h(O::PsMessageString, 0, &[PS_IDS_INVALID_FILE_NAME]);
            } else {
                h(O::PsMessageString, 0, &[PS_IDS_ERROR_CREATING_FILE]);
            }
        }
    }
    if r == PF_OK {
        r = h(O::PsSpecOpen, file, &[access]);
        if r != PF_OK {
            h(O::PsMessageString, 0, &[PS_IDS_FILE_COULDNT_BE_OPENED]);
        }
    }
    r
}

/// MuteForSaveLoad: the sound and music muted (their volumes kept) and 642 ms waited before a save or a load; before a
/// save the map's vertex colours applied, before a host's non-quiet load the splash form shown.
fn mute(loading: bool, quiet: bool) -> i64 {
    if !loading {
        h(O::PsMapApplyVertexColors, 0, &[]);
    }
    MASTER_VOLUME.store(hd(O::PsMasterVolume, 0, &[], &[]).to_bits(), Ordering::Relaxed);
    MUSIC_VOLUME.store(hd(O::PsMusicVolume, 0, &[], &[]).to_bits(), Ordering::Relaxed);
    hf(O::PsSetMasterVolume, 0, &[], &[0.0]);
    hf(O::PsSetMusicVolume, 0, &[], &[0.0]);
    ee_base::plat_core::time::PlatSleep(0x282);
    if quiet || !super::net::present() || h(O::CommIsServer, 0, &[]) == 0 || loading != true {
        return 0;
    }
    h(O::UiLockForCopy, 0, &[]);
    h(O::PsShowSplash, 0, &[]);
    h(O::UiUnlockForCopy, 0, &[]);
    0
}

/// UnmuteAfterSaveLoad: the volumes restored after a save or a load; after a save the selection areas shown again,
/// after a host's non-quiet load the main game form opened and the next render waited for.
fn unmute(loading: bool, quiet: bool) -> i64 {
    if !loading {
        h(O::PsShowSelectionAreas, 0, &[]);
    }
    hf(O::PsSetMasterVolume, 0, &[], &[f32::from_bits(MASTER_VOLUME.load(Ordering::Relaxed))]);
    hf(O::PsSetMusicVolume, 0, &[], &[f32::from_bits(MUSIC_VOLUME.load(Ordering::Relaxed))]);
    if !quiet && h(O::CommIsServer, 0, &[]) != 0 && loading == true {
        h(O::PsOpenMainGameForm, 0, &[]);
        h(O::PsWaitRenderNow, 0, &[]);
    }
    0
}

/// LoadIntoGame: a saved game file loaded into a game state, or for a reload into the world, while the saved-game flag
/// is set; the "save game invalid" message set on failure.
fn load_into_game(file: P, reload: bool, state: P) -> i64 {
    h(O::PsSetLoadingSavedGame, 0, &[1]);
    let r = if !reload { load_game_state(state, file) } else { reload_saved_game(file) };
    if r != PF_OK {
        h(O::PsMessageStringVerify, 0, &[PS_KEESL_SAVE_GAME_INVALID]);
    }
    h(O::PsSetLoadingSavedGame, 0, &[0]);
    r
}

/// SaveGameToFile: the game written into memory, then to the file (the old text twin deleted first), and "Game saved."
/// or an error reported; the first error.
fn save_to_file(file: P, time: c_ulong) -> i64 {
    let memory = owned(O::PsMemFileNew, O::PsMemFileDelete);
    let text_name = owned(O::PsUStrNew, O::PsUStrDelete);
    h(O::PsSaveTextName, file, &[pa(text_name.0), 1]);
    delete_text_twin(file);
    let mut r = write_save(text_name.0, time, memory.0, memory.0);
    if r == PF_OK {
        r = h(O::PsMemFileSave, memory.0, &[pa(file)]);
        if r == PF_OK {
            r = h(O::PsSpecClose, file, &[]);
        }
    } else {
        h(O::PsMessageString, 0, &[PS_IDS_ERROR_SAVING_FILE]);
    }
    let report: &std::ffi::CStr =
        if r == PF_OK { c"Game saved.  " } else { c"Error saving game.  Please report this bug." };
    super::utilities::ee_gamecpp_report_load_failure(report.as_ptr(), false, std::ptr::null_mut());
    r
}

/// LoadGameState: a saved game file loaded into a game state; 9 when it failed.
fn load_game_state(state: P, file: P) -> i64 {
    if loader_load(state, file) {
        PF_OK
    } else {
        PS_NOT_LOADED
    }
}

/// ReloadSavedGame: a saved game file loaded into the global game state and, when it worked, the world restarted from
/// it (the scenario editor and the multiplayer reload).
fn reload_saved_game(file: P) -> i64 {
    let st = h(O::PsGameState, 0, &[]) as P;
    h(O::PsStClear, st, &[]);
    let r = load_game_state(st, file);
    if tail(st).mIsLoadedGame && r == PF_OK {
        if h(O::PsEditorFormPresent, 0, &[]) != 0 {
            h(O::PsEditorClearSelection, 0, &[]);
        }
        // SAFETY: the game's preloading thread object
        crate::preload::ee_preload_wait_for_queue(unsafe { *std::ptr::addr_of!(super::globals::gDataPreLoading) });
        h(O::PsWorldClear, 0, &[]);
        super::world::ee_gamecpp_world_start_game(hp(O::WorldAddr, 0, &[]) as *mut c_void, st as *mut c_void, true);
        h(O::PsStReset, st, &[]);
    }
    r
}

/// `EECheckSavedGameOptions`: a save's players against the lobby (the scenario slots for a multiplayer scenario, else
/// every lobby player a saved one and all saved humans present); the problems listed; whether there are none.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ps_check_saved_options(
    saved: *mut c_void,
    current: *mut c_void,
    mut all_found: bool,
    missing: *mut c_void,
    count: c_long,
) -> bool {
    let (saved, current, missing) = (saved as P, current as P, missing as P);
    let humans = owned(O::PsWStrsNew, O::PsWStrsDelete);
    let computers = owned(O::PsWStrsNew, O::PsWStrsDelete);
    let lobby = owned(O::PsWStrsNew, O::PsWStrsDelete);
    let format = owned(O::PsWStrNew, O::PsWStrDelete);
    let message = owned(O::PsWStrNew, O::PsWStrDelete);
    let mut open: c_ulong = 0;
    let mut needed: c_ulong = 1;
    h(O::PsWStrsClear, missing, &[]);
    for i in 0..16i64 {
        let s = slot(saved, i);
        if s.mType == 1 {
            h(O::PsWStrsPushSlotName, humans.0, &[pa(saved), i]);
        } else if s.mType == 2 {
            h(O::PsWStrsPushSlotName, computers.0, &[pa(saved), i]);
        } else if s.mType == 0 {
            open += 1;
            if !s.mB36 && i < count as i64 {
                needed += 1;
            }
        }
        let c = slot(current, i);
        if c.mType == 1 || c.mType == 2 {
            h(O::PsWStrsPushSlotName, lobby.0, &[pa(current), i]);
        }
    }
    let n_humans = h(O::PsWStrsCount, humans.0, &[]) as c_ulong;
    let n_computers = h(O::PsWStrsCount, computers.0, &[]) as c_ulong;
    let n_lobby = h(O::PsWStrsCount, lobby.0, &[]) as c_ulong;
    // SAFETY: the current options' plain members
    let load_type = unsafe { (*rec::<crate::app::game_options::EeAppOptionsFields>(O::OptFields, current)).mLoadType };
    let push_string = |id: i64| {
        h(O::PsResourceString, message.0, &[id]);
        h(O::PsWStrsPush, missing, &[pa(message.0)]);
    };
    if load_type == 1 {
        if n_humans != 1 || open != 15 {
            push_string(PS_IDS_NOT_VALID_MULTIPLAYER_SCENARIO);
        }
        if n_lobby > needed {
            h(O::PsResourceString, format.0, &[PS_IDS_TOO_MANY_PLAYERS_TO_PLAY]);
            h(O::PsSprintfNumber, message.0, &[pa(format.0), needed as i64]);
            h(O::PsWStrsPush, missing, &[pa(message.0)]);
        }
    } else if n_lobby <= n_humans + n_computers {
        let mut found: c_ulong = 0;
        for j in 0..n_lobby as i64 {
            let name = hp(O::PsWStrsAt, lobby.0, &[j]);
            if h(O::PsWStrsContains, humans.0, &[pa(name)]) != 0 {
                found += 1;
            } else if h(O::PsWStrsContains, computers.0, &[pa(name)]) == 0 {
                h(O::PsResourceString, format.0, &[PS_IDS_PLAYER_NOT_IN_SAVED]);
                h(O::PsSprintfText, message.0, &[pa(format.0), pa(name)]);
                h(O::PsWStrsPush, missing, &[pa(message.0)]);
                all_found = false;
            }
        }
        if all_found == true && found != n_humans {
            h(O::PsResourceString, format.0, &[PS_IDS_SAVED_GAME_REQUIRES_PLAYERS]);
            h(O::PsSprintfTwoNumbers, message.0, &[pa(format.0), n_humans as i64, found as i64]);
            h(O::PsWStrsPush, missing, &[pa(message.0)]);
        }
    } else {
        push_string(PS_IDS_TOO_MANY_PLAYERS_IN_LOBBY);
    }
    h(O::PsWStrsCount, missing, &[]) == 0
}

fn slot<'a>(options: P, i: i64) -> &'a EeAppOptionsPlayerFields {
    // SAFETY: an options' player slot's plain members
    unsafe { &*rec(O::OptPlayerFields, hp(O::OptPlayer, options, &[i])) }
}

/// `EEValidateSaveGameFile`: whether a file holds a save this build can load; on failure the error message set (the
/// header check's specific text, else the generic "save game invalid").
#[no_mangle]
pub extern "C" fn ee_gamecpp_ps_validate(file: *mut c_void, message: *mut c_void) -> bool {
    let (file, message) = (file as P, message as P);
    let header = owned(O::PsHeaderNew, O::PsHeaderDelete);
    let memory = owned(O::PsMemFileNew1000, O::PsMemFileDelete);
    let mut result = false;
    h(O::PsWStrClear, message, &[]);
    h(O::PsSpecClose, file, &[]);
    if h(O::PsSpecOpen, file, &[PS_READ]) == PF_OK {
        if h(O::PsMemFileLoad, memory.0, &[pa(file)]) == PF_OK
            && h(O::PsHeaderLoad, header.0, &[major(), minor(), pa(memory.0)]) == PF_OK
            && check_header(header.0, message, memory.0)
        {
            result = true;
            h(O::PsSpecClose, file, &[]);
            h(O::PsSpecOpen, file, &[PS_READ]);
        } else if h(O::PsWStrLength, message, &[]) == 0 {
            // the generic text only when the header check left no specific one
            h(O::PsResourceStringVerify, message, &[PS_KEESL_SAVE_GAME_INVALID]);
        }
    } else {
        h(O::PsResourceString, message, &[PS_IDS_FILE_COULDNT_BE_OPENED]);
    }
    result
}

/// CheckSaveGameHeader: a save header's version and chunk tags (each of 1..7, 10 and 11 once, no 0) and every chunk's
/// size readable and sane; the message set on failure.
fn check_header(header: P, message: P, file: P) -> bool {
    let mut result = false;
    let mut err = PF_OK;
    // SAFETY: the header's versions
    let (hmaj, hmin) = unsafe {
        let v = hp(O::PsHdrVersions, header, &[]) as *const c_ulong;
        (*v, *v.add(1))
    };
    if hmaj <= major() as c_ulong && hmin <= minor() as c_ulong {
        let tags = header_tags(header);
        let count = |t: c_long| tags.iter().filter(|&&x| x == t).count();
        if count(0) == 0 {
            if [1, 2, 3, 4, 5, 6, 7, 10, 11].iter().all(|&t| count(t) == 1) {
                let mut i = 0;
                while i < tags.len() && err == PF_OK {
                    let chunk = owned(O::PsChunkNew, O::PsChunkDelete);
                    err = h(O::PsChunkLoadHeader, chunk.0, &[pa(file)]);
                    if err == PF_OK {
                        let mut size: c_ulong = 0;
                        if h(O::PsChunkGetSize, chunk.0, &[&mut size as *mut c_ulong as usize as i64]) != 0 {
                            if size as u64 <= 0x8000_0000 {
                                err = h(O::PsChunkSkip, chunk.0, &[pa(file)]);
                            } else {
                                err = 0xe;
                            }
                        } else {
                            err = 3;
                        }
                    }
                    i += 1;
                }
                if err == PF_OK {
                    result = true;
                }
            } else {
                h(O::PsResourceString, message, &[PS_IDS_SAVE_GAME_MISSING_DATA]);
            }
        } else {
            h(O::PsResourceString, message, &[PS_IDS_SAVE_GAME_INVALID_DATA]);
        }
    } else {
        h(O::PsResourceString, message, &[PS_IDS_SAVE_GAME_INCOMPATIBLE]);
    }
    result
}

fn header_tags(header: P) -> Vec<c_long> {
    let n = h(O::PsHdrTagsCount, header, &[]);
    (0..n).map(|i| h(O::PsHdrTagAt, header, &[i]) as c_long).collect()
}

/// `SaveGameToFile`'s C++ entry (EESaveLoad.cpp's `SaveGameToFile`, 0x14d34b).
#[no_mangle]
pub extern "C" fn ee_gamecpp_ps_save_to_file(file: *mut c_void, time: c_ulong) -> c_long {
    save_to_file(file as P, time) as c_long
}

/// `WriteSaveGame`'s C++ entry.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ps_write_save(
    name: *const c_void,
    time: c_ulong,
    file: *mut c_void,
    other: *mut c_void,
) -> c_long {
    write_save(name as P, time, file as P, other as P) as c_long
}

// ---------------------------------------------------------------------------------------------- the loader

/// `EEGameStateLoader::Load`: a saved game or scenario file loaded into memory and the game state restored from it.
fn loader_load(state: P, file: P) -> bool {
    let memory = owned(O::PsMemFileNew1000, O::PsMemFileDelete);
    let mut result = false;
    h(O::PsSetLoadingScenario, file, &[]);
    if (h(O::PsFileIsOpen, file, &[]) != 0 || h(O::PsSpecOpenAny, file, &[PS_READ]) == PF_OK)
        && h(O::PsMemFileLoad, memory.0, &[pa(file)]) == PF_OK
    {
        h(O::PsFileCloseAny, file, &[]);
        result = load_game(state, memory.0, memory.0);
        h(O::PsFileCloseAny, memory.0, &[]);
    }
    h(O::PsClearLoadingScenario, 0, &[]);
    result
}

/// `EEGameStateLoader::Load`'s C++ entry.
#[no_mangle]
pub extern "C" fn ee_gamecpp_loader_load(state: *mut c_void, file: *mut c_void) -> bool {
    loader_load(state as P, file as P)
}

/// `EEGameStateLoader::LoadGame`: the game state restored from a saved header and its chunks; on failure "Load game
/// file is corrupt." reported and the state cleared; whether it worked.
#[no_mangle]
pub extern "C" fn ee_gamecpp_loader_load_game(state: *mut c_void, header_file: *mut c_void, file: *mut c_void) -> bool {
    load_game(state as P, header_file as P, file as P)
}

fn load_game(st: P, header_file: P, file: P) -> bool {
    let header = owned(O::PsHeaderNew, O::PsHeaderDelete);
    let mut result = false;
    h(O::PsStWriteLock, st, &[]);
    h(O::PsStInit, st, &[]);
    if h(O::PsInScenarioEditor, 0, &[]) != 0 {
        tail(st).mInScenarioEditor = true;
    }
    if h(O::PsHeaderLoad, header.0, &[major(), minor(), pa(header_file)]) == PF_OK {
        // SAFETY: the header's versions
        let (hmaj, hmin) = unsafe {
            let v = hp(O::PsHdrVersions, header.0, &[]) as *const c_ulong;
            (*v, *v.add(1))
        };
        tail(st).mMajorVersion = hmaj as c_long;
        tail(st).mMinorVersion = hmin;
        h(O::PsStOptionsFromHeader, st, &[pa(header.0)]);
        let tags = header_tags(header.0);
        // port (decision D6): the chunks of a save older than 0.255 hold Windows-1252 text, converted as it loads
        ee_base::ustring::ee_ustring_set_legacy_text_load(!ee_sim::save_versions::EESaveTextIsUtf8(hmaj, hmin));
        if restore_persistents(st, &tags, file) == PF_OK {
            result = true;
        }
        h(O::PsStFinishLoad, st, &[hmaj as i64, hmin as i64]);
        ee_base::ustring::ee_ustring_set_legacy_text_load(false);
    }
    if !result {
        super::utilities::ee_gamecpp_report_load_failure(
            c"Load game file is corrupt.".as_ptr(),
            false,
            std::ptr::null_mut(),
        );
        h(O::PsStClear, st, &[]);
        // port: the failed load's teardown done (EE-SIM ends its wait for the game with it: a clean failure)
        h(O::PsDebugCorrupt, 0, &[]);
    }
    tail(st).mIsLoadedGame = result;
    h(O::PsStWriteUnlock, st, &[]);
    result
}

/// ReadPersistent: the next chunk of a saved game and its persistent object made; the read error.
fn read_persistent(file: P, out: &mut P) -> i64 {
    let chunk = owned(O::PsChunkNew, O::PsChunkDelete);
    *out = 0;
    let mut r = h(O::PsChunkLoadFromFile, chunk.0, &[pa(file)]);
    if r == PF_OK {
        r = h(O::PsChunkLoadPersistent, chunk.0, &[out as *mut P as usize as i64]);
    }
    r
}

/// `EEGameStateLoader::RestorePersistents`: every saved chunk in tag order into the game state while the load
/// progress bar advances; 3 for an unknown tag or a chunk of the wrong type.
fn restore_persistents(st: P, tags: &[c_long], file: P) -> i64 {
    let mut r = PF_OK;
    let n = tags.len();
    let mut i = 0;
    while i < n && r == PF_OK {
        crate::screens::ui_objects::ui_set_load_progress(i as f32 / n as f32 * 0.4 + 0.1);
        let mut item: P = 0;
        r = read_persistent(file, &mut item);
        if r != PF_OK {
            i += 1;
            continue;
        }
        let cast = |o: O| hp(o, item, &[]);
        match h(O::PsPersistentTag, item, &[]) {
            PS_TAG_CHECKSUM => match cast(O::PsCastChecksum) {
                0 => r = 3,
                c => {
                    h(O::PsStSetChecksum, st, &[pa(c)]);
                }
            },
            PS_TAG_SEED => match cast(O::PsCastSeed) {
                0 => r = 3,
                c => tail(st).mRandomSeed = h(O::PsSeedValue, c, &[]) as c_long,
            },
            PS_TAG_EFFECTS => match cast(O::PsCastEffects) {
                0 => r = 3,
                c => {
                    let manager = hp(O::PsEffectsChunkObject, c, &[]);
                    h(O::PsStSetEffectManager, st, &[pa(manager)]);
                    h(O::PsStRestorablePushEffects, st, &[pa(manager)]);
                    h(O::PsEffectsChunkUnown, c, &[]);
                }
            },
            PS_TAG_CAMERA => match cast(O::PsCastCamera) {
                0 => r = 3,
                c => {
                    if tail(st).mMinorVersion > 0x42 {
                        // SAFETY: the chunk's point and the state's centre
                        unsafe {
                            *(hp(O::StMapCenter, st, &[]) as *mut Ge3DPoint) =
                                *(hp(O::PsCameraChunkValue, c, &[]) as *const Ge3DPoint)
                        };
                    } else {
                        ee_gamecpp_st_default_map_center(st as *mut c_void);
                    }
                }
            },
            PS_TAG_MAP => match cast(O::PsCastMap) {
                0 => r = 3,
                c => {
                    h(O::PsStSetMap, st, &[pa(hp(O::PsMapChunkObject, c, &[]))]);
                    h(O::PsMapChunkUnown, c, &[]);
                }
            },
            PS_TAG_PLAYERS => match cast(O::PsCastPlayers) {
                0 => r = 3,
                c => {
                    restore_players(st, c);
                }
            },
            PS_TAG_WORLD => match cast(O::PsCastWorld) {
                0 => r = 3,
                c => super::persist_world::ee_gamecpp_pwh_restore(c as *mut c_void, st as *mut c_void),
            },
            PS_TAG_SCENARIO => match cast(O::PsCastScenario) {
                0 => r = 3,
                c => {
                    h(O::PwStSetScenario, st, &[pa(hp(O::PsScenarioChunkObject, c, &[]))]);
                    h(O::PsScenarioChunkUnown, c, &[]);
                }
            },
            PS_TAG_CALAMITIES => match cast(O::PsCastCalamities) {
                0 => r = 3,
                c => {
                    super::state::ee_gamecpp_state_set_calamity_helper(st as *mut c_void, c as *mut c_void);
                    h(O::PsStRestorablePushCalamities, st, &[pa(c)]);
                    item = 0;
                }
            },
            PS_TAG_ENVIRONMENT => match cast(O::PsCastEnvironment) {
                0 => r = 3,
                c => {
                    let manager = hp(O::PsEnvironmentChunkObject, c, &[]);
                    h(O::PsStSetEnvironmentManager, st, &[pa(manager)]);
                    h(O::PsStRestorablePushEnvironment, st, &[pa(manager)]);
                    h(O::PsEnvironmentChunkUnown, c, &[]);
                }
            },
            PS_TAG_MULTIPLAYER => {}
            _ => r = 3,
        }
        h(O::PsPersistentDelete, item, &[]);
        i += 1;
    }
    r
}

/// `EEGameState::SetDefaultMapCenter`: the camera centred on the middle of the map, if there is one.
#[no_mangle]
pub extern "C" fn ee_gamecpp_st_default_map_center(state: *mut c_void) {
    let st = state as P;
    let map = tail(st).mMap;
    if map == 0 {
        return;
    }
    let mut center = (h(O::PsMapL195610, map, &[]) as c_long + h(O::PsMapL195614, map, &[]) as c_long) as f32 * 0.5;
    center += 0.5;
    // SAFETY: the state's centre
    unsafe { *(hp(O::StMapCenter, st, &[]) as *mut Ge3DPoint) = Ge3DPoint::new(center, center, 0.0) };
}

/// `EEGameStateLoader::RestorePlayers`: the loaded players moved into the game state (players past the slot count
/// become closed loaded players) and, when hosting, the server slot given to the local player; the helper's error.
fn restore_players(st: P, helper: P) -> i64 {
    let mut local: c_long = 0;
    let max = h(O::PsPphMaxPlayers, helper, &[]) as c_ulong;
    let mut players: Vec<P> = Vec::new();
    let r = h(O::PsPphLocalIndex, helper, &[&mut local as *mut c_long as usize as i64]);
    if r == PF_OK {
        tail(st).mLocalPlayer = local;
        // GetPlayers: the same test GetLocalPlayerIndex passed, so its players
        let n = h(O::PsPphPlayerCount, helper, &[]);
        players = (0..n).map(|i| hp(O::PsPphPlayerAt, helper, &[i])).collect();
        if tail(st).mInScenarioEditor == true {
            h(O::StPlayersResize, st, &[players.len() as i64]);
            tail(st).mNumPlayersMade = max as c_long;
        } else {
            h(O::StPlayersResize, st, &[max as i64]);
            tail(st).mNumPlayersMade = max as c_long;
        }
        if r == PF_OK {
            for (i, &pl) in players.iter().enumerate() {
                if tail(st).mInScenarioEditor == false && i as c_ulong >= max {
                    h(O::PsStLoadedPlayerPush, st, &[pa(pl)]);
                    let n = h(O::StNumPlayers, st, &[]);
                    for j in 0..n {
                        h(O::PsPlayerRemoveAlly, hp(O::StPlayerAt, st, &[j]), &[pa(pl)]);
                    }
                    let options = hp(O::StOptionsPtr, st, &[]);
                    // SAFETY: the slot's plain members (the original copies the slot, sets the type, copies it back)
                    unsafe {
                        (*rec::<EeAppOptionsPlayerFields>(
                            O::OptPlayerFields,
                            hp(O::OptPlayer, options, &[i as i64 - 1]),
                        ))
                        .mType = 4
                    };
                } else {
                    h(O::StPlayerSet, st, &[i as i64, pa(pl)]);
                }
            }
        }
    }
    // only with the players read: a failed read left the list empty and the local index unset
    if r == PF_OK && super::net::present() && h(O::CommIsServer, 0, &[]) != 0 {
        if players.len() > 1 {
            prts(players[1]).mSlot = PS_SLOT_NONE;
        }
        if (local as c_ulong as usize) < players.len() {
            prts(players[local as usize]).mSlot = PS_SLOT_SERVER;
        }
    }
    r
}

fn prts<'a>(pl: P) -> &'a mut EeUnitsRtsPlayerFields {
    // SAFETY: a live player's record
    unsafe { &mut *rec(O::PlRts, pl) }
}

/// `EELoadPersistent`: one chunk (by tag) read out of a saved game file without loading the rest; the read error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ps_load_persistent(name: *const c_void, tag: c_long, out: *mut *mut c_void) -> c_long {
    let header = owned(O::PsHeaderNew, O::PsHeaderDelete);
    let memory = owned(O::PsMemFileNew1000, O::PsMemFileDelete);
    let mut found: P = 0;
    let mut r = h(O::PsDataPathFind, 0, &[name as i64, &mut found as *mut P as usize as i64]);
    if r == PF_OK {
        r = h(O::PsSpecOpenAny, found, &[PS_READ]);
        if r == PF_OK {
            h(O::PsMemFileLoad, memory.0, &[pa(found)]);
            r = h(O::PsHeaderLoad, header.0, &[major(), minor(), pa(memory.0)]);
            let tags = header_tags(header.0);
            // SAFETY: the header's versions
            let (hmaj, hmin) = unsafe {
                let v = hp(O::PsHdrVersions, header.0, &[]) as *const c_ulong;
                (*v, *v.add(1))
            };
            // port (decision D6): a chunk of a save older than 0.255 holds Windows-1252 text
            let legacy = r == PF_OK && !ee_sim::save_versions::EESaveTextIsUtf8(hmaj, hmin);
            ee_base::ustring::ee_ustring_set_legacy_text_load(legacy);
            for &t in &tags {
                let chunk = owned(O::PsChunkNew, O::PsChunkDelete);
                if t == tag {
                    r = h(O::PsChunkLoadFromFile, chunk.0, &[pa(memory.0)]);
                    if r == PF_OK {
                        r = h(O::PsChunkLoadPersistent, chunk.0, &[out as usize as i64]);
                    }
                    break;
                }
                h(O::PsChunkSkip, chunk.0, &[pa(memory.0)]);
            }
            ee_base::ustring::ee_ustring_set_legacy_text_load(false);
        }
        h(O::PsSpecDelete, found, &[]);
    }
    r as c_long
}

/// The platform layer's file calls (src/Platform).
/// cbindgen:ignore
#[cfg(not(test))]
mod plat {
    use std::ffi::{c_char, c_int};
    extern "C" {
        pub fn PlatDeleteFile(path: *const c_char) -> c_int;
        pub fn PlatGetLastError() -> u32;
    }
}

/// `PLAT_MAX_PATH`.
const PS_MAX_PATH: usize = 260;

/// The text twin's path of a save (its volume, directory and `GetSaveTextName(file, false)`), or None when it is
/// longer than `PLAT_MAX_PATH`.
fn text_twin_path(volume: &[u8], directory: &[u8], file: &[u8]) -> Option<Vec<u8>> {
    let name = super::persist_text::save_text_name(file, file.len() as c_ulong, false);
    let name = name.split(|&c| c == 0).next().unwrap_or(&[]);
    if volume.len() + directory.len() + name.len() > PS_MAX_PATH {
        return None;
    }
    Some([volume, directory, name, &[0]].concat())
}

/// DeleteSaveTextTwin: the old text twin (".txt") of a save deleted, as the original's unused debug file's Delete did
/// (FSBasicFile::Delete: a path longer than PLAT_MAX_PATH is left alone, a failure records the native error).
fn delete_text_twin(file: P) {
    // SAFETY: an FSFileSpec is its Rust record from the vtable on; its strings are live
    let path = unsafe {
        let s = &*(file as *const ee_base::fs::spec::EeFsSpec);
        let text = |t: &ee_base::fs::spec::EeFsString| {
            if t.string.is_null() {
                &[][..]
            } else {
                std::slice::from_raw_parts(t.string as *const u8, t.length as usize)
            }
        };
        text_twin_path(text(&s.volume), text(&s.directory), text(&s.file))
    };
    #[cfg(not(test))]
    if let Some(path) = path {
        // SAFETY: a NUL-terminated path
        unsafe {
            if plat::PlatDeleteFile(path.as_ptr().cast()) == 0 {
                ee_base::error::ee_error_record_native(plat::PlatGetLastError() as c_ulong);
            }
        }
    }
    #[cfg(test)]
    let _ = path;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_text_twin_is_the_save_with_a_txt_extension() {
        assert_eq!(text_twin_path(b"C:", b"\\Saved\\", b"game.ees").unwrap(), b"C:\\Saved\\game.txt\0");
        // 200 + 52 + 8 = PLAT_MAX_PATH: still deleted; one more is left alone
        assert!(text_twin_path(&[b'x'; 200], &[b'y'; 52], b"long.ees").is_some());
        assert!(text_twin_path(&[b'x'; 200], &[b'y'; 53], b"long.ees").is_none());
    }

    #[test]
    fn the_tags_a_save_needs_once() {
        let need =
            [PS_TAG_CHECKSUM, PS_TAG_SEED, PS_TAG_CAMERA, PS_TAG_MAP, PS_TAG_PLAYERS, PS_TAG_SCENARIO, PS_TAG_WORLD];
        assert_eq!(need, [1, 2, 3, 4, 5, 6, 7]);
        assert_eq!((PS_TAG_MULTIPLAYER, PS_TAG_EFFECTS, PS_TAG_CALAMITIES), (8, 10, 11));
    }

    #[test]
    fn the_volumes_keep_their_bits() {
        MASTER_VOLUME.store((-0.0f32).to_bits(), Ordering::Relaxed);
        assert_eq!(f32::from_bits(MASTER_VOLUME.load(Ordering::Relaxed)).to_bits(), (-0.0f32).to_bits());
    }
}
