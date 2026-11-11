//! The lobby's saved settings (LobbyPersistentData.cpp): the two files' layouts, written from plain lists and read
//! as a stream of events in the order the original applied them, and the small rules around them (a user's file
//! name, the first free user id, which users and LAN names are pruned, which LAN name is the most recent, the
//! default LAN name's number). The C++ keeps the maps and sets, the password key (a saved password is Blowfish
//! bytes here; the C++ encrypts and decrypts), the files' opening and the defaults.
//!
//! Both files start with the tag `WONPER` and a 32-bit version; every number is little-endian, a count or a
//! string's length is 16 bits, a wide string's characters are UTF-16 units, a byte string is UTF-8 (D6).
//!
//! `_wonlobbypersistent.dat`, version 2: the users (name, last use time, id as 16 bits, two longs, the encrypted
//! password: a length and its bytes), the lobby sound and music bytes, the network adapter's address (32 bits).
//!
//! `_wonuser<id>.dat`, version 4: for the LAN user (id 0) its LAN names (name, time), direct-connect addresses (six
//! bytes, name) and broadcast port; then the reconnect ids (six bytes, time, id), the ignored names, the friends
//! (name, user id), the blocked CD key ids, six setting bytes, the tab, last server and last room, five more setting
//! bytes, the connection speed, and the game filters (a byte-string name, a count, wide values). A setting byte
//! means on only when it is 1. A filter with no values is written without its count (the original's bug: the
//! filters after it are misread on the next load).
//!
//! A read that runs out stops where the original's `FileReaderException` stopped it: every event before the cut
//! has been given, as the original had assigned those fields; a reconnect entry exists from its address on, a game
//! filter from its name on, a saved user from its name on.

use std::ffi::c_void;

use crate::ffi::EeWonByteSink;
use crate::wire::{PastEnd, Reader, Writer};

pub(crate) const TAG: &[u8; 6] = b"WONPER";
pub(crate) const GLOBAL_VERSION: i32 = 2;
pub(crate) const USER_VERSION: i32 = 4;
/// At most this many saved users and LAN names are kept.
pub(crate) const MAX_USERS: usize = 50;
pub(crate) const MAX_LAN_NAMES: usize = 50;

/// `GetUserFileName`.
pub fn user_file_name(id: i32) -> String {
    format!("_wonuser{id}.dat")
}

/// `FindUnusedUserId` over the ids in ascending order (a `std::set`): the first id after the run of consecutive
/// ids from the smallest; 1 when there are none.
pub fn find_unused_user_id(sorted_ids: &[i32]) -> i32 {
    let Some(&first) = sorted_ids.first() else { return 1 };
    let mut id = first;
    for &have in sorted_ids {
        if have != id {
            break;
        }
        id = id.wrapping_add(1);
    }
    id
}

/// `WriteFileHeader`.
pub fn header(version: i32) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    w.append_bytes(TAG);
    w.append_long(version);
    w.to_message()
}

/// `ReadFileHeader`: the version after the tag (0 for another tag or a short file).
pub fn read_header(r: &mut Reader) -> i32 {
    let mut read = || -> Result<i32, PastEnd> {
        if r.read_bytes(6)? != TAG {
            return Ok(0);
        }
        r.read_long()
    };
    read().unwrap_or(0)
}

/// `PruneUsers` over the saved users in the map's order (`(id, last use time)`): nothing while there are at most
/// 50; else every user without an id goes, then the least recently used (equal times in map order) until 50 are
/// left. Each answer is a user's index and whether its file is deleted (the aged ones).
pub fn prune_users(users: &[(i32, i64)]) -> Vec<(usize, bool)> {
    if users.len() <= MAX_USERS {
        return Vec::new();
    }
    let mut out: Vec<(usize, bool)> = Vec::new();
    let mut aged: Vec<(i64, usize)> = Vec::new();
    for (i, &(id, time)) in users.iter().enumerate() {
        if id == 0 {
            out.push((i, false));
        } else {
            aged.push((time, i));
        }
    }
    // a multimap keyed by time: equal keys keep their insertion (map) order, which a stable sort keeps too
    aged.sort_by_key(|&(time, _)| time);
    let mut left = users.len() - out.len();
    for (_, i) in aged {
        if left <= MAX_USERS {
            break;
        }
        out.push((i, true));
        left -= 1;
    }
    out
}

/// `PruneLanNames` over the LAN names' times in the map's order: the indexes dropped (the least recently used,
/// equal times in map order) to keep 50.
pub fn prune_lan_names(times: &[i64]) -> Vec<usize> {
    if times.len() <= MAX_LAN_NAMES {
        return Vec::new();
    }
    let mut aged: Vec<(i64, usize)> = times.iter().enumerate().map(|(i, &t)| (t, i)).collect();
    aged.sort_by_key(|&(time, _)| time);
    aged.into_iter().take(times.len() - MAX_LAN_NAMES).map(|(_, i)| i).collect()
}

/// `SelectRecentLanName` over the times in the map's order: the index of the name made current (none: the
/// default name). A name is taken while none is (time 0 counts as none) or when its time is later.
pub fn recent_lan_name(times: &[i64]) -> Option<usize> {
    let (mut chosen, mut last) = (None, 0i64);
    for (i, &t) in times.iter().enumerate() {
        if last == 0 || t > last {
            chosen = Some(i);
            last = t;
        }
    }
    chosen
}

/// `GetDefaultLanName`'s number: the CRC-16 of the local address (`GetHost`, its four bytes little-endian) modulo
/// 10000.
pub fn default_lan_number(host: u32) -> u16 {
    crate::crc16::put(0, &host.to_le_bytes()) % 10000
}

/// A saved user of the global file (`LobbyUserInfo`; the password encrypted).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct SavedUser {
    pub name: Vec<u16>,
    pub last_use: i32,
    pub id: u16,
    pub value04: i32,
    pub value08: i32,
    pub password: Vec<u8>,
}

/// The global file's contents.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct GlobalPrefs {
    pub users: Vec<SavedUser>,
    pub sound_effects: bool,
    pub music: bool,
    pub network_adapter: i32,
}

/// `SaveGlobalPrefs`' file (the counts' low 16 bits).
pub fn write_global(p: &GlobalPrefs) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    w.append_bytes(&header(GLOBAL_VERSION));
    w.append_short(p.users.len() as i16);
    for u in &p.users {
        w.append_wstring(&u.name, 2);
        w.append_long(u.last_use);
        w.append_short(u.id as i16);
        w.append_long(u.value04);
        w.append_long(u.value08);
        w.append_short(u.password.len() as i16);
        w.append_bytes(&u.password);
    }
    w.append_byte(u8::from(p.sound_effects));
    w.append_byte(u8::from(p.music));
    w.append_long(p.network_adapter);
    w.to_message()
}

/// A game filter of the user file: its name and values.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct GameOption {
    pub name: Vec<u8>,
    pub values: Vec<Vec<u16>>,
}

/// The user file's settings in their order: six bytes, three strings, five bytes, the connection speed.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Settings {
    /// Filter dirty words, auto away, cross promotion, auto ping, anonymous to friends, show games in progress.
    pub first: [bool; 6],
    /// The tab, the last server, the last room.
    pub texts: [Vec<u16>; 3],
    /// Lobby sounds, music, chat sounds, staging sounds, confirmations.
    pub second: [bool; 5],
    pub connection_speed: u16,
}

/// The user file's contents (`lan` only for the LAN user).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct UserPrefs {
    pub lan: Option<LanPrefs>,
    pub reconnects: Vec<([u8; 6], i32, i32)>,
    pub ignored: Vec<Vec<u16>>,
    pub friends: Vec<(Vec<u16>, u32)>,
    pub blocked: Vec<u32>,
    pub settings: Settings,
    pub options: Vec<GameOption>,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct LanPrefs {
    pub names: Vec<(Vec<u16>, i32)>,
    pub direct: Vec<([u8; 6], Vec<u16>)>,
    pub broadcast_port: u16,
}

/// `SaveUserPrefs`' file.
pub fn write_user(p: &UserPrefs) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    w.append_bytes(&header(USER_VERSION));
    if let Some(lan) = &p.lan {
        w.append_short(lan.names.len() as i16);
        for (name, time) in &lan.names {
            w.append_wstring(name, 2);
            w.append_long(*time);
        }
        w.append_short(lan.direct.len() as i16);
        for (address, name) in &lan.direct {
            w.append_bytes(address);
            w.append_wstring(name, 2);
        }
        w.append_short(lan.broadcast_port as i16);
    }
    w.append_short(p.reconnects.len() as i16);
    for (address, time, id) in &p.reconnects {
        w.append_bytes(address);
        w.append_long(*time);
        w.append_long(*id);
    }
    w.append_short(p.ignored.len() as i16);
    for name in &p.ignored {
        w.append_wstring(name, 2);
    }
    w.append_short(p.friends.len() as i16);
    for (name, id) in &p.friends {
        w.append_wstring(name, 2);
        w.append_long(*id as i32);
    }
    w.append_short(p.blocked.len() as i16);
    for id in &p.blocked {
        w.append_long(*id as i32);
    }
    let s = &p.settings;
    for b in s.first {
        w.append_byte(u8::from(b));
    }
    for t in &s.texts {
        w.append_wstring(t, 2);
    }
    for b in s.second {
        w.append_byte(u8::from(b));
    }
    w.append_short(s.connection_speed as i16);
    w.append_short(p.options.len() as i16);
    for o in &p.options {
        w.append_string(&o.name, 2);
        if !o.values.is_empty() {
            w.append_short(o.values.len() as i16);
            for v in &o.values {
                w.append_wstring(v, 2);
            }
        }
    }
    w.to_message()
}

/// What a read gives, in the original's order of assignments. Texts are UTF-16 units (`Text`) or bytes (`Bytes`).
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Event {
    // the global file
    /// A saved user from its name on (inserted unless the name is there; its fields follow).
    User(Vec<u16>),
    UserTime(i32),
    UserId(u16),
    UserValue04(i32),
    UserValue08(i32),
    /// The encrypted password's bytes; the user's id joins the id set after it.
    UserPassword(Vec<u8>),
    SoundEffects(bool),
    Music(bool),
    NetworkAdapter(i32),
    // the user file
    LanName(Vec<u16>, i32),
    /// The LAN names were read whole (the original then selected the most recent).
    LanNamesDone,
    DirectConnect([u8; 6], Vec<u16>),
    BroadcastPort(u16),
    /// A reconnect entry from its address on (inserted; its time and id follow).
    Reconnect([u8; 6]),
    ReconnectTime(i32),
    ReconnectId(i32),
    Ignored(Vec<u16>),
    Friend(Vec<u16>, u32),
    Blocked(u32),
    /// The settings by their index: 0..6 the first bytes, 6..9 the texts, 9..14 the second bytes, 14 the speed.
    Flag(u8, bool),
    SettingText(u8, Vec<u16>),
    ConnectionSpeed(u16),
    /// The game filters' count was read (the defaults are no longer used).
    OptionsBegin,
    /// A game filter from its name on (created; its values follow).
    OptionName(Vec<u8>),
    OptionValue(Vec<u16>),
}

/// How a read ended: the whole file read, another version (or tag: nothing given), or cut (the events before the
/// cut given).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonPrefsRead {
    Whole = 0,
    OtherVersion = 1,
    Cut = 2,
}

/// `LoadGlobalPrefs`' read.
pub fn read_global(bytes: &[u8], mut event: impl FnMut(Event)) -> EeWonPrefsRead {
    let mut r = Reader::new(bytes);
    if read_header(&mut r) != GLOBAL_VERSION {
        return EeWonPrefsRead::OtherVersion;
    }
    let mut body = || -> Result<(), PastEnd> {
        let count = r.read_short()? as u16;
        for _ in 0..count {
            event(Event::User(r.read_wstring(2)?));
            event(Event::UserTime(r.read_long()?));
            event(Event::UserId(r.read_short()? as u16));
            event(Event::UserValue04(r.read_long()?));
            event(Event::UserValue08(r.read_long()?));
            let length = usize::from(r.read_short()? as u16);
            event(Event::UserPassword(r.read_bytes(length)?.to_vec()));
        }
        event(Event::SoundEffects(r.read_byte()? != 0));
        event(Event::Music(r.read_byte()? != 0));
        event(Event::NetworkAdapter(r.read_long()?));
        Ok(())
    };
    match body() {
        Ok(()) => EeWonPrefsRead::Whole,
        Err(PastEnd) => EeWonPrefsRead::Cut,
    }
}

fn six(r: &mut Reader) -> Result<[u8; 6], PastEnd> {
    let mut a = [0u8; 6];
    a.copy_from_slice(r.read_bytes(6)?);
    Ok(a)
}

/// `LoadUserPrefs`' read (`ReadLanPrefs` first for the LAN user).
pub fn read_user(bytes: &[u8], lan_user: bool, mut event: impl FnMut(Event)) -> EeWonPrefsRead {
    let mut r = Reader::new(bytes);
    if read_header(&mut r) != USER_VERSION {
        return EeWonPrefsRead::OtherVersion;
    }
    let mut body = || -> Result<(), PastEnd> {
        if lan_user {
            let count = r.read_short()? as u16;
            for _ in 0..count {
                let name = r.read_wstring(2)?;
                event(Event::LanName(name, r.read_long()?));
            }
            event(Event::LanNamesDone);
            let count = r.read_short()? as u16;
            for _ in 0..count {
                let address = six(&mut r)?;
                event(Event::DirectConnect(address, r.read_wstring(2)?));
            }
            event(Event::BroadcastPort(r.read_short()? as u16));
        }
        let count = r.read_short()? as u16;
        for _ in 0..count {
            event(Event::Reconnect(six(&mut r)?));
            event(Event::ReconnectTime(r.read_long()?));
            event(Event::ReconnectId(r.read_long()?));
        }
        let count = r.read_short()? as u16;
        for _ in 0..count {
            event(Event::Ignored(r.read_wstring(2)?));
        }
        let count = r.read_short()? as u16;
        for _ in 0..count {
            let name = r.read_wstring(2)?;
            event(Event::Friend(name, r.read_long()? as u32));
        }
        let count = r.read_short()? as u16;
        for _ in 0..count {
            event(Event::Blocked(r.read_long()? as u32));
        }
        for i in 0..6u8 {
            event(Event::Flag(i, r.read_byte()? == 1));
        }
        for i in 6..9u8 {
            event(Event::SettingText(i, r.read_wstring(2)?));
        }
        for i in 9..14u8 {
            event(Event::Flag(i, r.read_byte()? == 1));
        }
        event(Event::ConnectionSpeed(r.read_short()? as u16));
        let count = r.read_short()? as u16;
        event(Event::OptionsBegin);
        for _ in 0..count {
            event(Event::OptionName(r.read_string(2)?.to_vec()));
            let values = r.read_short()? as u16;
            for _ in 0..values {
                event(Event::OptionValue(r.read_wstring(2)?));
            }
        }
        Ok(())
    };
    match body() {
        Ok(()) => EeWonPrefsRead::Whole,
        Err(PastEnd) => EeWonPrefsRead::Cut,
    }
}

// ---- the C ABI (LobbyPersistentData.cpp)

/// A list of texts: `count` texts, text `i` the units (or bytes) from `offsets[i]` to `offsets[i + 1]`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonTextList {
    pub data: *const c_void,
    pub offsets: *const usize,
    pub count: usize,
}

unsafe fn units_of(l: &EeWonTextList) -> Vec<Vec<u16>> {
    (0..l.count)
        .map(|i| {
            // SAFETY: `count + 1` offsets into `data`'s units (the contract)
            unsafe {
                let (from, to) = (*l.offsets.add(i), *l.offsets.add(i + 1));
                std::slice::from_raw_parts((l.data as *const u16).add(from), to - from).to_vec()
            }
        })
        .collect()
}

unsafe fn bytes_of(l: &EeWonTextList) -> Vec<Vec<u8>> {
    (0..l.count)
        .map(|i| {
            // SAFETY: as `units_of`, in bytes
            unsafe {
                let (from, to) = (*l.offsets.add(i), *l.offsets.add(i + 1));
                std::slice::from_raw_parts((l.data as *const u8).add(from), to - from).to_vec()
            }
        })
        .collect()
}

unsafe fn array<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: `n` items (the contract)
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

unsafe fn six_at(p: *const u8, i: usize) -> [u8; 6] {
    let mut a = [0u8; 6];
    // SAFETY: six bytes per entry (the contract)
    a.copy_from_slice(unsafe { std::slice::from_raw_parts(p.add(i * 6), 6) });
    a
}

/// The global file's lists: `names.count` users with their fields at the same index; the passwords' bytes.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonGlobalPrefsOut {
    pub names: EeWonTextList,
    pub last_use: *const i32,
    pub ids: *const u16,
    pub value04: *const i32,
    pub value08: *const i32,
    pub passwords: EeWonTextList,
    pub sound_effects: bool,
    pub music: bool,
    pub network_adapter: i32,
}

/// `write_global` into the sink.
///
/// # Safety
/// The lists and arrays hold `names.count` entries; the sink's contract.
#[no_mangle]
pub unsafe extern "C" fn ee_won_prefs_write_global(
    p: *const EeWonGlobalPrefsOut,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    // SAFETY: valid (the contract)
    let p = unsafe { &*p };
    let n = p.names.count;
    let (names, passwords) = unsafe { (units_of(&p.names), bytes_of(&p.passwords)) };
    let (last_use, ids, v04, v08) =
        unsafe { (array(p.last_use, n), array(p.ids, n), array(p.value04, n), array(p.value08, n)) };
    let users = (0..n)
        .map(|i| SavedUser {
            name: names[i].clone(),
            last_use: last_use[i],
            id: ids[i],
            value04: v04[i],
            value08: v08[i],
            password: passwords[i].clone(),
        })
        .collect();
    let prefs =
        GlobalPrefs { users, sound_effects: p.sound_effects, music: p.music, network_adapter: p.network_adapter };
    unsafe { crate::ffi::give(sink, context, &write_global(&prefs)) }
}

/// The user file's lists (`lan_user` false: the LAN part is not written). Addresses are six bytes each; a game
/// filter `i` has the values from `option_first[i]` to `option_first[i + 1]` of `option_values`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonUserPrefsOut {
    pub lan_user: bool,
    pub lan_names: EeWonTextList,
    pub lan_times: *const i32,
    pub direct_addresses: *const u8,
    pub direct_names: EeWonTextList,
    pub broadcast_port: u16,
    pub reconnect_count: usize,
    pub reconnect_addresses: *const u8,
    pub reconnect_times: *const i32,
    pub reconnect_ids: *const i32,
    pub ignored: EeWonTextList,
    pub friend_names: EeWonTextList,
    pub friend_ids: *const u32,
    pub blocked_count: usize,
    pub blocked: *const u32,
    pub first: [bool; 6],
    /// The tab, the last server, the last room (three texts).
    pub texts: EeWonTextList,
    pub second: [bool; 5],
    pub connection_speed: u16,
    pub option_names: EeWonTextList,
    pub option_first: *const usize,
    pub option_values: EeWonTextList,
}

/// `write_user` into the sink.
///
/// # Safety
/// The lists and arrays hold their counts; the sink's contract.
#[no_mangle]
pub unsafe extern "C" fn ee_won_prefs_write_user(
    p: *const EeWonUserPrefsOut,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    // SAFETY: valid (the contract)
    let p = unsafe { &*p };
    let lan = if p.lan_user {
        let names = unsafe { units_of(&p.lan_names) };
        let times = unsafe { array(p.lan_times, names.len()) };
        let direct_names = unsafe { units_of(&p.direct_names) };
        Some(LanPrefs {
            names: names.into_iter().zip(times.iter().copied()).collect(),
            direct: direct_names
                .into_iter()
                .enumerate()
                .map(|(i, n)| (unsafe { six_at(p.direct_addresses, i) }, n))
                .collect(),
            broadcast_port: p.broadcast_port,
        })
    } else {
        None
    };
    let n = p.reconnect_count;
    let (times, ids) = unsafe { (array(p.reconnect_times, n), array(p.reconnect_ids, n)) };
    let reconnects = (0..n).map(|i| (unsafe { six_at(p.reconnect_addresses, i) }, times[i], ids[i])).collect();
    let friend_names = unsafe { units_of(&p.friend_names) };
    let friend_ids = unsafe { array(p.friend_ids, friend_names.len()) };
    let texts = unsafe { units_of(&p.texts) };
    let option_names = unsafe { bytes_of(&p.option_names) };
    let values = unsafe { units_of(&p.option_values) };
    let first = unsafe { array(p.option_first, option_names.len() + 1) };
    let options = option_names
        .into_iter()
        .enumerate()
        .map(|(i, name)| GameOption { name, values: values[first[i]..first[i + 1]].to_vec() })
        .collect();
    let prefs = UserPrefs {
        lan,
        reconnects,
        ignored: unsafe { units_of(&p.ignored) },
        friends: friend_names.into_iter().zip(friend_ids.iter().copied()).collect(),
        blocked: unsafe { array(p.blocked, p.blocked_count) }.to_vec(),
        settings: Settings {
            first: p.first,
            texts: [texts[0].clone(), texts[1].clone(), texts[2].clone()],
            second: p.second,
            connection_speed: p.connection_speed,
        },
        options,
    };
    unsafe { crate::ffi::give(sink, context, &write_user(&prefs)) }
}

/// A read's event for the C++: `kind`, a number (a time, an id, a flag, a port, a setting's index for a text or
/// flag in `index`), the text (UTF-16 units, `units`/`count`) or bytes (`bytes`/`length`), an address (six bytes).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonPrefsEventKind {
    User,
    UserTime,
    UserId,
    UserValue04,
    UserValue08,
    UserPassword,
    SoundEffects,
    Music,
    NetworkAdapter,
    LanName,
    LanNamesDone,
    DirectConnect,
    BroadcastPort,
    Reconnect,
    ReconnectTime,
    ReconnectId,
    Ignored,
    Friend,
    Blocked,
    Flag,
    SettingText,
    ConnectionSpeed,
    OptionsBegin,
    OptionName,
    OptionValue,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonPrefsEvent {
    pub kind: EeWonPrefsEventKind,
    pub value: i64,
    pub index: u8,
    pub units: *const u16,
    pub count: usize,
    pub bytes: *const u8,
    pub length: usize,
    pub address: [u8; 6],
}

/// Receives each event of a read in order.
pub type EeWonPrefsSink = unsafe extern "C" fn(context: *mut c_void, event: *const EeWonPrefsEvent);

unsafe fn deliver(sink: EeWonPrefsSink, context: *mut c_void, e: Event) {
    use EeWonPrefsEventKind as K;
    let mut out = EeWonPrefsEvent {
        kind: K::User,
        value: 0,
        index: 0,
        units: std::ptr::null(),
        count: 0,
        bytes: std::ptr::null(),
        length: 0,
        address: [0; 6],
    };
    let (mut text, mut data): (Vec<u16>, Vec<u8>) = (Vec::new(), Vec::new());
    match e {
        Event::User(t) => (out.kind, text) = (K::User, t),
        Event::UserTime(v) => (out.kind, out.value) = (K::UserTime, i64::from(v)),
        Event::UserId(v) => (out.kind, out.value) = (K::UserId, i64::from(v)),
        Event::UserValue04(v) => (out.kind, out.value) = (K::UserValue04, i64::from(v)),
        Event::UserValue08(v) => (out.kind, out.value) = (K::UserValue08, i64::from(v)),
        Event::UserPassword(b) => (out.kind, data) = (K::UserPassword, b),
        Event::SoundEffects(v) => (out.kind, out.value) = (K::SoundEffects, i64::from(v)),
        Event::Music(v) => (out.kind, out.value) = (K::Music, i64::from(v)),
        Event::NetworkAdapter(v) => (out.kind, out.value) = (K::NetworkAdapter, i64::from(v)),
        Event::LanName(t, v) => (out.kind, text, out.value) = (K::LanName, t, i64::from(v)),
        Event::LanNamesDone => out.kind = K::LanNamesDone,
        Event::DirectConnect(a, t) => (out.kind, out.address, text) = (K::DirectConnect, a, t),
        Event::BroadcastPort(v) => (out.kind, out.value) = (K::BroadcastPort, i64::from(v)),
        Event::Reconnect(a) => (out.kind, out.address) = (K::Reconnect, a),
        Event::ReconnectTime(v) => (out.kind, out.value) = (K::ReconnectTime, i64::from(v)),
        Event::ReconnectId(v) => (out.kind, out.value) = (K::ReconnectId, i64::from(v)),
        Event::Ignored(t) => (out.kind, text) = (K::Ignored, t),
        Event::Friend(t, v) => (out.kind, text, out.value) = (K::Friend, t, i64::from(v)),
        Event::Blocked(v) => (out.kind, out.value) = (K::Blocked, i64::from(v)),
        Event::Flag(i, v) => (out.kind, out.index, out.value) = (K::Flag, i, i64::from(v)),
        Event::SettingText(i, t) => (out.kind, out.index, text) = (K::SettingText, i, t),
        Event::ConnectionSpeed(v) => (out.kind, out.value) = (K::ConnectionSpeed, i64::from(v)),
        Event::OptionsBegin => out.kind = K::OptionsBegin,
        Event::OptionName(b) => (out.kind, data) = (K::OptionName, b),
        Event::OptionValue(t) => (out.kind, text) = (K::OptionValue, t),
    }
    out.units = text.as_ptr();
    out.count = text.len();
    out.bytes = data.as_ptr();
    out.length = data.len();
    // SAFETY: the sink's contract; `text` and `data` live through the call
    unsafe { sink(context, &out) }
}

/// `read_global` (the file's bytes) to the sink.
///
/// # Safety
/// `bytes` holds `length` bytes; the sink's contract. No reference into Rust is kept past each call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_prefs_read_global(
    bytes: *const u8,
    length: usize,
    sink: EeWonPrefsSink,
    context: *mut c_void,
) -> EeWonPrefsRead {
    let data = unsafe { array(bytes, length) }.to_vec();
    read_global(&data, |e| unsafe { deliver(sink, context, e) })
}

/// `read_user` to the sink.
///
/// # Safety
/// As `ee_won_prefs_read_global`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_prefs_read_user(
    bytes: *const u8,
    length: usize,
    lan_user: bool,
    sink: EeWonPrefsSink,
    context: *mut c_void,
) -> EeWonPrefsRead {
    let data = unsafe { array(bytes, length) }.to_vec();
    read_user(&data, lan_user, |e| unsafe { deliver(sink, context, e) })
}

/// `user_file_name` into the sink.
///
/// # Safety
/// The sink's contract.
#[no_mangle]
pub unsafe extern "C" fn ee_won_prefs_user_file_name(id: i32, sink: EeWonByteSink, context: *mut c_void) {
    unsafe { crate::ffi::give(sink, context, user_file_name(id).as_bytes()) }
}

/// `find_unused_user_id`.
///
/// # Safety
/// `ids` holds `count` ids in ascending order.
#[no_mangle]
pub unsafe extern "C" fn ee_won_prefs_unused_user_id(ids: *const i32, count: usize) -> i32 {
    find_unused_user_id(unsafe { array(ids, count) })
}

/// `prune_users`: `drop[i]` set to 1 for a user dropped without its file, 2 with it, else 0.
///
/// # Safety
/// `ids`, `times` and `drop` hold `count` entries.
#[no_mangle]
pub unsafe extern "C" fn ee_won_prefs_prune_users(ids: *const i32, times: *const i64, count: usize, drop: *mut u8) {
    let (ids, times) = unsafe { (array(ids, count), array(times, count)) };
    let users: Vec<(i32, i64)> = ids.iter().copied().zip(times.iter().copied()).collect();
    for i in 0..count {
        // SAFETY: `count` writable entries
        unsafe { *drop.add(i) = 0 };
    }
    for (i, file) in prune_users(&users) {
        unsafe { *drop.add(i) = if file { 2 } else { 1 } };
    }
}

/// `prune_lan_names`: `drop[i]` set for each name dropped.
///
/// # Safety
/// `times` and `drop` hold `count` entries.
#[no_mangle]
pub unsafe extern "C" fn ee_won_prefs_prune_lan_names(times: *const i64, count: usize, drop: *mut bool) {
    let times = unsafe { array(times, count) };
    for i in 0..count {
        // SAFETY: `count` writable entries
        unsafe { *drop.add(i) = false };
    }
    for i in prune_lan_names(times) {
        unsafe { *drop.add(i) = true };
    }
}

/// `recent_lan_name`, -1 for none.
///
/// # Safety
/// `times` holds `count` entries.
#[no_mangle]
pub unsafe extern "C" fn ee_won_prefs_recent_lan_name(times: *const i64, count: usize) -> i64 {
    recent_lan_name(unsafe { array(times, count) }).map_or(-1, |i| i as i64)
}

#[no_mangle]
pub extern "C" fn ee_won_prefs_default_lan_number(host: u32) -> u16 {
    default_lan_number(host)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn u(s: &str) -> Vec<u16> {
        s.encode_utf16().collect()
    }

    fn events_global(b: &[u8]) -> (EeWonPrefsRead, Vec<Event>) {
        let mut v = Vec::new();
        let r = read_global(b, |e| v.push(e));
        (r, v)
    }

    fn events_user(b: &[u8], lan: bool) -> (EeWonPrefsRead, Vec<Event>) {
        let mut v = Vec::new();
        let r = read_user(b, lan, |e| v.push(e));
        (r, v)
    }

    fn sample_user(lan: bool) -> UserPrefs {
        UserPrefs {
            lan: lan.then(|| LanPrefs {
                names: vec![(u("Bob"), 100), (u("\u{e9}ve"), 200)],
                direct: vec![([1, 2, 3, 4, 5, 6], u("x"))],
                broadcast_port: 8585,
            }),
            reconnects: vec![([9, 9, 1, 2, 3, 4], 5, -6)],
            ignored: vec![u("troll")],
            friends: vec![(u("pal"), 0xdeadbeef)],
            blocked: vec![7, 8],
            settings: Settings {
                first: [true, false, true, true, false, true],
                texts: [u("LAN"), u(""), u("room")],
                second: [true, true, false, true, true],
                connection_speed: 25,
            },
            options: vec![
                GameOption { name: b"Epoch".to_vec(), values: vec![u("Stone"), u("Atomic")] },
                GameOption { name: b"Map\xc3\xa9".to_vec(), values: vec![u("Large")] },
            ],
        }
    }

    #[test]
    fn names_ids_and_header() {
        assert_eq!(user_file_name(0), "_wonuser0.dat");
        assert_eq!(user_file_name(-3), "_wonuser-3.dat");
        assert_eq!(find_unused_user_id(&[]), 1);
        assert_eq!(find_unused_user_id(&[1, 2, 3]), 4);
        assert_eq!(find_unused_user_id(&[1, 2, 4]), 3);
        assert_eq!(find_unused_user_id(&[5, 6]), 7);
        assert_eq!(header(4), b"WONPER\x04\0\0\0");
        let mut r = Reader::new(b"WONPEX\x04\0\0\0");
        assert_eq!(read_header(&mut r), 0);
        let mut r = Reader::new(b"WONPER\x04\0");
        assert_eq!(read_header(&mut r), 0);
    }

    #[test]
    fn pruning_and_the_recent_name() {
        let mut users: Vec<(i32, i64)> = (0..52).map(|i| (i + 1, 1000 - i64::from(i))).collect();
        assert!(prune_users(&users[..50]).is_empty());
        // the two oldest go with their files
        assert_eq!(prune_users(&users), vec![(51, true), (50, true)]);
        // users without an id go first (all of them), then age only as far as needed
        users[3].0 = 0;
        users[10].0 = 0;
        users[11].0 = 0;
        assert_eq!(prune_users(&users), vec![(3, false), (10, false), (11, false)]);
        // equal times: map order
        let same: Vec<(i32, i64)> = (0..51).map(|i| (i + 1, 5)).collect();
        assert_eq!(prune_users(&same), vec![(0, true)]);
        let times: Vec<i64> = (0..53).map(|i| 100 - i).collect();
        assert_eq!(prune_lan_names(&times), vec![52, 51, 50]);
        assert!(prune_lan_names(&times[..50]).is_empty());
        assert_eq!(recent_lan_name(&[]), None);
        assert_eq!(recent_lan_name(&[3, 9, 4]), Some(1));
        // a time of 0 means none chosen yet: the next name is taken whatever its time
        assert_eq!(recent_lan_name(&[0, -5]), Some(1));
        assert_eq!(recent_lan_name(&[7, 7]), Some(0));
        assert_eq!(default_lan_number(0x0a000001), crate::crc16::put(0, &[1, 0, 0, 10]) % 10000);
    }

    #[test]
    fn the_global_file_round_trips() {
        let p = GlobalPrefs {
            users: vec![
                SavedUser {
                    name: u("max"),
                    last_use: 77,
                    id: 2,
                    value04: -1,
                    value08: 9,
                    password: vec![1, 2, 3, 4, 5, 6, 7, 8],
                },
                SavedUser { name: u(""), last_use: 0, id: 0xffff, value04: 0, value08: 0, password: vec![] },
            ],
            sound_effects: true,
            music: false,
            network_adapter: 0x0a000001,
        };
        let b = write_global(&p);
        assert_eq!(&b[..12], b"WONPER\x02\0\0\0\x02\0");
        let (r, ev) = events_global(&b);
        assert_eq!(r, EeWonPrefsRead::Whole);
        assert_eq!(ev.len(), 6 * 2 + 3);
        assert_eq!(ev[0], Event::User(u("max")));
        assert_eq!(ev[5], Event::UserPassword(vec![1, 2, 3, 4, 5, 6, 7, 8]));
        assert_eq!(ev[8], Event::UserId(0xffff));
        assert_eq!(ev[14], Event::NetworkAdapter(0x0a000001));
        // the version must match; a cut keeps the events before it
        let mut other = b.clone();
        other[6] = 3;
        assert_eq!(events_global(&other), (EeWonPrefsRead::OtherVersion, vec![]));
        let (r, ev) = events_global(&b[..b.len() - 1]);
        assert_eq!((r, ev.len()), (EeWonPrefsRead::Cut, 14));
        let (r, ev) = events_global(&b[..12 + 2 + 6 + 4]);
        assert_eq!((r, ev), (EeWonPrefsRead::Cut, vec![Event::User(u("max")), Event::UserTime(77)]));
    }

    #[test]
    fn the_user_file_round_trips() {
        for lan in [false, true] {
            let p = sample_user(lan);
            let b = write_user(&p);
            let (r, ev) = events_user(&b, lan);
            assert_eq!(r, EeWonPrefsRead::Whole);
            // rebuild the prefs from the events
            let mut q = UserPrefs { lan: lan.then(LanPrefs::default), ..UserPrefs::default() };
            for e in ev {
                match e {
                    Event::LanName(n, t) => q.lan.as_mut().unwrap().names.push((n, t)),
                    Event::DirectConnect(a, n) => q.lan.as_mut().unwrap().direct.push((a, n)),
                    Event::BroadcastPort(v) => q.lan.as_mut().unwrap().broadcast_port = v,
                    Event::Reconnect(a) => q.reconnects.push((a, 0, 0)),
                    Event::ReconnectTime(t) => q.reconnects.last_mut().unwrap().1 = t,
                    Event::ReconnectId(t) => q.reconnects.last_mut().unwrap().2 = t,
                    Event::Ignored(n) => q.ignored.push(n),
                    Event::Friend(n, i) => q.friends.push((n, i)),
                    Event::Blocked(i) => q.blocked.push(i),
                    Event::Flag(i, v) if i < 6 => q.settings.first[usize::from(i)] = v,
                    Event::Flag(i, v) => q.settings.second[usize::from(i) - 9] = v,
                    Event::SettingText(i, t) => q.settings.texts[usize::from(i) - 6] = t,
                    Event::ConnectionSpeed(v) => q.settings.connection_speed = v,
                    Event::OptionName(n) => q.options.push(GameOption { name: n, values: vec![] }),
                    Event::OptionValue(v) => q.options.last_mut().unwrap().values.push(v),
                    Event::LanNamesDone | Event::OptionsBegin => {}
                    other => panic!("{other:?}"),
                }
            }
            assert_eq!(q, p);
        }
    }

    #[test]
    fn odd_user_files() {
        // a setting byte is on only when it is 1
        let mut p = sample_user(false);
        p.options.clear();
        let mut b = write_user(&p);
        let flags_at = b.len() - 2 - 2 - 5 - (2 + 8) - 2 - (2 + 6) - 6;
        b[flags_at] = 2;
        let (_, ev) = events_user(&b, false);
        assert!(ev.contains(&Event::Flag(0, false)));
        // a filter without values has no count: the next filter's name is read as the count
        let mut p = sample_user(false);
        p.options[0].values.clear();
        let b = write_user(&p);
        let (r, ev) = events_user(&b, false);
        assert_eq!(r, EeWonPrefsRead::Cut);
        let names: Vec<&Event> = ev.iter().filter(|e| matches!(e, Event::OptionName(_))).collect();
        assert_eq!(names.len(), 1);
        // cut inside the LAN names: no LanNamesDone
        let b = write_user(&sample_user(true));
        let (r, ev) = events_user(&b[..10 + 2 + 2 + 6 + 4 + 2], true);
        assert_eq!(r, EeWonPrefsRead::Cut);
        assert_eq!(ev, vec![Event::LanName(u("Bob"), 100)]);
        // a reconnect entry exists from its address on
        let b = write_user(&sample_user(false));
        let (_, ev) = events_user(&b[..10 + 2 + 6 + 4], false);
        assert_eq!(ev, vec![Event::Reconnect([9, 9, 1, 2, 3, 4]), Event::ReconnectTime(5)]);
        // the options' count begins them even when nothing follows
        let mut p = sample_user(false);
        p.options.clear();
        let (r, ev) = events_user(&write_user(&p), false);
        assert_eq!((r, ev.last()), (EeWonPrefsRead::Whole, Some(&Event::OptionsBegin)));
        assert_eq!(events_user(b"WONPER\x03\0\0\0", false), (EeWonPrefsRead::OtherVersion, vec![]));
    }
}
