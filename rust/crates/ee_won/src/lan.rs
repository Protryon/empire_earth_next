//! The WON lobby's LAN protocol (LanLogic.cpp): the game channel's messages between the host (the captain) and the
//! players over TCP, the discovery packets on the broadcast UDP port, and the host's decisions about them.
//!
//! Game messages are `WriteBuffer(2, true)` messages: a 2-byte length prefix that counts itself, a type byte, then
//! the fields (shorts little-endian, wide strings as a 2-byte unit count and UTF-16 LE units). A message the host
//! delivers to itself still carries its prefix, which `HandleMessage` skips. Discovery packets have no prefix
//! (`WriteBuffer(0, true)`): a type byte and, but for the query, the hosted game's TCP port; a summary continues with
//! `LobbyGame::WriteSummary`'s fields, which stay with the lobby's game object.
//!
//! | type | game channel                                   | discovery            |
//! |------|------------------------------------------------|----------------------|
//! | 1    | join request: name, password                   | summary: port, ...   |
//! | 2    | join reply: status [captain, you, n, (id, name)*] | keep-alive: port  |
//! | 3    | client joined: id, name                        | removed: port        |
//! | 4    | client left: id                                | query                |
//! | 5    | chat request: flags, text, target              |                      |
//! | 6    | chat line: flags, sender, text, target         |                      |
//! | 7    | ping (joiner to host)                          |                      |
//! | 8    | staging: the staging message's bytes           |                      |

use crate::wire::{PastEnd, Reader, Writer};

pub(crate) const JOIN_REQUEST: u8 = 1;
pub(crate) const JOIN_REPLY: u8 = 2;
pub(crate) const CLIENT_JOINED: u8 = 3;
pub(crate) const CLIENT_LEFT: u8 = 4;
pub(crate) const CHAT_REQUEST: u8 = 5;
pub(crate) const CHAT_LINE: u8 = 6;
pub(crate) const PING: u8 = 7;
pub(crate) const STAGING: u8 = 8;

pub(crate) const DISCOVERY_SUMMARY: u8 = 1;
pub(crate) const DISCOVERY_KEEP_ALIVE: u8 = 2;
pub(crate) const DISCOVERY_REMOVED: u8 = 3;
pub(crate) const DISCOVERY_QUERY: u8 = 4;

/// Join reply statuses: joined, bad data (the joiner's own), wrong password, another subnet.
pub(crate) const JOIN_OK: i16 = 0;
pub(crate) const JOIN_BAD_REPLY: i32 = 1;
pub(crate) const JOIN_WRONG_PASSWORD: i16 = 2;
pub(crate) const JOIN_OTHER_SUBNET: i16 = 3;

/// Chat flags: a whisper to the target, a line to the sender's team.
pub(crate) const CHAT_WHISPER: u16 = 4;
pub(crate) const CHAT_TEAM: u16 = 0x100;

/// The host drops a socket silent for longer than this (seconds); a joiner pings after this many idle seconds; the
/// host re-sends its full summary instead of a keep-alive after this many.
pub(crate) const HOST_SOCKET_SILENCE: i64 = 300;
pub(crate) const JOINER_PING_IDLE: i64 = 120;
pub(crate) const SUMMARY_REPEAT: i64 = 30;

fn game_message(kind: u8) -> Writer {
    let mut w = Writer::new(2, true);
    w.append_byte(kind);
    w
}

/// Joiner: the join request (type 1).
pub fn join_request(name: &[u16], password: &[u16]) -> Vec<u8> {
    let mut w = game_message(JOIN_REQUEST);
    w.append_wstring(name, 2);
    w.append_wstring(password, 2);
    w.to_message()
}

/// Host: a refused join (type 2 with a status only).
pub fn join_refused(status: i16) -> Vec<u8> {
    let mut w = game_message(JOIN_REPLY);
    w.append_short(status);
    w.to_message()
}

/// Host: the accepted join: the captain's and the new player's ids, then every player (the new one included).
pub fn join_accepted(captain: u16, you: u16, clients: &[(u16, Vec<u16>)]) -> Vec<u8> {
    let mut w = game_message(JOIN_REPLY);
    w.append_short(JOIN_OK);
    w.append_short(captain as i16);
    w.append_short(you as i16);
    w.append_short(clients.len() as i16);
    for (id, name) in clients {
        w.append_short(*id as i16);
        w.append_wstring(name, 2);
    }
    w.to_message()
}

pub fn client_joined(id: u16, name: &[u16]) -> Vec<u8> {
    let mut w = game_message(CLIENT_JOINED);
    w.append_short(id as i16);
    w.append_wstring(name, 2);
    w.to_message()
}

pub fn client_left(id: u16) -> Vec<u8> {
    let mut w = game_message(CLIENT_LEFT);
    w.append_short(id as i16);
    w.to_message()
}

pub fn chat_request(flags: u16, text: &[u16], target: u16) -> Vec<u8> {
    let mut w = game_message(CHAT_REQUEST);
    w.append_short(flags as i16);
    w.append_wstring(text, 2);
    w.append_short(target as i16);
    w.to_message()
}

pub fn chat_line(flags: u16, sender: u16, text: &[u16], target: u16) -> Vec<u8> {
    let mut w = game_message(CHAT_LINE);
    w.append_short(flags as i16);
    w.append_short(sender as i16);
    w.append_wstring(text, 2);
    w.append_short(target as i16);
    w.to_message()
}

pub fn ping() -> Vec<u8> {
    game_message(PING).to_message()
}

/// A staging message wrapped for the game channel (`AppendBuffer(buffer, 0)`: no length of its own).
pub fn staging(payload: &[u8]) -> Vec<u8> {
    let mut w = game_message(STAGING);
    w.append_bytes(payload);
    w.to_message()
}

/// A discovery packet's head: the type and, but for the query, the game's port (a summary goes on in C++).
pub fn discovery(kind: u8, port: u16) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    w.append_byte(kind);
    if kind != DISCOVERY_QUERY {
        w.append_short(port as i16);
    }
    w.to_message()
}

/// A game-channel message as `HandleMessage` / `HandleNewSocketMessage` read it. Strings are UTF-16 units.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum GameMsg {
    JoinRequest {
        name: Vec<u16>,
        password: Vec<u16>,
    },
    ClientJoined {
        id: u16,
        name: Vec<u16>,
    },
    ClientLeft {
        id: u16,
    },
    ChatRequest {
        flags: u16,
        text: Vec<u16>,
        target: u16,
    },
    ChatLine {
        flags: u16,
        sender: u16,
        text: Vec<u16>,
        target: u16,
    },
    /// The staging message starts after the type byte.
    Staging,
    /// Any other type (a join reply, a ping): ignored by the dispatch.
    Other(u8),
}

/// Reads a game-channel message (`skip`: the prefix bytes to pass over, 2 for the host's own messages, else 0, as
/// `HandleMessage`). `Err` where the original's read threw (the handler then did nothing).
pub fn read_game(bytes: &[u8], skip: usize) -> Result<GameMsg, PastEnd> {
    let mut r = Reader::new(bytes.get(skip..).ok_or(PastEnd)?);
    Ok(match r.read_byte()? {
        JOIN_REQUEST => {
            let name = r.read_wstring(2)?;
            let password = r.read_wstring(2)?;
            GameMsg::JoinRequest { name, password }
        }
        CLIENT_JOINED => {
            let id = r.read_short()? as u16;
            GameMsg::ClientJoined { id, name: r.read_wstring(2)? }
        }
        CLIENT_LEFT => GameMsg::ClientLeft { id: r.read_short()? as u16 },
        CHAT_REQUEST => {
            let flags = r.read_short()? as u16;
            let text = r.read_wstring(2)?;
            GameMsg::ChatRequest { flags, text, target: r.read_short()? as u16 }
        }
        CHAT_LINE => {
            let flags = r.read_short()? as u16;
            let sender = r.read_short()? as u16;
            let text = r.read_wstring(2)?;
            GameMsg::ChatLine { flags, sender, text, target: r.read_short()? as u16 }
        }
        STAGING => GameMsg::Staging,
        other => GameMsg::Other(other),
    })
}

/// A join reply as `ReadJoinReply` reads it: the status it returns (`JOIN_BAD_REPLY` for a wrong type or a short
/// message), and what it had already applied when a read failed: the captain's id once read (the game's
/// `mCaptainId`), then each player whose id and name were read whole, in order.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct JoinReply {
    pub status: i32,
    pub captain: Option<u16>,
    pub you: u16,
    pub clients: Vec<(u16, Vec<u16>)>,
}

pub fn read_join_reply(bytes: &[u8]) -> JoinReply {
    let mut out = JoinReply { status: JOIN_BAD_REPLY, ..JoinReply::default() };
    let mut r = Reader::new(bytes);
    let mut body = || -> Result<i32, PastEnd> {
        if r.read_byte()? != JOIN_REPLY {
            return Ok(JOIN_BAD_REPLY);
        }
        let status = r.read_short()?;
        if status != JOIN_OK {
            return Ok(i32::from(status));
        }
        let captain = r.read_short()? as u16;
        out.you = r.read_short()? as u16;
        out.captain = Some(captain);
        let count = r.read_short()? as u16;
        for _ in 0..count {
            let id = r.read_short()? as u16;
            let name = r.read_wstring(2)?;
            out.clients.push((id, name));
        }
        Ok(i32::from(JOIN_OK))
    };
    out.status = body().unwrap_or(JOIN_BAD_REPLY);
    out
}

/// A discovery packet's head: its type and the port (0 for the query and unknown types); `Err` when short.
pub fn read_discovery(bytes: &[u8]) -> Result<(u8, u16), PastEnd> {
    let mut r = Reader::new(bytes);
    let kind = r.read_byte()?;
    let port = match kind {
        DISCOVERY_SUMMARY | DISCOVERY_KEEP_ALIVE | DISCOVERY_REMOVED => r.read_short()? as u16,
        _ => 0,
    };
    Ok((kind, port))
}

/// Host: the status for a join request. A LAN game (type 1) refuses a peer whose address's first two bytes differ
/// from the host's (`GetSixByte()[2..4]`); then the password must match.
pub fn join_status(game_type: i32, peer_net: [u8; 2], local_net: [u8; 2], password_matches: bool) -> i16 {
    if game_type == 1 && peer_net != local_net {
        JOIN_OTHER_SUBNET
    } else if !password_matches {
        JOIN_WRONG_PASSWORD
    } else {
        JOIN_OK
    }
}

/// Host: the next player id (`GetNextClientId`): 0 and 0xffff are stepped over before the id is taken, so after
/// 0xffff the counter wraps and hands out 0 once, as the original did.
pub fn next_client_id(next: &mut u16) -> u16 {
    if *next == 0xffff || *next == 0 {
        *next = next.wrapping_add(1);
    }
    let id = *next;
    *next = next.wrapping_add(1);
    id
}

/// Host: where a chat request goes: to the whisper target and the sender, to the sender's team, or to everyone.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ChatRoute {
    Everyone = 0,
    Whisper = 1,
    Team = 2,
}

pub fn chat_route(flags: u16) -> ChatRoute {
    if flags & CHAT_WHISPER != 0 {
        ChatRoute::Whisper
    } else if flags & CHAT_TEAM != 0 {
        ChatRoute::Team
    } else {
        ChatRoute::Everyone
    }
}

/// The flags a received chat line is shown with: a whisper's are 4 alone.
pub fn chat_line_flags(flags: u16) -> u16 {
    if flags & CHAT_WHISPER != 0 {
        CHAT_WHISPER
    } else {
        flags
    }
}

/// Host, each network timer: whether a socket silent since `last_message` is dropped.
pub fn host_socket_stale(now: i64, last_message: i64) -> bool {
    now.wrapping_sub(last_message) > HOST_SOCKET_SILENCE
}

/// Joiner, each network timer: whether to ping the host.
pub fn joiner_ping_due(now: i64, last_ping: i64) -> bool {
    now.wrapping_sub(last_ping) >= JOINER_PING_IDLE
}

/// Host, each discovery timer: whether the full summary goes out instead of a keep-alive.
pub fn summary_due(now: i64, last_broadcast: i64) -> bool {
    last_broadcast > 0 && now.wrapping_sub(last_broadcast) >= SUMMARY_REPEAT
}

#[cfg(test)]
mod tests {
    use super::*;

    fn u(s: &str) -> Vec<u16> {
        s.encode_utf16().collect()
    }

    #[test]
    fn join_request_bytes() {
        assert_eq!(
            join_request(&u("ab"), &u("")),
            vec![11, 0, 1, 2, 0, b'a', 0, b'b', 0, 0, 0],
            "prefix 11 counts itself; type 1; name 2 units; empty password"
        );
        assert_eq!(
            read_game(&join_request(&u("ab"), &u("pw")), 2).unwrap(),
            GameMsg::JoinRequest { name: u("ab"), password: u("pw") }
        );
    }

    #[test]
    fn join_reply_round_trip_and_truncation() {
        let clients = vec![(1, u("host")), (2, u("me"))];
        let msg = join_accepted(1, 2, &clients);
        assert_eq!(&msg[..11], &[msg.len() as u8, 0, 2, 0, 0, 1, 0, 2, 0, 2, 0]);
        // the joiner reads the reply without its prefix (RecvMsgOp strips it)
        let reply = read_join_reply(&msg[2..]);
        assert_eq!(reply, JoinReply { status: 0, captain: Some(1), you: 2, clients: clients.clone() });
        // cut inside the second name: the captain and the first player were applied, the status is bad data
        let cut = read_join_reply(&msg[2..msg.len() - 1]);
        assert_eq!(cut, JoinReply { status: 1, captain: Some(1), you: 2, clients: vec![clients[0].clone()] });
        assert_eq!(read_join_reply(&join_refused(2)[2..]).status, 2);
        assert_eq!(read_join_reply(&join_refused(-5)[2..]).status, -5, "a short status is sign-extended");
        assert_eq!(read_join_reply(&[3]).status, 1);
        assert_eq!(read_join_reply(&[]).status, 1);
    }

    #[test]
    fn game_messages() {
        let m = chat_line(0x104, 3, &u("hi"), 9);
        assert_eq!(read_game(&m, 2).unwrap(), GameMsg::ChatLine { flags: 0x104, sender: 3, text: u("hi"), target: 9 });
        let m = chat_request(0, &u("x"), 0);
        assert_eq!(read_game(&m, 2).unwrap(), GameMsg::ChatRequest { flags: 0, text: u("x"), target: 0 });
        assert_eq!(read_game(&client_joined(5, &u("n")), 2).unwrap(), GameMsg::ClientJoined { id: 5, name: u("n") });
        assert_eq!(read_game(&client_left(5), 2).unwrap(), GameMsg::ClientLeft { id: 5 });
        assert_eq!(staging(&[9, 9]), vec![5, 0, 8, 9, 9]);
        assert_eq!(read_game(&staging(&[9]), 2).unwrap(), GameMsg::Staging);
        assert_eq!(ping(), vec![3, 0, 7]);
        assert_eq!(read_game(&ping(), 2).unwrap(), GameMsg::Other(7));
        assert!(read_game(&client_left(5)[..4], 2).is_err());
        assert!(read_game(&[1], 2).is_err());
    }

    #[test]
    fn discovery_packets() {
        assert_eq!(discovery(DISCOVERY_QUERY, 8888), vec![4]);
        assert_eq!(discovery(DISCOVERY_SUMMARY, 0x22b8), vec![1, 0xb8, 0x22]);
        assert_eq!(read_discovery(&[2, 0xb8, 0x22]).unwrap(), (2, 0x22b8));
        assert_eq!(read_discovery(&[4]).unwrap(), (4, 0));
        assert_eq!(read_discovery(&[9]).unwrap(), (9, 0));
        assert!(read_discovery(&[3, 1]).is_err());
    }

    #[test]
    fn client_ids_step_over_0_and_0xffff() {
        let mut next = 0;
        assert_eq!((next_client_id(&mut next), next), (1, 2));
        let mut next = 0xfffe;
        assert_eq!(next_client_id(&mut next), 0xfffe);
        assert_eq!((next_client_id(&mut next), next), (0, 1), "0xffff steps to 0, which is handed out");
        assert_eq!(next_client_id(&mut next), 1);
    }

    #[test]
    fn decisions() {
        assert_eq!(join_status(1, [10, 0], [10, 1], true), JOIN_OTHER_SUBNET);
        assert_eq!(join_status(3, [10, 0], [10, 1], true), JOIN_OK, "only LAN games check the subnet");
        assert_eq!(join_status(1, [10, 0], [10, 0], false), JOIN_WRONG_PASSWORD);
        assert_eq!(chat_route(0x104), ChatRoute::Whisper);
        assert_eq!(chat_route(0x100), ChatRoute::Team);
        assert_eq!(chat_route(1), ChatRoute::Everyone);
        assert_eq!(chat_line_flags(0x105), 4);
        assert_eq!(chat_line_flags(0x101), 0x101);
        assert!(!host_socket_stale(1300, 1000) && host_socket_stale(1301, 1000));
        assert!(!joiner_ping_due(1119, 1000) && joiner_ping_due(1120, 1000));
        assert!(!summary_due(1030, 0), "never before the first broadcast");
        assert!(!summary_due(1029, 1000) && summary_due(1030, 1000));
    }
}
