//! LanLogic's flow (LanLogic.cpp): what the LAN lobby does with its sockets, players and game lists, as decisions
//! over the state the C++ objects hold. The protocol's messages and the host's rules about them are `lan`; the
//! C++ keeps the sockets (`LobbyLanSocket`, the listen, host and discovery sockets), the timers, the clients and
//! game lists, the dialogs and events, and runs what these answer:
//!
//! - a received message: stamped (host), a join request on a socket with no player yet, or dispatched; where a
//!   message read starts (the host's own messages keep their length prefix);
//! - where a message goes: to the host itself, to a socket, or nowhere (`Route`), and a broadcast's sockets;
//! - a closed socket: the host announces the player's departure, a joiner reports the lost host;
//! - discovery: what a received summary does to the LAN game list and which event it reports; whether a hosted
//!   game is advertised;
//! - joining: what each join reply status does to the join loop, who each listed player is, whether peer
//!   authentication runs;
//! - the room's player actions, the typed chat commands, the refresh command, the LAN name and port dialogs, a
//!   direct-connect entry's port and name.

use crate::chat;

/// Where a message goes: nowhere, handled by this lobby itself (the host), or queued on a socket.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLanRoute {
    Drop = 0,
    Local = 1,
    Socket = 2,
}

/// A received message (`HandleRecvMsg`): whether the socket's time is stamped, and whether it is a new socket's
/// first message (only a join request counts) or goes to `HandleMessage`; nothing for a failed or killed op.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonLanRecv {
    pub handle: bool,
    pub stamp: bool,
    pub new_socket: bool,
}

pub fn recv(op_ok: bool, is_captain: bool, socket_has_client: bool) -> EeWonLanRecv {
    if !op_ok {
        return EeWonLanRecv::default();
    }
    EeWonLanRecv { handle: true, stamp: is_captain, new_socket: is_captain && !socket_has_client }
}

/// `HandleMessage`: the host's messages to itself still carry the two-byte length prefix.
pub fn message_offset(is_captain: bool, from_me: bool) -> usize {
    if is_captain && from_me {
        2
    } else {
        0
    }
}

/// `SendMessageToClient` (host only): to itself, to the player's socket, or nowhere (not hosting, no socket).
pub fn send_to_client(is_captain: bool, connected: bool, to_me: bool, has_socket: bool) -> EeWonLanRoute {
    if !is_captain || !connected {
        EeWonLanRoute::Drop
    } else if to_me {
        EeWonLanRoute::Local
    } else if has_socket {
        EeWonLanRoute::Socket
    } else {
        EeWonLanRoute::Drop
    }
}

/// `SendMessageToCaptain`: the host handles its own, a joiner sends on the host socket (and stamps its ping time).
pub fn send_to_captain(connected: bool, is_captain: bool, has_server_socket: bool) -> EeWonLanRoute {
    if !connected {
        EeWonLanRoute::Drop
    } else if is_captain {
        EeWonLanRoute::Local
    } else if has_server_socket {
        EeWonLanRoute::Socket
    } else {
        EeWonLanRoute::Drop
    }
}

/// `BroadcastMessage` (host only): whether it is handled locally too, and for each socket (whether it has a player)
/// whether it is sent there.
pub fn broadcast(is_captain: bool, deliver_local: bool, socket_has_client: &[bool], send: &mut [bool]) -> bool {
    for (s, &has) in send.iter_mut().zip(socket_has_client) {
        *s = is_captain && has;
    }
    is_captain && deliver_local
}

/// `HandleSocketClosed`: the host announces a player's departure (and unlinks it) and forgets the socket; a joiner
/// whose host socket closed kills it and reports the lost game.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonLanClosed {
    pub announce_left: bool,
    pub forget_socket: bool,
    pub lost_host: bool,
}

pub fn socket_closed(have_socket: bool, is_captain: bool, has_client: bool, is_server_socket: bool) -> EeWonLanClosed {
    if !have_socket {
        return EeWonLanClosed::default();
    }
    if is_captain {
        EeWonLanClosed { announce_left: has_client, forget_socket: true, lost_host: false }
    } else {
        EeWonLanClosed { announce_left: false, forget_socket: false, lost_host: is_server_socket }
    }
}

/// `HandleGameSummary`: a summary read keeps the game (stamped) and reports it added (0) or changed (2); an
/// unreadable one drops the game and reports a known game as changed (2: the original's `aFlag ? 2 : 1` under
/// `if (aFlag)`), a new one not at all (-1).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeWonLanSummary {
    pub keep: bool,
    pub event: i32,
}

pub fn summary_outcome(read_ok: bool, existed: bool) -> EeWonLanSummary {
    if read_ok {
        EeWonLanSummary { keep: true, event: if existed { 2 } else { 0 } }
    } else {
        EeWonLanSummary { keep: false, event: if existed { 2 } else { -1 } }
    }
}

/// `CanBroadcastGame`: this lobby hosts a LAN game and discovery runs.
pub fn can_broadcast(is_captain: bool, discovery: bool, have_game: bool, game_type: i32) -> bool {
    is_captain && discovery && have_game && game_type == crate::staging::GAME_LAN
}

/// `AdvertiseGame` / `RemoveGameAdvertisement`: only LAN games are advertised.
pub fn advertises(game_type: i32) -> bool {
    game_type == crate::staging::GAME_LAN
}

/// `HostGame` / `JoinGame`: a direct-connect game with the CD-key check authenticates its peers.
pub fn needs_peer_auth(game_type: i32, cd_key_check: bool) -> bool {
    game_type == crate::staging::GAME_DIRECT && cd_key_check
}

/// What `JoinGame`'s loop does with a join reply's status: joined; give up; tell the wrong password and ask again;
/// tell the other subnet and give up; send the request again as it was (any other status).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLanJoinStep {
    Joined = 0,
    Fail = 1,
    WrongPassword = 2,
    OtherSubnet = 3,
    Again = 4,
}

pub fn join_step(status: i32) -> EeWonLanJoinStep {
    match status {
        0 => EeWonLanJoinStep::Joined,
        1 => EeWonLanJoinStep::Fail,
        2 => EeWonLanJoinStep::WrongPassword,
        3 => EeWonLanJoinStep::OtherSubnet,
        _ => EeWonLanJoinStep::Again,
    }
}

/// `ReadJoinReply`: a listed player is the joiner itself (1), the host (2: linked to the host socket and made
/// captain) or another player (0). The joiner's own id wins over the captain's.
pub fn reply_role(id: u16, my_id: u16, captain_id: u16) -> i32 {
    if id == my_id {
        1
    } else if id == captain_id {
        2
    } else {
        0
    }
}

/// `HandleClientAction`'s actions in the LAN room (room spec bit 8, connected, a client): the options popup (18),
/// ignore (5), unignore (12), whisper (16), kick (7); 0 for anything else.
pub fn client_action(connected: bool, room_spec: i32, has_client: bool, action: i32) -> i32 {
    if !connected || room_spec & 8 == 0 || !has_client {
        return 0;
    }
    match action {
        18 | 5 | 12 | 16 | 7 => action,
        _ => 0,
    }
}

/// `HandleChatCommand`: what a typed command does in the LAN room.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub struct EeWonLanChatCommand {
    /// 0 nothing, 1 send a chat line (`flags`, to the target when `whisper`), 2 toggle the target's ignore,
    /// 3 the invalid command error.
    pub action: i32,
    pub flags: u16,
    pub whisper: bool,
}

pub fn chat_command(connected: bool, room_flags: i32, command: i32, has_target: bool) -> EeWonLanChatCommand {
    let none = EeWonLanChatCommand::default();
    if !connected || room_flags & 8 == 0 {
        return none;
    }
    match command {
        chat::BROADCAST | chat::EMOTE => {
            let mut flags = if command == chat::EMOTE { 2 } else { 0 };
            if room_flags & 0x10 != 0 {
                flags |= crate::lan::CHAT_TEAM;
            }
            EeWonLanChatCommand { action: 1, flags, whisper: false }
        }
        chat::WHISPER if has_target => {
            EeWonLanChatCommand { action: 1, flags: crate::lan::CHAT_WHISPER, whisper: true }
        }
        chat::IGNORE if has_target => EeWonLanChatCommand { action: 2, ..none },
        chat::WHISPER | chat::IGNORE => none,
        _ => EeWonLanChatCommand { action: 3, ..none },
    }
}

/// `HandleChatMessage`: a whisper from someone else plays the whisper sound (if chat sounds are on) and sets the
/// reply name.
pub fn whisper_received(whisper: bool, from_me: bool) -> bool {
    whisper && !from_me
}

/// `RefreshGames`: the LAN screen (1) clears its games and queries again, the direct-connect screen (3) pings its
/// games; 0 for any other screen.
pub fn refresh(screen: i32) -> i32 {
    match screen {
        1 | 3 => screen,
        _ => 0,
    }
}

/// The dialogs' OK result (`0x186a1`).
pub(crate) const DIALOG_OK: i32 = 0x186a1;

/// `ChangeLanName`: saved when the dialog was confirmed with a name.
pub fn lan_name_accepted(result: i32, name_length: usize) -> bool {
    result == DIALOG_OK && name_length != 0
}

/// `ChangeLanPort`: saved (and discovery restarted) when the dialog was confirmed with another port.
pub fn lan_port_accepted(result: i32, old_port: i32, new_port: i32) -> bool {
    result == DIALOG_OK && new_port != old_port
}

/// `HandleChangeDirectConnect`: the default port (the config's when there is one, else the lobby's), the game's
/// name (the address when none was typed) and the event (2 a known game changed, 0 a new one).
pub fn direct_default_port(have_config: bool, config_port: i32, lobby_port: i32) -> i32 {
    if have_config {
        config_port
    } else {
        lobby_port
    }
}

pub fn direct_name_is_address(name_length: usize) -> bool {
    name_length == 0
}

pub fn direct_event(existed: bool) -> i32 {
    if existed {
        2
    } else {
        0
    }
}

// ---- the C ABI

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_recv(op_ok: bool, is_captain: bool, socket_has_client: bool) -> EeWonLanRecv {
    recv(op_ok, is_captain, socket_has_client)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_message_offset(is_captain: bool, from_me: bool) -> usize {
    message_offset(is_captain, from_me)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_send_to_client(
    is_captain: bool,
    connected: bool,
    to_me: bool,
    has_socket: bool,
) -> EeWonLanRoute {
    send_to_client(is_captain, connected, to_me, has_socket)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_send_to_captain(
    connected: bool,
    is_captain: bool,
    has_server_socket: bool,
) -> EeWonLanRoute {
    send_to_captain(connected, is_captain, has_server_socket)
}

/// `broadcast` over `count` sockets: `send[i]` set for each socket sent to; returns whether it is handled locally.
///
/// # Safety
/// Both arrays hold `count` entries (or are NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_lanlogic_broadcast(
    is_captain: bool,
    deliver_local: bool,
    socket_has_client: *const bool,
    send: *mut bool,
    count: usize,
) -> bool {
    if count == 0 || socket_has_client.is_null() || send.is_null() {
        return broadcast(is_captain, deliver_local, &[], &mut []);
    }
    // SAFETY: `count` entries each (the contract)
    let (has, out) =
        unsafe { (std::slice::from_raw_parts(socket_has_client, count), std::slice::from_raw_parts_mut(send, count)) };
    broadcast(is_captain, deliver_local, has, out)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_socket_closed(
    have_socket: bool,
    is_captain: bool,
    has_client: bool,
    is_server_socket: bool,
) -> EeWonLanClosed {
    socket_closed(have_socket, is_captain, has_client, is_server_socket)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_summary(read_ok: bool, existed: bool) -> EeWonLanSummary {
    summary_outcome(read_ok, existed)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_can_broadcast(
    is_captain: bool,
    discovery: bool,
    have_game: bool,
    game_type: i32,
) -> bool {
    can_broadcast(is_captain, discovery, have_game, game_type)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_advertises(game_type: i32) -> bool {
    advertises(game_type)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_needs_peer_auth(game_type: i32, cd_key_check: bool) -> bool {
    needs_peer_auth(game_type, cd_key_check)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_join_step(status: i32) -> EeWonLanJoinStep {
    join_step(status)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_reply_role(id: u16, my_id: u16, captain_id: u16) -> i32 {
    reply_role(id, my_id, captain_id)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_client_action(connected: bool, room_spec: i32, has_client: bool, action: i32) -> i32 {
    client_action(connected, room_spec, has_client, action)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_chat_command(
    connected: bool,
    room_flags: i32,
    command: i32,
    has_target: bool,
) -> EeWonLanChatCommand {
    chat_command(connected, room_flags, command, has_target)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_whisper_received(whisper: bool, from_me: bool) -> bool {
    whisper_received(whisper, from_me)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_refresh(screen: i32) -> i32 {
    refresh(screen)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_name_accepted(result: i32, name_length: usize) -> bool {
    lan_name_accepted(result, name_length)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_port_accepted(result: i32, old_port: i32, new_port: i32) -> bool {
    lan_port_accepted(result, old_port, new_port)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_direct_default_port(have_config: bool, config_port: i32, lobby_port: i32) -> i32 {
    direct_default_port(have_config, config_port, lobby_port)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_direct_name_is_address(name_length: usize) -> bool {
    direct_name_is_address(name_length)
}

#[no_mangle]
pub extern "C" fn ee_won_lanlogic_direct_event(existed: bool) -> i32 {
    direct_event(existed)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn receiving_and_routing() {
        assert_eq!(recv(false, true, false), EeWonLanRecv::default());
        assert_eq!(recv(true, true, false), EeWonLanRecv { handle: true, stamp: true, new_socket: true });
        assert_eq!(recv(true, true, true), EeWonLanRecv { handle: true, stamp: true, new_socket: false });
        assert_eq!(recv(true, false, false), EeWonLanRecv { handle: true, stamp: false, new_socket: false });
        assert_eq!(message_offset(true, true), 2);
        assert_eq!(message_offset(true, false), 0);
        assert_eq!(message_offset(false, true), 0);
        assert_eq!(send_to_client(false, true, true, true), EeWonLanRoute::Drop);
        assert_eq!(send_to_client(true, false, true, true), EeWonLanRoute::Drop);
        assert_eq!(send_to_client(true, true, true, false), EeWonLanRoute::Local);
        assert_eq!(send_to_client(true, true, false, true), EeWonLanRoute::Socket);
        assert_eq!(send_to_client(true, true, false, false), EeWonLanRoute::Drop);
        assert_eq!(send_to_captain(false, true, true), EeWonLanRoute::Drop);
        assert_eq!(send_to_captain(true, true, false), EeWonLanRoute::Local);
        assert_eq!(send_to_captain(true, false, true), EeWonLanRoute::Socket);
        assert_eq!(send_to_captain(true, false, false), EeWonLanRoute::Drop);
        let mut send = [true; 3];
        assert!(!broadcast(false, true, &[true, true, false], &mut send));
        assert_eq!(send, [false; 3]);
        assert!(broadcast(true, true, &[true, false, true], &mut send));
        assert_eq!(send, [true, false, true]);
        assert!(!broadcast(true, false, &[], &mut []));
    }

    #[test]
    fn sockets_and_discovery() {
        assert_eq!(socket_closed(false, true, true, true), EeWonLanClosed::default());
        assert_eq!(
            socket_closed(true, true, true, false),
            EeWonLanClosed { announce_left: true, forget_socket: true, lost_host: false }
        );
        assert_eq!(
            socket_closed(true, true, false, true),
            EeWonLanClosed { announce_left: false, forget_socket: true, lost_host: false }
        );
        assert!(socket_closed(true, false, true, true).lost_host);
        assert_eq!(socket_closed(true, false, true, false), EeWonLanClosed::default());
        assert_eq!(summary_outcome(true, false), EeWonLanSummary { keep: true, event: 0 });
        assert_eq!(summary_outcome(true, true), EeWonLanSummary { keep: true, event: 2 });
        assert_eq!(summary_outcome(false, true), EeWonLanSummary { keep: false, event: 2 });
        assert_eq!(summary_outcome(false, false), EeWonLanSummary { keep: false, event: -1 });
        assert!(can_broadcast(true, true, true, 1));
        assert!(!can_broadcast(true, true, true, 3) && !can_broadcast(false, true, true, 1));
        assert!(!can_broadcast(true, false, true, 1) && !can_broadcast(true, true, false, 1));
        assert!(advertises(1) && !advertises(2) && !advertises(3));
    }

    #[test]
    fn joining() {
        assert!(needs_peer_auth(3, true) && !needs_peer_auth(3, false) && !needs_peer_auth(1, true));
        assert_eq!(join_step(0), EeWonLanJoinStep::Joined);
        assert_eq!(join_step(1), EeWonLanJoinStep::Fail);
        assert_eq!(join_step(2), EeWonLanJoinStep::WrongPassword);
        assert_eq!(join_step(3), EeWonLanJoinStep::OtherSubnet);
        assert_eq!(join_step(4), EeWonLanJoinStep::Again);
        assert_eq!(join_step(-1), EeWonLanJoinStep::Again);
        assert_eq!(reply_role(3, 3, 3), 1);
        assert_eq!(reply_role(1, 3, 1), 2);
        assert_eq!(reply_role(2, 3, 1), 0);
    }

    #[test]
    fn commands() {
        assert_eq!(client_action(true, 8, true, 18), 18);
        assert_eq!(client_action(true, 8, true, 6), 0);
        assert_eq!(client_action(false, 8, true, 7), 0);
        assert_eq!(client_action(true, 4, true, 7), 0);
        assert_eq!(client_action(true, 8, false, 7), 0);
        assert_eq!(
            chat_command(true, 8, chat::BROADCAST, false),
            EeWonLanChatCommand { action: 1, flags: 0, whisper: false }
        );
        assert_eq!(chat_command(true, 0x18, chat::EMOTE, false).flags, 0x102);
        assert_eq!(
            chat_command(true, 8, chat::WHISPER, true),
            EeWonLanChatCommand { action: 1, flags: 4, whisper: true }
        );
        assert_eq!(chat_command(true, 8, chat::WHISPER, false), EeWonLanChatCommand::default());
        assert_eq!(chat_command(true, 8, chat::IGNORE, true).action, 2);
        assert_eq!(chat_command(true, 8, chat::IGNORE, false).action, 0);
        assert_eq!(chat_command(true, 8, chat::AWAY, false).action, 3);
        assert_eq!(chat_command(true, 4, chat::BROADCAST, false).action, 0);
        assert_eq!(chat_command(false, 8, chat::BROADCAST, false).action, 0);
        assert!(whisper_received(true, false) && !whisper_received(true, true) && !whisper_received(false, false));
        assert_eq!((refresh(1), refresh(3), refresh(2)), (1, 3, 0));
        assert!(lan_name_accepted(DIALOG_OK, 1) && !lan_name_accepted(DIALOG_OK, 0) && !lan_name_accepted(1, 3));
        assert!(
            lan_port_accepted(DIALOG_OK, 1, 2) && !lan_port_accepted(DIALOG_OK, 2, 2) && !lan_port_accepted(0, 1, 2)
        );
        assert_eq!(direct_default_port(true, 5, 6), 5);
        assert_eq!(direct_default_port(false, 5, 6), 6);
        assert!(direct_name_is_address(0) && !direct_name_is_address(1));
        assert_eq!((direct_event(true), direct_event(false)), (2, 0));
    }
}
