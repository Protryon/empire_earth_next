use super::*;

/// The host's recorded ops of one golden line: each `(op, a, b, c)` must come in order, answered with its result.
struct Replay {
    event: bool,
    ops: Vec<[i64; 5]>,
    at: usize,
    line: usize,
}

impl UiHost for Replay {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        let a = if self.event && op == EE_SCREENS_UI_EV_TRANSLATED_KEY {
            // the key's eight bytes, as the driver recorded them
            let mut k = [0u8; 8];
            unsafe { core::ptr::copy_nonoverlapping(a as *const u8, k.as_mut_ptr(), 8) };
            i64::from_le_bytes(k)
        } else {
            a
        };
        let want = self.ops.get(self.at).copied().unwrap_or_else(|| panic!("line {}: extra op {op}", self.line));
        assert_eq!([op as i64, a, b, c], want[..4], "line {}: op {}", self.line, self.at);
        self.at += 1;
        want[4]
    }
}

fn ops(fields: &[&str]) -> Vec<[i64; 5]> {
    fields
        .iter()
        .map(|f| {
            let v: Vec<i64> = f.split(' ').map(|x| x.parse().unwrap()).collect();
            [v[0], v[1], v[2], v[3], v[4]]
        })
        .collect()
}

fn unhex(s: &str) -> Vec<u8> {
    if s == "-" {
        return Vec::new();
    }
    (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
}

const GOLDEN: &str = include_str!("../golden_ui_thread.txt");

/// `build/sc22/ui/driver.cpp 1 3000 golden`: per message and per window event (one sequence: the statics carry
/// over), the new host's ops and answers; the driver checked them against the original's calls. The play-movie
/// (0x41c) and movie-done (0x41d) messages' ops were taken out when movies left the game (2026-11-17): nothing
/// posts them, and the thread ignores them.
#[test]
fn golden_ui_thread() {
    let st = EventState::new();
    let (mut messages, mut events) = (0, 0);
    for (k, line) in GOLDEN.lines().enumerate() {
        let mut parts = line.split('|');
        let head: Vec<&str> = parts.next().unwrap().split(' ').collect();
        let rest: Vec<&str> = parts.collect();
        let event = head[0] == "E";
        let mut h = Replay { event, ops: ops(&rest), at: 0, line: k + 1 };
        if !event {
            handle_message(&mut h, head[1].parse().unwrap(), head[2].parse().unwrap(), head[3].parse().unwrap());
            messages += 1;
        } else {
            let mut text = unhex(head[5]);
            let n = text.len();
            text.push(0);
            let ev = WindowEvent {
                kind: head[1].parse().unwrap(),
                virtual_key: head[2].parse().unwrap(),
                text_address: -7,
                text: &text[..n],
                cursor: head[3].parse().unwrap(),
                length: head[4].parse().unwrap(),
            };
            handle_event(&mut h, &st, &ev, plat_utf8_length);
            events += 1;
        }
        assert_eq!(h.at, h.ops.len(), "line {}: ops missing", k + 1);
    }
    assert!(messages >= 2000 && events >= 2000);
}

/// Every message the switch handles is in the golden with at least one op.
#[test]
fn golden_reaches_every_message() {
    let mut seen = std::collections::BTreeSet::new();
    for line in GOLDEN.lines().filter(|l| l.starts_with("M ") && l.contains('|')) {
        seen.insert(line.split(' ').nth(1).unwrap().parse::<u32>().unwrap());
    }
    for m in [
        0x12, 0x401, 0x402, 0x403, 0x404, 0x407, 0x408, 0x40a, 0x40b, 0x40c, 0x40d, 0x40e, 0x40f, 0x410, 0x411, 0x412,
        0x413, 0x414, 0x415, 0x416, 0x417, 0x418, 0x419, 0x41b, 0x41e, 0x41f, 0x420, 0x421, 0x422, 0x424, 0x426, 0x427,
        0x428, 0x429, 0x430, 0x431, 0x432, 0x435, 0x436, 0x437, 0x438, 0x439, 0x440, 0x441, 0x442, 0x443,
    ] {
        assert!(seen.contains(&m), "message {m:#x} never in the golden");
    }
}

/// Every op and answer in order; answers from the queue, then 0.
#[derive(Default)]
struct Log(Vec<(u32, i64, i64, i64)>, Vec<i64>);

impl UiHost for Log {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        self.0.push((op, a, b, c));
        if self.1.is_empty() {
            0
        } else {
            self.1.remove(0)
        }
    }
}

#[test]
fn lag_status_falls_through_to_the_splash_bar_and_the_queue() {
    // LAG, LOCK, FORM_BY_ID (5), VISIBLE (1), FORM_BY_ID (5), STATUS_VALUE, UNLOCK, QUEUE_DONE
    let mut h = Log(Vec::new(), vec![0, 0, 5, 1, 5]);
    handle_message(&mut h, 0x442, 0x1_0003, 0x1_0000_0000);
    // (unsigned short) player, (unsigned long) lagging > 0: the high bits are dropped
    assert_eq!(h.0[0], (EE_SCREENS_UI_OP_LAG, 3, 0, 0));
    assert_eq!(h.0[5], (EE_SCREENS_UI_OP_STATUS_VALUE, 5, 0x1_0017, 0x1_0000_0000));
    assert_eq!(h.0.last().unwrap().0, EE_SCREENS_UI_OP_QUEUE_DONE);
}

#[test]
fn unknown_messages_do_nothing() {
    for m in [0, 0x400, 0x405, 0x423, 0x444] {
        let mut h = Log::default();
        handle_message(&mut h, m, 1, 2);
        assert!(h.0.is_empty(), "{m:#x}");
    }
}

#[test]
fn control_without_alt_swallows_typed_text() {
    let st = EventState::new();
    let text = b"ab\0";
    let ev = |kind, vk| WindowEvent { kind, virtual_key: vk, text_address: 0, text: &text[..2], cursor: 0, length: 0 };
    let keys = |h: &Log| h.0.iter().filter(|o| o.0 == EE_SCREENS_UI_EV_TRANSLATED_KEY).count();
    handle_event(&mut Log::default(), &st, &ev(SCREENS_UI_EVENT_KEY_DOWN, 0xa2), plat_utf8_length);
    let mut h = Log::default();
    handle_event(&mut h, &st, &ev(SCREENS_UI_EVENT_TEXT, 0), plat_utf8_length);
    assert_eq!(keys(&h), 0);
    // AltGr (Control + right Alt) types
    handle_event(&mut Log::default(), &st, &ev(SCREENS_UI_EVENT_KEY_DOWN, 0xa5), plat_utf8_length);
    let mut h = Log::default();
    handle_event(&mut h, &st, &ev(SCREENS_UI_EVENT_TEXT, 0), plat_utf8_length);
    assert_eq!(keys(&h), 2);
    // the volumes are read once, at the first event
    let mut h = Log::default();
    handle_event(&mut h, &st, &ev(SCREENS_UI_EVENT_KEY_UP, 0x11), plat_utf8_length);
    assert!(!h.0.iter().any(|o| o.0 == EE_SCREENS_UI_EV_MASTER_VOLUME));
}
