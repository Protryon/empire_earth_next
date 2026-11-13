//! WONRouting's `SmartRoutingConnection` model as one Rust object (net round 17): the client, group and member maps
//! (shared records: a member's client is the client map's record, as the original's `SmartPtr`s were), the three
//! iterators the snapshot calls walk and the erase rules that step them, the records a notification was about
//! (`mHandlerClient` / `Group` / `Member`), this client's id, record and async flags, the default group, the ignore
//! list and its file, the chat command table and settings, and what the last chat input left (its error, command,
//! target member and client, the name it matched). Every step of `RoutingOpCompleteHook` (which op type updates
//! what), the chat input's parse (`HandleInput` / `HandleCommand`: the request it makes is returned as an
//! [`EeWonSmartAction`] for the C++ to send through its request helpers) and the snapshots are here.
//!
//! The C++ class keeps the connection (its socket, op list and requests) and hands each finished op's fields in:
//! its lists first (`ee_won_smart_put_*`), then the op itself (`ee_won_smart_op`). The C++ holds the connection's
//! data lock around every call. Text is `wchar_t` units (`u32` here, any width); the case maps and character
//! classes are the C runtime's (chat.rs's `CrtChat`), as the original called them. The behaviour is
//! `golden_smartroute.txt` (won_smartroute_test over 127.0.0.1, recorded over the C++ class before the move).
//!
//! The original's undefined corners, defined: an iterator a whole-map assignment or clear invalidated is at the end;
//! a member without a client is skipped where the original dereferenced it; a command whose target is missing names
//! client 0; a non-ASCII duration unit is no letter.

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;
use std::ops::Bound;
use std::rc::Rc;

use crate::chat::{self, ChatCtype, CrtChat};
use crate::ffi::EeWonByteSink;
use crate::{strcase, text};

// ---- the records

#[derive(Clone, Debug, Default)]
struct Client {
    id: u16,
    name: Vec<u32>,
    flags: u32,
    /// `mAddress`: the CD key id
    address: u32,
    num_groups: u16,
    ignored: bool,
}

#[derive(Debug)]
struct Member {
    id: u16,
    flags: u8,
    client: Option<CRef>,
}

#[derive(Debug, Default)]
struct Group {
    id: u16,
    name: Vec<u32>,
    captain: u16,
    max_clients: u16,
    flags: u32,
    async_flags: u32,
    observers: u16,
    /// `mMaxPlayers`: the group list's member count
    max_players: u16,
    members: BTreeMap<u16, MRef>,
}

type CRef = Rc<RefCell<Client>>;
type MRef = Rc<RefCell<Member>>;
type GRef = Rc<RefCell<Group>>;

/// A client record an op made (its list or its own client), handed in before the op.
#[derive(Clone, Debug, Default)]
struct NewClient {
    id: u16,
    name: Vec<u32>,
    flags: u32,
    address: u32,
}

/// A member record an op made, with its client record's name and flags when it has one.
#[derive(Clone, Debug, Default)]
struct NewMember {
    id: u16,
    flags: u8,
    client: Option<(Vec<u32>, u32)>,
}

/// A group record an op made.
#[derive(Clone, Debug, Default)]
struct NewGroup {
    id: u16,
    name: Vec<u32>,
    captain: u16,
    max_clients: u16,
    flags: u32,
    async_flags: u32,
    observers: u16,
    max_players: u16,
    members: Vec<NewMember>,
}

// ---- the numbers of the C++ enums

const MATCH_NOT_FOUND: i32 = 0;
const MATCH_AMBIGUOUS: i32 = 1;
const MATCH_PARTIAL: i32 = 2;
const MATCH_EXACT: i32 = 3;

const ERR_NONE: i32 = 0;
const ERR_INVALID_COMMAND: i32 = 1;
const ERR_CLIENT_NOT_FOUND: i32 = 2;
const ERR_CLIENT_AMBIGUOUS: i32 = 3;
const ERR_NO_CHAT: i32 = 4;
const ERR_NOT_LOGGED_IN: i32 = 5;
const ERR_BAD_TIME: i32 = 6;

const CMD_NONE: i32 = 0;
const CMD_BROADCAST: i32 = 1;
const CMD_WHISPER: i32 = 2;
const CMD_EMOTE: i32 = 3;
const CMD_IGNORE: i32 = 4;
const CMD_AWAY: i32 = 5;
const CMD_MODERATOR: i32 = 6;
const CMD_SERVER_MUTE: i32 = 7;
const CMD_SERVER_BAN: i32 = 8;
const CMD_MUTE: i32 = 9;
const CMD_BAN: i32 = 10;
const CMD_UNMUTE: i32 = 11;
const CMD_SERVER_UNMUTE: i32 = 12;
const CMD_INVITE: i32 = 13;
const CMD_UNINVITE: i32 = 14;

const CHAT_EMOTE: u32 = 2;
const CHAT_WHISPER: u32 = 4;
const CLIENT_MODERATOR: u32 = 1;
const CLIENT_AWAY: u32 = 8;
const ROUTING_GLOBAL: u16 = 0xffff;

// the op types the hook reads (WONRoutingOpTypes.h)
const OP_CLIENT_FLAGS: i32 = 6;
const OP_CLIENT_JOINED_GROUP: i32 = 7;
const OP_CLIENT_JOINED_SERVER: i32 = 8;
const OP_CLIENT_LEFT_GROUP: i32 = 9;
const OP_CLIENT_LEFT_SERVER: i32 = 10;
const OP_DATA_CREATED: i32 = 13;
const OP_DATA_MODIFIED: i32 = 15;
const OP_GET_CLIENT_LIST: i32 = 20;
const OP_GET_GROUP_LIST: i32 = 21;
const OP_GET_MEMBERS: i32 = 22;
const OP_CAPTAIN: i32 = 23;
const OP_GROUP_CREATED: i32 = 24;
const OP_GROUP_DELETED: i32 = 25;
const OP_GROUP_FLAGS: i32 = 26;
const OP_INVITATION: i32 = 27;
const OP_JOIN_ATTEMPT: i32 = 28;
const OP_MAX_PLAYERS: i32 = 29;
const OP_GROUP_NAME: i32 = 31;
const OP_OBSERVERS: i32 = 32;
const OP_JOIN_GROUP: i32 = 34;
const OP_MEMBER_FLAGS: i32 = 36;
const OP_PEER_CHAT: i32 = 39;
const OP_READ_DATA: i32 = 41;
const OP_REGISTER: i32 = 42;
const OP_SET_CLIENT_FLAGS: i32 = 51;

/// The data type whose objects carry the clients' CD key ids.
const KEY_ID_TYPE: &[u8] = b"_KeyId";

// ---- the action a chat input makes

/// No request.
pub const EE_WON_SMART_ACT_NONE: i32 = 0;
/// `SendChat(text, flags, group, NULL)` (`group`: the group, or a whisper's client)
pub const EE_WON_SMART_ACT_CHAT: i32 = 1;
/// `SetClientFlags(flags, value, 0, 0, NULL)`
pub const EE_WON_SMART_ACT_CLIENT_FLAGS: i32 = 2;
/// `InviteClient(group, client, on, text, NULL)`
pub const EE_WON_SMART_ACT_INVITE: i32 = 3;
/// `BecomeModerator(on, NULL)`
pub const EE_WON_SMART_ACT_MODERATOR: i32 = 4;
/// `MuteClient(group, on, seconds, text, client, NULL)`
pub const EE_WON_SMART_ACT_MUTE: i32 = 5;
/// `BanClient(group, on, seconds, text, client, NULL)`
pub const EE_WON_SMART_ACT_BAN: i32 = 6;
/// `BanClientByCDKeyId(group, on, seconds, text, value, NULL)`
pub const EE_WON_SMART_ACT_BAN_CDKEY: i32 = 7;

/// The request a chat input makes, for the C++ to send (`text` points into the object until its next call).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonSmartAction {
    pub kind: i32,
    pub group: u16,
    pub client: u16,
    pub flags: u32,
    pub value: u32,
    pub seconds: u32,
    pub on: bool,
    pub text: *const u32,
    pub text_len: usize,
}

impl Default for EeWonSmartAction {
    fn default() -> Self {
        EeWonSmartAction {
            kind: EE_WON_SMART_ACT_NONE,
            group: 0,
            client: 0,
            flags: 0,
            value: 0,
            seconds: 0,
            on: false,
            text: std::ptr::null(),
            text_len: 0,
        }
    }
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
struct Action {
    kind: i32,
    group: u16,
    client: u16,
    flags: u32,
    value: u32,
    seconds: u32,
    on: bool,
    text: Vec<u32>,
}

/// A finished op's fields (by op type; see the C++ shim, SmartRoutingConnection.cpp).
#[derive(Clone, Debug, Default)]
pub struct Op {
    pub kind: i32,
    pub status: i32,
    pub group: u16,
    pub client: u16,
    pub a: u32,
    pub b: u32,
    pub c: u32,
    pub has: bool,
    pub text: Vec<u32>,
    pub data_type: Vec<u8>,
    pub data: Vec<u8>,
}

/// A record's copy (`ClientInfo`, `GroupInfo`, `MemberInfo`): the fields of its kind are set.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Copy {
    pub id: u16,
    pub name: Vec<u32>,
    pub flags: u32,
    pub cdkey: u32,
    pub captain: u16,
    pub max_clients: u16,
    pub async_flags: u32,
    pub observers: u16,
    pub member_count: u16,
    pub client_flags: u32,
    pub member_flags: u8,
    /// a member's: it has a client record (its name and flags are set)
    pub has_client: bool,
}

// ---- the model

/// `SmartRoutingConnection`'s state (see the module).
pub struct Model {
    wchar_size: u32,
    groups: BTreeMap<u16, GRef>,
    clients: BTreeMap<u16, CRef>,
    /// the key the client iterator is at (`None`: the end)
    client_itr: Option<u16>,
    group_itr: Option<u16>,
    /// the member iterator: its group's map and key
    member_itr: Option<(GRef, Option<u16>)>,
    member_itr_group: Option<GRef>,
    default_group: u16,
    my_client: Option<CRef>,
    client_id: u16,
    async_flags: u32,
    commands: BTreeMap<Vec<u32>, i32>,
    tolerance: i32,
    server_wide: bool,
    colon: bool,
    last_error: i32,
    last_command: i32,
    last_member: Option<MRef>,
    last_client: Option<CRef>,
    last_name: Vec<u32>,
    h_client: Option<CRef>,
    h_group: Option<GRef>,
    h_member: Option<MRef>,
    /// `mIgnoreSet`, in `WStringCompareNoCase` order
    ignore: Vec<Vec<u32>>,
    ignore_path: Vec<u8>,
    // what the C++ handed in for the next op
    put_clients: Vec<NewClient>,
    put_members: Vec<NewMember>,
    put_groups: Vec<NewGroup>,
    put_objects: Vec<(u16, Vec<u8>)>,
    /// a text handed out (a copy's name, an action's text)
    scratch: Vec<u32>,
    scratch2: Vec<u32>,
}

fn next_key<V>(map: &BTreeMap<u16, V>, key: u16) -> Option<u16> {
    map.range((Bound::Excluded(key), Bound::Unbounded)).next().map(|(&k, _)| k)
}

fn first_key<V>(map: &BTreeMap<u16, V>) -> Option<u16> {
    map.keys().next().copied()
}

/// A client's group count up (`mNumGroups++`, wrapping as the 16-bit original).
fn bump_groups(c: &CRef) {
    let mut c = c.borrow_mut();
    c.num_groups = c.num_groups.wrapping_add(1);
}

fn upper(text: &[u32]) -> Vec<u32> {
    text.iter().map(|&u| strcase::upper_unit(u)).collect()
}

impl Model {
    /// The constructor: the ignore file's path, the default commands, partial names, group-wide commands, colon
    /// emotes. `wchar_size`: the C++'s `wchar_t` (2 or 4 bytes; the ignore file is UTF-16 either way).
    pub fn new(wchar_size: u32, ignore_path: &[u8]) -> Model {
        let mut m = Model {
            wchar_size,
            groups: BTreeMap::new(),
            clients: BTreeMap::new(),
            client_itr: None,
            group_itr: None,
            member_itr: None,
            member_itr_group: None,
            default_group: 0,
            my_client: None,
            client_id: 0,
            async_flags: 0,
            commands: BTreeMap::new(),
            tolerance: MATCH_PARTIAL,
            server_wide: false,
            colon: true,
            last_error: ERR_NONE,
            last_command: CMD_NONE,
            last_member: None,
            last_client: None,
            last_name: Vec::new(),
            h_client: None,
            h_group: None,
            h_member: None,
            ignore: Vec::new(),
            ignore_path: ignore_path.to_vec(),
            put_clients: Vec::new(),
            put_members: Vec::new(),
            put_groups: Vec::new(),
            put_objects: Vec::new(),
            scratch: Vec::new(),
            scratch2: Vec::new(),
        };
        for (name, command) in [
            ("W", CMD_WHISPER),
            ("WHISPER", CMD_WHISPER),
            ("ME", CMD_EMOTE),
            ("EMOTE", CMD_EMOTE),
            ("I", CMD_IGNORE),
            ("IGNORE", CMD_IGNORE),
            ("AWAY", CMD_AWAY),
            ("AFK", CMD_AWAY),
            ("MOD", CMD_MODERATOR),
            ("SMUTE", CMD_SERVER_MUTE),
            ("SBAN", CMD_SERVER_BAN),
            ("MUTE", CMD_MUTE),
            ("BAN", CMD_BAN),
            ("UNMUTE", CMD_UNMUTE),
            ("SUNMUTE", CMD_SERVER_UNMUTE),
        ] {
            m.add_command(&name.chars().map(u32::from).collect::<Vec<_>>(), command);
        }
        m
    }

    // ---- the ignore set (WStringCompareNoCase over the units as wchar_t)

    fn wide_key(&self, t: &[u32]) -> Vec<i64> {
        if self.wchar_size == 2 {
            t.iter().map(|&u| i64::from(u)).collect()
        } else {
            t.iter().map(|&u| i64::from(u as i32)).collect()
        }
    }

    fn ignore_search(&self, name: &[u32]) -> Result<usize, usize> {
        let key = self.wide_key(name);
        self.ignore.binary_search_by(|k| strcase::wide_compare_no_case(&self.wide_key(k), &key).cmp(&0))
    }

    fn is_ignored(&self, name: &[u32]) -> bool {
        self.ignore_search(name).is_ok()
    }

    fn ignore_insert(&mut self, name: &[u32]) {
        if let Err(i) = self.ignore_search(name) {
            self.ignore.insert(i, name.to_vec());
        }
    }

    fn ignore_erase(&mut self, name: &[u32]) {
        if let Ok(i) = self.ignore_search(name) {
            self.ignore.remove(i);
        }
    }

    /// `LoadIgnoreSet`: the names in the file (UTF-16 LE, each ended by a 16-bit NUL) added to the set.
    fn load_ignore(&mut self) {
        if self.ignore_path.is_empty() {
            return;
        }
        let Some(bytes) = crate::authlogin::read_file(&self.ignore_path, c"rb") else {
            return;
        };
        let mut name: Vec<u32> = Vec::new();
        for pair in bytes.as_chunks::<2>().0 {
            let unit = u16::from_le_bytes(*pair);
            if unit != 0 {
                if self.wchar_size == 2 {
                    name.push(u32::from(unit));
                } else {
                    text::push_utf16_unit(&mut name, unit);
                }
            } else {
                let n = std::mem::take(&mut name);
                self.ignore_insert(&n);
            }
        }
    }

    /// The ignore file's bytes (`SaveIgnoreSet`).
    fn ignore_file_bytes(&self) -> Vec<u8> {
        let mut out = Vec::new();
        for name in &self.ignore {
            let units: Vec<u16> = if self.wchar_size == 2 {
                name.iter().map(|&u| u as u16).collect()
            } else {
                text::utf16_from_utf32(name)
            };
            out.extend(text::utf16_to_le_bytes(&units));
            out.extend([0, 0]);
        }
        out
    }

    fn save_ignore(&self) {
        if self.ignore_path.is_empty() {
            return;
        }
        crate::authlogin::write_file(&self.ignore_path, &self.ignore_file_bytes());
    }

    // ---- lookups and erases

    /// `GetClient(id)`: the handled client first, else the map's.
    fn client(&self, id: u16) -> Option<CRef> {
        if let Some(c) = &self.h_client {
            if c.borrow().id == id {
                return Some(c.clone());
            }
        }
        self.clients.get(&id).cloned()
    }

    /// `GetGroup(id)`: the handled group first, else the map's.
    fn group(&self, id: u16) -> Option<GRef> {
        if let Some(g) = &self.h_group {
            if g.borrow().id == id {
                return Some(g.clone());
            }
        }
        self.groups.get(&id).cloned()
    }

    /// `GetMemberRef(client, group)` (group -1: the default one): the handled member first.
    fn member_ref(&self, client: u16, group: i32) -> Option<MRef> {
        let group = if group == -1 { self.default_group } else { group as u16 };
        if let (Some(m), Some(g)) = (&self.h_member, &self.h_group) {
            if m.borrow().id == client && g.borrow().id == group {
                return Some(m.clone());
            }
        }
        let g = self.group(group)?;
        let m = g.borrow().members.get(&client).cloned();
        m
    }

    fn erase_client(&mut self, id: u16) {
        if !self.clients.contains_key(&id) {
            return;
        }
        if self.client_itr == Some(id) {
            self.client_itr = next_key(&self.clients, id);
        }
        self.clients.remove(&id);
    }

    fn erase_group(&mut self, id: u16) {
        let Some(g) = self.groups.get(&id).cloned() else {
            return;
        };
        if self.member_itr_group.as_ref().is_some_and(|m| Rc::ptr_eq(m, &g)) {
            self.member_itr_group = None;
        }
        if self.group_itr == Some(id) {
            self.group_itr = next_key(&self.groups, id);
        }
        self.groups.remove(&id);
    }

    fn erase_member(&mut self, group: &GRef, id: u16) {
        if let Some((g, pos)) = &mut self.member_itr {
            if Rc::ptr_eq(g, group) && *pos == Some(id) {
                *pos = next_key(&group.borrow().members, id);
            }
        }
        group.borrow_mut().members.remove(&id);
    }

    fn set_client_name(&self, client: &CRef, name: &[u32]) {
        let ignored = self.is_ignored(name);
        let mut c = client.borrow_mut();
        c.name = name.to_vec();
        c.ignored = ignored;
    }

    fn ensure_client(&mut self, id: u16) -> CRef {
        self.clients.entry(id).or_insert_with(|| Rc::new(RefCell::new(Client { id, ..Default::default() }))).clone()
    }

    fn handle_client(&mut self, client: Option<CRef>) {
        self.h_client = client;
        self.h_group = None;
        self.h_member = None;
    }

    fn handle_group(&mut self, group: Option<GRef>, member: Option<MRef>) {
        self.h_client = member.as_ref().and_then(|m| m.borrow().client.clone());
        self.h_group = group;
        self.h_member = member;
    }

    fn sync_ignore(&mut self) {
        for c in self.clients.values() {
            let ignored = self.is_ignored(&c.borrow().name);
            c.borrow_mut().ignored = ignored;
        }
    }

    /// A client leaving a group: its group count down, and the client dropped when it is in no group, is not this
    /// client and the async flags say only group members are tracked.
    fn leave_count(&mut self, client: &Option<CRef>) {
        let Some(c) = client else {
            return;
        };
        let (id, groups) = {
            let mut c = c.borrow_mut();
            c.num_groups = c.num_groups.wrapping_sub(1);
            (c.id, c.num_groups)
        };
        if self.async_flags & 2 != 0 && id != self.client_id && groups == 0 {
            self.erase_client(id);
        }
    }

    /// A new member's client: the map's record (made when missing), its name and flags from the op's record.
    fn link_member(&mut self, m: &NewMember) -> CRef {
        let c = self.ensure_client(m.id);
        if let Some((name, flags)) = &m.client {
            if !name.is_empty() {
                self.set_client_name(&c, name);
            }
            if *flags != 0 {
                c.borrow_mut().flags = *flags;
            }
        }
        bump_groups(&c);
        c
    }

    /// `SyncMemberMapWithClientMap(old, new)`: the group's members merged with the op's.
    fn sync_members(&mut self, group: &GRef, new: &[NewMember]) {
        let mut new_sorted: Vec<&NewMember> = Vec::new();
        for m in new {
            // the op's map: one member per id, the first one read
            if !new_sorted.iter().any(|n| n.id == m.id) {
                new_sorted.push(m);
            }
        }
        new_sorted.sort_by_key(|m| m.id);
        let mut ni = 0;
        let mut old_pos = first_key(&group.borrow().members);
        while old_pos.is_some() || ni < new_sorted.len() {
            let take_old = match (old_pos, new_sorted.get(ni)) {
                (Some(_), None) => true,
                (Some(o), Some(n)) => o < n.id,
                _ => false,
            };
            if take_old {
                let o = old_pos.unwrap_or(0);
                let client = group.borrow().members.get(&o).and_then(|m| m.borrow().client.clone());
                self.leave_count(&client);
                old_pos = next_key(&group.borrow().members, o);
                self.erase_member(group, o);
                continue;
            }
            let n = new_sorted[ni];
            if old_pos.is_none() || Some(n.id) < old_pos {
                let c = self.link_member(n);
                let m = Rc::new(RefCell::new(Member { id: n.id, flags: n.flags, client: Some(c) }));
                group.borrow_mut().members.entry(n.id).or_insert(m);
                ni += 1;
            } else {
                let o = old_pos.unwrap_or(0);
                let old = group.borrow().members.get(&o).cloned();
                if let Some(old) = old {
                    old.borrow_mut().flags = n.flags;
                    let client = old.borrow().client.clone();
                    if let (Some((name, flags)), Some(c)) = (&n.client, client) {
                        if !name.is_empty() {
                            self.set_client_name(&c, name);
                        }
                        if *flags != 0 {
                            c.borrow_mut().flags = *flags;
                        }
                    }
                }
                old_pos = next_key(&group.borrow().members, o);
                ni += 1;
            }
        }
    }

    fn new_client(n: &NewClient) -> CRef {
        Rc::new(RefCell::new(Client {
            id: n.id,
            name: n.name.clone(),
            flags: n.flags,
            address: n.address,
            num_groups: 0,
            ignored: false,
        }))
    }

    /// The op's client map: by id, the first record of an id kept.
    fn client_map(list: &[NewClient]) -> BTreeMap<u16, NewClient> {
        let mut map = BTreeMap::new();
        for c in list {
            map.entry(c.id).or_insert_with(|| c.clone());
        }
        map
    }

    fn set_cdkey(&mut self, id: u16, data: &[u8]) {
        let Some(c) = self.client(id) else {
            return;
        };
        let key = if data.len() >= 8 { u32::from_le_bytes([data[4], data[5], data[6], data[7]]) } else { 0 };
        c.borrow_mut().address = key;
    }

    // ---- RoutingOpCompleteHook

    /// `RoutingOpCompleteHook(op)`: the model updated from the op (its lists put before); true when the op's
    /// completion is to be dropped (a chat, invitation or join attempt from an ignored client).
    pub fn op(&mut self, op: &Op) -> bool {
        let clients = std::mem::take(&mut self.put_clients);
        let members = std::mem::take(&mut self.put_members);
        let groups = std::mem::take(&mut self.put_groups);
        let objects = std::mem::take(&mut self.put_objects);
        self.h_client = None;
        self.h_group = None;
        self.h_member = None;
        let ok = op.status == 0;
        match op.kind {
            OP_CLIENT_FLAGS => {
                if let Some(c) = self.client(op.client) {
                    c.borrow_mut().flags = op.a;
                    self.handle_client(Some(c));
                }
            }
            OP_CLIENT_JOINED_GROUP => {
                let Some(g) = self.group(op.group) else {
                    return false;
                };
                let c = self.ensure_client(op.client);
                if !op.text.is_empty() {
                    self.set_client_name(&c, &op.text);
                }
                if op.has {
                    c.borrow_mut().flags = op.b;
                }
                let existing = g.borrow().members.get(&op.client).cloned();
                let member = match existing {
                    Some(m) => m,
                    None => {
                        let m =
                            Rc::new(RefCell::new(Member { id: op.client, flags: op.a as u8, client: Some(c.clone()) }));
                        g.borrow_mut().members.insert(op.client, m.clone());
                        bump_groups(&c);
                        m
                    }
                };
                self.handle_group(Some(g), Some(member));
            }
            OP_CLIENT_JOINED_SERVER => {
                let Some(n) = clients.first() else {
                    return false;
                };
                let c = Self::new_client(n);
                self.set_client_name(&c, &op.text);
                self.clients.insert(n.id, c.clone());
                self.handle_client(Some(c));
            }
            OP_CLIENT_LEFT_GROUP => {
                let Some(g) = self.group(op.group) else {
                    return false;
                };
                let member = g.borrow().members.get(&op.client).cloned();
                if let Some(m) = member {
                    let client = m.borrow().client.clone();
                    self.leave_count(&client);
                    self.handle_group(Some(g.clone()), Some(m));
                    self.erase_member(&g, op.client);
                }
            }
            OP_CLIENT_LEFT_SERVER => {
                if let Some(c) = self.clients.get(&op.client).cloned() {
                    self.handle_client(Some(c));
                    self.erase_client(op.client);
                }
            }
            OP_GET_CLIENT_LIST => {
                self.merge_clients(&Self::client_map(&clients));
                self.my_client = self.client(self.client_id);
                self.sync_ignore();
            }
            OP_GET_GROUP_LIST => self.merge_groups(&groups, op.has),
            OP_GET_MEMBERS => {
                if let Some(g) = self.group(op.group) {
                    g.borrow_mut().observers = op.a as u16;
                    self.sync_members(&g, &members);
                }
            }
            OP_CAPTAIN | OP_MAX_PLAYERS | OP_GROUP_FLAGS | OP_GROUP_NAME | OP_OBSERVERS => {
                if let Some(g) = self.group(op.group) {
                    {
                        let mut gm = g.borrow_mut();
                        match op.kind {
                            OP_CAPTAIN => gm.captain = op.a as u16,
                            OP_MAX_PLAYERS => gm.max_clients = op.a as u16,
                            OP_GROUP_FLAGS => gm.flags = op.a,
                            OP_GROUP_NAME => gm.name = op.text.clone(),
                            _ => gm.observers = op.a as u16,
                        }
                    }
                    self.handle_group(Some(g), None);
                }
            }
            OP_GROUP_CREATED => {
                let g = Rc::new(RefCell::new(Group {
                    id: op.group,
                    name: op.text.clone(),
                    captain: 0xffff,
                    max_clients: op.c as u16,
                    flags: op.a,
                    async_flags: op.b,
                    ..Default::default()
                }));
                self.groups.insert(op.group, g.clone());
                self.handle_group(Some(g), None);
            }
            OP_GROUP_DELETED => {
                if let Some(g) = self.groups.get(&op.group).cloned() {
                    self.handle_group(Some(g), None);
                    self.erase_group(op.group);
                }
            }
            OP_JOIN_GROUP => {
                if !ok {
                    return false;
                }
                self.default_group = op.group;
                let Some(g) = self.group(op.group) else {
                    return false;
                };
                if op.has {
                    self.sync_members(&g, &members);
                }
                self.handle_group(Some(g), None);
            }
            OP_MEMBER_FLAGS => {
                if let Some(g) = self.group(op.group) {
                    let member = g.borrow().members.get(&op.client).cloned();
                    if let Some(m) = member {
                        m.borrow_mut().flags = op.a as u8;
                        self.handle_group(Some(g), Some(m));
                    }
                }
            }
            OP_REGISTER => {
                if !ok {
                    return false;
                }
                self.load_ignore();
                self.client_id = op.client;
                self.async_flags = op.b;
                self.groups.clear();
                self.group_itr = None;
                self.clients = Self::client_map(&clients).iter().map(|(&k, n)| (k, Self::new_client(n))).collect();
                self.client_itr = None;
                if self.clients.is_empty() {
                    let me = Rc::new(RefCell::new(Client {
                        id: op.client,
                        name: op.text.clone(),
                        flags: op.a,
                        ..Default::default()
                    }));
                    self.clients.insert(op.client, me.clone());
                    self.my_client = Some(me);
                } else {
                    self.my_client = self.client(self.client_id);
                }
                self.sync_ignore();
            }
            OP_SET_CLIENT_FLAGS => {
                if ok {
                    self.async_flags = (self.async_flags & !op.a) | (op.b & op.a);
                }
            }
            OP_READ_DATA => {
                if op.data_type == KEY_ID_TYPE {
                    for (link, data) in &objects {
                        self.set_cdkey(*link, data);
                    }
                }
            }
            OP_DATA_CREATED | OP_DATA_MODIFIED => {
                if op.data_type == KEY_ID_TYPE {
                    self.set_cdkey(op.client, &op.data);
                }
            }
            OP_INVITATION | OP_JOIN_ATTEMPT | OP_PEER_CHAT => {
                let Some(c) = self.client(op.client) else {
                    return false;
                };
                if c.borrow().ignored {
                    return true;
                }
                self.handle_client(Some(c));
            }
            _ => {}
        }
        false
    }

    /// `HandleGetClientList`: gone clients dropped, new ones added, the others' names and flags updated.
    fn merge_clients(&mut self, new: &BTreeMap<u16, NewClient>) {
        let old: Vec<u16> = self.clients.keys().copied().collect();
        for id in old {
            if !new.contains_key(&id) {
                self.erase_client(id);
            }
        }
        for (&id, n) in new {
            match self.clients.get(&id) {
                Some(c) => {
                    let mut c = c.borrow_mut();
                    c.name = n.name.clone();
                    c.flags = n.flags;
                }
                None => {
                    self.clients.insert(id, Self::new_client(n));
                }
            }
        }
    }

    /// `HandleGetGroupList`: gone groups dropped (their members leaving), new ones added (their members linked
    /// when the list has them), the others' fields updated (and their members merged).
    fn merge_groups(&mut self, list: &[NewGroup], has_members: bool) {
        let mut new: BTreeMap<u16, &NewGroup> = BTreeMap::new();
        for g in list {
            new.entry(g.id).or_insert(g);
        }
        let old: Vec<u16> = self.groups.keys().copied().collect();
        let mut keys: Vec<u16> = old.iter().copied().chain(new.keys().copied()).collect();
        keys.sort_unstable();
        keys.dedup();
        for id in keys {
            let existing = self.groups.get(&id).cloned();
            match (existing, new.get(&id)) {
                (Some(g), None) => {
                    self.sync_members(&g, &[]);
                    self.erase_group(id);
                }
                (None, Some(n)) => {
                    let g = Rc::new(RefCell::new(Group {
                        id: n.id,
                        name: n.name.clone(),
                        captain: n.captain,
                        max_clients: n.max_clients,
                        flags: n.flags,
                        async_flags: n.async_flags,
                        observers: n.observers,
                        max_players: n.max_players,
                        members: BTreeMap::new(),
                    }));
                    let mut seen: Vec<u16> = Vec::new();
                    for m in &n.members {
                        if seen.contains(&m.id) {
                            continue;
                        }
                        seen.push(m.id);
                        let client = if has_members { Some(self.link_member(m)) } else { None };
                        g.borrow_mut()
                            .members
                            .insert(m.id, Rc::new(RefCell::new(Member { id: m.id, flags: m.flags, client })));
                    }
                    self.groups.insert(id, g);
                }
                (Some(g), Some(n)) => {
                    {
                        let mut o = g.borrow_mut();
                        o.name = n.name.clone();
                        o.id = n.id;
                        o.captain = n.captain;
                        o.max_clients = n.max_clients;
                        o.flags = n.flags;
                        o.async_flags = n.async_flags;
                    }
                    if has_members {
                        g.borrow_mut().observers = n.observers;
                        self.sync_members(&g, &n.members);
                    }
                }
                (None, None) => {}
            }
        }
    }

    // ---- the copies

    fn copy_client(c: &CRef) -> Copy {
        let c = c.borrow();
        Copy { id: c.id, name: c.name.clone(), flags: c.flags, cdkey: c.address, ..Default::default() }
    }

    fn copy_group(g: &GRef) -> Copy {
        let g = g.borrow();
        Copy {
            id: g.id,
            name: g.name.clone(),
            flags: g.flags,
            captain: g.captain,
            max_clients: g.max_clients,
            async_flags: g.async_flags,
            observers: g.observers,
            member_count: g.max_players,
            ..Default::default()
        }
    }

    fn copy_member(m: &MRef) -> Copy {
        let m = m.borrow();
        let mut out = Copy { id: m.id, member_flags: m.flags, ..Default::default() };
        if let Some(c) = &m.client {
            let c = c.borrow();
            out.name = c.name.clone();
            out.client_flags = c.flags;
            out.has_client = true;
        }
        out
    }

    pub fn get_client(&self, id: u16) -> Option<Copy> {
        self.client(id).map(|c| Self::copy_client(&c))
    }

    pub fn get_group(&self, id: u16) -> Option<Copy> {
        self.group(id).map(|g| Self::copy_group(&g))
    }

    pub fn get_member(&self, group: u16, client: u16) -> Option<Copy> {
        self.member_ref(client, i32::from(group)).map(|m| Self::copy_member(&m))
    }

    pub fn start_clients(&mut self) {
        self.client_itr = first_key(&self.clients);
    }

    pub fn next_client(&mut self) -> Option<Copy> {
        let k = self.client_itr?;
        let c = self.clients.get(&k).cloned()?;
        self.client_itr = next_key(&self.clients, k);
        Some(Self::copy_client(&c))
    }

    pub fn start_groups(&mut self) {
        self.group_itr = first_key(&self.groups);
    }

    pub fn next_group(&mut self) -> Option<Copy> {
        let k = self.group_itr?;
        let g = self.groups.get(&k).cloned()?;
        self.group_itr = next_key(&self.groups, k);
        Some(Self::copy_group(&g))
    }

    pub fn start_members(&mut self, group: u16) -> bool {
        self.member_itr_group = self.group(group);
        let Some(g) = self.member_itr_group.clone() else {
            return false;
        };
        let first = first_key(&g.borrow().members);
        self.member_itr = Some((g, first));
        true
    }

    pub fn next_member(&mut self) -> Option<Copy> {
        self.member_itr_group.as_ref()?;
        let (g, pos) = self.member_itr.as_mut()?;
        let k = (*pos)?;
        let m = g.borrow().members.get(&k).cloned()?;
        *pos = next_key(&g.borrow().members, k);
        Some(Self::copy_member(&m))
    }

    pub fn last_member(&self) -> Option<Copy> {
        self.last_member.as_ref().map(Self::copy_member)
    }

    pub fn last_client(&self) -> Option<Copy> {
        self.last_client.as_ref().map(Self::copy_client)
    }

    // ---- names and commands

    /// `FindClient(name)`: over the client map, exact names and unique prefixes ignoring case (the match and the
    /// last client it set).
    fn find_client(&self, name: &[u32]) -> (i32, Option<CRef>) {
        let mut result = MATCH_NOT_FOUND;
        let mut out = None;
        for c in self.clients.values() {
            let (exact, partial) = chat::compare_names(name, &c.borrow().name, &CrtChat);
            if exact {
                if result == MATCH_EXACT {
                    return (MATCH_AMBIGUOUS, out);
                }
                result = MATCH_EXACT;
                out = Some(c.clone());
            } else if partial {
                if result == MATCH_NOT_FOUND {
                    result = MATCH_PARTIAL;
                    out = Some(c.clone());
                } else if result == MATCH_PARTIAL {
                    result = MATCH_AMBIGUOUS;
                }
            }
        }
        (result, out)
    }

    pub fn find(&self, name: &[u32]) -> (i32, Option<Copy>) {
        let (r, c) = self.find_client(name);
        (r, c.map(|c| Self::copy_client(&c)))
    }

    /// `GetMemberByNameRef(group, name)`: as `find_client` over the group's members (members without a client
    /// skipped).
    fn member_by_name(&self, group: u16, name: &[u32]) -> (i32, Option<MRef>) {
        let Some(g) = self.group(group) else {
            return (MATCH_NOT_FOUND, None);
        };
        let mut result = MATCH_NOT_FOUND;
        let mut out = None;
        for m in g.borrow().members.values() {
            let Some(c) = m.borrow().client.clone() else {
                continue;
            };
            let (exact, partial) = chat::compare_names(name, &c.borrow().name, &CrtChat);
            if exact {
                if result == MATCH_EXACT {
                    return (MATCH_AMBIGUOUS, out);
                }
                result = MATCH_EXACT;
                out = Some(m.clone());
            } else if partial {
                if result == MATCH_NOT_FOUND {
                    result = MATCH_PARTIAL;
                    out = Some(m.clone());
                } else if result == MATCH_PARTIAL {
                    result = MATCH_AMBIGUOUS;
                }
            }
        }
        (result, out)
    }

    /// `IgnoreClient(client, on)` and the file saved.
    fn ignore_client(&mut self, c: &CRef, on: bool) {
        c.borrow_mut().ignored = on;
        let name = c.borrow().name.clone();
        if on {
            self.ignore_insert(&name);
        } else {
            self.ignore_erase(&name);
        }
        self.save_ignore();
    }

    /// `IgnoreClient` of the client with an id (`GetClient`'s record); none: nothing.
    pub fn ignore_id(&mut self, id: u16, on: bool) {
        if let Some(c) = self.client(id) {
            self.ignore_client(&c, on);
        }
    }

    pub fn add_command(&mut self, name: &[u32], command: i32) {
        self.commands.insert(upper(name), command);
    }

    pub fn clear_commands(&mut self) {
        self.commands.clear();
    }

    pub fn close(&mut self) {
        self.client_id = 0;
        self.my_client = None;
    }

    fn narrow(&self, t: &[u32]) -> Vec<u8> {
        let end = t.iter().position(|&u| u == 0).unwrap_or(t.len());
        if self.wchar_size == 2 {
            text::game_from_utf16(&t[..end].iter().map(|&u| u as u16).collect::<Vec<_>>())
        } else {
            text::game_from_utf32(&t[..end])
        }
    }

    /// `HandleInput(input, group)` (group -1: the default one): true when it made its request (`action`).
    fn input(&mut self, input: &[u32], group: i32) -> (bool, Action) {
        self.last_error = ERR_NONE;
        self.last_member = None;
        self.last_client = None;
        self.last_name.clear();
        let none = Action::default();
        if self.my_client.is_none() {
            self.last_error = ERR_NOT_LOGGED_IN;
            return (false, none);
        }
        let group = if group == -1 { i32::from(self.default_group) } else { group };
        let first = input.first().copied().unwrap_or(0);
        if first == u32::from(b'/') {
            return self.command(input, group);
        }
        let ct = CrtChat;
        let chat_to = |text: &[u32], flags: u32| Action {
            kind: EE_WON_SMART_ACT_CHAT,
            group: group as u16,
            flags,
            text: text.to_vec(),
            ..Default::default()
        };
        if self.colon && first == u32::from(b':') {
            self.last_command = CMD_EMOTE;
            let chat = &input[1..];
            if chat.iter().all(|&c| ct.iswspace(c)) {
                self.last_error = ERR_NO_CHAT;
                return (false, none);
            }
            return (true, chat_to(chat, CHAT_EMOTE));
        }
        self.last_command = CMD_BROADCAST;
        if input.iter().all(|&c| ct.iswspace(c)) {
            self.last_error = ERR_NO_CHAT;
            return (false, none);
        }
        (true, chat_to(input, 0))
    }

    fn target_id(&self) -> u16 {
        self.last_client.as_ref().map_or(0, |c| c.borrow().id)
    }

    /// `HandleCommand(input, group)`.
    fn command(&mut self, input: &[u32], group: i32) -> (bool, Action) {
        let ct = CrtChat;
        let none = Action::default();
        let mut pos = 1;
        let word = chat::get_token(input, &mut pos, true, false, &ct);
        let Some(&command) = self.commands.get(&word) else {
            self.last_command = CMD_NONE;
            self.last_error = ERR_INVALID_COMMAND;
            return (false, none);
        };
        self.last_command = command;
        let need_name = match command {
            CMD_IGNORE | CMD_UNMUTE | CMD_SERVER_UNMUTE => {
                self.last_name = chat::rest_of_line(input, pos, &ct);
                true
            }
            CMD_WHISPER | CMD_MUTE | CMD_SERVER_MUTE | CMD_BAN | CMD_SERVER_BAN | CMD_INVITE | CMD_UNINVITE => {
                self.last_name = chat::get_token(input, &mut pos, false, true, &ct);
                true
            }
            _ => false,
        };
        let mut cdkey_ban = u32::MAX;
        if need_name {
            if self.last_name.is_empty() {
                self.last_error = ERR_CLIENT_NOT_FOUND;
                return (false, none);
            }
            if command == CMD_SERVER_BAN && self.last_name[0] == u32::from(b'%') {
                self.last_name.remove(0);
                cdkey_ban = ct.atoi(&self.narrow(&self.last_name)) as u32;
            } else {
                let result;
                if self.server_wide {
                    let (r, c) = self.find_client(&self.last_name);
                    result = r;
                    if c.is_some() {
                        self.last_client = c;
                    }
                } else {
                    let (r, m) = self.member_by_name(group as u16, &self.last_name);
                    result = r;
                    if m.is_some() {
                        self.last_member = m;
                    }
                    if let Some(m) = &self.last_member {
                        self.last_client = m.borrow().client.clone();
                    }
                }
                if result < self.tolerance {
                    self.last_error =
                        if result == MATCH_AMBIGUOUS { ERR_CLIENT_AMBIGUOUS } else { ERR_CLIENT_NOT_FOUND };
                    return (false, none);
                }
            }
        }
        while pos < input.len() && ct.iswspace(input[pos]) {
            pos += 1;
        }
        let group16 = group as u16;
        let mut act = Action { group: group16, ..Default::default() };
        match command {
            CMD_WHISPER | CMD_EMOTE => {
                if pos >= input.len() {
                    self.last_error = ERR_NO_CHAT;
                    return (false, none);
                }
                act.kind = EE_WON_SMART_ACT_CHAT;
                if command == CMD_WHISPER {
                    act.text = input[pos..].to_vec();
                    act.flags = CHAT_WHISPER;
                    act.group = self.target_id();
                } else {
                    act.text = input[pos - 1..].to_vec();
                    act.flags = CHAT_EMOTE;
                }
                (true, act)
            }
            CMD_IGNORE => {
                if let Some(c) = self.last_client.clone() {
                    let on = !c.borrow().ignored;
                    self.ignore_client(&c, on);
                }
                (true, none)
            }
            CMD_AWAY => {
                let away = self.my_client.as_ref().is_some_and(|c| c.borrow().flags & CLIENT_AWAY != 0);
                act.kind = EE_WON_SMART_ACT_CLIENT_FLAGS;
                act.flags = CLIENT_AWAY;
                act.value = if away { 0 } else { CLIENT_AWAY };
                (true, act)
            }
            CMD_INVITE | CMD_UNINVITE => {
                act.kind = EE_WON_SMART_ACT_INVITE;
                act.client = self.target_id();
                act.on = command == CMD_INVITE;
                act.text = input.get(pos..).unwrap_or(&[]).to_vec();
                (true, act)
            }
            CMD_MODERATOR => {
                let moderator = self.my_client.as_ref().is_some_and(|c| c.borrow().flags & CLIENT_MODERATOR != 0);
                act.kind = EE_WON_SMART_ACT_MODERATOR;
                act.on = !moderator;
                (true, act)
            }
            CMD_MUTE | CMD_BAN | CMD_SERVER_MUTE | CMD_SERVER_BAN => {
                let allow_perm = command == CMD_SERVER_MUTE || command == CMD_SERVER_BAN;
                let mut seconds: u32 = 3600;
                let mut time = self.narrow(&chat::get_token(input, &mut pos, true, false, &ct));
                if time == b"INFINITE" && allow_perm {
                    seconds = 0;
                } else if let Some(&unit) = time.last() {
                    if unit < 0x80 && ct.isalpha(i32::from(unit)) {
                        time.pop();
                    }
                    seconds = ct.atoi(&time) as u32;
                    match unit {
                        b'D' => seconds = seconds.wrapping_mul(24 * 60 * 60),
                        b'H' => seconds = seconds.wrapping_mul(60 * 60),
                        b'M' => seconds = seconds.wrapping_mul(60),
                        b'S' => {}
                        _ => {
                            self.last_error = ERR_BAD_TIME;
                            return (false, none);
                        }
                    }
                    if seconds == 0 {
                        self.last_error = ERR_BAD_TIME;
                        return (false, none);
                    }
                }
                act.text = chat::get_token(input, &mut pos, false, true, &ct);
                act.seconds = seconds;
                act.on = true;
                act.client = self.target_id();
                match command {
                    CMD_MUTE => act.kind = EE_WON_SMART_ACT_MUTE,
                    CMD_SERVER_MUTE => {
                        act.kind = EE_WON_SMART_ACT_MUTE;
                        act.group = ROUTING_GLOBAL;
                    }
                    CMD_BAN => act.kind = EE_WON_SMART_ACT_BAN,
                    _ => {
                        act.group = ROUTING_GLOBAL;
                        if cdkey_ban == u32::MAX {
                            act.kind = EE_WON_SMART_ACT_BAN;
                        } else {
                            act.kind = EE_WON_SMART_ACT_BAN_CDKEY;
                            act.value = cdkey_ban;
                        }
                    }
                }
                (true, act)
            }
            CMD_UNMUTE | CMD_SERVER_UNMUTE => {
                act.kind = EE_WON_SMART_ACT_MUTE;
                if command == CMD_SERVER_UNMUTE {
                    act.group = ROUTING_GLOBAL;
                }
                act.client = self.target_id();
                (true, act)
            }
            _ => (true, none),
        }
    }

    // ---- the golden's dump (won_smartroute_test prints the original's members the same way)

    fn esc(t: &[u32]) -> String {
        let mut s = String::from("\"");
        for &c in t {
            if (0x20..0x7f).contains(&c) && c != u32::from(b'"') && c != u32::from(b'\\') {
                s.push(char::from(c as u8));
            } else {
                s.push_str(&format!("\\u{c:x}"));
            }
        }
        s.push('"');
        s
    }

    fn client_ref(&self, c: &Option<CRef>) -> String {
        match c {
            None => "-".into(),
            Some(c) => {
                let id = c.borrow().id;
                let shared = self.clients.get(&id).is_some_and(|m| Rc::ptr_eq(m, c));
                format!("{id}{}", if shared { "s" } else { "o" })
            }
        }
    }

    fn group_ref(&self, g: &Option<GRef>) -> String {
        match g {
            None => "-".into(),
            Some(g) => {
                let id = g.borrow().id;
                let shared = self.groups.get(&id).is_some_and(|m| Rc::ptr_eq(m, g));
                format!("{id}{}", if shared { "s" } else { "o" })
            }
        }
    }

    fn member_ref_text(m: &Option<MRef>) -> String {
        m.as_ref().map_or("-".into(), |m| m.borrow().id.to_string())
    }

    /// The model's lines (`async`: with the async flags).
    pub fn dump(&self, with_async: bool) -> Vec<String> {
        let mut out = Vec::new();
        out.push(format!(
            "model me={} id={}{} default={} tol={} wide={} colon={}",
            self.client_ref(&self.my_client),
            self.client_id,
            if with_async { format!(" async={:x}", self.async_flags) } else { String::new() },
            self.default_group,
            self.tolerance,
            u8::from(self.server_wide),
            u8::from(self.colon)
        ));
        for (k, c) in &self.clients {
            let c = c.borrow();
            out.push(format!(
                "client {k} id={} {} f={:x} cd={} g={} ign={}",
                c.id,
                Self::esc(&c.name),
                c.flags,
                c.address,
                c.num_groups,
                u8::from(c.ignored)
            ));
        }
        for (k, g) in &self.groups {
            let g = g.borrow();
            out.push(format!(
                "group {k} id={} {} cap={} maxc={} f={:x} af={:x} obs={} mp={}",
                g.id,
                Self::esc(&g.name),
                g.captain,
                g.max_clients,
                g.flags,
                g.async_flags,
                g.observers,
                g.max_players
            ));
            for (mk, m) in &g.members {
                let m = m.borrow();
                out.push(format!(" member {mk} id={} f={} c={}", m.id, m.flags, self.client_ref(&m.client)));
            }
        }
        out.push(format!(
            "handler c={} g={} m={}",
            self.client_ref(&self.h_client),
            self.group_ref(&self.h_group),
            Self::member_ref_text(&self.h_member)
        ));
        let mut ignore = String::from("ignore");
        for n in &self.ignore {
            ignore.push(' ');
            ignore.push_str(&Self::esc(n));
        }
        out.push(ignore);
        out
    }

    pub fn dump_commands(&self) -> String {
        let mut s = String::from("commands");
        for (k, v) in &self.commands {
            s.push_str(&format!(" {}={v}", Self::esc(k)));
        }
        s
    }

    pub fn dump_input(&self, with_command: bool) -> String {
        format!(
            "input err={}{} target m={} c={} name={}",
            self.last_error,
            if with_command { format!(" cmd={}", self.last_command) } else { String::new() },
            Self::member_ref_text(&self.last_member),
            self.client_ref(&self.last_client),
            Self::esc(&self.last_name)
        )
    }
}

// ---- the C ABI (WONRouting/SmartRoutingConnection.cpp)

/// The model (opaque): `SmartRoutingConnection::mModel`.
pub struct EeWonSmartRoute(Model);

/// A number of the model (`ee_won_smart_number` / `ee_won_smart_set`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonSmartNumber {
    /// `mClientId` (this client's id)
    ClientId = 0,
    /// `mMatchTolerance` set by `SetAllowPartialNameMatch` (1: partial names, 0: exact names only)
    PartialNames = 1,
    ServerWide = 2,
    ColonEmote = 3,
    LastError = 4,
    LastCommand = 5,
}

/// A record's copy for `ClientInfo` / `GroupInfo` / `MemberInfo` (`name` points into the object until its next
/// call).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonSmartRecord {
    pub id: u16,
    pub captain: u16,
    pub max_clients: u16,
    pub observers: u16,
    pub member_count: u16,
    pub flags: u32,
    pub async_flags: u32,
    pub cdkey: u32,
    pub client_flags: u32,
    pub member_flags: u8,
    pub has_client: bool,
    pub name: *const u32,
    pub name_len: usize,
}

/// Which copy `ee_won_smart_get` makes.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonSmartGet {
    /// `GetClient(a)`
    Client = 0,
    /// `GetGroup(a)`
    Group = 1,
    /// `GetMember(a group, b client)`
    Member = 2,
    NextClient = 3,
    NextGroup = 4,
    NextMember = 5,
    LastMember = 6,
    LastClient = 7,
}

/// A finished op's fields: `kind` the op type, then by type (SmartRoutingConnection.cpp's switch).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonSmartOp {
    pub kind: i32,
    pub status: i32,
    pub group: u16,
    pub client: u16,
    pub a: u32,
    pub b: u32,
    pub c: u32,
    pub has: bool,
    /// `text_len` `wchar_t`s
    pub text: *const c_void,
    pub text_len: usize,
    pub data_type: *const u8,
    pub data_type_len: usize,
    pub data: *const u8,
    pub data_len: usize,
}

unsafe fn model<'a>(r: *mut EeWonSmartRoute) -> &'a mut Model {
    // SAFETY: a model from ee_won_smart_new, under the connection's data lock (contract)
    unsafe { &mut (*r).0 }
}

unsafe fn wide(m: &Model, text: *const c_void, len: usize) -> Vec<u32> {
    // SAFETY: `len` wchar_t units (contract)
    unsafe { chat::units(chat::EeWonChatText { text, length: len }, m.wchar_size) }
}

fn record(scratch: &mut Vec<u32>, c: Copy) -> EeWonSmartRecord {
    *scratch = c.name;
    EeWonSmartRecord {
        id: c.id,
        captain: c.captain,
        max_clients: c.max_clients,
        observers: c.observers,
        member_count: c.member_count,
        flags: c.flags,
        async_flags: c.async_flags,
        cdkey: c.cdkey,
        client_flags: c.client_flags,
        member_flags: c.member_flags,
        has_client: c.has_client,
        name: scratch.as_ptr(),
        name_len: scratch.len(),
    }
}

/// A new model: the ignore file's path (`path_len` bytes), the C++'s `wchar_t` size.
///
/// # Safety
/// `path` holds `path_len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_new(wchar_size: u32, path: *const u8, path_len: usize) -> *mut EeWonSmartRoute {
    let p = unsafe { crate::ffi::bytes(path, path_len) };
    Box::into_raw(Box::new(EeWonSmartRoute(Model::new(wchar_size, p))))
}

/// # Safety
/// `r` is from `ee_won_smart_new` (or NULL), not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_free(r: *mut EeWonSmartRoute) {
    if !r.is_null() {
        drop(unsafe { Box::from_raw(r) });
    }
}

/// A client record of the next op (its list, or its own client).
///
/// # Safety
/// `r` is a model; `name` holds `name_len` `wchar_t`s.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_put_client(
    r: *mut EeWonSmartRoute,
    id: u16,
    name: *const c_void,
    name_len: usize,
    flags: u32,
    address: u32,
) {
    let m = unsafe { model(r) };
    let name = unsafe { wide(m, name, name_len) };
    m.put_clients.push(NewClient { id, name, flags, address });
}

/// A group record of the next op (a group list's); its members follow with `to_group`.
///
/// # Safety
/// `r` is a model; `name` holds `name_len` `wchar_t`s.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_put_group(
    r: *mut EeWonSmartRoute,
    id: u16,
    name: *const c_void,
    name_len: usize,
    captain: u16,
    max_clients: u16,
    flags: u32,
    async_flags: u32,
    observers: u16,
    max_players: u16,
) {
    let m = unsafe { model(r) };
    let name = unsafe { wide(m, name, name_len) };
    m.put_groups.push(NewGroup {
        id,
        name,
        captain,
        max_clients,
        flags,
        async_flags,
        observers,
        max_players,
        members: Vec::new(),
    });
}

/// A member record of the next op: into the last group put (`to_group`), else the op's member list; its client
/// record's name and flags when it has one.
///
/// # Safety
/// `r` is a model; `name` holds `name_len` `wchar_t`s.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_put_member(
    r: *mut EeWonSmartRoute,
    to_group: bool,
    id: u16,
    flags: u8,
    has_client: bool,
    name: *const c_void,
    name_len: usize,
    client_flags: u32,
) {
    let m = unsafe { model(r) };
    let client = has_client.then(|| (unsafe { wide(m, name, name_len) }, client_flags));
    let member = NewMember { id, flags, client };
    if to_group {
        if let Some(g) = m.put_groups.last_mut() {
            g.members.push(member);
        }
    } else {
        m.put_members.push(member);
    }
}

/// A data object of the next op (its link id and data).
///
/// # Safety
/// `r` is a model; `data` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_put_object(r: *mut EeWonSmartRoute, link: u16, data: *const u8, len: usize) {
    let m = unsafe { model(r) };
    let d = unsafe { crate::ffi::bytes(data, len) }.to_vec();
    m.put_objects.push((link, d));
}

/// `RoutingOpCompleteHook`: the model updated from the op; true when its completion is to be dropped.
///
/// # Safety
/// `r` is a model; `op` points to the op's fields, whose pointers hold their lengths.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_op(r: *mut EeWonSmartRoute, op: *const EeWonSmartOp) -> bool {
    let m = unsafe { model(r) };
    let o = unsafe { &*op };
    let op = Op {
        kind: o.kind,
        status: o.status,
        group: o.group,
        client: o.client,
        a: o.a,
        b: o.b,
        c: o.c,
        has: o.has,
        text: unsafe { wide(m, o.text, o.text_len) },
        data_type: unsafe { crate::ffi::bytes(o.data_type, o.data_type_len) }.to_vec(),
        data: unsafe { crate::ffi::bytes(o.data, o.data_len) }.to_vec(),
    };
    m.op(&op)
}

/// A copy (`which`, with the ids `a` and `b`): false when there is none (`out` untouched).
///
/// # Safety
/// `r` is a model, `out` a record.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_get(
    r: *mut EeWonSmartRoute,
    which: EeWonSmartGet,
    a: u16,
    b: u16,
    out: *mut EeWonSmartRecord,
) -> bool {
    let m = unsafe { model(r) };
    let c = match which {
        EeWonSmartGet::Client => m.get_client(a),
        EeWonSmartGet::Group => m.get_group(a),
        EeWonSmartGet::Member => m.get_member(a, b),
        EeWonSmartGet::NextClient => m.next_client(),
        EeWonSmartGet::NextGroup => m.next_group(),
        EeWonSmartGet::NextMember => m.next_member(),
        EeWonSmartGet::LastMember => m.last_member(),
        EeWonSmartGet::LastClient => m.last_client(),
    };
    match c {
        Some(c) => {
            let rec = record(&mut m.scratch, c);
            unsafe { *out = rec };
            true
        }
        None => false,
    }
}

/// `StartClientItr` (0), `StartGroupItr` (1) or `StartMemberItr(group)` (2: false when there is no such group).
///
/// # Safety
/// `r` is a model.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_start(r: *mut EeWonSmartRoute, which: i32, group: u16) -> bool {
    let m = unsafe { model(r) };
    match which {
        0 => {
            m.start_clients();
            true
        }
        1 => {
            m.start_groups();
            true
        }
        _ => m.start_members(group),
    }
}

/// `FindClient(name)`: the match (`MatchResult`), and the client it set in `out` (`found`).
///
/// # Safety
/// `r` is a model, `name` holds `len` `wchar_t`s, `out` and `found` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_find(
    r: *mut EeWonSmartRoute,
    name: *const c_void,
    len: usize,
    out: *mut EeWonSmartRecord,
    found: *mut bool,
) -> i32 {
    let m = unsafe { model(r) };
    let name = unsafe { wide(m, name, len) };
    let (result, c) = m.find(&name);
    unsafe { *found = c.is_some() };
    if let Some(c) = c {
        let rec = record(&mut m.scratch, c);
        unsafe { *out = rec };
    }
    result
}

/// `IgnoreClient` of the client with an id (the ignore file saved).
///
/// # Safety
/// `r` is a model.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_ignore(r: *mut EeWonSmartRoute, id: u16, on: bool) {
    unsafe { model(r) }.ignore_id(id, on);
}

/// `AddCommandString(text, command)`.
///
/// # Safety
/// `r` is a model, `name` holds `len` `wchar_t`s.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_add_command(
    r: *mut EeWonSmartRoute,
    name: *const c_void,
    len: usize,
    command: i32,
) {
    let m = unsafe { model(r) };
    let name = unsafe { wide(m, name, len) };
    m.add_command(&name, command);
}

/// `ClearCommandStrings`.
///
/// # Safety
/// `r` is a model.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_clear_commands(r: *mut EeWonSmartRoute) {
    unsafe { model(r) }.clear_commands();
}

/// `SetProductName`: the ignore file's path.
///
/// # Safety
/// `r` is a model, `path` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_set_path(r: *mut EeWonSmartRoute, path: *const u8, len: usize) {
    let m = unsafe { model(r) };
    m.ignore_path = unsafe { crate::ffi::bytes(path, len) }.to_vec();
}

/// The `CloseHook`'s part: this client's id and record forgotten.
///
/// # Safety
/// `r` is a model.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_close(r: *mut EeWonSmartRoute) {
    unsafe { model(r) }.close();
}

/// A number (see [`EeWonSmartNumber`]).
///
/// # Safety
/// `r` is a model.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_number(r: *mut EeWonSmartRoute, which: EeWonSmartNumber) -> i64 {
    let m = unsafe { model(r) };
    match which {
        EeWonSmartNumber::ClientId => i64::from(m.client_id),
        EeWonSmartNumber::PartialNames => i64::from(m.tolerance == MATCH_PARTIAL),
        EeWonSmartNumber::ServerWide => i64::from(m.server_wide),
        EeWonSmartNumber::ColonEmote => i64::from(m.colon),
        EeWonSmartNumber::LastError => i64::from(m.last_error),
        EeWonSmartNumber::LastCommand => i64::from(m.last_command),
    }
}

/// Sets a setting (`PartialNames`, `ServerWide`, `ColonEmote`; the others are read only).
///
/// # Safety
/// `r` is a model.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_set(r: *mut EeWonSmartRoute, which: EeWonSmartNumber, value: i64) {
    let m = unsafe { model(r) };
    match which {
        EeWonSmartNumber::PartialNames => m.tolerance = if value != 0 { MATCH_PARTIAL } else { MATCH_EXACT },
        EeWonSmartNumber::ServerWide => m.server_wide = value != 0,
        EeWonSmartNumber::ColonEmote => m.colon = value != 0,
        _ => {}
    }
}

/// `HandleInput(text, group)`: true when the input made its request, which `action` describes for the C++ to send.
///
/// # Safety
/// `r` is a model, `text` holds `len` `wchar_t`s, `action` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_input(
    r: *mut EeWonSmartRoute,
    text: *const c_void,
    len: usize,
    group: i32,
    action: *mut EeWonSmartAction,
) -> bool {
    let m = unsafe { model(r) };
    let input = unsafe { wide(m, text, len) };
    let (ok, act) = m.input(&input, group);
    m.scratch2 = act.text;
    let out = EeWonSmartAction {
        kind: act.kind,
        group: act.group,
        client: act.client,
        flags: act.flags,
        value: act.value,
        seconds: act.seconds,
        on: act.on,
        text: m.scratch2.as_ptr(),
        text_len: m.scratch2.len(),
    };
    unsafe { *action = out };
    ok
}

/// The golden's dump (won_smartroute_test): `what` 0 the model's lines (`flag`: with the async flags), 1 the
/// commands, 2 the input state (`flag`: with the command); the lines joined with '\n' into the sink.
///
/// # Safety
/// `r` is a model, `sink` a byte sink.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_dump(
    r: *mut EeWonSmartRoute,
    what: i32,
    flag: bool,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let m = unsafe { model(r) };
    let text = match what {
        0 => m.dump(flag).join("\n"),
        1 => m.dump_commands(),
        _ => m.dump_input(flag),
    };
    unsafe { sink(context, text.as_ptr(), text.len()) };
}

#[cfg(test)]
mod tests {
    use super::*;

    fn w(s: &str) -> Vec<u32> {
        s.chars().map(u32::from).collect()
    }

    fn client(id: u16, name: &str, flags: u32) -> NewClient {
        NewClient { id, name: w(name), flags, address: 0 }
    }

    fn registered() -> Model {
        let mut m = Model::new(2, b"");
        m.put_clients = vec![client(10, "me", 1), client(11, "Alice", 0), client(12, "bob", 4), client(13, "Bobby", 0)];
        let op = Op { kind: OP_REGISTER, client: 10, b: 2, ..Default::default() };
        assert!(!m.op(&op));
        m
    }

    fn group_list(m: &mut Model, groups: Vec<NewGroup>, has: bool) {
        m.put_groups = groups;
        m.op(&Op { kind: OP_GET_GROUP_LIST, has, ..Default::default() });
    }

    fn member(id: u16, flags: u8) -> NewMember {
        NewMember { id, flags, client: None }
    }

    #[test]
    fn register_and_groups() {
        let mut m = registered();
        assert_eq!(m.dump(true)[0], "model me=10s id=10 async=2 default=0 tol=2 wide=0 colon=1");
        group_list(
            &mut m,
            vec![NewGroup {
                id: 100,
                name: w("Lobby"),
                members: vec![member(10, 1), member(14, 2)],
                ..Default::default()
            }],
            true,
        );
        assert_eq!(m.clients[&14].borrow().num_groups, 1);
        assert_eq!(m.clients[&10].borrow().num_groups, 1);
        // a list without the group: its members leave, client 14 (in no group) is dropped
        group_list(&mut m, vec![], true);
        assert!(m.groups.is_empty());
        assert!(!m.clients.contains_key(&14));
        assert!(m.clients.contains_key(&10));
    }

    #[test]
    fn iterators_step_past_erased_records() {
        let mut m = registered();
        m.start_clients();
        assert_eq!(m.next_client().unwrap().id, 10);
        m.op(&Op { kind: OP_CLIENT_LEFT_SERVER, client: 11, ..Default::default() });
        assert_eq!(m.next_client().unwrap().id, 12);
        group_list(
            &mut m,
            vec![NewGroup {
                id: 100,
                members: vec![member(10, 0), member(12, 0), member(13, 0)],
                ..Default::default()
            }],
            true,
        );
        assert!(m.start_members(100));
        assert_eq!(m.next_member().unwrap().id, 10);
        m.op(&Op { kind: OP_CLIENT_LEFT_GROUP, group: 100, client: 12, ..Default::default() });
        // the handled client was dropped (no group, async flag 2): an orphan
        assert_eq!(m.dump(true).iter().find(|l| l.starts_with("handler")).unwrap(), "handler c=12o g=100s m=12");
        assert_eq!(m.next_member().unwrap().id, 13);
        assert!(m.next_member().is_none());
        // the member iterator's group deleted: the iteration ends
        assert!(m.start_members(100));
        m.op(&Op { kind: OP_GROUP_DELETED, group: 100, ..Default::default() });
        assert!(m.next_member().is_none());
    }

    #[test]
    fn ignored_senders_are_swallowed() {
        let mut m = registered();
        let mut m2 = Model::new(2, b"");
        m2.ignore_insert(&w("BOB"));
        m.ignore = m2.ignore.clone();
        m.sync_ignore();
        assert!(m.op(&Op { kind: OP_PEER_CHAT, client: 12, ..Default::default() }));
        assert!(!m.op(&Op { kind: OP_PEER_CHAT, client: 11, ..Default::default() }));
        assert_eq!(m.h_client.as_ref().unwrap().borrow().id, 11);
        assert!(!m.op(&Op { kind: OP_PEER_CHAT, client: 99, ..Default::default() }));
    }

    #[test]
    fn ignore_file_bytes() {
        let mut m = Model::new(4, b"");
        m.ignore_insert(&w("bob"));
        m.ignore_insert(&w("Al"));
        m.ignore_insert(&[0x1f600]);
        m.ignore_insert(&w("BOB"));
        assert_eq!(m.ignore.len(), 3);
        let b = m.ignore_file_bytes();
        assert_eq!(&b[..6], &[b'A', 0, b'l', 0, 0, 0]);
        assert_eq!(&b[6..14], &[b'b', 0, b'o', 0, b'b', 0, 0, 0]);
        assert_eq!(&b[14..], &[0x3d, 0xd8, 0x00, 0xde, 0, 0]);
    }

    #[test]
    fn chat_input_actions() {
        let mut m = registered();
        group_list(
            &mut m,
            vec![NewGroup {
                id: 100,
                members: vec![member(10, 0), member(12, 0), member(13, 0)],
                ..Default::default()
            }],
            true,
        );
        m.op(&Op { kind: OP_JOIN_GROUP, group: 100, ..Default::default() });
        let (ok, a) = m.input(&w("/w Bobby hi there"), -1);
        assert!(ok);
        assert_eq!((a.kind, a.group, a.flags, a.text), (EE_WON_SMART_ACT_CHAT, 13, CHAT_WHISPER, w("hi there")));
        let (ok, a) = m.input(&w("/me waves"), -1);
        assert!(ok);
        assert_eq!((a.group, a.flags, a.text), (100, CHAT_EMOTE, w(" waves")));
        let (ok, a) = m.input(&w("/mute Bobby 2h"), -1);
        assert!(ok);
        assert_eq!((a.kind, a.seconds, a.client), (EE_WON_SMART_ACT_MUTE, 7200, 13));
        let (ok, _) = m.input(&w("/mute Bobby 5x"), -1);
        assert!(!ok);
        assert_eq!(m.last_error, ERR_BAD_TIME);
        let (ok, a) = m.input(&w("/sban %12345 1d"), -1);
        assert!(ok);
        assert_eq!((a.kind, a.group, a.value, a.seconds), (EE_WON_SMART_ACT_BAN_CDKEY, 0xffff, 12345, 86400));
        let (ok, a) = m.input(&w("/smute Bobby INFINITE"), -1);
        assert!(ok);
        assert_eq!(a.seconds, 0);
        let (ok, _) = m.input(&w("/w B x"), -1);
        assert!(!ok);
        assert_eq!(m.last_error, ERR_CLIENT_AMBIGUOUS);
        let (ok, a) = m.input(&w("/away"), -1);
        assert!(ok);
        assert_eq!((a.kind, a.flags, a.value), (EE_WON_SMART_ACT_CLIENT_FLAGS, CLIENT_AWAY, CLIENT_AWAY));
        let (ok, a) = m.input(&w("/i bob"), -1);
        assert!(ok && a.kind == EE_WON_SMART_ACT_NONE);
        assert!(m.clients[&12].borrow().ignored);
        let (ok, _) = m.input(&w("   "), -1);
        assert!(!ok);
        assert_eq!((m.last_error, m.last_command), (ERR_NO_CHAT, CMD_BROADCAST));
        m.close();
        let (ok, _) = m.input(&w("hi"), -1);
        assert!(!ok);
        assert_eq!(m.last_error, ERR_NOT_LOGGED_IN);
    }

    #[test]
    fn cd_keys_from_data_objects() {
        let mut m = registered();
        let mut d = 1u32.to_le_bytes().to_vec();
        d.extend(555u32.to_le_bytes());
        m.put_objects = vec![(11, d.clone()), (12, vec![1, 0, 0, 0]), (99, d.clone())];
        m.op(&Op { kind: OP_READ_DATA, data_type: KEY_ID_TYPE.to_vec(), ..Default::default() });
        assert_eq!(m.clients[&11].borrow().address, 555);
        assert_eq!(m.clients[&12].borrow().address, 0);
        m.op(&Op { kind: OP_DATA_CREATED, client: 13, data_type: b"Room".to_vec(), data: d, ..Default::default() });
        assert_eq!(m.clients[&13].borrow().address, 0);
    }

    #[test]
    fn client_list_merge_keeps_records() {
        let mut m = registered();
        let alice = m.clients[&11].clone();
        m.put_clients = vec![client(5, "Five", 3), client(11, "Alice2", 7)];
        m.op(&Op { kind: OP_GET_CLIENT_LIST, ..Default::default() });
        assert_eq!(m.clients.keys().copied().collect::<Vec<_>>(), vec![5, 11]);
        assert!(Rc::ptr_eq(&m.clients[&11], &alice));
        assert_eq!(alice.borrow().name, w("Alice2"));
        assert!(m.my_client.is_none());
    }
}
