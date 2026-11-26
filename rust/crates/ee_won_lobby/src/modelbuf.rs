//! The lobby model's buffers (LobbyModelHost.cpp, R6 WON round 33): a game's listing summary written and read, its
//! details read, the join request, the host's join check and reply, the joiner's reading of the reply, the join
//! statuses' texts and a player's bytes, in Rust over one-statement C++ ops (`BufHost` in
//! LobbyModelHost.cpp), in the original's order. The bytes are staging.rs's; the buffers, the game's hooks and the
//! model objects stay the C++ ones (the ops hold them by pointer; a local buffer, iterator or list is made, held
//! and released by ops where the local was made and went out of scope).
//!
//! A `ReadBufferException` never unwinds through this code: the read ops catch it and answer `THREW`, and the Rust
//! takes the original's catch branch.
//!
//! Reach: the LAN game writes and reads summaries, sends, admits and answers a join, reads the reply and the
//! players' bytes; the refusals (a full or running game, a duplicate name, a short request, the
//! game code's refusal), a short reply, a details read and the status texts run only in won_lobby33_test's golden
//! cases.

use std::ffi::c_void;

/// The host: `(obj, op, a, b, c) -> result`.
pub type EeWonModelBufHostFn =
    Option<unsafe extern "C" fn(obj: *mut c_void, op: EeWonModelBufOp, a: isize, b: isize, c: isize) -> isize>;

/// The host's ops: each one C++ statement.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonModelBufOp {
    /// `rb->pos()`.
    BufPos = 1,
    /// `rb->data()`.
    BufData,
    /// `rb->length()`.
    BufLength,
    /// `rb->SetPos((int)b)`.
    BufSetPos,
    /// `return ReadOr([&] { return (intptr_t)rb->ReadShort(); });`.
    ReadShort,
    /// `return ReadOr([&] { return (intptr_t)rb->ReadBool(); });`.
    ReadBool,
    /// `new WriteBuffer(0, true)`.
    WriteNew,
    /// `delete wb`.
    WriteFree,
    /// `wb->AppendBytes(reinterpret_cast<const void*>(b), (unsigned long)c)`.
    AppendBytes,
    /// `wb->AppendByte((unsigned char)b)`.
    AppendByte,
    /// `wb->AppendShort((short)b)`.
    AppendShort,
    /// `wb->SetShort((unsigned long)b, (short)c)`.
    SetShort,
    /// `HeldConst(wb->ToByteBuffer(true).get())`.
    ToByteBuffer,
    /// `reinterpret_cast<RefCount*>(a)->Release()`.
    Release,
    /// `Held(new GUIStringData(UnitsText(reinterpret_cast<const uint16_t*>(a), (size_t)b)))`.
    TextOfUnits,
    /// `Held(new GUIStringData(reinterpret_cast<const char*>(a)))`.
    TextOfBytes,
    /// `GetEmptyGUIStringData()`.
    TextEmpty,
    /// `reinterpret_cast<GUIStringData*>(a)->mLength`.
    TextLength,
    /// `g->GetGameType()`.
    GameType,
    /// `memcpy(reinterpret_cast<void*>(a), g->GetAddress().GetSixByte(), 6)`.
    GameAddressSixByte,
    /// `GetLobbyProductId()`.
    ProductId,
    /// `g->IsInProgress()`.
    InProgress,
    /// `g->GetPassword().length() != 0`.
    HasPassword,
    /// `g->GetInviteOnly()`.
    InviteOnly,
    /// `g->GetAskToJoin()`.
    AskToJoin,
    /// `g->GetSkill()`.
    Skill,
    /// `g->GetNumPlayers()`.
    NumPlayers,
    /// `g->GetMaxPlayers()`.
    MaxPlayers,
    /// `SinkBytes(a, Utf16LE((std::wstring)g->GetName()))`.
    NameUtf16,
    /// `g->WriteSummaryHook(*wb)`.
    WriteSummaryHook,
    /// `g->M(LobbyMC::GameStoreSummary, a, b)`.
    StoreSummary,
    /// `return ReadOr([&] { return (intptr_t)g->ReadSummaryHook(*rb); });`.
    ReadSummaryHook,
    /// `return ReadOr([&] { return (intptr_t)g->ReadDetailsHook(*rb); });`.
    ReadDetailsHook,
    /// `g->MSet(LobbyMF::GameHaveDetails, true)`.
    SetHaveDetails,
    /// `g->GetPing()`.
    Ping,
    /// `g->GetJoinGameRequestHook(*wb)`.
    JoinRequestHook,
    /// `g->IsFull()`.
    IsFull,
    /// `GetLobbyConfig()`.
    ConfigGet,
    /// `reinterpret_cast<LobbyConfig*>(a)->mAllowDuplicateNames`.
    ConfigAllowDuplicates,
    /// `Held(g->GetClients().get())`.
    ClientsGet,
    /// `reinterpret_cast<LobbyClientList*>(a)->GetCount()`.
    ListCount,
    /// `reinterpret_cast<LobbyClientList*>(a)->ClientAt((size_t)b)`.
    ListAt,
    /// `reinterpret_cast<LobbyClient*>(a)->GetName() == reinterpret_cast<LobbyClient*>(b)->GetName()`.
    ClientSameName,
    /// `Held(LobbyPlayer::CreatePlayer())`.
    PlayerCreate,
    /// `reinterpret_cast<LobbyClient*>(a)->SetPlayer(reinterpret_cast<LobbyPlayer*>(b))`.
    ClientSetPlayer,
    /// `reinterpret_cast<LobbyPlayer*>(a)->SetPing((unsigned short)b)`.
    PlayerSetPing,
    /// `return ReadOr([&] { return (intptr_t)g->HandleJoinGameRequestHook(*rb, reinterpret_cast<LobbyClient*>(b)); });`.
    JoinRequestHookRead,
    /// `g->MSet(LobbyMF::GameNumPlayers, (long)a)`.
    SetNumPlayers,
    /// `new LobbyClientSetIterator(reinterpret_cast<LobbyClientList*>(a))`.
    IterNew,
    /// `reinterpret_cast<LobbyClientSetIterator*>(a)->HasMore()`.
    IterHasMore,
    /// `reinterpret_cast<LobbyClientSetIterator*>(a)->Next(reinterpret_cast<LobbyClient**>(b))`.
    IterNext,
    /// `delete reinterpret_cast<LobbyClientSetIterator*>(a)`.
    IterFree,
    /// `reinterpret_cast<LobbyClient*>(a)->GetClientId()`.
    ClientId,
    /// `reinterpret_cast<LobbyPlayer*>(a)->WriteToBuffer(*reinterpret_cast<WriteBuffer*>(b))`.
    PlayerWrite,
    /// `g->GetJoinGameReplyHook(reinterpret_cast<LobbyClient*>(a), *reinterpret_cast<WriteBuffer*>(b))`.
    JoinReplyHook,
    /// `g->HandleJoinRequest(*rb, reinterpret_cast<LobbyClient*>(b), *reinterpret_cast<WriteBuffer*>(c))`.
    HandleJoinRequest,
    /// `g->FindClient((unsigned short)a)`.
    FindClient,
    /// `reinterpret_cast<LobbyPlayer*>(a)->ReadFromBuffer(*reinterpret_cast<ReadBuffer*>(b))`.
    PlayerReadFrom,
    /// `return ReadOr([&] { g->HandleJoinGameReplyHook(*rb); return (intptr_t)0; });`.
    JoinReplyHookRead,
    /// `HeldText(LOBBY_STRING(LobbyGameStatus_Success_String))`.
    StatusSuccess,
    /// `HeldText(LOBBY_STRING(LobbyGameStatus_GameFull_String))`.
    StatusGameFull,
    /// `HeldText(LOBBY_STRING(LobbyGameStatus_GameInProgress_String))`.
    StatusGameInProgress,
    /// `HeldText(LOBBY_STRING(LobbyGameStatus_UnpackFailure_String))`.
    StatusUnpackFailure,
    /// `HeldText(LOBBY_STRING(LobbyGameStatus_NotInvited_String))`.
    StatusNotInvited,
    /// `HeldText(LOBBY_STRING(LobbyGameStatus_CaptainRejectedYou_String))`.
    StatusCaptainRejectedYou,
    /// `HeldText(LOBBY_STRING(LobbyGameStatus_InvalidPassword_String))`.
    StatusInvalidPassword,
    /// `HeldText(LOBBY_STRING(LobbyGameStatus_DuplicateName_String))`.
    StatusDuplicateName,
    /// `HeldText(g->GetStatusStringHook((short)a))`.
    StatusHook,
    /// `return ReadOr([&] { return (intptr_t)pl->ReadData(*rb); });`.
    PlayerReadData,
    /// `pl->SetPing((unsigned short)a)`.
    SelfSetPing,
    /// `pl->SetReady(a != 0)`.
    SelfSetReady,
    /// `pl->GetPing()`.
    PlayerPing,
    /// `pl->GetReady()`.
    PlayerReady,
    /// `pl->WriteData(*wb)`.
    PlayerWriteData,
}

/// The calls in (the object is the game, but for the player's calls).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonModelBufCall {
    /// `LobbyGame::WriteSummary(*(WriteBuffer*)a)`.
    WriteSummary = 1,
    /// `LobbyGame::ReadSummary(*(ReadBuffer*)a)`.
    ReadSummary,
    /// `LobbyGame::ReadDetails(*(ReadBuffer*)a)`.
    ReadDetails,
    /// `LobbyGame::GetJoinRequest()`: a held `ByteBuffer`.
    JoinRequest,
    /// `LobbyGame::HandleJoinRequest(*(ReadBuffer*)a, (LobbyClient*)b, *(WriteBuffer*)c)`.
    HandleJoinRequest,
    /// `LobbyGame::GetJoinReply(*(ReadBuffer*)a, (LobbyClient*)b)`: a held `ByteBuffer`.
    JoinReply,
    /// `LobbyGame::HandleJoinReply(*(ReadBuffer*)a)`.
    HandleJoinReply,
    /// `LobbyGame::GetStatusString(a)`: a held `GUIStringData`.
    StatusString,
    /// `LobbyPlayer::ReadFromBuffer(*(ReadBuffer*)a)` (the object is the player).
    PlayerReadFrom,
    /// `LobbyPlayer::WriteToBuffer(*(WriteBuffer*)a)` (the object is the player).
    PlayerWriteTo,
}

use crate::staging::EeWonGameSummary;
use EeWonModelBufOp as Op;

/// A read op whose statement threw `ReadBufferException` (LobbyModelHost.cpp's `ReadOr`).
const THREW: isize = isize::MIN;

/// The ops, `(op, a, b, c) -> result`.
type Out<'a> = &'a dyn Fn(Op, isize, isize, isize) -> isize;

unsafe extern "C" fn bytes_to_vec(context: *mut c_void, bytes: *const u8, length: usize) {
    if !bytes.is_null() && length != 0 {
        // SAFETY: the sink's contract: `context` is the Vec, `bytes` holds `length` bytes
        unsafe { (*(context as *mut Vec<u8>)).extend_from_slice(std::slice::from_raw_parts(bytes, length)) };
    }
}

unsafe extern "C" fn units_to_vec(context: *mut c_void, units: *const u16, count: usize) {
    if !units.is_null() && count != 0 {
        // SAFETY: as above, for units
        unsafe { (*(context as *mut Vec<u16>)).extend_from_slice(std::slice::from_raw_parts(units, count)) };
    }
}

fn append(o: Out, buffer: isize, bytes: &[u8]) {
    o(Op::AppendBytes, buffer, bytes.as_ptr() as isize, bytes.len() as isize);
}

fn write_summary(o: Out, out: isize) {
    let on = |op: Op| o(op, 0, 0, 0);
    let mut s = EeWonGameSummary { game_type: on(Op::GameType) as i32, ..EeWonGameSummary::default() };
    if s.game_type == 2 {
        o(Op::GameAddressSixByte, s.address.as_mut_ptr() as isize, 0, 0);
    } else {
        s.product_id = on(Op::ProductId) as u16;
    }
    s.in_progress = on(Op::InProgress) != 0;
    s.has_password = on(Op::HasPassword) != 0;
    s.invite_only = on(Op::InviteOnly) != 0;
    s.ask_to_join = on(Op::AskToJoin) != 0;
    s.skill = on(Op::Skill) as i32;
    s.players = on(Op::NumPlayers) as i32;
    s.open_slots = on(Op::MaxPlayers) as i32;
    let mut name: Vec<u8> = Vec::new();
    if s.game_type != 2 {
        o(Op::NameUtf16, &mut name as *mut Vec<u8> as isize, 0, 0);
    }
    let mut bytes: Vec<u8> = Vec::new();
    // SAFETY: the summary and the name live across the call; the sink appends to `bytes`
    unsafe {
        crate::staging::ee_won_game_write_summary(
            &s,
            name.as_ptr(),
            name.len(),
            bytes_to_vec,
            &mut bytes as *mut Vec<u8> as *mut c_void,
        )
    };
    append(o, out, &bytes);
    o(Op::WriteSummaryHook, out, 0, 0);
}

/// ReadSummary: a short summary keeps the fields read before the cut (the model stores what was read).
fn read_summary(o: Out, buffer: isize) -> isize {
    let start = o(Op::BufPos, buffer, 0, 0) as i32;
    let data = o(Op::BufData, buffer, 0, 0) as *const u8;
    let length = o(Op::BufLength, buffer, 0, 0) as i32;
    let mut s = EeWonGameSummary::default();
    let mut name: Vec<u16> = Vec::new();
    let game_type = o(Op::GameType, 0, 0, 0) as i32;
    let product_id = o(Op::ProductId, 0, 0, 0) as u16;
    // SAFETY: the buffer's bytes from `start` on; the sink appends to `name`
    unsafe {
        crate::staging::ee_won_game_read_summary(
            data.wrapping_add(start as usize),
            (length - start) as usize,
            game_type,
            product_id,
            &mut s,
            units_to_vec,
            &mut name as *mut Vec<u16> as *mut c_void,
        )
    };
    // the name's text when read, else an empty GUIString's (a default `GUIString` holds the empty data)
    let named = s.read & crate::staging::SUMMARY_NAME != 0;
    let text = if named {
        o(Op::TextOfUnits, name.as_ptr() as isize, name.len() as isize, 0)
    } else {
        o(Op::TextEmpty, 0, 0, 0)
    };
    o(Op::StoreSummary, &s as *const EeWonGameSummary as isize, text, 0);
    let r = if !s.ok {
        0
    } else {
        o(Op::BufSetPos, buffer, start as isize + s.rest as isize, 0);
        match o(Op::ReadSummaryHook, buffer, 0, 0) {
            THREW => 0,
            v => (v != 0) as isize,
        }
    };
    if named {
        o(Op::Release, text, 0, 0);
    }
    r
}

fn read_details(o: Out, buffer: isize) -> isize {
    match o(Op::ReadDetailsHook, buffer, 0, 0) {
        THREW | 0 => 0,
        _ => {
            o(Op::SetHaveDetails, 0, 0, 0);
            1
        }
    }
}

/// A held `ByteBuffer` of the write buffer's bytes; the buffer freed.
fn to_byte_buffer(o: Out, buffer: isize) -> isize {
    let r = o(Op::ToByteBuffer, buffer, 0, 0);
    o(Op::WriteFree, buffer, 0, 0);
    r
}

fn join_request(o: Out) -> isize {
    let buffer = o(Op::WriteNew, 0, 0, 0);
    let ping = o(Op::Ping, 0, 0, 0) as u16;
    append(o, buffer, &crate::staging::join_request_head(ping));
    o(Op::JoinRequestHook, buffer, 0, 0);
    to_byte_buffer(o, buffer)
}

/// HandleJoinRequest: the status; on success the player count and every player in `out`. The locals (the new
/// player, the game's client lists) are released in the original's reverse order at every return.
fn handle_join_request(o: Out, input: isize, client: isize, out: isize) -> isize {
    let check = crate::staging::host_join_check(o(Op::IsFull, 0, 0, 0) != 0, o(Op::InProgress, 0, 0, 0) != 0);
    if check != 0 {
        return check as isize;
    }
    let config = o(Op::ConfigGet, 0, 0, 0);
    let clients = o(Op::ClientsGet, 0, 0, 0);
    let release = |r: isize, player: isize| {
        o(Op::Release, clients, 0, 0);
        if player != 0 {
            o(Op::Release, player, 0, 0);
        }
        r
    };
    if config != 0 && o(Op::ConfigAllowDuplicates, config, 0, 0) == 0 {
        let n = o(Op::ListCount, clients, 0, 0) as usize;
        for i in 0..n {
            let member = o(Op::ListAt, clients, i as isize, 0);
            if o(Op::ClientSameName, member, client, 0) != 0 && member != client {
                return release(8, 0);
            }
        }
    }
    let player = o(Op::PlayerCreate, 0, 0, 0);
    o(Op::ClientSetPlayer, client, player, 0);
    // the try
    let id = o(Op::ReadShort, input, 0, 0);
    let result = if id == THREW {
        THREW
    } else {
        o(Op::PlayerSetPing, player, id as i16 as u16 as isize, 0);
        o(Op::JoinRequestHookRead, input, client, 0)
    };
    if result == THREW {
        o(Op::ClientSetPlayer, client, 0, 0);
        return release(4, player);
    }
    if result as i16 != 0 {
        o(Op::ClientSetPlayer, client, 0, 0);
        return release(result as i16 as isize, player);
    }
    let players = o(Op::NumPlayers, 0, 0, 0) as i32 + 1;
    o(Op::SetNumPlayers, players as isize, 0, 0);
    o(Op::AppendShort, out, players as isize, 0);
    let members = o(Op::ClientsGet, 0, 0, 0);
    let it = o(Op::IterNew, members, 0, 0);
    while o(Op::IterHasMore, it, 0, 0) != 0 {
        let mut other: isize = 0;
        let other_player = o(Op::IterNext, it, &mut other as *mut isize as isize, 0);
        if other_player != 0 {
            o(Op::AppendShort, out, o(Op::ClientId, other, 0, 0), 0);
            o(Op::PlayerWrite, other_player, out, 0);
        }
    }
    o(Op::JoinReplyHook, client, out, 0);
    o(Op::IterFree, it, 0, 0);
    o(Op::Release, members, 0, 0);
    release(0, player)
}

fn join_reply(o: Out, input: isize, client: isize) -> isize {
    let buffer = o(Op::WriteNew, 0, 0, 0);
    o(Op::AppendByte, buffer, 2, 0);
    o(Op::AppendShort, buffer, 0, 0);
    let status = o(Op::HandleJoinRequest, input, client, buffer);
    o(Op::SetShort, buffer, 1, status);
    to_byte_buffer(o, buffer)
}

fn handle_join_reply(o: Out, input: isize) -> isize {
    let result = o(Op::ReadShort, input, 0, 0);
    if result == THREW {
        return 4;
    }
    if result as i16 != 0 {
        return result as i16 as isize;
    }
    let count = match o(Op::ReadShort, input, 0, 0) {
        THREW => return 4,
        v => v as i16 as u16,
    };
    for _ in 0..count {
        let id = o(Op::ReadShort, input, 0, 0);
        if id == THREW {
            return 4;
        }
        let client = o(Op::FindClient, id as i16 as u16 as isize, 0, 0);
        if client != 0 {
            let player = o(Op::PlayerCreate, 0, 0, 0);
            if o(Op::PlayerReadFrom, player, input, 0) != 0 {
                o(Op::ClientSetPlayer, client, player, 0);
            }
            o(Op::Release, player, 0, 0);
        }
    }
    if o(Op::JoinReplyHookRead, input, 0, 0) == THREW {
        return 4;
    }
    result as i16 as isize
}

const STATUS_TEXTS: [Op; 8] = [
    Op::StatusSuccess,
    Op::StatusGameFull,
    Op::StatusGameInProgress,
    Op::StatusUnpackFailure,
    Op::StatusNotInvited,
    Op::StatusCaptainRejectedYou,
    Op::StatusInvalidPassword,
    Op::StatusDuplicateName,
];

fn status_string(o: Out, status: i16) -> isize {
    if let Some(k) = crate::model::status_text(status) {
        if let Some(&op) = STATUS_TEXTS.get(k as usize) {
            return o(op, 0, 0, 0);
        }
    }
    let custom = o(Op::StatusHook, status as isize, 0, 0);
    if o(Op::TextLength, custom, 0, 0) != 0 {
        return custom;
    }
    o(Op::Release, custom, 0, 0);
    // sprintf("%d")
    let digits = format!("{}\0", status as i32);
    o(Op::TextOfBytes, digits.as_ptr() as isize, 0, 0)
}

fn player_read_from(o: Out, buffer: isize) -> isize {
    let ping = o(Op::ReadShort, buffer, 0, 0);
    if ping == THREW {
        return 0;
    }
    o(Op::SelfSetPing, ping as i16 as u16 as isize, 0, 0);
    let ready = o(Op::ReadBool, buffer, 0, 0);
    if ready == THREW {
        return 0;
    }
    o(Op::SelfSetReady, ready, 0, 0);
    match o(Op::PlayerReadData, buffer, 0, 0) {
        THREW => 0,
        v => (v != 0) as isize,
    }
}

fn player_write_to(o: Out, buffer: isize) {
    let ping = o(Op::PlayerPing, 0, 0, 0) as u16;
    let ready = o(Op::PlayerReady, 0, 0, 0) != 0;
    append(o, buffer, &crate::staging::player_head(ping, ready));
    o(Op::PlayerWriteData, buffer, 0, 0);
}

fn call(which: EeWonModelBufCall, a: isize, b: isize, c: isize, o: Out) -> isize {
    type C = EeWonModelBufCall;
    match which {
        C::WriteSummary => {
            write_summary(o, a);
            0
        }
        C::ReadSummary => read_summary(o, a),
        C::ReadDetails => read_details(o, a),
        C::JoinRequest => join_request(o),
        C::HandleJoinRequest => handle_join_request(o, a, b, c),
        C::JoinReply => join_reply(o, a, b),
        C::HandleJoinReply => handle_join_reply(o, a),
        C::StatusString => status_string(o, a as i16),
        C::PlayerReadFrom => player_read_from(o, a),
        C::PlayerWriteTo => {
            player_write_to(o, a);
            0
        }
    }
}

/// One of the model's buffer methods on `obj` over the host `host`.
///
/// # Safety
/// `host` answers the ops for `obj` (LobbyModelHost.cpp's BufHost).
#[no_mangle]
pub unsafe extern "C" fn ee_won_modelbuf_call(
    obj: *mut c_void,
    host: EeWonModelBufHostFn,
    which: EeWonModelBufCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    let o = |op: Op, a: isize, b: isize, c: isize| match (crate::lobbytext::buf_op(op, a, b), host) {
        (Some(r), _) => r,
        // SAFETY: the contract
        (None, Some(f)) => unsafe { f(obj, op, a, b, c) },
        (None, None) => 0,
    };
    call(which, a, b, c, &o)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    fn run(c: EeWonModelBufCall, a: [isize; 3], answer: &dyn Fn(Op, isize) -> isize) -> (isize, Vec<String>) {
        let log = RefCell::new(Vec::new());
        let o = |op: Op, a: isize, b: isize, c: isize| {
            log.borrow_mut().push(format!("{op:?} {a} {b} {c}"));
            answer(op, a)
        };
        let r = call(c, a[0], a[1], a[2], &o);
        (r, log.into_inner())
    }

    #[test]
    fn a_refused_join_frees_the_player_and_the_lists() {
        // a read past the end: status 4, the player taken back, the clients then the player released
        let (r, log) = run(EeWonModelBufCall::HandleJoinRequest, [10, 20, 30], &|op, _| match op {
            Op::ClientsGet => 0x40,
            Op::PlayerCreate => 0x50,
            Op::ReadShort => THREW,
            _ => 0,
        });
        assert_eq!(r, 4);
        assert_eq!(log[log.len() - 3..], ["ClientSetPlayer 20 0 0", "Release 64 0 0", "Release 80 0 0"]);
        // a duplicate name
        let (r, log) = run(EeWonModelBufCall::HandleJoinRequest, [10, 20, 30], &|op, _| match op {
            Op::ConfigGet => 7,
            Op::ClientsGet => 0x40,
            Op::ListCount => 2,
            Op::ListAt => 0x99,
            Op::ClientSameName => 1,
            _ => 0,
        });
        assert_eq!((r, log.last().unwrap().as_str()), (8, "Release 64 0 0"));
    }

    #[test]
    fn the_reply_and_the_status_texts() {
        let (r, log) = run(EeWonModelBufCall::HandleJoinReply, [10, 0, 0], &|op, _| match op {
            Op::ReadShort => 1,
            _ => 0,
        });
        assert_eq!((r, log.len()), (1, 1));
        let (r, _) = run(EeWonModelBufCall::StatusString, [0, 0, 0], &|op, _| (op == Op::StatusSuccess) as isize * 9);
        assert_eq!(r, 9);
        let (r, log) = run(EeWonModelBufCall::StatusString, [-3, 0, 0], &|op, _| (op == Op::TextOfBytes) as isize * 5);
        assert_eq!((r, log[log.len() - 2].as_str()), (5, "Release 0 0 0"));
    }
}
