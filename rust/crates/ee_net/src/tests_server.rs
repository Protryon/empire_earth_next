//! The server's golden (`golden_server.txt`, recorded by tests/net/ne_server_test over the Rust side, whose world
//! traces equal the original C++ unit's): each recorded call is made again on a fresh Rust server whose hosts answer
//! from the record, and every op the Rust makes must be the next recorded one, with the same arguments. The main,
//! send and transfer threads are real threads here too, handed the baton where the record's `sw` lines say; the
//! server's state after each step must be the recorded one. Then the out-of-sync order's edges.

use std::cell::Cell;
use std::collections::{HashMap, VecDeque};
use std::ffi::CString;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Condvar, Mutex};
use std::thread::JoinHandle;
use std::time::Duration;

use crate::ffi::EeNetMmOp;
use crate::qhost::*;
use crate::server::*;

const GOLDEN: &str = include_str!("golden_server.txt");

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
    failed: AtomicBool,
    buffers: Mutex<Vec<Box<[u8]>>>,
    names: Mutex<Vec<CString>>,
    server: Mutex<usize>,
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
    fn expect(&self, args: &str) {
        let want = format!("op {args}");
        let line = self.pop();
        if line != want {
            self.fail(format!("the record has `{line}`, the Rust made `{want}`"));
        }
    }
    fn op(&self, args: String) -> String {
        self.expect(&args);
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
    fn server(&self) -> &Server<Replay> {
        let s = *self.0.server.lock().unwrap();
        unsafe { &*(s as *const Server<Replay>) }
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
            let s = unsafe { &*(obj as *const Server<Replay>) };
            match kind {
                THREAD_SV_MAIN => s.main_thread(),
                THREAD_SV_SEND => s.send_thread(),
                THREAD_SV_XFER => s.xfer_thread(),
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
    fn msg_export_size(&self, m: Msg) -> u32 {
        self.num(format!("14 m{m}")) as u32
    }
    fn msg_export(&self, _m: Msg, _buf: &mut [u8]) -> u32 {
        self.fail("the server's recorder only passes".into())
    }
    fn msg_from_bytes(&self, data: Option<&[u8]>) -> Msg {
        let Some(data) = data else { self.fail("no bytes".into()) };
        id(&self.op(format!("16 {}", hex(&data[..4]))))
    }
    fn deliver(&self, _kind: u32, _m: Msg) -> i32 {
        self.fail("the recorder delivers to the server".into())
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

fn mtok(m: Msg) -> String {
    format!("m{m}")
}

impl ServerHost for Replay {
    fn wait_any(&self, events: &[Ev], timeout: u32) -> i32 {
        let list: Vec<String> = events.iter().map(|e| format!("e{e}")).collect();
        self.num(format!("s0 {} {timeout}", list.join(","))) as i32
    }
    fn thread_priority(&self, t: Thr) {
        self.num(format!("s1 t{t}"));
    }
    fn sleep(&self, ms: u32) {
        self.num(format!("s2 {ms}"));
    }
    fn sim_mode(&self) -> bool {
        self.num("s3".into()) != 0
    }
    fn mm(&self, op: EeNetMmOp, a: usize, b: usize, c: usize, d: usize) -> isize {
        let o = op as u32;
        match op {
            EeNetMmOp::MsgReference
            | EeNetMmOp::MsgDereference
            | EeNetMmOp::MsgType
            | EeNetMmOp::MsgSlot
            | EeNetMmOp::MsgTick
            | EeNetMmOp::ProcessSystemMessage => self.num(format!("x{o} {}", mtok(a))) as isize,
            EeNetMmOp::MsgIsResponse => self.num(format!("x{o} {} {}", mtok(a), mtok(b))) as isize,
            EeNetMmOp::MsgSendTo => self.num(format!("x{o} {a} {}", mtok(b))) as isize,
            EeNetMmOp::NewResponse => id(&self.op(format!("x{o} {a} {}", mtok(b)))) as isize,
            EeNetMmOp::LastTick | EeNetMmOp::CtsProcessed | EeNetMmOp::AvgPhysics => self.num(format!("x{o}")) as isize,
            EeNetMmOp::SetLastTick | EeNetMmOp::ResetClientConnection => self.num(format!("x{o} {a}")) as isize,
            EeNetMmOp::XferStatus => self.num(format!("x{o} {a} {b} {c} {d}")) as isize,
            EeNetMmOp::XferSend => self.num(format!("x{o} {a} {b} {}", hex(bytes_at(c, d)))) as isize,
            EeNetMmOp::FileClone => id(&self.op(format!("x{o} f{a}"))) as isize,
            EeNetMmOp::FileFree
            | EeNetMmOp::FileIsOpen
            | EeNetMmOp::FileOpen
            | EeNetMmOp::FileClose
            | EeNetMmOp::FileLength => self.num(format!("x{o} f{a}")) as isize,
            EeNetMmOp::FileRead => {
                let ans = self.op(format!("x{o} f{a}"));
                let (r, bytes) = ans.split_once(' ').unwrap();
                let bytes = unhex(bytes);
                unsafe { std::ptr::copy_nonoverlapping(bytes.as_ptr(), b as *mut u8, bytes.len()) };
                r.parse().unwrap()
            }
            EeNetMmOp::FileName => {
                let name = CString::new(self.op(format!("x{o} f{a}"))).unwrap();
                let p = name.as_ptr() as isize;
                self.0.names.lock().unwrap().push(name);
                p
            }
            _ => self.fail(format!("mm op {o} is the server's own")),
        }
    }
    fn protocol_count(&self) -> u32 {
        self.num("s6".into()) as u32
    }
    fn protocol_get(&self, out: &mut usize) -> i32 {
        let a = self.op("s7".into());
        *out = 1;
        a.split(' ').next().unwrap().parse().unwrap()
    }
    fn publish(&self, _protocol: usize, events: &[Ev; 3], server: &mut usize, allow: bool) -> i32 {
        let a = self.op(format!("s8 p e{},e{},e{} {}", events[0], events[1], events[2], allow as u32));
        let (r, v) = a.split_once(' ').unwrap();
        if id(v) != 0 {
            *server = id(v);
        }
        r.parse().unwrap()
    }
    fn allow_new_connections(&self, _protocol: usize, server: usize, allow: bool) {
        self.num(format!("s9 p v{server} {}", allow as u32));
    }
    fn accept_for_xfer(&self, _protocol: usize, server: usize) {
        self.num(format!("s10 p v{server}"));
    }
    fn search_for_game(&self, _protocol: usize, start: bool) -> i32 {
        self.num(format!("s11 p {}", start as u32)) as i32
    }
    fn server_send_to(&self, server: usize, slot: u8, data: usize, size: u32) -> i32 {
        self.num(format!("s12 v{server} {slot} {}", hex(bytes_at(data, size as usize)))) as i32
    }
    fn server_receive_from(&self, server: usize, buf: &mut [u8], size: &mut u32, index: &mut u32) -> i32 {
        let a = self.op(format!("s13 v{server}"));
        let w: Vec<&str> = a.split(' ').collect();
        *size = w[1].parse().unwrap();
        *index = w[2].parse().unwrap();
        let r: i32 = w[0].parse().unwrap();
        if r == 0 {
            buf.fill(0);
            let bytes = unhex(w[3]);
            buf[..bytes.len()].copy_from_slice(&bytes);
        }
        r
    }
    fn server_close(&self, server: usize) {
        self.num(format!("s14 v{server}"));
    }
    fn msg_type(&self, m: Msg) -> u8 {
        self.num(format!("s15 m{m}")) as u8
    }
    fn msg_set_slot(&self, m: Msg, originating: bool, slot: u8) {
        self.num(format!("s16 m{m} {} {slot}", originating as u32));
    }
    fn msg_flag(&self, m: Msg, f: MsgFlag) -> bool {
        self.num(format!("s17 m{m} {}", f as u32)) != 0
    }
    fn msg_get(&self, m: Msg, f: MsgField) -> u32 {
        self.num(format!("s18 m{m} {}", f as u32)) as u32
    }
    fn msg_new(&self, kind: NewMsg, a: u32, b: u32, c: u32) -> Msg {
        id(&self.op(format!("s19 {} {a} {b} {c}", kind as u32)))
    }
    fn msg_tick_response_input(&self, m: Msg, i: u32, out: &mut Msg) -> i32 {
        let a = self.op(format!("s20 m{m} {i}"));
        let (r, input) = a.split_once(' ').unwrap();
        *out = id(input);
        r.parse().unwrap()
    }
    fn msg_add_input(&self, tick: Msg, input: Msg) -> i32 {
        self.num(format!("s21 m{tick} m{input}")) as i32
    }
    fn msg_set_s2aw_id(&self, m: Msg, id: u32) {
        self.num(format!("s22 m{m} {id}"));
    }
    fn msg_export_new(&self, m: Msg, size: &mut u32) -> usize {
        let a = self.op(format!("s23 m{m}"));
        let (n, bytes) = a.split_once(' ').unwrap();
        *size = n.parse().unwrap();
        self.keep(unhex(bytes))
    }
    fn bytes_free(&self, _data: usize) {
        self.num("s24".into());
    }
    fn boot_player(&self, slot: u8, was_booted: bool, game_is_exiting: bool) {
        self.num(format!("s25 {slot} {} {}", was_booted as u32, game_is_exiting as u32));
    }
    fn server_slot28(&self) -> bool {
        self.num("s26".into()) != 0
    }
    fn is_migrating(&self) -> bool {
        self.num("s27".into()) != 0
    }
    fn before_removing_players(&self) {
        self.num("s28".into());
    }
    fn handle_reconnect_wait_timed_out(&self) {
        self.num("s29".into());
    }
    fn player_send_to_slot(&self, player: u8) -> u8 {
        self.num(format!("s30 {player}")) as u8
    }
    fn filter_player_input(&self, m: Msg) -> bool {
        self.num(format!("s31 m{m}")) != 0
    }
    fn prepare_local_input(&self, m: Msg) {
        self.num(format!("s32 m{m}"));
    }
    fn is_player_present(&self, player: u8) -> bool {
        self.num(format!("s33 {player}")) != 0
    }
    fn report_laggers(&self, slots: &[u8], warning: bool) {
        self.num(format!("s34 {} {}", hex(slots), warning as u32));
    }
    fn process_system_message(&self, m: Msg) {
        self.num(format!("s35 m{m}"));
    }
    fn process_local_user_input(&self, m: Msg, player: u8) -> i32 {
        self.expect(&format!("s36 m{m} {player}"));
        // the virtual is the server's own: its ops are the record's next ones, then the answer
        let r = self.server().process_local_user_input(m, player);
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
        names: Mutex::new(Vec::new()),
        server: Mutex::new(0),
    }));
    ME.with(|m| m.set(0));
    let mut server: Option<Box<Server<Replay>>> = None;
    let mut comms: Option<Arc<Comms>> = None;
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
            let s = server.as_ref().unwrap();
            let fields = [
                SvGet::CurrentCommunicationsTick,
                SvGet::CurrentPhysicsTick,
                SvGet::GameState,
                SvGet::SendInterval,
                SvGet::PTsPerCt,
                SvGet::PTsPerSecond,
                SvGet::LastCtMessageId,
                SvGet::ProcessingSendToAllAndWait,
                SvGet::IsOos,
                SvGet::NextChecksumCt,
                SvGet::ReconnectCount,
                SvGet::LowestReconnectCt,
                SvGet::WaitForReconnectsTime,
                SvGet::GameWasPublished,
            ];
            let mut have: Vec<String> = fields.iter().map(|f| s.field(*f).to_string()).collect();
            let (i, w) = s.queue_lengths();
            have.push(i.to_string());
            have.push(w.to_string());
            let have = have.join(" ");
            if have != state {
                host.fail(format!("the server's state is `{have}`, the record's `{state}`"));
            }
            continue;
        }
        let call = line.strip_prefix("call ").unwrap_or_else(|| panic!("`{line}` between calls"));
        let w: Vec<&str> = call.split(' ').collect();
        let n = |i: usize| -> u32 { w[i].parse().unwrap() };
        let mut r: i64 = 0;
        match w[0] {
            "new" => {
                let a = ServerArgs {
                    max_players: n(1) as u8,
                    default_send_interval: n(2),
                    pts_per_ct: n(3) as u8,
                    max_pts_per_second: n(4),
                    min_pts_per_second: n(5),
                    cost_factor: w[6].parse().unwrap(),
                    max_send_interval: n(7),
                    send_interval_increment: n(8),
                    send_interval_decrement: n(9),
                    current_communications_tick: n(10),
                    current_physics_tick: 0,
                    allow_new_connections: w[11] == "1",
                };
                // the C++ base class's fields: the driver's (one human; the base takes the first tick)
                let c = Arc::new(Comms::new(n(10)));
                c.set(BaseField::NumHumanPlayers, 1);
                comms = Some(c.clone());
                let s = Server::new(host.clone(), c, a);
                *host.0.server.lock().unwrap() = &*s as *const Server<Replay> as usize;
                server = Some(s);
            }
            "free" => {
                let s = server.take().unwrap();
                s.release();
                drop(s);
            }
            "run" | "pass" | "packets" | "xferevent" => {}
            "base" => {
                // the driver's fields: the humans, multiplayer, paused, the average, the CTs processed
                let f = [
                    BaseField::NumHumanPlayers,
                    BaseField::IsMultiplayerSession,
                    BaseField::IsGamePaused,
                    BaseField::AveragePhysicsUpdateTime,
                    BaseField::CtsProcessed,
                ][n(1) as usize];
                comms.as_ref().unwrap().set(f, w[2].parse::<i64>().unwrap() as u32);
            }
            _ => {
                let s = server.as_ref().unwrap();
                match w[0] {
                    "startthreads" => s.start_threads(&**s as *const Server<Replay> as usize),
                    "stopthreads" => s.stop_threads(),
                    "input" => r = s.process_local_user_input(id(w[1]), n(2) as u8) as i64,
                    "start" => s.set_event(SvEvent::StartGame),
                    "stop" => s.set_event(SvEvent::StopGame),
                    "pause" => s.pause_game(true),
                    "unpause" => s.pause_game(false),
                    "s2aw" => {
                        r = s.send_to_all_and_wait(id(w[1]), n(2), n(3)) as i64;
                    }
                    "allow" => s.allow_new_players(w[1] == "1"),
                    "interval" => s.set_send_interval(n(1)),
                    "forcect" => s.set_event(SvEvent::ForceNextCt),
                    "ping" => s.set_field(SvSet::Ping, n(1)),
                    "reconnects" => s.start_waiting_for_reconnects(n(1)),
                    "resume" => s.set_event(SvEvent::ResumeMigratedGame),
                    "setct" => s.set_field(SvSet::CurrentCommunicationsTick, n(1)),
                    "xfer" => {
                        let mut x = 0;
                        r = if s.xfer_file_to_client(id(w[1]), 5, n(3) as u8, &mut x) { 0 } else { 0x10 };
                    }
                    "abortxfer" => s.abort_xfer(n(1)),
                    "checksum" => s.log_checksum_response(n(1), n(2), SLOT_SERVER),
                    "broadcast" => r = s.broadcast_message(id(w[1])) as i64,
                    "remove" => {
                        s.remove_player_from_game_in_progress(n(1) as u8, w[2] == "1", w[3] == "1", w[4] == "1")
                    }
                    "send" => {
                        r = if w[2] == "1" {
                            s.send_message_to_all(id(w[1]), w[4] == "1")
                        } else {
                            s.send_message_to(n(3) as u8, id(w[1]), w[4] == "1")
                        } as i32 as i64
                    }
                    "locksct" => {
                        s.lock_send_communications_tick();
                        s.unlock_send_communications_tick();
                    }
                    "slot" => {
                        if w[2] == "1" {
                            s.register_slot(n(1) as u8)
                        } else {
                            s.unregister_slot(n(1) as u8)
                        }
                    }
                    "state" => s.set_field(SvSet::GameState, n(1)),
                    "sendtoslot" => match w[1] {
                        "0" => s.set_send_to_slot_for_player(n(2) as u8, n(3) as u8),
                        "1" => r = s.send_to_slot_for_player(n(2) as u8) as i64,
                        _ => s.clear_send_to_slot_array(),
                    },
                    "search" => r = s.search_for_game(w[1] == "1") as i64,
                    "lowestcts" => s.mm().set_lowest_cts(n(1)),
                    "number" => r = s.mm().current_number() as i32 as i64,
                    "simtick" => s.sim_communications_tick(),
                    "tune" => r = s.tune_server() as i64,
                    _ => panic!("unknown call `{call}`"),
                }
            }
        }
        // (the world's own calls' rets come after the switches their threads made; the loop skips them)
        if !matches!(w[0], "run" | "pass" | "packets" | "base" | "xferevent") {
            expect_ret(&host, r);
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
fn the_server_golden_replays() {
    let seqs = sequences();
    assert!(seqs.len() >= 40);
    for (i, s) in seqs.into_iter().enumerate() {
        let r = std::thread::spawn(move || replay(s)).join();
        assert!(r.is_ok(), "sequence {} failed", i + 1);
    }
}

#[test]
fn the_record_reaches_the_lockstep_paths() {
    let s: Vec<String> = sequences().into_iter().flatten().collect();
    // ticks sent with inputs, a send-to-all-and-wait sent, a client's tick response's input replayed, laggers booted
    assert!(s.iter().any(|l| l.starts_with("op s21 ")), "no input went with a tick");
    assert!(s.iter().any(|l| l.starts_with("op s22 ")), "no send-to-all-and-wait went");
    assert!(s.iter().any(|l| l.starts_with("op s20 ")), "no client input was replayed");
    assert!(s.iter().any(|l| l.starts_with("op s19 6 ")), "no tick was made");
}
