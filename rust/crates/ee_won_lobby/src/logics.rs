//! Three of the lobby's logic modules as Rust objects (R6 WONLobby round 8): PingLogic (the LAN and Internet
//! game lists' pings: a game host's reply timed and its summary or details read, the pings beyond the connection
//! speed queued, the hosted game's answers) with its retried request PingGameOp; AccountLogic (login, account
//! creation and update, the lost password and user name, the credits, the terms of use); InitLogic (the Internet
//! lobby's start-up: the Internet check, the DNS test, the documents, the directory query, the version check, the
//! firewall test).
//!
//! The C++ keeps each class as the shell callers use (`LobbyLogic`'s tracker and completions, the dialogs' and ops'
//! `SmartPtr`s, `PingGameOp`'s game) and forwards every method here. Every call on a C++ object (the model's games,
//! the ops, the server lists, the dialogs, the events, the `Lobby*` helpers) is one op of a host
//! (`PingLogic.cpp`, `AccountLogic.cpp`, `InitLogic.cpp`): the original's statements with no decision between
//! them. The decisions, the queue, the request ids, the retries, the replies' bytes, the errors, the last user
//! name and the wire format are here. Start-up decisions shared with the golden of round 15 stay `flow.rs`'s.

use std::cell::{Cell, RefCell};
use std::collections::VecDeque;
use std::ffi::c_void;

use crate::flow;
use crate::resource::EeWonLobbyRes;
use ee_won_net::buffer::EeWonReadBuffer;
use ee_won_net::strcase::wide_compare_no_case;

/// The host's ops. `obj` is the C++ shell (`PingLogic*`, `PingGameOp*`, `AccountLogic*`, `InitLogic*`); `a`..`d`
/// are pointers or integers as each op says. An op that returns an object "held" gives Rust a reference it gives
/// back with `Release`.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLogicOp {
    // ---- shared
    /// `((RefCount*)a)->AddRef()`.
    AddRef = 1,
    /// `((RefCount*)a)->Release()`.
    Release,
    /// `(int)((AsyncOp*)a)->GetStatus()`.
    OpStatus,
    /// `((AsyncOp*)a)->IsKilled()`.
    OpKilled,
    /// `LobbyBroadcastLobbyEvent(a, true)`.
    BroadcastLobby,
    /// `LobbyExit(a)`.
    Exit,
    /// `GetLobbyAuthContext()`.
    AuthContext,
    /// `GetLobbyRoot()`.
    Root,
    /// `LobbyBroadcastEvent(new LoginInitStatusEvent(LOBBY_STRING(a), desc, d & 1, d >> 8), true)`; the
    /// description is `*(GUIString*)b`, or `LOBBY_STRING(c)` when `b` is NULL and `c >= 0`, else empty.
    InitStatus,

    // ---- PingLogic
    /// `LobbyGetUDPManager()`.
    UdpManager = 20,
    /// `((UDPManager*)a)->SetDefaultCompletion(new LogicCompletion<PingLogic>(&PingLogic::HandleUDPRecv, obj))`.
    SetDefaultRecv,
    /// `static_cast<PingGameEvent*>(a)->mGame.get()`.
    EventGame,
    /// `static_cast<PingGameEvent*>(a)->mFlag`.
    EventFlag,
    /// `static_cast<UDPManagerRecvOp*>(a)->GetMsg()`: its data (NULL for none), its length in `*(size_t*)b`.
    RecvMsg,
    /// `GetMyLobbyGame()`.
    MyGame,
    /// `((LobbyGame*)a)->GetIsHost()`.
    GameIsHost,
    /// `((LobbyGame*)a)->GetGameType()`.
    GameType,
    /// `((LobbyGame*)a)->IsInProgress()`.
    GameInProgress,
    /// `((LobbyGame*)a)->IsPinging()`.
    GameIsPinging,
    /// `((LobbyGame*)a)->IsQueryingDetails()`.
    GameQueryingDetails,
    /// `((LobbyGame*)a)->SetPing(b)`.
    GameSetPing,
    /// `((LobbyGame*)a)->SetPingReplied(true)`.
    GameSetPingReplied,
    /// `GetLobbyConfig() != NULL && GetLobbyConfig()->mNoPingInProgress`.
    NoPingInProgress,
    /// `GetLobbyConfig() != NULL ? GetLobbyConfig()->mPingCeiling : 5000`.
    PingCeiling,
    /// `LobbyGetConnectionSpeed()`.
    ConnectionSpeed,
    /// `new WriteBuffer(3, true)` (the reply).
    ReplyNew,
    /// `((WriteBuffer*)a)->AppendBytes(b, c)`.
    BufAppend,
    /// `((LobbyGame*)a)->WriteSummary(*(WriteBuffer*)b)`.
    WriteSummary,
    /// `((LobbyGame*)a)->WriteDetails(*(WriteBuffer*)b)`.
    WriteDetails,
    /// `((UDPManager*)a)->Send(((WriteBuffer*)b)->ToByteBuffer(true).get(), ((UDPManagerRecvOp*)c)->GetFromAddr())`.
    ReplySend,
    /// `delete (WriteBuffer*)a`.
    BufFree,
    /// (obj a `PingGameOp*`) a `WriteBuffer(0, true)` of `b` bytes at `a`, `SetOutgoing(ToByteBuffer(true))`,
    /// `UDPManagerSendRecvOp::RunHook()`.
    RequestSend,
    /// (obj a `PingGameOp*`) `WONTickCount() - GetStartTime()`.
    Elapsed,
    /// `static_cast<PingGameOp*>(a)->mGame.get()`.
    OpGame,
    /// `static_cast<PingGameOp*>(a)->mRust`.
    OpRecord,
    /// `RunTrackedOp((AsyncOp*)a, new LogicCompletion<PingLogic>(&PingLogic::PingGameDone, obj), b)`.
    RunTracked,
    /// `((LobbyGame*)a)->ReadSummary(reader over *(EeWonReadBuffer*)b)` (the position written back): 0 or 1.
    ReadSummary,
    /// `((LobbyGame*)a)->ReadDetails(reader over *(EeWonReadBuffer*)b)` (the position written back).
    ReadDetails,
    /// `LobbyBroadcastEvent(new GameChangedEvent((LobbyGame*)a, 2), true)`.
    BroadcastGameChanged,
    /// `LobbyNotifyPingChange((LobbyGame*)a)`.
    NotifyPing,
    /// `mTracker.GetNumTrack()`.
    NumTracked,
    /// `dynamic_cast<PingGameOp*>(mTracker.GetTrackedOp(a))`'s `mGame.get()`, or NULL.
    TrackedGame,
    /// `mTracker.KillAll()`.
    KillTracked,
    /// `new PingGameOp((LobbyGame*)a, b, c)` held, then `SetUDPManager((UDPManager*)d)`.
    NewPingOp,

    // ---- AccountLogic
    /// `LobbyGetVersionStatus()`.
    VersionStatus = 60,
    /// `LobbyDoYesNoDialog(VersionOutOfData title, details, true)`.
    OutOfDateDialog,
    /// `mCreditsDialog->ResetScrolling(); StartScrolling(); LobbyDoDialog(it); StopScrolling()`.
    ShowCredits,
    /// `mTOUDialog->Reset(); LobbyDoDialog(mTOUDialog)`: the result.
    ShowTOU,
    /// `LobbyShowCreateAccount()` (a 1), `LobbyShowLostPassword()` (2), `LobbyShowLostUsername()` (3).
    ShowScreen,
    /// `LobbyGetAccountServers()`.
    AccountServers,
    /// a `RequestPasswordEmailOp` of `((GetLostPasswordEvent*)a)->mUserName` on `b`, run behind its status box.
    RequestPasswordEmail,
    /// a `RequestAccountInfoEmailOp` of `((GetLostUserNameEvent*)a)->mEmail` on `b`, run behind its status box.
    RequestInfoEmail,
    /// `RustServerOp::NewGetAccount(a, b)` held, `SetNum(NEWSLETTER, 1)`, run behind its status box.
    QueryAccount,
    /// `LobbyBroadcastEvent(new QueryAccountResultEvent(a's e-mail, a's newsletter != 0), true)`.
    BroadcastAccount,
    /// `RustServerOp::NewUpdateAccount(a, b, event c's password and e-mail)`, its newsletter, run behind its box.
    UpdateAccount,
    /// `RustServerOp::NewCreateAccount(a, event b's fields)` held, run behind its status box.
    CreateAccount,
    /// event `a`'s user added, the terms' times set on it, the prefs saved, event 100052, the login fields set.
    AccountCreated,
    /// `LobbyGetRootWindow()`.
    RootWindow,
    /// `((Window*)a)->GetKeyMod()`.
    KeyMod,
    /// `((LoginEvent*)a)->mUserName`'s `wchar_t`s (its length in `*(size_t*)b`).
    LoginName,
    /// `event->mUserName = event->mUserName.substr(1)`.
    LoginStripAt,
    /// the event's user (`LobbyFindUser`) is unknown or its terms of use are older: 0 or 1.
    UserNeedsTOU,
    /// `((AuthContext*)a)->SetUserName(event b's name); SetPassword(event b's password)`.
    SetCredentials,
    /// `LobbyDoCDKeyCheck()`.
    DoCDKeyCheck,
    /// `LobbyIsCDKeyValid()`.
    CDKeyValid,
    /// `LobbyMessageBox(InvalidCDKey title, details)`.
    InvalidCDKeyBox,
    /// `((AuthContext*)a)->AddCommunity(L"Admin")` (b != 0) or `RemoveCommunity(L"Admin")`.
    SetAdmin,
    /// `new GetCertOp(a)` held, run behind the login status box.
    GetCert,
    /// `((AuthContext*)a)->SetDoAutoRefresh(true)`.
    AutoRefresh,
    /// the event `a`'s user saved (its password when remembered), its terms' times, the prefs, the current user.
    LoginSaveUser,
    /// `LobbyEnterInternetScreen(a != 0)`.
    EnterInternet,

    // ---- InitLogic
    /// `LobbySetVersionStatus(a)`.
    SetVersionStatus = 100,
    /// `GetLobbyConfig() != NULL && GetLobbyConfig()->mCheckForInternet`.
    CheckForInternet,
    /// `AsyncSocket::HasInternetConnection()`.
    HasInternet,
    /// the account, auth, firewall and patch server lists cleared (those that exist).
    ClearServerLists,
    /// `LobbyLoadGlobalPrefs(); IPAddr::SetLocalHost(LobbyGetNetworkAdapter(), true)`.
    LoadPrefs,
    /// `LoadTOUTimes()`'s statements: the saved terms' files' times into `LobbySetTOUTimes`.
    LoadTOUTimes,
    /// `LobbyBindUDPPort()`.
    BindUdp,
    /// the login screen's three controls enabled (`b`) or not on root `a`.
    EnableLogin,
    /// `LobbyGetLobbyMusic()`.
    LobbyMusic,
    /// the lobby music: `SetLooping(true)` (a 0), `Play()` (1), `Stop()` (2).
    Music,
    /// the config and the directory, room directory and auth server lists all exist.
    DnsReady,
    /// the directory, room directory and auth server lists filled from the config's names.
    FillDnsServers,
    /// the room directory list has no address.
    RoomDirEmpty,
    /// `LobbyResetRoomDirServers()`.
    ResetRoomDir,
    /// the image of `RustServerOp* a` decoded and broadcast with `mCrossPromotion`.
    PromotionImage,
    /// `mCrossPromotion` = `b` bytes at `a`.
    SetPromotionLink,
    /// an HTTP get of the image URL (`b` bytes at `a`) in the global cache, completing in
    /// `CrossPromotionImageCompletion`, added to `mHTTPSession`.
    FetchPromotionImage,
    /// `LobbyGetTOUTimes` into `*(int32_t*)a` (system) and `*(int32_t*)b` (game).
    TOUTimes,
    /// `((RustServerOp*)a)->GetNum(b, c)`.
    OpNum,
    /// the terms-of-use document of owner `b` of op `a` written to its file: 1 unless the writer threw.
    SaveTOU,
    /// `LobbyBroadcastEvent(new GotHTTPDocumentEvent(a), true)`.
    BroadcastDocument,
    /// `((RustServerOp*)a)->GetHttpDocument(b)`'s data; its length in `*(size_t*)c` (`usize::MAX`: no document).
    DocBytes,
    /// `LobbyBroadcastEvent(new SetCrossPromotionEvent(NULL, ""), true)`.
    BroadcastNoPromotion,
    /// the config and the account, auth, firewall and directory server lists all exist.
    DirReady,
    /// `!GetLobbyConfig()->mAuthServers.empty()`.
    HasAuthConfig,
    /// the list of target `a` (1 auth, 2 account, 3 firewall, 4 patch), if it exists, adds op `b`'s list `c`.
    AddDirAddresses,
    /// the auth (a 1), account (2) or patch (4) server list's number of addresses.
    ServerCount,
    /// `LobbyGetDirServers()->NotifyFailed(((RustServerOp*)a)->GetCurAddr())`.
    DirNotifyFailed,
    /// `new GUIString(LOBBY_STRING(a))`, or an empty one for `a < 0`.
    TextNew,
    /// `((GUIString*)a)->append(LOBBY_STRING(b))`.
    TextAppendRes,
    /// `((GUIString*)a)->append(GUIString(", "))`.
    TextAppendComma,
    /// `((GUIString*)a)->length()`.
    TextLength,
    /// `((GUIString*)a)->append(*(GUIString*)b)`.
    TextAppendText,
    /// `delete (GUIString*)a`.
    TextFree,
    /// `new GUIString(LobbyFormatString(LOBBY_STRING(a), GetWONStatusString(b)))`.
    TextStatus,
    /// the patch server list, if it exists, `NotifyFailed`s op `a`'s current address.
    VersionNotifyFailed,
    /// `mAcceptOp.get() == a`.
    IsAcceptOp,
    /// `mAcceptOp = NULL`.
    ClearAcceptOp,
    /// the accepted socket of `AcceptOp* a` is from `mFirewallAddress`'s host.
    AcceptedFromFirewall,
    /// `LobbySetBehindFirewall(a != 0)`.
    SetBehindFirewall,
    /// `mAcceptOp = new AcceptOp(((AcceptOp*)a)->GetSocket())`, run with `AcceptCompletion` and the accept timeout.
    RestartAccept,
    /// `LobbySetPublicAddress(((DetectFirewallOp*)a)->GetPublicAddr())`.
    SetPublicAddress,
    /// `mFirewallSocket.get() != NULL`.
    HasFirewallSocket,
    /// `mFirewallAddress` = op `a`'s current address; `mAcceptOp` on `mFirewallSocket`, run; the socket dropped.
    StartAccept,
    /// the config exists and names a directory server.
    DnsTestReady,
    /// a `DNSTestOp` of the config's first directory server, tracked with `DNSCompletion`.
    RunDnsTest,
    /// the message of the day, terms of use and advert documents requested (when the config exists).
    StartDocuments,
    /// `LobbyGetDirServers()`.
    DirServers,
    /// the multi-directory query of the auth, database, firewall and patch servers on list `a`, tracked.
    RunDirQuery,
    /// the patch server list and the config exist.
    VersionReady,
    /// the version check op on the patch servers, tracked with `VersionCompletion`.
    RunVersionCheck,
    /// `LobbyGetFirewallDetectServers()`.
    FirewallServers,
    /// `LobbyResetFirewallTest()`.
    ResetFirewallTest,
    /// a `DetectFirewallOp` on list `a` (the lobby port, no listen, no wait), tracked with `FirewallCompletion`.
    RunFirewallDetect,
    /// `mFirewallSocket = new AsyncSocket(0)`.
    FirewallSocketNew,
    /// `mFirewallSocket->Fn2B0D90(LobbyGetLobbyPort(), 0)`: the status.
    FirewallBind,
    /// `mFirewallSocket->Listen(1000)`: the status.
    FirewallListen,
    /// `mFirewallSocket = NULL`.
    FirewallSocketDrop,
    /// a running `mAcceptOp` killed, its socket closed, the member cleared.
    KillAcceptOp,
    /// `mHTTPSession->KillAll()`.
    KillHttp,
    /// `LobbyShowInternetScreen()`.
    ShowInternet,
    /// `mNetworkAdapterDialog->Reset(); LobbyDoDialog(it)`: the result.
    AdapterDialog,
    /// `LobbySetNetworkAdapter(mNetworkAdapterDialog->GetSelectedAddress())`.
    SetAdapter,
}

/// The inward calls (each a method of the logic, `a`..`c` its arguments).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLogicCall {
    // ---- PingLogic
    /// `InitComponent`.
    PingInit = 1,
    /// `HandleComponentEvent(a)` of type `b`.
    PingEvent,
    /// `HandleUDPRecv(a)`.
    PingRecv,
    /// `PingGameDone(a)`.
    PingDone,
    /// `GetNumPingsToDo()`.
    PingNumToDo,
    /// `PingGame(a, b, c)`.
    PingGame,
    /// `KillPings`.
    PingKill,
    /// the next request id (a member).
    PingNextId,
    /// the number of queued games (a member).
    PingPending,
    // ---- AccountLogic
    /// `HandleControlEvent`: 1 show create account, 2 lost password, 3 lost user name.
    AcctControl = 20,
    /// `HandleComponentEvent(a)` of type `b`.
    AcctEvent,
    /// `ShowCredits()`.
    AcctCredits,
    /// `ShowTOU()`: 0 or 1.
    AcctShowTOU,
    /// `CheckLoginStatus()`: 0 or 1.
    AcctCheckLogin,
    /// `mNeedTOU` (a member).
    AcctNeedTOU,
    /// `mLastUserName`'s length (a member).
    AcctLastNameLength,
    // ---- InitLogic
    /// `Init()`.
    InitInit = 40,
    /// `Start(a != 0)`.
    InitStart,
    /// `KillHook()`.
    InitKillHook,
    /// `KillFirewallTest()`.
    InitKillFirewall,
    /// `HandleControlEvent`: 1 back, 2 show the Internet screen, 3 choose the network adapter.
    InitControl,
    /// `HandleComponentEvent` of type `a`.
    InitEvent,
    /// the completions (`a` the op).
    InitDns,
    InitDocument,
    InitPromotionImage,
    InitDirectory,
    InitVersion,
    InitAccept,
    InitFirewall,
    /// `mErrors` (a 0) or `mIgnoredErrors` (1) (members).
    InitField,
}

/// Which logic an object is.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLogicKind {
    Ping = 1,
    Account,
    Init,
}

/// The host: `(obj, op, a, b, c, d) -> result`.
pub type EeWonLogicHostFn =
    Option<unsafe extern "C" fn(obj: *mut c_void, op: EeWonLogicOp, a: isize, b: isize, c: isize, d: isize) -> isize>;

/// `PingGameOp::CheckResponse`'s answer when a read went past the reply's end (the C++ rethrows
/// `ReadBufferException`).
pub const EE_WON_LOGIC_THREW: i64 = i64::MIN;

type Op = EeWonLogicOp;
type Res = EeWonLobbyRes;

const WS_SUCCESS: i64 = 0;
const WS_NONE: i64 = 100004;
const WS_DB_OUT_OF_DATE: isize = -2814;
const LOBBY_EVENT_CREATE_ACCOUNT: isize = 100010;
const LOBBY_EVENT_LOST_PASSWORD: isize = 100018;
const LOBBY_EVENT_LOST_USER_NAME: isize = 100019;
const LOBBY_EVENT_LOGIN: isize = 100035;
const LOBBY_EVENT_PING_GAME: isize = 100037;
const LOBBY_EVENT_QUERY_ACCOUNT: isize = 100040;
const LOBBY_EVENT_UPDATE_ACCOUNT: isize = 100060;
const PING_NONE: u16 = 0xffff;
const PING_NOT_PINGED: u16 = 0xfffe;
const PING_QUEUED: u16 = 0xfffd;
const PING_DETAILS: u16 = 0xfffc;
const DIALOG_OK: isize = 100001;
const HTTPDOC_TYPE: isize = ee_won_net::httpdoc::EE_WON_HTTPDOC_TYPE as isize;
const HTTPDOC_STATUS: isize = ee_won_net::httpdoc::EE_WON_HTTPDOC_STATUS as isize;
const HTTPDOC_MODIFIED: isize = ee_won_net::httpdoc::EE_WON_HTTPDOC_MODIFIED as isize;
const HTTPDOC_TOU: isize = ee_won_net::httpdoc::EE_WON_HTTPDOC_TOU as isize;
const HTTPDOC_ADV: isize = ee_won_net::httpdoc::EE_WON_HTTPDOC_ADV as isize;
const OWNER_SYS: isize = 0;
const OWNER_GAME: isize = 1;
const DIR_LISTS: isize = ee_won_net::reqdir::EE_WON_REQOP_DIR_LISTS as isize;
const DIR_STATUS: isize = ee_won_net::reqdir::EE_WON_REQOP_DIR_STATUS as isize;

fn host_call(host: EeWonLogicHostFn, obj: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    match host {
        // SAFETY: the host's contract (an op on its own object)
        Some(h) => unsafe { h(obj, op, a, b, c, d) },
        None => 0,
    }
}

/// A reader's next `n` bytes (the original's `ReadByte`/`ReadShort` read them; `None` where it threw).
fn take(r: &mut EeWonReadBuffer, n: u64) -> Option<&'static [u8]> {
    let at = r.take(n)?;
    // SAFETY: `take` checked the bytes are within the reader's live data
    Some(unsafe { std::slice::from_raw_parts(r.data.wrapping_add(at), n as usize) })
}

fn available(r: &EeWonReadBuffer) -> i64 {
    r.length as i64 - r.pos as i64
}

/// `wchar_t`s widened as the C++'s (unsigned 16-bit on Windows, signed 32-bit elsewhere).
///
/// # Safety
/// `p` holds `n` `wchar_t`s (or `n` is 0).
unsafe fn wide_units(p: isize, n: usize) -> Vec<i64> {
    if p == 0 || n == 0 {
        return Vec::new();
    }
    #[cfg(windows)]
    // SAFETY: the contract
    let v = unsafe { std::slice::from_raw_parts(p as *const u16, n) }.iter().map(|&u| u as i64).collect();
    #[cfg(not(windows))]
    // SAFETY: the contract
    let v = unsafe { std::slice::from_raw_parts(p as *const i32, n) }.iter().map(|&u| u as i64).collect();
    v
}

/// A start-up error's description: a resource string or a text an op made.
#[derive(Clone, Copy)]
enum Desc {
    Res(Res),
    Text(isize),
}

/// One logic object (its kind's members; the others stay at their defaults).
pub struct EeWonLogic {
    kind: EeWonLogicKind,
    obj: *mut c_void,
    host: EeWonLogicHostFn,
    // PingLogic
    next_id: Cell<u16>,
    pending: RefCell<VecDeque<isize>>,
    // AccountLogic
    last_user_name: RefCell<Vec<i64>>,
    need_tou: Cell<bool>,
    // InitLogic
    errors: Cell<i32>,
    ignored: Cell<i32>,
}

impl EeWonLogic {
    pub fn new(kind: EeWonLogicKind, obj: *mut c_void, host: EeWonLogicHostFn) -> Self {
        EeWonLogic {
            kind,
            obj,
            host,
            next_id: Cell::new(0),
            pending: RefCell::new(VecDeque::new()),
            last_user_name: RefCell::new(Vec::new()),
            need_tou: Cell::new(false),
            errors: Cell::new(0),
            ignored: Cell::new(0),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        host_call(self.host, self.obj, op, a, b, c, d)
    }

    fn op0(&self, op: Op) -> isize {
        self.op(op, 0, 0, 0, 0)
    }

    fn op1(&self, op: Op, a: isize) -> isize {
        self.op(op, a, 0, 0, 0)
    }

    fn op2(&self, op: Op, a: isize, b: isize) -> isize {
        self.op(op, a, b, 0, 0)
    }

    // ---- PingLogic

    fn ping_init(&self) {
        let manager = self.op0(Op::UdpManager);
        if manager != 0 {
            self.op1(Op::SetDefaultRecv, manager);
        }
    }

    fn ping_event(&self, event: isize, kind: isize) {
        if kind == LOBBY_EVENT_PING_GAME {
            let game = self.op1(Op::EventGame, event);
            let flag = self.op1(Op::EventFlag, event) != 0;
            self.ping_game(game, flag, false);
        }
    }

    /// `HandlePingRequest`: the hosted game's answer (a short request ends it where the original threw).
    fn ping_request(&self, reader: &mut EeWonReadBuffer, recv: isize) {
        let game = self.op0(Op::MyGame);
        if game == 0 || self.op1(Op::GameIsHost, game) == 0 {
            return;
        }
        let Some(id) = take(reader, 2) else { return };
        let id = [id[0], id[1]];
        let Some(kind) = take(reader, 1) else { return };
        let kind = kind[0];
        if self.op1(Op::GameType, game) != kind as isize {
            return;
        }
        if self.op1(Op::GameInProgress, game) != 0 && self.op0(Op::NoPingInProgress) != 0 {
            return;
        }
        let buffer = self.op0(Op::ReplyNew);
        let head = [8u8, id[0], id[1]];
        self.op(Op::BufAppend, buffer, head.as_ptr() as isize, 3, 0);
        let Some(flags) = take(reader, 1) else {
            self.op1(Op::BufFree, buffer);
            return;
        };
        let flags = flags[0] & 3;
        self.op(Op::BufAppend, buffer, &flags as *const u8 as isize, 1, 0);
        if flags & 1 != 0 {
            self.op2(Op::WriteSummary, game, buffer);
        }
        if flags & 2 != 0 {
            self.op2(Op::WriteDetails, game, buffer);
        }
        let manager = self.op0(Op::UdpManager);
        if manager != 0 {
            self.op(Op::ReplySend, manager, buffer, recv, 0);
        }
        self.op1(Op::BufFree, buffer);
    }

    fn ping_recv(&self, recv: isize) {
        if self.op1(Op::OpStatus, recv) as i32 != 0 || self.op1(Op::OpKilled, recv) != 0 {
            return;
        }
        let mut length = 0usize;
        let data = self.op2(Op::RecvMsg, recv, &mut length as *mut usize as isize);
        let mut reader = EeWonReadBuffer::new(data as *const u8, if data == 0 { 0 } else { length as _ });
        let Some(kind) = take(&mut reader, 1) else { return };
        if kind[0] as i8 == 7 {
            self.ping_request(&mut reader, recv);
        }
    }

    fn ping_done(&self, op: isize) {
        if self.op1(Op::OpKilled, op) != 0 {
            return;
        }
        let record = self.op1(Op::OpRecord, op) as *const EeWonPingOp;
        // SAFETY: the op's live record (the op is held by its completion)
        let record = unsafe { &*record };
        let ping = record.ping.get();
        let game = self.op1(Op::OpGame, op);
        if ping != PING_NONE {
            self.op1(Op::GameSetPingReplied, game);
        } else if record.retries.get() < 3 {
            record.retries.set(record.retries.get() + 1);
            let timeout = self.op0(Op::PingCeiling);
            self.op2(Op::RunTracked, op, timeout);
            return;
        }
        self.op2(Op::GameSetPing, game, ping as isize);
        let reply = record.reply.borrow().clone();
        if let Some(reply) = reply {
            let mut reader = EeWonReadBuffer::new(reply.as_ptr(), reply.len() as _);
            let flags = record.flags.get();
            let mut ok = true;
            if flags & 1 != 0 {
                ok = self.op2(Op::ReadSummary, game, &mut reader as *mut EeWonReadBuffer as isize) != 0;
            }
            if flags & 2 != 0 && ok {
                self.op2(Op::ReadDetails, game, &mut reader as *mut EeWonReadBuffer as isize);
            }
        }
        self.op1(Op::BroadcastGameChanged, game);
        self.op1(Op::NotifyPing, game);
        let maximum = self.op0(Op::ConnectionSpeed) as u16 as i64;
        loop {
            if self.pending.borrow().is_empty() {
                break;
            }
            if self.op0(Op::NumTracked) as i64 >= maximum && maximum != 0 {
                break;
            }
            let Some(next) = self.pending.borrow_mut().pop_front() else { break };
            if self.op1(Op::GameIsPinging, next) != 0 && self.op1(Op::GameQueryingDetails, next) == 0 {
                self.op2(Op::GameSetPing, next, PING_NOT_PINGED as isize);
                self.ping_game(next, false, true);
            }
            self.op1(Op::Release, next);
        }
    }

    fn ping_num_to_do(&self) -> isize {
        let pending = self.pending.borrow().len() as isize;
        pending + self.op0(Op::NumTracked) as i32 as isize
    }

    fn ping_game(&self, game: isize, details: bool, force: bool) {
        if game == 0 {
            return;
        }
        if self.op1(Op::GameInProgress, game) != 0 && self.op0(Op::NoPingInProgress) != 0 {
            return;
        }
        if details {
            if self.op1(Op::GameQueryingDetails, game) != 0 {
                return;
            }
        } else if self.op1(Op::GameIsPinging, game) != 0 {
            return;
        }
        if !force && !details {
            let maximum = self.op0(Op::ConnectionSpeed) as u16 as i64;
            if self.op0(Op::NumTracked) as i64 >= maximum && maximum > 0 {
                self.op2(Op::GameSetPing, game, PING_QUEUED as isize);
                self.op1(Op::NotifyPing, game);
                self.op1(Op::AddRef, game);
                self.pending.borrow_mut().push_back(game);
                return;
            }
        }
        let manager = self.op0(Op::UdpManager);
        if manager == 0 {
            return;
        }
        self.op2(Op::GameSetPing, game, if details { PING_DETAILS } else { PING_QUEUED } as isize);
        self.op1(Op::NotifyPing, game);
        let mut flags = 0isize;
        if self.op1(Op::GameType, game) == 3 {
            flags = 1;
        }
        if details {
            flags |= 2;
        }
        let id = self.next_id.get();
        self.next_id.set(id.wrapping_add(1));
        let op = self.op(Op::NewPingOp, game, id as isize, flags, manager);
        let timeout = self.op0(Op::PingCeiling);
        self.op2(Op::RunTracked, op, timeout);
        self.op1(Op::Release, op);
    }

    fn ping_kill(&self) {
        let pending: Vec<isize> = self.pending.borrow().iter().copied().collect();
        for &game in &pending {
            self.op2(Op::GameSetPing, game, PING_NOT_PINGED as isize);
        }
        let mut index = 0;
        while index < self.op0(Op::NumTracked) as i32 {
            let game = self.op1(Op::TrackedGame, index as isize);
            if game != 0 {
                self.op2(Op::GameSetPing, game, PING_NOT_PINGED as isize);
            }
            index += 1;
        }
        let cleared: Vec<isize> = self.pending.borrow_mut().drain(..).collect();
        for game in cleared {
            self.op1(Op::Release, game);
        }
        self.op0(Op::KillTracked);
    }

    // ---- AccountLogic

    fn check_login_status(&self) -> bool {
        if self.op0(Op::VersionStatus) as i32 as isize == WS_DB_OUT_OF_DATE {
            if self.op0(Op::OutOfDateDialog) == DIALOG_OK {
                self.op1(Op::Exit, 3);
            }
            return false;
        }
        true
    }

    fn show_tou(&self) -> bool {
        self.op0(Op::ShowTOU) as i32 == 1
    }

    fn acct_control(&self, which: isize) {
        match which {
            1 => {
                if self.check_login_status() {
                    self.op1(Op::ShowScreen, 1);
                    self.need_tou.set(true);
                }
            }
            2 | 3 if self.check_login_status() => {
                self.op1(Op::ShowScreen, which);
            }
            _ => {}
        }
    }

    fn acct_event(&self, event: isize, kind: isize) {
        match kind {
            LOBBY_EVENT_CREATE_ACCOUNT => self.create_account(event),
            LOBBY_EVENT_LOST_PASSWORD | LOBBY_EVENT_LOST_USER_NAME => {
                let account = self.op0(Op::AccountServers);
                if account == 0 || !self.check_login_status() {
                    return;
                }
                let op =
                    if kind == LOBBY_EVENT_LOST_PASSWORD { Op::RequestPasswordEmail } else { Op::RequestInfoEmail };
                self.op2(op, event, account);
            }
            LOBBY_EVENT_LOGIN => self.login(event),
            LOBBY_EVENT_QUERY_ACCOUNT => {
                let account = self.op0(Op::AccountServers);
                let auth = self.op0(Op::AuthContext);
                if account == 0 || auth == 0 {
                    return;
                }
                let op = self.op2(Op::QueryAccount, account, auth);
                if self.op1(Op::OpStatus, op) as i32 as i64 == WS_SUCCESS {
                    self.op1(Op::BroadcastAccount, op);
                }
                self.op1(Op::Release, op);
            }
            LOBBY_EVENT_UPDATE_ACCOUNT => {
                let account = self.op0(Op::AccountServers);
                let auth = self.op0(Op::AuthContext);
                if account == 0 || auth == 0 {
                    return;
                }
                self.op(Op::UpdateAccount, account, auth, event, 0);
            }
            _ => {}
        }
    }

    fn create_account(&self, event: isize) {
        if !self.check_login_status() {
            return;
        }
        if self.need_tou.get() {
            if !self.show_tou() {
                return;
            }
            self.need_tou.set(false);
        }
        let servers = self.op0(Op::AccountServers);
        if servers == 0 {
            return;
        }
        let op = self.op2(Op::CreateAccount, servers, event);
        if self.op1(Op::OpStatus, op) as i32 as i64 == WS_SUCCESS {
            self.op1(Op::AccountCreated, event);
        }
        self.op1(Op::Release, op);
    }

    fn login_name(&self, event: isize) -> Vec<i64> {
        let mut length = 0usize;
        let p = self.op2(Op::LoginName, event, &mut length as *mut usize as isize);
        // SAFETY: the event's live name
        unsafe { wide_units(p, length) }
    }

    fn login(&self, event: isize) {
        let auth = self.op0(Op::AuthContext);
        let window = self.op0(Op::RootWindow);
        if auth == 0 || window == 0 {
            return;
        }
        let key_mod = self.op1(Op::KeyMod, window) as i32;
        if key_mod & 7 == 7 {
            self.op0(Op::ShowCredits);
            return;
        }
        let mut name = self.login_name(event);
        if name.is_empty() || !self.check_login_status() {
            return;
        }
        let mut admin = false;
        if name[0] == '@' as i64 {
            admin = true;
            self.op1(Op::LoginStripAt, event);
            name = self.login_name(event);
        }
        let differs = wide_compare_no_case(&name, &self.last_user_name.borrow()) != 0;
        if differs {
            if self.op1(Op::UserNeedsTOU, event) != 0 && !self.show_tou() {
                return;
            }
            *self.last_user_name.borrow_mut() = name;
        }
        self.op2(Op::SetCredentials, auth, event);
        if self.op0(Op::DoCDKeyCheck) != 0 && self.op0(Op::CDKeyValid) == 0 {
            self.op0(Op::InvalidCDKeyBox);
            return;
        }
        self.op2(Op::SetAdmin, auth, admin as isize);
        let op = self.op1(Op::GetCert, auth);
        if self.op1(Op::OpStatus, op) as i32 as i64 != WS_SUCCESS {
            self.op1(Op::Release, op);
            return;
        }
        self.op1(Op::AutoRefresh, auth);
        self.op1(Op::LoginSaveUser, event);
        self.op1(Op::BroadcastLobby, 100052);
        self.op1(Op::EnterInternet, (key_mod & 4 == 0) as isize);
        self.op1(Op::Release, op);
    }

    // ---- InitLogic

    fn init_status(&self, title: Res, desc: Option<Desc>, allow: bool, code: isize) {
        let (text, res) = match desc {
            Some(Desc::Text(t)) => (t, -1),
            Some(Desc::Res(r)) => (0, r as isize),
            None => (0, -1),
        };
        self.op(Op::InitStatus, title as isize, text, res, allow as isize | code << 8);
    }

    fn set_error(&self, error: i32, desc: Desc) -> bool {
        let e = flow::init_errors(error, self.ignored.get());
        self.errors.set(e.errors);
        if e.errors != 0 {
            self.init_status(Res::LobbyContainer_InitFailed_String, Some(desc), e.allow_ignore, 3);
            return true;
        }
        false
    }

    fn start(&self, retry: bool) {
        self.op1(Op::SetVersionStatus, WS_NONE as isize);
        self.errors.set(0);
        if self.op0(Op::CheckForInternet) != 0
            && flow::no_internet(true, self.op0(Op::HasInternet) != 0)
            && self.set_error(1, Desc::Res(Res::LobbyContainer_NoConnection_String))
        {
            return;
        }
        if !retry {
            self.op0(Op::ClearServerLists);
        }
        self.start_dns_test();
    }

    fn init(&self) {
        self.ignored.set(0);
        self.op0(Op::LoadPrefs);
        self.op0(Op::LoadTOUTimes);
        self.op0(Op::BindUdp);
        self.op1(Op::BroadcastLobby, 100052);
        let root = self.op0(Op::Root);
        if root == 0 {
            return;
        }
        self.op2(Op::EnableLogin, root, 0);
        self.op1(Op::Music, 0);
        if self.op0(Op::LobbyMusic) != 0 {
            self.op1(Op::Music, 1);
        } else {
            self.op1(Op::Music, 2);
        }
        self.start(false);
    }

    fn dns_done(&self, op: isize) {
        if self.op0(Op::DnsReady) == 0 {
            return;
        }
        if self.op1(Op::OpStatus, op) as i32 as i64 == WS_SUCCESS {
            self.op0(Op::FillDnsServers);
            if self.op0(Op::RoomDirEmpty) != 0 {
                self.op0(Op::ResetRoomDir);
            }
            self.op0(Op::StartDocuments);
            self.start_directory_query();
        } else {
            self.set_error(2, Desc::Res(Res::LobbyContainer_DNSLookupFailed_String));
        }
    }

    fn parse_promotion(&self, doc: &[u8]) {
        let (link, image) = flow::cross_promotion(doc);
        self.op2(Op::SetPromotionLink, link.as_ptr() as isize, link.len() as isize);
        if !image.is_empty() {
            self.op2(Op::FetchPromotionImage, image.as_ptr() as isize, image.len() as isize);
        }
    }

    fn handle_tou(&self, op: isize) {
        let mut system = 0i32;
        let mut game = 0i32;
        self.op2(Op::TOUTimes, &mut system as *mut i32 as isize, &mut game as *mut i32 as isize);
        let mut changed = false;
        for (owner, saved) in [(OWNER_SYS, system), (OWNER_GAME, game)] {
            let status = self.op(Op::OpNum, op, HTTPDOC_STATUS, owner, 0) as i32;
            let modified = self.op(Op::OpNum, op, HTTPDOC_MODIFIED, owner, 0) as i32;
            if flow::tou_saved(status, modified, saved) && self.op2(Op::SaveTOU, op, owner) != 0 {
                changed = true;
            }
        }
        if changed {
            self.op0(Op::LoadTOUTimes);
        }
    }

    fn document_done(&self, op: isize) {
        self.op1(Op::BroadcastDocument, op);
        match self.op(Op::OpNum, op, HTTPDOC_TYPE, 0, 0) {
            HTTPDOC_TOU => self.handle_tou(op),
            HTTPDOC_ADV => {
                let mut length = 0usize;
                let data = self.op(Op::DocBytes, op, OWNER_GAME, &mut length as *mut usize as isize, 0);
                if length != usize::MAX && flow::document_bytes(length) != 0 {
                    // SAFETY: the op's live document
                    let doc = unsafe { std::slice::from_raw_parts(data as *const u8, length) };
                    self.parse_promotion(doc);
                    return;
                }
                self.op0(Op::BroadcastNoPromotion);
            }
            _ => {}
        }
    }

    fn directory_done(&self, op: isize) {
        if self.op0(Op::DirReady) == 0 {
            return;
        }
        let status = self.op1(Op::OpStatus, op) as i32;
        if !flow::dir_answered(status) {
            let text = self.op2(Op::TextStatus, Res::LobbyContainer_DirLookupFailure_String as isize, status as isize);
            self.set_error(0x20, Desc::Text(text));
            self.op1(Op::TextFree, text);
            return;
        }
        let count = self.op(Op::OpNum, op, DIR_LISTS, 0, 0) as i32;
        for index in 0..count {
            if self.op(Op::OpNum, op, DIR_STATUS, index as isize, 0) != 0 {
                continue;
            }
            let target = flow::dir_request_target(index, self.op0(Op::HasAuthConfig) != 0);
            if target != 0 {
                self.op(Op::AddDirAddresses, target as isize, op, index as isize, 0);
            }
        }
        if self.op1(Op::ServerCount, 1) == 0 || self.op1(Op::ServerCount, 2) == 0 || self.op1(Op::ServerCount, 4) == 0 {
            self.op1(Op::DirNotifyFailed, op);
            let names = self.op1(Op::TextNew, -1);
            let errors = flow::missing_servers(
                self.op1(Op::ServerCount, 1) as usize,
                self.op1(Op::ServerCount, 2) as usize,
                self.op1(Op::ServerCount, 4) as usize,
            );
            if errors & 8 != 0 {
                self.op2(Op::TextAppendRes, names, Res::LobbyContainer_AuthServers_String as isize);
            }
            if errors & 4 != 0 {
                if self.op1(Op::TextLength, names) != 0 {
                    self.op1(Op::TextAppendComma, names);
                }
                self.op2(Op::TextAppendRes, names, Res::LobbyContainer_AccountServers_String as isize);
            }
            if errors & 0x10 != 0 {
                if self.op1(Op::TextLength, names) != 0 {
                    self.op1(Op::TextAppendComma, names);
                }
                self.op2(Op::TextAppendRes, names, Res::LobbyContainer_VersionServers_String as isize);
            }
            let description = self.op1(Op::TextNew, Res::LobbyContainer_NoServers_String as isize);
            self.op2(Op::TextAppendText, description, names);
            let stop = self.set_error(errors, Desc::Text(description));
            self.op1(Op::TextFree, description);
            self.op1(Op::TextFree, names);
            if stop {
                return;
            }
        }
        self.start_version_check();
        self.start_firewall_test();
    }

    fn version_done(&self, op: isize) {
        let root = self.op0(Op::Root);
        if root == 0 {
            return;
        }
        let status = self.op1(Op::OpStatus, op) as i32;
        self.op1(Op::SetVersionStatus, status as isize);
        let outcome = flow::version_outcome(status);
        if outcome != 0 {
            if outcome == 1 {
                self.op1(Op::VersionNotifyFailed, op);
            }
            let text =
                self.op2(Op::TextStatus, Res::LobbyContainer_VersionCheckFailure_String as isize, status as isize);
            self.set_error(0x40, Desc::Text(text));
            self.op1(Op::TextFree, text);
            return;
        }
        self.op2(Op::EnableLogin, root, 1);
        self.init_status(Res::LobbyContainer_InitComplete_String, None, false, 2);
    }

    fn accept_done(&self, op: isize) {
        if self.op1(Op::IsAcceptOp, op) == 0 {
            return;
        }
        self.op0(Op::ClearAcceptOp);
        let accepted = self.op1(Op::OpStatus, op) as i32 as i64 == WS_SUCCESS;
        let outcome = flow::accept_outcome(accepted, accepted && self.op1(Op::AcceptedFromFirewall, op) != 0);
        match outcome {
            0 | 1 => {
                self.op1(Op::SetBehindFirewall, (outcome == 0) as isize);
                self.op1(Op::BroadcastLobby, 100055);
            }
            _ => {
                self.op1(Op::RestartAccept, op);
            }
        }
    }

    fn firewall_done(&self, op: isize) {
        if self.op1(Op::OpStatus, op) as i32 as i64 == WS_SUCCESS {
            self.op1(Op::SetPublicAddress, op);
            if self.op0(Op::HasFirewallSocket) != 0 {
                self.op1(Op::StartAccept, op);
            }
        } else {
            self.op1(Op::SetBehindFirewall, 1);
        }
        self.op1(Op::BroadcastLobby, 100055);
    }

    fn start_dns_test(&self) {
        if self.op0(Op::DnsTestReady) == 0 {
            return;
        }
        self.init_status(Res::LobbyContainer_InitStart_String, None, false, 1);
        self.op0(Op::RunDnsTest);
    }

    fn start_directory_query(&self) {
        let context = self.op0(Op::DirServers);
        if context == 0 {
            return;
        }
        self.init_status(Res::LobbyContainer_QueryingServers_String, None, false, 0);
        self.op1(Op::RunDirQuery, context);
    }

    fn start_version_check(&self) {
        if self.op0(Op::VersionReady) == 0 {
            return;
        }
        self.init_status(Res::LobbyContainer_CheckingVersion_String, None, false, 0);
        self.op0(Op::RunVersionCheck);
    }

    fn start_firewall_test(&self) {
        let firewall = self.op0(Op::FirewallServers);
        if firewall == 0 {
            return;
        }
        self.kill_firewall_test();
        self.op0(Op::ResetFirewallTest);
        self.op1(Op::RunFirewallDetect, firewall);
        self.op1(Op::BroadcastLobby, 100055);
        self.op0(Op::FirewallSocketNew);
        if self.op0(Op::FirewallBind) as i32 == 0 && self.op0(Op::FirewallListen) as i32 == 0 {
            return;
        }
        self.op0(Op::FirewallSocketDrop);
    }

    fn kill_firewall_test(&self) {
        self.op0(Op::KillAcceptOp);
        self.op0(Op::FirewallSocketDrop);
    }

    fn init_control(&self, which: isize) {
        match which {
            1 => {
                self.op1(Op::Exit, 0);
            }
            2 => {
                self.op0(Op::ShowInternet);
            }
            3 if self.op0(Op::AdapterDialog) == DIALOG_OK => {
                self.op0(Op::SetAdapter);
            }
            _ => {}
        }
    }

    fn init_event(&self, kind: isize) {
        match flow::init_event(kind as i32) {
            2 => self.ignored.set(self.ignored.get() | self.errors.get()),
            1 => {}
            _ => return,
        }
        self.start(true);
    }

    fn call(&self, call: EeWonLogicCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonLogicCall;
        match call {
            C::PingInit => self.ping_init(),
            C::PingEvent => self.ping_event(a, b),
            C::PingRecv => self.ping_recv(a),
            C::PingDone => self.ping_done(a),
            C::PingNumToDo => return self.ping_num_to_do(),
            C::PingGame => self.ping_game(a, b != 0, c != 0),
            C::PingKill => self.ping_kill(),
            C::PingNextId => return self.next_id.get() as isize,
            C::PingPending => return self.pending.borrow().len() as isize,
            C::AcctControl => self.acct_control(a),
            C::AcctEvent => self.acct_event(a, b),
            C::AcctCredits => {
                self.op0(Op::ShowCredits);
            }
            C::AcctShowTOU => return self.show_tou() as isize,
            C::AcctCheckLogin => return self.check_login_status() as isize,
            C::AcctNeedTOU => return self.need_tou.get() as isize,
            C::AcctLastNameLength => return self.last_user_name.borrow().len() as isize,
            C::InitInit => self.init(),
            C::InitStart => self.start(a != 0),
            C::InitKillHook => {
                self.op0(Op::KillHttp);
                self.kill_firewall_test();
            }
            C::InitKillFirewall => self.kill_firewall_test(),
            C::InitControl => self.init_control(a),
            C::InitEvent => self.init_event(a),
            C::InitDns => self.dns_done(a),
            C::InitDocument => self.document_done(a),
            C::InitPromotionImage => {
                if self.op1(Op::OpStatus, a) as i32 as i64 == WS_SUCCESS {
                    self.op1(Op::PromotionImage, a);
                }
            }
            C::InitDirectory => self.directory_done(a),
            C::InitVersion => self.version_done(a),
            C::InitAccept => self.accept_done(a),
            C::InitFirewall => self.firewall_done(a),
            C::InitField => return if a == 0 { self.errors.get() } else { self.ignored.get() } as isize,
        }
        0
    }

    /// The shell's destructor: the queued games' references given back.
    fn release_all(&self) {
        if self.kind == EeWonLogicKind::Ping {
            let games: Vec<isize> = self.pending.borrow_mut().drain(..).collect();
            for game in games {
                self.op1(Op::Release, game);
            }
        }
    }
}

/// `PingGameOp`'s members: the request id, the ping time (0xffff: no reply yet), the reply's flags (the request's
/// until a reply), the retries and the reply's bytes after its flags.
pub struct EeWonPingOp {
    obj: *mut c_void,
    host: EeWonLogicHostFn,
    request_id: u16,
    ping: Cell<u16>,
    flags: Cell<u8>,
    retries: Cell<u16>,
    reply: RefCell<Option<Vec<u8>>>,
}

/// `PingGameOp`'s virtuals.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonPingOpCall {
    /// `RunHook()` (`a` the game).
    RunHook = 1,
    /// `CheckResponse(*(EeWonReadBuffer*)a)`: the status, or [`EE_WON_LOGIC_THREW`].
    CheckResponse,
    /// a member: 0 the request id, 1 the ping, 2 the flags, 3 the retries, 4 the reply's length (-1: none).
    Field,
}

impl EeWonPingOp {
    fn op(&self, op: Op, a: isize, b: isize) -> isize {
        host_call(self.host, self.obj, op, a, b, 0, 0)
    }

    fn run_hook(&self, game: isize) {
        let id = self.request_id.to_le_bytes();
        let kind = self.op(Op::GameType, game, 0) as u8;
        let request = [7u8, id[0], id[1], kind, self.flags.get()];
        self.ping.set(PING_NONE);
        self.op(Op::RequestSend, request.as_ptr() as isize, request.len() as isize);
    }

    fn check_response(&self, reader: &mut EeWonReadBuffer) -> i64 {
        let Some(kind) = take(reader, 1) else { return EE_WON_LOGIC_THREW };
        if kind[0] != 8 {
            return WS_NONE;
        }
        let Some(id) = take(reader, 2) else { return EE_WON_LOGIC_THREW };
        // the signed ReadShort is compared with the unsigned member: ids from 0x8000 never match
        if i16::from_le_bytes([id[0], id[1]]) as i32 != self.request_id as i32 {
            return WS_NONE;
        }
        self.ping.set(self.op(Op::Elapsed, 0, 0) as u16);
        self.flags.set(0);
        if available(reader) <= 0 {
            return WS_SUCCESS;
        }
        let Some(flags) = take(reader, 1) else { return EE_WON_LOGIC_THREW };
        self.flags.set(flags[0]);
        let length = available(reader).max(0) as u64;
        let Some(rest) = take(reader, length) else { return EE_WON_LOGIC_THREW };
        *self.reply.borrow_mut() = Some(rest.to_vec());
        WS_SUCCESS
    }
}

// ---- the C ABI

/// A new logic of `kind` for the C++ shell `obj`.
#[no_mangle]
pub extern "C" fn ee_won_logic_new(kind: EeWonLogicKind, obj: *mut c_void, host: EeWonLogicHostFn) -> *mut EeWonLogic {
    Box::into_raw(Box::new(EeWonLogic::new(kind, obj, host)))
}

/// The shell's destructor.
///
/// # Safety
/// `p` came from [`ee_won_logic_new`] (or is null), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_logic_free(p: *mut EeWonLogic) {
    if !p.is_null() {
        // SAFETY: the contract
        let x = unsafe { Box::from_raw(p) };
        x.release_all();
    }
}

/// A method of the logic.
///
/// # Safety
/// `p` is a live logic; the host's calls may come back into it (a completion run at once).
#[no_mangle]
pub unsafe extern "C" fn ee_won_logic_call(
    p: *const EeWonLogic,
    call: EeWonLogicCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: the contract; the state is in cells, no borrow is held across a host call
    match unsafe { p.as_ref() } {
        Some(x) => x.call(call, a, b, c),
        None => 0,
    }
}

/// A new ping op's members for the C++ shell `obj`.
#[no_mangle]
pub extern "C" fn ee_won_pingop_new(
    obj: *mut c_void,
    host: EeWonLogicHostFn,
    request_id: u16,
    flags: u8,
) -> *mut EeWonPingOp {
    Box::into_raw(Box::new(EeWonPingOp {
        obj,
        host,
        request_id,
        ping: Cell::new(0),
        flags: Cell::new(flags),
        retries: Cell::new(0),
        reply: RefCell::new(None),
    }))
}

/// The ping op's destructor.
///
/// # Safety
/// `p` came from [`ee_won_pingop_new`] (or is null), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_pingop_free(p: *mut EeWonPingOp) {
    if !p.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(p) });
    }
}

/// A virtual of the ping op.
///
/// # Safety
/// `p` is a live ping op; for `CheckResponse`, `a` is a live reader.
#[no_mangle]
pub unsafe extern "C" fn ee_won_pingop_call(p: *const EeWonPingOp, call: EeWonPingOpCall, a: isize) -> i64 {
    // SAFETY: the contract
    let Some(x) = (unsafe { p.as_ref() }) else { return 0 };
    match call {
        EeWonPingOpCall::RunHook => {
            x.run_hook(a);
            0
        }
        // SAFETY: the contract
        EeWonPingOpCall::CheckResponse => x.check_response(unsafe { &mut *(a as *mut EeWonReadBuffer) }),
        EeWonPingOpCall::Field => match a {
            0 => x.request_id as i64,
            1 => x.ping.get() as i64,
            2 => x.flags.get() as i64,
            3 => x.retries.get() as i64,
            _ => x.reply.borrow().as_ref().map_or(-1, |r| r.len() as i64),
        },
    }
}

#[cfg(test)]
#[path = "logics_tests.rs"]
mod tests;
