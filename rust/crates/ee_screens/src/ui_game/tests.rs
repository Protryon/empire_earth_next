use super::*;

/// Every op and an answer from a closure.
struct Log<F: FnMut(u32, i64, i64, i64) -> i64> {
    ops: Vec<(u32, i64, i64)>,
    answer: F,
}

impl<F: FnMut(u32, i64, i64, i64) -> i64> UiHost for Log<F> {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        self.ops.push((op, a, b));
        (self.answer)(op, a, b, c)
    }
}

/// Two queued messages handled in order after the queue's unlock; the free camera moved by the elapsed seconds.
#[test]
fn run_handles_the_queue_then_quits() {
    let mut queue = vec![[0x401i64, 7, 8], [0x402, 9, 10]].into_iter();
    let mut quitting = 0;
    let mut time = 1000;
    let mut h = Log {
        ops: Vec::new(),
        answer: move |op, _a, _b, c| match op {
            EE_SCREENS_UG_OP_RECEIVE => match queue.next() {
                Some(m) => {
                    unsafe { core::ptr::copy_nonoverlapping(m.as_ptr(), c as *mut i64, 3) };
                    0
                }
                None => 1,
            },
            EE_SCREENS_UG_OP_QUITTING => {
                quitting += 1;
                (quitting > 2) as i64
            }
            EE_SCREENS_UG_OP_FREE_CAMERA => 1,
            EE_SCREENS_UG_OP_UPDATING => 1,
            EE_SCREENS_UG_OP_TIME => {
                time += 250;
                time
            }
            _ => 0,
        },
    };
    assert_eq!(run(&mut h), 0);
    let handled: Vec<_> = h.ops.iter().filter(|o| o.0 == EE_SCREENS_UG_OP_HANDLE).collect();
    assert_eq!(handled, [&(EE_SCREENS_UG_OP_HANDLE, 0x401, 7), &(EE_SCREENS_UG_OP_HANDLE, 0x402, 9)]);
    let camera = h.ops.iter().find(|o| o.0 == EE_SCREENS_UG_OP_CAMERA).unwrap();
    assert_eq!(f32::from_bits(camera.1 as u32), 0.25);
}

/// An invalid header: only the message, then the file deleted.
#[test]
fn invalid_save_is_reported_and_deleted() {
    let mut h = Log { ops: Vec::new(), answer: |_, _, _, _| 0 };
    load_saved_game(&mut h, 42);
    let ops: Vec<u32> = h.ops.iter().map(|o| o.0).collect();
    assert_eq!(ops, [EE_SCREENS_UG_OP_LOAD_HEADER, EE_SCREENS_UG_OP_INVALID_SAVE, EE_SCREENS_UG_OP_DELETE_FILE]);
}

const GOLDEN: &str = include_str!("../golden_ui_game.txt");

/// One golden call's recorded host ops: `(op, a, b, result, c or the received message)`.
struct Replay {
    ops: Vec<Vec<i64>>,
    at: usize,
    line: usize,
}

impl UiHost for Replay {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        let (line, at) = (self.line, self.at);
        let want = self.ops.get(at).unwrap_or_else(|| panic!("line {line}: extra op {op}"));
        assert_eq!([op as i64, a, b], want[..3], "line {line}: op {at}");
        if op == EE_SCREENS_UG_OP_RECEIVE {
            unsafe { core::ptr::copy_nonoverlapping(want[4..7].as_ptr(), c as *mut i64, 3) };
        } else {
            assert_eq!(c, want[4], "line {line}: op {at}'s c");
        }
        self.at += 1;
        want[3]
    }
}

/// `build/sc25/ui/driver.cpp`: per call (T `TestScenario`, S `SaveAndTestScenario`, L `LoadSavedGame` of the recorded file,
/// R `Run`), the new host's ops and answers; the driver checked them against the original's calls (cut from 7f5a8b0a).
#[test]
fn golden_ui_game() {
    let mut kinds = std::collections::BTreeMap::new();
    for (k, line) in GOLDEN.lines().enumerate() {
        let (kind, rest) = line.split_at(1);
        let ops = rest.split('|').skip(1).map(|f| f.split(' ').map(|x| x.parse().unwrap()).collect()).collect();
        let mut h = Replay { ops, at: 0, line: k + 1 };
        match kind {
            "T" => test_scenario(&mut h),
            "S" => save_and_test_scenario(&mut h),
            "L" => {
                let file = h.ops[0][1];
                load_saved_game(&mut h, file)
            }
            _ => assert_eq!(run(&mut h), 0),
        }
        assert_eq!(h.at, h.ops.len(), "line {}: ops missing", k + 1);
        *kinds.entry(kind.to_string()).or_insert(0) += 1;
    }
    for kind in ["T", "S", "L", "R"] {
        assert!(kinds.get(kind).copied().unwrap_or(0) >= 30, "{kinds:?}");
    }
}

const GOLDEN_MEMBERS: &str = include_str!("../golden_ui_members.txt");

/// `build/sc28/ui/driver.cpp 1 700 golden`: per call (M `LeaveMap`, C `CancelGame`, X `CloseScenarioEditor`, E
/// `EnterScenarioEditor`, L `StartLoadingFromEditor`, B `ShowCivilizationBuilder`, R `RestartGame`) with its arguments,
/// the new host's ops and answers; the driver checked them against the original's calls (cut from 2cac466f).
#[test]
fn golden_ui_members() {
    let mut kinds = std::collections::BTreeMap::new();
    for (k, line) in GOLDEN_MEMBERS.lines().enumerate() {
        let mut fields = line.split('|');
        let head: Vec<&str> = fields.next().unwrap().split(' ').collect();
        let args: Vec<i64> = head[1..4].iter().map(|x| x.parse().unwrap()).collect();
        let ops = fields.map(|f| f.split(' ').map(|x| x.parse().unwrap()).collect()).collect();
        let mut h = Replay { ops, at: 0, line: k + 1 };
        let which = match head[0] {
            "M" => EE_SCREENS_UG_DO_LEAVE_MAP,
            "C" => EE_SCREENS_UG_DO_CANCEL_GAME,
            "X" => EE_SCREENS_UG_DO_CLOSE_EDITOR,
            "E" => EE_SCREENS_UG_DO_ENTER_EDITOR,
            "L" => EE_SCREENS_UG_DO_LOAD_FROM_EDITOR,
            "B" => EE_SCREENS_UG_DO_CIV_BUILDER,
            _ => EE_SCREENS_UG_DO_RESTART,
        };
        member(&mut h, which, args[0], args[1], args[2]);
        assert_eq!(h.at, h.ops.len(), "line {}: ops missing", k + 1);
        *kinds.entry(head[0].to_string()).or_insert(0) += 1;
    }
    for kind in ["M", "C", "X", "E", "L", "B", "R"] {
        assert!(kinds.get(kind).copied().unwrap_or(0) >= 50, "{kinds:?}");
    }
}

/// A lost server's client is booted with the host's message (none with a lobby open, which is quit).
#[test]
fn a_lost_server_boots_the_client() {
    for lobby in [0, 1] {
        let mut h = Log {
            ops: Vec::new(),
            answer: move |op, _a, _b, _c| match op {
                EE_SCREENS_UG_OP_SERVER_LOST => 1,
                EE_SCREENS_UG_OP_LOBBY_OPEN => lobby,
                _ => 0,
            },
        };
        cancel_game(&mut h);
        let boot = h.ops.iter().find(|o| o.0 == EE_SCREENS_UG_OP_BOOT_CLIENT).unwrap();
        assert_eq!(boot.1, if lobby == 1 { 0 } else { EE_SCREENS_UG_IDS_HOST_GONE });
        assert_eq!(h.ops.iter().any(|o| o.0 == EE_SCREENS_UG_OP_QUIT_FLAG), lobby == 1);
    }
}
