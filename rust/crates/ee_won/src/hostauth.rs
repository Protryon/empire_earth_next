//! The op base's certificate and auth context calls answered in Rust (net round 20): GetCertOp's certificates, key
//! blocks, private keys and peer data (certhost.rs's `EE_WON_HOST_CERT_*`, round 6's hooks) as Rust handles, and
//! the auth context's members they read and write (its peer data, login texts, community data, hashes and secrets,
//! the verifier key) on the Rust `AuthContext` (authobj.rs); and the request ops' auth context calls (`mAuthContext`'s
//! user id, peer data, sessions and refreshes). ServerOp.cpp's `HostCall` answered these on the C++ objects; it now
//! answers one call, `EE_WON_HOST_AUTH_CORE` (a C++ `AuthContext*`, or the op's `mAuthContext`, to its Rust
//! object). The game's ops are bound with `ee_won_hostop_bind_native`; the drivers' stand-ins still see every call.
//!
//! A handle is an `Arc<Item>`'s pointer with its low bit set (a C++ `RefCount*` never has it), so the generic
//! `AddRef` / `Release` calls tell the two apart. Peer data carries its key block's handle, made with it (the C++
//! `AuthPeerData` shim's `mKeyBlock`: a new one per shim from `GetPeerData`, the given one for `new
//! AuthPeerData(cert, block, ...)`), so `GetPubKeyBlock()` is borrowed as before.
//!
//! The behaviour is `golden_authhost.txt` (tests/won/won_authhost_test.cpp, recorded over the C++ `HostCall` and
//! `AuthContext`). The original's NULL dereferences are defined: a missing object or context answers 0, none or
//! nothing; a secret shorter than 8 bytes is padded with zeros.

use std::ffi::c_void;
use std::sync::{Arc, Mutex};

use crate::auth::Kind;
use crate::authflow::{self, EeWonLightCert};
use crate::authlogin::{self, EE_WON_AUTH_LOGIN_CLEAR_ONE_TIME, EE_WON_AUTH_LOGIN_NAME, EE_WON_AUTH_LOGIN_PASSWORD};
use crate::authobj::{self, EeWonAuthCtx};
use crate::elgamal::ElGamal;
use crate::ffi::EeWonByteSink;
use crate::hostop::{self, Obj, Reply};
use crate::peerauth::EeWonAuthSession;
use crate::peerobj::{self, unix_now, Cert, PeerData, EE_WON_AUTHOBJ_AUTH_DELTA};
use crate::peerobj::{
    EE_WON_AUTHOBJ_BLOCK_ID, EE_WON_AUTHOBJ_EXPIRE, EE_WON_AUTHOBJ_ISSUE, EE_WON_AUTHOBJ_PEER_DELTA,
    EE_WON_AUTHOBJ_VALID,
};
use crate::random::Random;

/// What a handle holds.
pub(crate) enum Item {
    /// a certificate or a key block (`AuthBase`)
    Cert(Arc<Cert>),
    /// peer data and its key block's handle (held; none for NULL)
    Peer { peer: Arc<PeerData>, block: Obj },
    /// a private key (`new ElGamal`)
    Key(Box<Mutex<ElGamal>>),
    /// an HTTP cache (httpcache.rs; round 21)
    HttpCache(crate::httpcache::HttpCache),
    /// an HTTP cache's entry (its maps hold the same `Arc`, so a handle to it is the same number every time)
    CacheEntry(Mutex<crate::httpcache::CacheEntry>),
    /// a peer session (`AuthSession`; round 22): the peer auth op that made it, the auth context's cache and the
    /// sockets' `CryptTransform`s hold it
    Session(Mutex<EeWonAuthSession>),
}

impl Drop for Item {
    fn drop(&mut self) {
        if let Item::Peer { block, .. } = self {
            release(*block);
        }
    }
}

/// Whether `o` is a handle (not a C++ object).
pub(crate) fn is_handle(o: Obj) -> bool {
    o.0 & 1 == 1
}

/// A new handle (one reference).
pub(crate) fn handle(i: Item) -> Obj {
    Obj(Arc::into_raw(Arc::new(i)) as usize | 1)
}

/// A new handle reference to an item already shared (the same number as every other handle to it).
pub(crate) fn handle_of(i: &Arc<Item>) -> Obj {
    Obj(Arc::into_raw(i.clone()) as usize | 1)
}

fn raw(o: Obj) -> *const Item {
    (o.0 & !1) as *const Item
}

/// The handle's item (another reference), or None for anything else.
pub(crate) fn item(o: Obj) -> Option<Arc<Item>> {
    if !is_handle(o) {
        return None;
    }
    // SAFETY: a live handle (the caller holds a reference): one more count, then an Arc for it
    unsafe {
        Arc::increment_strong_count(raw(o));
        Some(Arc::from_raw(raw(o)))
    }
}

/// `AddRef` of a handle.
pub(crate) fn add_ref(o: Obj) {
    if is_handle(o) {
        // SAFETY: a live handle
        unsafe { Arc::increment_strong_count(raw(o)) };
    }
}

/// `Release` of a handle.
pub(crate) fn release(o: Obj) {
    if is_handle(o) {
        // SAFETY: a reference the caller held
        unsafe { Arc::decrement_strong_count(raw(o)) };
    }
}

/// Peer data's handle with a new key block handle (`AuthPeerData::Adopt`).
pub(crate) fn peer_handle(peer: Arc<PeerData>) -> Obj {
    let block = peer.block.clone().map_or(Obj::none(), |b| handle(Item::Cert(b)));
    handle(Item::Peer { peer, block })
}

/// A new `EeWonAuthObj` reference to a handle's peer data (the C++ `AuthPeerData` shims), null for anything else.
pub(crate) fn peer_obj(o: Obj) -> *mut peerobj::EeWonAuthObj {
    match peer_of(o) {
        Some(p) => peerobj::peer_ref(p),
        None => std::ptr::null_mut(),
    }
}

fn cert_of(o: Obj) -> Option<Arc<Cert>> {
    match item(o).as_deref() {
        Some(Item::Cert(c)) => Some(c.clone()),
        _ => None,
    }
}

fn peer_of(o: Obj) -> Option<Arc<PeerData>> {
    match item(o).as_deref() {
        Some(Item::Peer { peer, .. }) => Some(peer.clone()),
        _ => None,
    }
}

/// `AuthBase::Num(which)` of a handle (at the current time); 0 for anything else.
fn num_of(o: Obj, which: i32) -> i64 {
    match item(o).as_deref() {
        Some(Item::Cert(c)) => peerobj::cert_num(c, which, 0, 0, 0, unix_now()),
        Some(Item::Peer { peer, .. }) => peerobj::peer_num(peer, which, 0, unix_now()),
        _ => 0,
    }
}

fn num(n: i64) -> Option<Reply> {
    Some(Reply { obj: Obj::none(), num: n, addr: [0; 6], bytes: None })
}

fn obj(o: Obj) -> Option<Reply> {
    Some(Reply { obj: o, num: 0, addr: [0; 6], bytes: None })
}

fn bytes(b: Option<Vec<u8>>) -> Option<Reply> {
    Some(Reply { obj: Obj::none(), num: 0, addr: [0; 6], bytes: b })
}

unsafe extern "C" fn vec_sink(context: *mut c_void, data: *const u8, length: usize) {
    // SAFETY: the Vec this module passed; the callee's bytes
    let v = unsafe { &mut *(context as *mut Vec<u8>) };
    v.extend_from_slice(unsafe { crate::ffi::bytes(data, length) });
}

fn login_text(c: &EeWonAuthCtx, which: i32) -> Vec<u8> {
    let mut v = Vec::new();
    // SAFETY: the context's login, a Vec sink
    unsafe { authlogin::ee_won_auth_login_text(c.login(), which, vec_sink, &mut v as *mut Vec<u8> as *mut c_void) };
    v
}

fn login_block(c: &EeWonAuthCtx, which: i32, data: &[u8]) -> Vec<u8> {
    let mut v = Vec::new();
    // SAFETY: the context's login, the bytes, a Vec sink
    unsafe {
        authlogin::ee_won_auth_login_block(
            c.login(),
            which,
            data.as_ptr(),
            data.len(),
            vec_sink,
            &mut v as *mut Vec<u8> as *mut c_void,
        )
    };
    v
}

fn login_set(c: &EeWonAuthCtx, which: i32, value: &[u8]) {
    // SAFETY: the context's login, the bytes
    unsafe { authlogin::ee_won_auth_login_set(c.login(), which, std::ptr::null(), 0, value.as_ptr(), value.len(), 0) };
}

fn secrets(c: &EeWonAuthCtx, which: i32, key: &[u8], t0: i64, t1: i64) {
    let mut k = [0u8; 8];
    let n = key.len().min(8);
    k[..n].copy_from_slice(&key[..n]);
    // SAFETY: the context's login, 8 bytes
    unsafe { authlogin::ee_won_auth_login_secrets(c.login(), which, k.as_ptr(), t0, t1) };
}

/// A session handle's `GetLastUseTime()` (anything else: 0).
pub(crate) fn session_last_use(o: Obj) -> i64 {
    match item(o).as_deref() {
        Some(Item::Session(s)) => s.lock().unwrap_or_else(|e| e.into_inner()).last_use,
        _ => 0,
    }
}

/// A new session handle (one reference).
pub(crate) fn session_handle(s: EeWonAuthSession) -> Obj {
    handle(Item::Session(Mutex::new(s)))
}

/// A peer auth op's session calls, given the op's session (`EE_WON_PEEROP_SESSION`, borrowed; none for none):
/// `GetSession()` (a new reference) and `GetSession()->GetKey()`'s bytes.
pub(crate) fn op_session(code: i32, session: Obj) -> Reply {
    match item(session).as_deref() {
        Some(Item::Session(s)) if code == hostop::EE_WON_HOST_PEER_AUTH_KEY => {
            let key = s.lock().unwrap_or_else(|e| e.into_inner()).key.key().to_vec();
            Reply { obj: Obj::none(), num: 0, addr: [0; 6], bytes: Some(key) }
        }
        Some(Item::Session(_)) => {
            add_ref(session);
            Reply { obj: session, num: 0, addr: [0; 6], bytes: None }
        }
        _ => Reply { obj: Obj::none(), num: 0, addr: [0; 6], bytes: None },
    }
}

/// The call answered here, or None for the C++ side. `core(o)` is the Rust object of a C++ `AuthContext*` `o` (none:
/// the op's `mAuthContext`), through the one call left (`EE_WON_HOST_AUTH_CORE`).
pub(crate) fn answer(
    code: i32,
    o: [Obj; 3],
    n: [i64; 2],
    data: &[u8],
    addr: [u8; 6],
    core: &dyn Fn(Obj) -> Option<Arc<EeWonAuthCtx>>,
) -> Option<Reply> {
    use hostop::*;
    match code {
        EE_WON_HOST_ADD_REF if is_handle(o[0]) => {
            add_ref(o[0]);
            num(0)
        }
        EE_WON_HOST_RELEASE | EE_WON_HOST_CERT_PRIV_FREE if is_handle(o[0]) => {
            release(o[0]);
            num(0)
        }
        EE_WON_HOST_CERT_PRIV_FREE => num(0),
        // `AuthContext::GetVerifierKey().IsPublic()`
        EE_WON_HOST_CERT_VERIFIER_PUBLIC => num(i64::from(authobj::verifier(|k, _| k.is_public()))),
        // ctx's `GetPeerData()` (a new shim: a reference)
        EE_WON_HOST_CERT_CTX_PEER => obj(core(o[0]).and_then(|c| c.peer_data()).map_or(Obj::none(), peer_handle)),
        // `peer->GetPubKeyBlock()` (borrowed)
        EE_WON_HOST_CERT_PEER_BLOCK => obj(match item(o[0]).as_deref() {
            Some(Item::Peer { block, .. }) => *block,
            _ => Obj::none(),
        }),
        EE_WON_HOST_CERT_PEER_DELTA => num(num_of(o[0], EE_WON_AUTHOBJ_PEER_DELTA)),
        EE_WON_HOST_CERT_LOAD_SECRETS => {
            if let Some(c) = core(o[0]) {
                secrets(&c, 0, &[], 0, 0);
            }
            num(0)
        }
        EE_WON_HOST_CERT_BLOCK_VALID | EE_WON_HOST_CERT_VALID => num(num_of(o[0], EE_WON_AUTHOBJ_VALID)),
        // `ctx->SetPeerData(new AuthPeerData(block))`
        EE_WON_HOST_CERT_SET_PEER_FROM_BLOCK => {
            if let Some(c) = core(o[0]) {
                c.set_peer_data(Some(Arc::new(PeerData::empty(cert_of(o[1])))));
            }
            num(0)
        }
        EE_WON_HOST_CERT_BLOCK_ID => num(num_of(o[0], EE_WON_AUTHOBJ_BLOCK_ID)),
        EE_WON_HOST_CERT_BLOCK_ENCRYPT => bytes(cert_of(o[0]).and_then(|c| c.encrypt(data))),
        EE_WON_HOST_CERT_USER => bytes(core(o[0]).map(|c| login_text(&c, EE_WON_AUTH_LOGIN_NAME))),
        EE_WON_HOST_CERT_PASSWORD => bytes(core(o[0]).map(|c| login_text(&c, EE_WON_AUTH_LOGIN_PASSWORD))),
        EE_WON_HOST_CERT_COMMUNITY => bytes(core(o[0]).map(|c| login_block(&c, 0, &[]))),
        EE_WON_HOST_CERT_HASHES => bytes(core(o[0]).map(|c| login_block(&c, 1, data))),
        EE_WON_HOST_CERT_ADD_SECRET | EE_WON_HOST_CERT_REMOVE_SECRET => {
            if let Some(c) = core(o[0]) {
                secrets(&c, if code == EE_WON_HOST_CERT_ADD_SECRET { 2 } else { 3 }, data, 0, 0);
            }
            num(0)
        }
        EE_WON_HOST_CERT_NEW => {
            let kind = if n[0] == 1 { Kind::Certificate } else { Kind::Certificate2 };
            obj(handle(Item::Cert(Arc::new(Cert::new(kind, data)))))
        }
        EE_WON_HOST_CERT_NEW_BLOCK => obj(handle(Item::Cert(Arc::new(Cert::new(Kind::PubKeyBlock, data))))),
        // `block->Verify(cert)`: a key block's, or peer data's (its key block's)
        EE_WON_HOST_CERT_BLOCK_VERIFY => {
            let ok = cert_of(o[1]).is_some_and(|other| match item(o[0]).as_deref() {
                Some(Item::Cert(k)) => k.verify(&other),
                Some(Item::Peer { peer, .. }) => peer.verify(&other),
                _ => false,
            });
            num(i64::from(ok))
        }
        EE_WON_HOST_CERT_DELTA => num(num_of(o[0], EE_WON_AUTHOBJ_AUTH_DELTA)),
        EE_WON_HOST_CERT_TIMES => {
            let mut b = num_of(o[0], EE_WON_AUTHOBJ_ISSUE).to_le_bytes().to_vec();
            b.extend_from_slice(&num_of(o[0], EE_WON_AUTHOBJ_EXPIRE).to_le_bytes());
            bytes(Some(b))
        }
        // `block->IsValidAndVerified(AuthContext::GetVerifierKey())`
        EE_WON_HOST_CERT_BLOCK_VERIFIED => {
            let ok = cert_of(o[0]).is_some_and(|c| authobj::verifier(|k, _| c.is_valid_and_verified(k)));
            num(i64::from(ok))
        }
        EE_WON_HOST_CERT_COMMIT => {
            if let Some(c) = core(o[0]) {
                secrets(&c, 4, &[], n[0], n[1]);
            }
            num(0)
        }
        // `new ElGamal`
        EE_WON_HOST_CERT_PRIV_NEW => obj(handle(Item::Key(Box::new(Mutex::new(ElGamal::new()))))),
        EE_WON_HOST_CERT_PRIV_SET => num(match item(o[0]).as_deref() {
            Some(Item::Key(k)) => i64::from(k.lock().unwrap_or_else(|e| e.into_inner()).set_private_key(data)),
            _ => 0,
        }),
        // `new AuthPeerData(cert1, block, *key, delta, cert2)`: the block's handle is the peer data's key block
        EE_WON_HOST_CERT_NEW_PEER => {
            let key = match item(Obj(n[0] as usize)).as_deref() {
                Some(Item::Key(k)) => k.lock().unwrap_or_else(|e| e.into_inner()).clone(),
                _ => ElGamal::with_random(Random::default()),
            };
            let peer = PeerData {
                cert: cert_of(o[0]),
                cert2: cert_of(o[2]),
                block: cert_of(o[1]),
                private_key: key,
                delta: n[1],
            };
            add_ref(o[1]);
            obj(handle(Item::Peer { peer: Arc::new(peer), block: if is_handle(o[1]) { o[1] } else { Obj::none() } }))
        }
        // `ctx->SetPassword(password)`: whole UTF-16 units
        EE_WON_HOST_CERT_SET_PASSWORD => {
            if let Some(c) = core(o[0]) {
                login_set(&c, EE_WON_AUTH_LOGIN_PASSWORD, &data[..data.len() / 2 * 2]);
            }
            num(0)
        }
        EE_WON_HOST_CERT_SET_PEER => {
            if let Some(c) = core(o[0]) {
                c.set_peer_data(peer_of(o[1]));
            }
            num(0)
        }
        EE_WON_HOST_CERT_CLEAR_ONE_TIME => {
            if let Some(c) = core(o[0]) {
                login_set(&c, EE_WON_AUTH_LOGIN_CLEAR_ONE_TIME, &[]);
            }
            num(0)
        }
        // the request ops' `mAuthContext`
        EE_WON_HOST_HAS_AUTH => num(i64::from(core(Obj::none()).is_some())),
        EE_WON_HOST_CERT_USER_ID => num(core(Obj::none())
            .and_then(|c| c.peer_data())
            .and_then(|p| p.cert.as_ref().map(|c| i64::from(c.user_id())))
            .unwrap_or(0)),
        EE_WON_HOST_GET_SESSION => obj(core(Obj::none()).map_or(Obj::none(), |c| Obj::of(c.get_session(addr)))),
        EE_WON_HOST_PUT_SESSION => {
            if let Some(c) = core(Obj::none()) {
                c.put_session(addr, o[0].ptr());
            }
            num(0)
        }
        // `new AuthSession(session)`: the handle owns the op's new session (an `ee_won_auth_session_new` box)
        EE_WON_HOST_PEER_NEW_SESSION => obj(if n[0] == 0 {
            Obj::none()
        } else {
            // SAFETY: the op's new session, handed over (peerop.rs)
            session_handle(*unsafe { Box::from_raw(n[0] as usize as *mut EeWonAuthSession) })
        }),
        // a new `EeWonAuthObj*` the op takes
        EE_WON_HOST_PEER_CTX_DATA => {
            num(core(Obj::none()).and_then(|c| c.peer_data()).map_or(0, |p| peerobj::peer_ref(p) as usize as i64))
        }
        EE_WON_HOST_PEER_REFRESH_BLOCK => num(core(Obj::none()).map_or(0, |c| i64::from(c.refresh_block(n[0] as u32)))),
        // `RefreshAsync(op, cert)` (INFINITE)
        EE_WON_HOST_PEER_REFRESH_ASYNC => {
            if let Some(c) = core(Obj::none()) {
                // SAFETY: the op's light member (borrowed, alive during the call) or 0
                let cert = unsafe { authflow::light_ref(n[0] as usize as *const EeWonLightCert) };
                c.refresh_async(o[0].ptr(), cert, 0xffff_ffff);
            }
            num(0)
        }
        _ => None,
    }
}

// ---- `CryptTransform` (WONRouting.h): a socket's encryption by a session handle it holds a reference to

/// `CryptTransform`'s constructor: one more reference to the session handle.
///
/// # Safety
/// `session` is a live session handle (or NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_session_hold(session: *mut c_void) {
    add_ref(Obj(session as usize))
}

/// `CryptTransform`'s destructor: its reference given back.
///
/// # Safety
/// `session` is a reference the caller held (or NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_won_session_release(session: *mut c_void) {
    release(Obj(session as usize))
}

/// `CryptTransform::Send` (`decrypt` false: `AuthSession::Encrypt`) or `Recv` (`Decrypt`) at the current time: the
/// status; the replacing message handed to `sink` when there is one (`*replaced`). Anything but a session handle
/// leaves the message as it is (the original dereferenced it).
///
/// # Safety
/// `session` is a live handle or NULL; `data` holds `len` bytes; `sink` is valid with `context`; `replaced` is
/// writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_session_crypt(
    session: *mut c_void,
    decrypt: bool,
    data: *const u8,
    len: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
    replaced: *mut bool,
) -> i32 {
    // SAFETY: the caller's flag
    unsafe { *replaced = false };
    let Some(it) = item(Obj(session as usize)) else {
        return 0;
    };
    let Item::Session(s) = &*it else {
        return 0;
    };
    // SAFETY: the caller's bytes
    let msg = unsafe { crate::ffi::bytes(data, len) };
    let (st, out) = {
        let mut s = s.lock().unwrap_or_else(|e| e.into_inner());
        if decrypt {
            s.decrypt(msg, unix_now())
        } else {
            s.encrypt(msg, unix_now())
        }
    };
    if let Some(m) = out {
        // SAFETY: the caller's sink and flag
        unsafe {
            crate::ffi::give(sink, context, &m);
            *replaced = true;
        }
    }
    st
}

#[cfg(test)]
#[path = "hostauth_tests.rs"]
pub(crate) mod tests;
