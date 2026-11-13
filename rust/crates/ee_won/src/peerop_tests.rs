use std::ffi::c_void;

use super::*;
use crate::auth::Kind;
use crate::elgamal::ElGamal;
use crate::hostop::*;
use crate::peerauth::EeWonAuthSession;
use crate::peerobj::{peer_handle, Cert, PeerAuth, PeerData};
use crate::random::ee_won_random_test_seed;

fn hex(d: &[u8]) -> String {
    if d.is_empty() {
        return "-".to_string();
    }
    d.iter().map(|b| format!("{b:02x}")).collect()
}

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
}

// tests/won/won_peerop_test.cpp's keys (won_peerobj_test's)
const PRIV: [&str; 4] = [
    "3065021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a01ad613e700f0fe73a021900e90a2cffe3fabbe6ece0e935a39d1d5d424c542c077c261802080139dba7acd3581d",
    "3064021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a01ad613e700f0fe73a02184bb17e485a32d0a3f02eb0bc5d2b9a09f3cb369e38ff200602080c931814d44a5e5b",
    "3064021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a01ad613e700f0fe73a02183dda7c54fd793a4e534f60facc1301e37953f4df16f49861020808e58d3749117c1b",
    "3064021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a01ad613e700f0fe73a0218467548d5e50f1f981f842a63ceebe356665d1ec15fcdc85d02080f47ec32536bc8f1",
];

const SOCKET: usize = 1000;
const CONNECT_TIME: i64 = 60001;
const SEND_TIME: i64 = 60002;
const RECV_TIME: i64 = 60003;
const TIME_LEFT: i64 = 50000;

struct Spec<'a>(&'a [&'a str]);

/// The driver's `NewPeer` over the Rust objects: the certificates, then the private key (each a draw as there).
fn new_peer(s: &Spec, block: &Arc<Cert>) -> Arc<PeerData> {
    let f = s.0;
    if f[0] == "0" {
        return Arc::new(PeerData::empty(Some(block.clone())));
    }
    let c1 = Arc::new(Cert::new(Kind::Certificate, &unhex(f[2])));
    let c2 = if f[3] == "N" { None } else { Some(Arc::new(Cert::new(Kind::Certificate2, &unhex(f[3])))) };
    let mut key = ElGamal::new();
    key.set_private_key(&unhex(PRIV[f[1].parse::<usize>().unwrap()]));
    Arc::new(PeerData {
        cert: Some(c1),
        cert2: c2,
        block: Some(block.clone()),
        private_key: key,
        delta: f[4].parse().unwrap(),
    })
}

/// The stand-in again, answering from the recorded trace: each call must be the next recorded one, its answer the
/// recorded answer; the other end is fed every message sent, as the driver's (its draws are the run's too).
struct Replay {
    tokens: Vec<String>,
    at: usize,
    status: i32,
    lfs: usize,
    key: i64,
    auth_type: i64,
    is_async: bool,
    peers: [Option<Arc<PeerData>>; 2],
    other: PeerAuth,
    sessions: Vec<Box<EeWonAuthSession>>,
    line: usize,
    /// the light ops' members by object
    lights: Vec<(String, *const EeWonLightCert)>,
}

impl Replay {
    /// The next recorded call: its text up to the answer must be `call`; the answer.
    fn expect(&mut self, call: &str) -> String {
        let t = self.tokens.get(self.at).cloned().unwrap_or_default();
        self.at += 1;
        let (c, answer) = t.split_once('=').unwrap_or((&t, ""));
        assert_eq!(c, call, "line {} call {}", self.line, self.at);
        answer.to_string()
    }
    fn feed(&mut self, m: &[u8]) {
        let body = &m[self.lfs.min(m.len())..];
        self.other.recv(body, crate::peerobj::unix_now());
        self.other.session = None;
    }
    fn peer(&self, p: &str) -> i64 {
        let i: usize = p.trim_start_matches('p').parse().unwrap();
        match i {
            1 | 2 => self.peers[i - 1].clone().map_or(0, |p| peer_handle(p) as i64),
            _ => 0,
        }
    }
}

fn o(p: *mut c_void) -> String {
    format!("o{}", p as usize)
}

fn obj(answer: &str) -> *mut c_void {
    answer.trim_start_matches('o').parse::<usize>().unwrap() as *mut c_void
}

unsafe extern "C" fn host(ctx: *mut c_void, c: *mut EeWonHostCall) {
    let r = unsafe { &mut *ctx.cast::<Replay>() };
    let c = unsafe { &mut *c };
    let data = if c.len == 0 { Vec::new() } else { unsafe { std::slice::from_raw_parts(c.data, c.len) }.to_vec() };
    let n0 = c.num[0];
    let give = |c: &mut EeWonHostCall, b: &[u8]| unsafe { (c.out_sink)(c.out_context, b.as_ptr(), b.len()) };
    match c.code {
        EE_WON_HOST_STATUS => c.out_num = i64::from(r.status),
        EE_WON_HOST_MODE => c.out_num = if r.is_async { 1 } else { 2 },
        EE_WON_HOST_TIME_LEFT => c.out_num = TIME_LEFT,
        EE_WON_HOST_SEND_TIME => c.out_num = SEND_TIME,
        EE_WON_HOST_RECV_TIME => c.out_num = RECV_TIME,
        EE_WON_HOST_CONNECT_TIME => c.out_num = CONNECT_TIME,
        EE_WON_HOST_AUTH_TYPE => c.out_num = r.auth_type,
        EE_WON_HOST_OWN_SOCKET => c.out_obj = SOCKET as *mut c_void,
        EE_WON_HOST_SOCKET_LFS => c.out_num = r.lfs as i64,
        EE_WON_HOST_CLIENT_KEY => c.out_num = r.key,
        EE_WON_HOST_CONNECT => c.out_num = r.expect(&format!("connect({},{n0})", o(c.obj[0]))).parse().unwrap(),
        EE_WON_HOST_SEND_MSG => {
            let st: i64 = r.expect(&format!("send({},{},{n0})", o(c.obj[0]), hex(&data))).parse().unwrap();
            if st == 0 {
                r.feed(&data);
            }
            c.out_num = st;
        }
        EE_WON_HOST_RECV_MSG => {
            let a = r.expect(&format!("recv({},{n0})", o(c.obj[0])));
            let (st, b) = a.split_once(':').unwrap();
            c.out_num = st.parse().unwrap();
            if b != "N" {
                give(c, &unhex(b));
            }
        }
        EE_WON_HOST_NEW_OP => {
            let kind = match n0 {
                EE_WON_HOST_OP_CONNECT => 0,
                EE_WON_HOST_OP_SEND_MSG => 1,
                _ => 2,
            };
            c.out_obj = obj(&r.expect(&format!("newop({kind},{},{})", o(c.obj[0]), hex(&data))));
            if kind == 1 {
                r.feed(&data);
            }
        }
        EE_WON_HOST_TRACK => {
            r.expect(&format!("track({},{n0})", o(c.obj[0])));
        }
        EE_WON_HOST_QUEUE => {
            r.expect(&format!("queue({},{},{n0})", o(c.obj[0]), o(c.obj[1])));
        }
        EE_WON_HOST_FINISH => {
            r.expect(&format!("finish({n0})"));
            if r.status == ST_PENDING {
                r.status = n0 as i32;
            }
        }
        EE_WON_HOST_RELEASE | EE_WON_HOST_ADD_REF => {}
        EE_WON_HOST_PEER_CTX_DATA => {
            let a = r.expect("ctxpeer");
            c.out_num = r.peer(&a);
        }
        EE_WON_HOST_PEER_REFRESH_BLOCK => c.out_num = r.expect(&format!("refresh({n0})")).parse().unwrap(),
        EE_WON_HOST_PEER_NEW_LIGHT => c.out_obj = obj(&r.expect("light")),
        EE_WON_HOST_PEER_REFRESH_ASYNC => {
            r.expect(&format!("refreshasync({})", o(c.obj[0])));
            // the light op's member, borrowed (the op holds it)
            r.lights.push((o(c.obj[0]), n0 as usize as *const EeWonLightCert));
        }
        EE_WON_HOST_OP_STATUS => c.out_num = r.expect(&format!("opstatus({})", o(c.obj[0]))).parse().unwrap(),
        EE_WON_HOST_OP_IS_RECV_MSG => c.out_num = r.expect(&format!("isrecv({})", o(c.obj[0]))).parse().unwrap(),
        EE_WON_HOST_OP_BYTES => {
            let a = r.expect(&format!("opbytes({})", o(c.obj[0])));
            if a != "N" {
                give(c, &unhex(&a));
            }
        }
        EE_WON_HOST_PEER_NEW_SESSION => {
            // SAFETY: the op hands over a session it boxed
            let s = unsafe { Box::from_raw(n0 as usize as *mut EeWonAuthSession) };
            c.out_obj = obj(&r.expect(&format!("newsession({})", s.auth_type)));
            r.sessions.push(s);
        }
        EE_WON_HOST_ADD_CRYPT => {
            r.expect(&format!("crypt({},{})", o(c.obj[0]), o(c.obj[1])));
        }
        EE_WON_HOST_BASE_CLEANUP => {
            r.expect("cleanup");
        }
        code => panic!("line {}: unexpected call {code}", r.line),
    }
}

/// One golden line: the driver's `RunRust` over the recorded answers.
fn replay(n: usize, line: &str) -> bool {
    let (inputs, want) = line.split_once(" => ").unwrap();
    let f: Vec<&str> = inputs.split(' ').collect();
    assert_eq!(f[0], "O");
    let run: i64 = f[1].parse().unwrap();
    let server = f[2] == "1";
    let is_async = f[3] == "1";
    let use_auth2 = f[4] == "1";
    let lfs: u8 = f[6].parse().unwrap();
    let block_raw = unhex(f[8]);
    let (ctx, refreshed, other) = (Spec(&f[9..14]), Spec(&f[14..19]), Spec(&f[19..24]));
    let (other_auth2, other_type, other_key) = (f[24] == "1", f[25].parse().unwrap(), f[26].parse::<i64>().unwrap());
    let (trace, end) = want.split_once("|status ").unwrap();

    // Begin: the other end, then the op's peer data
    ee_won_random_test_seed(run);
    let now = crate::peerobj::unix_now();
    let other_block = Arc::new(Cert::new(Kind::PubKeyBlock, &block_raw));
    let other_peer = new_peer(&other, &other_block);
    let mut other_auth = PeerAuth::new(!server);
    other_auth.set_use_auth2(other_auth2);
    if server {
        other_auth.set_client_key_size(other_key as u8, (other_key >> 8) & 1 != 0);
        other_auth.start(Some(other_peer), other_type, lfs, now);
    } else {
        other_auth.start(Some(other_peer), 0, lfs, now);
    }
    let block = Arc::new(Cert::new(Kind::PubKeyBlock, &block_raw));
    let peers = [Some(new_peer(&ctx, &block)), Some(new_peer(&refreshed, &block))];
    let mut r = Replay {
        tokens: trace.split(';').filter(|t| !t.is_empty()).map(str::to_string).collect(),
        at: 0,
        status: ST_PENDING,
        lfs: usize::from(lfs),
        key: f[7].parse().unwrap(),
        auth_type: f[5].parse().unwrap(),
        is_async,
        peers,
        other: other_auth,
        sessions: Vec::new(),
        line: n,
        lights: Vec::new(),
    };
    let addr = [1u8, 2, 3, 4, 5, 6];
    let op = if server {
        ee_won_hostop_peerauth_server()
    } else {
        unsafe { ee_won_hostop_peerauth(addr.as_ptr(), use_auth2) }
    };
    let rp: *mut Replay = &mut r;
    unsafe {
        ee_won_hostop_bind(op, Some(host), rp.cast());
        ee_won_hostop_run(op);
    }
    // the driver's Drive: each completion, then the callback (the replay is reached through `rp` meanwhile)
    loop {
        let cb = {
            // SAFETY: no call into the op is running
            let r = unsafe { &mut *rp };
            if !r.tokens.get(r.at).is_some_and(|t| t.starts_with("done(")) {
                break;
            }
            r.at += 1;
            // a light op's completion: the context hands its member the peer data first
            if let Some(t) = r.tokens.get(r.at).filter(|t| t.starts_with("lightpeer(")).cloned() {
                let (call, p) = t.split_once('=').unwrap();
                let name = call.trim_start_matches("lightpeer(").trim_end_matches(')');
                let cert = r.lights.iter().find(|(n, _)| n == name).map(|l| l.1).unwrap();
                let h = r.peer(p) as usize as *mut crate::peerobj::EeWonAuthObj;
                // SAFETY: the op's live member; the handle (or NULL) made here, freed after
                unsafe {
                    crate::authflow::ee_won_light_cert_set_peer(cert, h);
                    crate::peerobj::ee_won_authobj_free(h);
                }
                r.at += 1;
            }
            r.at += 1;
            r.tokens[r.at - 1].clone()
        };
        let (id, o) = cb.trim_start_matches("cb(").trim_end_matches(')').split_once(',').unwrap();
        unsafe { ee_won_hostop_callback(op, id.parse().unwrap(), obj(o)) };
    }
    unsafe { ee_won_hostop_cleanup(op) };
    let session = u8::from(unsafe { ee_won_hostop_num(op, EE_WON_PEEROP_SESSION, 0) } != 0);
    let result = if server {
        format!("session {session} permits {}", unsafe { ee_won_hostop_num(op, EE_WON_PEEROP_CLIENT_PERMITS, 42) })
    } else {
        format!("session {session} gcs {}", unsafe { ee_won_hostop_num(op, EE_WON_PEEROP_GET_CERT_STATUS, 0) })
    };
    unsafe { ee_won_hostop_free(op) };
    assert_eq!(r.at, r.tokens.len(), "line {n}: calls left");
    assert_eq!(format!("{} {result}", r.status), end, "line {n}");
    session == 1 && end.starts_with("0 ")
}

/// The original's statements recorded by tests/won/won_peerop_test.cpp (`--print`), every call with its answer.
#[test]
fn golden() {
    let mut done = 0;
    let mut n = 0;
    for line in include_str!("golden_peerop.txt").lines() {
        done += usize::from(replay(n, line));
        n += 1;
    }
    assert!(n >= 400, "{n}");
    assert!(done >= 20, "{done} successful handshakes");
}
