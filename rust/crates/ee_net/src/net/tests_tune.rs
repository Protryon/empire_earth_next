//! The tuning's golden (`golden_tune.txt`, recorded by tests/net/ne_tune_test over the Rust side, whose world traces
//! equal the original C++ members'): each recorded call is made again on a fresh `Tune` whose host answers from the
//! record; every op must be the next recorded one, and the state after each call the recorded one. Then the
//! conversions' edges.

use std::cell::RefCell;
use std::collections::VecDeque;

use crate::net::tune::*;

const GOLDEN: &str = include_str!("golden_tune.txt");

struct Replay {
    lines: RefCell<VecDeque<String>>,
}

impl Replay {
    fn pop(&self) -> String {
        self.lines.borrow_mut().pop_front().expect("the record ended")
    }
    fn op(&self, args: String) -> i64 {
        let line = self.pop();
        assert_eq!(line, format!("op {args}"), "the record's op against the Rust's");
        let answer = self.pop();
        answer.strip_prefix("= ").unwrap_or_else(|| panic!("`{answer}` for an answer")).parse().unwrap()
    }
}

impl TuneHost for Replay {
    fn get(&self, f: Field) -> u32 {
        self.op(format!("0 {}", f as u32)) as u32
    }
    fn set(&self, f: Field, value: u32) {
        self.op(format!("1 {} {value}", f as u32));
    }
    fn event_set(&self, which: u32) {
        self.op(format!("2 {which}"));
    }
    fn mm_outstanding(&self, number: u32) -> u8 {
        self.op(format!("3 {number}")) as u8
    }
    fn mm_highest_response_time(&self) -> u32 {
        self.op("4".into()) as u32
    }
    fn mm_highest_cpu(&self) -> u16 {
        self.op("5".into()) as u16
    }
    fn mm_lowest_ct(&self) -> u32 {
        self.op("6".into()) as u32
    }
    fn mm_resend(&self, number: u32) {
        self.op(format!("7 {number}"));
    }
    fn mm_who(&self, number: u32) -> u8 {
        self.op(format!("8 {number}")) as u8
    }
    fn mm_who_next(&self, slot: u8, number: u32) -> u8 {
        self.op(format!("9 {slot} {number}")) as u8
    }
    fn mm_num_clients(&self) -> u8 {
        self.op("10".into()) as u8
    }
    fn mm_slowest_responder(&self) -> u8 {
        self.op("11".into()) as u8
    }
    fn mm_slowest_client_cpu(&self) -> u8 {
        self.op("12".into()) as u8
    }
    fn mm_set_force_ct(&self, on: bool) {
        self.op(format!("13 {}", on as u32));
    }
    fn unregister_slot(&self, slot: u8) {
        self.op(format!("14 {slot}"));
    }
    fn before_removing_players(&self) {
        self.op("15".into());
    }
    fn boot_player(&self, slot: u8, was_booted: bool, game_is_exiting: bool) {
        self.op(format!("16 {slot} {} {}", was_booted as u32, game_is_exiting as u32));
    }
    fn report_laggers(&self, slots: &[u8], warning: bool) {
        let hex: String =
            if slots.is_empty() { "-".into() } else { slots.iter().map(|b| format!("{b:02x}")).collect() };
        self.op(format!("17 {hex} {}", warning as u32));
    }
    fn remove_player_input(&self, slot: u8) {
        self.op(format!("18 {slot}"));
    }
}

fn replay(lines: Vec<String>) {
    let mut lines: VecDeque<String> = lines.into();
    let first = lines.pop_front().unwrap();
    let decrement = first.strip_prefix("new ").expect("the record starts with the constructor").parse().unwrap();
    let t = Tune::new(Replay { lines: RefCell::new(lines) }, decrement);
    loop {
        let Some(line) = t.host.lines.borrow_mut().pop_front() else { break };
        if let Some(state) = line.strip_prefix("state ") {
            let s = t.state();
            assert_eq!(format!("{} {} {}", s[0], s[8], s[11]), state, "the tuning's state");
            continue;
        }
        let call = line.strip_prefix("call ").unwrap_or_else(|| panic!("`{line}` between calls"));
        let w: Vec<&str> = call.split(' ').collect();
        let r: i64 = match w[0] {
            "0" => t.tune_server() as i64,
            "1" => t.tune_for_slow_network() as i64,
            "2" => t.tune_for_slow_cpu() as i64,
            "3" => {
                t.boot_all_laggers(w[1].parse().unwrap());
                0
            }
            "4" => {
                t.boot_the_slowest_responder();
                0
            }
            "5" => {
                t.boot_the_slowest_client_cpu();
                0
            }
            _ => t.slowest_client_cpu() as i64,
        };
        let ret = t.host.pop();
        assert_eq!(ret, format!("ret {r}"), "the call's answer");
    }
}

#[test]
fn the_tune_golden_replays() {
    let mut seqs: Vec<Vec<String>> = Vec::new();
    for line in GOLDEN.lines() {
        if line.starts_with("seq ") {
            seqs.push(Vec::new());
        } else {
            seqs.last_mut().unwrap().push(line.to_string());
        }
    }
    assert!(seqs.len() >= 40);
    for s in seqs {
        replay(s);
    }
}

#[test]
fn the_conversions_are_x86_64s() {
    // a float to unsigned char: 32-bit truncation, the low byte (out of range: 0x80000000's, 0)
    assert_eq!(f32_to_u8(39.9), 39);
    assert_eq!(f32_to_u8(300.5), 300u32 as u8);
    assert_eq!(f32_to_u8(1.0e10), 0);
    assert_eq!(f32_to_u8(f32::INFINITY), 0);
    assert_eq!(f32_to_u8(f32::NAN), 0);
    // a float to unsigned long: 64-bit truncation, the low 32 bits
    assert_eq!(f32_to_u32(13.99), 13);
    assert_eq!(f32_to_u32(4294967296.0 + 4096.0), 4096);
    assert_eq!(f32_to_u32(1.0e30), 0);
    // a server that has not sent: 40 PTs a second over a 0xffffffff interval (then 0xfe PTs per CT)
    let x = 40.0f32 / (1000.0f32 / 4294967295u32 as f32);
    assert_eq!(f32_to_u32(x), x as u32);
    assert!(f32_to_u32(x) >= 0xff);
}
