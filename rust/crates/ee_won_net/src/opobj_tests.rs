#![allow(clippy::manual_dangling_ptr)] // small integers stand in for the ops' and completions' addresses

use super::*;
use std::cell::RefCell;

thread_local! {
    static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
    /// what the run hook does: finish the op of this thread's test with a status
    static FINISH_IN_RUN: RefCell<Option<(*const EeWonAsyncOp, Status)>> = const { RefCell::new(None) };
}

fn log(s: String) {
    LOG.with(|l| l.borrow_mut().push(s));
}

fn take() -> Vec<String> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}

unsafe extern "C" fn run_hook(op: *mut c_void) {
    log(format!("run {}", op as usize));
    if let Some((o, s)) = FINISH_IN_RUN.with(|f| *f.borrow()) {
        // SAFETY: the test's live object
        unsafe { (*o).finish(s) };
    }
}
unsafe extern "C" fn cleanup_hook(op: *mut c_void) {
    log(format!("cleanup {}", op as usize));
}
unsafe extern "C" fn core_queue(op: *mut c_void) {
    log(format!("queue {}", op as usize));
}
unsafe extern "C" fn timer_add(op: *mut c_void, timeout: u32) {
    log(format!("timer {} {}", op as usize, timeout));
}
unsafe extern "C" fn timer_remove(op: *mut c_void) {
    log(format!("untimer {}", op as usize));
}
unsafe extern "C" fn c_add(c: *mut c_void) {
    log(format!("cref+ {}", c as usize));
}
unsafe extern "C" fn c_release(c: *mut c_void) {
    log(format!("cref- {}", c as usize));
}
unsafe extern "C" fn c_call(c: *mut c_void, op: *mut c_void) {
    log(format!("complete {} {}", c as usize, op as usize));
}
unsafe extern "C" fn op_add(op: *mut c_void) {
    log(format!("ref+ {}", op as usize));
}
unsafe extern "C" fn op_release(op: *mut c_void) {
    log(format!("ref- {}", op as usize));
}
unsafe extern "C" fn op_kill(op: *mut c_void) {
    log(format!("kill {}", op as usize));
}

static HOOKS: EeWonAsyncOpHooks = EeWonAsyncOpHooks {
    run_hook: Some(run_hook),
    cleanup_hook: Some(cleanup_hook),
    core_queue: Some(core_queue),
    timer_add: Some(timer_add),
    timer_remove: Some(timer_remove),
    completion_add_ref: Some(c_add),
    completion_release: Some(c_release),
    completion_call: Some(c_call),
    op_add_ref: Some(op_add),
    op_release: Some(op_release),
    op_kill: Some(op_kill),
    ref_add: Some(r_add),
    ref_release: Some(r_release),
    new_completion: Some(new_completion),
    callback_hook: Some(callback_hook),
};

unsafe extern "C" fn r_add(o: *mut c_void) {
    log(format!("obj+ {}", o as usize));
}
unsafe extern "C" fn r_release(o: *mut c_void) {
    log(format!("obj- {}", o as usize));
}
/// the stand-in `RustCompletion`: the body itself stands for the completion (the tests run and free it)
unsafe extern "C" fn new_completion(body: *mut EeWonCompletion) -> *mut c_void {
    log("new completion".into());
    body as *mut c_void
}
thread_local! {
    static CALLBACK_RESULT: RefCell<bool> = const { RefCell::new(true) };
}
unsafe extern "C" fn callback_hook(op: *mut c_void, child: *mut c_void, param: i32) -> bool {
    log(format!("callback {} {} {}", op as usize, child as usize, param));
    CALLBACK_RESULT.with(|r| *r.borrow())
}

/// The log with the completion bodies' addresses named `c`.
fn take_named(c: *mut c_void) -> Vec<String> {
    let n = (c as usize).to_string();
    take()
        .into_iter()
        .map(|l| l.split(' ').map(|w| if w == n { "c" } else { w }).collect::<Vec<_>>().join(" "))
        .collect()
}

fn op(id: usize) -> EeWonAsyncOp {
    EeWonAsyncOp::new(id as *mut c_void, &HOOKS)
}

#[test]
fn async_run_queues_then_completes() {
    let o = op(1);
    unsafe { o.set_completion(9 as *mut c_void) };
    assert_eq!(take(), ["cref+ 9"]);
    assert_eq!(o.run(ops::MODE_ASYNC, 0xffff_ffff), WS_PENDING);
    assert_eq!(take(), ["queue 1"]);
    // a second run while pending, nothing happens
    assert_eq!(o.run(ops::MODE_ASYNC, 0xffff_ffff), WS_PENDING);
    assert!(take().is_empty());
    o.start_run_async();
    assert_eq!(take(), ["run 1"]);
    o.finish(WS_SUCCESS);
    assert!(o.completion_pending());
    assert_eq!(take(), ["queue 1"]);
    assert_eq!(o.run(ops::MODE_ASYNC, 5), WS_AWAITING_COMPLETION);
    o.complete();
    assert_eq!(take(), ["untimer 1", "cleanup 1", "cref+ 9", "complete 9 1", "cref- 9"]);
    o.complete();
    assert!(take().is_empty());
    assert_eq!(o.status(), WS_SUCCESS);
    drop(o);
    assert_eq!(take(), ["cref- 9"]);
}

#[test]
fn timeouts_arm_or_fire() {
    let o = op(2);
    o.set_run_async_immediately(true);
    o.run(ops::MODE_ASYNC, 500);
    assert_eq!(take(), ["run 2", "timer 2 500"]);
    let z = op(3);
    z.set_run_async_immediately(true);
    z.run(ops::MODE_ASYNC, 0);
    assert_eq!(take(), ["run 3", "queue 3"]);
    assert_eq!(z.status(), WS_TIMED_OUT);
    let t = op(4);
    t.run_as_timer(70);
    assert_eq!(take(), ["timer 4 70"]);
    t.run_as_timer(70);
    assert!(take().is_empty());
    assert_eq!(t.time_left(), 0, "async runs wait on nothing");
}

#[test]
fn blocking_run_completes_at_once() {
    let o = op(5);
    FINISH_IN_RUN.with(|f| *f.borrow_mut() = Some((&o as *const _, WS_SUCCESS)));
    assert_eq!(o.run(ops::MODE_BLOCK, 3_600_000), WS_SUCCESS);
    FINISH_IN_RUN.with(|f| *f.borrow_mut() = None);
    assert_eq!(take(), ["run 5", "untimer 5", "cleanup 5"]);
    assert!(o.time_left() > 3_500_000);
    // a blocking run left pending: no completion until a finish and a complete
    assert_eq!(o.run(ops::MODE_BLOCK, 3_600_000), WS_PENDING);
    assert_eq!(take(), ["run 5"]);
    o.finish(WS_KILLED);
    assert!(take().is_empty(), "a blocking finish queues nothing");
    o.complete();
    assert_eq!(take(), ["untimer 5", "cleanup 5"]);
}

#[test]
fn kill_and_force_finish() {
    let o = op(6);
    o.kill(true);
    assert!(o.killed());
    assert!(take().is_empty(), "an idle op is not finished");
    o.run(ops::MODE_ASYNC, 1);
    assert!(!o.killed());
    take();
    o.kill(false);
    assert!(o.killed() && o.status() == WS_PENDING);
    o.kill(true);
    assert_eq!(o.status(), WS_KILLED);
    assert_eq!(take(), ["queue 6"]);
    o.force_finish(WS_SUCCESS);
    assert_eq!(o.status(), WS_KILLED, "a pending completion keeps the status");
    o.complete();
    take();
    o.force_finish(WS_SUCCESS);
    assert_eq!(o.status(), WS_SUCCESS);
    assert!(!o.killed());
    assert_eq!(take(), ["queue 6"]);
    o.set_timeout(7);
    o.set_mode(ops::MODE_BLOCK);
    assert_eq!((o.timeout(), o.run_mode()), (7, ops::MODE_BLOCK));
}

#[test]
fn replaced_completion_references() {
    let o = op(7);
    unsafe { o.set_completion(1 as *mut c_void) };
    unsafe { o.set_completion(2 as *mut c_void) };
    unsafe { o.set_completion(null_mut()) };
    assert_eq!(take(), ["cref+ 1", "cref+ 2", "cref- 1", "cref- 2"]);
    o.set_container(5 as *mut c_void, null_mut());
    o.queue_in_container();
    assert_eq!(take(), ["queue 7"], "without the Rust container the core queue takes it");
}

#[test]
fn tracker_order_and_references() {
    let t = EeWonOpTracker::new(&HOOKS);
    let (a, b, c) = (10 as *mut c_void, 11 as *mut c_void, 12 as *mut c_void);
    assert_eq!(t.add(a, 1), a);
    t.add(b, 1);
    t.add(c, 0);
    t.add(a, 2);
    assert_eq!(take(), ["ref+ 10", "ref+ 11", "ref+ 12"]);
    assert_eq!(t.len(), 3);
    let order: Vec<_> = (0..4).map(|i| t.at(i)).collect();
    assert_eq!(order, [(c, 0), (b, 1), (a, 1), (null_mut(), 0)]);
    assert!(t.remove(b));
    assert!(!t.remove(b));
    assert_eq!(take(), ["ref- 11"]);
    t.add(b, 5);
    take();
    t.kill_all();
    assert_eq!(take(), ["kill 12", "ref- 12", "kill 10", "ref- 10", "kill 11", "ref- 11"]);
    assert!(t.is_empty());
    t.add(a, 3);
    t.add(c, 3);
    take();
    drop(t);
    assert_eq!(take(), ["ref- 12", "ref- 10"]);
}

#[test]
fn track_routes_the_completion_to_the_callback_hook() {
    let parent = op(20);
    let child = op(21);
    let t = EeWonOpTracker::new(&HOOKS);
    let child_ptr = 21 as *mut c_void;
    let r = unsafe { parent.track(&t, child_ptr, &child, 4) };
    assert_eq!(r, child_ptr);
    let c = child.completion();
    assert!(!c.is_null());
    // the parent's reference, the completion made and taken by the child, the tracker's reference
    assert_eq!(take_named(c), ["ref+ 20", "new completion", "cref+ c", "ref+ 21"]);
    // an idle parent: the completion does not call back, the child stays tracked
    unsafe { ee_won_completion_run(c as *const EeWonCompletion, child_ptr) };
    assert!(take().is_empty());
    assert_eq!(t.len(), 1);
    // a pending parent: the child leaves the tracker, then the hook
    parent.run(ops::MODE_ASYNC, 0xffff_ffff);
    take();
    unsafe { ee_won_completion_run(c as *const EeWonCompletion, child_ptr) };
    assert_eq!(take(), ["ref- 21", "callback 20 21 4"]);
    assert!(t.is_empty());
    // again: no longer tracked, no call
    unsafe { ee_won_completion_run(c as *const EeWonCompletion, child_ptr) };
    assert!(take().is_empty());
    // the body's reference to the parent goes with it
    unsafe { ee_won_completion_free(c as *mut EeWonCompletion) };
    assert_eq!(take(), ["ref- 20"]);
}

#[test]
fn track_replaces_a_completion_and_tracks_once() {
    let parent = op(30);
    let child = op(31);
    let t = EeWonOpTracker::new(&HOOKS);
    let child_ptr = 31 as *mut c_void;
    unsafe { child.set_completion(5 as *mut c_void) };
    unsafe { parent.track(&t, child_ptr, &child, 1) };
    let c1 = child.completion();
    assert_eq!(take_named(c1), ["cref+ 5", "ref+ 30", "new completion", "cref+ c", "cref- 5", "ref+ 31"]);
    unsafe { parent.track(&t, child_ptr, &child, 2) };
    let c2 = child.completion();
    let log = take();
    assert_eq!(log.len(), 4, "{log:?}");
    assert_eq!(log[0], "ref+ 30");
    assert_eq!(t.at(0), (child_ptr, 1), "tracked once, under its first id");
    parent.run(ops::MODE_ASYNC, 0xffff_ffff);
    take();
    unsafe { ee_won_completion_run(c2 as *const EeWonCompletion, child_ptr) };
    assert_eq!(take(), ["ref- 31", "callback 30 31 2"], "the newest completion's param");
    unsafe {
        ee_won_completion_free(c1 as *mut EeWonCompletion);
        ee_won_completion_free(c2 as *mut EeWonCompletion);
    }
    assert_eq!(take(), ["ref- 30", "ref- 30"]);
}

#[test]
fn completion_without_a_maker_is_null() {
    let mut h = HOOKS;
    h.new_completion = None;
    assert!(EeWonCompletion::make(&h, |_| {}).is_null());
    let r = HeldRef::new(40 as *mut c_void, h.ref_add, h.ref_release);
    assert_eq!(r.get(), 40 as *mut c_void);
    drop(r);
    assert_eq!(take(), ["obj+ 40", "obj- 40"]);
}
