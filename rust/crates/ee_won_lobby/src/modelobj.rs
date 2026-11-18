//! The lobby's data model as Rust objects (R6 WONLobby round 10): LobbyPlayer, LobbyClient, LobbyClientList,
//! LobbyFriend, LobbyGroup, LobbyGame, LobbyGameList, LobbyServer and LobbyServerList. Each C++ class is a shell
//! (`LobbyModelObject`, WONLobbyModelObj.h: the vtable, the reference count and this object at its first member);
//! its members and methods are here, LobbyModel.cpp's statements in order.
//!
//! - A member that was a C++ object is held as the original held it: a `GUIString` is its `GUIStringData` pointer
//!   (never NULL: the empty string's data), a `SmartPtr` its pointer, each with one reference ([`EeWonModelOp::AddRef`]
//!   / [`EeWonModelOp::Release`], in `SmartPtr::operator=`'s order: the old one released before the new one is
//!   taken); an `IPAddr` is its Rust value (ipobj.rs).
//! - The containers (a client list's clients, a server's rooms and address names, the server list, a game list's
//!   two maps) are ordered vectors under the original comparators (lobbymap.rs's: ids as numbers, addresses by
//!   their six wire bytes, names ignoring case); a removed object is released after the container's borrow ended.
//! - Another model object is reached through its shell (`*(EeWonModel**)(shell + 2 * sizeof(void*))`, asserted by
//!   the C++), so one model object's method calls another's directly, as the original's C++ did.
//! - What is C++ alone (the factories, the game code's hooks, the routing server's records, the persistent ignore
//!   and friend lists, the routing connection's close) is one host's ops.
//!
//! The behaviour is `golden_modelobj.txt`, recorded by `won_modelobj_test` over the original statements.

// the C `long`s are 32-bit on Windows and 64-bit elsewhere: their casts to 32 bits are no-ops on one of them
#![allow(clippy::unnecessary_cast)]

use std::cell::{Cell, RefCell};
use std::cmp::Ordering;
use std::ffi::{c_long, c_ulong, c_void};

use crate::model::{self, EeWonMemberReady, EeWonPopulation, SyncStep};
use crate::staging;
use ee_won_net::ipobj::EeWonIpAddr;
use ee_won_net::strcase;

/// The shell's offset of its model object (after the vtable pointer and `RefCount::mRefCount`, padded).
const SHELL_MODEL: usize = 2 * std::mem::size_of::<usize>();

/// A model object's class.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonModelKind {
    Player = 0,
    Client,
    ClientList,
    Friend,
    Group,
    Game,
    GameList,
    Server,
    ServerList,
}

/// A member read by [`ee_won_model_get`] and written by [`ee_won_model_set`]. Numbers are the member's value; a
/// text is its `GUIStringData*` (read: no reference added; written: the object takes one); an object is its
/// pointer (the same); an address is its `const EeWonIpAddr*` (read: borrowed; written: copied).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonModelField {
    PlayerPing = 0,
    PlayerReady,
    // LobbyClient
    ClientName = 10,
    ClientId,
    ClientFlags,
    ClientWonUserId,
    ClientCdKeyId,
    ClientChatMemberFlags,
    ClientGameMemberFlags,
    ClientChatGroupId,
    ClientGameGroupId,
    ClientIgnored,
    ClientBlocked,
    ClientIsFriend,
    ClientGameCaptain,
    ClientChatCaptain,
    ClientInvited,
    ClientPlayer,
    // LobbyFriend
    FriendName = 40,
    FriendUserId,
    FriendAddress,
    FriendLocation,
    FriendLastSearch,
    FriendFlags,
    FriendLastLocate,
    FriendSearchStart,
    FriendAnonymous,
    // LobbyGroup
    GroupName = 60,
    GroupId,
    GroupCaptainId,
    GroupNumClients,
    GroupMaxClients,
    GroupFlags,
    GroupClients,
    // LobbyGame
    GameName = 80,
    GamePassword,
    GameHasPassword,
    GameInviteOnly,
    GameAskToJoin,
    GameInProgress,
    GameInvited,
    GameIsHost,
    GameShowDetails,
    GameHaveDetails,
    GamePingReplied,
    GameLastSeen,
    GameFiltered,
    GameNumPlayers,
    GameOpenSlots,
    GameType,
    GameSkill,
    GameClients,
    GameAddress,
    GameCaptainId,
    GameGroupId,
    GamePing,
    // LobbyGameList
    GameListType = 120,
    // LobbyServer
    ServerName = 130,
    ServerAddress,
    ServerNumChat,
    ServerNumClients,
    ServerLastPing,
    ServerQuerying,
    ServerClients,
    ServerConnection,
    ServerMyClientId,
}

/// A method ([`ee_won_model_call`]); `a`..`c` as each says. "Held" results carry a reference the caller gives
/// back (a `SmartPtr` return).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonModelCall {
    /// `IsSameTeam((LobbyPlayer*)a)`.
    PlayerIsSameTeam = 0,
    // ---- LobbyClient
    /// `ClearGroup(a, b)`.
    ClientClearGroup = 10,
    ClientInChat,
    ClientInGame,
    /// `ReadRoutingInfo`'s stores: the name `a` (a `GUIStringData*`), then `(const intptr_t*)b`: the flags, the id,
    /// the user id, the CD key id.
    ClientSetRouting,
    ClientUpdateIgnore,
    ClientAnonymous,
    /// `IsMuted(a)`.
    ClientMuted,
    ClientAway,
    ClientAdmin,
    ClientModerator,
    /// `IsCaptain(a)`.
    ClientIsCaptain,
    /// `SetCaptain(a, b)`.
    ClientSetCaptain,
    ClientHasPlayer,
    ClientIsReady,
    ClientPing,
    /// `IsTeammate((LobbyClient*)a)`.
    ClientIsTeammate,
    /// `SetGroupId(a, b)`.
    ClientSetGroupId,
    /// `SetMemberFlags(a, b)`.
    ClientSetMemberFlags,
    // ---- LobbyClientList
    ListClear = 40,
    /// `SyncClients`: `c` routing records, their ids at `(const uint16_t*)a`, their `RoutingClientInfo*`s at
    /// `(const intptr_t*)b`, in the map's order.
    ListSync,
    ListUpdateIgnore,
    /// `AddClient((LobbyClient*)a)`.
    ListAdd,
    /// `FindClient(a)`.
    ListFind,
    /// `RemoveClient(a)`: held.
    ListRemove,
    /// `FindClientByWONId(a)`.
    ListFindByWonId,
    ListClearInvited,
    ListCount,
    /// `ClientAt(a)`.
    ListAt,
    /// `LobbyClientIterator::HasMore` over the cursor `(EeWonModelCursor*)a`.
    ListIterHasMore,
    /// `LobbyClientIterator::Next` over the cursor `a`.
    ListIterNext,
    /// `LobbyClientSetIterator::HasMore` over the cursor `a`.
    ListSetHasMore,
    /// `LobbyClientSetIterator::Next((LobbyClient**)b)` over the cursor `a`: the player.
    ListSetNext,
    // ---- LobbyFriend
    /// `SetLocation(*(const EeWonIpAddr*)a, (GUIStringData*)b, c)`.
    FriendSetLocation = 70,
    /// `ClearLocation()` (`d`: the empty string's data).
    FriendClearLocation,
    /// `SetStatusFromClient((LobbyClient*)a)`.
    FriendSetStatusFromClient,
    /// `StartLocate(a)`.
    FriendStartLocate,
    /// `EndLocate(a)`.
    FriendEndLocate,
    // ---- LobbyGroup
    /// `ReadRoutingInfo`'s stores: the name `a`, then `(const intptr_t*)b`: id, current count, flags, captain,
    /// capacity.
    GroupSetRouting = 80,
    /// `SetMembers`: `c` members, `(const uint16_t*)a` their ids and `(const uint8_t*)b` their flags in the map's
    /// order, over the client list given by `GroupSetMembersList` first.
    GroupSetMembers,
    /// The client list `SetMembers` looks the members up in (`(LobbyClientList*)a`; NULL: none).
    GroupSetMembersList,
    /// `AddClient((LobbyClient*)a, b)`.
    GroupAdd,
    /// `RemoveClient(a)`: held.
    GroupRemove,
    /// `SetCaptain(a, (LobbyClient**)b)`.
    GroupSetCaptain,
    GroupCaptain,
    /// `FindClient(a)`.
    GroupFind,
    /// The room's kind: bit 0 password, 1 ask to join, 2 invite only, 3 chat, 4 public chat.
    GroupKind,
    // ---- LobbyGame
    /// `CopyFrom((LobbyGame*)a)`.
    GameCopyFrom = 100,
    /// `FindClient(a)`.
    GameFindClient,
    /// `MatchesRoomSpec(a)`.
    GameMatchesRoomSpec,
    GamePinging,
    GameQuerying,
    /// `RemoveClient((LobbyClient*)a)`.
    GameRemoveClient,
    GameIsFull,
    GameJoinStatus,
    GameClearPlayers,
    /// `IsSameGame((LobbyGame*)a)`.
    GameIsSameGame,
    /// `ReadGroup((LobbyGroup*)a)`.
    GameReadGroup,
    /// `HandleHostGame((LobbyClient*)a)`.
    GameHandleHost,
    GameAllReady,
    GameCanStart,
    /// `ReadSummary`'s stores: `(const EeWonGameSummary*)a`'s fields `read` names, the name `b` (a
    /// `GUIStringData*`, with bit 4).
    GameStoreSummary,
    // ---- LobbyGameList
    GameListClear = 130,
    /// `AddGame((LobbyGame*)a)`.
    GameListAdd,
    /// `GetOrCreateGame(a, (bool*)b)` (by id).
    GameListGetOrCreateId,
    /// `GetOrCreateGame(*(const EeWonIpAddr*)a, (bool*)b)`.
    GameListGetOrCreateAddr,
    /// `GetGame(a)` (by id).
    GameListGetId,
    /// `GetGame(*(const EeWonIpAddr*)a)`.
    GameListGetAddr,
    /// `RemoveGame(a)` (by id): held.
    GameListRemoveId,
    /// `RemoveGame(*(const EeWonIpAddr*)a)`: held.
    GameListRemoveAddr,
    GameListRewind,
    GameListHasMore,
    GameListNext,
    GameListIsEmpty,
    /// `RemoveStaleGames(a)` at the time `b` (`time(NULL)`).
    GameListRemoveStale,
    // ---- LobbyServer
    /// `HasAddress`: `b` bytes at `(const char*)a`.
    ServerHasAddress = 150,
    /// `SyncClients` (as `ListSync`, then the clients' ignore states).
    ServerSyncClients,
    ServerUpdateFriends,
    /// `ClearFriendLocations` at the time `a` (`d`: the empty string's data).
    ServerClearFriendLocations,
    /// `SyncGroups`: `c & 0xffffffff` rooms, ids at `a`, `RoutingGroupInfo*`s at `b`; `c >> 32`: update the clients.
    ServerSyncGroups,
    /// `AddClient((RoutingClientInfo*)a)`.
    ServerAddClient,
    /// `AddClientToGroup((LobbyClient*)a, b, (LobbyGroup*)c)`.
    ServerAddClientToGroup,
    /// `AddGroup((RoutingGroupInfo*)a)`.
    ServerAddGroup,
    /// `RemoveGroup(a)`: held.
    ServerRemoveGroup,
    /// `RemoveClient(a)`: held.
    ServerRemoveClient,
    /// `RemoveClientFromGroup(a, (LobbyGroup*)b)`: held.
    ServerRemoveClientFromGroup,
    /// `GetClient(a)`.
    ServerGetClient,
    /// `FindClientByWONId(a)`.
    ServerFindByWonId,
    ServerMyClient,
    /// `GetGroup(a)`.
    ServerGroup,
    ServerNumClientsNow,
    ServerConnected,
    ServerClearGroups,
    ServerGroupCount,
    /// `GroupAt(a)`.
    ServerGroupAt,
    /// `ReadDirEntity`'s population (`_C`): `a`.
    ServerSetListed,
    /// `ReadDirEntity`'s address names (`_L`): the set cleared.
    ServerAddressesClear,
    /// One address name: `b` bytes at `a`.
    ServerAddressAdd,
    /// `Disconnect()` at the time `a` (`d`: the empty string's data).
    ServerDisconnect,
    // ---- LobbyServerList
    /// `UpdateFromDirectory`'s entry: the server at `*(const EeWonIpAddr*)a`, made when new.
    ServerListInsert = 190,
    /// `FindServer(*(const EeWonIpAddr*)a)`.
    ServerListFind,
    ServerListCount,
    /// `ServerAt(a)`.
    ServerListAt,
    /// `AddressAt(a)`: a borrowed `const EeWonIpAddr*`.
    ServerListAddressAt,
}

/// The C++ the model calls (WONLobby/LobbyModelHost.cpp's `ModelHost`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonModelOp {
    /// `((RefCount*)a)->AddRef()`.
    AddRef = 0,
    /// `((RefCount*)a)->Release()`.
    Release,
    /// `new LobbyClient` (held).
    NewClient,
    /// `new LobbyClientList` (held).
    NewClientList,
    /// `new LobbyGroup` (held).
    NewGroup,
    /// `new LobbyServer` (held).
    NewServer,
    /// `LobbyGame::CreateGame(a)` (held).
    NewGame,
    /// `LobbyPlayer::CreatePlayer()` (held).
    NewPlayer,
    /// `((LobbyClient*)a)->ReadRoutingInfo((RoutingClientInfo*)b)`.
    ClientReadRouting,
    /// `((LobbyGroup*)a)->ReadRoutingInfo((RoutingGroupInfo*)b, (LobbyClientList*)c)`.
    GroupReadRouting,
    /// `LobbyIsNameIgnored(GUIString((GUIStringData*)a))`.
    NameIgnored,
    /// `LobbyIsCDKeyBlocked(a)`.
    CdKeyBlocked,
    /// `LobbyFindFriend(a)`.
    FindFriend,
    /// `((LobbyPlayer*)a)->IsTeammate((LobbyPlayer*)b)` (the virtual).
    PlayerIsTeammate,
    /// `((LobbyGame*)a)->CopyFromHook((LobbyGame*)b)`.
    GameCopyFromHook,
    /// `((LobbyGame*)a)->HandleHostGameHook((LobbyClient*)b)`.
    GameHostHook,
    /// `((LobbyGame*)a)->HandlePlayerLeftHook((LobbyClient*)b, (LobbyPlayer*)c)`.
    GamePlayerLeftHook,
    /// `((LobbyGame*)a)->GetJoinStatusHook()`.
    GameJoinStatusHook,
    /// `((LobbyGame*)a)->CanStartGameHook()`.
    GameCanStartHook,
    /// `((RoutingConnection*)a)->QueueClose(-1)`.
    ConnectionClose,
    /// `new GUIStringData(text)` of `b` UTF-16 units at `a` (held): a routing record's name (net round 30: the
    /// records are read here, no C++ record is made).
    TextOfUnits,
    /// `((LobbyGame*)a)->ReadSummaryBuffer(buffer)` over `c` bytes at `b`, NULL (`b` 0) for an op or object without
    /// a buffer.
    GameReadSummaryBytes,
}

/// The host: `ModelHost(op, a, b, c)`.
pub type EeWonModelHost = unsafe extern "C" fn(op: EeWonModelOp, a: isize, b: isize, c: isize) -> isize;

/// A walk over a client list as the original's map iterator walked (`LobbyClientIterator`,
/// `LobbyClientSetIterator`): the next client after the last one returned.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeWonModelCursor {
    pub started: bool,
    /// the set iterator's: a client with a player is found and not yet returned
    pub ready: bool,
    pub last: u16,
}

// ---- the containers

/// A `std::map<unsigned short, SmartPtr<T> >`.
#[derive(Default)]
struct IdMap(RefCell<Vec<(u16, isize)>>);

impl IdMap {
    fn len(&self) -> usize {
        self.0.borrow().len()
    }
    fn search(&self, k: u16) -> Result<usize, usize> {
        self.0.borrow().binary_search_by(|e| e.0.cmp(&k))
    }
    fn get(&self, k: u16) -> isize {
        match self.search(k) {
            Ok(i) => self.0.borrow()[i].1,
            Err(_) => 0,
        }
    }
    fn at(&self, i: usize) -> isize {
        self.0.borrow().get(i).map_or(0, |e| e.1)
    }
    fn key_at(&self, i: usize) -> u16 {
        self.0.borrow().get(i).map_or(0, |e| e.0)
    }
    fn keys(&self) -> Vec<u16> {
        self.0.borrow().iter().map(|e| e.0).collect()
    }
    fn objs(&self) -> Vec<isize> {
        self.0.borrow().iter().map(|e| e.1).collect()
    }
    /// `insert(value_type(k, NULL))`: the position and whether it is new.
    fn insert(&self, k: u16) -> (usize, bool) {
        match self.search(k) {
            Ok(i) => (i, false),
            Err(i) => {
                self.0.borrow_mut().insert(i, (k, 0));
                (i, true)
            }
        }
    }
    fn upper(&self, k: u16) -> usize {
        self.0.borrow().partition_point(|e| e.0 <= k)
    }
    /// The entry's object replaced (the caller added the new one's reference): the old one to release.
    #[must_use]
    fn replace(&self, i: usize, obj: isize) -> isize {
        std::mem::replace(&mut self.0.borrow_mut()[i].1, obj)
    }
    #[must_use]
    fn take_at(&self, i: usize) -> isize {
        let mut v = self.0.borrow_mut();
        if i < v.len() {
            v.remove(i).1
        } else {
            0
        }
    }
    #[must_use]
    fn drain_from(&self, i: usize) -> Vec<isize> {
        let mut v = self.0.borrow_mut();
        if i < v.len() {
            v.drain(i..).map(|e| e.1).collect()
        } else {
            Vec::new()
        }
    }
}

/// A `std::map<IPAddr, SmartPtr<T> >` (ordered by the six wire bytes).
#[derive(Default)]
struct AddrMap(RefCell<Vec<(EeWonIpAddr, isize)>>);

impl AddrMap {
    fn len(&self) -> usize {
        self.0.borrow().len()
    }
    fn search(&self, k: &EeWonIpAddr) -> Result<usize, usize> {
        self.0.borrow().binary_search_by(|e| e.0.compare(k))
    }
    fn get(&self, k: &EeWonIpAddr) -> isize {
        match self.search(k) {
            Ok(i) => self.0.borrow()[i].1,
            Err(_) => 0,
        }
    }
    fn at(&self, i: usize) -> isize {
        self.0.borrow().get(i).map_or(0, |e| e.1)
    }
    fn insert(&self, k: &EeWonIpAddr) -> (usize, bool) {
        match self.search(k) {
            Ok(i) => (i, false),
            Err(i) => {
                self.0.borrow_mut().insert(i, (k.clone(), 0));
                (i, true)
            }
        }
    }
    fn upper(&self, k: &EeWonIpAddr) -> usize {
        self.0.borrow().partition_point(|e| e.0.compare(k) != Ordering::Greater)
    }
    #[must_use]
    fn replace(&self, i: usize, obj: isize) -> isize {
        std::mem::replace(&mut self.0.borrow_mut()[i].1, obj)
    }
    #[must_use]
    fn take_at(&self, i: usize) -> isize {
        let mut v = self.0.borrow_mut();
        if i < v.len() {
            v.remove(i).1
        } else {
            0
        }
    }
    #[must_use]
    fn drain(&self) -> Vec<isize> {
        self.0.borrow_mut().drain(..).map(|e| e.1).collect()
    }
}

// ---- the classes' members

struct Player {
    ping: Cell<u16>,
    ready: Cell<bool>,
}

struct Client {
    name: Cell<isize>,
    id: Cell<u16>,
    flags: Cell<c_ulong>,
    won_user_id: Cell<c_ulong>,
    cd_key_id: Cell<c_ulong>,
    chat_member: Cell<u8>,
    game_member: Cell<u8>,
    chat_group: Cell<u16>,
    game_group: Cell<u16>,
    ignored: Cell<bool>,
    blocked: Cell<bool>,
    is_friend: Cell<bool>,
    game_captain: Cell<bool>,
    chat_captain: Cell<bool>,
    invited: Cell<bool>,
    player: Cell<isize>,
}

struct ClientList {
    clients: IdMap,
}

struct Friend {
    name: Cell<isize>,
    user_id: Cell<c_ulong>,
    address: RefCell<EeWonIpAddr>,
    location: Cell<isize>,
    last_search: Cell<c_ulong>,
    flags: Cell<u8>,
    last_locate: Cell<c_ulong>,
    search_start: Cell<c_ulong>,
    anonymous: Cell<bool>,
}

struct Group {
    name: Cell<isize>,
    id: Cell<u16>,
    captain: Cell<u16>,
    num_clients: Cell<c_ulong>,
    max_clients: Cell<c_ulong>,
    flags: Cell<c_ulong>,
    clients: Cell<isize>,
    /// `SetMembers`' lookup list (the call's argument, for the next `GroupSetMembers`)
    members_from: Cell<isize>,
}

struct Game {
    name: Cell<isize>,
    password: Cell<isize>,
    has_password: Cell<bool>,
    invite_only: Cell<bool>,
    ask_to_join: Cell<bool>,
    in_progress: Cell<bool>,
    invited: Cell<bool>,
    is_host: Cell<bool>,
    show_details: Cell<bool>,
    have_details: Cell<bool>,
    ping_replied: Cell<bool>,
    last_seen: Cell<c_long>,
    filtered: Cell<bool>,
    num_players: Cell<c_long>,
    open_slots: Cell<c_long>,
    game_type: Cell<c_long>,
    skill: Cell<c_long>,
    clients: Cell<isize>,
    address: RefCell<EeWonIpAddr>,
    captain: Cell<u16>,
    group: Cell<u16>,
    ping: Cell<u16>,
}

struct GameList {
    ty: Cell<c_long>,
    id_games: IdMap,
    addr_games: AddrMap,
    id_cursor: (Cell<bool>, Cell<u16>),
    addr_cursor: (Cell<bool>, RefCell<EeWonIpAddr>),
}

struct Server {
    name: Cell<isize>,
    addresses: RefCell<Vec<Vec<u8>>>,
    address: RefCell<EeWonIpAddr>,
    num_chat: Cell<i32>,
    num_clients: Cell<i32>,
    last_ping: Cell<c_ulong>,
    querying: Cell<bool>,
    groups: IdMap,
    clients: Cell<isize>,
    connection: Cell<isize>,
    my_client: Cell<u16>,
}

struct ServerList {
    servers: AddrMap,
}

enum Body {
    Player(Player),
    Client(Client),
    ClientList(ClientList),
    Friend(Friend),
    Group(Group),
    Game(Game),
    GameList(GameList),
    Server(Server),
    ServerList(ServerList),
}

/// One model object (see the module).
pub struct EeWonModel {
    shell: isize,
    host: EeWonModelHost,
    body: Body,
}

/// The model object of a C++ shell (NULL: none).
///
/// # Safety
/// `shell` is NULL or a live `LobbyModelObject`.
unsafe fn of<'a>(shell: isize) -> Option<&'a EeWonModel> {
    if shell == 0 {
        return None;
    }
    // SAFETY: the shell's first member (the C++ asserts the offset)
    unsafe { (*((shell as usize + SHELL_MODEL) as *const *const EeWonModel)).as_ref() }
}

macro_rules! body {
    ($fn:ident, $variant:ident, $ty:ty) => {
        fn $fn(&self) -> &$ty {
            match &self.body {
                Body::$variant(b) => b,
                _ => panic!(concat!("lobby model: not a ", stringify!($variant))),
            }
        }
    };
}

fn b(v: bool) -> isize {
    v as isize
}

impl EeWonModel {
    body!(player, Player, Player);
    body!(client, Client, Client);
    body!(list, ClientList, ClientList);
    body!(friend, Friend, Friend);
    body!(group, Group, Group);
    body!(game, Game, Game);
    body!(game_list, GameList, GameList);
    body!(server, Server, Server);
    body!(server_list, ServerList, ServerList);

    fn op(&self, op: EeWonModelOp, a: isize, b: isize, c: isize) -> isize {
        // SAFETY: the host's contract
        unsafe { (self.host)(op, a, b, c) }
    }
    fn add_ref(&self, p: isize) {
        if p != 0 {
            self.op(EeWonModelOp::AddRef, p, 0, 0);
        }
    }
    fn release(&self, p: isize) {
        if p != 0 {
            self.op(EeWonModelOp::Release, p, 0, 0);
        }
    }
    fn release_all(&self, objs: Vec<isize>) {
        for o in objs {
            self.release(o);
        }
    }
    /// `SmartPtr::operator=`: the old one released, then the new one taken.
    fn assign(&self, slot: &Cell<isize>, v: isize) {
        let old = slot.get();
        if old != v {
            self.release(old);
            self.add_ref(v);
            slot.set(v);
        }
    }

    fn new(shell: isize, kind: EeWonModelKind, host: EeWonModelHost, empty: isize) -> EeWonModel {
        let text = || Cell::new(empty);
        let body = match kind {
            EeWonModelKind::Player => Body::Player(Player { ping: Cell::new(0xfffe), ready: Cell::new(false) }),
            EeWonModelKind::Client => Body::Client(Client {
                name: text(),
                id: Cell::new(0xffff),
                flags: Cell::new(0),
                won_user_id: Cell::new(0),
                cd_key_id: Cell::new(0),
                chat_member: Cell::new(0),
                game_member: Cell::new(0),
                chat_group: Cell::new(0xffff),
                game_group: Cell::new(0xffff),
                ignored: Cell::new(false),
                blocked: Cell::new(false),
                is_friend: Cell::new(false),
                game_captain: Cell::new(false),
                chat_captain: Cell::new(false),
                invited: Cell::new(false),
                player: Cell::new(0),
            }),
            EeWonModelKind::ClientList => Body::ClientList(ClientList { clients: IdMap::default() }),
            EeWonModelKind::Friend => Body::Friend(Friend {
                name: text(),
                user_id: Cell::new(0),
                address: RefCell::new(EeWonIpAddr::default()),
                location: text(),
                last_search: Cell::new(0),
                flags: Cell::new(0),
                last_locate: Cell::new(0),
                search_start: Cell::new(0),
                anonymous: Cell::new(false),
            }),
            EeWonModelKind::Group => Body::Group(Group {
                name: text(),
                id: Cell::new(0xffff),
                captain: Cell::new(0xffff),
                num_clients: Cell::new(0),
                max_clients: Cell::new(0),
                flags: Cell::new(0),
                clients: Cell::new(0),
                members_from: Cell::new(0),
            }),
            EeWonModelKind::Game => Body::Game(Game {
                name: text(),
                password: text(),
                has_password: Cell::new(false),
                invite_only: Cell::new(false),
                ask_to_join: Cell::new(false),
                in_progress: Cell::new(false),
                invited: Cell::new(false),
                is_host: Cell::new(false),
                show_details: Cell::new(false),
                have_details: Cell::new(false),
                ping_replied: Cell::new(false),
                last_seen: Cell::new(0),
                filtered: Cell::new(false),
                num_players: Cell::new(0),
                open_slots: Cell::new(0),
                game_type: Cell::new(0),
                skill: Cell::new(0),
                clients: Cell::new(0),
                address: RefCell::new(EeWonIpAddr::default()),
                captain: Cell::new(0xffff),
                group: Cell::new(0xffff),
                ping: Cell::new(0xfffe),
            }),
            EeWonModelKind::GameList => Body::GameList(GameList {
                ty: Cell::new(0),
                id_games: IdMap::default(),
                addr_games: AddrMap::default(),
                id_cursor: (Cell::new(false), Cell::new(0)),
                addr_cursor: (Cell::new(false), RefCell::new(EeWonIpAddr::default())),
            }),
            EeWonModelKind::Server => Body::Server(Server {
                name: text(),
                addresses: RefCell::new(Vec::new()),
                address: RefCell::new(EeWonIpAddr::default()),
                num_chat: Cell::new(0),
                num_clients: Cell::new(0),
                last_ping: Cell::new(0),
                querying: Cell::new(false),
                groups: IdMap::default(),
                clients: Cell::new(0),
                connection: Cell::new(0),
                my_client: Cell::new(0xffff),
            }),
            EeWonModelKind::ServerList => Body::ServerList(ServerList { servers: AddrMap::default() }),
        };
        EeWonModel { shell, host, body }
    }

    /// The constructor's references (the empty names) and objects (a group's and a server's client list).
    fn construct(&self, empty: isize) {
        let texts = match &self.body {
            Body::Client(_) | Body::Group(_) | Body::Server(_) => 1,
            Body::Friend(_) | Body::Game(_) => 2,
            _ => 0,
        };
        for _ in 0..texts {
            self.add_ref(empty);
        }
        match &self.body {
            // `mClients = new LobbyClientList` (the op's reference is the member's)
            Body::Group(g) => g.clients.set(self.op(EeWonModelOp::NewClientList, 0, 0, 0)),
            Body::Server(s) => s.clients.set(self.op(EeWonModelOp::NewClientList, 0, 0, 0)),
            _ => {}
        }
    }

    /// The destructor's references, in reverse member order.
    fn held(&self) -> Vec<isize> {
        match &self.body {
            Body::Player(_) => Vec::new(),
            Body::Client(c) => vec![c.player.get(), c.name.get()],
            Body::ClientList(l) => l.clients.objs(),
            Body::Friend(f) => vec![f.location.get(), f.name.get()],
            Body::Group(g) => vec![g.clients.get(), g.name.get()],
            Body::Game(g) => vec![g.clients.get(), g.password.get(), g.name.get()],
            // the address map is the later member: destroyed first
            Body::GameList(l) => {
                let mut v = l.addr_games.drain();
                v.extend(l.id_games.objs());
                v
            }
            Body::Server(s) => {
                let mut v = vec![s.connection.get(), s.clients.get()];
                v.extend(s.groups.objs());
                v.push(s.name.get());
                v
            }
            Body::ServerList(l) => l.servers.drain(),
        }
    }

    // ---- the members

    fn get(&self, f: EeWonModelField) -> isize {
        use EeWonModelField as F;
        match (&self.body, f) {
            (Body::Player(p), F::PlayerPing) => p.ping.get() as isize,
            (Body::Player(p), F::PlayerReady) => b(p.ready.get()),
            (Body::Client(c), _) => match f {
                F::ClientName => c.name.get(),
                F::ClientId => c.id.get() as isize,
                F::ClientFlags => c.flags.get() as isize,
                F::ClientWonUserId => c.won_user_id.get() as isize,
                F::ClientCdKeyId => c.cd_key_id.get() as isize,
                F::ClientChatMemberFlags => c.chat_member.get() as isize,
                F::ClientGameMemberFlags => c.game_member.get() as isize,
                F::ClientChatGroupId => c.chat_group.get() as isize,
                F::ClientGameGroupId => c.game_group.get() as isize,
                F::ClientIgnored => b(c.ignored.get()),
                F::ClientBlocked => b(c.blocked.get()),
                F::ClientIsFriend => b(c.is_friend.get()),
                F::ClientGameCaptain => b(c.game_captain.get()),
                F::ClientChatCaptain => b(c.chat_captain.get()),
                F::ClientInvited => b(c.invited.get()),
                F::ClientPlayer => c.player.get(),
                _ => 0,
            },
            (Body::Friend(r), _) => match f {
                F::FriendName => r.name.get(),
                F::FriendUserId => r.user_id.get() as isize,
                F::FriendAddress => r.address.as_ptr() as isize,
                F::FriendLocation => r.location.get(),
                F::FriendLastSearch => r.last_search.get() as isize,
                F::FriendFlags => r.flags.get() as isize,
                F::FriendLastLocate => r.last_locate.get() as isize,
                F::FriendSearchStart => r.search_start.get() as isize,
                F::FriendAnonymous => b(r.anonymous.get()),
                _ => 0,
            },
            (Body::Group(g), _) => match f {
                F::GroupName => g.name.get(),
                F::GroupId => g.id.get() as isize,
                F::GroupCaptainId => g.captain.get() as isize,
                F::GroupNumClients => g.num_clients.get() as isize,
                F::GroupMaxClients => g.max_clients.get() as isize,
                F::GroupFlags => g.flags.get() as isize,
                F::GroupClients => g.clients.get(),
                _ => 0,
            },
            (Body::Game(g), _) => match f {
                F::GameName => g.name.get(),
                F::GamePassword => g.password.get(),
                F::GameHasPassword => b(g.has_password.get()),
                F::GameInviteOnly => b(g.invite_only.get()),
                F::GameAskToJoin => b(g.ask_to_join.get()),
                F::GameInProgress => b(g.in_progress.get()),
                F::GameInvited => b(g.invited.get()),
                F::GameIsHost => b(g.is_host.get()),
                F::GameShowDetails => b(g.show_details.get()),
                F::GameHaveDetails => b(g.have_details.get()),
                F::GamePingReplied => b(g.ping_replied.get()),
                F::GameLastSeen => g.last_seen.get() as isize,
                F::GameFiltered => b(g.filtered.get()),
                F::GameNumPlayers => g.num_players.get() as isize,
                F::GameOpenSlots => g.open_slots.get() as isize,
                F::GameType => g.game_type.get() as isize,
                F::GameSkill => g.skill.get() as isize,
                F::GameClients => g.clients.get(),
                F::GameAddress => g.address.as_ptr() as isize,
                F::GameCaptainId => g.captain.get() as isize,
                F::GameGroupId => g.group.get() as isize,
                F::GamePing => g.ping.get() as isize,
                _ => 0,
            },
            (Body::GameList(l), F::GameListType) => l.ty.get() as isize,
            (Body::Server(s), _) => match f {
                F::ServerName => s.name.get(),
                F::ServerAddress => s.address.as_ptr() as isize,
                F::ServerNumChat => s.num_chat.get() as isize,
                F::ServerNumClients => s.num_clients.get() as isize,
                F::ServerLastPing => s.last_ping.get() as isize,
                F::ServerQuerying => b(s.querying.get()),
                F::ServerClients => s.clients.get(),
                F::ServerConnection => s.connection.get(),
                F::ServerMyClientId => s.my_client.get() as isize,
                _ => 0,
            },
            _ => 0,
        }
    }

    /// # Safety
    /// An address field's `v` is a live `EeWonIpAddr`.
    unsafe fn set(&self, f: EeWonModelField, v: isize) {
        use EeWonModelField as F;
        // SAFETY: the caller's contract
        let addr = || unsafe { (*(v as *const EeWonIpAddr)).clone() };
        match (&self.body, f) {
            (Body::Player(p), F::PlayerPing) => p.ping.set(v as u16),
            (Body::Player(p), F::PlayerReady) => p.ready.set(v != 0),
            (Body::Client(c), _) => match f {
                F::ClientName => self.assign(&c.name, v),
                F::ClientId => c.id.set(v as u16),
                F::ClientFlags => c.flags.set(v as c_ulong),
                F::ClientWonUserId => c.won_user_id.set(v as c_ulong),
                F::ClientCdKeyId => c.cd_key_id.set(v as c_ulong),
                F::ClientChatMemberFlags => c.chat_member.set(v as u8),
                F::ClientGameMemberFlags => c.game_member.set(v as u8),
                F::ClientChatGroupId => c.chat_group.set(v as u16),
                F::ClientGameGroupId => c.game_group.set(v as u16),
                F::ClientIgnored => c.ignored.set(v != 0),
                F::ClientBlocked => c.blocked.set(v != 0),
                F::ClientIsFriend => c.is_friend.set(v != 0),
                F::ClientGameCaptain => c.game_captain.set(v != 0),
                F::ClientChatCaptain => c.chat_captain.set(v != 0),
                F::ClientInvited => c.invited.set(v != 0),
                F::ClientPlayer => self.assign(&c.player, v),
                _ => {}
            },
            (Body::Friend(r), _) => match f {
                F::FriendName => self.assign(&r.name, v),
                F::FriendUserId => r.user_id.set(v as c_ulong),
                F::FriendAddress => *r.address.borrow_mut() = addr(),
                F::FriendLocation => self.assign(&r.location, v),
                F::FriendLastSearch => r.last_search.set(v as c_ulong),
                F::FriendFlags => r.flags.set(v as u8),
                F::FriendLastLocate => r.last_locate.set(v as c_ulong),
                F::FriendSearchStart => r.search_start.set(v as c_ulong),
                F::FriendAnonymous => r.anonymous.set(v != 0),
                _ => {}
            },
            (Body::Group(g), _) => match f {
                F::GroupName => self.assign(&g.name, v),
                F::GroupId => g.id.set(v as u16),
                F::GroupCaptainId => g.captain.set(v as u16),
                F::GroupNumClients => g.num_clients.set(v as c_ulong),
                F::GroupMaxClients => g.max_clients.set(v as c_ulong),
                F::GroupFlags => g.flags.set(v as c_ulong),
                F::GroupClients => self.assign(&g.clients, v),
                _ => {}
            },
            (Body::Game(g), _) => match f {
                F::GameName => self.assign(&g.name, v),
                F::GamePassword => self.assign(&g.password, v),
                F::GameHasPassword => g.has_password.set(v != 0),
                F::GameInviteOnly => g.invite_only.set(v != 0),
                F::GameAskToJoin => g.ask_to_join.set(v != 0),
                F::GameInProgress => g.in_progress.set(v != 0),
                F::GameInvited => g.invited.set(v != 0),
                F::GameIsHost => g.is_host.set(v != 0),
                F::GameShowDetails => g.show_details.set(v != 0),
                F::GameHaveDetails => g.have_details.set(v != 0),
                F::GamePingReplied => g.ping_replied.set(v != 0),
                F::GameLastSeen => g.last_seen.set(v as c_long),
                F::GameFiltered => g.filtered.set(v != 0),
                F::GameNumPlayers => g.num_players.set(v as c_long),
                F::GameOpenSlots => g.open_slots.set(v as c_long),
                F::GameType => g.game_type.set(v as c_long),
                F::GameSkill => g.skill.set(v as c_long),
                F::GameClients => self.assign(&g.clients, v),
                F::GameAddress => *g.address.borrow_mut() = addr(),
                F::GameCaptainId => g.captain.set(v as u16),
                F::GameGroupId => g.group.set(v as u16),
                F::GamePing => g.ping.set(v as u16),
                _ => {}
            },
            (Body::GameList(l), F::GameListType) => l.ty.set(v as c_long),
            (Body::Server(s), _) => match f {
                F::ServerName => self.assign(&s.name, v),
                F::ServerAddress => *s.address.borrow_mut() = addr(),
                F::ServerNumChat => s.num_chat.set(v as i32),
                F::ServerNumClients => s.num_clients.set(v as i32),
                F::ServerLastPing => s.last_ping.set(v as c_ulong),
                F::ServerQuerying => s.querying.set(v != 0),
                F::ServerClients => self.assign(&s.clients, v),
                F::ServerConnection => self.assign(&s.connection, v),
                F::ServerMyClientId => s.my_client.set(v as u16),
                _ => {}
            },
            _ => {}
        }
    }
}

// ---- the methods (each the original's statements; a model object reached through its shell)

/// The model object of a shell the original dereferenced (a NULL there was a crash).
fn must<'a>(shell: isize) -> &'a EeWonModel {
    // SAFETY: the callers pass live shells
    unsafe { of(shell) }.expect("lobby model: a NULL object")
}

impl EeWonModel {
    // ---- LobbyPlayer

    /// `IsSameTeam`: this one, else the virtual `IsTeammate`.
    fn player_same_team(&self, other: isize) -> bool {
        if self.shell == other {
            return true;
        }
        if other == 0 {
            return false;
        }
        self.op(EeWonModelOp::PlayerIsTeammate, self.shell, other, 0) != 0
    }

    // ---- LobbyClient

    fn client_clear_group(&self, id: u16, game: bool) {
        let c = self.client();
        if game {
            if id == c.game_group.get() {
                c.game_group.set(0xffff);
                c.game_member.set(0);
                c.game_captain.set(false);
            }
        } else if id == c.chat_group.get() {
            c.chat_group.set(0xffff);
            c.chat_member.set(0);
            c.chat_captain.set(false);
        }
    }
    fn client_status(&self) -> model::EeWonClientStatus {
        let c = self.client();
        model::client_status(c.flags.get() as u32, c.chat_member.get(), c.game_member.get())
    }
    fn client_update_ignore(&self) {
        let c = self.client();
        let ignored = self.op(EeWonModelOp::NameIgnored, c.name.get(), 0, 0) != 0;
        c.ignored.set(ignored);
        let blocked = self.op(EeWonModelOp::CdKeyBlocked, c.cd_key_id.get() as isize, 0, 0) != 0;
        c.blocked.set(blocked);
    }
    fn client_set_captain(&self, captain: bool, game: bool) {
        let c = self.client();
        if game {
            c.game_captain.set(captain);
        } else {
            c.chat_captain.set(captain);
        }
    }
    fn client_set_group_id(&self, id: u16, game: bool) {
        let c = self.client();
        if game {
            c.game_group.set(id);
        } else {
            c.chat_group.set(id);
        }
    }
    fn client_set_member_flags(&self, flags: u8, game: bool) {
        let c = self.client();
        if game {
            c.game_member.set(flags);
        } else {
            c.chat_member.set(flags);
        }
    }
    fn client_is_ready(&self) -> bool {
        // SAFETY: a held player
        unsafe { of(self.client().player.get()) }.is_some_and(|p| p.player().ready.get())
    }
    fn client_ping(&self) -> u16 {
        // SAFETY: a held player
        unsafe { of(self.client().player.get()) }.map_or(0xfffe, |p| p.player().ping.get())
    }
    fn client_is_teammate(&self, other: isize) -> bool {
        let mine = self.client().player.get();
        // SAFETY: a live client
        let Some(o) = (unsafe { of(other) }) else {
            return false;
        };
        let theirs = o.client().player.get();
        if mine == 0 || theirs == 0 {
            return false;
        }
        must(mine).player_same_team(theirs)
    }

    // ---- LobbyClientList

    fn list_add(&self, client: isize) {
        let id = must(client).client().id.get();
        let l = &self.list().clients;
        let (i, _) = l.insert(id);
        self.add_ref(client);
        let old = l.replace(i, client);
        self.release(old);
    }
    fn list_remove(&self, id: u16) -> isize {
        // `Remove`: the object held out of the map, then the entry erased (its reference given back)
        let l = &self.list().clients;
        match l.search(id) {
            Ok(i) => {
                let obj = l.at(i);
                self.add_ref(obj);
                let gone = l.take_at(i);
                self.release(gone);
                obj
            }
            Err(_) => 0,
        }
    }
    fn list_clear(&self) {
        let gone = self.list().clients.drain_from(0);
        self.release_all(gone);
    }
    /// `SyncClients`: the merge of the routing map (ee_won_model_sync_plan; the walk's place a position, moved past
    /// an entry inserted before it).
    fn list_sync(&self, ids: &[u16], infos: &[isize]) {
        self.list_sync_with(ids, &|client, i| {
            self.op(EeWonModelOp::ClientReadRouting, client, infos[i], 0);
        });
    }
    /// The merge, each new or kept client read by `read(client, i)` from the `i`th record.
    fn list_sync_with(&self, ids: &[u16], read: &dyn Fn(isize, usize)) {
        let l = &self.list().clients;
        let plan = model::sync_plan(&l.keys(), ids);
        let mut at = 0usize;
        for step in plan {
            match step {
                SyncStep::Erase(_) => {
                    let gone = l.take_at(at);
                    self.release(gone);
                }
                SyncStep::Insert(i) => {
                    let client = self.op(EeWonModelOp::NewClient, 0, 0, 0);
                    read(client, i as usize);
                    let (pos, new) = l.insert(ids[i as usize]);
                    if new {
                        self.add_ref(client);
                        let old = l.replace(pos, client);
                        self.release(old);
                        if pos <= at {
                            at += 1;
                        }
                    }
                    self.release(client);
                }
                SyncStep::Update(_, i) => {
                    let client = l.at(at);
                    self.add_ref(client);
                    read(client, i as usize);
                    self.release(client);
                    at += 1;
                }
                SyncStep::EraseRest(_) => {
                    let gone = l.drain_from(at);
                    self.release_all(gone);
                    at = l.len();
                }
            }
        }
    }
    fn list_update_ignore(&self) {
        let l = &self.list().clients;
        let mut i = 0;
        while i < l.len() {
            must(l.at(i)).client_update_ignore();
            i += 1;
        }
    }
    fn list_find_by_won_id(&self, id: c_ulong) -> isize {
        let l = &self.list().clients;
        for i in 0..l.len() {
            let c = l.at(i);
            if must(c).client().won_user_id.get() == id {
                return c;
            }
        }
        0
    }
    fn list_clear_invited(&self) {
        let l = &self.list().clients;
        for i in 0..l.len() {
            must(l.at(i)).client().invited.set(false);
        }
    }
    fn cursor_peek(&self, c: &EeWonModelCursor) -> usize {
        if c.started {
            self.list().clients.upper(c.last)
        } else {
            0
        }
    }
    fn cursor_next(&self, c: &mut EeWonModelCursor) -> usize {
        let l = &self.list().clients;
        let i = self.cursor_peek(c);
        if i < l.len() {
            c.last = l.key_at(i);
            c.started = true;
        }
        i
    }
    fn set_has_more(&self, c: &mut EeWonModelCursor) -> bool {
        if c.ready {
            return true;
        }
        let l = &self.list().clients;
        loop {
            let i = self.cursor_peek(c);
            if i >= l.len() {
                return false;
            }
            if must(l.at(i)).client().player.get() != 0 {
                c.ready = true;
                return true;
            }
            self.cursor_next(c);
        }
    }
    fn set_next(&self, c: &mut EeWonModelCursor, out: *mut isize) -> isize {
        if self.set_has_more(c) {
            let client = self.list().clients.at(self.cursor_next(c));
            if !out.is_null() {
                // SAFETY: the caller's out pointer
                unsafe { *out = client };
            }
            let player = must(client).client().player.get();
            c.ready = false;
            return player;
        }
        0
    }

    // ---- LobbyFriend

    fn friend_set_location(&self, address: &EeWonIpAddr, location: isize, flags: u8) {
        let f = self.friend();
        *f.address.borrow_mut() = address.clone();
        self.assign(&f.location, location);
        f.flags.set(flags);
    }
    fn friend_clear_location(&self, empty: isize) {
        let f = self.friend();
        *f.address.borrow_mut() = EeWonIpAddr::default();
        self.assign(&f.location, empty);
        f.flags.set(0);
        f.last_locate.set(0);
        f.anonymous.set(false);
    }
    fn friend_status_from_client(&self, client: isize) {
        let c = must(client);
        let cc = c.client();
        let f = self.friend();
        f.flags.set(model::friend_status(cc.chat_group.get() != 0xffff, cc.game_group.get() != 0xffff));
        f.anonymous.set(c.client_status().anonymous);
    }

    // ---- LobbyGroup

    fn group_kind(&self) -> model::EeWonGroupKind {
        model::group_kind(self.group().flags.get() as u32)
    }
    fn group_is_game(&self) -> bool {
        !self.group_kind().chat
    }
    fn group_clients(&self) -> &EeWonModel {
        must(self.group().clients.get())
    }
    fn group_set_members(&self, ids: &[u16], flags: &[u8]) {
        let g = self.group();
        let from = g.members_from.get();
        self.group_clients().list_clear();
        for (k, id) in ids.iter().enumerate() {
            let client = must(from).list().clients.get(*id);
            if client != 0 {
                let c = must(client);
                c.client_set_member_flags(flags[k], self.group_is_game());
                c.client_set_captain(c.client().id.get() == g.captain.get(), self.group_is_game());
                c.client_set_group_id(g.id.get(), self.group_is_game());
                self.group_clients().list_add(client);
            }
        }
        g.num_clients.set(self.group_clients().list().clients.len() as c_ulong);
    }
    fn group_add(&self, client: isize, flags: u8) {
        let g = self.group();
        self.group_clients().list_add(client);
        let c = must(client);
        c.client_set_group_id(g.id.get(), self.group_is_game());
        c.client_set_member_flags(flags, self.group_is_game());
        c.client_set_captain(c.client().id.get() == g.captain.get(), self.group_is_game());
        g.num_clients.set(self.group_clients().list().clients.len() as c_ulong);
    }
    fn group_remove(&self, id: u16) -> isize {
        let g = self.group();
        let client = self.group_clients().list_remove(id);
        g.num_clients.set(self.group_clients().list().clients.len() as c_ulong);
        if client != 0 {
            must(client).client_clear_group(g.id.get(), self.group_is_game());
        }
        client
    }
    fn group_captain(&self) -> isize {
        let g = self.group();
        if g.captain.get() != 0xffff {
            return self.group_clients().list().clients.get(g.captain.get());
        }
        0
    }
    fn group_set_captain(&self, id: u16, out_old: *mut isize) -> isize {
        let old = self.group_captain();
        if old != 0 {
            must(old).client_set_captain(false, self.group_is_game());
        }
        let mut new = 0;
        if id != 0xffff {
            new = self.group_clients().list().clients.get(id);
            if new != 0 {
                must(new).client_set_captain(true, self.group_is_game());
            }
        }
        if !out_old.is_null() {
            // SAFETY: the caller's out pointer
            unsafe { *out_old = old };
        }
        self.group().captain.set(id);
        new
    }

    // ---- LobbyGame

    fn game_copy_from(&self, other: isize) {
        let o = must(other).game();
        let g = self.game();
        self.assign(&g.name, o.name.get());
        self.assign(&g.password, o.password.get());
        g.has_password.set(o.has_password.get());
        g.invite_only.set(o.invite_only.get());
        g.ask_to_join.set(o.ask_to_join.get());
        g.in_progress.set(o.in_progress.get());
        g.invited.set(o.invited.get());
        g.is_host.set(o.is_host.get());
        g.show_details.set(o.show_details.get());
        g.have_details.set(o.have_details.get());
        g.ping_replied.set(o.ping_replied.get());
        g.last_seen.set(o.last_seen.get());
        g.num_players.set(o.num_players.get());
        g.open_slots.set(o.open_slots.get());
        g.game_type.set(o.game_type.get());
        g.skill.set(o.skill.get());
        self.assign(&g.clients, o.clients.get());
        let a = o.address.borrow().clone();
        *g.address.borrow_mut() = a;
        g.captain.set(o.captain.get());
        g.group.set(o.group.get());
        g.ping.set(o.ping.get());
        self.op(EeWonModelOp::GameCopyFromHook, self.shell, other, 0);
    }
    fn game_find_client(&self, id: u16) -> isize {
        // SAFETY: a held list
        unsafe { of(self.game().clients.get()) }.map_or(0, |l| l.list().clients.get(id))
    }
    fn game_is_full(&self) -> bool {
        let g = self.game();
        staging::is_full(g.num_players.get() as i32, g.open_slots.get() as i32)
    }
    fn game_join_status(&self) -> isize {
        let g = self.game();
        match staging::join_status(
            self.game_is_full(),
            g.invite_only.get(),
            g.invited.get(),
            g.is_host.get(),
            g.in_progress.get(),
        ) {
            Some(s) => s as isize,
            None => self.op(EeWonModelOp::GameJoinStatusHook, self.shell, 0, 0) as i16 as isize,
        }
    }
    fn game_remove_client(&self, client: isize) {
        // `SmartPtr<LobbyPlayer> aPlayer(inClient->GetPlayer())`
        let player = must(client).client().player.get();
        if player == 0 {
            return;
        }
        self.add_ref(player);
        let g = self.game();
        g.num_players.set(g.num_players.get().wrapping_sub(1));
        let c = must(client);
        c.assign(&c.client().player, 0);
        self.op(EeWonModelOp::GamePlayerLeftHook, self.shell, client, player);
        self.release(player);
    }
    fn game_clear_players(&self) {
        // SAFETY: a held list
        let Some(l) = (unsafe { of(self.game().clients.get()) }) else {
            return;
        };
        let mut i = 0;
        while i < l.list().clients.len() {
            let c = must(l.list().clients.at(i));
            c.assign(&c.client().player, 0);
            i += 1;
        }
    }
    fn game_is_same(&self, other: isize) -> bool {
        let o = must(other).game();
        let g = self.game();
        let same_address = {
            let (a, b) = (o.address.borrow(), g.address.borrow());
            a.sa.mPort == b.sa.mPort && a.sa.mAddress == b.sa.mAddress
        };
        model::same_game(g.game_type.get() as i32, o.game_type.get() as i32, g.group.get(), o.group.get(), same_address)
    }
    fn game_read_group(&self, group: isize) {
        let gr = must(group);
        let r = gr.group();
        let g = self.game();
        self.assign(&g.name, r.name.get());
        let kind = gr.group_kind();
        g.has_password.set(kind.password);
        g.invite_only.set(kind.invite_only);
        g.ask_to_join.set(kind.ask_to_join);
        g.group.set(r.id.get());
        g.captain.set(r.captain.get());
        self.assign(&g.clients, r.clients.get());
    }
    fn game_handle_host(&self, client: isize) {
        let player = self.op(EeWonModelOp::NewPlayer, 0, 0, 0);
        let c = must(client);
        c.assign(&c.client().player, player);
        self.op(EeWonModelOp::GameHostHook, self.shell, client, 0);
        self.game().num_players.set(1);
        self.release(player);
    }
    fn game_all_ready(&self) -> bool {
        let l = must(self.game().clients.get());
        let members: Vec<EeWonMemberReady> = l
            .list()
            .clients
            .objs()
            .into_iter()
            .map(|c| {
                let m = must(c);
                EeWonMemberReady {
                    has_player: m.client().player.get() != 0,
                    game_captain: m.client().game_captain.get(),
                    ready: m.client_is_ready(),
                }
            })
            .collect();
        model::all_ready(&members)
    }
    fn game_store_summary(&self, s: &staging::EeWonGameSummary, name: isize) {
        let g = self.game();
        if s.read & 1 != 0 {
            g.address.borrow_mut().set_six(&s.address);
        }
        if s.read & 2 != 0 {
            g.in_progress.set(s.in_progress);
        }
        if s.read & 4 != 0 {
            self.assign(&g.name, name);
        }
        if s.read & 8 != 0 {
            g.skill.set(s.skill as c_long);
        }
        if s.read & 0x10 != 0 {
            g.has_password.set(s.has_password);
            g.invite_only.set(s.invite_only);
            g.ask_to_join.set(s.ask_to_join);
        }
        if s.read & 0x20 != 0 {
            g.num_players.set(s.players as c_long);
        }
        if s.read & 0x40 != 0 {
            g.open_slots.set(s.open_slots as c_long);
        }
    }

    // ---- LobbyGameList

    fn by_id(&self) -> bool {
        model::game_list_by_id(self.game_list().ty.get() as i32)
    }
    fn game_list_add(&self, game: isize) {
        let l = self.game_list();
        let gm = must(game);
        gm.game().game_type.set(l.ty.get());
        self.add_ref(game);
        let old = if self.by_id() {
            let (i, _) = l.id_games.insert(gm.game().group.get());
            l.id_games.replace(i, game)
        } else {
            let a = gm.game().address.borrow().clone();
            let (i, _) = l.addr_games.insert(&a);
            l.addr_games.replace(i, game)
        };
        self.release(old);
    }
    fn new_game(&self) -> isize {
        self.op(EeWonModelOp::NewGame, self.game_list().ty.get() as isize, 0, 0)
    }
    fn game_list_get_or_create_id(&self, id: u16, out: *mut bool) -> isize {
        let l = self.game_list();
        let (i, _) = l.id_games.insert(id);
        let mut game = l.id_games.at(i);
        let existed = game != 0;
        if game == 0 {
            game = self.new_game();
            // `m[...] = game` took a reference; the factory's raw `new` had none
            let old = l.id_games.replace(i, game);
            self.release(old);
            must(game).game().group.set(id);
        }
        if !out.is_null() {
            // SAFETY: the caller's out pointer
            unsafe { *out = existed };
        }
        game
    }
    fn game_list_get_or_create_addr(&self, a: &EeWonIpAddr, out: *mut bool) -> isize {
        let l = self.game_list();
        let (i, _) = l.addr_games.insert(a);
        let mut game = l.addr_games.at(i);
        let existed = game != 0;
        if game == 0 {
            game = self.new_game();
            let old = l.addr_games.replace(i, game);
            self.release(old);
            *must(game).game().address.borrow_mut() = a.clone();
        }
        if !out.is_null() {
            // SAFETY: the caller's out pointer
            unsafe { *out = existed };
        }
        game
    }
    fn game_list_remove(&self, found: Result<usize, usize>, by_id: bool) -> isize {
        let l = self.game_list();
        let Ok(i) = found else {
            return 0;
        };
        let game = if by_id { l.id_games.at(i) } else { l.addr_games.at(i) };
        self.add_ref(game);
        let gone = if by_id { l.id_games.take_at(i) } else { l.addr_games.take_at(i) };
        self.release(gone);
        if game != 0 {
            must(game).game().ping.set(0xfffe);
        }
        game
    }
    fn game_list_has_more(&self) -> bool {
        let l = self.game_list();
        if self.by_id() {
            let i = if l.id_cursor.0.get() { l.id_games.upper(l.id_cursor.1.get()) } else { 0 };
            i < l.id_games.len()
        } else {
            let i = if l.addr_cursor.0.get() { l.addr_games.upper(&l.addr_cursor.1.borrow()) } else { 0 };
            i < l.addr_games.len()
        }
    }
    fn game_list_next(&self) -> isize {
        let l = self.game_list();
        if self.by_id() {
            let i = if l.id_cursor.0.get() { l.id_games.upper(l.id_cursor.1.get()) } else { 0 };
            if i < l.id_games.len() {
                l.id_cursor.1.set(l.id_games.key_at(i));
                l.id_cursor.0.set(true);
            }
            l.id_games.at(i)
        } else {
            let i = if l.addr_cursor.0.get() { l.addr_games.upper(&l.addr_cursor.1.borrow()) } else { 0 };
            if i < l.addr_games.len() {
                let k = l.addr_games.0.borrow()[i].0.clone();
                *l.addr_cursor.1.borrow_mut() = k;
                l.addr_cursor.0.set(true);
            }
            l.addr_games.at(i)
        }
    }
    fn game_list_remove_stale(&self, max_age: c_long, now: i64) -> bool {
        if self.by_id() {
            return false;
        }
        let l = &self.game_list().addr_games;
        let old = l.len();
        let mut i = 0;
        while i < l.len() {
            let gm = must(l.at(i));
            if model::game_stale(now, gm.game().last_seen.get() as i32, max_age as i32) {
                gm.game().ping.set(0xfffe);
                let gone = l.take_at(i);
                self.release(gone);
            } else {
                i += 1;
            }
        }
        l.len() < old
    }

    // ---- LobbyServer

    fn server_clients(&self) -> &EeWonModel {
        must(self.server().clients.get())
    }
    fn server_update_friends(&self) {
        let s = self.server();
        let l = self.server_clients();
        let mut i = 0;
        while i < l.list().clients.len() {
            let client = l.list().clients.at(i);
            let c = must(client);
            let found = self.op(EeWonModelOp::FindFriend, c.client().won_user_id.get() as isize, 0, 0);
            if found != 0 {
                c.client().is_friend.set(true);
                let a = s.address.borrow().clone();
                must(found).friend_set_location(&a, s.name.get(), 0);
                must(found).friend_status_from_client(client);
            } else {
                c.client().is_friend.set(false);
            }
            i += 1;
        }
    }
    fn server_clear_friend_locations(&self, now: c_ulong, empty: isize) {
        let l = self.server_clients();
        let mut i = 0;
        while i < l.list().clients.len() {
            let c = must(l.list().clients.at(i));
            if c.client().is_friend.get() {
                let found = self.op(EeWonModelOp::FindFriend, c.client().won_user_id.get() as isize, 0, 0);
                if found != 0 {
                    if c.client_status().anonymous {
                        must(found).friend_clear_location(empty);
                    } else {
                        must(found).friend().last_locate.set(now);
                    }
                }
            }
            i += 1;
        }
    }
    /// `SyncGroups` (the plan and the walk as `SyncClients`; the populations `EeWonPopulation`).
    fn server_sync_groups(&self, ids: &[u16], infos: &[isize], update_clients: bool) {
        self.server_sync_groups_with(ids, update_clients, &|group, i, clients| {
            self.op(EeWonModelOp::GroupReadRouting, group, infos[i], clients);
        });
    }
    /// The merge, each new or kept room read by `read(group, i, clients)` from the `i`th record.
    fn server_sync_groups_with(&self, ids: &[u16], update_clients: bool, read: &dyn Fn(isize, usize, isize)) {
        let s = self.server();
        let clients = if update_clients { s.clients.get() } else { 0 };
        let mut pop = EeWonPopulation::default();
        let groups = &s.groups;
        let plan = model::sync_plan(&groups.keys(), ids);
        let mut at = 0usize;
        for step in plan {
            match step {
                SyncStep::Erase(_) => {
                    let gone = groups.take_at(at);
                    self.release(gone);
                }
                SyncStep::Insert(i) => {
                    let group = self.op(EeWonModelOp::NewGroup, 0, 0, 0);
                    read(group, i as usize, clients);
                    let (pos, new) = groups.insert(ids[i as usize]);
                    if new {
                        self.add_ref(group);
                        let old = groups.replace(pos, group);
                        self.release(old);
                        if pos <= at {
                            at += 1;
                        }
                    }
                    let g = must(group).group();
                    pop.add_group(g.flags.get() as u32, g.num_clients.get() as u32);
                    self.release(group);
                }
                SyncStep::Update(_, i) => {
                    let group = groups.at(at);
                    self.add_ref(group);
                    read(group, i as usize, clients);
                    let g = must(group).group();
                    pop.add_group(g.flags.get() as u32, g.num_clients.get() as u32);
                    self.release(group);
                    at += 1;
                }
                SyncStep::EraseRest(_) => {
                    let gone = groups.drain_from(at);
                    self.release_all(gone);
                    at = groups.len();
                }
            }
        }
        s.num_chat.set(pop.chat);
        s.num_clients.set(pop.total);
    }
    fn server_add_client(&self, info: isize) -> isize {
        self.server_add_client_with(&|client| {
            self.op(EeWonModelOp::ClientReadRouting, client, info, 0);
        })
    }
    fn server_add_client_with(&self, read: &dyn Fn(isize)) -> isize {
        let client = self.op(EeWonModelOp::NewClient, 0, 0, 0);
        read(client);
        must(client).client_update_ignore();
        self.server_clients().list_add(client);
        self.release(client);
        client
    }
    fn server_add_client_to_group(&self, client: isize, flags: u8, group: isize) {
        let g = must(group);
        g.group_add(client, flags);
        let s = self.server();
        let mut pop = EeWonPopulation { chat: s.num_chat.get(), total: s.num_clients.get() };
        pop.joined(g.group().flags.get() as u32);
        s.num_chat.set(pop.chat);
    }
    fn server_add_group(&self, info: isize) -> isize {
        self.server_add_group_with(&|group, clients| {
            self.op(EeWonModelOp::GroupReadRouting, group, info, clients);
        })
    }
    fn server_add_group_with(&self, read: &dyn Fn(isize, isize)) -> isize {
        let s = self.server();
        let group = self.op(EeWonModelOp::NewGroup, 0, 0, 0);
        read(group, s.clients.get());
        let (i, _) = s.groups.insert(must(group).group().id.get());
        self.add_ref(group);
        let old = s.groups.replace(i, group);
        self.release(old);
        self.release(group);
        group
    }
    fn server_remove_group(&self, id: u16) -> isize {
        let groups = &self.server().groups;
        match groups.search(id) {
            Ok(i) => {
                let group = groups.at(i);
                self.add_ref(group);
                let gone = groups.take_at(i);
                self.release(gone);
                group
            }
            Err(_) => 0,
        }
    }
    fn server_remove_client_from_group(&self, id: u16, group: isize) -> isize {
        let g = must(group);
        let client = g.group_remove(id);
        if client != 0 {
            let s = self.server();
            let mut pop = EeWonPopulation { chat: s.num_chat.get(), total: s.num_clients.get() };
            pop.left(g.group().flags.get() as u32);
            s.num_chat.set(pop.chat);
        }
        client
    }
    fn server_connected(&self) -> bool {
        self.server().connection.get() != 0
    }
    fn server_disconnect(&self, now: c_ulong, empty: isize) {
        let s = self.server();
        if s.connection.get() != 0 {
            self.server_clear_friend_locations(now, empty);
            s.num_clients.set(self.server_clients().list().clients.len() as i32);
            self.op(EeWonModelOp::ConnectionClose, s.connection.get(), 0, 0);
            self.assign(&s.connection, 0);
        }
        s.my_client.set(0xffff);
        self.server_clients().list_clear();
    }
    fn server_has_address(&self, name: &[u8]) -> bool {
        self.server().addresses.borrow().iter().any(|a| strcase::compare_no_case(a, name) == 0)
    }
    fn server_address_add(&self, name: &[u8]) {
        let mut v = self.server().addresses.borrow_mut();
        // a set: the first spelling kept
        if let Err(i) = v.binary_search_by(|a| strcase::compare_no_case(a, name).cmp(&0)) {
            v.insert(i, name.to_vec());
        }
    }

    // ---- LobbyServerList

    fn server_list_insert(&self, a: &EeWonIpAddr) -> isize {
        let l = &self.server_list().servers;
        let (i, _) = l.insert(a);
        if l.at(i) == 0 {
            let server = self.op(EeWonModelOp::NewServer, 0, 0, 0);
            let old = l.replace(i, server);
            self.release(old);
        }
        l.at(i)
    }
}

// ---- the C ABI

/// A new model object for the shell (its constructor's members); `empty` is the empty string's `GUIStringData`.
///
/// # Safety
/// `shell` is the C++ object whose first member will hold the result; `host` answers the ops.
#[no_mangle]
pub unsafe extern "C" fn ee_won_model_new(
    shell: *mut c_void,
    kind: EeWonModelKind,
    host: EeWonModelHost,
    empty: *mut c_void,
) -> *mut EeWonModel {
    EMPTY_TEXT.store(empty as isize, std::sync::atomic::Ordering::Relaxed);
    let m = Box::new(EeWonModel::new(shell as isize, kind, host, empty as isize));
    m.construct(empty as isize);
    Box::into_raw(m)
}

/// Frees a model object (its references given back in reverse member order, after it is gone).
///
/// # Safety
/// `m` came from `ee_won_model_new` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_model_free(m: *mut EeWonModel) {
    if m.is_null() {
        return;
    }
    // SAFETY: the caller's contract
    let m = unsafe { Box::from_raw(m) };
    let held = m.held();
    let host = m.host;
    drop(m);
    for p in held.into_iter().filter(|p| *p != 0) {
        // SAFETY: the host's contract
        unsafe { host(EeWonModelOp::Release, p, 0, 0) };
    }
}

/// A walk's `Rewind` (`LobbyClientIterator`, `LobbyClientSetIterator`): from the first client again.
///
/// # Safety
/// `c` is a live cursor.
#[no_mangle]
pub unsafe extern "C" fn ee_won_model_cursor_reset(c: *mut EeWonModelCursor) {
    // SAFETY: the caller's contract
    unsafe { *c = EeWonModelCursor::default() };
}

/// A member (see [`EeWonModelField`]).
///
/// # Safety
/// `m` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_model_get(m: *const EeWonModel, f: EeWonModelField) -> isize {
    // SAFETY: the caller's contract
    unsafe { &*m }.get(f)
}

/// Sets a member (see [`EeWonModelField`]).
///
/// # Safety
/// `m` is live; an address is a live value.
#[no_mangle]
pub unsafe extern "C" fn ee_won_model_set(m: *const EeWonModel, f: EeWonModelField, v: isize) {
    // SAFETY: the caller's contract
    unsafe { (*m).set(f, v) }
}

/// A method (see [`EeWonModelCall`]); `empty` (`d`) is the empty string's data where a method resets a name.
///
/// # Safety
/// `m` is live; the arguments are what each call says.
#[no_mangle]
pub unsafe extern "C" fn ee_won_model_call(
    m: *const EeWonModel,
    call: EeWonModelCall,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    // SAFETY: the caller's contract
    let m = unsafe { &*m };
    // SAFETY: the caller's contract
    unsafe { dispatch(m, call, a, b, c, d) }
}

unsafe fn slice<'a, T>(p: isize, n: usize) -> &'a [T] {
    if p == 0 || n == 0 {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(p as *const T, n) }
    }
}
unsafe fn addr<'a>(p: isize) -> &'a EeWonIpAddr {
    // SAFETY: the caller's contract
    unsafe { &*(p as *const EeWonIpAddr) }
}
unsafe fn cursor<'a>(p: isize) -> &'a mut EeWonModelCursor {
    // SAFETY: the caller's contract
    unsafe { &mut *(p as *mut EeWonModelCursor) }
}

unsafe fn dispatch(m: &EeWonModel, call: EeWonModelCall, a: isize, bb: isize, c: isize, d: isize) -> isize {
    use EeWonModelCall as C;
    // SAFETY (every arm): the call's arguments, the caller's contract
    unsafe {
        match call {
            C::PlayerIsSameTeam => b(m.player_same_team(a)),
            C::ClientClearGroup => {
                m.client_clear_group(a as u16, bb != 0);
                0
            }
            C::ClientInChat => b(m.client().chat_group.get() != 0xffff),
            C::ClientInGame => b(m.client().game_group.get() != 0xffff),
            C::ClientSetRouting => {
                // ReadRoutingInfo: the name, the flags, the id, the user id, the CD key id
                let cc = m.client();
                let v = slice::<isize>(bb, 4);
                m.assign(&cc.name, a);
                cc.flags.set(v[0] as c_ulong);
                cc.id.set(v[1] as u16);
                cc.won_user_id.set(v[2] as c_ulong);
                cc.cd_key_id.set(v[3] as c_ulong);
                0
            }
            C::ClientUpdateIgnore => {
                m.client_update_ignore();
                0
            }
            C::ClientAnonymous => b(m.client_status().anonymous),
            C::ClientMuted => {
                let s = m.client_status();
                b(if a != 0 { s.muted_game } else { s.muted_chat })
            }
            C::ClientAway => b(model::client_status(m.client().flags.get() as u32, 0, 0).away),
            C::ClientAdmin => b(model::client_status(m.client().flags.get() as u32, 0, 0).admin),
            C::ClientModerator => b(model::client_status(m.client().flags.get() as u32, 0, 0).moderator),
            C::ClientIsCaptain => b(if a != 0 { m.client().game_captain.get() } else { m.client().chat_captain.get() }),
            C::ClientSetCaptain => {
                m.client_set_captain(a != 0, bb != 0);
                0
            }
            C::ClientHasPlayer => b(m.client().player.get() != 0),
            C::ClientIsReady => b(m.client_is_ready()),
            C::ClientPing => m.client_ping() as isize,
            C::ClientIsTeammate => b(m.client_is_teammate(a)),
            C::ClientSetGroupId => {
                m.client_set_group_id(a as u16, bb != 0);
                0
            }
            C::ClientSetMemberFlags => {
                m.client_set_member_flags(a as u8, bb != 0);
                0
            }
            C::ListClear => {
                m.list_clear();
                0
            }
            C::ListSync => {
                m.list_sync(slice(a, c as usize), slice(bb, c as usize));
                0
            }
            C::ListUpdateIgnore => {
                m.list_update_ignore();
                0
            }
            C::ListAdd => {
                m.list_add(a);
                0
            }
            C::ListFind => m.list().clients.get(a as u16),
            C::ListRemove => m.list_remove(a as u16),
            C::ListFindByWonId => m.list_find_by_won_id(a as c_ulong),
            C::ListClearInvited => {
                m.list_clear_invited();
                0
            }
            C::ListCount => m.list().clients.len() as isize,
            C::ListAt => m.list().clients.at(a as usize),
            C::ListIterHasMore => b(m.cursor_peek(cursor(a)) < m.list().clients.len()),
            C::ListIterNext => {
                let i = m.cursor_next(cursor(a));
                m.list().clients.at(i)
            }
            C::ListSetHasMore => b(m.set_has_more(cursor(a))),
            C::ListSetNext => m.set_next(cursor(a), bb as *mut isize),
            C::FriendSetLocation => {
                m.friend_set_location(addr(a), bb, c as u8);
                0
            }
            C::FriendClearLocation => {
                m.friend_clear_location(d);
                0
            }
            C::FriendSetStatusFromClient => {
                m.friend_status_from_client(a);
                0
            }
            C::FriendStartLocate => {
                let f = m.friend();
                f.flags.set(model::friend_start_locate());
                f.search_start.set(a as c_ulong);
                0
            }
            C::FriendEndLocate => {
                let f = m.friend();
                f.flags.set(model::friend_end_locate(f.flags.get()));
                f.last_locate.set(a as c_ulong);
                0
            }
            C::GroupSetRouting => {
                // ReadRoutingInfo: id, name, current count (the field at +0x2a), flags, captain, capacity
                let g = m.group();
                let v = slice::<isize>(bb, 5);
                g.id.set(v[0] as u16);
                m.assign(&g.name, a);
                g.num_clients.set(v[1] as c_ulong);
                g.flags.set(v[2] as c_ulong);
                g.captain.set(v[3] as u16);
                g.max_clients.set(v[4] as c_ulong);
                0
            }
            C::GroupSetMembers => {
                m.group_set_members(slice(a, c as usize), slice(bb, c as usize));
                0
            }
            C::GroupSetMembersList => {
                m.group().members_from.set(a);
                0
            }
            C::GroupAdd => {
                m.group_add(a, bb as u8);
                0
            }
            C::GroupRemove => m.group_remove(a as u16),
            C::GroupSetCaptain => m.group_set_captain(a as u16, bb as *mut isize),
            C::GroupCaptain => m.group_captain(),
            C::GroupFind => m.group_clients().list().clients.get(a as u16),
            C::GroupKind => {
                let k = m.group_kind();
                b(k.password) | b(k.ask_to_join) << 1 | b(k.invite_only) << 2 | b(k.chat) << 3 | b(k.public_chat) << 4
            }
            C::GameCopyFrom => {
                m.game_copy_from(a);
                0
            }
            C::GameFindClient => m.game_find_client(a as u16),
            C::GameMatchesRoomSpec => b(staging::matches_room_spec(a as i32, m.game().game_type.get() as i32)),
            C::GamePinging => b(model::game_pinging(m.game().ping.get())),
            C::GameQuerying => b(model::game_querying(m.game().ping.get())),
            C::GameRemoveClient => {
                m.game_remove_client(a);
                0
            }
            C::GameIsFull => b(m.game_is_full()),
            C::GameJoinStatus => m.game_join_status(),
            C::GameClearPlayers => {
                m.game_clear_players();
                0
            }
            C::GameIsSameGame => b(m.game_is_same(a)),
            C::GameReadGroup => {
                m.game_read_group(a);
                0
            }
            C::GameHandleHost => {
                m.game_handle_host(a);
                0
            }
            C::GameAllReady => b(m.game_all_ready()),
            C::GameCanStart => {
                if !m.game_all_ready() {
                    return 0;
                }
                b(m.op(EeWonModelOp::GameCanStartHook, m.shell, 0, 0) != 0)
            }
            C::GameStoreSummary => {
                m.game_store_summary(&*(a as *const staging::EeWonGameSummary), bb);
                0
            }
            C::GameListClear => {
                let l = m.game_list();
                let mut gone = l.id_games.drain_from(0);
                gone.extend(l.addr_games.drain());
                m.release_all(gone);
                0
            }
            C::GameListAdd => {
                m.game_list_add(a);
                0
            }
            C::GameListGetOrCreateId => m.game_list_get_or_create_id(a as u16, bb as *mut bool),
            C::GameListGetOrCreateAddr => m.game_list_get_or_create_addr(addr(a), bb as *mut bool),
            C::GameListGetId => m.game_list().id_games.get(a as u16),
            C::GameListGetAddr => m.game_list().addr_games.get(addr(a)),
            C::GameListRemoveId => {
                let found = m.game_list().id_games.search(a as u16);
                m.game_list_remove(found, true)
            }
            C::GameListRemoveAddr => {
                let found = m.game_list().addr_games.search(addr(a));
                m.game_list_remove(found, false)
            }
            C::GameListRewind => {
                if m.by_id() {
                    m.game_list().id_cursor.0.set(false);
                } else {
                    m.game_list().addr_cursor.0.set(false);
                }
                0
            }
            C::GameListHasMore => b(m.game_list_has_more()),
            C::GameListNext => m.game_list_next(),
            C::GameListIsEmpty => {
                let l = m.game_list();
                b(if m.by_id() { l.id_games.len() == 0 } else { l.addr_games.len() == 0 })
            }
            C::GameListRemoveStale => b(m.game_list_remove_stale(a as c_long, bb as i64)),
            C::ServerHasAddress => b(m.server_has_address(slice(a, bb as usize))),
            C::ServerSyncClients => {
                let l = m.server_clients();
                l.list_sync(slice(a, c as usize), slice(bb, c as usize));
                l.list_update_ignore();
                0
            }
            C::ServerUpdateFriends => {
                m.server_update_friends();
                0
            }
            C::ServerClearFriendLocations => {
                m.server_clear_friend_locations(a as c_ulong, d);
                0
            }
            C::ServerSyncGroups => {
                let n = (c as u64 & 0xffff_ffff) as usize;
                m.server_sync_groups(slice(a, n), slice(bb, n), (c as u64 >> 32) != 0);
                0
            }
            C::ServerAddClient => m.server_add_client(a),
            C::ServerAddClientToGroup => {
                m.server_add_client_to_group(a, bb as u8, c);
                0
            }
            C::ServerAddGroup => m.server_add_group(a),
            C::ServerRemoveGroup => m.server_remove_group(a as u16),
            C::ServerRemoveClient => m.server_clients().list_remove(a as u16),
            C::ServerRemoveClientFromGroup => m.server_remove_client_from_group(a as u16, bb),
            // `GetClient(int)`: the id converted to the key type
            C::ServerGetClient => m.server_clients().list().clients.get(a as u16),
            C::ServerFindByWonId => m.server_clients().list_find_by_won_id(a as c_ulong),
            C::ServerMyClient => {
                let id = m.server().my_client.get();
                if id == 0xffff {
                    0
                } else {
                    m.server_clients().list().clients.get(id)
                }
            }
            C::ServerGroup => m.server().groups.get(a as u16),
            C::ServerNumClientsNow => model::server_num_clients(
                m.server_connected(),
                m.server_clients().list().clients.len() as u32,
                m.server().num_clients.get(),
            ) as isize,
            C::ServerConnected => b(m.server_connected()),
            C::ServerClearGroups => {
                let gone = m.server().groups.drain_from(0);
                m.release_all(gone);
                0
            }
            C::ServerGroupCount => m.server().groups.len() as isize,
            C::ServerGroupAt => m.server().groups.at(a as usize),
            C::ServerSetListed => {
                m.server().num_clients.set(a as u16 as i32);
                0
            }
            C::ServerAddressesClear => {
                m.server().addresses.borrow_mut().clear();
                0
            }
            C::ServerAddressAdd => {
                m.server_address_add(slice(a, bb as usize));
                0
            }
            C::ServerDisconnect => {
                m.server_disconnect(a as c_ulong, d);
                0
            }
            C::ServerListInsert => m.server_list_insert(addr(a)),
            C::ServerListFind => m.server_list().servers.get(addr(a)),
            C::ServerListCount => m.server_list().servers.len() as isize,
            C::ServerListAt => m.server_list().servers.at(a as usize),
            C::ServerListAddressAt => {
                let v = m.server_list().servers.0.borrow();
                v.get(a as usize).map_or(0, |e| &e.0 as *const EeWonIpAddr as isize)
            }
        }
    }
}

// ---- the routing server's records (R6 WONLobby round 12): read from the routing op's Rust object (routeop.rs), as
// `RoutingOp::GetClients` / `GetGroups` / `GetClientInfo` / `GetGroupInfo` / `GetDataObjects` copied them into C++
// records (RoutingOp.cpp, deleted) and `ReadRoutingInfo` / `SyncClients` / `SyncGroups` / `SetMembers`
// (LobbyModelHost.cpp) read those; the copies are taken before the first C++ call, as the original's were.

/// A client record (`RoutingClientInfo`): its map key, id, name, flags, WON user id and CD key id.
struct RouteClient {
    key: u16,
    id: u16,
    name: Vec<u16>,
    flags: u32,
    user_id: u32,
    cd_key: u32,
}

/// A room record (`RoutingGroupInfo`): its map key, the fields `ReadRoutingInfo` reads, its members' ids and flags
/// in key order.
struct RouteGroup {
    key: u16,
    id: u16,
    name: Vec<u16>,
    captain: u16,
    max_clients: u16,
    flags: c_ulong,
    /// the field at +0x2a: the current member count despite its name
    max_players: u16,
    members: Vec<(u16, u8)>,
}

fn route_client(key: u16, c: &ee_won_net::routeop::Client) -> RouteClient {
    RouteClient { key, id: c.id, name: c.name.clone(), flags: c.flags, user_id: c.user_id, cd_key: c.address }
}

/// The op's client map (`GetClients`).
fn route_clients(op: Option<&ee_won_net::routeop::RouteOp>) -> Vec<RouteClient> {
    op.map_or_else(Vec::new, |o| o.clients.iter().map(|(&k, c)| route_client(k, &c.borrow())).collect())
}

/// The op's group map with each group's members (`GetGroups`).
fn route_groups(op: Option<&ee_won_net::routeop::RouteOp>) -> Vec<RouteGroup> {
    let Some(o) = op else { return Vec::new() };
    o.groups
        .iter()
        .map(|(&key, g)| {
            let g = g.borrow();
            RouteGroup {
                key,
                id: g.id,
                name: g.name.clone(),
                captain: g.captain,
                max_clients: g.max_clients,
                flags: g.flags as c_ulong,
                max_players: g.max_players,
                members: g.members.iter().map(|(&k, m)| (k, m.borrow().flags)).collect(),
            }
        })
        .collect()
}

/// The group created notice's room (`GetGroupInfo`: a new record, the rest of it the constructor's).
fn route_notice_group(op: Option<&ee_won_net::routeop::RouteOp>) -> RouteGroup {
    use ee_won_net::routeop::{EE_WON_RTF_GROUP_FLAGS, EE_WON_RTF_GROUP_ID, EE_WON_RTF_MAX_CLIENTS, EE_WON_RTT_NAME};
    let num = |f: i32| op.map_or(0, |o| o.num(f));
    RouteGroup {
        key: 0,
        id: num(EE_WON_RTF_GROUP_ID) as u16,
        name: op.map_or_else(Vec::new, |o| o.text(EE_WON_RTT_NAME).to_vec()),
        captain: 0,
        max_clients: num(EE_WON_RTF_MAX_CLIENTS) as u16,
        flags: num(EE_WON_RTF_GROUP_FLAGS) as c_ulong,
        max_players: 0,
        members: Vec::new(),
    }
}

/// A data object list (`RDataObjects`): each object's link id and buffer.
type RouteObjects = Vec<(u16, Option<Vec<u8>>)>;

impl EeWonModel {
    /// The text of a record's name: a new `GUIStringData` (held).
    fn route_text(&self, units: &[u16]) -> isize {
        self.op(EeWonModelOp::TextOfUnits, units.as_ptr() as isize, units.len() as isize, 0)
    }
    /// `LobbyClient::ReadRoutingInfo` (this a client): the name, the flags, the id, the user id, the CD key id.
    fn client_read_route(&self, r: &RouteClient) {
        let name = self.route_text(&r.name);
        let cc = self.client();
        self.assign(&cc.name, name);
        cc.flags.set(r.flags as c_ulong);
        cc.id.set(r.id);
        cc.won_user_id.set(r.user_id as c_ulong);
        cc.cd_key_id.set(r.cd_key as c_ulong);
        self.release(name);
    }
    /// `LobbyGroup::ReadRoutingInfo` (this a room): the id, name, current count, flags, captain and capacity; then,
    /// with a client list, `SetMembers` over it. The name's temporary is released last, as the original's was.
    fn group_read_route(&self, r: &RouteGroup, clients: isize) {
        let name = self.route_text(&r.name);
        let g = self.group();
        g.id.set(r.id);
        self.assign(&g.name, name);
        g.num_clients.set(r.max_players as c_ulong);
        g.flags.set(r.flags);
        g.captain.set(r.captain);
        g.max_clients.set(r.max_clients as c_ulong);
        if clients != 0 {
            g.members_from.set(clients);
            let ids: Vec<u16> = r.members.iter().map(|m| m.0).collect();
            let flags: Vec<u8> = r.members.iter().map(|m| m.1).collect();
            self.group_set_members(&ids, &flags);
        }
        self.release(name);
    }
    /// `ReadSummaryBuffer` of a game over a buffer (none: false, the C++ not asked).
    fn game_read_bytes(&self, data: Option<&[u8]>) -> isize {
        match data {
            None => 0,
            Some(d) => self.op(EeWonModelOp::GameReadSummaryBytes, self.shell, d.as_ptr() as isize, d.len() as isize),
        }
    }
}

/// RoutingLogic's ops on the routing server's records (the C++ was RoutingLogic.cpp's host over RoutingOp.cpp's
/// copies): the answer, or `None` for another op. `a`, `bb`, `c` as `EeWonRouteLogicOp` documents; the C++ ops'
/// Rust objects through the core table.
fn route_record_op(op: crate::routelogic::EeWonRouteLogicOp, a: isize, bb: isize, c: isize) -> Option<isize> {
    use ee_won_net::cppcore::{host_op_of, route_op_of};
    route_record_op_in(op, a, bb, c, &|p| route_op_of(p as *mut c_void), &|p| {
        host_op_of(p as *mut c_void).map(ee_won_net::routeop::group_list_of)
    })
}

/// [`route_record_op`] over a routing op's lookup `ro` (`RoutingOp*` -> its Rust op) and `group_list` (a group
/// list request op -> the connection-less group list op its replies make).
fn route_record_op_in<'r>(
    op: crate::routelogic::EeWonRouteLogicOp,
    a: isize,
    bb: isize,
    c: isize,
    ro: &dyn Fn(isize) -> Option<&'r ee_won_net::routeop::RouteOp>,
    group_list: &dyn Fn(isize) -> Option<ee_won_net::routeop::RouteOp>,
) -> Option<isize> {
    use crate::routelogic::EeWonRouteLogicOp as O;
    Some(match op {
        O::ServerAddGroup => {
            let g = route_notice_group(ro(bb));
            must(a).server_add_group_with(&|group, clients| must(group).group_read_route(&g, clients))
        }
        O::ServerAddClient => {
            // (no client record: the original read a NULL record and faulted; the client keeps its defaults)
            let r = ro(bb).and_then(|o| o.op_client.as_ref().map(|c| route_client(c.borrow().id, &c.borrow())));
            must(a).server_add_client_with(&|client| {
                if let Some(r) = &r {
                    must(client).client_read_route(r);
                }
            })
        }
        O::ServerSyncClients => {
            let recs = route_clients(ro(bb));
            let ids: Vec<u16> = recs.iter().map(|r| r.key).collect();
            let l = must(a).server_clients();
            l.list_sync_with(&ids, &|client, i| must(client).client_read_route(&recs[i]));
            l.list_update_ignore();
            0
        }
        O::ServerSyncGroups | O::SyncGroupsOfQuery => {
            let recs =
                if op == O::ServerSyncGroups { route_groups(ro(bb)) } else { route_groups(group_list(bb).as_ref()) };
            let ids: Vec<u16> = recs.iter().map(|r| r.key).collect();
            let update = op == O::ServerSyncGroups && c != 0;
            must(a).server_sync_groups_with(&ids, update, &|group, i, clients| {
                must(group).group_read_route(&recs[i], clients)
            });
            0
        }
        O::GameReadSummary => {
            let data = ro(bb).and_then(|o| o.data.clone());
            must(a).game_read_bytes(data.as_deref())
        }
        O::RDataObjects => {
            let list: RouteObjects = ro(a).map_or_else(Vec::new, |o| {
                o.objects.iter().map(|x| (x.borrow().link, x.borrow().data.clone())).collect()
            });
            // SAFETY: the count's slot (the op's contract)
            unsafe { *(bb as *mut isize) = list.len() as isize };
            Box::into_raw(Box::new(list)) as isize
        }
        // SAFETY (the list ops): a list from RDataObjects, not yet freed
        O::DataObjectLink => unsafe { (&*(a as *const RouteObjects))[bb as usize].0 as isize },
        O::GameReadDataObject => {
            // SAFETY: as above
            let list = unsafe { &*(bb as *const RouteObjects) };
            must(a).game_read_bytes(list[c as usize].1.as_deref())
        }
        O::ListFree if bb == 1 => {
            // SAFETY: as above; freed once
            drop(unsafe { Box::from_raw(a as *mut RouteObjects) });
            0
        }
        _ => return None,
    })
}

/// The game's model's answer to one of RoutingLogic's record ops ([`route_record_op`]; `won_routemodel_test`'s): true
/// with the answer in `out`, false for another op.
///
/// # Safety
/// The op's arguments as `EeWonRouteLogicOp` documents them (live shells; C++ routing ops on a bound core table);
/// `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_model_route_record_op(op: u32, a: isize, b: isize, c: isize, out: *mut isize) -> bool {
    // (the op as its number: this header is included inside namespace WONAPI, routelogic.h outside it)
    use crate::routelogic::EeWonRouteLogicOp as O;
    const OPS: [O; 10] = [
        O::ServerAddGroup,
        O::ServerAddClient,
        O::ServerSyncClients,
        O::ServerSyncGroups,
        O::SyncGroupsOfQuery,
        O::GameReadSummary,
        O::RDataObjects,
        O::DataObjectLink,
        O::GameReadDataObject,
        O::ListFree,
    ];
    let Some(&op) = OPS.iter().find(|o| **o as u32 == op) else { return false };
    match route_record_op(op, a, b, c) {
        Some(r) => {
            // SAFETY: the caller's slot
            unsafe { *out = r };
            true
        }
        None => false,
    }
}

// ---- the logics' calls on the model, answered here (the logics made with `_model` constructors: the game's; the
// logics' goldens keep their scripted hosts)

/// The empty string's data the shells pass (a cleared friend location's), kept from the model objects made.
static EMPTY_TEXT: std::sync::atomic::AtomicIsize = std::sync::atomic::AtomicIsize::new(0);

/// `time(NULL)`.
fn now() -> isize {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map_or(0, |d| d.as_secs() as isize)
}

/// RoutingLogic's ops on the servers, rooms, clients, friends and games (routelogic.rs `EeWonRouteLogicOp`; their
/// C++ was RoutingLogic.cpp's host): the answer, or `None` for an op (or a case) that is C++'s.
pub(crate) fn route_op(
    op: crate::routelogic::EeWonRouteLogicOp,
    a: isize,
    bb: isize,
    c: isize,
    d: isize,
) -> Option<isize> {
    use crate::routelogic::EeWonRouteLogicOp as O;
    if let Some(r) = route_record_op(op, a, bb, c) {
        return Some(r);
    }
    let m = || must(a);
    let empty = EMPTY_TEXT.load(std::sync::atomic::Ordering::Relaxed);
    // SAFETY (the calls): the arguments each call takes, live shells (the logic's contract)
    let call =
        |x: &EeWonModel, k: EeWonModelCall, p: isize, q: isize, r: isize| unsafe { dispatch(x, k, p, q, r, empty) };
    Some(match op {
        O::ServerNum => match bb {
            0 => m().server().clients.get(),
            1 => m().server().connection.get(),
            2 => call(m(), EeWonModelCall::ServerMyClient, 0, 0, 0),
            3 => m().server().my_client.get() as isize,
            5 => call(m(), EeWonModelCall::ServerNumClientsNow, 0, 0, 0),
            7 => b(m().server_connected()),
            8 => m().server().groups.len() as isize,
            9 => b(m().server().querying.get()),
            10 => m().server().last_ping.get() as isize,
            4 | 6 => return None,
            _ => 0,
        },
        O::ServerFindByWonId => call(m(), EeWonModelCall::ServerFindByWonId, bb, 0, 0),
        O::ServerGroup => m().server().groups.get(bb as u16),
        O::ServerClient => call(m(), EeWonModelCall::ServerGetClient, bb, 0, 0),
        O::ServerGroupAt => m().server().groups.at(bb as usize),
        O::ServerAddClientToGroup => call(m(), EeWonModelCall::ServerAddClientToGroup, bb, c, d),
        O::ServerRemoveGroup => m().server_remove_group(bb as u16),
        O::ServerRemoveClient => m().server_clients().list_remove(bb as u16),
        O::ServerRemoveClientFromGroup => m().server_remove_client_from_group(bb as u16, c),
        O::ServerDo => {
            match bb {
                1 => m().server_disconnect(now() as c_ulong, empty),
                2 => m().server_clients().list_clear_invited(),
                3 => m().server_update_friends(),
                4 => {
                    call(m(), EeWonModelCall::ServerClearGroups, 0, 0, 0);
                }
                5 => m().server().querying.set(c != 0),
                6 => m().server().last_ping.set(c as c_ulong),
                7 => m().server().my_client.set(c as u16),
                8 => m().assign(&m().server().connection, c),
                _ => {}
            }
            0
        }
        O::FriendAtServer => {
            let f = m().friend().address.borrow().clone();
            let s = must(bb).server().address.borrow().clone();
            b(f.sa.mPort == s.sa.mPort && f.sa.mAddress == s.sa.mAddress)
        }
        O::GroupNum => {
            let g = m();
            let k = g.group_kind();
            match bb {
                0 => g.group().id.get() as isize,
                1 => b(!k.chat),
                2 => b(k.public_chat),
                3 => g.group().num_clients.get() as isize,
                4 => b(k.password),
                5 => b(k.ask_to_join),
                6 => g.group().captain.get() as isize,
                9 => g.group().clients.get(),
                7 | 8 => return None,
                _ => 0,
            }
        }
        O::GroupFindClient => call(m(), EeWonModelCall::GroupFind, bb, 0, 0),
        O::GroupSetCaptain => m().group_set_captain(bb as u16, c as *mut isize),
        O::ClientNum => {
            let x = m();
            let cl = x.client();
            let st = model::client_status(cl.flags.get() as u32, 0, 0);
            match bb {
                0 => cl.id.get() as isize,
                1 => cl.won_user_id.get() as isize,
                2 => b(cl.is_friend.get()),
                3 => b(x.client_status().anonymous),
                4 => b(cl.ignored.get()),
                5 => b(cl.blocked.get()),
                6 => b(cl.chat_group.get() != 0xffff),
                7 => cl.chat_group.get() as isize,
                8 => b(cl.game_group.get() != 0xffff),
                9 => cl.game_group.get() as isize,
                10 => b(st.away),
                11 => b(st.moderator),
                12 => b(st.admin),
                13 => cl.cd_key_id.get() as isize,
                _ => 0,
            }
        }
        O::ClientDo => {
            let x = m();
            match bb {
                1 => x.client_set_member_flags(c as u8, d != 0),
                2 => x.client().flags.set(c as c_ulong),
                3 => x.client().is_friend.set(c != 0),
                4 => x.client().invited.set(c != 0),
                _ => {}
            }
            0
        }
        O::ClientIsTeammate => b(m().client_is_teammate(bb)),
        O::FriendNum => {
            let x = m();
            let f = x.friend();
            match bb {
                0 => f.flags.get() as isize,
                1 => f.last_locate.get() as isize,
                2 => f.user_id.get() as isize,
                3 => b(f.anonymous.get()),
                4 => f.last_search.get() as isize,
                5 => f.search_start.get() as isize,
                _ => 0,
            }
        }
        O::FriendDo => {
            let x = m();
            match bb {
                1 => x.friend().anonymous.set(c != 0),
                2 => x.friend_status_from_client(c),
                3 => {
                    let s = must(c).server();
                    let address = s.address.borrow().clone();
                    x.friend_set_location(&address, s.name.get(), d as u8);
                }
                4 => x.friend_clear_location(empty),
                5 => {
                    call(x, EeWonModelCall::FriendStartLocate, c, 0, 0);
                }
                6 => x.friend().last_search.set(c as c_ulong),
                7 => {
                    call(x, EeWonModelCall::FriendEndLocate, c, 0, 0);
                }
                _ => {}
            }
            0
        }
        O::GameNum => {
            let x = m();
            let g = x.game();
            match bb {
                0 => b(g.ask_to_join.get()),
                1 => b(g.invite_only.get()),
                2 => g.game_type.get() as isize,
                3 => g.group.get() as isize,
                4 => b(g.is_host.get()),
                5 => b(g.filtered.get()),
                6 => b(g.has_password.get()),
                7 => b(x.game_join_status() == 0),
                8 => g.ping.get() as isize,
                9 => g.skill.get() as isize,
                _ => 0,
            }
        }
        O::GameDo => {
            let x = m();
            match bb {
                1 => x.game().group.set(c as u16),
                2 => x.assign(&x.game().clients, c),
                3 => x.game().invited.set(c != 0),
                4 => x.game_read_group(c),
                _ => {}
            }
            0
        }
        _ => return None,
    })
}

/// A model method on a live shell, as the C++ shells' inline forwards call it (the logics that hold model objects).
pub(crate) fn call(shell: isize, k: EeWonModelCall, a: isize, bb: isize, c: isize) -> isize {
    let empty = EMPTY_TEXT.load(std::sync::atomic::Ordering::Relaxed);
    // SAFETY: a live shell; the call's arguments as `EeWonModelCall` documents
    unsafe { dispatch(must(shell), k, a, bb, c, empty) }
}
/// A member of a live shell (`ee_won_model_get`).
pub(crate) fn get(shell: isize, f: EeWonModelField) -> isize {
    // SAFETY: a live shell
    unsafe { ee_won_model_get(must(shell), f) }
}
/// A member of a live shell set (`ee_won_model_set`).
pub(crate) fn set(shell: isize, f: EeWonModelField, v: isize) {
    // SAFETY: a live shell; the value as the field takes it
    unsafe { ee_won_model_set(must(shell), f, v) }
}
/// A reference on a model object taken (its shell's `AddRef`, through its own host); NULL: none.
pub(crate) fn hold(shell: isize) {
    if shell != 0 {
        let host = must(shell).host;
        // SAFETY: the host's contract
        unsafe { host(EeWonModelOp::AddRef, shell, 0, 0) };
    }
}
/// A reference on a model object given back (it may be deleted); NULL: none.
pub(crate) fn release(shell: isize) {
    if shell != 0 {
        let host = must(shell).host;
        // SAFETY: the host's contract; nothing of the object is used after
        unsafe { host(EeWonModelOp::Release, shell, 0, 0) };
    }
}
/// `SmartPtr<model object>::operator=`: the old one released, then the new one taken.
pub(crate) fn assign(slot: &Cell<isize>, v: isize) {
    let old = slot.get();
    if old != v {
        release(old);
        hold(v);
        slot.set(v);
    }
}

/// The lobby logics' ops on the games and clients (lobbyhost.rs `EeWonLobbyOp`; their C++ was LobbyLogicHost.cpp's
/// and LanLogicHost.cpp's): the answer, or `None` for an op that is C++'s.
pub(crate) fn lobby_op(op: crate::lobbyhost::EeWonLobbyOp, a: isize, bb: isize, c: isize) -> Option<isize> {
    use crate::lobbyhost::EeWonLobbyOp as O;
    let m = || must(a);
    Some(match op {
        O::GameIsHost => b(m().game().is_host.get()),
        O::GameFindClient => m().game_find_client(c as u16),
        O::GameType => m().game().game_type.get() as isize,
        O::GameIsInProgress => b(m().game().in_progress.get()),
        O::GameAreAllReady => b(m().game_all_ready()),
        O::GamePingReplied => b(m().game().ping_replied.get()),
        O::GameIsPinging => b(model::game_pinging(m().game().ping.get())),
        O::GameIsSameGame => b(m().game_is_same(bb)),
        O::GameJoinStatus => m().game_join_status(),
        O::GamePing => m().game().ping.get() as isize,
        O::GameBecomeHost => {
            let g = m().game();
            g.is_host.set(true);
            g.captain.set(must(bb).client().id.get());
            must(bb).client_set_captain(true, true);
            0
        }
        O::GameAddHostClient => {
            must(m().game().clients.get()).list_add(bb);
            m().game_handle_host(bb);
            0
        }
        O::GameRemoveClient => {
            m().game_remove_client(bb);
            0
        }
        O::GameSetCaptainId => {
            m().game().captain.set(c as u16);
            0
        }
        O::GameAddressPort => m().game().address.borrow().port() as isize,
        O::GameHasPassword => b(m().game().has_password.get()),
        O::ClientHasPlayer => b(m().client().player.get() != 0),
        O::ClientPlayer => m().client().player.get(),
        O::ClientIsReady => b(m().client_is_ready()),
        O::ClientId => m().client().id.get() as isize,
        O::ClientIsGameCaptain => b(m().client().game_captain.get()),
        O::ClientIsTeammate => b(m().client_is_teammate(bb)),
        O::ClientIsIgnored => b(m().client().ignored.get()),
        O::ClientPlayerSetReady => {
            must(m().client().player.get()).player().ready.set(c != 0);
            0
        }
        O::ClientPlayerSetPing => {
            must(m().client().player.get()).player().ping.set(c as u16);
            0
        }
        _ => return None,
    })
}

/// The options popups' ops on their client and game (lobbypopup.rs `EeWonLobbyPopupOp`; LobbyDialogHost.cpp's).
pub(crate) fn popup_op(op: crate::lobbypopup::EeWonLobbyPopupOp, a: isize, bb: isize) -> Option<isize> {
    use crate::lobbypopup::EeWonLobbyPopupOp as O;
    let m = || must(a);
    let flags = || model::client_status(m().client().flags.get() as u32, 0, 0);
    Some(match op {
        O::ClientIsModerator => b(flags().moderator),
        O::ClientIsCaptain => {
            b(if bb != 0 { m().client().game_captain.get() } else { m().client().chat_captain.get() })
        }
        O::ClientId => m().client().id.get() as isize,
        O::ClientWonUserId => m().client().won_user_id.get() as isize,
        O::ClientGameGroupId => m().client().game_group.get() as isize,
        O::ClientIsInvited => b(m().client().invited.get()),
        O::ClientIsFriend => b(m().client().is_friend.get()),
        O::ClientIsAway => b(flags().away),
        O::ClientIsIgnored => b(m().client().ignored.get()),
        O::ClientIsBlocked => b(m().client().blocked.get()),
        O::ClientIsMuted => {
            let s = m().client_status();
            b(if bb != 0 { s.muted_game } else { s.muted_chat })
        }
        O::GameCanJoin => b(m().game_join_status() == 0),
        O::GameInProgress => b(m().game().in_progress.get()),
        O::GameType => m().game().game_type.get() as isize,
        O::GameShowDetails => b(m().game().show_details.get()),
        _ => return None,
    })
}

/// The lobby controls' ops on the games and clients (lobbyctl.rs `EeWonLobbyCtlOp`; LobbyCtrlHost.cpp's).
pub(crate) fn ctl_op(op: crate::lobbyctl::EeWonLobbyCtlOp, a: isize, bb: isize, c: isize) -> Option<isize> {
    use crate::lobbyctl::EeWonLobbyCtlOp as O;
    let m = || must(a);
    Some(match op {
        // round 13: the controls' generic member reads, writes and methods (their own field and call numbers)
        // SAFETY: the controls pass a field of this enum
        O::MGet => m().get(unsafe { std::mem::transmute::<u32, EeWonModelField>(bb as u32) }),
        O::MSet => {
            // SAFETY: as MGet; a text or an address the controls pass is live
            unsafe { m().set(std::mem::transmute::<u32, EeWonModelField>(bb as u32), c) };
            0
        }
        O::MCall => {
            let empty = EMPTY_TEXT.load(std::sync::atomic::Ordering::Relaxed);
            // SAFETY: the controls pass a call of this enum and its argument
            unsafe { dispatch(m(), std::mem::transmute::<u32, EeWonModelCall>(bb as u32), c, 0, 0, empty) }
        }
        O::GameIsHost => b(m().game().is_host.get()),
        O::GameCanStart => {
            let empty = EMPTY_TEXT.load(std::sync::atomic::Ordering::Relaxed);
            // SAFETY: a live game
            unsafe { dispatch(m(), EeWonModelCall::GameCanStart, 0, 0, 0, empty) }
        }
        O::GameCaptainId => m().game().captain.get() as isize,
        O::GameClients => m().game().clients.get(),
        O::ClientsCount => m().list().clients.len() as isize,
        O::ClientId => m().client().id.get() as isize,
        O::ClientHasPlayer => b(m().client().player.get() != 0),
        O::ClientPlayer => m().client().player.get(),
        _ => return None,
    })
}

/// The EE lobby game's ops on itself, its clients and the server (eegame.rs `EeWonEeGameOp`; EELobbyGameHost.cpp's).
pub(crate) fn eegame_op(op: crate::eegame::EeWonEeGameOp, game: isize, a: isize, bb: isize) -> Option<isize> {
    use crate::eegame::EeWonEeGameOp as O;
    let m = || must(a);
    let g = || must(game).game();
    Some(match op {
        O::IsHost => b(g().is_host.get()),
        O::MaxPlayers => g().open_slots.get() as isize,
        O::SetMaxPlayers => {
            g().open_slots.set(a as c_long);
            0
        }
        O::GameType => g().game_type.get() as isize,
        O::Clients => g().clients.get(),
        O::FindClient => m().list().clients.get(bb as u16),
        O::ServerClient => m().server_clients().list().clients.get(bb as u16),
        O::ClientName => m().client().name.get(),
        O::ClientId => m().client().id.get() as isize,
        O::ClientPlayer => m().client().player.get(),
        O::ClientIsCaptain => b(m().client().game_captain.get()),
        // a LobbyClientSetIterator over a list (NULL: none)
        O::IterNew => Box::into_raw(Box::new((a, EeWonModelCursor::default()))) as isize,
        O::IterHasMore | O::IterNext => {
            // SAFETY: an iterator IterNew made
            let it = unsafe { &mut *(a as *mut (isize, EeWonModelCursor)) };
            // SAFETY: a live list
            let Some(l) = (unsafe { of(it.0) }) else {
                return Some(0);
            };
            if op == O::IterHasMore {
                b(l.set_has_more(&mut it.1))
            } else {
                let mut client = 0isize;
                l.set_next(&mut it.1, &mut client);
                client
            }
        }
        O::IterFree => {
            // SAFETY: an iterator IterNew made, freed once
            drop(unsafe { Box::from_raw(a as *mut (isize, EeWonModelCursor)) });
            0
        }
        _ => return None,
    })
}

#[cfg(test)]
#[path = "modelobj_golden.rs"]
mod golden;

#[cfg(test)]
#[path = "modelobj_tests.rs"]
mod tests;

#[cfg(test)]
#[path = "routerec_tests.rs"]
mod routerec_tests;
