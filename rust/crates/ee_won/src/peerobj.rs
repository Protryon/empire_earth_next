//! WONAuth's certificate classes, peer data and the two sides of the peer authentication as Rust objects (round 14
//! of the Internet code): `AuthBase` with `AuthCertificate`, `Auth2Certificate` and `AuthPubKeyBlock` ([`Cert`]:
//! the bytes, the unpacked block and its keys), `AuthPeerData` ([`PeerData`]: the certificates, the key block, the
//! private key and the clock delta) and `PeerAuthClient` / `PeerAuthServer` ([`PeerAuth`]: the handshake's state,
//! the peer data, the two secrets, the other side's certificate and the session it makes). The handshake's messages
//! and checks are peerauth.rs's; here is the environment the C++ classes were ([`PeerEnv`] over these objects).
//!
//! The C++ side keeps RefCount shims holding one reference each (`AuthBase`, `AuthPeerData`; WONAuthPeerData.h),
//! so the ops' host calls and SmartPtrs hold them as before.
//!
//! The random draws are the original's, in its order: every `ElGamal()` it constructed (a certificate's member
//! key before the unpacking, the one `Adopt(NULL)` made, a key block's `ElGamal key;` per key, the empty peer
//! data's private key) seeds a generator from the system seed here too, so a run from a fixed seed
//! (`ee_won_random_test_seed`) gives the same secrets and encryptions (`golden_peerobj.txt`, recorded from the
//! original classes by tests/won/won_peerobj_test.cpp).
//!
//! The original's undefined corners, defined: a NULL second certificate (`IsValid2`, `Certificate2Expired`, the
//! server's own raw certificate) is invalid, expired and empty; a NULL key block verifies nothing; a key block
//! with no key encrypts nothing (the original read the empty list's front).
//!
//! Reached by the Internet login (GetCertOp's certificates and peer data) and by the LAN game's direct connect with
//! the CD key check on (LanLogic's peer authentication, after a certificate from the WON servers, which are gone).

use std::ffi::c_void;
use std::sync::{Arc, Mutex, MutexGuard};

use crate::auth::{AuthBlock, Body, Kind};
use crate::blowfish::Blowfish;
use crate::elgamal::ElGamal;
use crate::ffi::{
    bytes, give, EeWonByteSink, EE_WON_AUTH_CERTIFICATE, EE_WON_AUTH_CERTIFICATE2, EE_WON_AUTH_PUB_KEY_BLOCK,
};
use crate::peerauth::{self, EeWonAuthSession, EeWonPeerState, Outcome, PeerEnv};

/// `time(NULL)`.
pub(crate) fn unix_now() -> i64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs() as i64).unwrap_or(0)
}

fn lock<T>(m: &Mutex<T>) -> MutexGuard<'_, T> {
    m.lock().unwrap_or_else(|e| e.into_inner())
}

/// A certificate (version 1 or 2) or a key block: `AuthBase` and its subclass.
pub struct Cert {
    pub block: AuthBlock,
    /// `mParsed != NULL`: bytes were given
    parsed: bool,
    /// a certificate's `mPubKey`, a key block's `mKeyBlock` (their generators advance on `Encrypt`)
    keys: Mutex<Vec<ElGamal>>,
}

impl Cert {
    /// The constructor of `kind`'s class over the bytes (empty: no bytes).
    pub fn new(kind: Kind, raw: &[u8]) -> Cert {
        let is_cert = matches!(kind, Kind::Certificate | Kind::Certificate2);
        // AuthCertificateBase's member key, constructed before the constructor's Unpack
        let mut keys = if is_cert { vec![ElGamal::new()] } else { Vec::new() };
        let parsed = !raw.is_empty();
        let block = AuthBlock::unpack(kind, raw);
        if parsed {
            match &block.body {
                // UnpackHook: mPubKey.Adopt(a copy of the key, or a new empty key)
                _ if is_cert => keys[0] = block.pub_key().cloned().unwrap_or_else(ElGamal::new),
                // UnpackHook: per key `ElGamal key; key.Adopt(copy); mKeyBlock.push_back(key)`
                Body::PubKeyBlock(k) => {
                    for key in &k.keys {
                        drop(ElGamal::new());
                        keys.push(key.clone());
                    }
                }
                _ => {}
            }
        }
        Cert { block, parsed, keys: Mutex::new(keys) }
    }

    pub fn valid(&self) -> bool {
        self.block.valid
    }

    /// `IsExpired(extra)` at `now`.
    pub fn is_expired(&self, now: i64, extra: i64) -> bool {
        !self.parsed || self.block.is_expired(now, extra)
    }

    /// `GetUserId()` (a certificate's; 0 for anything else).
    pub fn user_id(&self) -> u32 {
        if self.parsed {
            self.block.user_id()
        } else {
            0
        }
    }

    /// `VerifyPermissions`.
    pub fn verify_permissions(&self, user_id: u32, community: u32, min_trust: u16) -> bool {
        self.parsed && self.block.verify_permissions(user_id, community, min_trust)
    }

    /// `GetId()` of a key block.
    pub fn block_id(&self) -> u16 {
        match &self.block.body {
            Body::PubKeyBlock(k) => k.id,
            _ => 0,
        }
    }

    /// `AuthPubKeyBlock::Verify(other)`: both unpacked and any key verifies it.
    pub fn verify(&self, other: &Cert) -> bool {
        self.parsed && other.parsed && self.block.verify_with_block(&other.block)
    }

    /// `IsValidAndVerified(key)`.
    pub fn is_valid_and_verified(&self, key: &ElGamal) -> bool {
        self.block.is_valid_and_verified(key)
    }

    /// The first key's `Encrypt` (a certificate's public key, a key block's current key).
    pub fn encrypt(&self, data: &[u8]) -> Option<Vec<u8>> {
        lock(&self.keys).first_mut().and_then(|k| k.encrypt(data))
    }

    pub fn has_key(&self) -> bool {
        !lock(&self.keys).is_empty()
    }

    fn cert2(&self) -> Option<&crate::auth::Certificate2> {
        match &self.block.body {
            Body::Certificate2(c) if self.parsed => Some(c),
            _ => None,
        }
    }
}

/// `AuthPeerData`.
pub struct PeerData {
    pub cert: Option<Arc<Cert>>,
    pub cert2: Option<Arc<Cert>>,
    pub block: Option<Arc<Cert>>,
    pub private_key: ElGamal,
    pub delta: i64,
}

impl PeerData {
    /// `AuthPeerData(keyBlock)`: empty certificates and private key, the block (a new empty one when None).
    pub fn empty(block: Option<Arc<Cert>>) -> PeerData {
        let cert = Some(Arc::new(Cert::new(Kind::Certificate, &[])));
        let cert2 = Some(Arc::new(Cert::new(Kind::Certificate2, &[])));
        let private_key = ElGamal::new();
        let block = block.or_else(|| Some(Arc::new(Cert::new(Kind::PubKeyBlock, &[]))));
        PeerData { cert, cert2, block, private_key, delta: 0 }
    }

    /// `Verify(base)`: the key block's.
    pub fn verify(&self, other: &Cert) -> bool {
        self.block.as_ref().is_some_and(|b| b.verify(other))
    }

    /// `CertificateExpired(extra)` at `now`.
    pub fn cert_expired(&self, now: i64, extra: i64) -> bool {
        self.cert.as_ref().is_none_or(|c| c.is_expired(now, extra.wrapping_add(self.delta)))
    }

    /// `Certificate2Expired(extra)` at `now`.
    pub fn cert2_expired(&self, now: i64, extra: i64) -> bool {
        self.cert2.as_ref().is_none_or(|c| c.is_expired(now, extra.wrapping_add(self.delta)))
    }

    fn valid_with(&self, cert: &Option<Arc<Cert>>) -> bool {
        self.block.as_ref().is_some_and(|b| b.valid())
            && cert.as_ref().is_some_and(|c| c.valid())
            && self.private_key.is_private()
    }

    /// `IsValid()`.
    pub fn is_valid(&self) -> bool {
        self.valid_with(&self.cert)
    }

    /// `IsValid2()`.
    pub fn is_valid2(&self) -> bool {
        self.valid_with(&self.cert2)
    }

    /// `GetExpireDelta()` at `now`: 0 when expired.
    pub fn expire_delta(&self, now: i64) -> i64 {
        if self.cert_expired(now, 0) {
            return 0;
        }
        let expire = self.cert.as_ref().map_or(0, |c| i64::from(c.block.expire_time));
        expire.wrapping_sub(self.delta).wrapping_sub(now)
    }
}

/// `PeerAuthClient` (`server` false) or `PeerAuthServer`.
pub struct PeerAuth {
    server: bool,
    pub st: EeWonPeerState,
    peer: Option<Arc<PeerData>>,
    secrets: [Blowfish; 2],
    /// the other side's certificate (`mServerCertificate` / `mClientCertificate`)
    pub other: Option<Arc<Cert>>,
    /// the session the last step made, until its owner takes it
    pub session: Option<EeWonAuthSession>,
}

impl PeerAuth {
    /// The constructor: not started, version 2 certificates; the client's 8-byte server secret.
    pub fn new(server: bool) -> PeerAuth {
        let st = EeWonPeerState {
            use_auth2: true,
            client_key_size: if server { 0 } else { 8 },
            ..EeWonPeerState::default()
        };
        PeerAuth { server, st, peer: None, secrets: [Blowfish::new(); 2], other: None, session: None }
    }

    /// `Reset()` (the client's).
    pub fn reset(&mut self) {
        self.st.state = 0;
    }

    pub fn set_use_auth2(&mut self, value: bool) {
        self.st.use_auth2 = value;
    }

    /// The client's `SetClientKeySize(size, useSecret)`.
    pub fn set_client_key_size(&mut self, size: u8, use_secret: bool) {
        self.st.client_key_size = size;
        self.st.use_client_secret = use_secret;
    }

    fn env(&mut self, now: i64) -> Env<'_> {
        Env {
            peer: self.peer.as_deref(),
            secrets: &mut self.secrets,
            other: &mut self.other,
            session: &mut self.session,
            now,
        }
    }

    /// `PeerAuthClient::Start` (the request) or `PeerAuthServer::Start` (None).
    pub fn start(&mut self, peer: Option<Arc<PeerData>>, auth_type: i32, lfs: u8, now: i64) -> Option<Vec<u8>> {
        self.peer = peer;
        let mut st = self.st;
        let out = if self.server {
            peerauth::server_start(&mut st, lfs);
            None
        } else {
            Some(peerauth::client_start(&mut st, &mut self.env(now), auth_type, lfs))
        };
        self.st = st;
        out
    }

    /// `HandleRecvMsg`: the status and the out message.
    pub fn recv(&mut self, msg: &[u8], now: i64) -> Outcome {
        let mut st = self.st;
        let server = self.server;
        let out = if server {
            peerauth::server_recv(&mut st, &mut self.env(now), msg)
        } else {
            peerauth::client_recv(&mut st, &mut self.env(now), msg)
        };
        self.st = st;
        out
    }
}

/// The handshake's environment over the objects: the original's `Pa*` statements.
struct Env<'a> {
    peer: Option<&'a PeerData>,
    secrets: &'a mut [Blowfish; 2],
    other: &'a mut Option<Arc<Cert>>,
    session: &'a mut Option<EeWonAuthSession>,
    now: i64,
}

impl PeerEnv for Env<'_> {
    fn private_decrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        self.peer?.private_key.decrypt(data)
    }
    fn make_cert(&mut self, auth2: bool, data: &[u8]) -> bool {
        let c = Arc::new(Cert::new(if auth2 { Kind::Certificate2 } else { Kind::Certificate }, data));
        let valid = c.valid();
        *self.other = Some(c);
        valid
    }
    fn verify_cert(&mut self) -> bool {
        match (self.peer, self.other.as_deref()) {
            (Some(p), Some(c)) => p.verify(c),
            _ => false,
        }
    }
    fn cert_expired(&mut self) -> bool {
        let delta = self.peer.map_or(0, |p| p.delta);
        self.other.as_ref().is_none_or(|c| c.is_expired(self.now, delta))
    }
    fn cert_encrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        self.other.as_ref()?.encrypt(data)
    }
    fn has_own_cert2(&mut self) -> bool {
        self.peer.is_some_and(|p| p.cert2.is_some())
    }
    fn own_cert_raw(&mut self, auth2: bool) -> Vec<u8> {
        let cert = self.peer.and_then(|p| if auth2 { p.cert2.as_ref() } else { p.cert.as_ref() });
        cert.map(|c| c.block.raw.clone()).unwrap_or_default()
    }
    fn secret_create(&mut self, which: u8, length: i32) {
        if length >= 0 {
            self.secrets[usize::from(which & 1)].create(length as usize);
        }
    }
    fn secret_set_key(&mut self, which: u8, key: &[u8]) -> bool {
        self.secrets[usize::from(which & 1)].set_key(key)
    }
    fn secret_key(&mut self, which: u8) -> Vec<u8> {
        self.secrets[usize::from(which & 1)].key().to_vec()
    }
    fn new_session(&mut self, auth_type: i32, id: u16, which: u8, length_field_size: u8) {
        let key = self.secrets[usize::from(which & 1)];
        *self.session = Some(EeWonAuthSession::new(auth_type, id, key, length_field_size, self.now));
    }
}

// ---- FFI

enum Held {
    Cert(Arc<Cert>),
    Peer(Arc<PeerData>),
}

/// A reference to a certificate, a key block or peer data, as the C++ shims (`AuthBase`, `AuthPeerData`) hold it.
pub struct EeWonAuthObj(Held);

fn obj(h: Held) -> *mut EeWonAuthObj {
    Box::into_raw(Box::new(EeWonAuthObj(h)))
}

/// # Safety
/// `o` is null or a live object.
unsafe fn held<'a>(o: *const EeWonAuthObj) -> Option<&'a Held> {
    if o.is_null() {
        None
    } else {
        Some(unsafe { &(*o).0 })
    }
}

/// # Safety
/// `o` is null or a live object.
unsafe fn cert_of(o: *const EeWonAuthObj) -> Option<Arc<Cert>> {
    match unsafe { held(o) } {
        Some(Held::Cert(c)) => Some(c.clone()),
        _ => None,
    }
}

/// # Safety
/// `o` is null or a live object.
pub(crate) unsafe fn peer_of(o: *const EeWonAuthObj) -> Option<Arc<PeerData>> {
    match unsafe { held(o) } {
        Some(Held::Peer(p)) => Some(p.clone()),
        _ => None,
    }
}

/// `new AuthCertificate(data, len)` (`EE_WON_AUTH_CERTIFICATE`), `Auth2Certificate` or `AuthPubKeyBlock`; another
/// kind is the base block.
///
/// # Safety
/// `data` holds `length` bytes (or is null with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_cert_new(kind: u32, data: *const u8, length: usize) -> *mut EeWonAuthObj {
    let kind = match kind {
        EE_WON_AUTH_CERTIFICATE => Kind::Certificate,
        EE_WON_AUTH_CERTIFICATE2 => Kind::Certificate2,
        EE_WON_AUTH_PUB_KEY_BLOCK => Kind::PubKeyBlock,
        _ => Kind::Base,
    };
    obj(Held::Cert(Arc::new(Cert::new(kind, unsafe { bytes(data, length) }))))
}

/// `new AuthPeerData(keyBlock)` (null: a new empty block).
///
/// # Safety
/// `block` is null or a live object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_peer_new_empty(block: *const EeWonAuthObj) -> *mut EeWonAuthObj {
    obj(Held::Peer(Arc::new(PeerData::empty(unsafe { cert_of(block) }))))
}

/// `new AuthPeerData(certificate, keyBlock, key, delta, certificate2)` (the key copied).
///
/// # Safety
/// The objects are null or live; `key` is a live key.
#[no_mangle]
pub unsafe extern "C" fn ee_won_peer_new(
    cert: *const EeWonAuthObj,
    block: *const EeWonAuthObj,
    key: *const ElGamal,
    delta: i64,
    cert2: *const EeWonAuthObj,
) -> *mut EeWonAuthObj {
    let p = PeerData {
        cert: unsafe { cert_of(cert) },
        cert2: unsafe { cert_of(cert2) },
        block: unsafe { cert_of(block) },
        private_key: unsafe { (*key).clone() },
        delta,
    };
    obj(Held::Peer(Arc::new(p)))
}

/// Another reference to the same object.
///
/// # Safety
/// `o` is a live object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authobj_clone(o: *const EeWonAuthObj) -> *mut EeWonAuthObj {
    match unsafe { &(*o).0 } {
        Held::Cert(c) => obj(Held::Cert(c.clone())),
        Held::Peer(p) => obj(Held::Peer(p.clone())),
    }
}

/// Drops a reference (null: nothing).
///
/// # Safety
/// `o` came from this module and is not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authobj_free(o: *mut EeWonAuthObj) {
    if !o.is_null() {
        drop(unsafe { Box::from_raw(o) });
    }
}

// What `ee_won_authobj_num` reads (a, b, c its arguments; times at the current time):
/// `IsValid()` (peer data: its `IsValid()`)
pub const EE_WON_AUTHOBJ_VALID: i32 = 1;
/// `GetIssueTime()`
pub const EE_WON_AUTHOBJ_ISSUE: i32 = 2;
/// `GetExpireTime()`
pub const EE_WON_AUTHOBJ_EXPIRE: i32 = 3;
/// a certificate's `GetUserId()` (peer data: its certificate's)
pub const EE_WON_AUTHOBJ_USER_ID: i32 = 4;
/// `IsExpired(a)`
pub const EE_WON_AUTHOBJ_EXPIRED: i32 = 5;
/// `VerifyPermissions(a, b, c)`
pub const EE_WON_AUTHOBJ_PERMITS: i32 = 6;
/// `Auth2Certificate::GetFirstCommunity()` (peer data: its second certificate's)
pub const EE_WON_AUTHOBJ_FIRST_COMMUNITY: i32 = 7;
/// `GetFirstCommunityTrust()`
pub const EE_WON_AUTHOBJ_FIRST_TRUST: i32 = 8;
/// `GetKeyId(a)`
pub const EE_WON_AUTHOBJ_KEY_ID: i32 = 9;
/// a key block's `GetId()`
pub const EE_WON_AUTHOBJ_BLOCK_ID: i32 = 10;
/// whether it holds a key (a key block's current key)
pub const EE_WON_AUTHOBJ_HAS_KEY: i32 = 11;
/// `ComputeAuthDelta()`: the issue time less the current time
pub const EE_WON_AUTHOBJ_AUTH_DELTA: i32 = 12;
/// peer data: `IsValid2()`
pub const EE_WON_AUTHOBJ_PEER_VALID2: i32 = 20;
/// peer data: `CertificateExpired(a)`
pub const EE_WON_AUTHOBJ_PEER_EXPIRED: i32 = 21;
/// peer data: `Certificate2Expired(a)`
pub const EE_WON_AUTHOBJ_PEER_EXPIRED2: i32 = 22;
/// peer data: `GetExpireDelta()`
pub const EE_WON_AUTHOBJ_PEER_EXPIRE_DELTA: i32 = 23;
/// peer data: `GetAuthDelta()`
pub const EE_WON_AUTHOBJ_PEER_DELTA: i32 = 24;
/// peer data: its private key's `IsPrivate()`
pub const EE_WON_AUTHOBJ_PEER_PRIVATE: i32 = 25;
/// peer data: `GetCertificate2() != NULL`
pub const EE_WON_AUTHOBJ_PEER_HAS_CERT2: i32 = 26;
// What `ee_won_authobj_get` gives of peer data:
/// `GetCertificate()`
pub const EE_WON_AUTHOBJ_PEER_CERT: i32 = 1;
/// `GetCertificate2()`
pub const EE_WON_AUTHOBJ_PEER_CERT2: i32 = 2;
/// `GetPubKeyBlock()`
pub const EE_WON_AUTHOBJ_PEER_BLOCK: i32 = 3;

fn cert_num(c: &Cert, which: i32, a: i64, b: i64, d: i64, now: i64) -> i64 {
    let c2 = c.cert2();
    match which {
        EE_WON_AUTHOBJ_VALID => i64::from(c.valid()),
        EE_WON_AUTHOBJ_ISSUE => i64::from(c.block.issue_time),
        EE_WON_AUTHOBJ_EXPIRE => i64::from(c.block.expire_time),
        EE_WON_AUTHOBJ_USER_ID => i64::from(c.user_id()),
        EE_WON_AUTHOBJ_EXPIRED => i64::from(c.is_expired(now, a)),
        EE_WON_AUTHOBJ_PERMITS => i64::from(c.verify_permissions(a as u32, b as u32, d as u16)),
        EE_WON_AUTHOBJ_FIRST_COMMUNITY => c2.map_or(0, |c| i64::from(c.first_community())),
        EE_WON_AUTHOBJ_FIRST_TRUST => c2.map_or(0, |c| i64::from(c.first_community_trust())),
        EE_WON_AUTHOBJ_KEY_ID => c2.map_or(0, |c| i64::from(c.key_id(a as u32))),
        EE_WON_AUTHOBJ_BLOCK_ID => i64::from(c.block_id()),
        EE_WON_AUTHOBJ_HAS_KEY => i64::from(c.has_key()),
        EE_WON_AUTHOBJ_AUTH_DELTA => i64::from(c.block.issue_time).wrapping_sub(now),
        _ => 0,
    }
}

fn peer_num(p: &PeerData, which: i32, a: i64, now: i64) -> i64 {
    match which {
        EE_WON_AUTHOBJ_VALID => i64::from(p.is_valid()),
        EE_WON_AUTHOBJ_USER_ID => p.cert.as_ref().map_or(0, |c| i64::from(c.user_id())),
        EE_WON_AUTHOBJ_FIRST_COMMUNITY => {
            p.cert2.as_ref().and_then(|c| c.cert2()).map_or(0, |c| i64::from(c.first_community()))
        }
        EE_WON_AUTHOBJ_PEER_VALID2 => i64::from(p.is_valid2()),
        EE_WON_AUTHOBJ_PEER_EXPIRED => i64::from(p.cert_expired(now, a)),
        EE_WON_AUTHOBJ_PEER_EXPIRED2 => i64::from(p.cert2_expired(now, a)),
        EE_WON_AUTHOBJ_PEER_EXPIRE_DELTA => p.expire_delta(now),
        EE_WON_AUTHOBJ_PEER_DELTA => p.delta,
        EE_WON_AUTHOBJ_PEER_PRIVATE => i64::from(p.private_key.is_private()),
        EE_WON_AUTHOBJ_PEER_HAS_CERT2 => i64::from(p.cert2.is_some()),
        _ => 0,
    }
}

/// A number of the object (`EE_WON_AUTHOBJ_*`), at the current time.
///
/// # Safety
/// `o` is a live object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authobj_num(o: *const EeWonAuthObj, which: i32, a: i64, b: i64, c: i64) -> i64 {
    match unsafe { &(*o).0 } {
        Held::Cert(x) => cert_num(x, which, a, b, c, unix_now()),
        Held::Peer(p) => peer_num(p, which, a, unix_now()),
    }
}

/// Peer data's certificate, second certificate or key block (`EE_WON_AUTHOBJ_PEER_*`): a new reference, null for
/// none.
///
/// # Safety
/// `o` is a live object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authobj_get(o: *const EeWonAuthObj, which: i32) -> *mut EeWonAuthObj {
    let Some(p) = (unsafe { peer_of(o) }) else { return std::ptr::null_mut() };
    let c = match which {
        EE_WON_AUTHOBJ_PEER_CERT => p.cert.clone(),
        EE_WON_AUTHOBJ_PEER_CERT2 => p.cert2.clone(),
        EE_WON_AUTHOBJ_PEER_BLOCK => p.block.clone(),
        _ => None,
    };
    c.map_or(std::ptr::null_mut(), |c| obj(Held::Cert(c)))
}

/// A key block's (or peer data's) `Verify(other)`.
///
/// # Safety
/// The objects are null or live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authobj_verify(o: *const EeWonAuthObj, other: *const EeWonAuthObj) -> bool {
    let Some(other) = (unsafe { cert_of(other) }) else { return false };
    match unsafe { held(o) } {
        Some(Held::Cert(k)) => k.verify(&other),
        Some(Held::Peer(p)) => p.verify(&other),
        None => false,
    }
}

/// `IsValidAndVerified(key)`.
///
/// # Safety
/// `o` is live; `key` is a live key.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authobj_verified_by(o: *const EeWonAuthObj, key: *const ElGamal) -> bool {
    unsafe { cert_of(o) }.is_some_and(|c| c.is_valid_and_verified(unsafe { &*key }))
}

/// The first key's `Encrypt(data)` to the sink; false (no call) when it gives NULL.
///
/// # Safety
/// `o` is live; `data` holds `length` bytes; the sink is valid with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authobj_encrypt(
    o: *const EeWonAuthObj,
    data: *const u8,
    length: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    match unsafe { cert_of(o) }.and_then(|c| c.encrypt(unsafe { bytes(data, length) })) {
        Some(v) => {
            unsafe { give(sink, context, &v) };
            true
        }
        None => false,
    }
}

/// `PeerAuthClient` (`server` false) or `PeerAuthServer`; free with [`ee_won_peerauth_free`].
pub struct EeWonPeerAuth(Mutex<PeerAuth>);

#[no_mangle]
pub extern "C" fn ee_won_peerauth_new(server: bool) -> *mut EeWonPeerAuth {
    Box::into_raw(Box::new(EeWonPeerAuth(Mutex::new(PeerAuth::new(server)))))
}

/// # Safety
/// `p` came from [`ee_won_peerauth_new`] and is not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_peerauth_free(p: *mut EeWonPeerAuth) {
    if !p.is_null() {
        drop(unsafe { Box::from_raw(p) });
    }
}

// `ee_won_peerauth_set`'s setters and `ee_won_peerauth_num`'s reads:
/// `SetUseAuth2(num)`
pub const EE_WON_PEERAUTH_USE_AUTH2: i32 = 1;
/// `SetClientKeySize(num & 0xff, num >> 8)`
pub const EE_WON_PEERAUTH_CLIENT_KEY: i32 = 2;
/// `Reset()`
pub const EE_WON_PEERAUTH_RESET: i32 = 3;
/// `GetState()`
pub const EE_WON_PEERAUTH_STATE: i32 = 4;
/// the other side's certificate: 0 none, 1 invalid, 2 valid
pub const EE_WON_PEERAUTH_OTHER: i32 = 5;
/// the other side's certificate's user id
pub const EE_WON_PEERAUTH_OTHER_USER: i32 = 6;
/// the other side's certificate's `VerifyPermissions(a, b, c)`
pub const EE_WON_PEERAUTH_OTHER_PERMITS: i32 = 7;

/// A setter (`EE_WON_PEERAUTH_*`).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_peerauth_set(p: *const EeWonPeerAuth, which: i32, num: i64) {
    let mut a = lock(unsafe { &(*p).0 });
    match which {
        EE_WON_PEERAUTH_USE_AUTH2 => a.set_use_auth2(num != 0),
        EE_WON_PEERAUTH_CLIENT_KEY => a.set_client_key_size(num as u8, (num >> 8) & 1 != 0),
        EE_WON_PEERAUTH_RESET => a.reset(),
        _ => {}
    }
}

/// A read (`EE_WON_PEERAUTH_*`).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_peerauth_num(p: *const EeWonPeerAuth, which: i32, a: i64, b: i64, c: i64) -> i64 {
    let s = lock(unsafe { &(*p).0 });
    other_num(&s, which, a, b, c)
}

pub(crate) fn other_num(s: &PeerAuth, which: i32, a: i64, b: i64, c: i64) -> i64 {
    let other = s.other.as_deref();
    match which {
        EE_WON_PEERAUTH_STATE => i64::from(s.st.state),
        EE_WON_PEERAUTH_OTHER => other.map_or(0, |o| 1 + i64::from(o.valid())),
        EE_WON_PEERAUTH_OTHER_USER => other.map_or(0, |o| i64::from(o.user_id())),
        EE_WON_PEERAUTH_OTHER_PERMITS => {
            other.map_or(0, |o| i64::from(o.verify_permissions(a as u32, b as u32, c as u16)))
        }
        _ => 0,
    }
}

/// `Start(peer, type, lfs)`: the client's request to the sink.
///
/// # Safety
/// `p` is live, `peer` null or a live object; the sink is valid with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_peerauth_start(
    p: *const EeWonPeerAuth,
    peer: *const EeWonAuthObj,
    auth_type: i32,
    lfs: u8,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let peer = unsafe { peer_of(peer) };
    let out = lock(unsafe { &(*p).0 }).start(peer, auth_type, lfs, unix_now());
    if let Some(m) = out {
        unsafe { give(sink, context, &m) };
    }
}

/// `HandleRecvMsg`: the status; the out message to the sink when there is one (`*has_out`).
///
/// # Safety
/// `p` is live; `data` holds `length` bytes; the sink is valid with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_peerauth_recv(
    p: *const EeWonPeerAuth,
    data: *const u8,
    length: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
    has_out: *mut bool,
) -> i32 {
    let (st, out) = lock(unsafe { &(*p).0 }).recv(unsafe { bytes(data, length) }, unix_now());
    unsafe { *has_out = out.is_some() };
    if let Some(m) = out {
        unsafe { give(sink, context, &m) };
    }
    st
}

/// The session the last step made (the caller owns it, an `AuthSession`'s), or null.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_peerauth_take_session(p: *const EeWonPeerAuth) -> *mut EeWonAuthSession {
    lock(unsafe { &(*p).0 }).session.take().map_or(std::ptr::null_mut(), |s| Box::into_raw(Box::new(s)))
}

#[cfg(test)]
#[path = "peerobj_tests.rs"]
mod tests;
