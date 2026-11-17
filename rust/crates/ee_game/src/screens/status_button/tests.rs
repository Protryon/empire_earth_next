use super::*;

/// The host's recorded ops of one golden press: each `(op, a, b, c)` must come in order, answered with its result;
/// the help text's bytes (read into a, or handed out at a) as payloads.
struct Replay {
    ops: Vec<[i64; 5]>,
    payloads: Vec<Option<Vec<u8>>>,
    at: usize,
    line: usize,
}

/// One op's numbers and its payload (" x" and hex).
fn parse_op(f: &str) -> ([i64; 5], Option<Vec<u8>>) {
    let (nums, payload) = match f.find(" x") {
        Some(i) => {
            let hex = &f[i + 2..];
            let bytes = (0..hex.len() / 2).map(|k| u8::from_str_radix(&hex[2 * k..2 * k + 2], 16).unwrap()).collect();
            (&f[..i], Some(bytes))
        }
        None => (f, None),
    };
    let v: Vec<i64> = nums.split(' ').map(|x| x.parse().unwrap()).collect();
    ([v[0], v[1], v[2], v[3], v[4]], payload)
}

impl ButtonHost for Replay {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        if op == EE_SCREENS_SB_OP_BUTTON_UP {
            // the host's virtual ButtonUp runs the left press first (its ops are recorded before this one)
            button_up(self);
        }
        let want = self.ops.get(self.at).copied().unwrap_or_else(|| panic!("line {}: extra op {op}", self.line));
        let payload = self.payloads[self.at].clone();
        let a_ = if payload.is_some() { 0 } else { a };
        assert_eq!([op as i64, a_, b, c], want[..4], "line {}: op {}", self.line, self.at);
        self.at += 1;
        if let Some(bytes) = payload {
            if op == EE_SCREENS_SB_OP_HELP_OUT {
                // SAFETY: help_string hands out its b bytes at a
                let out = unsafe { core::slice::from_raw_parts(a as *const u8, b as usize) };
                assert_eq!(out, &bytes[..], "line {}: the help string", self.line);
            } else {
                assert!(bytes.len() <= b as usize, "line {}: the read's room", self.line);
                // SAFETY: the read's buffer at a has room for b bytes
                unsafe { core::ptr::copy_nonoverlapping(bytes.as_ptr(), a as *mut u8, bytes.len()) };
            }
        }
        want[4]
    }
}

const GOLDEN: &str = include_str!("../golden_status_button.txt");

/// `build/sc24/sb/driver.cpp 1 5000 golden` (round 22's driver with `CopyData` and `GetHelpString`): per call (L:
/// `ButtonUp`, R: `MouseRButtonUp`, C: `CopyData`, H: `GetHelpString`, the last two's originals the ones before
/// command_panel.rs, a137f1a9^), the new host's ops and answers; the driver checked them against the original's
/// calls.
#[test]
fn golden_status_button() {
    let mut n = 0;
    let mut copies = 0;
    let mut helps = 0;
    for (k, line) in GOLDEN.lines().enumerate() {
        let mut parts = line.split('|');
        let kind = parts.next().unwrap();
        let (ops, payloads) = parts.map(parse_op).unzip();
        let mut h = Replay { ops, payloads, at: 0, line: k + 1 };
        match kind {
            "R" => right_button_up(&mut h),
            "H" => {
                assert!(help_string(&mut h));
                helps += 1;
            }
            "C" => {
                let result = copy_data(&mut h);
                assert_eq!(result, h.ops[h.at - 1][4], "line {}: CopyData's result", k + 1);
                copies += 1;
            }
            _ => button_up(&mut h),
        }
        assert_eq!(h.at, h.ops.len(), "line {}: ops missing", k + 1);
        n += 1;
    }
    assert!(n >= 3000 && copies >= 500 && helps >= 500, "{n} cases, {copies} CopyData, {helps} GetHelpString");
}

/// Every message kind is sent somewhere in the golden.
#[test]
fn golden_sends_every_message() {
    let mut seen = std::collections::BTreeSet::new();
    for line in GOLDEN.lines() {
        for f in line.split('|').skip(1) {
            let (v, _) = parse_op(f);
            if v[0] == EE_SCREENS_SB_OP_SEND as i64 {
                seen.insert(v[1]);
            }
        }
    }
    for kind in EE_SCREENS_SB_SEND_UNLOAD..=EE_SCREENS_SB_SEND_REMOVE {
        assert!(seen.contains(&kind), "message {kind} never sent in the golden");
    }
}

/// Every op and its answers from a queue (then 0).
#[derive(Default)]
struct Log(Vec<(u32, i64, i64, i64)>, Vec<(u32, i64)>);

impl ButtonHost for Log {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        self.0.push((op, a, b, c));
        self.1.iter().find(|x| x.0 == op).map_or(0, |x| x.1)
    }
}

#[test]
fn jit_waits_for_the_deathmatch_delay() {
    let mut h = Log(
        Vec::new(),
        vec![
            (EE_SCREENS_SB_OP_BUTTON, 0x29),
            (EE_SCREENS_SB_OP_GAME_RESOURCES, 4),
            (EE_SCREENS_SB_OP_GAME_CLOCK, 59_999.0f32.to_bits() as i64),
            (EE_SCREENS_SB_OP_WORLD_LONG, 60),
        ],
    );
    command(&mut h);
    assert!(h.0.contains(&(EE_SCREENS_SB_OP_CHAT_STRING, EE_SCREENS_SB_IDS_JIT_NOT_YET, 0, 0)));
    assert!(!h.0.iter().any(|o| o.0 == EE_SCREENS_SB_OP_SEND));
    // at the delay: sent
    h.0.clear();
    h.1[2].1 = 60_000.0f32.to_bits() as i64;
    command(&mut h);
    assert!(h.0.contains(&(EE_SCREENS_SB_OP_SEND, EE_SCREENS_SB_SEND_JIT, 0, 0)));
    assert_eq!(h.0.last().unwrap(), &(EE_SCREENS_SB_OP_SOUND, EE_SCREENS_SB_SOUND_BUTTON1, 0, 0));
}

#[test]
fn a_refusal_waits_for_the_interval() {
    // time 100, last error 0, interval 100: not past it, no chat
    let mut h = Log(Vec::new(), vec![(EE_SCREENS_SB_OP_WORLD_LONG, 100), (EE_SCREENS_SB_OP_TIME, 100)]);
    button_up(&mut h);
    assert!(h.0.contains(&(EE_SCREENS_SB_OP_SOUND, EE_SCREENS_SB_SOUND_BUTTON2, 0, 0)));
    assert!(!h.0.iter().any(|o| o.0 == EE_SCREENS_SB_OP_CHAT_REASON));
    let mut h = Log(Vec::new(), vec![(EE_SCREENS_SB_OP_WORLD_LONG, 100), (EE_SCREENS_SB_OP_TIME, 101)]);
    button_up(&mut h);
    assert!(h.0.iter().any(|o| o.0 == EE_SCREENS_SB_OP_CHAT_REASON));
    assert!(h.0.contains(&(EE_SCREENS_SB_OP_SET_LAST_ERROR, 101, 0, 0)));
}
