//! `golden_hostop.txt` replayed: the stand-in of `tests/won/won_hostop_test.cpp` (every call of the op answered
//! from a generated stream and logged), the same steps, and each step's return, trace and members compared with what
//! the original's statements did; after each sequence the op is freed and every reference it took must be gone.

use super::*;
use crate::filepush::*;
use crate::ftpop::*;
use crate::multiping::*;
use crate::reqhost::*;
use crate::reqops::{
    EE_WON_REQOP_CONTEST_REPLY, EE_WON_REQOP_PING_LAG, EE_WON_REQOP_PING_TEXT, EE_WON_REQOP_PING_VERSION,
    EE_WON_REQOP_USER_ID,
};
use std::cell::RefCell;
use std::collections::BTreeMap;
use std::fmt::Write as _;

#[derive(Default)]
struct Standin {
    seed: u32,
    serial: i32,
    status: i32,
    mode: i32,
    lfs: i32,
    last_socket: i32,
    calls: i32,
    tick: u32,
    line: i32,
    /// a request exchange's sequence (its own answers)
    req: bool,
    auth: i32,
    rands: Vec<i32>,
    addrs: Vec<[u8; 6]>,
    own: BTreeMap<i32, i32>,
    kinds: BTreeMap<i32, i64>,
    trace: String,
}

thread_local! {
    static S: RefCell<Standin> = RefCell::new(Standin::default());
}

impl Standin {
    fn next(&mut self) -> u32 {
        self.seed = self.seed.wrapping_mul(1103515245).wrapping_add(12345);
        (self.seed >> 8) & 0xffffff
    }
    fn pick(&mut self, n: i32) -> i32 {
        (self.next() % n as u32) as i32
    }
    fn log(&mut self, s: String) {
        self.trace.push_str(&s);
        self.trace.push(';');
    }
    fn over(&self) -> bool {
        self.calls > 300
    }
    fn send_status(&mut self) -> i32 {
        if self.pick(6) == 0 || self.over() {
            100001
        } else {
            0
        }
    }
    fn recv_status(&mut self) -> i32 {
        const K: [i32; 8] = [0, 0, 0, 0, 100001, 100002, 10040, 100012];
        let s = K[self.pick(8) as usize];
        if self.over() {
            100001
        } else {
            s
        }
    }
    fn status_reply(&mut self) -> Vec<u8> {
        let s: i32 = if self.pick(3) == 0 { 0xffff } else { 0 };
        vec![5, 5, 0, 22, 0, (s & 0xff) as u8, (s >> 8) as u8]
    }
    fn ping_reply(id: i32) -> Vec<u8> {
        let b = id.to_le_bytes();
        vec![3, 1, 6, b[0], b[1], b[2], b[3]]
    }
    fn bytes(&mut self) -> Option<Vec<u8>> {
        let k = self.pick(8);
        match k {
            0 => None,
            1 => Some(Vec::new()),
            2 => Some(vec![self.pick(3) as u8]),
            3 | 4 => {
                let mut b = self.status_reply();
                if k == 4 {
                    let n = self.pick(7) as usize;
                    b.truncate(n);
                }
                Some(b)
            }
            5 | 6 => {
                let id = if !self.rands.is_empty() && self.pick(3) != 0 {
                    let n = self.rands.len() as i32;
                    let i = self.pick(n) as usize;
                    self.rands[i]
                } else {
                    (self.next() & 0x7fff) as i32
                };
                Some(Standin::ping_reply(id))
            }
            _ => {
                let n = self.pick(9);
                Some((0..n).map(|_| (self.next() & 0xff) as u8).collect())
            }
        }
    }
    fn status_bytes(&mut self) -> Option<Vec<u8>> {
        let k = self.pick(4);
        if k == 3 {
            return self.bytes();
        }
        let mut b = self.status_reply();
        if k == 2 {
            let n = self.pick(7) as usize;
            b.truncate(n);
        }
        Some(b)
    }
    fn ack_bytes(&mut self) -> Option<Vec<u8>> {
        if self.pick(4) == 3 {
            return self.bytes();
        }
        Some(vec![1])
    }
    fn ping_bytes(&mut self) -> Option<Vec<u8>> {
        if self.pick(4) == 3 || self.rands.is_empty() {
            return self.bytes();
        }
        let n = self.rands.len();
        let k = self.pick(if n < 3 { n as i32 } else { 3 }) as usize;
        Some(Standin::ping_reply(self.rands[n - 1 - k]))
    }
    fn line_bytes(&mut self) -> Option<Vec<u8>> {
        const K: [&str; 14] = [
            "220 ready",
            "331 need password",
            "230 logged in",
            "250 ok",
            "200 type set",
            "227 Entering Passive Mode (10,0,0,1,4,1)",
            "350 restarting",
            "504 not implemented",
            "150 opening",
            "226 done",
            "530 not logged in",
            "220-hello",
            "227 bad",
            "",
        ];
        // mostly the next line of a successful session, else any
        const SESSION: [usize; 9] = [0, 1, 2, 3, 4, 5, 6, 8, 9];
        let i = if self.pick(5) != 0 {
            let l = SESSION[(self.line % 9) as usize];
            self.line += 1;
            l
        } else {
            self.pick(15) as usize
        };
        if i == 14 {
            return None;
        }
        let mut b = K[i].as_bytes().to_vec();
        b.push(0);
        Some(b)
    }
    /// a request exchange's reply: a reused session's "expired", a ping reply, a DB proxy reply, anything
    fn req_bytes(&mut self) -> Option<Vec<u8>> {
        let k = self.pick(6);
        if k == 0 {
            return Some(vec![1, 0, 0, 0, 15, 0, 0, 0, self.pick(3) as u8, 0]);
        }
        if k == 1 || k == 2 {
            let t = self.tick.wrapping_sub(self.pick(50) as u32).to_le_bytes();
            let mut b = vec![3, 1, 6, t[0], t[1], t[2], t[3], self.pick(2) as u8];
            b.truncate(if k == 1 { 7 } else { 8 });
            return Some(b);
        }
        if k == 3 {
            let last = if self.pick(3) != 0 { 0x80 } else { 0 };
            let mut b = vec![5, 14, 0, 0xea, 3, 17, 0, 0, 0, 0, 0, last, 7, 0, 0, 0];
            if self.pick(4) == 0 {
                let n = self.pick(b.len() as i32) as usize;
                b.truncate(n);
            }
            return Some(b);
        }
        self.bytes()
    }
    fn addr(&mut self) -> [u8; 6] {
        if !self.addrs.is_empty() && self.pick(4) != 0 {
            let n = self.addrs.len() as i32;
            let i = self.pick(n) as usize;
            return self.addrs[i];
        }
        let mut a = [0u8; 6];
        for x in a.iter_mut() {
            *x = (self.next() & 0xff) as u8;
        }
        a
    }
    fn new_obj(&mut self) -> i32 {
        self.serial += 1;
        self.own.insert(self.serial, 1);
        self.serial
    }
}

fn blog(b: &Option<Vec<u8>>) -> String {
    match b {
        None => "null".into(),
        Some(b) => hex(b),
    }
}

unsafe fn give(c: &mut EeWonHostCall, b: &Option<Vec<u8>>) {
    if let Some(b) = b {
        // SAFETY: the op's sink, during the call
        unsafe { (c.out_sink)(c.out_context, b.as_ptr(), b.len()) };
    }
}

unsafe extern "C" fn host_fn(_host: *mut c_void, call: *mut EeWonHostCall) {
    // SAFETY: the op's call, during it
    let c = unsafe { &mut *call };
    S.with(|s| {
        let mut s = s.borrow_mut();
        s.calls += 1;
        let o0 = c.obj[0] as usize as i32;
        let o1 = c.obj[1] as usize as i32;
        let (n0, n1) = (c.num[0], c.num[1]);
        // SAFETY: the call's bytes
        let data = unsafe { ffi::bytes(c.data, c.len) }.to_vec();
        let addr = c.addr;
        match c.code {
            EE_WON_HOST_STATUS => c.out_num = i64::from(s.status),
            EE_WON_HOST_MODE => c.out_num = i64::from(s.mode),
            EE_WON_HOST_LFS => c.out_num = i64::from(s.lfs),
            EE_WON_HOST_TIME_LEFT => {
                const K: [u32; 4] = [0, 7, 300, 5000];
                let mut v = K[s.pick(4) as usize];
                if s.over() {
                    v = 0;
                }
                c.out_num = i64::from(v);
                s.log(format!("tl={v}"));
            }
            EE_WON_HOST_TICK => {
                let d = s.pick(1500) as u32;
                s.tick = s.tick.wrapping_add(d);
                c.out_num = i64::from(s.tick);
                let t = s.tick;
                s.log(format!("tick={t}"));
            }
            EE_WON_HOST_RAND => {
                let v = (s.next() & 0x7fff) as i32;
                s.rands.push(v);
                c.out_num = i64::from(v);
                s.log(format!("rand={v}"));
            }
            EE_WON_HOST_SRAND => s.log("srand".into()),
            EE_WON_HOST_SEND_TIME | EE_WON_HOST_RECV_TIME => {
                let v = if s.pick(2) != 0 { 20000 } else { 0 };
                c.out_num = v;
                let n = if c.code == EE_WON_HOST_SEND_TIME { "stime" } else { "rtime" };
                s.log(format!("{n}={v}"));
            }
            EE_WON_HOST_NEW_SOCKET => {
                let id = s.new_obj();
                s.last_socket = id;
                c.out_obj = id as usize as *mut c_void;
                s.log(format!("sock#{id} u{}", n0 as i32));
            }
            EE_WON_HOST_NEW_OP => {
                let id = s.new_obj();
                c.out_obj = id as usize as *mut c_void;
                if n0 == EE_WON_HOST_OP_SEND_BYTES_TO {
                    s.addrs.push(addr);
                }
                s.kinds.insert(id, n0);
                s.log(format!("op#{id} k{} s{o0} n{} a{} d{}", n0 as i32, n1 as i32, hex(&addr), hex(&data)));
            }
            EE_WON_HOST_NEW_TIMER => {
                let id = s.new_obj();
                c.out_obj = id as usize as *mut c_void;
                s.log(format!("timer#{id} id{}", n0 as i32));
            }
            EE_WON_HOST_NEW_PEER_AUTH => {
                let id = s.new_obj();
                c.out_obj = id as usize as *mut c_void;
                s.log(format!("pauth#{id} s{o0} a{} {}", hex(&addr), n0 as i32));
            }
            EE_WON_HOST_OP_STATUS => {
                let v = if s.kinds.get(&o0) == Some(&EE_WON_HOST_OP_RECV_CR_MSG) {
                    s.send_status()
                } else {
                    s.recv_status()
                };
                let v = if s.req && s.pick(6) == 0 { 100053 } else { v };
                c.out_num = i64::from(v);
                s.log(format!("st#{o0}={v}"));
            }
            EE_WON_HOST_OP_BYTES => {
                let b = if s.kinds.get(&o0) == Some(&EE_WON_HOST_OP_RECV_CR_MSG) {
                    s.line_bytes()
                } else if s.req {
                    s.req_bytes()
                } else {
                    s.bytes()
                };
                // SAFETY: during the call
                unsafe { give(c, &b) };
                s.log(format!("bytes#{o0}={}", blog(&b)));
            }
            EE_WON_HOST_OP_ADDR => {
                let a = s.addr();
                c.out_addr = a;
                s.log(format!("addr#{o0}={}", hex(&a)));
            }
            EE_WON_HOST_OP_SOCKET => {
                let v = if s.pick(5) == 0 { 999 } else { s.last_socket };
                c.out_obj = v as usize as *mut c_void;
                s.log(format!("osock#{o0}={v}"));
            }
            EE_WON_HOST_PEER_AUTH_KEY => {
                let is_null = s.pick(4) == 0;
                let k = if is_null { None } else { Some((0..8).map(|_| (s.next() & 0xff) as u8).collect()) };
                // SAFETY: during the call
                unsafe { give(c, &k) };
                s.log(format!("key#{o0}={}", blog(&k)));
            }
            EE_WON_HOST_SOCKET_BIND => {
                let v = if s.pick(4) == 0 { 10048 } else { 0 };
                c.out_num = v;
                s.log(format!("bind#{o0} {} {}={v}", n0 as i32, n1 as i32));
            }
            EE_WON_HOST_SOCKET_SET_LFS => s.log(format!("lfs#{o0} {}", n0 as i32)),
            EE_WON_HOST_SEND_BYTES_TO => {
                let v = s.send_status();
                c.out_num = i64::from(v);
                s.log(format!("sendto#{o0} {} {} t{}={v}", hex(&data), hex(&addr), n0 as u32));
            }
            EE_WON_HOST_RECV_BYTES_FROM => {
                let v = s.recv_status();
                let b = s.ping_bytes();
                let a = s.addr();
                c.out_num = i64::from(v);
                // SAFETY: during the call
                unsafe { give(c, &b) };
                c.out_addr = a;
                s.log(format!("recvfrom#{o0} n{} t{}={v} {} {}", n0 as i32, n1 as u32, blog(&b), hex(&a)));
            }
            EE_WON_HOST_SEND_BYTES | EE_WON_HOST_SEND_MSG => {
                let v = s.send_status();
                c.out_num = i64::from(v);
                let n = if c.code == EE_WON_HOST_SEND_BYTES { "send" } else { "sendmsg" };
                s.log(format!("{n}#{o0} {} t{}={v}", hex(&data), n0 as u32));
            }
            EE_WON_HOST_RECV_BYTES | EE_WON_HOST_RECV_MSG => {
                let v = s.send_status();
                let b = if c.code == EE_WON_HOST_RECV_BYTES {
                    s.ack_bytes()
                } else if s.req {
                    s.req_bytes()
                } else {
                    s.status_bytes()
                };
                c.out_num = i64::from(v);
                // SAFETY: during the call
                unsafe { give(c, &b) };
                if c.code == EE_WON_HOST_RECV_BYTES {
                    s.log(format!("recv#{o0} n{} t{}={v} {}", n0 as i32, n1 as u32, blog(&b)));
                } else {
                    s.log(format!("recvmsg#{o0} t{}={v} {}", n0 as u32, blog(&b)));
                }
            }
            EE_WON_HOST_RUN_BLOCK => {
                let p = s.pick(3);
                let v = i64::from(p != 0 && !s.over());
                c.out_num = v;
                s.log(format!("block#{o0} t{}={v}", n0 as u32));
            }
            EE_WON_HOST_FILE_OPEN => {
                let v = i64::from(s.pick(5) != 0);
                c.out_num = v;
                s.log(format!("fopen {}={v}", hex(&data)));
            }
            EE_WON_HOST_FILE_READ => {
                const K: [u32; 4] = [0, 3, 17, 64];
                let mut n = K[s.pick(4) as usize];
                if (n0 as u64) < u64::from(n) {
                    n = n0 as u32;
                }
                let b: Vec<u8> = (0..n).map(|_| (s.next() & 0xff) as u8).collect();
                // SAFETY: during the call
                unsafe { give(c, &Some(b.clone())) };
                s.log(format!("fread {}={}", n0 as u32, hex(&b)));
            }
            EE_WON_HOST_FILE_CLOSE => s.log("fclose".into()),
            EE_WON_HOST_OP_KILLED => {
                let v = i64::from(s.pick(6) == 0);
                c.out_num = v;
                s.log(format!("killed#{o0}={v}"));
            }
            EE_WON_HOST_RESOLVE => {
                let a = s.addr();
                c.out_addr = a;
                c.out_num = 1;
                s.log(format!("resolve {} {}={}", hex(&data), n0 as i32, hex(&a)));
            }
            EE_WON_HOST_CONNECT => {
                let v = s.send_status();
                c.out_num = i64::from(v);
                s.log(format!("connect#{o0} {} t{}={v}", hex(&addr), n0 as u32));
            }
            EE_WON_HOST_RECV_PROGRESS => s.log(format!("progress#{o0} c{o1} {}", n0 as i32)),
            EE_WON_HOST_RECV_FILE => s.log(format!("rfile#{o0} {} {}", hex(&data), n0 as i32)),
            EE_WON_HOST_FILE_SIZE => {
                let v = if s.pick(3) == 0 { -1 } else { s.pick(5000) };
                c.out_num = i64::from(v);
                s.log(format!("fsize {}={v}", hex(&data)));
            }
            EE_WON_HOST_FINISH => {
                s.log(format!("finish {}", n0 as i32));
                if s.status == 100000 {
                    s.status = n0 as i32;
                }
            }
            EE_WON_HOST_KILL => s.log(format!("kill#{o0} {}", n0 as i32)),
            EE_WON_HOST_TRACK => s.log(format!("track#{o0} {}", n0 as i32)),
            EE_WON_HOST_QUEUE => s.log(format!("queue#{o1} s{o0} t{}", n0 as u32)),
            EE_WON_HOST_RUN_ASYNC => s.log(format!("async#{o0} t{}", n0 as u32)),
            EE_WON_HOST_CLOSE => s.log(format!("close#{o0}")),
            EE_WON_HOST_SET_REPEAT => s.log(format!("repeat#{o1} s{o0} id{}", n0 as i32)),
            EE_WON_HOST_COMPLETE => s.log(format!("complete#{o0}")),
            EE_WON_HOST_BASE_CLEANUP => s.log("basecleanup".into()),
            EE_WON_HOST_RELEASE => *s.own.entry(o0).or_insert(0) -= 1,
            EE_WON_HOST_ADD_REF => *s.own.entry(o0).or_insert(0) += 1,
            EE_WON_HOST_AUTH_TYPE => c.out_num = i64::from(s.auth),
            EE_WON_HOST_HAS_AUTH => {
                c.out_num = i64::from(s.pick(5) != 0);
                s.log(format!("hasauth={}", c.out_num));
            }
            EE_WON_HOST_CONNECT_TIME | EE_WON_HOST_TIMEOUT => {
                let v: u32 = if s.pick(2) != 0 { 20000 } else { 0 };
                c.out_num = i64::from(v);
                let n = if c.code == EE_WON_HOST_TIMEOUT { "timeout" } else { "ctime" };
                s.log(format!("{n}={v}"));
            }
            EE_WON_HOST_CERT_USER_ID => {
                let v = s.next();
                c.out_num = i64::from(v);
                s.log(format!("uid={v}"));
            }
            EE_WON_HOST_CTX_ADDRS => {
                let n = s.pick(4);
                let mut b = Vec::new();
                for _ in 0..n {
                    let a = s.addr();
                    b.extend_from_slice(&a);
                }
                let null = n == 0 && s.pick(2) == 0;
                let given = if null { None } else { Some(b.clone()) };
                give(c, &given);
                s.log(format!("addrs#{o0}={}", hex(&b)));
            }
            EE_WON_HOST_CTX_FAILED => s.log(format!("failed#{o0} {}", hex(&addr))),
            EE_WON_HOST_GET_SESSION | EE_WON_HOST_PEER_AUTH_SESSION => {
                let id = if s.pick(3) == 0 { 0 } else { s.new_obj() };
                c.out_obj = id as usize as *mut c_void;
                if c.code == EE_WON_HOST_GET_SESSION {
                    s.log(format!("session {}={id}", hex(&addr)));
                } else {
                    s.log(format!("psession#{o0}={id}"));
                }
            }
            EE_WON_HOST_PUT_SESSION => s.log(format!("put {} {o0}", hex(&addr))),
            EE_WON_HOST_ADD_CRYPT => s.log(format!("crypt#{o0} {o1}")),
            EE_WON_HOST_KILL_ALL => s.log("killall".into()),
            EE_WON_HOST_PEER_AUTH_CERT => {
                const K: [i32; 4] = [0, 100001, 100062, 100072];
                let v = K[s.pick(4) as usize];
                c.out_num = i64::from(v);
                s.log(format!("cert#{o0}={v}"));
            }
            EE_WON_HOST_OP_IS_RECV_MSG => {
                c.out_num = i64::from(s.pick(4) != 0);
                s.log(format!("isrecv#{o0}={}", c.out_num));
            }
            code => s.log(format!("unknown {code}")),
        }
    });
}

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
}

const ADDRS: [[u8; 6]; 6] = [
    [0x00, 0x50, 0x0a, 0x00, 0x00, 0x01],
    [0x00, 0x50, 0x0a, 0x00, 0x00, 0x02],
    [0x1f, 0x90, 0x0a, 0x00, 0x00, 0x01],
    [0x00, 0x51, 0xc0, 0xa8, 0x01, 0x07],
    [0x00, 0x50, 0x0a, 0x00, 0x00, 0x09],
    [0x02, 0x00, 0x7f, 0x00, 0x00, 0x01],
];

struct Op(*mut EeWonHostOp);

impl Op {
    fn h(&self) -> &EeWonHostOp {
        // SAFETY: live until freed
        unsafe { &*self.0 }
    }
    fn set(&self, which: i32, num: i64, data: &[u8], addr: Option<[u8; 6]>, obj: i32) {
        let v = SetValue { num, data, addr: addr.unwrap_or([0; 6]), obj: Obj(obj as usize) };
        self.h().op.set(which, &v);
    }
    fn num(&self, which: i32, i: i64) -> i64 {
        self.h().op.num(which, i)
    }
}

fn ping_results(op: &Op) -> String {
    let n = op.num(EE_WON_MULTIPING_COUNT, 0);
    let mut r = format!("n{n}");
    for i in 0..n {
        let a = op.h().op.bytes(EE_WON_MULTIPING_ADDR, i).unwrap_or_default();
        let _ = write!(
            r,
            "{} {}:{}:{}:{}:{}",
            hex(&a),
            op.num(EE_WON_MULTIPING_TIME, i) as i32,
            op.num(EE_WON_MULTIPING_FAILURES, i) as i32,
            op.num(EE_WON_MULTIPING_ENTITY, i) as i32,
            op.num(EE_WON_MULTIPING_KEEPS, i) as i32,
            op.num(EE_WON_MULTIPING_KEEPS_ALL, i) as i32
        );
    }
    r
}

fn with<R>(f: impl FnOnce(&mut Standin) -> R) -> R {
    S.with(|s| f(&mut s.borrow_mut()))
}

/// One step on the Rust op (the driver's `PingStep` / `PushStep`, Rust half): the line after "=>".
fn step(op: &Op, which: char, kind: i32, a: i64, b: i64, data: &[u8], timer: &mut i32) -> String {
    with(|s| {
        s.trace.clear();
        s.calls = 0;
    });
    let mut ret = "-".to_string();
    let h = &op.h().op;
    if which == 'M' {
        match kind {
            0 | 1 => {
                let entity = if b != 0 { with(|s| s.new_obj()) } else { 0 };
                op.set(EE_WON_MULTIPING_ADD, 0, &[], Some(ADDRS[a as usize]), entity);
                if entity != 0 {
                    with(|s| *s.own.get_mut(&entity).unwrap() -= 1);
                }
            }
            2 => op.set(EE_WON_MULTIPING_CLEAR, 0, &[], None, 0),
            3 => op.set(EE_WON_MULTIPING_BATCH_SIZE, a, &[], None, 0),
            4 => op.set(EE_WON_MULTIPING_MAX_FAILURES, a, &[], None, 0),
            5 => op.set(EE_WON_MULTIPING_MAX_PING_TIME, a, &[], None, 0),
            6 => {
                with(|s| {
                    s.mode = a as i32;
                    s.status = 100000;
                });
                h.run();
            }
            7 | 8 => {
                let done = with(|s| {
                    s.serial += 1;
                    s.serial
                });
                ret = if h.callback(1, Obj(done as usize)) { "1" } else { "0" }.into();
            }
            9 => {
                let done = if a == 0 {
                    *timer
                } else {
                    with(|s| {
                        s.serial += 1;
                        s.serial
                    })
                };
                ret = if h.callback(2, Obj(done as usize)) { "1" } else { "0" }.into();
            }
            10 => h.cleanup(),
            11 => ret = ping_results(op),
            _ => {}
        }
    } else if which == 'R' {
        match kind {
            0 => {
                with(|s| {
                    s.mode = a as i32;
                    s.auth = b as i32;
                    s.status = 100000;
                });
                h.run();
            }
            1..=3 => {
                let done = with(|s| {
                    s.serial += 1;
                    s.serial
                });
                let param = if kind == 3 { 7 } else { kind };
                ret = if h.callback(param, Obj(done as usize)) { "1" } else { "0" }.into();
            }
            4 => h.cleanup(),
            5 => {
                let u = if a != 0 { with(|s| s.new_obj()) } else { 0 };
                op.set(EE_WON_REQHOST_UDP_SOCKET, 0, &[], None, u);
                if u != 0 {
                    with(|s| *s.own.get_mut(&u).unwrap() -= 1);
                }
            }
            6 => {
                let mut r = hex(&h.bytes(EE_WON_REQHOST_CUR_ADDR, 0).unwrap_or_default());
                for w in [EE_WON_REQOP_PING_LAG, EE_WON_REQOP_PING_VERSION, EE_WON_REQOP_USER_ID] {
                    let _ = write!(r, " {}", op.num(w, 0));
                }
                for w in [EE_WON_REQOP_CONTEST_REPLY, EE_WON_REQOP_PING_TEXT] {
                    let b = h.bytes(w, 0);
                    let _ = write!(r, " {}", b.as_ref().map_or("null".to_string(), |b| hex(b)));
                }
                ret = r;
            }
            _ => {}
        }
    } else if which == 'P' {
        match kind {
            0 => op.set(EE_WON_FTPGET_LOCAL_PATH, 0, data, None, 0),
            1 => op.set(EE_WON_FTPGET_REMOTE_PATH, 0, data, None, 0),
            2 => op.set(EE_WON_FTPGET_USER, 0, data, None, 0),
            3 => op.set(EE_WON_FTPGET_PASSWORD, 0, data, None, 0),
            4 => op.set(EE_WON_FTPGET_BINARY, a, &[], None, 0),
            5 => op.set(EE_WON_FTPGET_RESUME, a, &[], None, 0),
            6 => {
                let c = if a != 0 { with(|s| s.new_obj()) } else { 0 };
                op.set(EE_WON_FTPGET_CHUNK, b, &[], None, c);
                if c != 0 {
                    with(|s| *s.own.get_mut(&c).unwrap() -= 1);
                }
            }
            7 => {
                with(|s| {
                    s.mode = a as i32;
                    s.status = 100000;
                });
                h.run();
            }
            8 | 9 => {
                let done = with(|s| {
                    s.serial += 1;
                    if a == 2 {
                        s.kinds.insert(s.serial, EE_WON_HOST_OP_RECV_CR_MSG);
                    }
                    s.serial
                });
                ret = if h.callback(a as i32, Obj(done as usize)) { "1" } else { "0" }.into();
            }
            10 => h.cleanup(),
            11 => {
                let c = h.bytes(EE_WON_FTPGET_CONTENT, 0);
                let l = h.bytes(EE_WON_FTPGET_LOCAL, 0).unwrap_or_default();
                let m = h.bytes(EE_WON_FTPGET_REMOTE, 0).unwrap_or_default();
                ret = format!(
                    "st {} {} {} {}",
                    op.num(EE_WON_FTPGET_STATUS, 0) as i32,
                    c.as_ref().map_or("null".to_string(), |c| hex(c)),
                    hex(&l),
                    hex(&m)
                );
            }
            _ => {}
        }
    } else {
        match kind {
            0 => op.set(EE_WON_FILEPUSH_FILE, 0, data, None, 0),
            1 => op.set(EE_WON_FILEPUSH_REMOTE_FILE, 0, data, None, 0),
            2 => op.set(EE_WON_FILEPUSH_COMMENT, 0, data, None, 0),
            3 => op.set(EE_WON_FILEPUSH_COMPRESS, a, &[], None, 0),
            4 => op.set(EE_WON_FILEPUSH_BLOCK_SIZE, a, &[], None, 0),
            5 => op.set(EE_WON_FILEPUSH_MODIFY_TIME, a, &[], None, 0),
            6 => {
                let c = if a != 0 { with(|s| s.new_obj()) } else { 0 };
                op.set(EE_WON_FILEPUSH_PROGRESS, 0, &[], None, c);
                if c != 0 {
                    with(|s| *s.own.get_mut(&c).unwrap() -= 1);
                }
            }
            7 => {
                with(|s| {
                    s.mode = a as i32;
                    s.lfs = b as i32;
                    s.status = 100000;
                });
                h.run();
            }
            8 | 9 => {
                let done = with(|s| {
                    s.serial += 1;
                    s.serial
                });
                ret = if h.callback(a as i32, Obj(done as usize)) { "1" } else { "0" }.into();
            }
            10 | 11 => h.cleanup(),
            12 => {
                ret = format!(
                    "total {} last {}",
                    op.num(EE_WON_FILEPUSH_TOTAL_SENT, 0) as u32,
                    op.num(EE_WON_FILEPUSH_LAST_SENT, 0) as u32
                )
            }
            _ => {}
        }
    }
    let d = h.describe();
    *timer = d.rfind("timer ").map_or(0, |at| d[at + 6..].parse().unwrap_or(0));
    let trace = with(|s| s.trace.clone());
    format!("{ret}|{trace}|{d}")
}

fn reset(seed: u32) {
    with(|s| *s = Standin { seed, mode: 2, lfs: 4, ..Standin::default() });
}

fn free_and_check(op: Op) {
    // SAFETY: the op made here
    unsafe { ee_won_hostop_free(op.0) };
    with(|s| {
        for (id, n) in &s.own {
            assert_eq!(*n, 0, "object {id}: {n} references left");
        }
    });
}

/// The driver's `NewReqOp`: a contest call, a ping or a user id lookup.
fn new_req_op(kind: u8, arg: u8) -> *mut crate::reqop::EeWonReqOp {
    use crate::admin::EeWonAdminValue;
    let v = |num: i64| EeWonAdminValue { num, data: std::ptr::null(), len: 0 };
    // SAFETY: the values' bytes live through the calls
    unsafe {
        match kind {
            0 => {
                let d = [1u8, 2, 3];
                let vals =
                    [v(i64::from(arg)), v(i64::from(arg & 1)), EeWonAdminValue { num: 0, data: d.as_ptr(), len: 3 }];
                crate::reqops::ee_won_reqops_new(crate::reqops::EE_WON_REQOPS_CONTEST, vals.as_ptr(), 3)
            }
            1 => {
                let vals = [v(if arg & 1 != 0 { 2 } else { 4 }), v(i64::from((arg >> 1) & 1))];
                crate::reqops::ee_won_reqops_new(crate::reqops::EE_WON_REQOPS_PING, vals.as_ptr(), 2)
            }
            _ => {
                let n = [b'b', 0, b'o', 0, b'b', 0];
                crate::reqops::ee_won_reqop_user_id(n.as_ptr(), 6)
            }
        }
    }
}

fn new_op(which: char, addr: &[u8]) -> Op {
    if which == 'R' {
        let r = new_req_op(addr[0], addr[1]);
        // SAFETY: the new ops
        unsafe {
            let lfs = crate::reqop::ee_won_reqop_length_field_size(r);
            with(|s| {
                s.lfs = i32::from(lfs);
                s.req = true;
            });
            let op = Op(crate::reqhost::ee_won_hostop_request(r));
            ee_won_hostop_bind(op.0, Some(host_fn), std::ptr::null_mut());
            if addr[2] != 0 {
                let c = with(|s| s.new_obj());
                op.set(EE_WON_REQHOST_CONTEXT, 0, &[], None, c);
                with(|s| *s.own.get_mut(&c).unwrap() -= 1);
            } else {
                op.set(EE_WON_REQHOST_ADDR, 0, &[], Some(ADDRS[addr[3] as usize]), 0);
            }
            return op;
        }
    }
    let op = if which == 'M' {
        ee_won_multiping_new()
    } else if which == 'P' {
        const URLS: [&str; 4] = [
            "ftp://host.example/dir/file.bin",
            "ftp://bob:pw@ftp.won.net/patch.exe",
            "ftp://anon@h/a/b/c.txt",
            "ftp://h",
        ];
        let u = URLS[addr[0] as usize];
        // SAFETY: the URL's bytes
        unsafe { ee_won_ftpget_new_url(u.as_ptr(), u.len(), addr[1] != 0) }
    } else {
        // SAFETY: six bytes
        unsafe { ee_won_filepush_new(addr.as_ptr()) }
    };
    // SAFETY: the new op
    unsafe { ee_won_hostop_bind(op, Some(host_fn), std::ptr::null_mut()) };
    Op(op)
}

#[test]
fn golden() {
    let text = include_str!("golden_hostop.txt");
    let mut op: Option<Op> = None;
    let mut timer = 0;
    let (mut seqs, mut steps) = (0, 0);
    for (n, line) in text.lines().enumerate() {
        let f: Vec<&str> = line.splitn(7, ' ').collect();
        if f[0] == "S" {
            if let Some(o) = op.take() {
                free_and_check(o);
            }
            reset(f[2].parse().unwrap());
            timer = 0;
            op = Some(new_op(f[1].chars().next().unwrap(), &unhex(f[4])));
            seqs += 1;
            continue;
        }
        // T op kind a b data => expected
        let which = f[1].chars().next().unwrap();
        let (kind, a, b) = (f[2].parse().unwrap(), f[3].parse().unwrap(), f[4].parse().unwrap());
        let data = unhex(f[5]);
        let expected = f[6].strip_prefix("=> ").unwrap();
        let got = step(op.as_ref().unwrap(), which, kind, a, b, &data, &mut timer);
        assert_eq!(got, expected, "golden line {}", n + 1);
        steps += 1;
    }
    if let Some(o) = op.take() {
        free_and_check(o);
    }
    assert!(seqs >= 100 && steps > 2000, "{seqs} sequences, {steps} steps");
}

#[test]
fn calls_outside_the_lock() {
    // a hook re-entering the op (a call answered by another step of it) finds its members free: the plan never
    // calls under its lock (`invoke` asserts it), here with the cleanup run from inside a finish
    struct Reenter;
    static OP: std::sync::atomic::AtomicPtr<EeWonHostOp> = std::sync::atomic::AtomicPtr::new(std::ptr::null_mut());
    unsafe extern "C" fn reentrant(host: *mut c_void, call: *mut EeWonHostCall) {
        // SAFETY: the op's call
        let code = unsafe { (*call).code };
        if code == EE_WON_HOST_FINISH {
            let op = OP.load(std::sync::atomic::Ordering::Relaxed);
            // SAFETY: the live op
            unsafe { ee_won_hostop_cleanup(op) };
        }
        // SAFETY: the stand-in's contract
        unsafe { host_fn(host, call) };
    }
    let _ = Reenter;
    reset(77);
    with(|s| s.mode = 2);
    let op = ee_won_multiping_new();
    OP.store(op, std::sync::atomic::Ordering::Relaxed);
    // SAFETY: the new op
    unsafe {
        ee_won_hostop_bind(op, Some(reentrant), std::ptr::null_mut());
        let a = ADDRS[0];
        ee_won_hostop_set(op, EE_WON_MULTIPING_ADD, 0, std::ptr::null(), 0, a.as_ptr(), std::ptr::null_mut());
        ee_won_hostop_run(op);
        assert!(with(|s| s.trace.contains("finish")));
        assert!(with(|s| s.trace.contains("close#")));
        ee_won_hostop_free(op);
    }
}
