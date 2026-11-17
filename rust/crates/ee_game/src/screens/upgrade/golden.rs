//! The improvement button against the real C++: a scratch driver (`build/sc27/ub`) ran the unmodified
//! `EEUIBUpgrade`'s `ButtonUp`, `GetCanUpgrade`, `GetDisabled`, `GetHelpString`, `MouseMove`, `MouseRButtonUp` and
//! `UpdateCost` (cut from e80deffc) and the new ones (forwarders and host over this module) over one fake world per
//! case (a selection or none, units of three classes with step factors of 0, below 0 and above, players that can buy
//! or not with a reason or an empty one, maxed improvements, the help-only and repair improvements, the scenario
//! editor's first unit or none, missing tech nodes and class entries, refusal intervals of 0, a help label or none,
//! cost labels), each case a sequence of 1-6 calls, comparing every effect and the button's fields after.
//! `golden_upgrade.txt` is the new side's host log: per call its kind and per op `op a b c answer|bytes` (units and
//! nodes as stable handles; the nested tooltip of `MouseMove` is the host's and not listed).

use super::*;

const GOLDEN: &str = include_str!("../golden_upgrade.txt");

struct Op {
    op: u32,
    a: i64,
    b: i64,
    c: i64,
    ret: i64,
    bytes: String,
}

struct Replay<'a> {
    ops: std::slice::Iter<'a, Op>,
    case: String,
}

impl UpgradeHost for Replay<'_> {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        let want = self.ops.next().unwrap_or_else(|| panic!("{}: an extra op {op} {a} {b} {c}", self.case));
        let a = if op == EE_SCREENS_UP_OP_OUT_BYTES {
            let bytes = unsafe { std::slice::from_raw_parts(a as *const u8, b as usize) };
            assert_eq!(bytes, want.bytes.as_bytes(), "{}: the bytes", self.case);
            0
        } else {
            a
        };
        assert_eq!((op, a, b, c), (want.op, want.a, want.b, want.c), "{}", self.case);
        want.ret
    }
}

fn parse_op(line: &str) -> Op {
    let (nums, bytes) = line.split_once('|').unwrap();
    let v: Vec<i64> = nums.split(' ').map(|x| x.parse().unwrap()).collect();
    Op { op: v[0] as u32, a: v[1], b: v[2], c: v[3], ret: v[4], bytes: bytes.to_string() }
}

#[test]
fn replays_the_golden() {
    let mut case = String::new();
    let mut kind = ' ';
    let mut ops: Vec<Op> = Vec::new();
    let (mut cases, mut calls) = (0, 0);
    let mut kinds = [0u32; 7];
    for line in GOLDEN.lines() {
        if line.starts_with("case ") {
            case = line.to_string();
            cases += 1;
        } else if let Some(k) = line.strip_prefix("> ") {
            kind = k.chars().next().unwrap();
            ops.clear();
        } else if let Some(r) = line.strip_prefix("= ") {
            let want: i64 = r.parse().unwrap();
            let mut h = Replay { ops: ops.iter(), case: format!("{case} {kind}") };
            let got = match kind {
                'B' => {
                    button_up(&mut h);
                    0
                }
                'C' => can_upgrade(&mut h).0 as i64,
                'X' => disabled(&mut h) as i64,
                'H' => help(&mut h) as i64,
                'M' => mouse_move(&mut h),
                'R' => mouse_r_up(&mut h),
                'U' => {
                    update_cost(&mut h);
                    0
                }
                _ => panic!("{kind}"),
            };
            assert!(h.ops.next().is_none(), "{case} {kind}: ops left");
            assert_eq!(got, want, "{case} {kind}");
            kinds["BCXHMRU".find(kind).unwrap()] += 1;
            calls += 1;
        } else {
            ops.push(parse_op(line));
        }
    }
    assert!(cases >= 1000, "{cases}");
    assert!(kinds.iter().all(|&k| k > 150), "{kinds:?}");
    assert!(calls > 3000, "{calls}");
}

/// The greyed-out tint: none and no border; the normal: 0.15 and a 0.45 border one pixel thick.
#[test]
fn the_tint_follows_the_greyed_out_state() {
    struct Tint(Vec<(u32, i64)>, bool);
    impl UpgradeHost for Tint {
        fn op(&mut self, op: u32, a: i64, _: i64, _: i64) -> i64 {
            self.0.push((op, a));
            match op {
                EE_SCREENS_UP_OP_FIELD if a == EE_SCREENS_UP_FIELD_EDITOR => 1,
                EE_SCREENS_UP_OP_FIELD if a == EE_SCREENS_UP_FIELD_DISABLED => self.1 as i64,
                EE_SCREENS_UP_OP_EDITOR_UNIT => 7,
                EE_SCREENS_UP_OP_CAN_RAISE => 1,
                _ => 0,
            }
        }
    }
    let mut h = Tint(Vec::new(), true);
    update_cost(&mut h);
    assert!(h.0.contains(&(EE_SCREENS_UP_OP_BG_OPACITY, 0.15f32.to_bits() as i64)));
    assert!(h.0.contains(&(EE_SCREENS_UP_OP_BORDER_OPACITY, (0.15f32 * 3.0).to_bits() as i64)));
    assert!(h.0.contains(&(EE_SCREENS_UP_OP_BORDER_THICKNESS, 1)));
}
