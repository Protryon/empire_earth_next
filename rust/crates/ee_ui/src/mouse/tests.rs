//! The mouse dispatch against the real C++ (`golden_mouse.txt`): a scratch driver (`build/ui_mouse_golden`) ran the
//! unmodified `UIForm::UpdateMouse` and `UIControl::UpdateMouse` (`UIForm.cpp`, `UIControl.cpp`) over 1,500 random
//! worlds: a form with two fake controls whose handlers log, return random results and may capture the mouse,
//! release it or capture the other control (`MouseLButtonDoublePressed` either the inline default or its own),
//! the captured and mouse-over controls, UIManager's buttons and wheel (present or NULL, a scroll amount), the
//! mouse's six entities (the wheel and the middle button sometimes NULL) and the form's eleven events present or
//! not. Each case prints its inputs, the trace of every entity read and outside call, and the result with the
//! capture and button-down members after it. The replay below is the same world over this module; the driver
//! relinked over the forwarders prints the same file, and the same on two more seeds at ten times the size.

use super::*;

const GOLDEN: &str = include_str!("../golden_mouse.txt");

#[derive(Clone, Copy, Default)]
struct Entity {
    value: c_long,
    double: bool,
    pressed: bool,
    unpressed: bool,
}

#[derive(Clone, Copy, Default)]
struct Ctl {
    left: bool,
    right: bool,
    default_double: bool,
    result: [c_long; 7],
    effect: [i64; 7],
}

const HANDLERS: [&str; 7] = ["ldbl", "lup", "ldown", "rup", "rdown", "move", "wheel"];
const MOUSE: [&str; 6] = ["mx", "my", "mz", "ml", "mr", "mm"];

struct World {
    captured: Option<usize>,
    over: Option<usize>,
    wheel: bool,
    amount: c_long,
    buttons: [Entity; 2],
    mouse: [Entity; 6],
    mouse_null: [bool; 6],
    events: [Option<c_long>; 11],
    controls: [Ctl; 2],
    at: (String, String),
    trace: String,
}

impl World {
    fn tok(&mut self, t: String) {
        self.trace.push_str(&t);
        self.trace.push(' ');
    }
    fn ask(&mut self, name: &str, e: Entity, q: u8) -> bool {
        match q {
            b'v' => {
                self.tok(format!("{name}.v={}", e.value));
                e.value != 0
            }
            b'd' => {
                self.tok(format!("{name}.dp={}", e.double as i32));
                e.double
            }
            b'p' => {
                self.tok(format!("{name}.jp={}", e.pressed as i32));
                e.pressed
            }
            _ => {
                self.tok(format!("{name}.ju={}", e.unpressed as i32));
                e.unpressed
            }
        }
    }
    fn handler(&mut self, c: usize, h: usize) -> c_long {
        let ctl = self.controls[c];
        self.tok(format!("{}.{}={}/{}", ["A", "B"][c], HANDLERS[h], ctl.result[h], ctl.effect[h]));
        match ctl.effect[h] {
            1 => self.captured = Some(c),
            2 => self.captured = None,
            3 => self.captured = Some(1 - c),
            _ => {}
        }
        ctl.result[h]
    }
}

struct CtlHost<'a>(&'a mut World, usize);

impl ControlMouseHost for CtlHost<'_> {
    fn button(&mut self, right: bool, q: ButtonQuery) -> bool {
        let e = self.0.buttons[right as usize];
        let name = if right { "rb" } else { "lb" };
        let q = match q {
            ButtonQuery::JustDoublePressed => b'd',
            ButtonQuery::JustUnpressed => b'u',
            ButtonQuery::Value => b'v',
            ButtonQuery::JustPressed => b'p',
        };
        self.0.ask(name, e, q)
    }
    fn down(&self, right: bool) -> bool {
        let c = &self.0.controls[self.1];
        if right {
            c.right
        } else {
            c.left
        }
    }
    fn set_down(&mut self, right: bool, down: bool) {
        let c = &mut self.0.controls[self.1];
        if right {
            c.right = down;
        } else {
            c.left = down;
        }
    }
    fn call(&mut self, call: ControlMouseCall) -> c_long {
        let c = self.1;
        match call {
            ControlMouseCall::LeftDouble => {
                if self.0.controls[c].default_double {
                    self.0.tok(format!("{}.ldbl-default", ["A", "B"][c]));
                    self.0.controls[c].left = true;
                    self.0.handler(c, 2)
                } else {
                    self.0.handler(c, 0)
                }
            }
            ControlMouseCall::LeftUp => self.0.handler(c, 1),
            ControlMouseCall::LeftDown => self.0.handler(c, 2),
            ControlMouseCall::RightUp => self.0.handler(c, 3),
            ControlMouseCall::RightDown => self.0.handler(c, 4),
            ControlMouseCall::Move => {
                let t = format!("at={},{}", self.0.at.0, self.0.at.1);
                self.0.tok(t);
                self.0.handler(c, 5)
            }
        }
    }
}

impl FormMouseHost for World {
    fn captured(&self) -> Option<Control> {
        self.captured
    }
    fn mouse_over(&self) -> Option<Control> {
        self.over
    }
    fn control_update(&mut self, c: Control) -> c_long {
        control_update_mouse(&mut CtlHost(self, c))
    }
    fn wheel(&self) -> bool {
        self.wheel
    }
    fn wheel_amount(&mut self) -> c_long {
        self.tok(format!("amount={}", self.amount));
        self.amount
    }
    fn wheel_turned(&mut self, c: Control, amount: c_long) -> c_long {
        self.tok(format!("delta={amount}"));
        self.handler(c, 6)
    }
    fn entity(&mut self, e: MouseEntity) -> bool {
        self.tok(format!("get{}", e as i32));
        !self.mouse_null[e as usize]
    }
    fn ask(&mut self, e: MouseEntity, q: EntityQuery) -> bool {
        let i = e as usize;
        assert!(!self.mouse_null[i], "a NULL entity read");
        let q = match q {
            EntityQuery::Value => b'v',
            EntityQuery::JustPressed => b'p',
            EntityQuery::JustUnpressed => b'u',
            EntityQuery::JustDoublePressed => b'd',
        };
        World::ask(self, MOUSE[i], self.mouse[i], q)
    }
    fn has_event(&self, ev: FormMouseEvent) -> bool {
        self.events[ev as usize].is_some()
    }
    fn process(&mut self, ev: FormMouseEvent) -> c_long {
        let r = self.events[ev as usize].unwrap();
        self.tok(format!("ev{}={r}", ev as i32));
        r
    }
}

fn index(v: i64) -> Option<usize> {
    (v >= 0).then_some(v as usize)
}

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    assert_eq!(lines.len() % 4, 0);
    let mut cases = 0;
    for chunk in lines.chunks(4) {
        let mut n = chunk[0].split(' ').skip(2).map(|v| v.parse::<i64>().unwrap());
        let mut next = || n.next().unwrap();
        let kind = next();
        let captured = index(next());
        let over = index(next());
        let wheel = next() != 0;
        let amount = next() as c_long;
        let mut entity = |with_null: bool| {
            let null = with_null && next() != 0;
            let value = next() as c_long;
            let double = next() != 0;
            let pressed = next() != 0;
            let unpressed = next() != 0;
            (null, Entity { value, double, pressed, unpressed })
        };
        let buttons = [entity(false).1, entity(false).1];
        let mut mouse = [Entity::default(); 6];
        let mut mouse_null = [false; 6];
        for i in 0..6 {
            (mouse_null[i], mouse[i]) = entity(true);
        }
        let mut events = [None; 11];
        for e in &mut events {
            let present = next() != 0;
            let r = next() as c_long;
            *e = present.then_some(r);
        }
        let mut controls = [Ctl::default(); 2];
        for c in &mut controls {
            c.left = next() != 0;
            c.right = next() != 0;
            c.default_double = next() != 0;
            for h in 0..7 {
                c.result[h] = next() as c_long;
                c.effect[h] = next();
            }
        }
        let at: Vec<&str> = chunk[1].split(' ').collect();
        let mut w = World {
            captured,
            over,
            wheel,
            amount,
            buttons,
            mouse,
            mouse_null,
            events,
            controls,
            at: (at[1].to_string(), at[2].to_string()),
            trace: String::new(),
        };
        let result = if kind == 1 { control_update_mouse(&mut CtlHost(&mut w, 0)) } else { form_update_mouse(&mut w) };
        assert_eq!(format!("trace {}", w.trace), chunk[2], "{}", chunk[0]);
        let c = &w.controls;
        let out = format!(
            "out {result} cap {} A {} {} B {} {}",
            w.captured.map_or(-1, |c| c as i64),
            c[0].left as i32,
            c[0].right as i32,
            c[1].left as i32,
            c[1].right as i32
        );
        assert_eq!(out, chunk[3], "{}", chunk[0]);
        cases += 1;
    }
    assert_eq!(cases, 1500);
}

/// A world that answers every question with a fixed table and logs the calls, for the cases spelled out below.
#[derive(Default)]
struct Plain {
    double: bool,
    unpressed: bool,
    value: bool,
    pressed: bool,
    left: bool,
    right: bool,
    calls: Vec<ControlMouseCall>,
}

impl ControlMouseHost for Plain {
    fn button(&mut self, right: bool, q: ButtonQuery) -> bool {
        if right {
            return false;
        }
        match q {
            ButtonQuery::JustDoublePressed => self.double,
            ButtonQuery::JustUnpressed => self.unpressed,
            ButtonQuery::Value => self.value,
            ButtonQuery::JustPressed => self.pressed,
        }
    }
    fn down(&self, right: bool) -> bool {
        if right {
            self.right
        } else {
            self.left
        }
    }
    fn set_down(&mut self, right: bool, down: bool) {
        if right {
            self.right = down;
        } else {
            self.left = down;
        }
    }
    fn call(&mut self, c: ControlMouseCall) -> c_long {
        self.calls.push(c);
        if c == ControlMouseCall::Move {
            7
        } else {
            -1
        }
    }
}

#[test]
fn a_control_sees_a_release_when_the_button_is_up() {
    // the control thought the button down and it is up without a release event: still a release
    let mut p = Plain { left: true, ..Plain::default() };
    assert_eq!(control_update_mouse(&mut p), 7);
    assert_eq!(p.calls, [ControlMouseCall::LeftUp, ControlMouseCall::Move]);
    assert!(!p.left);
    // a double click wins over the release and leaves the member as the handler left it
    let mut p = Plain { left: true, double: true, ..Plain::default() };
    control_update_mouse(&mut p);
    assert_eq!(p.calls, [ControlMouseCall::LeftDouble, ControlMouseCall::Move]);
    assert!(p.left);
    // a press while the control thinks the button held is a press again
    let mut p = Plain { left: true, value: true, pressed: true, ..Plain::default() };
    control_update_mouse(&mut p);
    assert_eq!(p.calls, [ControlMouseCall::LeftDown, ControlMouseCall::Move]);
}
