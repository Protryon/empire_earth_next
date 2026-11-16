//! WONInternal's RegAuth whole (net round 27): the auth settings under `Software\Sierra\MPTools\Auth` (the server's
//! address and port, the login name, the community, the password stored encrypted), read and written through the
//! Rust RegKey ([`EeWonRegKey`], regkey.rs) with miscops.rs's cipher and port default. RegAuth.cpp is deleted; the
//! four functions of WONRegAuth.h forward here. `won_regauth_test` compares them with the original's statements
//! (golden `golden_regauth.txt`, recorded over RegAuth.cpp).

use std::ffi::c_void;

use crate::ffi::bytes;
use crate::miscops::{auth_port, password};
use crate::regkey::{EeWonRegKey, RegValue};

pub const KEY_TOOLS_AUTH: &[u8] = b"Software\\Sierra\\MPTools\\Auth";
pub const VALUE_AUTHSERVERIP: &[u8] = b"AuthServerIP";
pub const VALUE_AUTHSERVERPORT: &[u8] = b"AuthServerPort";
pub const VALUE_USERNAME: &[u8] = b"UserName";
pub const VALUE_COMMUNITY: &[u8] = b"Community";
pub const VALUE_PASSWORD: &[u8] = b"Password";

/// What `ReadAuthSettings` reads: each `None` leaves the caller's value as it was (`GetValue` erased it first,
/// so a value missing from an open key reads as empty).
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct AuthSettings {
    pub address: Option<Vec<u8>>,
    pub port: Option<u16>,
    pub login: Option<Vec<u8>>,
    pub community: Option<Vec<u8>>,
    pub password: Option<Vec<u8>>,
}

fn key() -> EeWonRegKey {
    let mut k = EeWonRegKey::default();
    k.open_new_key(1, KEY_TOOLS_AUTH);
    k
}

/// `ReadAuthSettings`: the five-value form (`with_server`) or the three-value one; true when a password decrypted.
pub fn read(with_server: bool) -> (bool, AuthSettings) {
    let k = key();
    let mut s = AuthSettings::default();
    if !k.open {
        return (false, s);
    }
    if with_server {
        s.address = Some(k.get_value(VALUE_AUTHSERVERIP).unwrap_or_default());
        let (code, value) = k.query_dword(VALUE_AUTHSERVERPORT);
        // a missing value (QueryDWORD's 1) gives the default 15200
        s.port = Some(auth_port(code, value));
    }
    s.login = Some(k.get_value(VALUE_USERNAME).unwrap_or_default());
    s.community = Some(k.get_value(VALUE_COMMUNITY).unwrap_or_default());
    if let Some(crate::regkey::Binary::Bytes(cipher)) = k.query_binary(VALUE_PASSWORD) {
        if let Some(p) = password(true, &cipher) {
            s.password = Some(p);
            return (true, s);
        }
    }
    (false, s)
}

/// `WriteAuthSettings`: the values set (the server's only with `server`), then the password encrypted when there
/// is one; true when it was written.
pub fn write(server: Option<(&[u8], u16)>, login: &[u8], community: &[u8], pw: &[u8]) -> bool {
    let k = key();
    if !k.open {
        return false;
    }
    if let Some((address, port)) = server {
        k.set_value(VALUE_AUTHSERVERIP, RegValue::Text(address));
        k.set_value(VALUE_AUTHSERVERPORT, RegValue::Dword(u32::from(port)));
    }
    k.set_value(VALUE_USERNAME, RegValue::Text(login));
    k.set_value(VALUE_COMMUNITY, RegValue::Text(community));
    if pw.is_empty() {
        return false;
    }
    match password(false, pw) {
        Some(cipher) => {
            k.set_value(VALUE_PASSWORD, RegValue::Binary(&cipher));
            true
        }
        None => false,
    }
}

// ---- the C ABI (WONRegAuth.h)

/// The texts `ee_won_regauth_read` hands back, by the sink's `which`.
pub const EE_WON_REGAUTH_ADDRESS: i32 = 0;
pub const EE_WON_REGAUTH_LOGIN: i32 = 1;
pub const EE_WON_REGAUTH_COMMUNITY: i32 = 2;
pub const EE_WON_REGAUTH_PASSWORD: i32 = 3;

/// `ReadAuthSettings`: each value read to `sink` with its `EE_WON_REGAUTH_*` (none for a value left alone); the
/// port to `*port` when read (`with_server`).
///
/// # Safety
/// `port` is NULL or writable; `sink` receives the texts during the call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_regauth_read(
    with_server: bool,
    port: *mut u16,
    sink: unsafe extern "C" fn(context: *mut c_void, which: i32, bytes: *const u8, length: usize),
    context: *mut c_void,
) -> bool {
    let (ok, s) = read(with_server);
    let out = |w: i32, v: &Option<Vec<u8>>| {
        if let Some(v) = v {
            // SAFETY: the sink's contract
            unsafe { sink(context, w, v.as_ptr(), v.len()) };
        }
    };
    out(EE_WON_REGAUTH_ADDRESS, &s.address);
    if let (Some(p), false) = (s.port, port.is_null()) {
        // SAFETY: the contract
        unsafe { *port = p };
    }
    out(EE_WON_REGAUTH_LOGIN, &s.login);
    out(EE_WON_REGAUTH_COMMUNITY, &s.community);
    out(EE_WON_REGAUTH_PASSWORD, &s.password);
    ok
}

/// One text argument of `ee_won_regauth_write`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonRegAuthText {
    pub data: *const u8,
    pub length: usize,
}

/// `WriteAuthSettings`; `address` NULL for the three-value form.
///
/// # Safety
/// Each text holds its length's bytes; `address` is NULL or a text.
#[no_mangle]
pub unsafe extern "C" fn ee_won_regauth_write(
    address: *const EeWonRegAuthText,
    port: u16,
    login: EeWonRegAuthText,
    community: EeWonRegAuthText,
    pw: EeWonRegAuthText,
) -> bool {
    // SAFETY: the contract
    let t = |x: EeWonRegAuthText| unsafe { bytes(x.data, x.length) };
    // SAFETY: as above
    let server = unsafe { address.as_ref() }.map(|a| (t(*a), port));
    write(server, t(login), t(community), t(pw))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_names_are_the_originals() {
        assert_eq!(
            crate::regkey::config_block(crate::regkey::ROOT_BLOCK, KEY_TOOLS_AUTH),
            b"won.Software.Sierra.MPTools.Auth"
        );
        assert_eq!(crate::regkey::config_name(VALUE_AUTHSERVERPORT), b"AuthServerPort");
    }

    #[test]
    fn the_cipher_round_trips() {
        let c = password(false, b"secret pw").unwrap();
        assert_eq!(password(true, &c).unwrap(), b"secret pw");
        assert_eq!(auth_port(1, 15200), 15200);
    }
}
