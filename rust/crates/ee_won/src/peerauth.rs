//! WONAuth's peer authentication (`PeerAuthClient.cpp`, `PeerAuthServer.cpp`) and the session it leaves
//! (`AuthSession.cpp`, wholly here).
//!
//! The handshake: the client's request (auth mode, encryption, flags, its certificate), the server's challenge 1
//! (its secret B encrypted with the client's key, its certificate), the client's challenge 2 (both secrets
//! encrypted with the server's key) and the server's complete (secret A back, or an error text), each side's state,
//! its checks and status codes, and the order of every step, so a message cut short leaves what the original left
//! when its `ReadBufferException` came. The certificates, the peer data's keys, ElGamal and the two secrets'
//! `Blowfish` objects stay with the C++ (the [`PeerEnv`] hooks); the C++ classes keep their members and hand them
//! over as a [`EeWonPeerState`] for the call.
//!
//! `AuthSession` is a Rust object ([`EeWonAuthSession`]): its type, id, key, sequence numbers and last use; `Encrypt`
//! and `Decrypt` frame a service message as the original did. The C++ `AuthSession` is a `RefCount` shim owning it.
//!
//! Reached by the Internet login's peer authentication with the WON servers only (they are gone): no game, LAN game
//! or EE-SIM run reaches this; only tests/won/won_peerauth_test.cpp's driver does.

use std::ffi::c_void;

use crate::blowfish::Blowfish;
use crate::ffi::{give, EeWonByteSink};
use crate::status;
use crate::wire::{PastEnd, Reader, Writer};

/// `AuthType`.
pub(crate) const AUTH_TYPE_NONE: i32 = 0;
pub(crate) const AUTH_TYPE_SESSION: i32 = 1;
pub(crate) const AUTH_TYPE_PERSISTENT: i32 = 2;
pub(crate) const AUTH_TYPE_PERSISTENT_NOCRYPT: i32 = 3;

const SERVICE: u32 = 203;
const MSG_REQUEST: u32 = 50;
const MSG_CHALLENGE1: u32 = 51;
const MSG_CHALLENGE2: u32 = 52;
const MSG_COMPLETE: u32 = 53;

/// The statuses (WONStatus.h).
pub(crate) mod ws {
    pub(crate) const CLIENT_CHALLENGE1_DECRYPT_FAILURE: i32 = 100025;
    pub(crate) const CLIENT_CHALLENGE1_INVALID_SECRET_LEN: i32 = 100026;
    pub(crate) const CLIENT_CHALLENGE1_INVALID_SECRET_KEY: i32 = 100027;
    pub(crate) const CLIENT_CHALLENGE1_CERTIFICATE_UNPACK_FAILURE: i32 = 100028;
    pub(crate) const CLIENT_CHALLENGE1_CERTIFICATE_VERIFY_FAILURE: i32 = 100029;
    pub(crate) const CLIENT_CHALLENGE2_ENCRYPT_FAILURE: i32 = 100030;
    pub(crate) const CLIENT_COMPLETE_DECRYPT_FAILURE: i32 = 100031;
    pub(crate) const CLIENT_COMPLETE_INVALID_SECRET_LEN: i32 = 100032;
    pub(crate) const CLIENT_COMPLETE_INVALID_SECRET_KEY: i32 = 100033;
    pub(crate) const CLIENT_INVALID_SERVICE_TYPE: i32 = 100034;
    pub(crate) const CLIENT_UNEXPECTED_CHALLENGE: i32 = 100035;
    pub(crate) const CLIENT_INVALID_MESSAGE: i32 = 100036;
    pub(crate) const CLIENT_MSG_UNPACK_FAILURE: i32 = 100037;
    pub(crate) const SERVER_NOT_STARTED: i32 = 100038;
    pub(crate) const SERVER_INVALID_SERVICE_TYPE: i32 = 100039;
    pub(crate) const SERVER_UNEXPECTED_REQUEST: i32 = 100040;
    pub(crate) const SERVER_UNEXPECTED_CHALLENGE: i32 = 100041;
    pub(crate) const SERVER_INVALID_MESSAGE: i32 = 100042;
    pub(crate) const SERVER_MSG_UNPACK_FAILURE: i32 = 100043;
    pub(crate) const SERVER_INVALID_AUTH_MODE: i32 = 100044;
    pub(crate) const SERVER_INVALID_ENCRYPT_TYPE: i32 = 100045;
    pub(crate) const SERVER_INVALID_CLIENT_CERTIFICATE: i32 = 100046;
    pub(crate) const SERVER_EXPIRED_CLIENT_CERTIFICATE: i32 = 100047;
    pub(crate) const SERVER_FAILED_TO_VERIFY_CLIENT_CERTIFICATE: i32 = 100048;
    pub(crate) const SERVER_FAILED_TO_ENCRYPT_WITH_CLIENT_PUB_KEY: i32 = 100049;
    pub(crate) const SERVER_FAILED_TO_DECRYPT_WITH_PRIVATE_KEY: i32 = 100050;
    pub(crate) const SERVER_INVALID_SECRET_A: i32 = 100051;
    pub(crate) const SERVER_INVALID_SECRET_B: i32 = 100052;
    pub(crate) const SESSION_ENCRYPT_FAILURE: i32 = 100082;
    pub(crate) const SESSION_DECRYPT_SESSION_ID_MISMATCH: i32 = 100083;
    pub(crate) const SESSION_DECRYPT_FAILURE: i32 = 100084;
    pub(crate) const SESSION_DECRYPT_BAD_LEN: i32 = 100085;
    pub(crate) const SESSION_DECRYPT_INVALID_SEQUENCE_NUM: i32 = 100086;
    pub(crate) const SESSION_DECRYPT_UNPACK_FAILURE: i32 = 100087;
}

/// Secret A (the client's) or B (the server's).
pub(crate) const SECRET_A: u8 = 0;
pub(crate) const SECRET_B: u8 = 1;

/// The C++ objects the handshake works on.
pub trait PeerEnv {
    /// The peer data's private key's `Decrypt`; None where it returned NULL.
    fn private_decrypt(&mut self, data: &[u8]) -> Option<Vec<u8>>;
    /// The other side's certificate made from these bytes (`Auth2Certificate` when `auth2`, else
    /// `AuthCertificate`) and kept; its `IsValid()`.
    fn make_cert(&mut self, auth2: bool, data: &[u8]) -> bool;
    /// The peer data's `Verify` of the other side's certificate.
    fn verify_cert(&mut self) -> bool;
    /// The other side's certificate's `IsExpired(peer data's GetAuthDelta())`.
    fn cert_expired(&mut self) -> bool;
    /// The other side's certificate's public key's `Encrypt`; None where it returned NULL.
    fn cert_encrypt(&mut self, data: &[u8]) -> Option<Vec<u8>>;
    /// Whether the peer data's `GetCertificate2()` is not NULL.
    fn has_own_cert2(&mut self) -> bool;
    /// The raw bytes of the peer data's certificate (version 2 or 1); empty without any.
    fn own_cert_raw(&mut self, auth2: bool) -> Vec<u8>;
    /// A secret's `Create(length)`.
    fn secret_create(&mut self, which: u8, length: i32);
    /// A secret's `SetKey`.
    fn secret_set_key(&mut self, which: u8, key: &[u8]) -> bool;
    /// A secret's key (`GetKey`, `GetKeyLen` bytes).
    fn secret_key(&mut self, which: u8) -> Vec<u8>;
    /// `mSession = new AuthSession(type, id, secret, lengthFieldSize)`.
    fn new_session(&mut self, auth_type: i32, id: u16, which: u8, length_field_size: u8);
}

/// The members of a `PeerAuthClient` or `PeerAuthServer` the handshake reads and writes (the server has no
/// `use_client_secret` and `client_key_size`).
#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct EeWonPeerState {
    pub state: i32,
    pub auth_type: i32,
    pub length_field_size: u8,
    pub use_auth2: bool,
    pub use_client_secret: bool,
    pub client_key_size: u8,
}

/// A handler's status and its out message (`ByteBufferPtr`, None for NULL).
pub type Outcome = (i32, Option<Vec<u8>>);

fn le16(d: &[u8]) -> u16 {
    u16::from(d[0]) | (u16::from(d[1]) << 8)
}

// ---- the client

/// `PeerAuthClient::Start` (after its `mPeerData = theData`): the request.
pub fn client_start(s: &mut EeWonPeerState, env: &mut dyn PeerEnv, auth_type: i32, lfs: u8) -> Vec<u8> {
    s.state = 1;
    s.auth_type = auth_type;
    s.length_field_size = lfs;
    let sessioned = auth_type == AUTH_TYPE_SESSION;
    let encrypted = sessioned || auth_type == AUTH_TYPE_PERSISTENT;
    let mut w = Writer::new(lfs, true);
    w.append_long(SERVICE as i32);
    w.append_long(MSG_REQUEST as i32);
    w.append_byte(if sessioned { 2 } else { 1 });
    w.append_byte(u8::from(encrypted));
    let mut flags = 0i32;
    if !sessioned {
        flags |= 0x0001;
    }
    if s.use_client_secret {
        flags |= 0x8000;
    }
    w.append_short(flags as i16);
    let has2 = env.has_own_cert2();
    let raw = if !has2 || !s.use_auth2 {
        s.use_auth2 = false;
        env.own_cert_raw(false)
    } else {
        env.own_cert_raw(true)
    };
    let len = raw.len() as u16;
    w.append_short(len as i16);
    w.append_bytes(&raw[..usize::from(len).min(raw.len())]);
    w.to_message()
}

fn client_challenge1(s: &mut EeWonPeerState, env: &mut dyn PeerEnv, r: &mut Reader) -> Result<Outcome, PastEnd> {
    let len = r.read_short()? as u16;
    let enc = r.read_bytes(usize::from(len))?;
    let Some(secret) = env.private_decrypt(enc) else {
        return Ok((ws::CLIENT_CHALLENGE1_DECRYPT_FAILURE, None));
    };
    // The original read the two length bytes whatever the length (past the end of a shorter buffer, with a
    // maximum that wrapped); the port calls a shorter secret an invalid length.
    if secret.len() < 2 {
        return Ok((ws::CLIENT_CHALLENGE1_INVALID_SECRET_LEN, None));
    }
    let secret_len = usize::from(le16(&secret));
    if secret_len > secret.len() - 2 {
        return Ok((ws::CLIENT_CHALLENGE1_INVALID_SECRET_LEN, None));
    }
    env.secret_create(SECRET_A, i32::from(s.client_key_size));
    if !env.secret_set_key(SECRET_B, &secret[2..2 + secret_len]) {
        return Ok((ws::CLIENT_CHALLENGE1_INVALID_SECRET_KEY, None));
    }
    let cert_len = r.read_short()? as u16;
    let cert = r.read_bytes(usize::from(cert_len))?;
    if !env.make_cert(s.use_auth2, cert) {
        return Ok((ws::CLIENT_CHALLENGE1_CERTIFICATE_UNPACK_FAILURE, None));
    }
    if !env.verify_cert() {
        return Ok((ws::CLIENT_CHALLENGE1_CERTIFICATE_VERIFY_FAILURE, None));
    }
    Ok(client_challenge2(s, env))
}

fn client_challenge2(s: &mut EeWonPeerState, env: &mut dyn PeerEnv) -> Outcome {
    s.state = 2;
    let mut w = Writer::new(s.length_field_size, true);
    w.append_long(SERVICE as i32);
    w.append_long(MSG_CHALLENGE2 as i32);
    let b = env.secret_key(SECRET_B);
    let mut e = Writer::new(0, true);
    e.append_short(b.len() as i16);
    e.append_bytes(&b);
    e.append_bytes(&env.secret_key(SECRET_A));
    let Some(enc) = env.cert_encrypt(e.data()) else {
        return (ws::CLIENT_CHALLENGE2_ENCRYPT_FAILURE, None);
    };
    w.append_short(enc.len() as i16);
    w.append_bytes(&enc);
    (0, Some(w.to_message()))
}

fn client_complete(s: &mut EeWonPeerState, env: &mut dyn PeerEnv, r: &mut Reader) -> Result<Outcome, PastEnd> {
    let st = r.read_short()?;
    if st < 0 {
        let n = r.read_short()? as u16;
        for _ in 0..n {
            r.read_string(2)?;
        }
        return Ok((i32::from(st), None));
    }
    let len = r.read_short()? as u16;
    let enc = r.read_bytes(usize::from(len))?;
    let Some(dec) = env.private_decrypt(enc) else {
        return Ok((ws::CLIENT_COMPLETE_DECRYPT_FAILURE, None));
    };
    if dec.len() < 2 {
        return Ok((ws::CLIENT_COMPLETE_INVALID_SECRET_LEN, None));
    }
    let n = usize::from(le16(&dec));
    if n > dec.len() - 2 {
        return Ok((ws::CLIENT_COMPLETE_INVALID_SECRET_KEY, None));
    }
    let a = env.secret_key(SECRET_A);
    if n != a.len() || a[..] != dec[2..2 + n] {
        return Ok((ws::CLIENT_COMPLETE_INVALID_SECRET_KEY, None));
    }
    let mut id = 0u16;
    if s.auth_type == AUTH_TYPE_SESSION {
        id = r.read_short()? as u16;
    }
    if s.auth_type != AUTH_TYPE_PERSISTENT_NOCRYPT {
        let which = if s.use_client_secret { SECRET_A } else { SECRET_B };
        env.new_session(s.auth_type, id, which, s.length_field_size);
    }
    Ok((0, None))
}

/// `PeerAuthClient::HandleRecvMsg`: the status and the out message.
pub fn client_recv(s: &mut EeWonPeerState, env: &mut dyn PeerEnv, msg: &[u8]) -> Outcome {
    let mut r = Reader::new(msg);
    let step = (|| -> Result<Outcome, PastEnd> {
        let service = r.read_long()? as u32;
        let kind = r.read_long()? as u32;
        if service != SERVICE {
            return Ok((ws::CLIENT_INVALID_SERVICE_TYPE, None));
        }
        if kind == MSG_COMPLETE {
            client_complete(s, env, &mut r)
        } else if kind == MSG_CHALLENGE1 {
            if s.state != 1 {
                return Ok((ws::CLIENT_UNEXPECTED_CHALLENGE, None));
            }
            client_challenge1(s, env, &mut r)
        } else {
            Ok((ws::CLIENT_INVALID_MESSAGE, None))
        }
    })();
    step.unwrap_or((ws::CLIENT_MSG_UNPACK_FAILURE, None))
}

// ---- the server

/// `PeerAuthServer::Start` (after its `mPeerData = data`).
pub fn server_start(s: &mut EeWonPeerState, lfs: u8) {
    s.length_field_size = lfs;
    s.state = 1;
}

fn server_challenge1(s: &mut EeWonPeerState, env: &mut dyn PeerEnv) -> Outcome {
    let mut w = Writer::new(s.length_field_size, true);
    w.append_long(SERVICE as i32);
    w.append_long(MSG_CHALLENGE1 as i32);
    env.secret_create(SECRET_B, 8);
    let b = env.secret_key(SECRET_B);
    let mut e = Writer::new(0, true);
    e.append_short(b.len() as i16);
    e.append_bytes(&b);
    let Some(enc) = env.cert_encrypt(e.data()) else {
        return (ws::SERVER_FAILED_TO_ENCRYPT_WITH_CLIENT_PUB_KEY, None);
    };
    w.append_short(enc.len() as i16);
    w.append_bytes(&enc);
    let raw = env.own_cert_raw(s.use_auth2);
    w.append_buffer(Some(&raw), 2);
    (0, Some(w.to_message()))
}

fn server_request(s: &mut EeWonPeerState, env: &mut dyn PeerEnv, r: &mut Reader) -> Result<Outcome, PastEnd> {
    let mode = r.read_byte()?;
    let encrypt = r.read_byte()?;
    r.read_short()?;
    if mode != 1 {
        return Ok((ws::SERVER_INVALID_AUTH_MODE, None));
    }
    if encrypt != 0 && encrypt != 1 {
        return Ok((ws::SERVER_INVALID_ENCRYPT_TYPE, None));
    }
    s.auth_type = if encrypt == 1 { AUTH_TYPE_PERSISTENT } else { AUTH_TYPE_PERSISTENT_NOCRYPT };
    let len = r.read_short()? as u16;
    let cert = r.read_bytes(usize::from(len))?;
    if !env.make_cert(s.use_auth2, cert) {
        return Ok((ws::SERVER_INVALID_CLIENT_CERTIFICATE, None));
    }
    if env.cert_expired() {
        return Ok((ws::SERVER_EXPIRED_CLIENT_CERTIFICATE, None));
    }
    if !env.verify_cert() {
        return Ok((ws::SERVER_FAILED_TO_VERIFY_CLIENT_CERTIFICATE, None));
    }
    Ok(server_challenge1(s, env))
}

/// `PeerAuthServer::GetComplete`: the complete message for a status (its name as the error text), or for success
/// secret A encrypted with the client's key and the session made.
pub fn server_complete(s: &mut EeWonPeerState, env: &mut dyn PeerEnv, st: i32) -> Outcome {
    s.state = 0;
    let mut w = Writer::new(s.length_field_size, true);
    w.append_long(SERVICE as i32);
    w.append_long(MSG_COMPLETE as i32);
    if st != 0 {
        w.append_short(-4);
        w.append_short(1);
        w.append_string(status::name(st as status::Status).as_bytes(), 2);
    } else {
        w.append_short(0);
        let a = env.secret_key(SECRET_A);
        let mut e = Writer::new(0, true);
        e.append_short(a.len() as i16);
        e.append_bytes(&a);
        let Some(enc) = env.cert_encrypt(e.data()) else {
            return (ws::SERVER_FAILED_TO_ENCRYPT_WITH_CLIENT_PUB_KEY, None);
        };
        w.append_short(enc.len() as i16);
        w.append_bytes(&enc);
        env.new_session(s.auth_type, 0, SECRET_B, s.length_field_size);
    }
    (0, Some(w.to_message()))
}

fn server_challenge2(s: &mut EeWonPeerState, env: &mut dyn PeerEnv, r: &mut Reader) -> Result<Outcome, PastEnd> {
    let len = r.read_short()? as u16;
    let enc = r.read_bytes(usize::from(len))?;
    let Some(dec) = env.private_decrypt(enc) else {
        return Ok((ws::SERVER_FAILED_TO_DECRYPT_WITH_PRIVATE_KEY, None));
    };
    let mut m = Reader::new(&dec);
    let b_len = m.read_short()? as u16;
    let b = env.secret_key(SECRET_B);
    if usize::from(b_len) != b.len() || m.read_bytes(usize::from(b_len))? != &b[..] {
        return Ok((ws::SERVER_INVALID_SECRET_B, None));
    }
    if !env.secret_set_key(SECRET_A, &dec[m.pos()..]) {
        return Ok((ws::SERVER_INVALID_SECRET_A, None));
    }
    Ok(server_complete(s, env, 0))
}

/// `PeerAuthServer::HandleRecvMsg`: the status and the out message (a failure's complete message when the
/// handler failed past the header checks).
pub fn server_recv(s: &mut EeWonPeerState, env: &mut dyn PeerEnv, msg: &[u8]) -> Outcome {
    if s.state == 0 {
        return (ws::SERVER_NOT_STARTED, None);
    }
    let mut r = Reader::new(msg);
    // Ok(Err(..)) is a return out of the try block without the failure's complete message
    let step = (|| -> Result<Result<Outcome, Outcome>, PastEnd> {
        let service = r.read_long()? as u32;
        let kind = r.read_long()? as u32;
        if service != SERVICE {
            return Ok(Err((ws::SERVER_INVALID_SERVICE_TYPE, None)));
        }
        if kind == MSG_REQUEST {
            if s.state != 1 {
                return Ok(Err((ws::SERVER_UNEXPECTED_REQUEST, None)));
            }
            let o = server_request(s, env, &mut r)?;
            s.state = 2;
            Ok(Ok(o))
        } else if kind == MSG_CHALLENGE2 {
            if s.state != 2 {
                return Ok(Err((ws::SERVER_UNEXPECTED_CHALLENGE, None)));
            }
            Ok(Ok(server_challenge2(s, env, &mut r)?))
        } else {
            Ok(Err((ws::SERVER_INVALID_MESSAGE, None)))
        }
    })();
    let (st, out) = match step {
        Ok(Err(o)) => return o,
        Ok(Ok(o)) => o,
        Err(PastEnd) => (ws::SERVER_MSG_UNPACK_FAILURE, None),
    };
    if st != 0 {
        let (_, complete) = server_complete(s, env, st);
        // GetComplete's own failure leaves the out message as it was
        return (st, complete.or(out));
    }
    (st, out)
}

// ---- AuthSession

/// `AuthSession` (0x10cc bytes in the original): the session's type, id, key, sequence numbers and last use.
#[derive(Clone)]
pub struct EeWonAuthSession {
    pub auth_type: i32,
    pub length_field_size: u8,
    pub id: u16,
    pub in_seq: u16,
    pub out_seq: u16,
    pub key: Blowfish,
    pub last_use: i64,
}

impl EeWonAuthSession {
    /// The constructor (`now` is its `time(NULL)`).
    pub fn new(auth_type: i32, id: u16, key: Blowfish, length_field_size: u8, now: i64) -> EeWonAuthSession {
        EeWonAuthSession { auth_type, length_field_size, id, in_seq: 0, out_seq: 0, key, last_use: now }
    }

    fn plain(&self) -> bool {
        self.auth_type == AUTH_TYPE_NONE || self.auth_type == AUTH_TYPE_PERSISTENT_NOCRYPT
    }

    /// `Encrypt`: the status and the replacing message (None: the message stays).
    pub fn encrypt(&mut self, msg: &[u8], now: i64) -> (i32, Option<Vec<u8>>) {
        self.last_use = now;
        if self.plain() {
            return (0, None);
        }
        let mut w = Writer::new(self.length_field_size, true);
        w.append_byte(12);
        let mut offset = usize::from(self.length_field_size);
        if offset == 2 && msg.len() >= 0xffff {
            offset = 6;
        }
        let body = &msg[offset.min(msg.len())..];
        let mut seq = Writer::new(0, true);
        let data = if self.auth_type == AUTH_TYPE_SESSION {
            w.append_short(self.id as i16);
            self.out_seq = self.out_seq.wrapping_add(1);
            seq.append_short(self.out_seq as i16);
            seq.append_bytes(body);
            seq.data()
        } else {
            body
        };
        let Some(enc) = self.key.encrypt(data) else {
            return (ws::SESSION_ENCRYPT_FAILURE, None);
        };
        w.append_bytes(&enc);
        (0, Some(w.to_message()))
    }

    /// `Decrypt`: the status and the replacing message (None: the message stays).
    pub fn decrypt(&mut self, msg: &[u8], now: i64) -> (i32, Option<Vec<u8>>) {
        self.last_use = now;
        if self.plain() {
            return (0, None);
        }
        let mut r = Reader::new(msg);
        let step = (|| -> Result<(i32, Option<Vec<u8>>), PastEnd> {
            let mut out = Writer::new(0, true);
            match r.read_byte()? {
                2 | 12 => {}
                4 => out.append_byte(3),
                6 => out.append_byte(5),
                8 => out.append_byte(7),
                _ => return Ok((0, None)),
            }
            let sessioned = self.auth_type == AUTH_TYPE_SESSION;
            if sessioned && r.read_short()? as u16 != self.id {
                return Ok((ws::SESSION_DECRYPT_SESSION_ID_MISMATCH, None));
            }
            let Some(dec) = self.key.decrypt(&msg[r.pos()..]) else {
                return Ok((ws::SESSION_DECRYPT_FAILURE, None));
            };
            if sessioned {
                if dec.len() < 2 {
                    return Ok((ws::SESSION_DECRYPT_BAD_LEN, None));
                }
                self.in_seq = self.in_seq.wrapping_add(1);
                if self.in_seq != le16(&dec) {
                    return Ok((ws::SESSION_DECRYPT_INVALID_SEQUENCE_NUM, None));
                }
                out.append_bytes(&dec[2..]);
            } else {
                out.append_bytes(&dec);
            }
            Ok((0, Some(out.to_message())))
        })();
        step.unwrap_or((ws::SESSION_DECRYPT_UNPACK_FAILURE, None))
    }
}

// ---- FFI

/// The C++ objects behind [`PeerEnv`]: each hook gets `context`; a sink hook hands its bytes to the sink and
/// returns false where the original's pointer was NULL.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonPeerHooks {
    pub context: *mut c_void,
    pub private_decrypt: unsafe extern "C" fn(*mut c_void, *const u8, usize, EeWonByteSink, *mut c_void) -> bool,
    pub make_cert: unsafe extern "C" fn(*mut c_void, bool, *const u8, usize) -> bool,
    pub verify_cert: unsafe extern "C" fn(*mut c_void) -> bool,
    pub cert_expired: unsafe extern "C" fn(*mut c_void) -> bool,
    pub cert_encrypt: unsafe extern "C" fn(*mut c_void, *const u8, usize, EeWonByteSink, *mut c_void) -> bool,
    pub has_own_cert2: unsafe extern "C" fn(*mut c_void) -> bool,
    pub own_cert_raw: unsafe extern "C" fn(*mut c_void, bool, EeWonByteSink, *mut c_void),
    pub secret_create: unsafe extern "C" fn(*mut c_void, u8, i32),
    pub secret_set_key: unsafe extern "C" fn(*mut c_void, u8, *const u8, usize) -> bool,
    pub secret_key: unsafe extern "C" fn(*mut c_void, u8, EeWonByteSink, *mut c_void),
    pub new_session: unsafe extern "C" fn(*mut c_void, i32, u16, u8, u8),
}

unsafe extern "C" fn vec_sink(context: *mut c_void, bytes: *const u8, length: usize) {
    let v = unsafe { &mut *context.cast::<Vec<u8>>() };
    v.clear();
    if length > 0 {
        v.extend_from_slice(unsafe { std::slice::from_raw_parts(bytes, length) });
    }
}

struct Hooked<'a>(&'a EeWonPeerHooks);

impl Hooked<'_> {
    fn sunk(&self, f: impl FnOnce(EeWonByteSink, *mut c_void) -> bool) -> Option<Vec<u8>> {
        let mut v = Vec::new();
        if f(vec_sink, (&mut v as *mut Vec<u8>).cast()) {
            Some(v)
        } else {
            None
        }
    }
}

impl PeerEnv for Hooked<'_> {
    fn private_decrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        let h = self.0;
        self.sunk(|s, c| unsafe { (h.private_decrypt)(h.context, data.as_ptr(), data.len(), s, c) })
    }
    fn make_cert(&mut self, auth2: bool, data: &[u8]) -> bool {
        unsafe { (self.0.make_cert)(self.0.context, auth2, data.as_ptr(), data.len()) }
    }
    fn verify_cert(&mut self) -> bool {
        unsafe { (self.0.verify_cert)(self.0.context) }
    }
    fn cert_expired(&mut self) -> bool {
        unsafe { (self.0.cert_expired)(self.0.context) }
    }
    fn cert_encrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        let h = self.0;
        self.sunk(|s, c| unsafe { (h.cert_encrypt)(h.context, data.as_ptr(), data.len(), s, c) })
    }
    fn has_own_cert2(&mut self) -> bool {
        unsafe { (self.0.has_own_cert2)(self.0.context) }
    }
    fn own_cert_raw(&mut self, auth2: bool) -> Vec<u8> {
        let h = self.0;
        self.sunk(|s, c| {
            unsafe { (h.own_cert_raw)(h.context, auth2, s, c) };
            true
        })
        .unwrap_or_default()
    }
    fn secret_create(&mut self, which: u8, length: i32) {
        unsafe { (self.0.secret_create)(self.0.context, which, length) }
    }
    fn secret_set_key(&mut self, which: u8, key: &[u8]) -> bool {
        unsafe { (self.0.secret_set_key)(self.0.context, which, key.as_ptr(), key.len()) }
    }
    fn secret_key(&mut self, which: u8) -> Vec<u8> {
        let h = self.0;
        self.sunk(|s, c| {
            unsafe { (h.secret_key)(h.context, which, s, c) };
            true
        })
        .unwrap_or_default()
    }
    fn new_session(&mut self, auth_type: i32, id: u16, which: u8, length_field_size: u8) {
        unsafe { (self.0.new_session)(self.0.context, auth_type, id, which, length_field_size) }
    }
}

unsafe fn give_out(out: Option<Vec<u8>>, sink: EeWonByteSink, context: *mut c_void) -> bool {
    match out {
        Some(v) => {
            unsafe { give(sink, context, &v) };
            true
        }
        None => false,
    }
}

unsafe fn msg<'a>(data: *const u8, len: usize) -> &'a [u8] {
    if data.is_null() || len == 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(data, len) }
    }
}

/// [`client_start`]: the request handed to `sink`.
///
/// # Safety
/// The pointers are valid; the hooks are callable with their context.
#[no_mangle]
pub unsafe extern "C" fn ee_won_peer_client_start(
    s: *mut EeWonPeerState,
    hooks: *const EeWonPeerHooks,
    auth_type: i32,
    lfs: u8,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let out = client_start(unsafe { &mut *s }, &mut Hooked(unsafe { &*hooks }), auth_type, lfs);
    unsafe { give(sink, context, &out) };
}

/// [`client_recv`]: the status; the out message handed to `sink` when there is one (`*has_out`).
///
/// # Safety
/// The pointers are valid (`data` holds `len` bytes); the hooks are callable with their context.
#[no_mangle]
pub unsafe extern "C" fn ee_won_peer_client_recv(
    s: *mut EeWonPeerState,
    hooks: *const EeWonPeerHooks,
    data: *const u8,
    len: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
    has_out: *mut bool,
) -> i32 {
    let (st, out) = client_recv(unsafe { &mut *s }, &mut Hooked(unsafe { &*hooks }), unsafe { msg(data, len) });
    unsafe { *has_out = give_out(out, sink, context) };
    st
}

/// [`server_start`].
///
/// # Safety
/// `s` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_peer_server_start(s: *mut EeWonPeerState, lfs: u8) {
    server_start(unsafe { &mut *s }, lfs);
}

/// [`server_recv`], as [`ee_won_peer_client_recv`].
///
/// # Safety
/// As [`ee_won_peer_client_recv`].
#[no_mangle]
pub unsafe extern "C" fn ee_won_peer_server_recv(
    s: *mut EeWonPeerState,
    hooks: *const EeWonPeerHooks,
    data: *const u8,
    len: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
    has_out: *mut bool,
) -> i32 {
    let (st, out) = server_recv(unsafe { &mut *s }, &mut Hooked(unsafe { &*hooks }), unsafe { msg(data, len) });
    unsafe { *has_out = give_out(out, sink, context) };
    st
}

/// A new session (the C++ `AuthSession` owns it; free with [`ee_won_auth_session_free`]).
///
/// # Safety
/// `key` is a valid Blowfish.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_session_new(
    auth_type: i32,
    id: u16,
    key: *const Blowfish,
    lfs: u8,
    now: i64,
) -> *mut EeWonAuthSession {
    Box::into_raw(Box::new(EeWonAuthSession::new(auth_type, id, unsafe { *key }, lfs, now)))
}

/// Frees a session.
///
/// # Safety
/// `s` came from [`ee_won_auth_session_new`] and is not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_session_free(s: *mut EeWonAuthSession) {
    if !s.is_null() {
        drop(unsafe { Box::from_raw(s) });
    }
}

/// A session's field: 0 type, 1 id, 2 last use, 3 length field size, 4 in sequence, 5 out sequence.
///
/// # Safety
/// `s` is a live session.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_session_get(s: *const EeWonAuthSession, field: i32) -> i64 {
    let s = unsafe { &*s };
    match field {
        0 => i64::from(s.auth_type),
        1 => i64::from(s.id),
        2 => s.last_use,
        3 => i64::from(s.length_field_size),
        4 => i64::from(s.in_seq),
        _ => i64::from(s.out_seq),
    }
}

/// `SetLengthFieldSize`.
///
/// # Safety
/// `s` is a live session.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_session_set_lfs(s: *mut EeWonAuthSession, lfs: u8) {
    unsafe { (*s).length_field_size = lfs };
}

/// The session's key (`GetKey`), valid while the session lives.
///
/// # Safety
/// `s` is a live session.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_session_key(s: *const EeWonAuthSession) -> *const Blowfish {
    unsafe { &(*s).key }
}

/// `Encrypt` (`decrypt` false) or `Decrypt`: the status; the replacing message handed to `sink` when there is one
/// (`*replaced`).
///
/// # Safety
/// `s` is a live session; `data` holds `len` bytes; `sink` is valid with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_session_crypt(
    s: *mut EeWonAuthSession,
    decrypt: bool,
    data: *const u8,
    len: usize,
    now: i64,
    sink: EeWonByteSink,
    context: *mut c_void,
    replaced: *mut bool,
) -> i32 {
    let s = unsafe { &mut *s };
    let m = unsafe { msg(data, len) };
    let (st, out) = if decrypt { s.decrypt(m, now) } else { s.encrypt(m, now) };
    unsafe { *replaced = give_out(out, sink, context) };
    st
}

#[cfg(test)]
#[path = "peerauth_tests.rs"]
mod tests;
