//! StagingLogic as a Rust object (R6 WON round 20): the lobby's game room, hosting, joining and leaving games, the
//! ready flags, pings, kicks and the start message. StagingLogic.cpp's statements in order; its scalar state
//! (the join status, the staging and ready flags, the last all-ready time) lives here, its SmartPtrs (the game,
//! our client, the join op, the create-game dialog) stay in the C++ shell (`WONLobbyStaging.h`), which the lobby's
//! other code reads. Every C++ call goes through the one host table (`lobbyhost`).
//!
//! Re-entry: the modal dialogs and the join's status wait run the lobby, whose messages come back into this object
//! (`HandleMessage` finishing the join): the state is `Cell`s behind a shared reference, read again after every
//! call out.

use std::cell::Cell;
use std::ffi::c_void;
use std::ptr::null_mut;

use crate::flow;
use crate::lobbyhost::{EeWonLobbyHostFn, EeWonLobbyOp as Op, EeWonLobbyResult, EeWonLobbySound, EeWonLobbyText};
use crate::lobbyhost::{HeldGame, Host, Msg};
use crate::staging::{self, RoomMsg};

/// The dialogs' OK / Yes (100001).
const DIALOG_OK: isize = 0x186a1;
/// The lobby events the room broadcasts: entered, left, changed.
const LOBBY_EVENT_ENTERED_STAGING: isize = 0x186bd;
const LOBBY_EVENT_LEFT_STAGING: isize = 0x186c0;
const LOBBY_EVENT_STAGING_CHANGED: isize = 0x186dd;
/// The component event types it handles (LobbyEvent_JoinGame, LobbyEvent_GameAction).
const EVENT_JOIN_GAME: isize = 100027;
const EVENT_GAME_ACTION: isize = 100017;

/// A read buffer's `ReadBufferException` (the original's `HandleMessage` caught it and did nothing more).
struct Threw;

fn threw(r: isize) -> Result<isize, Threw> {
    if r == EeWonLobbyResult::Threw as isize {
        Err(Threw)
    } else {
        Ok(r)
    }
}

/// The inward calls (StagingLogic's methods the lobby calls). `a`, `b` pointers, `c`, `d` integers; a message is
/// `a` its `ByteBuffer*`, `c` its bytes, `d` their length.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonStagingCall {
    /// The host (1), leave (2), logout (3) and start/ready (4) buttons (`c`).
    Control = 1,
    /// A component event: `c` its type, `a` the event.
    ComponentEvent,
    /// `HandleMessage(*a, b)`: `c` the bytes from the buffer's position, `d` their length.
    HandleMessage,
    SendToCaptain,
    BroadcastToGame,
    /// `SendToClient(b, message)`.
    SendToClient,
    IAmCaptain,
    IAmReady,
    JoinGame,
    HostGame,
    Logout,
    /// `KickClient(b, c)`.
    KickClient,
    HandleDisconnect,
    /// `HandleClientLeft(b)`.
    HandleClientLeft,
    /// `HandleKicked(b, c)`.
    HandleKicked,
    ResetReadyFlags,
    SetGameInProgress,
    LeaveGame,
    NotifyStagingChange,
    NotifyPingChange,
}

/// StagingLogic's state (the original's members 0x4c..0x54).
pub struct EeWonStaging {
    host: Host,
    join_status: Cell<i16>,
    in_staging: Cell<bool>,
    last_ready: Cell<bool>,
    last_all_ready: Cell<bool>,
    last_ready_time: Cell<i64>,
}

/// Leaves the half-joined or half-hosted game when dropped unless disarmed (the original's StagingLeaveOnError:
/// `LobbyLeaveGame(false)`, this object's `LeaveGame`).
struct LeaveOnError<'a> {
    staging: &'a EeWonStaging,
    leave: bool,
}

impl Drop for LeaveOnError<'_> {
    fn drop(&mut self) {
        if self.leave {
            self.staging.leave_game(false);
        }
    }
}

fn room(kind: u8, id: u16, flag: bool, ping: u16) -> Vec<u8> {
    staging::room_message(&RoomMsg { kind, id, flag, ping, rest: 0 })
}

impl EeWonStaging {
    pub fn new(logic: *mut c_void, call: EeWonLobbyHostFn) -> Self {
        EeWonStaging {
            host: Host { logic, call },
            join_status: Cell::new(0),
            in_staging: Cell::new(false),
            last_ready: Cell::new(false),
            last_all_ready: Cell::new(false),
            last_ready_time: Cell::new(0),
        }
    }

    fn h(&self) -> &Host {
        &self.host
    }
    fn game(&self) -> *mut c_void {
        self.h().get(Op::StGame) as *mut c_void
    }
    fn my_client(&self) -> *mut c_void {
        self.h().get(Op::StMyClient) as *mut c_void
    }
    fn find_client(&self, id: u16) -> *mut c_void {
        let g = self.game();
        self.h().op(Op::GameFindClient, g, null_mut(), id as isize, 0) as *mut c_void
    }
    fn client_id(&self, client: *mut c_void) -> u16 {
        self.h().on(Op::ClientId, client) as u16
    }
    fn client_event(&self, op: Op, client: *mut c_void, kind: isize) {
        self.h().op(op, client, null_mut(), kind, 0);
    }
    fn teammate_of_me(&self, client: *mut c_void) -> bool {
        let me = self.my_client();
        self.h().op(Op::ClientIsTeammate, client, me, 0, 0) != 0
    }
    fn message_box(&self, text: EeWonLobbyText) {
        self.h().int(Op::MessageBox, text as isize);
    }
    fn yes_no(&self, text: EeWonLobbyText) -> isize {
        self.h().int(Op::YesNo, text as isize)
    }
    fn lobby_event(&self, kind: isize) {
        self.h().int(Op::BroadcastLobbyEvent, kind);
    }
    fn show_status(&self, game: *mut c_void, status: i16) {
        self.h().op(Op::GameShowStatus, game, null_mut(), status as isize, 0);
    }

    /// `IAmCaptain`: we host the current game.
    pub fn i_am_captain(&self) -> bool {
        let g = self.game();
        !g.is_null() && self.h().on_bool(Op::GameIsHost, g)
    }

    /// `IAmReady`: our own player is ready.
    pub fn i_am_ready(&self) -> bool {
        let c = self.my_client();
        !c.is_null() && self.h().on_bool(Op::ClientIsReady, c)
    }

    /// Host: answers a client's request to join and announces the new player to the game, then re-advertises it.
    fn handle_join_request(&self, buffer: *mut c_void, client: *mut c_void) -> Result<(), Threw> {
        let g = self.game();
        if threw(self.h().op(Op::GameJoinReplySend, g, buffer, 0, client as isize))? == 0 {
            return Ok(());
        }
        if !self.h().on_ptr(Op::ClientPlayer, client).is_null() {
            self.h().on(Op::ClientBroadcastJoined, client);
            self.h().get(Op::NetAdvertiseGame);
        }
        Ok(())
    }

    /// Takes the host's answer to our join request and finishes the waiting join.
    fn handle_join_reply(&self, buffer: *mut c_void) -> Result<(), Threw> {
        let g = self.game();
        let status = threw(self.h().op(Op::GameHandleJoinReply, g, buffer, 0, 0))?;
        self.join_status.set(status as i16);
        self.h().get(Op::StJoinOpFinish);
        Ok(())
    }

    /// Adds a player announced by the host to its client, reports the change and plays the joined sound.
    fn handle_player_joined(&self, m: &RoomMsg, buffer: *mut c_void) -> Result<(), Threw> {
        let client = self.find_client(m.id);
        if client.is_null() {
            return Ok(());
        }
        let has_player = self.h().on_bool(Op::ClientHasPlayer, client);
        let captain = self.h().on_bool(Op::ClientHasPlayer, client) && self.i_am_captain();
        match flow::player_joined(has_player, captain) {
            0 => {
                if threw(self.h().op(Op::ClientReadPlayer, client, buffer, 0, 0))? == 0 {
                    return Ok(());
                }
            }
            2 => return Ok(()),
            _ => {}
        }
        self.client_event(Op::ClientChanged, client, 0);
        if self.teammate_of_me(client) {
            self.client_event(Op::ClientTeamChanged, client, 0);
        }
        let g = self.game();
        self.h().op(Op::GameHandlePlayerJoined, g, client, 0, 0);
        if self.h().get(Op::StagingSoundEffects) != 0 {
            self.h().int(Op::PlaySound, EeWonLobbySound::PlayerJoined as isize);
        }
        if self.last_all_ready.get() {
            let ready = self.h().on_bool(Op::ClientIsReady, client);
            self.last_all_ready.set(flow::joined_keeps_all_ready(true, ready));
        }
        self.notify_staging_change();
        Ok(())
    }

    /// Host: broadcasts a player's changed ready flag to the game.
    fn handle_ready_request(&self, m: &RoomMsg, client: *mut c_void) {
        let id = self.client_id(client);
        self.broadcast_to_game(Msg::built(&room(staging::READY_CHANGED, id, m.flag, 0)));
    }

    /// Sets a player's ready flag as announced by the host and updates the all-ready state.
    fn handle_ready_changed(&self, m: &RoomMsg) {
        let client = self.find_client(m.id);
        if client.is_null() || self.h().on_ptr(Op::ClientPlayer, client).is_null() {
            return;
        }
        self.h().op(Op::ClientPlayerSetReady, client, null_mut(), m.flag as isize, 0);
        self.update_ready_state();
        self.client_event(Op::ClientChanged, client, 2);
    }

    /// Host: broadcasts the ping a player reported to the game.
    fn handle_ping_report(&self, m: &RoomMsg, client: *mut c_void) {
        let id = self.client_id(client);
        self.broadcast_to_game(Msg::built(&room(staging::PING_CHANGED, id, false, m.ping)));
    }

    /// Sets a player's ping as announced by the host.
    fn handle_ping_changed(&self, m: &RoomMsg) {
        let client = self.find_client(m.id);
        if !client.is_null() && !self.h().on_ptr(Op::ClientPlayer, client).is_null() {
            self.h().op(Op::ClientPlayerSetPing, client, null_mut(), m.ping as isize, 0);
            self.client_event(Op::ClientChanged, client, 2);
        }
    }

    /// Leaves the game and tells the player when the host dissolved it.
    fn handle_game_dissolved(&self) {
        self.leave_game(false);
        self.message_box(EeWonLobbyText::GameDissolved);
    }

    /// Clears every player's ready flag in the game and updates the all-ready state.
    pub fn reset_ready_flags(&self) {
        let g = self.game();
        if g.is_null() {
            return;
        }
        self.h().on(Op::GameResetReadyFlags, g);
        self.update_ready_state();
    }

    /// Host: marks the game as in progress or not and re-advertises it.
    pub fn set_game_in_progress(&self, in_progress: bool) {
        let g = self.game();
        if g.is_null() {
            return;
        }
        let current = self.h().on_bool(Op::GameIsInProgress, g);
        if !flow::sets_in_progress(true, current, in_progress, self.i_am_captain()) {
            return;
        }
        self.h().op(Op::GameSetInProgressNet, g, null_mut(), in_progress as isize, 0);
    }

    /// Leaves the lobby to start the game when the host says so; the host also marks the game in progress.
    fn handle_start_game(&self) {
        let captain = self.i_am_captain();
        if captain {
            self.set_game_in_progress(true);
        }
        self.h().int(Op::Exit, staging::start_exit_code(captain) as isize);
    }

    /// Handles the host's announcement that a client was kicked or banned.
    fn handle_client_kicked(&self, m: &RoomMsg) {
        let client = self.find_client(m.id);
        if !client.is_null() {
            self.handle_kicked(client, m.flag);
        }
    }

    /// Dispatches a game room message by type, passing unknown types to the game; while joining only the reply
    /// counts.
    ///
    /// # Safety
    /// `data` holds `length` bytes.
    pub unsafe fn handle_message(&self, buffer: *mut c_void, client: *mut c_void, data: *const u8, length: usize) {
        if self.game().is_null() || client.is_null() {
            return;
        }
        let read = {
            let bytes: &[u8] = if data.is_null() || length == 0 {
                &[]
            } else {
                // SAFETY: the contract; the slice ends before any call out
                unsafe { std::slice::from_raw_parts(data, length) }
            };
            staging::read_room(bytes, self.join_status.get() == 1)
        };
        let m = match read {
            Ok(Some(m)) => m,
            _ => return,
        };
        self.h().op(Op::BufSkip, null_mut(), buffer, m.rest as isize, 0);
        let roles = staging::Roles {
            sender_is_captain: self.h().on_bool(Op::ClientIsGameCaptain, client),
            i_am_captain: self.i_am_captain(),
            sender_has_player: self.h().on_bool(Op::ClientHasPlayer, client),
            sender_ready: self.h().on_bool(Op::ClientIsReady, client),
        };
        if !staging::room_accepts(&m, &roles) {
            return;
        }
        // a handler whose read threw (Err) stops there, as the original's catch did
        match m.kind {
            staging::JOIN_REQUEST => {
                let _ = self.handle_join_request(buffer, client);
            }
            staging::JOIN_REPLY => {
                let _ = self.handle_join_reply(buffer);
            }
            staging::PLAYER_JOINED => {
                let _ = self.handle_player_joined(&m, buffer);
            }
            staging::READY_REQUEST => self.handle_ready_request(&m, client),
            staging::READY_CHANGED => self.handle_ready_changed(&m),
            staging::GAME_DISSOLVED => self.handle_game_dissolved(),
            staging::START_GAME => self.handle_start_game(),
            staging::PING_REPORT => self.handle_ping_report(&m, client),
            staging::PING_CHANGED => self.handle_ping_changed(&m),
            staging::CLIENT_KICKED => self.handle_client_kicked(&m),
            kind => {
                let g = self.game();
                let _ = threw(self.h().op(Op::GameHandleGameMessage, g, buffer, kind as isize, client as isize));
            }
        }
    }

    /// Handles a game room message held in a byte buffer.
    fn handle_message_buffer(&self, m: Msg, client: *mut c_void) {
        if !m.is_none() {
            self.h().msg(Op::HandleBytes, m, client);
        }
    }

    /// Sends a message to the host; the host handles its own message directly.
    pub fn send_to_captain(&self, m: Msg) {
        let have = !self.game().is_null();
        match flow::send_to_captain(have, self.i_am_captain()) {
            flow::EeWonStageRoute::Local => self.handle_message_buffer(m, self.my_client()),
            flow::EeWonStageRoute::Net => {
                self.h().msg(Op::NetSendToCaptain, m, null_mut());
            }
            _ => {}
        }
    }

    /// Host: handles a message itself and sends it to every client in the game.
    pub fn broadcast_to_game(&self, m: Msg) {
        let have = !self.game().is_null();
        if flow::broadcast(have, self.i_am_captain()) == flow::EeWonStageRoute::LocalAndNet {
            self.handle_message_buffer(m, self.my_client());
            self.h().msg(Op::NetBroadcastToGame, m, null_mut());
        }
    }

    /// Host: sends a message to one client in the game.
    pub fn send_to_client(&self, client: *mut c_void, m: Msg) {
        let have = !self.game().is_null();
        if flow::send_to_client(have, self.i_am_captain()) == flow::EeWonStageRoute::Net {
            self.h().msg(Op::NetSendToClient, m, client);
        }
    }

    /// Runs the host, leave, logout and start/ready buttons.
    pub fn control(&self, which: isize) {
        match which {
            1 => self.host_game(),
            2 => {
                self.leave_game(true);
            }
            3 => {
                self.logout(true);
            }
            4 => self.toggle_ready(),
            _ => {}
        }
    }

    /// Handles join-game and game-action events from the game lists.
    pub fn component_event(&self, kind: isize, event: *mut c_void) {
        match kind {
            EVENT_JOIN_GAME => {
                let g = self.h().on_ptr(Op::EventJoinGame, event);
                self.join_game(g);
            }
            EVENT_GAME_ACTION => self.handle_game_action(event),
            _ => {}
        }
    }

    /// Runs a game list action: options, join, ping, show or hide details, direct connect.
    fn handle_game_action(&self, event: *mut c_void) {
        let g = self.h().on_ptr(Op::EventActionGame, event);
        if g.is_null() {
            return;
        }
        let action = self.h().on(Op::EventAction, event) as i32;
        let kind = self.h().on(Op::GameType, g) as i32;
        match flow::game_action(action, kind) {
            7 => {
                self.h().on(Op::GameShowOptions, g);
            }
            1 => self.join_game(g),
            2 => {
                self.h().on(Op::GamePingAndNotify, g);
            }
            3 => {
                self.h().on(Op::GameShowDetails, g);
            }
            5 => {
                self.h().on(Op::GameHideDetails, g);
            }
            6 => {
                self.h().on(Op::GameDirectConnect, g);
            }
            _ => {}
        }
    }

    /// Leaves the game (confirming if asked), logs out, saves the settings and returns to the login screen.
    pub fn logout(&self, confirm: bool) -> bool {
        if !self.leave_game(confirm) {
            return false;
        }
        self.h().get(Op::StLogoutTail);
        true
    }

    /// Asks the host to set our ready flag.
    fn send_ready(&self, ready: bool) {
        self.send_to_captain(Msg::built(&room(staging::READY_REQUEST, 0, ready, 0)));
    }

    /// Host: tells every client in the game that it is dissolved.
    fn send_game_dissolved(&self) {
        if !self.i_am_captain() {
            return;
        }
        self.broadcast_to_game(Msg::built(&room(staging::GAME_DISSOLVED, 0, false, 0)));
    }

    /// Tells every client in the game to start it.
    fn send_start_game(&self) {
        self.broadcast_to_game(Msg::built(&room(staging::START_GAME, 0, false, 0)));
    }

    /// Host: starts the game when it can; others toggle their ready flag if the game allows it.
    fn toggle_ready(&self) {
        let have = !self.my_client().is_null() && !self.game().is_null();
        let captain = have && self.i_am_captain();
        let ready = have && {
            let c = self.my_client();
            self.h().on_bool(Op::ClientIsReady, c)
        };
        let t = flow::toggle_ready(have, have, captain, ready);
        if t.action == 1 {
            let g = self.game();
            if self.h().on_bool(Op::GameCanStartAndTry, g) {
                self.send_start_game();
            }
        } else if t.action == 2 {
            if t.ask_can_ready {
                let g = self.game();
                if !self.h().on_bool(Op::GameCanReady, g) {
                    return;
                }
            }
            self.last_ready.set(t.want_ready);
            self.send_ready(t.want_ready);
        }
    }

    /// Hosts a new game from the create-game dialog after firewall warnings, entering its game room.
    pub fn host_game(&self) {
        if !self.leave_game(true) {
            return;
        }
        let kind = self.h().get(Op::GameScreenType);
        let confirm = kind != 0 && kind != 1 && self.h().get(Op::ShowConfirmations) != 0;
        let done = confirm && self.h().get(Op::FirewallTestDone) != 0;
        let behind = done && self.h().get(Op::BehindFirewall) != 0;
        let warning = flow::host_warning(kind as i32, confirm, done, behind);
        if warning < 0 {
            return;
        }
        if warning != 0 {
            let text = if warning == 1 { EeWonLobbyText::ConnectWarning1 } else { EeWonLobbyText::ConnectWarning2 };
            if self.yes_no(text) != DIALOG_OK {
                return;
            }
        }
        if self.h().int(Op::StCreateDialogRun, kind) != DIALOG_OK {
            return;
        }
        let game = HeldGame::new(*self.h(), self.h().int(Op::StTakeDialogGame, kind) as *mut c_void);
        let g = game.game;
        self.join_status.set(0);
        let mut cleanup = LeaveOnError { staging: self, leave: true };
        if !self.h().on_bool(Op::GamePreNetCreate, g) || !self.h().on_bool(Op::NetHostGame, g) {
            return;
        }
        let me = self.h().get(Op::StSetMyClientFromNet) as *mut c_void;
        if me.is_null() {
            return;
        }
        self.h().op(Op::GameBecomeHost, g, me, 0, 0);
        if !self.h().on_bool(Op::GamePostNetCreate, g) {
            return;
        }
        let me = self.my_client();
        self.h().op(Op::GameAddHostClient, g, me, 0, 0);
        cleanup.leave = false;
        self.in_staging.set(true);
        self.last_ready_time.set(0);
        self.last_all_ready.set(self.h().on_bool(Op::GameAreAllReady, g));
        self.last_ready.set(self.i_am_ready());
        self.h().get(Op::NetAdvertiseGame);
        self.lobby_event(LOBBY_EVENT_ENTERED_STAGING);
    }

    /// For an Internet game not yet pinged, waits for a ping reply or asks to join anyway; whether to join.
    fn verify_ping_before_join(&self, game: *mut c_void) -> bool {
        if self.h().on(Op::GameType, game) != 2 || self.h().on_bool(Op::GamePingReplied, game) {
            return true;
        }
        let allow = self.h().get(Op::AllowJoinWithoutPing);
        let wait = allow >= 0 && allow == 0;
        let kind = self.h().on(Op::GameType, game) as i32;
        let replied = self.h().on_bool(Op::GamePingReplied, game);
        let show = !wait && self.h().get(Op::ShowConfirmations) != 0;
        match flow::verify_ping(kind, replied, allow >= 0, allow == 1, show) {
            1 => {
                self.h().on(Op::GamePingAndNotify, game);
                while self.h().on_bool(Op::GameIsPinging, game) {
                    let text = EeWonLobbyText::VerifyingPing as isize;
                    if self.h().op(Op::RunWaitOp, null_mut(), null_mut(), text, 2) != 0 {
                        return false;
                    }
                }
                if !self.h().on_bool(Op::GamePingReplied, game) {
                    self.message_box(EeWonLobbyText::NoPingReply);
                    return false;
                }
            }
            2 => return self.yes_no(EeWonLobbyText::ConnectWarning3) == DIALOG_OK,
            _ => {}
        }
        true
    }

    /// Joins a game from the list: checks ping and status, connects, waits for the host's reply and enters the room.
    pub fn join_game(&self, in_game: *mut c_void) {
        if in_game.is_null() {
            return;
        }
        let current = self.game();
        if !current.is_null() && self.h().op(Op::GameIsSameGame, in_game, current, 0, 0) != 0 {
            self.message_box(EeWonLobbyText::AlreadyJoined);
            return;
        }
        if !self.verify_ping_before_join(in_game) {
            return;
        }
        let status = self.h().on(Op::GameJoinStatus, in_game) as i16;
        if status != 0 {
            if flow::join_status_shown(status) {
                self.show_status(in_game, status);
            }
            return;
        }
        if !self.leave_game(true) {
            return;
        }
        let mut cleanup = LeaveOnError { staging: self, leave: true };
        let game = HeldGame::new(*self.h(), self.h().on_ptr(Op::GameNewCopy, in_game));
        let g = game.game;
        self.h().on(Op::StSetGame, g);
        self.join_status.set(1);
        if !self.h().on_bool(Op::GamePreNetJoin, g) || !self.h().on_bool(Op::NetJoinGame, g) {
            return;
        }
        if self.h().get(Op::StSetMyClientFromNet) == 0 || !self.h().on_bool(Op::GamePostNetJoin, g) {
            return;
        }
        self.h().get(Op::StJoinOpNew);
        self.h().get(Op::StSendJoinRequest);
        self.h().get(Op::StJoinOpRun);
        let status = self.join_status.get();
        if status != 0 {
            if flow::join_status_shown(status) {
                self.show_status(in_game, status);
            }
            return;
        }
        cleanup.leave = false;
        self.in_staging.set(true);
        self.last_ready_time.set(0);
        self.last_all_ready.set(self.h().on_bool(Op::GameAreAllReady, g));
        self.last_ready.set(self.i_am_ready());
        self.lobby_event(LOBBY_EVENT_ENTERED_STAGING);
        drop(game);
    }

    /// Leaves or dissolves the current game, confirming if asked; false only when the player cancels.
    pub fn leave_game(&self, confirm: bool) -> bool {
        if self.game().is_null() {
            return true;
        }
        let show = confirm && self.h().get(Op::ShowConfirmations) != 0;
        let ask = flow::leave_confirm(confirm, show, self.i_am_captain());
        if ask != 0 {
            let text = if ask == 1 { EeWonLobbyText::DissolveGame } else { EeWonLobbyText::LeaveGame };
            if self.yes_no(text) != DIALOG_OK {
                return false;
            }
        }
        if self.game().is_null() {
            return true;
        }
        if self.i_am_captain() {
            self.send_game_dissolved();
            let g = self.game();
            self.h().on(Op::NetRemoveGameAdvertisement, g);
        }
        if self.game().is_null() {
            return true;
        }
        self.h().get(Op::StJoinOpKill);
        let g = self.game();
        let game = HeldGame::new(*self.h(), self.h().on_ptr(Op::GameHold, g));
        self.h().on(Op::StSetGame, null_mut());
        self.h().on(Op::GameLeaveNet, game.game);
        self.last_ready_time.set(0);
        if self.in_staging.get() {
            self.lobby_event(LOBBY_EVENT_LEFT_STAGING);
            self.in_staging.set(false);
        }
        drop(game);
        true
    }

    /// Leaves the game after losing the connection, optionally telling the player.
    pub fn handle_disconnect(&self, show_error: bool) {
        if !self.game().is_null() {
            self.leave_game(false);
            if show_error {
                self.message_box(EeWonLobbyText::Disconnected);
            }
        }
    }

    /// Reports a kick or ban from the game; when it is us, leaves the game and tells the player.
    pub fn handle_kicked(&self, client: *mut c_void, ban: bool) {
        let g = self.game();
        if client.is_null() || g.is_null() {
            return;
        }
        let is_me = client == self.my_client();
        let kind = self.h().on(Op::GameType, g) as i32;
        let k = flow::kicked(ban, kind, is_me);
        let kind_and_me = ((k.kind as isize) << 1) | is_me as isize;
        self.h().op(Op::StClientModerated, client, null_mut(), k.flags as isize, kind_and_me);
        if k.message == 0 {
            return;
        }
        self.leave_game(false);
        self.message_box(if k.message == 1 { EeWonLobbyText::Banned } else { EeWonLobbyText::Kicked });
    }

    /// Removes a departed player from the game, reports it and re-advertises the game when hosting.
    pub fn handle_client_left(&self, client: *mut c_void) {
        let has_player = self.h().on_bool(Op::ClientHasPlayer, client);
        if !flow::client_left(has_player, !self.game().is_null()) {
            return;
        }
        self.client_event(Op::ClientChanged, client, 1);
        if self.teammate_of_me(client) {
            self.client_event(Op::ClientTeamChanged, client, 1);
        }
        let g = self.game();
        self.h().op(Op::GameRemoveClient, g, client, 0, 0);
        if self.i_am_captain() {
            self.h().get(Op::NetAdvertiseGame);
        }
        self.notify_staging_change();
    }

    /// Reports our new ping to the current game's host.
    pub fn notify_ping_change(&self, game: *mut c_void) {
        let have = !self.game().is_null();
        let captain = have && self.i_am_captain();
        let same = have && !captain && {
            let current = self.game();
            self.h().op(Op::GameIsSameGame, game, current, 0, 0) != 0
        };
        if !flow::reports_ping(have, captain, same) {
            return;
        }
        let ping = self.h().on(Op::GamePing, game) as u16;
        self.send_to_captain(Msg::built(&room(staging::PING_REPORT, 0, false, ping)));
    }

    /// Host: kicks or bans a client from the game and tells everyone.
    pub fn kick_client(&self, client: *mut c_void, ban: bool) {
        let have = !self.game().is_null();
        if !flow::kicks(!client.is_null(), have, self.i_am_captain()) {
            return;
        }
        let id = self.client_id(client);
        self.broadcast_to_game(Msg::built(&room(staging::CLIENT_KICKED, id, ban, 0)));
        self.h().op(Op::NetKickClient, null_mut(), client, ban as isize, 0);
    }

    /// Tells the lobby screens that the game room changed.
    pub fn notify_staging_change(&self) {
        self.lobby_event(LOBBY_EVENT_STAGING_CHANGED);
    }

    /// Plays the all-ready sound (at most every 15 s) or the not-ready sound when the game's ready state changes.
    fn update_ready_state(&self) {
        self.notify_staging_change();
        let g = self.game();
        if g.is_null() {
            return;
        }
        let all_ready = self.h().on_bool(Op::GameAreAllReady, g);
        let is_host = self.h().on_bool(Op::GameIsHost, g);
        let i_am_ready = self.i_am_ready();
        let now = self.h().get(Op::Now) as i64;
        let o = staging::ready_state(&staging::ReadyInput {
            all_ready,
            last_all_ready: self.last_all_ready.get(),
            now,
            last_ready_time: self.last_ready_time.get(),
            is_host,
            last_ready: self.last_ready.get(),
            i_am_ready,
        });
        self.last_all_ready.set(o.last_all_ready);
        self.last_ready.set(o.last_ready);
        self.last_ready_time.set(o.last_ready_time);
        if o.sound != staging::EeWonReadySound::Silent && self.h().get(Op::StagingSoundEffects) != 0 {
            let sound = if o.sound == staging::EeWonReadySound::AllReady {
                EeWonLobbySound::AllReady
            } else {
                EeWonLobbySound::AllNotReady
            };
            self.h().int(Op::PlaySound, sound as isize);
        }
    }

    /// One inward call.
    ///
    /// # Safety
    /// The call's arguments as `EeWonStagingCall` documents (a message's bytes valid for the call).
    pub unsafe fn call(&self, call: EeWonStagingCall, a: *mut c_void, b: *mut c_void, c: isize, d: isize) -> isize {
        let m = Msg { buffer: a, data: c as *const u8, length: d.max(0) as usize };
        match call {
            EeWonStagingCall::Control => self.control(c),
            EeWonStagingCall::ComponentEvent => self.component_event(c, a),
            // SAFETY: the contract
            EeWonStagingCall::HandleMessage => unsafe { self.handle_message(a, b, m.data, m.length) },
            EeWonStagingCall::SendToCaptain => self.send_to_captain(m),
            EeWonStagingCall::BroadcastToGame => self.broadcast_to_game(m),
            EeWonStagingCall::SendToClient => self.send_to_client(b, m),
            EeWonStagingCall::IAmCaptain => return self.i_am_captain() as isize,
            EeWonStagingCall::IAmReady => return self.i_am_ready() as isize,
            EeWonStagingCall::JoinGame => self.join_game(a),
            EeWonStagingCall::HostGame => self.host_game(),
            EeWonStagingCall::Logout => return self.logout(c != 0) as isize,
            EeWonStagingCall::KickClient => self.kick_client(b, c != 0),
            EeWonStagingCall::HandleDisconnect => self.handle_disconnect(c != 0),
            EeWonStagingCall::HandleClientLeft => self.handle_client_left(b),
            EeWonStagingCall::HandleKicked => self.handle_kicked(b, c != 0),
            EeWonStagingCall::ResetReadyFlags => self.reset_ready_flags(),
            EeWonStagingCall::SetGameInProgress => self.set_game_in_progress(c != 0),
            EeWonStagingCall::LeaveGame => return self.leave_game(c != 0) as isize,
            EeWonStagingCall::NotifyStagingChange => self.notify_staging_change(),
            EeWonStagingCall::NotifyPingChange => self.notify_ping_change(a),
        }
        0
    }
}

// ---- the C ABI (WONLobbyStaging.h)

/// A new StagingLogic state for the C++ shell `logic`, calling out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_staging_new(logic: *mut c_void, host: EeWonLobbyHostFn) -> *mut EeWonStaging {
    Box::into_raw(Box::new(EeWonStaging::new(logic, host)))
}

/// # Safety
/// `staging` came from `ee_won_staging_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_staging_free(staging: *mut EeWonStaging) {
    if !staging.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(staging) });
    }
}

/// One of StagingLogic's methods (`EeWonStagingCall`).
///
/// # Safety
/// `staging` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_staging_call(
    staging: *const EeWonStaging,
    call: EeWonStagingCall,
    a: *mut c_void,
    b: *mut c_void,
    c: isize,
    d: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*staging).call(call, a, b, c, d) }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;
    use std::collections::VecDeque;

    // the golden: won_stagingobj_test's traces of the original StagingLogic.cpp over its scripted host
    const GOLDEN: &str = include_str!("golden_stagingobj.txt");

    thread_local! {
        static EXPECT: RefCell<VecDeque<String>> = const { RefCell::new(VecDeque::new()) };
        static MISMATCH: RefCell<Option<String>> = const { RefCell::new(None) };
    }

    fn hex(data: *const u8, length: isize) -> String {
        if data.is_null() {
            return "-".into();
        }
        // SAFETY: the bytes the object passed (a Vec it holds or the test's)
        let bytes = unsafe { std::slice::from_raw_parts(data, length as usize) };
        bytes.iter().map(|b| format!("{b:02x}")).collect()
    }

    fn is_message_op(op: Op) -> bool {
        matches!(op, Op::HandleBytes | Op::NetSendToCaptain | Op::NetBroadcastToGame | Op::NetSendToClient)
    }

    unsafe extern "C" fn replay(
        _logic: *mut c_void,
        op: Op,
        a: *mut c_void,
        b: *mut c_void,
        c: isize,
        d: isize,
    ) -> isize {
        let got = if is_message_op(op) {
            format!("{} {:x} {:x} m:{}", op as u32, a as usize, b as usize, hex(c as *const u8, d))
        } else {
            format!("{} {:x} {:x} {} {}", op as u32, a as usize, b as usize, c, d)
        };
        let want = EXPECT.with(|e| e.borrow_mut().pop_front()).unwrap_or_default();
        let (head, result) = want.split_once(" -> ").unwrap_or((&want, "0"));
        if head != got {
            MISMATCH.with(|m| {
                m.borrow_mut().get_or_insert_with(|| format!("want {want} got {got}"));
            });
        }
        result.parse().unwrap_or(0)
    }

    fn call_of(n: u32) -> EeWonStagingCall {
        use EeWonStagingCall::*;
        const ALL: [EeWonStagingCall; 20] = [
            Control,
            ComponentEvent,
            HandleMessage,
            SendToCaptain,
            BroadcastToGame,
            SendToClient,
            IAmCaptain,
            IAmReady,
            JoinGame,
            HostGame,
            Logout,
            KickClient,
            HandleDisconnect,
            HandleClientLeft,
            HandleKicked,
            ResetReadyFlags,
            SetGameInProgress,
            LeaveGame,
            NotifyStagingChange,
            NotifyPingChange,
        ];
        ALL[(n - 1) as usize]
    }

    fn unhex(s: &str) -> Vec<u8> {
        (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
    }

    #[test]
    fn the_object_makes_the_original_calls_in_order() {
        let mut sequences = 0;
        let mut calls = 0;
        let mut ops = 0;
        for block in GOLDEN.split("## ").filter(|b| !b.is_empty()) {
            let mut lines = block.lines();
            let name = lines.next().unwrap();
            let staging = EeWonStaging::new(0x51 as *mut c_void, Some(replay));
            let mut lines = lines.peekable();
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
                let got = unsafe { staging.call(call, a, b, c, d) };
                let left = EXPECT.with(|e| e.borrow().len());
                let mismatch = MISMATCH.with(|m| m.borrow_mut().take());
                assert_eq!(mismatch, None, "sequence {name}: {line}");
                assert_eq!(left, 0, "sequence {name}: {line}: {left} ops not made");
                assert_eq!(got, result, "sequence {name}: {line}");
                calls += 1;
            }
            sequences += 1;
        }
        assert!(sequences >= 100 && calls >= 600 && ops >= 3000, "{sequences} {calls} {ops}");
    }

    unsafe extern "C" fn none(_: *mut c_void, _: Op, _: *mut c_void, _: *mut c_void, _: isize, _: isize) -> isize {
        0
    }

    #[test]
    fn without_a_game_nothing_happens() {
        let s = EeWonStaging::new(null_mut(), Some(none));
        assert!(s.leave_game(true));
        assert!(!s.i_am_captain() && !s.i_am_ready());
        // SAFETY: no message
        assert_eq!(unsafe { s.call(EeWonStagingCall::Logout, null_mut(), null_mut(), 1, 0) }, 1);
        let p = ee_won_staging_new(null_mut(), None);
        // SAFETY: from ee_won_staging_new
        unsafe { ee_won_staging_free(p) };
    }
}
