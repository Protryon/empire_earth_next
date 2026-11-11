//! The lobby's game room (StagingLogic.cpp, and LobbyModel.cpp's LobbyGame and LobbyPlayer): the room messages
//! between the host (the captain) and the players, who acts on which, the ready state's sounds, a game's join
//! rules, and the listing summary a game is advertised with. The C++ keeps the objects (the game, its clients and
//! players, the join op), the events, dialogs and sounds, and the game code's hooks, which read and write their own
//! fields after these.
//!
//! Room messages are `WriteBuffer(0, true)` buffers (no length prefix; shorts little-endian, a bool one byte); on a
//! LAN they travel inside the game channel's staging message (`lan::staging`).
//!
//! | type | message          | fields                        | sent by | acted on by                          |
//! |------|------------------|-------------------------------|---------|--------------------------------------|
//! | 1    | join request     | ping, the game code's         | joiner  | the host, from a client with no player |
//! | 2    | join reply       | the game's reply              | host    | a joiner, from the captain           |
//! | 3    | player joined    | id, ping, ready, the game's   | host    | all, from the captain                |
//! | 4    | ready request    | ready                         | player  | the host, from a player whose flag changes |
//! | 5    | ready changed    | id, ready                     | host    | all, from the captain                |
//! | 6    | game dissolved   |                               | host    | the others, from the captain         |
//! | 7    | start game       |                               | host    | all, from the captain                |
//! | 8    | ping report      | ping                          | player  | the host                             |
//! | 9    | ping changed     | id, ping                      | host    | all, from the captain                |
//! | 10   | client kicked    | id, ban                       | host    | all, from the captain                |
//! | else | the game code's  |                               |         | the game code                        |
//!
//! While a join waits for its reply (`mJoinStatus` 1) every other message is dropped.

use std::ffi::c_void;

use crate::ffi::{EeWonByteSink, EeWonUnitSink};
use crate::wire::{PastEnd, Reader, Writer};

pub(crate) const JOIN_REQUEST: u8 = 1;
pub(crate) const JOIN_REPLY: u8 = 2;
pub(crate) const PLAYER_JOINED: u8 = 3;
pub(crate) const READY_REQUEST: u8 = 4;
pub(crate) const READY_CHANGED: u8 = 5;
pub(crate) const GAME_DISSOLVED: u8 = 6;
pub(crate) const START_GAME: u8 = 7;
pub(crate) const PING_REPORT: u8 = 8;
pub(crate) const PING_CHANGED: u8 = 9;
pub(crate) const CLIENT_KICKED: u8 = 10;

/// `LobbyGameType`: a LAN game, an Internet game (a routing server's room), a direct-connect game.
pub(crate) const GAME_LAN: i32 = 1;
pub(crate) const GAME_INTERNET: i32 = 2;
pub(crate) const GAME_DIRECT: i32 = 3;

/// Join statuses (`GetJoinStatus`, `HandleJoinRequest`): joinable, full, in progress, not invited (4, a bad request,
/// and 8, a name already in the game, are the C++'s).
pub(crate) const JOIN_OK: i16 = 0;
pub(crate) const JOIN_FULL: i16 = 2;
pub(crate) const JOIN_IN_PROGRESS: i16 = 3;
pub(crate) const JOIN_NOT_INVITED: i16 = 5;

/// The all-ready sound plays at most this often (seconds).
pub(crate) const ALL_READY_SOUND_GAP: i64 = 15;

fn room(kind: u8) -> Writer {
    let mut w = Writer::new(0, true);
    w.append_byte(kind);
    w
}

/// Joiner: the join request's head (type 1, the local ping); the game code's request follows (`GetJoinRequest`).
pub fn join_request_head(ping: u16) -> Vec<u8> {
    let mut w = room(JOIN_REQUEST);
    w.append_short(ping as i16);
    w.to_message()
}

/// A player as `LobbyPlayer::WriteToBuffer` begins it: the ping and the ready flag; the game code's data follows.
pub fn player_head(ping: u16, ready: bool) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    w.append_short(ping as i16);
    w.append_bool(ready);
    w.to_message()
}

/// Host: a new player's announcement's head (type 3, the client id); the player (`player_head` and the game
/// code's data) follows.
pub fn player_joined_head(id: u16) -> Vec<u8> {
    let mut w = room(PLAYER_JOINED);
    w.append_short(id as i16);
    w.to_message()
}

/// Player: asks the host to set its ready flag (type 4).
pub fn ready_request(ready: bool) -> Vec<u8> {
    let mut w = room(READY_REQUEST);
    w.append_bool(ready);
    w.to_message()
}

/// Host: a player's new ready flag (type 5).
pub fn ready_changed(id: u16, ready: bool) -> Vec<u8> {
    let mut w = room(READY_CHANGED);
    w.append_short(id as i16);
    w.append_bool(ready);
    w.to_message()
}

/// Host: the game is dissolved (type 6).
pub fn game_dissolved() -> Vec<u8> {
    room(GAME_DISSOLVED).to_message()
}

/// Host: start the game (type 7).
pub fn start_game() -> Vec<u8> {
    room(START_GAME).to_message()
}

/// Player: its ping to the host (type 8).
pub fn ping_report(ping: u16) -> Vec<u8> {
    let mut w = room(PING_REPORT);
    w.append_short(ping as i16);
    w.to_message()
}

/// Host: a player's ping (type 9).
pub fn ping_changed(id: u16, ping: u16) -> Vec<u8> {
    let mut w = room(PING_CHANGED);
    w.append_short(id as i16);
    w.append_short(ping as i16);
    w.to_message()
}

/// Host: a client kicked (`ban` false) or banned (type 10).
pub fn client_kicked(id: u16, ban: bool) -> Vec<u8> {
    let mut w = room(CLIENT_KICKED);
    w.append_short(id as i16);
    w.append_bool(ban);
    w.to_message()
}

/// A room message's fixed fields as `HandleMessage`'s handlers read them; `rest` is where the game code's part
/// begins (for types 1, 2 and 3 and the game's own types; after the fields for the others).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct RoomMsg {
    pub kind: u8,
    pub id: u16,
    /// A ready flag (types 4, 5) or a ban (10).
    pub flag: bool,
    pub ping: u16,
    pub rest: usize,
}

/// `HandleMessage`'s read: `Ok(None)` for a message dropped while a join waits for its reply, `Err` for a message
/// too short for its fields (the handler's read threw and nothing was done: no handler acts before its reads).
pub fn read_room(bytes: &[u8], joining: bool) -> Result<Option<RoomMsg>, PastEnd> {
    let mut r = Reader::new(bytes);
    let kind = r.read_byte()?;
    if joining && kind != JOIN_REPLY {
        return Ok(None);
    }
    let mut m = RoomMsg { kind, ..RoomMsg::default() };
    match kind {
        PLAYER_JOINED => m.id = r.read_short()? as u16,
        READY_REQUEST => m.flag = r.read_bool()?,
        READY_CHANGED | CLIENT_KICKED => {
            m.id = r.read_short()? as u16;
            m.flag = r.read_bool()?;
        }
        PING_REPORT => m.ping = r.read_short()? as u16,
        PING_CHANGED => {
            m.id = r.read_short()? as u16;
            m.ping = r.read_short()? as u16;
        }
        _ => {}
    }
    m.rest = r.pos();
    Ok(Some(m))
}

/// What the receiver knows of the sender and itself when a room message arrives.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Roles {
    /// The sender is the game's captain (`IsCaptain(1)`).
    pub sender_is_captain: bool,
    /// We host the game (`IAmCaptain`).
    pub i_am_captain: bool,
    /// The sender has a player in the game, and whether it is ready.
    pub sender_has_player: bool,
    pub sender_ready: bool,
}

/// Whether the receiver acts on the message at all (each handler's guards before the objects are looked up); the
/// game code's own types always reach the game.
pub fn room_accepts(m: &RoomMsg, roles: &Roles) -> bool {
    match m.kind {
        JOIN_REQUEST => !roles.sender_has_player,
        JOIN_REPLY | PLAYER_JOINED | READY_CHANGED | START_GAME | PING_CHANGED | CLIENT_KICKED => {
            roles.sender_is_captain
        }
        READY_REQUEST => roles.i_am_captain && roles.sender_has_player && roles.sender_ready != m.flag,
        GAME_DISSOLVED => roles.sender_is_captain && !roles.i_am_captain,
        PING_REPORT => roles.i_am_captain && roles.sender_has_player,
        _ => true,
    }
}

/// `HandleStartGame`'s `LobbyExit` code: 2 for the host (which also marks the game in progress), 1 for a player.
pub fn start_exit_code(i_am_captain: bool) -> i32 {
    if i_am_captain {
        2
    } else {
        1
    }
}

/// `UpdateReadyState`'s inputs: whether every player but the captain is ready (`AreAllReady`), the state remembered
/// from the last call, the time (`time(NULL)`), whether we host and whether our own player is ready.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct ReadyInput {
    pub all_ready: bool,
    pub last_all_ready: bool,
    pub now: i64,
    pub last_ready_time: i64,
    pub is_host: bool,
    pub last_ready: bool,
    pub i_am_ready: bool,
}

/// The sound `UpdateReadyState` plays (if the staging sounds are on).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonReadySound {
    Silent = 0,
    AllReady = 1,
    NotReady = 2,
}

/// `UpdateReadyState`'s outputs: the sound and the remembered state.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ReadyOutput {
    pub sound: EeWonReadySound,
    pub last_all_ready: bool,
    pub last_ready_time: i64,
    pub last_ready: bool,
}

/// `UpdateReadyState`: the all-ready sound when the game becomes all ready, at most every 15 s; a player (not the
/// host) whose own flag went off hears the not-ready sound.
pub fn ready_state(i: &ReadyInput) -> ReadyOutput {
    let mut o = ReadyOutput {
        sound: EeWonReadySound::Silent,
        last_all_ready: i.all_ready,
        last_ready_time: i.last_ready_time,
        last_ready: i.last_ready,
    };
    if i.all_ready {
        if !i.last_all_ready && i.now.wrapping_sub(i.last_ready_time) >= ALL_READY_SOUND_GAP {
            o.last_ready_time = i.now;
            o.sound = EeWonReadySound::AllReady;
        }
    } else if !i.is_host && i.last_ready != i.i_am_ready {
        o.last_ready = i.i_am_ready;
        if !i.i_am_ready {
            o.sound = EeWonReadySound::NotReady;
        }
    }
    o
}

/// `IsFull`: every open slot taken (never full without slots).
pub fn is_full(players: i32, open_slots: i32) -> bool {
    open_slots > 0 && players >= open_slots
}

/// `GetJoinStatus` before the game code's hook: full, invite-only for someone neither invited nor the host, in
/// progress; `None` asks the hook.
pub fn join_status(full: bool, invite_only: bool, invited: bool, is_host: bool, in_progress: bool) -> Option<i16> {
    if full {
        Some(JOIN_FULL)
    } else if invite_only && !invited && !is_host {
        Some(JOIN_NOT_INVITED)
    } else if in_progress {
        Some(JOIN_IN_PROGRESS)
    } else {
        None
    }
}

/// Host, `HandleJoinRequest`'s first checks: full, then in progress (the name check and the game's follow).
pub fn host_join_check(full: bool, in_progress: bool) -> i16 {
    if full {
        JOIN_FULL
    } else if in_progress {
        JOIN_IN_PROGRESS
    } else {
        JOIN_OK
    }
}

/// `MatchesRoomSpec`: room flag 4 lists Internet games, flag 8 (without 4) LAN and direct-connect games.
pub fn matches_room_spec(flags: i32, game_type: i32) -> bool {
    if flags & 4 != 0 {
        game_type == GAME_INTERNET
    } else if flags & 8 != 0 {
        game_type == GAME_LAN || game_type == GAME_DIRECT
    } else {
        false
    }
}

/// A game's listing summary (`WriteSummary` / `ReadSummary`). An Internet game is listed by its address and has no
/// name or join flags in its summary; the others carry the product id instead of an address.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Summary {
    pub game_type: i32,
    pub address: [u8; 6],
    pub product_id: u16,
    pub in_progress: bool,
    pub name: Vec<u16>,
    pub skill: i32,
    pub has_password: bool,
    pub invite_only: bool,
    pub ask_to_join: bool,
    pub players: i32,
    pub open_slots: i32,
}

/// `WriteSummary`'s fields (the game code's summary follows): the skill's low byte, the counts' low 16 bits.
pub fn write_summary(s: &Summary) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    if s.game_type == GAME_INTERNET {
        w.append_bytes(&s.address);
    } else {
        w.append_short(s.product_id as i16);
    }
    w.append_bool(s.in_progress);
    if s.game_type != GAME_INTERNET {
        w.append_wstring(&s.name, 2);
    }
    w.append_byte(s.skill as u8);
    if s.game_type != GAME_INTERNET {
        let mut flags = u8::from(s.has_password);
        if s.invite_only {
            flags |= 2;
        }
        if s.ask_to_join {
            flags |= 4;
        }
        w.append_byte(flags);
    }
    w.append_short(s.players as i16);
    w.append_short(s.open_slots as i16);
    w.to_message()
}

/// Which of a summary's fields `read_summary` read (`Summary` holds them); the original assigned each to the game as
/// it read it, so a short summary leaves the ones before the cut set.
pub(crate) const SUMMARY_ADDRESS: u32 = 1;
pub(crate) const SUMMARY_IN_PROGRESS: u32 = 2;
pub(crate) const SUMMARY_NAME: u32 = 4;
pub(crate) const SUMMARY_SKILL: u32 = 8;
pub(crate) const SUMMARY_FLAGS: u32 = 0x10;
pub(crate) const SUMMARY_PLAYERS: u32 = 0x20;
pub(crate) const SUMMARY_OPEN_SLOTS: u32 = 0x40;

/// `read_summary`'s answer: `ok` false for another product or a short summary (then `ReadSummary` returns false
/// without the game's hook), the fields read, and where the game code's summary begins.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct SummaryRead {
    pub ok: bool,
    pub read: u32,
    pub summary: Summary,
    pub rest: usize,
}

/// `ReadSummary` for a game of `game_type` whose product id must be `product_id`: a skill outside 0..5 is 0, the
/// counts are sign-extended shorts.
pub fn read_summary(bytes: &[u8], game_type: i32, product_id: u16) -> SummaryRead {
    let mut out = SummaryRead { summary: Summary { game_type, ..Summary::default() }, ..SummaryRead::default() };
    let mut r = Reader::new(bytes);
    let s = &mut out.summary;
    let read = &mut out.read;
    let mut body = || -> Result<bool, PastEnd> {
        if game_type == GAME_INTERNET {
            s.address.copy_from_slice(r.read_bytes(6)?);
            *read |= SUMMARY_ADDRESS;
        } else {
            let id = r.read_short()? as u16;
            if id != product_id {
                return Ok(false);
            }
            s.product_id = id;
        }
        s.in_progress = r.read_bool()?;
        *read |= SUMMARY_IN_PROGRESS;
        if game_type != GAME_INTERNET {
            s.name = r.read_wstring(2)?;
            *read |= SUMMARY_NAME;
        }
        let skill = i32::from(r.read_byte()? as i8);
        s.skill = if (0..5).contains(&skill) { skill } else { 0 };
        *read |= SUMMARY_SKILL;
        if game_type != GAME_INTERNET {
            let flags = r.read_byte()?;
            s.has_password = flags & 1 != 0;
            s.invite_only = flags & 2 != 0;
            s.ask_to_join = flags & 4 != 0;
            *read |= SUMMARY_FLAGS;
        }
        s.players = i32::from(r.read_short()?);
        *read |= SUMMARY_PLAYERS;
        s.open_slots = i32::from(r.read_short()?);
        *read |= SUMMARY_OPEN_SLOTS;
        Ok(true)
    };
    out.ok = body().unwrap_or(false);
    out.rest = r.pos();
    out
}

/// Any of the room messages from its fields (`RoomMsg`; `rest` unused): the head for types 1 and 3, the whole
/// message for the others; an unknown type is the type byte alone.
pub fn room_message(m: &RoomMsg) -> Vec<u8> {
    match m.kind {
        JOIN_REQUEST => join_request_head(m.ping),
        PLAYER_JOINED => player_joined_head(m.id),
        READY_REQUEST => ready_request(m.flag),
        READY_CHANGED => ready_changed(m.id, m.flag),
        PING_REPORT => ping_report(m.ping),
        PING_CHANGED => ping_changed(m.id, m.ping),
        CLIENT_KICKED => client_kicked(m.id, m.flag),
        kind => room(kind).to_message(),
    }
}

// ---- the C ABI (StagingLogic.cpp, LobbyModel.cpp)

/// A room message's fields (`RoomMsg`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeWonRoomMsg {
    pub kind: u8,
    pub id: u16,
    pub flag: bool,
    pub ping: u16,
    pub rest: usize,
}

impl From<RoomMsg> for EeWonRoomMsg {
    fn from(m: RoomMsg) -> Self {
        EeWonRoomMsg { kind: m.kind, id: m.id, flag: m.flag, ping: m.ping, rest: m.rest }
    }
}

impl From<&EeWonRoomMsg> for RoomMsg {
    fn from(m: &EeWonRoomMsg) -> Self {
        RoomMsg { kind: m.kind, id: m.id, flag: m.flag, ping: m.ping, rest: m.rest }
    }
}

/// `Roles` for the C++.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeWonRoomRoles {
    pub sender_is_captain: bool,
    pub i_am_captain: bool,
    pub sender_has_player: bool,
    pub sender_ready: bool,
}

unsafe fn input<'a>(bytes: *const u8, length: usize) -> &'a [u8] {
    if bytes.is_null() || length == 0 {
        &[]
    } else {
        // SAFETY: `length` bytes (each function's contract)
        unsafe { std::slice::from_raw_parts(bytes, length) }
    }
}

/// Reads a room message (`read_room`): 0 read (into `out`), 1 dropped while joining, 2 too short.
///
/// # Safety
/// `bytes` holds `length` bytes; `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_room_read(
    bytes: *const u8,
    length: usize,
    joining: bool,
    out: *mut EeWonRoomMsg,
) -> i32 {
    match read_room(unsafe { input(bytes, length) }, joining) {
        Ok(Some(m)) => {
            // SAFETY: writable (the contract)
            unsafe { *out = m.into() };
            0
        }
        Ok(None) => 1,
        Err(PastEnd) => 2,
    }
}

/// `room_accepts`.
///
/// # Safety
/// Both point to valid structs.
#[no_mangle]
pub unsafe extern "C" fn ee_won_room_accepts(m: *const EeWonRoomMsg, roles: *const EeWonRoomRoles) -> bool {
    // SAFETY: valid (the contract)
    let (m, r) = unsafe { (&*m, &*roles) };
    let roles = Roles {
        sender_is_captain: r.sender_is_captain,
        i_am_captain: r.i_am_captain,
        sender_has_player: r.sender_has_player,
        sender_ready: r.sender_ready,
    };
    room_accepts(&m.into(), &roles)
}

/// `room_message` into the sink.
///
/// # Safety
/// `m` is valid; the sink's contract.
#[no_mangle]
pub unsafe extern "C" fn ee_won_room_message(m: *const EeWonRoomMsg, sink: EeWonByteSink, context: *mut c_void) {
    // SAFETY: valid (the contract)
    let b = room_message(&unsafe { &*m }.into());
    unsafe { crate::ffi::give(sink, context, &b) }
}

/// `player_head` into the sink.
///
/// # Safety
/// The sink's contract.
#[no_mangle]
pub unsafe extern "C" fn ee_won_room_player_head(ping: u16, ready: bool, sink: EeWonByteSink, context: *mut c_void) {
    unsafe { crate::ffi::give(sink, context, &player_head(ping, ready)) }
}

#[no_mangle]
pub extern "C" fn ee_won_room_start_exit_code(i_am_captain: bool) -> i32 {
    start_exit_code(i_am_captain)
}

/// `UpdateReadyState`'s state: the inputs, and on return the sound and the remembered state (`last_*` updated).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonReadyState {
    pub all_ready: bool,
    pub last_all_ready: bool,
    pub is_host: bool,
    pub last_ready: bool,
    pub i_am_ready: bool,
    pub now: i64,
    pub last_ready_time: i64,
    pub sound: EeWonReadySound,
}

/// `ready_state` in place.
///
/// # Safety
/// `s` is valid and writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_room_ready_state(s: *mut EeWonReadyState) {
    // SAFETY: valid (the contract)
    let s = unsafe { &mut *s };
    let o = ready_state(&ReadyInput {
        all_ready: s.all_ready,
        last_all_ready: s.last_all_ready,
        now: s.now,
        last_ready_time: s.last_ready_time,
        is_host: s.is_host,
        last_ready: s.last_ready,
        i_am_ready: s.i_am_ready,
    });
    s.sound = o.sound;
    s.last_all_ready = o.last_all_ready;
    s.last_ready_time = o.last_ready_time;
    s.last_ready = o.last_ready;
}

#[no_mangle]
pub extern "C" fn ee_won_game_is_full(players: i32, open_slots: i32) -> bool {
    is_full(players, open_slots)
}

/// `join_status`: the status, or -1 to ask the game code's hook.
#[no_mangle]
pub extern "C" fn ee_won_game_join_status(
    full: bool,
    invite_only: bool,
    invited: bool,
    is_host: bool,
    in_progress: bool,
) -> i16 {
    join_status(full, invite_only, invited, is_host, in_progress).unwrap_or(-1)
}

#[no_mangle]
pub extern "C" fn ee_won_game_host_join_check(full: bool, in_progress: bool) -> i16 {
    host_join_check(full, in_progress)
}

#[no_mangle]
pub extern "C" fn ee_won_game_matches_room_spec(flags: i32, game_type: i32) -> bool {
    matches_room_spec(flags, game_type)
}

/// A summary's fields but the name (`Summary`), and a read's answer (`SummaryRead`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeWonGameSummary {
    pub game_type: i32,
    pub address: [u8; 6],
    pub product_id: u16,
    pub in_progress: bool,
    pub has_password: bool,
    pub invite_only: bool,
    pub ask_to_join: bool,
    pub skill: i32,
    pub players: i32,
    pub open_slots: i32,
    /// For a read: true when it succeeded, the `SUMMARY_*` bits read, where the game code's part begins.
    pub ok: bool,
    pub read: u32,
    pub rest: usize,
}

/// `write_summary` into the sink; `name` is UTF-16 LE bytes.
///
/// # Safety
/// `s` is valid; `name` holds `name_length` bytes; the sink's contract.
#[no_mangle]
pub unsafe extern "C" fn ee_won_game_write_summary(
    s: *const EeWonGameSummary,
    name: *const u8,
    name_length: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    // SAFETY: valid (the contract)
    let s = unsafe { &*s };
    let summary = Summary {
        game_type: s.game_type,
        address: s.address,
        product_id: s.product_id,
        in_progress: s.in_progress,
        name: crate::text::utf16_from_le_bytes(unsafe { input(name, name_length) }),
        skill: s.skill,
        has_password: s.has_password,
        invite_only: s.invite_only,
        ask_to_join: s.ask_to_join,
        players: s.players,
        open_slots: s.open_slots,
    };
    unsafe { crate::ffi::give(sink, context, &write_summary(&summary)) }
}

/// `read_summary` into `out`; the name (when read) goes to the sink as UTF-16 units.
///
/// # Safety
/// `bytes` holds `length` bytes; `out` is writable; the sink's contract.
#[no_mangle]
pub unsafe extern "C" fn ee_won_game_read_summary(
    bytes: *const u8,
    length: usize,
    game_type: i32,
    product_id: u16,
    out: *mut EeWonGameSummary,
    name_sink: EeWonUnitSink,
    context: *mut c_void,
) {
    let r = read_summary(unsafe { input(bytes, length) }, game_type, product_id);
    let s = &r.summary;
    // SAFETY: writable (the contract)
    unsafe {
        *out = EeWonGameSummary {
            game_type,
            address: s.address,
            product_id: s.product_id,
            in_progress: s.in_progress,
            has_password: s.has_password,
            invite_only: s.invite_only,
            ask_to_join: s.ask_to_join,
            skill: s.skill,
            players: s.players,
            open_slots: s.open_slots,
            ok: r.ok,
            read: r.read,
            rest: r.rest,
        }
    };
    if r.read & SUMMARY_NAME != 0 {
        unsafe { name_sink(context, s.name.as_ptr(), s.name.len()) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn u(s: &str) -> Vec<u16> {
        s.encode_utf16().collect()
    }

    #[test]
    fn the_messages_bytes() {
        assert_eq!(join_request_head(0x1234), vec![1, 0x34, 0x12]);
        assert_eq!(player_head(0xfffe, true), vec![0xfe, 0xff, 1]);
        assert_eq!(player_joined_head(7), vec![3, 7, 0]);
        assert_eq!(ready_request(true), vec![4, 1]);
        assert_eq!(ready_changed(0x0102, false), vec![5, 2, 1, 0]);
        assert_eq!(game_dissolved(), vec![6]);
        assert_eq!(start_game(), vec![7]);
        assert_eq!(ping_report(300), vec![8, 0x2c, 1]);
        assert_eq!(ping_changed(2, 0xffff), vec![9, 2, 0, 0xff, 0xff]);
        assert_eq!(client_kicked(9, true), vec![10, 9, 0, 1]);
    }

    #[test]
    fn reads_give_back_the_fields() {
        let m = read_room(&ready_changed(0x0102, true), false).unwrap().unwrap();
        assert_eq!((m.kind, m.id, m.flag, m.rest), (5, 0x0102, true, 4));
        let m = read_room(&ping_changed(3, 40), false).unwrap().unwrap();
        assert_eq!((m.id, m.ping, m.rest), (3, 40, 5));
        let m = read_room(&[3, 1, 0, 0xfe, 0xff, 1, 9], false).unwrap().unwrap();
        assert_eq!((m.id, m.rest), (1, 3));
        let m = read_room(&[1, 5, 0, 42], false).unwrap().unwrap();
        assert_eq!((m.kind, m.rest), (1, 1));
        let m = read_room(&[0x64, 1, 2], false).unwrap().unwrap();
        assert_eq!((m.kind, m.rest), (0x64, 1));
        // a ready flag is any non-zero byte
        assert!(read_room(&[4, 2], false).unwrap().unwrap().flag);
    }

    #[test]
    fn short_messages_and_the_join_filter() {
        assert_eq!(read_room(&[], false), Err(PastEnd));
        assert_eq!(read_room(&[5, 1, 0], false), Err(PastEnd));
        assert_eq!(read_room(&[9, 1, 0, 2], false), Err(PastEnd));
        assert_eq!(read_room(&[8, 1], false), Err(PastEnd));
        assert_eq!(read_room(&[6], false).unwrap().unwrap().kind, 6);
        // while joining only the reply counts, short or not
        assert_eq!(read_room(&[5, 1], true), Ok(None));
        assert_eq!(read_room(&[2], true).unwrap().unwrap().kind, 2);
        assert_eq!(read_room(&[], true), Err(PastEnd));
    }

    #[test]
    fn who_acts() {
        let captain = Roles { sender_is_captain: true, ..Roles::default() };
        let host = Roles { i_am_captain: true, sender_has_player: true, ..Roles::default() };
        let m = |kind, flag| RoomMsg { kind, flag, ..RoomMsg::default() };
        assert!(room_accepts(&m(1, false), &Roles::default()));
        assert!(!room_accepts(&m(1, false), &host));
        for k in [2, 3, 5, 7, 9, 10] {
            assert!(room_accepts(&m(k, false), &captain));
            assert!(!room_accepts(&m(k, false), &host));
        }
        assert!(room_accepts(&m(4, true), &host));
        assert!(!room_accepts(&m(4, false), &host));
        assert!(!room_accepts(&m(4, true), &Roles { sender_has_player: false, ..host }));
        assert!(room_accepts(&m(6, false), &captain));
        assert!(!room_accepts(&m(6, false), &Roles { i_am_captain: true, ..captain }));
        assert!(room_accepts(&m(8, false), &host));
        assert!(!room_accepts(&m(8, false), &captain));
        assert!(room_accepts(&m(0x64, false), &Roles::default()));
        assert_eq!((start_exit_code(true), start_exit_code(false)), (2, 1));
    }

    #[test]
    fn ready_sounds() {
        let base = ReadyInput { all_ready: true, now: 100, last_ready_time: 85, ..ReadyInput::default() };
        let o = ready_state(&base);
        assert_eq!((o.sound, o.last_ready_time, o.last_all_ready), (EeWonReadySound::AllReady, 100, true));
        // too soon after the last one, or already all ready
        assert_eq!(ready_state(&ReadyInput { last_ready_time: 86, ..base }).sound, EeWonReadySound::Silent);
        assert_eq!(ready_state(&ReadyInput { last_ready_time: 86, ..base }).last_ready_time, 86);
        assert_eq!(ready_state(&ReadyInput { last_all_ready: true, ..base }).sound, EeWonReadySound::Silent);
        // a player whose flag went off
        let off = ReadyInput { last_ready: true, last_all_ready: true, ..ReadyInput::default() };
        let o = ready_state(&off);
        assert_eq!((o.sound, o.last_ready, o.last_all_ready), (EeWonReadySound::NotReady, false, false));
        let o = ready_state(&ReadyInput { i_am_ready: true, last_ready: false, ..off });
        assert_eq!((o.sound, o.last_ready), (EeWonReadySound::Silent, true));
        assert!(ready_state(&ReadyInput { is_host: true, ..off }).last_ready);
    }

    #[test]
    fn join_rules() {
        assert!(!is_full(3, 0));
        assert!(is_full(4, 4));
        assert!(!is_full(3, 4));
        assert_eq!(join_status(true, true, false, false, true), Some(JOIN_FULL));
        assert_eq!(join_status(false, true, false, false, true), Some(JOIN_NOT_INVITED));
        assert_eq!(join_status(false, true, true, false, true), Some(JOIN_IN_PROGRESS));
        assert_eq!(join_status(false, true, false, true, false), None);
        assert_eq!(host_join_check(true, true), JOIN_FULL);
        assert_eq!(host_join_check(false, true), JOIN_IN_PROGRESS);
        assert_eq!(host_join_check(false, false), JOIN_OK);
        assert!(matches_room_spec(4 | 8, GAME_INTERNET));
        assert!(!matches_room_spec(4 | 8, GAME_LAN));
        assert!(matches_room_spec(8, GAME_DIRECT));
        assert!(!matches_room_spec(1, GAME_LAN));
    }

    #[test]
    fn summaries_round_trip() {
        let lan = Summary {
            game_type: GAME_LAN,
            product_id: 0x1e0,
            in_progress: true,
            name: u("Bob's game"),
            skill: 3,
            has_password: true,
            ask_to_join: true,
            players: 2,
            open_slots: 8,
            ..Summary::default()
        };
        let b = write_summary(&lan);
        assert_eq!(&b[..5], &[0xe0, 1, 1, 10, 0]);
        let r = read_summary(&b, GAME_LAN, 0x1e0);
        assert!(r.ok);
        assert_eq!(r.summary, lan);
        assert_eq!(r.rest, b.len());
        assert_eq!(r.read, 0x7f & !SUMMARY_ADDRESS);
        let net = Summary {
            game_type: GAME_INTERNET,
            address: [0x1e, 0x61, 10, 0, 0, 7],
            skill: 1,
            players: -2,
            open_slots: 0x7fff,
            ..Summary::default()
        };
        let b = write_summary(&net);
        assert_eq!(b.len(), 6 + 1 + 1 + 4);
        let r = read_summary(&b, GAME_INTERNET, 0);
        assert_eq!(r.summary, net);
        assert_eq!(r.read, 0x7f & !(SUMMARY_NAME | SUMMARY_FLAGS));
    }

    #[test]
    fn odd_summaries() {
        // another product: nothing read
        let r = read_summary(&[1, 0, 0], GAME_LAN, 2);
        assert_eq!((r.ok, r.read), (false, 0));
        // a skill outside 0..5 is 0 (the byte is signed)
        let mut b = write_summary(&Summary { game_type: GAME_DIRECT, skill: 5, ..Summary::default() });
        assert_eq!(read_summary(&b, GAME_DIRECT, 0).summary.skill, 0);
        b[5] = 0xff;
        assert_eq!(read_summary(&b, GAME_DIRECT, 0).summary.skill, 0);
        b[5] = 4;
        assert_eq!(read_summary(&b, GAME_DIRECT, 0).summary.skill, 4);
        // a cut summary keeps what was read before the cut
        let r = read_summary(&b[..6], GAME_DIRECT, 0);
        assert!(!r.ok);
        assert_eq!(r.read, SUMMARY_IN_PROGRESS | SUMMARY_NAME | SUMMARY_SKILL);
        // the skill's low byte, the counts' low 16 bits
        let b = write_summary(&Summary { game_type: GAME_LAN, skill: 0x102, players: 0x10005, ..Summary::default() });
        let r = read_summary(&b, GAME_LAN, 0);
        assert_eq!((r.summary.skill, r.summary.players), (2, 5));
    }
}
