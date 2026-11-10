//! The C ABI of `lan` for the C++ `LanLogic`: the messages are built into a byte sink (the C++ copies them into a
//! `ByteBuffer`) and read into small C structs; strings cross as UTF-16 LE bytes in and UTF-16 units out (the C++
//! has `WideAppendUtf16LE` / `WideAppendUtf16Unit`).

use std::ffi::c_void;

use crate::ffi::{EeWonByteSink, EeWonUnitSink};
use crate::lan::{self, GameMsg};
use crate::text;

unsafe fn le_units(bytes: *const u8, length: usize) -> Vec<u16> {
    if bytes.is_null() || length == 0 {
        return Vec::new();
    }
    // SAFETY: `length` bytes (each function's contract)
    text::utf16_from_le_bytes(unsafe { std::slice::from_raw_parts(bytes, length) })
}

unsafe fn input<'a>(bytes: *const u8, length: usize) -> &'a [u8] {
    if bytes.is_null() || length == 0 {
        &[]
    } else {
        // SAFETY: as `le_units`
        unsafe { std::slice::from_raw_parts(bytes, length) }
    }
}

unsafe fn give(sink: EeWonByteSink, context: *mut c_void, data: &[u8]) {
    unsafe { sink(context, data.as_ptr(), data.len()) }
}

/// The joiner's join request; `name` / `password` are UTF-16 LE bytes.
///
/// # Safety
/// The byte arguments hold their lengths; `sink` takes the message.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_join_request(
    name: *const u8,
    name_len: usize,
    password: *const u8,
    password_len: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let m = lan::join_request(&unsafe { le_units(name, name_len) }, &unsafe { le_units(password, password_len) });
    unsafe { give(sink, context, &m) }
}

/// The host's refusal of a join (`status` 2 wrong password, 3 another subnet).
///
/// # Safety
/// `sink` takes the message.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_join_refused(status: i16, sink: EeWonByteSink, context: *mut c_void) {
    unsafe { give(sink, context, &lan::join_refused(status)) }
}

/// The host's accepted join reply: `ids[i]` and the UTF-16 LE name `names` from `name_offsets[i]` (in bytes) to
/// `name_offsets[i + 1]` (`count + 1` offsets).
///
/// # Safety
/// `ids` holds `count` ids, `name_offsets` `count + 1` offsets into `names`; `sink` takes the message.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_join_accepted(
    captain: u16,
    you: u16,
    count: usize,
    ids: *const u16,
    names: *const u8,
    name_offsets: *const usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let mut clients = Vec::with_capacity(count);
    for i in 0..count {
        // SAFETY: the arrays' lengths (the contract)
        let (id, from, to) = unsafe { (*ids.add(i), *name_offsets.add(i), *name_offsets.add(i + 1)) };
        clients.push((id, unsafe { le_units(names.add(from), to - from) }));
    }
    unsafe { give(sink, context, &lan::join_accepted(captain, you, &clients)) }
}

/// The host's announcement of a new player.
///
/// # Safety
/// `name` holds `name_len` UTF-16 LE bytes; `sink` takes the message.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_client_joined(
    id: u16,
    name: *const u8,
    name_len: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    unsafe { give(sink, context, &lan::client_joined(id, &le_units(name, name_len))) }
}

/// The host's announcement of a departed player.
///
/// # Safety
/// `sink` takes the message.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_client_left(id: u16, sink: EeWonByteSink, context: *mut c_void) {
    unsafe { give(sink, context, &lan::client_left(id)) }
}

/// A player's chat request to the host.
///
/// # Safety
/// `text` holds `text_len` UTF-16 LE bytes; `sink` takes the message.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_chat_request(
    flags: u16,
    text: *const u8,
    text_len: usize,
    target: u16,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    unsafe { give(sink, context, &lan::chat_request(flags, &le_units(text, text_len), target)) }
}

/// The host's relayed chat line.
///
/// # Safety
/// As `ee_won_lan_chat_request`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_chat_line(
    flags: u16,
    sender: u16,
    text: *const u8,
    text_len: usize,
    target: u16,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    unsafe { give(sink, context, &lan::chat_line(flags, sender, &le_units(text, text_len), target)) }
}

/// The joiner's ping.
///
/// # Safety
/// `sink` takes the message.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_ping(sink: EeWonByteSink, context: *mut c_void) {
    unsafe { give(sink, context, &lan::ping()) }
}

/// A staging message wrapped for the game channel.
///
/// # Safety
/// `payload` holds `length` bytes; `sink` takes the message.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_staging(
    payload: *const u8,
    length: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    unsafe { give(sink, context, &lan::staging(input(payload, length))) }
}

/// A discovery packet's head (`kind` 1 summary, 2 keep-alive, 3 removed, 4 query).
///
/// # Safety
/// `sink` takes the packet.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_discovery(kind: u8, port: u16, sink: EeWonByteSink, context: *mut c_void) {
    unsafe { give(sink, context, &lan::discovery(kind, port)) }
}

/// A game-channel message read: `kind` is the type byte (0 when the read failed: nothing to do), the ids and
/// flags per type; the text (a name, a password's companion or a chat line) goes to the sinks.
#[repr(C)]
pub struct EeWonLanMsg {
    pub kind: u8,
    pub flags: u16,
    pub id: u16,
    pub target: u16,
}

/// Reads a game-channel message (`skip` 2 for the host's own, else 0). The join request's name goes to
/// `first`, its password to `second`; a joined player's name and a chat text go to `first`.
///
/// # Safety
/// `bytes` holds `length` bytes; `out` is valid; the sinks take the units.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_read_game(
    bytes: *const u8,
    length: usize,
    skip: usize,
    out: *mut EeWonLanMsg,
    first: EeWonUnitSink,
    second: EeWonUnitSink,
    context: *mut c_void,
) {
    let mut m = EeWonLanMsg { kind: 0, flags: 0, id: 0, target: 0 };
    let units = |sink: EeWonUnitSink, u: &[u16]| unsafe { sink(context, u.as_ptr(), u.len()) };
    if let Ok(msg) = lan::read_game(unsafe { input(bytes, length) }, skip) {
        match msg {
            GameMsg::JoinRequest { name, password } => {
                m.kind = lan::JOIN_REQUEST;
                units(first, &name);
                units(second, &password);
            }
            GameMsg::ClientJoined { id, name } => {
                m.kind = lan::CLIENT_JOINED;
                m.id = id;
                units(first, &name);
            }
            GameMsg::ClientLeft { id } => {
                m.kind = lan::CLIENT_LEFT;
                m.id = id;
            }
            GameMsg::ChatRequest { flags, text, target } => {
                m.kind = lan::CHAT_REQUEST;
                m.flags = flags;
                m.target = target;
                units(first, &text);
            }
            GameMsg::ChatLine { flags, sender, text, target } => {
                m.kind = lan::CHAT_LINE;
                m.flags = flags;
                m.id = sender;
                m.target = target;
                units(first, &text);
            }
            GameMsg::Staging => m.kind = lan::STAGING,
            GameMsg::Other(kind) => m.kind = kind,
        }
    }
    unsafe { *out = m };
}

/// Receives one player of a join reply.
pub type EeWonLanClientSink = unsafe extern "C" fn(context: *mut c_void, id: u16, name: *const u16, count: usize);

/// Reads a join reply: returns `ReadJoinReply`'s status; `*captain` / `*you` are set and `true` returned through
/// `have_captain` once the captain's id was read; each player read whole goes to `client`, in order.
///
/// # Safety
/// `bytes` holds `length` bytes; the out pointers are valid; `client` takes the players.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_read_join_reply(
    bytes: *const u8,
    length: usize,
    have_captain: *mut bool,
    captain: *mut u16,
    you: *mut u16,
    client: EeWonLanClientSink,
    context: *mut c_void,
) -> i32 {
    let r = lan::read_join_reply(unsafe { input(bytes, length) });
    unsafe {
        *have_captain = r.captain.is_some();
        *captain = r.captain.unwrap_or(0);
        *you = r.you;
        for (id, name) in &r.clients {
            client(context, *id, name.as_ptr(), name.len());
        }
    }
    r.status
}

/// Reads a discovery packet's head: true and its type and port, false when it is short.
///
/// # Safety
/// `bytes` holds `length` bytes; the out pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_read_discovery(
    bytes: *const u8,
    length: usize,
    kind: *mut u8,
    port: *mut u16,
) -> bool {
    match lan::read_discovery(unsafe { input(bytes, length) }) {
        Ok((k, p)) => {
            unsafe {
                *kind = k;
                *port = p;
            }
            true
        }
        Err(_) => false,
    }
}

/// `lan::join_status`: 0 accept, 2 wrong password, 3 another subnet.
#[no_mangle]
pub extern "C" fn ee_won_lan_join_status(
    game_type: i32,
    peer0: u8,
    peer1: u8,
    local0: u8,
    local1: u8,
    password_matches: bool,
) -> i16 {
    lan::join_status(game_type, [peer0, peer1], [local0, local1], password_matches)
}

/// `lan::next_client_id` over the C++ counter.
///
/// # Safety
/// `next` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_next_client_id(next: *mut u16) -> u16 {
    lan::next_client_id(unsafe { &mut *next })
}

/// `lan::chat_route`: 0 everyone, 1 the whisper target and the sender, 2 the sender's team.
#[no_mangle]
pub extern "C" fn ee_won_lan_chat_route(flags: u16) -> i32 {
    lan::chat_route(flags) as i32
}

/// `lan::chat_line_flags`.
#[no_mangle]
pub extern "C" fn ee_won_lan_chat_line_flags(flags: u16) -> u16 {
    lan::chat_line_flags(flags)
}

/// `lan::host_socket_stale`.
#[no_mangle]
pub extern "C" fn ee_won_lan_host_socket_stale(now: i64, last_message: i64) -> bool {
    lan::host_socket_stale(now, last_message)
}

/// `lan::joiner_ping_due`.
#[no_mangle]
pub extern "C" fn ee_won_lan_joiner_ping_due(now: i64, last_ping: i64) -> bool {
    lan::joiner_ping_due(now, last_ping)
}

/// `lan::summary_due`.
#[no_mangle]
pub extern "C" fn ee_won_lan_summary_due(now: i64, last_broadcast: i64) -> bool {
    lan::summary_due(now, last_broadcast)
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe extern "C" fn collect(context: *mut c_void, bytes: *const u8, length: usize) {
        unsafe { &mut *(context as *mut Vec<u8>) }
            .extend_from_slice(unsafe { std::slice::from_raw_parts(bytes, length) });
    }

    unsafe extern "C" fn collect_units(context: *mut c_void, units: *const u16, count: usize) {
        let v = unsafe { &mut *(context as *mut Vec<Vec<u16>>) };
        v.push(unsafe { std::slice::from_raw_parts(units, count) }.to_vec());
    }

    unsafe extern "C" fn collect_client(context: *mut c_void, id: u16, name: *const u16, count: usize) {
        let v = unsafe { &mut *(context as *mut Vec<(u16, Vec<u16>)>) };
        v.push((id, unsafe { std::slice::from_raw_parts(name, count) }.to_vec()));
    }

    #[test]
    fn join_accepted_through_the_abi() {
        let names = [b'h', 0, b'm', 0, b'e', 0];
        let offsets = [0usize, 2, 6];
        let ids = [1u16, 2];
        let mut msg = Vec::new();
        unsafe {
            ee_won_lan_join_accepted(
                1,
                2,
                2,
                ids.as_ptr(),
                names.as_ptr(),
                offsets.as_ptr(),
                collect,
                (&mut msg as *mut Vec<u8>).cast(),
            )
        };
        let mut clients = Vec::new();
        let (mut have, mut captain, mut you) = (false, 0, 0);
        let status = unsafe {
            ee_won_lan_read_join_reply(
                msg[2..].as_ptr(),
                msg.len() - 2,
                &mut have,
                &mut captain,
                &mut you,
                collect_client,
                (&mut clients as *mut Vec<(u16, Vec<u16>)>).cast(),
            )
        };
        assert_eq!((status, have, captain, you), (0, true, 1, 2));
        assert_eq!(clients, vec![(1u16, vec![0x68u16]), (2, vec![0x6d, 0x65])]);
    }

    #[test]
    fn read_game_through_the_abi() {
        let msg = lan::join_request(&[0x61], &[0x62, 0x63]);
        let mut texts: Vec<Vec<u16>> = Vec::new();
        let mut m = EeWonLanMsg { kind: 9, flags: 9, id: 9, target: 9 };
        let ctx = (&mut texts as *mut Vec<Vec<u16>>).cast();
        unsafe { ee_won_lan_read_game(msg.as_ptr(), msg.len(), 2, &mut m, collect_units, collect_units, ctx) };
        assert_eq!(m.kind, 1);
        assert_eq!(texts, vec![vec![0x61], vec![0x62, 0x63]]);
        unsafe { ee_won_lan_read_game(msg.as_ptr(), 3, 2, &mut m, collect_units, collect_units, ctx) };
        assert_eq!(m.kind, 0, "short: nothing to do");
        let (mut kind, mut port) = (0, 0);
        assert!(unsafe { ee_won_lan_read_discovery([2u8, 1, 0].as_ptr(), 3, &mut kind, &mut port) });
        assert_eq!((kind, port), (2, 1));
    }
}
