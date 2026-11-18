//! The lobby's saved settings as one Rust object (R6 WONLobby round 15): LobbyPersistentData whole, with the
//! `Lobby*` functions that read and change it (LobbyPersistentData.cpp, LobbyPersistentAccessors.cpp,
//! LobbyPersistentSetters.cpp, LobbyPersistentQueries.cpp, LobbyPersistentFilters.cpp, LobbyPersistentLoad.cpp,
//! LobbyRestoreDefaults.cpp and LobbyOptionSet, as they were at the round's start).
//!
//! The object holds what the class held: the scalar state (prefsobj.rs), the saved users, the user ids, the ignored
//! names, the blocked CD key ids, the friends, the default and current game filters, the reconnect ids, the recent
//! LAN names, the direct-connect addresses and the password key. Each container is ordered by the original's
//! comparator (lobbymap.rs's keys): a name ignoring case (`GUIStringLessNoCase`), an id, an address's six bytes; a
//! filter's values are unique by `Compare` in insertion order. Texts are the `GUIString`'s `wchar_t` units (UTF-16
//! on Windows, code points elsewhere: the object's `wchar_size`); the files' texts are UTF-16 (persist.rs).
//!
//! What stays C++ is reached through the host ([`EeWonPersistOp`]): a friend is a `LobbyFriend` shell (made,
//! referenced and named by the host; the object holds one reference per friend it keeps), the resource strings, the
//! click sound's mute, the lobby config's LAN-number switch and the clock. The files are read and written here
//! (file.rs, wonfile.rs: the same calls the C++ made). A NULL object is the original's "no lobby data": every call
//! answers what the function answered without it.

use std::cmp::Ordering;
use std::ffi::{c_long, c_void, CString};

use ee_won_gui::guistring;
use ee_won_net::blowfish::Blowfish;
use ee_won_net::file::{EeWonFileReader, EeWonFileWriter};
use ee_won_net::ipobj::{self, EeWonIpAddr};
use ee_won_net::text;
use ee_won_net::wire::{Reader, Writer};
use ee_won_net::wonfile::EeWonFile;

use crate::lobbymap::{compare, EeWonLobbyKeyKind as K, Key};
use crate::persist::{self, Event};
use crate::prefsobj::{EeWonLobbyPrefs, EeWonPrefsFlag as F, EeWonPrefsNumber, EeWonPrefsText};
use crate::resource::EeWonLobbyRes;

/// What the object asks of the C++ (`EeWonPersistHost::op`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonPersistOp {
    /// `new LobbyFriend(name, c)` with one reference taken: the shell. The name is `b` units at `a` (`uint32_t`s,
    /// one `wchar_t` each).
    FriendNew = 0,
    /// `((LobbyFriend*)a)->AddRef()` when `b` is 1, `->Release()` when 0.
    FriendRef,
    /// `((LobbyFriend*)a)->GetName()`'s units (the friend's own), their count in `*(size_t*)b`.
    FriendName,
    /// `gLobbyResources[a].mString`'s units (the resource's own), their count in `*(size_t*)b`.
    ResText,
    /// `LOBBY_SOUND(LobbyGlobal_ButtonClick_Sound)->SetMuted(a != 0)`.
    MuteClick,
    /// `GetLobbyConfig() == NULL || GetLobbyConfig()->mAppendNumberToDefaultLanName`: 0 or 1.
    AppendLanNumber,
    /// `time(NULL)` as a `long`.
    Now,
}

/// The object's host: `op(ctx, op, a, b, c)`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonPersistHost {
    pub ctx: *mut c_void,
    pub op: Option<unsafe extern "C" fn(*mut c_void, EeWonPersistOp, isize, isize, isize) -> isize>,
}

/// A text handed over: `count` units of `unit_size` bytes at `text` (NULL with a count of 0: none).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonPersistText {
    pub text: *const c_void,
    pub count: usize,
    pub unit_size: u32,
}

/// A saved login user (`LobbyUserInfo`): the record the user functions hand out (it lives while it is saved).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeWonPersistUser {
    user_id: i64,
    value04: i64,
    value08: i64,
    last_use: i64,
    name: Vec<u32>,
    password: Vec<u32>,
}

/// An ordered container under one of lobbymap.rs's comparators, its values boxed (their addresses stay while the
/// entry does, as a `std::map`'s).
#[derive(Debug)]
struct Ordered<V> {
    kind: K,
    entries: Vec<(Key, Box<V>)>,
}

impl<V: Default> Ordered<V> {
    fn new(kind: K) -> Self {
        Ordered { kind, entries: Vec::new() }
    }
    fn len(&self) -> usize {
        self.entries.len()
    }
    fn search(&self, key: &Key) -> Result<usize, usize> {
        if self.kind == K::InOrder {
            return match self.entries.iter().position(|(k, _)| compare(self.kind, k, key) == Ordering::Equal) {
                Some(i) => Ok(i),
                None => Err(self.entries.len()),
            };
        }
        self.entries.binary_search_by(|(k, _)| compare(self.kind, k, key))
    }
    fn find(&self, key: &Key) -> Option<usize> {
        self.search(key).ok()
    }
    /// `insert(value_type(key, V()))` / `operator[]`: the position and whether it is new (a key already present
    /// keeps its first spelling).
    fn insert(&mut self, key: Key) -> (usize, bool) {
        match self.search(&key) {
            Ok(i) => (i, false),
            Err(i) => {
                self.entries.insert(i, (key, Box::default()));
                (i, true)
            }
        }
    }
    fn at(&self, i: usize) -> &V {
        &self.entries[i].1
    }
    fn at_mut(&mut self, i: usize) -> &mut V {
        &mut self.entries[i].1
    }
    fn get(&self, key: &Key) -> Option<&V> {
        self.find(key).map(|i| self.at(i))
    }
    fn erase_at(&mut self, i: usize) -> Box<V> {
        self.entries.remove(i).1
    }
    fn erase(&mut self, key: &Key) -> Option<Box<V>> {
        self.find(key).map(|i| self.erase_at(i))
    }
    fn clear(&mut self) -> Vec<Box<V>> {
        self.entries.drain(..).map(|(_, v)| v).collect()
    }
    fn contains(&self, key: &Key) -> bool {
        self.find(key).is_some()
    }
    fn units_at(&self, i: usize) -> &[u32] {
        match &self.entries[i].0 {
            Key::Units(u) => u,
            _ => &[],
        }
    }
    fn num_at(&self, i: usize) -> u64 {
        match &self.entries[i].0 {
            Key::Num(n) => *n,
            _ => 0,
        }
    }
    fn addr_at(&self, i: usize) -> &EeWonIpAddr {
        match &self.entries[i].0 {
            Key::Addr(a) => a,
            _ => panic!("persist: not an address key"),
        }
    }
}

impl<V: Default + Clone> Clone for Ordered<V> {
    fn clone(&self) -> Self {
        Ordered { kind: self.kind, entries: self.entries.clone() }
    }
}

/// LobbyOptionSet: values unique by `Compare`, in insertion order.
type OptionSet = Ordered<()>;
impl Default for OptionSet {
    fn default() -> Self {
        Ordered::new(K::InOrder)
    }
}

/// The lobby's saved settings (LobbyPersistentData).
pub struct EeWonPersist {
    host: EeWonPersistHost,
    /// `sizeof(wchar_t)` of the C++ (2 or 4).
    ws: u32,
    prefs: EeWonLobbyPrefs,
    user_ids: Ordered<()>,
    ignored: Ordered<()>,
    blocked: Ordered<()>,
    /// the friends' shells (one reference each)
    friends: Ordered<isize>,
    default_options: Ordered<OptionSet>,
    options: Ordered<OptionSet>,
    users: Ordered<EeWonPersistUser>,
    /// the last use time and the reconnect id
    reconnect: Ordered<[i64; 2]>,
    /// the last use time
    lan_names: Ordered<i64>,
    /// the name
    direct: Ordered<Vec<u32>>,
    key: Blowfish,
    /// what the last text call gave (valid to the next call)
    out_text: Vec<u32>,
    /// what the last address call gave
    out_addr: EeWonIpAddr,
}

const GLOBAL_FILE: &str = "_wonlobbypersistent.dat";

fn num_key(n: i64) -> Key {
    Key::Num(n as u64)
}
fn addr_key(a: &EeWonIpAddr) -> Key {
    Key::Addr(a.clone())
}
fn six_addr(six: &[u8; 6]) -> EeWonIpAddr {
    let mut a = EeWonIpAddr::default();
    a.set_six(six);
    a
}

impl EeWonPersist {
    /// The constructor: the containers, the password key, the defaults (RestoreDefaults, port 8585, both dirty
    /// flags clear, the saved sound and music on).
    pub fn new(host: EeWonPersistHost, ws: u32, key: &[u8]) -> Box<EeWonPersist> {
        let mut bf = Blowfish::new();
        bf.set_key(key);
        let mut p = Box::new(EeWonPersist {
            host,
            ws,
            prefs: EeWonLobbyPrefs::new(),
            user_ids: Ordered::new(K::Signed),
            ignored: Ordered::new(K::NoCase),
            blocked: Ordered::new(K::Unsigned),
            friends: Ordered::new(K::Unsigned),
            default_options: Ordered::new(K::NoCase),
            options: Ordered::new(K::NoCase),
            users: Ordered::new(K::NoCase),
            reconnect: Ordered::new(K::Addr),
            lan_names: Ordered::new(K::NoCase),
            direct: Ordered::new(K::Addr),
            key: bf,
            out_text: Vec::new(),
            out_addr: EeWonIpAddr::default(),
        });
        // the original did not set mCurrentUserId before this reset (0: the LAN user)
        p.restore_defaults();
        p.prefs.set_number(EeWonPrefsNumber::LanBroadcastPort, 8585);
        p.prefs.set_flag(F::GlobalDirty, false);
        p.prefs.set_flag(F::UserDirty, false);
        p.prefs.set_flag(F::SavedLobbySoundEffects, true);
        p.prefs.set_flag(F::SavedLobbyMusic, true);
        p
    }

    fn op(&self, op: EeWonPersistOp, a: isize, b: isize, c: isize) -> isize {
        match self.host.op {
            // SAFETY: the host's contract
            Some(f) => unsafe { f(self.host.ctx, op, a, b, c) },
            None => 0,
        }
    }
    /// Units a host op gave (its count through `b`).
    fn op_units(&self, op: EeWonPersistOp, a: isize) -> Vec<u32> {
        let mut n = 0usize;
        let p = self.op(op, a, &mut n as *mut usize as isize, 0);
        // SAFETY: the host gives `n` units of the object's wchar_t size at `p` (or NULL)
        unsafe { units(p as *const c_void, n, self.ws) }
    }
    fn res(&self, r: EeWonLobbyRes) -> Vec<u32> {
        self.op_units(EeWonPersistOp::ResText, r as isize)
    }
    fn now(&self) -> i64 {
        self.op(EeWonPersistOp::Now, 0, 0, 0) as c_long as i64
    }
    fn friend_release(&self, shell: isize) {
        if shell != 0 {
            self.op(EeWonPersistOp::FriendRef, shell, 0, 0);
        }
    }
    fn mute_click(&self) {
        self.op(EeWonPersistOp::MuteClick, isize::from(!self.prefs.flag(F::LobbySoundEffects)), 0, 0);
    }

    // ---- the texts' conversions, as the C++ made them

    /// `(std::string)GUIString`: UTF-8.
    fn narrow(&self, u: &[u32]) -> Vec<u8> {
        guistring::narrow(u, self.ws)
    }
    /// `GUIString(std::string)`.
    fn widen(&self, b: &[u8]) -> Vec<u32> {
        guistring::widen(b, self.ws)
    }
    /// `WideAppendUtf16LE`'s units.
    fn utf16(&self, u: &[u32]) -> Vec<u16> {
        if self.ws == 2 {
            u.iter().map(|&c| c as u16).collect()
        } else {
            text::utf16_from_utf32(u)
        }
    }
    /// `WideAppendUtf16Unit` per unit (`WideAssignUtf16LE` the same).
    fn wide(&self, u: &[u16]) -> Vec<u32> {
        if self.ws == 2 {
            u.iter().map(|&c| u32::from(c)).collect()
        } else {
            text::utf32_from_utf16(u)
        }
    }
    fn text(&self, t: EeWonPrefsText) -> Vec<u32> {
        self.widen(self.prefs.text(t))
    }
    fn set_text(&mut self, t: EeWonPrefsText, v: &[u32]) {
        let b = self.narrow(v);
        self.prefs.set_text(t, &b);
    }

    // ---- the class's methods

    /// `GetUserFileName`.
    fn user_file_name(id: i64) -> String {
        persist::user_file_name(id as i32)
    }
    /// `FindUnusedUserId`.
    fn find_unused_user_id(&self) -> i64 {
        let ids: Vec<i32> = (0..self.user_ids.len()).map(|i| self.user_ids.num_at(i) as i64 as i32).collect();
        i64::from(persist::find_unused_user_id(&ids))
    }
    /// `DecryptString`: an empty text when it cannot be decoded.
    fn decrypt(&self, data: &[u8]) -> Vec<u32> {
        let Some(plain) = self.key.decrypt(data) else { return Vec::new() };
        let mut r = Reader::new(&plain);
        match r.read_wstring(2) {
            Ok(u) => self.wide(&u),
            Err(_) => Vec::new(),
        }
    }
    /// `EncryptString` (an empty length when it fails).
    fn encrypt(&self, t: &[u32]) -> Vec<u8> {
        let mut w = Writer::new(0, true);
        w.append_wstring(&self.utf16(t), 2);
        self.key.encrypt(w.data()).unwrap_or_default()
    }
    /// `GetDefaultLanName`: the resource's name, with the local address's number unless the config turns it off.
    fn default_lan_name(&self) -> Vec<u32> {
        let mut name = self.res(EeWonLobbyRes::LobbyOptions_DefaultName_String);
        if self.op(EeWonPersistOp::AppendLanNumber, 0, 0, 0) == 0 {
            return name;
        }
        let host = ipobj::local_addr().host();
        let n = persist::default_lan_number(host);
        name.extend(n.to_string().bytes().map(u32::from));
        name
    }
    /// `RestoreDefaults`.
    fn restore_defaults(&mut self) {
        let q = self.narrow(&self.res(EeWonLobbyRes::LobbyGlobal_QuickPlay_String));
        let l = self.narrow(&self.res(EeWonLobbyRes::LobbyGlobal_Lan_String));
        if self.prefs.restore_defaults(&q, &l) {
            let n = self.default_lan_name();
            self.set_text(EeWonPrefsText::LanName, &n);
        }
    }
    /// `PruneUsers`: at most 50 (persist.rs chooses; a dropped user with an id loses its file and its id).
    fn prune_users(&mut self) {
        if self.users.len() <= 50 {
            return;
        }
        let list: Vec<(i32, i64)> =
            (0..self.users.len()).map(|i| (self.users.at(i).user_id as i32, self.users.at(i).last_use)).collect();
        let mut drop = vec![0u8; list.len()];
        for (i, with_file) in persist::prune_users(&list) {
            drop[i] = if with_file { 2 } else { 1 };
        }
        // from the end, so the earlier positions stay
        for i in (0..drop.len()).rev() {
            if drop[i] == 0 {
                continue;
            }
            if drop[i] == 2 {
                let id = self.users.at(i).user_id;
                delete_file(&Self::user_file_name(id));
                self.user_ids.erase(&num_key(id));
            }
            self.users.erase_at(i);
        }
    }
    /// `PruneLanNames`.
    fn prune_lan_names(&mut self) {
        if self.lan_names.len() <= 50 {
            return;
        }
        let times: Vec<i64> = (0..self.lan_names.len()).map(|i| *self.lan_names.at(i)).collect();
        let mut drop = vec![false; times.len()];
        for i in persist::prune_lan_names(&times) {
            drop[i] = true;
        }
        for i in (0..times.len()).rev() {
            if drop[i] {
                self.lan_names.erase_at(i);
            }
        }
    }
    /// `SelectRecentLanName`.
    fn select_recent_lan_name(&mut self) {
        let d = self.default_lan_name();
        self.set_text(EeWonPrefsText::LanName, &d);
        let times: Vec<i64> = (0..self.lan_names.len()).map(|i| *self.lan_names.at(i)).collect();
        if let Some(i) = persist::recent_lan_name(&times) {
            let n = self.lan_names.units_at(i).to_vec();
            self.set_text(EeWonPrefsText::LanName, &n);
        }
    }

    /// `LoadGlobalPrefs`.
    fn load_global(&mut self) {
        let Some(bytes) = read_file(GLOBAL_FILE) else { return };
        self.users.clear();
        self.prefs.set_network_adapter(0);
        let mut user: Option<usize> = None;
        let mut events = Vec::new();
        let read = persist::read_global(&bytes, |e| events.push(e));
        for e in events {
            match e {
                Event::User(n) => {
                    let name = self.wide(&n);
                    let (i, _) = self.users.insert(Key::Units(name.clone()));
                    self.users.at_mut(i).name = name;
                    user = Some(i);
                }
                Event::UserTime(v) => self.user_mut(user).last_use = i64::from(v),
                Event::UserId(v) => self.user_mut(user).user_id = i64::from(v),
                Event::UserValue04(v) => self.user_mut(user).value04 = i64::from(v),
                Event::UserValue08(v) => self.user_mut(user).value08 = i64::from(v),
                Event::UserPassword(b) => {
                    let pw = self.decrypt(&b);
                    let u = self.user_mut(user);
                    u.password = pw;
                    let id = u.user_id;
                    self.user_ids.insert(num_key(id));
                }
                Event::SoundEffects(v) => {
                    self.prefs.set_flag(F::LobbySoundEffects, v);
                    self.prefs.set_flag(F::SavedLobbySoundEffects, v);
                    // the click sound is used without a null check, as the original did
                    self.mute_click();
                }
                Event::Music(v) => {
                    self.prefs.set_flag(F::LobbyMusic, v);
                    self.prefs.set_flag(F::SavedLobbyMusic, v);
                }
                Event::NetworkAdapter(v) => self.prefs.set_network_adapter(i64::from(v)),
                _ => {}
            }
        }
        if read == persist::EeWonPrefsRead::OtherVersion {
            return;
        }
        self.prefs.set_flag(F::GlobalDirty, false);
    }
    fn user_mut(&mut self, i: Option<usize>) -> &mut EeWonPersistUser {
        self.users.at_mut(i.expect("persist: a user's field before its name"))
    }

    /// `SaveGlobalPrefs`.
    fn save_global(&mut self) {
        let users = (0..self.users.len())
            .map(|i| {
                let u = self.users.at(i);
                persist::SavedUser {
                    name: self.utf16(&u.name),
                    last_use: u.last_use as i32,
                    id: u.user_id as u16,
                    value04: u.value04 as i32,
                    value08: u.value08 as i32,
                    password: self.encrypt(&u.password),
                }
            })
            .collect();
        let g = persist::GlobalPrefs {
            users,
            sound_effects: self.prefs.flag(F::LobbySoundEffects),
            music: self.prefs.flag(F::LobbyMusic),
            network_adapter: self.prefs.network_adapter() as i32,
        };
        if !write_file(GLOBAL_FILE, &persist::write_global(&g)) {
            return;
        }
        // the saved sound and music states are updated even when a write failed
        self.prefs.after_global_save();
    }

    /// `LoadUserPrefs`.
    fn load_user(&mut self) {
        self.restore_defaults();
        self.reconnect.clear();
        self.ignored.clear();
        let old: Vec<isize> = self.friends.clear().into_iter().map(|b| *b).collect();
        for f in old {
            self.friend_release(f);
        }
        self.lan_names.clear();
        self.direct.clear();
        self.options.clear();
        let current = self.prefs.current_user();
        let Some(bytes) = read_file(&Self::user_file_name(current)) else {
            self.options = self.default_options.clone();
            return;
        };
        let mut events = Vec::new();
        let read = persist::read_user(&bytes, current == 0, |e| events.push(e));
        let (mut reconnect, mut option, mut have_options) = (0usize, None::<usize>, false);
        // the settings in the file's order (persist.rs Settings)
        const FLAGS: [Option<F>; 14] = [
            Some(F::FilterDirtyWords),
            Some(F::DoAutoAfk),
            Some(F::CrossPromotion),
            Some(F::AutoPingGames),
            Some(F::AnonymousToFriends),
            Some(F::ShowGamesInProgress),
            None,
            None,
            None,
            Some(F::LobbySoundEffects),
            Some(F::LobbyMusic),
            Some(F::ChatSoundEffects),
            Some(F::StagingSoundEffects),
            Some(F::ShowConfirmations),
        ];
        const TEXTS: [EeWonPrefsText; 3] =
            [EeWonPrefsText::TabSelect, EeWonPrefsText::LastServerName, EeWonPrefsText::LastRoomName];
        for e in events {
            match e {
                Event::LanName(n, t) => {
                    let (i, _) = self.lan_names.insert(Key::Units(self.wide(&n)));
                    *self.lan_names.at_mut(i) = i64::from(t);
                }
                Event::LanNamesDone => self.select_recent_lan_name(),
                Event::DirectConnect(six, n) => {
                    let name = self.wide(&n);
                    let (i, _) = self.direct.insert(Key::Addr(six_addr(&six)));
                    *self.direct.at_mut(i) = name;
                }
                Event::BroadcastPort(v) => self.prefs.set_number(EeWonPrefsNumber::LanBroadcastPort, v),
                Event::Reconnect(six) => reconnect = self.reconnect.insert(Key::Addr(six_addr(&six))).0,
                Event::ReconnectTime(v) => self.reconnect.at_mut(reconnect)[0] = i64::from(v),
                Event::ReconnectId(v) => self.reconnect.at_mut(reconnect)[1] = i64::from(v),
                Event::Ignored(n) => {
                    self.ignored.insert(Key::Units(self.wide(&n)));
                }
                Event::Friend(n, id) => {
                    let name = self.wide(&n);
                    let shell = self.friend_new(&name, i64::from(id));
                    let (i, new) = self.friends.insert(num_key(i64::from(id)));
                    if new {
                        *self.friends.at_mut(i) = shell;
                    } else {
                        self.friend_release(shell);
                    }
                }
                Event::Blocked(v) => {
                    self.blocked.insert(num_key(i64::from(v)));
                }
                Event::Flag(i, v) => {
                    if let Some(Some(f)) = FLAGS.get(usize::from(i)) {
                        self.prefs.set_flag(*f, v);
                    }
                }
                Event::SettingText(i, t) => {
                    if (6..9).contains(&i) {
                        let w = self.wide(&t);
                        self.set_text(TEXTS[usize::from(i) - 6], &w);
                    }
                }
                Event::ConnectionSpeed(v) => self.prefs.set_number(EeWonPrefsNumber::ConnectionSpeed, v),
                Event::OptionsBegin => have_options = true,
                // the filter's name is UTF-8 (D6)
                Event::OptionName(b) => option = Some(self.options.insert(Key::Units(self.widen(&b))).0),
                Event::OptionValue(v) => {
                    let w = self.wide(&v);
                    let i = option.expect("persist: a filter value before its name");
                    self.options.at_mut(i).insert(Key::Units(w));
                }
                _ => {}
            }
        }
        if read == persist::EeWonPrefsRead::OtherVersion {
            return;
        }
        if !have_options {
            self.options = self.default_options.clone();
        }
        self.prefs.after_user_load();
    }
    fn friend_new(&self, name: &[u32], id: i64) -> isize {
        self.op(EeWonPersistOp::FriendNew, name.as_ptr() as isize, name.len() as isize, id as isize)
    }

    /// `SaveUserPrefs`.
    fn save_user(&mut self) {
        let current = self.prefs.current_user();
        let lan = (current == 0).then(|| persist::LanPrefs {
            names: (0..self.lan_names.len())
                .map(|i| (self.utf16(self.lan_names.units_at(i)), *self.lan_names.at(i) as i32))
                .collect(),
            direct: (0..self.direct.len())
                .map(|i| (self.direct.addr_at(i).addr().six_bytes(), self.utf16(self.direct.at(i))))
                .collect(),
            broadcast_port: self.prefs.number(EeWonPrefsNumber::LanBroadcastPort),
        });
        let friends = (0..self.friends.len())
            .map(|i| {
                let name = self.op_units(EeWonPersistOp::FriendName, *self.friends.at(i));
                (self.utf16(&name), self.friends.num_at(i) as u32)
            })
            .collect();
        let p = &self.prefs;
        let settings = persist::Settings {
            first: [
                p.flag(F::FilterDirtyWords),
                p.flag(F::DoAutoAfk),
                p.flag(F::CrossPromotion),
                p.flag(F::AutoPingGames),
                p.flag(F::AnonymousToFriends),
                p.flag(F::ShowGamesInProgress),
            ],
            texts: [
                self.utf16(&self.text(EeWonPrefsText::TabSelect)),
                self.utf16(&self.text(EeWonPrefsText::LastServerName)),
                self.utf16(&self.text(EeWonPrefsText::LastRoomName)),
            ],
            second: [
                p.flag(F::LobbySoundEffects),
                p.flag(F::LobbyMusic),
                p.flag(F::ChatSoundEffects),
                p.flag(F::StagingSoundEffects),
                p.flag(F::ShowConfirmations),
            ],
            connection_speed: p.number(EeWonPrefsNumber::ConnectionSpeed),
        };
        let u = persist::UserPrefs {
            lan,
            reconnects: (0..self.reconnect.len())
                .map(|i| {
                    let r = self.reconnect.at(i);
                    (self.reconnect.addr_at(i).addr().six_bytes(), r[0] as i32, r[1] as i32)
                })
                .collect(),
            ignored: (0..self.ignored.len()).map(|i| self.utf16(self.ignored.units_at(i))).collect(),
            friends,
            blocked: (0..self.blocked.len()).map(|i| self.blocked.num_at(i) as u32).collect(),
            settings,
            options: (0..self.options.len())
                .map(|i| {
                    // the filter's name as UTF-8 (GetString)
                    let set = self.options.at(i);
                    persist::GameOption {
                        name: self.narrow(self.options.units_at(i)),
                        values: (0..set.len()).map(|v| self.utf16(set.units_at(v))).collect(),
                    }
                })
                .collect(),
        };
        if !write_file(&Self::user_file_name(current), &persist::write_user(&u)) {
            return;
        }
        self.prefs.set_flag(F::UserDirty, false);
    }

    // ---- the Lobby* functions

    /// `LobbyAddUser`: the record, or None (`inExistingOnly` and a user never seen before).
    fn add_user(&mut self, name: Vec<u32>, password: Vec<u32>, existing_only: bool) -> *mut EeWonPersistUser {
        if !existing_only {
            self.prune_users();
        }
        let (i, _) = self.users.insert(Key::Units(name.clone()));
        if existing_only && self.users.at(i).last_use == 0 {
            self.users.erase_at(i);
            return std::ptr::null_mut();
        }
        if self.users.at(i).user_id == 0 {
            let id = self.find_unused_user_id();
            self.users.at_mut(i).user_id = id;
            self.user_ids.insert(num_key(id));
            delete_file(&Self::user_file_name(id));
        }
        let now = self.now();
        let u = self.users.at_mut(i);
        u.last_use = now;
        u.name = name;
        u.password = password;
        self.prefs.set_flag(F::GlobalDirty, true);
        self.users.at_mut(i) as *mut EeWonPersistUser
    }
    /// `LobbyGetCurrentUser`.
    fn current_user(&mut self) -> *mut EeWonPersistUser {
        let id = self.prefs.current_user();
        if id == 0 {
            return std::ptr::null_mut();
        }
        for i in 0..self.users.len() {
            if self.users.at(i).user_id == id {
                return self.users.at_mut(i) as *mut EeWonPersistUser;
            }
        }
        std::ptr::null_mut()
    }
    /// `LobbyAddFriend`: the friend (the map's).
    fn add_friend(&mut self, name: &[u32], id: i64) -> isize {
        let (i, _) = self.friends.insert(num_key(id));
        if *self.friends.at(i) == 0 {
            let shell = self.friend_new(name, id);
            *self.friends.at_mut(i) = shell;
            self.prefs.set_flag(F::UserDirty, true);
        }
        *self.friends.at(i)
    }
    /// `LobbyAddGameOption`: comma-separated values into a default filter (skipped when it has values, unless
    /// appending), read as StringParser::ReadString read them.
    fn add_game_option(&mut self, name: Vec<u32>, values: &[u32], append: bool) {
        let (i, _) = self.default_options.insert(Key::Units(name));
        if self.default_options.at(i).len() != 0 && !append {
            return;
        }
        let mut text: Vec<u32> = values.to_vec();
        text.push(0);
        let mut pos = 0i32;
        loop {
            let mut value: Vec<u32> = Vec::new();
            // SAFETY: a NUL-terminated text of 4-byte units; the sink appends to `value`
            unsafe {
                ee_won_net::ffi_parse::ee_won_parser_read_wstring(
                    text.as_ptr().cast(),
                    4,
                    &mut pos,
                    false,
                    append_units,
                    (&mut value as *mut Vec<u32>).cast(),
                )
            };
            if value.is_empty() {
                break;
            }
            self.default_options.at_mut(i).insert(Key::Units(value));
            // SAFETY: as above
            if !unsafe { ee_won_net::ffi_parse::ee_won_parser_check_next_char(text.as_ptr().cast(), 4, &mut pos, 44) } {
                break;
            }
        }
    }
    /// `LobbySetLanName`.
    fn set_lan_name(&mut self, name: Vec<u32>) {
        if name.is_empty() || guistring::compare(&name, &self.text(EeWonPrefsText::LanName)) == 0 {
            return;
        }
        let now = self.now();
        let (i, _) = self.lan_names.insert(Key::Units(name.clone()));
        *self.lan_names.at_mut(i) = now;
        self.set_text(EeWonPrefsText::LanName, &name);
        self.prune_lan_names();
        self.prefs.set_flag(F::UserDirty, true);
    }
    /// `LobbyRemoveLanName`.
    fn remove_lan_name(&mut self) {
        if self.lan_names.erase(&Key::Units(self.text(EeWonPrefsText::LanName))).is_none() {
            return;
        }
        self.select_recent_lan_name();
        self.prefs.set_flag(F::UserDirty, true);
    }
    /// `LobbySetReconnectId`: then saves.
    fn set_reconnect_id(&mut self, a: &EeWonIpAddr, id: i64) {
        let (i, _) = self.reconnect.insert(addr_key(a));
        let now = self.now();
        self.reconnect.at_mut(i)[0] = now;
        self.reconnect.at_mut(i)[1] = id as i32 as i64;
        self.save_user();
    }

    /// One call (see [`EeWonPersistCall`]); `t1`, `t2` the texts, `a`, `b` the numbers; a text result's count in
    /// `out` (its units the object's own, valid to the next call).
    fn call(&mut self, call: EeWonPersistCall, t1: Vec<u32>, t2: Vec<u32>, a: i64, b: i64, out: &mut usize) -> isize {
        use EeWonPersistCall as C;
        let flag = |a: i64| -> F {
            // SAFETY: the callers pass an EeWonPrefsFlag value
            unsafe { std::mem::transmute::<i32, F>(a as i32) }
        };
        let r: isize = match call {
            C::Flag => isize::from(self.prefs.flag(flag(a))),
            C::ChangeFlag => {
                self.prefs.change_flag(flag(a), b != 0);
                0
            }
            C::ConnectionSpeed => self.prefs.number(EeWonPrefsNumber::ConnectionSpeed) as isize,
            C::ChangeConnectionSpeed => {
                self.prefs.change_number(EeWonPrefsNumber::ConnectionSpeed, a as u16);
                0
            }
            C::LanBroadcastPort => self.prefs.number(EeWonPrefsNumber::LanBroadcastPort) as isize,
            C::ChangeLanBroadcastPort => {
                self.prefs.change_number(EeWonPrefsNumber::LanBroadcastPort, a as u16);
                0
            }
            C::NetworkAdapter => self.prefs.network_adapter() as c_long as isize,
            C::SaveNetworkAdapter => {
                self.prefs.save_network_adapter(i64::from(ipobj::local_host()));
                0
            }
            C::SaveGlobal => {
                if a != 0 || self.prefs.flag(F::GlobalDirty) {
                    self.save_global();
                }
                0
            }
            C::SaveUser => {
                if a != 0 || self.prefs.flag(F::UserDirty) {
                    self.save_user();
                }
                0
            }
            C::LoadGlobal => {
                self.load_global();
                0
            }
            C::RestoreDefaults => {
                let old = self.prefs.network_adapter();
                self.restore_defaults();
                self.prefs.after_restore(old);
                0
            }
            C::SetCurrentUser => {
                let u = a as isize as *const EeWonPersistUser;
                // SAFETY: NULL or a record this object handed out, still saved
                let id = if u.is_null() { 0 } else { unsafe { (*u).user_id } };
                self.prefs.set_current_user(id);
                self.load_user();
                0
            }
            C::CurrentUser => self.current_user() as isize,
            C::UserCount => self.users.len() as isize,
            C::UserNameAt => return self.give(self.users.units_at(a as usize).to_vec(), out),
            C::UserLastUseAt => self.users.at(a as usize).last_use as c_long as isize,
            C::FindUser => match self.users.find(&Key::Units(t1)) {
                Some(i) => self.users.at_mut(i) as *mut EeWonPersistUser as isize,
                None => 0,
            },
            C::AddUser => self.add_user(t1, t2, a != 0) as isize,
            C::FriendCount => self.friends.len() as isize,
            C::FriendAt => *self.friends.at(a as usize),
            C::FindFriend => self.friends.get(&num_key(a)).copied().unwrap_or(0),
            C::AddFriend => self.add_friend(&t1, a),
            C::RemoveFriend => {
                let gone = self.friends.erase(&num_key(a));
                self.prefs.set_flag(F::UserDirty, gone.is_some());
                if let Some(f) = gone {
                    self.friend_release(*f);
                }
                0
            }
            C::IgnoredCount => self.ignored.len() as isize,
            C::IgnoredAt => return self.give(self.ignored.units_at(a as usize).to_vec(), out),
            C::IsIgnored => isize::from(self.ignored.contains(&Key::Units(t1))),
            C::SetIgnored => {
                let dirty = if a != 0 {
                    self.ignored.insert(Key::Units(t1)).1
                } else {
                    self.ignored.erase(&Key::Units(t1)).is_some()
                };
                self.prefs.set_flag(F::UserDirty, dirty);
                0
            }
            C::IsCDKeyBlocked => isize::from(self.blocked.contains(&num_key(a))),
            C::Text => {
                // SAFETY: the callers pass an EeWonPrefsText value
                let t = unsafe { std::mem::transmute::<i32, EeWonPrefsText>(a as i32) };
                return self.give(self.text(t), out);
            }
            C::ChangeTabSelect => {
                let b = self.narrow(&t1);
                self.prefs.change_tab_select(&b);
                0
            }
            C::SetLastRoom => {
                let (s, r) = (self.narrow(&t1), self.narrow(&t2));
                self.prefs.set_last_room(&s, &r);
                0
            }
            C::LanNameCount => self.lan_names.len() as isize,
            C::LanNameAt => return self.give(self.lan_names.units_at(a as usize).to_vec(), out),
            C::SetLanName => {
                self.set_lan_name(t1);
                0
            }
            C::RemoveLanName => {
                self.remove_lan_name();
                0
            }
            C::DirectCount => self.direct.len() as isize,
            C::DirectAddrAt => {
                self.out_addr = self.direct.addr_at(a as usize).clone();
                &self.out_addr as *const EeWonIpAddr as isize
            }
            C::DirectNameAt => return self.give(self.direct.at(a as usize).clone(), out),
            C::AddDirect => {
                // SAFETY: a live address (the caller's)
                let addr = unsafe { &*(a as isize as *const EeWonIpAddr) };
                let (i, _) = self.direct.insert(addr_key(addr));
                *self.direct.at_mut(i) = t1;
                self.prefs.set_flag(F::UserDirty, true);
                0
            }
            C::RemoveDirect => {
                // SAFETY: as AddDirect
                let addr = unsafe { &*(a as isize as *const EeWonIpAddr) };
                if self.direct.erase(&addr_key(addr)).is_some() {
                    self.prefs.set_flag(F::UserDirty, true);
                }
                0
            }
            C::ReconnectId => {
                // SAFETY: as AddDirect
                let addr = unsafe { &*(a as isize as *const EeWonIpAddr) };
                match self.reconnect.get(&addr_key(addr)) {
                    Some(r) => r[1] as i32 as isize,
                    None => 0,
                }
            }
            C::SetReconnectId => {
                // SAFETY: as AddDirect
                let addr = unsafe { &*(a as isize as *const EeWonIpAddr) }.clone();
                self.set_reconnect_id(&addr, b);
                0
            }
            C::FilterCount => match self.options.get(&Key::Units(t1)) {
                Some(s) => s.len() as isize,
                None => 0,
            },
            C::FilterValueAt => {
                let v = self.options.get(&Key::Units(t1)).map(|s| s.units_at(a as usize).to_vec()).unwrap_or_default();
                return self.give(v, out);
            }
            C::AddFilterValue => {
                let (i, _) = self.options.insert(Key::Units(t1));
                self.options.at_mut(i).insert(Key::Units(t2));
                self.prefs.set_flag(F::UserDirty, true);
                0
            }
            C::RemoveFilterValue => {
                let (i, _) = self.options.insert(Key::Units(t1));
                if self.options.at_mut(i).erase(&Key::Units(t2)).is_some() {
                    self.prefs.set_flag(F::UserDirty, true);
                }
                0
            }
            C::ResetFilters => {
                // the original copied the option sets memberwise (lobbymap.rs copied them; a copy here)
                self.options = self.default_options.clone();
                self.prefs.set_flag(F::UserDirty, true);
                0
            }
            C::AddGameOption => {
                self.add_game_option(t1, &t2, a != 0);
                0
            }
        };
        r
    }
    fn give(&mut self, t: Vec<u32>, out: &mut usize) -> isize {
        *out = t.len();
        self.out_text = t;
        self.out_text.as_ptr() as isize
    }
}

impl Drop for EeWonPersist {
    fn drop(&mut self) {
        let old: Vec<isize> = self.friends.clear().into_iter().map(|b| *b).collect();
        for f in old {
            self.friend_release(f);
        }
    }
}

/// `count` units of `unit_size` bytes as u32s.
///
/// # Safety
/// `text` holds them (or is NULL with a count of 0).
unsafe fn units(text: *const c_void, count: usize, unit_size: u32) -> Vec<u32> {
    if text.is_null() || count == 0 {
        return Vec::new();
    }
    // SAFETY: the caller's contract
    unsafe { ee_won_net::chat::units(ee_won_net::chat::EeWonChatText { text, length: count }, unit_size) }
}

unsafe extern "C" fn append_units(ctx: *mut c_void, u: *const u32, n: usize) {
    // SAFETY: the parser's sink contract: `n` units at `u`; `ctx` is the Vec this module passed
    unsafe { (*ctx.cast::<Vec<u32>>()).extend_from_slice(std::slice::from_raw_parts(u, n)) };
}

/// The whole file, as FileReader read it in 4,096-byte pieces (a read error ends it); None when it does not open.
fn read_file(name: &str) -> Option<Vec<u8>> {
    let path = CString::new(name).ok()?;
    let mut r = EeWonFileReader::default();
    // SAFETY: a NUL-terminated path
    if !unsafe { r.open(path.as_ptr()) } {
        return None;
    }
    let mut out = Vec::new();
    let mut buf = [0u8; 4096];
    loop {
        // SAFETY: room for 4,096 bytes
        let (n, status) = unsafe { r.read_max(buf.as_mut_ptr(), buf.len() as i32) };
        if status != 0 || n <= 0 {
            break;
        }
        out.extend_from_slice(&buf[..n as usize]);
    }
    r.close();
    Some(out)
}

/// FileWriter's Open (not retrying a read-only file) and WriteBytes (a failed write ignored): false when it did
/// not open.
fn write_file(name: &str, bytes: &[u8]) -> bool {
    let Ok(path) = CString::new(name) else { return false };
    let mut w = EeWonFileWriter::new();
    // SAFETY: a NUL-terminated path
    if !unsafe { w.open(path.as_ptr()) } {
        return false;
    }
    let _ = w.write(bytes);
    w.close();
    true
}

/// `WONFile(name).Delete(true)`.
fn delete_file(name: &str) {
    EeWonFile::new(name.as_bytes()).delete(true);
}

/// The calls of [`ee_won_persist_call`]: the `Lobby*` functions (and the class's methods they ran). `a`, `b` are
/// numbers, `t1`, `t2` texts; a record is an `EeWonPersistUser*`, a friend its shell, an address an
/// `EeWonIpAddr*`.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonPersistCall {
    /// `GetFlag(a)` (an `EeWonPrefsFlag`); false without lobby data.
    Flag = 0,
    /// `ChangeFlag(a, b)`: the `LobbySet*` flag setters.
    ChangeFlag,
    /// `LobbyGetConnectionSpeed` (50 without lobby data).
    ConnectionSpeed,
    /// `LobbySetConnectionSpeed(a)`.
    ChangeConnectionSpeed,
    /// The stored LAN broadcast port (8585 without lobby data; the C++ adds the test offset).
    LanBroadcastPort,
    /// `ChangeLanBroadcastPort(a)` (the C++ took the test offset off).
    ChangeLanBroadcastPort,
    /// `LobbyGetNetworkAdapter`.
    NetworkAdapter,
    /// `LobbySaveNetworkAdapter`.
    SaveNetworkAdapter,
    /// `LobbySaveGlobalPrefs(a)`.
    SaveGlobal,
    /// `LobbySaveUserPrefs(a)`.
    SaveUser,
    /// `LobbyLoadGlobalPrefs`.
    LoadGlobal,
    /// `LobbyRestoreDefaults`.
    RestoreDefaults,
    /// `LobbySetCurrentUser((LobbyUserInfo*)a)`.
    SetCurrentUser,
    /// `LobbyGetCurrentUser`.
    CurrentUser,
    /// `LobbyGetUserCount`.
    UserCount,
    /// `LobbyGetUserNameAt(a)`: a text.
    UserNameAt,
    /// `LobbyGetUserLastUseAt(a)`.
    UserLastUseAt,
    /// `LobbyFindUser(t1)`.
    FindUser,
    /// `LobbyAddUser(t1, t2, a)`.
    AddUser,
    /// `LobbyGetFriendCount`.
    FriendCount,
    /// `LobbyGetFriendAt(a)`.
    FriendAt,
    /// `LobbyFindFriend(a)`.
    FindFriend,
    /// `LobbyAddFriend(t1, a)`.
    AddFriend,
    /// `LobbyRemoveFriend(a)`.
    RemoveFriend,
    /// `LobbyGetIgnoredNameCount`.
    IgnoredCount,
    /// `LobbyGetIgnoredNameAt(a)`: a text.
    IgnoredAt,
    /// `LobbyIsNameIgnored(t1)`.
    IsIgnored,
    /// `LobbySetNameIgnored(t1, a)`.
    SetIgnored,
    /// `LobbyIsCDKeyBlocked(a)`.
    IsCDKeyBlocked,
    /// A saved text (`a`, an `EeWonPrefsText`: `LobbyGetTabSelect`, `LobbyGetLanName`, the last server and room).
    Text,
    /// `LobbySetTabSelect(t1)`.
    ChangeTabSelect,
    /// `LobbySetLastRoom(t1, t2)`.
    SetLastRoom,
    /// `LobbyGetLanNameCount`.
    LanNameCount,
    /// `LobbyGetLanNameAt(a)`: a text.
    LanNameAt,
    /// `LobbySetLanName(t1)`.
    SetLanName,
    /// `LobbyRemoveLanName`.
    RemoveLanName,
    /// `LobbyGetDirectConnectCount`.
    DirectCount,
    /// `LobbyGetDirectConnectAddressAt(a)`: an `EeWonIpAddr*` (the object's, to the next call).
    DirectAddrAt,
    /// `LobbyGetDirectConnectNameAt(a)`: a text.
    DirectNameAt,
    /// `LobbyAddDirectConnect(*(EeWonIpAddr*)a, t1)`.
    AddDirect,
    /// `LobbyRemoveDirectConnect(*(EeWonIpAddr*)a)`.
    RemoveDirect,
    /// `LobbyGetReconnectId(*(EeWonIpAddr*)a)`.
    ReconnectId,
    /// `LobbySetReconnectId(*(EeWonIpAddr*)a, b)`.
    SetReconnectId,
    /// `LobbyGetFilterValues(t1).size()`.
    FilterCount,
    /// `LobbyGetFilterValues(t1).ValueAt(a)`: a text.
    FilterValueAt,
    /// `LobbyAddFilterValue(t1, t2)`.
    AddFilterValue,
    /// `LobbyRemoveFilterValue(t1, t2)`.
    RemoveFilterValue,
    /// `LobbyResetFilters`.
    ResetFilters,
    /// `LobbyAddGameOption(t1, t2, a)`.
    AddGameOption,
}

/// What a call answers without lobby data (the functions' early returns).
fn no_data(call: EeWonPersistCall, out: &mut usize) -> isize {
    use EeWonPersistCall as C;
    *out = 0;
    match call {
        C::ConnectionSpeed => 50,
        C::LanBroadcastPort => 8585,
        _ => 0,
    }
}

// ---- FFI

/// A new object: the host, the C++ `sizeof(wchar_t)`, the password key's bytes (the original's: 0x22fa655f and
/// the machine id, as two little-endian longs).
///
/// # Safety
/// `key` holds `key_length` bytes; the host lives as long as the object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_persist_new(
    host: EeWonPersistHost,
    wchar_size: u32,
    key: *const u8,
    key_length: usize,
) -> *mut EeWonPersist {
    let k = if key.is_null() {
        &[][..]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(key, key_length) }
    };
    Box::into_raw(EeWonPersist::new(host, wchar_size, k))
}

/// Frees the object (its friends' references given back).
///
/// # Safety
/// `p` is NULL or from `ee_won_persist_new`, not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_persist_free(p: *mut EeWonPersist) {
    if !p.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(p) });
    }
}

/// One call (see [`EeWonPersistCall`]); `p` NULL answers as without lobby data. A text result is a pointer to
/// `*out` units (`uint32_t`s, one `wchar_t` each; the object's, valid to its next call); `out` may be NULL when the
/// call gives none.
///
/// # Safety
/// `p` is NULL or live; the texts hold their units; the pointers the call takes are live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_persist_call(
    p: *mut EeWonPersist,
    call: EeWonPersistCall,
    t1: EeWonPersistText,
    t2: EeWonPersistText,
    a: i64,
    b: i64,
    out: *mut usize,
) -> isize {
    let mut n = 0usize;
    let r = if p.is_null() {
        no_data(call, &mut n)
    } else {
        // SAFETY: the caller's contract
        let (u1, u2) = unsafe { (units(t1.text, t1.count, t1.unit_size), units(t2.text, t2.count, t2.unit_size)) };
        // SAFETY: a live object, not otherwise borrowed (the host never calls back into it)
        unsafe { (*p).call(call, u1, u2, a, b, &mut n) }
    };
    if !out.is_null() {
        // SAFETY: writable
        unsafe { *out = n };
    }
    r
}

/// A saved user's field: 0 the id, 1 and 2 the two longs (the terms of use's times), 3 the last use.
///
/// # Safety
/// `u` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_won_persist_user_number(u: *const EeWonPersistUser, field: i32) -> i64 {
    // SAFETY: the caller's contract
    let u = unsafe { &*u };
    match field {
        0 => u.user_id,
        1 => u.value04,
        2 => u.value08,
        _ => u.last_use,
    }
}

/// Sets a saved user's two longs (`LobbyGetTOUTimes(user->mValue04, user->mValue08)`).
///
/// # Safety
/// `u` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_won_persist_user_set_tou(u: *mut EeWonPersistUser, value04: i64, value08: i64) {
    // SAFETY: the caller's contract
    let u = unsafe { &mut *u };
    u.value04 = value04;
    u.value08 = value08;
}

/// A saved user's name (`which` 0) or password (1): `*count` units (`uint32_t`s, one `wchar_t` each; the
/// record's own, valid while it is unchanged).
///
/// # Safety
/// `u` is a live record; `count` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_persist_user_text(
    u: *const EeWonPersistUser,
    which: i32,
    count: *mut usize,
) -> *const u32 {
    // SAFETY: the caller's contract
    let u = unsafe { &*u };
    let t = if which == 0 { &u.name } else { &u.password };
    unsafe { *count = t.len() };
    t.as_ptr()
}

#[cfg(test)]
#[path = "persistobj_tests.rs"]
mod tests;
