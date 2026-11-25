//! The game's lock-step server and client objects' own bodies (were EEServer.cpp's and EEClient.cpp's members): the
//! constructors' members and locks, the destructors, the fast selection's reset, the session's frame
//! (`EESession`), a client's connection request, the connection response sent to a slot, a refused client, a slot's
//! full player information, a ping statistic, the first free player slot, the warning sounds, a transfer's progress.
//! Each C++ call or member is a one-statement case of EEServer.cpp's `EECore11OpC2` and EEClient.cpp's
//! `EECore11OpC3` ([`super::EeGameCpp`]'s `C11` ops).
//!
//! Network code: a single-player game makes a server (the constructor, the destructor) but sends nothing; the rest
//! only a LAN game (the lead's gates) runs.

use std::collections::BTreeMap;
use std::ffi::{c_long, c_ulong, c_void};
use std::sync::{Mutex, MutexGuard};

use ee_sim::netmsg::session::{ee_netmsg_session, EeNetSessCall, EE_NETMSG_SESS_DO_CONNECTION_REQUEST};

use super::server::{ee_gamecpp_session_host, SS_SLOT_NONE};
use super::{h, hf, hp, pa, ptr, rec, Owned, O, P};
use crate::app::game_options::EeAppOptionsPlayerFields;

/// `kConnectionRemovedByHost` (NEMessageID.h; EEServer.cpp asserts it).
const CONNECTION_REMOVED_BY_HOST: i64 = 5;

/// The server's network slot of each lobby slot (was EEServer's `mSendToSlots`, a `std::map`: kept in key order, an
/// insert keeps a key's first value). One server at a time: its constructor and destructor clear it.
static SEND_TO_SLOTS: Mutex<BTreeMap<u8, u8>> = Mutex::new(BTreeMap::new());

fn send_to_slots() -> MutexGuard<'static, BTreeMap<u8, u8>> {
    SEND_TO_SLOTS.lock().unwrap_or_else(|e| e.into_inner())
}

/// EEServer::GetSendToSlot: a lobby slot's network slot, 0xff when none.
#[no_mangle]
pub extern "C" fn ee_gamecpp_server_send_to_slot(lobby: u8) -> u8 {
    send_to_slots().get(&lobby).copied().unwrap_or(0xff)
}

/// EEServer::GetLobbySlot: the first lobby slot (in slot order) that uses a network slot, 0 when none.
pub fn lobby_slot(send_to: u8) -> u8 {
    send_to_slots().iter().find(|(_, &v)| v == send_to).map_or(0, |(&k, _)| k)
}

/// The send-to slots cleared (a connection request; the server made or deleted).
pub fn send_to_clear() {
    send_to_slots().clear();
}

/// A lobby slot's network slot, unless it has one already (`std::map::insert`).
pub fn send_to_insert(lobby: u8, send_to: u8) {
    send_to_slots().entry(lobby).or_insert(send_to);
}

/// A lobby slot's network slot forgotten.
pub fn send_to_erase(lobby: u8) {
    send_to_slots().remove(&lobby);
}

/// EEServer's plain members from `mVoted` to `mHasLeftGame` (EEServer.cpp asserts the offsets). Written field by
/// field, never whole (its padding is not the server's).
#[repr(C)]
pub struct EeGamecppServerTail {
    pub voted: [bool; 256],
    pub vote_edict: u16,
    pub yes_votes: u16,
    pub no_votes: u16,
    pub lag_mask: u16,
    pub num_votes: u8,
    pub num_load_responses: u8,
    pub load_state: u8,
    pub num_dropped_loaders: u8,
    pub fast_selection: [u8; 16],
    pub u348: u8,
    pub fast_selection_id: u8,
    pub is_executing_selection: bool,
    pub is_migrating: bool,
    pub has_left_game: bool,
}

/// EEClient's plain members from `mHasLeftGame` to `mWasBooted` (EEClient.cpp asserts the offsets).
#[repr(C)]
pub struct EeGamecppClientTail {
    pub has_left_game: bool,
    pub ui_slot_number: u8,
    pub fast_selection: [u8; 16],
    pub uf2: u8,
    pub fast_selection_id: u8,
    pub is_executing_selection: bool,
    pub is_migrating: bool,
    pub allow_server_migration: bool,
    pub ignore_connection_timeout: bool,
    pub is_server_lost: bool,
    pub has_loaded: bool,
    pub server_lost_after_loading: bool,
    pub was_booted: bool,
}

/// The constructor's members (the votes, the loads, the lag mask, the flags), in the C++'s order.
pub fn server_members(t: &mut EeGamecppServerTail) {
    t.vote_edict = 0;
    t.voted = [false; 256];
    t.yes_votes = 0;
    t.no_votes = 0;
    t.num_votes = 0;
    t.num_load_responses = 0;
    t.load_state = 0;
    t.num_dropped_loaders = 0;
    t.has_left_game = false;
    t.lag_mask = 0;
    t.is_migrating = false;
}

/// ResetFastSelection: no selection executing, id 0, empty table.
pub fn server_reset_fast(t: &mut EeGamecppServerTail) {
    t.is_executing_selection = false;
    t.u348 = 0;
    t.fast_selection_id = 0;
    t.fast_selection = [0; 16];
}

/// The client's constructor's members: no lobby slot yet, server migration allowed.
pub fn client_members(t: &mut EeGamecppClientTail) {
    t.ui_slot_number = 0xff;
    t.has_left_game = false;
    t.is_migrating = false;
    t.allow_server_migration = true;
    t.ignore_connection_timeout = false;
    t.is_server_lost = false;
    t.has_loaded = false;
    t.server_lost_after_loading = false;
    t.was_booted = false;
}

/// EEClient::ResetFastSelection.
pub fn client_reset_fast(t: &mut EeGamecppClientTail) {
    t.is_executing_selection = false;
    t.uf2 = 0;
    t.fast_selection_id = 0;
    t.fast_selection = [0; 16];
}

/// The first player index below `max` whose world player is NULL (`present(i)` false), else 0xff.
pub fn free_player_slot(max: u8, mut present: impl FnMut(u8) -> bool) -> u8 {
    (0..max).find(|&i| !present(i)).unwrap_or(0xff)
}

/// EEServer's constructor body: the members, the fast selection, the three locks (search, fast selection, attempt).
#[no_mangle]
pub extern "C" fn ee_gamecpp_server_init(server: *mut c_void) {
    let s = server as P;
    // SAFETY: the server's plain tail, no C++ call while written
    unsafe {
        let t = &mut *rec::<EeGamecppServerTail>(O::C11SvTail, s);
        server_members(t);
        server_reset_fast(t);
    }
    send_to_clear();
    h(O::C11SvCreateSearchLock, s, &[]);
    h(O::C11SvCreateFastLock, s, &[]);
    h(O::C11SvCreateAttemptLock, s, &[]);
}

/// EEServer's destructor body: the threads stopped, the three locks released.
#[no_mangle]
pub extern "C" fn ee_gamecpp_server_drop(server: *mut c_void) {
    let s = server as P;
    h(O::C11SvStopThreads, s, &[]);
    h(O::C11SvDestroySearchLock, s, &[]);
    h(O::C11SvDestroyFastLock, s, &[]);
    h(O::C11SvDestroyAttemptLock, s, &[]);
    send_to_clear();
}

/// EEClient's constructor body: the members, the fast selection, the lock.
#[no_mangle]
pub extern "C" fn ee_gamecpp_client_init(client: *mut c_void) {
    let c = client as P;
    // SAFETY: the client's plain tail, no C++ call while written
    unsafe {
        let t = &mut *rec::<EeGamecppClientTail>(O::C11ClTail, c);
        client_members(t);
        client_reset_fast(t);
    }
    h(O::C11ClCreateLock, c, &[]);
}

/// EEClient's destructor body.
#[no_mangle]
pub extern "C" fn ee_gamecpp_client_drop(client: *mut c_void) {
    let c = client as P;
    h(O::C11ClStopThreads, c, &[]);
    h(O::C11ClDestroyLock, c, &[]);
}

/// EEClient::ResetFastSelection.
#[no_mangle]
pub extern "C" fn ee_gamecpp_client_reset_fast(client: *mut c_void) {
    // SAFETY: the client's plain tail
    unsafe { client_reset_fast(&mut *rec::<EeGamecppClientTail>(O::C11ClTail, client as P)) }
}

/// The session host as ee_sim's host type.
unsafe extern "C" fn session_host(op: u32, call: *mut EeNetSessCall) -> i64 {
    ee_gamecpp_session_host(op, call as *mut c_void)
}

/// EESession: the server's (`server`) or client's member `what` (EE_NETMSG_SESS_DO_*) over the session host, in a
/// frame of its own (a join request's player and version for a connection request).
///
/// # Safety
/// `comms` is the live server or client; `player` and `version` are NULL or live for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_session(
    comms: *mut c_void,
    server: bool,
    what: u32,
    a: i64,
    b: i64,
    c: i64,
    d: i64,
    player: *const c_void,
    version: *const c_void,
) -> i64 {
    let frame = hp(O::C11FrameNew, 0, &[comms as i64, server as i64, player as i64, version as i64]);
    let r = ee_netmsg_session(session_host, frame as *mut c_void, server, what, a, b, c, d);
    h(O::C11FrameDelete, frame, &[]);
    r
}

/// A lobby slot player's plain members.
///
/// # Safety
/// `player` is a live EEGameOptionsPlayer; no C++ call while the reference is held.
unsafe fn fields<'a>(player: P) -> &'a mut EeAppOptionsPlayerFields {
    &mut *rec::<EeAppOptionsPlayerFields>(O::C11PlayerFields, player)
}

/// A client's connection request (firewall flag, version, name, binary checksum) on the server: its player (team 0,
/// human, the default citizens, no ping, not ready), its firewall flag, then the request checked and answered
/// (ProcessConnectionRequest); the request released.
pub fn take_connection_request(server: P, request: P) {
    let player = Owned::new(O::C11PlayerNew, O::C11PlayerDelete);
    h(O::C11PlayerNameFromRequest, player.0, &[pa(request), 2]);
    let citizens = h(O::SsCitizensDefault, 0, &[]);
    // SAFETY: the new player's plain members
    unsafe {
        let r = fields(player.0);
        r.mTeam = 0;
        r.mType = 1;
        r.mStartingCitizens = citizens as c_ulong;
        r.mPing = 0.0;
        r.mIsReady = false;
    }
    h(O::C11RequestFirewall, server, &[pa(request)]);
    let origin = h(O::SsMsgOrigin, 0, &[pa(request)]);
    let version = Owned::new(O::SsNewString, O::SsDeleteString);
    h(O::C11RequestString, version.0, &[pa(request), 1]);
    let checksum = h(O::C11RequestChecksum, 0, &[pa(request)]);
    // SAFETY: the server, the player and the version live through the call
    unsafe {
        ee_gamecpp_session(
            server as *mut c_void,
            true,
            EE_NETMSG_SESS_DO_CONNECTION_REQUEST,
            origin,
            checksum,
            0,
            0,
            player.0 as *const c_void,
            version.0 as *const c_void,
        );
    }
    h(O::SsMsgDereference, 0, &[pa(request)]);
}

/// The connection response (error, send-to slot, lobby slot) sent to `slot` from the server's, the slot forgotten
/// after it when `unregister`.
pub fn send_connection_response(server: P, slot: i64, error: i64, send_to: i64, ui_slot: i64, unregister: bool) {
    let response = hp(O::C11SvNewResponse, server, &[error, send_to, ui_slot]);
    h(O::SsMsgSetOriginServer, 0, &[pa(response)]);
    h(O::C11SvSendTo, server, &[slot, pa(response)]);
    h(O::SsMsgDereference, 0, &[pa(response)]);
    if unregister {
        h(O::C11SvUnregister, server, &[slot]);
    }
}

/// A lobby slot's player options sent to everyone (a copy of the plain members and the name; the loaded type, the
/// local flag and the transfer id as a new player has them), then the slot's line refreshed.
pub fn send_full_player_information(server: P, options: P) {
    let player = Owned::new(O::C11PlayerNew, O::C11PlayerDelete);
    // SAFETY: two live players' plain members, no C++ call while held
    let slot = unsafe {
        let from = *fields(options);
        let to = fields(player.0);
        to.mType = from.mType;
        to.mColor = from.mColor;
        to.mTeam = from.mTeam;
        to.mStartingCitizens = from.mStartingCitizens;
        to.mPing = from.mPing;
        to.mIsReady = from.mIsReady;
        to.mSlot = from.mSlot;
        to.mB36 = from.mB36;
        to.mEngine = from.mEngine;
        to.mSlot
    };
    h(O::C11PlayerNameCopy, player.0, &[pa(options)]);
    h(O::C11SvSendPlayerInfo, server, &[pa(player.0)]);
    h(O::SsLobbyUpdatePlayer, 0, &[slot as i64]);
}

/// EEServer::RefuseClient: the client removed from its lobby slot (removed-by-host response), its slot forgotten and
/// the slot's info sent again.
///
/// # Safety
/// `server` and `player` are live.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_server_refuse(server: *mut c_void, player: *mut c_void) {
    let (s, p) = (server as P, player as P);
    let slot = fields(p).mSlot as i64;
    let send_to = ee_gamecpp_server_send_to_slot(slot as u8) as i64;
    send_connection_response(s, send_to, CONNECTION_REMOVED_BY_HOST, SS_SLOT_NONE, 0xff, true);
    send_to_erase(slot as u8);
    send_full_player_information(s, p);
}

/// A human slot's connection quality sent to everyone (under the options' read lock).
pub fn send_ping_stat(server: P, player: i64, ping: f32) {
    h(O::C11OptionsReadLock, 0, &[]);
    // SAFETY: the options' slot's plain members, read under the lock
    let kind = unsafe { (*rec::<EeAppOptionsPlayerFields>(O::C11OptionsPlayerFields, player as P)).mType };
    if kind == 1 || kind == 2 {
        hf(O::C11SvPingStat, server, &[player], &[ping]);
    }
    h(O::C11OptionsReadUnlock, 0, &[]);
}

/// EEServer::GetFreePlayerSlot: the first world player index below the maximum that holds no player, else 0xff.
#[no_mangle]
pub extern "C" fn ee_gamecpp_server_free_player_slot(server: *mut c_void) -> u8 {
    let max = h(O::C11SvMaxPlayers, server as P, &[]) as u8;
    free_player_slot(max, |i| hp(O::SsWorldPlayer, 0, &[i as i64]) != 0)
}

/// EEServer::PlayWarningSound: 1 out of sync, 2 resending.
#[no_mangle]
pub extern "C" fn ee_gamecpp_server_warning_sound(sound: c_long) {
    if sound == 1 || sound == 2 {
        let mut sample: c_ulong = 0;
        let mut instance: c_ulong = 0;
        h(O::C11WarningSound, 0, &[sound as i64, ptr(&mut sample), ptr(&mut instance)]);
    }
}

/// EEServer::HandleXferStatus: a file transfer's progress sent for the lobby slot of the network slot.
#[no_mangle]
pub extern "C" fn ee_gamecpp_server_xfer_status(server: *mut c_void, slot: u8, percent: u8, xfer_id: u32, status: u8) {
    let s = server as P;
    let player = lobby_slot(slot) as i64;
    h(O::C11SvXferStatus, s, &[player, percent as i64, xfer_id as i64, status as i64]);
}

/// EEServer::ServerSlot48: message 0x12 posted to the UI thread (two zero parameters).
#[no_mangle]
pub extern "C" fn ee_gamecpp_server_post_12() {
    let mut first: c_long = 0;
    let mut second: c_long = 0;
    h(O::C11UiPost12, 0, &[ptr(&mut first), ptr(&mut second)]);
}

/// The server of a session frame.
pub fn frame_server(frame: P) -> P {
    hp(O::C11FrameComms, frame, &[])
}

#[cfg(test)]
mod tests {
    use super::super::server::SS_SLOT_SERVER;
    use super::*;

    #[test]
    fn the_send_to_slots_keep_the_maps_order_and_first_insert() {
        send_to_clear();
        assert_eq!(ee_gamecpp_server_send_to_slot(3), 0xff);
        assert_eq!(lobby_slot(7), 0);
        send_to_insert(5, 7);
        send_to_insert(2, 7);
        send_to_insert(5, 9);
        assert_eq!(ee_gamecpp_server_send_to_slot(5), 7);
        assert_eq!(lobby_slot(7), 2);
        send_to_erase(2);
        assert_eq!(lobby_slot(7), 5);
        send_to_clear();
        assert_eq!(ee_gamecpp_server_send_to_slot(5), 0xff);
    }

    #[test]
    fn the_tails_have_the_cpp_offsets() {
        assert_eq!(std::mem::offset_of!(EeGamecppServerTail, vote_edict), 256);
        assert_eq!(std::mem::offset_of!(EeGamecppServerTail, lag_mask), 262);
        assert_eq!(std::mem::offset_of!(EeGamecppServerTail, num_votes), 264);
        assert_eq!(std::mem::offset_of!(EeGamecppServerTail, num_dropped_loaders), 267);
        assert_eq!(std::mem::offset_of!(EeGamecppServerTail, fast_selection), 268);
        assert_eq!(std::mem::offset_of!(EeGamecppServerTail, u348), 284);
        assert_eq!(std::mem::offset_of!(EeGamecppServerTail, has_left_game), 288);
        assert_eq!(std::mem::offset_of!(EeGamecppClientTail, ui_slot_number), 1);
        assert_eq!(std::mem::offset_of!(EeGamecppClientTail, uf2), 18);
        assert_eq!(std::mem::offset_of!(EeGamecppClientTail, is_executing_selection), 20);
        assert_eq!(std::mem::offset_of!(EeGamecppClientTail, was_booted), 27);
    }

    #[test]
    fn the_constructors_set_what_the_cpp_set() {
        let mut t: EeGamecppServerTail = unsafe { std::mem::zeroed() };
        t.voted[7] = true;
        t.vote_edict = 3;
        t.has_left_game = true;
        t.fast_selection[15] = 9;
        t.is_executing_selection = true;
        server_members(&mut t);
        server_reset_fast(&mut t);
        assert!(!t.voted.iter().any(|&v| v));
        assert_eq!(
            (t.vote_edict, t.has_left_game, t.fast_selection, t.is_executing_selection),
            (0, false, [0; 16], false)
        );
        let mut c: EeGamecppClientTail = unsafe { std::mem::zeroed() };
        c.was_booted = true;
        c.fast_selection_id = 4;
        client_members(&mut c);
        client_reset_fast(&mut c);
        assert_eq!(
            (c.ui_slot_number, c.allow_server_migration, c.was_booted, c.fast_selection_id),
            (0xff, true, false, 0)
        );
    }

    #[test]
    fn the_free_slot_is_the_first_empty_below_the_maximum() {
        assert_eq!(free_player_slot(4, |i| i < 2), 2);
        assert_eq!(free_player_slot(4, |_| true), 0xff);
        assert_eq!(free_player_slot(0, |_| false), 0xff);
        assert_eq!(free_player_slot(3, |_| false), 0);
    }

    #[test]
    fn the_slot_constants_are_the_engine_s() {
        assert_eq!((SS_SLOT_SERVER, SS_SLOT_NONE, CONNECTION_REMOVED_BY_HOST), (0xfe, 0xff, 5));
    }
}
