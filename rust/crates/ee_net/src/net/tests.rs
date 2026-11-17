//! The message manager's golden (`golden_msgmgr.txt`, recorded by tests/net/ne_msgmgr_test over the Rust side, whose
//! world traces equal the original C++ manager's): each recorded call is made again on a fresh Rust manager whose host
//! answers from the record, and every op the Rust makes must be the next recorded one, with the same arguments.

use std::cell::RefCell;
use std::collections::VecDeque;

use crate::net::msgmgr::{File, Host, Msg, MsgMgr};

const GOLDEN: &str = include_str!("golden_msgmgr.txt");

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

struct Replay {
    lines: RefCell<VecDeque<String>>,
}

impl Replay {
    /// The next recorded op, which must be `code` with these arguments: its answer
    fn op(&self, code: u32, a: String, b: String, c: String, d: String) -> String {
        let want = format!("op {code} {a} {b} {c} {d} = ");
        let line = self.lines.borrow_mut().pop_front().unwrap_or_default();
        assert!(line.starts_with(&want), "expected `{line}`, the Rust made `{want}`");
        line[want.len()..].to_string()
    }
    fn num(&self, code: u32, a: usize, b: usize, c: usize, d: usize) -> i64 {
        self.op(code, a.to_string(), b.to_string(), c.to_string(), d.to_string()).parse().unwrap()
    }
    fn msg(&self, code: u32, m: Msg) -> i64 {
        self.op(code, format!("m{m}"), "0".into(), "0".into(), "0".into()).parse().unwrap()
    }
    fn file(&self, code: u32, f: File) -> String {
        self.op(code, format!("f{f}"), "0".into(), "0".into(), "0".into())
    }
}

impl Host for Replay {
    fn msg_reference(&self, m: Msg) {
        self.msg(0, m);
    }
    fn msg_dereference(&self, m: Msg) {
        self.msg(1, m);
    }
    fn msg_type(&self, m: Msg) -> u8 {
        self.msg(2, m) as u8
    }
    fn msg_is_response(&self, sent: Msg, resp: Msg) -> bool {
        self.op(3, format!("m{sent}"), format!("m{resp}"), "0".into(), "0".into()) != "0"
    }
    fn msg_slot(&self, m: Msg) -> u8 {
        self.msg(4, m) as u8
    }
    fn msg_send_to(&self, slot: u8, m: Msg) -> u32 {
        self.op(5, slot.to_string(), format!("m{m}"), "0".into(), "0".into()).parse::<i64>().unwrap() as u32
    }
    fn msg_tick(&self, m: Msg) -> u32 {
        self.msg(6, m) as u32
    }
    fn new_response(&self, kind: u8, m: Msg) -> Msg {
        id(&self.op(7, kind.to_string(), format!("m{m}"), "0".into(), "0".into()))
    }
    fn last_tick(&self) -> i32 {
        self.num(8, 0, 0, 0, 0) as i32
    }
    fn set_last_tick(&self, tick: u32) {
        self.num(9, tick as usize, 0, 0, 0);
    }
    fn cts_processed(&self) -> u32 {
        self.num(10, 0, 0, 0, 0) as u32
    }
    fn avg_physics(&self) -> u32 {
        self.num(11, 0, 0, 0, 0) as u32
    }
    fn process_system_message(&self, m: Msg) {
        self.msg(12, m);
    }
    fn force_ct(&self) {
        self.num(13, 0, 0, 0, 0);
    }
    fn last_ct_message_id(&self) -> u32 {
        self.num(14, 0, 0, 0, 0) as u32
    }
    fn start_send_to_all_and_wait(&self, number: u32, kind: u8, timeout: u32, retries: u32) {
        self.num(15, number as usize, kind as usize, timeout as usize, retries as usize);
    }
    fn time(&self) -> u32 {
        self.num(16, 0, 0, 0, 0) as u32
    }
    fn reset_client_connection(&self, slot: u8) {
        self.num(17, slot as usize, 0, 0, 0);
    }
    fn xfer_status(&self, slot: u8, percent: u8, id: u32, status: u8) {
        self.num(18, slot as usize, percent as usize, id as usize, status as usize);
    }
    fn xfer_send(&self, slot: u8, status: u8, data: &[u8]) -> bool {
        self.op(19, slot.to_string(), status.to_string(), hex(data), data.len().to_string()) != "0"
    }
    fn file_clone(&self, spec: usize) -> File {
        id(&self.file(20, spec))
    }
    fn file_free(&self, f: File) {
        self.file(21, f);
    }
    fn file_is_open(&self, f: File) -> bool {
        self.file(22, f) != "0"
    }
    fn file_open(&self, f: File) -> bool {
        self.file(23, f) != "0"
    }
    fn file_close(&self, f: File) {
        self.file(24, f);
    }
    fn file_length(&self, f: File) -> u32 {
        self.file(25, f).parse().unwrap()
    }
    fn file_read(&self, f: File, buf: &mut [u8; 0x400]) -> Option<u32> {
        let answer = self.file(26, f);
        let (r, bytes) = answer.split_once(' ').unwrap();
        let r: i64 = r.parse().unwrap();
        let bytes = unhex(bytes);
        buf[..bytes.len()].copy_from_slice(&bytes);
        (r >= 0).then_some(r as u32)
    }
    fn file_name(&self, f: File) -> Vec<u8> {
        unhex(&self.file(27, f))
    }
}

/// One recorded sequence made again: the ops between a call and its result come from the record.
fn replay(sequence: &[&str]) {
    let host = Replay { lines: RefCell::new(VecDeque::new()) };
    let mut mm: Option<MsgMgr<Replay>> = None;
    let mut i = 0;
    while i < sequence.len() {
        let call = sequence[i].strip_prefix("call ").unwrap_or_else(|| panic!("a call, not `{}`", sequence[i]));
        i += 1;
        // the ops up to the result (or to the query's line)
        let mut ops = VecDeque::new();
        while i < sequence.len() && sequence[i].starts_with("op ") {
            ops.push_back(sequence[i].to_string());
            i += 1;
        }
        let m = match &mm {
            Some(m) => m,
            None => {
                assert_eq!(call, "new");
                mm = Some(MsgMgr::new(Replay { lines: RefCell::new(VecDeque::new()) }));
                mm.as_ref().unwrap().register_slot(0xfe);
                assert_eq!(sequence[i], "ret 0");
                i += 1;
                continue;
            }
        };
        *m.host.lines.borrow_mut() = ops;
        let w: Vec<&str> = call.split(' ').collect();
        let n = |k: usize| -> u32 { w[k].parse::<u64>().unwrap() as u32 };
        let r: u64 = match w[0] {
            "reg" => !m.register_slot(n(1) as u8) as u64,
            "unreg" => {
                m.unregister_slot(n(1) as u8);
                0
            }
            "send" => m.send(id(w[1]), w[2] == "1") as u64,
            "sendto" => m.send_to(id(w[1]), n(2) as u8, w[3] == "1") as u64,
            "saw" => m.send_to_all_and_wait(id(w[1]), n(2), n(3)) as u64,
            "logresp" => {
                m.log_response(id(w[1]));
                0
            }
            "logtime" => {
                m.log_response_time(n(1) as u8, n(2), n(3) as u16);
                0
            }
            "out" => m.outstanding(n(1)) as u64,
            "resend" => {
                if m.resend(n(1)) {
                    0
                } else {
                    0xffff_ffff
                }
            }
            "who" => m.who_has_not_responded(n(1)) as u64,
            "next" => m.who_has_not_responded_next(n(1) as u8, n(2)) as u64,
            "query" => {
                let q = format!(
                    "q {} {} {} {} {} {} {}",
                    m.lowest_ct(),
                    m.highest_cpu(),
                    m.highest_response_time(),
                    m.slowest_responder(),
                    m.slowest_client_cpu(),
                    m.num_clients(),
                    m.current_number()
                );
                assert_eq!(sequence[i], q, "the queries after `{call}`");
                i += 1;
                0
            }
            "setnum" => {
                m.set_current_number(n(1));
                0
            }
            "setlow" => {
                m.set_lowest_cts(n(1));
                0
            }
            "force" => {
                m.set_force_ct(w[1] == "1");
                0
            }
            "addx" => match m.add_xfer(id(w[1]), n(2), n(3) as u8) {
                Some(_) => 0,
                None => 3,
            },
            "abortx" => {
                m.abort_xfer(n(1));
                0
            }
            "procx" => m.process_xfers() as u64,
            "free" => {
                m.release();
                0
            }
            other => panic!("unknown call `{other}`"),
        };
        let left: Vec<String> = m.host.lines.borrow_mut().drain(..).collect();
        assert!(left.is_empty(), "after `{call}` the record has ops the Rust did not make: {left:?}");
        assert_eq!(sequence[i], format!("ret {r}"), "the result of `{call}`");
        i += 1;
    }
    drop(host);
}

#[test]
fn the_golden_sequences_replay() {
    let mut sequences: Vec<Vec<&str>> = Vec::new();
    for line in GOLDEN.lines() {
        if line.starts_with("seq ") {
            sequences.push(Vec::new());
        } else {
            sequences.last_mut().unwrap().push(line);
        }
    }
    assert!(sequences.len() >= 100);
    for s in &sequences {
        replay(s);
    }
}

/// A host that answers every read with a default and records the calls that change something.
#[derive(Default)]
struct Log {
    calls: RefCell<Vec<String>>,
}

impl Host for Log {
    fn msg_reference(&self, m: Msg) {
        self.calls.borrow_mut().push(format!("ref {m}"));
    }
    fn msg_dereference(&self, m: Msg) {
        self.calls.borrow_mut().push(format!("deref {m}"));
    }
    fn msg_type(&self, _: Msg) -> u8 {
        0x41
    }
    fn msg_is_response(&self, _: Msg, _: Msg) -> bool {
        false
    }
    fn msg_slot(&self, _: Msg) -> u8 {
        0
    }
    fn msg_send_to(&self, slot: u8, m: Msg) -> u32 {
        self.calls.borrow_mut().push(format!("send {slot} {m}"));
        0
    }
    fn msg_tick(&self, _: Msg) -> u32 {
        0
    }
    fn new_response(&self, _: u8, _: Msg) -> Msg {
        99
    }
    fn last_tick(&self) -> i32 {
        -2
    }
    fn set_last_tick(&self, _: u32) {}
    fn cts_processed(&self) -> u32 {
        0
    }
    fn avg_physics(&self) -> u32 {
        0
    }
    fn process_system_message(&self, m: Msg) {
        self.calls.borrow_mut().push(format!("system {m}"));
    }
    fn force_ct(&self) {}
    fn last_ct_message_id(&self) -> u32 {
        0
    }
    fn start_send_to_all_and_wait(&self, _: u32, _: u8, _: u32, _: u32) {}
    fn time(&self) -> u32 {
        0
    }
    fn reset_client_connection(&self, _: u8) {}
    fn xfer_status(&self, _: u8, _: u8, _: u32, _: u8) {}
    fn xfer_send(&self, _: u8, _: u8, _: &[u8]) -> bool {
        true
    }
    fn file_clone(&self, spec: usize) -> File {
        spec + 100
    }
    fn file_free(&self, f: File) {
        self.calls.borrow_mut().push(format!("free file {f}"));
    }
    fn file_is_open(&self, _: File) -> bool {
        false
    }
    fn file_open(&self, _: File) -> bool {
        true
    }
    fn file_close(&self, _: File) {}
    fn file_length(&self, _: File) -> u32 {
        0
    }
    fn file_read(&self, _: File, _: &mut [u8; 0x400]) -> Option<u32> {
        None
    }
    fn file_name(&self, _: File) -> Vec<u8> {
        Vec::new()
    }
}

#[test]
fn the_ring_keeps_the_last_fifty_and_release_frees_queued_transfers() {
    let m = MsgMgr::new(Log::default());
    assert!(m.register_slot(0xfe));
    assert!(m.register_slot(3));
    assert!(!m.register_slot(3));
    for i in 0..60usize {
        assert_eq!(m.send(1000 + i, true), i as u32);
    }
    // number 9 left the ring, 10..59 are stored, each still owed by slot 3 (the server's own was logged? no: its
    // response is not one of these messages), so two slots owe each
    assert_eq!(m.outstanding(9), 0);
    assert_eq!(m.outstanding(10), 2);
    assert_eq!(m.who_has_not_responded(59), 3);
    assert_eq!(m.who_has_not_responded_next(3, 59), 0xfe);
    assert_eq!(m.who_has_not_responded_next(7, 59), 0xff);
    assert_eq!(m.add_xfer(5, 0, 3), Some(1));
    assert_eq!(m.add_xfer(6, 0, 3), Some(2));
    assert_eq!(m.add_xfer(6, 0, 4), None);
    m.calls_clear();
    m.release();
    let calls = m.host.calls.borrow();
    // the 50 stored messages released in ring order (60 sends: the ring's slot 0 holds number 49, msg 1049)
    assert_eq!(calls.iter().filter(|c| c.starts_with("deref")).count(), 50);
    assert_eq!(calls[0], "deref 1049");
    assert_eq!(calls[50..], ["free file 105".to_string(), "free file 106".to_string()]);
}

impl MsgMgr<Log> {
    fn calls_clear(&self) {
        self.host.calls.borrow_mut().clear();
    }
}
