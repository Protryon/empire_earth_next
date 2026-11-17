//! The queues' golden (`golden_queues.txt`, recorded by tests/net/ne_queues_test over the Rust side, whose world
//! traces equal the original C++ units'): each recorded call is made again on fresh Rust objects whose host answers
//! from the record, and every op the Rust makes must be the next recorded one, with the same arguments. The lag and
//! playback threads are real threads here too, handed the baton where the record's `sw` lines say.

use std::cell::Cell;
use std::collections::{HashMap, VecDeque};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Condvar, Mutex};
use std::thread::JoinHandle;
use std::time::Duration;

use crate::net::lagloss::LagLoss;
use crate::net::qhost::*;
use crate::net::recplay::RecPlay;

const GOLDEN: &str = include_str!("golden_queues.txt");

thread_local! {
    static ME: Cell<usize> = const { Cell::new(0) };
}

fn hex(data: &[u8]) -> String {
    if data.is_empty() {
        return "-".into();
    }
    data.iter().map(|b| format!("{b:02x}")).collect()
}

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

fn id(tok: &str) -> usize {
    tok[1..].parse().unwrap()
}

struct Shared {
    lines: Mutex<VecDeque<String>>,
    baton: Mutex<usize>,
    turn: Condvar,
    threads: Mutex<HashMap<usize, JoinHandle<()>>>,
    /// a thread found the Rust off the record: every waiter gives up
    failed: AtomicBool,
}

#[derive(Clone)]
struct Replay(Arc<Shared>);

impl Replay {
    fn pop(&self) -> String {
        let line = self.0.lines.lock().unwrap().pop_front();
        line.unwrap_or_else(|| self.fail("the record ended".into()))
    }
    fn fail(&self, why: String) -> ! {
        self.0.failed.store(true, Ordering::SeqCst);
        self.0.turn.notify_all();
        panic!("{why}");
    }
    /// Until the baton is `me`'s (a failure elsewhere, or a minute without it, fails this thread too)
    fn wait_for(&self, me: usize) {
        let mut b = self.0.baton.lock().unwrap();
        let mut waited = 0;
        while *b != me {
            if self.0.failed.load(Ordering::SeqCst) || waited > 600 {
                drop(b);
                self.fail(format!("thread {me} never got the baton back"));
            }
            b = self.0.turn.wait_timeout(b, Duration::from_millis(100)).unwrap().0;
            waited += 1;
        }
    }
    /// The baton to thread `k`; back when it is this thread's again (never for a thread whose body ended).
    fn switch_to(&self, k: usize, done: bool) {
        let me = ME.with(|m| m.get());
        {
            let mut b = self.0.baton.lock().unwrap();
            *b = k;
            self.0.turn.notify_all();
        }
        if !done {
            self.wait_for(me);
        }
    }
    /// The next recorded op, which must be `args`: its answer (the switches before it followed).
    fn op(&self, args: String) -> String {
        let want = format!("op {args}");
        let line = self.pop();
        if line != want {
            self.fail(format!("the record has `{line}`, the Rust made `{want}`"));
        }
        loop {
            let line = self.pop();
            if let Some(answer) = line.strip_prefix("= ") {
                return answer.to_string();
            }
            let k = line.strip_prefix("sw ").unwrap_or_else(|| panic!("`{line}` inside an op"));
            self.switch_to(k.parse().unwrap(), false);
        }
    }
    fn num(&self, args: String) -> i64 {
        self.op(args).parse().unwrap()
    }
}

impl QHost for Replay {
    fn time(&self) -> u32 {
        self.num("0".into()) as u32
    }
    fn verify(&self, unit: u32, line: u32) {
        self.num(format!("1 {unit} {line}"));
    }
    fn fpu_start(&self) {
        self.num("2".into());
    }
    fn fpu_check(&self) {
        self.num("3".into());
    }
    fn event_new(&self) -> Ev {
        id(&self.op("4".into()))
    }
    fn event_free(&self, e: Ev) {
        self.num(format!("5 e{e}"));
    }
    fn event_set(&self, e: Ev) {
        self.num(format!("6 e{e}"));
    }
    fn event_clear(&self, e: Ev) {
        self.num(format!("7 e{e}"));
    }
    fn event_wait(&self, e: Ev, timeout: u32) -> i32 {
        self.num(format!("8 e{e} {timeout}")) as i32
    }
    fn thread_new(&self, kind: u32, obj: usize, out: &mut Thr) -> i32 {
        let answer = self.op(format!("9 {kind}"));
        let (t, r) = answer.split_once(' ').unwrap();
        let k = id(t);
        *out = k;
        let host = self.clone();
        let handle = std::thread::spawn(move || {
            ME.with(|m| m.set(k));
            host.wait_for(k);
            if kind == THREAD_LAG {
                unsafe { &*(obj as *const LagLoss<Replay>) }.lag_thread();
            } else {
                unsafe { &*(obj as *const RecPlay<Replay>) }.playback_thread();
            }
            let end = host.pop();
            if end != "end" {
                host.fail(format!("`{end}` where the thread ended"));
            }
            let line = host.pop();
            let next = line.strip_prefix("sw ").unwrap_or_else(|| panic!("`{line}` after a thread's end"));
            host.switch_to(next.parse().unwrap(), true);
        });
        self.0.threads.lock().unwrap().insert(k, handle);
        r.parse().unwrap()
    }
    fn thread_name(&self, t: Thr, name: &str) {
        self.num(format!("10 t{t} {name}"));
    }
    fn thread_free(&self, t: Thr) {
        self.num(format!("11 t{t}"));
        let handle = self.0.threads.lock().unwrap().remove(&t);
        if let Some(h) = handle {
            h.join().unwrap();
        }
    }
    fn msg_reference(&self, m: Msg) {
        self.num(format!("12 m{m}"));
    }
    fn msg_dereference(&self, m: Msg) {
        self.num(format!("13 m{m}"));
    }
    fn msg_export_size(&self, m: Msg) -> u32 {
        self.num(format!("14 m{m}")) as u32
    }
    fn msg_export(&self, m: Msg, buf: &mut [u8]) -> u32 {
        let answer = self.op(format!("15 m{m} {}", buf.len()));
        let (size, bytes) = answer.split_once(' ').unwrap();
        let bytes = unhex(bytes);
        buf[..bytes.len()].copy_from_slice(&bytes);
        size.parse().unwrap()
    }
    fn msg_from_bytes(&self, data: Option<&[u8]>) -> Msg {
        id(&self.op(format!("16 {}", data.map_or("null".to_string(), hex))))
    }
    fn deliver(&self, kind: u32, m: Msg) -> i32 {
        self.num(format!("17 {kind} m{m}")) as i32
    }
    fn file_create(&self, f: FileH) -> i32 {
        self.num(format!("18 f{f}")) as i32
    }
    fn file_delete(&self, f: FileH) -> i32 {
        self.num(format!("19 f{f}")) as i32
    }
    fn file_open(&self, f: FileH, access: u32) -> i32 {
        self.num(format!("20 f{f} {access}")) as i32
    }
    fn file_close(&self, f: FileH) {
        self.num(format!("21 f{f}"));
    }
    fn file_write(&self, f: FileH, data: &[u8]) -> i32 {
        self.num(format!("22 f{f} {}", hex(data))) as i32
    }
    fn file_write_u32(&self, f: FileH, value: u32) -> i32 {
        self.num(format!("23 f{f} {value}")) as i32
    }
    fn file_read_u32(&self, f: FileH, value: &mut u32) -> i32 {
        let answer = self.op(format!("24 f{f} {value}"));
        let (r, v) = answer.split_once(' ').unwrap();
        *value = v.parse().unwrap();
        r.parse().unwrap()
    }
    fn file_read(&self, f: FileH, buf: &mut [u8]) -> i32 {
        let answer = self.op(format!("25 f{f} {}", buf.len()));
        let (r, bytes) = answer.split_once(' ').unwrap();
        let bytes = unhex(bytes);
        buf[..bytes.len()].copy_from_slice(&bytes);
        r.parse().unwrap()
    }
}

fn expect_ret(host: &Replay, r: i64) {
    let line = host.pop();
    if line != format!("ret {r}") {
        host.fail(format!("the record has `{line}`, the Rust returned {r}"));
    }
}

/// One recorded sequence made again: its calls, in order, on the main replay thread.
fn replay(lines: Vec<String>) {
    let host = Replay(Arc::new(Shared {
        lines: Mutex::new(lines.into()),
        baton: Mutex::new(0),
        turn: Condvar::new(),
        threads: Mutex::new(HashMap::new()),
        failed: AtomicBool::new(false),
    }));
    ME.with(|m| m.set(0));
    let mut lag: Option<Box<LagLoss<Replay>>> = None;
    let mut rec: Option<Box<RecPlay<Replay>>> = None;
    loop {
        let Some(line) = host.0.lines.lock().unwrap().pop_front() else { break };
        if let Some(k) = line.strip_prefix("sw ") {
            host.switch_to(k.parse().unwrap(), false);
            continue;
        }
        if line.starts_with("ret ") {
            continue;
        }
        let call = line.strip_prefix("call ").unwrap_or_else(|| panic!("`{line}` between calls"));
        let w: Vec<&str> = call.split(' ').collect();
        let msg = |t: &str| id(t);
        match w[..] {
            ["new"] => {
                let l = Box::new(LagLoss::new(host.clone()));
                l.disable_lag(false);
                l.disable_receive_loss();
                l.disable_send_loss();
                let r = Box::new(RecPlay::new(host.clone()));
                r.pass_mode();
                lag = Some(l);
                rec = Some(r);
                expect_ret(&host, 0);
            }
            ["free"] => {
                let r = rec.take().unwrap();
                r.release();
                drop(r);
                let l = lag.take().unwrap();
                l.release();
                drop(l);
                expect_ret(&host, 0);
            }
            ["run"] | ["pass", _] => {}
            ["ll", "process", m] => {
                let r = lag.as_ref().unwrap().process(msg(m));
                expect_ret(&host, r as i64);
            }
            ["ll", "send", m] => {
                let r = lag.as_ref().unwrap().send(msg(m));
                expect_ret(&host, r as i64);
            }
            ["ll", "enablelag", a, b] => {
                let l = lag.as_ref().unwrap();
                l.enable_lag(&**l as *const LagLoss<Replay> as usize, a.parse().unwrap(), b.parse().unwrap());
                expect_ret(&host, 0);
            }
            ["ll", "disablelag", f] => {
                lag.as_ref().unwrap().disable_lag(f == "1");
                expect_ret(&host, 0);
            }
            ["ll", "loss", kind, p] => {
                let l = lag.as_ref().unwrap();
                let p: f32 = p.parse().unwrap();
                match kind {
                    "0" => l.enable_receive_loss(p),
                    "1" => l.enable_send_loss(p),
                    "2" => l.disable_receive_loss(),
                    _ => l.disable_send_loss(),
                }
                expect_ret(&host, 0);
            }
            ["rp", "process", m] => {
                rec.as_ref().unwrap().process(msg(m));
                expect_ret(&host, 0);
            }
            ["rp", "pass"] => {
                let r = rec.as_ref().unwrap().pass_mode();
                expect_ret(&host, r as i64);
            }
            ["rp", "record", f] => {
                let r = rec.as_ref().unwrap().record_mode_file(id(f));
                expect_ret(&host, r as i64);
            }
            ["rp", "recordnet"] => {
                let r = rec.as_ref().unwrap().record_mode_network();
                expect_ret(&host, r as i64);
            }
            ["rp", "play", f] => {
                let r = rec.as_ref().unwrap();
                let v = r.playback_mode_file(&**r as *const RecPlay<Replay> as usize, id(f));
                expect_ret(&host, v as i64);
            }
            ["rp", "playnet"] => {
                let r = rec.as_ref().unwrap().playback_mode_network();
                expect_ret(&host, r as i64);
            }
            ["rp", "toggle", kind] => {
                let r = rec.as_ref().unwrap();
                let v = match kind {
                    "0" => r.playback_complete() as i64,
                    "1" => {
                        r.set_slow_playback();
                        0
                    }
                    _ => {
                        r.set_normal_playback();
                        0
                    }
                };
                expect_ret(&host, v);
            }
            ["rp", "mode"] => {
                let v = rec.as_ref().unwrap().mode();
                expect_ret(&host, v as i64);
            }
            _ => panic!("unknown call `{call}`"),
        }
    }
    assert!(host.0.threads.lock().unwrap().is_empty(), "a thread was never freed");
    assert!(!host.0.failed.load(Ordering::SeqCst));
}

fn sequences() -> Vec<Vec<String>> {
    let mut out: Vec<Vec<String>> = Vec::new();
    for line in GOLDEN.lines() {
        if line.starts_with("seq ") {
            out.push(Vec::new());
        } else {
            out.last_mut().unwrap().push(line.to_string());
        }
    }
    out
}

#[test]
fn the_golden_replays() {
    let seqs = sequences();
    assert!(seqs.len() >= 50);
    for (i, s) in seqs.into_iter().enumerate() {
        let r = std::thread::spawn(move || replay(s)).join();
        assert!(r.is_ok(), "sequence {} failed", i + 1);
    }
}

// ---- unit tests without the record

#[derive(Default)]
struct Log {
    ops: Mutex<Vec<String>>,
    clock: Mutex<u32>,
}

#[derive(Clone, Default)]
struct Plain(Arc<Log>);

impl Plain {
    fn log(&self, s: String) {
        self.0.ops.lock().unwrap().push(s);
    }
}

impl QHost for Plain {
    fn time(&self) -> u32 {
        *self.0.clock.lock().unwrap()
    }
    fn verify(&self, _unit: u32, line: u32) {
        self.log(format!("verify {line}"));
    }
    fn fpu_start(&self) {}
    fn fpu_check(&self) {}
    fn event_new(&self) -> Ev {
        1
    }
    fn event_free(&self, _e: Ev) {}
    fn event_set(&self, e: Ev) {
        self.log(format!("set {e}"));
    }
    fn event_clear(&self, _e: Ev) {}
    fn event_wait(&self, _e: Ev, _timeout: u32) -> i32 {
        ERROR_OK
    }
    fn thread_new(&self, _kind: u32, _obj: usize, out: &mut Thr) -> i32 {
        *out = 9;
        0
    }
    fn thread_name(&self, _t: Thr, name: &str) {
        self.log(format!("name {name}"));
    }
    fn thread_free(&self, _t: Thr) {}
    fn msg_reference(&self, m: Msg) {
        self.log(format!("ref {m}"));
    }
    fn msg_dereference(&self, m: Msg) {
        self.log(format!("deref {m}"));
    }
    fn msg_export_size(&self, _m: Msg) -> u32 {
        3
    }
    fn msg_export(&self, _m: Msg, buf: &mut [u8]) -> u32 {
        buf.copy_from_slice(&[7, 8, 9]);
        3
    }
    fn msg_from_bytes(&self, _data: Option<&[u8]>) -> Msg {
        0
    }
    fn deliver(&self, kind: u32, m: Msg) -> i32 {
        self.log(format!("deliver {kind} {m}"));
        0
    }
    fn file_create(&self, _f: FileH) -> i32 {
        0
    }
    fn file_delete(&self, _f: FileH) -> i32 {
        0
    }
    fn file_open(&self, _f: FileH, _access: u32) -> i32 {
        0
    }
    fn file_close(&self, _f: FileH) {}
    fn file_write(&self, _f: FileH, data: &[u8]) -> i32 {
        self.log(format!("write {}", hex(data)));
        0
    }
    fn file_write_u32(&self, _f: FileH, value: u32) -> i32 {
        self.log(format!("write32 {value}"));
        0
    }
    fn file_read_u32(&self, _f: FileH, _value: &mut u32) -> i32 {
        1
    }
    fn file_read(&self, _f: FileH, _buf: &mut [u8]) -> i32 {
        1
    }
}

#[test]
fn lagged_messages_sort_after_equal_times() {
    let host = Plain::default();
    let l = LagLoss::new(host.clone());
    l.enable_lag(0, 50, 50);
    *host.0.clock.lock().unwrap() = 100;
    l.process(1);
    *host.0.clock.lock().unwrap() = 90;
    l.process(2);
    *host.0.clock.lock().unwrap() = 100;
    l.process(3);
    assert_eq!(l.lagged(), vec![(140, 2), (150, 1), (150, 3)]);
}

#[test]
fn the_loss_roll_is_the_original_expression() {
    // seed 1: the first draw 0x269ec3 + 0x343fd = 2,745,024 / 2^31 = 0.00127823..; * 99 + 1 = 1.1265..
    let host = Plain::default();
    *host.0.clock.lock().unwrap() = 1;
    let l = LagLoss::new(host.clone());
    l.enable_send_loss(1.13);
    l.send(5);
    l.enable_send_loss(1.12);
    // the second draw is far larger: sent
    l.send(6);
    let ops = host.0.ops.lock().unwrap().clone();
    assert_eq!(ops, vec!["deref 5", "deliver 1 6"]);
}

#[test]
fn a_record_is_time_size_message() {
    let host = Plain::default();
    *host.0.clock.lock().unwrap() = 0x0102_0304;
    let r = RecPlay::new(host.clone());
    r.record_mode_file(4);
    r.process(7);
    let ops = host.0.ops.lock().unwrap().clone();
    assert_eq!(ops, vec!["write32 16909060", "write 0403020103000000070809", "ref 7", "deliver 2 7", "deref 7"]);
    assert_eq!(export_timestamped(&host, 7, 1), vec![1, 0, 0, 0, 3, 0, 0, 0, 7, 8, 9]);
}

#[test]
fn a_null_recorders_calls_are_harmless_and_a_missing_event_is_not_set() {
    let host = Plain::default();
    let r = RecPlay::new(host.clone());
    r.set_slow_playback();
    r.set_normal_playback();
    assert!(!r.playback_complete());
    assert_eq!(host.0.ops.lock().unwrap().clone(), vec!["verify 86"]);
    assert_eq!(crate::net::ffi::ee_net_rp_mode(std::ptr::null()), crate::net::recplay::MODE_PASSING);
    crate::net::ffi::ee_net_rp_process(std::ptr::null(), std::ptr::null_mut());
}
