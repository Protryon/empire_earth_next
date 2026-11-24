//! The session host (was EEServer.cpp's `EESessionHost` and its helpers): what ee_sim's session decisions
//! (`netmsg::session`, the server's and client's members) ask of the C++ world, the lobby, the UI and the network
//! engine. Each C++ call or member is a one-statement case of EEServer.cpp's `EECore9OpD` / `EECore9OpD2`
//! ([`super::EeGameCpp`]'s `Ss` ops, `p` the session's frame); [`ee_gamecpp_session_host`] is the host function
//! `EESession` hands to `ee_netmsg_session`.
//!
//! Network code: single-player games never run it; only a LAN game (the lead's gates) does.

use std::ffi::{c_long, c_ulong, c_void};

use ee_sim::netmsg::session::*;

use super::session_objs::{ee_gamecpp_server_send_to_slot, send_to_clear, send_to_erase, send_to_insert};
use super::{h, hp, ptr, session_objs, Owned, O, P};
use crate::app::game_options::EeAppOptionsPlayerFields;

/// `kSlotServer`, `kSlotNone` (NEMessageID.h).
pub const SS_SLOT_SERVER: i64 = 0xfe;
pub const SS_SLOT_NONE: i64 = 0xff;

/// A member of a C++ object, by the address its op answers, as the type it has.
#[derive(Clone, Copy)]
enum Member {
    Bool(O),
    Byte(O),
    Short(O),
}

/// Reads a member (the integer it holds) and, when `set`, writes `v` first (as the member's type, truncated).
fn member_rw(f: P, m: Member, set: bool, v: i64) -> i64 {
    match m {
        Member::Bool(op) => {
            let at = hp(op, f, &[]) as *mut bool;
            // SAFETY: the address of the server's or client's bool member
            unsafe {
                if set {
                    *at = v != 0;
                }
                *at as i64
            }
        }
        Member::Byte(op) => {
            let at = hp(op, f, &[]) as *mut u8;
            // SAFETY: an unsigned char member
            unsafe {
                if set {
                    *at = v as u8;
                }
                *at as i64
            }
        }
        Member::Short(op) => {
            let at = hp(op, f, &[]) as *mut u16;
            // SAFETY: an unsigned short member
            unsafe {
                if set {
                    *at = v as u16;
                }
                *at as i64
            }
        }
    }
}

/// The member of a session member id (the server's or the client's where both have it).
fn member_of(server: bool, which: i64) -> Member {
    use Member::*;
    match which {
        EE_NETMSG_SESS_SELF_HAS_LEFT => Bool(if server { O::SsSHasLeft } else { O::SsCHasLeft }),
        EE_NETMSG_SESS_SELF_MIGRATING => Bool(if server { O::SsSMigrating } else { O::SsCMigrating }),
        EE_NETMSG_SESS_SELF_FAST_ID => Byte(if server { O::SsSFastId } else { O::SsCFastId }),
        EE_NETMSG_SESS_SELF_EXECUTING => Bool(if server { O::SsSExecuting } else { O::SsCExecuting }),
        EE_NETMSG_SESS_SELF_LOAD_STATE => Byte(O::SsLoadState),
        EE_NETMSG_SESS_SELF_DROPPED_LOADERS => Byte(O::SsDroppedLoaders),
        EE_NETMSG_SESS_SELF_VOTE_EDICT => Short(O::SsVoteEdict),
        EE_NETMSG_SESS_SELF_YES_VOTES => Short(O::SsYesVotes),
        EE_NETMSG_SESS_SELF_NO_VOTES => Short(O::SsNoVotes),
        EE_NETMSG_SESS_SELF_NUM_VOTES => Byte(O::SsNumVotes),
        EE_NETMSG_SESS_SELF_LAG_MASK => Short(O::SsLagMask),
        EE_NETMSG_SESS_SELF_UI_SLOT => Byte(O::SsUiSlot),
        EE_NETMSG_SESS_SELF_ALLOW_MIGRATION => Bool(O::SsAllowMigration),
        EE_NETMSG_SESS_SELF_IGNORE_TIMEOUT => Bool(O::SsIgnoreTimeout),
        EE_NETMSG_SESS_SELF_SERVER_LOST => Bool(O::SsServerLost),
        EE_NETMSG_SESS_SELF_HAS_LOADED => Bool(O::SsHasLoaded),
        EE_NETMSG_SESS_SELF_LOST_AFTER_LOADING => Bool(O::SsLostAfterLoading),
        _ => Bool(O::SsWasBooted),
    }
}

/// The server's and client's members (EESessionMember): the voted flags by index (a negative index set clears all
/// 256), the others by id.
fn member(f: P, which: i64, index: i64, set: bool, v: i64) -> i64 {
    if which == EE_NETMSG_SESS_SELF_VOTED {
        let voted = hp(O::SsVoted, f, &[]) as *mut bool;
        // SAFETY: the server's bool mVoted[256]
        unsafe {
            if set && index < 0 {
                for k in 0..256 {
                    *voted.add(k) = false;
                }
                return 0;
            }
            let at = voted.add((index & 0xff) as usize);
            if set {
                *at = v != 0;
            }
            return *at as i64;
        }
    }
    let server = h(O::SsFrameServer, f, &[]) != 0;
    member_rw(f, member_of(server, which), set, v)
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
            h(if on { O::SsLockCt } else { O::SsUnlockCt }, f, &[]);
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
            h(if on { O::SsStopPing } else { O::SsStartPing }, f, &[]);
            return 0;
        }
        EE_NETMSG_SESS_LOCK_SEARCH => hp(O::SsSearchLock, f, &[]),
        EE_NETMSG_SESS_LOCK_ATTEMPT => hp(O::SsAttemptLock, f, &[]),
        EE_NETMSG_SESS_LOCK_FAST => {
            if h(O::SsFrameServer, f, &[]) != 0 {
                hp(O::SsServerFastLock, f, &[])
            } else {
                hp(O::SsClientFastLock, f, &[])
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
        EE_NETMSG_SESS_FACT_OPTIONS_MULTIPLAYER => O::SsOptionsMultiplayer,
        EE_NETMSG_SESS_FACT_LOAD_TYPE => O::SsLoadType,
        EE_NETMSG_SESS_FACT_GAME_STATE => O::SsGameState,
        EE_NETMSG_SESS_FACT_DEFAULT_COLOR => O::SsDefaultColor,
        EE_NETMSG_SESS_FACT_CITIZENS_DEFAULT => O::SsCitizensDefault,
        EE_NETMSG_SESS_FACT_VERSION_MATCH => O::SsVersionMatch,
        EE_NETMSG_SESS_FACT_BINARY_CHECKSUM => O::SsBinaryChecksum,
        EE_NETMSG_SESS_FACT_CURRENT_CT => O::SsCurrentCt,
        EE_NETMSG_SESS_FACT_CTS_IN_PHYSICS => O::SsCtsInPhysics,
        EE_NETMSG_SESS_FACT_HUMANS => O::SsHumans,
        EE_NETMSG_SESS_FACT_MP_SESSION => O::SsMpSession,
        EE_NETMSG_SESS_FACT_PLAYER_NUMBER => O::SsPlayerNumber,
        EE_NETMSG_SESS_FACT_CURRENT_PT => O::SsCurrentPt,
        EE_NETMSG_SESS_FACT_MY_SEND_TO_SLOT => O::SsMySendToSlot,
        EE_NETMSG_SESS_FACT_NUM_PLAYERS => O::SsNumPlayers,
        EE_NETMSG_SESS_FACT_WORLD_PLAYERS => O::SsWorldPlayers,
        EE_NETMSG_SESS_FACT_TIME => O::SsTime,
        EE_NETMSG_SESS_FACT_WON => O::SsWon,
        EE_NETMSG_SESS_FACT_SCREEN_ACTIVE => O::SsScreenActive,
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
    h(op, f, &[v[1]])
}

fn reg(f: P, v: &[i64; 8]) -> i64 {
    let (r, kind, x) = (v[0], v[1], v[2]);
    match kind {
        EE_NETMSG_SESS_REG_LOAD_OPTIONS => h(O::SsRegLoadOptions, f, &[r, x]),
        EE_NETMSG_SESS_REG_LOAD_LOBBY => h(O::SsRegLoadLobby, f, &[r, x]),
        EE_NETMSG_SESS_REG_LOAD_REG => h(O::SsRegLoadReg, f, &[r, x]),
        EE_NETMSG_SESS_REG_STORE_OPTIONS => h(O::SsRegStoreOptions, f, &[r, x]),
        EE_NETMSG_SESS_REG_STORE_LOBBY => h(O::SsRegStoreLobby, f, &[r, x]),
        EE_NETMSG_SESS_REG_GET | EE_NETMSG_SESS_REG_SET => {
            let fields = hp(O::SsReg, f, &[r]) as *mut EeAppOptionsPlayerFields;
            // SAFETY: the register's plain members (EEGameOptionsPlayer's base), no C++ call while held
            let fields = unsafe { &mut *fields };
            player_field(fields, x, kind == EE_NETMSG_SESS_REG_SET, v[3])
        }
        EE_NETMSG_SESS_REG_NAME_CLEAR => h(O::SsRegNameClear, f, &[r]),
        EE_NETMSG_SESS_REG_NAME_COPY => h(O::SsRegNameCopy, f, &[r, x]),
        EE_NETMSG_SESS_REG_NAME_ICMP => h(O::SsRegNameIcmp, f, &[r, x]),
        EE_NETMSG_SESS_REG_NAME_EQ => h(O::SsRegNameEq, f, &[r, x]),
        EE_NETMSG_SESS_REG_NAME_FROM_MSG => h(O::SsRegNameFromMsg, f, &[r, x]),
        _ => h(O::SsRegNameIcmpPlayer, f, &[r, x]),
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
        EE_NETMSG_SESS_SEND_LOCAL_RESPONSE => h(O::SsSendLocalResponse, f, &[v[1], v[2], v[3], v[4]]),
        EE_NETMSG_SESS_SEND_FULL_OPTIONS => h(O::SsSendFullOptions, f, &[v[1]]),
        EE_NETMSG_SESS_SEND_PLAYER_INFO => {
            let info = hp(O::SsNewPlayerInfo, f, &[v[1]]);
            if v[3] != 0 {
                h(O::SsMsgSetOriginServer, f, &[info as i64]);
            }
            h(O::SsLocalInput, f, &[info as i64, v[2]]);
            0
        }
        EE_NETMSG_SESS_SEND_NEW_RECORD => match v[2] {
            // the values' count
            0 => h(O::SsNewRecord0, f, &[v[1]]),
            2 => h(O::SsNewRecord2, f, &[v[1], v[3], v[4]]),
            3 => h(O::SsNewRecord3, f, &[v[1], v[3], v[4], v[5]]),
            _ => h(O::SsNewRecord4, f, &[v[1], v[3], v[4], v[5], v[6]]),
        },
        EE_NETMSG_SESS_SEND_PING_STAT => {
            session_objs::send_ping_stat(session_objs::frame_server(f), v[1] as u8 as i64, f32::from_bits(v[2] as u32));
            0
        }
        EE_NETMSG_SESS_SEND_SLOT_INFO => {
            // player v1 at send-to slot v2: the host's address, none, a client's
            let (index, slot) = (v[1] as u8 as i64, v[2] as u8 as i64);
            let message = if slot == SS_SLOT_SERVER {
                hp(O::SsSlotInfoServer, f, &[index])
            } else {
                let address = Owned::new(O::SsNewString, O::SsDeleteString);
                if slot != SS_SLOT_NONE {
                    h(O::SsClientAddress, f, &[slot, address.0 as i64]);
                }
                hp(O::SsSlotInfoRecord, f, &[index, slot, address.0 as i64])
            };
            h(O::SsCommsLocalInput, f, &[message as i64, SS_SLOT_NONE]);
            0
        }
        EE_NETMSG_SESS_SEND_PHYSICS_TICK => h(O::SsNewPhysicsTick, f, &[v[1], v[2], v[3], v[4], v[5]]),
        _ => h(O::SsConnectionCheckResponse, f, &[v[1]]),
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
        EE_NETMSG_SESS_MSG_LOG_RESPONSE => h(O::SsMsgLogResponse, f, &[m]),
        EE_NETMSG_SESS_MSG_LOCAL => h(O::SsLocalInput, f, &[m, v[2]]),
        EE_NETMSG_SESS_MSG_BROADCAST => {
            h(O::SsMsgSetOriginServer, f, &[m]);
            h(O::SsBroadcast, f, &[m])
        }
        EE_NETMSG_SESS_MSG_SEND_ALL => {
            h(O::SsMsgSetOriginServer, f, &[m]);
            h(O::SsSendToAll, f, &[m]);
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
            let ids = h(O::SsIdsMsgGame, f, &[]);
            h(O::SsGetString, f, &[ids, name.0 as i64]);
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
            h(O::SsSprintfRetry, f, &[text.0 as i64, format.0 as i64]);
            h(O::SsDisplayMessage, f, &[text.0 as i64, 0, 0]);
        }
        EE_NETMSG_SESS_UI_LEAVE => {
            // back to the game list, the reason shown (with the host's version and ours) or the boot posted, then
            // disconnected
            let text = Owned::new(O::SsNewString, O::SsDeleteString);
            let format = Owned::new(O::SsNewString, O::SsDeleteString);
            if h(O::SsWon, f, &[]) != 0 {
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
            h(O::SsDisconnect, f, &[]);
        }
        _ => {
            let player = hp(O::SsReg, f, &[v[2]]) as *const EeAppOptionsPlayerFields;
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
        EE_NETMSG_SESS_WORLD_PLAYER => h(O::SsWorldPlayer, f, &[v[1]]),
        EE_NETMSG_SESS_WORLD_PLAYER_BY_SLOT => h(O::SsWorldPlayerBySlot, f, &[v[1]]),
        EE_NETMSG_SESS_WORLD_IS_HUMAN => h(O::SsPlIsHuman, pl, &[]),
        EE_NETMSG_SESS_WORLD_SLOT => h(O::SsPlSlot, pl, &[]),
        EE_NETMSG_SESS_WORLD_HAS_HUMAN => h(O::SsPlHasHuman, pl, &[]),
        EE_NETMSG_SESS_WORLD_ID => h(O::SsPlId, pl, &[]),
        EE_NETMSG_SESS_WORLD_SET_SLOT => h(O::SsPlSetSlot, pl, &[v[2]]),
        EE_NETMSG_SESS_WORLD_SET_FIREWALL => h(O::SsPlSetFirewall, pl, &[v[2]]),
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
        if h(O::SsHeaderLoad, f, &[header.0 as i64, file as i64]) != 0 && h(O::SsMultiplayerGame, f, &[]) == 0 {
            let mut xfer_id: c_ulong = 0;
            let send_to = ee_gamecpp_server_send_to_slot(slot as u8) as i64;
            h(O::SsXferToClient, f, &[file as i64, header.0 as i64, send_to, ptr(&mut xfer_id)]);
        }
    }
}

fn comms(f: P, v: &[i64; 8]) {
    let op = match v[0] {
        EE_NETMSG_SESS_COMMS_SET_CT => O::SsSetCt,
        EE_NETMSG_SESS_COMMS_SET_CTS_IN_PHYSICS => O::SsSetCtsInPhysics,
        EE_NETMSG_SESS_COMMS_INCREMENT_CTS => O::SsIncrementCts,
        EE_NETMSG_SESS_COMMS_SET_MP_SESSION => O::SsSetMpSession,
        EE_NETMSG_SESS_COMMS_SET_HUMANS => O::SsSetHumans,
        EE_NETMSG_SESS_COMMS_SET_PT => {
            let mut tick = v[1] as c_ulong;
            h(O::SsSetPt, f, &[ptr(&mut tick)]);
            return;
        }
        EE_NETMSG_SESS_COMMS_REGISTER => O::SsRegisterSlot,
        EE_NETMSG_SESS_COMMS_UNREGISTER => O::SsUnregisterSlot,
        EE_NETMSG_SESS_COMMS_SEND_TO_CLEAR => return send_to_clear(),
        EE_NETMSG_SESS_COMMS_SEND_TO_INSERT => return send_to_insert(v[1] as u8, v[2] as u8),
        EE_NETMSG_SESS_COMMS_SEND_TO_ERASE => return send_to_erase(v[1] as u8),
        EE_NETMSG_SESS_COMMS_ABORT_XFER => O::SsAbortXfer,
        EE_NETMSG_SESS_COMMS_REMOVE_IN_PROGRESS => O::SsRemoveInProgress,
        EE_NETMSG_SESS_COMMS_XFER_GAME_FILE => return xfer_game_file(f, v[1], v[2]),
        EE_NETMSG_SESS_COMMS_START_GAME => O::SsStartGame,
        EE_NETMSG_SESS_COMMS_PAUSE_GAME => O::SsPauseGame,
        EE_NETMSG_SESS_COMMS_BECOME_CLIENT => {
            // the host removed: the server deleted, a client in its place
            h(O::SsCommsStopPing, f, &[]);
            h(O::SsCommsResetXfer, f, &[]);
            h(O::SsUnregisterSlot, f, &[SS_SLOT_SERVER]);
            h(O::SsRegisterSlot, f, &[SS_SLOT_SERVER]);
            if h(O::SsCommsPresent, f, &[]) != 0 {
                h(O::SsCommsDelete, f, &[]);
                h(O::SsCommsNewClient, f, &[]);
                h(O::SsClientStartThreads, f, &[]);
            }
            return;
        }
        EE_NETMSG_SESS_COMMS_LOWEST_CTS => O::SsLowestCts,
        EE_NETMSG_SESS_COMMS_DISCONNECT => O::SsDisconnect,
        EE_NETMSG_SESS_COMMS_LOCAL_OUT => O::SsLocalOut,
        EE_NETMSG_SESS_COMMS_SLEEP => return ee_base::plat_core::time::PlatSleep(v[1] as u32),
        _ => O::SsFrameProtocol,
    };
    h(op, f, &[v[1], v[2], v[3]]);
}

fn start(f: P, v: &[i64; 8]) {
    if v[0] == EE_NETMSG_SESS_START_PREP {
        // the offset start time, the physics' draw, the protocol's status
        let mut protocol: P = 0;
        h(O::SsStartTimeFromMsg, f, &[v[1]]);
        h(O::SsPhysicsRandom, f, &[]);
        h(O::SsNumTeams, f, &[]);
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
        h(O::ClHeaderLoad, 0, &[file, header.0 as i64]);
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
    let ids = h(O::ClIdsNotResponding, 0, &[]);
    h(O::SsGetString, 0, &[ids, text.0 as i64]);
    let ids = h(O::ClIdsSeconds, 0, &[]);
    h(O::SsGetString, 0, &[ids, seconds.0 as i64]);
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
        assert!(matches!(member_of(true, EE_NETMSG_SESS_SELF_HAS_LEFT), Member::Bool(O::SsSHasLeft)));
        assert!(matches!(member_of(false, EE_NETMSG_SESS_SELF_HAS_LEFT), Member::Bool(O::SsCHasLeft)));
        assert!(matches!(member_of(false, EE_NETMSG_SESS_SELF_FAST_ID), Member::Byte(O::SsCFastId)));
        assert!(matches!(member_of(true, EE_NETMSG_SESS_SELF_LAG_MASK), Member::Short(O::SsLagMask)));
        assert!(matches!(member_of(true, 1000), Member::Bool(O::SsWasBooted)));
    }
}
