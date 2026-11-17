//! `golden_fwop.txt` replayed: the stand-ins of `tests/won/won_fwop_test.cpp` (a scripted environment, every call
//! logged), the same setup, and each step's trace, return and members compared with what the original's statements
//! did.

use super::*;
use crate::fwhost::{FwSide, HostEnv};
use crate::hostop::{
    EeWonHostCall, Host, EE_WON_HOST_CLOSE, EE_WON_HOST_NEW_OP, EE_WON_HOST_NEW_SOCKET, EE_WON_HOST_OP_ADDR,
    EE_WON_HOST_OP_RECV_BYTES_FROM, EE_WON_HOST_RELEASE, EE_WON_HOST_RUN, EE_WON_HOST_SOCKET_BIND,
    EE_WON_HOST_SOCKET_LISTEN, EE_WON_HOST_SOCKET_PORT,
};
use crate::reqhost::{Calls, Plain, ReqPlan};
use std::fmt::Write as _;

const ST_TIMED_OUT: i32 = 100001;
const ST_TRY_NEXT_SERVER: i32 = 100057;
const ST_EXIT_LOOP: i32 = 100060;

fn hex(d: &[u8]) -> String {
    if d.is_empty() {
        return "-".into();
    }
    d.iter().map(|b| format!("{b:02x}")).collect()
}

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
}

#[derive(Clone, Copy)]
enum SObj {
    Socket(i32),
    Recv(i32, i32),
}

struct Std {
    objs: Vec<SObj>,
    refs: Vec<i32>,
    trace: String,
    serial: i32,
    mode: i32,
    time_left: u32,
    cur_host: i32,
    bind: i32,
    listen: i32,
    local_port: u16,
    base_callback: bool,
    hosts: Vec<i32>,
    runs: Vec<i32>,
    request: Option<Vec<u8>>,
    response: Vec<u8>,
}

fn idx(o: Obj) -> usize {
    o as usize - 1
}

impl Std {
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
    fn serial(&self, o: Obj) -> i32 {
        if o.is_null() {
            return 0;
        }
        match self.objs[idx(o)] {
            SObj::Socket(s) | SObj::Recv(s, _) => s,
        }
    }
}

impl Env for Std {
    fn release(&mut self, o: Obj) {
        self.refs[idx(o)] -= 1;
        assert!(self.refs[idx(o)] >= 0);
    }
    fn new_socket(&mut self, udp: bool) -> Obj {
        self.serial += 1;
        let s = self.serial;
        self.log(format!("sock#{s} u{}", udp as i32));
        self.add(SObj::Socket(s))
    }
    fn bind(&mut self, socket: Obj, port: u16) -> i32 {
        let s = self.serial(socket);
        self.log(format!("bind#{s} {port}"));
        self.bind
    }
    fn listen(&mut self, socket: Obj) -> i32 {
        let s = self.serial(socket);
        self.log(format!("listen#{s} 1000"));
        self.listen
    }
    fn local_port(&mut self, socket: Obj) -> u16 {
        let s = self.serial(socket);
        self.log(format!("port#{s}"));
        self.local_port
    }
    fn close(&mut self, socket: Obj) {
        let s = self.serial(socket);
        self.log(format!("close#{s}"));
    }
    fn new_recv(&mut self, socket: Obj) -> Obj {
        self.serial += 1;
        let s = self.serial;
        let host = if self.hosts.is_empty() { self.cur_host } else { self.hosts.remove(0) };
        let k = self.serial(socket);
        self.log(format!("recv#{s} 255 s{k}"));
        self.add(SObj::Recv(s, host))
    }
    fn recv_run(&mut self, recv: Obj, wait: u32) -> i32 {
        let s = self.serial(recv);
        self.log(format!("run#{s} 2 {wait}"));
        if self.runs.is_empty() {
            ST_TIMED_OUT
        } else {
            self.runs.remove(0)
        }
    }
    fn recv_from_current(&mut self, recv: Obj) -> bool {
        let SObj::Recv(s, host) = self.objs[idx(recv)] else { panic!() };
        self.log(format!("host#{s}"));
        host == self.cur_host
    }
    fn time_left(&mut self) -> u32 {
        self.log("timeleft".into());
        self.time_left
    }
    fn mode(&mut self) -> i32 {
        self.log("mode".into());
        self.mode
    }
    fn set_request(&mut self, message: &[u8]) {
        self.request = Some(message.to_vec());
    }
    fn response(&mut self) -> Vec<u8> {
        self.response.clone()
    }
    fn invalid_header(&mut self) -> i32 {
        self.log("invalid".into());
        ST_TRY_NEXT_SERVER
    }
    fn set_public(&mut self, host: i32) {
        self.log(format!("public {host}"));
    }
}

fn dump(o: &FirewallOp, e: &Std, lfs: u8) -> String {
    let mut s = format!(
        "port {} wait {} lfs {} sock {} listen {} waitc {} udp {} recv {} pub {} req ",
        o.listen_port,
        o.max_wait,
        lfs,
        e.serial(o.socket),
        o.do_listen as i32,
        o.wait_for_connect as i32,
        o.use_udp as i32,
        e.serial(o.recv),
        o.public.map_or("-".to_string(), |h| h.to_string())
    );
    write!(s, "{}", e.request.as_ref().map_or("-".to_string(), |r| hex(r))).unwrap();
    s
}

// ---- the same replay through the Rust op base (fwhost.rs): every object statement a host call, answered by the
// stand-in as RustServerOp::HostCall answers it; the op's own side the stand-in's

struct TestSide(*mut Std);

// SAFETY (every method): the replay's stand-in, alive and otherwise untouched during the step
impl FwSide for TestSide {
    fn time_left(&mut self, _c: &Calls) -> u32 {
        unsafe { &mut *self.0 }.time_left()
    }
    fn mode(&mut self, _c: &Calls) -> i32 {
        unsafe { &mut *self.0 }.mode()
    }
    fn set_request(&mut self, _c: &Calls, message: &[u8]) {
        unsafe { &mut *self.0 }.set_request(message)
    }
    fn response(&mut self, _c: &Calls) -> Vec<u8> {
        unsafe { &mut *self.0 }.response()
    }
    fn invalid_header(&mut self, _c: &Calls) -> i32 {
        unsafe { &mut *self.0 }.invalid_header()
    }
    fn set_public(&mut self, host: i32) {
        unsafe { &mut *self.0 }.set_public(host)
    }
}

/// The current server's address in the exchange, and another host's.
const CUR: [u8; 6] = [0, 0, 1, 1, 1, 1];
const OTHER: [u8; 6] = [0, 0, 2, 2, 2, 2];

/// The host calls answered by the stand-in (`host` the replay's `Std`).
unsafe extern "C" fn std_host(host: *mut c_void, call: *mut EeWonHostCall) {
    let e = unsafe { &mut *(host as *mut Std) };
    let c = unsafe { &mut *call };
    let o0 = c.obj[0];
    let (n0, n1) = (c.num[0], c.num[1]);
    match c.code {
        EE_WON_HOST_RELEASE => e.release(o0),
        EE_WON_HOST_NEW_SOCKET => c.out_obj = e.new_socket(n0 != 0),
        EE_WON_HOST_SOCKET_BIND => {
            assert_eq!(n1, 0);
            c.out_num = i64::from(e.bind(o0, n0 as u16))
        }
        EE_WON_HOST_SOCKET_LISTEN => c.out_num = i64::from(e.listen(o0)),
        EE_WON_HOST_SOCKET_PORT => c.out_num = i64::from(e.local_port(o0)),
        EE_WON_HOST_CLOSE => e.close(o0),
        EE_WON_HOST_NEW_OP => {
            assert_eq!((n0, n1), (EE_WON_HOST_OP_RECV_BYTES_FROM, 255));
            c.out_obj = e.new_recv(o0)
        }
        EE_WON_HOST_RUN => {
            assert_eq!(n0, 2);
            c.out_num = i64::from(e.recv_run(o0, n1 as u32))
        }
        EE_WON_HOST_OP_ADDR => c.out_addr = if e.recv_from_current(o0) { CUR } else { OTHER },
        k => panic!("host call {k}"),
    }
}

/// `$body` over the stand-in itself, or over the base's host calls answered by it.
macro_rules! env {
    ($host:expr, $e:ident, |$v:ident| $body:expr) => {
        match $host {
            Some(h) => {
                let p: *mut Std = &mut *$e;
                h.bind_to(Some(std_host), p as *mut c_void);
                let calls = Calls { h };
                let mut he = HostEnv { c: &calls, side: TestSide(p) };
                let $v = &mut he;
                $body
            }
            None => {
                let $v = &mut *$e;
                $body
            }
        }
    };
}

/// `RecvServerDatagram` as the C++ shim runs it (and the base, `reqhost::recv_datagram`).
fn datagram(o: &mut FirewallOp, e: &mut Std, host: Option<&Host<ReqPlan>>) -> i32 {
    match env!(host, e, |v| o.recv_datagram(v)) {
        Datagram::Async(w) => {
            let s = e.serial(o.recv);
            e.log(format!("track#{s} 1000"));
            e.log(format!("async#{s} {w}"));
            ST_EXIT_LOOP
        }
        Datagram::Status(s) => s,
    }
}

fn list(s: &str, p: &str) -> Vec<i32> {
    let s = s.strip_prefix(p).unwrap();
    if s.is_empty() {
        return Vec::new();
    }
    s.split(',').map(|x| x.parse().unwrap()).collect()
}

fn replay(setup: &str, steps: &[&str]) -> usize {
    replay_on(setup, steps, None)
}

fn replay_on(setup: &str, steps: &[&str], host: Option<&Host<ReqPlan>>) -> usize {
    if let Some(h) = host {
        h.st().ex.cur_addr = CUR;
    }
    let f: Vec<&str> = setup.split(' ').collect();
    let n = |i: usize| f[i].parse::<i64>().unwrap();
    let mut std = Std {
        objs: Vec::new(),
        refs: Vec::new(),
        trace: String::new(),
        serial: 0,
        mode: n(1) as i32,
        time_left: n(2) as u32,
        cur_host: n(3) as i32,
        bind: n(4) as i32,
        listen: n(5) as i32,
        local_port: n(6) as u16,
        base_callback: n(7) != 0,
        hosts: list(f[12], "h"),
        runs: list(f[13], "r"),
        request: None,
        response: Vec::new(),
    };
    let e = &mut std;
    let mut o = FirewallOp::new();
    let mut lfs = 2u8;
    if n(9) == 0 {
        o.use_udp = n(8) != 0;
        lfs = n(10) as u8;
        o.max_wait = n(11) as u32;
    }
    for line in steps {
        let (step, want) = line.split_once(" => ").unwrap();
        let t: Vec<&str> = step.split(' ').collect();
        let (kind, param, status) = (t[1].parse::<i32>().unwrap(), t[2].parse::<i32>().unwrap(), t[3].parse().unwrap());
        let (a, b) = (t[4] == "1", t[5] == "1");
        if kind == 3 && param == 1000 && status == 0 && o.recv.is_null() {
            assert_eq!(want, "skip");
            continue;
        }
        e.trace.clear();
        let mut r = 0;
        let mut threw = false;
        match kind {
            0 => match env!(host, e, |v| o.run_hook(v)) {
                Some(s) => e.log(format!("finish {s}")),
                None => e.log("baserun".into()),
            },
            1 => r = env!(host, e, |v| o.get_next_request(v, lfs)),
            2 => {
                e.response = unhex(t[6]);
                match env!(host, e, |v| o.check_response(v)) {
                    Ok(Check::Status(s)) => r = s,
                    Ok(Check::Datagram) => r = datagram(&mut o, e, host),
                    Err(Cut) => threw = true,
                }
            }
            3 => {
                r = 1;
                match env!(host, e, |v| o.callback_hook(v, param, status)) {
                    Callback::Base => {
                        e.log(format!("basecb {param}"));
                        r = e.base_callback as i32;
                    }
                    Callback::Datagram => {
                        datagram(&mut o, e, host);
                    }
                    Callback::Finish(s) => e.log(format!("finish {s}")),
                }
            }
            4 => {
                e.log("baseclean".into());
                env!(host, e, |v| o.cleanup_hook(v));
            }
            5 => r = datagram(&mut o, e, host),
            6 => {
                o.listen_port = param as u16;
                o.do_listen = a;
                o.wait_for_connect = b;
            }
            _ => panic!("{kind}"),
        }
        let got = format!("{} {}|{}|{}", threw as i32, r, e.trace, dump(&o, e, lfs));
        assert_eq!(got, want, "\n{setup}\n{step}");
    }
    env!(host, e, |v| o.release_all(v));
    assert!(e.refs.iter().all(|&r| r == 0), "{setup}: {:?}", e.refs);
    steps.len()
}

#[test]
fn golden() {
    let text = include_str!("golden_fwop.txt");
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
    assert_eq!(sequences, 500);
    assert!(steps > 2500, "{steps}");
}

#[test]
fn golden_through_the_base() {
    let text = include_str!("golden_fwop.txt");
    let h = Host::new(ReqPlan::new(Box::new(Plain(Box::new(crate::reqops::UserIdOp::default())))));
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
    assert!(steps > 2500, "{steps}");
}

#[test]
fn defaults_and_datagram_wait() {
    let o = FirewallOp::new();
    assert!(o.do_listen && o.wait_for_connect && !o.use_udp);
    assert_eq!((o.listen_port, o.max_wait, o.public), (0, 0, None));
    // an asynchronous wait of 0 is forever; the longest wait caps the time left
    assert_eq!(firewall_wait(0, 0, true), u32::MAX);
    assert_eq!(firewall_wait(500, 20, false), 20);
}
