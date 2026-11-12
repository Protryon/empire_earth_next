//! `GetCertOp` (WONAuth/GetCertOp.cpp) as a Rust object: the op's members (the auth context, the session key, the
//! key block, the server clock delta, the peer data, the challenge seed, the new password, the create-account
//! flag, the state, the error list and the community status list) and every step of the op: `RunHook`,
//! `CleanupHook`, `Reset`, `GetNextRequest`, the three requests and `CheckResponse` with the key block reply, the
//! login challenge and the login reply (the reads are `cert.rs`'s).
//!
//! The C++ class stays a `ServerRequestOp` (its base's exchange calls these steps through its virtuals) and keeps
//! no state of its own. The objects the op holds or makes (the auth context, key blocks, certificates, peer data,
//! the private key) are C++ objects reached through [`EeWonGetCertHooks`]; the session key is this object's
//! `Blowfish` (a driver hands stand-in key hooks instead). The base class's `RunHook`, `CleanupHook` and `Finish`
//! are the C++ side's last statements, after the call: no step here calls back into the op. A read past the
//! reply's end (`ReadBufferException` in the original) comes back as `cut`, every reference released, and the
//! C++ throws.
//!
//! The behaviour is `golden_getcert.txt` (whole-op sequences over the original's statements, recorded first) and
//! `golden_cert.txt` (the reads). Reached by nothing in the game: the WON servers are gone.

// c_long is 32 bits on Windows: its casts from and to i32 are needed elsewhere
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_long, c_void};
use std::ptr;

use crate::blowfish::Blowfish;
use crate::cert::{self, EeWonCertEvent};
use crate::ffi::EeWonByteSink;

/// A C++ `RefCount*` (NULL for none).
pub(crate) type Obj = *mut c_void;

pub(crate) const GETTING_PUB_KEYS: i32 = 1;
pub(crate) const GETTING_CHALLENGE: i32 = 2;

const ST_SUCCESS: i32 = 0;
const ST_TRY_NEXT_SERVER: i32 = 100057;
const ST_SEND: i32 = 100058;
const ST_RECV: i32 = 100059;
const ST_PUB_KEY_ENCRYPT_FAILURE: i32 = 100065;
const ST_UNEXPECTED_LOGIN_CHALLENGE: i32 = 100069;
const ST_NEED_VERIFIER_KEY: i32 = 100070;
const ST_INVALID_CERTIFICATE: i32 = 100072;
const ST_INVALID_PUB_KEY_BLOCK: i32 = 100073;
const ST_DECRYPT_FAILURE: i32 = 100074;
const ST_INVALID_PRIVATE_KEY: i32 = 100075;
const ST_INVALID_SECRET_CONFIRM: i32 = 100076;
const ST_MISSING_CERTIFICATE: i32 = 100077;

/// The first certificate (`AuthCertificate`) and the second (`Auth2Certificate`): the hooks' `kind`.
pub(crate) const CERT1: i32 = 1;
pub(crate) const CERT2: i32 = 2;

/// What the op does to the C++ objects, in the original's statements. `ctx` is the auth context; every `Obj` a
/// hook returns carries one reference for the caller unless it says "borrowed".
pub(crate) trait Env {
    fn add_ref(&mut self, o: Obj);
    fn release(&mut self, o: Obj);
    /// `AuthContext::GetVerifierKey().IsPublic()`
    fn verifier_public(&mut self, ctx: Obj) -> bool;
    /// `ctx->GetPeerData()` (a reference)
    fn ctx_peer(&mut self, ctx: Obj) -> Obj;
    /// `peer->GetPubKeyBlock()` (borrowed)
    fn peer_block(&mut self, peer: Obj) -> Obj;
    /// `peer->GetAuthDelta()`
    fn peer_delta(&mut self, peer: Obj) -> i64;
    /// `ctx->LoadSecretList()`
    fn load_secrets(&mut self, ctx: Obj);
    /// the op's `GetStatus()`
    fn op_status(&mut self) -> i32;
    /// `block->IsValid()`
    fn block_valid(&mut self, block: Obj) -> bool;
    /// `ctx->SetPeerData(new AuthPeerData(block))`
    fn set_peer_from_block(&mut self, ctx: Obj, block: Obj);
    /// `block->GetId()`
    fn block_id(&mut self, block: Obj) -> u16;
    /// `block->GetCurrentKey().Encrypt(data)`: None for NULL
    fn block_encrypt(&mut self, block: Obj, data: &[u8]) -> Option<Vec<u8>>;
    /// `mSessionKey.Create(n)`
    fn key_create(&mut self, n: i32);
    /// `mSessionKey.GetKey()`, `GetKeyLen()` bytes
    fn key(&mut self) -> Vec<u8>;
    /// `mSessionKey.Encrypt`: None for NULL
    fn key_encrypt(&mut self, data: &[u8]) -> Option<Vec<u8>>;
    /// `mSessionKey.Decrypt`: None for NULL
    fn key_decrypt(&mut self, data: &[u8]) -> Option<Vec<u8>>;
    /// `ctx->GetUserName()` as UTF-16 LE bytes
    fn user(&mut self, ctx: Obj) -> Vec<u8>;
    /// `ctx->GetPassword()` as UTF-16 LE bytes
    fn password(&mut self, ctx: Obj) -> Vec<u8>;
    /// what `ctx->AppendCommunityData` appends
    fn community(&mut self, ctx: Obj) -> Vec<u8>;
    /// what `ctx->AppendHashes(seed)` appends
    fn hashes(&mut self, ctx: Obj, seed: &[u8]) -> Vec<u8>;
    /// `mRequest = new ByteBuffer(message)`
    fn set_request(&mut self, message: &[u8]);
    /// `mResponse`'s bytes
    fn response(&mut self) -> Vec<u8>;
    /// `SetLastServerError`
    fn set_last_error(&mut self, status: i32);
    /// `return InvalidReplyHeader()`
    fn invalid_header(&mut self) -> i32;
    /// `ctx->AddSecret(seed)`
    fn add_secret(&mut self, ctx: Obj, seed: &[u8]);
    /// `ctx->RemoveSecret(seed)`
    fn remove_secret(&mut self, ctx: Obj, seed: &[u8]);
    /// `new AuthCertificate(data)` (`CERT1`) or `new Auth2Certificate(data)` (`CERT2`)
    fn new_cert(&mut self, kind: i32, data: &[u8]) -> Obj;
    /// `cert->IsValid()`
    fn cert_valid(&mut self, kind: i32, cert: Obj) -> bool;
    /// `block->Verify(cert)`
    fn block_verify(&mut self, block: Obj, kind: i32, cert: Obj) -> bool;
    /// `cert->ComputeAuthDelta()`
    fn cert_delta(&mut self, kind: i32, cert: Obj) -> i64;
    /// `cert->GetIssueTime()`, `cert->GetExpireTime()`
    fn cert_times(&mut self, kind: i32, cert: Obj) -> (i64, i64);
    /// `new AuthPubKeyBlock(data)`
    fn new_block(&mut self, data: &[u8]) -> Obj;
    /// `block->IsValidAndVerified(ctx->GetVerifierKey())`
    fn block_verified(&mut self, block: Obj, ctx: Obj) -> bool;
    /// `ctx->CommitSecrets(time, duration)`
    fn commit(&mut self, ctx: Obj, time: i64, duration: i64);
    /// a local `ElGamal` (not a `RefCount`)
    fn priv_new(&mut self) -> *mut c_void;
    /// `key.SetPrivateKey(data)`
    fn priv_set(&mut self, key: *mut c_void, data: &[u8]) -> bool;
    fn priv_free(&mut self, key: *mut c_void);
    /// `new AuthPeerData(cert1, block, key, delta, cert2)`
    fn new_peer(&mut self, cert1: Obj, block: Obj, key: *mut c_void, delta: i64, cert2: Obj) -> Obj;
    /// `ctx->SetPassword(password)` (UTF-16 LE bytes)
    fn set_password(&mut self, ctx: Obj, password: &[u8]);
    /// `ctx->SetPeerData(peer)`
    fn set_peer(&mut self, ctx: Obj, peer: Obj);
    /// `ctx->ClearOneTimeData()`
    fn clear_one_time(&mut self, ctx: Obj);
}

/// `COMMUNITY_STATUS`: the name as UTF-16 LE bytes.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub(crate) struct Community {
    pub(crate) status: i16,
    pub(crate) id: u32,
    pub(crate) name: Vec<u8>,
}

/// A read past the reply's end: the C++ throws `ReadBufferException`.
#[derive(Debug, PartialEq, Eq)]
pub(crate) struct Cut;

/// The op's members (the session key apart: see [`EeWonGetCert`]).
pub(crate) struct GetCert {
    pub(crate) context: Obj,
    pub(crate) key_block: Obj,
    /// left untouched by the constructor in the original (0 here)
    pub(crate) auth_delta: i64,
    pub(crate) peer: Obj,
    pub(crate) seed: Vec<u8>,
    /// UTF-16 LE bytes
    pub(crate) new_password: Vec<u8>,
    pub(crate) create_account: bool,
    /// left untouched by the constructor in the original (`GETTING_PUB_KEYS` here; RunHook's base starts with
    /// `Reset`)
    pub(crate) state: i32,
    pub(crate) errors: Vec<Vec<u8>>,
    pub(crate) communities: Vec<Community>,
}

/// `slot = o` as a `SmartPtr` assignment: the new reference taken before the old one is let go.
fn assign<E: Env>(env: &mut E, slot: &mut Obj, o: Obj) {
    if !o.is_null() {
        env.add_ref(o);
    }
    let old = std::mem::replace(slot, o);
    if !old.is_null() {
        env.release(old);
    }
}

/// `slot = new X(...)`: `o` carries the reference the slot keeps.
fn adopt<E: Env>(env: &mut E, slot: &mut Obj, o: Obj) {
    let old = std::mem::replace(slot, o);
    if !old.is_null() {
        env.release(old);
    }
}

fn span(m: &[u8], at: u32, len: usize) -> &[u8] {
    let at = (at as usize).min(m.len());
    &m[at..(at + len).min(m.len())]
}

/// `HandleLoginReply`'s locals, released as the original's destructors ran (the key, then the certificates).
struct Locals {
    cert1: Obj,
    cert2: Obj,
    key: *mut c_void,
}

impl GetCert {
    /// `GetCertOp(context)`: `context` carries the reference the op keeps.
    pub(crate) fn new(context: Obj) -> GetCert {
        GetCert {
            context,
            key_block: ptr::null_mut(),
            auth_delta: 0,
            peer: ptr::null_mut(),
            seed: Vec::new(),
            new_password: Vec::new(),
            create_account: false,
            state: GETTING_PUB_KEYS,
            errors: Vec::new(),
            communities: Vec::new(),
        }
    }

    /// The destructor's releases.
    pub(crate) fn release_all<E: Env>(&mut self, env: &mut E) {
        for o in [&mut self.peer, &mut self.key_block, &mut self.context] {
            let old = std::mem::replace(o, ptr::null_mut());
            if !old.is_null() {
                env.release(old);
            }
        }
    }

    /// `SetAuthContext`.
    pub(crate) fn set_context<E: Env>(&mut self, env: &mut E, context: Obj) {
        assign(env, &mut self.context, context);
    }

    /// The test drivers' setup: the key block a scenario starts with.
    pub(crate) fn set_key_block<E: Env>(&mut self, env: &mut E, block: Obj) {
        assign(env, &mut self.key_block, block);
    }

    /// `RunHook`: Some(status) to `Finish` with, None to go on to `ServerRequestOp::RunHook`.
    pub(crate) fn run_hook<E: Env>(&mut self, env: &mut E) -> Option<i32> {
        if !env.verifier_public(self.context) {
            return Some(ST_NEED_VERIFIER_KEY);
        }
        let peer = env.ctx_peer(self.context);
        let block = env.peer_block(peer);
        assign(env, &mut self.key_block, block);
        self.auth_delta = env.peer_delta(peer);
        env.load_secrets(self.context);
        if !peer.is_null() {
            env.release(peer);
        }
        None
    }

    /// `CleanupHook` before `ServerRequestOp::CleanupHook` (the C++ side's next statement). A NULL key block
    /// (RunHook finished before it set one: the original dereferenced NULL) is not valid.
    pub(crate) fn cleanup_hook<E: Env>(&mut self, env: &mut E) {
        if env.op_status() != ST_SUCCESS && !self.key_block.is_null() && env.block_valid(self.key_block) {
            let peer = env.ctx_peer(self.context);
            let block = env.peer_block(peer);
            let peer_valid = !block.is_null() && env.block_valid(block);
            if !peer.is_null() {
                env.release(peer);
            }
            if !peer_valid {
                env.set_peer_from_block(self.context, self.key_block);
            }
        }
    }

    /// `Reset`.
    pub(crate) fn reset(&mut self) {
        self.state = GETTING_PUB_KEYS;
        self.seed.clear();
        self.errors.clear();
        self.communities.clear();
    }

    /// `GetNextRequest`.
    pub(crate) fn get_next_request<E: Env>(&mut self, env: &mut E) -> i32 {
        if self.key_block.is_null() || !env.block_valid(self.key_block) {
            self.send_pub_key_request(env)
        } else if self.state == GETTING_CHALLENGE {
            self.send_login_confirm(env)
        } else {
            self.send_login_request(env)
        }
    }

    fn send_pub_key_request<E: Env>(&mut self, env: &mut E) -> i32 {
        self.state = GETTING_PUB_KEYS;
        env.set_request(&cert::request(cert::EE_WON_CERT_REQ_PUBKEYS, 0, &[], &[]));
        ST_RECV
    }

    fn send_login_request<E: Env>(&mut self, env: &mut E) -> i32 {
        let key_id = env.block_id(self.key_block) as i16;
        env.key_create(8);
        let key = env.key();
        let Some(encrypted_key) = env.block_encrypt(self.key_block, &key) else {
            return ST_PUB_KEY_ENCRYPT_FAILURE;
        };
        let user = units(&env.user(self.context));
        let password = units(&env.password(self.context));
        let key_id2 = env.block_id(self.key_block) as i16;
        let first = [key.first().copied().unwrap_or(0), key.get(1).copied().unwrap_or(0)];
        let mut login =
            cert::login_data(first, key_id2, self.create_account, &user, &password, &units(&self.new_password));
        login.extend_from_slice(&env.community(self.context));
        // the original dereferenced the NULL an encryption without a key returns; empty here
        let encrypted_login = env.key_encrypt(&login).unwrap_or_default();
        env.set_request(&cert::request(cert::EE_WON_CERT_REQ_LOGIN, key_id, &encrypted_key, &encrypted_login));
        self.state = GETTING_CHALLENGE;
        ST_RECV
    }

    fn send_login_confirm<E: Env>(&mut self, env: &mut E) -> i32 {
        let block = env.hashes(self.context, &self.seed);
        let encrypted = env.key_encrypt(&block).unwrap_or_default();
        env.set_request(&cert::request(cert::EE_WON_CERT_REQ_CONFIRM, 0, &encrypted, &[]));
        ST_RECV
    }

    fn try_next<E: Env>(env: &mut E, error: i32) -> i32 {
        env.set_last_error(error);
        ST_TRY_NEXT_SERVER
    }

    /// `CheckResponse`.
    pub(crate) fn check_response<E: Env>(&mut self, env: &mut E) -> Result<i32, Cut> {
        let m = env.response();
        let ev = cert::reply(&m, self.state);
        let e = ev[0];
        match e.tag {
            cert::EE_WON_CERT_INVALID_HEADER => Ok(env.invalid_header()),
            cert::EE_WON_CERT_SERVER_ERROR => Ok(Self::try_next(env, e.num)),
            cert::EE_WON_CERT_KEY_BLOCK => {
                let block = env.new_block(span(&m, e.at, e.len as usize));
                let r = if !env.block_verified(block, self.context) {
                    Self::try_next(env, ST_INVALID_PUB_KEY_BLOCK)
                } else {
                    assign(env, &mut self.key_block, block);
                    ST_SEND
                };
                if !block.is_null() {
                    env.release(block);
                }
                Ok(r)
            }
            cert::EE_WON_CERT_UNEXPECTED_CHALLENGE => Ok(Self::try_next(env, ST_UNEXPECTED_LOGIN_CHALLENGE)),
            cert::EE_WON_CERT_CHALLENGE => self.login_challenge(env, span(&m, e.at, e.len as usize)),
            cert::EE_WON_CERT_LOGIN_STATUS => {
                let mut l = Locals { cert1: ptr::null_mut(), cert2: ptr::null_mut(), key: env.priv_new() };
                let r = self.login_reply(env, &m, &ev, &mut l);
                env.priv_free(l.key);
                for o in [l.cert2, l.cert1] {
                    if !o.is_null() {
                        env.release(o);
                    }
                }
                r
            }
            _ => Err(Cut),
        }
    }

    fn login_challenge<E: Env>(&mut self, env: &mut E, data: &[u8]) -> Result<i32, Cut> {
        let Some(d) = env.key_decrypt(data) else {
            return Ok(Self::try_next(env, ST_DECRYPT_FAILURE));
        };
        if d.len() < 16 {
            return Err(Cut);
        }
        self.seed = d[..16].to_vec();
        env.add_secret(self.context, &self.seed);
        Ok(ST_SEND)
    }

    fn login_reply<E: Env>(
        &mut self,
        env: &mut E,
        m: &[u8],
        ev: &[EeWonCertEvent],
        l: &mut Locals,
    ) -> Result<i32, Cut> {
        let status = ev[0].num;
        if status < 0 {
            if !self.seed.is_empty() {
                env.remove_secret(self.context, &self.seed);
            }
            if status == -1 {
                return Ok(Self::try_next(env, status));
            }
        }

        let mut delta = 0i64;
        let mut i = 1;
        while i < ev.len() && ev[i].tag != cert::EE_WON_CERT_REST {
            let e = ev[i];
            let data = span(m, e.at, e.len as usize);
            match e.tag {
                cert::EE_WON_CERT_CUT => return Err(Cut),
                cert::EE_WON_CERT_ERROR => self.errors.push(data.to_vec()),
                cert::EE_WON_CERT_COMMUNITY => {
                    let mut c = Community::default();
                    if e.kind >= 1 {
                        c.status = e.num as i16;
                    }
                    if e.kind >= 2 {
                        c.id = e.id;
                    }
                    if e.kind >= 3 {
                        c.name = span(m, e.at, 2 * e.len as usize).to_vec();
                    }
                    self.communities.push(c);
                }
                _ => match e.kind {
                    1 | 6 => {
                        let (kind, slot) = if e.kind == 1 { (CERT1, &mut l.cert1) } else { (CERT2, &mut l.cert2) };
                        let c = env.new_cert(kind, data);
                        adopt(env, slot, c);
                        if !env.cert_valid(kind, c) || !env.block_verify(self.key_block, kind, c) {
                            return Ok(Self::try_next(env, ST_INVALID_CERTIFICATE));
                        }
                        delta = env.cert_delta(kind, c);
                    }
                    3 => {
                        let block = env.new_block(data);
                        let ok = env.block_verified(block, self.context);
                        let r = if ok {
                            assign(env, &mut self.key_block, block);
                            None
                        } else {
                            Some(Self::try_next(env, ST_INVALID_PUB_KEY_BLOCK))
                        };
                        if !block.is_null() {
                            env.release(block);
                        }
                        if let Some(r) = r {
                            return Ok(r);
                        }
                    }
                    _ => {}
                },
            }
            i += 1;
        }
        if i == ev.len() {
            return Err(Cut);
        }
        if status < 0 {
            return Ok(status);
        }

        // the secrets committed on the auth server's clock: the times of the certificate read (the original took
        // the other one's, a NULL dereference unless both came; with both it used the first's, as here; with
        // none it passed an indeterminate duration)
        let (time, duration) = if !l.cert1.is_null() {
            let (t, x) = env.cert_times(CERT1, l.cert1);
            (t, x - t)
        } else if !l.cert2.is_null() {
            let (t, x) = env.cert_times(CERT2, l.cert2);
            (t, x - t)
        } else {
            (0, 0)
        };
        env.commit(self.context, time, duration);

        let rest = ev[i];
        let Some(d) = env.key_decrypt(span(m, rest.at, rest.len as usize)) else {
            return Ok(Self::try_next(env, ST_DECRYPT_FAILURE));
        };
        for e in cert::decrypted(&d) {
            if e.tag == cert::EE_WON_CERT_CUT {
                return Err(Cut);
            }
            let data = span(&d, e.at, e.len as usize);
            if e.kind == 2 {
                if !env.priv_set(l.key, data) {
                    return Ok(Self::try_next(env, ST_INVALID_PRIVATE_KEY));
                }
            } else if e.kind == 4 {
                let key = env.key();
                if data.len() != key.len() + 2 || data[2..] != key[..] {
                    return Ok(Self::try_next(env, ST_INVALID_SECRET_CONFIRM));
                }
            }
        }

        if l.cert1.is_null() || l.cert2.is_null() {
            return Ok(Self::try_next(env, ST_MISSING_CERTIFICATE));
        }
        let peer = env.new_peer(l.cert1, self.key_block, l.key, delta, l.cert2);
        adopt(env, &mut self.peer, peer);
        if !self.new_password.is_empty() {
            env.set_password(self.context, &self.new_password);
        }
        env.set_peer(self.context, self.peer);
        env.clear_one_time(self.context);
        Ok(ST_SUCCESS)
    }
}

fn units(b: &[u8]) -> Vec<u16> {
    b.as_chunks::<2>().0.iter().map(|c| u16::from_le_bytes(*c)).collect()
}

// ---- the C ABI

unsafe extern "C" fn vec_sink(context: *mut c_void, bytes: *const u8, length: usize) {
    let v = unsafe { &mut *(context as *mut Vec<u8>) };
    if length > 0 && !bytes.is_null() {
        v.extend_from_slice(unsafe { std::slice::from_raw_parts(bytes, length) });
    }
}

/// The C++ side of a `GetCertOp`: `context` is the op (the hooks' first argument); objects are `RefCount*`. The
/// four key hooks are NULL for the real op (the session key is the object's own `Blowfish`); a driver hands
/// stand-ins.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonGetCertHooks {
    pub context: *mut c_void,
    pub add_ref: unsafe extern "C" fn(*mut c_void),
    pub release: unsafe extern "C" fn(*mut c_void),
    pub verifier_public: unsafe extern "C" fn(*mut c_void, *mut c_void) -> bool,
    pub ctx_peer: unsafe extern "C" fn(*mut c_void, *mut c_void) -> *mut c_void,
    pub peer_block: unsafe extern "C" fn(*mut c_void, *mut c_void) -> *mut c_void,
    pub peer_delta: unsafe extern "C" fn(*mut c_void, *mut c_void) -> i64,
    pub load_secrets: unsafe extern "C" fn(*mut c_void, *mut c_void),
    pub op_status: unsafe extern "C" fn(*mut c_void) -> c_long,
    pub block_valid: unsafe extern "C" fn(*mut c_void, *mut c_void) -> bool,
    pub set_peer_from_block: unsafe extern "C" fn(*mut c_void, *mut c_void, *mut c_void),
    pub block_id: unsafe extern "C" fn(*mut c_void, *mut c_void) -> u16,
    /// false for NULL
    pub block_encrypt:
        unsafe extern "C" fn(*mut c_void, *mut c_void, *const u8, usize, EeWonByteSink, *mut c_void) -> bool,
    pub key_create: Option<unsafe extern "C" fn(*mut c_void, i32)>,
    pub key: Option<unsafe extern "C" fn(*mut c_void, EeWonByteSink, *mut c_void)>,
    pub key_encrypt: Option<unsafe extern "C" fn(*mut c_void, *const u8, usize, EeWonByteSink, *mut c_void) -> bool>,
    pub key_decrypt: Option<unsafe extern "C" fn(*mut c_void, *const u8, usize, EeWonByteSink, *mut c_void) -> bool>,
    pub user: unsafe extern "C" fn(*mut c_void, *mut c_void, EeWonByteSink, *mut c_void),
    pub password: unsafe extern "C" fn(*mut c_void, *mut c_void, EeWonByteSink, *mut c_void),
    pub community: unsafe extern "C" fn(*mut c_void, *mut c_void, EeWonByteSink, *mut c_void),
    pub hashes: unsafe extern "C" fn(*mut c_void, *mut c_void, *const u8, usize, EeWonByteSink, *mut c_void),
    pub set_request: unsafe extern "C" fn(*mut c_void, *const u8, usize),
    pub response: unsafe extern "C" fn(*mut c_void, EeWonByteSink, *mut c_void),
    pub set_last_error: unsafe extern "C" fn(*mut c_void, c_long),
    pub invalid_header: unsafe extern "C" fn(*mut c_void) -> c_long,
    pub add_secret: unsafe extern "C" fn(*mut c_void, *mut c_void, *const u8, usize),
    pub remove_secret: unsafe extern "C" fn(*mut c_void, *mut c_void, *const u8, usize),
    pub new_cert: unsafe extern "C" fn(*mut c_void, i32, *const u8, usize) -> *mut c_void,
    pub cert_valid: unsafe extern "C" fn(*mut c_void, i32, *mut c_void) -> bool,
    pub block_verify: unsafe extern "C" fn(*mut c_void, *mut c_void, i32, *mut c_void) -> bool,
    pub cert_delta: unsafe extern "C" fn(*mut c_void, i32, *mut c_void) -> i64,
    pub cert_times: unsafe extern "C" fn(*mut c_void, i32, *mut c_void, *mut i64, *mut i64),
    pub new_block: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> *mut c_void,
    pub block_verified: unsafe extern "C" fn(*mut c_void, *mut c_void, *mut c_void) -> bool,
    pub commit: unsafe extern "C" fn(*mut c_void, *mut c_void, i64, i64),
    pub priv_new: unsafe extern "C" fn(*mut c_void) -> *mut c_void,
    pub priv_set: unsafe extern "C" fn(*mut c_void, *mut c_void, *const u8, usize) -> bool,
    pub priv_free: unsafe extern "C" fn(*mut c_void, *mut c_void),
    pub new_peer:
        unsafe extern "C" fn(*mut c_void, *mut c_void, *mut c_void, *mut c_void, i64, *mut c_void) -> *mut c_void,
    pub set_password: unsafe extern "C" fn(*mut c_void, *mut c_void, *const u8, usize),
    pub set_peer: unsafe extern "C" fn(*mut c_void, *mut c_void, *mut c_void),
    pub clear_one_time: unsafe extern "C" fn(*mut c_void, *mut c_void),
}

/// `GetCertOp`'s state: the members and the session key.
pub struct EeWonGetCert {
    op: GetCert,
    key: Blowfish,
}

struct FfiEnv<'a> {
    h: EeWonGetCertHooks,
    key: &'a mut Blowfish,
}

fn collect(f: impl FnOnce(EeWonByteSink, *mut c_void)) -> Vec<u8> {
    let mut v = Vec::new();
    f(vec_sink, &mut v as *mut Vec<u8> as *mut c_void);
    v
}

fn collect_opt(f: impl FnOnce(EeWonByteSink, *mut c_void) -> bool) -> Option<Vec<u8>> {
    let mut v = Vec::new();
    f(vec_sink, &mut v as *mut Vec<u8> as *mut c_void).then_some(v)
}

impl Env for FfiEnv<'_> {
    fn add_ref(&mut self, o: Obj) {
        unsafe { (self.h.add_ref)(o) }
    }
    fn release(&mut self, o: Obj) {
        unsafe { (self.h.release)(o) }
    }
    fn verifier_public(&mut self, ctx: Obj) -> bool {
        unsafe { (self.h.verifier_public)(self.h.context, ctx) }
    }
    fn ctx_peer(&mut self, ctx: Obj) -> Obj {
        unsafe { (self.h.ctx_peer)(self.h.context, ctx) }
    }
    fn peer_block(&mut self, peer: Obj) -> Obj {
        unsafe { (self.h.peer_block)(self.h.context, peer) }
    }
    fn peer_delta(&mut self, peer: Obj) -> i64 {
        unsafe { (self.h.peer_delta)(self.h.context, peer) }
    }
    fn load_secrets(&mut self, ctx: Obj) {
        unsafe { (self.h.load_secrets)(self.h.context, ctx) }
    }
    fn op_status(&mut self) -> i32 {
        unsafe { (self.h.op_status)(self.h.context) as i32 }
    }
    fn block_valid(&mut self, block: Obj) -> bool {
        unsafe { (self.h.block_valid)(self.h.context, block) }
    }
    fn set_peer_from_block(&mut self, ctx: Obj, block: Obj) {
        unsafe { (self.h.set_peer_from_block)(self.h.context, ctx, block) }
    }
    fn block_id(&mut self, block: Obj) -> u16 {
        unsafe { (self.h.block_id)(self.h.context, block) }
    }
    fn block_encrypt(&mut self, block: Obj, data: &[u8]) -> Option<Vec<u8>> {
        let h = self.h;
        collect_opt(|s, c| unsafe { (h.block_encrypt)(h.context, block, data.as_ptr(), data.len(), s, c) })
    }
    fn key_create(&mut self, n: i32) {
        match self.h.key_create {
            Some(f) => unsafe { f(self.h.context, n) },
            None => {
                self.key.create(n.max(0) as usize);
            }
        }
    }
    fn key(&mut self) -> Vec<u8> {
        match self.h.key {
            Some(f) => collect(|s, c| unsafe { f(self.h.context, s, c) }),
            None => self.key.key().to_vec(),
        }
    }
    fn key_encrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        match self.h.key_encrypt {
            Some(f) => collect_opt(|s, c| unsafe { f(self.h.context, data.as_ptr(), data.len(), s, c) }),
            None => self.key.encrypt(data),
        }
    }
    fn key_decrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        match self.h.key_decrypt {
            Some(f) => collect_opt(|s, c| unsafe { f(self.h.context, data.as_ptr(), data.len(), s, c) }),
            None => self.key.decrypt(data),
        }
    }
    fn user(&mut self, ctx: Obj) -> Vec<u8> {
        let h = self.h;
        collect(|s, c| unsafe { (h.user)(h.context, ctx, s, c) })
    }
    fn password(&mut self, ctx: Obj) -> Vec<u8> {
        let h = self.h;
        collect(|s, c| unsafe { (h.password)(h.context, ctx, s, c) })
    }
    fn community(&mut self, ctx: Obj) -> Vec<u8> {
        let h = self.h;
        collect(|s, c| unsafe { (h.community)(h.context, ctx, s, c) })
    }
    fn hashes(&mut self, ctx: Obj, seed: &[u8]) -> Vec<u8> {
        let h = self.h;
        collect(|s, c| unsafe { (h.hashes)(h.context, ctx, seed.as_ptr(), seed.len(), s, c) })
    }
    fn set_request(&mut self, message: &[u8]) {
        unsafe { (self.h.set_request)(self.h.context, message.as_ptr(), message.len()) }
    }
    fn response(&mut self) -> Vec<u8> {
        let h = self.h;
        collect(|s, c| unsafe { (h.response)(h.context, s, c) })
    }
    fn set_last_error(&mut self, status: i32) {
        unsafe { (self.h.set_last_error)(self.h.context, status as c_long) }
    }
    fn invalid_header(&mut self) -> i32 {
        unsafe { (self.h.invalid_header)(self.h.context) as i32 }
    }
    fn add_secret(&mut self, ctx: Obj, seed: &[u8]) {
        unsafe { (self.h.add_secret)(self.h.context, ctx, seed.as_ptr(), seed.len()) }
    }
    fn remove_secret(&mut self, ctx: Obj, seed: &[u8]) {
        unsafe { (self.h.remove_secret)(self.h.context, ctx, seed.as_ptr(), seed.len()) }
    }
    fn new_cert(&mut self, kind: i32, data: &[u8]) -> Obj {
        unsafe { (self.h.new_cert)(self.h.context, kind, data.as_ptr(), data.len()) }
    }
    fn cert_valid(&mut self, kind: i32, cert: Obj) -> bool {
        unsafe { (self.h.cert_valid)(self.h.context, kind, cert) }
    }
    fn block_verify(&mut self, block: Obj, kind: i32, cert: Obj) -> bool {
        unsafe { (self.h.block_verify)(self.h.context, block, kind, cert) }
    }
    fn cert_delta(&mut self, kind: i32, cert: Obj) -> i64 {
        unsafe { (self.h.cert_delta)(self.h.context, kind, cert) }
    }
    fn cert_times(&mut self, kind: i32, cert: Obj) -> (i64, i64) {
        let (mut a, mut b) = (0, 0);
        unsafe { (self.h.cert_times)(self.h.context, kind, cert, &mut a, &mut b) };
        (a, b)
    }
    fn new_block(&mut self, data: &[u8]) -> Obj {
        unsafe { (self.h.new_block)(self.h.context, data.as_ptr(), data.len()) }
    }
    fn block_verified(&mut self, block: Obj, ctx: Obj) -> bool {
        unsafe { (self.h.block_verified)(self.h.context, block, ctx) }
    }
    fn commit(&mut self, ctx: Obj, time: i64, duration: i64) {
        unsafe { (self.h.commit)(self.h.context, ctx, time, duration) }
    }
    fn priv_new(&mut self) -> *mut c_void {
        unsafe { (self.h.priv_new)(self.h.context) }
    }
    fn priv_set(&mut self, key: *mut c_void, data: &[u8]) -> bool {
        unsafe { (self.h.priv_set)(self.h.context, key, data.as_ptr(), data.len()) }
    }
    fn priv_free(&mut self, key: *mut c_void) {
        unsafe { (self.h.priv_free)(self.h.context, key) }
    }
    fn new_peer(&mut self, cert1: Obj, block: Obj, key: *mut c_void, delta: i64, cert2: Obj) -> Obj {
        unsafe { (self.h.new_peer)(self.h.context, cert1, block, key, delta, cert2) }
    }
    fn set_password(&mut self, ctx: Obj, password: &[u8]) {
        unsafe { (self.h.set_password)(self.h.context, ctx, password.as_ptr(), password.len()) }
    }
    fn set_peer(&mut self, ctx: Obj, peer: Obj) {
        unsafe { (self.h.set_peer)(self.h.context, ctx, peer) }
    }
    fn clear_one_time(&mut self, ctx: Obj) {
        unsafe { (self.h.clear_one_time)(self.h.context, ctx) }
    }
}

/// Runs `f` on the op's members with the hooks (the session key borrowed apart).
unsafe fn with<R>(
    op: *mut EeWonGetCert,
    hooks: *const EeWonGetCertHooks,
    f: impl FnOnce(&mut GetCert, &mut FfiEnv) -> R,
) -> R {
    let o = unsafe { &mut *op };
    let mut env = FfiEnv { h: unsafe { *hooks }, key: &mut o.key };
    f(&mut o.op, &mut env)
}

unsafe fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if n == 0 || p.is_null() {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// `GetCertOp(context)`: the op keeps a reference to `context` (taken here).
///
/// # Safety
/// `hooks` is valid; `context` is a `RefCount*` or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_new(
    hooks: *const EeWonGetCertHooks,
    context: *mut c_void,
) -> *mut EeWonGetCert {
    let mut o = Box::new(EeWonGetCert { op: GetCert::new(ptr::null_mut()), key: Blowfish::new() });
    unsafe { with(&mut *o, hooks, |g, e| g.set_context(e, context)) };
    Box::into_raw(o)
}

/// The destructor: every reference released.
///
/// # Safety
/// `op` came from `ee_won_getcert_new` (or is NULL); `hooks` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_free(op: *mut EeWonGetCert, hooks: *const EeWonGetCertHooks) {
    if op.is_null() {
        return;
    }
    unsafe { with(op, hooks, |g, e| g.release_all(e)) };
    drop(unsafe { Box::from_raw(op) });
}

/// `SetAuthContext`.
///
/// # Safety
/// `op` and `hooks` are valid; `context` is a `RefCount*` or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_set_context(
    op: *mut EeWonGetCert,
    hooks: *const EeWonGetCertHooks,
    context: *mut c_void,
) {
    unsafe { with(op, hooks, |g, e| g.set_context(e, context)) }
}

/// `SetCreateAccount`.
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_set_create_account(op: *mut EeWonGetCert, value: bool) {
    unsafe { (*op).op.create_account = value }
}

/// `SetNewPassword` (UTF-16 LE bytes).
///
/// # Safety
/// `op` is valid; `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_set_new_password(op: *mut EeWonGetCert, data: *const u8, length: usize) {
    unsafe { (*op).op.new_password = bytes(data, length).to_vec() }
}

/// `RunHook`: true to `Finish(*status)`, false to go on to `ServerRequestOp::RunHook`.
///
/// # Safety
/// `op`, `hooks` and `status` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_run_hook(
    op: *mut EeWonGetCert,
    hooks: *const EeWonGetCertHooks,
    status: *mut c_long,
) -> bool {
    match unsafe { with(op, hooks, |g, e| g.run_hook(e)) } {
        Some(s) => {
            unsafe { *status = s as c_long };
            true
        }
        None => false,
    }
}

/// `CleanupHook` up to `ServerRequestOp::CleanupHook`, which the caller calls next.
///
/// # Safety
/// `op` and `hooks` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_cleanup_hook(op: *mut EeWonGetCert, hooks: *const EeWonGetCertHooks) {
    unsafe { with(op, hooks, |g, e| g.cleanup_hook(e)) }
}

/// `Reset`.
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_reset(op: *mut EeWonGetCert) {
    unsafe { (*op).op.reset() }
}

/// `GetNextRequest`.
///
/// # Safety
/// `op` and `hooks` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_get_next_request(
    op: *mut EeWonGetCert,
    hooks: *const EeWonGetCertHooks,
) -> c_long {
    unsafe { with(op, hooks, |g, e| g.get_next_request(e)) as c_long }
}

/// `CheckResponse`: `*cut` true where the original threw `ReadBufferException` (the caller throws it).
///
/// # Safety
/// `op`, `hooks` and `cut` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_check_response(
    op: *mut EeWonGetCert,
    hooks: *const EeWonGetCertHooks,
    cut: *mut bool,
) -> c_long {
    let r = unsafe { with(op, hooks, |g, e| g.check_response(e)) };
    unsafe { *cut = r.is_err() };
    r.unwrap_or(0) as c_long
}

/// `GetPeerData` (borrowed; NULL before a login).
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_peer_data(op: *const EeWonGetCert) -> *mut c_void {
    unsafe { (*op).op.peer }
}

/// `GetSessionKey`: the C++ `Blowfish` is this one `EeWonBlowfish` (pointer-interconvertible).
///
/// # Safety
/// `op` is valid; the pointer lives as long as the op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_session_key(op: *const EeWonGetCert) -> *const Blowfish {
    unsafe { &(*op).key }
}

/// The state (`GETTING_PUB_KEYS` 1, `GETTING_CHALLENGE` 2, `GETTING_CERTIFICATE` 3).
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_state(op: *const EeWonGetCert) -> i32 {
    unsafe { (*op).op.state }
}

/// The key block (borrowed).
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_key_block(op: *const EeWonGetCert) -> *mut c_void {
    unsafe { (*op).op.key_block }
}

/// The auth server's clock delta (`mAuthDelta`).
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_auth_delta(op: *const EeWonGetCert) -> i64 {
    unsafe { (*op).op.auth_delta }
}

/// The challenge seed into `sink`.
///
/// # Safety
/// `op` is valid; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_seed(op: *const EeWonGetCert, sink: EeWonByteSink, context: *mut c_void) {
    let s = unsafe { &(*op).op.seed };
    unsafe { sink(context, s.as_ptr(), s.len()) }
}

/// The number of error strings (`GetErrorList`).
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_error_count(op: *const EeWonGetCert) -> usize {
    unsafe { &*op }.op.errors.len()
}

/// Error string `index` into `sink`.
///
/// # Safety
/// `op` is valid; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_error(
    op: *const EeWonGetCert,
    index: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    if let Some(s) = unsafe { &*op }.op.errors.get(index) {
        unsafe { sink(context, s.as_ptr(), s.len()) }
    }
}

/// The number of community statuses (`GetCommunityStatusList`).
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_community_count(op: *const EeWonGetCert) -> usize {
    unsafe { &*op }.op.communities.len()
}

/// Community status `index`: the status and id, the name's UTF-16 LE bytes into `sink`.
///
/// # Safety
/// `op`, `status` and `id` are valid; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_community(
    op: *const EeWonGetCert,
    index: usize,
    status: *mut i16,
    id: *mut u32,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    if let Some(c) = unsafe { &*op }.op.communities.get(index) {
        unsafe {
            *status = c.status;
            *id = c.id;
            sink(context, c.name.as_ptr(), c.name.len());
        }
    }
}

/// The drivers' setup: the state, the challenge seed and the key block a scenario starts with.
///
/// # Safety
/// `op` and `hooks` are valid; `seed` holds `length` bytes; `block` is a `RefCount*` or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_won_getcert_setup(
    op: *mut EeWonGetCert,
    hooks: *const EeWonGetCertHooks,
    state: i32,
    seed: *const u8,
    length: usize,
    block: *mut c_void,
) {
    unsafe {
        (*op).op.state = state;
        (*op).op.seed = bytes(seed, length).to_vec();
        with(op, hooks, |g, e| g.set_key_block(e, block));
    }
}

#[cfg(test)]
#[path = "getcert_tests.rs"]
mod tests;
