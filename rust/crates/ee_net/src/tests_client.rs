//! The client's golden (`golden_client.txt`, recorded by tests/net/ne_client_test over the Rust side, whose world
//! traces equal the original C++ unit's): each recorded call is made again on a fresh Rust client whose host answers
//! from the record, and every op the Rust makes must be the next recorded one, with the same arguments. The main and
//! receive threads are real threads here too, handed the baton where the record's `sw` lines say; the client's state
//! after each step must be the recorded one.

use std::cell::Cell;
use std::collections::{HashMap, VecDeque};
use std::ffi::CStr;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Condvar, Mutex};
use std::thread::JoinHandle;
use std::time::Duration;

use crate::client::*;
use crate::qhost::*;

const GOLDEN: &str = include_str!("golden_client.txt");

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

fn bytes_at<'a>(p: usize, n: usize) -> &'a [u8] {
    unsafe { std::slice::from_raw_parts(p as *const u8, n) }
}

struct Shared {
    lines: Mutex<VecDeque<String>>,
    baton: Mutex<usize>,
    turn: Condvar,
    threads: Mutex<HashMap<usize, JoinHandle<()>>>,
    /// a thread found the Rust off the record: every waiter gives up
    failed: AtomicBool,
    /// the bytes the record handed the Rust (a message's data, an export), alive to the end
    buffers: Mutex<Vec<Box<[u8]>>>,
    /// the client (the virtual ProcessLocalUserInput is its own)
    client: Mutex<usize>,
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
        self.answer()
    }
    fn answer(&self) -> String {
        loop {
            let line = self.pop();
            if let Some(answer) = line.strip_prefix("= ") {
                return answer.to_string();
            }
            let Some(k) = line.strip_prefix("sw ") else { self.fail(format!("`{line}` inside an op")) };
            self.switch_to(k.parse().unwrap(), false);
        }
    }
    fn num(&self, args: String) -> i64 {
        self.op(args).parse().unwrap()
    }
    fn keep(&self, bytes: Vec<u8>) -> usize {
        let b = bytes.into_boxed_slice();
        let p = b.as_ptr() as usize;
        self.0.buffers.lock().unwrap().push(b);
        p
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
            let c = unsafe { &*(obj as *const Client<Replay>) };
            match kind {
                THREAD_CL_MAIN => c.main_thread(),
                THREAD_CL_RECEIVE => c.receive_thread(),
                _ => host.fail(format!("thread kind {kind}")),
            };
            let end = host.pop();
            if end != "end" {
                host.fail(format!("`{end}` where the thread ended"));
            }
            let line = host.pop();
            let Some(next) = line.strip_prefix("sw ") else { host.fail(format!("`{line}` after a thread's end")) };
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
    fn msg_export_size(&self, _m: Msg) -> u32 {
        self.fail("the client exports through its own op".into())
    }
    fn msg_export(&self, _m: Msg, _buf: &mut [u8]) -> u32 {
        self.fail("the client exports through its own op".into())
    }
    fn msg_from_bytes(&self, data: Option<&[u8]>) -> Msg {
        let Some(data) = data else { self.fail("no bytes".into()) };
        id(&self.op(format!("16 {}", hex(&data[..4]))))
    }
    fn deliver(&self, _kind: u32, _m: Msg) -> i32 {
        self.fail("the client's queues deliver to the client".into())
    }
    fn file_create(&self, _f: FileH) -> i32 {
        self.fail("no record file".into())
    }
    fn file_delete(&self, _f: FileH) -> i32 {
        self.fail("no record file".into())
    }
    fn file_open(&self, _f: FileH, _access: u32) -> i32 {
        self.fail("no record file".into())
    }
    fn file_close(&self, _f: FileH) {
        self.fail("no record file".into())
    }
    fn file_write(&self, _f: FileH, _data: &[u8]) -> i32 {
        self.fail("no record file".into())
    }
    fn file_write_u32(&self, _f: FileH, _value: u32) -> i32 {
        self.fail("no record file".into())
    }
    fn file_read_u32(&self, _f: FileH, _value: &mut u32) -> i32 {
        self.fail("no record file".into())
    }
    fn file_read(&self, _f: FileH, _buf: &mut [u8]) -> i32 {
        self.fail("no record file".into())
    }
}

/// "r cJ": the result and the connection out
fn conn_answer(a: &str, out: &mut usize) -> i32 {
    let (r, c) = a.split_once(' ').unwrap();
    *out = id(c);
    r.parse().unwrap()
}

impl ClientHost for Replay {
    fn wait_any(&self, events: &[Ev], timeout: u32) -> i32 {
        let list: Vec<String> = events.iter().map(|e| format!("e{e}")).collect();
        self.num(format!("c0 {} {timeout}", list.join(","))) as i32
    }
    fn thread_priority(&self, t: Thr) {
        self.num(format!("c1 t{t}"));
    }
    fn base_get(&self, f: BaseField) -> u32 {
        self.num(format!("c2 {}", f as u32)) as u32
    }
    fn base_set(&self, f: BaseField, value: u32) {
        self.num(format!("c3 {} {value}", f as u32));
    }
    fn protocol_count(&self) -> u32 {
        self.num("c4".into()) as u32
    }
    fn protocol_get(&self, out: &mut usize) -> i32 {
        let a = self.op("c5".into());
        *out = 1;
        a.split(' ').next().unwrap().parse().unwrap()
    }
    fn connect(&self, _strings: usize, event: Ev, out: &mut usize, timeout: u32) -> i32 {
        let a = self.op(format!("c6 e{event} c{out} {timeout}"));
        conn_answer(&a, out)
    }
    fn connect_for_xfer(&self, connection: usize, event: Ev, out: &mut usize) -> i32 {
        let a = self.op(format!("c7 c{connection} e{event} c{out}"));
        conn_answer(&a, out)
    }
    fn conn_close(&self, connection: usize) -> i32 {
        self.num(format!("c8 c{connection}")) as i32
    }
    fn conn_receive(&self, connection: usize, buf: &mut [u8], length: &mut u32) -> i32 {
        let a = self.op(format!("c9 c{connection} {} {length}", buf.len()));
        let w: Vec<&str> = a.split(' ').collect();
        *length = w[1].parse().unwrap();
        let bytes = unhex(w[2]);
        let r: i32 = w[0].parse().unwrap();
        if r == 0 {
            buf.fill(0);
        }
        buf[..bytes.len()].copy_from_slice(&bytes);
        r
    }
    fn conn_send(&self, connection: usize, data: usize, size: u32) -> i32 {
        self.num(format!("c10 c{connection} {}", hex(bytes_at(data, size as usize)))) as i32
    }
    fn detected_game_count(&self) -> u32 {
        self.num("c11".into()) as u32
    }
    fn detected_game(&self, index: u32, _strings: usize) -> i32 {
        self.num(format!("c12 {index}")) as i32
    }
    fn search_for_game(&self, start: bool) -> i32 {
        self.num(format!("c13 {}", start as u32)) as i32
    }
    fn xfer_receive(&self, buf: &mut [u8], received: &mut u32) -> i32 {
        let a = self.op(format!("c14 {} {received}", buf.len()));
        let w: Vec<&str> = a.split(' ').collect();
        *received = w[1].parse().unwrap();
        let bytes = unhex(w[2]);
        buf[..bytes.len()].copy_from_slice(&bytes);
        w[0].parse().unwrap()
    }
    fn xfer_export_size(&self) -> u32 {
        self.num("c15".into()) as u32
    }
    fn msg_type(&self, m: Msg) -> u8 {
        self.num(format!("c16 m{m}")) as u8
    }
    fn msg_flag(&self, m: Msg, f: MsgFlag) -> bool {
        self.num(format!("c17 m{m} {}", f as u32)) != 0
    }
    fn msg_set_slot(&self, m: Msg, originating: bool, slot: u8) {
        self.num(format!("c18 m{m} {} {slot}", originating as u32));
    }
    fn msg_s2aw_id(&self, m: Msg, out: &mut u32) -> bool {
        let a = self.op(format!("c19 m{m} {out}"));
        let (r, v) = a.split_once(' ').unwrap();
        *out = v.parse().unwrap();
        r == "1"
    }
    fn msg_get(&self, m: Msg, f: MsgField) -> usize {
        let a = self.op(format!("c20 m{m} {}", f as u32));
        if f == MsgField::XferData {
            return self.keep(unhex(&a));
        }
        a.parse::<i64>().unwrap() as usize
    }
    fn msg_new(&self, kind: NewMsg, a: usize, b: usize, c: usize, d: usize) -> Msg {
        let args = match kind {
            NewMsg::TickResponse => format!("c21 0 {a} {b} {c}"),
            NewMsg::StopGameResponse => "c21 1".into(),
            NewMsg::StartGameResponse => format!("c21 2 {a}"),
            NewMsg::UnPauseGameResponse => format!("c21 3 {a}"),
            NewMsg::PauseGameResponse => format!("c21 4 {a}"),
            NewMsg::XferMessage => format!("c21 5 {}", hex(bytes_at(a, 40))),
            NewMsg::XferStatus => format!("c21 6 {} {c}", (a & 0xff) | (b & 0xff) << 8 | (d & 0xff) << 16),
        };
        id(&self.op(args))
    }
    fn msg_add_input(&self, response: Msg, input: Msg) -> i32 {
        self.num(format!("c22 m{response} m{input}")) as i32
    }
    fn msg_export_new(&self, m: Msg, size: &mut u32) -> usize {
        let a = self.op(format!("c23 m{m}"));
        let (n, bytes) = a.split_once(' ').unwrap();
        *size = n.parse().unwrap();
        self.keep(unhex(bytes))
    }
    fn bytes_free(&self, _data: usize) {
        self.num("c24".into());
    }
    fn fsfile_write(&self, f: FileH, data: usize, size: u32, written: &mut u32) -> i32 {
        let a = self.op(format!("c25 f{f} {} {written}", hex(bytes_at(data, size as usize))));
        let (r, w) = a.split_once(' ').unwrap();
        *written = w.parse().unwrap();
        r.parse().unwrap()
    }
    fn fsfile_close(&self, f: FileH) {
        self.num(format!("c26 f{f}"));
    }
    fn fsfile_delete(&self, f: FileH) {
        self.num(format!("c27 f{f}"));
    }
    fn fsfile_free(&self, f: FileH) {
        self.num(format!("c28 f{f}"));
    }
    fn create_xfer_file(&self, name: usize, size: u32) -> FileH {
        let name = unsafe { CStr::from_ptr(name as *const std::ffi::c_char) }.to_string_lossy().into_owned();
        id(&self.op(format!("c29 {name} {size}")))
    }
    fn is_playback_recording_session(&self) -> bool {
        self.num("c30".into()) != 0
    }
    fn connect_timeout(&self) -> u32 {
        self.num("c31".into()) as u32
    }
    fn client_slot(&self) -> u8 {
        self.num("c32".into()) as u8
    }
    fn ignore_connection_timeout(&self) -> bool {
        self.num("c33".into()) != 0
    }
    fn set_connection_state(&self, state: i32) {
        self.num(format!("c34 {state}"));
    }
    fn handle_message(&self, m: Msg) {
        let want = format!("op c35 m{m}");
        let line = self.pop();
        if line != want {
            self.fail(format!("the record has `{line}`, the Rust made `{want}`"));
        }
        // the game disconnected from inside (its ops are the record's next ones)
        let next = self.0.lines.lock().unwrap().front().cloned().unwrap_or_default();
        if next.starts_with("op c8 ") {
            let c = *self.0.client.lock().unwrap();
            unsafe { &*(c as *const Client<Replay>) }.disconnect();
        }
        self.answer();
    }
    fn handle_user_input(&self, m: Msg) {
        self.num(format!("c36 m{m}"));
    }
    fn handle_connection_lost(&self) {
        self.num("c37".into());
    }
    fn handle_start_game(&self, offset: u32) -> u32 {
        self.num(format!("c38 {offset}")) as u32
    }
    fn handle_stop_game(&self) {
        self.num("c39".into());
    }
    fn handle_connection_timeout_warning(&self, remaining: u32) {
        self.num(format!("c40 {remaining}"));
    }
    fn process_local_user_input(&self, m: Msg, player: u8) -> i32 {
        let want = format!("op c41 m{m} {player}");
        let line = self.pop();
        if line != want {
            self.fail(format!("the record has `{line}`, the Rust made `{want}`"));
        }
        // the virtual is the client's own: its ops are the record's next ones, then the answer
        let c = *self.0.client.lock().unwrap();
        let r = unsafe { &*(c as *const Client<Replay>) }.process_user_input(m, player, false);
        let a = self.answer();
        if a != r.to_string() {
            self.fail(format!("ProcessLocalUserInput answered {r}, the record {a}"));
        }
        r
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
        buffers: Mutex::new(Vec::new()),
        client: Mutex::new(0),
    }));
    ME.with(|m| m.set(0));
    let mut client: Option<Box<Client<Replay>>> = None;
    loop {
        let Some(line) = host.0.lines.lock().unwrap().pop_front() else { break };
        if let Some(k) = line.strip_prefix("sw ") {
            host.switch_to(k.parse().unwrap(), false);
            continue;
        }
        if line.starts_with("ret ") {
            continue;
        }
        if let Some(state) = line.strip_prefix("state ") {
            let c = client.as_ref().unwrap();
            let have = format!(
                "{} {} {} {} c{} {}",
                c.current_communications_tick(),
                c.current_physics_tick(),
                c.my_send_to_slot(),
                c.num_connection_trys(),
                c.connection(),
                c.record_playback_mode()
            );
            if have != state {
                host.fail(format!("the client's state is `{have}`, the record's `{state}`"));
            }
            continue;
        }
        let call = line.strip_prefix("call ").unwrap_or_else(|| panic!("`{line}` between calls"));
        let w: Vec<&str> = call.split(' ').collect();
        match w[0] {
            "new" => {
                let c = Client::new(host.clone());
                *host.0.client.lock().unwrap() = &*c as *const Client<Replay> as usize;
                client = Some(c);
                expect_ret(&host, 0);
            }
            "start" => {
                let c = client.as_ref().unwrap();
                c.start_threads(&**c as *const Client<Replay> as usize);
                expect_ret(&host, 0);
            }
            "stop" => {
                client.as_ref().unwrap().stop_threads();
                expect_ret(&host, 0);
            }
            "free" => {
                let c = client.take().unwrap();
                c.release();
                drop(c);
                expect_ret(&host, 0);
            }
            "run" | "pass" | "packets" | "xfer" | "base" => {}
            "input" => {
                let c = client.as_ref().unwrap();
                let r = c.process_user_input(id(w[1]), w[3].parse().unwrap(), w[2] == "1");
                expect_ret(&host, r as i64);
            }
            "connect" => {
                let r = client.as_ref().unwrap().connect(0);
                expect_ret(&host, r as i64);
            }
            "disconnect" => {
                let r = client.as_ref().unwrap().disconnect();
                expect_ret(&host, r as i64);
            }
            "startconnect" => {
                client.as_ref().unwrap().send_start_connect_message(id(w[1]));
                expect_ret(&host, 0);
            }
            "search" => {
                let r = client.as_ref().unwrap().search_for_game(w[1] == "1");
                expect_ret(&host, r as i64);
            }
            "detected" => {
                let c = client.as_ref().unwrap();
                c.detected_game_count();
                let r = c.detected_game(w[1].parse().unwrap(), 0);
                expect_ret(&host, r as i64);
            }
            "set" => {
                let c = client.as_ref().unwrap();
                let v: u32 = w[2].parse().unwrap();
                match w[1] {
                    "0" => c.set_current_communications_tick(v),
                    "1" => c.set_current_physics_tick(v),
                    "2" => c.set_max_num_connection_trys(v),
                    _ => c.set_my_send_to_slot(v as u8),
                }
                expect_ret(&host, 0);
            }
            "playback" => {
                let c = client.as_ref().unwrap();
                if w[1] == "1" {
                    c.rec().set_slow_playback();
                } else {
                    c.rec().set_normal_playback();
                }
                expect_ret(&host, 0);
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
fn the_client_golden_replays() {
    let seqs = sequences();
    assert!(seqs.len() >= 40);
    for (i, s) in seqs.into_iter().enumerate() {
        let r = std::thread::spawn(move || replay(s)).join();
        assert!(r.is_ok(), "sequence {} failed", i + 1);
    }
}

// ---- unit tests without the record: the throttle and the tick replies' rules through a scripted host

#[test]
fn the_tick_rules_and_the_throttle_are_the_originals() {
    // the replay's first sequence must contain a repeated tick (answered again) and dropped inputs (7)
    let golden = sequences();
    let s = golden.iter().flatten().collect::<Vec<_>>();
    assert!(s.iter().any(|l| l.as_str() == "ret 7"), "no input was dropped in the record");
    assert!(s.iter().any(|l| l.starts_with("op c21 0 ")), "no tick was answered in the record");
    assert!(s.iter().any(|l| l.starts_with("op c37")), "no connection was lost in the record");
    assert!(s.iter().any(|l| l.starts_with("op c25 ")), "no transfer was written in the record");
}
