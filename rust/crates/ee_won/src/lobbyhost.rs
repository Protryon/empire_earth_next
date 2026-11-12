//! The lobby logic objects' one outward call table (R6 WON round 20): every C++ call a Rust lobby logic object
//! makes (the model's objects, the events, the `Lobby*` helpers, the dialogs, the ops) is one op code of a single
//! host function, `ee_won_lobbycpp_call` in `WONLobby/LobbyLogicHost.cpp`. An op is a run of C++ calls with no
//! decision between them (the original's statements in order), so the host stays a switch of short cases.
//!
//! The object pointers the ops take and return (`LobbyGame*`, `LobbyClient*`, `ReadBuffer*`, events) are opaque
//! here: Rust only passes them back. A pointer an op returns "held" carries a reference Rust gives back with
//! `GameRelease` (the original's local `SmartPtr<LobbyGame>`).

use std::ffi::c_void;

/// The host: `(logic, op, a, b, c, d) -> result`. `logic` is the C++ object the Rust one belongs to.
pub type EeWonLobbyHostFn = Option<
    unsafe extern "C" fn(
        logic: *mut c_void,
        op: EeWonLobbyOp,
        a: *mut c_void,
        b: *mut c_void,
        c: isize,
        d: isize,
    ) -> isize,
>;

/// What an op that reads a message returns when the read threw `ReadBufferException` (the original's handler
/// caught it in `HandleMessage` and did nothing more); outside every `short` status.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyResult {
    Threw = 0x10000,
}

/// The string pairs (title, details) of the message boxes, yes/no dialogs and status waits.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyText {
    GameDissolved = 0,
    AlreadyJoined = 1,
    NoPingReply = 2,
    Disconnected = 3,
    Banned = 4,
    Kicked = 5,
    ConnectWarning1 = 6,
    ConnectWarning2 = 7,
    ConnectWarning3 = 8,
    DissolveGame = 9,
    LeaveGame = 10,
    VerifyingPing = 11,
    GettingGameInfo = 12,
    WrongPassword = 13,
    OtherSubnet = 14,
}

/// The lobby sounds an op plays.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbySound {
    PlayerJoined = 0,
    AllReady = 1,
    AllNotReady = 2,
}

/// The op codes. Arguments: `a`, `b` pointers, `c`, `d` integers (a message is `a` = its `ByteBuffer*` or NULL for
/// one Rust built, `c` = its bytes, `d` = their length).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyOp {
    // ---- StagingLogic's members (the C++ object keeps its SmartPtrs)
    /// `mGame.get()`.
    StGame = 1,
    /// `mMyClient.get()`.
    StMyClient,
    /// `mMyClient = LobbyGetNetMyClient()`; returns it.
    StSetMyClientFromNet,
    /// `mGame = a`.
    StSetGame,
    /// `mJoinOp = new AsyncOp`.
    StJoinOpNew,
    /// `if (mJoinOp) mJoinOp->Finish(WS_Success)`.
    StJoinOpFinish,
    /// `LobbyRunOpWithStatus(mJoinOp, GettingGameInfo, 1, 0)`, then `mJoinOp = NULL`.
    StJoinOpRun,
    /// `if (mJoinOp) { mJoinOp->Kill(true); mJoinOp = NULL; }`.
    StJoinOpKill,
    /// `mGame->GetJoinRequest()` sent with `this->SendToCaptain`.
    StSendJoinRequest,
    /// `mCreateGameDialog->SetGameType(c); Reset(false); return LobbyDoDialog(dialog)`.
    StCreateDialogRun,
    /// `aGame = mCreateGameDialog->GetGame(); aGame->SetAddress(LobbyGetMyAddress(c)); mGame = aGame`; held.
    StTakeDialogGame,
    /// Logout's tail: `mCreateGameDialog->Reset(true)`, `LobbyNetLogout`, `LobbyKillPings(0)`,
    /// `LobbyShowLoginScreen`, `LobbySaveGlobalPrefs(false)`, `LobbySaveUserPrefs(false)`, the logon sound stopped.
    StLogoutTail,
    /// `new ClientModeratedEvent(a, c, mGame->GetName(), d >> 1, 0, d & 1, "")` broadcast.
    StClientModerated,
    // ---- a game (`a` = LobbyGame*)
    /// `a->AddRef()`: Rust holds it (returns `a`).
    GameHold = 39,
    GameRelease = 40,
    GameIsHost,
    GameFindClient,
    GameType,
    GameIsInProgress,
    GameAreAllReady,
    GameCanReady,
    /// `CanStartGame() && TryStartGame()`.
    GameCanStartAndTry,
    GamePingReplied,
    GameIsPinging,
    /// `a->IsSameGame(b)`.
    GameIsSameGame,
    GameJoinStatus,
    GamePing,
    /// `LobbyGame::CreateGame(a's type)` with `CopyFrom(a)`; held.
    GameNewCopy,
    GamePreNetCreate,
    GamePostNetCreate,
    GamePreNetJoin,
    GamePostNetJoin,
    /// `SetIsHost(true)`, `SetCaptainId(b's id)`, `b->SetCaptain(true, true)`.
    GameBecomeHost,
    /// `GetClients()->AddClient(b)`, `HandleHostGame(b)`.
    GameAddHostClient,
    /// `ClearPlayers()`, `mMyClient = NULL`, `PreNetLeaveGame()`, `LobbyNetLeaveGame(a)`, `PostNetLeaveGame()`.
    GameLeaveNet,
    GameRemoveClient,
    GameHandlePlayerJoined,
    /// `SetInProgress(c)`, `LobbyNetSetGameInProgress(c)`, `LobbyNetAdvertiseGame()`.
    GameSetInProgressNet,
    /// every client with a player: `SetReady(false)` and a `PlayerChangedEvent(client, 2)`.
    GameResetReadyFlags,
    /// `a->GetJoinReply(*b, d)`; if not NULL sent with `this->SendToClient(d, reply)`: 1 sent, 0 none, Threw.
    GameJoinReplySend,
    /// `a->HandleJoinReply(*b)` or Threw.
    GameHandleJoinReply,
    /// `a->HandleGameMessage(c, *b, d)`: 0 or Threw.
    GameHandleGameMessage,
    /// `LobbyMessageBox(JoinGameErrorTitle, a->GetStatusString(c))`.
    GameShowStatus,
    /// `LobbyPingGame(a, true)` and a `GameChangedEvent(a, 2)`.
    GamePingAndNotify,
    /// `LobbyQueryGameDetails(a)`, `SetShowDetails(true)`, a `GameChangedEvent(a, 2)`.
    GameShowDetails,
    /// `SetShowDetails(false)`, a `GameChangedEvent(a, 2)`.
    GameHideDetails,
    /// `LobbyDispatchEvent(new ChangeDirectConnectEvent(1, a))`.
    GameDirectConnect,
    /// a `GameOptionsPopup` for `a` opened as a standard popup.
    GameShowOptions,
    // ---- a client (`a` = LobbyClient*)
    ClientHasPlayer = 80,
    ClientPlayer,
    ClientIsReady,
    ClientId,
    /// `IsCaptain(1)`.
    ClientIsGameCaptain,
    /// `a->IsTeammate(b)`.
    ClientIsTeammate,
    ClientPlayerSetReady,
    ClientPlayerSetPing,
    /// a new player read from `*b` set on `a`: 1, 0 when the read failed, or Threw.
    ClientReadPlayer,
    /// the player-joined head for `a` and its player's bytes broadcast with `this->BroadcastToGame`.
    ClientBroadcastJoined,
    /// `new PlayerChangedEvent(a, c)` broadcast.
    ClientChanged,
    /// `new PlayerTeamChangedEvent(a, c)` broadcast.
    ClientTeamChanged,
    // ---- a message, a read buffer, events
    /// `b->SetPos(b->pos() + c)`.
    BufSkip = 110,
    /// a `ReadBuffer` over the message handed to `this->HandleMessage(.., b)`.
    HandleBytes,
    /// `LobbyNetSendToCaptain(message)`.
    NetSendToCaptain,
    /// `LobbyNetBroadcastToGame(message)`.
    NetBroadcastToGame,
    /// `LobbyNetSendToClient(b, message)`.
    NetSendToClient,
    NetKickClient,
    NetRemoveGameAdvertisement,
    NetHostGame,
    NetJoinGame,
    NetAdvertiseGame,
    /// `static_cast<JoinGameEvent*>(a)->mGame.get()`.
    EventJoinGame,
    /// `static_cast<GameActionEvent*>(a)->mGame.get()`.
    EventActionGame,
    /// `static_cast<GameActionEvent*>(a)->mAction`.
    EventAction,
    // ---- the lobby
    BroadcastLobbyEvent = 140,
    MessageBox,
    YesNo,
    /// a new AsyncOp run with status (text `c`, mode `d`, 500 ms); returns whether it was killed.
    RunWaitOp,
    PlaySound,
    StagingSoundEffects,
    ShowConfirmations,
    FirewallTestDone,
    BehindFirewall,
    GameScreenType,
    /// -1 without a lobby config, else its `mAllowJoinGameWithoutPing`.
    AllowJoinWithoutPing,
    Exit,
    /// `time(NULL)`.
    Now,
    // ---- LanLogic (`logic` a LanLogic; the C++ shell keeps its SmartPtrs, the socket set and the addresses). A
    // "take" op appends bytes (UTF-16 LE for a text) to the Rust buffer `b` with `ee_won_lobby_sink`.
    /// `mMyClient.get()`.
    LanMyClient = 200,
    /// `mMyClient = a`.
    LanSetMyClient,
    LanListenSocket,
    LanServerSocket,
    LanNetworkTimer,
    LanDiscoveryTimer,
    LanDiscoverySocket,
    /// `&mDiscoveryAddress`.
    LanDiscoveryAddress,
    /// the lobby's auth context, if any, stops refreshing its certificate.
    LanAuthAutoRefreshOff,
    /// CloseConnections' sockets: the listen socket killed, the host socket closed, the network timer killed, every
    /// pending socket closed, `mMyClient`, the socket set and the client list cleared.
    LanCloseSockets,
    /// `static_cast<SocketOp*>(a)->GetSocket()`.
    LanOpSocket,
    /// a LobbyLanSocket's client.
    LanSocketClient,
    /// `a->SetClient(b)`.
    LanSocketSetClient,
    /// `mPendingSockets.erase(a)`.
    LanForgetSocket,
    /// `mServerSocket->Kill()`, `mServerSocket = NULL`, `LobbyGameDisconnected(true)`.
    LanLoseHost,
    LanPendingCount,
    /// the pending socket `c` in the set's order.
    LanPendingAt,
    LanSocketLastTime,
    /// `a->QueueClose(c)`.
    LanSocketClose,
    LanStartNetworkTimer,
    /// `GetMyLobbyGame()`.
    LanMyGame,
    /// a LAN join's subnet bytes: the peer's `GetSixByte()[2..4]` and ours, packed low to high.
    LanSubnets,
    /// whether the password (`c` UTF-16 units, `d` of them) is `a`'s.
    LanGamePasswordIs,
    /// a SendMsgOp of the message queued on the socket `b`.
    LanSocketSend,
    /// a new LobbyLanClient named `c`/`d` (UTF-16 units); held.
    LanNewClient,
    /// `a->SetClientId(c)`, `mClients->AddClient(a)`.
    LanAddClient,
    ClientHold,
    ClientRelease,
    /// `a`'s name, taken.
    ClientNameTake,
    LanClientCount,
    /// the client `c` in the list's order.
    LanClientAt,
    /// `mClients->FindClient(c)`.
    LanFindClient,
    /// `mClients->RemoveClient(c)`; held.
    LanRemoveClient,
    /// `new ClientChangedEvent(a, 8, c)` broadcast.
    LanClientChanged,
    /// `LobbyClientLeftGame(a)`.
    ClientLeftGame,
    GameSetCaptainId,
    /// `mServerSocket->SetClient(a)`, `a->SetCaptain(true, true)`.
    LanServerClientIsCaptain,
    /// the receive op succeeded and was not killed.
    LanRecvOk,
    /// the socket's last message time is now.
    LanSocketStamp,
    /// the receive op's message (its ByteBuffer, or 0), taken.
    LanRecvMsgTake,
    /// AddPendingSocket: the close and receive completions, the receive op, the time, the set.
    LanAddPending,
    /// a LobbyLanClient's socket.
    LanClientSocket,
    /// a SendMsgOp of the message queued on the client `b`'s socket.
    LanClientSocketSend,
    /// `mServerSocket->QueueOp(new SendMsgOp(message))`.
    LanServerSend,
    ClientIsIgnored,
    /// a whisper received: the sound (if the chat sounds are on), the parser's reply name.
    LanWhisperReceived,
    /// `new RecvChatEvent(8, d >> 32, a, text, b)` broadcast; the text is `c` with `d & 0xffffffff` units.
    LanRecvChat,
    /// the staging message (the bytes from its type byte) read past the type and handed to the staging logic.
    LanStagingRead,
    /// ChangeLanName's dialog: Reset, LobbyDoDialog.
    LanNameDialogRun,
    LanNameDialogLength,
    /// `LobbySetLanName(dialog's name)`, the lobby event 0x186d2.
    LanNameDialogSave,
    LanBroadcastPort,
    /// ChangeLanPort's dialog: the game port, the port `c`, Reset, LobbyDoDialog.
    LanPortDialogRun,
    LanPortDialogPort,
    /// `LobbySetLanBroadcastPort(c)`, the lobby event 0x186d2.
    LanSetBroadcastPort,
    /// `a->mComponent`.
    EventComponent,
    EventDirectIsDelete,
    EventDirectIsAdd,
    EventDirectGame,
    /// a saved direct-connect game removed: the saved one, the list's, a `GameChangedEvent(a, 1)`.
    LanDirectRemove,
    /// HandleChangeDirectConnect's add: the dialog and the address (its DNS check), the game and its event.
    LanDirectAdd,
    /// a ClientOptionsPopup for `a` (room spec `c`).
    LanShowClientOptions,
    EventClientActionClient,
    EventClientActionRoomSpec,
    EventClientActionAction,
    /// `LobbySetClientIgnored(a, c, d)`.
    SetClientIgnored,
    /// `new SetChatInputEvent(2, a's name, c, false)` broadcast.
    LanSetChatInput,
    /// `LobbyKickClient(a, false)`.
    KickClientFromLobby,
    /// `LobbyGetChatCommandParser()`.
    ChatParser,
    ParserRoomFlags,
    ParserTarget,
    ParserCommand,
    ParserTextTake,
    /// `LobbySendEventTo(new ChatCommandErrorEvent(InvalidCommand), a)`.
    LanChatCommandError,
    /// the LAN games cleared and synced.
    LanClearLanGames,
    /// every unfiltered direct-connect game pinged, the lobby event 0x186da.
    LanPingDirectGames,
    /// EnterLanScreen's lists: no current user, the games cleared, the saved direct-connect games, their sync.
    LanEnterScreenLists,
    ShowLanScreen,
    DoCDKeyCheck,
    KillFirewallTest,
    /// RefreshCertificate.
    LanRefreshCertificate,
    /// HostGame's client: a new LobbyLanClient (id `c`, the LAN name, captain) added, the game's clients.
    LanHostMyClient,
    /// HostGame's listen socket bound and listening (an error dialog when not), its accept op, JoinedRoomEvent.
    LanHostListen,
    GameAddressPort,
    /// the summary: the head (the message), the game's summary, sent to the address `b`.
    LanSendSummary,
    /// a SendBytesToOp of the message to the address `b`.
    LanDiscoverySend,
    /// stale LAN games removed (synced if any).
    LanRemoveStaleGames,
    LanStartDiscoveryTimer,
    /// the discovery receive op is the discovery socket's and succeeded.
    LanDiscoveryOk,
    /// its bytes, taken.
    LanDiscoveryTake,
    /// HandleGameSummary from the packet past its head (port `c`).
    LanGameSummary,
    /// HandleGameKeepAlive (port `c`).
    LanGameKeepAlive,
    /// HandleGameRemoved (port `c`).
    LanGameRemoved,
    /// the receive op's sender address.
    LanRecvFrom,
    /// StartDiscovery's socket: the address, the QueueSocket, its bind (0 when bound).
    LanOpenDiscovery,
    /// the discovery socket's receive completion and op.
    LanDiscoveryArm,
    LanStopDiscovery,
    LanClearDirectGames,
    /// the accept op's socket (NULL when it failed or was killed).
    LanAcceptedSocket,
    /// a PeerAuthServerOp on the socket, tracked (30 s); nothing without an auth context.
    LanStartPeerAuth,
    /// `mServerSocket = new LobbyLanSocket`.
    LanNewServerSocket,
    GameHasPassword,
    /// JoinGame's password: emptied.
    LanJoinPasswordClear,
    /// the password dialog: Reset, LobbyDoDialog, the password kept on OK.
    LanPasswordDialogRun,
    /// a PeerAuthOp (`c`) or ConnectOp to the game run with status: whether it succeeded.
    LanConnect,
    /// `LobbyGetLanName()`, taken.
    LanNameTake,
    LanJoinPasswordTake,
    /// a RecvMsgOp on the host socket run with status: -1 failed, 0 no message, 1 its bytes taken.
    LanRecvReplyTake,
    /// JoinGame's end: the host socket's completions and receive op, the network timer, the game's clients,
    /// JoinedRoomEvent.
    LanJoinArm,
}

/// A Rust lobby object's way out: the host and the C++ object it belongs to.
#[derive(Clone, Copy)]
pub struct Host {
    pub logic: *mut c_void,
    pub call: EeWonLobbyHostFn,
}

/// A message: its C++ `ByteBuffer` (NULL for one Rust built) and its bytes (a pointer, not a slice: the bytes are
/// the C++ object's while C++ runs).
#[derive(Clone, Copy)]
pub struct Msg {
    pub buffer: *mut c_void,
    pub data: *const u8,
    pub length: usize,
}

impl Msg {
    /// A message Rust built (the bytes outlive every use of it).
    pub fn built(bytes: &[u8]) -> Msg {
        Msg { buffer: std::ptr::null_mut(), data: bytes.as_ptr(), length: bytes.len() }
    }
    /// No message (a NULL `ByteBuffer*`).
    pub fn is_none(&self) -> bool {
        self.buffer.is_null() && self.data.is_null()
    }
}

impl Host {
    /// The pointers are the C++ objects' and pass through: Rust never dereferences them.
    #[allow(clippy::not_unsafe_ptr_arg_deref)]
    pub fn op(&self, op: EeWonLobbyOp, a: *mut c_void, b: *mut c_void, c: isize, d: isize) -> isize {
        match self.call {
            // SAFETY: the host's contract (each op's arguments as documented)
            Some(f) => unsafe { f(self.logic, op, a, b, c, d) },
            None => 0,
        }
    }
    /// An op on one object.
    pub fn on(&self, op: EeWonLobbyOp, a: *mut c_void) -> isize {
        self.op(op, a, std::ptr::null_mut(), 0, 0)
    }
    pub fn on_bool(&self, op: EeWonLobbyOp, a: *mut c_void) -> bool {
        self.on(op, a) != 0
    }
    pub fn on_ptr(&self, op: EeWonLobbyOp, a: *mut c_void) -> *mut c_void {
        self.on(op, a) as *mut c_void
    }
    /// An op without arguments.
    pub fn get(&self, op: EeWonLobbyOp) -> isize {
        self.on(op, std::ptr::null_mut())
    }
    /// An op with one integer.
    pub fn int(&self, op: EeWonLobbyOp, c: isize) -> isize {
        self.op(op, std::ptr::null_mut(), std::ptr::null_mut(), c, 0)
    }
    /// An op on a message (and a client in `b`).
    pub fn msg(&self, op: EeWonLobbyOp, m: Msg, client: *mut c_void) -> isize {
        self.op(op, m.buffer, client, m.data as isize, m.length as isize)
    }
}

/// A game reference the Rust side holds (the original's local `SmartPtr<LobbyGame>`), given back when dropped.
pub struct HeldGame {
    host: Host,
    pub game: *mut c_void,
}

impl HeldGame {
    pub fn new(host: Host, game: *mut c_void) -> Self {
        HeldGame { host, game }
    }
}

impl Drop for HeldGame {
    fn drop(&mut self) {
        if !self.game.is_null() {
            self.host.on(EeWonLobbyOp::GameRelease, self.game);
        }
    }
}

/// A "take" op's sink: appends `length` bytes to the Rust buffer `sink` (a `Vec<u8>` the op was given).
///
/// # Safety
/// `sink` is the `b` of a take op; `bytes` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobby_sink(sink: *mut c_void, bytes: *const u8, length: usize) {
    if sink.is_null() || bytes.is_null() || length == 0 {
        return;
    }
    // SAFETY: the contract
    unsafe { (*(sink as *mut Vec<u8>)).extend_from_slice(std::slice::from_raw_parts(bytes, length)) };
}
