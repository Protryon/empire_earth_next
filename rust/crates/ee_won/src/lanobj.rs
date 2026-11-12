//! LanLogic as a Rust object (R6 WON round 20): the LAN and direct-connect lobby, hosting and joining peer games
//! over TCP (the host relays chat and staging) and the UDP discovery of LAN games. LanLogic.cpp's statements in
//! order over `lan` (the messages) and `lanlogic` (the decisions); its scalar state (the next client id, the
//! captain and peer-auth flags, the last host ping and broadcast times) lives here, its SmartPtrs (the dialogs, the
//! game lists, the sockets and their set, the timers, the discovery address, the clients) stay in the C++ shell
//! (`WONLobbyLan.h`). Every C++ call goes through the host table (`lobbyhost`); the pure C++ steps without a
//! decision (a socket's arming, the certificate refresh, the summary's read, the peer-auth check) are host ops.
//!
//! Re-entry as StagingLogic's: the dialogs and the status waits run the lobby, and the socket completions call back
//! in (`Cell`s behind a shared reference).

use std::cell::Cell;
use std::ffi::c_void;
use std::ptr::null_mut;

use crate::lan::{self, GameMsg};
use crate::lanlogic::{self, EeWonLanJoinStep, EeWonLanRoute};
use crate::lobbyhost::{EeWonLobbyHostFn, EeWonLobbyOp as Op, EeWonLobbyText, Host, Msg};

const DIALOG_OK: isize = 0x186a1;
const EVENT_CLIENT_ACTION: isize = 100005;
const EVENT_CHANGE_DIRECT_CONNECT: isize = 100008;
const EVENT_CHAT_COMMAND: isize = 0x186ac;

/// The inward calls (LanLogic's virtual slots and its completions). `a`, `b` pointers, `c`, `d` integers; a
/// message is `a` its `ByteBuffer*`, `c` its bytes, `d` their length.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLanCall {
    /// The LAN name (1), LAN port (2), refresh (3) and LAN screen (4) buttons (`c`).
    Control = 1,
    /// A component event: `c` its type, `a` the event.
    ComponentEvent,
    HostGame,
    JoinGame,
    SendToCaptain,
    BroadcastToGame,
    /// `SendToClient(b, message)`.
    SendToClient,
    AdvertiseGame,
    RemoveGameAdvertisement,
    /// `KickClient(b)`.
    KickClient,
    LeaveGame,
    Logout,
    /// The completions (`a` the op).
    SocketClosed,
    NetworkTimer,
    RecvMsg,
    DiscoveryTimer,
    DiscoveryRecv,
    Accept,
}

/// LanLogic's scalar state (the original's 0x6c, 0x74, 0xa8, 0xb4, 0xb6).
pub struct EeWonLan {
    host: Host,
    next_client_id: Cell<u16>,
    is_captain: Cell<bool>,
    require_peer_auth: Cell<bool>,
    last_server_ping: Cell<i64>,
    last_broadcast_time: Cell<i64>,
}

/// A client reference the Rust side holds (the original's local `SmartPtr<LobbyClient>`), given back when dropped.
struct HeldClient {
    host: Host,
    client: *mut c_void,
}

impl Drop for HeldClient {
    fn drop(&mut self) {
        if !self.client.is_null() {
            self.host.on(Op::ClientRelease, self.client);
        }
    }
}

fn units(bytes: &[u8]) -> Vec<u16> {
    bytes.as_chunks::<2>().0.iter().map(|p| u16::from_le_bytes(*p)).collect()
}

impl EeWonLan {
    pub fn new(logic: *mut c_void, call: EeWonLobbyHostFn) -> Self {
        EeWonLan {
            host: Host { logic, call },
            next_client_id: Cell::new(0),
            is_captain: Cell::new(false),
            require_peer_auth: Cell::new(false),
            last_server_ping: Cell::new(0),
            last_broadcast_time: Cell::new(0),
        }
    }

    fn h(&self) -> &Host {
        &self.host
    }
    /// `GetNextClientId`.
    fn next_id(&self) -> u16 {
        let mut next = self.next_client_id.get();
        let id = lan::next_client_id(&mut next);
        self.next_client_id.set(next);
        id
    }
    fn ptr(&self, op: Op) -> *mut c_void {
        self.h().get(op) as *mut c_void
    }
    fn my_client(&self) -> *mut c_void {
        self.ptr(Op::LanMyClient)
    }
    fn held(&self, client: *mut c_void) -> HeldClient {
        HeldClient { host: *self.h(), client }
    }
    fn client_id(&self, client: *mut c_void) -> u16 {
        self.h().on(Op::ClientId, client) as u16
    }
    fn game_type(&self, game: *mut c_void) -> i32 {
        self.h().on(Op::GameType, game) as i32
    }
    /// A take op: its result and the bytes it appended.
    fn take(&self, op: Op, a: *mut c_void) -> (isize, Vec<u8>) {
        let mut v: Vec<u8> = Vec::new();
        let r = self.h().op(op, a, &mut v as *mut Vec<u8> as *mut c_void, 0, 0);
        (r, v)
    }
    fn take_text(&self, op: Op, a: *mut c_void) -> Vec<u16> {
        units(&self.take(op, a).1)
    }
    fn text_op(&self, op: Op, a: *mut c_void, text: &[u16]) -> isize {
        self.h().op(op, a, null_mut(), text.as_ptr() as isize, text.len() as isize)
    }
    fn new_client(&self, name: &[u16]) -> HeldClient {
        let c = self.text_op(Op::LanNewClient, null_mut(), name) as *mut c_void;
        self.held(c)
    }
    fn discovery_send(&self, bytes: &[u8]) {
        let address = self.ptr(Op::LanDiscoveryAddress);
        self.h().msg(Op::LanDiscoverySend, Msg::built(bytes), address);
    }

    /// `IsConnected`: hosting (listening) or connected to a host.
    fn is_connected(&self) -> bool {
        self.h().get(Op::LanListenSocket) != 0 || self.h().get(Op::LanServerSocket) != 0
    }

    /// Leaves the LAN game: stops certificate refresh, closes the listen, host and player sockets, clears the players.
    fn close_connections(&self) {
        self.h().get(Op::LanAuthAutoRefreshOff);
        self.next_client_id.set(0);
        self.is_captain.set(false);
        self.require_peer_auth.set(false);
        self.h().get(Op::LanCloseSockets);
    }

    /// Host: announces the departed player and drops its socket; joiner: reports the lost host connection.
    fn handle_socket_closed(&self, op: *mut c_void) {
        let socket = self.h().on_ptr(Op::LanOpSocket, op);
        let have = !socket.is_null();
        let has_client = have && !self.h().on_ptr(Op::LanSocketClient, socket).is_null();
        let is_server = have && socket == self.ptr(Op::LanServerSocket);
        let closed = lanlogic::socket_closed(have, self.is_captain.get(), has_client, is_server);
        if closed.announce_left {
            let client = self.h().on_ptr(Op::LanSocketClient, socket);
            let message = lan::client_left(self.client_id(client));
            self.h().op(Op::LanSocketSetClient, socket, null_mut(), 0, 0);
            self.broadcast_message(Msg::built(&message), true);
        }
        if closed.forget_socket {
            self.h().on(Op::LanForgetSocket, socket);
        }
        if closed.lost_host {
            self.h().get(Op::LanLoseHost);
        }
    }

    /// Host: drops sockets silent for 5 minutes; joiner: pings the host after 2 idle minutes; then rearms.
    fn handle_network_timer(&self, op: *mut c_void) {
        if op != self.ptr(Op::LanNetworkTimer) {
            return;
        }
        if self.is_captain.get() {
            let now = self.h().get(Op::Now) as i64;
            let mut i = 0;
            while i < self.h().get(Op::LanPendingCount) {
                let socket = self.h().int(Op::LanPendingAt, i) as *mut c_void;
                if lan::host_socket_stale(now, self.h().on(Op::LanSocketLastTime, socket) as i64) {
                    self.h().op(Op::LanSocketClose, socket, null_mut(), 1000, 0);
                }
                i += 1;
            }
        } else if self.h().get(Op::LanServerSocket) != 0
            && lan::joiner_ping_due(self.h().get(Op::Now) as i64, self.last_server_ping.get())
        {
            self.send_message_to_captain(Msg::built(&lan::ping()));
        }
        self.h().get(Op::LanStartNetworkTimer);
    }

    /// Host: answers a join request: rejects other subnets (LAN) or a wrong password, else adds and announces the
    /// player.
    fn handle_join_request(&self, name: &[u16], password: &[u16], socket: *mut c_void) {
        if self.my_client().is_null() {
            return;
        }
        let game = self.ptr(Op::LanMyGame);
        if game.is_null() {
            return;
        }
        let (mut peer, mut local) = ([0u8; 2], [0u8; 2]);
        if self.game_type(game) == 1 {
            let nets = self.h().on(Op::LanSubnets, socket) as u32;
            peer = [nets as u8, (nets >> 8) as u8];
            local = [(nets >> 16) as u8, (nets >> 24) as u8];
        }
        let kind = self.game_type(game);
        let matches = self.text_op(Op::LanGamePasswordIs, game, password) != 0;
        let status = lan::join_status(kind, peer, local, matches);
        if status != 0 {
            self.h().msg(Op::LanSocketSend, Msg::built(&lan::join_refused(status)), socket);
            return;
        }
        let client = self.new_client(name);
        let id = self.next_id();
        self.h().op(Op::LanAddClient, client.client, null_mut(), id as isize, 0);
        let joined_name = self.take_text(Op::ClientNameTake, client.client);
        let notify = lan::client_joined(self.client_id(client.client), &joined_name);
        self.broadcast_message(Msg::built(&notify), true);
        self.h().op(Op::LanSocketSetClient, socket, client.client, 0, 0);
        let mut listed = Vec::new();
        let mut i = 0;
        while i < self.h().get(Op::LanClientCount) {
            let c = self.h().int(Op::LanClientAt, i) as *mut c_void;
            let id = self.client_id(c);
            listed.push((id, self.take_text(Op::ClientNameTake, c)));
            i += 1;
        }
        let me = self.my_client();
        let captain = self.client_id(me);
        let reply = lan::join_accepted(captain, self.client_id(client.client), &listed);
        self.send_message_to_client(client.client, Msg::built(&reply));
    }

    /// Reads the host's join reply into the player list and the game's captain; its status, 1 on bad data.
    fn read_join_reply(&self, m: Msg, game: *mut c_void) -> i32 {
        if m.is_none() || self.h().get(Op::LanServerSocket) == 0 || game.is_null() {
            return 1;
        }
        // SAFETY: the message's bytes (a Rust buffer here)
        let reply = lan::read_join_reply(unsafe { message_bytes(&m) });
        if let Some(captain) = reply.captain {
            self.h().op(Op::GameSetCaptainId, game, null_mut(), captain as isize, 0);
        }
        for (id, name) in &reply.clients {
            let client = self.new_client(name);
            self.h().op(Op::LanAddClient, client.client, null_mut(), *id as isize, 0);
            match lanlogic::reply_role(*id, reply.you, reply.captain.unwrap_or(0)) {
                1 => {
                    self.h().on(Op::LanSetMyClient, client.client);
                }
                2 => {
                    self.h().on(Op::LanServerClientIsCaptain, client.client);
                }
                _ => {}
            }
        }
        reply.status
    }

    /// Host: handles the first message of a socket that has no player yet (only join requests).
    fn handle_new_socket_message(&self, m: Msg, socket: *mut c_void) {
        if m.is_none() || socket.is_null() || !self.is_captain.get() {
            return;
        }
        // SAFETY: the message's bytes
        if let Ok(GameMsg::JoinRequest { name, password }) = lan::read_game(unsafe { message_bytes(&m) }, 0) {
            self.handle_join_request(&name, &password, socket);
        }
    }

    /// Handles a received LAN message: stamps the socket's activity (host) and dispatches it.
    fn handle_recv_msg(&self, op: *mut c_void) {
        let socket = self.h().on_ptr(Op::LanOpSocket, op);
        let ok = self.h().on_bool(Op::LanRecvOk, op);
        let has_client = ok && !self.h().on_ptr(Op::LanSocketClient, socket).is_null();
        let recv = lanlogic::recv(ok, self.is_captain.get(), has_client);
        if !recv.handle {
            return;
        }
        if recv.stamp {
            self.h().on(Op::LanSocketStamp, socket);
        }
        let (buffer, bytes) = self.take(Op::LanRecvMsgTake, op);
        let m = received(buffer, &bytes);
        if recv.new_socket {
            self.handle_new_socket_message(m, socket);
            return;
        }
        let client = self.h().on_ptr(Op::LanSocketClient, socket);
        self.handle_message(m, client);
    }

    /// Host: sends a message to one player, handling it directly when that is the host itself.
    fn send_message_to_client(&self, client: *mut c_void, m: Msg) {
        let captain = self.is_captain.get() && self.is_connected();
        let to_me = captain && client == self.my_client();
        let has_socket = captain && !to_me && !self.h().on_ptr(Op::LanClientSocket, client).is_null();
        match lanlogic::send_to_client(self.is_captain.get(), self.is_connected(), to_me, has_socket) {
            EeWonLanRoute::Local => self.handle_message(m, self.my_client()),
            EeWonLanRoute::Socket => {
                self.h().msg(Op::LanClientSocketSend, m, client);
            }
            _ => {}
        }
    }

    /// Sends a message to the host, or handles it directly when this player is the host.
    fn send_message_to_captain(&self, m: Msg) {
        let connected = self.is_connected();
        let has_server = self.h().get(Op::LanServerSocket) != 0;
        match lanlogic::send_to_captain(connected, self.is_captain.get(), has_server) {
            EeWonLanRoute::Local => self.handle_message(m, self.my_client()),
            EeWonLanRoute::Socket => {
                self.last_server_ping.set(self.h().get(Op::Now) as i64);
                self.h().msg(Op::LanServerSend, m, null_mut());
            }
            _ => {}
        }
    }

    /// Host: sends a message to every joined player, optionally handling it locally too.
    fn broadcast_message(&self, m: Msg, deliver_local: bool) {
        let captain = self.is_captain.get();
        if lanlogic::broadcast(captain, deliver_local, &[], &mut []) {
            self.handle_message(m, self.my_client());
        }
        if !captain {
            return;
        }
        let mut sockets = Vec::new();
        let mut i = 0;
        while i < self.h().get(Op::LanPendingCount) {
            sockets.push(self.h().int(Op::LanPendingAt, i) as *mut c_void);
            i += 1;
        }
        if sockets.is_empty() {
            return;
        }
        let has_client: Vec<bool> =
            sockets.iter().map(|s| !self.h().on_ptr(Op::LanSocketClient, *s).is_null()).collect();
        let mut send = vec![false; sockets.len()];
        lanlogic::broadcast(captain, false, &has_client, &mut send);
        for (socket, send) in sockets.iter().zip(send) {
            if send {
                self.h().msg(Op::LanSocketSend, m, *socket);
            }
        }
    }

    /// The staging message wrapped (after the type byte, no length of its own).
    fn staging(m: Msg) -> Vec<u8> {
        // SAFETY: the message's bytes (the C++ ByteBuffer's for the call)
        lan::staging(unsafe { message_bytes(&m) })
    }

    /// Adds the announced player (the host already has it) and broadcasts a client-added event.
    fn handle_client_joined(&self, id: u16, name: &[u16]) {
        let client = if self.is_captain.get() {
            let c = self.h().int(Op::LanFindClient, id as isize) as *mut c_void;
            if !c.is_null() {
                self.h().on(Op::ClientHold, c);
            }
            self.held(c)
        } else {
            let c = self.new_client(name);
            self.h().op(Op::LanAddClient, c.client, null_mut(), id as isize, 0);
            c
        };
        if !client.client.is_null() {
            self.h().op(Op::LanClientChanged, client.client, null_mut(), 0, 0);
        }
    }

    /// Removes the announced player and tells the lobby the player left the game.
    fn handle_client_left(&self, id: u16) {
        let client = self.held(self.h().int(Op::LanRemoveClient, id as isize) as *mut c_void);
        if !client.client.is_null() {
            self.h().op(Op::LanClientChanged, client.client, null_mut(), 1, 0);
            self.h().on(Op::ClientLeftGame, client.client);
        }
    }

    /// Host: relays a chat line to its whisper target and sender, to the sender's team, or to everyone.
    fn handle_chat_request(&self, flags: u16, text: &[u16], target: u16, sender: *mut c_void) {
        if !self.is_captain.get() {
            return;
        }
        let line = lan::chat_line(flags, self.client_id(sender), text, target);
        let m = Msg::built(&line);
        match lan::chat_route(flags) {
            lan::ChatRoute::Whisper => {
                let to = self.h().int(Op::LanFindClient, target as isize) as *mut c_void;
                if to.is_null() {
                    return;
                }
                self.send_message_to_client(to, m);
                if sender != to {
                    self.send_message_to_client(sender, m);
                }
            }
            lan::ChatRoute::Team => {
                let mut i = 0;
                while i < self.h().get(Op::LanClientCount) {
                    let c = self.h().int(Op::LanClientAt, i) as *mut c_void;
                    if self.h().op(Op::ClientIsTeammate, c, sender, 0, 0) != 0 {
                        self.send_message_to_client(c, m);
                    }
                    i += 1;
                }
            }
            lan::ChatRoute::Everyone => self.broadcast_message(m, true),
        }
    }

    /// Shows a relayed chat line unless the sender is ignored; whispers play a sound and set the reply name.
    fn handle_chat_message(&self, flags: u16, sender_id: u16, text: &[u16], target_id: u16) {
        let sender = self.h().int(Op::LanFindClient, sender_id as isize) as *mut c_void;
        if sender.is_null() || self.h().on_bool(Op::ClientIsIgnored, sender) {
            return;
        }
        let mut target = null_mut();
        let line_flags = lan::chat_line_flags(flags);
        if lan::chat_route(flags) == lan::ChatRoute::Whisper {
            target = self.h().int(Op::LanFindClient, target_id as isize) as *mut c_void;
            if target.is_null() {
                return;
            }
            if lanlogic::whisper_received(true, sender == self.my_client()) {
                self.h().on(Op::LanWhisperReceived, sender);
            }
        }
        let d = ((line_flags as isize) << 32) | text.len() as isize;
        self.h().op(Op::LanRecvChat, sender, target, text.as_ptr() as isize, d);
    }

    /// Dispatches a LAN message by type: player joined/left, chat request, chat line, staging.
    fn handle_message(&self, m: Msg, client: *mut c_void) {
        if m.is_none() || client.is_null() {
            return;
        }
        let offset = lanlogic::message_offset(self.is_captain.get(), client == self.my_client());
        // SAFETY: the message's bytes; the read copies what it keeps
        let read = lan::read_game(unsafe { message_bytes(&m) }, offset);
        match read {
            Ok(GameMsg::ClientJoined { id, name }) => self.handle_client_joined(id, &name),
            Ok(GameMsg::ClientLeft { id }) => self.handle_client_left(id),
            Ok(GameMsg::ChatRequest { flags, text, target }) => self.handle_chat_request(flags, &text, target, client),
            Ok(GameMsg::ChatLine { flags, sender, text, target }) => {
                self.handle_chat_message(flags, sender, &text, target)
            }
            Ok(GameMsg::Staging) => {
                // the bytes from the type byte (the read above proved they reach it)
                let rest = Msg { buffer: null_mut(), data: m.data.wrapping_add(offset), length: m.length - offset };
                self.h().msg(Op::LanStagingRead, rest, client);
            }
            _ => {}
        }
    }

    /// Runs the LAN name, LAN port, refresh game list and show LAN screen commands.
    fn control(&self, which: isize) {
        match which {
            1 => self.change_lan_name(),
            2 => self.change_lan_port(),
            3 => self.refresh_games(),
            4 => self.enter_lan_screen(),
            _ => {}
        }
    }

    /// Handles client actions, direct-connect list changes and chat commands.
    fn component_event(&self, kind: isize, event: *mut c_void) {
        match kind {
            EVENT_CLIENT_ACTION => self.handle_client_action(event),
            EVENT_CHANGE_DIRECT_CONNECT => self.handle_change_direct_connect(event),
            EVENT_CHAT_COMMAND => {
                let component = self.h().on_ptr(Op::EventComponent, event);
                self.handle_chat_command(component);
            }
            _ => {}
        }
    }

    /// Asks for a new LAN player name and saves it when confirmed and not empty.
    fn change_lan_name(&self) {
        let result = self.h().get(Op::LanNameDialogRun);
        let length = if result == DIALOG_OK { self.h().get(Op::LanNameDialogLength) as usize } else { 0 };
        if lanlogic::lan_name_accepted(result as i32, length) {
            self.h().get(Op::LanNameDialogSave);
        }
    }

    /// Asks for a new LAN broadcast port; on change saves it and restarts discovery.
    fn change_lan_port(&self) {
        let port = self.h().get(Op::LanBroadcastPort);
        let result = self.h().int(Op::LanPortDialogRun, port);
        let new_port = if result == DIALOG_OK { self.h().get(Op::LanPortDialogPort) } else { port };
        if !lanlogic::lan_port_accepted(result as i32, port as i32, new_port as i32) {
            return;
        }
        self.h().int(Op::LanSetBroadcastPort, new_port);
        self.start_discovery();
    }

    /// Removes a saved direct-connect game, or asks for an address and adds or renames one.
    fn handle_change_direct_connect(&self, event: *mut c_void) {
        if self.h().on_bool(Op::EventDirectIsDelete, event) {
            let game = self.h().on_ptr(Op::EventDirectGame, event);
            if !game.is_null() {
                self.h().on(Op::LanDirectRemove, game);
            }
        } else if self.h().on_bool(Op::EventDirectIsAdd, event) {
            self.h().get(Op::LanDirectAdd);
        }
    }

    /// Opens the options popup for a player.
    fn show_client_options(&self, client: *mut c_void, room_spec: isize) {
        if self.my_client().is_null() || client.is_null() {
            return;
        }
        self.h().op(Op::LanShowClientOptions, client, null_mut(), room_spec, 0);
    }

    /// Handles a player action in the LAN room: options popup, ignore, unignore, whisper or kick.
    fn handle_client_action(&self, event: *mut c_void) {
        let client = self.h().on_ptr(Op::EventClientActionClient, event);
        let connected = self.is_connected();
        let room_spec = self.h().on(Op::EventClientActionRoomSpec, event) as i32;
        let action = self.h().on(Op::EventClientActionAction, event) as i32;
        match lanlogic::client_action(connected, room_spec, !client.is_null(), action) {
            18 => {
                let room_spec = self.h().on(Op::EventClientActionRoomSpec, event);
                self.show_client_options(client, room_spec);
            }
            5 => {
                self.h().op(Op::SetClientIgnored, client, null_mut(), 1, 8);
            }
            12 => {
                self.h().op(Op::SetClientIgnored, client, null_mut(), 0, 8);
            }
            16 => {
                let room_spec = self.h().on(Op::EventClientActionRoomSpec, event);
                self.h().op(Op::LanSetChatInput, client, null_mut(), room_spec, 0);
            }
            7 => {
                self.h().on(Op::KickClientFromLobby, client);
            }
            _ => {}
        }
    }

    /// Sends a chat request (text, flags, whisper target) to the host.
    fn send_chat(&self, text: &[u16], flags: u16, target: u16) {
        self.send_message_to_captain(Msg::built(&lan::chat_request(flags, text, target)));
    }

    /// Sends a typed LAN chat line, emote or whisper, or toggles ignore; else reports an error.
    fn handle_chat_command(&self, component: *mut c_void) {
        if !self.is_connected() {
            return;
        }
        let parser = self.ptr(Op::ChatParser);
        if parser.is_null() || self.h().on(Op::ParserRoomFlags, parser) & 8 == 0 {
            return;
        }
        let client = self.h().on_ptr(Op::ParserTarget, parser);
        let room_flags = self.h().on(Op::ParserRoomFlags, parser) as i32;
        let command = self.h().on(Op::ParserCommand, parser) as i32;
        let c = lanlogic::chat_command(true, room_flags, command, !client.is_null());
        match c.action {
            1 => {
                let text = self.take_text(Op::ParserTextTake, parser);
                let target = if c.whisper { self.client_id(client) } else { 0 };
                self.send_chat(&text, c.flags, target);
            }
            2 => {
                let ignored = self.h().on_bool(Op::ClientIsIgnored, client);
                self.h().op(Op::SetClientIgnored, client, null_mut(), !ignored as isize, 4);
            }
            3 => {
                self.h().on(Op::LanChatCommandError, component);
            }
            _ => {}
        }
    }

    /// LAN screen: clears the games and broadcasts a discovery query; direct connect: pings the shown games.
    fn refresh_games(&self) {
        match lanlogic::refresh(self.h().get(Op::GameScreenType) as i32) {
            1 => {
                self.h().get(Op::LanClearLanGames);
                if self.h().get(Op::LanDiscoverySocket) == 0 {
                    return;
                }
                self.discovery_send(&lan::discovery(lan::DISCOVERY_QUERY, 0));
            }
            3 => {
                self.h().get(Op::LanPingDirectGames);
            }
            _ => {}
        }
    }

    /// Enters the LAN screen: loads the saved direct-connect games and starts LAN discovery.
    fn enter_lan_screen(&self) {
        self.h().get(Op::LanEnterScreenLists);
        self.start_discovery();
        self.h().get(Op::ShowLanScreen);
    }

    /// Both games' peer authentication check (a direct-connect game with the CD key check on).
    fn needs_peer_auth(&self, game: *mut c_void) -> bool {
        self.game_type(game) == 3 && {
            let kind = self.game_type(game);
            lanlogic::needs_peer_auth(kind, self.h().get(Op::DoCDKeyCheck) != 0)
        }
    }

    /// Hosts a LAN game: certificate check for direct connect, listens on the lobby port, joins as captain.
    fn host_game(&self, game: *mut c_void) -> bool {
        self.close_connections();
        if self.needs_peer_auth(game) {
            self.require_peer_auth.set(true);
            if self.h().get(Op::LanRefreshCertificate) == 0 {
                return false;
            }
        }
        self.h().get(Op::KillFirewallTest);
        self.is_captain.set(true);
        let id = self.next_id();
        self.h().op(Op::LanHostMyClient, game, null_mut(), id as isize, 0);
        if self.h().on(Op::LanHostListen, game) == 0 {
            return false;
        }
        self.last_broadcast_time.set(0);
        self.h().get(Op::LanStartNetworkTimer);
        true
    }

    /// Host: closes the player's socket.
    fn kick_client(&self, client: *mut c_void) {
        if self.is_captain.get() {
            let socket = self.h().on_ptr(Op::LanClientSocket, client);
            self.h().op(Op::LanSocketClose, socket, null_mut(), 1000, 0);
        }
    }

    /// Broadcasts a LAN game's summary on the discovery port.
    fn advertise_game(&self, game: *mut c_void) {
        if lanlogic::advertises(self.game_type(game)) {
            let address = self.ptr(Op::LanDiscoveryAddress);
            self.broadcast_game_summary(game, address);
        }
    }

    /// Broadcasts that a LAN game is gone.
    fn remove_game_advertisement(&self, game: *mut c_void) {
        if lanlogic::advertises(self.game_type(game)) {
            self.broadcast_game_removed(game);
        }
    }

    /// This player hosts a LAN game and discovery is running.
    fn can_broadcast_game(&self, game: *mut c_void) -> bool {
        let discovery = self.h().get(Op::LanDiscoverySocket) != 0;
        let kind = if game.is_null() { 0 } else { self.game_type(game) };
        lanlogic::can_broadcast(self.is_captain.get(), discovery, !game.is_null(), kind)
    }

    /// Sends the hosted game's port and summary to an address.
    fn broadcast_game_summary(&self, game: *mut c_void, address: *mut c_void) {
        if !self.can_broadcast_game(game) {
            return;
        }
        let port = self.h().on(Op::GameAddressPort, game) as u16;
        let head = lan::discovery(lan::DISCOVERY_SUMMARY, port);
        self.h().op(Op::LanSendSummary, game, address, head.as_ptr() as isize, head.len() as isize);
    }

    /// Broadcasts a keep-alive for the hosted game, or its full summary every 30 seconds.
    fn broadcast_game_keep_alive(&self, game: *mut c_void) {
        if !self.can_broadcast_game(game) {
            return;
        }
        let now = self.h().get(Op::Now) as i64;
        if lan::summary_due(now, self.last_broadcast_time.get()) {
            let address = self.ptr(Op::LanDiscoveryAddress);
            self.broadcast_game_summary(game, address);
        } else {
            let port = self.h().on(Op::GameAddressPort, game) as u16;
            self.discovery_send(&lan::discovery(lan::DISCOVERY_KEEP_ALIVE, port));
        }
        self.last_broadcast_time.set(now);
    }

    /// Broadcasts that the hosted game is gone.
    fn broadcast_game_removed(&self, game: *mut c_void) {
        if !self.can_broadcast_game(game) {
            return;
        }
        let port = self.h().on(Op::GameAddressPort, game) as u16;
        self.discovery_send(&lan::discovery(lan::DISCOVERY_REMOVED, port));
    }

    /// Sends the host's keep-alive, drops LAN games unseen for 30 seconds, then rearms.
    fn handle_discovery_timer(&self, op: *mut c_void) {
        if op != self.ptr(Op::LanDiscoveryTimer) {
            return;
        }
        if self.is_captain.get() {
            let game = self.ptr(Op::LanMyGame);
            if !game.is_null() {
                self.broadcast_game_keep_alive(game);
            }
        }
        self.h().get(Op::LanRemoveStaleGames);
        self.h().get(Op::LanStartDiscoveryTimer);
    }

    /// Dispatches a discovery packet: game summary, keep-alive, removal or query.
    fn handle_discovery_recv(&self, op: *mut c_void) {
        if !self.h().on_bool(Op::LanDiscoveryOk, op) {
            return;
        }
        let (_, bytes) = self.take(Op::LanDiscoveryTake, op);
        let Ok((kind, port)) = lan::read_discovery(&bytes) else {
            return;
        };
        match kind {
            lan::DISCOVERY_SUMMARY => {
                self.h().op(Op::LanGameSummary, op, null_mut(), port as isize, 0);
            }
            lan::DISCOVERY_KEEP_ALIVE => {
                self.h().op(Op::LanGameKeepAlive, op, null_mut(), port as isize, 0);
            }
            lan::DISCOVERY_REMOVED => {
                self.h().op(Op::LanGameRemoved, op, null_mut(), port as isize, 0);
            }
            lan::DISCOVERY_QUERY => {
                let from = self.h().on_ptr(Op::LanRecvFrom, op);
                let game = self.ptr(Op::LanMyGame);
                if !game.is_null() {
                    self.broadcast_game_summary(game, from);
                }
            }
            _ => {}
        }
    }

    /// Opens the broadcast discovery socket, queries for LAN games and advertises the hosted game.
    fn start_discovery(&self) {
        self.h().get(Op::LanStopDiscovery);
        if self.h().get(Op::LanOpenDiscovery) != 0 {
            self.h().get(Op::LanStopDiscovery);
            return;
        }
        self.h().get(Op::LanDiscoveryArm);
        self.h().get(Op::LanStartDiscoveryTimer);
        self.discovery_send(&lan::discovery(lan::DISCOVERY_QUERY, 0));
        if !self.is_captain.get() {
            return;
        }
        let game = self.ptr(Op::LanMyGame);
        if !game.is_null() {
            let address = self.ptr(Op::LanDiscoveryAddress);
            self.broadcast_game_summary(game, address);
        }
    }

    /// Stops LAN discovery and clears the direct-connect games.
    fn logout(&self) {
        self.h().get(Op::LanStopDiscovery);
        self.h().get(Op::LanClearDirectGames);
    }

    /// Host: takes an accepted connection, running peer authentication first when required.
    fn handle_accept(&self, op: *mut c_void) {
        let socket = self.h().on_ptr(Op::LanAcceptedSocket, op);
        if socket.is_null() {
            return;
        }
        if self.require_peer_auth.get() {
            self.h().on(Op::LanStartPeerAuth, socket);
        } else {
            self.h().on(Op::LanAddPending, socket);
        }
    }

    /// Joins a LAN or direct-connect game: connects (peer auth if needed), asks again on a wrong password.
    fn join_game(&self, game: *mut c_void) -> bool {
        self.close_connections();
        if self.needs_peer_auth(game) {
            self.require_peer_auth.set(true);
            if self.h().get(Op::LanRefreshCertificate) == 0 {
                return false;
            }
        }
        self.h().get(Op::LanNewServerSocket);
        let mut joined = false;
        let mut need_password = self.h().on_bool(Op::GameHasPassword, game);
        let mut need_connect = true;
        while !joined {
            self.h().get(Op::LanJoinPasswordClear);
            if need_password && self.h().get(Op::LanPasswordDialogRun) != DIALOG_OK {
                return false;
            }
            if need_connect {
                let peer = self.require_peer_auth.get() as isize;
                if self.h().op(Op::LanConnect, game, null_mut(), peer, 0) == 0 {
                    return false;
                }
                need_connect = false;
            }
            let name = self.take_text(Op::LanNameTake, null_mut());
            let secret = self.take_text(Op::LanJoinPasswordTake, null_mut());
            let request = lan::join_request(&name, &secret);
            self.h().msg(Op::LanServerSend, Msg::built(&request), null_mut());
            self.last_server_ping.set(self.h().get(Op::Now) as i64);
            let (got, bytes) = self.take(Op::LanRecvReplyTake, null_mut());
            if got < 0 {
                return false;
            }
            let m = if got == 0 {
                Msg { buffer: null_mut(), data: std::ptr::null(), length: 0 }
            } else {
                built_or_empty(&bytes)
            };
            let status = self.read_join_reply(m, game);
            match lanlogic::join_step(status) {
                EeWonLanJoinStep::Joined => joined = true,
                EeWonLanJoinStep::Fail => return false,
                EeWonLanJoinStep::WrongPassword => {
                    self.h().int(Op::MessageBox, EeWonLobbyText::WrongPassword as isize);
                    need_password = true;
                }
                EeWonLanJoinStep::OtherSubnet => {
                    self.h().int(Op::MessageBox, EeWonLobbyText::OtherSubnet as isize);
                    return false;
                }
                EeWonLanJoinStep::Again => {}
            }
        }
        self.h().on(Op::LanJoinArm, game);
        true
    }

    /// One inward call.
    ///
    /// # Safety
    /// The call's arguments as `EeWonLanCall` documents (a message's bytes valid for the call).
    pub unsafe fn call(&self, call: EeWonLanCall, a: *mut c_void, b: *mut c_void, c: isize, d: isize) -> isize {
        let m = Msg { buffer: a, data: c as *const u8, length: d.max(0) as usize };
        match call {
            EeWonLanCall::Control => self.control(c),
            EeWonLanCall::ComponentEvent => self.component_event(c, a),
            EeWonLanCall::HostGame => return self.host_game(a) as isize,
            EeWonLanCall::JoinGame => return self.join_game(a) as isize,
            EeWonLanCall::SendToCaptain => self.send_message_to_captain(Msg::built(&Self::staging(m))),
            EeWonLanCall::BroadcastToGame => self.broadcast_message(Msg::built(&Self::staging(m)), false),
            EeWonLanCall::SendToClient => self.send_message_to_client(b, Msg::built(&Self::staging(m))),
            EeWonLanCall::AdvertiseGame => self.advertise_game(a),
            EeWonLanCall::RemoveGameAdvertisement => self.remove_game_advertisement(a),
            EeWonLanCall::KickClient => self.kick_client(b),
            EeWonLanCall::LeaveGame => self.close_connections(),
            EeWonLanCall::Logout => self.logout(),
            EeWonLanCall::SocketClosed => self.handle_socket_closed(a),
            EeWonLanCall::NetworkTimer => self.handle_network_timer(a),
            EeWonLanCall::RecvMsg => self.handle_recv_msg(a),
            EeWonLanCall::DiscoveryTimer => self.handle_discovery_timer(a),
            EeWonLanCall::DiscoveryRecv => self.handle_discovery_recv(a),
            EeWonLanCall::Accept => self.handle_accept(a),
        }
        0
    }
}

/// A received message: its C++ ByteBuffer (none when NULL) over the bytes taken from it.
fn received(buffer: isize, bytes: &[u8]) -> Msg {
    if buffer == 0 {
        Msg { buffer: null_mut(), data: std::ptr::null(), length: 0 }
    } else {
        Msg { buffer: buffer as *mut c_void, data: non_null(bytes), length: bytes.len() }
    }
}

/// A message over Rust bytes that is present even when empty.
fn built_or_empty(bytes: &[u8]) -> Msg {
    Msg { buffer: null_mut(), data: non_null(bytes), length: bytes.len() }
}

fn non_null(bytes: &[u8]) -> *const u8 {
    static EMPTY: u8 = 0;
    if bytes.is_empty() {
        &EMPTY
    } else {
        bytes.as_ptr()
    }
}

/// A message's bytes.
///
/// # Safety
/// `m.data` holds `m.length` bytes (or is NULL).
unsafe fn message_bytes<'a>(m: &Msg) -> &'a [u8] {
    if m.data.is_null() || m.length == 0 {
        &[]
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(m.data, m.length) }
    }
}

// ---- the C ABI (WONLobbyLan.h)

/// A new LanLogic state for the C++ shell `logic`, calling out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_lan_new(logic: *mut c_void, host: EeWonLobbyHostFn) -> *mut EeWonLan {
    Box::into_raw(Box::new(EeWonLan::new(logic, host)))
}

/// # Safety
/// `lan` came from `ee_won_lan_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_free(lan: *mut EeWonLan) {
    if !lan.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(lan) });
    }
}

/// One of LanLogic's methods (`EeWonLanCall`).
///
/// # Safety
/// `lan` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lan_call(
    lan: *const EeWonLan,
    call: EeWonLanCall,
    a: *mut c_void,
    b: *mut c_void,
    c: isize,
    d: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*lan).call(call, a, b, c, d) }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;
    use std::collections::VecDeque;

    // the golden: won_lanobj_test's traces of the original LanLogic.cpp over its scripted host
    const GOLDEN: &str = include_str!("golden_lanobj.txt");

    thread_local! {
        static EXPECT: RefCell<VecDeque<String>> = const { RefCell::new(VecDeque::new()) };
        static MISMATCH: RefCell<Option<String>> = const { RefCell::new(None) };
    }

    fn hex(data: *const u8, length: usize) -> String {
        if data.is_null() {
            return "-".into();
        }
        // SAFETY: the bytes the object passed
        let bytes = unsafe { std::slice::from_raw_parts(data, length) };
        bytes.iter().map(|b| format!("{b:02x}")).collect()
    }

    fn unhex(s: &str) -> Vec<u8> {
        (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
    }

    fn is_take(op: Op) -> bool {
        matches!(
            op,
            Op::LanRecvMsgTake
                | Op::LanDiscoveryTake
                | Op::LanRecvReplyTake
                | Op::ClientNameTake
                | Op::ParserTextTake
                | Op::LanNameTake
                | Op::LanJoinPasswordTake
        )
    }

    fn is_message(op: Op) -> bool {
        matches!(
            op,
            Op::LanSocketSend
                | Op::LanClientSocketSend
                | Op::LanServerSend
                | Op::LanStagingRead
                | Op::LanDiscoverySend
                | Op::LanSendSummary
        )
    }

    fn is_text(op: Op) -> bool {
        matches!(op, Op::LanNewClient | Op::LanGamePasswordIs | Op::LanRecvChat)
    }

    unsafe extern "C" fn replay(
        _logic: *mut c_void,
        op: Op,
        a: *mut c_void,
        b: *mut c_void,
        c: isize,
        d: isize,
    ) -> isize {
        let want = EXPECT.with(|e| e.borrow_mut().pop_front()).unwrap_or_default();
        let (head, tail) = want.split_once(" -> ").unwrap_or((&want, "0"));
        let (result, taken) = tail.split_once(" t:").unwrap_or((tail, ""));
        let got = if is_take(op) {
            let bytes = unhex(taken);
            // SAFETY: a take op's sink
            unsafe { crate::lobbyhost::ee_won_lobby_sink(b, bytes.as_ptr(), bytes.len()) };
            format!("{} {:x} s {} {}", op as u32, a as usize, c, d)
        } else if is_message(op) {
            format!("{} {:x} {:x} m:{}", op as u32, a as usize, b as usize, hex(c as *const u8, d as usize))
        } else if is_text(op) {
            let count = (d & 0xffff_ffff) as usize;
            format!("{} {:x} {:x} u:{} {}", op as u32, a as usize, b as usize, hex(c as *const u8, count * 2), d >> 32)
        } else {
            format!("{} {:x} {:x} {} {}", op as u32, a as usize, b as usize, c, d)
        };
        // a text op with no units prints its pointer's bytes as none: compare the rest
        let same = head == got || (is_text(op) && head.replace("u:-", "u:") == got.replace("u:-", "u:"));
        if !same {
            MISMATCH.with(|m| {
                m.borrow_mut().get_or_insert_with(|| format!("want {want} got {got}"));
            });
        }
        result.parse().unwrap_or(0)
    }

    fn call_of(n: u32) -> EeWonLanCall {
        use EeWonLanCall::*;
        const ALL: [EeWonLanCall; 18] = [
            Control,
            ComponentEvent,
            HostGame,
            JoinGame,
            SendToCaptain,
            BroadcastToGame,
            SendToClient,
            AdvertiseGame,
            RemoveGameAdvertisement,
            KickClient,
            LeaveGame,
            Logout,
            SocketClosed,
            NetworkTimer,
            RecvMsg,
            DiscoveryTimer,
            DiscoveryRecv,
            Accept,
        ];
        ALL[(n - 1) as usize]
    }

    #[test]
    fn the_object_makes_the_original_calls_in_order() {
        let (mut sequences, mut calls, mut ops) = (0, 0, 0);
        for block in GOLDEN.split("## ").filter(|b| !b.is_empty()) {
            let mut lines = block.lines();
            let name = lines.next().unwrap();
            let lan = EeWonLan::new(0x52 as *mut c_void, Some(replay));
            while let Some(line) = lines.next() {
                let rest = line.strip_prefix("== ").expect("a call line");
                let (head, message) = rest.split_once(" m:").unwrap();
                let f: Vec<&str> = head.split(' ').collect();
                let call = call_of(f[0].parse().unwrap());
                let a = usize::from_str_radix(f[1], 16).unwrap() as *mut c_void;
                let b = usize::from_str_radix(f[2], 16).unwrap() as *mut c_void;
                let mut c: isize = f[3].parse().unwrap();
                let bytes = if message == "-" { None } else { Some(unhex(message)) };
                let mut d = 0;
                let empty = [0u8; 1];
                if let Some(v) = &bytes {
                    c = if v.is_empty() { empty.as_ptr() as isize } else { v.as_ptr() as isize };
                    d = v.len() as isize;
                }
                let mut expect = VecDeque::new();
                let mut result = 0;
                for l in lines.by_ref() {
                    if let Some(r) = l.strip_prefix("=> ") {
                        result = r.parse().unwrap();
                        break;
                    }
                    expect.push_back(l.to_string());
                }
                ops += expect.len();
                EXPECT.with(|e| *e.borrow_mut() = expect);
                // SAFETY: the message's bytes live through the call
                let got = unsafe { lan.call(call, a, b, c, d) };
                let left = EXPECT.with(|e| e.borrow().len());
                let mismatch = MISMATCH.with(|m| m.borrow_mut().take());
                assert_eq!(mismatch, None, "sequence {name}: {line}");
                assert_eq!(left, 0, "sequence {name}: {line}: {left} ops not made");
                assert_eq!(got, result, "sequence {name}: {line}");
                calls += 1;
            }
            sequences += 1;
        }
        assert!(sequences >= 100 && calls >= 800 && ops >= 3000, "{sequences} {calls} {ops}");
    }

    #[test]
    fn units_are_utf16_le() {
        assert_eq!(units(&[0x61, 0, 0x3a, 0x26, 7]), vec![0x61, 0x263a]);
        let p = ee_won_lan_new(null_mut(), None);
        // SAFETY: from ee_won_lan_new; no host: every op answers 0
        unsafe {
            assert_eq!(ee_won_lan_call(p, EeWonLanCall::HostGame, null_mut(), null_mut(), 0, 0), 0);
            ee_won_lan_free(p);
        }
    }
}
