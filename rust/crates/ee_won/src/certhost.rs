//! `GetCertOp` on the Rust op base (round 10): getcert.rs's op (its members and every step, as round 6 moved them)
//! as a [`HostRequest`] of the request exchange (reqhost.rs), so the C++ class is a `RustServerOp` with two
//! accessors and no hook table. Every C++ object the op touches (the auth context, key blocks, certificates, peer
//! data, the private key) is one host call per statement of round 6's hooks ([`HostEnv`]); the op's own members
//! (the request, the reply, the server error) are the exchange's and the session key is this object's `Blowfish`
//! ([`CertSide`]).
//!
//! The behaviour is `golden_getcert.txt` again: `tests::golden_through_the_base` replays it with every object
//! statement a host call answered by the replay's stand-in. Reached by nothing in the game but an Internet login
//! (the servers are gone).

use std::ffi::c_void;

use crate::blowfish::Blowfish;
use crate::getcert::{Cut, Env, GetCert, Obj as CObj};
use crate::hostop::{
    EeWonHostOp, Obj, SetValue, EE_WON_HOST_CERT_ADD_SECRET, EE_WON_HOST_CERT_BLOCK_ENCRYPT, EE_WON_HOST_CERT_BLOCK_ID,
    EE_WON_HOST_CERT_BLOCK_VALID, EE_WON_HOST_CERT_BLOCK_VERIFIED, EE_WON_HOST_CERT_BLOCK_VERIFY,
    EE_WON_HOST_CERT_CLEAR_ONE_TIME, EE_WON_HOST_CERT_COMMIT, EE_WON_HOST_CERT_COMMUNITY, EE_WON_HOST_CERT_CTX_PEER,
    EE_WON_HOST_CERT_DELTA, EE_WON_HOST_CERT_HASHES, EE_WON_HOST_CERT_LOAD_SECRETS, EE_WON_HOST_CERT_NEW,
    EE_WON_HOST_CERT_NEW_BLOCK, EE_WON_HOST_CERT_NEW_PEER, EE_WON_HOST_CERT_PASSWORD, EE_WON_HOST_CERT_PEER_BLOCK,
    EE_WON_HOST_CERT_PEER_DELTA, EE_WON_HOST_CERT_PRIV_FREE, EE_WON_HOST_CERT_PRIV_NEW, EE_WON_HOST_CERT_PRIV_SET,
    EE_WON_HOST_CERT_REMOVE_SECRET, EE_WON_HOST_CERT_SET_PASSWORD, EE_WON_HOST_CERT_SET_PEER,
    EE_WON_HOST_CERT_SET_PEER_FROM_BLOCK, EE_WON_HOST_CERT_TIMES, EE_WON_HOST_CERT_USER, EE_WON_HOST_CERT_VALID,
    EE_WON_HOST_CERT_VERIFIER_PUBLIC,
};
use crate::reqhost::{set_last_server_error, Calls, HostRequest, ReqPlan};
use crate::reqop::Checked;
use crate::server::st;

/// `GetPeerData()` (a borrowed `RefCount*`, NULL before a login): `ee_won_hostop_num`
pub const EE_WON_GETCERT_PEER: i32 = 1;
/// `GetSessionKey()` (the `Blowfish`, as round 6's `ee_won_getcert_session_key`): `ee_won_hostop_num`
pub const EE_WON_GETCERT_SESSION_KEY: i32 = 2;
/// the number of error strings: `ee_won_hostop_num`; string `index`: `ee_won_hostop_bytes`
pub const EE_WON_GETCERT_ERRORS: i32 = 3;
/// the number of community statuses: `ee_won_hostop_num`; status `index`'s name (UTF-16 LE): `ee_won_hostop_bytes`
pub const EE_WON_GETCERT_COMMUNITIES: i32 = 4;
/// community status `index`'s status: `ee_won_hostop_num`
pub const EE_WON_GETCERT_COMMUNITY_STATUS: i32 = 5;
/// community status `index`'s id: `ee_won_hostop_num`
pub const EE_WON_GETCERT_COMMUNITY_ID: i32 = 6;
/// `SetCreateAccount(num)`: `ee_won_hostop_set`
pub const EE_WON_GETCERT_CREATE_ACCOUNT: i32 = 7;
/// `SetNewPassword(data)` (UTF-16 LE): `ee_won_hostop_set`
pub const EE_WON_GETCERT_NEW_PASSWORD: i32 = 8;
/// `SetAuthContext(obj)` (the op takes its own reference): `ee_won_hostop_set`
pub const EE_WON_GETCERT_AUTH_CONTEXT: i32 = 9;

fn o(p: CObj) -> Obj {
    Obj::of(p)
}

/// The op's own side of its environment: the members the exchange keeps and the session key.
pub(crate) trait CertSide {
    fn op_status(&mut self, c: &Calls) -> i32;
    fn set_request(&mut self, c: &Calls, message: &[u8]);
    fn response(&mut self, c: &Calls) -> Vec<u8>;
    fn set_last_error(&mut self, c: &Calls, status: i32);
    fn invalid_header(&mut self, c: &Calls) -> i32;
    fn key_create(&mut self, n: i32);
    fn key(&mut self) -> Vec<u8>;
    fn key_encrypt(&mut self, data: &[u8]) -> Option<Vec<u8>>;
    fn key_decrypt(&mut self, data: &[u8]) -> Option<Vec<u8>>;
}

/// The real side: the exchange's members, the op's `Blowfish`.
pub(crate) struct ExSide<'k>(pub(crate) &'k mut Blowfish);

impl CertSide for ExSide<'_> {
    fn op_status(&mut self, c: &Calls) -> i32 {
        c.h.status()
    }
    fn set_request(&mut self, c: &Calls, message: &[u8]) {
        c.h.st().ex.request = Some(message.to_vec());
    }
    fn response(&mut self, c: &Calls) -> Vec<u8> {
        c.h.st().ex.response.clone().unwrap_or_default()
    }
    fn set_last_error(&mut self, c: &Calls, status: i32) {
        set_last_server_error(c.h, status)
    }
    fn invalid_header(&mut self, c: &Calls) -> i32 {
        set_last_server_error(c.h, st::INVALID_REPLY_HEADER);
        st::TRY_NEXT_SERVER
    }
    fn key_create(&mut self, n: i32) {
        self.0.create(n.max(0) as usize);
    }
    fn key(&mut self) -> Vec<u8> {
        self.0.key().to_vec()
    }
    fn key_encrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        self.0.encrypt(data)
    }
    fn key_decrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        self.0.decrypt(data)
    }
}

/// getcert.rs's environment over the host: one call per statement of round 6's hooks.
pub(crate) struct HostEnv<'a, 'b, S> {
    pub(crate) c: &'a Calls<'b>,
    pub(crate) side: S,
}

impl<S> HostEnv<'_, '_, S> {
    fn num(&self, code: i32, obj: [CObj; 3], num: [i64; 2], data: &[u8]) -> i64 {
        self.c.h.call(code, [o(obj[0]), o(obj[1]), o(obj[2])], num, data, [0; 6]).num
    }
    fn obj(&self, code: i32, obj: [CObj; 3], num: [i64; 2], data: &[u8]) -> CObj {
        self.c.h.call(code, [o(obj[0]), o(obj[1]), o(obj[2])], num, data, [0; 6]).obj.ptr()
    }
    fn bytes(&self, code: i32, obj: CObj, data: &[u8]) -> Option<Vec<u8>> {
        self.c.h.call(code, [o(obj), Obj::none(), Obj::none()], [0, 0], data, [0; 6]).bytes
    }
}

const N: CObj = std::ptr::null_mut();

impl<S: CertSide> Env for HostEnv<'_, '_, S> {
    fn add_ref(&mut self, p: CObj) {
        self.c.h.add_ref(o(p))
    }
    fn release(&mut self, p: CObj) {
        self.c.h.release(o(p))
    }
    fn verifier_public(&mut self, ctx: CObj) -> bool {
        self.num(EE_WON_HOST_CERT_VERIFIER_PUBLIC, [ctx, N, N], [0, 0], &[]) != 0
    }
    fn ctx_peer(&mut self, ctx: CObj) -> CObj {
        self.obj(EE_WON_HOST_CERT_CTX_PEER, [ctx, N, N], [0, 0], &[])
    }
    fn peer_block(&mut self, peer: CObj) -> CObj {
        self.obj(EE_WON_HOST_CERT_PEER_BLOCK, [peer, N, N], [0, 0], &[])
    }
    fn peer_delta(&mut self, peer: CObj) -> i64 {
        self.num(EE_WON_HOST_CERT_PEER_DELTA, [peer, N, N], [0, 0], &[])
    }
    fn load_secrets(&mut self, ctx: CObj) {
        self.num(EE_WON_HOST_CERT_LOAD_SECRETS, [ctx, N, N], [0, 0], &[]);
    }
    fn op_status(&mut self) -> i32 {
        self.side.op_status(self.c)
    }
    fn block_valid(&mut self, block: CObj) -> bool {
        self.num(EE_WON_HOST_CERT_BLOCK_VALID, [block, N, N], [0, 0], &[]) != 0
    }
    fn set_peer_from_block(&mut self, ctx: CObj, block: CObj) {
        self.num(EE_WON_HOST_CERT_SET_PEER_FROM_BLOCK, [ctx, block, N], [0, 0], &[]);
    }
    fn block_id(&mut self, block: CObj) -> u16 {
        self.num(EE_WON_HOST_CERT_BLOCK_ID, [block, N, N], [0, 0], &[]) as u16
    }
    fn block_encrypt(&mut self, block: CObj, data: &[u8]) -> Option<Vec<u8>> {
        self.bytes(EE_WON_HOST_CERT_BLOCK_ENCRYPT, block, data)
    }
    fn key_create(&mut self, n: i32) {
        self.side.key_create(n)
    }
    fn key(&mut self) -> Vec<u8> {
        self.side.key()
    }
    fn key_encrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        self.side.key_encrypt(data)
    }
    fn key_decrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        self.side.key_decrypt(data)
    }
    fn user(&mut self, ctx: CObj) -> Vec<u8> {
        self.bytes(EE_WON_HOST_CERT_USER, ctx, &[]).unwrap_or_default()
    }
    fn password(&mut self, ctx: CObj) -> Vec<u8> {
        self.bytes(EE_WON_HOST_CERT_PASSWORD, ctx, &[]).unwrap_or_default()
    }
    fn community(&mut self, ctx: CObj) -> Vec<u8> {
        self.bytes(EE_WON_HOST_CERT_COMMUNITY, ctx, &[]).unwrap_or_default()
    }
    fn hashes(&mut self, ctx: CObj, seed: &[u8]) -> Vec<u8> {
        self.bytes(EE_WON_HOST_CERT_HASHES, ctx, seed).unwrap_or_default()
    }
    fn set_request(&mut self, message: &[u8]) {
        self.side.set_request(self.c, message)
    }
    fn response(&mut self) -> Vec<u8> {
        self.side.response(self.c)
    }
    fn set_last_error(&mut self, status: i32) {
        self.side.set_last_error(self.c, status)
    }
    fn invalid_header(&mut self) -> i32 {
        self.side.invalid_header(self.c)
    }
    fn add_secret(&mut self, ctx: CObj, seed: &[u8]) {
        self.num(EE_WON_HOST_CERT_ADD_SECRET, [ctx, N, N], [0, 0], seed);
    }
    fn remove_secret(&mut self, ctx: CObj, seed: &[u8]) {
        self.num(EE_WON_HOST_CERT_REMOVE_SECRET, [ctx, N, N], [0, 0], seed);
    }
    fn new_cert(&mut self, kind: i32, data: &[u8]) -> CObj {
        self.obj(EE_WON_HOST_CERT_NEW, [N, N, N], [i64::from(kind), 0], data)
    }
    fn cert_valid(&mut self, kind: i32, cert: CObj) -> bool {
        self.num(EE_WON_HOST_CERT_VALID, [cert, N, N], [i64::from(kind), 0], &[]) != 0
    }
    fn block_verify(&mut self, block: CObj, kind: i32, cert: CObj) -> bool {
        self.num(EE_WON_HOST_CERT_BLOCK_VERIFY, [block, cert, N], [i64::from(kind), 0], &[]) != 0
    }
    fn cert_delta(&mut self, kind: i32, cert: CObj) -> i64 {
        self.num(EE_WON_HOST_CERT_DELTA, [cert, N, N], [i64::from(kind), 0], &[])
    }
    fn cert_times(&mut self, kind: i32, cert: CObj) -> (i64, i64) {
        let r = self.c.h.call(
            EE_WON_HOST_CERT_TIMES,
            [o(cert), Obj::none(), Obj::none()],
            [i64::from(kind), 0],
            &[],
            [0; 6],
        );
        let b = r.bytes.unwrap_or_default();
        let at = |i: usize| b.get(i..i + 8).map_or(0, |x| i64::from_le_bytes(x.try_into().unwrap_or([0; 8])));
        (at(0), at(8))
    }
    fn new_block(&mut self, data: &[u8]) -> CObj {
        self.obj(EE_WON_HOST_CERT_NEW_BLOCK, [N, N, N], [0, 0], data)
    }
    fn block_verified(&mut self, block: CObj, ctx: CObj) -> bool {
        self.num(EE_WON_HOST_CERT_BLOCK_VERIFIED, [block, ctx, N], [0, 0], &[]) != 0
    }
    fn commit(&mut self, ctx: CObj, time: i64, duration: i64) {
        self.num(EE_WON_HOST_CERT_COMMIT, [ctx, N, N], [time, duration], &[]);
    }
    fn priv_new(&mut self) -> *mut c_void {
        self.obj(EE_WON_HOST_CERT_PRIV_NEW, [N, N, N], [0, 0], &[])
    }
    fn priv_set(&mut self, key: *mut c_void, data: &[u8]) -> bool {
        self.num(EE_WON_HOST_CERT_PRIV_SET, [key, N, N], [0, 0], data) != 0
    }
    fn priv_free(&mut self, key: *mut c_void) {
        self.num(EE_WON_HOST_CERT_PRIV_FREE, [key, N, N], [0, 0], &[]);
    }
    fn new_peer(&mut self, cert1: CObj, block: CObj, key: *mut c_void, delta: i64, cert2: CObj) -> CObj {
        self.obj(EE_WON_HOST_CERT_NEW_PEER, [cert1, block, cert2], [key as usize as i64, delta], &[])
    }
    fn set_password(&mut self, ctx: CObj, password: &[u8]) {
        self.num(EE_WON_HOST_CERT_SET_PASSWORD, [ctx, N, N], [0, 0], password);
    }
    fn set_peer(&mut self, ctx: CObj, peer: CObj) {
        self.num(EE_WON_HOST_CERT_SET_PEER, [ctx, peer, N], [0, 0], &[]);
    }
    fn clear_one_time(&mut self, ctx: CObj) {
        self.num(EE_WON_HOST_CERT_CLEAR_ONE_TIME, [ctx, N, N], [0, 0], &[]);
    }
}

/// `GetCertOp` on the base: getcert.rs's members and the session key.
pub(crate) struct CertReq {
    pub(crate) g: GetCert,
    pub(crate) key: Blowfish,
}

// SAFETY: the op's pointers are C++ `RefCount*`s (and a private key) reached only through the host calls, under the
// op's lock; the C++ object they belong to is used from whichever thread runs the op, as the original's was
unsafe impl Send for CertReq {}

impl CertReq {
    fn env<'a, 'b>(c: &'a Calls<'b>, key: &'a mut Blowfish) -> HostEnv<'a, 'b, ExSide<'a>> {
        HostEnv { c, side: ExSide(key) }
    }
}

impl HostRequest for CertReq {
    fn length_field_size(&self) -> u8 {
        4
    }
    fn use_auth2(&self) -> bool {
        true
    }
    fn run_hook(&mut self, c: &Calls) -> Option<i32> {
        self.g.run_hook(&mut CertReq::env(c, &mut self.key))
    }
    fn cleanup(&mut self, c: &Calls, after: bool) {
        if !after {
            self.g.cleanup_hook(&mut CertReq::env(c, &mut self.key));
        }
    }
    fn next_request(&mut self, c: &Calls, _datagram: bool) -> (i32, Option<Vec<u8>>) {
        (self.g.get_next_request(&mut CertReq::env(c, &mut self.key)), None)
    }
    fn check_response(&mut self, c: &Calls, _response: &[u8]) -> Checked {
        match self.g.check_response(&mut CertReq::env(c, &mut self.key)) {
            Ok(s) => Checked::Status(s),
            Err(Cut) => Checked::Cut,
        }
    }
    fn reset(&mut self, _c: &Calls) {
        self.g.reset()
    }
    fn describe(&self) -> String {
        let g = &self.g;
        format!(
            "state {} delta {} errors {} communities {}",
            g.state,
            g.auth_delta,
            g.errors.len(),
            g.communities.len()
        )
    }
    fn num(&self, which: i32, index: i64) -> i64 {
        let c = usize::try_from(index).ok().and_then(|i| self.g.communities.get(i));
        match which {
            EE_WON_GETCERT_PEER => self.g.peer as usize as i64,
            EE_WON_GETCERT_SESSION_KEY => &self.key as *const Blowfish as usize as i64,
            EE_WON_GETCERT_ERRORS => self.g.errors.len() as i64,
            EE_WON_GETCERT_COMMUNITIES => self.g.communities.len() as i64,
            EE_WON_GETCERT_COMMUNITY_STATUS => c.map_or(0, |c| i64::from(c.status)),
            EE_WON_GETCERT_COMMUNITY_ID => c.map_or(0, |c| i64::from(c.id)),
            _ => 0,
        }
    }
    fn bytes(&self, which: i32, index: i64) -> Option<Vec<u8>> {
        let i = usize::try_from(index).ok()?;
        match which {
            EE_WON_GETCERT_ERRORS => self.g.errors.get(i).cloned(),
            EE_WON_GETCERT_COMMUNITIES => self.g.communities.get(i).map(|c| c.name.clone()),
            _ => None,
        }
    }
    fn set(&mut self, c: &Calls, which: i32, value: &SetValue) {
        match which {
            EE_WON_GETCERT_CREATE_ACCOUNT => self.g.create_account = value.num != 0,
            EE_WON_GETCERT_NEW_PASSWORD => self.g.new_password = value.data.to_vec(),
            EE_WON_GETCERT_AUTH_CONTEXT => self.g.set_context(&mut CertReq::env(c, &mut self.key), value.obj.ptr()),
            _ => {}
        }
    }
    fn take_objects(&mut self) -> Vec<Obj> {
        let g = &mut self.g;
        [&mut g.peer, &mut g.key_block, &mut g.context]
            .into_iter()
            .map(|p| o(std::mem::replace(p, std::ptr::null_mut())))
            .filter(|p| !p.is_none())
            .collect()
    }
}

/// A new `GetCertOp`: its auth context (`EE_WON_GETCERT_AUTH_CONTEXT`) and its server context
/// (`EE_WON_REQHOST_CONTEXT`, the auth context's) are set after the bind.
#[no_mangle]
pub extern "C" fn ee_won_hostop_getcert() -> *mut EeWonHostOp {
    let r = CertReq { g: GetCert::new(std::ptr::null_mut()), key: Blowfish::new() };
    EeWonHostOp::boxed(ReqPlan::new(Box::new(r)))
}
