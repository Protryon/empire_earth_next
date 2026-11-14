//! The portrait against the real C++: a scratch driver (`build/sc27/pt`) ran the unmodified `EEUICPortrait`'s
//! `GetBarRectangles`, `DrawPolygons`, `GetHelpString`, `MouseLButtonDown` and `Update` (cut from e80deffc; `SetUnit` from
//! 2cac466f, round 28's `build/sc28/pt`) and the new
//! ones (forwarders and host over this module) over one fake world per case (the shown unit alive or not, on the map
//! or not, its complex unit missing, the local player's or not; production times below 0, research of no technology
//! or of a missing node, nodes that make units with an upgraded class or not and build times of 0; improvements
//! researched or not, at level 1 or later, world times of 0; levels past the maximum; portraits of width 0; the mouse
//! on the bars' edges and the middle line), each case a sequence of 1-5 calls, comparing every effect and the
//! portrait's members after. `golden_portrait.txt` is the new side's host log: per call its kind and per op
//! `op a b answer|floats in|floats out|text` (units and nodes as stable handles).

use super::*;

const GOLDEN: &str = include_str!("../golden_portrait.txt");

struct Op {
    op: u32,
    a: i64,
    b: i64,
    ret: i64,
    before: Vec<u32>,
    after: Vec<u32>,
    text: String,
}

struct Replay<'a> {
    ops: std::slice::Iter<'a, Op>,
    case: String,
}

impl PortraitHost for Replay<'_> {
    fn op(&mut self, op: u32, a: i64, b: i64, f: &mut [f32]) -> i64 {
        let want = self.ops.next().unwrap_or_else(|| panic!("{}: an extra op {op} {a} {b}", self.case));
        let a = if op == EE_SCREENS_PT_OP_TEXT {
            let bytes = unsafe { std::slice::from_raw_parts(a as *const u8, b as usize) };
            assert_eq!(bytes, want.text.as_bytes(), "{}: the text", self.case);
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
    let parts: Vec<&str> = line.splitn(4, '|').collect();
    let v: Vec<i64> = parts[0].split(' ').map(|x| x.parse().unwrap()).collect();
    Op {
        op: v[0] as u32,
        a: v[1],
        b: v[2],
        ret: v[3],
        before: hex_list(parts[1]),
        after: hex_list(parts[2]),
        text: parts[3].to_string(),
    }
}

#[test]
fn replays_the_golden() {
    let mut case = String::new();
    let mut kind = ' ';
    let mut ops: Vec<Op> = Vec::new();
    let (mut cases, mut calls) = (0, 0);
    let mut kinds = [0u32; 5];
    let mut arg = 0i64;
    for line in GOLDEN.lines() {
        if line.starts_with("case ") {
            case = line.to_string();
            cases += 1;
        } else if let Some(k) = line.strip_prefix("> ") {
            kind = k.chars().next().unwrap();
            arg = k.get(2..).map_or(0, |a| a.parse().unwrap());
            ops.clear();
        } else if let Some(r) = line.strip_prefix("= ") {
            let want: i64 = r.parse().unwrap();
            let mut h = Replay { ops: ops.iter(), case: format!("{case} {kind}") };
            let got = match kind {
                'U' => {
                    update(&mut h);
                    0
                }
                'D' => draw(&mut h),
                'H' => help(&mut h) as i64,
                'M' => mouse_down(&mut h),
                'S' => {
                    set_unit(&mut h, arg);
                    0
                }
                _ => panic!("{kind}"),
            };
            assert!(h.ops.next().is_none(), "{case} {kind}: ops left");
            assert_eq!(got, want, "{case} {kind}");
            kinds["UDHMS".find(kind).unwrap()] += 1;
            calls += 1;
        } else {
            ops.push(parse_op(line));
        }
    }
    assert!(cases >= 1000, "{cases}");
    assert!(kinds.iter().all(|&k| k > 300), "{kinds:?}");
    assert!(calls > 2500, "{calls}");
}

#[test]
fn both_bars_split_the_rectangle() {
    let s = State { level: 0, first: true, second: true };
    let (first, second) = bar_rectangles([0.0, 10.0, 40.0, 30.0], s);
    assert_eq!(second, [0.0, 10.0, 40.0, 20.0]);
    assert_eq!(first, [0.0, 11.0, 40.0, 30.0]);
    let (first, second) = bar_rectangles([0.0, 10.0, 40.0, 30.0], State { second: false, ..s });
    assert_eq!((first, second), ([0.0, 10.0, 40.0, 30.0], [0.0, 10.0, 40.0, 30.0]));
}
