//! The action queues against the real C++: a scratch driver (`build/sc13/act_driver.cpp`) ran the unmodified
//! `EEUserInterface.cpp`'s `PostAction`, `ProcessActions`, `Undo`, `Redo` and `ClearQueue` over fake actions (each
//! run, undo and delete printed; three in four undoable), the UI copy lock and the world's write lock, on 2,000 cases
//! of one operation each (a new action posted and the poster's reference released, a live action posted again,
//! process, undo, redo, a queue cleared), printing every live action's reference count after each. The driver
//! relinked over the new unit (this module over `ee_screenscpp_action`) printed the same on seeds 1-4 (2,000 cases;
//! 20,000 on seeds 2-4). Its host log is `golden_actions.txt`: per case the operation, then the host calls `op action
//! answer` (actions by the driver's numbers). The replay runs the operations on one `Actions` over a host that checks
//! every call against the log and gives its answer.

use super::*;

const GOLDEN: &str = include_str!("../golden_actions.txt");

/// A case: its operation, the host calls, Undo's or Redo's answer.
type Case = (&'static str, Vec<[u32; 3]>, Option<&'static str>);

struct Replay<'a> {
    calls: std::slice::Iter<'a, [u32; 3]>,
    case: &'a str,
}

impl ActionHost for Replay<'_> {
    fn call(&mut self, op: u32, action: Action) -> u32 {
        let want = self.calls.next().unwrap_or_else(|| panic!("{}: an extra call {op} {action}", self.case));
        assert_eq!([op, action as u32], want[..2], "{}", self.case);
        want[2]
    }
}

#[test]
fn replays_the_golden() {
    // (each case: its operation, the host calls, and for Undo and Redo a `result` line)
    let mut cases: Vec<Case> = Vec::new();
    for line in GOLDEN.lines() {
        if let Some(call) = line.strip_prefix(' ') {
            let v: Vec<u32> = call.split(' ').map(|x| x.parse().unwrap()).collect();
            cases.last_mut().unwrap().1.push([v[0], v[1], v[2]]);
        } else if let Some(result) = line.strip_prefix("result ") {
            cases.last_mut().unwrap().2 = Some(result);
        } else {
            cases.push((line, Vec::new(), None));
        }
    }
    assert_eq!(cases.len(), 2000);
    let actions = Actions::new();
    let (mut repeats, mut limits, mut most) = (0, 0, 0);
    for (header, calls, result) in &cases {
        let words: Vec<&str> = header.split(' ').collect();
        let mut h = Replay { calls: calls.iter(), case: header };
        match words[2] {
            "post" => actions.post(&mut h, words[3].parse().unwrap()),
            "none" => {}
            "process" => {
                // an action posted again while last on the undo list: released, not pushed
                let last = actions.undo.lock().unwrap().back().copied();
                let queued: Vec<Action> = actions.queue.lock().unwrap().iter().copied().collect();
                repeats += queued.iter().filter(|&&a| Some(a) == last).count();
                actions.process(&mut h);
            }
            "undo" => assert_eq!(Some(actions.undo(&mut h).to_string().as_str()), *result, "{header}"),
            "redo" => assert_eq!(Some(actions.redo(&mut h).to_string().as_str()), *result, "{header}"),
            "clear" => {
                let queue = match words[3] {
                    "0" => Queue::Actions,
                    "1" => Queue::Undo,
                    _ => Queue::Redo,
                };
                actions.clear(&mut h, queue);
            }
            other => panic!("{other}"),
        }
        assert!(h.calls.next().is_none(), "{header}: calls left");
        limits += (actions.len(Queue::Undo) == ACTIONS_UNDO_LIMIT) as u32;
        most = most.max(actions.len(Queue::Undo));
    }
    assert!(repeats > 5 && limits > 20 && most == ACTIONS_UNDO_LIMIT, "{repeats} {limits} {most}");
}

/// A host that records the calls; every action undoable.
#[derive(Default)]
struct Record(Vec<(u32, Action)>);

impl ActionHost for Record {
    fn call(&mut self, op: u32, action: Action) -> u32 {
        self.0.push((op, action));
        (op == EE_SCREENS_ACTION_UNDOABLE) as u32
    }
}

#[test]
fn undo_and_redo_move_an_action_between_the_lists() {
    let actions = Actions::new();
    let mut h = Record::default();
    assert_eq!(actions.undo(&mut h), EE_SCREENS_ACTION_NONE);
    actions.post(&mut h, 5);
    actions.process(&mut h);
    assert_eq!(actions.undo(&mut h), 0);
    assert_eq!((actions.len(Queue::Undo), actions.len(Queue::Redo)), (0, 1));
    assert_eq!(actions.redo(&mut h), 0);
    assert_eq!((actions.len(Queue::Undo), actions.len(Queue::Redo)), (1, 0));
    assert_eq!(h.0.last(), Some(&(EE_SCREENS_ACTION_RUN, 5)));
}

#[test]
fn nothing_queued_takes_no_lock() {
    let actions = Actions::new();
    let mut h = Record::default();
    actions.process(&mut h);
    assert!(h.0.is_empty());
}
