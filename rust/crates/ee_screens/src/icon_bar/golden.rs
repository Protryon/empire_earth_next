//! The icon bar against the real C++: a scratch driver (`build/sc27/ib`) ran the unmodified `EEUICUnitIconBar.cpp`
//! (cut from e80deffc) and the new one (forwarders and host over this module) over one fake world per case (a
//! selection of 0-44 units, some with cargo, of every family, with queued goals, hit points out of range and maximums
//! of 0; bars 24 wide (no icon width) and narrower; the mouse on and past the edges; Ctrl and Shift), each case a
//! random sequence of `Update`, `CopyData`, `DrawPolygons`, the clicks and `MouseMove` ending in `CopyData`,
//! `DrawPolygons` and the destructor, comparing every effect (draws with their rectangles' bits, messages, sounds,
//! acquires and releases, the button reference counts). `golden_icon_bar.txt` is the new side's host log: per case
//! the bar's size, per call its host ops (`op a b answer|floats in|floats out|ids`, units and buttons as stable
//! handles) and its result.

use super::*;

const GOLDEN: &str = include_str!("../golden_icon_bar.txt");

struct Op {
    op: u32,
    a: i64,
    b: i64,
    ret: i64,
    before: Vec<u32>,
    after: Vec<u32>,
    list: Vec<i32>,
}

struct Replay<'a> {
    ops: std::slice::Iter<'a, Op>,
    case: String,
}

impl IconBarHost for Replay<'_> {
    fn op(&mut self, op: u32, a: i64, b: i64, f: &mut [f32]) -> i64 {
        let want = self.ops.next().unwrap_or_else(|| panic!("{}: an extra op {op} {a} {b}", self.case));
        let a = if op == EE_SCREENS_IB_OP_SELECT_THESE {
            let ids = unsafe { std::slice::from_raw_parts(a as *const i32, b as usize) };
            assert_eq!(ids, &want.list[..], "{}: the ids", self.case);
            0
        } else {
            a
        };
        assert_eq!((op, a, b), (want.op, want.a, want.b), "{}", self.case);
        let bits: Vec<u32> = f.iter().map(|x| x.to_bits()).collect();
        assert_eq!(bits, want.before, "{}: op {op}'s floats", self.case);
        for (x, &w) in f.iter_mut().zip(&want.after) {
            *x = f32::from_bits(w);
        }
        want.ret
    }
}

fn hex_list(s: &str) -> Vec<u32> {
    s.split(' ').filter(|x| !x.is_empty()).map(|x| u32::from_str_radix(x, 16).unwrap()).collect()
}

fn parse_op(line: &str) -> Op {
    let parts: Vec<&str> = line.split('|').collect();
    let v: Vec<i64> = parts[0].split(' ').map(|x| x.parse().unwrap()).collect();
    Op {
        op: v[0] as u32,
        a: v[1],
        b: v[2],
        ret: v[3],
        before: hex_list(parts[1]),
        after: hex_list(parts[2]),
        list: parts[3].split(' ').filter(|x| !x.is_empty()).map(|x| x.parse().unwrap()).collect(),
    }
}

#[test]
fn replays_the_golden() {
    let mut bar: Option<IconBar> = None;
    let mut case = String::new();
    let mut kind = ' ';
    let mut ops: Vec<Op> = Vec::new();
    let (mut cases, mut calls) = (0, 0);
    let mut kinds = [0u32; 6];
    for line in GOLDEN.lines() {
        if let Some(rest) = line.strip_prefix("case ") {
            let v: Vec<&str> = rest.split(' ').collect();
            let (w, h) = (
                f32::from_bits(u32::from_str_radix(v[1], 16).unwrap()),
                f32::from_bits(u32::from_str_radix(v[2], 16).unwrap()),
            );
            bar = Some(IconBar::new(w, h));
            case = line.to_string();
            cases += 1;
        } else if let Some(k) = line.strip_prefix("> ") {
            kind = k.chars().next().unwrap();
            ops.clear();
        } else if let Some(r) = line.strip_prefix("= ") {
            let want: i64 = r.parse().unwrap();
            let b = bar.as_mut().unwrap();
            let mut h = Replay { ops: ops.iter(), case: format!("{case} {kind}") };
            let got = match kind {
                'U' => {
                    b.update(&mut h);
                    0
                }
                'C' => b.copy_data(&mut h),
                'D' => b.draw(&mut h),
                'L' => b.mouse_l_up(&mut h),
                'R' => b.mouse_r_up(&mut h),
                'M' => b.mouse_move(&mut h),
                'F' => {
                    b.release(&mut h);
                    0
                }
                _ => panic!("{kind}"),
            };
            assert!(h.ops.next().is_none(), "{case} {kind}: ops left");
            assert_eq!(got, want, "{case} {kind}");
            kinds["UCDLRM".find(kind).unwrap_or(5)] += 1;
            calls += 1;
        } else {
            ops.push(parse_op(line));
        }
    }
    assert!(cases >= 400, "{cases}");
    assert!(kinds.iter().all(|&k| k > 100), "{kinds:?}");
    assert!(calls > 3000, "{calls}");
}

#[test]
fn the_float_conversion_is_the_x86_one() {
    assert_eq!(cvtt(f32::NAN), i32::MIN);
    assert_eq!(cvtt(3.9), 3);
    assert_eq!(cvtt(-3.9), -3);
    assert_eq!(cvtt(3e9), i32::MIN);
}

#[test]
fn an_icon_is_a_twenty_fifth_of_the_bar_less_24() {
    let bar = IconBar::new(524.0, 40.0);
    assert_eq!(bar.icon, Rect { left: 0.0, top: 0.0, right: 500.0 * 0.04, bottom: 40.0 });
}

/// The air base's scramble asks each family's own queue (the golden reaches the atomic bomber's rarely).
#[test]
fn an_air_base_asks_the_family_s_queue() {
    struct Base(i64, Vec<(u32, i64, i64)>);
    impl IconBarHost for Base {
        fn op(&mut self, op: u32, a: i64, b: i64, f: &mut [f32]) -> i64 {
            self.1.push((op, a, b));
            match op {
                EE_SCREENS_IB_OP_CONTAINS | EE_SCREENS_IB_OP_GROUP_COUNT | EE_SCREENS_IB_OP_NO_GOALS => 1,
                EE_SCREENS_IB_OP_GROUP_UNIT => 10,
                EE_SCREENS_IB_OP_CARGO_COUNT => 3,
                EE_SCREENS_IB_OP_CARGO => 20,
                EE_SCREENS_IB_OP_CLASS if a == 10 => EE_SCREENS_IB_CLASS_AIR_BASE,
                EE_SCREENS_IB_OP_CLASS => self.0 << 8,
                EE_SCREENS_IB_OP_RECT => {
                    f.copy_from_slice(&[0.0, 0.0, 500.0, 40.0]);
                    0
                }
                _ => 0,
            }
        }
    }
    for (family, queue) in [(5, 1), (0x39, 1), (0x14, 2), (0x2a, 3)] {
        let mut bar = IconBar::new(500.0, 40.0);
        bar.buttons = vec![1, 2, 3];
        bar.show_cargo = true;
        bar.spacing = 1.0;
        let mut h = Base(family, Vec::new());
        bar.mouse_l_up(&mut h);
        assert!(h.1.contains(&(EE_SCREENS_IB_OP_QUEUED_GOAL, 10, queue)), "{family}");
        assert!(h.1.iter().any(|o| o.0 == EE_SCREENS_IB_OP_SCRAMBLE), "{family}");
    }
}
