//! The lobby logics' one C++ shell (R6 WON lobby and network round 36). AccountLogic, DialogLogic, InitLogic,
//! RoutingLogic, StagingLogic and LanLogic were six C++ classes, each a `LobbyLogic` (or `NetLogic`) whose virtuals
//! forwarded to its Rust object (logics.rs, dlglogic.rs, routelogic.rs, stagingobj.rs, lanobj.rs) and which held the
//! C++ objects its host reaches. They are one C++ class now, `RustLogic` (ee/wonapi/WONLobbyLogic.h): the kind, the
//! kind's Rust object, and counted slots for the dialogs and the other objects. This module is what the six classes'
//! virtuals, InitComponent bodies, control maps and completions were:
//!
//! - [`route`]: a virtual of a kind as the call on the kind's Rust object the class's method made (`Route`);
//! - [`dialogs`]: the dialogs a kind's InitComponent looked up (`SafeGetDialog`), in the original's order, into the
//!   shell's dialog slots; a missing one throws in the C++ hook, which answers `EE_WON_LOBBY_THROWN`: the lookups
//!   stop and the C++ entry rethrows;
//! - [`done_route`]: a completion (`LogicCompletion<T>` with a member, `RoutingOpCompletion` with its code) as the
//!   call it made, after `LogicCompletion`'s test (the logic not killed, a tracked op its tracked one);
//!   `RoutingOpCompletion` made none (routelogic.rs tests the logic and the op itself).
//!
//! PingLogic (logics.rs's Ping kind) joined the shell in R6 WON round 39: its class, its two forwarding virtuals and
//! its two `LogicCompletion<PingLogic>`s are the Ping kind's routes.
//!
//! The shell holds no Rust state: a call is routed from its arguments alone, and the kind's object is entered through
//! its own entry point, so a re-entering call is what it was.
//!
//! Reach: every LAN game runs each kind's InitComponent, control and component events, timer reset and kill hook, the
//! LAN logic's NetLogic calls and completions and the dialog logic's status completion. The routing kind's NetLogic
//! calls and the Internet completions are reached by nothing (golden: won_shell36_test over the classes as they
//! were).

use crate::dlglogic::{ee_won_dlglogic_call, ee_won_dlglogic_free, EeWonDlgLogic, EeWonDlgLogicCall as Dc};
use crate::lanobj::{ee_won_lan_call, ee_won_lan_free, EeWonLan, EeWonLanCall as Nc};
use crate::lobbydlg::EeWonLobbyDlgKind;
use crate::lobbydlg::EeWonLobbyDlgKind as Dk;
use crate::lobbyids::{
    ID_AddLANName, ID_ChangeLANBroadcast, ID_HostGame, ID_LeaveGame, ID_Logout, ID_RefreshGameList, ID_ShowLanScreen,
    ID_StartGame,
};
use crate::lobbyprv::EE_WON_LOBBY_THROWN;
use crate::logics::{ee_won_logic_call, ee_won_logic_free, EeWonLogic, EeWonLogicCall as Lc};
use crate::routelogic::{ee_won_routelogic_call, ee_won_routelogic_free, EeWonRouteLogic, EeWonRouteLogicCall as Rc};
use crate::stagingobj::{ee_won_staging_call, ee_won_staging_free, EeWonStaging, EeWonStagingCall as Sc};
use ee_won_net::cppnet as net;
use ee_won_net::opobj::{
    ee_won_logic_completion_runs, ee_won_op_tracker_add, ee_won_op_tracker_at, ee_won_op_tracker_kill_all,
    ee_won_op_tracker_len, EeWonOpTracker, EeWonRemoveTrackedFn,
};
use std::ffi::{c_char, c_void, CStr};
use std::sync::atomic::{AtomicPtr, Ordering};

/// Which logic a shell is (its Rust object's type).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLogicShellKind {
    /// AccountLogic: an `EeWonLogic`.
    Account = 1,
    /// DialogLogic: an `EeWonDlgLogic`.
    Dialog,
    /// InitLogic: an `EeWonLogic`.
    Init,
    /// RoutingLogic: an `EeWonRouteLogic`.
    Routing,
    /// StagingLogic: an `EeWonStaging`.
    Staging,
    /// LanLogic: an `EeWonLan`.
    Lan,
    /// PingLogic: an `EeWonLogic` (its own C++ class until R6 WON round 39).
    Ping,
}

/// The shell's virtuals: LobbyLogic's five, then NetLogic's fourteen. Arguments: the virtual's own, in order; a
/// message call's are the buffer, the client, the buffer's bytes and their length.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLogicShellV {
    KillHook = 1,
    /// `a` the ComponentConfig.
    InitComponent,
    Reset,
    /// `a` the control id.
    HandleControlEvent,
    /// `a` the event, `b` its type.
    HandleComponentEvent,
    HostGame,
    JoinGame,
    GetMyClient,
    SendToCaptain,
    BroadcastToGame,
    SendToClient,
    AdvertiseGame,
    RemoveGameAdvertisement,
    /// `a` the client, `b` the ban flag.
    KickClient,
    SetGameInProgress,
    LeaveGame,
    Suspend,
    /// Answers 1 when the C++ runs `LobbyLeaveGame(0)` (LanLogic's), 0 after another kind's call.
    Resume,
    Logout,
}

/// The completions the logics' hosts make (the member each `LogicCompletion<T>` named; `RouteNotify` is
/// `RoutingOpCompletion`, its code the completion's number).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLogicShellDone {
    InitDns = 1,
    InitDocument,
    InitPromotionImage,
    InitDirectory,
    InitVersion,
    InitAccept,
    InitFirewall,
    RouteUserStatus,
    RouteGroups,
    RouteClosed,
    RouteNotify,
    LanSocketClosed,
    LanNetworkTimer,
    LanRecvMsg,
    LanDiscoveryTimer,
    LanDiscoveryRecv,
    LanAccept,
    LanPeerAuthDone,
    DlgStatusOp,
    /// PingLogic::HandleUDPRecv (the UDP manager's default completion).
    PingRecv,
    /// PingLogic::PingGameDone (a tracked ping's).
    PingDone,
}

/// The call a virtual or completion is on the kind's Rust object.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Route {
    /// No call; the answer.
    Answer(isize),
    /// `ee_won_logic_call(rust, call, a, b, 0)`.
    Logic(Lc, isize, isize),
    /// `ee_won_dlglogic_call(rust, call, a, 0, 0, 0, 0)`.
    Dlg(Dc, isize),
    /// `ee_won_routelogic_call(rust, call, a, b)`.
    Routing(Rc, isize, isize),
    /// `ee_won_staging_call(rust, call, a, b, c, d)`.
    Staging(Sc, isize, isize, isize, isize),
    /// `ee_won_lan_call(rust, call, a, b, c, d)`.
    Lan(Nc, isize, isize, isize, isize),
}

/// StagingLogic::HandleControlEvent's map: the host, leave, logout and start/ready buttons.
pub(crate) fn staging_control(id: isize) -> isize {
    match id as i32 {
        x if x == ID_HostGame => 1,
        x if x == ID_LeaveGame => 2,
        x if x == ID_Logout => 3,
        x if x == ID_StartGame => 4,
        _ => 0,
    }
}

/// LanLogic::HandleControlEvent's map: the LAN name, LAN port, refresh game list and show LAN screen commands.
pub(crate) fn lan_control(id: isize) -> isize {
    match id as i32 {
        x if x == ID_AddLANName => 1,
        x if x == ID_ChangeLANBroadcast => 2,
        x if x == ID_RefreshGameList => 3,
        x if x == ID_ShowLanScreen => 4,
        _ => 0,
    }
}

/// The virtual `v` of a `kind` logic as the class's method made it (`Answer(0)`: the base's empty virtual, or a
/// NetLogic virtual of a kind that is none).
pub fn route(kind: EeWonLogicShellKind, v: EeWonLogicShellV, a: isize, b: isize, c: isize, d: isize) -> Route {
    use EeWonLogicShellKind as K;
    use EeWonLogicShellV as V;
    match (kind, v) {
        (K::Account, V::HandleControlEvent) => Route::Logic(Lc::AcctControlId, a, 0),
        (K::Account, V::HandleComponentEvent) => Route::Logic(Lc::AcctEvent, a, b),
        (K::Init, V::KillHook) => Route::Logic(Lc::InitKillHook, 0, 0),
        (K::Init, V::HandleControlEvent) => Route::Logic(Lc::InitControlId, a, 0),
        (K::Init, V::HandleComponentEvent) => Route::Logic(Lc::InitEvent, b, 0),
        (K::Dialog, V::KillHook) => Route::Dlg(Dc::KillHook, 0),
        (K::Dialog, V::InitComponent) => Route::Dlg(Dc::Init, a),
        (K::Dialog, V::Reset) => Route::Dlg(Dc::Reset, 0),
        (K::Routing, V::Reset) => Route::Routing(Rc::Reset, 0, 0),
        (K::Routing, V::HandleControlEvent) => Route::Routing(Rc::ControlId, a, 0),
        (K::Routing, V::HandleComponentEvent) => Route::Routing(Rc::Event, a, b),
        (K::Routing, V::HostGame) => Route::Routing(Rc::HostGame, a, 0),
        (K::Routing, V::JoinGame) => Route::Routing(Rc::JoinGame, a, 0),
        (K::Routing, V::GetMyClient) => Route::Routing(Rc::MyClient, 0, 0),
        (K::Routing, V::SendToCaptain) => Route::Routing(Rc::SendToCaptain, a, 0),
        (K::Routing, V::BroadcastToGame) => Route::Routing(Rc::BroadcastToGame, a, 0),
        (K::Routing, V::SendToClient) => Route::Routing(Rc::SendToClient, b, a),
        (K::Routing, V::AdvertiseGame) => Route::Routing(Rc::Advertise, a, 0),
        (K::Routing, V::RemoveGameAdvertisement) => Route::Routing(Rc::RemoveAdvertisement, 0, 0),
        (K::Routing, V::KickClient) => Route::Routing(Rc::KickClient, a, b),
        (K::Routing, V::SetGameInProgress) => Route::Routing(Rc::SetGameInProgress, a, 0),
        (K::Routing, V::LeaveGame) => Route::Routing(Rc::LeaveGame, 0, 0),
        (K::Routing, V::Suspend) => Route::Routing(Rc::Suspend, 0, 0),
        (K::Routing, V::Resume) => Route::Routing(Rc::Resume, 0, 0),
        (K::Routing, V::Logout) => Route::Routing(Rc::Logout, 0, 0),
        (K::Staging, V::HandleControlEvent) => match staging_control(a) {
            0 => Route::Answer(0),
            which => Route::Staging(Sc::Control, 0, 0, which, 0),
        },
        (K::Staging, V::HandleComponentEvent) => Route::Staging(Sc::ComponentEvent, a, 0, b, 0),
        (K::Lan, V::HandleControlEvent) => match lan_control(a) {
            0 => Route::Answer(0),
            which => Route::Lan(Nc::Control, 0, 0, which, 0),
        },
        (K::Lan, V::HandleComponentEvent) => Route::Lan(Nc::ComponentEvent, a, 0, b, 0),
        (K::Lan, V::HostGame) => Route::Lan(Nc::HostGame, a, 0, 0, 0),
        (K::Lan, V::JoinGame) => Route::Lan(Nc::JoinGame, a, 0, 0, 0),
        (K::Lan, V::GetMyClient) => Route::Lan(Nc::MyClient, 0, 0, 0, 0),
        (K::Lan, V::SendToCaptain) => Route::Lan(Nc::SendToCaptain, a, 0, c, d),
        (K::Lan, V::BroadcastToGame) => Route::Lan(Nc::BroadcastToGame, a, 0, c, d),
        (K::Lan, V::SendToClient) => Route::Lan(Nc::SendToClient, a, b, c, d),
        (K::Lan, V::AdvertiseGame) => Route::Lan(Nc::AdvertiseGame, a, 0, 0, 0),
        (K::Lan, V::RemoveGameAdvertisement) => Route::Lan(Nc::RemoveGameAdvertisement, a, 0, 0, 0),
        (K::Lan, V::KickClient) => Route::Lan(Nc::KickClient, 0, a, 0, 0),
        (K::Lan, V::LeaveGame) => Route::Lan(Nc::LeaveGame, 0, 0, 0, 0),
        (K::Lan, V::Resume) => Route::Answer(1),
        (K::Lan, V::Logout) => Route::Lan(Nc::Logout, 0, 0, 0, 0),
        (K::Ping, V::InitComponent) => Route::Logic(Lc::PingInit, 0, 0),
        (K::Ping, V::HandleComponentEvent) => Route::Logic(Lc::PingEvent, a, b),
        _ => Route::Answer(0),
    }
}

/// A completion as the call its member made on the kind's object, and whether `LogicCompletion`'s test runs first
/// (`RoutingOpCompletion` had none). `None`: the completion is not this kind's.
pub fn done_route(
    kind: EeWonLogicShellKind,
    done: EeWonLogicShellDone,
    code: isize,
    op: isize,
) -> Option<(Route, bool)> {
    use EeWonLogicShellDone as D;
    use EeWonLogicShellKind as K;
    let r = match (kind, done) {
        (K::Init, D::InitDns) => Route::Logic(Lc::InitDns, op, 0),
        (K::Init, D::InitDocument) => Route::Logic(Lc::InitDocument, op, 0),
        (K::Init, D::InitPromotionImage) => Route::Logic(Lc::InitPromotionImage, op, 0),
        (K::Init, D::InitDirectory) => Route::Logic(Lc::InitDirectory, op, 0),
        (K::Init, D::InitVersion) => Route::Logic(Lc::InitVersion, op, 0),
        (K::Init, D::InitAccept) => Route::Logic(Lc::InitAccept, op, 0),
        (K::Init, D::InitFirewall) => Route::Logic(Lc::InitFirewall, op, 0),
        (K::Routing, D::RouteUserStatus) => Route::Routing(Rc::UserStatusDone, op, 0),
        (K::Routing, D::RouteGroups) => Route::Routing(Rc::GroupsDone, op, 0),
        (K::Routing, D::RouteClosed) => Route::Routing(Rc::ConnectionClosed, op, 0),
        (K::Routing, D::RouteNotify) => return Some((Route::Routing(Rc::Notify, code, op), false)),
        (K::Lan, D::LanSocketClosed) => Route::Lan(Nc::SocketClosed, op, 0, 0, 0),
        (K::Lan, D::LanNetworkTimer) => Route::Lan(Nc::NetworkTimer, op, 0, 0, 0),
        (K::Lan, D::LanRecvMsg) => Route::Lan(Nc::RecvMsg, op, 0, 0, 0),
        (K::Lan, D::LanDiscoveryTimer) => Route::Lan(Nc::DiscoveryTimer, op, 0, 0, 0),
        (K::Lan, D::LanDiscoveryRecv) => Route::Lan(Nc::DiscoveryRecv, op, 0, 0, 0),
        (K::Lan, D::LanAccept) => Route::Lan(Nc::Accept, op, 0, 0, 0),
        (K::Lan, D::LanPeerAuthDone) => Route::Lan(Nc::PeerAuthDone, op, 0, 0, 0),
        (K::Dialog, D::DlgStatusOp) => Route::Dlg(Dc::StatusOpComplete, op),
        (K::Ping, D::PingRecv) => Route::Logic(Lc::PingRecv, op, 0),
        (K::Ping, D::PingDone) => Route::Logic(Lc::PingDone, op, 0),
        _ => return None,
    };
    Some((r, true))
}

/// A dialog a kind's InitComponent looks up: the shell's dialog slot, the dialog's kind, its name in the lobby
/// layout, and the file and line the original's SafeGetComponent reported for a missing one.
#[derive(Debug, PartialEq, Eq)]
pub struct DialogRow {
    pub slot: i32,
    pub kind: Dk,
    pub name: &'static CStr,
    pub file: &'static CStr,
    pub line: i32,
}

const fn row(slot: i32, kind: Dk, name: &'static CStr, file: &'static CStr, line: i32) -> DialogRow {
    DialogRow { slot, kind, name, file, line }
}

const ACCOUNT_FILE: &CStr = c"C:\\EEX\\Empire Earth\\WONAPI\\WONLobby\\AccountLogic.cpp";
const INIT_FILE: &CStr = c"C:\\EEX\\Empire Earth\\WONAPI\\WONLobby\\InitLogic.cpp";
const ROUTING_FILE: &CStr = c"C:\\EEX\\Empire Earth\\WONAPI\\WONLobby\\RoutingLogic.cpp";

static ACCOUNT_DIALOGS: [DialogRow; 2] =
    [row(0, Dk::Credits, c"CreditsDialog", ACCOUNT_FILE, 43), row(1, Dk::Tou, c"TOUDialog", ACCOUNT_FILE, 44)];
static INIT_DIALOGS: [DialogRow; 1] = [row(0, Dk::ChooseAdapter, c"ChooseNetworkAdapterDialog", INIT_FILE, 49)];
static ROUTING_DIALOGS: [DialogRow; 6] = [
    row(0, Dk::AddFriend, c"AddFriendDialog", ROUTING_FILE, 125),
    row(1, Dk::AdminAction, c"AdminActionDialog", ROUTING_FILE, 126),
    row(2, Dk::BadUser, c"BadUserDialog", ROUTING_FILE, 127),
    row(3, Dk::CreateRoom, c"CreateRoomDialog", ROUTING_FILE, 128),
    // the slots in the members' order: the room dialog (0x84) before the password dialog (0x88)
    row(5, Dk::Password, c"PasswordDialog", ROUTING_FILE, 129),
    row(4, Dk::Room, c"RoomDialog", ROUTING_FILE, 130),
];
static STAGING_DIALOGS: [DialogRow; 1] = [row(0, Dk::CreateGame, c"CreateGameDialog", c"StagingLogic.cpp", 41)];
static LAN_DIALOGS: [DialogRow; 4] = [
    row(0, Dk::DirectConnect, c"DirectConnectDialog", c"LanLogic.cpp", 105),
    row(1, Dk::LanName, c"LanNameDialog", c"LanLogic.cpp", 106),
    row(2, Dk::LanPort, c"LanPortDialog", c"LanLogic.cpp", 107),
    row(3, Dk::Password, c"PasswordDialog", c"LanLogic.cpp", 108),
];

/// The dialogs `kind`'s InitComponent looks up, in the original's order (DialogLogic's two are its Rust object's:
/// dlglogic.rs `Init`).
pub fn dialogs(kind: EeWonLogicShellKind) -> &'static [DialogRow] {
    match kind {
        EeWonLogicShellKind::Account => &ACCOUNT_DIALOGS,
        EeWonLogicShellKind::Init => &INIT_DIALOGS,
        EeWonLogicShellKind::Routing => &ROUTING_DIALOGS,
        EeWonLogicShellKind::Staging => &STAGING_DIALOGS,
        EeWonLogicShellKind::Lan => &LAN_DIALOGS,
        EeWonLogicShellKind::Dialog | EeWonLogicShellKind::Ping => &[],
    }
}

/// The C++ hook of a dialog lookup: `SafeGetDialog(config, logic->mDialog[slot], kind, name, file, line)` under
/// WonGuard (`EE_WON_LOBBY_THROWN` when it threw).
pub type EeWonLogicShellFindFn = Option<
    unsafe extern "C" fn(
        logic: *mut c_void,
        config: *mut c_void,
        slot: i32,
        kind: EeWonLobbyDlgKind,
        name: *const c_char,
        file: *const c_char,
        line: i32,
    ) -> isize,
>;

/// Runs a route on the kind's Rust object.
///
/// # Safety
/// `rust` is the live object of the route's kind.
unsafe fn run(rust: *mut c_void, r: Route) -> isize {
    // SAFETY (each arm): the caller's object of the arm's type; the entry points take shared references, so a call
    // that re-enters is sound
    match r {
        Route::Answer(x) => x,
        Route::Logic(call, a, b) => unsafe { ee_won_logic_call(rust as *const EeWonLogic, call, a, b, 0) },
        Route::Dlg(call, a) => unsafe { ee_won_dlglogic_call(rust as *const EeWonDlgLogic, call, a, 0, 0, 0, 0) },
        Route::Routing(call, a, b) => unsafe { ee_won_routelogic_call(rust as *const EeWonRouteLogic, call, a, b) },
        Route::Staging(call, a, b, c, d) => unsafe {
            ee_won_staging_call(rust as *const EeWonStaging, call, a as *mut c_void, b as *mut c_void, c, d)
        },
        Route::Lan(call, a, b, c, d) => unsafe {
            ee_won_lan_call(rust as *const EeWonLan, call, a as *mut c_void, b as *mut c_void, c, d)
        },
    }
}

/// A virtual of the shell `logic` (a `RustLogic`) of `kind` over its Rust object. InitComponent first looks the
/// kind's dialogs up through `find` (`EE_WON_LOBBY_THROWN` from one stops and is the answer: the C++ rethrows).
///
/// # Safety
/// `rust` is the live Rust object of `kind`; `logic` and `find` are the shell and its hook; the arguments are the
/// virtual's.
#[no_mangle]
pub unsafe extern "C" fn ee_won_logicshell_virtual(
    kind: EeWonLogicShellKind,
    rust: *mut c_void,
    v: EeWonLogicShellV,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    logic: *mut c_void,
    find: EeWonLogicShellFindFn,
) -> isize {
    if v == EeWonLogicShellV::InitComponent {
        if let Some(find) = find {
            for r in dialogs(kind) {
                // SAFETY: the shell's hook on the shell; the texts are NUL-ended statics
                if unsafe { find(logic, a as *mut c_void, r.slot, r.kind, r.name.as_ptr(), r.file.as_ptr(), r.line) }
                    == EE_WON_LOBBY_THROWN
                {
                    return EE_WON_LOBBY_THROWN;
                }
            }
        }
    }
    let r = route(kind, v, a, b, c, d);
    // SAFETY: the contract
    let answer = unsafe { run(rust, r) };
    // Resume answers only whether the C++ leaves the game (a call's own answer is not that)
    if v == EeWonLogicShellV::Resume {
        return (r == Route::Answer(1)) as isize;
    }
    answer
}

/// A completion of the shell `logic` called with `op`: unless it is a checked one and the logic was killed or
/// (`check_tracker`) the op was not its tracked one, the call its member made.
///
/// # Safety
/// As [`ee_won_logicshell_virtual`]; `remove` untracks `op` from `logic` (LobbyLogic::RemoveTrackedOp).
#[no_mangle]
pub unsafe extern "C" fn ee_won_logicshell_done(
    kind: EeWonLogicShellKind,
    rust: *mut c_void,
    done: EeWonLogicShellDone,
    code: isize,
    op: *mut c_void,
    killed: bool,
    check_tracker: bool,
    remove: EeWonRemoveTrackedFn,
    logic: *mut c_void,
) {
    let Some((r, checked)) = done_route(kind, done, code, op as isize) else {
        return;
    };
    // SAFETY: the caller's function on its logic and op
    if checked && !unsafe { ee_won_logic_completion_runs(killed, check_tracker, remove, logic, op) } {
        return;
    }
    // SAFETY: the contract
    unsafe { run(rust, r) };
}

/// The shell's C++ (LobbyLogic.cpp binds it at start-up, R6 WON lobby and network round 40): `new
/// ShellCompletion(logic, done, code)` (its `mCheckTracker` set for a tracked run; no reference taken yet; a
/// `LobbyCompletionBase`, run on the window thread) and the logic's op tracker (`logic->mTracker`'s Rust object).
#[repr(C)]
pub struct EeWonLogicShellCpp {
    pub completion: Option<
        unsafe extern "C" fn(logic: *mut c_void, done: EeWonLogicShellDone, code: i32, tracked: bool) -> *mut c_void,
    >,
    pub tracker: Option<unsafe extern "C" fn(logic: *mut c_void) -> *mut EeWonOpTracker>,
}

static SHELL_CPP: AtomicPtr<EeWonLogicShellCpp> = AtomicPtr::new(std::ptr::null_mut());

/// Binds the shell's C++ (a static table; null leaves the binding as it is).
#[no_mangle]
pub extern "C" fn ee_won_logicshell_bind(cpp: *const EeWonLogicShellCpp) {
    if !cpp.is_null() {
        SHELL_CPP.store(cpp.cast_mut(), Ordering::Release);
    }
}

/// The bound table, in the game only (the network core bound too; the drivers' hosts answer otherwise).
fn shell_cpp() -> Option<&'static EeWonLogicShellCpp> {
    if !net::bound() {
        return None;
    }
    // SAFETY: a bound table lives for the program
    unsafe { SHELL_CPP.load(Ordering::Acquire).as_ref() }
}

/// A new completion of `logic` (null unbound).
fn completion(logic: *mut c_void, done: EeWonLogicShellDone, code: i32, tracked: bool) -> *mut c_void {
    // SAFETY: the table's contract: `logic` is the live shell the caller's object belongs to
    shell_cpp().and_then(|c| c.completion).map_or(std::ptr::null_mut(), |f| unsafe { f(logic, done, code, tracked) })
}

/// `logic->mTracker`'s Rust object (null unbound).
fn tracker(logic: *mut c_void) -> *const EeWonOpTracker {
    // SAFETY: as `completion`
    shell_cpp().and_then(|c| c.tracker).map_or(std::ptr::null(), |f| unsafe { f(logic) })
}

/// `LobbyLogic::RunOp(op, completion, timeout)` (`done` none: a NULL completion) or, `tracked`, `RunTrackedOp`:
/// the op's completion set, the op tracked, run async (`LobbyRunOp`: a 0 timeout is the lobby's 60 s).
fn run_op(logic: *mut c_void, op: isize, done: Option<(EeWonLogicShellDone, i32)>, tracked: bool, timeout: u32) {
    let op = op as *mut c_void;
    let c = done.map_or(std::ptr::null_mut(), |(d, code)| completion(logic, d, code, tracked));
    net::op_set_completion(op, c);
    if tracked {
        let t = tracker(logic);
        if !t.is_null() {
            // SAFETY: the logic's live tracker and the caller's op
            unsafe { ee_won_op_tracker_add(t, op, 0) };
        }
    }
    net::op_run_async(op, if timeout == 0 { 60000 } else { timeout });
}

/// RoutingLogic's host cases of its completions and runs (RoutingLogic.cpp's until R6 WON lobby and network round
/// 40), answered here in the game (`logic` the shell; `None`: the host's).
pub(crate) fn route_op(
    logic: *mut c_void,
    op: crate::routelogic::EeWonRouteLogicOp,
    a: isize,
    b: isize,
    c: isize,
) -> Option<isize> {
    use crate::routelogic::EeWonRouteLogicOp as R;
    use EeWonLogicShellDone as D;
    shell_cpp()?;
    let p = a as *mut c_void;
    match op {
        // `L->RunOp(AO(a), NULL, 0)`
        R::RunOp => run_op(logic, a, None, false, 0),
        // `L->RunOp(AO(a), new ShellCompletion(L, RouteUserStatus), 10000)`
        R::StatusOpRun => run_op(logic, a, Some((D::RouteUserStatus, 0)), false, 10000),
        // `L->RunTrackedOp(AO(a), new ShellCompletion(L, RouteGroups), 0)`
        R::RunTrackedGroups => run_op(logic, a, Some((D::RouteGroups, 0)), true, 0),
        // `RC(a)->SetOpCompletion(b, new ShellCompletion(L, RouteNotify, c))`
        R::ConnSetNotice => {
            net::routeconn_set_completion(p, b as i32, completion(logic, D::RouteNotify, c as i32, false))
        }
        // `RC(a)->SetCloseCompletion(new ShellCompletion(L, RouteClosed))`
        R::ConnSetClose => net::queue_set_close_completion(p, completion(logic, D::RouteClosed, 0, false)),
        // SubscribeDataObject(0xffff, the game data type "G", false, the notice completion of code 21)
        R::ConnSubscribeGames => {
            use ee_won_net::routecpp::{request, Value, EE_WON_RTQ_READ_DATA};
            let n = |num: i64| Value { num, bytes: Vec::new() };
            let values = [n(1), n(0xffff), Value { num: 0, bytes: b"G".to_vec() }, n(0)];
            request(p, EE_WON_RTQ_READ_DATA, &values, completion(logic, D::RouteNotify, 21, false));
        }
        _ => return None,
    }
    Some(0)
}

/// PingLogic's host cases of its completions, runs, receive and tracker (PingLogic.cpp's until R6 WON lobby and
/// network round 40) and InitLogic's tracked runs, answered here in the game (`None`: the host's).
pub(crate) fn logic_op(logic: *mut c_void, op: crate::logics::EeWonLogicOp, a: isize, b: isize) -> Option<isize> {
    use crate::logics::EeWonLogicOp as O;
    use EeWonLogicShellDone as D;
    shell_cpp()?;
    let run = |done: D| {
        run_op(logic, a, Some((done, 0)), true, 0);
        0
    };
    let r = match op {
        // `((UDPManager*)a)->SetDefaultCompletion(new ShellCompletion(logic, PingRecv))`
        O::SetDefaultRecv => {
            net::udp_set_default_completion(a as *mut c_void, completion(logic, D::PingRecv, 0, false));
            0
        }
        // `logic->RunTrackedOp((AsyncOp*)a, new ShellCompletion(logic, PingDone), b)`
        O::RunTracked => {
            run_op(logic, a, Some((D::PingDone, 0)), true, b as u32);
            0
        }
        // InitLogic's `aLogic->RunTrackedOp((AsyncOp*)a, new ShellCompletion(aLogic, Init...), 0)`
        O::RunTrackedDns => run(D::InitDns),
        O::RunTrackedDocument => run(D::InitDocument),
        O::RunTrackedDirectory => run(D::InitDirectory),
        O::RunTrackedVersion => run(D::InitVersion),
        O::RunTrackedFirewall => run(D::InitFirewall),
        // `((UDPManagerRecvOp*)a)->GetMsg()`: its data (NULL for none), its length in `*(size_t*)b`
        O::RecvMsg => {
            let bytes = net::buffer_bytes(net::udp_recv_msg(a as *mut c_void));
            // SAFETY: the logic's length slot
            unsafe { *(b as *mut usize) = bytes.map_or(0, |v| v.len()) };
            bytes.map_or(0, |v| v.as_ptr() as isize)
        }
        // `mTracker.GetNumTrack()`, `mTracker.KillAll()`, a tracked ping op's game (NULL for another op)
        O::NumTracked => {
            let t = tracker(logic);
            // SAFETY: the logic's live tracker
            if t.is_null() {
                0
            } else {
                unsafe { ee_won_op_tracker_len(t) as i32 as isize }
            }
        }
        O::KillTracked => {
            let t = tracker(logic);
            if !t.is_null() {
                // SAFETY: as above
                unsafe { ee_won_op_tracker_kill_all(t) };
            }
            0
        }
        O::TrackedGame => {
            let t = tracker(logic);
            if t.is_null() {
                return Some(0);
            }
            // SAFETY: the logic's live tracker; the op it gives (or null) and its ping body (or null)
            unsafe {
                let o = ee_won_op_tracker_at(t, a as usize);
                let body = ee_won_net::udpops::ee_won_udp_body_of(o, ee_won_net::udpops::EE_WON_UDP_BODY_PING);
                crate::logics::ee_won_pingop_call(body.cast(), crate::logics::EeWonPingOpCall::Field, 5) as isize
            }
        }
        _ => return None,
    };
    Some(r)
}

/// DialogLogic's `RunOp` case, `logic->RunOp((AsyncOp*)a, new ShellCompletion(logic, DlgStatusOp), b)`, in the game
/// (`None`: the host's).
pub(crate) fn dlg_op(logic: *mut c_void, op: crate::dlglogic::EeWonDlgLogicOp, a: isize, b: isize) -> Option<isize> {
    shell_cpp()?;
    match op {
        crate::dlglogic::EeWonDlgLogicOp::RunOp => {
            run_op(logic, a, Some((EeWonLogicShellDone::DlgStatusOp, 0)), false, b as u32);
            Some(0)
        }
        _ => None,
    }
}

/// Frees the kind's Rust object (the shell's destructor).
///
/// # Safety
/// `rust` is the object the shell's maker returned for `kind`, not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_logicshell_free(kind: EeWonLogicShellKind, rust: *mut c_void) {
    // SAFETY (each arm): the contract
    match kind {
        EeWonLogicShellKind::Account | EeWonLogicShellKind::Init | EeWonLogicShellKind::Ping => unsafe {
            ee_won_logic_free(rust as *mut EeWonLogic)
        },
        EeWonLogicShellKind::Dialog => unsafe { ee_won_dlglogic_free(rust as *mut EeWonDlgLogic) },
        EeWonLogicShellKind::Routing => unsafe { ee_won_routelogic_free(rust as *mut EeWonRouteLogic) },
        EeWonLogicShellKind::Staging => unsafe { ee_won_staging_free(rust as *mut EeWonStaging) },
        EeWonLogicShellKind::Lan => unsafe { ee_won_lan_free(rust as *mut EeWonLan) },
    }
}

/// A route as numbers, for the golden driver (won_shell36_test): the target (0 none, 1 logic, 2 dialog, 3 routing,
/// 4 staging, 5 LAN), the call's number, its arguments, the answer of a call-less route, and a completion's
/// `checked`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonLogicShellRoute {
    pub target: i32,
    pub call: u32,
    pub args: [isize; 4],
    pub answer: isize,
    pub checked: bool,
}

fn describe(r: Route, checked: bool) -> EeWonLogicShellRoute {
    let (target, call, args, answer) = match r {
        Route::Answer(x) => (0, 0, [0; 4], x),
        Route::Logic(k, a, b) => (1, k as u32, [a, b, 0, 0], 0),
        Route::Dlg(k, a) => (2, k as u32, [a, 0, 0, 0], 0),
        Route::Routing(k, a, b) => (3, k as u32, [a, b, 0, 0], 0),
        Route::Staging(k, a, b, c, d) => (4, k as u32, [a, b, c, d], 0),
        Route::Lan(k, a, b, c, d) => (5, k as u32, [a, b, c, d], 0),
    };
    EeWonLogicShellRoute { target, call, args, answer, checked }
}

/// [`route`] as numbers.
#[no_mangle]
pub extern "C" fn ee_won_logicshell_route(
    kind: EeWonLogicShellKind,
    v: EeWonLogicShellV,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> EeWonLogicShellRoute {
    describe(route(kind, v, a, b, c, d), false)
}

/// [`done_route`] as numbers (target -1: not this kind's completion).
#[no_mangle]
pub extern "C" fn ee_won_logicshell_done_route(
    kind: EeWonLogicShellKind,
    done: EeWonLogicShellDone,
    code: isize,
    op: isize,
) -> EeWonLogicShellRoute {
    match done_route(kind, done, code, op) {
        Some((r, checked)) => describe(r, checked),
        None => EeWonLogicShellRoute { target: -1, ..Default::default() },
    }
}

/// The `i`th dialog of `kind`'s InitComponent for the golden driver (false past the last).
///
/// # Safety
/// The out pointers are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_logicshell_dialog(
    kind: EeWonLogicShellKind,
    i: usize,
    slot: *mut i32,
    dlg_kind: *mut EeWonLobbyDlgKind,
    name: *mut *const c_char,
    file: *mut *const c_char,
    line: *mut i32,
) -> bool {
    let Some(r) = dialogs(kind).get(i) else {
        return false;
    };
    // SAFETY: the contract
    unsafe {
        *slot = r.slot;
        *dlg_kind = r.kind;
        *name = r.name.as_ptr();
        *file = r.file.as_ptr();
        *line = r.line;
    }
    true
}

#[cfg(test)]
#[path = "logicshell_tests.rs"]
mod tests;
