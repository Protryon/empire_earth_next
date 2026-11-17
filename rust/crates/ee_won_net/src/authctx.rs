//! `AuthContext`'s data logic (WONAuth/AuthContext.cpp): the login secrets (the 8-byte keys the auth server's
//! login hands out, kept across runs in `_wonlogin.ks`: their file, the order, the commit rule that dates them,
//! drops the expired and caps the list) and the login request's two blocks, the community data (the communities,
//! their CD keys, the CD-key joins, the user data, the secrets and the nicknames as typed records) and the hashes
//! (each hashed community's file hash and its hash keyed with the server's challenge). The C++ `AuthContext` keeps
//! its lock, maps, sessions, certificate ops and timers; the secret list is a Rust object it owns
//! (`AuthContext::mSecrets`).
//!
//! Reached only by the internet login (the servers are gone) and by direct-connect games with the CD-key check (peer
//! auth reads the certificate, not these).

use std::ffi::{c_char, c_ulong, c_void};

use crate::ffi::EeWonByteSink;
use crate::md5::Md5;
use crate::text;
use crate::wire::Writer;

/// One login secret: its expiry (0 until the login that sent it is committed) and its 8 bytes.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Secret {
    pub expires: i64,
    pub key: [u8; 8],
}

/// The login secrets, newest first.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Secrets {
    pub list: Vec<Secret>,
}

/// The file's records as the original's stdio loop read them: a count byte, then while the stream has not hit its
/// end and fewer records than the count were read, a 4-byte expiry and the 8 bytes (a record cut by the end keeps
/// what was read, zeros after it; one more record follows a file that ends exactly after the last, as the end flag
/// is set only by a read that comes short).
pub fn parse_secret_file(data: &[u8]) -> Vec<Secret> {
    let mut pos = 0usize;
    let mut eof = false;
    let read = |n: usize, eof: &mut bool, pos: &mut usize| -> Vec<u8> {
        let take = n.min(data.len() - *pos);
        let out = data[*pos..*pos + take].to_vec();
        *pos += take;
        if take < n {
            *eof = true;
        }
        out
    };
    // fgetc: EOF (-1) becomes a count of 255
    let count = if data.is_empty() {
        eof = true;
        255usize
    } else {
        pos = 1;
        data[0] as usize
    };
    let mut out = Vec::new();
    let mut i = 0;
    while !eof && i < count {
        let mut expires = [0u8; 4];
        let e = read(4, &mut eof, &mut pos);
        expires[..e.len()].copy_from_slice(&e);
        let mut key = [0u8; 8];
        let k = read(8, &mut eof, &mut pos);
        key[..k.len()].copy_from_slice(&k);
        out.push(Secret { expires: i32::from_le_bytes(expires) as i64, key });
        i += 1;
    }
    out
}

impl Secrets {
    /// `SaveSecretList`'s bytes: the count (its low byte), then per secret the expiry in 32 bits and the key.
    pub fn file_bytes(&self) -> Vec<u8> {
        let mut out = vec![self.list.len() as u8];
        for s in &self.list {
            out.extend_from_slice(&(s.expires as i32).to_le_bytes());
            out.extend_from_slice(&s.key);
        }
        out
    }

    /// `AddSecret`: a new uncommitted secret in front.
    pub fn add(&mut self, key: [u8; 8]) {
        self.list.insert(0, Secret { expires: 0, key });
    }

    /// `RemoveSecret`: the first uncommitted secret with these bytes; whether one was removed.
    pub fn remove(&mut self, key: &[u8; 8]) -> bool {
        match self.list.iter().position(|s| s.expires == 0 && &s.key == key) {
            Some(i) => {
                self.list.remove(i);
                true
            }
            None => false,
        }
    }

    /// `CommitSecrets`: the uncommitted secrets expire at `auth_time + duration + 120` (summed in an `unsigned
    /// long`, as the original), the secrets expired by `auth_time` go, and the oldest go until fewer than 256 are
    /// left. False for an empty list (nothing is saved then).
    pub fn commit(&mut self, auth_time: i64, duration: i64) -> bool {
        if self.list.is_empty() {
            return false;
        }
        let expires = (auth_time as c_ulong).wrapping_add(duration as c_ulong).wrapping_add(120) as i64;
        self.list.retain_mut(|s| {
            if s.expires == 0 {
                s.expires = expires;
                true
            } else {
                s.expires > auth_time
            }
        });
        self.list.truncate(255);
        true
    }

    /// `AppendLoginSecrets`: a count byte (wrapping), then every secret's bytes.
    pub fn login_bytes(&self) -> Vec<u8> {
        let mut out = vec![self.list.len() as u8];
        for s in &self.list {
            out.extend_from_slice(&s.key);
        }
        out
    }
}

/// A community in the login's maps, in the maps' order. `cd_key` is the community's valid CD key's bytes.
#[derive(Clone, Debug, Default)]
pub struct Community {
    pub name: Vec<u16>,
    pub cd_key: Option<Vec<u8>>,
    pub simple_hash: Vec<u8>,
    pub keyed_hash_data: Vec<u8>,
}

/// The community data block's inputs, each list in its map's order.
#[derive(Clone, Debug, Default)]
pub struct CommunityData {
    pub communities: Vec<Community>,
    /// the CD-key joins: community, key bytes (a join without a key is left out)
    pub joins: Vec<(Vec<u16>, Option<Vec<u8>>)>,
    /// the user data to set: community, data (left out when NULL)
    pub user_data: Vec<(Vec<u16>, Option<Vec<u8>>)>,
    /// the nicknames: key, value
    pub nicknames: Vec<(Vec<u16>, Vec<u16>)>,
}

fn set_short(w: &mut Writer, at: usize, v: i16) {
    w.set_bytes(at, &v.to_le_bytes());
}

/// `AppendCommunityData`: a 0 byte, the community count (its low byte) and names, then a record count and the
/// records: 1 a community's CD key, 7 a CD-key join, 8 user data, 6 the login secrets, 4 a nickname key and 3 a
/// nickname with its value (each a type byte, a 16-bit length and the fields; lengths as the original's `short`).
pub fn community_data(d: &CommunityData, secrets: &Secrets) -> Vec<u8> {
    let mut w = Writer::new(4, false);
    w.append_byte(0);
    w.append_byte(d.communities.len() as u8);
    for c in &d.communities {
        w.append_wstring(&c.name, 2);
    }
    let count_at = w.data().len();
    w.skip_bytes(2);
    let mut count: i32 = 0;
    for c in &d.communities {
        if let Some(key) = &c.cd_key {
            w.append_byte(1);
            w.append_short((c.name.len() * 2 + key.len() + 2) as i16);
            w.append_wstring(&c.name, 2);
            w.append_bytes(key);
            count += 1;
        }
    }
    for (name, key) in &d.joins {
        if let Some(key) = key {
            w.append_byte(7);
            w.append_short((name.len() * 2 + 6 + key.len()) as i16);
            w.append_wstring(name, 2);
            w.append_long(0);
            w.append_bytes(key);
            count += 1;
        }
    }
    for (name, data) in &d.user_data {
        if let Some(data) = data {
            w.append_byte(8);
            w.append_short((name.len() * 2 + 6 + data.len()) as i16);
            w.append_wstring(name, 2);
            w.append_long(0);
            w.append_bytes(data);
            count += 1;
        }
    }
    if !secrets.list.is_empty() {
        w.append_byte(6);
        let at = w.data().len();
        w.skip_bytes(2);
        w.append_bytes(&secrets.login_bytes());
        let len = (w.data().len() - at - 2) as i16;
        set_short(&mut w, at, len);
        count += 1;
    }
    for (key, value) in &d.nicknames {
        w.append_byte(4);
        let at = w.data().len();
        w.skip_bytes(2);
        w.append_wstring(key, 2);
        let len = (w.data().len() - at - 2) as i16;
        set_short(&mut w, at, len);
        count += 1;
        if !value.is_empty() {
            w.append_byte(3);
            let at = w.data().len();
            w.skip_bytes(2);
            w.append_wstring(key, 2);
            w.append_wstring(value, 2);
            let len = (w.data().len() - at - 2) as i16;
            set_short(&mut w, at, len);
            count += 1;
        }
    }
    set_short(&mut w, count_at, count as i16);
    w.data().to_vec()
}

/// `AppendHashes`: a count byte (its low byte), then per community with a file hash: 1, the name, the hash and
/// MD5(challenge, the per-section hashes).
pub fn hashes(communities: &[Community], challenge: &[u8]) -> Vec<u8> {
    let mut w = Writer::new(4, false);
    w.skip_bytes(1);
    let mut count: i32 = 0;
    for c in communities {
        if !c.simple_hash.is_empty() {
            let mut keyed = Md5::new();
            keyed.update(challenge);
            keyed.update(&c.keyed_hash_data);
            w.append_byte(1);
            w.append_wstring(&c.name, 2);
            w.append_bytes(&c.simple_hash);
            w.append_bytes(&keyed.finish());
            count += 1;
        }
    }
    w.set_bytes(0, &[count as u8]);
    w.data().to_vec()
}

/// `StartAutoRefresh`'s timer: `None` when no timer starts, else the wait in seconds: two minutes before a valid
/// certificate expires (at least two minutes), or five minutes when forced without one.
pub fn auto_refresh_wait(enabled: bool, force: bool, peer_valid: bool, expire_delta: i64) -> Option<i64> {
    if !enabled {
        return None;
    }
    if peer_valid {
        return Some((expire_delta - 120).max(120));
    }
    force.then_some(300)
}

// ---- the C ABI (WONAuth/AuthContext.cpp)

/// cbindgen:ignore
mod stdio {
    use std::ffi::{c_char, c_int, c_void};
    extern "C" {
        pub fn PlatFOpen(path: *const c_char, mode: *const c_char) -> *mut c_void;
        pub fn fclose(file: *mut c_void) -> c_int;
        pub fn fread(data: *mut c_void, size: usize, count: usize, file: *mut c_void) -> usize;
        pub fn fwrite(data: *const c_void, size: usize, count: usize, file: *mut c_void) -> usize;
    }
}

/// The secret list (opaque): `AuthContext::mSecrets`.
pub struct EeWonAuthSecrets(Secrets);

#[no_mangle]
pub extern "C" fn ee_won_auth_secrets_new() -> *mut EeWonAuthSecrets {
    Box::into_raw(Box::new(EeWonAuthSecrets(Secrets::default())))
}

/// # Safety
/// `s` is from `ee_won_auth_secrets_new` (or NULL), not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_secrets_free(s: *mut EeWonAuthSecrets) {
    if !s.is_null() {
        drop(unsafe { Box::from_raw(s) });
    }
}

unsafe fn secrets<'a>(s: *mut EeWonAuthSecrets) -> &'a mut Secrets {
    // SAFETY: a list from ee_won_auth_secrets_new (contract)
    unsafe { &mut (*s).0 }
}

/// `LoadSecretList`: the list emptied, then read from the file; false when it cannot open.
///
/// # Safety
/// `s` is a list, `path` a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_secrets_load(s: *mut EeWonAuthSecrets, path: *const c_char) -> bool {
    unsafe { load_secrets(secrets(s), path) }
}

/// `LoadSecretList` into a list (authlogin.rs's login holds one).
///
/// # Safety
/// `path` is a C string.
pub(crate) unsafe fn load_secrets(list: &mut Secrets, path: *const c_char) -> bool {
    list.list.clear();
    let file = unsafe { stdio::PlatFOpen(path, c"rb".as_ptr()) };
    if file.is_null() {
        return false;
    }
    let mut data = Vec::new();
    let mut buffer = [0u8; 4096];
    loop {
        // SAFETY: the open FILE*, a local buffer
        let n = unsafe { stdio::fread(buffer.as_mut_ptr().cast(), 1, buffer.len(), file) };
        data.extend_from_slice(&buffer[..n]);
        if n < buffer.len() {
            break;
        }
    }
    unsafe { stdio::fclose(file) };
    list.list = parse_secret_file(&data);
    true
}

/// `SaveSecretList`: false when the file cannot be created.
///
/// # Safety
/// `s` is a list, `path` a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_secrets_save(s: *mut EeWonAuthSecrets, path: *const c_char) -> bool {
    unsafe { save_secrets(secrets(s), path) }
}

/// `SaveSecretList` of a list.
///
/// # Safety
/// `path` is a C string.
pub(crate) unsafe fn save_secrets(list: &Secrets, path: *const c_char) -> bool {
    let bytes = list.file_bytes();
    let file = unsafe { stdio::PlatFOpen(path, c"wb".as_ptr()) };
    if file.is_null() {
        return false;
    }
    // SAFETY: the open FILE*
    unsafe {
        stdio::fwrite(bytes.as_ptr().cast(), 1, bytes.len(), file);
        stdio::fclose(file);
    }
    true
}

/// # Safety
/// `s` is a list, `key` 8 bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_secrets_add(s: *mut EeWonAuthSecrets, key: *const u8) {
    let key = unsafe { *(key as *const [u8; 8]) };
    unsafe { secrets(s) }.add(key);
}

/// # Safety
/// `s` is a list, `key` 8 bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_secrets_remove(s: *mut EeWonAuthSecrets, key: *const u8) -> bool {
    let key = unsafe { &*(key as *const [u8; 8]) };
    unsafe { secrets(s) }.remove(key)
}

/// # Safety
/// `s` is a list.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_secrets_commit(s: *mut EeWonAuthSecrets, auth_time: i64, duration: i64) -> bool {
    unsafe { secrets(s) }.commit(auth_time, duration)
}

/// # Safety
/// `s` is a list.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_secrets_count(s: *mut EeWonAuthSecrets) -> usize {
    unsafe { secrets(s) }.list.len()
}

/// `AppendLoginSecrets`' bytes, to the sink.
///
/// # Safety
/// `s` is a list.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_secrets_login(
    s: *mut EeWonAuthSecrets,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let b = unsafe { secrets(s) }.login_bytes();
    unsafe { sink(context, b.as_ptr(), b.len()) };
}

/// One entry of the login's maps across the C ABI: a name (UTF-16 LE bytes), a second text (UTF-16 LE bytes: a
/// nickname's value), bytes (a CD key, user data, a file hash) when `present`, and more bytes (the keyed hash
/// data).
#[repr(C)]
pub struct EeWonAuthEntry {
    pub name: *const u8,
    pub name_len: usize,
    pub value: *const u8,
    pub value_len: usize,
    pub bytes: *const u8,
    pub bytes_len: usize,
    pub extra: *const u8,
    pub extra_len: usize,
    pub present: bool,
}

unsafe fn raw<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: `n` bytes (contract)
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

unsafe fn entries<'a>(p: *const EeWonAuthEntry, n: usize) -> &'a [EeWonAuthEntry] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: `n` entries (contract)
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

impl EeWonAuthEntry {
    unsafe fn name(&self) -> Vec<u16> {
        text::utf16_from_le_bytes(unsafe { raw(self.name, self.name_len) })
    }
    unsafe fn value(&self) -> Vec<u16> {
        text::utf16_from_le_bytes(unsafe { raw(self.value, self.value_len) })
    }
    unsafe fn bytes(&self) -> Option<Vec<u8>> {
        self.present.then(|| unsafe { raw(self.bytes, self.bytes_len) }.to_vec())
    }
    unsafe fn community(&self) -> Community {
        Community {
            name: unsafe { self.name() },
            cd_key: unsafe { self.bytes() },
            simple_hash: Vec::new(),
            keyed_hash_data: unsafe { raw(self.extra, self.extra_len) }.to_vec(),
        }
    }
}

/// `AppendCommunityData`'s block, to the sink. `communities[i].bytes` is the community's valid CD key's bytes
/// (`present` false without one).
///
/// # Safety
/// Each array holds its count of entries, each entry's pointers their lengths; `s` is a list.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_auth_community_data(
    communities: *const EeWonAuthEntry,
    community_count: usize,
    joins: *const EeWonAuthEntry,
    join_count: usize,
    user_data: *const EeWonAuthEntry,
    user_data_count: usize,
    nicknames: *const EeWonAuthEntry,
    nickname_count: usize,
    s: *mut EeWonAuthSecrets,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let d = unsafe {
        CommunityData {
            communities: entries(communities, community_count).iter().map(|e| e.community()).collect(),
            joins: entries(joins, join_count).iter().map(|e| (e.name(), e.bytes())).collect(),
            user_data: entries(user_data, user_data_count).iter().map(|e| (e.name(), e.bytes())).collect(),
            nicknames: entries(nicknames, nickname_count).iter().map(|e| (e.name(), e.value())).collect(),
        }
    };
    let b = community_data(&d, unsafe { secrets(s) });
    unsafe { sink(context, b.as_ptr(), b.len()) };
}

/// `AppendHashes`' block, to the sink: per community its name, its file hash in `bytes` and its section hashes in
/// `extra`.
///
/// # Safety
/// `communities` holds `count` entries; `challenge` its length.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_hashes(
    communities: *const EeWonAuthEntry,
    count: usize,
    challenge: *const u8,
    challenge_len: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let list: Vec<Community> = unsafe { entries(communities, count) }
        .iter()
        .map(|e| unsafe {
            Community {
                name: e.name(),
                cd_key: None,
                simple_hash: raw(e.bytes, e.bytes_len).to_vec(),
                keyed_hash_data: raw(e.extra, e.extra_len).to_vec(),
            }
        })
        .collect();
    let b = hashes(&list, unsafe { raw(challenge, challenge_len) });
    unsafe { sink(context, b.as_ptr(), b.len()) };
}

/// `StartAutoRefresh`'s decision: the wait in seconds, or -1 for no timer.
#[no_mangle]
pub extern "C" fn ee_won_auth_auto_refresh_wait(
    enabled: bool,
    force: bool,
    peer_valid: bool,
    expire_delta: i64,
) -> i64 {
    auto_refresh_wait(enabled, force, peer_valid, expire_delta).unwrap_or(-1)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn k(b: u8) -> [u8; 8] {
        [b; 8]
    }

    #[test]
    fn secret_file_round_trip_and_the_reader_quirks() {
        let mut s = Secrets::default();
        s.add(k(1));
        s.add(k(2));
        s.list[1].expires = 0x1234_5678;
        let bytes = s.file_bytes();
        assert_eq!(bytes.len(), 1 + 2 * 12);
        // the file ends right after the last record: one more (zero) record is read before the end shows
        let mut read = parse_secret_file(&bytes);
        assert_eq!(read.len(), 2);
        assert_eq!(read, s.list);
        let mut longer = bytes.clone();
        longer[0] = 3;
        read = parse_secret_file(&longer);
        assert_eq!(read.len(), 3);
        assert_eq!(read[2], Secret { expires: 0, key: [0; 8] });
        // a record cut inside its key keeps the bytes read
        let cut = &longer[..1 + 12 + 6];
        read = parse_secret_file(cut);
        assert_eq!(read.len(), 2);
        assert_eq!(read[1].key, [1, 1, 0, 0, 0, 0, 0, 0]);
        // an empty file: fgetc's EOF, nothing read
        assert!(parse_secret_file(&[]).is_empty());
        assert!(parse_secret_file(&[0]).is_empty());
    }

    #[test]
    fn commit_dates_drops_and_caps() {
        let mut s = Secrets::default();
        assert!(!s.commit(100, 10));
        for i in 0..300u32 {
            s.add(k(i as u8));
        }
        s.list[5].expires = 50;
        s.list[6].expires = 200;
        assert!(s.commit(100, 10));
        assert_eq!(s.list.len(), 255);
        assert_eq!(s.list[0].expires, 230);
        assert_eq!(s.list[5].expires, 200);
        // the sum wraps in an unsigned long
        let mut t = Secrets::default();
        t.add(k(9));
        t.commit(c_ulong::MAX as i64, 1);
        assert_eq!(t.list[0].expires, (c_ulong::MAX as i64 as c_ulong).wrapping_add(121) as i64);
    }

    #[test]
    fn remove_takes_the_first_uncommitted_match() {
        let mut s = Secrets::default();
        s.add(k(1));
        s.add(k(1));
        s.list[0].expires = 5;
        assert!(s.remove(&k(1)));
        assert_eq!(s.list.len(), 1);
        assert_eq!(s.list[0].expires, 5);
        assert!(!s.remove(&k(1)));
    }

    #[test]
    fn login_bytes_count_wraps() {
        let mut s = Secrets::default();
        for _ in 0..257 {
            s.add(k(3));
        }
        let b = s.login_bytes();
        assert_eq!(b[0], 1);
        assert_eq!(b.len(), 1 + 257 * 8);
    }

    #[test]
    fn community_data_layout() {
        let w = |t: &str| t.encode_utf16().collect::<Vec<u16>>();
        let d = CommunityData {
            communities: vec![
                Community { name: w("EE"), cd_key: Some(vec![0xaa, 0xbb]), ..Default::default() },
                Community { name: w("X"), ..Default::default() },
            ],
            joins: vec![(w("J"), Some(vec![1])), (w("K"), None)],
            user_data: vec![(w("U"), Some(vec![9, 9]))],
            nicknames: vec![(w("n"), w("")), (w("m"), w("v"))],
        };
        let mut s = Secrets::default();
        s.add(k(7));
        let b = community_data(&d, &s);
        let mut want = vec![0u8, 2, 2, 0, b'E', 0, b'E', 0, 1, 0, b'X', 0];
        want.extend_from_slice(&[7, 0]); // the record count
        want.extend_from_slice(&[1, 8, 0, 2, 0, b'E', 0, b'E', 0, 0xaa, 0xbb]);
        want.extend_from_slice(&[7, 9, 0, 1, 0, b'J', 0, 0, 0, 0, 0, 1]);
        want.extend_from_slice(&[8, 10, 0, 1, 0, b'U', 0, 0, 0, 0, 0, 9, 9]);
        want.extend_from_slice(&[6, 9, 0, 1, 7, 7, 7, 7, 7, 7, 7, 7]);
        want.extend_from_slice(&[4, 4, 0, 1, 0, b'n', 0]);
        want.extend_from_slice(&[4, 4, 0, 1, 0, b'm', 0]);
        want.extend_from_slice(&[3, 8, 0, 1, 0, b'm', 0, 1, 0, b'v', 0]);
        assert_eq!(b, want);
    }

    #[test]
    fn hashes_skip_unhashed_communities() {
        let w = |t: &str| t.encode_utf16().collect::<Vec<u16>>();
        let c = vec![
            Community { name: w("A"), simple_hash: vec![1; 16], keyed_hash_data: vec![2; 16], ..Default::default() },
            Community { name: w("B"), ..Default::default() },
        ];
        let b = hashes(&c, b"seed");
        assert_eq!(b[0], 1);
        assert_eq!(&b[1..6], &[1, 1, 0, b'A', 0]);
        assert_eq!(&b[6..22], &[1; 16]);
        let mut m = Md5::new();
        m.update(b"seed");
        m.update(&[2; 16]);
        assert_eq!(&b[22..38], &m.finish());
        assert_eq!(hashes(&[], b""), vec![0]);
    }

    #[test]
    fn auto_refresh() {
        assert_eq!(auto_refresh_wait(false, true, true, 1000), None);
        assert_eq!(auto_refresh_wait(true, false, false, 0), None);
        assert_eq!(auto_refresh_wait(true, true, false, 0), Some(300));
        assert_eq!(auto_refresh_wait(true, false, true, 1000), Some(880));
        assert_eq!(auto_refresh_wait(true, false, true, 100), Some(120));
    }
}
