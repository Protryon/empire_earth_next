//! The debug sound panel against the real C++: a scratch driver (`build/sc10/dsu_driver.cpp`) ran the unmodified
//! `EEDSUIEvents.cpp` over a fake first selected unit (its complex unit, its class's and model's ten sounds among four
//! sound objects or none), the event's entity (a slider and its value, another control or none), the panel's value
//! labels by id (a label, another control or none) and the manager's Pop: one session of 2,000 cases, each drawing
//! that state (printed) and firing one of the 11 events (`golden_debug_sound.txt`). The replay prints what the
//! driver's fakes print for each host call. The driver relinked over `EEScreens.cpp` and this crate prints the same
//! file, and the same on seeds 2-4 at 20,000 cases.

use std::collections::BTreeMap;

use super::*;

const GOLDEN: &[u8] = include_bytes!("../golden_debug_sound.txt");

/// The kinds' sound fields in the state line's order (acknowledge 0, 1, creation, death, selection 0, 1; the model's
/// first and second arrays' first entries, then their last).
const KIND_FIELD: [usize; 11] = [usize::MAX, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9];

#[derive(Default)]
struct World {
    out: Vec<String>,
    selection: bool,
    complex: bool,
    entity: u32,
    range: i32,
    sounds: [i32; 10],
    ids: BTreeMap<u32, u32>,
    kept: Option<i32>,
}

impl DebugSoundHost for World {
    fn call(&mut self, op: u32, a: u32) -> u32 {
        match op {
            EE_SCREENS_GAME_DSU_SOUND => {
                self.out.push(format!("  firstselected -> {}", self.selection as u32));
                self.kept = None;
                if !self.selection || !self.complex {
                    return 0;
                }
                let sound = self.sounds[KIND_FIELD[a as usize]];
                if sound < 0 {
                    return 1;
                }
                self.kept = Some(sound);
                2
            }
            EE_SCREENS_OP_DSU_SLIDER => (self.entity == 1) as u32,
            EE_SCREENS_OP_ENTITY_RANGE => {
                assert_eq!(self.entity, 1);
                self.range as u32
            }
            EE_SCREENS_GAME_DSU_VOLUME => {
                self.out.push(format!("  volume {} {a:08x}", self.kept.unwrap()));
                0
            }
            EE_SCREENS_OP_POP => {
                self.out.push("  pop".into());
                0
            }
            other => panic!("unexpected op {other:#x}"),
        }
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) {
        assert_eq!(which, EE_SCREENS_TEXT_DSU_LABEL);
        let kind = self.ids[&arg];
        self.out.push(format!("  controlbyid 22 {arg} -> {kind}"));
        if kind == 1 {
            self.out.push(format!("  text 1 '{}' 1", String::from_utf8(text.to_vec()).unwrap()));
        }
    }
}

#[test]
fn golden_debug_sound() {
    let text = String::from_utf8(GOLDEN.to_vec()).unwrap().replace("\r\n", "\n");
    let mut cases: Vec<Vec<&str>> = Vec::new();
    for line in text.lines() {
        if line.starts_with("case ") {
            cases.push(Vec::new());
        }
        cases.last_mut().unwrap().push(line);
    }
    assert_eq!(cases.len(), 2000);
    let mut sorted_ids: Vec<u32> = DSU_VALUE_LABELS[1..].to_vec();
    sorted_ids.sort();
    let mut events = std::collections::BTreeSet::new();
    let mut volumes = 0;
    for (n, case) in cases.iter().enumerate() {
        let id: core::ffi::c_ulong = case[0].split(' ').nth(3).unwrap().parse().unwrap();
        let event = event_for_control(id);
        events.insert(event);
        // state selection S complex C entity E range R sounds x10 ids x10
        let t: Vec<&str> = case[1].split_whitespace().collect();
        let mut w = World {
            selection: t[2] == "1",
            complex: t[4] == "1",
            entity: t[6].parse().unwrap(),
            range: t[8].parse::<i64>().unwrap() as i32,
            ..Default::default()
        };
        for k in 0..10 {
            w.sounds[k] = t[10 + k].parse().unwrap();
            w.ids.insert(sorted_ids[k], t[21 + k].parse().unwrap());
        }
        let result = dispatch(&mut w, event).unwrap();
        w.out.push(format!("  result {result}"));
        volumes += w.out.iter().filter(|l| l.starts_with("  volume")).count();
        let expected = &case[2..];
        assert_eq!(w.out, expected, "case {n} ({})", case[0]);
    }
    assert_eq!(events.len(), 11);
    assert!(volumes > 200);
}
