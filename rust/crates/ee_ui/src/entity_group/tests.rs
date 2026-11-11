//! `Disable` and `Enable` against the real C++ (`golden_group.txt`, its `group` cases): a scratch driver
//! (`build/ui_group_golden`) ran the unmodified `UIEntityGroup.cpp` over 400 groups of up to six entities, buttons
//! (a vtable of logging stubs; shown, hidden or another mode, on either of two forms, a tab index) and hot keys
//! (enabled or not), the forms' tabbing on or off and their tab stops, either form the manager's current one
//! (`AddDirtyRectangle` and `Tab` log; `Tab` moves the stop on, so a later button sees the new stop). Each case
//! prints its inputs, the trace and the hot keys' and forms' state after it. The replay is the same group over this
//! module; the driver relinked over the forwarders prints the same file, and the same on two more seeds at ten
//! times the size.

use super::*;

const GOLDEN: &str = include_str!("../golden_group.txt");

enum Entity {
    Key(bool),
    Button { mode: u32, form: usize, index: u32, rect: String },
}

struct Group {
    current: usize,
    tabbing: [bool; 2],
    stops: [u32; 2],
    entities: Vec<Entity>,
    trace: String,
}

impl Group {
    fn button(&self, i: usize) -> (u32, usize, u32, String) {
        match &self.entities[i] {
            Entity::Button { mode, form, index, rect } => (*mode, *form, *index, rect.clone()),
            Entity::Key(_) => panic!("a hot key asked as a button"),
        }
    }
}

impl GroupHost for Group {
    fn count(&self) -> usize {
        self.entities.len()
    }
    fn is_button(&mut self, i: usize) -> bool {
        matches!(self.entities[i], Entity::Button { .. })
    }
    fn shown(&mut self, i: usize) -> bool {
        self.button(i).0 == 1
    }
    fn on_current_form(&mut self, i: usize) -> bool {
        self.button(i).1 == self.current
    }
    fn add_dirty(&mut self, i: usize) {
        let (_, form, _, rect) = self.button(i);
        self.trace.push_str(&format!("dirtyrect({rect},f{form}) "));
    }
    fn hide(&mut self, i: usize) {
        self.trace.push_str(&format!("o{i}.hide "));
    }
    fn set_dirty(&mut self, i: usize) {
        self.trace.push_str(&format!("o{i}.dirty(1) "));
    }
    fn tabbing(&mut self, i: usize) -> bool {
        self.tabbing[self.button(i).1]
    }
    fn is_tab_stop(&mut self, i: usize) -> bool {
        let (_, form, index, _) = self.button(i);
        self.stops[form] == index
    }
    fn tab(&mut self, i: usize) {
        let form = self.button(i).1;
        self.stops[form] = (self.stops[form] + 1) % 6;
        self.trace.push_str(&format!("f{form}.tab->{} ", self.stops[form]));
    }
    fn set_enabled(&mut self, i: usize, on: bool) {
        match &mut self.entities[i] {
            Entity::Key(e) => *e = on,
            Entity::Button { .. } => panic!("a button enabled as a hot key"),
        }
    }
}

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let mut cases = 0;
    for at in (0..lines.len()).step_by(3) {
        let t: Vec<&str> = lines[at].split(' ').collect();
        if t[0] != "group" {
            continue;
        }
        let on = t[2] == "enable";
        let n: usize = t[11].parse().unwrap();
        let mut g = Group {
            current: t[4].parse().unwrap(),
            tabbing: [t[6] == "1", t[8] == "1"],
            stops: [t[7].parse().unwrap(), t[9].parse().unwrap()],
            entities: Vec::new(),
            trace: String::new(),
        };
        let mut k = 12;
        for _ in 0..n {
            if t[k] == "key" {
                g.entities.push(Entity::Key(t[k + 1] == "1"));
                k += 2;
            } else {
                g.entities.push(Entity::Button {
                    mode: t[k + 1].parse().unwrap(),
                    form: t[k + 2].parse().unwrap(),
                    index: t[k + 3].parse().unwrap(),
                    rect: t[k + 4..k + 8].join(","),
                });
                k += 8;
            }
        }
        // mDisableCount is the C++'s assignment before the loop
        let count = if on {
            enable(&mut g);
            0
        } else {
            disable(&mut g);
            1
        };
        assert_eq!(format!("trace {}", g.trace), lines[at + 1], "{}", lines[at]);
        let keys: String = g
            .entities
            .iter()
            .filter_map(|e| match e {
                Entity::Key(on) => Some(if *on { " 1" } else { " 0" }),
                Entity::Button { .. } => None,
            })
            .collect();
        let out = format!("out count {count} stops {} {} keys{keys}", g.stops[0], g.stops[1]);
        assert_eq!(out, lines[at + 2], "{}", lines[at]);
        cases += 1;
    }
    assert_eq!(cases, 400);
}
