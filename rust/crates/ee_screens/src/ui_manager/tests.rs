use super::*;

/// Every op with its arguments (the records at c read as words) and an answer from a closure.
struct Log<F: FnMut(u32, i64, i64) -> i64> {
    ops: Vec<(u32, i64, i64, Vec<u32>)>,
    answer: F,
}

/// The record at c as 32-bit words (`ADD_LIKE`, `MOVE`, `SET_PLAYER`), else nothing.
fn record(op: u32, c: i64) -> Vec<u32> {
    let words = match op {
        EE_SCREENS_UM_OP_ADD_LIKE => core::mem::size_of::<EeScreensManagerControl>() / 4,
        EE_SCREENS_UM_OP_MOVE => core::mem::size_of::<EeScreensManagerMove>() / 4,
        EE_SCREENS_UM_OP_SET_PLAYER => core::mem::size_of::<EeScreensManagerPlayer>() / 4,
        _ => 0,
    };
    (0..words).map(|i| unsafe { *(c as *const u32).add(i) }).collect()
}

impl<F: FnMut(u32, i64, i64) -> i64> UiHost for Log<F> {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        self.ops.push((op, a, b, record(op, c)));
        (self.answer)(op, a, b)
    }
}

/// The constructor adds 13 settings controls, moves three lobby ranges and adds 33 engine controls; the engine
/// combo's open list is at most eight lines.
#[test]
fn construct_adds_the_port_controls() {
    let mut h = Log { ops: Vec::new(), answer: |op, _, _| if op == EE_SCREENS_UM_OP_ENGINE_COUNT { 12 } else { 0 } };
    construct(&mut h);
    let adds: Vec<_> = h.ops.iter().filter(|o| o.0 == EE_SCREENS_UM_OP_ADD_LIKE).collect();
    assert_eq!(adds.len(), 13 + 33);
    assert_eq!(h.ops.iter().filter(|o| o.0 == EE_SCREENS_UM_OP_MOVE).count(), 3);
    let combo = &adds[14].3;
    let top = f32::from_bits(combo[6]);
    assert_eq!(f32::from_bits(combo[7]), top + 0.025 + 8.0 * 0.034);
    assert_eq!(h.ops.last().unwrap().0, EE_SCREENS_UM_OP_SAVE_EVENT);
}

/// A movie stops on Space and is waited for; F9 then posts the screenshot and skips the base update.
#[test]
fn keyboard_stops_the_movie_then_f9() {
    let mut playing = 3;
    let mut h = Log {
        ops: Vec::new(),
        answer: move |op, a, _| match op {
            EE_SCREENS_UM_OP_KEY_PRESSED => (a == EE_SCREENS_UM_KEY_SPACE || a == EE_SCREENS_UM_KEY_F9) as i64,
            EE_SCREENS_UM_OP_MOVIE_PLAYING => {
                playing -= 1;
                (playing >= 0) as i64
            }
            EE_SCREENS_UM_OP_KEYS_FREE => 1,
            _ => 7,
        },
    };
    assert_eq!(update_keyboard(&mut h), 0);
    let ops: Vec<u32> = h.ops.iter().map(|o| o.0).collect();
    assert_eq!(ops.iter().filter(|&&o| o == EE_SCREENS_UM_OP_SLEEP).count(), 2);
    assert_eq!(ops.last(), Some(&EE_SCREENS_UM_OP_POST_F9));
}

/// No press: no movie test, the base update's result.
#[test]
fn mouse_without_a_press_only_updates() {
    let mut h = Log { ops: Vec::new(), answer: |op, _, _| if op == EE_SCREENS_UM_OP_BASE_MOUSE { 5 } else { 0 } };
    assert_eq!(update_mouse(&mut h), 5);
    let ops: Vec<u32> = h.ops.iter().map(|o| o.0).collect();
    assert_eq!(ops, [EE_SCREENS_UM_OP_BUTTON_PRESSED, EE_SCREENS_UM_OP_BUTTON_PRESSED, EE_SCREENS_UM_OP_BASE_MOUSE]);
}

const GOLDEN: &str = include_str!("../golden_ui_manager.txt");

/// One golden call's recorded host ops: `(op, a, b, answer, the record's words)`.
struct Replay {
    ops: Vec<(u32, i64, i64, i64, Vec<u32>)>,
    at: usize,
    line: usize,
}

impl UiHost for Replay {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        let (line, at) = (self.line, self.at);
        let want = self.ops.get(at).unwrap_or_else(|| panic!("line {line}: extra op {op}"));
        assert_eq!((op, a, b), (want.0, want.1, want.2), "line {line}: op {at}");
        assert_eq!(record(op, c), want.4, "line {line}: op {at}'s record");
        self.at += 1;
        want.3
    }
}

/// The original constructor, `StartScenarioEditor`, `UpdateKeyboard` and `UpdateMouse` over random fake worlds
/// (`build/sc26/um`, seed 1): the same ops with the same arguments and records, in order, and the same results.
#[test]
fn golden_ui_manager() {
    let mut count = 0;
    for (n, line) in GOLDEN.lines().enumerate() {
        let (body, result) = line.rsplit_once('=').unwrap();
        let mut parts = body.split('|');
        let kind = parts.next().unwrap();
        let ops = parts
            .map(|p| {
                let mut w = p.split(' ');
                let mut num = || w.next().unwrap().parse::<i64>().unwrap();
                let (op, a, b, r) = (num() as u32, num(), num(), num());
                let words = w.map(|x| u32::from_str_radix(x, 16).unwrap()).collect();
                (op, a, b, r, words)
            })
            .collect();
        let mut h = Replay { ops, at: 0, line: n + 1 };
        let r = match kind {
            "K" => {
                construct(&mut h);
                0
            }
            "S" => {
                start_scenario_editor(&mut h);
                0
            }
            "Y" => update_keyboard(&mut h),
            _ => update_mouse(&mut h),
        };
        assert_eq!(h.at, h.ops.len(), "line {}: ops left", n + 1);
        assert_eq!(r, result.parse::<i64>().unwrap(), "line {}: result", n + 1);
        count += 1;
    }
    assert_eq!(count, 600);
}
