use super::*;

#[derive(Clone, Default)]
struct Unit {
    kind: u32,
    attack: bool,
    built: bool,
    on_map: bool,
    cargo: u32,
    unit_type: i32,
    air_base: bool,
    crusader: bool,
    priest: bool,
    complex: bool,
    pos: [f32; 3],
    min_range: f32,
    can_attack: bool,
    convert: bool,
    shields: bool,
}

/// One golden case: what the original's world answers (`build/sc21/cursor/driver.cpp`'s "W" line).
#[derive(Clone, Default)]
struct World {
    mouse_ok: bool,
    in_view: bool,
    pick: bool,
    seen: bool,
    targetable: bool,
    heal: bool,
    flags: u32,
    first: i32,
    gather: [u32; 2],
    medic: bool,
    t_kind: u32,
    t_disabled: bool,
    t_owner: i32,
    t_hp: i32,
    t_max_hp: i32,
    t_attack: bool,
    t_type: i32,
    t_tower: bool,
    t_category: i32,
    t_enterable: bool,
    t_complex: bool,
    aim: [f32; 3],
    units: Vec<(Unit, Vec<Unit>)>,
    result: i32,
    trace: String,
}

struct Host<'a> {
    w: &'a World,
    trace: String,
}

impl CursorHost for Host<'_> {
    fn op(&mut self, op: u32, a: u32, b: u32, f: &mut [f32; 3]) -> i32 {
        let w = self.w;
        let sel = |i: u32| &w.units[i as usize].0;
        let cargo = |i: u32, j: u32| &w.units[i as usize].1[j as usize];
        let mut tr = |s: String| {
            self.trace.push_str(&s);
            self.trace.push(';');
        };
        match op {
            EE_SCREENS_CURSOR_OP_MOUSE_OK => w.mouse_ok as i32,
            EE_SCREENS_CURSOR_OP_FIRST => w.first,
            EE_SCREENS_CURSOR_OP_FLAGS => w.flags as i32,
            EE_SCREENS_CURSOR_OP_IN_VIEW => w.in_view as i32,
            EE_SCREENS_CURSOR_OP_PICK => {
                tr(format!("pick {a}"));
                w.pick as i32
            }
            EE_SCREENS_CURSOR_OP_RELEASE => {
                tr("release".into());
                0
            }
            EE_SCREENS_CURSOR_OP_SEEN => {
                tr("seen".into());
                w.seen as i32
            }
            EE_SCREENS_CURSOR_OP_T_KIND => w.t_kind as i32,
            EE_SCREENS_CURSOR_OP_T_DISABLED => w.t_disabled as i32,
            EE_SCREENS_CURSOR_OP_FIRST_MEDIC => w.medic as i32,
            EE_SCREENS_CURSOR_OP_T_TARGETABLE => {
                tr("targetable".into());
                w.targetable as i32
            }
            EE_SCREENS_CURSOR_OP_T_OWNER => w.t_owner,
            EE_SCREENS_CURSOR_OP_T_HP => w.t_hp,
            EE_SCREENS_CURSOR_OP_T_MAX_HP => w.t_max_hp,
            EE_SCREENS_CURSOR_OP_T_ATTACK => w.t_attack as i32,
            EE_SCREENS_CURSOR_OP_T_TYPE => w.t_type,
            EE_SCREENS_CURSOR_OP_T_TOWER => w.t_tower as i32,
            EE_SCREENS_CURSOR_OP_T_CATEGORY => w.t_category,
            EE_SCREENS_CURSOR_OP_T_ENTERABLE => w.t_enterable as i32,
            EE_SCREENS_CURSOR_OP_T_COMPLEX => w.t_complex as i32,
            EE_SCREENS_CURSOR_OP_AIM => {
                *f = w.aim;
                0
            }
            EE_SCREENS_CURSOR_OP_CAN_GATHER => {
                // the group's FindUnitWithFlags: the first selected unit with one of the flags
                let k = w.units.iter().position(|u| u.0.kind & a != 0).expect("a unit with the flags");
                tr(format!("gather {a} {k}"));
                (if a == 1 { w.gather[0] } else { w.gather[1] }) as i32
            }
            EE_SCREENS_CURSOR_OP_CAN_HEAL => {
                tr("heal".into());
                w.heal as i32
            }
            EE_SCREENS_CURSOR_OP_SEL_COUNT => w.units.len() as i32,
            EE_SCREENS_CURSOR_OP_SEL_KIND => sel(a).kind as i32,
            EE_SCREENS_CURSOR_OP_SEL_ATTACK => sel(a).attack as i32,
            EE_SCREENS_CURSOR_OP_SEL_BUILT => sel(a).built as i32,
            EE_SCREENS_CURSOR_OP_SEL_ON_MAP => sel(a).on_map as i32,
            EE_SCREENS_CURSOR_OP_SEL_CARGO => sel(a).cargo as i32,
            EE_SCREENS_CURSOR_OP_SEL_TYPE => sel(a).unit_type,
            EE_SCREENS_CURSOR_OP_SEL_AIR_BASE => sel(a).air_base as i32,
            EE_SCREENS_CURSOR_OP_SEL_CRUSADER => sel(a).crusader as i32,
            EE_SCREENS_CURSOR_OP_SEL_PRIEST => sel(a).priest as i32,
            EE_SCREENS_CURSOR_OP_SEL_COMPLEX => sel(a).complex as i32,
            EE_SCREENS_CURSOR_OP_SEL_POSITION => {
                *f = sel(a).pos;
                0
            }
            EE_SCREENS_CURSOR_OP_SEL_MIN_RANGE => {
                f[0] = sel(a).min_range;
                0
            }
            EE_SCREENS_CURSOR_OP_SEL_CAN_ATTACK => {
                tr(format!("attack {a}"));
                sel(a).can_attack as i32
            }
            EE_SCREENS_CURSOR_OP_SEL_CONVERT => {
                tr(format!("convert {a} {b}"));
                let u = sel(a);
                u.convert as i32 | (((b != 0 || u.shields) as i32) << 1)
            }
            EE_SCREENS_CURSOR_OP_CARGO_CAN_ATTACK => {
                tr(format!("cattack {a} {b}"));
                cargo(a, b).can_attack as i32
            }
            EE_SCREENS_CURSOR_OP_CARGO_ATTACK => cargo(a, b).attack as i32,
            EE_SCREENS_CURSOR_OP_CARGO_POSITION => {
                *f = cargo(a, b).pos;
                0
            }
            EE_SCREENS_CURSOR_OP_CARGO_MIN_RANGE => {
                f[0] = cargo(a, b).min_range;
                0
            }
            _ => panic!("op {op}"),
        }
    }
}

struct Tok<'a>(std::str::SplitWhitespace<'a>);

impl<'a> Tok<'a> {
    fn n(&mut self) -> &'a str {
        self.0.next().expect("a token")
    }
}

fn parse(line: &str) -> World {
    // the trace after the result holds spaces: "... R <result> <trace>"
    let (head, tail) = line.split_once(" R ").expect("a result");
    let (result, trace) = tail.split_once(' ').unwrap_or((tail, ""));
    let mut t = Tok(head.split_whitespace());
    fn b(s: &str) -> bool {
        s != "0"
    }
    fn hex(s: &str) -> u32 {
        u32::from_str_radix(s, 16).unwrap()
    }
    fn fl(s: &str) -> f32 {
        f32::from_bits(hex(s))
    }
    fn int(s: &str) -> i32 {
        s.parse().unwrap()
    }
    let mut w = World::default();
    assert_eq!(t.n(), "W");
    w.mouse_ok = b(t.n());
    w.in_view = b(t.n());
    w.pick = b(t.n());
    w.seen = b(t.n());
    w.targetable = b(t.n());
    w.heal = b(t.n());
    w.flags = hex(t.n());
    w.first = int(t.n());
    w.gather = [int(t.n()) as u32, int(t.n()) as u32];
    w.medic = b(t.n());
    assert_eq!(t.n(), "T");
    w.t_kind = hex(t.n());
    w.t_disabled = b(t.n());
    w.t_owner = int(t.n());
    w.t_hp = int(t.n());
    w.t_max_hp = int(t.n());
    w.t_attack = b(t.n());
    w.t_type = int(t.n());
    w.t_tower = b(t.n());
    w.t_category = int(t.n());
    w.t_enterable = b(t.n());
    w.t_complex = b(t.n());
    w.aim = [fl(t.n()), fl(t.n()), fl(t.n())];
    assert_eq!(t.n(), "S");
    let n = int(t.n());
    while let Some(tag) = t.0.next() {
        let u = Unit {
            kind: hex(t.n()),
            attack: b(t.n()),
            built: b(t.n()),
            on_map: b(t.n()),
            cargo: int(t.n()) as u32,
            unit_type: int(t.n()),
            air_base: b(t.n()),
            crusader: b(t.n()),
            priest: b(t.n()),
            complex: b(t.n()),
            pos: [fl(t.n()), fl(t.n()), fl(t.n())],
            min_range: fl(t.n()),
            can_attack: b(t.n()),
            convert: b(t.n()),
            shields: b(t.n()),
        };
        if tag == "U" {
            w.units.push((u, Vec::new()));
        } else {
            w.units.last_mut().unwrap().1.push(u);
        }
    }
    assert_eq!(w.units.len(), n as usize);
    w.result = int(result);
    w.trace = trace.to_string();
    w
}

fn run(w: &World) -> (i32, String) {
    let mut h = Host { w, trace: String::new() };
    let r = cursor_kind(&mut h);
    (r, h.trace)
}

const GOLDEN: &str = include_str!("../golden_cursor.txt");

/// `build/sc21/cursor/driver.cpp 1 N golden`: the original's cursor kind and its predicate calls, per world.
#[test]
fn golden_cursor() {
    let mut n = 0;
    for (k, line) in GOLDEN.lines().enumerate() {
        let w = parse(line);
        let (r, trace) = run(&w);
        assert_eq!((r, trace.as_str()), (w.result, w.trace.as_str()), "case {k}: {line}");
        n += 1;
    }
    assert!(n >= 2000);
}

#[test]
fn golden_reaches_every_kind() {
    let mut seen = std::collections::BTreeSet::new();
    for line in GOLDEN.lines() {
        seen.insert(parse(line).result);
    }
    for kind in [0, 1, 2, 3, 4, 5, 7, 10, 0xb, 0x10, 0x12, 0x14] {
        assert!(seen.contains(&kind), "kind {kind:#x} never in the golden");
    }
}

#[test]
fn can_enter_unit_pairs() {
    assert!(can_enter_unit(0x400, 0x80));
    assert!(can_enter_unit(0x800, 0x100));
    assert!(can_enter_unit(0x1000, 0x200));
    assert!(can_enter_unit(0x20000, 0x10000));
    assert!(can_enter_unit(0x2000, 0x4000));
    assert!(!can_enter_unit(0x400, 0x100));
    assert!(!can_enter_unit(0, 0x4000 | 0x80));
    assert!(!can_enter_unit(0x3fc00, 0));
}

fn soldier_world() -> World {
    let unit = Unit {
        kind: 2,
        attack: true,
        on_map: true,
        complex: true,
        can_attack: true,
        min_range: 2.0,
        ..Unit::default()
    };
    World {
        mouse_ok: true,
        in_view: true,
        pick: true,
        targetable: true,
        flags: 2,
        first: 1,
        t_kind: 2,
        t_owner: 1,
        t_hp: 10,
        t_max_hp: 10,
        t_complex: true,
        aim: [2.0, 0.0, 0.0],
        units: vec![(unit, Vec::new())],
        ..World::default()
    }
}

#[test]
fn military_over_an_enemy_by_range() {
    let mut w = soldier_world();
    // exactly the minimum range away: in range (>=)
    assert_eq!(run(&w), (1, "pick 0;seen;targetable;attack 0;release;".to_string()));
    w.aim = [1.5, 0.0, 0.0];
    assert_eq!(run(&w).0, 3);
    // unseen (allowed dead) with long range: 7
    w.aim = [3.0, 0.0, 0.0];
    w.flags = 0x8002;
    w.seen = false;
    assert_eq!(run(&w).0, 7);
    w.seen = true;
    assert_eq!(run(&w).0, 1);
}

#[test]
fn nothing_without_a_live_first_or_outside_the_view() {
    let mut w = soldier_world();
    w.first = 2;
    assert_eq!(run(&w), (0, String::new()));
    let mut w = soldier_world();
    w.in_view = false;
    assert_eq!(run(&w), (0, String::new()));
    let mut w = soldier_world();
    w.t_disabled = true;
    assert_eq!(run(&w), (2, "pick 0;seen;release;".to_string()));
}
