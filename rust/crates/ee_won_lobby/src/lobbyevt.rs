//! The lobby's events as Rust objects (R6 WONLobby round 18).
//!
//! The lobby's screens and logics talk through events: 43 C++ classes over `LobbyEvent` (WONLobbyEvents.h,
//! LobbyEvents.cpp, LobbySyncGameEvent.cpp), each a few members (numbers, model objects and operations held by a
//! `SmartPtr`, `GUIString`s, addresses, `std::wstring` / `std::string` copies) set by its constructor and read by
//! the event's receivers. They are one Rust object now: an event of a kind (`EeWonLobbyEvtKind`) holds its members
//! in its class's declaration order (`layout`), and the C++ keeps one shell, `LobbyRustEvent : LobbyEvent`, whose
//! destructor frees it. The C++ makers (WONLobbyEvents.h, the original constructors' signatures) pass the
//! constructors' values in member order; the receivers read a member by name (`EeWonLobbyEvtField`).
//!
//! What the members do, as the original's did:
//! - a number (`int`, `bool`, `unsigned short`, `unsigned long`) is the member's value, converted by the maker;
//! - an object (`SmartPtr<T>`: a model object, an operation, a `GUIString`'s `GUIStringData`) is referenced when the
//!   event is made (NULL: none) and given back when it ends; the references go through the event's host (the C++
//!   `RefCount`'s `AddRef` / `Release`), in reverse member order at the end (the C++ destructor's order);
//! - an address (`IPAddr`) is a copy (`ee_won_ipaddr_clone`), freed at the end;
//! - a `std::wstring` / `std::string` is a copy of its bytes.
//!
//! The shells are bound to their events (`ee_won_lobbyevt_bind`) so a Rust receiver can read an event it is handed
//! as a `ComponentEvent*` (`of`).

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::sync::Mutex;

use ee_won_net::ipobj::{ee_won_ipaddr_clone, ee_won_ipaddr_free, EeWonIpAddr};

/// An event's class (one per original class, in WONLobbyEvents.h's order).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum EeWonLobbyEvtKind {
    AcceptClient = 0,
    Alert = 1,
    AnnounceClientLocation = 2,
    AnnounceFriend = 3,
    FriendAction = 4,
    FriendChanged = 5,
    GameAction = 6,
    GameChanged = 7,
    JoinFriendsRoom = 8,
    JoinGame = 9,
    PingGame = 10,
    PlayerChanged = 11,
    PlayerTeamChanged = 12,
    ServerChanged = 13,
    ClientAction = 14,
    GotHttpDocument = 15,
    ChangeDirectConnect = 16,
    ClientChanged = 17,
    ClientModerated = 18,
    GroupChanged = 19,
    GroupInvitation = 20,
    GroupJoinAttempt = 21,
    JoinedRoom = 22,
    RecvChat = 23,
    SetRoomSpecFlags = 24,
    LoginInitStatus = 25,
    SetLoginFields = 26,
    QueryAccountResult = 27,
    Login = 28,
    GetLostPassword = 29,
    GetLostUserName = 30,
    LeftRoom = 31,
    SetChatInput = 32,
    ChatCommandError = 33,
    CreateRoom = 34,
    JoinRoom = 35,
    CreateAccount = 36,
    SyncGameList = 37,
    JumpToRoom = 38,
    QuickPlay = 39,
    SetCrossPromotion = 40,
    UpdateAccount = 41,
    QueryServerGroups = 42,
}

/// A member, by the original's name (`mChangeKind` is `LobbyChangeEvent`'s).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum EeWonLobbyEvtField {
    ChangeKind = 0,
    Client = 1,
    Accepted = 2,
    Kind = 3,
    Value = 4,
    Severity = 5,
    Text = 6,
    Server = 7,
    Room = 8,
    RoomId = 9,
    Handled = 10,
    Friend = 11,
    Location = 12,
    Action = 13,
    Game = 14,
    Flag = 15,
    Op = 16,
    RoomSpec = 17,
    RoomName = 18,
    ModerationKind = 19,
    Duration = 20,
    Comment = 21,
    Group = 22,
    GameName = 23,
    GameId = 24,
    Invite = 25,
    Clients = 26,
    ServerName = 27,
    GroupName = 28,
    ClientId = 29,
    Sender = 30,
    Target = 31,
    Flags = 32,
    Status = 33,
    Description = 34,
    AllowIgnore = 35,
    UserName = 36,
    Password = 37,
    Remember = 38,
    Email = 39,
    Newsletter = 40,
    Forced = 41,
    Command = 42,
    Address = 43,
    GroupId = 44,
    Month = 45,
    Day = 46,
    Year = 47,
    Games = 48,
    GameType = 49,
    Image = 50,
    Link = 51,
    NewPassword = 52,
}

/// What the event's host does to a referenced object (`RefCount`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyEvtRef {
    /// `((RefCount*)obj)->AddRef()`.
    AddRef = 0,
    /// `((RefCount*)obj)->Release()`.
    Release = 1,
}

/// The event's host: a reference taken or given back.
pub type EeWonLobbyEvtHostFn = Option<unsafe extern "C" fn(op: EeWonLobbyEvtRef, obj: isize)>;

/// A member's kind.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Ty {
    /// a number
    Int,
    /// a `SmartPtr` or `GUIString`: an object referenced while the event lives
    Ref,
    /// an `IPAddr`: a copy
    Addr,
    /// a `std::wstring` / `std::string`: a copy of its bytes (two maker values: the data and the byte count)
    Bytes,
}

use EeWonLobbyEvtField as F;
use EeWonLobbyEvtKind as K;
use Ty::{Addr, Bytes, Int, Ref};

/// A class's members in declaration order (a `LobbyChangeEvent`'s change kind first).
pub fn layout(k: K) -> &'static [(F, Ty)] {
    match k {
        K::AcceptClient => &[(F::Client, Ref), (F::Accepted, Int)],
        K::Alert => &[(F::Kind, Int), (F::Value, Int), (F::Severity, Int), (F::Text, Ref)],
        K::AnnounceClientLocation => {
            &[(F::Client, Ref), (F::Server, Ref), (F::Room, Ref), (F::RoomId, Int), (F::Handled, Int)]
        }
        K::AnnounceFriend => &[(F::ChangeKind, Int), (F::Friend, Ref), (F::Location, Int), (F::Handled, Int)],
        K::FriendAction => &[(F::Action, Int), (F::Friend, Ref), (F::Value, Int)],
        K::FriendChanged => &[(F::ChangeKind, Int), (F::Friend, Ref)],
        K::GameAction => &[(F::Action, Int), (F::Game, Ref)],
        K::GameChanged | K::ChangeDirectConnect => &[(F::ChangeKind, Int), (F::Game, Ref)],
        K::JoinFriendsRoom => &[(F::Friend, Ref)],
        K::JoinGame => &[(F::Game, Ref)],
        K::PingGame => &[(F::Game, Ref), (F::Flag, Int)],
        K::PlayerChanged | K::PlayerTeamChanged => &[(F::ChangeKind, Int), (F::Client, Ref)],
        K::ServerChanged => &[(F::ChangeKind, Int), (F::Server, Ref)],
        K::ClientAction => &[(F::Action, Int), (F::RoomSpec, Int), (F::Client, Ref)],
        K::GotHttpDocument => &[(F::Op, Ref)],
        K::ClientChanged => &[(F::ChangeKind, Int), (F::Client, Ref), (F::RoomSpec, Int)],
        K::ClientModerated => &[
            (F::Client, Ref),
            (F::RoomSpec, Int),
            (F::RoomName, Ref),
            (F::ModerationKind, Int),
            (F::Duration, Int),
            (F::Flag, Int),
            (F::Comment, Ref),
        ],
        K::GroupChanged => &[(F::ChangeKind, Int), (F::Server, Ref), (F::Group, Ref)],
        K::GroupInvitation => &[
            (F::Kind, Int),
            (F::GameName, Ref),
            (F::GameId, Int),
            (F::Client, Ref),
            (F::Invite, Int),
            (F::Flag, Int),
            (F::Comment, Ref),
        ],
        K::GroupJoinAttempt => &[(F::Kind, Int), (F::Client, Ref), (F::Comment, Ref)],
        K::JoinedRoom => {
            &[(F::Clients, Ref), (F::ServerName, Ref), (F::GroupName, Ref), (F::ClientId, Int), (F::RoomSpec, Int)]
        }
        K::RecvChat => &[(F::Sender, Ref), (F::Target, Ref), (F::RoomSpec, Int), (F::Flags, Int), (F::Text, Ref)],
        K::SetRoomSpecFlags => &[(F::Flags, Int)],
        K::LoginInitStatus => &[(F::Status, Ref), (F::Description, Ref), (F::AllowIgnore, Int), (F::Kind, Int)],
        K::SetLoginFields => &[(F::UserName, Ref), (F::Password, Ref), (F::Remember, Int)],
        K::QueryAccountResult => &[(F::Email, Ref), (F::Newsletter, Int)],
        K::Login => &[(F::UserName, Bytes), (F::Password, Bytes), (F::Remember, Int)],
        K::GetLostPassword => &[(F::UserName, Bytes)],
        K::GetLostUserName => &[(F::Email, Bytes)],
        K::LeftRoom => &[(F::RoomSpec, Int), (F::Forced, Int), (F::Flag, Int)],
        K::SetChatInput => &[(F::Command, Int), (F::Text, Ref), (F::RoomSpec, Int), (F::Handled, Int)],
        K::ChatCommandError => &[(F::Text, Ref)],
        K::CreateRoom | K::QueryServerGroups => &[(F::Address, Addr)],
        K::JoinRoom => &[(F::Address, Addr), (F::GroupId, Int)],
        K::CreateAccount => &[
            (F::UserName, Bytes),
            (F::Password, Bytes),
            (F::Email, Bytes),
            (F::Month, Int),
            (F::Day, Int),
            (F::Year, Int),
            (F::Newsletter, Int),
        ],
        K::SyncGameList => &[(F::Games, Ref)],
        K::JumpToRoom => &[(F::RoomName, Ref), (F::GroupId, Int), (F::Address, Addr)],
        K::QuickPlay => &[(F::GameType, Int)],
        K::SetCrossPromotion => &[(F::Image, Int), (F::Link, Bytes)],
        K::UpdateAccount => &[(F::Email, Ref), (F::NewPassword, Ref), (F::Newsletter, Int)],
    }
}

/// How many values a kind's maker passes (two for each copied string).
pub fn arg_count(k: K) -> usize {
    layout(k).iter().map(|&(_, t)| if t == Bytes { 2 } else { 1 }).sum()
}

#[derive(Debug)]
enum Val {
    Int(isize),
    Ref(isize),
    Addr(*mut EeWonIpAddr),
    Bytes(Vec<u8>),
}

/// One event: its kind, its members in declaration order, its host and its C++ shell.
pub struct EeWonLobbyEvt {
    kind: K,
    vals: Vec<Val>,
    host: EeWonLobbyEvtHostFn,
    shell: usize,
}

/// The bound shells (`ComponentEvent*` -> event). Events are made and read on the window thread; the map is shared
/// so a driver's threads see the same rule.
static SHELLS: Mutex<BTreeMap<usize, usize>> = Mutex::new(BTreeMap::new());

fn shells() -> std::sync::MutexGuard<'static, BTreeMap<usize, usize>> {
    SHELLS.lock().unwrap_or_else(|e| e.into_inner())
}

impl EeWonLobbyEvt {
    fn refs(&self, op: EeWonLobbyEvtRef, obj: isize) {
        if obj == 0 {
            return;
        }
        if let Some(f) = self.host {
            // SAFETY: the host's contract: `obj` is a live `RefCount` the event holds (or is taking)
            unsafe { f(op, obj) };
        }
    }

    /// The kind.
    pub fn kind(&self) -> K {
        self.kind
    }

    fn slot(&self, f: F) -> Option<usize> {
        layout(self.kind).iter().position(|&(g, _)| g == f)
    }

    /// A member's value: a number, an object (no reference added), an address's `EeWonIpAddr*`, a string's bytes'
    /// address; 0 for a member the class does not have.
    pub fn get(&self, f: F) -> isize {
        match self.slot(f).map(|i| &self.vals[i]) {
            Some(Val::Int(v)) | Some(Val::Ref(v)) => *v,
            Some(Val::Addr(a)) => *a as isize,
            Some(Val::Bytes(b)) => b.as_ptr() as isize,
            None => 0,
        }
    }

    /// A copied string's bytes (empty for any other member).
    pub fn bytes(&self, f: F) -> &[u8] {
        match self.slot(f).map(|i| &self.vals[i]) {
            Some(Val::Bytes(b)) => b,
            _ => &[],
        }
    }

    /// A number member written (`mHandled = true`, CreateAccountEvent's fields); any other member is left alone.
    pub fn set(&mut self, f: F, v: isize) {
        if let Some(i) = self.slot(f) {
            if let Val::Int(old) = &mut self.vals[i] {
                *old = v;
            }
        }
    }

    /// A copied string member written (`std::wstring::operator=`).
    pub fn set_bytes(&mut self, f: F, b: &[u8]) {
        if let Some(i) = self.slot(f) {
            if let Val::Bytes(old) = &mut self.vals[i] {
                old.clear();
                old.extend_from_slice(b);
            }
        }
    }
}

impl Drop for EeWonLobbyEvt {
    /// The C++ destructor: the members in reverse declaration order.
    fn drop(&mut self) {
        while let Some(v) = self.vals.pop() {
            match v {
                Val::Ref(p) => self.refs(EeWonLobbyEvtRef::Release, p),
                // SAFETY: the event's own copy
                Val::Addr(a) => unsafe { ee_won_ipaddr_free(a) },
                Val::Int(_) | Val::Bytes(_) => {}
            }
        }
        if self.shell != 0 {
            shells().remove(&self.shell);
        }
    }
}

/// A new event of `kind` over the maker's `n` values at `args` in member order (`arg_count`): a number, an object
/// (referenced), an `EeWonIpAddr*` (copied), or a string's data and byte count (copied).
///
/// # Safety
/// `args` holds `n` values; each object is NULL or a live `RefCount` the host takes; each address a live value; each
/// string's data holds its count of bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyevt_new(
    kind: EeWonLobbyEvtKind,
    args: *const isize,
    n: usize,
    host: EeWonLobbyEvtHostFn,
) -> *mut EeWonLobbyEvt {
    let args: &[isize] = if args.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(args, n) }
    };
    let mut e = EeWonLobbyEvt { kind, vals: Vec::new(), host, shell: 0 };
    let mut at = 0usize;
    let mut next = || {
        let v = args.get(at).copied().unwrap_or(0);
        at += 1;
        v
    };
    for &(_, t) in layout(kind) {
        let v = match t {
            Int => Val::Int(next()),
            Ref => {
                let p = next();
                e.refs(EeWonLobbyEvtRef::AddRef, p);
                Val::Ref(p)
            }
            Addr => {
                let p = next() as *const EeWonIpAddr;
                // SAFETY: the contract (a live address; NULL gives a default one)
                Val::Addr(if p.is_null() {
                    ee_won_net::ipobj::ee_won_ipaddr_new()
                } else {
                    unsafe { ee_won_ipaddr_clone(p) }
                })
            }
            Bytes => {
                let p = next() as *const u8;
                let len = next() as usize;
                Val::Bytes(if p.is_null() || len == 0 {
                    Vec::new()
                } else {
                    // SAFETY: the contract
                    unsafe { std::slice::from_raw_parts(p, len) }.to_vec()
                })
            }
        };
        e.vals.push(v);
    }
    Box::into_raw(Box::new(e))
}

/// Binds an event to its C++ shell (the `LobbyRustEvent`'s constructor), so `of` finds it.
///
/// # Safety
/// `evt` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyevt_bind(evt: *mut EeWonLobbyEvt, shell: *mut c_void) {
    if evt.is_null() {
        return;
    }
    // SAFETY: the contract
    unsafe { (*evt).shell = shell as usize };
    shells().insert(shell as usize, evt as usize);
}

/// The end of an event (its shell's destructor): its objects given back in reverse member order, its copies freed.
///
/// # Safety
/// `evt` is NULL or live, and not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyevt_free(evt: *mut EeWonLobbyEvt) {
    if !evt.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(evt) });
    }
}

/// A member's value (`EeWonLobbyEvt::get`).
///
/// # Safety
/// `evt` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyevt_get(evt: *const EeWonLobbyEvt, field: EeWonLobbyEvtField) -> isize {
    // SAFETY: the contract
    unsafe { (*evt).get(field) }
}

/// A copied string member's byte count.
///
/// # Safety
/// `evt` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyevt_len(evt: *const EeWonLobbyEvt, field: EeWonLobbyEvtField) -> usize {
    // SAFETY: the contract
    unsafe { (*evt).bytes(field).len() }
}

/// A number member written.
///
/// # Safety
/// `evt` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyevt_set(evt: *mut EeWonLobbyEvt, field: EeWonLobbyEvtField, v: isize) {
    // SAFETY: the contract
    unsafe { (*evt).set(field, v) }
}

/// A copied string member written from `n` bytes at `data`.
///
/// # Safety
/// `evt` is live; `data` holds `n` bytes (or `n` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyevt_set_bytes(
    evt: *mut EeWonLobbyEvt,
    field: EeWonLobbyEvtField,
    data: *const u8,
    n: usize,
) {
    let b: &[u8] = if data.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(data, n) }
    };
    // SAFETY: the contract
    unsafe { (*evt).set_bytes(field, b) }
}

/// The event bound to the C++ shell `shell` (a `ComponentEvent*`), when it is one.
pub fn of(shell: isize) -> Option<*mut EeWonLobbyEvt> {
    shells().get(&(shell as usize)).map(|&e| e as *mut EeWonLobbyEvt)
}

/// How a receiver's read answers: the member's value (a text's `GUIStringData*` with no reference added, as the
/// hosts' `Dat` gave it), a change kind test, or a bit of the member.
#[derive(Clone, Copy)]
enum Read {
    Val(F),
    Kind(isize),
    Bit(F, isize),
}

/// A read of the event `e` (the C++ shell), when it is a Rust event.
fn read(e: isize, r: Read) -> Option<isize> {
    let evt = of(e)?;
    // SAFETY: a bound event is live until its shell's destructor frees (and unbinds) it
    let evt = unsafe { &*evt };
    Some(match r {
        Read::Val(f) => evt.get(f),
        Read::Kind(k) => (evt.get(F::ChangeKind) == k) as isize,
        Read::Bit(f, m) => (evt.get(f) & m != 0) as isize,
    })
}

/// The controls' event ops (lobbyctl.rs; their C++ was LobbyCtrlHost.cpp's and LobbyNetCtrlHost.cpp's member
/// reads), answered for a game control: `None` for an op that is not one, or an event that is not a Rust event.
pub(crate) fn ctl_op(op: crate::lobbyctl::EeWonLobbyCtlOp, a: isize, b: isize) -> Option<isize> {
    use crate::lobbyctl::EeWonLobbyCtlEvField as C;
    use crate::lobbyctl::EeWonLobbyCtlOp as O;
    let r = match op {
        O::EventFlags => Read::Val(F::Flags),
        O::EventRoomSpec => Read::Val(F::RoomSpec),
        O::EventIsAdd => Read::Kind(0),
        O::EventIsDelete => Read::Kind(1),
        O::EventIsModify => Read::Kind(2),
        O::ClientChangedClient | O::PlayerChangedClient => Read::Val(F::Client),
        O::EventGame => Read::Val(F::Game),
        O::EventGames => Read::Val(F::Games),
        O::EvSetHandled => {
            let f = ctl_field(b)?;
            if matches!(f, C::AclHandled | C::AfHandled | C::SciHandled) {
                let evt = of(a)?;
                // SAFETY: as `read`
                unsafe { (*evt).set(F::Handled, 1) };
            }
            return of(a).map(|_| 0);
        }
        O::EvGet => match ctl_field(b)? {
            C::AclHandled | C::AfHandled | C::SciHandled => Read::Val(F::Handled),
            C::AclRoomId => Read::Val(F::RoomId),
            C::AclClient | C::PtcClient | C::GiClient | C::GjaClient | C::CmClient => Read::Val(F::Client),
            C::AclRoom => Read::Val(F::Room),
            C::AclServer => Read::Val(F::Server),
            C::AfLocation => Read::Val(F::Location),
            C::AfFriend | C::FcFriend => Read::Val(F::Friend),
            C::RcSender => Read::Val(F::Sender),
            C::RcTarget => Read::Val(F::Target),
            C::RcRoomSpec | C::JrRoomSpec | C::LrRoomSpec | C::SciRoomSpec | C::CmRoomSpec => Read::Val(F::RoomSpec),
            C::RcText | C::CceText | C::AlText | C::SciText => Read::Val(F::Text),
            C::RcIsTeam => Read::Bit(F::Flags, 256),
            C::RcIsWhisper => Read::Bit(F::Flags, 4),
            C::RcIsWarning => Read::Bit(F::Flags, 16),
            C::RcIsEmote => Read::Bit(F::Flags, 2),
            C::JrClientId => Read::Val(F::ClientId),
            C::JrGroupName => Read::Val(F::GroupName),
            C::JrServerName => Read::Val(F::ServerName),
            C::JrClients => Read::Val(F::Clients),
            C::LrFlag | C::GiFlag | C::CmFlag => Read::Val(F::Flag),
            C::LrForced => Read::Val(F::Forced),
            C::AlKind | C::GiKind | C::GjaKind | C::LisKind => Read::Val(F::Kind),
            C::AlValue => Read::Val(F::Value),
            C::AlSeverity => Read::Val(F::Severity),
            C::SciCommand => Read::Val(F::Command),
            C::GiGameName => Read::Val(F::GameName),
            C::GiGameId => Read::Val(F::GameId),
            C::GiComment | C::GjaComment | C::CmComment => Read::Val(F::Comment),
            C::GiInvite => Read::Val(F::Invite),
            C::CmKind => Read::Val(F::ModerationKind),
            C::CmDuration => Read::Val(F::Duration),
            C::CmRoomName => Read::Val(F::RoomName),
            C::GcGroup => Read::Val(F::Group),
            C::GcServer | C::ScServer => Read::Val(F::Server),
            C::LisStatus => Read::Val(F::Status),
            C::LisDescription => Read::Val(F::Description),
            C::LisAllowIgnore => Read::Val(F::AllowIgnore),
            C::SlfUserName => Read::Val(F::UserName),
            C::SlfPassword => Read::Val(F::Password),
            C::QarEmail => Read::Val(F::Email),
            C::QarNewsletter => Read::Val(F::Newsletter),
            C::GhdOp => Read::Val(F::Op),
            // a WONGUI event's (TextLinkActivatedEvent): the C++'s
            _ => return None,
        },
        _ => return None,
    };
    read(a, r)
}

fn ctl_field(b: isize) -> Option<crate::lobbyctl::EeWonLobbyCtlEvField> {
    use crate::lobbyctl::EeWonLobbyCtlEvField as C;
    if b < 0 || b > C::GhdOp as isize {
        return None;
    }
    // SAFETY: a value in the enum's dense range 0..=GhdOp
    Some(unsafe { std::mem::transmute::<u32, C>(b as u32) })
}

/// The lobby logics' event ops (lobbyhost.rs; LanLogicHost.cpp's and LobbyLogicHost.cpp's member reads).
pub(crate) fn lobby_op(op: crate::lobbyhost::EeWonLobbyOp, a: isize) -> Option<isize> {
    use crate::lobbyhost::EeWonLobbyOp as O;
    let r = match op {
        O::EventDirectIsDelete => Read::Kind(1),
        O::EventDirectIsAdd => Read::Kind(0),
        O::EventDirectGame | O::EventJoinGame | O::EventActionGame => Read::Val(F::Game),
        O::EventClientActionClient => Read::Val(F::Client),
        O::EventClientActionRoomSpec => Read::Val(F::RoomSpec),
        O::EventClientActionAction | O::EventAction => Read::Val(F::Action),
        _ => return None,
    };
    read(a, r)
}

/// RoutingLogic's `EventNum(e, n)` (routelogic.rs; RoutingLogic.cpp's switch): the member numbered `n`, 1..12.
pub(crate) fn route_event_num(e: isize, n: isize) -> Option<isize> {
    const FIELDS: [F; 12] = [
        F::Client,   // AcceptClientEvent
        F::Accepted, // AcceptClientEvent
        F::Friend,   // FriendActionEvent
        F::Action,   // FriendActionEvent
        F::Value,    // FriendActionEvent
        F::GroupId,  // JumpToRoomEvent
        F::GameType, // QuickPlayEvent
        F::RoomSpec, // ClientActionEvent
        F::Client,   // ClientActionEvent
        F::Action,   // ClientActionEvent
        F::GroupId,  // JoinRoomEvent
        F::Friend,   // JoinFriendsRoomEvent
    ];
    if !(1..=12).contains(&n) {
        return None;
    }
    read(e, Read::Val(FIELDS[n as usize - 1]))
}

/// `ctl_op` for a driver (`isize::MIN`: not answered here).
#[no_mangle]
pub extern "C" fn ee_won_lobbyevt_ctl_op(op: crate::lobbyctl::EeWonLobbyCtlOp, a: isize, b: isize) -> isize {
    ctl_op(op, a, b).unwrap_or(isize::MIN)
}

/// `lobby_op` for a driver (`isize::MIN`: not answered here).
#[no_mangle]
pub extern "C" fn ee_won_lobbyevt_lobby_op(op: crate::lobbyhost::EeWonLobbyOp, a: isize) -> isize {
    lobby_op(op, a).unwrap_or(isize::MIN)
}

/// `route_event_num` for a driver (`isize::MIN`: not answered here).
#[no_mangle]
pub extern "C" fn ee_won_lobbyevt_route_num(e: isize, n: isize) -> isize {
    route_event_num(e, n).unwrap_or(isize::MIN)
}

#[cfg(test)]
#[path = "lobbyevt_tests.rs"]
mod tests;
