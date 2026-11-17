//! The lobby's data model (LobbyModel.cpp): the decisions of its client, group, game and server lists. The C++
//! keeps the objects (`LobbyClient`, `LobbyGroup`, `LobbyGame`, `LobbyServer` and their `std::map`s of
//! `SmartPtr`s), which the game code and the GUI hold; Rust answers what they do with them:
//!
//! - the merge of a routing server's client or room map into the lobby's (`sync_plan`): which entries are dropped,
//!   which are new and which are refreshed, in the order the original walked both maps;
//! - a server's population (`Population`): the people in its rooms and in its chat rooms;
//! - what a client's and a member's flags say (`client_status`), what a room's flags say (`group_kind`);
//! - a friend's status from a client, and the locate mark;
//! - a game's ping states, when a LAN game is stale, when two games are the same, how a game list is keyed, which
//!   text a join status has, whether every player is ready;
//! - a directory entry's population object (`_C`) and address list (`_L`).
//!
//! The routing maps are keyed by their entries' own ids (`SmartRoutingConnection`, `RoutingGet*ListOp`), so the
//! merge inserts under the key.

/// `LobbyGame::mPing`'s states besides a time (0xfffe, unknown, is the C++'s): a ping outstanding, a details
/// query outstanding.
pub(crate) const PING_PINGING: u16 = 0xfffd;
pub(crate) const PING_QUERYING: u16 = 0xfffc;

/// A client's flags on its routing server.
pub(crate) const CLIENT_MODERATOR: u32 = 1;
pub(crate) const CLIENT_MUTED: u32 = 2;
pub(crate) const CLIENT_AWAY: u32 = 8;
pub(crate) const CLIENT_ADMIN: u32 = 0x20;
pub(crate) const CLIENT_ANONYMOUS: u32 = 0x40;
/// A member's flags in one room.
pub(crate) const MEMBER_MUTED: u8 = 2;

/// A room's flags.
pub(crate) const GROUP_PASSWORD: u32 = 1 << 2;
pub(crate) const GROUP_INVITE_ONLY: u32 = 1 << 7;
pub(crate) const GROUP_ASK_TO_JOIN: u32 = 1 << 8;
pub(crate) const GROUP_CHAT: u32 = 1 << 11;

/// A friend's status flags: in a chat room, in a game, being located.
pub(crate) const FRIEND_IN_CHAT: u8 = 1;
pub(crate) const FRIEND_IN_GAME: u8 = 2;
pub(crate) const FRIEND_LOCATING: u8 = 4;

/// One step of a merge, over the lobby's entries (`existing`, an index into the sorted ids) and the server's
/// (`incoming`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SyncStep {
    /// Drop the lobby's entry.
    Erase(u32),
    /// Create an entry from the server's (inserted at the walk's hint).
    Insert(u32),
    /// Refresh the lobby's entry from the server's.
    Update(u32, u32),
    /// Drop the lobby's entries from this one to the end (one range erase, as the original).
    EraseRest(u32),
}

/// `LobbyClientList::SyncClients` / `LobbyServer::SyncGroups`: the sorted merge of both id lists (each strictly
/// ascending, as a `std::map`'s keys). While both have entries: a server id above the lobby's drops the lobby's,
/// below it creates one, equal refreshes; then the lobby's rest is dropped and the server's rest created.
pub fn sync_plan(existing: &[u16], incoming: &[u16]) -> Vec<SyncStep> {
    let mut out = Vec::with_capacity(existing.len() + incoming.len());
    let (mut e, mut i) = (0usize, 0usize);
    while e < existing.len() && i < incoming.len() {
        if incoming[i] > existing[e] {
            out.push(SyncStep::Erase(e as u32));
            e += 1;
        } else if incoming[i] < existing[e] {
            out.push(SyncStep::Insert(i as u32));
            i += 1;
        } else {
            out.push(SyncStep::Update(e as u32, i as u32));
            e += 1;
            i += 1;
        }
    }
    if e < existing.len() {
        out.push(SyncStep::EraseRest(e as u32));
    }
    while i < incoming.len() {
        out.push(SyncStep::Insert(i as u32));
        i += 1;
    }
    out
}

/// A server's population as `SyncGroups`, `AddClientToGroup` and `RemoveClientFromGroup` count it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonPopulation {
    /// The people in chat rooms (`mNumChatClients`).
    pub chat: i32,
    /// The people in all rooms (`mNumClients`).
    pub total: i32,
}

impl EeWonPopulation {
    /// A room read from the server: its members count, in the chat count too for a chat room. The room's count is
    /// an `unsigned long` added into the `int`s (wrapping).
    pub fn add_group(&mut self, flags: u32, members: u32) {
        if group_kind(flags).chat {
            self.chat = self.chat.wrapping_add(members as i32);
        }
        self.total = self.total.wrapping_add(members as i32);
    }

    /// A client entered a room (only the chat count moves).
    pub fn joined(&mut self, flags: u32) {
        if group_kind(flags).chat {
            self.chat = self.chat.wrapping_add(1);
        }
    }

    /// A client left a room (the chat count moves, never below 0).
    pub fn left(&mut self, flags: u32) {
        if group_kind(flags).chat {
            self.chat = self.chat.wrapping_sub(1);
            if self.chat < 0 {
                self.chat = 0;
            }
        }
    }
}

/// `LobbyServer::GetNumClients`: the live client count while connected, else the directory's.
pub fn server_num_clients(connected: bool, live: u32, listed: i32) -> i32 {
    if connected {
        live as i32
    } else {
        listed
    }
}

/// What a client's flags say (`IsAnonymous`, `IsAway`, `IsAdmin`, `IsModerator`, `IsMuted`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonClientStatus {
    pub anonymous: bool,
    pub away: bool,
    pub admin: bool,
    pub moderator: bool,
    /// Muted server-wide or in the chat room / in the game.
    pub muted_chat: bool,
    pub muted_game: bool,
}

pub fn client_status(flags: u32, chat_member: u8, game_member: u8) -> EeWonClientStatus {
    let server_muted = flags & CLIENT_MUTED != 0;
    EeWonClientStatus {
        anonymous: flags & CLIENT_ANONYMOUS != 0,
        away: flags & CLIENT_AWAY != 0,
        admin: flags & CLIENT_ADMIN != 0,
        moderator: flags & CLIENT_MODERATOR != 0,
        muted_chat: server_muted || chat_member & MEMBER_MUTED != 0,
        muted_game: server_muted || game_member & MEMBER_MUTED != 0,
    }
}

/// What a room's flags say.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonGroupKind {
    pub password: bool,
    pub invite_only: bool,
    pub ask_to_join: bool,
    /// A chat room; otherwise a game.
    pub chat: bool,
    /// A chat room anyone may enter: no password, invitation or approval.
    pub public_chat: bool,
}

pub fn group_kind(flags: u32) -> EeWonGroupKind {
    let password = flags & GROUP_PASSWORD != 0;
    let invite_only = flags & GROUP_INVITE_ONLY != 0;
    let ask_to_join = flags & GROUP_ASK_TO_JOIN != 0;
    let chat = flags & GROUP_CHAT != 0;
    EeWonGroupKind {
        password,
        invite_only,
        ask_to_join,
        chat,
        public_chat: chat && !password && !invite_only && !ask_to_join,
    }
}

/// `LobbyFriend::SetStatusFromClient`'s flags.
pub fn friend_status(in_chat: bool, in_game: bool) -> u8 {
    let mut f = 0;
    if in_chat {
        f = FRIEND_IN_CHAT;
    }
    if in_game {
        f |= FRIEND_IN_GAME;
    }
    f
}

/// `StartLocate` replaces the flags with the locating mark; `EndLocate` clears only the mark.
pub fn friend_start_locate() -> u8 {
    FRIEND_LOCATING
}

pub fn friend_end_locate(flags: u8) -> u8 {
    flags & !FRIEND_LOCATING
}

/// `IsPinging` / `IsQueryingDetails`.
pub fn game_pinging(ping: u16) -> bool {
    ping == PING_PINGING || ping == PING_QUERYING
}

pub fn game_querying(ping: u16) -> bool {
    ping == PING_QUERYING
}

/// `LobbyGameList::RemoveStaleGames`'s rule: unseen for longer than the age (seconds; `time_t` minus the game's
/// `long`, compared with the `long`).
pub fn game_stale(now: i64, last_seen: i32, max_age: i32) -> bool {
    now.wrapping_sub(last_seen as i64) > max_age as i64
}

/// Whether a game list keys its games by room id (Internet games) rather than by address.
pub fn game_list_by_id(game_type: i32) -> bool {
    game_type == crate::staging::GAME_INTERNET
}

/// `LobbyGame::IsSameGame`: the same type and the same room (Internet) or the same address.
pub fn same_game(type_a: i32, type_b: i32, group_a: u16, group_b: u16, same_address: bool) -> bool {
    if type_a != type_b {
        return false;
    }
    if game_list_by_id(type_a) {
        group_a == group_b
    } else {
        same_address
    }
}

/// `LobbyGame::GetStatusString`: the lobby's text for a join status (`STATUS_TEXT_*`, in the order of the C++'s
/// table), or `None` to ask the game code and then print the number.
pub fn status_text(status: i16) -> Option<u8> {
    match status {
        0 => Some(0),
        2 => Some(1),
        3 => Some(2),
        4 => Some(3),
        5 => Some(4),
        6 => Some(5),
        7 => Some(6),
        8 => Some(7),
        _ => None,
    }
}

/// A game member as `AreAllReady` sees it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonMemberReady {
    pub has_player: bool,
    pub game_captain: bool,
    pub ready: bool,
}

/// `LobbyGame::AreAllReady`: every player but the captain clicked ready.
pub fn all_ready(members: &[EeWonMemberReady]) -> bool {
    members.iter().all(|m| !m.has_player || m.game_captain || m.ready)
}

/// A directory entry's data object as `LobbyServer::ReadDirEntity` reads it.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DirObject {
    /// `_C` with two bytes: the server's population (little-endian).
    Population(u16),
    /// `_L`: the server's address names (a StringParser list).
    Addresses,
    /// Anything else, a `_C` of another length or without data.
    Other,
}

pub fn dir_object(kind: &[u8], data: Option<&[u8]>) -> DirObject {
    match kind {
        b"_C" => match data {
            Some(&[lo, hi]) => DirObject::Population(u16::from_le_bytes([lo, hi])),
            _ => DirObject::Other,
        },
        b"_L" => DirObject::Addresses,
        _ => DirObject::Other,
    }
}

// ---- the C ABI

/// A `SyncStep` for the C++: `kind` 0 erase, 1 insert, 2 update, 3 erase the rest.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonSyncStep {
    pub kind: u8,
    pub existing: u32,
    pub incoming: u32,
}

impl From<SyncStep> for EeWonSyncStep {
    fn from(s: SyncStep) -> Self {
        match s {
            SyncStep::Erase(e) => EeWonSyncStep { kind: 0, existing: e, incoming: 0 },
            SyncStep::Insert(i) => EeWonSyncStep { kind: 1, existing: 0, incoming: i },
            SyncStep::Update(e, i) => EeWonSyncStep { kind: 2, existing: e, incoming: i },
            SyncStep::EraseRest(e) => EeWonSyncStep { kind: 3, existing: e, incoming: 0 },
        }
    }
}

unsafe fn ids<'a>(p: *const u16, n: usize) -> &'a [u16] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: `n` ids (the contract)
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// `sync_plan` into `out` (room for `existing_count + incoming_count` steps); returns the step count.
///
/// # Safety
/// The id arrays hold their counts; `out` has room for both counts' sum.
#[no_mangle]
pub unsafe extern "C" fn ee_won_model_sync_plan(
    existing: *const u16,
    existing_count: usize,
    incoming: *const u16,
    incoming_count: usize,
    out: *mut EeWonSyncStep,
) -> usize {
    let plan = sync_plan(unsafe { ids(existing, existing_count) }, unsafe { ids(incoming, incoming_count) });
    for (k, s) in plan.iter().enumerate() {
        // SAFETY: room for every step (the contract; a plan is never longer than both lists)
        unsafe { *out.add(k) = (*s).into() };
    }
    plan.len()
}

/// `EeWonPopulation::add_group` (0), `joined` (1) or `left` (2) in place.
///
/// # Safety
/// `p` is valid and writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_model_population(p: *mut EeWonPopulation, what: i32, flags: u32, members: u32) {
    // SAFETY: valid (the contract)
    let p = unsafe { &mut *p };
    match what {
        0 => p.add_group(flags, members),
        1 => p.joined(flags),
        _ => p.left(flags),
    }
}

#[no_mangle]
pub extern "C" fn ee_won_model_server_num_clients(connected: bool, live: u32, listed: i32) -> i32 {
    server_num_clients(connected, live, listed)
}

#[no_mangle]
pub extern "C" fn ee_won_model_client_status(flags: u32, chat_member: u8, game_member: u8) -> EeWonClientStatus {
    client_status(flags, chat_member, game_member)
}

#[no_mangle]
pub extern "C" fn ee_won_model_group_kind(flags: u32) -> EeWonGroupKind {
    group_kind(flags)
}

#[no_mangle]
pub extern "C" fn ee_won_model_friend_status(in_chat: bool, in_game: bool) -> u8 {
    friend_status(in_chat, in_game)
}

#[no_mangle]
pub extern "C" fn ee_won_model_friend_start_locate() -> u8 {
    friend_start_locate()
}

#[no_mangle]
pub extern "C" fn ee_won_model_friend_end_locate(flags: u8) -> u8 {
    friend_end_locate(flags)
}

#[no_mangle]
pub extern "C" fn ee_won_model_game_pinging(ping: u16) -> bool {
    game_pinging(ping)
}

#[no_mangle]
pub extern "C" fn ee_won_model_game_querying(ping: u16) -> bool {
    game_querying(ping)
}

#[no_mangle]
pub extern "C" fn ee_won_model_game_stale(now: i64, last_seen: i32, max_age: i32) -> bool {
    game_stale(now, last_seen, max_age)
}

#[no_mangle]
pub extern "C" fn ee_won_model_game_list_by_id(game_type: i32) -> bool {
    game_list_by_id(game_type)
}

#[no_mangle]
pub extern "C" fn ee_won_model_same_game(
    type_a: i32,
    type_b: i32,
    group_a: u16,
    group_b: u16,
    same_address: bool,
) -> bool {
    same_game(type_a, type_b, group_a, group_b, same_address)
}

/// `status_text`: the text's index, or -1.
#[no_mangle]
pub extern "C" fn ee_won_model_status_text(status: i16) -> i32 {
    status_text(status).map_or(-1, i32::from)
}

/// `all_ready` over `count` members.
///
/// # Safety
/// `members` holds `count` entries (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_model_all_ready(members: *const EeWonMemberReady, count: usize) -> bool {
    let m = if members.is_null() || count == 0 {
        &[][..]
    } else {
        // SAFETY: `count` entries (the contract)
        unsafe { std::slice::from_raw_parts(members, count) }
    };
    all_ready(m)
}

/// `dir_object`: 1 a population (into `out_count`), 2 the address list, 0 anything else. `data` NULL: no data.
///
/// # Safety
/// `kind` holds `kind_length` bytes, `data` `data_length` (or is NULL); `out_count` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_model_dir_object(
    kind: *const u8,
    kind_length: usize,
    data: *const u8,
    data_length: usize,
    out_count: *mut u16,
) -> i32 {
    let k = if kind.is_null() || kind_length == 0 {
        &[][..]
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(kind, kind_length) }
    };
    let d = if data.is_null() {
        None
    } else if data_length == 0 {
        Some(&[][..])
    } else {
        // SAFETY: the contract
        Some(unsafe { std::slice::from_raw_parts(data, data_length) })
    };
    match dir_object(k, d) {
        DirObject::Population(n) => {
            // SAFETY: writable (the contract)
            unsafe { *out_count = n };
            1
        }
        DirObject::Addresses => 2,
        DirObject::Other => 0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The original's walk over two `std::map`s, as a model: what the lobby's map holds afterwards and which
    /// entries were refreshed.
    fn walk(existing: &[u16], incoming: &[u16]) -> (Vec<u16>, Vec<u16>) {
        let mut out = Vec::new();
        let mut refreshed = Vec::new();
        for s in sync_plan(existing, incoming) {
            match s {
                SyncStep::Erase(_) | SyncStep::EraseRest(_) => {}
                SyncStep::Insert(i) => out.push(incoming[i as usize]),
                SyncStep::Update(e, i) => {
                    assert_eq!(existing[e as usize], incoming[i as usize]);
                    out.push(existing[e as usize]);
                    refreshed.push(existing[e as usize]);
                }
            }
        }
        out.sort_unstable();
        (out, refreshed)
    }

    #[test]
    fn the_merge_ends_with_the_servers_ids() {
        assert_eq!(walk(&[1, 3, 5], &[2, 3, 6]), (vec![2, 3, 6], vec![3]));
        assert_eq!(walk(&[], &[4, 9]), (vec![4, 9], vec![]));
        assert_eq!(walk(&[4, 9], &[]), (vec![], vec![]));
        assert_eq!(
            sync_plan(&[1, 3, 5, 7], &[3, 4]),
            vec![SyncStep::Erase(0), SyncStep::Update(1, 0), SyncStep::Insert(1), SyncStep::EraseRest(2)]
        );
        assert_eq!(sync_plan(&[5], &[1, 2]), vec![SyncStep::Insert(0), SyncStep::Insert(1), SyncStep::EraseRest(0)]);
        assert!(sync_plan(&[], &[]).is_empty());
        // every pair of small id sets
        for a in 0u32..64 {
            for b in 0u32..64 {
                let ea: Vec<u16> = (0..6).filter(|k| a >> k & 1 != 0).collect();
                let ib: Vec<u16> = (0..6).filter(|k| b >> k & 1 != 0).collect();
                let (got, refreshed) = walk(&ea, &ib);
                assert_eq!(got, ib);
                let both: Vec<u16> = ea.iter().copied().filter(|x| ib.contains(x)).collect();
                assert_eq!(refreshed, both);
                assert!(sync_plan(&ea, &ib).len() <= ea.len() + ib.len());
            }
        }
    }

    #[test]
    fn population_counts_chat_rooms_and_clamps() {
        let mut p = EeWonPopulation::default();
        p.add_group(GROUP_CHAT, 4);
        p.add_group(0, 3);
        assert_eq!(p, EeWonPopulation { chat: 4, total: 7 });
        p.joined(GROUP_CHAT);
        p.joined(0);
        assert_eq!(p.chat, 5);
        let mut q = EeWonPopulation::default();
        q.left(GROUP_CHAT);
        assert_eq!(q.chat, 0);
        q.left(0);
        assert_eq!(q, EeWonPopulation::default());
        assert_eq!(server_num_clients(true, 12, 99), 12);
        assert_eq!(server_num_clients(false, 12, 99), 99);
    }

    #[test]
    fn flags() {
        let s = client_status(0x6b, 0, 2);
        assert!(s.anonymous && s.away && s.admin && s.moderator && s.muted_chat && s.muted_game);
        let s = client_status(0, 2, 0);
        assert!(s.muted_chat && !s.muted_game && !s.anonymous);
        let g = group_kind(GROUP_CHAT);
        assert!(g.chat && g.public_chat);
        let g = group_kind(GROUP_CHAT | GROUP_ASK_TO_JOIN);
        assert!(g.chat && g.ask_to_join && !g.public_chat);
        assert!(!group_kind(GROUP_PASSWORD).chat);
        assert_eq!(friend_status(true, true), 3);
        assert_eq!(friend_status(false, true), 2);
        assert_eq!(friend_end_locate(7), 3);
        assert_eq!(friend_start_locate(), 4);
    }

    #[test]
    fn games() {
        assert!(game_pinging(0xfffd) && game_pinging(0xfffc) && !game_pinging(0xfffe));
        assert!(game_querying(0xfffc) && !game_querying(0xfffd));
        assert!(game_stale(131, 100, 30) && !game_stale(130, 100, 30));
        assert!(game_list_by_id(2) && !game_list_by_id(1) && !game_list_by_id(3));
        assert!(same_game(2, 2, 5, 5, false) && !same_game(2, 2, 5, 6, true));
        assert!(same_game(1, 1, 5, 6, true) && !same_game(1, 3, 0, 0, true));
        assert_eq!(status_text(0), Some(0));
        assert_eq!(status_text(1), None);
        assert_eq!(status_text(8), Some(7));
        assert_eq!(status_text(-1), None);
        let m = |has_player, game_captain, ready| EeWonMemberReady { has_player, game_captain, ready };
        assert!(all_ready(&[]));
        assert!(all_ready(&[m(true, true, false), m(false, false, false), m(true, false, true)]));
        assert!(!all_ready(&[m(true, false, false)]));
    }

    #[test]
    fn directory_objects() {
        assert_eq!(dir_object(b"_C", Some(&[0x34, 0x12])), DirObject::Population(0x1234));
        assert_eq!(dir_object(b"_C", Some(&[1])), DirObject::Other);
        assert_eq!(dir_object(b"_C", None), DirObject::Other);
        assert_eq!(dir_object(b"_L", None), DirObject::Addresses);
        assert_eq!(dir_object(b"_c", Some(&[1, 2])), DirObject::Other);
    }
}
