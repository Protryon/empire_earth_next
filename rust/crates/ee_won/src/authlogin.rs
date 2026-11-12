//! `AuthContext`'s login data (WONAuth/AuthContext.cpp) as one Rust object: the user name and password, the login
//! secret file and its secrets, the communities (each one's CD key and file hashes), the CD-key joins, the user
//! data to set and the nicknames, with every setter of the original and the reads the login request makes (the
//! community data block, the hashes block, the secrets' block and file). The C++ `AuthContext` is a shim over it
//! (`AuthContext::mLogin`); its lock, peer data, server context and certificate flow (authflow.rs) stay there.
//!
//! The maps keep `std::map<std::wstring, ...>`'s order: by UTF-16 code unit, Windows' `wchar_t` (a POSIX build's
//! 32-bit `wchar_t` ordered a name above U+FFFF before U+E000..U+FFFF; nothing in the game uses such names).
//!
//! The communities' file hashes (`SetHashFileList`) are the original's MD5 loop over `PlatFOpen` / `fread` /
//! `feof`: one MD5 over every file's bytes and one per 16 KB section across the files, a short last section at the
//! end of the last file; a file that does not open returns false with what was hashed so far left as it was.
//!
//! Behaviour: `golden_authlogin.txt` (`tests/won/won_auth_test.cpp`'s login sequences over the original's
//! statements, recorded before the port) replayed by the tests. Reached only by the internet login and by
//! direct-connect games' certificate request (the servers are gone).

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::sync::Mutex;

use crate::authctx::{self, Community, CommunityData, Secrets};
use crate::ffi::EeWonByteSink;
use crate::md5::Md5;
use crate::text;

/// One community of the login (`AuthLoginCommunityData`): its CD key (`mKey`, `None`: NULL; `mValid`) and file
/// hashes.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct LoginCommunity {
    pub cd_key: Option<Vec<u8>>,
    pub cd_key_valid: bool,
    pub simple_hash: Vec<u8>,
    pub keyed_hash_data: Vec<u8>,
}

/// The login data.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Login {
    pub name: Vec<u16>,
    pub password: Vec<u16>,
    pub secret_file: Vec<u8>,
    pub communities: BTreeMap<Vec<u16>, LoginCommunity>,
    /// `mCDKeyCommunityJoinMap`: the join's CD key bytes (`None`: NULL)
    pub joins: BTreeMap<Vec<u16>, Option<Vec<u8>>>,
    /// `mSetCommunityUserDataMap` (`None`: NULL)
    pub user_data: BTreeMap<Vec<u16>, Option<Vec<u8>>>,
    pub nicknames: BTreeMap<Vec<u16>, Vec<u16>>,
    pub secrets: Secrets,
}

/// A file read as the original's stdio loop read it.
pub trait Stream {
    /// `fread(buf, 1, buf.len())`: the bytes read
    fn read(&mut self, buf: &mut [u8]) -> usize;
    /// `feof`
    fn eof(&self) -> bool;
}

/// A file's bytes as a stream: the end flag set by a read that comes short, as the CRT's.
pub struct Bytes {
    data: Vec<u8>,
    at: usize,
    eof: bool,
}

impl Bytes {
    pub fn new(data: Vec<u8>) -> Bytes {
        Bytes { data, at: 0, eof: false }
    }
}

impl Stream for Bytes {
    fn read(&mut self, buf: &mut [u8]) -> usize {
        let n = buf.len().min(self.data.len() - self.at);
        buf[..n].copy_from_slice(&self.data[self.at..self.at + n]);
        self.at += n;
        if n < buf.len() {
            self.eof = true;
        }
        n
    }
    fn eof(&self) -> bool {
        self.eof
    }
}

const SECTION: usize = 16384;

impl Login {
    /// `SetCommunity`: the community the only one, its data kept when it was there
    pub fn set_community(&mut self, name: &[u16]) {
        let kept = self.communities.remove(name).unwrap_or_default();
        self.communities.clear();
        self.communities.insert(name.to_vec(), kept);
    }

    /// `SetHashFileList(community, files)`: false when a file does not open (`open` returns `None`)
    pub fn hash_files(
        &mut self,
        community: &[u16],
        files: &[Vec<u8>],
        mut open: impl FnMut(&[u8]) -> Option<Box<dyn Stream>>,
    ) -> bool {
        let data = self.communities.entry(community.to_vec()).or_default();
        data.simple_hash.clear();
        data.keyed_hash_data.clear();
        let mut simple = Md5::new();
        let mut section = Md5::new();
        let mut position = 0usize;
        for (k, path) in files.iter().enumerate() {
            let Some(mut file) = open(path) else {
                return false;
            };
            let last = k + 1 == files.len();
            let mut buffer = vec![0u8; SECTION];
            while !file.eof() {
                let count = file.read(&mut buffer[..SECTION - position]);
                if count == 0 {
                    continue;
                }
                position += count;
                simple.update(&buffer[..count]);
                section.update(&buffer[..count]);
                if position != SECTION && (!file.eof() || !last) {
                    continue;
                }
                position = 0;
                data.keyed_hash_data.extend_from_slice(&section.finish());
                section = Md5::new();
            }
        }
        data.simple_hash = simple.finish().to_vec();
        true
    }

    fn community_list(&self) -> Vec<Community> {
        self.communities
            .iter()
            .map(|(name, c)| Community {
                name: name.clone(),
                cd_key: if c.cd_key_valid { c.cd_key.clone() } else { None },
                simple_hash: c.simple_hash.clone(),
                keyed_hash_data: c.keyed_hash_data.clone(),
            })
            .collect()
    }

    /// `AppendCommunityData`'s block
    pub fn community_data(&self) -> Vec<u8> {
        let d = CommunityData {
            communities: self.community_list(),
            joins: self.joins.iter().map(|(k, v)| (k.clone(), v.clone())).collect(),
            user_data: self.user_data.iter().map(|(k, v)| (k.clone(), v.clone())).collect(),
            nicknames: self.nicknames.iter().map(|(k, v)| (k.clone(), v.clone())).collect(),
        };
        authctx::community_data(&d, &self.secrets)
    }

    /// `AppendHashes`' block
    pub fn hashes(&self, challenge: &[u8]) -> Vec<u8> {
        authctx::hashes(&self.community_list(), challenge)
    }

    /// The members, one line (the driver's form)
    pub fn describe(&self) -> String {
        let u = |t: &[u16]| crate::reqop::hex(&text::utf16_to_le_bytes(t));
        let o = |b: &Option<Vec<u8>>| b.as_ref().map_or("n".to_string(), |b| crate::reqop::hex(b));
        let mut s = format!("name {} pass {} c", u(&self.name), u(&self.password));
        for (k, c) in &self.communities {
            s += &format!(
                " {}:{}/{}/{}/{}",
                u(k),
                o(&c.cd_key),
                u8::from(c.cd_key_valid),
                crate::reqop::hex(&c.simple_hash),
                crate::reqop::hex(&c.keyed_hash_data)
            );
        }
        s += " j";
        for (k, v) in &self.joins {
            s += &format!(" {}:{}", u(k), o(v));
        }
        s += " d";
        for (k, v) in &self.user_data {
            s += &format!(" {}:{}", u(k), o(v));
        }
        s += " n";
        for (k, v) in &self.nicknames {
            s += &format!(" {}:{}", u(k), u(v));
        }
        s
    }
}

// ---- the setters by number (`ee_won_auth_login_set`)

/// `SetUserName(text)`
pub const EE_WON_AUTH_LOGIN_NAME: i32 = 1;
/// `SetPassword(text)`
pub const EE_WON_AUTH_LOGIN_PASSWORD: i32 = 2;
/// `SetLoginSecretFile(bytes)`
pub const EE_WON_AUTH_LOGIN_SECRET_FILE: i32 = 3;
/// `AddCommunity(key)`
pub const EE_WON_AUTH_LOGIN_ADD_COMMUNITY: i32 = 4;
/// `SetCommunity(key)`
pub const EE_WON_AUTH_LOGIN_SET_COMMUNITY: i32 = 5;
/// `RemoveCommunity(key)`
pub const EE_WON_AUTH_LOGIN_REMOVE_COMMUNITY: i32 = 6;
/// `RemoveAllCommunity()`
pub const EE_WON_AUTH_LOGIN_REMOVE_ALL_COMMUNITY: i32 = 7;
/// `SetCDKey(key, CDKey)`: the key's bytes, `num` bit 0 its `mKey` present, bit 1 `mValid`
pub const EE_WON_AUTH_LOGIN_CDKEY: i32 = 8;
/// `AddCDKeyCommunityJoin(key, CDKey)`: the key's bytes, `num` bit 0 present
pub const EE_WON_AUTH_LOGIN_ADD_JOIN: i32 = 9;
/// `ClearCDKeyCommunityJoins()`
pub const EE_WON_AUTH_LOGIN_CLEAR_JOINS: i32 = 10;
/// `AddUserData(key, data)`: the data, `num` bit 0 present
pub const EE_WON_AUTH_LOGIN_ADD_USER_DATA: i32 = 11;
/// `ClearUserData()`
pub const EE_WON_AUTH_LOGIN_CLEAR_USER_DATA: i32 = 12;
/// `ClearOneTimeData()`
pub const EE_WON_AUTH_LOGIN_CLEAR_ONE_TIME: i32 = 13;
/// `AddNickname(key)`
pub const EE_WON_AUTH_LOGIN_ADD_NICKNAME: i32 = 14;
/// `SetNickname(key, text)`
pub const EE_WON_AUTH_LOGIN_SET_NICKNAME: i32 = 15;
/// `ClearNicknames()`
pub const EE_WON_AUTH_LOGIN_CLEAR_NICKNAMES: i32 = 16;

impl Login {
    /// A setter by number: `key` a map's key (UTF-16), `value` a text (UTF-16) or bytes, `num` its flags.
    pub fn set(&mut self, which: i32, key: &[u16], value: &[u8], num: i64) {
        let text = || text::utf16_from_le_bytes(value);
        let present = num & 1 != 0;
        match which {
            EE_WON_AUTH_LOGIN_NAME => self.name = text(),
            EE_WON_AUTH_LOGIN_PASSWORD => self.password = text(),
            EE_WON_AUTH_LOGIN_SECRET_FILE => self.secret_file = value.to_vec(),
            EE_WON_AUTH_LOGIN_ADD_COMMUNITY => {
                self.communities.entry(key.to_vec()).or_default();
            }
            EE_WON_AUTH_LOGIN_SET_COMMUNITY => self.set_community(key),
            EE_WON_AUTH_LOGIN_REMOVE_COMMUNITY => {
                self.communities.remove(key);
            }
            EE_WON_AUTH_LOGIN_REMOVE_ALL_COMMUNITY => self.communities.clear(),
            EE_WON_AUTH_LOGIN_CDKEY => {
                let c = self.communities.entry(key.to_vec()).or_default();
                c.cd_key = present.then(|| value.to_vec());
                c.cd_key_valid = num & 2 != 0;
            }
            EE_WON_AUTH_LOGIN_ADD_JOIN => {
                self.joins.insert(key.to_vec(), present.then(|| value.to_vec()));
            }
            EE_WON_AUTH_LOGIN_CLEAR_JOINS => self.joins.clear(),
            EE_WON_AUTH_LOGIN_ADD_USER_DATA => {
                self.user_data.insert(key.to_vec(), present.then(|| value.to_vec()));
            }
            EE_WON_AUTH_LOGIN_CLEAR_USER_DATA => self.user_data.clear(),
            EE_WON_AUTH_LOGIN_CLEAR_ONE_TIME => {
                self.user_data.clear();
                self.joins.clear();
            }
            EE_WON_AUTH_LOGIN_ADD_NICKNAME => {
                self.nicknames.insert(key.to_vec(), Vec::new());
            }
            EE_WON_AUTH_LOGIN_SET_NICKNAME => {
                self.nicknames.insert(key.to_vec(), text());
            }
            EE_WON_AUTH_LOGIN_CLEAR_NICKNAMES => self.nicknames.clear(),
            _ => {}
        }
    }
}

// ---- the C ABI (AuthContext.cpp's shim)

/// cbindgen:ignore
mod stdio {
    use std::ffi::{c_char, c_int, c_void};
    extern "C" {
        pub fn PlatFOpen(path: *const c_char, mode: *const c_char) -> *mut c_void;
        pub fn fclose(file: *mut c_void) -> c_int;
        pub fn fread(data: *mut c_void, size: usize, count: usize, file: *mut c_void) -> usize;
        pub fn feof(file: *mut c_void) -> c_int;
    }
}

/// An open `FILE*` as a stream (closed when dropped).
struct CFile(*mut c_void);

impl Stream for CFile {
    fn read(&mut self, buf: &mut [u8]) -> usize {
        // SAFETY: the open FILE*, the buffer's bytes
        unsafe { stdio::fread(buf.as_mut_ptr().cast(), 1, buf.len(), self.0) }
    }
    fn eof(&self) -> bool {
        // SAFETY: the open FILE*
        unsafe { stdio::feof(self.0) != 0 }
    }
}

impl Drop for CFile {
    fn drop(&mut self) {
        // SAFETY: the open FILE*, closed once
        unsafe { stdio::fclose(self.0) };
    }
}

fn open_c(path: &[u8]) -> Option<Box<dyn Stream>> {
    let p = std::ffi::CString::new(path.to_vec()).ok()?;
    // SAFETY: two C strings
    let f = unsafe { stdio::PlatFOpen(p.as_ptr(), c"rb".as_ptr()) };
    (!f.is_null()).then(|| Box::new(CFile(f)) as Box<dyn Stream>)
}

/// The login data (opaque): `AuthContext::mLogin`, its own lock.
pub struct EeWonAuthLogin(Mutex<Login>);

unsafe fn login<'a>(l: *const EeWonAuthLogin) -> std::sync::MutexGuard<'a, Login> {
    // SAFETY: a login from ee_won_auth_login_new (contract)
    unsafe { &*l }.0.lock().unwrap_or_else(|e| e.into_inner())
}

unsafe fn give(sink: EeWonByteSink, context: *mut c_void, b: &[u8]) {
    // SAFETY: the caller's sink
    unsafe { sink(context, b.as_ptr(), b.len()) };
}

/// An empty login (the secret file: `secret_file`, the WON directory's `_wonlogin.ks`).
///
/// # Safety
/// `secret_file` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_login_new(secret_file: *const u8, len: usize) -> *mut EeWonAuthLogin {
    let l = Login { secret_file: unsafe { crate::ffi::bytes(secret_file, len) }.to_vec(), ..Default::default() };
    Box::into_raw(Box::new(EeWonAuthLogin(Mutex::new(l))))
}

/// # Safety
/// `l` is from `ee_won_auth_login_new` (or NULL), not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_login_free(l: *mut EeWonAuthLogin) {
    if !l.is_null() {
        drop(unsafe { Box::from_raw(l) });
    }
}

/// A setter (`EE_WON_AUTH_LOGIN_*`): `key` a map's key and `value` a text (UTF-16 LE bytes) or bytes.
///
/// # Safety
/// `l` is a login; the buffers hold their lengths.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_login_set(
    l: *const EeWonAuthLogin,
    which: i32,
    key: *const u8,
    key_len: usize,
    value: *const u8,
    value_len: usize,
    num: i64,
) {
    let k = text::utf16_from_le_bytes(unsafe { crate::ffi::bytes(key, key_len) });
    let v = unsafe { crate::ffi::bytes(value, value_len) };
    unsafe { login(l) }.set(which, &k, v, num);
}

/// `EE_WON_AUTH_LOGIN_NAME` / `_PASSWORD`: the text (UTF-16 LE bytes); `_SECRET_FILE`: its bytes.
///
/// # Safety
/// `l` is a login; `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_login_text(
    l: *const EeWonAuthLogin,
    which: i32,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let b = {
        let g = unsafe { login(l) };
        match which {
            EE_WON_AUTH_LOGIN_NAME => text::utf16_to_le_bytes(&g.name),
            EE_WON_AUTH_LOGIN_PASSWORD => text::utf16_to_le_bytes(&g.password),
            _ => g.secret_file.clone(),
        }
    };
    unsafe { give(sink, context, &b) };
}

/// `SetHashFileList(community, files)`: the files NUL-separated (`files_len` bytes, each path NUL-ended).
///
/// # Safety
/// `l` is a login; the buffers hold their lengths.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_login_hash_files(
    l: *const EeWonAuthLogin,
    community: *const u8,
    community_len: usize,
    files: *const u8,
    files_len: usize,
) -> bool {
    let c = text::utf16_from_le_bytes(unsafe { crate::ffi::bytes(community, community_len) });
    let list: Vec<Vec<u8>> =
        unsafe { crate::ffi::bytes(files, files_len) }.split(|&b| b == 0).map(<[u8]>::to_vec).collect();
    let list = &list[..list.len().saturating_sub(1)];
    unsafe { login(l) }.hash_files(&c, list, open_c)
}

/// The members, one line (`Login::describe`, the drivers' form).
///
/// # Safety
/// `l` is a login; `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_login_describe(
    l: *const EeWonAuthLogin,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let d = unsafe { login(l) }.describe();
    unsafe { give(sink, context, d.as_bytes()) };
}

/// The login request's blocks: 0 `AppendCommunityData`, 1 `AppendHashes(challenge)` (`data`), 2
/// `AppendLoginSecrets`.
///
/// # Safety
/// `l` is a login; `data` holds `len` bytes; `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_login_block(
    l: *const EeWonAuthLogin,
    which: i32,
    data: *const u8,
    len: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let b = {
        let g = unsafe { login(l) };
        match which {
            0 => g.community_data(),
            1 => g.hashes(unsafe { crate::ffi::bytes(data, len) }),
            _ => g.secrets.login_bytes(),
        }
    };
    unsafe { give(sink, context, &b) };
}

/// The secrets: 0 `LoadSecretList`, 1 `SaveSecretList` (false: the file did not open), 2 `AddSecret(key)` and save,
/// 3 `RemoveSecret(key)` and save when removed, 4 `CommitSecrets(t0, t1)` and save when it changed anything.
///
/// # Safety
/// `l` is a login; `key` 8 bytes for 2 and 3.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_login_secrets(
    l: *const EeWonAuthLogin,
    which: i32,
    key: *const u8,
    t0: i64,
    t1: i64,
) -> bool {
    let mut g = unsafe { login(l) };
    let path = std::ffi::CString::new(g.secret_file.clone()).unwrap_or_default();
    let p = path.as_ptr();
    let key = || {
        let mut k = [0u8; 8];
        // SAFETY: 8 bytes (contract)
        k.copy_from_slice(unsafe { std::slice::from_raw_parts(key, 8) });
        k
    };
    let s = &mut g.secrets;
    // SAFETY: the path a C string
    unsafe {
        match which {
            0 => authctx::load_secrets(s, p),
            1 => authctx::save_secrets(s, p),
            2 => {
                s.add(key());
                authctx::save_secrets(s, p);
                true
            }
            3 => {
                if s.remove(&key()) {
                    authctx::save_secrets(s, p);
                }
                true
            }
            _ => {
                if s.commit(t0, t1) {
                    authctx::save_secrets(s, p);
                }
                true
            }
        }
    }
}

#[cfg(test)]
#[path = "authlogin_tests.rs"]
mod tests;
