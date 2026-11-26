//! The session host (was EEServer.cpp's `EESessionHost` and its helpers): what ee_sim's session decisions
//! (`netmsg::session`, the server's and client's members) ask of the C++ world, the lobby, the UI and the network
//! engine. Each C++ call or member is a one-statement case of EEServer.cpp's `EECore9OpD` / `EECore9OpD2`
//! ([`super::EeGameCpp`]'s `Ss` ops, `p` the session's frame); [`ee_gamecpp_session_host`] is the host function
//! `EESession` hands to `ee_netmsg_session`.
//!
//! Network code: single-player games never run it; only a LAN game (the lead's gates) does.

use std::ffi::{c_long, c_ulong, c_void};

use ee_sim::netmsg::session::*;

use super::session_objs::{
    ee_gamecpp_server_send_to_slot, send_to_clear, send_to_erase, send_to_insert, EeGamecppClientTail,
    EeGamecppServerTail,
};
use super::message::prts;
use super::{h, hp, net, ptr, session_objs, Owned, O, P};
use ee_net::net::ffi_comms::EeNetCommsField;
use ee_net::net::ffi_server::{EeNetSvGet, EeNetSvSet};

/// The network engine's handles of the session's communications object (the frame's).
fn n(f: P) -> net::Net {
    net::net(comms_of(f) as *const c_void)
}

/// The session's communications object (the server or the client: the C++ cases' p).
fn comms_of(f: P) -> P {
    session_objs::frame_server(f)
}
use crate::app::game_options::EeAppOptionsPlayerFields;

/// `kSlotServer`, `kSlotNone` (NEMessageID.h).
pub const SS_SLOT_SERVER: i64 = 0xfe;
pub const SS_SLOT_NONE: i64 = 0xff;
/// The strings the session's lines show (ee/EEStringIDs.h; cpp_consts_tests.rs checks them): `IDS_MSG_GAME`,
/// `IDS_HOST_IS_NOT_RESPONDING_PLEASE_WAIT`, `IDS_SECONDS`.
pub const SS_IDS_MSG_GAME: i64 = 31813;
pub const SS_IDS_HOST_IS_NOT_RESPONDING: i64 = 30621;
pub const SS_IDS_SECONDS: i64 = 30379;

/// The lobby's game options' plain members (`gWorld.mLobbyOptions`).
fn lobby_options<'a>() -> &'a crate::app::game_options::EeAppOptionsFields {
    // SAFETY: the world's lobby options record, read before the next C++ call
    unsafe { &*super::rec(O::LobbyOptions, 0) }
}

fn rw_bool(at: &mut bool, set: bool, v: i64) -> i64 {
    if set {
        *at = v != 0;
    }
    *at as i64
}

fn rw_u8(at: &mut u8, set: bool, v: i64) -> i64 {
    if set {
        *at = v as u8;
    }
    *at as i64
}

fn rw_u16(at: &mut u16, set: bool, v: i64) -> i64 {
    if set {
        *at = v as u16;
    }
    *at as i64
}

/// Whether member `which` is the server's: the four both classes have are the frame's object's, the votes, loads
/// and lag mask the server's, the rest the client's.
fn server_member(which: i64, frame_server: impl FnOnce() -> bool) -> bool {
    match which {
        EE_NETMSG_SESS_SELF_HAS_LEFT
        | EE_NETMSG_SESS_SELF_MIGRATING
        | EE_NETMSG_SESS_SELF_FAST_ID
        | EE_NETMSG_SESS_SELF_EXECUTING => frame_server(),
        _ => which < EE_NETMSG_SESS_SELF_UI_SLOT,
    }
}

/// The server's and client's members (EESessionMember) in their plain tails (session_objs.rs): the voted flags by
/// index (a negative index set clears all 256), the others by id; the four both classes have are the frame's
/// object's, the others the server's or the client's.
fn member(f: P, which: i64, index: i64, set: bool, v: i64) -> i64 {
    let comms = session_objs::frame_server(f);
    if server_member(which, || session_objs::frame(f).server) {
        // SAFETY: the server's plain tail, no C++ call while used
        let t = unsafe { &mut *super::rec::<EeGamecppServerTail>(O::C11SvTail, comms) };
        match which {
            EE_NETMSG_SESS_SELF_VOTED => {
                if set && index < 0 {
                    t.voted = [false; 256];
                    return 0;
                }
                rw_bool(&mut t.voted[(index & 0xff) as usize], set, v)
            }
            EE_NETMSG_SESS_SELF_HAS_LEFT => rw_bool(&mut t.has_left_game, set, v),
            EE_NETMSG_SESS_SELF_MIGRATING => rw_bool(&mut t.is_migrating, set, v),
            EE_NETMSG_SESS_SELF_FAST_ID => rw_u8(&mut t.fast_selection_id, set, v),
            EE_NETMSG_SESS_SELF_EXECUTING => rw_bool(&mut t.is_executing_selection, set, v),
            EE_NETMSG_SESS_SELF_LOAD_STATE => rw_u8(&mut t.load_state, set, v),
            EE_NETMSG_SESS_SELF_DROPPED_LOADERS => rw_u8(&mut t.num_dropped_loaders, set, v),
            EE_NETMSG_SESS_SELF_VOTE_EDICT => rw_u16(&mut t.vote_edict, set, v),
            EE_NETMSG_SESS_SELF_YES_VOTES => rw_u16(&mut t.yes_votes, set, v),
            EE_NETMSG_SESS_SELF_NO_VOTES => rw_u16(&mut t.no_votes, set, v),
            EE_NETMSG_SESS_SELF_NUM_VOTES => rw_u8(&mut t.num_votes, set, v),
            _ => rw_u16(&mut t.lag_mask, set, v),
        }
    } else {
        // SAFETY: the client's plain tail, no C++ call while used
        let t = unsafe { &mut *super::rec::<EeGamecppClientTail>(O::C11ClTail, comms) };
        match which {
            EE_NETMSG_SESS_SELF_HAS_LEFT => rw_bool(&mut t.has_left_game, set, v),
            EE_NETMSG_SESS_SELF_MIGRATING => rw_bool(&mut t.is_migrating, set, v),
            EE_NETMSG_SESS_SELF_FAST_ID => rw_u8(&mut t.fast_selection_id, set, v),
            EE_NETMSG_SESS_SELF_EXECUTING => rw_bool(&mut t.is_executing_selection, set, v),
            EE_NETMSG_SESS_SELF_UI_SLOT => rw_u8(&mut t.ui_slot_number, set, v),
            EE_NETMSG_SESS_SELF_ALLOW_MIGRATION => rw_bool(&mut t.allow_server_migration, set, v),
            EE_NETMSG_SESS_SELF_IGNORE_TIMEOUT => rw_bool(&mut t.ignore_connection_timeout, set, v),
            EE_NETMSG_SESS_SELF_SERVER_LOST => rw_bool(&mut t.is_server_lost, set, v),
            EE_NETMSG_SESS_SELF_HAS_LOADED => rw_bool(&mut t.has_loaded, set, v),
            EE_NETMSG_SESS_SELF_LOST_AFTER_LOADING => rw_bool(&mut t.server_lost_after_loading, set, v),
            _ => rw_bool(&mut t.was_booted, set, v),
        }
    }
}

/// A lobby slot player's plain member (EESessionPlayerField): read, or written then read (the ping as its float's bits).
pub fn player_field(r: &mut EeAppOptionsPlayerFields, field: i64, set: bool, v: i64) -> i64 {
    match field {
        EE_NETMSG_SESS_FIELD_TYPE => {
            if set {
                r.mType = v as c_ulong;
            }
            r.mType as i64
        }
        EE_NETMSG_SESS_FIELD_LOADED_TYPE => {
            if set {
                r.mLoadedType = v as c_long;
            }
            r.mLoadedType as i64
        }
        EE_NETMSG_SESS_FIELD_PING => {
            if set {
                r.mPing = f32::from_bits(v as u32);
            }
            r.mPing.to_bits() as i64
        }
        EE_NETMSG_SESS_FIELD_SLOT => {
            if set {
                r.mSlot = v as c_ulong;
            }
            r.mSlot as i64
        }
        EE_NETMSG_SESS_FIELD_COLOR => {
            if set {
                r.mColor = v as c_ulong;
            }
            r.mColor as i64
        }
        EE_NETMSG_SESS_FIELD_TEAM => {
            if set {
                r.mTeam = v as c_ulong;
            }
            r.mTeam as i64
        }
        EE_NETMSG_SESS_FIELD_CITIZENS => {
            if set {
                r.mStartingCitizens = v as c_ulong;
            }
            r.mStartingCitizens as i64
        }
        EE_NETMSG_SESS_FIELD_TRANSFER_ID => {
            if set {
                r.mTransferID = v as c_ulong;
            }
            r.mTransferID as i64
        }
        EE_NETMSG_SESS_FIELD_LOCAL => {
            if set {
                r.mIsLocal = v != 0;
            }
            r.mIsLocal as i64
        }
        EE_NETMSG_SESS_FIELD_READY => {
            if set {
                r.mIsReady = v != 0;
            }
            r.mIsReady as i64
        }
        EE_NETMSG_SESS_FIELD_B36 => {
            if set {
                r.mB36 = v != 0;
            }
            r.mB36 as i64
        }
        _ => {
            if set {
                r.mEngine = v as c_ulong;
            }
            r.mEngine as i64
        }
    }
}

fn lock(f: P, v: &[i64; 8]) -> i64 {
    let on = v[1] != 0;
    let section = match v[0] {
        EE_NETMSG_SESS_LOCK_CT => {
            let server = n(f).server;
            if on {
                ee_net::net::ffi_server::ee_net_sv_lock_send_communications_tick(server);
            } else {
                ee_net::net::ffi_server::ee_net_sv_unlock_send_communications_tick(server);
            }
            return 0;
        }
        EE_NETMSG_SESS_LOCK_OPTIONS_WRITE => {
            h(if on { O::SsOptionsWriteLock } else { O::SsOptionsWriteUnlock }, f, &[]);
            return 0;
        }
        EE_NETMSG_SESS_LOCK_WORLD_WRITE => {
            h(if on { O::SsWorldWriteLock } else { O::SsWorldWriteUnlock }, f, &[]);
            return 0;
        }
        EE_NETMSG_SESS_LOCK_PING => {
            n(f).sv_set(EeNetSvSet::Ping, !on as u32);
            return 0;
        }
        EE_NETMSG_SESS_LOCK_SEARCH => hp(O::SsSearchLock, comms_of(f), &[]),
        EE_NETMSG_SESS_LOCK_ATTEMPT => hp(O::SsAttemptLock, comms_of(f), &[]),
        EE_NETMSG_SESS_LOCK_FAST => {
            if session_objs::frame(f).server {
                hp(O::SsServerFastLock, comms_of(f), &[])
            } else {
                hp(O::SsClientFastLock, comms_of(f), &[])
            }
        }
        _ => {
            h(if on { O::SsPhysicsQueueEnter } else { O::SsPhysicsQueueLeave }, f, &[]);
            return 0;
        }
    };
    h(if on { O::SsSectionEnter } else { O::SsSectionLeave }, f, &[section as i64]);
    0
}

fn fact(f: P, v: &[i64; 8]) -> i64 {
    let op = match v[0] {
        EE_NETMSG_SESS_FACT_MULTIPLAYER_GAME => O::SsMultiplayerGame,
        EE_NETMSG_SESS_FACT_OPTIONS_MULTIPLAYER => return lobby_options().mIsMultiplayer as i64,
        EE_NETMSG_SESS_FACT_LOAD_TYPE => return lobby_options().mLoadType as i64,
        EE_NETMSG_SESS_FACT_GAME_STATE => return n(f).sv_get(EeNetSvGet::GameState) as u8 as i64,
        EE_NETMSG_SESS_FACT_DEFAULT_COLOR => return super::globals::get!(gDefaultPlayerColors)[v[1] as usize] as i64,
        EE_NETMSG_SESS_FACT_CITIZENS_DEFAULT => O::SsCitizensDefault,
        EE_NETMSG_SESS_FACT_VERSION_MATCH => return h(O::SsVersionMatch, session_objs::frame(f).version, &[]),
        EE_NETMSG_SESS_FACT_BINARY_CHECKSUM => return super::globals::get!(gBinaryChecksum) as i64,
        EE_NETMSG_SESS_FACT_CURRENT_CT => return n(f).current_ct() as i64,
        EE_NETMSG_SESS_FACT_CTS_IN_PHYSICS => return net::global().get(EeNetCommsField::CtsProcessed) as i64,
        EE_NETMSG_SESS_FACT_HUMANS => return net::global().get(EeNetCommsField::NumHumanPlayers) as u8 as i64,
        EE_NETMSG_SESS_FACT_MP_SESSION => return (n(f).get(EeNetCommsField::IsMultiplayerSession) != 0) as i64,
        EE_NETMSG_SESS_FACT_PLAYER_NUMBER => return n(f).get(EeNetCommsField::PlayerNumber) as i64,
        EE_NETMSG_SESS_FACT_CURRENT_PT => {
            return ee_net::net::ffi_client::ee_net_cl_current_physics_tick(n(f).client) as i64;
        }
        EE_NETMSG_SESS_FACT_MY_SEND_TO_SLOT => {
            return ee_net::net::ffi_client::ee_net_cl_my_send_to_slot(n(f).client) as i64;
        }
        EE_NETMSG_SESS_FACT_NUM_PLAYERS => return n(f).sv_get(EeNetSvGet::NumberOfPlayers) as u8 as i64,
        EE_NETMSG_SESS_FACT_WORLD_PLAYERS => return super::message::num_players() as i64,
        EE_NETMSG_SESS_FACT_TIME => O::SsTime,
        EE_NETMSG_SESS_FACT_WON => return super::globals::have!(gWonNet) as i64,
        EE_NETMSG_SESS_FACT_SCREEN_ACTIVE => return *super::message::wm::<bool>(O::WGameScreenActive) as i64,
        EE_NETMSG_SESS_FACT_ENGINE_CHOICES => return crate::cp_engine::ee_cp_engine_choice_count() as c_ulong as i64,
        EE_NETMSG_SESS_FACT_FORM_IS_CURRENT => {
            // the form looked up first (v2 0) or the current form first
            return if v[2] == 0 {
                let form = hp(O::SsFormById, f, &[v[1]]);
                (hp(O::SsCurrentForm, f, &[]) == form) as i64
            } else {
                let current = hp(O::SsCurrentForm, f, &[]);
                (current == hp(O::SsFormById, f, &[v[1]])) as i64
            };
        }
        EE_NETMSG_SESS_FACT_FORM_VISIBLE => O::SsFormVisible,
        EE_NETMSG_SESS_FACT_SEND_TO_SLOT => return ee_gamecpp_server_send_to_slot(v[1] as u8) as i64,
        _ => O::SsPlayerSendToSlot,
    };
    h(op, comms_of(f), &[v[1]])
}

fn reg(f: P, v: &[i64; 8]) -> i64 {
    let (r, kind, x) = (session_objs::register(f, v[0]), v[1], v[2]);
    let other = || session_objs::register(f, x) as i64;
    match kind {
        EE_NETMSG_SESS_REG_LOAD_OPTIONS => h(O::SsRegLoadOptions, r, &[x]),
        EE_NETMSG_SESS_REG_LOAD_LOBBY => h(O::SsRegLoadLobby, r, &[x]),
        EE_NETMSG_SESS_REG_LOAD_REG => h(O::SsRegLoadReg, r, &[other()]),
        EE_NETMSG_SESS_REG_STORE_OPTIONS => h(O::SsRegStoreOptions, r, &[x]),
        EE_NETMSG_SESS_REG_STORE_LOBBY => h(O::SsRegStoreLobby, r, &[x]),
        EE_NETMSG_SESS_REG_GET | EE_NETMSG_SESS_REG_SET => {
            let fields = hp(O::C11PlayerFields, r, &[]) as *mut EeAppOptionsPlayerFields;
            // SAFETY: the register's plain members (EEGameOptionsPlayer's base), no C++ call while held
            let fields = unsafe { &mut *fields };
            player_field(fields, x, kind == EE_NETMSG_SESS_REG_SET, v[3])
        }
        EE_NETMSG_SESS_REG_NAME_CLEAR => h(O::SsRegNameClear, r, &[]),
        EE_NETMSG_SESS_REG_NAME_COPY => h(O::SsRegNameCopy, r, &[other()]),
        EE_NETMSG_SESS_REG_NAME_ICMP => h(O::SsRegNameIcmp, r, &[other()]),
        EE_NETMSG_SESS_REG_NAME_EQ => h(O::SsRegNameEq, r, &[other()]),
        EE_NETMSG_SESS_REG_NAME_FROM_MSG => h(O::SsRegNameFromMsg, r, &[x]),
        _ => h(O::SsRegNameIcmpPlayer, r, &[x]),
    }
}

fn send(f: P, v: &[i64; 8]) -> i64 {
    match v[0] {
        EE_NETMSG_SESS_SEND_RESPONSE => {
            let server = session_objs::frame_server(f);
            session_objs::send_connection_response(
                server,
                v[1] as u8 as i64,
                v[2] as c_long as i64,
                v[3] as u8 as i64,
                v[4] as u8 as i64,
                v[5] != 0,
            );
            0
        }
        EE_NETMSG_SESS_SEND_LOCAL_RESPONSE => {
            let response = hp(O::C11SvNewResponse, comms_of(f), &[v[1], v[2], v[3]]);
            n(f).local_input(response as *mut c_void, v[4] as u8);
            0
        }
        EE_NETMSG_SESS_SEND_FULL_OPTIONS => h(O::SsSendFullOptions, f, &[v[1]]),
        EE_NETMSG_SESS_SEND_PLAYER_INFO => {
            let info = hp(O::SsNewPlayerInfo, session_objs::register(f, v[1]), &[]);
            if v[3] != 0 {
                h(O::SsMsgSetOriginServer, f, &[info as i64]);
            }
            n(f).local_input(info as *mut c_void, v[2] as u8);
            0
        }
        EE_NETMSG_SESS_SEND_NEW_RECORD => {
            // the values' count (0, 2, 3, else 4)
            let n = match v[2] {
                0 | 2 | 3 => v[2] as usize,
                _ => 4,
            };
            let values: Vec<_> = v[3..3 + n].iter().map(|&x| super::mrecord::EeGamecppMrValue::integer(x)).collect();
            super::mrecord::new_record(v[1] as u8, &values) as i64
        }
        EE_NETMSG_SESS_SEND_PING_STAT => {
            session_objs::send_ping_stat(session_objs::frame_server(f), v[1] as u8 as i64, f32::from_bits(v[2] as u32));
            0
        }
        EE_NETMSG_SESS_SEND_SLOT_INFO => {
            // player v1 at send-to slot v2: the host's address, none, a client's
            let (index, slot) = (v[1] as u8 as i64, v[2] as u8 as i64);
            let message = if slot == SS_SLOT_SERVER {
                hp(O::SsSlotInfoServer, comms_of(f), &[index, session_objs::frame(f).protocol as i64])
            } else {
                let address = Owned::new(O::SsNewString, O::SsDeleteString);
                if slot != SS_SLOT_NONE {
                    h(O::SsClientAddress, comms_of(f), &[slot, address.0 as i64]);
                }
                hp(O::SsSlotInfoRecord, comms_of(f), &[index, slot, address.0 as i64])
            };
            net::global().local_input(message as *mut c_void, SS_SLOT_NONE as u8);
            0
        }
        EE_NETMSG_SESS_SEND_PHYSICS_TICK => h(O::SsNewPhysicsTick, f, &[v[1], v[2], v[3], v[4], v[5]]),
        _ => h(O::SsConnectionCheckResponse, comms_of(f), &[v[1]]),
    }
}

fn message(f: P, v: &[i64; 8]) -> i64 {
    let m = v[1];
    match v[0] {
        EE_NETMSG_SESS_MSG_TYPE => h(O::SsMsgType, f, &[m]),
        EE_NETMSG_SESS_MSG_ORIGIN => h(O::SsMsgOrigin, f, &[m]),
        EE_NETMSG_SESS_MSG_FOR_SLOT => h(O::SsMsgForSlot, f, &[m]),
        EE_NETMSG_SESS_MSG_NE_VALUE => h(O::SsMsgNeValue, f, &[m, v[2]]),
        EE_NETMSG_SESS_MSG_RECORD => h(O::SsMsgRecord, f, &[m, v[2]]),
        EE_NETMSG_SESS_MSG_INPUT_COUNT => h(O::SsMsgInputCount, f, &[m]),
        EE_NETMSG_SESS_MSG_INPUT => {
            let mut input: P = 0;
            h(O::SsMsgInput, f, &[m, v[2], ptr(&mut input)]);
            input as i64
        }
        EE_NETMSG_SESS_MSG_REFERENCE => h(O::SsMsgReference, f, &[m]),
        EE_NETMSG_SESS_MSG_DEREFERENCE => h(O::SsMsgDereference, f, &[m]),
        EE_NETMSG_SESS_MSG_EXECUTE => h(O::SsMsgExecute, f, &[m]),
        EE_NETMSG_SESS_MSG_SET_FAST_ID => h(O::SsMsgSetFastId, f, &[m, v[2]]),
        EE_NETMSG_SESS_MSG_IS_GAME => h(O::SsMsgIsGame, f, &[m]),
        EE_NETMSG_SESS_MSG_QUEUE => h(O::SsMsgQueue, f, &[m]),
        EE_NETMSG_SESS_MSG_LOG_RESPONSE => {
            ee_net::net::ffi_server::ee_net_sv_log_response(n(f).server, m as P as *mut c_void);
            0
        }
        EE_NETMSG_SESS_MSG_LOCAL => {
            n(f).local_input(m as P as *mut c_void, v[2] as u8);
            0
        }
        EE_NETMSG_SESS_MSG_BROADCAST => {
            h(O::SsMsgSetOriginServer, f, &[m]);
            ee_net::net::ffi_server::ee_net_sv_broadcast_message(n(f).server, m as P as *mut c_void);
            0
        }
        EE_NETMSG_SESS_MSG_SEND_ALL => {
            h(O::SsMsgSetOriginServer, f, &[m]);
            ee_net::net::ffi_server::ee_net_sv_send_message_to_all(n(f).server, m as P as *mut c_void, false);
            h(O::SsMsgDereference, f, &[m])
        }
        _ => {
            session_objs::take_connection_request(session_objs::frame_server(f), m as P);
            0
        }
    }
}

/// An action posted to the UI thread, then the poster's reference dropped (EESessionPost).
fn post(f: P, action: P) {
    h(O::SsPostAction, f, &[action as i64]);
    h(O::SsActionDereference, f, &[action as i64]);
}

/// A string resource shown in the UI manager's message box (`cancel`: the cancel-game callback).
fn display(f: P, text: i64, cancel: bool, flag: bool) {
    let s = Owned::new(O::SsNewString, O::SsDeleteString);
    h(O::SsGetString, f, &[text, s.0 as i64]);
    h(O::SsDisplayMessage, f, &[s.0 as i64, cancel as i64, flag as i64]);
}

/// Back to the game list (the lobby service's leave).
fn won_leave(f: P) {
    h(O::SsWonClearGameList, f, &[]);
    h(O::SsWonRejoinRoom, f, &[]);
}

fn ui(f: P, v: &[i64; 8]) -> i64 {
    match v[0] {
        EE_NETMSG_SESS_UI_LOBBY_PLAYER => {
            h(O::SsLobbyUpdatePlayer, f, &[v[1]]);
        }
        EE_NETMSG_SESS_UI_ACTION_REMOVE => post(f, hp(O::SsNewRemovePlayer, f, &[v[1], v[2]])),
        EE_NETMSG_SESS_UI_ACTION_BOOT => post(f, hp(O::SsNewBootClient, f, &[v[1]])),
        EE_NETMSG_SESS_UI_ACTION_MIGRATION => post(f, hp(O::SsNewServerMigration, f, &[])),
        EE_NETMSG_SESS_UI_PAUSE_EVENT => {
            let event = hp(O::SsNewPauseEvent, f, &[]);
            h(O::SsPostEvent, f, &[event as i64]);
            h(O::SsEventDereference, f, &[event as i64]);
        }
        EE_NETMSG_SESS_UI_DISPLAY => display(f, v[1], v[2] != 0, v[3] != 0),
        EE_NETMSG_SESS_UI_CHAT_PLAYER => {
            // the game's line about player v2 (string v1 formats its name)
            let text = Owned::new(O::SsNewString, O::SsDeleteString);
            let format = Owned::new(O::SsNewString, O::SsDeleteString);
            let name = Owned::new(O::SsNewString, O::SsDeleteString);
            h(O::SsGetString, f, &[v[1], format.0 as i64]);
            h(O::SsSprintfName, f, &[text.0 as i64, format.0 as i64, v[2]]);
            h(O::SsGetString, f, &[SS_IDS_MSG_GAME, name.0 as i64]);
            h(O::SsChatGameLine, f, &[name.0 as i64, text.0 as i64]);
        }
        EE_NETMSG_SESS_UI_CHAT_TEXT => {
            let text = Owned::new(O::SsNewString, O::SsDeleteString);
            h(O::SsGetString, f, &[v[1], text.0 as i64]);
            h(O::SsChatText, f, &[text.0 as i64, v[2]]);
        }
        EE_NETMSG_SESS_UI_CLOSE_FORM => {
            h(O::SsCloseForm, f, &[]);
        }
        EE_NETMSG_SESS_UI_OPEN_FORM => {
            h(O::SsOpenForm, f, &[v[1]]);
        }
        EE_NETMSG_SESS_UI_PUSH_FORM => {
            h(O::SsPushForm, f, &[v[1]]);
        }
        EE_NETMSG_SESS_UI_CANCEL_GAME => {
            h(O::SsCancelGame, f, &[]);
        }
        EE_NETMSG_SESS_UI_CANCEL_CALLBACK => {
            let mut unused: c_ulong = 0;
            h(O::SsCancelCallback, f, &[ptr(&mut unused)]);
        }
        EE_NETMSG_SESS_UI_LOBBY_FORM => {
            h(O::SsLobbyForm, f, &[]);
        }
        EE_NETMSG_SESS_UI_HOST_MIGRATED => {
            h(O::SsHostMigrated, f, &[]);
        }
        EE_NETMSG_SESS_UI_TRANSFER => {
            h(O::SsTransfer, f, &[v[1], v[2]]);
        }
        EE_NETMSG_SESS_UI_CONNECT_STATUS => {
            h(O::SsConnectStatus, f, &[v[1]]);
        }
        EE_NETMSG_SESS_UI_RETRY => {
            // the connecting box with the retries
            let text = Owned::new(O::SsNewString, O::SsDeleteString);
            let format = Owned::new(O::SsNewString, O::SsDeleteString);
            h(O::SsGetString, f, &[v[1], format.0 as i64]);
            h(O::SsSprintfRetry, comms_of(f), &[text.0 as i64, format.0 as i64]);
            h(O::SsDisplayMessage, f, &[text.0 as i64, 0, 0]);
        }
        EE_NETMSG_SESS_UI_LEAVE => {
            // back to the game list, the reason shown (with the host's version and ours) or the boot posted, then
            // disconnected
            let text = Owned::new(O::SsNewString, O::SsDeleteString);
            let format = Owned::new(O::SsNewString, O::SsDeleteString);
            if super::globals::have!(gWonNet) {
                won_leave(f);
            }
            if v[2] == EE_NETMSG_SESS_LEAVE_BOOT {
                post(f, hp(O::SsNewBootClient, f, &[v[1]]));
            } else if v[2] != EE_NETMSG_SESS_LEAVE_QUIET {
                h(O::SsCloseForm, f, &[]);
                h(O::SsGetString, f, &[v[1], format.0 as i64]);
                let versions = v[2] == EE_NETMSG_SESS_LEAVE_SHOW_VERSIONS;
                if versions {
                    h(O::SsSprintfVersions, f, &[text.0 as i64, format.0 as i64, v[3]]);
                }
                let shown = if versions { text.0 } else { format.0 };
                h(O::SsDisplayMessage, f, &[shown as i64, 0, 1]);
            }
            ee_net::net::ffi_client::ee_net_cl_disconnect(n(f).client);
        }
        _ => {
            let player =
                hp(O::C11PlayerFields, session_objs::register(f, v[2]), &[]) as *const EeAppOptionsPlayerFields;
            // SAFETY: the register's plain members
            ee_gamecpp_client_slot_update(v[1] as u8, unsafe { &*player });
        }
    }
    0
}

fn won(f: P, kind: i64, a: i64, b: i64) {
    match kind {
        EE_NETMSG_SESS_WON_COUNT | EE_NETMSG_SESS_WON_HUMANS_DEC => {
            let info = Owned::new(O::SsNewWonInfo, O::SsDeleteWonInfo);
            h(O::SsWonGetInfo, f, &[info.0 as i64]);
            let humans = hp(O::SsWonInfoHumans, f, &[info.0 as i64]) as *mut u32;
            // SAFETY: the info's uint32_t members, no C++ call while written
            unsafe {
                if kind == EE_NETMSG_SESS_WON_COUNT {
                    let players = hp(O::SsWonInfoPlayers, f, &[info.0 as i64]) as *mut u32;
                    *players = a as u32;
                    *humans = b as u32;
                } else {
                    *humans = (*humans).wrapping_sub(1);
                }
            }
            h(O::SsWonSetInfo, f, &[info.0 as i64]);
        }
        EE_NETMSG_SESS_WON_JOINED => {
            h(O::SsWonJoined, f, &[]);
        }
        _ => won_leave(f),
    }
}

fn sound(f: P, v: &[i64; 8]) {
    if v[0] == 0 {
        // the server's: loaded and played
        let sample = h(O::SsSoundLoadServer, f, &[v[1]]);
        if sample as u32 != u32::MAX {
            h(O::SsSoundPlayServer, f, &[]);
        }
    } else {
        let mut sample: c_ulong = 0;
        let mut instance: c_ulong = 0;
        h(O::SsLoadSound, f, &[(v[0] == 1) as i64, ptr(&mut sample), ptr(&mut instance)]);
    }
}

fn world(f: P, v: &[i64; 8]) -> i64 {
    let pl = v[1] as P;
    match v[0] {
        EE_NETMSG_SESS_WORLD_PLAYER => h(O::WorldPlayer, 0, &[v[1]]),
        EE_NETMSG_SESS_WORLD_PLAYER_BY_SLOT => super::message::player_by_slot(v[1]) as i64,
        EE_NETMSG_SESS_WORLD_IS_HUMAN => h(O::SsPlIsHuman, pl, &[]),
        EE_NETMSG_SESS_WORLD_SLOT => prts(pl).mSlot as i64,
        EE_NETMSG_SESS_WORLD_HAS_HUMAN => h(O::SsPlHasHuman, pl, &[]),
        EE_NETMSG_SESS_WORLD_ID => prts(pl).mPlayerID as i64,
        EE_NETMSG_SESS_WORLD_SET_SLOT => {
            prts(pl).mSlot = v[2] as u8;
            0
        }
        EE_NETMSG_SESS_WORLD_SET_FIREWALL => {
            super::message::pfields(pl).mBehindFirewall = v[2] != 0;
            0
        }
        EE_NETMSG_SESS_WORLD_NAME_FROM_MSG => h(O::SsPlNameFromMsg, pl, &[v[2]]),
        EE_NETMSG_SESS_WORLD_RECONNECT => h(O::SsWorldReconnect, f, &[v[1], v[2]]),
        EE_NETMSG_SESS_WORLD_REMOVE => h(O::SsWorldRemove, f, &[v[1]]),
        _ => h(O::SsExecuteSelection, f, &[v[1]]),
    }
}

/// Load type v1's file (the map type) sent to lobby slot v2's client (a saved game or a scenario).
fn xfer_game_file(f: P, load_type: i64, slot: i64) {
    let mut file: P = 0;
    if h(O::SsOpenSaveGame, f, &[(load_type == 2) as i64, ptr(&mut file)]) != 0 {
        let header = Owned::new(O::SsNewHeader, O::SsDeleteHeader);
        h(O::SsHeaderSetVersions, f, &[header.0 as i64]);
        let loaded =
            super::persist_parts::ee_gamecpp_hdr_load_from_file(header.0 as *mut c_void, file as *mut c_void) == 0;
        if loaded && h(O::SsMultiplayerGame, f, &[]) == 0 {
            let mut xfer_id: c_ulong = 0;
            let send_to = ee_gamecpp_server_send_to_slot(slot as u8) as i64;
            h(O::SsXferToClient, comms_of(f), &[file as i64, header.0 as i64, send_to, ptr(&mut xfer_id)]);
        }
    }
}

fn comms(f: P, v: &[i64; 8]) {
    use ee_net::net::ffi_server as sv;
    match v[0] {
        EE_NETMSG_SESS_COMMS_SET_CT => net::global().set_current_ct(v[1] as u32),
        EE_NETMSG_SESS_COMMS_SET_CTS_IN_PHYSICS => net::global().set(EeNetCommsField::CtsProcessed, v[1] as u32),
        EE_NETMSG_SESS_COMMS_INCREMENT_CTS => net::global().increment_cts(),
        EE_NETMSG_SESS_COMMS_SET_MP_SESSION => {
            net::global().set(EeNetCommsField::IsMultiplayerSession, (v[1] != 0) as u32);
        }
        EE_NETMSG_SESS_COMMS_SET_HUMANS => n(f).set(EeNetCommsField::NumHumanPlayers, v[1] as u8 as u32),
        EE_NETMSG_SESS_COMMS_SET_PT => {
            ee_net::net::ffi_client::ee_net_cl_set_current_physics_tick(n(f).client, v[1] as u32);
        }
        EE_NETMSG_SESS_COMMS_REGISTER => sv::ee_net_sv_register_slot(n(f).server, v[1] as u8),
        EE_NETMSG_SESS_COMMS_UNREGISTER => sv::ee_net_sv_unregister_slot(n(f).server, v[1] as u8),
        EE_NETMSG_SESS_COMMS_SEND_TO_CLEAR => send_to_clear(),
        EE_NETMSG_SESS_COMMS_SEND_TO_INSERT => send_to_insert(v[1] as u8, v[2] as u8),
        EE_NETMSG_SESS_COMMS_SEND_TO_ERASE => send_to_erase(v[1] as u8),
        EE_NETMSG_SESS_COMMS_ABORT_XFER => sv::ee_net_sv_abort_xfer(n(f).server, v[1] as u32),
        EE_NETMSG_SESS_COMMS_REMOVE_IN_PROGRESS => {
            sv::ee_net_sv_remove_player_from_game_in_progress(n(f).server, v[1] as u8, v[2] != 0, false, v[3] != 0);
        }
        EE_NETMSG_SESS_COMMS_XFER_GAME_FILE => xfer_game_file(f, v[1], v[2]),
        EE_NETMSG_SESS_COMMS_START_GAME => sv::ee_net_sv_set_event(n(f).server, sv::EeNetSvEvent::StartGame),
        EE_NETMSG_SESS_COMMS_PAUSE_GAME => sv::ee_net_sv_pause_game(n(f).server, true),
        EE_NETMSG_SESS_COMMS_BECOME_CLIENT => {
            // the host removed: the server deleted, a client in its place
            let host = net::global();
            host.sv_set(EeNetSvSet::Ping, 0);
            // ResetXfer: protocol 0's reset of the published server's transfer
            let (protocol, published) = (sv::ee_net_sv_protocol(host.server), sv::ee_net_sv_server(host.server));
            // SAFETY: the live server's protocol and published server
            unsafe { ee_net::net::ffi_protocol::ee_net_pr_reset(protocol.cast(), published, true, 0) };
            sv::ee_net_sv_unregister_slot(n(f).server, SS_SLOT_SERVER as u8);
            sv::ee_net_sv_register_slot(n(f).server, SS_SLOT_SERVER as u8);
            if net::present() {
                h(O::SsCommsDelete, f, &[]);
                h(O::SsCommsNewClient, f, &[]);
                ee_net::net::ffi_client::ee_net_cl_start_threads(net::global().client);
            }
        }
        EE_NETMSG_SESS_COMMS_LOWEST_CTS => {
            sv::ee_net_sv_set_message_manager_lowest_cts(net::global().server, v[1] as u32);
        }
        EE_NETMSG_SESS_COMMS_DISCONNECT => {
            ee_net::net::ffi_client::ee_net_cl_disconnect(n(f).client);
        }
        EE_NETMSG_SESS_COMMS_LOCAL_OUT => super::globals::set!(gLocalPlayerOut, v[1] != 0),
        EE_NETMSG_SESS_COMMS_SLEEP => ee_base::plat_core::time::PlatSleep(v[1] as u32),
        _ => {
            // protocol 0 into the frame (the slot info's local address)
            h(O::SsGetProtocol, 0, &[ptr(&mut session_objs::frame(f).protocol)]);
        }
    }
}

fn start(f: P, v: &[i64; 8]) {
    if v[0] == EE_NETMSG_SESS_START_PREP {
        // the offset start time, the physics' draw, the protocol's status
        let mut protocol: P = 0;
        h(O::SsStartTimeFromMsg, f, &[v[1]]);
        h(O::SsPhysicsRandom, f, &[]);
        h(O::SsGetProtocol, f, &[ptr(&mut protocol)]);
        h(O::SsProtocolStatus, f, &[protocol as i64]);
    } else if v[0] == EE_NETMSG_SESS_START_RESUME {
        super::globals::graphics_post_resume();
        while h(O::SsGfxPaused, f, &[]) != 0 {
            ee_base::plat_core::time::PlatSleep(1);
        }
    } else {
        h(O::SsPhysicsPostStart, f, &[]);
        while h(O::SsTimerRunning, f, &[]) == 0 {
            ee_base::plat_core::time::PlatSleep(1);
        }
        h(O::SsMainGameUiState0, f, &[]);
    }
}

/// EEClient::HandleSlotUpdate: this client booted from the canceled game when the host's slot (0) has become open
/// (closed), else the slot's line refreshed.
#[no_mangle]
pub extern "C" fn ee_gamecpp_client_slot_update(slot: u8, player: &EeAppOptionsPlayerFields) {
    if player.mSlot == 0 && player.mType == 3 {
        post(0, hp(O::ClNewBootDefault, 0, &[]));
    } else {
        h(O::SsLobbyUpdatePlayer, 0, &[slot as i64]);
    }
}

/// EEClient::CreateXferFile: the file a transfer writes (a saved game, scenario or campaign by its extension, under
/// the data folder); NULL for another extension, a name under five characters, a file already there with the same
/// checksum, or one that cannot be created and opened for writing.
#[no_mangle]
pub extern "C" fn ee_gamecpp_client_xfer_file(name: *const c_void, size: c_ulong) -> *mut c_void {
    let name = name as i64;
    if h(O::ClNameLength, 0, &[name]) < 5 {
        return std::ptr::null_mut();
    }
    let extension = Owned::new(O::SsNewString, O::SsDeleteString);
    h(O::ClExtension, 0, &[name, extension.0 as i64]);
    let file = hp(O::ClNewFileSpec, 0, &[]) as i64;
    h(O::ClFileSetVolume, 0, &[file]);
    let folder = if h(O::ClExtIsSaved, 0, &[extension.0 as i64]) != 0 {
        0
    } else if h(O::ClExtIsScenario, 0, &[extension.0 as i64]) != 0 {
        1
    } else if h(O::ClExtIsCampaign, 0, &[extension.0 as i64]) != 0 {
        2
    } else {
        h(O::ClFileFree, 0, &[file]);
        return std::ptr::null_mut();
    };
    h(O::ClFileSetDirectory, 0, &[file, folder]);
    h(O::ClFileSetFile, 0, &[file, name]);
    let ok = if h(O::ClFileOpenRead, 0, &[file]) != 0 {
        h(O::ClFileClose, 0, &[file]);
        let header = Owned::new(O::SsNewHeader, O::SsDeleteHeader);
        h(O::SsHeaderSetVersions, 0, &[header.0 as i64]);
        super::persist_parts::ee_gamecpp_hdr_load_from_file(header.0 as *mut c_void, file as P as *mut c_void);
        if h(O::ClHeaderChecksum, 0, &[header.0 as i64]) as c_ulong == size {
            false
        } else {
            h(O::ClFileDeleteOnDisk, 0, &[file]);
            h(O::ClFileCreateWrite, 0, &[file]) != 0
        }
    } else {
        h(O::ClFileDeleteOnDisk, 0, &[file]);
        h(O::ClFileCreateWrite, 0, &[file]) != 0
    };
    if !ok {
        h(O::ClFileFree, 0, &[file]);
        return std::ptr::null_mut();
    }
    file as usize as *mut c_void
}

/// EEClient::HandleConnectionTimeoutWarning: the server migration's countdown in the chat log ("host not
/// responding (%d seconds)").
#[no_mangle]
pub extern "C" fn ee_gamecpp_client_timeout_warning(remaining: c_ulong) {
    let text = Owned::new(O::SsNewString, O::SsDeleteString);
    let seconds = Owned::new(O::SsNewString, O::SsDeleteString);
    let format = Owned::new(O::SsNewString, O::SsDeleteString);
    h(O::SsGetString, 0, &[SS_IDS_HOST_IS_NOT_RESPONDING, text.0 as i64]);
    h(O::SsGetString, 0, &[SS_IDS_SECONDS, seconds.0 as i64]);
    h(O::ClTimeoutFormat, 0, &[format.0 as i64, text.0 as i64, seconds.0 as i64]);
    h(O::ClSprintfSeconds, 0, &[text.0 as i64, format.0 as i64, (remaining / 1000) as i64]);
    h(O::SsChatText, 0, &[text.0 as i64, 1]);
}

/// The session host (ee_sim `EeNetSessHostFn`; `call` an `EeNetSessCall`: the frame and the values).
///
/// # Safety
/// `call` is the session's live call record; its frame is EEServer.cpp's `EESessionFrame`.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_session_host(op: u32, call: *mut c_void) -> i64 {
    let call = &*(call as *const EeNetSessCall);
    let f = call.frame as P;
    let v = call.v;
    match op {
        EE_NETMSG_SESS_OP_LOCK => lock(f, &v),
        EE_NETMSG_SESS_OP_FACT => fact(f, &v),
        EE_NETMSG_SESS_OP_SELF_GET => member(f, v[0], v[1], false, 0),
        EE_NETMSG_SESS_OP_SELF_SET => member(f, v[0], v[1], true, v[2]),
        EE_NETMSG_SESS_OP_REG => reg(f, &v),
        EE_NETMSG_SESS_OP_SEND => send(f, &v),
        EE_NETMSG_SESS_OP_MSG => message(f, &v),
        EE_NETMSG_SESS_OP_UI => ui(f, &v),
        EE_NETMSG_SESS_OP_WON => {
            won(f, v[0], v[1], v[2]);
            0
        }
        EE_NETMSG_SESS_OP_SOUND => {
            sound(f, &v);
            0
        }
        EE_NETMSG_SESS_OP_WORLD => world(f, &v),
        EE_NETMSG_SESS_OP_COMMS => {
            comms(f, &v);
            0
        }
        _ => {
            start(f, &v);
            0
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_player_fields_read_and_write_as_the_cpp_did() {
        let mut r = EeAppOptionsPlayerFields::default();
        assert_eq!(player_field(&mut r, EE_NETMSG_SESS_FIELD_TYPE, true, 2), 2);
        assert_eq!(r.mType, 2);
        let bits = 0.25f32.to_bits() as i64;
        assert_eq!(player_field(&mut r, EE_NETMSG_SESS_FIELD_PING, true, bits), bits);
        assert_eq!(r.mPing, 0.25);
        assert_eq!(player_field(&mut r, EE_NETMSG_SESS_FIELD_READY, true, 7), 1);
        assert!(r.mIsReady);
        assert_eq!(player_field(&mut r, EE_NETMSG_SESS_FIELD_LOADED_TYPE, true, -1), -1);
        // the default is the engine (the port's member)
        assert_eq!(player_field(&mut r, 99, true, 3), 3);
        assert_eq!(r.mEngine, 3);
        assert_eq!(player_field(&mut r, EE_NETMSG_SESS_FIELD_TEAM, false, 9), 0);
    }

    #[test]
    fn the_members_pick_the_server_or_the_client() {
        assert!(server_member(EE_NETMSG_SESS_SELF_HAS_LEFT, || true));
        assert!(!server_member(EE_NETMSG_SESS_SELF_HAS_LEFT, || false));
        assert!(!server_member(EE_NETMSG_SESS_SELF_FAST_ID, || false));
        assert!(server_member(EE_NETMSG_SESS_SELF_LAG_MASK, || false));
        assert!(server_member(EE_NETMSG_SESS_SELF_VOTED, || false));
        assert!(!server_member(EE_NETMSG_SESS_SELF_UI_SLOT, || true));
        assert!(!server_member(1000, || true));
    }

    #[test]
    fn the_members_read_and_write_as_their_types() {
        let (mut b, mut c, mut s) = (false, 0u8, 0u16);
        assert_eq!(rw_bool(&mut b, true, 5), 1);
        assert_eq!(rw_bool(&mut b, false, 0), 1);
        assert_eq!(rw_u8(&mut c, true, 0x1ff), 0xff);
        assert_eq!(rw_u16(&mut s, true, 0x1_0002), 2);
        assert_eq!(rw_u16(&mut s, false, 9), 2);
    }
}
