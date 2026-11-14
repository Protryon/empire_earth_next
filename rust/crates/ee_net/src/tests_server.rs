//! The server's checksums golden (`golden_checks.txt`, recorded by tests/net/ne_checks_test over the Rust side, whose
//! world traces equal the original C++ members'): each recorded call is made again on a fresh `Server` whose host
//! answers from the record; every op must be the next recorded one, and the state after each call the recorded one.
//! Then the out-of-sync order's edges.

use std::cell::{Cell, RefCell};
use std::collections::VecDeque;

use crate::server::*;

const GOLDEN: &str = include_str!("golden_checks.txt");

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

impl ServerHost for Replay {
    fn get(&self, f: SvField) -> u32 {
        self.op(format!("0 {}", f as u32)) as u32
    }
    fn set(&self, f: SvField, value: u32) {
        self.op(format!("1 {} {value}", f as u32));
    }
    fn is_player_present(&self, player: u8) -> bool {
        self.op(format!("2 {player}")) != 0
    }
    fn player_send_to_slot(&self, player: u8) -> u8 {
        self.op(format!("3 {player}")) as u8
    }
    fn unregister_slot(&self, slot: u8) {
        self.op(format!("4 {slot}"));
    }
    fn before_removing_players(&self) {
        self.op("5".into());
    }
    fn remove_player_input(&self, player: u8, was_booted: bool, is_oos: bool, game_is_exiting: bool) {
        self.op(format!("6 {player} {} {} {}", was_booted as u32, is_oos as u32, game_is_exiting as u32));
    }
}

fn replay(lines: Vec<String>) {
    let mut lines: VecDeque<String> = lines.into();
    assert_eq!(lines.pop_front().unwrap(), "new", "the record starts with the constructor");
    let s = Server::new(Replay { lines: RefCell::new(lines) });
    loop {
        let Some(line) = s.host.lines.borrow_mut().pop_front() else { break };
        if let Some(state) = line.strip_prefix("state ") {
            // the humans are the host's field: the record's own; the out-of-sync flag and the checksums the Rust's
            let rest = state.split_once(' ').unwrap().1;
            assert_eq!(format!("{} {}", s.is_oos() as u32, s.checks_state()), rest, "the server's state");
            continue;
        }
        let call = line.strip_prefix("call ").unwrap_or_else(|| panic!("`{line}` between calls"));
        let w: Vec<u32> = call.split(' ').map(|x| x.parse().unwrap()).collect();
        match w[0] {
            0 => s.log_checksum_response(w[1], w[2], w[3] as u8),
            1 => s.remove_oos_players(),
            _ => s.remove_player_from_game_in_progress(w[1] as u8, w[2] != 0, w[3] != 0, w[4] != 0),
        }
        assert_eq!(s.host.pop(), "ret 0", "the call's end");
    }
}

#[test]
fn the_checks_golden_replays() {
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

/// A host of three humans; the server's slot and clients 1 and 2 answer
struct Three {
    humans: Cell<u32>,
}

impl ServerHost for Three {
    fn get(&self, f: SvField) -> u32 {
        match f {
            SvField::NumHumanPlayers => self.humans.get(),
            SvField::MaxPlayers => 8,
        }
    }
    fn set(&self, _: SvField, value: u32) {
        self.humans.set(value);
    }
    fn is_player_present(&self, _: u8) -> bool {
        true
    }
    fn player_send_to_slot(&self, player: u8) -> u8 {
        player
    }
    fn unregister_slot(&self, _: u8) {}
    fn before_removing_players(&self) {}
    fn remove_player_input(&self, _: u8, _: bool, _: bool, _: bool) {}
}

#[test]
fn a_tie_goes_to_the_server_and_its_slot_comes_first() {
    let s = Server::new(Three { humans: Cell::new(4) });
    // four humans, two and two: the server's checksum wins, the others are out of sync in their checksum order
    s.log_checksum_response(5, 7, 3);
    s.log_checksum_response(5, 9, SLOT_SERVER);
    s.log_checksum_response(5, 7, 1);
    s.log_checksum_response(5, 9, 2);
    assert!(s.is_oos());
    assert_eq!(s.checks_state(), "5:7/3,7/1,9/254,9/2; oos 3 1");
    // a later tick's first answer keeps the tick that decided (the original erased only the ticks before it)
    s.log_checksum_response(6, 1, 1);
    assert_eq!(s.checks_state(), "5:7/3,7/1,9/254,9/2;6:1/1; oos 3 1");
    // the server off the most common checksum: its slot first
    let s = Server::new(Three { humans: Cell::new(3) });
    s.log_checksum_response(2, 9, 2);
    s.log_checksum_response(2, 4, SLOT_SERVER);
    s.log_checksum_response(2, 9, 1);
    assert_eq!(s.checks_state(), "2:4/254,9/2,9/1; oos 254");
    s.remove_oos_players();
    assert!(!s.is_oos());
    assert_eq!(s.checks_state(), "2:4/254,9/2,9/1; oos");
}
