//! The HUD controls against the real C++: a scratch driver (`build/sc29/drv`) ran the unmodified `EEUICPing`,
//! `EEUICChatLine`, `EEUICSelectUnits` and `EEUICCommandUnits` units (cut from 00754e4f) and the new ones (forwarders,
//! `EEUIRustControl`'s host, this crate) over one fake world per case (the world rolled before each call: the mouse in
//! and out of the game view, modifiers, timers running out, picked units dead, alive, the player's or not, groups
//! of 0-6 units of every category, points hit or missed, perspective and orthographic views), each case the control
//! made, 1-25 random calls and the destructor, comparing every effect and the members after each call.
//! `golden_hud_ctl.txt` is the new side's log: per case its kind and arguments, per call its outermost host ops
//! (`op a b answer|floats in|floats out`, pointers as `#k`), its result and the record after it.

use super::HudHost;

const GOLDEN: &str = include_str!("../golden_hud_ctl.txt");

/// One logged op.
pub struct Op {
    pub op: u32,
    pub a: i64,
    pub b: i64,
    pub ret: i64,
    pub before: Vec<u32>,
    pub after: Vec<u32>,
}

/// One call: its line (`> X args`), its ops, result and the record after it.
pub struct Call {
    pub what: Vec<String>,
    pub ops: Vec<Op>,
    pub result: i64,
    pub record: String,
}

/// One case of a kind (0 ping, 1 chat line, 2 select, 3 command): the case line's floats, the constructor's ops and
/// record, the calls.
pub struct Case {
    pub head: Vec<f32>,
    pub init: Call,
    pub calls: Vec<Call>,
}

/// A logged value: a pointer `#k` as `0x7000_0000 + k`.
pub fn value(s: &str) -> i64 {
    match s.strip_prefix('#') {
        Some(k) => 0x7000_0000 + k.parse::<i64>().unwrap(),
        None => s.parse().unwrap(),
    }
}

pub fn hex_f(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

fn hex_list(s: &str) -> Vec<u32> {
    s.split(' ').filter(|x| !x.is_empty()).map(|x| u32::from_str_radix(x, 16).unwrap()).collect()
}

fn parse_op(line: &str) -> Op {
    let parts: Vec<&str> = line.split('|').collect();
    let v: Vec<&str> = parts[0].split(' ').collect();
    Op {
        op: v[0].parse().unwrap(),
        a: value(v[1]),
        b: value(v[2]),
        ret: value(v[3]),
        before: hex_list(parts[1]),
        after: hex_list(parts[2]),
    }
}

/// The golden's cases of one kind.
pub fn cases(kind: u32) -> Vec<Case> {
    cases_in(GOLDEN, kind)
}

/// The scenario editor's controls against the real C++ (actions round 2, `build/ga2/hud` in its worktree: the same
/// driver, the unmodified `EESEUICCommandUnits` and `EESEUICTriggerSelectUnits` units of 32a9ab6a over the
/// original base classes against `EEUICCommandUnits` / `EEUICSelectUnits` with the editor's flags): kinds 4 (the
/// editor's command) and 5 (the trigger tab's selection).
const EDITOR_GOLDEN: &str = include_str!("../golden_hud_editor.txt");

/// The editor golden's cases of one kind.
pub fn editor_cases(kind: u32) -> Vec<Case> {
    cases_in(EDITOR_GOLDEN, kind)
}

/// A golden text's cases of one kind.
pub fn cases_in(text: &str, kind: u32) -> Vec<Case> {
    let mut out = Vec::new();
    let mut lines = text.lines().peekable();
    while let Some(line) = lines.next() {
        let Some(rest) = line.strip_prefix("case ") else { panic!("not a case: {line}") };
        let v: Vec<&str> = rest.split(' ').collect();
        let k: u32 = v[0].parse().unwrap();
        let head = v[1..].iter().map(|x| hex_f(x)).collect();
        let mut calls: Vec<Call> = Vec::new();
        loop {
            let line = lines.next().unwrap();
            if line == "end" {
                break;
            }
            if let Some(what) = line.strip_prefix("> ") {
                calls.push(Call {
                    what: what.split(' ').map(String::from).collect(),
                    ops: Vec::new(),
                    result: 0,
                    record: String::new(),
                });
            } else if let Some(r) = line.strip_prefix("= ") {
                calls.last_mut().unwrap().result = r.parse().unwrap();
            } else if let Some(r) = line.strip_prefix("r ") {
                calls.last_mut().unwrap().record = r.to_string();
            } else {
                calls.last_mut().unwrap().ops.push(parse_op(line));
            }
        }
        if k == kind {
            let init = calls.remove(0);
            out.push(Case { head, init, calls });
        }
    }
    out
}

/// The host that answers a call's logged ops in order, checking each.
pub struct Replay<'a> {
    pub ops: std::slice::Iter<'a, Op>,
    pub ctx: String,
}

impl<'a> Replay<'a> {
    pub fn new(call: &'a Call, ctx: String) -> Self {
        Replay { ops: call.ops.iter(), ctx }
    }
    /// Every op was asked.
    pub fn done(&mut self) {
        assert!(self.ops.next().is_none(), "{}: ops left over", self.ctx);
    }
}

impl HudHost for Replay<'_> {
    fn op(&mut self, op: u32, a: i64, b: i64, f: &mut [f32]) -> i64 {
        let want = self.ops.next().unwrap_or_else(|| panic!("{}: an extra op {op} {a} {b}", self.ctx));
        assert_eq!((op, a, b), (want.op, want.a, want.b), "{}", self.ctx);
        let bits: Vec<u32> = f.iter().map(|x| x.to_bits()).collect();
        assert_eq!(bits, want.before, "{}: op {op}'s floats", self.ctx);
        for (x, &w) in f.iter_mut().zip(&want.after) {
            *x = f32::from_bits(w);
        }
        want.ret
    }
}

/// The driver's digest pieces.
pub fn f(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

pub fn rect(r: &super::EeScreensRect) -> String {
    format!("{} {} {} {}", f(r.left), f(r.top), f(r.right), f(r.bottom))
}

pub fn flag(b: bool) -> &'static str {
    if b {
        "1"
    } else {
        "0"
    }
}
