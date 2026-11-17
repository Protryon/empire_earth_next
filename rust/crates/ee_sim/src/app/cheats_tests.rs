//! cheats.rs over a mock host: the table, the parse, the list and the dispatch.

use crate::app::cheats::{CheatAction, CheatHost, EeAppCheats};
use std::os::raw::{c_long, c_ulong};

#[derive(Default)]
struct Mock {
    multiplayer: bool,
    disabled: bool,
    log: Vec<String>,
    actions: Vec<(CheatAction, c_ulong)>,
}

impl CheatHost for Mock {
    fn string(&mut self, id: u32) -> Vec<u8> {
        match id {
            30622 => b"headshot".to_vec(),
            30626 => b"atm".to_vec(),
            30632 => b"bam".to_vec(),
            30633 => b"Brainstorm".to_vec(),
            30671 => b"Cheat: ".to_vec(),
            _ => format!("c{id}").into_bytes(),
        }
    }
    fn multiplayer(&mut self) -> bool {
        self.multiplayer
    }
    fn cheats_enabled(&mut self) -> bool {
        !self.disabled
    }
    fn players(&mut self) -> c_ulong {
        3
    }
    fn chat(&mut self, line: &[u8]) {
        self.log.push(String::from_utf8_lossy(line).into_owned());
    }
    fn goto(&mut self, x: f32, y: f32) {
        self.log.push(format!("goto {x} {y}"));
    }
    fn find(&mut self, id: c_ulong) {
        self.log.push(format!("find {id}"));
    }
    fn action(&mut self, action: CheatAction, player: c_ulong) {
        self.actions.push((action, player));
    }
}

fn parse(t: &EeAppCheats, h: &mut Mock, text: &str) -> (c_long, c_long) {
    let (mut code, mut parameter) = (-7, -7);
    t.parse(h, text.as_bytes(), &mut code, &mut parameter);
    (code, parameter)
}

#[test]
fn the_parse_strips_a_trailing_number_and_lowers_the_text() {
    let mut h = Mock::default();
    let t = EeAppCheats::new(&mut h);
    // a line longer than one character always takes atoi of its last two (0 here)
    assert_eq!(parse(&t, &mut h, "atm"), (5, 0));
    assert_eq!(parse(&t, &mut h, "ATM"), (5, 0));
    assert_eq!(parse(&t, &mut h, "brainstorm"), (0xd, 0));
    assert_eq!(parse(&t, &mut h, "bam 3"), (0xc, 3));
    assert_eq!(parse(&t, &mut h, "atm 77"), (5, 77));
    assert_eq!(parse(&t, &mut h, "atm 0"), (5, 0));
    assert_eq!(parse(&t, &mut h, "headshot 10"), (1, 10));
    // the last two characters "m7": 0 and not " 0", the text kept (and unknown)
    assert_eq!(parse(&t, &mut h, "atm7"), (0, 0));
    // a negative number: steps back a character
    assert_eq!(parse(&t, &mut h, "atm-5"), (0, -5));
    assert_eq!(parse(&t, &mut h, "a"), (0, -7));
    assert_eq!(parse(&t, &mut h, ""), (0, -7));
    assert_eq!(parse(&t, &mut h, "nothing here"), (0, 0));
}

#[test]
fn single_player_codes_are_refused_in_a_multiplayer_game() {
    let mut h = Mock { multiplayer: true, ..Default::default() };
    let t = EeAppCheats::new(&mut h);
    assert_eq!(parse(&t, &mut h, "headshot"), (0, 0));
    assert_eq!(parse(&t, &mut h, "atm"), (5, 0));
}

#[test]
fn goto_and_find_move_the_camera() {
    let mut h = Mock::default();
    let t = EeAppCheats::new(&mut h);
    assert_eq!(parse(&t, &mut h, "goto 40 50.5"), (0, -7));
    assert_eq!(parse(&t, &mut h, "goto 12"), (0, -7));
    assert_eq!(parse(&t, &mut h, "goto  1e2x  -3"), (0, -7));
    assert_eq!(parse(&t, &mut h, "find 2 5"), (0, -7));
    assert_eq!(parse(&t, &mut h, "find 33554440"), (0, -7));
    assert_eq!(parse(&t, &mut h, "find"), (0, -7));
    assert_eq!(parse(&t, &mut h, "find 1 2 3"), (0, -7));
    assert_eq!(h.log, vec!["goto 40 50.5", "goto 100 -3", "find 33554437", "find 33554440"]);
}

#[test]
fn the_list_has_six_lines_of_five_slots() {
    let mut h = Mock::default();
    let t = EeAppCheats::new(&mut h);
    t.list(&mut h);
    assert_eq!(h.log.len(), 6);
    assert_eq!(h.log[0], "Cheat: headshot     c30623     c30624     c30625     atm");
    assert_eq!(h.log[2], "Cheat:      c30631     bam     Brainstorm     c30636");
    assert!(h.log[3].ends_with("c30640     "));
}

#[test]
fn the_dispatch_checks_the_player_and_runs_each_cheat() {
    use CheatAction::*;
    let mut h = Mock::default();
    let t = EeAppCheats::new(&mut h);
    t.apply(&mut h, 5, 1, 0);
    t.apply(&mut h, 5, 3, 0);
    t.apply(&mut h, 0xc, 2, 4);
    t.apply(&mut h, 1, 0, 12);
    t.apply(&mut h, 1, 0, 30);
    t.apply(&mut h, 1, 0, 2);
    assert_eq!(
        h.actions,
        vec![(AddResource(5, 1000), 1), (AdvanceEpoch(3), 2), (Kill { all: false }, 2), (Kill { all: true }, 2)]
    );
    h.actions.clear();
    t.apply(&mut h, 0x15, 1, 0);
    assert_eq!(
        h.actions.iter().map(|a| a.0).collect::<Vec<_>>(),
        vec![
            AddResource(4, 100000),
            AddResource(5, 100000),
            AddResource(3, 100000),
            AddResource(2, 100000),
            AddResource(6, 100000),
            Explore,
            MakeVisible,
            ToggleInstantBuild,
            ResearchAll
        ]
    );
    h.actions.clear();
    t.apply(&mut h, 2, 1, 0);
    t.apply(&mut h, 0x1a, 1, 0);
    t.apply(&mut h, 99, 1, 0);
    assert_eq!(h.actions, vec![(EndGame { win: true }, 1), (EndGame { win: false }, 1)]);
    t.apply(&mut h, 0x17, 1, 0);
    assert_eq!(h.log.len(), 6);
    let mut off = Mock { disabled: true, ..Default::default() };
    t.apply(&mut off, 5, 1, 0);
    assert!(off.actions.is_empty());
}
