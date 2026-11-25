//! The game messages' own bodies that called the game's C++ (was EEMessage.cpp: the record types' Execute bodies
//! the C++ kept, the world's, the units' and the players' actions and the fast selection of the messages' host,
//! the group command's markers, the saved game's request): each C++ call or member is a case of EEMessage.cpp's
//! `EEMessageOp` ([`super::EeGameCpp`]). The decisions of the other types are ee_sim netmsg's (bodies.rs), which
//! asks the C++ host for these.
//!
//! Single-player games execute these too (the local server hands the records straight to the physics); the network
//! halves (the lobby's, the loading's, the migration's) only a LAN game runs.

#![allow(non_snake_case)]
// the C++'s own casts, kept as written
#![allow(
    clippy::unnecessary_cast,
    clippy::bool_comparison,
    clippy::nonminimal_bool,
    clippy::collapsible_else_if,
    clippy::collapsible_match,
    clippy::manual_is_multiple_of
)]

use std::ffi::{c_long, c_ulong, c_void};

use ee_base::math::types::Ge3DPoint;
use ee_sim::netmsg::bodies::*;
use ee_sim::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved, EeUnitsRtsPlayerFields};
use ee_sim::world::unit_class_fields::EeUnitsUnitClassFields;
use ee_sim::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};

use super::session_objs::{EeGamecppClientTail, EeGamecppServerTail};
use super::{cptr, h, hf, hp, pa, ptr, rec, Owned, O, P};
use ee_net::net::ffi_comms::EeNetCommsField;
use ee_net::net::ffi_server::EeNetSvSet;
use ee_sim::units::group_host::ee_units_selection;
use ee_sim::units::player_body::game::ee_units_player_body;
use ee_sim::units::player_body::{EE_UNITS_PLAYER_DO_CLEAR_SAVED_GROUP, EE_UNITS_PLAYER_DO_UNSELECT_ALL};
use ee_sim::units::player_rest::game::ee_units_player_rest;
use ee_sim::units::player_rest::EE_UNITS_PLAYER_REST_SET_GAME_RESULT;

// ---- the constants the bodies pass (EEMessage.cpp static_asserts them)
/// `kGraphicEffectCommandConfirmationMove`, `...Attack`, `...GatherOther`, `kGraphicEffectDisbandUnitEmitter`.
const EFFECT_MOVE: i64 = 0xb8;
const EFFECT_ATTACK: i64 = 0xb9;
const EFFECT_TARGET: i64 = 0xba;
const EFFECT_DISBAND: i64 = 0xbc;
/// `kAIKindAircraftCarrier`, `kAIKindSpaceCarrier`, `kUnitTypeAirport`, `kGoalUnloadTransport`,
/// `kUnitCategoryFortress`.
const AI_KIND_AIRCRAFT_CARRIER: c_ulong = 0xf;
const AI_KIND_SPACE_CARRIER: c_ulong = 0x27;
const UNIT_TYPE_AIRPORT: c_ulong = 0x42;
const MSG_GOAL_UNLOAD_TRANSPORT: i64 = 8;
const UNIT_CATEGORY_FORTRESS: c_ulong = 0x1d;
/// `kActionIdle`.
const MSG_ACTION_IDLE: i64 = 5000;
/// `kSoundButton2`, `kSoundIncomingChatMessage`, `kSoundClockTick`, `kSoundCashRegister`.
const SOUND_BUTTON2: i64 = 0x208;
const SOUND_INCOMING_CHAT: i64 = 0x3ae;
const SOUNDS: [i64; 2] = [0x3a8, 0xe];
/// `kSlotNone`, `kSlotServer`.
const MSG_SLOT_NONE: u8 = 0xff;
const MSG_SLOT_SERVER: u8 = 0xfe;
/// `kEECPChoiceClassic`.
const ENGINE_CLASSIC: c_ulong = 0;
/// the resource strings (ee/EEStringIDs.h; EEMessage.cpp static_asserts them)
const IDS_LOADING: i64 = 30663;
const IDS_SAVING_GAME_AS: i64 = 30661;
const IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME: i64 = 30612;
const IDS_NEW_HOST_ESTABLISHED: i64 = 30634;
const IDS_HOSTILE: i64 = 29514;
const IDS_FRIENDLY: i64 = 29513;
const IDS_CHANGES_DIPLOMATIC_STANCE_TO: i64 = 31601;
const IDS_YOU_TRIBUTE: i64 = 31602;
const IDS_TRIBUTES_YOU: i64 = 31600;
const IDS_CARBON: i64 = 30121;
const IDS_PLAYER_HAS_RESIGNED_FROM_GAME: i64 = 31808;
const IDS_MSG_GAME: i64 = 31813;
/// `kNames`: the tribute's resource names (food, wood, stone, gold, iron by resource 2..6)
const RESOURCE_NAMES: [i64; 7] = [0, 0, 30116, 30117, 30118, 30119, 30120];

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

fn ucf<'a>(uc: P) -> &'a mut EeUnitsUnitClassFields {
    // SAFETY: as rts
    unsafe { &mut *rec(O::UcFields, uc) }
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

/// A record's member `i` (1 = the first after the selection id) as T.
fn get<T: Copy>(m: P, i: i64) -> T {
    let at = hp(O::MsgField, m, &[i, std::mem::size_of::<T>() as i64]) as *const T;
    // SAFETY: the record's bytes of the member (its size verified by the C++)
    unsafe { at.read_unaligned() }
}

fn get_bool(m: P, i: i64) -> bool {
    get::<u8>(m, i) != 0
}

fn world() -> P {
    hp(O::WorldAddr, 0, &[])
}

/// A plain member of the world (`op` answers its address).
fn wm<'a, T>(op: O) -> &'a mut T {
    // SAFETY: the op answers the address of the world's member of type T
    unsafe { &mut *rec(op, world()) }
}

fn current() -> P {
    hp(O::WorldCurrentPlayer, 0, &[])
}

fn world_player(i: i64) -> P {
    hp(O::WorldPlayer, 0, &[i])
}

fn sender(m: P) -> P {
    hp(O::MsgSender, m, &[])
}

fn num_players() -> c_ulong {
    // SAFETY: the world's member
    unsafe { *rec::<c_ulong>(O::WNumPlayers, world()) }
}

fn complex(u: P) -> P {
    hp(O::UnComplex, u, &[])
}

fn unit_class(u: P) -> P {
    hp(O::UnClass, u, &[])
}

fn new_str() -> Owned {
    Owned::new(O::StrNew, O::StrFree)
}

fn res(s: &Owned, id: i64) -> i64 {
    h(O::ResString, s.0, &[id])
}

fn chat(text: P, kind: i64) {
    h(O::UiAddChat, text, &[kind]);
}

/// A game line in white (`AddChatLogEntry(GEColor(1, 1, 1), "Game", text, the display time * 2, 0)`).
fn chat_game_line(text: P) {
    let name = new_str();
    res(&name, IDS_MSG_GAME);
    h(O::UiAddChatGameLine, text, &[pa(name.0)]);
}

// ---- the Execute bodies the C++ kept (kNetMsgBodies' order; ee_sim netmsg's EE_NETMSG_BODY_*)

/// The sender's selection (and its saved group) given a new formation of the requested type.
fn change_formation(m: P) {
    let group = hp(O::PlActiveGroup, sender(m), &[]);
    let formation = hp(O::FormationCreate, 0, &[get::<i32>(m, 1) as i64]);
    h(O::FormationSetB5C, formation, &[get_bool(m, 2) as i64]);
    h(O::SelGroupSetFormation, group, &[pa(formation)]);
    let saved = hp(O::SelGroupSavedGroup, group, &[]);
    if saved != 0 {
        h(O::GroupSetFormation, saved, &[pa(formation)]);
    }
    h(O::FormationDereference, formation, &[]);
}

/// A player's civilization (its name and upgrades) validated by the server applied, once only when civilizations
/// are bought; the choice locked for players other than the local one.
fn civ_upgrades_purchased(m: P) {
    let name = new_str();
    let index = get::<u8>(m, 1) as i64;
    let pl = world_player(index);
    let apply = if h(O::WCanBuyCivilization, world(), &[]) != 0 { !psaved(pl).mCivUpgradesPurchased } else { true };
    if apply {
        h(O::StrAssignMsg, name.0, &[pa(m)]);
        if get::<i32>(m, 2) != 0 {
            res(&name, get::<i32>(m, 2) as i64);
        }
        h(O::PlCivSetName, pl, &[pa(name.0)]);
        let upgrades = Owned(hp(O::VecCivUpgradesOfMsg, m, &[]), O::VecCivUpgradesDelete);
        h(O::PlCivAddUpgrades, pl, &[pa(upgrades.0), index]);
        psaved(pl).mCivUpgradesPurchased = true;
        if pl != current() {
            psaved(pl).mCivilizationChosen = true;
            psaved(pl).mCivilizationLocked = true;
        }
    }
}

/// The server validates a player's requested civilization upgrades.
fn civ_upgrades_requested(m: P) {
    let upgrades = Owned(hp(O::VecCivUpgradesOfMsg, m, &[]), O::VecCivUpgradesDelete);
    h(O::CivRequestUpgrades, m, &[get::<u8>(m, 1) as i64, pa(upgrades.0)]);
}

/// One cargo unit unloaded from a living, enabled transport at once; if that fails, the transport class's
/// unload-failed event posted to its owner.
fn immediate_cargo_unload(m: P) {
    let transport = hp(O::WorldUnit, 0, &[get::<i32>(m, 1) as i64]);
    let cargo = hp(O::WorldUnit, 0, &[get::<i32>(m, 2) as i64]);
    let mut text: c_long = 0;
    if transport != 0
        && cargo != 0
        && uf(transport).mHitPoints > 0
        && !cf(complex(transport)).mIsDisabled
        && h(O::CxNumCargoUnits, complex(transport), &[]) > 0
        && h(O::CxHasCargo, hp(O::UnGetComplex, transport, &[]), &[pa(hp(O::UnGetComplex, cargo, &[]))]) != 0
        && h(O::CxUnloadCargoNow, hp(O::UnGetComplex, transport, &[]), &[pa(hp(O::UnGetComplex, cargo, &[]))]) == 0
    {
        if h(O::UcIsTransport, unit_class(transport), &[]) != 0 {
            text = h(O::UcUnloadFailedText, unit_class(transport), &[]) as c_long;
        }
        let (x, y) = (rts(transport).mTileX, rts(transport).mTileY);
        let owner = hp(O::UnPlayer, transport, &[]);
        h(O::PlAddEvent, owner, &[x as i64, y as i64, 2, pa(owner), text as i64, 0]);
    }
}

/// With the flag set, the graphics thread restarted and waited for; otherwise paused, the game (or scenario editor)
/// screen shown and the server answered with a load-main-screen response.
fn load_main_screen(m: P) {
    if get::<u8>(m, 1) != 0 {
        super::globals::graphics_post_resume();
        loop {
            if h(O::GfxIsPaused, 0, &[]) == 0 {
                break;
            }
            ee_base::plat_core::time::PlatSleep(1);
        }
    } else {
        super::globals::graphics_post_pause();
        let mut started = h(O::GfxIsPaused, 0, &[]) != 0;
        while !started {
            ee_base::plat_core::time::PlatSleep(1);
            started = h(O::GfxIsPaused, 0, &[]) != 0;
        }
        if *wm::<bool>(O::WGameScreenActive) {
            h(O::UiSwapToMainGame, 0, &[]);
            h(O::MainFormSetInfoBlinking, 0, &[0]);
            h(O::MainFormSetButtonBlinking, 0, &[0]);
            h(O::MainFormSetUiState, 0, &[0x11]);
        } else {
            h(O::UiSwapToScenarioEditor, 0, &[]);
        }
        hf(O::CameraUpdate, 0, &[], &[1.0]);
        h(O::CommSendLoadMainScreenResponse, 0, &[]);
    }
}

/// A single-player server's load: the saved game loaded and the response logged; anything else queues the save
/// with the physics thread; the chat log told, and the error box shown if loading failed.
fn load_or_save_game(m: P) {
    let text = new_str();
    let format = new_str();
    let result;
    if get::<u8>(m, 1) != 0 && h(O::CommIsServer, 0, &[]) != 0 && h(O::CommIsMultiplayer, 0, &[]) == 0 {
        res(&format, IDS_LOADING);
        h(O::StrFormatS, text.0, &[pa(format.0), pa(hp(O::MsgString, m, &[]))]);
        chat(text.0, 0);
        result = h(O::MsgLoadSavedGame, m, &[get_bool(m, 2) as i64]);
        let response = hp(O::NewLoadSaveResponse, 0, &[get::<i32>(m, 3) as i64]);
        h(O::MsgSetOrigin, response, &[prts(current()).mSlot as i64]);
        ee_net::net::ffi_server::ee_net_sv_log_response(super::net::global().server, response as *mut c_void);
    } else {
        // SAFETY: the game's physics
        crate::physics::ee_physics_queue_message(
            unsafe { *std::ptr::addr_of!(super::globals::gPhysics) },
            m as *mut c_void,
        );
        res(&format, IDS_SAVING_GAME_AS);
        h(O::StrFormatS, text.0, &[pa(format.0), pa(hp(O::MsgString, m, &[]))]);
        chat(text.0, 0);
        result = 0;
    }
    if result != 0 {
        h(O::UiShowLoadSaveError, 0, &[]);
    }
}

/// The sender's cheat unit class changed: the old class's data unloaded for every player and the new one preloaded.
fn load_unit_data(m: P) {
    let pl = sender(m);
    let class = get::<i32>(m, 1) as c_ulong;
    if pfields(pl).mHackUnitClass != class {
        let preload = |op: O, pl: P| {
            for i in 1..num_players() {
                let hack = pfields(pl).mHackUnitClass;
                if (hack as i64) < h(O::UnitClassCount, 0, &[]) && hp(O::UcAt, 0, &[hack as i64]) != 0 {
                    h(op, 0, &[prts(world_player(i as i64)).mPlayerID as i64, hack as i64]);
                }
            }
        };
        preload(O::PreloadUnloadClass, pl);
        h(O::PlSetHackUnitClass, pl, &[get::<i32>(m, 1) as i64]);
        preload(O::PreloadLoadClass, pl);
    }
}

/// After a host migration, a player moved to its new slot; one left without a slot announced as dropped and (unless
/// observing) removed as defeated; the local client told a new host is set.
fn migration_slot_info(m: P) {
    let text = new_str();
    let format = new_str();
    let index = get::<u8>(m, 1) as i64;
    if get::<u8>(m, 2) == MSG_SLOT_NONE {
        let slot = prts(world_player(index)).mSlot;
        if slot != MSG_SLOT_NONE && slot != MSG_SLOT_SERVER {
            res(&format, IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME);
            h(O::StrFormatS, text.0, &[pa(format.0), pa(hp(O::PlName, world_player(index), &[]))]);
            chat(text.0, 1);
        }
        if !prts(world_player(index)).mObserverMode {
            h(O::WRemovePlayerFromGame, world(), &[0, index]);
            set_game_result(world_player(index), 2);
        }
    }
    prts(world_player(index)).mSlot = get::<u8>(m, 2);
    if index as c_ulong == prts(current()).mPlayerID && super::net::global().is_client() {
        res(&text, IDS_NEW_HOST_ESTABLISHED);
        chat(text.0, 0);
    }
}

/// The sender's saved group selected; for the local player the command panel reset, the camera centred on it when
/// asked and its selection sound played.
fn saved_group_select(m: P) {
    let mut location = Ge3DPoint::default();
    let pl = sender(m);
    let main_group = hp(O::MainFormGroup0, 0, &[]);
    let slot = h(O::MsgOrigin, m, &[]);
    let local_slot = h(O::PlGetSlot, current(), &[]);
    h(
        O::SelGroupSelectSaved,
        hp(O::PlActiveGroup, pl, &[]),
        &[pa(hp(O::PlSavedGroupAt, pl, &[get::<u8>(m, 1) as i64])), 1],
    );
    if slot == local_slot && *wm::<bool>(O::WExecutingLocalSelection) {
        h(O::MouseSetPointer, 0, &[0]);
        h(O::GameUiShowCommands, main_group, &[]);
        h(O::MainGroupReset, main_group, &[]);
        h(O::MainFormSetUiState, 0, &[0]);
        h(O::MainFormSetCursorLocked, 0, &[0]);
        let unit = hp(O::SelGroupFirstUnit, hp(O::PlSelectedGroup, pl, &[]), &[]);
        if unit != 0 {
            if get_bool(m, 2) {
                h(O::UnPosition, unit, &[ptr(&mut location)]);
                h(O::CameraSetChanged, 0, &[]);
                h(O::CameraSetLocation, 0, &[cptr(&location), 0, 1]);
                h(O::MiniMapRefresh, 0, &[]);
                h(O::SetCameraFollowing, 0, &[0]);
            }
            if *wm::<bool>(O::WExecutingLocalSelection) {
                h(O::PlayForLocalSelectionSound, pl, &[pa(unit)]);
            }
        }
    }
}

/// A flare shown at the tile to the sender and each ally, and each ally told it received one.
fn send_flare(m: P) {
    let pl = sender(m);
    let allies = Owned::new(O::VecPlayersNew, O::VecPlayersDelete);
    h(O::PlCopyAllies, pl, &[pa(allies.0)]);
    let (x, y) = (get::<i32>(m, 1) as i64, get::<i32>(m, 2) as i64);
    h(O::PlAddEvent, pl, &[x, y, 3, pa(pl), -1, 1]);
    for k in 0..h(O::VecPlayersSize, allies.0, &[]) {
        let ally = hp(O::VecPlayersAt, allies.0, &[k]);
        h(O::PlAddEvent, ally, &[x, y, 3, pa(ally), -1, 1]);
        h(O::PlFlareReceived, ally, &[pa(pl), x, y]);
    }
}

/// The sender's stance toward a player set, and "<name> changes diplomatic stance to" friendly (stance 0) or
/// hostile posted to the chat log.
fn set_diplomatic_stance(m: P) {
    let ally = new_str();
    let enemy = new_str();
    let is_now = new_str();
    let text = Owned::new(O::StrNew1K, O::StrFree);
    let pl = sender(m);
    h(
        O::WSetDiplomaticStanceCall,
        world(),
        &[prts(pl).mPlayerID as i64, get::<i32>(m, 1) as i64, get::<i32>(m, 2) as i64],
    );
    res(&ally, IDS_HOSTILE);
    res(&enemy, IDS_FRIENDLY);
    res(&is_now, IDS_CHANGES_DIPLOMATIC_STANCE_TO);
    let stance = if get::<i32>(m, 2) == 0 { enemy.0 } else { ally.0 };
    h(O::StrFormatSSS, text.0, &[pa(hp(O::PlName, pl, &[])), pa(is_now.0), pa(stance)]);
    h(O::UiAddChatPlayerLine, text.0, &[pa(pl)]);
}

/// The loading started: the random generator seeded, the human player count and the multiplayer flag set (members:
/// the humans, the seed). EEStartLoading (C++) runs it: the legacy harness's seed pin breaks there.
#[no_mangle]
pub extern "C" fn ee_gamecpp_msg_start_loading(m: *mut c_void) {
    let m = m as P;
    if h(O::SimSeedSet, 0, &[]) != 0 {
        // TEST ONLY: the headless simulation's seed (platform/PlatSim.h; the harness's --seed pin)
        let seed = h(O::SimSeed, 0, &[]) as i32;
        // SAFETY: the record's member 2 (an int32)
        unsafe { (hp(O::MsgField, m, &[2, 4]) as *mut i32).write_unaligned(seed) };
    }
    h(O::LobbyReadLock, 0, &[]);
    h(O::LobbyReadUnlock, 0, &[]);
    h(O::PhysSetSeed, 0, &[get::<i32>(m, 2) as i64]);
    h(O::UiSetRandomSeed, 0, &[get::<i32>(m, 2) as i64]);
    let comms = super::net::global();
    comms.set(EeNetCommsField::NumHumanPlayers, get::<u8>(m, 1) as u32);
    comms.set(EeNetCommsField::IsMultiplayerSession, (get::<u8>(m, 1) > 1) as u32);
    crate::screens::ui_objects::ui_post_start_loading();
}

/// The improvement queued for the unit's type if its owner can buy it; otherwise the local owner hears an error
/// click and sees the reason in the chat log.
fn improve_unit(m: P) {
    let text = new_str();
    let unit = hp(O::WorldUnit, 0, &[get::<i32>(m, 1) as i64]);
    if unit != 0 && uf(unit).mHitPoints > 0 {
        let owner = hp(O::UnPlayer, unit, &[]);
        let kind = get::<i32>(m, 2) as i64;
        let unit_type = ucf(unit_class(unit)).mUnitType as i64;
        if h(O::PlCanBuy, owner, &[unit_type, kind, pa(text.0)]) != 0 {
            h(O::PlQueueImprovement, owner, &[ucf(unit_class(unit)).mUnitType as i64, kind]);
        } else {
            if owner == current() {
                let button = hp(O::SoundObjectGet, 0, &[SOUND_BUTTON2]);
                hf(O::SoPlayOnce, button, &[], &[1.0, 0.0, 1.0]);
            }
            if h(O::StrLength, text.0, &[]) > 0 && owner == current() {
                chat(text.0, 0);
            }
        }
    }
}

/// The record type's body the C++ kept, by its EE_NETMSG_BODY_* index (ee_sim netmsg bodies.rs).
#[no_mangle]
pub extern "C" fn ee_gamecpp_msg_body(index: i64, m: *mut c_void) {
    let m = m as P;
    match index {
        EE_NETMSG_BODY_CIV_UPGRADES_PURCHASED => civ_upgrades_purchased(m),
        EE_NETMSG_BODY_CIV_UPGRADES_REQUESTED => civ_upgrades_requested(m),
        EE_NETMSG_BODY_IMMEDIATE_CARGO_UNLOAD => immediate_cargo_unload(m),
        EE_NETMSG_BODY_IMPROVE_UNIT => improve_unit(m),
        EE_NETMSG_BODY_LOAD_MAIN_SCREEN => load_main_screen(m),
        EE_NETMSG_BODY_LOAD_OR_SAVE_GAME => load_or_save_game(m),
        EE_NETMSG_BODY_LOAD_UNIT_DATA => load_unit_data(m),
        EE_NETMSG_BODY_MIGRATION_SLOT_INFO => migration_slot_info(m),
        EE_NETMSG_BODY_SAVED_GROUP_SELECT => saved_group_select(m),
        EE_NETMSG_BODY_SEND_FLARE => send_flare(m),
        EE_NETMSG_BODY_SET_DIPLOMATIC_STANCE => set_diplomatic_stance(m),
        11 => {
            // EE_NETMSG_BODY_START_LOADING: through EEStartLoading (the harness's breakpoint)
            h(O::MsgStartLoading, m, &[]);
        }
        _ => change_formation(m),
    }
}

/// The saved game's request (the old EEMLoadOrSaveGame::LoadOrSave; EEPhysics::ServeLoadSaveRequests): the game
/// saved under the requested name, the auto-saves rotated for an auto-save, the mouse restored and an error box
/// shown on failure; the request's tick.
#[no_mangle]
pub extern "C" fn ee_gamecpp_msg_load_or_save(m: *mut c_void) -> c_long {
    let m = m as P;
    let mut position = [0f32; 2];
    h(O::MouseGetPosition, 0, &[ptr(&mut position)]);
    h(O::IsMouseBuffered, 0, &[0]);
    let result = h(O::MsgSaveGame, m, &[get::<i32>(m, 4) as i64, get_bool(m, 2) as i64]);
    if get::<u8>(m, 5) == 1 {
        h(O::TrimAutoSaves, 0, &[]);
    }
    if h(O::CommIsServer, 0, &[]) != 0 && !get_bool(m, 2) && result == 0 {
        h(O::UiCloseForm, 0, &[]);
    }
    h(O::IsMouseRefresh, 0, &[]);
    h(O::IsMouseBuffered, 0, &[1]);
    h(O::MouseUpdate, 0, &[]);
    h(O::MouseSetPosition, 0, &[cptr(&position)]);
    if result != 0 {
        h(O::UiShowLoadSaveError, 0, &[]);
    }
    get::<i32>(m, 3) as c_long
}

// ---- the messages' host's actions (EENetMsgHost's EE_NETMSG_OP_*; i the host's integers, f its floats)

/// EE_NETMSG_OP_WORLD_ACT: the lobby's, the loading's, the chat's and the world's one calls (a the sender, b the
/// local player where the act names them).
///
/// # Safety
/// `i` points to the host call's integers, `f` to its floats.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_msg_world_act(
    m: *mut c_void,
    i: *const i64,
    f: *const f32,
    a: *mut c_void,
    b: *mut c_void,
) {
    let (m, a, b) = (m as P, a as P, b as P);
    // SAFETY: the host's arrays (at least five integers and two floats)
    let (i, f) = unsafe { (std::slice::from_raw_parts(i, 8), std::slice::from_raw_parts(f, 2)) };
    match i[0] {
        EE_NETMSG_WACT_ATTACK_PLAYERS => {
            super::startup::ee_gamecpp_startup_attack_players(i[1] as c_long, i[2] as u8);
        }
        EE_NETMSG_WACT_BUILD_GATE => {
            // the EETileCoordinate (two longs) the C++ takes by reference
            let mut tile: [c_long; 2] = [f[0] as c_long, f[1] as c_long];
            h(O::BuildGate, 0, &[ptr(&mut tile), i[1], i[2], i[3]]);
        }
        EE_NETMSG_WACT_EDICT => {
            h(O::VotingPropose, 0, &[i[1] as u16 as i64]);
            h(O::PhysClearF84, 0, &[]);
        }
        EE_NETMSG_WACT_END_CINEMATIC => {
            h(O::ScenarioEndCinematic, 0, &[]);
        }
        EE_NETMSG_WACT_GAME_SPEED => {
            hf(O::WSetGameSpeed, world(), &[], &[f[0]]);
        }
        EE_NETMSG_WACT_MIGRATING_OFF => {
            // the host migration over as the game restarts
            if super::net::present() && super::net::global().is_client() {
                let comms = super::globals::get!(gCommunications) as P;
                // SAFETY: the live client's plain tail
                unsafe { (*rec::<EeGamecppClientTail>(O::C11ClTail, comms)).is_migrating = false };
            } else if super::net::present() {
                server_tail().is_migrating = false;
            }
        }
        EE_NETMSG_WACT_LAG => {
            // SAFETY: the world's member
            unsafe { *rec::<u16>(O::WLagStatus, world()) = i[1] as u16 };
            h(O::UiSetLagStatus, 0, &[i[1] as u16 as i64, i[2] as u8 as i64]);
        }
        EE_NETMSG_WACT_LOAD_PERCENT => {
            h(O::UiSetLoadPercent, 0, &[i[1] as i32 as i64, i[2] as u32 as i64]);
        }
        EE_NETMSG_WACT_POST_PHYSICS => {
            h(O::PhysPost, 0, &[i[1]]);
        }
        EE_NETMSG_WACT_COLLISION_UNIT => {
            super::globals::set!(gCollisionDetectionUnit, i[1] as std::ffi::c_ulong);
        }
        EE_NETMSG_WACT_COLLISION_FLAGS => {
            // SAFETY: the world's member
            let flags = unsafe { &mut *rec::<c_long>(O::WCollisionFlags, world()) };
            *flags = (flags.wrapping_add(1)) % 16;
        }
        EE_NETMSG_WACT_ZOOM => {
            hf(O::CameraSetZoom, 0, &[], &[f[0]]);
        }
        EE_NETMSG_WACT_RECORD_FRAMES => {
            // slow or normal playback
            h(O::GfxPostSetFlag5B, 0, &[(i[1] != 0) as i64]);
            h(O::PhysSetSlowPlayback, 0, &[(i[1] != 0) as i64]);
            if super::net::global().is_client() && i[1] != 0 {
                ee_net::net::ffi_client::ee_net_cl_set_slow_playback(super::net::global().client);
            } else if super::net::global().is_client() {
                ee_net::net::ffi_client::ee_net_cl_set_normal_playback(super::net::global().client);
            }
        }
        EE_NETMSG_WACT_OPTIONS_LOCK => {
            if i[1] != 0 {
                h(O::LobbyWriteLock, 0, &[]);
            } else {
                h(O::LobbyWriteUnlock, 0, &[]);
                h(O::UiPostLobbyUpdateGameOptions, 0, &[]);
            }
        }
        EE_NETMSG_WACT_PLAYER_INFO => {
            // lobby slot i1's field i2 (6 a computer's engine) set to i3, the ping f0
            let slot = Owned::new(O::OptionsPlayerNew, O::OptionsPlayerDelete);
            let value = i[3] as u32;
            h(O::LobbyWriteLock, 0, &[]);
            h(O::OptPlayerAssign, slot.0, &[pa(hp(O::LobbyOptPlayer, 0, &[i[1]]))]);
            // SAFETY: the slot's plain members
            let s =
                unsafe { &mut *rec::<crate::app::game_options::EeAppOptionsPlayerFields>(O::OptPlayerFields, slot.0) };
            match i[2] {
                6 => {
                    s.mEngine = if (value as i64) < crate::cp_engine::ee_cp_engine_choice_count() as c_ulong as i64 {
                        value as c_ulong
                    } else {
                        ENGINE_CLASSIC
                    };
                }
                5 => s.mIsReady = value > 0,
                4 => s.mStartingCitizens = value as c_ulong,
                3 => s.mTeam = value as c_ulong,
                2 => s.mColor = value as c_ulong,
                1 => s.mPing = f[0],
                0 => s.mType = value as c_ulong,
                _ => {}
            }
            h(O::OptPlayerAssign, hp(O::LobbyOptPlayer, 0, &[i[1]]), &[pa(slot.0)]);
            h(O::LobbyWriteUnlock, 0, &[]);
            h(O::UiPostLobbyUpdatePlayer, 0, &[i[1], i[2]]);
        }
        EE_NETMSG_WACT_POST_CHAT => {
            let action = hp(O::NewChatAction, m, &[(i[1] != 0) as i64]);
            h(O::UiPostAction, action, &[]);
            h(O::ActionDereference, action, &[]);
        }
        EE_NETMSG_WACT_TIMER_EXPIRED => {
            let event = hp(O::NewTimerExpiredEvent, 0, &[]);
            h(O::UiPostEvent, event, &[]);
            h(O::EventDereference, event, &[]);
        }
        EE_NETMSG_WACT_START_GAME => {
            // every human has shown the main screen: the start at the offset time
            let time = ee_base::time::ee_time_get() as i64;
            let message = hp(O::NewStartGameRecord, 0, &[time]);
            server_tail().load_state = 0;
            super::net::global().local_input(message as *mut c_void, MSG_SLOT_NONE);
            super::net::global().sv_set(EeNetSvSet::GameState, 2);
        }
        EE_NETMSG_WACT_CLEAR_SEND_TO_SLOTS => {
            ee_net::net::ffi_server::ee_net_sv_clear_send_to_slot_array(super::net::global().server);
        }
        EE_NETMSG_WACT_LOAD_RESPONSES => {
            server_tail().num_load_responses = i[1] as u8;
        }
        EE_NETMSG_WACT_SEND_TO_SLOT => {
            let to = super::session_objs::ee_gamecpp_server_send_to_slot(i[2] as u8) as i64;
            let server = super::net::global().server;
            ee_net::net::ffi_server::ee_net_sv_set_send_to_slot_for_player(server, i[1] as u8, to as u8);
        }
        EE_NETMSG_WACT_ALL_LOADED => {
            // every human has loaded: the slots sent, the main screen shown everywhere
            h(O::ServerSendPlayerSlotInfo, 0, &[]);
            server_tail().load_state = 2;
            let message = hp(O::NewLoadMainScreenRecord, 0, &[]);
            super::net::global().local_input(message as *mut c_void, MSG_SLOT_NONE);
        }
        EE_NETMSG_WACT_CHAT_LOG => {
            h(O::GameLogChat, m, &[prts(a).mPlayerID as i64]);
        }
        EE_NETMSG_WACT_SHOW_CHAT => {
            // the sender a's chat target's text (an observer's: the observers'), the line
            let kind = new_str();
            for k in 1..=2 {
                if i[k] >= 0 {
                    h(O::ResChatText, kind.0, &[i[k]]);
                }
            }
            if h(O::StrLength, kind.0, &[]) != 0 {
                h(O::StrAppendSpace, kind.0, &[]);
            }
            h(O::UiAddChatMsgLine, kind.0, &[pa(a), pa(m)]);
            let sound = if a != b { hp(O::SoundObjectGet, 0, &[SOUND_INCOMING_CHAT]) } else { 0 };
            if sound != 0 {
                h(O::SoundObjectPlay, sound, &[]);
            }
        }
        EE_NETMSG_WACT_LAST_CHAT => {
            h(O::ScenarioSetLastChat, m, &[]);
        }
        EE_NETMSG_WACT_TRIBUTE_TEXT => {
            // "<you tribute> <to> " (viewer 1) or "<from> <tributes you> ", the amounts
            let you = new_str();
            let name = new_str();
            let text = Owned::new(O::StrNew1K, O::StrFree);
            res(&you, if i[1] == 1 { IDS_YOU_TRIBUTE } else { IDS_TRIBUTES_YOU });
            if i[1] == 1 {
                h(O::StrFormatSS, text.0, &[pa(you.0), pa(hp(O::PlName, b, &[]))]);
            } else {
                h(O::StrFormatSS, text.0, &[pa(hp(O::PlName, a, &[])), pa(you.0)]);
            }
            for k in 0..i[2] {
                let r = ((i[3] >> (8 * k)) & 0xff) as usize;
                // wood's name is carbon's on the carbon maps
                let carbon = *wm::<bool>(O::WUsingCarbon);
                res(&name, if r == 3 && carbon { IDS_CARBON } else { RESOURCE_NAMES[r] });
                // PlatIToA's decimal digits
                let number = format!("{}\0", i[4 + r - 2] as u32);
                h(O::StrAppendC, text.0, &[number.as_ptr() as usize as i64]);
                h(O::StrAppendC, text.0, &[c" ".as_ptr() as usize as i64]);
                h(O::StrAppendStr, text.0, &[pa(name.0)]);
                h(O::StrAppendC, text.0, &[c" ".as_ptr() as usize as i64]);
            }
            h(O::UiAddChatPlayerLine, text.0, &[pa(a)]);
        }
        EE_NETMSG_WACT_RESIGN_TEXT => {
            // "<name> has resigned" for the others
            let text = new_str();
            let format = new_str();
            res(&format, IDS_PLAYER_HAS_RESIGNED_FROM_GAME);
            h(O::StrFormatS, text.0, &[pa(format.0), pa(hp(O::PlName, a, &[]))]);
            chat_game_line(text.0);
        }
        EE_NETMSG_WACT_FLUSH => {
            // SAFETY: the game's physics
            crate::physics::ee_physics_flush_world(unsafe { *std::ptr::addr_of!(super::globals::gPhysics) });
        }
        _ => {}
    }
}

/// EE_NETMSG_OP_CHAT_TEXT: text `i0` of the chat table posted as kind `i1` (when `i2`, only a non-empty one).
#[no_mangle]
pub extern "C" fn ee_gamecpp_msg_chat_text(text: i64, kind: i64, nonempty: i64) {
    let s = new_str();
    h(O::ResChatText, s.0, &[text]);
    if nonempty == 0 || h(O::StrLength, s.0, &[]) > 0 {
        chat(s.0, kind);
    }
}

/// EE_NETMSG_OP_SOUND: sound `index` of the host's table played, if it is loaded.
#[no_mangle]
pub extern "C" fn ee_gamecpp_msg_sound(index: i64) {
    let sound = hp(O::SoundObjectGet, 0, &[SOUNDS[index as usize]]);
    if sound != 0 {
        h(O::SoundObjectPlay, sound, &[]);
    }
}

/// EE_NETMSG_OP_SELECT_LIST: the ids added to and removed from (0), added to (1) or made (2) the player's selection.
///
/// # Safety
/// `ids` points to `count` ids.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_msg_select_list(pl: *mut c_void, how: i64, ids: *const i32, count: i64) {
    // the ids as the player's std::vector<long> held them
    // SAFETY: the caller's `count` ids
    let list: Vec<c_long> =
        unsafe { std::slice::from_raw_parts(ids, count as usize) }.iter().map(|&id| id as c_long).collect();
    // AddAndRemoveUnitsFromSelectedGroup, AddUnitsToSelectedGroup, SelectUnits
    let op = match how {
        0 => 5,
        1 => 4,
        _ => 6,
    };
    // SAFETY: the live player and the list
    unsafe { ee_units_selection(pl, op, core::ptr::null_mut(), 0, list.as_ptr(), list.len(), false, false) };
}

/// EE_NETMSG_OP_SELECT: one unit removed from (0), added to (1) or made (2) the selection, or none selected.
#[no_mangle]
pub extern "C" fn ee_gamecpp_msg_select(pl: *mut c_void, how: i64, id: i64) {
    let pl = pl as P;
    // RemoveUnitFromSelectedGroup's long id, AddUnitToSelectedGroup's and SelectUnit's unsigned long
    match how {
        0 => select(pl, 8, id as c_long as i64, false, false),
        1 => select(pl, 3, id as c_ulong as i64, true, false),
        2 => select(pl, 2, id as c_ulong as i64, true, true),
        _ => unselect_all(pl),
    }
}

/// EEPlayer's one-unit selection members by id (ee_sim group_host.rs `ee_units_selection`).
fn select(pl: P, op: i32, id: i64, flag: bool, flag2: bool) {
    // SAFETY: the live player, no unit, no list
    unsafe { ee_units_selection(pl as *mut c_void, op, core::ptr::null_mut(), id, core::ptr::null(), 0, flag, flag2) };
}

/// EEPlayer::SetGameResult.
fn set_game_result(pl: P, state: i64) {
    // SAFETY: the live player
    unsafe {
        ee_units_player_rest(pl as *mut c_void, EE_UNITS_PLAYER_REST_SET_GAME_RESULT, 0, state as c_long as i64, 0)
    };
}

/// The player's member `what` with one argument (EEPlayer's one-call members over ee_sim player_body.rs).
///
/// # Safety
/// `pl` is a live player.
unsafe fn player_body(pl: P, what: u32, a: u64) {
    unsafe { ee_units_player_body(pl as *mut c_void, what, a, 0, 0, 0.0) };
}

/// EEPlayer::UnselectAllUnits.
fn unselect_all(pl: P) {
    // SAFETY: the live player
    unsafe { player_body(pl, EE_UNITS_PLAYER_DO_UNSELECT_ALL, 0) };
}

/// EE_NETMSG_OP_RESOURCE: a resource added (0), taken (1), traded (2) or a tribute recorded.
#[no_mangle]
pub extern "C" fn ee_gamecpp_msg_resource(pl: *mut c_void, how: i64, r: i64, amount: i64, from: i64) {
    let pl = pl as P;
    match how {
        0 => {
            h(O::PlAddResource, pl, &[r, amount]);
        }
        1 => {
            h(O::PlTakeResource, pl, &[r, amount]);
        }
        2 => {
            let trade = &mut psaved(pl).mResourceTrade[(r - 2) as usize];
            *trade = trade.wrapping_add(amount as c_long);
        }
        _ => {
            h(O::PlAddTribute, pl, &[from, r, amount]);
        }
    }
}

/// EE_NETMSG_OP_DISBAND: the unit's technology refunded at the factor, the disband emitter played and the unit
/// killed.
#[no_mangle]
pub extern "C" fn ee_gamecpp_msg_disband(pl: *mut c_void, u: *mut c_void, factor: f32) {
    let (pl, u) = (pl as P, u as P);
    let class = ucf(unit_class(u));
    let (technology, unit_type) = (class.mTechnology, class.mUnitType);
    let tech = hp(O::PlTechnology, pl, &[technology as i64]);
    hf(O::PlRefundTechnology, pl, &[pa(tech), unit_type as i64], &[factor]);
    let effect = hp(O::GfxEffectGet, 0, &[EFFECT_DISBAND]);
    h(O::EffUnit, u, &[pa(effect), 0, 1, 0, 0]);
    h(O::UnKill, u, &[0, 0, 0]);
}

/// EE_NETMSG_OP_DELETE_REFUND: a deleted unit's technology refunded at the delete refund factor.
#[no_mangle]
pub extern "C" fn ee_gamecpp_msg_delete_refund(u: *mut c_void) {
    let u = u as P;
    let tech = hp(O::UnTreeTechnology, u, &[]);
    if tech != 0 {
        h(O::PlRefundDelete, hp(O::UnPlayer, u, &[]), &[pa(tech)]);
    }
}

/// EE_NETMSG_OP_UNIT_ACT: a unit's one action (`class` the unit class or player the act names, `u` the unit).
///
/// # Safety
/// `i` points to the host call's integers.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_msg_unit_act(i: *const i64, a: *mut c_void, u: *mut c_void) {
    let (a, u) = (a as P, u as P);
    // SAFETY: the host's integers
    let i = unsafe { std::slice::from_raw_parts(i, 3) };
    match i[0] {
        EE_NETMSG_ACT_HALT => {
            h(O::CxClearGoals, u, &[0]);
            h(O::CxStartAction, u, &[MSG_ACTION_IDLE]);
            h(O::UcActionChanged, unit_class(u), &[pa(u)]);
        }
        EE_NETMSG_ACT_GO_FANATIC => {
            h(O::CxGoFanatic, u, &[]);
        }
        EE_NETMSG_ACT_JIT => {
            h(O::CxToggleJit, u, &[]);
            h(O::CxUpdateJit, u, &[]);
        }
        EE_NETMSG_ACT_JIT_TOGGLE => {
            h(O::CxToggleJit, u, &[]);
        }
        EE_NETMSG_ACT_REMOVE_RESEARCH => {
            h(O::CxRemoveResearch, u, &[i[1]]);
        }
        EE_NETMSG_ACT_LOCK_GATE => {
            // a gate built as one piece (scenario Create effect) has no second or fourth part
            let gate = hp(O::UcAsGate, unit_class(u), &[]);
            h(O::CxSetLocked, u, &[(i[1] != 0) as i64]);
            let part = hp(O::GateSecondPart, gate, &[pa(u)]);
            if part != 0 {
                h(O::CxSetLocked, part, &[(i[1] != 0) as i64]);
            }
            let part = hp(O::GateFourthPart, gate, &[pa(u)]);
            if part != 0 {
                h(O::CxSetLocked, part, &[(i[1] != 0) as i64]);
            }
        }
        EE_NETMSG_ACT_RALLY_TIME => {
            if h(O::CxQueueSize, u, &[i[1]]) > 0 {
                h(O::CxQueueAddTime, u, &[i[1], 60000]);
            }
        }
        EE_NETMSG_ACT_BEHAVIOR => {
            let behavior = hp(O::BehaviorAt, 0, &[i[1]]);
            if hp(O::CxBehavior, u, &[]) != behavior {
                h(O::CxSetBehavior, u, &[pa(behavior)]);
            }
        }
        EE_NETMSG_ACT_REPLANT => {
            h(O::UcReplantFarm, unit_class(u), &[pa(u), pa(a)]);
        }
        EE_NETMSG_ACT_HOME_BASE => {
            h(O::CxSetHomeBase, u, &[pa(a)]);
        }
        EE_NETMSG_ACT_LAND => {
            h(O::CxSetFlightTime, u, &[0]);
        }
        EE_NETMSG_ACT_SCRAMBLE => {
            let goal = hp(O::NewLocationGoal2D, 0, &[i[1] as c_long as i64, i[2] as c_long as i64]);
            h(O::CxClearAllGoalQueues, u, &[]);
            h(O::CxQueueGoal, u, &[pa(goal), 0]);
            h(O::CxQueueGoal, u, &[pa(hp(O::GoalClone, goal, &[])), 1]);
            h(O::CxQueueAddTime, u, &[1, 1000]);
        }
        EE_NETMSG_ACT_PACK => {
            let new = hp(O::ReplaceUnit, u, &[i[1], pa(a)]);
            h(O::CxSetIsPacked, new, &[1]);
        }
        EE_NETMSG_ACT_UNPACK => {
            let new = hp(O::ReplaceUnit, u, &[i[1], pa(a)]);
            let (x, y) = (rts(u).mTileX, rts(u).mTileY);
            h(O::PlaceUnit, new, &[pa(unit_class(new)), i[2], x as i64, y as i64]);
            h(O::CxSetIsPacked, new, &[0]);
        }
        EE_NETMSG_ACT_SPHERE => {
            // grown or shrunk by 0.05, the inner (mode 9) or the outer (10)
            let class = ucf(unit_class(u));
            let step = if i[2] != 0 { 0.05f32 } else { -0.05f32 };
            if i[1] == 9 {
                class.mInnerCollisionSphere += step;
            } else {
                class.mOuterCollisionSphere += step;
            }
            let inner = class.mInnerCollisionSphere;
            cf(u).mCollisionRadius = inner;
        }
        EE_NETMSG_ACT_RADIUS => {
            cf(u).mCollisionRadius = ucf(unit_class(a)).mInnerCollisionSphere;
        }
        _ => {}
    }
}

/// EE_NETMSG_OP_PLAYER_ACT: a player's one action.
///
/// # Safety
/// `i` points to the host call's integers.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_msg_player_act(i: *const i64, pl: *mut c_void) {
    let pl = pl as P;
    // SAFETY: the host's integers
    let i = unsafe { std::slice::from_raw_parts(i, 3) };
    match i[0] {
        EE_NETMSG_PACT_SHIFT_MODE => {
            pfields(pl).mShiftMode = i[1] != 0;
            if pl == current() {
                h(O::MouseSetShiftMode, 0, &[(i[1] != 0) as i64]);
            }
        }
        EE_NETMSG_PACT_SEES_ALL => {
            prts(pl).mSeesAll = !prts(pl).mSeesAll;
            if pl == current() {
                h(O::MiniMapRedraw, 0, &[]);
            }
        }
        EE_NETMSG_PACT_INCREMENT_EPIC => {
            h(O::PlIncrementEpic, pl, &[]);
        }
        EE_NETMSG_PACT_JIT | EE_NETMSG_PACT_UPDATE_JIT => {
            // (JIT falls through)
            if i[0] == EE_NETMSG_PACT_JIT {
                pfields(pl).mJITManufacturing = !pfields(pl).mJITManufacturing;
            }
            if pl == current() {
                h(O::UiUpdateJit, 0, &[]);
            }
        }
        EE_NETMSG_PACT_CHEAT => {
            h(O::CheatApply, 0, &[i[1], prts(pl).mPlayerID as i64, i[2]]);
        }
        EE_NETMSG_PACT_CHAT_TYPE => psaved(pl).mChatType = i[1] as c_ulong,
        EE_NETMSG_PACT_SEND_CHAT => psaved(pl).mSendChatMessage[i[1] as usize] = i[2] != 0,
        EE_NETMSG_PACT_REPORT => {
            h(O::PlSavedGroupsSet, pl, &[0x14, i[1]]);
            h(O::PlSavedGroupsSet, pl, &[0x15, i[2]]);
        }
        EE_NETMSG_PACT_CLEAR_SAVED => {
            // SAFETY: the live player (EEPlayer::ClearSavedGroup)
            unsafe { player_body(pl, EE_UNITS_PLAYER_DO_CLEAR_SAVED_GROUP, i[1] as u8 as u64) };
        }
        EE_NETMSG_PACT_ASSIGN_SAVED => {
            h(O::PlAssignSavedGroup, pl, &[i[1]]);
        }
        EE_NETMSG_PACT_ACKNOWLEDGE => {
            h(O::PlPlayAcknowledgement, pl, &[i[1]]);
        }
        EE_NETMSG_PACT_LOCK_CIV => psaved(pl).mCivilizationLocked = true,
        EE_NETMSG_PACT_REMOVE => {
            h(O::WRemovePlayerFromGame, world(), &[0, prts(pl).mPlayerID as i64]);
            set_game_result(pl, 2);
        }
        EE_NETMSG_PACT_REMOVE_IMPROVEMENT => {
            let queued = h(O::PlCountQueuedImprovements, pl, &[i[1], i[2]]) != 0;
            h(O::PlRemoveQueuedImprovement, pl, &[i[1], i[2], queued as i64]);
        }
        EE_NETMSG_PACT_UPDATE_HELP => {
            let index = *wm::<c_long>(O::WCurrentPlayer) as i64;
            if prts(world_player(index)).mPlayerID == prts(pl).mPlayerID {
                h(O::MainFormUpdateHelp, 0, &[]);
            }
        }
        EE_NETMSG_PACT_PATROL_GROUP => {
            let unit = hp(O::UnCastComplex, hp(O::WorldUnit, 0, &[i[1]]), &[]);
            if unit != 0 {
                h(O::PatrolGroupSelect, unit, &[pa(pl)]);
            }
        }
        _ => {}
    }
}

/// `gCommunications`' fast-selection id and its 16 last ids (the server's or the client's plain tail).
fn fast_selection(server: bool) -> (*mut u8, *mut u8) {
    let comms = super::globals::get!(gCommunications) as P;
    // SAFETY: the live communications object's plain tail
    unsafe {
        if server {
            let t = &mut *rec::<EeGamecppServerTail>(O::C11SvTail, comms);
            (&mut t.fast_selection_id, t.fast_selection.as_mut_ptr())
        } else {
            let t = &mut *rec::<EeGamecppClientTail>(O::C11ClTail, comms);
            (&mut t.fast_selection_id, t.fast_selection.as_mut_ptr())
        }
    }
}

/// The server's plain tail of `gCommunications` (the host's).
fn server_tail() -> &'static mut EeGamecppServerTail {
    let comms = super::globals::get!(gCommunications) as P;
    // SAFETY: the live server's plain tail, no C++ call while used
    unsafe { &mut *rec::<EeGamecppServerTail>(O::C11SvTail, comms) }
}

/// EE_NETMSG_OP_SELECTION: the communications object's fast-selection state (the server's or the client's).
///
/// # Safety
/// `i` points to the host call's integers.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_msg_selection(i: *const i64, pl: *mut c_void) -> i64 {
    let pl = pl as P;
    // SAFETY: the host's integers
    let i = unsafe { std::slice::from_raw_parts(i, 2) };
    let server = h(O::CommIsServer, 0, &[]) != 0;
    let pick = |s: O, c: O| if server { s } else { c };
    match i[0] {
        EE_NETMSG_SEL_LOCK => {
            if i[1] != 0 {
                h(pick(O::ServerFastLockEnter, O::ClientFastLockEnter), 0, &[]);
            } else {
                h(pick(O::ServerFastLockLeave, O::ClientFastLockLeave), 0, &[]);
            }
        }
        EE_NETMSG_SEL_MARK_LOCAL => {
            *wm::<bool>(O::WExecutingLocalSelection) = i[1] != 0;
        }
        EE_NETMSG_SEL_CURRENT_ID => return unsafe { *fast_selection(server).0 } as i64,
        EE_NETMSG_SEL_LAST_ID => {
            let ids = fast_selection(server).1;
            // SAFETY: the communications object's 16 last ids
            return unsafe { *ids.add(prts(pl).mPlayerID as u8 as usize) } as i64;
        }
        EE_NETMSG_SEL_SET_LAST_ID => {
            let ids = fast_selection(server).1;
            // SAFETY: as above
            unsafe { *ids.add(prts(pl).mPlayerID as u8 as usize) = i[1] as u8 };
        }
        EE_NETMSG_SEL_COPY => {
            h(O::PlSelectedCopyActive, pl, &[]);
            h(O::PlSetSelectedViewedUnit, pl, &[]);
        }
        EE_NETMSG_SEL_RESET => {
            unselect_all(pl);
            h(O::PlSelectedCopyActive, pl, &[]);
        }
        EE_NETMSG_SEL_JIT_OFF => {
            if pfields(pl).mJITManufacturing {
                pfields(pl).mJITManufacturing = false;
                if pl == current() {
                    h(O::UiUpdateJit, 0, &[]);
                }
            }
        }
        _ => {}
    }
    0
}

/// EE_NETMSG_OP_CAN_PLACE: whether the player can place the unit type at the tile (none off the map).
#[no_mangle]
pub extern "C" fn ee_gamecpp_msg_can_place(pl: *mut c_void, unit_type: i64, x: i64, y: i64, tx: i64, ty: i64) -> bool {
    let location = Ge3DPoint::new(x as f32, y as f32, 0.0);
    if hp(O::MapTile, 0, &[tx, ty]) != 0 {
        h(O::CanPlace, pl as P, &[unit_type, cptr(&location), tx, ty]) != 0
    } else {
        false
    }
}

/// EESendGroupCommand: the local command markers of a group order (move 0xb8, attack 0xb9, target 0xba, unload
/// 0xbb) shown on the goal the units just queued, else on the first unit whose last goal fits.
#[no_mangle]
pub extern "C" fn ee_gamecpp_msg_group_command(
    group: *mut c_void,
    target: *mut c_void,
    player_id: c_ulong,
    command: c_long,
    goal_queue: c_long,
) {
    let (group, target) = (group as P, target as P);
    let mut location = Ge3DPoint::default();
    let mut ground = Ge3DPoint::default();
    let mut index = goal_queue as c_ulong;
    let mut formation = h(O::GroupFormationType, group, &[]) as c_long;
    let mut best_time: c_ulong = 0;
    let mut markers: c_long = 0;
    let mut done = false;
    let mut handle: u32 = 0;
    if super::net::global().get(EeNetCommsField::IsGamePaused) != 0 {
        return;
    }
    let attack_effect = hp(O::GfxEffectGet, 0, &[EFFECT_ATTACK]);
    let move_effect = hp(O::GfxEffectGet, 0, &[EFFECT_MOVE]);
    let target_effect = hp(O::GfxEffectGet, 0, &[EFFECT_TARGET]);
    let count = || h(O::GroupCount, group, &[]) as c_ulong;
    let fraction = count() as f32 * 0.05f32;
    let mut rounded = ee_base::math::um::lrintf(fraction) as c_long;
    if (rounded as f32) < fraction {
        rounded += 1;
    }
    let step = if rounded == 0 { 1 } else { rounded };
    if (1..=5).contains(&formation) {
        formation = 1;
    }
    let local = |handle: u32| {
        let e = hp(O::EffGet, 0, &[handle as i64]);
        h(O::EffSetLocalTo, e, &[player_id as i64]);
    };
    let last_goal = |u: P, q: c_ulong| -> P {
        if (q as i64) < h(O::CxNumGoalQueues, u, &[]) && h(O::CxQueueSize, u, &[q as i64]) != 0 {
            hp(O::CxQueueBack, u, &[q as i64])
        } else {
            0
        }
    };
    let mut i: c_ulong = 0;
    while i < count() {
        let unit = hp(O::GroupUnit, group, &[i as i64]);
        let slot = cf(unit).mFormationSlot;
        if (formation != 0 || !(slot < 15))
            && (formation != 1 || !(slot < 20))
            && ((formation != 6 && formation != 7) || i % step as c_ulong != 0)
        {
            i += 1;
            continue;
        }
        let class = ucf(unit_class(unit));
        if class.mAIKind == AI_KIND_AIRCRAFT_CARRIER
            || class.mAIKind == AI_KIND_SPACE_CARRIER
            || class.mUnitType == UNIT_TYPE_AIRPORT
        {
            let mut j: c_ulong = 0;
            while (j as i64) < h(O::CxNumGoalQueues, unit, &[]) {
                if h(O::CxQueueSize, unit, &[j as i64]) == 0 {
                    j += 1;
                    continue;
                }
                let queued = hp(O::CxQueueBack, unit, &[j as i64]);
                if queued != 0 && best_time <= h(O::GoalCreationTime, queued, &[]) as c_ulong {
                    best_time = h(O::GoalCreationTime, queued, &[]) as c_ulong;
                    index = j;
                }
                j += 1;
            }
        } else {
            index = goal_queue as c_ulong;
        }
        let goal = last_goal(unit, index);
        if goal == 0 {
            done = true;
            i += 1;
            continue;
        }
        if command == 0xb8 && h(O::GoalHasLocation, goal, &[]) != 0 {
            h(O::GoalLocation, goal, &[ptr(&mut location)]);
            if hp(O::MapTile, 0, &[location.x as c_long as i64, location.y as c_long as i64]) != 0 {
                ground = super::map::world_ground_point(&location);
                h(O::UcGroundLocation, unit_class(unit), &[ptr(&mut ground)]);
                if ground.z < 0.0 {
                    ground.z = 0.0;
                }
                h(O::EffAt, unit, &[cptr(&ground), pa(move_effect), ptr(&mut handle), index as i64]);
                local(handle);
                done = true;
            }
            i += 1;
            continue;
        }
        if command == 0xb9 {
            if h(O::GoalIsAttackUnit, goal, &[]) != 0 {
                break;
            }
            if h(O::GoalIsAttackGround, goal, &[]) != 0 {
                h(O::GoalLocation, goal, &[ptr(&mut ground)]);
                h(O::EffAt, 0, &[cptr(&ground), pa(attack_effect), ptr(&mut handle), index as i64]);
                local(handle);
                return;
            }
        }
        if command == 0xbb {
            h(O::GoalLocation, goal, &[ptr(&mut ground)]);
            if h(O::GoalKind, goal, &[]) == MSG_GOAL_UNLOAD_TRANSPORT {
                let unload = hp(O::GoalAsUnload, goal, &[]);
                if unload != 0 {
                    let transport = hp(O::UnloadGoalTransport, unload, &[]);
                    if transport != 0 && ucf(unit_class(transport)).mCategory == UNIT_CATEGORY_FORTRESS {
                        h(O::UnPosition, hp(O::UnloadGoalTransport, unload, &[]), &[ptr(&mut ground)]);
                    }
                }
            }
            h(O::EffAt, unit, &[cptr(&ground), pa(move_effect), ptr(&mut handle), index as i64]);
            let e = hp(O::EffGet, 0, &[handle as i64]);
            markers += 1;
            h(O::EffSetLocalTo, e, &[player_id as i64]);
            done = true;
            if markers == 10 {
                return;
            }
            i += 1;
            continue;
        }
        if target != 0 {
            if h(O::GoalIsBuildRepair, goal, &[]) != 0
                || h(O::GoalIsGather, goal, &[]) != 0
                || h(O::GoalIsLoadGarrison, goal, &[]) != 0
            {
                break;
            }
            h(O::EffUnit, target, &[pa(target_effect), ptr(&mut handle), 0, 0, index as i64]);
        } else {
            h(O::GoalLocation, goal, &[ptr(&mut ground)]);
            h(O::EffAt, 0, &[cptr(&ground), pa(target_effect), ptr(&mut handle), index as i64]);
        }
        local(handle);
        done = true;
        break;
    }
    if done {
        return;
    }
    let mut i: c_ulong = 0;
    while i < count() {
        let unit = hp(O::GroupUnit, group, &[i as i64]);
        if !((index as i64) < h(O::CxNumGoalQueues, unit, &[])) {
            i += 1;
            continue;
        }
        let goal =
            if h(O::CxQueueSize, unit, &[index as i64]) > 0 { hp(O::CxQueueBack, unit, &[index as i64]) } else { 0 };
        if goal == 0
            || command == 0xb8
            || ((command != 0xb9 || h(O::GoalHasLocation, goal, &[]) == 0)
                && (command != 0xba || h(O::GoalHasLocation, goal, &[]) == 0))
        {
            i += 1;
            continue;
        }
        h(O::GoalLocation, goal, &[ptr(&mut location)]);
        ground = super::map::world_ground_point(&location);
        h(O::UcGroundLocation, unit_class(unit), &[ptr(&mut ground)]);
        h(O::EffAt, unit, &[cptr(&ground), pa(move_effect), ptr(&mut handle), index as i64]);
        local(handle);
        return;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_tribute_names_and_sounds_are_the_tables() {
        assert_eq!(RESOURCE_NAMES[2..].len(), 5);
        assert_eq!(SOUNDS, [0x3a8, 0xe]);
        assert_eq!(EFFECT_MOVE + 1, EFFECT_ATTACK);
        assert_eq!(EFFECT_ATTACK + 1, EFFECT_TARGET);
    }
}
