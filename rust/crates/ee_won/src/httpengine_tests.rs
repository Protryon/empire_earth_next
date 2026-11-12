//! The session and connections over Rust stand-ins that behave as `tests/won/won_httpengine_test.cpp`'s (the
//! HTTPGetOps' runs, kills, forced finishes and completions; the user completions that add an op from inside; the
//! connections' completions holding their connection; the sockets; the reference counts, creations and frees),
//! replaying `golden_httpengine.txt` (recorded over the original's statements) step by step.

use super::*;
use std::cell::RefCell;

const HOSTS: [&str; 5] = ["a.com", "A.COM", "b.org", "c.net", "B.org"];

#[derive(Default)]
struct Fake {
    kind: char,
    refs: i32,
    // an op
    host: Vec<u8>,
    only_send: bool,
    only_recv: bool,
    running: bool,
    finished: bool,
    status: c_long,
    completion: usize,
    socket: usize,
    // a user completion: add an op for host `add - 1`; a connection's: its connection
    add: usize,
    conn: usize,
    // a connection's or the session's Rust object
    conn_obj: Option<Box<EeWonHttpConn>>,
    sess_obj: Option<Box<EeWonHttpSession>>,
}

#[derive(Default)]
struct World {
    trace: Vec<String>,
    objs: Vec<Fake>,
    thread: Vec<usize>,
    now: i64,
    session: usize,
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
    if id == 0 {
        return "-".to_string();
    }
    with(|w| format!("{}{}", w.objs[id].kind, id))
}
fn ptr(id: usize) -> *mut c_void {
    id as *mut c_void
}
fn id(p: *mut c_void) -> usize {
    p as usize
}

fn create(kind: char) -> usize {
    let n = with(|w| {
        if w.objs.is_empty() {
            w.objs.push(Fake::default());
        }
        w.objs.push(Fake { kind, ..Fake::default() });
        w.objs.len() - 1
    });
    log(format!("new:{}", name(n)));
    n
}

fn add_ref(n: usize) {
    with(|w| w.objs[n].refs += 1);
}
fn release(n: usize) {
    let gone = with(|w| {
        w.objs[n].refs -= 1;
        w.objs[n].refs <= 0
    });
    if !gone {
        return;
    }
    // the members (the destructors' order; the frees are compared sorted), then ~FObj's log
    let (socket, completion, conn, conn_obj, sess_obj) = with(|w| {
        let o = &mut w.objs[n];
        (
            std::mem::take(&mut o.socket),
            std::mem::take(&mut o.completion),
            std::mem::take(&mut o.conn),
            o.conn_obj.take(),
            o.sess_obj.take(),
        )
    });
    drop(conn_obj);
    drop(sess_obj);
    for m in [socket, completion, conn] {
        if m != 0 {
            release(m);
        }
    }
    log(format!("free:{}", name(n)));
}
// a SmartPtr assignment: the old released, then the new referenced
fn assign(slot: impl Fn(&mut World) -> &mut usize, new: usize) {
    let old = with(|w| *slot(w));
    if old == new {
        return;
    }
    with(|w| *slot(w) = 0);
    if old != 0 {
        release(old);
    }
    if new != 0 {
        add_ref(new);
    }
    with(|w| *slot(w) = new);
}

fn conn_obj(n: usize) -> &'static EeWonHttpConn {
    let p: *const EeWonHttpConn = with(|w| &**w.objs[n].conn_obj.as_ref().unwrap() as *const EeWonHttpConn);
    // SAFETY: the connection's boxed object, alive while the connection is
    unsafe { &*p }
}
fn session() -> Option<&'static EeWonHttpSession> {
    let n = with(|w| w.session);
    if n == 0 {
        return None;
    }
    let p: *const EeWonHttpSession = with(|w| &**w.objs[n].sess_obj.as_ref().unwrap() as *const EeWonHttpSession);
    // SAFETY: the session's boxed object, alive while the session is
    Some(unsafe { &*p })
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

fn comp_complete(c: usize, op: usize) {
    log(format!("complete:{}:{}", name(c), name(op)));
    let conn = with(|w| w.objs[c].conn);
    if conn != 0 {
        conn_obj(conn).op_callback(ptr(op));
    }
    let add = with(|w| std::mem::take(&mut w.objs[c].add));
    if add != 0 {
        let g = create('g');
        with(|w| w.objs[g].host = HOSTS[add - 1].as_bytes().to_vec());
        add_ref(g);
        let u = create('u');
        assign(move |w| &mut w.objs[g].completion, u);
        if let Some(s) = session() {
            s.add_op(ptr(g));
        }
        release(g);
    }
}

fn op_complete(n: usize, status: c_long) {
    let fresh = with(|w| {
        let o = &mut w.objs[n];
        if o.finished {
            return false;
        }
        o.finished = true;
        o.running = false;
        o.status = status;
        true
    });
    if !fresh {
        return;
    }
    log(format!("done:{}:{}", name(n), status));
    add_ref(n);
    let c = with(|w| w.objs[n].completion);
    if c != 0 {
        add_ref(c);
        comp_complete(c, n);
    }
    unhold(n);
    release(n);
    if c != 0 {
        release(c);
    }
}

fn set_completion(op: usize, c: usize) {
    log(format!("setcomp:{}:{}", name(op), name(c)));
    assign(move |w| &mut w.objs[op].completion, c);
}

// ---- the connection's hooks

unsafe extern "C" fn h_nop(_: *mut c_void) {}
unsafe extern "C" fn h_add_ref(p: *mut c_void) {
    add_ref(id(p))
}
unsafe extern "C" fn h_release(p: *mut c_void) {
    release(id(p))
}
unsafe extern "C" fn h_status(p: *mut c_void) -> c_long {
    with(|w| w.objs[id(p)].status)
}
unsafe extern "C" fn h_kill(p: *mut c_void) {
    let n = id(p);
    log(format!("kill:{}", name(n)));
    if with(|w| w.objs[n].running) {
        op_complete(n, WS_KILLED);
    }
}
unsafe extern "C" fn h_force(p: *mut c_void, s: c_long) {
    let n = id(p);
    log(format!("force:{}:{}", name(n), s));
    op_complete(n, s);
}
unsafe extern "C" fn h_completion(p: *mut c_void) -> *mut c_void {
    let c = with(|w| w.objs[id(p)].completion);
    if c == 0 {
        return std::ptr::null_mut();
    }
    add_ref(c);
    ptr(c)
}
unsafe extern "C" fn h_set_completion(p: *mut c_void, c: *mut c_void) {
    set_completion(id(p), id(c))
}
unsafe extern "C" fn h_set_conn_completion(p: *mut c_void, conn: *mut c_void) {
    let q = create('q');
    assign(move |w| &mut w.objs[q].conn, id(conn));
    set_completion(id(p), q);
}
unsafe extern "C" fn h_prepare(p: *mut c_void, s: *mut c_void) {
    let n = id(p);
    log(format!("close:{}:0", name(n)));
    log(format!("range:{}:0", name(n)));
    log(format!("setsock:{}:{}", name(n), name(id(s))));
    assign(move |w| &mut w.objs[n].socket, id(s));
}
unsafe extern "C" fn h_run_async(p: *mut c_void) {
    let n = id(p);
    let (s, r) = with(|w| (w.objs[n].only_send as u8, w.objs[n].only_recv as u8));
    log(format!("runasync:{}:{}:{s}{r}", name(n), 1000 + n));
    with(|w| {
        let o = &mut w.objs[n];
        o.running = true;
        o.finished = false;
        o.status = WS_PENDING;
    });
    hold(n);
}
unsafe extern "C" fn h_socket(p: *mut c_void) -> *mut c_void {
    let s = with(|w| w.objs[id(p)].socket);
    if s == 0 {
        return std::ptr::null_mut();
    }
    add_ref(s);
    ptr(s)
}
unsafe extern "C" fn h_set_mode(p: *mut c_void, m: c_int) {
    with(|w| {
        let o = &mut w.objs[id(p)];
        o.only_send = m == 0;
        o.only_recv = m == 2;
    });
}
unsafe extern "C" fn h_complete(c: *mut c_void, p: *mut c_void) {
    comp_complete(id(c), id(p))
}
unsafe extern "C" fn h_socket_kill(s: *mut c_void) {
    log(format!("kill:{}", name(id(s))));
}
unsafe extern "C" fn h_now(_: *mut c_void) -> i64 {
    with(|w| w.now)
}

fn new_conn() -> usize {
    let n = create('n');
    let hooks = EeWonHttpConnHooks {
        context: ptr(n),
        lock: h_nop,
        unlock: h_nop,
        add_ref: h_add_ref,
        release: h_release,
        op_status: h_status,
        op_kill: h_kill,
        op_force_finish: h_force,
        op_completion: h_completion,
        op_set_completion: h_set_completion,
        op_set_conn_completion: h_set_conn_completion,
        op_prepare: h_prepare,
        op_run_async: h_run_async,
        op_socket: h_socket,
        op_set_mode: h_set_mode,
        complete: h_complete,
        socket_kill: h_socket_kill,
        now: h_now,
    };
    let obj = Box::new(EeWonHttpConn::new(hooks));
    with(|w| w.objs[n].conn_obj = Some(obj));
    n
}

// ---- the session's hooks

unsafe extern "C" fn s_host(p: *mut c_void, sink: EeWonByteSink, sc: *mut c_void) {
    let h = with(|w| w.objs[id(p)].host.clone());
    // SAFETY: the session's sink
    unsafe { sink(sc, h.as_ptr(), h.len()) }
}
unsafe extern "C" fn s_new_conn(_: *mut c_void) -> *mut c_void {
    let n = new_conn();
    add_ref(n);
    ptr(n)
}
unsafe extern "C" fn s_queue(c: *mut c_void, p: *mut c_void) {
    conn_obj(id(c)).queue_op(p)
}
unsafe extern "C" fn s_kill(c: *mut c_void) {
    conn_obj(id(c)).kill()
}
unsafe extern "C" fn s_num(c: *mut c_void) -> c_int {
    conn_obj(id(c)).num_ops()
}

fn new_session() -> usize {
    let n = create('e');
    let hooks = EeWonHttpSessionHooks {
        context: ptr(n),
        lock: h_nop,
        unlock: h_nop,
        add_ref: h_add_ref,
        release: h_release,
        op_host: s_host,
        new_conn: s_new_conn,
        conn_queue_op: s_queue,
        conn_kill: s_kill,
        conn_num_ops: s_num,
    };
    let obj = Box::new(EeWonHttpSession::new(hooks));
    with(|w| w.objs[n].sess_obj = Some(obj));
    n
}

fn held(filter: impl Fn(&Fake) -> bool, n: i64) -> Option<usize> {
    with(|w| {
        let v: Vec<usize> = w.thread.iter().copied().filter(|&x| filter(&w.objs[x])).collect();
        if v.is_empty() {
            None
        } else {
            Some(v[(n % v.len() as i64) as usize])
        }
    })
}

fn step(cmd: char, a: [i64; 3]) {
    let s = session().unwrap();
    match cmd {
        'A' => {
            let g = create('g');
            with(|w| {
                w.objs[g].host = HOSTS[a[0] as usize].as_bytes().to_vec();
                w.objs[g].status = a[1] as c_long;
            });
            add_ref(g);
            let u = create('u');
            with(|w| w.objs[u].add = a[2] as usize);
            assign(move |w| &mut w.objs[g].completion, u);
            s.add_op(ptr(g));
            release(g);
        }
        'C' => {
            if let Some(op) = held(|_| true, a[0]) {
                if a[2] == 0 {
                    assign(move |w| &mut w.objs[op].socket, 0);
                } else if a[2] == 2 {
                    let sk = create('s');
                    assign(move |w| &mut w.objs[op].socket, sk);
                }
                op_complete(op, a[1] as c_long);
            }
        }
        'S' | 'Z' => {
            if let Some(op) = held(|o| o.only_send, a[0]) {
                let sk = if cmd == 'S' { create('s') } else { 0 };
                assign(move |w| &mut w.objs[op].socket, sk);
                op_complete(op, WS_HTTP_SEND_SUCCESS);
            }
        }
        'K' => s.kill_all(),
        'N' => log(format!("numops:{}", s.num_ops())),
        'T' => with(|w| w.now = a[0]),
        _ => panic!("step {cmd}"),
    }
}

fn take_trace() -> String {
    let t = with(|w| std::mem::take(&mut w.trace));
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

fn run_script(steps: &[(char, [i64; 3])]) -> Vec<String> {
    with(|w| {
        *w = World::default();
        w.now = 1000;
    });
    let e = new_session();
    add_ref(e);
    with(|w| w.session = e);
    take_trace();
    let mut out = Vec::new();
    for &(c, a) in steps {
        step(c, a);
        out.push(take_trace());
    }
    session().unwrap().kill_all();
    with(|w| w.session = 0);
    release(e);
    while let Some(n) = with(|w| w.thread.pop()) {
        release(n);
    }
    out.push(take_trace());
    with(|w| assert!(w.objs.iter().skip(1).all(|o| o.refs <= 0), "a leak"));
    out
}

#[test]
fn golden() {
    let mut scripts = 0;
    for line in include_str!("golden_httpengine.txt").lines() {
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
            steps.push((c, [f[1].parse().unwrap(), f[2].parse().unwrap(), f[3].parse().unwrap()]));
        }
        let got = run_script(&steps);
        assert_eq!(got.len(), want.len());
        for (i, (g, w)) in got.iter().zip(&want).enumerate() {
            assert_eq!(g, w, "script {scripts} step {i}: {:?}", steps.get(i));
        }
        scripts += 1;
    }
    assert_eq!(scripts, 100);
}

#[test]
fn hosts_share_a_connection_case_insensitively() {
    let got = run_script(&[('A', [0, 0, 0]), ('A', [1, 0, 0]), ('A', [2, 0, 0]), ('N', [0, 0, 0])]);
    // the first op runs on a new connection; "A.COM" queues behind it on the same one; "b.org" gets another
    assert!(got[0].starts_with("new:g2 new:u3 new:n4 new:q5 setcomp:g2:q5"));
    assert_eq!(got[1], "new:g6 new:u7");
    assert!(got[2].starts_with("new:g8 new:u9 new:n10 "));
    assert_eq!(got[3], "numops:3");
}

#[test]
fn a_sent_op_waits_for_its_reply_on_the_receive_queue() {
    let got =
        run_script(&[('T', [4242, 0, 0]), ('A', [0, 0, 0]), ('S', [0, 0, 0]), ('N', [0, 0, 0]), ('C', [0, 0, 1])]);
    let got = &got[1..];
    // sent with a socket: the op moves to the receive queue (receive only) and runs again on that socket
    assert!(got[1].contains("runasync:g2:1002:01"), "{}", got[1]);
    assert_eq!(got[2], "numops:1");
    // the reply: the user's completion, restored
    assert!(got[3].contains("setcomp:g2:u3 complete:u3:g2"), "{}", got[3]);
}

#[test]
fn a_completed_reply_records_the_last_use() {
    with(|w| {
        *w = World::default();
        w.now = 4242;
    });
    let n = new_conn();
    add_ref(n);
    let c = conn_obj(n);
    let g = create('g');
    add_ref(g);
    c.queue_op(ptr(g));
    assert_eq!(c.last_use(), 0);
    op_complete(g, WS_SUCCESS);
    assert_eq!(c.last_use(), 4242);
    assert_eq!(c.num_ops(), 0);
    release(g);
    release(n);
}
