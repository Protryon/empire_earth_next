//! `golden_getcert.txt` replayed: the stand-ins of `tests/won/won_cert_test.cpp` (verdicts that follow their
//! bytes, every call logged), the same setup, and each step's trace, return or throw and members compared with
//! what the original's statements did. And the real op's own session key (a `Blowfish`) through the C ABI.

use super::*;
use crate::certhost::{CertSide, HostEnv};
use crate::hostop::{
    EeWonHostCall, Host, EE_WON_HOST_ADD_REF, EE_WON_HOST_CERT_ADD_SECRET, EE_WON_HOST_CERT_BLOCK_ENCRYPT,
    EE_WON_HOST_CERT_BLOCK_ID, EE_WON_HOST_CERT_BLOCK_VALID, EE_WON_HOST_CERT_BLOCK_VERIFIED,
    EE_WON_HOST_CERT_BLOCK_VERIFY, EE_WON_HOST_CERT_CLEAR_ONE_TIME, EE_WON_HOST_CERT_COMMIT,
    EE_WON_HOST_CERT_COMMUNITY, EE_WON_HOST_CERT_CTX_PEER, EE_WON_HOST_CERT_DELTA, EE_WON_HOST_CERT_HASHES,
    EE_WON_HOST_CERT_LOAD_SECRETS, EE_WON_HOST_CERT_NEW, EE_WON_HOST_CERT_NEW_BLOCK, EE_WON_HOST_CERT_NEW_PEER,
    EE_WON_HOST_CERT_PASSWORD, EE_WON_HOST_CERT_PEER_BLOCK, EE_WON_HOST_CERT_PEER_DELTA, EE_WON_HOST_CERT_PRIV_FREE,
    EE_WON_HOST_CERT_PRIV_NEW, EE_WON_HOST_CERT_PRIV_SET, EE_WON_HOST_CERT_REMOVE_SECRET,
    EE_WON_HOST_CERT_SET_PASSWORD, EE_WON_HOST_CERT_SET_PEER, EE_WON_HOST_CERT_SET_PEER_FROM_BLOCK,
    EE_WON_HOST_CERT_TIMES, EE_WON_HOST_CERT_USER, EE_WON_HOST_CERT_VALID, EE_WON_HOST_CERT_VERIFIER_PUBLIC,
    EE_WON_HOST_RELEASE,
};
use crate::reqhost::{Calls, Plain, ReqPlan};
use std::fmt::Write as _;

fn hex(d: &[u8]) -> String {
    d.iter().map(|b| format!("{b:02x}")).collect()
}

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
}

fn at(d: &[u8], i: usize) -> u8 {
    d.get(i).copied().unwrap_or(0)
}

#[derive(Clone)]
enum SObj {
    Ctx,
    Block { serial: i32, id: u16, valid: bool, verified: bool, fail: bool },
    Cert { serial: i32, valid: bool, signed: bool, issue: i64, expire: i64 },
    Peer { serial: i32, block: Obj, delta: i64 },
}

struct Std {
    objs: Vec<SObj>,
    refs: Vec<i32>,
    trace: String,
    serial: i32,
    all_pass: bool,
    key: [u8; 8],
    key_len: i32,
    request: Option<Vec<u8>>,
    response: Vec<u8>,
    status: i32,
    public: bool,
    peer: Obj,
    user: Vec<u8>,
    password: Vec<u8>,
    community: Vec<u8>,
}

fn idx(o: Obj) -> usize {
    o as usize - 1
}

impl Std {
    fn new(all_pass: bool) -> Std {
        let mut key = [0u8; 8];
        for (i, k) in key.iter_mut().enumerate() {
            *k = 0x10 + i as u8;
        }
        Std {
            objs: vec![SObj::Ctx],
            refs: vec![1],
            trace: String::new(),
            serial: 0,
            all_pass,
            key,
            key_len: 8,
            request: None,
            response: Vec::new(),
            status: 0,
            public: true,
            peer: ptr::null_mut(),
            user: Vec::new(),
            password: Vec::new(),
            community: Vec::new(),
        }
    }
    fn ctx() -> Obj {
        1 as Obj
    }
    /// `Log`: the driver formats into 256 bytes
    fn log(&mut self, mut s: String) {
        s.truncate(255);
        self.trace += &s;
        self.trace.push(';');
    }
    fn add(&mut self, o: SObj) -> Obj {
        self.objs.push(o);
        self.refs.push(1);
        self.objs.len() as Obj
    }
    fn serial_of(&self, o: Obj) -> i32 {
        if o.is_null() {
            return 0;
        }
        match self.objs[idx(o)] {
            SObj::Block { serial, .. } | SObj::Cert { serial, .. } | SObj::Peer { serial, .. } => serial,
            SObj::Ctx => -1,
        }
    }
    /// `SKeyBlock(data)`, or `SKeyBlock()` for None
    fn block(&mut self, d: Option<&[u8]>) -> Obj {
        self.serial += 1;
        let d = d.unwrap_or(&[]);
        let b = SObj::Block {
            serial: self.serial,
            id: at(d, 1) as u16 | (at(d, 2) as u16) << 8,
            valid: !d.is_empty(),
            verified: self.all_pass || at(d, 0) & 1 != 0,
            fail: !self.all_pass && at(d, 0) & 4 != 0,
        };
        let s = self.serial;
        self.log(format!("block#{s} {}", hex(d)));
        self.add(b)
    }
    fn set_fail(&mut self, b: Obj, v: bool) {
        if let SObj::Block { fail, .. } = &mut self.objs[idx(b)] {
            *fail = v;
        }
    }
    /// `SPeerData(block)`
    fn peer_of(&mut self, b: Obj) -> Obj {
        let b = if b.is_null() { self.block(None) } else { b };
        self.refs[idx(b)] += 1;
        self.serial += 1;
        let s = self.serial;
        let bs = self.serial_of(b);
        self.log(format!("peer#{s} b{bs}"));
        self.add(SObj::Peer { serial: s, block: b, delta: 0 })
    }
    fn set_peer_obj(&mut self, p: Obj) {
        if !p.is_null() {
            self.refs[idx(p)] += 1;
        }
        let old = std::mem::replace(&mut self.peer, p);
        if !old.is_null() {
            self.release(old);
        }
    }
}

impl Env for Std {
    fn add_ref(&mut self, o: Obj) {
        self.refs[idx(o)] += 1;
    }
    fn release(&mut self, o: Obj) {
        self.refs[idx(o)] -= 1;
        assert!(self.refs[idx(o)] >= 0, "released too often");
        if self.refs[idx(o)] == 0 {
            if let SObj::Peer { block, .. } = self.objs[idx(o)] {
                self.release(block);
            }
        }
    }
    fn verifier_public(&mut self, _ctx: Obj) -> bool {
        self.log("public".into());
        self.public
    }
    fn ctx_peer(&mut self, _ctx: Obj) -> Obj {
        let s = self.serial_of(self.peer);
        self.log(format!("getpeer#{s}"));
        let p = self.peer;
        self.add_ref(p);
        p
    }
    fn peer_block(&mut self, peer: Obj) -> Obj {
        match self.objs[idx(peer)] {
            SObj::Peer { block, .. } => block,
            _ => panic!("not a peer"),
        }
    }
    fn peer_delta(&mut self, peer: Obj) -> i64 {
        match self.objs[idx(peer)] {
            SObj::Peer { delta, .. } => delta,
            _ => panic!("not a peer"),
        }
    }
    fn load_secrets(&mut self, _ctx: Obj) {
        self.log("load".into());
    }
    fn op_status(&mut self) -> i32 {
        let s = self.status;
        self.log(format!("status {s}"));
        s
    }
    fn block_valid(&mut self, block: Obj) -> bool {
        let s = self.serial_of(block);
        self.log(format!("isvalid#{s}"));
        matches!(self.objs[idx(block)], SObj::Block { valid: true, .. })
    }
    fn set_peer_from_block(&mut self, _ctx: Obj, block: Obj) {
        let p = self.peer_of(block);
        let s = self.serial_of(p);
        self.log(format!("setpeer#{s}"));
        self.set_peer_obj(p);
        self.release(p);
    }
    fn block_id(&mut self, block: Obj) -> u16 {
        match self.objs[idx(block)] {
            SObj::Block { id, .. } => id,
            _ => panic!("not a block"),
        }
    }
    fn block_encrypt(&mut self, block: Obj, data: &[u8]) -> Option<Vec<u8>> {
        self.log(format!("elg {}", hex(data)));
        if matches!(self.objs[idx(block)], SObj::Block { fail: true, .. }) {
            return None;
        }
        let mut v: Vec<u8> = data.iter().map(|b| b ^ 0x5a).collect();
        v.push(b'E');
        Some(v)
    }
    fn key_create(&mut self, n: i32) {
        self.log(format!("create {n}"));
        for k in self.key.iter_mut() {
            *k = k.wrapping_mul(7).wrapping_add(3);
        }
        self.key_len = n;
    }
    fn key(&mut self) -> Vec<u8> {
        self.key[..self.key_len as usize].to_vec()
    }
    fn key_encrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        self.log(format!("bf {}", hex(data)));
        Some(data.iter().map(|b| b ^ 0x33).collect())
    }
    fn key_decrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        self.log(format!("unbf {}", data.len()));
        if !self.all_pass && at(data, 0) == 0xdd {
            return None;
        }
        Some(data.to_vec())
    }
    fn user(&mut self, _ctx: Obj) -> Vec<u8> {
        self.user.clone()
    }
    fn password(&mut self, _ctx: Obj) -> Vec<u8> {
        self.password.clone()
    }
    fn community(&mut self, _ctx: Obj) -> Vec<u8> {
        self.log("community".into());
        self.community.clone()
    }
    fn hashes(&mut self, _ctx: Obj, seed: &[u8]) -> Vec<u8> {
        self.log("hashes".into());
        let mut v = seed.to_vec();
        v.push(b'H');
        v
    }
    fn set_request(&mut self, message: &[u8]) {
        self.request = Some(message.to_vec());
    }
    fn response(&mut self) -> Vec<u8> {
        self.response.clone()
    }
    fn set_last_error(&mut self, status: i32) {
        self.log(format!("lasterror {status}"));
    }
    fn invalid_header(&mut self) -> i32 {
        self.log("invalid".into());
        ST_TRY_NEXT_SERVER
    }
    fn add_secret(&mut self, _ctx: Obj, seed: &[u8]) {
        self.log(format!("add {}", hex(&seed[..16])));
    }
    fn remove_secret(&mut self, _ctx: Obj, seed: &[u8]) {
        self.log(format!("remove {}", hex(&seed[..16])));
    }
    fn new_cert(&mut self, kind: i32, d: &[u8]) -> Obj {
        self.serial += 1;
        let s = self.serial;
        let issue = at(d, 1) as i64 * 1000;
        let c = SObj::Cert {
            serial: s,
            valid: self.all_pass || at(d, 0) & 1 != 0,
            signed: self.all_pass || at(d, 0) & 2 != 0,
            issue,
            expire: issue + at(d, 2) as i64 * 10,
        };
        self.log(format!("cert{kind}#{s} {}", hex(d)));
        self.add(c)
    }
    fn cert_valid(&mut self, _kind: i32, cert: Obj) -> bool {
        let s = self.serial_of(cert);
        self.log(format!("valid#{s}"));
        matches!(self.objs[idx(cert)], SObj::Cert { valid: true, .. })
    }
    fn block_verify(&mut self, block: Obj, _kind: i32, cert: Obj) -> bool {
        let (b, c) = (self.serial_of(block), self.serial_of(cert));
        self.log(format!("verify#{b}#{c}"));
        matches!(self.objs[idx(cert)], SObj::Cert { signed: true, .. })
    }
    fn cert_delta(&mut self, _kind: i32, cert: Obj) -> i64 {
        let s = self.serial_of(cert);
        self.log(format!("delta#{s}"));
        match self.objs[idx(cert)] {
            SObj::Cert { issue, .. } => issue - 500,
            _ => panic!(),
        }
    }
    fn cert_times(&mut self, _kind: i32, cert: Obj) -> (i64, i64) {
        match self.objs[idx(cert)] {
            SObj::Cert { issue, expire, .. } => (issue, expire),
            _ => panic!(),
        }
    }
    fn new_block(&mut self, data: &[u8]) -> Obj {
        self.block(Some(data))
    }
    fn block_verified(&mut self, block: Obj, _ctx: Obj) -> bool {
        let s = self.serial_of(block);
        self.log(format!("verified#{s}"));
        matches!(self.objs[idx(block)], SObj::Block { verified: true, .. })
    }
    fn commit(&mut self, _ctx: Obj, time: i64, duration: i64) {
        self.log(format!("commit {} {}", time as i32, duration as i32));
    }
    fn priv_new(&mut self) -> *mut c_void {
        // the stand-in key keeps no state: any non-NULL handle
        ptr::NonNull::<u8>::dangling().as_ptr() as *mut c_void
    }
    fn priv_set(&mut self, _key: *mut c_void, data: &[u8]) -> bool {
        self.log(format!("priv {}", hex(data)));
        self.all_pass || (!data.is_empty() && data[0] != 0)
    }
    fn priv_free(&mut self, _key: *mut c_void) {}
    fn new_peer(&mut self, cert1: Obj, block: Obj, _key: *mut c_void, delta: i64, cert2: Obj) -> Obj {
        self.serial += 1;
        let s = self.serial;
        let (a, b, c) = (self.serial_of(cert1), self.serial_of(block), self.serial_of(cert2));
        self.log(format!("peer#{s} c{a} b{b} d{} c{c}", delta as i32));
        self.add_ref(block);
        self.add(SObj::Peer { serial: s, block, delta })
    }
    fn set_password(&mut self, _ctx: Obj, password: &[u8]) {
        self.log(format!("password {}", password.len() / 2));
    }
    fn set_peer(&mut self, _ctx: Obj, peer: Obj) {
        let s = self.serial_of(peer);
        self.log(format!("setpeer#{s}"));
        self.set_peer_obj(peer);
    }
    fn clear_one_time(&mut self, _ctx: Obj) {
        self.log("clear".into());
    }
}

fn dump(g: &GetCert, e: &Std) -> String {
    let mut s = format!(
        "state {} delta {} block {} peer {} seed {} key {} req {}",
        g.state,
        g.auth_delta,
        e.serial_of(g.key_block),
        e.serial_of(g.peer),
        hex(&g.seed),
        hex(&e.key),
        e.request.as_ref().map_or("-".to_string(), |r| hex(r))
    );
    for x in &g.errors {
        write!(s, " err {}", hex(x)).unwrap();
    }
    for c in &g.communities {
        write!(s, " cs {} {} {}", c.status, c.id, hex(&c.name)).unwrap();
    }
    write!(s, " ctxpeer {}", e.serial_of(e.peer)).unwrap();
    s
}

// ---- the same replay through the Rust op base (certhost.rs): every object statement a host call, answered by the
// stand-in as RustServerOp::HostCall answers it on the C++ objects; the op's own side (its members, the stand-in key)
// the stand-in's

struct TestSide(*mut Std);

// SAFETY (every method): the replay's stand-in, alive and otherwise untouched during the step
impl CertSide for TestSide {
    fn op_status(&mut self, _c: &Calls) -> i32 {
        unsafe { &mut *self.0 }.op_status()
    }
    fn set_request(&mut self, _c: &Calls, message: &[u8]) {
        unsafe { &mut *self.0 }.set_request(message)
    }
    fn response(&mut self, _c: &Calls) -> Vec<u8> {
        unsafe { &mut *self.0 }.response()
    }
    fn set_last_error(&mut self, _c: &Calls, status: i32) {
        unsafe { &mut *self.0 }.set_last_error(status)
    }
    fn invalid_header(&mut self, _c: &Calls) -> i32 {
        unsafe { &mut *self.0 }.invalid_header()
    }
    fn key_create(&mut self, n: i32) {
        unsafe { &mut *self.0 }.key_create(n)
    }
    fn key(&mut self) -> Vec<u8> {
        unsafe { &mut *self.0 }.key()
    }
    fn key_encrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        unsafe { &mut *self.0 }.key_encrypt(data)
    }
    fn key_decrypt(&mut self, data: &[u8]) -> Option<Vec<u8>> {
        unsafe { &mut *self.0 }.key_decrypt(data)
    }
}

unsafe fn give(c: &mut EeWonHostCall, b: &[u8]) {
    unsafe { (c.out_sink)(c.out_context, b.as_ptr(), b.len()) }
}

/// The host calls answered by the stand-in (`host` the replay's `Std`).
unsafe extern "C" fn std_host(host: *mut c_void, call: *mut EeWonHostCall) {
    let e = unsafe { &mut *(host as *mut Std) };
    let c = unsafe { &mut *call };
    let (o0, o1, o2) = (c.obj[0], c.obj[1], c.obj[2]);
    let (n0, n1) = (c.num[0], c.num[1]);
    let data = unsafe { bytes(c.data, c.len) }.to_vec();
    match c.code {
        EE_WON_HOST_ADD_REF => e.add_ref(o0),
        EE_WON_HOST_RELEASE => e.release(o0),
        EE_WON_HOST_CERT_VERIFIER_PUBLIC => c.out_num = i64::from(e.verifier_public(o0)),
        EE_WON_HOST_CERT_CTX_PEER => c.out_obj = e.ctx_peer(o0),
        EE_WON_HOST_CERT_PEER_BLOCK => c.out_obj = e.peer_block(o0),
        EE_WON_HOST_CERT_PEER_DELTA => c.out_num = e.peer_delta(o0),
        EE_WON_HOST_CERT_LOAD_SECRETS => e.load_secrets(o0),
        EE_WON_HOST_CERT_BLOCK_VALID => c.out_num = i64::from(e.block_valid(o0)),
        EE_WON_HOST_CERT_SET_PEER_FROM_BLOCK => e.set_peer_from_block(o0, o1),
        EE_WON_HOST_CERT_BLOCK_ID => c.out_num = i64::from(e.block_id(o0)),
        EE_WON_HOST_CERT_BLOCK_ENCRYPT => {
            if let Some(v) = e.block_encrypt(o0, &data) {
                unsafe { give(c, &v) }
            }
        }
        EE_WON_HOST_CERT_USER => unsafe { give(c, &e.user(o0)) },
        EE_WON_HOST_CERT_PASSWORD => unsafe { give(c, &e.password(o0)) },
        EE_WON_HOST_CERT_COMMUNITY => unsafe { give(c, &e.community(o0)) },
        EE_WON_HOST_CERT_HASHES => unsafe { give(c, &e.hashes(o0, &data)) },
        EE_WON_HOST_CERT_ADD_SECRET => e.add_secret(o0, &data),
        EE_WON_HOST_CERT_REMOVE_SECRET => e.remove_secret(o0, &data),
        EE_WON_HOST_CERT_NEW => c.out_obj = e.new_cert(n0 as i32, &data),
        EE_WON_HOST_CERT_VALID => c.out_num = i64::from(e.cert_valid(n0 as i32, o0)),
        EE_WON_HOST_CERT_BLOCK_VERIFY => c.out_num = i64::from(e.block_verify(o0, n0 as i32, o1)),
        EE_WON_HOST_CERT_DELTA => c.out_num = e.cert_delta(n0 as i32, o0),
        EE_WON_HOST_CERT_TIMES => {
            let (a, b) = e.cert_times(n0 as i32, o0);
            let mut v = a.to_le_bytes().to_vec();
            v.extend_from_slice(&b.to_le_bytes());
            unsafe { give(c, &v) }
        }
        EE_WON_HOST_CERT_NEW_BLOCK => c.out_obj = e.new_block(&data),
        EE_WON_HOST_CERT_BLOCK_VERIFIED => c.out_num = i64::from(e.block_verified(o0, o1)),
        EE_WON_HOST_CERT_COMMIT => e.commit(o0, n0, n1),
        EE_WON_HOST_CERT_PRIV_NEW => c.out_obj = e.priv_new(),
        EE_WON_HOST_CERT_PRIV_SET => c.out_num = i64::from(e.priv_set(o0, &data)),
        EE_WON_HOST_CERT_PRIV_FREE => e.priv_free(o0),
        EE_WON_HOST_CERT_NEW_PEER => c.out_obj = e.new_peer(o0, o1, n0 as usize as *mut c_void, n1, o2),
        EE_WON_HOST_CERT_SET_PASSWORD => e.set_password(o0, &data),
        EE_WON_HOST_CERT_SET_PEER => e.set_peer(o0, o1),
        EE_WON_HOST_CERT_CLEAR_ONE_TIME => e.clear_one_time(o0),
        k => panic!("host call {k}"),
    }
}

fn new_host() -> Host<ReqPlan> {
    Host::new(ReqPlan::new(Box::new(Plain(Box::new(crate::reqops::UserIdOp::default())))))
}

/// `$body` over the stand-in itself, or over the base's host calls answered by it.
macro_rules! env {
    ($host:expr, $e:ident, |$v:ident| $body:expr) => {
        match $host {
            Some(h) => {
                let p: *mut Std = &mut $e;
                h.bind_to(Some(std_host), p as *mut c_void);
                let calls = Calls { h };
                let mut he = HostEnv { c: &calls, side: TestSide(p) };
                let $v = &mut he;
                $body
            }
            None => {
                let $v = &mut $e;
                $body
            }
        }
    };
}

/// One golden sequence: the setup line and the step lines.
fn replay(setup: &str, steps: &[&str]) -> usize {
    replay_on(setup, steps, None)
}

fn replay_on(setup: &str, steps: &[&str], host: Option<&Host<ReqPlan>>) -> usize {
    let f: Vec<&str> = setup.split(' ').collect();
    let n = |i: usize| f[i].parse::<i64>().unwrap();
    let (state, seed, block, encfail, create) = (n(1) as i32, n(2) != 0, n(3) != 0, n(4) != 0, n(5) != 0);
    let (all_pass, public, peer_block, peer_delta) = (n(6) != 0, n(7) != 0, n(8) != 0, n(9));
    let (block_bytes, peer_bytes) = (unhex(f[10]), unhex(f[11]));
    let text = |i: usize, p: &str| unhex(f[i].strip_prefix(p).unwrap());

    let mut e = Std::new(all_pass);
    e.user = text(12, "u");
    e.password = text(13, "p");
    e.community = text(15, "c");
    let mut g = GetCert::new(Std::ctx());
    e.add_ref(Std::ctx());
    g.state = state;
    let b = if block {
        let b = e.block(Some(&block_bytes));
        e.set_fail(b, encfail);
        b
    } else {
        e.block(None)
    };
    g.set_key_block(&mut e, b);
    e.release(b);
    if seed {
        g.seed = (0x80..0x90).collect();
    }
    g.new_password = text(14, "n");
    g.create_account = create;
    e.public = public;
    let pb = if peer_block {
        let pb = e.block(Some(&peer_bytes));
        e.set_fail(pb, encfail);
        pb
    } else {
        ptr::null_mut()
    };
    let p = e.peer_of(pb);
    if !pb.is_null() {
        e.release(pb);
    }
    if let SObj::Peer { delta, .. } = &mut e.objs[idx(p)] {
        *delta = peer_delta;
    }
    e.set_peer_obj(p);
    e.release(p);

    let mut checked = 0;
    for line in steps {
        let (step, want) = line.split_once(" => ").unwrap();
        let mut sf = step.split(' ');
        sf.next();
        let kind: i32 = sf.next().unwrap().parse().unwrap();
        let arg = sf.next().unwrap();
        e.trace.clear();
        let mut threw = false;
        let mut status = 0;
        match kind {
            0 => match env!(host, e, |v| g.run_hook(v)) {
                Some(s) => e.log(format!("finish {s}")),
                None => e.log("baserun".into()),
            },
            1 => status = env!(host, e, |v| g.get_next_request(v)),
            2 => {
                e.response = unhex(arg);
                match env!(host, e, |v| g.check_response(v)) {
                    Ok(s) => status = s,
                    Err(Cut) => threw = true,
                }
            }
            3 => g.reset(),
            4 => {
                e.status = arg.parse().unwrap();
                env!(host, e, |v| g.cleanup_hook(v));
                e.log("baseclean".into());
            }
            5 => {
                g.create_account = arg.starts_with('1');
                g.new_password = unhex(&arg[2..]);
            }
            _ => panic!("step {kind}"),
        }
        if let Some(t) = want.strip_prefix("crash|") {
            // the original dereferenced NULL: the port's trace starts with its own and goes on to the commit
            assert!(e.trace.starts_with(t), "{setup}\n{step}\n  want {t}\n  got  {}", e.trace);
            assert!(e.trace[t.len()..].starts_with("commit "), "{step}: {}", e.trace);
            return checked + 1;
        }
        let got = format!("{} {}|{}|{}", threw as i32, status, e.trace, dump(&g, &e));
        assert_eq!(got, *want, "\n{setup}\n{step}");
        checked += 1;
    }
    env!(host, e, |v| g.release_all(v));
    checked
}

#[test]
fn golden_sequences() {
    let text = include_str!("golden_getcert.txt");
    let mut lines = text.lines().peekable();
    let (mut sequences, mut steps) = (0, 0);
    while let Some(setup) = lines.next() {
        assert!(setup.starts_with("S "), "{setup}");
        let mut body = Vec::new();
        while let Some(l) = lines.peek() {
            if l.starts_with("S ") {
                break;
            }
            body.push(lines.next().unwrap());
        }
        steps += replay(setup, &body);
        sequences += 1;
    }
    assert_eq!(sequences, 400);
    assert_eq!(steps, 1935);
}

#[test]
fn golden_through_the_base() {
    let text = include_str!("golden_getcert.txt");
    let h = new_host();
    let mut lines = text.lines().peekable();
    let mut steps = 0;
    while let Some(setup) = lines.next() {
        let mut body = Vec::new();
        while let Some(l) = lines.peek() {
            if l.starts_with("S ") {
                break;
            }
            body.push(lines.next().unwrap());
        }
        steps += replay_on(setup, &body, Some(&h));
    }
    assert_eq!(steps, 1935);
}

#[test]
fn references_balance() {
    // a whole login: the key block, the challenge and the reply; every reference the op took is let go
    let mut e = Std::new(true);
    let mut g = GetCert::new(Std::ctx());
    e.add_ref(Std::ctx());
    let pb = e.block(Some(&[1, 2, 3]));
    let p = e.peer_of(pb);
    e.release(pb);
    e.set_peer_obj(p);
    e.release(p);
    assert_eq!(g.run_hook(&mut e), None);
    assert_eq!(g.get_next_request(&mut e), ST_RECV);
    e.response = vec![5, 12, 0, 4, 0, 16, 0];
    e.response.extend((0..16).map(|i| 0xa0 + i));
    assert_eq!(g.check_response(&mut e), Ok(ST_SEND));
    assert_eq!(g.seed.len(), 16);
    g.release_all(&mut e);
    e.set_peer_obj(ptr::null_mut());
    assert_eq!(e.refs[0], 1);
    assert!(e.refs[1..].iter().all(|&r| r == 0), "{:?}", e.refs);
}

// ---- the real op's session key through the C ABI

static LAST_REQUEST: std::sync::Mutex<Vec<u8>> = std::sync::Mutex::new(Vec::new());

unsafe extern "C" fn no_obj(_: *mut c_void) {}
unsafe extern "C" fn yes(_: *mut c_void, _: *mut c_void) -> bool {
    true
}
unsafe extern "C" fn yes3(_: *mut c_void, _: *mut c_void, _: *mut c_void) -> bool {
    true
}
unsafe extern "C" fn null_obj(_: *mut c_void, _: *mut c_void) -> *mut c_void {
    ptr::null_mut()
}
unsafe extern "C" fn zero_i64(_: *mut c_void, _: *mut c_void) -> i64 {
    0
}
unsafe extern "C" fn nothing(_: *mut c_void, _: *mut c_void) {}
unsafe extern "C" fn status0(_: *mut c_void) -> c_long {
    0
}
unsafe extern "C" fn nothing3(_: *mut c_void, _: *mut c_void, _: *mut c_void) {}
unsafe extern "C" fn id7(_: *mut c_void, _: *mut c_void) -> u16 {
    7
}
unsafe extern "C" fn copy_out(
    _: *mut c_void,
    _: *mut c_void,
    d: *const u8,
    n: usize,
    s: EeWonByteSink,
    c: *mut c_void,
) -> bool {
    unsafe { s(c, d, n) };
    true
}
unsafe extern "C" fn text(_: *mut c_void, _: *mut c_void, s: EeWonByteSink, c: *mut c_void) {
    unsafe { s(c, [b'a', 0].as_ptr(), 2) }
}
unsafe extern "C" fn hashes0(_: *mut c_void, _: *mut c_void, _: *const u8, _: usize, _: EeWonByteSink, _: *mut c_void) {
}
unsafe extern "C" fn keep_request(_: *mut c_void, d: *const u8, n: usize) {
    *LAST_REQUEST.lock().unwrap() = unsafe { bytes(d, n) }.to_vec();
}
unsafe extern "C" fn response0(_: *mut c_void, _: EeWonByteSink, _: *mut c_void) {}
unsafe extern "C" fn error0(_: *mut c_void, _: c_long) {}
unsafe extern "C" fn secret0(_: *mut c_void, _: *mut c_void, _: *const u8, _: usize) {}
unsafe extern "C" fn cert0(_: *mut c_void, _: i32, _: *const u8, _: usize) -> *mut c_void {
    ptr::null_mut()
}
unsafe extern "C" fn cert_yes(_: *mut c_void, _: i32, _: *mut c_void) -> bool {
    true
}
unsafe extern "C" fn verify_yes(_: *mut c_void, _: *mut c_void, _: i32, _: *mut c_void) -> bool {
    true
}
unsafe extern "C" fn cert_i64(_: *mut c_void, _: i32, _: *mut c_void) -> i64 {
    0
}
unsafe extern "C" fn times0(_: *mut c_void, _: i32, _: *mut c_void, _: *mut i64, _: *mut i64) {}
unsafe extern "C" fn block0(_: *mut c_void, _: *const u8, _: usize) -> *mut c_void {
    ptr::null_mut()
}
unsafe extern "C" fn commit0(_: *mut c_void, _: *mut c_void, _: i64, _: i64) {}
unsafe extern "C" fn priv0(_: *mut c_void) -> *mut c_void {
    ptr::null_mut()
}
unsafe extern "C" fn priv_set0(_: *mut c_void, _: *mut c_void, _: *const u8, _: usize) -> bool {
    true
}
unsafe extern "C" fn peer0(
    _: *mut c_void,
    _: *mut c_void,
    _: *mut c_void,
    _: *mut c_void,
    _: i64,
    _: *mut c_void,
) -> *mut c_void {
    ptr::null_mut()
}

#[test]
fn real_session_key() {
    let hooks = EeWonGetCertHooks {
        context: ptr::null_mut(),
        add_ref: no_obj,
        release: no_obj,
        verifier_public: yes,
        ctx_peer: null_obj,
        peer_block: null_obj,
        peer_delta: zero_i64,
        load_secrets: nothing,
        op_status: status0,
        block_valid: yes,
        set_peer_from_block: nothing3,
        block_id: id7,
        block_encrypt: copy_out,
        key_create: None,
        key: None,
        key_encrypt: None,
        key_decrypt: None,
        user: text,
        password: text,
        community: text,
        hashes: hashes0,
        set_request: keep_request,
        response: response0,
        set_last_error: error0,
        invalid_header: status0,
        add_secret: secret0,
        remove_secret: secret0,
        new_cert: cert0,
        cert_valid: cert_yes,
        block_verify: verify_yes,
        cert_delta: cert_i64,
        cert_times: times0,
        new_block: block0,
        block_verified: yes3,
        commit: commit0,
        priv_new: priv0,
        priv_set: priv_set0,
        priv_free: nothing,
        new_peer: peer0,
        set_password: secret0,
        set_peer: nothing3,
        clear_one_time: nothing,
    };
    unsafe {
        let op = ee_won_getcert_new(&hooks, ptr::null_mut());
        ee_won_getcert_setup(op, &hooks, 1, ptr::null(), 0, 8 as *mut c_void);
        // a login request: an 8-byte random key created, the request's session key is it (the stand-in
        // encryption copies), the login data encrypted with it
        assert_eq!(ee_won_getcert_get_next_request(op, &hooks), ST_RECV as c_long);
        assert_eq!(ee_won_getcert_state(op), GETTING_CHALLENGE);
        let key = &*ee_won_getcert_session_key(op);
        assert_eq!(key.key().len(), 8);
        let m = LAST_REQUEST.lock().unwrap().clone();
        assert_eq!(&m[4..11], &[5, 12, 0, 3, 0, 7, 0]);
        assert_eq!(&m[11..13], &[8, 0]);
        assert_eq!(&m[13..21], key.key());
        let n = u16::from_le_bytes([m[21], m[22]]) as usize;
        let login = key.decrypt(&m[23..23 + n]).unwrap();
        assert_eq!(&login[..2], &key.key()[..2]);
        assert_eq!(&login[2..4], &[7, 0]);
        assert_eq!(&login[4..8], &[14, 0, 0, 0]);
        // the login data's strings, then the community data
        assert_eq!(&login[8..], &[1, 0, b'a', 0, 1, 0, b'a', 0, 0, 0, b'a', 0]);
        ee_won_getcert_free(op, &hooks);
    }
}
