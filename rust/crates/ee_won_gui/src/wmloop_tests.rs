use super::*;
use crate::wmobj::{ee_won_wm_free, ee_won_wm_new, EeWonWmCall};
use std::cell::RefCell as Cell;
use std::collections::HashMap;
use std::ffi::c_void;
use std::sync::atomic::{AtomicIsize, AtomicUsize};
use std::sync::{Arc, Barrier, Mutex};

// ---- a recording host for the single-thread tests: the log, fixed answers per (op, sub-call), and a hook run on
// an event's Deliver (the manager reached through a thread-local pointer)

type Entry = (Op, isize, isize, isize, isize);
type Hook = Box<dyn Fn(&EeWonWm, isize)>;

thread_local! {
    static LOG: Cell<Vec<Entry>> = const { Cell::new(Vec::new()) };
    static ANSWERS: Cell<HashMap<(u32, isize, isize), isize>> = Cell::new(HashMap::new());
    static WM: Cell<*const EeWonWm> = const { Cell::new(std::ptr::null()) };
    static ON_DELIVER: Cell<Option<Hook>> = const { Cell::new(None) };
}

unsafe extern "C" fn rec_host(_wm: *mut c_void, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    LOG.with(|l| l.borrow_mut().push((op, a, b, c, d)));
    if op == Op::WmEvent && a == EeWonWmEvent::Deliver as isize {
        let hook = ON_DELIVER.with(|h| h.borrow_mut().take());
        if let Some(h) = hook {
            let wm = WM.with(|w| *w.borrow());
            // SAFETY: the test's live manager
            h(unsafe { &*wm }, b);
            ON_DELIVER.with(|x| {
                if x.borrow().is_none() {
                    *x.borrow_mut() = Some(h)
                }
            });
        }
    }
    let sub = if matches!(op, Op::WmComp | Op::WmWin | Op::WmEvent) { a } else { 0 };
    ANSWERS.with(|m| {
        let m = m.borrow();
        m.get(&(op as u32, sub, b)).or_else(|| m.get(&(op as u32, sub, 0))).copied().unwrap_or(0)
    })
}

/// `op`'s answer for the sub-call `sub` on the object `obj` (0: any object).
fn answer(op: Op, sub: isize, obj: isize, v: isize) {
    ANSWERS.with(|m| m.borrow_mut().insert((op as u32, sub, obj), v));
}

fn take() -> Vec<Entry> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}

fn new_wm() -> Box<EeWonWm> {
    ANSWERS.with(|m| m.borrow_mut().clear());
    ON_DELIVER.with(|h| *h.borrow_mut() = None);
    take();
    let wm = Box::new(EeWonWm::new(std::ptr::dangling_mut::<c_void>(), Some(rec_host)));
    WM.with(|w| *w.borrow_mut() = &*wm as *const EeWonWm);
    wm
}

fn ev_ops(log: &[Entry], call: EeWonWmEvent) -> Vec<isize> {
    log.iter().filter(|e| e.0 == Op::WmEvent && e.1 == call as isize).map(|e| e.2).collect()
}

fn win_ops(log: &[Entry], call: EeWonWmWin) -> Vec<isize> {
    log.iter().filter(|e| e.0 == Op::WmWin && e.1 == call as isize).map(|e| e.2).collect()
}

#[test]
fn the_field_counts_are_the_last_fields() {
    assert_eq!(EE_WON_WM_FIELD_COUNT_CHECK, F::BroadcastCount as i32);
    assert_eq!(crate::wmobj::EE_WON_WM_FIELD_POPUP_LAST, F::PopupFront as i32);
}
const EE_WON_WM_FIELD_COUNT_CHECK: i32 = crate::wmobj::EE_WON_WM_FIELD_COUNT;

#[test]
fn windows_are_held_once_and_the_first_is_the_default() {
    let w = new_wm();
    assert!(w.add_window(30));
    assert!(w.add_window(20));
    assert!(w.add_window(30));
    assert_eq!(w.field(F::DefaultWindow as i32), 30);
    assert_eq!(w.field(F::WindowCount as i32), 2);
    assert_eq!(win_ops(&take(), EeWonWmWin::Hold), vec![30, 20]);
    w.set_field(F::TargetDepth as i32, 4);
    assert!(w.remove_window(20));
    assert!(!w.remove_window(20));
    assert_eq!(w.field(F::DefaultWindow as i32), 30);
    assert_eq!(w.field(F::TargetDepth as i32), 4);
    assert!(w.remove_window(30));
    assert_eq!(w.field(F::DefaultWindow as i32), 0);
    // the last window gone: the loops' target depth back at 0
    assert_eq!(w.field(F::TargetDepth as i32), 0);
    assert_eq!(win_ops(&take(), EeWonWmWin::Release), vec![20, 30]);
}

#[test]
fn removing_a_window_drops_only_its_queued_events() {
    let w = new_wm();
    w.add_window(1);
    w.add_window(2);
    for (ev, win) in [(101, 1), (102, 2), (103, 1), (104, 2)] {
        answer(Op::WmEvent, EeWonWmEvent::Window as isize, ev, win);
        w.post_event(ev, false);
    }
    take();
    w.remove_window(1);
    let log = take();
    // each event's window read, the matches given back in the list's order between the reads
    let seq: Vec<(isize, isize)> = log.iter().filter(|e| e.0 == Op::WmEvent).map(|e| (e.1, e.2)).collect();
    let (wi, re) = (EeWonWmEvent::Window as isize, EeWonWmEvent::Release as isize);
    assert_eq!(seq, vec![(wi, 101), (re, 101), (wi, 102), (wi, 103), (re, 103), (wi, 104)]);
    assert_eq!(w.set_at(EeWonWmSet::Events as u32, 0), 102);
    assert_eq!(w.set_at(EeWonWmSet::Events as u32, 1), 104);
    assert_eq!(w.peek_next_event(), 102);
}

#[test]
fn post_event_signals_when_asked_or_outside_a_loop() {
    let w = new_wm();
    w.post_event(5, false);
    w.cur_depth.store(1, Ordering::SeqCst);
    w.post_event(6, false);
    w.post_event(7, true);
    let log = take();
    assert_eq!(log.iter().filter(|e| e.0 == Op::WmSignal).count(), 2);
    assert_eq!(ev_ops(&log, EeWonWmEvent::Hold), vec![5, 6, 7]);
    assert_eq!(w.field(F::EventCount as i32), 3);
}

#[test]
fn a_timer_removed_while_the_timers_run_waits_and_the_removals_are_kept() {
    let w = new_wm();
    w.add_timer_event(40);
    w.add_timer_event(50);
    answer(Op::WmTick, 0, 0, 100);
    // the first timer's delivery removes the second: it is still delivered this step
    ON_DELIVER.with(|h| {
        *h.borrow_mut() = Some(Box::new(|wm: &EeWonWm, ev: isize| {
            if ev == 40 {
                wm.remove_timer_event(50);
            }
        }))
    });
    take();
    w.process_events();
    let log = take();
    assert_eq!(ev_ops(&log, EeWonWmEvent::Deliver), vec![40, 50]);
    assert_eq!(w.field(F::TimerCount as i32), 1);
    // the deferred removal applied after the step, and (as in the original) never cleared
    assert_eq!(w.field(F::RemoveTimerCount as i32), 1);
    assert_eq!(w.field(F::CallingTimers as i32), 0);
    // a timer added while they run, after the one delivering, is delivered in the same step (the live set)
    ON_DELIVER.with(|h| {
        *h.borrow_mut() = Some(Box::new(|wm: &EeWonWm, ev: isize| {
            if ev == 40 {
                wm.add_timer_event(45);
            }
        }))
    });
    answer(Op::WmTick, 0, 0, 200);
    w.process_events();
    assert_eq!(ev_ops(&take(), EeWonWmEvent::Deliver), vec![40, 45]);
}

#[test]
fn remove_all_timers_while_they_run_defers_them_all() {
    let w = new_wm();
    w.add_timer_event(40);
    w.add_timer_event(50);
    w.lp.borrow_mut().calling_timers = true;
    w.remove_all_timer_events();
    assert_eq!(w.field(F::RemoveTimerCount as i32), 2);
    w.lp.borrow_mut().calling_timers = false;
    take();
    w.remove_all_timer_events();
    assert_eq!(w.field(F::TimerCount as i32), 0);
    assert_eq!(ev_ops(&take(), EeWonWmEvent::Release), vec![40, 50]);
}

#[test]
fn a_broadcast_reaches_each_subscriber_once_even_when_one_leaves() {
    let w = new_wm();
    for c in [3, 1, 2] {
        w.subscribe_to_broadcast(c, true);
    }
    w.subscribe_to_broadcast(2, true);
    assert_eq!(w.field(F::BroadcastCount as i32), 3);
    take();
    w.broadcast_event(77);
    let log = take();
    let got: Vec<isize> = log
        .iter()
        .filter(|e| e.0 == Op::WmComp && e.1 == EeWonWmComp::HandleComponentEvent as isize)
        .map(|e| e.2)
        .collect();
    assert_eq!(got, vec![1, 2, 3]);
    w.subscribe_to_broadcast(2, false);
    w.clear_subscriptions();
    assert_eq!(w.field(F::BroadcastCount as i32), 0);
}

#[test]
fn the_screen_resolution_changes_only_when_both_sides_differ() {
    let w = new_wm();
    // the hook answers 0 x 0: a mode of 0 x 600 shares the width
    w.set_screen_resolution(0, 600);
    assert_eq!(take().len(), 1);
    w.set_screen_resolution(800, 600);
    let log = take();
    assert_eq!(log.len(), 2);
    assert_eq!((log[1].1, log[1].2, log[1].3), (1, 800, 600));
    // the remembered mode was 0 x 0 (and no window): nothing to restore, no op
    w.restore_screen_resolution();
    assert!(take().is_empty());
}

#[test]
fn a_mouse_move_followed_by_another_for_the_same_window_is_dropped() {
    let w = new_wm();
    answer(Op::WmEvent, EeWonWmEvent::Type as isize, 0, EVENT_MOUSE_MOVE);
    answer(Op::WmEvent, EeWonWmEvent::Window as isize, 0, 9);
    w.post_event(11, false);
    w.post_event(12, false);
    take();
    w.deliver_event(11);
    assert!(win_ops(&take(), EeWonWmWin::HandleMouse).is_empty());
    // the last one goes to its window and records the input tick
    answer(Op::WmTick, 0, 0, 1234);
    w.events.lock().unwrap().clear();
    w.deliver_event(12);
    assert_eq!(win_ops(&take(), EeWonWmWin::HandleMouse), vec![9]);
    assert_eq!(w.field(F::LastInputTick as i32), 1234);
}

#[test]
fn a_window_close_removes_the_window_and_delivers_its_close_event() {
    let w = new_wm();
    w.add_window(9);
    answer(Op::WmEvent, EeWonWmEvent::Type as isize, 0, EVENT_WINDOW_CLOSE);
    answer(Op::WmEvent, EeWonWmEvent::Window as isize, 0, 9);
    answer(Op::WmWin, EeWonWmWin::CloseEvent as isize, 9, 500);
    w.notify_got_focus(9);
    take();
    w.deliver_event(70);
    let log = take();
    assert_eq!(win_ops(&log, EeWonWmWin::PlatformClose), vec![9]);
    assert_eq!(ev_ops(&log, EeWonWmEvent::Deliver), vec![500]);
    assert_eq!(ev_ops(&log, EeWonWmEvent::Hold), vec![500]);
    assert_eq!(ev_ops(&log, EeWonWmEvent::Release), vec![500]);
    // the close event given back after the window left the set
    let rel_win = log.iter().position(|e| e.0 == Op::WmWin && e.1 == EeWonWmWin::Release as isize).unwrap();
    let rel_ev = log.iter().position(|e| e.0 == Op::WmEvent && e.1 == EeWonWmEvent::Release as isize).unwrap();
    assert!(rel_win < rel_ev);
    assert_eq!(w.field(F::FocusWindow as i32), 0);
    assert_eq!(w.field(F::WindowSetCount as i32), 0);
}

#[test]
fn the_destructor_gives_every_container_back() {
    let wm = ee_won_wm_new(std::ptr::dangling_mut::<c_void>(), Some(rec_host));
    // SAFETY: just made
    let w = unsafe { &*wm };
    w.add_window(1);
    w.post_event(2, false);
    w.add_timer_event(3);
    w.subscribe_to_broadcast(4, true);
    take();
    // SAFETY: made above, not used after
    unsafe { ee_won_wm_free(wm) };
    let log = take();
    let order: Vec<(Op, isize)> = log
        .iter()
        .filter(|e| {
            (e.0 == Op::WmEvent && e.1 == EeWonWmEvent::Release as isize)
                || (e.0 == Op::WmWin && e.1 == EeWonWmWin::Release as isize)
                || (e.0 == Op::WmComp && e.1 == EeWonWmComp::Release as isize)
        })
        .map(|e| (e.0, e.2))
        .collect();
    assert_eq!(order, vec![(Op::WmComp, 4), (Op::WmEvent, 3), (Op::WmEvent, 2), (Op::WmWin, 1)]);
}

#[test]
fn a_delivery_that_posts_does_not_deadlock_and_is_delivered_in_the_same_pass() {
    let w = new_wm();
    answer(Op::WmEvent, EeWonWmEvent::Type as isize, 0, 99);
    ON_DELIVER.with(|h| {
        *h.borrow_mut() = Some(Box::new(|wm: &EeWonWm, ev: isize| {
            if ev < 5 {
                wm.post_event(ev + 1, true);
            }
        }))
    });
    w.post_event(1, false);
    take();
    w.process_events();
    assert_eq!(ev_ops(&take(), EeWonWmEvent::Deliver), vec![1, 2, 3, 4, 5]);
    assert_eq!(w.field(F::EventCount as i32), 0);
}

#[test]
fn the_calls_reach_the_methods() {
    let w = new_wm();
    assert_eq!(w.call(EeWonWmCall::AddWindow, 8, 0, 0, 0), 1);
    assert_eq!(w.call(EeWonWmCall::RemoveWindow, 9, 0, 0, 0), 0);
    w.call(EeWonWmCall::PostEvent, 3, 0, 0, 0);
    assert_eq!(w.call(EeWonWmCall::PeekNextEvent, 0, 0, 0, 0), 3);
    w.call(EeWonWmCall::SetField, F::FullScreen as isize, 1, 0, 0);
    assert_eq!(w.field(F::FullScreen as i32), 1);
    w.call(EeWonWmCall::SetField, F::LastTimerTick as isize, 77, 0, 0);
    assert_eq!(w.field(F::LastTimerTick as i32), 77);
    assert_eq!(w.set_at(9, 0), 0);
}

// ---- PostEvent from other threads: a deterministic check of what any interleaving must give

static MT_HOLDS: AtomicUsize = AtomicUsize::new(0);
static MT_RELEASES: AtomicUsize = AtomicUsize::new(0);
static MT_SIGNALS: AtomicUsize = AtomicUsize::new(0);
static MT_DELIVERED: Mutex<Vec<isize>> = Mutex::new(Vec::new());
static MT_GUI: AtomicIsize = AtomicIsize::new(0);

fn thread_key() -> isize {
    // the GUI thread's id as a number (any stable per-thread value)
    let id = format!("{:?}", std::thread::current().id());
    id.bytes().fold(7isize, |h, b| h.wrapping_mul(31).wrapping_add(b as isize))
}

unsafe extern "C" fn mt_host(_wm: *mut c_void, op: Op, a: isize, b: isize, _c: isize, _d: isize) -> isize {
    match op {
        Op::WmEvent if a == EeWonWmEvent::Hold as isize => {
            MT_HOLDS.fetch_add(1, Ordering::SeqCst);
        }
        Op::WmEvent if a == EeWonWmEvent::Release as isize => {
            // events are given back on the GUI thread only
            assert_eq!(thread_key(), MT_GUI.load(Ordering::SeqCst));
            MT_RELEASES.fetch_add(1, Ordering::SeqCst);
        }
        Op::WmEvent if a == EeWonWmEvent::Deliver as isize => {
            assert_eq!(thread_key(), MT_GUI.load(Ordering::SeqCst));
            MT_DELIVERED.lock().unwrap().push(b);
        }
        Op::WmEvent if a == EeWonWmEvent::Type as isize => return 99,
        Op::WmSignal => {
            MT_SIGNALS.fetch_add(1, Ordering::SeqCst);
        }
        _ => {}
    }
    0
}

struct Shared(*const EeWonWm);
// SAFETY: the other threads call only post_event, which touches the Mutex, the atomic and the thread-safe host
unsafe impl Send for Shared {}
// SAFETY: as above
unsafe impl Sync for Shared {}

#[test]
fn post_event_from_four_threads_while_the_gui_thread_delivers() {
    const THREADS: isize = 4;
    const EACH: isize = 500;
    MT_GUI.store(thread_key(), Ordering::SeqCst);
    let wm = ee_won_wm_new(std::ptr::dangling_mut::<c_void>(), Some(mt_host));
    let shared = Arc::new(Shared(wm));
    let start = Arc::new(Barrier::new(THREADS as usize + 1));
    let mut joins = Vec::new();
    for t in 0..THREADS {
        let s = Arc::clone(&shared);
        let b = Arc::clone(&start);
        joins.push(std::thread::spawn(move || {
            b.wait();
            for i in 0..EACH {
                // SAFETY: live until every thread is joined
                unsafe { (*s.0).post_event(((t + 1) << 16) | (i + 1), false) };
            }
        }));
    }
    start.wait();
    // the GUI thread delivers while they post, until every event is through
    // SAFETY: live until freed below
    let w = unsafe { &*wm };
    let mut rounds = 0;
    while MT_DELIVERED.lock().unwrap().len() < (THREADS * EACH) as usize {
        w.process_events();
        rounds += 1;
        assert!(rounds < 50_000_000, "events lost");
        std::thread::yield_now();
    }
    for j in joins {
        j.join().unwrap();
    }
    w.process_events();
    let got = std::mem::take(&mut *MT_DELIVERED.lock().unwrap());
    assert_eq!(got.len(), (THREADS * EACH) as usize);
    // each thread's events in the order it posted them, none twice
    for t in 1..=THREADS {
        let mine: Vec<isize> = got.iter().copied().filter(|e| e >> 16 == t).map(|e| e & 0xFFFF).collect();
        assert_eq!(mine, (1..=EACH).collect::<Vec<_>>(), "thread {t}");
    }
    assert_eq!(MT_HOLDS.load(Ordering::SeqCst), (THREADS * EACH) as usize);
    assert_eq!(MT_RELEASES.load(Ordering::SeqCst), (THREADS * EACH) as usize);
    // no message loop runs (depth 0): every post signals
    assert_eq!(MT_SIGNALS.load(Ordering::SeqCst), (THREADS * EACH) as usize);
    assert_eq!(w.field(F::EventCount as i32), 0);
    // SAFETY: made above; every thread joined
    unsafe { ee_won_wm_free(wm) };
}
