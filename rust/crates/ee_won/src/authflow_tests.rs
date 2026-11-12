//! The flow over Rust stand-ins that behave as `tests/won/won_authctx_test.cpp`'s (an op's run, kill, finish and
//! completion; the "threads" holding running ops; the reference counts, creations and frees), replaying
//! `golden_authflow.txt` (recorded over the original's statements) step by step.

use super::*;
use std::cell::RefCell;

const PENDING: c_long = 100001;
const KILLED: c_long = 100002;

#[derive(Clone, Copy, PartialEq)]
enum Done {
    None,
    Timer,
    Cert,
    Reenter,
}

struct Fake {
    kind: char,
    refs: i32,
    // a session's last use; a peer's validity and expire delta
    last_use: i64,
    valid: bool,
    delta: i64,
    // an op's run
    running: bool,
    finished: bool,
    killed: bool,
    status: c_long,
    cert_sync: bool,
    cert_status: c_long,
    done: Done,
    peer: usize,
}

#[derive(Default)]
struct World {
    trace: Vec<String>,
    objs: Vec<Fake>,
    thread: Vec<usize>,
    cert_sync: bool,
    cert_status: c_long,
    ctx_peer: usize,
    flow: usize,
}

thread_local! {
    static W: RefCell<World> = RefCell::new(World::default());
}

fn with<R>(f: impl FnOnce(&mut World) -> R) -> R {
    W.with(|w| f(&mut w.borrow_mut()))
}
fn log(s: String) {
    with(|w| w.trace.push(s));
}
fn name(id: usize) -> String {
    with(|w| format!("{}{}", w.objs[id].kind, id))
}
fn ptr(id: usize) -> *mut c_void {
    id as *mut c_void
}
fn id(p: *mut c_void) -> usize {
    p as usize
}
fn flow() -> &'static EeWonAuthFlow {
    // SAFETY: the flow of the running script, alive while its steps run
    unsafe { &*(with(|w| w.flow) as *const EeWonAuthFlow) }
}

fn create(kind: char) -> usize {
    let n = with(|w| {
        if w.objs.is_empty() {
            w.objs.push(Fake::new('-', false, 0));
        }
        let f = Fake::new(kind, w.cert_sync, w.cert_status);
        w.objs.push(f);
        w.objs.len() - 1
    });
    log(format!("new:{}", name(n)));
    n
}

impl Fake {
    fn new(kind: char, cert_sync: bool, cert_status: c_long) -> Self {
        Fake {
            kind,
            refs: 0,
            last_use: 0,
            valid: false,
            delta: 0,
            running: false,
            finished: false,
            killed: false,
            status: PENDING,
            cert_sync,
            cert_status,
            done: Done::None,
            peer: 0,
        }
    }
}

fn add_ref(n: usize) {
    with(|w| w.objs[n].refs += 1);
}
fn release(n: usize) {
    let gone = with(|w| {
        w.objs[n].refs -= 1;
        w.objs[n].refs <= 0
    });
    if gone {
        // ~FOp's member (its peer data), then ~FObj's log
        let peer = with(|w| std::mem::take(&mut w.objs[n].peer));
        if peer != 0 {
            release(peer);
        }
        log(format!("free:{}", name(n)));
    }
}
// ConstSmartPtr's assignment
fn assign(slot: impl Fn(&mut World) -> &mut usize, new: usize) {
    let old = with(|w| *slot(w));
    if old == new {
        return;
    }
    if old != 0 {
        release(old);
    }
    if new != 0 {
        add_ref(new);
    }
    with(|w| *slot(w) = new);
}

fn hold(n: usize) {
    add_ref(n);
    with(|w| w.thread.push(n));
}
fn unhold(n: usize) {
    let found = with(|w| {
        let i = w.thread.iter().position(|&x| x == n);
        if let Some(i) = i {
            w.thread.remove(i);
        }
        i.is_some()
    });
    if found {
        release(n);
    }
}
fn complete(n: usize, killed: bool, status: c_long) {
    let done = with(|w| {
        let o = &mut w.objs[n];
        if o.finished {
            return None;
        }
        o.finished = true;
        o.killed = killed;
        o.status = status;
        Some(o.done)
    });
    let Some(done) = done else { return };
    log(format!("done:{}:{}", name(n), status));
    add_ref(n);
    match done {
        Done::None => {}
        Done::Timer => flow().auto_refresh_callback(ptr(n)),
        Done::Cert => flow().get_cert_callback(ptr(n)),
        Done::Reenter => {
            let l = create('l');
            add_ref(l);
            flow().refresh_async(ptr(l), 7);
            release(l);
        }
    }
    unhold(n);
    release(n);
}
fn finish(n: usize, s: c_long) {
    log(format!("finish:{}:{}", name(n), s));
    if with(|w| w.objs[n].running) {
        complete(n, false, s);
    }
}

unsafe extern "C" fn h_add_ref(p: *mut c_void) {
    add_ref(id(p))
}
unsafe extern "C" fn h_release(p: *mut c_void) {
    release(id(p))
}
unsafe extern "C" fn h_last_use(p: *mut c_void) -> i64 {
    log(format!("lastuse:{}", name(id(p))));
    with(|w| w.objs[id(p)].last_use)
}
unsafe extern "C" fn h_kill(p: *mut c_void) {
    let n = id(p);
    log(format!("kill:{}", name(n)));
    if with(|w| w.objs[n].running) {
        complete(n, true, KILLED);
    }
}
unsafe extern "C" fn h_killed(p: *mut c_void) -> bool {
    with(|w| w.objs[id(p)].killed)
}
unsafe extern "C" fn h_status(p: *mut c_void) -> c_long {
    with(|w| w.objs[id(p)].status)
}
#[allow(clippy::unnecessary_cast)] // c_long is i64 on LP64
unsafe extern "C" fn h_run_async(p: *mut c_void, t: c_long) {
    let n = id(p);
    // the golden is Windows' (a 32-bit long)
    log(format!("runasync:{}:{}", name(n), t as i32));
    with(|w| w.objs[n].running = true);
    hold(n);
}
unsafe extern "C" fn h_run(p: *mut c_void, mode: c_long, t: c_ulong) -> c_long {
    let n = id(p);
    log(format!("run:{}:{}:{}", name(n), mode, t));
    if mode == 2 {
        return with(|w| {
            let o = &mut w.objs[n];
            o.finished = true;
            o.status = o.cert_status;
            o.status
        });
    }
    with(|w| w.objs[n].running = true);
    hold(n);
    let (sync, st) = with(|w| (w.objs[n].cert_sync, w.objs[n].cert_status));
    if sync {
        complete(n, false, st);
        return with(|w| w.objs[n].status);
    }
    PENDING
}
unsafe extern "C" fn h_finish(p: *mut c_void, s: c_long) {
    finish(id(p), s)
}
unsafe extern "C" fn h_new_timer(_: *mut c_void) -> *mut c_void {
    let n = create('t');
    add_ref(n);
    with(|w| w.objs[n].done = Done::Timer);
    ptr(n)
}
unsafe extern "C" fn h_new_cert(_: *mut c_void, completion: bool) -> *mut c_void {
    let n = create('c');
    add_ref(n);
    if completion {
        with(|w| w.objs[n].done = Done::Cert);
    }
    ptr(n)
}
unsafe extern "C" fn h_cert_peer(p: *mut c_void) -> *mut c_void {
    let peer = with(|w| w.objs[id(p)].peer);
    if peer == 0 {
        return std::ptr::null_mut();
    }
    add_ref(peer);
    ptr(peer)
}
unsafe extern "C" fn h_set_peer(l: *mut c_void, p: *mut c_void) {
    let (l, p) = (id(l), id(p));
    let pn = if p == 0 { "-".to_string() } else { name(p) };
    log(format!("setpeer:{}:{}", name(l), pn));
    assign(move |w| &mut w.objs[l].peer, p);
}
unsafe extern "C" fn h_peer_valid(_: *mut c_void) -> bool {
    let p = with(|w| w.ctx_peer);
    log(format!("valid:{}", name(p)));
    with(|w| w.objs[p].valid)
}
unsafe extern "C" fn h_peer_delta(_: *mut c_void) -> i64 {
    let p = with(|w| w.ctx_peer);
    log(format!("delta:{}", name(p)));
    with(|w| w.objs[p].delta)
}

fn hooks() -> EeWonAuthFlowHooks {
    EeWonAuthFlowHooks {
        context: std::ptr::null_mut(),
        add_ref: h_add_ref,
        release: h_release,
        session_last_use: h_last_use,
        op_kill: h_kill,
        op_killed: h_killed,
        op_status: h_status,
        op_run_async: h_run_async,
        op_run: h_run,
        op_finish: h_finish,
        new_timer: h_new_timer,
        new_cert_op: h_new_cert,
        cert_peer_data: h_cert_peer,
        light_set_peer_data: h_set_peer,
        peer_valid: h_peer_valid,
        peer_expire_delta: h_peer_delta,
    }
}

fn new_peer(valid: bool, delta: i64) -> usize {
    let p = create('p');
    with(|w| {
        w.objs[p].valid = valid;
        w.objs[p].delta = delta;
    });
    p
}
fn set_ctx_peer(p: usize) {
    assign(|w| &mut w.ctx_peer, p);
    flow().clear_sessions();
}

// the driver's Addr(i): 10.0.0.(1 + i % 3), port 1000 + i / 3
fn key(i: i64) -> [u8; 6] {
    let host: u32 = 0x0a00_0001 + (i % 3) as u32;
    let port = (1000 + i / 3) as u16;
    let p = port.to_be_bytes();
    let a = host.to_be_bytes();
    [p[0], p[1], a[0], a[1], a[2], a[3]]
}

fn held(kind: char, n: i64) -> Option<usize> {
    with(|w| {
        let v: Vec<usize> = w.thread.iter().copied().filter(|&x| w.objs[x].kind == kind).collect();
        if v.is_empty() {
            None
        } else {
            Some(v[(n % v.len() as i64) as usize])
        }
    })
}

fn step(cmd: char, a: [i64; 4], now: &mut i64) {
    let f = flow();
    match cmd {
        'T' => *now = a[0],
        'U' => *now += a[0],
        'O' => {
            let s = create('s');
            with(|w| w.objs[s].last_use = *now - a[1]);
            add_ref(s);
            f.put_session(key(a[0]), ptr(s), *now);
            release(s);
        }
        'P' => {
            let s = create('s');
            with(|w| w.objs[s].last_use = a[1]);
            add_ref(s);
            f.put_session(key(a[0]), ptr(s), *now);
            release(s);
        }
        'N' => f.put_session(key(a[0]), std::ptr::null_mut(), *now),
        'G' => {
            let p = id(f.get_session(key(a[0]), *now));
            log(format!("got:{}", if p == 0 { "-".to_string() } else { name(p) }));
            if p != 0 {
                release(p);
            }
        }
        'D' => {
            let p = new_peer(a[0] != 0, a[1]);
            set_ctx_peer(p);
        }
        'A' => f.set_do_auto_refresh(a[0] != 0),
        'S' => f.start_auto_refresh(a[0] != 0),
        'X' => f.stop_auto_refresh(),
        'K' => f.kill(),
        'R' => {
            with(|w| {
                w.cert_sync = a[2] != 0;
                w.cert_status = a[3] as c_long;
            });
            let mut l = 0;
            if a[0] != 0 {
                l = create('l');
                add_ref(l);
                if a[0] == 2 {
                    with(|w| w.objs[l].done = Done::Reenter);
                }
            }
            f.refresh_async(ptr(l), a[1] as u32);
            if l != 0 {
                release(l);
            }
        }
        'Q' => {
            if let Some(l) = held('l', a[0]) {
                f.refresh_async(ptr(l), a[1] as u32);
            }
        }
        'B' => {
            with(|w| w.cert_status = a[1] as c_long);
            let st = f.refresh_block(a[0] as u32);
            log(format!("block:{st}"));
        }
        'F' => {
            if let Some(t) = held('t', a[0]) {
                complete(t, a[1] != 0, if a[1] != 0 { KILLED } else { 0 });
            }
        }
        'C' => {
            if let Some(c) = held('c', a[0]) {
                if a[2] != 0 {
                    let p = new_peer(a[2] == 2, a[3]);
                    assign(move |w| &mut w.objs[c].peer, p);
                }
                complete(c, false, a[1] as c_long);
            }
        }
        'L' => {
            if let Some(l) = held('l', a[0]) {
                finish(l, a[1] as c_long);
            }
        }
        _ => panic!("step {cmd}"),
    }
}

fn take_trace() -> String {
    let t = with(|w| std::mem::take(&mut w.trace));
    // runs of consecutive frees sorted
    let mut out: Vec<String> = Vec::new();
    let mut run: Vec<String> = Vec::new();
    for tok in t {
        if tok.starts_with("free:") {
            run.push(tok);
            continue;
        }
        run.sort();
        out.append(&mut run);
        out.push(tok);
    }
    run.sort();
    out.append(&mut run);
    out.join(" ")
}

/// One script; the normalized trace of each step and of the end.
fn run_script(steps: &[(char, [i64; 4])]) -> Vec<String> {
    with(|w| *w = World::default());
    let fl = Box::into_raw(Box::new(EeWonAuthFlow::new(hooks())));
    with(|w| w.flow = fl as usize);
    let p = new_peer(false, 0);
    set_ctx_peer(p);
    take_trace();
    let mut now = 1000;
    let mut out = Vec::new();
    for &(c, a) in steps {
        step(c, a, &mut now);
        out.push(take_trace());
    }
    // SAFETY: the box made above, not used after
    drop(unsafe { Box::from_raw(fl) });
    assign(|w| &mut w.ctx_peer, 0);
    while let Some(n) = with(|w| w.thread.pop()) {
        release(n);
    }
    out.push(take_trace());
    // everything freed
    with(|w| assert!(w.objs.iter().skip(1).all(|o| o.refs <= 0)));
    out
}

#[test]
fn golden() {
    let mut scripts = 0;
    let mut steps_seen = 0;
    for line in include_str!("golden_authflow.txt").lines() {
        let mut steps = Vec::new();
        let mut want = Vec::new();
        for part in line.split('|') {
            let (s, t) = part.split_once('=').unwrap();
            want.push(t.to_string());
            if s == "E" {
                continue;
            }
            let f: Vec<&str> = s.split(',').collect();
            let c = f[0].chars().next().unwrap();
            let a = [f[1].parse().unwrap(), f[2].parse().unwrap(), f[3].parse().unwrap(), f[4].parse().unwrap()];
            steps.push((c, a));
        }
        let got = run_script(&steps);
        assert_eq!(got.len(), want.len(), "{line}");
        for (i, (g, w)) in got.iter().zip(&want).enumerate() {
            assert_eq!(g, w, "script {scripts} step {i}: {:?}", steps.get(i));
        }
        scripts += 1;
        steps_seen += steps.len();
    }
    assert_eq!(scripts, 400);
    assert!(steps_seen > 8000);
}

#[test]
fn sessions_expire_and_the_cursor_wraps() {
    // three addresses; the cursor starts at the end, wraps to the first entry, then walks and drops the stale one
    let steps = [
        ('P', [0, 1000, 0, 0]),
        ('P', [1, 1000, 0, 0]),
        ('T', [1400, 0, 0, 0]),
        ('P', [2, 1400, 0, 0]),
        ('G', [0, 0, 0, 0]),
        ('G', [2, 0, 0, 0]),
    ];
    let got = run_script(&steps);
    assert_eq!(got[0], "new:s2");
    assert_eq!(got[1], "new:s3");
    // put s4: the cursor (at address 0) drops address 0 (unused for 400 s) and address 1, then wraps
    assert_eq!(got[3], "new:s4 free:s2 free:s3");
    assert_eq!(got[4], "got:-");
    assert_eq!(got[5], "lastuse:s4 got:s4 free:s4");
}

#[test]
fn refresh_runs_one_request_for_every_waiting_op() {
    let steps = [('R', [1, 5000, 0, 0]), ('R', [1, 6000, 0, 0]), ('C', [0, 0, 2, 900])];
    let got = run_script(&steps);
    assert_eq!(got[0], "new:l2 runasync:l2:5000 new:c3 run:c3:1:60000");
    assert_eq!(got[1], "new:l4 runasync:l4:6000");
    assert!(got[2].starts_with("new:p5 done:c3:0 setpeer:l2:p5 finish:l2:0 done:l2:0 setpeer:l4:p5 finish:l4:0"));
    assert_eq!(flow_status_after(&steps), 0);
}

fn flow_status_after(steps: &[(char, [i64; 4])]) -> c_long {
    with(|w| *w = World::default());
    let fl = Box::into_raw(Box::new(EeWonAuthFlow::new(hooks())));
    with(|w| w.flow = fl as usize);
    let p = new_peer(false, 0);
    set_ctx_peer(p);
    let mut now = 1000;
    for &(c, a) in steps {
        step(c, a, &mut now);
    }
    let st = flow().refresh_status();
    // SAFETY: the box made above
    drop(unsafe { Box::from_raw(fl) });
    assign(|w| &mut w.ctx_peer, 0);
    while let Some(n) = with(|w| w.thread.pop()) {
        release(n);
    }
    st
}
