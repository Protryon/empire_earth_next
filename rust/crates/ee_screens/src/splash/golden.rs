//! The loading screen against the real C++: a scratch driver (`build/sc11/ss_driver.cpp`) ran the unmodified
//! `EEUIESSEOpen.cpp` over a fake form 23 (labels, status bars with their vertex arrays, background 0x84), the lobby
//! options' 16 slots (type, colour, local flag, name) and read lock, the communications' human count, the decryption
//! (`D:` and the first six characters), the registered name and the player colours: one session of 2,000 cases, each
//! drawing that state (printed) and firing the open event (`golden_splash.txt`). The replay prints what the driver's
//! fakes print for each host call. The driver relinked over `EEScreens.cpp` and this crate prints the same file, and
//! the same on seeds 2-4 at 20,000 cases.

use super::*;
use crate::menus::EE_SCREENS_TEXT_LABEL;

const GOLDEN: &[u8] = include_bytes!("../golden_splash.txt");

/// The decryption's input per text (the original's encrypted lines).
const TITLE: &str = "B7B2265DB7B2265D2C11B5F7AC114576B5A95DCC84F90596C4116DB6D6B115C626B327BEB7B274D5";
const COPYRIGHT: &str =
    "B7B227FDB7B227FD1CC12D1714F15D2EFC610D9EB419BD94242B2506FCD147BEA49B253E24C9BD96AC19859696B327BEB7B25FB5";

#[derive(Default)]
struct World {
    out: Vec<String>,
    humans: u32,
    slots: Vec<(u32, u32, bool, String)>,
    local: i64,
}

fn bits_of(colour: [f32; 3]) -> String {
    format!("{:08x} {:08x} {:08x}", colour[0].to_bits(), colour[1].to_bits(), colour[2].to_bits())
}

impl SplashHost for World {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32 {
        match op {
            EE_SCREENS_GAME_SS_LOCK => self.out.push(if a != 0 { "  readlock" } else { "  readunlock" }.into()),
            EE_SCREENS_GAME_SS_HUMANS => return self.humans,
            EE_SCREENS_GAME_SS_PLAYER => {
                let (kind, _, local, _) = &self.slots[a as usize];
                return (kind & 0xff) | (*local as u32) << 8;
            }
            EE_SCREENS_GAME_SS_LOCAL => self.local = a as i64,
            EE_SCREENS_OP_SHOW => self.out.push(format!("  mode {a} {b}")),
            EE_SCREENS_OP_PGS_BAR_OPACITY => self.out.push(format!("  opacity {a} {b:08x}")),
            EE_SCREENS_OP_PGS_BAR_COLOUR => self.out.push(format!("  color {a} {b:08x} {c:08x} {d:08x}")),
            EE_SCREENS_OP_SS_BAR_PLAYER_COLOUR => {
                let colour = self.slots[b as usize].1;
                self.out.push(format!("  playercolor {colour}"));
                let i = (colour & 15) as f32;
                let rgb = [i * 0.0625, 1.0 - i * 0.0625, (colour & 15) as f32 % 3.0 * 0.5];
                self.out.push(format!("  color {a} {}", bits_of(rgb)));
            }
            EE_SCREENS_OP_SS_BAR_VALUE => self.out.push(format!("  value {a} {}", b as i32)),
            EE_SCREENS_OP_PGS_RECT => {
                assert_eq!(b, 3);
                return (10.25f32 * a as f32 + 7.75).to_bits();
            }
            EE_SCREENS_OP_SS_BACKGROUND => {
                self.out.push("  background 84".into());
                self.out.push(format!(
                    "  bgrect {:08x} {:08x} {:08x} {a:08x}",
                    5.0f32.to_bits(),
                    6.0f32.to_bits(),
                    300.0f32.to_bits()
                ));
                self.out.push("  bgvisible 1".into());
            }
            EE_SCREENS_OP_BACKGROUND => {
                assert_eq!((a, b), (0x84, 0));
                self.out.push("  background 84".into());
                self.out.push("  bgvisible 0".into());
            }
            other => panic!("unexpected op {other:#x}"),
        }
        0
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        let decrypt = |out: &mut Vec<String>, text: &str| {
            out.push(format!("  decrypt '{text}'"));
            format!("D:{}", &text[..6]).into_bytes()
        };
        match which {
            EE_SCREENS_TEXT_SS_TITLE => decrypt(&mut self.out, TITLE),
            EE_SCREENS_TEXT_SS_COPYRIGHT => decrypt(&mut self.out, COPYRIGHT),
            EE_SCREENS_TEXT_SS_SUFFIX => "Max \u{c9}".as_bytes().to_vec(),
            EE_SCREENS_TEXT_SS_NAME => self.slots[arg as usize].3.clone().into_bytes(),
            other => panic!("unexpected text {other:#x}"),
        }
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) {
        assert_eq!(which, EE_SCREENS_TEXT_LABEL);
        self.out.push(format!("  text {arg} '{}' 1", String::from_utf8(text.to_vec()).unwrap()));
    }
}

#[test]
fn golden_splash() {
    let text = String::from_utf8(GOLDEN.to_vec()).unwrap().replace("\r\n", "\n");
    let mut cases: Vec<Vec<&str>> = Vec::new();
    for line in text.lines() {
        if line.starts_with("case ") {
            cases.push(Vec::new());
        }
        cases.last_mut().unwrap().push(line);
    }
    assert_eq!(cases.len(), 2000);
    let mut rows = 0;
    for (n, case) in cases.iter().enumerate() {
        // state humans H slots type,colour,local,'name' x16
        let state = case[1];
        let humans: u32 = state.split(' ').nth(4).unwrap().parse().unwrap();
        let slots_text = &state[state.find(" slots ").unwrap() + 7..];
        let mut slots = Vec::new();
        for slot in slots_text.split(' ').fold(Vec::<String>::new(), |mut acc, t| {
            // names may hold spaces: a token without the closing quote continues the previous one
            match acc.last_mut() {
                Some(last) if last.matches('\'').count() == 1 => {
                    last.push(' ');
                    last.push_str(t);
                }
                _ => acc.push(t.to_string()),
            }
            acc
        }) {
            let mut parts = slot.splitn(4, ',');
            let kind: u32 = parts.next().unwrap().parse().unwrap();
            let colour: u32 = parts.next().unwrap().parse().unwrap();
            let local = parts.next().unwrap() == "1";
            let name = parts.next().unwrap().trim_matches('\'').to_string();
            slots.push((kind, colour, local, name));
        }
        assert_eq!(slots.len(), 16, "case {n}");
        let mut w = World { humans, slots, local: -7, ..Default::default() };
        let result = dispatch(&mut w, EE_SCREENS_SS_OPEN).unwrap();
        rows += w.out.iter().filter(|l| l.starts_with("  value")).count();
        w.out.push(format!("  result {result} local {}", w.local));
        assert_eq!(w.out, &case[2..], "case {n}");
    }
    assert!(rows > 5000);
}
